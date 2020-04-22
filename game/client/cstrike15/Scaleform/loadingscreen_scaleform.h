//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#if defined( INCLUDE_SCALEFORM )

#if !defined( __LOADINGSCREEN_SCALEFORM_H__ )
#define __LOADINGSCREEN_SCALEFORM_H__

#include "scaleformui/scaleformui.h"
#include "matchmaking/imatchframework.h"
#include "uigamedata.h"
#include "gameui_interface.h"
#include "gameeventlistener.h"

class CLoadingScreenScaleform : public ScaleformFlashInterface, public IMatchEventsSink, public CGameEventListener
{
public:
	static CLoadingScreenScaleform* m_pInstance;
	
	CLoadingScreenScaleform();
	virtual ~CLoadingScreenScaleform();

	//These mirror the VGUI LoadingDialog interface
	static bool SetProgressPoint( float fraction, bool showDialog = true );
	static bool SetSecondaryProgressPoint( float fraction );
	static void DisplayVACBannedError( void );
	static void DisplayNoSteamConnectionError( void );
	static void DisplayLoggedInElsewhereError( void );
	static bool LoadingProgressWantsIsolatedRender( bool bContextValid );

	static void FinishLoading( void );
	static void CloseLoadingScreen( void );

	//These mirror the VGUI LoadingDialog interface and are unused
	static void Activate( void ){}
	static void Open( void ){}		
	static void SetStatusText( const char *statusText, bool showDialog = true );
	static void SetStatusText( const wchar_t *desc );
	static void SetSecondaryStatusText( const wchar_t *desc );
	static void SetSecondaryProgress( float progress ){ SetSecondaryProgressPoint( progress ); }
	static void SetSecondaryProgressText( const wchar_t *desc ){ SetSecondaryStatusText( desc ); }
	static bool SetShowProgressText( bool show ){ return true; }	

	static void LoadDialog( void );
	static void LoadDialogForCommand( const char* command );
	static void LoadDialogForKeyValues( KeyValues* keyValues );
	static void UnloadDialog( void );
	static bool IsOpen( void );

	void Show( void );
	void SetPendingCommand( const char* command );
	void SetProgressInternal( float fraction );
	void SetStatusTextInternal( const char *statusText );
	void SetStatusTextInternal( const wchar_t *desc );
	void SetSecondaryProgressInternal( float fraction );
	void SetSecondaryStatusTextInternal( const wchar_t *desc );
	void CloseScreenUpdateScaleform( void );

	void PlayAnimation();
	void PlayUnblurAnimation();
	


	/************************************
	 * callbacks from scaleform
	 */

	void ReadyForLoading( SCALEFORM_CALLBACK_ARGS_DECL );
	void AnimComplete( SCALEFORM_CALLBACK_ARGS_DECL );
	void SWFLoadError( SCALEFORM_CALLBACK_ARGS_DECL );
	void SWFLoadSuccess( SCALEFORM_CALLBACK_ARGS_DECL );
	void ContinueButtonPressed( SCALEFORM_CALLBACK_ARGS_DECL );
	void CloseAndUnload( SCALEFORM_CALLBACK_ARGS_DECL );

	/************************************************************
	 *  Flash Interface methods
	 */

	virtual void FlashReady( void );	

	bool PreUnloadFlash( void );
	void PostUnloadFlash( void );
	void SetPendingKeyValues( KeyValues* keyValues );

	/************************************************************
	 *  IMatchEventsSink methods
	 */
	void OnEvent( KeyValues *pEvent );

	/********************************************
	* CGameEventListener methods
	*/
	virtual void FireGameEvent( IGameEvent *event );


protected:
	void SetLoadingScreenElementsData( const char* mapName );
	void PopulateLevelInfo( const char* mapName, const char* szGameType, const char* szGameMode );
	void PopulateLevelInfo( const char* mapName, const char* gameTypeNameID, const char* gameModeNameID, int iGameType, int iGameMode );
	void PopulateHintText( void );

	void ShowContinueButton( void );


	bool				m_serverInfoReady;	
	char				m_pendingCommand[1024];
	char				m_pendingAttributePurchaseActivate[256];
	int					m_nRequiredAttributeValueForPurchaseActivate;
	KeyValues	*		m_pPendingKeyValues;

	double				m_flLoadStartTime;

	int					m_nAnimFrameCurrent;
	int					m_nAnimFrameTarget;
	bool				m_bStartedUnblur;
	bool				m_bAnimPlaying;
	bool				m_bSWFLoadSuccess;
	float				m_flTimeLastHintUpdate;

	bool				m_readyForLoading;
	bool				m_bCreatedMapLoadingScreen;

	bool				m_bCheckedForSWFAndFailed;
};

extern CLoadingScreenScaleform g_loadingScreen;

#endif //__LOADINGSCREEN_SCALEFORM_H__

#endif //INCLUDE_SCALEFORM
