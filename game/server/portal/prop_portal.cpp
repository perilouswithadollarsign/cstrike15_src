//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "prop_portal_shared.h"
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
#include "weapon_portalgun.h"
#include "portal_placement.h"
#include "physicsshadowclone.h"
#include "particle_parse.h"
#include "rumble_shared.h"
#include "func_portal_orientation.h"
#include "env_debughistory.h"
#include "tier1/callqueue.h"
#include "baseprojector.h"
#include "tier1/convar.h"
#include "iextpropportallocator.h"
#include "matchmaking/imatchframework.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef PORTAL2
extern bool UTIL_FizzlePlayerPhotos( CPortal_Player *pPlayer );
#endif // PORTAL2

static CUtlVector<CProp_Portal *> s_PortalLinkageGroups[256];
const char *CProp_Portal::s_szDelayedPlacementThinkContext = "CProp_Portal::DelayedPlacementThink";
extern ConVar sv_portal_placement_never_fail;
extern ConVar use_server_portal_particles;


BEGIN_DATADESC( CProp_Portal )
	//saving
	DEFINE_KEYFIELD( m_iLinkageGroupID,	FIELD_CHARACTER,	"LinkageGroupID" ),
	DEFINE_KEYFIELD( m_bActivated,		FIELD_BOOLEAN,		"Activated" ),
	DEFINE_KEYFIELD( m_bOldActivatedState,		FIELD_BOOLEAN,		"OldActivated" ),	
	DEFINE_KEYFIELD( m_bIsPortal2,		FIELD_BOOLEAN,		"PortalTwo" ),

	DEFINE_FIELD( m_NotifyOnPortalled, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hFiredByPlayer, FIELD_EHANDLE ),

	DEFINE_SOUNDPATCH( m_pAmbientSound ),

		// Function Pointers
	DEFINE_THINKFUNC( DelayedPlacementThink ),

	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "SetActivatedState", InputSetActivatedState ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Fizzle", InputFizzle ),
	DEFINE_INPUTFUNC( FIELD_STRING, "NewLocation", InputNewLocation ),
	DEFINE_INPUTFUNC( FIELD_STRING, "Resize", InputResize ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "SetLinkageGroupId", InputSetLinkageGroupId ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CProp_Portal, DT_Prop_Portal )
	SendPropEHandle( SENDINFO( m_hFiredByPlayer ) ),
	SendPropInt( SENDINFO( m_nPlacementAttemptParity ), EF_PARITY_BITS, SPROP_UNSIGNED ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( prop_portal, CProp_Portal );



CProp_Portal::CProp_Portal( void )
{
	if( !ms_DefaultPortalSizeInitialized )
	{
		ms_DefaultPortalSizeInitialized = true; // for CEG protection
		CEG_GCV_PRE();
		ms_DefaultPortalHalfHeight = CEG_GET_CONSTANT_VALUE( DefaultPortalHalfHeight ); // only protecting one to reduce the cost of first-portal check
		CEG_GCV_POST();
	}
	m_FizzleEffect = PORTAL_FIZZLE_KILLED;
	CProp_Portal_Shared::AllPortals.AddToTail( this );
}

CProp_Portal::~CProp_Portal( void )
{
	CProp_Portal_Shared::AllPortals.FindAndRemove( this );
	s_PortalLinkageGroups[m_iLinkageGroupID].FindAndRemove( this );
}

void CProp_Portal::Precache( void )
{
	PrecacheScriptSound( "Portal.ambient_loop" );

	PrecacheScriptSound( "Portal.open_blue" );
	PrecacheScriptSound( "Portal.open_red" );
	PrecacheScriptSound( "Portal.close_blue" );
	PrecacheScriptSound( "Portal.close_red" );
	PrecacheScriptSound( "Portal.fizzle_moved" );
	PrecacheScriptSound( "Portal.fizzle_invalid_surface" );

	PrecacheModel( "models/portals/portal1.mdl" );
	PrecacheModel( "models/portals/portal2.mdl" );

	//PrecacheParticleSystem( "portal_1_particles" );
	//PrecacheParticleSystem( "portal_2_particles" );
	//PrecacheParticleSystem( "portal_1_edge" );
	//PrecacheParticleSystem( "portal_2_edge" );
	//PrecacheParticleSystem( "portal_1_close" );
	//PrecacheParticleSystem( "portal_2_close" );
	//PrecacheParticleSystem( "portal_1_badsurface" );
	//PrecacheParticleSystem( "portal_2_badsurface" );	
	//PrecacheParticleSystem( "portal_1_success" );
	//PrecacheParticleSystem( "portal_2_success" );

	// adjustable color for coop, two colorable systems instead of four unique -mtw
	// need two systems here because they spin different directions
	PrecacheParticleSystem( "portal_edge" );
	PrecacheParticleSystem( "portal_edge_reverse" );
	PrecacheParticleSystem( "portal_close" );
	PrecacheParticleSystem( "portal_badsurface" );
	PrecacheParticleSystem( "portal_success" );

	BaseClass::Precache();
}

void CProp_Portal::CreateSounds()
{
	if (!m_pAmbientSound)
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		CPASAttenuationFilter filter( this );

		m_pAmbientSound = controller.SoundCreate( filter, entindex(), "Portal.ambient_loop" );
		controller.Play( m_pAmbientSound, 0, 100 );
	}
}

