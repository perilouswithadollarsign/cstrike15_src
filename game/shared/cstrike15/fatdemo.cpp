//=========== Copyright  Valve Corporation, All rights reserved. ==============//

#include "cbase.h"
#include "fatdemo.h"

#include "baseplayer_shared.h"
#include "cs_gamerules.h"
#include "gametypes/igametypes.h"

#ifdef CLIENT_DLL
#include "c_team.h"
#include "c_playerresource.h"
#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#else
#include "team.h"

#include "cs_player.h"
#include "cs_player_resource.h"
#endif

#include "weapon_csbase.h"
#include "cs_weapon_parse.h"
#include "proto_oob.h" // For MAKE_4BYTES

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if !defined( CSTRIKE_REL_BUILD )

// Globals
CCSFatDemoRecorder g_fatDemoRecorder;
CCSFatDemoRecorder *g_pFatDemoRecorder = &g_fatDemoRecorder;

ConVar csgo_fatdemo_enable( "csgo_fatdemo_enable", "0", FCVAR_RELEASE );
ConVar csgo_fatdemo_output( "csgo_fatdemo_output", "test.fatdem", FCVAR_RELEASE );

// The file structure is thus:
// FatDemoHeader
// for each protobuf message:
//     size of message
//     protobuf message

struct FatDemoHeader
{
	uint32 m_magic; // Must be characters "GOML", 
	uint32 m_version; // Which version of the header. Protobuf mechanisms are used for the actual payloads.
};

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void CaptureGameState( MLGameState* pOutState );
void CaptureMatchState( MLMatchState* pOutState );
void CaptureRoundState( MLRoundState* pOutState );
void CapturePlayerState( MLPlayerState* pOutState, CCSPlayer* pCsPlayer );
void CaptureWeaponState( MLWeaponState* pOutState, CWeaponCSBase* pCsWeapon, int index, CCSPlayer* pCsPlayer );

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
class CCSFatDemoEventVisitor : public IGameEventVisitor2
{
public:
	CCSFatDemoEventVisitor( MLEvent* pEvent )
	: m_pEvent( pEvent )
	{}

	// IGameEventVisitor2
	virtual bool VisitString( const char* name, const char* value ) OVERRIDE
	{
		MLDict* pEntry = m_pEvent->add_data();
		pEntry->set_key( name );
		pEntry->set_val_string( value );
		return true;
	}

	virtual bool VisitFloat( const char* name, float value ) OVERRIDE
	{
		MLDict* pEntry = m_pEvent->add_data();
		pEntry->set_key( name );
		pEntry->set_val_float( value );
		return true;
	}

	virtual bool VisitInt( const char* name, int value ) OVERRIDE
	{
		MLDict* pEntry = m_pEvent->add_data();
		pEntry->set_key( name );
		pEntry->set_val_int( value );
		return true;
	}

	virtual bool VisitBool( const char*name, bool value ) OVERRIDE
	{
		MLDict* pEntry = m_pEvent->add_data();
		pEntry->set_key( name );
		pEntry->set_val_int( value ? 1 : 0 );
		return true;
	}

private:
	MLEvent* m_pEvent;
};


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
CCSFatDemoRecorder::CCSFatDemoRecorder()
: m_tickcount( -1 )
, m_bInLevel( false )
, m_pCurrentTick( NULL )
{
}

// ------------------------------------------------------------------------------------------------
CCSFatDemoRecorder::~CCSFatDemoRecorder()
{

}

// ------------------------------------------------------------------------------------------------
void CCSFatDemoRecorder::Reset()
{
	// Sync up the state of our trackers with the current state of the game.
}

// ------------------------------------------------------------------------------------------------
void CCSFatDemoRecorder::FireGameEvent( IGameEvent *pEvent )
{
	if ( !csgo_fatdemo_enable.GetBool() )
		return;

	if ( !m_pCurrentTick )
		return;

	MLEvent* pOutEvent = m_pCurrentTick->add_events();
	pOutEvent->set_event_name( pEvent->GetName() );
	CCSFatDemoEventVisitor visitor( pOutEvent );
	pEvent->ForEventData( &visitor );
}

// ------------------------------------------------------------------------------------------------
void CCSFatDemoRecorder::PostInit()
{
	ListenForAllGameEvents();
}

// ------------------------------------------------------------------------------------------------
void CCSFatDemoRecorder::LevelInitPreEntity()
{
	BeginFile();

	m_bInLevel = true;
	m_tickcount = -1;
}

