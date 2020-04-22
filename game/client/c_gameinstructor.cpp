//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Client handler implementations for instruction players how to play
//
//=============================================================================//

#include "cbase.h"

#include "c_gameinstructor.h"
#include "c_baselesson.h"
#include "c_keyvalue_saver.h"
#include "filesystem.h"
#include "vprof.h"
#include "ixboxsystem.h"
#include "tier0/icommandline.h"
#include "iclientmode.h"
#include "isaverestore.h"
#include "saverestoretypes.h"

#include "matchmaking/imatchframework.h"
#include "matchmaking/mm_helpers.h"

#if defined( PORTAL2 )
#include "matchmaking/portal2/imatchext_portal2.h"
#endif

#if defined( CSTRIKE15 )
#include "cs_gamerules.h"
#include "matchmaking/cstrike15/imatchext_cstrike15.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


PRECACHE_REGISTER_BEGIN( GLOBAL, GameinstructorIcons )
	PRECACHE( KV_DEP_FILE, "scripts/instructor_texturemanifest.txt" )
PRECACHE_REGISTER_END()


#define MOD_DIR "MOD"

#define GAMEINSTRUCTOR_SCRIPT_FILE "scripts/instructor_lessons.txt"
#define GAMEINSTRUCTOR_MOD_SCRIPT_FILE "scripts/mod_lessons.txt"

#define GAMEINSTRUCTOR_SAVE_FILE "game_instructor_counts.txt"


// Game instructor auto game system instantiation
C_GameInstructor g_GameInstructor[ MAX_SPLITSCREEN_PLAYERS ];
C_GameInstructor &GetGameInstructor()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return g_GameInstructor[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}

ConVar gameinstructor_save_restore_lessons( "gameinstructor_save_restore_lessons", "1", FCVAR_CHEAT, "Set to 0 to disable save/load of open lesson opportunities in single player." );
ConVar gameinstructor_verbose( "gameinstructor_verbose", "0", FCVAR_CHEAT, "Set to 1 for standard debugging or 2 (in combo with gameinstructor_verbose_lesson) to show update actions." );
ConVar gameinstructor_verbose_lesson( "gameinstructor_verbose_lesson", "", FCVAR_CHEAT, "Display more verbose information for lessons have this name." );
ConVar gameinstructor_find_errors( "gameinstructor_find_errors", "0", FCVAR_CHEAT, "Set to 1 and the game instructor will run EVERY scripted command to uncover errors." );

void GameInstructorEnable_ChangeCallback( IConVar *var, const char *pOldValue, float flOldValue );
ConVar gameinstructor_enable( "gameinstructor_enable", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Display in game lessons that teach new players.", GameInstructorEnable_ChangeCallback );
extern ConVar sv_gameinstructor_disable;


ConVar gameinstructor_start_sound_cooldown( "gameinstructor_start_sound_cooldown", "4.0", FCVAR_NONE, "Number of seconds forced between similar lesson start sounds." );

static void FixGameInstructorLessonNameForTitleData( char *chBuffer )
{
	for ( ; chBuffer && *chBuffer; ++ chBuffer )
	{
		char const ch = *chBuffer;
		if ( ch >= 'a' && ch <= 'z' )
			continue;
		if ( ch >= 'A' && ch <= 'Z' )
			continue;
		if ( ch >= '0' && ch <= '9' )
			continue;
		if ( ch == '.' || ch == '_' )
			continue;
		*chBuffer = '_';
	}
}

// Enable or Disable the game instructor based on the client setting
void EnableDisableInstructor( void )
{
	bool bEnabled = (!sv_gameinstructor_disable.GetBool() && gameinstructor_enable.GetBool());

#if defined( CSTRIKE15 )
	if ( CSGameRules() && CSGameRules()->IsPlayingTraining() )
	{
		bEnabled = true;
	}
#endif

	if ( bEnabled )
	{
		// Game instructor has been enabled, so init it!
		GetGameInstructor().Init();
	}
	else 
	{
		// Game instructor has been disabled, so shut it down!
		GetGameInstructor().Shutdown();
	}
}

void GameInstructorEnable_ChangeCallback( IConVar *var, const char *pOldValue, float flOldValue )
{
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		if ( ( flOldValue != 0.0f ) != gameinstructor_enable.GetBool() )
		{
			EnableDisableInstructor();
		}
	}
}

void SVGameInstructorDisable_ChangeCallback( IConVar *var, const char *pOldValue, float flOldValue );
ConVar sv_gameinstructor_disable( "sv_gameinstructor_disable", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Force all clients to disable their game instructors.", SVGameInstructorDisable_ChangeCallback );
void SVGameInstructorDisable_ChangeCallback( IConVar *var, const char *pOldValue, float flOldValue )
{
	if ( !engine )
		return;

	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		EnableDisableInstructor();
	}
}


void GameInstructor_KeyValueBuilder( KeyValues *pKeyValues )
{
	GetGameInstructor().KeyValueBuilder( pKeyValues );
}


// Merged from L4D but waiting on other code to be merged before this can compile 
class CGameInstructorUserNotificationsListener : public IMatchEventsSink
{
public:
	CGameInstructorUserNotificationsListener() : m_nRefCount( 0 ) {}

public:
	void RefCount( int nDelta );

public:
	virtual void OnEvent( KeyValues *pEvent );

protected:
	void OnGameUsersChanged();
	void OnStorageDeviceAvailable( int iCtrlr );

protected:
	int m_nRefCount;
};

void CGameInstructorUserNotificationsListener::RefCount( int nDelta )
{
	if ( !g_pMatchFramework )
		return;

	if ( m_nRefCount <= 0 && nDelta > 0 )
	{
		g_pMatchFramework->GetEventsSubscription()->Subscribe( this );
	}

	if ( m_nRefCount > 0 && m_nRefCount - nDelta <= 0 )
	{
		g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
	}

	m_nRefCount = MAX( 0, m_nRefCount + nDelta );
}

void CGameInstructorUserNotificationsListener::OnEvent( KeyValues *pEvent )
{
	char const *szEvent = pEvent->GetName();

	if ( !Q_stricmp( szEvent, "OnProfileDataLoaded" ) )
	{
		OnStorageDeviceAvailable( pEvent->GetInt( "iController" ) );
	}
	else if ( !Q_stricmp( szEvent, "OnProfilesChanged" ) )
	{
		OnGameUsersChanged();
	}
}

