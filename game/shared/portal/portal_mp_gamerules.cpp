//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: The Half-Life 2 game rules, such as the relationship tables and ammo
//			damage cvars.
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "portal_mp_gamerules.h"
#include "viewport_panel_names.h"
#include "gameeventdefs.h"
#include <KeyValues.h>
#include "ammodef.h"
#include "tier1/fmtstr.h"
#include "hl2_shareddefs.h"
#include "portal_shareddefs.h"
#include "matchmaking/imatchframework.h"
#include "matchmaking/mm_helpers.h"

#ifndef CLIENT_DLL
#include "player_voice_listener.h"
#endif // CLIENT_DLL

#ifdef CLIENT_DLL

	#include "c_portal_player.h"
	#include "c_user_message_register.h"
	#include "c_keyvalue_saver.h"
	#include "radialmenu_taunt.h"
	#include "c_portal_mp_stats.h"

#else

	#include "eventqueue.h"
	#include "player.h"
	#include "gamerules.h"
	#include "game.h"
	#include "items.h"
	#include "entitylist.h"
	#include "mapentities.h"
	#include "in_buttons.h"
	#include <ctype.h>
	#include "voice_gamemgr.h"
	#include "iscorer.h"
	#include "portal_player.h"
	#include "team.h"
	#include "voice_gamemgr.h"
	#include "globalstate.h"
	#include "portal_mp_stats.h"
	#include "portal_ui_controller.h"

#endif	// CLIENT_DLL

#ifndef CLIENT_DLL

	extern void respawn(CBaseEntity *pEdict, bool fCopyCorpse);

	ConVar sv_hl2mp_weapon_respawn_time( "sv_hl2mp_weapon_respawn_time", "20", FCVAR_GAMEDLL | FCVAR_NOTIFY );
	ConVar sv_hl2mp_item_respawn_time( "sv_hl2mp_item_respawn_time", "30", FCVAR_GAMEDLL | FCVAR_NOTIFY );
	ConVar sv_report_client_settings("sv_report_client_settings", "0", FCVAR_GAMEDLL | FCVAR_NOTIFY );

	extern ConVar mp_dev_wait_for_other_player;
	extern ConVar mp_chattime;

	#define WEAPON_MAX_DISTANCE_FROM_SPAWN 64

#endif // #ifndef CLIENT_DLL

#ifdef CLIENT_DLL
	extern ConVar locator_lerp_rest;
	extern ConVar locator_start_at_crosshair;
	extern ConVar locator_topdown_style;
	extern ConVar locator_background_style;
	extern ConVar locator_background_color;
	extern ConVar locator_background_thickness_x;
	extern ConVar locator_background_thickness_y;
	extern ConVar locator_target_offset_x;
	extern ConVar locator_target_offset_y;
	extern ConVar locator_background_shift_x;
	extern ConVar locator_background_shift_y;
	extern ConVar locator_background_border_color;
	extern ConVar locator_icon_min_size_non_ss;
	extern ConVar locator_icon_max_size_non_ss;
	extern ConVar voice_icons_use_particles;
#endif // CLIENT_DLL


#define PropStringArrayArrayElement( _sendOrRecv, _structName, _arrayName, _firstElementNum, _secondElementNum, _secondArraySize, _stringSize ) \
	_sendOrRecv##PropString( #_arrayName"["#_firstElementNum"]["#_secondElementNum"]", offsetof( _structName, _arrayName ) + _firstElementNum * _secondArraySize * _stringSize + _secondElementNum * _stringSize, _stringSize )

#define PropStringArrayArray( _arrayName, _sendOrRecv, _outterNum, _innerNum ) \
	PropStringArrayArrayElement( _sendOrRecv, CPortalMPGameRules, _arrayName, _outterNum, _innerNum, MAX_PORTAL2_COOP_LEVELS_PER_BRANCH, MAX_PORTAL2_COOP_LEVEL_NAME_SIZE )

#define PropStringArrayArrayInnerList( _arrayName, _sendOrRecv, _outterNum ) \
	PropStringArrayArray( _arrayName, _sendOrRecv, _outterNum, 0 ), \
	PropStringArrayArray( _arrayName, _sendOrRecv, _outterNum, 1 ), \
	PropStringArrayArray( _arrayName, _sendOrRecv, _outterNum, 2 ), \
	PropStringArrayArray( _arrayName, _sendOrRecv, _outterNum, 3 ), \
	PropStringArrayArray( _arrayName, _sendOrRecv, _outterNum, 4 ), \
	PropStringArrayArray( _arrayName, _sendOrRecv, _outterNum, 5 ), \
	PropStringArrayArray( _arrayName, _sendOrRecv, _outterNum, 6 ), \
	PropStringArrayArray( _arrayName, _sendOrRecv, _outterNum, 7 ), \
	PropStringArrayArray( _arrayName, _sendOrRecv, _outterNum, 8 ), \
	PropStringArrayArray( _arrayName, _sendOrRecv, _outterNum, 9 ), \
	PropStringArrayArray( _arrayName, _sendOrRecv, _outterNum, 10 ), \
	PropStringArrayArray( _arrayName, _sendOrRecv, _outterNum, 11 ), \
	PropStringArrayArray( _arrayName, _sendOrRecv, _outterNum, 12 ), \
	PropStringArrayArray( _arrayName, _sendOrRecv, _outterNum, 13 ), \
	PropStringArrayArray( _arrayName, _sendOrRecv, _outterNum, 14 ), \
	PropStringArrayArray( _arrayName, _sendOrRecv, _outterNum, 15 )

#define PropStringArrayArrayOuterList( _arrayName, _sendOrRecv ) \
	PropStringArrayArrayInnerList( _arrayName, _sendOrRecv, 0 ), \
	PropStringArrayArrayInnerList( _arrayName, _sendOrRecv, 1 ), \
	PropStringArrayArrayInnerList( _arrayName, _sendOrRecv, 2 ), \
	PropStringArrayArrayInnerList( _arrayName, _sendOrRecv, 3 ), \
	PropStringArrayArrayInnerList( _arrayName, _sendOrRecv, 4 ), \
	PropStringArrayArrayInnerList( _arrayName, _sendOrRecv, 5 )

#define RecvPropStringArrayArray( _arrayName ) PropStringArrayArrayOuterList( _arrayName, Recv )

#define SendPropStringArrayArray( _arrayName ) PropStringArrayArrayOuterList( _arrayName, Send )

REGISTER_GAMERULES_CLASS( CPortalMPGameRules );

BEGIN_NETWORK_TABLE_NOBASE( CPortalMPGameRules, DT_PortalMPGameRules )

#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bTeamPlayEnabled ) ),
	RecvPropInt( RECVINFO( m_nCoopSectionIndex ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_nCoopBranchIndex ), RecvPropInt( RECVINFO( m_nCoopBranchIndex[0] ) ) ),
	RecvPropInt( RECVINFO( m_nSelectedDLCCourse ) ),
	RecvPropInt( RECVINFO( m_nNumPortalsPlaced ) ),
	RecvPropBool( RECVINFO( m_bMapNamesLoaded ) ),
	RecvPropStringArrayArray( m_szLevelNames ),
	RecvPropArray3( RECVINFO_ARRAY( m_nLevelCount ), RecvPropInt( RECVINFO( m_nLevelCount[0] ) ) ),
	// CREDITS
	RecvPropString( RECVINFO( m_szCoopCreditsNameSingle ) ),
	RecvPropString( RECVINFO( m_szCoopCreditsJobTitle ) ),
	RecvPropInt( RECVINFO( m_nCoopCreditsIndex ) ),
	RecvPropBool( RECVINFO( m_bCoopCreditsLoaded ) ),
	RecvPropInt( RECVINFO( m_nCoopCreditsState ) ),
	RecvPropInt( RECVINFO( m_nCoopCreditsScanState ) ),
	RecvPropBool( RECVINFO( m_bCoopFadeCreditsState ) ),
#else
	SendPropBool( SENDINFO( m_bTeamPlayEnabled ) ),
	SendPropInt( SENDINFO( m_nCoopSectionIndex ) ),
	SendPropArray3( SENDINFO_ARRAY3( m_nCoopBranchIndex ), SendPropInt( SENDINFO_ARRAY( m_nCoopBranchIndex ) ) ),
	SendPropInt( SENDINFO( m_nSelectedDLCCourse ) ),
	SendPropInt( SENDINFO( m_nNumPortalsPlaced ) ),
	SendPropBool( SENDINFO( m_bMapNamesLoaded ) ),
	SendPropStringArrayArray( m_szLevelNames ),
	SendPropArray3( SENDINFO_ARRAY3( m_nLevelCount ), SendPropInt( SENDINFO_ARRAY( m_nLevelCount ) ) ),
	// CREDITS
	SendPropString( SENDINFO( m_szCoopCreditsNameSingle ) ),
	SendPropString( SENDINFO( m_szCoopCreditsJobTitle ) ),
	SendPropInt( SENDINFO( m_nCoopCreditsIndex ) ),
	SendPropBool( SENDINFO( m_bCoopCreditsLoaded ) ),
	SendPropInt( SENDINFO( m_nCoopCreditsState ) ),
	SendPropInt( SENDINFO( m_nCoopCreditsScanState ) ),
	SendPropBool( SENDINFO( m_bCoopFadeCreditsState ) ),
#endif

END_NETWORK_TABLE()


IMPLEMENT_NETWORKCLASS_ALIASED( PortalMPGameRulesProxy, DT_PortalMPGameRulesProxy )
LINK_ENTITY_TO_CLASS_ALIASED( portalmp_gamerules, PortalMPGameRulesProxy );

static PortalMPViewVectors g_PortalMPViewVectors(
	Vector( 0, 0, 64 ),       //VEC_VIEW (m_vView) 

	Vector(-16, -16, 0 ),	  //VEC_HULL_MIN (m_vHullMin)
	Vector( 16,  16,  72 ),	  //VEC_HULL_MAX (m_vHullMax)

	Vector(-16, -16, 0 ),	  //VEC_DUCK_HULL_MIN (m_vDuckHullMin)
	Vector( 16,  16,  36 ),	  //VEC_DUCK_HULL_MAX	(m_vDuckHullMax)
	Vector( 0, 0, 28 ),		  //VEC_DUCK_VIEW		(m_vDuckView)

	Vector(-10, -10, -10 ),	  //VEC_OBS_HULL_MIN	(m_vObsHullMin)
	Vector( 10,  10,  10 ),	  //VEC_OBS_HULL_MAX	(m_vObsHullMax)

	Vector( 0, 0, 60 ),		  //VEC_DEAD_VIEWHEIGHT (m_vDeadViewHeight) // previously 14

	Vector(-16, -16, 0 ),	  //VEC_CROUCH_TRACE_MIN (m_vCrouchTraceMin)
	Vector( 16,  16,  60 )	  //VEC_CROUCH_TRACE_MAX (m_vCrouchTraceMax)
	);

static const char *s_PreserveEnts[] =
{
	"ai_network",
		"ai_hint",
		"hl2mp_gamerules",
		"team_manager",
		"player_manager",
		"env_soundscape",
		"env_soundscape_proxy",
		"env_soundscape_triggerable",
		"env_sun",
		"env_wind",
		"env_fog_controller",
		"func_brush",
		"func_wall",
		"func_buyzone",
		"func_illusionary",
		"infodecal",
		"info_projecteddecal",
		"info_node",
		"info_target",
		"info_node_hint",
		"info_player_deathmatch",
		"info_player_combine",
		"info_player_rebel",
		"info_map_parameters",
		"keyframe_rope",
		"move_rope",
		"info_ladder",
		"player",
		"point_viewcontrol",
		"scene_manager",
		"shadow_control",
		"sky_camera",
		"soundent",
		"trigger_soundscape",
		"viewmodel",
		"predicted_viewmodel",
		"worldspawn",
		"point_devshot_camera",
		"", // END Marker
};

CPortalMPGameRules *g_pPortalMPGameRules = NULL;

ConVar cm_is_current_community_map_coop( "cm_is_current_community_map_coop", "0", FCVAR_HIDDEN | FCVAR_REPLICATED );



#ifdef CLIENT_DLL
void RecvProxy_PortalMPRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	Assert( pRules );
	*pOut = pRules;
}

