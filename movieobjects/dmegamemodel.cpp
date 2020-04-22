//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Dme version of a game model (MDL)
//
//=============================================================================
#include "movieobjects/dmegamemodel.h"
#include "movieobjects_interfaces.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "studio.h"
#include "tier3/tier3.h"
#include "tier1/fmtstr.h"
#include "bone_setup.h"

#include "movieobjects/dmeoverlay.h"		// FIXME: Why do I have to explicitly include dmeoverlay.h here?

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeGlobalFlexControllerOperator, CDmeGlobalFlexControllerOperator );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeGlobalFlexControllerOperator::OnConstruction()
{
	m_flexWeight.Init( this, "flexWeight" );
	m_gameModel.Init( this, "gameModel", FATTRIB_HAS_CALLBACK | FATTRIB_NEVERCOPY );

	m_ToAttributeHandle = DMATTRIBUTE_HANDLE_INVALID;

	m_nFlexControllerIndex = -1;
}

void CDmeGlobalFlexControllerOperator::OnDestruction()
{
}

void CDmeGlobalFlexControllerOperator::Resolve()
{
	if ( m_nFlexControllerIndex < 0 )
	{
		m_nFlexControllerIndex = FindGlobalFlexControllerIndex();
	}
	if ( m_ToAttributeHandle == DMATTRIBUTE_HANDLE_INVALID )
	{
		SetupToAttribute();
	}
}

void CDmeGlobalFlexControllerOperator::OnAttributeChanged( CDmAttribute *pAttribute )
{
	// Don't have the required interface...
	if ( !g_pGlobalFlexController )
		return;

	if ( pAttribute == m_gameModel.GetAttribute() && m_gameModel.GetElement() )
	{
		m_nFlexControllerIndex = FindGlobalFlexControllerIndex();
		
		SetupToAttribute();
	}
}

void CDmeGlobalFlexControllerOperator::Operate()
{
	CDmAttribute *pToAttr = g_pDataModel->GetAttribute( m_ToAttributeHandle );
	if ( !pToAttr )
		return;

	DmAttributeType_t type = m_flexWeight.GetAttribute()->GetType();

	const void *pValue = m_flexWeight.GetAttribute()->GetValueUntyped();
	if ( IsArrayType( pToAttr->GetType() ) )
	{
		if ( m_nFlexControllerIndex == -1 )
			return;

		CDmrGenericArray array( pToAttr );
		array.Set( m_nFlexControllerIndex, type, pValue );
	}
	else
	{
		pToAttr->SetValue( type, pValue );
	}
}

void CDmeGlobalFlexControllerOperator::SetGameModel( CDmeGameModel *gameModel )
{
	m_gameModel = gameModel;
}

void CDmeGlobalFlexControllerOperator::SetWeight( float flWeight )
{
	m_flexWeight = flWeight;
}

void CDmeGlobalFlexControllerOperator::SetMapping( int globalIndex )
{
	m_nFlexControllerIndex = globalIndex;
	if ( m_gameModel.GetElement() )
	{
		if ( (uint)globalIndex >= m_gameModel->NumFlexWeights() )
		{
			m_gameModel->SetNumFlexWeights( (uint)( globalIndex + 1 ) );
		}
	}
}

int	CDmeGlobalFlexControllerOperator::GetGlobalIndex() const
{
	return m_nFlexControllerIndex;
}

void CDmeGlobalFlexControllerOperator::GetInputAttributes ( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_flexWeight.GetAttribute() );
}

void CDmeGlobalFlexControllerOperator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	CDmAttribute *toAttribute = g_pDataModel->GetAttribute( m_ToAttributeHandle );
	if ( toAttribute )
	{
		attrs.AddToTail( toAttribute );
	}
}

void CDmeGlobalFlexControllerOperator::SetupToAttribute()
{
	CDmElement *pObject = m_gameModel.GetElement();
	if ( pObject == NULL)
		return;

	CDmAttribute *pAttr = pObject->GetAttribute( "flexWeights" );
	Assert( pAttr );
	if ( !pAttr )
		return;

	m_ToAttributeHandle = pAttr->GetHandle();
	return;
}


