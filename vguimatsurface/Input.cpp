//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Implementation of the VGUI ISurface interface using the 
// material system to implement it
//
//===========================================================================//

#if defined( WIN32 ) && !defined( _X360 )
#include <windows.h>
#include <zmouse.h>
#endif
#include "inputsystem/iinputsystem.h"
#include "tier2/tier2.h"
#include "Input.h"
#include "vguimatsurface.h"
#include "../vgui2/src/VPanel.h"
#include <vgui/KeyCode.h>
#include <vgui/MouseCode.h>
#include <vgui/IVGui.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui/IClientPanel.h>
#include "inputsystem/ButtonCode.h"
#include "Cursor.h"
#include "tier0/dbg.h"
#include "../vgui2/src/vgui_key_translation.h"
#include <vgui/IInputInternal.h>
#include "tier0/icommandline.h"
#ifdef _X360
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;



//-----------------------------------------------------------------------------
// Converts an input system button code to a vgui key code
// FIXME: Remove notion of vgui::KeyCode + vgui::MouseCode altogether
//-----------------------------------------------------------------------------
static vgui::KeyCode ButtonCodeToKeyCode( ButtonCode_t buttonCode )
{
	return ( vgui::KeyCode )buttonCode;
}

static vgui::MouseCode ButtonCodeToMouseCode( ButtonCode_t buttonCode )
{
	return ( vgui::MouseCode )buttonCode;
}


//-----------------------------------------------------------------------------
// Handles an input event, returns true if the event should be filtered
// from the rest of the game
//-----------------------------------------------------------------------------
bool InputHandleInputEvent( InputContextHandle_t hContext, const InputEvent_t &event )
{
	switch( event.m_nType )
	{
	case IE_ButtonPressed:
		{
			// NOTE: data2 is the virtual key code (data1 contains the scan-code one)
			ButtonCode_t code = (ButtonCode_t)event.m_nData2;
			if ( IsKeyCode( code ) || IsJoystickCode( code ) )
			{
				vgui::KeyCode keyCode = ButtonCodeToKeyCode( code );
				return g_pIInput->InternalKeyCodePressed( keyCode );
			}

			if ( IsJoystickCode( code ) )
			{
				vgui::KeyCode keyCode = ButtonCodeToKeyCode( code );
				return g_pIInput->InternalKeyCodePressed( keyCode );
			}

			if ( IsMouseCode( code ) )
			{
				vgui::MouseCode mouseCode = ButtonCodeToMouseCode( code );
				return g_pIInput->InternalMousePressed( mouseCode );
			}
		}
		break;

	case IE_ButtonReleased:
		{
			// NOTE: data2 is the virtual key code (data1 contains the scan-code one)
			ButtonCode_t code = (ButtonCode_t)event.m_nData2;
			if ( IsKeyCode( code ) || IsJoystickCode( code ) )
			{
				vgui::KeyCode keyCode = ButtonCodeToKeyCode( code );
				return g_pIInput->InternalKeyCodeReleased( keyCode );
			}

			if ( IsJoystickCode( code ) )
			{
				vgui::KeyCode keyCode = ButtonCodeToKeyCode( code );
				return g_pIInput->InternalKeyCodeReleased( keyCode );
			}

			if ( IsMouseCode( code ) )
			{
				vgui::MouseCode mouseCode = ButtonCodeToMouseCode( code );
				return g_pIInput->InternalMouseReleased( mouseCode );
			}
		}
		break;

	case IE_ButtonDoubleClicked:
		{
			// NOTE: data2 is the virtual key code (data1 contains the scan-code one)
			ButtonCode_t code = (ButtonCode_t)event.m_nData2;
			if ( IsMouseCode( code ) )
			{
				vgui::MouseCode mouseCode = ButtonCodeToMouseCode( code );
				return g_pIInput->InternalMouseDoublePressed( mouseCode );
			}
		}
		break;

	case IE_AnalogValueChanged:
		{
			if ( event.m_nData == MOUSE_WHEEL )
				return g_pIInput->InternalMouseWheeled( event.m_nData3 );
			if ( event.m_nData == MOUSE_XY )
				return g_pIInput->InternalCursorMoved( event.m_nData2, event.m_nData3 );

			//=============================================================================
			// HPE_BEGIN
			// [dwenger] Handle gamepad joystick movement.
			//=============================================================================
  			if ( event.m_nData == JOYSTICK_AXIS(0, JOY_AXIS_X ))
 			{
 				return g_pIInput->InternalJoystickMoved ( 0, event.m_nData2 );
 			}
 			if ( event.m_nData == JOYSTICK_AXIS(0, JOY_AXIS_Y ) )
 			{
 				return g_pIInput->InternalJoystickMoved ( 1, event.m_nData2 );
 			}
 			if ( event.m_nData == JOYSTICK_AXIS(0, JOY_AXIS_U ))
 			{
 				return g_pIInput->InternalJoystickMoved ( 2, event.m_nData2 );
 			}
 			if ( event.m_nData == JOYSTICK_AXIS(0, JOY_AXIS_R ) )
 			{
 				return g_pIInput->InternalJoystickMoved ( 3, event.m_nData2 );
 			}
			//=============================================================================
			// HPE_END
			//=============================================================================

		}
		break;

	case IE_KeyCodeTyped:
		{
			vgui::KeyCode code = (vgui::KeyCode)event.m_nData;
			g_pIInput->InternalKeyCodeTyped( code );
		}
		return true;

	case IE_KeyTyped:
		{
			wchar_t code = (wchar_t)event.m_nData;
			g_pIInput->InternalKeyTyped( code );
		}
		// return false so scaleform can use this
		break;

	case IE_Quit:
		g_pVGui->Stop();
#if defined( USE_SDL ) || defined( OSX ) 
		return false; // also let higher layers consume it
#else
		return true;
#endif
			
	case IE_Close:
		// FIXME: Change this so we don't stop until 'save' occurs, etc.
		g_pVGui->Stop();
		return true;

	case IE_SetCursor:
		ActivateCurrentCursor( hContext );
		return true;

	case IE_IMESetWindow:
		g_pIInput->SetIMEWindow( (void *)event.m_nData );
		return true;

	case IE_LocateMouseClick:
		g_pIInput->InternalCursorMoved( event.m_nData, event.m_nData2 );
		return true;

	case IE_InputLanguageChanged:
		g_pIInput->OnInputLanguageChanged();
		return true;

	case IE_IMEStartComposition:
		g_pIInput->OnIMEStartComposition();
		return true;

	case IE_IMEComposition:
		g_pIInput->OnIMEComposition( event.m_nData );
		return true;

	case IE_IMEEndComposition:
		g_pIInput->OnIMEEndComposition();
		return true;

	case IE_IMEShowCandidates:
		g_pIInput->OnIMEShowCandidates();
		return true;

	case IE_IMEChangeCandidates:
		g_pIInput->OnIMEChangeCandidates();
		return true;

	case IE_IMECloseCandidates:
		g_pIInput->OnIMECloseCandidates();
		return true;

	case IE_IMERecomputeModes:
		g_pIInput->OnIMERecomputeModes();
		return true;
	}

	return false;
}



