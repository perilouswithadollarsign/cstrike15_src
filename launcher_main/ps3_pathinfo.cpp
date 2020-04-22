//===== Copyright © 1996-2010, Valve Corporation, All rights reserved. ======//
//
// Purpose: Utility class for discovering and caching path info on the PS3.
//
// $NoKeywords: $
//===========================================================================//


#ifndef SN_TARGET_PS3
#error You're compiling this file on the wrong platform!
#endif

#include <stdlib.h>
#include <string.h>
#include <cell/sysmodule.h>
#include "../public/ps3_pathinfo.h"
#include <sys/tty.h>
#include "errorrenderloop.h"
#include <np.h>

// statically defined because not available in LauncherMain:
#ifndef DBG_H
static void LocalError( const char *fmt, ... )
{
	va_list args;
	va_start(args, fmt);
	vprintf( fmt, args );
	ErrorRenderLoop loop;
	loop.Run();
	exit(1);
}
static void AssertMsg( bool cond, const char *complaint )
{
	if (!cond)
	{
		LocalError(complaint);
	}
}
#else
#define LocalError Error
#endif

#define CheckError( x, str ) if ( x < 0 ) { LocalError( "%s: %s\n", str, GetSonyErrorString( x ) ); return x; }

#ifndef _CERT
#define DiagnosticStringMode 1
#define DiagnosticString( x ) do { unsigned int dummy; sys_tty_write( SYS_TTYP15, x, strlen( x ), &dummy ); } while(0)
#else
#define DiagnosticString( x ) ((void)0)
#endif


CPs3ContentPathInfo g_Ps3GameDataPathInfo;

CPs3ContentPathInfo::CPs3ContentPathInfo() :
	m_bInitialized(false),
	m_nHDDFreeSizeKb( 0 ),
	m_nBootType( 0 ),
	m_nBootAttribs( 0 ),
	m_gameParentalLevel( 0 ),
	m_gameResolution( 0 ),
	m_gameSoundFormat( 0 )
{
#define GAME_INIT( x ) memset( x, 0, sizeof( x ) )
	GAME_INIT( m_gameTitle );
	GAME_INIT( m_gameTitleID );
	GAME_INIT( m_gameAppVer );
	GAME_INIT( m_gamePatchAppVer );
	GAME_INIT( m_gameContentPath );
	GAME_INIT( m_gamePatchContentPath );
	GAME_INIT( m_gameBasePath );
	GAME_INIT( m_gamePatchBasePath );
	GAME_INIT( m_gameExesPath );
	GAME_INIT( m_gameHDDataPath );
	GAME_INIT( m_gameImageDataPath );
	GAME_INIT( m_gameSystemCachePath );
	GAME_INIT( m_gameSavesShadowPath );
#undef GAME_INIT
}

