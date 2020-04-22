//===== Copyright © 1996-2010, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "inputsystem/iinputstacksystem.h"
#include "tier2/tier2.h"
#include "tier1/utlstack.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// An input context
//-----------------------------------------------------------------------------
struct InputContext_t 
{
	InputCursorHandle_t m_hCursorIcon;
	bool m_bEnabled;
	bool m_bCursorVisible;
	bool m_bMouseCaptureEnabled;
};


//-----------------------------------------------------------------------------
// Stack system implementation
//-----------------------------------------------------------------------------
class CInputStackSystem : public CTier2AppSystem< IInputStackSystem >
{
	typedef CTier2AppSystem< IInputStackSystem > BaseClass;

	// Methods of IAppSystem
public:
	virtual const AppSystemInfo_t* GetDependencies();
	virtual void Shutdown();

	// Methods of IInputStackSystem
public:
	virtual InputContextHandle_t PushInputContext();
	virtual void PopInputContext( );
	virtual void EnableInputContext( InputContextHandle_t hContext, bool bEnable );
	virtual void SetCursorVisible( InputContextHandle_t hContext, bool bVisible );
	virtual void SetCursorIcon( InputContextHandle_t hContext, InputCursorHandle_t hCursor );
	virtual void SetMouseCapture( InputContextHandle_t hContext, bool bEnable );
	virtual void SetCursorPosition( InputContextHandle_t hContext, int x, int y );
	virtual bool IsTopmostEnabledContext( InputContextHandle_t hContext ) const;

private:
	// Updates the cursor based on the current state of the input stack
	void UpdateCursorState();

	CUtlStack< InputContext_t * > m_ContextStack;
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CInputStackSystem s_InputStackSystem;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CInputStackSystem, IInputStackSystem, 
						INPUTSTACKSYSTEM_INTERFACE_VERSION, s_InputStackSystem );


//-----------------------------------------------------------------------------
// Get dependencies
//-----------------------------------------------------------------------------
static AppSystemInfo_t s_Dependencies[] =
{
	{ "inputsystem" DLL_EXT_STRING, INPUTSYSTEM_INTERFACE_VERSION },
	{ NULL, NULL }
};

const AppSystemInfo_t* CInputStackSystem::GetDependencies()
{
	return s_Dependencies;
}


