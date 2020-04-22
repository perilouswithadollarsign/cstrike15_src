//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_title.h"
#include "mm_title_richpresence.h"
#include "portal2.spa.h"

#ifdef _PS3
#include <netex/net.h>
#include <netex/libnetctl.h>
#endif

#include "fmtstr.h"

#include "matchmaking/portal2/imatchext_portal2.h"
#include "matchmaking/mm_helpers.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CMatchTitle::CMatchTitle()
{
	;
}

CMatchTitle::~CMatchTitle()
{
	;
}


//
// Init / shutdown
//

InitReturnVal_t CMatchTitle::Init()
{
	if ( IGameEventManager2 *mgr = g_pMatchExtensions->GetIGameEventManager2() )
	{
		mgr->AddListener( this, "server_pre_shutdown", false );
		mgr->AddListener( this, "game_newmap", false );
		mgr->AddListener( this, "finale_start", false );
		mgr->AddListener( this, "round_start", false );
		mgr->AddListener( this, "round_end", false );
		mgr->AddListener( this, "difficulty_changed", false );
	}

#ifndef SWDS
	// Initialize Title Update version
	extern ConVar mm_tu_string;
	mm_tu_string.SetValue( "20110805" );
#endif

	return INIT_OK;
}

void CMatchTitle::Shutdown()
{
	if ( IGameEventManager2 *mgr = g_pMatchExtensions->GetIGameEventManager2() )
	{
		mgr->RemoveListener( this );
	}
}


//
// Implementation
//

uint64 CMatchTitle::GetTitleID()
{
#ifdef _X360
	#ifndef _DEMO
		return TITLEID_PORTAL_2_DISC_XBOX_360;
	#else
		return TITLEID_PORTAL_2_DEMO;
	#endif
#elif !defined( SWDS ) && !defined( NO_STEAM )
	static uint64 uiAppID = 0ull;
	if ( !uiAppID && steamapicontext && steamapicontext->SteamUtils() )
	{
		uiAppID = steamapicontext->SteamUtils()->GetAppID();
	}
	return uiAppID;
#else
	return 0ull;
#endif
}

uint64 CMatchTitle::GetTitleServiceID()
{
#ifdef _X360
	return 0x45410880ull; // Left 4 Dead 1 Service ID
#else
	return 0ull;
#endif
}

uint64 CMatchTitle::GetTitleSettingsFlags()
{
	return MATCHTITLE_SETTING_MULTIPLAYER
		| MATCHTITLE_SETTING_NODEDICATED
		| MATCHTITLE_PLAYERMGR_DISABLED
		| MATCHTITLE_SERVERMGR_DISABLED
		| MATCHTITLE_INVITE_ONLY_SINGLE_USER
	;
}

#ifdef _PS3
void *g_pMatchTitle_NetMemory;
#endif

void CMatchTitle::PrepareNetStartupParams( void *pNetStartupParams )
{
#ifdef _X360
	XNetStartupParams &xnsp = *( XNetStartupParams * ) pNetStartupParams;

	xnsp.cfgQosDataLimitDiv4 = 128; // 512 bytes
	xnsp.cfgSockDefaultRecvBufsizeInK = 64; // Increase receive size for UDP to 64k
	xnsp.cfgSockDefaultSendBufsizeInK = 64; // Keep send size at 64k too

	int numGamePlayersMax = GetTotalNumPlayersSupported();

	int numConnections = 4 * ( numGamePlayersMax - 1 );
	//   - the max number of connections to members of your game party
	//   - the max number of connections to members of your social party
	//   - the max number of connections to a pending game party (if you are joining a new one ).
	//   - matchmakings client info structure also creates a connection per client for the lobby.

	//   1 - the main game session
	int numTotalConnections = 1 + numConnections;

	//   29 - total Connections (XNADDR/XNKID pairs) ,using 5 sessions (XNKID/XNKEY pairs).

	xnsp.cfgKeyRegMax = 16; //adding some extra room because of lazy dealocation of these pairs.
	xnsp.cfgSecRegMax = MAX( 64, numTotalConnections ); //adding some extra room because of lazy dealocation of these pairs.
	
	xnsp.cfgSockMaxDgramSockets = xnsp.cfgSecRegMax;
	xnsp.cfgSockMaxStreamSockets = xnsp.cfgSecRegMax;
#endif

#if defined( _PS3 ) && defined( NO_STEAM )
	MEM_ALLOC_CREDIT_( "NO_STEAM: CMatchTitle::PrepareNetStartupParams" );
	sys_net_initialize_parameter_t &snip = *( sys_net_initialize_parameter_t * ) pNetStartupParams;

	snip.memory_size = 512 * 1024;
	snip.memory = malloc( snip.memory_size ); // alternatively this can be a global array

	g_pMatchTitle_NetMemory = snip.memory;	// bookmark the memory address for later inspection if necessary
#endif
}