const char *GetSonyErrorString( int errorcode ) ; /// return a description for a CELL_GAME_ERROR code
int CPs3ContentPathInfo::Init( unsigned int uiFlagsMask )
{
	AssertMsg( !m_bInitialized, "CPs3ContentPathInfo is being initialized twice!\n" );

	/////////////////////////////////////////////////////////////////////////
	//
	// load sysutil NP
	//
	//////////////////////////////////////////////////////////////////////////

	// we'll need to haul libsysutil into memory  ( CELL_SYSMODULE_SYSUTIL_NP )
	{
		int suc = cellSysmoduleLoadModule( CELL_SYSMODULE_SYSUTIL_NP );
		if ( suc != CELL_OK )
		{
			LocalError( "Failed to load sysutil_np: %s\n", GetSonyErrorString(suc) );
			return suc;
		}
	}


	/////////////////////////////////////////////////////////////////////////
	//
	// load sysutil GAME
	//
	//////////////////////////////////////////////////////////////////////////

	// we'll need to haul libsysutil into memory  ( CELL_SYSMODULE_SYSUTIL_GAME )
	bool bSysModuleIsLoaded = cellSysmoduleIsLoaded( CELL_SYSMODULE_SYSUTIL_GAME ) == CELL_SYSMODULE_LOADED ;
	// if this assert trips, then:
	// 1) look at where the sysutil_game module is loaded to make sure it still needs to be loaded at this point (maybe you can dump it to save memory)
	// 2) if it's being taken care of somewhere else, we don't need to load the module here. 
	AssertMsg( !bSysModuleIsLoaded, "The SYSUTIL_GAME module is already loaded -- revist load order logic in CPs3ContentPathInfo::Init()\n"); 
	if ( !bSysModuleIsLoaded )
	{
		int suc = cellSysmoduleLoadModule( CELL_SYSMODULE_SYSUTIL_GAME );
		if ( suc != CELL_OK )
		{
			LocalError( "Failed to load sysutil_game: %s\n", GetSonyErrorString(suc) );
			return suc;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//
	// cellGameBootCheck
	//
	//////////////////////////////////////////////////////////////////////////

	// get the base to the content directory.
	CellGameContentSize size; // For game content of a disc boot game, sizeKB and sysSizeKB take no meaning – please do not use them
	memset(&size, 0, sizeof(CellGameContentSize));
	char bootdir[CELL_GAME_DIRNAME_SIZE] = {0};

	int success = cellGameBootCheck( &m_nBootType, &m_nBootAttribs, &size, bootdir );
	if ( success != CELL_GAME_RET_OK )
	{
		LocalError("cellGameBootCheck failed (line %d, code %d): %s\n", __LINE__, success, GetSonyErrorString(success) );
		return success;
	}

#if DiagnosticStringMode
	if ( m_nBootAttribs & CELL_GAME_ATTRIBUTE_DEBUG ) { DiagnosticString( "GAME BOOT: DEBUG MODE\n" ); }
	if ( m_nBootAttribs & CELL_GAME_ATTRIBUTE_APP_HOME ) { DiagnosticString( "GAME BOOT: HOSTFS MODE (app_home)\n" ); }
	if ( m_nBootAttribs & CELL_GAME_ATTRIBUTE_PATCH ) { DiagnosticString( "GAME BOOT: PATCH MODE\n" ); }
	if ( m_nBootAttribs & CELL_GAME_ATTRIBUTE_INVITE_MESSAGE ) { DiagnosticString( "GAME BOOT: INVITE MESSAGE\n" ); }
	if ( m_nBootAttribs & CELL_GAME_ATTRIBUTE_CUSTOM_DATA_MESSAGE ) { DiagnosticString( "GAME BOOT: CUSTOM DATA MESSAGE\n" ); }
	DiagnosticString( "BOOT DIR   " ); DiagnosticString( bootdir ); DiagnosticString( "\n" );
#endif

	m_bInitialized = true;
	m_nHDDFreeSizeKb = size.hddFreeSizeKB;

	//////////////////////////////////////////////////////////////////////////
	//
	// cellGameContentPermit
	//
	//////////////////////////////////////////////////////////////////////////

	if ( !( uiFlagsMask & INIT_RETAIL_MODE ) )
	{
		DiagnosticString( "BOOT INFO  USING NON-RETAIL BOOT\n" );
		//
		// BOOT MODE required: PARAM.SFO
		//
		success = cellGameGetParamString( CELL_GAME_PARAMID_TITLE, m_gameTitle, sizeof( m_gameTitle ) ); CheckError( success, "PARAM.SFO getParam( CELL_GAME_PARAMID_TITLE )" );
		success = cellGameGetParamString( CELL_GAME_PARAMID_TITLE_ID, m_gameTitleID, sizeof( m_gameTitleID ) ); CheckError( success, "PARAM.SFO getParam( CELL_GAME_PARAMID_TITLE_ID )" );
		success = cellGameGetParamString( CELL_GAME_PARAMID_APP_VER, m_gameAppVer, sizeof( m_gameAppVer ) ); CheckError( success, "PARAM.SFO getParam( CELL_GAME_PARAMID_APP_VER )" );

		success = cellGameGetParamInt( CELL_GAME_PARAMID_PARENTAL_LEVEL, &m_gameParentalLevel ); CheckError( success, "PARAM.SFO getParam( CELL_GAME_PARAMID_PARENTAL_LEVEL )" );
		success = cellGameGetParamInt( CELL_GAME_PARAMID_RESOLUTION, &m_gameResolution ); CheckError( success, "PARAM.SFO getParam( CELL_GAME_PARAMID_RESOLUTION )" );
		success = cellGameGetParamInt( CELL_GAME_PARAMID_SOUND_FORMAT, &m_gameSoundFormat ); CheckError( success, "PARAM.SFO getParam( CELL_GAME_PARAMID_SOUND_FORMAT )" );

		// Access /app_home/...
		success = cellGameContentPermit( m_gameContentPath, m_gameBasePath ) ; 
		DiagnosticString( "BOOT ROOT  " ); DiagnosticString( m_gameContentPath ); DiagnosticString( "\n" );
		DiagnosticString( "BOOT USR   " ); DiagnosticString( m_gameBasePath ); DiagnosticString( "\n" );
		// when running the game from the debugger, the data path returned by ContentPermit contains
		// HOSTFS formatted path like /app_home/D:\perforce\...
		// Perform the fixup to conform to disk image layout
		if ( (m_nBootAttribs & CELL_GAME_ATTRIBUTE_DEBUG) && !strncmp( m_gameBasePath, "/app_home", sizeof( "/app_home" ) - 1 ) )
		{
			snprintf( m_gameContentPath, sizeof( m_gameContentPath ) - 1, "/app_home/PS3_GAME" );
			snprintf( m_gameBasePath, sizeof( m_gameBasePath ) - 1, "/app_home/PS3_GAME/USRDIR" );
			DiagnosticString( "BOOT ROOT/ " ); DiagnosticString( m_gameContentPath ); DiagnosticString( "\n" );
			DiagnosticString( "BOOT USR// " ); DiagnosticString( m_gameBasePath ); DiagnosticString( "\n" );
		}
	}
	else
	{
		if ( m_nBootType != CELL_GAME_GAMETYPE_DISC )
		{
			LocalError("Only disk boot is supported in RETAIL mode! (bootmode=%d)\n", m_nBootType );
			return -1;
		}

		// Finish access to boot executable
		{
			char tmp_contentInfoPath[CELL_GAME_PATH_MAX] = {0};
			char tmp_usrdirPath[CELL_GAME_PATH_MAX] = {0};
			success = cellGameContentPermit( tmp_contentInfoPath, tmp_usrdirPath ); // must call this to allow mounting of BDVD
			DiagnosticString( "BOOT ROOT  " ); DiagnosticString( tmp_contentInfoPath ); DiagnosticString( "\n" );
			DiagnosticString( "BOOT USR   " ); DiagnosticString( tmp_usrdirPath ); DiagnosticString( "\n" );
		}

		// in RETAIL mode we always have our assets on BDVD
		success = cellGameDataCheck( m_nBootType, NULL, &size);
		if ( success == CELL_GAME_RET_OK )
		{
			//
			// BOOT MODE required: PARAM.SFO
			//
			success = cellGameGetParamString( CELL_GAME_PARAMID_TITLE, m_gameTitle, sizeof( m_gameTitle ) ); CheckError( success, "PARAM.SFO getParam( CELL_GAME_PARAMID_TITLE )" );
			success = cellGameGetParamString( CELL_GAME_PARAMID_TITLE_ID, m_gameTitleID, sizeof( m_gameTitleID ) ); CheckError( success, "PARAM.SFO getParam( CELL_GAME_PARAMID_TITLE_ID )" );
			success = cellGameGetParamString( CELL_GAME_PARAMID_APP_VER, m_gameAppVer, sizeof( m_gameAppVer ) ); CheckError( success, "PARAM.SFO getParam( CELL_GAME_PARAMID_APP_VER )" );

			success = cellGameGetParamInt( CELL_GAME_PARAMID_PARENTAL_LEVEL, &m_gameParentalLevel ); CheckError( success, "PARAM.SFO getParam( CELL_GAME_PARAMID_PARENTAL_LEVEL )" );
			success = cellGameGetParamInt( CELL_GAME_PARAMID_RESOLUTION, &m_gameResolution ); CheckError( success, "PARAM.SFO getParam( CELL_GAME_PARAMID_RESOLUTION )" );
			success = cellGameGetParamInt( CELL_GAME_PARAMID_SOUND_FORMAT, &m_gameSoundFormat ); CheckError( success, "PARAM.SFO getParam( CELL_GAME_PARAMID_SOUND_FORMAT )" );

			// Access BDVD:
			success = cellGameContentPermit( m_gameContentPath, m_gameBasePath ) ; 
		}
		else
		{
			LocalError("cellGameDataCheck failed (line %d, code %d): %s\n", __LINE__, success, GetSonyErrorString(success) );
			return success;
		}
	}

	DiagnosticString( "-----------PARAM.SFO----------" );
	DiagnosticString( "\nTITLE      " ); DiagnosticString( m_gameTitle );
	DiagnosticString( "\nTITLE ID   " ); DiagnosticString( m_gameTitleID );
	DiagnosticString( "\nAPP_VER    " ); DiagnosticString( m_gameAppVer );

	if ( m_nBootAttribs & CELL_GAME_ATTRIBUTE_PATCH )
	{
		CellGameContentSize cgcs;
		memset( &cgcs, 0, sizeof(CellGameContentSize) );
		success = cellGamePatchCheck( &cgcs, NULL );
		if ( success == CELL_GAME_RET_OK )
		{
			success = cellGameGetParamString( CELL_GAME_PARAMID_APP_VER, m_gamePatchAppVer, sizeof( m_gamePatchAppVer ) ); CheckError( success, "PARAM.SFO PATCH getParam( CELL_GAME_PARAMID_APP_VER )" );
			DiagnosticString( "\nAPP_VER****" ); DiagnosticString( m_gamePatchAppVer );
			success = cellGameContentPermit( m_gamePatchContentPath, m_gamePatchBasePath ) ; 
		}
		else
		{
			LocalError("cellGamePatchCheck failed (line %d, code %d): %s\n", __LINE__, success, GetSonyErrorString(success) );
			return success;
		}
	}
	DiagnosticString( "\n------------------------------\n" );


	//////////////////////////////////////////////////////////////////////////
	//
	// filesystem path setup
	//
	//////////////////////////////////////////////////////////////////////////
	

	DiagnosticString( "----------FILESYSTEM----------" );
	DiagnosticString( "\nPS3_GAME   " ); DiagnosticString( m_gameContentPath );
	DiagnosticString( "\nUSRDIR     " ); DiagnosticString( m_gameBasePath );
	if ( m_nBootAttribs & CELL_GAME_ATTRIBUTE_PATCH )
	{
		DiagnosticString( "\nPS3_GAME***" ); DiagnosticString( m_gamePatchContentPath );
		DiagnosticString( "\nUSRDIR*****" ); DiagnosticString( m_gamePatchBasePath );
	}

#if 0
	// Get the game data directory on the hard disk. 
	success = cellGameDataCheck( CELL_GAME_GAMETYPE_GAMEDATA, m_gameTitleID, &size );
	if ( success == CELL_GAME_RET_NONE )
	{
		CellGameSetInitParams init; memset( &init, 0, sizeof( init ) );
		memcpy( init.title, m_gameTitle, sizeof( m_gameTitle ) );
		memcpy( init.titleId, m_gameTitleID, sizeof( m_gameTitleID ) );
		memcpy( init.version, m_gameAppVer, sizeof( m_gameAppVer ) );

		char tmp_contentInfoPath[CELL_GAME_PATH_MAX] = {0};
		char tmp_usrdirPath[CELL_GAME_PATH_MAX] = {0};

		success = cellGameCreateGameData( &init, tmp_contentInfoPath, tmp_usrdirPath );
		DiagnosticString( "\nTMP_GAME   " ); DiagnosticString( tmp_contentInfoPath );
		DiagnosticString( "\nTMP_USRD   " ); DiagnosticString( tmp_usrdirPath );
	}

	char contentInfoPath[256];
	if ( success == CELL_GAME_RET_OK )
	{
		success = cellGameContentPermit( contentInfoPath, m_gameHDDataPath );
	}
#else
	snprintf( m_gameHDDataPath, sizeof( m_gameHDDataPath ), "/dev_hdd0/game/NPUB30589/USRDIR" );
	//snprintf( m_gameHDDataPath, sizeof( m_gameHDDataPath ), "/dev_hdd0/game/BLUS30732/USRDIR" );
#endif
	DiagnosticString( "\nHDD_PATH   " ); DiagnosticString( m_gameHDDataPath );

	// Mount system cache
	if ( success >= CELL_GAME_RET_OK )
	{
		CellSysCacheParam sysCacheParams;
		memset( &sysCacheParams, 0, sizeof( CellSysCacheParam ) );
		memcpy( sysCacheParams.cacheId, GetWWMASTER_TitleID(), 10 );
		success = cellSysCacheMount( &sysCacheParams );
		if ( success >= CELL_GAME_RET_OK )
		{
			memcpy( m_gameSystemCachePath, sysCacheParams.getCachePath, sizeof( m_gameSystemCachePath ) );
			if ( uiFlagsMask & INIT_SYS_CACHE_CLEAR )
				cellSysCacheClear();
		}
	}
	DiagnosticString( "\nSYS_CACH   " ); DiagnosticString( m_gameSystemCachePath );

	// Determine where image files (maps, zips, etc.) are located:
	snprintf( m_gameImageDataPath, sizeof( m_gameImageDataPath ), m_gameBasePath );
	if ( uiFlagsMask & INIT_IMAGE_APP_HOME )
		snprintf( m_gameImageDataPath, sizeof( m_gameImageDataPath ), "/app_home/PS3_GAME/USRDIR" );
	else if ( uiFlagsMask & INIT_IMAGE_ON_HDD )
		snprintf( m_gameImageDataPath, sizeof( m_gameImageDataPath ), m_gameHDDataPath );
	else if ( uiFlagsMask & INIT_IMAGE_ON_BDVD )
		snprintf( m_gameImageDataPath, sizeof( m_gameImageDataPath ), "/dev_bdvd/PS3_GAME/USRDIR" );
	DiagnosticString( "\nIMAGE_PATH " ); DiagnosticString( m_gameImageDataPath );

	// Determine where PRX files are located:
	snprintf( m_gameExesPath, sizeof( m_gameExesPath ), "%s/bin", m_gameBasePath );
	if ( uiFlagsMask & INIT_PRX_APP_HOME )
		snprintf( m_gameExesPath, sizeof( m_gameExesPath ), "/app_home/PS3_GAME/USRDIR/bin" );
	else if ( uiFlagsMask & INIT_PRX_ON_HDD )
		snprintf( m_gameExesPath, sizeof( m_gameExesPath ), "%s/bin", m_gameHDDataPath );
	else if ( uiFlagsMask & INIT_PRX_ON_BDVD )
		snprintf( m_gameExesPath, sizeof( m_gameExesPath ), "/dev_bdvd/PS3_GAME/USRDIR/bin" );
	DiagnosticString( "\nPRX_PATH   " ); DiagnosticString( m_gameExesPath );

	DiagnosticString( "\n------------------------------\n" );

	// we cache the saves to a local directory -- keep that info here so it's in a uniform 
	// place accessible from everywhere. 
	strncpy( m_gameSavesShadowPath, m_gameSystemCachePath, sizeof(m_gameSavesShadowPath) );
	strncat( m_gameSavesShadowPath, "/tempsave/", sizeof(m_gameSavesShadowPath) );
	


	//////////////////////////////////////////////////////////////////////////
	//
	// finished
	//
	//////////////////////////////////////////////////////////////////////////

	if ( !bSysModuleIsLoaded ) // actually this means it wasn't loaded when we got into the function
	{
		cellSysmoduleUnloadModule( CELL_SYSMODULE_SYSUTIL_GAME );
	}


	return success;
}



const char *GetSonyErrorString( int errorcode )
{
	switch( errorcode )
	{
	case CELL_GAME_RET_OK:
		return "CELL_GAME_RET_OK";

	case CELL_GAME_ERROR_ACCESS_ERROR:
		return "HDD access error";

	case CELL_GAME_ERROR_BUSY:
		return "The call of an access preparing function was repeated";

	case CELL_GAME_ERROR_IN_SHUTDOWN:
		return "Processing cannot be executed because application termination is being processed";

	case CELL_GAME_ERROR_INTERNAL:
		return "Fatal error occurred in the utility";

	case CELL_GAME_ERROR_PARAM:
		return "There is an error in the argument (application bug)";

	case CELL_GAME_ERROR_BOOTPATH:
		return "Pathname of booted program file is too long" ;

	case CELL_SYSMODULE_ERROR_UNKNOWN:
		return "Tried to load an unknown PRX";

	case CELL_SYSMODULE_ERROR_FATAL:
		return "Sysmodule PRX load failed";

	case CELL_GAME_ERROR_BROKEN:
			return "The specified game content is corrupted";



	default:
		return "Unknown error code";
	}
}

/* The boot attributes member of CPs3ContentPathInfo is some combination of:
CELL_GAME_ATTRIBUTE_PATCH	Booted from a patch (only for a disc boot game)

CELL_GAME_ATTRIBUTE_APP_HOME	Booted from the host machine (development machine only)

CELL_GAME_ATTRIBUTE_DEBUG	Booted from the debugger (development machine only) 

CELL_GAME_ATTRIBUTE_XMBBUY	Rebooted from the game purchasing feature of the NP DRM utility

CELL_GAME_ATTRIBUTE_COMMERCE2_BROWSER	Rebooted from the store browsing feature of the NP IN-GAME commerce 2 utility

CELL_GAME_ATTRIBUTE_INVITE_MESSAGE	Booted from the game boot invitation message of the NP basic utility

CELL_GAME_ATTRIBUTE_CUSTOM_DATA_MESSAGE	Booted from a message with a custom data attachment of the NP basic utility

CELL_GAME_ATTRIBUTE_WEB_BROWSER	Booted from the full browser feature of the web browser utility 
*/