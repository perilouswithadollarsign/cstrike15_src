//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// Used for material system apps
//=============================================================================

#ifndef MATERIALSYSTEM2APP_H
#define MATERIALSYSTEM2APP_H

#ifdef _WIN32
#pragma once
#endif


#include "appframework/tier2app.h"
#include "tier0/platwindow.h"
#include "rendersystem/irenderdevice.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
FORWARD_DECLARE_HANDLE( SwapChainHandle_t );


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CMaterialSystem2App : public CTier2SteamApp, public IRenderDeviceSetup
{
	typedef CTier2SteamApp BaseClass;

public:
	enum RenderSystemDLL_t
	{
		RENDER_SYSTEM_DX9 = 0,
		RENDER_SYSTEM_DX11,
		RENDER_SYSTEM_GL,
		RENDER_SYSTEM_X360,
	};

public:
	CMaterialSystem2App();

	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit();
	virtual bool PostInit();
	virtual void PreShutdown();
	virtual void Destroy();

	// Returns the window handle 
	PlatWindow_t GetAppWindow();

	// Gets the render system we're running on
	RenderSystemDLL_t GetRenderSystem() const;

	// Returns the number of threads our thread pool is using
	int GetThreadCount() const;

	// Creates a 3D-capable window
	SwapChainHandle_t Create3DWindow( const char *pTitle, int nWidth, int nHeight, bool bResizing, bool bFullscreen, bool bAcceptsInput );

protected:
	void AppPumpMessages();

	// Sets up the game path
	bool SetupSearchPaths( const char *pStartingDir, bool bOnlyUseStartingDir, bool bIsTool );

private:
	// Inherited from IRenderDeviceSetup
	virtual bool CreateRenderDevice();

private:
	// Returns the app name
	virtual const char *GetAppName() = 0;
	virtual bool IsConsoleApp() { return false; }

	bool AddRenderSystem();
	void ApplyModSettings( );
	bool CreateMainWindow( bool bResizing );
	bool CreateMainConsoleWindow();

	SwapChainHandle_t m_hSwapChain;
	int m_nThreadCount;
	RenderSystemDLL_t m_nRenderSystem;
	CreateInterfaceFn m_RenderFactory;
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Gets the render system we're running on
//-----------------------------------------------------------------------------
inline CMaterialSystem2App::RenderSystemDLL_t CMaterialSystem2App::GetRenderSystem() const
{
	return m_nRenderSystem;
}


//-----------------------------------------------------------------------------
// Returns the number of threads our thread pool is using
//-----------------------------------------------------------------------------
inline int CMaterialSystem2App::GetThreadCount() const
{
	return m_nThreadCount;
}


#endif // MATERIALSYSTEM2APP_H
