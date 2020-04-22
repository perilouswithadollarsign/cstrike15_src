//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cbase.h"
#include "hltvdirector.h"
#include "igameevents.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


class CCSHLTVDirector : public CHLTVDirector
{
public:
	DECLARE_CLASS( CCSHLTVDirector, CHLTVDirector );

	const char** GetModEvents();
	void AddHLTVServer( IHLTVServer *hltv );
	void CreateShotFromEvent( CHLTVGameEvent *event );

};

void CCSHLTVDirector::AddHLTVServer( IHLTVServer *hltv )
{
	BaseClass::AddHLTVServer( hltv );

	// mod specific events the director uses to find interesting shots
	ListenForGameEvent( "hostage_rescued" );
	ListenForGameEvent( "hostage_killed" );
	ListenForGameEvent( "hostage_hurt" );
	ListenForGameEvent( "hostage_follows" );
	ListenForGameEvent( "bomb_pickup" );
	ListenForGameEvent( "bomb_dropped" );
	ListenForGameEvent( "bomb_exploded" );
	ListenForGameEvent( "bomb_defused" );
	ListenForGameEvent( "bomb_planted" );
	ListenForGameEvent( "bomb_begindefuse" );
	ListenForGameEvent( "bomb_beginplant" );
	ListenForGameEvent( "vip_escaped" );
	ListenForGameEvent( "vip_killed" );
}


void CCSHLTVDirector::CreateShotFromEvent( CHLTVGameEvent *event )
{
	// show event at least for 2 more seconds after it occured
	const char *name = event->m_Event->GetName();
	IGameEvent *shot = NULL;

	CBaseEntity *player = NULL;

	if ( !Q_strcmp( "hostage_rescued", name ) ||
		 !Q_strcmp( "hostage_hurt", name ) ||
		 !Q_strcmp( "hostage_follows", name ) ||
		 !Q_strcmp( "hostage_killed", name ) )
	{
		player = UTIL_PlayerByUserId( event->m_Event->GetInt("userid") );

		if ( !player )
			return;

		// shot player as primary, hostage as secondary target
		shot = gameeventmanager->CreateEvent( "hltv_chase", true );
		shot->SetInt( "target1", player->entindex() );
		shot->SetInt( "target2", event->m_Event->GetInt("hostage") );
		shot->SetFloat( "distance", 96.0f );
		shot->SetInt( "theta", 40 );
		shot->SetInt( "phi", 20 );

		// shot 2 seconds after event
		m_nNextShotTick = MIN( m_nNextShotTick, (event->m_Tick+TIME_TO_TICKS(2.0)) );
		m_iPVSEntity = player->entindex();
	}

	else if (	!Q_strcmp( "bomb_beginplant", name ) ||
				!Q_strcmp( "bomb_begindefuse", name ) )
	{

		player = UTIL_PlayerByUserId( event->m_Event->GetInt("userid") );

		if ( !player )
			return;

		// chasecam
		shot = gameeventmanager->CreateEvent( "hltv_chase", true );

		if ( shot )
		{
			shot->SetInt( "target1", player->entindex() );
			shot->SetInt( "target2", 0 );
			shot->SetFloat( "distance", 500.0f );
			shot->SetInt( "theta", 180 );
			shot->SetInt( "phi", 45 );
			shot->SetBool( "ineye", true );

			// shot 3 seconds after pickup
			m_nNextShotTick = MIN( m_nNextShotTick, (event->m_Tick+TIME_TO_TICKS(3.0)) );
			m_iPVSEntity = player->entindex();

			for ( int i = 0; i < m_HltvServers.Count(); ++i )
			{
				m_HltvServers[ i ].m_pHLTVServer->BroadcastEvent( shot );
			}
			gameeventmanager->FreeEvent( shot );
			DevMsg("DrcCmd: %s\n", name );

			return;
		}
	}
	
	// let baseclass create a shot
	BaseClass::CreateShotFromEvent( event );	
}

const char** CCSHLTVDirector::GetModEvents()
{
	// game events relayed to spectator clients
	static const char *s_modevents[] =
	{
		"hltv_status",
		"hltv_chat",
		"player_connect",
		"player_connect_full",
		"player_disconnect",
		"player_team",
		"player_info",
		"server_cvar",
		"player_changename",
		"teamplay_broadcast_audio",
		"player_death",
		"other_death",
		"player_hurt",
		"player_chat",
		"round_start",
		"round_end",
		// additional CS:S events:
		"bomb_planted",	
		"bomb_defused",
		"bomb_beginplant",	
		"bomb_begindefuse",
		"hostage_killed",
		"hostage_hurt",
		"begin_new_match",
		// UI events
		"cs_match_end_restart",
		"cs_game_disconnected",
		"announce_phase_end", 	
		"round_mvp",	
		"server_spawn",
		"player_spawn",
		"hltv_status",
		"cs_win_panel_round",
		"endmatch_cmm_start_reveal_items",
		"game_newmap",
		"hostage_rescued",
		"bomb_exploded",
		"bomb_pickup",
		"bomb_dropped",
		"defuser_pickup",
		"defuser_dropped",
		"decoy_started",
		"decoy_detonate",
		"hegrenade_detonate",
		"flashbang_detonate",
		"smokegrenade_detonate",
		"smokegrenade_expired",
		"inferno_startburn",
		"inferno_expire",
		"bot_takeover",
		"bomb_beep",
		"weapon_fire",
		"weapon_fire_on_empty",
		"weapon_outofammo",
		"weapon_reload",
		"weapon_zoom",
		"player_footstep",
		"player_jump",
		"player_blind",
		"round_freeze_end",
		"cs_win_panel_match",			
		"cs_pre_restart",
		"tournament_reward",
		"item_found",
		"items_gifted",
		"achievement_earned",
		"round_announce_warmup",
		"round_announce_last_round_half",
		"round_announce_final",
		"round_announce_match_point",
		"round_poststart",
		"buytime_ended",
		"round_time_warning",
		"dm_bonus_weapon_start",
		"endmatch_mapvote_selecting_map",
		"cs_round_start_beep",
		"cs_round_final_beep",
		"round_announce_match_start",
		"seasoncoin_levelup",
		"player_falldamage",
		"hostage_rescued_all",
		"round_officially_ended",
		"round_prestart",
		NULL
	};

	return s_modevents;
}

static CCSHLTVDirector s_HLTVDirector;	// singleton

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CHLTVDirector, IHLTVDirector, INTERFACEVERSION_HLTVDIRECTOR, s_HLTVDirector );

CHLTVDirector* HLTVDirector()
{
	return &s_HLTVDirector;
}

IGameSystem* HLTVDirectorSystem()
{
	return &s_HLTVDirector;
}