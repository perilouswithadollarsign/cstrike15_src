//===== Copyright � 2005-2013, Valve Corporation, All rights reserved. ======//
//
// Purpose: Utility methods for mdl files
//
//===========================================================================//

#include "tier3/mdlutils.h"
#include "tier0/dbg.h"
#include "tier3/tier3.h"
#include "studio.h"
#include "istudiorender.h"
#include "bone_setup.h"
#include "bone_accessor.h"
#include "materialsystem/imaterialvar.h"
#include "vcollide_parse.h"
#include "renderparm.h"
#include "tier2/renderutils.h"
#include "mathlib/camera.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Returns the bounding box for the model
//-----------------------------------------------------------------------------
void GetMDLBoundingBox( Vector *pMins, Vector *pMaxs, MDLHandle_t h, int nSequence )
{
	if ( h == MDLHANDLE_INVALID || !g_pMDLCache )
	{
		pMins->Init();
		pMaxs->Init();
		return;
	}

	pMins->Init( FLT_MAX, FLT_MAX );
	pMaxs->Init( -FLT_MAX, -FLT_MAX );

	studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( h );
	if ( !VectorCompare( vec3_origin, pStudioHdr->view_bbmin ) || !VectorCompare( vec3_origin, pStudioHdr->view_bbmax ))
	{
		// look for view clip
		*pMins = pStudioHdr->view_bbmin;
		*pMaxs = pStudioHdr->view_bbmax;
	}
	else if ( !VectorCompare( vec3_origin, pStudioHdr->hull_min ) || !VectorCompare( vec3_origin, pStudioHdr->hull_max ))
	{
		// look for hull
		*pMins = pStudioHdr->hull_min;
		*pMaxs = pStudioHdr->hull_max;
	}

	// Else use the sequence box
	mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( nSequence );
	VectorMin( seqdesc.bbmin, *pMins, *pMins );
	VectorMax( seqdesc.bbmax, *pMaxs, *pMaxs );
}


//-----------------------------------------------------------------------------
// Returns the radius of the model as measured from the origin
//-----------------------------------------------------------------------------
float GetMDLRadius( MDLHandle_t h, int nSequence )
{
	Vector vecMins, vecMaxs;
	GetMDLBoundingBox( &vecMins, &vecMaxs, h, nSequence );
	float flRadius = vecMaxs.Length();
	float flRadius2 = vecMins.Length();
	if ( flRadius2 > flRadius )
	{
		flRadius = flRadius2;
	}
	return flRadius;
}


//-----------------------------------------------------------------------------
// Returns a more accurate bounding sphere
//-----------------------------------------------------------------------------
void GetMDLBoundingSphere( Vector *pVecCenter, float *pRadius, MDLHandle_t h, int nSequence )
{
	Vector vecMins, vecMaxs;
	GetMDLBoundingBox( &vecMins, &vecMaxs, h, nSequence );
	VectorAdd( vecMins, vecMaxs, *pVecCenter );
	*pVecCenter *= 0.5f;
	*pRadius = vecMaxs.DistTo( *pVecCenter );
}


