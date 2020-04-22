//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "portal_base2d.h"
#include "portal_player.h"
#include "portal/weapon_physcannon.h"
#include "physics_npc_solver.h"
#include "envmicrophone.h"
#include "env_speaker.h"
#include "func_portal_detector.h"
#include "model_types.h"
#include "te_effect_dispatch.h"
#include "collisionutils.h"
#include "physobj.h"
#include "world.h"
#include "hierarchy.h"
#include "physics_saverestore.h"
#include "PhysicsCloneArea.h"
#include "portal_gamestats.h"
#include "portal_base2d_shared.h"
#include "weapon_portalgun.h"
#include "portal_placement.h"
#include "physicsshadowclone.h"
#include "particle_parse.h"
#include "rumble_shared.h"
#include "func_portal_orientation.h"
#include "env_debughistory.h"
#include "tier1/callqueue.h"
#include "vphysics/player_controller.h"
#include "saverestore_utlvector.h"
#include "baseprojector.h"
#include "prop_weightedcube.h"
#include "tier0/stackstats.h"

#include "portal2/portal_grabcontroller_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern Vector Portal_FindUsefulVelocity( CBaseEntity *pOther );

ConVar sv_portal_debug_touch("sv_portal_debug_touch", "0", FCVAR_REPLICATED );
ConVar sv_portal_new_velocity_check("sv_portal_new_velocity_check", "1", FCVAR_CHEAT );
ConVar sv_portal_enable_microphone("sv_portal_enable_microphone", "0", FCVAR_DEVELOPMENTONLY );
ConVar sv_portal_microphone_sensitivity ( "sv_portal_microphone_sensitivity", "1.0f" );
ConVar sv_portal_microphone_max_range ( "sv_portal_microphone_max_range", "256.0f" );
ConVar sv_portal_high_speed_physics_early_untouch( "sv_portal_high_speed_physics_early_untouch", "1" );
extern ConVar sv_allow_mobile_portal_teleportation;

const char *CPortal_Base2D::s_szTestRestingSurfaceThinkContext = "CPortal_Base2D::TestRestingSurfaceThink";
const char *CPortal_Base2D::s_szDeactivatePortalNowContext = "CPortal_Base2D::DeactivatePortalNow";

BEGIN_DATADESC( CPortal_Base2D )
	//saving
	DEFINE_FIELD( m_hLinkedPortal,		FIELD_EHANDLE ),
	DEFINE_FIELD( m_matrixThisToLinked, FIELD_VMATRIX ),
	DEFINE_KEYFIELD( m_bActivated,		FIELD_BOOLEAN,		"Activated" ),
	DEFINE_KEYFIELD( m_bOldActivatedState,		FIELD_BOOLEAN,		"OldActivated" ),
	DEFINE_KEYFIELD( m_bIsPortal2,		FIELD_BOOLEAN,		"PortalTwo" ),
	DEFINE_FIELD( m_vPrevForward,		FIELD_VECTOR ),
	DEFINE_FIELD( m_hMicrophone,		FIELD_EHANDLE ),
	DEFINE_FIELD( m_hSpeaker,			FIELD_EHANDLE ),
	DEFINE_FIELD( m_bMicAndSpeakersLinkedToRemote, FIELD_BOOLEAN ),

	DEFINE_FIELD( m_vAudioOrigin,		FIELD_VECTOR ),
	DEFINE_FIELD( m_vDelayedPosition,	FIELD_VECTOR ),
	DEFINE_FIELD( m_qDelayedAngles,		FIELD_VECTOR ),
	DEFINE_FIELD( m_iDelayedFailure,	FIELD_INTEGER ),
	DEFINE_FIELD( m_vOldPosition,		FIELD_VECTOR ),
	DEFINE_FIELD( m_qOldAngles,			FIELD_VECTOR ),
	DEFINE_FIELD( m_hPlacedBy,			FIELD_EHANDLE ),

	DEFINE_KEYFIELD( m_fNetworkHalfWidth, FIELD_FLOAT,		"HalfWidth" ),
	DEFINE_KEYFIELD( m_fNetworkHalfHeight, FIELD_FLOAT,		"HalfHeight" ),

	DEFINE_FIELD( m_bIsMobile, FIELD_BOOLEAN ),
	
	// DEFINE_FIELD( m_plane_Origin, cplane_t ),
	// DEFINE_FIELD( m_pAttachedCloningArea, CPhysicsCloneArea ),
	// DEFINE_FIELD( m_PortalSimulator, CPortalSimulator ),
	// DEFINE_FIELD( m_pCollisionShape, CPhysCollide ),
	
	DEFINE_FIELD( m_bSharedEnvironmentConfiguration, FIELD_BOOLEAN ),
	DEFINE_ARRAY( m_vPortalCorners, FIELD_POSITION_VECTOR, 4 ),
	
	DEFINE_UTLVECTOR( m_PortalEventListeners, FIELD_EHANDLE ),

	// Function Pointers
	DEFINE_THINKFUNC( TestRestingSurfaceThink ),
	DEFINE_THINKFUNC( DeactivatePortalNow ),

	DEFINE_OUTPUT( m_OnPlacedSuccessfully, "OnPlacedSuccessfully" ),
	DEFINE_OUTPUT( m_OnEntityTeleportFromMe, "OnEntityTeleportFromMe" ),
	DEFINE_OUTPUT( m_OnPlayerTeleportFromMe, "OnPlayerTeleportFromMe" ),
	DEFINE_OUTPUT( m_OnEntityTeleportToMe, "OnEntityTeleportToMe" ),
	DEFINE_OUTPUT( m_OnPlayerTeleportToMe, "OnPlayerTeleportToMe" ),

	DEFINE_FIELD( m_vPortalSpawnLocation, FIELD_VECTOR ),

END_DATADESC()

extern void SendProxy_Origin( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );
extern void SendProxy_Angles( const SendProp *pProp, const void *pStruct, const void *pData, DVariant *pOut, int iElement, int objectID );

IMPLEMENT_SERVERCLASS_ST( CPortal_Base2D, DT_Portal_Base2D )
	//upgrade origin and angles to high precision to prevent prediction errors with projected walls.
	SendPropExclude( "DT_BaseEntity", "m_vecOrigin" ),
	SendPropExclude( "DT_BaseEntity", "m_angRotation" ),
	SendPropVector( SENDINFO(m_vecOrigin), -1,  SPROP_NOSCALE, 0.0f, HIGH_DEFAULT, SendProxy_Origin ),
	SendPropVector( SENDINFO(m_angRotation), -1, SPROP_NOSCALE, 0.0f, HIGH_DEFAULT, SendProxy_Angles ),
	
	//if we're resting on another entity, we still need ultra-precise absolute coords. We should probably downgrade local origin/angles in favor of these
	SendPropVector( SENDINFO(m_ptOrigin), -1,  SPROP_NOSCALE, 0.0f, HIGH_DEFAULT ),
	SendPropVector( SENDINFO(m_qAbsAngle), -1, SPROP_NOSCALE, 0.0f, HIGH_DEFAULT ),

	SendPropEHandle( SENDINFO(m_hLinkedPortal) ),
	SendPropBool( SENDINFO(m_bActivated) ),
	SendPropBool( SENDINFO(m_bOldActivatedState) ),
	SendPropBool( SENDINFO(m_bIsPortal2) ),
	SendPropFloat( SENDINFO( m_fNetworkHalfWidth ), -1, SPROP_NOSCALE, 0.0f, HIGH_DEFAULT ),
	SendPropFloat( SENDINFO( m_fNetworkHalfHeight ), -1, SPROP_NOSCALE, 0.0f, HIGH_DEFAULT ),
	SendPropBool( SENDINFO( m_bIsMobile ) ),

	SendPropDataTable( SENDINFO_DT( m_PortalSimulator ), &REFERENCE_SEND_TABLE( DT_PortalSimulator ) )
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( portal_base2D, CPortal_Base2D );


CON_COMMAND_F( portal_place, "Places a portal. Indicate the group #, then the portal #, then pos + angle", FCVAR_CHEAT )
{
	if ( args.ArgC() != 9 )
	{
		ConMsg( "Usage: portal_place <group #> <portal #> <pos.x pos.y pos.z> <angle.x angle.y angle.z>\n" );
		return;
	}

	int iLinkageGroupID = atoi( args[1] );
	// clamp to either 0, 1 or 2
	iLinkageGroupID = MIN( iLinkageGroupID, 2 );
	iLinkageGroupID = MAX( iLinkageGroupID, 0 );

	bool bPortal2 = atoi( args[2] ) != 0;
	CProp_Portal *pPortal = CProp_Portal::FindPortal( iLinkageGroupID, bPortal2, true );
	if ( !pPortal )
	{
		ConMsg( "Error finding portal!\n" );
		return;
	}

	Vector vNewOrigin;
	vNewOrigin.x = atof( args[3] );
	vNewOrigin.y = atof( args[4] );
	vNewOrigin.z = atof( args[5] );

	// Next 3 entries are new angles
	QAngle vNewAngles;
	vNewAngles.x = atof( args[6] );
	vNewAngles.y = atof( args[7] );
	vNewAngles.z = atof( args[8] );

	// Call main placement function (skipping placement rules)
	pPortal->NewLocation( vNewOrigin, vNewAngles );
}

void PortalReportFunc( bool bOnlySpewIfPortalsChanged = false )
{
	struct PortalReportItem_t
	{
		Vector vOrigin;
		QAngle vAngles;
		int    nIndex;
		int    iLinkageGroupID;
	};
	struct PortalReport_t
	{
		int nPortals;
		int nPlayers;
		PortalReportItem_t portals[4];
		PortalReportItem_t players[2];
	};
	static PortalReport_t oldReport = { 0, 0 };

	PortalReport_t newReport;
	memset( &newReport, 0, sizeof( PortalReport_t ) );

	// Portal linkage groups are 0 in single-player or 1 & 2 for first and second player in co-op
	for ( int iLinkageGroupID = 0; iLinkageGroupID < 3; ++iLinkageGroupID )
	{
		for ( int nPortal = 0; nPortal < 2; ++nPortal )
		{
			CProp_Portal *pPortal = CProp_Portal::FindPortal( iLinkageGroupID, (nPortal != 0), false );
			if ( !pPortal )
				continue;

			PortalReportItem_t &item = newReport.portals[ newReport.nPortals++ ];
			item.vOrigin			= pPortal->m_ptOrigin;
			item.vAngles			= pPortal->m_qAbsAngle;
			item.nIndex				= nPortal;
			item.iLinkageGroupID	= iLinkageGroupID;
		}
	}

	// Player indices are 1 or 2 (only 1 in single-player)
	for ( int i = 1; i <= 2; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer )
			continue;

		PortalReportItem_t &item = newReport.players[ newReport.nPlayers++ ];
		item.vOrigin	= pPlayer->GetAbsOrigin();
		item.vAngles	= pPlayer->GetAbsAngles();
		item.nIndex		= i;
	}


	if ( !bOnlySpewIfPortalsChanged || memcmp( &oldReport.portals, &newReport.portals, sizeof( newReport.portals ) ) )
	{
		for ( int i = 0; i < newReport.nPortals; i++ )
		{
			PortalReportItem_t &item = newReport.portals[ i ];
			ConMsg( "portal_place %d %d %.3f %.3f %.3f %.3f %.3f %.3f; ",
				item.iLinkageGroupID, item.nIndex,
				item.vOrigin.x, item.vOrigin.y, item.vOrigin.z,
				item.vAngles.x, item.vAngles.y, item.vAngles.z );
		}
		for ( int i = 0; i < newReport.nPlayers; i++ )
		{
			PortalReportItem_t &item = newReport.players[ i ];
			ConMsg( "cmd%d setpos_exact %.3f %.3f %.3f; cmd%d setang_exact %.3f %.3f %.3f; ",
				item.nIndex, item.vOrigin.x, item.vOrigin.y, item.vOrigin.z,
				item.nIndex, item.vAngles.x, item.vAngles.y, item.vAngles.z );
		}
		ConMsg( "\n" );
	}
	memcpy( &oldReport, &newReport, sizeof( PortalReport_t ) );
}
CON_COMMAND_F( portal_report, "Reports the location of all portals", FCVAR_CHEAT )
{
	PortalReportFunc();
}


