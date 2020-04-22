//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmedag.h"
#include "movieobjects/dmeshape.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "movieobjects/dmetransform.h"
#include "movieobjects/dmeoverlay.h"
#include "movieobjects_interfaces.h"
#include "movieobjects/dmedrawsettings.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmelog.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmerigconstraintoperators.h"
#include "movieobjects/dmetransformcontrol.h"
#include "movieobjects/dmeattributereference.h"
#include "movieobjects/dmerig.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


static const char OVERRIDE_PARENT[] = "overrideParent";

//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeDag, CDmeDag );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CUtlStack<CDmeDag::TransformInfo_t> CDmeDag::s_TransformStack;
bool CDmeDag::s_bDrawUsingEngineCoordinates = false;
bool CDmeDag::s_bDrawZUp = false;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeDag::OnConstruction()
{
	m_Transform.InitAndCreate( this, "transform" );
	m_Shape.Init( this, "shape" );
	m_Visible.InitAndSet( this, "visible", true, FATTRIB_HAS_CALLBACK );
	m_Children.Init( this, "children" );
	m_bDisableOverrideParent.InitAndSet( this, "disableOverride", false, FATTRIB_DONTSAVE | FATTRIB_HIDDEN );
}

void CDmeDag::OnDestruction()
{
	g_pDataModel->DestroyElement( m_Transform.GetHandle() );
}

void CDmeDag::Resolve()
{
	// Since the overrideParent attribute is added dynamically we must update 
	// its flags is it present when re-loaded since the flags are not stored.
	CDmAttribute *pAttribute = GetAttribute( OVERRIDE_PARENT );
	if ( pAttribute )
	{
		pAttribute->AddFlag( FATTRIB_NEVERCOPY );
	}
}

//-----------------------------------------------------------------------------
// Accessors
//-----------------------------------------------------------------------------
CDmeTransform *CDmeDag::GetTransform() const
{
	return m_Transform.GetElement();
}

CDmeShape *CDmeDag::GetShape()
{
	return m_Shape.GetElement();
}

void CDmeDag::SetShape( CDmeShape *pShape )
{
	m_Shape = pShape;
}


bool CDmeDag::IsVisible() const
{
	return m_Visible;
}

void CDmeDag::SetVisible( bool bVisible )
{
	m_Visible = bVisible;
}


//-----------------------------------------------------------------------------
// Returns the visibility attribute for DmeRenderable support
//-----------------------------------------------------------------------------
CDmAttribute *CDmeDag::GetVisibilityAttribute() 
{ 
	return m_Visible.GetAttribute(); 
}


//-----------------------------------------------------------------------------
// child helpers
//-----------------------------------------------------------------------------
const CUtlVector< DmElementHandle_t > &CDmeDag::GetChildren() const
{
	return m_Children.Get();
}

int CDmeDag::GetChildCount() const
{
	return m_Children.Count();
}

CDmeDag *CDmeDag::GetChild( int i ) const
{
	if ( i < 0 || i >= m_Children.Count() )
		return NULL;

	return m_Children.Get( i );
}

bool CDmeDag::AddChild( CDmeDag* pDag )
{
	if ( !pDag || pDag == this )
		return false;

	// Don't allow a cycle to be created 
	if ( pDag->IsAncestorOfDag( this ) )
		return false;

	m_Children.AddToTail( pDag );
	return true;
}

void CDmeDag::RemoveChild( int i )
{
	m_Children.FastRemove( i );
}

void CDmeDag::RemoveChild( const CDmeDag *pChild, bool bRecurse )
{
	int i = FindChild( pChild );
	if ( i >= 0 )
	{
		RemoveChild( i );
	}
}

void CDmeDag::RemoveAllChildren()
{
	m_Children.RemoveAll();
}


//-----------------------------------------------------------------------------
// Sets the parent of this node to the specified parent. Removes any other
// parent nodes
//-----------------------------------------------------------------------------
bool CDmeDag::SetParent( CDmeDag *pDmeDagParent )
{
	if ( !pDmeDagParent || pDmeDagParent == this )
		return false;

	CUtlVector< CDmeDag * > parentList;
	FindAncestorsReferencingElement( this, parentList );
	for ( int i = 0; i < parentList.Count(); ++i )
	{
		if ( !parentList[ i ] )
			continue;

		parentList[ i ]->RemoveChild( this );
	}

	return pDmeDagParent->AddChild( this );
}


int CDmeDag::FindChild( const CDmeDag *pChild ) const
{
	return m_Children.Find( pChild->GetHandle() );
}

// recursive
int CDmeDag::FindChild( CDmeDag *&pParent, const CDmeDag *pChild )
{
	int index = FindChild( pChild );
	if ( index >= 0 )
	{
		pParent = this;
		return index;
	}

	int nChildren = m_Children.Count();
	for ( int ci = 0; ci < nChildren; ++ci )
	{
		index = m_Children[ ci ]->FindChild( pParent, pChild );
		if ( index >= 0 )
			return index;
	}

	pParent = NULL;
	return -1;
}

int CDmeDag::FindChild( const char *name ) const
{
	int nChildren = m_Children.Count();
	for ( int ci = 0; ci < nChildren; ++ci )
	{
		if ( V_strcmp( m_Children[ ci ]->GetName(), name ) == 0 )
			return ci;
	}
	return -1;
}

CDmeDag *CDmeDag::FindOrAddChild( const char *name )
{
	int i = FindChild( name );
	if ( i >= 0 )
		return GetChild( i );

	CDmeDag *pChild = CreateElement< CDmeDag >( name, GetFileId() );
	AddChild( pChild );
	return pChild;
}

