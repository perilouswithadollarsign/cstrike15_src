//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//
#if defined( USE_SDL )
#undef PROTECTED_THINGS_ENABLE
#include "SDL.h"
#endif

#if defined( WIN32 ) && !defined( _X360 ) && !defined( DX_TO_GL_ABSTRACTION )
#include "winlite.h"
#include "xbox/xboxstubs.h"
#endif

#if defined( IS_WINDOWS_PC ) && !defined( USE_SDL )
#include <winsock.h>
#elif defined(_X360)
// nothing to include for 360
#elif defined(OSX)
#include <Carbon/Carbon.h>
#elif defined(LINUX)
	#include "tier0/dynfunction.h"
#elif defined(_WIN32)
	#include "tier0/dynfunction.h"
#elif defined( _PS3 )
#include "basetypes.h"
#include "ps3/ps3_core.h"
#include "ps3/ps3_win32stubs.h"
#include <cell/audio.h>
#include <sysutil/sysutil_sysparam.h>
#else
#error
#endif
#include "appframework/ilaunchermgr.h"

#include "igame.h"
#include "cl_main.h"
#include "host.h"
#include "quakedef.h"
#include "tier0/icommandline.h"
#include "ivideomode.h"
#include "gl_matsysiface.h"
#include "cdll_engine_int.h"
#include "vgui_baseui_interface.h"
#include "iengine.h"
#include "avi/iavi.h"
#include "keys.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "tier3/tier3.h"
#include "sound.h"
#include "vgui_controls/Controls.h"
#include "vgui_controls/MessageDialog.h"
#include "sys_dll.h"
#include "inputsystem/iinputsystem.h"
#include "inputsystem/ButtonCode.h"
#include "GameUI/IGameUI.h"
#include "sv_main.h"
#if defined( BINK_VIDEO )
#include "bink/bink.h"
#endif
#include "vgui/IVGui.h"
#include "IHammer.h"
#include "inputsystem/iinputstacksystem.h"
#include "avi/ibik.h"
#include "materialsystem/imaterial.h"
#include "characterset.h"
#include "server.h"

#if defined( INCLUDE_SCALEFORM )
#include "scaleformui/scaleformui.h"
#endif

#include <vgui/ILocalize.h>
#include <vgui/ISystem.h>

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#include "snd_dev_xaudio.h"
#include "xmp.h"
#include "xbox/xbox_launch.h"
#include "ixboxsystem.h"
extern IXboxSystem *g_pXboxSystem;
#endif

#if defined( LINUX )
#include "snd_dev_sdl.h"
#endif

#include "matchmaking/imatchframework.h"
#include "tier2/tier2.h"

#include "tier1/fmtstr.h"

#if !defined( PLATFORM_X360 )
#include "cl_steamauth.h"
#endif

#if defined( PLATFORM_WINDOWS )
#include "vaudio/ivaudio.h"
extern void VAudioInit();
extern IVAudio * vaudio;
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar cv_vguipanel_active;

void S_BlockSound (void);
void S_UnblockSound (void);
void ClearIOStates( void );


//-----------------------------------------------------------------------------
// Game input events
//-----------------------------------------------------------------------------
enum GameInputEventType_t
{
	IE_WindowMove = IE_FirstAppEvent,
	IE_AppActivated,
};

//-----------------------------------------------------------------------------
// Purpose: Main game interface, including message pump and window creation
//-----------------------------------------------------------------------------
class CGame : public IGame
{
public:
					CGame( void );
	virtual			~CGame( void );

	bool			Init( void *pvInstance );
	bool			Shutdown( void );

	bool			CreateGameWindow( void );
	void			DestroyGameWindow();
	void			SetGameWindow( void* hWnd );

	// This is used in edit mode to override the default wnd proc associated w/
	bool			InputAttachToGameWindow();
	void			InputDetachFromGameWindow();

	void			PlayStartupVideos( void );

	void*			GetMainWindow( void );
	void**			GetMainWindowAddress( void );

	void			GetDesktopInfo( int &width, int &height, int &refreshrate );


	void			SetWindowXY( int x, int y );
	void			SetWindowSize( int w, int h );
	void			GetWindowRect( int *x, int *y, int *w, int *h );

	bool			IsActiveApp( void );

	void			SetCanPostActivateEvents( bool bEnable );
	bool			CanPostActivateEvents();

	virtual void    OnScreenSizeChanged( int nOldWidth, int nOldHeight );

public:
	void			SetMainWindow( HWND window );
	void			SetActiveApp( bool active );
#if defined( WIN32 ) || defined( _GAMECONSOLE )
	int				WindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
#endif
	// plays a video file and waits until completed. Can be interrupted by user input.
	virtual void	PlayVideoListAndWait( const char *szVideoFileList, bool bNeedHealthWarning = false );
	virtual void	PlayVideoAndWait(const char *filename, bool bNeedHealthWarning = false);

// Message handlers.
public:
	void	HandleMsg_WindowMove( const InputEvent_t &event );
	void	HandleMsg_ActivateApp( const InputEvent_t &event );
	void	HandleMsg_Close( const InputEvent_t &event );

	// Call the appropriate HandleMsg_ function.
	void	DispatchInputEvent( const InputEvent_t &event );

	// Dispatch all the queued up messages.
	virtual void	DispatchAllStoredGameMessages();

	InputContextHandle_t GetInputContext() { return m_hInputContext; }

private:
	void			AppActivate( bool fActive );

private:
	void AttachToWindow();
	void DetachFromWindow();

#ifndef _X360
	static const wchar_t CLASSNAME[];
#else
	static const char CLASSNAME[];
#endif

	bool			m_bExternallySuppliedWindow;

#if USE_SDL
	SDL_Window		*m_hWindow;
#elif defined( WIN32 ) 
	HWND			m_hWindow;
	HINSTANCE		m_hInstance;

	// Stores a wndproc to chain message calls to
	WNDPROC			m_ChainedWindowProc;

	RECT			m_rcLastRestoredClientRect;
#elif OSX
	WindowRef		m_hWindow;
#else
#error
#endif

	int				m_x;
	int				m_y;
	int				m_width;
	int				m_height;
	bool			m_bActiveApp;
	bool			m_bCanPostActivateEvents;

	int				m_iDesktopWidth, m_iDesktopHeight, m_iDesktopRefreshRate;
	void			UpdateDesktopInformation( HWND hWnd );
#ifdef WIN32
	void			UpdateDesktopInformation( WPARAM wParam, LPARAM lParam );
#endif
	InputContextHandle_t m_hInputContext;
};

static CGame g_Game;
IGame *game = ( IGame * )&g_Game;

#if defined( _PS3 )
extern void AbortLoadingUpdatesDueToShutdown();
extern bool SaveUtilV2_CanShutdown();
void PS3_sysutil_callback_forwarder( uint64 uiStatus, uint64 uiParam )
{
	if ( Steam3Client().SteamUtils() )
		Steam3Client().SteamUtils()->PostPS3SysutilCallback( uiStatus, uiParam, NULL );
	
	if ( uiStatus == CELL_SYSUTIL_REQUEST_EXITGAME )
	{
		SaveUtilV2_CanShutdown();
		AbortLoadingUpdatesDueToShutdown();
	}
	
}
int PS3_WindowProc_Proxy( xevent_t const &ev )
{
	// HWND = NULL
	// message = WM_*** (arg1)
	// LPARAM = parameter (arg2)
	// WPARAM = 0 (arg3)
	// Note the order of parameters to WindowProc:
	//		WindowProc( HWND, MSG, WPARAM=arg3=0, LPARAM )
	if ( ev.arg3 )
	{
		// Event has sysutil payload
		PS3_sysutil_callback_forwarder( ev.sysutil_status, ev.sysutil_param );
		if ( 0 && g_pMatchFramework )
		{
			KeyValues *kv = new KeyValues( "Ps3SysutilCallback" );
			kv->SetUint64( "status", ev.sysutil_status );
			kv->SetUint64( "param", ev.sysutil_param );
			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( kv );
		}
	}
	return g_Game.WindowProc( NULL, ev.arg1, ev.arg3, ev.arg2 );
}
#endif

#if !defined( _X360 )
const wchar_t CGame::CLASSNAME[] = L"Valve001";
#else
const char CGame::CLASSNAME[] = "Valve001";
#endif

// In VCR playback mode, it sleeps this amount each frame.
int g_iVCRPlaybackSleepInterval = 0;

// During VCR playback, if this is true, then it'll pause at the end of each frame.
bool g_bVCRSingleStep = false;

bool g_bWaitingForStepKeyUp = false;	// Used to prevent it from running frames while you hold the S key down.

bool g_bShowVCRPlaybackDisplay = true;

InputContextHandle_t GetGameInputContext()
{
	return g_Game.GetInputContext();
}

// These are all the windows messages that can change game state.
// See CGame::WindowProc for a description of how they work.
struct GameMessageHandler_t
{
	int	m_nEventType;
	void (CGame::*pFn)( const InputEvent_t &event );
};

GameMessageHandler_t g_GameMessageHandlers[] = 
{
	{ IE_AppActivated,			&CGame::HandleMsg_ActivateApp },
	{ IE_WindowMove,			&CGame::HandleMsg_WindowMove },
	{ IE_Close,					&CGame::HandleMsg_Close },
	{ IE_Quit,					&CGame::HandleMsg_Close },
};


void CGame::AppActivate( bool fActive )
{
	// If text mode, force it to be active.
	if ( g_bTextMode )
	{
		fActive = true;
	}

	// Don't bother if we're already in the correct state
	if ( IsActiveApp() == fActive )
		return;

	// Don't let video modes changes queue up another activate event
	SetCanPostActivateEvents( false );

#ifndef DEDICATED
	if ( videomode )
	{
		if ( fActive )
		{
			videomode->RestoreVideo();
		}
		else
		{
			videomode->ReleaseVideo();
		}
	}
#ifdef OSX
	// make sure the mouse cursor is in a sane location, force it to screen middle
	if ( fActive )
	{
		g_pLauncherMgr->SetCursorPosition( m_width/2, m_height/2 );
	}
#endif

	if ( host_initialized )
	{
		if ( fActive )
		{
			// Clear keyboard states (should be cleared already but...)
			// VGui_ActivateMouse will reactivate the mouse soon.
			ClearIOStates();
			
			UpdateMaterialSystemConfig();
		}
		else
		{
			// Clear keyboard input and deactivate the mouse while we're away.
			ClearIOStates();

			if ( g_ClientDLL )
			{
				g_ClientDLL->IN_DeactivateMouse();
			}
		}
	}
#endif // DEDICATED
	SetActiveApp( fActive );

	// Allow queueing of activation events
	SetCanPostActivateEvents( true );
}

void CGame::HandleMsg_WindowMove( const InputEvent_t &event )
{
	game->SetWindowXY( event.m_nData, event.m_nData2 );
#ifndef DEDICATED
	videomode->UpdateWindowPosition();
#endif
}

void CGame::HandleMsg_ActivateApp( const InputEvent_t &event )
{
	AppActivate( event.m_nData ? true : false );
}