CPortal_Base2D::CPortal_Base2D( void )
{
	m_vPrevForward = Vector( 0.0f, 0.0f, 0.0f );
	m_PortalSimulator.SetPortalSimulatorCallbacks( this );

	// Init to something safe
	for ( int i = 0; i < 4; ++i )
	{
		m_vPortalCorners[i] = Vector(0,0,0);
	}

	CPortal_Base2D_Shared::AllPortals.AddToTail( this );

	// Make sure all listeners are clear
	m_PortalEventListeners.Purge();

	m_vPortalSpawnLocation.Invalidate();
}

CPortal_Base2D::~CPortal_Base2D( void )
{
	CPortal_Base2D_Shared::AllPortals.FindAndRemove( this );

	if( m_pCollisionShape )
	{
		physcollision->DestroyCollide( m_pCollisionShape );
		m_pCollisionShape = NULL;
	}
}


void CPortal_Base2D::UpdateOnRemove( void )
{
	m_PortalSimulator.ClearEverything();

	RemovePortalMicAndSpeaker();

	CPortal_Base2D *pRemote = m_hLinkedPortal;
	if( pRemote != NULL )
	{
		m_PortalSimulator.DetachFromLinked();
		m_hLinkedPortal = NULL;
		SetActive( false );
		m_bOldActivatedState = false;
		pRemote->UpdatePortalLinkage();
		pRemote->UpdatePortalTeleportMatrix();
	}

	if( m_pAttachedCloningArea )
	{
		UTIL_Remove( m_pAttachedCloningArea );
		m_pAttachedCloningArea = NULL;
	}
	
	m_PortalEventListeners.Purge();

	BaseClass::UpdateOnRemove();
}

void CPortal_Base2D::Spawn( void )
{
	Precache();

	UpdateCollisionShape();

	Assert( (m_fNetworkHalfHeight > 0.0f) && (m_fNetworkHalfWidth > 0.0f) );

	m_matrixThisToLinked.Identity(); //don't accidentally teleport objects to zero space
	
	AddEffects( EF_NORECEIVESHADOW | EF_NOSHADOW );

	SetSolid( SOLID_OBB );
	SetSolidFlags( FSOLID_TRIGGER | FSOLID_NOT_SOLID | FSOLID_CUSTOMBOXTEST | FSOLID_CUSTOMRAYTEST );
	SetMoveType( MOVETYPE_NONE );
	SetCollisionGroup( COLLISION_GROUP_PLAYER );

	SetSize( GetLocalMins(), GetLocalMaxs() );

	UpdateCorners();

	if( sv_portal_enable_microphone.GetInt() )
	{
		CreateMicAndSpeaker();
	}

	BaseClass::Spawn();

	m_pAttachedCloningArea = CPhysicsCloneArea::CreatePhysicsCloneArea( this );

	m_bMicAndSpeakersLinkedToRemote = false;

	// Because we're not solid
	AddEFlags( EFL_USE_PARTITION_WHEN_NOT_SOLID );

	m_vPortalSpawnLocation = GetAbsOrigin();
}

void CPortal_Base2D::OnRestore()
{
	m_ptOrigin = GetAbsOrigin();
	m_qAbsAngle = GetAbsAngles();

	UpdatePortalTeleportMatrix();
	UpdateCorners();
	Assert( m_pAttachedCloningArea == NULL );
	m_pAttachedCloningArea = CPhysicsCloneArea::CreatePhysicsCloneArea( this );

	BaseClass::OnRestore();
}

void DumpActiveCollision( const CPortalSimulator *pPortalSimulator, const char *szFileName );
void PortalSimulatorDumps_DumpCollideToGlView( CPhysCollide *pCollide, const Vector &origin, const QAngle &angles, float fColorScale, const char *pFilename );

bool CPortal_Base2D::TestCollision( const Ray_t &ray, unsigned int fContentsMask, trace_t& tr )
{
	if ( !m_pCollisionShape )
	{
		//HACK: This is a last-gasp type fix for a crash caused by m_pCollisionShape not yet being set up
		// during a restore.
		UpdateCollisionShape();
	}

	physcollision->TraceBox( ray, MASK_ALL, NULL, m_pCollisionShape, GetAbsOrigin(), GetAbsAngles(), &tr );
	return tr.DidHit();
}

ConVar portal_test_resting_surface_for_paint( "portal_test_resting_surface_for_paint", "0", 0, "Test if a portal is on a white painted surface and fizzle if it goes away.  Test it EVERY FRAME." );

//-----------------------------------------------------------------------------
// Purpose: When placed on a surface that could potentially go away (anything but world geo), we test for that condition and fizzle
//-----------------------------------------------------------------------------
void CPortal_Base2D::TestRestingSurfaceThink( void )
{
	// Make sure there's still a surface behind the portal
	Vector vOrigin = GetAbsOrigin();

	Vector vForward, vRight, vUp;
	GetVectors( &vForward, &vRight, &vUp );

	trace_t tr;
	CTraceFilterSimpleClassnameList baseFilter( NULL, COLLISION_GROUP_NONE );
	UTIL_Portal_Trace_Filter( &baseFilter );
	baseFilter.AddClassnameToIgnore( "prop_portal" );
	CTraceFilterTranslateClones traceFilterPortalShot( &baseFilter );

	int iCornersOnVolatileSurface = 0;

	bool bAnyCornerOnPortalPaint = false;

	// Check corners
	for ( int iCorner = 0; iCorner < 4; ++iCorner )
	{
		Vector vCorner = vOrigin;

		if ( iCorner % 2 == 0 )
			vCorner += vRight * ( m_fNetworkHalfWidth - PORTAL_BUMP_FORGIVENESS * 1.1f );
		else
			vCorner += -vRight * ( m_fNetworkHalfWidth - PORTAL_BUMP_FORGIVENESS * 1.1f );

		if ( iCorner < 2 )
			vCorner += vUp * ( m_fNetworkHalfHeight - PORTAL_BUMP_FORGIVENESS * 1.1f );
		else
			vCorner += -vUp * ( m_fNetworkHalfHeight - PORTAL_BUMP_FORGIVENESS * 1.1f );

		Ray_t ray;
		ray.Init( vCorner, vCorner - vForward );
		enginetrace->TraceRay( ray, MASK_SOLID_BRUSHONLY, &traceFilterPortalShot, &tr );

		// This corner isn't on a valid brush (skipping phys converts or physboxes because they frequently go through portals and can't be placed upon).
		if ( tr.fraction == 1.0f && !tr.startsolid && ( !tr.m_pEnt || ( tr.m_pEnt && !FClassnameIs( tr.m_pEnt, "func_physbox" ) && !FClassnameIs( tr.m_pEnt, "simple_physics_brush" ) ) ) ) 
		{
			DevMsg( "Surface removed from behind portal.\n" );
			DeactivatePortalOnThink();
			SetContextThink( NULL, TICK_NEVER_THINK, s_szTestRestingSurfaceThinkContext );
			break;
		}

		if ( portal_test_resting_surface_for_paint.GetBool() )
		{
			PortalSurfaceType_t portalSurfType = PortalSurfaceType( tr );

			// check if we still on portal paint
			if ( portalSurfType == PORTAL_SURFACE_PAINT )
			{
				bAnyCornerOnPortalPaint = true;
			}

			// This corner is on no portal surf without portal paint
			if ( portalSurfType == PORTAL_SURFACE_INVALID )
			{
				DevMsg( "Portal paint removed from behind portal.\n" );
				DeactivatePortalOnThink();
				SetContextThink( NULL, TICK_NEVER_THINK, s_szTestRestingSurfaceThinkContext );
				break;
			}
		}

		if ( !tr.DidHitWorld() )
		{
			iCornersOnVolatileSurface++;
		}
	}

	// Still on a movable or deletable surface or portal paint
	if ( iCornersOnVolatileSurface > 0 || bAnyCornerOnPortalPaint )
	{
		SetContextThink ( &CPortal_Base2D::TestRestingSurfaceThink, gpGlobals->curtime + 0.1f, s_szTestRestingSurfaceThinkContext );
	}
	else
	{
		// All corners on world, we don't need to test
		SetContextThink( NULL, TICK_NEVER_THINK, s_szTestRestingSurfaceThinkContext );
	}
}


void CPortal_Base2D::DeactivatePortalOnThink( void )
{
	if( IsActive() && (GetNextThink( s_szDeactivatePortalNowContext ) == TICK_NEVER_THINK) )
	{
		SetContextThink( &CPortal_Base2D::DeactivatePortalNow, gpGlobals->curtime, s_szDeactivatePortalNowContext );
	}
}


void CPortal_Base2D::DeactivatePortalNow( void )
{
	// if held entity is on the opposite side of portal, make the player drops the entity when either portal gets deactivated
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CPortal_Player *pPlayer = ToPortalPlayer( UTIL_PlayerByIndex( i ) );

		//If the other player exists and is connected
		if( pPlayer && pPlayer->IsConnected() && pPlayer->IsUsingVMGrab() )
		{
			if ( GetPlayerHeldEntity( pPlayer ) && pPlayer->IsHeldObjectOnOppositeSideOfPortal() )
			{
				CPortal_Base2D *pHeldObjectPortal = pPlayer->GetHeldObjectPortal();
				CPortal_Base2D *pPlayerPortal = pHeldObjectPortal->m_hLinkedPortal.Get();
				if ( pPlayerPortal == this || pHeldObjectPortal == this )
				{
					pPlayer->ClearUseEntity();
				}
			}
		}
	}

	CPortal_Base2D *pRemotePortal = m_hLinkedPortal;

	StopParticleEffects( this );

	SetActive( false );
	m_bOldActivatedState = false;
	m_hLinkedPortal = NULL;
	m_PortalSimulator.DetachFromLinked();
	m_PortalSimulator.ReleaseAllEntityOwnership();

	if( pRemotePortal )
	{
		pRemotePortal->UpdatePortalLinkage();
	}

	UpdateClientCheckPVS();

	SetMoveType( MOVETYPE_NONE );
	SetMobileState( false );

	SetContextThink( NULL, TICK_NEVER_THINK, s_szDeactivatePortalNowContext );
	SetContextThink( NULL, TICK_NEVER_THINK, s_szTestRestingSurfaceThinkContext );

	OnPortalDeactivated();
}

