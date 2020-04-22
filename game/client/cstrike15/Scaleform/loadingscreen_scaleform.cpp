//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#if defined( INCLUDE_SCALEFORM )
#include "basepanel.h"
#include "loadingscreen_scaleform.h"
#include "createmainmenuscreen_scaleform.h"
#include "iclientmode.h"
#include "ienginevgui.h"
#include "cs_gamerules.h"
#include "vgui/ILocalize.h"
#include "gametypes/igametypes.h"
#include "gametypes.h"
#include "gameui_interface.h"
#include "cs_gameplay_hints.h"
#include "../gameui/cstrike15/cstrike15basepanel.h"
#include "inputsystem/iinputsystem.h"
#include "vguitextwindow.h"
#include "teammenu_scaleform.h"
#include "ugc_utils.h"
#include "vstdlib/vstrtools.h"
#include <engine/IEngineSound.h>
#include "cs_workshop_manager.h"
#include "cstrike15/cstrike15_item_inventory.h"
#include "cstrike15/cs_player_rank_shared.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar cl_show_new_hint_delay( "cl_show_new_hint_delay", "10.0", FCVAR_DEVELOPMENTONLY );
ConVar cl_operation_premium_reminder( "cl_operation_premium_reminder_" MEDAL_SEASON_ACCESS_OPERATION_NAME, "0", FCVAR_ARCHIVE );
extern ConVar sv_disable_motd;

CLoadingScreenScaleform* CLoadingScreenScaleform::m_pInstance = NULL;

SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( ReadyForLoading ),
	SFUI_DECL_METHOD( AnimComplete ),
	SFUI_DECL_METHOD( SWFLoadError ),
	SFUI_DECL_METHOD( SWFLoadSuccess ),
	SFUI_DECL_METHOD( ContinueButtonPressed ),
	SFUI_DECL_METHOD( CloseAndUnload ),
SFUI_END_GAME_API_DEF( CLoadingScreenScaleform, LoadingScreen );

static float CEG_MIN_LOADING_RANGE = -1.f;
static float CEG_MAX_LOADING_RANGE = -1.f;

CLoadingScreenScaleform::CLoadingScreenScaleform() :	
	m_serverInfoReady( false ),
	m_pPendingKeyValues( NULL ),
	m_flLoadStartTime( -1.0f ),
	m_nAnimFrameCurrent( -1 ),
	m_nAnimFrameTarget( -1 ),
	m_bStartedUnblur( false ),
	m_bAnimPlaying( false ),
	m_bSWFLoadSuccess( false ),
	m_flTimeLastHintUpdate( -1.0f ),
	m_bCreatedMapLoadingScreen( false ),
	m_bCheckedForSWFAndFailed( false )
{	
	m_pendingAttributePurchaseActivate[0] = 0;
	m_nRequiredAttributeValueForPurchaseActivate = -1;
	m_pendingCommand[0] = NULL;
	m_readyForLoading = false;
	// Register for matchmaking events so that if the load fails, we can go back to the mainmenu
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );

	g_pInputSystem->SetSteamControllerMode( "MenuControls", this );

}

CLoadingScreenScaleform::~CLoadingScreenScaleform()
{
	g_pInputSystem->SetSteamControllerMode( NULL, this );

	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );

	if ( m_pPendingKeyValues )
	{
		m_pPendingKeyValues->deleteThis();
	}
}

void CLoadingScreenScaleform::FlashReady( void )
{
	m_flLoadStartTime = -1.0f;
	m_bStartedUnblur = false;

	CEG_MIN_LOADING_RANGE = RandomFloat( 0.3f, 0.7f );
	CEG_MAX_LOADING_RANGE = fclamp( CEG_MIN_LOADING_RANGE + RandomFloat( 0.1f, 0.3f ), 0.5f, 0.9f );


	Show();
}

void CLoadingScreenScaleform::Show( void )
{	
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", 0, NULL );
		}

		//Immediately hide the "Searching..." message box
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "LoadingScreenOpened" ) );

		//Immediately hide the main menu
		CCreateMainMenuScreenScaleform::ShowPanel( false, true );

		GameUI().SetBackgroundMusicDesired( false );
		GameUI().ReleaseBackgroundMusic( );
	
		ListenForGameEvent( "ugc_file_download_finished" );
	}
}

bool CLoadingScreenScaleform::PreUnloadFlash( void )
{
	return ScaleformFlashInterface::PreUnloadFlash();
}

#ifdef _PS3
IMatchSession *g_pMatchSessionChatRestrictionsAck = NULL;
IMatchSession *g_pMatchSessionChatRestrictionsPending = NULL;
CON_COMMAND_F( confirm_chat_restrictions, "Confirm that we have chat restrictions", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_HIDDEN )
{
	IMatchSession *pSession = g_pMatchFramework->GetMatchSession();
	if ( pSession == g_pMatchSessionChatRestrictionsPending )
	{
		g_pMatchSessionChatRestrictionsAck = g_pMatchSessionChatRestrictionsPending;
		g_pMatchSessionChatRestrictionsPending = NULL;
	}
}
#endif

void CLoadingScreenScaleform::PostUnloadFlash( void )
{
	StopListeningForAllEvents();
	GameStats_UserStartedPlaying( 0 );
	GameStats_ReportAction( "session_start" );
	GameStats_UserStartedPlaying( Plat_FloatTime() );
	m_pInstance = NULL;
	delete this;

#ifdef _PS3
	if( IsQuitting() )
		return;

	void ConfigurePSNPresenceStatusBasedOnCurrentSessionState( bool bCanUseSession = true );
	ConfigurePSNPresenceStatusBasedOnCurrentSessionState();

	IMatchSession *pSession = g_pMatchFramework->GetMatchSession();
	if ( pSession && ( g_pMatchSessionChatRestrictionsAck != pSession ) &&
		engine->IsConnected() &&
		!V_stricmp( pSession->GetSessionSettings()->GetString( "system/network" ), "LIVE" ) &&
		steamapicontext->SteamFriends()->GetUserRestrictions() )
	{
		g_pMatchSessionChatRestrictionsPending = pSession;
		GameUI().CreateCommandMsgBoxInSlot(
			CMB_SLOT_FULL_SCREEN, 
			"#SFUI_GameUI_ChatRestrictionPS3_Title", 
			"#SFUI_GameUI_ChatRestrictionPS3_Message", 
			true, 
			false, 
			"confirm_chat_restrictions\n", 
			NULL, 
			NULL, 
			NULL );
	}
#endif
}