int CMatchTitle::GetTotalNumPlayersSupported()
{
	// Portal 2 is a 2-player game
	return 2;
}

// Get a guest player name
char const * CMatchTitle::GetGuestPlayerName( int iUserIndex )
{
	if ( vgui::IVGUILocalize *pLocalize = g_pMatchExtensions->GetILocalize() )
	{
		if ( wchar_t* wStringTableEntry = pLocalize->Find( "#L4D360UI_Character_Guest" ) )
		{
			static char szName[ MAX_PLAYER_NAME_LENGTH ] = {0};
			pLocalize->ConvertUnicodeToANSI( wStringTableEntry, szName, ARRAYSIZE( szName ) );
			return szName;
		}
	}
	
	return "";
}

// Sets up all necessary client-side convars and user info before
// connecting to server
void CMatchTitle::PrepareClientForConnect( KeyValues *pSettings )
{
#ifndef SWDS
	int numPlayers = 1;
#ifdef _GAMECONSOLE
	numPlayers = XBX_GetNumGameUsers();
#endif

	//
	// Now we set the convars
	//

	for ( int k = 0; k < numPlayers; ++ k )
	{
		int iController = k;
#ifdef _GAMECONSOLE
		iController = XBX_GetUserId( k );
#endif
		IPlayerLocal *pPlayerLocal = g_pPlayerManager->GetLocalPlayer( iController );
		if ( !pPlayerLocal )
			continue;

		// Set "name"
		static SplitScreenConVarRef s_cl_name( "name" );
		char const *szName = pPlayerLocal->GetName();
		s_cl_name.SetValue( k, szName );

		// Set "networkid_force"
		if ( IsX360() )
		{
			static SplitScreenConVarRef s_networkid_force( "networkid_force" );
			uint64 xid = pPlayerLocal->GetXUID();
			s_networkid_force.SetValue( k, CFmtStr( "%08X:%08X:", uint32( xid >> 32 ), uint32( xid ) ) );
		}
	}
#endif
}

bool CMatchTitle::StartServerMap( KeyValues *pSettings )
{
	int numPlayers = 1;
#ifdef _GAMECONSOLE
	numPlayers = XBX_GetNumGameUsers();
#endif

	char const *szMap = pSettings->GetString( "game/map", NULL );
	if ( !szMap )
		return false;

	// Check that we have the server interface and that the map is valid
	if ( !g_pMatchExtensions->GetIVEngineServer() )
		return false;
	if ( !g_pMatchExtensions->GetIVEngineServer()->IsMapValid( szMap ) )
		return false;

	//
	// Prepare game dll reservation package
	//
	KeyValues *pGameDllReserve = g_pMatchFramework->GetMatchNetworkMsgController()->PackageGameDetailsForReservation( pSettings );
	KeyValues::AutoDelete autodelete( pGameDllReserve );

	pGameDllReserve->SetString( "map/mapcommand", ( numPlayers <= 1 ) ? "map" : "ss_map" );

	char const *szPlayOptions = pSettings->GetString( "options/play", "" );
	if ( !Q_stricmp( "commentary", szPlayOptions ) )
	{
		pGameDllReserve->SetString( "map/mapcommand", "map_commentary" );
	}
	else if ( !Q_stricmp( "challenge", szPlayOptions ) )
	{
		pGameDllReserve->SetString( "options/play", "challenge" );
	}

	// Run map based off the faked reservation packet
	g_pMatchExtensions->GetIServerGameDLL()->ApplyGameSettings( pGameDllReserve );

	return true;
}

void CMatchTitle::RunFrame()
{
	IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
	if ( !pIMatchSession )
		return;

	if ( pIMatchSession->GetSessionSettings()->GetBool( "game/sv_cheats" ) )
		return; // already flagged as cheats session

	// capture either currently set or has been set during this session
	static ConVarRef ref_sv_cheats_flagged( "sv_cheats_flagged" );
	if ( ( ref_sv_cheats_flagged.IsValid() && !ref_sv_cheats_flagged.GetBool() ) || !g_pMatchExtensions->GetIVEngineClient()->IsConnected() )
		return;

	// Bypassing session update rules, each client can flag sv_cheats
	// separately once they see it before server session sees sv_cheats
	pIMatchSession->GetSessionSettings()->SetInt( "game/sv_cheats", 1 );
}