// ------------------------------------------------------------------------------------------------
void CCSFatDemoRecorder::LevelShutdownPostEntity()
{
#ifdef _LINUX
	bool bWasInLevel = m_bInLevel;
#endif

	m_bInLevel = false;

	FinalizeFile();

	// Clean up our temp memory.
	m_tempPacketStorage.Purge();

	if ( m_pCurrentTick )
	{
		delete m_pCurrentTick;
		m_pCurrentTick = NULL;
	}

	// There's an ugly crash in the bowels of scaleform that makes it hard for us to tell whether
	// CSGO was actually successful or not. However, at this point we have been successful, so we
	// should go ahead and exit with a success code if we're in demo_quitafterplayback mode (which
	// is the usual case for autonomous capture.
#ifdef _LINUX
	static ConVarRef demo_quitafterplayback( "demo_quitafterplayback" );
	if ( bWasInLevel && demo_quitafterplayback.GetBool() )
	{
		_exit( 0 );
	}
#endif
}

// ------------------------------------------------------------------------------------------------
void CCSFatDemoRecorder::OnTickPre( int tickcount )
{
	if ( !csgo_fatdemo_enable.GetBool() )
		return;

	// Guard against multiple updates in the client if we're running a demo that isn't a timedemo.
	if ( m_tickcount == tickcount ) 
		return;

	if ( !m_bInLevel )
		return;

	if ( !m_outFile )
		return;

	Assert( CSGameRules() );

	if ( m_pCurrentTick )
	{
		m_pCurrentTick->set_tick_count( tickcount );
		CaptureGameState( m_pCurrentTick->mutable_state() );
		
		OutputProtobuf( m_pCurrentTick );

		// TODO: This should be serialized or written out to a queue or something.
		delete m_pCurrentTick;
		m_pCurrentTick = NULL;
	}

	// Set up the current tick for next tick. We do this here so that any events captured from now
	// until then affect the next tick (since we're done with this tick).
	m_pCurrentTick = new MLTick;

	// We've updated for this tick now.
	m_tickcount = tickcount;
}

// ------------------------------------------------------------------------------------------------
void CCSFatDemoRecorder::OutputProtobuf( ::google::protobuf::Message* pProto )
{
	Assert( pProto );

	int32 size = pProto->ByteSize();
	int32 totalSize = size + sizeof( int32 );

	m_tempPacketStorage.EnsureCapacity( totalSize );
	*( ( int32* ) m_tempPacketStorage.Base() ) = size;

	if ( !pProto->SerializeToArray( ( ( byte* ) m_tempPacketStorage.Base() ) + sizeof( int ), size ) )
	{
		Assert( !"Serialization failed for... reasons." );
		return;
	}

	g_pFullFileSystem->Write( m_tempPacketStorage.Base(), totalSize, m_outFile );
}

// ------------------------------------------------------------------------------------------------
void CCSFatDemoRecorder::BeginFile()
{
	char buffer[MAX_PATH];
	V_strcpy_safe( buffer, csgo_fatdemo_output.GetString() );
	V_DefaultExtension( buffer, ".fatdem", sizeof( buffer ) );

	m_outFile = g_pFullFileSystem->OpenEx( buffer,  "wb" );
	if ( !m_outFile )
		return;
	
	FatDemoHeader header;
	header.m_magic = MAKE_4BYTES( 'G', 'O', 'M', 'L' );
	header.m_version = 1;

	g_pFullFileSystem->Write( &header, sizeof( header ), m_outFile );

	// Now the protobuf header, 
	MLDemoHeader protoHeader;
#ifdef CLIENT_DLL
	protoHeader.set_map_name( engine->GetLevelNameShort() );
#else
	protoHeader.set_map_name( gpGlobals->mapname.ToCStr() );
#endif

	if ( gpGlobals->interval_per_tick != 0.0f )
		protoHeader.set_tick_rate(1 / gpGlobals->interval_per_tick );

#ifdef CLIENT_DLL
	protoHeader.set_version( engine->GetClientVersion() );
#else
	protoHeader.set_version( engine->GetServerVersion() );
#endif

#ifndef NO_STEAM
	EUniverse eUniverse = steamapicontext && steamapicontext->SteamUtils()
		? steamapicontext->SteamUtils()->GetConnectedUniverse()
		: k_EUniverseInvalid;
	protoHeader.set_steam_universe( ( int ) eUniverse );
#else
	// Pretty sure this doesn't actually work anymore.
	protoHeader.set_steam_universe( -1 );
#endif

	OutputProtobuf( &protoHeader );
}