void CLoadingScreenScaleform::LoadDialog( void )
{
	SFDevMsg( "CLoadingScreenScaleform::LoadDialog\n" );
	if ( !m_pInstance )
	{
		m_pInstance = new CLoadingScreenScaleform();
		SFUI_REQUEST_ELEMENT( SF_FULL_SCREEN_SLOT, g_pScaleformUI, CLoadingScreenScaleform, m_pInstance, LoadingScreen );
	}
}


void CLoadingScreenScaleform::UnloadDialog( void )
{
	SFDevMsg( "CLoadingScreenScaleform::UnloadDialog\n" );
	if ( m_pInstance )
	{
		SFDevMsg( "CLoadingScreenScaleform::RemoveFlashElement\n" );
		m_pInstance->RemoveFlashElement();
	}	
}

void CLoadingScreenScaleform::ReadyForLoading( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_readyForLoading = true;
	m_bCreatedMapLoadingScreen = false;
	if ( m_pendingCommand[0] )
	{
		{
			engine->ClientCmd_Unrestricted( m_pendingCommand );
		}
		m_pendingCommand[0] = NULL;

		char const *szMap = NULL;
		if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
			szMap = pMatchSession->GetSessionSettings()->GetString( "game/map", NULL );
		SetLoadingScreenElementsData( szMap );
	}
	else if ( m_pPendingKeyValues )
	{
		const char* mapName = m_pPendingKeyValues->GetString( "game/map" );
		const char* gameType = m_pPendingKeyValues->GetString( "game/type" );
		const char* gameMode = m_pPendingKeyValues->GetString( "game/mode" );	

		m_serverInfoReady = true;
		PopulateLevelInfo( mapName, gameType, gameMode );
		
		if ( !engine->IsTransitioningToLoad() )
		{
			g_pMatchFramework->ApplySettings( m_pPendingKeyValues );
		}

		m_pPendingKeyValues->deleteThis();
		m_pPendingKeyValues = NULL;
	}
	else
	{
		// Session always has the data, no reason not to enjoy using it
		char const *szMap = NULL;
		if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
			szMap = pMatchSession->GetSessionSettings()->GetString( "game/map", NULL );
		SetLoadingScreenElementsData( szMap );
	}
}

void CLoadingScreenScaleform::LoadDialogForCommand( const char* command )
{	
	LoadDialog();
	if ( m_pInstance )
	{
		if ( m_pInstance->m_readyForLoading )
		{
			{
				engine->ClientCmd_Unrestricted( command );
			}
		}
		else
		{
			m_pInstance->SetPendingCommand( command );
		}
	}
}

void CLoadingScreenScaleform::LoadDialogForKeyValues( KeyValues* keyValues )
{
	LoadDialog();
	if ( m_pInstance )
	{
		m_pInstance->SetPendingKeyValues( keyValues );
	}
}

void CLoadingScreenScaleform::SetPendingCommand( const char* command )
{
	V_strncpy( m_pendingCommand, command, sizeof( m_pendingCommand ) );
}

void CLoadingScreenScaleform::SetPendingKeyValues( KeyValues* keyValues )
{
	if ( m_pPendingKeyValues )
	{
		m_pPendingKeyValues->deleteThis();
	}
	m_pPendingKeyValues = keyValues->MakeCopy();
}

void CLoadingScreenScaleform::SetProgressInternal( float fraction )
{
	if ( FlashAPIIsValid() )
	{
		if ( m_flLoadStartTime < 0 )
			m_flLoadStartTime = Plat_FloatTime();

		//Set the flash loading bar progress		
		WITH_SLOT_LOCKED
		{			
			WITH_SFVALUEARRAY( data, 2 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, fraction );
				m_pScaleformUI->ValueArray_SetElement( data, 1, (float)( Plat_FloatTime() - m_flLoadStartTime ) );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setProgressFraction", data, 2 );
			}
		}

		if ( CEG_MIN_LOADING_RANGE >= 0.f	&& 
			 CEG_MAX_LOADING_RANGE >= 0.f	&&
			 fraction >= CEG_MIN_LOADING_RANGE && 
			 fraction <= CEG_MAX_LOADING_RANGE )
		{
			STEAMWORKS_TESTSECRETALWAYS();
			CEG_MIN_LOADING_RANGE = -1.f;
			CEG_MAX_LOADING_RANGE = -1.f;
		}

		int nProgress = fraction * 100;

		if ( nProgress > 0 )
		{
			m_nAnimFrameTarget =  ( ( nProgress / 25 ) );
		}

		if (  nProgress > 24 && m_bStartedUnblur == false )
		{
			m_pInstance->PlayUnblurAnimation();
		}

		if ( CEG_MIN_LOADING_RANGE >= 0.f	&& 
			CEG_MAX_LOADING_RANGE >= 0.f	&&
			fraction >= CEG_MIN_LOADING_RANGE && 
			fraction <= CEG_MAX_LOADING_RANGE )
		{
			STEAMWORKS_TESTSECRETALWAYS();
			CEG_MIN_LOADING_RANGE = -1.f;
			CEG_MAX_LOADING_RANGE = -1.f;
		}

		SetLoadingScreenElementsData( engine->GetLevelNameShort() );

		if ( Plat_FloatTime() >= m_flTimeLastHintUpdate + cl_show_new_hint_delay.GetFloat() )
		{
			// PopulateLevelInfo fills out the hint text the first time. After that point
			// update the hint text aprox every 10 seconds for cert reasons.
			PopulateHintText();
		}

// 		if ( ( m_pInstance->m_nAnimFrameTarget > 0 ) &&
// 			 ( m_pInstance->m_nAnimFrameCurrent < m_pInstance->m_nAnimFrameTarget ) &&
// 			 m_bAnimPlaying == false )
// 		{
// 			m_pInstance->PlayAnimation();
// 		}
	}
}

