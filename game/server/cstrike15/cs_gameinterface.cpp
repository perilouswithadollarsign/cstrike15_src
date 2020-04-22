//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "cdll_int.h"
#include "gameinterface.h"
#include "mapentities.h"
#include "cs_gameinterface.h"
#include "ai_responsesystem.h"
#include "iachievementmgr.h"
#include "fmtstr.h"
#include "gametypes.h"
#include "matchmaking/imatchframework.h"
#include "cs_shareddefs.h"
#include "cs_gamerules.h"
#include "gametypes.h"
#include "engine/inetsupport.h"
#include "dedicated_server_ugc_manager.h"
#include "cs_player.h"
#include "server_log_http_dispatcher.h"

#include "netmessages.h"
#include "usermessages.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//////////////////////////////////////////////////////////////////////////
//
// Convars
//
ConVar sv_workshop_allow_other_maps( "sv_workshop_allow_other_maps", "1", FCVAR_RELEASE, "When hosting a workshop collection, users can play other workshop map on this server when it is empty and then mapcycle into this server collection." );
static ConVar tv_allow_camera_man_steamid( "tv_allow_camera_man_steamid", "", FCVAR_RELEASE, "Allows tournament production cameraman to run csgo.exe -interactivecaster on SteamID 7650123456XXX and be the camera man." );

// #define SVGC_RESERVATION_DEBUG 1

// -------------------------------------------------------------------------------------------- //
// Mod-specific CServerGameClients implementation.
// -------------------------------------------------------------------------------------------- //

void CServerGameClients::GetPlayerLimits( int& minplayers, int& maxplayers, int &defaultMaxPlayers ) const
{
	minplayers = 1;  // allow single player for the test maps (but we default to multi)
	maxplayers = MAX_PLAYERS;
	
	defaultMaxPlayers = MAX_PLAYERS;
}


// -------------------------------------------------------------------------------------------- //
// Mod-specific CServerGameDLL implementation.
// -------------------------------------------------------------------------------------------- //

void CServerGameDLL::LevelInit_ParseAllEntities( const char *pMapEntities )
{
	if ( Q_strcmp( STRING(gpGlobals->mapname), "cs_" ) )
	{
		// don't precache AI responses (hostages) if it's not a hostage rescure map
		extern IResponseSystem *g_pResponseSystem;
		g_pResponseSystem->PrecacheResponses( false );	
	}
}

//
// Twitch.tv reservation updates
//
class ClientJob_EMsgGCCStrike15_v2_GC2ServerReservationUpdate : public GCSDK::CGCClientJob 
{
public:
	ClientJob_EMsgGCCStrike15_v2_GC2ServerReservationUpdate( GCSDK::CGCClient *pGCClient ) 
		: GCSDK::CGCClientJob( pGCClient )
	{
	}
	virtual bool BYieldingRunJobFromMsg( GCSDK::IMsgNetPacket *pNetPacket )
	{
		GCSDK::CProtoBufMsg<CMsgGCCStrike15_v2_GC2ServerReservationUpdate> msg( pNetPacket );

		uint32 numTotalViewers = msg.Body().viewers_external_total();
		uint32 numSteamLinkedViewers = msg.Body().viewers_external_steam();
		
		engine->UpdateHltvExternalViewers( numTotalViewers, numSteamLinkedViewers );

		return true;
	}
};
GC_REG_CLIENT_JOB( ClientJob_EMsgGCCStrike15_v2_GC2ServerReservationUpdate, k_EMsgGCCStrike15_v2_GC2ServerReservationUpdate );

void GCCStrikeWelcomeMessageReceived( CMsgCStrike15Welcome const &msgCStrike )
{
}