static KeyValues * GetCurrentMatchSessionSettings()
{
	IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
	return pIMatchSession ? pIMatchSession->GetSessionSettings() : NULL;
}

#ifndef SWDS
static void SendPreConnectClientDataToServer( int nSlot )
{
	// We have just connected to the server,
	// send our avatar information

	int iController = nSlot;
#ifdef _GAMECONSOLE
	iController = XBX_GetUserId( nSlot );
#endif

	// Portal 2 for now has no preconnect data
	if ( 1 )
		return;

	KeyValues *pPreConnectData = new KeyValues( "preconnectdata" );

	//
	// Now we prep the keyvalues for server
	//
	if ( IPlayerLocal *pPlayerLocal = g_pPlayerManager->GetLocalPlayer( iController ) )
	{
		// Set session-specific user info
		XUID xuid = pPlayerLocal->GetXUID();
		pPreConnectData->SetUint64( "xuid", xuid );

		KeyValues *pSettings = GetCurrentMatchSessionSettings();

		KeyValues *pMachine = NULL;
		if ( KeyValues *pPlayer = SessionMembersFindPlayer( pSettings, xuid, &pMachine ) )
		{
		}
	}

	// Deliver the keyvalues to the server
	int nRestoreSlot = g_pMatchExtensions->GetIVEngineClient()->GetActiveSplitScreenPlayerSlot();
	g_pMatchExtensions->GetIVEngineClient()->SetActiveSplitScreenPlayerSlot( nSlot );
	g_pMatchExtensions->GetIVEngineClient()->ServerCmdKeyValues( pPreConnectData );
	g_pMatchExtensions->GetIVEngineClient()->SetActiveSplitScreenPlayerSlot( nRestoreSlot );
}

static bool MatchSessionIsSinglePlayerOnline( char const *szGameType )
{
	if ( XBX_GetNumGameUsers() != 1 )
		return false; // not playing with a single committed profile
	IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
	if ( !pIMatchSession )
		return false; // don't have a valid session
	KeyValues *pSettings = pIMatchSession->GetSessionSettings();
	if ( Q_stricmp( pSettings->GetString( "system/network" ), "LIVE" ) )
		return false; // session is not online
	if ( szGameType )
	{
		if ( Q_stricmp( pSettings->GetString( "game/type" ), szGameType ) )
			return false; // session is not correct game type
	}
	return true;
}

static bool MatchSessionPlayersAreFriends( int iLocalController, XUID xuidPartner )
{
	if ( !xuidPartner )
		return false;
#ifdef _X360
	BOOL bFriend = FALSE;
	if ( ERROR_SUCCESS != XUserAreUsersFriends( iLocalController, &xuidPartner, 1, &bFriend, NULL ) )
		return false;
	if ( !bFriend )
		return false;
#else
#ifndef NO_STEAM
	if ( !steamapicontext->SteamFriends()->HasFriend( xuidPartner, /*k_EFriendFlagImmediate*/ 0x04 ) )
#endif
		return false;
#endif
	return true;
}

static void MatchSessionUpdateSinglePlayerProgress( char const *szMap )
{
	if ( XBX_GetNumGameUsers() != 1 )
		return;

	IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
	if ( !pIMatchSession )
		return;

	KeyValues *pSettings = pIMatchSession->GetSessionSettings();
	if ( Q_stricmp( pSettings->GetString( "system/network" ), "offline" ) )
		return;
	if ( Q_stricmp( pSettings->GetString( "game/mode" ), "sp" ) )
		return;

	// Ok, we've got a single player offline session with one user
	IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetPrimaryUserId() );
	if ( !pPlayer )
		return;

	static ContextValue_t s_SP_MAP_2_PROGRESS[] = {
#define CFG( spmapname, chapternum, subchapter ) { #spmapname, chapternum },
#include "inc_sp_maps.inc"
#undef CFG
		{ NULL, 0 },
	};
	uint32 uiChapterNum = s_SP_MAP_2_PROGRESS->ScanValues( szMap );
	if ( !uiChapterNum )
		return;

	// Locate the single player progress field
	TitleDataFieldsDescription_t const *fields = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();
	fields = TitleDataFieldsDescriptionFindByString( fields, "SP.progress" );
	if ( !fields )
		return;

	uint32 uiCurrentlyMaxChapter = TitleDataFieldsDescriptionGetValue<uint32>( fields, pPlayer );
	if ( uiChapterNum <= uiCurrentlyMaxChapter )
		return;

	// Update the single player progress
	TitleDataFieldsDescriptionSetValue<uint32>( fields, pPlayer, uiChapterNum );
}