void CLoadingScreenScaleform::SetSecondaryProgressInternal( float fraction )
{
	if ( FlashAPIIsValid() )
	{
		//Set the flash loading bar progress		
		WITH_SLOT_LOCKED
		{			
			WITH_SFVALUEARRAY( data, 2 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, fraction );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setSecondaryProgressFraction", data, 2 );
			}
		}
	}
}

void CLoadingScreenScaleform::SetLoadingScreenElementsData( const char* mapName )
{
	if ( !m_serverInfoReady && mapName && mapName[0] )
	{
		// these are just flat out wrong, they are based on cvars that have not been communicated yet.
		// using these for unexpected error condition defaults
		int iGameType = g_pGameTypes->GetCurrentGameType();
		int iGameMode = g_pGameTypes->GetCurrentGameMode();
		char const *pGameType = g_pGameTypes->GetGameTypeFromInt( iGameType );
		char const *pGameMode = g_pGameTypes->GetGameModeFromInt( iGameType, iGameMode );

		IMatchSession *pIMatchSession = g_pMatchFramework->GetMatchSession();
		if ( pIMatchSession )
		{
			// the session has the CORRECT state
			pGameType = pIMatchSession->GetSessionSettings()->GetString( "game/type" );
			pGameMode = pIMatchSession->GetSessionSettings()->GetString( "game/mode" );
		}

		PopulateLevelInfo( mapName, pGameType, pGameMode );

		m_serverInfoReady = true;
	}
}


void CLoadingScreenScaleform::PopulateLevelInfo( const char* mapName, const char* szGameType, const char* szGameMode )
{
	int iGameType = -1;
	int iGameMode = -1;

	DevMsg( "PopulateLevelInfo: %s %s %s\n", mapName, szGameType, szGameMode );

	if ( V_stricmp( szGameType, "default" ) == 0 )
	{
		szGameType = "classic";
		szGameMode = "casual";
	}

	if ( g_pGameTypes->GetGameModeAndTypeIntsFromStrings( szGameType, szGameMode, iGameType, iGameMode ) )
	{
		const char* gameTypeNameID = NULL;
		const char* gameModeNameID = NULL;

		if ( g_pGameTypes->GetGameModeAndTypeNameIdsFromStrings( szGameType, szGameMode, gameTypeNameID, gameModeNameID ) )
		{
			PopulateLevelInfo ( mapName, gameTypeNameID, gameModeNameID, iGameType, iGameMode );
		}
	}
}



