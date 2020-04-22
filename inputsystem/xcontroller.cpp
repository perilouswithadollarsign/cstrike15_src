//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Xbox controller implementation for inputsystem.dll
//
//===========================================================================//

#include "inputsystem.h"

#ifdef _PS3
#include <sysutil/sysutil_common.h>
#include <sysutil/sysutil_sysparam.h>
#include <cell/mouse.h>
#include <cell/keyboard.h>
#include "movecontroller_ps3.h"
#include "key_translation.h"
#include <cell/gcm.h>
#endif 

#include "vstdlib/IKeyValuesSystem.h"
#include "materialsystem/imaterialsystem.h"
#include "vgui/isurface.h"
#include "vgui_controls/controls.h"


// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"
#include "input_device.h"

//-----------------------------------------------------------------------------
// Xbox helpers
//-----------------------------------------------------------------------------
#ifdef PLATFORM_PS3
#define XBX_MAX_MOTOR_SPEED			255
#define PS3_MAX_MOUSE				2	//Max PS3 mouse connections
#define PS3_MAX_KEYBOARD			2	//Max PS3 keyboard connections
#define PS3_MOUSE_LEFT				( 1<<0 )
#define PS3_MOUSE_RIGHT				( 1<<1 )
#define PS3_MOUSE_WHEEL				( 1<<2 )
#define GetTickCount Plat_MSTime
ConVar ps3_joy_ss( "ps3_joy_ss", "0", FCVAR_DEVELOPMENTONLY, "" );
extern ConVar ps3_move_enabled;
extern ConVar ps3_move_roll_trigger;
#else
#define XBX_MAX_MOTOR_SPEED			65535
#endif

#define XBX_MAX_BUTTONSAMPLE		32768
#define XBX_MAX_ANALOGSAMPLE		255
#define XBX_MAX_STICKSAMPLE_LEFT	32768
#define XBX_MAX_STICKSAMPLE_RIGHT	32767
#define XBX_MAX_STICKSAMPLE_DOWN	32768
#define XBX_MAX_STICKSAMPLE_UP		32767

#define XBX_STICK_SCALE_LEFT(x) 	( ( float )XBX_MAX_STICKSAMPLE_LEFT/( float )( XBX_MAX_STICKSAMPLE_LEFT-(x) ) )
#define XBX_STICK_SCALE_RIGHT(x) 	( ( float )XBX_MAX_STICKSAMPLE_RIGHT/( float )( XBX_MAX_STICKSAMPLE_RIGHT-(x) ) )
#define XBX_STICK_SCALE_DOWN(x) 	( ( float )XBX_MAX_STICKSAMPLE_DOWN/( float )( XBX_MAX_STICKSAMPLE_DOWN-(x) ) )
#define XBX_STICK_SCALE_UP(x)	 	( ( float )XBX_MAX_STICKSAMPLE_UP/( float )( XBX_MAX_STICKSAMPLE_UP-(x) ) )

#define XBX_STICK_SMALL_THRESHOLD	((int)( 0.20f * XBX_MAX_STICKSAMPLE_LEFT ))

// Threshold for counting analog movement as a button press
#define JOYSTICK_ANALOG_BUTTON_THRESHOLD	XBX_MAX_STICKSAMPLE_LEFT * 0.4f

// Xbox key translation
typedef struct
{
	int	xinput;
	int	xkey;
} xInputToXKey_t;

xInputToXKey_t g_digitalXKeyTable[] = 
{
	{XINPUT_GAMEPAD_DPAD_UP,		XK_BUTTON_UP}, 
	{XINPUT_GAMEPAD_DPAD_DOWN,		XK_BUTTON_DOWN}, 
	{XINPUT_GAMEPAD_DPAD_LEFT,		XK_BUTTON_LEFT}, 
	{XINPUT_GAMEPAD_DPAD_RIGHT,		XK_BUTTON_RIGHT}, 
	{XINPUT_GAMEPAD_START,			XK_BUTTON_START}, 
	{XINPUT_GAMEPAD_BACK,			XK_BUTTON_BACK}, 
	{XINPUT_GAMEPAD_LEFT_THUMB,		XK_BUTTON_STICK1}, 
	{XINPUT_GAMEPAD_RIGHT_THUMB,	XK_BUTTON_STICK2}, 
	{XINPUT_GAMEPAD_LEFT_SHOULDER,	XK_BUTTON_LEFT_SHOULDER}, 
	{XINPUT_GAMEPAD_RIGHT_SHOULDER,	XK_BUTTON_RIGHT_SHOULDER}, 
	{XINPUT_GAMEPAD_A,				XK_BUTTON_A}, 
	{XINPUT_GAMEPAD_B,				XK_BUTTON_B}, 
	{XINPUT_GAMEPAD_X,				XK_BUTTON_X}, 
	{XINPUT_GAMEPAD_Y,				XK_BUTTON_Y}, 
};

#if !defined( _GAMECONSOLE )
	typedef DWORD (WINAPI *XInputGetState_t)
		(
		DWORD         dwUserIndex,  // [in] Index of the gamer associated with the device
		XINPUT_STATE* pState        // [out] Receives the current state
		);

	typedef DWORD (WINAPI *XInputSetState_t)
		(
		DWORD             dwUserIndex,  // [in] Index of the gamer associated with the device
		XINPUT_VIBRATION* pVibration    // [in, out] The vibration information to send to the controller
		);

	typedef DWORD (WINAPI *XInputGetCapabilities_t)
		(
		DWORD                dwUserIndex,   // [in] Index of the gamer associated with the device
		DWORD                dwFlags,       // [in] Input flags that identify the device type
		XINPUT_CAPABILITIES* pCapabilities  // [out] Receives the capabilities
		);

	XInputGetState_t PC_XInputGetState;
	XInputSetState_t PC_XInputSetState;
	XInputGetCapabilities_t PC_XInputGetCapabilities;

	#define XINPUTGETSTATE			PC_XInputGetState
	#define XINPUTSETSTATE			PC_XInputSetState
	#define XINPUTGETCAPABILITIES	PC_XInputGetCapabilities
#elif defined( PLATFORM_PS3 )
	#define XINPUTGETSTATE			PS3_XInputGetState
	#define XINPUTSETSTATE			PS3_XInputSetState
	#define XINPUTGETCAPABILITIES	PS3_XInputGetCapabilities
#else
	#define XINPUTGETSTATE			XInputGetState
	#define XINPUTSETSTATE			XInputSetState
	#define XINPUTGETCAPABILITIES	XInputGetCapabilities
#endif

#if defined( PLATFORM_PS3 )

// PS3 key modifier translation
// CTRL/SHIFT/ALT provided as modifiers instead of keycodes, so need a separate table
typedef struct
{
	int				modifier;
	ButtonCode_t	buttonCode;
} ps3ModifierToButtonCode_t;

static ps3ModifierToButtonCode_t s_ps3ModifierTable[] = 
{
	{CELL_KB_MKEY_L_SHIFT,		KEY_LSHIFT}, 
	{CELL_KB_MKEY_R_SHIFT,		KEY_RSHIFT}, 
	{CELL_KB_MKEY_L_CTRL,		KEY_LCONTROL}, 
	{CELL_KB_MKEY_R_CTRL,		KEY_RCONTROL}, 
	{CELL_KB_MKEY_L_ALT,		KEY_LALT}, 
	{CELL_KB_MKEY_R_ALT,		KEY_RALT}
};

static int PS3_SetupKb(int i);

static class PS3_XInputInfo_t
{
	public:
		PS3_XInputInfo_t()
		{
			ps3_move_roll_old = 0.0f;
			ps3_move_rumble_value = 0;
			ps3_move_rumble_queued = false;
		}

	// Global information about all controllers connection and settings state
	CellPadInfo2 m_CellPadInfo2;
	float m_flLastConnectedTime[ MAX( XUSER_MAX_COUNT, CELL_PAD_MAX_PORT_NUM ) ];

	// Information about each controller button and stick data
	CellPadData m_CellPadData[ MAX( XUSER_MAX_COUNT, CELL_PAD_MAX_PORT_NUM ) ];
	CellPadData m_lastCellPadData[ MAX( XUSER_MAX_COUNT, CELL_PAD_MAX_PORT_NUM ) ];

	// Packet number is incremented every time data is successfully obtained from each controller
	DWORD m_dwPacketNumber[ MAX( XUSER_MAX_COUNT, CELL_PAD_MAX_PORT_NUM ) ];
	DWORD m_dwPadPortSettingPressOn;
	
	// Global setting whether "ACCEPT" input is CIRCLE button - at the lowest level when
	// CIRCLE is pressed we return it as A button and when CROSS is pressed as B button
	bool m_bInputJapaneseSwapAB;

	// In single-controller mode where multiple controllers can control the game we need
	// to keep tracking of who is the "active" controller for vibration
	int m_iActiveSingleControllerIndex;
	float m_flLastActiveSingleControllerTime;

	MoveControllerState	m_moveControllerState;
	MoveControllerState m_moveControllerStateOld;

	// Mouse connections
	CellMouseInfo		m_CellMouseInfo;
	CellMouseInfo		m_CellMouseInfoOld; // Previous mouse status
	CellMouseData		m_CellMouseData[PS3_MAX_MOUSE];
	CellMouseData		m_CellMouseDataOld[PS3_MAX_MOUSE];

	// Keyboard connections
	CellKbInfo			m_CellKbInfo;
	CellKbInfo			m_CellKbInfoOld; // Previous kb status
	CellKbData			m_CellKbData[PS3_MAX_KEYBOARD];
	CellKbData			m_CellKbDataOld[PS3_MAX_KEYBOARD];
	float				ps3_move_roll_old;
	WORD				ps3_move_rumble_value;
	bool				ps3_move_rumble_queued;

}
g_PS3_XInputInfo;
float display_aspect_ratio;
// [HARDWARE CURSOR] : hardware cursor address in local memory for 64x64x4 argb cursor image (2k aligned)
static uint32_t *s_cursorBufferAddress = NULL; // currently active cursor

void createCursorImage_SolidColor(void *cursorImage, const uint32_t argbColor)
{
	uint32_t *argb = (uint32_t*)cursorImage;
	const int w=64, h=64;
	for (int i=0; i<(w*h); i++)
		argb[i]=argbColor;
}

void initCursor()
{
	// load the Prx module needed for cellVideoOutConvertCursorColor
	int ret = cellSysmoduleLoadModule(CELL_SYSMODULE_AVCONF_EXT);
	if( ret<0 ) { printf("cellSysmoduleLoadModule(CELL_SYSMODULE_AVCONF_EXT) failed (0x%x)\n", ret); exit(-1); }

	if ( cellGcmInitCursor() != CELL_OK )
	{
		Msg("Error initializing hardware cursor\n");
	}

}
void CInputSystem::DisableHardwareCursor()
{
	cellGcmSetCursorDisable();
	cellGcmUpdateCursor();
}

void CInputSystem::ExitHardwareCursor()
{
	DisableHardwareCursor();
	cellSysmoduleUnloadModule(CELL_SYSMODULE_AVCONF_EXT);
}

void loadCursorImage(const void *cursorImage, uint32_t *cursorBufferAddress)
{
	assert(( (intptr_t) cursorBufferAddress % (2*1024) )==0); // must be 2k aligned
	cellVideoOutConvertCursorColor(CELL_VIDEO_OUT_PRIMARY, CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_X8R8G8B8, 1.0f, CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_X8R8G8B8, (void*)cursorImage, cursorBufferAddress, 64*64);
}