void CProp_Portal::StopLoopingSounds()
{
	if ( m_pAmbientSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		controller.SoundDestroy( m_pAmbientSound );
		m_pAmbientSound = NULL;
	}

	BaseClass::StopLoopingSounds();
}

class CPortalServerDllPropPortalLocator : public IPortalServerDllPropPortalLocator
{
public:
	virtual void LocateAllPortals( CUtlVector<PortalInfo_t> &arrPortals )
	{
		for ( int iLinkageGroupID = 0; iLinkageGroupID < 3; ++iLinkageGroupID )
		{
			for ( int nPortal = 0; nPortal < 2; ++nPortal )
			{
				CProp_Portal *pPortal = CProp_Portal::FindPortal( iLinkageGroupID, (nPortal != 0), false );
				if ( !pPortal )
					continue;

				const Vector &vecOrigin = pPortal->GetAbsOrigin();
				const QAngle &vecAngle = pPortal->GetAbsAngles();
				
				PortalInfo_t pi;
				pi.iLinkageGroupId = iLinkageGroupID;
				pi.nPortal = nPortal;
				pi.vecOrigin = vecOrigin;
				pi.vecAngle = vecAngle;
				arrPortals.AddToTail( pi );
			}
		}
	}
} s_PortalServerDllPropPortalLocator;

void CProp_Portal::Spawn( void )
{
	Precache();

	AddToLinkageGroup();

	ResetModel();
	if( (GetHalfWidth() <= 0) || (GetHalfHeight() <= 0) )
		Resize( ms_DefaultPortalHalfWidth, ms_DefaultPortalHalfHeight );

	BaseClass::Spawn();

	static bool s_bPortalLocatorForClientRegistered;
	if ( !s_bPortalLocatorForClientRegistered && g_pMatchFramework )
	{
		s_bPortalLocatorForClientRegistered = true;
		g_pMatchFramework->GetMatchExtensions()->RegisterExtensionInterface( IEXTPROPPORTALLOCATOR_INTERFACE_NAME, &s_PortalServerDllPropPortalLocator );
	}
}

void CProp_Portal::OnRestore()
{
	BaseClass::OnRestore();

	if ( IsActive() )
	{
		// Place the particles in position
		DispatchPortalPlacementParticles( m_bIsPortal2 );
	}

	AddToLinkageGroup();
}

ConVar sv_portals_block_other_players( "sv_portals_block_other_players", "0", FCVAR_REPLICATED | FCVAR_DEVELOPMENTONLY );

void CProp_Portal::StartTouch( CBaseEntity *pOther )
{
	if( sv_portals_block_other_players.GetBool() && g_pGameRules->IsMultiplayer() )
	{
		if( pOther->IsPlayer() && (m_hFiredByPlayer.Get() != pOther) )
			return; //block the interaction
	}

	return BaseClass::StartTouch( pOther );
}

void CProp_Portal::Touch( CBaseEntity *pOther )
{
	if( sv_portals_block_other_players.GetBool() && g_pGameRules->IsMultiplayer() )
	{
		if( pOther->IsPlayer() && (m_hFiredByPlayer.Get() != pOther) )
			return; //block the interaction
	}

	return BaseClass::Touch( pOther );
}

void CProp_Portal::EndTouch( CBaseEntity *pOther )
{
	if( sv_portals_block_other_players.GetBool() && g_pGameRules->IsMultiplayer() )
	{
		if( pOther->IsPlayer() && (m_hFiredByPlayer.Get() != pOther) )
			return; //block the interaction
	}

	return BaseClass::EndTouch( pOther );
}