void CLoadingScreenScaleform::PopulateLevelInfo( const char* mapName, const char* gameTypeNameID, const char* gameModeNameID, int iGameType, int iGameMode )
{
	const int MAP_PREFIX_SIZE = 3;
	if ( !mapName || V_strlen( mapName ) < MAP_PREFIX_SIZE )
	{
		return;
	}

	const char* mapFileName = V_GetFileName( mapName );

	bool isGunGameProgressive =	( iGameType == CS_GameType_GunGame ) && ( iGameMode == CS_GameMode::GunGame_Progressive );
	bool isGunGameTRBomb =		( iGameType == CS_GameType_GunGame ) && ( iGameMode == CS_GameMode::GunGame_Bomb );
	bool isTrainingMap =		( iGameType == CS_GameType_Training ) && ( iGameMode == CS_GameMode::Training_Training );
	bool isGunGameDeathmatch =	( iGameType == CS_GameType_GunGame ) && ( iGameMode == CS_GameMode::GunGame_Deathmatch );
	bool isCoopGuardian =		( iGameType == CS_GameType_Cooperative ) && ( iGameMode == CS_GameMode::Cooperative_Guardian );
	bool isCooperativeMission =	( iGameType == CS_GameType_Cooperative ) && ( iGameMode == CS_GameMode::Cooperative_Mission );
	bool isCustomRules =		( iGameType == CS_GameType_Custom );

	//////////////////////////////////////////////////////////////////////////
	// Set Map Name
	//////////////////////////////////////////////////////////////////////////

	const wchar_t* translatedMapName = CSGameRules()->GetFriendlyMapName(mapName);
	
	//////////////////////////////////////////////////////////////////////////
	// Set Game Mode, type and/or style
	//////////////////////////////////////////////////////////////////////////

	const wchar_t* szGameTypeNiceName = g_pVGuiLocalize->Find( gameTypeNameID );
	const wchar_t* szGameModeNiceName = g_pVGuiLocalize->Find ( gameModeNameID );	

	//////////////////////////////////////////////////////////////////////////
	// Check to see if we need to load a blank loading screen
	//////////////////////////////////////////////////////////////////////////
	
	bool bHasJpg = m_bCheckedForSWFAndFailed;
	bool bCustomText = m_bCheckedForSWFAndFailed;
	bool bUseBlankScreen = m_bCheckedForSWFAndFailed;
	// make sure the loading screen exists for this map, otherwise load the blank one
	//if( !m_bCheckedForSWFAndFailed && !g_pFullFileSystem->FileExists( VarArgs( "resource/flash/Loading-%s.swf", mapFileName ) ) )
	if( !m_bCheckedForSWFAndFailed )
	{
		bUseBlankScreen = true;
		bHasJpg = true;
		bCustomText = true;
		m_bCheckedForSWFAndFailed = true;
	}

	//////////////////////////////////////////////////////////////////////////
	// Get the map path so we can load the thumbnail
	//////////////////////////////////////////////////////////////////////////
	char szPath[MAX_PATH];
	char szMapID[MAX_PATH];
	char szJPG[MAX_PATH];
	szJPG[0] = '\0';

	V_strcpy_safe( szPath, mapName );
	V_FixSlashes( szPath, '/' ); // internal path strings use forward slashes, make sure we compare like that.
	if ( V_strstr( szPath, "workshop/" ) )
	{
		V_snprintf( szMapID, MAX_PATH, "%llu", GetMapIDFromMapPath( szPath ) );
		V_StripFilename(szPath);

		V_snprintf( szJPG, MAX_PATH, "maps/%s/thumb%s.jpg", szPath, szMapID );
		if( !g_pFullFileSystem->FileExists( szJPG ) )
		{
			V_snprintf( szJPG, MAX_PATH, "maps/%s/%s.jpg", szPath, mapFileName );
			if( !g_pFullFileSystem->FileExists( szJPG ) )
			{
				// last chance.  try to see if we made one locally
				V_snprintf( szJPG, MAX_PATH, "maps/%s.jpg", mapFileName );
				if( !g_pFullFileSystem->FileExists( szJPG ) )
				{
					bHasJpg = false;
				}
			}
		}
	}
	else
	{
		V_snprintf( szJPG, MAX_PATH, "maps/%s.jpg", mapFileName );
		if( !g_pFullFileSystem->FileExists( szJPG ) )
		{
			bHasJpg = false;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Show Rules
	//////////////////////////////////////////////////////////////////////////

	wchar_t wszFinalScenarioNameString[1024];
	wszFinalScenarioNameString[0] = '\0';
	const wchar_t *scenarioName = L"";	
	const wchar_t *scenarioNameCredits = L"";	
	wchar_t wszText[1024] = {};

	char textFilename[MAX_PATH];
	V_snprintf( textFilename, MAX_PATH, "maps/%s.txt", mapFileName );

	bool bNeedRules = true;
	// Load the user's text file
	CUtlBuffer buf;
	if ( bCustomText && !V_stricmp( "overwatch", mapFileName ) )
	{
		bNeedRules = false;
		const wchar_t * const kwszOverwatchDesc = g_pVGuiLocalize->Find( "#CSGO_LoadingScreen_Overwatch" );
		Assert( kwszOverwatchDesc );
		V_wcscpy_safe( wszText, kwszOverwatchDesc );
		scenarioName = wszText;
	}
	else if ( bCustomText && g_pFullFileSystem->ReadFile( textFilename, NULL, buf ) )
	{
		bNeedRules = false;

		if ( (const char*)buf.Base() )
		{
			char chReplace[3] = { 0xD, 0xA, 0 };
			while ( char *psz = strstr( (char *)buf.Base(), chReplace ) )
			{
				Q_memmove( psz, psz + 1, Q_strlen( (char *)buf.Base() ) - ( psz - (char *)buf.Base() ) );
			}
		
			if ( StringHasPrefix( ( char * ) buf.Base(), "COMMUNITYMAPCREDITS:" ) )
			{
				const char *szText = "";
				szText = ( char * ) buf.Base();
				szText += 20;
				
				wchar_t wszCredits[1024];
				V_UTF8ToUnicode( szText, wszCredits, sizeof( wszCredits ) );

				const wchar_t * const kwszCommunityMapCreditsToken = g_pVGuiLocalize->Find( "#CSGO_LoadingScreen_CommunityMapCredits" );
				V_swprintf_safe( wszText, L"\n\n<font color='#727272'>" PRI_WS_FOR_WS L"</font><font color='#ff9844'>" PRI_WS_FOR_WS L"\n</font>\n",
					kwszCommunityMapCreditsToken, wszCredits );

				bNeedRules = true;
			}
			else if ( StringHasPrefix( ( char * ) buf.Base(), "RULESHERE" ) )
			{
				const char *szText = "";
				szText = (char *)buf.Base();
				szText += 9;
				V_UTF8ToUnicode( szText, wszText, sizeof( wszText ) );

				bNeedRules = true;
			}
			else
			{
				V_UTF8ToUnicode( (const char*)buf.Base(), wszText, sizeof( wszText ) );
			}		

			if ( bNeedRules )
				scenarioNameCredits = wszText;
			else
				scenarioName = wszText;
		}
		else
		{
			// the file exists, but it is blank
			wchar_t wszMap[64];	
			V_UTF8ToUnicode( textFilename, wszMap, sizeof( wszMap ) );
			V_snwprintf( wszText, ARRAYSIZE( wszText ), L"YOUR TEXT FILE IS BLANK!\n\nIf you intend for this space to be blank, open up '<font color='#72b4d0'>%s</font>', enter a single space and save the file.", wszMap );
			scenarioName = wszText;
		}

	}

	if ( bNeedRules == true )
	{
		if ( g_pGameTypes->GetLoadingScreenDataIsCorrect() == false )
		{
			szGameTypeNiceName = L"";
			szGameModeNiceName = g_pVGuiLocalize->Find( "SFUI_LOADING" );
			scenarioName = g_pVGuiLocalize->Find( "SFUI_LOADING" );
		}
		else if ( isTrainingMap )
		{
			scenarioName = g_pVGuiLocalize->Find( "SFUI_Rules_Training_Loading" );
		}
		else if ( isGunGameTRBomb ) 
		{
			scenarioName = g_pVGuiLocalize->Find( "SFUI_Rules_TRBomb_Loading" );
		}
		else if ( isGunGameProgressive )
		{		
			scenarioName = g_pVGuiLocalize->Find( "SFUI_Rules_GunGame_Progressive" );
		}
		else if ( isGunGameDeathmatch )
		{
			scenarioName = g_pVGuiLocalize->Find( "SFUI_Rules_Deathmatch_Loading" );
		}
		else if ( isCustomRules )
		{
			scenarioName = g_pVGuiLocalize->Find( "SFUI_Rules_Custom_Loading" );
		}
		else if ( isCoopGuardian )
		{
			scenarioName = g_pVGuiLocalize->Find( "SFUI_Rules_Guardian_Loading" );
		}
		else if ( isCooperativeMission )
		{
			scenarioName = g_pVGuiLocalize->Find( "SFUI_Rules_CoopMission_Loading" );
		}
		else
		{
			if ( StringHasPrefix( mapFileName, "cs_" ) )
			{
				if ( iGameMode == CS_GameMode::Classic_Casual )
				{
					scenarioName = g_pVGuiLocalize->Find( "SFUI_Rules_Hostage_Loading_Classic" );
				}
				else if ( iGameMode == CS_GameMode::Classic_Competitive )
				{
					scenarioName = g_pVGuiLocalize->Find( "SFUI_Rules_Hostage_Loading_Competetive" );
				}
				else if ( iGameMode == CS_GameMode::Classic_Competitive_Unranked )
				{
					scenarioName = g_pVGuiLocalize->Find( "SFUI_Rules_Hostage_Loading_Competetive_Unranked" );
				}
				else
				{
					scenarioName = g_pVGuiLocalize->Find( "SFUI_Rules_Hostage_Header" );
				}	
			}
			else if ( StringHasPrefix( mapFileName, "de_" ) )
			{
				if ( iGameMode == CS_GameMode::Classic_Casual )
				{
					scenarioName = g_pVGuiLocalize->Find( "SFUI_Rules_Bomb_Loading_Classic" );
				}
				else if ( iGameMode == CS_GameMode::Classic_Competitive )
				{
					scenarioName = g_pVGuiLocalize->Find( "SFUI_Rules_Bomb_Loading_Competetive" );
				}
				else if ( iGameMode == CS_GameMode::Classic_Competitive_Unranked )
				{
					scenarioName = g_pVGuiLocalize->Find( "SFUI_Rules_Bomb_Loading_Competetive_Unranked" );
				}
				else
				{
					scenarioName = g_pVGuiLocalize->Find( "SFUI_Rules_Bomb_Header" );
				}
			}	
			else
			{
				if ( iGameMode == CS_GameMode::Classic_Casual )
				{
					scenarioName = g_pVGuiLocalize->Find( "SFUI_Rules_ClassicCas_Unknown" );
				}
				else if ( iGameMode == CS_GameMode::Classic_Competitive )
				{
					scenarioName = g_pVGuiLocalize->Find( "SFUI_Rules_ClassicComp_Unknown" );
				}
			}
		}
	}

	V_swprintf_safe( wszFinalScenarioNameString, PRI_WS_FOR_WS PRI_WS_FOR_WS, scenarioName, scenarioNameCredits );

	//////////////////////////////////////////////////////////////////////////
	// Set Map Image
	//////////////////////////////////////////////////////////////////////////
	if ( FlashAPIIsValid() )
	{
		WITH_SFVALUEARRAY( data, 8 )
		{
			WITH_SLOT_LOCKED
			{
				// Pass through last team selected:
				ConVarRef player_teamplayedlast( "player_teamplayedlast" );

				m_pScaleformUI->ValueArray_SetElement( data, 0, mapFileName != NULL ? mapFileName : "" );
				m_pScaleformUI->ValueArray_SetElement( data, 1, player_teamplayedlast.GetInt() );
				m_pScaleformUI->ValueArray_SetElement( data, 2, translatedMapName != NULL ? translatedMapName : L"" );
				m_pScaleformUI->ValueArray_SetElement( data, 3, szGameTypeNiceName != NULL ? szGameTypeNiceName : L"" );
				m_pScaleformUI->ValueArray_SetElement( data, 4, szGameModeNiceName != NULL ? szGameModeNiceName : L"" );
				m_pScaleformUI->ValueArray_SetElement( data, 5, wszFinalScenarioNameString != NULL ? wszFinalScenarioNameString : L"" );
				m_pScaleformUI->ValueArray_SetElement( data, 6, bHasJpg ? szJPG : "" );
				m_pScaleformUI->ValueArray_SetElement( data, 7, bUseBlankScreen );

				if ( !m_bCreatedMapLoadingScreen )
				{
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setMapInfo", data, 8 );
					m_bCreatedMapLoadingScreen = true;
				}
				else
				{
					// just update the data
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "updateMapInfo", data, 8 );
				}
			}
		}
	}

	// now push down the overview icon data
	if ( FlashAPIIsValid() )
	{
		char tempfile[MAX_PATH];
		Q_snprintf( tempfile, sizeof( tempfile ), "resource/overviews/%s.txt", mapName );

		KeyValues* MapKeyValues = new KeyValues( mapName );
		if ( !MapKeyValues->LoadFromFile( g_pFullFileSystem, tempfile, "GAME" ) )
		{
			DevMsg( 1, "Error! CLoadingScreenScaleform::PopulateLevelInfo: couldn't load file %s.\n", tempfile );
			return;
		}

		WITH_SFVALUEARRAY( data, 3 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, "CTSpawn" );
			m_pScaleformUI->ValueArray_SetElement( data, 1, MapKeyValues->GetFloat( "CTSpawn_x" ) );
			m_pScaleformUI->ValueArray_SetElement( data, 2, MapKeyValues->GetFloat( "CTSpawn_y" ) );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetIconPosition", data, 3 );
		}

		WITH_SFVALUEARRAY( data, 3 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, "TSpawn" );
			m_pScaleformUI->ValueArray_SetElement( data, 1, MapKeyValues->GetFloat( "TSpawn_x" ) );
			m_pScaleformUI->ValueArray_SetElement( data, 2, MapKeyValues->GetFloat( "TSpawn_y" ) );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetIconPosition", data, 3 );
		}

		bool bShowBomb = !isGunGameProgressive;
		WITH_SFVALUEARRAY( data, 3 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, "Bomb" );
			m_pScaleformUI->ValueArray_SetElement( data, 1, bShowBomb ? MapKeyValues->GetFloat( "bomb_x" ) : 0 );
			m_pScaleformUI->ValueArray_SetElement( data, 2, bShowBomb ? MapKeyValues->GetFloat( "bomb_y" ) : 0 );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetIconPosition", data, 3 );
		}

		WITH_SFVALUEARRAY( data, 3 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, "BombA" );
			m_pScaleformUI->ValueArray_SetElement( data, 1, bShowBomb ? MapKeyValues->GetFloat( "bombA_x" ) : 0 );
			m_pScaleformUI->ValueArray_SetElement( data, 2, bShowBomb ? MapKeyValues->GetFloat( "bombA_y" ) : 0 );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetIconPosition", data, 3 );
		}

		WITH_SFVALUEARRAY( data, 3 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, "BombB" );
			m_pScaleformUI->ValueArray_SetElement( data, 1, bShowBomb ? MapKeyValues->GetFloat( "bombB_x" ) : 0 );
			m_pScaleformUI->ValueArray_SetElement( data, 2, bShowBomb ? MapKeyValues->GetFloat( "bombB_y" ) : 0 );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetIconPosition", data, 3 );
		}

		bool bShowHostages = !isGunGameProgressive;
		WITH_SFVALUEARRAY( data, 3 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, "Hostage1" );
			m_pScaleformUI->ValueArray_SetElement( data, 1, bShowHostages ? MapKeyValues->GetFloat( "Hostage1_x" ) : 0  );
			m_pScaleformUI->ValueArray_SetElement( data, 2, bShowHostages ? MapKeyValues->GetFloat( "Hostage1_y" ) : 0  );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetIconPosition", data, 3 );
		}

		WITH_SFVALUEARRAY( data, 3 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, "Hostage2" );
			m_pScaleformUI->ValueArray_SetElement( data, 1, bShowHostages ? MapKeyValues->GetFloat( "Hostage2_x" ) : 0  );
			m_pScaleformUI->ValueArray_SetElement( data, 2, bShowHostages ? MapKeyValues->GetFloat( "Hostage2_y" ) : 0  );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetIconPosition", data, 3 );
		}

		WITH_SFVALUEARRAY( data, 3 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, "Hostage3" );
			m_pScaleformUI->ValueArray_SetElement( data, 1, bShowHostages ? MapKeyValues->GetFloat( "Hostage3_x" ) : 0  );
			m_pScaleformUI->ValueArray_SetElement( data, 2, bShowHostages ? MapKeyValues->GetFloat( "Hostage3_y" ) : 0  );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetIconPosition", data, 3 );
		}

		WITH_SFVALUEARRAY( data, 3 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, "Hostage4" );
			m_pScaleformUI->ValueArray_SetElement( data, 1, bShowHostages ? MapKeyValues->GetFloat( "Hostage4_x" ) : 0  );
			m_pScaleformUI->ValueArray_SetElement( data, 2, bShowHostages ? MapKeyValues->GetFloat( "Hostage4_y" ) : 0  );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetIconPosition", data, 3 );
		}

		WITH_SFVALUEARRAY( data, 3 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, "Hostage5" );
			m_pScaleformUI->ValueArray_SetElement( data, 1, bShowHostages ? MapKeyValues->GetFloat( "Hostage5_x" ) : 0  );
			m_pScaleformUI->ValueArray_SetElement( data, 2, bShowHostages ? MapKeyValues->GetFloat( "Hostage5_y" ) : 0  );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetIconPosition", data, 3 );
		}

		WITH_SFVALUEARRAY( data, 3 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, "Hostage6" );
			m_pScaleformUI->ValueArray_SetElement( data, 1, bShowHostages ? MapKeyValues->GetFloat( "Hostage6_x" ) : 0  );
			m_pScaleformUI->ValueArray_SetElement( data, 2, bShowHostages ? MapKeyValues->GetFloat( "Hostage6_y" ) : 0  );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetIconPosition", data, 3 );
		}
	}

	PopulateHintText();
}

