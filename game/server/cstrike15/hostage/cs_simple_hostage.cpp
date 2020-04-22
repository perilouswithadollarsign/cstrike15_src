//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

// cs_simple_hostage.cpp
// Simple CS1.6 level hostage
// Author: Michael S. Booth and Matt Boone, July 2004
// Bugs Fixed by: Everyone else

#include "cbase.h"
#include "cs_simple_hostage.h"
#include "cs_player.h"
#include "cs_gamerules.h"
#include "game.h"
#include "bot.h"
#include <keyvalues.h>
#include "obstacle_pushaway.h"
#include "props_shared.h"
#include "te_effect_dispatch.h"

#include "cs_gamestats.h"
#include "cs_gamerules.h"
#include "cs_achievement_constants.h"
#include "gametypes.h"
#include "cvisibilitymonitor.h"
#include "cs_entity_spotting.h"
#include "cs_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define HOSTAGE_THINK_INTERVAL	(1.0f/60.0f)//We increased this from 0.1 in CS:GO so that the hostages don't stutter as much.
#define HOSTAGE_THINK_CARRIED_INTERVAL	(1.0f/45.0f)

#define DrawLine( from, to, duration, red, green, blue )		NDebugOverlay::Line( from, to, red, green, blue, true, 0.1f )
#define HOSTAGE_PUSHAWAY_THINK_CONTEXT	"HostagePushawayThink"

#define HOSTAGE_BBOX_VEC_MIN	Vector( -8, -8, 0 )
#define HOSTAGE_BBOX_VEC_MAX	Vector( 8, 8, 64 )

#define MAX_HOSTAGE_MOVE_FORCE 1024

#define HOSTAGE_CARRY_PROP_MODEL		"models/hostage/hostage_carry.mdl"
#define HOSTAGE_CARRY_VIEW_MODEL		"models/hostage/v_hostage_arm.mdl"

// amount of time a player is forced to continue defusing after not USEing. this effects other player's ability to interrupt
const float GRAB_HOSTAGE_LOCKIN_PERIOD = 0.05f;	

ConVar mp_hostagepenalty( "mp_hostagepenalty", "10", FCVAR_NOTIFY, "Terrorist are kicked for killing too much hostages" );
ConVar hostage_debug( "hostage_debug", "0", FCVAR_CHEAT, "Show hostage AI debug information" );
ConVar hostage_drop_time( "hostage_drop_time", "1", FCVAR_DEVELOPMENTONLY, "Time for the hostage before it fully drops to ground" );
ConVar hostage_is_silent( "hostage_is_silent", "0", FCVAR_CHEAT, "When set, the hostage won't play any code driven response rules lines" );


extern ConVar sv_pushaway_force;
extern ConVar sv_pushaway_min_player_speed;
extern ConVar sv_pushaway_max_force;
extern ConVar mp_hostages_takedamage;
extern ConVar mp_hostages_rescuetowin;

// We need hostage-specific pushaway cvars because the hostage doesn't have the same friction etc as players
ConVar sv_pushaway_hostage_force( "sv_pushaway_hostage_force", "20000", FCVAR_REPLICATED | FCVAR_CHEAT, "How hard the hostage is pushed away from physics objects (falls off with inverse square of distance)." );
ConVar sv_pushaway_max_hostage_force( "sv_pushaway_max_hostage_force", "1000", FCVAR_REPLICATED | FCVAR_CHEAT, "Maximum of how hard the hostage is pushed away from physics objects." );

ConVar mp_hostages_max( "mp_hostages_max", "2", FCVAR_REPLICATED | FCVAR_RELEASE, "Maximum number of hostages to spawn." );
ConVar mp_hostages_spawn_farthest( "mp_hostages_spawn_farthest", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "When enabled will consistently force the farthest hostages to spawn." );
ConVar mp_hostages_spawn_same_every_round( "mp_hostages_spawn_same_every_round", "1", FCVAR_REPLICATED | FCVAR_RELEASE, "0 = spawn hostages randomly every round, 1 = same spawns for entire match." );
ConVar mp_hostages_spawn_force_positions( "mp_hostages_spawn_force_positions", "", FCVAR_REPLICATED | FCVAR_RELEASE, "Comma separated list of zero based indices to force spawn positions, e.g. '0,2' or '1,6'" );
ConVar mp_hostages_run_speed_modifier( "mp_hostages_run_speed_modifier", "1.0", FCVAR_REPLICATED | FCVAR_RELEASE, "Default is 1.0, slow down hostages by setting this to < 1.0.", true, 0.1, true, 1.5 );


LINK_ENTITY_TO_CLASS( hostage_carriable_prop, CHostageCarriableProp );
PRECACHE_REGISTER( hostage_carriable_prop );

BEGIN_DATADESC( CHostageCarriableProp )
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CHostageCarriableProp, DT_HostageCarriableProp )
END_SEND_TABLE()

BEGIN_PREDICTION_DATA( CHostageCarriableProp )
END_PREDICTION_DATA()

const int DEFAULT_NUM_HOSTAGE_MODELS = 4;
static char *HostageModel[DEFAULT_NUM_HOSTAGE_MODELS] = 
{
	"models/hostage/hostage.mdl",
	"models/hostage/hostage_variantA.mdl",
	"models/hostage/hostage_variantB.mdl",
	"models/hostage/hostage_variantC.mdl",
};

void SendProxy_CropFlagsToPlayerFlagBitsLength( const SendProp *pProp, const void *pStruct, const void *pVarData, DVariant *pOut, int iElement, int objectID);

Vector DropToGround( CBaseEntity *pMainEnt, const Vector &vPos, const Vector &vMins, const Vector &vMaxs );

BEGIN_ENT_SCRIPTDESC( CHostage, CBaseEntity, "hostage_carriable_prop" )
DEFINE_SCRIPTFUNC_NAMED( IsBeingCarried, "IsBeingCarried", "Get whether the hostage is currently being carried or not" )
END_SCRIPTDESC()

//-----------------------------------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( info_hostage_spawn, CHostage );
LINK_ENTITY_TO_CLASS( hostage_entity, CHostage );

//-----------------------------------------------------------------------------------------------------
BEGIN_DATADESC( CHostage )

	DEFINE_INPUTFUNC( FIELD_VOID, "OnRescueZoneTouch", HostageRescueZoneTouch ),

	DEFINE_USEFUNC( HostageUse ), 
	DEFINE_THINKFUNC( HostageThink ),

	//Outputs	
	DEFINE_OUTPUT( m_OnHostageBeginGrab, "OnHostageBeginGrab" ),
	DEFINE_OUTPUT( m_OnFirstPickedUp, "OnFirstPickedUp" ),
	DEFINE_OUTPUT( m_OnDroppedNotRescued, "OnDroppedNotRescued" ),
	DEFINE_OUTPUT( m_OnRescued, "OnRescued" ),
END_DATADESC()


//-----------------------------------------------------------------------------------------------------
IMPLEMENT_SERVERCLASS_ST( CHostage, DT_CHostage )
	SendPropExclude( "DT_BaseAnimating", "m_flPoseParameter" ),
	SendPropExclude( "DT_BaseAnimating", "m_flPlaybackRate" ),	
	SendPropExclude( "DT_BaseAnimating", "m_nSequence" ),
	SendPropExclude( "DT_BaseAnimating", "m_nNewSequenceParity" ),
	SendPropExclude( "DT_BaseAnimating", "m_nResetEventsParity" ),
	SendPropExclude( "DT_BaseAnimatingOverlay", "overlay_vars" ),
	
	// cs_playeranimstate and clientside animation takes care of these on the client
	SendPropExclude( "DT_ServerAnimationData" , "m_flCycle" ),	
	SendPropExclude( "DT_AnimTimeMustBeFirst" , "m_flAnimTime" ),

	SendPropBool( SENDINFO(m_isRescued) ),
	SendPropBool( SENDINFO(m_jumpedThisFrame) ),
	SendPropInt( SENDINFO(m_iHealth), 10 ),
	SendPropInt( SENDINFO(m_iMaxHealth), 10 ),
	SendPropInt( SENDINFO(m_lifeState), 3, SPROP_UNSIGNED ),
	SendPropInt( SENDINFO(m_fFlags), PLAYER_FLAG_BITS, SPROP_UNSIGNED, SendProxy_CropFlagsToPlayerFlagBitsLength ),
	SendPropVector( SENDINFO( m_vel ), 12, 0x0, -MAX_HOSTAGE_MOVE_FORCE, MAX_HOSTAGE_MOVE_FORCE ),
	SendPropEHandle( SENDINFO(m_leader) ),
	SendPropInt( SENDINFO(m_nHostageState) ),

	SendPropFloat( SENDINFO(m_flRescueStartTime) ),
	SendPropFloat( SENDINFO(m_flGrabSuccessTime) ),
	SendPropFloat( SENDINFO(m_flDropStartTime) ),

