//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"
#ifdef _WIN32
#include "winerror.h"
#endif
#include "achievementmgr.h"
#include "icommandline.h"
#include "keyvalues.h"
#include "filesystem.h"
#include "inputsystem/InputEnums.h"
#include "usermessages.h"
#include "fmtstr.h"
#ifdef CLIENT_DLL
#include "achievement_notification_panel.h"
#include "c_playerresource.h"
#include "c_cs_player.h"
#ifdef TF_CLIENT_DLL
#include "item_inventory.h"
#endif //TF_CLIENT_DLL
#else
#include "enginecallback.h"
#endif // CLIENT_DLL

#ifndef NO_STEAM
#include "steam/isteamuserstats.h"
#include "steam/isteamfriends.h"
#include "steam/isteamutils.h"
#endif
#include "cs_gamerules.h"
#include "tier3/tier3.h"
#include "vgui/ILocalize.h"

#include "matchmaking/mm_helpers.h"

#ifdef _X360
#include "xbox/xbox_win32stubs.h"
#endif

#ifdef _PS3
#include "ps3/ps3_core.h"
#include "ps3/ps3_win32stubs.h"
#endif

#ifdef _GAMECONSOLE
#include "gameui/igameui.h"
#include "ixboxsystem.h"
#include "ienginevgui.h"
#endif  // _GAMECONSOLE

#include "matchmaking/imatchframework.h"
#include "tier0/vprof.h"
#include "cs_weapon_parse.h"
#include "achievements_cs.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

ConVar	cc_achievement_debug("achievement_debug", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "Turn on achievement debug msgs." );
const char *COM_GetModDirectory();

// We want to debug getting achievements in various development scenarios (including with cheats enabled). This convar allows bypassing
// the normal check against the cheats when launched with -dev. We need to be very careful not to ship with this enabled on any platforms.
// #if defined(_CERT)
#define ALLOW_ACHIEVEMENTS_WITH_CHEATS 0
// #else
// #define ALLOW_ACHIEVEMENTS_WITH_CHEATS 1
// #endif

extern ConVar developer;

#ifdef DEDICATED
// Hack this for now until we get steam_api recompiling in the Steam codebase.
ISteamUserStats *SteamUserStats()
{
	return NULL;
}
#endif

#if defined( XBX_GetPrimaryUserId )
#undef XBX_GetPrimaryUserId
#endif

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

static int AchievementIDCompare( CBaseAchievement * const *ach1, CBaseAchievement * const *ach2 )
{
	return (*ach1)->GetAchievementID() > (*ach2)->GetAchievementID();
}

static int AchievementOrderCompare( CBaseAchievement * const *ach1, CBaseAchievement * const *ach2 )
{
	return (*ach1)->GetDisplayOrder() - (*ach2)->GetDisplayOrder();
}

#ifdef _X360
static TitleAchievementsDescription_t const * FindTitleAchievementByName( TitleAchievementsDescription_t const *pMap, char const *szName )
{
	while ( pMap && pMap->m_szAchievementName )
		if ( !Q_stricmp( pMap->m_szAchievementName, szName ) )
			return pMap;
		else
			++ pMap;

	return NULL;
}
static TitleAchievementsDescription_t const * FindTitleAchievementById( TitleAchievementsDescription_t const *pMap, int id )
{
	while ( pMap && pMap->m_szAchievementName )
		if ( pMap->m_idAchievement == id )
			return pMap;
		else
			++ pMap;

	return NULL;
}
#endif

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CAchievementMgr::CAchievementMgr() : CAutoGameSystemPerFrame( "CAchievementMgr" )
#if !defined(NO_STEAM)
, m_CallbackUserStatsStored( this, &CAchievementMgr::Steam_OnUserStatsStored )
#endif
{
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		SetDefLessFunc( m_mapAchievement[i] );
		m_flLastClassChangeTime[i] = 0;
		m_flTeamplayStartTime[i] = 0;
		m_iMiniroundsCompleted[i] = 0;
		m_bDirty[i] = false;

		m_AchievementsAwarded[i].RemoveAll();
		m_AchievementsAwardedDuringCurrentGame[i].RemoveAll();
		m_bUserSlotActive[i] = false;
	}

	m_szMap[0] = 0;
	m_bCheatsEverOn = false;
	m_flTimeLastUpload = 0;

	m_bCheckSigninState = true;
	m_bReadingFromTitleData = false;

#ifdef _X360
	// Mark that we're not waiting for an async call to finish
	m_pendingAchievementState.Purge();
#endif // _X360
}

//#if defined (_X360)
void CAchievementMgr::ResetProfileInfo()
{
	m_UIProfileInfo.m_BotDifficulty = 0;
	m_UIProfileInfo.m_GameMode = 0;
	m_UIProfileInfo.m_IsPublic = true;
	m_UIProfileInfo.m_Map = 0;

	m_UIProfileInfo.m_LeaderboardFilter = 0;
	m_UIProfileInfo.m_LeaderboardMode = 0;
	m_UIProfileInfo.m_LeaderboardType = 0;
}
//#endif


//-----------------------------------------------------------------------------
// Purpose: Initializer
//-----------------------------------------------------------------------------
bool CAchievementMgr::Init()
{
	// We can be created on either client (for multiplayer games) or server
	// (for single player), so register ourselves with the engine so UI has a uniform place 
	// to go get the pointer to us

#ifdef _DEBUG
	// There can be only one achievement manager instance; no one else should be registered
	IAchievementMgr *pAchievementMgr = engine->GetAchievementMgr();
	Assert( NULL == pAchievementMgr );
#endif // _DEBUG

	// register ourselves
	engine->SetAchievementMgr( GetInstanceInterface() );

	// register for events
#ifdef GAME_DLL
	ListenForGameEvent( "entity_killed" );
	ListenForGameEvent( "game_init" );
#else
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "player_stats_updated" );
	ListenForGameEvent( "achievement_write_failed" );
	ListenForGameEvent( "user_data_downloaded" );
	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		m_UMCMsgAchievementEvent.Bind< CS_UM_AchievementEvent, CCSUsrMsg_AchievementEvent >( UtlMakeDelegate( MsgFunc_AchievementEvent ) );
	}
	ListenForGameEvent( "read_game_titledata" );
	ListenForGameEvent( "write_game_titledata" );
	ListenForGameEvent( "reset_game_titledata" ); 
	ListenForGameEvent( "write_profile_data" );
#endif // CLIENT_DLL

#ifdef TF_CLIENT_DLL
	ListenForGameEvent( "localplayer_changeclass" );
	ListenForGameEvent( "localplayer_changeteam" );
	ListenForGameEvent( "teamplay_round_start" );	
	ListenForGameEvent( "teamplay_round_win" );
#endif // TF_CLIENT_DLL

	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Called at init time after all systems are init'd.  We have to
//			do this in PostInit because the Steam app ID is not available earlier.
//-----------------------------------------------------------------------------
void CAchievementMgr::PostInit()
{
	// get current game dir
	const char *pGameDir = COM_GetModDirectory();

	CBaseAchievementHelper *pAchievementHelper = CBaseAchievementHelper::s_pFirst;
	while ( pAchievementHelper )
	{
		for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
		{
			// create and initialize all achievements and insert them in our maps
			CBaseAchievement *pAchievement = pAchievementHelper->m_pfnCreate();
			pAchievement->m_pAchievementMgr = this;
			pAchievement->Init();
			pAchievement->CalcProgressMsgIncrement();
			pAchievement->SetUserSlot( i );

			// only add an achievement if it does not have a game filter (only compiled into the game it
			// applies to, or truly cross-game) or, if it does have a game filter, the filter matches current game.
			// (e.g. EP 1/2/... achievements are in shared binary but are game specific, they have a game filter for runtime check.)
			const char *pGameDirFilter = pAchievement->m_pGameDirFilter;
			if ( !pGameDirFilter || ( 0 == Q_strcmp( pGameDir, pGameDirFilter ) ) )
			{
				if ( IsX360() || ( ( IsPC() || IsPS3() ) && !pAchievement->IsAssetAward() ) )
				{
					// We don't insert achievements with asset awards on the PC.
					m_mapAchievement[i].Insert( pAchievement->GetAchievementID(), pAchievement );
				}
				else
				{
					delete pAchievement;
				}
			}
			else
			{
				// achievement is not for this game, don't use it
				delete pAchievement;
			}
		}

		pAchievementHelper = pAchievementHelper->m_pNext;
	}
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		// Add each of the achievements to a CUtlVector ordered by achievement ID
		FOR_EACH_MAP( m_mapAchievement[i], iter )
		{
			if ( !m_mapAchievement[i][iter]->IsAssetAward() )
			{
				m_vecAchievement[i].AddToTail( m_mapAchievement[i][iter] );
			}
			else
			{
				m_vecAward[i].AddToTail( m_mapAchievement[i][iter] );
			}
		}
		m_vecAchievement[i].Sort( AchievementIDCompare );

		m_vecAchievementInOrder[i].AddVectorToTail( m_vecAchievement[i] );
		m_vecAchievementInOrder[i].Sort( AchievementOrderCompare );

		m_vecAwardInOrder[i].AddVectorToTail( m_vecAward[i] );
		m_vecAwardInOrder[i].Sort( AchievementOrderCompare );

		// Clear the progress and achieved data for each splitscreen player
		ClearAchievementData( i );
	}

	if ( IsPC() )
	{
		UserConnected( STEAM_PLAYER_SLOT );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Shuts down the achievement manager.
//-----------------------------------------------------------------------------
void CAchievementMgr::Shutdown()
{
	SaveGlobalState();

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		FOR_EACH_MAP( m_mapAchievement[i], iter )
		{
			delete m_mapAchievement[i][iter];
		}
		m_mapAchievement[i].RemoveAll();
		m_vecAchievement[i].RemoveAll();
		m_vecAward[i].RemoveAll();
		m_vecAchievementInOrder[i].RemoveAll();
		m_vecAwardInOrder[i].RemoveAll();
		m_vecKillEventListeners[i].RemoveAll();
		m_vecMapEventListeners[i].RemoveAll();
		m_vecComponentListeners[i].RemoveAll();
		m_AchievementsAwarded[i].RemoveAll();
		m_AchievementsAwardedDuringCurrentGame[i].RemoveAll();
	}

	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
}

//-----------------------------------------------------------------------------
// Purpose: Cleans up all achievements and then re-initializes them
//-----------------------------------------------------------------------------
void CAchievementMgr::InitializeAchievements( )
{
	Shutdown();
	PostInit();
}

