//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#if !defined( _GAMECONSOLE )
#include <windows.h>
#include <imm.h>
#endif
#include <string.h>

#include "inputgameui.h"
#include "tier0/icommandline.h"
#include "gameuisystemmgr.h"
#include "inputsystem/analogcode.h"
#include "inputsystem/iinputstacksystem.h"

#if defined( _GAMECONSOLE )
#include "xbox/xbox_win32stubs.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

SHORT System_GetKeyState( int virtualKeyCode ); // in System.cpp, a hack to only have g_pVCR in system.cpp

CInputGameUI g_InputGameUI;
CInputGameUI *g_pInputGameUI = &g_InputGameUI;


//-----------------------------------------------------------------------------
// Construct, destruct
//-----------------------------------------------------------------------------
CInputGameUI::CInputGameUI()
{
	m_nDebugMessages = -1;
#ifndef _GAMECONSOLE
	_imeWnd = 0;
	_imeCandidates = 0;
#endif
	InitInputContext( &m_DefaultInputContext );
	m_hContext = DEFAULT_INPUT_CONTEXT;
	m_nWindowWidth = 0;
	m_nWindowHeight = 0;

	// build key to text translation table
	// first byte unshifted key
	// second byte shifted key
	// the rest is the name of the key
	_keyTrans[KEY_0]			="0)KEY_0";
	_keyTrans[KEY_1]			="1!KEY_1";
	_keyTrans[KEY_2]			="2@KEY_2";
	_keyTrans[KEY_3]			="3#KEY_3";
	_keyTrans[KEY_4]			="4$KEY_4";
	_keyTrans[KEY_5]			="5%KEY_5";
	_keyTrans[KEY_6]			="6^KEY_6";
	_keyTrans[KEY_7]			="7&KEY_7";
	_keyTrans[KEY_8]			="8*KEY_8";
	_keyTrans[KEY_9]			="9(KEY_9";
	_keyTrans[KEY_A]			="aAKEY_A";
	_keyTrans[KEY_B]			="bBKEY_B";
	_keyTrans[KEY_C]			="cCKEY_C";
	_keyTrans[KEY_D]			="dDKEY_D";
	_keyTrans[KEY_E]			="eEKEY_E";
	_keyTrans[KEY_F]			="fFKEY_F";
	_keyTrans[KEY_G]			="gGKEY_G";
	_keyTrans[KEY_H]			="hHKEY_H";
	_keyTrans[KEY_I]			="iIKEY_I";
	_keyTrans[KEY_J]			="jJKEY_J";
	_keyTrans[KEY_K]			="kKKEY_K";
	_keyTrans[KEY_L]			="lLKEY_L"", L";
	_keyTrans[KEY_M]			="mMKEY_M";
	_keyTrans[KEY_N]			="nNKEY_N";
	_keyTrans[KEY_O]			="oOKEY_O";
	_keyTrans[KEY_P]			="pPKEY_P";
	_keyTrans[KEY_Q]			="qQKEY_Q";
	_keyTrans[KEY_R]			="rRKEY_R";
	_keyTrans[KEY_S]			="sSKEY_S";
	_keyTrans[KEY_T]			="tTKEY_T";
	_keyTrans[KEY_U]			="uUKEY_U";
	_keyTrans[KEY_V]			="vVKEY_V";
	_keyTrans[KEY_W]			="wWKEY_W";
	_keyTrans[KEY_X]			="xXKEY_X";
	_keyTrans[KEY_Y]			="yYKEY_Y";
	_keyTrans[KEY_Z]			="zZKEY_Z";
	_keyTrans[KEY_PAD_0]		="0\0KEY_PAD_0";
	_keyTrans[KEY_PAD_1]		="1\0KEY_PAD_1";
	_keyTrans[KEY_PAD_2]		="2\0KEY_PAD_2";
	_keyTrans[KEY_PAD_3]		="3\0KEY_PAD_3";
	_keyTrans[KEY_PAD_4]		="4\0KEY_PAD_4";
	_keyTrans[KEY_PAD_5]		="5\0KEY_PAD_5";
	_keyTrans[KEY_PAD_6]		="6\0KEY_PAD_6";
	_keyTrans[KEY_PAD_7]		="7\0KEY_PAD_7";
	_keyTrans[KEY_PAD_8]		="8\0KEY_PAD_8";
	_keyTrans[KEY_PAD_9]		="9\0KEY_PAD_9";
	_keyTrans[KEY_PAD_DIVIDE]	="//KEY_PAD_DIVIDE";
	_keyTrans[KEY_PAD_MULTIPLY]	="**KEY_PAD_MULTIPLY";
	_keyTrans[KEY_PAD_MINUS]	="--KEY_PAD_MINUS";
	_keyTrans[KEY_PAD_PLUS]		="++KEY_PAD_PLUS";
	_keyTrans[KEY_PAD_ENTER]	="\0\0KEY_PAD_ENTER";
	_keyTrans[KEY_PAD_DECIMAL]	=".\0KEY_PAD_DECIMAL"", L";
	_keyTrans[KEY_LBRACKET]		="[{KEY_LBRACKET";
	_keyTrans[KEY_RBRACKET]		="]}KEY_RBRACKET";
	_keyTrans[KEY_SEMICOLON]	=";:KEY_SEMICOLON";
	_keyTrans[KEY_APOSTROPHE]	="'\"KEY_APOSTROPHE";
	_keyTrans[KEY_BACKQUOTE]	="`~KEY_BACKQUOTE";
	_keyTrans[KEY_COMMA]		=",<KEY_COMMA";
	_keyTrans[KEY_PERIOD]		=".>KEY_PERIOD";
	_keyTrans[KEY_SLASH]		="/?KEY_SLASH";
	_keyTrans[KEY_BACKSLASH]	="\\|KEY_BACKSLASH";
	_keyTrans[KEY_MINUS]		="-_KEY_MINUS";
	_keyTrans[KEY_EQUAL]		="=+KEY_EQUAL"", L";
	_keyTrans[KEY_ENTER]		="\0\0KEY_ENTER";
	_keyTrans[KEY_SPACE]		="  KEY_SPACE";
	_keyTrans[KEY_BACKSPACE]	="\0\0KEY_BACKSPACE";
	_keyTrans[KEY_TAB]			="\0\0KEY_TAB";
	_keyTrans[KEY_CAPSLOCK]		="\0\0KEY_CAPSLOCK";
	_keyTrans[KEY_NUMLOCK]		="\0\0KEY_NUMLOCK";
	_keyTrans[KEY_ESCAPE]		="\0\0KEY_ESCAPE";
	_keyTrans[KEY_SCROLLLOCK]	="\0\0KEY_SCROLLLOCK";
	_keyTrans[KEY_INSERT]		="\0\0KEY_INSERT";
	_keyTrans[KEY_DELETE]		="\0\0KEY_DELETE";
	_keyTrans[KEY_HOME]			="\0\0KEY_HOME";
	_keyTrans[KEY_END]			="\0\0KEY_END";
	_keyTrans[KEY_PAGEUP]		="\0\0KEY_PAGEUP";
	_keyTrans[KEY_PAGEDOWN]		="\0\0KEY_PAGEDOWN";
	_keyTrans[KEY_BREAK]		="\0\0KEY_BREAK";
	_keyTrans[KEY_LSHIFT]		="\0\0KEY_LSHIFT";
	_keyTrans[KEY_RSHIFT]		="\0\0KEY_RSHIFT";
	_keyTrans[KEY_LALT]			="\0\0KEY_LALT";
	_keyTrans[KEY_RALT]			="\0\0KEY_RALT";
	_keyTrans[KEY_LCONTROL]		="\0\0KEY_LCONTROL"", L";
	_keyTrans[KEY_RCONTROL]		="\0\0KEY_RCONTROL"", L";
	_keyTrans[KEY_LWIN]			="\0\0KEY_LWIN";
	_keyTrans[KEY_RWIN]			="\0\0KEY_RWIN";
	_keyTrans[KEY_APP]			="\0\0KEY_APP";
	_keyTrans[KEY_UP]			="\0\0KEY_UP";
	_keyTrans[KEY_LEFT]			="\0\0KEY_LEFT";
	_keyTrans[KEY_DOWN]			="\0\0KEY_DOWN";
	_keyTrans[KEY_RIGHT]		="\0\0KEY_RIGHT";
	_keyTrans[KEY_F1]			="\0\0KEY_F1";
	_keyTrans[KEY_F2]			="\0\0KEY_F2";
	_keyTrans[KEY_F3]			="\0\0KEY_F3";
	_keyTrans[KEY_F4]			="\0\0KEY_F4";
	_keyTrans[KEY_F5]			="\0\0KEY_F5";
	_keyTrans[KEY_F6]			="\0\0KEY_F6";
	_keyTrans[KEY_F7]			="\0\0KEY_F7";
	_keyTrans[KEY_F8]			="\0\0KEY_F8";
	_keyTrans[KEY_F9]			="\0\0KEY_F9";
	_keyTrans[KEY_F10]			="\0\0KEY_F10";
	_keyTrans[KEY_F11]			="\0\0KEY_F11";
	_keyTrans[KEY_F12]			="\0\0KEY_F12";
}

CInputGameUI::~CInputGameUI()
{
	DestroyCandidateList();
}

//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
void CInputGameUI::Init()
{
	m_hEventChannel = g_pEventSystem->CreateEventQueue();
	CursorEnterEvent::RegisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnCursorEnter );
	CursorExitEvent::RegisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnCursorExit );
	CursorMoveEvent::RegisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnCursorMove );
	InternalCursorMoveEvent::RegisterMemberFunc( m_hEventChannel, this, &CInputGameUI::UpdateCursorPosInternal );

	MouseDownEvent::RegisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnMouseDown );
	MouseUpEvent::RegisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnMouseUp );
	MouseDoubleClickEvent::RegisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnMouseDoubleClick );
	MouseWheelEvent::RegisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnMouseWheel );
	KeyDownEvent::RegisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnKeyDown );
	KeyUpEvent::RegisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnKeyUp );
	KeyCodeTypedEvent::RegisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnKeyCodeTyped );
	KeyTypedEvent::RegisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnKeyTyped );
	LoseKeyFocusEvent::RegisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnLoseKeyFocus );
	GainKeyFocusEvent::RegisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnGainKeyFocus );
}

void CInputGameUI::Shutdown()
{
	CursorEnterEvent::UnregisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnCursorEnter );
	CursorExitEvent::UnregisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnCursorExit );
	CursorMoveEvent::UnregisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnCursorMove );
	InternalCursorMoveEvent::UnregisterMemberFunc( m_hEventChannel, this, &CInputGameUI::UpdateCursorPosInternal );

	MouseDownEvent::UnregisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnMouseDown );
	MouseUpEvent::UnregisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnMouseUp );
	MouseDoubleClickEvent::UnregisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnMouseDoubleClick );
	MouseWheelEvent::UnregisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnMouseWheel );
	KeyDownEvent::UnregisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnKeyDown );
	KeyUpEvent::UnregisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnKeyUp );
	KeyCodeTypedEvent::UnregisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnKeyCodeTyped );
	KeyTypedEvent::UnregisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnKeyTyped );
	LoseKeyFocusEvent::UnregisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnLoseKeyFocus );
	GainKeyFocusEvent::UnregisterMemberFunc( m_hEventChannel, this, &CInputGameUI::OnGainKeyFocus );

	g_pEventSystem->DestroyEventQueue( m_hEventChannel );
}

