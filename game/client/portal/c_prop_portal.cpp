//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "c_prop_portal.h"
#include "portal_shareddefs.h"
#include "clientsideeffects.h"
#include "tier0/vprof.h"
#include "materialsystem/ITexture.h"
#include "hud_macros.h"
#include "IGameSystem.h"
#include "game_timescale_shared.h"
#include "view.h"						// For MainViewOrigin()
#include "clientleafsystem.h"			// For finding the leaves our portals are in
#include "portal_render_targets.h"		// Access to static references to Portal-specific render textures
#include "toolframework/itoolframework.h"
#include "toolframework_client.h"
#include "tier1/keyvalues.h"
#include "rendertexture.h"
#include "prop_portal_shared.h"
#include "particles_new.h"
#include "materialsystem/imaterialvar.h"
#include "portal_mp_gamerules.h"
#include "c_weapon_portalgun.h"
#include "prediction.h"
#include "particle_parse.h"
#include "c_user_message_register.h"
#include "c_world.h"

#include "C_Portal_Player.h"

#include "c_pixel_visibility.h"

#include "glow_overlay.h"

#include "dlight.h"
#include "iefx.h"

#include "simple_keys.h"

#include "c_baseprojectedentity.h"
#include "view_scene.h"

#ifdef _DEBUG
#include "filesystem.h"
#endif

#include "debugoverlay_shared.h"

IMPLEMENT_CLIENTCLASS_DT( C_Prop_Portal, DT_Prop_Portal, CProp_Portal )
	RecvPropEHandle( RECVINFO( m_hFiredByPlayer ) ),
	RecvPropInt( RECVINFO( m_nPlacementAttemptParity ) ),
END_RECV_TABLE()

LINK_ENTITY_TO_CLASS( prop_portal, C_Prop_Portal );

BEGIN_PREDICTION_DATA( C_Prop_Portal )
	DEFINE_PRED_FIELD( m_nPlacementAttemptParity, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

bool g_bShowGhostedPortals = false;

PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheBasicPropPortalDrawingMaterials )
#if !defined( _GAMECONSOLE ) //XBox 360 is guaranteed to use stencil mode, and therefore doesn't need texture mode materials
PRECACHE( MATERIAL, "models/portals/portal_1_dynamicmesh" )
PRECACHE( MATERIAL, "models/portals/portal_2_dynamicmesh" )
PRECACHE( MATERIAL, "models/portals/portal_1_renderfix_dynamicmesh" )
PRECACHE( MATERIAL, "models/portals/portal_2_renderfix_dynamicmesh" )
#endif
PRECACHE( MATERIAL, "models/portals/portal_depthdoubler" )
PRECACHE( MATERIAL, "models/portals/portalstaticoverlay_1" )
PRECACHE( MATERIAL, "models/portals/portalstaticoverlay_2" )
PRECACHE( MATERIAL, "models/portals/portalstaticoverlay_tinted" )
PRECACHE( MATERIAL, "models/portals/portal_stencil_hole" )
PRECACHE( MATERIAL, "models/portals/portal_refract_1" )
//PRECACHE( MATERIAL, "models/portals/portal_refract_2" )
PRECACHE( MATERIAL, "models/portals/portalstaticoverlay_1_noz" )
PRECACHE( MATERIAL, "models/portals/portalstaticoverlay_2_noz" )
PRECACHE( MATERIAL, "models/portals/portalstaticoverlay_noz" )
//PRECACHE( MATERIAL, "effects/flashlight001" ) //light transfers disabled indefinitely
PRECACHE_REGISTER_END()