//updateMapInfo

void CLoadingScreenScaleform::PopulateHintText( void )
{
	if ( FlashAPIIsValid() )
	{
		const char* pszLocToken = g_CSGameplayHints.GetRandomLeastPlayedHint();
		if ( pszLocToken )
		{
			const wchar_t* pwsText = g_pVGuiLocalize->Find( pszLocToken );
			if ( pwsText )
			{	
				wchar_t wzFinal[1024] = L"";
				UTIL_ReplaceKeyBindings( pwsText, 0, wzFinal, sizeof( wzFinal ) );

				m_flTimeLastHintUpdate = Plat_FloatTime();
				WITH_SFVALUEARRAY( data, 1 )
				{
					WITH_SLOT_LOCKED
					{
						m_pScaleformUI->ValueArray_SetElement( data, 0, wzFinal );
						m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setHintText", data, 1 );
					}
				}
			}
		}
	}
}

void CLoadingScreenScaleform::SetStatusTextInternal( const char *statusText )
{
	if ( FlashAPIIsValid() )
	{
		const wchar_t* pwsText = g_pVGuiLocalize->Find( statusText );
		SetStatusTextInternal( pwsText );
	}
}

void CLoadingScreenScaleform::SetStatusTextInternal( const wchar_t *pwsText )
{
	if ( !FlashAPIIsValid() )
		return;

	if ( pwsText )
	{	
		m_flTimeLastHintUpdate = Plat_FloatTime();
		WITH_SFVALUEARRAY( data, 1 )
		{
			WITH_SLOT_LOCKED
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, pwsText );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setLoadingText", data, 1 );
			}
		}
	}
}