//-----------------------------------------------------------------------------
// Purpose: Removes the portal microphone and speakers. This is done in two places
//			(fizzle and UpdateOnRemove) so the code is consolidated here.
// Input  :  - 
//-----------------------------------------------------------------------------
void CPortal_Base2D::RemovePortalMicAndSpeaker()
{

	// Shut down microphone/speaker if they exist
	if ( m_hMicrophone )
	{
		CEnvMicrophone *pMicrophone = (CEnvMicrophone*)(m_hMicrophone.Get());
		if ( pMicrophone )
		{
			inputdata_t inMicDisable;
			pMicrophone->InputDisable( inMicDisable );
			UTIL_Remove( pMicrophone );
		}
		m_hMicrophone = 0;
	}

	if ( m_hSpeaker )
	{
		CSpeaker *pSpeaker = (CSpeaker *)(m_hSpeaker.Get());
		if ( pSpeaker )
		{
			// Remove the remote portal's microphone, as it references the speaker we're about to remove.
			if ( m_hLinkedPortal.Get() )
			{
				CPortal_Base2D* pRemotePortal =  m_hLinkedPortal.Get();
				if ( pRemotePortal->m_hMicrophone )
				{
					inputdata_t inputdata;
					inputdata.pActivator = this;
					inputdata.pCaller = this;
					CEnvMicrophone* pRemotePortalMic = dynamic_cast<CEnvMicrophone*>(pRemotePortal->m_hMicrophone.Get());
					if ( pRemotePortalMic )
					{
						pRemotePortalMic->Remove();
					}
				}
			}
			inputdata_t inTurnOff;
			pSpeaker->InputTurnOff( inTurnOff );
			UTIL_Remove( pSpeaker );
		}
		m_hSpeaker = 0;
	}

	m_bMicAndSpeakersLinkedToRemote = false;

	if ( m_hLinkedPortal.Get() )
	{
		if ( m_hLinkedPortal->m_hMicrophone.Get() )
		{
			CEnvMicrophone* pRemoteMic = dynamic_cast<CEnvMicrophone*>( m_hLinkedPortal->m_hMicrophone.Get() );
			if ( pRemoteMic )
			{
				pRemoteMic->SetSpeaker( NULL_STRING, NULL );
			}
		}
		m_hLinkedPortal->m_bMicAndSpeakersLinkedToRemote = false;
	}
}

void CPortal_Base2D::PunchPenetratingPlayer( CBasePlayer *pPlayer )
{
	if ( m_PortalSimulator.IsReadyToSimulate() )
	{
		ICollideable *pCollideable = pPlayer->GetCollideable();
		if ( pCollideable )
		{
			Vector vMin, vMax;

			pCollideable->WorldSpaceSurroundingBounds( &vMin, &vMax );

			if ( UTIL_IsBoxIntersectingPortal( ( vMin + vMax ) / 2.0f, ( vMax - vMin ) / 2.0f, this ) )
			{
				Ray_t playerRay;
				playerRay.Init( pPlayer->GetAbsOrigin(), pPlayer->GetAbsOrigin(), pPlayer->GetPlayerMins(), pPlayer->GetPlayerMaxs() );

				trace_t WorldTrace;
				CTraceFilterSimple traceFilter( pPlayer, COLLISION_GROUP_PLAYER_MOVEMENT );
				enginetrace->TraceRay( playerRay, MASK_PLAYERSOLID, &traceFilter, &WorldTrace );

				if( WorldTrace.startsolid ) //player would be stuck unless moving using portal traces. Really good indicator that they're actually in the portal plane.
				{
					Vector vForward;
					GetVectors( &vForward, 0, 0 );
					vForward *= 100.0f;
					pPlayer->VelocityPunch( vForward );
				}
			}
		}
	}
}

void CPortal_Base2D::PunchAllPenetratingPlayers( void )
{
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer )
		{
			PunchPenetratingPlayer( pPlayer );
		}
	}
}

void CPortal_Base2D::Activate( void )
{
	UpdateCollisionShape();

	if( m_pAttachedCloningArea == NULL )
		m_pAttachedCloningArea = CPhysicsCloneArea::CreatePhysicsCloneArea( this );

	UpdatePortalTeleportMatrix();
	
	UpdatePortalLinkage();

	UpdateClientCheckPVS();

	BaseClass::Activate();

	AddEffects( EF_NOSHADOW | EF_NORECEIVESHADOW );

	m_PortalSimulator.SetCarvedParent( GetParent() );

	if( IsActive() && (m_hLinkedPortal.Get() != NULL) )
	{
		Vector ptCenter = GetAbsOrigin();
		QAngle qAngles = GetAbsAngles();
		SetSize( GetLocalMins(), GetLocalMaxs() );
		m_PortalSimulator.SetSize( GetHalfWidth(), GetHalfHeight() );
		m_PortalSimulator.MoveTo( ptCenter, qAngles );


		//resimulate everything we're touching
		touchlink_t *root = ( touchlink_t * )GetDataObject( TOUCHLINK );
		if( root )
		{
			for( touchlink_t *link = root->nextLink; link != root; link = link->nextLink )
			{
				if( (link->flags & FTOUCHLINK_START_TOUCH) == 0 )
					continue; //not actually touching this thing

				CBaseEntity *pOther = link->entityTouched;
				bool bTeleportable = CPortal_Base2D_Shared::IsEntityTeleportable( pOther );
				bool bShouldCloneAcrossPortals = CPortal_Base2D_Shared::ShouldPhysicsCloneNonTeleportableEntityAcrossPortals( pOther );
				if( bTeleportable || bShouldCloneAcrossPortals )
				{
					CCollisionProperty *pOtherCollision = pOther->CollisionProp();
					Vector vWorldMins, vWorldMaxs;
					pOtherCollision->WorldSpaceAABB( &vWorldMins, &vWorldMaxs );
					Vector ptOtherCenter = (vWorldMins + vWorldMaxs) / 2.0f;

					if( m_plane_Origin.normal.Dot( ptOtherCenter ) > m_plane_Origin.dist )
					{
						//we should be interacting with this object, add it to our environment
						if( bTeleportable )
						{
							if( SharedEnvironmentCheck( pOther ) )
							{
								Assert( ((m_PortalSimulator.GetLinkedPortalSimulator() == NULL) && (m_hLinkedPortal.Get() == NULL)) || 
									(m_PortalSimulator.GetLinkedPortalSimulator() == &m_hLinkedPortal->m_PortalSimulator) ); //make sure this entity is linked to the same portal as our simulator

								CPortalSimulator *pOwningSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pOther );
								if( pOwningSimulator && (pOwningSimulator != &m_PortalSimulator) )
									pOwningSimulator->ReleaseOwnershipOfEntity( pOther );

								m_PortalSimulator.TakeOwnershipOfEntity( pOther );
							}
						}
						else if( bShouldCloneAcrossPortals )
						{
							m_PortalSimulator.StartCloningEntityAcrossPortals( pOther );
						}
					}
				}
			}
		}
	}
}