void CGameInstructorUserNotificationsListener::OnGameUsersChanged()
{
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetGameInstructor().ResetDisplaysAndSuccesses();
	}
}

void CGameInstructorUserNotificationsListener::OnStorageDeviceAvailable( int iCtrlr )
{
#ifdef _GAMECONSOLE
	if ( iCtrlr < 0 || iCtrlr >= XUSER_MAX_COUNT )
		return;

	int iSlot = XBX_GetSlotByUserId( iCtrlr );

	if ( iSlot < 0 || iSlot >= MAX_SPLITSCREEN_PLAYERS )
		return;
#elif !defined( SPLIT_SCREEN_STUBS )
	int iSlot = iCtrlr;
#endif

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( iSlot );
	GetGameInstructor().RefreshDisplaysAndSuccesses();
}

CGameInstructorUserNotificationsListener s_GameInstructorUserNotificationsListener;

void GameInstructor_Init()
{
	// Subscribe for match events
	s_GameInstructorUserNotificationsListener.RefCount( +1 );
}

void GameInstructor_Shutdown()
{
	// Unsubscribe for match events
	s_GameInstructorUserNotificationsListener.RefCount( -1 );
}


//
// C_GameInstructor
//

bool C_GameInstructor::Init( void )
{
	// Make sure split slot is up to date
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		if ( &GetGameInstructor() == this )
		{
			SetSlot( i );
			break;
		}
	}

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

#if defined( CSTRIKE15 )
	if ( (!gameinstructor_enable.GetBool() || sv_gameinstructor_disable.GetBool()) && !(CSGameRules() && CSGameRules()->IsPlayingTraining()) )
#else
	if ( !gameinstructor_enable.GetBool() || sv_gameinstructor_disable.GetBool() )
#endif
	{
		// Don't init if it's disabled
		return true;
	}

	if ( gameinstructor_verbose.GetInt() > 0 )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Initializing...\n" );
	}

	if ( m_bEnsureThatInitIsNotCalledMultipleTimes )
	{
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Initializing is prevented by reentry guard, high level code probably has a problem...\n" );
		}
		return true;
	}
	m_bEnsureThatInitIsNotCalledMultipleTimes = true;

	m_bNoDraw = false;
	m_bHiddenDueToOtherElements = false;

	m_iCurrentPriority = 0;
	m_hLastSpectatedPlayer = NULL;
	m_bSpectatedPlayerChanged = false;

	m_szPreviousStartSound[ 0 ] = '\0';
	m_fNextStartSoundTime = 0;

	ReadLessonsFromFile( GAMEINSTRUCTOR_MOD_SCRIPT_FILE );
	ReadLessonsFromFile( GAMEINSTRUCTOR_SCRIPT_FILE );

	InitLessonPrerequisites();

	KeyValueSaver().InitKeyValues( GAMEINSTRUCTOR_SAVE_FILE, GameInstructor_KeyValueBuilder );

	ListenForGameEvent( "gameinstructor_draw" );
	ListenForGameEvent( "gameinstructor_nodraw" );

	ListenForGameEvent( "round_end" );
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "player_team" );
	ListenForGameEvent( "player_disconnect" );
	ListenForGameEvent( "map_transition" );
	ListenForGameEvent( "game_newmap" );

#if defined( _X360 )
	ListenForGameEvent( "reset_game_titledata" );
	ListenForGameEvent( "read_game_titledata" );
	ListenForGameEvent( "write_game_titledata" );
#endif

#ifdef TERROR
	ListenForGameEvent( "player_bot_replace" );
	ListenForGameEvent( "bot_player_replace" );
#endif

	ListenForGameEvent( "set_instructor_group_enabled" );

	EvaluateLessonsForGameRules();

	return true;
}

void C_GameInstructor::Shutdown( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	if ( gameinstructor_verbose.GetInt() > 0 )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Shutting down...\n" );
	}

	CloseAllOpenOpportunities();

	WriteSaveData();

	// Clear out all the lessons
	for ( int i = 0; i < m_Lessons.Count(); ++i )
	{
		if ( m_Lessons[ i ] )
		{
			m_Lessons[ i ]->StopListeningForAllEvents();
			delete m_Lessons[ i ];
			m_Lessons[ i ] = NULL;
		}
	}
	m_Lessons.RemoveAll();

	m_LessonGroupConVarToggles.RemoveAll();

	// Stop listening for events
	StopListeningForAllEvents();

	m_bEnsureThatInitIsNotCalledMultipleTimes = false;
}

void C_GameInstructor::UpdateHiddenByOtherElements( void )
{
	bool bHidden = Mod_HiddenByOtherElements();

	if ( bHidden && !m_bHiddenDueToOtherElements )
	{
		StopAllLessons();
	}

	m_bHiddenDueToOtherElements = bHidden;
}

void C_GameInstructor::Update( float frametime )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	VPROF_BUDGET( "C_GameInstructor::Update", "GameInstructor" );

	UpdateHiddenByOtherElements();

#if defined( CSTRIKE15 )
	if ( (!gameinstructor_enable.GetBool() || m_bNoDraw || m_bHiddenDueToOtherElements) && !(CSGameRules() && CSGameRules()->IsPlayingTraining()) )
#else
	if ( !gameinstructor_enable.GetBool() || m_bNoDraw || m_bHiddenDueToOtherElements )