//-----------------------------------------------------------------------------
// Shutdown
//-----------------------------------------------------------------------------
void CInputStackSystem::Shutdown()
{
	// Delete any leaked contexts
	while( m_ContextStack.Count() )
	{
		InputContext_t *pContext = NULL;
		m_ContextStack.Pop( pContext );
		delete pContext;
	}

	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Allocates an input context, pushing it on top of the input stack, 
// thereby giving it top priority
//-----------------------------------------------------------------------------
InputContextHandle_t CInputStackSystem::PushInputContext()
{
	InputContext_t *pContext = new InputContext_t;
	pContext->m_bEnabled = true;
	pContext->m_bCursorVisible = true;
	pContext->m_bMouseCaptureEnabled = false;
	pContext->m_hCursorIcon = g_pInputSystem->GetStandardCursor( INPUT_CURSOR_ARROW );
	m_ContextStack.Push( pContext );

	UpdateCursorState();

	return (InputContextHandle_t)pContext;
}


//-----------------------------------------------------------------------------
// Pops the top input context off the input stack, and destroys it.
//-----------------------------------------------------------------------------
void CInputStackSystem::PopInputContext( )
{
	if ( m_ContextStack.Count() == 0 )
		return;

	InputContext_t *pContext = NULL;
	m_ContextStack.Pop( pContext );
	delete pContext;

	UpdateCursorState();
}


//-----------------------------------------------------------------------------
// Enables/disables an input context, allowing something lower on the
// stack to have control of input. Disabling an input context which
// owns mouse capture
//-----------------------------------------------------------------------------
void CInputStackSystem::EnableInputContext( InputContextHandle_t hContext, bool bEnable )
{
	InputContext_t *pContext = ( InputContext_t* )hContext;
	if ( !pContext )
		return;

	if ( pContext->m_bEnabled == bEnable )
		return;

	// Disabling an input context will deactivate mouse capture, if it's active
	if ( !bEnable )
	{
		SetMouseCapture( hContext, false );
	}

	pContext->m_bEnabled = bEnable;

	// Updates the cursor state since the stack changed
	UpdateCursorState();
}


//-----------------------------------------------------------------------------
// Allows a context to make the cursor visible;
// the topmost enabled context wins
//-----------------------------------------------------------------------------
void CInputStackSystem::SetCursorVisible( InputContextHandle_t hContext, bool bVisible )
{
	InputContext_t *pContext = ( InputContext_t* )hContext;
	if ( !pContext )
		return;

	if ( pContext->m_bCursorVisible == bVisible )
		return;

	pContext->m_bCursorVisible = bVisible;

	// Updates the cursor state since the stack changed
	UpdateCursorState();
}


//-----------------------------------------------------------------------------
// Allows a context to set the cursor icon;
// the topmost enabled context wins
//-----------------------------------------------------------------------------
void CInputStackSystem::SetCursorIcon( InputContextHandle_t hContext, InputCursorHandle_t hCursor )
{
	InputContext_t *pContext = ( InputContext_t* )hContext;
	if ( !pContext )
		return;

	if ( pContext->m_hCursorIcon == hCursor )
		return;

	pContext->m_hCursorIcon = hCursor;

	// Updates the cursor state since the stack changed
	UpdateCursorState();
}


//-----------------------------------------------------------------------------
// Allows a context to enable mouse capture. Disabling an input context
// deactivates mouse capture. Capture will occur if it happens on the
// topmost enabled context
//-----------------------------------------------------------------------------
void CInputStackSystem::SetMouseCapture( InputContextHandle_t hContext, bool bEnable )
{
	InputContext_t *pContext = ( InputContext_t* )hContext;
	if ( !pContext )
		return;

	if ( pContext->m_bMouseCaptureEnabled == bEnable )
		return;

	pContext->m_bMouseCaptureEnabled = bEnable;

	// Updates the cursor state since the stack changed
	UpdateCursorState();
}


//-----------------------------------------------------------------------------
// Allows a context to set the mouse position. It only has any effect if the
// specified context is the topmost enabled context
//-----------------------------------------------------------------------------
void CInputStackSystem::SetCursorPosition( InputContextHandle_t hContext, int x, int y )
{
	if ( IsTopmostEnabledContext( hContext ) )
	{
		g_pInputSystem->SetCursorPosition( x, y );
	}
}


//-----------------------------------------------------------------------------
// This this context the topmost enabled context?
//-----------------------------------------------------------------------------
bool CInputStackSystem::IsTopmostEnabledContext( InputContextHandle_t hContext ) const
{
	InputContext_t *pContext = ( InputContext_t* )hContext;
	if ( !pContext )
		return false;

	int nCount = m_ContextStack.Count();
	for ( int i = nCount; --i >= 0; )
	{
		InputContext_t *pStackContext = m_ContextStack[i];
		if ( !pStackContext->m_bEnabled )
			continue;

		return ( pStackContext == pContext );
	}
	return false;
}


//-----------------------------------------------------------------------------
// Updates the cursor based on the current state of the input stack
//-----------------------------------------------------------------------------
void CInputStackSystem::UpdateCursorState()
{
	int nCount = m_ContextStack.Count();
	for ( int i = nCount; --i >= 0; )
	{
		InputContext_t *pContext = m_ContextStack[i];
		if ( !pContext->m_bEnabled )
			continue;

		if ( !pContext->m_bCursorVisible )
		{
			g_pInputSystem->SetCursorIcon( INPUT_CURSOR_HANDLE_INVALID );
		}
		else
		{
			g_pInputSystem->SetCursorIcon( pContext->m_hCursorIcon );
		}

		if ( pContext->m_bMouseCaptureEnabled )
		{
			g_pInputSystem->EnableMouseCapture( g_pInputSystem->GetAttachedWindow() );
		}
		else
		{
			g_pInputSystem->DisableMouseCapture( );
		}
		break;
	}
}