void DumpActiveCollision( const CPortalSimulator *pPortalSimulator, const char *szFileName );
void PortalSimulatorDumps_DumpCollideToGlView( CPhysCollide *pCollide, const Vector &origin, const QAngle &angles, float fColorScale, const char *pFilename );




void CProp_Portal::ResetModel( void )
{
	if( !m_bIsPortal2 )
		SetModel( "models/portals/portal1.mdl" );
	else
		SetModel( "models/portals/portal2.mdl" );

	if( IsMobile() || ((m_hLinkedPortal.Get() != NULL) && !m_hLinkedPortal->IsMobile()) )
	{
		SetSize( GetLocalMins(), Vector( 4.0f, m_fNetworkHalfWidth, m_fNetworkHalfHeight ) );
	}
	else
	{
		SetSize( GetLocalMins(), GetLocalMaxs() );
	}

	SetSolid( SOLID_OBB );
	SetSolidFlags( FSOLID_TRIGGER | FSOLID_NOT_SOLID | FSOLID_CUSTOMBOXTEST | FSOLID_CUSTOMRAYTEST );
}

void CProp_Portal::DoFizzleEffect( int iEffect, bool bDelayedPos /*= true*/ )
{
	m_vAudioOrigin = ( ( bDelayedPos ) ? ( m_vDelayedPosition ) : ( m_vOldPosition ) );

	CEffectData	fxData;

	fxData.m_vAngles = ( ( bDelayedPos ) ? ( m_qDelayedAngles ) : ( m_qOldAngles ) );

	Vector vForward, vUp;
	AngleVectors( fxData.m_vAngles, &vForward, &vUp, NULL );
	fxData.m_vOrigin = m_vAudioOrigin + vForward * 1.0f;

	fxData.m_nColor = ( ( m_bIsPortal2 ) ? ( 1 ) : ( 0 ) );

	EmitSound_t ep;
	CPASAttenuationFilter filter( m_vDelayedPosition );

	ep.m_nChannel = CHAN_STATIC;
	ep.m_flVolume = 1.0f;
	ep.m_pOrigin = &m_vAudioOrigin;

	int nTeam = GetTeamNumber();
	int nPortalNum = m_bIsPortal2 ? 2 : 1;

	// Rumble effects on the firing player (if one exists)
	CWeaponPortalgun *pPortalGun = dynamic_cast<CWeaponPortalgun*>( m_hPlacedBy.Get() );
	CBasePlayer* pPlayer = NULL;

	if ( pPortalGun	)
	{
		pPlayer = (CBasePlayer*)pPortalGun->GetOwner();
		if ( pPlayer )
		{
			if ( iEffect != PORTAL_FIZZLE_CLOSE && 
				 iEffect != PORTAL_FIZZLE_SUCCESS && 
				 iEffect != PORTAL_FIZZLE_NONE )
			{
				pPlayer->RumbleEffect( RUMBLE_PORTAL_PLACEMENT_FAILURE, 0, RUMBLE_FLAGS_NONE );
			}

			nTeam = pPlayer->GetTeamNumber();
		}
	}

	// Pick a fizzle effect
	switch ( iEffect )
	{
		case PORTAL_FIZZLE_CANT_FIT:
			//DispatchEffect( "PortalFizzleCantFit", fxData );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			CreatePortalEffect( pPlayer, PORTAL_FIZZLE_BAD_SURFACE, fxData.m_vOrigin, fxData.m_vAngles, nTeam, nPortalNum );
			break;

		case PORTAL_FIZZLE_OVERLAPPED_LINKED:
		{
			/*CProp_Portal *pLinkedPortal = m_hLinkedPortal;
			if ( pLinkedPortal )
			{
				Vector vLinkedForward;
				pLinkedPortal->GetVectors( &vLinkedForward, NULL, NULL );
				fxData.m_vStart = pLink3edPortal->GetAbsOrigin() + vLinkedForward * 5.0f;
			}*/

			//DispatchEffect( "PortalFizzleOverlappedLinked", fxData );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			CreatePortalEffect( pPlayer, PORTAL_FIZZLE_BAD_SURFACE, fxData.m_vOrigin, fxData.m_vAngles, nTeam, nPortalNum );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			break;
		}

		case PORTAL_FIZZLE_BAD_VOLUME:
			//DispatchEffect( "PortalFizzleBadVolume", fxData );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			CreatePortalEffect( pPlayer, PORTAL_FIZZLE_BAD_SURFACE, fxData.m_vOrigin, fxData.m_vAngles, nTeam, nPortalNum );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			break;

		case PORTAL_FIZZLE_BAD_SURFACE:
			//DispatchEffect( "PortalFizzleBadSurface", fxData );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			CreatePortalEffect( pPlayer, PORTAL_FIZZLE_BAD_SURFACE, fxData.m_vOrigin, fxData.m_vAngles, nTeam, nPortalNum );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			break;

		case PORTAL_FIZZLE_KILLED:
			//DispatchEffect( "PortalFizzleKilled", fxData );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			CreatePortalEffect( pPlayer, PORTAL_FIZZLE_CLOSE, fxData.m_vOrigin, fxData.m_vAngles, nTeam, nPortalNum );
			ep.m_pSoundName = "Portal.fizzle_moved";
			break;

		case PORTAL_FIZZLE_CLEANSER:
			//DispatchEffect( "PortalFizzleCleanser", fxData );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			CreatePortalEffect( pPlayer, PORTAL_FIZZLE_BAD_SURFACE, fxData.m_vOrigin, fxData.m_vAngles, nTeam, nPortalNum );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			break;

		case PORTAL_FIZZLE_CLOSE:
			//DispatchEffect( "PortalFizzleKilled", fxData );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			CreatePortalEffect( pPlayer, PORTAL_FIZZLE_CLOSE, fxData.m_vOrigin, fxData.m_vAngles, nTeam, nPortalNum );
			ep.m_pSoundName = ( ( m_bIsPortal2 ) ? ( "Portal.close_red" ) : ( "Portal.close_blue" ) );
			break;

		case PORTAL_FIZZLE_NEAR_BLUE:
		{
			if ( !m_bIsPortal2 )
			{
				Vector vLinkedForward;
				m_hLinkedPortal->GetVectors( &vLinkedForward, NULL, NULL );
				fxData.m_vOrigin = m_hLinkedPortal->GetAbsOrigin() + vLinkedForward * 16.0f;
				fxData.m_vAngles = m_hLinkedPortal->GetAbsAngles();
			}
			else
			{
				GetVectors( &vForward, NULL, NULL );
				fxData.m_vOrigin = GetAbsOrigin() + vForward * 16.0f;
				fxData.m_vAngles = GetAbsAngles();
			}

			//DispatchEffect( "PortalFizzleNear", fxData );
			AngleVectors( fxData.m_vAngles, &vForward, &vUp, NULL );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			CreatePortalEffect( pPlayer, PORTAL_FIZZLE_BAD_SURFACE, fxData.m_vOrigin, fxData.m_vAngles, nTeam, nPortalNum );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			break;
		}

		case PORTAL_FIZZLE_NEAR_RED:
		{
			if ( m_bIsPortal2 )
			{
				Vector vLinkedForward;
				m_hLinkedPortal->GetVectors( &vLinkedForward, NULL, NULL );
				fxData.m_vOrigin = m_hLinkedPortal->GetAbsOrigin() + vLinkedForward * 16.0f;
				fxData.m_vAngles = m_hLinkedPortal->GetAbsAngles();
			}
			else
			{
				GetVectors( &vForward, NULL, NULL );
				fxData.m_vOrigin = GetAbsOrigin() + vForward * 16.0f;
				fxData.m_vAngles = GetAbsAngles();
			}

			//DispatchEffect( "PortalFizzleNear", fxData );
			AngleVectors( fxData.m_vAngles, &vForward, &vUp, NULL );
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			CreatePortalEffect( pPlayer, PORTAL_FIZZLE_BAD_SURFACE, fxData.m_vOrigin, fxData.m_vAngles, nTeam, nPortalNum );
			ep.m_pSoundName = "Portal.fizzle_invalid_surface";
			break;
		}

		case PORTAL_FIZZLE_SUCCESS:
			VectorAngles( vUp, vForward, fxData.m_vAngles );
			CreatePortalEffect( pPlayer, PORTAL_FIZZLE_SUCCESS, fxData.m_vOrigin, fxData.m_vAngles, nTeam, nPortalNum );
			// Don't make a sound!
			return;

		case PORTAL_FIZZLE_NONE:
			// Don't do anything!
			return;
	}

	EmitSound( filter, SOUND_FROM_WORLD, ep );
}