template< typename T >
static bool MatchSessionSetAchievementBasedOnComponents( T valComponent, char const *szAchievement, int numComponentFields, IPlayerLocal *pPlayerLocal )
{
	TitleDataFieldsDescription_t const *field1 = TitleDataFieldsDescriptionFindByString(
		g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage(), CFmtStr( "%s[1]", szAchievement ) );
	if ( !field1 )
		return false;
	int iComponent = 0;
	for ( ; iComponent < numComponentFields; ++ iComponent, ++ field1 )
	{
		T valSlot = TitleDataFieldsDescriptionGetValue<T>( field1, pPlayerLocal );
		if ( valSlot == valComponent )
			return false; // already have such component
		if ( !valSlot )
		{
			TitleDataFieldsDescriptionSetValue<T>( field1, pPlayerLocal, valComponent );
			++ iComponent;
			break;
		}
	}
	if ( iComponent < numComponentFields )
		return false; // not enough components met yet

	// Awesome, we are eligible for achievement
	{
		KeyValues *kvAwardAch = new KeyValues( "" );
		KeyValues::AutoDelete autodelete_kvAwardAch( kvAwardAch );
		kvAwardAch->SetInt( szAchievement, 1 );
		pPlayerLocal->UpdateAwardsData( kvAwardAch );
	}

	return true;
}
#endif

void CMatchTitle::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();
	
	if ( !Q_stricmp( "OnPlayerRemoved", szEvent ) ||
		 !Q_stricmp( "OnPlayerUpdated", szEvent ) )
	{
		MM_Title_RichPresence_PlayersChanged( GetCurrentMatchSessionSettings() );
	}
	else if ( !Q_stricmp( "OnPlayerMachinesConnected", szEvent ) ||
			  !Q_stricmp( "OnPlayerMachinesDisconnected", szEvent ) )
	{
		// Player counts changed on host, update aggregate fields
		IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
		if ( !pMatchSession )
			return;
		KeyValues *kvPackage = new KeyValues( "Update" );
		if ( KeyValues *kvUpdate = kvPackage->FindKey( "update", true ) )
		{
			void UpdateAggregateMembersSettings( KeyValues *pFullGameSettings, KeyValues *pUpdate );
			UpdateAggregateMembersSettings( pMatchSession->GetSessionSettings(), kvUpdate );
		}
		pMatchSession->UpdateSessionSettings( KeyValues::AutoDeleteInline( kvPackage ) );
	}
	else if ( !Q_stricmp( "OnProfileDataSaved", szEvent ) )
	{
		// Player profile data updated, recompute skills
		IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
		if ( !pMatchSession )
			return;

		IPlayerLocal *player = g_pPlayerManager->GetLocalPlayer( pEvent->GetInt( "iController" ) );
		if ( !player )
			return;

		// Initialize member settings on a temporary copy of session settings
		KeyValues *kvPlayerUpdate = pMatchSession->GetSessionSettings()->MakeCopy();
		KeyValues::AutoDelete autodelete_kvPlayerUpdate( kvPlayerUpdate );

		void InitializeMemberSettings( KeyValues *pSettings );
		InitializeMemberSettings( kvPlayerUpdate );

		// Find the updated player info
		KeyValues *pPlayerData = SessionMembersFindPlayer( kvPlayerUpdate, player->GetXUID() );
		if ( !pPlayerData || !pPlayerData->FindKey( "game" ) )
			return;

		// Send the request to the host to update our player info
		KeyValues *pRequest = new KeyValues( "Game::PlayerInfo" );
		KeyValues::AutoDelete autodelete( pRequest );
		pRequest->SetString( "run", "host" );
		pRequest->SetUint64( "xuid", player->GetXUID() );
		pRequest->AddSubKey( pPlayerData->FindKey( "game" )->MakeCopy() );
		pMatchSession->Command( pRequest );
	}
	else if ( !Q_stricmp( "OnMatchSessionUpdate", szEvent ) )
	{
		if ( !Q_stricmp( pEvent->GetString( "state" ), "updated" ) )
		{
			if ( KeyValues *kvUpdate = pEvent->FindKey( "update" ) )
			{
				MM_Title_RichPresence_Update( GetCurrentMatchSessionSettings(), kvUpdate );
			}
		}
		else if ( !Q_stricmp( pEvent->GetString( "state" ), "created" ) ||
				  !Q_stricmp( pEvent->GetString( "state" ), "ready" ) )
		{
			MM_Title_RichPresence_Update( GetCurrentMatchSessionSettings(), NULL );
		}
		else if ( !Q_stricmp( pEvent->GetString( "state" ), "closed" ) )
		{
			MM_Title_RichPresence_Update( NULL, NULL );
		}
	}