//-----------------------------------------------------------------------------
// Resets an input context 
//-----------------------------------------------------------------------------
void CInputGameUI::InitInputContext( InputContext_t *pContext )
{
	pContext->_keyFocus = NULL;
	pContext->_bKeyTrap = false;

	pContext->_oldMouseFocus = NULL;
	pContext->_mouseFocus = NULL;
	pContext->_mouseOver = NULL;
	pContext->_mouseCapture = NULL;
	pContext->_mouseLeftTrap = NULL;
	pContext->_mouseMiddleTrap = NULL;
	pContext->_mouseRightTrap = NULL;

	pContext->m_nCursorX = pContext->m_nCursorY = 0;
	pContext->m_nLastPostedCursorX = pContext->m_nLastPostedCursorY = -9999;
	pContext->m_nExternallySetCursorX = pContext->m_nExternallySetCursorY = 0;
	pContext->m_bSetCursorExplicitly = false;

	// zero mouse and keys
	memset(pContext->_mousePressed, 0, sizeof(pContext->_mousePressed));
	memset(pContext->_mouseDoublePressed, 0, sizeof(pContext->_mouseDoublePressed));
	memset(pContext->_mouseDown, 0, sizeof(pContext->_mouseDown));
	memset(pContext->_mouseReleased, 0, sizeof(pContext->_mouseReleased));
	memset(pContext->_keyPressed, 0, sizeof(pContext->_keyPressed));
	memset(pContext->_keyTyped, 0, sizeof(pContext->_keyTyped));
	memset(pContext->_keyDown, 0, sizeof(pContext->_keyDown));
	memset(pContext->_keyReleased, 0, sizeof(pContext->_keyReleased));

	pContext->m_MouseCaptureStartCode = (ButtonCode_t)-1;

	pContext->m_KeyCodeUnhandledListeners.RemoveAll();

	pContext->m_pUnhandledMouseClickListener = NULL;
	pContext->m_bRestrictMessagesToModalSubTree = false;
}

void CInputGameUI::ResetInputContext( vgui::HInputContext context )
{
	// FIXME: Needs to release various keys, mouse buttons, etc...?
	// At least needs to cause things to lose focus
	InitInputContext( GetInputContext(context) );
}


//-----------------------------------------------------------------------------
// Creates/ destroys "input" contexts, which contains information
// about which controls have mouse + key focus, for example.
//-----------------------------------------------------------------------------
vgui::HInputContext CInputGameUI::CreateInputContext()
{
	vgui::HInputContext i = m_Contexts.AddToTail();
	InitInputContext( &m_Contexts[i] );
	return i;
}

void CInputGameUI::DestroyInputContext( vgui::HInputContext context )
{
	Assert( context != DEFAULT_INPUT_CONTEXT );
	if ( m_hContext == context )
	{
		ActivateInputContext( DEFAULT_INPUT_CONTEXT );
	}
	m_Contexts.Remove(context);
}


//-----------------------------------------------------------------------------
// Returns the current input context
//-----------------------------------------------------------------------------
CInputGameUI::InputContext_t *CInputGameUI::GetInputContext( vgui::HInputContext context )
{
	if (context == DEFAULT_INPUT_CONTEXT)
	{
		return &m_DefaultInputContext;
	}
	return &m_Contexts[context];
}


//-----------------------------------------------------------------------------
// Activates a particular input context, use DEFAULT_INPUT_CONTEXT
// to get the one normally used by VGUI
//-----------------------------------------------------------------------------
void CInputGameUI::ActivateInputContext( vgui::HInputContext context )
{
	Assert( (context == DEFAULT_INPUT_CONTEXT) || m_Contexts.IsValidIndex(context) );
	m_hContext = context;
}