BEGIN_RECV_TABLE( CPortalMPGameRulesProxy, DT_PortalMPGameRulesProxy )
RecvPropDataTable( "portalmp_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE( DT_PortalMPGameRules ), RecvProxy_PortalMPRules )
END_RECV_TABLE()
#else
void* SendProxy_PortalMPRules( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	Assert( pRules );
	return pRules;
}

BEGIN_SEND_TABLE( CPortalMPGameRulesProxy, DT_PortalMPGameRulesProxy )
SendPropDataTable( "portalmp_gamerules_data", 0, &REFERENCE_SEND_TABLE( DT_PortalMPGameRules ), SendProxy_PortalMPRules )
END_SEND_TABLE()
#endif

#if defined ( GAME_DLL )
BEGIN_DATADESC( CPortalMPGameRulesProxy )
	// Inputs.
	DEFINE_INPUTFUNC( FIELD_INTEGER, "AddRedTeamScore", InputAddRedTeamScore ),
	DEFINE_INPUTFUNC( FIELD_INTEGER, "AddBlueTeamScore", InputAddBlueTeamScore ),
END_DATADESC()


// TEMP: This sucks long term... probably want some class to handle all team effecting operations.
void UTIL_Portal2MP_AddTeamScore( int iTeamNum, int iScoreIncrement )
{
	bool bSuccess = false;
	if ( g_Teams.IsValidIndex( iTeamNum ) )
	{
		CTeam *pTeam = g_Teams[ iTeamNum ];

		if ( pTeam )
		{
			pTeam->AddScore( iScoreIncrement );
			bSuccess = true;
		}
	}
	if ( !bSuccess )
	{
		Warning( "AddTeamScore failed to add score. Invalid team index?\n" );
		Assert ( 0 );
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CPortalMPGameRulesProxy::InputAddRedTeamScore( inputdata_t &inputdata )
{
	UTIL_Portal2MP_AddTeamScore( TEAM_RED, inputdata.value.Int() );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CPortalMPGameRulesProxy::InputAddBlueTeamScore( inputdata_t &inputdata )
{
	UTIL_Portal2MP_AddTeamScore( TEAM_BLUE, inputdata.value.Int() );
}
#endif // GAME_DLL

// NOTE: the indices here must match TEAM_TERRORIST, TEAM_CT, TEAM_SPECTATOR, etc.
char *sTeamNames[] =
{
	"Unassigned",
		"Spectator",
		"Combine",
		"Rebels",
};


CPortalMPGameRules::CPortalMPGameRules()
{
	g_pPortalMPGameRules = this;
#ifndef CLIENT_DLL
	// Create the team managers
	for ( int i = 0; i < ARRAYSIZE( sTeamNames ); i++ )
	{
		CTeam *pTeam = static_cast<CTeam*>(CreateEntityByName( "team_manager" ));
		pTeam->Init( sTeamNames[i], i );

		g_Teams.AddToTail( pTeam );
	}

	m_bTeamPlayEnabled = teamplay.GetBool();
	m_flIntermissionEndTime = 0.0f;
	m_flGameStartTime = 0;

	m_hRespawnableItemsAndWeapons.RemoveAll();
	m_tmNextPeriodicThink = 0;
	m_flRestartGameTime = 0;
	m_bCompleteReset = false;
	m_bHeardAllPlayersReady = false;
	m_bAwaitingReadyRestart = false;
	m_bGladosJustBlewUp = false;

	m_fNextDLCSelectTime = 0.0f;
	m_nCoopSectionIndex = 0;
	for (int i=0; i<MAX_PORTAL2_COOP_BRANCHES; i++)
	{
		m_nCoopBranchIndex.Set( i, 1 );
	}
	m_nSelectedDLCCourse = 0;
	m_nNumPortalsPlaced = 0;

	/// ????
	{
		g_pCVar->FindVar( "sv_maxreplay" )->SetValue( "1.5" );

		//Set health regeneration to enabled
		if ( !GlobalEntity_IsInTable( "player_regenerates_health" ) )
		{
			GlobalEntity_Add( MAKE_STRING("player_regenerates_health"), gpGlobals->mapname, GLOBAL_ON );
		}
		else
		{
			GlobalEntity_SetState( MAKE_STRING("player_regenerates_health"), GLOBAL_ON );
		}

		if ( !GlobalEntity_IsInTable( "player_blue_deaths" ) )
		{
			GlobalEntity_Add( MAKE_STRING( "player_blue_deaths" ), gpGlobals->mapname, GLOBAL_ON );
			GlobalEntity_SetCounter( MAKE_STRING( "player_blue_deaths" ), 0 );
		}

		if ( !GlobalEntity_IsInTable( "player_orange_deaths" ) )
		{
			GlobalEntity_Add( MAKE_STRING( "player_orange_deaths" ), gpGlobals->mapname, GLOBAL_ON );
			GlobalEntity_SetCounter( MAKE_STRING( "player_orange_deaths" ), 0 );
		}

		// Need a bunch of flags for Chet to hold global state per session
		if ( !GlobalEntity_IsInTable( "glados_spoken_flags0" ) )
		{
			GlobalEntity_Add( MAKE_STRING( "glados_spoken_flags0" ), gpGlobals->mapname, GLOBAL_ON );
			GlobalEntity_SetFlags( MAKE_STRING( "glados_spoken_flags0" ), 0 );
		}

		if ( !GlobalEntity_IsInTable( "glados_spoken_flags1" ) )
		{
			GlobalEntity_Add( MAKE_STRING( "glados_spoken_flags1" ), gpGlobals->mapname, GLOBAL_ON );
			GlobalEntity_SetFlags( MAKE_STRING( "glados_spoken_flags1" ), 0 );
		}

		if ( !GlobalEntity_IsInTable( "glados_spoken_flags2" ) )
		{
			GlobalEntity_Add( MAKE_STRING( "glados_spoken_flags2" ), gpGlobals->mapname, GLOBAL_ON );
			GlobalEntity_SetFlags( MAKE_STRING( "glados_spoken_flags2" ), 0 );
		}

		if ( !GlobalEntity_IsInTable( "glados_spoken_flags3" ) )
		{
			GlobalEntity_Add( MAKE_STRING( "glados_spoken_flags3" ), gpGlobals->mapname, GLOBAL_ON );
			GlobalEntity_SetFlags( MAKE_STRING( "glados_spoken_flags3" ), 0 );
		}

		if ( !GlobalEntity_IsInTable( "current_branch" ) )
		{
			GlobalEntity_Add( MAKE_STRING( "current_branch"), gpGlobals->mapname, GLOBAL_ON );
			GlobalEntity_SetCounter( MAKE_STRING( "current_branch" ), 0 );
		}

		if ( !GlobalEntity_IsInTable( "levels_completed_this_branch" ) )
		{
			GlobalEntity_Add( MAKE_STRING( "levels_completed_this_branch"), gpGlobals->mapname, GLOBAL_ON );
			GlobalEntity_SetCounter( MAKE_STRING( "levels_completed_this_branch" ), 0 );
		}

		if ( !GlobalEntity_IsInTable( "came_from_last_dlc_map" ) )
		{
			GlobalEntity_Add( MAKE_STRING( "came_from_last_dlc_map"), gpGlobals->mapname, GLOBAL_ON );
			GlobalEntity_SetFlags( MAKE_STRING( "came_from_last_dlc_map" ), 0 );
		}

		if ( !GlobalEntity_IsInTable( "have_seen_dlc_tubes_reveal" ) )
		{
			GlobalEntity_Add( MAKE_STRING( "have_seen_dlc_tubes_reveal"), gpGlobals->mapname, GLOBAL_ON );
			GlobalEntity_SetFlags( MAKE_STRING( "have_seen_dlc_tubes_reveal" ), 0 );
		}
	}

	m_bDataReceived[ 0 ] = m_bDataReceived[ 1 ] = false;

	m_nRPSWinCount[ 0 ] = m_nRPSWinCount[ 1 ] = 0;

	static ConVarRef flashlightbrightness( "r_flashlightbrightness" );
	if ( flashlightbrightness.IsValid() )
	{
		// All MP maps use this brightness and we can't set it in the map because there's no cheating in MP!
		flashlightbrightness.SetValue( 0.25f );
	}


#else
	locator_lerp_rest.SetValue( 0.0f );
	locator_start_at_crosshair.SetValue( 1 );
	locator_topdown_style.SetValue( 0 );
	locator_background_style.SetValue( 0 );
	locator_background_color.SetValue( "0 0 0 128");
	locator_target_offset_x.SetValue( 0 );
	locator_target_offset_y.SetValue( 0 );
	locator_background_thickness_x.SetValue( 12 );
	locator_background_thickness_y.SetValue( 12 );
	locator_background_shift_x.SetValue( 0 );
	locator_background_shift_y.SetValue( 0 );
	locator_background_border_color.SetValue( "32 32 32 64" );
	locator_icon_min_size_non_ss.SetValue( 1.0f );
	locator_icon_max_size_non_ss.SetValue( 1.15f );

	voice_icons_use_particles.SetValue( 1 );

#endif

	m_bMapNamesLoaded = false;
	m_bCoopCreditsLoaded = false;
	m_nCoopCreditsState = LIST_NAMES;
	m_nCoopCreditsScanState = 0;
	m_bCoopFadeCreditsState = false;
	memset( m_bLevelCompletions, 0, sizeof( m_bLevelCompletions ) );
}

const CViewVectors* CPortalMPGameRules::GetViewVectors()const
{
	return &g_PortalMPViewVectors;
}

const PortalMPViewVectors* CPortalMPGameRules::GetPortalMPViewVectors()const
{
	return &g_PortalMPViewVectors;
}

void CPortalMPGameRules::LevelInitPreEntity()
{
	m_bIsCoopInMapName = (V_stristr( MapName(), "coop" ) != NULL);
	m_bIs2GunsInMapName = (V_stristr( MapName(), "2guns" ) != NULL);
	m_bIsVSInMapName = (V_stristr( MapName(), "vs" ) != NULL);

#ifdef GAME_DLL
	m_nRPSWinCount[ 0 ] = m_nRPSWinCount[ 1 ] = 0;
	m_bGladosJustBlewUp = false;
	
	CPortalMPStats *pStats = GetPortalMPStats();
	if ( pStats )
	{
		pStats->ClearPerMapStats();
	}

	static ConVarRef flashlightbrightness( "r_flashlightbrightness" );
	if ( flashlightbrightness.IsValid() )
	{
		// All MP maps use this brightness and we can't set it in the map because there's no cheating in MP!
		flashlightbrightness.SetValue( 0.25f );
	}

	if ( IsLobbyMap() )
	{
		// For achievement STAYING_ALIVE:
		// reset number of levels completed when coming to the lobby
		GlobalEntity_SetCounter( MAKE_STRING( "levels_completed_this_branch" ), 0 );
		GlobalEntity_SetCounter( MAKE_STRING( "current_branch" ), -1 );
	}

#else

	m_bIsClientCrossplayingPCvsPC = IsPC() && !ClientIsCrossplayingWithConsole();

#endif
}

bool CPortalMPGameRules::IsCoOp( void )
{
	static ConVarRef coop_ref( "coop" );
	return m_bIsCoopInMapName || coop_ref.GetBool() || IsCommunityCoop();
}


bool CPortalMPGameRules::Is2GunsCoOp( void )
{
	return m_bIs2GunsInMapName;
}

bool CPortalMPGameRules::IsVS( void )
{
	return m_bIsVSInMapName;
}


#ifdef CLIENT_DLL
bool CPortalMPGameRules::IsChallengeMode()
{
	CBasePlayer* pPlayer = UTIL_PlayerByIndex( 1 );
	if ( pPlayer )
		return pPlayer->GetBonusChallenge() != 0;

	return false;
}
#endif


CPortalMPGameRules::~CPortalMPGameRules( void )
{
#ifdef CLIENT_DLL
	KeyValueSaver().WriteDirtyKeyValues( PORTAL2_MP_SAVE_FILE );
#endif

	g_pPortalMPGameRules = NULL;

#ifndef CLIENT_DLL
	// Note, don't delete each team since they are in the gEntList and will 
	// automatically be deleted from there, instead.
	g_Teams.Purge();
#endif
}

void CPortalMPGameRules::CreateStandardEntities( void )
{

#ifndef CLIENT_DLL
	// Create the entity that will send our data to the client.

	BaseClass::CreateStandardEntities();

#ifdef _DEBUG
	CBaseEntity *pEnt = 
#endif
		CBaseEntity::Create( "portalmp_gamerules", vec3_origin, vec3_angle );
	Assert( pEnt );
	
	// Create stats entities
	CPortalMPStats::InitPortalMPStats();
#endif
}

//=========================================================
// FlWeaponRespawnTime - what is the time in the future
// at which this weapon may spawn?
//=========================================================
float CPortalMPGameRules::FlWeaponRespawnTime( CBaseCombatWeapon *pWeapon )
{
#ifndef CLIENT_DLL
	if ( weaponstay.GetInt() > 0 )
	{
		// make sure it's only certain weapons
		if ( !(pWeapon->GetWeaponFlags() & ITEM_FLAG_LIMITINWORLD) )
		{
			return 0;		// weapon respawns almost instantly
		}
	}

	return sv_hl2mp_weapon_respawn_time.GetFloat();
#endif

	return 0;		// weapon respawns almost instantly
}


//Runs think for all player's conditions
//Need to do this here instead of the player so players that crash still run their important thinks
void CPortalMPGameRules::RunPlayerConditionThink( void )
{
	for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
	{
		CPortal_Player *pPlayer = ToPortalPlayer( UTIL_PlayerByIndex( i ) );

		if ( pPlayer )
		{
			pPlayer->m_Shared.ConditionGameRulesThink();
		}
	}
}

void CPortalMPGameRules::FrameUpdatePostEntityThink( void )
{
	RunPlayerConditionThink();

#ifndef CLIENT_DLL
	BaseClass::FrameUpdatePostEntityThink();
#endif
}

bool CPortalMPGameRules::IsIntermission( void )
{
#ifndef CLIENT_DLL
	return m_flIntermissionEndTime > gpGlobals->curtime;
#endif

	return false;
}

void CPortalMPGameRules::PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
#ifndef CLIENT_DLL
	if ( IsIntermission() )
		return;
	BaseClass::PlayerKilled( pVictim, info );
#endif
}


void CPortalMPGameRules::Think( void )
{
#ifndef CLIENT_DLL
	CGameRules::Think();
#endif
}

void CPortalMPGameRules::GoToIntermission( void )
{
#ifndef CLIENT_DLL
	if ( g_fGameOver )
		return;

	g_fGameOver = true;

	m_flIntermissionEndTime = gpGlobals->curtime + mp_chattime.GetInt();

	for ( int i = 0; i < MAX_PLAYERS; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		if ( !pPlayer )
			continue;

		pPlayer->ShowViewPortPanel( PANEL_SCOREBOARD );
		pPlayer->AddFlag( FL_FROZEN );
	}
#endif

}

bool CPortalMPGameRules::CheckGameOver()
{
#ifndef CLIENT_DLL
	if ( g_fGameOver )   // someone else quit the game already
	{
		// check to see if we should change levels now
		if ( m_flIntermissionEndTime < gpGlobals->curtime )
		{
			ChangeLevel(); // intermission is over			
		}

		return true;
	}
#endif

	return false;
}

// when we are within this close to running out of entities,  items 
// marked with the ITEM_FLAG_LIMITINWORLD will delay their respawn
#define ENTITY_INTOLERANCE	100

//=========================================================
// FlWeaponRespawnTime - Returns 0 if the weapon can respawn 
// now,  otherwise it returns the time at which it can try
// to spawn again.
//=========================================================
float CPortalMPGameRules::FlWeaponTryRespawn( CBaseCombatWeapon *pWeapon )
{
#ifndef CLIENT_DLL
	if ( pWeapon && (pWeapon->GetWeaponFlags() & ITEM_FLAG_LIMITINWORLD) )
	{
		if ( gEntList.NumberOfEntities() < (gpGlobals->maxEntities - ENTITY_INTOLERANCE) )
			return 0;

		// we're past the entity tolerance level,  so delay the respawn
		return FlWeaponRespawnTime( pWeapon );
	}
#endif
	return 0;
}

//=========================================================
// VecWeaponRespawnSpot - where should this weapon spawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CPortalMPGameRules::VecWeaponRespawnSpot( CBaseCombatWeapon *pWeapon )
{
//#pragma message( __FILE__ "(" __LINE__AS_STRING ") : warning custom: Disabled weapon respawn location code" )
#if 0
#ifndef CLIENT_DLL
	CWeaponHL2MPBase *pHL2Weapon = dynamic_cast< CWeaponHL2MPBase*>( pWeapon );

	if ( pHL2Weapon )
	{
		return pHL2Weapon->GetOriginalSpawnOrigin();
	}
#endif
#endif

	return pWeapon->GetAbsOrigin();
}

#ifndef CLIENT_DLL

CItem* IsManagedObjectAnItem( CBaseEntity *pObject )
{
	return dynamic_cast< CItem*>( pObject );
}

CWeaponPortalBase* IsManagedObjectAWeapon( CBaseEntity *pObject )
{
	return dynamic_cast<CWeaponPortalBase*>( pObject );
}

bool GetObjectsOriginalParameters( CBaseEntity *pObject, Vector &vOriginalOrigin, QAngle &vOriginalAngles )
{
	if ( CItem *pItem = IsManagedObjectAnItem( pObject ) )
	{
//#pragma message( __FILE__ "(" __LINE__AS_STRING ") : warning custom: Disabled rest time code" )
#if 0
		if ( pItem->m_flNextResetCheckTime > gpGlobals->curtime )
			return false;
#endif
		vOriginalOrigin = pItem->GetOriginalSpawnOrigin();
		vOriginalAngles = pItem->GetOriginalSpawnAngles();

#if 0
		pItem->m_flNextResetCheckTime = gpGlobals->curtime + sv_hl2mp_item_respawn_time.GetFloat();
#endif
		return true;
	}
	else if ( CWeaponPortalBase *pWeapon = IsManagedObjectAWeapon( pObject )) 
	{
		if ( pWeapon->m_flNextResetCheckTime > gpGlobals->curtime )
			return false;

		vOriginalOrigin = pWeapon->GetOriginalSpawnOrigin();
		vOriginalAngles = pWeapon->GetOriginalSpawnAngles();

		pWeapon->m_flNextResetCheckTime = gpGlobals->curtime + sv_hl2mp_weapon_respawn_time.GetFloat();
		return true;
	}

	return false;
}

void CPortalMPGameRules::ManageObjectRelocation( void )
{
	int iTotal = m_hRespawnableItemsAndWeapons.Count();

	if ( iTotal > 0 )
	{
		for ( int i = 0; i < iTotal; i++ )
		{
			CBaseEntity *pObject = m_hRespawnableItemsAndWeapons[i].Get();

			if ( pObject )
			{
				Vector vSpawOrigin;
				QAngle vSpawnAngles;

				if ( GetObjectsOriginalParameters( pObject, vSpawOrigin, vSpawnAngles ) == true )
				{
					float flDistanceFromSpawn = (pObject->GetAbsOrigin() - vSpawOrigin ).Length();

					if ( flDistanceFromSpawn > WEAPON_MAX_DISTANCE_FROM_SPAWN )
					{
						bool shouldReset = false;
						IPhysicsObject *pPhysics = pObject->VPhysicsGetObject();

						if ( pPhysics )
						{
							shouldReset = pPhysics->IsAsleep();
						}
						else
						{
							shouldReset = (pObject->GetFlags() & FL_ONGROUND) ? true : false;
						}

						if ( shouldReset )
						{
							pObject->Teleport( &vSpawOrigin, &vSpawnAngles, NULL );
							pObject->EmitSound( "AlyxEmp.Charge" );

							IPhysicsObject *pPhys = pObject->VPhysicsGetObject();

							if ( pPhys )
							{
								pPhys->Wake();
							}
						}
					}
				}
			}
		}
	}
}

//=========================================================
//AddLevelDesignerPlacedWeapon
//=========================================================
void CPortalMPGameRules::AddLevelDesignerPlacedObject( CBaseEntity *pEntity )
{
	if ( m_hRespawnableItemsAndWeapons.Find( pEntity ) == -1 )
	{
		m_hRespawnableItemsAndWeapons.AddToTail( pEntity );
	}
}

//=========================================================
//RemoveLevelDesignerPlacedWeapon
//=========================================================
void CPortalMPGameRules::RemoveLevelDesignerPlacedObject( CBaseEntity *pEntity )
{
	if ( m_hRespawnableItemsAndWeapons.Find( pEntity ) != -1 )
	{
		m_hRespawnableItemsAndWeapons.FindAndRemove( pEntity );
	}
}

//=========================================================
// Where should this item respawn?
// Some game variations may choose to randomize spawn locations
//=========================================================
Vector CPortalMPGameRules::VecItemRespawnSpot( CItem *pItem )
{
	return pItem->GetOriginalSpawnOrigin();
}

//=========================================================
// What angles should this item use to respawn?
//=========================================================
QAngle CPortalMPGameRules::VecItemRespawnAngles( CItem *pItem )
{
	return pItem->GetOriginalSpawnAngles();
}

//=========================================================
// At what time in the future may this Item respawn?
//=========================================================
float CPortalMPGameRules::FlItemRespawnTime( CItem *pItem )
{
	return sv_hl2mp_item_respawn_time.GetFloat();
}


//=========================================================
// CanHaveWeapon - returns false if the player is not allowed
// to pick up this weapon
//=========================================================
bool CPortalMPGameRules::CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pItem )
{
	if ( weaponstay.GetInt() > 0 )
	{
		if ( pPlayer->Weapon_OwnsThisType( pItem->GetClassname(), pItem->GetSubType() ) )
			return false;
	}

	return BaseClass::CanHavePlayerItem( pPlayer, pItem );
}

#endif

//=========================================================
// WeaponShouldRespawn - any conditions inhibiting the
// respawning of this weapon?
//=========================================================
int CPortalMPGameRules::WeaponShouldRespawn( CBaseCombatWeapon *pWeapon )
{
#ifndef CLIENT_DLL
	if ( pWeapon->HasSpawnFlags( SF_NORESPAWN ) )
	{
		return GR_WEAPON_RESPAWN_NO;
	}
#endif

	return GR_WEAPON_RESPAWN_YES;
}


//=========================================================
// Deathnotice. 
//=========================================================
void CPortalMPGameRules::DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
#ifndef CLIENT_DLL
	// Work out what killed the player, and send a message to all clients about it
	const char *killer_weapon_name = "world";		// by default, the player is killed by the world
	int killer_ID = 0;

	// Find the killer & the scorer
	CBaseEntity *pInflictor = info.GetInflictor();
	CBaseEntity *pKiller = info.GetAttacker();
	CBasePlayer *pScorer = GetDeathScorer( pKiller, pInflictor );

	// Custom kill type?
	if ( info.GetDamageCustom() )
	{
		killer_weapon_name = GetDamageCustomString( info );
		if ( pScorer )
		{
			killer_ID = pScorer->GetUserID();
		}
	}
	else
	{
		// Is the killer a client?
		if ( pScorer )
		{
			killer_ID = pScorer->GetUserID();

			if ( pInflictor )
			{
				if ( pInflictor == pScorer )
				{
					// If the inflictor is the killer,  then it must be their current weapon doing the damage
					if ( pScorer->GetActiveWeapon() )
					{
						killer_weapon_name = pScorer->GetActiveWeapon()->GetClassname();
					}
				}
				else
				{
					killer_weapon_name = pInflictor->GetClassname();  // it's just that easy
				}
			}
		}
		else
		{
			killer_weapon_name = pInflictor->GetClassname();
		}

		// strip the NPC_* or weapon_* from the inflictor's classname
		if ( strncmp( killer_weapon_name, "weapon_", 7 ) == 0 )
		{
			killer_weapon_name += 7;
		}
		else if ( strncmp( killer_weapon_name, "npc_", 4 ) == 0 )
		{
			killer_weapon_name += 4;
		}
		else if ( strncmp( killer_weapon_name, "func_", 5 ) == 0 )
		{
			killer_weapon_name += 5;
		}
		else if ( strstr( killer_weapon_name, "physics" ) )
		{
			killer_weapon_name = "physics";
		}

		if ( strcmp( killer_weapon_name, "prop_combine_ball" ) == 0 )
		{
			killer_weapon_name = "combine_ball";
		}
		else if ( strcmp( killer_weapon_name, "grenade_ar2" ) == 0 )
		{
			killer_weapon_name = "smg1_grenade";
		}
		else if ( strcmp( killer_weapon_name, "satchel" ) == 0 || strcmp( killer_weapon_name, "tripmine" ) == 0)
		{
			killer_weapon_name = "slam";
		}


	}

	IGameEvent *event = gameeventmanager->CreateEvent( "player_death" );
	if( event )
	{
		event->SetInt("userid", pVictim->GetUserID() );
		event->SetInt("attacker", killer_ID );
		event->SetString("weapon", killer_weapon_name );
		event->SetInt( "priority", 7 );
		gameeventmanager->FireEvent( event );
	}

	bool bIsBlue = pVictim->GetTeamNumber() == TEAM_BLUE;

	if ( bIsBlue )
	{
		GlobalEntity_AddToCounter( MAKE_STRING( "player_blue_deaths" ), 1 );
		engine->ClientCommand( pVictim->edict(), "signify death_blue -1 0 %.2f %.2f %.2f 0 0 1", pVictim->GetAbsOrigin().x, pVictim->GetAbsOrigin().y, pVictim->GetAbsOrigin().z + 32.0f );
	}
	else
	{
		GlobalEntity_AddToCounter( MAKE_STRING( "player_orange_deaths" ), 1 );
		engine->ClientCommand( pVictim->edict(), "signify death_orange -1 0 %.2f %.2f %.2f 0 0 1", pVictim->GetAbsOrigin().x, pVictim->GetAbsOrigin().y, pVictim->GetAbsOrigin().z + 32.0f );
	}
#endif

}

void CPortalMPGameRules::ClientSettingsChanged( CBasePlayer *pPlayer )
{
#ifndef CLIENT_DLL
	if ( sv_report_client_settings.GetInt() == 1 )
	{
		UTIL_LogPrintf( "\"%s\" cl_cmdrate = \"%s\"\n", pPlayer->GetPlayerName(), engine->GetClientConVarValue( pPlayer->entindex(), "cl_cmdrate" ));
	}

	BaseClass::ClientSettingsChanged( pPlayer );
#endif

}

int CPortalMPGameRules::PlayerRelationship( CBaseEntity *pPlayer, CBaseEntity *pTarget )
{
#ifndef CLIENT_DLL
	// half life multiplay has a simple concept of Player Relationships.
	// you are either on another player's team, or you are not.
	if ( !pPlayer || !pTarget || !pTarget->IsPlayer() || IsTeamplay() == false )
		return GR_NOTTEAMMATE;

	if ( (*GetTeamID(pPlayer) != '\0') && (*GetTeamID(pTarget) != '\0') && !stricmp( GetTeamID(pPlayer), GetTeamID(pTarget) ) )
	{
		return GR_TEAMMATE;
	}
#endif

	return GR_NOTTEAMMATE;
}

const char *CPortalMPGameRules::GetGameDescription( void )
{
	return "Portal 2 Coop";
} 


float CPortalMPGameRules::GetMapRemainingTime()
{
	// if timelimit is disabled, return 0
	if ( mp_timelimit.GetInt() <= 0 )
		return 0;

	// timelimit is in minutes

	float timeleft = (m_flGameStartTime + mp_timelimit.GetInt() * 60.0f ) - gpGlobals->curtime;

	return timeleft;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPortalMPGameRules::Precache( void )
{
#ifndef CLIENT_DLL
	BaseClass::Precache();
#endif
	CBaseEntity::PrecacheScriptSound( "AlyxEmp.Charge" );
}

bool CPortalMPGameRules::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
	if ( collisionGroup0 > collisionGroup1 )
	{
		// swap so that lowest is always first
		V_swap(collisionGroup0,collisionGroup1);
	}

	if ( (collisionGroup0 == COLLISION_GROUP_PLAYER || collisionGroup0 == COLLISION_GROUP_PLAYER_MOVEMENT) &&
		collisionGroup1 == COLLISION_GROUP_WEAPON )
	{
		return false;
	}

	// Cubes shouldn't collide with debris but should otherwise act like COLLISION_GROUP_NONE
	if( collisionGroup1 == COLLISION_GROUP_WEIGHTED_CUBE && collisionGroup0 == COLLISION_GROUP_DEBRIS )
		return false;

	if( collisionGroup0 == COLLISION_GROUP_WEIGHTED_CUBE )
		collisionGroup0 = COLLISION_GROUP_NONE;

	if( collisionGroup1 == COLLISION_GROUP_WEIGHTED_CUBE )
		collisionGroup1 = COLLISION_GROUP_NONE;

	return BaseClass::ShouldCollide( collisionGroup0, collisionGroup1 ); 

}

#if !defined ( CLIENT_DLL )
const char *CPortalMPGameRules::GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer )
{
	return "";
}
#endif