// set the active hardware cursor address (can only set active on a loaded cursor image)
void setActiveCursor( uint32_t *cursorBufferAddress)
{
	assert(( (intptr_t) cursorBufferAddress % (2*1024))==0); // must be 2k aligned
	uint32_t cursorOffset;
	cellGcmAddressToOffset( cursorBufferAddress, &cursorOffset );
	if( cellGcmSetCursorImageOffset(cursorOffset) != CELL_OK )
	{
		Msg( "Hardware Cursor Error: setting up hardware cursor offset\n" );
	}
}
#if defined( _PS3 )
void CInputSystem::EnableHardwareCursor()
{
	if (cellGcmSetCursorEnable() != CELL_OK )
	{
		Msg( "Hardware Cursor Error: trouble with enable\n" );
	}

	if ( cellGcmUpdateCursor() != CELL_OK )
	{	
		Msg( "Hardware Cursor Error: trouble with update\n" );
	}
}
#endif


#define CELL_PAD_DEADZONE_PROPORTION 0.2
#define CELL_PAD_ACTIVE_CONTROL_TIME 0.2
void PS3_XInputActiveSingleControllerSet( int iActiveSingleControllerIndex )
{
	if ( iActiveSingleControllerIndex == g_PS3_XInputInfo.m_iActiveSingleControllerIndex )
	{
		g_PS3_XInputInfo.m_flLastActiveSingleControllerTime = Plat_FloatTime();
		return; // remember when was last time of activity on the original controller
	}

	// Request is coming in to activate a different controller
	if ( ( iActiveSingleControllerIndex > 0 ) &&
		( Plat_FloatTime() - g_PS3_XInputInfo.m_flLastActiveSingleControllerTime < CELL_PAD_ACTIVE_CONTROL_TIME ) )
	{
		return; // let the original controller remain in control
	}
	
	// Deactivate vibration on the previously active controller
	if ( g_PS3_XInputInfo.m_iActiveSingleControllerIndex > 0 )
	{
		CellPadActParam param;
		memset( &param, 0, sizeof( param ) );
		cellPadSetActDirect( g_PS3_XInputInfo.m_iActiveSingleControllerIndex - 1, &param );
	}

	g_PS3_XInputInfo.m_iActiveSingleControllerIndex = iActiveSingleControllerIndex;
}


// [HARDWARE CURSOR] allocate cursor buffer in local memory (multiple of 2k pages, aligned on 2k boundaries)
static uint32_t *allocateCursorBuffer()
{
	CellGcmConfig config;
	cellGcmGetConfiguration(&config);
	return (uint32_t*)config.localAddress;
}

void UpdatePS3Mouse( int i )
{
	int res;
	CellMouseData msData;

	res = cellMouseGetData(i, &msData);
	if (res == CELL_MOUSE_OK)
	{
		if(g_PS3_XInputInfo.m_CellMouseData[i].update == CELL_MOUSE_DATA_UPDATE)
		{
			// Keep copy of last valid mouse data
			g_PS3_XInputInfo.m_CellMouseDataOld[i] = g_PS3_XInputInfo.m_CellMouseData[i];
		}
		g_PS3_XInputInfo.m_CellMouseData[i] = msData;
	}	
}

// sets x and y screen coordinates for mouse and PS move input devices
bool CInputSystem::GetPS3CursorPos( int &x, int &y )
{
	bool result = false;
#ifndef _PS3
	// only defined under PS3
	Assert(0); 
#endif
 	InputDevice_t currentDevice = GetCurrentInputDevice();
 	if ( ( IsInputDeviceConnected( INPUT_DEVICE_KEYBOARD_MOUSE ) ) && 
 		 ( currentDevice == INPUT_DEVICE_KEYBOARD_MOUSE || currentDevice ==  INPUT_DEVICE_NONE) )
	{
		for(int i=0;i<PS3_MAX_MOUSE;i++)  
		{
			UpdatePS3Mouse( i );
		}

		    PollXMouse( );

		if((!(g_PS3_XInputInfo.m_CellMouseInfo.info & CELL_MOUSE_INFO_INTERCEPTED)) &&
			(g_PS3_XInputInfo.m_CellMouseInfo.status[0] == CELL_MOUSE_STATUS_CONNECTED) &&
			(g_PS3_XInputInfo.m_CellMouseData[0].update == CELL_MOUSE_DATA_UPDATE))
		{
			x = g_PS3_XInputInfo.m_CellMouseData[0].x_axis;
			y = g_PS3_XInputInfo.m_CellMouseData[0].y_axis;
			
		}
		else
		{
			x = g_PS3_XInputInfo.m_CellMouseDataOld[0].x_axis;
			y = g_PS3_XInputInfo.m_CellMouseDataOld[0].y_axis;
		}

		x = m_mouseRawAccumX;
		y = m_mouseRawAccumY;

		result = true;
	}
	else if ( ( IsInputDeviceConnected( INPUT_DEVICE_PLAYSTATION_MOVE ) &&
			    ( currentDevice == INPUT_DEVICE_PLAYSTATION_MOVE || currentDevice ==  INPUT_DEVICE_NONE ) )
			  ||
			  ( IsInputDeviceConnected( INPUT_DEVICE_SHARPSHOOTER ) &&
				( currentDevice == INPUT_DEVICE_SHARPSHOOTER || currentDevice ==  INPUT_DEVICE_NONE ) ) )
	{
		int nScreenWidth, nScreenHeight;
		materials->GetBackBufferDimensions( nScreenWidth, nScreenHeight );

		x = nScreenWidth * ( ( GetMotionControllerPosX() + 1.0f ) * 0.5f );
		y = nScreenHeight * ( 1.0 - ( ( GetMotionControllerPosY() + 1.0f ) * 0.5f ) );

		if ( vgui::surface()->IsCursorVisible() )
		{
			PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_XY, x, y );
		}

		result = true;
	}

	return result;
}

void CInputSystem::PS3SetupHardwareCursor( void* image )
{
	if( image == NULL )
	{
		// [dkorus] using our old default block cursor for testing!
		uint32_t solidCursorImage[64*64]; // original cursor images:
		image = solidCursorImage;
		createCursorImage_SolidColor( image, 0xa0ffffff );
	}

	s_cursorBufferAddress = allocateCursorBuffer();
	initCursor();
	loadCursorImage( image, s_cursorBufferAddress );
	setActiveCursor( s_cursorBufferAddress );
}

void CInputSystem::PS3_PollMouse()
{
	int res;
	CellMouseData msData;
	CellMouseInfo* pMsInfo = &g_PS3_XInputInfo.m_CellMouseInfo;
	CellMouseInfo* pMsInfoOld = &g_PS3_XInputInfo.m_CellMouseInfoOld;
	*pMsInfoOld = *pMsInfo;

	if (CELL_OK != (res = cellMouseGetInfo(pMsInfo))) {
		Msg("Error%08X : cellMouseGetInfo\n", res);
	}
	else
	{
		// Check info field for monitoring the INTERCEPTED state (when system grabs ownership of the mouse data).
		bool bIntercepted = pMsInfo->info & CELL_MOUSE_INFO_INTERCEPTED;
		if( bIntercepted &&	(!(pMsInfoOld->info & CELL_MOUSE_INFO_INTERCEPTED)))
		{
			Msg("Lost the ownership of the mouse data\n");
		}
		else if(!bIntercepted && (pMsInfoOld->info & CELL_MOUSE_INFO_INTERCEPTED))
		{
			Msg("Regained ownership of the mouse data\n");
			for(int i=0;i<PS3_MAX_MOUSE;i++)
			{
				cellMouseClearBuf(i);
			}
		}

		if(!bIntercepted)
		{
			for(int i=0;i<PS3_MAX_MOUSE;i++) 
			{
				// Check for mouse device insertion and removal */
				if (pMsInfo->status[i] == CELL_MOUSE_STATUS_CONNECTED) 
				{
					if (pMsInfoOld->status[i] == CELL_MOUSE_STATUS_DISCONNECTED) 
					{
						Msg("Mouse[%d] connected.\n", i);
						cellMouseClearBuf(i);
						// testing the hardware cursor
						m_PS3MouseConnected = true;

						EnableHardwareCursor();
					}
					UpdatePS3Mouse( i );
				}
				else if(pMsInfo->status[i] == CELL_MOUSE_STATUS_DISCONNECTED) 
				{
					if(pMsInfoOld->status[i] == CELL_MOUSE_STATUS_CONNECTED) 
					{
						//	call these when we're done:
						// 	disableCursor();
						//	exitCursor();
						Msg("Mouse[%d] disconnected.\n", i);
						m_PS3MouseConnected = false;		// ?

					}
				}

				SetInputDeviceConnected( INPUT_DEVICE_KEYBOARD_MOUSE, ( m_PS3KeyboardConnected && m_PS3MouseConnected ) );

			}
		}
	}
}

void CInputSystem::PS3_PollKeyboard()
{
	int res;
	CellKbData kbData;
	CellKbInfo* pKbInfo = &g_PS3_XInputInfo.m_CellKbInfo;
	CellKbInfo* pKbInfoOld = &g_PS3_XInputInfo.m_CellKbInfoOld;
	*pKbInfoOld = *pKbInfo;

	if (CELL_OK != (res = cellKbGetInfo(pKbInfo))) {
		Msg("Error%08X : cellKbGetInfo\n", res);
	}
	else
	{
		// Check info field for monitoring the INTERCEPTED state (when system grabs ownership of the keyboard data).
		bool bIntercepted = pKbInfo->info & CELL_KB_INFO_INTERCEPTED;
		if( bIntercepted &&	(!(pKbInfoOld->info & CELL_KB_INFO_INTERCEPTED)))
		{
			Msg("Lost the ownership of the keyboard data\n");
		}
		else if(!bIntercepted && (pKbInfoOld->info & CELL_KB_INFO_INTERCEPTED))
		{
			Msg("Regained ownership of the keyboard data\n");
			for(int i=0;i<PS3_MAX_KEYBOARD;i++)
			{
				cellKbClearBuf(i);
			}
		}

		if(!bIntercepted)
		{
			for(int i=0;i<PS3_MAX_KEYBOARD;i++) 
			{
				// Check for keyboard insertion and removal */
				if (pKbInfo->status[i] == CELL_KB_STATUS_CONNECTED) 
				{
					if (pKbInfoOld->status[i] == CELL_KB_STATUS_DISCONNECTED) 
					{
						Msg("Keyboard[%d] connected.\n", i);
						m_PS3KeyboardConnected = true;
						PS3_SetupKb(i);
					}
					res = cellKbRead(i, &kbData);
					if (res == CELL_KB_OK)
					{
						// Keep copy of last valid keyboard data
						if(g_PS3_XInputInfo.m_CellKbData[i].len > 0)
						{
							g_PS3_XInputInfo.m_CellKbDataOld[i] = g_PS3_XInputInfo.m_CellKbData[i];
						}
						g_PS3_XInputInfo.m_CellKbData[i] = kbData;
					}	
				}
				else if(pKbInfo->status[i] == CELL_KB_STATUS_DISCONNECTED) 
				{
					if(pKbInfoOld->status[i] == CELL_KB_STATUS_CONNECTED) 
					{
						Msg("Keyboard[%d] disconnected.\n", i);
						m_PS3KeyboardConnected = false;			// ?
					}
				}
				SetInputDeviceConnected( INPUT_DEVICE_KEYBOARD_MOUSE, ( m_PS3KeyboardConnected && m_PS3MouseConnected ) );
			}
		}
	}
}

