//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "inputsystem.h"
#include "key_translation.h"
#include "inputsystem/ButtonCode.h"
#include "inputsystem/AnalogCode.h"
#include "tier0/etwprof.h"
#include "tier1/convar.h"
#include "filesystem.h"
#include "platforminputdevice.h"

#ifdef _PS3
#include <vjobs_interface.h>
#endif

#ifdef PLATFORM_OSX
#include <Carbon/Carbon.h>
#include "materialsystem/imaterialsystem.h"
#endif

#if defined( INCLUDE_SCALEFORM )
#include "scaleformui/scaleformui.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

#if defined( INCLUDE_SCALEFORM )
IScaleformUI* g_pScaleformUI = NULL;
#endif

#if defined( USE_SDL )
#include "SDL.h"
static void initKeymap(void);
#endif

ConVar joy_xcontroller_found( "joy_xcontroller_found", "1", FCVAR_NONE, "Automatically set to 1 if an xcontroller has been detected." );
ConVar joy_deadzone_mode( "joy_deadzone_mode", "0", FCVAR_NONE, "0 => Cross-shaped deadzone (default), 1 => Square deadzone." );
ConVar pc_fake_controller( "pc_fake_controller", "0", FCVAR_DEVELOPMENTONLY, "" );
ConVar dev_force_selected_device( "dev_force_selected_device", "0", FCVAR_DEVELOPMENTONLY, "" );

//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CInputSystem g_InputSystem;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CInputSystem, IInputSystem, 
						INPUTSYSTEM_INTERFACE_VERSION, g_InputSystem );

#ifdef _PS3
IVJobs * g_pVJobs = NULL;
#endif



#if defined( WIN32 ) && !defined( _X360 )
typedef BOOL (WINAPI *RegisterRawInputDevices_t)
(
	PCRAWINPUTDEVICE pRawInputDevices,
	UINT uiNumDevices,
	UINT cbSize
);

typedef UINT (WINAPI *GetRawInputData_t)
(
	HRAWINPUT hRawInput,
	UINT uiCommand,
	LPVOID pData,
	PUINT pcbSize,
	UINT cbSizeHeader
);

RegisterRawInputDevices_t pfnRegisterRawInputDevices;
GetRawInputData_t pfnGetRawInputData;
#endif



extern int countBits( uint32 iValue );

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CInputSystem::CInputSystem()
{
	m_nLastPollTick = m_nLastSampleTick = m_StartupTimeTick = 0;
	m_ChainedWndProc = 0;
	m_hAttachedHWnd = 0;
	m_hEvent = NULL;
	m_bEnabled = true;
	m_bPumpEnabled = true;
	m_bIsPolling = false;
	m_bIsInGame = false;
	m_JoysticksEnabled.ClearAllFlags();
	m_nJoystickCount = 0;
	m_nJoystickBaseline = 0;
	m_nPollCount = 0;
	m_uiMouseWheel = 0;
	m_bXController = false;
	m_bRawInputSupported = false;
	m_bIMEComposing = false;
	m_nUIEventClientCount = 0;
	m_hLastIMEHWnd = NULL;
	m_hCurrentCaptureWnd = PLAT_WINDOW_INVALID;
	m_bCursorVisible = true;
	m_hCursor = INPUT_CURSOR_HANDLE_INVALID;
	m_bMotionControllerActive = false;
	m_qMotionControllerOrientation.Init();
	m_fMotionControllerPosX = 0.0f;
	m_fMotionControllerPosY = 0.0f;
	m_nMotionControllerStatus = INPUT_DEVICE_MC_STATE_CAMERA_NOT_CONNECTED;
	m_nMotionControllerStatusFlags = 0;

	// This is B.S., must be a compile-time assert with valid expression:
	// Assert( (MAX_JOYSTICKS + 7) >> 3 << sizeof(unsigned short) ); 

#if !defined( _CERT ) && !defined(LINUX)
	V_memset( m_press_x360_buttons, 0, sizeof( m_press_x360_buttons ) );
#endif

#ifdef _PS3
	m_pPS3CellNoPadDataHook = NULL;
	m_pPS3CellPadDataHook = NULL;
	m_PS3KeyboardConnected = false;
	m_PS3MouseConnected = false;
#endif

	m_pXInputDLL = NULL;
	m_pRawInputDLL = NULL;

	for ( int i = 0; i < Q_ARRAYSIZE(m_nControllerType); i++)
	{
		m_nControllerType[i] = INPUT_TYPE_GENERIC_JOYSTICK;
	}

	InitPlatfromInputDeviceInfo();
}

#if defined( USE_SDL ) 

void CInputSystem::DisableHardwareCursor(  )
{
	m_pLauncherMgr->SetMouseVisible(false);
}

void CInputSystem::EnableHardwareCursor( )
{
	m_pLauncherMgr->SetMouseVisible(true);

}
#endif

CInputSystem::~CInputSystem()
{
	if ( m_pXInputDLL )
	{
		Sys_UnloadModule( m_pXInputDLL );
		m_pXInputDLL = NULL;
	}

	if ( m_pRawInputDLL )
	{
		Sys_UnloadModule( m_pRawInputDLL );
		m_pRawInputDLL = NULL;
	}

}


//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
InitReturnVal_t CInputSystem::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	m_StartupTimeTick = Plat_MSTime();

#if !defined( PLATFORM_POSIX )
	if ( IsPC() )
	{
		m_uiMouseWheel = RegisterWindowMessage( "MSWHEEL_ROLLMSG" );
	}

	m_hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	if ( !m_hEvent )
		return INIT_FAILED;
#endif


	ButtonCode_InitKeyTranslationTable();
	ButtonCode_UpdateScanCodeLayout();

	joy_xcontroller_found.SetValue( 0 );

#if !defined( _GAMECONSOLE )
	if ( IsPC() )
	{
#if !defined( PLATFORM_POSIX )
		m_pXInputDLL = Sys_LoadModule( "XInput1_3.dll" );
		if ( m_pXInputDLL )
		{
			InitializeXDevices();
		}
#endif
		if ( !m_nJoystickCount )
		{
			// Didn't find any XControllers. See if we can find other joysticks.
			InitializeJoysticks();
		}
		else
		{
			m_bXController = true;
		}

		if ( m_bXController )
			joy_xcontroller_found.SetValue( 1 );


	}
#elif defined( _GAMECONSOLE )
	if ( IsGameConsole() )
	{
		InitializeXDevices();
		m_bXController = true;
		joy_xcontroller_found.SetValue( 1 );
	}
#endif

	InitCursors();

	m_bRawInputSupported = false;

#if defined( LINUX )

	m_bRawInputSupported = true;
	
#elif defined( WIN32 ) && !defined( _X360 )
	// Check if this version of windows supports raw mouse input (later than win2k)

	CSysModule *m_pRawInputDLL = Sys_LoadModule( "USER32.dll" );
	if ( m_pRawInputDLL )
	{
		pfnRegisterRawInputDevices = (RegisterRawInputDevices_t)GetProcAddress( (HMODULE)m_pRawInputDLL, "RegisterRawInputDevices" );
		pfnGetRawInputData = (GetRawInputData_t)GetProcAddress( (HMODULE)m_pRawInputDLL, "GetRawInputData" );
		if ( pfnRegisterRawInputDevices && pfnGetRawInputData )
			m_bRawInputSupported = true;
	}
#endif

#if defined( USE_SDL )
	initKeymap();
#endif

    m_unNumSteamControllerConnected = 0;
    m_bSteamController = InitializeSteamControllers();

	return INIT_OK; 
}

bool CInputSystem::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

#if defined( INCLUDE_SCALEFORM )
	g_pScaleformUI = (IScaleformUI*)factory( SCALEFORMUI_INTERFACE_VERSION, NULL );
#endif
#ifdef _PS3
	g_pVJobs = ( IVJobs* )factory( VJOBS_INTERFACE_VERSION, NULL );
#endif

#if defined( USE_SDL )
	m_pLauncherMgr = (ILauncherMgr *)factory(  SDLMGR_INTERFACE_VERSION, NULL );
#elif defined( OSX )
	m_pLauncherMgr = (ILauncherMgr *)factory(  COCOAMGR_INTERFACE_VERSION, NULL );
#endif

return true;
}

#ifdef _PS3
extern void PS3_XInputShutdown();
#endif

#ifdef _PS3
void CInputSystem::SetPS3CellPadDataHook( BCellPadDataHook_t hookFunc )
{
	m_pPS3CellPadDataHook = hookFunc;
}
void CInputSystem::SetPS3CellPadNoDataHook( BCellPadNoDataHook_t hookFunc )
{
	m_pPS3CellNoPadDataHook = hookFunc;
}
#endif



//-----------------------------------------------------------------------------
// Shutdown
//-----------------------------------------------------------------------------
void CInputSystem::Shutdown()
{
#if !defined( PLATFORM_POSIX )
	if ( m_hEvent != NULL )
	{
		CloseHandle( m_hEvent );
		m_hEvent = NULL;
	}
#endif

	ShutdownCursors();

	BaseClass::Shutdown();

#ifdef _PS3
	PS3_XInputShutdown();
#endif
}


//-----------------------------------------------------------------------------
// Sleep until input
//-----------------------------------------------------------------------------
void CInputSystem::SleepUntilInput( int nMaxSleepTimeMS )
{
#if defined( USE_SDL ) || defined( OSX )
	m_pLauncherMgr->WaitUntilUserInput( nMaxSleepTimeMS );
#elif defined( _WIN32 ) 
	if ( nMaxSleepTimeMS < 0 )
	{
		nMaxSleepTimeMS = INFINITE;
	}

	MsgWaitForMultipleObjects( 1, &m_hEvent, FALSE, nMaxSleepTimeMS, QS_ALLEVENTS );
#elif defined( _PS3 )
	// no-op
#else
#warning "need a SleepUntilInput impl"
#endif
}


//-----------------------------------------------------------------------------
// Tells the input system to generate UI-related events, defined
//-----------------------------------------------------------------------------
void CInputSystem::AddUIEventListener()
{
	++m_nUIEventClientCount;
}

void CInputSystem::RemoveUIEventListener()
{
	--m_nUIEventClientCount;
}


//-----------------------------------------------------------------------------
// Returns the currently attached window
//-----------------------------------------------------------------------------
PlatWindow_t CInputSystem::GetAttachedWindow() const
{
	return (PlatWindow_t)m_hAttachedHWnd;
}


//-----------------------------------------------------------------------------
// Callback to call into our class
//-----------------------------------------------------------------------------
#if !defined( PLATFORM_POSIX )
static LRESULT CALLBACK InputSystemWindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return g_InputSystem.WindowProc( hwnd, uMsg, wParam, lParam );
}
#endif