bool CPortalMPGameRules::ClientCommand( CBaseEntity *pEdict, const CCommand &args )
{
	const char *pcmd = args[0];
	if ( FStrEq( pcmd, "lobby_select_day" ) )
	{
		if ( args.ArgC() < 2 )
			return true;

		int nDay = atoi( args[1] );
//		Msg("Selecting day %d\n", nDay );

		m_nCoopSectionIndex = nDay;
		return true;
	}
	else if ( FStrEq( pcmd, "coop_set_credits_jobtitle" ) )
	{
		if ( args.ArgC() < 2 )
			return true;

		//int nIndex = atoi( args[1] );
		V_strcpy( m_szCoopCreditsJobTitle.GetForModify(), args[1] );
		return true;
	}
	else if ( FStrEq( pcmd, "coop_set_credits_index" ) )
	{
		if ( args.ArgC() < 3 )
			return true;

		int nIndex = atoi( args[1] );
		int nScan = atoi( args[2] );
		//Msg("Selecting index %d\n", nIndex );
		//Msg("Scanning type %d\n", nScan );
		if ( nIndex < m_szCoopCreditsNames.Count() )
		{
			V_strcpy( m_szCoopCreditsNameSingle.GetForModify(), m_szCoopCreditsNames[ nIndex ] );
			m_nCoopCreditsIndex = nIndex;
		}
		m_nCoopCreditsScanState = nScan;
		return true;
	}
	else if ( FStrEq( pcmd, "coop_set_credits_state" ) )
	{
		if ( args.ArgC() < 2 )
			return true;

		int nIndex = atoi( args[1] );
		//Msg("Setting credits state to %d\n", nIndex );
		m_nCoopCreditsState = nIndex;
		m_bCoopFadeCreditsState = !m_bCoopFadeCreditsState;
		return true;
	}
	else if ( FStrEq( pcmd, "coop_lobby_select_level" ) )
	{
		if ( args.ArgC() < 3 )
		{
			Msg("Not enough arguments for coop_lobby_select_level.  Format should be: coop_lobby_select_level <branch#> <level#>\n" );
			return true;
		}

		int nBranch = atoi( args[1] );
		nBranch--;
		if ( nBranch < 0 || nBranch >= MAX_PORTAL2_COOP_BRANCHES )
		{
			Msg("Branch argument is out of range for coop_lobby_select_level.  It needs to be a positive number less than %d.\n", MAX_PORTAL2_COOP_BRANCHES );
			return true;
		}
		int nLevel = atoi( args[2] );
		int nSkipRequirement = atoi( args[3] );

		// Start at the back an loop toward the start
		int nMaxLevel;
		for ( nMaxLevel = MAX_PORTAL2_COOP_LEVELS_PER_BRANCH - 1; nMaxLevel >= 0; --nMaxLevel )
		{
			// Not a valid level, keep going
			if ( m_szLevelNames[ nBranch ][ nMaxLevel ][ 0 ] == '\0' )
				continue;

			// First valid level? We're done if skipping requirement
			if ( nSkipRequirement )
				break;

			// First completed level for either player? This is the max
			if ( m_bLevelCompletions[ 0 ][ nBranch ][ nMaxLevel ] || m_bLevelCompletions[ 1 ][ nBranch ][ nMaxLevel ] )
				break;
		}

		// Scoot back up to the last level not completed by either player
		nMaxLevel++;

		nLevel = MIN( nLevel, MIN( nMaxLevel + 1, m_nLevelCount[ nBranch ] ) );

		m_nCoopBranchIndex.Set( nBranch, nLevel );
		//Msg("Selecting branch %d, level %d\n", nBranch+1, nLevel );

#ifdef GAME_DLL
		if ( !IsLobbyMap() )
		{
			if ( nLevel >= 0 )
			{
				if ( PortalMPGameRules()->IsPlayerLevelInBranchComplete( 0, nBranch, nLevel ) && PortalMPGameRules()->IsPlayerLevelInBranchComplete( 1, nBranch, nLevel ) )
				{
					IGameEvent *event = gameeventmanager->CreateEvent( "map_already_completed" );
					if ( event )
					{
						gameeventmanager->FireEvent( event );
					}
				}
			}

			int nCurrentBranch = GlobalEntity_GetCounter( MAKE_STRING( "current_branch" ) );
			if ( nCurrentBranch != ( nBranch + 1 ) || nLevel <= 1 )
			{
				// For achievement STAYING_ALIVE:
				// Reset both players' death counts and level tracking on day change.
				GlobalEntity_SetCounter( MAKE_STRING( "player_blue_deaths" ), 0 );
				GlobalEntity_SetCounter( MAKE_STRING( "player_orange_deaths" ), 0 );

				GlobalEntity_SetCounter( MAKE_STRING( "levels_completed_this_branch" ), 0 );
				GlobalEntity_SetCounter( MAKE_STRING( "current_branch" ), ( nBranch + 1 ) );
			}
		}
#endif

		return true;
	}
	else if ( FStrEq( pcmd, "coop_lobby_select_course" ) )
	{
		if ( args.ArgC() < 2 )
		{
			Msg("Not enough arguments for coop_lobby_select_course.  Format should be: coop_lobby_select_course <increment/decrement#>\n" );
			return true;
		}

		// HACK HACK: For some reason this command is fired 3 times every time I press the button
		// The commands above have this same problem, but they aren't relative
		if ( m_fNextDLCSelectTime < gpGlobals->curtime )
		{
			m_fNextDLCSelectTime = gpGlobals->curtime + 0.01f;

			m_nSelectedDLCCourse += atoi( args[ 1 ] );
		}
	}

#ifndef CLIENT_DLL
	if( BaseClass::ClientCommand( pEdict, args ) )
		return true;


	CBasePlayer *pPlayer = (CBasePlayer *) pEdict;

	if ( pPlayer->ClientCommand( args ) )
		return true;
#endif

	return false;
}

