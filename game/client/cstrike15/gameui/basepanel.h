//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BASEPANEL_H
#define BASEPANEL_H
#ifdef _WIN32
#pragma once
#endif

#if !defined( NO_STEAM )
	#include "utlvector.h"
	#include "clientsteamcontext.h"
#endif

#if defined( SWARM_DLL )

#include "swarm/basemodpanel.h"
inline BaseModUI::CBaseModPanel * BasePanel() { return &BaseModUI::CBaseModPanel::GetSingleton(); }

#elif defined( PORTAL2_UITEST_DLL )

#include "portal2uitest/basemodpanel.h"
inline BaseModUI::CBaseModPanel * BasePanel() { return &BaseModUI::CBaseModPanel::GetSingleton(); }

#else

#define BASEPANEL_LEGACY_SOURCE1

#include "vgui_controls/Panel.h"
#include "vgui_controls/PHandle.h"
#include "vgui_controls/MenuItem.h"
#include "vgui_controls/MessageDialog.h"
#include "keyvalues.h"
#include "utlvector.h"
#include "tier1/commandbuffer.h"
#include "vgui_controls/footerpanel.h"

#include "ixboxsystem.h"

#if !defined( _GAMECONSOLE )
#include "xbox/xboxstubs.h"
#endif

enum
{
	DIALOG_STACK_IDX_STANDARD,
	DIALOG_STACK_IDX_WARNING,
	DIALOG_STACK_IDX_ERROR,
};

class CBackgroundMenuButton;
class CGameMenu;

// X360TBD: Move into a separate module when finished
class CMessageDialogHandler
{
public:
	CMessageDialogHandler();
	void ShowMessageDialog( int nType, vgui::Panel *pOwner );
	void CloseMessageDialog( const uint nType = 0 );
	void CloseAllMessageDialogs();
	void CreateMessageDialog( const uint nType, const char *pTitle, const char *pMsg, const char *pCmdA, const char *pCmdB, vgui::Panel *pCreator, bool bShowActivity = false );
	void ActivateMessageDialog( int nStackIdx );
	void PositionDialogs( int wide, int tall );
	void PositionDialog( vgui::PHandle dlg, int wide, int tall );

private:
	static const int MAX_MESSAGE_DIALOGS = 3;
	vgui::DHANDLE< CMessageDialog > m_hMessageDialogs[MAX_MESSAGE_DIALOGS];
	int							m_iDialogStackTop;
};

//-----------------------------------------------------------------------------
// Purpose: EditablePanel that can replace the GameMenuButtons in CBaseModPanel
//-----------------------------------------------------------------------------
class CMainMenuGameLogo : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CMainMenuGameLogo, vgui::EditablePanel );
public:
	CMainMenuGameLogo( vgui::Panel *parent, const char *name );

	virtual void ApplySettings( KeyValues *inResourceData );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	int GetOffsetX(){ return m_nOffsetX; }
	int GetOffsetY(){ return m_nOffsetY; }

private:
	int m_nOffsetX;
	int m_nOffsetY;
};

//-----------------------------------------------------------------------------
// Purpose: Transparent menu item designed to sit on the background ingame
//-----------------------------------------------------------------------------
class CGameMenuItem : public vgui::MenuItem
{
	DECLARE_CLASS_SIMPLE( CGameMenuItem, vgui::MenuItem );
public:
	CGameMenuItem(vgui::Menu *parent, const char *name);

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PaintBackground( void );
	void SetRightAlignedText( bool state );

private:
	bool		m_bRightAligned;
};

// dgoodenough - GCC does not treat "friend class foo;" as a forward declaration of foo, so
// explicitly forward declare class CAsyncCtxOnDeviceAttached here.
// PS3_BUILDFIX
class CAsyncCtxOnDeviceAttached;

//-----------------------------------------------------------------------------
// Purpose: This is the panel at the top of the panel hierarchy for GameUI
//			It handles all the menus, background images, and loading dialogs
//-----------------------------------------------------------------------------
class CBaseModPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CBaseModPanel, vgui::Panel );

protected:
	explicit CBaseModPanel( const char *panelName );

public:
	virtual ~CBaseModPanel();