END_SEND_TABLE()


//-----------------------------------------------------------------------------------------------------
CUtlVector< CHostage * > g_Hostages;
static CountdownTimer announceTimer;		// used to stop "hostage rescued" announcements from stepping on each other

//-----------------------------------------------------------------------------------------------------
CHostage::CHostage()
{
	if ( g_Hostages.Count() < MAX_HOSTAGES )
	{
		g_Hostages.AddToTail( this );
		m_bRemove = false;
	}
	else
	{
		DevMsg( "The maximum number of hostages (%i) has been exceeded.", MAX_HOSTAGES );
		m_bRemove = true;
	}

	m_uiHostageSpawnExclusionGroupMask = 0u;
	m_nHostageSpawnRandomFactor = 1u;

	m_PlayerAnimState = CreateHostageAnimState( this, this, LEGANIM_9WAY, false );
	UseClientSideAnimation();
	SetBloodColor( BLOOD_COLOR_RED );
	m_pExpresser = NULL;

	SetSpotRules(  CCSEntitySpotting::SPOT_RULE_ALWAYS_SEEN_BY_CT | CCSEntitySpotting::SPOT_RULE_ALWAYS_SEEN_BY_T );
}

//-----------------------------------------------------------------------------------------------------
CHostage::~CHostage()
{
	g_Hostages.FindAndRemove( this );
	m_PlayerAnimState->Release();
	delete m_pExpresser;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAI_Expresser *CHostage::CreateExpresser( void )
{
	m_pExpresser = new CMultiplayer_Expresser( this );
	if ( !m_pExpresser)
		return NULL;

	m_pExpresser->Connect( this );
	return m_pExpresser;
}

CWeaponCSBase* CHostage::CSAnim_GetActiveWeapon()
{
	return NULL;
}

bool CHostage::CSAnim_CanMove()
{
	return true;
}

bool CHostage::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( char const *szHostageSpawnExclusionGroup = StringAfterPrefix( szKeyName, "HostageSpawnExclusionGroup" ) )
	{
		int iGroupId = Q_atoi( szHostageSpawnExclusionGroup );
		if ( ( iGroupId >= 1 ) && ( iGroupId <= 32 ) )
		{
			bool bValue = !!Q_atoi( szValue );
			uint32 uiBitMaskValue = 1u << ( iGroupId - 1 );
			if ( bValue )
				m_uiHostageSpawnExclusionGroupMask |= uiBitMaskValue;
			else
				m_uiHostageSpawnExclusionGroupMask &=~uiBitMaskValue;
		}
		return true;
	}
	else if ( FStrEq( szKeyName, "HostageSpawnRandomFactor" ) )
	{
		int iFactor = Q_atoi( szValue );
		if ( iFactor < 1 )
			iFactor = 1;
		else if ( iFactor > 100 )
			iFactor = 100;
		m_nHostageSpawnRandomFactor = iFactor;
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}

//-----------------------------------------------------------------------------------------------------
void CHostage::Spawn( void )
{
	// No hostages in deathmatch
	if ( CSGameRules()->IsPlayingGunGame() || ( CSGameRules()->IsWarmupPeriod() && !mp_hostages_spawn_same_every_round.GetBool() ) )
		return;

	// remove hostage spawns that exceeded MAX_HOSTAGES and weren't added to g_Hostages
	if ( m_bRemove )
	{
		UTIL_Remove( this );
	}

	SetClassname( "hostage_entity");

	Precache();

	// round-robin through the hostage models
	static int hostageModelIndex = 0;

	bool hostageSet = false;
	//const CUtlStringList *pTModelNames = g_pGameTypes->GetTModelsForMap( m_mapName );
	const char *mapName = ( gpGlobals ? STRING( gpGlobals->mapname ) : NULL );
	if ( mapName )
	{
		const CUtlStringList *pHostageModelNames = g_pGameTypes->GetHostageModelsForMap( mapName );	
		if ( NULL != pHostageModelNames )
		{
			int count = pHostageModelNames->Count();
			if ( count > 0 )
			{
				int whichModel = hostageModelIndex % count;
				SetModel( (*pHostageModelNames)[ whichModel ] );
				hostageSet = true;
			}
		}
	}

	if ( !hostageSet )
	{
		int whichModel = hostageModelIndex % DEFAULT_NUM_HOSTAGE_MODELS;
		SetModel( HostageModel[ whichModel ] );
	}

	// Advance to the next hostage model type.
	++hostageModelIndex;

	RemoveEffects( EF_NODRAW );
	SetHullType( HULL_HUMAN );

	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetMoveType( MOVETYPE_STEP );
	SetCollisionGroup( COLLISION_GROUP_PLAYER );

	AddFlag( FL_OBJECT ); // have the spawned hostage participate in high-priority entity pickup rules

	SetGravity( 1.0 );

	m_iHealth = 100;	
	m_iMaxHealth = m_iHealth;
	m_takedamage = DAMAGE_YES;

	InitBoneControllers( ); 

	// we must set this, because its zero by default thus putting their eyes in their feet
	SetViewOffset( Vector( 0, 0, 35 ) );


	// set up think callback
	SetNextThink( gpGlobals->curtime + HOSTAGE_THINK_INTERVAL );
	SetThink( &CHostage::HostageThink );

	SetContextThink( &CHostage::PushawayThink, gpGlobals->curtime + PUSHAWAY_THINK_INTERVAL, HOSTAGE_PUSHAWAY_THINK_CONTEXT );

	SetUse( &CHostage::HostageUse );

	m_leader = INVALID_EHANDLE;
	m_reuseTimer.Invalidate();
	m_hasBeenUsed = false;

	m_isRescued = false;
	m_jumpedThisFrame = false;

	m_nHostageState = k_EHostageStates_Idle;

	m_vel = Vector( 0, 0, 0 );
	m_accel = Vector( 0, 0, 0 );

	m_path.Invalidate();
	m_repathTimer.Invalidate();

	m_pathFollower.Reset();
	m_pathFollower.SetPath( &m_path );
	m_pathFollower.SetImprov( this );

	m_lastKnownArea = NULL;

	// Need to make sure the hostages are on the ground when they spawn
	Vector GroundPos = DropToGround( this, GetAbsOrigin(), HOSTAGE_BBOX_VEC_MIN, HOSTAGE_BBOX_VEC_MAX );
	SetAbsOrigin( GroundPos );

	m_isCrouching = false;
	m_isRunning = true;
	m_jumpTimer.Invalidate();
	m_inhibitObstacleAvoidanceTimer.Invalidate();

	m_isWaitingForLeader = false;
	
	m_isAdjusted = false;

	m_lastLeaderID = 0;

	announceTimer.Invalidate();
	m_flRescueStartTime = 0.0f;

	m_flGrabSuccessTime = 0.0f;
	m_flDropStartTime = 0.0f;

	m_fLastGrabTime = 0.0f;
	//m_bBeingGrabbed = false;
	m_flGrabbingLength = 0.0f;
	m_bHandsHaveBeenCut = false;

	m_vecGrabbedPos = GetAbsOrigin();

	CreateExpresser();

	VisibilityMonitor_AddEntity( this, 1600.0f, NULL, NULL );
}

//-----------------------------------------------------------------------------------------------------
void CHostage::Precache()
{
	const char *mapName = ( gpGlobals ? STRING( gpGlobals->mapname ) : NULL );
	
	int numHostageModelsForMap = 0;

	if ( mapName )
	{
		const CUtlStringList *pHostageModelNames = g_pGameTypes->GetHostageModelsForMap( mapName );	
		if ( NULL != pHostageModelNames )
		{
			numHostageModelsForMap = pHostageModelNames->Count();
			FOR_EACH_VEC( *pHostageModelNames, iModel )
			{
				const char *modelName = (*pHostageModelNames)[iModel];
				if ( modelName )
				{
					PrecacheModel( modelName );
				}
				else
				{
					Warning( "CHostage::Precache: Null hostage model name found for map %s in GameModes.txt\n", mapName );
				}
			}
		}
	}

	// No specified list of hostage models, use the default.
	if ( 0 == numHostageModelsForMap )
	{
		Warning( "CHostage::Precache: missing hostage models for map %s. Adding the default models.\n", mapName );
		// Could not find a list of hostages for this map, so use the defaults.
		for ( int i=0; i<DEFAULT_NUM_HOSTAGE_MODELS; ++i )
		{
			PrecacheModel( HostageModel[i] );
		}
	}

	// Used for the PlayerAnimState activities etc.
	PrecacheModel( HOSTAGE_ANIM_MODEL );
	PrecacheModel( HOSTAGE_CARRY_PROP_MODEL );
	PrecacheModel( HOSTAGE_CARRY_VIEW_MODEL );

	PrecacheScriptSound( "Hostage.StartFollowCT" );
	PrecacheScriptSound( "Hostage.StartFollowCTGuardian" );
	PrecacheScriptSound( "Hostage.StopFollowCT" );
	PrecacheScriptSound( "Hostage.Pain" );

	PrecacheScriptSound( "Hostage.CutFreeWithDefuser" );
	PrecacheScriptSound( "Hostage.CutFreeWithoutDefuser" );
	PrecacheScriptSound( "Hostage.PickUp" );
	PrecacheScriptSound( "Hostage.Drop" );

	BaseClass::Precache();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHostage::ModifyOrAppendCriteria( AI_CriteriaSet& set )
{
	BaseClass::ModifyOrAppendCriteria( set );

	set.AppendCriteria( "rescued", (IsRescued() ? "yes" : "no") );
}

//-----------------------------------------------------------------------------------------------------
int CHostage::OnTakeDamage_Alive( const CTakeDamageInfo &info )
{
	float actualDamage = MIN( info.GetDamage(), GetHealth() );

	// say something
	if ( hostage_is_silent.GetBool() == false )
	{
		AIConcept_t concept( "Pain" );
		GetExpresser()->Speak( concept );
	}

	CCSPlayer *player = ToCSPlayer( info.GetAttacker() );

	if (player)
	{
		// [dwenger] Track which player injured the hostage
		player->SetInjuredAHostage(true);
		CSGameRules()->HostageInjured();

		CSGameRules()->ScoreHostageDamage( player, actualDamage );

		if ( !( player->m_iDisplayHistoryBits & DHF_HOSTAGE_INJURED ) )
		{
			player->HintMessage( "#Hint_careful_around_hostages", FALSE );
			player->m_iDisplayHistoryBits |= DHF_HOSTAGE_INJURED;
		}

		IGameEvent *event = gameeventmanager->CreateEvent( "hostage_hurt" );
		if ( event )
		{
			event->SetInt( "userid", player->GetUserID() );
			event->SetInt( "hostage", entindex() );
			event->SetInt( "priority", 5 );

			gameeventmanager->FireEvent( event );
		}

		player->AddAccountAward( PlayerCashAward::DAMAGE_HOSTAGE, CSGameRules()->PlayerCashAwardValue( PlayerCashAward::DAMAGE_HOSTAGE ) * (int)actualDamage );
	}

	return ( mp_hostages_takedamage.GetBool() ? BaseClass::OnTakeDamage_Alive( info ) : 1 );
}

//-----------------------------------------------------------------------------------------------------
/**
 * Modify damage the hostage takes by hitgroup
 */
float CHostage::GetModifiedDamage( float flDamage, int nHitGroup )
{
	switch ( nHitGroup )
	{
	case HITGROUP_GENERIC:	flDamage *=	1.75;	break;
	case HITGROUP_HEAD:		flDamage *=	2.5;	break;
	case HITGROUP_CHEST:	flDamage *=	1.5;	break;
	case HITGROUP_STOMACH:	flDamage *=	1.75;	break;
	case HITGROUP_LEFTARM:	flDamage *=	0.75;	break;
	case HITGROUP_RIGHTARM:	flDamage *=	0.75;	break;
	case HITGROUP_LEFTLEG:	flDamage *=	0.6;	break;
	case HITGROUP_RIGHTLEG:	flDamage *=	0.6;	break;
	default:				flDamage *=	1.5;	break;
	} 

	return flDamage;
}

//-----------------------------------------------------------------------------------------------------
void CHostage::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{
	CTakeDamageInfo scaledInfo = info;
	scaledInfo.SetDamage( GetModifiedDamage( info.GetDamage(), ptr->hitgroup ) );

	Vector vecOrigin = ptr->endpos - vecDir * 4;

	if ( m_takedamage )
	{
		CDisablePredictionFiltering disabler;

		AddMultiDamage( info, this );

		// This does smaller splotches on the guy and splats blood on the world.
		TraceBleed( info.GetDamage(), vecDir, ptr, info.GetDamageType() );

		CEffectData	data;
		data.m_vOrigin = ptr->endpos;
		data.m_vNormal = vecDir * -1;
		data.m_nEntIndex = ptr->m_pEnt ?  ptr->m_pEnt->entindex() : 0;
		data.m_flMagnitude =  info.GetDamage();

		DispatchEffect( "csblood", data );
	}

}

//-----------------------------------------------------------------------------------------------------
/**
 * Check for hostage-killer abuse
 */
void CHostage::CheckForHostageAbuse( CCSPlayer *player )
{
	int hostageKillLimit = mp_hostagepenalty.GetInt();

	if (hostageKillLimit > 0)
	{
		player->IncrementHostageKillsCount( 1 );

		if ( player->m_iHostagesKilled == hostageKillLimit - 1 )
		{
			//player->HintMessage( "#Hint_removed_for_next_hostage_killed", TRUE );
			ClientPrint( player, HUD_PRINTTALK, "#Hint_removed_for_next_hostage_killed" );
		}
		else if ( player->m_iHostagesKilled >= hostageKillLimit )
		{
			if ( !CSGameRules()->IsPlayingOffline() )
			{
				Msg( "Kicking client \"%s\" for killing too many hostages\n", player->GetPlayerName() );
			}
			engine->ServerCommand( UTIL_VarArgs( "kickid_ex %d %d For killing too many hostages\n", player->GetUserID(), CSGameRules()->IsPlayingOffline() ? 0 : 1 ) );
		}
	}
}


//-----------------------------------------------------------------------------------------------------
/**
 * Hostage was killed
 */
void CHostage::Event_Killed( const CTakeDamageInfo &info )
{
	// clear followers for music cue
	CCSPlayer *player = GetLeader();
	if (player)
	{
		player->DecrementNumFollowers();
	}


	// tell the game logic that we've died
	CSGameRules()->CheckWinConditions();

	// [tj] Let the game know that a hostage has been killed
	CSGameRules()->HostageKilled();

	CCSPlayer *attacker = ToCSPlayer( info.GetAttacker() );

	if (attacker)
	{
		CSGameRules()->ScoreHostageKilled( attacker );
		if ( !( attacker->m_iDisplayHistoryBits & DHF_HOSTAGE_KILLED ) )
		{
			attacker->HintMessage( "#Hint_lost_money", FALSE );
			attacker->m_iDisplayHistoryBits |= DHF_HOSTAGE_KILLED;
		}

		// monetary penalty for killing the hostage
		attacker->AddAccountAward( PlayerCashAward::KILL_HOSTAGE );

		// check for hostage-killer abuse
		if (attacker->GetTeamNumber() == TEAM_TERRORIST)
		{
			CheckForHostageAbuse( attacker );
		}
	}

	m_lastLeaderID = 0;

	SetUse( NULL );	
	BaseClass::Event_Killed( info );

	IGameEvent *event = gameeventmanager->CreateEvent("hostage_killed");
	if ( event )
	{
		event->SetInt( "userid", (attacker)?attacker->GetUserID():0 );
		event->SetInt( "hostage", entindex() );
		event->SetInt( "priority", 5 );
		gameeventmanager->FireEvent( event );
	}

	m_nHostageState = k_EHostageStates_Dead;
}


//-----------------------------------------------------------------------------------------------------
/**
 * Invoked when a Hostage touches a Rescue Zone
 */
void CHostage::HostageRescueZoneTouch( inputdata_t &inputdata )
{
	if (!m_isRescued)
	{
		m_isRescued = true;
		m_lastLeaderID = 0;

		SetUse( NULL );
		m_takedamage = DAMAGE_NO;

		// give rescuer a cash bonus
		CCSPlayer *player = GetLeader();
		if (player)
		{
			player->AddAccountAward( PlayerCashAward::RESCUED_HOSTAGE );
			player->DecrementNumFollowers();

			m_OnRescued.FireOutput(this, player);

			if ( HOSTAGE_RULE_CAN_PICKUP == 1 )
			{
				// Put him a short distance in front of the player.
				Vector vecTarget;
				Vector vForward;
				AngleVectors( player->GetAbsAngles(), &vForward );
				vForward.z = 0;
				vecTarget = player->GetAbsOrigin() + vForward * 100;

				Vector hullSizeNormal = VEC_HULL_MAX - VEC_HULL_MIN;
				Vector hullSizeCrouch = VEC_DUCK_HULL_MAX - VEC_DUCK_HULL_MIN;

				Vector newOrigin;
				VectorCopy( vecTarget, newOrigin );
				newOrigin += -0.5f * ( hullSizeNormal - hullSizeCrouch );

				trace_t trace;
				UTIL_TraceHull( newOrigin, vecTarget + Vector( 0, 0, 1 ), VEC_HULL_MIN, VEC_HULL_MAX, MASK_PLAYERSOLID, player, COLLISION_GROUP_PLAYER_MOVEMENT, &trace );

				if ( trace.fraction == 1.0 )
					DropHostage( vecTarget, true );  // drop the hostage in front of the player
				else
					DropHostage( player->GetAbsOrigin(), true );  // just drop the hostage where the player stands
			}
		}

		SetSolid( SOLID_NONE );
		SetSolidFlags( 0 );

		// start fading out
		m_flRescueStartTime = gpGlobals->curtime;
		// mark them as rescued
		m_nHostageState = k_EHostageStates_Rescued;

		// tell the bots someone has rescued a hostage
		IGameEvent *event = gameeventmanager->CreateEvent( "hostage_rescued" );
		if ( event )
		{
			event->SetInt( "userid", player ? player->GetUserID() : (-1) );
			event->SetInt( "hostage", entindex() );
			event->SetInt( "site", 0 ); // TODO add site index
			event->SetInt( "priority", 5 );
			gameeventmanager->FireEvent( event );
		}

		// update game rules
		CSGameRules()->m_iHostagesRescued++;

		// [dwenger] Hostage rescue achievement processing
		// Track rescuers
		if ( CSGameRules()->m_arrRescuers.Find( player ) == CSGameRules()->m_arrRescuers.InvalidIndex() )
			CSGameRules()->m_arrRescuers.AddToTail( player );

		bool roundWasAlreadyOver = (CSGameRules()->m_iRoundWinStatus != WINNER_NONE);

		//
		// Play the sound when every hostage is getting rescued
		//
		if (announceTimer.IsElapsed())
		{
			CSGameRules()->BroadcastSound( "Event.HostageRescued" );
			
			// avoid having the announcer talk over himself
			announceTimer.Start( 2.0f );
		}

		// MVP hook
		class CSimpleHostageRescueMVP : public CCSGameRules::ICalculateEndOfRoundMVPHook_t
		{
		public:
			virtual CCSPlayer* CalculateEndOfRoundMVP() OVERRIDE
			{
				if( player->HasControlledBotThisRound() )
				{ // [dkorus] if we controlled a bot this round, use standard MVP conditions
					return CSGameRules()->CalculateEndOfRoundMVP();
				}
				else
				{	// We got kills and rescued hostages, we deserve the MVP
					player->IncrementNumMVPs( CSMVP_HOSTAGERESCUE );
					return player;
				}
			}
			CCSPlayer *player;
		} mvpHook;
		mvpHook.player = player;

		if ( !roundWasAlreadyOver && player )
			CSGameRules()->m_pfnCalculateEndOfRoundMVPHook = &mvpHook;

		//
		// Check match win conditions and if round is now won award achievements
		//
		if ( CSGameRules()->CheckWinConditions() && player )
		{
			//Check hostage rescue achievements
			if ( !CSGameRules()->WasHostageKilled() && ( HOSTAGE_RULE_CAN_PICKUP || ( CSGameRules()->m_arrRescuers.Count() == 1 ) ) )
			{
				//check for unrescued hostages
				bool allHostagesRescued = true;				
				CHostage* hostage = NULL;
				int iNumHostages = g_Hostages.Count();

				for ( int i = 0 ; i < iNumHostages; i++ )
				{
					hostage = g_Hostages[i];

					if ( hostage->m_iHealth > 0 && !hostage->IsRescued() )
					{
						allHostagesRescued = false;
						break;
					}
				}

				if ( allHostagesRescued )
				{
					FOR_EACH_VEC( CSGameRules()->m_arrRescuers, iRescuer )
					{
						if ( CCSPlayer *pRoundRescuer = CSGameRules()->m_arrRescuers[iRescuer].Get() )
							pRoundRescuer->AwardAchievement(CSRescueAllHostagesInARound);
					}

					//[tj] fast version                
					if (gpGlobals->curtime - CSGameRules()->GetRoundStartTime() < AchievementConsts::FastHostageRescue_Time)
					{
						FOR_EACH_VEC( CSGameRules()->m_arrRescuers, iRescuer )
						{
							if ( CCSPlayer *pRoundRescuer = CSGameRules()->m_arrRescuers[iRescuer].Get() )
								pRoundRescuer->AwardAchievement(CSFastHostageRescue);
						}
					}
				}
			}
		}

		if ( !roundWasAlreadyOver && player )
			CSGameRules()->m_pfnCalculateEndOfRoundMVPHook = NULL;

		bool bWinningRescueEvent = !roundWasAlreadyOver && ( CSGameRules()->m_iRoundWinStatus != WINNER_NONE );
		if ( player )
		{
			CCS_GameStats.Event_HostageRescued(player);
			CSGameRules()->ScoreHostageRescue( player, this, bWinningRescueEvent );

			// record in matchstats
			if ( CSGameRules()->ShouldRecordMatchStats() )
			{
				int iCurrentRound = CSGameRules()->GetTotalRoundsPlayed();
				++ player->m_iMatchStats_Objective.GetForModify( iCurrentRound );

				// Keep track of Match stats in QMM data
				if ( CCSGameRules::CQMMPlayerData_t *pQMM = CSGameRules()->QueuedMatchmakingPlayersDataFind( player->GetHumanPlayerAccountID() ) )
				{
					pQMM->m_iMatchStats_Objective[ iCurrentRound ] = player->m_iMatchStats_Objective.Get( iCurrentRound );
				}
			}


		}

		if ( !roundWasAlreadyOver )
		{
			// All alive CTs also get assistance credit for hostage rescue.
			// This way hostage rescued after round was already won gives 1 pt to the rescuer
			// The winning rescue in pickup mode scores the rescuer 4 pts and all other alive
			// teammates get 1 pt for assist / suppressing fire.
			for ( int i = 1; i <= MAX_PLAYERS; i++ )
			{
				CCSPlayer* pCheckPlayer = (CCSPlayer*)UTIL_PlayerByIndex( i );
				if ( !pCheckPlayer )
					continue;
				if ( pCheckPlayer->GetTeamNumber() != TEAM_CT )
					continue;
				if ( pCheckPlayer->IsAlive() )
					CSGameRules()->ScoreHostageRescue( pCheckPlayer, this, false );
			}
		}
	}
}


//-----------------------------------------------------------------------------------------------------
/**
 * In contact with "other"
 */
void CHostage::Touch( CBaseEntity *other )
{
	BaseClass::Touch( other );

	if ( HOSTAGE_RULE_CAN_PICKUP == 1 )
		return;

	// allow players and other hostages to push me around
	if ( ( other->IsPlayer() && other->GetTeamNumber() == TEAM_CT ) || FClassnameIs( other, "hostage_entity" ) )
	{
		// only push in 2D
		Vector to = GetAbsOrigin() - other->GetAbsOrigin();
		to.z = 0.0f;
		to.NormalizeInPlace();
		
		const float pushForce = 500.0f;
		ApplyForce( pushForce * to );
	}
	else if ( m_inhibitDoorTimer.IsElapsed() &&
		( other->ClassMatches( "func_door*" ) || other->ClassMatches( "prop_door*" ) ) )
	{
		m_inhibitDoorTimer.Start( 3.0f );
		other->Use( this, this, USE_TOGGLE, 0.0f );
	}
	else if ( FClassnameIs( other, "func_breakable_surf" ) )
	{
		// break glass we've run into, so we're not stuck behind it
		CTakeDamageInfo damageInfo( this, this, 100.0f, DMG_CRUSH );
		other->OnTakeDamage( damageInfo );
	}
}


//-----------------------------------------------------------------------------------------------------
/** 
 * Hostage is stuck - attempt to wiggle out
 */
void CHostage::Wiggle( void )
{
	if (m_wiggleTimer.IsElapsed())
	{
		m_wiggleDirection = (NavRelativeDirType)RandomInt( 0, 3 );
		m_wiggleTimer.Start( RandomFloat( 0.3f, 0.5f ) );
	}

	Vector dir, lat;
	AngleVectors( GetAbsAngles(), &dir, &lat, NULL );

	const float speed = 500.0f;

	switch( m_wiggleDirection )
	{
		case LEFT:
			ApplyForce( speed * lat );
			break;

		case RIGHT:
			ApplyForce( -speed * lat );
			break;

		case FORWARD:
			ApplyForce( speed * dir );
			break;

		case BACKWARD:
			ApplyForce( -speed * dir );
			break;
	}

	const float minStuckJumpTime = 0.25f;
	if (m_pathFollower.GetStuckDuration() > minStuckJumpTime)
	{
		Jump();
	}
}


//-----------------------------------------------------------------------------------------------------
/**
 * Do following behavior
 */
void CHostage::UpdateFollowing( float deltaT )
{
	if ( HOSTAGE_RULE_CAN_PICKUP && IsFollowingSomeone() && GetLeader()  )
	{
		if ( m_nHostageState != k_EHostageStates_GettingPickedUp )
		{
			// only set the origin when we are carrying and not just when we are in the process of "getting picked up"
			SetAbsOrigin( GetLeader()->GetAbsOrigin() );	
		}
	
		return;
	}

	if ( !IsFollowingSomeone() && m_lastLeaderID != 0 )
	{
		// emit hostage_stops_following event
		IGameEvent *event = gameeventmanager->CreateEvent( "hostage_stops_following" );
		if ( event )
		{
			event->SetInt( "userid", m_lastLeaderID );
			event->SetInt( "hostage", entindex() );
			event->SetInt( "priority", 5 );
			gameeventmanager->FireEvent( event );
		}

		m_lastLeaderID = 0;
	}

	// if we have a leader, follow him
	CCSPlayer *leader = GetLeader();
	if (leader)
	{
		// if leader is dead, stop following him
		if (!leader->IsAlive())
		{
			Idle();
			return;
		}

		if ( HOSTAGE_RULE_CAN_PICKUP == 0 )
			m_nHostageState = k_EHostageStates_FollowingPlayer;

		// if leader has moved, repath
		if (m_path.IsValid())
		{
			Vector pathError = leader->GetAbsOrigin() - m_path.GetEndpoint();
			
			const float repathRange = 100.0f;
			if (pathError.IsLengthGreaterThan( repathRange ))
			{
				m_path.Invalidate();
			}
		}

		// build a path to our leader
		if (!m_path.IsValid() && m_repathTimer.IsElapsed())
		{
			const float repathInterval = 0.5f;
			m_repathTimer.Start( repathInterval );

			Vector from = GetAbsOrigin();
			Vector to = leader->GetAbsOrigin();
			HostagePathCost pathCost;

			m_path.Compute( from, to, pathCost );
			m_pathFollower.Reset();
		}


		// if our rescuer is too far away, give up
		const float giveUpRange = 2000.0f;
		const float maxPathLength = 4000.0f;
		Vector toLeader = leader->GetAbsOrigin() - GetAbsOrigin();
		if (toLeader.IsLengthGreaterThan( giveUpRange ) || (m_path.IsValid() && m_path.GetLength() > maxPathLength))
		{
			if ( hostage_debug.GetInt() < 2 )
			{
				Idle();
			}
			return;
		}

		
		// don't crowd the leader
		if (m_isWaitingForLeader)
		{
			// we are close to our leader and waiting for him to move
			const float waitRange = 150.0f;
			if (toLeader.IsLengthGreaterThan( waitRange ))
			{
				// leader has moved away - follow him
				m_isWaitingForLeader = false;
			}

			// face the leader
			//FaceTowards( leader->GetAbsOrigin(), deltaT );
		}
		else
		{
			// we are far from our leader, and need to check if we're close enough to wait
			const float nearRange = 125.0f;

			if (toLeader.IsLengthLessThan( nearRange ))
			{
				// we are close to the leader - wait for him to move
				m_isWaitingForLeader = true;
			}
		}

		if (!m_isWaitingForLeader)
		{
			// move along path towards the leader
			m_pathFollower.Update( deltaT, m_inhibitObstacleAvoidanceTimer.IsElapsed() );

			if (hostage_debug.GetBool())
			{
				m_pathFollower.Debug( true );
			}

			if (m_pathFollower.IsStuck())
			{
				Wiggle();
			}

			if (hostage_debug.GetBool())
			{
				m_path.Draw();
			}
		}
	}
}



//-----------------------------------------------------------------------------------------------------
void CHostage::AvoidPhysicsProps( void )
{
	if ( m_lifeState == LIFE_DEAD || HOSTAGE_RULE_CAN_PICKUP )
		return;

	CBaseEntity *props[512];
	int nEnts = GetPushawayEnts( this, props, ARRAYSIZE( props ), 0.0f, PARTITION_ENGINE_SOLID_EDICTS );

	for ( int i=0; i < nEnts; i++ )
	{
		// Don't respond to this entity on the client unless it has PHYSICS_MULTIPLAYER_FULL set.
		IMultiplayerPhysics *pInterface = dynamic_cast<IMultiplayerPhysics*>( props[i] );
		if ( pInterface && pInterface->GetMultiplayerPhysicsMode() != PHYSICS_MULTIPLAYER_SOLID )
			continue;

		const float minMass = 10.0f; // minimum mass that can push a player back
		const float maxMass = 30.0f; // cap at a decently large value
		float mass = maxMass;
		if ( pInterface )
		{
			mass = pInterface->GetMass();
		}
		mass = MIN( mass, maxMass );
		mass -= minMass;
		mass = MAX( mass, 0 );
		mass /= (maxMass-minMass); // bring into a 0..1 range

		// Push away from the collision point. The closer our center is to the collision point,
		// the harder we push away.
		Vector vPushAway = (WorldSpaceCenter() - props[i]->WorldSpaceCenter());
		float flDist = VectorNormalize( vPushAway );
		flDist = MAX( flDist, 1 );

		float flForce = sv_pushaway_hostage_force.GetFloat() / flDist * mass;
		flForce = MIN( flForce, sv_pushaway_max_hostage_force.GetFloat() );
		vPushAway *= flForce;

		ApplyForce( vPushAway );
	}

	//
	// Handle step and ledge "step-up" movement here, before m_accel is zero'd
	//
	if ( !m_accel.IsZero() )
	{
		trace_t trace;
		Vector start = GetAbsOrigin();
		Vector forward = m_accel;
		forward.NormalizeInPlace();
		UTIL_TraceEntity( this, start, start + forward, MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER, &trace );
		if ( !trace.startsolid && trace.fraction < 1.0f && trace.plane.normal.z < 0.7f )
		{
			Vector vecStepStart = trace.endpos + forward;
			vecStepStart.z += StepHeight;

			UTIL_TraceEntity( this, vecStepStart, vecStepStart + Vector( 0, 0, -StepHeight), MASK_PLAYERSOLID, this, COLLISION_GROUP_PLAYER, &trace );
			if ( !trace.startsolid && trace.fraction > 0 )
			{
				start.z += (StepHeight*(1-trace.fraction) + 1);
				SetAbsOrigin( start );
			}
		}
	}
}


//-----------------------------------------------------------------------------------------------------
/**
 * Push physics objects away from the hostage
 */
void CHostage::PushawayThink( void )
{
	if ( HOSTAGE_RULE_CAN_PICKUP )
		return;

	PerformObstaclePushaway( this );
	SetNextThink( gpGlobals->curtime + PUSHAWAY_THINK_INTERVAL, HOSTAGE_PUSHAWAY_THINK_CONTEXT );
}

//-----------------------------------------------------------------------------------------------------
/**
 * @TODO imitate player movement:
 * MoveHelperServer()->SetHost( this );
 * this->PlayerRunCommand( &cmd, MoveHelperServer() );
 */
void CHostage::PhysicsSimulate( void )
{
	BaseClass::PhysicsSimulate();

	SetAbsVelocity( m_vel );
}

//-----------------------------------------------------------------------------------------------------
/**
 * Update Hostage behaviors
 */
void CHostage::HostageThink( void )
{
	m_jumpedThisFrame = false;

	if (!m_isAdjusted)
	{
		m_isAdjusted = true;

		// HACK - figure out why the default bbox is 6 units too low
		SetCollisionBounds( HOSTAGE_BBOX_VEC_MIN, HOSTAGE_BBOX_VEC_MAX );
	}

	const float deltaT = ( HOSTAGE_RULE_CAN_PICKUP && IsFollowingSomeone() ) ? HOSTAGE_THINK_CARRIED_INTERVAL : HOSTAGE_THINK_INTERVAL;

	SetNextThink( gpGlobals->curtime + deltaT );

	//if the defusing process has started
	if ( HOSTAGE_RULE_CAN_PICKUP && m_nHostageState == k_EHostageStates_BeingUntied  && (m_pHostageGrabber != NULL))
	{
		//if the defusing process has not ended yet
		if ( gpGlobals->curtime < m_flGrabSuccessTime )
		{
			int iOnGround = FBitSet( m_pHostageGrabber->GetFlags(), FL_ONGROUND );

			const CUserCmd *pCmd = m_pHostageGrabber->GetLastUserCommand();
			bool bPlayerStoppedHoldingUse = !(pCmd->buttons & IN_USE) && (gpGlobals->curtime > m_fLastGrabTime + GRAB_HOSTAGE_LOCKIN_PERIOD);

			CConfigurationForHighPriorityUseEntity_t cfgUseEntity;
			bool bPlayerUseIsValidNow = m_pHostageGrabber->GetUseConfigurationForHighPriorityUseEntity( this, cfgUseEntity ) &&
				( cfgUseEntity.m_pEntity == this ) && cfgUseEntity.UseByPlayerNow( m_pHostageGrabber, cfgUseEntity.k_EPlayerUseType_Progress );

			//if the bomb defuser has stopped defusing the bomb
			if ( bPlayerStoppedHoldingUse || !bPlayerUseIsValidNow || !iOnGround )
			{
				if ( !iOnGround && m_pHostageGrabber->IsAlive() )
					ClientPrint( m_pHostageGrabber, HUD_PRINTCENTER, "#SFUI_Notice_Hostage_Pickup_Must_Be_On_Ground");

				// tell the bots someone has aborted defusing
// 				IGameEvent * event = gameeventmanager->CreateEvent( "bomb_abortdefuse" );
// 				if( event )
// 				{
// 					event->SetInt("userid", m_pHostageGrabber->GetUserID() );
// 					event->SetInt( "priority", 5 ); // bomb_abortdefuse
// 					gameeventmanager->FireEvent( event );
// 				}

				//cancel the progress bar
				m_pHostageGrabber->SetProgressBarTime( 0 );
				//m_pHostageGrabber->OnCanceledGrab();

				// release the player from being frozen
				m_pHostageGrabber->m_bIsGrabbingHostage = false;
				//m_bBeingGrabbed = false;
				m_nHostageState = k_EHostageStates_Idle;

				if ( m_bHandsHaveBeenCut || m_pHostageGrabber->HasDefuser() )
					StopSound( "Hostage.CutFreeWithDefuser" );
				else
					StopSound( "Hostage.CutFreeWithoutDefuser" );
			}
			return;
		}
		else if ( m_pHostageGrabber->IsAlive() )
		{
			Vector soundPosition = m_pHostageGrabber->GetAbsOrigin() + Vector( 0, 0, 5 );
			CBroadcastRecipientFilter filter;

			EmitSound( filter, entindex(), "Hostage.PickUp" );

			// release the player from being frozen
			m_pHostageGrabber->m_bIsGrabbingHostage = false;

			SetHostageStartFollowingPlayer( m_pHostageGrabber.Get() );

			// Clear their progress bar.
			m_pHostageGrabber->SetProgressBarTime( 0 );
			m_pHostageGrabber = NULL;
			m_flGrabbingLength = 10;

			m_vecGrabbedPos = GetAbsOrigin();

			m_nHostageState = k_EHostageStates_GettingPickedUp;

			return;
		}
	}

	// keep track of which Navigation Area we are in (or were in, if we're "off the mesh" right now)
	CNavArea *area = TheNavMesh->GetNavArea( GetAbsOrigin() );
	if (area != NULL && area != m_lastKnownArea)
	{
		// entered a new nav area
		m_lastKnownArea = area;
	}

	// do leader-following behavior, if necessary
	UpdateFollowing( deltaT );

	AvoidPhysicsProps();

	SmoothlyDropHostageToGround( GetAbsOrigin() );

	// update hostage velocity in the XY plane
	Vector vel = m_vel;

	if ( m_accel.IsZero() )
	{
		// Increase the damping if we've stopped adding acceleration.
		const float DAMPING = 4.0f;
		vel -=  ( vel * ( deltaT * DAMPING ) );

		if ( vel.AsVector2D().IsZero( 1.0f ) )
		{
			vel.x = 0.0f;
			vel.y = 0.0f;
		}
	}
	else
	{
		const float DAMPING = 2.0f;
		vel += deltaT * (m_accel - DAMPING * vel);
	}
	
	// leave Z component untouched
	vel.z = GetAbsVelocity().z;
	m_vel = vel;

	m_accel = Vector( 0, 0, 0 );

	// set animation to idle for now
	StudioFrameAdvance();

	int sequence = SelectWeightedSequence( ACT_IDLE );
	
	if (GetSequence() != sequence)
	{
		SetSequence( sequence );
	}

	m_PlayerAnimState->Update( GetAbsAngles()[YAW], GetAbsAngles()[PITCH] );

	if ( m_nHostageState == k_EHostageStates_GettingPickedUp )
	{
		SetSolid( SOLID_NONE );
		SetSolidFlags( 0 );
		SetUse( NULL );
		RemoveFlag( FL_OBJECT );

		if ( m_flGrabSuccessTime + CS_HOSTAGE_TRANSTIME_PICKUP < gpGlobals->curtime )
		{
			m_nHostageState = k_EHostageStates_BeingCarried;

			m_takedamage = DAMAGE_NO;
		}
		else if ( GetLeader() )
		{
			float flFrac = ((gpGlobals->curtime - m_flGrabSuccessTime) /  CS_HOSTAGE_TRANSTIME_PICKUP);
			Vector vecStart = m_vecGrabbedPos;
			Vector vecDest = GetLeader()->GetAbsOrigin() + Vector( 0, 0, 40 );
			Vector vecCur = vecStart + ((vecDest - vecStart) * flFrac);

			SetAbsOrigin( vecCur );
		}
	}
	else if ( m_nHostageState == k_EHostageStates_GettingDropped && m_flDropStartTime + CS_HOSTAGE_TRANSTIME_DROP < gpGlobals->curtime )
	{
		m_nHostageState = k_EHostageStates_Idle;

		SetSolid( SOLID_BBOX );
		SetCollisionGroup( COLLISION_GROUP_PLAYER );
		SetUse( &CHostage::HostageUse );
		AddFlag( FL_OBJECT );

		AddSolidFlags( FSOLID_NOT_STANDABLE );
	}
	else if ( m_nHostageState == k_EHostageStates_Rescued && m_flRescueStartTime + CS_HOSTAGE_TRANSTIME_RESCUE < gpGlobals->curtime )
	{
		// finished fading - remove us completely
// 		RemoveEffects( EF_NODRAW );
// 
// 		SetSolid( SOLID_BBOX );
// 		SetCollisionGroup( COLLISION_GROUP_PLAYER );
// 		SetUse( &CHostage::HostageUse );
// 
// 		AddSolidFlags( FSOLID_NOT_STANDABLE );

		SetSolid( SOLID_NONE );
		SetSolidFlags( 0 );
		RemoveFlag( FL_OBJECT );
	}
}

bool CHostage::IsBeingCarried( void )
{
	return IsFollowingSomeone();
}

//-----------------------------------------------------------------------------------------------------
bool CHostage::IsFollowingSomeone( void )
{
	return (m_leader.m_Value != NULL);
}

//-----------------------------------------------------------------------------------------------------
bool CHostage::IsFollowing( const CBaseEntity *entity )
{
	return (m_leader.m_Value == entity);
}

//-----------------------------------------------------------------------------------------------------
bool CHostage::IsValid( void ) const
{
	return (m_iHealth > 0 && !IsRescued());
}

//-----------------------------------------------------------------------------------------------------
bool CHostage::IsRescuable( void ) const
{
	return (m_iHealth > 0 && !IsRescued());
}

//-----------------------------------------------------------------------------------------------------
bool CHostage::IsRescued( void ) const
{
	return m_isRescued;
}

//-----------------------------------------------------------------------------------------------------
bool CHostage::IsOnGround( void ) const
{
	return (GetFlags() & FL_ONGROUND);
}

//-----------------------------------------------------------------------------------------------------
/**
 * Return true if hostage can see position
 */
bool CHostage::IsVisible( const Vector &pos, bool testFOV ) const
{
	trace_t result;
	UTIL_TraceLine( EyePosition(), pos, CONTENTS_SOLID, this, COLLISION_GROUP_NONE, &result );
	return (result.fraction >= 1.0f);
}

//-----------------------------------------------------------------------------------------------------
/**
 * Give bonus to CT's for talking to a hostage
 */
void CHostage::GiveCTUseBonus( CCSPlayer *rescuer )
{
	// money to team
	CSGameRules()->AddTeamAccount( TEAM_CT, TeamCashAward::HOSTAGE_INTERACTION, CSGameRules()->TeamCashAwardValue( TeamCashAward::HOSTAGE_INTERACTION ));

	// money to rescuer
	rescuer->AddAccountAward( PlayerCashAward::INTERACT_WITH_HOSTAGE );
}

//-----------------------------------------------------------------------------------------------------
/**
 * Stand idle
 */
void CHostage::Idle( void )
{
	m_leader = INVALID_EHANDLE;
}

//-----------------------------------------------------------------------------------------------------
/**
 * Begin following "leader"
 */
void CHostage::Follow( CCSPlayer *leader )
{
	// [dwenger] Set variable to track whether player is currently rescuing hostages
	if ( leader )
	{
		leader->IncrementNumFollowers( );
		leader->SetIsRescuing(true);

		// say something
		if ( hostage_is_silent.GetBool() == false )
		{
			AIConcept_t concept( "StartFollowing" );
			GetExpresser()->Speak( concept, "leaderteam:CT" );		
		}

		// emit hostage_follows event
		IGameEvent *event = gameeventmanager->CreateEvent( "hostage_follows" );
		if ( event )
		{
			event->SetInt( "userid", leader->GetUserID() );
			event->SetInt( "hostage", entindex() );
			event->SetInt( "priority", 5 );
			gameeventmanager->FireEvent( event );
		}
	}

	m_leader = leader;
	m_isWaitingForLeader = false;
	m_lastLeaderID = (leader) ? leader->GetUserID() : 0;

	if ( leader && HOSTAGE_RULE_CAN_PICKUP )
	{
		leader->GiveCarriedHostage( this );
	}
}


//-----------------------------------------------------------------------------------------------------
/**
 * Return our leader, or NULL
 */
CCSPlayer *CHostage::GetLeader( void ) const
{
	return ToCSPlayer( m_leader.m_Value );
}


//-----------------------------------------------------------------------------------------------------
/**
 * Invoked when a Hostage is "used" by a player
 */
void CHostage::HostageUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CCSPlayer *pPlayer = ToCSPlayer( pActivator );
	if ( !pPlayer )
		return;

	if ( CSGameRules() && CSGameRules()->IsWarmupPeriod() )
	{
		if ( pPlayer->m_iNextTimeCheck < gpGlobals->curtime )
		{
			ClientPrint( pPlayer, HUD_PRINTCENTER, "#Cstrike_TitlesTXT_Cannot_Move_Hostages_Warmup" );
			pPlayer->m_iNextTimeCheck = gpGlobals->curtime + 1.f;
		}
		return;
	}

	// limit use range
	float useRange = 1000.0f;
	Vector to = pActivator->GetAbsOrigin() - GetAbsOrigin();
	if (to.IsLengthGreaterThan( useRange ))
	{
		return;
	}

	if ( HOSTAGE_RULE_CAN_PICKUP == 1 )
	{
		if ( pPlayer->m_hCarriedHostage != NULL )
		{
			ClientPrint( pPlayer, HUD_PRINTCENTER, "#Cstrike_TitlesTXT_CanOnlyCarryOneHostage" );
			return;
		}

		if ( pPlayer && pPlayer->GetTeamNumber() == TEAM_TERRORIST )
		{
			SetHostageStartFollowingPlayer( pPlayer );
		}
		else
		{
			if ( m_nHostageState == k_EHostageStates_BeingUntied )
			{
				if ( pPlayer != m_pHostageGrabber )
				{
					if ( pPlayer->m_iNextTimeCheck < gpGlobals->curtime )
					{
						ClientPrint( pPlayer, HUD_PRINTCENTER, "#Cstrike_TitlesTXT_SomeonePickingUpHostage" );
						pPlayer->m_iNextTimeCheck = gpGlobals->curtime + 1.f;
					}
					return;
				}

				m_fLastGrabTime = gpGlobals->curtime;
			}
			else
			{
				Vector soundPosition = pPlayer->GetAbsOrigin() + Vector( 0, 0, 5 );
				CBroadcastRecipientFilter filter;

				if ( CSGameRules()->IsPlayingCooperativeGametype() )
				{
					EmitSound( filter, entindex(), "Hostage.CutFreeWithoutDefuser" );

					if ( !CSGameRules()->IsPlayingCoopMission() )
					{
						CBroadcastRecipientFilter filter;
						EmitSound( "Hostage.StartFollowCTGuardian" );
					}

					m_flGrabbingLength = 5;
				}
				else if ( m_bHandsHaveBeenCut || pPlayer->HasDefuser() )
				{
					EmitSound( filter, entindex(), "Hostage.CutFreeWithDefuser" );
					m_flGrabbingLength = 1;
				}
				else
				{
					EmitSound( filter, entindex(), "Hostage.CutFreeWithoutDefuser" );
					m_flGrabbingLength = 4;
				}
				
				m_flGrabSuccessTime = gpGlobals->curtime + m_flGrabbingLength;
				pPlayer->SetProgressBarTime( m_flGrabbingLength );

				m_pHostageGrabber = pPlayer;
				m_nHostageState = k_EHostageStates_BeingUntied;
				pPlayer->m_bIsGrabbingHostage = true;
			
				m_fLastGrabTime = gpGlobals->curtime;

				//start the progress bar

				//player->OnStartedGrab();

				m_OnHostageBeginGrab.FireOutput(this, pPlayer);
			}
		}
	}
	else
	{
		SetHostageStartFollowingPlayer( pPlayer );
	}
}

void CHostage::SetHostageStartFollowingPlayer( CCSPlayer *pPlayer )
{
	// only members of the CT team can use hostages (no T's or spectators)
	if (!hostage_debug.GetBool() && pPlayer->GetTeamNumber() != TEAM_CT)
	{
		if ( pPlayer->GetTeamNumber() == TEAM_TERRORIST )
		{
			if ( !(pPlayer->m_iDisplayHistoryBits & DHF_HOSTAGE_CTMOVE) )
			{
				pPlayer->m_iDisplayHistoryBits |= DHF_HOSTAGE_CTMOVE;
				pPlayer->HintMessage( "#Only_CT_Can_Move_Hostages", false, true );
			}
		}

		return;
	}

	CCSPlayer *leader = GetLeader();
	if( leader && !leader->IsAlive() )
	{
		Idle();
		leader = NULL;
	}

	// throttle how often leader can change
	if (!m_reuseTimer.IsElapsed())
	{
		return;
	}

	// give money bonus to first CT touching this hostage
	if (!m_hasBeenUsed)
	{
		m_hasBeenUsed = true;

		GiveCTUseBonus( pPlayer );
		if ( pPlayer )
		{
			CSGameRules()->ScoreHostageRescue( pPlayer, this, false );
		}

		CSGameRules()->HostageTouched();

		if ( pPlayer && !pPlayer->IsBot() && pPlayer->m_flGotHostageTalkTimer < gpGlobals->curtime )
		{
			pPlayer->Radio( "EscortingHostages"/*"EscortingHostages"*/, "#Cstrike_TitlesTXT_Got_Hostages", true );
			pPlayer->m_flGotHostageTalkTimer = gpGlobals->curtime + 10.0f;
		}

		m_bHandsHaveBeenCut = true;

		m_OnFirstPickedUp.FireOutput(this, pPlayer);
	}

	// Turn off T's ability to see hostage position anywhere.
	ClearSpotRule( CCSEntitySpotting::SPOT_RULE_ALWAYS_SEEN_BY_T );
	SetSpotRules( CCSEntitySpotting::SPOT_RULE_T );

	// Add hostage rescue time to round timer
	if ( !CSGameRules()->IsPlayingCoopMission() )
		CSGameRules()->AddHostageRescueTime();

	// if we are already following the player who used us, stop following
	if (IsFollowing( pPlayer ))
	{
		Idle();

		if ( hostage_is_silent.GetBool() == false )
		{
			// say something
			AIConcept_t concept( "StopFollowing" );
			GetExpresser()->Speak( concept, "leaderteam:CT" );
		}
	}
	else
	{
		// if we're already following a CT, ignore new uses
		if (IsFollowingSomeone())
		{
			return;
		}

		// start following
		Follow( pPlayer );
	}

	m_reuseTimer.Start( 1.0f );
}

void CHostage::SmoothlyDropHostageToGround( Vector vecPosition )
{
	if ( ( m_flDropStartTime > 0 ) && ( gpGlobals->curtime < m_flDropStartTime + hostage_drop_time.GetFloat() ) &&
		( hostage_drop_time.GetFloat() > 0 ) )
	{	// Perform smooth drop
		Vector GroundPos = DropToGround( this, vecPosition, HOSTAGE_BBOX_VEC_MIN, HOSTAGE_BBOX_VEC_MAX );

		// How far from ground should the hostage be now taking into account its starting location
		float flLengthOriginal = ( GroundPos - m_vecPositionWhenStartedDroppingToGround ).Length();
		float flLengthRemaining = ( GroundPos - vecPosition ).Length();
		float flDropTimeRemaining = ( m_flDropStartTime + hostage_drop_time.GetFloat() - gpGlobals->curtime );
		float flFractionOfDropDistanceRemaining = flDropTimeRemaining / hostage_drop_time.GetFloat();
		if ( flLengthRemaining > flLengthOriginal * flFractionOfDropDistanceRemaining )
		{
			flLengthRemaining = flLengthOriginal * flFractionOfDropDistanceRemaining;
			GroundPos = GroundPos + flLengthRemaining*( vecPosition - GroundPos ).Normalized();
		}
		else
			GroundPos = vecPosition;
		
		SetAbsOrigin( GroundPos );
	}
	else if ( ( m_flDropStartTime > 0 ) && ( m_nHostageSpawnRandomFactor != k_EHostageStates_GettingDropped ) )
	{	// Perform the final drop
		Vector GroundPos = DropToGround( this, vecPosition, HOSTAGE_BBOX_VEC_MIN, HOSTAGE_BBOX_VEC_MAX );
		SetAbsOrigin( GroundPos );
		m_flDropStartTime = 0;
	}
}

void CHostage::DropHostage( Vector vecPosition, bool bIsRescued )
{
	m_lastLeaderID = 0;

	CCSPlayer *player = GetLeader();
	if (player)
	{
		player->DecrementNumFollowers();
		player->RemoveCarriedHostage();

		if ( !bIsRescued )
			m_OnDroppedNotRescued.FireOutput(this, player);
	}

	if ( !bIsRescued )
	{
		SetSolid( SOLID_BBOX );
		SetCollisionGroup( COLLISION_GROUP_PLAYER );
		SetUse( &CHostage::HostageUse );

		AddSolidFlags( FSOLID_NOT_STANDABLE );
		// say something
		if ( hostage_is_silent.GetBool() == false )
		{
			AIConcept_t concept( "StopFollowing" );
			GetExpresser()->Speak( concept, "leaderteam:CT" );
		}

		m_nHostageState = k_EHostageStates_GettingDropped;
		m_flDropStartTime = gpGlobals->curtime;

		
	}

	//Vector soundPosition = GetAbsOrigin() + Vector( 0, 0, 5 );
	//CBroadcastRecipientFilter filter;
	//EmitSound( filter, entindex(), "Hostage.Drop" );

	m_leader = INVALID_EHANDLE;

	// Drop the hostage as much down as we can immediately
	Vector GroundPos = DropToGround( this, vecPosition, HOSTAGE_BBOX_VEC_MIN, HOSTAGE_BBOX_VEC_MAX );
	SetAbsOrigin( GroundPos );
	m_vecPositionWhenStartedDroppingToGround = GroundPos;

	SetGravity( 1.0 );

	// set up think callback
	SetNextThink( gpGlobals->curtime );
}

//--------------------------------------------------------------------------------------------------------------
/**
 * Rotate body to face towards "target"
 */
void CHostage::FaceTowards( const Vector &target, float deltaT )
{
	Vector to = target - GetFeet();
	to.z = 0.0f;

	QAngle desiredAngles;
	VectorAngles( to, desiredAngles );

	QAngle angles = GetAbsAngles();

	// The animstate system for hostages will smooth out the transition to this direction, so no need to double smooth it here.
	angles.y = desiredAngles.y;

	SetAbsAngles( angles );
}

//-----------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------

const Vector &CHostage::GetCentroid( void ) const
{
	static Vector centroid;
	
	centroid = GetFeet();
	centroid.z += HalfHumanHeight;

	return centroid;
}

//-----------------------------------------------------------------------------------------------------
/**
 * Return position of "feet" - point below centroid of improv at feet level
 */
const Vector &CHostage::GetFeet( void ) const
{
	static Vector feet;
	
	feet = GetAbsOrigin();

	return feet;
}

//-----------------------------------------------------------------------------------------------------
const Vector &CHostage::GetEyes( void ) const
{
	static Vector eyes;
	
	eyes = EyePosition();

	return eyes;
}

//-----------------------------------------------------------------------------------------------------
/**
 * Return direction of movement
 */
float CHostage::GetMoveAngle( void ) const
{
	return GetAbsAngles().y;
}

//-----------------------------------------------------------------------------------------------------
CNavArea *CHostage::GetLastKnownArea( void ) const
{
	return m_lastKnownArea;
}

//-----------------------------------------------------------------------------------------------------
/**
 * Find "simple" ground height, treating current nav area as part of the floo
 */
bool CHostage::GetSimpleGroundHeightWithFloor( const Vector &pos, float *height, Vector *normal )
{
	if (TheNavMesh->GetSimpleGroundHeight( pos, height, normal ))
	{
		// our current nav area also serves as a ground polygon
		if (m_lastKnownArea && m_lastKnownArea->IsOverlapping( pos ))
		{
			*height = MAX( (*height), m_lastKnownArea->GetZ( pos ) );
		}

		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------------------------------
void CHostage::Crouch( void )
{
	m_isCrouching = true;
}

//-----------------------------------------------------------------------------------------------------
/**
 * un-crouch
 */
void CHostage::StandUp( void )
{
	m_isCrouching = false;
}

//-----------------------------------------------------------------------------------------------------
bool CHostage::IsCrouching( void ) const
{
	return m_isCrouching;
}

//-----------------------------------------------------------------------------------------------------
/**
 * Initiate a jump
 */
void CHostage::Jump( void )
{
	// don't jump if the nav disallows it
	CNavArea *myArea = GetLastKnownArea();
	if ( myArea && myArea->HasAttributes( NAV_MESH_NO_JUMP ) )
		return;

	if (m_jumpTimer.IsElapsed() && IsOnGround())
	{
		const float minJumpInterval = 0.5f;
		m_jumpTimer.Start( minJumpInterval );

		Vector vel = GetAbsVelocity();
		vel.z += HOSTAGE_JUMP_POWER;
		SetAbsVelocity( vel );

		m_jumpedThisFrame = true;
		m_PlayerAnimState->DoAnimationEvent( PLAYERANIMEVENT_JUMP );
	}
}


//-----------------------------------------------------------------------------------------------------
bool CHostage::IsJumping( void ) const
{
	return !m_jumpTimer.IsElapsed();
}

//-----------------------------------------------------------------------------------------------------
/**
 * Set movement speed to running
 */
void CHostage::Run( void )
{
	m_isRunning = true;
}


//-----------------------------------------------------------------------------------------------------
/**
 * Set movement speed to walking
 */
void CHostage::Walk( void )
{
	m_isRunning = false;
}


//-----------------------------------------------------------------------------------------------------
bool CHostage::IsRunning( void ) const
{
	return m_isRunning;
}

//-----------------------------------------------------------------------------------------------------
/**
 * Invoked when a ladder is encountered while following a path
 */
void CHostage::StartLadder( const CNavLadder *ladder, NavTraverseType how, const Vector &approachPos, const Vector &departPos )
{
}

//-----------------------------------------------------------------------------------------------------
/**
 * Traverse given ladder
 */
bool CHostage::TraverseLadder( const CNavLadder *ladder, NavTraverseType how, const Vector &approachPos, const Vector &departPos, float deltaT )
{
	return false;
}

//-----------------------------------------------------------------------------------------------------
bool CHostage::IsUsingLadder( void ) const
{
	return false;
}

//-----------------------------------------------------------------------------------------------------
/**
 * Move Hostage directly toward "pathGoal", causing Hostage to track the current path.
 */
void CHostage::TrackPath( const Vector &pathGoal, float deltaT )
{
	// face in the direction of our motion
	FaceTowards( GetAbsOrigin() + 10.0f * m_vel, deltaT );

	if (GetFlags() & FL_ONGROUND)
	{
		// on the ground - move towards pathGoal
		Vector to = pathGoal - GetFeet();
		to.z = 0.0f;
		to.NormalizeInPlace();

		float flHostageSlowMultiplier = mp_hostages_run_speed_modifier.GetFloat();

		ApplyForce( (float)(MAX_HOSTAGE_MOVE_FORCE * flHostageSlowMultiplier) * to );
	}
	else
	{
		// in the air - continue forward motion
		Vector to;
		QAngle angles = GetAbsAngles();
		AngleVectors( angles, &to );

		const float AIR_FORCE = 350.0f;
		ApplyForce( AIR_FORCE * to );
	}
}

//-----------------------------------------------------------------------------------------------------
/**
 * Invoked when an improv reaches its MoveTo goal
 */
void CHostage::OnMoveToSuccess( const Vector &goal )
{
}

//-----------------------------------------------------------------------------------------------------
/**
 * Invoked when an improv fails to reach a MoveTo goal
 */
void CHostage::OnMoveToFailure( const Vector &goal, MoveToFailureType reason )
{
}


unsigned int CHostage::PhysicsSolidMaskForEntity() const
{
	return MASK_PLAYERSOLID;
}