// shared ammo definition
// JAY: Trying to make a more physical bullet response
#define BULLET_MASS_GRAINS_TO_LB(grains)	(0.002285*(grains)/16.0f)
#define BULLET_MASS_GRAINS_TO_KG(grains)	lbs2kg(BULLET_MASS_GRAINS_TO_LB(grains))

// exaggerate all of the forces, but use real numbers to keep them consistent
#define BULLET_IMPULSE_EXAGGERATION			3.5
// convert a velocity in ft/sec and a mass in grains to an impulse in kg in/s
#define BULLET_IMPULSE(grains, ftpersec)	((ftpersec)*12*BULLET_MASS_GRAINS_TO_KG(grains)*BULLET_IMPULSE_EXAGGERATION)


#ifdef CLIENT_DLL

ConVar cl_autowepswitch(
						"cl_autowepswitch",
						"1",
						FCVAR_ARCHIVE | FCVAR_USERINFO,
						"Automatically switch to picked up weapons (if more powerful)" );

#else


bool CPortalMPGameRules::FShouldSwitchWeapon( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon )
{		
	if ( pPlayer->GetActiveWeapon() && pPlayer->IsNetClient() )
	{
		// Player has an active item, so let's check cl_autowepswitch.
		const char *cl_autowepswitch = engine->GetClientConVarValue( pPlayer->entindex(), "cl_autowepswitch" );
		if ( cl_autowepswitch && atoi( cl_autowepswitch ) <= 0 )
		{
			return false;
		}
	}

	return BaseClass::FShouldSwitchWeapon( pPlayer, pWeapon );
}

#endif

#ifndef CLIENT_DLL

void CPortalMPGameRules::PlayerSpawn( CBasePlayer *pPlayer )
{
	bool		addDefault;
	CBaseEntity	*pWeaponEntity = NULL;

	//don't equip the suit
	//pPlayer->EquipSuit();

	addDefault = true;

	while ( (pWeaponEntity = gEntList.FindEntityByClassname( pWeaponEntity, "game_player_equip" )) != NULL)
	{
		pWeaponEntity->Touch( pPlayer );
		addDefault = false;
	}
}

bool CPortalMPGameRules::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	if ( m_bGladosJustBlewUp == true )
		return false;

	return true;
}

