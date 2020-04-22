//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_title_richpresence.h"
#include "portal2.spa.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//
// Mapping of context values
//

ContextValue_t g_pcv_CONTEXT_GAME_MODE[] = {
#define CFG( gamemode ) { #gamemode, CONTEXT_GAME_MODE_##gamemode },
#include "inc_gamemode.inc"
#undef CFG
	{ NULL,				0xFFFF },
};

struct MpCoopMapRichPresence_t
{
	char const *szMapName;
	DWORD dwCtxValue;
	int idxChapter;
	int numChapters;
}
g_pcv_CONTEXT_COOP_PRESENCE_TRACK[] = {
#define CFG( mpcoopmap, ctxval, idx, num ) { #mpcoopmap, ctxval, idx, num },
#include "inc_coop_maps.inc"
#undef CFG
	{ NULL, 0, 0, 0 },
};

ContextValue_t g_pcv_CONTEXT_SP_PRESENCE_TEXT[] = {
#define CFG( spmapname, chapternum, subchapter ) { #spmapname, CONTEXT_SP_PRESENCE_TEXT_CH##chapternum },
#include "inc_sp_maps.inc"
#undef CFG
	{ NULL, CONTEXT_SP_PRESENCE_TEXT_DEFAULT },
};

static MpCoopMapRichPresence_t const * FindMpCoopMapRichPresence( char const *szMapName )
{
	MpCoopMapRichPresence_t const *p = g_pcv_CONTEXT_COOP_PRESENCE_TRACK;
	for ( ; p->szMapName; ++ p )
	{
		if ( !Q_stricmp( p->szMapName, szMapName ) )
			return p;
	}
	return p;
}


//
// User context and property setting
//

static void SetAllUsersContext( DWORD dwContextId, DWORD dwValue, bool bAsync = true )
{
#ifdef _X360
	for ( int k = 0; k < ( int ) XBX_GetNumGameUsers(); ++ k )
	{
		if ( XBX_GetUserIsGuest( k ) )
			continue;
		int iCtrlr = XBX_GetUserId( k );
		if ( bAsync )
			XUserSetContextEx( iCtrlr, dwContextId, dwValue, MMX360_NewOverlappedDormant() );
		else
			XUserSetContext( iCtrlr, dwContextId, dwValue );
	}
#endif
}

static void SetAllUsersProperty( DWORD dwPropertyId, DWORD cbValue, void const *pvValue )
{
#ifdef _X360
	for ( int k = 0; k < ( int ) XBX_GetNumGameUsers(); ++ k )
	{
		if ( XBX_GetUserIsGuest( k ) )
			continue;
		int iCtrlr = XBX_GetUserId( k );
		XUserSetPropertyEx( iCtrlr, dwPropertyId, cbValue, pvValue, MMX360_NewOverlappedDormant() );
	}
#endif
}

KeyValues * MM_Title_RichPresence_PrepareForSessionCreate( KeyValues *pSettings )
{
	if ( char const *szValue = pSettings->GetString( "game/mode", NULL ) )
	{
		SetAllUsersContext( X_CONTEXT_GAME_MODE, g_pcv_CONTEXT_GAME_MODE->ScanValues( szValue ), false );
	}

	// matchmaking version
	{
		static int val; // must be valid for the async call
		extern ConVar mm_matchmaking_version;
		val = mm_matchmaking_version.GetInt();
		SetAllUsersProperty( PROPERTY_MMVERSION, sizeof( val ), &val );
		DevMsg( "PrepareForSessionCreate: matchmaking version %d\n", val );
	}

	return NULL;
}