//-----------------------------------------------------------------------------
// Used to know the dimensions of the window on screen.
// Very important when viewport is not the same as the window.
//-----------------------------------------------------------------------------
void CInputGameUI::SetWindowSize( int width, int height )
{
	g_pGameUISystemMgrImpl->SetWindowSize( width, height );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInputGameUI::RunFrame()
{
	if ( m_nDebugMessages == -1 )
	{
		m_nDebugMessages = CommandLine()->FindParm( "-debuggameui" ) ? 1 : 0;
	}

	InputContext_t *pContext = GetInputContext(m_hContext);

	//clear mouse and key states
	int i;
	for ( i = 0; i < MOUSE_COUNT; i++ )
	{
		pContext->_mousePressed[i] = 0;
		pContext->_mouseDoublePressed[i] = 0;
		pContext->_mouseReleased[i] = 0;
	}
	for ( i = 0; i < BUTTON_CODE_COUNT; i++ )
	{
		pContext->_keyPressed[i] = 0;
		pContext->_keyTyped[i] = 0;
		pContext->_keyReleased[i] = 0;
	}

	CHitArea *wantedKeyFocus = CalculateNewKeyFocus();

	// make sure old and new focus get painted
	if ( pContext->_keyFocus != wantedKeyFocus )
	{
		if ( pContext->_keyFocus != NULL )
		{
			LoseKeyFocusEvent::Post( pContext->_keyFocus );
		}
		if ( wantedKeyFocus != NULL )
		{
			GainKeyFocusEvent::Post( wantedKeyFocus );
		}

		// accept the focus request
		pContext->_keyFocus = wantedKeyFocus;
		pContext->_bKeyTrap = false;
	}

	// Pump any key repeats
	ButtonCode_t repeatCode = pContext->m_keyRepeater.KeyRepeated();
	if (repeatCode)
	{
		InternalKeyCodePressed( repeatCode );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CInputGameUI::ProcessEvents()
{
	g_pEventSystem->ProcessEvents( m_hEventChannel );
}


//-----------------------------------------------------------------------------
// Purpose: Calculate the new key focus
//-----------------------------------------------------------------------------
CHitArea *CInputGameUI::CalculateNewKeyFocus()
{
	// get the top-order panel
	CHitArea *wantedKeyFocus = NULL;

	// ask the gameui for what it would like to be the current focus
	wantedKeyFocus = g_pGameUISystemMgrImpl->GetRequestedKeyFocus();	
	if ( wantedKeyFocus == NULL )
	{
		InputContext_t *pContext = GetInputContext(m_hContext);
		return pContext->_keyFocus;
	}
	else
	{
		Assert( wantedKeyFocus->IsHitArea() );
		return wantedKeyFocus;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Graphic's visibility was set to false.
//-----------------------------------------------------------------------------
void CInputGameUI::GraphicHidden( CHitArea *focus, InputContext_t &context )
{
	if ( context._keyFocus == focus )
	{
		context._keyFocus = NULL;
		context._bKeyTrap = false;
	}
	if (context._mouseOver == focus )
	{
		/*
		if ( m_nDebugMessages > 0 )
		{
		g_pIVgui->DPrintf2( "removing kb focus %s\n", 
		context._keyFocus ? pcontext._keyFocus->GetName() : "(no name)" );
		}
		*/
		context._mouseOver = NULL;
	}
	if ( context._oldMouseFocus == focus )
	{
		context._oldMouseFocus = NULL;
	}
	if ( context._mouseFocus == focus )
	{
		context._mouseFocus = NULL;
	}
	if ( context._mouseLeftTrap == focus )
	{
		context._mouseLeftTrap = NULL;
	}
	if ( context._mouseMiddleTrap == focus )
	{
		context._mouseMiddleTrap = NULL;
	}
	if ( context._mouseRightTrap == focus )
	{
		context._mouseRightTrap = NULL;
	}

	// NOTE: These two will only ever happen for the default context at the moment
	if ( context._mouseCapture == focus )
	{
		SetMouseCapture(NULL);
		context._mouseCapture = NULL;
	}
	if ( context.m_pUnhandledMouseClickListener == focus )
	{
		context.m_pUnhandledMouseClickListener = NULL;
	}

	//context.m_KeyCodeUnhandledListeners.FindAndRemove( focus );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *focus - 
//-----------------------------------------------------------------------------
void CInputGameUI::GraphicHidden( CHitArea *focus )
{
	HInputContext i;
	for ( i = m_Contexts.Head(); i != m_Contexts.InvalidIndex(); i = m_Contexts.Next(i) )
	{
		
		GraphicHidden( focus, m_Contexts[i] );
	}
	GraphicHidden( focus, m_DefaultInputContext );
}


//-----------------------------------------------------------------------------
// Purpose: An Input Graphic's visibility was set to true.	Force focus update to occur with no mouse movement.
//-----------------------------------------------------------------------------
void CInputGameUI::ForceInputFocusUpdate()
{
	// Force the focus to get updated.
	HInputContext i;
	for ( i = m_Contexts.Head(); i != m_Contexts.InvalidIndex(); i = m_Contexts.Next(i) )
	{
		InputContext_t *pContext = &m_Contexts[i];
		UpdateMouseFocus( pContext->m_nCursorX, pContext->m_nCursorY );
	}
	UpdateMouseFocus( m_DefaultInputContext.m_nCursorX, m_DefaultInputContext.m_nCursorY );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInputGameUI::PanelDeleted( CHitArea *focus, InputContext_t &context )
{
	if (context._keyFocus == focus)
	{
		context._keyFocus = NULL;
		context._bKeyTrap = false;
	}
	if (context._mouseOver == focus)
	{
		/*
		if ( m_nDebugMessages > 0 )
		{
		g_pIVgui->DPrintf2( "removing kb focus %s\n", 
		context._keyFocus ? pcontext._keyFocus->GetName() : "(no name)" );
		}
		*/
		context._mouseOver = NULL;
	}
	if ( context._oldMouseFocus == focus )
	{
		context._oldMouseFocus = NULL;
	}
	if ( context._mouseFocus == focus )
	{
		context._mouseFocus = NULL;
	}
	if ( context._mouseLeftTrap == focus )
	{
		context._mouseLeftTrap = NULL;
	}
	if ( context._mouseMiddleTrap == focus )
	{
		context._mouseMiddleTrap = NULL;
	}
	if ( context._mouseRightTrap == focus )
	{
		context._mouseRightTrap = NULL;
	}

	// NOTE: These two will only ever happen for the default context at the moment
	if ( context._mouseCapture == focus )
	{
		SetMouseCapture(NULL);
		context._mouseCapture = NULL;
	}
	if ( context.m_pUnhandledMouseClickListener == focus )
	{
		context.m_pUnhandledMouseClickListener = NULL;
	}

	context.m_KeyCodeUnhandledListeners.FindAndRemove( focus );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *focus - 
//-----------------------------------------------------------------------------
void CInputGameUI::PanelDeleted( CHitArea *focus )
{
	HInputContext i;
	for ( i = m_Contexts.Head(); i != m_Contexts.InvalidIndex(); i = m_Contexts.Next(i) )
	{
		PanelDeleted( focus, m_Contexts[i] );
	}
	PanelDeleted( focus, m_DefaultInputContext );
}

//-----------------------------------------------------------------------------
// Purpose: Sets the new mouse focus
//			won't override _mouseCapture settings
// Input  : newMouseFocus - 
//-----------------------------------------------------------------------------
void CInputGameUI::SetMouseFocus( CHitArea *newMouseFocus )
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext->_mouseOver != newMouseFocus || ( !pContext->_mouseCapture && pContext->_mouseFocus != newMouseFocus ) )
	{
		pContext->_oldMouseFocus = pContext->_mouseOver;
		pContext->_mouseOver = newMouseFocus;

		//tell the old panel with the mouseFocus that the cursor exited
		if ( pContext->_oldMouseFocus != NULL )
		{
			// only notify of entry if the mouse is not captured or we're the captured panel
			if ( !pContext->_mouseCapture || pContext->_oldMouseFocus == pContext->_mouseCapture )
			{
				CursorExitEvent::Post( pContext->_oldMouseFocus );
			}
		}

		//tell the new panel with the mouseFocus that the cursor entered
		if ( pContext->_mouseOver != NULL )
		{
			// only notify of entry if the mouse is not captured or we're the captured panel
			if ( !pContext->_mouseCapture || pContext->_mouseOver == pContext->_mouseCapture )
			{
				CursorEnterEvent::Post( pContext->_mouseOver );
			}
		}

		// set where the mouse is currently over
		// mouse capture overrides destination
		pContext->_mouseFocus = pContext->_mouseCapture ? pContext->_mouseCapture : pContext->_mouseOver;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Calculates which panel the cursor is currently over and sets it up
//			as the current mouse focus.
//-----------------------------------------------------------------------------
void CInputGameUI::UpdateMouseFocus( int x, int y )
{
	//InputContext_t *pContext = GetInputContext( m_hContext );
	CHitArea *pFocus = NULL;

	//if ( g_pSurface->IsCursorVisible() && g_pSurface->IsWithin(x, y) )
	{
		if (!pFocus)
		{
			// find the panel that has the focus
			pFocus = g_pGameUISystemMgrImpl->GetMouseFocus( x, y );
		}
	}

	SetMouseFocus( pFocus );
}

//-----------------------------------------------------------------------------
// Passes in a keycode which allows hitting other mouse buttons w/o cancelling capture mode
//-----------------------------------------------------------------------------
void CInputGameUI::SetMouseCaptureEx( CHitArea *panel, ButtonCode_t captureStartMouseCode )
{
	// This sets m_MouseCaptureStartCode to -1, so we set the real value afterward
	SetMouseCapture( panel );

	InputContext_t *pContext = GetInputContext( m_hContext );
	Assert( pContext );
	pContext->m_MouseCaptureStartCode = captureStartMouseCode;
}

//-----------------------------------------------------------------------------
// Passes in a keycode which allows hitting other mouse buttons w/o canceling capture mode
// Purpose: Sets or releases the mouse capture
// Input  : panel - pointer to the panel to get mouse capture
//			a NULL panel means that you want to clear the mouseCapture
//-----------------------------------------------------------------------------
void CInputGameUI::SetMouseCapture( CHitArea *panel )
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	Assert( pContext );

	pContext->m_MouseCaptureStartCode = (ButtonCode_t)-1;
	pContext->_mouseCapture = panel;
}


CHitArea *CInputGameUI::GetMouseCapture() 
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	return (CHitArea *)pContext->_mouseCapture;
}


void SetKeyFocus( CHitArea *pFocus ); // note this will not post any messages.

//-----------------------------------------------------------------------------
// Purpose: Used to set keyfocus to mousefocus when they are linked together.
// 
// This will not post any messages. If it did anims would get played twice,
// once for mouse focus gained/lost, once for key focus gained/lost
//-----------------------------------------------------------------------------
void CInputGameUI::SetKeyFocus( CHitArea *pFocus)
{
	GetInputContext( m_hContext )->_keyFocus = pFocus;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHitArea *CInputGameUI::GetKeyFocus()
{
	return (CHitArea *)( GetInputContext( m_hContext )->_keyFocus );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHitArea *CInputGameUI::GetCalculatedKeyFocus()
{
	return (CHitArea *) CalculateNewKeyFocus();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHitArea * CInputGameUI::GetMouseOver()
{
	return (CHitArea *)( GetInputContext( m_hContext )->_mouseOver );
}

CHitArea * CInputGameUI::GetMouseFocus()
{
	return (CHitArea *)( GetInputContext( m_hContext )->_mouseFocus );
}

bool CInputGameUI::WasMousePressed( ButtonCode_t code )
{
	return GetInputContext( m_hContext )->_mousePressed[ code - MOUSE_FIRST ];
}

bool CInputGameUI::WasMouseDoublePressed( ButtonCode_t code )
{
	return GetInputContext( m_hContext )->_mouseDoublePressed[ code - MOUSE_FIRST ];
}

bool CInputGameUI::IsMouseDown( ButtonCode_t code )
{
	return GetInputContext( m_hContext )->_mouseDown[ code - MOUSE_FIRST ];
}

bool CInputGameUI::WasMouseReleased( ButtonCode_t code )
{
	return GetInputContext( m_hContext )->_mouseReleased[ code - MOUSE_FIRST ];
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CInputGameUI::WasKeyPressed( ButtonCode_t code )
{
	return GetInputContext( m_hContext )->_keyPressed[ code - KEY_FIRST ];
}

bool CInputGameUI::IsKeyDown( ButtonCode_t code )
{
	return GetInputContext( m_hContext )->_keyDown[ code - KEY_FIRST ];
}

bool CInputGameUI::WasKeyTyped( ButtonCode_t code )
{
	return GetInputContext( m_hContext )->_keyTyped[ code - KEY_FIRST ];
}

bool CInputGameUI::WasKeyReleased( ButtonCode_t code )
{
	// changed from: only return true if the key was released and the passed in panel matches the keyFocus
	return GetInputContext( m_hContext )->_keyReleased[ code - KEY_FIRST ];
}


//-----------------------------------------------------------------------------
// Cursor position; this is the current position read from the input queue.
// We need to set it because client code may read this during Mouse Pressed
// events, etc.
//-----------------------------------------------------------------------------
void CInputGameUI::UpdateCursorPosInternal( const int &x, const int &y )
{
	// Windows sends a CursorMoved message even when you haven't actually
	// moved the cursor, this means we are going into this fxn just by clicking
	// in the window. We only want to execute this code if we have actually moved
	// the cursor while dragging. So this code has been added to check
	// if we have actually moved from our previous position.
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext->m_nCursorX == x && pContext->m_nCursorY == y )
		return;

	pContext->m_nCursorX = x;
	pContext->m_nCursorY = y;

	// Rescale this accounting the window size being different from the viewport size.
	int windowWidth, windowHeight;
	g_pGameUISystemMgrImpl->GetWindowSize( windowWidth, windowHeight );
	int viewportWidth, viewportHeight;
	g_pGameUISystemMgrImpl->GetViewportSize( viewportWidth, viewportHeight );
	if ( windowHeight != 0 && viewportHeight != 0 )
	{
		if ( windowWidth != viewportWidth || windowHeight != viewportHeight ) 
		{
			pContext->m_nCursorX = x * viewportWidth/windowWidth;
			pContext->m_nCursorY = y * viewportHeight/windowHeight;	
		}
	}

	// Cursor has moved, so make sure the mouseFocus is current
	UpdateMouseFocus( pContext->m_nCursorX, pContext->m_nCursorY );
}


//-----------------------------------------------------------------------------
// This is called by panels to teleport the cursor
//-----------------------------------------------------------------------------
void CInputGameUI::SetCursorPos( int x, int y )
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	pContext->m_nExternallySetCursorX = x;
	pContext->m_nExternallySetCursorY = y;
	pContext->m_bSetCursorExplicitly = true;	
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CInputGameUI::GetCursorPos(int &x, int &y)
{
	GetCursorPosition( x, y );
}


//-----------------------------------------------------------------------------
// Here for backward compat
//-----------------------------------------------------------------------------
void CInputGameUI::GetCursorPosition( int &x, int &y )
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	x = pContext->m_nCursorX;
	y = pContext->m_nCursorY;
}

//-----------------------------------------------------------------------------
// Purpose: Converts a key code into a full key name
//-----------------------------------------------------------------------------
void CInputGameUI::GetKeyCodeText(ButtonCode_t code, char *buf, int buflen)
{
	if (!buf)
		return;

	// copy text into buf up to buflen in length
	// skip 2 in _keyTrans because the first two are for GetKeyCodeChar
	for (int i = 0; i < buflen; i++)
	{
		char ch = _keyTrans[code][i+2];
		buf[i] = ch;
		if (ch == 0)
			break;
	}

}


//-----------------------------------------------------------------------------
// Low-level cursor getting/setting functions 
//-----------------------------------------------------------------------------
void CInputGameUI::SurfaceSetCursorPos( int x, int y )
{
	g_pInputStackSystem->SetCursorPosition( g_pGameUISystemMgrImpl->GetInputContext(), x, y );
	//if ( g_pSurface->HasCursorPosFunctions() ) // does the surface export cursor functions for us to use?
	{
	//	g_pSurface->SurfaceSetCursorPos(x,y);
	}
	//else
	{
		// translate into coordinates relative to surface
		//int px, py. pw, pt;
		//g_pSurface->GetAbsoluteWindowBounds(px, py, pw, pt);
		//x += px;
		//y += py;
		// set windows cursor pos
		//::SetCursorPos(x, y);
	}
}

void CInputGameUI::SurfaceGetCursorPos( int &x, int &y )
{
#ifndef _GAMECONSOLE // X360TBD
	//if ( g_pSurface->HasCursorPosFunctions() ) // does the surface export cursor functions for us to use?
	{
	//	g_pSurface->SurfaceGetCursorPos( x,y );
	}
	//else
	{
//		g_pInputSystem->GetCursorPosition( &x, &y );

		// get mouse position in windows
		POINT pnt;
		::GetCursorPos(&pnt);
		x = pnt.x;
		y = pnt.y;

		// translate into coordinates relative to surface
		//int px, py, pw, pt;
		//g_pSurface->GetAbsoluteWindowBounds(px, py, pw, pt);
		//x -= px;
		//y -= py;
	}
#else
	x = 0;
	y = 0;
#endif
}

void CInputGameUI::SetCursorOveride( vgui::HCursor cursor )
{
	_cursorOverride = cursor;
}

vgui::HCursor CInputGameUI::GetCursorOveride()
{
	return _cursorOverride;
}


//-----------------------------------------------------------------------------
// Called when we've detected cursor has moved via a windows message
//-----------------------------------------------------------------------------
bool CInputGameUI::InternalCursorMoved( int x, int y )
{
	InternalCursorMoveEvent::Post( x, y );
	return true;
}


//-----------------------------------------------------------------------------
// Makes sure the windows cursor is in the right place after processing input 
//-----------------------------------------------------------------------------
void CInputGameUI::HandleExplicitSetCursor( )
{
	InputContext_t *pContext = GetInputContext( m_hContext );

	if ( pContext->m_bSetCursorExplicitly )
	{
		pContext->m_nCursorX = pContext->m_nExternallySetCursorX;
		pContext->m_nCursorY = pContext->m_nExternallySetCursorY;
		pContext->m_bSetCursorExplicitly = false;

		// NOTE: This forces a cursor moved message to be posted next time
		pContext->m_nLastPostedCursorX = pContext->m_nLastPostedCursorY = -9999;

		SurfaceSetCursorPos( pContext->m_nCursorX, pContext->m_nCursorY );
		UpdateMouseFocus( pContext->m_nCursorX, pContext->m_nCursorY ); 
	}
}


//-----------------------------------------------------------------------------
// Called when we've detected cursor has moved via a windows message
//-----------------------------------------------------------------------------
void CInputGameUI::PostCursorMessage( )
{
	InputContext_t *pContext = GetInputContext( m_hContext );

	if ( pContext->m_bSetCursorExplicitly )
	{
		// NOTE m_bSetCursorExplicitly will be reset to false in HandleExplicitSetCursor
		pContext->m_nCursorX = pContext->m_nExternallySetCursorX;
		pContext->m_nCursorY = pContext->m_nExternallySetCursorY;
	}

	if ( pContext->m_nLastPostedCursorX == pContext->m_nCursorX && pContext->m_nLastPostedCursorY == pContext->m_nCursorY )
		return;

	pContext->m_nLastPostedCursorX = pContext->m_nCursorX;
	pContext->m_nLastPostedCursorY = pContext->m_nCursorY;

	if ( pContext->_mouseCapture )
	{
		// the panel with mouse capture gets all messages
		CursorMoveEvent::Post( pContext->_mouseCapture, pContext->m_nCursorX, pContext->m_nCursorY );
	}
	else if ( pContext->_mouseFocus != NULL)
	{
		// mouse focus is current from UpdateMouse focus
		// so the appmodal check has already been made.
		CursorMoveEvent::Post( pContext->_mouseFocus, pContext->m_nCursorX, pContext->m_nCursorY );
	}
}


bool CInputGameUI::InternalMousePressed( ButtonCode_t code )
{
	// True means we've processed the message and other code shouldn't see this message
	bool bFilter = false;

	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext->_mouseCapture )
	{
		// The faked mouse wheel button messages are specifically ignored by vgui
		if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
			return true;

		bool captureLost = code == pContext->m_MouseCaptureStartCode || pContext->m_MouseCaptureStartCode == (ButtonCode_t)-1;

		// the panel with mouse capture gets all messages
		MouseDownEvent::Post( pContext->_mouseCapture, code );

		if ( captureLost )
		{
			// this has to happen after MousePressed so the panel doesn't Think it got a mouse press after it lost capture
			SetMouseCapture(NULL);
		}
		bFilter = true;
	}
	else if ( pContext->_mouseFocus != NULL )
	{
		// The faked mouse wheel button messages are specifically ignored by vgui
		if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
			return true;

		// tell the panel with the mouseFocus that the mouse was presssed
		//g_pIVgui->PostMessage((VPANEL)pContext->_mouseFocus, new KeyValues("MousePressed", "code", code), NULL);
		//		g_pIVgui->DPrintf2("MousePressed: (%s, %s)\n", _mouseFocus->GetName(), _mouseFocus->GetClassName());

		MouseDownEvent::Post( pContext->_mouseFocus, code );

		if ( code == MOUSE_LEFT )
		{
			pContext->_mouseLeftTrap = pContext->_mouseFocus;
		}
		else if ( code == MOUSE_MIDDLE )
		{
			pContext->_mouseMiddleTrap = pContext->_mouseFocus;
		}
		else if ( code == MOUSE_RIGHT )
		{
			pContext->_mouseRightTrap = pContext->_mouseFocus;
		}
		bFilter = true;
	}

	return bFilter;
}

bool CInputGameUI::InternalMouseDoublePressed( ButtonCode_t code )
{
	// True means we've processed the message and other code shouldn't see this message
	bool bFilter = false;

	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext->_mouseCapture )
	{
		// The faked mouse wheel button messages are specifically ignored by vgui
		if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
			return true;

		// the panel with mouse capture gets all messages	
		MouseDoubleClickEvent::Post( pContext->_mouseCapture, code );
		bFilter = true;
	}
	else if ( pContext->_mouseFocus != NULL )
	{			
		// The faked mouse wheel button messages are specifically ignored by vgui
		if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
			return true;

		// tell the panel with the mouseFocus that the mouse was double presssed
		MouseDoubleClickEvent::Post( pContext->_mouseFocus, code );
		bFilter = true;
	}

	return bFilter;
}

bool CInputGameUI::InternalMouseReleased( ButtonCode_t code )
{
	// True means we've processed the message and other code shouldn't see this message
	bool bFilter = false;

	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext->_mouseCapture )
	{
		// The faked mouse wheel button messages are specifically ignored by vgui
		if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
			return true;

		// the panel with mouse capture gets all messages
		MouseUpEvent::Post( pContext->_mouseCapture, pContext->_mouseCapture, code );
		bFilter = true;
	}
	else if ( pContext->_mouseLeftTrap )
	{
		MouseUpEvent::Post( pContext->_mouseLeftTrap, pContext->_mouseLeftTrap, code );
		pContext->_mouseLeftTrap = NULL;
		bFilter = true;
	}
	else if ( pContext->_mouseMiddleTrap )
	{
		MouseUpEvent::Post( pContext->_mouseMiddleTrap, pContext->_mouseLeftTrap, code );
		pContext->_mouseMiddleTrap = NULL;
		bFilter = true;
	}
	else if ( pContext->_mouseRightTrap )
	{
		MouseUpEvent::Post( pContext->_mouseRightTrap, pContext->_mouseLeftTrap, code );
		pContext->_mouseRightTrap = NULL;
		bFilter = true;
	}
	/* If the mouse was not trapped by a down, don't propagate this message.
	else if ( pContext->_mouseFocus != NULL )
	{
		// The faked mouse wheel button messages are specifically ignored by vgui
		if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
			return true;

		MouseUpEvent::Post( pContext->_mouseFocus, code );	
		bFilter = true;
	}
	*/

	return bFilter;
}

bool CInputGameUI::InternalMouseWheeled(int delta)
{
	// True means we've processed the message and other code shouldn't see this message
	bool bFilter = false;

	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext->_mouseFocus != NULL )
	{
		// the mouseWheel works with the mouseFocus, not the keyFocus
		MouseWheelEvent::Post( pContext->_mouseFocus, delta );
		bFilter = true;
	}
	return bFilter;
}

//-----------------------------------------------------------------------------
// Updates the internal key/mouse state associated with the current input context without sending messages
//-----------------------------------------------------------------------------
void CInputGameUI::SetMouseCodeState( ButtonCode_t code, GameUIMouseCodeState_t state )
{
	if ( !IsMouseCode( code ) )
		return;

	InputContext_t *pContext = GetInputContext( m_hContext );
	switch( state )
	{
	case BUTTON_RELEASED:
		pContext->_mouseReleased[ code - MOUSE_FIRST ] = 1;
		break;

	case BUTTON_PRESSED:
		pContext->_mousePressed[ code - MOUSE_FIRST ] = 1;
		break;

	case BUTTON_DOUBLECLICKED:
		pContext->_mouseDoublePressed[ code - MOUSE_FIRST ] = 1;
		break;
	}

	pContext->_mouseDown[ code - MOUSE_FIRST ] = ( state != BUTTON_RELEASED );
}

void CInputGameUI::SetKeyCodeState( ButtonCode_t code, bool bPressed )
{
	if ( !IsKeyCode( code ) 
#ifdef _GAMECONSOLE
		&& !IsJoystickCode( code )
#endif
		)
		return;

	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( bPressed )
	{
		//set key state
		pContext->_keyPressed[ code - KEY_FIRST ] = 1;
	}
	else
	{
		// set key state
		pContext->_keyReleased[ code - KEY_FIRST ] = 1;
	}
	pContext->_keyDown[ code - KEY_FIRST ] = bPressed;
}

void CInputGameUI::UpdateButtonState( const InputEvent_t &event )
{
	switch( event.m_nType )
	{
	case IE_ButtonPressed:
	case IE_ButtonReleased:
	case IE_ButtonDoubleClicked:
		{
			// NOTE: data2 is the virtual key code (data1 contains the scan-code one)
			ButtonCode_t code = (ButtonCode_t)event.m_nData2;

			// FIXME: Workaround hack
			if ( IsKeyCode( code ) || IsJoystickCode( code ) )
			{
				SetKeyCodeState( code, ( event.m_nType != IE_ButtonReleased ) );
				break;
			}

			if ( IsMouseCode( code ) )
			{
				GameUIMouseCodeState_t state;
				state = ( event.m_nType == IE_ButtonReleased ) ? BUTTON_RELEASED : BUTTON_PRESSED;
				if ( event.m_nType == IE_ButtonDoubleClicked )
				{
					state = BUTTON_DOUBLECLICKED;
				}

				SetMouseCodeState( code, state );
				break;
			}
		}
		break;
	}
}


bool CInputGameUI::InternalKeyCodePressed( ButtonCode_t code )
{
	InputContext_t *pContext = GetInputContext( m_hContext );

	// mask out bogus keys
	if ( !IsKeyCode( code ) && !IsJoystickCode( code ) )
		return false;

	bool bFilter = false;
	if( pContext->_keyFocus!= NULL )
	{
#ifdef _GAMECONSOLE
		//g_pIVgui->PostMessage((VPANEL) MESSAGE_CURRENT_KEYFOCUS, message, NULL );
#else
		//tell the current focused panel that a key was typed
		KeyDownEvent::Post( pContext->_keyFocus, code );
		pContext->_bKeyTrap = true;
#endif
		bFilter = true;
	}

	if ( bFilter )
	{
		// Only notice the key down for repeating if we actually used the key
		pContext->m_keyRepeater.KeyDown( code );
	}
	return bFilter;
}

void CInputGameUI::InternalKeyCodeTyped( ButtonCode_t code )
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	// mask out bogus keys
	if ( !IsKeyCode( code ) && !IsJoystickCode( code ) )
		return;

	// set key state
	pContext->_keyTyped[ code - KEY_FIRST ] = 1;

	if( pContext->_keyFocus!= NULL )
	{
#ifdef _GAMECONSOLE
		//g_pIVgui->PostMessage((VPANEL) MESSAGE_CURRENT_KEYFOCUS, message, NULL );
#else
		//tell the current focused panel that a key was typed
		KeyCodeTypedEvent::Post( pContext->_keyFocus, code );
		g_pGameUISystemMgrImpl->OnKeyCodeTyped( code );	// FIXME, make work like mouse clicked.
#endif
	}
}

void CInputGameUI::InternalKeyTyped( wchar_t unichar )
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	// set key state
	if( unichar <= KEY_LAST )
	{
		pContext->_keyTyped[unichar]=1;
	}

	if( pContext->_keyFocus!= NULL )
	{
#ifdef _GAMECONSOLE
		//g_pIVgui->PostMessage((VPANEL) MESSAGE_CURRENT_KEYFOCUS, message, NULL );
#else
		//tell the current focused panel that a key was typed
		KeyTypedEvent::Post( pContext->_keyFocus, unichar );
		g_pGameUISystemMgrImpl->OnKeyTyped( unichar );   // FIXME, make work like mouse clicked.
#endif
	}

}

bool CInputGameUI::InternalKeyCodeReleased( ButtonCode_t code )
{	
	InputContext_t *pContext = GetInputContext( m_hContext );

	// mask out bogus keys
	if ( !IsKeyCode( code ) && !IsJoystickCode( code ) )
		return false;

	pContext->m_keyRepeater.KeyUp( code );

	if ( ( pContext->_keyFocus!= NULL ) && pContext->_bKeyTrap )
	{
#ifdef _GAMECONSOLE
		//g_pIVgui->PostMessage((VPANEL) MESSAGE_CURRENT_KEYFOCUS, message, NULL );
#else
		//tell the current focused panel that a key was released
		KeyUpEvent::Post( pContext->_keyFocus, code );
		pContext->_bKeyTrap = false;
#endif
		return true;
	}

	return false;

}

//-----------------------------------------------------------------------------
// Purpose: posts a message to the key focus if it's valid
//-----------------------------------------------------------------------------
bool CInputGameUI::PostKeyMessage(KeyValues *message)
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if( pContext->_keyFocus!= NULL )
	{
#ifdef _GAMECONSOLE
		//g_pIVgui->PostMessage((VPANEL) MESSAGE_CURRENT_KEYFOCUS, message, NULL );
#else
		//tell the current focused panel that a key was released
		//g_pIVgui->PostMessage((VPANEL)pContext->_keyFocus, message, NULL );
#endif
		return true;
	}

	message->deleteThis();
	return false;
}

enum LANGFLAG
{
	ENGLISH,				
	TRADITIONAL_CHINESE,	
	JAPANESE,
	KOREAN,
	SIMPLIFIED_CHINESE,
	UNKNOWN,

	NUM_IMES_SUPPORTED
} LangFlag;  

struct LanguageIds
{
	// char const			*idname;
	unsigned short			id;
	int						languageflag;
	wchar_t	const			*shortcode;
	wchar_t const			*displayname;
	bool					invertcomposition;
};

LanguageIds g_LanguageIds[] =
{
	{ 0x0000, UNKNOWN, L"",		L"Neutral" }, 
	{ 0x007f, UNKNOWN, L"",		L"Invariant" }, 
	{ 0x0400, UNKNOWN, L"",		L"User Default Language" },  
	{ 0x0800, UNKNOWN, L"",		L"System Default Language" },  
	{ 0x0436, UNKNOWN, L"AF",	L"Afrikaans" },  
	{ 0x041c, UNKNOWN, L"SQ",	L"Albanian" },  
	{ 0x0401, UNKNOWN, L"AR",	L"Arabic (Saudi Arabia)" },  
	{ 0x0801, UNKNOWN, L"AR",	L"Arabic (Iraq)" },  
	{ 0x0c01, UNKNOWN, L"AR",	L"Arabic (Egypt)" },  
	{ 0x1001, UNKNOWN, L"AR",	L"Arabic (Libya)" },  
	{ 0x1401, UNKNOWN, L"AR",	L"Arabic (Algeria)" },  
	{ 0x1801, UNKNOWN, L"AR",	L"Arabic (Morocco)" },  
	{ 0x1c01, UNKNOWN, L"AR",	L"Arabic (Tunisia)" },  
	{ 0x2001, UNKNOWN, L"AR",	L"Arabic (Oman)" },  
	{ 0x2401, UNKNOWN, L"AR",	L"Arabic (Yemen)" },  
	{ 0x2801, UNKNOWN, L"AR",	L"Arabic (Syria)" },  
	{ 0x2c01, UNKNOWN, L"AR",	L"Arabic (Jordan)" },  
	{ 0x3001, UNKNOWN, L"AR",	L"Arabic (Lebanon)" },  
	{ 0x3401, UNKNOWN, L"AR",	L"Arabic (Kuwait)" },  
	{ 0x3801, UNKNOWN, L"AR",	L"Arabic (U.A.E.)" },  
	{ 0x3c01, UNKNOWN, L"AR",	L"Arabic (Bahrain)" },  
	{ 0x4001, UNKNOWN, L"AR",	L"Arabic (Qatar)" },  
	{ 0x042b, UNKNOWN, L"HY",	L"Armenian" },  
	{ 0x042c, UNKNOWN, L"AZ",	L"Azeri (Latin)" },  
	{ 0x082c, UNKNOWN, L"AZ",	L"Azeri (Cyrillic)" },  
	{ 0x042d, UNKNOWN, L"ES",	L"Basque" },  
	{ 0x0423, UNKNOWN, L"BE",	L"Belarusian" },  
	{ 0x0445, UNKNOWN, L"",		L"Bengali (India)" },  
	{ 0x141a, UNKNOWN, L"",		L"Bosnian (Bosnia and Herzegovina)" },  
	{ 0x0402, UNKNOWN, L"BG",	L"Bulgarian" },  
	{ 0x0455, UNKNOWN, L"",		L"Burmese" },  
	{ 0x0403, UNKNOWN, L"CA",	L"Catalan" },  
	{ 0x0404, TRADITIONAL_CHINESE, L"CHT", L"#IME_0404", true },  
	{ 0x0804, SIMPLIFIED_CHINESE, L"CHS", L"#IME_0804", true },  
	{ 0x0c04, UNKNOWN, L"CH",	L"Chinese (Hong Kong SAR, PRC)" },  
	{ 0x1004, UNKNOWN, L"CH",	L"Chinese (Singapore)" },  
	{ 0x1404, UNKNOWN, L"CH",	L"Chinese (Macao SAR)" },  
	{ 0x041a, UNKNOWN, L"HR",	L"Croatian" },  
	{ 0x101a, UNKNOWN, L"HR",	L"Croatian (Bosnia and Herzegovina)" },  
	{ 0x0405, UNKNOWN, L"CZ",	L"Czech" },  
	{ 0x0406, UNKNOWN, L"DK",	L"Danish" },  
	{ 0x0465, UNKNOWN, L"MV",	L"Divehi" },  
	{ 0x0413, UNKNOWN, L"NL",	L"Dutch (Netherlands)" },  
	{ 0x0813, UNKNOWN, L"BE",	L"Dutch (Belgium)" },  
	{ 0x0409, ENGLISH, L"EN",	L"#IME_0409" },  
	{ 0x0809, ENGLISH, L"EN",	L"English (United Kingdom)" },  
	{ 0x0c09, ENGLISH, L"EN",	L"English (Australian)" },  
	{ 0x1009, ENGLISH, L"EN",	L"English (Canadian)" },  
	{ 0x1409, ENGLISH, L"EN",	L"English (New Zealand)" },  
	{ 0x1809, ENGLISH, L"EN",	L"English (Ireland)" },  
	{ 0x1c09, ENGLISH, L"EN",	L"English (South Africa)" },  
	{ 0x2009, ENGLISH, L"EN",	L"English (Jamaica)" },  
	{ 0x2409, ENGLISH, L"EN",	L"English (Caribbean)" },  
	{ 0x2809, ENGLISH, L"EN",	L"English (Belize)" },  
	{ 0x2c09, ENGLISH, L"EN",	L"English (Trinidad)" },  
	{ 0x3009, ENGLISH, L"EN",	L"English (Zimbabwe)" },  
	{ 0x3409, ENGLISH, L"EN",	L"English (Philippines)" },  
	{ 0x0425, UNKNOWN, L"ET",	L"Estonian" },  
	{ 0x0438, UNKNOWN, L"FO",	L"Faeroese" },  
	{ 0x0429, UNKNOWN, L"FA",	L"Farsi" },  
	{ 0x040b, UNKNOWN, L"FI",	L"Finnish" },  
	{ 0x040c, UNKNOWN, L"FR",	L"#IME_040c" },  
	{ 0x080c, UNKNOWN, L"FR",	L"French (Belgian)" },  
	{ 0x0c0c, UNKNOWN, L"FR",	L"French (Canadian)" },  
	{ 0x100c, UNKNOWN, L"FR",	L"French (Switzerland)" },  
	{ 0x140c, UNKNOWN, L"FR",	L"French (Luxembourg)" },  
	{ 0x180c, UNKNOWN, L"FR",	L"French (Monaco)" },  
	{ 0x0456, UNKNOWN, L"GL",	L"Galician" },  
	{ 0x0437, UNKNOWN, L"KA",	L"Georgian" },  
	{ 0x0407, UNKNOWN, L"DE",	L"#IME_0407" },  
	{ 0x0807, UNKNOWN, L"DE",	L"German (Switzerland)" },  
	{ 0x0c07, UNKNOWN, L"DE",	L"German (Austria)" },  
	{ 0x1007, UNKNOWN, L"DE",	L"German (Luxembourg)" },  
	{ 0x1407, UNKNOWN, L"DE",	L"German (Liechtenstein)" },  
	{ 0x0408, UNKNOWN, L"GR",	L"Greek" },  
	{ 0x0447, UNKNOWN, L"IN",	L"Gujarati" },  
	{ 0x040d, UNKNOWN, L"HE",	L"Hebrew" },  
	{ 0x0439, UNKNOWN, L"HI",	L"Hindi" },  
	{ 0x040e, UNKNOWN, L"HU",	L"Hungarian" },  
	{ 0x040f, UNKNOWN, L"IS",	L"Icelandic" },  
	{ 0x0421, UNKNOWN, L"ID",	L"Indonesian" },  
	{ 0x0434, UNKNOWN, L"",		L"isiXhosa/Xhosa (South Africa)" },  
	{ 0x0435, UNKNOWN, L"",		L"isiZulu/Zulu (South Africa)" },  
	{ 0x0410, UNKNOWN, L"IT",	L"#IME_0410" },  
	{ 0x0810, UNKNOWN, L"IT",	L"Italian (Switzerland)" },  
	{ 0x0411, JAPANESE, L"JP",	L"#IME_0411" },  
	{ 0x044b, UNKNOWN, L"IN",	L"Kannada" },  
	{ 0x0457, UNKNOWN, L"IN",	L"Konkani" },  
	{ 0x0412, KOREAN, L"KR",	L"#IME_0412" },  
	{ 0x0812, UNKNOWN, L"KR",	L"Korean (Johab)" },  
	{ 0x0440, UNKNOWN, L"KZ",	L"Kyrgyz." },  
	{ 0x0426, UNKNOWN, L"LV",	L"Latvian" },  
	{ 0x0427, UNKNOWN, L"LT",	L"Lithuanian" },  
	{ 0x0827, UNKNOWN, L"LT",	L"Lithuanian (Classic)" },  
	{ 0x042f, UNKNOWN, L"MK",	L"FYRO Macedonian" },  
	{ 0x043e, UNKNOWN, L"MY",	L"Malay (Malaysian)" },  
	{ 0x083e, UNKNOWN, L"MY",	L"Malay (Brunei Darussalam)" },  
	{ 0x044c, UNKNOWN, L"IN",	L"Malayalam (India)" },  
	{ 0x0481, UNKNOWN, L"",		L"Maori (New Zealand)" },  
	{ 0x043a, UNKNOWN, L"",		L"Maltese (Malta)" },  
	{ 0x044e, UNKNOWN, L"IN",	L"Marathi" },  
	{ 0x0450, UNKNOWN, L"MN",	L"Mongolian" },  
	{ 0x0414, UNKNOWN, L"NO",	L"Norwegian (Bokmal)" },  
	{ 0x0814, UNKNOWN, L"NO",	L"Norwegian (Nynorsk)" },  
	{ 0x0415, UNKNOWN, L"PL",	L"Polish" },  
	{ 0x0416, UNKNOWN, L"PT",	L"Portuguese (Brazil)" },  
	{ 0x0816, UNKNOWN, L"PT",	L"Portuguese (Portugal)" },  
	{ 0x0446, UNKNOWN, L"IN",	L"Punjabi" },  
	{ 0x046b, UNKNOWN, L"",		L"Quechua (Bolivia)" },  
	{ 0x086b, UNKNOWN, L"",		L"Quechua (Ecuador)" },  
	{ 0x0c6b, UNKNOWN, L"",		L"Quechua (Peru)" },  
	{ 0x0418, UNKNOWN, L"RO",	L"Romanian" },  
	{ 0x0419, UNKNOWN, L"RU",	L"#IME_0419" },  
	{ 0x044f, UNKNOWN, L"IN",	L"Sanskrit" },  
	{ 0x043b, UNKNOWN, L"",		L"Sami, Northern (Norway)" },  
	{ 0x083b, UNKNOWN, L"",		L"Sami, Northern (Sweden)" },  
	{ 0x0c3b, UNKNOWN, L"",		L"Sami, Northern (Finland)" },  
	{ 0x103b, UNKNOWN, L"",		L"Sami, Lule (Norway)" },  
	{ 0x143b, UNKNOWN, L"",		L"Sami, Lule (Sweden)" },  
	{ 0x183b, UNKNOWN, L"",		L"Sami, Southern (Norway)" },  
	{ 0x1c3b, UNKNOWN, L"",		L"Sami, Southern (Sweden)" },  
	{ 0x203b, UNKNOWN, L"",		L"Sami, Skolt (Finland)" },  
	{ 0x243b, UNKNOWN, L"",		L"Sami, Inari (Finland)" },  
	{ 0x0c1a, UNKNOWN, L"SR",	L"Serbian (Cyrillic)" },  
	{ 0x1c1a, UNKNOWN, L"SR",	L"Serbian (Cyrillic, Bosnia, and Herzegovina)" },  
	{ 0x081a, UNKNOWN, L"SR",	L"Serbian (Latin)" },  
	{ 0x181a, UNKNOWN, L"SR",	L"Serbian (Latin, Bosnia, and Herzegovina)" }, 
	{ 0x046c, UNKNOWN, L"",		L"Sesotho sa Leboa/Northern Sotho (South Africa)" },  
	{ 0x0432, UNKNOWN, L"",		L"Setswana/Tswana (South Africa)" },  
	{ 0x041b, UNKNOWN, L"SK",	L"Slovak" },  
	{ 0x0424, UNKNOWN, L"SI",	L"Slovenian" },  
	{ 0x040a, UNKNOWN, L"ES",	L"#IME_040a" },  
	{ 0x080a, UNKNOWN, L"ES",	L"Spanish (Mexican)" },  
	{ 0x0c0a, UNKNOWN, L"ES",	L"Spanish (Spain, Modern Sort)" },  
	{ 0x100a, UNKNOWN, L"ES",	L"Spanish (Guatemala)" },  
	{ 0x140a, UNKNOWN, L"ES",	L"Spanish (Costa Rica)" },  
	{ 0x180a, UNKNOWN, L"ES",	L"Spanish (Panama)" },  
	{ 0x1c0a, UNKNOWN, L"ES",	L"Spanish (Dominican Republic)" },  
	{ 0x200a, UNKNOWN, L"ES",	L"Spanish (Venezuela)" },  
	{ 0x240a, UNKNOWN, L"ES",	L"Spanish (Colombia)" },  
	{ 0x280a, UNKNOWN, L"ES",	L"Spanish (Peru)" },  
	{ 0x2c0a, UNKNOWN, L"ES",	L"Spanish (Argentina)" },  
	{ 0x300a, UNKNOWN, L"ES",	L"Spanish (Ecuador)" },  
	{ 0x340a, UNKNOWN, L"ES",	L"Spanish (Chile)" },  
	{ 0x380a, UNKNOWN, L"ES",	L"Spanish (Uruguay)" },  
	{ 0x3c0a, UNKNOWN, L"ES",	L"Spanish (Paraguay)" },  
	{ 0x400a, UNKNOWN, L"ES",	L"Spanish (Bolivia)" },  
	{ 0x440a, UNKNOWN, L"ES",	L"Spanish (El Salvador)" },  
	{ 0x480a, UNKNOWN, L"ES",	L"Spanish (Honduras)" },  
	{ 0x4c0a, UNKNOWN, L"ES",	L"Spanish (Nicaragua)" },  
	{ 0x500a, UNKNOWN, L"ES",	L"Spanish (Puerto Rico)" },  
	{ 0x0430, UNKNOWN, L"",		L"Sutu" },  
	{ 0x0441, UNKNOWN, L"KE",	L"Swahili (Kenya)" },  
	{ 0x041d, UNKNOWN, L"SV",	L"Swedish" },  
	{ 0x081d, UNKNOWN, L"SV",	L"Swedish (Finland)" },  
	{ 0x045a, UNKNOWN, L"SY",	L"Syriac" },  
	{ 0x0449, UNKNOWN, L"IN",	L"Tamil" },  
	{ 0x0444, UNKNOWN, L"RU",	L"Tatar (Tatarstan)" },  
	{ 0x044a, UNKNOWN, L"IN",	L"Telugu" },  
	{ 0x041e, UNKNOWN, L"TH",	L"#IME_041e" },  
	{ 0x041f, UNKNOWN, L"TR",	L"Turkish" },  
	{ 0x0422, UNKNOWN, L"UA",	L"Ukrainian" },  
	{ 0x0420, UNKNOWN, L"PK",	L"Urdu (Pakistan)" },  
	{ 0x0820, UNKNOWN, L"IN",	L"Urdu (India)" },  
	{ 0x0443, UNKNOWN, L"UZ",	L"Uzbek (Latin)" },  
	{ 0x0843, UNKNOWN, L"UZ",	L"Uzbek (Cyrillic)" },  
	{ 0x042a, UNKNOWN, L"VN",	L"Vietnamese" },  
	{ 0x0452, UNKNOWN, L"",		L"Welsh (United Kingdom)" }, 
};

static LanguageIds *GetLanguageInfo( unsigned short id )
{
	for ( int j = 0; j < sizeof( g_LanguageIds ) / sizeof( g_LanguageIds[ 0 ] ); ++j )
	{
		if ( g_LanguageIds[ j ].id == id )
		{
			return &g_LanguageIds[ j ];
			break;
		}
	}
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////
// CIMEDlg message handlers
static bool IsIDInList( unsigned short id, int count, HKL *list )
{
	for ( int i = 0; i < count; ++i )
	{
		if ( LOWORD( list[ i ] ) == id )
		{
			return true;
		}
	}
	return false;
}

static const wchar_t *GetLanguageName( unsigned short id )
{
	wchar_t const *name = L"???";
	for ( int j = 0; j < sizeof( g_LanguageIds ) / sizeof( g_LanguageIds[ 0 ] ); ++j )
	{
		if ( g_LanguageIds[ j ].id == id )
		{
			name = g_LanguageIds[ j ].displayname;
			break;
		}
	}
	return name;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *hwnd - 
//-----------------------------------------------------------------------------
void CInputGameUI::SetIMEWindow( void *hwnd )
{
#ifndef _GAMECONSOLE
	_imeWnd = hwnd;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void *CInputGameUI::GetIMEWindow()
{
#ifndef _GAMECONSOLE
	return _imeWnd;
#else
	return NULL;
#endif
}

static void SpewIMEInfo( int langid )
{
	LanguageIds *info = GetLanguageInfo( langid );
	if ( info )
	{
		wchar_t const *name = info->shortcode ? info->shortcode : L"???";
		wchar_t outstr[ 512 ];
		_snwprintf( outstr, sizeof( outstr ) / sizeof( wchar_t ), L"IME language changed to:  %s", name );
		OutputDebugStringW( outstr );
		OutputDebugStringW( L"\n" );
	}
}

// Change keyboard layout type
void CInputGameUI::OnChangeIME( bool forward )
{
#ifndef _GAMECONSOLE
	HKL currentKb = GetKeyboardLayout( 0 );

	UINT numKBs = GetKeyboardLayoutList( 0, NULL );
	if ( numKBs > 0 )
	{
		HKL *list = new HKL[ numKBs ];

		GetKeyboardLayoutList( numKBs, list );

		int oldKb = 0;
		CUtlVector< HKL >	selections;

		for ( unsigned int i = 0; i < numKBs; ++i )
		{
			BOOL first = !IsIDInList( LOWORD( list[ i ] ), i, list );

			if ( !first )
				continue;

			selections.AddToTail( list[ i ] );
			if ( list[ i ] == currentKb )
			{
				oldKb = selections.Count() - 1;
			}
		}

		oldKb += forward ? 1 : -1;
		if ( oldKb < 0 )
		{
			oldKb = max( 0, selections.Count() - 1 );
		}
		else if ( oldKb >= selections.Count() )
		{
			oldKb = 0;
		}

		ActivateKeyboardLayout( selections[ oldKb ], 0 );

		int langid = LOWORD( selections[ oldKb ] );
		SpewIMEInfo( langid );

		delete[] list;
	}
#endif
}

int CInputGameUI::GetCurrentIMEHandle()
{
#ifndef _GAMECONSOLE
	HKL hkl = (HKL)GetKeyboardLayout( 0 );
	return (int)hkl;
#else
	return 0;
#endif
}

int CInputGameUI::GetEnglishIMEHandle()
{
#ifndef _GAMECONSOLE
	HKL hkl = (HKL)0x04090409;
	return (int)hkl;
#else
	return 0;
#endif
}

void CInputGameUI::OnChangeIMEByHandle( int handleValue )
{
#ifndef _GAMECONSOLE
	HKL hkl = (HKL)handleValue;

	ActivateKeyboardLayout( hkl, 0 );

	int langid = LOWORD( hkl);

	SpewIMEInfo( langid );
#endif
}

// Returns the Language Bar label (Chinese, Korean, Japanese, Russion, Thai, etc.)
void CInputGameUI::GetIMELanguageName( wchar_t *buf, int unicodeBufferSizeInBytes )
{
#ifndef _GAMECONSOLE
	wchar_t const *name = GetLanguageName( LOWORD( GetKeyboardLayout( 0 ) ) );
	wcsncpy( buf, name, unicodeBufferSizeInBytes / sizeof( wchar_t ) - 1 );
	buf[ unicodeBufferSizeInBytes / sizeof( wchar_t ) - 1 ] = L'\0';
#else
	buf[0] = L'\0';
#endif
}
// Returns the short code for the language (EN, CH, KO, JP, RU, TH, etc. ).
void CInputGameUI::GetIMELanguageShortCode( wchar_t *buf, int unicodeBufferSizeInBytes )
{
#ifndef _GAMECONSOLE
	LanguageIds *info = GetLanguageInfo( LOWORD( GetKeyboardLayout( 0 ) ) );
	if ( !info )
	{
		buf[ 0 ] = L'\0';
	}
	else
	{
		wcsncpy( buf, info->shortcode, unicodeBufferSizeInBytes / sizeof( wchar_t ) - 1 );
		buf[ unicodeBufferSizeInBytes / sizeof( wchar_t ) - 1 ] = L'\0';
	}
#else
	buf[0] = L'\0';
#endif
}

// Call with NULL dest to get item count
int CInputGameUI::GetIMELanguageList( LanguageItem *dest, int destcount )
{
#ifndef _GAMECONSOLE
	int iret = 0;

	UINT numKBs = GetKeyboardLayoutList( 0, NULL );
	if ( numKBs > 0 )
	{
		HKL *list = new HKL[ numKBs ];

		GetKeyboardLayoutList( numKBs, list );

		CUtlVector< HKL >	selections;

		for ( unsigned int i = 0; i < numKBs; ++i )
		{
			BOOL first = !IsIDInList( LOWORD( list[ i ] ), i, list );

			if ( !first )
				continue;

			selections.AddToTail( list[ i ] );
		}

		iret = selections.Count();
		if ( dest )
		{
			for ( int i = 0; i < min(iret,destcount); ++i )
			{
				HKL hkl = selections[ i ];

				LanguageItem *p = &dest[ i ];

				LanguageIds *info = GetLanguageInfo( LOWORD( hkl ) );

				memset( p, 0, sizeof( IInput::LanguageItem ) );

				wcsncpy( p->shortname, info->shortcode, sizeof( p->shortname ) / sizeof( wchar_t ) );
				p->shortname[ sizeof( p->shortname ) / sizeof( wchar_t ) - 1 ] = L'\0';

				wcsncpy( p->menuname, info->displayname, sizeof( p->menuname ) / sizeof( wchar_t ) );
				p->menuname[ sizeof( p->menuname ) / sizeof( wchar_t ) - 1 ] = L'\0';

				p->handleValue = (int)hkl;
				p->active = ( hkl == GetKeyboardLayout( 0 ) ) ? true : false;
			}
		}

		delete[] list;
	}
	return iret;
#else
	return 0;
#endif
}

/*
// Flag for effective options in conversion mode 
BOOL fConvMode[NUM_IMES_SUPPORTED][13] = 
{	
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	// EN
{1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0},	// Trad CH
{1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1},	// Japanese
{1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},	// Kor
{1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0},	// Simp CH
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	// UNK(same as EN)
}

// Flag for effective options in sentence mode 
BOOL fSentMode[NUM_IMES_SUPPORTED][6] = 
{	
{0, 0, 0, 0, 0, 0},		// EN
{0, 1, 0, 0, 0, 0},		// Trad CH
{1, 1, 1, 1, 1, 1},		// Japanese
{0, 0, 0, 0, 0, 0},		// Kor
{0, 0, 0, 0, 0, 0}		// Simp CH
{0, 0, 0, 0, 0, 0},		// UNK(same as EN)
};

// Conversion mode message 
DWORD dwConvModeMsg[13] = {
IME_CMODE_ALPHANUMERIC,		IME_CMODE_NATIVE,		IME_CMODE_KATAKANA, 
IME_CMODE_LANGUAGE,			IME_CMODE_FULLSHAPE,	IME_CMODE_ROMAN, 
IME_CMODE_CHARCODE,			IME_CMODE_HANJACONVERT, IME_CMODE_SOFTKBD, 
IME_CMODE_NOCONVERSION,		IME_CMODE_EUDC,			IME_CMODE_SYMBOL, 
IME_CMODE_FIXED};

// Sentence mode message 
DWORD dwSentModeMsg[6] = {
IME_SMODE_NONE,			IME_SMODE_PLAURALCLAUSE,	IME_SMODE_SINGLECONVERT,	
IME_SMODE_AUTOMATIC,	IME_SMODE_PHRASEPREDICT,	IME_SMODE_CONVERSATION };

//	ENGLISH,				
//	TRADITIONAL_CHINESE,	
//	JAPANESE,
//	KOREAN,
//	SIMPLIFIED_CHINESE,
//	UNKNOWN,
*/

#ifndef _GAMECONSOLE

struct IMESettingsTransform
{
	IMESettingsTransform( unsigned int cmr, unsigned int cma, unsigned int smr, unsigned int sma ) :
cmode_remove( cmr ),
cmode_add( cma ),
smode_remove( smr ),
smode_add( sma )
{
}

void Apply( HWND hwnd )
{
	HIMC hImc = ImmGetContext( hwnd );
	if ( hImc )
	{
		DWORD	dwConvMode, dwSentMode;

		ImmGetConversionStatus( hImc, &dwConvMode, &dwSentMode );

		dwConvMode &= ~cmode_remove;
		dwSentMode &= ~smode_remove;

		ImmSetConversionStatus( hImc, dwConvMode, dwSentMode );

		dwConvMode |= cmode_add;
		dwSentMode |= smode_add;

		ImmSetConversionStatus( hImc, dwConvMode, dwSentMode );

		ImmReleaseContext( hwnd, hImc );
	}
}

bool ConvMatches( DWORD convFlags )
{
	// To match, the active flags have to have none of the remove flags and have to have all of the "add" flags
	if ( convFlags & cmode_remove )
		return false;

	if ( ( convFlags & cmode_add ) == cmode_add )
	{
		return true;
	}
	return false;
}

bool SentMatches( DWORD sentFlags )
{
	// To match, the active flags have to have none of the remove flags and have to have all of the "add" flags
	if ( sentFlags & smode_remove )
		return false;

	if ( ( sentFlags & smode_add ) == smode_add )
	{
		return true;
	}
	return false;
}

unsigned int		cmode_remove;
unsigned int		cmode_add;
unsigned int		smode_remove;
unsigned int		smode_add;
};

static IMESettingsTransform g_ConversionMode_CHT_ToChinese( 
	IME_CMODE_ALPHANUMERIC,
	IME_CMODE_NATIVE | IME_CMODE_LANGUAGE,
	0,
	0 );
static IMESettingsTransform g_ConversionMode_CHT_ToEnglish( 
	IME_CMODE_NATIVE | IME_CMODE_LANGUAGE,
	IME_CMODE_ALPHANUMERIC,
	0,
	0 );

static IMESettingsTransform g_ConversionMode_CHS_ToChinese( 
	IME_CMODE_ALPHANUMERIC,
	IME_CMODE_NATIVE | IME_CMODE_LANGUAGE,
	0,
	0 );
static IMESettingsTransform g_ConversionMode_CHS_ToEnglish( 
	IME_CMODE_NATIVE | IME_CMODE_LANGUAGE,
	IME_CMODE_ALPHANUMERIC,
	0,
	0 );

static IMESettingsTransform g_ConversionMode_KO_ToKorean( 
	IME_CMODE_ALPHANUMERIC,
	IME_CMODE_NATIVE | IME_CMODE_LANGUAGE,
	0,
	0 );

static IMESettingsTransform g_ConversionMode_KO_ToEnglish( 
	IME_CMODE_NATIVE | IME_CMODE_LANGUAGE,
	IME_CMODE_ALPHANUMERIC,
	0,
	0 );

static IMESettingsTransform g_ConversionMode_JP_Hiragana( 
	IME_CMODE_ALPHANUMERIC | IME_CMODE_KATAKANA,
	IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE,
	0,
	0 );

static IMESettingsTransform g_ConversionMode_JP_DirectInput( 
	IME_CMODE_NATIVE | ( IME_CMODE_KATAKANA | IME_CMODE_LANGUAGE ) | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
	IME_CMODE_ALPHANUMERIC,
	0,
	0 );

static IMESettingsTransform g_ConversionMode_JP_FullwidthKatakana( 
	IME_CMODE_ALPHANUMERIC,
	IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN | IME_CMODE_KATAKANA | IME_CMODE_LANGUAGE,
	0,
	0 );

static IMESettingsTransform g_ConversionMode_JP_HalfwidthKatakana( 
	IME_CMODE_ALPHANUMERIC | IME_CMODE_FULLSHAPE,
	IME_CMODE_NATIVE | IME_CMODE_ROMAN | ( IME_CMODE_KATAKANA | IME_CMODE_LANGUAGE ),
	0,
	0 );

static IMESettingsTransform g_ConversionMode_JP_FullwidthAlphanumeric( 
	IME_CMODE_NATIVE | ( IME_CMODE_KATAKANA | IME_CMODE_LANGUAGE ),
	IME_CMODE_ALPHANUMERIC | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
	0,
	0 );

static IMESettingsTransform g_ConversionMode_JP_HalfwidthAlphanumeric( 
	IME_CMODE_NATIVE | ( IME_CMODE_KATAKANA | IME_CMODE_LANGUAGE ) | IME_CMODE_FULLSHAPE,
	IME_CMODE_ALPHANUMERIC | IME_CMODE_ROMAN,
	0,
	0 );

#endif // _GAMECONSOLE

int CInputGameUI::GetIMEConversionModes( ConversionModeItem *dest, int destcount )
{
#ifndef _GAMECONSOLE
	if ( dest )
	{
		memset( dest, 0, destcount * sizeof( ConversionModeItem ) );
	}

	DWORD	dwConvMode = 0, dwSentMode = 0;

	HIMC hImc = ImmGetContext( ( HWND )GetIMEWindow() );
	if ( hImc )
	{
		ImmGetConversionStatus( hImc, &dwConvMode, &dwSentMode );
		ImmReleaseContext( ( HWND )GetIMEWindow(), hImc );
	}

	LanguageIds *info = GetLanguageInfo( LOWORD( GetKeyboardLayout( 0 ) ) );
	switch ( info->languageflag )
	{
	default:
		return 0;
	case TRADITIONAL_CHINESE:
		// This is either native or alphanumeric
		if ( dest )
		{
			ConversionModeItem *item;
			int i = 0;
			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_Chinese", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_CHT_ToChinese;
			item->active = g_ConversionMode_CHT_ToChinese.ConvMatches( dwConvMode );

			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_English", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_CHT_ToEnglish;
			item->active = g_ConversionMode_CHT_ToEnglish.ConvMatches( dwConvMode );
		}
		return 2;
	case JAPANESE:
		// There are 6 Japanese modes
		if ( dest )
		{
			ConversionModeItem *item;

			int i = 0;
			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_Hiragana", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_JP_Hiragana;
			item->active = g_ConversionMode_JP_Hiragana.ConvMatches( dwConvMode );

			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_FullWidthKatakana", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_JP_FullwidthKatakana;
			item->active = g_ConversionMode_JP_FullwidthKatakana.ConvMatches( dwConvMode );

			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_FullWidthAlphanumeric", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_JP_FullwidthAlphanumeric;
			item->active = g_ConversionMode_JP_FullwidthAlphanumeric.ConvMatches( dwConvMode );

			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_HalfWidthKatakana", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_JP_HalfwidthKatakana;
			item->active = g_ConversionMode_JP_HalfwidthKatakana.ConvMatches( dwConvMode );

			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_HalfWidthAlphanumeric", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_JP_HalfwidthAlphanumeric;
			item->active = g_ConversionMode_JP_HalfwidthAlphanumeric.ConvMatches( dwConvMode );

			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_English", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_JP_DirectInput;
			item->active = g_ConversionMode_JP_DirectInput.ConvMatches( dwConvMode );

		}
		return 6;
	case KOREAN:
		// This is either native or alphanumeric
		if ( dest )
		{
			ConversionModeItem *item;
			int i = 0;
			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_Korean", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_KO_ToKorean;
			item->active = g_ConversionMode_KO_ToKorean.ConvMatches( dwConvMode );

			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_English", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_KO_ToEnglish;
			item->active = g_ConversionMode_KO_ToEnglish.ConvMatches( dwConvMode );
		}
		return 2;
	case SIMPLIFIED_CHINESE:
		// This is either native or alphanumeric
		if ( dest )
		{
			ConversionModeItem *item;
			int i = 0;
			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_Chinese", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_CHS_ToChinese;
			item->active = g_ConversionMode_CHS_ToChinese.ConvMatches( dwConvMode );

			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_English", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_ConversionMode_CHS_ToChinese;
			item->active = g_ConversionMode_CHS_ToChinese.ConvMatches( dwConvMode );
		}
		return 2;
	}
#endif
	return 0;
}

#ifndef _GAMECONSOLE

static IMESettingsTransform g_SentenceMode_JP_None( 
	0,
	0,
	IME_SMODE_PLAURALCLAUSE | IME_SMODE_SINGLECONVERT | IME_SMODE_AUTOMATIC | IME_SMODE_PHRASEPREDICT | IME_SMODE_CONVERSATION,
	IME_SMODE_NONE );

static IMESettingsTransform g_SentenceMode_JP_General( 
	0,
	0,
	IME_SMODE_NONE | IME_SMODE_PLAURALCLAUSE | IME_SMODE_SINGLECONVERT | IME_SMODE_AUTOMATIC | IME_SMODE_CONVERSATION,
	IME_SMODE_PHRASEPREDICT
	);

static IMESettingsTransform g_SentenceMode_JP_BiasNames( 
	0,
	0,
	IME_SMODE_NONE | IME_SMODE_PHRASEPREDICT | IME_SMODE_SINGLECONVERT | IME_SMODE_AUTOMATIC | IME_SMODE_CONVERSATION,
	IME_SMODE_PLAURALCLAUSE
	);

static IMESettingsTransform g_SentenceMode_JP_BiasSpeech( 
	0,
	0,
	IME_SMODE_NONE | IME_SMODE_PHRASEPREDICT | IME_SMODE_SINGLECONVERT | IME_SMODE_AUTOMATIC | IME_SMODE_PLAURALCLAUSE,
	IME_SMODE_CONVERSATION
	);

#endif // _GAMECONSOLE

int CInputGameUI::GetIMESentenceModes( SentenceModeItem *dest, int destcount )
{
#ifndef _GAMECONSOLE
	if ( dest )
	{
		memset( dest, 0, destcount * sizeof( SentenceModeItem ) );
	}

	DWORD	dwConvMode = 0, dwSentMode = 0;

	HIMC hImc = ImmGetContext( ( HWND )GetIMEWindow() );
	if ( hImc )
	{
		ImmGetConversionStatus( hImc, &dwConvMode, &dwSentMode );
		ImmReleaseContext( ( HWND )GetIMEWindow(), hImc );
	}

	LanguageIds *info = GetLanguageInfo( LOWORD( GetKeyboardLayout( 0 ) ) );
	switch ( info->languageflag )
	{
	default:
		return 0;
		//	case TRADITIONAL_CHINESE:
		//		break;
	case JAPANESE:
		// There are 4 Japanese sentence modes
		if ( dest )
		{
			SentenceModeItem *item;

			int i = 0;
			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_General", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_SentenceMode_JP_General;
			item->active = g_SentenceMode_JP_General.SentMatches( dwSentMode );

			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_BiasNames", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_SentenceMode_JP_BiasNames;
			item->active = g_SentenceMode_JP_BiasNames.SentMatches( dwSentMode );

			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_BiasSpeech", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_SentenceMode_JP_BiasSpeech;
			item->active = g_SentenceMode_JP_BiasSpeech.SentMatches( dwSentMode );

			item = &dest[ i++ ];
			wcsncpy( item->menuname, L"#IME_NoConversion", sizeof( item->menuname ) / sizeof( wchar_t ) );
			item->handleValue = (int)&g_SentenceMode_JP_None;
			item->active = g_SentenceMode_JP_None.SentMatches( dwSentMode );
		}
		return 4;
	}
#endif
	return 0;
}

void CInputGameUI::OnChangeIMEConversionModeByHandle( int handleValue )
{
#ifndef _GAMECONSOLE
	if ( handleValue == 0 )
		return;

	IMESettingsTransform *txform = ( IMESettingsTransform * )handleValue;
	txform->Apply( (HWND)GetIMEWindow() );
#endif
}

void CInputGameUI::OnChangeIMESentenceModeByHandle( int handleValue )
{
}

void CInputGameUI::OnInputLanguageChanged()
{
}

void CInputGameUI::OnIMEStartComposition()
{
}

void DescribeIMEFlag( char const *string, bool value )
{
	if ( value )
	{
		Msg( "   %s\n", string );
	}
}

#define IMEDesc( x )	DescribeIMEFlag( #x, flags & x );

void CInputGameUI::OnIMEComposition( int flags )
{
#ifndef _GAMECONSOLE
	/*
	Msg( "OnIMEComposition\n" );

	IMEDesc( VGUI_GCS_COMPREADSTR );
	IMEDesc( VGUI_GCS_COMPREADATTR );
	IMEDesc( VGUI_GCS_COMPREADCLAUSE );
	IMEDesc( VGUI_GCS_COMPSTR );
	IMEDesc( VGUI_GCS_COMPATTR );
	IMEDesc( VGUI_GCS_COMPCLAUSE );
	IMEDesc( VGUI_GCS_CURSORPOS );
	IMEDesc( VGUI_GCS_DELTASTART );
	IMEDesc( VGUI_GCS_RESULTREADSTR );
	IMEDesc( VGUI_GCS_RESULTREADCLAUSE );
	IMEDesc( VGUI_GCS_RESULTSTR );
	IMEDesc( VGUI_GCS_RESULTCLAUSE );
	IMEDesc( VGUI_CS_INSERTCHAR );
	IMEDesc( VGUI_CS_NOMOVECARET );
	*/

	HIMC hIMC = ImmGetContext( ( HWND )GetIMEWindow() );
	if ( hIMC )
	{
		if ( flags & VGUI_GCS_RESULTSTR )
		{
			wchar_t tempstr[ 32 ];

			int len = ImmGetCompositionStringW( hIMC, GCS_RESULTSTR, (LPVOID)tempstr, sizeof( tempstr ) );
			if ( len > 0 )
			{
				if ((len % 2) != 0)
					len++;
				int numchars = len / sizeof( wchar_t );

				for ( int i = 0; i < numchars; ++i )
				{
					InternalKeyTyped( tempstr[ i ] );
				}
			}
		}
		if ( flags & VGUI_GCS_COMPSTR )
		{
			wchar_t tempstr[ 256 ];

			int len = ImmGetCompositionStringW( hIMC, GCS_COMPSTR, (LPVOID)tempstr, sizeof( tempstr ) );
			if ( len > 0 )
			{
				if ((len % 2) != 0)
					len++;
				int numchars = len / sizeof( wchar_t );
				tempstr[ numchars ] = L'\0';

				InternalSetCompositionString( tempstr );
			}
		}

		ImmReleaseContext( ( HWND )GetIMEWindow(), hIMC );
	}
#endif
}

void CInputGameUI::OnIMEEndComposition()
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext )
	{
		// tell the current focused panel that a key was typed
		PostKeyMessage( new KeyValues( "DoCompositionString", "string", L"" ) );
	}
}

void CInputGameUI::DestroyCandidateList()
{
#ifndef _GAMECONSOLE
	if ( _imeCandidates )
	{
		delete[] (char *)_imeCandidates;
		_imeCandidates = 0;
	}
#endif
}

void CInputGameUI::OnIMEShowCandidates() 
{
#ifndef _GAMECONSOLE
	DestroyCandidateList();
	CreateNewCandidateList();

	InternalShowCandidateWindow();
#endif
}

void CInputGameUI::OnIMECloseCandidates() 
{
#ifndef _GAMECONSOLE
	InternalHideCandidateWindow();
	DestroyCandidateList();
#endif
}

void CInputGameUI::OnIMEChangeCandidates() 
{
#ifndef _GAMECONSOLE
	DestroyCandidateList();
	CreateNewCandidateList();

	InternalUpdateCandidateWindow();
#endif
}

void CInputGameUI::CreateNewCandidateList()
{
#ifndef _GAMECONSOLE
	Assert( !_imeCandidates );

	HIMC hImc = ImmGetContext( ( HWND )GetIMEWindow() );
	if ( hImc )
	{
		DWORD numCandidates = 0;

		DWORD bytes = ImmGetCandidateListCountW( hImc, &numCandidates );
		if ( numCandidates > 0 )
		{
			DWORD buflen = bytes + 1;

			char *buf = new char[ buflen ];
			Q_memset( buf, 0, buflen );

			CANDIDATELIST *list = ( CANDIDATELIST *)buf;
			DWORD copyBytes = ImmGetCandidateListW( hImc, 0, list, buflen );
			if ( copyBytes > 0 )
			{
				_imeCandidates = list;
			}
			else
			{
				delete[] buf;
			}
		}
		ImmReleaseContext( ( HWND )GetIMEWindow(), hImc );
	}
#endif
}

int CInputGameUI::GetCandidateListCount()
{
#ifndef _GAMECONSOLE
	if ( !_imeCandidates )
		return 0;

	return (int)_imeCandidates->dwCount;
#else
	return 0;
#endif
}

void CInputGameUI::GetCandidate( int num, wchar_t *dest, int destSizeBytes )
{
	dest[ 0 ] = L'\0';
#ifndef _GAMECONSOLE
	if ( num < 0 || num >= (int)_imeCandidates->dwCount )
	{
		return;
	}

	DWORD offset = *( DWORD *)( (char *)( _imeCandidates->dwOffset + num ) );
	wchar_t *s = ( wchar_t *)( (char *)_imeCandidates + offset );

	wcsncpy( dest, s, destSizeBytes / sizeof( wchar_t ) - 1 );
	dest[ destSizeBytes / sizeof( wchar_t ) - 1 ] = L'\0';
#endif
}

int CInputGameUI::GetCandidateListSelectedItem()
{
#ifndef _GAMECONSOLE
	if ( !_imeCandidates )
		return 0;

	return (int)_imeCandidates->dwSelection;
#else
	return 0;
#endif
}

int CInputGameUI::GetCandidateListPageSize()
{
#ifndef _GAMECONSOLE
	if ( !_imeCandidates )
		return 0;
	return (int)_imeCandidates->dwPageSize;
#else
	return 0;
#endif
}

int CInputGameUI::GetCandidateListPageStart()
{
#ifndef _GAMECONSOLE
	if ( !_imeCandidates )
		return 0;
	return (int)_imeCandidates->dwPageStart;
#else
	return 0;
#endif
}

void CInputGameUI::SetCandidateListPageStart( int start )
{
#ifndef _GAMECONSOLE
	HIMC hImc = ImmGetContext( ( HWND )GetIMEWindow() );
	if ( hImc )
	{
		ImmNotifyIME( hImc, NI_SETCANDIDATE_PAGESTART, 0, start );
		ImmReleaseContext( ( HWND )GetIMEWindow(), hImc );
	}
#endif
}

void CInputGameUI::OnIMERecomputeModes()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CInputGameUI::CandidateListStartsAtOne()
{
#ifndef _GAMECONSOLE
	DWORD prop = ImmGetProperty( GetKeyboardLayout( 0 ), IGP_PROPERTY );
	if ( prop &	IME_PROP_CANDLIST_START_FROM_1 )
	{
		return true;
	}
#endif
	return false;
}

void CInputGameUI::SetCandidateWindowPos( int x, int y ) 
{
#ifndef _GAMECONSOLE
	POINT		point;
	CANDIDATEFORM Candidate;

	point.x = x;
	point.y = y;

	HIMC hIMC = ImmGetContext( ( HWND )GetIMEWindow() );
	if ( hIMC ) 
	{
		// Set candidate window position near caret position
		Candidate.dwIndex = 0;
		Candidate.dwStyle = CFS_FORCE_POSITION;
		Candidate.ptCurrentPos.x = point.x;
		Candidate.ptCurrentPos.y = point.y;
		ImmSetCandidateWindow( hIMC, &Candidate );

		ImmReleaseContext( ( HWND )GetIMEWindow(),hIMC );
	}
#endif
}

void CInputGameUI::InternalSetCompositionString( const wchar_t *compstr )
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext )
	{
		// tell the current focused panel that a key was typed
		PostKeyMessage( new KeyValues( "DoCompositionString", "string", compstr ) );
	}
}