bool Helper_FillServerReservationStateAndPlayers( CMsgGCCStrike15_v2_MatchmakingServerReservationResponse &msgbody )
{
	if ( !engine->IsDedicatedServer() )
		return false;

	msgbody.set_server_version( ( ( INetSupport * ) g_pMatchFramework->GetMatchExtensions()->GetRegisteredExtensionInterface( INETSUPPORT_VERSION_STRING ) )->GetEngineBuildNumber() );
	msgbody.set_map( STRING( gpGlobals->mapname ) );
	static ConVarRef sv_steamdatagramtransport_port( "sv_steamdatagramtransport_port" );

	// Expose information about our community server GOTV port so that clients could connect
	static ConVarRef tv_advertise_watchable( "tv_advertise_watchable" );
	static int s_nTvPort = 0;	// make the TV port sticky: if we reported it non-zero once then keep reporting
	CEngineHltvInfo_t engineHltvInfo;
	if ( tv_advertise_watchable.GetBool() &&
		engine->GetEngineHltvInfo( engineHltvInfo ) && engineHltvInfo.m_bBroadcastActive )
	{
		s_nTvPort = engineHltvInfo.m_nTvPort;
	}
	if ( s_nTvPort )
	{
		msgbody.mutable_tv_info()->set_tv_udp_port( s_nTvPort );
	}

	// Build the list of players who are actively playing on the game server
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer )
		{
			if ( pPlayer->IsBot() )
				continue;
			CSteamID steamIdPlayer;
			if ( !pPlayer->GetSteamID( &steamIdPlayer ) )
				continue;
			if ( !steamIdPlayer.IsValid() )
				continue;
			switch ( pPlayer->GetTeamNumber() )
			{
			case TEAM_CT:
			case TEAM_TERRORIST:
				msgbody.add_reward_player_accounts( steamIdPlayer.GetAccountID() );
				break;
			default:
				msgbody.add_idle_player_accounts( steamIdPlayer.GetAccountID() );
				break;
			}
		}
	}

	return true;
}


void CServerGameDLL::UpdateGCInformation()
{
	/** Removed for partner depot **/
}

// Marks the queue matchmaking game as starting
void CServerGameDLL::ReportGCQueuedMatchStart( int32 iReservationStage, uint32 *puiConfirmedAccounts, int numConfirmedAccounts )
{
	/** Removed for partner depot **/
}

//-----------------------------------------------------------------------------
// Purpose: A user has had their network id setup and validated 
//-----------------------------------------------------------------------------
void CServerGameClients::NetworkIDValidated( const char *pszUserName, const char *pszNetworkID, CSteamID steamID )
{
	/** Removed for partner depot **/
}



//
// Order workshop maps by MRU
//
static int Helper_SortWorkshopMapsMRU( const DedicatedServerUGCFileInfo_t * const *a, const DedicatedServerUGCFileInfo_t * const *b )
{
	if ( (*a)->m_dblPlatFloatTimeReceived != (*b)->m_dblPlatFloatTimeReceived )
		return ( (*a)->m_dblPlatFloatTimeReceived > (*b)->m_dblPlatFloatTimeReceived ) ? -1 : 1;
	else
		return 0;
}

//
// Matchmaking game data buffer to set into SteamGameServer()->SetGameData
//
void CServerGameDLL::GetMatchmakingGameData( char *buf, size_t bufSize )
{
	char * const bufBase = buf;
	int len = 0;

	extern ConVar game_type;
	extern ConVar game_mode;

	// Put the game key
	Q_snprintf( buf, bufSize, "g:csgo,gt:%u,gm:%u,", game_type.GetInt(), game_mode.GetInt() );
	len = strlen( buf );
	buf += len;
	bufSize -= len;


	if ( gpGlobals && !StringIsEmpty( gpGlobals->mapGroupName.ToCStr() ) )
	{
		const CUtlStringList* mapsInGroup = g_pGameTypes->GetMapGroupMapList( gpGlobals->mapGroupName.ToCStr() );
		if ( mapsInGroup && g_pGameTypes->IsWorkshopMapGroup( gpGlobals->mapGroupName.ToCStr() ) )
		{
			if ( sv_workshop_allow_other_maps.GetBool() && ( bufSize >= 7 ) )
			{	// Advertise support for other maps
				Q_strncpy( buf, "wks:1,", 7 );
				buf += 6;
				bufSize -= 7;
			}

			CUtlVector< PublishedFileId_t > arrAdvertisedFileIds;
			FOR_EACH_VEC( *mapsInGroup, i )
			{
				PublishedFileId_t id = DedicatedServerWorkshop().GetUGCMapPublishedFileID((*mapsInGroup)[i]);
				CFmtStr szIdAsHexString( "%llx", id );
				size_t len = szIdAsHexString.Length();

				if ( bufSize <= len + 1 )
				{
					Warning( "GameData: Too many community maps installed, not advertising for map id \"%llu (0x%s)\"\n", id, szIdAsHexString.Access() );
					continue;
				}

				Q_strncpy( buf, szIdAsHexString.Access(), len + 1 );
				buf += len;
				*( buf ++ ) = ',';
				bufSize -= len + 1;

				arrAdvertisedFileIds.AddToTail( id );
			}

			// Advertise maps that have been recently checked and downloaded from Workshop
			if ( sv_workshop_allow_other_maps.GetBool() )
			{
				CUtlVector<const DedicatedServerUGCFileInfo_t *> arrInfoMaps;
				DedicatedServerWorkshop().GetWorkshopMasWithValidUgcInformation( arrInfoMaps );
				arrInfoMaps.Sort( Helper_SortWorkshopMapsMRU );
				FOR_EACH_VEC( arrInfoMaps, iInfoMap )
				{
					PublishedFileId_t id = arrInfoMaps[iInfoMap]->fileId;
					if ( arrAdvertisedFileIds.Find( id ) != arrAdvertisedFileIds.InvalidIndex() )
						continue; // already advertised

					CFmtStr szIdAsHexString( "%llx", id );
					size_t len = szIdAsHexString.Length();

					if ( bufSize <= len + 1 )
						break;	// Advertise only as much downloaded stuff as can fit

					Q_strncpy( buf, szIdAsHexString.Access(), len + 1 );
					buf += len;
					*( buf ++ ) = ',';
					bufSize -= len + 1;
				}
			}
		}
	}

	// Trim the last comma if anything was written
	if ( buf > bufBase )
		buf[ -1 ] = 0;
}