#endif
	{
		// Don't update if disabled or hidden
		return;
	}

	if ( gameinstructor_find_errors.GetBool() )
	{
		FindErrors();

		gameinstructor_find_errors.SetValue( 0 );
	}

	if ( m_bSpectatedPlayerChanged )
	{
		// Safe spot to clean out stale lessons if spectator changed
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Spectated player changed...\n" );
		}

		CloseAllOpenOpportunities();

		m_bSpectatedPlayerChanged = false;
	}

	// Loop through all the lesson roots and reset their active status
	for ( int i = m_OpenOpportunities.Count() - 1; i >= 0; --i )
	{
		CBaseLesson *pLesson = m_OpenOpportunities[ i ];
		CBaseLesson *pRootLesson = pLesson->GetRoot();

		if ( pRootLesson->InstanceType() == LESSON_INSTANCE_SINGLE_ACTIVE )
		{
			pRootLesson->SetInstanceActive( false );
		}
	}

	int iCurrentPriority = 0;

	// Loop through all the open lessons
	for ( int i = m_OpenOpportunities.Count() - 1; i >= 0; --i )
	{
		CBaseLesson *pLesson = m_OpenOpportunities[ i ];

		if ( !pLesson->IsOpenOpportunity() || pLesson->IsTimedOut() )
		{
			// This opportunity has closed
			CloseOpportunity( pLesson );
			RANDOM_CEG_TEST_SECRET_PERIOD( 11, 23 );
			continue;
		}

		// Lesson should be displayed, so it can affect priority
		CBaseLesson *pRootLesson = pLesson->GetRoot();

		bool bShouldDisplay = pLesson->ShouldDisplay();
		bool bIsLocked = pLesson->IsLocked();

		if ( ( bShouldDisplay || bIsLocked ) && 
			 ( pLesson->GetPriority() >= m_iCurrentPriority || pLesson->NoPriority() || bIsLocked ) && 
			 ( pRootLesson && ( pRootLesson->InstanceType() != LESSON_INSTANCE_SINGLE_ACTIVE || !pRootLesson->IsInstanceActive() ) ) )
		{
			// Lesson is at the highest priority level, isn't violating instance rules, and has met all the prerequisites
			if ( UpdateActiveLesson( pLesson, pRootLesson ) || pRootLesson->IsLearned() )
			{
				// Lesson is active
				if ( pLesson->IsVisible() || pRootLesson->IsLearned() )
				{
					pRootLesson->SetInstanceActive( true );

					if ( iCurrentPriority < pLesson->GetPriority() && !pLesson->NoPriority() )
					{
						// This active or learned lesson has the highest priority so far
						iCurrentPriority = pLesson->GetPriority();
					}
				}
			}
			else
			{
				// On second thought, this shouldn't have been displayed
				bShouldDisplay = false;
			}
		}
		else
		{
			// Lesson shouldn't be displayed right now
			UpdateInactiveLesson( pLesson );
		}
	}

	// Set the priority for next frame
	if ( gameinstructor_verbose.GetInt() > 1 && m_iCurrentPriority != iCurrentPriority )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Priority changed from " );
		ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "%i ", m_iCurrentPriority );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "to " );
		ConColorMsg( CBaseLesson::m_rgbaVerboseOpen, "%i", iCurrentPriority );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ".\n" );
	}

	m_iCurrentPriority = iCurrentPriority;
}

void C_GameInstructor::FireGameEvent( IGameEvent *event )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	VPROF_BUDGET( "C_GameInstructor::FireGameEvent", "GameInstructor" );

	const char *name = event->GetName();

	if ( Q_strcmp( name, "gameinstructor_draw" ) == 0 )
	{
		if ( m_bNoDraw )
		{
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Set to draw...\n" );
			}

			m_bNoDraw = false;
		}
	}
	else if ( Q_strcmp( name, "gameinstructor_nodraw" ) == 0 )
	{
		if ( !m_bNoDraw )
		{
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Set to not draw...\n" );
			}

			m_bNoDraw = true;
			StopAllLessons();
		}
	}
	else if ( Q_strcmp( name, "round_end" ) == 0 )
	{
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Round ended...\n" );
		}

		CloseAllOpenOpportunities();
	}
	else if ( Q_strcmp( name, "round_start" ) == 0 )
	{
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Round started...\n" );
		}

		CloseAllOpenOpportunities();

		EvaluateLessonsForGameRules();
	}
	else if ( Q_strcmp( name, "player_death" ) == 0 )
	{
		STEAMWORKS_TESTSECRET_AMORTIZE( 37 ); 

		C_BasePlayer *pLocalPlayer = GetLocalPlayer();
		if ( pLocalPlayer && pLocalPlayer == UTIL_PlayerByUserId( event->GetInt( "userid" ) ) )
		{
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Local player died...\n" );
			}

			for ( int i = m_OpenOpportunities.Count() - 1; i >= 0; --i )
			{
				CBaseLesson *pLesson = m_OpenOpportunities[ i ];
				CBaseLesson *pRootLesson = pLesson->GetRoot();
				if ( !pRootLesson->CanOpenWhenDead() )
				{
					CloseOpportunity( pLesson );
				}
			}
		}
	}
	else if ( Q_strcmp( name, "player_team" ) == 0 )
	{
		C_BasePlayer *pLocalPlayer = GetLocalPlayer();
		if ( pLocalPlayer && pLocalPlayer == UTIL_PlayerByUserId( event->GetInt( "userid" ) ) && 
			 ( event->GetInt( "team" ) != event->GetInt( "oldteam" ) || event->GetBool( "disconnect" ) ) )
		{
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Local player changed team (or disconnected)...\n" );
			}

			CloseAllOpenOpportunities();
		}

		EvaluateLessonsForGameRules();
	}
	else if ( Q_strcmp( name, "player_disconnect" ) == 0 )
	{
		C_BasePlayer *pLocalPlayer = GetLocalPlayer();
		if ( pLocalPlayer && pLocalPlayer == UTIL_PlayerByUserId( event->GetInt( "userid" ) ) )
		{
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Local player disconnected...\n" );
			}

			STEAMWORKS_SELFCHECK();

			CloseAllOpenOpportunities();
		}
	}
	else if ( Q_strcmp( name, "map_transition" ) == 0 )
	{
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Map transition...\n" );
		}

		CloseAllOpenOpportunities();

		if ( m_bNoDraw )
		{
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( Color( 255, 128, 64, 255 ), "GAME INSTRUCTOR: " );
				ConColorMsg( Color( 64, 128, 255, 255 ), "Set to draw...\n" );
			}

			m_bNoDraw = false;
		}
	}
	else if ( Q_strcmp( name, "game_newmap" ) == 0 )
	{
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "New map...\n" );
		}

		CloseAllOpenOpportunities();

		if ( m_bNoDraw )
		{
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( Color( 255, 128, 64, 255 ), "GAME INSTRUCTOR: " );
				ConColorMsg( Color( 64, 128, 255, 255 ), "Set to draw...\n" );
			}

			m_bNoDraw = false;
		}
	}