void CInputGameUI::InternalShowCandidateWindow()
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext )
	{
		PostKeyMessage( new KeyValues( "DoShowIMECandidates" ) );
	}
}

void CInputGameUI::InternalHideCandidateWindow()
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext )
	{
		PostKeyMessage( new KeyValues( "DoHideIMECandidates" ) );
	}
}

void CInputGameUI::InternalUpdateCandidateWindow()
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext )
	{
		PostKeyMessage( new KeyValues( "DoUpdateIMECandidates" ) );
	}
}

bool CInputGameUI::GetShouldInvertCompositionString()
{
#ifndef _GAMECONSOLE
	LanguageIds *info = GetLanguageInfo( LOWORD( GetKeyboardLayout( 0 ) ) );
	if ( !info )
		return false;

	// Only Chinese (simplified and traditional)
	return info->invertcomposition;
#else
	return false;
#endif
}


//-----------------------------------------------------------------------------
// Handles an input event, returns true if the event should be filtered
// from the rest of the game
//-----------------------------------------------------------------------------
bool InputGameUIHandleInputEvent( const InputEvent_t &event )
{
	switch( event.m_nType )
	{
	case IE_ButtonPressed:
		{
			// NOTE: data2 is the virtual key code (data1 contains the scan-code one)
			ButtonCode_t code = (ButtonCode_t)event.m_nData2;
			if ( IsKeyCode( code ) || IsJoystickCode( code ) )
			{
				return g_pInputGameUI->InternalKeyCodePressed( code );
			}

			if ( IsJoystickCode( code ) )
			{
				return g_pInputGameUI->InternalKeyCodePressed( code );
			}

			if ( IsMouseCode( code ) )
			{
				return g_pInputGameUI->InternalMousePressed( code );
			}
		}
		break;

	case IE_ButtonReleased:
		{
			// NOTE: data2 is the virtual key code (data1 contains the scan-code one)
			ButtonCode_t code = (ButtonCode_t)event.m_nData2;
			if ( IsKeyCode( code ) || IsJoystickCode( code ) )
			{
				return g_pInputGameUI->InternalKeyCodeReleased( code );
			}

			if ( IsJoystickCode( code ) )
			{
				return g_pInputGameUI->InternalKeyCodeReleased( code );
			}

			if ( IsMouseCode( code ) )
			{
				return g_pInputGameUI->InternalMouseReleased( code );
			}
		}
		break;

	case IE_ButtonDoubleClicked:
		{
			// NOTE: data2 is the virtual key code (data1 contains the scan-code one)
			ButtonCode_t code = (ButtonCode_t)event.m_nData2;
			if ( IsMouseCode( code ) )
			{
				return g_pInputGameUI->InternalMouseDoublePressed( code );
			}
		}
		break;

	case IE_AnalogValueChanged:
		{
			if ( event.m_nData == MOUSE_WHEEL )
				return g_pInputGameUI->InternalMouseWheeled( event.m_nData3 );
			if ( event.m_nData == MOUSE_XY )
				return g_pInputGameUI->InternalCursorMoved( event.m_nData2, event.m_nData3 );
		}
		break;

	case IE_KeyCodeTyped:
		{
			ButtonCode_t code = (ButtonCode_t)event.m_nData;
			g_pInputGameUI->InternalKeyCodeTyped( code );
		}
		return true;

	case IE_KeyTyped:
		{
			ButtonCode_t code = (ButtonCode_t)event.m_nData;
			g_pInputGameUI->InternalKeyTyped( code );
		}
		return true;

	case IE_Quit:
		return true;

	case IE_Close:
		return true;

	case IE_SetCursor:
		//ActivateCurrentCursor();
		return true;

	case IE_WindowSizeChanged:
		{
			g_pInputGameUI->SetWindowSize( event.m_nData, event.m_nData2 );
		}

	case IE_IMESetWindow:
		g_pInputGameUI->SetIMEWindow( (void *)event.m_nData );
		return true;

	case IE_LocateMouseClick:
		g_pInputGameUI->InternalCursorMoved( event.m_nData, event.m_nData2 );
		return true;

	case IE_InputLanguageChanged:
		g_pInputGameUI->OnInputLanguageChanged();
		return true;

	case IE_IMEStartComposition:
		g_pInputGameUI->OnIMEStartComposition();
		return true;

	case IE_IMEComposition:
		g_pInputGameUI->OnIMEComposition( event.m_nData );
		return true;

	case IE_IMEEndComposition:
		g_pInputGameUI->OnIMEEndComposition();
		return true;

	case IE_IMEShowCandidates:
		g_pInputGameUI->OnIMEShowCandidates();
		return true;

	case IE_IMEChangeCandidates:
		g_pInputGameUI->OnIMEChangeCandidates();
		return true;

	case IE_IMECloseCandidates:
		g_pInputGameUI->OnIMECloseCandidates();
		return true;

	case IE_IMERecomputeModes:
		g_pInputGameUI->OnIMERecomputeModes();
		return true;
	}

	return false;
}