bool HandleMoveControllerSelectButtonHack( CellPadData &data, int controllerIndex )
{
	// [dkorus] hacky solution to let steam overlay open with SELECT button from the last frame's input
	bool usingMoveControllerSelectBtn = (g_PS3_XInputInfo.m_moveControllerStateOld.m_aCellGemState[0].pad.digitalbuttons  & CELL_GEM_CTRL_SELECT);

	// need to track whether we were using the move controller select button last time we ran this code
	static bool wasUsingMoveControllerSelectBtn = false;

	if ( usingMoveControllerSelectBtn || wasUsingMoveControllerSelectBtn )
	{
		if ( data.len <= 0 )
		{
			// fill in valid data from our last successful pad sample
			data = g_PS3_XInputInfo.m_lastCellPadData[ controllerIndex ];
		}
	}

	if (  usingMoveControllerSelectBtn )
	{
		// force in a button value for select
		data.button[ CELL_PAD_BTN_OFFSET_DIGITAL1 ] |= CELL_PAD_CTRL_SELECT;

		// give a valid length (this is just the value in the data structure when SELECT is normally pressed)
		data.len = MAX(20, data.len);
	}
	else if ( wasUsingMoveControllerSelectBtn )
	{
		// force in a button value for select
		data.button[ CELL_PAD_BTN_OFFSET_DIGITAL1 ] &= ~(CELL_PAD_CTRL_SELECT);

		// give a valid length (this is just the value in the data structure when SELECT is normally pressed)
		data.len = MAX(20,data.len);
	}
	wasUsingMoveControllerSelectBtn = usingMoveControllerSelectBtn;

	return usingMoveControllerSelectBtn;
}


void CInputSystem::PS3_XInputPollEverything( BCellPadDataHook_t hookFunc, BCellPadNoDataHook_t hookNoDataFunc )
{
	memset( &g_PS3_XInputInfo.m_CellPadInfo2, 0, sizeof( g_PS3_XInputInfo.m_CellPadInfo2 ) );

	int res = cellPadGetInfo2( &g_PS3_XInputInfo.m_CellPadInfo2 );
	if ( res < CELL_OK )
	{
		if ( hookNoDataFunc )
			hookNoDataFunc();
		PS3_XInputActiveSingleControllerSet( 0 );
		return;
	}

	if ( g_PS3_XInputInfo.m_CellPadInfo2.system_info & CELL_PAD_INFO_INTERCEPTED )
	{
		// OS is intercepting all controller input
		if ( hookNoDataFunc )
			hookNoDataFunc();
		PS3_XInputActiveSingleControllerSet( 0 );
		return;
	}

	int iActiveControllerDetected = ( XBX_GetNumGameUsers() == 1 ) ? 0 : -1;
	float flCurrentTime = Plat_FloatTime();

	for ( int k = 0; k < ARRAYSIZE( g_PS3_XInputInfo.m_CellPadInfo2.port_status ); ++ k )
	{
		if ( g_PS3_XInputInfo.m_CellPadInfo2.port_status[k] & CELL_PAD_STATUS_CONNECTED )
		{
			if ( 0 == ( g_PS3_XInputInfo.m_dwPadPortSettingPressOn & ( 1 << k ) ) )
			{
				res = cellPadSetPortSetting( k, CELL_PAD_SETTING_PRESS_ON ); // Set pad to all-analog mode
				if ( res == CELL_PAD_OK )
				{
					g_PS3_XInputInfo.m_CellPadInfo2.port_setting[k] |= CELL_PAD_SETTING_PRESS_ON;
					g_PS3_XInputInfo.m_dwPadPortSettingPressOn |= ( 1 << k );
				}
			}
			
			CellPadData data;
			res = cellPadGetData( k, &data );

			// [dkorus] this func checks/adds select button data from the move controller to the CellPadData we're passing through.  Used for the steam overlay on PS3
			//			please remove when steam overlay is reworked to correctly sample move controller data!
			HandleMoveControllerSelectButtonHack( data, k );

			if ( res < CELL_OK )
			{
				// Failed to obtain data - mark ctrlr as disconnected!
				g_PS3_XInputInfo.m_CellPadInfo2.port_status[k] &=~ CELL_PAD_STATUS_CONNECTED;
			}
			else if ( data.len > 0 )
			{
				// New data obtained!

				bool bDiscard = false;
				if ( hookFunc )
				{
					bDiscard = hookFunc( data );
				}

				if ( !bDiscard )
				{
					Q_memcpy( &g_PS3_XInputInfo.m_CellPadData[k], &data, sizeof( CellPadData ) );
					++ g_PS3_XInputInfo.m_dwPacketNumber[k];

					if ( ( g_PS3_XInputInfo.m_iActiveSingleControllerIndex - 1 == k ) ||
						!iActiveControllerDetected )
						iActiveControllerDetected = k + 1;
				}
				g_PS3_XInputInfo.m_lastCellPadData[k] = data;
			}
		}

		if ( ( g_PS3_XInputInfo.m_CellPadInfo2.port_status[k] & CELL_PAD_STATUS_CONNECTED ) != CELL_PAD_STATUS_CONNECTED )
		{
			// PS3 controllers can get disconnected for brief periods of time
			// when they transition between charging and wireless modes :(
			if ( flCurrentTime - g_PS3_XInputInfo.m_flLastConnectedTime[k] > CELL_PAD_ACTIVE_CONTROL_TIME )
			{
				// Controller is treated as disconnected, reset data state
				memset( &g_PS3_XInputInfo.m_CellPadData[k], 0, sizeof( CellPadData ) );
			}
			else
			{
				// Pretend like it's still connected, just no state changes
				g_PS3_XInputInfo.m_CellPadInfo2.port_status[k] |= CELL_PAD_STATUS_CONNECTED;
			}
		}
		else
		{
			// Remember when this controller was last connected
			g_PS3_XInputInfo.m_flLastConnectedTime[k] = flCurrentTime;
		}
	}

	if(g_pMoveController->m_bEnabled)
	{
		g_PS3_XInputInfo.m_moveControllerStateOld = g_PS3_XInputInfo.m_moveControllerState;
		g_pMoveController->ReadState(&g_PS3_XInputInfo.m_moveControllerState);


	}

	PS3_PollMouse();
	PS3_PollKeyboard();

	if ( iActiveControllerDetected )
	{
		PS3_XInputActiveSingleControllerSet( iActiveControllerDetected );
	}
}

// [dkorus] mode masks  and button masks for sharpshooter straight from sharpshooter sdk demo
#define CUSTOM0_MODE_1 (1 << 0)
#define CUSTOM0_MODE_2 (1 << 1)
#define CUSTOM0_MODE_3 (1 << 2)
#define CUSTOM0_MODE_MASK (CUSTOM0_MODE_1 | CUSTOM0_MODE_2 | CUSTOM0_MODE_3)
#define CUSTOM0_T_BUTTON_TRIGGER   (1 << 6)
#define CUSTOM0_RL_RELOAD_BUTTON   (1 << 7)
class CSharpshooterButtonData
{
	public:
		CSharpshooterButtonData() 
		{
			prev_mode = 0;
			m_reloadPressedLastFrame = false;
			m_triggerPressedLastFrame = false;
			m_pumpActionPressedLastFrame = false;
		}
		unsigned char prev_mode;
		bool m_reloadPressedLastFrame;
		bool m_triggerPressedLastFrame;
		bool m_pumpActionPressedLastFrame;
};
CSharpshooterButtonData g_sharpshooterData;


void CInputSystem::HandlePS3SharpshooterButtons( void )
{
	// we have a sharpshooter, read in the special buttons
	
	static const unsigned char num_dots_lookup[8] = {0, 1, 2, 0, 3, 0, 0, 0};
	unsigned char mode = num_dots_lookup[g_PS3_XInputInfo.m_moveControllerState.m_aCellGemState[0].ext.custom[0] & CUSTOM0_MODE_MASK];
	
	if ( mode != g_sharpshooterData.prev_mode )
	{
		// fire mode selector changed
		ButtonCode_t prev_mode_button = KEY_XBUTTON_FIREMODE_SELECTOR_1;
		switch ( g_sharpshooterData.prev_mode )
		{
			case 1:
				prev_mode_button = KEY_XBUTTON_FIREMODE_SELECTOR_1;
				break;
			case 2:
				prev_mode_button = KEY_XBUTTON_FIREMODE_SELECTOR_2;
				break;
			case 3:
				prev_mode_button = KEY_XBUTTON_FIREMODE_SELECTOR_3;
				break;
				default: Msg( "Button prev_mode %d unhandled\n",g_sharpshooterData.prev_mode );
		};
		PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, prev_mode_button, prev_mode_button );

		ButtonCode_t new_mode_button = KEY_XBUTTON_FIREMODE_SELECTOR_1;
		switch ( mode )
		{
			case 1:
				new_mode_button = KEY_XBUTTON_FIREMODE_SELECTOR_1;
				break;
			case 2:
				new_mode_button = KEY_XBUTTON_FIREMODE_SELECTOR_2;
				break;
			case 3:
				new_mode_button = KEY_XBUTTON_FIREMODE_SELECTOR_3;
				break;
			default: Msg( "Button mode %d unhandled\n",mode );
		};		
		PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, new_mode_button, new_mode_button );

		g_sharpshooterData.prev_mode = mode;
	}


	if ( g_PS3_XInputInfo.m_moveControllerState.m_aCellGemState[0].ext.custom[0] & CUSTOM0_RL_RELOAD_BUTTON )
	{
		// reload button pressed
		if ( !g_sharpshooterData.m_reloadPressedLastFrame )
		{
			PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, KEY_XBUTTON_RELOAD, KEY_XBUTTON_RELOAD );
		}
		g_sharpshooterData.m_reloadPressedLastFrame = true;
	}
	else 
	{
		if ( g_sharpshooterData.m_reloadPressedLastFrame )
		{
			PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, KEY_XBUTTON_RELOAD, KEY_XBUTTON_RELOAD );
		}

		g_sharpshooterData.m_reloadPressedLastFrame = false;
	}


	if ( g_PS3_XInputInfo.m_moveControllerState.m_aCellGemState[0].ext.custom[0] & CUSTOM0_T_BUTTON_TRIGGER ) // custom T button trigger takes priority over actual Move T button
	{
		// trigger pressed
		if ( !g_sharpshooterData.m_triggerPressedLastFrame )
		{
			if( m_setCurrentInputDeviceOnNextButtonPress )
			{
				// [dkorus] since this is a move specific button, it's either triggered by the move or the sharpshooter
				if ( IsInputDeviceConnected( INPUT_DEVICE_SHARPSHOOTER ) )
					SetCurrentInputDevice( INPUT_DEVICE_SHARPSHOOTER );
				else
					SetCurrentInputDevice( INPUT_DEVICE_PLAYSTATION_MOVE );
			
				m_setCurrentInputDeviceOnNextButtonPress = false;
			}

			PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, KEY_XBUTTON_TRIGGER, KEY_XBUTTON_TRIGGER );
		}
		g_sharpshooterData.m_triggerPressedLastFrame = true;
	}
	else 
	{
		if ( g_sharpshooterData.m_triggerPressedLastFrame )
		{
			PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, KEY_XBUTTON_TRIGGER, KEY_XBUTTON_TRIGGER );
		}
		g_sharpshooterData.m_triggerPressedLastFrame = false;

		if ( g_PS3_XInputInfo.m_moveControllerState.m_aCellGemState[0].pad.digitalbuttons & CELL_GEM_CTRL_T )
		{
			if ( !g_sharpshooterData.m_pumpActionPressedLastFrame )
			{
				// pump action grip pulled
				PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, KEY_XBUTTON_PUMP_ACTION, KEY_XBUTTON_PUMP_ACTION );
			}
			g_sharpshooterData.m_pumpActionPressedLastFrame = true;
		}
		else
		{
			if ( g_sharpshooterData.m_pumpActionPressedLastFrame ) 
			{
				PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, KEY_XBUTTON_PUMP_ACTION, KEY_XBUTTON_PUMP_ACTION );
			}
			g_sharpshooterData.m_pumpActionPressedLastFrame = false;
		}

		
	}
}