void CPortalMPGameRules::ClientCommandKeyValues( edict_t *pEntity, KeyValues *pKeyValues )
{
	if ( !pKeyValues || !pEntity || !pEntity->GetIServerEntity() )
		return;

	CPortal_Player *pPlayer = assert_cast< CPortal_Player* >( pEntity->GetIServerEntity()->GetBaseEntity() );
	if ( !pPlayer )
		return;

	char const *szCommand = pKeyValues->GetName();

	if ( FStrEq( szCommand, "read_stats" ) )
	{
		int nPlayer = pPlayer->GetTeamNumber() == TEAM_BLUE ? 0 : 1;
		m_bDataReceived[ nPlayer ] = true;

		int nStrLen = V_strlen( "MP.complete." );
		for ( KeyValues *kvValue = pKeyValues->GetFirstValue(); kvValue; kvValue = kvValue->GetNextValue() )
		{
			char const *pchName = kvValue->GetName();

			if ( StringHasPrefix( pchName, "MP.complete." ) )
			{
				pchName += nStrLen;
			}
			else
			{
				continue;
			}

			// Request key values for all the levels
			if ( kvValue->GetInt( "" ) != 0 )
			{
				SetMapCompleteSimple( nPlayer, pchName, kvValue->GetInt() != 0 );
			}
		}

		if ( ( m_bDataReceived[ 0 ] && m_bDataReceived[ 1 ] ) || (!mp_dev_wait_for_other_player.GetBool() || (IsLocalSplitScreen() && IsCreditsMap()) ) || IsCommunityCoopHub() )
		{
			SendAllMapCompleteData();

			StartPlayerTransitionThinks();
		}
	}
	else if ( FStrEq( szCommand, "read_awards" ) )
	{
		// TODO
	}
	else if ( FStrEq( szCommand, "read_leaderboard" ) )
	{
		KeyValuesDumpAsDevMsg( pKeyValues, 1 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Damage (applied per second) value of the npc_laser_turret
//-----------------------------------------------------------------------------
float CPortalMPGameRules::GetLaserTurretDamage( void )
{
	switch( GetSkillLevel() )
	{
	case SKILL_EASY:
		return 120.0f;

	case SKILL_MEDIUM:
		return 200.0f;

	case SKILL_HARD:
		return 200.0f;

	default:
		return 100.0f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Movement speed of the turret. 
//-----------------------------------------------------------------------------
float CPortalMPGameRules::GetLaserTurretMoveSpeed( void )
{
	switch( GetSkillLevel() )
	{
	case SKILL_EASY:
		return 0.6f;

	case SKILL_MEDIUM:
		return 0.8f;

	case SKILL_HARD:
		return 1.0f;

	default:
		return 0.7f;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Damage value of the npc_rocket_turret
//-----------------------------------------------------------------------------
float CPortalMPGameRules::GetRocketTurretDamage( void )
{
	switch( GetSkillLevel() )
	{
	case SKILL_EASY:
		return 120.0f;

	case SKILL_MEDIUM:
		return 200.0f;

	case SKILL_HARD:
		return 200.0f;

	default:
		return 100.0f;
	}
}

float CPortalMPGameRules::FlPlayerFallDamage( CBasePlayer *pPlayer )
{
	// No fall damage in Portal!
	return 0.0f;
}

// Stealing a chunk of HL2 Gamerules for portal2 coop
// This is just to make turrets shoot at the players under multiplayer to
// facilitate an experiment... We might need our own separate gamerules for coop depending
// on what the game ends up being like.
void CPortalMPGameRules::InitDefaultAIRelationships()
{
	int i,j;

	//  Allocate memory for default relationships
	CBaseCombatCharacter::AllocateDefaultRelationships();

	// --------------------------------------------------------------
	// First initialize table so we can report missing relationships
	// --------------------------------------------------------------
	int iNumClasses = GameRules() ? GameRules()->NumEntityClasses() : LAST_SHARED_ENTITY_CLASS;
	for (i=0;i<iNumClasses;i++)
	{
		for (j=0;j<iNumClasses;j++)
		{
			// By default all relationships are neutral of priority zero
			CBaseCombatCharacter::SetDefaultRelationship( (Class_T)i, (Class_T)j, D_NU, 0 );
		}
	}

	// ------------------------------------------------------------
	//	> CLASS_COMBINE
	// ------------------------------------------------------------
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_COMBINE,			CLASS_NONE,				D_NU, 0);			
	CBaseCombatCharacter::SetDefaultRelationship(CLASS_COMBINE,			CLASS_PLAYER,			D_HT, 0);			

}

void CPortalMPGameRules::SetMapCompleteData( int nPlayer )
{
	if ( m_bDataReceived[ nPlayer ] )
	{
		// Already got data for this player! Don't ask for it again
		return;
	}

	// Get the player
	CPortal_Player *pPlayer = NULL;

	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CPortal_Player *pPlayerTemp = ToPortalPlayer( UTIL_PlayerByIndex( i ) );

		//If the other player does not exist or if the other player is the local player
		if ( pPlayerTemp == NULL )
			continue;

		if ( pPlayerTemp->GetTeamNumber() == ( nPlayer == 0 ? TEAM_BLUE : TEAM_RED ) )
		{
			pPlayer = pPlayerTemp;
			break;
		}
	}

	if ( !pPlayer )
		return;

	// Request key values for all the levels
	KeyValues *kvClientRequest = new KeyValues( "read_stats" );

	for ( int nBranch = 0; nBranch < MAX_PORTAL2_COOP_BRANCHES; ++nBranch )
	{
		for ( int nLevel = 0; nLevel < MAX_PORTAL2_COOP_LEVELS_PER_BRANCH; ++nLevel )
		{
			if ( m_szLevelNames[ nBranch ][ nLevel ][ 0 ] == '\0' )
				continue;

			kvClientRequest->SetInt( CFmtStr( "MP.complete.%s", m_szLevelNames[ nBranch ][ nLevel ] ), 0 );
		}
	}

	engine->ClientCommandKeyValues( pPlayer->edict(), kvClientRequest );
}

void CPortalMPGameRules::StartPlayerTransitionThinks( void )
{
	// Turn off video for all players once they've all connected
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pToPlayer = UTIL_PlayerByIndex( i );
		if ( pToPlayer )
		{
			// Respawn other players who were waiting
			pToPlayer->SetThink( &CPortal_Player::PlayerTransitionCompleteThink );
			pToPlayer->SetNextThink( gpGlobals->curtime + 1.0f );

			if ( !pToPlayer->HasAttachedSplitScreenPlayers() && !pToPlayer->IsSplitScreenPlayer() )
			{
				CBasePlayer *pOtherPlayer = UTIL_OtherPlayer( pToPlayer );
				if ( pOtherPlayer )
				{
					pOtherPlayer->AddPictureInPicturePlayer( pToPlayer );
					pToPlayer->AddPictureInPicturePlayer( pOtherPlayer );
				}
			}
			else if ( IsPC() && ( pToPlayer->HasAttachedSplitScreenPlayers() || pToPlayer->IsSplitScreenPlayer() ) )
			{
				SetAllMapsComplete();
			}
		}
	}

	ResetAllPlayersStats();
	g_portal_ui_controller.OnLevelStart();
}


//-----------------------------------------------------------------------------
// Purpose: Player has just left the game
//-----------------------------------------------------------------------------
void CPortalMPGameRules::ClientDisconnected( edict_t *pClient )
{
	// Msg( "CLIENT DISCONNECTED, REMOVING FROM TEAM.\n" );
	CPortal_Player::ClientDisconnected( pClient );

	CBasePlayer *pPlayer = (CBasePlayer *)CBaseEntity::Instance( pClient );
	if ( pPlayer )
	{
		// Remove the player from his team
		if ( pPlayer->GetTeam() )
		{
			pPlayer->GetTeam()->RemovePlayer( pPlayer );
		}
	}

	bool bIsSSCredits = (IsLocalSplitScreen() && IsCreditsMap());

	if ( ( m_bDataReceived[ 0 ] && m_bDataReceived[ 1 ] && mp_dev_wait_for_other_player.GetBool() && !bIsSSCredits ) || IsCommunityCoop() )
	{
		for ( int i = 1; i <= MAX_PLAYERS; i++ )
		{
			CPortal_Player *pOtherPlayer = static_cast<CPortal_Player *>(UTIL_PlayerByIndex( i ));
			if ( !pOtherPlayer )
				continue;

			if ( pOtherPlayer == pPlayer )
				continue;

			DevMsg( "Client disconnected and we're left with no partner!\n" );
			engine->ClientCommand( pOtherPlayer->edict(), "disconnect \"Partner disconnected\"" );
		}
	}

	BaseClass::ClientDisconnected( pClient );
}


bool FindInList( const char **pStrings, const char *pToFind );

void CPortalMPGameRules::RestartGame()
{
	// bounds check
	if ( mp_timelimit.GetInt() < 0 )
	{
		mp_timelimit.SetValue( 0 );
	}
	m_flGameStartTime = gpGlobals->curtime;
	if ( !IsFinite( m_flGameStartTime.Get() ) )
	{
		Warning( "Trying to set a NaN game start time\n" );
		m_flGameStartTime.GetForModify() = 0.0f;
	}

	CleanUpMap();

	// now respawn all players
	for (int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );

		if ( !pPlayer )
			continue;

		if ( pPlayer->GetActiveWeapon() )
		{
			pPlayer->GetActiveWeapon()->Holster();
		}
		pPlayer->RemoveAllItems( true );
		respawn( pPlayer, false );
//#pragma message( __FILE__ "(" __LINE__AS_STRING ") : warning custom: Disabled player reset" )
#if 0
		pPlayer->Reset();
#endif
	}

	// Respawn entities (glass, doors, etc..)

	CTeam *pBlue = GetGlobalTeam( TEAM_BLUE );
	CTeam *pRed = GetGlobalTeam( TEAM_RED );

	if ( pBlue )
	{
		pBlue->SetScore( 0 );
	}

	if ( pRed )
	{
		pRed->SetScore( 0 );
	}

	m_nNumPortalsPlaced = 0;

	m_flIntermissionEndTime = 0;
	m_flRestartGameTime = 0.0;		
	m_bCompleteReset = false;

	IGameEvent * event = gameeventmanager->CreateEvent( "round_start" );
	if ( event )
	{
		event->SetInt("fraglimit", 0 );
		event->SetInt( "priority", 6 ); // HLTV event priority, not transmitted

		event->SetString("objective","DEATHMATCH");

		gameeventmanager->FireEvent( event );
	}
}

// Utility function
bool FindInList( const char **pStrings, const char *pToFind )
{
	int i = 0;
	while ( pStrings[i][0] != 0 )
	{
		if ( Q_stricmp( pStrings[i], pToFind ) == 0 )
			return true;
		i++;
	}

	return false;
}

void CPortalMPGameRules::CleanUpMap()
{
	// Recreate all the map entities from the map data (preserving their indices),
	// then remove everything else except the players.

	// Get rid of all entities except players.
	CBaseEntity *pCur = gEntList.FirstEnt();
	while ( pCur )
	{
		CBaseCombatWeapon *pWeapon = dynamic_cast< CBaseCombatWeapon* >( pCur );
		// Weapons with owners don't want to be removed..
		if ( pWeapon )
		{
			if ( !pWeapon->GetOwner() || !pWeapon->GetOwner()->IsPlayer() )
			{
				UTIL_Remove( pCur );
			}
		}
		// remove entities that has to be restored on roundrestart (breakables etc)
		else if ( !FindInList( s_PreserveEnts, pCur->GetClassname() ) )
		{
			UTIL_Remove( pCur );
		}

		pCur = gEntList.NextEnt( pCur );
	}

	// Really remove the entities so we can have access to their slots below.
	gEntList.CleanupDeleteList();

	// Cancel all queued events, in case a func_bomb_target fired some delayed outputs that
	// could kill respawning CTs
	g_EventQueue.Clear();

//#pragma message( __FILE__ "(" __LINE__AS_STRING ") : warning custom: Disabled entity parsing" )
#if 0
	// Now reload the map entities.
	class CHL2MPMapEntityFilter : public IMapEntityFilter
	{
	public:
		virtual bool ShouldCreateEntity( const char *pClassname )
		{
			// Don't recreate the preserved entities.
			if ( !FindInList( s_PreserveEnts, pClassname ) )
			{
				return true;
			}
			else
			{
				// Increment our iterator since it's not going to call CreateNextEntity for this ent.
				if ( m_iIterator != g_MapEntityRefs.InvalidIndex() )
					m_iIterator = g_MapEntityRefs.Next( m_iIterator );

				return false;
			}
		}


		virtual CBaseEntity* CreateNextEntity( const char *pClassname )
		{
			if ( m_iIterator == g_MapEntityRefs.InvalidIndex() )
			{
				// This shouldn't be possible. When we loaded the map, it should have used 
				// CCSMapLoadEntityFilter, which should have built the g_MapEntityRefs list
				// with the same list of entities we're referring to here.
				Assert( false );
				return NULL;
			}
			else
			{
				CMapEntityRef &ref = g_MapEntityRefs[m_iIterator];
				m_iIterator = g_MapEntityRefs.Next( m_iIterator );	// Seek to the next entity.

				if ( ref.m_iEdict == -1 || INDEXENT( ref.m_iEdict ) )
				{
					// Doh! The entity was delete and its slot was reused.
					// Just use any old edict slot. This case sucks because we lose the baseline.
					return CreateEntityByName( pClassname );
				}
				else
				{
					// Cool, the slot where this entity was is free again (most likely, the entity was 
					// freed above). Now create an entity with this specific index.
					return CreateEntityByName( pClassname, ref.m_iEdict );
				}
			}
		}

	public:
		int m_iIterator; // Iterator into g_MapEntityRefs.
	};

	CHL2MPMapEntityFilter filter;
	filter.m_iIterator = g_MapEntityRefs.Head();

	// DO NOT CALL SPAWN ON info_node ENTITIES!

	MapEntity_ParseAllEntities( engine->GetMapEntitiesString(), &filter, true );
#endif
}

void CPortalMPGameRules::CheckRestartGame( void )
{
	// Restart the game if specified by the server
	int iRestartDelay = mp_restartgame.GetInt();

	if ( iRestartDelay > 0 )
	{
		if ( iRestartDelay > 60 )
			iRestartDelay = 60;


		// let the players know
		char strRestartDelay[64];
		Q_snprintf( strRestartDelay, sizeof( strRestartDelay ), "%d", iRestartDelay );
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "Game will restart in %s1 %s2", strRestartDelay, iRestartDelay == 1 ? "SECOND" : "SECONDS" );
		UTIL_ClientPrintAll( HUD_PRINTCONSOLE, "Game will restart in %s1 %s2", strRestartDelay, iRestartDelay == 1 ? "SECOND" : "SECONDS" );

		m_flRestartGameTime = gpGlobals->curtime + iRestartDelay;
		m_bCompleteReset = true;
		mp_restartgame.SetValue( 0 );
	}

//#pragma message( __FILE__ "(" __LINE__AS_STRING ") : warning custom: Disabled ready restart" )
#if 0
	if( mp_readyrestart.GetBool() )
	{
		m_bAwaitingReadyRestart = true;
		m_bHeardAllPlayersReady = false;


		const char *pszReadyString = mp_ready_signal.GetString();


		// Don't let them put anything malicious in there
		if( pszReadyString == NULL || Q_strlen(pszReadyString) > 16 )
		{
			pszReadyString = "ready";
		}

		IGameEvent *event = gameeventmanager->CreateEvent( "hl2mp_ready_restart" );
		if ( event )
			gameeventmanager->FireEvent( event );

		mp_readyrestart.SetValue( 0 );

		// cancel any restart round in progress
		m_flRestartGameTime = -1;
	}
#endif
}

void CPortalMPGameRules::CheckAllPlayersReady( void )
{
//#pragma message( __FILE__ "(" __LINE__AS_STRING ") : warning custom: Disabled ready restart" )
#if 0
	for (int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHL2MP_Player *pPlayer = (CHL2MP_Player*) UTIL_PlayerByIndex( i );

		if ( !pPlayer )
			continue;
		if ( !pPlayer->IsReady() )
			return;
	}
#endif
	m_bHeardAllPlayersReady = true;
}

void CPortalMPGameRules::AddBranchLevel( int nBranch, const char *pchName )
{
	if ( V_strcmp( pchName, "CLEAR ALL" ) == 0 )
	{
		for ( int i = 0; i < MAX_PORTAL2_COOP_BRANCHES; ++i )
		{
			m_nLevelCount.Set( i, 0 );
		}

		m_bMapNamesLoaded = false;

		return;
	}

	if ( nBranch < 0 || nBranch >= MAX_PORTAL2_COOP_BRANCHES )
		return;

	if ( m_nLevelCount[ nBranch ] >= MAX_PORTAL2_COOP_LEVELS_PER_BRANCH )
		return;

	V_strcpy( m_szLevelNames[ nBranch ][ m_nLevelCount[ nBranch ] ], pchName );
	m_nLevelCount.Set( nBranch, m_nLevelCount[ nBranch ] + 1 );
	NetworkStateChanged();

	m_bMapNamesLoaded = true;
}

void CPortalMPGameRules::SaveMPStats( void )
{
	// Mark it in storage
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CPortal_Player *pPlayerTemp = ToPortalPlayer( UTIL_PlayerByIndex( i ) );

		if ( pPlayerTemp == NULL )
			continue;

		if ( GetPortalMPStats() )
		{
			GetPortalMPStats()->SaveStats( pPlayerTemp );
		}
	}
}

void CPortalMPGameRules::AddCreditsName( const char *pchName )
{
	if ( V_strcmp( pchName, "CLEAR ALL" ) == 0 )
	{
		m_bCoopCreditsLoaded = false;
		m_nCoopCreditsIndex = -1;
		return;
	}

	m_szCoopCreditsNames.AddToTail( CUtlString( pchName ) );
	NetworkStateChanged();

	m_bCoopCreditsLoaded = true;
}

void CPortalMPGameRules::SetAllMapsComplete( bool bComplete /*= true*/, int nPlayer /*= -1*/ )
{
	for ( int nBranch = 0; nBranch < MAX_PORTAL2_COOP_BRANCHES; ++nBranch )
	{
		SetBranchComplete( nBranch, bComplete );
	}

	// Also mark first level in storage
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CPortal_Player *pPlayerTemp = ToPortalPlayer( UTIL_PlayerByIndex( i ) );

		if ( pPlayerTemp == NULL )
			continue;

		if ( nPlayer != -1 && ( nPlayer == 0 && pPlayerTemp->GetTeamNumber() != TEAM_BLUE || 
								nPlayer == 1 && pPlayerTemp->GetTeamNumber() != TEAM_RED ) )
		{
			continue;
		}

		// We have to build these key values for each player because they are destroyed in ClientCommandKeyValues
		KeyValues *kvClientRequest = new KeyValues( "write_stats" );
		kvClientRequest->SetInt( "MP.complete.mp_coop_start", bComplete ? 1 : 0 );

		engine->ClientCommandKeyValues( pPlayerTemp->edict(), kvClientRequest );
	}
}