#ifdef TERROR
	else if ( Q_strcmp( name, "player_bot_replace" ) == 0 )
	{
		C_BasePlayer *pLocalPlayer = GetLocalPlayer();
		if ( pLocalPlayer && pLocalPlayer == UTIL_PlayerByUserId( event->GetInt( "player" ) ) )
		{
			CloseAllOpenOpportunities();
		}
		else
		{
			for ( int i = m_OpenOpportunities.Count() - 1; i >= 0; --i )
			{
				CBaseLesson *pLesson = m_OpenOpportunities[ i ];
				pLesson->SwapOutPlayers( event->GetInt( "player" ), event->GetInt( "bot" ) );
			}
		}
	}
	else if ( Q_strcmp( name, "bot_player_replace" ) == 0 )
	{
		C_BasePlayer *pLocalPlayer = GetLocalPlayer();
		if ( pLocalPlayer && pLocalPlayer == UTIL_PlayerByUserId( event->GetInt( "player" ) ) )
		{
			CloseAllOpenOpportunities();
		}
		else
		{
			for ( int i = m_OpenOpportunities.Count() - 1; i >= 0; --i )
			{
				CBaseLesson *pLesson = m_OpenOpportunities[ i ];
				pLesson->SwapOutPlayers( event->GetInt( "bot" ), event->GetInt( "player" ) );
			}
		}
	}
#endif
	else if ( Q_strcmp( name, "set_instructor_group_enabled" ) == 0 )
	{
		const char *pszGroup = event->GetString( "group" );
		bool bEnabled = event->GetInt( "enabled" ) != 0;

		if ( pszGroup && pszGroup[0] )
		{
			SetLessonGroupEnabled( pszGroup, bEnabled );
		}
	}
#if defined( _X360 )
	else if ( Q_strcmp( name, "read_game_titledata" ) == 0 )
	{
		ReadSaveData();
	}
	else if ( Q_strcmp( name, "write_game_titledata" ) == 0 )
	{
		KeyValueSaver().MarkKeyValuesDirty( GAMEINSTRUCTOR_SAVE_FILE );
		WriteSaveData();
	}
	else if ( Q_strcmp( name, "reset_game_titledata" ) == 0 )
	{
		ResetDisplaysAndSuccesses();
	}
#endif
}

void C_GameInstructor::DefineLesson( CBaseLesson *pLesson )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	if ( gameinstructor_verbose.GetInt() > 0 )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Lesson " );
		ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\" ", pLesson->GetName() );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "defined.\n" );
	}

	m_Lessons.AddToTail( pLesson );
}

const CBaseLesson * C_GameInstructor::GetLesson( const char *pchLessonName )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );
	return GetLesson_Internal( pchLessonName );
}

bool C_GameInstructor::IsLessonOfSameTypeOpen( const CBaseLesson *pLesson ) const
{
	for ( int i = 0; i < m_OpenOpportunities.Count(); ++i )
	{
		CBaseLesson *pOpenOpportunity = m_OpenOpportunities[ i ];

		if ( pOpenOpportunity->GetNameSymbol() == pLesson->GetNameSymbol() )
		{	
			return true;
		}
	}

	return false;
}

void C_GameInstructor::SaveGameBlock( ISave *pSave )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	if ( gameinstructor_save_restore_lessons.GetBool() )
	{
		// Save the lessons
		int nCount = m_OpenOpportunities.Count();
		pSave->WriteInt( &nCount );
		for ( int i = 0; i < nCount; i++ )
		{
			pSave->StartBlock();
			pSave->WriteAll( static_cast< CScriptedIconLesson * >( m_OpenOpportunities[ i ] ) );
			pSave->EndBlock();
		}
	}
	else
	{
		int nCount = 0;
		pSave->WriteInt( &nCount );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pRestore - 
//			fCreatePlayers - 
//-----------------------------------------------------------------------------
void C_GameInstructor::RestoreGameBlock( IRestore *pRestore, bool fCreatePlayers )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	CGameSaveRestoreInfo *pSaveData = pRestore->GetGameSaveRestoreInfo();

	// Game Instructor is a singleton so we only need to restore it once,
	// from the level that we are going into.
	if( !pSaveData->levelInfo.fUseLandmark )
	{
		CloseAllOpenOpportunities();

		if ( gameinstructor_save_restore_lessons.GetBool() )
		{
			// Read in the lessons
			int nCount = pRestore->ReadInt();
			for ( int i = 0; i < nCount; i++ )
			{
				CScriptedIconLesson *pOpenLesson = new CScriptedIconLesson( "", false, true, m_nSplitScreenSlot );

				pRestore->StartBlock();
				pRestore->ReadAll( pOpenLesson );
				pRestore->EndBlock();

				const CScriptedIconLesson *pRootLesson = static_cast<const CScriptedIconLesson *>( GetLesson( pOpenLesson->GetName() ) );
				pOpenLesson->SetRoot( const_cast<CScriptedIconLesson *>( pRootLesson ) );

				GetGameInstructor().OpenOpportunity( pOpenLesson );
			}
		}
		else
		{
			CScriptedIconLesson *pOpenLessonDummy = new CScriptedIconLesson( "", false, true, m_nSplitScreenSlot );

			int nCount = pRestore->ReadInt();
			for ( int i = 0; i < nCount; i++ )
			{
				pRestore->StartBlock();
				pRestore->ReadAll( pOpenLessonDummy );
				pRestore->EndBlock();
			}

			delete pOpenLessonDummy;
		}
	}	
}

bool C_GameInstructor::ReadSaveData( void )
{
	// for external playtests, don't ever read in persisted instructor state, always start fresh
	if( CommandLine()->FindParm( "-playtest" ) )
		return true;

	if ( m_bHasLoadedSaveData )
		return true;
	
	// Always reset state first in case storage device
	// was declined or ends up in faulty state
	ResetDisplaysAndSuccesses();

	if ( m_nSplitScreenSlot < 0 || m_nSplitScreenSlot >= (int) XBX_GetNumGameUsers() )
		return true;

	IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetUserId( m_nSplitScreenSlot ) );
	if ( !pPlayer )
		return true;

	TitleDataFieldsDescription_t const *fields = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();