void CInputSystem::HandlePS3Move( PXINPUT_STATE& pState )
{
	// handling controller buttons
	CellGemPadData gemPadDataOld = g_PS3_XInputInfo.m_moveControllerStateOld.m_aCellGemState[0].pad;
	CellGemPadData gemPadData = g_PS3_XInputInfo.m_moveControllerState.m_aCellGemState[0].pad;

	if ( IsDeviceReadingInput( INPUT_DEVICE_SHARPSHOOTER ) && IsInputDeviceConnected( INPUT_DEVICE_SHARPSHOOTER ) )
	{
		HandlePS3SharpshooterButtons( );
	}

	// check for queued rumble
	if ( g_PS3_XInputInfo.ps3_move_rumble_queued )
	{
		g_pMoveController->Rumble( g_PS3_XInputInfo.ps3_move_rumble_value );
		g_PS3_XInputInfo.ps3_move_rumble_queued = false;
	}

	if (gemPadData.digitalbuttons & CELL_GEM_CTRL_START)
		pState->Gamepad.wButtons |= XINPUT_GAMEPAD_START;
	if (gemPadData.digitalbuttons & CELL_GEM_CTRL_SELECT)
		pState->Gamepad.wButtons |= XINPUT_GAMEPAD_BACK;

	if ( (gemPadData.digitalbuttons & CELL_GEM_CTRL_CROSS) ||
		( gemPadData.analog_T>0 && !g_sharpshooterData.m_pumpActionPressedLastFrame ) )
	{
		if( m_setCurrentInputDeviceOnNextButtonPress )
		{
			if ( IsInputDeviceConnected( INPUT_DEVICE_SHARPSHOOTER ) )
				SetCurrentInputDevice( INPUT_DEVICE_SHARPSHOOTER );
			else
				SetCurrentInputDevice( INPUT_DEVICE_PLAYSTATION_MOVE );
		
			m_setCurrentInputDeviceOnNextButtonPress = false;
		}

	}

	if ( ( gemPadData.digitalbuttons & CELL_GEM_CTRL_CROSS ) )
		pState->Gamepad.wButtons |= XINPUT_GAMEPAD_A;
	if (gemPadData.digitalbuttons & CELL_GEM_CTRL_CIRCLE)
		pState->Gamepad.wButtons |= XINPUT_GAMEPAD_B;
	if (gemPadData.digitalbuttons & CELL_GEM_CTRL_TRIANGLE)
		pState->Gamepad.wButtons |= XINPUT_GAMEPAD_Y;
	if (gemPadData.digitalbuttons & CELL_GEM_CTRL_SQUARE)
		pState->Gamepad.wButtons |= XINPUT_GAMEPAD_X;

	if ( !IsInputDeviceConnected( INPUT_DEVICE_SHARPSHOOTER ) )
	{
		if ( gemPadData.analog_T>0 && !g_sharpshooterData.m_pumpActionPressedLastFrame )
		{
			if ( gemPadDataOld.analog_T <= 0 )
			{
				PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, KEY_XBUTTON_TRIGGER, KEY_XBUTTON_TRIGGER );
			}
		}
		else if ( gemPadDataOld.analog_T > 0)
		{
			PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, KEY_XBUTTON_TRIGGER, KEY_XBUTTON_TRIGGER );
		}
	}

	gemPadDataOld.analog_T = gemPadData.analog_T;

	// Get roll from Quaternion
	Quaternion qMc = Quaternion(-g_PS3_XInputInfo.m_moveControllerState.m_aCellGemState[0].quat[2],
		-g_PS3_XInputInfo.m_moveControllerState.m_aCellGemState[0].quat[0],
		g_PS3_XInputInfo.m_moveControllerState.m_aCellGemState[0].quat[1],
		g_PS3_XInputInfo.m_moveControllerState.m_aCellGemState[0].quat[3]);
	float fRoll = 0.0f;
	float m23 = ( 2.0f * qMc.y * qMc.z ) + ( 2.0f * qMc.w * qMc.x );
	float m33 = ( 2.0f * qMc.w * qMc.w ) + ( 2.0f * qMc.z * qMc.z ) - 1.0f;
	if(!(m23==0 && m33==0))
	{
		fRoll = RAD2DEG(atan2(m23,m33));
	}

	float fRollTrigger = ps3_move_roll_trigger.GetFloat();
	if ( fRoll > fRollTrigger )
	{
		if ( g_PS3_XInputInfo.ps3_move_roll_old <= fRollTrigger )
		{
			PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, KEY_XBUTTON_ROLL_RIGHT, KEY_XBUTTON_ROLL_RIGHT );		
		}
	}
	else if ( g_PS3_XInputInfo.ps3_move_roll_old > fRollTrigger )
	{
		PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, KEY_XBUTTON_ROLL_RIGHT, KEY_XBUTTON_ROLL_RIGHT );
	}

	if(fRoll < (fRollTrigger*-1.0f))
	{
		if ( g_PS3_XInputInfo.ps3_move_roll_old >= ( fRollTrigger * -1.0f ) )
		{
			PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, KEY_XBUTTON_ROLL_LEFT, KEY_XBUTTON_ROLL_LEFT );
		}
	}
	else if ( g_PS3_XInputInfo.ps3_move_roll_old < ( fRollTrigger * -1.0f ) )
	{
		PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, KEY_XBUTTON_ROLL_LEFT, KEY_XBUTTON_ROLL_LEFT );
	}

	g_PS3_XInputInfo.ps3_move_roll_old = fRoll;
}


DWORD CInputSystem::PS3_XInputGetState(
					 DWORD dwUserIndex,
					 PXINPUT_STATE pState
					 )
{
	if ( !( g_PS3_XInputInfo.m_CellPadInfo2.port_status[dwUserIndex] & CELL_PAD_STATUS_CONNECTED ) )
		return ERROR_DEVICE_NOT_CONNECTED;
	
	CellPadData const padData = g_PS3_XInputInfo.m_CellPadData[ dwUserIndex ];

	// Convert cellPad state to Valve (x360) gamepad state 

	memset(&pState->Gamepad.wButtons, 0, sizeof(pState->Gamepad.wButtons));

	
	if ( !IsDeviceReadingInput( INPUT_DEVICE_GAMEPAD ) && 
		 !IsDeviceReadingInput( INPUT_DEVICE_PLAYSTATION_MOVE ) && 
		 !IsDeviceReadingInput( INPUT_DEVICE_SHARPSHOOTER ) &&
		 !IsDeviceReadingInput( INPUT_DEVICE_MOVE_NAV_CONTROLLER ) )
	{
		return ERROR_SUCCESS;
	}

	// Analog-only cell pad buttons

	//	pState->Gamepad.sThumbLX = (short)( ( (unsigned int) ( ( ( short) padData.button[ CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X ] ) << 8 ) - 1 ) - 0x7fff );
	//	pState->Gamepad.sThumbLY = (short)( ( (unsigned int) ( ( ( short) padData.button[ CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y ] ) << 8 ) - 1 ) - 0x7fff );
	//	pState->Gamepad.sThumbRX = (short)( ( (unsigned int) ( ( ( short) padData.button[ CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X ] ) << 8 ) - 1 ) - 0x7fff );
	//	pState->Gamepad.sThumbRY = (short)( ( (unsigned int) ( ( ( short) padData.button[ CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y ] ) << 8 ) - 1 ) - 0x7fff );

	int lx,ly,rx,ry;

	lx = padData.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X] &0xff;
	ly = padData.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y] &0xff;
	rx = 0;
	ry = 0;

	// don't use right stick input when using move controller.
	if ( IsDeviceReadingInput( INPUT_DEVICE_GAMEPAD ) || IsDeviceReadingInput( INPUT_DEVICE_MOVE_NAV_CONTROLLER ) )
	{
		rx = padData.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X] &0xff;
		ry = padData.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y] &0xff;
		rx = (rx * 65535) / 255;
		ry = (ry * 65535) / 255;
		rx -= 0x8000;
		ry = 0x7FFF - ry;
	}

	lx = (lx * 65535) / 255;
	ly = (ly * 65535) / 255;
	lx -= 0x8000;
	ly = 0x7FFF - ly;

	pState->Gamepad.sThumbLX = (signed short)lx;
	pState->Gamepad.sThumbLY = (signed short)ly;
	pState->Gamepad.sThumbRX = (signed short)rx;
	pState->Gamepad.sThumbRY = (signed short)ry;


	// Digital-only cell pad buttons

	if (padData.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_START)
		pState->Gamepad.wButtons |= XINPUT_GAMEPAD_START;
	if (padData.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_SELECT)
		pState->Gamepad.wButtons |= XINPUT_GAMEPAD_BACK;
	if (padData.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_L3)
		pState->Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
	if (padData.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_R3)
		pState->Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;

	// Buttons which can be either analog or digital

	bool bAllAnalog = !!( g_PS3_XInputInfo.m_CellPadInfo2.port_setting[ dwUserIndex ] & CELL_PAD_SETTING_PRESS_ON );
	if (bAllAnalog)
	{
		if (padData.button[CELL_PAD_BTN_OFFSET_PRESS_LEFT] > CELL_PAD_DEADZONE_PROPORTION)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
		if (padData.button[CELL_PAD_BTN_OFFSET_PRESS_UP] > CELL_PAD_DEADZONE_PROPORTION)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_UP;
		if (padData.button[CELL_PAD_BTN_OFFSET_PRESS_DOWN] > CELL_PAD_DEADZONE_PROPORTION)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
		if (padData.button[CELL_PAD_BTN_OFFSET_PRESS_RIGHT] > CELL_PAD_DEADZONE_PROPORTION)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;


		if (padData.button[CELL_PAD_BTN_OFFSET_PRESS_CROSS] > CELL_PAD_DEADZONE_PROPORTION)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_A;
		if (padData.button[CELL_PAD_BTN_OFFSET_PRESS_CIRCLE] > CELL_PAD_DEADZONE_PROPORTION)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_B;
		if (padData.button[CELL_PAD_BTN_OFFSET_PRESS_TRIANGLE] > CELL_PAD_DEADZONE_PROPORTION)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_Y;
		if (padData.button[CELL_PAD_BTN_OFFSET_PRESS_SQUARE] > CELL_PAD_DEADZONE_PROPORTION)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_X;

		if (padData.button[CELL_PAD_BTN_OFFSET_PRESS_L1] > CELL_PAD_DEADZONE_PROPORTION)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
		if (padData.button[CELL_PAD_BTN_OFFSET_PRESS_R1] > CELL_PAD_DEADZONE_PROPORTION)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;


		pState->Gamepad.bLeftTrigger = padData.button[CELL_PAD_BTN_OFFSET_PRESS_L2];
		pState->Gamepad.bRightTrigger = padData.button[CELL_PAD_BTN_OFFSET_PRESS_R2];
	}

	else
	{
		if (padData.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_LEFT)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
		if (padData.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_UP)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_UP;
		if (padData.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_DOWN)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
		if (padData.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_RIGHT)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;


		if (padData.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CROSS)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_A;
		if (padData.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CIRCLE)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_B;
		if (padData.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_TRIANGLE)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_Y;
		if (padData.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_SQUARE)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_X;
		if (padData.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L1)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_LEFT_SHOULDER;
		if (padData.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R1)
			pState->Gamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_SHOULDER;


		if (padData.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L2)
			pState->Gamepad.bLeftTrigger = 0xFF;
		else
			pState->Gamepad.bLeftTrigger = 0x00;

		if (padData.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R2)
		{
			pState->Gamepad.bRightTrigger = 0xFF;
		}
		else
		{
			pState->Gamepad.bRightTrigger = 0x00;
		}
	}

	if ( g_PS3_XInputInfo.m_bInputJapaneseSwapAB )
	{
		pState->Gamepad.wButtons
			=
			( pState->Gamepad.wButtons & ~( XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B ) )
				|
			( ( pState->Gamepad.wButtons & XINPUT_GAMEPAD_A ) ? XINPUT_GAMEPAD_B : 0 )
				|
			( ( pState->Gamepad.wButtons & XINPUT_GAMEPAD_B ) ? XINPUT_GAMEPAD_A : 0 );
	}

	pState->dwPacketNumber = g_PS3_XInputInfo.m_dwPacketNumber[dwUserIndex];

	// Add motion controller button states (overwrite gamepad data)

	// If motion controller connected and calibrated...
	if( g_pMoveController->m_bEnabled &&
		(g_PS3_XInputInfo.m_moveControllerState.m_CellGemInfo.status[0]==CELL_GEM_STATUS_READY) &&
		(g_PS3_XInputInfo.m_moveControllerState.m_aStatus[0] == CELL_OK) &&
		( IsDeviceReadingInput( INPUT_DEVICE_PLAYSTATION_MOVE ) || IsDeviceReadingInput( INPUT_DEVICE_SHARPSHOOTER ) ) )
	{
		// [dkorus] pState passed by reference for ease of use, it may change inside of this func
		HandlePS3Move( pState );
	}

	return ERROR_SUCCESS;
}