void CGame::HandleMsg_Close( const InputEvent_t &event )
{
	if ( eng->GetState() == IEngine::DLL_ACTIVE )
	{
		eng->SetQuitting( IEngine::QUIT_TODESKTOP );
	}
}

void CGame::DispatchInputEvent( const InputEvent_t &event )
{
	switch( event.m_nType )
	{
	// Handle button events specially, 
	// since we have all manner of crazy filtering going on	when dealing with them
	case IE_ButtonPressed:
	case IE_ButtonDoubleClicked:
	case IE_ButtonReleased:
	case IE_KeyTyped:
	case IE_KeyCodeTyped:
		Key_Event( event );
		break;

	// Broadcast analog values both to VGui & to GameUI
	case IE_AnalogValueChanged:
		{
			// mouse events should go to vgui first, but joystick events should go to scaleform first

			if ( event.m_nData >= JOYSTICK_FIRST_AXIS )
			{
				if ( g_pScaleformUI && g_pScaleformUI->HandleInputEvent( event ) )
					break;

				if ( g_pMatSystemSurface && g_pMatSystemSurface->HandleInputEvent( event ) )
					break;
			}
			else
			{
				if ( g_pMatSystemSurface && g_pMatSystemSurface->HandleInputEvent( event ) )
					break;

#if defined( INCLUDE_SCALEFORM )
				bool vguiActive = IsPC() && cv_vguipanel_active.GetBool();			
			
				// we filter input while the console is visible, to prevent scaleform from
				//		handling anything underneath the console
				if ( !vguiActive && g_pScaleformUI && g_pScaleformUI->HandleInputEvent( event ) )
					break;
#endif // INCLUDE_SCALEFORM
			}

			// Let GameUI have the next whack at events
			if ( g_ClientDLL && g_ClientDLL->HandleGameUIEvent( event ) )
				break;
		}
		break;

	case IE_OverlayEvent:
		if ( event.m_nData == 1 )
		{
			// Overlay has activated
			if ( !EngineVGui()->IsGameUIVisible() && sv.IsActive() && sv.IsSinglePlayerGame() )
			{
				Cbuf_AddText( Cbuf_GetCurrentPlayer(), "gameui_activate" );
			}
		}
		break;

#ifdef _PS3
	case IE_ControllerUnplugged:
		WindowProc( 0, WM_SYS_INPUTDEVICESCHANGED, 0, ( 1 << event.m_nData ) );
		break;
	case IE_PS_CameraUnplugged:
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnPSEyeChangedStatus", "CamStatus", event.m_nData ) );
		break;
	case IE_PS_Move_OutOfView:
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnPSMoveOutOfViewChanged", "OutOfViewBool", event.m_nData ) );
		break;
#endif
	default:
		
		// Let vgui have the first whack at events
		if ( g_pMatSystemSurface && g_pMatSystemSurface->HandleInputEvent( event ) )
			break;

#if defined( INCLUDE_SCALEFORM )
		bool vguiActive = IsPC() && cv_vguipanel_active.GetBool();			

		// we filter all input while the console is visible, to prevent scaleform from
		//		handling anything underneath the console
		if ( !vguiActive && g_pScaleformUI && g_pScaleformUI->HandleInputEvent( event ) )
			break;
#endif // INCLUDE_SCALEFORM

		for ( int i=0; i < ARRAYSIZE( g_GameMessageHandlers ); i++ )
		{
			if ( g_GameMessageHandlers[i].m_nEventType == event.m_nType )
			{
				(this->*g_GameMessageHandlers[i].pFn)( event );
				break;
			}
		}
		break;
	}
}


void CGame::DispatchAllStoredGameMessages()
{
	int nEventCount = g_pInputSystem->GetEventCount();
	const InputEvent_t* pEvents = g_pInputSystem->GetEventData( );
	for ( int i = 0; i < nEventCount; ++i )
	{
		DispatchInputEvent( pEvents[i] );
	}
}

void VCR_EnterPausedState()
{
	// Turn this off in case they're in single-step mode.
	g_bVCRSingleStep = false;

#ifdef WIN32
	// This is cheesy, but GetAsyncKeyState is blocked (in protected_things.h) 
	// from being accidentally used, so we get it through it by getting its pointer directly.
	static HINSTANCE hInst = LoadLibrary( "user32.dll" );
	if ( !hInst )
		return;

	typedef SHORT (WINAPI *GetAsyncKeyStateFn)( int vKey );
	static GetAsyncKeyStateFn pfn = (GetAsyncKeyStateFn)GetProcAddress( hInst, "GetAsyncKeyState" );
	if ( !pfn )
		return;

	// In this mode, we enter a wait state where we only pay attention to R and Q.
	while ( 1 )
	{
		if ( pfn( 'R' ) & 0x8000 )
			break;

		if ( pfn( 'Q' ) & 0x8000 )
			TerminateProcess( GetCurrentProcess(), 1 );

		if ( pfn( 'S' ) & 0x8000 )
		{
			if ( !g_bWaitingForStepKeyUp )
			{
				// Do a single step.
				g_bVCRSingleStep = true;
				g_bWaitingForStepKeyUp = true;	// Don't do another single step until they release the S key.
				break;
			}
		}
		else
		{
			// Ok, they released the S key, so we'll process it next time the key goes down.
			g_bWaitingForStepKeyUp = false;
		}
	
		Sleep( 2 );
	}
#else
	Assert( !"Impl me" );
#endif
}

#ifdef WIN32
void VCR_HandlePlaybackMessages( 
	HWND hWnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam 
	)
{
	if ( uMsg == WM_KEYDOWN )
	{
		if ( wParam == VK_SUBTRACT || wParam == 0xbd )
		{
			g_iVCRPlaybackSleepInterval += 5;
		}
		else if ( wParam == VK_ADD || wParam == 0xbb )
		{
			g_iVCRPlaybackSleepInterval -= 5;
		}
		else if ( toupper( wParam ) == 'Q' )
		{
			TerminateProcess( GetCurrentProcess(), 1 );
		}
		else if ( toupper( wParam ) == 'P' )
		{
			VCR_EnterPausedState();
		}
		else if ( toupper( wParam ) == 'S' && !g_bVCRSingleStep )
		{
			g_bWaitingForStepKeyUp = true;
			VCR_EnterPausedState();
		}
		else if ( toupper( wParam ) == 'D' )
		{
			g_bShowVCRPlaybackDisplay = !g_bShowVCRPlaybackDisplay;
		}

		g_iVCRPlaybackSleepInterval = clamp( g_iVCRPlaybackSleepInterval, 0, 500 );
	}
}

//-----------------------------------------------------------------------------
// Calls the default window procedure
// FIXME: It would be nice to remove the need for this, which we can do
// if we can make unicode work when running inside hammer.
//-----------------------------------------------------------------------------
static LONG WINAPI CallDefaultWindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
#if defined( _GAMECONSOLE )
	return 0;
#else
	return DefWindowProcW( hWnd, uMsg, wParam, lParam );
#endif
}
#endif

//-----------------------------------------------------------------------------
// Purpose: The user has accepted an invitation to a game, we need to detect if 
//			it's our game and restart properly if it is
//-----------------------------------------------------------------------------
void XBX_HandleInvite( DWORD nUserId )
{
#ifdef _X360
	g_pMatchFramework->AcceptInvite( nUserId );
#endif //_X360
}

#if defined( WIN32 ) && !defined( USE_SDL )
//-----------------------------------------------------------------------------
// Main windows procedure
//-----------------------------------------------------------------------------
int CGame::WindowProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)

