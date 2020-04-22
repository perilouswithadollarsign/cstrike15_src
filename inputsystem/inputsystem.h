//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#ifndef INPUTSYSTEM_H
#define INPUTSYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#define DONT_DEFINE_DWORD

#include "platform.h"
#include "basetypes.h"

#ifdef PLATFORM_WINDOWS_PC
#define OEMRESOURCE //for OCR_* cursor junk
#define _WIN32_WINNT 0x502
#include <windows.h>
#include <zmouse.h>
#include "xbox/xboxstubs.h"
#include "../../dx9sdk/include/XInput.h"
#endif

#if defined( _WIN32 ) && defined( USE_SDL )
#include "appframework/ilaunchermgr.h"
#endif

#if defined(PLATFORM_POSIX) && !defined(_PS3)
#ifdef PLATFORM_OSX
#define DWORD DWORD
#define CARBON_WORKAROUND

#include <CoreFoundation/CoreFoundation.h>
#include <Carbon/Carbon.h>    
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <Kernel/IOKit/hidsystem/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <ForceFeedback/ForceFeedback.h>
#include <ForceFeedback/ForceFeedbackConstants.h>
#undef DWORD
#else
typedef char xKey_t;
#endif // OSX
#include "posix_stubs.h"
#endif // POSIX
#include "appframework/ilaunchermgr.h"

#include "inputsystem/iinputsystem.h"
#include "tier2/tier2.h"

#ifdef _PS3
#include "ps3/ps3_platform.h"
#include "ps3/ps3_win32stubs.h"
#include "ps3/ps3_core.h"
#include "ps3/ps3stubs.h"

#include <cell/pad.h>
#endif

#include "tier1/UtlStringMap.h"
#include "inputsystem/ButtonCode.h"
#include "inputsystem/AnalogCode.h"
#include "bitvec.h"
#include "tier1/utlvector.h"
#include "tier1/utlflags.h"
#include "input_device.h"

#include "steam/steam_api.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#include "xbox/xbox_console.h"
#endif

enum
{
	INPUT_TYPE_GENERIC_JOYSTICK = 0,
	INPUT_TYPE_STEAMCONTROLLER,
};

//-----------------------------------------------------------------------------
// Implementation of the input system
//-----------------------------------------------------------------------------
class CInputSystem : public CTier2AppSystem< IInputSystem >
{
	typedef CTier2AppSystem< IInputSystem > BaseClass;

public:
	// Constructor, destructor
	CInputSystem();
	virtual ~CInputSystem();

	// Inherited from IAppSystem
	virtual	InitReturnVal_t Init();
	virtual bool Connect( CreateInterfaceFn factory );

	virtual void Shutdown();

	// Inherited from IInputSystem
	virtual void AttachToWindow( void* hWnd );
	virtual void DetachFromWindow( );
	virtual void EnableInput( bool bEnable );
	virtual void EnableMessagePump( bool bEnable );
	virtual int GetPollTick() const;
	virtual void PollInputState( bool bIsInGame = false );
	virtual bool IsButtonDown( ButtonCode_t code ) const;
	virtual int GetButtonPressedTick( ButtonCode_t code ) const;
	virtual int GetButtonReleasedTick( ButtonCode_t code ) const;
	virtual int GetAnalogValue( AnalogCode_t code ) const;
	virtual int GetAnalogDelta( AnalogCode_t code ) const;
	virtual int GetEventCount() const;
	virtual bool MotionControllerActive() const;
	virtual Quaternion GetMotionControllerOrientation() const;
	virtual float	   GetMotionControllerPosX() const;
	virtual float	   GetMotionControllerPosY() const;
	virtual int		   GetMotionControllerDeviceStatus() const;
	virtual uint64	   GetMotionControllerDeviceStatusFlags() const;
	virtual void	   SetMotionControllerDeviceStatus( int nStatus );
	virtual void	   SetMotionControllerCalibrationInvalid( void );
	virtual void	   StepMotionControllerCalibration( void );
	virtual void	   ResetMotionControllerScreenCalibration( void );

