//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
//=============================================================================
#ifdef _WIN32

#include "appframework/vguimaterialsystem2app.h"
#include "vgui/IVGui.h"
#include "vgui/ISurface.h"
#include "vgui_controls/controls.h"
#include "vgui/IScheme.h"
#include "vgui/ILocalize.h"
#include "tier0/dbg.h"
#include "vguirendersurface/ivguirendersurface.h"
#include "tier3/tier3.h"
#include "interfaces/interfaces.h"
#include "inputsystem/iinputstacksystem.h"


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CVGuiMaterialSystem2App::CVGuiMaterialSystem2App()
{
	m_hAppInputContext = INPUT_CONTEXT_HANDLE_INVALID;
}


//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CVGuiMaterialSystem2App::Create()
{
	if ( !BaseClass::Create() )
		return false;

	AppSystemInfo_t appSystems[] = 
	{
		{ "inputsystem.dll",		INPUTSTACKSYSTEM_INTERFACE_VERSION },
		// NOTE: This has to occur before vgui2.dll so it replaces vgui2's surface implementation
		{ "vguirendersurface.dll",	VGUI_SURFACE_INTERFACE_VERSION },
		{ "vgui2.dll",				VGUI_IVGUI_INTERFACE_VERSION },

		// Required to terminate the list
		{ "", "" }
	};

	return AddSystems( appSystems );
}

void CVGuiMaterialSystem2App::Destroy()
{
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CVGuiMaterialSystem2App::PreInit( )
{
	if ( !BaseClass::PreInit() )
		return false;

	CreateInterfaceFn factory = GetFactory();
	ConnectTier3Libraries( &factory, 1 );
	if ( !vgui::VGui_InitInterfacesList( "CVGuiMaterialSystem2App", &factory, 1 ) )
		return false;

	if ( !g_pVGuiRenderSurface )
	{
		Warning( "CVGuiMaterialSystem2App::PreInit: Unable to connect to necessary interface!\n" );
		return false;
	}

	return true; 
}

bool CVGuiMaterialSystem2App::PostInit()
{
	if ( !BaseClass::PostInit() )
		return false;

	m_hAppInputContext = g_pInputStackSystem->PushInputContext();
	InputContextHandle_t hVGuiInputContext = g_pInputStackSystem->PushInputContext();
	g_pVGuiRenderSurface->SetInputContext( hVGuiInputContext );
	g_pVGuiRenderSurface->EnableWindowsMessages( true );
	return true; 
}

void CVGuiMaterialSystem2App::PreShutdown()
{
	g_pVGuiRenderSurface->EnableWindowsMessages( false );
	g_pVGuiRenderSurface->SetInputContext( NULL );
	if ( m_hAppInputContext != INPUT_CONTEXT_HANDLE_INVALID )
	{
		g_pInputStackSystem->PopInputContext();	// Vgui
		g_pInputStackSystem->PopInputContext();	// App
	}
	
	BaseClass::PreShutdown();
}

void CVGuiMaterialSystem2App::PostShutdown()
{
	DisconnectTier3Libraries();
	BaseClass::PostShutdown();
}

InputContextHandle_t CVGuiMaterialSystem2App::GetAppInputContext()
{
	return m_hAppInputContext;
}

	
#endif // _WIN32

