//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: An application framework 
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#ifndef APPFRAMEWORK_H
#define APPFRAMEWORK_H

#ifdef _WIN32
#pragma once
#endif

#include "appframework/IAppSystemGroup.h"
#include "ilaunchabledll.h"

//-----------------------------------------------------------------------------
// Gets the application instance..
//-----------------------------------------------------------------------------
void *GetAppInstance();


//-----------------------------------------------------------------------------
// Sets the application instance, should only be used if you're not calling AppMain.
//-----------------------------------------------------------------------------
void SetAppInstance( void* hInstance );


//-----------------------------------------------------------------------------
// Main entry point for the application
//-----------------------------------------------------------------------------
int AppMain( void* hInstance, void* hPrevInstance, const char* lpCmdLine, int nCmdShow, CAppSystemGroup *pAppSystemGroup );
int AppMain( int argc, char **argv, CAppSystemGroup *pAppSystemGroup );


//-----------------------------------------------------------------------------
// Used to startup/shutdown the application
//-----------------------------------------------------------------------------
int AppStartup( void* hInstance, void* hPrevInstance, const char* lpCmdLine, int nCmdShow, CAppSystemGroup *pAppSystemGroup );
int AppStartup( int argc, char **argv, CAppSystemGroup *pAppSystemGroup );
void AppShutdown( CAppSystemGroup *pAppSystemGroup );


//-----------------------------------------------------------------------------
// Macros to create singleton application objects for windowed + console apps
//-----------------------------------------------------------------------------

// This assumes you've used one of the 
#define DEFINE_LAUNCHABLE_DLL_STEAM_APP() \
	class CAppLaunchableDLL : public ILaunchableDLL \
	{ \
	public: \
		virtual int	main( int argc, char **argv ) \
		{ \
			return AppMain( argc, argv, &__s_SteamApplicationObject ); \
		} \
	}; \
	static CAppLaunchableDLL __g_AppLaunchableDLL; \
	EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CAppLaunchableDLL, ILaunchableDLL, LAUNCHABLE_DLL_INTERFACE_VERSION, __g_AppLaunchableDLL ); 


#if !defined( _X360 )
#if defined( _OSX )
#define DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR( _globalVarName ) \
	int main( int argc, char **argv )										\
	{																							\
		extern int ValveCocoaMain( int argc, char **argv, CAppSystemGroup *pAppSystemGroup ); \
		return ValveCocoaMain( argc, argv, &_globalVarName ); \
	}
#elif defined( PLATFORM_LINUX )
#define DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR( _globalVarName ) \
	int main( int argc, char **argv )										\
	{																							\
		extern int ValveLinuxWindowedMain( int argc, char **argv, CAppSystemGroup *pAppSystemGroup ); \
		return ValveLinuxWindowedMain( argc, argv, &_globalVarName ); \
	}
#else
#define DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR( _globalVarName ) \
	int __stdcall WinMain( struct HINSTANCE__* hInstance, struct HINSTANCE__* hPrevInstance, NULLTERMINATED char *lpCmdLine, int nCmdShow )	\
	{																							\
		return AppMain( hInstance, hPrevInstance, lpCmdLine, nCmdShow, &_globalVarName );		\
	}
#endif
#else
#define DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR( _globalVarName )	\
	DLL_EXPORT int AppMain360( struct HINSTANCE__* hInstance, struct HINSTANCE__* hPrevInstance, NULLTERMINATED char *lpCmdLine, int nCmdShow )	\
	{																				\
		return AppMain( hInstance, hPrevInstance, lpCmdLine, nCmdShow, &_globalVarName );	\
	}
#endif

#if !defined( _X360 )
#define DEFINE_CONSOLE_APPLICATION_OBJECT_GLOBALVAR( _globalVarName ) \
	int main( int argc, char **argv )			\
	{											\
		return AppMain( argc, argv, &_globalVarName );	\
	}
#else
#define DEFINE_CONSOLE_APPLICATION_OBJECT_GLOBALVAR( _globalVarName ) \
	DLL_EXPORT int AppMain360( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )	\
	{																				\
		return AppMain( hInstance, hPrevInstance, lpCmdLine, nCmdShow, &_globalVarName );	\
	}
#endif

#define DEFINE_BINLAUNCHABLE_APPLICATION_OBJECT_GLOBALVAR( _globalVarName )	\
	class CApplicationDLL : public ILaunchableDLL								\
	{																			\
	public:																		\
		virtual int main( int argc, char **argv )								\
		{																		\
			return AppMain( argc, argv, &_globalVarName );						\
		}																		\
	};																			\
	EXPOSE_SINGLE_INTERFACE( CApplicationDLL, ILaunchableDLL, LAUNCHABLE_DLL_INTERFACE_VERSION )

#define DEFINE_WINDOWED_APPLICATION_OBJECT( _className )	\
	static _className __s_ApplicationObject;				\
	DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR( __s_ApplicationObject )

