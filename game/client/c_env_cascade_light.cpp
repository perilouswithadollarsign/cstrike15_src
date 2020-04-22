//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Directional lighting with cascaded shadow mapping entity.
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

#include "c_env_cascade_light.h"

#include "engine/ivdebugoverlay.h"
#include "materialsystem/imaterialvar.h"
#include "view_shared.h"
#include "iviewrender.h"
#include "c_world.h"
#include "materialsystem/materialsystem_config.h"

#if defined (_PS3 )
#include <algorithm>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//#define CsmDbgMsg Msg
#define CsmDbgMsg(x)

#define MAX_CSM_CASCADES 3

ConVar cl_csm_enabled( "cl_csm_enabled", "1", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_max_shadow_dist("cl_csm_max_shadow_dist", ( IsX360() ) ? "350" : IsPS3() ? "250" : "-1", FCVAR_DEVELOPMENTONLY, "" );

ConVar cl_csm_capture_state( "cl_csm_capture_state", "0", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_clear_captured_state( "cl_csm_clear_captured_state", "0", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_debug_render_ztest( "cl_csm_debug_render_ztest", "1", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_max_visible_dist("cl_csm_max_visible_dist", "5000", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_debug_vis_lo_range("cl_csm_debug_vis_lo_range", ".35", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_debug_vis_hi_range("cl_csm_debug_vis_hi_range", "1.0", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_use_forced_view_matrices("cl_csm_use_forced_view_matrices", "1", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_debug_2d( "cl_csm_debug_2d", "0", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_debug_3d( "cl_csm_debug_3d", "0", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_debug_culling( "cl_csm_debug_culling", "0", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_debug_culling_cascade( "cl_csm_debug_culling_cascade", "-1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_print_culling_planes( "cl_csm_print_culling_planes", "0", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_viz_numplanes( "cl_csm_viz_numplanes", "-1", FCVAR_DEVELOPMENTONLY, "" );
ConVar cl_csm_viz_polyhedron_quad_size( "cl_csm_viz_polyhedron_quad_size", "131072", FCVAR_DEVELOPMENTONLY, "" );

ConVar cl_csm_use_env_light_direction( "cl_csm_use_env_light_direction", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_rot_override( "cl_csm_rot_override", "0", FCVAR_DEVELOPMENTONLY );

// dust2's angles
ConVar cl_csm_rot_x( "cl_csm_rot_x", "50", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_rot_y( "cl_csm_rot_y", "43", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_rot_z( "cl_csm_rot_z", "0", FCVAR_DEVELOPMENTONLY );

ConVar cl_csm_disable_culling( "cl_csm_disable_culling", "0", FCVAR_DEVELOPMENTONLY );

ConVar cl_csm_shadows( "cl_csm_shadows", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_entity_shadows( "cl_csm_entity_shadows", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_static_prop_shadows( "cl_csm_static_prop_shadows", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_world_shadows( "cl_csm_world_shadows", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_world_shadows_in_viewmodelcascade( "cl_csm_world_shadows_in_viewmodelcascade", ( IsGameConsole() || IsPlatformOSX() ) ? "0" : "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_sprite_shadows( "cl_csm_sprite_shadows", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_rope_shadows( "cl_csm_rope_shadows", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_translucent_shadows( "cl_csm_translucent_shadows", ( IsGameConsole() || IsPlatformOSX()  )? "0" : "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_translucent_shadows_using_opaque_path( "cl_csm_translucent_shadows_using_opaque_path", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_ignore_disable_shadow_depth_rendering( "cl_csm_ignore_disable_shadow_depth_rendering", "0", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_optimize_static_props( "cl_csm_optimize_static_props", "1", FCVAR_DEVELOPMENTONLY, "Enable/Disable optimal static prop rendering into CSM's (cull static props that make no visual contribution to shadows)" );

ConVar cl_csm_viewmodel_shadows( "cl_csm_viewmodel_shadows", "1", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_viewmodel_max_shadow_dist( "cl_csm_viewmodel_max_shadow_dist", "21", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_viewmodel_farz( "cl_csm_viewmodel_farz", ( IsGameConsole() || IsPlatformOSX() ) ? "15" : "30", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_viewmodel_max_visible_dist( "cl_csm_viewmodel_max_visible_dist", "1000", FCVAR_DEVELOPMENTONLY );

ConVar cl_csm_slopescaledepthbias_c0( "cl_csm_slopescaledepthbias_c0", ( IsGameConsole() || IsPlatformOSX() ) ? "2" : "1.3", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_slopescaledepthbias_c1( "cl_csm_slopescaledepthbias_c1", IsPlatformOSX() ? "4" : "2", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_slopescaledepthbias_c2( "cl_csm_slopescaledepthbias_c2", IsPlatformOSX() ? "4" : "2", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_slopescaledepthbias_c3( "cl_csm_slopescaledepthbias_c3", "2", FCVAR_DEVELOPMENTONLY );

ConVar cl_csm_depthbias_c0(	"cl_csm_depthbias_c0", ( IsGameConsole() || IsPlatformOSX() ) ? ".000005" : ".000025", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_depthbias_c1(	"cl_csm_depthbias_c1", IsPlatformOSX() ? "2" : ".000025", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_depthbias_c2(	"cl_csm_depthbias_c2", IsPlatformOSX() ? "2" : ".000025", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_depthbias_c3(	"cl_csm_depthbias_c3", ".000025", FCVAR_DEVELOPMENTONLY );

ConVar cl_csm_viewmodel_slopescaledepthbias( "cl_csm_viewmodel_slopescaledepthbias", ( IsGameConsole() || IsPlatformOSX() ) ? "2" : "1.5", FCVAR_DEVELOPMENTONLY );
ConVar cl_csm_viewmodel_depthbias( "cl_csm_viewmodel_depthbias", ( IsGameConsole() || IsPlatformOSX() ) ? ".000005" : ".00005", FCVAR_DEVELOPMENTONLY );

ConVar cl_csm_hack_proj_matrices_for_cull_debugging( "cl_csm_hack_proj_matrices_for_cull_debugging", "0", FCVAR_DEVELOPMENTONLY );

ConVar cl_csm_xlat_continuity( "cl_csm_xlat_continuity", "1", FCVAR_DEVELOPMENTONLY );

ConVar cl_csm_force_no_csm_in_reflections( "cl_csm_force_no_csm_in_reflections", "0", FCVAR_DEVELOPMENTONLY );

ConVar cl_csm_cull_small_prop_threshold_volume( "cl_csm_cull_small_prop_threshold_volume", "2000.0f ", FCVAR_DEVELOPMENTONLY );

void CC_CSM_Status( const CCommand& args );
static ConCommand cl_csm_status("cl_csm_status", CC_CSM_Status, "Usage:\n   cl_csm_status\n", 0);

CCascadeLightManager g_CascadeLightManager;

IMPLEMENT_CLIENTCLASS_DT(C_CascadeLight, DT_CascadeLight, CCascadeLight)
	RecvPropVector(RECVINFO(m_shadowDirection)),
	RecvPropVector(RECVINFO(m_envLightShadowDirection)),
	RecvPropBool(RECVINFO(m_bEnabled)),
	RecvPropBool(RECVINFO(m_bUseLightEnvAngles)),
	RecvPropInt(RECVINFO(m_LightColor), 0, RecvProxy_Int32ToColor32),
	RecvPropInt(RECVINFO(m_LightColorScale), 0, RecvProxy_Int32ToInt32 ),
	RecvPropFloat(RECVINFO(m_flMaxShadowDist) )
END_RECV_TABLE()

LINK_ENTITY_TO_CLASS(env_cascade_light, C_CascadeLight);

C_CascadeLight *C_CascadeLight::m_pCascadeLight;

C_CascadeLight::C_CascadeLight() : 
	C_BaseEntity(),
	m_shadowDirection( 0, 0, -1 ),
	m_envLightShadowDirection( 0, 0, -1 ),
	m_bEnabled( false ),
	m_bUseLightEnvAngles( true ),
	m_flMaxShadowDist( 400.0f )
{
	CsmDbgMsg( "C_CascadeLight::C_CascadeLight\n" );
	
	m_LightColor.r = 255;
	m_LightColor.g = 255;
	m_LightColor.b = 255;
	m_LightColor.a = 255;
	m_LightColorScale = 255;
}

C_CascadeLight::~C_CascadeLight()
{
	CsmDbgMsg( "C_CascadeLight::~C_CascadeLight\n" );
}

void C_CascadeLight::Spawn()
{
	CsmDbgMsg( "C_CascadeLight::Spawn\n" );

	BaseClass::Spawn();

	SetNextClientThink( CLIENT_THINK_ALWAYS );

	m_pCascadeLight = this;
}

void C_CascadeLight::Release()
{
	CsmDbgMsg( "C_CascadeLight::Release\n" );

	m_pCascadeLight = NULL;

	BaseClass::Release();
}

bool C_CascadeLight::ShouldDraw()
{
	return false;
}

void C_CascadeLight::ClientThink()
{
	VPROF("C_CascadeLight::ClientThink");

	BaseClass::ClientThink();
}

// 4 cascades plus 1 for the scene frustum itself
static Vector g_vCascadeFrustumColors[ 4 + 1 ] =
{
	Vector( 0, 1, 0 ),
	Vector( 0, 0, 1 ),
	Vector( 0, 1, 1 ),
	Vector( 1, 0, 0 ),
	Vector( .85f, .85f, .2f )
};

void CCascadeLightManager::RotXPlusDown( const CCommand &args ) { g_CascadeLightManager.m_flRotX[0] = 1.0f; }
void CCascadeLightManager::RotXPlusUp( const CCommand &args ) { g_CascadeLightManager.m_flRotX[0] = 0.0f; }
void CCascadeLightManager::RotXNegDown( const CCommand &args ) { g_CascadeLightManager.m_flRotX[1] = -1.0f; }
void CCascadeLightManager::RotXNegUp( const CCommand &args ) { g_CascadeLightManager.m_flRotX[1] = 0.0f; }
void CCascadeLightManager::RotYPlusDown( const CCommand &args ) { g_CascadeLightManager.m_flRotY[0] = 1.0f; }
void CCascadeLightManager::RotYPlusUp( const CCommand &args ) { g_CascadeLightManager.m_flRotY[0] = 0.0f; }
void CCascadeLightManager::RotYNegDown( const CCommand &args ) { g_CascadeLightManager.m_flRotY[1] = -1.0f; }
void CCascadeLightManager::RotYNegUp( const CCommand &args ) { g_CascadeLightManager.m_flRotY[1] = 0.0f; }

static ConCommand start_csm_rot_x_plus( "+csm_rot_x_plus", CCascadeLightManager::RotXPlusDown);
static ConCommand end_csm_rot_x_plus( "-csm_rot_x_plus", CCascadeLightManager::RotXPlusUp);
static ConCommand start_csm_rot_x_neg( "+csm_rot_x_neg", CCascadeLightManager::RotXNegDown);
static ConCommand end_csm_rot_x_neg( "-csm_rot_x_neg", CCascadeLightManager::RotXNegUp);
static ConCommand start_csm_rot_y_plus( "+csm_rot_y_plus", CCascadeLightManager::RotYPlusDown);
static ConCommand end_csm_rot_y_plus( "-csm_rot_y_plus", CCascadeLightManager::RotYPlusUp);
static ConCommand start_csm_rot_y_neg( "+csm_rot_y_neg", CCascadeLightManager::RotYNegDown);
static ConCommand end_csm_rot_y_neg( "-csm_rot_y_neg", CCascadeLightManager::RotYNegUp);

static void DebugRenderWireframeFrustum3D( const VMatrix &xform, const Vector &color, bool bDepthTest = false )
{
	for ( uint i = 0; i < CCSMFrustumDefinition::NUM_FRUSTUM_LINES; ++i )
	{
		Vector4D s, e;

		xform.V4Mul( CCSMFrustumDefinition::g_vProjFrustumVerts[ CCSMFrustumDefinition::g_nFrustumLineVertIndices[ i * 2 + 0 ] ], s );
		xform.V4Mul( CCSMFrustumDefinition::g_vProjFrustumVerts[ CCSMFrustumDefinition::g_nFrustumLineVertIndices[ i * 2 + 1 ] ], e );

		s *= ( 1.0f / s.w );
		e *= ( 1.0f / e.w );

		NDebugOverlay::Line( Vector( s.x, s.y, s.z ), Vector( e.x, e.y, e.z ), (int)(color.x * 255.0f), (int)(color.y * 255.0f), (int)(color.z * 255.0f), !bDepthTest, NDEBUG_PERSIST_TILL_NEXT_SERVER );
	}
}

static void Add3DLineOverlay( const Vector &s, const Vector &e, const Vector &color, bool bDepthTest )
{
	NDebugOverlay::Line( s, e, (int)(color.x * 255.0f), (int)(color.y * 255.0f), (int)(color.z * 255.0f), !bDepthTest, NDEBUG_PERSIST_TILL_NEXT_SERVER );
}

#if defined(_X360)
template <class T> void _swap ( T& a, T& b )
{
	T c(a); a=b; b=c;
}
#endif

static void DebugRenderConvexPolyhedron( const Vector4D *pPlanes, uint nNumPlanes, const Vector &color, bool zTest )
{
#ifdef _DEBUG
	for ( uint nPlaneIndex = 0; nPlaneIndex < nNumPlanes; ++nPlaneIndex )
	{
		float l = pPlanes[nPlaneIndex].x*pPlanes[nPlaneIndex].x + pPlanes[nPlaneIndex].y*pPlanes[nPlaneIndex].y + pPlanes[nPlaneIndex].z*pPlanes[nPlaneIndex].z;
		l = sqrt( l );
		Assert( (fabs( l ) - 1.0f) < .00125f );
	}
#endif

	uint nLastPlane = cl_csm_viz_numplanes.GetInt();
	if ( nLastPlane < 0 )
		nLastPlane = nNumPlanes;
	nLastPlane = MIN( nLastPlane, nNumPlanes );

	const float Q = cl_csm_viz_polyhedron_quad_size.GetFloat();

	for ( uint nPlaneIndex = 0; nPlaneIndex < nLastPlane; ++nPlaneIndex )
	{
		Vector4D plane( pPlanes[nPlaneIndex] );
				
		Vector vNormal( plane.x, plane.y, plane.z );
		float flDist = -plane.w;

		Vector vOrigin( vNormal * flDist );
		
		uint nMinorAxis = 0;
		float flMag = fabs( vNormal.x );
		if ( fabs( vNormal.y ) < flMag ) { flMag = fabs( vNormal.y ); nMinorAxis = 1; }
		if ( fabs( vNormal.z ) < flMag ) { flMag = fabs( vNormal.z ); nMinorAxis = 2; }

		Vector vU( 0.0f, 0.0f, 0.0f );
		vU.Base()[nMinorAxis] = 1.0f;

		float flDirAlong = vU.Dot( vNormal );
		
		vU -= flDirAlong * vNormal;
		vU.NormalizeInPlace();

		Vector vV( vNormal.Cross( vU ) );
		vV.NormalizeInPlace();
						
		double verts[2][64][3];
		
		verts[0][0][0] = vOrigin.x - vU.x * Q - vV.x * Q;
		verts[0][0][1] = vOrigin.y - vU.y * Q - vV.y * Q;
		verts[0][0][2] = vOrigin.z - vU.z * Q - vV.z * Q;

		verts[0][1][0] = vOrigin.x + vU.x * Q - vV.x * Q;
		verts[0][1][1] = vOrigin.y + vU.y * Q - vV.y * Q;
		verts[0][1][2] = vOrigin.z + vU.z * Q - vV.z * Q;

		verts[0][2][0] = vOrigin.x + vU.x * Q + vV.x * Q;
		verts[0][2][1] = vOrigin.y + vU.y * Q + vV.y * Q;
		verts[0][2][2] = vOrigin.z + vU.z * Q + vV.z * Q;

		verts[0][3][0] = vOrigin.x - vU.x * Q + vV.x * Q;
		verts[0][3][1] = vOrigin.y - vU.y * Q + vV.y * Q;
		verts[0][3][2] = vOrigin.z - vU.z * Q + vV.z * Q;
				
		uint nVerts = 4;
		double *pSrcVerts = &verts[0][0][0];
		double *pDstVerts = &verts[1][0][0];

		for ( uint nClipPlaneIndex = 0; nClipPlaneIndex < nLastPlane; ++nClipPlaneIndex )
		{
			if ( nPlaneIndex == nClipPlaneIndex )
				continue;

			Vector4D clipPlane( pPlanes[nClipPlaneIndex] );

			double vClipNormal[3] = { clipPlane.x, clipPlane.y, clipPlane.z };
			double flClipDist = -clipPlane.w;

			int nClipped = ClipPolyToPlane_Precise( pSrcVerts, nVerts, pDstVerts, vClipNormal, flClipDist, .000000125f );
			
			nVerts = nClipped;
#if defined(_X360)
			_swap( pSrcVerts, pDstVerts );
#else
			std::swap( pSrcVerts, pDstVerts );
#endif
			if ( nVerts < 3 )
				break;
		}

		if ( nVerts >= 3 )
		{
			Vector vAvg( 0.0f, 0.0f, 0.0f );
			for ( uint i = 0; i < nVerts; ++i )
			{
				const uint j = ( i + 1 ) % nVerts;
				Vector s( pSrcVerts[i*3+0], pSrcVerts[i*3+1], pSrcVerts[i*3+2] );
				Vector e( pSrcVerts[j*3+0], pSrcVerts[j*3+1], pSrcVerts[j*3+2] );
				
				Add3DLineOverlay( s, e, color, zTest );
				vAvg += s;
			}

			vAvg /= nVerts;

			Vector2D vLo( 1e+10f, 1e+10f );
			Vector2D vHi( -1e+10f, -1e+10f );
			for ( uint i = 0; i < nVerts; ++i )
			{
				Vector p( pSrcVerts[i*3+0], pSrcVerts[i*3+1], pSrcVerts[i*3+2] );

				p -= vAvg;
				Vector2D p2D( p.Dot( vU ),  p.Dot( vV ) );
				
				vLo = vLo.Min( p2D );
				vHi = vHi.Max( p2D );
			}

			float L = 75.0f;
			Add3DLineOverlay( vAvg, vAvg + vNormal * L, Vector( .1f, .1f, 1.0f ), true );
			Add3DLineOverlay( vAvg, vAvg + vU * L, Vector( 1.0f, .1f, .1f ), true );
			Add3DLineOverlay( vAvg, vAvg + vV * L, Vector( 0.1f, 1.0f, .1f ), true );

			float flXD = ( vHi.x - vLo.x ) / 24.0f;
			float flYD = ( vHi.y - vLo.y ) / 24.0f;

			for ( float flX = vLo.x; flX < vHi.x; flX += flXD )
			{
				for ( float flY = vLo.y; flY < vHi.y; flY += flYD )
				{
					Vector p( vAvg + vU * flX + vV * flY );

					uint i;
					for ( i = 0; i < nLastPlane; ++i )
					{
						if ( i == nPlaneIndex )
							continue;
						float flDist = pPlanes[i].x * p.x + pPlanes[i].y * p.y + pPlanes[i].z * p.z + pPlanes[i].w;
						if ( flDist < -.25f )
							break;
					}
					if ( i == nLastPlane )
					{
						Add3DLineOverlay( p, p + vNormal * 4.0f, Vector( .1f, .1f, 1.0f ), true );
					}
				}
			}
		}
	}

}

static void	ScreenText( int x, int y, int r, int g, int b, int a, const char *text, ...)
{
	char buf[256];
	va_list args;
	va_start( args, text );
	V_vsnprintf( buf, sizeof( buf ), text, args );
	va_end( args );

	const float flTextColWidth = 0.0044f; // 227
	const float flTextRowHeight = 0.0083f; // 120		
	NDebugOverlay::ScreenText( x * flTextColWidth, y * flTextRowHeight, buf, r, g, b, a, NDEBUG_PERSIST_TILL_NEXT_SERVER );
}

CDebugPrimRenderer2D::CDebugPrimRenderer2D()
{
	m_debugLines.EnsureCapacity( 128 );
}

void CDebugPrimRenderer2D::Clear()
{
	m_debugLines.SetCount( 0 );
}

void CDebugPrimRenderer2D::AddNormalizedLine2D( float sx, float sy, float ex, float ey, uint r, uint g, uint b )
{
	int nIndex = m_debugLines.AddToTail();
	m_debugLines[nIndex].m_EndPoints[0].Init( sx, sy );
	m_debugLines[nIndex].m_EndPoints[1].Init( ex, ey );
	m_debugLines[nIndex].m_nColor[0] = r; 
	m_debugLines[nIndex].m_nColor[1] = g;	
	m_debugLines[nIndex].m_nColor[2] = b; 
	m_debugLines[nIndex].m_nColor[3] = 255;
}

void CDebugPrimRenderer2D::AddScreenspaceLine2D( float sx, float sy, float ex, float ey, uint r, uint g, uint b )
{
	int nBackBufWidth, nBackBufHeight;
	materials->GetBackBufferDimensions( nBackBufWidth, nBackBufHeight );
	float flInvBackBufWidth = 1.0f / nBackBufWidth, flInvBackBufHeight = 1.0f / nBackBufHeight;

	int nIndex = m_debugLines.AddToTail();
	m_debugLines[nIndex].m_EndPoints[0].Init( sx * flInvBackBufWidth, sy * flInvBackBufHeight );
	m_debugLines[nIndex].m_EndPoints[1].Init( ex * flInvBackBufWidth, ey * flInvBackBufHeight );
	m_debugLines[nIndex].m_nColor[0] = r; 
	m_debugLines[nIndex].m_nColor[1] = g;	
	m_debugLines[nIndex].m_nColor[2] = b; 
	m_debugLines[nIndex].m_nColor[3] = 255;
}

void CDebugPrimRenderer2D::AddScreenspaceRect2D( float sx, float sy, float ex, float ey, uint r, uint g, uint b )
{
	AddScreenspaceLine2D( sx, sy, ex, sy, r, g, b );
	AddScreenspaceLine2D( ex, sy, ex, ey, r, g, b );
	AddScreenspaceLine2D( ex, ey, sx, ey, r, g, b );
	AddScreenspaceLine2D( sx, ey, sx, sy, r, g, b );
}

void CDebugPrimRenderer2D::AddScreenspaceLineList2D( uint nCount, const Vector2D *pVerts, const VertexColor_t &color )
{
	int nBackBufWidth, nBackBufHeight;
	materials->GetBackBufferDimensions( nBackBufWidth, nBackBufHeight );
	float flInvBackBufWidth = 1.0f / nBackBufWidth, flInvBackBufHeight = 1.0f / nBackBufHeight;

	for ( uint i = 0; i < nCount; ++i )
	{
		int nIndex = m_debugLines.AddToTail();
		m_debugLines[nIndex].m_EndPoints[0].Init( pVerts[0].x * flInvBackBufWidth, pVerts[0].y * flInvBackBufHeight );
		m_debugLines[nIndex].m_EndPoints[1].Init( pVerts[1].x * flInvBackBufWidth, pVerts[1].y * flInvBackBufHeight );
		m_debugLines[nIndex].m_nColor[0] = color.r;
		m_debugLines[nIndex].m_nColor[1] = color.g;
		m_debugLines[nIndex].m_nColor[2] = color.b; 
		m_debugLines[nIndex].m_nColor[3] = 255;

		pVerts += 2;
	}
}

void CDebugPrimRenderer2D::RenderScreenspaceDepthTexture( float sx, float sy, float ex, float ey, float su, float sv, float eu, float ev, CTextureReference &depthTex, float zLo, float zHi )
{
	int nBackBufWidth, nBackBufHeight;
	materials->GetBackBufferDimensions( nBackBufWidth, nBackBufHeight );
	float flInvBackBufWidth = 1.0f / nBackBufWidth, flInvBackBufHeight = 1.0f / nBackBufHeight;

	sx *= flInvBackBufWidth;
	sy *= flInvBackBufHeight;
	ex *= flInvBackBufWidth;
	ey *= flInvBackBufHeight;

	const float xl = -1.0f;
	const float yl = 1.0f; 
	const float xh = 1.0f; 
	const float yh = -1.0f;

	IMaterial *pMaterial = materials->FindMaterial( "debug/debugshadowbuffer", TEXTURE_GROUP_OTHER, true );
	if ( !pMaterial ) 
		return;
			
	float flInvZRange = 1.0f / ( zHi - zLo );

	IMaterialVar *c0_x = pMaterial->FindVar( "$c0_x", NULL, false );
	c0_x->SetFloatValue( flInvZRange );	

	IMaterialVar *c0_y = pMaterial->FindVar( "$c0_y", NULL, false );
	c0_y->SetFloatValue( -zLo * flInvZRange );

	IMaterialVar *c0_z = pMaterial->FindVar( "$c0_z", NULL, false );
	c0_z->SetFloatValue( 0.0f );

	bool bFound = false;
	IMaterialVar *pMatVar = pMaterial->FindVar( "$basetexture", &bFound, false );
	if ( bFound && pMatVar )
	{
		if ( pMatVar->GetTextureValue() != depthTex )
		{
			pMatVar->SetTextureValue( depthTex );
		}
	}

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( pMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.Position3f( Lerp( sx, xl, xh ), Lerp( sy, yl, yh ), 0.0f );
	meshBuilder.TexCoord2f( 0, su, sv );
	meshBuilder.Color4ub( 255, 255, 255, 255);
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( Lerp( ex, xl, xh ), Lerp( sy, yl, yh ), 0.0f );
	meshBuilder.TexCoord2f( 0, eu, sv );
	meshBuilder.Color4ub( 255, 255, 255, 255);
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( Lerp( ex, xl, xh ), Lerp( ey, yl, yh ), 0.0f );
	meshBuilder.TexCoord2f( 0, eu, ev );
	meshBuilder.Color4ub( 255, 255, 255, 255);
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( Lerp( sx, xl, xh ), Lerp( ey, yl, yh ), 0.0f );
	meshBuilder.TexCoord2f( 0, su, ev );
	meshBuilder.Color4ub( 255, 255, 255, 255);
	meshBuilder.AdvanceVertex();

	meshBuilder.End();
	pMesh->Draw();
}

void CDebugPrimRenderer2D::Render2D( )
{
	if ( m_debugLines.Count() )
	{
		RenderDebugLines2D( m_debugLines.Count(), &m_debugLines[0] );
	}
}

void CDebugPrimRenderer2D::RenderDebugLines2D( uint nNumLines, const CDebugLine *pLines )
{
	if ( !nNumLines )
		return;

	const float xl = -1.0f;
	const float yl = 1.0f;
	const float xh = 1.0f;
	const float yh = -1.0f;
		
	IMaterial *pMaterial = materials->FindMaterial( "debug/debugscreenspacewireframe", TEXTURE_GROUP_OTHER, true );
	if ( !pMaterial ) 
		return;

	IMaterialVar *c0_x = pMaterial->FindVar( "$c0_x", NULL, false );
	c0_x->SetFloatValue( 1.0f );	

	IMaterialVar *c0_y = pMaterial->FindVar( "$c0_y", NULL, false );
	c0_y->SetFloatValue( 1.0f );

	IMaterialVar *c0_z = pMaterial->FindVar( "$c0_z", NULL, false );
	c0_z->SetFloatValue( 1.0f );

	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->Bind( pMaterial );
	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );

	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_LINES, nNumLines );

	for ( uint i = 0; i < nNumLines; ++i )
	{
		const CDebugLine &debugLine = pLines[i];

		meshBuilder.Position3f( Lerp( debugLine.m_EndPoints[0].x, xl, xh ), Lerp( debugLine.m_EndPoints[0].y, yl, yh ), 0.0f );
		meshBuilder.Color4ub( debugLine.m_nColor[0], debugLine.m_nColor[1], debugLine.m_nColor[2], 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( Lerp( debugLine.m_EndPoints[1].x, xl, xh ), Lerp( debugLine.m_EndPoints[1].y, yl, yh ), 0.0f );
		meshBuilder.Color4ub( debugLine.m_nColor[0], debugLine.m_nColor[1], debugLine.m_nColor[2], 255 );
		meshBuilder.AdvanceVertex();
	}

	meshBuilder.End();
	pMesh->Draw();
}

void CDebugPrimRenderer2D::AddScreenspaceWireframeFrustum2D( const VMatrix &xform, const VertexColor_t &color, bool bShowAxes )
{
	Vector2D points[ CCSMFrustumDefinition::NUM_FRUSTUM_LINES * 2 ];
	Vector2D *pDstPoint = points;

	for ( uint i = 0; i < CCSMFrustumDefinition::NUM_FRUSTUM_LINES; ++i )
	{
		Vector4D s, e;

		xform.V4Mul( CCSMFrustumDefinition::g_vProjFrustumVerts[ CCSMFrustumDefinition::g_nFrustumLineVertIndices[ i * 2 + 0 ] ], s );
		xform.V4Mul( CCSMFrustumDefinition::g_vProjFrustumVerts[ CCSMFrustumDefinition::g_nFrustumLineVertIndices[ i * 2 + 1 ] ], e );

		s *= ( 1.0f / s.w );
		e *= ( 1.0f / e.w );

		pDstPoint[0].Init( s.x, s.y ); 
		pDstPoint[1].Init( e.x, e.y );
		pDstPoint += 2;
	}

	AddScreenspaceLineList2D( CCSMFrustumDefinition::NUM_FRUSTUM_LINES, points, color );

	if ( bShowAxes )
	{
		Vector4D s, e;
		xform.V4Mul( Vector4D( 0.0f, 0.0f, 0.0f, 1.0f ), s );
		xform.V4Mul( Vector4D( 0.0f, 0.0f, 0.95f, 1.0f ), e );
		s *= ( 1.0f / s.w );
		e *= ( 1.0f / e.w );
		points[0].Init( s.x, s.y ); 
		points[1].Init( e.x, e.y );
		AddScreenspaceLineList2D( 1, points, VertexColor_t( 20, 20, 255, 255 ) );

		xform.V4Mul( Vector4D( 0.0f, 0.0f, 0.0f, 1.0f ), s );
		xform.V4Mul( Vector4D( 0.0f, 15.0f, 0.0f, 1.0f ), e );
		s *= ( 1.0f / s.w );
		e *= ( 1.0f / e.w );
		points[0].Init( s.x, s.y ); 
		points[1].Init( e.x, e.y );
		AddScreenspaceLineList2D( 1, points, VertexColor_t( 20, 255, 20, 255 ) );

		xform.V4Mul( Vector4D( 0.0f, 0.0f, 0.0f, 1.0f ), s );
		xform.V4Mul( Vector4D( 15.0f, 0.0f, 0.0f, 1.0f ), e );
		s *= ( 1.0f / s.w );
		e *= ( 1.0f / e.w );
		points[0].Init( s.x, s.y ); 
		points[1].Init( e.x, e.y );
		AddScreenspaceLineList2D( 1, points, VertexColor_t( 255, 20, 20, 255 ) );
	}
}

// TODO: Break CCascadeLightManager out to a separate file?

#if defined( _X360 )
	#define CSM_DEFAULT_DEPTH_TEXTURE_RESOLUTION 704*2
#elif defined( _PS3 )
	#define CSM_DEFAULT_DEPTH_TEXTURE_RESOLUTION 640*2
#else
	// Important note: On PC, this depth texture resolution (or the inverse of it) is effectively hardcoded into the filter kernels sample offsets. Don't change it unless you fix this dependency.
	#define CSM_DEFAULT_DEPTH_TEXTURE_RESOLUTION 1024*2
	#define CSM_FALLBACK_DEPTH_TEXTURE_RESOLUTION 768*2
	#define CSM_FALLBACK2_DEPTH_TEXTURE_RESOLUTION 640*2
#endif

CCascadeLightManager::CCascadeLightManager() :
	m_bRenderTargetsAllocated( false ),
	m_nDepthTextureResolution( CSM_DEFAULT_DEPTH_TEXTURE_RESOLUTION ),
	m_bCSMIsActive( false ),
	m_bStateIsValid( false ),
	m_nCurRenderTargetQualityMode( CSMQUALITY_HIGH )
{
	V_memset( &m_flRotX, 0, sizeof( m_flRotX ) );
	V_memset( &m_flRotY, 0, sizeof( m_flRotY ) );
		
	m_curState.m_CSMParallelSplit.Init( m_nDepthTextureResolution / 2, MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE );
	m_curViewModelState.m_CSMParallelSplit.Init( m_nDepthTextureResolution / 2, MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE );
}

CCascadeLightManager::~CCascadeLightManager()
{
}

#ifdef OSX

static ConVar mat_osx_force_csm_enabled( "mat_osx_force_csm_enabled", "0", FCVAR_RELEASE );

static bool OSX_HardwareGoodEnoughForCSMs()
{
	if ( IsPlatformOSX() )
	{
		// Historically, CS:GO did not have CSMs or multicore rendering on Mac. Both features are
		// available on Mac post the Sep 2014 Linux port integration, but multicore is not enough
		// to absorb the perf hit of CSMs on low end Macs. This function identifies the Macs on
		// which we do not want to enable CSMs, those that satisfy the following properties:
		// 1. lowend GPU identified in CShaderDeviceMgrBase::ReadHardwareCaps by setting 
		//    the convar mat_osx_csm_enabled to false;
		// 2. CPU has four or less logical processors (and less than 2.6GHz recorded clock speed)

		if ( mat_osx_force_csm_enabled.GetBool() )
		{
			return true;
		}

		bool bGoodEnough = true;

		// Check GPU
		static ConVarRef mat_osx_csm_enabled( "mat_osx_csm_enabled" );
		if ( !mat_osx_csm_enabled.GetBool() )
		{
			// GPU not good enough
			//printf("CSM: GPU matched string \"%s\", not good enough\n");
			bGoodEnough = false;
		}

		// Check CPU
		CPUInformation const& cpuInfo = GetCPUInformation();

		//printf( "CSM: CPU has %d logical processors\n", cpuInfo.m_nLogicalProcessors );

		if ( cpuInfo.m_nLogicalProcessors <= 4 )
		{
			// allow if clock speed is >= 2.6GHz, observing the list of Mac CPU's since Jan '08, this will now enable CSM's
			// on most Mac Pro's and iMacs (those that have the ability for GPU's to pass the test above, but would have been excluded due to logical processor count).
			if ( ( (double)cpuInfo.m_Speed / 1000000000.0 ) < 2.6 )
			{
				//printf("CSM: CPU cores not enough\n");
				bGoodEnough = false;
			}
		}

		return bGoodEnough;
	}
	else
	{
		// Platform other than OSX
		Assert( 0 );
		return true;
	}
}

#endif

bool CCascadeLightManager::InitRenderTargets()
{
	VPROF_BUDGET( "CCascadeLightManager::InitRenderTargets", VPROF_BUDGETGROUP_SHADOW_DEPTH_TEXTURING );
	
	CsmDbgMsg( "C_CascadeLight::InitRenderTargets\n" );

	if (
#ifdef OSX
		!OSX_HardwareGoodEnoughForCSMs() ||
#endif
		!cl_csm_enabled.GetBool() ||
		!g_pMaterialSystemHardwareConfig->SupportsCascadedShadowMapping() ||
		!g_pMaterialSystemHardwareConfig->SupportsShadowDepthTextures()
		)
	{
		DeinitRenderTargets();

		cl_csm_enabled.SetValue( 0 );
		return false;
	}

	if ( m_bRenderTargetsAllocated )
		return true;
	
	m_bRenderTargetsAllocated = true;
			
	ImageFormat dstFormat  = g_pMaterialSystemHardwareConfig->GetShadowDepthTextureFormat();	// Vendor-dependent depth texture format
#ifndef _X360
	ImageFormat nullFormat = g_pMaterialSystemHardwareConfig->GetNullTextureFormat();			// Vendor-dependent null texture format (takes as little memory as possible)
#endif
		
	RenderTargetSizeMode_t sizeMode = RT_SIZE_OFFSCREEN;
	
	// Don't allow the shadow buffer render target's to get resized to always be <= the size of the backbuffer on the PC.
	// This allows us to use 1024x1024 or larger shadow depth buffers when 1024x768 backbuffers, for example.
	sizeMode = RT_SIZE_NO_CHANGE;

	m_nCurRenderTargetQualityMode = GetCSMQualityMode();

	if( !( IsGameConsole() ) )
	{
		m_nDepthTextureResolution = CSM_DEFAULT_DEPTH_TEXTURE_RESOLUTION;
		if ( GetCSMQualityMode() == CSMQUALITY_VERY_LOW )
		{
			m_nDepthTextureResolution = CSM_FALLBACK2_DEPTH_TEXTURE_RESOLUTION;
		}
		else if ( GetCSMQualityMode() == CSMQUALITY_LOW )
		{
			m_nDepthTextureResolution = CSM_FALLBACK_DEPTH_TEXTURE_RESOLUTION;
		}
	}
	
	m_curState.m_CSMParallelSplit.Init( m_nDepthTextureResolution / 2, MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE );
	m_curViewModelState.m_CSMParallelSplit.Init( m_nDepthTextureResolution / 2, MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE );

	materials->BeginRenderTargetAllocation();
	
#if defined(_PS3)
	
	m_DummyColorTexture.InitRenderTarget( 8, 8, sizeMode, nullFormat, 
 										  MATERIAL_RT_DEPTH_NONE, false, "_rt_CSMDummy" );
	m_ShadowDepthTexture.InitRenderTarget( m_nDepthTextureResolution, m_nDepthTextureResolution, sizeMode, dstFormat, 
											MATERIAL_RT_DEPTH_NONE, false, "_rt_CSMShadowDepth" );

#elif defined(_X360)

	// For the 360, we'll be rendering depth directly into the dummy depth and Resolve()ing to the depth texture.
	// only need the dummy surface, don't care about color results
	m_DummyColorTexture.InitRenderTargetTexture( m_nDepthTextureResolution/2, m_nDepthTextureResolution/2, RT_SIZE_OFFSCREEN, IMAGE_FORMAT_BGR565, //IMAGE_FORMAT_BGRA8888, 
												 MATERIAL_RT_DEPTH_SHARED, false, "_rt_CSMShadowDummy", CREATERENDERTARGETFLAGS_ALIASCOLORANDDEPTHSURFACES );
	m_DummyColorTexture.InitRenderTargetSurface( m_nDepthTextureResolution/2, m_nDepthTextureResolution/2, IMAGE_FORMAT_BGR565, false );

	m_ShadowDepthTexture.InitRenderTargetTexture( m_nDepthTextureResolution, m_nDepthTextureResolution, RT_SIZE_OFFSCREEN, 
												  dstFormat, MATERIAL_RT_DEPTH_NONE, false, "_rt_CSMShadowDepth" );
	m_ShadowDepthTexture.InitRenderTargetSurface( 1, 1, dstFormat, false );

#else

	m_DummyColorTexture.InitRenderTarget( m_nDepthTextureResolution, m_nDepthTextureResolution, sizeMode, nullFormat, 
		MATERIAL_RT_DEPTH_NONE, false, "_rt_CSMShadowDummy" );
	m_ShadowDepthTexture.InitRenderTarget( m_nDepthTextureResolution, m_nDepthTextureResolution, sizeMode, dstFormat, 
		MATERIAL_RT_DEPTH_NONE, false, "_rt_CSMShadowDepth" );

#endif

	materials->EndRenderTargetAllocation();

	if ( ( !m_DummyColorTexture.IsValid() ) && ( !m_ShadowDepthTexture.IsValid() ) )
	{
		DeinitRenderTargets();

		cl_csm_enabled.SetValue(0);
		
		return false;
	}

	V_memset( &m_flRotX, 0, sizeof( m_flRotX ) );
	V_memset( &m_flRotY, 0, sizeof( m_flRotY ) );

	return true;
}

void CCascadeLightManager::ShutdownRenderTargets()
{
	// This purposely does NOT actually destroy the render targets here, to clone the (strange, but well tested?) behavior of the clientshadowmgr.cpp.
	// Not destroying the shadow buffers when baseclientrendertargets.cpp calls this method is beneficial when loading HDR maps, which triggers a recreation of the well known 
	// render targets during level load.
	CsmDbgMsg( "C_CascadeLight::ShutdownRenderTargets\n" );
}

void CCascadeLightManager::LevelInitPreEntity()
{
	CsmDbgMsg( "C_CascadeLight::LevelInitPreEntity\n" );
}

void CCascadeLightManager::LevelInitPostEntity()
{
	CsmDbgMsg( "C_CascadeLight::LevelInitPostEntity\n" );
}

void CCascadeLightManager::LevelShutdownPreEntity()
{
	CsmDbgMsg( "C_CascadeLight::LevelShutdownPreEntity\n" );
}

void CCascadeLightManager::LevelShutdownPostEntity()
{
	CsmDbgMsg( "C_CascadeLight::LevelShutdownPostEntity\n" );
}

void CCascadeLightManager::Shutdown()
{
	CsmDbgMsg( "C_CascadeLight::Shutdown\n" );

	DeinitRenderTargets();
}

void CCascadeLightManager::DeinitRenderTargets()
{
	CsmDbgMsg( "C_CascadeLight::DeinitRenderTargets\n" );

	UnlockAllShadowDepthTextures();

	if ( m_bRenderTargetsAllocated )
	{
		m_DummyColorTexture.Shutdown();
		m_ShadowDepthTexture.Shutdown();
		
		m_bRenderTargetsAllocated = false;
	}
}

bool CCascadeLightManager::IsEnabled() const
{
	if ( !C_CascadeLight::Get() || !C_CascadeLight::Get()->IsEnabled() || !cl_csm_enabled.GetBool() || !m_bRenderTargetsAllocated )
		return false;
	return true;
}

bool CCascadeLightManager::IsEnabledAndActive() const
{
	return IsEnabled() && ( C_CascadeLight::Get() != NULL );
}

void CCascadeLightManager::Draw3DDebugInfo()
{
	if ( !IsEnabled() )
		return;
			
	CFullCSMState &state = m_capturedState.m_CSMParallelSplit.IsValid() ? m_capturedState : m_curState;

	if ( !state.m_CSMParallelSplit.IsValid() )
		return;

	const bool zTest = cl_csm_debug_render_ztest.GetBool();

	const SunLightState_t &lightState = state.m_CSMParallelSplit.GetLightState();
	const CFrustum *pCascadeFrustums = lightState.m_CascadeFrustums; pCascadeFrustums;
	const ShadowFrustaDebugInfo_t *pDebugInfo = lightState.m_DebugInfo; pDebugInfo;
	//const SunLightShaderParamsCB_t &shadowParams = lightState.m_SunLightShaderParams;
	const VMatrix *pCascadeProjToTexMatrices = lightState.m_CascadeProjToTexMatrices; pCascadeProjToTexMatrices;
	const CFrustum &sceneFrustum = state.m_sceneFrustum;

	const VMatrix sceneWorldToView( sceneFrustum.GetView() );
	const VMatrix &sceneViewToProj = sceneFrustum.GetProj();
	const VMatrix sceneWorldToProj( sceneViewToProj * sceneWorldToView );
	VMatrix sceneProjToWorld;
	sceneWorldToProj.InverseGeneral( sceneProjToWorld );

	const Vector &vEye = sceneFrustum.GetCameraPosition(); 
	const Vector &vForward = sceneFrustum.CameraForward(); 
	const Vector &vLeft = sceneFrustum.CameraLeft(); 
	const Vector &vUp = sceneFrustum.CameraUp(); 

	const Vector vDirToLight( -state.m_shadowDir );

	COMPILE_TIME_ASSERT( 4 == MAX_SUN_LIGHT_SHADOW_CASCADE_SIZE );

	if ( cl_csm_debug_culling.GetBool() )
	{
		int s = 0, e = lightState.m_nShadowCascadeSize;
		if ( cl_csm_debug_culling_cascade.GetInt() >= 0 )
		{
			s = clamp<int, int, int>( cl_csm_debug_culling_cascade.GetInt(), 0, lightState.m_nShadowCascadeSize - 1 );
			e = s + 1;
		}
		for ( int i = s; i < e; ++i )
		{
			if ( lightState.m_CascadeVolumeCullers[i].HasBaseFrustum() )
			{
				VPlane basePlanes[CVolumeCuller::cNumBaseFrustumPlanes];
				lightState.m_CascadeVolumeCullers[i].GetBaseFrustumPlanes( basePlanes );
				int nPlaneCount = lightState.m_CascadeVolumeCullers[i].GetNumBaseFrustumPlanes();
				DebugRenderConvexPolyhedron( (Vector4D*)basePlanes, nPlaneCount, Vector( 1.0f, 1.0f, .2f ), true );
			}

			if ( lightState.m_CascadeVolumeCullers[i].HasInclusionVolume() )
				DebugRenderConvexPolyhedron( (Vector4D*)lightState.m_CascadeVolumeCullers[i].GetInclusionVolumePlanes(), lightState.m_CascadeVolumeCullers[i].GetNumInclusionVolumePlanes(), Vector( .3f, 1.0f, .3f ), true );

			if ( lightState.m_CascadeVolumeCullers[i].HasExclusionFrustum() )
				DebugRenderConvexPolyhedron( (Vector4D *)lightState.m_CascadeVolumeCullers[i].GetExclusionFrustumPlanes(), lightState.m_CascadeVolumeCullers[i].GetNumExclusionFrustumPlanes(), Vector( 1, .25f, .25f ), true );
		}

		return;
	}

	for ( uint nCascadeIndex = 0; nCascadeIndex < lightState.m_nShadowCascadeSize; ++nCascadeIndex )
	{
		const CFrustum &shadowFrustum = lightState.m_CascadeFrustums[ nCascadeIndex ];
		//const VMatrix &shadowWorldToView = shadowFrustum.GetView();
		//const VMatrix &shadowViewToProj = shadowFrustum.GetProj();

		VMatrix projToWorld( shadowFrustum.GetInvViewProj() );

		DebugRenderWireframeFrustum3D( projToWorld, g_vCascadeFrustumColors[nCascadeIndex], zTest );
	}

	Add3DLineOverlay( sceneFrustum.GetCameraPosition(), sceneFrustum.GetCameraPosition() + vDirToLight * 250.0f, Vector( 1, 1, 1 ), zTest );
		
	for ( uint nCascadeIndex = 0; nCascadeIndex <= lightState.m_nShadowCascadeSize; ++nCascadeIndex )
	{
		const VMatrix sceneWorldToView( sceneFrustum.GetView() );
		const VMatrix &sceneViewToProj = sceneFrustum.GetProj();

		VMatrix sceneProjToView;
		sceneViewToProj.InverseGeneral( sceneProjToView );

		VMatrix sceneViewToWorld;
		sceneWorldToView.InverseGeneral( sceneViewToWorld );

		Vector v[8];
		for ( uint z = 0; z < 2; ++z )
		{
			const float flZ = z ? 1.0f : 0.0f;

			for ( uint i = 0; i < 4; ++i )
			{
				Vector vViewspacePoint;
				sceneProjToView.V3Mul( Vector( CCSMFrustumDefinition::g_vProjFrustumVerts[i].x, CCSMFrustumDefinition::g_vProjFrustumVerts[i].y, flZ ), vViewspacePoint );

				// Scale the vector so its Z is -1.0f (negative viewspace Z is inside the frustum due to RH proj matrices)
								
				if ( ( z ) && ( nCascadeIndex < lightState.m_nShadowCascadeSize ) )
				{
					float flOneOverZ = 1.0f / vViewspacePoint.z;
					vViewspacePoint *= flOneOverZ;
					vViewspacePoint.z *= -1.0f;

					vViewspacePoint *= lightState.m_DebugInfo[nCascadeIndex].m_flSplitPlaneDistance;
				}
				else if ( ( !z ) && ( nCascadeIndex ) )
				{
					float flOneOverZ = 1.0f / vViewspacePoint.z;
					vViewspacePoint *= flOneOverZ;
					vViewspacePoint.z *= -1.0f;

					vViewspacePoint *= lightState.m_DebugInfo[nCascadeIndex - 1].m_flSplitPlaneDistance;
				}
				else
				{
					// flip xy to be consistent with the other cases
					vViewspacePoint.x *= -1.0f;
					vViewspacePoint.y *= -1.0f;
				}

				sceneViewToWorld.V3Mul( vViewspacePoint, v[i + z * 4] );
			}
		}

		if ( !nCascadeIndex )
		{
			Add3DLineOverlay( v[0], v[1], g_vCascadeFrustumColors[nCascadeIndex], zTest );
			Add3DLineOverlay( v[1], v[2], g_vCascadeFrustumColors[nCascadeIndex], zTest );
			Add3DLineOverlay( v[2], v[3], g_vCascadeFrustumColors[nCascadeIndex], zTest );
			Add3DLineOverlay( v[3], v[0], g_vCascadeFrustumColors[nCascadeIndex], zTest );
		}

		Add3DLineOverlay( v[4], v[5], g_vCascadeFrustumColors[nCascadeIndex], zTest );
		Add3DLineOverlay( v[5], v[6], g_vCascadeFrustumColors[nCascadeIndex], zTest );
		Add3DLineOverlay( v[6], v[7], g_vCascadeFrustumColors[nCascadeIndex], zTest );
		Add3DLineOverlay( v[7], v[4], g_vCascadeFrustumColors[nCascadeIndex], zTest );

		Add3DLineOverlay( v[0], v[4], g_vCascadeFrustumColors[nCascadeIndex], zTest );
		Add3DLineOverlay( v[1], v[5], g_vCascadeFrustumColors[nCascadeIndex], zTest );
		Add3DLineOverlay( v[2], v[6], g_vCascadeFrustumColors[nCascadeIndex], zTest );
		Add3DLineOverlay( v[3], v[7], g_vCascadeFrustumColors[nCascadeIndex], zTest );
	}

	Vector vFadeStart( vEye + vForward * lightState.m_flZLerpStartDist );
	Vector vFadeEnd( vEye + vForward * lightState.m_flZLerpEndDist );
	for ( uint y = 0; y < 8; y++ )
	{
		Add3DLineOverlay( 
			vFadeStart + vLeft * -300.0f + vUp * Lerp( y / 7.0f, -1.0f, 1.0f ) * 300.0f, 
			vFadeStart + vLeft *  300.0f + vUp * Lerp( y / 7.0f, -1.0f, 1.0f ) * 300.0f, 
			Vector( .1f, 0.0f, 1.0f ), true );
	}

	for ( uint y = 0; y < 8; y++ )
	{
		Add3DLineOverlay( 
			vFadeStart + vUp * -300.0f + vLeft * Lerp( y / 7.0f, -1.0f, 1.0f ) * 300.0f, 
			vFadeStart + vUp *  300.0f + vLeft * Lerp( y / 7.0f, -1.0f, 1.0f ) * 300.0f, 
			Vector( .1f, 0.0f, 1.0f ), true );
	}

	for ( uint y = 0; y < 8; y++ )
	{
		Add3DLineOverlay( 
			vFadeEnd + vLeft * -300.0f + vUp * Lerp( y / 7.0f, -1.0f, 1.0f ) * 300.0f, 
			vFadeEnd + vLeft *  300.0f + vUp * Lerp( y / 7.0f, -1.0f, 1.0f ) * 300.0f, 
			Vector( .1f, 0.5f, 1.0f ), true );
	}

	for ( uint y = 0; y < 8; y++ )
	{
		Add3DLineOverlay( 
			vFadeEnd + vUp * -300.0f + vLeft * Lerp( y / 7.0f, -1.0f, 1.0f ) * 300.0f, 
			vFadeEnd + vUp *  300.0f + vLeft * Lerp( y / 7.0f, -1.0f, 1.0f ) * 300.0f, 
			Vector( .1f, 0.5f, 1.0f ), true );
	}

	//DebugRenderWireframeFrustum3D( sceneFrustum.GetInvViewProj(), Vector( .4f, .4f, .8f ), zTest );
}

void CCascadeLightManager::Draw2DDebugInfo()
{
	if ( !IsEnabled() )
		return;

	CFullCSMState &state = m_capturedState.m_CSMParallelSplit.IsValid() ? m_capturedState : m_curState;

	if ( !state.m_CSMParallelSplit.IsValid() )
		return;

	const SunLightState_t &lightState = state.m_CSMParallelSplit.GetLightState();
	const CFrustum *pCascadeFrustums = lightState.m_CascadeFrustums;
	const ShadowFrustaDebugInfo_t *pDebugInfo = lightState.m_DebugInfo;
	//const SunLightShaderParamsCB_t &shadowParams = lightState.m_SunLightShaderParams;
	const VMatrix *pCascadeProjToTexMatrices = lightState.m_CascadeProjToTexMatrices;
	const CFrustum &sceneFrustum = state.m_sceneFrustum;

	const VMatrix sceneWorldToView( sceneFrustum.GetView() );
	const VMatrix &sceneViewToProj = sceneFrustum.GetProj();
	const VMatrix sceneWorldToProj( sceneViewToProj * sceneWorldToView );
	VMatrix sceneProjToWorld;
	sceneWorldToProj.InverseGeneral( sceneProjToWorld );
	
	m_debugPrimRenderer.Clear();

	int nFirstCascadeIndex = 0;
	int nLastCascadeIndex = (int)lightState.m_nShadowCascadeSize - 1;
	
#if defined( _GAMECONSOLE )	
	const int nShadowBufferDebugVisWidth  = 256;
	const int nShadowBufferDebugVisHeight = 256;
#else
	const int nShadowBufferDebugVisWidth  = 512;
	const int nShadowBufferDebugVisHeight = 512;
#endif

	for ( int nCascadeIndex = nFirstCascadeIndex; nCascadeIndex <= nLastCascadeIndex; ++nCascadeIndex )
	{
		const Rect_t &cascadeViewport = lightState.m_CascadeViewports[nCascadeIndex];

		int nPlacementX = nCascadeIndex & 1;
		int nPlacementY = nCascadeIndex >> 1;
		
		Rect_t destRect;

#if defined( _GAMECONSOLE )		
		destRect.x = 64 + ( nShadowBufferDebugVisWidth + 75 ) * nPlacementX;
		destRect.y = 64 + ( nShadowBufferDebugVisHeight + 75 ) * nPlacementY;
#else
		destRect.x = 16 + ( nShadowBufferDebugVisWidth + 75 ) * nPlacementX;
		destRect.y = 16 + ( nShadowBufferDebugVisHeight + 75 ) * nPlacementY;
#endif		
		destRect.width = nShadowBufferDebugVisWidth;
		destRect.height = nShadowBufferDebugVisHeight;

		VertexColor_t color( g_vCascadeFrustumColors[nCascadeIndex].x * 255, g_vCascadeFrustumColors[nCascadeIndex].y * 255, g_vCascadeFrustumColors[nCascadeIndex].z * 255, 255 );

		m_debugPrimRenderer.AddScreenspaceRect2D( destRect.x, destRect.y, destRect.x + destRect.width, destRect.y + destRect.height, color.r, color.g, color.b );

		m_debugPrimRenderer.RenderScreenspaceDepthTexture( 
			destRect.x, destRect.y, destRect.x + destRect.width, destRect.y + destRect.height, 
			(float)cascadeViewport.x / m_nDepthTextureResolution,
			(float)cascadeViewport.y / m_nDepthTextureResolution,
			(float)( cascadeViewport.x + cascadeViewport.width ) / m_nDepthTextureResolution,
			(float)( cascadeViewport.y + cascadeViewport.height ) / m_nDepthTextureResolution,
			m_ShadowDepthTexture, cl_csm_debug_vis_lo_range.GetFloat(), cl_csm_debug_vis_hi_range.GetFloat() );
							
		const CFrustum &shadowFrustum = pCascadeFrustums[ nCascadeIndex ];
		const VMatrix shadowWorldToView( shadowFrustum.GetView() );
		const VMatrix &shadowViewToProj = shadowFrustum.GetProj();

		const VMatrix shadowWorldToProj( shadowViewToProj * shadowWorldToView );

		const VMatrix shadowProjToTex( pCascadeProjToTexMatrices[nCascadeIndex] );

		const VMatrix sceneProjToTex( shadowProjToTex * ( shadowWorldToProj * sceneProjToWorld ) );

		VMatrix shadowTexToRect( 
			destRect.width,     0.0f,					0.0f,   destRect.x,
			0.0f,				destRect.height,		0.0f,   destRect.y,
			0.0f,				0.0f,					0.0f,	.5f,
			0.0f,				0.0f,					0.0f,	1.0f );

		const VMatrix sceneProjToRect( shadowTexToRect * sceneProjToTex );

		m_debugPrimRenderer.AddScreenspaceWireframeFrustum2D( sceneProjToRect, color, true );	

		if ( m_capturedState.m_CSMParallelSplit.IsValid() )
		{
			CFullCSMState &actualState = m_curState;
			const CFrustum &actualSceneFrustum = actualState.m_sceneFrustum;
			
			const VMatrix actualSceneWorldToView( actualSceneFrustum.GetView() );
			const VMatrix &actualSceneViewToProj = actualSceneFrustum.GetProj();

			const VMatrix actualSceneWorldToProj( actualSceneViewToProj * actualSceneWorldToView );
			VMatrix actualSceneProjToWorld;
			actualSceneWorldToProj.InverseGeneral( actualSceneProjToWorld );

			const VMatrix actualSceneProjToTex( shadowProjToTex * ( shadowWorldToProj * actualSceneProjToWorld ) );
			const VMatrix actualSceneProjToRect( shadowTexToRect * actualSceneProjToTex );

			VertexColor_t actualFrustumColor( 0, 255, 0, 255 );
			m_debugPrimRenderer.AddScreenspaceWireframeFrustum2D( actualSceneProjToRect, actualFrustumColor, true );	
		}
		
		VMatrix shadowWorldToRect( shadowTexToRect * ( shadowProjToTex * shadowWorldToProj ) );

		for ( uint i = 0; i < pDebugInfo[nCascadeIndex].m_nNumWorldFocusVerts; ++i )
		{
			const Vector &v = pDebugInfo[nCascadeIndex].m_WorldFocusVerts[i];
			Vector4D worldVert( v.x, v.y, v.z, 1.0f );

			Vector4D rectVert;
			shadowWorldToRect.V4Mul( worldVert, rectVert );

			rectVert *= ( 1.0f / rectVert.w );

			Vector2D points[4];
			points[0].Init( rectVert.x - 5, rectVert.y );
			points[1].Init( rectVert.x + 5, rectVert.y );
			points[2].Init( rectVert.x, rectVert.y - 5 );
			points[3].Init( rectVert.x, rectVert.y + 5);

			m_debugPrimRenderer.AddScreenspaceLineList2D( 2, points, VertexColor_t( 20, 255, 20, 255 ) );
		}

		for ( int nPrevSplitIndex = 0; nPrevSplitIndex < nCascadeIndex; ++nPrevSplitIndex )
		{
			const CFrustum &prevShadowFrustum = pCascadeFrustums[ nPrevSplitIndex ];
			const VMatrix prevShadowWorldToView( prevShadowFrustum.GetView() );
			const VMatrix &prevShadowViewToProj = prevShadowFrustum.GetProj();

			const VMatrix prevShadowWorldToProj( prevShadowViewToProj * prevShadowWorldToView );
			VMatrix prevShadowProjToWorld;
			prevShadowWorldToProj.InverseGeneral( prevShadowProjToWorld	);
			
			const VMatrix prevShadowProjToShadowRect( shadowTexToRect * ( shadowProjToTex * ( shadowWorldToProj * prevShadowProjToWorld ) ) );

			VertexColor_t prevSplitColor( g_vCascadeFrustumColors[nPrevSplitIndex].x * 255, g_vCascadeFrustumColors[nPrevSplitIndex].y * 255, g_vCascadeFrustumColors[nPrevSplitIndex].z * 255, 255 );

			m_debugPrimRenderer.AddScreenspaceWireframeFrustum2D( prevShadowProjToShadowRect, prevSplitColor, false );
		}
	}

	m_debugPrimRenderer.Render2D();
}

void CCascadeLightManager::DrawTextDebugInfo()
{
	CFullCSMState &curState = GetActiveState();

	uint cur_y = 105;

	ScreenText( 5, cur_y, 255, 95, 95, 255, "Active State: %s Pos:(%3.3f, %3.3f, %3.3f), Forward:(%3.3f, %3.3f, %3.3f), Left:(%3.3f, %3.3f, %3.3f), Up:(%3.3f, %3.3f, %3.3f)", 
		m_capturedState.m_CSMParallelSplit.IsValid() ? "CAP" : "CUR", 
		curState.m_sceneFrustum.GetCameraPosition().x, curState.m_sceneFrustum.GetCameraPosition().y, curState.m_sceneFrustum.GetCameraPosition().z,
		curState.m_sceneFrustum.CameraForward().x, curState.m_sceneFrustum.CameraForward().y, curState.m_sceneFrustum.CameraForward().z,
		curState.m_sceneFrustum.CameraLeft().x, curState.m_sceneFrustum.CameraLeft().y, curState.m_sceneFrustum.CameraLeft().z,
		curState.m_sceneFrustum.CameraUp().x, curState.m_sceneFrustum.CameraUp().y, curState.m_sceneFrustum.CameraUp().z );
	cur_y += 2;

	if ( GetClientWorldEntity() )
	{
		const Vector &worldMins = GetClientWorldEntity()->m_WorldMins;
		const Vector &worldMaxs = GetClientWorldEntity()->m_WorldMaxs;
	
		ScreenText( 5, cur_y, 255, 95, 95, 255, "World Bounds: (%f,%f,%f) - (%f,%f,%f)", 
			worldMins.x, worldMins.y, worldMins.z,
			worldMaxs.x, worldMaxs.y, worldMaxs.z );
		cur_y += 2;
	}
	
	uint m_nTotalAABB = 0;
	uint m_nTotalAABBPassed = 0;
	uint m_nTotalCenterHalfDiagonal = 0;
	uint m_nTotalCenterHalfDiagonalPassed = 0;

	uint i;
	for ( i = 0; i < 4; i++ )
	{
		CVolumeCuller &cascadeVolumeCuller = (i == 3) ? m_curViewModelState.m_CSMParallelSplit.GetLightState().m_CascadeVolumeCullers[0] : curState.m_CSMParallelSplit.GetLightState().m_CascadeVolumeCullers[i];
		CVolumeCuller::CullCheckStats_t &cullStats = cascadeVolumeCuller.GetStats();
		
		ScreenText( 5, cur_y, 255, 95, 95, 255, "Cascade %u: Total AABB Cull Checks: %u, Passed: %u, Total World AABB Cull Checks: %u, Passed: %u", 
			i,
			cullStats.m_nTotalAABB, cullStats.m_nTotalAABBPassed,
			cullStats.m_nTotalCenterHalfDiagonal, cullStats.m_nTotalCenterHalfDiagonalPassed );
		cur_y += 2;

		m_nTotalAABB += cullStats.m_nTotalAABB;
		m_nTotalAABBPassed += cullStats.m_nTotalAABBPassed;
		m_nTotalCenterHalfDiagonal += cullStats.m_nTotalCenterHalfDiagonal;
		m_nTotalCenterHalfDiagonalPassed += cullStats.m_nTotalCenterHalfDiagonalPassed;
		
		cascadeVolumeCuller.ClearCullCheckStats();
	}

	ScreenText( 5, cur_y, 255, 95, 95, 255, "All Cascades: Total AABB Cull Checks: %u, Passed: %u, Total World AABB Cull Checks: %u, Passed: %u", 
		m_nTotalAABB, m_nTotalAABBPassed,
		m_nTotalCenterHalfDiagonal, m_nTotalCenterHalfDiagonalPassed );
	cur_y += 2;
}

void CCascadeLightManager::CFullCSMState::Update( const CViewSetup &viewSetup, const Vector &shadowDir, color32 lightColor, int lightColorScale, float flMaxShadowDist, float flMaxVisibleDist, uint nMaxCascadeSize, uint nAtlasFirstCascadeIndex, int nCSMQualityLevel, bool bSetAllCascadesToFirst )
{
	m_shadowDir = shadowDir;
	m_flMaxShadowDist = flMaxShadowDist;
	m_flMaxVisibleDist = flMaxVisibleDist;
	m_nMaxCascadeSize = nMaxCascadeSize;
	
	m_flSceneAspectRatio = viewSetup.ComputeViewMatrices( &m_sceneWorldToView, &m_sceneViewToProj, &m_sceneWorldToProj );

	m_sceneFrustum.BuildFrustumFromParameters( viewSetup.origin, viewSetup.angles, viewSetup.zNear, viewSetup.zFar, viewSetup.fov, m_flSceneAspectRatio, m_sceneWorldToView, m_sceneViewToProj );

	SunLightViewState_t sunLightViewState;
	sunLightViewState.m_Direction = shadowDir;
	sunLightViewState.m_LightColor = lightColor;
	sunLightViewState.m_LightColorScale = lightColorScale;
	sunLightViewState.m_flMaxShadowDist = flMaxShadowDist;
	sunLightViewState.m_flMaxVisibleDist = flMaxVisibleDist;
	sunLightViewState.m_frustum = m_sceneFrustum;
	sunLightViewState.m_nMaxCascadeSize = nMaxCascadeSize;
	sunLightViewState.m_nAtlasFirstCascadeIndex = nAtlasFirstCascadeIndex;
	sunLightViewState.m_nCSMQualityLevel = nCSMQualityLevel;
	sunLightViewState.m_bSetAllCascadesToFirst = bSetAllCascadesToFirst;
	m_CSMParallelSplit.Update( sunLightViewState );
	
	m_bValid = ( m_CSMParallelSplit.GetLightState().m_nShadowCascadeSize != 0 );
}


void CCascadeLightManager::RenderViews( CCascadeLightManager::CFullCSMState &state, bool bIncludeViewModels )
{
	SunLightState_t &lightState = state.m_CSMParallelSplit.GetLightState();
	
#if 0
	VMatrix computedWorldToView;
	VMatrix computedViewToProj;
	VMatrix computedWorldToProj;
#endif

	uint nCascadeIndex = 0;
    if( IsGameConsole() )
	{
		// don't use cascade 0 for console. TODO - at some point we should make better use of texture space, right now we waste 25%
		nCascadeIndex = bIncludeViewModels ? 0 : 1;
	}
	else
	{
		if ( ( !bIncludeViewModels ) && ( GetCSMQualityMode() <= CSMQUALITY_LOW ) )
			nCascadeIndex = 1;
	}

	CMatRenderContextPtr pRenderContext( materials );

	for ( ; nCascadeIndex < lightState.m_nShadowCascadeSize; ++nCascadeIndex )
	{
		CViewSetup shadowView;
		shadowView.m_flAspectRatio = 1.0f;
		shadowView.x = lightState.m_CascadeViewports[nCascadeIndex].x;
		shadowView.y = lightState.m_CascadeViewports[nCascadeIndex].y;
		shadowView.width = lightState.m_CascadeViewports[nCascadeIndex].width;
		shadowView.height = lightState.m_CascadeViewports[nCascadeIndex].height;
		shadowView.m_bRenderToSubrectOfLargerScreen = true;

#if defined(_X360)
		// render into top left viewport
		// resolve to appropriate quadrant of final texture
		shadowView.xCsmDstOffset = shadowView.x;
		shadowView.yCsmDstOffset = shadowView.y;
		shadowView.x = 0;
		shadowView.y = 0;
#endif

		const CFrustum &cascadeFrustum = lightState.m_CascadeFrustums[nCascadeIndex];

		// Force custom matrices, to avoid FP precision issues causing shadow continuity problems. (We need precise control over the matrices used for rendering.)
		if ( cl_csm_use_forced_view_matrices.GetBool() )
		{
			shadowView.m_bCustomViewMatrix = true;
			// Argh - m_matCustomViewMatrix is actually a custom world->camera matrix, not world->view!
			// The view->camera matrix only has 0,-1,1 values so it shouldn't affect precision.
			shadowView.m_matCustomViewMatrix = ( g_matViewToCameraMatrix * VMatrix( cascadeFrustum.GetView() ) ).As3x4();

			shadowView.m_bCustomProjMatrix = true;
			shadowView.m_matCustomProjMatrix = cascadeFrustum.GetProj();
		}

		cascadeFrustum.GetClipSpaceBounds( shadowView.m_OrthoLeft, shadowView.m_OrthoBottom, shadowView.m_OrthoRight, shadowView.m_OrthoTop );
		shadowView.origin = cascadeFrustum.GetCameraPosition();
		shadowView.angles = cascadeFrustum.GetCameraAngles();
		shadowView.zNear = shadowView.zNearViewmodel = cascadeFrustum.GetCameraNearPlane();
		shadowView.zFar = shadowView.zFarViewmodel = cascadeFrustum.GetCameraFarPlane();

		if ( cl_csm_debug_2d.GetBool() && cl_csm_use_forced_view_matrices.GetBool() && cl_csm_hack_proj_matrices_for_cull_debugging.GetBool() ) 
		{
			MatrixBuildOrtho( 
				shadowView.m_matCustomProjMatrix, 
				Lerp( -2.0f, shadowView.m_OrthoLeft, shadowView.m_OrthoRight ),
				Lerp( -2.0f, shadowView.m_OrthoTop, shadowView.m_OrthoBottom ),
				Lerp( -2.0f, shadowView.m_OrthoRight, shadowView.m_OrthoLeft ),
				Lerp( -2.0f, shadowView.m_OrthoBottom, shadowView.m_OrthoTop ),
				shadowView.zNear, 
				shadowView.zFar );
		}

		shadowView.m_bDoBloomAndToneMapping = false;
		shadowView.m_nMotionBlurMode = MOTION_BLUR_DISABLE;

		// Set depth bias factors specific to this cascade
		
#if 0		
		float flShadowSlopeScaleDepthBias = g_pMaterialSystemHardwareConfig->GetShadowSlopeScaleDepthBias();
		float flShadowDepthBias = g_pMaterialSystemHardwareConfig->GetShadowDepthBias();
#else
		static ConVar *s_csm_slopescales[4] = { &cl_csm_slopescaledepthbias_c0, &cl_csm_slopescaledepthbias_c1, &cl_csm_slopescaledepthbias_c2, &cl_csm_slopescaledepthbias_c3 };
		static ConVar *s_csm_depthbias[4] = { &cl_csm_depthbias_c0, &cl_csm_depthbias_c1, &cl_csm_depthbias_c2, &cl_csm_depthbias_c3 };
				
		float flShadowSlopeScaleDepthBias = s_csm_slopescales[nCascadeIndex]->GetFloat();
		float flShadowDepthBias = s_csm_depthbias[nCascadeIndex]->GetFloat();

		if ( bIncludeViewModels )
		{
			flShadowSlopeScaleDepthBias = cl_csm_viewmodel_slopescaledepthbias.GetFloat();
			flShadowDepthBias = cl_csm_viewmodel_depthbias.GetFloat();
		}
#endif

		pRenderContext->PerpareForCascadeDraw( nCascadeIndex, flShadowSlopeScaleDepthBias, flShadowDepthBias );
		pRenderContext->SetShadowDepthBiasFactors( flShadowSlopeScaleDepthBias, flShadowDepthBias );
		pRenderContext->CullMode( MATERIAL_CULLMODE_NONE );

		shadowView.m_bOrtho = true;
		shadowView.m_bCSMView = true;
		
		CVolumeCuller &volumeCuller = lightState.m_CascadeVolumeCullers[nCascadeIndex];
		if ( ( nCascadeIndex == ( lightState.m_nShadowCascadeSize - 1 ) ) && ( GetCSMQualityMode() <= CSMQUALITY_LOW ) )
		{
			volumeCuller.SetCullSmallObjects( true, cl_csm_cull_small_prop_threshold_volume.GetFloat() );
		}
		else
		{
			volumeCuller.SetCullSmallObjects( false, 1e+10f );
		}

		shadowView.m_pCSMVolumeCuller = &volumeCuller;

#if 0
		// Purely for debugging.
		shadowView.m_bCustomViewMatrix = false;
		shadowView.m_bCustomProjMatrix = false;

		shadowView.ComputeViewMatrices( &computedWorldToView, &computedViewToProj, &computedWorldToProj );

		if ( cl_csm_use_forced_view_matrices.GetBool() )
		{
			shadowView.m_bCustomViewMatrix = true;
			shadowView.m_bCustomProjMatrix = true;
		}

		Vector currentViewForward, currentViewRight, currentViewUp;
		AngleVectors( shadowView.angles, &currentViewForward, &currentViewRight, &currentViewUp );

		// Now compute the culling planes the same way as CRender::OrthoExtractFrustumPlanes() does, so they can be manually 
		// compared against the planes the CSM manager computed. The game makes several assumptions about view and ortho projection space.
		VPlane frustumPlanes[6];

		float orgOffset = DotProduct(shadowView.origin, currentViewForward);
		frustumPlanes[FRUSTUM_FARZ].m_Normal = -currentViewForward;
		frustumPlanes[FRUSTUM_FARZ].m_Dist = -shadowView.zFar - orgOffset;

		frustumPlanes[FRUSTUM_NEARZ].m_Normal = currentViewForward;
		frustumPlanes[FRUSTUM_NEARZ].m_Dist = shadowView.zNear + orgOffset;

		orgOffset = DotProduct(shadowView.origin, currentViewRight);
		frustumPlanes[FRUSTUM_LEFT].m_Normal = currentViewRight;
		frustumPlanes[FRUSTUM_LEFT].m_Dist = shadowView.m_OrthoLeft + orgOffset;

		frustumPlanes[FRUSTUM_RIGHT].m_Normal = -currentViewRight;
		frustumPlanes[FRUSTUM_RIGHT].m_Dist = -shadowView.m_OrthoRight - orgOffset;

		orgOffset = DotProduct(shadowView.origin, currentViewUp);
		frustumPlanes[FRUSTUM_TOP].m_Normal = currentViewUp;
		frustumPlanes[FRUSTUM_TOP].m_Dist = shadowView.m_OrthoTop + orgOffset;

		frustumPlanes[FRUSTUM_BOTTOM].m_Normal = -currentViewUp;
		frustumPlanes[FRUSTUM_BOTTOM].m_Dist = -shadowView.m_OrthoBottom - orgOffset;
#endif
		
		// Render to the shadow depth texture with appropriate view
		
		view->UpdateShadowDepthTexture( m_DummyColorTexture, m_ShadowDepthTexture, shadowView, true, bIncludeViewModels );
	}

	pRenderContext->CullMode( MATERIAL_CULLMODE_CCW );
}

void CCascadeLightManager::PreRender()
{
}

Vector CCascadeLightManager::GetShadowDirection()
{
	Vector vShadowDir( C_CascadeLight::Get()->GetShadowDirection() );
	if ( ( cl_csm_use_env_light_direction.GetBool() ) && ( C_CascadeLight::Get()->UseLightEnvAngles() ) && ( C_CascadeLight::Get()->GetEnvLightShadowDirection().Length() > .5f ) )
	{
		vShadowDir = C_CascadeLight::Get()->GetEnvLightShadowDirection();
	}

	if ( cl_csm_rot_override.GetBool() ) 
	{
		const float flRotDegPerSec = 20.0f;

		float flRotX = cl_csm_rot_x.GetFloat(), flRotY = cl_csm_rot_y.GetFloat();
		if ( m_flRotX[0] || m_flRotX[1] )
		{
			flRotX += m_flRotX[0] * gpGlobals->frametime * flRotDegPerSec;
			flRotX += m_flRotX[1] * gpGlobals->frametime * flRotDegPerSec;
			if ( flRotX > 360.0f )
				flRotX -= 360.0f;
			else if ( flRotX < 0.0f )
				flRotX += 360.0f;
			cl_csm_rot_x.SetValue( flRotX );
		}

		if ( m_flRotY[0] || m_flRotY[1] ) 
		{
			flRotY += m_flRotY[0] * gpGlobals->frametime * flRotDegPerSec;
			flRotY += m_flRotY[1] * gpGlobals->frametime * flRotDegPerSec;
			if ( flRotY > 360.0f )
				flRotY -= 360.0f;
			else if ( flRotY < 0.0f )
				flRotY += 360.0f;
			cl_csm_rot_y.SetValue( flRotY );
		}

		QAngle angles;
		angles.Init( flRotX, flRotY, cl_csm_rot_z.GetFloat() );
		AngleVectors( angles, &vShadowDir );
	}
	
	return vShadowDir.Normalized();
}

// bSetup only used on PS3 right now - to support 2 pass Build/Draw rendering 
// bSetup indicates whether to execute once per frame setup code, do this in the buildlist (1st pass) pass only
// Needed since this will get called twice on PS3 if 2 pass drawing is on
// bSetup = true otherwise
void CCascadeLightManager::ComputeShadowDepthTextures( const CViewSetup &viewSetup, bool bSetup )
{
	m_bStateIsValid = false;
	m_bCSMIsActive = false;
	
	if (
#ifdef OSX
		!OSX_HardwareGoodEnoughForCSMs() ||
#endif
		!cl_csm_enabled.GetBool() ||
		!g_pMaterialSystemHardwareConfig->SupportsCascadedShadowMapping() ||
		!g_pMaterialSystemHardwareConfig->SupportsShadowDepthTextures()
		)
	{
		cl_csm_enabled.SetValue( 0 );

		if ( m_bRenderTargetsAllocated )
		{
			// This code path will only execute if CSM initializes successfully at startup, but is then disabled either from the console or a config/device change.
			materials->ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly();
			DeinitRenderTargets();
			materials->FinishRenderTargetAllocation();
		}
		
		return;
	}

	if ( !m_bRenderTargetsAllocated )
	{
		// If shadow depth texture is now needed but wasn't allocated, allocate it. 
		// (This is not a normal code path - it's only taken if the user turns on CSM from the console.)
		materials->ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly();
		InitRenderTargets();
		materials->FinishRenderTargetAllocation();
	}
	else if ( m_nCurRenderTargetQualityMode != GetCSMQualityMode() )
	{
		DeinitRenderTargets();
		
		// Quality level has changed - recreate render targets as they may change size.
		materials->ReEnableRenderTargetAllocation_IRealizeIfICallThisAllTexturesWillBeUnloadedAndLoadTimeWillSufferHorribly();
		InitRenderTargets();
		materials->FinishRenderTargetAllocation();
	}
	
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	if ( !m_bRenderTargetsAllocated || !C_CascadeLight::Get() )
	{
		m_curState.Reset();
		m_curViewModelState.Reset();
	}
	else
	{
		Vector vShadowDir( GetShadowDirection() );

		color32 lightColor = C_CascadeLight::Get()->GetColor();
		int lightColorScale = C_CascadeLight::Get()->GetColorScale();
		
		float flMaxShadowDist = cl_csm_max_shadow_dist.GetFloat();
		if ( flMaxShadowDist <= 0.0f )
		{
			flMaxShadowDist = C_CascadeLight::Get()->GetMaxShadowDist();
#ifdef OSX
			if ( GetCSMQualityMode() == CSMQUALITY_HIGH )
			{
				// At the highest CSM quality level boost the max shadow distance (match Windows on high end Macs)
				// This seems OK from a CS fairness perspective (it can be argued either way whether this gives a player an advantage, or disadvantage).
				flMaxShadowDist *= 1.4f;
			}
            else if ( GetCSMQualityMode() == CSMQUALITY_LOW )
            {
                // match PS3 distance for lowest performing Macs
				flMaxShadowDist *= 0.8f;
            }
            else if ( GetCSMQualityMode() == CSMQUALITY_VERY_LOW )
            {
                // match PS3 distance for lowest performing Macs
				flMaxShadowDist *= 0.6f;
            }
#else
			if ( ( !IsGameConsole() ) &&
                 ( GetCSMQualityMode() == CSMQUALITY_HIGH ) )
			{
				// At the highest CSM quality level boost the max shadow distance.
				// This seems OK from a CS fairness perspective (it can be argued either way whether this gives a player an advantage, or disadvantage).
				flMaxShadowDist *= 1.4f;
			}
#endif
		}
		if ( flMaxShadowDist <= 0.0f )
			flMaxShadowDist = 400.0f;
				
		flMaxShadowDist = clamp<float>( flMaxShadowDist, 1.0f, 10000.0f );
		m_curState.Update( viewSetup, vShadowDir, lightColor, lightColorScale, flMaxShadowDist, cl_csm_max_visible_dist.GetFloat(), MAX_CSM_CASCADES, 0, GetCSMQualityMode(), false );
		
		CViewSetup viewModelViewSetup( viewSetup );
		viewModelViewSetup.zNear = .5f;
		viewModelViewSetup.zFar = cl_csm_viewmodel_farz.GetFloat();
		m_curViewModelState.Update( viewModelViewSetup, vShadowDir, lightColor, lightColorScale, cl_csm_viewmodel_max_shadow_dist.GetFloat(), cl_csm_viewmodel_max_visible_dist.GetFloat(), 1, 3, GetCSMQualityMode(), true );
							
		if ( cl_csm_capture_state.GetInt() )
		{
			cl_csm_capture_state.SetValue( 0 );
			m_capturedState = m_curState;
		}

		if ( cl_csm_clear_captured_state.GetInt() )
		{
			cl_csm_clear_captured_state.SetValue( 0 );
			m_capturedState.Clear();
		}

		// The CSM sampling shader can only handle MAX_CSM_CASCADES cascades to reduce the number of dynamic combos.
		if ( GetActiveState().IsValid() && ( GetActiveState().m_CSMParallelSplit.GetLightState().m_nShadowCascadeSize == MAX_CSM_CASCADES ) )
		{
			pRenderContext->BeginGeneratingCSMs();

			RenderViews( GetActiveState(), false );
			
			bool bRenderViewModelCascade = cl_csm_viewmodel_shadows.GetBool();
			if ( ( !IsGameConsole() ) &&
				 ( GetCSMQualityMode() == CSMQUALITY_VERY_LOW ) )
			{
				bRenderViewModelCascade = false;
			}
							
			if ( bRenderViewModelCascade )
			{
				RenderViews( m_curViewModelState, true );
			}

			pRenderContext->EndGeneratingCSMs();
			m_bCSMIsActive = true;
		}
	}
				
	pRenderContext->SetCascadedShadowMapping( m_bCSMIsActive );
	if ( m_bCSMIsActive )
	{
		pRenderContext->SetCascadedShadowMappingState( GetActiveState().m_CSMParallelSplit.GetLightState().m_SunLightShaderParams, m_ShadowDepthTexture );
				
		if ( cl_csm_debug_2d.GetBool() )
		{
			// Draw the text here so it doesn't lag 1 frame behind the other debug info
			DrawTextDebugInfo();
		}
	}

	m_bStateIsValid = m_bCSMIsActive;
}

void CCascadeLightManager::UnlockAllShadowDepthTextures()
{
	if ( m_bCSMIsActive )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		pRenderContext->SetCascadedShadowMapping( false );

		m_bCSMIsActive = false;
	}
}

void CCascadeLightManager::BeginViewModelRendering()
{
	if ( !IsEnabledAndActive() || !m_bStateIsValid || !cl_csm_viewmodel_shadows.GetBool() )
		return;

	if ( ( !IsGameConsole() ) &&
		 ( GetCSMQualityMode() == CSMQUALITY_VERY_LOW ) )
		return;
			
	m_bCSMIsActive = true;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->SetCascadedShadowMapping( true );

	CascadedShadowMappingState_t &viewModelShaderParams = m_curViewModelState.m_CSMParallelSplit.GetLightState().m_SunLightShaderParams;
	viewModelShaderParams.m_bIsRenderingViewModels = true;

	pRenderContext->SetCascadedShadowMappingState( viewModelShaderParams, m_ShadowDepthTexture );
}

void CCascadeLightManager::EndViewModelRendering()
{
	if ( !IsEnabledAndActive() || !m_bStateIsValid || !cl_csm_viewmodel_shadows.GetBool() )
		return;

	if ( ( !IsGameConsole() ) &&
		 ( GetCSMQualityMode() == CSMQUALITY_VERY_LOW ) )
		return;

	if ( m_bCSMIsActive )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		pRenderContext->SetCascadedShadowMapping( false );

		m_bCSMIsActive = false;

		// 7LS - poke is view model rendering, testing, should move to somewhere more sensible
		m_curViewModelState.m_CSMParallelSplit.GetLightState().m_SunLightShaderParams.m_bIsRenderingViewModels = false;
	}
}

void CCascadeLightManager::BeginReflectionView()
{
	if ( !m_bCSMIsActive )
		return;
	if ( ( GetCSMQualityMode() > CSMQUALITY_LOW ) && !cl_csm_force_no_csm_in_reflections.GetBool() )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->SetCascadedShadowMapping( false );
}

void CCascadeLightManager::EndReflectionView()
{
	if ( !m_bCSMIsActive )
		return;
	if ( ( GetCSMQualityMode() > CSMQUALITY_LOW ) && !cl_csm_force_no_csm_in_reflections.GetBool() )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->SetCascadedShadowMapping( true );
}

void CCascadeLightManager::DumpStatus()
{
	Msg( "CSM enabled: %i\n", cl_csm_enabled.GetBool() );
	Msg( "Render targets allocated: %i\n", m_bRenderTargetsAllocated );
	Msg( "Depth texture resolution: %i\n", m_nDepthTextureResolution );
	Msg( "Hardware config supportsCascadedShadowMapping: %i\n", g_pMaterialSystemHardwareConfig->SupportsCascadedShadowMapping() );
	Msg( "Hardware config supportsShadowDepthTextures: %i\n", g_pMaterialSystemHardwareConfig->SupportsShadowDepthTextures() );
	Msg( "Hardware config SupportsBilinearPCFSampling: %i\n", g_pMaterialSystemHardwareConfig->SupportsBilinearPCFSampling() );
	Msg( "Current actual CSM quality level (%i=highest, will be forced to 0 if HW doesn't support bilinear PCF): %i\n", CSMQUALITY_TOTAL_MODES - 1, GetCSMQualityMode() );
	Msg( "env_cascade_light entity exists: %i\n", C_CascadeLight::Get() != NULL );
	if ( C_CascadeLight::Get() )
	{
		Msg( "env_cascade_light values:\n" );
		Msg( "Shadow direction: %f %f %f\n", C_CascadeLight::Get()->GetShadowDirection().x, C_CascadeLight::Get()->GetShadowDirection().y, C_CascadeLight::Get()->GetShadowDirection().z );
		Msg( "Light env shadow direction: %f %f %f\n", C_CascadeLight::Get()->GetEnvLightShadowDirection().x, C_CascadeLight::Get()->GetEnvLightShadowDirection().y, C_CascadeLight::Get()->GetEnvLightShadowDirection().z );
		Msg( "Use light env shadow angles: %u\n", C_CascadeLight::Get()->UseLightEnvAngles() );
		Msg( "Max shadow dist: %f\n", C_CascadeLight::Get()->GetMaxShadowDist() );
	}
}

void CC_CSM_Status( const CCommand& args )
{
	g_CascadeLightManager.DumpStatus();
}

CSMQualityMode_t CCascadeLightManager::GetCSMQualityMode()
{
	if ( IsGameConsole() )
		return CSMQUALITY_VERY_LOW;

	if ( !g_pMaterialSystemHardwareConfig->SupportsBilinearPCFSampling() )
	{
		// DX9-class ATI cards: always using VERY_LOW, because the PCF sampling shader has hardcoded knowledge of the shadow depth texture's res
		return CSMQUALITY_VERY_LOW;
	}
		
	return materials->GetCurrentConfigForVideoCard().GetCSMQualityMode();
}