{
	LONG			lRet = 0;
	BOOL			bCallDefault = 0;
#if defined( WIN32 )
	HDC				hdc;
	PAINTSTRUCT		ps;

	//
	// NOTE: the way this function works is to handle all messages that just call through to
	// Windows or provide data to it.
	//
	// Any messages that change the engine's internal state (like key events) are stored in a list
	// and processed at the end of the frame. This is necessary for VCR mode to work correctly because
	// Windows likes to pump messages during some of its API calls like SetWindowPos, and unless we add
	// custom code around every Windows API call so VCR mode can trap the wndproc calls, VCR mode can't 
	// reproduce the calls to the wndproc.
	//

	if ( eng->GetQuitting() != IEngine::QUIT_NOTQUITTING )
		return CallWindowProc( m_ChainedWindowProc, hWnd, uMsg, wParam, lParam );
#endif // WIN32

	//
	// Note: NO engine state should be changed in here while in VCR record or playback. 
	// We can send whatever we want to Windows, but if we change its state in here instead of 
	// in DispatchAllStoredGameMessages, the playback may not work because Windows messages 
	// are not deterministic, so you might get different messages during playback than you did during record.
	//
	InputEvent_t event;
	memset( &event, 0, sizeof(event) );
	event.m_nTick = g_pInputSystem->GetPollTick();

	switch ( uMsg )
	{
	case WM_CREATE:
		::SetForegroundWindow( hWnd );
		break;

	case WM_ACTIVATEAPP:
		{
			if ( CanPostActivateEvents() )
			{
				bool bActivated = ( wParam == 1 );
				event.m_nType = IE_AppActivated;
				event.m_nData = bActivated;
				g_pInputSystem->PostUserEvent( event );
			}
			// handle focus changes including fullscreen 
			if ( wParam == 0 )
			{
				S_UpdateWindowFocus( false );
			}
			else
			{
				S_UpdateWindowFocus( true );
			}
		}
		break;

	case WM_POWERBROADCAST:
		// Don't go into Sleep mode when running engine, we crash on resume for some reason (as
		//  do half of the apps I have running usually anyway...)
		if ( wParam == PBT_APMQUERYSUSPEND )
		{
			Msg( "OS requested hibernation, ignoring request.\n" );
			return BROADCAST_QUERY_DENY;
		}

		bCallDefault = true;
		break;

#if defined( WIN32 )
	case WM_SYSCOMMAND:
		if ( ( wParam == SC_MONITORPOWER ) || ( wParam == SC_KEYMENU ) || ( wParam == SC_SCREENSAVE ) )
            return lRet;
    
		if ( wParam == SC_CLOSE ) 
		{
			// handle the close message, but make sure 
			// it's not because we accidently hit ALT-F4
			if ( HIBYTE(GetKeyState(VK_LMENU)) || HIBYTE(GetKeyState(VK_RMENU) ) )
				return lRet;

			Cbuf_Clear( Cbuf_GetCurrentPlayer() );
			Cbuf_AddText( Cbuf_GetCurrentPlayer(), "quit\n" );
		}

#ifndef DEDICATED
		S_BlockSound();
		S_ClearBuffer();
#endif

		lRet = CallWindowProc( m_ChainedWindowProc, hWnd, uMsg, wParam, lParam );

#ifndef DEDICATED
		S_UnblockSound();
#endif
		break;
#endif

	case WM_SYS_SHUTDOWNREQUEST:
		Assert( IsGameConsole() );
		Cbuf_Clear( Cbuf_GetCurrentPlayer() );
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "quit_gameconsole\n" );
		break;

	case WM_MOVE:
		event.m_nType = IE_WindowMove;
		event.m_nData = (short)LOWORD(lParam);
		event.m_nData2 = (short)HIWORD(lParam);
		g_pInputSystem->PostUserEvent( event );
		break;

#if defined( WIN32 )
	case WM_SIZE:
		if ( wParam != SIZE_MINIMIZED )
		{
			// Update restored client rect
			::GetClientRect( hWnd, &m_rcLastRestoredClientRect );
		}
		else
		{
#ifndef _GAMECONSOLE
			// Fix the window rect to have same client area as it used to have
			// before it got minimized
			RECT rcWindow;
			::GetWindowRect( hWnd, &rcWindow );

			rcWindow.right = rcWindow.left + m_rcLastRestoredClientRect.right;
			rcWindow.bottom = rcWindow.top + m_rcLastRestoredClientRect.bottom;

			::AdjustWindowRect( &rcWindow, ::GetWindowLong( hWnd, GWL_STYLE ), FALSE );
			::MoveWindow( hWnd, rcWindow.left, rcWindow.top,
				rcWindow.right - rcWindow.left, rcWindow.bottom - rcWindow.top, FALSE );
#endif
		}
		break;
#endif

	case WM_SETFOCUS:
		if ( g_pHammer )
			g_pHammer->NoteEngineGotFocus();
		break;
	
	case WM_SYSCHAR:
		// keep Alt-Space from happening
		break;

	case WM_COPYDATA:
		//
		// Researching all codebase legacy yields the following use cases.
		// COPYDATASTRUCT.dwParam = 0 in most cases:
		// + another engine instance passing over commandline when executed with -hijack param
		// + worldcraft editor sending a command
		// + mdlviewer sending a reload model command
		// + Hammer -> engine remote console command.
		// COPYDATASTRUCT.cbData = 0 in another case:
		// + materialsystem enumerating and sending message to other materialsystem windows
		// Our WNDPROC should return true to indicate that the message was handled.
		//
		{
			COPYDATASTRUCT &cpData = *( ( COPYDATASTRUCT * ) lParam );
			if ( cpData.cbData )
			{	// There is payload supplied to the message
				if ( cpData.dwData == 0 )
				{	// Legacy protocol to put console command into command buffer
					const char *pcBuffer = ( const char * ) ( cpData.lpData );
					Cbuf_AddText( Cbuf_GetCurrentPlayer(), pcBuffer );
					lRet = 1;
				}
				else if ( cpData.dwData == 0x43525950 ) // CRYP
				{	// Encryption key supplied for connection
					// Format:
					// dot.ted.ip.adr:port>4bytes16bytesupto256bytes
					const char *pcBuffer = ( const char * ) ( cpData.lpData );
					const char *pcTerm = V_strnchr( pcBuffer, '>', 24 );
					if ( pcTerm && pcTerm > pcBuffer )
					{
						DWORD numBytesForAddress = pcTerm - pcBuffer + 1;
						if ( ( numBytesForAddress < cpData.cbData ) &&
							( cpData.cbData - numBytesForAddress > sizeof( int32 ) + NET_CRYPT_KEY_LENGTH ) &&
							( cpData.cbData - numBytesForAddress - sizeof( int32 ) - NET_CRYPT_KEY_LENGTH <= 256 ) &&
							!!( *reinterpret_cast< const int32 * >( pcTerm + 1 ) & 0xFFFF0000 ) ) // client key must use high bits and be not zero
						{
							CFmtStr fmtAddr( "%.*s", pcTerm - pcBuffer, pcBuffer );
							extern void RegisterServerCertificate( char const *szServerAddress, int numBytesPayload, void const *pvPayload );
							RegisterServerCertificate( fmtAddr.Access(), cpData.cbData - numBytesForAddress, pcTerm + 1 );
							lRet = 1;
						}
					}
				}
			}
		}
		break;

#if defined( _GAMECONSOLE )
	case WM_XREMOTECOMMAND:
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), (const char*)lParam );
		break;

	case WM_SYS_STORAGEDEVICESCHANGED:
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnSysStorageDevicesChanged" ) );
		break;

	case WM_LIVE_CONTENT_INSTALLED:
		{
#if defined ( _X360 )
			bool isArcadeTitleUnlocked = g_pXboxSystem->IsArcadeTitleUnlocked();
			g_pXboxSystem->UpdateArcadeTitleUnlockStatus();
			if ( !isArcadeTitleUnlocked && g_pXboxSystem->IsArcadeTitleUnlocked() )
			{
				g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnUnlockArcadeTitle" ) );
			}
#endif
			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnDowloadableContentInstalled" ) );
			break;
		}
	case WM_LIVE_MEMBERSHIP_PURCHASED:
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnLiveMembershipPurchased" ) );
		break;

	case WM_LIVE_VOICECHAT_AWAY:
#if defined( _X360 )
		// If we're triggered with lParam = true, we are now using LIVE Party Chat or Private Chat, not the Game Chat Channel.
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnLiveVoicechatAway", "NotTitleChat", ( lParam == 1 ) ? "1" : "0" ) );
#endif
		break;

	case WM_XMP_PLAYBACKCONTROLLERCHANGED:
		S_EnableMusic( lParam != 0 );
		break;

	case WM_LIVE_INVITE_ACCEPTED:
		XBX_HandleInvite( LOWORD( lParam ) );
		break;

	case WM_SYS_SIGNINCHANGED:
		{

		xevent_SYS_SIGNINCHANGED_t *pSysEvent = reinterpret_cast< xevent_SYS_SIGNINCHANGED_t * >( lParam );
		Assert( pSysEvent );
		if ( !pSysEvent )
			break;
#if defined( _X360 ) && !defined( _CERT_NODEFINE )
		DevMsg( "WM_SYS_SIGNINCHANGED( ptr=0x%p, 0x%08X )\n", pSysEvent, pSysEvent->dwParam );
		for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
		{
			XUSER_SIGNIN_STATE eState = XUserGetSigninState( k );
			XUID xid;
			if ( ERROR_SUCCESS != XUserGetXUID( k, &xid ) )
				xid = 0ull;
			DevMsg( "                    User%d [ %llx %d ] XUID=%llx state = %d\n", k, pSysEvent->xuid[k], pSysEvent->state[k], xid, eState );
		}
#endif
		if ( pSysEvent->dwParam )
		{
			//
			//	This is a special handler for crazy Xbox LIVE notifications
			//	when console lost connection to Secure Gateway and tries to
			//	re-establish its security tickets and other secure crap.
			//	See: https://bugbait.valvesoftware.com/show_bug.cgi?id=27583
			//	TCR 001 BAS Game Stability
			//	Repro Steps:
			//		1) Launch [Game] with two controllers and an extra profile that is Gold, and has no Ethernet connected.
			//		2) From the main menu select "Start Game"
			//		3) During gameplay have the inactive controller sign into the gold profile.
			//	It will generate the following sign-in notification:
			//		[DBG]: [XNET:2] AuthWarn: SG connection failed!  Retrying with fresh ticket (update 0).
			//		[DBG]: [XNET:2] AuthWarn: XNetDnsLookup timed out for XEAS.PART.XBOXLIVE.COM
			//		[DBG]: [XNET:2] Warning: Unexpected TGT error 0x80151904!
			//		WM_SYS_SIGNINCHANGED( 0x00000000, 0x00000001 )
			//			User0 XUID=0 state = 0		<--- all the users state is reported as signed out with NULL XUID
			//		WM_LIVE_CONNECTIONCHANGED( 0x00000000, 0x80151904 )
			//			User0 XUID=0 state = 0
			//		Followed by:
			//		WM_SYS_SIGNINCHANGED( 0x00000000, 0x00000002 )
			//			User0 XUID=e0000a2e5a849e42 state = 1
			//			User1 XUID=e0000b49fab8416e state = 1
			//	To handle this we will ignore notifications when controller mask doesn't specify signed-in controllers.
			//
			bool bSomeUsersStillSignedIn = false;
			for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
			{
				if ( (pSysEvent->dwParam & ( 1 << k )) == 0 )
					continue;

				if ( pSysEvent->state[k] != eXUserSigninState_NotSignedIn )
				{
					bSomeUsersStillSignedIn = true;
					break;
				}
			}
			if ( !bSomeUsersStillSignedIn )
			{
				// This is the crazy notification mentioned above, discard
				DevMsg( "WM_SYS_SIGNINCHANGED is discarded due to invalid parameters!\n" );
				break;
			}
		}
		{
			//
			//	This is a handler for TCR exploit of X360 blade
			//	TCR 015 BAS Sign-In Changes
			//	Using inactive controller to initiate sign-in, but actually
			//	pressing last "A" button on an active controller will not
			//	generate a sign-out message, but will generate a new sign-in
			//	message.
			//	We need to keep XUIDs around and if a new sign-in message is
			//	coming from a new XUID we fake a sign-out message first and
			//	then the new sign-in message.
			//

			MEM_ALLOC_CREDIT();
			static XUID s_arrSignedInXUIDs[ XUSER_MAX_COUNT ] = { 0 };
			KeyValues *pEvent = NULL;
			for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
			{
				XUID xidOld = s_arrSignedInXUIDs[k];
				XUID xidNew = pSysEvent->xuid[k];

				if ( xidOld )
				{
					// We had a ctrl signed in this slot
					bool bSignedOut = false;
					
					if ( pSysEvent->state[k] == eXUserSigninState_NotSignedIn )
					{
						bSignedOut = true;
					}
					else if ( !IsEqualXUID( xidNew, xidOld ) )
					{
						bSignedOut = true;
					}

					// If user signed out, add to notification
					if ( bSignedOut )
					{
						if ( !pEvent )
						{
							pEvent = new KeyValues( "OnSysSigninChange" );
							pEvent->SetString( "action", "signout" );
						}
						int idx = pEvent->GetInt( "numUsers", 0 );
						pEvent->SetInt( "numUsers", idx + 1 );

						int nMask = pEvent->GetInt( "mask", 0 );
						pEvent->SetInt( "mask", nMask | ( 1 << k ) );

						char bufUserIdx[32];
						sprintf( bufUserIdx, "user%d", idx );
						pEvent->SetInt( bufUserIdx, k );
					}
				}

				s_arrSignedInXUIDs[k] = xidNew;
			}
			if ( pEvent )
			{
				g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( pEvent );
			}
		}
		if ( pSysEvent->dwParam )
		{
			MEM_ALLOC_CREDIT();
			KeyValues *pEvent = new KeyValues( "OnSysSigninChange" );
			pEvent->SetString( "action", "signin" );
			for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
			{
				if ( (pSysEvent->dwParam & ( 1 << k )) == 0 )
					continue;

				int idx = pEvent->GetInt( "numUsers", 0 );
				pEvent->SetInt( "numUsers", idx + 1 );

				int nMask = pEvent->GetInt( "mask", 0 );
				pEvent->SetInt( "mask", nMask | ( 1 << k ) );

				char bufUserIdx[32];
				sprintf( bufUserIdx, "user%d", idx );
				pEvent->SetInt( bufUserIdx, k );
			}
			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( pEvent );
		}

		}
		break;

	case WM_LIVE_CONNECTIONCHANGED:
#if defined( _X360 ) && !defined( _CERT )
		DevMsg( "WM_LIVE_CONNECTIONCHANGED( 0x%08X, 0x%08X )\n", wParam, lParam );
		for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
		{
			XUSER_SIGNIN_STATE eState = XUserGetSigninState( k );
			XUID xid;
			if ( ERROR_SUCCESS != XUserGetXUID( k, &xid ) )
				xid = 0ull;
			DevMsg( "                    User%d XUID=%llx state = %d\n", k, xid, eState );
		}
#endif

		// Vitaliy [8/5/2008]
		// Triggering any callbacks from inside WM_LIVE_CONNECTIONCHANGED is
		// unreliable because access to accounts sign-in information is blocked.
		// Repro case: user1 is signed into Live, user2 signs in with local account
		// then WM_LIVE_CONNECTIONCHANGED will be triggered and XUserGetSigninState
		// will be returning 0 for all user ids.
		break;   // end case WM_LIVE_CONNECTIONCHANGED

	case WM_SYS_UI:
		if ( lParam )
		{
			// When the blade opens, release all buttons
			g_pInputSystem->ResetInputState();

			// Don't activate it if it's already active (a sub window may be active)
			// Multiplayer doesn't want the UI to appear, since it can't pause anyway
			if ( !EngineVGui()->IsGameUIVisible() && sv.IsActive() && sv.IsSinglePlayerGame() )
			{
				Cbuf_AddText( Cbuf_GetCurrentPlayer(), "gameui_activate" );
			}
		}
		{
		MEM_ALLOC_CREDIT();
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnSysXUIEvent", "action", lParam ? "opening" : "closed" ) );

		}
		break;

	case WM_FRIENDS_FRIEND_ADDED:		// Need to update mutelist for friends changes in case of Friends-Only privileges
	case WM_FRIENDS_FRIEND_REMOVED:
	case WM_SYS_MUTELISTCHANGED:
		{
			MEM_ALLOC_CREDIT();
			g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnSysMuteListChanged" ) );
		}
		break;

	case WM_SYS_PROFILESETTINGCHANGED:
		{
			MEM_ALLOC_CREDIT();
			if ( KeyValues *kvNotify = new KeyValues( "OnSysProfileSettingsChanged" ) )
			{
				kvNotify->SetInt( "mask", lParam );
				for ( int k = 0; k < XUSER_MAX_COUNT; ++ k )
				{
					if ( lParam & ( 1 << k ) )
					{
						kvNotify->SetInt( CFmtStr( "user%d", k ), 1 );
					}
				}
				g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( kvNotify );
			}
		}
		break;

	case WM_SYS_INPUTDEVICESCHANGED:
		{
			MEM_ALLOC_CREDIT();
			int nDisconnectedDeviceMask = 0;
			for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
			{
#ifdef _X360
				XINPUT_CAPABILITIES caps;
				if ( XInputGetCapabilities( XBX_GetUserId(k), XINPUT_FLAG_GAMEPAD, &caps ) == ERROR_DEVICE_NOT_CONNECTED )
#elif defined( _PS3 )
				if ( lParam & ( 1 << XBX_GetUserId(k) ) )	// PS3 passes disconnected controllers mask in lParam
#else
				Assert(0);
				if ( 0 )
#endif
				{
					nDisconnectedDeviceMask |= ( 1 << k );
				}				
			}

			if ( nDisconnectedDeviceMask )
			{
				// This message is only sent when one of our active users has lost their controller connection
				// FIXME: Only do this when the guest is at fault?  A "toast" is already presented to the user by the API otherwise.
				g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
					"OnSysInputDevicesChanged", "mask", nDisconnectedDeviceMask ) );
			}
		}
		break;

#if defined( _DEMO )
	case WM_XCONTROLLER_KEY:
		if ( lParam )
		{
			// any keydown activity resets the timeout or changes into interactivbe demo mode
			Host_RestartDemoTimeout( true );
		}
		bCallDefault = true;
		break;
#endif
#endif

#if defined( WIN32 )
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		RECT rcClient;
		GetClientRect( hWnd, &rcClient );
		EndPaint(hWnd, &ps);
		break;
#endif

#if defined( WIN32 ) && !defined( _X360 )
	case WM_DISPLAYCHANGE:
		if ( !m_iDesktopHeight || !m_iDesktopWidth )
		{
			UpdateDesktopInformation( wParam, lParam );
		}
		break;
#endif

	case WM_IME_NOTIFY:
		switch ( wParam )
		{
		default:
			break;

#ifndef DEDICATED
		case 14:
            if ( !videomode->IsWindowedMode() )
				return 0;
			break;
#endif
		}
		bCallDefault = true;
		break;

	default:
		bCallDefault = true;
	    break;
    }

	if ( bCallDefault )
	{
#ifdef _PS3
		lRet = 0;
#else
		lRet = CallWindowProc( m_ChainedWindowProc, hWnd, uMsg, wParam, lParam );
#endif
	}

    // return 0 if handled message, 1 if not
    return lRet;
}
#elif defined(OSX)

#elif defined( LINUX )

#elif defined(_WIN32)

#else
#error
#endif


#if defined( WIN32 ) && !defined( USE_SDL )
//-----------------------------------------------------------------------------
// Creates the game window 
//-----------------------------------------------------------------------------
static LRESULT WINAPI HLEngineWindowProc( HWND hWnd, UINT uMsg, WPARAM  wParam, LPARAM  lParam )
{
	return g_Game.WindowProc( hWnd, uMsg, wParam, lParam );
}

#define DEFAULT_EXE_ICON 101

static void DoSomeSocketStuffInOrderToGetZoneAlarmToNoticeUs( void )
{
#ifdef IS_WINDOWS_PC
	WSAData wsaData;
	if ( ! WSAStartup( 0x0101, &wsaData ) )
	{
		SOCKET tmpSocket = socket( AF_INET, SOCK_DGRAM, 0 );
		if ( tmpSocket != INVALID_SOCKET )
		{
			char Options[]={ 1 };
			setsockopt( tmpSocket, SOL_SOCKET, SO_BROADCAST, Options, sizeof(Options));
			char pszHostName[256];
			gethostname( pszHostName, sizeof( pszHostName ) );
			hostent *hInfo = gethostbyname( pszHostName );
			if ( hInfo )
			{
				sockaddr_in myIpAddress;
				memset( &myIpAddress, 0, sizeof( myIpAddress ) );
				myIpAddress.sin_family = AF_INET;
				myIpAddress.sin_port = htons( 27015 );			// our normal server port
				myIpAddress.sin_addr.S_un.S_un_b.s_b1 = hInfo->h_addr_list[0][0];
				myIpAddress.sin_addr.S_un.S_un_b.s_b2 = hInfo->h_addr_list[0][1];
				myIpAddress.sin_addr.S_un.S_un_b.s_b3 = hInfo->h_addr_list[0][2];
				myIpAddress.sin_addr.S_un.S_un_b.s_b4 = hInfo->h_addr_list[0][3];
				if ( bind( tmpSocket, ( sockaddr * ) &myIpAddress, sizeof( myIpAddress ) ) != -1 )
				{
					if ( sendto( tmpSocket, pszHostName, 1, 0, ( sockaddr *) &myIpAddress, sizeof( myIpAddress ) ) == -1 )
					{
						// error?
					}

				}
			}
			closesocket( tmpSocket );
		}
		WSACleanup();
	}
	
#endif
}
#endif

