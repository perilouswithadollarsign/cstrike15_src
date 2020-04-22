//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
//=============================================================================
#ifdef _WIN32

#include "appframework/vguimatsysapp.h"
#include "vgui/IVGui.h"
#include "vgui/ISurface.h"
#include "vgui_controls/controls.h"
#include "vgui/IScheme.h"
#include "vgui/ILocalize.h"
#include "tier0/dbg.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "tier3/tier3.h"
#include "inputsystem/iinputstacksystem.h"


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CVguiMatSysApp::CVguiMatSysApp()
{
	m_hAppInputContext = INPUT_CONTEXT_HANDLE_INVALID;
}


//-----------------------------------------------------------------------------
// Create all singleton systems
//-----------------------------------------------------------------------------
bool CVguiMatSysApp::Create()
{
	if ( !BaseClass::Create() )
		return false;

	AppSystemInfo_t appSystems[] = 
	{
		{ "inputsystem.dll",		INPUTSTACKSYSTEM_INTERFACE_VERSION },
		// NOTE: This has to occur before vgui2.dll so it replaces vgui2's surface implementation
		{ "vguimatsurface.dll",		VGUI_SURFACE_INTERFACE_VERSION },
		{ "vgui2.dll",				VGUI_IVGUI_INTERFACE_VERSION },

		// Required to terminate the list
		{ "", "" }
	};

	return AddSystems( appSystems );
}

void CVguiMatSysApp::Destroy()
{
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
bool CVguiMatSysApp::PreInit( )
{
	if ( !BaseClass::PreInit() )
		return false;

	CreateInterfaceFn factory = GetFactory();
	ConnectTier3Libraries( &factory, 1 );
	if ( !vgui::VGui_InitInterfacesList( "CVguiSteamApp", &factory, 1 ) )
		return false;

	if ( !g_pMatSystemSurface )
	{
		Warning( "CVguiMatSysApp::PreInit: Unable to connect to necessary interface!\n" );
		return false;
	}

	g_pMatSystemSurface->EnableWindowsMessages( true );
	return true; 
}

bool CVguiMatSysApp::PostInit()
{
	if ( !BaseClass::PostInit() )
		return false;

	m_hAppInputContext = g_pInputStackSystem->PushInputContext();
	InputContextHandle_t hVGuiInputContext = g_pInputStackSystem->PushInputContext();
	g_pMatSystemSurface->SetInputContext( hVGuiInputContext );
	g_pMatSystemSurface->EnableWindowsMessages( true );
	return true; 
}

void CVguiMatSysApp::PreShutdown()
{
	g_pMatSystemSurface->EnableWindowsMessages( false );
	g_pMatSystemSurface->SetInputContext( NULL );
	if ( m_hAppInputContext != INPUT_CONTEXT_HANDLE_INVALID )
	{
		g_pInputStackSystem->PopInputContext();	// Vgui
		g_pInputStackSystem->PopInputContext();	// App
	}

	BaseClass::PreShutdown();
}

void CVguiMatSysApp::PostShutdown()
{
	DisconnectTier3Libraries();
	BaseClass::PostShutdown();
}

InputContextHandle_t CVguiMatSysApp::GetAppInputContext()
{
	return m_hAppInputContext;
}

#endif // _WIN32