//-----------------------------------------------------------------------------
// Hooks input listening up to a window
//-----------------------------------------------------------------------------
void CInputSystem::AttachToWindow( void* hWnd )
{
	Assert( m_hAttachedHWnd == 0 );
	if ( m_hAttachedHWnd )
	{
		Warning( "CInputSystem::AttachToWindow: Cannot attach to two windows at once!\n" );
		return;
	}

#if defined ( USE_SDL )
#elif defined( PLATFORM_OSX )
#elif defined( PLATFORM_WINDOWS )
#if defined( PLATFORM_X360 ) //GetWindowLongPtrW/SetWindowLongPtrW don't exist on the 360
	m_ChainedWndProc = (WNDPROC)GetWindowLongPtr( (HWND)hWnd, GWLP_WNDPROC );
	SetWindowLongPtr( (HWND)hWnd, GWLP_WNDPROC, (LONG_PTR)InputSystemWindowProc );
#else

	m_ChainedWndProc = (WNDPROC)GetWindowLongPtrW( (HWND)hWnd, GWLP_WNDPROC );
	SetWindowLongPtrW( (HWND)hWnd, GWLP_WNDPROC, (LONG_PTR)InputSystemWindowProc );
	
	// register to read raw mouse input
#if !defined(HID_USAGE_PAGE_GENERIC)
#define HID_USAGE_PAGE_GENERIC         ((USHORT) 0x01)
#endif
#if !defined(HID_USAGE_GENERIC_MOUSE)
#define HID_USAGE_GENERIC_MOUSE        ((USHORT) 0x02)
#endif

	RAWINPUTDEVICE Rid[1];
	Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC; 
	Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE; 
	Rid[0].dwFlags = RIDEV_INPUTSINK;   
	Rid[0].hwndTarget = (HWND)hWnd; // g_InputSystem.m_hAttachedHWnd; // GetHhWnd;
	::RegisterRawInputDevices(Rid, ARRAYSIZE(Rid), sizeof(Rid[0]));
	
#endif
#elif defined( _PS3 )
#else
#error
#endif

	m_hAttachedHWnd = (HWND)hWnd;

#if defined( WIN32 ) && !defined( _X360 )
	// register to read raw mouse input

#if !defined(HID_USAGE_PAGE_GENERIC)
#define HID_USAGE_PAGE_GENERIC         ((USHORT) 0x01)
#endif
#if !defined(HID_USAGE_GENERIC_MOUSE)
#define HID_USAGE_GENERIC_MOUSE        ((USHORT) 0x02)
#endif

	if ( m_bRawInputSupported )
	{
		RAWINPUTDEVICE Rid[1];
		Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC; 
		Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE; 
		Rid[0].dwFlags = RIDEV_INPUTSINK;   
		Rid[0].hwndTarget = g_InputSystem.m_hAttachedHWnd; // GetHhWnd;
		pfnRegisterRawInputDevices(Rid, ARRAYSIZE(Rid), sizeof(Rid[0]));
	}
#endif

	// New window, clear input state
	ClearInputState( true );
}


//-----------------------------------------------------------------------------
// Unhooks input listening from a window
//-----------------------------------------------------------------------------
void CInputSystem::DetachFromWindow( )
{
	if ( !m_hAttachedHWnd )
		return;

	ResetInputState();

#if !defined( PLATFORM_POSIX )
	if ( m_ChainedWndProc )
	{
		SetWindowLongPtrW( m_hAttachedHWnd, GWLP_WNDPROC, (LONG_PTR)m_ChainedWndProc );
		m_ChainedWndProc = 0;
	}
#endif

	m_hAttachedHWnd = 0;
}


//-----------------------------------------------------------------------------
// Enables/disables input
//-----------------------------------------------------------------------------
void CInputSystem::EnableInput( bool bEnable )
{
	m_bEnabled = bEnable;
}


//-----------------------------------------------------------------------------
// Enables/disables the inputsystem windows message pump
//-----------------------------------------------------------------------------
void CInputSystem::EnableMessagePump( bool bEnable )
{
	m_bPumpEnabled = bEnable;
}
	

//-----------------------------------------------------------------------------
// Clears the input state, doesn't generate key-up messages
//-----------------------------------------------------------------------------
void CInputSystem::ClearInputState( bool bPurgeState )
{
	for ( int i = 0; i < INPUT_STATE_COUNT; ++i )
	{
		InputState_t& state = m_InputState[i];
		state.m_ButtonState.ClearAll();
		memset( state.m_pAnalogDelta, 0, ANALOG_CODE_LAST * sizeof(int) );
		memset( state.m_pAnalogValue, 0, ANALOG_CODE_LAST * sizeof(int) );
		memset( state.m_ButtonPressedTick, 0, BUTTON_CODE_LAST * sizeof(int) );
		memset( state.m_ButtonReleasedTick, 0, BUTTON_CODE_LAST * sizeof(int) );
		if ( bPurgeState )
		{
			state.m_Events.Purge();
			state.m_bDirty = false;
		}
	}
	memset( m_appXKeys, 0, XUSER_MAX_COUNT * XK_MAX_KEYS * sizeof(appKey_t) );
	m_mouseRawAccumX = m_mouseRawAccumY = 0;
	m_flLastControllerPollTime = 0;
}

//-----------------------------------------------------------------------------
// Resets the input state
//-----------------------------------------------------------------------------
void CInputSystem::ResetInputState()
{
	ReleaseAllButtons();
	ZeroAnalogState( 0, ANALOG_CODE_LAST - 1 );
	ClearInputState( false );
}


//-----------------------------------------------------------------------------
// Convert back + forth between ButtonCode/AnalogCode + strings
//-----------------------------------------------------------------------------
const char *CInputSystem::ButtonCodeToString( ButtonCode_t code ) const
{
	return ButtonCode_ButtonCodeToString( code, m_bXController );
}

const char *CInputSystem::AnalogCodeToString( AnalogCode_t code ) const
{
	return AnalogCode_AnalogCodeToString( code );
}

ButtonCode_t CInputSystem::StringToButtonCode( const char *pString ) const
{
	return ButtonCode_StringToButtonCode( pString, true );
}

AnalogCode_t CInputSystem::StringToAnalogCode( const char *pString ) const
{
	return AnalogCode_StringToAnalogCode( pString );
}


//-----------------------------------------------------------------------------
// Convert back + forth between virtual codes + button codes
// FIXME: This is a temporary piece of code
//-----------------------------------------------------------------------------
ButtonCode_t CInputSystem::VirtualKeyToButtonCode( int nVirtualKey ) const
{
	return ButtonCode_VirtualKeyToButtonCode( nVirtualKey );
}

int CInputSystem::ButtonCodeToVirtualKey( ButtonCode_t code ) const
{
	return ButtonCode_ButtonCodeToVirtualKey( code );
}

ButtonCode_t CInputSystem::XKeyToButtonCode( int nPort, int nXKey ) const
{
	if ( m_bXController )
		return ButtonCode_XKeyToButtonCode( nPort, nXKey );
	return KEY_NONE;
}

ButtonCode_t CInputSystem::ScanCodeToButtonCode( int lParam ) const
{
	return ButtonCode_ScanCodeToButtonCode( lParam );
}

ButtonCode_t CInputSystem::SKeyToButtonCode( int nPort, int nXKey ) const
{
	return ButtonCode_SKeyToButtonCode( nPort, nXKey );
}

//-----------------------------------------------------------------------------
// Post an event to the queue
//-----------------------------------------------------------------------------
void CInputSystem::PostEvent( int nType, int nTick, int nData, int nData2, int nData3 )
{
	InputState_t &state = m_InputState[ m_bIsPolling ];
	int i = state.m_Events.AddToTail();
	InputEvent_t &event = state.m_Events[i];
	event.m_nType = nType;
	event.m_nTick = nTick;
	event.m_nData = nData;
	event.m_nData2 = nData2;
	event.m_nData3 = nData3;
	state.m_bDirty = true;
}


//-----------------------------------------------------------------------------
// Post an button press event to the queue
//-----------------------------------------------------------------------------
void CInputSystem::PostButtonPressedEvent( InputEventType_t nType, int nTick, ButtonCode_t scanCode, ButtonCode_t virtualCode )
{
	InputState_t &state = m_InputState[ m_bIsPolling ];
	if ( !state.m_ButtonState.IsBitSet( scanCode ) )
	{
		// Update button state
		state.m_ButtonState.Set( scanCode ); 
		state.m_ButtonPressedTick[ scanCode ] = nTick;

		// Add this event to the app-visible event queue
		PostEvent( nType, nTick, scanCode, virtualCode );

		if ( IsGameConsole() && ShouldGenerateUIEvents() && IsJoystickCode( scanCode ) )
		{
			// xboxissue - as yet input hasn't been made aware of analog inputs or ports
			// so just digital produce a key typed message
			PostEvent( IE_KeyCodeTyped, nTick, scanCode );
		}
	}
}


//-----------------------------------------------------------------------------
// Post an button release event to the queue
//-----------------------------------------------------------------------------
void CInputSystem::PostButtonReleasedEvent( InputEventType_t nType, int nTick, ButtonCode_t scanCode, ButtonCode_t virtualCode )
{
	InputState_t &state = m_InputState[ m_bIsPolling ];
	if ( state.m_ButtonState.IsBitSet( scanCode ) )
	{
		// Update button state
		state.m_ButtonState.Clear( scanCode ); 
		state.m_ButtonReleasedTick[ scanCode ] = nTick;

		// Add this event to the app-visible event queue
		PostEvent( nType, nTick, scanCode, virtualCode );
	}
}


//-----------------------------------------------------------------------------
//	Purpose: Pass Joystick button events through the engine's window procs
//-----------------------------------------------------------------------------
void CInputSystem::ProcessEvent( UINT uMsg, WPARAM wParam, LPARAM lParam )
{
#if !defined( PLATFORM_POSIX )
	// To prevent subtle input timing bugs, all button events must be fed 
	// through the window proc once per frame, same as the keyboard and mouse.
	HWND hWnd = GetFocus();
	WNDPROC windowProc = (WNDPROC)GetWindowLongPtrW(hWnd, GWLP_WNDPROC );
	if ( windowProc )
	{
		windowProc( hWnd, uMsg, wParam, lParam );
	}
#endif
}


//-----------------------------------------------------------------------------
// Copies the input state record over
//-----------------------------------------------------------------------------
void CInputSystem::CopyInputState( InputState_t *pDest, const InputState_t &src, bool bCopyEvents )
{
	pDest->m_Events.RemoveAll();
	pDest->m_bDirty = false;
	if ( src.m_bDirty )
	{
		pDest->m_ButtonState = src.m_ButtonState;
		memcpy( &pDest->m_ButtonPressedTick, &src.m_ButtonPressedTick, sizeof( pDest->m_ButtonPressedTick ) );
		memcpy( &pDest->m_ButtonReleasedTick, &src.m_ButtonReleasedTick, sizeof( pDest->m_ButtonReleasedTick ) );
		memcpy( &pDest->m_pAnalogDelta, &src.m_pAnalogDelta, sizeof( pDest->m_pAnalogDelta ) );
		memcpy( &pDest->m_pAnalogValue, &src.m_pAnalogValue, sizeof( pDest->m_pAnalogValue ) );
		if ( bCopyEvents )
		{
			if ( src.m_Events.Count() > 0 )
			{
				pDest->m_Events.EnsureCount( src.m_Events.Count() );
				memcpy( pDest->m_Events.Base(), src.m_Events.Base(), src.m_Events.Count() * sizeof(InputEvent_t) );
			}
		}
	}
}


#if defined( WIN32 ) && !defined( USE_SDL )
void CInputSystem::PollInputState_Windows()
{
	if ( IsPC() && m_bPumpEnabled )
	{
		// Poll mouse + keyboard
		MSG msg;
		while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
		{
			if ( msg.message == WM_QUIT )
			{
				PostEvent( IE_Quit, m_nLastSampleTick );
				break;
			}

#if defined( INCLUDE_SCALEFORM )
			if ( g_pScaleformUI )
			{
				// Scaleform IME requirement. Pass these messages to GFxIME BEFORE any TranlsateMessage/DispatchMessage.
				if ( (msg.message == WM_KEYDOWN) || (msg.message == WM_KEYUP) || ImmIsUIMessage( NULL, msg.message, msg.wParam, msg.lParam ) 
					|| (msg.message == WM_LBUTTONDOWN) || (msg.message == WM_LBUTTONUP) )
				{
					g_pScaleformUI->PreProcessKeyboardEvent( (size_t)msg.hwnd, msg.message, msg.wParam, msg.lParam );
				}
			}
#endif

			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}

		// NOTE: Under some implementations of Win9x, 
		// dispatching messages can cause the FPU control word to change
		SetupFPUControlWord();
	}
}
#endif



