//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// A class representing a Dag (directed acyclic graph) node used for holding transforms, lights, cameras and shapes
//
//=============================================================================

#ifndef DMEDAG_H
#define DMEDAG_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlstack.h"
#include "datamodel/dmehandle.h"
#include "datamodel/dmelement.h"
#include "datamodel/dmattribute.h"
#include "datamodel/dmattributevar.h"
#include "movieobjects/dmeshape.h"
#include "movieobjects/dmetransform.h"
#include "movieobjects/dmeoverlay.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeOverlay;
class CDmeTransform;
class CDmeShape;
class CDmeDrawSettings;
class CDmeChannel;
class CDmeClip;
class DmeLog_TimeSelection_t;
class CDmeTransformControl;


//-----------------------------------------------------------------------------
// A class representing a camera
//-----------------------------------------------------------------------------
class CDmeDag : public CDmElement
{
	DEFINE_ELEMENT( CDmeDag, CDmElement );

public:

	virtual void Resolve();

	// Accessors
	CDmeTransform *GetTransform() const;
	CDmeShape *GetShape();

	// Changes the shage
	void SetShape( CDmeShape *pShape );

	bool IsVisible() const;
	void SetVisible( bool bVisible = true );

	// child helpers
	const CUtlVector< DmElementHandle_t > &GetChildren() const;
	int GetChildCount() const;
	CDmeDag *GetChild( int i ) const;
	bool AddChild( CDmeDag* pDag );
	void RemoveChild( int i );
	void RemoveChild( const CDmeDag *pChild, bool bRecurse = false );
	int FindChild( const CDmeDag *pChild ) const;
	int FindChild( CDmeDag *&pParent, const CDmeDag *pChild );
	int FindChild( const char *name ) const;
	CDmeDag *FindOrAddChild( const char *name );
	void RemoveAllChildren();
	bool SetParent( CDmeDag *pDmeDagParent );

	CDmeDag *FindChildByName_R( const char *name ) const;


	// Count the number of steps between the node and the specified child node, returns -1 if not related
	int StepsToChild( const CDmeDag *pChild ) const;

	// Recursively render the Dag hierarchy
	virtual void Draw( CDmeDrawSettings *pDrawSettings = NULL );
	void GetBoundingSphere( Vector &center, float &radius ) const
	{
		matrix3x4_t identity;
		SetIdentityMatrix( identity );
		GetBoundingSphere( center, radius, identity );
	}
	void GetBoundingBox( Vector &min, Vector &max ) const
	{
		matrix3x4_t identity;
		SetIdentityMatrix( identity );
		GetBoundingBox( min, max, identity );
	}

	void GetShapeToWorldTransform( matrix3x4_t &mat );

	void GetLocalMatrix( matrix3x4_t &mat ) const;
	void SetLocalMatrix( matrix3x4_t &mat );

	void GetAbsTransform( matrix3x4_t &matAbsTransform ) const;
	void SetAbsTransform( const matrix3x4_t &matAbsTransform );

	void GetAbsPosition( Vector &absPos ) const;
	void SetAbsPosition( const Vector &absPos );
	void GetAbsOrientation( Quaternion &absOrientation ) const;
	void SetAbsOrientation( const Quaternion &absOrientation );

	void BakeStaticTransforms( bool bRecurse = true );

	void OnAttachToDmeDag( CDmeDag *pParentDag, bool bFixupLogs = true );
	CDmeDag *GetParent() const;
	template <class T>
	T *FindParentDagOfType();

	// Find all nodes in the sub-tree of the dag which are of the specified type.
	template< class T >
	void FindChildrenOfType( CUtlVector< T* > &children );

	// Determine if the dag has an override parent
	bool HasOverrideParent() const;

	// Get the current override parent, returns NULL if no override parent is set
	const CDmeDag *GetOverrideParent( bool bIgnoreEnable = false ) const;
	const CDmeDag *GetOverrideParent( bool &bPosition, bool &bRotation, bool bIgnoreEnable = false ) const;

	// Set the current override parent, if the parameter is NULL the overrider parent will be cleared.
	void SetOverrideParent( const CDmeDag *pParentDag, bool bPosition, bool bRotation );

	// Set the flag which enables or disables the override parent 
	void EnableOverrideParent( bool bEnable );

	// Determine if the override parent is enabled ( This only says if an override parent is allowed, not if the dag has an override parent)
	bool IsOverrideParentEnabled() const;

	// Determine if this dag node is ancestor of the specified dag 
	bool IsAncestorOfDag( const CDmeDag *pDag ) const;