#ifdef CLIENT_DLL
extern const ConVar *sv_cheats;
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAchievementMgr::SetAchievementThink( CBaseAchievement *pAchievement, float flThinkTime )
{
	// Is the achievement already in the think list?
	int iCount = m_vecThinkListeners.Count();
	for ( int i = 0; i < iCount; i++ )
	{
		if ( m_vecThinkListeners[i].pAchievement == pAchievement )
		{
			if ( flThinkTime == THINK_CLEAR )
			{
				m_vecThinkListeners.Remove(i);
				return;
			}

			m_vecThinkListeners[i].m_flThinkTime = gpGlobals->curtime + flThinkTime;
			return;
		}
	}

	if ( flThinkTime == THINK_CLEAR )
		return;

	// Otherwise, add it to the list
	int iIdx = m_vecThinkListeners.AddToTail();
	m_vecThinkListeners[iIdx].pAchievement = pAchievement;
	m_vecThinkListeners[iIdx].m_flThinkTime = gpGlobals->curtime + flThinkTime;
}

//-----------------------------------------------------------------------------
// Purpose: called on level init
//-----------------------------------------------------------------------------
void CAchievementMgr::LevelInitPreEntity()
{
	m_bCheatsEverOn = false;

	// sb: need to make sure we enable achievement manager on the client in split screen??

#if defined( PORTAL2 )
	// portal 2 can run in both single player and multiplayer modes
	// achievement manager is on the client
#else
#	ifdef GAME_DLL
		// For single-player games, achievement mgr must live on the server.  (Only the server has detailed knowledge of game state.)
		Assert( !GameRules()->IsMultiplayer() );
#	else
		// For multiplayer games, achievement mgr must live on the client.  (Only the client can read/write player state from Steam/XBox Live.)
		Assert( GameRules()->IsMultiplayer() );
#	endif
#endif

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		// clear list of achievements listening for events
		m_vecKillEventListeners[i].RemoveAll();
		m_vecMapEventListeners[i].RemoveAll();
		m_vecComponentListeners[i].RemoveAll();

		m_AchievementsAwarded[i].RemoveAll();

		m_flLastClassChangeTime[i] = 0;
		m_flTeamplayStartTime[i] = 0;
		m_iMiniroundsCompleted[i] = 0;
	}

	// client and server have map names available in different forms (full path on client, just file base name on server), 
	// cache it in base file name form here so we don't have to have different code paths each time we access it
#ifdef CLIENT_DLL	
	Q_FileBase( engine->GetLevelName(), m_szMap, ARRAYSIZE( m_szMap ) );
#else
	Q_strncpy( m_szMap, gpGlobals->mapname.ToCStr(), ARRAYSIZE( m_szMap ) );
#endif // CLIENT_DLL

	if ( PLATFORM_EXT[0] )
	{
		// need to remove the .360 extension on the end of the map name
		char *pExt = Q_stristr( m_szMap, PLATFORM_EXT );
		if ( pExt )
		{
			*pExt = '\0';
		}
	}

	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		// Don't bother listening if a slot is not active
		if ( m_bUserSlotActive[i] )
		{
			// look through all achievements, see which ones we want to have listen for events
			FOR_EACH_MAP( m_mapAchievement[i], iAchievement )
			{
				CBaseAchievement *pAchievement = m_mapAchievement[i][iAchievement];

				// if the achievement only applies to a specific map, and it's not the current map, skip it
				const char *pMapNameFilter = pAchievement->m_pMapNameFilter;
				if ( pMapNameFilter && ( 0 != Q_strcmp( m_szMap, pMapNameFilter ) ) )
					continue;

				// if the achievement needs kill events, add it as a listener
				if ( pAchievement->GetFlags() & ACH_LISTEN_KILL_EVENTS )
				{
					m_vecKillEventListeners[i].AddToTail( pAchievement );
				}
				// if the achievement needs map events, add it as a listener
				if ( pAchievement->GetFlags() & ACH_LISTEN_MAP_EVENTS )
				{
					m_vecMapEventListeners[i].AddToTail( pAchievement );
				}
				// if the achievement needs map events, add it as a listener
				if ( pAchievement->GetFlags() & ACH_LISTEN_COMPONENT_EVENTS )
				{
					m_vecComponentListeners[i].AddToTail( pAchievement );
				}
				if ( pAchievement->IsActive() )
				{
					pAchievement->ListenForEvents();
				}
			}
			m_flLevelInitTime[i] = gpGlobals->curtime;
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: called on level shutdown
//-----------------------------------------------------------------------------
void CAchievementMgr::LevelShutdownPreEntity()
{
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		// make all achievements stop listening for events 
		FOR_EACH_MAP( m_mapAchievement[i], iAchievement )
		{
			CBaseAchievement *pAchievement = m_mapAchievement[i][iAchievement];
			pAchievement->StopListeningForAllEvents();
		}
	}

	// save global state if we have any changes
	SaveGlobalStateIfDirty();

	// $FIXME(hpe) guessing this is a hack we care to fix
// 12/2/2008ywb:  HACK, this needs to know the nUserSlot!!!
	int nUserSlot = 0;
	UploadUserData( nUserSlot );
}

