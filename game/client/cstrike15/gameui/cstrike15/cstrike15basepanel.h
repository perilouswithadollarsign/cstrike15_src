//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CSTRIKE15BASEPANEL_H
#define CSTRIKE15BASEPANEL_H

#ifdef _WIN32
#pragma once
#endif

#ifdef _PS3
#include "steam/steam_api.h"
#endif

#include "basepanel.h"
#include "messagebox_scaleform.h"
#include "GameEventListener.h"
#include "splitscreensignon.h"

#ifdef _PS3
void MarkRegisteredKnownConsoleUserSteamIDToUnregisterLater( CSteamID steamIdConsoleUser );
void ConfigurePSNPresenceStatusBasedOnCurrentSessionState( bool bCanUseSession = true );
#endif

//-----------------------------------------------------------------------------
// Purpose: This is the panel at the top of the panel hierarchy for GameUI
//			It handles all the menus, background images, and loading dialogs
//-----------------------------------------------------------------------------
class CCStrike15BasePanel: public CBaseModPanel, public IMessageBoxEventCallback, public IMatchEventsSink, public CGameEventListener
{
	DECLARE_CLASS_SIMPLE( CCStrike15BasePanel, CBaseModPanel );

public:
	CCStrike15BasePanel();
	virtual ~CCStrike15BasePanel();

	virtual void OnEvent( KeyValues *pEvent );

	virtual void FireGameEvent( IGameEvent *event );

#if defined( _X360 )
	// Prompts the user via the Xbox Guide to switch to Game Chat channel, as necessary
	void	Xbox_PromptSwitchToGameVoiceChannel( void );

	// Is the local user using Party Chat currently?
	bool	Xbox_IsPartyChatEnabled( void );
#endif // _X360

#if defined(INCLUDE_SCALEFORM)
	virtual void OnOpenCreateStartScreen( void ); // [jason] provides the "Press Start" screen interface
	virtual void DismissStartScreen( void );
	virtual bool IsStartScreenActive( void );

	virtual void OnOpenCreateMainMenuScreen( void ); 
	virtual void DismissMainMenuScreen( void );
	virtual void RestoreMainMenuScreen( void );
	virtual void DismissAllMainMenuScreens( bool bHideMainMenuOnly = false );

	void RestoreMPGameMenu( void );

	virtual void ShowScaleformMainMenu( bool bShow );
	virtual bool IsScaleformMainMenuActive( void );

	virtual void OnOpenCreateSingleplayerGameDialog( bool bMatchmakingFilter );
	virtual void OnOpenCreateMultiplayerGameDialog( void );
	virtual void OnOpenCreateMultiplayerGameCommunity( void );
	virtual void OnOpenDisconnectConfirmationDialog( void );
	virtual void OnOpenQuitConfirmationDialog( bool bForceToDesktop = false );

	virtual	void OnOpenServerBrowser();
	virtual void OnOpenCreateLobbyScreen( bool bIsHost = false );
	virtual void OnOpenLobbyBrowserScreen( bool bIsHost = false );
	virtual void UpdateLobbyScreen( void );
	virtual void UpdateMainMenuScreen();
	virtual void UpdateLobbyBrowser( void );

	virtual void OnOpenMessageBox( char const *pszTitle, char const *pszMessage, char const *pszButtonLegend, DWORD dwFlags, IMessageBoxEventCallback *pEventCallback = NULL, CMessageBoxScaleform** ppInstance = NULL, wchar_t const *pszWideMessage = NULL );
	virtual void OnOpenMessageBoxInSlot( int slot, char const *pszTitle, char const *pszMessage, char const *pszButtonLegend, DWORD dwFlags, IMessageBoxEventCallback *pEventCallback = NULL, CMessageBoxScaleform** ppInstance = NULL );
	virtual void OnOpenMessageBoxThreeway( char const *pszTitle, char const *pszMessage, char const *pszButtonLegend, char const *pszThirdButtonLabel, DWORD dwFlags, IMessageBoxEventCallback *pEventCallback = NULL, CMessageBoxScaleform** ppInstance = NULL );

	virtual void CreateCommandMsgBox( const char* pszTitle, const char* pszMessage, bool showOk = true, bool showCancel = false, const char* okCommand = NULL, const char* cancelCommand = NULL, const char* closedCommand = NULL, const char* pszLegend = NULL );
	virtual void CreateCommandMsgBoxInSlot( ECommandMsgBoxSlot slot, const char* pszTitle, const char* pszMessage, bool showOk = true, bool showCancel = false, const char* okCommand = NULL, const char* cancelCommand = NULL, const char* closedCommand = NULL, const char* pszLegend = NULL );

	virtual void ShowMatchmakingStatus( void );

	// returns true if message box is displayed successfully
	virtual bool ShowLockInput(  void );

