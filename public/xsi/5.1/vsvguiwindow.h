//===== Copyright © 1996-2006, Valve Corporation, All rights reserved. ======//
//
// Base class for windows that draw vgui in Maya
//
//===========================================================================//

#ifndef VSVGUIWINDOW_H
#define VSVGUIWINDOW_H

#ifdef _WIN32
#pragma once
#endif


#include "imayavgui.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IMayaVGui;


//-----------------------------------------------------------------------------
// The singleton is defined here twice just so we don't have to include valvemaya.h also
//-----------------------------------------------------------------------------
extern IMayaVGui *g_pMayaVGui;


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
	class EditablePanel;
}


//-----------------------------------------------------------------------------
// Creates, destroys a maya vgui window
//-----------------------------------------------------------------------------
void CreateMayaVGuiWindow( HWND in_hParent, vgui::EditablePanel *pRootPanel, const char *pPanelName );
void DestroyMayaVGuiWindow( const char *pPanelName );


//-----------------------------------------------------------------------------
// Factory used to install vgui windows easily
//-----------------------------------------------------------------------------
class CVsVguiWindowFactoryBase : public IMayaVguiWindowFactory
{
public:
	CVsVguiWindowFactoryBase( const char *pWindowTypeName );

	// Registers/deregisters all vgui windows
	static void RegisterAllVguiWindows( );
	static void UnregisterAllVguiWindows( );

protected:
	const char *m_pWindowTypeName;

private:
	CVsVguiWindowFactoryBase *m_pNext;
	static CVsVguiWindowFactoryBase *s_pFirstCommandFactory;
};


template< class T >
class CVsVguiWindowFactory : public CVsVguiWindowFactoryBase
{
	typedef CVsVguiWindowFactoryBase BaseClass;

public:
	CVsVguiWindowFactory( const char *pWindowTypeName ) : BaseClass( pWindowTypeName )
	{
	}

	virtual void CreateVguiWindow(HWND in_hParent,  const char *pPanelName )
	{
		T *pVguiPanel = new T;
		CreateMayaVGuiWindow( in_hParent, pVguiPanel, pPanelName );
	}

	virtual void DestroyVguiWindow( const char *pPanelName )
	{
		DestroyMayaVGuiWindow( pPanelName );
	}

private:
};


#define INSTALL_MAYA_VGUI_WINDOW( _className, _windowTypeName )	\
	static CVsVguiWindowFactory< _className > s_VsVguiWindowFactory##_className##( _windowTypeName )


#endif // VSVGUIWINDOW_H
