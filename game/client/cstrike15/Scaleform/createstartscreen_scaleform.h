//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: [jason] Creates the Start Screen in Scaleform.
//
// $NoKeywords: $
//=============================================================================//
#if defined( INCLUDE_SCALEFORM )

#ifndef CREATESTARTSCREEN_SCALEFORM_H
#define CREATESTARTSCREEN_SCALEFORM_H
#ifdef _WIN32
#pragma once
#endif

#include "matchmaking/imatchframework.h"
#include "scaleformui/scaleformui.h"

class CCreateStartScreenScaleform : public ScaleformFlashInterface, public IMatchEventsSink
{
protected:
	static CCreateStartScreenScaleform* m_pInstance;

	CCreateStartScreenScaleform( );

public:
	static void LoadDialog( void );
	static void UnloadDialog( void );
	static bool IsActive() { return m_pInstance != NULL; }
	static bool ShowStartLogo( void )
	{
		if ( m_pInstance )
			return m_pInstance->ShowStartLogo_Internal();
		return false;
	}

protected:
	virtual void FlashReady( void );
	virtual void PostUnloadFlash( void );
	virtual void FlashLoaded( void );

	void Show( void );
	void Hide( void );
	bool ShowStartLogo_Internal( void );

	virtual void OnEvent( KeyValues *pEvent );

private:
	bool m_bLoadedAndReady;
};

#endif // CREATESTARTSCREEN_SCALEFORM_H
#endif // include scaleform