#define DEFINE_CONSOLE_APPLICATION_OBJECT( _className )	\
	static _className __s_ApplicationObject;			\
	DEFINE_CONSOLE_APPLICATION_OBJECT_GLOBALVAR( __s_ApplicationObject )

#define DEFINE_BINLAUNCHABLE_APPLICATION_OBJECT( _className )	\
	static _className __s_ApplicationObject;					\
	DEFINE_BINLAUNCHABLE_APPLICATION_OBJECT_GLOBALVAR( __s_ApplicationObject )


//-----------------------------------------------------------------------------
// This class is a helper class used for steam-based applications.
// It loads up the file system in preparation for using it to load other
// required modules from steam.
//-----------------------------------------------------------------------------
class CSteamApplication : public CAppSystemGroup
{
	typedef CAppSystemGroup BaseClass;

public:
	CSteamApplication( CSteamAppSystemGroup *pAppSystemGroup );

	// Implementation of IAppSystemGroup
	virtual bool Create( );
	virtual bool PreInit( );
	virtual int Main( );
	virtual void PostShutdown();
	virtual void Destroy();

	// Use this version in cases where you can't control the main loop and
	// expect to be ticked
	virtual int Startup();
	virtual void Shutdown();

public:
	// Here's a hook to override the filesystem DLL that it tries to load.
	// By default, it uses FileSystem_GetFileSystemDLLName to figure this out.
	virtual bool GetFileSystemDLLName( char *pOut, int nMaxBytes, bool &bIsSteam );

protected:
	IFileSystem *m_pFileSystem;
	CSteamAppSystemGroup *m_pChildAppSystemGroup;
	bool m_bSteam;
};


class CBinLaunchableSteamApp : public CSteamApplication
{
public:
	CBinLaunchableSteamApp( CSteamAppSystemGroup *pAppSystemGroup ) : CSteamApplication( pAppSystemGroup )
	{
	}

	virtual bool GetFileSystemDLLName( char *pOut, int nMaxChars, bool &bIsSteam )
	{
		// Our path should already include game\bin, so just use the filename directly
		// and don't try to figure out an absolute path to it as CSteamApplication does.
		V_strncpy( pOut, "filesystem_stdio", nMaxChars );
		bIsSteam = false;
		return true;
	}
};


//-----------------------------------------------------------------------------
// Macros to help create singleton application objects for windowed + console steam apps
//-----------------------------------------------------------------------------
#define DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT_GLOBALVAR( _className, _varName )	\
	static CSteamApplication __s_SteamApplicationObject( &_varName );	\
	DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR( __s_SteamApplicationObject )

#define DEFINE_WINDOWED_STEAM_APPLICATION_OBJECT( _className )	\
	static _className __s_ApplicationObject;				\
	static CSteamApplication __s_SteamApplicationObject( &__s_ApplicationObject );	\
	DEFINE_WINDOWED_APPLICATION_OBJECT_GLOBALVAR( __s_SteamApplicationObject )

#define DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT_GLOBALVAR( _className, _varName )	\
	static CSteamApplication __s_SteamApplicationObject( &_varName );	\
	DEFINE_CONSOLE_APPLICATION_OBJECT_GLOBALVAR( __s_SteamApplicationObject )

#define DEFINE_CONSOLE_STEAM_APPLICATION_OBJECT( _className )	\
	static _className __s_ApplicationObject;			\
	static CSteamApplication __s_SteamApplicationObject( &__s_ApplicationObject );	\
	DEFINE_CONSOLE_APPLICATION_OBJECT_GLOBALVAR( __s_SteamApplicationObject )

#define DEFINE_BINLAUNCHABLE_STEAM_APPLICATION_OBJECT_GLOBALVAR( _className, _varName )	\
	static CBinLaunchableSteamApp __s_SteamApplicationObject( &_varName );	\
	DEFINE_BINLAUNCHABLE_APPLICATION_OBJECT_GLOBALVAR( __s_SteamApplicationObject )

#define DEFINE_BINLAUNCHABLE_STEAM_APPLICATION_OBJECT( _className )	\
	static _className __s_ApplicationObject;			\
	static CBinLaunchableSteamApp __s_SteamApplicationObject( &__s_ApplicationObject );	\
	DEFINE_BINLAUNCHABLE_APPLICATION_OBJECT_GLOBALVAR( __s_SteamApplicationObject )


// This defines your steam application object and ties it to your appsystemgroup.
// This does NOT hookup its AppMain to get called. You'll have to call that from startup code
// or use something like DEFINE_LAUNCHABLE_DLL_STEAM_APP() to call it.
//
// _steamApplicationClass derives from CSteamApplication.
// _appClass derives from CAppSystemGroup (.. can derive from - or be - CTier2SteamApp for example).
// 
#define DEFINE_CUSTOM_STEAM_APPLICATION_OBJECT( _steamApplicationClassName, _appClassName ) \
	static _appClassName __s_ApplicationObject;				\
	static _steamApplicationClassName __s_SteamApplicationObject( &__s_ApplicationObject );

#endif // APPFRAMEWORK_H