	// Find the dag node which is the root of the tree the dag node is in.
	CDmeDag *FindRoot();

	void BindTransformToDmeDag( const CDmeDag *pTargetDag, const DmeLog_TimeSelection_t &params, const CDmeClip *pMovie, const Vector& offsset, 
								bool bPosition, bool bOrientation, bool bMaintainOffset );

	void ComputeTransformAtTime( DmeTime_t globalTime,const CDmeClip* pMovie, matrix3x4_t &matAbsTransform, matrix3x4_t &localTransform );

	void MoveToTarget( const CDmeDag *pTargetDag, const DmeLog_TimeSelection_t &params, const CDmeClip *pMovie );

	void GetParentWorldMatrix( matrix3x4_t &mat ) const;

	void GetTranslationParentWorldMatrix( matrix3x4_t &mParentToWorld );

	static void DrawUsingEngineCoordinates( bool bEnable );

	static void DrawZUp( bool bZUp );
	static bool IsDrawZUp() { return !s_bDrawZUp; }

	// Transform from DME to engine coordinates
	static void DmeToEngineMatrix( matrix3x4_t& dmeToEngine, bool bZUp );
	static void EngineToDmeMatrix( matrix3x4_t& engineToDme, bool bZUp );

	// Find the channels targeting the transform of the object either directly or through a constraint
	void FindTransformChannels( CUtlVector< CDmeChannel * > &list ) const;

	// Find the transform controls driving the dag node
	CDmeTransformControl *FindTransformControl() const;

	// Find all of the operators on which dag node is dependent, this recursively finds operators for the parents of the dag.
	void FindRelevantOperators( CUtlVector< CDmeOperator * > &operatorList ) const;

	// Find all of the operators on which the dag node is dependent, splitting the channels into a separate list.
	void FindRelevantOperators( CUtlVector< CDmeChannel * > &list, CUtlVector< CDmeOperator * > &operatorList ) const;

	// This one only looks at the current CDmeDag
	void FindLocalOperators( CUtlVector< CDmeOperator * > &operatorList ) const;


protected:

	void GetBoundingSphere( Vector &center, float &radius, const matrix3x4_t &pMat ) const;
	void GetBoundingBox( Vector &min, Vector &max, const matrix3x4_t &pMat ) const;

	void PushDagTransform();
	void PopDagTransform();
	CDmAttribute *GetVisibilityAttribute();

	CDmaVar< bool >					m_Visible;
	CDmaVar< bool >					m_bDisableOverrideParent;
	CDmaElement< CDmeTransform >	m_Transform;
	CDmaElement< CDmeShape >		m_Shape;
	CDmaElementArray< CDmeDag >		m_Children;

private:
	struct TransformInfo_t
	{
		CDmeTransform *m_pTransform;
		matrix3x4_t	m_DagToWorld;
		bool m_bComputedDagToWorld;
	};
	
	static CUtlStack<TransformInfo_t> s_TransformStack;
	static bool s_bDrawUsingEngineCoordinates;
	static bool s_bDrawZUp;									// NOTE: s_bZUp doesn't mean the model is in engine coordinates, it means it's Z Up, -Y Front & X Right.  Engine is Z Up, X Front & Y Right.
};


void RemoveDagFromParents( CDmeDag *pDag, bool bRecursivelyRemoveEmptyDagsFromParents = false );

template <class T>
inline T *CDmeDag::FindParentDagOfType()
{
	CDmeDag *parent = GetParent();
	while ( parent )
	{
		if ( CastElement< T >( parent ) )
			return static_cast< T * >( parent );

		parent = parent->GetParent();
	}

	if ( CastElement< T >( this ) )
		return static_cast< T * >( this );

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Find all dag nodes in the sub-tree of the dag which are of the
// specified type.
//-----------------------------------------------------------------------------
template< class T >
void CDmeDag::FindChildrenOfType( CUtlVector< T* > &children )
{
	int nChildren = m_Children.Count();
	for ( int iChild = 0; iChild < nChildren; ++iChild )
	{
		CDmeDag *pChild = m_Children[ iChild ];
		if ( pChild == NULL )
			continue;

		// Add the child to the list if it is of the specified type
		T* pChildType = CastElement< T >( pChild );
		if ( pChildType )
		{
			children.AddToTail( pChildType );
		}
		
		// Recursively add the children of the child.
		pChild->FindChildrenOfType( children );
	}
}


#endif // DMEDAG_H