#if defined( _X360 )
	// check version number is valid to ensure there is good data before reading
	ConVarRef cl_titledataversionblock3 ( "cl_titledataversionblock3" );
	TitleDataFieldsDescription_t const *versionField = TitleDataFieldsDescriptionFindByString( fields, "TITLEDATA.BLOCK3.VERSION" );
	if ( !versionField || versionField->m_eDataType != TitleDataFieldsDescription_t::DT_uint16 )
	{
		Warning( "C_GameInstructor::ReadSaveData TITLEDATA.BLOCK3.VERSION is expected to be defined as DT_uint16\n" );
		return true;
	}

	int versionNumber = TitleDataFieldsDescriptionGetValue<uint16>( versionField, pPlayer );
	if ( versionNumber != cl_titledataversionblock3.GetInt() )
	{
		Warning ( "C_GameInstructor::ReadSaveData wrong version # for TITLEDATA.BLOCK3.VERSION; expected %d, got %d\n", cl_titledataversionblock3.GetInt(), versionNumber );
		return true;
	}
#endif

	m_bHasLoadedSaveData = true;

	int nVersion = 0;

	for ( int i = 0; i < m_Lessons.Count();++i )
	{
		CBaseLesson *pLesson = m_Lessons[i];
		if ( !pLesson || ( pLesson->GetDisplayLimit() == 0 && pLesson->GetSuccessLimit() == 0 ) )
			continue;

		CFmtStr tdKey( "GI.lesson.%s", pLesson->GetName() );
		FixGameInstructorLessonNameForTitleData( tdKey.Access() );
		TitleDataFieldsDescription_t const *fdKey = TitleDataFieldsDescriptionFindByString( fields, tdKey );
		if ( !fdKey )
		{
			Warning( "C_GameInstructor::ReadSaveData failed to read %s\n", tdKey.Access() );
			continue;
		}

		TitleData3::GameInstructorData_t::LessonInfo_t li;
		li.u8dummy = TitleDataFieldsDescriptionGetValue<uint8>( fdKey, pPlayer );
		
		pLesson->SetDisplayCount( li.display );
		pLesson->SetSuccessCount( li.success );

		if ( Q_strcmp( pLesson->GetName(), "version number" ) == 0 )
		{
			nVersion = pLesson->GetSuccessCount();
		}
	}

	CBaseLesson *pLessonVersionNumber = GetLesson_Internal( "version number" );
	if ( pLessonVersionNumber && !pLessonVersionNumber->IsLearned() )
	{
		ResetDisplaysAndSuccesses();
		pLessonVersionNumber->SetSuccessCount( pLessonVersionNumber->GetSuccessLimit() );
		KeyValueSaver().MarkKeyValuesDirty( GAMEINSTRUCTOR_SAVE_FILE );
	}
#ifdef TERROR
	else if ( IsPressDemoMode() )
	{
		ResetDisplaysAndSuccesses();
		KeyValueSaver().MarkKeyValuesDirty( GAMEINSTRUCTOR_SAVE_FILE );
	}
#endif

	return true;
}

bool C_GameInstructor::WriteSaveData( void )
{
	if ( engine->IsPlayingDemo() )
		return false;

	return KeyValueSaver().WriteDirtyKeyValues( GAMEINSTRUCTOR_SAVE_FILE );
}

void C_GameInstructor::KeyValueBuilder( KeyValues *pKeyValues )
{
	if ( m_nSplitScreenSlot < 0 || m_nSplitScreenSlot >= (int) XBX_GetNumGameUsers() )
		return;
	
	IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetUserId( m_nSplitScreenSlot ) );
	if ( !pPlayer )
		return;

	TitleDataFieldsDescription_t const *fields = g_pMatchFramework->GetMatchTitle()->DescribeTitleDataStorage();

	// Build key value data to save
	for ( int i = 0; i < m_Lessons.Count();++i )
	{
		CBaseLesson *pLesson = m_Lessons[ i ];
		TitleData3::GameInstructorData_t::LessonInfo_t li;
		li.u8dummy = 0;
		li.display = pLesson->GetDisplayCount() & 0xF;
		li.success = pLesson->GetSuccessCount() & 0xF;

		CFmtStr tdKey( "GI.lesson.%s", pLesson->GetName() );
		FixGameInstructorLessonNameForTitleData( tdKey.Access() );
		TitleDataFieldsDescription_t const *fdKey = TitleDataFieldsDescriptionFindByString( fields, tdKey );
		if ( fdKey )
		{
			TitleDataFieldsDescriptionSetValue<uint8>( fdKey, pPlayer, li.u8dummy );
		}
		else
		{
			DevWarning( "C_GameInstructor::KeyValueBuilder did not save %s; is it missing from inc_gameinstructor_lessons.inc\n", tdKey.Access() );
		}
	}
}

void C_GameInstructor::RefreshDisplaysAndSuccesses( void )
{
	m_bHasLoadedSaveData = false;
	ReadSaveData();
}

void C_GameInstructor::ResetDisplaysAndSuccesses( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );
	if ( gameinstructor_verbose.GetInt() > 0 )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Reset all lesson display and success counts.\n" );
	}

	for ( int i = 0; i < m_Lessons.Count(); ++i )
	{
		m_Lessons[ i ]->ResetDisplaysAndSuccesses();
	}
}

void C_GameInstructor::MarkDisplayed( const char *pchLessonName )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );
	CBaseLesson *pLesson = GetLesson_Internal( pchLessonName );
	if ( !pLesson )
		return;

	if ( gameinstructor_verbose.GetInt() > 0 )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Lesson " );
		ConColorMsg( CBaseLesson::m_rgbaVerboseOpen, "\"%s\" ", pLesson->GetName() );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "marked as displayed.\n" );
	}

	if ( pLesson->IncDisplayCount() )
	{
		KeyValueSaver().MarkKeyValuesDirty( GAMEINSTRUCTOR_SAVE_FILE );
	}
}