	virtual const InputEvent_t* GetEventData( ) const;
	virtual void PostUserEvent( const InputEvent_t &event );
	virtual int GetJoystickCount() const;
	virtual void EnableJoystickInput( int nJoystick, bool bEnable );
	virtual void EnableJoystickDiagonalPOV( int nJoystick, bool bEnable );
	virtual void SampleDevices( void );
	virtual void SetRumble( float fLeftMotor, float fRightMotor, int userId );
	virtual void StopRumble( int userId = INVALID_USER_ID );
	virtual void ResetInputState( void );
	virtual const char *ButtonCodeToString( ButtonCode_t code ) const;
	virtual const char *AnalogCodeToString( AnalogCode_t code ) const;
	virtual ButtonCode_t StringToButtonCode( const char *pString ) const;
	virtual AnalogCode_t StringToAnalogCode( const char *pString ) const;
	virtual ButtonCode_t VirtualKeyToButtonCode( int nVirtualKey ) const;
	virtual int ButtonCodeToVirtualKey( ButtonCode_t code ) const;
	virtual ButtonCode_t ScanCodeToButtonCode( int lParam ) const;
	virtual void SleepUntilInput( int nMaxSleepTimeMS );
	virtual int GetPollCount() const;
	virtual void SetCursorPosition( int x, int y );
	void GetRawMouseAccumulators( int& accumX, int& accumY );
	virtual void GetCursorPosition( int *pX, int *pY );
	virtual void SetMouseCursorVisible( bool bVisible );
	virtual void AddUIEventListener();
	virtual void RemoveUIEventListener();
	virtual PlatWindow_t GetAttachedWindow() const;
	virtual InputCursorHandle_t GetStandardCursor( InputStandardCursor_t id );
	virtual InputCursorHandle_t LoadCursorFromFile( const char *pFileName, const char *pPathID = NULL );
	virtual void SetCursorIcon( InputCursorHandle_t hCursor );
	virtual void EnableMouseCapture( PlatWindow_t hWnd );
	virtual void DisableMouseCapture();

#ifdef PLATFORM_WINDOWS
	LRESULT WindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );
#elif defined(PLATFORM_OSX)
	// helper function for callbacks
	struct JoystickInfo_t;
	void HIDAddElement(CFTypeRef refElement, JoystickInfo_t &info );
#endif

#ifdef _PS3
	virtual void SetPS3CellPadDataHook( BCellPadDataHook_t hookFunc );
	virtual void SetPS3CellPadNoDataHook( BCellPadNoDataHook_t hookFunc );

	virtual void SetPS3StartButtonIdentificationMode();
	virtual bool GetPS3CursorPos( int &x, int &y );
	virtual void DisableHardwareCursor( void );
	virtual void EnableHardwareCursor( void );
	void ExitHardwareCursor( void );
#endif

#if defined( USE_SDL )
	virtual void DisableHardwareCursor( void );
	virtual void EnableHardwareCursor( void );
#endif
	
	virtual void ResetCursorIcon();

	// handles the connected input devices
	virtual InputDevice_t GetConnectedInputDevices( void );					// returns the bitfield of all connected devices
	virtual bool IsInputDeviceConnected( InputDevice_t device );
	virtual void SetInputDeviceConnected( InputDevice_t device, bool connected = true );
	virtual InputDevice_t IsOnlySingleDeviceConnected( void );

	// handles the selected "current" primary input device
	virtual bool IsDeviceReadingInput( InputDevice_t device ) const;				// returns whether the passed in device is the current device.  Returns true if no current device is defined
	virtual InputDevice_t GetCurrentInputDevice( void );					// returns the enum referring to the one currently selected device
	virtual void SetCurrentInputDevice( InputDevice_t device );
	virtual void ResetCurrentInputDevice( void );							// sets the input device to the platform default

	virtual void SampleInputToFindCurrentDevice( bool );  // looks for the next 'significant' button press to determine and set the current input device
	virtual bool IsSamplingForCurrentDevice( void );

	void InitPlatfromInputDeviceInfo( void );