// this returns true if they were already in the list or were successfully added
// returns false if they were not added (not allowed to be a caster)
bool AddAccountToActiveCasters( const CSteamID &steamID )
{
	// first check if they are already in the list
	bool bAlreadyAdded = false;
	for ( int j = 0; j < CSGameRules()->m_arrTournamentActiveCasterAccounts.Count(); j++ )
	{
		if ( steamID.GetAccountID() == CSGameRules()->m_arrTournamentActiveCasterAccounts[ j ] )
		{
			// this caster is already in the list so skip adding them, but allow them
			bAlreadyAdded = true;
			break;
		}
		if ( CSGameRules()->m_arrTournamentActiveCasterAccounts[ j ] )
		{
			// already have an active caster, so don't allow another
			return false;
		}
	}

	if ( !bAlreadyAdded )
	{
		// not already added, so find an empty slot and put them in it
		for (int j = 0; j < CSGameRules()->m_arrTournamentActiveCasterAccounts.Count(); j++ )
		{
			if ( CSGameRules()->m_arrTournamentActiveCasterAccounts[ j ] == 0 )
			{
				CSGameRules()->m_arrTournamentActiveCasterAccounts.Set( j, steamID.GetAccountID() );
				if ( steamapicontext->SteamUser() && steamapicontext->SteamFriends() )
				{
					const char *pszName = steamapicontext->SteamFriends()->GetFriendPersonaName( steamID );
					ConMsg( "Adding %s (ID:%d) to active caster list!\n", pszName, steamID.GetAccountID() );
				}
				else
				{
					ConMsg( "Adding ID:%d to active caster list!\n", steamID.GetAccountID() );
				}
				break;
			}
		}
	}

	return true;
}

// validate if player is a caster and is not playing in the current game, then add them to the active caster list
// returns false if they are not allow to be a caster
bool CServerGameDLL::ValidateAndAddActiveCaster( const CSteamID &steamID )
{
	// check if they are a player in the current game. Note: players can be casters sometimes (and might be in the casters list below), but we don't want their voice data "public" when they are playing
	for ( int i = 0; i < CCSGameRules::sm_QueuedServerReservation.account_ids().size(); i++ )
	{
		if ( steamID.GetAccountID() == CCSGameRules::sm_QueuedServerReservation.account_ids( i ) )
		{
			// this is a player
			return false;
		}
	}
	// they weren't in the player list, so now check the caster list
	for ( int i = 0; i < CCSGameRules::sm_QueuedServerReservation.tournament_casters_account_ids().size(); i++ )
	{
		if ( steamID.GetAccountID() == CCSGameRules::sm_QueuedServerReservation.tournament_casters_account_ids( i ) )
		{
			// this is a caster
			return AddAccountToActiveCasters( steamID );
		}
	}
	if ( tv_allow_camera_man_steamid.GetString()[0] && engine->IsDedicatedServer() )
	{
		CSteamID steamidCameraMan( V_atoui64( tv_allow_camera_man_steamid.GetString() ) );
		if ( steamidCameraMan.IsValid() && steamidCameraMan.BIndividualAccount() && steamidCameraMan.GetAccountID() &&
			( steamidCameraMan.GetAccountID() == steamID.GetAccountID() ) )
		{
			return AddAccountToActiveCasters( steamID );
		}
	}
	return false;
}