//-----------------------------------------------------------------------------
// Purpose: Create the portal effect
//-----------------------------------------------------------------------------
void CProp_Portal::CreatePortalEffect( CBasePlayer* pPlayer, int iEffect, Vector vecOrigin, QAngle qAngles, int nTeam, int nPortalNum )
{
	if ( !pPlayer || iEffect == PORTAL_FIZZLE_NONE )
		return;

	CBroadcastRecipientFilter filter;
	filter.MakeReliable();

	// remove the player who shot it because we handle this in 
	// the client code and don't need to send a message
	if ( pPlayer->m_bPredictionEnabled )
	{
		filter.RemoveRecipient( pPlayer );
	}

	UserMessageBegin( filter, "PortalFX_Surface" );
	WRITE_SHORT( entindex() );
	WRITE_SHORT( pPlayer->entindex() );
	WRITE_BYTE( nTeam );
	WRITE_BYTE( nPortalNum );
	WRITE_BYTE( iEffect );
	WRITE_VEC3COORD( vecOrigin );
	WRITE_ANGLES( qAngles );
	MessageEnd();
}

//-----------------------------------------------------------------------------
// Purpose: Fizzle the portal
//-----------------------------------------------------------------------------
void CProp_Portal::OnPortalDeactivated( void )
{
	if ( m_pAmbientSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		controller.SoundChangeVolume( m_pAmbientSound, 0.0, 0.0 );
	}

	//TODO: Fizzle Effects
	DoFizzleEffect( m_FizzleEffect );
	m_FizzleEffect = PORTAL_FIZZLE_KILLED; //assume we want a generic killed type unless someone sets it to something else before we fizzle next. Lets CPortal_Base2D kill us with a fizzle effect while it has no knowledge of fizzling

	BaseClass::OnPortalDeactivated();
}