bool CGame::CreateGameWindow( void )
{
	// get the window name
	char windowName[256];
	windowName[0] = 0;
	KeyValues *modinfo = new KeyValues("ModInfo");
	if (modinfo->LoadFromFile(g_pFileSystem, "gameinfo.txt"))
	{
		Q_strncpy( windowName, modinfo->GetString("game"), sizeof(windowName) );
	}

	if (!windowName[0])
	{
		Q_strncpy( windowName, "HALF-LIFE 2", sizeof(windowName) );
	}

	if ( IsOpenGL() )
	{
		V_strcat( windowName, " - OpenGL", sizeof( windowName ) );
	}

#if PIX_ENABLE || defined( PIX_INSTRUMENTATION )
	// PIX_ENABLE/PIX_INSTRUMENTATION is a big slowdown (that should never be checked in, but sometimes is by accident), so add this to the Window title too.
	V_strcat( windowName, " - PIX_ENABLE", sizeof( windowName ) );
#endif

	const char *p = CommandLine()->ParmValue( "-window_name_suffix", "" );
	if ( p && V_strlen( p ) )
	{
		V_strcat( windowName, " - ", sizeof( windowName ) );
		V_strcat( windowName, p, sizeof( windowName ) );
	}
		
#if defined( USE_SDL )
	modinfo->deleteThis();
	modinfo = NULL;

	if ( !g_pLauncherMgr->CreateGameWindow( windowName, true, 0, 0, true ) )
	{
		Error( "Fatal Error:  Unable to create game window!" );
		return false;
	}
	
	char localPath[ MAX_PATH ];
	if ( g_pFileSystem->GetLocalPath( "resource/game-icon.bmp", localPath, sizeof(localPath) ) )
	{
		g_pFileSystem->GetLocalCopy( localPath );
		g_pLauncherMgr->SetApplicationIcon( localPath );
	}
	
	SetMainWindow( ( HWND )g_pLauncherMgr->GetWindowRef() );

	AttachToWindow( );
	return true;
#elif defined( WIN32 ) && !defined( USE_SDL )
#ifndef DEDICATED

#if !defined( _X360 )
	WNDCLASSW wc;
#else
	WNDCLASS wc;
#endif
	memset( &wc, 0, sizeof( wc ) );

    wc.style         = CS_OWNDC | CS_DBLCLKS;

#if !defined( _GAMECONSOLE )
    wc.lpfnWndProc   = DefWindowProcW;
#else
	wc.lpfnWndProc   = CallDefaultWindowProc;
#endif
    wc.hInstance     = m_hInstance;
    wc.lpszClassName = CLASSNAME;

	// find the icon file in the filesystem
	if ( IsPC() )
	{
		char localPath[ MAX_PATH ];
		if ( g_pFileSystem->GetLocalPath( "resource/game.ico", localPath, sizeof(localPath) ) )
		{
			g_pFileSystem->GetLocalCopy( localPath );
			wc.hIcon = (HICON)::LoadImage(NULL, localPath, IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
		}
		else
		{
			wc.hIcon = (HICON)LoadIcon( GetModuleHandle( 0 ), MAKEINTRESOURCE( DEFAULT_EXE_ICON ) );
		}
	}
	

#ifndef DEDICATED
	char const *pszGameType = modinfo->GetString( "type" );
	if ( pszGameType && Q_stristr( pszGameType, "multiplayer" ) )
		DoSomeSocketStuffInOrderToGetZoneAlarmToNoticeUs();
#endif

	wchar_t uc[512];
	if ( IsPC() )
	{
		::MultiByteToWideChar(CP_UTF8, 0, windowName, -1, uc, sizeof( uc ) / sizeof(wchar_t));
	}

	modinfo->deleteThis();
	modinfo = NULL;
	// Oops, we didn't clean up the class registration from last cycle which
	// might mean that the wndproc pointer is bogus
#ifndef _X360
	UnregisterClassW( CLASSNAME, m_hInstance );
	// Register it again
    RegisterClassW( &wc );
#else
	RegisterClass( &wc );
#endif

	// Note, it's hidden
	DWORD style = WS_POPUP | WS_CLIPSIBLINGS;
	
	// Give it a frame if we want a border
	if ( videomode->IsWindowedMode() )
	{
		if( !CommandLine()->FindParm( "-noborder" )&& !videomode->NoWindowBorder() )
		{
			style |= WS_OVERLAPPEDWINDOW;
			style &= ~WS_THICKFRAME;
		}
	}

	// Never a max box
	style &= ~WS_MAXIMIZEBOX;

	int w, h;

	// Create a full screen size window by default, it'll get resized later anyway
	w = GetSystemMetrics( SM_CXSCREEN );
	h = GetSystemMetrics( SM_CYSCREEN );

	// Create the window
	DWORD exFlags = 0;
	if ( g_bTextMode )
	{
		style &= ~WS_VISIBLE;
		exFlags |= WS_EX_TOOLWINDOW; // So it doesn't show up in the taskbar.
	}

#if !defined( _X360 )
	HWND hwnd = CreateWindowExW( exFlags, CLASSNAME, uc, style, 
		0, 0, w, h, NULL, NULL, m_hInstance, NULL );
	// NOTE: On some cards, CreateWindowExW slams the FPU control word
	SetupFPUControlWord();
#else
	HWND hwnd = CreateWindowEx( exFlags, CLASSNAME, windowName, style, 
			0, 0, w, h, NULL, NULL, m_hInstance, NULL );
#endif

	if ( !hwnd )
	{
		Error( "Fatal Error:  Unable to create game window!" );
		return false;
	}

	SetMainWindow( hwnd );

	AttachToWindow( );
	return true;
#else
	return true;
#endif
#elif defined(OSX)
	modinfo->deleteThis();
	modinfo = NULL;

	if ( !g_pLauncherMgr->CreateGameWindow( windowName, true, 640, 480 ) )
	{
		Error( "Fatal Error:  Unable to create game window!" );
		return false;
	}
	
	char localPath[ MAX_PATH ];
	if ( g_pFileSystem->GetLocalPath( "resource/game.icns", localPath, sizeof(localPath) ) )
	{
		g_pFileSystem->GetLocalCopy( localPath );
		g_pLauncherMgr->SetApplicationIcon( localPath );
	}
	
	SetMainWindow( g_pLauncherMgr->GetWindowRef() );

	AttachToWindow( );
	return true;
#else
#error
#endif
}


//-----------------------------------------------------------------------------
// Destroys the game window 
//-----------------------------------------------------------------------------
void CGame::DestroyGameWindow()
{
#if defined( USE_SDL )
	g_pLauncherMgr->DestroyGameWindow();
#elif defined( WIN32 )
#ifndef DEDICATED
	// Destroy all things created when the window was created
	if ( !m_bExternallySuppliedWindow )
	{
		DetachFromWindow( );

		if ( m_hWindow )
		{
			DestroyWindow( m_hWindow );
			m_hWindow = (HWND)0;
		}

#if !defined( _X360 )
		UnregisterClassW( CLASSNAME, m_hInstance );
#else
		UnregisterClass( CLASSNAME, m_hInstance );
#endif
	}
	else
	{
		m_hWindow = (HWND)0;
		m_bExternallySuppliedWindow = false;
	}

#endif // !DEDICATED 
#elif defined( OSX )
	g_pLauncherMgr->DestroyGameWindow();
#elif defined (_PS3)
#else
#error
#endif
}


//-----------------------------------------------------------------------------
// This is used in edit mode to specify a particular game window (created by hammer)
//-----------------------------------------------------------------------------
void CGame::SetGameWindow( void *hWnd )
{
	m_bExternallySuppliedWindow = true;
#if defined( USE_SDL )
	SDL_RaiseWindow( (SDL_Window *)hWnd );
#elif defined( WIN32 ) 
	SetMainWindow( (HWND)hWnd );
#elif defined( OSX ) && defined( PLATFORM_64BITS )
	Assert( !"unimpl OSX-64" );
#elif defined( OSX )
	SetUserFocusWindow( (WindowRef)hWnd );
#else
#error
#endif
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGame::AttachToWindow()
{
	if ( !m_hWindow )
		return;

#if defined( WIN32 ) && !defined( USE_SDL )
	m_ChainedWindowProc = (WNDPROC)GetWindowLongPtrW( m_hWindow, GWLP_WNDPROC );
	SetWindowLongPtrW( m_hWindow, GWLP_WNDPROC, (LONG_PTR)HLEngineWindowProc );
#endif
	if ( g_pInputSystem )
	{
		// Attach the input system window proc
		g_pInputSystem->AttachToWindow( (void *)m_hWindow );
		g_pInputSystem->EnableInput( true );
		g_pInputSystem->EnableMessagePump( false );
	}

	if ( g_pMatSystemSurface )
	{
		// Attach the vgui matsurface window proc
        g_pMatSystemSurface->SetAppDrivesInput( true );
        g_pMatSystemSurface->EnableWindowsMessages( true );
	}
}

void CGame::DetachFromWindow()
{
#if defined( WIN32 ) && !defined( USE_SDL )
	if ( !m_hWindow || !m_ChainedWindowProc )
	{
		m_ChainedWindowProc = NULL;
		return;
	}
#endif

	if ( g_pMatSystemSurface )
	{
		// Detach the vgui matsurface
        g_pMatSystemSurface->EnableWindowsMessages( false );
    }

	if ( g_pInputSystem )
	{
		// Detach the input system window proc
		g_pInputSystem->EnableInput( false );
		g_pInputSystem->DetachFromWindow( );
	}

#if defined( WIN32 ) && !defined( USE_SDL )
	Assert( (WNDPROC)GetWindowLongPtrW( m_hWindow, GWLP_WNDPROC ) == HLEngineWindowProc );
	SetWindowLongPtrW( m_hWindow, GWLP_WNDPROC, (LONG_PTR)m_ChainedWindowProc );
#endif
}


//-----------------------------------------------------------------------------
// This is used in edit mode to override the default wnd proc associated w/
// the game window specified in SetGameWindow. 
//-----------------------------------------------------------------------------
bool CGame::InputAttachToGameWindow()
{
	// We can't use this feature unless we didn't control the creation of the window
	if ( !m_bExternallySuppliedWindow )
		return true;

	AttachToWindow();

#ifndef DEDICATED
	vgui::surface()->OnScreenSizeChanged( videomode->GetModeWidth(), videomode->GetModeHeight() );
#endif

	// We don't get WM_ACTIVATEAPP messages in this case; simulate one.
	AppActivate( true );

#if defined( WIN32 ) && !defined( USE_SDL )
	// Capture + hide the mouse
    g_pInputStackSystem->SetMouseCapture( m_hInputContext, true );
#elif defined(OSX)
	Assert( !"Impl me" );
	return false;
#elif defined( LINUX )
	Assert( !"Impl me" );
	return false;
#elif defined(_WIN32)
	Assert( !"Impl me" );
	return false;
#elif defined(_PS3)
#else
#error
#endif
	return true;
}

void CGame::InputDetachFromGameWindow()
{
	// We can't use this feature unless we didn't control the creation of the window
	if ( !m_bExternallySuppliedWindow )
		return;

#if defined( WIN32 ) && !defined( USE_SDL )
	if ( !m_ChainedWindowProc )
		return;

	// Release + show the mouse
	ReleaseCapture();
#elif defined(OSX)
	Assert( !"Impl me" );
#elif defined( LINUX )
	Assert( !"Impl me" );
#elif defined(_WIN32)
	Assert( !"Impl me" );
#elif defined(_PS3)
#else
#error
#endif

	// We don't get WM_ACTIVATEAPP messages in this case; simulate one.
	AppActivate( false );

	DetachFromWindow();
}

void CGame::PlayStartupVideos( void )
{
	if ( Plat_IsInBenchmarkMode() )
		return;

#ifndef DEDICATED
	// Wait for the mode to change and stabilized
	// FIXME: There's really no way to know when this is completed, so we have to guess a time that will mostly be correct
	if ( IsPC() && videomode->IsWindowedMode() == false )
	{
		ThreadSleep( 1000 );
	}

	bool bEndGame = CommandLine()->CheckParm("-endgamevid") ? true : false;
	bool bRecap = CommandLine()->CheckParm("-recapvid") ? true : false;	// FIXME: This is a temp addition until the movie playback is centralized -- jdw
	bool bNeedHealthWarning = IsPC() && g_pFullFileSystem->FileExists( "media/HealthWarning.txt" );

	if ( !bNeedHealthWarning && 
		!bEndGame && 
		!bRecap && 
		( CommandLine()->CheckParm( "-dev" ) || 
			CommandLine()->CheckParm( "-novid" ) || 
			CommandLine()->CheckParm( "-allowdebug" ) ||
			CommandLine()->CheckParm( "-console" ) ||
			CommandLine()->CheckParm( "-toconsole" ) ) )
		return;

	char *pszFile = "media/startupvids" PLATFORM_EXT ".txt";
	if ( bEndGame )
	{
		// Don't go back into the map that triggered this.
		CommandLine()->RemoveParm( "+map" );
		CommandLine()->RemoveParm( "+load" );
		
		pszFile = "media/EndGameVids.txt";
	}
	else if ( bRecap )
	{
		pszFile = "media/RecapVids.txt";
	}

#if defined( PLATFORM_WINDOWS ) && defined( BINK_VIDEO )
	VAudioInit();
	void *pMilesEngine = NULL;
	if ( g_pBIK) 
	{
		ConVarRef windows_speaker_config("windows_speaker_config");
		
		if ( windows_speaker_config.IsValid() && windows_speaker_config.GetInt() >= 5 )
		{
			pMilesEngine = vaudio ? vaudio->CreateMilesAudioEngine() : NULL;
#if !defined( _GAMECONSOLE )
			g_pBIK->SetMilesSoundDevice( pMilesEngine );
#endif	//!defined( _GAMECONSOLE )
		}
		else
		{
#if !defined( _GAMECONSOLE )
			g_pBIK->SetMilesSoundDevice( NULL );
#endif //!defined( _GAMECONSOLE )
		}
	}
#endif // defined( PLATFORM_WINDOWS ) && defined( BINK_VIDEO )

	PlayVideoListAndWait( pszFile );

#if defined( PLATFORM_WINDOWS ) && defined( BINK_VIDEO )
	if ( pMilesEngine )
	{
#if !defined( _GAMECONSOLE )
		g_pBIK->SetMilesSoundDevice( NULL );
#endif //!defined( _GAMECONSOLE )
		vaudio->DestroyMilesAudioEngine( pMilesEngine );
	}
#endif

#endif // DEDICATED
}
	
#define MAX_CAPTION_LENGTH	256

class CCaptionSequencer
{
public:
	CCaptionSequencer( void ) : m_bCaptions( false )
	{
		Reset();
	}

	void Reset( void )
	{
		// captioning start when rendering stable, not simply at movie start
		m_CaptionStartTime = 0;

		// initial priming state to fetch a caption
		m_bShowingCaption = false;
		m_bCaptionStale = true;

		m_CurCaptionString[0] = '\0';
		m_CurCaptionStartTime = 0.0f;
		m_CurCaptionEndTime = 0.0f;
		m_CurCaptionColor = 0xFFFFFFFF;
		if ( m_CaptionBuf.TellPut() )
		{
			// reset to start
			m_CaptionBuf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
		}
	}

	void Init( const char *pFilename )
	{
		m_bCaptions = false;	

		if ( g_pFullFileSystem->ReadFile( pFilename, "GAME", m_CaptionBuf ) )
		{
			// FIXME: This needs the MOD dir to construct the filename properly!
			//		  See me when this is being merged into Main -- jweier
			
			g_pVGuiLocalize->AddFile( "resource/l4d360ui_%language%.txt", "GAME", true );
			
			m_bCaptions = true;	
		}
	}

	void SetStartTime( float flStarTtime )
	{
		// Start our captions now
		m_CaptionStartTime = Plat_FloatTime();
	}

	bool GetCaptionToken( char *token, int tokenLen )
	{
		if ( !token || !tokenLen )
			return false;

		if ( !m_CaptionBuf.IsValid() )
		{
			// end of data
			return false;
		}

		m_CaptionBuf.GetLine( token, tokenLen );
#ifdef _WIN32
		char *pCRLF = V_stristr( token, "\r" );
		if ( pCRLF )
		{
			*pCRLF = '\0';
		}
		m_CaptionBuf.SeekGet( CUtlBuffer::SEEK_CURRENT, 1 );
#else
		char *pCRLF = V_stristr( token, "\n" );
		if ( pCRLF )
		{
			*pCRLF = '\0';
		}
#endif
		return true;
	}

	bool GetNextCaption( void )
	{
		char buff[MAX_CAPTION_LENGTH];

		if ( !GetCaptionToken( m_CurCaptionString, sizeof( m_CurCaptionString ) ) )
		{
			// end of captions
			m_CurCaptionString[0] = '\0';
			return false;
		}

		// hex color		
		GetCaptionToken( buff, sizeof( buff ) );
		sscanf( buff, "%x", &m_CurCaptionColor );

		// float start time
		GetCaptionToken( buff, sizeof( buff ) );
		m_CurCaptionStartTime = atof( buff );

		// float end time
		GetCaptionToken( buff, sizeof( buff ) );
		m_CurCaptionEndTime = atof( buff );

		// have valid caption
		m_bCaptionStale = false;
		return true;
	}

	const char *GetCurrentCaption( int *pColorOut )
	{
		if ( m_bCaptions == false )
			return NULL;

		if ( m_CaptionStartTime )
		{
			// get a timeline
			float elapsed = Plat_FloatTime() - m_CaptionStartTime;

			// Get a new caption because we've just finished one
			if ( !m_bShowingCaption && m_bCaptionStale )
			{
				GetNextCaption();
			}

			if ( m_bShowingCaption )
			{
				if ( elapsed > m_CurCaptionEndTime )	// Caption just turned off
				{
					m_bShowingCaption = false;			// Don't draw caption
					m_bCaptionStale = true;				// Trigger getting a new one on the next frame
				}
			}
			else
			{
				if ( elapsed > m_CurCaptionStartTime )	// Turn Caption on
				{
					m_bShowingCaption = true;
				}
			}

			if ( m_bShowingCaption && m_CurCaptionString[0] )
			{
				if ( pColorOut )
				{
					*pColorOut = m_CurCaptionColor;
				}
				return m_CurCaptionString;
			}
		}

		return NULL;
	}

private:
	// Captions / Subtitles
	bool				m_bCaptions;
	bool				m_bShowingCaption;
	bool				m_bCaptionStale;
	vgui::HScheme		m_hCaptionFont;
	float				m_CaptionStartTime;
	CUtlBuffer			m_CaptionBuf;

	char				m_CurCaptionString[MAX_CAPTION_LENGTH];
	float				m_CurCaptionStartTime;
	float				m_CurCaptionEndTime;
	unsigned int		m_CurCaptionColor;
};

// Panel for drawing subtitles on a movie panel

// Panel for drawing subtitles on a movie panel
class CSubtitlePanel : public vgui::Panel
{
public:
	CSubtitlePanel( vgui::Panel *parent, const char *pMovieName, int nPlaybackHeight ) : vgui::Panel( parent, "SubtitlePanel" ) 
	{
		// FIXME: Need a better method for this
		vgui::HScheme hScheme = vgui::scheme()->LoadSchemeFromFile("Resource/SourceScheme.res", "Tracker" );
		vgui::IScheme *pNewScheme = vgui::scheme()->GetIScheme( hScheme );
		if ( pNewScheme )
		{	
			m_hFont = pNewScheme->GetFont( "CloseCaption_IntroMovie", true );
		}

		m_pSubtitleLabel = new vgui::Label( this, "SubtitleLabel", L"" );
		m_pSubtitleLabel->SetFont( m_hFont );
		int fontTall = vgui::surface()->GetFontTall( m_hFont );

		int width, height;
		vgui::surface()->GetScreenSize( width, height );

		// clamp width to title safe area
		int xPos = width * 0.05f;
		width *= 0.9f;

		// assume video is centered
		// must be scaled according to playback height, due to letterboxing
		// don't want to cut into or overlap border, need to be within video, and title safe
		// so pushes up according to font height
		int yOffset = ( nPlaybackHeight - height )/2;
		int yPos = ( 0.85f * nPlaybackHeight - fontTall ) - yOffset;

		// captions are anchored to a baseline and grow upward
		// any resolution changes then are title safe
		m_pSubtitleLabel->SetPos( xPos, yPos );
		m_pSubtitleLabel->SetTall( fontTall*2 );
		m_pSubtitleLabel->SetWide( width );
		m_pSubtitleLabel->SetContentAlignment( vgui::Label::a_center );
		m_pSubtitleLabel->SetCenterWrap( true );

		// Strip our extension
		char captionFilename[MAX_QPATH];
		Q_StripExtension( pMovieName, captionFilename, MAX_QPATH );

		// Now add on the '_captions.txt' ending
		Q_strncat( captionFilename, "_captions.txt", MAX_QPATH );

		// Setup our captions
		m_Captions.Init( captionFilename );
	}

	void StartCaptions( void )
	{
		m_Captions.SetStartTime( Plat_FloatTime() );
	}

	virtual void Paint( void )
	{
		int nColor = 0xFFFFFFFF;
		const char *pCaptionText = m_Captions.GetCurrentCaption( &nColor );

		m_pSubtitleLabel->SetText( pCaptionText );

		// Pull the color out of this hex value
		int r = ( nColor >> 24 ) & 0xFF;
		int g = ( nColor >> 16 ) & 0xFF;
		int b = ( nColor >> 8 ) & 0xFF;
		int a = ( nColor >> 0 ) & 0xFF;
		m_pSubtitleLabel->SetFgColor( Color(r,g,b,a) );

		vgui::Panel::Paint();
	}

private:
	CCaptionSequencer	m_Captions;

	vgui::HFont			m_hFont;
	vgui::Label			*m_pSubtitleLabel;
};


const char *lpszDubbedLanguages[] =
{ 
	"english",
	"french",
	"german",
	"spanish",
	"russian"
};

//-----------------------------------------------------------------------------
// Purpose: Determines if we should be playing with captions
//-----------------------------------------------------------------------------
inline bool ShouldUseCaptioning( void )
{
	char language[64];

	// Fallback to English
	V_strncpy( language, "english", sizeof( language ) );

#if !defined( NO_STEAM ) && !defined( DEDICATED )
	// When Steam isn't running we can't get the language info... 
	if ( Steam3Client().SteamApps() )
	{
		V_strncpy( language, Steam3Client().SteamApps()->GetCurrentGameLanguage(), sizeof(language) );
	}
#endif

	// Iterate through the language we have dubbed and don't subtitle in that case
	for ( int i = 0; i < ARRAYSIZE( lpszDubbedLanguages ); i++ )
	{
		if ( Q_stricmp( language, lpszDubbedLanguages[i] ) == 0 )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Tests for players attempting to skip a movie via keypress
//-----------------------------------------------------------------------------
bool UserRequestingMovieSkip( void )
{
	if ( IsGameConsole() )
	{
		// Any joystick can cause the skip, so we must check all four
		for ( int i = 0; i < XUSER_MAX_COUNT; i++ )
		{
			// If any of these buttons are down, we skip
			if ( g_pInputSystem->IsButtonDown( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_A, i ) ) || 
			 	 g_pInputSystem->IsButtonDown( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, i ) ) || 
				 g_pInputSystem->IsButtonDown( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_X, i ) ) || 
				 g_pInputSystem->IsButtonDown( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_Y, i ) ) || 
				 g_pInputSystem->IsButtonDown( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_START, i ) ) || 
				 g_pInputSystem->IsButtonDown( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_BACK, i ) ) )
			{
				return true;
			}
		}
		
		// Nothing pressed
		return false;
	}

	return ( g_pInputSystem->IsButtonDown( KEY_ESCAPE ) || 
			g_pInputSystem->IsButtonDown( KEY_SPACE ) || 
			g_pInputSystem->IsButtonDown( KEY_ENTER ) );
}