void C_GameInstructor::MarkSucceeded( const char *pchLessonName )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );
	CBaseLesson *pLesson = GetLesson_Internal( pchLessonName );
	if ( !pLesson )
		return;

	if ( gameinstructor_verbose.GetInt() > 0 )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Lesson " );
		ConColorMsg( CBaseLesson::m_rgbaVerboseSuccess, "\"%s\" ", pLesson->GetName() );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "marked as succeeded.\n" );
	}

	if ( pLesson->IncSuccessCount() )
	{
		KeyValueSaver().MarkKeyValuesDirty( GAMEINSTRUCTOR_SAVE_FILE );
	}
}

void C_GameInstructor::PlaySound( const char *pchSoundName )
{
	// emit alert sound
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pLocalPlayer )
	{
		// Local player exists
		if ( pchSoundName[ 0 ] != '\0' && Q_strcmp( m_szPreviousStartSound, pchSoundName ) != 0 )
		{
			V_strcpy_safe( m_szPreviousStartSound, pchSoundName );
			m_fNextStartSoundTime = 0.0f;
		}

		if ( gpGlobals->curtime >= m_fNextStartSoundTime && pchSoundName[ 0 ] != '\0' )
		{
			// A sound was specified, so play it!
			pLocalPlayer->EmitSound( pchSoundName );
			m_fNextStartSoundTime = gpGlobals->curtime + gameinstructor_start_sound_cooldown.GetFloat();
		}
	}
}

bool C_GameInstructor::OpenOpportunity( CBaseLesson *pLesson )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );
	// Get the root lesson
	CBaseLesson *pRootLesson = pLesson->GetRoot();
	if ( !pRootLesson )
	{
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Opportunity " );
			ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\"%s\" ", pLesson->GetName() );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "NOT opened (because root lesson could not be found).\n" );
		}

		delete pLesson;
		return false;
	}

	C_BasePlayer *pLocalPlayer = GetLocalPlayer();
	if ( !pRootLesson->CanOpenWhenDead() && ( !pLocalPlayer || !pLocalPlayer->IsAlive() ) )
	{
		// If the player is dead don't allow lessons that can't be opened when dead
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Opportunity " );
			ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\"%s\" ", pLesson->GetName() );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "NOT opened (because player is dead and can_open_when_dead not set).\n" );
		}

		delete pLesson;
		return false;
	}

	if ( !pRootLesson->CanOpenOnceLearned() && pRootLesson->IsLearned() )
	{
		// If the player has learned this and we don't want it to open onced learned
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Opportunity " );
			ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\"%s\" ", pLesson->GetName() );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "NOT opened (because this is learned and ONCE_LEARNED_NEVER_OPEN is set).\n" );
		}

		delete pLesson;
		return false;
	}

	if ( !pRootLesson->PrerequisitesHaveBeenMet() )
	{
		// If the prereqs haven't been met, don't open it
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Opportunity " );
			ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\"%s\" ", pLesson->GetName() );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "NOT opened (because prereqs haven't been met).\n" );
		}

		delete pLesson;
		return false;
	}

	if ( pRootLesson->InstanceType() == LESSON_INSTANCE_FIXED_REPLACE )
	{
		CBaseLesson *pLessonToReplace = NULL;
		CBaseLesson *pLastReplacableLesson = NULL;

		int iInstanceCount = 0;

		// Check how many are already open
		for ( int i = m_OpenOpportunities.Count() - 1; i >= 0; --i )
		{
			CBaseLesson *pOpenOpportunity = m_OpenOpportunities[ i ];

			if ( pOpenOpportunity->GetNameSymbol() == pLesson->GetNameSymbol() && 
				 pOpenOpportunity->GetReplaceKeySymbol() == pLesson->GetReplaceKeySymbol() )
			{
				iInstanceCount++;

				if ( pRootLesson->ShouldReplaceOnlyWhenStopped() )
				{
					if ( !pOpenOpportunity->IsInstructing() )
					{
						pLastReplacableLesson = pOpenOpportunity;
					}
				}
				else
				{
					pLastReplacableLesson = pOpenOpportunity;
				}

				if ( iInstanceCount >= pRootLesson->GetFixedInstancesMax() )
				{
					pLessonToReplace = pLastReplacableLesson;
					break;
				}
			}
		}

		if ( pLessonToReplace )
		{
			// Take the place of the previous instance
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Opportunity " );
				ConColorMsg( CBaseLesson::m_rgbaVerboseOpen, "\"%s\" ", pLesson->GetName() );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "replacing open lesson of same type.\n" );
			}

			pLesson->TakePlaceOf( pLessonToReplace );
			CloseOpportunity( pLessonToReplace );
		}
		else if ( iInstanceCount >= pRootLesson->GetFixedInstancesMax() )
		{
			// Don't add another lesson of this type
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Opportunity " );
				ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\"%s\" ", pLesson->GetName() );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "NOT opened (there is too many started lessons of this type).\n" );
			}

			delete pLesson;
			return false;
		}
	}

	if ( gameinstructor_verbose.GetInt() > 0 )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Opportunity " );
		ConColorMsg( CBaseLesson::m_rgbaVerboseOpen, "\"%s\" ", pLesson->GetName() );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "opened.\n" );
	}

	m_OpenOpportunities.AddToTail( pLesson );

	return true;
}

void C_GameInstructor::DumpOpenOpportunities( void )
{
	ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
	ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Open lessons...\n" );

	for ( int i = m_OpenOpportunities.Count() - 1; i >= 0; --i )
	{
		CBaseLesson *pLesson = m_OpenOpportunities[ i ];
		CBaseLesson *pRootLesson = pLesson->GetRoot();

		Color color;

		if ( pLesson->IsInstructing() )
		{
			// Green
			color = CBaseLesson::m_rgbaVerboseOpen;
		}
		else if ( pRootLesson->IsLearned() && pLesson->GetPriority() >= m_iCurrentPriority )
		{
			// Yellow
			color = CBaseLesson::m_rgbaVerboseSuccess;
		}
		else
		{
			// Red
			color = CBaseLesson::m_rgbaVerboseClose;
		}

		ConColorMsg( color, "\t%s\n", pLesson->GetName() );
	}
}

KeyValues * C_GameInstructor::GetScriptKeys( void )
{
	return m_pScriptKeys;
}

