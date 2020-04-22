//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// scenesunlight.cpp
//
//===============================================================================
#include "cbase.h"
#include "csm_parallel_split.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

// We don't currently support smooth cascade transitioning on Gameconsoles. This convar must be adjusted if we ever do.
ConVar cl_csm_shadow_split_lerp_factor_range( "cl_csm_shadow_split_lerp_factor_range", IsGameConsole() ? "0" : ".2", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_shadow_split_radial_dist_lerp_factor_multiplier( "cl_csm_shadow_split_radial_dist_lerp_factor_multiplier", IsGameConsole() ? ".75" : ".85", FCVAR_DEVELOPMENTONLY );

namespace CCSMFrustumDefinition
{
	Vector4D g_vProjFrustumVerts[NUM_FRUSTUM_VERTS] = 
	{
		Vector4D( -1.0f, -1.0f, 0.0f, 1.0f ),
		Vector4D(  1.0f, -1.0f, 0.0f, 1.0f ),
		Vector4D(  1.0f,  1.0f, 0.0f, 1.0f ),
		Vector4D( -1.0f,  1.0f, 0.0f, 1.0f ),

		Vector4D( -1.0f, -1.0f, 1.0f, 1.0f ),
		Vector4D(  1.0f, -1.0f, 1.0f, 1.0f ),
		Vector4D(  1.0f,  1.0f, 1.0f, 1.0f ),
		Vector4D( -1.0f,  1.0f, 1.0f, 1.0f )
	};

	const uint8 g_nFrustumLineVertIndices[24] =
	{
		0, 1, 1, 2, 2, 3, 3, 0,
		4, 5, 5, 6, 6, 7, 7, 4,
		0, 4, 1, 5, 2, 6, 3, 7
	};

	const uint8 g_nQuadVertIndices[NUM_FRUSTUM_QUADS][4] =
	{
		// 0 left
		{ 4, 0, 3, 7 }, 
		// 1 right
		{ 1, 5, 6, 2 }, 
		// 2 bottom
		{ 0, 4, 5, 1 }, 
		// 3 top
		{ 2, 6, 7, 3 }, 
		// 4 near
		{ 0, 1, 2, 3 }, 
		// 5 far
		{ 7, 6, 5, 4 }  
	};

	// The vertices of each frustum edge
	const uint8 g_nEdgeVertIndices[NUM_FRUSTUM_EDGES][2] =
	{
		{ 4, 0 }, { 0, 3 }, { 3, 7 }, { 7, 4 },
		{ 1, 5 }, { 5, 6 }, { 6, 2 }, { 2, 1 },
		{ 4, 5 }, { 1, 0 }, { 6, 7 }, { 3, 2 }
	};

	// The quads sharing each frustum edge
	const uint8 g_nEdgeQuadIndices[NUM_FRUSTUM_EDGES][2] =
	{
		{ 0, 2 }, { 0, 4 },
		{ 0, 3 }, { 0, 5 },
		{ 1, 2 }, { 1, 5 },
		{ 1, 3 }, { 1, 4 },
		{ 2, 5 }, { 2, 4 },
		{ 3, 5 }, { 3, 4 }
	};
} // namespace CCSMFrustumDefinition

static void InvertVMatrix( const VMatrix &src, VMatrix &dst )
{
#ifdef PLATFORM_X360
	XMVECTOR vDet;
	XMMATRIX vInverse = XMMatrixInverse( &vDet, XMLoadFloat4x4( reinterpret_cast< const XMFLOAT4X4 * >( &src ) ) );
	XMStoreFloat4x4( reinterpret_cast< XMFLOAT4X4 * >( &dst ), vInverse );
#else
	src.InverseGeneral( dst );
#endif
}

CCSMParallelSplit::CCSMParallelSplit() :
	m_bValid( false ),
	m_nShadowBufferSize( 0 ),
	m_nShadowCascadeMaxSize( 0 ),
	m_nShadowAtlasWidth( 0 ),
	m_nShadowAtlasHeight( 0 )
{
	V_memset( &m_viewState, 0, sizeof( m_viewState ) );
	V_memset( &m_lightState, 0, sizeof( m_lightState ) );
}

CCSMParallelSplit::~CCSMParallelSplit()
{
}

void CCSMParallelSplit::Clear()
{
	m_bValid = false;
	m_nShadowBufferSize = 0;
	m_nShadowCascadeMaxSize = 0;
	m_nShadowAtlasWidth = 0;
	m_nShadowAtlasHeight = 0;
	V_memset( &m_viewState, 0, sizeof( m_viewState ) );
	V_memset( &m_lightState, 0, sizeof( m_lightState ) );
}

CCSMParallelSplit::CCSMParallelSplit( const CCSMParallelSplit &other )
{
	m_bValid = other.m_bValid;
	m_nShadowBufferSize = other.m_nShadowBufferSize;	
	m_nShadowCascadeMaxSize = other.m_nShadowCascadeMaxSize;
	m_nShadowAtlasWidth = other.m_nShadowAtlasWidth;
	m_nShadowAtlasHeight = other.m_nShadowAtlasHeight;
	V_memcpy( &m_viewState, &other.m_viewState, sizeof( m_viewState ) );
	V_memcpy( &m_lightState, &other.m_lightState, sizeof( m_lightState ) );
}

const CCSMParallelSplit &CCSMParallelSplit::operator==( const CCSMParallelSplit &rhs )
{
	if ( this == &rhs )
		return *this;
	
	m_bValid = rhs.m_bValid;
	m_nShadowBufferSize = rhs.m_nShadowBufferSize;	
	m_nShadowCascadeMaxSize = rhs.m_nShadowCascadeMaxSize;
	m_nShadowAtlasWidth = rhs.m_nShadowAtlasWidth;
	m_nShadowAtlasHeight = rhs.m_nShadowAtlasHeight;
	V_memcpy( &m_viewState, &rhs.m_viewState, sizeof( m_viewState ) );
	V_memcpy( &m_lightState, &rhs.m_lightState, sizeof( m_lightState ) );	
	
	return *this;
}

void CCSMParallelSplit::Init( uint nMaxShadowBufferSize, uint nMaxCascadeSize )
{
	m_nShadowBufferSize = nMaxShadowBufferSize;
	m_nShadowCascadeMaxSize = nMaxCascadeSize;
	m_nShadowAtlasWidth = m_nShadowBufferSize * 2;
	m_nShadowAtlasHeight = m_nShadowBufferSize * 2;
}

ConVar cl_csm_parallel_split_log_lin_lerp( "cl_csm_parallel_split_log_lin_lerp", ".94", FCVAR_DEVELOPMENTONLY, "" );

ConVar cl_csm_parallel_split_dist1( "cl_csm_parallel_split_dist1", "-1", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_parallel_split_dist2( "cl_csm_parallel_split_dist2", "-1", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_parallel_split_dist3( "cl_csm_parallel_split_dist3", "-1", FCVAR_DEVELOPMENTONLY, "" );

float CCSMParallelSplit::CalculateSplitPlaneDistance( int nSplit, int nShadowSplits, float flMaxShadowDistance, float flZNear, float flZFar )
{
	if ( ( nSplit == 1 ) && ( cl_csm_parallel_split_dist1.GetFloat() >= 0.0f ) )
		return Lerp( cl_csm_parallel_split_dist1.GetFloat(), flZNear, flZFar );
	else if ( ( nSplit == 2 ) && ( cl_csm_parallel_split_dist2.GetFloat() >= 0.0f ) )
		return Lerp( cl_csm_parallel_split_dist2.GetFloat(), flZNear, flZFar );
	else if ( ( nSplit == 3 ) && ( cl_csm_parallel_split_dist3.GetFloat() >= 0.0f ) )
		return Lerp( cl_csm_parallel_split_dist3.GetFloat(), flZNear, flZFar );

	Assert( flZNear > 0.f );
	Assert( flZFar > flZNear );
	// http://appsrv.cse.cuhk.edu.hk/~fzhang/pssm_vrcia/shadow_vrcia.pdf
	const float fIM = (float)nSplit / (float)nShadowSplits;
	const float f = MIN( flMaxShadowDistance, flZFar );
	const float n = flZNear;
	const float fBias = 0.1f;

	const float flLog = n * powf( f / n, fIM );	
	const float flLinear = n + ( f - n ) * fIM;
	
	const float flLogScaler = clamp( cl_csm_parallel_split_log_lin_lerp.GetFloat(), 0.0f, 1.0f );
	const float flLinScaler = 1.0f - flLogScaler;

	return ( flLog * flLogScaler + flLinear * flLinScaler ) + fBias;
}

#define MAX_INCLUSION_VOLUME_PLANES 11

template<typename T> inline void swapPlane( T &lhs, T &rhs ) { T temp( lhs ); lhs = rhs; rhs = temp; }

extern ConVar cl_csm_print_culling_planes;

// Note: flMaxDistBetweenAnyCasterAndReceiver is only used to ensure the "top" cap plane is placed far enough away from the view frustum to catch all potential casters.
uint CCSMParallelSplit::ComputeCullingVolumePlanes( VPlane *pOutPlanes, Vector vDirToLight, const CFrustum &sceneFrustum, float flMaxDistBetweenAnyCasterAndReceiver )
{
	Vector vLightDir( -vDirToLight );

	const VMatrix sceneWorldToProj( sceneFrustum.GetViewProj() );
	VMatrix sceneProjToWorld;
	InvertVMatrix( sceneWorldToProj, sceneProjToWorld );
		
	// Extract world relative frustum planes (UpdateCameraAndFrustumFromOrthoViewProjMatrices computes camera worldspace relative planes assuming no world->view translation).
	VPlane vWorldFrustumPlanes[CCSMFrustumDefinition::NUM_FRUSTUM_PLANES];
	ExtractClipPlanesFromNonTransposedMatrix( sceneWorldToProj, vWorldFrustumPlanes );

	// Canonicalize the plane order to be consistent with the predefined frustum edge/quad definitions, new order: posX, negX, posY, negY, posZ, negZ.
	swapPlane( vWorldFrustumPlanes[0], vWorldFrustumPlanes[1] );
	swapPlane( vWorldFrustumPlanes[2], vWorldFrustumPlanes[3] );
	
	// Compute frustum cornerpoints. First set are the actual cornerpoints, the 2nd set are pushed "up" towards the light.
	Vector vWorldFrustumVerts[CCSMFrustumDefinition::NUM_FRUSTUM_VERTS * 2];
	for ( uint i = 0; i < CCSMFrustumDefinition::NUM_FRUSTUM_VERTS; ++i )
	{
		sceneProjToWorld.V3Mul( Vector( CCSMFrustumDefinition::g_vProjFrustumVerts[i].x, CCSMFrustumDefinition::g_vProjFrustumVerts[i].y, CCSMFrustumDefinition::g_vProjFrustumVerts[i].z ), vWorldFrustumVerts[i] );
		if ( !vWorldFrustumVerts[i].IsValid() )
		{
			// If this assert fires, something is probably wrong with the frustum's matrices.
			// this log triggers all the time with the particle editor up
			//Log_Warning(  LOG_SCENESYSTEM, "CCSMParallelSplit::ComputeCullingVolumePlanes: Failed computing world frustum vertices from inverted scene matrices!\n" );
			return 0;
		}
		vWorldFrustumVerts[i + CCSMFrustumDefinition::NUM_FRUSTUM_VERTS] = vWorldFrustumVerts[i] + vDirToLight * flMaxDistBetweenAnyCasterAndReceiver;
	}
	
	const Vector vFrustumCenter( ( vWorldFrustumVerts[0] + vWorldFrustumVerts[6] ) * .5f );

	for ( uint i = 0; i < CCSMFrustumDefinition::NUM_FRUSTUM_PLANES; ++i )
	{
		if ( vWorldFrustumPlanes[i].DistTo( vFrustumCenter ) <= 0.0f )
		{
			// Invalid view frustum, don't even try to compute culling volume (should never happen).
			return 0;
		}
	}
	uint nNumCullingPlanes = 0;
	
	// First compute "lower" planes, which are any planes from the scene's view frustum which face towards the light.
	bool bFacingFlags[CCSMFrustumDefinition::NUM_FRUSTUM_QUADS];
	uint nNumFacingPlanes = 0, nNumEdgePlanes = 0;
	for ( uint nQuadIndex = 0; nQuadIndex < CCSMFrustumDefinition::NUM_FRUSTUM_QUADS; ++nQuadIndex )
	{
		const VPlane& lowerPlane = vWorldFrustumPlanes[nQuadIndex];

		Assert( lowerPlane.DistTo( vFrustumCenter ) >= 0.0f );

		const bool bFacingFlag = ( lowerPlane.m_Normal.Dot( vLightDir ) ) < 0.0f;

		bFacingFlags[nQuadIndex] = bFacingFlag;

		// Reject quads that face away from the light (i.e. the "uppermost" quads relative to the light dir).
		
		if ( !bFacingFlag )
			continue;

		pOutPlanes[nNumCullingPlanes++] = lowerPlane;
		
		nNumFacingPlanes++;
	}

	// Compute the upper plane, which faces towards the center of the view frustum but is very far away in the direction of the light to capture all potential shadow casters. 
	VPlane upperPlane;
	upperPlane.m_Normal = vLightDir;
	upperPlane.m_Dist = 0.0f;

	double flPlaneDist = 0.0f;

	for ( int i = 0; i < CCSMFrustumDefinition::NUM_FRUSTUM_VERTS * 2; ++i )
	{
		double flDist = vWorldFrustumVerts[i].x * upperPlane.m_Normal.x + vWorldFrustumVerts[i].y * upperPlane.m_Normal.y + vWorldFrustumVerts[i].z * upperPlane.m_Normal.z - flPlaneDist;
		if ( flDist < 0.0f )
		{
			flPlaneDist += flDist;
		}
	}
		
	// FIXME: Find a better way to ensure the frustum verts (plus the extra "top" vert) are always on the pos halfspace of the top cap plane. This is a crappy way to account for double->float precision loss. 
	flPlaneDist *= 1.125f;

	upperPlane.m_Dist = static_cast< float >( flPlaneDist );
		
//#ifdef _DEBUG
#if 0
	for ( int i = 0; i < CCSMFrustumDefinition::NUM_FRUSTUM_VERTS * 2; ++i )
	{
		float flDist = upperPlane.DistTo( vWorldFrustumVerts[i] );
		Assert( flDist >= 0.0f );
	}
#endif

	Assert( upperPlane.DistTo( vFrustumCenter ) >= 0.0f );
	pOutPlanes[nNumCullingPlanes++] = upperPlane;

	// Now compute "side" planes by finding the silhouette edges relative to the light dir.
	for ( uint nEdgeIndex = 0; nEdgeIndex < CCSMFrustumDefinition::NUM_FRUSTUM_EDGES; ++nEdgeIndex )
	{
		// Determine if the quads sharing this edge are a silhouette.
		if ( bFacingFlags[CCSMFrustumDefinition::g_nEdgeQuadIndices[nEdgeIndex][0]] == bFacingFlags[CCSMFrustumDefinition::g_nEdgeQuadIndices[nEdgeIndex][1]])
			continue;

		Vector vEdgeVec( vWorldFrustumVerts[CCSMFrustumDefinition::g_nEdgeVertIndices[nEdgeIndex][0]] - vWorldFrustumVerts[CCSMFrustumDefinition::g_nEdgeVertIndices[nEdgeIndex][1]]);
		vEdgeVec.NormalizeInPlace();
		
		Vector vNorm( vEdgeVec.Cross( vLightDir) );
		vNorm.NormalizeInPlace();
		VPlane sidePlane;
		sidePlane.m_Normal = vNorm;
		sidePlane.m_Dist = vNorm.Dot( vWorldFrustumVerts[CCSMFrustumDefinition::g_nEdgeVertIndices[nEdgeIndex][0]] );
			
		// Ensure side plane is always facing towards the center of the view frustum.
		if ( sidePlane.DistTo( vFrustumCenter ) < 0.0f)
		{
			sidePlane = sidePlane.Flip();
		}

		pOutPlanes[nNumCullingPlanes++] = sidePlane;
		
		nNumEdgePlanes++;
	}
		
	if ( nNumCullingPlanes < 6 )
	{
		// this log triggers continuously when editing particles
		//Log_Warning( LOG_SCENESYSTEM, "CCSMParallelSplit::ComputeCullingVolumePlanes: Failed computing closed culling volume!\n" );
		return 0;
	}

	Assert( nNumCullingPlanes <= CVolumeCuller::cMaxInclusionVolumePlanes );
	
	// nNumFacingPlanes can range from [1,5]
	// nNumEdgePlanes can either be 4 or 6.
	// There's always 1 "upper" cap plane.
	// Worst case number of planes (I think):
	// Facing=5, Edge=4, Total=10
	// Facing=4, Edge=6, Total=11
	if ( cl_csm_print_culling_planes.GetBool() ) 
	{
		Warning( "Total inclusion planes: %u (Facing: %u Edge: %u)\n", nNumCullingPlanes, nNumFacingPlanes, nNumEdgePlanes );
	}
	
	return nNumCullingPlanes;
}

float CCSMParallelSplit::ComputeFarPlaneCameraRelativePoints( Vector *pPoints, float flZNear, float flZFar, const CFrustum &frustum )
{
	float flClipSpaceBottomLeftX = -1.0f;
	float flClipSpaceBottomLeftY = -1.0f;
	float flClipSpaceTopRightX = 1.0f;
	float flClipSpaceTopRightY = 1.0f;
		
	if ( 0 ) //( GetFlag( SUNLIGHT_MANAGER_FLAG_FIXED_NEAR_FAR_PLANES_FOR_CASCADE_CREATION ) )
	{ 
		// Ensure that we're an ortho off-center frustum, push our bounds out
		if ( flClipSpaceBottomLeftX < -1 || flClipSpaceBottomLeftY < -1 || flClipSpaceTopRightX > 1 || flClipSpaceTopRightY > 1 )
		{
			flClipSpaceBottomLeftX -= 400.0f;
			flClipSpaceBottomLeftY -= 400.0f;
			flClipSpaceTopRightX += 400.0f;
			flClipSpaceTopRightY += 400.0f;
		}
	}

	frustum.CalcFarPlaneCameraRelativePoints( pPoints, flZNear, flClipSpaceBottomLeftX, flClipSpaceBottomLeftY, flClipSpaceTopRightX, flClipSpaceTopRightY );
	frustum.CalcFarPlaneCameraRelativePoints( &pPoints[ 4 ], flZFar, flClipSpaceBottomLeftX, flClipSpaceBottomLeftY, flClipSpaceTopRightX, flClipSpaceTopRightY );
	
	const Vector &vEye = frustum.GetCameraPosition();

	for ( uint i = 0; i < CCSMFrustumDefinition::NUM_FRUSTUM_VERTS; ++i )
	{
		pPoints[i] += vEye;
	}

	// Brute force code to determine the largest possible projection of this cascade's subfrustum from the light's point of view.
	float flSubfrustumDiagonalLength = 0.0f;
	for ( uint i = 0; i < 8; i++ )
	{
		for ( uint j = i + 1; j < 8; j++ )
		{
			Vector vSubfrustumDiagonal( pPoints[i] - pPoints[j] );
			float flTrialDiagonalLength = vSubfrustumDiagonal.Length();
			if ( flTrialDiagonalLength > flSubfrustumDiagonalLength  )
			{
				flSubfrustumDiagonalLength  = flTrialDiagonalLength;
			}
		}
	}
	return flSubfrustumDiagonalLength;
}

void CCSMParallelSplit::CalculateShadowFrustaParallelSplits( 
	const CFrustum &sceneFrustum,
	uint &nOutCascadeSize,
	CFrustum *pOutFrustums, 
	CVolumeCuller *pOutVolumeCullers,
	float &flOutActualMaxShadowDist, 
	int nMaxCascadeSize, 
	Vector vDirToLight, 
	float flMaxShadowDistance, 
	float flMaxVisibleDistance,
	int nCSMQualityLevel,
	ShadowFrustaDebugInfo_t* pDebugInfo )
{
	const uint NUM_WORLD_FOCUS_POINTS = 8;
	
	// vDirToLight - dir to light
	vDirToLight.NormalizeInPlace();
		
	//const Vector &vEye = sceneFrustum.GetCameraPosition();
	const Vector vEyeDir( sceneFrustum.CameraForward().Normalized() );

	//VMatrix invViewProj( sceneFrustum.GetInvViewProj().Transpose() );
		
	float flSplitPlaneDistances[MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE];

	float flSceneZNear = MAX( 0.125f, sceneFrustum.GetCameraNearPlane() );
	float flSceneZFar = sceneFrustum.GetCameraFarPlane();
					
	for ( int nCascadeIndex = 0; nCascadeIndex < nMaxCascadeSize; ++nCascadeIndex )
	{
		flSplitPlaneDistances[nCascadeIndex] = CalculateSplitPlaneDistance( 1 + nCascadeIndex, nMaxCascadeSize, flMaxShadowDistance, flSceneZNear, flSceneZFar );
	}
	
	// Compute scene frustum cornerpoints at the max. shadow distance, which is used to compute the max. actual shadow distance used for smoothly transitioning to unshadowed based off distance.
	Vector vWorldFrustumVertsAtMaxShadowDist[CCSMFrustumDefinition::NUM_FRUSTUM_VERTS];
	float flMaxSubfrustumDiagonalLength = ComputeFarPlaneCameraRelativePoints( vWorldFrustumVertsAtMaxShadowDist, flSceneZNear, flSplitPlaneDistances[nMaxCascadeSize - 1], sceneFrustum );
	if ( IsGameConsole() || ( nCSMQualityLevel <= CSMQUALITY_LOW ) )
	{
		// 7LS - check with RichG - this seems to give the appropriate length calc for using radial distance fade in the PS as opposed to the one below
		// RG - Yup, this is correct.
		flOutActualMaxShadowDist = flMaxSubfrustumDiagonalLength / 1.42f; // sqrt(2) - account for dist from center to corner of square texture
	}
	else
	{
		flOutActualMaxShadowDist = flMaxSubfrustumDiagonalLength * 1.42f; // sqrt(2) - account for dist from center to corner of square texture
	}
	ComputeFarPlaneCameraRelativePoints( vWorldFrustumVertsAtMaxShadowDist, flSceneZNear, flOutActualMaxShadowDist, sceneFrustum );
			
	// flMaxDistanceBetweenAnyCasterAndReceiver is only needed to capture all potential invisible shadow casters.
	// We don't have the scene's bounds so there's a big fudge factor here to account for large terrains and low sun angles.
	// We wouldn't need to push back the near plane so much if we where using pancaking.
	//const float flMaxDistanceBetweenAnyCasterAndReceiver = MAX( flOutActualMaxShadowDist, MAX( MAX( flMaxVisibleDistance, flMaxShadowDistance ), 15000.0f ) );
	const float flMaxDistanceBetweenAnyCasterAndReceiver = MAX( flOutActualMaxShadowDist, MAX( MAX( flMaxVisibleDistance, flMaxShadowDistance ), 2000.0f ) );

	//flOutActualMaxShadowDist = flMaxSubfrustumDiagonalLength; // sqrt(2) - account for dist from center to corner of square texture
		
	// Compute look-at matrix 
	Vector shadowWorldToViewAt( -vDirToLight );
	Vector shadowWorldToViewRight( CrossProduct( Vector( 0, 0, 1 ), shadowWorldToViewAt ) );
	if ( shadowWorldToViewRight.LengthSqr() < .001f )
	{
		shadowWorldToViewRight = CrossProduct( Vector( 0, 1, 0 ), shadowWorldToViewAt );
	}
	shadowWorldToViewRight.NormalizeInPlace();

	Vector shadowWorldToViewUp( CrossProduct( shadowWorldToViewAt, shadowWorldToViewRight ) );
	shadowWorldToViewUp.NormalizeInPlace();

	VMatrix shadowWorldToViewRot;
	
	shadowWorldToViewRot.Init(
		shadowWorldToViewRight.x, shadowWorldToViewRight.y, shadowWorldToViewRight.z, 0.0f, 
		-shadowWorldToViewUp.x, -shadowWorldToViewUp.y, -shadowWorldToViewUp.z, 0.0f,
		-shadowWorldToViewAt.x, -shadowWorldToViewAt.y, -shadowWorldToViewAt.z, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f );
	
	nOutCascadeSize = 0;

	//float flCascade0ShadowViewMaxZ = -1e-10f; 
	float flCascade0ShadowViewMinZ = 1e-10f; flCascade0ShadowViewMinZ;
	VPlane exclusionFrustumPlanes[MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE][6];

	COMPILE_TIME_ASSERT( CVolumeCuller::cMaxInclusionVolumePlanes >= MAX_INCLUSION_VOLUME_PLANES );
	
	struct CascadeParams_t
	{
		VMatrix m_ShadowWorldToView;
		VMatrix m_ShadowViewToProj;
		float m_flLeft;
		float m_flRight;
		float m_flTop;
		float m_flBottom;
		float m_flZNear;
		float m_flZFar;
	};

	CascadeParams_t cascadeParams[MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE];

	float flCascadeShadowViewMinZ = 1e+10f;
	float flCascadeShadowViewMaxZ = -1e-10f;

	for ( int nCascadeIndex = 0; nCascadeIndex < nMaxCascadeSize; ++nCascadeIndex )
	{
		float flSceneSplitZNear = !nCascadeIndex ? flSceneZNear : flSplitPlaneDistances[nCascadeIndex - 1];
		float flSceneSplitZFar = flSplitPlaneDistances[nCascadeIndex];

		if ( nCascadeIndex )
		{
			// No need to expand the cascade beyond the max visible distance (we'll never see any shadows cast into invisible areas of the scene).
			if ( flSceneSplitZNear > flMaxVisibleDistance )
				break;
		}

		// 0=[left,bottom],1=[right,bottom],2=[left,top],3=[right,top]
		Vector vSubfrustumWorldPoints[ NUM_WORLD_FOCUS_POINTS ];
		ComputeFarPlaneCameraRelativePoints( vSubfrustumWorldPoints, flSceneSplitZNear, flSceneSplitZFar, sceneFrustum );
					
		// Determine bounds of orthographic projection of this subfrustum's cornerpoints.
		Vector shadowFocusRegionVerts[NUM_WORLD_FOCUS_POINTS];
		const float flVeryLarge = 9e+12;
		Vector vShadowFocusRegionMin( flVeryLarge, flVeryLarge, flVeryLarge );
		Vector vShadowFocusRegionMax( -flVeryLarge, -flVeryLarge, -flVeryLarge );
		for ( uint i = 0; i < NUM_WORLD_FOCUS_POINTS; ++i )
		{
			shadowWorldToViewRot.V3Mul( vSubfrustumWorldPoints[i], shadowFocusRegionVerts[i] );
			vShadowFocusRegionMin = vShadowFocusRegionMin.Min( shadowFocusRegionVerts[i] );
			vShadowFocusRegionMax = vShadowFocusRegionMax.Max( shadowFocusRegionVerts[i] );
		}

		// Expand the subfrustum "up" towards the light to catch all shadow casters (even those outside of the scene's view frustum).
		for ( uint i = 0; i < NUM_WORLD_FOCUS_POINTS; ++i )
		{
			Vector vLightPoint;
			shadowWorldToViewRot.V3Mul( vSubfrustumWorldPoints[i] + vDirToLight * flMaxDistanceBetweenAnyCasterAndReceiver, vLightPoint );
			vShadowFocusRegionMin.z = MIN( vShadowFocusRegionMin.z, vLightPoint.z );
			vShadowFocusRegionMax.z = MAX( vShadowFocusRegionMax.z, vLightPoint.z );
		}

		// Now expand the Z near/far range so it fully encompasses the actual worldspace area that could be shadowed with this cascade's texture (this must take into account the shadow distance fading in the shader).
		// Note this is very wasteful of Z precision.
		for ( uint i = 0; i < CCSMFrustumDefinition::NUM_FRUSTUM_VERTS; ++i )
		{
			Vector vLightPoint;
			shadowWorldToViewRot.V3Mul( vWorldFrustumVertsAtMaxShadowDist[i], vLightPoint );
			vShadowFocusRegionMin.z = MIN( vShadowFocusRegionMin.z, vLightPoint.z );
			vShadowFocusRegionMax.z = MAX( vShadowFocusRegionMax.z, vLightPoint.z );
		}

		flCascadeShadowViewMinZ = MIN( vShadowFocusRegionMin.z, flCascadeShadowViewMinZ );
		flCascadeShadowViewMaxZ = MAX( vShadowFocusRegionMax.z, flCascadeShadowViewMaxZ );
	}

	// Important note: Source1 uses right handed ortho projection matrices, so negative viewspace Z is actually inside the frustum. This is opposite from the CSM code in Source2.
	for ( int nCascadeIndex = 0; nCascadeIndex < nMaxCascadeSize; ++nCascadeIndex )
	{
		pDebugInfo[nCascadeIndex].m_nNumWorldFocusVerts = NUM_WORLD_FOCUS_POINTS;

		float flSceneSplitZNear = !nCascadeIndex ? flSceneZNear : flSplitPlaneDistances[nCascadeIndex - 1];
		float flSceneSplitZFar = flSplitPlaneDistances[nCascadeIndex];
		
		if ( nCascadeIndex )
		{
			// No need to expand the cascade beyond the max visible distance (we'll never see any shadows cast into invisible areas of the scene).
			if ( flSceneSplitZNear > flMaxVisibleDistance )
				break;
		}
				
		pDebugInfo[nCascadeIndex].m_flSplitPlaneDistance = flSceneSplitZFar;
		
		// 0=[left,bottom],1=[right,bottom],2=[left,top],3=[right,top]
		Vector vSubfrustumWorldPoints[ NUM_WORLD_FOCUS_POINTS ];
		float flSubfrustumDiagonalLength = ComputeFarPlaneCameraRelativePoints( vSubfrustumWorldPoints, flSceneSplitZNear, flSceneSplitZFar, sceneFrustum );
		
		flSubfrustumDiagonalLength = ceil( flSubfrustumDiagonalLength * 100.0f ) / 100.0f;
				
		Vector vSubfrustumWorldCenter( ( vSubfrustumWorldPoints[0] + vSubfrustumWorldPoints[4 + 3] ) * .5f );
							
		V_memcpy( &pDebugInfo[nCascadeIndex].m_WorldFocusVerts, vSubfrustumWorldPoints, sizeof( vSubfrustumWorldPoints ) );
		
		// Determine bounds of orthographic projection of this subfrustum's cornerpoints.
		Vector shadowFocusRegionVerts[NUM_WORLD_FOCUS_POINTS];
		const float flVeryLarge = 9e+12;
		Vector vShadowFocusRegionMin( flVeryLarge, flVeryLarge, flVeryLarge );
		Vector vShadowFocusRegionMax( -flVeryLarge, -flVeryLarge, -flVeryLarge );
		for ( uint i = 0; i < NUM_WORLD_FOCUS_POINTS; ++i )
		{
			shadowWorldToViewRot.V3Mul( vSubfrustumWorldPoints[i], shadowFocusRegionVerts[i] );
			vShadowFocusRegionMin = vShadowFocusRegionMin.Min( shadowFocusRegionVerts[i] );
			vShadowFocusRegionMax = vShadowFocusRegionMax.Max( shadowFocusRegionVerts[i] );
		}

		// Expand the subfrustum "up" towards the light to catch all shadow casters (even those outside of the scene's view frustum).
		for ( uint i = 0; i < NUM_WORLD_FOCUS_POINTS; ++i )
		{
			Vector vLightPoint;
			shadowWorldToViewRot.V3Mul( vSubfrustumWorldPoints[i] + vDirToLight * flMaxDistanceBetweenAnyCasterAndReceiver, vLightPoint );
			vShadowFocusRegionMin.z = MIN( vShadowFocusRegionMin.z, vLightPoint.z );
			vShadowFocusRegionMax.z = MAX( vShadowFocusRegionMax.z, vLightPoint.z );
		}
		
		// Now expand the Z near/far range so it fully encompasses the actual worldspace area that could be shadowed with this cascade's texture (this must take into account the shadow distance fading in the shader).
		// Note this is very wasteful of Z precision.
		for ( uint i = 0; i < CCSMFrustumDefinition::NUM_FRUSTUM_VERTS; ++i )
		{
			Vector vLightPoint;
			shadowWorldToViewRot.V3Mul( vWorldFrustumVertsAtMaxShadowDist[i], vLightPoint );
			vShadowFocusRegionMin.z = MIN( vShadowFocusRegionMin.z, vLightPoint.z );
			vShadowFocusRegionMax.z = MAX( vShadowFocusRegionMax.z, vLightPoint.z );
		}

#if 1
		// Set all cascades to the same MinZ/MaxZ so we only need to transform into ortho Z once in the shader (not per cascade). This is doable with 24-bit Z.
		vShadowFocusRegionMin.z = flCascadeShadowViewMinZ;
		vShadowFocusRegionMax.z = flCascadeShadowViewMaxZ;
#else				
		// This is tricky - push "back" the max shadow view Z of the cascades to be at least as far back (or away) as cascade 0's.
		// This is so the Z [0,1] transitioning in the sun shadow shader can safely assume that it's always possible to transition to the next sequential cascade when Z is out of range.
		if ( !nCascadeIndex )
		{
			flCascade0ShadowViewMinZ = vShadowFocusRegionMin.z;
		}
		else
		{
			vShadowFocusRegionMin.z = MIN( vShadowFocusRegionMin.z, flCascade0ShadowViewMinZ );
		}
#endif
		
		// Push "back" the shadow camera towards the light, so the NearZ used to construct the projection matrix can be set to 0.
		// X, Y MUST be 0,0 here for the UV continuity solution to work.
		VMatrix shadowWorldToViewTrans;
		
		Vector vShadowCameraPos( 0.0f, 0.0f, vShadowFocusRegionMax.z );
		vShadowFocusRegionMin.z -= vShadowFocusRegionMax.z;
		vShadowFocusRegionMax.z = 0.0f;

		MatrixBuildTranslation( shadowWorldToViewTrans, -vShadowCameraPos );
		
		VMatrix shadowWorldToView( shadowWorldToViewTrans * shadowWorldToViewRot );

		float flShadowFocusRegionWidth = vShadowFocusRegionMax.x - vShadowFocusRegionMin.x;
		float flShadowFocusRegionHeight = vShadowFocusRegionMax.y - vShadowFocusRegionMin.y;

		float flShadowFocusRegionOffsetX = ( flSubfrustumDiagonalLength - flShadowFocusRegionWidth ) * .5f;
		float flShadowFocusRegionOffsetY = ( flSubfrustumDiagonalLength - flShadowFocusRegionHeight ) * .5f;

		// Expand the ortho projection so it is always large enough to contain the largest projection of the subfrustum in any orientation.
		vShadowFocusRegionMin.x -= flShadowFocusRegionOffsetX;
		vShadowFocusRegionMax.x += flShadowFocusRegionOffsetX;
		vShadowFocusRegionMin.y -= flShadowFocusRegionOffsetY;
		vShadowFocusRegionMax.y += flShadowFocusRegionOffsetY;

		// Eliminate any subpixel offset in the ortho projection parameters.
		float flWorldUnitsPerTexel = flSubfrustumDiagonalLength / m_nShadowBufferSize;
		
		extern ConVar cl_csm_xlat_continuity;
		if ( cl_csm_xlat_continuity.GetBool() )
		{
			vShadowFocusRegionMin.x = ((int)floor( vShadowFocusRegionMin.x / flWorldUnitsPerTexel ) & ~1 ) * flWorldUnitsPerTexel;
			vShadowFocusRegionMax.x = ((int)floor( vShadowFocusRegionMax.x / flWorldUnitsPerTexel ) & ~1 ) * flWorldUnitsPerTexel;
			vShadowFocusRegionMin.y = ((int)floor( vShadowFocusRegionMin.y / flWorldUnitsPerTexel ) & ~1 ) * flWorldUnitsPerTexel;
			vShadowFocusRegionMax.y = ((int)floor( vShadowFocusRegionMax.y / flWorldUnitsPerTexel ) & ~1 ) * flWorldUnitsPerTexel;

			vShadowFocusRegionMin.z = floor( vShadowFocusRegionMin.z );
			vShadowFocusRegionMax.z = ceil( vShadowFocusRegionMax.z );
		}
				
		// Build ortho projection matrix
		VMatrix shadowViewToProj;
		float left = vShadowFocusRegionMin.x, right = vShadowFocusRegionMax.x;
		float bottom = vShadowFocusRegionMax.y, top = vShadowFocusRegionMin.y;
		float zNear, zFar;
		
		// Flipped/negated because MatrixBuildOrtho() is RH (viewspace -Z is inside the frustum)
		zNear = vShadowFocusRegionMax.z ? -vShadowFocusRegionMax.z : 0.0f;
		zFar = -vShadowFocusRegionMin.z;
		
		// Beware MatrixBuildOrtho() actually flips top/bottom! (ARGH)
		MatrixBuildOrtho( shadowViewToProj, left, top, right, bottom, zNear, zFar );
						
		// Ensure m[2][3] is exactly 0.0f
		shadowViewToProj.m[2][3] = 0.0f;

		pDebugInfo[nCascadeIndex].m_flLeft = left;
		pDebugInfo[nCascadeIndex].m_flRight = right;
		pDebugInfo[nCascadeIndex].m_flTop = top;
		pDebugInfo[nCascadeIndex].m_flBottom = bottom;
		pDebugInfo[nCascadeIndex].m_flZNear = zNear;
		pDebugInfo[nCascadeIndex].m_flZFar = zFar;

#if 0
		VMatrix shadowProjToView;
		shadowViewToProj.InverseGeneral( shadowProjToView );
		Vector vCornerPoints[2];
		shadowProjToView.V3Mul( Vector( -1.0f,  1.0f, 0.0f ), vCornerPoints[0] );	// left/bottom
		shadowProjToView.V3Mul( Vector(  1.0f, -1.0f, 0.0f ), vCornerPoints[1] );	// right/top
		
		// testing
		VMatrix shadowWorldToProj( shadowViewToProj * shadowWorldToView );
		Vector vTestVerts[NUM_WORLD_FOCUS_POINTS];
		for ( uint i = 0; i < NUM_WORLD_FOCUS_POINTS; ++i )
		{
			shadowWorldToProj.V3Mul( vSubfrustumWorldPoints[i], vTestVerts[i] );
		}
#endif

		CascadeParams_t &params = cascadeParams[nCascadeIndex];
		params.m_ShadowWorldToView = shadowWorldToView;
		params.m_ShadowViewToProj = shadowViewToProj;
		params.m_flLeft = left;
		params.m_flRight = right;
		params.m_flTop = top;
		params.m_flBottom = bottom;
		params.m_flZNear = zNear;
		params.m_flZFar = zFar;

		nOutCascadeSize++;
	}

	VPlane cullingInclusionPlanes[CVolumeCuller::cMaxInclusionVolumePlanes];
	const uint nNumCullingInclusionPlanes = ComputeCullingVolumePlanes( cullingInclusionPlanes, vDirToLight, sceneFrustum, flMaxDistanceBetweenAnyCasterAndReceiver );
	
	// Phase 2 - create output frustum, matrices, and culling volumes.
	for ( uint nCascadeIndex = 0; nCascadeIndex < nOutCascadeSize; ++nCascadeIndex )
	{
		CascadeParams_t &params = cascadeParams[nCascadeIndex];

		// Build this cascade's output frustum.
		CFrustum *pOutFrustum = &pOutFrustums[nCascadeIndex];

		pOutFrustum->BuildShadowFrustum( params.m_ShadowWorldToView, params.m_ShadowViewToProj );

#if 0
		pOutFrustum->SetView( params.m_ShadowWorldToView );
		pOutFrustum->SetProj( params.m_ShadowViewToProj );
		pOutFrustum->CalcViewProj();
		pOutFrustum->UpdateCameraAndFrustumFromOrthoViewProjMatrices();
#endif
								
		// Compute exclusion frustum planes, taking into account smooth cascade transitioning.
		// The exclusion volume must completely fit inside the "inner" circle defining the beginning of the cascade transition region.
		float flLerpAmount = cl_csm_shadow_split_lerp_factor_range.GetFloat();
						
		VMatrix conservativeShadowViewToProj;
		MatrixBuildOrtho( conservativeShadowViewToProj, 
			Lerp( flLerpAmount, params.m_flLeft, params.m_flRight ),
			Lerp( flLerpAmount, params.m_flTop, params.m_flBottom ),
			Lerp( flLerpAmount, params.m_flRight, params.m_flLeft ),
			Lerp( flLerpAmount, params.m_flBottom, params.m_flTop ),
			params.m_flZNear, params.m_flZFar );
		
		VMatrix conservativeShadowWorldToProj( conservativeShadowViewToProj * params.m_ShadowWorldToView );
		// Extract world relative frustum planes (UpdateCameraAndFrustumFromOrthoViewProjMatrices computes camera worldspace relative planes assuming no world->view translation).
		VPlane *pFrustumPlanes = &exclusionFrustumPlanes[nCascadeIndex][0];
		
		ExtractClipPlanesFromNonTransposedMatrix( conservativeShadowWorldToProj, pFrustumPlanes );
						
		// Init this cascade's volume culler
		pOutVolumeCullers[nCascadeIndex].Clear();

		VPlane basePlanes[6];
		ExtractClipPlanesFromNonTransposedMatrix( pOutFrustum->GetViewProj(), basePlanes );
		pOutVolumeCullers[nCascadeIndex].SetBaseFrustumPlanes( basePlanes );

		if ( nCascadeIndex > 1 )
		{
			pOutVolumeCullers[nCascadeIndex].SetExclusionFrustumPlanes( &exclusionFrustumPlanes[nCascadeIndex - 1][0] );
		}
				
		if ( nNumCullingInclusionPlanes )
		{
			pOutVolumeCullers[nCascadeIndex].SetInclusionVolumePlanes( cullingInclusionPlanes, nNumCullingInclusionPlanes );
		}
	}
}

void CCSMParallelSplit::ComputeCascadeProjToTexMatrices( SunLightState_t &lightState )
{
	float fOffsetX = 0.5f + ( 0.5f / static_cast< float >( m_nShadowBufferSize ) );
	float fOffsetY = 0.5f + ( 0.5f / static_cast< float >( m_nShadowBufferSize ) );
	float frange   = 1.0f;
				
	COMPILE_TIME_ASSERT( 4 == MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE );

	float flBiases[MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE];
	static float s_flDefBliases[MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE] = { -0.000005f, -0.000005f, -0.000008f, -0.00009f };
	V_memcpy( flBiases, s_flDefBliases, sizeof( flBiases )	);
	
	const bool bInvertZ = IsPlatformX360(); 

	// Z is *not* inverted when rendering to R32F textures (because the Z flip is in viewport matrix created by rendersystem).
	if ( bInvertZ )
	{
		frange   = -1.0f;
	}
	
	for ( uint i = 0; i < lightState.m_nShadowCascadeSize; ++i )
	{	
		// Create projection to normalized texture space matrix used for shadow mapping.
		float flBiasVal = flBiases[i];
		if ( bInvertZ )
		{
			flBiasVal = 1.0f - flBiasVal;
		}

#if 0
		// This is a transposed matrix because CFrustum::GetViewProj() returns a transposed matrix.
		VMatrix texScaleBiasMat( 0.5f,     0.0f,     0.0f,      0.0f,
								 0.0f,    -0.5f,     0.0f,      0.0f,
								 0.0f,     0.0f,     frange,	0.0f,
								 fOffsetX, fOffsetY, flBiasVal,	1.0f );
#endif

		VMatrix texScaleBiasMat( 
			0.5f,     0.0f,     0.0f,		fOffsetX,
			0.0f,    -0.5f,     0.0f,		fOffsetY,
			0.0f,     0.0f,     frange,		flBiasVal,
			0.0f,	   0.0f,     0.0f,	    1.0f );
		
		lightState.m_CascadeProjToTexMatrices[ i ] = texScaleBiasMat;
	}
}

float g_flDebugForcedMaxVisibleRadius;

bool CCSMParallelSplit::Update( const SunLightViewState_t &viewState )
{
	if ( !m_nShadowBufferSize )
	{
		m_bValid = false;
		return false;
	}

	m_viewState = viewState;
	
	m_lightState.m_nShadowCascadeSize = m_nShadowCascadeMaxSize;

	// Important note: The scaleU/scaleV params are currently assumed to be (.5, .5) in the shaders!
	static const Vector4D s_vecAtlasUVs[4] = 
	{
		// ofsU, ofsV, scaleU, scaleV
		Vector4D( 0.0f, 0.0f, 0.5f, 0.5f ),
		Vector4D( 0.5f, 0.0f, 0.5f, 0.5f ),
		Vector4D( 0.0f, 0.5f, 0.5f, 0.5f ),
		Vector4D( 0.5f, 0.5f, 0.5f, 0.5f )
	};
	
	// Initializing them all because the loop below reads them all (to guarantee ALL shader constants are all initialized to something).
	for ( int i = 0; i < MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE; ++i )
	{
		m_lightState.m_SunLightShaderParams.m_vCascadeAtlasUVOffsets[i] = s_vecAtlasUVs[ ( i + viewState.m_nAtlasFirstCascadeIndex ) % MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE ];
	}
				
	Vector vDirToLight( 0, 0, 1 );
	vDirToLight = -m_viewState.m_Direction;
	
	Vector4D lightColor( m_viewState.m_LightColor.r / 255.0f, m_viewState.m_LightColor.g / 255.0f, m_viewState.m_LightColor.b / 255.0f, m_viewState.m_LightColorScale / 255.0f);

	// convert to linear space color for shader use, also save some shader instructions be pre-multiplying by 3.0f for bump normalized values stored in vertex/lightmap alpha for CSM blending
	lightColor.x = GammaToLinearFullRange( lightColor.x ) * lightColor.w * 3.0f;
	lightColor.y = GammaToLinearFullRange( lightColor.y ) * lightColor.w * 3.0f;
	lightColor.z = GammaToLinearFullRange( lightColor.z ) * lightColor.w * 3.0f;
	lightColor.w = 1.0f / g_pMaterialSystemHardwareConfig->GetLightMapScaleFactor();

	const CFrustum &sceneFrustum = m_viewState.m_frustum;;

	static bool bDebugBreak = false;
			
	float flMaxVisibleDist = m_viewState.m_flMaxVisibleDist;
	flMaxVisibleDist = MIN( flMaxVisibleDist, 7500.0f );	// arbitrary limit - where did I get this from?

	if ( g_flDebugForcedMaxVisibleRadius )
	{
		flMaxVisibleDist = g_flDebugForcedMaxVisibleRadius;
	}
	
	ComputeCascadeProjToTexMatrices( m_lightState );

	uint nMaxCascades = MIN( (int)m_nShadowCascadeMaxSize, (int)m_viewState.m_nMaxCascadeSize );
	if ( !nMaxCascades ) 
	{
		nMaxCascades = 1;
	}
	
	const float flMaxShadowDist = m_viewState.m_flMaxShadowDist;
	
	CalculateShadowFrustaParallelSplits( sceneFrustum, m_lightState.m_nShadowCascadeSize, m_lightState.m_CascadeFrustums, m_lightState.m_CascadeVolumeCullers, m_lightState.m_flActualMaxShadowDist, nMaxCascades, vDirToLight, flMaxShadowDist, flMaxVisibleDist, viewState.m_nCSMQualityLevel, m_lightState.m_DebugInfo );

	m_lightState.m_SunLightShaderParams.m_nNumCascades = m_lightState.m_nShadowCascadeSize;
	for ( uint nCascadeIndex = 0; nCascadeIndex < MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE; ++nCascadeIndex )
	{
		m_lightState.m_SunLightShaderParams.m_matWorldToShadowTexMatrices[nCascadeIndex] = m_lightState.m_CascadeProjToTexMatrices[nCascadeIndex] * m_lightState.m_CascadeFrustums[nCascadeIndex].GetViewProj();

		Rect_t &viewport = m_lightState.m_CascadeViewports[nCascadeIndex];
		viewport.x = m_lightState.m_SunLightShaderParams.m_vCascadeAtlasUVOffsets[nCascadeIndex].x * m_nShadowAtlasWidth;
		viewport.y = m_lightState.m_SunLightShaderParams.m_vCascadeAtlasUVOffsets[nCascadeIndex].y * m_nShadowAtlasWidth;
		viewport.width = m_nShadowBufferSize;
		viewport.height = m_nShadowBufferSize;

		ExtractClipPlanesFromNonTransposedMatrix( m_lightState.m_CascadeFrustums[nCascadeIndex].GetViewProj(), &m_lightState.m_CascadeFrustumPlanes[nCascadeIndex][0] );
	}		

	m_lightState.m_SunLightShaderParams.m_vLightDir = vDirToLight;
	m_lightState.m_SunLightShaderParams.m_vLightColor = lightColor;	
	
	m_lightState.m_SunLightShaderParams.m_TexParams.m_flInvShadowTextureWidth = 1.0f / m_nShadowAtlasWidth;
	m_lightState.m_SunLightShaderParams.m_TexParams.m_flInvShadowTextureHeight = 1.0f / m_nShadowAtlasHeight;
	m_lightState.m_SunLightShaderParams.m_TexParams.m_flHalfInvShadowTextureWidth = .5f / m_nShadowAtlasWidth;
	m_lightState.m_SunLightShaderParams.m_TexParams.m_flHalfInvShadowTextureHeight = .5f / m_nShadowAtlasHeight;
	
	m_lightState.m_SunLightShaderParams.m_TexParams2.m_flShadowTextureWidth = m_nShadowAtlasWidth;
	m_lightState.m_SunLightShaderParams.m_TexParams2.m_flShadowTextureHeight = m_nShadowAtlasHeight;
	m_lightState.m_SunLightShaderParams.m_TexParams2.m_flSplitLerpFactorBase = .5f - cl_csm_shadow_split_lerp_factor_range.GetFloat();
	m_lightState.m_SunLightShaderParams.m_TexParams2.m_flSplitLerpFactorInvRange = 1.0f / cl_csm_shadow_split_lerp_factor_range.GetFloat();

#if 0
	// old linear distance fade (requires recip sqrt and sqrt in shader)
	float flZLerpStartDist = m_lightState.m_flActualMaxShadowDist * cl_csm_shadow_split_radial_dist_lerp_factor_multiplier.GetFloat();
	m_lightState.m_SunLightShaderParams.m_TexParams3.m_flDistLerpFactorBase = flZLerpStartDist;
	m_lightState.m_SunLightShaderParams.m_TexParams3.m_flDistLerpFactorInvRange = 1.0f / ( m_lightState.m_flActualMaxShadowDist - flZLerpStartDist );
	m_lightState.m_SunLightShaderParams.m_TexParams3.m_flDistLerpFactorBase *= -m_lightState.m_SunLightShaderParams.m_TexParams3.m_flDistLerpFactorInvRange;
#else
	// cheaper quadratic distance fade 
	float flZLerpStartDist = m_lightState.m_flActualMaxShadowDist * cl_csm_shadow_split_radial_dist_lerp_factor_multiplier.GetFloat();
	float flZLerpEndDist = m_lightState.m_flActualMaxShadowDist;
	float flQ = 1.0f / ( flZLerpEndDist * flZLerpEndDist - flZLerpStartDist * flZLerpStartDist );
	m_lightState.m_SunLightShaderParams.m_TexParams3.m_flDistLerpFactorBase = - ( ( flZLerpStartDist * flZLerpStartDist ) * flQ );
	m_lightState.m_SunLightShaderParams.m_TexParams3.m_flDistLerpFactorInvRange = flQ;
#endif

	m_lightState.m_SunLightShaderParams.m_TexParams3.m_flUnused0 = 0.0f;
	m_lightState.m_SunLightShaderParams.m_TexParams3.m_flUnused1 = 0.0f;

	m_lightState.m_SunLightShaderParams.m_vCamPosition.Init( sceneFrustum.GetCameraPosition().x, sceneFrustum.GetCameraPosition().y, sceneFrustum.GetCameraPosition().z, 1.0f );
	
	m_lightState.m_flZLerpStartDist = flZLerpStartDist;
	m_lightState.m_flZLerpEndDist = m_lightState.m_flActualMaxShadowDist;

	if ( viewState.m_bSetAllCascadesToFirst )
	{
		// Copy cascade 0's world->shadow matrix and UV texture offsets to all other cascades (used for the viewmodel),
		// so the pixel shader can use any of them for viewmodel shadowing.
		for ( int i = 1; i < MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE; ++i )
		{
			m_lightState.m_SunLightShaderParams.m_vCascadeAtlasUVOffsets[i] = m_lightState.m_SunLightShaderParams.m_vCascadeAtlasUVOffsets[0];
			m_lightState.m_SunLightShaderParams.m_matWorldToShadowTexMatrices[ i ] = m_lightState.m_SunLightShaderParams.m_matWorldToShadowTexMatrices[ 0 ];
		}
	}
	
	m_bValid = true;

	return true;
}