private:

	enum
	{
		STICK1_AXIS_X,
		STICK1_AXIS_Y,
		STICK2_AXIS_X,
		STICK2_AXIS_Y,
		MAX_STICKAXIS
	};
	
	enum
	{
		INPUT_STATE_QUEUED = 0,
		INPUT_STATE_CURRENT,

		INPUT_STATE_COUNT,
	};

public:
#if defined(PLATFORM_OSX)
	struct OSXInputValue_t
	{
		bool m_bSet;
		int m_MinVal;
		int m_MaxVal;
		int m_MinReport;
		int m_MaxReport;
		int m_Cookie;
		uint32 m_Usage;
		CFTypeRef m_RefElement;
	};
#define MAX_JOYSTICK_BUTTONS 32
#endif
	
	struct JoystickInfo_t
	{
#if defined(PLATFORM_WINDOWS) || defined(_GAMECONSOLE)
		JOYINFOEX m_JoyInfoEx;
#elif defined(PLATFORM_OSX)
		FFDeviceObjectReference m_FFInterface;
		IOHIDDeviceInterface **m_Interface;
		long usage;  // from IOUSBHID Parser.h
		long usagePage;  // from IOUSBHID Parser.h
		CInputSystem *m_pParent;
		bool m_bRemoved;
		bool m_bXBoxRumbleEnabled;
		OSXInputValue_t m_xaxis;
		OSXInputValue_t m_yaxis;
		OSXInputValue_t m_zaxis;
		OSXInputValue_t m_raxis;
		OSXInputValue_t m_uaxis;
		OSXInputValue_t m_vaxis;
		OSXInputValue_t m_POV;
		OSXInputValue_t m_Buttons[MAX_JOYSTICK_BUTTONS];
#elif defined(LINUX)
		void *m_pDevice;  // Really an SDL_GameController*, NULL if not present.
		void *m_pHaptic;  // Really an SDL_Haptic*
		float m_fCurrentRumble;
		bool m_bRumbleEnabled;

#else
#error
#endif
		int m_nButtonCount;
		int m_nAxisFlags;
		int m_nDeviceId;
		bool m_bHasPOVControl;
		bool m_bDiagonalPOVControlEnabled;
		unsigned int m_nFlags;
		unsigned int m_nLastPolledButtons;
		unsigned int m_nLastPolledAxisButtons;
		unsigned int m_nLastPolledPOVState;
		unsigned long m_pLastPolledAxes[MAX_JOYSTICK_AXES];
	};
private:
	struct xdevice_t
	{
		int					userId;
		byte				type;			
		byte				subtype;
		word				flags;
		bool				active;
		XINPUT_STATE		states[2];
		int					newState;
		// track Xbox stick keys from previous frame
		xKey_t				lastStickKeys[MAX_STICKAXIS];
		int					stickThreshold[MAX_STICKAXIS];
		float				stickScale[MAX_STICKAXIS];
		int					quitTimeout;
		int					dpadLock;
		// rumble
		XINPUT_VIBRATION	vibration;
		bool				pendingRumbleUpdate;
	};

	struct appKey_t
	{
		int repeats;
		int	sample;
	};

	struct InputState_t
	{
		// Analog states
		CBitVec<BUTTON_CODE_LAST> m_ButtonState;
		int m_ButtonPressedTick[ BUTTON_CODE_LAST ];
		int m_ButtonReleasedTick[ BUTTON_CODE_LAST ];
		int m_pAnalogDelta[ ANALOG_CODE_LAST ];
		int m_pAnalogValue[ ANALOG_CODE_LAST ];
		CUtlVector< InputEvent_t > m_Events;
		bool m_bDirty;
	};

	// Steam Controller
	struct steampad_t
	{
		steampad_t()
		{ 
			m_nHardwareIndex = 0;
			m_nJoystickIndex = INVALID_USER_ID;
			m_nLastPacketIndex = 0;
			active = false; 
			memset( lastAnalogKeys, 0, sizeof( lastAnalogKeys ) );
		}
		bool				active;

		sKey_t				lastAnalogKeys[MAX_STEAMPADAXIS];
		appKey_t			m_appSKeys[ SK_MAX_KEYS ];
		// Hardware index and joystick index don't necessarily match
		// Joystick index will depend on the order of multiple initialized devices
		// Which could include other controller types
		// Hardware index should line up 1:1 with the order they're polled
		// and could change based on removing devices, unlike Joystick Index
		uint32				m_nHardwareIndex;
		int					m_nJoystickIndex;
		uint32				m_nLastPacketIndex;
	};

	steampad_t m_SteamControllerDevice[MAX_STEAM_CONTROLLERS];
	uint32 m_unNumSteamControllerConnected;

	bool	m_bControllerModeActive;

	int m_nControllerType[MAX_JOYSTICKS+MAX_STEAM_CONTROLLERS];

	//Steam controllers start after this index.
	int m_nJoystickBaseline;