C_BasePlayer * C_GameInstructor::GetLocalPlayer( void )
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

	// If we're not a developer, don't do the special spectator hook ups
	if ( !developer.GetBool() )
		return pLocalPlayer;

	// If there is no local player and we're not spectating, just return that
	if ( !pLocalPlayer || pLocalPlayer->GetTeamNumber() != TEAM_SPECTATOR )
		return pLocalPlayer;

	// We're purely a spectator let's get lessons of the person we're spectating
	C_BasePlayer *pSpectatedPlayer = NULL;

	if ( pLocalPlayer->GetObserverMode() == OBS_MODE_IN_EYE || pLocalPlayer->GetObserverMode() == OBS_MODE_CHASE )
	{
		pSpectatedPlayer = ToBasePlayer( pLocalPlayer->GetObserverTarget() );
	}

	if ( m_hLastSpectatedPlayer != pSpectatedPlayer )
	{
		// We're spectating someone new! Close all the stale lessons!
		m_bSpectatedPlayerChanged = true;
		m_hLastSpectatedPlayer = pSpectatedPlayer;
	}

	return pSpectatedPlayer;
}

void C_GameInstructor::EvaluateLessonsForGameRules( void )
{
	// Enable everything by default
	for ( int i = 0; i < m_Lessons.Count(); ++i )
	{
		m_Lessons[ i ]->SetEnabled( true );
	}

	// Then see if we should disable anything
	for ( int nConVar = 0; nConVar < m_LessonGroupConVarToggles.Count(); ++nConVar )
	{
		LessonGroupConVarToggle_t *pLessonGroupConVarToggle = &(m_LessonGroupConVarToggles[ nConVar ]);
		if ( pLessonGroupConVarToggle->var.IsValid() )
		{
			if ( pLessonGroupConVarToggle->var.GetBool() )
			{
				SetLessonGroupEnabled( pLessonGroupConVarToggle->szLessonGroupName, false );
			}
		}
	}
}

void C_GameInstructor::SetLessonGroupEnabled( const char *pszGroup, bool bEnabled )
{
	for ( int i = 0; i < m_Lessons.Count(); ++i )
	{
		if ( !Q_stricmp(pszGroup, m_Lessons[i]->GetGroup()) )
		{
			m_Lessons[ i ]->SetEnabled( bEnabled );
		}
	}
}

void C_GameInstructor::FindErrors( void )
{
	// Loop through all the lesson and run all their scripted actions
	for ( int i = 0; i < m_Lessons.Count(); ++i )
	{
		CScriptedIconLesson *pLesson = dynamic_cast<CScriptedIconLesson *>( m_Lessons[ i ] );
		if ( pLesson )
		{
			// Process all open events
			for ( int iLessonEvent = 0; iLessonEvent < pLesson->GetOpenEvents().Count(); ++iLessonEvent )
			{
				const LessonEvent_t *pLessonEvent = &(pLesson->GetOpenEvents()[ iLessonEvent ]);
				pLesson->ProcessElements( NULL, &(pLessonEvent->elements) );
			}

			// Process all close events
			for ( int iLessonEvent = 0; iLessonEvent < pLesson->GetCloseEvents().Count(); ++iLessonEvent )
			{
				const LessonEvent_t *pLessonEvent = &(pLesson->GetCloseEvents()[ iLessonEvent ]);
				pLesson->ProcessElements( NULL, &(pLessonEvent->elements) );
			}

			// Process all success events
			for ( int iLessonEvent = 0; iLessonEvent < pLesson->GetSuccessEvents().Count(); ++iLessonEvent )
			{
				const LessonEvent_t *pLessonEvent = &(pLesson->GetSuccessEvents()[ iLessonEvent ]);
				pLesson->ProcessElements( NULL, &(pLessonEvent->elements) );
			}

			// Process all on open events
			for ( int iLessonEvent = 0; iLessonEvent < pLesson->GetOnOpenEvents().Count(); ++iLessonEvent )
			{
				const LessonEvent_t *pLessonEvent = &(pLesson->GetOnOpenEvents()[ iLessonEvent ]);
				pLesson->ProcessElements( NULL, &(pLessonEvent->elements) );
			}

			// Process all update events
			for ( int iLessonEvent = 0; iLessonEvent < pLesson->GetUpdateEvents().Count(); ++iLessonEvent )
			{
				const LessonEvent_t *pLessonEvent = &(pLesson->GetUpdateEvents()[ iLessonEvent ]);
				pLesson->ProcessElements( NULL, &(pLessonEvent->elements) );
			}
		}
	}
}

bool C_GameInstructor::UpdateActiveLesson( CBaseLesson *pLesson, const CBaseLesson *pRootLesson )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	VPROF_BUDGET( "C_GameInstructor::UpdateActiveLesson", "GameInstructor" );

	bool bIsOpen = pLesson->IsInstructing();

	RANDOM_CEG_TEST_SECRET()

	if ( !bIsOpen && !pRootLesson->IsLearned() )
	{
		pLesson->SetStartTime();
		pLesson->Start();

		// Check to see if it successfully started
		bIsOpen = ( pLesson->IsOpenOpportunity() && pLesson->ShouldDisplay() );

		if ( bIsOpen )
		{
			// Lesson hasn't been started and hasn't been learned
			if ( gameinstructor_verbose.GetInt() > 0 )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Started lesson " );
				ConColorMsg( CBaseLesson::m_rgbaVerboseOpen, "\"%s\"", pLesson->GetName() );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ".\n" );
			}
		}
		else
		{
			pLesson->Stop();
			pLesson->ResetStartTime();
		}
	}

	if ( bIsOpen )
	{
		// Update the running lesson
		pLesson->Update();
		return true;
	}
	else
	{
		pLesson->UpdateInactive();
		return false;
	}
}

void C_GameInstructor::UpdateInactiveLesson( CBaseLesson *pLesson )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );

	VPROF_BUDGET( "C_GameInstructor::UpdateInactiveLesson", "GameInstructor" );

	if ( pLesson->IsInstructing() )
	{
		// Lesson hasn't been stopped
		if ( gameinstructor_verbose.GetInt() > 0 )
		{
			ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Stopped lesson " );
			ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\"%s\"", pLesson->GetName() );
			ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ".\n" );
		}

		pLesson->Stop();
		pLesson->ResetStartTime();
	}

	pLesson->UpdateInactive();
}