#ifndef SWDS
	else if ( !Q_stricmp( "OnEngineClientSignonStateChange", szEvent ) )
	{
		int nSlot = pEvent->GetInt( "slot" );
		int iOldState = pEvent->GetInt( "old" );
		int iNewState = pEvent->GetInt( "new" );

		if ( iOldState < SIGNONSTATE_CONNECTED &&
			 iNewState >= SIGNONSTATE_CONNECTED )
		{
			SendPreConnectClientDataToServer( nSlot );
		}
	}
	else if ( !Q_stricmp( "OnEngineSplitscreenClientAdded", szEvent ) )
	{
		int nSlot = pEvent->GetInt( "slot" );
		SendPreConnectClientDataToServer( nSlot );
	}
	else if ( !Q_stricmp( szEvent, "OnPlayerAward" ) )
	{
		int iCtrlr = pEvent->GetInt( "iController" );
		IPlayerLocal *pPlayerLocal = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iCtrlr );
		if ( !pPlayerLocal )
			return;

		char const *szAward = pEvent->GetString( "award" );

		// ACH.TEACHER implementation is based off ACH.HI_FIVE_YOUR_PARTNER
		if ( !Q_stricmp( szAward, "ACH.HI_FIVE_YOUR_PARTNER" ) )
		{
			// We are being awarded a calibration success achievement
			// check that we are in an online session, that we haven't
			// yet completed any maps and haven't gotten any other
			// achievements except for ACH.HI_FIVE_YOUR_PARTNER that we
			// have just been awarded
			if ( !MatchSessionIsSinglePlayerOnline( "friends" ) )
				return;
			TitleData1 const *td1 = ( TitleData1 const * ) pPlayerLocal->GetPlayerTitleData( TitleDataFieldsDescription_t::DB_TD1 );
			if ( !td1 )
				return; // failed to get TD1
			else
			{
				TitleData1 td1copy = *td1;
				// we don't care whether the newbie played mp_coop_lobby_2 map
				uint8 *pBits = ( uint8 * ) td1copy.coop.mapbits;
				COMPILE_TIME_ASSERT( TitleData1::CoopData_t::mp_coop_lobby_2 < 8*sizeof( uint8 ) );
				pBits[0] &=~ ( 1 << TitleData1::CoopData_t::mp_coop_lobby_2 );
				// compare our newbie mapbits with all zeroes
				uint32 allzeroes[ ARRAYSIZE( td1copy.coop.mapbits ) ];
				COMPILE_TIME_ASSERT( sizeof( allzeroes ) == sizeof( td1copy.coop.mapbits ) );
				Q_memset( allzeroes, 0, sizeof( allzeroes ) );
				if ( Q_memcmp( allzeroes, td1copy.coop.mapbits, sizeof( allzeroes ) ) )
					return; // we aren't a newbie, played some other maps already
			}
			
			// We are a newbie who just received the achievement, let our potential teacher know
			int nActiveSlot = g_pMatchExtensions->GetIVEngineClient()->GetActiveSplitScreenPlayerSlot();
			g_pMatchExtensions->GetIVEngineClient()->SetActiveSplitScreenPlayerSlot( XBX_GetSlotByUserId( iCtrlr ) );
			KeyValues *pServerEvent = new KeyValues( "OnPlayerAward" );
			pServerEvent->SetString( "award", "ACH.HI_FIVE_YOUR_PARTNER" );
			pServerEvent->SetUint64( "xuid", pPlayerLocal->GetXUID() );
#if defined( _PS3 ) && !defined( NO_STEAM )
			pServerEvent->SetUint64( "psnid", steamapicontext->SteamUser()->GetConsoleSteamID().ConvertToUint64() );
#endif
			pServerEvent->SetInt( "newbie", 1 );
			g_pMatchExtensions->GetIVEngineClient()->ServerCmdKeyValues( pServerEvent );
			g_pMatchExtensions->GetIVEngineClient()->SetActiveSplitScreenPlayerSlot( nActiveSlot );
		}
		else if ( !Q_stricmp( szAward, "ACH.SHOOT_THE_MOON" ) )
		{
			// Unlock player progress towards the credits map
			MatchSessionUpdateSinglePlayerProgress( "sp_a5_credits" );
		}
	}
	else if ( !Q_stricmp( szEvent, "Client::CmdKeyValues" ) )
	{
		KeyValues *pCmd = pEvent->GetFirstTrueSubKey();
		if ( !pCmd )
			return;

		int nSlot = pEvent->GetInt( "slot" );
		int iCtrlr = XBX_GetUserId( nSlot );
		IPlayerLocal *pPlayerLocal = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iCtrlr );
		if ( !pPlayerLocal )
			return;

		char const *szCmd = pCmd->GetName();
		if ( !Q_stricmp( "OnPlayerAward", szCmd ) )
		{
			//
			// ACH.TEACHER implementation
			//
			// our partner received an award
			if ( !Q_stricmp( pCmd->GetString( "award" ), "ACH.HI_FIVE_YOUR_PARTNER" ) )
			{
				if ( pCmd->GetInt( "newbie" ) != 1 )
					return;
				// Our newbie partner is being awarded a calibration success achievement
				// check that we are in an online session and that we have
				// already completed all maps
				if ( !MatchSessionIsSinglePlayerOnline( "friends" ) )
					return;
				TitleData1 const *td1 = ( TitleData1 const * ) pPlayerLocal->GetPlayerTitleData( TitleDataFieldsDescription_t::DB_TD1 );
				if ( !td1 )
					return; // failed to get TD1
				
				{
					// We must have the HiFive and NewBlood achievements
					KeyValues *kvAchievementsEarned = new KeyValues( "" );
					KeyValues::AutoDelete autodelete_kvAchievementsEarned( kvAchievementsEarned );
					kvAchievementsEarned->SetInt( "ACH.HI_FIVE_YOUR_PARTNER", 0 );
					kvAchievementsEarned->SetInt( "ACH.NEW_BLOOD", 0 );
					pPlayerLocal->GetAwardsData( kvAchievementsEarned );
					if ( !kvAchievementsEarned->GetInt( "ACH.HI_FIVE_YOUR_PARTNER" ) )
						return;
					if ( !kvAchievementsEarned->GetInt( "ACH.NEW_BLOOD" ) )
						return;
				}

				{
					// we must have completed all coop maps
					uint8 *pBits = ( uint8 * ) td1->coop.mapbits;
					for ( int k = 0; k < TitleData1::CoopData_t::mapbits_total_basegame; ++ k )
					{
						if ( k != TitleData1::CoopData_t::mp_coop_start && k != TitleData1::CoopData_t::mp_coop_lobby_2 && k != TitleData1::CoopData_t::mp_coop_credits )
						{
							if ( !( pBits[ k/8 ] & ( 1 << (k%8) ) ) )
								return;
						}
					}
				}

				// We must be friends with this newbie
#if defined( _PS3 ) && !defined( NO_STEAM )
				if ( !MatchSessionPlayersAreFriends( pPlayerLocal->GetPlayerIndex(), pCmd->GetUint64( "psnid" ) ) )
#endif
				if ( !MatchSessionPlayersAreFriends( pPlayerLocal->GetPlayerIndex(), pCmd->GetUint64( "xuid" ) ) )
					return;

				// Awesome, we are eligible for ACH.TEACHER
				{
					KeyValues *kvAwardTeacher = new KeyValues( "" );
					KeyValues::AutoDelete autodelete_kvAwardTeacher( kvAwardTeacher );
					kvAwardTeacher->SetInt( "ACH.TEACHER", 1 );
					pPlayerLocal->UpdateAwardsData( kvAwardTeacher );
				}
			}
			//
			// ACH.SPREAD_THE_LOVE implementation
			//
			else if ( !Q_stricmp( pCmd->GetString( "award" ), "ACH.SPREAD_THE_LOVE" ) )
			{
				if ( pCmd->GetInt( "hugged" ) != 1 )
					return;
				if ( !MatchSessionIsSinglePlayerOnline( "friends" ) )
					return;
				// We must be friends with the person we've hugged
				XUID xuidHugged = pCmd->GetUint64( "xuid" );
				if ( xuidHugged == pPlayerLocal->GetXUID() )
					return;
#if defined( _PS3 ) && !defined( NO_STEAM )
				if ( !MatchSessionPlayersAreFriends( pPlayerLocal->GetPlayerIndex(), pCmd->GetUint64( "psnid" ) ) )
#endif
				if ( !MatchSessionPlayersAreFriends( pPlayerLocal->GetPlayerIndex(), xuidHugged ) )
					return;

				// Set achievement component
				bool bAchieved = MatchSessionSetAchievementBasedOnComponents( xuidHugged, "ACH.SPREAD_THE_LOVE",
					TitleData2::kAchievement_SpreadTheLove_FriendsHuggedCount, pPlayerLocal );

				if ( bAchieved )
				{
					KeyValues *kvAwardShirt2 = new KeyValues( "" );
					KeyValues::AutoDelete autodelete_kvAwardShirt2( kvAwardShirt2 );
					kvAwardShirt2->SetInt( "AV_SHIRT2", 1 );
					pPlayerLocal->UpdateAwardsData( kvAwardShirt2 );
				}
			}
		}
		else if ( !Q_stricmp( "OnCoopBotTaunt", szCmd ) )
		{
			if ( !Q_stricmp( "teamhug", pCmd->GetString( "taunt" ) ) )
			{
				if ( !MatchSessionIsSinglePlayerOnline( "friends" ) )
					return;

				// A hug has happened, let's spread the love
				int nActiveSlot = g_pMatchExtensions->GetIVEngineClient()->GetActiveSplitScreenPlayerSlot();
				g_pMatchExtensions->GetIVEngineClient()->SetActiveSplitScreenPlayerSlot( XBX_GetSlotByUserId( iCtrlr ) );
				KeyValues *pServerEvent = new KeyValues( "OnPlayerAward" );
				pServerEvent->SetString( "award", "ACH.SPREAD_THE_LOVE" );
				pServerEvent->SetUint64( "xuid", pPlayerLocal->GetXUID() );
#if defined( _PS3 ) && !defined( NO_STEAM )
				pServerEvent->SetUint64( "psnid", steamapicontext->SteamUser()->GetConsoleSteamID().ConvertToUint64() );
#endif
				pServerEvent->SetInt( "hugged", 1 );
				g_pMatchExtensions->GetIVEngineClient()->ServerCmdKeyValues( pServerEvent );
				g_pMatchExtensions->GetIVEngineClient()->SetActiveSplitScreenPlayerSlot( nActiveSlot );
			}
		}
		else if ( !Q_stricmp( "OnSpeedRunCoopEvent", szCmd ) )
		{
			//
			// A qualifying speedrun has happened
			//
			// Determine the map context
			char const *szMapSpeedRun = pCmd->GetString( "map" );
			if ( !szMapSpeedRun || !*szMapSpeedRun )
				return;
			TitleDataFieldsDescription_t const *fieldMapComplete = TitleDataFieldsDescriptionFindByString( DescribeTitleDataStorage(), CFmtStr( "MP.complete.%s", szMapSpeedRun ) );
			if ( !fieldMapComplete )
				return;
			uint16 uiMapCompleteId = (uint16)(uint32) fieldMapComplete->m_numBytesOffset;
			
			// Set achievement component
			MatchSessionSetAchievementBasedOnComponents<uint16>( uiMapCompleteId, "ACH.SPEED_RUN_COOP",
				TitleData2::kAchievement_SpeedRunCoop_QualifiedRunsCount, pPlayerLocal );
		}
	}
	else if ( !Q_stricmp( szEvent, "OnDowloadableContentInstalled" ) )
	{
		if ( !IsX360() )
		{
			IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
			if ( !pIMatchSession )
				return;

			IPlayerLocal *playerPrimary = g_pPlayerManager->GetLocalPlayer( XBX_GetPrimaryUserId() );
			if ( !playerPrimary )
				return;

			//
			// Find local machine in session settings
			//
			KeyValues *pSettings = pIMatchSession->GetSessionSettings();
			KeyValues *pMachine = NULL;
			KeyValues *pPlayer = SessionMembersFindPlayer( pSettings, playerPrimary->GetXUID(), &pMachine );
			pPlayer;
			if ( !pMachine )
				return;

			// Initialize DLC for the machine
			extern void InitializeDlcMachineSettings( KeyValues *pSettings, KeyValues *pMachine );
			InitializeDlcMachineSettings( pSettings, pMachine );
		}
	}
