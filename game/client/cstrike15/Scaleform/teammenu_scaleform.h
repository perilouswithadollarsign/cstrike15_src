//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#if defined( INCLUDE_SCALEFORM )

#if !defined( __TEAMMENU_SCALEFORM_H__ )
#define __TEAMMENU_SCALEFORM_H__

#include "../VGUI/counterstrikeviewport.h"
#include "messagebox_scaleform.h"
#include "GameEventListener.h"

#define TEAM_MENU_MAX_PLAYERS 12

class CCSTeamMenuScaleform : public ScaleformFlashInterface, public IViewPortPanel, public CGameEventListener
{
public:
	explicit CCSTeamMenuScaleform( CounterStrikeViewport* pViewPort );
	virtual ~CCSTeamMenuScaleform( );

	/************************************
	 * callbacks from scaleform
	 */

	void OnOk( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnCancel( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnSpectate( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnAutoSelect( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnTimer( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnShowScoreboard( SCALEFORM_CALLBACK_ARGS_DECL );
	void UpdateNavText( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnTeamHighlight( SCALEFORM_CALLBACK_ARGS_DECL );
	void IsInitialTeamMenu( SCALEFORM_CALLBACK_ARGS_DECL );
	void IsQueuedMatchmaking( SCALEFORM_CALLBACK_ARGS_DECL );

	/****************************************
	 * functionality
	 */

	void RefreshCounts( void );
	void UpdateSpectatorOption( void );
	void SetTeamNames( void );		// Use clan names or teamname_1 or 2 convars
	void UpdateHelpText();
	void HandleForceSelect( void );		// Updates the force select timer and pushes a player to a team when it expires
	void UpdateTeamAvatars( void );		// Pushes any changes to team-member avatars to Scaleform

	/************************************************************
	 *  Flash Interface methods
	 */

	virtual void FlashReady( void );
	virtual void FlashLoaded( void );

	void Show( void );

	// if bRemove, then remove all elements after hide animation completes
	void Hide( bool bRemove = false );

	bool PreUnloadFlash( void );

	/*************************************************************
	 * IViewPortPanel interface
	 */

	virtual const char *GetName( void ) { return PANEL_TEAM; }
	virtual void SetData( KeyValues *data ) {};

	virtual void Reset( void ) {}  // hibernate
	virtual void Update( void ) {}	// updates all ( size, position, content, etc )
	virtual bool NeedsUpdate( void ) { return false; } // query panel if content needs to be updated
	virtual bool HasInputElements( void ) { return true; }
	virtual void ReloadScheme( void ) {}
	virtual bool CanReplace( const char *panelName ) const { return true; } // returns true if this panel can appear on top of the given panel
	virtual bool CanBeReopened( void ) const { return true; } // returns true if this panel can be re-opened after being hidden by another panel
	virtual void ViewportThink( void );

	virtual void ShowPanel( bool state );

	// VGUI functions:
	virtual vgui::VPANEL GetVPanel( void ) { return 0; } // returns VGUI panel handle
	virtual bool IsVisible( void ) { return m_bVisible; }  // true if panel is visible
	virtual void SetParent( vgui::VPANEL parent ) {}

	virtual bool WantsBackgroundBlurred( void ) { return false; }

	/********************************************
	* CGameEventListener methods
	*/

	virtual void FireGameEvent( IGameEvent *event );

protected:
	void HandlePostTeamSelect( int team );	// Display post select overlay and wait for pre-match restart stutters to pass
	void ResetForceSelect( void );		// Reset state's associated with forced team selection

	void StartListeningForEvents( void );
	void StopListeningForEvents( void );
	void StartAlwaysListenEvents( void );

	void SetPlayerXuid( bool bIsCT, int index, XUID xuid, const char* pPlayerName, bool bIsLocalPlayer );

protected:
	enum MessageBoxClosedAction
	{
		NOTHING,
		DISCONNECT,
		HIDEPANEL,
	};

	CounterStrikeViewport* m_pViewPort;
	ISFTextObject* m_pCTCountHuman;
	ISFTextObject* m_pCTCountBot;
	ISFTextObject* m_pTCountHuman;
	ISFTextObject* m_pTCountBot;
	ISFTextObject* m_pCTHelpText;
	ISFTextObject* m_pTHelpText;

	ISFTextObject* m_pNavText;
	ISFTextObject* m_pTName;
	ISFTextObject* m_pCTName;
	ISFTextObject* m_pTimerTextLabel;
	ISFTextObject* m_pTimerTextGreen;
	ISFTextObject* m_pTimerTextRed;
	SFVALUE m_pTimerHandle;
	MessageBoxClosedAction m_OnClosedAction;
	int m_iSplitScreenSlot;
	int m_nCTHumanCount;
	int m_nTHumanCount;
	int	 m_nForceSelectTimeLast;	// The value of the timer at the last update
	bool m_bVisible;
	bool m_bLoading;
	bool m_bAllowSpectate;
	bool m_bPostSelectOverlay;		// True when we are waiting for pre-match restart stuttuers to pass
	bool m_bGreenTimerVisible;		// Green timer is shown
	bool m_bRedTimerVisible;		// Red timer is shown
	bool m_bMatchStart;
	bool m_bSelectingTeam;

	void JoinTeam( int side );

	enum PLAYER_TEAM_COUNT
	{
		MAX_TEAM_SIZE = 12
	};

	// Save off the Xuids we've pushed to Scaleform, so we only update when these have changed
	XUID	m_CT_Xuids[MAX_TEAM_SIZE];
	XUID	m_T_Xuids[MAX_TEAM_SIZE];
	int		m_nCTLocalPlayers[MAX_TEAM_SIZE];
	int		m_nTLocalPlayers[MAX_TEAM_SIZE];
	char	m_chCTNames[MAX_TEAM_SIZE][MAX_PLAYER_NAME_LENGTH];
	char	m_chTNames[MAX_TEAM_SIZE][MAX_PLAYER_NAME_LENGTH];
};

#endif

#endif
