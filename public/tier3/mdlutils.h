//===== Copyright © 2005-2013, Valve Corporation, All rights reserved. ======//
//
// Purpose: A higher level link library for general use in the game and tools.
//
//===========================================================================//


#ifndef MDLUTILS_H
#define MDLUTILS_H

#if defined( _WIN32 )
#pragma once
#endif

#include "datacache/imdlcache.h"
#include "mathlib/vector.h"
#include "color.h"
#include "studio.h"
#include "materialsystem/custommaterialowner.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "irendertorthelperobject.h"

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct matrix3x4_t;

class CMDLAttachmentData
{
public:
	matrix3x4_t	m_AttachmentToWorld;
	bool m_bValid;
};

struct MDLSquenceLayer_t
{
	int		m_nSequenceIndex;
	float	m_flWeight;
};

//-----------------------------------------------------------------------------
// Class containing simplistic MDL state for use in rendering
//-----------------------------------------------------------------------------
class CMDL : public CCustomMaterialOwner, public IRenderToRTHelperObject
{
public:
	CMDL();
	virtual ~CMDL();

	void SetMDL( MDLHandle_t h );
	MDLHandle_t GetMDL() const;


	/// NOTE: This version of draw assumes you've filled in the bone to world
	/// matrix yourself by calling IStudioRender::LockBoneMatrices. The pointer
	/// returned by that method needs to be passed into here
	/// @param flags allows you to specify additional STUDIORENDER_ flags -- usually never necessary
	///        unless you need to (eg) forcibly disable shadows for some reason.
	void Draw( const matrix3x4_t& rootToWorld, const matrix3x4_t *pBoneToWorld, int flags = 0 );

	// from IRenderToRTHelperObject
	// Simple version of drawing; sets up bones for you
	void Draw( const matrix3x4_t& rootToWorld );
	bool GetBoundingSphere( Vector &vecCenter, float &flRadius );
	ITexture *GetEnvCubeMap();

	void SetUpBones( const matrix3x4_t& shapeToWorld, int nMaxBoneCount, matrix3x4_t *pOutputMatrices, const float *pPoseParameters = NULL, MDLSquenceLayer_t *pSequenceLayers = NULL, int nNumSequenceLayers = 0 );
	void SetupBonesWithBoneMerge( const CStudioHdr *pMergeHdr, matrix3x4_t *pMergeBoneToWorld, 
		const CStudioHdr *pFollow, const matrix3x4_t *pFollowBoneToWorld, const matrix3x4_t &matModelToWorld );
	
	studiohdr_t *GetStudioHdr();

	virtual bool GetAttachment( int number, matrix3x4_t &matrix );
	bool GetAttachment( const char *szAttachment, matrix3x4_t& matrixOut );

	void AdjustTime( float flAmount );

	void SetSimpleMaterialOverride( IMaterial *pNewMaterial );

private:
	void UnreferenceMDL();

	void SetupBones_AttachmentHelper( CStudioHdr *hdr, matrix3x4_t *pBoneToWorld );
	bool PutAttachment( int number, const matrix3x4_t &attachmentToWorld );
	CUtlVector<CMDLAttachmentData>		m_Attachments;

	CMaterialReference	m_pSimpleMaterialOverride;

protected:
	CTextureReference m_DefaultEnvCubemap;
	CTextureReference m_DefaultHDREnvCubemap;

public:
	MDLHandle_t	m_MDLHandle;
	
	// Adding m_Padding to coax the PS3 compiler into generating working code.
	// Without it the m_Color field won't be 4-byte aligned and the PS3 compiler will try and vectorize
	// some of the color arithmetic and trash nearby memory.
	unsigned short m_Padding; 
	Color		m_Color;
	int			m_nSkin;
	int			m_nBody;
	int			m_nSequence;
	int			m_nLOD;
	float		m_flPlaybackRate;
	float		m_flTime;
	float		m_flCurrentAnimEndTime;
	float		m_pFlexControls[ MAXSTUDIOFLEXCTRL * 4 ];
	Vector		m_vecViewTarget;
	bool		m_bWorldSpaceViewTarget;
	bool		m_bUseSequencePlaybackFPS;
	void		*m_pProxyData;
	float		m_flTimeBasisAdjustment;
	CUtlVector< int > m_arrSequenceFollowLoop;
};

struct MDLData_t
{
	MDLData_t();

