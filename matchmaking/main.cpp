//====== Copyright © 1996-2007, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifndef POSIX
#include <conio.h>
#include <direct.h>
#include <io.h>
#endif

#include "mm_framework.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// GCSDK uses the console log channel
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_CONSOLE, "Console" );

void LinkMatchmakingLib()
{
	// This function is required for the linker to include CMatchFramework
}

static CMatchFramework g_MatchFramework;
CMatchFramework *g_pMMF = &g_MatchFramework;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CMatchFramework, IMatchFramework,
								   IMATCHFRAMEWORK_VERSION_STRING, g_MatchFramework );

//
// IAppSystem implementation
//

static CreateInterfaceFn s_pfnDelegateFactory;
static void * InternalFactory( const char *pName, int *pReturnCode )
{
	if ( pReturnCode )
	{
		*pReturnCode = IFACE_OK;
	}

	// Try to get interface via delegate
	if ( void *pInterface = s_pfnDelegateFactory ? s_pfnDelegateFactory( pName, pReturnCode ) : NULL )
	{
		return pInterface;
	}

	// Try to get internal interface
	if ( void *pInterface = Sys_GetFactoryThis()( pName, pReturnCode ) )
	{
		return pInterface;
	}

	// Failed
	if ( pReturnCode )
	{
		*pReturnCode = IFACE_FAILED;
	}
	return NULL;	
}

namespace
{

typedef void * (CMatchExtensions::* ExtFn_t)();

struct MatchExtInterface_t
{
	char const *m_szName;
	ExtFn_t m_pfnGetInterface;
	bool m_bConnected;
};

static MatchExtInterface_t s_table[] =
{
	{ LOCALIZE_INTERFACE_VERSION,		(ExtFn_t) &CMatchExtensions::GetILocalize,			false },
	{ INETSUPPORT_VERSION_STRING,		(ExtFn_t) &CMatchExtensions::GetINetSupport,		false },
	{ IENGINEVOICE_INTERFACE_VERSION,	(ExtFn_t) &CMatchExtensions::GetIEngineVoice,		false },
	{ VENGINE_CLIENT_INTERFACE_VERSION,	(ExtFn_t) &CMatchExtensions::GetIVEngineClient,		false },
	{ INTERFACEVERSION_VENGINESERVER,	(ExtFn_t) &CMatchExtensions::GetIVEngineServer,		false },
	{ INTERFACEVERSION_GAMEEVENTSMANAGER2, (ExtFn_t) &CMatchExtensions::GetIGameEventManager2, false },
#ifdef _X360
	{ XBOXSYSTEM_INTERFACE_VERSION,		(ExtFn_t) &CMatchExtensions::GetIXboxSystem,		false },
	{ XONLINE_INTERFACE_VERSION,		(ExtFn_t) &CMatchExtensions::GetIXOnline,			false },
#endif
	{ NULL, NULL, NULL }
};

};

bool CMatchFramework::Connect( CreateInterfaceFn factory )
{
	Assert( !s_pfnDelegateFactory );

	s_pfnDelegateFactory = factory;

	CreateInterfaceFn ourFactory = InternalFactory;
	ConnectTier1Libraries( &ourFactory, 1 );
	ConnectTier2Libraries( &ourFactory, 1 );
	ConVar_Register();

	// Get our extension interfaces
	for ( MatchExtInterface_t *ptr = s_table; ptr->m_szName; ++ ptr )
	{
		if ( !ptr->m_bConnected
			 // && !(g_pMatchExtensions->*(ptr->m_pfnGetInterface))()
			 )
		{
			void *pvInterface = ourFactory( ptr->m_szName, NULL );
			if ( pvInterface )
			{
				g_pMatchExtensions->RegisterExtensionInterface( ptr->m_szName, pvInterface );
				ptr->m_bConnected = true;
			}
		}
	}

	s_pfnDelegateFactory = NULL;

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );
	
	SteamApiContext_Init();

#if !defined( _GAMECONSOLE ) && !defined( SWDS )
	// Trigger intialization from Steam users
	if ( g_pPlayerManager )
		g_pPlayerManager->OnGameUsersChanged();
#endif
	
	return true;
}

void CMatchFramework::Disconnect()
{
	SteamApiContext_Shutdown();

	for ( MatchExtInterface_t *ptr = s_table; ptr->m_szName; ++ ptr )
	{
		if ( ptr->m_bConnected )
		{
			void *pvInterface = (g_pMatchExtensions->*(ptr->m_pfnGetInterface))();
			Assert( pvInterface );
			g_pMatchExtensions->UnregisterExtensionInterface( ptr->m_szName, pvInterface );
			ptr->m_bConnected = false;
		}
	}

	DisconnectTier2Libraries();
	ConVar_Unregister();
	DisconnectTier1Libraries();
}

void * CMatchFramework::QueryInterface( const char *pInterfaceName )
{
	if ( !Q_stricmp( pInterfaceName, IMATCHFRAMEWORK_VERSION_STRING ) )
		return static_cast< IMatchFramework* >( this );

	return NULL;
}

const char *COM_GetModDirectory()
{
	static char modDir[MAX_PATH];
	if ( Q_strlen( modDir ) == 0 )
	{
		const char *gamedir = CommandLine()->ParmValue("-game", CommandLine()->ParmValue( "-defaultgamedir", "hl2" ) );
		Q_strncpy( modDir, gamedir, sizeof(modDir) );
		if ( strchr( modDir, '/' ) || strchr( modDir, '\\' ) )
		{
			Q_StripLastDir( modDir, sizeof(modDir) );
			int dirlen = Q_strlen( modDir );
			Q_strncpy( modDir, gamedir + dirlen, sizeof(modDir) - dirlen );
		}
	}

	return modDir;
}

bool IsLocalClientConnectedToServer()
{
	return
		( g_pMatchExtensions &&
		  g_pMatchExtensions->GetIVEngineClient() &&
		  ( g_pMatchExtensions->GetIVEngineClient()->IsConnected() ||
		    g_pMatchExtensions->GetIVEngineClient()->IsDrawingLoadingImage() ||
			g_pMatchExtensions->GetIVEngineClient()->IsTransitioningToLoad() ) );
}