void CInputGameUI::OnCursorEnter( CHitArea* const & pTarget )
{
	Assert( pTarget );
	pTarget->OnCursorEnter();
}

void CInputGameUI::OnCursorExit( CHitArea * const & pTarget )
{
	Assert( pTarget );
	pTarget->OnCursorExit();
}

void CInputGameUI::OnCursorMove( CHitArea * const & pTarget, const int &x, const int &y )
{
	Assert( pTarget );
	pTarget->OnCursorMove( x, y );
}

void CInputGameUI::OnMouseDown( CHitArea * const & pTarget, const ButtonCode_t &code )
{
	Assert( pTarget );
	pTarget->OnMouseDown( code );
}


// Traps get nulled out when the up event is recieved so it is passed in here.
void CInputGameUI::OnMouseUp( CHitArea * const & pTarget, CHitArea * const & pTrap, const ButtonCode_t &code )
{
	Assert( pTarget );
	InputContext_t *pContext = GetInputContext( m_hContext );
	// Scripts are only run if the current mouse focus is the same as the trap.
	// If they aren't the same, then the user moved the mouse off the target area.
	if ( code == MOUSE_LEFT )
	{
		pTarget->OnMouseUp( code, ( pTrap == pContext->_mouseFocus ) );
	}
	else if ( code == MOUSE_MIDDLE )
	{
		pTarget->OnMouseUp( code, ( pTrap == pContext->_mouseFocus ) );
	}
	else if ( code == MOUSE_RIGHT )
	{
		pTarget->OnMouseUp( code, ( pTrap == pContext->_mouseFocus ) );
	}	
}