//-----------------------------------------------------------------------------
// Purpose: Portal will fizzle next time we get to think
//-----------------------------------------------------------------------------
void CProp_Portal::Fizzle( void )
{
	m_FizzleEffect = PORTAL_FIZZLE_NONE; //Logic that uses Fizzle() always calls DoFizzleEffect() manually
	//DeactivatePortalOnThink();
	DeactivatePortalNow();
}


void CProp_Portal::Activate( void )
{

	CreateSounds();

	BaseClass::Activate();
}



//-----------------------------------------------------------------------------
// Purpose: Kinda sucks... Normal triggers won't find portals because they're also triggers.
//			Rather than addressing that directly, portal detectors look for portals with an explicit OBB check.
//			
//-----------------------------------------------------------------------------
void CProp_Portal::UpdatePortalDetectorsOnPortalMoved( void )
{
	for ( CFuncPortalDetector *pDetector = GetPortalDetectorList(); pDetector != NULL; pDetector = pDetector->m_pNext )
	{
		pDetector->UpdateOnPortalMoved( this );
	}
}

void CProp_Portal::UpdatePortalDetectorsOnPortalActivated( void )
{
	for ( CFuncPortalDetector *pDetector = GetPortalDetectorList(); pDetector != NULL; pDetector = pDetector->m_pNext )
	{
		pDetector->UpdateOnPortalActivated( this );
	}
}

void CProp_Portal::UpdatePortalLinkage( void )
{
	if( IsActive() )
	{
		CProp_Portal *pLink = (CProp_Portal *)m_hLinkedPortal.Get();

		if( !(pLink && pLink->IsActive()) )
		{
			//no old link, or inactive old link

			if( pLink )
			{
				//we had an old link, must be inactive. Make doubly sure it's disconnected
				if( pLink->m_hLinkedPortal.Get() != NULL )
				{
					if( pLink->m_hLinkedPortal.Get() == this )
						pLink->m_hLinkedPortal = NULL; //avoid recursion

					pLink->UpdatePortalLinkage();
				}

				pLink = NULL;
			}

			int iPortalCount = s_PortalLinkageGroups[m_iLinkageGroupID].Count();

			// More than two sharing a linkage id? is that valid?
			//Assert( iPortalCount <3 ); yes it is as long as only two are active

			if( iPortalCount != 0 )
			{
				CProp_Portal **pPortals = s_PortalLinkageGroups[m_iLinkageGroupID].Base();
				for( int i = 0; i != iPortalCount; ++i )
				{
					CProp_Portal *pCurrentPortal = pPortals[i];
					if( pCurrentPortal == this )
						continue;
					if( pCurrentPortal->IsActive() && 
						(pCurrentPortal->m_hLinkedPortal.Get() == NULL) &&
						(pCurrentPortal->m_fNetworkHalfWidth == m_fNetworkHalfWidth) &&
						(pCurrentPortal->m_fNetworkHalfHeight == m_fNetworkHalfHeight) )
					{
						pLink = pCurrentPortal;
						pCurrentPortal->m_hLinkedPortal = this;
						pCurrentPortal->UpdatePortalLinkage();
						break;
					}
				}
			}
		}

		m_hLinkedPortal = pLink;
	}
	else
	{
		CProp_Portal *pRemote = (CProp_Portal *)m_hLinkedPortal.Get();
		//apparently we've been deactivated
		m_PortalSimulator.DetachFromLinked();
		m_PortalSimulator.ReleaseAllEntityOwnership();

		m_hLinkedPortal = NULL;
		if( pRemote )
			pRemote->UpdatePortalLinkage();
	}

	BaseClass::UpdatePortalLinkage();
}



