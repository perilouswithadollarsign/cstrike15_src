//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VATTRACTSCREEN_H__
#define __VATTRACTSCREEN_H__

#include "basemodui.h"
#include "UIGameData.h"

namespace BaseModUI {

class CAttractScreen : public CBaseModFrame, public IMatchEventsSink, public IBaseModFrameListener
{
	DECLARE_CLASS_SIMPLE( CAttractScreen, CBaseModFrame );

public:
	enum AttractMode_t
	{
		ATTRACT_GAMESTART,			// When the game initially boots, try not to bug user too much
		ATTRACT_GOSPLITSCREEN,		// When enabled from the main menu
		ATTRACT_GOSINGLESCREEN,		// Splitscreen disabled from the main menu
		ATTRACT_GUESTSIGNIN,		// When guest wants to sign in
		ATTRACT_ACCEPTINVITE,		// Accepting invite
	};
	static void SetAttractMode( AttractMode_t eMode, int iCtrlr = -1 );

	enum BladeStatus_t
	{
		BLADE_WAITINGFOROPEN,
		BLADE_WAITINGFORCLOSE,
		BLADE_WAITINGFORSIGNIN,
		BLADE_NOTWAITING,
		BLADE_TAKEDOWNCONFIRMATION,
		BLADE_WAITINGTOSHOWPROMOTEUI,
		BLADE_WAITINGTOSHOWSTORAGESELECTUI,
		BLADE_WAITINGTOSHOWSIGNIN2,
	};

	CAttractScreen( vgui::Panel *parent, const char *panelName );
	~CAttractScreen();

	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void OnOpen();
	virtual void OnClose();
	virtual void OnThink();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PaintBackground();
	virtual void RunFrame();

	MESSAGE_FUNC( OpenMainMenu, "OpenMainMenu" );
	MESSAGE_FUNC_CHARPTR( OpenMainMenuJoinFailed, "OpenMainMenuJoinFailed", msg );
	MESSAGE_FUNC( OnChangeGamersFromMainMenu, "ChangeGamers" );
	MESSAGE_FUNC( StartWaitingForBlade1, "StartWaitingForBlade1" );
	MESSAGE_FUNC( StartWaitingForBlade2, "StartWaitingForBlade2" );
	MESSAGE_FUNC_CHARPTR( DisplayGameBootProgress, "DisplayGameBootProgress", msg );

	void ReportDeviceFail( ISelectStorageDeviceClient::FailReason_t eReason );

	BladeStatus_t GetBladeStatus();
	void HidePressStart();
	void ShowPressStart();
	void AcceptInvite();
	bool BypassAttractScreen();
	
	void HideProgress();
	void HideMsgs();

	virtual void OnEvent( KeyValues *pEvent );

	void OnMsgResetCancelFn();

	void StartGameWithTemporaryProfile_Stage1();
	void StartGameWithTemporaryProfile_Real();

	void StartGame( int idxUser1 = -1, int idxUser2 = -1 );
	void StartGame_Stage1_Storage1();
	void StartGame_Stage2_Storage2();
	void StartGame_Stage3_Ready();
	void StartGame_Real( int idxUser1 = -1, int idxUser2 = -1 );

	bool OfferPromoteToLiveGuest();

	bool IsUserIdleForAttractMode();
	bool IsGameBootReady() { return m_bGameBootReady; }

private:
	vgui::Label			*m_pPressStartlbl;
	float				m_flFadeStart;
	bool				m_bHidePressStart;
	bool				m_bGameBootReady;
	
	enum BladeSignInUI_t
	{
		SIGNIN_NONE,
		SIGNIN_SINGLE,
		SIGNIN_DOUBLE,
		SIGNIN_PROMOTETOGUEST,
	};
	BladeSignInUI_t	m_eSignInUI;

	enum StorageSelection_t
	{
		STORAGE_NONE,
		STORAGE_0,
		STORAGE_1,
	};
	StorageSelection_t m_eStorageUI;
	
	float			m_flWaitStart;
	int				m_msgData;
	void			( *m_pfnMsgChanged )();
	BladeStatus_t	m_bladeStatus;

private:
	void SetBladeStatus( BladeStatus_t bladeStatus );
	void ShowSignInDialog( int iPrimaryUser, int iSecondaryUser, BladeSignInUI_t eForceSignin = SIGNIN_NONE );
	void ShowStorageDeviceSelectUI();
	void PerformGameBootWork();
	void ShowFatalError( uint32 unSize );

#ifndef NO_STEAM
	STEAM_CALLBACK_MANUAL( CAttractScreen, Steam_OnUserStatsReceived, UserStatsReceived_t, m_CallbackOnUserStatsReceived );	
#ifdef _PS3
	STEAM_CALLBACK_MANUAL( CAttractScreen, Steam_OnPSNGameBootInviteResult, PSNGameBootInviteResult_t, m_CallbackOnPSNGameBootInviteResult );
	STEAM_CALLBACK_MANUAL( CAttractScreen, Steam_OnPS3TrophiesInstalled, PS3TrophiesInstalled_t, m_CallbackOnPS3TrophiesInstalled );
#endif
#endif
#ifdef _PS3
	void OnGameBootSaveContainerReady();
#endif
};

};

#endif