// Returns which encryption key to use for messages to be encrypted for TV
EncryptedMessageKeyType_t CServerGameDLL::GetMessageEncryptionKey( INetMessage *pMessage )
{
	switch ( pMessage->GetType() )
	{
	case svc_VoiceData:
		{
			// check the voice data packets for being from an active caster and add the caster flag and use the public key
			CSVCMsg_VoiceData_t *pVoiceData = ( CSVCMsg_VoiceData_t * ) pMessage;
			CSteamID steamID( static_cast<uint64>( pVoiceData->xuid() ) );
			if ( steamID.GetAccountID() )
			{
				for ( int j = 0; j < CSGameRules()->m_arrTournamentActiveCasterAccounts.Count(); j++ )
				{
					if ( steamID.GetAccountID() == CSGameRules()->m_arrTournamentActiveCasterAccounts[ j ] )
					{
						pVoiceData->set_caster( true );
						return kEncryptedMessageKeyType_Public;
					}
				}
			}
		}
		return kEncryptedMessageKeyType_Private;

	case svc_UserMessage:
		{
			CSVCMsg_UserMessage_t *pUsrMessageHeader = ( CSVCMsg_UserMessage_t * ) pMessage;
			switch ( pUsrMessageHeader->msg_type() )
			{
			case CS_UM_SayText:
				{
					CCSUsrMsg_SayText usrMsg;
					if ( usrMsg.ParseFromArray( &pUsrMessageHeader->msg_data().at( 0 ), pUsrMessageHeader->msg_data().size() ) )
					{
						if ( usrMsg.textallchat() )
							return kEncryptedMessageKeyType_Public;
					}
				}
				return kEncryptedMessageKeyType_Private;
			
			case CS_UM_SayText2:
				{
					CCSUsrMsg_SayText2 usrMsg;
					if ( usrMsg.ParseFromArray( &pUsrMessageHeader->msg_data().at( 0 ), pUsrMessageHeader->msg_data().size() ) )
					{
						if ( usrMsg.textallchat() )
							return kEncryptedMessageKeyType_Public;
					}
				}
				return kEncryptedMessageKeyType_Private;

			case CS_UM_TextMsg:
			case CS_UM_RadioText:
			case CS_UM_RawAudio:
			case CS_UM_SendAudio:
				return kEncryptedMessageKeyType_Private;

			default:
				return kEncryptedMessageKeyType_None;
			}
		}
		return kEncryptedMessageKeyType_None;

	case svc_EncryptedData:
	default:
		return kEncryptedMessageKeyType_None;
	}
}

// If server game dll needs more time before server process quits then
// it should return true to hold game server reservation from this interface method.
// If this method returns false then the server process will clear the reservation
// and might shutdown to meet uptime or memory limit requirements.
bool CServerGameDLL::ShouldHoldGameServerReservation( float flTimeElapsedWithoutClients )
{
	/** Removed for partner depot **/
	return false; // let the server get unreserved
}

// Pure server validation failed for the given client, client supplied
// data is included in the payload
void CServerGameDLL::OnPureServerFileValidationFailure( edict_t *edictClient, const char *path, const char *fileName, uint32 crc, int32 hashType, int32 len, int packNumber, int packFileID )
{
	/** Removed for partner depot **/
}

// Last chance validation on connect packet for the client, non-NULL return value
// causes the client connect to be aborted with the provided error
char const * CServerGameDLL::ClientConnectionValidatePreNetChan( bool bGameServer, char const *adr, int nAuthProtocol, uint64 ullSteamID )
{
	/** Removed for partner depot **/
	return NULL;	// allow connections by default
}

// Network channel notification from engine to game server code
void CServerGameDLL::OnEngineClientNetworkEvent( edict_t *edictClient, uint64 ullSteamID, int nEventType, void *pvParam )
{
	/** Removed for partner depot **/
}

// Game server notifying GC with its sync packet
void CServerGameDLL::EngineGotvSyncPacket( const CEngineGotvSyncPacket *pPkt )
{
	/** Removed for partner depot **/
}

// GOTV client attempt redirect over SDR
bool CServerGameDLL::OnEngineClientProxiedRedirect( uint64 ullClient, const char *adrProxiedRedirect, const char *adrRegular )
{
	/** Removed for partner depot **/
	return false;
}

bool CServerGameDLL::LogForHTTPListeners( const char* szLogLine )
{
	return GetServerLogHTTPDispatcher()->LogForHTTPListeners( szLogLine );
}
	