void CInputGameUI::OnMouseDoubleClick( CHitArea * const & pTarget, const ButtonCode_t &code )
{
	Assert( pTarget );
	pTarget->OnMouseDoubleClick( code );
}

void CInputGameUI::OnMouseWheel( CHitArea * const & pTarget, const int &delta )
{
	Assert( pTarget );
	pTarget->OnMouseWheel( delta );
}

void CInputGameUI::OnKeyDown( CHitArea * const & pTarget, const ButtonCode_t &code )
{
	Assert( pTarget );
	pTarget->OnKeyDown( code );
}

void CInputGameUI::OnKeyUp( CHitArea * const & pTarget, const ButtonCode_t &code )
{
	Assert( pTarget );
	pTarget->OnKeyUp( code );	
}

void CInputGameUI::OnKeyCodeTyped( CHitArea * const & pTarget, const ButtonCode_t &code )
{
	Assert( pTarget );
	pTarget->OnKeyCodeTyped( code );
}

void CInputGameUI::OnKeyTyped( CHitArea * const & pTarget, const wchar_t &unichar )
{
	Assert( pTarget );
	pTarget->OnKeyTyped( unichar );	
}

void CInputGameUI::OnLoseKeyFocus( CHitArea * const & pTarget )
{
	Assert( pTarget );
	pTarget->OnLoseKeyFocus();	
}

void CInputGameUI::OnGainKeyFocus( CHitArea * const & pTarget )
{
	Assert( pTarget );
	pTarget->OnGainKeyFocus();	
}