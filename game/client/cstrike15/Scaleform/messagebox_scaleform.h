#if defined( INCLUDE_SCALEFORM )
//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef MESSAGEBOX_SCALEFORM_H
#define MESSAGEBOX_SCALEFORM_H
#ifdef _WIN32
#pragma once
#endif

#include "matchmaking/imatchframework.h"
#include "scaleformui/scaleformui.h"
#include "GameUI/IGameUI.h"
#include "GameEventListener.h"

#define MAX_SCALEFORM_MESSAGE_BOX_LENGTH 1024

enum MessageBoxFlags_t
{
	MESSAGEBOX_FLAG_INVALID 					= 0x00,
	MESSAGEBOX_FLAG_OK 							= 0x01,
	MESSAGEBOX_FLAG_CANCEL 						= 0x02,
	MESSAGEBOX_FLAG_BOX_CLOSED 					= 0x04,
	MESSAGEBOX_FLAG_AUTO_CLOSE_ON_DISCONNECT	= 0x08,
	MESSAGEBOX_FLAG_TERTIARY 					= 0x10	// for third options, like "Press Y for Default"
};

class CMessageBoxScaleform;
void ClearMessageBoxCallback( CMessageBoxScaleform* pMsgBox );

abstract_class IMessageBoxEventCallback
{
	friend class CMessageBoxScaleform;

public:
	IMessageBoxEventCallback() 
	{ 
		m_pMessageBoxReference = NULL; 
	}

	virtual ~IMessageBoxEventCallback() 
	{ 
		ClearMessageBoxCallback( m_pMessageBoxReference ); 
		m_pMessageBoxReference = NULL;
	}

	// Which button the user selected.  Callback should return true in order to dismiss the message box.
	virtual bool OnMessageBoxEvent( MessageBoxFlags_t buttonPressed ) = 0;
	
	virtual bool OnUpdate( void ) { return false; }
	virtual void NotifyOnReady( void ) { }

	// Override this as true for any message that you want to persist when you go in/out of levels or the front-end (eg. error codes, game-modal dialogs)
	virtual bool IsPriorityMessage() { return false; }

protected:
	// Allow all callbacks to keep a reference to their owner message box
	CMessageBoxScaleform	*m_pMessageBoxReference;
};

class CMessageBoxScaleform : public ScaleformFlashInterfaceMixin<CGameEventListener>
{
protected:
	static CUtlVector<CMessageBoxScaleform*> m_sMessageBoxes;

	CMessageBoxScaleform( char const *pszTitle, char const *pszMessage, char const *pszButtonLegend, DWORD dwFlags, IMessageBoxEventCallback *pEventCallback = NULL, wchar_t const *pszWideMessage = NULL );

	virtual ~CMessageBoxScaleform();

public:
	static CMessageBoxScaleform * GetLastMessageBoxCreated();
	static void LoadDialog( char const *pszTitle, char const *pszMessage, char const *pszButtonLegend, DWORD dwFlags, IMessageBoxEventCallback *pEventCallback = NULL, CMessageBoxScaleform** ppInstance = NULL, wchar_t const *pszWideMessage = NULL );
	static void LoadDialogInSlot( int slot, char const *pszTitle, char const *pszMessage, char const *pszButtonLegend, DWORD dwFlags, IMessageBoxEventCallback *pEventCallback = NULL, CMessageBoxScaleform** ppInstance = NULL, wchar_t const *pszWideMessage = NULL );
	
	// Creates the message box with three options: OK, Cancel, and "your button legend" 
	static void LoadDialogThreeway( char const *pszTitle, char const *pszMessage, char const *pszButtonLegend, char const *pszTertiaryButtonLabel, DWORD dwFlags, IMessageBoxEventCallback *pEventCallback = NULL, CMessageBoxScaleform** ppInstance = NULL );

	// When the bClosePriorityMsgBoxes is set, we will also close every message box that overrides IsPriorityMessageBox=true (CCommandMsgBox for example)
	static void UnloadAllDialogs( bool bClosePriorityMsgBoxes = true );

	// Are there any important messages open we want to leave active?
	static bool IsPriorityMessageOpen();

	bool IsReady() { return m_bIsReady; }
	bool IsPriorityMessage();

	void SetTitle( char const *pszTitle );
	void SetMessage( char const *pszMessage );
	void SetTitle( wchar_t const * pwcTitle );
	void SetMessage( wchar_t const * pszMessage );
	void SetButtonLegend( char const *pszButtonLegend );
	void SetFlags( DWORD dwFlags );

	void SetOKButtonLabel( char const *pszOKButtonLabel );

	void SetThirdButtonLabel( char const *pszThirdButtonLabel );	