public:
	// Initializes all Xbox controllers
	void InitializeXDevices( void );

	// Opens an Xbox controller
	void OpenXDevice( xdevice_t* pXDevice, int userId );

	// Closes an Xbox controller
	void CloseXDevice( xdevice_t* pXDevice );

	// Samples the Xbox controllers
	void PollXDevices( void );

	// Samples console mouse
	void PollXMouse();

	// Samples console keyboard
	void PollXKeyboard();

	// Helper function used by ReadXDevice to handle stick direction events
	void HandleXDeviceAxis( xdevice_t *pXDevice, int nAxisValue, xKey_t negativeKey, xKey_t positiveKey, int axisID );

	// Samples an Xbox controller and queues input events
	void ReadXDevice( xdevice_t* pXDevice );

	// Submits force feedback data to an Xbox controller
	void WriteToXDevice( xdevice_t* pXDevice );

	// Sets rumble values for an Xbox controller
	void SetXDeviceRumble( float fLeftMotor, float fRightMotor, int userId );

#if !defined( _CERT ) && !defined(LINUX)
	CON_COMMAND_MEMBER_F( CInputSystem, "press_x360_button", PressX360Button, "Press the specified Xbox 360 controller button (lt, rt, st[art], ba[ck], lb, rb, a, b, x, y, l[eft], r[right], u[p], d[own])", 0 );
	void PollPressX360Button( void );
	uint32 m_press_x360_buttons[ 2 ];
#endif

#if defined( _PS3 )
	void PS3_PollKeyboard( void );
	void PS3_PollMouse( void );
	void PS3_XInputPollEverything( BCellPadDataHook_t hookFunc, BCellPadNoDataHook_t hookNoDataFunc );
	DWORD PS3_XInputGetState( DWORD dwUserIndex, PXINPUT_STATE pState );
	void HandlePS3SharpshooterButtons( void );
	void HandlePS3Move( PXINPUT_STATE& pState );

	virtual void PS3SetupHardwareCursor( void* image );
#endif
	void QueueMoveControllerRumble( float fRightMotor );

	// Posts an Xbox key event, ignoring key repeats 
	void PostXKeyEvent( int nUserId, xKey_t xKey, int nSample );

	// Dispatches all joystick button events through the game's window procs
	void ProcessEvent( UINT uMsg, WPARAM wParam, LPARAM lParam );

	// Initializes SteamControllers - Returns true if steam is running and finds controllers, otherwise false
	bool InitializeSteamControllers( void );

	// Samples all Steam Controllers - returns true if active this frame
	bool PollSteamControllers( void );

	// Initializes joysticks
	void InitializeJoysticks( void );

	// Samples the joystick
	void PollJoystick( void );

	// Update the joystick button state
	void UpdateJoystickButtonState( int nJoystick );

	// Update the joystick POV control
	void UpdateJoystickPOVControl( int nJoystick );

	// Record button state and post the event
	void JoystickButtonEvent( ButtonCode_t button, int sample );

	bool IsSteamControllerActive() const;
	void SetSteamControllerMode( const char *pSteamControllerMode, const void *obj );