void CLoadingScreenScaleform::SetSecondaryStatusTextInternal( const wchar_t *pwsText )
{
	if ( !FlashAPIIsValid() )
		return;

	if ( pwsText )
	{	
		m_flTimeLastHintUpdate = Plat_FloatTime();
		WITH_SFVALUEARRAY( data, 1 )
		{
			WITH_SLOT_LOCKED
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, pwsText );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setSecondaryLoadingText", data, 1 );
			}
		}
	}
}

#ifdef _DEBUG
ConVar ui_loadingscreen_force_eligible( "ui_loadingscreen_force_eligible", "", FCVAR_DEVELOPMENTONLY );
#endif

void CLoadingScreenScaleform::OnEvent( KeyValues *pEvent )
{
	char const *pszEvent = pEvent->GetName();

	// If we're disconnected during the load process, make sure we go back to the main menu.
	if ( !V_stricmp( "OnEngineDisconnectReason", pszEvent ) )
	{
		CCreateMainMenuScreenScaleform::LoadDialog();
	}
	else if ( !V_stricmp( "OnLevelLoadingSetDefaultGameModeAndType", pszEvent ) )
	{
		char const *szMapName = pEvent->GetString( "mapname", "" );
		if ( szMapName )
		{
			g_pGameTypes->SetLoadingScreenDataIsCorrect( true );
			m_serverInfoReady = false;
			SetLoadingScreenElementsData( szMapName );
			//g_pGameTypes->SetLoadingScreenDataIsCorrect( false );

			//
			// Validate that we are eligible to play on the server
			//
			char const *szRequiredAttr = g_pGameTypes->GetRequiredAttrForMap( szMapName );
			int nRequiredAttrValue = g_pGameTypes->GetRequiredAttrValueForMap( szMapName );
			if ( szRequiredAttr && *szRequiredAttr && nRequiredAttrValue != -1 )
			{
				int nEligibilityLevel = ( engine->IsHLTV() || engine->IsClientLocalToActiveServer() ) ? 2 : 0;
				if ( !nEligibilityLevel )
				{
					char chCopyRequiredItem[128] = {0};
					V_strcpy_safe( chCopyRequiredItem, szRequiredAttr );

					engine->ExecuteClientCmd( "disconnect" );
					g_pMatchFramework->CloseSession();

					UnloadDialog();
					CCreateMainMenuScreenScaleform::LoadDialog();
				}
				else if ( nEligibilityLevel == 1 )
				{
					V_strcpy_safe( m_pendingAttributePurchaseActivate, szRequiredAttr );
					m_nRequiredAttributeValueForPurchaseActivate = nRequiredAttrValue;
				}
			}
		}
	}
	else if ( !V_stricmp( "OnMatchSessionUpdate", pszEvent ) )
	{
		IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
		if ( pMatchSession && !Q_stricmp( pEvent->GetString( "state" ), "created" ) )
		{
			char const *szMapName = pMatchSession->GetSessionSettings()->GetString( "game/map", NULL );
			if ( szMapName )
			{
				g_pGameTypes->SetLoadingScreenDataIsCorrect( true );
				m_serverInfoReady = false;
				SetLoadingScreenElementsData( szMapName );
				//g_pGameTypes->SetLoadingScreenDataIsCorrect( false );
			}
		}
	}
	else if ( !V_stricmp( "OnEngineDisconnectReason", pszEvent ) )
	{
		CCreateMainMenuScreenScaleform::LoadDialog();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Stolen from VGUI interface and called externally
///////////////////////////////////////////////////////////////////////////////////////////////////

bool CLoadingScreenScaleform::SetProgressPoint( float fraction, bool showDialog )
{
	if ( m_pInstance )
	{
		m_pInstance->SetProgressInternal( fraction );
	}
	else if ( showDialog )
	{
		LoadDialog();
	}
	
	return true;
}

void CLoadingScreenScaleform::SetStatusText( const char *statusText, bool showDialog )
{
	if ( m_pInstance )
	{
		m_pInstance->SetStatusTextInternal( statusText );
	}
}

void CLoadingScreenScaleform::SetStatusText( const wchar_t *desc )
{
	if ( m_pInstance )
	{
		m_pInstance->SetStatusTextInternal( desc );
	}
}

bool CLoadingScreenScaleform::SetSecondaryProgressPoint( float fraction )
{
	if ( m_pInstance )
	{
		m_pInstance->SetSecondaryProgressInternal( fraction );
	}

	return true;
}

void CLoadingScreenScaleform::SetSecondaryStatusText( const wchar_t *desc )
{
	if ( m_pInstance )
	{
		m_pInstance->SetSecondaryStatusTextInternal( desc );
	}
}

void CLoadingScreenScaleform::DisplayVACBannedError( void )
{
	Warning( "CLoadingScreenScaleform::DisplayVACBannedError Not Implemented\n" );
}

void CLoadingScreenScaleform::DisplayNoSteamConnectionError( void )
{
	Warning( "CLoadingScreenScaleform::DisplayNoSteamConnectionError Not Implemented\n" );
}

void CLoadingScreenScaleform::DisplayLoggedInElsewhereError( void )
{
	Warning( "CLoadingScreenScaleform::DisplayLoggedInElsewhereError Not Implemented\n" );
}

bool CLoadingScreenScaleform::IsOpen( void )
{
	return m_pInstance != NULL;
}

bool CLoadingScreenScaleform::LoadingProgressWantsIsolatedRender( bool bContextValid )
{
	if ( m_pInstance )
	{
		return m_pInstance->m_bAnimPlaying;
	}
	else
	{
		return false;
	}
}

void CLoadingScreenScaleform::AnimComplete( SCALEFORM_CALLBACK_ARGS_DECL )
{
	++m_nAnimFrameCurrent;

	m_bAnimPlaying = false;
}

void CLoadingScreenScaleform::SWFLoadError( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_bAnimPlaying = false;
	m_bSWFLoadSuccess = false;
}

void CLoadingScreenScaleform::SWFLoadSuccess( SCALEFORM_CALLBACK_ARGS_DECL )
{
	//m_bAnimPlaying = true;
	m_bSWFLoadSuccess = true;
}

void CLoadingScreenScaleform::PlayAnimation()
{
	if ( FlashAPIIsValid() && m_bSWFLoadSuccess && m_nAnimFrameCurrent < 3 )
	{
		m_bAnimPlaying = true;

		WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
		{
			m_pScaleformUI->ValueArray_SetElement( args, 0, m_nAnimFrameCurrent );
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "playAnim", args, 1 );
		}
	}
}

void CLoadingScreenScaleform::PlayUnblurAnimation()
{
	if ( FlashAPIIsValid() && m_bSWFLoadSuccess )
	{
		WITH_SLOT_LOCKED
		{
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "PlayUnblurAnimation", NULL, 0 );
		}

		m_bAnimPlaying = true;
		m_bStartedUnblur = true;
	}
}