void MM_Title_RichPresence_Update( KeyValues *pFullSettings, KeyValues *pUpdatedSettings )
{
	if ( !pFullSettings )
	{
		SetAllUsersContext( X_CONTEXT_PRESENCE, 1 ); // main menu
		return;
	}

	// Also set players information during initial rich presence update
	if ( !pUpdatedSettings && pFullSettings )
	{
		MM_Title_RichPresence_PlayersChanged( pFullSettings );
	}

	// pUpdatedSettings = NULL when the session is created and all contexts need to be set
	KeyValues *pNewSettings = pUpdatedSettings ? pUpdatedSettings : pFullSettings;

	if ( char const *szValue = pNewSettings->GetString( "game/mode", NULL ) )
	{
		SetAllUsersContext( X_CONTEXT_GAME_MODE, g_pcv_CONTEXT_GAME_MODE->ScanValues( szValue ) );
	}

	if ( KeyValues *kvVal = pNewSettings->FindKey( "game/dlcrequired" ) )
	{
		static int val[10]; // must be valid for the async call
		uint64 uiDlcRequired = kvVal->GetUint64();
		extern ConVar mm_matchmaking_dlcsquery;
		for ( int k = 1; k <= mm_matchmaking_dlcsquery.GetInt(); ++ k )
		{
			val[k] = !!( uiDlcRequired & ( 1ull << k ) );
			DevMsg( "DLC%d required: %d\n", k, val[k] );
			SetAllUsersProperty( PROPERTY_REQUIRED_DLC1 - 1 + k, sizeof( val ), &val );
		}
	}

	//
	// Determine Rich Presence Display
	//
	if ( char const *szGameModeForRichPresence = pFullSettings->GetString( "game/mode", NULL ) )
	{
		unsigned int dwLevelPresence = CONTEXT_PRESENCE_MAINMENU;

		if ( !Q_stricmp( szGameModeForRichPresence, "sp" ) )
		{
			//
			// Game chapter
			//
			static char s_chLastMapNameSet[128] = {0};
			char const *szMap = pFullSettings->GetString( "Game/map" );
			if ( Q_stricmp( szMap, s_chLastMapNameSet ) )
			{
				Q_strncpy( s_chLastMapNameSet, szMap, sizeof( s_chLastMapNameSet ) );
				SetAllUsersContext( CONTEXT_SP_PRESENCE_TEXT, g_pcv_CONTEXT_SP_PRESENCE_TEXT->ScanValues( s_chLastMapNameSet ) );
			}
			
			dwLevelPresence = CONTEXT_PRESENCE_SP;
		}
		else if ( !Q_stricmp( szGameModeForRichPresence, "coop" ) || !Q_stricmp( szGameModeForRichPresence, "coop_challenge" ) )
		{
			//
			// Game type: splitscreen / friends /quickmatch
			//
			DWORD dwGameType = CONTEXT_COOP_PRESENCE_TAGLINE_DEFAULT;
			if ( XBX_GetNumGameUsers() > 1 )
				dwGameType = CONTEXT_COOP_PRESENCE_TAGLINE_SPLITSCREEN;
			else if ( !Q_stricmp( "lan", pFullSettings->GetString( "system/network" ) ) )
				dwGameType = CONTEXT_COOP_PRESENCE_TAGLINE_SYSTEMLINK;
			else if ( !Q_stricmp( "friends", pFullSettings->GetString( "game/type" ) ) )
				dwGameType = CONTEXT_COOP_PRESENCE_TAGLINE_FRIEND;
			else if ( !Q_stricmp( "quickmatch", pFullSettings->GetString( "game/type" ) ) )
				dwGameType = CONTEXT_COOP_PRESENCE_TAGLINE_QUICKMATCH;

			static DWORD s_dwLastGameTypeSet = CONTEXT_COOP_PRESENCE_TAGLINE_DEFAULT;
			if ( s_dwLastGameTypeSet != dwGameType )
			{
				s_dwLastGameTypeSet = dwGameType;
				SetAllUsersContext( CONTEXT_COOP_PRESENCE_WAITING, dwGameType );
				SetAllUsersContext( CONTEXT_COOP_PRESENCE_TAGLINE, dwGameType );
			}

			//
			// Game track
			//
			static int nNumChapters = 0;
			static int nIdxChapter = 0;
			static char s_chLastMapNameSet[128] = {0};
			char const *szMap = pFullSettings->GetString( "Game/map" );
			if ( Q_stricmp( szMap, s_chLastMapNameSet ) )
			{
				Q_strncpy( s_chLastMapNameSet, szMap, sizeof( s_chLastMapNameSet ) );
				
				// Determine the track
				MpCoopMapRichPresence_t const *pMP = FindMpCoopMapRichPresence( szMap );
				SetAllUsersContext( CONTEXT_COOP_PRESENCE_TRACK, pMP->dwCtxValue );
				nIdxChapter = pMP->idxChapter;
				SetAllUsersProperty( PROPERTY_COOP_TRACK_CHAPTER, sizeof( nIdxChapter ), &nIdxChapter );
				nNumChapters = pMP->numChapters;
				SetAllUsersProperty( PROPERTY_COOP_TRACK_NUMCHAPTERS, sizeof( nNumChapters ), &nNumChapters );
			}

			// Presence
			if ( !Q_stricmp( "game", pFullSettings->GetString( "game/state" ) ) )
			{
				dwLevelPresence = ( nNumChapters > 0 ) ? CONTEXT_PRESENCE_COOPGAME_TRACK : CONTEXT_PRESENCE_COOPGAME;
			}
			else
			{
				dwLevelPresence = CONTEXT_PRESENCE_COOPMENU;
			}
		}

		SetAllUsersContext( X_CONTEXT_PRESENCE, dwLevelPresence );
	}
}

void MM_Title_RichPresence_PlayersChanged( KeyValues *pFullSettings )
{
	/*
	if ( int numPlayers = pFullSettings->GetInt( "members/numPlayers" ) )
	{
		static int val; // must be valid for the async call
		val = numPlayers;
		SetAllUsersProperty( PROPERTY_NUMPLAYERS, sizeof( val ), &val );
	}
	*/

#ifdef _X360
	// Set the installed DLCs masks
	static int val[10]; // must be valid for the async call
	uint64 uiDlcInstalled = g_pMatchFramework->GetMatchSystem()->GetDlcManager()->GetDataInfo()->GetUint64( "@info/installed" );
	extern ConVar mm_matchmaking_dlcsquery;
	for ( int k = 1; k <= mm_matchmaking_dlcsquery.GetInt(); ++ k )
	{
		val[k] = !!( uiDlcInstalled & ( 1ull << k ) );
		DevMsg( "DLC%d installed: %d\n", k, val[k] );
		SetAllUsersProperty( PROPERTY_INSTALLED_DLC1 - 1 + k, sizeof( val[k] ), &val[k] );
	}
#endif
}