#if defined( _X360 )
static char const * GetConsoleLocaleRatingsBoard()
{
	switch ( XGetLocale() )
	{
	case XC_LOCALE_AUSTRALIA: return "OFLC";
	case XC_LOCALE_AUSTRIA: return "PEGI";
	case XC_LOCALE_BELGIUM: return "PEGI";
	case XC_LOCALE_BRAZIL: return "";
	case XC_LOCALE_CANADA: return "ESRB";
	case XC_LOCALE_CHILE: return "";
	case XC_LOCALE_CHINA: return "";
	case XC_LOCALE_COLOMBIA: return "";
	case XC_LOCALE_CZECH_REPUBLIC: return "PEGI";
	case XC_LOCALE_DENMARK: return "PEGI";
	case XC_LOCALE_FINLAND: return "PEGI";
	case XC_LOCALE_FRANCE: return "PEGI";
	case XC_LOCALE_GERMANY: return "USK";
	case XC_LOCALE_GREECE: return "PEGI";
	case XC_LOCALE_HONG_KONG: return "";
	case XC_LOCALE_HUNGARY: return "";
	case XC_LOCALE_INDIA: return "";
	case XC_LOCALE_IRELAND: return "BBFCPEGI";
	case XC_LOCALE_ITALY: return "PEGI";
	case XC_LOCALE_JAPAN: return "CERO";
	case XC_LOCALE_KOREA: return "GRB";
	case XC_LOCALE_MEXICO: return "";
	case XC_LOCALE_NETHERLANDS: return "PEGI";
	case XC_LOCALE_NEW_ZEALAND: return "OFLC";
	case XC_LOCALE_NORWAY: return "PEGI";
	case XC_LOCALE_POLAND: return "PEGI";
	case XC_LOCALE_PORTUGAL: return "PEGI";
	case XC_LOCALE_SINGAPORE: return "";
	case XC_LOCALE_SLOVAK_REPUBLIC: return "PEGI";
	case XC_LOCALE_SOUTH_AFRICA: return "";
	case XC_LOCALE_SPAIN: return "PEGI";
	case XC_LOCALE_SWEDEN: return "PEGI";
	case XC_LOCALE_SWITZERLAND: return "PEGI";
	case XC_LOCALE_TAIWAN: return "";
	case XC_LOCALE_GREAT_BRITAIN: return "BBFCPEGI";
	case XC_LOCALE_UNITED_STATES: return "ESRB";
	default: return NULL;
	}
}
#endif