void CPortal_Base2D::Touch( CBaseEntity *pOther )
{
	if( pOther->IsPlayer() || (pOther == GetMoveParent()) )
		return;

	BaseClass::Touch( pOther );
	pOther->Touch( this );

	// Don't do anything on touch if it's not active
	if( !IsActive() || (m_hLinkedPortal.Get() == NULL) )
	{
		Assert( !m_PortalSimulator.OwnsEntity( pOther ) );
		Assert( !pOther->IsPlayer() || (((CPortal_Player *)pOther)->m_hPortalEnvironment.Get() != this) );
		
		//I'd really like to fix the root cause, but this will keep the game going
		m_PortalSimulator.ReleaseOwnershipOfEntity( pOther );
		m_PortalSimulator.StopCloningEntityAcrossPortals( pOther );
		return;
	}

	Assert( IsMobile() || ((m_PortalSimulator.GetLinkedPortalSimulator() == NULL) && (m_hLinkedPortal.Get() == NULL)) || (m_hLinkedPortal->IsMobile()) ||
		(m_PortalSimulator.GetLinkedPortalSimulator() == &m_hLinkedPortal->m_PortalSimulator) ); //make sure this entity is linked to the same portal as our simulator

	if( IsMobile() || ((m_hLinkedPortal.Get() != NULL) && m_hLinkedPortal->IsMobile()) )
	{
		if( !sv_allow_mobile_portal_teleportation.GetBool() || !pOther->IsPlayer() )
			return;
	}

	// Fizzle portal with any moving brush
	Vector vVelocityCheck;
	AngularImpulse vAngularImpulseCheck;
	pOther->GetVelocity( &vVelocityCheck, &vAngularImpulseCheck );

	if( vVelocityCheck != vec3_origin || vAngularImpulseCheck != vec3_origin )
	{
		if ( modelinfo->GetModelType( pOther->GetModel() ) == mod_brush )
		{
			if ( !FClassnameIs( pOther, "func_physbox" ) && !FClassnameIs( pOther, "simple_physics_brush" ) )	// except CPhysBox
			{
				Vector vForward;
				GetVectors( &vForward, NULL, NULL );

				Vector vMin, vMax;
				pOther->GetCollideable()->WorldSpaceSurroundingBounds( &vMin, &vMax );

				if ( UTIL_IsBoxIntersectingPortal( ( vMin + vMax ) / 2.0f, ( vMax - vMin ) / 2.0f - Vector( 2.0f, 2.0f, 2.0f ), this, 0.0f ) &&
					((pOther->GetSolid() != SOLID_VPHYSICS) || !m_pCollisionShape || UTIL_IsCollideableIntersectingPhysCollide( pOther->GetCollideable(), m_pCollisionShape, m_ptOrigin, m_qAbsAngle )) )
				{
					DevMsg( "Moving brush intersected portal plane.\n" );
					DeactivatePortalOnThink();
					return;
				}
				else
				{
					Vector vOrigin = GetAbsOrigin();

					trace_t tr;

					UTIL_TraceLine( vOrigin, vOrigin - vForward * PORTAL_HALF_DEPTH, MASK_SOLID_BRUSHONLY, NULL, COLLISION_GROUP_NONE, &tr );

					// Something went wrong
					if ( tr.fraction == 1.0f && !tr.startsolid )
					{
						DevMsg( "Surface removed from behind portal.\n" );
						DeactivatePortalOnThink();
						return;
					}
					else if ( !sv_allow_mobile_portals.GetBool() && tr.m_pEnt && tr.m_pEnt->IsMoving() )
					{
						DevMsg( "Surface behind portal is moving.\n" );
						DeactivatePortalOnThink();
						return;
					}
				}
			}
		}
	}
	
	if( m_hLinkedPortal == NULL )
		return;

	if( sv_portal_high_speed_physics_early_untouch.GetBool() )
	{
		IPhysicsObject *pOtherPhysObject = pOther->VPhysicsGetObject();
		if( (pOther->GetMoveType() == MOVETYPE_VPHYSICS) && (pOtherPhysObject != NULL) )
		{
			Vector vPhysVelocity; //physics velocity not necessarily equal to entity velocity
			pOtherPhysObject->GetVelocity( &vPhysVelocity, NULL );
			float fExitSpeed = m_plane_Origin.normal.Dot( vPhysVelocity );

			if( fExitSpeed > 200.0f ) //200.0f is a magic number indicating "leaving the portal and unlikely to turn back in a single tick"
			{
				const CPhysCollide *pCollide = pOtherPhysObject->GetCollide();
				if( pCollide )
				{
					//vphysics object leaving portal at a healthy velocity.
					//If it's moving fast enough, it will have left our bounds by a large margin before we receive an EndTouch(). It's possible that "large margin" lets the object pass outside
					//	the area that we carved portal collision. See if we should remove it from the portal environment now instead so it always has valid collision
					
					Vector vPos;
					QAngle qAngle;
					pOtherPhysObject->GetPosition( &vPos, &qAngle );
					float fRadius = physcollision->CollideGetRadius( pCollide );

					if( (m_plane_Origin.normal.Dot( vPos ) - fRadius) > m_plane_Origin.dist )
					{
						//has fully left portal hole, switching collision should be a safe operation
						
						const float fCollisionDataEndsDist = m_PortalSimulator.GetInternalData().Placement.vCollisionCloneExtents.x; //we don't have collision data for the world past this plane!
						float fDistToEndPlane = -((m_plane_Origin.normal.Dot( vPos ) - (m_plane_Origin.dist + fCollisionDataEndsDist)) + fRadius);
											
						float fTimeToExit = fDistToEndPlane / fExitSpeed;
						
						if( fTimeToExit < TICK_INTERVAL )
						{
							//at our current velocity, we will pass outside our carved collision within a tick, need to switch now.
							
#if 0 //debugging overlays
							//projection of where we believe we absolutely must be colliding with the real world as a white OBB
							NDebugOverlay::BoxAngles( vPos + vPhysVelocity * fTimeToExit, pOther->CollisionProp()->OBBMins(), pOther->CollisionProp()->OBBMaxs(), qAngle, 255, 255, 255, 64, 30.0f );
							//where we are right now as a green OBB
							NDebugOverlay::BoxAngles( vPos, pOther->CollisionProp()->OBBMins(), pOther->CollisionProp()->OBBMaxs(), qAngle, 0, 255, 0, 64, 30.0f );
#endif

							m_PortalSimulator.ReleaseOwnershipOfEntity( pOther );
							return;
						}
					}
				}
			}
		}
	}

	bool bTeleportable = CPortal_Base2D_Shared::IsEntityTeleportable( pOther );	
	
	//see if we should even be interacting with this object, this is a bugfix where some objects get added to physics environments through walls
	if( !IsMobile() && !m_hLinkedPortal->IsMobile() )
	{
		bool bShouldCloneAcrossPortals = CPortal_Base2D_Shared::ShouldPhysicsCloneNonTeleportableEntityAcrossPortals( pOther );
		if( bTeleportable || bShouldCloneAcrossPortals )
		{
			CCollisionProperty *pOtherCollision = pOther->CollisionProp();
			Vector vWorldMins, vWorldMaxs;
			pOtherCollision->WorldSpaceAABB( &vWorldMins, &vWorldMaxs );
			Vector ptOtherCenter = (vWorldMins + vWorldMaxs) / 2.0f;

			//hmm, not in our environment, plane tests, sharing tests
			if( (m_plane_Origin.normal.Dot( ptOtherCenter ) >= m_plane_Origin.dist) )
			{
				if( bTeleportable && !m_PortalSimulator.OwnsEntity( pOther ) )
				{
					if( SharedEnvironmentCheck( pOther ) )
					{
						CPortalSimulator *pOwningSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pOther );
						if( pOwningSimulator && (pOwningSimulator != &m_PortalSimulator) )
							pOwningSimulator->ReleaseOwnershipOfEntity( pOther );

						m_PortalSimulator.TakeOwnershipOfEntity( pOther );
					}
				}
				else if( bShouldCloneAcrossPortals )
				{
					m_PortalSimulator.StartCloningEntityAcrossPortals( pOther );
				}
			}
		}
	}

	if( bTeleportable && ShouldTeleportTouchingEntity( pOther ) &&
		(m_PortalSimulator.OwnsEntity( pOther ) || IsMobile() || m_hLinkedPortal->IsMobile()) )
	{
		TeleportTouchingEntity( pOther );
	}
}

void CPortal_Base2D::StartTouch( CBaseEntity *pOther )
{
	if( pOther->IsPlayer() || (pOther == GetMoveParent()) )
		return;

	BaseClass::StartTouch( pOther );

	// Since prop_portal is a trigger it doesn't send back start touch, so I'm forcing it
	pOther->StartTouch( this );

	if( sv_portal_debug_touch.GetBool() )
	{
		Vector vVelocity = Portal_FindUsefulVelocity( pOther );
		DevMsg( "Portal %i StartTouch: %s : %f %f %f : %f\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname(), vVelocity.x, vVelocity.y, vVelocity.z, gpGlobals->curtime );
	}
#if !defined ( DISABLE_DEBUG_HISTORY )
	if ( !IsMarkedForDeletion() )
	{
		ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Portal %i StartTouch: %s : %f\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname(), gpGlobals->curtime ) );
	}
#endif

	if( (m_hLinkedPortal == NULL) || (IsActive() == false) || IsMobile() || m_hLinkedPortal->IsMobile() )
		return;

	bool bTeleportable = CPortal_Base2D_Shared::IsEntityTeleportable( pOther );
	bool bShouldCloneAcrossPortals = CPortal_Base2D_Shared::ShouldPhysicsCloneNonTeleportableEntityAcrossPortals( pOther );
	if( bTeleportable || bShouldCloneAcrossPortals )
	{
		CCollisionProperty *pOtherCollision = pOther->CollisionProp();
		Vector vWorldMins, vWorldMaxs;
		pOtherCollision->WorldSpaceAABB( &vWorldMins, &vWorldMaxs );
		Vector ptOtherCenter = (vWorldMins + vWorldMaxs) / 2.0f;

		if( m_plane_Origin.normal.Dot( ptOtherCenter ) > m_plane_Origin.dist )
		{
			//we should be interacting with this object, add it to our environment
			if( bTeleportable )
			{
				if( SharedEnvironmentCheck( pOther ) )
				{
					Assert( IsMobile() || m_hLinkedPortal->IsMobile() || ((m_PortalSimulator.GetLinkedPortalSimulator() == NULL) && (m_hLinkedPortal.Get() == NULL)) || 
						(m_PortalSimulator.GetLinkedPortalSimulator() == &m_hLinkedPortal->m_PortalSimulator) ); //make sure this entity is linked to the same portal as our simulator

					CPortalSimulator *pOwningSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pOther );
					if( pOwningSimulator && (pOwningSimulator != &m_PortalSimulator) )
						pOwningSimulator->ReleaseOwnershipOfEntity( pOther );

					m_PortalSimulator.TakeOwnershipOfEntity( pOther );
				}
			}
			else if( bShouldCloneAcrossPortals )
			{
				m_PortalSimulator.StartCloningEntityAcrossPortals( pOther );
			}
		}
	}	
}