// ------------------------------------------------------------------------------------------------
void CCSFatDemoRecorder::FinalizeFile()
{
	if ( m_outFile )
		g_pFullFileSystem->Close( m_outFile );
	m_outFile = 0;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void CaptureGameState( MLGameState* pOutState )
{
	CaptureMatchState( pOutState->mutable_match() );
	CaptureRoundState( pOutState->mutable_round() );

	for ( int i = 1; i < MAX_PLAYERS; ++i )
	{
		CCSPlayer* pPlayer = dynamic_cast< CCSPlayer* >( UTIL_PlayerByIndex( i ) );

		if ( !pPlayer )
			continue;

		CapturePlayerState( pOutState->add_players(), pPlayer );
	}
}

// ------------------------------------------------------------------------------------------------
void CaptureMatchState( MLMatchState* pOutState )
{
	char const *szGameMode = g_pGameTypes->GetGameModeFromInt( g_pGameTypes->GetCurrentGameType(), g_pGameTypes->GetCurrentGameMode() );
	if ( !szGameMode || !*szGameMode )
		szGameMode = "custom";

	pOutState->set_game_mode( szGameMode );

	char const *szPhase = "warmup";
	bool bActivePhase = false;
	if ( !CSGameRules()->IsWarmupPeriod() )
	{
		bActivePhase = true;
		switch ( CSGameRules()->GetGamePhase() )
		{
		case GAMEPHASE_HALFTIME:
			szPhase = "intermission";
			break;
		case GAMEPHASE_MATCH_ENDED:
			szPhase = "gameover";
			break;
		default:
			szPhase = "live";
			break;
		}
	}

	pOutState->set_phase( szPhase );

	if ( bActivePhase )
		pOutState->set_round( CSGameRules()->GetTotalRoundsPlayed() );

	int nTeams[2] = { TEAM_CT, TEAM_TERRORIST };
	int nScores[ 2 ] = { 0, 0 };
	for ( int j = 0; j < 2; ++j )
	{
		auto *pTeam = GetGlobalTeam( nTeams[ j ] );

		if ( !pTeam ) 
			continue;

#ifdef CLIENT_DLL
		nScores[ j ] = pTeam->Get_Score();
#else
		nScores[ j ] = pTeam->GetScore();
#endif
	}

	pOutState->set_score_ct( nScores[ 0 ] );
	pOutState->set_score_t( nScores[ 1 ] );
}

// ------------------------------------------------------------------------------------------------
void CaptureRoundState( MLRoundState* pOutState )
{
	char const *szPhase = "freezetime";
	if ( !CSGameRules()->IsFreezePeriod() )
	{
		if ( CSGameRules()->IsRoundOver() )
			szPhase = "over";
		else
			szPhase = "live";
	}
	pOutState->set_phase( szPhase );

	switch ( CSGameRules()->m_iRoundWinStatus )
	{
	case WINNER_CT:
		pOutState->set_win_team( ET_CT );
		break;
	case WINNER_TER:
		pOutState->set_win_team( ET_Terrorist );
		break;
	}

	if ( CSGameRules()->IsBombDefuseMap() )
	{
		char const *szBombState = "";

		if ( CSGameRules()->m_bBombPlanted && !CSGameRules()->IsRoundOver() )
			szBombState = "planted";

		if ( CSGameRules()->IsRoundOver() )
		{
			// Check if the bomb exploded or got defused?
			switch ( CSGameRules()->m_eRoundWinReason )
			{
			case Target_Bombed:
				szBombState = "exploded";
				break;
			case Bomb_Defused:
				szBombState = "defused";
				break;
			}
		}

		if ( *szBombState )
			pOutState->set_bomb_state( szBombState );
	}
}

// ------------------------------------------------------------------------------------------------
static void DemoSetVector( CMsgVector* pOutVec, const Vector& inVec )
{
	Assert( pOutVec );
	pOutVec->set_x( inVec.x );
	pOutVec->set_y( inVec.y );
	pOutVec->set_z( inVec.z );
}

// ------------------------------------------------------------------------------------------------
static void DemoSetQAngle( CMsgQAngle* pOutAng, const QAngle& inAng )
{
	Assert( pOutAng );
	pOutAng->set_x( inAng.x );
	pOutAng->set_y( inAng.y );
	pOutAng->set_z( inAng.z );
}

// ------------------------------------------------------------------------------------------------
static void DemoSetQAngleAndForward( CMsgQAngle* pOutAng, CMsgVector* pOutVec, const QAngle& inAng )
{
	DemoSetQAngle( pOutAng, inAng );
	
	Vector fwd;
	AngleVectors( inAng, &fwd );
	DemoSetVector( pOutVec, fwd );
}

// ------------------------------------------------------------------------------------------------
void CapturePlayerState( MLPlayerState* pOutState, CCSPlayer* pCsPlayer )
{
	CSteamID steamID; 
	if ( pCsPlayer->GetSteamID( &steamID ) )
		pOutState->set_account_id(  steamID.GetAccountID() );

	pOutState->set_entindex( pCsPlayer->entindex() );

	pOutState->set_name( pCsPlayer->GetPlayerName() );
	// pOutState->set_clan( );
	pOutState->set_team( ( ETeam )( pCsPlayer->GetTeamNumber() ) );
	pOutState->set_user_id( pCsPlayer->GetUserID() );

	DemoSetVector( pOutState->mutable_abspos(), pCsPlayer->GetAbsOrigin() );
	DemoSetQAngleAndForward( pOutState->mutable_eyeangle(), pOutState->mutable_eyeangle_fwd(), pCsPlayer->EyeAngles() );

	pOutState->set_health( pCsPlayer->GetHealth() );
	pOutState->set_armor( pCsPlayer->ArmorValue() );
#ifdef CLIENT_DLL
	pOutState->set_flashed( clamp( pCsPlayer->m_flFlashOverlayAlpha, 0.0f, 1.0f ) );
	pOutState->set_smoked( clamp( pCsPlayer->GetLastSmokeOverlayAlpha(), 0.0f, 1.0f ) );
	pOutState->set_money( pCsPlayer->GetAccount() );
	pOutState->set_helmet( pCsPlayer->HasHelmet() );
#else
	// TODO pOutState->set_flashed( clamp( pCsPlayer->m_flFlashOverlayAlpha, 0.0f, 1.0f ) );
	// TODO	pOutState->set_smoked( clamp( pCsPlayer->GetLastSmokeOverlayAlpha(), 0.0f, 1.0f ) );
	pOutState->set_money( pCsPlayer->m_iAccount );
	pOutState->set_helmet( pCsPlayer->m_bHasHelmet );

#endif
	pOutState->set_round_kills( pCsPlayer->m_iNumRoundKills );
	pOutState->set_round_killhs( pCsPlayer->m_iNumRoundKillsHeadshots  );

	pOutState->set_defuse_kit( pCsPlayer->HasDefuser() );

	float flOnFireAmount = 0.0f;
	if ( ( pCsPlayer->m_fMolotovDamageTime > 0.0f ) && ( gpGlobals->curtime - pCsPlayer->m_fMolotovDamageTime < 2 ) )	// took burn damage in last two seconds
	{
		flOnFireAmount = ( gpGlobals->curtime - pCsPlayer->m_fMolotovDamageTime <= 1.0f ) 
					   ? 1.0f
					   : ( 2.0f - gpGlobals->curtime + pCsPlayer->m_fMolotovDamageTime );
	}
	pOutState->set_burning( clamp( flOnFireAmount, 0.0f, 1.0f ) );

	int numWeapons = 0;
	for ( int i = 0; i < pCsPlayer->WeaponCount(); ++i )
	{
		CWeaponCSBase* pCsWeapon = dynamic_cast< CWeaponCSBase * >( pCsPlayer->GetWeapon( i ) );
		if ( !pCsWeapon ) 
			continue;

		CaptureWeaponState( pOutState->add_weapons(), pCsWeapon, numWeapons, pCsPlayer );
		++numWeapons;
	}
}

// ------------------------------------------------------------------------------------------------
void CaptureWeaponState( MLWeaponState* pOutState, CWeaponCSBase* pCsWeapon, int index, CCSPlayer* pCsPlayer )
{
	pOutState->set_index( index );
	pOutState->set_name( pCsWeapon->GetName() );
	pOutState->set_type( ( EWeaponType ) pCsWeapon->GetWeaponType() );
	pOutState->set_recoil_index( pCsWeapon->m_flRecoilIndex );

	if ( pCsWeapon->m_iClip1 >= 0 )
		pOutState->set_ammo_clip( pCsWeapon->m_iClip1 );

	int iMaxClip1 = pCsWeapon->GetMaxClip1();
	if ( iMaxClip1 > 0 )
		pOutState->set_ammo_clip_max( iMaxClip1 );

	if ( pCsWeapon->GetPrimaryAmmoType() >= 0 )
		pOutState->set_ammo_reserve( pCsWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY ) );

	char const *szState = "holstered";
	if ( pCsPlayer->GetActiveCSWeapon() == pCsWeapon )
	{
		szState = "active";
		if ( pCsWeapon->m_bInReload )
			szState = "reloading";
	}
	pOutState->set_state( szState );
}

#endif