void CGame::PlayVideoListAndWait( const char *szVideoFileList, bool bNeedHealthWarning /* = false */ )
{
#ifndef DEDICATED

	CUtlBuffer vidBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFullFileSystem->ReadFile( szVideoFileList, "GAME", vidBuffer ) )
	{
		return;
	}

#if defined( USE_SDL )
	int CursorStateBak = SDL_ShowCursor( -1 );
	SDL_ShowCursor( 0 );
#elif defined( WIN32 )
	// hide cursor while playing videos
	::ShowCursor(FALSE);
#endif
#if defined( OSX ) && !defined( USE_SDL )
    CGDisplayHideCursor( kCGDirectMainDisplay );
#endif
	
#ifdef _X360
	// TCR024
	XMPOverrideBackgroundMusic();
#endif

#ifdef _GAMECONSOLE
	// Install movie player match framework
	extern IMatchFramework *g_pMoviePlayer_MatchFramework;
	bool bInstalledMoviePlayerMatchFramework = false;
	if ( !g_pMatchFramework && IsGameConsole() )
	{
#ifdef _X360
		XOnlineStartup();
#endif
		g_pMatchFramework = g_pMoviePlayer_MatchFramework;
		bInstalledMoviePlayerMatchFramework = true;
	}
#ifdef _PS3
	int iLibAudioInitCode = cellAudioInit();
	CellAudioOutState caosDevice = {0};
	if ( iLibAudioInitCode >= 0 )
	{
		int numDevices = cellAudioOutGetNumberOfDevice( CELL_AUDIO_OUT_PRIMARY );
		if ( numDevices > 0 )
		{
			int iAudioState = cellAudioOutGetState( CELL_AUDIO_OUT_PRIMARY, 0, &caosDevice );
			if ( g_pBIK && ( iAudioState >= 0 ) )
			{
				g_pBIK->SetPS3SoundDevice( caosDevice.soundMode.channel );
			}
		}
	}
#endif
#endif

	characterset_t breakSet;
	CharacterSetBuild( &breakSet, "" );
	char moviePath[MAX_PATH];
	while ( !IsPS3QuitRequested() )
	{
		int nTokenSize = vidBuffer.ParseToken( &breakSet, moviePath, sizeof( moviePath ) );
		if ( nTokenSize <= 0 )
		{
			break;
		}

		// get the path to the file and play it.
		PlayVideoAndWait( moviePath, bNeedHealthWarning );
	}

#ifdef _GAMECONSOLE
#ifdef _PS3
	if ( iLibAudioInitCode >= 0 )
	{
		cellAudioQuit();
	}
#endif
	if ( ( g_pMatchFramework == g_pMoviePlayer_MatchFramework ) && bInstalledMoviePlayerMatchFramework )
	{
#ifdef _X360
		XOnlineCleanup();
#endif
		g_pMatchFramework = NULL;
	}
#endif

#ifdef _X360
	// TCR024
	XMPRestoreBackgroundMusic();
#endif

#if defined( USE_SDL )
	SDL_ShowCursor( CursorStateBak );
#elif defined( WIN32 )
	// show cursor again
	::ShowCursor(TRUE);
#endif
#ifdef OSX
    CGDisplayShowCursor( kCGDirectMainDisplay );
#endif
#endif // DEDICATED
}

