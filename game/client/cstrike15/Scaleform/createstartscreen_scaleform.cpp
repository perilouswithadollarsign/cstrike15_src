//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: [jason] The "Press Start" Screen in Scaleform.
//
//=============================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include "basepanel.h"
#include "createstartscreen_scaleform.h"
#include "../engine/filesystem_engine.h"

#include "vgui/IInput.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"


#ifdef _X360
#include "xbox/xbox_launch.h"
#endif
#include "keyvalues.h"
#include "engineinterface.h"
#include "modinfo.h"
#include "gameui_interface.h"

#include "tier1/utlbuffer.h"
#include "filesystem.h"

using namespace vgui;

// for SRC
#include <vstdlib/random.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

CCreateStartScreenScaleform* CCreateStartScreenScaleform::m_pInstance = NULL;

SFUI_BEGIN_GAME_API_DEF
SFUI_END_GAME_API_DEF( CCreateStartScreenScaleform, StartScreen )
;

void CCreateStartScreenScaleform::LoadDialog( void )
{
	if ( !m_pInstance )
	{
		m_pInstance = new CCreateStartScreenScaleform( );
		SFUI_REQUEST_ELEMENT( SF_FULL_SCREEN_SLOT, g_pScaleformUI, CCreateStartScreenScaleform, m_pInstance, StartScreen );
	}

}

void CCreateStartScreenScaleform::UnloadDialog( void )
{
	if ( m_pInstance )
	{
		m_pInstance->RemoveFlashElement();
	}
}

CCreateStartScreenScaleform::CCreateStartScreenScaleform( void )
{
	m_bLoadedAndReady = false;
	// Setup subscription so we are notified when the user signs in
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );
	g_pScaleformUI->DenyInputToGame( true );
}

void CCreateStartScreenScaleform::FlashLoaded( void )
{
	// $TODO: Call into any necessary Initializers on the action script side
}

void CCreateStartScreenScaleform::FlashReady( void )
{
	GameUI().SetBackgroundMusicDesired( true );
	m_bLoadedAndReady = true;
}

void CCreateStartScreenScaleform::PostUnloadFlash( void )
{
	// this is called when the dialog has finished animating, and
	// is ready for use to execute the server command.

	// it gets called no matter how the dialog was dismissed.

	// Remember to unsubscribe so we don't crash later!
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
	g_pScaleformUI->DenyInputToGame( false );

	m_pInstance = NULL;
	delete this;
}

bool CCreateStartScreenScaleform::ShowStartLogo_Internal( void )
{
	if ( m_bLoadedAndReady )
	{
		WITH_SLOT_LOCKED
		{
			ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "ShowStartLogo", 0, NULL );
		}
	}
	return m_bLoadedAndReady;
}

void CCreateStartScreenScaleform::OnEvent( KeyValues *pEvent )
{
	char const *szName = pEvent->GetName();

	// Notify that sign-in has completed
	if ( !Q_stricmp( szName, "OnSysSigninChange" ) 	&&
		 !Q_stricmp( "signin", pEvent->GetString( "action", "" ) ) )
	{
		int userID = pEvent->GetInt( "user0", -1 );

		BasePanel()->NotifySignInCompleted( userID );
	}
	else 
	if ( !Q_stricmp( szName, "OnSysXUIEvent" ) 	&&
		 !Q_stricmp( "closed", pEvent->GetString( "action", "" ) ) )
	{
		BasePanel()->NotifySignInCancelled();
	}
}

#endif // INCLUDE_SCALEFORM