DWORD PS3_XInputSetState(
					 DWORD dwUserIndex,
					 PXINPUT_VIBRATION pVibration
					 )
{
	CellPadActParam param;
	memset( &param, 0, sizeof( param ) );
	
	if ( XBX_GetNumGameUsers() )
	{
		if ( g_PS3_XInputInfo.m_iActiveSingleControllerIndex > 0 )
			// single player vibration override
			dwUserIndex = g_PS3_XInputInfo.m_iActiveSingleControllerIndex - 1;
		param.motor[0] = !!pVibration->wLeftMotorSpeed;
		param.motor[1] = pVibration->wRightMotorSpeed;
	}

	cellPadSetActDirect( dwUserIndex, &param );

	return ERROR_SUCCESS;
}

DWORD PS3_XInputGetCapabilities( DWORD dwUserIndex, DWORD dwFlags, PXINPUT_CAPABILITIES pCapabilities )
{
	// [dkorus] in the future it would make sense to move PS3 device information here
	//			note, this might be complicated for some devices as some input types have their capabilitys change on the fly
	//			for exmaple, a standard controller has rumble, but it's disabled if the move controller is set to the 
	//			current controller.
	pCapabilities->Type = XINPUT_DEVTYPE_GAMEPAD;
	pCapabilities->SubType =  XINPUT_DEVSUBTYPE_GAMEPAD;
	pCapabilities->Flags = 0;

	return ( g_PS3_XInputInfo.m_CellPadInfo2.port_status[dwUserIndex] & CELL_PAD_STATUS_CONNECTED ) ? ERROR_SUCCESS : ERROR_DEVICE_NOT_CONNECTED;
}

void PS3_XInputShutdown()
{
	cellPadEnd();
    g_pMoveController->Shutdown();
	cellMouseEnd();
	cellKbEnd();
}

//--------------------------------------------------------------------------------------------------
// PS3_InitMouse
// Init the PS3 mouse library
//--------------------------------------------------------------------------------------------------
static int PS3_InitMouse()
{
	int ret = cellMouseInit(PS3_MAX_MOUSE);

	if (ret != CELL_MOUSE_OK) 
	{
		Msg("Error(%08X) : PS3 cellMouseInit error\n", ret);
		return (ret);
	}
	return (CELL_OK);
}

//--------------------------------------------------------------------------------------------------
// PS3_SetupKb
// Setup initial keyboard state
//--------------------------------------------------------------------------------------------------
static int PS3_SetupKb(int i)
{
	int ret = cellKbSetLEDStatus(i, CELL_KB_LED_NUM_LOCK | CELL_KB_LED_CAPS_LOCK | CELL_KB_LED_SCROLL_LOCK );
	if (ret	!= CELL_KB_OK) 
	{
		Msg("Error(%08X) : cellKbSetLEDStatus, kb no = %d\n", ret, i);
	}
	ret = cellKbSetReadMode(i, CELL_KB_RMODE_PACKET);
	if (ret	!= CELL_KB_OK) 
	{
		Msg("Error(%08X) : cellKbSetReadMode, kb no = %d\n", ret, i);
		cellKbEnd();
		return (ret);
	}
	ret = cellKbSetCodeType(i, CELL_KB_CODETYPE_RAW);
	if (ret	!= CELL_KB_OK) 
	{
		Msg("Error(%08X) : cellKbSetCodeType, kb no = %d\n", ret, i);
		cellKbEnd();
		return (ret);
	}
	ret = cellKbClearBuf(i);
	if (ret	!= CELL_KB_OK) 
	{
		Msg("Error(%08X) : cellKbClearBuf, kb no = %d\n", ret, i);
		cellKbEnd();
		return (ret);
	}
	return ret;
}

//--------------------------------------------------------------------------------------------------
// PS3_InitKb
// Init the PS3 keyboard library
//--------------------------------------------------------------------------------------------------
static int PS3_InitKb()
{
	int ret = cellKbInit(PS3_MAX_KEYBOARD);

	if (ret != CELL_KB_OK) 
	{
		Msg("Error(%08X) : PS3 cellKbInit error\n", ret);
		return (ret);
	}

	CellKbInfo kbInfo;
	ret = cellKbGetInfo(&kbInfo);
	if (ret != CELL_KB_OK) 
	{
		Msg("Error%08X : cellKbGetInfo\n", ret);
		return (ret);
	}

	if(!(kbInfo.info & CELL_KB_INFO_INTERCEPTED))
	{
		for (int i = 0; i < PS3_MAX_KEYBOARD; i++) 
		{
			if(kbInfo.status[i]==CELL_KB_STATUS_CONNECTED)
			{
				PS3_SetupKb(i);
			}
		}
	}
	return (CELL_OK);
}

#endif

//-----------------------------------------------------------------------------
//	Purpose: Initialize all Xbox controllers
//-----------------------------------------------------------------------------
void CInputSystem::InitializeXDevices( void )
{
	bool bInputSwapAB = false;
#ifdef PLATFORM_PS3

	memset( &g_PS3_XInputInfo, 0, sizeof( g_PS3_XInputInfo ) );

	cellPadInit( XUSER_MAX_COUNT );

	int bEnterAssignment = CELL_SYSUTIL_ENTER_BUTTON_ASSIGN_CROSS; // default to western standard assignment
	if ( CELL_OK == cellSysutilGetSystemParamInt( CELL_SYSUTIL_SYSTEMPARAM_ID_ENTER_BUTTON_ASSIGN, &bEnterAssignment ) &&
		bEnterAssignment == CELL_SYSUTIL_ENTER_BUTTON_ASSIGN_CIRCLE )
	{
		g_PS3_XInputInfo.m_bInputJapaneseSwapAB = true;
		bInputSwapAB = true;
	}

	// Init Move Controller
	g_pMoveController->Init();

	// Init mouse lib
	PS3_InitMouse();

	// Init keyboard lib
	PS3_InitKb();

#endif
	KeyValuesSystem()->SetKeyValuesExpressionSymbol( "INPUTSWAPAB", bInputSwapAB );

	int					i;
	xdevice_t*			pXDevice;

	// assume no joystick
	m_nJoystickCount = 0; 

#if !defined( _GAMECONSOLE )
	PC_XInputGetState = (XInputGetState_t)GetProcAddress( (HMODULE)m_pXInputDLL, "XInputGetState" );
	PC_XInputSetState = (XInputSetState_t)GetProcAddress( (HMODULE)m_pXInputDLL, "XInputSetState" );
	PC_XInputGetCapabilities = (XInputGetCapabilities_t)GetProcAddress( (HMODULE)m_pXInputDLL, "XInputGetCapabilities" );
	if ( !PC_XInputGetState || !PC_XInputSetState || !PC_XInputGetCapabilities )
		return;
#endif

	// query gamepads
	pXDevice = m_XDevices;
	for ( i = 0; i < XUSER_MAX_COUNT; ++i, ++pXDevice )
	{
		OpenXDevice( pXDevice, i );
		//Msg( "UserID %d: %s\n", i+1, pXDevice->userId != INVALID_USER_ID ? "GamePad" : "???" );
	}
}
bool PS3IsNavController( int userId )
{
#if defined( _PS3 )
		CellPadPeriphInfo padPeriphInfo;
		int ret = cellPadPeriphGetInfo( &padPeriphInfo);
		if ( ret == CELL_PAD_OK && padPeriphInfo.pclass_type[ userId ] == CELL_PAD_PCLASS_TYPE_NAVIGATION )
			return true;
#endif
		return false;
}

//-----------------------------------------------------------------------------
//	Purpose: Open an Xbox controller
//-----------------------------------------------------------------------------
void CInputSystem::OpenXDevice( xdevice_t* pXDevice, int userId )
{
	XINPUT_CAPABILITIES	capabilities;

	// Invalidate device properties
	pXDevice->userId = INVALID_USER_ID;
	pXDevice->active = false;

	DWORD result = XINPUTGETCAPABILITIES( userId, XINPUT_FLAG_GAMEPAD, &capabilities );
	if ( result == ERROR_SUCCESS )
	{
		bool bIsSupported = false;
		if ( IsGameConsole() )
		{
			// TCR says that we cannot restrict input based on subtype, so don't check it
			bIsSupported = ( capabilities.Type == XINPUT_DEVTYPE_GAMEPAD );
		}
		else
		{
			// Current version of XInput mistakenly returns 0 as the Type. Ignore it and ensure the subtype is a gamepad.
			bIsSupported = ( capabilities.SubType == XINPUT_DEVSUBTYPE_GAMEPAD );
		}
		if ( !bIsSupported )
		{
			// TBD: This may not be sufficient to not crash us later
			Assert( 0 && "Unsupported XDevice Type" );
			return;
		}
#if defined ( _PS3 )
		if ( PS3IsNavController( userId ) )
		{
			SetInputDeviceConnected( INPUT_DEVICE_MOVE_NAV_CONTROLLER );
		}
		else
#endif
		{
			SetInputDeviceConnected( INPUT_DEVICE_GAMEPAD );
		}

		// valid
		pXDevice->type		  = capabilities.Type;
		pXDevice->subtype	  = capabilities.SubType;
		pXDevice->flags		  = capabilities.Flags;
		pXDevice->userId      = userId;
		pXDevice->active	  = true;
		pXDevice->quitTimeout = 0;
		pXDevice->dpadLock    = 0;

		// left stick, default to narrow zone
		pXDevice->stickThreshold[STICK1_AXIS_X]  = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
		pXDevice->stickScale[STICK1_AXIS_X]      = XBX_STICK_SCALE_LEFT( XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE );
		pXDevice->stickThreshold[STICK1_AXIS_Y]  = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
		pXDevice->stickScale[STICK1_AXIS_Y]      = XBX_STICK_SCALE_DOWN( XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE );
		
		// right stick, default to narrow zone
		pXDevice->stickThreshold[STICK2_AXIS_X]  = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
		pXDevice->stickScale[STICK2_AXIS_X]      = XBX_STICK_SCALE_LEFT( XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE );
		pXDevice->stickThreshold[STICK2_AXIS_Y]  = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
		pXDevice->stickScale[STICK2_AXIS_Y]      = XBX_STICK_SCALE_DOWN( XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE );
		
		pXDevice->vibration.wLeftMotorSpeed = WORD( 0 );
		pXDevice->vibration.wRightMotorSpeed = WORD( 0 );
		pXDevice->pendingRumbleUpdate = false;
		++m_nJoystickCount;
	}
}


//-----------------------------------------------------------------------------
//	Purpose: Close an Xbox controller
//-----------------------------------------------------------------------------
void CInputSystem::CloseXDevice( xdevice_t* pXDevice )
{
	pXDevice->userId = INVALID_USER_ID;
	pXDevice->active = false;
	--m_nJoystickCount;
}