public:
	//
	// Implementation of async jobs
	//	An async job is enqueued by calling "ExecuteAsync" with the proper job context.
	//	Job's function "ExecuteAsync" is called on a separate thread.
	//	After the job finishes the "Completed" function is called on the
	//	main thread.
	//
	class CAsyncJobContext
	{
	public:
		explicit CAsyncJobContext( float flLeastExecuteTime = 0.0f ) : m_flLeastExecuteTime( flLeastExecuteTime ), m_hThreadHandle( NULL )  {}
		virtual ~CAsyncJobContext() {}

		virtual void ExecuteAsync() = 0;		// Executed on the secondary thread
		virtual void Completed() = 0;			// Executed on the main thread

	public:
		void * volatile m_hThreadHandle;		// Handle to an async job thread waiting for
		float m_flLeastExecuteTime;				// Least amount of time this job should keep executing
	};

	CAsyncJobContext *m_pAsyncJob;
	void ExecuteAsync( CAsyncJobContext *pAsync );


public:
	// notifications
	virtual void OnLevelLoadingStarted( const char *levelName, bool bShowProgressDialog );
	virtual void OnLevelLoadingFinished();
	virtual bool UpdateProgressBar(float progress, const char *statusText, bool showDialog = true ) { return false; }	

	// update the taskbar a frame
	virtual void RunFrame();

	// fades to black then runs an engine command (usually to start a level)
	void FadeToBlackAndRunEngineCommand( const char *engineCommand );

	// sets the blinking state of a menu item
	void SetMenuItemBlinkingState( const char *itemName, bool state );

	// handles gameUI being shown
	virtual void OnGameUIActivated();

	// game dialogs
	void OnOpenNewGameDialog( const char *chapter = NULL );
	void OnOpenBonusMapsDialog();
	void OnOpenLoadGameDialog();
	void OnOpenLoadGameDialog_Xbox();
	void OnOpenSaveGameDialog();
	void OnOpenSaveGameDialog_Xbox();
	void OnCloseServerBrowser();
	void OnOpenFriendsDialog();
	void OnOpenDemoDialog();
	// Overridden with the Scaleform dialog box in Cstrike15Basepanel
	virtual void OnOpenQuitConfirmationDialog( bool bForceToDesktop = false );
	void OnOpenChangeGameDialog();
	void OnOpenPlayerListDialog();
	void OnOpenBenchmarkDialog();
	void OnOpenOptionsDialog();
	void OnOpenOptionsDialog_Xbox();
	void OnOpenLoadCommentaryDialog();
	void OpenLoadSingleplayerCommentaryDialog();
	void OnOpenAchievementsDialog();
	void OnOpenAchievementsDialog_Xbox();
	void OnOpenCSAchievementsDialog();
	virtual void OnOpenSettingsDialog();
	virtual void OnOpenControllerDialog();
	virtual void OnOpenMouseDialog();
	virtual void OnOpenKeyboardDialog();
	virtual void OnOpenMotionControllerMoveDialog();
	virtual void OnOpenMotionControllerSharpshooterDialog();
	virtual void OnOpenMotionControllerDialog();
	virtual void OnOpenMotionCalibrationDialog();
	virtual void OnOpenVideoSettingsDialog();
	virtual void OnOpenOptionsQueued();
	virtual void OnOpenAudioSettingsDialog();

	// [jason] For displaying medals/stats in Cstrike15
	virtual void OnOpenMedalsDialog();
	virtual void OnOpenStatsDialog();
	virtual void CloseMedalsStatsDialog();

	// [jason] For displaying Leaderboards in Cstrike15
	virtual void OnOpenLeaderboardsDialog();
	virtual void OnOpenCallVoteDialog();
	virtual void OnOpenMarketplace();
	virtual void UpdateLeaderboardsDialog();
	virtual void CloseLeaderboardsDialog();

	virtual void OnOpenDisconnectConfirmationDialog();

	// gameconsole
	void SystemNotification( const int notification );
	void ShowMessageDialog( const uint nType, vgui::Panel *pParent = NULL );
	void CloseMessageDialog( const uint nType );
	void OnChangeStorageDevice();
	bool ValidateStorageDevice();
	bool ValidateStorageDevice( int *pStorageDeviceValidated );
	void OnCreditsFinished();

	virtual void OnOpenCreateSingleplayerGameDialog( bool bMatchmakingFilter = false ) {}
	virtual void OnOpenCreateMultiplayerGameDialog();
	virtual void OnOpenCreateMultiplayerGameCommunity();
	virtual void ShowMatchmakingStatus() {}

	// returns true if message box is displayed successfully
	virtual bool ShowLockInput( void ) { return false; }

	virtual void OnOpenHowToPlayDialog() {}	

	virtual void OnOpenServerBrowser() {}
	virtual void OnOpenCreateLobbyScreen( bool bIsHost = false ) {}
	virtual void OnOpenLobbyBrowserScreen( bool bIsHost = false ) {}
	virtual void UpdateLobbyScreen( void ) {} 
	virtual void UpdateMainMenuScreen() {}
	virtual void UpdateLobbyBrowser( void ) {} 
	
	// Determine if we have an active team lobby we are part of
	virtual bool InTeamLobby( void );

	// [jason]  Provides the "Press Start" screen interface (show, hide, and reset flags)
	virtual void	OnOpenCreateStartScreen( void ); 
	virtual void	HandleOpenCreateStartScreen( void );
	virtual void	DismissStartScreen( void );
	virtual bool	IsStartScreenActive( void );

	// Do we want the start screen to come up when we boot up, before we reach main menu?
	bool			IsStartScreenEnabled( void )	{ return m_bShowStartScreen; }

	// [jason] Start the sign-in blade
	void			SignInFromStartScreen( void );

	// [jason] Dismiss the start screen and commit the user once they've signed in
	void			CompleteStartScreenSignIn( void );

	// [jason] Callback for the CreateStartScreen interface to allow us to complete the signin process
	void			NotifySignInCompleted(int userID = -1);
	void			NotifySignInCancelled( void );

	// [jason] Helper function to allow show/hide of the standard Valve main menu (which also enables/disables its processing)
	void			ShowMainMenu( bool bShow );

	virtual void OnPlayCreditsVideo( void );

	// [jason] New scaleform Main Menu setup
	virtual void	OnOpenCreateMainMenuScreen( void ); 
	virtual void	DismissMainMenuScreen( void );
	virtual void	RestoreMainMenuScreen( void );

	// Tear down all screens, and either tear down or just hide the main/pause menu (by default, they are also torn down)
	virtual void	DismissAllMainMenuScreens( bool bHideMainMenuOnly = false );

	// Controls whether we allow the next main menu transition to use the Scaleform main menu, or the vgui version
	void			EnableScaleformMainMenu( bool bEnable ) { m_bScaleformMainMenuEnabled = bEnable; }
	bool			IsScaleformMainMenuEnabled( void ) { return m_bScaleformMainMenuEnabled; }

	// [jason] Notification that a vgui dialog has completed, in case we need to restore Scaleform menu
	void			NotifyVguiDialogClosed( void );

	virtual void	OnOpenPauseMenu( void );
	virtual void	DismissPauseMenu( void );
	virtual void	RestorePauseMenu( void );

	virtual void	OnOpenUpsellDialog( void );

	void			OnMakeGamePublic( void );

	virtual void	ShowScaleformPauseMenu( bool bShow );
	virtual bool	IsScaleformPauseMenuActive( void );
	virtual bool	IsScaleformPauseMenuVisible( void );

	bool			IsScaleformPauseMenuEnabled( void ) { return m_bScaleformPauseMenuEnabled; }

	KeyValues *GetConsoleControlSettings( void );

	// forces any changed options dialog settings to be applied immediately, if it's open
	void ApplyOptionsDialogSettings();

	vgui::AnimationController *GetAnimationController( void ) { return m_pConsoleAnimationController; }
	void RunCloseAnimation( const char *animName );
	void RunAnimationWithCallback( vgui::Panel *parent, const char *animName, KeyValues *msgFunc );
	void PositionDialog( vgui::PHandle dlg );

	virtual void OnSizeChanged( int newWide, int newTall );

	void ArmFirstMenuItem( void );

	void OnGameUIHidden();

	virtual void CloseBaseDialogs( void );
	bool IsWaitingForConsoleUI( void ) { return m_bWaitingForStorageDeviceHandle || m_bWaitingForUserSignIn || m_bXUIVisible; }

	bool LoadingProgressWantsIsolatedRender( bool bContextValid );

	bool IsLevelLoading( void ) const { return m_bLevelLoading; }

