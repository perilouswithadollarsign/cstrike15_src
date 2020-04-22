//=========== (C) Copyright Valve, L.L.C. All rights reserved. ===========

#include "cbase.h"
#include "ui_nugget.h"

#include "vstdlib/iprocessutils.h"
#include "../uigamedata.h"

class CUiNuggetApplication : public CUiNuggetBase
{
	DECLARE_NUGGET_FN_MAP( CUiNuggetApplication, CUiNuggetBase );

	NUGGET_FN( Quit )
	{
		// We need to quit the app
		if ( IsPC() )
		{
			engine->ClientCmd( "quit" );
		}

		// X360 can quit in demo mode
		if ( IsGameConsole() )
		{
			engine->ExecuteClientCmd( "demo_exit" );
		}

		return NULL;
	}

	NUGGET_FN( ResumeGame )
	{
		engine->ClientCmd("gameui_hide");
		return NULL;
	}

	NUGGET_FN( ExitToMainMenu )
	{
		engine->ExecuteClientCmd( "gameui_hide" );

		if ( IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession() )
		{
			// Closing an active session results in disconnecting from the game.
			g_pMatchFramework->CloseSession();
		}
		else
		{
			// On PC people can be playing via console bypassing matchmaking
			// and required session settings, so to leave game duplicate
			// session closure with an extra "disconnect" command.
			engine->ExecuteClientCmd( "disconnect" );
		}

		engine->ExecuteClientCmd( "gameui_activate" );
		return NULL;
	}

	NUGGET_FN( SteamOverlayCommand )
	{
#ifndef _GAMECONSOLE
		BaseModUI::CUIGameData::Get()->ExecuteOverlayCommand( args->GetString( "command" ) );
#endif
		return NULL;
	}

	NUGGET_FN( LaunchExternalApp )
	{
		int nExitCode = g_pProcessUtils ? g_pProcessUtils->SimpleRunProcess( args->GetString( "command" ) ) : -1;
		return new KeyValues( "", "code", nExitCode );
	}
};

UI_NUGGET_FACTORY_SINGLETON( CUiNuggetApplication, "app" );