void CPortal_Base2D::EndTouch( CBaseEntity *pOther )
{
	if ( pOther->IsPlayer() || (pOther == GetMoveParent()) )
		return;

	BaseClass::EndTouch( pOther );

	// Since prop_portal is a trigger it doesn't send back end touch, so I'm forcing it
	pOther->EndTouch( this );

	// Don't do anything on end touch if it's not active
	if ( !IsActive() || IsMobile() || ((m_hLinkedPortal.Get() != NULL) && m_hLinkedPortal->IsMobile())  )
	{
		return;
	}

	if ( ShouldTeleportTouchingEntity( pOther ) ) //an object passed through the plane and all the way out of the touch box
	{
		 TeleportTouchingEntity( pOther );
	}
	else if ( pOther->IsPlayer() && //player
			  IsCeilingPortal( -0.7071f ) && //most likely falling out of the portal
			  (m_PortalSimulator.GetInternalData().Placement.PortalPlane.m_Normal.Dot( pOther->WorldSpaceCenter() ) < m_PortalSimulator.GetInternalData().Placement.PortalPlane.m_Dist) && //but behind the portal plane
			  (((CPortal_Player *)pOther)->m_Local.m_bInDuckJump) ) //while ducking
	{
		//player has pulled their feet up (moving their center instantaneously) while falling downward out of the portal, send them back (probably only for a frame)
		
		DevMsg( "Player pulled feet above the portal they fell out of, postponing Releasing ownership\n" );
		//TeleportTouchingEntity( pOther );
	}
	else
	{
		//only 1 of these 2 calls should actually perform work depending on how the entity interacts with portals
		m_PortalSimulator.ReleaseOwnershipOfEntity( pOther );
		m_PortalSimulator.StopCloningEntityAcrossPortals( pOther );
	}

	if ( sv_portal_debug_touch.GetBool() )
	{
		Vector vVelocity = Portal_FindUsefulVelocity( pOther );
		DevMsg( "Portal %i EndTouch: %s : %f %f %f : %f\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname(), vVelocity.x, vVelocity.y, vVelocity.z, gpGlobals->curtime );
	}

#if !defined( DISABLE_DEBUG_HISTORY )
	if ( !IsMarkedForDeletion() )
	{
		ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Portal %i EndTouch: %s : %f\n", ((m_bIsPortal2)?(2):(1)), pOther->GetClassname(), gpGlobals->curtime ) );
	}
#endif
}

bool CPortal_Base2D::SharedEnvironmentCheck( CBaseEntity *pEntity )
{
	Assert( IsMobile() || ((m_PortalSimulator.GetLinkedPortalSimulator() == NULL) && (m_hLinkedPortal.Get() == NULL)) || (m_hLinkedPortal->IsMobile()) ||
		(m_PortalSimulator.GetLinkedPortalSimulator() == &m_hLinkedPortal->m_PortalSimulator) ); //make sure this entity is linked to the same portal as our simulator

	CPortalSimulator *pOwningSimulator = CPortalSimulator::GetSimulatorThatOwnsEntity( pEntity );
	if( (pOwningSimulator == NULL) || (pOwningSimulator == &m_PortalSimulator) )
	{
		//nobody else is claiming ownership
		return true;
	}

	Vector ptCenter = pEntity->WorldSpaceCenter();
	if( (ptCenter - m_PortalSimulator.GetInternalData().Placement.ptCenter).LengthSqr() < (ptCenter - pOwningSimulator->GetInternalData().Placement.ptCenter).LengthSqr() )
		return true;

	/*if( !m_hLinkedPortal->m_PortalSimulator.EntityIsInPortalHole( pEntity ) )
	{
		Vector vOtherVelocity;
		pEntity->GetVelocity( &vOtherVelocity );

		if( vOtherVelocity.Dot( m_PortalSimulator.GetInternalData().Placement.vForward ) < vOtherVelocity.Dot( m_hLinkedPortal->m_PortalSimulator.GetInternalData().Placement.vForward ) )
			return true; //entity is going towards this portal more than the other
	}*/

	return false;

	//we're in the shared configuration, and the other portal already owns the object, see if we'd be a better caretaker (distance check
	/*CCollisionProperty *pEntityCollision = pEntity->CollisionProp();
	Vector vWorldMins, vWorldMaxs;
	pEntityCollision->WorldSpaceAABB( &vWorldMins, &vWorldMaxs );
	Vector ptEntityCenter = (vWorldMins + vWorldMaxs) / 2.0f;

	Vector vEntToThis = GetAbsOrigin() - ptEntityCenter;
	Vector vEntToRemote = m_hLinkedPortal->GetAbsOrigin() - ptEntityCenter;

	return ( vEntToThis.LengthSqr() < vEntToRemote.LengthSqr() );*/
}

void CPortal_Base2D::WakeNearbyEntities( void )
{
	CBaseEntity*	pList[ 1024 ];

	Vector vForward, vUp, vRight;
	GetVectors( &vForward, &vRight, &vUp );

	Vector ptOrigin = GetAbsOrigin();
	QAngle qAngles = GetAbsAngles();

	Vector vLocalMins = GetLocalMins();
	Vector vLocalMaxs = GetLocalMaxs();

	Vector ptOBBStart = ptOrigin;
	ptOBBStart += vForward * vLocalMins.x;
	ptOBBStart += vRight * vLocalMins.y;
	ptOBBStart += vUp * vLocalMins.z;


	vForward *= vLocalMaxs.x - vLocalMins.x;
	vRight *= vLocalMaxs.y - vLocalMins.y;
	vUp *= vLocalMaxs.z - vLocalMins.z;


	Vector vAABBMins, vAABBMaxs;
	vAABBMins = vAABBMaxs = ptOBBStart;

	for( int i = 1; i != 8; ++i )
	{
		Vector ptTest = ptOBBStart;
		if( i & (1 << 0) ) ptTest += vForward;
		if( i & (1 << 1) ) ptTest += vRight;
		if( i & (1 << 2) ) ptTest += vUp;

		if( ptTest.x < vAABBMins.x ) vAABBMins.x = ptTest.x;
		if( ptTest.y < vAABBMins.y ) vAABBMins.y = ptTest.y;
		if( ptTest.z < vAABBMins.z ) vAABBMins.z = ptTest.z;
		if( ptTest.x > vAABBMaxs.x ) vAABBMaxs.x = ptTest.x;
		if( ptTest.y > vAABBMaxs.y ) vAABBMaxs.y = ptTest.y;
		if( ptTest.z > vAABBMaxs.z ) vAABBMaxs.z = ptTest.z;
	}
	
	int count = UTIL_EntitiesInBox( pList, 1024, vAABBMins, vAABBMaxs, 0 );

	//Iterate over all the possible targets
	for ( int i = 0; i < count; i++ )
	{
		CBaseEntity *pEntity = pList[i];

		if ( pEntity && (pEntity != this) )
		{
			CCollisionProperty *pEntCollision = pEntity->CollisionProp();
			Vector ptEntityCenter = pEntCollision->GetCollisionOrigin();

			//double check intersection at the OBB vs OBB level, we don't want to affect large piles of physics objects if we don't have to. It gets slow
			if( IsOBBIntersectingOBB( ptOrigin, qAngles, vLocalMins, vLocalMaxs, 
				ptEntityCenter, pEntCollision->GetCollisionAngles(), pEntCollision->OBBMins(), pEntCollision->OBBMaxs() ) )
			{
				pEntity->WakeRestingObjects();
				//pEntity->SetGroundEntity( NULL );

				if ( pEntity->GetMoveType() == MOVETYPE_VPHYSICS )
				{
					IPhysicsObject *pPhysicsObject = pEntity->VPhysicsGetObject();

					//Check if the reflective cube is in its disabled state and enable it
					if ( UTIL_IsReflectiveCube( pEntity ) || UTIL_IsSchrodinger( pEntity ) )
					{
						CPropWeightedCube *pReflectiveCube = assert_cast<CPropWeightedCube*>( pEntity );
						pReflectiveCube->ExitDisabledState();
					}

					if ( pPhysicsObject && pPhysicsObject->IsMoveable() )
					{
						pPhysicsObject->Wake();

						// If the target is debris, convert it to non-debris
						if ( pEntity->GetCollisionGroup() == COLLISION_GROUP_DEBRIS )
						{
							// Interactive debris converts back to debris when it comes to rest
							pEntity->SetCollisionGroup( COLLISION_GROUP_INTERACTIVE_DEBRIS );
						}
					}
				}
			}
		}
	}
}

void CPortal_Base2D::ForceEntityToFitInPortalWall( CBaseEntity *pEntity )
{
	CCollisionProperty *pCollision = pEntity->CollisionProp();
	Vector vWorldMins, vWorldMaxs;
	pCollision->WorldSpaceAABB( &vWorldMins, &vWorldMaxs );
	Vector ptCenter = pEntity->WorldSpaceCenter(); //(vWorldMins + vWorldMaxs) / 2.0f;
	Vector ptOrigin = pEntity->GetAbsOrigin();
	Vector vEntityCenterToOrigin = ptOrigin - ptCenter;


	Vector ptPortalCenter = GetAbsOrigin();
	Vector vPortalCenterToEntityCenter = ptCenter - ptPortalCenter;
	Vector vPortalForward;
	GetVectors( &vPortalForward, NULL, NULL );
	Vector ptProjectedEntityCenter = ptPortalCenter + ( vPortalForward * vPortalCenterToEntityCenter.Dot( vPortalForward ) );

	Vector ptDest;

	if ( m_PortalSimulator.IsReadyToSimulate() )
	{
		Ray_t ray;
		ray.Init( ptProjectedEntityCenter, ptCenter, vWorldMins - ptCenter, vWorldMaxs - ptCenter );

		trace_t ShortestTrace;
		ShortestTrace.fraction = 2.0f;

		const PS_SD_Static_Wall_Local_Brushes_t &WallBrushes = m_PortalSimulator.GetInternalData().Simulation.Static.Wall.Local.Brushes;

		int iTestMask = MASK_SOLID;
		if( pEntity->IsPlayer() )
		{
			iTestMask |= CONTENTS_PLAYERCLIP;
		}
		else if( pEntity->IsNPC() )
		{
			iTestMask |= CONTENTS_MONSTERCLIP;
		}

		for( int i = 0; i != ARRAYSIZE( WallBrushes.BrushSets ); ++i )
		{
			if( (WallBrushes.BrushSets[i].iSolidMask & iTestMask) != 0 )
			{
				trace_t TempTrace;
				physcollision->TraceBox( ray, m_PortalSimulator.GetInternalData().Simulation.Static.Wall.Local.Brushes.BrushSets[i].pCollideable, vec3_origin, vec3_angle, &TempTrace );
				if( TempTrace.fraction < ShortestTrace.fraction )
				{
					ShortestTrace = TempTrace;
				}
			}
		}

		if( ShortestTrace.fraction < 2.0f )
		{
			Vector ptNewPos = ShortestTrace.endpos + vEntityCenterToOrigin;
			pEntity->Teleport( &ptNewPos, NULL, NULL );
			pEntity->AddEffects( EF_NOINTERP );
#if !defined ( DISABLE_DEBUG_HISTORY )
			if ( !IsMarkedForDeletion() )
			{
				ADD_DEBUG_HISTORY( HISTORY_PLAYER_DAMAGE, UTIL_VarArgs( "Teleporting %s inside 'ForceEntityToFitInPortalWall'\n", pEntity->GetDebugName() ) );
			}
#endif
			if( sv_portal_debug_touch.GetBool() )
			{
				DevMsg( "Teleporting %s inside 'ForceEntityToFitInPortalWall'\n", pEntity->GetDebugName() );
			}
			//pEntity->SetAbsOrigin( ShortestTrace.endpos + vEntityCenterToOrigin );
		}
	}	
}

void CPortal_Base2D::UpdatePortalTeleportMatrix( void )
{
	//copied from client to ensure the numbers match as closely as possible.
	{		
		ALIGN16 matrix3x4_t finalMatrix;
		if( GetMoveParent() )
		{
			// Construct the entity-to-world matrix
			// Start with making an entity-to-parent matrix
			ALIGN16 matrix3x4_t matEntityToParent;
			AngleMatrix( GetLocalAngles(), matEntityToParent );
			MatrixSetColumn( GetLocalOrigin(), 3, matEntityToParent );

			// concatenate with our parent's transform
			ALIGN16 matrix3x4_t scratchMatrix;
			ConcatTransforms( GetParentToWorldTransform( scratchMatrix ), matEntityToParent, finalMatrix );

			MatrixGetColumn( finalMatrix, 0, m_vForward );
			MatrixGetColumn( finalMatrix, 1, m_vRight );
			MatrixGetColumn( finalMatrix, 2, m_vUp );
			Vector vTempOrigin;
			MatrixGetColumn( finalMatrix, 3, vTempOrigin );
			m_ptOrigin = vTempOrigin;
			m_vRight = -m_vRight;

			QAngle qTempAngle;
			MatrixAngles( finalMatrix, qTempAngle );
			m_qAbsAngle = qTempAngle;
		}
		else
		{
			AngleMatrix( m_qAbsAngle, finalMatrix );
			MatrixGetColumn( finalMatrix, 0, m_vForward );
			MatrixGetColumn( finalMatrix, 1, m_vRight );
			MatrixGetColumn( finalMatrix, 2, m_vUp );
			m_vRight = -m_vRight;
		}		
	}

	//setup our origin plane
	m_plane_Origin.normal = m_vForward;
	m_plane_Origin.dist = m_plane_Origin.normal.Dot( m_ptOrigin );
	m_plane_Origin.signbits = SignbitsForPlane( &m_plane_Origin );

	Vector vAbsNormal;
	vAbsNormal.x = fabs(m_plane_Origin.normal.x);
	vAbsNormal.y = fabs(m_plane_Origin.normal.y);
	vAbsNormal.z = fabs(m_plane_Origin.normal.z);

	if( vAbsNormal.x > vAbsNormal.y )
	{
		if( vAbsNormal.x > vAbsNormal.z )
		{
			if( vAbsNormal.x > 0.999f )
				m_plane_Origin.type = PLANE_X;
			else
				m_plane_Origin.type = PLANE_ANYX;
		}
		else
		{
			if( vAbsNormal.z > 0.999f )
				m_plane_Origin.type = PLANE_Z;
			else
				m_plane_Origin.type = PLANE_ANYZ;
		}
	}
	else
	{
		if( vAbsNormal.y > vAbsNormal.z )
		{
			if( vAbsNormal.y > 0.999f )
				m_plane_Origin.type = PLANE_Y;
			else
				m_plane_Origin.type = PLANE_ANYY;
		}
		else
		{
			if( vAbsNormal.z > 0.999f )
				m_plane_Origin.type = PLANE_Z;
			else
				m_plane_Origin.type = PLANE_ANYZ;
		}
	}

	UTIL_Portal_ComputeMatrix( this, m_hLinkedPortal.Get() );
}


void CPortal_Base2D::CreateMicAndSpeaker( void )
{
	RemovePortalMicAndSpeaker();

	inputdata_t inputdata;

	m_hMicrophone = CreateEntityByName( "env_microphone" );
	CEnvMicrophone *pMicrophone = static_cast<CEnvMicrophone*>( m_hMicrophone.Get() );
	pMicrophone->AddSpawnFlags( SF_MICROPHONE_IGNORE_NONATTENUATED );
	pMicrophone->AddSpawnFlags( SF_MICROPHONE_SOUND_COMBAT | SF_MICROPHONE_SOUND_WORLD | SF_MICROPHONE_SOUND_PLAYER | SF_MICROPHONE_SOUND_BULLET_IMPACT | SF_MICROPHONE_SOUND_EXPLOSION );
	DispatchSpawn( pMicrophone );

	m_hSpeaker = CreateEntityByName( "env_speaker" );
	CSpeaker *pSpeaker = static_cast<CSpeaker*>( m_hSpeaker.Get() );
	
	float flMicrophoneSensitivity = sv_portal_microphone_sensitivity.GetFloat();
	flMicrophoneSensitivity = clamp( flMicrophoneSensitivity, 0.0f, 10.0f );

	float flMicrophoneRange = sv_portal_microphone_max_range.GetFloat();
	if ( flMicrophoneRange < 0.0f )
	{
		flMicrophoneRange = 0.0f;
	}

	pSpeaker->SetName( MAKE_STRING( m_bIsPortal2 ? "PortalMicrophone_2" : "PortalMicrophone_1" ) );
	pMicrophone->SetName( MAKE_STRING( m_bIsPortal2 ? "PortalSpeaker_2" : "PortalSpeaker_1" ) );
	pMicrophone->Activate();
	pMicrophone->SetSensitivity( flMicrophoneSensitivity );
	pMicrophone->SetMaxRange( flMicrophoneRange );
	
	// Set microphone/speaker positions
	pMicrophone->AddSpawnFlags( SF_MICROPHONE_IGNORE_NONATTENUATED );
	pMicrophone->Teleport( &GetAbsOrigin(), &GetAbsAngles(), &vec3_origin );
	pSpeaker->Teleport( &GetAbsOrigin(), &GetAbsAngles(), &vec3_origin );
}

void CPortal_Base2D::UpdatePortalLinkage( void )
{
	if( IsActive() )
	{
		CPortal_Base2D *pLink = m_hLinkedPortal.Get();

		if( pLink != NULL )
		{
			CHandle<CPortal_Base2D> hThis = this;
			CHandle<CPortal_Base2D> hRemote = pLink;

			this->m_hLinkedPortal = hRemote;
			pLink->m_hLinkedPortal = hThis;
			m_bIsPortal2 = !m_hLinkedPortal->m_bIsPortal2;

			// Link up mic and speakers to remote portal
			// NOTE: This does the work for both portals
			if( sv_portal_enable_microphone.GetInt() )
			{

				if ( m_bMicAndSpeakersLinkedToRemote == false )
				{
					// Initialize mics/speakers
					if( m_hMicrophone.Get() == NULL || m_hSpeaker.Get() == NULL )
					{

						CreateMicAndSpeaker();
					}

					if ( m_hLinkedPortal->m_hMicrophone.Get() == NULL || m_hSpeaker.Get() == NULL )
					{
						m_hLinkedPortal->CreateMicAndSpeaker();
					}

					// Cross link the mics and speakers
					CEnvMicrophone* pMyMic = dynamic_cast<CEnvMicrophone*>( m_hMicrophone.Get() );
					CEnvMicrophone* pRemoteMic = dynamic_cast<CEnvMicrophone*>( m_hLinkedPortal->m_hMicrophone.Get() );
					
					Assert( pMyMic && pRemoteMic && m_hSpeaker.Get() && m_hLinkedPortal->m_hSpeaker.Get() );
					if ( pMyMic )
					{
						pMyMic->SetSpeaker( m_hLinkedPortal->m_hSpeaker->GetEntityName(), m_hLinkedPortal->m_hSpeaker );
					}
					if ( pRemoteMic )
					{
						pRemoteMic->SetSpeaker( m_hSpeaker->GetEntityName(), m_hSpeaker );
					}

					m_bMicAndSpeakersLinkedToRemote = true;
					m_hLinkedPortal->m_bMicAndSpeakersLinkedToRemote = true;
				}
			}

			UpdatePortalTeleportMatrix();
		}
		else
		{
			m_PortalSimulator.DetachFromLinked();
			m_PortalSimulator.ReleaseAllEntityOwnership();

			if ( m_hMicrophone.Get() )
			{
				CEnvMicrophone* pMyMic = dynamic_cast<CEnvMicrophone*>( m_hMicrophone.Get() );
				if ( pMyMic )
				{
					pMyMic->SetSpeaker( NULL_STRING, NULL );
				}
			}
			m_bMicAndSpeakersLinkedToRemote = false;

		}

		
		m_PortalSimulator.SetSize( GetHalfWidth(), GetHalfHeight() );
		m_PortalSimulator.MoveTo( m_ptOrigin, m_qAbsAngle );

		if( pLink )
		{
			m_PortalSimulator.AttachTo( &pLink->m_PortalSimulator );
			if( IsMobile() || pLink->IsMobile() )
			{
				SetSize( GetLocalMins(), Vector( 4.0f, m_fNetworkHalfWidth, m_fNetworkHalfHeight ) );
				pLink->SetSize( GetLocalMins(), Vector( 4.0f, m_fNetworkHalfWidth, m_fNetworkHalfHeight ) );
			}
			else
			{
				SetSize( GetLocalMins(), GetLocalMaxs() );
				pLink->SetSize( GetLocalMins(), GetLocalMaxs() );
			}
		}

		if( m_pAttachedCloningArea )
			m_pAttachedCloningArea->UpdatePosition();
	}
	else
	{
		CPortal_Base2D *pRemote = m_hLinkedPortal;
		//apparently we've been deactivated
		m_PortalSimulator.DetachFromLinked();
		m_PortalSimulator.ReleaseAllEntityOwnership();

		m_hLinkedPortal = NULL;
		if( pRemote )
		{
			pRemote->UpdatePortalLinkage();
		}
	}

	if( m_bIsPortal2 )
	{
		m_PortalSimulator.EditDebuggingData().overlayColor.SetColor( 255, 0, 0, 255 );
	}
	else
	{
		m_PortalSimulator.EditDebuggingData().overlayColor.SetColor( 0, 0, 255, 255 );
	}

// 79483: We have a handle on perf now and don't need to dump this every portal placement.
#if 0
#if !defined( _CERT ) 
	// Make sure we have the last state of all portals in the console log, in case of a crash
	PortalReportFunc( true );
#endif // !_CERT
#endif
}


//#define STACK_ANALYZE_PORTALPLACEMENT

#if defined( STACK_ANALYZE_PORTALPLACEMENT )
struct PortalPlacementInfo_t
{
	DECLARE_CALLSTACKSTATSTRUCT();
	DECLARE_CALLSTACKSTATSTRUCT_FIELDDESCRIPTION();

	int iEntered;
	int iActuallyMoved;
	int iMaxTest;
	int iMinTest;
};

BEGIN_STATSTRUCTDESCRIPTION( PortalPlacementInfo_t )
	WRITE_STATSTRUCT_FIELDDESCRIPTION();
	//WRITE_STATSTRUCT_FIELDMERGESCRIPT( SSMSL_Squirrel );
END_STATSTRUCTDESCRIPTION()

BEGIN_STATSTRUCTFIELDDESCRIPTION( PortalPlacementInfo_t )
	DEFINE_STATSTRUCTFIELD( iEntered, BasicStatStructFieldDesc, ( BSSFT_INT32, BSSFCM_ADD ) )
	DEFINE_STATSTRUCTFIELD( iActuallyMoved, BasicStatStructFieldDesc, ( BSSFT_INT32, BSSFCM_ADD ) )
	DEFINE_STATSTRUCTFIELD( iMaxTest, BasicStatStructFieldDesc, ( BSSFT_INT32, BSSFCM_MAX ) )
	DEFINE_STATSTRUCTFIELD( iMinTest, BasicStatStructFieldDesc, ( BSSFT_INT32, BSSFCM_MIN ) )
END_STATSTRUCTFIELDDESCRIPTION()

CCallStackStatsGatherer<PortalPlacementInfo_t, 32, GetCallStack_Fast> s_PortalPlacementStats;
CCallStackStatsGatherer_Standardized_t g_PlacementStats = s_PortalPlacementStats;

void DumpPlacementStats()
{
	s_PortalPlacementStats.DumpToFile( "PlacementStats.vcsf" );
}

static ConCommand dump_placement( "dump_placement", DumpPlacementStats, "", FCVAR_CHEAT );
#endif



void CPortal_Base2D::NewLocation( const Vector &vOrigin, const QAngle &qAngles )
{
#if defined( STACK_ANALYZE_PORTALPLACEMENT )
	CCallStackStatsGatherer_StructAccessor_AutoLock<PortalPlacementInfo_t> entry = s_PortalPlacementStats.GetEntry();
	++entry->iEntered;
	entry->iMaxTest = RandomInt(0, 10000);
	entry->iMinTest = RandomInt(0, 10000);
	if( vOrigin != m_ptOrigin || qAngles != m_qAbsAngle )
	{
		++entry->iActuallyMoved;
	}
#endif

	// Tell our physics environment to stop simulating it's entities.
	// Fast moving objects can pass through the hole this frame while it's in the old location.
	m_PortalSimulator.ReleaseAllEntityOwnership();
	Vector vOldForward;
	GetVectors( &vOldForward, 0, 0 );

	m_vPrevForward = vOldForward;

	SetParent( NULL );
	SetAbsVelocity( vec3_origin );
	WakeNearbyEntities();

	SetMobileState( false );
	Teleport( &vOrigin, &qAngles, 0 );

	m_ptOrigin = vOrigin;
	m_qAbsAngle = qAngles;

	if ( m_hMicrophone )
	{
		CEnvMicrophone *pMicrophone = static_cast<CEnvMicrophone*>( m_hMicrophone.Get() );
		pMicrophone->Teleport( &vOrigin, &qAngles, 0 );
		inputdata_t inMicEnable;
		pMicrophone->InputEnable( inMicEnable );
	}

	if ( m_hSpeaker )
	{
		CSpeaker *pSpeaker = static_cast<CSpeaker*>( m_hSpeaker.Get() );
		pSpeaker->Teleport( &vOrigin, &qAngles, 0 );
		inputdata_t inTurnOn;
		pSpeaker->InputTurnOn( inTurnOn );
	}

	//if the other portal should be static, let's not punch stuff resting on it
	bool bOtherShouldBeStatic = false;
	if( !m_hLinkedPortal )
		bOtherShouldBeStatic = true;

	if ( IsActive() )
	{
		BroadcastPortalEvent( PORTALEVENT_MOVED );
	}

	SetActive( true );

	UpdatePortalLinkage();
	UpdatePortalTeleportMatrix();

	// Update the four corners of this portal for faster reference
	UpdateCorners();

	WakeNearbyEntities();

	// Make sure it's not a floor portal... allowing those to punch creates a floor to floor exploit
	if ( !IsFloorPortal() )
	{
		if ( m_hLinkedPortal )
		{
			m_hLinkedPortal->WakeNearbyEntities();
			if ( !bOtherShouldBeStatic ) 
			{
				m_hLinkedPortal->PunchAllPenetratingPlayers();
			}
		}
	}

	UpdateClientCheckPVS();

	CBaseEntity *pAttachedToMovingEntity;
	
	//check to see if we landed on an entity that could potentially move
	{
		trace_t tr;
		Vector vForward, vUp;
		AngleVectors( qAngles, &vForward, NULL, &vUp );
		UTIL_TraceLine( vOrigin + vForward * 5.0f, vOrigin - vForward * 10.0f, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
		
		if( tr.m_pEnt && ((tr.m_pEnt->GetMoveType() != MOVETYPE_NONE) || (tr.m_pEnt->GetParent() != NULL)) )
		{
			pAttachedToMovingEntity = tr.m_pEnt;

			QAngle qNewAngles; //recompute the angles since they may have changed between when we fired and when we landed
			VectorAngles( vForward, vUp, qNewAngles );
			SetAbsAngles( qNewAngles );
			SetAbsOrigin( tr.endpos );
			m_ptOrigin = tr.endpos;
			m_qAbsAngle = qNewAngles;
		}
		else
		{
			pAttachedToMovingEntity = NULL;
		}
	}

	if( pAttachedToMovingEntity )
	{
		SetMoveType( MOVETYPE_NOCLIP ); //switch the movetype to something that gets CPortal_Base2D::PhysicsSimulate() called.
		SetParent( pAttachedToMovingEntity );
	}
	else
	{
		SetMoveType( MOVETYPE_NONE );
	}

	m_PortalSimulator.SetCarvedParent( pAttachedToMovingEntity );

	SetMobileState( sv_allow_mobile_portals.GetBool() && (pAttachedToMovingEntity != NULL) && UTIL_IsEntityMovingOrRotating( pAttachedToMovingEntity ) );

	m_vPortalSpawnLocation = m_ptOrigin;
	PhysicsTouchTriggers( NULL );
}

void CPortal_Base2D::PhysicsSimulate( void )
{
	BaseClass::PhysicsSimulate();

	//update as if placed in a new position if we're mobile
	bool bMoving = GetParent() && UTIL_IsEntityMovingOrRotating( GetParent() );
	bool bPortalMoving = m_vPortalSpawnLocation.IsValid() && m_vPortalSpawnLocation.DistToSqr( GetAbsOrigin() ) > 0.1f;

	if( ( bMoving || bPortalMoving ) && !sv_allow_mobile_portals.GetBool() )
	{
		DeactivatePortalOnThink();
		SetMoveType( MOVETYPE_NONE );
		SetParent( NULL );
		return;
	}
	
	SetMobileState( bMoving );
	
	if( bMoving )
	{
		UpdatePortalLinkage(); //this needs to change names or something. It does so much more than just linkage
		UpdatePortalTeleportMatrix();
		m_PortalSimulator.MoveTo( m_ptOrigin, m_qAbsAngle );
		UpdateCorners();

#if 0 //mobile portal debugging code
		NDebugOverlay::EntityBounds( this, 0, 255, 0, 50, 10.0f );

		CPortal_Base2D *pLinked = m_hLinkedPortal;
		if( m_hLinkedPortal )
		{			
			Vector vTransformedOrigin = pLinked->m_PortalSimulator.m_DataAccess.Placement.matThisToLinked * pLinked->m_PortalSimulator.m_DataAccess.Placement.ptCenter;
			Vector vTransformedForward = pLinked->m_PortalSimulator.m_DataAccess.Placement.matThisToLinked.ApplyRotation( pLinked->m_PortalSimulator.m_DataAccess.Placement.vForward );

			NDebugOverlay::Line( vTransformedOrigin, vTransformedOrigin + vTransformedForward * (-50.0f), 100, 0, 0, true, 10.0f );
		}
#endif
	}
}


void CPortal_Base2D::SetMobileState( bool bSet )
{
	if( m_bIsMobile != bSet )
	{
		m_bIsMobile = bSet;
		if( bSet )
		{
			//disable physics if it's setup
			//m_PortalSimulator.DetachFromLinked();
			m_PortalSimulator.SetCollisionGenerationEnabled( false );
			m_PortalSimulator.ReleaseAllEntityOwnership();
			if( m_hLinkedPortal.Get() != NULL )
			{
				m_hLinkedPortal->m_PortalSimulator.ReleaseAllEntityOwnership();
			}
			
			if( IsActive() && (GetNextThink( s_szTestRestingSurfaceThinkContext ) == TICK_NEVER_THINK) )
				SetContextThink( &CPortal_Base2D::TestRestingSurfaceThink, gpGlobals->curtime + 0.1f, s_szTestRestingSurfaceThinkContext );
		}
		else
		{
			//re-enable physics if we're supposed to have it working
			m_PortalSimulator.SetCollisionGenerationEnabled( true );
			
			if( IsActive() )
			{
				UpdatePortalTeleportMatrix();
				m_PortalSimulator.MoveTo( m_ptOrigin, m_qAbsAngle );
				m_PortalSimulator.SetSize( GetHalfWidth(), GetHalfHeight() );

				CPortal_Base2D *pLink = m_hLinkedPortal.Get();
				if( pLink && !pLink->m_bIsMobile )
					m_PortalSimulator.AttachTo( &pLink->m_PortalSimulator );
			}
		}

		bool bSmallSize = bSet;

		CPortal_Base2D *pLinked = m_hLinkedPortal;
		if( pLinked )
		{
			bSmallSize |= pLinked->IsMobile();

			pLinked->SetSize( pLinked->GetLocalMins(), bSmallSize ? Vector( 4.0f, pLinked->m_fNetworkHalfWidth, pLinked->m_fNetworkHalfHeight ) : pLinked->GetLocalMaxs() );
		}

		SetSize( GetLocalMins(), bSmallSize ? Vector( 4.0f, m_fNetworkHalfWidth, m_fNetworkHalfHeight ) : GetLocalMaxs() );
	}	
}


void CPortal_Base2D::Resize( float fHalfWidth, float fHalfHeight )
{
	if( (fHalfWidth == m_fNetworkHalfWidth) && (fHalfHeight == m_fNetworkHalfHeight) )
		return;

	m_fNetworkHalfWidth = fHalfWidth;
	m_fNetworkHalfHeight = fHalfHeight;

	CPortal_Base2D *pLinked = m_hLinkedPortal;
	if( pLinked )
	{
		if( (m_fNetworkHalfWidth != pLinked->m_fNetworkHalfWidth) || (m_fNetworkHalfHeight != pLinked->m_fNetworkHalfHeight) )
		{
			//different portal sizes, unsupported, unlink. Scaling is a whole different ball of wax.
			//if you're resizing both portals. They'll find eachother in UpdatePortalLinkage() once they're both resized.
			m_hLinkedPortal = NULL;
			m_PortalSimulator.DetachFromLinked();
			pLinked->m_hLinkedPortal = NULL;
			pLinked->m_PortalSimulator.DetachFromLinked();
		}
	}
	
	UpdateCollisionShape();
	if( m_pAttachedCloningArea )
		m_pAttachedCloningArea->Resize( fHalfWidth, fHalfHeight );

	m_PortalSimulator.SetSize( fHalfWidth, fHalfHeight );

	if( pLinked )
		pLinked->UpdatePortalLinkage();
	
	UpdatePortalLinkage();

	CBaseProjector::TestAllForProjectionChanges();
}


void CPortal_Base2D::UpdateClientCheckPVS( void )
{

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CPortal_Player	*pPlayer = dynamic_cast<CPortal_Player*>(UTIL_PlayerByIndex( i ));
		if ( pPlayer == NULL )
			continue;

		pPlayer->MarkClientCheckPVSDirty();
	}
}

void CPortal_Base2D::UpdateCorners()
{
	Vector vOrigin = m_ptOrigin;
	Vector vUp, vRight;
	GetVectors( NULL, &vRight, &vUp );
	vRight *= m_fNetworkHalfWidth;
	vUp *= m_fNetworkHalfHeight;

	m_vPortalCorners[0] = (vOrigin + vRight) + vUp;
	m_vPortalCorners[1] = (vOrigin - vRight) + vUp;
	m_vPortalCorners[2] = (vOrigin - vRight) - vUp;
	m_vPortalCorners[3] = (vOrigin + vRight) - vUp;
}

//-----------------------------------------------------------------------------
// Purpose: Tell all listeners about an event that just occurred 
//-----------------------------------------------------------------------------
void CPortal_Base2D::OnPortalDeactivated( void )
{
	BroadcastPortalEvent( PORTALEVENT_FIZZLE );
	CBaseProjector::TestAllForProjectionChanges();
}

//-----------------------------------------------------------------------------
// Purpose: Tell all listeners about an event that just occurred 
//-----------------------------------------------------------------------------
void CPortal_Base2D::BroadcastPortalEvent( PortalEvent_t nEventType )
{
	/*
	switch( nEventType )
	{
	case PORTALEVENT_MOVED:
		Msg("[ Portal moved ]\n");
		break;
	
	case PORTALEVENT_FIZZLE:
		Msg("[ Portal fizzled ]\n");
		break;
	
	case PORTALEVENT_LINKED:
		Msg("[ Portal linked ]\n");
		break;
	}
	*/

	// We need to walk the list backwards because callers can remove themselves from our list as they're notified
	for ( int i = m_PortalEventListeners.Count()-1; i >= 0; i-- )
	{
		if ( m_PortalEventListeners[i] == NULL )
			continue;

		m_PortalEventListeners[i]->NotifyPortalEvent( nEventType, this );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Add a listener to our collection
//-----------------------------------------------------------------------------
void CPortal_Base2D::AddPortalEventListener( EHANDLE hListener )
{
	// Don't multiply add
	if ( m_PortalEventListeners.Find( hListener ) != m_PortalEventListeners.InvalidIndex() )
		return;

	m_PortalEventListeners.AddToTail( hListener );
}

//-----------------------------------------------------------------------------
// Purpose: Remove a listener to our collection
//-----------------------------------------------------------------------------
void CPortal_Base2D::RemovePortalEventListener( EHANDLE hListener )
{
	m_PortalEventListeners.FindAndFastRemove( hListener );
}

// Adds the PVS of the cluster where the portal's partner is placed to the parameter PVS.
// NOTE: adds the *LINKED* portal's cluster, not the parameter portal.
void AddPortalVisibilityToPVS( CPortal_Base2D* pPortal, int outputpvslength, unsigned char *outputpvs )
{
	Assert( pPortal );
	if ( pPortal && pPortal->IsActivedAndLinked() )
	{
		CPortal_Base2D* pLinked = pPortal->m_hLinkedPortal.Get();
		int iCluster = engine->GetClusterForOrigin( pLinked->GetAbsOrigin() );

		// get the pvs for the linked portal's cluster
		byte	pvs[MAX_MAP_LEAFS/8];	
		engine->GetPVSForCluster( iCluster, sizeof( pvs ), pvs );

		// Do the bulk on blocks of 4
		uint32 nDWords = outputpvslength / 4;
		uint32 *pInputDWords = (uint32*)pvs;
		uint32 *RESTRICT pOutputDWords = (uint32*)outputpvs;
		for ( int i=0; i<nDWords; ++i )
		{
			pOutputDWords[ i ] |= pInputDWords[ i ];
		}

		// Do the remaining (up to 3) in bytes
		for ( int i=nDWords * 4; i<outputpvslength; ++i )
		{
			outputpvs[ i ] |= pvs[ i ];
		}
	}
}

CServerNetworkProperty *CPortal_Base2D::GetExtenderNetworkProp( void )
{
	return NetworkProp();
}

const edict_t *CPortal_Base2D::GetExtenderEdict( void ) const
{
	return edict();
}

Vector CPortal_Base2D::GetExtensionPVSOrigin( void )
{
	return GetAbsOrigin() + m_plane_Origin.normal;
}

bool CPortal_Base2D::IsExtenderValid( void )
{
	return IsActivedAndLinked();
}

int CPortal_Base2D::GetPolyVertCount( void )
{
	return 4;
}

int CPortal_Base2D::ComputeFrustumThroughPolygon( const Vector &vVisOrigin, const VPlane *pInputFrustum, int iInputFrustumPlanes, VPlane *pOutputFrustum, int iOutputFrustumMaxPlanes )
{
	int iReturnedPlanes = UTIL_CalcFrustumThroughConvexPolygon( m_vPortalCorners, 4, vVisOrigin, pInputFrustum, iInputFrustumPlanes, pOutputFrustum, iOutputFrustumMaxPlanes, 0 );
	
	if( (iReturnedPlanes < iOutputFrustumMaxPlanes) && (iReturnedPlanes != 0) )
	{
		//add the portal plane as a near plane
		pOutputFrustum[iReturnedPlanes].Init( -m_plane_Origin.normal, -m_plane_Origin.dist );
		++iReturnedPlanes;
	}

	return iReturnedPlanes;
}


//////////////////////////////////////////////////////////////////////////
// AddPortalCornersToEnginePVS
// Subroutine to wrap the adding of portal corners to the PVS which is called once for the setup of each portal.
// input - pPortal: the portal we are viewing 'out of' which needs it's corners added to the PVS
//////////////////////////////////////////////////////////////////////////
void AddPortalCornersToEnginePVS( CPortal_Base2D* pPortal )
{
	Assert ( pPortal );

	if ( !pPortal )
		return;

	Vector vForward, vRight, vUp;
	pPortal->GetVectors( &vForward, &vRight, &vUp );

	// Center of the remote portal
	Vector ptOrigin			= pPortal->GetAbsOrigin();

	// Distance offsets to the different edges of the portal... Used in the placement checks
	Vector vToTopEdge = vUp * ( pPortal->GetHalfHeight() - PORTAL_BUMP_FORGIVENESS );
	Vector vToBottomEdge = -vToTopEdge;
	Vector vToRightEdge = vRight * ( pPortal->GetHalfWidth() - PORTAL_BUMP_FORGIVENESS );
	Vector vToLeftEdge = -vToRightEdge;

	// Distance to place PVS points away from portal, to avoid being in solid
	Vector vForwardBump		= vForward * 1.0f;

	// Add center and edges to the engine PVS
	engine->AddOriginToPVS( ptOrigin + vForwardBump);
	engine->AddOriginToPVS( ptOrigin + vToTopEdge + vToLeftEdge + vForwardBump );
	engine->AddOriginToPVS( ptOrigin + vToTopEdge + vToRightEdge + vForwardBump );
	engine->AddOriginToPVS( ptOrigin + vToBottomEdge + vToLeftEdge + vForwardBump );
	engine->AddOriginToPVS( ptOrigin + vToBottomEdge + vToRightEdge + vForwardBump );
}

void CPortal_Base2D::ComputeSubVisibility( CPVS_Extender **pExtenders, int iExtenderCount, unsigned char *outputPVS, int pvssize, const Vector &vVisOrigin, const VPlane *pVisFrustum, int iVisFrustumPlanes, VisExtensionChain_t *pVisChain, int iAreasNetworked[MAX_MAP_AREAS], int iMaxRecursionsLeft )
{
	if( iAreasNetworked[MAX_MAP_AREAS - 1] != -1 ) //early out, can't add any more data if we wanted to
		return;

	if( m_plane_Origin.normal.Dot( vVisOrigin ) < m_plane_Origin.dist )
		return; //vis origin is behind the portal plane	

	//both test if the portal is within the view frustum, and calculate the new one at the same time
	int iFrustumPlanesMax = (iVisFrustumPlanes + GetPolyVertCount() + 1);
	VPlane *pNewFrustum = (VPlane *)stackalloc( sizeof( VPlane ) * iFrustumPlanesMax );

	int iNewFrustumPlanes = ComputeFrustumThroughPolygon( vVisOrigin, pVisFrustum, iVisFrustumPlanes, pNewFrustum, iFrustumPlanesMax );
	if( iNewFrustumPlanes == 0 )
	{
		//NDebugOverlay::EntityBounds( this, 255, 0, 0, 100, 0.0f );
		return;
	}

	//NDebugOverlay::EntityBounds( this, 0, 255, 0, 100, 0.0f );

	CPortal_Base2D *pLinkedPortal = m_hLinkedPortal.Get();

	int iLinkedArea = pLinkedPortal->NetworkProp()->AreaNum();

	unsigned char *pLinkedPVS = pLinkedPortal->m_pExtenderData->iPVSBits;

	if( !pLinkedPortal->m_pExtenderData->bAddedToPVSAlready )
	{
		bool bFound = false;
		for( int i = 0; i != MAX_MAP_AREAS; ++i )
		{
			if( iAreasNetworked[i] == iLinkedArea )
			{
				bFound = true;
				break;
			}

			if( iAreasNetworked[i] == -1 )
			{
				bFound = true; //we found it by adding it
				iAreasNetworked[i] = iLinkedArea;
				int iOutputPVSIntSize = pvssize / sizeof( unsigned int );
				for( int j = 0; j != iOutputPVSIntSize; ++j )
				{
					((unsigned int *)outputPVS)[j] |= ((unsigned int *)pLinkedPVS)[j];
				}
				for( int j = iOutputPVSIntSize * sizeof( unsigned int ); j != pvssize; ++j )
				{
					outputPVS[j] |= pLinkedPVS[j];
				}
				break;
			}
		}
	
		AddPortalCornersToEnginePVS( pLinkedPortal );
		pLinkedPortal->m_pExtenderData->bAddedToPVSAlready = true;
	}
	
	--iMaxRecursionsLeft;
	if( iMaxRecursionsLeft == 0 )
		return;

	edict_t *linkedPortalEdict = pLinkedPortal->edict();

	VisExtensionChain_t chainNode;
	chainNode.m_nArea = iLinkedArea;
	chainNode.pParentChain = pVisChain;
	

	//transform vis origin to linked space
	Vector vTransformedVisOrigin = m_matrixThisToLinked * vVisOrigin;

	Vector vTranslation = m_matrixThisToLinked.GetTranslation();
	
	//transform the planes into the linked portal space
	for( int i = 0; i != iNewFrustumPlanes; ++i )
	{
		pNewFrustum[i].m_Normal = m_matrixThisToLinked.ApplyRotation( pNewFrustum[i].m_Normal );
		pNewFrustum[i].m_Dist += pNewFrustum[i].m_Normal.Dot( vTranslation );
	}

	Assert( pLinkedPVS != NULL );

	//extend the vis by what the linked portal can see
	for( int i = 0; i != iExtenderCount; ++i )
	{
		CPVS_Extender *pExtender = pExtenders[i];

		if ( pExtender->GetExtenderEdict() == linkedPortalEdict )
			continue;

		if ( pExtender->GetExtenderNetworkProp()->IsInPVS( linkedPortalEdict, pLinkedPVS, (MAX_MAP_LEAFS/8) ) ) //test against linked portal PVS, not aggregate PVS
		{
			chainNode.pExtender = pExtender;
			pExtender->ComputeSubVisibility( pExtenders, iExtenderCount, outputPVS, pvssize, vTransformedVisOrigin, pNewFrustum, iNewFrustumPlanes, &chainNode, iAreasNetworked, iMaxRecursionsLeft );			
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Notify this the supplied entity has teleported to this portal
//-----------------------------------------------------------------------------
void CPortal_Base2D::OnEntityTeleportedToPortal( CBaseEntity *pEntity )
{
	m_OnEntityTeleportToMe.FireOutput( this, this );
	BroadcastPortalEvent( PORTALEVENT_ENTITY_TELEPORTED_TO );

	if ( pEntity->IsPlayer() )
	{
		m_OnPlayerTeleportToMe.FireOutput( this, this );
		BroadcastPortalEvent( PORTALEVENT_PLAYER_TELEPORTED_TO );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Notify this the supplied entity has teleported from this portal
//-----------------------------------------------------------------------------
void CPortal_Base2D::OnEntityTeleportedFromPortal( CBaseEntity *pEntity )
{
	m_OnEntityTeleportFromMe.FireOutput( this, this );
	BroadcastPortalEvent( PORTALEVENT_ENTITY_TELEPORTED_FROM );

	if ( pEntity->IsPlayer() )
	{
		m_OnPlayerTeleportFromMe.FireOutput( this, this );
		BroadcastPortalEvent( PORTALEVENT_PLAYER_TELEPORTED_FROM );
	}
}


void EntityPortalled( CPortal_Base2D *pPortal, CBaseEntity *pOther, const Vector &vNewOrigin, const QAngle &qNewAngles, bool bForcedDuck )
{
	/*if( pOther->IsPlayer() )
	{
		Warning( "Server player portalled %f   %f %f %f   %f %f %f\n", gpGlobals->curtime, XYZ( vNewOrigin ), XYZ( ((CPortal_Player *)pOther)->pl.v_angle ) ); 
	}*/

	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CPortal_Player *pPlayer = (CPortal_Player *)UTIL_PlayerByIndex( i );
		if( pPlayer )
		{
			pPlayer->NetworkPortalTeleportation( pOther, pPortal, gpGlobals->curtime, bForcedDuck );
		}
	}
}

void DebugPortalCollideables_f( const CCommand &command )
{
	for( int i = 0; i != CPortal_Base2D_Shared::AllPortals.Count(); ++i )
	{
		CPortal_Base2D *pPortal = CPortal_Base2D_Shared::AllPortals[i];
		pPortal->m_PortalSimulator.DebugCollisionOverlay( true, 30.0f );
	}
}

static ConCommand debugportalcollideables("debugportalcollideables", DebugPortalCollideables_f, "Dump all CPhysCollides for all portals to the debug overlay" );