#if defined( _GAMECONSOLE )
	CON_COMMAND_MEMBER_F( CBaseModPanel, "gameui_reload_resources", Reload_Resources, "Reload the Xbox 360 UI res files", 0 );
#endif

protected:
	virtual void PaintBackground();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	
	// [jason] Input locking and unlocking based on user sign-in
	virtual void LockInput( void );
	virtual void UnlockInput( void );

	// [jason] Allow toggle of the new scaleform version of the main menu
	virtual void	ShowScaleformMainMenu( bool bShow );

	virtual bool	IsScaleformMainMenuActive( void );

    virtual bool    IsScaleformIntroMovieEnabled( void ) { return false; }
    virtual void    CreateScaleformIntroMovie( void ) {}
    virtual void    DismissScaleformIntroMovie( void ) {}

    void CreateStartScreenIfNeeded( void );
    int CheckForAnyKeyPressed( bool bCheckKeyboard );

	void	ForcePrimaryUserId( int id ) { m_primaryUserId = id; }

public:
	// FIXME: This should probably become a friend relationship between the classes
	bool HandleSignInRequest( const char *command );
	bool HandleStorageDeviceRequest( const char *command );
	void ClearPostPromptCommand( const char *pCompletedCommand );

	bool IsSinglePlayer() const { return m_bSinglePlayer; }
	void SetSinglePlayer( bool singlePlayer ) { m_bSinglePlayer = singlePlayer; }
	void UpdateRichPresenceInfo();

	virtual void OnCommand(const char *command);

	//HACK: could make this generic...
	bool	m_bReturnToMPGameMenuOnDisconnect;
	bool	m_bForceQuitToDesktopOnDisconnect;