	CMDL		m_MDL;
	matrix3x4_t	m_MDLToWorld;
	bool		m_bRequestBoneMergeTakeover;
};

class CMergedMDL : public IRenderToRTHelperObject
{
public:
	// constructor, destructor
	CMergedMDL();
	virtual ~CMergedMDL();

	// Sets the current mdl
	virtual void SetMDL( MDLHandle_t handle, CCustomMaterialOwner* pCustomMaterialOwner = NULL, void *pProxyData = NULL );
	virtual void SetMDL( const char *pMDLName, CCustomMaterialOwner* pCustomMaterialOwner = NULL, void *pProxyData = NULL );
	CMDL *GetMDL() { return &m_RootMDL.m_MDL; }

	// Sets the current sequence
	void SetSequence( int nSequence, bool bUseSequencePlaybackFPS );
	void AddSequenceFollowLoop( int nSequence, bool bUseSequencePlaybackFPS );
	void ClearSequenceFollowLoop();

	// Set the pose parameters
	void SetPoseParameters( const float *pPoseParameters, int nCount );

	// Set the overlay sequence layers
	void SetSequenceLayers( const MDLSquenceLayer_t *pSequenceLayers, int nCount );

	void SetSkin( int nSkin );

	// Bounds.
	bool GetBoundingBox( Vector &vecBoundsMin, Vector &vecBoundsMax );
	bool GetAttachment( const char *szAttachment, matrix3x4_t& matrixOut );
	bool GetAttachment( int iAttachmentNum, matrix3x4_t& matrixOut );

	void SetModelAnglesAndPosition( const QAngle &angRot, const Vector &vecPos );

	// Attached models.
	void	SetMergeMDL( MDLHandle_t handle, CCustomMaterialOwner* pCustomMaterialOwner = NULL, void *pProxyData = NULL, bool bRequestBonemergeTakeover = false );
	MDLHandle_t SetMergeMDL( const char *pMDLName, CCustomMaterialOwner* pCustomMaterialOwner = NULL, void *pProxyData = NULL, bool bRequestBonemergeTakeover = false );
	int		GetMergeMDLIndex( MDLHandle_t handle );
	CMDL	*GetMergeMDL(MDLHandle_t handle );
	void	ClearMergeMDLs( void );

	void Draw();

	void SetupBonesForAttachmentQueries( void );

	// from IRenderToRTHelperObject
	void Draw( const matrix3x4_t &rootToWorld );
	bool GetBoundingSphere( Vector &vecCenter, float &flRadius );
	ITexture *GetEnvCubeMap() { return m_RootMDL.m_MDL.GetEnvCubeMap(); }

	void UpdateModelCustomMaterials( MDLHandle_t handle, CCustomMaterialOwner* pCustomMaterialOwner );

protected:
	virtual void OnPostSetUpBonesPreDraw() {}
	virtual void OnModelDrawPassStart( int iPass, CStudioHdr *pStudioHdr, int &nFlags ) {}
	virtual void OnModelDrawPassFinished( int iPass, CStudioHdr *pStudioHdr, int &nFlags ) {}

protected:

	MDLData_t				m_RootMDL;
	CUtlVector<MDLData_t>	m_aMergeMDLs;

	float	m_PoseParameters[ MAXSTUDIOPOSEPARAM ];

private:

	static const int MAX_SEQUENCE_LAYERS = 8;
	int					m_nNumSequenceLayers;
	MDLSquenceLayer_t	m_SequenceLayers[ MAX_SEQUENCE_LAYERS ];
};

//-----------------------------------------------------------------------------
// Returns the bounding box for the model
//-----------------------------------------------------------------------------
void GetMDLBoundingBox( Vector *pMins, Vector *pMaxs, MDLHandle_t h, int nSequence );

//-----------------------------------------------------------------------------
// Returns the radius of the model as measured from the origin
//-----------------------------------------------------------------------------
float GetMDLRadius( MDLHandle_t h, int nSequence );

//-----------------------------------------------------------------------------
// Returns a more accurate bounding sphere
//-----------------------------------------------------------------------------
void GetMDLBoundingSphere( Vector *pVecCenter, float *pRadius, MDLHandle_t h, int nSequence );

//-----------------------------------------------------------------------------
// Determines which pose parameters are used by the specified sequence
//-----------------------------------------------------------------------------
void FindSequencePoseParameters( CStudioHdr &hdr, int nSequence, bool *pPoseParameters, int nCount );


#endif // MDLUTILS_H

