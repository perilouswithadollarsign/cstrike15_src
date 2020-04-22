//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: This is input priority system, allowing various clients to
// cause input messages / cursor control to be routed to them as opposed to
// other clients.
//
//===========================================================================//

#ifndef IINPUTCLIENTSTACK_H
#define IINPUTCLIENTSTACK_H
#ifdef _WIN32
#pragma once
#endif

#include "appframework/iappsystem.h"
#include "inputsystem/iinputsystem.h"


///-----------------------------------------------------------------------------
/// A handle to an input context. These are arranged in a priority-based
/// stack; the top context on the stack which is also enabled wins.
///-----------------------------------------------------------------------------
DECLARE_POINTER_HANDLE( InputContextHandle_t );
#define INPUT_CONTEXT_HANDLE_INVALID ( (InputContextHandle_t)0 )


///-----------------------------------------------------------------------------
/// Purpose: This is input priority system, allowing various clients to
/// cause input messages / cursor control to be routed to them as opposed to
/// other clients.
///
/// NOTE: For Source1, it would be a huge change to move all input (like 
/// the code in engine/keys.cpp for example) to go through this interface. 
/// Therefore, I'm going to stick with only dealing with cursor control, 
/// which is necessary for Jen's new gameUI system to interoperate with VGui.
///-----------------------------------------------------------------------------
abstract_class IInputStackSystem : public IAppSystem
{
public:
	/// Allocates an input context, pushing it on top of the input stack, 
	/// thereby giving it top priority
	virtual InputContextHandle_t PushInputContext() = 0;

	/// Pops the top input context off the input stack, and destroys it.
	virtual void PopInputContext( ) = 0;

	/// Enables/disables an input context, allowing something lower on the
	/// stack to have control of input. Disabling an input context which
	/// owns mouse capture
	virtual void EnableInputContext( InputContextHandle_t hContext, bool bEnable ) = 0;

	/// Allows a context to make the cursor visible;
	/// the topmost enabled context wins
	virtual void SetCursorVisible( InputContextHandle_t hContext, bool bVisible ) = 0;

	/// Allows a context to set the cursor icon;
	/// the topmost enabled context wins
	virtual void SetCursorIcon( InputContextHandle_t hContext, InputCursorHandle_t hCursor ) = 0;

	/// Allows a context to enable mouse capture. Disabling an input context
	/// deactivates mouse capture. Capture will occur if it happens on the
	/// topmost enabled context
	virtual void SetMouseCapture( InputContextHandle_t hContext, bool bEnable ) = 0;

	/// Allows a context to set the mouse position. It only has any effect if the
	/// specified context is the topmost enabled context
	virtual void SetCursorPosition( InputContextHandle_t hContext, int x, int y ) = 0;

	/// Returns true if the specified context is the topmost enabled context
	virtual bool IsTopmostEnabledContext( InputContextHandle_t hContext ) const = 0;
};

DECLARE_TIER2_INTERFACE( IInputStackSystem, g_pInputStackSystem );


#endif // IINPUTCLIENTSTACK_H