#if defined(OSX) || defined( USE_SDL )

#if defined( USE_SDL )
static BYTE        scantokey[SDL_NUM_SCANCODES];

static void initKeymap(void)
{
	memset(scantokey, '\0', sizeof (scantokey));

	for (int i = SDL_SCANCODE_A; i <= SDL_SCANCODE_Z; i++)
		scantokey[i] = KEY_A + (i - SDL_SCANCODE_A);
	for (int i = SDL_SCANCODE_1; i <= SDL_SCANCODE_9; i++)
		scantokey[i] = KEY_1 + (i - SDL_SCANCODE_1);
	for (int i = SDL_SCANCODE_F1; i <= SDL_SCANCODE_F12; i++)
		scantokey[i] = KEY_F1 + (i - SDL_SCANCODE_F1);
	for (int i = SDL_SCANCODE_KP_1; i <= SDL_SCANCODE_KP_9; i++)
		scantokey[i] = KEY_PAD_1 + (i - SDL_SCANCODE_KP_1);

	scantokey[SDL_SCANCODE_0] = KEY_0;
    scantokey[SDL_SCANCODE_KP_0] = KEY_PAD_0;
    scantokey[SDL_SCANCODE_RETURN] = KEY_ENTER;
    scantokey[SDL_SCANCODE_ESCAPE] = KEY_ESCAPE;
    scantokey[SDL_SCANCODE_BACKSPACE] = KEY_BACKSPACE;
    scantokey[SDL_SCANCODE_TAB] = KEY_TAB;
    scantokey[SDL_SCANCODE_SPACE] = KEY_SPACE;
    scantokey[SDL_SCANCODE_MINUS] = KEY_MINUS;
    scantokey[SDL_SCANCODE_EQUALS] = KEY_EQUAL;
    scantokey[SDL_SCANCODE_LEFTBRACKET] = KEY_LBRACKET;
    scantokey[SDL_SCANCODE_RIGHTBRACKET] = KEY_RBRACKET;
    scantokey[SDL_SCANCODE_BACKSLASH] = KEY_BACKSLASH;
    scantokey[SDL_SCANCODE_SEMICOLON] = KEY_SEMICOLON;
    scantokey[SDL_SCANCODE_APOSTROPHE] = KEY_APOSTROPHE;
    scantokey[SDL_SCANCODE_GRAVE] = KEY_BACKQUOTE;
    scantokey[SDL_SCANCODE_COMMA] = KEY_COMMA;
    scantokey[SDL_SCANCODE_PERIOD] = KEY_PERIOD;
    scantokey[SDL_SCANCODE_SLASH] = KEY_SLASH;
    scantokey[SDL_SCANCODE_CAPSLOCK] = KEY_CAPSLOCK;
    scantokey[SDL_SCANCODE_SCROLLLOCK] = KEY_SCROLLLOCK;
    scantokey[SDL_SCANCODE_INSERT] = KEY_INSERT;
    scantokey[SDL_SCANCODE_HOME] = KEY_HOME;
    scantokey[SDL_SCANCODE_PAGEUP] = KEY_PAGEUP;
    scantokey[SDL_SCANCODE_DELETE] = KEY_DELETE;
    scantokey[SDL_SCANCODE_END] = KEY_END;
    scantokey[SDL_SCANCODE_PAGEDOWN] = KEY_PAGEDOWN;
    scantokey[SDL_SCANCODE_RIGHT] = KEY_RIGHT;
    scantokey[SDL_SCANCODE_LEFT] = KEY_LEFT;
    scantokey[SDL_SCANCODE_DOWN] = KEY_DOWN;
    scantokey[SDL_SCANCODE_UP] = KEY_UP;
    scantokey[SDL_SCANCODE_NUMLOCKCLEAR] = KEY_NUMLOCK;
    scantokey[SDL_SCANCODE_KP_DIVIDE] = KEY_PAD_DIVIDE;
    scantokey[SDL_SCANCODE_KP_MULTIPLY] = KEY_PAD_MULTIPLY;
    scantokey[SDL_SCANCODE_KP_MINUS] = KEY_PAD_MINUS;
    scantokey[SDL_SCANCODE_KP_PLUS] = KEY_PAD_PLUS;
	// Map keybad enter to enter for vgui. This means vgui dialog won't ever see KEY_PAD_ENTER
    scantokey[SDL_SCANCODE_KP_ENTER] = KEY_ENTER;
    scantokey[SDL_SCANCODE_KP_PERIOD] = KEY_PAD_DECIMAL;
    scantokey[SDL_SCANCODE_APPLICATION] = KEY_APP;
    scantokey[SDL_SCANCODE_LCTRL] = KEY_LCONTROL;
    scantokey[SDL_SCANCODE_LSHIFT] = KEY_LSHIFT;
    scantokey[SDL_SCANCODE_LALT] = KEY_LALT;
    scantokey[SDL_SCANCODE_LGUI] = KEY_LWIN;
    scantokey[SDL_SCANCODE_RCTRL] = KEY_RCONTROL;
    scantokey[SDL_SCANCODE_RSHIFT] = KEY_RSHIFT;
    scantokey[SDL_SCANCODE_RALT] = KEY_RALT;
    scantokey[SDL_SCANCODE_RGUI] = KEY_RWIN;
}

#elif defined(OSX)
static BYTE        scantokey[128] = 
{ 
	KEY_A, KEY_S, KEY_D, KEY_F, KEY_H, KEY_G, KEY_Z, KEY_X,
	KEY_C, KEY_V,  KEY_BACKQUOTE /*german backquote char*/ , KEY_B, KEY_Q, KEY_W, KEY_E, KEY_R,  //15
	KEY_Y, KEY_T, KEY_1, KEY_2, KEY_3, KEY_4, KEY_6, KEY_5, // 23
	KEY_EQUAL, KEY_9, KEY_7, KEY_MINUS, KEY_8, KEY_0, KEY_RBRACKET, KEY_O, //31
	KEY_U, KEY_LBRACKET, KEY_I, KEY_P, KEY_ENTER , KEY_L, KEY_J, KEY_APOSTROPHE, //39
	KEY_K, KEY_SEMICOLON, KEY_BACKSLASH, KEY_COMMA,KEY_SLASH, KEY_N, KEY_M, KEY_PERIOD, // 47
	KEY_TAB, KEY_SPACE, KEY_BACKQUOTE, KEY_BACKSPACE, 0, KEY_ESCAPE, KEY_RWIN, KEY_LWIN, //55
	KEY_LSHIFT, KEY_CAPSLOCK, KEY_LALT, KEY_LCONTROL, KEY_LSHIFT, 0, KEY_RCONTROL, 0, //63
	0, KEY_PAD_DECIMAL,    0  ,    KEY_PAD_MULTIPLY,    0  ,  KEY_PAD_PLUS,    0  , KEY_NUMLOCK , // 71
	0, 0  ,    0  , KEY_PAD_DIVIDE, KEY_PAD_ENTER,    0  ,    KEY_PAD_MINUS,    0  ,  // 79
	0, KEY_PAD_DIVIDE, KEY_PAD_0, KEY_PAD_1, KEY_PAD_2, KEY_PAD_3, KEY_PAD_4, KEY_PAD_5,  // 87
	KEY_PAD_6, KEY_PAD_7, 0, KEY_PAD_8, KEY_PAD_9,  0,    0  ,    0  , // 95
	KEY_F5, KEY_F6, KEY_F7, KEY_F3, KEY_F8, KEY_F9, 0, KEY_F11, // 103
	0, 0  ,    0  ,    0  , 0, KEY_F10,    KEY_APP  , KEY_F12, // 111
	0  ,    0, KEY_INSERT, KEY_HOME, KEY_PAGEUP, KEY_DELETE, KEY_F4, KEY_END,  // 119
	KEY_F2, KEY_PAGEDOWN, KEY_F1, KEY_LEFT, KEY_RIGHT, KEY_DOWN, KEY_UP,  0,  // 127
}; 
#else 
#error
#endif


bool MapCocoaVirtualKeyToButtonCode( int nCocoaVirtualKeyCode, ButtonCode_t *pOut )
{
	if ( nCocoaVirtualKeyCode < 0 )
		*pOut = (ButtonCode_t)(-1 * nCocoaVirtualKeyCode);
	else 
	{
#ifdef OSX
		int modified = nCocoaVirtualKeyCode & 255;
	
		if ( modified > 127)
		{
			return false;
		}
#else
		nCocoaVirtualKeyCode &= 0x000000ff;
#endif
	
		*pOut = (ButtonCode_t)scantokey[nCocoaVirtualKeyCode];
	}

	return true;
}