//-----------------------------------------------------------------------------
// Purpose: Called to apply lobby settings to a dedicated server
//-----------------------------------------------------------------------------
void CServerGameDLL::ApplyGameSettings( KeyValues *pKV )
{
	if ( !pKV )
	{
		return;
	}

	if ( engine )
	{
		DevMsg( "CServerGameDLL::ApplyGameSettings game settings payload received:\n" );
		KeyValuesDumpAsDevMsg( pKV, 1 );

		const char* pMapName = NULL;
		const char* pMapNameFromKV = pKV->GetString( "game/map" );
		const char* pGameType = pKV->GetString( "game/type" );
		const char* pGameMode = pKV->GetString( "game/mode" );
		const char* pMapGroupName = pKV->GetString( "game/mapgroupname", NULL );
		const char* pMapGroupNameToValidate = NULL;		// pMapGroupName is ok to be NULL; this variable lets us easily use a non null pMapGroupName or gpGlobals->mapGroupName

		if ( !IsValveDS() &&
			pMapNameFromKV && StringHasPrefix( pMapNameFromKV, "workshop" ) &&
			pMapGroupName && Q_stristr( pMapGroupName, "@workshop" ) )
		{
			// A community server is getting reserved by a client for a workshop map,
			// retain our current workshop collection if we are hosting one to preserve
			// map rotation process
			pMapGroupName = engine->IsDedicatedServer() ? STRING( gpGlobals->mapGroupName ) : pMapGroupName;
		}

		if ( pMapGroupName && (pMapGroupName[0] != '\0') && !pMapNameFromKV )
		{
			// if we have a mapgroup name, then we don't care about any map name from the pKV and we just want the first map from the mapgroup
			pMapName = g_pGameTypes->GetRandomMap( pMapGroupName );
			pMapGroupNameToValidate = pMapGroupName;
		}
		else
		{
			pMapGroupNameToValidate = ( pMapGroupName && (pMapGroupName[0] != '\0') ) ? pMapGroupName : STRING( gpGlobals->mapGroupName );
		}

		// make sure we are not using a bogus mapgroup name
		if ( pMapGroupNameToValidate && !StringIsEmpty( pMapGroupNameToValidate ) && !g_pGameTypes->IsValidMapGroupName( pMapGroupNameToValidate ) )
		{
			Warning( "ApplyGameSettings: Invalid mapgroup name %s\n", pMapGroupNameToValidate );
			return;
		}

		// only use the map name from the pKV if there was no mapgroup name in the pKV
		if ( !pMapName )
		{
			pMapName = pMapNameFromKV;
		}

		// For team games we add the prefix "team" to the game type. This is to
		// eliminate team game lobbies from searches for QuickMatch and Custom Match
		char *teamStr = "team";
		const char *pTeamPrefix = Q_strstr( pGameType, teamStr);
		if ( pTeamPrefix == pGameType )
		{
			pGameType += Q_strlen( teamStr );
		}

		if ( pMapName && pMapName[0] != '\0' )
		{
			// validate map exists in the mapgroup
			if ( !g_pGameTypes->IsValidMapInMapGroup( pMapGroupNameToValidate, pMapName ) )
			{
				Warning( "ApplyGameSettings: Map %s not part of Mapgroup %s\n", pMapName, pMapGroupNameToValidate );
			}

			int extraSpectators = 2;

			if ( ( pGameType && pGameType[0] != '\0' ) &&
				 ( pGameMode && pGameMode[0] != '\0' ) )
			{
				// make sure the mapgroup is in this game type & mode
				if ( !g_pGameTypes->IsValidMapGroupForTypeAndMode( pMapGroupNameToValidate, pGameType, pGameMode ) )
				{
					Warning( "ApplyGameSettings: MapGroup %s not part of type %s mode %s\n", pMapGroupNameToValidate, pGameType, pGameMode );
				}

				// Get the bot difficulty setting before it gets reverted.
				ConVarRef cvCustomBotDiff( "custom_bot_difficulty" );
				int customBotDiff = cvCustomBotDiff.GetInt();

/*
				// FIXME[pmf]: We don't want to reset all replicated convars unless we also re-exec game.cfg on the server,
				// otherwise we'll overwrite all the game configuration convars specified in game.cfg

				// Reset server enforced convars
				g_pCVar->RevertFlaggedConVars( FCVAR_REPLICATED );
*/

				// Cheats were disabled; revert all cheat cvars to their default values.
				// This must be done heading into multiplayer games because people can play
				// demos etc and set cheat cvars with sv_cheats 0.
				g_pCVar->RevertFlaggedConVars( FCVAR_CHEAT );

				// we know that the loading screen data is correct, so let the loading screen know
				//g_pGameTypes->SetLoadingScreenDataIsCorrect( true );
				g_pGameTypes->SetRunMapWithDefaultGametype( false );
				// Set game_type and game_mode convars.
				g_pGameTypes->SetGameTypeAndMode( pGameType, pGameMode );

				extern ConVar game_online;
				if ( char const *szOnline = pKV->GetString( "system/network", NULL ) )
				{
					game_online.SetValue( ( !V_stricmp( szOnline, "LIVE" ) ) ? 1 : 0 );
				}
				
				extern ConVar game_public;
				if ( char const *szAccess = pKV->GetString( "system/access", NULL ) )
				{
					game_public.SetValue( ( !V_stricmp( szAccess, "public" ) ) ? 1 : 0 );
				}

#ifndef CLIENT_DLL
				if ( engine->IsDedicatedServer() )
					game_online.SetValue( 1 );
#endif
				// Special case: set the custom bot difficulty for offline games
				if ( !game_online.GetBool() )
				{
					g_pGameTypes->SetCustomBotDifficulty( customBotDiff );
				}

				// Make sure that correct number of slots is set for the engine
				{
					int iType, iMode;
					if ( g_pGameTypes->GetGameModeAndTypeIntsFromStrings( pGameType, pGameMode, iType, iMode ) )
					{
						int iMaxPlayersForTypeMode = g_pGameTypes->GetMaxPlayersForTypeAndMode( iType, iMode );
						pKV->SetInt( "members/numSlots", iMaxPlayersForTypeMode );
					}
				}

				// Make sure the settings keys have extra spectator info
				pKV->SetInt( "members/numExtraSpectatorSlots", extraSpectators );
			}

			CFmtStr command;

			if ( pMapGroupName )
			{
				command.AppendFormat( "mapgroup %s\n", pMapGroupName );
			}
			command.AppendFormat( "nextlevel %s\n", pMapName ); // gamerules will clean it up when they construct for the next map
			command.AppendFormat( "map %s reserved\n", pMapName );
			
			Warning( "Executing server command:\n%s\n---\n", command.Access() );
			engine->ServerCommand( command );
			if ( engine->IsDedicatedServer() )
				engine->ServerExecute();
		}
	}
}