protected:
	virtual void StartExitingProcess( void );
	void SetStatsLoaded( bool value ) { m_bStatsLoaded = value; }
	bool GetStatsLoaded( void ) const { return m_bStatsLoaded; }

private:
	enum EBackgroundState
	{
		BACKGROUND_INITIAL,
		BACKGROUND_LOADING,
		BACKGROUND_MAINMENU,
		BACKGROUND_LEVEL,
		BACKGROUND_DISCONNECTED,
		BACKGROUND_EXITING,			// Console has started an exiting state, cannot be stopped
	};
	void SetBackgroundRenderState(EBackgroundState state);

	friend class CAsyncCtxOnDeviceAttached;
	void OnDeviceAttached( void );
	void OnCompletedAsyncDeviceAttached( CAsyncCtxOnDeviceAttached *job );

	void IssuePostPromptCommand( void );

	void UpdateBackgroundState();

	// sets the menu alpha [0..255]
	void SetMenuAlpha(int alpha);

	// menu manipulation
	void CreatePlatformMenu();
	void CreateGameMenu();
	void CreateGameLogo();
	void CheckBonusBlinkState();
	void UpdateGameMenus();
	CGameMenu *RecursiveLoadGameMenu(KeyValues *datafile);


	bool IsPromptableCommand( const char *command );
	bool CommandRequiresSignIn( const char *command );
	bool CommandRequiresStorageDevice( const char *command );
	bool CommandRespectsSignInDenied( const char *command );

	void QueueCommand( const char *pCommand );
	void RunQueuedCommands();
	void ClearQueuedCommands();

	virtual void PerformLayout();
	MESSAGE_FUNC_INT( OnActivateModule, "ActivateModule", moduleIndex);

	void LoadVersionNumbers();
	bool LoadVersionNumber( const char *fileNameA, const char *fileNameB, wchar_t *pVersionBuffer, unsigned int versionBufferSizeBytes );

	// Primary user id: used to determine signin privileges, etc
	int	m_primaryUserId;

	// menu logo
	CMainMenuGameLogo *m_pGameLogo;
	
	// menu buttons
	CUtlVector< CBackgroundMenuButton * >m_pGameMenuButtons;
	CGameMenu *m_pGameMenu;
	bool m_bPlatformMenuInitialized;
	int m_iGameMenuInset;

	struct coord {
		int x;
		int y;
	};
	CUtlVector< coord > m_iGameTitlePos;
	coord m_iGameMenuPos;

	// base dialogs
	vgui::DHANDLE<vgui::Frame> m_hNewGameDialog;
	vgui::DHANDLE<vgui::Frame> m_hBonusMapsDialog;
	vgui::DHANDLE<vgui::Frame> m_hLoadGameDialog;
	vgui::DHANDLE<vgui::Frame> m_hLoadGameDialog_Xbox;
	vgui::DHANDLE<vgui::Frame> m_hSaveGameDialog;
	vgui::DHANDLE<vgui::Frame> m_hSaveGameDialog_Xbox;
	vgui::DHANDLE<vgui::PropertyDialog> m_hOptionsDialog;
	vgui::DHANDLE<vgui::Frame> m_hCreateMultiplayerGameDialog;
	//vgui::DHANDLE<vgui::Frame> m_hDemoPlayerDialog;
	vgui::DHANDLE<vgui::Frame> m_hChangeGameDialog;
	vgui::DHANDLE<vgui::Frame> m_hPlayerListDialog;
	vgui::DHANDLE<vgui::Frame> m_hBenchmarkDialog;
	vgui::DHANDLE<vgui::Frame> m_hLoadCommentaryDialog;

	vgui::Label					*m_pCodeVersionLabel;
	vgui::Label					*m_pContentVersionLabel;

	EBackgroundState m_eBackgroundState;

	CMessageDialogHandler		m_MessageDialogHandler;
	CUtlVector< CUtlString >	m_CommandQueue;

	vgui::AnimationController	*m_pConsoleAnimationController;
	KeyValues					*m_pConsoleControlSettings;

	void						DrawBackgroundImage();
	int							m_iBackgroundImageID;
	int							m_iRenderTargetImageID;
	int							m_iLoadingImageID;
	int							m_iProductImageID;
	bool						m_bLevelLoading;
	bool						m_bEverActivated;
	bool						m_bCopyFrameBuffer;
	bool						m_bUseRenderTargetImage;
	int							m_ExitingFrameCount;
	bool						m_bXUIVisible;
	bool						m_bUseMatchmaking;
	bool						m_bRestartFromInvite;
	bool						m_bRestartSameGame;
	
	// Used for internal state dealing with blades
	bool						m_bUserRefusedSignIn;
	bool						m_bUserRefusedStorageDevice;
	bool						m_bWaitingForUserSignIn;
	bool						m_bStorageBladeShown;
	CUtlString					m_strPostPromptCommand;

	// Used on PS3 to make sure stats get loaded before the player presses start.
	bool						m_bStatsLoaded;

	// Storage device changing vars
	bool			m_bWaitingForStorageDeviceHandle;
	bool			m_bNeedStorageDeviceHandle;
	AsyncHandle_t	m_hStorageDeviceChangeHandle;
	uint			m_iStorageID;
	int				*m_pStorageDeviceValidatedNotify;

	// background transition
	bool m_bFadingInMenus;
	float m_flFadeMenuStartTime;
	float m_flFadeMenuEndTime;

	bool m_bRenderingBackgroundTransition;
	float m_flTransitionStartTime;
	float m_flTransitionEndTime;

	// Used for rich presence updates on xbox360
	bool m_bSinglePlayer;
	uint m_iGameID;	// matches context value in hl2orange.spa.h

	// background fill transition
	bool m_bHaveDarkenedBackground;
	bool m_bHaveDarkenedTitleText;
	bool m_bForceTitleTextUpdate;
	float m_flFrameFadeInTime;
	Color m_BackdropColor;
	CPanelAnimationVar( float, m_flBackgroundFillAlpha, "m_flBackgroundFillAlpha", "0" );

	// [jason] For platforms that don't require the start screen to set the default controller
	bool m_bBypassStartScreen;

	// [jason] When flagged, we initially display "Press Start" screen and wait for controller input
	bool m_bShowStartScreen;
	
	// [jason] Have we been notified (via CreateStartScreen) that the signin completed successfully?
	bool m_bStartScreenPlayerSigninCompleted;

	// [jason] Should we use the Scaleform main menu, or the old vgui one?
	bool m_bScaleformMainMenuEnabled;
	bool m_bScaleformPauseMenuEnabled;

	// [jason] Last value that ShowMainMenu was called with: is the old vgui active or not?
	bool m_bMainMenuShown;


protected:
    int m_iIntroMovieButtonPressed;
    bool m_bIntroMovieWaitForButtonToClear;

	// [sb] If true, then force us back to the start screen; useful for when primary player signs out
	bool m_bForceStartScreen;

public:
	void SetForceStartScreen() { m_bForceStartScreen = true; };

protected:

	// fading to game
	MESSAGE_FUNC_CHARPTR( RunEngineCommand, "RunEngineCommand", command );
	MESSAGE_FUNC_CHARPTR( RunMenuCommand, "RunMenuCommand", command );
	MESSAGE_FUNC_INT_CHARPTR( RunSlottedMenuCommand, "RunSlottedMenuCommand", slot, command );
	MESSAGE_FUNC( FinishDialogClose, "FinishDialogClose" );
};

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
// 
// These must be defined in the mod's derived panel
//-----------------------------------------------------------------------------
extern CBaseModPanel *BasePanel();
extern CBaseModPanel *BasePanelSingleton(); // Constructs if not built yet

#endif

#endif // BASEPANEL_H