#ifdef LINUX
void CInputSystem::PollInputState_Linux()
#elif defined( OSX )
void CInputSystem::PollInputState_OSX()
#elif defined( _WIN32 )
void CInputSystem::PollInputState_Windows()
#endif
{
	InputState_t &state = m_InputState[ m_bIsPolling ];

	if (  m_bPumpEnabled )
		m_pLauncherMgr->PumpWindowsMessageLoop();
	// These are Carbon virtual key codes. AFAIK they don't have a header that defines these, but they are supposed to map
	// to the same letters across international keyboards, so our mapping here should work.
	CCocoaEvent events[32];
	while ( 1 )
	{
		int nEvents = m_pLauncherMgr->GetEvents( events, ARRAYSIZE( events ) );
		if ( nEvents == 0 )
			break;

		for ( int iEvent=0; iEvent < nEvents; iEvent++ )
		{
			CCocoaEvent *pEvent = &events[iEvent];

			switch( pEvent->m_EventType )
			{
				case CocoaEvent_Deleted:
					break;

				case CocoaEvent_KeyDown:
				{
					ButtonCode_t virtualCode;
					if ( MapCocoaVirtualKeyToButtonCode( pEvent->m_VirtualKeyCode, &virtualCode ) )
					{
						ButtonCode_t scanCode = virtualCode;

#ifdef LINUX
						if( scanCode != BUTTON_CODE_NONE )
#endif
						{
							// For SDL, hitting spacebar causes a SDL_KEYDOWN event, then SDL_TEXTINPUT with
							//	event.text.text[0] = ' ', and then we get here and wind up sending two events
							//	to PostButtonPressedEvent. The first is virtualCode = ' ', the 2nd has virtualCode = 0.
							// This will confuse Button::OnKeyCodePressed(), which is checking for space keydown
							//	followed by space keyup. So we ignore all BUTTON_CODE_NONE events here.
							PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, scanCode, virtualCode );
						}
						
						InputEvent_t event;
						memset( &event, 0, sizeof(event) );
						event.m_nTick = GetPollTick();
						event.m_nType = IE_KeyCodeTyped;
						event.m_nData = scanCode;
						g_pInputSystem->PostUserEvent( event );
						
#if defined( LINUX ) || (defined( OSX ) && defined( USE_SDL ) )
						if ( scanCode == KEY_BACKSPACE )
						{
							// On Linux (and OS X, when using SDL), we need to fire this event to have backspace keypresses picked up by scaleform.
							PostEvent( IE_KeyTyped, GetPollTick(), (wchar_t)8 );
						}
#endif
					}

					if ( !(pEvent->m_ModifierKeyMask & (1<<eCommandKey) ) && pEvent->m_VirtualKeyCode >= 0 && pEvent->m_UnicodeKey > 0 )
					{
						InputEvent_t event;
						memset( &event, 0, sizeof(event) );
						event.m_nTick = GetPollTick();
						event.m_nType = IE_KeyTyped;
						event.m_nData = (int)pEvent->m_UnicodeKey;
						g_pInputSystem->PostUserEvent( event );
					}
					
#if defined ( CSTRIKE15 )
					// [will] - HACK: Allow cmd+a, cmd+c, cmd+v, cmd+x to go through, and treat them as the ctrl modified versions.
					// This allows these to work in the Scaleform chat window.
					if ( pEvent->m_ModifierKeyMask & (1<<eCommandKey)
						&& ( pEvent->m_UnicodeKey == 'a'
						|| pEvent->m_UnicodeKey == 'c'
						|| pEvent->m_UnicodeKey == 'v'
						|| pEvent->m_UnicodeKey == 'x' ) )
					{
						InputEvent_t event;
						memset( &event, 0, sizeof(event) );
						event.m_nTick = GetPollTick();
						event.m_nType = IE_KeyTyped;
						event.m_nData = (int)pEvent->m_UnicodeKey - 96; // Subtract 96 to give the ctrl version of this character.
						g_pInputSystem->PostUserEvent( event );
					}
#endif

				}
				break;

				case CocoaEvent_KeyUp:
				{
					ButtonCode_t virtualCode;
					if ( MapCocoaVirtualKeyToButtonCode( pEvent->m_VirtualKeyCode, &virtualCode ) )
					{
						ButtonCode_t scanCode = virtualCode;
						PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, scanCode, virtualCode );
					}
				}
				break;

				case CocoaEvent_MouseButtonDown:
				{
					int nButtonMask = pEvent->m_MouseButtonFlags;
					ButtonCode_t dblClickCode = BUTTON_CODE_INVALID;
					if ( pEvent->m_nMouseClickCount > 1 )
					{
						switch( pEvent->m_MouseButton )
						{
							default:
							case COCOABUTTON_LEFT:
								dblClickCode = MOUSE_LEFT;
								break;
							case COCOABUTTON_RIGHT:
								dblClickCode = MOUSE_RIGHT;
								break;
							case COCOABUTTON_MIDDLE:
								dblClickCode = MOUSE_MIDDLE;
								break;
							case COCOABUTTON_4:
								dblClickCode = MOUSE_4;
								break;
							case COCOABUTTON_5:
								dblClickCode = MOUSE_5;
								break;
						}
					}
					UpdateMouseButtonState( nButtonMask, dblClickCode );
				}
				break;

				case CocoaEvent_MouseButtonUp:
				{
					int nButtonMask = pEvent->m_MouseButtonFlags;
					UpdateMouseButtonState( nButtonMask );
				}
				break;

				case CocoaEvent_MouseMove:
				{
					UpdateMousePositionState( state, (short)pEvent->m_MousePos[0], (short)pEvent->m_MousePos[1] );

					InputEvent_t event;
					memset( &event, 0, sizeof(event) );
					event.m_nTick = GetPollTick();
					event.m_nType = IE_LocateMouseClick;
					event.m_nData = (short)pEvent->m_MousePos[0];
					event.m_nData2 = (short)pEvent->m_MousePos[1];
					g_pInputSystem->PostUserEvent( event );
				}
				break;
					
				case CocoaEvent_MouseScroll:
				{
					ButtonCode_t code = (short)pEvent->m_MousePos[1] > 0 ? MOUSE_WHEEL_UP : MOUSE_WHEEL_DOWN;
					state.m_ButtonPressedTick[ code ] = state.m_ButtonReleasedTick[ code ] = m_nLastSampleTick;
					PostEvent( IE_ButtonPressed, m_nLastSampleTick, code, code );
					PostEvent( IE_ButtonReleased, m_nLastSampleTick, code, code );
					
#ifdef LINUX
					state.m_pAnalogDelta[ MOUSE_WHEEL ] = pEvent->m_MousePos[1];
#else
					state.m_pAnalogDelta[ MOUSE_WHEEL ] = ( (short)pEvent->m_MousePos[1] ) / 10;
#endif
					state.m_pAnalogValue[ MOUSE_WHEEL ] += state.m_pAnalogDelta[ MOUSE_WHEEL ];
					PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_WHEEL, state.m_pAnalogValue[ MOUSE_WHEEL ], state.m_pAnalogDelta[ MOUSE_WHEEL ] );
				}
				break;
					
				case CocoaEvent_AppActivate:
				{		
					InputEvent_t event;
					memset( &event, 0, sizeof(event) );
					event.m_nType = IE_FirstAppEvent + 1;
					event.m_nData = (bool)pEvent->m_ModifierKeyMask;

					g_pInputSystem->PostUserEvent( event );
				}
				break;
				case CocoaEvent_AppQuit:
				{
					PostEvent( IE_Quit, m_nLastSampleTick );

				}
				break;
			}
		}
	}
}
#endif // PLATFORM_OSX


//-----------------------------------------------------------------------------
// Polls the current input state
//-----------------------------------------------------------------------------
void CInputSystem::PollInputState( bool bIsInGame )
{
#if !defined( _CERT ) && !defined(LINUX)
	PollPressX360Button();
#endif

	m_bIsPolling = true;
	++m_nPollCount;

	// set whether in a game or not
	m_bIsInGame = bIsInGame;

	// Deals with polled input events
	InputState_t &queuedState = m_InputState[ INPUT_STATE_QUEUED ];
	CopyInputState( &m_InputState[ INPUT_STATE_CURRENT ], queuedState, true );

	// Sample the joystick
	SampleDevices();

	// NOTE: This happens after SampleDevices since that updates LastSampleTick
	// Also, I believe it's correct to post the joystick events with
	// the LastPollTick not updated (not 100% sure though)
	m_nLastPollTick = m_nLastSampleTick;

#if defined( PLATFORM_OSX )
	PollInputState_OSX();
#elif defined( LINUX )
	PollInputState_Linux();
#elif defined( WIN32 )
	PollInputState_Windows();
#elif defined( _PS3 )
#else
#error
#endif

	// Leave the queued state up-to-date with the current
	CopyInputState( &queuedState, m_InputState[ INPUT_STATE_CURRENT ], false );

	m_bIsPolling = false;
}


//-----------------------------------------------------------------------------
// Computes the sample tick
//-----------------------------------------------------------------------------
int CInputSystem::ComputeSampleTick()
{
	// This logic will only fail if the app has been running for 49.7 days
	int nSampleTick;

	DWORD nCurrentTick = Plat_MSTime();
	if ( nCurrentTick >= m_StartupTimeTick )
	{
		nSampleTick = (int)( nCurrentTick - m_StartupTimeTick );
	}
	else
	{
		DWORD nDelta = (DWORD)0xFFFFFFFF - m_StartupTimeTick;
		nSampleTick = (int)( nCurrentTick + nDelta ) + 1;
	}
	return nSampleTick;
}


//-----------------------------------------------------------------------------
// How many times has poll been called?
//-----------------------------------------------------------------------------
int CInputSystem::GetPollCount() const
{
	return m_nPollCount;
}


//-----------------------------------------------------------------------------
// Samples attached devices and appends events to the input queue
//-----------------------------------------------------------------------------
void CInputSystem::SampleDevices( void )
{
	m_nLastSampleTick = ComputeSampleTick();

	static ConVarRef joystick_force_disabled( "joystick_force_disabled" );
#if !defined( PLATFORM_POSIX ) || defined( _GAMECONSOLE )
	if ( joystick_force_disabled.IsValid() && joystick_force_disabled.GetBool() == false )
	{
		PollXDevices();
	}
	
#endif
	if ( m_bXController == false && joystick_force_disabled.IsValid() && joystick_force_disabled.GetBool() == false  )
	{
		PollJoystick();
	}

	m_bSteamController = PollSteamControllers();
}


//-----------------------------------------------------------------------------
//	Purpose: Forwards rumble info to attached devices
//-----------------------------------------------------------------------------
void CInputSystem::SetRumble( float fLeftMotor, float fRightMotor, int userId )
{
#ifndef LINUX
	// TODO: send force feedback to rumble-enabled joysticks
	SetXDeviceRumble( fLeftMotor, fRightMotor, userId );
#endif
}


//-----------------------------------------------------------------------------
//	Purpose: Force an immediate stop, transmits immediately to all devices
//-----------------------------------------------------------------------------
void CInputSystem::StopRumble( int userId )
{
	if ( IsPlatformWindowsPC() )
	{
		if ( userId == INVALID_USER_ID )
		{
			xdevice_t* pXDevice = &m_XDevices[0];

			for ( int i = 0; i < XUSER_MAX_COUNT; ++i, ++pXDevice )
			{
				if ( pXDevice->active )
				{
					pXDevice->vibration.wLeftMotorSpeed = 0;
					pXDevice->vibration.wRightMotorSpeed = 0;
					pXDevice->pendingRumbleUpdate = true;
					WriteToXDevice( pXDevice );
				}
			}
		}
		else
		{
			xdevice_t* pXDevice = &m_XDevices[userId];

			if ( pXDevice->active )
			{
				pXDevice->vibration.wLeftMotorSpeed = 0;
				pXDevice->vibration.wRightMotorSpeed = 0;
				pXDevice->pendingRumbleUpdate = true;
				WriteToXDevice( pXDevice );
			}
		}
	}
	else
	{
#ifndef LINUX
		SetXDeviceRumble( 0, 0, userId );
#endif
	}
}


//-----------------------------------------------------------------------------
// Joystick interface
//-----------------------------------------------------------------------------
int CInputSystem::GetJoystickCount() const
{
	return m_nJoystickCount;
}

void CInputSystem::EnableJoystickInput( int nJoystick, bool bEnable )
{
	m_JoysticksEnabled.SetFlag( 1 << nJoystick, bEnable ); 
}

void CInputSystem::EnableJoystickDiagonalPOV( int nJoystick, bool bEnable )
{
	m_pJoystickInfo[ nJoystick ].m_bDiagonalPOVControlEnabled = bEnable;
}

//-----------------------------------------------------------------------------
// Poll current state
//-----------------------------------------------------------------------------
int CInputSystem::GetPollTick() const
{
	return m_nLastPollTick;
}
	
bool CInputSystem::IsButtonDown( ButtonCode_t code ) const
{
	return m_InputState[INPUT_STATE_CURRENT].m_ButtonState.IsBitSet( code );
}

int CInputSystem::GetAnalogValue( AnalogCode_t code ) const
{
	return m_InputState[INPUT_STATE_CURRENT].m_pAnalogValue[code];
}

int CInputSystem::GetAnalogDelta( AnalogCode_t code ) const
{
	return m_InputState[INPUT_STATE_CURRENT].m_pAnalogDelta[code];
}

int CInputSystem::GetButtonPressedTick( ButtonCode_t code ) const
{
	return m_InputState[INPUT_STATE_CURRENT].m_ButtonPressedTick[code];
}

int CInputSystem::GetButtonReleasedTick( ButtonCode_t code ) const
{
	return m_InputState[INPUT_STATE_CURRENT].m_ButtonReleasedTick[code];
}