	virtual void OnOpenPauseMenu( void );
	virtual void DismissPauseMenu( void );
	virtual void RestorePauseMenu( void );
	virtual void OnOpenControllerDialog( void );
	virtual void OnOpenSettingsDialog( void );
	virtual void OnOpenMouseDialog();
	virtual void OnOpenKeyboardDialog();
	virtual void OnOpenMotionControllerMoveDialog();
	virtual void OnOpenMotionControllerSharpshooterDialog();
	virtual void OnOpenMotionControllerDialog();
	virtual void OnOpenMotionCalibrationDialog();
	virtual void OnOpenVideoSettingsDialog();
	virtual void OnOpenOptionsQueued();
	virtual void OnOpenAudioSettingsDialog();

	virtual void OnOpenUpsellDialog( void );

	virtual void OnOpenHowToPlayDialog( void );	
	

	virtual void ShowScaleformPauseMenu( bool bShow );
	virtual bool IsScaleformPauseMenuActive( void );
	virtual bool IsScaleformPauseMenuVisible( void );

	virtual bool OnMessageBoxEvent( MessageBoxFlags_t buttonPressed );

	virtual void OnOpenMedalsDialog();
	virtual void OnOpenStatsDialog();
	virtual void CloseMedalsStatsDialog();


	virtual void OnOpenLeaderboardsDialog();
	virtual void OnOpenCallVoteDialog();
	virtual void OnOpenMarketplace();
	virtual void UpdateLeaderboardsDialog();
	virtual void CloseLeaderboardsDialog();
	virtual void StartExitingProcess( void );

	virtual void RunFrame( void );

	void DoCommunityQuickPlay( void );

protected:
	virtual void LockInput( void );
	virtual void UnlockInput( void );

    virtual bool IsScaleformIntroMovieEnabled( void );
    virtual void CreateScaleformIntroMovie( void );
    virtual void DismissScaleformIntroMovie( void );
   	virtual void OnPlayCreditsVideo( void );

    void CheckIntroMovieStaticDependencies( void );


protected:
	enum CCSOnClosedCommand
	{
		ON_CLOSED_NULL,
		ON_CLOSED_DISCONNECT,
		ON_CLOSED_QUIT,
		ON_CLOSED_RESTORE_PAUSE_MENU,
		ON_CLOSED_RESTORE_MAIN_MENU,
		ON_CLOSED_DISCONNECT_TO_MP_GAME_MENU, // quit from a game and return to the create game menu instead of main menu
	};

	CCSOnClosedCommand m_OnClosedCommand;

	bool	m_bMigratingActive;

	bool	m_bShowRequiredGameVoiceChannelUI;
	CountdownTimer m_GameVoiceChannelRecheckTimer;

    bool m_bNeedToStartIntroMovie;
    bool m_bTestedStaticIntroMovieDependencies;


private:

#if defined ( _PS3 )&& !defined ( NO_STEAM )

	void OnGameBootCheckInvites();
	void OnGameBootInstallTrophies();
	void ShowFatalError( uint32 unSize );
	void PerformPS3GameBootWork();
	void OnGameBootVerifyPs3DRM();
	void OnGameBootDrmVerified();
	void OnGameBootSaveContainerReady();

	STEAM_CALLBACK_MANUAL( CCStrike15BasePanel, Steam_OnUserStatsReceived, UserStatsReceived_t, m_CallbackOnUserStatsReceived );
	STEAM_CALLBACK_MANUAL( CCStrike15BasePanel, Steam_OnPS3TrophiesInstalled, PS3TrophiesInstalled_t, m_CallbackOnPS3TrophiesInstalled );
	STEAM_CALLBACK_MANUAL( CCStrike15BasePanel, Steam_OnPSNGameBootInviteResult, PSNGameBootInviteResult_t, m_CallbackOnPSNGameBootInviteResult );
	STEAM_CALLBACK_MANUAL( CCStrike15BasePanel, Steam_OnLobbyInvite, LobbyInvite_t, m_CallbackOnLobbyInvite );

#endif// _PS3 && !NO_STEAM

#endif// Scaleform

	SplitScreenSignonWidget* m_pSplitScreenSignon;
	bool	m_bStartLogoIsShowing;
	bool m_bServerBrowserWarningRaised;
	bool m_bCommunityQuickPlayWarningRaised;
	bool m_bCommunityServerWarningIssued;
	bool m_bGameIsShuttingDown;
};

#ifdef _GAMECONSOLE
void GameStats_UserStartedPlaying( float flTime );
void GameStats_ReportAction( char const *szReportAction );
#else
inline void GameStats_UserStartedPlaying( float flTime ) {}
inline void GameStats_ReportAction( char const *szReportAction ) {}
#endif

#endif // CSTRIKE15BASEPANEL_H