CDmeDag *CDmeDag::FindChildByName_R( const char *name ) const
{
	int i = FindChild( name );
	if ( i >= 0 )
		return GetChild( i );

	int nChildren = m_Children.Count();
	for ( int ci = 0; ci < nChildren; ++ci )
	{
		CDmeDag *found = m_Children[ ci ]->FindChildByName_R( name );
		if ( found )
			return found;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Return the number of steps ( levels, connections, etc...) between
// the the node a the specified child node. If the provided node is not a 
// child of this node -1 will be returned.
//-----------------------------------------------------------------------------
int CDmeDag::StepsToChild( const CDmeDag *pChild ) const
{
	int nSteps = 0;
	for ( const CDmeDag *pDag = pChild; pDag; pDag = pDag->GetParent(), ++nSteps )
	{
		if ( pDag == this )
			return nSteps;
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Recursively render the Dag hierarchy
//-----------------------------------------------------------------------------
void CDmeDag::PushDagTransform()
{
	int i = s_TransformStack.Push();
	TransformInfo_t &info = s_TransformStack[i];
	info.m_pTransform = GetTransform();
	info.m_bComputedDagToWorld = false;
}

void CDmeDag::PopDagTransform()
{
	Assert( s_TransformStack.Top().m_pTransform == GetTransform() );
	s_TransformStack.Pop();
}


//-----------------------------------------------------------------------------
// Transform from DME to engine coordinates
//-----------------------------------------------------------------------------
void CDmeDag::DmeToEngineMatrix( matrix3x4_t& dmeToEngine, bool bZUp )
{
	if ( bZUp )
	{
		VMatrix rotationZ;
		MatrixBuildRotationAboutAxis( rotationZ, Vector( 0, 0, 1 ), 90 );
		rotationZ.Set3x4( dmeToEngine );
	}
	else
	{
		VMatrix rotation, rotationZ;
		MatrixBuildRotationAboutAxis( rotation, Vector( 1, 0, 0 ), 90 );
		MatrixBuildRotationAboutAxis( rotationZ, Vector( 0, 1, 0 ), 90 );
		ConcatTransforms( rotation.As3x4(), rotationZ.As3x4(), dmeToEngine );
	}
}

//-----------------------------------------------------------------------------
// Transform from engine to DME coordinates
//-----------------------------------------------------------------------------
void CDmeDag::EngineToDmeMatrix( matrix3x4_t& engineToDme, bool bZUp )
{
	if ( bZUp )
	{
		VMatrix rotationZ;
		MatrixBuildRotationAboutAxis( rotationZ, Vector( 0, 0, 1 ), -90 );
		rotationZ.Set3x4( engineToDme );
	}
	else
	{
		VMatrix rotation, rotationZ;
		MatrixBuildRotationAboutAxis( rotation, Vector( 1, 0, 0 ), -90 );
		MatrixBuildRotationAboutAxis( rotationZ, Vector( 0, 1, 0 ), -90 );
		ConcatTransforms( rotationZ.As3x4(), rotation.As3x4(), engineToDme );
	}
}


void CDmeDag::GetShapeToWorldTransform( matrix3x4_t &mat )
{
	int nCount = s_TransformStack.Count();
	if ( nCount == 0 )
	{
		if ( !s_bDrawUsingEngineCoordinates )
		{
			SetIdentityMatrix( mat );
		}
		else
		{
			DmeToEngineMatrix( mat, s_bDrawZUp );
		}
		return;
	}

	if ( s_TransformStack.Top().m_bComputedDagToWorld )
	{
		MatrixCopy( s_TransformStack.Top().m_DagToWorld, mat );
		return;
	}

	// Compute all uncomputed dag to worls
	int i;
	for ( i = 0; i < nCount; ++i )
	{
		TransformInfo_t &info = s_TransformStack[i];
		if ( !info.m_bComputedDagToWorld )
			break;
	}

	// Set up the initial transform
	if ( i == 0 )
	{
		if ( !s_bDrawUsingEngineCoordinates )
		{
			SetIdentityMatrix( mat );
		}
		else
		{
			DmeToEngineMatrix( mat, s_bDrawZUp );
		}
	}
	else
	{
		MatrixCopy( s_TransformStack[i-1].m_DagToWorld, mat );
	}

	// Compute all transforms
	for ( ; i < nCount; ++i )
	{
		matrix3x4_t localToParent;
		TransformInfo_t &info = s_TransformStack[i];
		info.m_pTransform->GetTransform( localToParent );
		ConcatTransforms( mat, localToParent, info.m_DagToWorld );
		info.m_bComputedDagToWorld = true;
		MatrixCopy( info.m_DagToWorld, mat );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDag::GetLocalMatrix( matrix3x4_t &m ) const
{
	CDmeTransform *pTransform = GetTransform();
	if ( pTransform )
	{
		pTransform->GetTransform( m );
	}
	else
	{
		SetIdentityMatrix( m );
	}
}

void CDmeDag::SetLocalMatrix( matrix3x4_t &mat )
{
	CDmeTransform *pTransform = GetTransform();
	if ( !pTransform )
		return;

	pTransform->SetTransform( mat );
}

//-----------------------------------------------------------------------------
// Get the transform matrix which converts from the space of the parent of the
// dag node to world space. If the dag node has an override parent specified
// it will be accounted for when generating the matrix.
//-----------------------------------------------------------------------------
void CDmeDag::GetParentWorldMatrix( matrix3x4_t &mParentToWorld ) const
{
	CDmeDag *pParent = GetParent();

	if ( HasOverrideParent() )
	{
		bool bOverridePos = false;
		bool bOverrideRot = false;
		const CDmeDag *pOverrideParent = GetOverrideParent( bOverridePos, bOverrideRot );

		if ( bOverridePos && bOverrideRot )
		{
			pOverrideParent->GetAbsTransform( mParentToWorld );
		}
		else
		{
			if ( pParent )
			{
				pParent->GetAbsTransform( mParentToWorld );
			}
			else
			{
				SetIdentityMatrix( mParentToWorld );
			}
			
			Quaternion absOrientation;
			Vector absPosition;

			if ( bOverridePos )
			{			
				// The desired result of a position only parent override is that no changes to the original parent in either 
				// position or rotation will have any impact on the position of the dag. Furthermore we prefer rotations of
				// the original parent to pivot around the dag node's center, not the override parent's center. To accomplish 
				// this we essentially want to compute the rotation normally, but then apply the local translation of the dag
				// to the position of the override dag in world space. In order to construct the a parent matrix that still
				// allows the local transform of the dag to be concatenated normally, we essentially do the whole transform
				// here when constructing the parent matrix and then apply the inverse of the local matrix, so we know when
				// the local matrix is concatenated later the result will be what we setup before applying the inverse of the
				// local matrix.

				CDmeTransform *pLocalTransform = GetTransform();
				if ( pLocalTransform )
				{				
					Vector localPosition = pLocalTransform->GetPosition();
					Quaternion localOrientation = pLocalTransform->GetOrientation();
					
					// Compute the orientation normally as if there is no override parent
					// by concatenating the local orientation with the parent orientation.
					Quaternion parentOrientation;
					Quaternion worldOrientation;
					MatrixQuaternion( mParentToWorld, parentOrientation );
					QuaternionMult( parentOrientation, localOrientation, worldOrientation );

					// Compute the position by simply adding the local position to the world space 
					// position of the override parent, this effectively make the local translation 
					// of the dag a world space operation so it is never effected by rotation.
					Vector overridePosition;
					pOverrideParent->GetAbsPosition( overridePosition );
					Vector worldPosition = overridePosition + localPosition;

					// Compute the final world transform we will want for the dag 
					// (i.e. this is the reuslt we want GetAbsTransform() to return)
					matrix3x4_t mWorldTransform;
					QuaternionMatrix( worldOrientation, worldPosition, mWorldTransform );
					
					// Now construct an apply the inverse of the local transform in order to get
					// an appropriate parent matrix that will yield the desired mWorldTransfom 
					// when the local transform of the dag is applied to it.
					matrix3x4_t mLocalTransform;
					matrix3x4_t mInvLocalTransform;
					QuaternionMatrix( localOrientation, localPosition, mLocalTransform );
					MatrixInvert( mLocalTransform, mInvLocalTransform );
					ConcatTransforms( mWorldTransform, mInvLocalTransform, mParentToWorld );
				}
			}
			else if ( bOverrideRot )
			{
				// Rotation only override is much simpler than position, just construct a matrix with 
				// the orientation of the override parent and the position of the original parent.
				pOverrideParent->GetAbsOrientation( absOrientation );	
				MatrixPosition( mParentToWorld, absPosition );
				QuaternionMatrix( absOrientation, absPosition, mParentToWorld );
			}
		}
	}
	else if ( pParent )
	{
		pParent->GetAbsTransform( mParentToWorld );
	}
	else
	{
		SetIdentityMatrix( mParentToWorld );
	}
}


//-----------------------------------------------------------------------------
// Get the matrix matrix for computing the position of the dag node. This is 
// the same as GetParentWorldMatrix except in the case where the dag node has
// a position only override parent. This should be used when trying to apply a
// translation to the dag and conversion from world space to the space of the
// parent of the dag is required.
//-----------------------------------------------------------------------------
void CDmeDag::GetTranslationParentWorldMatrix( matrix3x4_t &mParentToWorld )
{
	bool bOverridePos = false;
	bool bOverrideRot = false;
	const CDmeDag *pOverrideParent = GetOverrideParent( bOverridePos, bOverrideRot );

	if ( pOverrideParent && bOverridePos && !bOverrideRot )
	{
		Vector overridePosition;
		pOverrideParent->GetAbsPosition( overridePosition );
		SetIdentityMatrix( mParentToWorld );
		PositionMatrix( overridePosition, mParentToWorld );
	}
	else
	{
		GetParentWorldMatrix( mParentToWorld );
	}
}


//-----------------------------------------------------------------------------
// Recursively render the Dag hierarchy
//-----------------------------------------------------------------------------
void CDmeDag::DrawUsingEngineCoordinates( bool bEnable )
{
	s_bDrawUsingEngineCoordinates = bEnable;
}


//-----------------------------------------------------------------------------
// Specify if the model is a Z Up Model
//-----------------------------------------------------------------------------
void CDmeDag::DrawZUp( bool bZUp )
{
	s_bDrawZUp = bZUp;
}


//-----------------------------------------------------------------------------
// Recursively render the Dag hierarchy
//-----------------------------------------------------------------------------
void CDmeDag::Draw( CDmeDrawSettings *pDrawSettings )
{
	if ( !m_Visible )
		return;

	PushDagTransform();

	CDmeShape *pShape = GetShape();
	if ( pShape )
	{
		matrix3x4_t shapeToWorld;
		GetShapeToWorldTransform( shapeToWorld );
		pShape->Draw( shapeToWorld, pDrawSettings );
	}

	uint cn = m_Children.Count();
	for ( uint ci = 0; ci < cn; ++ci )
	{
		m_Children[ ci ]->Draw( pDrawSettings );
	}

	PopDagTransform();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDag::GetBoundingSphere( Vector &c0, float &r0, const matrix3x4_t &pMat ) const
{
	matrix3x4_t lMat;
	m_Transform.GetElement()->GetTransform( lMat );
	matrix3x4_t wMat;
	ConcatTransforms( pMat, lMat, wMat );

	c0.Zero();
	r0 = 0.0f;

	Vector vTemp;

	const CDmeShape *pShape = m_Shape.GetElement();
	if ( pShape )
	{
		pShape->GetBoundingSphere( c0, r0 );
		VectorTransform( c0, lMat, vTemp );
		c0 = vTemp;
	}

	// No scale in Dme! :)
	VectorTransform( c0, pMat, vTemp );
	c0 = vTemp;

	const int nChildren = m_Children.Count();
	if ( nChildren > 0 )
	{
		Vector c1;	// Child center
		float r1;	// Child radius

		Vector v01;	// c1 - c0
		float l01;	// |v01|

		for ( int i = 0; i < nChildren; ++i )
		{
			m_Children[ i ]->GetBoundingSphere( c1, r1, wMat );

			if ( r0 == 0.0f )
			{
				c0 = c1;
				r0 = r1;
				continue;
			}

			v01 = c1 - c0;
			l01 = v01.NormalizeInPlace();

			if ( r0 < l01 + r1 )
			{
				// Current sphere doesn't contain both spheres
				if ( r1 < l01 + r0 )
				{
					// Child sphere doesn't contain both spheres
					c0 = c0 + 0.5f * ( r1 + l01 - r0 ) * v01;
					r0 = 0.5f * ( r0 + l01 + r1 );
				}
				else
				{
					// Child sphere contains both spheres
					c0 = c1;
					r0 = r1;
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Find the channels targeting the transform of the object either 
// directly or through a constraint
//-----------------------------------------------------------------------------
void CDmeDag::FindTransformChannels( CUtlVector< CDmeChannel * > &channelList ) const
{
	// Find and channels targeting the transform of the dag node
	FindAncestorsReferencingElement( GetTransform(), channelList );

	// Find the slave instances targeting the specified node
	CUtlVector< CDmeConstraintSlave* > slaveList;
	FindAncestorsReferencingElement( this, slaveList );

	int nSlaves = slaveList.Count();
	for ( int iSlave = 0; iSlave < nSlaves; ++iSlave )
	{
		CDmeConstraintSlave *pSlave = slaveList[ iSlave ];
		if ( pSlave )
		{
			FindAncestorsReferencingElement( pSlave, channelList );
		}
	}

}


//-----------------------------------------------------------------------------
// Purpose: Find the transform controls driving the dag node
//-----------------------------------------------------------------------------
CDmeTransformControl *CDmeDag::FindTransformControl() const
{
	CUtlVector< CDmeChannel* > channelList( 0, 4 );
	FindTransformChannels( channelList );
	
	int nChannels = channelList.Count();
	for ( int iChannel = 0; iChannel < nChannels; ++iChannel )
	{		
		CDmeChannel *pChannel = channelList[ iChannel ];
		if ( pChannel )
		{
			CDmeTransformControl *pTransformControl = CastElement< CDmeTransformControl >( pChannel->GetFromElement() );
			if ( pTransformControl )
				return pTransformControl;
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Find the channels and operators which must be evaluated in order
// to determine the state of the dag node. Note that this function returns only
// the operators targeting this dag node, it does not include those targeting 
// the parent of this node, but it does return all operators that this node
// is dependent on even if they are are connected to this node through other
// operators.
//-----------------------------------------------------------------------------
void CDmeDag::FindLocalOperators( CUtlVector< CDmeOperator* > &operatorList ) const
{
	// Find any operators directly targeting the transform of the dag node
	GatherOperatorsForElement( GetTransform(), operatorList );

	// Find the slave instances targeting the specified node
	for ( DmAttributeReferenceIterator_t it = g_pDataModel->FirstAttributeReferencingElement( GetHandle() );
		it != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID;
		it = g_pDataModel->NextAttributeReferencingElement( it ) )
	{
		CDmAttribute *pAttr = g_pDataModel->GetAttribute( it );
		CDmElement *pElement = pAttr->GetOwner();

		if ( !g_pDataModel->GetElement( pElement->GetHandle() ) )
			continue;

		CDmeConstraintSlave *pConstraintSlave = CastElement< CDmeConstraintSlave >( pElement );
		if ( pConstraintSlave )
		{			
			CDmeRigBaseConstraintOperator *pConstraint = pConstraintSlave->GetConstraint();
			if ( pConstraint )
			{
				if ( operatorList.Find( pConstraint ) == operatorList.InvalidIndex() )
				{
					pConstraint->GatherInputOperators( operatorList );
					operatorList.AddToTail( pConstraint );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Find all of the operators on which dag node is dependent, this 
// recursively finds operators for the parents of the dag. The list of operators
// that is returned represents all of the operators which must be evaluated in
// order to determine the current state of the node. The list is returned in
// the order that the operators should be run.
//-----------------------------------------------------------------------------
void CDmeDag::FindRelevantOperators( CUtlVector< CDmeOperator * > &operatorList ) const
{	
	const CDmeDag *pOverrideParent = GetOverrideParent();
	if ( pOverrideParent )
	{
		pOverrideParent->FindRelevantOperators( operatorList );
	}

	CDmeDag *pParent = GetParent();
	if ( pParent )
	{
		pParent->FindRelevantOperators( operatorList );
	}

	FindLocalOperators( operatorList );
}


//-----------------------------------------------------------------------------
// Purpose: Find all of the operators on which the dag node is dependent, 
// splitting the channels into a separate list. This function merely calls
// the FindRelvantOperators() which returns a single list of operators and 
// then creates a list of channels and a list of other operators. It is 
// preferable to just use the version which returns a single list of operators.
//-----------------------------------------------------------------------------
void CDmeDag::FindRelevantOperators( CUtlVector< CDmeChannel * > &channelList, CUtlVector< CDmeOperator * > &operatorList ) const
{
	CUtlVector< CDmeOperator* > allOperatorsList( 0, 128 );
	FindRelevantOperators( allOperatorsList );

	int nTotalOperators = allOperatorsList.Count();
	channelList.EnsureCapacity( channelList.Count() + nTotalOperators );
	operatorList.EnsureCapacity( operatorList.Count() + nTotalOperators );

	for ( int iOperator = 0; iOperator < nTotalOperators; ++iOperator )
	{
		CDmeOperator *pOperator = allOperatorsList[ iOperator ];
		if ( pOperator == NULL )
			continue;

		CDmeChannel *pChannel = CastElement< CDmeChannel >( pOperator );
		if ( pChannel )
		{
			channelList.AddToTail( pChannel );
		}
		else
		{
			operatorList.AddToTail( pOperator );
		}
	}
}


void CDmeDag::GetBoundingBox( Vector &min, Vector &max, const matrix3x4_t &pMat ) const
{
	matrix3x4_t lMat;
	m_Transform.GetElement()->GetTransform( lMat );
	matrix3x4_t wMat;
	ConcatTransforms( pMat, lMat, wMat );

	min.Zero();
	max.Zero();
	bool bHasBox = false;

	const CDmeShape *pShape = m_Shape.GetElement();
	if ( pShape )
	{
		Vector cmin, cmax;
		pShape->GetBoundingBox( cmin, cmax );
		if ( cmin != cmax )
		{
			Vector bmin( cmin ), bmax( cmax );
			VectorTransform( cmin, pMat, bmin );
			VectorTransform( cmax, pMat, bmax );
			for ( int kx = 0; kx < 2; ++ kx )
				for ( int ky = 0; ky < 2; ++ ky )
					for ( int kz = 0; kz < 2; ++ kz )
					{
						Vector tp(
							kx ? cmax.x : cmin.x,
							ky ? cmax.y : cmin.y,
							kz ? cmax.z : cmin.z );
						Vector vTemp;
						VectorTransform( tp, pMat, vTemp );

						bmin = bmin.Min( vTemp );
						bmax = bmax.Max( vTemp );
					}

			if ( !bHasBox )
			{
				min = bmin;
				max = bmax;
			}
			min = min.Min( bmin );
			max = max.Max( bmax );
			bHasBox = true;
		}
	}

	for ( int i = 0, nChildren = m_Children.Count(); i < nChildren; ++ i )
	{
		CDmeDag *pChild = m_Children[i];

		Vector cmin, cmax;
		pChild->GetBoundingBox( cmin, cmax, wMat );
		if ( cmin != cmax )
		{
			if ( !bHasBox )
			{
				min = cmin;
				max = cmax;
			}
			min = min.Min( cmin );
			max = max.Max( cmax );
			bHasBox = true;
		}
	}
}


Quaternion QuaternionTransform( const Quaternion &quat, const matrix3x4_t &mat )
{
	matrix3x4_t quatmat;
	QuaternionMatrix( quat, quatmat );

	matrix3x4_t newmat;
	ConcatTransforms( mat, quatmat, newmat );

	Quaternion result;
	MatrixQuaternion( newmat, result );

	return result;
}

void CDmeDag::BakeStaticTransforms( bool bRecurse /*= true*/ )
{
	// determine whether to bake this transform into its children

	if ( FindAncestorReferencingElement< CDmeChannel >( this ) || FindAncestorReferencingElement< CDmeConstraintSlave >( this ) )
		return; // don't bake, since it's transform isn't static

	if ( ( GetType() != CDmeDag::GetStaticTypeSymbol() ) && ( GetType() != CDmeRig::GetStaticTypeSymbol() ) )
		return; // don't bake CDmeGameModel's root transform into the child bones, CDmeParticleSystem's root transform into the control points, etc.

	int nChildrenFound = 0;

	int nChildren = GetChildCount();
	for ( int i = 0; i < nChildren; ++i )
	{
		CDmeDag *pChild = GetChild( i );
		if ( !pChild )
			continue;

		if ( FindAncestorReferencingElement< CDmeConstraintSlave >( pChild ) )
			return; // don't bake, since child's transform isn't static

		// TODO: it probably wouldn't be that hard to make this work with dags with overriden parents
		// the simplest path may be to unparent (and let the unparent do the conversion keeping everything in place), bake, and reparent
		// there are faster and less obtrusive ways, but we don't need this functionality now, so I'm not solving it yet
		if ( pChild->HasOverrideParent() )
			return; // don't bake

		++nChildrenFound;
	}

	if ( nChildrenFound == 0 )
		return;


	// now, actually bake this transform into its children

	matrix3x4_t parentMat;
	GetLocalMatrix( parentMat );

	matrix3x4_t identityMat;
	SetIdentityMatrix( identityMat );
	SetLocalMatrix( identityMat );

	static CUtlSymbolLarge symToElement = g_pDataModel->GetSymbol( "toElement" );

	for ( int i = 0; i < nChildren; ++i )
	{
		CDmeDag *pChild = GetChild( i );
		if ( !pChild )
			continue;

		CDmeTransform *pTransform = pChild->GetTransform();
		if ( !pTransform )
			continue;

		CDmeVector3Log *pPosLog = NULL;
		CDmeQuaternionLog *pRotLog = NULL;
		CDmeTransformControl *pTransformControl = NULL;

		for ( CAttributeReferenceIterator it( pTransform ); it; ++it )
		{
			if ( CDmeChannel *pChannel = it.FilterReference< CDmeChannel >( symToElement, true, TD_ALL ) )
			{
				CDmAttribute *pToAttr = pChannel->GetToAttribute();
				if ( pToAttr == pTransform->GetPositionAttribute() )
				{
					pPosLog = CastElement< CDmeVector3Log >( pChannel->GetLog() );
				}
				else if ( pToAttr == pTransform->GetOrientationAttribute() )
				{
					pRotLog = CastElement< CDmeQuaternionLog >( pChannel->GetLog() );
				}

				if ( !pTransformControl )
				{
					pTransformControl = CastElement< CDmeTransformControl >( pChannel->GetFromElement() );
				}
			}
		}

		if ( pPosLog )
		{
			int bestLayer = pPosLog->GetTopmostLayer();
			CDmeVector3LogLayer *pPosLogLayer = bestLayer >= 0 ? pPosLog->GetLayer( bestLayer ) : NULL;
			if ( pPosLogLayer )
			{
				int nPosKeys = pPosLogLayer->GetKeyCount();
				for ( int i = 0; i < nPosKeys; ++i )
				{
					pPosLogLayer->SetKeyValue( i, VectorTransform( pPosLogLayer->GetKeyValue( i ), parentMat ) );
				}
			}
			pPosLog->SetDefaultValue( VectorTransform( pPosLog->GetDefaultValue(), parentMat ) ); // do we really want to transform the default???
		}

		if ( pRotLog )
		{
			int bestLayer = pRotLog->GetTopmostLayer();
			CDmeQuaternionLogLayer *pRotLogLayer = bestLayer >= 0 ? pRotLog->GetLayer( bestLayer ) : NULL;
			if ( pRotLogLayer )
			{
				int nRotKeys = pRotLogLayer->GetKeyCount();
				for ( int i = 0; i < nRotKeys; ++i )
				{
					pRotLogLayer->SetKeyValue( i, QuaternionTransform( pRotLogLayer->GetKeyValue( i ), parentMat ) );
				}
			}
			pRotLog->SetDefaultValue( QuaternionTransform( pRotLog->GetDefaultValue(), parentMat ) ); // do we really want to transform the default???
		}

		if ( pTransformControl )
		{
			// bake into transform control
			pTransformControl->SetPosition   ( VectorTransform    ( pTransformControl->GetPosition   (), parentMat ) );
			pTransformControl->SetOrientation( QuaternionTransform( pTransformControl->GetOrientation(), parentMat ) );

			// we explicitly do NOT set the transform control's defaults, since the root default should still be the origin
		}

		// bake into transform
		pTransform->SetPosition   ( VectorTransform    ( pTransform->GetPosition   (), parentMat ) );
		pTransform->SetOrientation( QuaternionTransform( pTransform->GetOrientation(), parentMat ) );

		if ( bRecurse )
		{
			pChild->BakeStaticTransforms( bRecurse );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Attach the dag node to the specified parent node. If the dag node
// is currently a child of another node it will be removed the from that node.
// The local space transform of the dag node will be modified so that it will 
// have the same world space transform after being attached as before it was 
// attached. Passing in NULL for the parent effectively moves the dag node into
// world space without any parents.
//
//-----------------------------------------------------------------------------
void CDmeDag::OnAttachToDmeDag( CDmeDag *pParentDag, bool bFixupLogs /*= true*/ )
{
	if ( GetParent() == pParentDag )
		return;

	CDmeTransform *pTransform = GetTransform();
	if ( !pTransform )
		return;

	CUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "CDmeDag::OnAttachToDmeDag" );

	CDmeFilmClip *pFilmClip = NULL;
	
	if ( pParentDag != NULL )
	{
		pFilmClip = FindFilmClipContainingDag( pParentDag );
	}
	else
	{
		pFilmClip = FindFilmClipContainingDag( this );
	}


	matrix3x4_t cameraMat, parentMat, invParentMat, newCameraMat;
	GetAbsTransform( cameraMat );
	if ( pParentDag && pFilmClip )
	{
		pParentDag->GetAbsTransform( parentMat );
	}
	else
	{
		SetIdentityMatrix( parentMat );
	}

	MatrixInvert( parentMat, invParentMat );
	ConcatTransforms( invParentMat, cameraMat, newCameraMat );
	pTransform->SetTransform( newCameraMat );

	matrix3x4_t cameraParentMat, oldToNewCameraMat;
	GetParentWorldMatrix( cameraParentMat );
	ConcatTransforms( invParentMat, cameraParentMat, oldToNewCameraMat );

	CUtlVector< CDmeDag* > parents;
	FindAncestorsReferencingElement( this, parents );
	int nParents = parents.Count();
	for ( int i = 0; i < nParents; ++i )
	{
		parents[ i ]->RemoveChild( this );
	}

	if ( ( bFixupLogs ) && ( pFilmClip != NULL ) )
	{
		CDmeTransform *pTransform = GetTransform();
		CDmeChannel *pPosChannel = FindChannelTargetingElement( pFilmClip, pTransform, TRANSFORM_POSITION, NULL );
		if ( pPosChannel )
		{
			CDmeLog *pLog = pPosChannel->GetLog();
			if ( pLog )
			{
				CDmeLogLayer *pLayer = pLog->GetLayer( pLog->GetTopmostLayer() );
				CDmeTypedLogLayer< Vector > *pVectorLayer = CastElement< CDmeTypedLogLayer< Vector > >( pLayer );
				if ( pVectorLayer )
				{
					int nKeys = pVectorLayer->GetKeyCount();
					for ( int i = 0; i < nKeys; ++i )
					{
						Vector newvec, oldvec = pVectorLayer->GetKeyValue( i );
						VectorTransform( oldvec, oldToNewCameraMat, newvec );
						pVectorLayer->SetKeyValue( i, newvec );
					}
				}
			}
		}
		CDmeChannel *pRotChannel = FindChannelTargetingElement( pFilmClip, pTransform, TRANSFORM_ORIENTATION, NULL );
		if ( pRotChannel )
		{
			CDmeLog *pLog = pRotChannel->GetLog();
			if ( pLog )
			{
				CDmeLogLayer *pLayer = pLog->GetLayer( pLog->GetTopmostLayer() );
				CDmeTypedLogLayer< Quaternion > *pQuatLayer = CastElement< CDmeTypedLogLayer< Quaternion > >( pLayer );
				if ( pQuatLayer )
				{
					int nKeys = pQuatLayer->GetKeyCount();
					for ( int i = 0; i < nKeys; ++i )
					{
						matrix3x4_t oldmat, newmat;
						QuaternionMatrix( pQuatLayer->GetKeyValue( i ), oldmat );
						ConcatTransforms( oldToNewCameraMat, oldmat, newmat );
						Quaternion quat;
						MatrixQuaternion( newmat, quat );
						pQuatLayer->SetKeyValue( i, quat );
					}
				}
			}
		}
	}

	if ( pParentDag )
	{
		pParentDag->AddChild( this );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Modify the logs controlling the transform of this dag node so that
// the transform maintains the same relative position and orientation to the 
// target dag node for the duration of the provided time period.
// Input  : pTargetDag - Pointer to the dag node whose transform the transform
//			of this dag node it to be bound to.
// Input  : timeSelection - Time selection for which the modifications are to
//			be applied, including falloffs.
// Input  : pMovie - Pointer to the movie to which the dag belongs
// Input  : offset - Offset relative to the dag where the offset is going to be
//			placed if the reset position flag is true.
// Input  : bPosition - flag indicating that the position of the dag node
//			should be modified.
// Input  : bOrientation - flag indicating that the orientation of the dag 
//			node should be modified.
// Input  : bResetPosition - flag indicating that the position of the target 
//			dag node should be repositioned.
//-----------------------------------------------------------------------------
void CDmeDag::BindTransformToDmeDag( const CDmeDag *pTargetDag, const DmeLog_TimeSelection_t &timeSelection, const CDmeClip* pMovie, const Vector& offset, bool bPosition, bool bOrientation, bool bMaintainOffset )
{
	// Must specify a target dag and it must not be this dag.
	if ( ( pTargetDag == NULL ) || ( pTargetDag == this ) )
	{
		Assert( pTargetDag );
		return;
	}

	// Find the film clip for this dag and the target dag node and make sure they are the same
	CDmeFilmClip *pFilmClip = FindFilmClipContainingDag( this );
	if ( pFilmClip == NULL )
	{
		pFilmClip = FindReferringElement< CDmeFilmClip >( this, "camera" );
	}

	// Find the channels which must be updated in order to evaluate 
	// transforms of both this dag node and the target dag node.
	CUtlVector< CDmeChannel* > localChannelList, targetChannelList;
	CUtlVector< CDmeOperator* > localOperatorList, targetOperatorList;
	FindRelevantOperators( localChannelList, localOperatorList );
	pTargetDag->FindRelevantOperators( targetChannelList, targetOperatorList );


	// Build the clip stacks for the channel lists.
	CUtlVector< DmeClipStack_t > localClipStackList, targetClipStackList;
	CUtlVector< DmeTime_t > localChannelTimeList, targetChannelTimeList;
	BuildClipStackList( localChannelList, localClipStackList, localChannelTimeList, pMovie, pFilmClip );
	BuildClipStackList( targetChannelList, targetClipStackList, targetChannelTimeList, pMovie, pFilmClip );

	// Construct the offset transform matrix which will be used to maintain the relative 
	// offset and orientation of the current dag to the target dag at the current time
	CDmeTransform *pTransform = GetTransform();

	matrix3x4_t targetMat;
	matrix3x4_t invTargetMat;
	pTargetDag->GetAbsTransform( targetMat );
	MatrixInvert( targetMat, invTargetMat );

	matrix3x4_t orginalTransform;

	if ( bMaintainOffset )
	{
		GetAbsTransform( orginalTransform );
	}
	else
	{
		Vector basePosition;
		Vector offsetPosition;
		MatrixPosition( targetMat, basePosition );
		offsetPosition = basePosition + offset;
		orginalTransform = targetMat;
		PositionMatrix( offsetPosition, orginalTransform );
	}
	
	matrix3x4_t offsetTransform;
	ConcatTransforms( invTargetMat, orginalTransform, offsetTransform );

	
	// Find the position and orientation channels associated with the transform
	CDmeChannel *pLocalPosChannel = bPosition ? FindChannelTargetingElement( pFilmClip, pTransform, "position", NULL ) : NULL;
	CDmeChannel *pLocalRotChannel = bOrientation ? FindChannelTargetingElement( pFilmClip, pTransform, "orientation", NULL ) : NULL;

	if ( pLocalPosChannel || pLocalRotChannel )
	{
		// Start a new undo group for the operation
		g_pDataModel->StartUndo( "Attach Transform", "Attach Transform" );

		CDmeLog *pPosLog = ( pLocalPosChannel != NULL ) ? pLocalPosChannel->GetLog() : NULL;
		CDmeLog *pRotLog = ( pLocalRotChannel != NULL ) ? pLocalRotChannel->GetLog() : NULL;
		CDmeChannelsClip *pPosChannelsClip = ( pLocalPosChannel != NULL ) ? FindAncestorReferencingElement< CDmeChannelsClip >( pLocalPosChannel ) : NULL;
		CDmeChannelsClip *pRotChannelsClip = ( pLocalRotChannel != NULL ) ? FindAncestorReferencingElement< CDmeChannelsClip >( pLocalRotChannel ) : NULL;
		CDmeChannelsClip *pChannelsClip = ( pPosChannelsClip != NULL ) ? pPosChannelsClip : pRotChannelsClip;
		Assert( pPosChannelsClip == pRotChannelsClip || pPosChannelsClip == NULL || pRotChannelsClip == NULL );
		Assert( pPosLog != pRotLog );
		Assert( pChannelsClip );
		
		if ( pChannelsClip )
		{
			// Construct the clip stack from the movie down to the channels clip containing the channel
			DmeClipStack_t clipStack;
			if ( pPosChannelsClip )
			{
				pPosChannelsClip->BuildClipStack( &clipStack, pMovie, pFilmClip );
			}
			else 
			{
				pRotChannelsClip->BuildClipStack( &clipStack, pMovie, pFilmClip );
			}

			// Add a new layer to the active logs, the new layer will contain the values 
			// required to position and orient the transform relative to the target.
			CDmeLogLayer *pPosLayer = ( pPosLog != NULL ) ? pPosLog->AddNewLayer() : NULL;
			CDmeLogLayer *pRotLayer = ( pRotLog != NULL ) ? pRotLog->AddNewLayer() : NULL;
			CDmeTypedLogLayer< Vector > *pVectorLayer = CastElement< CDmeTypedLogLayer< Vector > >( pPosLayer );
			CDmeTypedLogLayer< Quaternion > *pQuatLayer = CastElement< CDmeTypedLogLayer< Quaternion > >( pRotLayer );

			// Iterate through the time selection based on the resample interval
			// and generate keys in the active logs at each time time step.
			DmeTime_t time = timeSelection.m_nTimes[ 0 ];
			DmeTime_t endTime = timeSelection.m_nTimes[ 3 ];
			bool done = false;

			// Disable the undo, the only thing that is actually 
			// needed is the add new layer and the flatten layers.
			CDisableUndoScopeGuard undosg;

			while ( !done )
			{
				// Make sure the time does not pass the end of the time selection and set the 
				// done  flag if it has. By setting the flag instead of breaking immediately we 
				// are guaranteed a sample will be made at the very end of the time selection.
				if ( time >= endTime )
				{
					time = endTime;
					done = true;
				}
				
				// Evaluate all of the channels relevant to the target dag and the local dag at the 
				// specified time so that the transform evaluations will correspond to the correct time.
				PlayChannelsAtTime( time, targetChannelList, targetOperatorList, targetClipStackList );
				PlayChannelsAtTime( time, localChannelList, localOperatorList, localClipStackList );

				// Get the world space transform of the target
				matrix3x4_t targetTransform;
				pTargetDag->GetAbsTransform( targetTransform );
				
				// Apply the offset transform to the target transform and make the
				// result the world space transform of the current dag node.
				matrix3x4_t finalTransform;
				ConcatTransforms( targetTransform, offsetTransform, finalTransform );
				SetAbsTransform( finalTransform );

				// Calculate the time relative to the log
				DmeTime_t logTime = clipStack.ToChildMediaTime( time );

				// Add the position of the transform to the position log of the dag node's transform
				if ( pVectorLayer )
				{
					Vector position = pTransform->GetPosition();
					pVectorLayer->SetKey( logTime, position, SEGMENT_INTERPOLATE, CURVE_DEFAULT, false );
				}

				// Add the orientation of the transform to the orientation log of the dag node's transform
				if ( pQuatLayer )
				{
					Quaternion rotation = pTransform->GetOrientation();
					pQuatLayer->SetKey( logTime, rotation, SEGMENT_INTERPOLATE, CURVE_DEFAULT, false );
				}
				
				// Move the evaluation time up by the resample interval
				time += timeSelection.m_nResampleInterval;
			}
				
			// Convert the time selection into the local time space of the log.
			DmeLog_TimeSelection_t logTimeSelection = timeSelection;
			clipStack.ToChildMediaTime( logTimeSelection.m_nTimes );
			
			// Blend the top level log layers contain the new position and orientation with the base log 
			// layer according to the time selection parameters and then flatten the log layers.
			if ( pPosLog )
			{
				pPosLog->BlendLayersUsingTimeSelection( logTimeSelection );

				CEnableUndoScopeGuard undosg;
				pPosLog->FlattenLayers( timeSelection.m_flThreshold, 0 );
			}

			if ( pRotLog )
			{
				pRotLog->BlendLayersUsingTimeSelection( logTimeSelection );

				CEnableUndoScopeGuard undosg;
				pRotLog->FlattenLayers( timeSelection.m_flThreshold, 0 );
			}

			// Restore all of the channels to the original times so that this operation does not have side effects.
			PlayChannelsAtLocalTimes( targetChannelTimeList, targetChannelList, targetOperatorList );
			PlayChannelsAtLocalTimes( localChannelTimeList, localChannelList, localOperatorList );
		}

		g_pDataModel->FinishUndo();
	}
}



//-----------------------------------------------------------------------------
// Compute the absolute and the local transform of the dag node at the 
// specified global time.
//-----------------------------------------------------------------------------
void CDmeDag::ComputeTransformAtTime( DmeTime_t globalTime, const CDmeClip* pMovie, matrix3x4_t &matAbsTransform, matrix3x4_t &localTransform )
{
	// This operation should have no side effects and 
	// no entries should be added to the undo stack.
	CDisableUndoScopeGuard disableUndoSG;

	// Find the film clip containing this dag
	CDmeFilmClip *pFilmClip = FindFilmClipContainingDag( this );
	if ( pFilmClip == NULL )
	{
		pFilmClip = FindReferringElement< CDmeFilmClip >( this, "camera" );
	}

	// Get the transform associated with the dag
	CDmeTransform *pTransform = GetTransform();

	if ( ( pMovie == NULL ) || ( pFilmClip == NULL ) || ( pTransform == NULL ) )
		return;


	// Build a list of channels and other operators which must be
	// evaluated in order to evaluate the transform of this dag node.
	CUtlVector< CDmeChannel* > channelList;
	CUtlVector< CDmeOperator* > operatorList;
	CUtlVector< DmeClipStack_t > clipStackList;
	CUtlVector< DmeTime_t > channelTimeList;
	FindRelevantOperators( channelList, operatorList );
	BuildClipStackList( channelList, clipStackList, channelTimeList, pMovie, pFilmClip );

	// Evaluate the channels and operators relevant to the dag at the specified time
	PlayChannelsAtTime( globalTime, channelList, operatorList, clipStackList );

	// Get the absolute world transform and the local transform 
	GetAbsTransform( matAbsTransform );
	pTransform->GetTransform( localTransform );

	// Restore all of the channels to the original times so that this operation does not have side effects.
	PlayChannelsAtLocalTimes( channelTimeList, channelList, operatorList );
}


//-----------------------------------------------------------------------------
// Move the the dag node to the position of the specified node at the current 
// time, for the rest of the time selection apply the same world space offset
// that was required to move the dag to the target position at the current time.
//-----------------------------------------------------------------------------
void CDmeDag::MoveToTarget( const CDmeDag *pTargetDag, const DmeLog_TimeSelection_t &timeSelection, const CDmeClip *pMovie )
{	
	// Must specify a target dag and it must not be this dag.
	if ( ( pTargetDag == NULL ) || ( pTargetDag == this ) )
	{
		Assert( pTargetDag );
		return;
	}

	// Find the film clip for the dag
	CDmeFilmClip *pFilmClip = FindFilmClipContainingDag( this );
	if ( pFilmClip == NULL )
	{
		pFilmClip = FindReferringElement< CDmeFilmClip >( this, "camera" );
	}

	// Find the channels which must be updated in order to evaluate transforms of both this dag node.
	CUtlVector< CDmeChannel* > channelList;
	CUtlVector< CDmeOperator* > operatorList;
	FindRelevantOperators( channelList,operatorList );

	// Build the clip stacks for the channel lists.
	CUtlVector< DmeClipStack_t > clipStackList;
	CUtlVector< DmeTime_t > channelTimeList;
	BuildClipStackList( channelList, clipStackList, channelTimeList, pMovie, pFilmClip );
		

	// Compute the world space offset between the dag node and the target
	Vector vWorldPos, vTargetPos;
	GetAbsPosition( vWorldPos );
	pTargetDag->GetAbsPosition( vTargetPos );
	Vector vOffset = vTargetPos - vWorldPos;


	// Find the position channel of driving the dag node which will be updated.
	CDmeTransform *pTransform = GetTransform();
	CDmeChannel *pLocalPosChannel = FindChannelTargetingElement( pFilmClip, pTransform, "position", NULL );

	if ( pLocalPosChannel != NULL )
	{
		// Start a new undo group for the operation
		g_pDataModel->StartUndo( "Move dag to target", "Move dag to target" );

		CDmeLog *pPosLog = ( pLocalPosChannel != NULL ) ? pLocalPosChannel->GetLog() : NULL;
		CDmeChannelsClip *pChannelsClip = FindAncestorReferencingElement< CDmeChannelsClip >( pLocalPosChannel );
		Assert( pChannelsClip );

		if ( pChannelsClip && pPosLog )
		{
			// Construct the clip stack from the movie down to the channels clip containing the channel
			DmeClipStack_t clipStack;
			pChannelsClip->BuildClipStack( &clipStack, pMovie, pFilmClip );
			
			// Add a new layer to the active logs, the new layer will contain the values 
			// required to position and orient the transform relative to the target.
			CDmeLogLayer *pPosLayer = pPosLog->AddNewLayer();
			CDmeTypedLogLayer< Vector > *pVectorLayer = CastElement< CDmeTypedLogLayer< Vector > >( pPosLayer );

			
			// Iterate through the time selection based on the resample interval
			// and generate keys in the active logs at each time time step.
			DmeTime_t time = timeSelection.m_nTimes[ 0 ];
			DmeTime_t endTime = timeSelection.m_nTimes[ 3 ];
			bool done = false;

			// Disable the undo, the only thing that is actually 
			// needed is the add new layer and the flatten layers.
			CDisableUndoScopeGuard undosg;

			int nAproxNumTimes = ( ( endTime - time ) / timeSelection.m_nResampleInterval ) + 2;
			CUtlVector< Vector > samplePositions( 0, nAproxNumTimes );
			CUtlVector< DmeTime_t > sampleTimes( 0, nAproxNumTimes );

			while ( !done )
			{
				// Make sure the time does not pass the end of the time selection and set the 
				// done  flag if it has. By setting the flag instead of breaking immediately we 
				// are guaranteed a sample will be made at the very end of the time selection.
				if ( time >= endTime )
				{
					time = endTime;
					done = true;
				}

				// Evaluate all of the channels relevant to the target dag and the local dag at the 
				// specified time so that the transform evaluations will correspond to the correct time.
				PlayChannelsAtTime( time, channelList, operatorList, clipStackList );

				// Get the current world space position of the node
				Vector vOriginalWorldPos;
				GetAbsPosition( vOriginalWorldPos );
				Vector vNewWorldPos = vOriginalWorldPos + vOffset;
				SetAbsPosition( vNewWorldPos );
				Vector vLocalPosition = pTransform->GetPosition();

				// Store the sample time and the local position
				samplePositions.AddToTail( vLocalPosition );
				sampleTimes.AddToTail( time );

				// Move the evaluation time up by the resample interval
				time += timeSelection.m_nResampleInterval;
			}


			// Iterate through all of the samples and construct 
			int nNumSamples = sampleTimes.Count();
			for ( int i = 0; i < nNumSamples; ++i )
			{				
				// Calculate the time relative to the log
				DmeTime_t logTime = clipStack.ToChildMediaTime( sampleTimes[ i ] );

				// Add the position of the transform to the position log of the dag node's transform
				if ( pVectorLayer )
				{
					Vector vLocalPosition = samplePositions[ i ];
					pVectorLayer->SetKey( logTime, vLocalPosition, SEGMENT_INTERPOLATE, CURVE_DEFAULT, false );
				}
			}

			// Convert the time selection into the local time space of the log.
			DmeLog_TimeSelection_t logTimeSelection = timeSelection;
			clipStack.ToChildMediaTime( logTimeSelection.m_nTimes );

			// Blend the top level log layers contain the new position and orientation with the base log 
			// layer according to the time selection parameters and then flatten the log layers.
			pPosLog->BlendLayersUsingTimeSelection( logTimeSelection );
			
			{
				CEnableUndoScopeGuard undosg;
				pPosLog->FlattenLayers( timeSelection.m_flThreshold, 0 );
			}

			// Restore all of the channels to the original times so that this operation does not have side effects.
			PlayChannelsAtLocalTimes( channelTimeList, channelList, operatorList );
		}

		g_pDataModel->FinishUndo();
	}

}


void CDmeDag::GetAbsTransform( matrix3x4_t &matAbsTransform ) const
{
	matrix3x4_t parentToWorld;
	GetParentWorldMatrix( parentToWorld );

	matrix3x4_t localMatrix;
	GetLocalMatrix( localMatrix );

	ConcatTransforms( parentToWorld, localMatrix, matAbsTransform );
}


void CDmeDag::SetAbsTransform( const matrix3x4_t &matAbsTransform )
{
	CDmeTransform *pTransform = GetTransform();
	if ( pTransform == NULL )
		return;

	matrix3x4_t parentToWorld;
	GetParentWorldMatrix( parentToWorld );

	matrix3x4_t worldToParent;
	MatrixInvert( parentToWorld, worldToParent );

	matrix3x4_t localSpace;
	ConcatTransforms( worldToParent, matAbsTransform, localSpace );

	Vector localPos;
	Quaternion localRot;
	MatrixAngles( localSpace, localRot, localPos );
	
	// The position only override depends on the local transform when calculating the parent transform,
	// meaning that simply inverting the parent to world matrix and concatenating it with the desired 
	// absolute transform does not produce the correct local matrix since changing the local matrix 
	// changes the result of GetParentWorldMatrix(). So when the position only override is enabled we 
	// must compute the rotation and position separately.
	if ( HasOverrideParent() )
	{
		matrix3x4_t mTranslationParentToWorld;
		GetTranslationParentWorldMatrix( mTranslationParentToWorld );
	
		matrix3x4_t mTranslationWorldToParent;
		MatrixInvert( mTranslationParentToWorld, mTranslationWorldToParent );

		matrix3x4_t mTranslationLocalSpace;
		ConcatTransforms( mTranslationWorldToParent, matAbsTransform, mTranslationLocalSpace );

		MatrixPosition( mTranslationLocalSpace, localPos);
	}

	pTransform->SetPosition( localPos );
	pTransform->SetOrientation( localRot );
}


CDmeDag *CDmeDag::GetParent() const
{
	const static CUtlSymbolLarge symChildren = g_pDataModel->GetSymbol( "children" );
	CDmeDag *pParent = FindReferringElement< CDmeDag >( this, symChildren, false );
	return pParent;
}



//-----------------------------------------------------------------------------
// Determine if the dag has an override parent
//-----------------------------------------------------------------------------
bool CDmeDag::HasOverrideParent() const
{
	if ( m_bDisableOverrideParent )
		return false;

	return ( GetValueElement< CDmeDag >( OVERRIDE_PARENT ) != NULL );
}


//-----------------------------------------------------------------------------
// Get the current override parent, returns NULL if no override parent is set
//-----------------------------------------------------------------------------
const CDmeDag *CDmeDag::GetOverrideParent( bool bIgnoreEnable ) const
{
	if ( m_bDisableOverrideParent && !bIgnoreEnable )
		return NULL;

	return GetValueElement< CDmeDag >( OVERRIDE_PARENT );
}


//-----------------------------------------------------------------------------
// Get the current override parent, returns NULL if no override parent is set
//-----------------------------------------------------------------------------
const CDmeDag *CDmeDag::GetOverrideParent( bool &bPosition, bool &bRotation, bool bIgnoreEnable ) const
{
	bPosition = false;
	bRotation = false;

	if ( m_bDisableOverrideParent && !bIgnoreEnable )
		return NULL;

	CDmeDag *pOverrideParent = GetValueElement< CDmeDag >( OVERRIDE_PARENT );

	if ( pOverrideParent )
	{
		bPosition = GetValue< bool >( "overridePos", false );
		bRotation = GetValue< bool >( "overrideRot", false );
	}

	if ( bPosition || bRotation )
	{
		return pOverrideParent;
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Set the current override parent, if the parameter is NULL the overrider 
// parent will be cleared.
//-----------------------------------------------------------------------------
void CDmeDag::SetOverrideParent( const CDmeDag *pParentDag, bool bPosition, bool bRotation )
{
	if ( ( pParentDag == NULL ) || ( !bPosition && !bRotation ) )
	{
		RemoveAttribute( OVERRIDE_PARENT );
		RemoveAttribute( "overridePos" );
		RemoveAttribute( "overrideRot" );
	}
	else
	{
		CDmAttribute *pOverrideAttr = SetValue( OVERRIDE_PARENT, pParentDag );
		if ( pOverrideAttr )
		{
			// Set the never copy flag so that the reference 
			// will not be walked when deleting an animation set.
			pOverrideAttr->AddFlag( FATTRIB_NEVERCOPY );
		}
		
		SetValue< bool >( "overridePos", bPosition );
		SetValue< bool >( "overrideRot", bRotation );
	}
}


//-----------------------------------------------------------------------------
// Set the flag which enables or disables the override parent 
//-----------------------------------------------------------------------------
void CDmeDag::EnableOverrideParent( bool bEnable )
{
	m_bDisableOverrideParent = !bEnable;	
}


//-----------------------------------------------------------------------------
// Determine if the override parent is enabled. This only says if an override 
// parent is allowed, not if the dag has an override parent)
//-----------------------------------------------------------------------------
bool CDmeDag::IsOverrideParentEnabled() const
{
	return !m_bDisableOverrideParent;
}


//-----------------------------------------------------------------------------
// Determine if this dag node is ancestor of the specified dag 
//-----------------------------------------------------------------------------
bool CDmeDag::IsAncestorOfDag( const CDmeDag *pDag ) const
{
	if ( pDag == NULL )
		return false;

	const CDmeDag *pCurrentDag = pDag;
	const CDmeDag *pParent = pDag->GetParent();

	while ( pParent )
	{
		if ( pParent == this )
			return true;

		pCurrentDag = pParent;
		pParent = pParent->GetParent();
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Find the dag node which is the root of the tree the dag node is in.
//-----------------------------------------------------------------------------
CDmeDag *CDmeDag::FindRoot()
{
	CDmeDag *pRoot = this;
	CDmeDag *pParent = GetParent();
	while ( pParent )
	{
		pRoot = pParent;
		pParent = pParent->GetParent();
	}
	return pRoot;
}


void CDmeDag::GetAbsPosition( Vector &absPos ) const
{
	matrix3x4_t abs;
	GetAbsTransform( abs );
	MatrixGetColumn( abs, 3, absPos );
}

void CDmeDag::SetAbsPosition( const Vector &absPos )
{
	matrix3x4_t abs;
	GetAbsTransform( abs );
	MatrixSetColumn( absPos, 3, abs );
	SetAbsTransform( abs );
}

void CDmeDag::GetAbsOrientation( Quaternion &absOrientation ) const
{
	matrix3x4_t abs;
	GetAbsTransform( abs );
	Vector absPos;
	MatrixQuaternion( abs, absOrientation );
}

void CDmeDag::SetAbsOrientation( const Quaternion &absOrientation )
{
	matrix3x4_t abs;
	GetAbsTransform( abs );
	Vector absPos;
	MatrixGetColumn( abs, 3, absPos );
	QuaternionMatrix( absOrientation, absPos, abs );
	SetAbsTransform( abs );
}

void RemoveDagFromParents( CDmeDag *pDag, bool bRecursivelyRemoveEmptyDagsFromParents /*= false*/ )
{
	DmAttributeReferenceIterator_t hAttr = g_pDataModel->FirstAttributeReferencingElement( pDag->GetHandle() );
	for ( ; hAttr != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID; hAttr = g_pDataModel->NextAttributeReferencingElement( hAttr ) )
	{
		CDmAttribute *pAttr = g_pDataModel->GetAttribute( hAttr );
		if ( !pAttr )
			continue;

		CDmeDag *pParent = CastElement< CDmeDag >( pAttr->GetOwner() );
		if ( !pParent )
			continue;

		pParent->RemoveChild( pDag );
		if ( bRecursivelyRemoveEmptyDagsFromParents && pParent->GetChildCount() == 0 )
		{
			RemoveDagFromParents( pParent );
		}
	}
}