void CLoadingScreenScaleform::ShowContinueButton( void )
{
	//Since demo playback starts immediately, skip the whole continue button flow
	if ( engine->IsPlayingDemo() || engine->IsPlayingTimeDemo() )
	{
		CloseLoadingScreen();
	}
	else if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowContinueButton", NULL, 0 );
		}
	}
}

void CLoadingScreenScaleform::FinishLoading( void )
{
	DevMsg( "CLoadingScreenScaleform::FinishLoading\n" );
	if ( m_pInstance )
	{
		m_pInstance->ShowContinueButton();
	}

	bool bQueueMode = false;
	if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
		bQueueMode = !!*pMatchSession->GetSessionSettings()->GetString( "game/mmqueue" );

	if ( bQueueMode )
	{
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "UI.QueuedMatchStartLoading" );
	}
}

void ReadyToJoinGameProceedToMotdAndTeamSelect()
{
	if ( !engine->IsConnected() )
		return;

	CTextWindow *pHTMLPanel = (CTextWindow *)GetViewPortInterface()->FindPanelByName( PANEL_INFO );
	//CCSTeamMenuScaleform* pTeamMenu = (CCSTeamMenuScaleform*)GetViewPortInterface()->FindPanelByName( PANEL_TEAM );


	if ( sv_disable_motd.GetBool() || engine->IsClientLocalToActiveServer() || !pHTMLPanel->HasMotd() || ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() ) )
	{
		DevMsg( "CLoadingScreenScaleform::CloseLoadingScreen-doing(joingame)\n" );
		GetViewPortInterface()->ShowPanel( PANEL_INFO, false );	
		engine->ClientCmd( "joingame\n" );	
	}
	else
	{
		DevMsg( "CLoadingScreenScaleform::CloseLoadingScreen-doing(motd)\n" );
		pHTMLPanel->ShowPanel2(true);
		GetViewPortInterface()->ShowPanel( PANEL_TEAM, false );	
	}
}

