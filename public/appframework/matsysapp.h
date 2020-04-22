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

#ifndef MATSYSAPP_H
#define MATSYSAPP_H

#ifdef _WIN32
#pragma once
#endif


#include "appframework/tier2app.h"


//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CMatSysApp : public CTier2SteamApp
{
	typedef CTier2SteamApp BaseClass;

public:
	CMatSysApp();

	// Methods of IApplication
	virtual bool Create();
	virtual bool PreInit();
	virtual void PostShutdown();
	virtual void Destroy();

	// Returns the window handle (HWND in Win32)
	void* GetAppWindow();

	// Gets the window size
	int GetWindowWidth() const;
	int GetWindowHeight() const;

protected:
	void AppPumpMessages();

	// Sets the video mode
	bool SetVideoMode( );

	// Sets up the game path
	bool SetupSearchPaths( const char *pStartingDir, bool bOnlyUseStartingDir, bool bIsTool );

private:
	// Returns the app name
	virtual const char *GetAppName() = 0;
	virtual bool AppUsesReadPixels() { return false; }

	void *m_HWnd;
	int m_nWidth;
	int m_nHeight;
};


#endif // MATSYSAPP_H