//-----------------------------------------------------------------------------
// Connect up stuff by index
//-----------------------------------------------------------------------------
int CDmeGlobalFlexControllerOperator::FindGlobalFlexControllerIndex() const
{
	int nGlobalFlexControllerIndex = -1;
	MDLHandle_t h = m_gameModel->GetModelHandle();
	if ( h != MDLHANDLE_INVALID )
	{
		studiohdr_t *hdr = g_pMDLCache->GetStudioHdr( h );
		Assert( hdr );
		if ( hdr )
		{
			int fc = hdr->numflexcontrollers;
			for ( LocalFlexController_t i = LocalFlexController_t(0) ; i < fc; ++i )
			{
				mstudioflexcontroller_t *flex = hdr->pFlexcontroller( i );
				if ( flex->localToGlobal == -1 && g_pGlobalFlexController )
				{
					flex->localToGlobal = g_pGlobalFlexController->FindGlobalFlexController( flex->pszName() );
				}

				if ( !Q_stricmp( flex->pszName(), GetName() ) )
				{
					nGlobalFlexControllerIndex = flex->localToGlobal;
					// Grow the array
					if ( (uint)flex->localToGlobal >= m_gameModel->NumFlexWeights() )
					{
						m_gameModel->SetNumFlexWeights( (uint)( flex->localToGlobal + 1 ) );
					}
					break;
				}
			}
		}

	}

	return nGlobalFlexControllerIndex;
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeGameModel, CDmeGameModel );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeGameModel::OnConstruction()
{
	m_flexWeights.Init( this, "flexWeights" );
	m_modelName.Init( this, "modelName", FATTRIB_HAS_CALLBACK );
	m_skin.Init( this, "skin" );
	m_body.Init( this, "body" );
	m_sequence.Init( this, "sequence" );
	m_flags.Init( this, "flags" );
	m_bones.Init( this, "bones" );
	m_globalFlexControllers.Init( this, "globalFlexControllers" );
	m_bComputeBounds.Init( this, "computeBounds" );
	m_bEvaluateProceduralBones.InitAndSet( this, "evaluateProceduralBones", true );

	m_hMDL = MDLHANDLE_INVALID;
	m_bHMDLDirty = true;
}

void CDmeGameModel::OnDestruction()
{
	if ( m_hMDL != MDLHANDLE_INVALID )
	{
		g_pMDLCache->Release( m_hMDL );
		m_hMDL = MDLHANDLE_INVALID;
	}
}

void CDmeGameModel::OnAttributeChanged( CDmAttribute *pAttribute )
{
	if ( pAttribute == m_modelName.GetAttribute() )
	{
		m_bHMDLDirty = true;
	}
}

CDmeGlobalFlexControllerOperator *CDmeGameModel::AddGlobalFlexController( const char *controllerName, int globalIndex )
{
	int i, c;
	c = m_globalFlexControllers.Count();
	for ( i = 0; i < c; ++i )
	{
		CDmeGlobalFlexControllerOperator *op = m_globalFlexControllers.Get( i );
		Assert( op );
		if ( op && !Q_stricmp( op->GetName(), controllerName ) )
			break;
	}

	if ( i >= c )
	{
		CDmeGlobalFlexControllerOperator *newOperator = CreateElement< CDmeGlobalFlexControllerOperator >( controllerName, GetFileId() );
		Assert( newOperator );
		if ( !newOperator )
			return NULL;

		i = m_globalFlexControllers.AddToTail( newOperator );
	}

	Assert( m_globalFlexControllers.IsValidIndex( i ) );
	CDmeGlobalFlexControllerOperator *op = m_globalFlexControllers.Get( i );
	Assert( op );
	if ( op )
	{
		op->SetMapping( globalIndex );
		op->SetGameModel( this );
	}

	if ( (uint)globalIndex >= NumFlexWeights() )
	{
		SetNumFlexWeights( globalIndex + 1 );
	}

	return op;
}


//-----------------------------------------------------------------------------
// Find a flex controller by its global index
//-----------------------------------------------------------------------------
CDmeGlobalFlexControllerOperator *CDmeGameModel::FindGlobalFlexController( int nGlobalIndex )
{
	int i, c;
	c = m_globalFlexControllers.Count();
	for ( i = 0; i < c; ++i )
	{
		CDmeGlobalFlexControllerOperator *op = m_globalFlexControllers.Get( i );
		Assert( op );
		if ( op && op->GetGlobalIndex() == nGlobalIndex )
			return op;
	}

	return NULL;
}