	void OnButtonPress( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnMessageBoxClosed( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnTimerCallback( SCALEFORM_CALLBACK_ARGS_DECL );

	void Show();
	void Hide();
	void HideImmediate();

	virtual void FireGameEvent( IGameEvent *event );
	void ClearCallback() { m_pEventCallback = NULL; }

protected:
	bool m_bIsReady;

	char m_szTitle[MAX_SCALEFORM_MESSAGE_BOX_LENGTH];
	char m_szMessage[MAX_SCALEFORM_MESSAGE_BOX_LENGTH];
	char m_szButtonLegend[2048];
	char m_szThirdButtonLabel[MAX_SCALEFORM_MESSAGE_BOX_LENGTH];
	char m_szOKButtonLabel[MAX_SCALEFORM_MESSAGE_BOX_LENGTH];

	wchar_t m_szWideMessage[MAX_SCALEFORM_MESSAGE_BOX_LENGTH];

	DWORD m_dwFlags;		// See MessageBoxFlags_t

	IMessageBoxEventCallback *m_pEventCallback;
	
	virtual void FlashReady();
	virtual void PostUnloadFlash();
	virtual void FlashLoaded();
};


// hosts a message box that can execute con commands based on the user's response
// commands are strings that will be passed to engine->ClientCommand_Unrestricted.  If the command begins with '!', the code will call engine->ClientCommand instead.
// this is a 

class CCommandMsgBox : public IMessageBoxEventCallback
{
public:
	static void CreateAndShow( const char* pszTitle, const char* pszMessage, bool showOk = true, bool showCancel = false, const char* okCommand = NULL, const char* cancelCommand = NULL, const char* closedCommand = NULL, const char* pszLegend = NULL );
	static void CreateAndShowInSlot( ECommandMsgBoxSlot slot, const char* pszTitle, const char* pszMessage, bool showOk = true, bool showCancel = false, const char* okCommand = NULL, const char* cancelCommand = NULL, const char* closedCommand = NULL, const char* pszLegend = NULL );

	// Command messages are typically game-critical: they are used for trial mode messages, as well as explanation for being kicked from a server or failure to load a map
	virtual bool IsPriorityMessage() { return true; }

protected:
	CCommandMsgBox( ECommandMsgBoxSlot slot, const char* pszTitle, const char* pszMessage, bool showOk = true, bool showCancel = false, const char* okCommand = NULL, const char* cancelCommand = NULL, const char* closedCommand = NULL, const char* pszLegend = NULL );
	~CCommandMsgBox();
	char* m_pCommands[3];
	int m_iExitCommand;
	CMessageBoxScaleform* m_pMessageBox;

	void SetCommand( int index, const char* command );
	void ExecuteCommand( int index );

	virtual bool OnMessageBoxEvent( MessageBoxFlags_t buttonProssed );
};

// hosts the matchmaking message box

class CMatchmakingStatus : public IMatchEventsSink, public IMessageBoxEventCallback
{
public:
	CMatchmakingStatus();
	CMatchmakingStatus( char const *szCustomTitle, char const *szCustomText );
	~CMatchmakingStatus();

	void SetTimeToAutoCancel( double dblPlatFloatTime );

protected:
	virtual void OnEvent( KeyValues *pEvent );
	
	// IMessageBoxEventsCallback implementation
	virtual bool OnMessageBoxEvent( MessageBoxFlags_t buttonPressed );

	CMessageBoxScaleform *m_pMessageBoxInstance;
	bool	m_bErrorEncountered;

	double	m_dblTimeToAutoCancel;
};

// hosts the store message box

class CStoreStatusScaleform : public IMessageBoxEventCallback
{
public:
	static void HideInstance();
	explicit CStoreStatusScaleform( const char *szText, bool bAllowClose, bool bCancel, const char *szCommandOk = NULL );
	~CStoreStatusScaleform();

protected:
	// IMessageBoxEventsCallback implementation
	virtual bool OnMessageBoxEvent( MessageBoxFlags_t buttonPressed );

	CMessageBoxScaleform *m_pMessageBoxInstance;
	static CStoreStatusScaleform *s_pStoreStatusBox;

	const char *m_pszCommandOk;
};



// since first frame key-input has crossover with the button press to open the menu
// we have a state machine to force us to skip first frame input
enum CMessageBoxLockInputState
{
	MESSAGE_BOX_LOCK_STATE_INIT,
	MESSAGE_BOX_LOCK_STATE_SCANNING,
	MESSAGE_BOX_LOCK_STATE_FINISHED,
};

// hosts the lock input message box
class CMessageBoxLockInput : public IMatchEventsSink, public IMessageBoxEventCallback
{
public:
	CMessageBoxLockInput( void );
	~CMessageBoxLockInput( void );

	virtual bool IsPriorityMessage() { return true; }

protected:
	virtual void OnEvent( KeyValues *pEvent );
	virtual bool OnUpdate( void );
	virtual void NotifyOnReady( void );
	virtual bool OnMessageBoxEvent( MessageBoxFlags_t buttonPressed );

	CMessageBoxScaleform *m_pMessageBoxInstance;
	CMessageBoxLockInputState m_lockState;
};


class CMessageBoxCalibrateNotification : public IMatchEventsSink, public IMessageBoxEventCallback
{
public:
	CMessageBoxCalibrateNotification( void );
	~CMessageBoxCalibrateNotification( void );

	virtual bool IsPriorityMessage() { return true; }

protected:
	virtual void OnEvent( KeyValues *pEvent );
	virtual bool OnUpdate( void );
	virtual void NotifyOnReady( void );
	virtual bool OnMessageBoxEvent( MessageBoxFlags_t buttonPressed );

	CMessageBoxScaleform *m_pMessageBoxInstance;
};


// [dkorus] used to differenciate popups, so we can ensure we're enabling and disabling the same popup type
//          while managing them in a one-at-a-time method
enum ManagedPopupType
{
	POPUP_TYPE_NONE = 0,
	POPUP_TYPE_HIDING,
	POPUP_TYPE_PSEYE_DISCONNECTED,
	POPUP_TYPE_PSMOVE_OUT_OF_VIEW,
};

// Used to manage the numerous popups we need for TRC and TCR compliance.
class PopupManager
{
public:
	static void Update( void );
 
	static void UpdateTryHideSingleUsePopup( void );
	static bool ShowSingleUsePopup( ManagedPopupType popupType );
	static bool HideSingleUsePopup( ManagedPopupType popupType );

private:
	
	static ManagedPopupType s_singleUsePopupType;
};

#endif // MESSAGEBOX_SCALEFORM_H
#endif // INCLUDE_SCALEFORM