void CProp_Portal::DispatchPortalPlacementParticles( bool bIsSecondaryPortal )
{
	// never do this in multiplayer
	if ( GameRules()->IsMultiplayer() )
		return;

	// the particle effects are no longer created on the server in SP unless this convar is set,
	// if it's not set, they are created on the client in function: CreateAttachedParticles
	if ( !use_server_portal_particles.GetBool() )
		return;

	// Send the particles only to the player who 
	CBasePlayer *pFiringPlayer = ToBasePlayer( m_hFiredByPlayer.Get() );
	if ( pFiringPlayer )
	{
		CSingleUserRecipientFilter localFilter( pFiringPlayer );
		localFilter.MakeReliable();
		DispatchParticleEffect( ( ( bIsSecondaryPortal ) ? ( "portal_2_edge" ) : ( "portal_1_edge" ) ), PATTACH_POINT_FOLLOW, this, "particles", true, -1, &localFilter );
	}
}

void CProp_Portal::NewLocation( const Vector &vOrigin, const QAngle &qAngles )
{
	BaseClass::NewLocation( vOrigin, qAngles );
	CreateSounds();

	UpdatePortalDetectorsOnPortalMoved();
	if( (m_hLinkedPortal.Get() != NULL) && (m_bOldActivatedState == false) && (IsActive() == true) )
	{
		//went from inactive to active
		UpdatePortalDetectorsOnPortalActivated();
		((CProp_Portal *)m_hLinkedPortal.Get())->UpdatePortalDetectorsOnPortalActivated();
	}

	if( m_NotifyOnPortalled && !m_NotifyOnPortalled->IsPortalTouchingDetector( this ) )
		m_NotifyOnPortalled = NULL;

	if ( m_pAmbientSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
		controller.SoundChangeVolume( m_pAmbientSound, 0.4, 0.1 );
	}

	// Place the particles in position
	DispatchPortalPlacementParticles( m_bIsPortal2 );

	if( !IsMobile() )
	{
		if ( m_bIsPortal2 )
		{
			EmitSound( "Portal.open_red" );
		}
		else
		{
			EmitSound( "Portal.open_blue" );
		}
	}
}

void CProp_Portal::PreTeleportTouchingEntity( CBaseEntity *pOther )
{
	if( m_NotifyOnPortalled )
		m_NotifyOnPortalled->OnPrePortalled( pOther, true );

	CProp_Portal *pLinked = (CProp_Portal *)m_hLinkedPortal.Get();
	if( pLinked->m_NotifyOnPortalled )
		pLinked->m_NotifyOnPortalled->OnPrePortalled( pOther, false );

	BaseClass::PreTeleportTouchingEntity( pOther );
}