//-----------------------------------------------------------------------------
// Determines which pose parameters are used by the specified sequence
//-----------------------------------------------------------------------------
void FindSequencePoseParameters( CStudioHdr &hdr, int nSequence, bool *pPoseParameters, int nCount )
{
	if ( ( nSequence < 0 ) && ( nSequence >= hdr.GetNumSeq() ) )
		return;

	const mstudioseqdesc_t &seqdesc = hdr.pSeqdesc( nSequence );

	// Add the pose parameters that are directly referenced by this sequence
	int nParamIndex;
	nParamIndex = hdr.GetSharedPoseParameter( nSequence, seqdesc.paramindex[ 0 ] );
	if ( ( nParamIndex >= 0 ) && ( nParamIndex < nCount ) )
	{	
		pPoseParameters[ nParamIndex ] = true;
	}

	nParamIndex = hdr.GetSharedPoseParameter( nSequence, seqdesc.paramindex[ 1 ] );
	if ( ( nParamIndex >= 0 ) && ( nParamIndex < nCount ) )
	{
		pPoseParameters[ nParamIndex ] = true;
	}

	if ( seqdesc.flags & STUDIO_CYCLEPOSE )
	{
		nParamIndex = hdr.GetSharedPoseParameter( nSequence, seqdesc.cycleposeindex );
		if ( ( nParamIndex >= 0 ) && ( nParamIndex < nCount ) )
		{
			pPoseParameters[ nParamIndex ] = true;
		}
	}

	// Now recursively add the parameters for the auto layers
	for ( int i = 0; i < seqdesc.numautolayers; ++i )
	{
		const mstudioautolayer_t *pLayer = seqdesc.pAutolayer( i );
		int nLayerSequence = hdr.iRelativeSeq( nSequence, pLayer->iSequence );
		if ( nLayerSequence != nSequence )
		{
			FindSequencePoseParameters( hdr, nLayerSequence, pPoseParameters, nCount );
		}
	}
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CMDL::CMDL()
{
	m_MDLHandle = MDLHANDLE_INVALID;
	m_Color.SetColor( 255, 255, 255, 255 );
	m_nSkin = 0;
	m_nBody = 0;
	m_nSequence = 0;
	m_nLOD = 0;
	m_flPlaybackRate = 30.0f;
	m_flTime = 0.0f;
	m_vecViewTarget.Init( 0, 0, 0 );
	m_bWorldSpaceViewTarget = false;
	memset( m_pFlexControls, 0, sizeof(m_pFlexControls) );
	m_pProxyData = NULL;
	m_bUseSequencePlaybackFPS = false;
	m_flTimeBasisAdjustment = 0.0f;

	// Deal with the default cubemap
	ITexture *pCubemapTexture = g_pMaterialSystem->FindTexture( "editor/cubemap", NULL, true );
	m_DefaultEnvCubemap.Init( pCubemapTexture );
	pCubemapTexture = g_pMaterialSystem->FindTexture( "editor/cubemap.hdr", NULL, true );
	m_DefaultHDREnvCubemap.Init( pCubemapTexture );

	m_pSimpleMaterialOverride = NULL;
}

CMDL::~CMDL()
{
	m_DefaultEnvCubemap.Shutdown( );
	m_DefaultHDREnvCubemap.Shutdown();

	if ( m_pSimpleMaterialOverride != NULL )
	{
		m_pSimpleMaterialOverride.Shutdown();
		m_pSimpleMaterialOverride = NULL;
	}

	UnreferenceMDL();
}

ITexture *CMDL::GetEnvCubeMap()
{
	if ( g_pMaterialSystemHardwareConfig->GetHDRType() == HDR_TYPE_NONE )
	{
		return m_DefaultEnvCubemap;
	}
	else
	{
		return m_DefaultHDREnvCubemap;
	}
}

void CMDL::SetMDL( MDLHandle_t h )
{
	UnreferenceMDL();
	m_MDLHandle = h;
	if ( m_MDLHandle != MDLHANDLE_INVALID )
	{
		g_pMDLCache->AddRef( m_MDLHandle );

		studiohdr_t *pHdr = g_pMDLCache->GetStudioHdr( m_MDLHandle );

		if ( pHdr )
		{
			for ( LocalFlexController_t i = LocalFlexController_t(0); i < pHdr->numflexcontrollers; ++i )
			{
				if ( pHdr->pFlexcontroller( i )->localToGlobal == -1 )
				{
					pHdr->pFlexcontroller( i )->localToGlobal = i;
				}
			}

			if ( m_Attachments.Count() != pHdr->GetNumAttachments() )
			{
				m_Attachments.SetSize( pHdr->GetNumAttachments() );

				// This is to make sure we don't use the attachment before its been set up
				for ( int i=0; i < m_Attachments.Count(); i++ )
				{
					m_Attachments[i].m_bValid = false;
#ifdef _DEBUG
					m_Attachments[i].m_AttachmentToWorld.Invalidate();
#endif
				}

			}
		}
	}
}

MDLHandle_t CMDL::GetMDL() const
{
	return m_MDLHandle;
}


//-----------------------------------------------------------------------------
// Release the MDL handle
//-----------------------------------------------------------------------------
void CMDL::UnreferenceMDL()
{
	if ( !g_pMDLCache )
		return;

	if ( m_MDLHandle != MDLHANDLE_INVALID )
	{
		g_pMDLCache->Release( m_MDLHandle );
		m_MDLHandle = MDLHANDLE_INVALID;
	}
}


//-----------------------------------------------------------------------------
// Gets the studiohdr
//-----------------------------------------------------------------------------
studiohdr_t *CMDL::GetStudioHdr()
{
	if ( !g_pMDLCache )
		return NULL;
	return g_pMDLCache->GetStudioHdr( m_MDLHandle );
}


void CMDL::SetSimpleMaterialOverride( IMaterial *pNewMaterial )
{
	m_pSimpleMaterialOverride.Init( pNewMaterial );
}


//-----------------------------------------------------------------------------
// Draws the mesh
//-----------------------------------------------------------------------------
void CMDL::Draw( const matrix3x4_t& rootToWorld, const matrix3x4_t *pBoneToWorld, int flags )
{
	if ( !g_pMaterialSystem || !g_pMDLCache || !g_pStudioRender )
		return;

	if ( m_MDLHandle == MDLHANDLE_INVALID )
		return;

	// Color + alpha modulation
	Vector white( m_Color.r() / 255.0f, m_Color.g() / 255.0f, m_Color.b() / 255.0f );
	g_pStudioRender->SetColorModulation( white.Base() );
	g_pStudioRender->SetAlphaModulation( m_Color.a() / 255.0f );

	DrawModelInfo_t info;
	info.m_pStudioHdr = g_pMDLCache->GetStudioHdr( m_MDLHandle );
	info.m_pHardwareData = g_pMDLCache->GetHardwareData( m_MDLHandle );
	info.m_Decals = STUDIORENDER_DECAL_INVALID;
	info.m_Skin = m_nSkin;
	info.m_Body = m_nBody;
	info.m_HitboxSet = 0;
	info.m_pClientEntity = m_pProxyData;
	info.m_pColorMeshes = NULL;
	info.m_bStaticLighting = false;
	info.m_Lod = m_nLOD;

	Vector vecWorldViewTarget;
	if ( m_bWorldSpaceViewTarget )
	{
		vecWorldViewTarget = m_vecViewTarget;
	}
	else
	{
		VectorTransform( m_vecViewTarget, rootToWorld, vecWorldViewTarget );
	}
	g_pStudioRender->SetEyeViewTarget( info.m_pStudioHdr, info.m_Body, vecWorldViewTarget );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	CMatRenderData< float > rdFlexWeights( pRenderContext );

	// Set default flex values
	float *pFlexWeights = NULL;
	const int nFlexDescCount = info.m_pStudioHdr->numflexdesc;
	if ( nFlexDescCount )
	{
		CStudioHdr cStudioHdr( info.m_pStudioHdr, g_pMDLCache );
		pFlexWeights = rdFlexWeights.Lock( info.m_pStudioHdr->numflexdesc );
		cStudioHdr.RunFlexRules( m_pFlexControls, pFlexWeights );
	}

	Vector vecModelOrigin;
	MatrixGetColumn( rootToWorld, 3, vecModelOrigin );


	bool bOverride = false;
	static ConVarRef cl_custom_material_override( "cl_custom_material_override" );
	if ( cl_custom_material_override.IsValid() && cl_custom_material_override.GetBool() && !g_pStudioRender->IsForcedMaterialOverride() )
	{
		for ( int i = 0; i < GetCustomMaterialCount(); i++ )
		{
			if ( IsCustomMaterialValid( i ) )
			{
				g_pStudioRender->ForcedMaterialOverride( GetCustomMaterial( i )->GetMaterial(), OVERRIDE_SELECTIVE, i );
				bOverride = true;
			}
		}
	}

	if ( m_pSimpleMaterialOverride != NULL )
	{
		bOverride = true;
		g_pStudioRender->ForcedMaterialOverride( m_pSimpleMaterialOverride );
	}

	g_pStudioRender->DrawModel( NULL, info, const_cast<matrix3x4_t*>( pBoneToWorld ), 
		pFlexWeights, NULL, vecModelOrigin, STUDIORENDER_DRAW_ENTIRE_MODEL | flags );

	if ( bOverride )
	{
		g_pStudioRender->ForcedMaterialOverride( NULL );
	}
}

void CMDL::Draw( const matrix3x4_t &rootToWorld )
{
	if ( !g_pMaterialSystem || !g_pMDLCache || !g_pStudioRender )
		return;

	if ( m_MDLHandle == MDLHANDLE_INVALID )
		return;

	studiohdr_t *pStudioHdr = g_pMDLCache->GetStudioHdr( m_MDLHandle );

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	CMatRenderData< matrix3x4_t > rdBoneToWorld( pRenderContext, pStudioHdr->numbones );
	SetUpBones( rootToWorld, pStudioHdr->numbones, rdBoneToWorld.Base() );
	Draw( rootToWorld, rdBoneToWorld.Base() );
}


void CMDL::SetUpBones( const matrix3x4_t& rootToWorld, int nMaxBoneCount, matrix3x4_t *pBoneToWorld, const float *pPoseParameters, MDLSquenceLayer_t *pSequenceLayers, int nNumSequenceLayers )
{
	MDLCACHE_CRITICAL_SECTION();

	CStudioHdr studioHdr( g_pMDLCache->GetStudioHdr( m_MDLHandle ), g_pMDLCache );

	// Default to middle of the pose parameter range
	float defaultPoseParameters[MAXSTUDIOPOSEPARAM];
	if ( pPoseParameters == NULL )
	{
		Studio_CalcDefaultPoseParameters( &studioHdr, defaultPoseParameters, MAXSTUDIOPOSEPARAM );
		pPoseParameters = defaultPoseParameters;
	}

	int nFrameCount = Studio_MaxFrame( &studioHdr, m_nSequence, pPoseParameters );
	if ( nFrameCount == 0 )
	{
		nFrameCount = 1;
	}
	float flPlaybackRate = m_bUseSequencePlaybackFPS ? Studio_FPS( &studioHdr, m_nSequence, pPoseParameters ) : m_flPlaybackRate;
	float flAdjustedTime = m_flTime - m_flTimeBasisAdjustment;
	float flCycle = ( flAdjustedTime * flPlaybackRate ) / nFrameCount;
	m_flCurrentAnimEndTime = flPlaybackRate > 0.0f ? float( nFrameCount ) / flPlaybackRate : float( nFrameCount );

	if ( flCycle > 1.0f )
	{
		// We need to rollover into the next sequence followup
		if ( flPlaybackRate > 0.0f && flAdjustedTime < float(flPlaybackRate) )
			m_flTimeBasisAdjustment += float( nFrameCount ) / float( flPlaybackRate );
		else
			m_flTimeBasisAdjustment = m_flTime;

		if ( m_arrSequenceFollowLoop.Count() )
		{
			m_nSequence = m_arrSequenceFollowLoop.Head();
			m_arrSequenceFollowLoop.RemoveMultipleFromHead( 1 );
			// Recurse with the updated sequence
			SetUpBones( rootToWorld, nMaxBoneCount, pBoneToWorld, pPoseParameters, pSequenceLayers, nNumSequenceLayers );
			return;
		}
		else
		{
			flAdjustedTime = m_flTime - m_flTimeBasisAdjustment;
			flCycle = ( flAdjustedTime * flPlaybackRate ) / nFrameCount;
		}
	}

	// FIXME: We're always wrapping; may want to determing if we should clamp
	flCycle = SubtractIntegerPart(flCycle);

	BoneVector		pos[MAXSTUDIOBONES];
	BoneQuaternionAligned	q[MAXSTUDIOBONES];

	IBoneSetup boneSetup( &studioHdr, BONE_USED_BY_ANYTHING_AT_LOD( m_nLOD ), pPoseParameters, NULL );
	boneSetup.InitPose( pos, q );
	boneSetup.AccumulatePose( pos, q, m_nSequence, flCycle, 1.0f, flAdjustedTime, NULL );

	// Accumulate the additional layers if specified.
	if ( pSequenceLayers )
	{
		int nNumSeq = studioHdr.GetNumSeq();
		for ( int i = 0; i < nNumSequenceLayers; ++i )
		{
			int nSeqIndex = pSequenceLayers[ i ].m_nSequenceIndex;
			if ( ( nSeqIndex >= 0 ) && ( nSeqIndex < nNumSeq ) )
			{				
				float flWeight = pSequenceLayers[ i ].m_flWeight;

				int nFrameCount = MAX( 1, Studio_MaxFrame( &studioHdr, nSeqIndex, pPoseParameters ) );
				float flLayerCycle = ( flAdjustedTime * flPlaybackRate ) / nFrameCount;

				// FIXME: We're always wrapping; may want to determing if we should clamp
				flLayerCycle = SubtractIntegerPart(flLayerCycle);

				boneSetup.AccumulatePose( pos, q, nSeqIndex, flLayerCycle, flWeight, flAdjustedTime, NULL );
			}
		}
	}

	// FIXME: Try enabling this?
	//	CalcAutoplaySequences( pStudioHdr, NULL, pos, q, pPoseParameter, BONE_USED_BY_VERTEX_AT_LOD( m_nLOD ), flTime );

	matrix3x4_t temp;

	if ( nMaxBoneCount > studioHdr.numbones() )
	{
		nMaxBoneCount = studioHdr.numbones();
	}

	for ( int i = 0; i < nMaxBoneCount; i++ ) 
	{
		// If it's not being used, fill with NAN for errors
#ifdef _DEBUG
		if ( !(studioHdr.pBone( i )->flags & BONE_USED_BY_ANYTHING_AT_LOD( m_nLOD ) ) )
		{
			int j, k;
			for (j = 0; j < 3; j++)
			{
				for (k = 0; k < 4; k++)
				{
					pBoneToWorld[i][j][k] = VEC_T_NAN;
				}
			}
			continue;
		}
#endif

		matrix3x4_t boneMatrix;
		QuaternionMatrix( q[i], boneMatrix );
		MatrixSetColumn( pos[i], 3, boneMatrix );

		// WARNING: converting from matrix3x4_t to matrix3x4a_t is going to asplode on a console.
		// Calculate helper bones!
		AssertAligned( pBoneToWorld );
		CBoneAccessor tempCBoneAccessor( ( matrix3x4a_t * )pBoneToWorld );
		if ( CalcProceduralBone( &studioHdr, i,  tempCBoneAccessor ) )
		{
		}
		else if ( studioHdr.pBone(i)->parent == -1 ) 
		{
			ConcatTransforms( rootToWorld, boneMatrix, pBoneToWorld[i] );
		} 
		else 
		{
			ConcatTransforms( pBoneToWorld[ studioHdr.pBone(i)->parent ], boneMatrix, pBoneToWorld[i] );
		}
	}

	Studio_RunBoneFlexDrivers( m_pFlexControls, &studioHdr, pos, pBoneToWorld, rootToWorld );

	SetupBones_AttachmentHelper( &studioHdr, pBoneToWorld );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMDL::SetupBonesWithBoneMerge( const CStudioHdr *pMergeHdr, matrix3x4_t *pMergeBoneToWorld, 
								    const CStudioHdr *pFollow, const matrix3x4_t *pFollowBoneToWorld,
									const matrix3x4_t &matModelToWorld )
{

	// Default to middle of the pose parameter range
	float flPoseParameter[MAXSTUDIOPOSEPARAM];
	Studio_CalcDefaultPoseParameters( pMergeHdr, flPoseParameter, MAXSTUDIOPOSEPARAM );

	int nFrameCount = Studio_MaxFrame( pMergeHdr, m_nSequence, flPoseParameter );
	if ( nFrameCount == 0 )
	{
		nFrameCount = 1;
	}
	float flPlaybackRate = m_bUseSequencePlaybackFPS ? Studio_FPS( pMergeHdr, m_nSequence, flPoseParameter ) : m_flPlaybackRate;
	float flAdjustedTime = m_flTime - m_flTimeBasisAdjustment;
	float flCycle = ( flAdjustedTime * flPlaybackRate ) / nFrameCount;
	m_flCurrentAnimEndTime = flPlaybackRate > 0.0f ? float( nFrameCount ) / flPlaybackRate : float( nFrameCount );

	if ( flCycle > 1.0f )
	{
		// We need to rollover into the next sequence followup
		if ( flPlaybackRate > 0.0f && flAdjustedTime < float(flPlaybackRate) )
			m_flTimeBasisAdjustment += float( nFrameCount ) / float( flPlaybackRate );
		else
			m_flTimeBasisAdjustment = m_flTime;

		if ( m_arrSequenceFollowLoop.Count() )
		{
			m_nSequence = m_arrSequenceFollowLoop.Head();
			m_arrSequenceFollowLoop.RemoveMultipleFromHead( 1 );
			// Recurse with the updated sequence
			SetupBonesWithBoneMerge( pMergeHdr, pMergeBoneToWorld, pFollow, pFollowBoneToWorld, matModelToWorld );
			return;
		}
		else
		{
			flAdjustedTime = m_flTime - m_flTimeBasisAdjustment;
			flCycle = ( flAdjustedTime * flPlaybackRate ) / nFrameCount;
		}
	}

	// FIXME: We're always wrapping; may want to determing if we should clamp
	flCycle = SubtractIntegerPart(flCycle);

	BoneVector		pos[MAXSTUDIOBONES];
	BoneQuaternionAligned	q[MAXSTUDIOBONES];

	IBoneSetup boneSetup( pMergeHdr,  BONE_USED_BY_ANYTHING_AT_LOD( m_nLOD ), flPoseParameter );
	boneSetup.InitPose( pos, q );
	boneSetup.AccumulatePose( pos, q, m_nSequence, flCycle, 1.0f, flAdjustedTime, NULL );

	// Get the merge bone list.
	const mstudiobone_t *pMergeBones = pMergeHdr->pBone( 0 );
	for ( int iMergeBone = 0; iMergeBone < pMergeHdr->numbones(); ++iMergeBone )
	{
		// Now find the bone in the parent entity.
		bool bMerged = false;
		int iParentBoneIndex = Studio_BoneIndexByName( pFollow, pMergeBones[iMergeBone].pszName() );
		if ( iParentBoneIndex >= 0 )
		{
			MatrixCopy( pFollowBoneToWorld[iParentBoneIndex], pMergeBoneToWorld[iMergeBone] );
			bMerged = true;
		}

		if ( !bMerged )
		{
			// If we get down here, then the bone wasn't merged.
			matrix3x4_t matBone;
			QuaternionMatrix( q[iMergeBone], pos[iMergeBone], matBone );

			if ( pMergeBones[iMergeBone].parent == -1 ) 
			{
				ConcatTransforms( matModelToWorld, matBone, pMergeBoneToWorld[iMergeBone] );
			} 
			else 
			{
				ConcatTransforms( pMergeBoneToWorld[pMergeBones[iMergeBone].parent], matBone, pMergeBoneToWorld[iMergeBone] );
			}
		}
	}
}

void CMDL::SetupBones_AttachmentHelper( CStudioHdr *hdr, matrix3x4_t *pBoneToWorld )
{
	if ( !hdr || !hdr->GetNumAttachments() )
		return;

	// calculate attachment points
	matrix3x4_t world;
	for (int i = 0; i < hdr->GetNumAttachments(); i++)
	{
		const mstudioattachment_t &pattachment = hdr->pAttachment( i );
		int iBone = hdr->GetAttachmentBone( i );

		if ( (pattachment.flags & ATTACHMENT_FLAG_WORLD_ALIGN) == 0 )
		{
			ConcatTransforms( pBoneToWorld[iBone], pattachment.local, world ); 
		}
		else
		{
			Vector vecLocalBonePos, vecWorldBonePos;
			MatrixGetColumn( pattachment.local, 3, vecLocalBonePos );
			VectorTransform( vecLocalBonePos, pBoneToWorld[iBone], vecWorldBonePos );

			SetIdentityMatrix( world );
			MatrixSetColumn( vecWorldBonePos, 3, world );
		}

		PutAttachment( i + 1, world );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Put a value into an attachment point by index
// Input  : number - which point
// Output : float * - the attachment point
//-----------------------------------------------------------------------------
bool CMDL::PutAttachment( int number, const matrix3x4_t &attachmentToWorld )
{
	if ( number < 1 || number > m_Attachments.Count() )
		return false;

	CMDLAttachmentData *pAtt = &m_Attachments[number-1];
	pAtt->m_AttachmentToWorld = attachmentToWorld;
	pAtt->m_bValid = true;

	return true;
}

bool CMDL::GetAttachment( int number, matrix3x4_t& matrix )
{
	if ( number < 1 || number > m_Attachments.Count() )
		return false;

	if ( !m_Attachments[number-1].m_bValid )
		return false;

	matrix = m_Attachments[number-1].m_AttachmentToWorld;
	return true;
}

bool CMDL::GetAttachment( const char *pszAttachment, matrix3x4_t& matrixOut )
{
	if ( GetMDL() == MDLHANDLE_INVALID )
		return false;

	CStudioHdr studioHdr( g_pMDLCache->GetStudioHdr( GetMDL() ), g_pMDLCache );

	int iAttachmentNum = Studio_FindAttachment( &studioHdr, pszAttachment );
	if ( iAttachmentNum == -1 )
		return false;

	return GetAttachment( iAttachmentNum + 1, matrixOut );
}

bool CMDL::GetBoundingSphere( Vector &vecCenter, float &flRadius )
{
	// Check to see if we have a valid model to look at.
	if ( m_MDLHandle == MDLHANDLE_INVALID )
		return false;

	GetMDLBoundingSphere( &vecCenter, &flRadius, m_MDLHandle, m_nSequence );

	return true;
}

void CMDL::AdjustTime( float flAmount )
{
	m_flTime += flAmount;
}

MDLData_t::MDLData_t()
{
	SetIdentityMatrix( m_MDLToWorld );
	m_bRequestBoneMergeTakeover = false;
}

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CMergedMDL::CMergedMDL()
{
	m_nNumSequenceLayers = 0;
}

CMergedMDL::~CMergedMDL()
{
	m_aMergeMDLs.Purge();
}

//-----------------------------------------------------------------------------
// Stores the clip
//-----------------------------------------------------------------------------
void CMergedMDL::SetMDL( MDLHandle_t handle, CCustomMaterialOwner* pCustomMaterialOwner, void *pProxyData )
{
	m_RootMDL.m_MDL.SetMDL( handle );
	m_RootMDL.m_MDL.m_pProxyData = pProxyData;

	Vector vecMins, vecMaxs;
	GetMDLBoundingBox( &vecMins, &vecMaxs, handle, m_RootMDL.m_MDL.m_nSequence );

	m_RootMDL.m_MDL.m_bWorldSpaceViewTarget = false;
	m_RootMDL.m_MDL.m_vecViewTarget.Init( 100.0f, 0.0f, vecMaxs.z );

	if ( pCustomMaterialOwner )
	{
		pCustomMaterialOwner->DuplicateCustomMaterialsToOther( &m_RootMDL.m_MDL );
	}

	// Set the pose parameters to the default for the mdl
	SetPoseParameters( NULL, 0 );
	
	// Clear any sequence layers
	SetSequenceLayers( NULL, 0 );
}

//-----------------------------------------------------------------------------
// An MDL was selected
//-----------------------------------------------------------------------------
void CMergedMDL::SetMDL( const char *pMDLName, CCustomMaterialOwner* pCustomMaterialOwner, void *pProxyData )
{
	MDLHandle_t hMDL = pMDLName ? g_pMDLCache->FindMDL( pMDLName ) : MDLHANDLE_INVALID;
	if ( g_pMDLCache->IsErrorModel( hMDL ) )
	{
		hMDL = MDLHANDLE_INVALID;
	}

	SetMDL( hMDL, pCustomMaterialOwner, pProxyData );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a model bounding box.
//-----------------------------------------------------------------------------
bool CMergedMDL::GetBoundingBox( Vector &vecBoundsMin, Vector &vecBoundsMax )
{
	// Check to see if we have a valid model to look at.
	if ( m_RootMDL.m_MDL.GetMDL() == MDLHANDLE_INVALID )
		return false;

	GetMDLBoundingBox( &vecBoundsMin, &vecBoundsMax, m_RootMDL.m_MDL.GetMDL(), m_RootMDL.m_MDL.m_nSequence );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a more accurate bounding sphere
//-----------------------------------------------------------------------------
bool CMergedMDL::GetBoundingSphere( Vector &vecCenter, float &flRadius )
{
	// Check to see if we have a valid model to look at.
	if ( m_RootMDL.m_MDL.GetMDL() == MDLHANDLE_INVALID )
		return false;

	Vector vecEngineCenter;
	GetMDLBoundingSphere( &vecEngineCenter, &flRadius, m_RootMDL.m_MDL.GetMDL(), m_RootMDL.m_MDL.m_nSequence );
	VectorTransform( vecEngineCenter, m_RootMDL.m_MDLToWorld, vecCenter );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMergedMDL::GetAttachment( const char *pszAttachment, matrix3x4_t& matrixOut )
{
	if ( m_RootMDL.m_MDL.GetMDL() == MDLHANDLE_INVALID )
		return false;

	CStudioHdr studioHdr( g_pMDLCache->GetStudioHdr( m_RootMDL.m_MDL.GetMDL() ), g_pMDLCache );

	int iAttachmentNum = Studio_FindAttachment( &studioHdr, pszAttachment );
	if ( iAttachmentNum == -1 )
		return false;

	return GetAttachment( iAttachmentNum + 1, matrixOut );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMergedMDL::GetAttachment( int iAttachmentNum, matrix3x4_t& matrixOut )
{
	if ( m_RootMDL.m_MDL.GetMDL() == MDLHANDLE_INVALID )
		return false;

	return m_RootMDL.m_MDL.GetAttachment( iAttachmentNum, matrixOut );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMergedMDL::SetModelAnglesAndPosition(  const QAngle &angRot, const Vector &vecPos )
{
	SetIdentityMatrix( m_RootMDL.m_MDLToWorld );
	AngleMatrix( angRot, vecPos, m_RootMDL.m_MDLToWorld );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMergedMDL::SetupBonesForAttachmentQueries( void )
{
	if ( !g_pMDLCache )
		return;

	if ( m_RootMDL.m_MDL.GetMDL() == MDLHANDLE_INVALID )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	CStudioHdr *pRootStudioHdr = new CStudioHdr( g_pMDLCache->GetStudioHdr( m_RootMDL.m_MDL.GetMDL() ), g_pMDLCache );
	CMatRenderData< matrix3x4_t > rdBoneToWorld( pRenderContext, pRootStudioHdr->numbones() );
	m_RootMDL.m_MDL.SetUpBones( m_RootMDL.m_MDLToWorld, pRootStudioHdr->numbones(), rdBoneToWorld.Base(), m_PoseParameters, m_SequenceLayers, m_nNumSequenceLayers );

	delete pRootStudioHdr;
}

//-----------------------------------------------------------------------------
// paint it!
//-----------------------------------------------------------------------------
void CMergedMDL::Draw()
{
	if ( !g_pMDLCache )
		return;

	if ( m_RootMDL.m_MDL.GetMDL() == MDLHANDLE_INVALID )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	// Draw the MDL
	CStudioHdr *pRootStudioHdr = new CStudioHdr( g_pMDLCache->GetStudioHdr( m_RootMDL.m_MDL.GetMDL() ), g_pMDLCache );
	CMatRenderData< matrix3x4_t > rdBoneToWorld( pRenderContext, pRootStudioHdr->numbones() );
	const matrix3x4_t *pRootMergeHdrModelToWorld = &m_RootMDL.m_MDLToWorld;
	const matrix3x4_t *pFollowBoneToWorld = rdBoneToWorld.Base();
	m_RootMDL.m_MDL.SetUpBones( m_RootMDL.m_MDLToWorld, pRootStudioHdr->numbones(), rdBoneToWorld.Base(), m_PoseParameters, m_SequenceLayers, m_nNumSequenceLayers );

	OnPostSetUpBonesPreDraw();

	int nFlags = STUDIORENDER_DRAW_NO_SHADOWS;

	OnModelDrawPassStart( 0, pRootStudioHdr, nFlags );
	m_RootMDL.m_MDL.Draw( m_RootMDL.m_MDLToWorld, rdBoneToWorld.Base(), nFlags );
	OnModelDrawPassFinished( 0, pRootStudioHdr, nFlags );

	// Draw the merge MDLs.
	matrix3x4_t *pStackCopyOfRootMergeHdrModelToWorld = NULL;
	matrix3x4_t matMergeBoneToWorld[MAXSTUDIOBONES];
	int nMergeCount = m_aMergeMDLs.Count();
	for ( int iMerge = 0; iMerge < nMergeCount; ++iMerge )
	{
		matrix3x4_t *pMergeBoneToWorld = &matMergeBoneToWorld[0];

		// Get the merge studio header.
		CStudioHdr *pMergeHdr = new CStudioHdr( g_pMDLCache->GetStudioHdr( m_aMergeMDLs[iMerge].m_MDL.GetMDL() ), g_pMDLCache );
		m_aMergeMDLs[iMerge].m_MDL.SetupBonesWithBoneMerge( pMergeHdr, pMergeBoneToWorld, pRootStudioHdr, pFollowBoneToWorld, *pRootMergeHdrModelToWorld );

		OnModelDrawPassStart( 0, pMergeHdr, nFlags );
		m_aMergeMDLs[iMerge].m_MDL.Draw( m_aMergeMDLs[iMerge].m_MDLToWorld, pMergeBoneToWorld, nFlags );
		OnModelDrawPassFinished( 0, pMergeHdr, nFlags );

		if ( m_aMergeMDLs[iMerge].m_bRequestBoneMergeTakeover && ( iMerge + 1 < nMergeCount ) )
		{
			// This model is requesting bonemerge takeover and we have more models to render after it
			delete pRootStudioHdr;
			pRootStudioHdr = pMergeHdr;
			pRootMergeHdrModelToWorld = &m_aMergeMDLs[iMerge].m_MDLToWorld;

			// Make a copy of bone to world transforms in a separate stack buffer and repoint root transforms
			// for future bonemerge into that buffer
			if ( !pStackCopyOfRootMergeHdrModelToWorld )
				pStackCopyOfRootMergeHdrModelToWorld = ( matrix3x4_t * ) stackalloc( sizeof( matMergeBoneToWorld ) );
			Q_memcpy( pStackCopyOfRootMergeHdrModelToWorld, matMergeBoneToWorld, sizeof( matMergeBoneToWorld ) );
			pFollowBoneToWorld = pStackCopyOfRootMergeHdrModelToWorld;
		}
		else
		{
			delete pMergeHdr;
		}
	}
	rdBoneToWorld.Release();

	delete pRootStudioHdr;
}


void CMergedMDL::Draw( const matrix3x4_t &rootToWorld )
{
	m_RootMDL.m_MDLToWorld = rootToWorld;
	Draw();
}


//-----------------------------------------------------------------------------
// Sets the current sequence
//-----------------------------------------------------------------------------
void CMergedMDL::SetSequence( int nSequence, bool bUseSequencePlaybackFPS )
{
	m_RootMDL.m_MDL.m_nSequence = nSequence;
	m_RootMDL.m_MDL.m_bUseSequencePlaybackFPS = bUseSequencePlaybackFPS;
	m_RootMDL.m_MDL.m_flTimeBasisAdjustment = m_RootMDL.m_MDL.m_flTime;
}

//-----------------------------------------------------------------------------
// Add a follow loop sequence
//-----------------------------------------------------------------------------
void CMergedMDL::AddSequenceFollowLoop( int nSequence, bool bUseSequencePlaybackFPS )
{
	Assert( bUseSequencePlaybackFPS == m_RootMDL.m_MDL.m_bUseSequencePlaybackFPS );
	m_RootMDL.m_MDL.m_arrSequenceFollowLoop.AddToTail( nSequence );
}

//-----------------------------------------------------------------------------
// Clear any follow loop sequences
//-----------------------------------------------------------------------------
void CMergedMDL::ClearSequenceFollowLoop()
{
	m_RootMDL.m_MDL.m_arrSequenceFollowLoop.RemoveAll();
}


//-----------------------------------------------------------------------------
// Set the current pose parameters. If NULL the pose parameters will be reset
// to the default values.
//-----------------------------------------------------------------------------
void CMergedMDL::SetPoseParameters( const float *pPoseParameters, int nCount )
{
	if ( pPoseParameters )
	{
		int nParameters = MIN( MAXSTUDIOPOSEPARAM, nCount );
		for ( int iParam = 0; iParam < nParameters; ++iParam )
		{
			m_PoseParameters[ iParam ] = pPoseParameters[ iParam ];
		}
	}
	else if ( m_RootMDL.m_MDL.GetMDL() != MDLHANDLE_INVALID )
	{
		CStudioHdr studioHdr( g_pMDLCache->GetStudioHdr( m_RootMDL.m_MDL.GetMDL() ), g_pMDLCache );
		Studio_CalcDefaultPoseParameters( &studioHdr, m_PoseParameters, MAXSTUDIOPOSEPARAM );
	}
}


//-----------------------------------------------------------------------------
// Set the overlay sequence layers
//-----------------------------------------------------------------------------
void CMergedMDL::SetSequenceLayers( const MDLSquenceLayer_t *pSequenceLayers, int nCount )
{
	if ( pSequenceLayers )
	{
		m_nNumSequenceLayers = MIN( MAX_SEQUENCE_LAYERS, nCount );
		for ( int iLayer = 0; iLayer < m_nNumSequenceLayers; ++iLayer )
		{
			m_SequenceLayers[ iLayer ] = pSequenceLayers[ iLayer ];
		}
	}
	else
	{
		m_nNumSequenceLayers = 0;
		V_memset( m_SequenceLayers, 0, sizeof( m_SequenceLayers ) );
	}
}

void CMergedMDL::SetSkin( int nSkin )
{
	m_RootMDL.m_MDL.m_nSkin = nSkin;
}

	
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMergedMDL::SetMergeMDL( MDLHandle_t handle, CCustomMaterialOwner* pCustomMaterialOwner, void *pProxyData, bool bRequestBonemergeTakeover )
{
	// Verify that we have a root model to merge to.
	if ( m_RootMDL.m_MDL.GetMDL() == MDLHANDLE_INVALID )
		return;

	int iIndex = m_aMergeMDLs.AddToTail();
	if ( !m_aMergeMDLs.IsValidIndex( iIndex ) )
		return;

	m_aMergeMDLs[iIndex].m_MDL.SetMDL( handle );
	m_aMergeMDLs[iIndex].m_MDL.m_pProxyData = pProxyData;
	m_aMergeMDLs[iIndex].m_bRequestBoneMergeTakeover = bRequestBonemergeTakeover;

	if ( pCustomMaterialOwner )
	{
		pCustomMaterialOwner->DuplicateCustomMaterialsToOther( &m_aMergeMDLs[iIndex].m_MDL );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
MDLHandle_t CMergedMDL::SetMergeMDL( const char *pMDLName, CCustomMaterialOwner* pCustomMaterialOwner, void *pProxyData, bool bRequestBonemergeTakeover )
{
	if ( g_pMDLCache == NULL )
		return MDLHANDLE_INVALID;

	MDLHandle_t hMDL = pMDLName ? g_pMDLCache->FindMDL( pMDLName ) : MDLHANDLE_INVALID;
	if ( g_pMDLCache->IsErrorModel( hMDL ) )
	{
		hMDL = MDLHANDLE_INVALID;
	}

	SetMergeMDL( hMDL, pCustomMaterialOwner, pProxyData, bRequestBonemergeTakeover );
	return hMDL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMergedMDL::GetMergeMDLIndex( MDLHandle_t handle )
{
	int nMergeCount = m_aMergeMDLs.Count();
	for ( int iMerge = 0; iMerge < nMergeCount; ++iMerge )
	{
		if ( m_aMergeMDLs[iMerge].m_MDL.GetMDL() == handle )
			return iMerge;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMDL *CMergedMDL::GetMergeMDL( MDLHandle_t handle )
{
	int nMergeCount = m_aMergeMDLs.Count();
	for ( int iMerge = 0; iMerge < nMergeCount; ++iMerge )
	{
		if ( m_aMergeMDLs[iMerge].m_MDL.GetMDL() == handle )
			return (&m_aMergeMDLs[iMerge].m_MDL);
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMergedMDL::ClearMergeMDLs( void )
{
	m_aMergeMDLs.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMergedMDL::UpdateModelCustomMaterials( MDLHandle_t handle, CCustomMaterialOwner* pCustomMaterialOwner )
{
	CMDL* pMDL = (handle != MDLHANDLE_INVALID) ? GetMergeMDL( handle ) : NULL;

	if ( pMDL )
	{
		if ( pCustomMaterialOwner )
		{
			pCustomMaterialOwner->DuplicateCustomMaterialsToOther( pMDL );
		}
		else
		{
			pMDL->ClearCustomMaterials();
		}
	}
}