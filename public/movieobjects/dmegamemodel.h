//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Dme version of a game model (MDL)
//
//=============================================================================

#ifndef DMEGAMEMODEL_H
#define DMEGAMEMODEL_H
#ifdef _WIN32
#pragma once
#endif

#include "movieobjects/dmedag.h"
#include "movieobjects/dmeoperator.h"
#include "datacache/imdlcache.h"
#include "tier1/UtlDict.h"
#include "toolframework/itoolframework.h"

struct studiohdr_t;

// Fixme, this might not be the best spot for this
class IGlobalFlexController
{
public:
	virtual int				FindGlobalFlexController( char const *name ) = 0;
	virtual char const		*GetGlobalFlexControllerName( int idx ) = 0;
};

class CDmeGameModel;

// Mapping from name to dest index
class CDmeGlobalFlexControllerOperator : public CDmeOperator
{
	DEFINE_ELEMENT( CDmeGlobalFlexControllerOperator, CDmeOperator );

public:

	void		SetGameModel( CDmeGameModel *gameModel );

	virtual void Resolve();
	virtual void Operate();
	virtual void GetInputAttributes ( CUtlVector< CDmAttribute * > &attrs );
	virtual void GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs );

	void	SetWeight( float flWeight );
	void	SetMapping( int globalIndex );
	void	SetupToAttribute();

	int	GetGlobalIndex() const;

	virtual void OnAttributeChanged( CDmAttribute *pAttribute );
	CDmaVar< float >	m_flexWeight;
	CDmaElement< CDmeGameModel >	m_gameModel;
	
	DmAttributeHandle_t m_ToAttributeHandle;

private:
	int FindGlobalFlexControllerIndex() const;

	int m_nFlexControllerIndex;
};

//-----------------------------------------------------------------------------
// A class representing a game model
//-----------------------------------------------------------------------------
class CDmeGameModel : public CDmeDag
{
	DEFINE_ELEMENT( CDmeGameModel, CDmeDag );

public:
	virtual void OnAttributeChanged( CDmAttribute *pAttribute );

	void AddBone( CDmeTransform* pTransform );
	void AddBones( studiohdr_t *pStudioHdr, int nFirstBone, int nCount );
	void SetBone( uint index, const Vector& pos, const Quaternion& rot );
	uint NumBones() const;
	CDmeTransform *GetBone( uint index ) const;
	int FindBone( CDmeTransform *pTransform ) const;
	void RemoveAllBones();

	// A src bone transform transforms pre-compiled data (.dmx or .smd files, for example)
	// into post-compiled data (.mdl or .ani files)
	// Returns false if there is no transform (or if the transforms are identity)
	bool GetSrcBoneTransforms( matrix3x4_t *pPreTransform, matrix3x4_t *pPostTransform, int nBoneIndex ) const;

	bool GetBoneDefaultPosition( int nBoneIndex, Vector &position ) const;
	bool GetBoneDefaultOrientation( int nBoneIndex, Quaternion &orientation ) const;

	bool IsRootTransform( int nBoneIndex ) const;

	uint NumFlexWeights() const;
	const CUtlVector< float >& GetFlexWeights() const;
	// We drive these through the operators instead
	// void SetFlexWeights( uint nFlexWeights, const float* flexWeights );
	void SetNumFlexWeights( uint nFlexWeights );
	void SetFlexWeights( uint nFlexWeights, const float* flexWeights );

	void SetSkin( int nSkin );
	void SetBody( int nBody );
	void SetSequence( int nSequence );
	void SetFlags( int nFlags );

	int GetSkin() const;
	int GetBody() const;
	int GetSequence() const;
	int GetFlags() const;

	const char *GetModelName() const;
	MDLHandle_t GetModelHandle();

	CDmeGlobalFlexControllerOperator *AddGlobalFlexController( char const *controllerName, int globalIndex );
	CDmeGlobalFlexControllerOperator *FindGlobalFlexController( int nGlobalIndex );
	void	RemoveGlobalFlexController( CDmeGlobalFlexControllerOperator *controller );