class CAutoInitBasicPropPortalDrawingMaterials : public CAutoGameSystem
{
public:
	PropPortalRenderingMaterials_t m_Materials;
	void LevelInitPreEntity()
	{
		m_Materials.m_PortalMaterials[0].Init( "models/portals/portal_1_dynamicmesh", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_PortalMaterials[1].Init( "models/portals/portal_2_dynamicmesh", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_PortalRenderFixMaterials[0].Init( "models/portals/portal_1_renderfix_dynamicmesh", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_PortalRenderFixMaterials[1].Init( "models/portals/portal_2_renderfix_dynamicmesh", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_PortalDepthDoubler.Init( "models/portals/portal_depthdoubler", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_PortalStaticOverlay[0].Init( "models/portals/portalstaticoverlay_1", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_PortalStaticOverlay[1].Init( "models/portals/portalstaticoverlay_2", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_PortalStaticOverlay_Tinted.Init( "models/portals/portalstaticoverlay_tinted", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_PortalStaticGhostedOverlay[0].Init( "models/portals/portalstaticoverlay_1_noz", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_PortalStaticGhostedOverlay[1].Init( "models/portals/portalstaticoverlay_2_noz", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_PortalStaticGhostedOverlay_Tinted.Init( "models/portals/portalstaticoverlay_noz", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_Portal_Stencil_Hole.Init( "models/portals/portal_stencil_hole", TEXTURE_GROUP_CLIENT_EFFECTS );
		m_Materials.m_Portal_Refract.Init( "models/portals/portal_refract_1", TEXTURE_GROUP_CLIENT_EFFECTS );

		m_Materials.m_nDepthDoubleViewMatrixVarCache = 0;
		m_Materials.m_PortalDepthDoubler->FindVarFast( "$alternateviewmatrix", &m_Materials.m_nDepthDoubleViewMatrixVarCache ); // Warm cache

		m_Materials.m_nStaticOverlayTintedColorGradientLightVarCache = 0;
		m_Materials.m_PortalStaticOverlay_Tinted->FindVarFast( "$PortalColorGradientLight", &m_Materials.m_nStaticOverlayTintedColorGradientLightVarCache ); // Warm cache

		IMaterialVar *pPortalCoopColorPlayerOnePortalOne = m_Materials.m_PortalStaticOverlay_Tinted->FindVar( "$PortalCoopColorPlayerOnePortalOne", NULL, false );
		if ( pPortalCoopColorPlayerOnePortalOne )
		{
			pPortalCoopColorPlayerOnePortalOne->GetVecValue( &( m_Materials.m_coopPlayerPortalColors[0][0].x ), 3 );
		}

		IMaterialVar *pPortalCoopColorPlayerOnePortalTwo = m_Materials.m_PortalStaticOverlay_Tinted->FindVar( "$PortalCoopColorPlayerOnePortalTwo", NULL, false );
		if ( pPortalCoopColorPlayerOnePortalTwo )
		{
			pPortalCoopColorPlayerOnePortalTwo->GetVecValue( &( m_Materials.m_coopPlayerPortalColors[0][1].x ), 3 );
		}

		IMaterialVar *pPortalCoopColorPlayerTwoPortalOne = m_Materials.m_PortalStaticOverlay_Tinted->FindVar( "$PortalCoopColorPlayerTwoPortalOne", NULL, false );
		if ( pPortalCoopColorPlayerTwoPortalOne )
		{
			pPortalCoopColorPlayerTwoPortalOne->GetVecValue( &( m_Materials.m_coopPlayerPortalColors[1][0].x ), 3 );
		}

		IMaterialVar *pPortalCoopColorPlayerTwoPortalTwo = m_Materials.m_PortalStaticOverlay_Tinted->FindVar( "$PortalCoopColorPlayerTwoPortalTwo", NULL, false );
		if ( pPortalCoopColorPlayerTwoPortalTwo )
		{
			pPortalCoopColorPlayerTwoPortalTwo->GetVecValue( &( m_Materials.m_coopPlayerPortalColors[1][1].x ), 3 );
		}

		m_Materials.m_singlePlayerPortalColors[0] = Vector( 64.0f/255.0f, 160.0f/255.0f, 1.0f );
		m_Materials.m_singlePlayerPortalColors[1] = Vector( 1.0f, 160.0f/255.0f, 32.0f/255.0f );
	}
};
static CAutoInitBasicPropPortalDrawingMaterials s_FlatBasicPortalDrawingMaterials;

PropPortalRenderingMaterials_t& C_Prop_Portal::m_Materials = s_FlatBasicPortalDrawingMaterials.m_Materials;

// Throttle portal "ghosting" rendering
ConVar portal_draw_ghosting( "portal_draw_ghosting", "1", FCVAR_NONE );

// Determines if portals should cast light through themselves when 
ConVar portal_transmit_light( "portal_transmit_light", "0", FCVAR_CHEAT );
extern ConVar use_server_portal_particles;

void __MsgFunc_PortalFX_Surface(bf_read &msg)
{
	int iPortalEnt = msg.ReadShort();		
	C_Prop_Portal *pPortal= dynamic_cast<C_Prop_Portal*>( ClientEntityList().GetEnt( iPortalEnt ) );

	if( !pPortal )
	{
		Warning("!!! Failed to find portal %d !!!\n", iPortalEnt );
		return;
	}

	int iOwnerEnt = msg.ReadShort();		
	C_BaseEntity *pOwner = ClientEntityList().GetEnt( iOwnerEnt );

	int nTeam = msg.ReadByte();
	int nPortalNum = msg.ReadByte();
	int nEffect = msg.ReadByte();

	Vector vecOrigin;
	msg.ReadBitVec3Coord( vecOrigin );

	QAngle qAngle;
	msg.ReadBitAngles( qAngle );

	pPortal->CreateFizzleEffect( pOwner, nEffect, vecOrigin, qAngle, nTeam, nPortalNum );
}

USER_MESSAGE_REGISTER( PortalFX_Surface );

C_Prop_Portal::C_Prop_Portal( void )
:	m_fStaticAmount( 0.0f ),
	m_fSecondaryStaticAmount( 0.0f ),
	m_fOpenAmount( 0.0f )
{
	if( !ms_DefaultPortalSizeInitialized )
	{
		ms_DefaultPortalSizeInitialized = true; // for CEG protection

		CEG_GCV_PRE();
		ms_DefaultPortalHalfHeight = CEG_GET_CONSTANT_VALUE( DefaultPortalHalfHeight ); // only protecting one to reduce the cost of first-portal check
		CEG_GCV_POST();
	}
	m_bIsPropPortal = true;	// Member of CPortalRenderable
	TransformedLighting.m_LightShadowHandle = CLIENTSHADOW_INVALID_HANDLE;
	CProp_Portal_Shared::AllPortals.AddToTail( this );
	SetPredictionEligible( true );
}

C_Prop_Portal::~C_Prop_Portal( void )
{
	// Shut down our effect if we have it
	DestroyAttachedParticles();

	CProp_Portal_Shared::AllPortals.FindAndRemove( this );
}

void C_Prop_Portal::Spawn( void )
{
	SetThink( &C_Prop_Portal::ClientThink );
	SetNextClientThink( CLIENT_THINK_ALWAYS );

	m_matrixThisToLinked.Identity(); //don't accidentally teleport objects to zero space
	m_hEffect = NULL;
	BaseClass::Spawn();
}

void C_Prop_Portal::Activate( void )
{
	BaseClass::Activate();
}

void C_Prop_Portal::ClientThink( void )
{
	bool bDidAnything = false;
	if( m_fStaticAmount > 0.0f )
	{
		m_fStaticAmount -= gpGlobals->frametime;
		if( m_fStaticAmount < 0.0f ) 
			m_fStaticAmount = 0.0f;

		bDidAnything = true;
	}
	if( m_fSecondaryStaticAmount > 0.0f )
	{
		m_fSecondaryStaticAmount -= gpGlobals->frametime;
		if( m_fSecondaryStaticAmount < 0.0f ) 
			m_fSecondaryStaticAmount = 0.0f;

		bDidAnything = true;
	}

	if( m_fOpenAmount < 1.0f )
	{
		float flSlowdown = GameTimescale()->GetCurrentTimescale();
		m_fOpenAmount += ( gpGlobals->frametime * ( 2.0f / flSlowdown ) );
		if( m_fOpenAmount > 1.0f ) 
			m_fOpenAmount = 1.0f;

		bDidAnything = true;
	}

	// Speculative workaround for multiplayer bug where 
	// active and linked portals have full static.
	// No known repro, so as a hammer just think all the time.
	// The above logic is pretty cheap, so this shouldn't be a big deal.
	// Also, there really isn't any more than 4 of these entities at one time, 
	// so the upper bound is pretty small.
#if 0
	if( bDidAnything == false )
	{
		SetNextClientThink( CLIENT_THINK_NEVER );
	}
#endif
}


void C_Prop_Portal::UpdateOnRemove( void )
{
	/*
	if( TransformedLighting.m_LightShadowHandle != CLIENTSHADOW_INVALID_HANDLE )
	{
		g_pClientShadowMgr->DestroyFlashlight( TransformedLighting.m_LightShadowHandle );
		TransformedLighting.m_LightShadowHandle = CLIENTSHADOW_INVALID_HANDLE;
	}
	*/

	// Kill any dlight we may have
	if( TransformedLighting.m_pEntityLight )
	{
		TransformedLighting.m_pEntityLight->die = gpGlobals->curtime;
		TransformedLighting.m_pEntityLight = NULL;
	}

	// Shut down our effect if we have it
	DestroyAttachedParticles();

	BaseClass::UpdateOnRemove();
}

void C_Prop_Portal::OnRestore( void )
{
	BaseClass::OnRestore();

	if ( !m_hEffect && !m_hEffect.IsValid() && IsActive() )
	{
		CreateAttachedParticles();
	}
}

void C_Prop_Portal::OnNewParticleEffect( const char *pszParticleName, CNewParticleEffect *pNewParticleEffect )
{
	if ( Q_stricmp( pszParticleName, "portal_1_overlap" ) == 0 || Q_stricmp( pszParticleName, "portal_2_overlap" ) == 0 )
	{
		float fClosestDistanceSqr = -1.0f;
		Vector vClosestPosition;

		int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
		if( iPortalCount != 0 )
		{
			CProp_Portal **pPortals = CProp_Portal_Shared::AllPortals.Base();
			for( int i = 0; i != iPortalCount; ++i )
			{
				CProp_Portal *pTempPortal = pPortals[i];
				if ( pTempPortal != this && pTempPortal->IsActive() )
				{
					Vector vPosition = pTempPortal->GetAbsOrigin();

					float fDistanceSqr = pNewParticleEffect->GetRenderOrigin().DistToSqr( vPosition );

					if ( fClosestDistanceSqr == -1.0f || fClosestDistanceSqr > fDistanceSqr )
					{
						fClosestDistanceSqr = fDistanceSqr;
						vClosestPosition = vPosition;
					}
				}
			}
		}

		if ( fClosestDistanceSqr != -1.0f )
		{
			pNewParticleEffect->SetControlPoint( 1, vClosestPosition );
		}
	}
}

void C_Prop_Portal::CreateFizzleEffect( C_BaseEntity *pOwner, int iEffect, Vector vecOrigin, QAngle qAngles, int nTeam, int nPortalNum )
{
	Color color = UTIL_Portal_Color_Particles( nPortalNum, nTeam );

	Vector vColor;
	vColor.x = color.r();
	vColor.y = color.g();
	vColor.z = color.b();

	CUtlReference<CNewParticleEffect> pEffect;
	if ( !pOwner )
		return;

	bool bCreated = false;

	switch ( iEffect )
	{
	case PORTAL_FIZZLE_SUCCESS:
		{
			pEffect = CNewParticleEffect::CreateOrAggregate( NULL, "portal_success", vecOrigin, NULL );
			bCreated = true;
		}
		break;

	case PORTAL_FIZZLE_BAD_SURFACE:
		{
			pEffect = CNewParticleEffect::CreateOrAggregate( NULL, "portal_badsurface", vecOrigin, NULL );
		}
		break;

	case PORTAL_FIZZLE_CLOSE:
		{
			pEffect = CNewParticleEffect::CreateOrAggregate( NULL, "portal_close", vecOrigin, NULL );
		}
		break;
	}

	if ( pEffect )
	{
		pEffect->SetControlPoint( 0, vecOrigin );

		Vector vecForward, vecRight, vecUp;
		AngleVectors( qAngles, &vecForward, &vecRight, &vecUp );
		pEffect->SetControlPointOrientation( 0, vecForward, vecRight, vecUp );

		pEffect->SetControlPoint( 2, vColor );
	}

	if ( bCreated )
	{
		Color rgbaColor = Color( vColor.x, vColor.y, vColor.z, 255 );
		CreateAttachedParticles( &rgbaColor );
	}
}


void C_Prop_Portal::OnPortalMoved( void )
{
	if( IsActive() )
	{
		if( !IsMobile() )
		{
			if( !GetPredictable() || (prediction->InPrediction() && prediction->IsFirstTimePredicted()) )
			{
				m_fOpenAmount = 0.0f;
				SetNextClientThink( CLIENT_THINK_ALWAYS ); //we need this to help open up
				
				C_Prop_Portal *pRemote = (C_Prop_Portal *)m_hLinkedPortal.Get();
				//add static to the remote
				if( pRemote )
				{
					pRemote->m_fStaticAmount = 1.0f; // This will cause the other portal to show the static effect
					pRemote->SetNextClientThink( CLIENT_THINK_ALWAYS );
				}
			}
		}

		UpdateTransformedLighting();

		C_BaseProjectedEntity::TestAllForProjectionChanges();
	}

	BaseClass::OnPortalMoved();
}

void C_Prop_Portal::OnActiveStateChanged( void )
{
	if( IsActive() )
	{
		// UpdateTransformedLighting();
		if( !GetPredictable() || (prediction->InPrediction() && prediction->IsFirstTimePredicted()) )
		{
			m_fOpenAmount = 0.0f;
			m_fStaticAmount = 1.0f;
			SetNextClientThink( CLIENT_THINK_ALWAYS ); //we need this to help open up

			C_Prop_Portal *pRemote = (C_Prop_Portal *)m_hLinkedPortal.Get();
			//add static to the remote
			if( pRemote )
			{
				pRemote->m_fStaticAmount = 1.0f; // This will cause the other portal to show the static effect
				pRemote->SetNextClientThink( CLIENT_THINK_ALWAYS );
			}
		}
	}
	else
	{
		if( TransformedLighting.m_pEntityLight )
		{
			TransformedLighting.m_pEntityLight->die = gpGlobals->curtime;
			TransformedLighting.m_pEntityLight = NULL;
		}

		// the portal closed
		DestroyAttachedParticles();
		DoFizzleEffect( PORTAL_FIZZLE_CLOSE, false );

		/*
		if( TransformedLighting.m_LightShadowHandle != CLIENTSHADOW_INVALID_HANDLE )
		{
			g_pClientShadowMgr->DestroyFlashlight( TransformedLighting.m_LightShadowHandle );
			TransformedLighting.m_LightShadowHandle = CLIENTSHADOW_INVALID_HANDLE;
		}
		*/
	}

	BaseClass::OnActiveStateChanged();
}


void C_Prop_Portal::OnLinkageChanged( C_Portal_Base2D *pOldLinkage )
{
	if ( IsActive() )
	{
		CreateAttachedParticles();
	}

	if( m_hLinkedPortal.Get() != NULL )
	{
		UpdateTransformedLighting();
	}
	/*
	else
	{
		if( TransformedLighting.m_pEntityLight )
		{
			TransformedLighting.m_pEntityLight->die = gpGlobals->curtime;
			TransformedLighting.m_pEntityLight = NULL;
		}

		if( TransformedLighting.m_LightShadowHandle != CLIENTSHADOW_INVALID_HANDLE )
		{
			g_pClientShadowMgr->DestroyFlashlight( TransformedLighting.m_LightShadowHandle );
			TransformedLighting.m_LightShadowHandle = CLIENTSHADOW_INVALID_HANDLE;
		}
	}
	*/
}

void C_Prop_Portal::CreateAttachedParticles( Color *pColors /*= NULL*/ )
{
	DestroyAttachedParticles();

	if ( !GameRules()->IsMultiplayer() && use_server_portal_particles.GetBool() )
		return;

	C_BaseAnimating::PushAllowBoneAccess( true, false, "portalparticles" );

	// create a new effect for this portal
	mdlcache->BeginLock();
	m_hEffect = ParticleProp()->Create( m_bIsPortal2 ? "portal_edge_reverse" : "portal_edge", PATTACH_POINT_FOLLOW, "particles" );
	mdlcache->EndLock();
	if ( m_hEffect.IsValid() )
	{
		Color clrPortal;

		if ( !pColors )
		{
			int nTeam = GetTeamNumber();
			C_BasePlayer *pPlayer = GetPredictionOwner();
			if ( pPlayer )
			{
				nTeam = pPlayer->GetTeamNumber();
			}

			clrPortal = UTIL_Portal_Color_Particles( (m_bIsPortal2)?(2):(1), nTeam );
			pColors = &clrPortal;
		}

		const Vector vecPortalColor( ((float)pColors->r()), 
									 ((float)pColors->g()), 
									 ((float)pColors->b()) );
		m_hEffect->SetControlPoint( 7, vecPortalColor );
	}

	C_BaseAnimating::PopBoneAccess( "portalparticles" );
}

void C_Prop_Portal::DestroyAttachedParticles( void )
{
	// If there's already a different stream particle effect, get rid of it.
	// Shut down our effect if we have it
	if ( m_hEffect && m_hEffect.IsValid() )
	{
		ParticleProp()->StopEmission( m_hEffect, false, true, false, true );
		m_hEffect = NULL;
	}
}

//ConVar r_portal_light_innerangle( "r_portal_light_innerangle", "90.0", FCVAR_CLIENTDLL );
//ConVar r_portal_light_outerangle( "r_portal_light_outerangle", "90.0", FCVAR_CLIENTDLL );
//ConVar r_portal_light_forward( "r_portal_light_forward", "0.0", FCVAR_CLIENTDLL );
ConVar r_portal_use_dlights( "r_portal_use_dlights", "0", FCVAR_CLIENTDLL );

void C_Prop_Portal::UpdateTransformedLighting( void )
{
	// C_Prop_Portal *pRemote = (C_Prop_Portal *)m_hLinkedPortal.Get();
	
	Vector ptForwardOrigin = m_ptOrigin + m_vForward;// * 3.0f;
	Vector vScaledRight = m_vRight; //* (GetHalfWidth() * 0.95f);
	Vector vScaledUp = m_vUp * (GetHalfHeight() * 0.95f);

	dlight_t *pFakeLight = NULL;
	// ClientShadowHandle_t ShadowHandle = CLIENTSHADOW_INVALID_HANDLE;
	/*
	if( pRemote )
	{
		pFakeLight = pRemote->TransformedLighting.m_pEntityLight;
		// ShadowHandle = pRemote->TransformedLighting.m_LightShadowHandle;
		// AssertMsg( (ShadowHandle == CLIENTSHADOW_INVALID_HANDLE) || (TransformedLighting.m_LightShadowHandle == CLIENTSHADOW_INVALID_HANDLE), "Two shadow handles found, should only have one shared handle" );
		AssertMsg( (pFakeLight == NULL) || (TransformedLighting.m_pEntityLight == NULL), "two lights found, should only have one shared light" );
		pRemote->TransformedLighting.m_pEntityLight = NULL;
		// pRemote->TransformedLighting.m_LightShadowHandle = CLIENTSHADOW_INVALID_HANDLE;
	}
	*/

	if( TransformedLighting.m_pEntityLight )
	{
		pFakeLight = TransformedLighting.m_pEntityLight;
		TransformedLighting.m_pEntityLight = NULL;
	}

	/*
	if( TransformedLighting.m_LightShadowHandle != CLIENTSHADOW_INVALID_HANDLE )
	{
		ShadowHandle = TransformedLighting.m_LightShadowHandle;
		TransformedLighting.m_LightShadowHandle = CLIENTSHADOW_INVALID_HANDLE;
	}
	*/

	/*
	if( pFakeLight != NULL )
	{
		//turn off the light so it doesn't interfere with absorbed light calculations
		pFakeLight->color.r = 0;
		pFakeLight->color.g = 0;
		pFakeLight->color.b = 0;
		pFakeLight->flags = DLIGHT_NO_WORLD_ILLUMINATION | DLIGHT_NO_MODEL_ILLUMINATION;
		pFakeLight->radius = 0.0f;
		render->TouchLight( pFakeLight );
	}
	*/

	// if ( pRemote /*&& portal_transmit_light.GetBool()*/ ) //now, see if we need to fake light coming through a portal
	{			
		/*
		Vector vLightAtRemotePortal( vec3_origin ), vLightAtLocalPortal( vec3_origin );

		if( pRemote ) //get lighting at remote portal
		{
			engine->ComputeLighting( pRemote->m_ptOrigin, NULL, false, vLightAtRemotePortal, NULL );
		}

		//now get lighting at the local portal
		{
			engine->ComputeLighting( m_ptOrigin, NULL, false, vLightAtLocalPortal, NULL );
		}
		*/

		//Vector vLightDiff = vLightAtLocalPortal - vLightAtRemotePortal;
		//if( vLightDiff.Length() > 0.6f ) //a significant difference in lighting, remember that the light vectors are NOT normalized in length
		{
			//time to fake some light coming through the greater intensity portal to the lower intensity
			
			//are we transferring light from the local portal to the remote?
			Vector ptLightOrigin, vLightForward, vColor, vClampedColor;
			float fColorScale;
			{
				int nTeam = GetTeamNumber();
				C_BasePlayer *pPlayer = GetPredictionOwner();
				if ( pPlayer )
					nTeam = pPlayer->GetTeamNumber();

				Color clrPortal = UTIL_Portal_Color( (m_bIsPortal2)?(2):(1), nTeam );
				Vector vecPortalColor
					(
						(float)(clrPortal.r()) / 255.0f, 
						(float)(clrPortal.g()) / 255.0f, 
						(float)(clrPortal.b()) / 255.0f	
					);

				/*
				if ( bLocalToRemote )
				{
					vColor = vecPortalColor;
					vLightForward = pRemote->m_vForward;
					ptLightOrigin = pRemote->m_ptOrigin;
				}
				else
				*/
				{
					vColor = vecPortalColor;
					vLightForward = m_vForward;
					ptLightOrigin = m_ptOrigin;
				}

				//clamp color values
				fColorScale = vColor.x;
				if( vColor.y > fColorScale )
					fColorScale = vColor.y;
				if( vColor.z > fColorScale )
					fColorScale = vColor.z;

				if( fColorScale > 1.0f )
					vClampedColor = vColor * (1.0f / fColorScale);
				else
					vClampedColor = vColor;
				
				/*if( vColor.x < 0.0f ) 
					vColor.x = 0.0f;
				if( vColor.x > 1.0f ) 
					vColor.x = 1.0f;

				if( vColor.y < 0.0f ) 
					vColor.y = 0.0f;
				if( vColor.y > 1.0f ) 
					vColor.y = 1.0f;

				if( vColor.z < 0.0f ) 
					vColor.z = 0.0f;
				if( vColor.z > 1.0f ) 
					vColor.z = 1.0f;*/
			}

			// Turn on the dlight
			if ( r_portal_use_dlights.GetBool() )
			{
				if( pFakeLight == NULL )
					pFakeLight = effects->CL_AllocDlight( LIGHT_INDEX_TE_DYNAMIC + entindex() ); //is there a difference between DLight and ELight when only lighting ents?
			}

			if ( pFakeLight != NULL ) //be absolutely sure that light allocation hasn't failed
			{
				/*
				if( bLocalToRemote )
				{
					//local light is greater, fake at remote portal
					pRemote->TransformedLighting.m_pEntityLight = pFakeLight;
					pFakeLight->key = pRemote->index;							
				}
				else
				*/
				{
					//remote light is greater, fake at local portal
					TransformedLighting.m_pEntityLight = pFakeLight;
					pFakeLight->key = index;					
				}

				pFakeLight->die = gpGlobals->curtime + 1e10;
				pFakeLight->flags = 0; // DLIGHT_NO_WORLD_ILLUMINATION;
				pFakeLight->minlight = 0.0f;
				pFakeLight->radius = 128.0f;
				pFakeLight->m_InnerAngle = 0.0f; //r_portal_light_innerangle.GetFloat();
				pFakeLight->m_OuterAngle = 120.0f; //r_portal_light_outerangle.GetFloat();
				pFakeLight->style = 0;
				
				pFakeLight->origin = ptLightOrigin;
				pFakeLight->m_Direction = vLightForward;

				pFakeLight->color.r = vClampedColor.x * 255;
				pFakeLight->color.g = vClampedColor.y * 255;
				pFakeLight->color.b = vClampedColor.z * 255;
				pFakeLight->color.exponent = 0.0f;

				// pFakeLight->color.exponent = ((signed int)(((*((unsigned int *)(&fColorScale))) & 0x7F800000) >> 23)) - 125; //strip the exponent from our maximum color
				
				render->TouchLight( pFakeLight );
			}

			/*
			FlashlightState_t state;
			{
				state.m_NearZ = 4.0f;
				state.m_FarZ = 500.0f;
				state.m_nSpotlightTextureFrame = 0;
				state.m_pSpotlightTexture = materials->FindTexture( "effects/flashlight001", TEXTURE_GROUP_OTHER, false );
				state.m_fConstantAtten = 0.0f;
				state.m_fLinearAtten = 500.0f;
				state.m_fQuadraticAtten = 0.0f;			
				state.m_fHorizontalFOVDegrees = 140.0f;
				state.m_fVerticalFOVDegrees = 140.0f;

				state.m_flShadowSlopeScaleDepthBias = 16.0f;
				state.m_flShadowDepthBias = 0.0005f;
				state.m_nSpotlightTextureFrame = 0;

				state.m_bEnableShadows = true;
				state.m_vecLightOrigin = ptLightOrigin;

				Vector vLightRight, vLightUp( 0.0f, 0.0f, 1.0f );
				if( fabs( DotProduct( vLightUp, vLightForward ) ) > 0.99f )
					vLightUp.Init( 0.0f, 1.0f, 0.0f );	// Don't want vLightUp and vLightForward to be parallel

				CrossProduct( vLightUp, vLightForward, vLightRight );
				VectorNormalize( vLightRight );
				CrossProduct( vLightForward, vLightRight, vLightUp );
				VectorNormalize( vLightUp );

				BasisToQuaternion( vLightForward, vLightRight, vLightUp, state.m_quatOrientation );

				state.m_Color[0] = vColor.x;// * 0.35f;
				state.m_Color[1] = vColor.y;// * 0.35f;
				state.m_Color[2] = vColor.z;// * 0.35f;
				state.m_Color[3] = 1.0f;
			}
			*/

			/*
			if( ShadowHandle != CLIENTSHADOW_INVALID_HANDLE )
			{
				g_pClientShadowMgr->UpdateFlashlightState( ShadowHandle, state ); //simpler update for existing handle
				g_pClientShadowMgr->UpdateProjectedTexture( ShadowHandle, true );
			}

			if( ShadowHandle == CLIENTSHADOW_INVALID_HANDLE )
			{
				ShadowHandle = g_pClientShadowMgr->CreateFlashlight( state );
				if( ShadowHandle != CLIENTSHADOW_INVALID_HANDLE )
					g_pClientShadowMgr->UpdateProjectedTexture( ShadowHandle, true );
			}


			if( bLocalToRemote )
				pRemote->TransformedLighting.m_LightShadowHandle = ShadowHandle;				
			else
				TransformedLighting.m_LightShadowHandle = ShadowHandle;
			*/
		}
	}
}


void C_Prop_Portal::DrawPreStencilMask( IMatRenderContext *pRenderContext )
{
	//draw the warpy effect if we're still opening up
	if ( ( m_fOpenAmount > 0.0f ) && ( m_fOpenAmount < 1.0f ) )
	{
		DrawSimplePortalMesh( pRenderContext, m_Materials.m_Portal_Refract, 0.25f );
	}
}


void C_Prop_Portal::DrawStencilMask( IMatRenderContext *pRenderContext )
{
	DrawSimplePortalMesh( pRenderContext, m_Materials.m_Portal_Stencil_Hole, 0.25f );
	DrawRenderFixMesh( pRenderContext, g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model );
}

void C_Prop_Portal::DrawDepthDoublerMesh( IMatRenderContext *pRenderContext, float fForwardOffsetModifier )
{
	if( CPortalRender::DepthDoublerPIPDisableCheck() )
		return;

	if ( m_Materials.m_PortalDepthDoubler.IsValid() )
	{
		IMaterialVar *pVar = m_Materials.m_PortalDepthDoubler->FindVarFast( "$alternateviewmatrix", &m_Materials.m_nDepthDoubleViewMatrixVarCache );
		if ( pVar != NULL )
		{
			pVar->SetMatrixValue( m_InternallyMaintainedData.m_DepthDoublerTextureView[GET_ACTIVE_SPLITSCREEN_SLOT()] );
		}
	}

	DrawSimplePortalMesh( pRenderContext, m_Materials.m_PortalDepthDoubler, fForwardOffsetModifier, 0.25f );
}

float C_Prop_Portal::GetPortalGhostAlpha( void ) const
{
	/*
	// THIS CODE HAS MOVED TO THE SHADER (portalstaticoverlay VS)
	// If we're facing away, always be full strength
	float flDot = DotProduct( m_vForward, m_ptOrigin - CurrentViewOrigin() );
	if ( flDot > 0.0f )
		return 1.0f;

	float flDistSqr = ( CurrentViewOrigin() - m_ptOrigin ).LengthSqr();
	float flAlpha = RemapValClamped( flDistSqr, Square(10*12), Square(20*12), 0.0f, 1.0f );
	*/

	// Factor how "open" this portal is, but don't lerp in the visibility until the portal is 85% open...otherwise
	//    the colored oval becomes visible before the portal's firey border is fully open.
	float flOpenScalar = clamp( ( m_fOpenAmount - 0.85f ) / 0.15f, 0.0f, 1.0f );
	return flOpenScalar;
}

void C_Prop_Portal::DrawPortalGhostLocations( IMatRenderContext *pRenderContext )
{
	// Allow this to be gated by a convar for recording
	if ( portal_draw_ghosting.GetBool() == false )
		return;

	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();
	if( iPortalCount == 0 )
		return;

	CPortalRenderable *pExitView = g_pPortalRender->GetCurrentViewExitPortal();
	C_Portal_Player *pPlayer = C_Portal_Player::GetLocalPlayer();
	bool bCoop = g_pGameRules->IsMultiplayer() && PortalMPGameRules()->IsCoOp();

	for( int i = 0; i != iPortalCount; ++i )
	{
		C_Prop_Portal *pPortal = CProp_Portal_Shared::AllPortals[i];
		if( (pPortal != pExitView) && pPortal->IsActive() )
		{
			if( (pPortal->m_hFiredByPlayer.Get() != NULL) && (bCoop || (pPortal->m_hFiredByPlayer == pPlayer)) )
			{
				//portal is a candidate for drawing ghost outlines
				if( GameRules()->IsMultiplayer() ) 
				{
					Color clrPortal = UTIL_Portal_Color( (pPortal->m_bIsPortal2)?(2):(1), pPortal->GetTeamNumber() );
					const Vector vecPortalColor
						(
						(float)(clrPortal.r()) / 255.0f, 
						(float)(clrPortal.g()) / 255.0f, 
						(float)(clrPortal.b()) / 255.0f	
						);
					float flAlpha = pPortal->GetPortalGhostAlpha();
					if ( flAlpha > 0.0f )
					{
						pPortal->DrawSimplePortalMesh( pRenderContext, m_Materials.m_PortalStaticGhostedOverlay_Tinted, 0.0f, flAlpha, &vecPortalColor );
					}
				}
				else
				{
					float flAlpha = pPortal->GetPortalGhostAlpha();
					if ( flAlpha > 0.0f )
					{
						pPortal->DrawSimplePortalMesh( pRenderContext, m_Materials.m_PortalStaticGhostedOverlay[((pPortal->m_bIsPortal2)?(1):(0))], 0.0f, flAlpha );
					}
				}
			}
		}
	}
}


void C_Prop_Portal::BuildPortalGhostRenderInfo( const CUtlVector< CPortalRenderable* > &allPortals, 
												CUtlVector< GhostPortalRenderInfo_t > &ghostPortalRenderInfosOut )
{
	Assert( ghostPortalRenderInfosOut.Count() == 0 );

	C_Portal_Player *pPlayer = C_Portal_Player::GetLocalPlayer();
	bool bIsMultiplayer = g_pGameRules->IsMultiplayer();
	bool bCoop = bIsMultiplayer && PortalMPGameRules()->IsCoOp();

	for( int i = 0; i < allPortals.Count(); i++ )
	{
		if ( allPortals[i]->IsPropPortal() )
		{
			C_Prop_Portal *pPortal = static_cast< C_Prop_Portal* >( allPortals[i] );

			if ( pPortal->IsActive() &&
				( pPortal->m_hFiredByPlayer.Get() != NULL ) &&
				( bCoop || ( pPortal->m_hFiredByPlayer == pPlayer ) ) )
			{
				ghostPortalRenderInfosOut.AddToTail();
				ghostPortalRenderInfosOut.Tail().m_pPortal = pPortal;
				ghostPortalRenderInfosOut.Tail().m_nGhostPortalQuadIndex = i;
				if ( bIsMultiplayer )
				{
					ghostPortalRenderInfosOut.Tail().m_pGhostMaterial = m_Materials.m_PortalStaticGhostedOverlay_Tinted;
				}
				else
				{
					ghostPortalRenderInfosOut.Tail().m_pGhostMaterial = m_Materials.m_PortalStaticGhostedOverlay[ pPortal->m_bIsPortal2 ? 1 : 0 ];
				}
			}
		} // end if ( is prop_portal )
	}	// end for ( each portal )
}

void C_Prop_Portal::DrawPortal( IMatRenderContext *pRenderContext )
{
	if( (view->GetDrawFlags() & DF_RENDER_REFLECTION) != 0 )
		return;

	if( WillUseDepthDoublerThisDraw() )
		m_fSecondaryStaticAmount = 0.0f;


	bool bUseAlternatePortalColors = GameRules()->IsMultiplayer() && !PortalMPGameRules()->Is2GunsCoOp(); //( m_hFiredByPlayer.Get() && ( m_hFiredByPlayer->GetTeamNumber() == TEAM_BLUE ) );

	const IMaterial *pStaticOverlayMaterial = bUseAlternatePortalColors ? m_Materials.m_PortalStaticOverlay_Tinted : m_Materials.m_PortalStaticOverlay[((m_bIsPortal2)?(1):(0))];

	if ( bUseAlternatePortalColors )
	{
		int nTeam = GetTeamNumber();
		C_BasePlayer *pPlayer = GetPredictionOwner();
		if ( pPlayer )
			nTeam = pPlayer->GetTeamNumber();

		Color clrPortal = UTIL_Portal_Color( (m_bIsPortal2)?(2):(1), nTeam );
		const Vector vecPortalColor
		(
			(float)(clrPortal.r()) / 255.0f, 
			(float)(clrPortal.g()) / 255.0f, 
			(float)(clrPortal.b()) / 255.0f	
		);

		if ( m_Materials.m_PortalStaticOverlay_Tinted.IsValid() )
		{
			IMaterialVar *pVar = m_Materials.m_PortalStaticOverlay_Tinted->FindVarFast( "$PortalColorGradientLight", &m_Materials.m_nStaticOverlayTintedColorGradientLightVarCache );
			if ( pVar != NULL )
			{
				pVar->SetVecValue( &vecPortalColor.x, 3 );
			}
		}
	}

	//stencil-based rendering
	if( g_pPortalRender->IsRenderingPortal() == false ) //main view
	{
		if( m_pLinkedPortal == NULL ) //didn't pass through pre-stencil mask
		{
			if ( ( m_fOpenAmount > 0.0f ) && ( m_fOpenAmount < 1.0f ) )
			{
				DrawSimplePortalMesh( pRenderContext, m_Materials.m_Portal_Refract, 0.25f );
			}
		}

		DrawSimplePortalMesh( pRenderContext, pStaticOverlayMaterial, 0.25f );
		
		// NOTE [BRJ 9/20/10]: This is almost certainly not necessary. Investigate if deletion is possible
		DrawRenderFixMesh( pRenderContext, g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model );
	}
	else if( g_pPortalRender->GetCurrentViewExitPortal() != this )
	{
		if( m_pLinkedPortal == NULL ) //didn't pass through pre-stencil mask
		{
			if ( ( m_fOpenAmount > 0.0f ) && ( m_fOpenAmount < 1.0f ) )
			{
				DrawSimplePortalMesh( pRenderContext, m_Materials.m_Portal_Refract, 0.25f );
			}
		}

		if( (m_InternallyMaintainedData.m_bUsableDepthDoublerConfiguration) 
			&& (g_pPortalRender->GetRemainingPortalViewDepth() == 0) 
			&& (g_pPortalRender->GetViewRecursionLevel() > 1)
			&& (g_pPortalRender->GetCurrentViewEntryPortal() == this) )
		{
			DrawDepthDoublerMesh( pRenderContext );
		}
		else
		{
			DrawSimplePortalMesh( pRenderContext, pStaticOverlayMaterial, 0.25f );
		}
	}
}

// TODO: Return the material instead, cache off material pointers?
int C_Prop_Portal::BindPortalMaterial( IMatRenderContext *pRenderContext, int nPassIndex, bool *pAllowRingMeshOptimizationOut )
{
	VPROF_BUDGET( __FUNCTION__, "BindPortalMaterial" );

	*pAllowRingMeshOptimizationOut = true;

	if( (view->GetDrawFlags() & DF_RENDER_REFLECTION) != 0 )
	{
		return 0;
	}

	if( WillUseDepthDoublerThisDraw() )
	{
		m_fSecondaryStaticAmount = 0.0f;
	}

	bool bUseAlternatePortalColors = GameRules()->IsMultiplayer() && !PortalMPGameRules()->Is2GunsCoOp(); //( m_hFiredByPlayer.Get() && ( m_hFiredByPlayer->GetTeamNumber() == TEAM_BLUE ) );

	IMaterial *pStaticOverlayMaterial = bUseAlternatePortalColors ? m_Materials.m_PortalStaticOverlay_Tinted : m_Materials.m_PortalStaticOverlay[ m_bIsPortal2 ? 1 : 0 ];

	if ( bUseAlternatePortalColors )
	{
		int nTeam = GetTeamNumber();
		C_BasePlayer *pPlayer = GetPredictionOwner();
		if ( pPlayer )
			nTeam = pPlayer->GetTeamNumber();

		Color clrPortal = UTIL_Portal_Color( (m_bIsPortal2)?(2):(1), nTeam );
		const Vector vecPortalColor
			(
			(float)(clrPortal.r()) / 255.0f, 
			(float)(clrPortal.g()) / 255.0f, 
			(float)(clrPortal.b()) / 255.0f	
			);

		if ( m_Materials.m_PortalStaticOverlay_Tinted.IsValid() )
		{
			IMaterialVar *pVar = m_Materials.m_PortalStaticOverlay_Tinted->FindVarFast( "$PortalColorGradientLight", &m_Materials.m_nStaticOverlayTintedColorGradientLightVarCache );
			if ( pVar != NULL )
			{
				pVar->SetVecValue( &vecPortalColor.x, 3 );
			}
		}
	}

	//stencil-based rendering
	if( g_pPortalRender->IsRenderingPortal() == false ) //main view
	{
		if( m_pLinkedPortal == NULL ) //didn't pass through pre-stencil mask
		{
			if ( IsPortalOpening() && ( nPassIndex == 0 ) )
			{
				pRenderContext->Bind( m_Materials.m_Portal_Refract, GetClientRenderable() );
				UpdateFrontBufferTexturesForMaterial( m_Materials.m_Portal_Refract );
				return 2;
			}
		}

		pRenderContext->Bind( pStaticOverlayMaterial, GetClientRenderable() );
		return 1;

		// NOTE [BRJ 9/20/10]: This is almost certainly not necessary. Investigate if deletion is possible
		//DrawRenderFixMesh( pRenderContext, g_pPortalRender->m_MaterialsAccess.m_WriteZ_Model );
	}
	else if( g_pPortalRender->GetCurrentViewExitPortal() != this )
	{
		if( m_pLinkedPortal == NULL ) //didn't pass through pre-stencil mask
		{
			if ( IsPortalOpening() && ( nPassIndex == 0 ) )
			{
				pRenderContext->Bind( m_Materials.m_Portal_Refract, GetClientRenderable() );
				UpdateFrontBufferTexturesForMaterial( m_Materials.m_Portal_Refract );
				return 2;
			}
		}

		if( (m_InternallyMaintainedData.m_bUsableDepthDoublerConfiguration) 
			&& (g_pPortalRender->GetRemainingPortalViewDepth() == 0) 
			&& (g_pPortalRender->GetViewRecursionLevel() > 1)
			&& (g_pPortalRender->GetCurrentViewEntryPortal() == this) )
		{
			if( CPortalRender::DepthDoublerPIPDisableCheck() )
			{
				// render a static portal instead of a broken depth doubler
				pRenderContext->Bind( pStaticOverlayMaterial, GetClientRenderable() );
				return 1;
			}

			if ( m_Materials.m_PortalDepthDoubler.IsValid() )
			{
				IMaterialVar *pVar = m_Materials.m_PortalDepthDoubler->FindVarFast( "$alternateviewmatrix", &m_Materials.m_nDepthDoubleViewMatrixVarCache );
				if ( pVar != NULL )
				{
					pVar->SetMatrixValue( m_InternallyMaintainedData.m_DepthDoublerTextureView[GET_ACTIVE_SPLITSCREEN_SLOT()] );
				}
			}

			pRenderContext->Bind( m_Materials.m_PortalDepthDoubler, GetClientRenderable() );
			*pAllowRingMeshOptimizationOut = false;
			return 1;
		}
		else
		{
			pRenderContext->Bind( pStaticOverlayMaterial, GetClientRenderable() );
			return 1;
		}
	}
	return 0;
}


void C_Prop_Portal::DoFizzleEffect( int iEffect, bool bDelayedPos /*= true*/ )
{
	if( prediction->InPrediction() && !prediction->IsFirstTimePredicted() )
		return; //early out if we're repeatedly creating particles. Creates way too many particles.

	Vector vecOrigin = ( ( bDelayedPos ) ? ( m_vDelayedPosition ) : ( GetAbsOrigin() ) );
	QAngle qAngles = ( ( bDelayedPos ) ? ( m_qDelayedAngles ) : ( GetAbsAngles() ) );

	Vector vForward, vUp;
	AngleVectors( qAngles, &vForward, &vUp, NULL );
	vecOrigin = vecOrigin + vForward * 1.0f;

	int nPortalNum = m_bIsPortal2 ? 2 : 1;
	int nTeam = GetTeamNumber();

	if ( iEffect != PORTAL_FIZZLE_SUCCESS && iEffect != PORTAL_FIZZLE_CLOSE )
		iEffect = PORTAL_FIZZLE_BAD_SURFACE;

	C_BasePlayer *pPlayer = GetPredictionOwner();
	if ( pPlayer )
		nTeam = pPlayer->GetTeamNumber();

	VectorAngles( vUp, vForward, qAngles );
	CreateFizzleEffect( pPlayer, iEffect, vecOrigin, qAngles, nTeam, nPortalNum );
}

void C_Prop_Portal::Fizzle( void )
{

}

float C_Prop_Portal::ComputeStaticAmountForRendering() const
{
	float flStaticAmount = m_fStaticAmount;

	if ( !GetLinkedPortal() )
	{
		flStaticAmount = 1.0f;
	}
	if ( WillUseDepthDoublerThisDraw() )
	{
		if ( CPortalRender::DepthDoublerPIPDisableCheck() )
		{
			flStaticAmount = 1.0f;
		}
		else
		{
			flStaticAmount = 0.0f;
		}
	}
	else if ( g_pPortalRender->GetRemainingPortalViewDepth() == 0 ) //end of the line, no more views
	{
		flStaticAmount = 1.0f;
	}
	else if ( (g_pPortalRender->GetRemainingPortalViewDepth() == 1) && (m_fSecondaryStaticAmount > flStaticAmount) ) //fading in from no views to another view (player just walked through it)
	{
		flStaticAmount = m_fSecondaryStaticAmount;
	}
	return flStaticAmount;
}


bool C_Prop_Portal::ShouldPredict( void )
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer( hh );
		if ( pLocalPlayer )
		{
			if ( m_hFiredByPlayer == pLocalPlayer )
				return true;

			CWeaponPortalgun *pPortalGun = dynamic_cast<CWeaponPortalgun*>( pLocalPlayer->Weapon_OwnsThisType( "weapon_portalgun" ) );
			if ( pPortalGun && ((pPortalGun->GetAssociatedPortal( false ) == this) || (pPortalGun->GetAssociatedPortal( true ) == this)) )
				return true;
		}
	}

	return BaseClass::ShouldPredict();
}

C_BasePlayer *C_Prop_Portal::GetPredictionOwner( void )
{
	if ( m_hFiredByPlayer != NULL )
		return (C_BasePlayer *)m_hFiredByPlayer.Get();

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( iSplitScreenSlot )
	{
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer( iSplitScreenSlot );

		if ( pLocalPlayer )
		{
			CWeaponPortalgun *pPortalGun = dynamic_cast<CWeaponPortalgun*>( pLocalPlayer->Weapon_OwnsThisType( "weapon_portalgun" ) );
			if ( pPortalGun && ((pPortalGun->GetAssociatedPortal( false ) == this) || (pPortalGun->GetAssociatedPortal( true ) == this)) )
			{
				m_hFiredByPlayer = pLocalPlayer;	// probably portal_place made this portal don't keep doing this
				return pLocalPlayer;
			}
		}
	}

	return NULL;
}



void C_Prop_Portal::HandlePredictionError( bool bErrorInThisEntity )
{
	BaseClass::HandlePredictionError( bErrorInThisEntity );
	if( bErrorInThisEntity )
	{
		if( IsActive() )
		{
			if ( !m_hEffect || !m_hEffect.IsValid() )
			{
				CreateAttachedParticles();
			}
		}
		else
		{
			DestroyAttachedParticles();
		}
	}
}



void C_Prop_Portal::GetToolRecordingState( KeyValues *msg )
{
	if ( !ToolsEnabled() )
		return;

	VPROF_BUDGET( "C_Prop_Portal::GetToolRecordingState", VPROF_BUDGETGROUP_TOOLS );

	BaseClass::GetToolRecordingState( msg );
	
	{
		PortalRecordingState_t dummyState;
		PortalRecordingState_t *pState = (PortalRecordingState_t *)msg->GetPtr( "portal", &dummyState );
		Assert( pState != &dummyState );
		
		pState->m_fOpenAmount = m_fOpenAmount;
		pState->m_fStaticAmount = m_fStaticAmount;

		pState->m_portalType = "Prop_Portal";
	}

	{
		KeyValues *pKV = CIFM_EntityKeyValuesHandler_AutoRegister::FindOrCreateNonConformantKeyValues( msg );
		pKV->SetString( CIFM_EntityKeyValuesHandler_AutoRegister::GetHandlerIDKeyString(), "C_Prop_Portal" );

		pKV->SetInt( "entIndex", index );
		pKV->SetInt( "teamNumber", GetTeamNumber() );
	}
}

class C_Prop_Portal_EntityKeyValuesHandler : public CIFM_EntityKeyValuesHandler_AutoRegister
{
public:
	C_Prop_Portal_EntityKeyValuesHandler( void ) 
		: CIFM_EntityKeyValuesHandler_AutoRegister( "C_Prop_Portal" )
	{ }

	virtual void HandleData_PreUpdate( void )
	{
		for( int i = 0; i != m_PlaybackPortals.Count(); ++i )
		{
			m_PlaybackPortals[i].bTouched = false;
		}
	}

	virtual void HandleData_PostUpdate( void )
	{
		for( int i = m_PlaybackPortals.Count(); --i >= 0; )
		{
			if( !m_PlaybackPortals[i].bTouched )
			{
				m_PlaybackPortals.FastRemove( i );
			}
		}
	}

	virtual void HandleData( KeyValues *pKeyValues )
	{
		int iEntIndex = pKeyValues->GetInt( "entIndex", -1 );
		Assert( iEntIndex != -1 );
		if( iEntIndex == -1 )
			return;

		for( int i = 0; i != m_PlaybackPortals.Count(); ++i )
		{
			if( m_PlaybackPortals[i].iEntIndex == iEntIndex )
			{
				m_PlaybackPortals[i].iTeamNumber = pKeyValues->GetInt( "teamNumber", 0 );
				m_PlaybackPortals[i].bTouched = true;
				return;
			}
		}

		//didn't exist, create it.
		RecordedPortal_t temp;
		temp.iEntIndex = iEntIndex;
		temp.bTouched = true;
		temp.iTeamNumber = pKeyValues->GetInt( "teamNumber", 0 );

		m_PlaybackPortals.AddToTail( temp );
		
	}


	virtual void HandleData_RemoveAll( void )
	{
		m_PlaybackPortals.RemoveAll();
	}


	struct RecordedPortal_t
	{
		bool bTouched;
		int iEntIndex;
		int iTeamNumber;
	};

	CUtlVector<RecordedPortal_t> m_PlaybackPortals;
};

static C_Prop_Portal_EntityKeyValuesHandler s_ProjectedWallEntityIFMHandler;

void C_Prop_Portal::HandlePortalPlaybackMessage( KeyValues *pKeyValues )
{
	BaseClass::HandlePortalPlaybackMessage( pKeyValues );

	m_fOpenAmount = pKeyValues->GetFloat( "openAmount" );
	m_fStaticAmount = pKeyValues->GetFloat( "staticAmount" );
	
	UpdateTeleportMatrix();

	
	int iEntIndexint = pKeyValues->GetInt( "portalId" );
	for( int i = 0; i != s_ProjectedWallEntityIFMHandler.m_PlaybackPortals.Count(); ++i )
	{
		if( iEntIndexint == s_ProjectedWallEntityIFMHandler.m_PlaybackPortals[i].iEntIndex )
		{
			m_iTeamNum = s_ProjectedWallEntityIFMHandler.m_PlaybackPortals[i].iTeamNumber;
		}
	}
}


CPortalRenderable *CreateProp_Portal_Fn( void )
{
	return new C_Prop_Portal;
}

static CPortalRenderableCreator_AutoRegister CreateProp_Portal( "Prop_Portal", CreateProp_Portal_Fn );