bool CInputSystem::MotionControllerActive( ) const
{
	bool isReadingMotionControllerInput = IsDeviceReadingInput( INPUT_DEVICE_HYDRA )  ||
									IsDeviceReadingInput( INPUT_DEVICE_PLAYSTATION_MOVE ) || 
									IsDeviceReadingInput( INPUT_DEVICE_SHARPSHOOTER );

	return ( isReadingMotionControllerInput && m_bMotionControllerActive );
}

Quaternion CInputSystem::GetMotionControllerOrientation( ) const
{
	return m_qMotionControllerOrientation;
}


float CInputSystem::GetMotionControllerPosX( ) const
{
	return m_fMotionControllerPosX;
}

float CInputSystem::GetMotionControllerPosY( ) const
{
	return m_fMotionControllerPosY;
}


int CInputSystem::GetMotionControllerDeviceStatus( ) const
{
	return m_nMotionControllerStatus;
}

void CInputSystem::SetMotionControllerDeviceStatus( int nStatus )
{
	m_nMotionControllerStatus = nStatus;
}

uint64 CInputSystem::GetMotionControllerDeviceStatusFlags( ) const
{
	return m_nMotionControllerStatusFlags;
}

#if defined( _OSX ) || defined (LINUX)
// this is defined in xcontroller.cpp, but that file isn't included
// in posix builds
void CInputSystem::SetMotionControllerCalibrationInvalid( void )
{
}

void CInputSystem::StepMotionControllerCalibration( void )
{

}

void CInputSystem::ResetMotionControllerScreenCalibration( void )
{

}

#endif // _OSX

//-----------------------------------------------------------------------------
// Returns the input events since the last poll
//-----------------------------------------------------------------------------
int CInputSystem::GetEventCount() const
{
	return m_InputState[INPUT_STATE_CURRENT].m_Events.Count();
}

const InputEvent_t* CInputSystem::GetEventData( ) const
{
	return m_InputState[INPUT_STATE_CURRENT].m_Events.Base();
}


//-----------------------------------------------------------------------------
// Posts a user-defined event into the event queue; this is expected
// to be called in overridden wndprocs connected to the root panel.
//-----------------------------------------------------------------------------
void CInputSystem::PostUserEvent( const InputEvent_t &event )
{
	InputState_t &state = m_InputState[ m_bIsPolling ];
	state.m_Events.AddToTail( event );
	state.m_bDirty = true;
}

	
//-----------------------------------------------------------------------------
// Chains the window message to the previous wndproc
//-----------------------------------------------------------------------------
inline LRESULT CInputSystem::ChainWindowMessage( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
#if !defined( PLATFORM_POSIX )
	if ( m_ChainedWndProc )
		return CallWindowProc( m_ChainedWndProc, hwnd, uMsg, wParam, lParam );
#endif
	// FIXME: This comment is lifted from vguimatsurface; 
	// may not apply in future when the system is completed.

	// This means the application is driving the messages (calling our window procedure manually)
	// rather than us hooking their window procedure. The engine needs to do this in order for VCR 
	// mode to play back properly.
	return 0;	
}

	
//-----------------------------------------------------------------------------
// Release all buttons
//-----------------------------------------------------------------------------
void CInputSystem::ReleaseAllButtons( int nFirstButton, int nLastButton )
{
	// Force button up messages for all down buttons
	for ( int i = nFirstButton; i <= nLastButton; ++i )
	{
		PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, (ButtonCode_t)i, (ButtonCode_t)i );
	}
}


//-----------------------------------------------------------------------------
// Zero analog state
//-----------------------------------------------------------------------------
void CInputSystem::ZeroAnalogState( int nFirstState, int nLastState )
{
	InputState_t &state = m_InputState[ m_bIsPolling ];
	memset( &state.m_pAnalogDelta[nFirstState], 0, ( nLastState - nFirstState + 1 ) * sizeof(int) );
	memset( &state.m_pAnalogValue[nFirstState], 0, ( nLastState - nFirstState + 1 ) * sizeof(int) );
}


//-----------------------------------------------------------------------------
// Determines all mouse button presses
//-----------------------------------------------------------------------------
int CInputSystem::ButtonMaskFromMouseWParam( WPARAM wParam, ButtonCode_t code, bool bDown ) const
{
	int nButtonMask = 0;

#if !defined( POSIX ) && !defined( USE_SDL)
	if ( wParam & MK_LBUTTON )
	{
		nButtonMask |= 1;
	}

	if ( wParam & MK_RBUTTON )
	{
		nButtonMask |= 2;
	}

	if ( wParam & MK_MBUTTON )
	{
		nButtonMask |= 4;
	}

	if ( wParam & MS_MK_BUTTON4 )
	{
		nButtonMask |= 8;
	}

	if ( wParam & MS_MK_BUTTON5 )
	{
		nButtonMask |= 16;
	}
#endif

#ifdef _DEBUG
	if ( code != BUTTON_CODE_INVALID )
	{
		int nMsgMask = 1 << ( code - MOUSE_FIRST );
		int nTestMask = bDown ? nMsgMask : 0;
		Assert( ( nButtonMask & nMsgMask ) == nTestMask );
	}
#endif

	return nButtonMask;
}


//-----------------------------------------------------------------------------
// Updates the state of all mouse buttons
//-----------------------------------------------------------------------------
void CInputSystem::UpdateMouseButtonState( int nButtonMask, ButtonCode_t dblClickCode )
{
	for ( int i = 0; i < 5; ++i )
	{
		ButtonCode_t code = (ButtonCode_t)( MOUSE_FIRST + i );
		bool bDown = ( nButtonMask & ( 1 << i ) ) != 0;
		if ( bDown )
		{
			InputEventType_t type = ( code != dblClickCode ) ? IE_ButtonPressed : IE_ButtonDoubleClicked; 
			PostButtonPressedEvent( type, m_nLastSampleTick, code, code );
		}
		else
		{
			PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, code, code );
		}
	}
}


//-----------------------------------------------------------------------------
// Handles input messages
//-----------------------------------------------------------------------------
void CInputSystem::SetCursorPosition( int x, int y )
{
	if ( !m_hAttachedHWnd )
		return;

#if defined( USE_SDL )
	m_pLauncherMgr->SetCursorPosition( x, y );
#elif defined( OSX )
	m_pLauncherMgr->SetCursorPosition( x, y );
#elif defined( WIN32 ) 
	POINT pt;
	pt.x = x; pt.y = y;
	ClientToScreen( (HWND)m_hAttachedHWnd, &pt );
	SetCursorPos( pt.x, pt.y );
#elif defined( PLATFORM_PS3 )
	POINT pt;
	pt.x = x; pt.y = y;
	SetCursorPos( pt.x, pt.y );
#else
#error
#endif

	InputState_t &state = m_InputState[ m_bIsPolling ];
	bool bXChanged = ( state.m_pAnalogValue[ MOUSE_X ] != x );
	bool bYChanged = ( state.m_pAnalogValue[ MOUSE_Y ] != y );

	state.m_pAnalogValue[ MOUSE_X ] = x;
	state.m_pAnalogValue[ MOUSE_Y ] = y;
	state.m_pAnalogDelta[ MOUSE_X ] = 0;
	state.m_pAnalogDelta[ MOUSE_Y ] = 0;

	if ( bXChanged )
	{
		PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_X, state.m_pAnalogValue[ MOUSE_X ], state.m_pAnalogDelta[ MOUSE_X ] );
	}
	if ( bYChanged )
	{
		PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_Y, state.m_pAnalogValue[ MOUSE_Y ], state.m_pAnalogDelta[ MOUSE_Y ] );
	}
	if ( bXChanged || bYChanged )
	{
		PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_XY, state.m_pAnalogValue[ MOUSE_X ], state.m_pAnalogValue[ MOUSE_Y ] );
	}
}
 
void CInputSystem::GetCursorPosition( int *pX, int *pY )
{
	if ( !m_hAttachedHWnd )
	{
		*pX = *pY = 0;
		return;
	}

#if defined( USE_SDL )
	*pX = m_InputState[INPUT_STATE_CURRENT].m_pAnalogValue[MOUSE_X];
	*pY = m_InputState[INPUT_STATE_CURRENT].m_pAnalogValue[MOUSE_Y];
#elif defined( PLATFORM_OSX )
	if ( m_bCursorVisible )
	{
		CGEventRef event = CGEventCreate( NULL );
		CGPoint pnt = CGEventGetLocation( event );

		// [will] - QuickDraw functions removed in 10.7, so using using CocoaMgr for window info instead.
		unsigned int displayWidth, displayHeight;
		m_pLauncherMgr->DisplayedSize( displayWidth, displayHeight );

		*pX = pnt.x;
		*pY = pnt.y;
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		int rx, ry, width, height;
		pRenderContext->GetViewport( rx, ry, width, height );
		
		int windowHeight = (int)displayWidth;
		int windowWidth = (int)displayHeight;
		if ( width != windowWidth || abs( height - windowHeight ) > 22 )
		{
			// scale the x/y back into the co-ords of the back buffer, not the scaled up window 
			//DevMsg( "Mouse x:%d y:%d %d %d %d %d\n", x, y, width, windowWidth, height, abs( height - windowHeight ) );
			*pX = *pX * (float)width/windowWidth;
			*pY = *pY * (float)height/windowHeight;
		}

		CFRelease( event );
	}
	else
	{
		// cursor is invisible, just say the center of the screen
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		int rx, ry, width, height;
		pRenderContext->GetViewport( rx, ry, width, height );
		*pX = width/2;
		*pY = height/2;
	}
#elif !defined( PLATFORM_POSIX )
	POINT pt;
	::GetCursorPos( &pt );
	ScreenToClient((HWND)m_hAttachedHWnd, &pt);
	*pX = pt.x; *pY = pt.y;
#endif
}

void CInputSystem::SetMouseCursorVisible( bool bVisible )
{
	m_bCursorVisible = bVisible;
}


void CInputSystem::UpdateMousePositionState( InputState_t &state, short x, short y )
{
	int nOldX = state.m_pAnalogValue[ MOUSE_X ];
	int nOldY = state.m_pAnalogValue[ MOUSE_Y ];

	state.m_pAnalogValue[ MOUSE_X ] = x;
	state.m_pAnalogValue[ MOUSE_Y ] = y;
	state.m_pAnalogDelta[ MOUSE_X ] = state.m_pAnalogValue[ MOUSE_X ] - nOldX;
	state.m_pAnalogDelta[ MOUSE_Y ] = state.m_pAnalogValue[ MOUSE_Y ] - nOldY;

	if ( state.m_pAnalogDelta[ MOUSE_X ] != 0 )
	{
		PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_X, state.m_pAnalogValue[ MOUSE_X ], state.m_pAnalogDelta[ MOUSE_X ] );
	}
	if ( state.m_pAnalogDelta[ MOUSE_Y ] != 0 )
	{
		PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_Y, state.m_pAnalogValue[ MOUSE_Y ], state.m_pAnalogDelta[ MOUSE_Y ] );
	}
	if ( state.m_pAnalogDelta[ MOUSE_X ] != 0 || state.m_pAnalogDelta[ MOUSE_Y ] != 0 )
	{
		PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_XY, state.m_pAnalogValue[ MOUSE_X ], state.m_pAnalogValue[ MOUSE_Y ] );
	}
}