	int NumGlobalFlexControllers() const;
	CDmeGlobalFlexControllerOperator *GetGlobalFlexController( int localIndex ); // localIndex is in order of calls to AddGlobalFlexController

	void AppendGlobalFlexControllerOperators( CUtlVector< IDmeOperator * >& list );
	studiohdr_t* GetStudioHdr() const;

	void FindFlexControllerDependencies( CUtlVector< CUtlVector< int > > &dependencyList ) const;

	int FindAttachment( const char *pchAttachmentName ) const;
	Vector ComputeAttachmentPosition( const char *pchAttachmentName ) const;
	CDmeDag *CreateDagForAttachment( const char *pchAttachmentName ) const;

	void SetComputeBounds( bool bEnable )				{ m_bComputeBounds = bEnable;			}
	void SetEvaluateProceduralBones( bool bEnable )		{ m_bEvaluateProceduralBones = bEnable;	}
	bool ShouldComputeBounds() const					{ return m_bComputeBounds;				}
	bool ShouldEvaluateProceduralBones() const			{ return m_bEvaluateProceduralBones;	}

public:

protected:
	void PopulateExistingDagList( CDmeDag** pDags, int nCount );
	void UpdateHMDL();

	// This holds the operators which map to the m_flexWeights below
	CDmaElementArray< CDmeGlobalFlexControllerOperator > m_globalFlexControllers;

	CDmaArray< float > m_flexWeights; // These are global flex weights (so there can be gaps, unused indices)
	CDmaString m_modelName;
	CDmaVar< int > m_skin;
	CDmaVar< int > m_body;
	CDmaVar< int > m_sequence;
	CDmaVar< int > m_flags;

	CDmaVar< bool > m_bComputeBounds;
	CDmaVar< bool > m_bEvaluateProceduralBones;


	// this is different than m_Children - this is ALL transforms in the tree that are used by the model
	// m_Children holds the roots of that tree
	CDmaElementArray< CDmeTransform > m_bones;

	MDLHandle_t m_hMDL;
	bool m_bHMDLDirty;
};

//-----------------------------------------------------------------------------
// A class representing a game sprite
//-----------------------------------------------------------------------------
class CDmeGameSprite : public CDmeDag
{
	DEFINE_ELEMENT( CDmeGameSprite, CDmeDag );

public:
	const char *GetModelName() const;

	float GetScale() const;
	float GetFrame() const;
	int GetRenderMode() const;
	int GetRenderFX() const;
	const Color &GetColor() const;
	float GetProxyRadius() const;

	void SetState( bool bVisible, float nFrame, int nRenderMode, int nRenderFX, float flRenderScale, float flProxyRadius, const Vector &pos, const Quaternion &rot, const Color &color );

protected:
	CDmaString				m_modelName;
	CDmaVar< float >		m_frame;
	CDmaVar< int >			m_rendermode;
	CDmaVar< int >			m_renderfx;
	CDmaVar< float >		m_renderscale;
	CDmaVar< Color >		m_color;
	CDmaVar< float >		m_proxyRadius;
};


//-----------------------------------------------------------------------------
// A class representing a game portal
//-----------------------------------------------------------------------------
class CDmeGamePortal : public CDmeDag
{
	DEFINE_ELEMENT( CDmeGamePortal, CDmeDag );

public:
	CDmaVar< int > m_nPortalId;
	CDmaVar< int > m_nLinkedPortalId;
	CDmaVar< float > m_flStaticAmount;
	CDmaVar< float > m_flSecondaryStaticAmount;
	CDmaVar< float > m_flOpenAmount;
	CDmaVar< float > m_flHalfWidth;
	CDmaVar< float > m_flHalfHeight;
	CDmaVar< bool > m_bIsPortal2;
	CDmaString m_PortalType;
};

#endif // DMEGAMEMODEL_H