CBaseLesson * C_GameInstructor::GetLesson_Internal( const char *pchLessonName )
{
	for ( int i = 0; i < m_Lessons.Count(); ++i )
	{
		CBaseLesson *pLesson = m_Lessons[ i ];

		if ( Q_strcmp( pLesson->GetName(), pchLessonName ) == 0 )
		{
			return pLesson;
		}
	}

	return NULL;
}

void C_GameInstructor::StopAllLessons( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );
	// Stop all the current lessons
	for ( int i = m_OpenOpportunities.Count() - 1; i >= 0; --i )
	{
		CBaseLesson *pLesson = m_OpenOpportunities[ i ];
		UpdateInactiveLesson( pLesson );
	}
}

void C_GameInstructor::CloseAllOpenOpportunities( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );
	// Clear out all the open opportunities
	for ( int i = m_OpenOpportunities.Count() - 1; i >= 0; --i )
	{
		CBaseLesson *pLesson = m_OpenOpportunities[ i ];
		CloseOpportunity( pLesson );
	}

	Assert( m_OpenOpportunities.Count() == 0 );
}

void C_GameInstructor::CloseOpportunity( CBaseLesson *pLesson )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );
	UpdateInactiveLesson( pLesson );

	if ( pLesson->WasDisplayed() )
	{
		MarkDisplayed( pLesson->GetName() );
	}

	if ( gameinstructor_verbose.GetInt() > 0 )
	{
		ConColorMsg( CBaseLesson::m_rgbaVerboseHeader, "GAME INSTRUCTOR: " );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "Opportunity " );
		ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\"%s\" ", pLesson->GetName() );
		ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "closed for reason: " );
		ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "%s\n", pLesson->GetCloseReason() );
	}

	pLesson->StopListeningForAllEvents();

	m_OpenOpportunities.FindAndRemove( pLesson );
	delete pLesson;
}

void C_GameInstructor::ReadLessonsFromFile( const char *pchFileName )
{
	// Static init function
	CScriptedIconLesson::PreReadLessonsFromFile();

	MEM_ALLOC_CREDIT();
	
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( m_nSplitScreenSlot );
	KeyValues *pLessonKeys = new KeyValues( "instructor_lessons" );
	KeyValues::AutoDelete autoDelete(pLessonKeys);
	pLessonKeys->LoadFromFile( g_pFullFileSystem, pchFileName, NULL );

	for ( m_pScriptKeys = pLessonKeys->GetFirstTrueSubKey(); m_pScriptKeys; m_pScriptKeys = m_pScriptKeys->GetNextTrueSubKey() )
	{
		if ( Q_stricmp( m_pScriptKeys->GetName(), "GroupConVarToggle" ) == 0 )
		{
			// Add convar group toggler to the list
			int nLessonGroupConVarToggle = m_LessonGroupConVarToggles.AddToTail( LessonGroupConVarToggle_t( m_pScriptKeys->GetString( "convar" ) ) );
			LessonGroupConVarToggle_t *pLessonGroupConVarToggle = &(m_LessonGroupConVarToggles[ nLessonGroupConVarToggle ]);
			V_strcpy_safe( pLessonGroupConVarToggle->szLessonGroupName, m_pScriptKeys->GetString( "group" ) );

			continue;
		}

		// Ensure that lessons aren't added twice
		if ( GetLesson_Internal( m_pScriptKeys->GetName() ) )
		{
			DevWarning( "Lesson \"%s\" defined twice!\n", m_pScriptKeys->GetName() );
			continue;
		}

		CScriptedIconLesson *pNewLesson = new CScriptedIconLesson( m_pScriptKeys->GetName(), false, false, m_nSplitScreenSlot );
		GetGameInstructor().DefineLesson( pNewLesson );
	}

	m_pScriptKeys = NULL;
}

void C_GameInstructor::InitLessonPrerequisites( void )
{
	for ( int i = 0; i < m_Lessons.Count(); ++i )
	{
		m_Lessons[ i ]->InitPrerequisites();
	}
}


//====================================================================================================
// CLIENTSIDE GAME INSTRUCTOR SAVE/RESTORE 
//====================================================================================================
static short GAMEINSTRUCTOR_SAVE_RESTORE_VERSION = 1;

class CGameInstructorSaveRestoreBlockHandler :	public CDefSaveRestoreBlockHandler
{
	struct QueuedItem_t;
public:
	CGameInstructorSaveRestoreBlockHandler()
	{
	}

	const char *GetBlockName()
	{
		return "GameInstructor";
	}

	virtual void Save( ISave *pSave ) 
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
		GetGameInstructor().SaveGameBlock( pSave );
	}

	virtual void WriteSaveHeaders( ISave *pSave )
	{
		pSave->WriteShort( &GAMEINSTRUCTOR_SAVE_RESTORE_VERSION );
	}

	virtual void ReadRestoreHeaders( IRestore *pRestore )
	{
		// No reason why any future version shouldn't try to retain backward compatibility. The default here is to not do so.
		short version = pRestore->ReadShort();
		m_bDoLoad = ( version == GAMEINSTRUCTOR_SAVE_RESTORE_VERSION );
	}

	virtual void Restore( IRestore *pRestore, bool fCreatePlayers ) 
	{
		if ( m_bDoLoad )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( 0 );
			GetGameInstructor().RestoreGameBlock( pRestore, fCreatePlayers );
		}
	}

private:
	bool	m_bDoLoad;
};

CGameInstructorSaveRestoreBlockHandler g_GameInstructorSaveRestoreBlockHandler;

ISaveRestoreBlockHandler *GetGameInstructorRestoreBlockHandler()
{
	return &g_GameInstructorSaveRestoreBlockHandler;
}


CON_COMMAND_F( gameinstructor_reload_lessons, "Shuts down all open lessons and reloads them from the script file.", FCVAR_CHEAT )
{
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetGameInstructor().Shutdown();
		GetGameInstructor().Init();
	}
}

CON_COMMAND_F( gameinstructor_reset_counts, "Resets all display and success counts to zero.", FCVAR_NONE )
{
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetGameInstructor().ResetDisplaysAndSuccesses();
	}
}

CON_COMMAND_F( gameinstructor_dump_open_lessons, "Gives a list of all currently open lessons.", FCVAR_CHEAT )
{
	for ( int i = 0 ; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetGameInstructor().DumpOpenOpportunities();
	}
}