#endif
}

//
//
//

int CMatchTitle::GetEventDebugID( void )
{
	return EVENT_DEBUG_ID_INIT;
}

void CMatchTitle::FireGameEvent( IGameEvent *pIGameEvent )
{
#ifndef SWDS
	// Check if the current match session is on an active game server
	IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
	if ( !pMatchSession )
		return;
	KeyValues *pSessionSettings = pMatchSession->GetSessionSettings();
	char const *szGameServer = pSessionSettings->GetString( "server/server", "" );
	char const *szSystemLock = pSessionSettings->GetString( "system/lock", "" );
	if ( ( !szGameServer || !*szGameServer ) &&
		( !szSystemLock || !*szSystemLock ) )
		return;

	// Also don't run on the client when there's a host
	char const *szSessionType = pMatchSession->GetSessionSystemData()->GetString( "type", NULL );
	if ( szSessionType && !Q_stricmp( szSessionType, "client" ) )
		return;

	// Parse the game event
	char const *szGameEvent = pIGameEvent->GetName();
	if ( !szGameEvent || !*szGameEvent )
		return;

	if ( !Q_stricmp( "round_start", szGameEvent ) )
	{
		pMatchSession->UpdateSessionSettings( KeyValues::AutoDeleteInline( KeyValues::FromString(
			"update",
			" update { "
				" game { "
					" state game "
				" } "
			" } "
			) ) );
	}
	else if ( !Q_stricmp( "round_end", szGameEvent ) )
	{
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnProfilesWriteOpportunity", "reason", "checkpoint"
			) );
	}
	else if ( !Q_stricmp( "finale_start", szGameEvent ) )
	{
		pMatchSession->UpdateSessionSettings( KeyValues::AutoDeleteInline( KeyValues::FromString(
			"update",
			" update { "
				" game { "
					" state finale "
				" } "
			" } "
			) ) );
	}
	else if ( !Q_stricmp( "game_newmap", szGameEvent ) )
	{
		const char *szMapName = pIGameEvent->GetString( "mapname", "" );

		KeyValues *kvUpdate = KeyValues::FromString(
			"update",
			" update { "
				" game { "
					" state game "
				" } "
			" } "
			);
		KeyValues::AutoDelete autodelete( kvUpdate );

		Assert( szMapName && *szMapName );
		if ( szMapName && *szMapName )
		{
			kvUpdate->SetString( "update/game/map", szMapName );

			MatchSessionUpdateSinglePlayerProgress( szMapName );

			if ( !pSessionSettings->GetString( "game/type", NULL ) )
			{
				bool bFriends = false;
				if ( MatchSessionIsSinglePlayerOnline( NULL ) )
				{
					IPlayerLocal *pPlayerLocal = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetPrimaryUserId() );
					if ( pPlayerLocal )
					{
						XUID xuidLocal = pPlayerLocal->GetXUID();
						int numSessionFriends = 0, numSessionPlayers = 0;
						for ( int iMachine = 0, numMachines = pSessionSettings->GetInt( "members/numMachines" ); iMachine < numMachines; ++ iMachine )
						{
							KeyValues *pMachine = pSessionSettings->FindKey( CFmtStr( "members/machine%d", iMachine ) );
							for ( int iPlayer = 0, numPlayers = pMachine->GetInt( "numPlayers" ); iPlayer < numPlayers; ++ iPlayer )
							{
								KeyValues *pPlayer = pMachine->FindKey( CFmtStr( "player%d", iPlayer ) );
								if ( !pPlayer )
									continue;

								++ numSessionPlayers;
								XUID xuidPlayer = pPlayer->GetUint64( "xuid" );
								if ( xuidPlayer == xuidLocal )
									continue;

								if ( MatchSessionPlayersAreFriends( pPlayerLocal->GetPlayerIndex(), xuidPlayer ) )
								{
									++ numSessionFriends;
								}
#if defined( _PS3 ) && !defined( NO_STEAM )
								else if ( MatchSessionPlayersAreFriends( pPlayerLocal->GetPlayerIndex(), pMachine->GetUint64( "psnid" ) ) )
								{
									++ numSessionFriends;
								}
#endif
							}
						}
						if ( numSessionFriends && numSessionPlayers && ( numSessionFriends + 1 == numSessionPlayers ) )
							bFriends = true;
					}
				}
				kvUpdate->SetString( "update/game/type", bFriends ? "friends" : "default" );
			}
		}

		pMatchSession->UpdateSessionSettings( kvUpdate );

		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnProfilesWriteOpportunity", "reason", "checkpoint"
			) );
	}
	else if ( !Q_stricmp( "server_pre_shutdown", szGameEvent ) )
	{
		char const *szReason = pIGameEvent->GetString( "reason", "quit" );
		if ( !Q_stricmp( szReason, "quit" ) )
		{
			DevMsg( "Received server_pre_shutdown notification - server is shutting down...\n" );

			// Transform the server shutdown event into game end event
			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
				"OnEngineDisconnectReason", "reason", "Server shutting down"
				) );
		}
	}
#endif
}