//-----------------------------------------------------------------------------
//	Purpose: Sample the console mouse (currently only implemented for PS3).
//-----------------------------------------------------------------------------
void CInputSystem::PollXMouse()
{
#ifdef _PS3
	if( (!(g_PS3_XInputInfo.m_CellMouseInfo.info & CELL_MOUSE_INFO_INTERCEPTED)) &&
		(g_PS3_XInputInfo.m_CellMouseInfo.status[0] == CELL_MOUSE_STATUS_CONNECTED) &&
		(g_PS3_XInputInfo.m_CellMouseData[0].update == CELL_MOUSE_DATA_UPDATE) &&
		IsDeviceReadingInput( INPUT_DEVICE_KEYBOARD_MOUSE ) )
	{
		int oldMouseX = m_mouseRawAccumX;
		int oldMouseY = m_mouseRawAccumY;
		m_mouseRawAccumX += g_PS3_XInputInfo.m_CellMouseData[0].x_axis;
		m_mouseRawAccumY += g_PS3_XInputInfo.m_CellMouseData[0].y_axis;

		// handle positional values

		bool bXChanged = ( m_mouseRawAccumX != oldMouseX );
		bool bYChanged = ( m_mouseRawAccumY != oldMouseY );

		if ( bXChanged )
		{
			PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_X, m_mouseRawAccumX, 0 );
		}
		if ( bYChanged )
		{
			PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_Y, m_mouseRawAccumY, 0 );
		}
		if ( bXChanged || bYChanged )
		{
			PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, MOUSE_XY, m_mouseRawAccumX, m_mouseRawAccumY );
		}

		// handle buttons

		if((g_PS3_XInputInfo.m_CellMouseData[0].buttons & PS3_MOUSE_LEFT) &&
			!(g_PS3_XInputInfo.m_CellMouseDataOld[0].buttons & PS3_MOUSE_LEFT))
		{
			PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, MOUSE_LEFT, MOUSE_LEFT );
			
			// [dkorus] check whether we're trying to set the current controller
			if( m_setCurrentInputDeviceOnNextButtonPress )
			{
				if( IsInputDeviceConnected( INPUT_DEVICE_KEYBOARD_MOUSE ) )
				{
					SetCurrentInputDevice( INPUT_DEVICE_KEYBOARD_MOUSE );
					m_setCurrentInputDeviceOnNextButtonPress = false;
				}
			}
		}

		if((!(g_PS3_XInputInfo.m_CellMouseData[0].buttons & PS3_MOUSE_LEFT)) &&
			(g_PS3_XInputInfo.m_CellMouseDataOld[0].buttons & PS3_MOUSE_LEFT))
		{
			PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, MOUSE_LEFT, MOUSE_LEFT );
		}

		if((g_PS3_XInputInfo.m_CellMouseData[0].buttons & PS3_MOUSE_RIGHT) &&
			!(g_PS3_XInputInfo.m_CellMouseDataOld[0].buttons & PS3_MOUSE_RIGHT))
		{
			PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, MOUSE_RIGHT, MOUSE_RIGHT );
		}

		if((!(g_PS3_XInputInfo.m_CellMouseData[0].buttons & PS3_MOUSE_RIGHT)) &&
			(g_PS3_XInputInfo.m_CellMouseDataOld[0].buttons & PS3_MOUSE_RIGHT))
		{
			PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, MOUSE_RIGHT, MOUSE_RIGHT );
		}

		if((g_PS3_XInputInfo.m_CellMouseData[0].buttons & PS3_MOUSE_WHEEL) &&
			!(g_PS3_XInputInfo.m_CellMouseDataOld[0].buttons & PS3_MOUSE_WHEEL))
		{
			PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, MOUSE_MIDDLE, MOUSE_MIDDLE );
		}

		if((!(g_PS3_XInputInfo.m_CellMouseData[0].buttons & PS3_MOUSE_WHEEL)) &&
			(g_PS3_XInputInfo.m_CellMouseDataOld[0].buttons & PS3_MOUSE_WHEEL))
		{
			PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, MOUSE_MIDDLE, MOUSE_MIDDLE );
		}

		if(g_PS3_XInputInfo.m_CellMouseData[0].wheel > 0)
		{
			PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, MOUSE_WHEEL_UP, MOUSE_WHEEL_UP);
			PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, MOUSE_WHEEL_UP, MOUSE_WHEEL_UP);
		}
		else if(g_PS3_XInputInfo.m_CellMouseData[0].wheel < 0)
		{
			PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, MOUSE_WHEEL_DOWN, MOUSE_WHEEL_DOWN);
			PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, MOUSE_WHEEL_DOWN, MOUSE_WHEEL_DOWN);
		}
	}
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Sample the console keyboard (currently only implemented for PS3).
//-----------------------------------------------------------------------------
void CInputSystem::PollXKeyboard()
{
#ifdef _PS3
	if( (!(g_PS3_XInputInfo.m_CellKbInfo.info & CELL_KB_INFO_INTERCEPTED)) &&
		(g_PS3_XInputInfo.m_CellKbInfo.status[0] == CELL_KB_STATUS_CONNECTED) &&
		(g_PS3_XInputInfo.m_CellKbData[0].len > 0) && 
		IsDeviceReadingInput( INPUT_DEVICE_KEYBOARD_MOUSE ) )
	{
		CBitVec<256> keysPressed;
		keysPressed.Init(0);

		// Check for key presses
		for(int i=0; i<g_PS3_XInputInfo.m_CellKbData[0].len; ++i)
		{
			int iKeycode = g_PS3_XInputInfo.m_CellKbData[0].keycode[i] & 0x00ff;
			ButtonCode_t virtualCode = ButtonCode_VirtualKeyToButtonCode( iKeycode );

			if ( iKeycode != 0 )
			{
				keysPressed.Set( iKeycode );
				// TODO:: Get scan code (see CInputSystem::WindowProc)
				PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, virtualCode, virtualCode );
			}
			// [dkorus] check whether we're trying to set the current controller
			if( ( virtualCode & KEY_SPACE || virtualCode & KEY_ENTER )
				&& m_setCurrentInputDeviceOnNextButtonPress )
			{
				if( IsInputDeviceConnected( INPUT_DEVICE_KEYBOARD_MOUSE ) )
				{
					SetCurrentInputDevice( INPUT_DEVICE_KEYBOARD_MOUSE );
					m_setCurrentInputDeviceOnNextButtonPress = false;
				}
			}

			// TODO:: Deal with capslock/scrolllock/numlock (see CInputSystem::WindowProc)
			// TODO:: IE_KeyCodeTyped events (see CInputSystem::WindowProc)
		}

		// Check for key releases
		for(int i=0; i<g_PS3_XInputInfo.m_CellKbDataOld[0].len; ++i)
		{
			int iKeycode = g_PS3_XInputInfo.m_CellKbDataOld[0].keycode[i] & 0x00ff;
			if ( !keysPressed.IsBitSet(iKeycode) )
			{
				ButtonCode_t virtualCode = ButtonCode_VirtualKeyToButtonCode( iKeycode );
				PostButtonReleasedEvent(IE_ButtonReleased, m_nLastSampleTick, virtualCode, virtualCode);
			}
		}

		// Shift/CTRL/Alt
		int iMkey = g_PS3_XInputInfo.m_CellKbData[0].mkey;
		int iMkeyOld = g_PS3_XInputInfo.m_CellKbDataOld[0].mkey;

		for(int i=0; i<ARRAYSIZE(s_ps3ModifierTable); ++i)
		{
			if(iMkey & s_ps3ModifierTable[i].modifier)
			{
				PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, s_ps3ModifierTable[i].buttonCode, s_ps3ModifierTable[i].buttonCode );
			}
			else if(iMkeyOld & s_ps3ModifierTable[i].modifier)
			{
				PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, s_ps3ModifierTable[i].buttonCode, s_ps3ModifierTable[i].buttonCode );
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------------
//	Purpose: Sample the Xbox controllers.
//-----------------------------------------------------------------------------
void CInputSystem::PollXDevices( void )
{
	float flCheckDelay = (m_bIsInGame && !m_bXController) ? 20 : 0.5;
	if ( m_bXController )
		flCheckDelay = 0;

	static ConVarRef joystick_force_disabled( "joystick_force_disabled" );
	if ( joystick_force_disabled.IsValid() && joystick_force_disabled.GetBool() )
	{
		if ( m_bXController )
		{
			xdevice_t* pXDevice = m_XDevices;
			m_bXController = false;
			for ( int userId = 0; userId < XUSER_MAX_COUNT; ++userId, ++pXDevice )
			{
				// Get input data in the form of Xbox controller data (for PS3, map onto Xbox struct)
				DWORD result = XINPUTGETSTATE( userId, &pXDevice->states[pXDevice->newState] );
				if ( result == ERROR_SUCCESS )
				{
					ReleaseAllButtons( JOYSTICK_BUTTON_INTERNAL( pXDevice->userId, JOYSTICK_FIRST ), JOYSTICK_BUTTON_INTERNAL( pXDevice->userId + 1, JOYSTICK_FIRST ) - 1 );
					ZeroAnalogState( JOYSTICK_AXIS_INTERNAL( pXDevice->userId, JOYSTICK_FIRST_AXIS ), JOYSTICK_AXIS_INTERNAL( pXDevice->userId, JOYSTICK_FIRST_AXIS ) - 1 );
					memset( &m_appXKeys[pXDevice->userId][0], 0, XK_MAX_KEYS * sizeof(appKey_t) );
				}
			}
		}
		m_flLastControllerPollTime = -1;
		return;
	}
	else if ( m_flLastControllerPollTime != -1 && m_flLastControllerPollTime + flCheckDelay <= Plat_FloatTime() )
	{
		bool bAnyActive = false;
#ifdef PLATFORM_PS3
		if ( g_pMoveController->m_bEnabled && !ps3_move_enabled.GetBool() )
		{
			g_pMoveController->Disable();
		}
		else if ( !g_pMoveController->m_bEnabled && ps3_move_enabled.GetBool() )
		{
			g_pMoveController->Enable();
		}

		PS3_XInputPollEverything( m_pPS3CellPadDataHook, m_pPS3CellNoPadDataHook );	// update all the joysticks

		// Read move controller data
		if( g_pMoveController->m_bEnabled &&
			(g_PS3_XInputInfo.m_moveControllerState.m_CellGemInfo.status[0]==CELL_GEM_STATUS_READY) &&
			(g_PS3_XInputInfo.m_moveControllerState.m_aStatus[0] == CELL_OK)
			)
		{
			m_bMotionControllerActive = true;
			bool bMcVisible = g_PS3_XInputInfo.m_moveControllerState.m_aCellGemState[0].tracking_flags & CELL_GEM_TRACKING_FLAG_VISIBLE;
			if (bMcVisible) 
			{
				m_qMotionControllerOrientation = 
					Quaternion(	-g_PS3_XInputInfo.m_moveControllerState.m_aCellGemState[0].quat[2],
					-g_PS3_XInputInfo.m_moveControllerState.m_aCellGemState[0].quat[0],
					g_PS3_XInputInfo.m_moveControllerState.m_aCellGemState[0].quat[1],
					g_PS3_XInputInfo.m_moveControllerState.m_aCellGemState[0].quat[3]);


				//m_vecMotionControllerAngle = g_PS3_XInputInfo.m_moveControllerState.m_aAngle[0];
				m_vecMotionControllerPos = g_PS3_XInputInfo.m_moveControllerState.m_pos[0];

				m_fMotionControllerPosX = g_PS3_XInputInfo.m_moveControllerState.m_posX[0];
				m_fMotionControllerPosY = g_PS3_XInputInfo.m_moveControllerState.m_posY[0];
			}
			else
			{
				// If not visible, set screen x and y position to be 0.0
				m_fMotionControllerPosX = m_fMotionControllerPosX*0.85f;
				m_fMotionControllerPosY = m_fMotionControllerPosY*0.85f;
			}

		}
		else
		{
			m_bMotionControllerActive = false;
		}

		m_nMotionControllerStatusFlags = g_PS3_XInputInfo.m_moveControllerState.m_aStatusFlags[0];
#endif

		PollXMouse();
		PollXKeyboard();

		xdevice_t* pXDevice = m_XDevices;

		for ( int userId = 0; userId < XUSER_MAX_COUNT; ++userId, ++pXDevice )
		{
			// Get input data in the form of Xbox controller data (for PS3, map onto Xbox struct)
			DWORD result = XINPUTGETSTATE( userId, &pXDevice->states[pXDevice->newState] );
			switch ( result )
			{
			case ERROR_SUCCESS:
				if ( !pXDevice->active )
				{
					// just inserted
					OpenXDevice( pXDevice, userId );
					int openedPort = userId;
#ifdef _PS3
					if ( ( XBX_GetNumGameUsers() <= 1 ) && !ps3_joy_ss.GetBool() )
						openedPort = 0;
#endif
					PostEvent( IE_ControllerInserted, m_nLastSampleTick, openedPort );

#if defined( _PS3 )
					if ( PS3IsNavController( userId ) )
					{
						SetInputDeviceConnected( INPUT_DEVICE_MOVE_NAV_CONTROLLER, true );
					}
					else
#endif
					{
						SetInputDeviceConnected( INPUT_DEVICE_GAMEPAD, true );

						ConVarRef var( "joystick" );
						if ( var.IsValid() && var.GetBool() == false )
							var.SetValue( 1 );
					}
				}

				bAnyActive = true;

				// See if primary user has been set, and if so then block all other input
				// if ( pXDevice->userId == m_PrimaryUserId ||  m_PrimaryUserId == INVALID_USER_ID )
				{
					ReadXDevice( pXDevice );
					WriteToXDevice( pXDevice );
				}

				break;

			case ERROR_DEVICE_NOT_CONNECTED:
				if ( pXDevice->active )
				{
					// just removed
					int closedPort = pXDevice->userId;
					CloseXDevice( pXDevice );
				
#ifdef _X360
					if ( XBX_GetSlotByUserId( closedPort ) >= 0 )
#else
					if ( 1 )
#endif
					{
						// Controller unplugged - game needs to take action
						// Release buttons of the specific joystick that was unplugged
						ReleaseAllButtons( JOYSTICK_BUTTON_INTERNAL( closedPort, JOYSTICK_FIRST ), JOYSTICK_BUTTON_INTERNAL( closedPort + 1, JOYSTICK_FIRST ) - 1 );
						ZeroAnalogState( JOYSTICK_AXIS_INTERNAL( closedPort, JOYSTICK_FIRST_AXIS ), JOYSTICK_AXIS_INTERNAL( closedPort, JOYSTICK_FIRST_AXIS ) - 1 );
						memset( &m_appXKeys[closedPort][0], 0, XK_MAX_KEYS * sizeof(appKey_t) );

#ifdef _PS3
						if ( ( XBX_GetNumGameUsers() <= 1 ) && !ps3_joy_ss.GetBool() &&
							( closedPort + 1 == g_PS3_XInputInfo.m_iActiveSingleControllerIndex ) )
						{
							closedPort = 0;
							PostEvent( IE_ControllerUnplugged, m_nLastSampleTick, closedPort );
						}
#else
						PostEvent( IE_ControllerUnplugged, m_nLastSampleTick, closedPort );
#endif

#if defined( _PS3 )
						if ( PS3IsNavController( userId ) )
						{
							SetInputDeviceConnected( INPUT_DEVICE_MOVE_NAV_CONTROLLER, false );
						}
						else
#endif
						{
							SetInputDeviceConnected( INPUT_DEVICE_GAMEPAD, false );
						}

					}
				}
				break;
			}
		}
		// We don't need to check every frame if nothing is connected
		if ( bAnyActive )
		{
			m_bXController = true;
			ConVarRef var( "joy_xcontroller_found" );
			if ( var.IsValid() && var.GetBool() == false )
				var.SetValue( 1 );
		}
		else
		{
			m_bXController = false;
		}

		m_flLastControllerPollTime = Plat_FloatTime();
	}
}


#ifdef _PS3
static float g_ps3_flTimeStartButtonIdentificationMode = 0.0f;
void CInputSystem::SetPS3StartButtonIdentificationMode()
{
	g_ps3_flTimeStartButtonIdentificationMode = Plat_FloatTime();
}
#endif


//-----------------------------------------------------------------------------
//	Purpose: Post Xbox events, ignoring key repeats
//-----------------------------------------------------------------------------
void CInputSystem::PostXKeyEvent( int userId, xKey_t xKey, int nSample )
{
	AnalogCode_t	code	= ANALOG_CODE_LAST;
	float			value	= 0.f;

	// Map the physical controller slot to the split screen slot
#if defined( _GAMECONSOLE )
	int nMsgSlot = XBX_GetSlotByUserId( userId );
	#ifdef _PS3
	if ( ( XBX_GetNumGameUsers() <= 1 ) && !ps3_joy_ss.GetBool() )
	{
		// In PS3 START button identification mode START key notification
		// is replaced with INACTIVE_START notification that can identify
		// controller that pressed the button
		if ( ( xKey == XK_BUTTON_START ) && ( nMsgSlot < 0 )
			&& ( ( Plat_FloatTime() - g_ps3_flTimeStartButtonIdentificationMode ) < 0.5f ) )
		{
			xKey = XK_BUTTON_INACTIVE_START;
			nMsgSlot = userId;
		}
		else
		{
			// When we don't have splitscreen then any controller can
			// play and will be visible as controller #0
			nMsgSlot = 0;
		}
	}
	#endif
	if ( nMsgSlot < 0 )
	{
		// special case, that if you press start on a controller we've marked inactive, switch it to an
		// XK_BUTTON_INACTIVE_START which you can handle joins from inactive controllers
		if ( xKey == XK_BUTTON_START )
		{
			xKey = XK_BUTTON_INACTIVE_START;
			nMsgSlot = userId;
		}
		else
		{
			return; // We are not listening to this controller (not signed in and assigned)
		}
	}
#else //defined( _GAMECONSOLE )
	int nMsgSlot = userId;
#endif //defined( _GAMECONSOLE )

	int nSampleThreshold = 0; 

	// Look for changes on the analog axes
	switch( xKey )
	{
	case XK_STICK1_LEFT:
	case XK_STICK1_RIGHT:
		{
			code = (AnalogCode_t)JOYSTICK_AXIS( nMsgSlot, JOY_AXIS_X );
			value = ( xKey == XK_STICK1_LEFT ) ? -nSample : nSample;
			nSampleThreshold = ( int )( JOYSTICK_ANALOG_BUTTON_THRESHOLD );
		}
		break;

	case XK_STICK1_UP:
	case XK_STICK1_DOWN:
		{
			code = (AnalogCode_t)JOYSTICK_AXIS( nMsgSlot, JOY_AXIS_Y );
			value = ( xKey == XK_STICK1_UP ) ? -nSample : nSample;
			nSampleThreshold = ( int )( JOYSTICK_ANALOG_BUTTON_THRESHOLD );
		}
		break;

	case XK_STICK2_LEFT:
	case XK_STICK2_RIGHT:
		{
			code = (AnalogCode_t)JOYSTICK_AXIS( nMsgSlot, JOY_AXIS_U );
			value = ( xKey == XK_STICK2_LEFT ) ? -nSample : nSample;
			nSampleThreshold = ( int )( JOYSTICK_ANALOG_BUTTON_THRESHOLD );
		}
		break;

	case XK_STICK2_UP:
	case XK_STICK2_DOWN:
		{
			code = (AnalogCode_t)JOYSTICK_AXIS( nMsgSlot, JOY_AXIS_R );
			value = ( xKey == XK_STICK2_UP ) ? -nSample : nSample;
			nSampleThreshold = ( int )( JOYSTICK_ANALOG_BUTTON_THRESHOLD );
		}
		break;
	}

	// Store the analog event
	if ( ANALOG_CODE_LAST != code )
	{
		InputState_t &state = m_InputState[ m_bIsPolling ];
		state.m_pAnalogDelta[ code ] = ( int )( value - state.m_pAnalogValue[ code ] );
		state.m_pAnalogValue[ code ] = ( int )value;
		if ( state.m_pAnalogDelta[ code ] != 0 )
		{
			PostEvent( IE_AnalogValueChanged, m_nLastSampleTick, code, ( int )value, state.m_pAnalogDelta[ code ] );
		}
	}

	// store the key
	m_appXKeys[userId][xKey].sample = nSample;
	if ( nSample > nSampleThreshold )
	{
		m_appXKeys[userId][xKey].repeats++;
	}
	else
	{
		m_appXKeys[userId][xKey].repeats = 0;
		nSample = 0;
	}

	if ( m_appXKeys[userId][xKey].repeats > 1 )
	{
		// application cannot handle streaming keys
		// first keypress is the only edge trigger
		return;
	}

	// package the key
	ButtonCode_t buttonCode = XKeyToButtonCode( nMsgSlot, xKey );
	if ( nSample )
	{
		PostButtonPressedEvent( IE_ButtonPressed, m_nLastSampleTick, buttonCode, buttonCode );
		
		// [dkorus] check whether we're trying to set the current controller
		if( ( buttonCode == KEY_XBUTTON_A || buttonCode == XK_BUTTON_START )
			&& m_setCurrentInputDeviceOnNextButtonPress )
		{

#if defined( _PS3 )
			if ( PS3IsNavController( userId ) )
			{
				if ( IsInputDeviceConnected( INPUT_DEVICE_MOVE_NAV_CONTROLLER ) )
				{
					// [dkorus] if someone trys to lock input with the nav controller...
					//			select the sharpshooter if it's available
					//			otherwise select the MOVE if it's available
					if ( IsInputDeviceConnected( INPUT_DEVICE_SHARPSHOOTER ) )
					{
						SetCurrentInputDevice( INPUT_DEVICE_SHARPSHOOTER );
						m_setCurrentInputDeviceOnNextButtonPress = false;
						
						
					}
					else if ( IsInputDeviceConnected( INPUT_DEVICE_PLAYSTATION_MOVE ) )
					{
						SetCurrentInputDevice( INPUT_DEVICE_PLAYSTATION_MOVE );
						m_setCurrentInputDeviceOnNextButtonPress = false;
					}
				}
			}
			else
#endif
			{
				if ( IsInputDeviceConnected( INPUT_DEVICE_GAMEPAD ) )
				{
					SetCurrentInputDevice( INPUT_DEVICE_GAMEPAD );
					ConVarRef var( "joystick" );
					if( var.IsValid( ) )
						var.SetValue( 1 );
					m_setCurrentInputDeviceOnNextButtonPress = false;
				}
			}
		}

	}
	else
	{
		PostButtonReleasedEvent( IE_ButtonReleased, m_nLastSampleTick, buttonCode, buttonCode );
	}
}


//-----------------------------------------------------------------------------
//	Purpose: Send force feedback to an Xbox controller
//-----------------------------------------------------------------------------
void CInputSystem::WriteToXDevice( xdevice_t* pXDevice )
{
	if ( pXDevice->pendingRumbleUpdate )
	{
		XINPUTSETSTATE( pXDevice->userId, &pXDevice->vibration );
		pXDevice->pendingRumbleUpdate = false;
	}
}

static int NormalizeStickValue_Cross( int nValue, int nThreshold, float flScale )
{
	if ( nValue >= -nThreshold )
	{
		if ( nValue <= nThreshold )
		{
			return 0;
		}
		else
		{
			return ( int )( ( nValue - nThreshold ) * flScale );
		}
	}
	else
	{
		return ( int )( ( nValue + nThreshold ) * flScale );
	}
}

static void NormalizeStickValue_Square( int nX, int nY, int nThresholdX, int nThresholdY, int *pX, int *pY )
{
	if ( nX >= -nThresholdX && nX <= nThresholdX && nY >= -nThresholdY && nY <= nThresholdY )
	{
		*pX	= 0;
		*pY = 0;
	}
	else
	{
		*pX = nX;
		*pY = nY;
	}
}

void CInputSystem::HandleXDeviceAxis( xdevice_t *pXDevice, int nAxisValue, xKey_t negativeKey, xKey_t positiveKey, int axisID )
{
	xKey_t key;

	// Queue stick axis push response
	if ( nAxisValue < 0 )
	{
		PostXKeyEvent( pXDevice->userId, negativeKey, -nAxisValue );
		key = negativeKey;
	}
	else if ( nAxisValue > 0 )
	{
		PostXKeyEvent( pXDevice->userId, positiveKey, nAxisValue );
		key = positiveKey;
	}
	else
	{
		key = XK_NULL;
	}

	if ( pXDevice->lastStickKeys[axisID] && pXDevice->lastStickKeys[axisID] != key )
	{
		// Queue stick axis release response
		PostXKeyEvent( pXDevice->userId, pXDevice->lastStickKeys[axisID], 0 );
	}
	pXDevice->lastStickKeys[axisID] = key;


}

//-----------------------------------------------------------------------------
//	Purpose: Queue input key events for a device
//-----------------------------------------------------------------------------
void CInputSystem::ReadXDevice( xdevice_t* pXDevice )
{
	XINPUT_STATE*	oldStatePtr;
	XINPUT_STATE*	newStatePtr;
	int				mask;
	int				buttons;
	int				sample;

	oldStatePtr = &pXDevice->states[pXDevice->newState ^ 1];
	newStatePtr = &pXDevice->states[pXDevice->newState];
	
	// forceful exit
	if ( newStatePtr->Gamepad.wButtons == ( XINPUT_GAMEPAD_START|XINPUT_GAMEPAD_BACK ) )
	{
		if ( !pXDevice->quitTimeout )
		{
			pXDevice->quitTimeout = GetTickCount();
		}
		else if ( GetTickCount() - pXDevice->quitTimeout > 2*1000 )
		{
			// mandatory 2sec hold
			pXDevice->quitTimeout = 0;
			ProcessEvent( WM_XREMOTECOMMAND, 0, (LPARAM)"quit_gameconsole" );
		}
	}
	else
	{
		// reset
		pXDevice->quitTimeout = 0;
	}

	// No changes if packet numbers match
	if (( oldStatePtr->dwPacketNumber == newStatePtr->dwPacketNumber ) && !m_bMotionControllerActive)
		return;

	// PS3 builds allow gamepad input even if we're using another device 
	// move and other controllers use the same gamepad button presses
#if !defined ( _PS3 )
	if ( !IsDeviceReadingInput( INPUT_DEVICE_GAMEPAD ) )
		return;
#endif

	// digital events
	buttons = newStatePtr->Gamepad.wButtons ^ oldStatePtr->Gamepad.wButtons;
	if ( buttons )
	{
		// determine if dpad press is axial or diagonal combos
		bool bDpadIsAxial = IsPowerOfTwo( newStatePtr->Gamepad.wButtons & (XINPUT_GAMEPAD_DPAD_UP|XINPUT_GAMEPAD_DPAD_DOWN|XINPUT_GAMEPAD_DPAD_LEFT|XINPUT_GAMEPAD_DPAD_RIGHT) );

		// determine digital difference - up or down?
		for ( int i = 0; i < sizeof( g_digitalXKeyTable )/sizeof( g_digitalXKeyTable[0] ); ++i )
		{
			mask = buttons & g_digitalXKeyTable[i].xinput;
			if ( !mask )
				continue;

			if ( mask & newStatePtr->Gamepad.wButtons )
			{
				// down event
				sample = XBX_MAX_BUTTONSAMPLE;
			}
			else
			{
				// up event
				sample = 0;
			}

			// due to rocker mechanics, allow only 1 of 4 behavior
			// the last down axial event trumps, preventing diagonals causing multiple events
			if ( mask & (XINPUT_GAMEPAD_DPAD_UP|XINPUT_GAMEPAD_DPAD_DOWN|XINPUT_GAMEPAD_DPAD_LEFT|XINPUT_GAMEPAD_DPAD_RIGHT) )
			{
				if ( !bDpadIsAxial )
				{
					// diagonal dpad event, discard any dpad event that isn't the lock
					if ( pXDevice->dpadLock && pXDevice->dpadLock != mask )
						continue;
				}
				else if ( sample )
				{
					// axial dpad down event, set the lock
					pXDevice->dpadLock = mask;
				}
			}

			PostXKeyEvent( pXDevice->userId, (xKey_t)g_digitalXKeyTable[i].xkey, sample );
		}
	}

	// analog events
	// queue left trigger axis analog response
	if ( newStatePtr->Gamepad.bLeftTrigger <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD )
	{
		newStatePtr->Gamepad.bLeftTrigger = 0;	
	}
	sample = newStatePtr->Gamepad.bLeftTrigger;
	if ( sample != oldStatePtr->Gamepad.bLeftTrigger )
	{
		PostXKeyEvent( pXDevice->userId, XK_BUTTON_LTRIGGER, ( int )( sample * ( float )XBX_MAX_BUTTONSAMPLE/( float )XBX_MAX_ANALOGSAMPLE ) );
	}

	// queue right trigger axis analog response
	if ( newStatePtr->Gamepad.bRightTrigger <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD )
	{
		newStatePtr->Gamepad.bRightTrigger = 0;	
	}
	sample = newStatePtr->Gamepad.bRightTrigger;
	if ( sample != oldStatePtr->Gamepad.bRightTrigger )
	{
		PostXKeyEvent( pXDevice->userId, XK_BUTTON_RTRIGGER, ( int )( sample * ( float )XBX_MAX_BUTTONSAMPLE/( float )XBX_MAX_ANALOGSAMPLE ) );
	}

	int nX1 = 0, nY1 = 0, nX2 = 0, nY2 = 0;

	extern ConVar joy_deadzone_mode;

	switch ( joy_deadzone_mode.GetInt() )
	{
	case JOYSTICK_DEADZONE_CROSS:
		nX1 = NormalizeStickValue_Cross( newStatePtr->Gamepad.sThumbLX, pXDevice->stickThreshold[STICK1_AXIS_X], pXDevice->stickScale[STICK1_AXIS_X] );
		nY1 = NormalizeStickValue_Cross( newStatePtr->Gamepad.sThumbLY, pXDevice->stickThreshold[STICK1_AXIS_Y], pXDevice->stickScale[STICK1_AXIS_Y] );
		nX2 = NormalizeStickValue_Cross( newStatePtr->Gamepad.sThumbRX, pXDevice->stickThreshold[STICK2_AXIS_X], pXDevice->stickScale[STICK2_AXIS_X] );
		nY2 = NormalizeStickValue_Cross( newStatePtr->Gamepad.sThumbRY, pXDevice->stickThreshold[STICK2_AXIS_Y], pXDevice->stickScale[STICK2_AXIS_Y] );
		break;
	case JOYSTICK_DEADZONE_SQUARE:
		NormalizeStickValue_Square( newStatePtr->Gamepad.sThumbLX, newStatePtr->Gamepad.sThumbLY, pXDevice->stickThreshold[STICK1_AXIS_X], pXDevice->stickThreshold[STICK1_AXIS_Y], &nX1, &nY1 );
		NormalizeStickValue_Square( newStatePtr->Gamepad.sThumbRX, newStatePtr->Gamepad.sThumbRY, pXDevice->stickThreshold[STICK2_AXIS_X], pXDevice->stickThreshold[STICK2_AXIS_Y], &nX2, &nY2 );
		break;
	default:
		UNREACHABLE();
	}

	HandleXDeviceAxis( pXDevice, nX1, XK_STICK1_LEFT, XK_STICK1_RIGHT, STICK1_AXIS_X );
	HandleXDeviceAxis( pXDevice, nY1, XK_STICK1_DOWN, XK_STICK1_UP, STICK1_AXIS_Y );
	HandleXDeviceAxis( pXDevice, nX2, XK_STICK2_LEFT, XK_STICK2_RIGHT, STICK2_AXIS_X );
	HandleXDeviceAxis( pXDevice, nY2, XK_STICK2_DOWN, XK_STICK2_UP, STICK2_AXIS_Y );

	// toggle the states
	pXDevice->newState ^= 1;
}

void CInputSystem::QueueMoveControllerRumble( float fRightMotor )
{
#if defined( _PS3 )
	WORD wNewRight = ( WORD )( XBX_MAX_MOTOR_SPEED * fRightMotor );

	if ( wNewRight > XBX_MAX_MOTOR_SPEED )
		wNewRight = XBX_MAX_MOTOR_SPEED;

	if ( fRightMotor != g_PS3_XInputInfo.ps3_move_rumble_value ) 
	{
		g_PS3_XInputInfo.ps3_move_rumble_value = wNewRight;
		g_PS3_XInputInfo.ps3_move_rumble_queued = true;
	}
#endif
}
//-----------------------------------------------------------------------------
//	Purpose: Queues a left and right motor value for the Xbox controller
//-----------------------------------------------------------------------------
void CInputSystem::SetXDeviceRumble( float fLeftMotor, float fRightMotor, int userId )
{
	WORD wOldLeft, wOldRight;// Last values we sent
	WORD wNewLeft, wNewRight;// Values we're about to send.
	xdevice_t* pXDevice;

	if ( IsDeviceReadingInput( INPUT_DEVICE_PLAYSTATION_MOVE ) || 
		IsDeviceReadingInput( INPUT_DEVICE_SHARPSHOOTER ) )
 	{
		// [dkorus] route move controller commands seperately, no need for an pXDevice for the move controller
		//			note: move controller only needs one motor setting
		QueueMoveControllerRumble( fRightMotor );
		return;
	}

	if ( userId == INVALID_USER_ID )
		return;

	pXDevice = &m_XDevices[userId];

	// can only set rumble on active controllers
	if ( pXDevice->userId == INVALID_USER_ID )
	{
		return;
	}

	wNewLeft = ( WORD )( XBX_MAX_MOTOR_SPEED * fLeftMotor );
	wNewRight = ( WORD )( XBX_MAX_MOTOR_SPEED * fRightMotor );

	if ( wNewLeft > XBX_MAX_MOTOR_SPEED )
		wNewLeft = XBX_MAX_MOTOR_SPEED;

	if ( wNewRight > XBX_MAX_MOTOR_SPEED )
		wNewRight = XBX_MAX_MOTOR_SPEED;

	wOldLeft = pXDevice->vibration.wLeftMotorSpeed;
	wOldRight = pXDevice->vibration.wRightMotorSpeed;

	if ( wNewLeft != wOldLeft || wNewRight != wOldRight )
	{
		pXDevice->vibration.wLeftMotorSpeed = wNewLeft;
		pXDevice->vibration.wRightMotorSpeed = wNewRight;
		pXDevice->pendingRumbleUpdate = true;
	}
}

void CInputSystem::SetMotionControllerCalibrationInvalid( void )
{
#if defined( PLATFORM_PS3 )
	g_pMoveController->InvalidateCalibration();
#endif // PLATFORM_PS3
}

void CInputSystem::StepMotionControllerCalibration( void )
{
#if defined( PLATFORM_PS3 )
	g_pMoveController->StepMotionControllerCalibration();
#endif // PLATFORM_PS3
}

void CInputSystem::ResetMotionControllerScreenCalibration( void )
{
#if defined( PLATFORM_PS3 )
	g_pMoveController->ResetMotionControllerScreenCalibration();
#endif // PLATFORM_PS3
}