void CPortalMPGameRules::SetBranchComplete( int nBranch, bool bComplete /*= true*/ )
{
	if ( nBranch < 0 || nBranch >= MAX_PORTAL2_COOP_BRANCHES )
		return;

	for ( int nLevel = 0; nLevel < m_nLevelCount[ nBranch ]; ++nLevel )
	{
		if ( ( bComplete && ( !m_bLevelCompletions[ 0 ][ nBranch ][ nLevel ] || !m_bLevelCompletions[ 1 ][ nBranch ][ nLevel ] ) ) || 
			( !bComplete && ( m_bLevelCompletions[ 0 ][ nBranch ][ nLevel ] || m_bLevelCompletions[ 1 ][ nBranch ][ nLevel ] ) ) )
		{
			// One of the players hasn't completed it
			CReliableBroadcastRecipientFilter player;
			player.AddAllPlayers();

			UserMessageBegin( player, bComplete ? "MPMapCompleted" : "MPMapIncomplete" );
				WRITE_CHAR( nBranch );
				WRITE_CHAR( nLevel );
			MessageEnd();

			m_bLevelCompletions[ 0 ][ nBranch ][ nLevel ] = bComplete;
			m_bLevelCompletions[ 1 ][ nBranch ][ nLevel ] = bComplete;
		}
	}

	// TODO unlock taunts per branch here
	switch ( nBranch )
	{
	case 0:
		break;
	case 1:
		break;
	case 2:
		break;
	case 3:
		break;
	case 4:
		break;
	}

	// Mark it in storage
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CPortal_Player *pPlayerTemp = ToPortalPlayer( UTIL_PlayerByIndex( i ) );

		if ( pPlayerTemp == NULL )
			continue;

		// We have to build these key values for each player because they are destroyed in ClientCommandKeyValues
		KeyValues *kvClientRequest = new KeyValues( "write_stats" );
		for ( int nLevel = 0; nLevel < m_nLevelCount[ nBranch ]; ++nLevel )
		{
			kvClientRequest->SetInt( CFmtStr( "MP.complete.%s", m_szLevelNames[ nBranch ][ nLevel ] ), bComplete ? 1 : 0 );
		}

		engine->ClientCommandKeyValues( pPlayerTemp->edict(), kvClientRequest );
	}
}

void CPortalMPGameRules::SetMapComplete( const char *pchName, bool bComplete /*= true*/ )
{
	// Mark it in storage
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CPortal_Player *pPlayerTemp = ToPortalPlayer( UTIL_PlayerByIndex( i ) );

		if ( pPlayerTemp == NULL )
			continue;

		// tell the gamestats to send off our per map stats data 	
		if ( GetPortalMPStats() )
		{
			GetPortalMPStats()->SavePerMapStats( pPlayerTemp, pchName );
#if !defined ( CLIENT_DLL ) && !defined( _GAMECONSOLE ) && !defined( NO_STEAM )
			GetPortalMPStats()->SubmitOGSEndOfMapStatsForPlayer( pPlayerTemp, pchName );
#endif
		}

		// We have to build these key values for each player because they are destroyed in ClientCommandKeyValues
		KeyValues *kvClientRequest = new KeyValues( "write_stats" );
		kvClientRequest->SetInt( CFmtStr( "MP.complete.%s", pchName ), bComplete ? 1 : 0 );

		engine->ClientCommandKeyValues( pPlayerTemp->edict(), kvClientRequest );
	}

	GetPortalMPStats()->IncrementMapsCompleted();

	// Mark it in memory
	for ( int nBranch = 0; nBranch < MAX_PORTAL2_COOP_BRANCHES; ++nBranch )
	{
		for ( int nLevel = 0; nLevel < m_nLevelCount[ nBranch ]; ++nLevel )
		{
			if ( V_strcmp( m_szLevelNames[ nBranch ][ nLevel ], pchName ) == 0 )
			{
				// One of the players hasn't completed it
				if ( ( bComplete && ( !m_bLevelCompletions[ 0 ][ nBranch ][ nLevel ] || !m_bLevelCompletions[ 1 ][ nBranch ][ nLevel ] ) ) || 
					 ( !bComplete && ( m_bLevelCompletions[ 0 ][ nBranch ][ nLevel ] || m_bLevelCompletions[ 1 ][ nBranch ][ nLevel ] ) ) )
				{
					CReliableBroadcastRecipientFilter player;
					player.AddAllPlayers();

					UserMessageBegin( player, bComplete ? "MPMapCompleted" : "MPMapIncomplete" );
						WRITE_CHAR( nBranch );
						WRITE_CHAR( nLevel );
					MessageEnd();

					m_bLevelCompletions[ 0 ][ nBranch ][ nLevel ] = bComplete;
					m_bLevelCompletions[ 1 ][ nBranch ][ nLevel ] = bComplete;
				}

				// Increment our number of completed levels so far in the current run of the branch.
				GlobalEntity_AddToCounter( MAKE_STRING( "levels_completed_this_branch" ), 1 );

				return;
			}
		}
	}
}

void CPortalMPGameRules::SetMapCompleteSimple( int nPlayer, const char *pchName, bool bComplete )
{
	for ( int nBranch = 0; nBranch < MAX_PORTAL2_COOP_BRANCHES; ++nBranch )
	{
		for ( int nLevel = 0; nLevel < m_nLevelCount[ nBranch ]; ++nLevel )
		{
			if ( V_strcmp( m_szLevelNames[ nBranch ][ nLevel ], pchName ) == 0 )
			{
				m_bLevelCompletions[ nPlayer ][ nBranch ][ nLevel ] = bComplete;
				return;
			}
		}
	}
}

void CPortalMPGameRules::SendAllMapCompleteData( void )
{
	CReliableBroadcastRecipientFilter player;
	player.AddAllPlayers();

	const int nNumBits = 2 * MAX_PORTAL2_COOP_BRANCHES * MAX_PORTAL2_COOP_LEVELS_PER_BRANCH;

	byte buff[ sizeof( byte ) * 8 + nNumBits / ( sizeof( byte ) * 8 ) ];
	memset( buff, 0, sizeof(buff) );

	byte *pCurrent = buff;
	int nMask = 0x01;

	for ( int nPlayer = 0; nPlayer < 2; ++nPlayer )
	{
		for ( int nBranch = 0; nBranch < MAX_PORTAL2_COOP_BRANCHES; ++nBranch )
		{
			for ( int nLevel = 0; nLevel < MAX_PORTAL2_COOP_LEVELS_PER_BRANCH; ++nLevel )
			{
				if ( m_bLevelCompletions[ nPlayer ][ nBranch ][ nLevel ] )
				{
					*pCurrent |= nMask;
				}

				nMask <<= 1;

				if ( nMask >= 0x0100 )
				{
					pCurrent++;
					nMask = 0x01;
				}
			}
		}
	}

	UserMessageBegin( player, "MPMapCompletedData" );
		WRITE_BITS( buff, nNumBits );
	MessageEnd();
}

bool CPortalMPGameRules::SupressSpawnPortalgun( int nTeam )
{
	// Using globals for this is risky because it would carry across levels
	// Using level names isn't possible because the player might die after picking up a portal gun partway through
	// So level designers just place an info target with this name and delete it once the player gets a gun
	if ( nTeam == TEAM_BLUE )
	{
		return gEntList.FindEntityByName( NULL, "supress_blue_portalgun_spawn" ) != NULL;
	}
	else if ( nTeam == TEAM_RED )
	{
		return gEntList.FindEntityByName( NULL, "supress_orange_portalgun_spawn" ) != NULL;
	}

	return false;
}


CEG_NOINLINE void CPortalMPGameRules::PlayerWinRPS( CBasePlayer* pWinnerPlayer )
{
	bool bIsBlueTeam = ( pWinnerPlayer->GetTeamNumber() == TEAM_BLUE );
	int nWinnerSlot = bIsBlueTeam ? 0 : 1;
	int nLoserSlot = bIsBlueTeam ? 1 : 0;

#if defined CLIENT_DLL
	CEG_PROTECT_MEMBER_FUNCTION( CPortalMPGameRules_PlayerWinRPS );
#endif

	++m_nRPSWinCount[ nWinnerSlot ];
	m_nRPSWinCount[ nLoserSlot ] = 0;

	if ( m_nRPSWinCount[ nWinnerSlot ] == 3 )
	{
		UTIL_RecordAchievementEvent( "ACH.ROCK_CRUSHES_ROBOT", pWinnerPlayer );
	}
}

#endif // #ifndef CLIENT_DLL


int CPortalMPGameRules::GetActiveBranches( void )
{
	bool bEverythingComplete = true;

	int nActiveBranches = 1;	// The first one is always active

	// HACK HACK: We don't care about anything after course 5! Completing maps in the dlc shouldn't affect how the rest unlock!!!
	const int nMaxBranchesWeCareAbout = 5; //MAX_PORTAL2_COOP_BRANCHES;

	for ( int nBranch = 0; nBranch < nMaxBranchesWeCareAbout; ++nBranch )
	{
		if ( m_nLevelCount[ nBranch ] > 1 )	// Don't include branches that only have 1 map (credits)... they don't count
		{
			bool bAnyComplete = false;
			bool bAllComplete = true;

			for ( int nLevel = 0; nLevel < m_nLevelCount[ nBranch ]; ++nLevel )
			{
				if ( !m_bLevelCompletions[ 0 ][ nBranch ][ nLevel ] && !m_bLevelCompletions[ 1 ][ nBranch ][ nLevel ] )
				{
					bAllComplete = false;
					bEverythingComplete = false;
				}

				if ( m_bLevelCompletions[ 0 ][ nBranch ][ nLevel ] || m_bLevelCompletions[ 1 ][ nBranch ][ nLevel ] )
				{
					bAnyComplete = true;
				}

				if ( !bAllComplete && bAnyComplete )
				{
					// We're not going to learn anything else by checking the remaining levels in this branch
					break;
				}
			}

			if ( bAllComplete )
			{
				// This one and the next one are active
				nActiveBranches = nBranch + 2;
			}
			else if ( bAnyComplete )
			{
				// This one is active
				nActiveBranches = nBranch + 1;
			}
		}
	}

	if ( bEverythingComplete )
	{
		nActiveBranches++;
	}

	//DevMsg( "\n============\nNUM ACTIVE BRANCHES %i\n============\n", nActiveBranches );

	return nActiveBranches;
}

int CPortalMPGameRules::GetSelectedDLCCourse( void )
{
	return m_nSelectedDLCCourse;
}

bool CPortalMPGameRules::IsAnyLevelComplete( void )
{
	for ( int nBranch = 0; nBranch < MAX_PORTAL2_COOP_BRANCHES; ++nBranch )
	{
		for ( int nLevel = 0; nLevel < MAX_PORTAL2_COOP_LEVELS_PER_BRANCH; ++nLevel )
		{
			if ( m_szLevelNames[ nBranch ][ nLevel ][ 0 ] == '\0' )
				continue;

			if ( m_bLevelCompletions[ 0 ][ nBranch ][ nLevel ] || m_bLevelCompletions[ 1 ][ nBranch ][ nLevel ] )
			{
				// If any level is completed, return true
				return true;
			}
		}
	}

	return false;
}