private:

	// Purpose: Get raw joystick sample along axis
#if defined(LINUX)
	void AxisAnalogButtonEvent( ButtonCode_t buttonCode, bool state, int nLastSampleTick );
#elif defined(OSX)
	unsigned int AxisValue( JoystickAxis_t axis, JoystickInfo_t &info );
#else
	unsigned int AxisValue( JoystickAxis_t axis, JOYINFOEX& ji );
#endif

	// Chains the window message to the previous wndproc
	LRESULT ChainWindowMessage( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

	// Post an event to the queue
	void PostEvent( int nType, int nTick, int nData = 0, int nData2 = 0, int nData3 = 0 );

	// Deals with app deactivation (sends a bunch of button released messages)
	void ActivateInputSystem( bool bActivated );

	// Determines all mouse button presses
	int ButtonMaskFromMouseWParam( WPARAM wParam, ButtonCode_t code = BUTTON_CODE_INVALID, bool bDown = false ) const;

	// Updates the state of all mouse buttons
	void UpdateMouseButtonState( int nButtonMask, ButtonCode_t dblClickCode = BUTTON_CODE_INVALID );

	// Copies the input state record over
	void CopyInputState( InputState_t *pDest, const InputState_t &src, bool bCopyEvents );

	// Post an button press/release event to the queue
	void PostButtonPressedEvent( InputEventType_t nType, int nTick, ButtonCode_t scanCode, ButtonCode_t virtualCode );
	void PostButtonReleasedEvent( InputEventType_t nType, int nTick, ButtonCode_t scanCode, ButtonCode_t virtualCode );

	// Release all buttons
	void ReleaseAllButtons( int nFirstButton = 0, int nLastButton = BUTTON_CODE_LAST - 1 );

	// Zero analog state
	void ZeroAnalogState( int nFirstState, int nLastState );

	// Converts xbox keys to button codes
	ButtonCode_t XKeyToButtonCode( int nUserId, int nXKey ) const;

	// Converts SteamController keys to button codes
	ButtonCode_t SKeyToButtonCode( int nUserId, int nXKey ) const;	
	
	// Computes the sample tick
	int ComputeSampleTick();

	// Clears the input state, doesn't generate key-up messages
	void ClearInputState( bool bPurgeState );

	// Called for mouse move events. Sets the current x and y and posts events for the mouse moving.
	void UpdateMousePositionState( InputState_t &state, short x, short y );

	// Should we generate UI events?
	bool ShouldGenerateUIEvents() const;

	// Generates LocateMouseClick messages
	void LocateMouseClick( LPARAM lParam );

	// Initializes, shuts down cursors
	void InitCursors();
	void ShutdownCursors();


#ifdef WIN32
	void PollInputState_Windows();
#endif
	// Poll input state for different OSes.
#if defined( PLATFORM_OSX )
	void PollInputState_OSX();
	void HIDGetElementInfo( CFTypeRef refElement, OSXInputValue_t &input );
	bool HIDBuildDevice( io_object_t ioHIDDeviceObject, JoystickInfo_t &info );
	bool HIDCreateOpenDeviceInterface( io_object_t hidDevice, JoystickInfo_t &info );
	void HIDGetDeviceInfo( io_object_t hidDevice, CFMutableDictionaryRef hidProperties, JoystickInfo_t &info );
	void HIDGetElements( CFTypeRef refElementCurrent, JoystickInfo_t &info );
	void HIDGetCollectionElements( CFMutableDictionaryRef deviceProperties, JoystickInfo_t &info );
	void HIDDisposeDevice( JoystickInfo_t &info );
	int	HIDGetElementValue( JoystickInfo_t &info, OSXInputValue_t &value );
	int HIDScaledCalibratedValue( JoystickInfo_t &info, OSXInputValue_t &value );
	void HIDSortJoystickButtons( JoystickInfo_t &info );

#elif defined(LINUX)
public:
	void PollInputState_Linux();
	void JoystickHotplugAdded( int joystickIndex );
	void JoystickHotplugRemoved( int joystickId );
	void JoystickButtonPress( int joystickId, int button ); // button is a SDL_CONTROLLER_BUTTON;
	void JoystickButtonRelease( int joystickId, int button ); // same as above.
	void JoystickAxisMotion( int joystickId, int axis, int value );

#endif

private:

#if defined( USE_SDL ) || defined( OSX )
	ILauncherMgr *m_pLauncherMgr;
#endif

	WNDPROC m_ChainedWndProc;
	HWND m_hAttachedHWnd;
	HWND m_hLastIMEHWnd;
	bool m_bEnabled;
	bool m_bPumpEnabled;
	bool m_bIsPolling;
	bool m_bIMEComposing;
	bool m_bIsInGame;

	// Current button state
	InputState_t m_InputState[INPUT_STATE_COUNT];

	DWORD m_StartupTimeTick;
	int m_nLastPollTick;
	int m_nLastSampleTick;
	int m_nPollCount;

	// Mouse wheel hack
	UINT m_uiMouseWheel;

	// Joystick info
	CUtlFlags<unsigned short> m_JoysticksEnabled;
	int m_nJoystickCount;
	bool m_bXController;
	bool m_bSteamController;
	float m_flLastSteamControllerInput;

	float m_flLastControllerPollTime;
public:
	JoystickInfo_t m_pJoystickInfo[ MAX_JOYSTICKS ];

private:

	// Xbox controller info
	appKey_t	m_appXKeys[ XUSER_MAX_COUNT ][ XK_MAX_KEYS ];
	xdevice_t	m_XDevices[ XUSER_MAX_COUNT ];

	// Used to determine whether to generate UI events
	int			m_nUIEventClientCount;

	// raw mouse input
	bool m_bRawInputSupported;
	int	 m_mouseRawAccumX, m_mouseRawAccumY;

	// Current mouse capture window
	PlatWindow_t m_hCurrentCaptureWnd;

	// For the 'SleepUntilInput' feature
	HANDLE m_hEvent;

	// Cursors, foiled again!
	InputCursorHandle_t m_pDefaultCursors[ INPUT_CURSOR_COUNT ];
	CUtlStringMap< InputCursorHandle_t >	m_UserCursors;

	CSysModule   *m_pXInputDLL;
	CSysModule   *m_pRawInputDLL;

	// NVNT falcon module
	CSysModule	 *m_pNovintDLL;

private:
	bool		m_bCursorVisible;
	bool		m_bMotionControllerActive;
	Quaternion	m_qMotionControllerOrientation;
	float		m_fMotionControllerPosX;
	float		m_fMotionControllerPosY;
	Vector		m_vecMotionControllerPos;
	int			m_nMotionControllerStatus;
	uint64		m_nMotionControllerStatusFlags;

public:

	InputCursorHandle_t m_hCursor;
#ifdef _PS3
	BCellPadDataHook_t m_pPS3CellPadDataHook;
	BCellPadNoDataHook_t m_pPS3CellNoPadDataHook;

	bool	m_PS3KeyboardConnected;
	bool	m_PS3MouseConnected;
#endif

	// describes all connected devices.  A bitmask of InputDevice entries
	InputDevice_t m_currentlyConnectedInputDevices; 
	// describes the current default input device
	InputDevice_t m_currentInputDevice;
	// number of different input devices on this platform

	bool m_setCurrentInputDeviceOnNextButtonPress;

};


// Should we generate UI events?
inline bool CInputSystem::ShouldGenerateUIEvents() const
{
	return m_nUIEventClientCount > 0;
}


#endif // INPUTSYSTEM_H