//$FIXME(hpe) we should probably guard for out of index nUserSlot values in all of these getachievement functions
//-----------------------------------------------------------------------------
// Purpose: returns achievement for specified ID
//-----------------------------------------------------------------------------
CBaseAchievement *CAchievementMgr::GetAchievementByID( int iAchievementID, int nUserSlot )
{
	Assert(nUserSlot < MAX_SPLITSCREEN_PLAYERS);
	if( nUserSlot >= MAX_SPLITSCREEN_PLAYERS )
	{
		return NULL;
	}

	int iAchievement = m_mapAchievement[nUserSlot].Find( iAchievementID );
	if ( iAchievement != m_mapAchievement[nUserSlot].InvalidIndex() )
	{
		return m_mapAchievement[nUserSlot][iAchievement];
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: returns achievement with specified name.  NOTE: this iterates through
//			all achievements to find the name, intended for debugging purposes.
//			Use GetAchievementByID for fast lookup.
//-----------------------------------------------------------------------------
CBaseAchievement *CAchievementMgr::GetAchievementByName( const char *pchName, int nUserSlot )
{
	VPROF("GetAchievementByName");

	Assert(nUserSlot < MAX_SPLITSCREEN_PLAYERS);
	if( nUserSlot >= MAX_SPLITSCREEN_PLAYERS )
	{
		return NULL;
	}


	FOR_EACH_MAP_FAST( m_mapAchievement[nUserSlot], i )
	{
		CBaseAchievement *pAchievement = m_mapAchievement[nUserSlot][i];
		if ( pAchievement && 0 == ( Q_stricmp( pchName, pAchievement->GetName() ) ) )
			return pAchievement;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Returns true if the achievement with the specified name has been achieved
//-----------------------------------------------------------------------------
bool CAchievementMgr::HasAchieved( const char *pchName, int nUserSlot )
{
	CBaseAchievement *pAchievement = GetAchievementByName( pchName, nUserSlot );
	if ( pAchievement )
		return pAchievement->IsAchieved();
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: downloads user data from Steam or XBox Live
//-----------------------------------------------------------------------------
void CAchievementMgr::UserDisconnected( int nUserSlot )
{
	m_bUserSlotActive[nUserSlot] = false;

	ClearAchievementData( nUserSlot );
}


//-----------------------------------------------------------------------------
// Read our achievement data from the title data.
//-----------------------------------------------------------------------------
void CAchievementMgr::ReadAchievementsFromTitleData( int iController, int iSlot )
{
	IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
	if ( !pPlayer )
		return;

	m_bReadingFromTitleData = true;

	// Set the incremental achievement progress and components
	for ( int i=0; i<m_vecAchievement[iSlot].Count(); ++i )
	{
		CBaseAchievement *pAchievement = m_vecAchievement[iSlot][i];
		if ( pAchievement )
		{
			pAchievement->ReadProgress( pPlayer );
		}
	}

	for ( int i=0; i<m_vecAward[iSlot].Count(); ++i )
	{
		CBaseAchievement *pAward = m_vecAward[iSlot][i];
		if ( pAward )
		{
			pAward->ReadProgress( pPlayer );
		}
	}

	m_bReadingFromTitleData = false;

	SaveGlobalStateIfDirty();
}



//-----------------------------------------------------------------------------
// Purpose: downloads user data from Steam or XBox Live
//-----------------------------------------------------------------------------
void CAchievementMgr::UserConnected( int nUserSlot )
{
#ifdef CLIENT_DLL
	if ( IsPC() || IsPS3() )
	{
#ifdef _PS3
		if ( XBX_GetUserIsGuest( nUserSlot ) )
			return;

		const int iController = XBX_GetUserId( nUserSlot );

		if ( iController == XBX_INVALID_USER_ID )
			return;
#endif
		// ASSERT( STEAM_PLAYER_SLOT == nUserSlot )

		m_bUserSlotActive[STEAM_PLAYER_SLOT] = true;
	}
	else if ( IsX360() )
	{
#if defined( _X360 )

		if ( XBX_GetUserIsGuest( nUserSlot ) )
			return;

		const int iController = XBX_GetUserId( nUserSlot );

		if ( iController == XBX_INVALID_USER_ID )
			return;

		if ( XUserGetSigninState( iController ) == eXUserSigninState_NotSignedIn )
			return;

		m_bUserSlotActive[nUserSlot] = true;

		// Download achievements from XBox Live
		const DWORD nTotalAchievements = GetAchievementCount();
		DWORD nTotalX360AchsEnumerated = 0;
		HANDLE hEnumerator = NULL;
		DWORD bytes;
		DWORD ret = XUserCreateAchievementEnumerator( 0, iController, INVALID_XUID, XACHIEVEMENT_DETAILS_ALL, 0, nTotalAchievements, &bytes, &hEnumerator );
		if ( ret != ERROR_SUCCESS )
		{
			Warning( "Enumerate Achievements for controller %d failed! Failed to create enumerator with code %d.\n", iController, ret );
			return;
		}

		// Allocate the buffer
		CUtlVector< char > vBuffer;
		vBuffer.SetCount( bytes );

		// Enumerate the achievements from Live
		ret = XEnumerate( hEnumerator, vBuffer.Base(), bytes, &nTotalX360AchsEnumerated, NULL );
		CloseHandle( hEnumerator );
		hEnumerator = NULL;

		if ( ret != ERROR_SUCCESS )
		{
			Warning( "Enumerate Achievements for controller %d failed! Failed to enumerate with code %d.\n", iController, ret );
			return;
		}

		if ( nTotalX360AchsEnumerated != nTotalAchievements )
		{
			Warning( "Enumerate achievements returned %d achievements != %d total registered achievements!\n",
				nTotalX360AchsEnumerated, nTotalAchievements );
		}

#if !defined ( CSTRIKE15 )
		// Give live a chance to mark achievements as unlocked, in case the achievement manager
		// wasn't able to get that data (storage device missing, read failure, etc)
		XACHIEVEMENT_DETAILS const *pXboxAchievements = ( XACHIEVEMENT_DETAILS const * ) vBuffer.Base();
		TitleAchievementsDescription_t const *pTitleAchMap = g_pMatchFramework->GetMatchTitle()->DescribeTitleAchievements();
		for ( DWORD i = 0; i < nTotalX360AchsEnumerated; ++i )
		{
			TitleAchievementsDescription_t const *pAchEntry = FindTitleAchievementById( pTitleAchMap, pXboxAchievements[i].dwId );
			if ( !pAchEntry )
			{
				Warning( "X360 downloaded title achievement ID=%d is not in title achievement map, skipping!\n", pXboxAchievements[i].dwId );
				continue;
			}
			
			CBaseAchievement *pAchievement = GetAchievementByName( pAchEntry->m_szAchievementName, nUserSlot );
			if ( !pAchievement )
				continue;

			// Give Live a chance to claim the achievement as unlocked
			if ( AchievementEarned( pXboxAchievements[i].dwFlags ) )
			{
				pAchievement->SetAchieved( true );
				pAchievement->CheckAssetAwards( nUserSlot );
			}
			else
			{
				pAchievement->SetAchieved( false );
			}
		}
#endif

#endif // X360
	}
#endif // CLIENT_DLL
}

const char *COM_GetModDirectory()
{
	static char modDir[MAX_PATH];
	if ( Q_strlen( modDir ) == 0 )
	{
		const char *gamedir = CommandLine()->ParmValue("-game", CommandLine()->ParmValue( "-defaultgamedir", "hl2" ) );
		Q_strncpy( modDir, gamedir, sizeof(modDir) );
		if ( strchr( modDir, '/' ) || strchr( modDir, '\\' ) )
		{
			Q_StripLastDir( modDir, sizeof(modDir) );
			int dirlen = Q_strlen( modDir );
			Q_strncpy( modDir, gamedir + dirlen, sizeof(modDir) - dirlen );
		}
	}

	return modDir;
}

//-----------------------------------------------------------------------------
// Purpose: Uploads user data to Steam or XBox Live
//-----------------------------------------------------------------------------
void CAchievementMgr::UploadUserData( int nUserSlot )
{
#if defined( CLIENT_DLL ) && !defined( NO_STEAM )
	if ( ( IsPC() || IsPS3() ) && ( nUserSlot == STEAM_PLAYER_SLOT ) )
	{
		if ( steamapicontext->SteamUserStats() )
		{
			// Upload current Steam client achievements & stats state to Steam.  Will get called back at OnUserStatsStored when complete.
			// Only values previously set via SteamUserStats() get uploaded
			steamapicontext->SteamUserStats()->StoreStats();
			m_flTimeLastUpload = Plat_FloatTime();
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: saves global state to file
//-----------------------------------------------------------------------------
void CAchievementMgr::SaveGlobalState( )
{
	int iController = 0;

#ifdef _X360
	for ( int j = 0; j < MAX_SPLITSCREEN_PLAYERS; ++ j )
	{
		if ( !IsUserConnected( j ) )
			continue;
		iController = XBX_GetUserId( j );
#else
	int j = STEAM_PLAYER_SLOT;
	VPROF_BUDGET( "CAchievementMgr::SaveGlobalState", "Achievements" );
	{
#endif

		IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
		if ( !pPlayer )
#ifdef _X360
			continue;
#else
			return;
#endif

		for ( int i = 0; i < m_vecAchievement[j].Count(); ++i )
		{
			CBaseAchievement *pAchievement = m_vecAchievement[j][i];
			if ( pAchievement )
			{
				pAchievement->WriteProgress( pPlayer );
			}
		}

		for ( int i = 0; i < m_vecAward[j].Count(); ++i )
		{
			CBaseAchievement *pAward = m_vecAward[j][i];
			if ( pAward )
			{
				pAward->WriteProgress( pPlayer );
			}
		}

		if ( IsPC() || IsPS3() )
		{
			m_bDirty[iController] = false;
		}
	}

}

void CAchievementMgr::SendResetProfileEvent()
{
	IGameEvent * resetUserProfileEvent = gameeventmanager->CreateEvent( "reset_user_profile" );
	if (resetUserProfileEvent)
		gameeventmanager->FireEventClientSide( resetUserProfileEvent );
}

void CAchievementMgr::SendWriteProfileEvent()
{
	IGameEvent * writeUserProfileEvent = gameeventmanager->CreateEvent( "write_user_profile" );
	if (writeUserProfileEvent)
		gameeventmanager->FireEventClientSide( writeUserProfileEvent );
}

//-----------------------------------------------------------------------------
// Purpose: saves global state to file if there have been any changes
//-----------------------------------------------------------------------------
void CAchievementMgr::SaveGlobalStateIfDirty( )
{
	if ( m_bDirty && !m_bReadingFromTitleData )
	{
		SaveGlobalState( );
	}
}

bool CAchievementMgr::IsAchievementAllowedInGame( int iAchievementID )
{
	// Offline modes with trivial bots disable ALL achievements
	if ( CSGameRules() && !CSGameRules()->IsAwardsProgressAllowedForBotDifficulty() )
		return false;

	bool isGunGameAchievement = false;

	switch( iAchievementID )
	{
		// a list of gun game achievements
		//case CSFastRoundWin:
		case CSGunGameKillKnifer:
		case CSWinEveryGGMap:
		case CSGunGameProgressiveRampage:
		case CSGunGameFirstKill:
		case CSFirstBulletKills:
		case CSGunGameConservationist:
		case CSWinMatchAR_SHOOTS:
		case CSWinMatchAR_BAGGAGE:
		case CSPlantBombsTRLow:
		case CSDefuseBombsTRLow:
		case CSKillEnemyTerrTeamBeforeBombPlant:
		case CSKillEnemyCTTeamBeforeBombPlant:
		case CSWinMatchDE_LAKE:
		case CSWinMatchDE_SAFEHOUSE:   
		case CSWinMatchDE_SUGARCANE:
		case CSWinMatchDE_STMARC:
		case CSWinMatchDE_BANK:
		case CSWinMatchDE_EMBASSY:
		case CSWinMatchDE_DEPOT:
		case CSWinMatchDE_SHORTTRAIN:
		//case CSHipShot:
		case CSBornReady:
		case CSSpawnCamper:
		case CSGunGameKnifeKillKnifer:
		case CSGunGameSMGKillKnifer:
		case CSStillAlive:
		case CSGGWinRoundsLow:
		case CSGGWinRoundsMed:
		case CSGGWinRoundsHigh:
		case CSGGWinRoundsExtreme:
		case CSGGWinRoundsUltimate:
		case CSGGRoundsLow:
		case CSGGRoundsMed:
		case CSGGRoundsHigh:
		case CSPlayEveryGGMap:
		case CSDominationsLow:
		case CSDominationsHigh:
		case CSRevengesLow:
		case CSRevengesHigh:
		case CSDominationOverkillsLow:
		case CSDominationOverkillsHigh:
		case CSDominationOverkillsMatch:
		case CSExtendedDomination:
		case CSConcurrentDominations:
		//case CSAvengeFriend:
			isGunGameAchievement = true;
	};

	// Only unlock gungame achievements in gun game modes
	if( isGunGameAchievement ) 
	{
		return CSGameRules()->IsPlayingGunGame();
	}
	else
	{
		// Other achievements are valid for all game types.
		return true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: awards specified achievement
//-----------------------------------------------------------------------------
void CAchievementMgr::AwardAchievement( int iAchievementID, int nUserSlot )
{
	if( !IsAchievementAllowedInGame( iAchievementID ) )
		return;

#ifdef CLIENT_DLL
	C_BasePlayer *pPlayerLocal = C_BasePlayer::GetLocalPlayer();
	C_CSPlayer* pPlayer = ToCSPlayer(pPlayerLocal);

	if( ( pPlayer && pPlayer->IsControllingBot() ) ) // if we're controlling a bot, no achievements for us!
		return;

	CBaseAchievement *pAchievement = GetAchievementByID( iAchievementID, nUserSlot );
	Assert( pAchievement );
	if ( !pAchievement )
		return;

#if defined ( _X360 ) && !defined ( CSTRIKE15 )
	TitleAchievementsDescription_t const *pAchEntryMap = g_pMatchFramework->GetMatchTitle()->DescribeTitleAchievements();
	TitleAchievementsDescription_t const *pAchEntry = FindTitleAchievementByName( pAchEntryMap, pAchievement->GetName() );
	if ( !pAchEntry && !pAchievement->IsAssetAward() )
	{
		Warning( "X360 cannot award title achievement '%s' ID=%d because it is not in title achievement map, skipping!\n", pAchievement->GetName(), iAchievementID );
		return;
	}
#endif

	if ( !CheckAchievementsEnabled() )
	{
		Msg( "Achievements disabled, ignoring achievement unlock for %s\n", pAchievement->GetName() );
		return;
	}

	if ( pAchievement->IsAchieved() )
	{
		if ( cc_achievement_debug.GetInt() > 0 )
		{
			Msg( "Achievement award called but already achieved: %s\n", pAchievement->GetName() );
		}
		return;
	}
	pAchievement->SetAchieved( true );

	pAchievement->OnAchieved();

#if defined ( CSTRIKE15 )
	IGameEvent * event = gameeventmanager->CreateEvent( "achievement_earned_local" , true );
	if ( event )
	{
		event->SetInt( "achievement", pAchievement->GetAchievementID() );
		event->SetInt( "splitscreenplayer", nUserSlot );
		gameeventmanager->FireEventClientSide( event );
	}

#if defined ( CLIENT_DLL )
	STEAMWORKS_TESTSECRET();
#endif
#endif

	if ( cc_achievement_debug.GetInt() > 0 )
	{
		Msg( "Achievement awarded: %s\n", pAchievement->GetName() );
	}

	// save state at next good opportunity.  (Don't do it immediately, may hitch at bad time.)
	m_bDirty[nUserSlot] = true;	

	if ( IsPC() || IsPS3() )
	{
#ifndef NO_STEAM
		if ( steamapicontext->SteamUserStats() )
		{
			VPROF_BUDGET( "AwardAchievement", VPROF_BUDGETGROUP_STEAM );
			// set this achieved in the Steam client
			bool bRet = steamapicontext->SteamUserStats()->SetAchievement( pAchievement->GetName() );
			//		Assert( bRet );
			if ( bRet )
			{
				// upload achievement to steam
				UploadUserData( nUserSlot );
				m_AchievementsAwarded[nUserSlot].AddToTail( iAchievementID );
			}
		}
#endif
	}
	else if ( IsX360() )
	{
#ifdef _X360
#if !defined ( CSTRIKE15 )
		if ( xboxsystem )
		{
			if ( pAchievement->IsAssetAward() )
			{
				// Fire off the asynchronous asset award operation.
				PendingAchievementInfo_t pendingAssetAwardState = { iAchievementID, nUserSlot, NULL };
				xboxsystem->AwardAvatarAsset( XBX_GetUserId( nUserSlot ), pAchievement->GetAssetAwardID(), &pendingAssetAwardState.pOverlappedResult );
				m_pendingAchievementState.AddToTail( pendingAssetAwardState );
			}
			else
			{
				// Fire off the asynchronous achievement award operation.
				PendingAchievementInfo_t pendingAchievementState = { iAchievementID, nUserSlot, NULL };
				xboxsystem->AwardAchievement( XBX_GetUserId( nUserSlot ), pAchEntry->m_idAchievement, &pendingAchievementState.pOverlappedResult );
				m_pendingAchievementState.AddToTail( pendingAchievementState );
			}
		}
#endif
#endif
	}

	SaveGlobalStateIfDirty();

	// Add this one to the list of achievements earned during current session
	m_AchievementsAwardedDuringCurrentGame[nUserSlot].AddToTail( iAchievementID );
#endif // CLIENT_DLL
}

#if defined ( _X360 )
void CAchievementMgr::AwardXBoxAchievement( int iAchievementID, int iXBoxAchievementID, int nUserSlot )
{
	if ( xboxsystem->IsArcadeTitleUnlocked() )
	{
		PendingAchievementInfo_t pendingAchievementState = { iAchievementID, nUserSlot, NULL };
		xboxsystem->AwardAchievement( XBX_GetUserId( nUserSlot ), iXBoxAchievementID, &pendingAchievementState.pOverlappedResult );
		// Save off the results for checking later
		m_pendingAchievementState.AddToTail( pendingAchievementState );
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: updates specified achievement
//-----------------------------------------------------------------------------
void CAchievementMgr::UpdateAchievement( int iAchievementID, int nData, int nUserSlot )
{
	CBaseAchievement *pAchievement = GetAchievementByID( iAchievementID, nUserSlot );
	Assert( pAchievement );
	if ( !pAchievement )
		return;

	if ( !CheckAchievementsEnabled() )
	{
		Msg( "Achievements disabled, ignoring achievement update for %s\n", pAchievement->GetName() );
		return;
	}

	if ( pAchievement->IsAchieved() )
	{
		if ( cc_achievement_debug.GetInt() > 0 )
		{
			Msg( "Achievement update called but already achieved: %s\n", pAchievement->GetName() );
		}
		return;
	}

	pAchievement->UpdateAchievement( nData );
}

#if defined( CLIENT_DLL ) && !defined( NO_STEAM )

void CAchievementMgr::UpdateStateFromSteam_Internal( int nUserSlot )
{
#if !defined ( _X360 )
	Assert( steamapicontext->SteamUserStats() );
	if ( !steamapicontext->SteamUserStats() )
		return;

	// run through the achievements and set their achieved state according to Steam data
	FOR_EACH_MAP( m_mapAchievement[nUserSlot], i )
	{
		CBaseAchievement *pAchievement = m_mapAchievement[nUserSlot][i];
		bool bAchieved = false;

		uint32 unlockTime;

		// Get the achievement status, and the time it was unlocked if unlocked.
		// If the return value is true, but the unlock time is zero, that means it was unlocked before Steam 
		// began tracking achievement unlock times (December 2009). Time is seconds since January 1, 1970.
		bool bRet = steamapicontext->SteamUserStats()->GetAchievementAndUnlockTime( pAchievement->GetName(), &bAchieved, &unlockTime );

		if ( bRet )
		{
			// set local achievement state
			pAchievement->SetAchieved( bAchieved );
			pAchievement->SetUnlockTime(unlockTime);
		}
		else
		{
			DevMsg( "ISteamUserStats::GetAchievement failed for %s\n", pAchievement->GetName() );
		}

		if ( pAchievement->StoreProgressInSteam() )
		{
			int iValue;
			char pszProgressName[1024];
			Q_snprintf( pszProgressName, 1024, "%s_STAT", pAchievement->GetName() );
			bRet = steamapicontext->SteamUserStats()->GetStat( pszProgressName, &iValue );
			if ( bRet )
			{
				pAchievement->SetCount( iValue );
				pAchievement->EvaluateNewAchievement();
			}
			else
			{
				DevMsg( "ISteamUserStats::GetStat failed to get progress value from Steam for achievement %s\n", pszProgressName );
			}
		}
	}

	IGameEvent * event = gameeventmanager->CreateEvent( "achievement_info_loaded" );
	if ( event )
	{
		gameeventmanager->FireEventClientSide( event );
	}

#endif
}
#endif

//-----------------------------------------------------------------------------
// Purpose: clears state for all achievements
//-----------------------------------------------------------------------------
void CAchievementMgr::PreRestoreSavedGame()
{
	for ( int j = 0; j < MAX_SPLITSCREEN_PLAYERS; ++j )
	{
		FOR_EACH_MAP( m_mapAchievement[j], i )
		{
			m_mapAchievement[j][i]->PreRestoreSavedGame();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: clears state for all achievements
//-----------------------------------------------------------------------------
void CAchievementMgr::PostRestoreSavedGame()
{
	for ( int j = 0; j < MAX_SPLITSCREEN_PLAYERS; ++j )
	{
		FOR_EACH_MAP( m_mapAchievement[j], i )
		{
			m_mapAchievement[j][i]->PostRestoreSavedGame();
		}
	}
}

extern bool IsInCommentaryMode( void );

ConVar	cc_achievement_disable("achievement_disable", "0", FCVAR_CHEAT | FCVAR_REPLICATED, "Turn off achievements." );

//-----------------------------------------------------------------------------
// Purpose: checks if achievements are enabled
//-----------------------------------------------------------------------------
bool CAchievementMgr::CheckAchievementsEnabled( )
{
	// if PC, Steam must be running and user logged in
	if ( cc_achievement_disable.GetBool() )
		return false;

	if ( IsPC() && !LoggedIntoSteam() )
	{
		Msg( "Achievements disabled: Steam not running.\n" );
		return false;
	}

	//No achievements in demo version.
#ifdef _DEMO
	return false;
#endif

#if defined( _X360 ) && defined( CLIENT_DLL )
	if ( m_bCheckSigninState )
	{
		uint state = XUserGetSigninState( XBX_GetActiveUserId() );
		if ( state == eXUserSigninState_NotSignedIn )
		{
			Msg( "Achievements disabled: not signed in to XBox user account.\n" );
			return false;
		}
	}
#endif

	// can't be in commentary mode, user is invincible
	if ( IsInCommentaryMode() )
	{
		Msg( "Achievements disabled: in commentary mode.\n" );
		return false;
	}

#ifdef CLIENT_DLL
	// achievements disabled if playing demo (Playback demo) or watching HLTV
	if ( engine->IsHLTV() )
	{
		Msg( "Achievements disabled: demo playing.\n" );
		return false;
	}
#endif // CLIENT_DLL

	if ( IsPC() )
	{
#if ALLOW_ACHIEVEMENTS_WITH_CHEATS
		if ( developer.GetInt() != 0 )
			return true;
#endif // ALLOW_ACHIEVEMENTS_WITH_CHEATS

		// Don't award achievements if cheats are turned on.  
		if ( WereCheatsEverOn() )
		{
			// Cheats get turned on automatically if you run with -dev which many people do internally, so allow cheats if developer is turned on and we're not running
			// on Steam public
#if defined( CLIENT_DLL ) && !defined( NO_STEAM )
			if ( ( developer.GetInt() == 0 ) || !steamapicontext->SteamUtils() || ( k_EUniversePublic == steamapicontext->SteamUtils()->GetConnectedUniverse() ) )
#else
			if ( developer.GetInt() == 0 )
#endif
			{
				Msg( "Achievements disabled: cheats turned on in this app session.\n" );
				return false;
			}
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Determine friendness on xbox
//-----------------------------------------------------------------------------
#if defined ( _X360 )
bool IsXboxFriends( int userID, int entityIndex )
{
	// $TODO(hpe) connect the matchmaking bits
	return false;
	//if ( !matchmaking )
	//	return false;

	//XUID XUid[1];
	//XUid[0] = matchmaking->PlayerIdToXuid( entityIndex );
	//BOOL bFriend = false;

	//// If we don't have a XUID, we don't even need to bother asking...
	//if ( XUid[0] == 0 )
	//{
	//	return false;
	//}

	//XUserAreUsersFriends( userID, XUid, 1, &bFriend, NULL );

	//return bFriend;
}
#endif


#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: Returns the whether all of the local player's team mates are
//			on her friends list, and if there are at least the specified # of
//			teammates.  Involves cross-process calls to Steam so this is mildly
//			expensive, don't call this every frame.
//-----------------------------------------------------------------------------
bool CalcPlayersOnFriendsList( int iMinFriends )
{
	// Got message during connection
	if ( !g_PR )
		return false;

	Assert( g_pGameRules->IsMultiplayer() );

	// Do a cheap rejection: check teammate count first to see if we even need to bother checking w/Steam
	// Subtract 1 for the local player.
	if ( CalcPlayerCount()-1 < iMinFriends )
		return false;

	// determine local player team
	int iLocalPlayerIndex =  GetLocalPlayerIndex();
	uint64 XPlayerUid = 0;

	if ( IsPC() || IsPS3() )
	{
#ifndef NO_STEAM
		if ( !steamapicontext->SteamFriends() || !steamapicontext->SteamUtils() || !g_pGameRules->IsMultiplayer() )
#endif
			return false;
	}
	else if ( IsX360() )
	{
		if ( !g_pMatchFramework )
			return false;

		XPlayerUid = XBX_GetActiveUserId();
	}
	else
	{
		// other platforms...?
		return false;
	}
	// Loop through the players
	int iTotalFriends = 0;
	for( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
	{
		// find all players who are on the local player's team
		if( ( iPlayerIndex != iLocalPlayerIndex ) && ( g_PR->IsConnected( iPlayerIndex ) ) )
		{
			player_info_t pi;
			if ( !engine->GetPlayerInfo( iPlayerIndex, &pi ) )
				continue;
			if ( !pi.xuid )
				continue;

#ifdef _X360
			// check and see if they're on the local player's friends list
			BOOL bFriend = FALSE;
			XUserAreUsersFriends( XPlayerUid, &pi.xuid, 1, &bFriend, NULL );
			if ( !bFriend )
				continue;
#elif !defined( NO_STEAM )
			// check and see if they're on the local player's friends list
			if ( !steamapicontext->SteamFriends()->HasFriend( pi.xuid, /*k_EFriendFlagImmediate*/ 0x04 ) )
				continue;
#else
			continue;
#endif

			iTotalFriends++;
		}
	}

	return (iTotalFriends >= iMinFriends);
}

//-----------------------------------------------------------------------------
// Purpose: Returns whether there are a specified # of teammates who all belong
//			to same clan as local player. Involves cross-process calls to Steam 
//			so this is mildly expensive, don't call this every frame.
//-----------------------------------------------------------------------------
bool CalcHasNumClanPlayers( int iClanTeammates )
{
	Assert( g_pGameRules->IsMultiplayer() );

	if ( IsPC() || IsPS3() )
	{
#ifndef NO_STEAM
		// Do a cheap rejection: check teammate count first to see if we even need to bother checking w/Steam
		// Subtract 1 for the local player.
		if ( CalcPlayerCount()-1 < iClanTeammates )
			return false;

		if ( !steamapicontext->SteamFriends() || !steamapicontext->SteamUtils() || !g_pGameRules->IsMultiplayer() )
			return false;

		// determine local player team
		int iLocalPlayerIndex =  GetLocalPlayerIndex();

		for ( int iClan = 0; iClan < steamapicontext->SteamFriends()->GetClanCount(); iClan++ )
		{
			int iClanMembersOnTeam = 0;
			CSteamID clanID = steamapicontext->SteamFriends()->GetClanByIndex( iClan );
			// enumerate all players
			for( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
			{
				if( ( iPlayerIndex != iLocalPlayerIndex ) && ( g_PR->IsConnected( iPlayerIndex ) ) )
				{
					player_info_t pi;
					if ( engine->GetPlayerInfo( iPlayerIndex, &pi ) && ( pi.friendsID ) )
					{	
						// check and see if they're on the local player's friends list
						CSteamID steamID( pi.friendsID, 1, steamapicontext->SteamUtils()->GetConnectedUniverse(), k_EAccountTypeIndividual );
						if ( steamapicontext->SteamFriends()->IsUserInSource( steamID, clanID ) )
						{
							iClanMembersOnTeam++;
							if ( iClanMembersOnTeam == iClanTeammates )
								return true;
						}
					}
				}
			}
		}
#endif

		return false;
	}
	else if ( IsGameConsole() )
	{
		// TODO: implement for 360
		return false;
	}
	else 
	{
		// other platforms...?
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns the # of teammates of the local player
//-----------------------------------------------------------------------------
int	CalcTeammateCount()
{
	Assert( g_pGameRules->IsMultiplayer() );

	// determine local player team
	int iLocalPlayerIndex =  GetLocalPlayerIndex();
	int iLocalPlayerTeam = g_PR->GetTeam( iLocalPlayerIndex );

	int iNumTeammates = 0;
	for( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
	{
		// find all players who are on the local player's team
		if( ( iPlayerIndex != iLocalPlayerIndex ) && ( g_PR->IsConnected( iPlayerIndex ) ) && ( g_PR->GetTeam( iPlayerIndex ) == iLocalPlayerTeam ) )
		{
			iNumTeammates++;
		}
	}
	return iNumTeammates;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the # of teammates of the local player
//-----------------------------------------------------------------------------
int	CalcPlayerCount()
{
	int iCount = 0;
	for( int iPlayerIndex = 1 ; iPlayerIndex <= MAX_PLAYERS; iPlayerIndex++ )
	{
		// find all players who are on the local player's team
		if( g_PR->IsConnected( iPlayerIndex ) )
		{
			iCount++;
		}
	}
	return iCount;
}

#endif // CLIENT_DLL


////-----------------------------------------------------------------------------
// Purpose: Resets all achievements. Behaves like ResetAchievements but does not automatically save profile
//-----------------------------------------------------------------------------
void CAchievementMgr::ClearAchievements( int nUserSlot )
{
	if ( nUserSlot < 0 || nUserSlot >= MAX_SPLITSCREEN_PLAYERS )
	{
		AssertMsg( false, "CAchievementMgr::ClearAchievements invalid nUserSlot\n" );
		return;
	}

	FOR_EACH_MAP( m_mapAchievement[nUserSlot], i )
	{
		CBaseAchievement *pAchievement = m_mapAchievement[nUserSlot][i];
		ResetAchievement_Internal( pAchievement );
	}
}


//-----------------------------------------------------------------------------
// Purpose: Resets all achievements.  For debugging purposes only
//-----------------------------------------------------------------------------
void CAchievementMgr::ResetAchievements()
{
#if defined( CLIENT_DLL ) && !defined( NO_STEAM )
	if ( !IsPC() )
	{
		DevMsg( "Only available on PC\n" );
		return;
	}

	if ( !LoggedIntoSteam() )
	{
		Msg( "Steam not running, achievements disabled. Cannot reset achievements.\n" );
		return;
	}

	FOR_EACH_MAP( m_mapAchievement[STEAM_PLAYER_SLOT], i )
	{
		CBaseAchievement *pAchievement = m_mapAchievement[STEAM_PLAYER_SLOT][i];
		ResetAchievement_Internal( pAchievement );
	}
	if ( steamapicontext->SteamUserStats() )
	{
		steamapicontext->SteamUserStats()->StoreStats();
	}
	SaveGlobalState();
#endif // CLIENT_DLL
}

void CAchievementMgr::ResetAchievement( int iAchievementID )
{
#if defined( CLIENT_DLL ) && !defined( NO_STEAM )
	if ( !IsPC() )
	{
		DevMsg( "Only available on PC\n" );
		return;
	}

	if ( !LoggedIntoSteam() )
	{
		Msg( "Steam not running, achievements disabled. Cannot reset achievements.\n" );
		return;
	}

	CBaseAchievement *pAchievement = GetAchievementByID( iAchievementID, STEAM_PLAYER_SLOT );
	Assert( pAchievement );
	if ( pAchievement )
	{
		ResetAchievement_Internal( pAchievement );
		if ( steamapicontext->SteamUserStats() )
		{
			steamapicontext->SteamUserStats()->StoreStats();
		}
		SaveGlobalState();
	}
#endif // CLIENT_DLL
}

//-----------------------------------------------------------------------------
// Purpose: Resets all achievements.  For debugging purposes only
//-----------------------------------------------------------------------------
void CAchievementMgr::PrintAchievementStatus()
{
#if defined( CLIENT_DLL )
	if ( IsPC() && !LoggedIntoSteam() )
	{
		Msg( "Steam not running, achievements disabled. Cannot view or unlock achievements.\n" );
		return;
	}

	Msg( "%42s %-30s %s\n", "Name:", "Status:", "Point value:" );
	int iTotalAchievements = 0, iTotalPoints = 0;
	FOR_EACH_MAP( m_mapAchievement[STEAM_PLAYER_SLOT], i )
	{
		CBaseAchievement *pAchievement = m_mapAchievement[STEAM_PLAYER_SLOT][i];

		Msg( "%42s ", pAchievement->GetName() );	

		CFailableAchievement *pFailableAchievement = dynamic_cast<CFailableAchievement *>( pAchievement );
		if ( pAchievement->IsAchieved() )
		{
			CCSBaseAchievement* pCSAchievement = dynamic_cast<CCSBaseAchievement*>(pAchievement);

			// Assign the award date text
			char dateBuffer[32] = "";
			int year, month, day, hour, minute, second;
			if ( pCSAchievement && pCSAchievement->GetAwardTime(year, month, day, hour, minute, second) )
			{
				Q_snprintf( dateBuffer, sizeof(dateBuffer), "ACHIEVED %4d-%02d-%02d %2d:%02d:%02d", year, month, day, hour, minute, second );
				Msg( "%-30s", dateBuffer );
			}
			else
			{
				Msg( "%-30s", "ACHIEVED" );
			}
		}
		else if ( pFailableAchievement && pFailableAchievement->IsFailed() )
		{
			Msg( "%-30s", "FAILED" );
		}
		else 
		{
			char szBuf[255];
			char szComponents[67] = "";
			int componentsLen = ARRAYSIZE( szComponents );
			
			if ( pAchievement->HasComponents() )
			{
				uint64 bits = pAchievement->GetComponentBits();
				int numComponents = pAchievement->GetNumComponents();

				Q_strcat( szComponents, "(", componentsLen );

				for ( int i=numComponents-1; i>=0; i-- )
				{
					Q_strcat( szComponents, ( bits & ((uint64)1<<i) ) ? "1" : "0", componentsLen );
				}				

				Q_strcat( szComponents, ")", componentsLen );
			}

			Q_snprintf( szBuf, ARRAYSIZE( szBuf ), "(%d/%d)%s %s",
				pAchievement->GetCount(), pAchievement->GetGoal(),
				pAchievement->IsActive() ? "" : " (inactive)",
				szComponents );

			Msg( "%-30s", szBuf );
		}
		Msg( " %d   ", pAchievement->GetPointValue() );
		pAchievement->PrintAdditionalStatus();
		Msg( "\n" );
		iTotalAchievements++;
		iTotalPoints += pAchievement->GetPointValue();
	}
	Msg( "Total achievements: %d  Total possible points: %d\n", iTotalAchievements, iTotalPoints );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: called when a game event is fired
//-----------------------------------------------------------------------------
void CAchievementMgr::FireGameEvent( IGameEvent *event )
{
#ifdef CLIENT_DLL
	int nSplitScreenPlayer = event->GetInt( "splitscreenplayer" );
	Assert(nSplitScreenPlayer < MAX_SPLITSCREEN_PLAYERS);

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSplitScreenPlayer );
#endif

	VPROF_( "CAchievementMgr::FireGameEvent", 1, VPROF_BUDGETGROUP_STEAM, false, 0 );
	const char *name = event->GetName();
	if ( 0 == Q_strcmp( name, "entity_killed" ) )
	{
#ifdef GAME_DLL
		CBaseEntity *pVictim = UTIL_EntityByIndex( event->GetInt( "entindex_killed", 0 ) );
		CBaseEntity *pAttacker = UTIL_EntityByIndex( event->GetInt( "entindex_attacker", 0 ) );
		CBaseEntity *pInflictor = UTIL_EntityByIndex( event->GetInt( "entindex_inflictor", 0 ) );
		OnKillEvent( pVictim, pAttacker, pInflictor, event );
#endif // GAME_DLL
	}
	else if ( 0 == Q_strcmp( name, "game_init" ) )
	{
#ifdef GAME_DLL
		// clear all state as though we were loading a saved game, but without loading the game
		PreRestoreSavedGame();
		PostRestoreSavedGame();
#endif // GAME_DLL
	}
#ifdef CLIENT_DLL
	else if ( 0 == Q_strcmp( name, "player_death" ) )
	{
		CBaseEntity *pVictim = ClientEntityList().GetEnt( engine->GetPlayerForUserID( event->GetInt("userid") ) );
		CBaseEntity *pAttacker = ClientEntityList().GetEnt( engine->GetPlayerForUserID( event->GetInt("attacker") ) );
		OnKillEvent( pVictim, pAttacker, NULL, event );
	}
	else if ( 0 == Q_strcmp( name, "localplayer_changeclass" ) )
	{
		// keep track of when the player last changed class
		m_flLastClassChangeTime[nSplitScreenPlayer] =  gpGlobals->curtime;
	}
	else if ( 0 == Q_strcmp( name, "localplayer_changeteam" ) )
	{
		// keep track of the time of transitions to and from a game team
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pLocalPlayer )
		{
			int iTeam = pLocalPlayer->GetTeamNumber();
			if ( iTeam > TEAM_SPECTATOR )
			{
				if ( 0 == m_flTeamplayStartTime[nSplitScreenPlayer] )
				{
					// player transitioned from no/spectator team to a game team, mark the time
					m_flTeamplayStartTime[nSplitScreenPlayer] = gpGlobals->curtime;
				}				
			}
			else
			{
				// player transitioned to no/spectator team, clear the teamplay start time
				m_flTeamplayStartTime[nSplitScreenPlayer] = 0;
			}			
		}		
	}
	else if ( 0 == Q_strcmp( name, "teamplay_round_start" ) )
	{
		if ( event->GetBool( "full_reset" ) )
		{
			// we're starting a full round, clear miniround count
			m_iMiniroundsCompleted[nSplitScreenPlayer] = 0;
		}
	}
	else if ( 0 == Q_strcmp( name, "teamplay_round_win" ) )
	{
		if ( false == event->GetBool( "full_round", true ) )
		{
			// we just finished a miniround but the round is continuing, increment miniround count
			m_iMiniroundsCompleted[nSplitScreenPlayer]++;
		}
	}
	else if ( 0 == Q_strcmp( name, "player_stats_updated" ) )
	{
		FOR_EACH_MAP( m_mapAchievement[nSplitScreenPlayer], i )
		{
			CBaseAchievement *pAchievement = m_mapAchievement[nSplitScreenPlayer][i];
			pAchievement->OnPlayerStatsUpdate( nSplitScreenPlayer );
		}
	}
	else if ( 0 == Q_strcmp( name, "read_game_titledata" ) )
	{
		SyncAchievementsToTitleData( event->GetInt( "controllerId" ), ACHIEVEMENT_READ_ACHIEVEMENT );
#if defined ( _X360 )
		IGameEvent * repostEvent = gameeventmanager->CreateEvent( "repost_xbox_achievements" );
		if ( repostEvent )
		{
			int userSlot = XBX_GetSlotByUserId( event->GetInt( "controllerId" ) );
			if ( userSlot != -1 )
			{
				repostEvent->SetInt( "splitscreenplayer", userSlot );
				gameeventmanager->FireEventClientSide( repostEvent );
			}
		}
#endif
	}
	else if ( 0 == Q_strcmp( name, "write_game_titledata" ) )
	{
		SyncAchievementsToTitleData( event->GetInt( "controllerId" ), ACHIEVEMENT_WRITE_ACHIEVEMENT );
	}
	else if ( 0 == Q_strcmp( name, "reset_game_titledata" ) )
	{
		ClearAchievements( XBX_GetSlotByUserId( event->GetInt( "controllerId" ) ) );
	}
	else if ( 0 == Q_strcmp( name, "write_profile_data" ) )
	{
		for ( int i=0; i<XUSER_MAX_COUNT; ++i )
		{
			char cmdLine[80];
			Q_snprintf( cmdLine, 80, "host_writeconfig_ss %d", i );
			engine->ClientCmd_Unrestricted( cmdLine );
		}
	}
	else if ( 0 == Q_strcmp( name, "achievement_write_failed" ) )
	{
#ifdef _GAMECONSOLE
		// We didn't succeed and we're not waiting, so we failed
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnProfileUnavailable", "iController", XBX_GetUserId( nSplitScreenPlayer ) ) );
#endif
	}
	else if ( 0 == Q_strcmp( name, "user_data_downloaded" ) )
	{
#ifndef NO_STEAM
		UpdateStateFromSteam_Internal( nSplitScreenPlayer );
#endif
	}

#endif // CLIENT_DLL
}

//-----------------------------------------------------------------------------
// Purpose: called when a player or character has been killed
//-----------------------------------------------------------------------------
void CAchievementMgr::OnKillEvent( CBaseEntity *pVictim, CBaseEntity *pAttacker, CBaseEntity *pInflictor, IGameEvent *event )
{
#ifdef CLIENT_DLL
	int nSplitScreenPlayer = SINGLE_PLAYER_SLOT;
	nSplitScreenPlayer = event->GetInt( "splitscreenplayer" );
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSplitScreenPlayer );
#endif

	// can have a NULL victim on client if victim has never entered local player's PVS
	if ( !pVictim )
		return;

	// if single-player game, calculate if the attacker is the local player and if the victim is the player enemy
	bool bAttackerIsPlayer = false;
	bool bVictimIsPlayerEnemy = false;
#ifdef GAME_DLL
	if ( !g_pGameRules->IsMultiplayer() )
	{
		CBasePlayer *pLocalPlayer = UTIL_GetLocalPlayer();
		if ( pLocalPlayer )
		{
			if ( pAttacker == pLocalPlayer )
			{
				bAttackerIsPlayer = true;
			}

			CBaseCombatCharacter *pBCC = ToBaseCombatCharacter( pVictim );
			if ( pBCC && ( D_HT == pBCC->IRelationType( pLocalPlayer ) ) )
			{
				bVictimIsPlayerEnemy = true;
			}
		}
	}
#else
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	bVictimIsPlayerEnemy = !pLocalPlayer->InSameTeam( pVictim );
	if ( pAttacker == pLocalPlayer )
	{
		bAttackerIsPlayer = true;
	}
#endif // GAME_DLL

	for ( int j = 0; j < MAX_SPLITSCREEN_PLAYERS; ++j )
	{
		// look through all the kill event listeners and notify any achievements whose filters we pass
		FOR_EACH_VEC( m_vecKillEventListeners[j], iAchievement )
		{
			CBaseAchievement *pAchievement = m_vecKillEventListeners[j][iAchievement];

			if ( !pAchievement->IsActive() )
				continue;

			// if this achievement only looks for kills where attacker is player and that is not the case here, skip this achievement
			if ( ( pAchievement->GetFlags() & ACH_FILTER_ATTACKER_IS_PLAYER ) && !bAttackerIsPlayer )
				continue;

			// if this achievement only looks for kills where victim is killer enemy and that is not the case here, skip this achievement
			if ( ( pAchievement->GetFlags() & ACH_FILTER_VICTIM_IS_PLAYER_ENEMY ) && !bVictimIsPlayerEnemy )
				continue;

#if GAME_DLL
			// if this achievement only looks for a particular victim class name and this victim is a different class, skip this achievement
			const char *pVictimClassNameFilter = pAchievement->m_pVictimClassNameFilter;
			if ( pVictimClassNameFilter && !pVictim->ClassMatches( pVictimClassNameFilter ) )
				continue;

			// if this achievement only looks for a particular inflictor class name and this inflictor is a different class, skip this achievement
			const char *pInflictorClassNameFilter = pAchievement->m_pInflictorClassNameFilter;
			if ( pInflictorClassNameFilter &&  ( ( NULL == pInflictor ) || !pInflictor->ClassMatches( pInflictorClassNameFilter ) ) )
				continue;

			// if this achievement only looks for a particular attacker class name and this attacker is a different class, skip this achievement
			const char *pAttackerClassNameFilter = pAchievement->m_pAttackerClassNameFilter;
			if ( pAttackerClassNameFilter && ( ( NULL == pAttacker ) || !pAttacker->ClassMatches( pAttackerClassNameFilter ) ) )
				continue;

			// if this achievement only looks for a particular inflictor entity name and this inflictor has a different name, skip this achievement
			const char *pInflictorEntityNameFilter = pAchievement->m_pInflictorEntityNameFilter;
			if ( pInflictorEntityNameFilter && ( ( NULL == pInflictor ) || !pInflictor->NameMatches( pInflictorEntityNameFilter ) ) )
				continue;
#endif // GAME_DLL

			// we pass all filters for this achievement, notify the achievement of the kill
			pAchievement->Event_EntityKilled( pVictim, pAttacker, pInflictor, event );
		}
	}
}

void CAchievementMgr::OnAchievementEvent( int iAchievementID, int nUserSlot )
{
	// handle event for specific achievement
	CBaseAchievement *pAchievement = GetAchievementByID( iAchievementID, nUserSlot );
	Assert( pAchievement );
	if ( pAchievement )
	{
		if ( !pAchievement->IsAchieved() )
		{
			pAchievement->IncrementCount();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when a map-fired achievement event occurs
//-----------------------------------------------------------------------------
void CAchievementMgr::OnMapEvent( const char *pchEventName, int nUserSlot )
{
	Assert( pchEventName && *pchEventName );
	if ( !pchEventName || !*pchEventName ) 
		return;

	// see if this event matches the prefix for an achievement component
	FOR_EACH_VEC( m_vecComponentListeners[nUserSlot], iAchievement )
	{
		CBaseAchievement *pAchievement = m_vecComponentListeners[nUserSlot][iAchievement];
		Assert( pAchievement->m_pszComponentPrefix );
		if ( 0 == Q_strncmp( pchEventName, pAchievement->m_pszComponentPrefix, pAchievement->m_iComponentPrefixLen ) )
		{
			// prefix matches, tell the achievement a component was found
			pAchievement->OnComponentEvent( pchEventName );
			return;
		}
	}

	// look through all the map event listeners
	FOR_EACH_VEC( m_vecMapEventListeners[nUserSlot], iAchievement )
	{
		CBaseAchievement *pAchievement = m_vecMapEventListeners[nUserSlot][iAchievement];
		pAchievement->OnMapEvent( pchEventName );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Returns an achievement as it's abstract object. This interface is used by gameui.dll for getting achievement info.
// Input  : index - 
// Output : IBaseAchievement*
//-----------------------------------------------------------------------------
IAchievement* CAchievementMgr::GetAchievementByIndex( int index, int nUserSlot )
{
	Assert( m_vecAchievement[nUserSlot].IsValidIndex(index) );
	return (IAchievement*)m_vecAchievement[nUserSlot][index];
}


//-----------------------------------------------------------------------------
// Purpose: Returns an achievement as it's abstract object. This interface is used by gameui.dll for getting achievement info.
// Input  : orderIndex - 
// Output : IBaseAchievement*
//-----------------------------------------------------------------------------
IAchievement* CAchievementMgr::GetAchievementByDisplayOrder( int orderIndex, int nUserSlot )
{
	Assert( m_vecAchievementInOrder[nUserSlot].IsValidIndex(orderIndex) );
	return (IAchievement*)m_vecAchievementInOrder[nUserSlot][orderIndex];
}


//-----------------------------------------------------------------------------
// Purpose: Returns an asset award achievement as it's abstract object.
// Input  : orderIndex - 
// Output : IBaseAchievement*
//-----------------------------------------------------------------------------
IAchievement* CAchievementMgr::GetAwardByDisplayOrder( int orderIndex, int nUserSlot )
{
	Assert( m_vecAwardInOrder[nUserSlot].IsValidIndex(orderIndex) );
	return (IAchievement*)m_vecAwardInOrder[nUserSlot][orderIndex];
}


//-----------------------------------------------------------------------------
// Purpose: Returns total achievement count. This interface is used by gameui.dll for getting achievement info.
// Input  :  - 
// Output : Count of achievements in manager's vector.
//-----------------------------------------------------------------------------
int CAchievementMgr::GetAchievementCount( bool bAssets )
{
	int listCount = m_mapAchievement[SINGLE_PLAYER_SLOT].Count();
	int achCount = 0;
	for ( int i=0; i<listCount; ++i )
	{
		CBaseAchievement *pAchievement = m_mapAchievement[STEAM_PLAYER_SLOT][i];
		if ( (bAssets && pAchievement->IsAssetAward()) || (!bAssets && !pAchievement->IsAssetAward()) )
		{
			achCount++;
		}
	}
	return achCount;
}

//-----------------------------------------------------------------------------
// Purpose: Handles events from the matchmaking framework.
//-----------------------------------------------------------------------------
void CAchievementMgr::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( FStrEq( szEvent, "OnProfileDataLoaded" ) )
	{
		// This event is sent when the title data blocks have been loaded.
		int iController = pEvent->GetInt( "iController" );
#ifdef _GAMECONSOLE
		int nSlot = XBX_GetSlotByUserId( iController );
#else
		int nSlot = STEAM_PLAYER_SLOT;
#endif
		ReadAchievementsFromTitleData( iController, nSlot );
	}
	else if ( FStrEq( szEvent, "sv_cheats_changed" ) )
	{	// we got a local event that sv_cheats value changed!
		if ( pEvent->GetInt( "value" ) )
			m_bCheatsEverOn = true;
	}
#ifdef _GAMECONSOLE
	else if ( FStrEq( szEvent, "OnProfilesChanged" ) )
	{
		// This is essentially a RESET
		for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
		{
			UserDisconnected( i );
		}

		// Mark the valid users as connected and try to download achievement data from LIVE
		for ( unsigned int i = 0; i < XBX_GetNumGameUsers(); ++i )
		{
			UserConnected( i );  
		}
	}
#endif
}

#if !defined(NO_STEAM)
//-----------------------------------------------------------------------------
// Purpose: called when stat upload is complete
// this needs to handle k_EResultInvalidParam which means that steam has rejected our uploaded stats
// and reset them with its copy. at that point we need to update our own copy of the stats to reflect steam's
//-----------------------------------------------------------------------------
void CAchievementMgr::Steam_OnUserStatsStored( UserStatsStored_t *pUserStatsStored )
{
#if !defined(GAME_DLL) && defined(USE_CEG)
	Steamworks_TestSecret() ; 
#endif
	if ( k_EResultOK != pUserStatsStored->m_eResult )
	{
		DevMsg( "CAchievementMgr: Failed to upload stats to Steam, EResult %d!\n", pUserStatsStored->m_eResult );
	} 
	else
	{
#if defined( CLIENT_DLL )
		UpdateStateFromSteam_Internal(STEAM_PLAYER_SLOT);

		// send a message to the server about our achievements
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		{
			C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer( FirstValidSplitScreenSlot() );
			if ( pLocalPlayer )
			{
				while ( m_AchievementsAwarded[STEAM_PLAYER_SLOT].Count() > 0 )
				{
					int nAchievementID = m_AchievementsAwarded[STEAM_PLAYER_SLOT].Head();
					CBaseAchievement* pAchievement = GetAchievementByID( nAchievementID, STEAM_PLAYER_SLOT );

					// verify that it is still achieved (it could have been rejected by Steam)
					if ( pAchievement->IsAchieved() )
					{
						IGameEvent * event = gameeventmanager->CreateEvent( "achievement_earned" );
						if ( event )
						{
							event->SetInt( "player", pLocalPlayer->entindex() );
							event->SetInt( "achievement", nAchievementID );
							gameeventmanager->FireEventClientSide( event );
						}
					}
					m_AchievementsAwarded[STEAM_PLAYER_SLOT].RemoveMultipleFromHead(1);
				}
			}
		}
#endif			

		// for each achievement that has not been achieved
		FOR_EACH_MAP( m_mapAchievement[STEAM_PLAYER_SLOT], iAchievement )
		{
			CBaseAchievement *pAchievement = m_mapAchievement[STEAM_PLAYER_SLOT][iAchievement];

			if ( !pAchievement->IsAchieved() )
			{
				pAchievement->OnSteamUserStatsStored();
			}
		}
	}
}
#endif // !defined(NO_STEAM)

bool CAchievementMgr::SyncAchievementsToTitleData( int iController, SyncAchievementValueDirection_t eOp )
{
#if defined (_X360)

	// get the local player
	IPlayerLocal *pPlayerLocal = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iController );
	if ( !pPlayerLocal )
		return false;

	TitleDataFieldsDescription_t const *pFields = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();

	
	// check version number
	TitleDataFieldsDescription_t const *versionField = TitleDataFieldsDescriptionFindByString( pFields, "TITLEDATA.BLOCK2.VERSION" );
	if ( !versionField || versionField->m_eDataType != TitleDataFieldsDescription_t::DT_uint16 )
	{
		Warning( "TITLEDATA.BLOCK2.VERSION is expected to be defined as DT_uint16\n" );
		return false;
	}

	ConVarRef cl_titledataversionblock2( "cl_titledataversionblock2" );
	if ( eOp == ACHIEVEMENT_READ_ACHIEVEMENT )
	{
		int versionNumber = TitleDataFieldsDescriptionGetValue<uint16>( versionField, pPlayerLocal );
		if ( versionNumber != cl_titledataversionblock2.GetInt() )
		{
			Warning( "SyncAchievementsToTitleData incorrect verion #; got %d, expected %d\n", versionNumber, cl_titledataversionblock2.GetInt() );
			return false;
		}
	}
	else
	{
		TitleDataFieldsDescriptionSetValue<uint16>( versionField, pPlayerLocal,cl_titledataversionblock2.GetInt() );
	}

	bool bIsAchieved;
	uint8 iochar;
	char achName[ 256 ];
	uint32 ioint;

	int userSlot = XBX_GetSlotByUserId( iController );

	Assert(userSlot < MAX_SPLITSCREEN_PLAYERS);

	FOR_EACH_MAP( m_mapAchievement[userSlot], i )
	{
		CBaseAchievement *pAchievement = m_mapAchievement[userSlot][i];
		Q_snprintf( achName, 255, "MEDALS.AWARDED%.3d", i );
		TitleDataFieldsDescription_t const *pFieldAwarded = TitleDataFieldsDescriptionFindByString( pFields, achName );
		Q_snprintf( achName, 255, "MEDALS.MEDALINFO%.3d", i );
		TitleDataFieldsDescription_t const *pFieldMedalInfo = TitleDataFieldsDescriptionFindByString( pFields, achName );

		if ( !pFieldAwarded || !pFieldMedalInfo )
		{
			continue;
		}

		if ( eOp == ACHIEVEMENT_WRITE_ACHIEVEMENT )
		{
			bIsAchieved = pAchievement->IsAchieved();

			iochar = 0;
			ioint = pAchievement->GetCount();

			if ( bIsAchieved )
			{
				iochar = 2;
				ioint = pAchievement->GetUnlockTime();
			}

			TitleDataFieldsDescriptionSetValue<uint8>( pFieldAwarded, pPlayerLocal, iochar );
			TitleDataFieldsDescriptionSetValue<uint32>( pFieldMedalInfo, pPlayerLocal, ioint );
		}
		else
		{
			bIsAchieved = static_cast< bool >( TitleDataFieldsDescriptionGetValue<uint8>( pFieldAwarded, pPlayerLocal ) != 0 );
			ioint = TitleDataFieldsDescriptionGetValue<uint32>( pFieldMedalInfo, pPlayerLocal );
			if ( bIsAchieved )
			{
				pAchievement->SetUnlockTime( ioint );
				pAchievement->SetAchieved( true );
			}
			else
			{
				pAchievement->SetAchieved( false );
				pAchievement->SetCount( ioint );
			}
		}
	}

	if ( eOp == ACHIEVEMENT_READ_ACHIEVEMENT )
	{
		IGameEvent * event = gameeventmanager->CreateEvent( "achievement_info_loaded" );
		if ( event )
		{
			gameeventmanager->FireEventClientSide( event );
		}
	}

#endif
	return true;
}

void CAchievementMgr::ResetAchievement_Internal( CBaseAchievement *pAchievement )
{
#if defined( CLIENT_DLL )
	Assert( pAchievement );

#if !defined ( NO_STEAM )
	if ( steamapicontext->SteamUserStats() )
	{
		steamapicontext->SteamUserStats()->ClearAchievement( pAchievement->GetName() );		
	}
#endif

	pAchievement->SetAchieved( false );
	pAchievement->SetCount( 0 );	
	if ( pAchievement->HasComponents() )
	{
		pAchievement->SetComponentBits( 0 );
	}
	pAchievement->SetProgressShown( 0 );
	pAchievement->StopListeningForAllEvents();
	if ( pAchievement->IsActive() )
	{
		pAchievement->ListenForEvents();
	}
#endif // CLIENT_DLL
}

#ifdef CLIENT_DLL

bool MsgFunc_AchievementEvent( const CCSUsrMsg_AchievementEvent &msg )
{
	int iAchievementID = (int) msg.achievement();
	CAchievementMgr *pAchievementMgr = CAchievementMgr::GetInstance();
	if ( !pAchievementMgr )
		return true;

	int iCount = (int) msg.count();
	int userID = (int) msg.user_id();

	// Fix unused variable warning
	NOTE_UNUSED(iCount);

	int userSlot = STEAM_PLAYER_SLOT;

#if defined ( _X360 )
	for ( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer(i);
		if ( pLocalPlayer && !pLocalPlayer->IsNPC() )
		{
			if ( pLocalPlayer->GetUserID() == userID )
			{
				userSlot = i;
			}
		}
	}
#else
	NOTE_UNUSED(userID);
#endif // _X360

	pAchievementMgr->OnAchievementEvent( iAchievementID, userSlot );

	return true;
}

#if ALLOW_ACHIEVEMENTS_WITH_CHEATS
CON_COMMAND_F( achievement_reset_all, "Clears all achievements", FCVAR_DEVELOPMENTONLY )
{
	CAchievementMgr *pAchievementMgr = CAchievementMgr::GetInstance();
	if ( !pAchievementMgr )
		return;
	pAchievementMgr->ResetAchievements();
}

int StringSortFunc( const void *p1, const void *p2 ) 
{
	const char *psz1 = (const char *)p1;
	const char *psz2 = (const char *)p2;

	return ( Q_strcmp( psz1, psz2 ) );
}

static int AchievementNameCompletion( const char *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	CAchievementMgr *pAchievementMgr = CAchievementMgr::GetInstance();
	if ( !pAchievementMgr )
		return 0;

	const char *firstSpace = V_strstr( partial, " " );
	if ( !firstSpace )
		return 0;

	// store the command
	char commandName[COMMAND_COMPLETION_ITEM_LENGTH];
	int commandLength = firstSpace - partial;
	V_StrSlice( partial, 0, commandLength, commandName, sizeof( commandName ) );

	// skip the command
	partial += commandLength + 1;
	int partialLength = Q_strlen( partial );

	if ( !partial )
		return 0;

	int numMatches = 0;

	CUtlMap< int, CBaseAchievement *> &achievements = pAchievementMgr->GetAchievements( STEAM_PLAYER_SLOT );

	for ( int i = achievements.FirstInorder(); i != achievements.InvalidIndex() && numMatches < COMMAND_COMPLETION_MAXITEMS; i = achievements.NextInorder( i ) )
	{
		CBaseAchievement *pAchievement = achievements[i];

		const char *pszAchievementName = pAchievement->GetName();

		if ( !Q_strncasecmp( pszAchievementName, partial, partialLength ) )
		{
			Q_snprintf( commands[ numMatches ], sizeof( commands[ numMatches ] ), "%s %s", commandName, pszAchievementName );
			numMatches++;
		}
	}

	// sort the commands
	qsort( commands, numMatches, COMMAND_COMPLETION_ITEM_LENGTH, StringSortFunc );

	return numMatches;
}

CON_COMMAND_F_COMPLETION( achievement_reset, "<internal name> Clears specified achievement", FCVAR_DEVELOPMENTONLY, AchievementNameCompletion )
{
	CAchievementMgr *pAchievementMgr = CAchievementMgr::GetInstance();
	if ( !pAchievementMgr )
		return;

	if ( 2 != args.ArgC() )
	{
		Msg( "Usage: achievement_reset <internal name>\n" );
		return;
	}
	CBaseAchievement *pAchievement = pAchievementMgr->GetAchievementByName( args[1], STEAM_PLAYER_SLOT );
	if ( !pAchievement )
	{
		Msg( "Achievement %s not found\n", args[1] );
		return;
	}
	pAchievementMgr->ResetAchievement( pAchievement->GetAchievementID() );

}

CON_COMMAND_F( achievement_status, "Shows status of all achievement", FCVAR_DEVELOPMENTONLY )
{
	CAchievementMgr *pAchievementMgr = CAchievementMgr::GetInstance();
	if ( !pAchievementMgr )
		return;
	pAchievementMgr->PrintAchievementStatus();
}

CON_COMMAND_F_COMPLETION( achievement_unlock, "<internal name> Unlocks achievement", FCVAR_DEVELOPMENTONLY, AchievementNameCompletion )
{
	CAchievementMgr *pAchievementMgr = CAchievementMgr::GetInstance();
	if ( !pAchievementMgr )
		return;

	if ( 2 != args.ArgC() )
	{
		Msg( "Usage: achievement_unlock <internal name>\n" );
		return;
	}
	CBaseAchievement *pAchievement = pAchievementMgr->GetAchievementByName( args[1], STEAM_PLAYER_SLOT );
	if ( !pAchievement )
	{
		Msg( "Achievement %s not found\n", args[1] );
		return;
	}
	pAchievementMgr->AwardAchievement( pAchievement->GetAchievementID(), STEAM_PLAYER_SLOT );
}

CON_COMMAND_F( achievement_unlock_all, "Unlocks all achievements", FCVAR_DEVELOPMENTONLY )
{
	CAchievementMgr *pAchievementMgr = CAchievementMgr::GetInstance();
	if ( !pAchievementMgr )
		return;

	int iCount = pAchievementMgr->GetAchievementCount();
	for ( int i = 0; i < iCount; i++ )
	{
		IAchievement *pAchievement = pAchievementMgr->GetAchievementByIndex( i, STEAM_PLAYER_SLOT );
		if ( !pAchievement->IsAchieved() )
		{
			pAchievementMgr->AwardAchievement( pAchievement->GetAchievementID(), STEAM_PLAYER_SLOT );
		}
	}	
}

CON_COMMAND_F_COMPLETION( achievement_evaluate, "<internal name> Causes failable achievement to be evaluated", FCVAR_DEVELOPMENTONLY, AchievementNameCompletion )
{
	CAchievementMgr *pAchievementMgr = CAchievementMgr::GetInstance();
	if ( !pAchievementMgr )
		return;

	if ( 2 != args.ArgC() )
	{
		Msg( "Usage: achievement_evaluate <internal name>\n" );
		return;
	}
	CBaseAchievement *pAchievement = pAchievementMgr->GetAchievementByName( args[1], STEAM_PLAYER_SLOT );
	if ( !pAchievement )
	{
		Msg( "Achievement %s not found\n", args[1] );
		return;
	}

	CFailableAchievement *pFailableAchievement = dynamic_cast<CFailableAchievement *>( pAchievement );
	Assert( pFailableAchievement );
	if ( pFailableAchievement )
	{
		pFailableAchievement->OnEvaluationEvent();
	}
}

CON_COMMAND_F( achievement_test_friend_count, "Counts the # of teammates on local player's friends list", FCVAR_DEVELOPMENTONLY )
{
	CAchievementMgr *pAchievementMgr = CAchievementMgr::GetInstance();
	if ( !pAchievementMgr )
		return;
	if ( 2 != args.ArgC() )
	{
		Msg( "Usage: achievement_test_friend_count <min # of teammates>\n" );
		return;
	}
	int iMinFriends = atoi( args[1] );
	bool bRet = CalcPlayersOnFriendsList( iMinFriends );
	Msg( "You %s have at least %d friends in the game.\n", bRet ? "do" : "do not", iMinFriends );
}

CON_COMMAND_F( achievement_test_clan_count, "Determines if specified # of teammates belong to same clan w/local player", FCVAR_DEVELOPMENTONLY )
{
	CAchievementMgr *pAchievementMgr = CAchievementMgr::GetInstance();
	if ( !pAchievementMgr )
		return;

	if ( 2 != args.ArgC() )
	{
		Msg( "Usage: achievement_test_clan_count <# of clan teammates>\n" );
		return;
	}
	int iClanPlayers = atoi( args[1] );
	bool bRet = CalcHasNumClanPlayers( iClanPlayers );
	Msg( "There %s %d players who you're in a Steam group with.\n", bRet ? "are" : "are not", iClanPlayers );
}

CON_COMMAND_F( achievement_mark_dirty, "Mark achievement data as dirty", FCVAR_DEVELOPMENTONLY )
{
	CAchievementMgr *pAchievementMgr = CAchievementMgr::GetInstance();
	if ( !pAchievementMgr )
		return;
	pAchievementMgr->SetDirty( true, STEAM_PLAYER_SLOT );
}
#endif // _DEBUG

#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: helper function to get entity model name
//-----------------------------------------------------------------------------
const char *GetModelName( CBaseEntity *pBaseEntity )
{
	CBaseAnimating *pBaseAnimating = dynamic_cast<CBaseAnimating *>( pBaseEntity );
	if ( pBaseAnimating )
	{
		CStudioHdr *pStudioHdr = pBaseAnimating->GetModelPtr();
		if ( pStudioHdr )
		{
			return pStudioHdr->pszName();
		}
	}

	return "";
}

//-----------------------------------------------------------------------------
// Purpose: Gets the list of achievements achieved during the current game
//-----------------------------------------------------------------------------
const CUtlVector<int>& CAchievementMgr::GetAchievedDuringCurrentGame( int nPlayerSlot )
{
	return m_AchievementsAwardedDuringCurrentGame[nPlayerSlot];
}

//-----------------------------------------------------------------------------
// Purpose: Reset the list of achievements achieved during the current game
//-----------------------------------------------------------------------------
void CAchievementMgr::ResetAchievedDuringCurrentGame( int nPlayerSlot )
{
	m_AchievementsAwardedDuringCurrentGame[nPlayerSlot].RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Clears achievement data for the a particular user slot
//-----------------------------------------------------------------------------
void CAchievementMgr::ClearAchievementData( int nUserSlot )
{
	Assert(nUserSlot < MAX_SPLITSCREEN_PLAYERS);

	FOR_EACH_MAP_FAST( m_mapAchievement[nUserSlot], i )
	{
		m_mapAchievement[nUserSlot][i]->ClearAchievementData();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Do per-frame handling
//-----------------------------------------------------------------------------
void CAchievementMgr::Update( float frametime )
{
#ifdef CLIENT_DLL
	if ( !sv_cheats )
	{
		sv_cheats = cvar->FindVar( "sv_cheats" );
	}
#endif

#ifndef _DEBUG
	// keep track if cheats have ever been turned on during this level
	if ( !WereCheatsEverOn() )
	{
		if ( sv_cheats && sv_cheats->GetBool() )
		{
			m_bCheatsEverOn = true;
		}
	}
#endif

	// [sbodenbender] brought over from orange box
	// Call think functions. Work backwards, because we may remove achievements from the list.
	int iCount = m_vecThinkListeners.Count();
	for ( int i = iCount-1; i >= 0; i-- )
	{
		if ( m_vecThinkListeners[i].m_flThinkTime < gpGlobals->curtime )
		{
			m_vecThinkListeners[i].pAchievement->Think();

			// The think function may have pushed out the think time. If not, remove ourselves from the list.
			if ( m_vecThinkListeners[i].pAchievement->IsAchieved() || m_vecThinkListeners[i].m_flThinkTime < gpGlobals->curtime )
			{
				m_vecThinkListeners.Remove(i);
			}
		}
	}

#ifdef _X360
	bool bWarningShown = false;
	for ( int i = m_pendingAchievementState.Count()-1; i >= 0; i-- )	// Iterate backwards to make deletion safe
	{
		// Check for a pending achievement write
		uint nResultCode;
		int nReturn = xboxsystem->GetOverlappedResult( m_pendingAchievementState[i].pOverlappedResult, &nResultCode, false );
		if ( nReturn == ERROR_IO_PENDING || nReturn == ERROR_IO_INCOMPLETE )
			continue;

		// We are attempting to grant an achievement.
		if ( nReturn != ERROR_SUCCESS )
		{
			// The achievement write has failed.
			if ( bWarningShown == false )
			{
				// Create a game message to pop up a warning to the user
				IGameEvent *event = gameeventmanager->CreateEvent( "achievement_write_failed" );
				if ( event )
				{
					gameeventmanager->FireEvent( event );
					bWarningShown = true;
				}
			}

			// We need to unaward the achievement in this case!
			CBaseAchievement *pAchievement = GetAchievementByID( m_pendingAchievementState[i].nAchievementID, m_pendingAchievementState[i].nUserSlot );
			if ( pAchievement != NULL )
			{
				pAchievement->SetAchieved( false );
				m_bDirty[m_pendingAchievementState[i].nUserSlot] = true;
				m_AchievementsAwardedDuringCurrentGame->FindAndRemove( m_pendingAchievementState[i].nAchievementID );
				// FIXME: This doesn't account for incremental progress, but if *will* re-achieve these if you get them again
			}
		}

		// We've either succeeded or failed at this point, in both cases we don't care anymore!
		xboxsystem->ReleaseAsyncHandle( m_pendingAchievementState[i].pOverlappedResult );
		m_pendingAchievementState.FastRemove( i );
	}
#endif // _X360
}


bool CAchievementMgr::IsCurrentMap( const char *pszMapName )
{
	if ( !pszMapName )
		return false;

	return StringHasPrefixCaseSensitive( GetMapName(), pszMapName );
}