//-----------------------------------------------------------------------------
// Plays a Bink video until the video completes or user input cancels
//-----------------------------------------------------------------------------
void CGame::PlayVideoAndWait( const char *filename, bool bNeedHealthWarning )
{
#if defined( BINK_VIDEO )

#if defined( IS_WINDOWS_PC ) || defined( OSX ) || defined( _GAMECONSOLE )
	if ( !filename || !filename[0] )
		return;

	if ( !g_pBIK )
		return;

#if defined( _X360 ) && defined( _DEMO )
	// Xbox 360 is required to show ratings from the locale specific ratings board
	if ( char const *pszRating = Q_stristr( filename, "RATINGBOARD" ) )
	{
		// Determine the rating of the current locale
		char const *szRatingBoard = GetConsoleLocaleRatingsBoard();
		if ( !szRatingBoard || !*szRatingBoard )
			return;
		
		// Format it into the buffer
		int nRatingPrefixLen = ( pszRating - filename );
		int numBufferBytes = nRatingPrefixLen + Q_strlen( szRatingBoard ) + 32;
		char *pchRatingBuffer = ( char * ) stackalloc( numBufferBytes );
		Q_snprintf( pchRatingBuffer, numBufferBytes, "%.*s%s.bik", nRatingPrefixLen, filename, szRatingBoard );
		filename = pchRatingBuffer;	// stackalloc ensures that the buffer is valid until the function returns
	}
#else
	if ( Q_stristr( filename, "RATINGBOARD" ) )
		return;
#endif

	// Supplying a NULL context will cause Bink to allocate its own
	// FIXME: At this point we're playing at the full volume of the computer, NOT the user's set volume in the game!
#if defined( _X360 ) 
	if ( Audio_CreateXAudioDevice( false ) )
	{
    #if defined ( BINK_VIDEO )
		if ( !g_pBIK->HookXAudio() )
			return;
	#endif
	}
#elif defined( LINUX )
	Audio_CreateSDLAudioDevice();
#elif defined( _PS3 )
	// S_Init(); // fully initialize sound system here
#elif defined( PLATFORM_WINDOWS )
	//BinkSoundUseDirectSound( NULL );	// Bink sound is initialized by the caller now
#endif

 	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

#if defined ( QUICKTIME_VIDEO )
	IQuickTime *pVideoPlayer = g_pQuickTime;
	QUICKTIMEMaterial_t VideoHandle;
	QUICKTIMEMaterial_t InvalidVideoHandle = QUICKTIMEMATERIAL_INVALID;
#elif defined( BINK_VIDEO )
	IBik *pVideoPlayer = g_pBIK;
	BIKHandle_t	VideoHandle;
	BIKHandle_t InvalidVideoHandle = BIKHANDLE_INVALID;
#else
  #error "Need to have support for video playback enabled via source_video_base.vpc"
#endif
	
	if ( !pVideoPlayer )
		return;

	// get the path to the media file and play it.
	char localPath[MAX_PATH];
	
	// Are we wanting to use a quicktime ".mov" version of the media instead of what's specified?
#if defined( FORCE_QUICKTIME ) && defined( QUICKTIME_VIDEO )
	// is it not a .mov file extension?
	if ( V_stristr( com_token, ".mov") == NULL )
	{
		// Compose Quicktime version
		char QTPath[MAX_PATH];
		V_strncpy( QTPath, filename, MAX_PATH );
		V_SetExtension( QTPath, ".mov", MAX_PATH );
		
		g_pFileSystem->GetLocalPath( QTPath, localPath, sizeof(localPath) );
	}
	else
#endif 
	{
		V_strncpy( localPath, filename, sizeof(localPath) );
	}
	
	// Load and create our BINK or QuickTime video
	VideoHandle = pVideoPlayer->CreateMaterial( "VideoMaterial", localPath, "GAME" );
	if ( VideoHandle == InvalidVideoHandle )
	{
		return;
	}

	float flU0 = 0.0f;
	float flV0 = 0.0f;
	float flU1, flV1;
	pVideoPlayer->GetTexCoordRange( VideoHandle, &flU1, &flV1 );

	IMaterial *pMaterial = pVideoPlayer->GetMaterial( VideoHandle );

	int nTexHeight = pMaterial->GetMappingHeight();
	int nTexWidth = pMaterial->GetMappingWidth();

	int nWidth, nHeight;
	pVideoPlayer->GetFrameSize( VideoHandle, &nWidth, &nHeight );

	const AspectRatioInfo_t &aspectRatioInfo = materials->GetAspectRatioInfo();

	// Determine how the video's aspect ratio relates to the screen's
	float flPhysicalFrameRatio = aspectRatioInfo.m_flFrameBuffertoPhysicalScalar * ( ( float )m_width / ( float )m_height );
	float flVideoRatio = ( ( float ) nWidth / ( float ) nHeight );

	int nPlaybackWidth;
	int nPlaybackHeight;
	
	if ( flVideoRatio > flPhysicalFrameRatio )
	{
		// Height must be adjusted
		nPlaybackWidth = m_width;
		// Have to account for the difference between physical and pixel aspect ratios.
		nPlaybackHeight = ( ( float )m_width / aspectRatioInfo.m_flPhysicalToFrameBufferScalar ) / flVideoRatio;
	}
	else if ( flVideoRatio < flPhysicalFrameRatio )
	{
		// Width must be adjusted
		// Have to account for the difference between physical and pixel aspect ratios.
		nPlaybackWidth = ( float )m_height * flVideoRatio * aspectRatioInfo.m_flPhysicalToFrameBufferScalar;
		nPlaybackHeight = m_height;
	}
	else
	{
		// Ratio matches
		nPlaybackWidth = m_width;
		nPlaybackHeight = m_height;
	}

	// Turn off our vertex alpha for these draw calls as they don't write alpha per-vertex
	pMaterial->SetMaterialVarFlag( MATERIAL_VAR_VERTEXALPHA, false );

	// Prep the screen
	pRenderContext->Viewport( 0, 0, m_width, m_height );
	pRenderContext->DepthRange( 0, 1 );
	pRenderContext->ClearColor3ub( 0, 0, 0 );
	pRenderContext->SetToneMappingScaleLinear( Vector(1,1,1) );
	
	// Find our letterboxing offset
	int xpos = ( (float) ( m_width - nPlaybackWidth ) / 2 );
	int ypos = ( (float) ( m_height - nPlaybackHeight ) / 2 );

	// Enable the input system's message pump
	g_pInputSystem->EnableMessagePump( true );

	// Panel which allows for subtitling of startup movies
	CSubtitlePanel *pSubtitlePanel = NULL;

	bool bUseCaptioning = ShouldUseCaptioning();
	if ( bUseCaptioning )
	{
		// Create a panel whose purpose is to 
		pSubtitlePanel = new CSubtitlePanel( NULL, filename, nPlaybackHeight );
		pSubtitlePanel->SetParent( g_pMatSystemSurface->GetEmbeddedPanel() );
		pSubtitlePanel->SetPaintBackgroundEnabled( false );
		pSubtitlePanel->SetPaintEnabled( true );
		pSubtitlePanel->SetBounds( 0, 0, m_width, m_height );
		
		// VGUI needs a chance to move this panel into its global space
		vgui::ivgui()->RunFrame();

		// Start the caption sequence
		pSubtitlePanel->StartCaptions();
	}

	// We need to make sure that these keys have been released since last pressed, otherwise you can skip
	// movies inadvertently 
	bool bKeyDebounced = ( UserRequestingMovieSkip() == false );
	bool bExitingProcess = false;
#if defined( _DEMO ) && defined( _X360 )
	bExitingProcess = Host_IsDemoExiting();
#endif

	while ( 1 )
	{
#ifdef _GAMECONSOLE
		if ( !bExitingProcess )
		{
			XBX_ProcessEvents();		// Force events to be processed that will deliver us ingame invites
			XBX_DispatchEventsQueue();	// Dispatch the events too
		}
#endif

		// Pump messages to avoid lockups on focus change
		g_pInputSystem->PollInputState( GetBaseLocalClient().IsActive() );
		game->DispatchAllStoredGameMessages();

		// xbox cannot skip legals
		if ( bKeyDebounced && ( IsPC() || ( IsGameConsole() && !Q_stristr( filename, "valve" ) ) ) )
		{
			if ( !bExitingProcess && UserRequestingMovieSkip() )
				break;
		}
		else
		{
			bKeyDebounced = ( UserRequestingMovieSkip() == false );
		}

		// Update our frame
		if ( pVideoPlayer->Update( VideoHandle ) == false )
			break;
		
		if( IsPS3QuitRequested() )
			break;

		pRenderContext->AntiAliasingHint( AA_HINT_MOVIE );

		// Clear the draw buffer and blt the material to it
		pRenderContext->ClearBuffers( true, true, true );
		pRenderContext->DrawScreenSpaceRectangle( pMaterial, xpos, ypos, nPlaybackWidth, nPlaybackHeight, flU0*nTexWidth, flV0*nTexHeight, flU1*nTexWidth-1, flV1*nTexHeight-1, nTexWidth, nTexHeight );

		// Draw our VGUI panel
		if ( bUseCaptioning )
		{
			vgui::surface()->PaintTraverse( pSubtitlePanel->GetVPanel() );
		}
				
		// Busy wait until we are ready to swap.
#ifdef QUICKTIME_VIDEO
		while ( !pVideoPlayer->ReadyForSwap( BIKHandle ) )
#else
		// TODO - is this valid with threaded bink changes?: while ( !pVideoPlayer->ReadyForSwap( BIKHandle ) )
#endif
		{
			NULL;
		}

		g_pMaterialSystem->SwapBuffers();
		
		if ( ENABLE_BIK_PERF_SPEW )
		{
			// timing debug code for bink playback
			static double flPreviousTime = -1.0;
			double flTime = Plat_FloatTime();
			double flDeltaTime = flTime - flPreviousTime;
			if ( flDeltaTime > 0.0 )
			{
				Warning( "%0.2lf sec*60 %0.2lf fps\n", flDeltaTime * 60.0, 1.0 / flDeltaTime );
			}
			flPreviousTime = flTime;
		}
	}

	// Disable the input system's message pump
	g_pInputSystem->EnableMessagePump( false );

	// Clean up the Bink video
	if ( VideoHandle != InvalidVideoHandle )
	{
		pVideoPlayer->DestroyMaterial( VideoHandle );
	}

	// Clean up VGUI work
	delete pSubtitlePanel;
#endif

#endif // BINK_VIDEO
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CGame::CGame()
{
#ifndef LINUX
	m_hWindow = 0;
#endif
	m_x = m_y = 0;
	m_width = m_height = 0;
	m_bActiveApp = false;
	m_bCanPostActivateEvents = true;
	m_iDesktopWidth = 0;
	m_iDesktopHeight = 0;
	m_iDesktopRefreshRate = 0;
	m_hInputContext = INPUT_CONTEXT_HANDLE_INVALID;
#if defined( WIN32 ) && !defined( USE_SDL )
	m_hInstance = 0;
	m_ChainedWindowProc = NULL;
#endif

}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGame::~CGame()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CGame::Init( void *pvInstance )
{
	m_bExternallySuppliedWindow = false;

#if defined( WIN32 ) && !defined( USE_SDL )
	OSVERSIONINFO	vinfo;
	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if ( !GetVersionEx( &vinfo ) )
	{
		return false;
	}

	if ( vinfo.dwPlatformId == VER_PLATFORM_WIN32s )
	{
		return false;
	}

	m_hInstance = (HINSTANCE)pvInstance;
#endif

	m_hInputContext = g_pInputStackSystem->PushInputContext();

	// Capture + hide the mouse
	g_pInputStackSystem->SetMouseCapture( m_hInputContext, true );

	return true;
}


bool CGame::Shutdown( void )
{
	if ( m_hInputContext != INPUT_CONTEXT_HANDLE_INVALID )
	{
		g_pInputStackSystem->PopInputContext();
		m_hInputContext = INPUT_CONTEXT_HANDLE_INVALID;
	}

#if defined( WIN32 ) && !defined( USE_SDL )
	m_hInstance = 0;
#endif

#ifdef _PS3
	AbortLoadingUpdatesDueToShutdown();
#endif

	return true;
}

void *CGame::GetMainWindow( void )
{
#if defined( LINUX )
	return 0;
#else
	return (void*)m_hWindow;
#endif
}

#if defined(USE_SDL)
void** CGame::GetMainWindowAddress( void )
{
	m_hWindow = (SDL_Window *)g_pLauncherMgr->GetWindowRef();
	return (void**)&m_hWindow;
}
#elif defined( WIN32 ) 
void** CGame::GetMainWindowAddress( void )
{
	return (void**)&m_hWindow;
}
#elif defined(OSX)
void** CGame::GetMainWindowAddress( void )
{
	m_hWindow = (WindowRef)g_pLauncherMgr->GetWindowRef();
	return (void**)&m_hWindow;
}
#else
#error
#endif

void CGame::GetDesktopInfo( int &width, int &height, int &refreshrate )
{
#if defined(USE_SDL)

	width = 640;
	height = 480;
	refreshrate = 0;

	// Go through all the displays and return the size of the largest.
	for( int i = 0; i < SDL_GetNumVideoDisplays(); i++ )
	{
		SDL_Rect rect;

		if ( !SDL_GetDisplayBounds( i, &rect ) )
		{
			if ( ( rect.w > width ) || ( ( rect.w == width ) && ( rect.h > height ) ) )
			{
				width = rect.w;
				height = rect.h;
			}
		}
	}

#elif defined( WIN32 )
	// order of initialization means that this might get called early.  In that case go ahead and grab the current
	// screen window and setup based on that.
	// we need to do this when initializing the base list of video modes, for example
	if ( m_iDesktopWidth == 0 )
	{
		HDC dc = ::GetDC( NULL );
		width = ::GetDeviceCaps(dc, HORZRES);
		height = ::GetDeviceCaps(dc, VERTRES);
		refreshrate = ::GetDeviceCaps(dc, VREFRESH);
		::ReleaseDC( NULL, dc );
		return;
	}
	width = m_iDesktopWidth;
	height = m_iDesktopHeight;
	refreshrate = m_iDesktopRefreshRate;
#elif defined(OSX)
	if ( m_iDesktopWidth == 0 )
			{
		CGDirectDisplayID mainDisplay = CGMainDisplayID();
		CGDisplayModeRef displayMode = CGDisplayCopyDisplayMode(mainDisplay);
		width = (int)CGDisplayModeGetWidth(displayMode);
		height = (int)CGDisplayModeGetHeight(displayMode);
		refreshrate = (int)CGDisplayModeGetRefreshRate(displayMode);
	}
	width = m_iDesktopWidth;
	height = m_iDesktopHeight;
	refreshrate = m_iDesktopRefreshRate;
#else
#error
#endif
}

void CGame::UpdateDesktopInformation( HWND hWnd )
{
#if defined(USE_SDL)
	// Get the size of the display we will be displayed fullscreen on.
	static ConVarRef sdl_displayindex( "sdl_displayindex" );
	int displayIndex = sdl_displayindex.IsValid() ? sdl_displayindex.GetInt() : 0;

	SDL_DisplayMode mode;
	SDL_GetDesktopDisplayMode( displayIndex, &mode );

	m_iDesktopWidth = mode.w;
	m_iDesktopHeight = mode.h;
	m_iDesktopRefreshRate = mode.refresh_rate;
#elif defined( WIN32 ) 
	HDC dc = ::GetDC( hWnd );
	m_iDesktopWidth = ::GetDeviceCaps(dc, HORZRES);
	m_iDesktopHeight = ::GetDeviceCaps(dc, VERTRES);
	m_iDesktopRefreshRate = ::GetDeviceCaps(dc, VREFRESH);
	::ReleaseDC( hWnd, dc );
#elif defined(OSX)
	CGDirectDisplayID mainDisplay = CGMainDisplayID();
	CGDisplayModeRef displayMode = CGDisplayCopyDisplayMode(mainDisplay);
	m_iDesktopWidth = (int)CGDisplayModeGetWidth(displayMode);
	m_iDesktopHeight = (int)CGDisplayModeGetHeight(displayMode);;
	m_iDesktopRefreshRate = (int)CGDisplayModeGetRefreshRate(displayMode);
#else
#error
#endif
}

#ifdef WIN32
void CGame::UpdateDesktopInformation( WPARAM wParam, LPARAM lParam )
{
	m_iDesktopWidth = LOWORD( lParam );
	m_iDesktopHeight = HIWORD( lParam );
}
#endif

void CGame::SetMainWindow( HWND window )
{
#if defined( USE_SDL )
	m_hWindow = (SDL_Window*)window;
#elif defined( WIN32 ) && !defined( USE_SDL )
	m_hWindow = window;
#elif OSX
	m_hWindow = (WindowRef)window;
#else
#error
#endif

	if ( IsPC() && !IsPosix() )
	{
		avi->SetMainWindow( (void*)window );
	}

	// update our desktop info (since the results will change if we are going to fullscreen mode)
	if ( !m_iDesktopWidth || !m_iDesktopHeight )
	{
		UpdateDesktopInformation( window );
	}
}

void CGame::SetWindowXY( int x, int y )
{
	m_x = x;
	m_y = y;
}

void CGame::SetWindowSize( int w, int h )
{
	m_width = w;
	m_height = h;
}

void CGame::GetWindowRect( int *x, int *y, int *w, int *h )
{
	if ( x )
	{
		*x = m_x;
	}
	if ( y )
	{
		*y = m_y;
	}
	if ( w )
	{
		*w = m_width;
	}
	if ( h )
	{
		*h = m_height;
	}
}

bool CGame::IsActiveApp( void )
{
	return m_bActiveApp;
}

void CGame::SetCanPostActivateEvents( bool bEnabled )
{
	m_bCanPostActivateEvents = bEnabled;
}

bool CGame::CanPostActivateEvents()
{
	return m_bCanPostActivateEvents;
}

void CGame::SetActiveApp( bool active )
{
	m_bActiveApp = active;
}

void CGame::OnScreenSizeChanged( int nOldWidth, int nOldHeight )
{
	if ( g_ClientDLL )
	{
		g_ClientDLL->OnScreenSizeChanged( nOldWidth, nOldHeight );
	}
}