studiohdr_t* CDmeGameModel::GetStudioHdr() const
{
	const char *pModelName = GetModelName();
	MDLHandle_t h = pModelName && pModelName[0] ? g_pMDLCache->FindMDL( pModelName ) : MDLHANDLE_INVALID;
	return ( h != MDLHANDLE_INVALID ) ? g_pMDLCache->GetStudioHdr( h ) : NULL;
}


// A src bone transform transforms pre-compiled data (.dmx or .smd files, for example)
// into post-compiled data (.mdl or .ani files)
bool CDmeGameModel::GetSrcBoneTransforms( matrix3x4_t *pPreTransform, matrix3x4_t *pPostTransform, int nBoneIndex ) const
{
	studiohdr_t *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return false;

	if ( pStudioHdr->numbones <= nBoneIndex )
		return false;

	const char *pBoneName = pStudioHdr->pBone( nBoneIndex )->pszName();
	int nCount = pStudioHdr->NumSrcBoneTransforms();
	for ( int i = 0; i < nCount; ++i )
	{
		const mstudiosrcbonetransform_t *pSrcTransform = pStudioHdr->SrcBoneTransform( i );
		if ( Q_stricmp( pSrcTransform->pszName(), pBoneName ) )
			continue;

		MatrixCopy( pSrcTransform->pretransform, *pPreTransform );
		MatrixCopy( pSrcTransform->posttransform, *pPostTransform );
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Get the default position of the specified bone.
//-----------------------------------------------------------------------------
bool CDmeGameModel::GetBoneDefaultPosition( int nBoneIndex, Vector &position ) const
{
	studiohdr_t *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return false;

	if ( ( nBoneIndex < 0 ) || ( nBoneIndex >= pStudioHdr->numbones ) )
		return false;
		
	const mstudiobone_t *pBone = pStudioHdr->pBone( nBoneIndex );
	if ( pBone == NULL )
		return false;

	position = pBone->pos;
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Get the default orientation of the specified bone.
//-----------------------------------------------------------------------------
bool CDmeGameModel::GetBoneDefaultOrientation( int nBoneIndex, Quaternion &orientation ) const
{
	studiohdr_t *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return false;

	if ( ( nBoneIndex < 0 ) || ( nBoneIndex >= pStudioHdr->numbones ) )
		return false;

	const mstudiobone_t *pBone = pStudioHdr->pBone( nBoneIndex );
	if ( pBone == NULL )
		return false;

	orientation = pBone->quat;
	return true;
}


bool CDmeGameModel::IsRootTransform( int nBoneIndex ) const
{
	studiohdr_t *pStudioHdr = GetStudioHdr();
	if ( !pStudioHdr )
		return true;

	if ( pStudioHdr->numbones <= nBoneIndex )
		return true;

	const mstudiobone_t *pBone = pStudioHdr->pBone( nBoneIndex );
	return pBone->parent == -1;
}

int CDmeGameModel::NumGlobalFlexControllers() const
{
	return m_globalFlexControllers.Count();
}

CDmeGlobalFlexControllerOperator *CDmeGameModel::GetGlobalFlexController( int localIndex )
{
	return m_globalFlexControllers.Get( localIndex );
}

void CDmeGameModel::RemoveGlobalFlexController( CDmeGlobalFlexControllerOperator *controller )
{
	int c = m_globalFlexControllers.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmeGlobalFlexControllerOperator *check = m_globalFlexControllers.Get( i );
		if ( check == controller )
		{
			m_globalFlexControllers.Remove( i );
			break;
		}
	}
}

void CDmeGameModel::AppendGlobalFlexControllerOperators( CUtlVector< IDmeOperator * >& list )
{
	int c = m_globalFlexControllers.Count();
	for ( int i = 0 ; i < c; ++i )
	{
		CDmeOperator *op = m_globalFlexControllers.Get( i );
		if ( !op )
			continue;
		list.AddToTail( op );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Find the dependencies of each flex controller on the other flex 
// controllers
//-----------------------------------------------------------------------------
void CDmeGameModel::FindFlexControllerDependencies( CUtlVector< CUtlVector< int > > &dependencyList ) const
{
	studiohdr_t *hdr = GetStudioHdr();
	if ( hdr == NULL )
		return;

	
	// Build a table to reference the controller operators by the
	// global index of the flex control they are associated with.
	int controllerTable[ MAXSTUDIOFLEXDESC ];
	memset( controllerTable, -1, sizeof( controllerTable ) );

	int nControllers = m_globalFlexControllers.Count();
	if ( nControllers > MAXSTUDIOFLEXDESC )
	{
		Assert( nControllers < MAXSTUDIOFLEXDESC );
		return;
	}

	for ( int iCtrl = 0; iCtrl < nControllers; ++iCtrl )
	{
		CDmeGlobalFlexControllerOperator *pCtrlOp = m_globalFlexControllers[ iCtrl ];
		if ( pCtrlOp )
		{
			int globalIndex = pCtrlOp->GetGlobalIndex();
			if ( ( globalIndex < MAXSTUDIOFLEXDESC ) && ( globalIndex >= 0 ) )
			{
				controllerTable[ globalIndex ] = iCtrl;
			}
		}
	}


	// Determine which rules each of the controllers contribute to
	CStudioHdr studioHdr( hdr );

	CUtlVector< int > controllerRuleTable[ MAXSTUDIOFLEXDESC ]; // Table of rules each controller contributes to
	CUtlVector< int > ruleDepTable[ MAXSTUDIOFLEXDESC ];		// Table of controllers contributing to each rule
	
	int nFlexRules = studioHdr.numflexrules();

	for ( int iRule = 0; iRule < nFlexRules; ++iRule )
	{
		mstudioflexrule_t *pRule = studioHdr.pFlexRule( iRule );

		if ( pRule == NULL )
			return;

		for ( int iOp = 0; iOp < pRule->numops; ++iOp )
		{
			mstudioflexop_t *pOp = pRule->iFlexOp( iOp );

			switch ( pOp->op )
			{
				case STUDIO_FETCH1:
				case STUDIO_2WAY_0:
				case STUDIO_2WAY_1:
				case STUDIO_NWAY:
				case STUDIO_DME_LOWER_EYELID:
				case STUDIO_DME_UPPER_EYELID:
					{
						int globalIndex = studioHdr.pFlexcontroller( (LocalFlexController_t)pOp->d.index )->localToGlobal;
						if ( ( globalIndex < MAXSTUDIOFLEXDESC ) && ( pRule->flex < MAXSTUDIOFLEXDESC ) && ( globalIndex >= 0 ) )
						{						
							int controllerIndex = controllerTable[ globalIndex ];
							if ( controllerIndex >= 0 )
							{								
								if ( controllerRuleTable[ controllerIndex ].Find( pRule->flex ) == CUtlVector< int >::InvalidIndex() )
								{
									controllerRuleTable[ controllerIndex ].AddToTail( pRule->flex );
									ruleDepTable[ pRule->flex ].AddToTail( controllerIndex );
								}
							}
						}
					}
					break;
				default:
					break;
			}
		}
	}

	
	// For each controller, find the other controllers that contribute to the same rules. 
	bool dependencyTable[ MAXSTUDIOFLEXDESC ];
	memset( dependencyTable, 0, sizeof( dependencyTable ) );
	for ( int iCtrl = 0; iCtrl < nControllers; ++iCtrl )
	{
		memset( dependencyTable, 0, sizeof( bool ) * nControllers );

		// Get the list of rules that controller contributes to
		CUtlVector< int > &ruleList = controllerRuleTable[ iCtrl ];
		int nRules = ruleList.Count();
		int nDependencies = 0;
		
		for ( int iRule = 0; iRule < nRules; ++iRule )
		{
			int ruleIndex = ruleList[ iRule ];
			CUtlVector< int > &ruleControllerList = ruleDepTable[ ruleIndex ];
			int nRuleDep = ruleControllerList.Count();

			for ( int iDep = 0; iDep < nRuleDep; ++iDep )
			{
				int controllerIndex = ruleControllerList[ iDep ];
				if ( ( controllerIndex != iCtrl ) && ( dependencyTable[ controllerIndex ] == false ) )
				{
					dependencyTable[ controllerIndex ] = true;
					++nDependencies;
				}
			}
		}
		if ( nDependencies > 0 )
		{
			dependencyList.AddToTail();
			CUtlVector< int > &dependencies = dependencyList.Tail();
			dependencies.EnsureCapacity( nDependencies + 1 );
			dependencies.AddToTail( iCtrl );
			for ( int iDepCtrl = 0; iDepCtrl < nControllers; ++iDepCtrl )
			{
				if ( dependencyTable[ iDepCtrl ] == true )
				{
					dependencies.AddToTail( iDepCtrl );
					--nDependencies;
				}
			}
			Assert( nDependencies == 0 );
		}
	}	
}

//-----------------------------------------------------------------------------
// accessors
//-----------------------------------------------------------------------------
void CDmeGameModel::AddBone( CDmeTransform* pTransform )
{
	m_bones.AddToTail( pTransform );
}

//-----------------------------------------------------------------------------
// Is this dag under the game model?
//-----------------------------------------------------------------------------
static bool IsDagUnderGameModel( CDmeDag *pDag, CDmeGameModel *pGameModel )
{
	if ( pDag == pGameModel )
		return true;

	DmAttributeReferenceIterator_t i = g_pDataModel->FirstAttributeReferencingElement( pDag->GetHandle() );
	while ( i != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID )
	{
		CDmAttribute *pAttribute = g_pDataModel->GetAttribute( i );
		CDmElement *pDmeParent = pAttribute->GetOwner();
		const static CUtlSymbolLarge symChildren = g_pDataModel->GetSymbol( "children" );
		if ( pDmeParent && pAttribute->GetNameSymbol() == symChildren )
		{
			CDmeDag *pParent = CastElement< CDmeDag >( pDmeParent );
			if ( pParent && ( pParent->GetFileId() == pDag->GetFileId() ) )
			{
				if ( IsDagUnderGameModel( pParent, pGameModel ) )
					return true;
			}
		}
		i = g_pDataModel->NextAttributeReferencingElement( i );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Finds existing dags
//-----------------------------------------------------------------------------
void CDmeGameModel::PopulateExistingDagList( CDmeDag** pDags, int nCount )
{
	int nCurrentBoneCount = m_bones.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( i >= nCurrentBoneCount )
		{
			pDags[ i ] = NULL;
			continue;
		}

		CDmeTransform *pTransform = GetBone( i );
		Assert( pTransform );
		pDags[ i ] = pTransform ? pTransform->GetDag() : NULL;
	}
}


//-----------------------------------------------------------------------------
// Adds bones to the game model
//-----------------------------------------------------------------------------
void CDmeGameModel::AddBones( studiohdr_t *pStudioHdr, int nFirstBone, int nCount )
{
	if ( nFirstBone + nCount > pStudioHdr->numbones )
	{
		nCount = pStudioHdr->numbones - nFirstBone;
		if ( nCount <= 0 )
			return;
	}

	// make room for bones
	CDmeDag** pDags = ( CDmeDag** )_alloca( pStudioHdr->numbones * sizeof(CDmeDag*) );
	int nDagCount = nFirstBone;
	PopulateExistingDagList( pDags, nFirstBone );

	char name[ 256 ];
	for ( int i = 0; i < nCount; ++i )
	{
		int bi = i + nFirstBone;

		// get parent
		const mstudiobone_t *pBone = pStudioHdr->pBone( bi );
		int parentIndex = pBone->parent;
		Assert( parentIndex < nDagCount );

		// build dag hierarchy to match bone hierarchy
		CDmeDag *pParent = ( parentIndex < 0 ) ? this : pDags[ parentIndex ];
		Q_snprintf( name, sizeof( name ), "bone %d (%s)", bi, pBone->pszName() );
		CDmeDag *pDag = CreateElement< CDmeDag >( name, GetFileId() );
		pDags[nDagCount++] = pDag;
		pParent->AddChild( pDag );

		CDmeTransform *pTransform = pDag->GetTransform();
		pTransform->SetName( name );

		// add different bone representations to dme model and input
		AddBone( pTransform );
	}
}

void CDmeGameModel::SetBone( uint index, const Vector& pos, const Quaternion& rot )
{
	m_bones[ index ]->SetPosition( pos );
	m_bones[ index ]->SetOrientation( rot );
}

void CDmeGameModel::RemoveAllBones()
{
	m_bones.RemoveAll();
}

uint CDmeGameModel::NumBones() const
{
	return m_bones.Count();
}

CDmeTransform *CDmeGameModel::GetBone( uint index ) const
{
	return m_bones[ index ];
}

int CDmeGameModel::FindBone( CDmeTransform *pTransform ) const
{
	return m_bones.Find( pTransform );
}

uint CDmeGameModel::NumFlexWeights() const
{
	return m_flexWeights.Count();
}

const CUtlVector< float >& CDmeGameModel::GetFlexWeights() const
{
	return m_flexWeights.Get();
}

void CDmeGameModel::SetNumFlexWeights( uint nFlexWeights )
{
	if ( nFlexWeights > (uint)m_flexWeights.Count() )
	{
		while ( (uint)m_flexWeights.Count() < nFlexWeights )
		{
			m_flexWeights.AddToTail( 0.0f );
		}
	}
	else
	{
		while ( (uint)m_flexWeights.Count() > nFlexWeights )
		{
			m_flexWeights.Remove( (uint)m_flexWeights.Count() - 1 );
		}
	}
}

void CDmeGameModel::SetFlexWeights( uint nFlexWeights, const float* flexWeights )
{
	m_flexWeights.CopyArray( flexWeights, nFlexWeights );
}

void CDmeGameModel::SetFlags( int nFlags )
{
	m_flags = nFlags;
}

void CDmeGameModel::SetSkin( int nSkin )
{
	m_skin = nSkin;
}

void CDmeGameModel::SetBody( int nBody )
{
	m_body = nBody;
}

void CDmeGameModel::SetSequence( int nSequence )
{
	m_sequence = nSequence;
}

int CDmeGameModel::GetSkin() const
{
	return m_skin;
}

int CDmeGameModel::GetBody() const
{
	return m_body;
}

int CDmeGameModel::GetSequence() const
{
	return m_sequence;
}

int CDmeGameModel::GetFlags() const
{
	return m_flags;
}

const char *CDmeGameModel::GetModelName() const
{
	return m_modelName.Get();
}

MDLHandle_t CDmeGameModel::GetModelHandle()
{
	if ( m_bHMDLDirty )
	{
		UpdateHMDL();
	}
	return m_hMDL;
}

void CDmeGameModel::UpdateHMDL()
{
	// Yes, we're intentionally referencing before we unref
	MDLHandle_t h = MDLHANDLE_INVALID;
	const char *pModelName = m_modelName.Get();
	if ( pModelName && *pModelName )
	{
		h = g_pMDLCache->FindMDL( pModelName );
	}

	if ( m_hMDL != MDLHANDLE_INVALID )
	{
		g_pMDLCache->Release( m_hMDL );
	}

	m_hMDL = h;
	m_bHMDLDirty = false;
}


int CDmeGameModel::FindAttachment( const char *pchAttachmentName ) const
{
	if ( studiohdr_t *pStudioHdr = GetStudioHdr() )
	{
		CStudioHdr studioHdr( pStudioHdr );
		return Studio_FindAttachment( &studioHdr, pchAttachmentName ) + 1;
	}
	return 0;
}


//-----------------------------------------------------------------------------
// Compute the world space position of the specified attachment.
//-----------------------------------------------------------------------------
Vector CDmeGameModel::ComputeAttachmentPosition( const char *pchAttachmentName ) const
{
	studiohdr_t *pStudioHdr = GetStudioHdr();
	if ( pStudioHdr == NULL )
		return vec3_origin;

	// Find the index of the attachment by its name and verify 
	// that the attachment was found and that the index is valid.
	CStudioHdr studioHdr( pStudioHdr );
	int attachmentIndex = Studio_FindAttachment( &studioHdr, pchAttachmentName );
	if ( ( attachmentIndex < 0 ) || ( attachmentIndex > studioHdr.GetNumAttachments() ) )
		return vec3_origin;

	// Get the bone to which in attachment position is defined
	// and then find the dag node using the bone transform.
	int boneIndex = studioHdr.GetAttachmentBone( attachmentIndex );
	CDmeTransform *pBoneTranform = GetBone( boneIndex );
	CDmeDag *pBoneDag = pBoneTranform->GetDag();
	if ( pBoneDag  == NULL )
		return vec3_origin;

	// Get the local offset position of the attachment and then transform 
	// it into world space using the transform of the associated dag node.
	const mstudioattachment_t &attachment = studioHdr.pAttachment( attachmentIndex );
	
	matrix3x4_t wsTransform;
	Vector localPosition;
	Vector worldPosition;
	pBoneDag->GetAbsTransform( wsTransform );
	MatrixPosition( attachment.local, localPosition );
	VectorTransform( localPosition, wsTransform, worldPosition );
	
	return worldPosition;
}


//-----------------------------------------------------------------------------
// Create a dag node for the specified attachment and make it a child of the
// the bone it is local to.
//-----------------------------------------------------------------------------
CDmeDag *CDmeGameModel::CreateDagForAttachment( const char *pchAttachmentName ) const
{
	studiohdr_t *pStudioHdr = GetStudioHdr();
	if ( pStudioHdr == NULL )
		return NULL;

	// Find the index of the attachment by its name and verify 
	// that the attachment was found and that the index is valid.
	CStudioHdr studioHdr( pStudioHdr );
	int attachmentIndex = Studio_FindAttachment( &studioHdr, pchAttachmentName );
	if ( ( attachmentIndex < 0 ) || ( attachmentIndex > studioHdr.GetNumAttachments() ) )
		return NULL;

	// Get the bone in which the attachment position is defined
	// and then find the dag node using the bone transform.
	int boneIndex = studioHdr.GetAttachmentBone( attachmentIndex );
	CDmeTransform *pBoneTranform = GetBone( boneIndex );
	CDmeDag *pBoneDag = pBoneTranform->GetDag();
	if ( pBoneDag == NULL )
		return NULL;
	
	CDmeDag *pDagNode = CreateElement< CDmeDag >( CFmtStr( "attach_%s", pchAttachmentName ), GetFileId() );

	if ( pDagNode )
	{
		// Position the node based on the attachment position
		const mstudioattachment_t &attachment = studioHdr.pAttachment( attachmentIndex );
		CDmeTransform *pTransform = pDagNode->GetTransform();
		if ( pTransform )
		{
			Vector vAttachmentPos;
			MatrixPosition( attachment.local, vAttachmentPos );
			pTransform->SetPosition( vAttachmentPos );
		}

		// Make the attachment dag node a child of bone it is associated with
		pBoneDag->AddChild( pDagNode );
	}

	return pDagNode;
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeGameSprite, CDmeGameSprite );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeGameSprite::OnConstruction()
{
	m_modelName	.Init( this, "modelName" );
	m_frame		.Init( this, "frame" );
	m_rendermode.Init( this, "rendermode" );
	m_renderfx	.Init( this, "renderfx" );
	m_renderscale.Init( this, "renderscale" );
	m_color		.Init( this, "color" );
	m_proxyRadius.Init( this, "proxyRadius" );
}

void CDmeGameSprite::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// accessors
//-----------------------------------------------------------------------------
const char *CDmeGameSprite::GetModelName() const
{
	return m_modelName.Get();
}

float CDmeGameSprite::GetScale() const
{
	return m_renderscale;
}

float CDmeGameSprite::GetFrame() const
{
	return m_frame;
}

int CDmeGameSprite::GetRenderMode() const
{
	return m_rendermode;
}

int CDmeGameSprite::GetRenderFX() const
{
	return m_renderfx;
}

const Color &CDmeGameSprite::GetColor() const
{
	return m_color;
}

float CDmeGameSprite::GetProxyRadius() const
{
	return m_proxyRadius;
}

void CDmeGameSprite::SetState( bool bVisible, float nFrame, int nRenderMode, int nRenderFX, float flRenderScale, float flProxyRadius,
							   const Vector &pos, const Quaternion &rot, const Color &color )
{
	m_Visible = bVisible;
	m_frame = nFrame;
	m_rendermode = nRenderMode;
	m_renderfx = nRenderFX;
	m_renderscale = flRenderScale;
	m_proxyRadius = flProxyRadius;
	m_color = color;

	CDmeTransform *pTransform = GetTransform();
	pTransform->SetPosition( pos );
	pTransform->SetOrientation( rot );
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeGamePortal, CDmeGamePortal );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeGamePortal::OnConstruction()
{
	m_flStaticAmount	.Init( this, "staticAmount" );
	m_flSecondaryStaticAmount	.Init( this, "secondaryStaticAmount" );
	m_flOpenAmount		.Init( this, "openAmount" );
	m_flHalfWidth		.Init( this, "halfWidth" );
	m_flHalfHeight		.Init( this, "halfHeight" );
	m_nPortalId			.Init( this, "portalId" );
	m_nLinkedPortalId	.Init( this, "linkedPortalId" );
	m_bIsPortal2		.Init( this, "isPortal2" );
	m_PortalType		.Init( this, "portalType" );
}

void CDmeGamePortal::OnDestruction()
{
}