const char * CServerGameClients::ClientNameHandler( uint64 xuid, const char *pchName )
{
	CSteamID steamID( xuid );

	// In tournament mode force names for the players according to the reservation
	if ( steamID.IsValid() && steamID.BIndividualAccount() &&
		CCSGameRules::sm_QueuedServerReservation.has_tournament_event() )
	{
		for ( int32 iTeam = 0; iTeam < CCSGameRules::sm_QueuedServerReservation.tournament_teams().size(); ++iTeam )
		{
			TournamentTeam const &ttTeam = CCSGameRules::sm_QueuedServerReservation.tournament_teams( iTeam );
			for ( int32 iTeamPlayer = 0; iTeamPlayer < ttTeam.players().size(); ++iTeamPlayer )
			{
				TournamentPlayer const &ttPlayer = ttTeam.players( iTeamPlayer );
				if ( ttPlayer.account_id() && ( ttPlayer.account_id() == steamID.GetAccountID() ) )
				{
					return ( ttPlayer.player_nick().c_str() );
				}
			}
		}
	}

	// Throttle name changes from clients (hacked clients can set the name convar at any rate)
	extern CCSPlayer *ToCSPlayer( CBaseEntity *pEntity );
	if ( CCSPlayer* pPlayer = ToCSPlayer( CBasePlayer::GetPlayerBySteamID( steamID ) ) )
	{
		if ( !pPlayer->CanChangeName() )
			return pPlayer->GetPlayerName();
	}

	// Account not resolved, use whatever name they provided
	return pchName;
}

void CServerGameClients::ClientSvcUserMessage( edict_t *pEntity, int nType, int nPassthrough, uint32 cbSize, const void *pvBuffer )
{
	CCSPlayer *pPlayer = ToCSPlayer( GetContainingEntity( pEntity ) );
	if ( !pPlayer )
		return;

	switch ( nType )
	{
	case CS_UM_PlayerDecalDigitalSignature:
		{
			CCSUsrMsg_PlayerDecalDigitalSignature msg;
			if ( msg.ParseFromArray( pvBuffer, cbSize ) )
				pPlayer->SprayPaint( msg );
		}
		return;
	}
}