#ifdef PLATFORM_WINDOWS
//-----------------------------------------------------------------------------
// Generates LocateMouseClick messages
//-----------------------------------------------------------------------------
void CInputSystem::LocateMouseClick( LPARAM lParam )
{
	if ( ShouldGenerateUIEvents() )
	{
		PostEvent( IE_LocateMouseClick, m_nLastSampleTick, (short)LOWORD(lParam), (short)HIWORD(lParam) );
	}
}


//-----------------------------------------------------------------------------
// Handles input messages
//-----------------------------------------------------------------------------
LRESULT CInputSystem::WindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
#if !defined( POSIX ) && !defined( USE_SDL )

	if ( !m_bEnabled )
		return ChainWindowMessage( hwnd, uMsg, wParam, lParam );

	if ( ShouldGenerateUIEvents() && ( hwnd != m_hLastIMEHWnd ) )
	{
		m_hLastIMEHWnd = hwnd;
		PostEvent( IE_IMESetWindow, m_nLastSampleTick, (intp)hwnd );
	}

	// Allow ActivateApp messages to get through so we know when to reset input state
	if ( ( hwnd != m_hAttachedHWnd ) && ( uMsg != WM_ACTIVATEAPP ) )
		return ChainWindowMessage( hwnd, uMsg, wParam, lParam );

	InputState_t &state = m_InputState[ m_bIsPolling ];
	switch( uMsg )
	{
	case WM_ACTIVATEAPP:
		if ( hwnd == m_hAttachedHWnd )
		{
			bool bActivated = ( wParam == 1 );
			if ( !bActivated )
			{
				ResetInputState();
			}
		}
		break;

	case WM_CLOSE:
		// Handle close messages
		PostEvent( IE_Close, m_nLastSampleTick );

		// don't Run default message pump, as that destroys the window
		return 0;

	case WM_SETCURSOR:
		if ( ShouldGenerateUIEvents() )
		{
			PostEvent( IE_SetCursor, m_nLastSampleTick );
		}
		break;

	case WM_SIZE:
		{
			int nWidth = LOWORD( lParam );
			int nHeight = HIWORD( lParam );
			bool bMinimized = ( wParam == SIZE_MINIMIZED ) || IsIconic( hwnd );
			if ( bMinimized )
			{
				nWidth = nHeight = 0;
			}
			PostEvent( IE_WindowSizeChanged, m_nLastSampleTick, nWidth, nHeight, bMinimized );
		}
		break;

	case WM_LBUTTONDOWN:
		{
			LocateMouseClick( lParam );
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, MOUSE_LEFT, true );
			ETWMouseDown( 0, (short)LOWORD(lParam), (short)HIWORD(lParam) );
			UpdateMouseButtonState( nButtonMask );
		}
		break;

	case WM_LBUTTONUP:
		{
			LocateMouseClick( lParam );
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, MOUSE_LEFT, false );
			ETWMouseUp( 0, (short)LOWORD(lParam), (short)HIWORD(lParam) );
			UpdateMouseButtonState( nButtonMask );
		}
		break;

	case WM_RBUTTONDOWN:
		{
			LocateMouseClick( lParam );
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, MOUSE_RIGHT, true );
			ETWMouseDown( 2, (short)LOWORD(lParam), (short)HIWORD(lParam) );
			UpdateMouseButtonState( nButtonMask );
		}
		break;

	case WM_RBUTTONUP:
		{
			LocateMouseClick( lParam );
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, MOUSE_RIGHT, false );
			ETWMouseUp( 2, (short)LOWORD(lParam), (short)HIWORD(lParam) );
			UpdateMouseButtonState( nButtonMask );
		}
		break;

	case WM_MBUTTONDOWN:
		{
			LocateMouseClick( lParam );
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, MOUSE_MIDDLE, true );
			ETWMouseDown( 1, (short)LOWORD(lParam), (short)HIWORD(lParam) );
			UpdateMouseButtonState( nButtonMask );
		}
		break;

	case WM_MBUTTONUP:
		{
			LocateMouseClick( lParam );
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, MOUSE_MIDDLE, false );
			ETWMouseUp( 1, (short)LOWORD(lParam), (short)HIWORD(lParam) );
			UpdateMouseButtonState( nButtonMask );
		}
		break;

	case MS_WM_XBUTTONDOWN:
		{
			LocateMouseClick( lParam );

			ButtonCode_t code = ( HIWORD( wParam ) == 1 ) ? MOUSE_4 : MOUSE_5;
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, code, true );
			UpdateMouseButtonState( nButtonMask );

			// Windows docs say the XBUTTON messages we should return true from
			return TRUE;
		}
		break;

	case MS_WM_XBUTTONUP:
		{
			LocateMouseClick( lParam );

			ButtonCode_t code = ( HIWORD( wParam ) == 1 ) ? MOUSE_4 : MOUSE_5;
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, code, false );
			UpdateMouseButtonState( nButtonMask );

			// Windows docs say the XBUTTON messages we should return true from
			return TRUE;
		}
		break;

	case WM_LBUTTONDBLCLK:
		{
			LocateMouseClick( lParam );
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, MOUSE_LEFT, true );
			ETWMouseDown( 0, (short)LOWORD(lParam), (short)HIWORD(lParam) );
			UpdateMouseButtonState( nButtonMask, MOUSE_LEFT );
		}
		break;

	case WM_RBUTTONDBLCLK:
		{
			LocateMouseClick( lParam );
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, MOUSE_RIGHT, true );
			ETWMouseDown( 2, (short)LOWORD(lParam), (short)HIWORD(lParam) );
			UpdateMouseButtonState( nButtonMask, MOUSE_RIGHT );
		}
		break;

	case WM_MBUTTONDBLCLK:
		{
			LocateMouseClick( lParam );
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, MOUSE_MIDDLE, true );
			ETWMouseDown( 1, (short)LOWORD(lParam), (short)HIWORD(lParam) );
			UpdateMouseButtonState( nButtonMask, MOUSE_MIDDLE );
		}
		break;

	case MS_WM_XBUTTONDBLCLK:
		{
			LocateMouseClick( lParam );

			ButtonCode_t code = ( HIWORD( wParam ) == 1 ) ? MOUSE_4 : MOUSE_5;
			int nButtonMask = ButtonMaskFromMouseWParam( wParam, code, true );
			UpdateMouseButtonState( nButtonMask, code );

			// Windows docs say the XBUTTON messages we should return true from
			return TRUE;
		}
		break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		{
			// Suppress key repeats
			if ( !( lParam & ( 1<<30 ) ) )
			{
				// NOTE: These two can be unequal! For example, keypad enter
				// which returns KEY_ENTER from virtual keys, and KEY_PAD_ENTER from scan codes
				// Since things like vgui care about virtual keys; we're going to
				// put both scan codes in the input message
				ButtonCode_t virtualCode = ButtonCode_VirtualKeyToButtonCode( wParam );
				ButtonCode_t scanCode = ButtonCode_ScanCodeToButtonCode( lParam );
				PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, scanCode, virtualCode );

				// Post ETW events describing key presses to help correlate input events to performance
				// problems in the game.
				ETWKeyDown( scanCode, virtualCode, ButtonCodeToString( virtualCode ) );

				// Deal with toggles
				if ( scanCode == KEY_CAPSLOCK || scanCode == KEY_SCROLLLOCK || scanCode == KEY_NUMLOCK )
				{
					int nVirtualKey;
					ButtonCode_t toggleCode;
					switch( scanCode )
					{
					default: case KEY_CAPSLOCK: nVirtualKey = VK_CAPITAL; toggleCode = KEY_CAPSLOCKTOGGLE; break;
					case KEY_SCROLLLOCK: nVirtualKey = VK_SCROLL; toggleCode = KEY_SCROLLLOCKTOGGLE; break;
					case KEY_NUMLOCK: nVirtualKey = VK_NUMLOCK; toggleCode = KEY_NUMLOCKTOGGLE; break;
					};

					SHORT wState = GetKeyState( nVirtualKey );
					bool bToggleState = ( wState & 0x1 ) != 0;
					PostButtonPressedEvent( bToggleState ? IE_ButtonPressed : IE_ButtonReleased, m_nLastSampleTick, toggleCode, toggleCode );
				}
			}

			if ( ShouldGenerateUIEvents() )
			{
				ButtonCode_t virtualCode = ButtonCode_VirtualKeyToButtonCode( wParam );
				int nKeyRepeat = LOWORD( lParam );
				for ( int i = 0; i < nKeyRepeat; ++i )
				{
					PostEvent( IE_KeyCodeTyped, m_nLastSampleTick, virtualCode );
				}
			}
		}
		break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		{
			// Don't handle key ups if the key's already up. This can happen when we alt-tab back to the engine.
			ButtonCode_t virtualCode = ButtonCode_VirtualKeyToButtonCode( wParam );
			ButtonCode_t scanCode = ButtonCode_ScanCodeToButtonCode( lParam );
			PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, scanCode, virtualCode );
		}
		break;

	case WM_MOUSEWHEEL:
		{
			ButtonCode_t code = (short)HIWORD( wParam ) > 0 ? MOUSE_WHEEL_UP : MOUSE_WHEEL_DOWN;
			state.m_ButtonPressedTick[ code ] = state.m_ButtonReleasedTick[ code ] = m_nLastSampleTick;
			PostEvent( IE_ButtonPressed, m_nLastSampleTick, code, code );
			PostEvent( IE_ButtonReleased, m_nLastSampleTick, code, code );

			state.m_pAnalogDelta[ MOUSE_WHEEL ] = ( (short)HIWORD(wParam) ) / WHEEL_DELTA;
			state.m_pAnalogValue[ MOUSE_WHEEL ] += state.m_pAnalogDelta[ MOUSE_WHEEL ];
			PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_WHEEL, state.m_pAnalogValue[ MOUSE_WHEEL ], state.m_pAnalogDelta[ MOUSE_WHEEL ] );
		}
		break;

	case WM_MOUSEMOVE:
		{
			UpdateMousePositionState( state, (short)LOWORD(lParam), (short)HIWORD(lParam) );

			int nButtonMask = ButtonMaskFromMouseWParam( wParam );
			UpdateMouseButtonState( nButtonMask );
		}
 		break;

#if defined ( WIN32 ) && !defined ( _X360 )
	case WM_INPUT:
		{
			if ( m_bRawInputSupported )
			{
				UINT dwSize = sizeof( RAWINPUT );
				static BYTE lpb[ sizeof( RAWINPUT ) ];

				pfnGetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));

				RAWINPUT* raw = (RAWINPUT*)lpb;
				if (raw->header.dwType == RIM_TYPEMOUSE) 
				{
					m_mouseRawAccumX += raw->data.mouse.lLastX;
					m_mouseRawAccumY += raw->data.mouse.lLastY;
				} 
			}
		}
		break;
#endif

	case WM_SYSCHAR:
	case WM_CHAR:
		if ( ShouldGenerateUIEvents() && !m_bIMEComposing )
		{
			PostEvent( IE_KeyTyped, m_nLastSampleTick, (wchar_t)wParam );
		}
		break;

	case WM_INPUTLANGCHANGE:
		// Note that this is passed to IME managers even if the IME is currently
		// disallowed so that IMEs are still aware of the current language
		// in case they are allowed in the future.
#if defined( INCLUDE_SCALEFORM )
		if ( g_pScaleformUI )
		{
			g_pScaleformUI->HandleIMEEvent( (size_t)hwnd, uMsg, wParam, lParam );
		}
#endif
		if ( ShouldGenerateUIEvents() )
		{
			PostEvent( IE_InputLanguageChanged, m_nLastSampleTick );
		}
		break;

	case WM_IME_KEYDOWN:
#if defined( INCLUDE_SCALEFORM )
		if ( g_pScaleformUI && g_pScaleformUI->HandleIMEEvent( (size_t)hwnd, uMsg, wParam, lParam ) )
			return 0;
#endif
		break;

	case WM_IME_STARTCOMPOSITION:
#if defined( INCLUDE_SCALEFORM )
		if ( g_pScaleformUI && g_pScaleformUI->HandleIMEEvent( (size_t)hwnd, uMsg, wParam, lParam ) )
		{
			m_bIMEComposing = true;
			return 0;
		}
#endif

		if ( ShouldGenerateUIEvents() )
		{
			m_bIMEComposing = true;
			PostEvent( IE_IMEStartComposition, m_nLastSampleTick );
			return TRUE;
		}
		break;

	case WM_IME_COMPOSITION:
#if defined( INCLUDE_SCALEFORM )
		if ( g_pScaleformUI && g_pScaleformUI->HandleIMEEvent( (size_t)hwnd, uMsg, wParam, lParam ) )
			return 0;
#endif

		if ( ShouldGenerateUIEvents() )
		{
			PostEvent( IE_IMEComposition, m_nLastSampleTick, (int)lParam );
			return TRUE;
		}
		break;

	case WM_IME_ENDCOMPOSITION:
#if defined( INCLUDE_SCALEFORM )
		if ( g_pScaleformUI && g_pScaleformUI->HandleIMEEvent( (size_t)hwnd, uMsg, wParam, lParam ) )
		{
			m_bIMEComposing = false;
			return 0;
		}
#endif

		if ( ShouldGenerateUIEvents() )
		{
			m_bIMEComposing = false;
			PostEvent( IE_IMEEndComposition, m_nLastSampleTick );
			return TRUE;
		}
		break;

	case WM_IME_NOTIFY:
#if defined( INCLUDE_SCALEFORM )
		if ( g_pScaleformUI && g_pScaleformUI->HandleIMEEvent( (size_t)hwnd, uMsg, wParam, lParam ) )
			return 0;
#endif

		if ( ShouldGenerateUIEvents() )
		{
			switch (wParam)
			{
			default:
				break;

			case 14:  // Chinese Traditional IMN_PRIVATE...
				break; 

			case IMN_OPENCANDIDATE:
				PostEvent( IE_IMEShowCandidates, m_nLastSampleTick );
				return 1;

			case IMN_CHANGECANDIDATE:
				PostEvent( IE_IMEChangeCandidates, m_nLastSampleTick );
				return 0;

			case IMN_CLOSECANDIDATE:
				PostEvent( IE_IMECloseCandidates, m_nLastSampleTick );
				break;

				// To detect the change of IME mode, or the toggling of Japanese IME 
			case IMN_SETCONVERSIONMODE:
			case IMN_SETSENTENCEMODE:
			case IMN_SETOPENSTATUS:   
				PostEvent( IE_IMERecomputeModes, m_nLastSampleTick );
				if ( wParam == IMN_SETOPENSTATUS )
					return 0;
				break;

			case IMN_CLOSESTATUSWINDOW:   
			case IMN_GUIDELINE:   
			case IMN_OPENSTATUSWINDOW:   
			case IMN_SETCANDIDATEPOS:   
			case IMN_SETCOMPOSITIONFONT:   
			case IMN_SETCOMPOSITIONWINDOW:   
			case IMN_SETSTATUSWINDOWPOS:   
				break;
			}
		}
		break;

	case WM_IME_CHAR:
#if defined( INCLUDE_SCALEFORM )
		if ( g_pScaleformUI && g_pScaleformUI->HandleIMEEvent( (size_t)hwnd, uMsg, wParam, lParam ) )
			return 0;
#endif

		if ( ShouldGenerateUIEvents() )
		{
			// We need to process this message so that the IME doesn't double 
			// convert the unicode IME characters into garbage characters and post
			// them to our window... (get ? marks after text entry ).
			return 0;
		}
		break;

	case WM_IME_SETCONTEXT:
#if defined( INCLUDE_SCALEFORM )
		if ( g_pScaleformUI )
		{
			g_pScaleformUI->HandleIMEEvent( (size_t)hwnd, uMsg, wParam, lParam );
			lParam = 0;
		}
		else 
#endif
		if ( ShouldGenerateUIEvents() )
		{
			// We draw all IME windows ourselves
			lParam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
			lParam &= ~ISC_SHOWUIGUIDELINE;
			lParam &= ~ISC_SHOWUIALLCANDIDATEWINDOW;
		}

		break;

	}

	// Can't put this in the case statement, it's not constant
	if ( IsPC() && ( uMsg == m_uiMouseWheel ) )
	{
		ButtonCode_t code = ( ( int )wParam ) > 0 ? MOUSE_WHEEL_UP : MOUSE_WHEEL_DOWN;
		state.m_ButtonPressedTick[ code ] = state.m_ButtonReleasedTick[ code ] = m_nLastSampleTick;
		PostEvent( IE_ButtonPressed, m_nLastSampleTick, code, code );
		PostEvent( IE_ButtonReleased, m_nLastSampleTick, code, code );

		state.m_pAnalogDelta[ MOUSE_WHEEL ] = ( ( int )wParam ) / WHEEL_DELTA;
		state.m_pAnalogValue[ MOUSE_WHEEL ] += state.m_pAnalogDelta[ MOUSE_WHEEL ];
		PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_WHEEL, state.m_pAnalogValue[ MOUSE_WHEEL ], state.m_pAnalogDelta[ MOUSE_WHEEL ] );
	}
	return ChainWindowMessage( hwnd, uMsg, wParam, lParam );
#else

	return 0;

#endif
}
#endif


//-----------------------------------------------------------------------------
// Initializes, shuts down cursors
//-----------------------------------------------------------------------------
void CInputSystem::InitCursors()
{
#ifdef PLATFORM_WINDOWS
	// load up all default cursors
	memset( m_pDefaultCursors, 0, sizeof(m_pDefaultCursors) );
	m_pDefaultCursors[INPUT_CURSOR_NONE]		= INPUT_CURSOR_HANDLE_INVALID;
	m_pDefaultCursors[INPUT_CURSOR_ARROW]		= (InputCursorHandle_t)LoadCursor(NULL, (LPCTSTR)OCR_NORMAL);
	m_pDefaultCursors[INPUT_CURSOR_IBEAM]		= (InputCursorHandle_t)LoadCursor(NULL, (LPCTSTR)OCR_IBEAM);
	m_pDefaultCursors[INPUT_CURSOR_HOURGLASS]	= (InputCursorHandle_t)LoadCursor(NULL, (LPCTSTR)OCR_WAIT);
	m_pDefaultCursors[INPUT_CURSOR_CROSSHAIR]	= (InputCursorHandle_t)LoadCursor(NULL, (LPCTSTR)OCR_CROSS);
	m_pDefaultCursors[INPUT_CURSOR_WAITARROW]	= (InputCursorHandle_t)LoadCursor(NULL, (LPCTSTR)32650);
	m_pDefaultCursors[INPUT_CURSOR_UP]			= (InputCursorHandle_t)LoadCursor(NULL, (LPCTSTR)OCR_UP);
	m_pDefaultCursors[INPUT_CURSOR_SIZE_NW_SE]	= (InputCursorHandle_t)LoadCursor(NULL, (LPCTSTR)OCR_SIZENWSE);
	m_pDefaultCursors[INPUT_CURSOR_SIZE_NE_SW]	= (InputCursorHandle_t)LoadCursor(NULL, (LPCTSTR)OCR_SIZENESW);
	m_pDefaultCursors[INPUT_CURSOR_SIZE_W_E]	= (InputCursorHandle_t)LoadCursor(NULL, (LPCTSTR)OCR_SIZEWE);
	m_pDefaultCursors[INPUT_CURSOR_SIZE_N_S]	= (InputCursorHandle_t)LoadCursor(NULL, (LPCTSTR)OCR_SIZENS);
	m_pDefaultCursors[INPUT_CURSOR_SIZE_ALL]	= (InputCursorHandle_t)LoadCursor(NULL, (LPCTSTR)OCR_SIZEALL);
	m_pDefaultCursors[INPUT_CURSOR_NO]			= (InputCursorHandle_t)LoadCursor(NULL, (LPCTSTR)OCR_NO);
	m_pDefaultCursors[INPUT_CURSOR_HAND]		= (InputCursorHandle_t)LoadCursor(NULL, (LPCTSTR)32649);
#endif
}

void CInputSystem::ShutdownCursors()
{
#ifdef PLATFORM_WINDOWS
	int nCount = m_UserCursors.GetNumStrings();
	for ( int i = 0; i < nCount; ++i )
	{
		::DestroyCursor( (HCURSOR)m_UserCursors[ i ] );
	}
	m_UserCursors.Purge();

	for ( int i = 0; i < ARRAYSIZE( m_pDefaultCursors ); ++i )
	{
		if ( m_pDefaultCursors[i] != INPUT_CURSOR_HANDLE_INVALID )
		{
			::DestroyCursor( (HCURSOR)m_pDefaultCursors[ i ] );
			m_pDefaultCursors[ i ] = INPUT_CURSOR_HANDLE_INVALID;
		}
	}
#endif
}


//-----------------------------------------------------------------------------
// Gets the cursor
//-----------------------------------------------------------------------------
InputCursorHandle_t CInputSystem::GetStandardCursor( InputStandardCursor_t id )
{
	return m_pDefaultCursors[id];
}

InputCursorHandle_t CInputSystem::LoadCursorFromFile( const char *pFileName, const char *pPathID )
{
	if ( !g_pFullFileSystem )
		return INPUT_CURSOR_HANDLE_INVALID;

	char fn[ 512 ];
	Q_strncpy( fn, pFileName, sizeof( fn ) );
	Q_strlower( fn );
	Q_FixSlashes( fn );

	UtlSymId_t nCursorIndex = m_UserCursors.Find( fn );
	if ( nCursorIndex != m_UserCursors.InvalidIndex() )
		return m_UserCursors[ nCursorIndex ];

	g_pFullFileSystem->GetLocalCopy( fn );

#ifdef PLATFORM_WINDOWS
	char fullpath[ 512 ];
	g_pFullFileSystem->RelativePathToFullPath( fn, pPathID, fullpath, sizeof( fullpath ) );

	HCURSOR newCursor = (HCURSOR)::LoadCursorFromFile( fullpath );
	m_UserCursors[ fn ] = (InputCursorHandle_t)newCursor;
	return (InputCursorHandle_t)newCursor;
#endif
	return 0;
}

void CInputSystem::SetCursorIcon( InputCursorHandle_t hCursor )
{
#ifdef PLATFORM_WINDOWS
	m_hCursor = hCursor;
	HCURSOR hWindowsCursor = (HCURSOR)hCursor;
	::SetCursor( hWindowsCursor ); 
#endif
}

void CInputSystem::ResetCursorIcon()
{
	SetCursorIcon( m_hCursor );
}