bool CPortalMPGameRules::IsFullBranchComplete( int nBranch )
{
	bool bAllComplete = true;

	for ( int nLevel = 0; nLevel < m_nLevelCount[ nBranch ]; ++nLevel )
	{
		if ( !m_bLevelCompletions[ 0 ][ nBranch ][ nLevel ] && !m_bLevelCompletions[ 1 ][ nBranch ][ nLevel ] )
		{
			bAllComplete = false;
			break;
		}
	}

	return bAllComplete;
}

bool CPortalMPGameRules::IsPlayerFullBranchComplete( int nPlayer, int nBranch )
{
	bool bAllComplete = true;

	for ( int nLevel = 0; nLevel < m_nLevelCount[ nBranch ]; ++nLevel )
	{
		if ( !m_bLevelCompletions[ nPlayer ][ nBranch ][ nLevel ] )
		{
			bAllComplete = false;
			break;
		}
	}

	return bAllComplete;
}

#ifndef CLIENT_DLL
int CPortalMPGameRules::GetLevelsCompletedThisBranch( void )
{
	return GlobalEntity_GetCounter( MAKE_STRING( "levels_completed_this_branch" ) );
}
#endif

bool CPortalMPGameRules::IsLevelInBranchComplete( int nBranch, int nLevel )
{ 
	if ( nLevel < 0 || nLevel >= MAX_PORTAL2_COOP_LEVELS_PER_BRANCH ||
		 nBranch < 0 || nBranch >= MAX_PORTAL2_COOP_BRANCHES )
		return true;

	return m_bLevelCompletions[ 0 ][ nBranch ][ nLevel ] || m_bLevelCompletions[ 1 ][ nBranch ][ nLevel ]; 
}

void CPortalMPGameRules::SetMapComplete( int nPlayer, int nBranch, int nLevel, bool bComplete /*= true*/ )
{
	m_bLevelCompletions[ nPlayer ][ nBranch ][ nLevel ] = bComplete;
}

bool CPortalMPGameRules::IsLobbyMap( void )
{
#ifdef CLIENT_DLL
	return StringHasPrefix( engine->GetLevelNameShort(), "mp_coop_lobby" );
#else
	return StringHasPrefix( gpGlobals->mapname.ToCStr(), "mp_coop_lobby" );
#endif
}

bool CPortalMPGameRules::IsStartMap( void )
{
#ifdef CLIENT_DLL
	return V_strcmp( engine->GetLevelNameShort(), "mp_coop_start" ) == 0;
#else
	return V_strcmp( gpGlobals->mapname.ToCStr(), "mp_coop_start" ) == 0;
#endif
}

bool CPortalMPGameRules::IsCreditsMap( void )
{
#ifdef CLIENT_DLL
	return V_strcmp( engine->GetLevelNameShort(), "mp_coop_credits" ) == 0;
#else
	return V_strcmp( gpGlobals->mapname.ToCStr(), "mp_coop_credits" ) == 0;
#endif
}

bool CPortalMPGameRules::IsCommunityCoopHub( void )
{
#ifdef CLIENT_DLL
	return V_strcmp( engine->GetLevelNameShort(), "mp_coop_community_hub" ) == 0;
#else
	return V_strcmp( gpGlobals->mapname.ToCStr(), "mp_coop_community_hub" ) == 0;
#endif
}

bool CPortalMPGameRules::IsCommunityCoop( void )
{
	return cm_is_current_community_map_coop.GetBool();
}

#ifdef CLIENT_DLL

static void __MsgFunc_MPMapCompleted( bf_read &msg )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return;

	int nBranch = msg.ReadChar();
	int nLevel = msg.ReadChar();

	// Both players
	pRules->SetMapComplete( 0, nBranch, nLevel );
	pRules->SetMapComplete( 1, nBranch, nLevel );
}
USER_MESSAGE_REGISTER( MPMapCompleted );

static void __MsgFunc_MPMapIncomplete( bf_read &msg )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return;

	int nBranch = msg.ReadChar();
	int nLevel = msg.ReadChar();

	// Both players
	pRules->SetMapComplete( 0, nBranch, nLevel, false );
	pRules->SetMapComplete( 1, nBranch, nLevel, false );
}
USER_MESSAGE_REGISTER( MPMapIncomplete );

static void __MsgFunc_MPMapCompletedData( bf_read &msg )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return;

	const int nNumBits = 2 * MAX_PORTAL2_COOP_BRANCHES * MAX_PORTAL2_COOP_LEVELS_PER_BRANCH;

	byte buff[ sizeof( byte ) * 8 + nNumBits / ( sizeof( byte ) * 8 ) ];
	memset( buff, 0, sizeof(buff) );

	msg.ReadBits( buff, nNumBits );

	byte *pCurrent = buff;
	int nMask = 0x01;

	for ( int nPlayer = 0; nPlayer < 2; ++nPlayer )
	{
		for ( int nBranch = 0; nBranch < MAX_PORTAL2_COOP_BRANCHES; ++nBranch )
		{
			for ( int nLevel = 0; nLevel < MAX_PORTAL2_COOP_LEVELS_PER_BRANCH; ++nLevel )
			{
				if ( ( *pCurrent & nMask ) != 0 )
				{
					pRules->SetMapComplete( nPlayer, nBranch, nLevel );
				}

				nMask <<= 1;

				if ( nMask >= 0x0100 )
				{
					pCurrent++;
					nMask = 0x01;
				}
			}
		}
	}
}
USER_MESSAGE_REGISTER( MPMapCompletedData );


void CPortalMPGameRules::LoadMapCompleteData( void )
{
	if ( !m_bMapNamesLoaded )
		return;

	CPortal_Player *pLocalPlayer = CPortal_Player::GetLocalPlayer();

	int nPlayer = 0;
	if ( pLocalPlayer )
	{
		nPlayer = pLocalPlayer->GetTeamNumber() == TEAM_BLUE ? 0 : 1;
	}

	char szCommand[ 32 ];
	V_snprintf( szCommand, sizeof( szCommand ), "level_complete_data %i", nPlayer );

	IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetActiveUserId() );
	if ( !pPlayer )
	{
		// We don't support multiple logins on this platform! Just use the primary player
		pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetUserId( 0 ) );

		if ( !pPlayer )
		{
			// Let the server know that we at least attempted to load completion data on the client for this player
			engine->ClientCmd( szCommand );
			return;
		}
	}

	TitleDataFieldsDescription_t const *fields = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();

	// Process completed maps:
	{
		for ( int nBranch = 0; nBranch < MAX_PORTAL2_COOP_BRANCHES; ++nBranch )
		{
			for ( int nLevel = 0; nLevel < MAX_PORTAL2_COOP_LEVELS_PER_BRANCH; ++nLevel )
			{
				if ( m_szLevelNames[ nBranch ][ nLevel ][ 0 ] == '\0' )
					continue;

				CFmtStr tdKey( "MP.complete.%s", m_szLevelNames[ nBranch ][ nLevel ] );
				TitleDataFieldsDescription_t const *fdKey = TitleDataFieldsDescriptionFindByString( fields, tdKey );
				int nComplete = 0;
				if ( fdKey )
				{
					nComplete = TitleDataFieldsDescriptionGetBit( fdKey, pPlayer ) ? 1 : 0;
				}
				else
				{
					Warning( "CPortalMPGameRules::LoadMapCompleteData failed to load %s\n", tdKey.Access() );
				}

				m_bLevelCompletions[ nPlayer ][ nBranch ][ nLevel ] = ( nComplete != 0 );
			}
		}
	}

	GetClientMenuManagerTaunt().KeyValueProcessor( pPlayer );

	if ( GetPortalMPStats() )
	{
		GetPortalMPStats()->RefreshStats( pPlayer, pLocalPlayer );
	}

	// Let the server know that we loaded completion data on the client for this player
	engine->ClientCmd( szCommand );
}

static void __MsgFunc_MPTauntEarned( bf_read &msg )
{
	char szTaunt[ 32 ];
	msg.ReadString( szTaunt, sizeof( szTaunt ) );
	bool bAwardSilently = !!msg.ReadByte();
	
	GetClientMenuManagerTaunt().SetTauntOwned( szTaunt, bAwardSilently );

	//if ( bAwardSilently )
	//	DevMsg( "Awarding %s, but doing it silently.\n", szTaunt );

	// Send event for everything but small wave
	if ( V_strcmp( szTaunt, "smallWave" ) != 0 )
	{
		IGameEvent *event = gameeventmanager->CreateEvent( "gesture_earned" );
		if ( event )
		{
			event->SetInt( "userid", C_BasePlayer::GetLocalPlayer()->GetUserID() );
			event->SetBool( "teamtaunt", GetClientMenuManagerTaunt().IsTauntTeam( szTaunt ) );

			gameeventmanager->FireEventClientSide( event );
		}
	}
}
USER_MESSAGE_REGISTER( MPTauntEarned );

static void __MsgFunc_MPTauntLocked( bf_read &msg )
{
	char szTaunt[ 32 ];
	msg.ReadString( szTaunt, sizeof( szTaunt ) );

	GetClientMenuManagerTaunt().SetTauntLocked( szTaunt );
}
USER_MESSAGE_REGISTER( MPTauntLocked );

static void __MsgFunc_MPAllTauntsLocked( bf_read& /*msg*/ )
{
	GetClientMenuManagerTaunt().SetAllTauntsLocked();
}
USER_MESSAGE_REGISTER( MPAllTauntsLocked );

#endif // #ifdef CLIENT_DLL

///////////////////////////////////////////////////////////////////////
// Portal multiplayer specific global vscript functions
///////////////////////////////////////////////////////////////////////

void CC_DumpCompletionData( const CCommand &args )
{
	if ( !PortalMPGameRules() )
		return;

#if !defined ( CLIENT_DLL )
	DevMsg( "Dump Server Completion Data\n");
#else
	DevMsg( "Dump Client Completion Data\n");
#endif

	DevMsg( "B\tL\tP1\tP2\n");

	for ( int nBranch = 0; nBranch < MAX_PORTAL2_COOP_BRANCHES; ++nBranch )
	{
		DevMsg( "------------\n");
		for ( int nLevel = 0; nLevel < MAX_PORTAL2_COOP_LEVELS_PER_BRANCH; ++nLevel )
		{
			DevMsg( "%i\t%i\t%i\t%i\n", nBranch, nLevel, 
					( PortalMPGameRules()->IsPlayerLevelInBranchComplete( 0, nBranch, nLevel ) ? 1 : 0 ),
					( PortalMPGameRules()->IsPlayerLevelInBranchComplete( 1, nBranch, nLevel ) ? 1 : 0 ) );
		}
	}

	DevMsg( "------------\n\n");
}
ConCommand mp_dump_completion_data( 
#if !defined ( CLIENT_DLL )
								   "mp_dump_server_completion_data", 
#else
								   "mp_dump_client_completion_data", 
#endif
								   CC_DumpCompletionData, "Prints player completion data for all maps.", 0 );

#if !defined ( CLIENT_DLL )

void CC_EarnTaunt( const CCommand &args )
{
	bool bAwardSilently = false;
	const char *pNewTaunt = "new"; 
	if ( args.ArgC() >= 2 )
	{
		pNewTaunt = args[ 1 ];
	}
	if ( args.ArgC() >= 3 )
	{
		int nSilent = Q_atoi( args[ 2 ] );
		if ( nSilent >= 1 )
		{
			bAwardSilently = true;
			//DevMsg( "Awarding taunt %s silently ------------\n", pNewTaunt);
		}
	}

	CReliableBroadcastRecipientFilter player;
	player.AddAllPlayers();

	UserMessageBegin( player, "MPTauntEarned" );
		WRITE_STRING( pNewTaunt );
		WRITE_BOOL( bAwardSilently );
	MessageEnd();
}
ConCommand mp_earn_taunt( "mp_earn_taunt", CC_EarnTaunt, "Unlocks, owns, and puts a taunt in the gesture wheel.", 0 );

void CC_LockTaunt( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		return;
	}

	CReliableBroadcastRecipientFilter player;
	player.AddAllPlayers();

	UserMessageBegin( player, "MPTauntLocked" );
		WRITE_STRING( args[ 1 ] );
	MessageEnd();
}
ConCommand mp_lock_taunt( "mp_lock_taunt", CC_LockTaunt, "Locks a taunt and removes it from the gesture wheel.", 0 );

void CC_LockAllTaunts( const CCommand &args )
{
	CReliableBroadcastRecipientFilter player;
	player.AddAllPlayers();

	UserMessageBegin( player, "MPAllTauntsLocked" );
	MessageEnd();
}
ConCommand mp_lock_all_taunts( "mp_lock_all_taunts", CC_LockAllTaunts, "Locks all available taunts and removes them from the gesture wheel.", 0 );

/*
void CC_MP_Gib_All_Bots( void )
{
	if ( !GameRules()->IsMultiplayer() )
	{
		Warning( "This command only works in multiplayer coop." );
		return;
	}

	for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
	{
		CPortal_Player *pPlayer = ToPortalPlayer( UTIL_PlayerByIndex( i ) );

		if ( pPlayer && pPlayer->IsAlive() )
		{
			pPlayer->TakeDamage( CTakeDamageInfo( pPlayer, pPlayer, 1000, DMG_CRUSH ) );
		}
	}
}

// TODO: make this a cheat once the maps all use the script function call
static ConCommand mp_gib_all_bots("mp_gib_all_bots", CC_MP_Gib_All_Bots, "Kills all bots doing CRUSH damage which kills them.", 0 );
*/

