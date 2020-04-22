//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Interface for dealing with vgui focus issues across all plugins
//
// $NoKeywords: $
//===========================================================================//

#ifndef IMAYAVGUI_H
#define IMAYAVGUI_H

#ifdef _WIN32
#pragma once
#endif

#include <windows.h>
#include "tier0/platform.h"
#include "appframework/iappsystem.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
	class EditablePanel;
}


//-----------------------------------------------------------------------------
// Factory for creating vgui windows
//-----------------------------------------------------------------------------
abstract_class IMayaVguiWindowFactory
{
public:
	virtual void CreateVguiWindow( HWND in_hParent, const char *pPanelName ) = 0; 
	virtual void DestroyVguiWindow( const char *pPanelName ) = 0; 
};


//-----------------------------------------------------------------------------
// Interface for dealing with vgui focus issues across all plugins
//-----------------------------------------------------------------------------
#define MAYA_VGUI_INTERFACE_VERSION "VMayaVGui001"
abstract_class IMayaVGui : public IAppSystem
{
public:
	virtual void InstallVguiWindowFactory( const char *pWindowTypeName, IMayaVguiWindowFactory *pFactory ) = 0;
	virtual void RemoveVguiWindowFactory( const char *pWindowTypeName, IMayaVguiWindowFactory *pFactory ) = 0;
	virtual void SetFocus( void *hWnd, int hVGuiContext ) = 0;
	virtual bool HasFocus( void *hWnd ) = 0;
};

// hacked factory
IMayaVGui*	CoCreateIMayaVGuiInstance();

#endif // IMAYAVGUI_H