CON_COMMAND_F( ready_to_join_game_proceed_to_motd_and_team_select, "Ready to join game, proceed to motd and team select", FCVAR_CLIENTCMD_CAN_EXECUTE | FCVAR_HIDDEN )
{
	ReadyToJoinGameProceedToMotdAndTeamSelect();
}

void CLoadingScreenScaleform::CloseLoadingScreen( void )
{
	DevMsg( "CLoadingScreenScaleform::CloseLoadingScreen\n" );

	if( m_pInstance ) // <sergiy> speculative fix for CSN-9401; this seems to be a pattern in here, and it crashed when m_pInstance was NULL
	{
		char chPendingAttributePurchaseActivate[sizeof( m_pInstance->m_pendingAttributePurchaseActivate )];
		V_strcpy_safe( chPendingAttributePurchaseActivate, m_pInstance->m_pendingAttributePurchaseActivate );

		m_pInstance->CloseScreenUpdateScaleform();

		static ConVarRef cv_ignore_ui_activate_key( "ignore_ui_activate_key" );
		cv_ignore_ui_activate_key.SetValue( 0 );

		ReadyToJoinGameProceedToMotdAndTeamSelect();
	}
}

void CLoadingScreenScaleform::CloseScreenUpdateScaleform( void )
{
	DevMsg( "CLoadingScreenScaleform::CloseScreenUpdateScaleform\n" );
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "StartHide", NULL, 0 );
		}
	}
}

void CLoadingScreenScaleform::CloseAndUnload( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_bCreatedMapLoadingScreen = false;
	m_bCheckedForSWFAndFailed = false;

	// tell scaleform that we are here to respond and can return
	m_pScaleformUI->Params_SetResult( obj, true );

	DevMsg( "CLoadingScreenScaleform::CloseAndUnload\n" );

	UnloadDialog();
}

void CLoadingScreenScaleform::ContinueButtonPressed( SCALEFORM_CALLBACK_ARGS_DECL )
{
	DevMsg( "CLoadingScreenScaleform::ContinueButtonPressed\n" );
	CloseLoadingScreen();
}

void CLoadingScreenScaleform::FireGameEvent( IGameEvent *event )
{
	if ( !V_strcmp( event->GetName(), "ugc_file_download_finished" ) )
	{
		IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
		if ( pMatchSession )
		{
			char const *szMapName = pMatchSession->GetSessionSettings()->GetString( "game/map", NULL );
			if ( szMapName && !StringIsEmpty( szMapName ) )
			{
				UGCHandle_t hcontent = event->GetUint64( "hcontent", 0 );
				char filebuf[MAX_PATH];
				WorkshopManager().GetUGCFullPath( hcontent, filebuf, sizeof( filebuf ) );

				// if it's a jpg
				if ( !V_stricmp( "jpg", V_GetFileExtension( filebuf ) ) )
				{
					V_FixSlashes( filebuf, '/' );
					char szMapPath[MAX_PATH];
					V_ExtractFilePath( szMapName, szMapPath, sizeof( szMapPath ) );
					// rough check to make sure this is the jpg for this map... possible false positives but not likely
					if ( V_stristr( filebuf, szMapPath ) )
					{
						SetLoadingScreenElementsData( szMapName );
					}
				}
			}
		}
	}
}

#endif //INCLUDE_SCALEFORM