void CInputSystem::EnableMouseCapture( PlatWindow_t hWnd )
{
#ifdef PLATFORM_WINDOWS
	if ( m_hCurrentCaptureWnd == hWnd )
		return;

	// Determine if we're the foreground window.  If not, force release of the mouse.  Otherwise, we can capture the mouse
	// while we're in the background and then we never get WM_ACTIVATE messages when trying to click on the app.  This
	// causes the app to react like it has mouse focus (firing weapons, etc) but doesn't actually come to the foreground
	// and doesn't accept keyboard input.
	//
	// We're using GetForegroundWindow here, but we really want to ask engine or game if they're the ActiveApp.
	bool bActiveWindow = true;

#if !defined( _GAMECONSOLE )
	HWND hInputWnd = reinterpret_cast< HWND >( hWnd );
	bActiveWindow = ( hInputWnd == ::GetForegroundWindow() );
#else
	HWND hInputWnd = reinterpret_cast< HWND >( m_hCurrentCaptureWnd );
#endif

	if ( m_hCurrentCaptureWnd != PLAT_WINDOW_INVALID || !bActiveWindow )
	{
		::ReleaseCapture();
	}

	m_hCurrentCaptureWnd = hWnd;
	if ( m_hCurrentCaptureWnd != PLAT_WINDOW_INVALID && bActiveWindow )
	{
		::SetCapture( hInputWnd );
	}
#endif
}

void CInputSystem::GetRawMouseAccumulators( int& accumX, int& accumY )
{
#if defined( USE_SDL )

	if ( m_pLauncherMgr )
	{
		m_pLauncherMgr->GetMouseDelta( accumX, accumY, false );
	}

#else

	accumX = m_mouseRawAccumX;
	accumY = m_mouseRawAccumY;
	m_mouseRawAccumX = m_mouseRawAccumY = 0;

#endif
}

void CInputSystem::DisableMouseCapture()
{
#ifdef PLATFORM_WINDOWS
	EnableMouseCapture( PLAT_WINDOW_INVALID );
#endif
}


// ===================================================================
//  If we add another support for another input device, we need to
//  update the platform assignments below to reflect it.  From here,
//  pretty much everything else that uses these interfaces will work 
//   unchanged (obviously UI and device code needs to be added)
//  Also: Add name to GetInputDeviceNameUI/Internal() in 
//  PlatformInputDevice.cpp
// ===================================================================
void  CInputSystem::InitPlatfromInputDeviceInfo( void )
{
	PlatformInputDevice::InitPlatfromInputDeviceInfo();

	// Set the platform for which this code/client is compiled on and
	// the input devices that are assumed to be already installed (as 
	// opposed to being queried by the inputsystem)

#if defined( PLATFORM_WINDOWS_PC )
	m_currentlyConnectedInputDevices = INPUT_DEVICE_KEYBOARD_MOUSE;
#elif defined( PLATFORM_OSX )
	m_currentlyConnectedInputDevices = INPUT_DEVICE_KEYBOARD_MOUSE;
#elif defined( PLATFORM_LINUX )
	m_currentlyConnectedInputDevices = INPUT_DEVICE_KEYBOARD_MOUSE;
#elif defined( PLATFORM_X360 )
	m_currentlyConnectedInputDevices = INPUT_DEVICE_GAMEPAD;
#elif defined( PLATFORM_PS3 )
	m_currentlyConnectedInputDevices = INPUT_DEVICE_NONE;
#else
	m_currentlyConnectedInputDevices = INPUT_DEVICE_NONE;
#endif

	ResetCurrentInputDevice();

	m_setCurrentInputDeviceOnNextButtonPress = false;
}


void CInputSystem::ResetCurrentInputDevice( void )
{
	if ( m_currentInputDevice == INPUT_DEVICE_STEAM_CONTROLLER )
	{
		// Disable resetting away from the steam controller if it's being used.
		return;
	}

#if defined( PLATFORM_WINDOWS_PC )
	m_currentInputDevice = INPUT_DEVICE_KEYBOARD_MOUSE;
#elif defined( PLATFORM_OSX )
	m_currentInputDevice = INPUT_DEVICE_KEYBOARD_MOUSE;
#elif defined( PLATFORM_LINUX )
	m_currentInputDevice = INPUT_DEVICE_KEYBOARD_MOUSE;
#elif defined( PLATFORM_X360 )
	m_currentInputDevice = INPUT_DEVICE_GAMEPAD;
#elif defined( PLATFORM_PS3 )
	m_currentInputDevice = INPUT_DEVICE_NONE;
#else
	m_currentInputDevice = INPUT_DEVICE_NONE;
#endif

}


InputDevice_t CInputSystem::GetConnectedInputDevices( void )
{
	return m_currentlyConnectedInputDevices;
}


bool CInputSystem::IsInputDeviceConnected( InputDevice_t device )
{
	if ( countBits( device ) != 1 || ( device & PlatformInputDevice::s_AllInputDevices ) != device )
	{
		AssertMsg( false, "invalid input device" );
		return false;
	}

	return ( ( m_currentlyConnectedInputDevices & device ) == device );
}


void CInputSystem::SetInputDeviceConnected( InputDevice_t device, bool connected )
{
	if ( ( countBits( device ) != 1 ) || ( device & PlatformInputDevice::s_validPlatformInputDevices[PlatformInputDevice::s_LocalInputPlatform] ) != device   )
	{
		AssertMsg( false, "invalid input device" );
		return;
	}

	if ( connected )
	{
		// Message if device already connected?
		m_currentlyConnectedInputDevices = m_currentlyConnectedInputDevices | device; 
	}
	else
	{
		// Message if device not currently connected?
		m_currentlyConnectedInputDevices = m_currentlyConnectedInputDevices & (~device);
	}
}


InputDevice_t CInputSystem::IsOnlySingleDeviceConnected( void )
{
	int32 mask = 1;

	// nav controller doesn't need to be considered a seperate device.
	int32 connectedMask = m_currentlyConnectedInputDevices & (~INPUT_DEVICE_MOVE_NAV_CONTROLLER);
	
	if ( IsInputDeviceConnected( INPUT_DEVICE_SHARPSHOOTER ) )
	{
		connectedMask = m_currentlyConnectedInputDevices & ( ~INPUT_DEVICE_PLAYSTATION_MOVE );
	}

	// [dkorus] loop through a mask that represents each possible device. 
	//			if one matches our connected mask exactly, we have only that device connected
	while( mask <= INPUT_DEVICE_MAX )
	{
		if ( connectedMask == mask )
			return (InputDevice_t) connectedMask;
		mask = mask << 1;
	}

	return INPUT_DEVICE_NONE;
}


bool CInputSystem::IsDeviceReadingInput( InputDevice_t device ) const
{
#ifndef _GAMECONSOLE
	return true;
#endif

#if !defined( _CERT )
	// [dkorus] test code for the device selection 
	int forceSelected = dev_force_selected_device.GetInt(); 
	if ( forceSelected != 0)
	{
		if ( device == forceSelected )
		{
			return true;
		}
		else 
		{
			return false;
		}
	}
#endif

	if ( device == m_currentInputDevice ||
		 m_currentInputDevice == INPUT_DEVICE_NONE )
		{
			return true;
		}

	return false;
}


InputDevice_t CInputSystem::GetCurrentInputDevice( void )
{
	return m_currentInputDevice;
}


void CInputSystem::SetCurrentInputDevice( InputDevice_t device )
{
	if ( ( device != INPUT_DEVICE_NONE ) && 
		( ( countBits( device ) != 1 ) || ( device & PlatformInputDevice::s_validPlatformInputDevices[PlatformInputDevice::s_LocalInputPlatform] ) != device   ) )
	{
		AssertMsg( false, "invalid input device" );
		return;
	}

	m_currentInputDevice = device;
}

void CInputSystem::SampleInputToFindCurrentDevice( bool doSample )
{
	m_setCurrentInputDeviceOnNextButtonPress = doSample;
}

bool CInputSystem::IsSamplingForCurrentDevice( void )
{
	return m_setCurrentInputDeviceOnNextButtonPress;
}


#ifndef LINUX

#if !defined( _CERT )
// [mhansen] Add support for pressing Xbox 360 controller buttons (should work on PS3 too)
struct C_press_x360_button_code
{
	char c1;
	char c2;
	xKey_t key;
};

static const C_press_x360_button_code press_x360_button_codes[] =
{ 
	{ 'l', 't', XK_BUTTON_LTRIGGER },
	{ 'r', 't', XK_BUTTON_RTRIGGER },
	{ 's', 't', XK_BUTTON_START },
	{ 'b', 'a', XK_BUTTON_BACK },
	{ 'l', 'b', XK_BUTTON_LEFT_SHOULDER },
	{ 'r', 'b', XK_BUTTON_RIGHT_SHOULDER },
	{ 'l', 's', XK_BUTTON_LEFT_SHOULDER },
	{ 'r', 's', XK_BUTTON_RIGHT_SHOULDER },
	{ 'a', 0, XK_BUTTON_A },
	{ 'b', 0, XK_BUTTON_B },
	{ 'x', 0, XK_BUTTON_X },
	{ 'y', 0, XK_BUTTON_Y },
	{ 'l', 0, XK_BUTTON_LEFT },
	{ 'r', 0, XK_BUTTON_RIGHT },
	{ 'u', 0, XK_BUTTON_UP },
	{ 'd', 0, XK_BUTTON_DOWN },
};
static const int cNum_press_x360_button_codes = ARRAYSIZE( press_x360_button_codes );

void CInputSystem::PressX360Button( const CCommand &args )
{
	if ( pc_fake_controller.GetBool( ) && !m_bXController )
	{
		// [dkorus] we're simulating fake controller input and we don't have a controller enabled.  Fake a controller so we can accept controller presses.
		//			this fixes the PC so it can use the same scripting engine as the other setups
		//			NOTE:  This is wrapped in a !_CERT block.  This shouldn't end up in the shipped game.
		m_bXController = true;
	}

	if ( args.ArgC() < 2 )
	{
		Warning( "press_x360_button: requires a key to send (lt, rt, st[art], ba[ck], lb, rb, a, b, x, y, l[eft], r[right], u[p], d[own])" );
		return;
	}
	
	const char* pKey = args[1];

	// We're stashing this in a bitmask so make sure we don't overflow it
	//COMPILE_TIME_ASSERT( cNum_press_x360_button_codes < sizeof( m_press_x360_buttons[ 0 ] ) );

	xKey_t key = XK_BUTTON_A;
	for ( uint32 i = 0; i < cNum_press_x360_button_codes; i++ )
	{
		if ( pKey[0] == press_x360_button_codes[i].c1 && ( pKey[1] == press_x360_button_codes[i].c2 || press_x360_button_codes[i].c2 == 0 ) )
		{
			key = press_x360_button_codes[i].key;
			m_press_x360_buttons[ 0 ] = m_press_x360_buttons[ 0 ] | (1 << i );
			break;
		}
	}
}

void CInputSystem::PollPressX360Button( void )
{
	uint32 pressedButtons = m_press_x360_buttons[ 0 ];
	uint32 releasedButtons = m_press_x360_buttons[ 1 ];

	// Reset the buttons we pressed this frame
	m_press_x360_buttons[ 0 ] = 0;

	// Store the buttons we pressed this frame so we can clear them next frame
	m_press_x360_buttons[ 1 ] = pressedButtons;

	// Clear any old button presses and press any new ones
	for ( uint32 i = 0; i < cNum_press_x360_button_codes; i++ )
	{
		uint32 mask = 1 << i;
		if ( releasedButtons & mask )
		{
			PostXKeyEvent( 0, press_x360_button_codes[i].key, 0 );
		}

		if ( pressedButtons & mask )
		{
			PostXKeyEvent( 0, press_x360_button_codes[i].key, 32768/*XBX_MAX_BUTTONSAMPLE*/ );
		}
	}
}

#endif // !_CERT

#endif