void CProp_Portal::PostTeleportTouchingEntity( CBaseEntity *pOther )
{
	if( m_NotifyOnPortalled )
		m_NotifyOnPortalled->OnPostPortalled( pOther, true );

	CProp_Portal *pLinked = (CProp_Portal *)m_hLinkedPortal.Get();
	if( pLinked->m_NotifyOnPortalled )
		pLinked->m_NotifyOnPortalled->OnPostPortalled( pOther, false );

	BaseClass::PostTeleportTouchingEntity( pOther );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CProp_Portal::ActivatePortal( void )
{
	m_hPlacedBy = NULL;

	Vector vOrigin;
	vOrigin = GetAbsOrigin();

	Vector vForward, vUp;
	GetVectors( &vForward, 0, &vUp );

	CTraceFilterSimpleClassnameList baseFilter( this, COLLISION_GROUP_NONE );
	UTIL_Portal_Trace_Filter( &baseFilter );
	CTraceFilterTranslateClones traceFilterPortalShot( &baseFilter );

	trace_t tr;
	UTIL_TraceLine( vOrigin + vForward, vOrigin + vForward * -8.0f, MASK_SHOT_PORTAL, &traceFilterPortalShot, &tr );

	QAngle qAngles;
	VectorAngles( tr.plane.normal, vUp, qAngles );

	PortalPlacementResult_t eResult = VerifyPortalPlacementAndFizzleBlockingPortals( this, tr.endpos, qAngles, GetHalfWidth(), GetHalfHeight(), PORTAL_PLACED_BY_FIXED );

	PlacePortal( tr.endpos, qAngles, eResult );

	// If the fixed portal is overlapping a portal that was placed before it... kill it!
	if ( PortalPlacementSucceeded( eResult ) )
	{
		CreateSounds();

		if ( m_pAmbientSound )
		{
			CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

			controller.SoundChangeVolume( m_pAmbientSound, 0.4, 0.1 );
		}

		// Place the particles in position
		DispatchPortalPlacementParticles( m_bIsPortal2 );

		if ( m_bIsPortal2 )
		{
			EmitSound( "Portal.open_red" );
		}
		else
		{
			EmitSound( "Portal.open_blue" );
		}
	}

	UpdatePortalTeleportMatrix();

	UpdatePortalLinkage();

	CBaseProjector::TestAllForProjectionChanges();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CProp_Portal::DeactivatePortal( void )
{
	if ( m_pAmbientSound )
	{
		CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();

		controller.SoundChangeVolume( m_pAmbientSound, 0.0, 0.0 );
	}

	StopParticleEffects( this );

	UpdatePortalTeleportMatrix();

	UpdatePortalLinkage();

	CBaseProjector::TestAllForProjectionChanges();
}

void CProp_Portal::InputSetActivatedState( inputdata_t &inputdata )
{
	SetActive( inputdata.value.Bool() );

	if ( IsActive() )
	{ 
		ActivatePortal();
	}
	else
	{
		DeactivatePortal();
	}
}

void CProp_Portal::InputFizzle( inputdata_t &inputdata )
{
	DoFizzleEffect( PORTAL_FIZZLE_KILLED, false );
	Fizzle();
}

//-----------------------------------------------------------------------------
// Purpose: Map can call new location, so far it's only for debugging purposes so it's not made to be very robust.
// Input  : &inputdata - String with 6 float entries with space delimiters, location and orientation
//-----------------------------------------------------------------------------
void CProp_Portal::InputNewLocation( inputdata_t &inputdata )
{
	char sLocationStats[MAX_PATH];
	Q_strncpy( sLocationStats, inputdata.value.String(), sizeof(sLocationStats) );

	// first 3 are location of new origin
	Vector vNewOrigin;
	char* pTok = strtok( sLocationStats, " " ); 
	vNewOrigin.x = atof(pTok);
	pTok = strtok( NULL, " " );
	vNewOrigin.y = atof(pTok);
	pTok = strtok( NULL, " " );
	vNewOrigin.z = atof(pTok);

	// Next 3 entries are new angles
	QAngle vNewAngles;
	pTok = strtok( NULL, " " );
	vNewAngles.x = atof(pTok);
	pTok = strtok( NULL, " " );
	vNewAngles.y = atof(pTok);
	pTok = strtok( NULL, " " );
	vNewAngles.z = atof(pTok);

	// Call main placement function (skipping placement rules)
	NewLocation( vNewOrigin, vNewAngles );
}

void CProp_Portal::InputResize( inputdata_t &inputdata )
{
	char sResizeStats[MAX_PATH];
	Q_strncpy( sResizeStats, inputdata.value.String(), sizeof(sResizeStats) );

	char* pTok = strtok( sResizeStats, " " ); 
	float fHalfWidth = atof(pTok);
	pTok = strtok( NULL, " " );
	float fHalfHeight = atof(pTok);

	Resize( fHalfWidth, fHalfHeight );	
}

void CProp_Portal::InputSetLinkageGroupId( inputdata_t &inputdata )
{
	int iGroupId = inputdata.value.Int(); 

	if ( ( iGroupId >= 0 ) && ( iGroupId < 255 )  )
	{
		ChangeLinkageGroup( iGroupId );

		if ( IsActive() )
		{
			SetActive( false );
			
			// shut the portal down and reactivate it so it will re-link with new portal group id
			DeactivatePortal();
			SetActive( true );
			ActivatePortal();
		}
	}
	else
	{
		Warning( "*** SetLinkageGroupId input failed because Portal ID must be between 0 and 255!\n" );
	}
}

void CProp_Portal::AddToLinkageGroup( void )
{
	if ( m_iLinkageGroupID != PORTAL_LINKAGE_GROUP_INVALID )
	{
		if( s_PortalLinkageGroups[m_iLinkageGroupID].Find( this ) == -1 )
			s_PortalLinkageGroups[m_iLinkageGroupID].AddToTail( this );
	}
}

void CProp_Portal::ChangeLinkageGroup( unsigned char iLinkageGroupID )
{
	if ( iLinkageGroupID == PORTAL_LINKAGE_GROUP_INVALID )
	{
		// invalid is the 'inactive portal' group for portals not yet linked.
		m_iLinkageGroupID = iLinkageGroupID;
		return;
	}

	// We should be moving from a linkage id to another one, unles we're coming from INVALID
	Assert( s_PortalLinkageGroups[m_iLinkageGroupID].Find( this ) != -1 || m_iLinkageGroupID == PORTAL_LINKAGE_GROUP_INVALID );
	s_PortalLinkageGroups[m_iLinkageGroupID].FindAndRemove( this );
	s_PortalLinkageGroups[iLinkageGroupID].AddToTail( this );
	m_iLinkageGroupID = iLinkageGroupID;
}



CProp_Portal *CProp_Portal::FindPortal( unsigned char iLinkageGroupID, bool bPortal2, bool bCreateIfNothingFound /*= false*/ )
{
	int iPortalCount = s_PortalLinkageGroups[iLinkageGroupID].Count();

	if( iPortalCount != 0 )
	{
		CProp_Portal *pFoundInactive = NULL;
		CProp_Portal **pPortals = s_PortalLinkageGroups[iLinkageGroupID].Base();
		for( int i = 0; i != iPortalCount; ++i )
		{
			if( pPortals[i]->m_bIsPortal2 == bPortal2 )
			{
				if( pPortals[i]->IsActive() )
					return pPortals[i];
				else
					pFoundInactive = pPortals[i];
			}
		}

		if( pFoundInactive )
			return pFoundInactive;
	}

	if( bCreateIfNothingFound )
	{
		CProp_Portal *pPortal = (CProp_Portal *)CreateEntityByName( "prop_portal" );
		pPortal->m_iLinkageGroupID = iLinkageGroupID;
		pPortal->m_bIsPortal2 = bPortal2;
		DispatchSpawn( pPortal );
		return pPortal;
	}

	return NULL;
}

const CUtlVector<CProp_Portal *> *CProp_Portal::GetPortalLinkageGroup( unsigned char iLinkageGroupID )
{
	return &s_PortalLinkageGroups[iLinkageGroupID];
}


// Hands out linkage IDs in order. If somebody has taken the slot, it walks to a free one and picks that as the new starting location.
static unsigned char s_iBestGuessUnusedLinkageID = 0;
unsigned char UTIL_GetUnusedLinkageID( void )
{
	if ( s_PortalLinkageGroups[s_iBestGuessUnusedLinkageID].Count() == 0 )
	{
		// early out for best guess 
		return s_iBestGuessUnusedLinkageID++;
	}
	else
	{
		// walk all linkage groups for a free one
		for ( int i = 0; i < 256; ++i )
		{
			if ( s_PortalLinkageGroups[i].Count() == 0 )
			{
				s_iBestGuessUnusedLinkageID = i+1;
				return i;
			}
		}
	}

	Warning( "*** All portal linkage IDs in use! ***\nThere may be >254 portal pairs, or some bug causing the linkage IDs not to be freed up.\n" );
	Assert( 0 );
	return PORTAL_LINKAGE_GROUP_INVALID;
}




//------------------------------------------------------------------------------
// Purpose: Create an NPC of the given type
//------------------------------------------------------------------------------
void CC_Resize_Portals( const CCommand &args )
{
	if( args.ArgC() < 3 )
	{
		Warning( "syntax: Portals_ResizeAll [half width] [half height]\n" );
		return;
	}

	float fHalfWidth = atof(args[1]);
	float fHalfHeight = atof(args[2]);

	int iPortalCount = CProp_Portal_Shared::AllPortals.Count();

	for( int i = 0; i != iPortalCount; ++i )
	{
		CProp_Portal_Shared::AllPortals[i]->Resize( fHalfWidth, fHalfHeight );
	}

	CProp_Portal::ms_DefaultPortalHalfWidth = fHalfWidth;
	CProp_Portal::ms_DefaultPortalHalfHeight = fHalfHeight;
}
static ConCommand Portals_ResizeAll("Portals_ResizeAll", CC_Resize_Portals, "Resizes all portals (for testing), Portals_ResizeAll [half width] [half height]", FCVAR_CHEAT);