static bool ScriptIsMultiplayer( void )
{
	return true; //g_pGameRules->IsMultiplayer();
}

float GetPlayerSilenceDuration( int nPlayer )
{
	return PlayerVoiceListener().GetPlayerSilenceDuration( nPlayer );
}

int GetTeamPlayerByIndex( int nTeamNum )
{
	CTeam *pTeam = GetGlobalTeam( nTeamNum );
	Assert( pTeam );
	if ( pTeam == NULL )
		return -1;

	for ( int i = 0; i < pTeam->GetNumPlayers(); i++ )
	{
		CBasePlayer *player = pTeam->GetPlayer( i );
		if ( player == NULL )
			continue;

		return player->entindex();
	}

	return -1;
}

int GetOrangePlayerIndex( void )
{
	return GetTeamPlayerByIndex( TEAM_RED );
}

int GetBluePlayerIndex( void )
{
	return GetTeamPlayerByIndex( TEAM_BLUE );
}

int GetCoopSectionIndex( void )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return 0;

	return pRules->GetCoopSection();
}

int GetCoopBranchLevelIndex( int nBranch )
{
	//int nBranch = 0;
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return 0;

	return pRules->GetCoopBranchLevel( nBranch - 1 );
}

int GetHighestActiveBranch( void )
{
	//int nBranch = 0;
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return 0;

	return pRules->GetActiveBranches();
}

void AddBranchLevelName( int nBranch, const char *pchName )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return;

	pRules->AddBranchLevel( nBranch - 1, pchName );
}

void SaveMPStatsData( void )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return;

	pRules->SaveMPStats();
}

void MarkMapComplete( const char *pchName )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return;

	pRules->SetMapComplete( pchName );
}

bool IsBranchComplete( int nBranch )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return 0;

	return pRules->IsFullBranchComplete( nBranch );
}

bool IsPlayerBranchComplete( int nPlayer, int nBranch )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return 0;

	return pRules->IsPlayerFullBranchComplete( nPlayer, nBranch );
}

int CoopGetLevelsCompletedThisBranch()
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return 0;

	return pRules->GetLevelsCompletedThisBranch();
}

int CoopGetBranchTotalLevelCount( int nBranch )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return 0;

	return pRules->GetBranchTotalLevelCount( nBranch );
}

bool IsLevelComplete( int nBranch, int nLevel )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return 0;

	return pRules->IsLevelInBranchComplete( nBranch, nLevel );
}

bool IsPlayerLevelComplete( int nPlayer, int nBranch, int nLevel )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return 0;

	return pRules->IsPlayerLevelInBranchComplete( nPlayer, nBranch, nLevel );
}

void AddCoopCreditsName( const char *pchName )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return;

	pRules->AddCreditsName( pchName );
}

int GetPlayerDeathCount( int nPlayer )
{
	return ( nPlayer == 1 ? GlobalEntity_GetCounter( MAKE_STRING( "player_orange_deaths" ) ) : GlobalEntity_GetCounter( MAKE_STRING( "player_blue_deaths" ) ) );
}

int GetGladosSpokenFlags( int nBatch )
{
	if ( nBatch < 0 || nBatch >= 4 )
	{
		DevWarning( "GetGLaDOSSpoteFlags out of range!\n" );
		return 0;
	}

	char szName[ 32 ];
	V_snprintf( szName, sizeof( szName ), "glados_spoken_flags%i", nBatch );
	return GlobalEntity_GetFlags( MAKE_STRING( szName ) );
}

void AddGladosSpokenFlags( int nBatch, int nFlags )
{
	if ( nBatch < 0 || nBatch >= 4 )
	{
		DevWarning( "AddGLaDOSSpoteFlags out of range!\n" );
		return;
	}

	char szName[ 32 ];
	V_snprintf( szName, sizeof( szName ), "glados_spoken_flags%i", nBatch );
	GlobalEntity_AddFlags( MAKE_STRING( szName ), nFlags );
}

bool IsLocalSplitScreen( void )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return 0;

	CPortal_Player *pPlayer = NULL;

	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		pPlayer = ToPortalPlayer( UTIL_PlayerByIndex( i ) );

		//If the other player does not exist or if the other player is the local player
		if ( pPlayer )
			return pPlayer->GetSplitScreenPlayers().Count() > 0;
	}

	return 0;
}

int GetNumPlayersConnected( void )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return 0;

	CPortal_Player *pPlayer = NULL;

	int nNum = 0;
	for( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		pPlayer = ToPortalPlayer( UTIL_PlayerByIndex( i ) );

		//count this player
		if ( pPlayer )
			nNum++;
	}

	return nNum;
}

void CoopGladosBlowUpBots( void )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
	{
		Warning( "This command only works in multiplayer coop." );
		return;
	}

	for ( int i = 1 ; i <= gpGlobals->maxClients ; i++ )
	{
		CPortal_Player *pPlayer = ToPortalPlayer( UTIL_PlayerByIndex( i ) );

		if ( pPlayer && pPlayer->IsAlive() )
		{
			pPlayer->TakeDamage( CTakeDamageInfo( pPlayer, pPlayer, 1000, DMG_CRUSH ) );
		}
	}

	pRules->SetGladosJustBlewUpBots();
}

int CoopGetNumPortalsPlaced( void )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return 0;

	return pRules->GetNumPortalsPlaced();
}

void CoopSetMapRunTime( float flRunLength )
{
	CPortalMPStats *pStats = GetPortalMPStats();
	if ( pStats )
	{
		pStats->SetTimeToCompleteMap( flRunLength );
	}
}

void NotifySpeedRunSuccess( int iRunLength, const char* mapname )
{
	KeyValues *kvNotifySpeedRunCoop = new KeyValues( "OnSpeedRunCoopEvent" );
	kvNotifySpeedRunCoop->SetString( "map", mapname );
	UTIL_SendClientCommandKVToPlayer( kvNotifySpeedRunCoop );
}

void CoopSetCameFromLastDLCMap( bool bComingFromLastDLCMap )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return;
	
	if( bComingFromLastDLCMap )
		GlobalEntity_SetFlags( "came_from_last_dlc_map", 1 );
	else
		GlobalEntity_SetFlags( "came_from_last_dlc_map", 0 );
}

bool GetCameFromLastDLCMap()
{

	if( (GlobalEntity_GetFlags( "came_from_last_dlc_map" ) & 1) != 0 )
		return true;

	return false;
}

void SetHaveSeenDLCTubesReveal( void )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return;
	
	GlobalEntity_SetFlags( "have_seen_dlc_tubes_reveal", 1 );
}

bool GetHaveSeenDLCTubesReveal()
{
	if( (GlobalEntity_GetFlags( "have_seen_dlc_tubes_reveal" ) & 1) != 0 )
		return true;

	 return false;
}

void CC_MarkAllMapsComplete( const CCommand &args )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return;

	pRules->SetAllMapsComplete();

	DevMsg( "All Maps Unlocked!\n" );
}
ConCommand mp_mark_all_maps_complete( "mp_mark_all_maps_complete", CC_MarkAllMapsComplete, "Marks all levels as complete in the save file.", 0 );

void CC_MarkAllMapsIncomplete( const CCommand &args )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return;

	pRules->SetAllMapsComplete( false );

	DevMsg( "All Maps Locked!\n" );
}
ConCommand mp_mark_all_maps_incomplete( "mp_mark_all_maps_incomplete", CC_MarkAllMapsIncomplete, "Marks all levels as incomplete in the save file.", 0 );

void CC_MarkBranchComplete( const CCommand &args )
{
	CPortalMPGameRules *pRules = PortalMPGameRules();
	if ( !pRules )
		return;

	if ( args.ArgC() <= 1 )
		return;

	pRules->SetBranchComplete( atoi(args[ 1 ]), true );

	DevMsg( "Branch Unlocked!\n" );
}
ConCommand mp_mark_course_complete( "mp_mark_course_complete", CC_MarkBranchComplete, "Marks all levels in a branch as complete in the save file.", 0 );

void CPortalMPGameRules::RegisterScriptFunctions( void )
{
	ScriptRegisterFunctionNamed( g_pScriptVM, ScriptIsMultiplayer, "IsMultiplayer", "Is this a multiplayer game?" );
	ScriptRegisterFunction( g_pScriptVM, GetPlayerSilenceDuration, "Time that the specified player has been silent on the mic." );
	ScriptRegisterFunction( g_pScriptVM, GetOrangePlayerIndex, "Player index of the orange player." );
	ScriptRegisterFunction( g_pScriptVM, GetBluePlayerIndex, "Player index of the blue player." );
	ScriptRegisterFunction( g_pScriptVM, GetCoopSectionIndex, "Section that the coop players have selected to load." );
	ScriptRegisterFunction( g_pScriptVM, GetCoopBranchLevelIndex, "Given the 'branch' argument, returns the current chosen level." );
	ScriptRegisterFunction( g_pScriptVM, GetHighestActiveBranch, "Returns which branches should be available in the hub." );
	ScriptRegisterFunction( g_pScriptVM, AddBranchLevelName, "Adds a level to the specified branche's list." );
	ScriptRegisterFunction( g_pScriptVM, SaveMPStatsData, "Save the multiplayer stats for the score board." );
	ScriptRegisterFunction( g_pScriptVM, MarkMapComplete, "Marks a maps a complete for both players." );
	ScriptRegisterFunction( g_pScriptVM, IsBranchComplete, "Returns true if every level in the branch has been completed by either." );
	ScriptRegisterFunction( g_pScriptVM, IsPlayerBranchComplete, "Returns true if every level in the branch has been completed by the specified player." );
	ScriptRegisterFunction( g_pScriptVM, IsLevelComplete, "Returns true if the level in the specified branch is completed by either player." );
	ScriptRegisterFunction( g_pScriptVM, IsPlayerLevelComplete, "Returns true if the level in the specified branch is completed by a specific player." );
	ScriptRegisterFunction( g_pScriptVM, GetPlayerDeathCount, "Returns the number of times that a specific player has died in the session." );
	ScriptRegisterFunction( g_pScriptVM, GetGladosSpokenFlags, "Returns bit flags for specific lines that we want to track per session." );
	ScriptRegisterFunction( g_pScriptVM, AddGladosSpokenFlags, "Adds bit flags for specific lines that we want to track per session." );
	ScriptRegisterFunction( g_pScriptVM, IsLocalSplitScreen, "Are these players playing in Splitscreen?" );
	ScriptRegisterFunction( g_pScriptVM, GetNumPlayersConnected, "Returns how many players are connected" );
	ScriptRegisterFunction( g_pScriptVM, AddCoopCreditsName, "Adds a name to the coop credit's list." );
	ScriptRegisterFunction( g_pScriptVM, CoopGladosBlowUpBots, "Call this to blow up both robots and prevent respawning!" );
	ScriptRegisterFunction( g_pScriptVM, CoopGetNumPortalsPlaced, "Returns the number of portals the players have placed so far." );
	ScriptRegisterFunction( g_pScriptVM, CoopGetLevelsCompletedThisBranch, "Returns the number of levels the players have completed in their run of the current branch." );
	ScriptRegisterFunction( g_pScriptVM, CoopGetBranchTotalLevelCount, "Returns the number of levels in the current branch." );
	ScriptRegisterFunction( g_pScriptVM, NotifySpeedRunSuccess, "Tells the client that a successful speed run has been completed." );
	ScriptRegisterFunction( g_pScriptVM, CoopSetMapRunTime, "Sets the time to complete a coop map from spawn to completing the puzzle." );
	ScriptRegisterFunction( g_pScriptVM, CoopSetCameFromLastDLCMap, "Set whether we came from the last coop DLC map or not" );
	ScriptRegisterFunction( g_pScriptVM, GetCameFromLastDLCMap, "Returns true if coming from the last DLC coop map." );
	ScriptRegisterFunction( g_pScriptVM, SetHaveSeenDLCTubesReveal, "Set that we have seen the DLC tubes reveal this session." );
	ScriptRegisterFunction( g_pScriptVM, GetHaveSeenDLCTubesReveal, "Get whether we have seen the DLC tubes reveal this session." );
	g_pScriptVM->RegisterInstance( &PlayerVoiceListener(), "PlayerVoiceListener" );
}
#endif // !CLIENT_DLL

#ifdef CLIENT_DLL
bool ClientIsCrossplayingWithConsole( void )
{
	IMatchSession *pSession = g_pMatchFramework->GetMatchSession();
	if ( !pSession )
		return false;

	KeyValues *kvSettings = pSession->GetSessionSettings();
	int numMachines = kvSettings->GetInt( "members/numMachines" );
	for ( int iMachine = 0; iMachine < numMachines; ++ iMachine )
	{
		uint64 uiFlags = kvSettings->GetUint64( CFmtStr( "members/machine%d/flags", iMachine ) );
		static const uint64 kFlagMachinePS3 = 1ull;
		if ( uiFlags & kFlagMachinePS3 )
			return true;
	}

	return false;
}
#endif
