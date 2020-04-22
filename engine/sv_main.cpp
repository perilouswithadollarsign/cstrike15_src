//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose:
//
// $Workfile:     $
// $NoKeywords: $
//===========================================================================//

#include "server_pch.h"
#include "decal.h"
#include "host_cmd.h"
#include "cmodel_engine.h"
#include "sv_log.h"
#include "zone.h"
#include "sound.h"
#include "vox.h"
#include "EngineSoundInternal.h"
#include "checksum_engine.h"
#include "master.h"
#include "host.h"
#include "keys.h"
#include "vengineserver_impl.h"
#include "sv_filter.h"
#include "pr_edict.h"
#include "screen.h"
#include "sys_dll.h"
#include "world.h"
#include "sv_main.h"
#include "networkstringtableserver.h"
#include "datamap.h"
#include "filesystem_engine.h"
#include "string_t.h"
#include "vstdlib/random.h"
#include "networkstringtable.h"
#include "dt_send_eng.h"
#include "sv_packedentities.h"
#include "testscriptmgr.h"
#include "PlayerState.h"
#include "saverestoretypes.h"
#include "tier0/vprof.h"
#include "proto_oob.h"
#include "staticpropmgr.h"
#include "checksum_crc.h"
#include "console.h"
#include "tier0/icommandline.h"
#include "host_state.h"
#include "gl_matsysiface.h"
#include "GameEventManager.h"
#include "sys.h"
#include "tier3/tier3.h"
#include "voice.h"

#ifndef DEDICATED
#include "vgui_baseui_interface.h"
#endif
#include "cbenchmark.h"
#include "client.h"
#include "hltvserver.h"
#if defined( REPLAY_ENABLED )
#include "replay.h"
#include "replayserver.h"
#include "replayhistorymanager.h"
#endif
#include "keyvalues.h"
#include "sv_logofile.h"
#include "cl_steamauth.h"
#include "sv_steamauth.h"
#include "sv_plugin.h"
#include "DownloadListGenerator.h"
#include "sv_steamauth.h"
#include "LocalNetworkBackdoor.h"
#include "cvar.h"
#include "enginethreads.h"
#include "tier1/functors.h"
#include "vstdlib/jobthread.h"
#include "pure_server.h"
#include "datacache/idatacache.h"
#include "filesystem/IQueuedLoader.h"
#include "vstdlib/jobthread.h"
#include "SourceAppInfo.h"
#include "cl_rcon.h"
#include "toolframework/itoolframework.h"
#include "snd_audio_source.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "serializedentity.h"
#include "matchmaking/imatchframework.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#ifdef _LINUX
#include <syscall.h>
#endif

#ifdef _PS3
#include <sys/memory.h>
#endif

extern CNetworkStringTableContainer *networkStringTableContainerServer;
extern CNetworkStringTableContainer *networkStringTableContainerClient;
extern void Host_EnsureHostNameSet();
void OnHibernateWhenEmptyChanged( IConVar *var, const char *pOldValue, float flOldValue );
extern ConVar deathmatch;
extern ConVar sv_sendtables;
ConVar sv_hibernate_when_empty( "sv_hibernate_when_empty", "1", FCVAR_RELEASE, "Puts the server into extremely low CPU usage mode when no clients connected", OnHibernateWhenEmptyChanged );
ConVar sv_hibernate_punt_tv_clients( "sv_hibernate_punt_tv_clients", "0", FCVAR_RELEASE, "When enabled will punt all GOTV clients during hibernation" );
ConVar sv_hibernate_ms( "sv_hibernate_ms", "20", FCVAR_RELEASE, "# of milliseconds to sleep per frame while hibernating" );
ConVar sv_hibernate_ms_vgui( "sv_hibernate_ms_vgui", "20", FCVAR_RELEASE, "# of milliseconds to sleep per frame while hibernating but running the vgui dedicated server frontend" );
static ConVar sv_hibernate_postgame_delay( "sv_hibernate_postgame_delay", "5", FCVAR_RELEASE, "# of seconds to wait after final client leaves before hibernating.");

ConVar	host_flush_threshold( "host_flush_threshold", "12", FCVAR_RELEASE, "Memory threshold below which the host should flush caches between server instances" );
extern ConVar fps_max;

static ConVar sv_pausable_dev( "sv_pausable_dev", IsGameConsole() ? "0" : "1", FCVAR_DEVELOPMENTONLY, "Whether listen server is pausable when running -dev and playing solo against bots" );
static ConVar sv_pausable_dev_ds( "sv_pausable_dev_ds", "0", FCVAR_DEVELOPMENTONLY, "Whether dedicated server is pausable when running -dev and playing solo against bots" );

// Server default maxplayers value
#define DEFAULT_SERVER_CLIENTS	6
// This many players on a Lan with same key, is ok.
#define MAX_IDENTICAL_CDKEYS	5

CGameServer	sv;

CGlobalVars g_ServerGlobalVariables( false );

static int	current_skill;
extern bool UseCDKeyAuth();


void RevertAllModifiedLocalState()
{
	// cheats were disabled, revert all cheat cvars to their default values
	g_pCVar->RevertFlaggedConVars( FCVAR_CHEAT );

#ifndef DEDICATED
	// Reload all sound mixers that might have been tampered with
	extern bool MXR_LoadAllSoundMixers( void );
	MXR_LoadAllSoundMixers();
#endif

	DevMsg( "FCVAR_CHEAT cvars reverted to defaults.\n" );

	extern bool g_bHasIssuedMatSuppressOrDebug;
	if ( g_bHasIssuedMatSuppressOrDebug )
	{
		// Reload all materials in case the user has tried to cheat by using mat_suppress.
		if ( materials )
		{
			materials->ReloadMaterials( NULL );
		}
		g_bHasIssuedMatSuppressOrDebug = false;
	}
}

static void SV_CheatsChanged_f( IConVar *pConVar, const char *pOldString, float flOldValue )
{
    if ( IsGameConsole() )		// Cheats are always on for console, don't care reverting convars
        return;

    ConVarRef var( pConVar );
    if ( var.GetInt() == 0 )
    {
        RevertAllModifiedLocalState();
    }
	
	if ( g_pMatchFramework )
	{
		// Raise an event for other system to notice cheats state being manipulated
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"sv_cheats_changed", "value", var.GetInt() ) );
	}
}

static int g_sv_pure_mode = 1; // default to on
static void SV_Pure_f( const CCommand &args )
{
    int pure_mode = -1;
    if ( args.ArgC() == 2 )
    {
        pure_mode = atoi( args[1] );
    }

    Msg( "--------------------------------------------------------\n" );
    if ( pure_mode == 0 || pure_mode == 1 || pure_mode == 2 )
    {
        // you can turn off pure mode after specifying on the command line ( allow 0 here )
		int pure_mode_cmd_line = 1; // default is on
		if ( CommandLine()->CheckParm("+sv_pure") )
			pure_mode_cmd_line = CommandLine()->ParmValue( "+sv_pure", 1 );
		else if ( CommandLine()->CheckParm("-sv_pure") )
			pure_mode_cmd_line = CommandLine()->ParmValue( "-sv_pure", 1 );
        if ( pure_mode_cmd_line == 0 && pure_mode != 0 )
        {
            // if it wasn't set on the command line
            Msg( "sv_pure must be specified on the command line to function properly. sv_pure mode not changed\n" );
            return;
        }
        if ( pure_mode == 2 )
        {
            Msg( "sv_pure 2 is obsolete. Changed to 1.\n" );
            pure_mode = 1;
        }
        // Set the value.
        if ( pure_mode == g_sv_pure_mode )
        {
            Msg( "sv_pure value unchanged (current value is %d).\n", g_sv_pure_mode );
        }
        else
        {
            g_sv_pure_mode = pure_mode;
            Msg( "sv_pure set to %d.\n", g_sv_pure_mode );

            if ( sv.IsActive() )
            {
                Msg( "Note: Changes to sv_pure take effect when the next map is loaded.\n" );
            }
        }
    }
    else
    {
        Msg( "sv_pure:"
            "\n\nIf set to 1, the server will force all client files except the whitelisted ones "
            "(in pure_server_whitelist.txt) to match the server's files. " );
    }

    if ( pure_mode == -1 )
    {
        // If we're a client on a server with sv_pure = 1, display the current whitelist.
#ifndef DEDICATED
        if ( GetBaseLocalClient().IsConnected() )
        {
            Msg( "\n\n" );
            extern void CL_PrintWhitelistInfo(); // from cl_main.cpp
            CL_PrintWhitelistInfo();
        }
        else
#endif
        {
            Msg( "\nCurrent sv_pure value is %d.\n", g_sv_pure_mode );
        }
    }
    Msg( "--------------------------------------------------------\n" );
}

static ConCommand sv_pure( "sv_pure", SV_Pure_f, "Show user data." );

ConVar	sv_pure_kick_clients( "sv_pure_kick_clients", "1", FCVAR_RELEASE, "If set to 1, the server will kick clients with mismatching files. Otherwise, it will issue a warning to the client." );
ConVar	sv_pure_trace( "sv_pure_trace", "0", FCVAR_RELEASE, "If set to 1, the server will print a message whenever a client is verifying a CRC for a file." );
ConVar	sv_pure_consensus( "sv_pure_consensus", "99999999", FCVAR_RELEASE, "Minimum number of file hashes to agree to form a consensus." );
ConVar	sv_pure_retiretime( "sv_pure_retiretime", "900", FCVAR_RELEASE, "Seconds of server idle time to flush the sv_pure file hash cache." );

ConVar  sv_cheats( "sv_cheats", "0", FCVAR_NOTIFY|FCVAR_REPLICATED | FCVAR_RELEASE, "Allow cheats on server", SV_CheatsChanged_f );
ConVar  sv_lan( "sv_lan", "0", FCVAR_RELEASE, "Server is a lan server ( no heartbeat, no authentication, no non-class C addresses )" );


static	ConVar	sv_pausable( "sv_pausable","0", FCVAR_RELEASE, "Is the server pausable." );
static	ConVar	sv_contact( "sv_contact", "", FCVAR_NOTIFY  | FCVAR_RELEASE, "Contact email for server sysop" );
static	ConVar	sv_cacheencodedents("sv_cacheencodedents", "1", 0, "If set to 1, does an optimization to prevent extra SendTable_Encode calls.");
        ConVar	sv_voicecodec("sv_voicecodec", "vaudio_celt", FCVAR_RELEASE | FCVAR_REPLICATED, "Specifies which voice codec DLL to use in a game. Set to the name of the DLL without the extension.");
static	ConVar	sv_voiceenable( "sv_voiceenable", "1", FCVAR_ARCHIVE|FCVAR_NOTIFY  | FCVAR_RELEASE ); // set to 0 to disable all voice forwarding.
        ConVar  sv_downloadurl( "sv_downloadurl", "", FCVAR_REPLICATED | FCVAR_RELEASE, "Location from which clients can download missing files" );
        ConVar  sv_consistency( "sv_consistency", "0", FCVAR_REPLICATED | FCVAR_RELEASE, "Whether the server enforces file consistency for critical files" );
        ConVar	sv_maxreplay("sv_maxreplay", "0", 0, "Maximum replay time in seconds", true, 0, true, 30 );

ConVar  sv_mincmdrate( "sv_mincmdrate", "64", FCVAR_REPLICATED | FCVAR_RELEASE, "This sets the minimum value for cl_cmdrate. 0 == unlimited." );
ConVar  sv_maxcmdrate( "sv_maxcmdrate", "64", FCVAR_REPLICATED, "(If sv_mincmdrate is > 0), this sets the maximum value for cl_cmdrate." );
ConVar  sv_client_cmdrate_difference( "sv_client_cmdrate_difference", "0", FCVAR_REPLICATED | FCVAR_RELEASE,
    "cl_cmdrate is moved to within sv_client_cmdrate_difference units of cl_updaterate before it "
    "is clamped between sv_mincmdrate and sv_maxcmdrate." );

ConVar  sv_client_min_interp_ratio( "sv_client_min_interp_ratio", "1", FCVAR_REPLICATED, 
                                   "This can be used to limit the value of cl_interp_ratio for connected clients "
                                   "(only while they are connected).\n"
                                   "              -1 = let clients set cl_interp_ratio to anything\n"
                                   " any other value = set minimum value for cl_interp_ratio"
                                   );
ConVar  sv_client_max_interp_ratio( "sv_client_max_interp_ratio", "5", FCVAR_REPLICATED, 
                                   "This can be used to limit the value of cl_interp_ratio for connected clients "
                                   "(only while they are connected). If sv_client_min_interp_ratio is -1, "
                                   "then this cvar has no effect."
                                   );
ConVar  sv_client_predict( "sv_client_predict", "-1", FCVAR_REPLICATED, 
    "This can be used to force the value of cl_predict for connected clients "
    "(only while they are connected).\n"
    "   -1 = let clients set cl_predict to anything\n"
    "    0 = force cl_predict to 0\n"
    "    1 = force cl_predict to 1"
    );


void OnTVEnablehanged( IConVar *pConVar, const char *pOldString, float flOldValue )
{
    ConVarRef var( pConVar );

    //Let's check maxclients and make sure we have room for SourceTV
    if ( var.GetBool() == true )
    {
        sv.InitMaxClients();
    }
}

ConVar tv_enable( "tv_enable", "0", FCVAR_NOTIFY | FCVAR_RELEASE, "Activates GOTV on server (0=off;1=on;2=on when reserved)", OnTVEnablehanged );
ConVar tv_enable1( "tv_enable1", "0", FCVAR_NOTIFY | FCVAR_RELEASE, "Activates GOTV[1] on server (0=off;1=on;2=on when reserved)", OnTVEnablehanged );

extern ConVar *sv_noclipduringpause;

static bool s_bForceSend = false;

void SV_ForceSend()
{
    s_bForceSend = true;
}

//-----------------------------------------------------------------------------
// Set the flush trigger.
//-----------------------------------------------------------------------------
bool g_bFlushMemoryOnNextServer;
int g_FlushMemoryOnNextServerCounter;
void SV_FlushMemoryOnNextServer()
{
    g_bFlushMemoryOnNextServer = true;
    g_FlushMemoryOnNextServerCounter++;
}

//-----------------------------------------------------------------------------
// Check and possibly set the flush trigger.
//-----------------------------------------------------------------------------
void SV_CheckForFlushMemory( const char *pCurrentMapName, const char *pDestMapName )
{
#ifdef _GAMECONSOLE
    if ( host_flush_threshold.GetInt() == 0 )
        return;

    // There are three cases in which we flush memory
    //   Case 1: changing from one map to another
    //          -> flush temp data caches
    //   Case 2: loading any map (inc. A to A) and free memory is below host_flush_threshold MB
    //          -> flush everything
    //   Case 3: loading a 'blacklisted' map (the known biggest memory users, or where texture sets change)
    //          -> flush everything
    static const char *mapBlackList[] = 
    {
        ""
    };

    char szCurrentMapName[MAX_PATH];
    char szDestMapName[MAX_PATH];
    if ( pCurrentMapName )
    {
        V_FileBase( pCurrentMapName, szCurrentMapName, sizeof( szCurrentMapName ) );
    }
    else
    {
        szCurrentMapName[0] = '\0';
    }
    pCurrentMapName = szCurrentMapName;

    if ( pDestMapName )
    {
        V_FileBase( pDestMapName, szDestMapName, sizeof( szDestMapName ) );
    }
    else
    {
        szDestMapName[0] = '\0';
    }
    pDestMapName = szDestMapName;

    bool bIsMapChanging = pCurrentMapName[0] && V_stricmp( pCurrentMapName, pDestMapName );

    bool bIsDestMapBlacklisted = false;
    for ( int i = 0; i < ARRAYSIZE( mapBlackList ); i++ )
    {
        if ( pDestMapName && !V_stricmp( pDestMapName, mapBlackList[i] ) )
        {
            bIsDestMapBlacklisted = true;
        }
    }
    
    size_t dwSizePhysical = 0xffffffff;
#ifdef _WIN32
    {
        MEMORYSTATUS stat;
        GlobalMemoryStatus( &stat );
        dwSizePhysical = stat.dwAvailPhys;
    }
#elif defined( _PS3 )
    {
        sys_memory_info_t smi = {0,0};
        sys_memory_get_user_memory_size( &smi );
        dwSizePhysical = smi.available_user_memory;
    }
#endif

    // console csgo wants a full flush always for fragmentation concerns
    bool bFullFlush = ( ( dwSizePhysical < host_flush_threshold.GetInt() * 1024 * 1024 ) || ( bIsDestMapBlacklisted && bIsMapChanging ) ) || ( IsGameConsole() && !V_stricmp( COM_GetModDirectory(), "csgo" ) );
    bool bPartialFlush = !bFullFlush && bIsMapChanging;

    const char *pReason = "No Flush";
    if ( bFullFlush )
    {
        // Flush everything; all map data should get reloaded
        SV_FlushMemoryOnNextServer();
        g_pDataCache->Flush();
        wavedatacache->Flush();
        pReason = "Full Flush";
    }
    else if ( bPartialFlush )
    {
        // Flush temporary data (async anim, non-locked async audio)
        g_pMDLCache->Flush( MDLCACHE_FLUSH_ANIMBLOCK );
        wavedatacache->Flush();
        pReason = "Partial Flush";
    }

    Msg( "Current Map: (%s), Next Map: (%s), %s\n", (pCurrentMapName[0] ? pCurrentMapName : ""), (pDestMapName[0] ? pDestMapName : ""), pReason );
#endif	// console
}

//-----------------------------------------------------------------------------
// Returns true if flush occured, false otherwise.
//-----------------------------------------------------------------------------
bool SV_FlushMemoryIfMarked()
{
    if ( g_bFlushMemoryOnNextServer )
    {
        g_bFlushMemoryOnNextServer = false;
        if ( IsGameConsole() )
        {
            g_pQueuedLoader->PurgeAll();
        }
        g_pDataCache->Flush();
        g_pMaterialSystem->CompactMemory();
        g_pFileSystem->AsyncFinishAll();
#if !defined( DEDICATED )
        extern CThreadMutex g_SndMutex;
        g_SndMutex.Lock();
        g_pFileSystem->AsyncSuspend();
        g_pThreadPool->SuspendExecution();
        g_pMemAlloc->CompactHeap();
        g_pThreadPool->ResumeExecution();
        g_pFileSystem->AsyncResume();
        g_SndMutex.Unlock();
#endif // DEDICATED

        return true;
    }
    else
    {
        g_pMemAlloc->CompactHeap();
		if ( IsPlatformOpenGL() )
		{
			// Frees all memory associated with models and their material
			// This function is usually called at the end of loading but on OSX we are calling
			// it before loading the next level in an attempt to free as much memory as possible.
			// Could easily get over the 4GB virtual mem limit when transition from cs_thunder to 
			// de_cbble for example.
			modelloader->PurgeUnusedModels();
		}
    }
    return false;
}

// Prints important entity creation/deletion events to console
#if defined( _DEBUG )
ConVar  sv_deltatrace( "sv_deltatrace", "0", 0, "For debugging, print entity creation/deletion info to console." );
#define TRACE_DELTA( text ) if ( sv_deltatrace.GetInt() ) { ConMsg( text ); };
#else
#define TRACE_DELTA( funcs )
#endif


#if defined( DEBUG_NETWORKING )

//-----------------------------------------------------------------------------
// Opens the recording file
//-----------------------------------------------------------------------------

static FILE* OpenRecordingFile()
{
    FILE* fp = 0;
    static bool s_CantOpenFile = false;
    static bool s_NeverOpened = true;
    if (!s_CantOpenFile)
    {
        fp = fopen( "svtrace.txt", s_NeverOpened ? "wt" : "at" );
        if (!fp)
        {
            s_CantOpenFile = true;
        }
        s_NeverOpened = false;
    }
    return fp;
}

//-----------------------------------------------------------------------------
// Records an argument for a command, flushes when the command is done
//-----------------------------------------------------------------------------
/*
void SpewToFile( char const* pFmt, ... )
static void SpewToFile( const char* pFmt, ... )
{
    static CUtlVector<unsigned char> s_RecordingBuffer;

    char temp[2048];
    va_list args;

    va_start( args, pFmt );
    int len = Q_vsnprintf( temp, sizeof( temp ), pFmt, args );
    va_end( args );
    Assert( len < 2048 );

    int idx = s_RecordingBuffer.AddMultipleToTail( len );
    memcpy( &s_RecordingBuffer[idx], temp, len );
    if ( 1 ) //s_RecordingBuffer.Size() > 8192)
    {
        FILE* fp = OpenRecordingFile();
        fwrite( s_RecordingBuffer.Base(), 1, s_RecordingBuffer.Size(), fp );
        fclose( fp );

        s_RecordingBuffer.RemoveAll();
    }
}
*/

#endif // #if defined( DEBUG_NETWORKING )


/*void SV_Init(bool isDedicated)
{
    sv.Init( isDedicated );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void SV_Shutdown( void )
{
    sv.Shutdown();
}*/

void CGameServer::Clear( void )
{
	m_pModelPrecacheTable = NULL;
	m_pDynamicModelTable = NULL;
	m_pGenericPrecacheTable = NULL;
	m_pSoundPrecacheTable = NULL;
	m_pDecalPrecacheTable = NULL;
	m_bIsLevelMainMenuBackground = false;

    m_bLoadgame = false;
    
    host_state.SetWorldModel( NULL );	

    Q_memset( m_szStartspot, 0, sizeof( m_szStartspot ) );
    
    num_edicts = 0;
    max_edicts = 0;
    edicts = NULL;
    g_ServerGlobalVariables.maxEntities = 0;
    g_ServerGlobalVariables.pEdicts = NULL;

    // Clear the instance baseline indices in the ServerClasses.
    if ( serverGameDLL )
    {
        for( ServerClass *pCur = serverGameDLL->GetAllServerClasses(); pCur; pCur=pCur->m_pNext )
        {
            pCur->m_InstanceBaselineIndex = INVALID_STRING_INDEX;
        }
    }

    for ( int i = 0; i < m_TempEntities.Count(); i++ )
    {
        delete m_TempEntities[i];
    }

    m_TempEntities.Purge();

    BaseClass::Clear();
}



//-----------------------------------------------------------------------------
// Purpose: Create any client/server string tables needed internally by the engine
//-----------------------------------------------------------------------------
ASSERT_INVARIANT( ABSOLUTE_PLAYER_LIMIT <= 2048 ); // must be reasonable number that has a power-of-2 value not less than it
void CGameServer::CreateEngineStringTables( void )
{
    int i,j;

    m_StringTables->SetTick( m_nTickCount ); // set first tick

    int fileFlags = NSF_DICTIONARY_ENABLED;

    m_pDownloadableFileTable = m_StringTables->CreateStringTable( 
        DOWNLOADABLE_FILE_TABLENAME, 
        MAX_DOWNLOADABLE_FILES,
        0,
        0,
        fileFlags );

    m_pModelPrecacheTable = m_StringTables->CreateStringTable( 
        MODEL_PRECACHE_TABLENAME, 
        MAX_MODELS,
        sizeof ( CPrecacheUserData ),
        PRECACHE_USER_DATA_NUMBITS,
        fileFlags );

    m_pGenericPrecacheTable = m_StringTables->CreateStringTable(
        GENERIC_PRECACHE_TABLENAME,
        MAX_GENERIC,
        sizeof ( CPrecacheUserData ),
        PRECACHE_USER_DATA_NUMBITS,
        fileFlags );

    m_pSoundPrecacheTable = m_StringTables->CreateStringTable(
        SOUND_PRECACHE_TABLENAME,
        MAX_SOUNDS,
        sizeof ( CPrecacheUserData ),
        PRECACHE_USER_DATA_NUMBITS,
        fileFlags );

    m_pDecalPrecacheTable = m_StringTables->CreateStringTable(
        DECAL_PRECACHE_TABLENAME,
        MAX_BASE_DECALS,
        sizeof ( CPrecacheUserData ),
        PRECACHE_USER_DATA_NUMBITS,
        fileFlags );

    m_pInstanceBaselineTable = m_StringTables->CreateStringTable(
        INSTANCE_BASELINE_TABLENAME,
        MAX_DATATABLES );

    m_pLightStyleTable = m_StringTables->CreateStringTable( 
        LIGHT_STYLES_TABLENAME, 
        MAX_LIGHTSTYLES );

	int nAbsolutePlayerLimitPowerOf2 = 1;
	while ( nAbsolutePlayerLimitPowerOf2 < ABSOLUTE_PLAYER_LIMIT )
		nAbsolutePlayerLimitPowerOf2 <<= 1;
    m_pUserInfoTable = m_StringTables->CreateStringTable(
        USER_INFO_TABLENAME,
        nAbsolutePlayerLimitPowerOf2 );

	m_pDynamicModelTable = m_StringTables->CreateStringTable( 
		DYNAMIC_MODEL_TABLENAME, 
		MAX_MODELS,
		1, // Single bit of userdata: is this model loaded on the server yet
		1 );

    // Send the query info..
    m_pServerStartupTable = m_StringTables->CreateStringTable( 
        SERVER_STARTUP_DATA_TABLENAME,
        4 );
    SetQueryPortFromSteamServer();
    CopyPureServerWhitelistToStringTable();


    Assert ( m_pModelPrecacheTable && 
             m_pGenericPrecacheTable &&
             m_pSoundPrecacheTable &&
             m_pDecalPrecacheTable &&
             m_pInstanceBaselineTable &&
             m_pLightStyleTable &&
             m_pUserInfoTable &&
             m_pServerStartupTable &&
             m_pDownloadableFileTable );

    // create an empty lightstyle table with unique index names
    for ( i = 0; i<MAX_LIGHTSTYLES; i++ )
    {
        char name[8]; Q_snprintf( name, 8, "%i", i );
        j = m_pLightStyleTable->AddString( true, name );
        Assert( j==i ); // indices must match 
    }

    for ( i = 0; i<ABSOLUTE_PLAYER_LIMIT; i++ )
    {
        char name[8]; Q_snprintf( name, 8, "%i", i );
        j = m_pUserInfoTable->AddString( true, name );
        Assert( j==i ); // indices must match 
    }

    // set up the downloadable files generator
    DownloadListGenerator().SetStringTable( m_pDownloadableFileTable );
}

void CGameServer::SetQueryPortFromSteamServer()
{
    if ( !m_pServerStartupTable )
        return;
        
    int queryPort = Steam3Server().GetQueryPort();
    m_pServerStartupTable->AddString( true, "QueryPort", sizeof( queryPort ), &queryPort );
}

void CGameServer::CopyPureServerWhitelistToStringTable()
{
    if ( !m_pPureServerWhitelist )
        return;
    
    CUtlBuffer buf;
    m_pPureServerWhitelist->Encode( buf );
    m_pServerStartupTable->AddString( true, "PureServerWhitelist", buf.TellPut(), buf.Base() );
}


void SV_InstallClientStringTableMirrors( void )
{
#ifndef DEDICATED
#ifndef SHARED_NET_STRING_TABLES

    int numTables = networkStringTableContainerServer->GetNumTables();

    for ( int i =0; i<numTables; i++)
    {
        // iterate through server tables
        CNetworkStringTable *serverTable = 
            (CNetworkStringTable*)networkStringTableContainerServer->GetTable( i );

        if ( !serverTable )
            continue;

        // get mathcing client table
        CNetworkStringTable *clientTable = 
            (CNetworkStringTable*)networkStringTableContainerClient->FindTable( serverTable->GetTableName() );

        if ( !clientTable )
        {
            DevMsg("SV_InstallClientStringTableMirrors! Missing client table \"%s\".\n ", serverTable->GetTableName() );
            continue;
        }

        // link client table to server table
        serverTable->SetMirrorTable( 0, clientTable );
    }
#endif
#endif
}



//-----------------------------------------------------------------------------
// user <name or userid>
//
// Dump userdata / masterdata for a user
//-----------------------------------------------------------------------------
CON_COMMAND( user, "Show user data." )
{
    int		uid;
    int		i;
    
    if ( !sv.IsActive() )
    {
        ConMsg( "Can't 'user', not running a server\n" );
        return;
    }

    if (args.ArgC() != 2)
    {
        ConMsg ("Usage: user <username / userid>\n");
        return;
    }

    uid = atoi(args[1]);

    for (i=0 ; i< sv.GetClientCount() ; i++)
    {
        IClient *cl = sv.GetClient( i );

        if ( !cl->IsConnected() )
            continue;

        if ( ( cl->GetPlayerSlot()== uid ) || !Q_strcmp( cl->GetClientName(), args[1]) )
        {
            ConMsg ("TODO: SV_User_f.\n");
            return;
        }
    }

    ConMsg ("User not in server.\n");
}


//-----------------------------------------------------------------------------
// Dump userids for all current players
//-----------------------------------------------------------------------------
CON_COMMAND( users, "Show user info for players on server." )
{
    if ( !sv.IsActive() )
    {
        ConMsg( "Can't 'users', not running a server\n" );
        return;
    }

    int c = 0;
    ConMsg ("<slot:userid:\"name\">\n");
    for ( int i=0 ; i< sv.GetClientCount() ; i++ )
    {
        IClient *cl = sv.GetClient( i );

        if ( cl->IsConnected() )
        {
            ConMsg ("%i:%i:\"%s\"\n", cl->GetPlayerSlot(), cl->GetUserID(), cl->GetClientName() );
            c++;
        }
    }

    ConMsg ( "%i users\n", c );
}

//-----------------------------------------------------------------------------
// Purpose: Determine the value of sv.maxclients
//-----------------------------------------------------------------------------
bool CL_IsHL2Demo(); // from cl_main.cpp
bool CL_IsPortalDemo(); // from cl_main.cpp

void CGameServer::InitMaxClients( void )
{
    int minmaxplayers = 1;
    int maxmaxplayers = ABSOLUTE_PLAYER_LIMIT;
    int defaultmaxplayers = 1;

    if ( serverGameClients )
    {
        serverGameClients->GetPlayerLimits( minmaxplayers, maxmaxplayers, defaultmaxplayers );

        if ( minmaxplayers < 1 )
        {
            Sys_Error( "GetPlayerLimits:  min maxplayers must be >= 1 (%i)", minmaxplayers );
        }
        else if ( defaultmaxplayers < 1 )
        {
            Sys_Error( "GetPlayerLimits:  default maxplayers must be >= 1 (%i)", minmaxplayers );
        }

        if ( minmaxplayers > maxmaxplayers || defaultmaxplayers > maxmaxplayers )
        {
            Sys_Error( "GetPlayerLimits:  min maxplayers %i > max %i", minmaxplayers, maxmaxplayers );
        }

        if ( maxmaxplayers > ABSOLUTE_PLAYER_LIMIT )
        {
            Sys_Error( "GetPlayerLimits:  max players limited to %i", ABSOLUTE_PLAYER_LIMIT );
        }
    }

    // Determine absolute limit
    m_nMinClientsLimit = minmaxplayers;
    m_nMaxClientsLimit = maxmaxplayers;

    // Check for command line override
#if defined( CSTRIKE15 )
    int newmaxplayers = -HLTV_SERVER_MAX_COUNT; // CStrike doesn't allow command line override for maxplayers
#else
	int newmaxplayers = CommandLine()->ParmValue( "-maxplayers", -1 );
#endif

	for ( int nHltvServerIndex = 0; nHltvServerIndex < HLTV_SERVER_MAX_COUNT; ++nHltvServerIndex )
	{
#if defined( REPLAY_ENABLED )
		if ( GetIndexedConVar( tv_enable, nHltvServerIndex ).GetBool() || Replay_IsEnabled() ) 
#else
		if ( GetIndexedConVar( tv_enable, nHltvServerIndex ).GetBool() )
#endif
		{
			newmaxplayers += 1;
			m_nMaxClientsLimit += 1;
		}
	}

    if ( newmaxplayers >= 1 )
    {
        // Never go above/below what the game .dll can handle
        newmaxplayers	= MIN( newmaxplayers, maxmaxplayers );
        m_nMaxClientsLimit	= MAX( m_nMinClientsLimit, newmaxplayers );
    }
    else
    {
        newmaxplayers	= defaultmaxplayers;
    }

    newmaxplayers = clamp( newmaxplayers, m_nMinClientsLimit, m_nMaxClientsLimit );

    if ( ( CL_IsHL2Demo() || CL_IsPortalDemo() ) && !IsDedicated() )
    {
        newmaxplayers = 1;
        m_nMinClientsLimit = 1;
        m_nMaxClientsLimit = 1;
    }

    SetMaxClients( newmaxplayers );
}

//-----------------------------------------------------------------------------
// Purpose: Changes the maximum # of players allowed on the server.
//  Server cannot be running when command is issued.
//-----------------------------------------------------------------------------
CON_COMMAND( maxplayers, "Change the maximum number of players allowed on this server." )
{
#if defined( CSTRIKE15 )
	ConMsg( "Maxplayers is deprecated, set it in gamemodes_server.txt.example or use -maxplayers_override instead.\n");
	return;
#endif

    if ( args.ArgC () != 2 )
    {
        ConMsg ("\"maxplayers\" is \"%u\"\n", sv.GetMaxClients() );
        if ( serverGameClients )
        {
            int minmaxplayers = 1;
            int maxmaxplayers = 1;
            int defaultmaxplayers = 1;

            serverGameClients->GetPlayerLimits( minmaxplayers, maxmaxplayers, defaultmaxplayers );

            ConMsg ("\"mininum_maxplayers\" is \"%u\"\n", minmaxplayers );
            ConMsg ("\"absolute_maxplayers\" is \"%u\"\n", maxmaxplayers );
            ConMsg ("\"default_maxplayers\" is \"%u\"\n", defaultmaxplayers );
#ifndef DEDICATED
            if ( toolframework->InToolMode() ) 
            {
                ConMsg ("\"max_splitscreen_players\" is \"%u\" (limited by -tools mode)\n", host_state.max_splitscreen_players );
            }
            else
#endif
            {
                ConMsg ("\"max_splitscreen_players\" is \"%u\"\n", host_state.max_splitscreen_players );
            }
        }
        return;
    }

	// Allow maxplayers to change if server is hibernating so matchmaking servers can switch to accomodate different
	// player limits for different game modes.

    if ( sv.IsActive() && !sv.IsHibernating() )
    {
        ConMsg( "Maxplayers can only be changed while server is hibernating.\n");
        return;
    }

    sv.SetMaxClients( Q_atoi( args[ 1 ] ) );
}

int SV_BuildSendTablesArray( ServerClass *pClasses, SendTable **pTables, int nMaxTables )
{
        int nTables = 0;

        for( ServerClass *pCur=pClasses; pCur; pCur=pCur->m_pNext )
        {
                ErrorIfNot( nTables < nMaxTables, ("SV_BuildSendTablesArray: too many SendTables!") );
                pTables[nTables] = pCur->m_pTable;
                ++nTables;
        }

        return nTables;
}


// Builds an alternate copy of the datatable for any classes that have datatables with props excluded.
void SV_InitSendTables( ServerClass *pClasses )
{
    SendTable *pTables[MAX_DATATABLES];
    int nTables = SV_BuildSendTablesArray( pClasses, pTables, ARRAYSIZE( pTables ) );

    SendTable_Init( pTables, nTables );
}


void SV_TermSendTables( ServerClass *pClasses )
{
    SendTable_Term();
}


//-----------------------------------------------------------------------------
// Purpose: returns which games/mods we're allowed to play
//-----------------------------------------------------------------------------
struct ModDirPermissions_t
{
    int m_iAppID;
    const char *m_pchGameDir;
};

static ModDirPermissions_t g_ModDirPermissions[] =
{
    { GetAppSteamAppId( k_App_CSS ),        GetAppModName( k_App_CSS ) },
    { GetAppSteamAppId( k_App_DODS ),       GetAppModName( k_App_DODS ) },
    { GetAppSteamAppId( k_App_HL2MP ),      GetAppModName( k_App_HL2MP ) },
    { GetAppSteamAppId( k_App_LOST_COAST ), GetAppModName( k_App_LOST_COAST ) },
    { GetAppSteamAppId( k_App_HL1DM ),      GetAppModName( k_App_HL1DM ) },
    { GetAppSteamAppId( k_App_PORTAL ),     GetAppModName( k_App_PORTAL ) },
    { GetAppSteamAppId( k_App_HL2 ),        GetAppModName( k_App_HL2 ) },
    { GetAppSteamAppId( k_App_HL2_EP1 ),    GetAppModName( k_App_HL2_EP1 ) },
    { GetAppSteamAppId( k_App_HL2_EP2 ),    GetAppModName( k_App_HL2_EP2 ) },
    { GetAppSteamAppId( k_App_TF2 ),        GetAppModName( k_App_TF2 ) },
    { GetAppSteamAppId( k_App_L4D ),        GetAppModName( k_App_L4D ) },
};

bool ServerDLL_Load( bool bServerOnly )
{
    // Load in the game .dll
    LoadEntityDLLs( GetBaseDirectory(), bServerOnly );
    return g_ServerFactory != NULL;
}

void ServerDLL_Unload()
{
    UnloadEntityDLLs();
}

//-----------------------------------------------------------------------------
// Purpose: Loads the game .dll
//-----------------------------------------------------------------------------
void SV_InitGameDLL( void )
{
    COM_TimestampedLog( "SV_InitGameDLL" );

	SV_SetSteamCrashComment();

    // Clear out the command buffer.
    Cbuf_Execute();

    // Don't initialize a second time
    if ( sv.dll_initialized )
    {
        return;
    }

#if !defined(DEDICATED) && !defined( _GAMECONSOLE )
    bool CL_IsHL2Demo();
    if ( CL_IsHL2Demo() && !sv.IsDedicated() && Q_stricmp( COM_GetModDirectory(), "hl2" ) )
    {
        Error( "The HL2 demo is unable to run Mods.\n" );
        return;			
    } 

    if ( CL_IsPortalDemo() && !sv.IsDedicated() && Q_stricmp( COM_GetModDirectory(), "portal" ) )
    {
        Error( "The Portal demo is unable to run Mods.\n" );
        return;			
    } 

    // check permissions

    if ( Steam3Client().SteamApps() && g_pFileSystem->IsSteam() && !CL_IsHL2Demo() && !CL_IsPortalDemo() )
    {
        bool bVerifiedMod = false;

        // find the game dir we're running
        for ( int i = 0; i < ARRAYSIZE( g_ModDirPermissions ); i++ )
        {
            if ( !Q_stricmp( COM_GetModDirectory(), g_ModDirPermissions[i].m_pchGameDir ) )
            {
                // we've found the mod, make sure we own the app
                if (  Steam3Client().SteamApps()->BIsSubscribedApp( g_ModDirPermissions[i].m_iAppID ) )
                {
                    bVerifiedMod = true;
                }
                else
                {
                    Error( "No permissions to run '%s'\n", COM_GetModDirectory() );
                    return;			
                }

                break;
            }
        }

        if ( !bVerifiedMod )
        {
            // make sure they can run the Source engine
            if ( ! Steam3Client().SteamApps()->BIsSubscribedApp( 215  ) )
            {
                Error( "A Source engine game is required to run mods\n" );
                return;
            }
        }
    }
#endif

    if ( !serverGameDLL )
    {
        Warning( "Failed to load server binary\n" );
        return;
    }

    // Flag that we've started the game .dll
    sv.dll_initialized = true;

    COM_TimestampedLog( "serverGameDLL->DLLInit - Start" );

    // Tell the game DLL to start up
    if ( !serverGameDLL->DLLInit( g_GameSystemFactory, g_AppSystemFactory, g_AppSystemFactory, &g_ServerGlobalVariables ) )
    {
        Sys_Error( "serverGameDLL->DLLInit() failed.\n");
    }

    COM_TimestampedLog( "serverGameDLL->DLLInit - Finish" );

    if ( CommandLine()->FindParm( "-NoLoadPluginsForClient" ) == 0 )
        g_pServerPluginHandler->LoadPlugins(); // load 3rd party plugins
    

    // let's not have any servers with no name
    Host_EnsureHostNameSet();

    sv_noclipduringpause = ( ConVar * )g_pCVar->FindVar( "sv_noclipduringpause" );

    COM_TimestampedLog( "SV_InitSendTables" );

    // Make extra copies of data tables if they have SendPropExcludes.
    SV_InitSendTables( serverGameDLL->GetAllServerClasses() );

    host_state.interval_per_tick = serverGameDLL->GetTickInterval();
    if ( host_state.interval_per_tick < MINIMUM_TICK_INTERVAL ||
         host_state.interval_per_tick > MAXIMUM_TICK_INTERVAL )
    {
        Sys_Error( "GetTickInterval returned bogus tick interval (%f)[%f to %f is valid range]", host_state.interval_per_tick,
            MINIMUM_TICK_INTERVAL, MAXIMUM_TICK_INTERVAL );
    }

    extern ConVar sv_maxupdaterate;
    sv_maxupdaterate.SetValue(1.0f / host_state.interval_per_tick);
    sv_maxcmdrate.SetValue(1.0f / host_state.interval_per_tick);

    host_state.max_splitscreen_players_clientdll = clamp( serverGameClients->GetMaxSplitscreenPlayers(), 1, MAX_SPLITSCREEN_CLIENTS );
    host_state.max_splitscreen_players = host_state.max_splitscreen_players_clientdll;
    if ( CommandLine()->CheckParm( "-tools" ) )
    {
        Msg( "Clamping split screen users to 1 due to -tools mode\n" );
        host_state.max_splitscreen_players = 1;
    }

    if ( host_state.max_splitscreen_players > 1 )
    {
        Msg( "Game supporting (%d) split screen players\n", host_state.max_splitscreen_players );
    }

    g_pCVar->SetMaxSplitScreenSlots( host_state.max_splitscreen_players );

    // set maxclients limit based on Mod or commandline settings
    sv.InitMaxClients();

    // Execute and server commands the game .dll added at startup
    Cbuf_Execute();
}

//
// Release resources associated with extension DLLs.
//
void SV_ShutdownGameDLL( void )
{
    if ( !sv.dll_initialized )
    {
        return;
    }

    // Delete any extra SendTable copies we've attached to the game DLL's classes, if any.
    SV_TermSendTables( serverGameDLL->GetAllServerClasses() );
    g_pServerPluginHandler->UnloadPlugins();
    serverGameDLL->DLLShutdown();

    UnloadEntityDLLs();

    sv.dll_initialized = false;

    // Shutdown the steam interfaces
    Steam3Server().Shutdown();
}


static bool s_bExitWhenEmpty = false;
static void sv_ShutDownMsg( char const *szReason )
{
	s_bExitWhenEmpty = true;
	
	extern char const *g_szHostStateDelayedMessage;
	g_szHostStateDelayedMessage = szReason;

	if ( sv.IsHibernating() && !sv.IsReserved() )
	{
		HostState_Shutdown();
	}
}
static void sv_ShutDown( void )
{
	if ( sv.IsHibernating() && !sv.IsReserved() )
	{
		sv_ShutDownMsg( "sv_shutdown hibernating server right now." );
	}
	else
	{
		sv_ShutDownMsg( "sv_shutdown live server, delaying request." );
	}
}
bool sv_ShutDown_WasRequested( void )
{
	return s_bExitWhenEmpty;
}

static ConCommand sv_shutdown( "sv_shutdown", sv_ShutDown, "Sets the server to shutdown when all games have completed", FCVAR_RELEASE );




ServerClass* SV_FindServerClass( const char *pName )
{
    ServerClass *pCur = serverGameDLL->GetAllServerClasses();
    while ( pCur )
    {
        if ( Q_stricmp( pCur->GetName(), pName ) == 0 )
            return pCur;

        pCur = pCur->m_pNext;
    }
    
    return NULL;
}

ServerClass* SV_FindServerClass( int index )
{
    ServerClass *pCur = serverGameDLL->GetAllServerClasses();
    int count = 0;

    while ( (count < index) && (pCur != NULL) )
    {
        count++;
        pCur = pCur->m_pNext;
    }

    return pCur;
}

//-----------------------------------------------------------------------------
// Purpose: General initialization of the server
//-----------------------------------------------------------------------------
void CGameServer::Init (bool isDedicated)
{
    BaseClass::Init( isDedicated );

    m_FullSendTables.SetDebugName( "m_FullSendTables" );
    
    dll_initialized = false;

    if ( isDedicated )
    {
        // if dedicated server, hibernate until first client connects
        UpdateHibernationState( );
    }

	// Install signal handlers for the dedicated server
	#ifdef POSIX
		if ( isDedicated )
		{
			signal( SIGINT,
				[](int) -> void
				{
					char const *szMsg;
					if ( sv.IsHibernating() && !sv.IsReserved() )
					{
						szMsg = "SIGINT received, hibernating server, shutting down now.";
					}
					else
					{
						szMsg = "SIGINT received, live server, delaying request.";
					}
					sv_ShutDownMsg( szMsg );
				}
			);
			signal( SIGTERM,
				[](int) -> void
				{
					extern char const *g_szHostStateDelayedMessage;
					g_szHostStateDelayedMessage = "SIGTERM received, forcing immediate shutdown.";
					HostState_Shutdown();
				}
			);
		}
	#endif
}

bool CGameServer::IsPausable( void ) const
{
    if ( IsSinglePlayerGame() )
        return true;

    // In developer mode when there are no remote clients connected
    // allow the server to be pausable too
    if ( !NET_IsDedicated() && sv_pausable_dev.GetBool() && developer.GetInt() && IsPlayingSoloAgainstBots() )
        return true;
    if ( NET_IsDedicated() && sv_pausable_dev_ds.GetBool() && developer.GetInt() && IsPlayingSoloAgainstBots() )
        return true;

    // Normally only single player game can be pausable,
    // but we allow if sv_pausable is set on the server:
    return sv_pausable.GetBool();
}

void CGameServer::Shutdown( void )
{
    m_bIsLevelMainMenuBackground = false;

    if ( IGameEvent *event = g_GameEventManager.CreateEvent( "server_pre_shutdown" ) )
    {
        event->SetString( "reason", "quit" );
        g_GameEventManager.FireEvent( event );
    }

    BaseClass::Shutdown();

    // Actually performs a shutdown.
    framesnapshotmanager->LevelChanged();

    if ( IGameEvent *event = g_GameEventManager.CreateEvent( "server_shutdown" ) )
    {
        event->SetString( "reason", "quit" );
        g_GameEventManager.FireEvent( event );
    }

    // Log_Printf( "Server shutdown.\n" );
    g_Log.Close();
}


/*
==================
SV_StartSound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.  (max 4 attenuation)

Pitch should be PITCH_NORM (100) for no pitch shift. Values over 100 (up to 255)
shift pitch higher, values lower than 100 lower the pitch.
==================
*/
void SV_StartSound ( IRecipientFilter& filter, edict_t *pSoundEmittingEntity, int iChannel, 
    const char *pSoundEntry, HSOUNDSCRIPTHASH iSoundEntryHash, const char *pSample, float flVolume, soundlevel_t iSoundLevel, int iFlags, 
    int iPitch, const Vector *pOrigin, float soundtime, int speakerentity, CUtlVector< Vector >* pUtlVecOrigins, int nSeed )
{

    SoundInfo_t sound; 
    sound.SetDefault();

    sound.nEntityIndex = pSoundEmittingEntity ? NUM_FOR_EDICT( pSoundEmittingEntity ) : 0;
    sound.nChannel = iChannel;
    sound.fVolume = flVolume;
    sound.Soundlevel = iSoundLevel;
    sound.nFlags = iFlags;
    sound.nPitch = iPitch;
    sound.nSpeakerEntity = speakerentity;

    sound.nRandomSeed = nSeed;

    // just for debug spew
    sound.pszName = pSoundEntry;

    if ( iFlags & SND_STOP )
    {
        Assert( filter.IsReliable() );
    }

    // Compute the sound origin
    if ( pOrigin )
    {
        VectorCopy( *pOrigin, sound.vOrigin );
    }
    else if ( pSoundEmittingEntity )
    {
        IServerEntity *serverEntity = pSoundEmittingEntity->GetIServerEntity();
        if ( serverEntity )
        {
            CM_WorldSpaceCenter( serverEntity->GetCollideable(), &sound.vOrigin );
        }
    }

    // Add actual sound origin to vector if requested
    if ( pUtlVecOrigins )
    {
        (*pUtlVecOrigins).AddToTail( sound.vOrigin );
    }

    // set sound delay
    if ( soundtime != 0.0f )
    {
        // add one tick since server time ends at the current tick
        // we'd rather delay sounds slightly than skip the beginning samples
        // so add one tick of latency
        soundtime += sv.GetTickInterval();

        sound.fTickTime = sv.GetFinalTickTime();
        sound.fDelay = soundtime - sv.GetFinalTickTime();
        sound.nFlags |= SND_DELAY;
#if 0
        static float lastSoundTime = 0;
        Msg("SV: [%.3f] Play %s at %.3f\n", soundtime - lastSoundTime, pSample, soundtime );
        lastSoundTime = soundtime;
#endif
    }
    
    // find precache number for sound
    
    // if this is a sentence, get sentence number
    if ( pSample && TestSoundChar(pSample, CHAR_SENTENCE) )
    {
        sound.bIsSentence = true;
        sound.nSoundNum = Q_atoi( PSkipSoundChars(pSample) );
        if ( sound.nSoundNum >= VOX_SentenceCount() )
        {
            ConMsg("SV_StartSound: invalid sentence number: %s", PSkipSoundChars(pSample));
            return;
        }
    }
    else
    {
        sound.bIsSentence = false;
        if( sound.nFlags & SND_IS_SCRIPTHANDLE )
        {
            sound.nSoundNum = iSoundEntryHash;
        }
        else
        {
            sound.nSoundNum = sv.LookupSoundIndex( pSample );
            if ( !sound.nSoundNum || !sv.GetSound( sound.nSoundNum ) )
            {
                ConMsg ("SV_StartSound: %s not precached (%d)\n", pSample, sound.nSoundNum );
                return;
            }
        }
    }

    // now sound message is complete, send to clients in filter
    sv.BroadcastSound( sound, filter );
}

//-----------------------------------------------------------------------------
// Purpose: Sets bits of playerbits based on valid multicast recipients
// Input  : usepas - 
//			origin - 
//			playerbits - 
//-----------------------------------------------------------------------------
void SV_DetermineMulticastRecipients( bool usepas, const Vector& origin, CPlayerBitVec& playerbits )
{
    // determine cluster for origin
    int cluster = CM_LeafCluster( CM_PointLeafnum( origin ) );
    byte pvs[MAX_MAP_LEAFS/8];
    int visType = usepas ? DVIS_PAS : DVIS_PVS;
    const byte *pMask = CM_Vis( pvs, sizeof(pvs), cluster, visType );

    playerbits.ClearAll();

    // Check for relevent clients
    for (int i = 0; i < sv.GetClientCount(); i++ )
    {
        CGameClient *pClient = sv.Client( i );

        if ( !pClient->IsActive() )
            continue;

        // HACK:  Should above also check pClient->spawned instead of this
        if ( !pClient->edict || pClient->edict->IsFree() || pClient->edict->GetUnknown() == NULL )
            continue;
        
        // Always add the HLTV or Replay client
#if defined( REPLAY_ENABLED )
        if ( pClient->IsHLTV() || pClient->IsReplay() )
#else
        if ( pClient->IsHLTV() )
#endif
        {
            playerbits.Set( i );
            continue;
        }

        Vector vecEarPosition;
        serverGameClients->ClientEarPosition( pClient->edict, &vecEarPosition );

        int iBitNumber = CM_LeafCluster( CM_PointLeafnum( vecEarPosition ) );
        if ( !(pMask[iBitNumber>>3] & (1<<(iBitNumber&7)) ) )
            continue;

        if ( pClient->IsSplitScreenUser() )
        {
            playerbits.Set( pClient->m_pAttachedTo->GetPlayerSlot() );
            continue;
        }
        playerbits.Set( i );
    }
}

//-----------------------------------------------------------------------------
// Purpose: Write single ConVar change to all connected clients
// Input  : *var - 
//			*newValue - 
//-----------------------------------------------------------------------------
void SV_ReplicateConVarChange( ConVar const *var, const char *newValue )
{
    Assert( var );
    Assert( var->IsFlagSet( FCVAR_REPLICATED ) );
    Assert( newValue );

    if ( !sv.IsActive() || !sv.IsMultiplayer() )
        return;

    CNETMsg_SetConVar_t cvarMsg( var->GetName(), Host_CleanupConVarStringValue( newValue ) );

    sv.BroadcastMessage( cvarMsg );
}


//-----------------------------------------------------------------------------
// Purpose: Execute a command on all clients or a particular client
// Input  : *var - 
//			*newValue - 
//-----------------------------------------------------------------------------
void SV_ExecuteRemoteCommand( const char *pCommand, int nClientSlot )
{
    if ( !sv.IsActive() || !sv.IsMultiplayer() )
        return;

    CNETMsg_StringCmd_t cmdMsg( pCommand );

    if ( nClientSlot >= 0 )
    {
        CEngineSingleUserFilter filter( nClientSlot + 1, true );
        sv.BroadcastMessage( cmdMsg, filter );
    }
    else
    {
        sv.BroadcastMessage( cmdMsg );
    }
}



/*
==============================================================================

CLIENT SPAWNING

==============================================================================
*/

CGameServer::CGameServer()
{
    m_nMinClientsLimit = 0;
    m_nMaxClientsLimit = 0;
    m_pPureServerWhitelist = NULL;
    m_bHibernating = false;
    m_bLoadedPlugins = false;
	m_bUpdateHibernationStateDeferred = false;
}


CGameServer::~CGameServer()
{
    if ( m_pPureServerWhitelist )
    {
        m_pPureServerWhitelist->Release();
    }
}


//-----------------------------------------------------------------------------
// Purpose: Disconnects the client and cleans out the m_pEnt CBasePlayer container object
// Input  : *clientedict - 
//-----------------------------------------------------------------------------
void CGameServer::RemoveClientFromGame( CBaseClient *client )
{
    CGameClient *cl = (CGameClient*)client;

    // we must have an active server and a spawned client 
    if ( !cl->edict || !cl->IsSpawned() || !IsActive() )
        return;

    Assert( g_pServerPluginHandler );

    g_pServerPluginHandler->ClientDisconnect( cl->edict );
    // release the DLL entity that's attached to this edict, if any
    serverGameEnts->FreeContainingEntity( cl->edict );
}

CBaseClient *CGameServer::CreateNewClient(int slot )
{
    CBaseClient *cl = new CGameClient( slot, this );

    const char *pszValue = NULL;
    if ( cl && CommandLine()->CheckParm( "-netspike", &pszValue ) && 
        pszValue )
    {
        cl->SetTraceThreshold( Q_atoi( pszValue ) );
    }

    return cl;
}



/*
================
SV_FinishCertificateCheck

For LAN connections, make sure we don't have too many people with same cd key hash
For Authenticated net connections, check the certificate and also double check won userid
 from that certificate
================
*/
bool CGameServer::FinishCertificateCheck( netadr_t &adr, int nAuthProtocol, const char *szRawCertificate )
{
    // Now check auth information
    switch ( nAuthProtocol )
    {
    default:
    case PROTOCOL_AUTHCERTIFICATE:
        RejectConnection( adr, "Authentication disabled!!!\n");
        return false;

    case PROTOCOL_STEAM:
        return true; // the SteamAuthServer() state machine checks this
        break;

    case PROTOCOL_HASHEDCDKEY:
        if ( !UseCDKeyAuth() )
        {
            RejectConnection( adr, "#Valve_Reject_CD_Key_Auth_Invalid" );
            return false;
        }

        if ( Q_strlen( szRawCertificate ) != 32 )
        {
            RejectConnection( adr, "#Valve_Reject_Invalid_CD_Key" );
            return false;
        }

        int nHashCount = 0;

        // Now make sure that this hash isn't "overused"
        for ( int i=0; i< GetClientCount(); i++ )
        {
            CBaseClient *cl = Client(i);

            if ( !cl->IsConnected() )
                continue;

            if ( Q_strnicmp ( szRawCertificate, cl->m_GUID, SIGNED_GUID_LEN ) )
                continue;

            nHashCount++;
        }

        if ( nHashCount >= MAX_IDENTICAL_CDKEYS )
        {
            RejectConnection( adr, "#Valve_Reject_CD_Key_In_Use" );
            return false;
        }
        break;
    }

    return true;
}

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/

static int		s_FatBytes;
static byte*	s_pFatPVS = 0;

CUtlVector<int> g_AreasNetworked;
CUtlVector<int> g_ClustersNetworked;

static void SV_AddToFatPVS( int nClusterIndex )
{
    int		i;
    byte	pvs[MAX_MAP_LEAFS/8];

    CM_Vis( pvs, sizeof(pvs), nClusterIndex, DVIS_PVS );
    int nLastInt = s_FatBytes & (~3);
    for ( i = 0; i < nLastInt; i +=4 )
    {
        uint *pOut = (uint *)(s_pFatPVS + i);
        uint *pIn = (uint *)(pvs + i);
        *pOut |= *pIn;
    }
    for (; i<s_FatBytes ; i++)
    {
        s_pFatPVS[i] |= pvs[i];
    }
}

//-----------------------------------------------------------------------------
// Purpose: Zeroes out pvs, this way we can or together multiple pvs's for a player
//-----------------------------------------------------------------------------
void SV_ResetPVS( byte* pvs, int nPVSSize )
{
    s_pFatPVS = pvs;
    s_FatBytes = Bits2Bytes(CM_NumClusters());

    if ( s_FatBytes > nPVSSize )
    {
        Sys_Error( "SV_ResetPVS:  Size %i too big for buffer %i\n", s_FatBytes, nPVSSize );
    }

    Q_memset (s_pFatPVS, 0, s_FatBytes);
    g_ClustersNetworked.RemoveAll();
    g_AreasNetworked.RemoveAll();
}

/*
=============
Calculates a PVS that is the inclusive or of all leafs within 8 pixels of the
given point.
=============
*/
void SV_AddOriginToPVS( const Vector& vOrigin )
{
    int nLeafIndex = CM_PointLeafnum( vOrigin );
    int nClusterIndex = CM_LeafCluster( nLeafIndex );
    // already included this cluster in the PVS?
    if ( g_ClustersNetworked.Find( nClusterIndex ) != -1 )
        return;

    // mark as included
    g_ClustersNetworked.AddToTail( nClusterIndex );
    SV_AddToFatPVS( nClusterIndex );

    int nArea = CM_LeafArea( nLeafIndex );
    if ( g_AreasNetworked.Find( nArea ) != -1 )
        return;
    g_AreasNetworked.AddToTail( nArea );
}

void CGameServer::BroadcastSound( SoundInfo_t &sound, IRecipientFilter &filter )
{
    int num = filter.GetRecipientCount();
    
    // don't add sounds while paused, unless we're in developer mode
    if ( IsPaused() && !developer.GetInt() )
        return;

    for ( int i = 0; i < num; i++ )
    {
        int index = filter.GetRecipientIndex( i );

        if ( index < 1 || index > GetClientCount() )
        {
            Msg( "CGameServer::BroadcastSound:  Recipient Filter for sound (reliable: %s, init: %s) with bogus client index (%i) in list of %i clients\n", 
                    filter.IsReliable() ? "yes" : "no",
                    filter.IsInitMessage() ? "yes" : "no",
                    index, num );

            continue;
        }

        CGameClient *cl = Client( index - 1 );

        // client must be fully connect to hear sounds
        if ( !cl->IsActive() )
        {
            continue;
        }

        cl->SendSound( sound, filter.IsReliable() );
    }
}

bool CGameServer::IsInPureServerMode() const
{
    return (m_pPureServerWhitelist != NULL);
}

CPureServerWhitelist * CGameServer::GetPureServerWhitelist() const
{
    return m_pPureServerWhitelist;
}

void OnHibernateWhenEmptyChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
    // We only need to do something special if we were preventing hibernation
    // with sv_hibernate_when_empty but we would otherwise have been hibernating.
    // In that case, punt all connected clients.
    sv.UpdateHibernationState( ); 
}

bool CGameServer::IsHibernating() const
{
    return m_bHibernating;
}

void Heartbeat_f();



static ConVar sv_memlimit(  "sv_memlimit", "0", FCVAR_RELEASE, 
                            "If set, whenever a game ends, if the total memory used by the server is "
                            "greater than this # of megabytes, the server will exit."	);
static ConVar sv_minuptimelimit(  "sv_minuptimelimit", "0", FCVAR_RELEASE, 
	"If set, whenever a game ends, if the server uptime is less than "
	"this number of hours, the server will continue running regardless of sv_memlimit."	);
static ConVar sv_maxuptimelimit(  "sv_maxuptimelimit", "0", FCVAR_RELEASE, 
	"If set, whenever a game ends, if the server uptime exceeds "
	"this number of hours, the server will exit."	);


#if 0
static void sv_WasteMemory( void )
{
    uint8 *pWastedRam = new uint8[ 100 * 1024 * 1024 ];
    memset( pWastedRam, 0xff, 100 * 1024 * 1024 );			// make sure it gets committed
    Msg( "waste 100mb. using %dMB with an sv_memory_limit of %dMB\n", ApproximateProcessMemoryUsage() / ( 1024 * 1024 ), sv_memlimit.GetInt() );
}

static ConCommand sv_wastememory( "sv_wastememory", sv_WasteMemory, "Causes the server to allocate 100MB of ram and never free it", FCVAR_CHEAT );
#endif




void CGameServer::SetHibernating( bool bHibernating )
{
	static bool s_bPlatFloatTimeInitialized = false;
	static double s_flPlatFloatTimeBeginUptime = 0.0;
	if ( !s_bPlatFloatTimeInitialized )
	{
		s_bPlatFloatTimeInitialized = true;
		s_flPlatFloatTimeBeginUptime = Plat_FloatTime();
	}

    if ( m_bHibernating != bHibernating )
    {
        m_bHibernating = bHibernating;
        Msg( m_bHibernating ? "Server is hibernating\n" : "Server waking up from hibernation\n" );
        if ( m_bHibernating )
        {
			// if we are hibernating we also might want to punt all GOTV clients
			if ( sv_hibernate_punt_tv_clients.GetBool() )
			{
				for ( CHltvServerIterator hltv; hltv; hltv.Next() )
				{
					hltv->Shutdown();
				}
			}
            // see if we have any other connected bot clients
            for ( int iClient = 0; iClient < m_Clients.Count(); iClient++ )
            {			
                CBaseClient *pClient = m_Clients[iClient];
                if ( pClient->IsFakeClient() && pClient->IsConnected() && !pClient->IsSplitScreenUser() )
                {
                    pClient->Disconnect( "Punting bot, server is hibernating" );
                }
            }
            // if we are hibernating, and we want to quit, quit
            bool bExit = false;
            if ( sv_ShutDown_WasRequested() )
            {
                bExit = true;
                Warning( "Server shutting down because sv_shutdown was done and a game has ended.\n" );
            }
            else
            {
				// Also check to see if we're supposed to restart on level change due to being out of date.
				// Catches the cases where the server is out of date on first launch and when players
				// connect and then disconnected without ever changing levels.
				if ( sv.RestartOnLevelChange() )
				{
					bExit = true;
					Warning( "Server is shutting down to update.");
				}

                if ( sv_memlimit.GetInt() )
                {
                    if ( ApproximateProcessMemoryUsage() > 1024 * 1024 * sv_memlimit.GetInt() )
                    {
						if ( ( sv_minuptimelimit.GetFloat() > 0 ) &&
							( ( Plat_FloatTime() - s_flPlatFloatTimeBeginUptime ) / 3600.0 < sv_minuptimelimit.GetFloat() ) )
						{
							Warning( "Server is using %dMB with an sv_memory_limit of %dMB, but will not shutdown because sv_minuptimelimit is %.3f hr while current uptime is %.3f\n",
								ApproximateProcessMemoryUsage() / ( 1024 * 1024 ), sv_memlimit.GetInt(),
								sv_minuptimelimit.GetFloat(), ( Plat_FloatTime() - s_flPlatFloatTimeBeginUptime ) / 3600.0 );
						}
						else
						{
							Warning( "Server shutting down because of using %dMB with an sv_memory_limit of %dMB\n", ApproximateProcessMemoryUsage() / ( 1024 * 1024 ), sv_memlimit.GetInt() );
							bExit = true;
						}
                    }
                }

				if ( ( sv_maxuptimelimit.GetFloat() > 0 ) &&
					( ( Plat_FloatTime() - s_flPlatFloatTimeBeginUptime ) / 3600.0 > sv_maxuptimelimit.GetFloat() ) )
				{
					Warning( "Server will shutdown because sv_maxuptimelimit is %.3f hr while current uptime is %.3f, using %dMB with an sv_memory_limit of %dMB\n",
						sv_maxuptimelimit.GetFloat(), ( Plat_FloatTime() - s_flPlatFloatTimeBeginUptime ) / 3600.0,
						ApproximateProcessMemoryUsage() / ( 1024 * 1024 ), sv_memlimit.GetInt() );
					bExit = true;
				}
            }
            
#ifdef _LINUX
            // if we are a child process running forked, we want to exit now. We want to "really" exit. no destructors, no nothing
            if ( IsChildProcess() )							// are we a subprocess?
            {
                syscall( SYS_exit, 0 );	// we are not going to perform a normal c++ exit. We _dont_ want to run destructors, etc.
            }
#endif
            if ( bExit )
            {
                HostState_Shutdown();
            }
            ResetGameConVarsToDefaults();

            // Reset gametype based on map
            ExecGameTypeCfg( m_szMapname );

            SetReservationCookie( 0u, "SetHibernating(true)" );
            m_flReservationExpiryTime = 0.0f;
        }

        UpdateGameData();

        // Force a heartbeat to update the master servers
        Heartbeat_f();

		if ( serverGameDLL )
			serverGameDLL->ServerHibernationUpdate( m_bHibernating );

		SV_SetSteamCrashComment();
    }
}

void CGameServer::UpdateReservedState()
{
	if ( m_bUpdateHibernationStateDeferred )
	{
		m_bUpdateHibernationStateDeferred = false;
		UpdateHibernationState();
	}

	CBaseServer::UpdateReservedState();
}

void CGameServer::UpdateHibernationStateDeferred()
{
	m_bUpdateHibernationStateDeferred = true;
}


void CGameServer::UpdateHibernationState()
{
	if ( !IsDedicated() )
		return;

	// is this the last client disconnecting?
	bool bHaveAnyClients = false;

	// see if we have any other connected clients
	for ( int iClient = 0; iClient < m_Clients.Count(); iClient++ )
	{
		CBaseClient *pClient = m_Clients[ iClient ];
		// don't consider the client being removed, it still shows as connected but won't be in a moment
		if ( pClient->IsConnected() && ( pClient->IsSplitScreenUser() || !pClient->IsFakeClient() ) )
		{
			bHaveAnyClients = true;
			break;
		}
	}

	float flMaxDirectorDelay = 0.0f;
	// if we are a relay that has an active reservation then hold off from hibernating
	// as long as we are connected
	for ( CActiveHltvServerIterator hltv; hltv; hltv.Next() )
	{
		if ( hltv->IsTVRelay() )
		{
			bHaveAnyClients = true;
		}
		else
		{
			if ( ( hltv->m_nGlobalClients > 0 ) || hltv->m_Broadcast.IsRecording() )
			{
				flMaxDirectorDelay = Max( flMaxDirectorDelay, hltv->GetDirector()->GetDelay() );
			}
		}
	}

    bool bSufficientTimeWithoutClients = false;

    if ( bHaveAnyClients )
    {
        // Clear timer
        m_flTimeLastClientLeft = -1.0f;
    }
    else
    {
        if ( IsReserved() )
        {
            if ( m_flTimeLastClientLeft == -1.0f )
            {
                // Start timer
                m_flTimeLastClientLeft = Plat_FloatTime();
            }

            // Check timer
            float flElapsed = Plat_FloatTime() - m_flTimeLastClientLeft;
			
			// If we have connected TV clients then reduce the elapsed time by tv delay
			flElapsed -= flMaxDirectorDelay;

            if ( flElapsed > sv_hibernate_postgame_delay.GetFloat() )
            {
                // Act like we still have some clients
                bSufficientTimeWithoutClients = true;

				// If game server requests to hold reservation for longer then don't unreserve
				if ( serverGameDLL && serverGameDLL->ShouldHoldGameServerReservation( flElapsed ) )
					bSufficientTimeWithoutClients = false;
            }
        }
    }

    // If we had some clients and now don't, unreserve right away.
    if ( IsReserved() && 
        !bHaveAnyClients &&
        bSufficientTimeWithoutClients &&
        ( m_flReservationExpiryTime == 0.0f || m_flReservationExpiryTime < net_time ) )
    {
        SetReservationCookie( 0ull, "reserved(%s), clients(%s), reservationexpires(%.2f)",
            IsReserved() ? "yes" : "no", bHaveAnyClients ? "yes" : "no", m_flReservationExpiryTime );

		//
		// Automatically stop recording on Valve official servers
		// when the server gets unreserved
		//
		if ( serverGameDLL && serverGameDLL->IsValveDS() )
		{
			for ( CHltvServerIterator hltv; hltv; hltv.Next() )
			{
				if ( hltv->IsRecording() )
				{
					hltv->StopRecording( );
				}
			}
		}
    }
    
    SetHibernating( sv_hibernate_when_empty.GetBool() && !IsReserved() && !bHaveAnyClients );
}

void CGameServer::FinishRestore()
{
#ifndef DEDICATED
    CSaveRestoreData currentLevelData;
    char			name[MAX_OSPATH];

    if ( !m_bLoadgame )
        return;

    g_ServerGlobalVariables.pSaveData = &currentLevelData;
    // Build the adjacent map list
    serverGameDLL->BuildAdjacentMapList();

    if ( !saverestore->IsXSave() )
    {
        Q_snprintf( name, sizeof( name ), "%s%s.HL2", saverestore->GetSaveDir(), m_szMapname );
    }
    else
    {
        Q_snprintf( name, sizeof( name ), "%s:\\%s.HL2", GetCurrentMod(), m_szMapname );
    }

    Q_FixSlashes( name );

    saverestore->RestoreClientState( name, false );

    if ( g_ServerGlobalVariables.eLoadType == MapLoad_Transition )
    {
        for ( int i = 0; i < currentLevelData.levelInfo.connectionCount; i++ )
        {
            saverestore->RestoreAdjacenClientState( currentLevelData.levelInfo.levelList[i].mapName );
        }
    }

    saverestore->OnFinishedClientRestore();

    g_ServerGlobalVariables.pSaveData = NULL;

    // Reset
    m_bLoadgame = false;
    saverestore->SetIsXSave( IsX360() );
#endif
}

void CGameServer::CopyTempEntities( CFrameSnapshot* pSnapshot )	
{
    Assert( pSnapshot->m_pTempEntities == NULL );

    if ( m_TempEntities.Count() > 0 )
    {
        // copy temp entities if any
        pSnapshot->m_nTempEntities = m_TempEntities.Count();

        pSnapshot->m_pTempEntities = new CEventInfo*[pSnapshot->m_nTempEntities];

        Q_memcpy( pSnapshot->m_pTempEntities, m_TempEntities.Base(), m_TempEntities.Count() * sizeof( CEventInfo * ) );

        // clear server list
        m_TempEntities.RemoveAll();
    }
}

static ConVar sv_parallel_sendsnapshot( "sv_parallel_sendsnapshot", 
#ifndef DEDICATED
										"1", 
#else //DEDICATED
										"0", 
#endif //DEDICATED
										FCVAR_RELEASE );

void SV_ParallelSendSnapshot( CGameClient *& pClient )
{
    CClientFrame *pFrame = pClient->GetSendFrame();
    if ( !pFrame )
        return;
    pClient->SendSnapshot( pFrame );
    pClient->UpdateSendState();
}

void CGameServer::SendClientMessages ( bool bSendSnapshots )
{
    VPROF_BUDGET( "SendClientMessages", VPROF_BUDGETGROUP_OTHER_NETWORKING );

    // build individual updates
    int receivingClientCount = 0;
    CGameClient*	pReceivingClients[ABSOLUTE_PLAYER_LIMIT];
	bool bHLTVOnly = true; // true when there's no HLTV; which will mean there's no IsHLTV clients, so it'll get reset
	int nHltvMaxAckCount = -1;
	for ( CHltvServerIterator hltv; hltv; hltv.Next() )
	{
		if ( hltv->m_MasterClient )
		{
			nHltvMaxAckCount = Max( hltv->m_MasterClient->GetMaxAckTickCount(), nHltvMaxAckCount );
		}
		if ( hltv->m_MasterClient && ( hltv->GetClientCount() != 0 ) || hltv->IsRecording() )
		{
			bHLTVOnly = false;	// we have clients connected to this HLTV server, keep sending snapshots
		}
	}

    for (int i=0; i< GetClientCount(); i++ )
    {

        CGameClient* client = Client(i);
        
        // Update Host client send state...
        if ( !client->ShouldSendMessages() )
        {
            // For split screen users, adds this into parent stream
            // This works since this user is always "ready" to receive data since they don't
            // exist while the parent entity isn't good for receiving data
            if ( client->IsSplitScreenUser() )
            {
                client->WriteViewAngleUpdate();
            }
            continue;
        }
        
		client->StepHltvReplayStatus( m_nTickCount );

        // Append the unreliable data (player updates and packet entities)
        if ( bSendSnapshots && client->IsActive() )
        {
            // Add this client to the list of clients we're gonna send to.
            pReceivingClients[receivingClientCount] = client;
            ++receivingClientCount;
			bHLTVOnly = bHLTVOnly && client->IsHLTV() && client->m_pLastSnapshot.IsValid();
        }
        else
        {
            // Connected, but inactive, just send reliable, sequenced info.
            if ( client->IsFakeClient() )
                continue;
                
            // if client never send a netchannl packet yet, send S2C_CONNECTION 
            // because it could get lost in multiplayer
            if ( NET_IsMultiplayer() && client->m_NetChannel->GetSequenceNr(FLOW_INCOMING) == 0 )
            {
                NET_OutOfBandPrintf ( m_Socket, client->m_NetChannel->GetRemoteAddress(), "%c00000000000000", S2C_CONNECTION );
            }

#ifdef SHARED_NET_STRING_TABLES
            sv.m_StringTables->TriggerCallbacks( client->m_nDeltaTick );
#endif

            client->m_NetChannel->Transmit();
            client->UpdateSendState();
        }
    }

    if ( receivingClientCount )
    {
		// Don't send a snapshot if there is only 1 client and it is the HLTV client!
		if ( !bHLTVOnly )
		{
			// if any client wants an update, take new snapshot now
			CFrameSnapshot* pSnapshot = framesnapshotmanager->TakeTickSnapshot(
#ifdef DEBUG_SNAPSHOT_REFERENCES
				"CGameServer::SendClientMessages",
#endif
				m_nTickCount );

			// copy temp ents references to pSnapshot
			CopyTempEntities( pSnapshot );

			// Compute the client packs
			SV_ComputeClientPacks( receivingClientCount, pReceivingClients, pSnapshot );

#ifndef SHARED_NET_STRING_TABLES
			if ( nHltvMaxAckCount >= 0 )
			{// copy string updates from server to hltv stringtable
				networkStringTableContainerServer->DirectUpdate( nHltvMaxAckCount ); // !!!! WARNING: THIS IS NOT THREAD SAFE! MEMORY CORRUPTION GUARANTEED WITH MULTIPLE HLTV SERVERS!
			}
#endif

			if ( receivingClientCount > 1 && sv_parallel_sendsnapshot.GetBool() )
			{
				VPROF_BUDGET( "SendSnapshots(Parallel)", VPROF_BUDGETGROUP_OTHER_NETWORKING );

				ParallelProcess( pReceivingClients, receivingClientCount, &SV_ParallelSendSnapshot );
			}
			else
			{
				VPROF_BUDGET( "SendSnapshots", VPROF_BUDGETGROUP_OTHER_NETWORKING );

				for (int i = 0; i < receivingClientCount; ++i)
				{
					CGameClient *pClient = pReceivingClients[i];
					CClientFrame *pFrame = pClient->GetSendFrame();

					if ( !pFrame )
						continue;

					pClient->SendSnapshot( pFrame );
					pClient->UpdateSendState();

				}
			}

			pSnapshot->ReleaseReference();
		}
    }

    // Allow game .dll to run code, including unsetting EF_MUZZLEFLASH and EF_NOINTERP on effects fields
    // etc.
    serverGameClients->PostClientMessagesSent();
}


bool CGameServer::AnyClientsInHltvReplayMode()
{
	for ( int i = 0; i < GetClientCount(); i++ )
	{
		CGameClient* client = Client( i );
		if ( client->GetHltvReplayDelay() )
		{																																
			Assert( !client->IsFakeClient() ); // fake clients should never be in hltv replay mode; it's technically possible, but logically doesn't make sense and has no use case
			return true;
		}
	}
	return false;
}

void CGameServer::SetMaxClients( int number )
{
    m_nMaxclients = clamp( number, m_nMinClientsLimit, m_nMaxClientsLimit );

    ConMsg( "maxplayers set to %i\n", m_nMaxclients );

    deathmatch.SetValue( m_nMaxclients > 1 );
}

//-----------------------------------------------------------------------------
// A potential optimization of the client data sending; the optimization
// is based around the fact that we think that we're spending all our time in
// cache misses since we are accessing so much memory
//-----------------------------------------------------------------------------





/*
==============================================================================
SERVER SPAWNING

==============================================================================
*/

void SV_WriteVoiceCodec(bf_write &pBuf)
{
	CSVCMsg_VoiceInit_t voiceinit;
	voiceinit.set_codec( sv.IsMultiplayer() ? sv_voicecodec.GetString() : "" );
	voiceinit.set_quality( 5 );
	voiceinit.set_version( VOICE_CURRENT_VERSION );
	voiceinit.WriteToBuffer( pBuf );
}

// Gets voice data from a client and forwards it to anyone who can hear this client.
ConVar voice_debugfeedbackfrom( "voice_debugfeedbackfrom", "0" );

void SV_BroadcastVoiceData(IClient * cl, const CCLCMsg_VoiceData& msg )
{
    ConVarRef voice_verbose( "voice_verbose" );

    // Disable voice?
    if( !sv_voiceenable.GetInt() )
    {
        if ( voice_verbose.GetBool() )
        {
            Msg( "* SV_BroadcastVoiceData:  Dropping all voice.  sv_voiceenable is not set.\n" );
        }
        return;
    }

    // Build voice message once
	CSVCMsg_VoiceData_t voiceData;
	voiceData.set_client( cl->GetPlayerSlot() );
	voiceData.set_voice_data( msg.data().c_str(), msg.data().size() );
	if ( msg.xuid() )
	{
		voiceData.set_xuid( msg.xuid() );
	}
	voiceData.set_format( msg.format() );
	voiceData.set_sequence_bytes( msg.sequence_bytes() );
	voiceData.set_section_number( msg.section_number() );
	voiceData.set_uncompressed_sample_offset( msg.uncompressed_sample_offset() );

	if ( voice_debugfeedbackfrom.GetBool() )
	{
		Msg( "Sending voice from: %s - playerslot: %d [ xuid %llx ]\n", cl->GetClientName(), cl->GetPlayerSlot() + 1, msg.xuid() );
	}

    for(int i=0; i < sv.GetClientCount(); i++)
    {
        CBaseClient *pDestClient = static_cast< CBaseClient * >( sv.GetClient(i) );

        bool bSelf = (pDestClient == cl);

        // Only send voice to active clients
        if( !pDestClient->IsActive() )
        {
            if ( voice_verbose.GetBool() )
            {
                Msg( "* SV_BroadcastVoiceData:  Not active (SignonState %d).  Dropping %d bytes from %s (%s) to %s (%s)\n", 
					((CBaseClient*)pDestClient)->GetSignonState(), voiceData.voice_data().size(), 
					cl->GetClientName(), 
					cl->GetNetChannel() ? cl->GetNetChannel()->GetAddress() : "null", 
					pDestClient->GetClientName(), pDestClient->GetNetChannel() ? pDestClient->GetNetChannel()->GetAddress() : "null" );
            }
            continue;
        }

        // We'll check these guys later when we're on the host of them
        if ( pDestClient->IsSplitScreenUser() )
        {
            continue;
        }

        // Does the game code want cl sending to this client?
		bool bHearsPlayer = pDestClient->IsHearingClient( voiceData.client() );
		voiceData.set_audible_mask(bHearsPlayer);
		voiceData.set_proximity( pDestClient->IsProximityHearingClient( voiceData.client() ) );

		// If any of the parasites of the host can hear it, send it to the host
		for ( int i = 1; i < ARRAYSIZE( pDestClient->m_SplitScreenUsers ); ++i )
		{
			voiceData.set_audible_mask( voiceData.audible_mask() | ( i << 1) );
			CBaseClient *splitUser = pDestClient->m_SplitScreenUsers[ i ];
			if ( splitUser )
			{
				// Set which splitscreen players can hear this voice packet
				bool bSplitUserHearsPlayer = splitUser->IsHearingClient( voiceData.client() );

				bHearsPlayer |= bSplitUserHearsPlayer;
				voiceData.set_audible_mask( voiceData.audible_mask() | ( i << 1) );
				if ( splitUser->IsProximityHearingClient( voiceData.client() ) )
				{
					voiceData.set_proximity( true );
				}
			}
		}

        if ( IsGameConsole() && bSelf == true )	
        {
            if ( voice_verbose.GetBool() )
            {
                Msg( "* SV_BroadcastVoiceData:  Self.  Dropping %d bytes from %s (%s) to %s (%s)\n", 
					voiceData.voice_data().size(), cl->GetClientName(), 
					cl->GetNetChannel() ? cl->GetNetChannel()->GetAddress() : "null", 
					pDestClient->GetClientName(), pDestClient->GetNetChannel() ? pDestClient->GetNetChannel()->GetAddress() : "null" );
            }
            continue;
        }
            
        if ( !bHearsPlayer && !bSelf )
        {
            if ( voice_verbose.GetBool() )
            {
                Msg( "* SV_BroadcastVoiceData:  Doesn't hear player.  Dropping %d bytes from %s (%s) to %s (%s)\n", 
					voiceData.voice_data().size(), cl->GetClientName(), 
					cl->GetNetChannel() ? cl->GetNetChannel()->GetAddress() : "null", 
					pDestClient->GetClientName(), pDestClient->GetNetChannel() ? pDestClient->GetNetChannel()->GetAddress() : "null" );
            }
            continue;
        }
		
		// Is loopback enabled?
		if( !bHearsPlayer )
		{
			// Still send something, just zero length (this is so the client 
			// can display something that shows knows the server knows it's talking).
			CSVCMsg_VoiceData_t emptyVoiceMsg;
			emptyVoiceMsg.set_client( voiceData.client() );
			emptyVoiceMsg.set_audible_mask( voiceData.audible_mask() );
			emptyVoiceMsg.set_proximity( voiceData.proximity() );
			if ( voiceData.has_xuid())
			{
				emptyVoiceMsg.set_xuid( voiceData.xuid() );
			}

			pDestClient->SendNetMsg( emptyVoiceMsg, false, true );
		}
		else
		{
			pDestClient->SendNetMsg( voiceData, false, true );
		}

        if ( voice_verbose.GetBool() )
        {
            Msg( "* SV_BroadcastVoiceData: Sending %d bits (%d bytes) from %s (%s) to %s (%s).  Proximity %s.\n", voiceData.voice_data().size(), Bits2Bytes(voiceData.voice_data().size()), cl->GetClientName(), cl->GetNetChannel() ? cl->GetNetChannel()->GetAddress() : "null", pDestClient->GetClientName(), pDestClient->GetNetChannel() ? pDestClient->GetNetChannel()->GetAddress() : "null", voiceData.proximity() ? "true" : "false" );
        }
    }
}


// UNDONE: "player.mdl" ???  This should be set by name in the DLL
/*
================
SV_CreateBaseline

================
*/
void SV_CreateBaseline (void)
{
    SV_WriteVoiceCodec( sv.m_Signon );

    ServerClass *pClasses = serverGameDLL->GetAllServerClasses();

    // Send SendTable info.
    if ( sv_sendtables.GetInt() )
    {
        sv.m_FullSendTablesBuffer.EnsureCapacity( NET_MAX_PAYLOAD );
        sv.m_FullSendTables.StartWriting( sv.m_FullSendTablesBuffer.Base(), sv.m_FullSendTablesBuffer.Count() );
        
        SV_WriteSendTables( pClasses, sv.m_FullSendTables );
        
        if ( sv.m_FullSendTables.IsOverflowed() )
        {
            Host_Error("SV_CreateBaseline: WriteSendTables overflow.\n" );
            return;
        }

        // Send class descriptions.
        SV_WriteClassInfos(pClasses, sv.m_FullSendTables);

        if ( sv.m_FullSendTables.IsOverflowed() )
        {
            Host_Error("SV_CreateBaseline: WriteClassInfos overflow.\n" );
            return;
        }
    }

    SerializedEntityHandle_t handle = g_pSerializedEntities->AllocateSerializedEntity(__FILE__, __LINE__);

    // If we're using the local network backdoor, we'll never use the instance baselines.
    if ( !g_pLocalNetworkBackdoor )
    {
        int		count = 0;
        int		bytes = 0;
        
        for ( int entnum = 0; entnum < sv.num_edicts ; entnum++)
        {
            // get the current server version
            edict_t *edict = sv.edicts + entnum;

            if ( edict->IsFree() || !edict->GetUnknown() )
                continue;

            ServerClass *pClass   = edict->GetNetworkable() ? edict->GetNetworkable()->GetServerClass() : 0;

            if ( !pClass )
            {
                Assert( pClass );
                continue;	// no Class ?
            }

            if ( pClass->m_InstanceBaselineIndex != INVALID_STRING_INDEX )
                continue; // we already have a baseline for this class

            SendTable *pSendTable = pClass->m_pTable;

            //
            // create entity baseline
            //
            
            char packedData[MAX_PACKEDENTITY_DATA];
            bf_write writeBuf( "SV_CreateBaseline->writeBuf", packedData, sizeof( packedData ) );


            // create basline from zero values
            if ( !SendTable_Encode(
                pSendTable, 
                handle,
                edict->GetUnknown(), 
                entnum,
                NULL
                ) )
            {
                Host_Error("SV_CreateBaseline: SendTable_Encode returned false (ent %d).\n", entnum);
            }

            // copy baseline into baseline stringtable
            SV_EnsureInstanceBaseline( pClass, entnum, handle );

            CSerializedEntity *pEntity = ( CSerializedEntity * )handle;

            bytes += Bits2Bytes( pEntity->GetFieldDataBitCount() );
            count ++;
        }
        DevMsg("Created class baseline: %i classes, %i bytes.\n", count,bytes); 
    }
    g_pSerializedEntities->ReleaseSerializedEntity( handle );

    g_GameEventManager.ReloadEventDefinitions();

	CSVCMsg_GameEventList_t gameevents;
	g_GameEventManager.WriteEventList( &gameevents );
	gameevents.WriteToBuffer( sv.m_Signon );
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  : runPhysics -
//-----------------------------------------------------------------------------
bool SV_ActivateServer()
{
    COM_TimestampedLog( "SV_ActivateServer" );
#ifndef DEDICATED
    EngineVGui()->UpdateProgressBar(PROGRESS_ACTIVATESERVER);
#endif

    COM_TimestampedLog( "serverGameDLL->ServerActivate" );

    bool bPrevState = networkStringTableContainerServer->Lock( false );
    // Activate the DLL server code
    g_pServerPluginHandler->ServerActivate( sv.edicts, sv.num_edicts, sv.GetMaxClients() );

    // all setup is completed, any further precache statements are errors
    sv.m_State = ss_active;
    
    COM_TimestampedLog( "SV_CreateBaseline" );

    // create a baseline for more efficient communications
    SV_CreateBaseline();

    sv.allowsignonwrites = false;

    // set skybox name
    ConVar const *skyname = g_pCVar->FindVar( "sv_skyname" );

    if ( skyname )
    {
        Q_strncpy( sv.m_szSkyname, skyname->GetString(), sizeof( sv.m_szSkyname ) );
    }
    else
    {
        Q_strncpy( sv.m_szSkyname, "unknown", sizeof( sv.m_szSkyname ) );
    }

    COM_TimestampedLog( "Send Reconnects" );

    // Tell connected clients to reconnect
    sv.ReconnectClients();

    // Tell what kind of server has been started.
    if ( sv.IsMultiplayer() )
    {
        ConDMsg ("%i player server started\n", sv.GetMaxClients() );
    }
    else
    {
        ConDMsg ("Game started\n");
    }

    // Replay setup
#if defined( REPLAY_ENABLED )
    if ( !g_pServerReplayHistoryManager )
    {
        g_pServerReplayHistoryManager = CreateServerReplayHistoryManager();
        g_pServerReplayHistoryManager->Init();
    }

    if ( Replay_IsEnabled() )
    {
        if ( CommandLine()->FindParm("-noreplay") )
        {
            // let user know that Replay will not work
            ConMsg ("Replay is disabled on this server.\n");
        }
        else
        {
            if ( !replay )
            {
                replay = new CReplayServer;
                replay->Init( NET_IsDedicated() );
            }

#if !defined( NO_STEAM )
            Steam3Server().UpdateSpectatorPort( NET_GetUDPPort( NS_REPLAY ) );
#endif

            if ( replay->IsActive() )
            {
                // replay master already running, just activate client
                replay->m_MasterClient->ActivatePlayer();
                replay->StartMaster( replay->m_MasterClient );
            }
            else
            {
                // create new replay client
                CGameClient *cl = (CGameClient*)sv.CreateFakeClient( "Replay" );
                replay->StartMaster( cl );
            }
        }
    }
    else
    {

        // make sure replay is disabled
        if ( replay )
            replay->Shutdown();
    }
#endif

    if (sv.IsDedicated())
    {
        // purge unused models and their data hierarchy (materials, shaders, etc)
        modelloader->PurgeUnusedModels();
		g_pMDLCache->UnloadQueuedHardwareData(); // need to do this to properly remove the data associated with purged models (on the client this is called by materialsystem, but not on the dedicated server)
    }

	// Steam is required for proper hltv server startup if the server is broadcasting in http
    if ( sv.IsMultiplayer() || serverGameDLL->ShouldPreferSteamAuth() )
    {
        // We always need to activate the Steam3Server
        // it will have different auth modes for SP and MP
        Steam3Server().Activate();
        sv.SetQueryPortFromSteamServer();
        sv.UpdateGameData();	// Set server tags after server creation
        if ( serverGameDLL )
        {
            serverGameDLL->GameServerSteamAPIActivated( true );
        }
    }

	// HLTV setup
	for ( int nHltvServerIndex = 0; nHltvServerIndex < HLTV_SERVER_MAX_COUNT; ++nHltvServerIndex )
	{
		CHLTVServer *&hltv = g_pHltvServer[ nHltvServerIndex ];
		const ConVar &cvEnable = GetIndexedConVar( tv_enable, nHltvServerIndex );
		if ( cvEnable.GetBool() )
		{
			if ( CommandLine()->FindParm( "-nohltv" ) )
			{
				// let user know that SourceTV will not work
				ConMsg( "GOTV is disabled on this server.\n" );
			}
			else if ( nHltvServerIndex > 0 && !CommandLine()->FindParm( "-addhltv1" ) )
			{
				// default behavior - GOTV[1] disabled - but tv_enable* is ON
				ConMsg( "GOTV[%d] must be explicitly enabled (with -addhltv1) on this server. tv_enable%d 0 to hide this message\n", nHltvServerIndex, nHltvServerIndex );
			}
			else if ( !sv.IsDedicated() || sv.IsReserved() || ( cvEnable.GetInt() != 2 ) )
			{
				// create SourceTV object if not already there
				if ( !hltv )
				{
					extern ConVar tv_snapshotrate;
					hltv = new CHLTVServer( nHltvServerIndex, GetIndexedConVar( tv_snapshotrate, nHltvServerIndex ).GetFloat() );
					hltv->Init( NET_IsDedicated() );
				}

				if ( hltv->IsActive() && hltv->IsMasterProxy() )
				{
					// HLTV master already running, just activate client
					hltv->m_MasterClient->ActivatePlayer();
					hltv->StartMaster( hltv->m_MasterClient );
				}
				else
				{
					// create new HLTV client
					CGameClient *cl = ( CGameClient* )sv.CreateFakeClient( "GOTV" );
					hltv->StartMaster( cl );
				}
			}
		}
		else
		{

			// make sure HLTV is disabled
			if ( hltv )
				hltv->Shutdown();
		}
	}
    networkStringTableContainerServer->Lock( bPrevState );

    // Heartbeat the master server in case we turned SrcTV on or off.
    Steam3Server().SendUpdatedServerDetails();
	#if !defined( NO_STEAM )
	{
        if ( Steam3Server().SteamGameServer() )
            Steam3Server().SteamGameServer()->ForceHeartbeat();
	}
	#endif

	if ( serverGameDLL && Steam3Server().GetGSSteamID().IsValid() )
		serverGameDLL->UpdateGCInformation();

    COM_TimestampedLog( "SV_ActivateServer(finished)" );

    return true;
}

#include "tier0/memdbgoff.h"

static void SV_AllocateEdicts()
{
    sv.edicts = (edict_t *)Hunk_AllocName( sv.max_edicts*sizeof(edict_t), "edicts" );

    // Invoke the constructor so the vtable is set correctly..
    for (int i = 0; i < sv.max_edicts; ++i)
    {
        new( &sv.edicts[i] ) edict_t;
    }
    ED_ClearTimes();

    sv.edictchangeinfo = (IChangeInfoAccessor *)Hunk_AllocName( sv.max_edicts * sizeof( IChangeInfoAccessor ), "edictchangeinfo" );
}

#include "tier0/memdbgon.h"


void CGameServer::ReloadWhitelist( const char *pMapName )
{
	// listen servers should not ever run sv_pure
	if ( !sv.IsDedicated() )
	{
		g_sv_pure_mode = 0;
	}

    // Always return - until we get the whilelist stuff resolved for TF2.
    if ( m_pPureServerWhitelist )
    {
        m_pPureServerWhitelist->Release();
        m_pPureServerWhitelist = NULL;
    }

    // Don't do sv_pure stuff in SP games.
    if ( GetMaxClients() <= 1 )
        return;

    // Get rid of the old whitelist.
    if ( m_pPureServerWhitelist )
    {
        m_pPureServerWhitelist->Release();
        m_pPureServerWhitelist = NULL;
    }

    // Don't use the whitelist if sv_pure is not set.
    if ( g_sv_pure_mode == 0 )
        return;

    m_pPureServerWhitelist = CPureServerWhitelist::Create( g_pFileSystem );
    if ( g_sv_pure_mode == 2 )
    {
        // sv_pure 2 means to ignore the pure_server_whitelist.txt file and force everything to come from Steam.
        m_pPureServerWhitelist->EnableFullyPureMode();
        Msg( "Server using sv_pure 2.\n" );
    }
    else
    {
        const char *pGlobalWhitelistFilename = "pure_server_whitelist.txt";
        const char *pMapWhitelistSuffix = "_whitelist.txt";
        
        // Load the new whitelist.
        KeyValues *kv = new KeyValues( "" );
        bool bLoaded = kv->LoadFromFile( g_pFileSystem, pGlobalWhitelistFilename, "game" );
        if ( bLoaded )
            bLoaded = m_pPureServerWhitelist->LoadFromKeyValues( kv );
        
        if ( !bLoaded )
            Warning( "Can't load pure server whitelist in %s.\n", pGlobalWhitelistFilename );
        
        // Load the per-map whitelist.
        char testFilename[MAX_PATH] = "maps";
        V_AppendSlash( testFilename, sizeof( testFilename ) );
        V_strncat( testFilename, pMapName, sizeof( testFilename ) );
        V_strncat( testFilename, pMapWhitelistSuffix, sizeof( testFilename ) );
        
        kv->Clear();
        if ( kv->LoadFromFile( g_pFileSystem, testFilename ) )
            m_pPureServerWhitelist->LoadFromKeyValues( kv );

        kv->deleteThis();
    }

}


//-----------------------------------------------------------------------------
// Update the game type based on map name, only used when user types "map"
// we are we hibernating (since someone could join our currently running
// map).
void CGameServer::ExecGameTypeCfg( const char *mapname )
{
    MEM_ALLOC_CREDIT();
    KeyValues *pGameSettings = new KeyValues( "::ExecGameTypeCfg" );
    KeyValues::AutoDelete autodelete( pGameSettings );

    pGameSettings->SetString( "map/mapname", mapname );

	if ( serverGameDLL )
		serverGameDLL->ApplyGameSettings( pGameSettings );

    int numSlots = pGameSettings->GetInt( "members/numSlots", -1 );

#if defined (_GAMECONSOLE) && defined ( CSTRIKE15 )
    // FIXME(hpe) sb: temp: quick hack for splitscreen; NOTE: SetMaxClients required since integration (taken from PORTAL2 below)
    ConVarRef ss_enable( "ss_enable" );
    if ( ss_enable.GetInt() > 0 )
    {
        numSlots = 9;
        SetMaxClients( numSlots );
    }
#endif

    if ( numSlots >= 0 )
    {
        m_numGameSlots = numSlots;
#ifdef PORTAL2	// HACK: PORTAL2 uses maxclients instead of GAMERULES
        SetMaxClients( numSlots );
#endif
    }
}


void CGameServer::SetMapGroupName( char const *mapGroupName )
{
    if ( mapGroupName && mapGroupName[0] )
    {
        V_strncpy( m_szMapGroupName, mapGroupName, sizeof( m_szMapGroupName ) );
    }

    g_ServerGlobalVariables.mapGroupName = MAKE_STRING( m_szMapGroupName );
}

/*
================
SV_SpawnServer

This is called at the start of each level
================
*/
bool CGameServer::SpawnServer( char *mapname, char * mapGroupName, char *startspot )
{
    int		i;
    char	szDllName[MAX_QPATH];

    Assert( serverGameClients );

	SV_SetSteamCrashComment();

    if ( CommandLine()->FindParm( "-NoLoadPluginsForClient" ) != 0 )
    {
        if ( !m_bLoadedPlugins )
        {
            // Only load plugins once.
            m_bLoadedPlugins = true;
            g_pServerPluginHandler->LoadPlugins(); // load 3rd party plugins
        }
    }

    if ( IsGameConsole() && g_pQueuedLoader->IsMapLoading() )
    {
        Msg( "Spawning a new server - loading map %s. Forcing current map load to end.\n", mapname );
        g_pQueuedLoader->EndMapLoading( true );
    }

//	NOTE[pmf]: Removed this. We don't want to limit the server fps below what our desired tick rate is; apparently 
//	this restriction was only put in because of people selling dedicated server hosting and offering 500+fps servers, 
//	which really does nothing other than waste extra cycles doing additional iterations of the main loop
//  	if ( IsDedicated() )
//  	{
//  		fps_max.SetValue( 60 );
//  	}

    ReloadWhitelist( mapname );

    COM_TimestampedLog( "SV_SpawnServer(%s)", mapname );
#ifndef DEDICATED
    EngineVGui()->UpdateProgressBar(PROGRESS_SPAWNSERVER);
#endif
    COM_SetupLogDir( mapname );

    g_Log.Open();
    g_Log.Printf( "Loading map \"%s\"\n", mapname );
    g_Log.PrintServerVars();

    if ( startspot )
    {
        ConDMsg("Spawn Server: %s: [%s]\n", mapname, startspot );
    }
    else
    {
        ConDMsg("Spawn Server: %s\n", mapname );
    }

    // Any partially connected client will be restarted if the spawncount is not matched.
    gHostSpawnCount = ++m_nSpawnCount;

    //
    // make cvars consistant
    //
    deathmatch.SetValue( IsMultiplayer() ? 1 : 0 );
    if ( coop.GetInt() )
    {
        deathmatch.SetValue( 0 );
    }

    current_skill = MAX( current_skill, 0 );
    current_skill = MIN( current_skill, 3 );

    skill.SetValue( (float)current_skill );

    // Setup gamemode based on the settings the map was started with
    // ExecGameTypeCfg( mapname );

    COM_TimestampedLog( "StaticPropMgr()->LevelShutdown()" );

#if !defined( DEDICATED )
    g_pShadowMgr->LevelShutdown();
#endif // DEDICATED
    StaticPropMgr()->LevelShutdown();

    // if we have an hltv relay proxy running, stop it now
	for ( CHltvServerIterator hltv; hltv; hltv.Next() )
	{
		if ( !hltv->IsMasterProxy() )
		{
			hltv->Shutdown();
		}
	}

    COM_TimestampedLog( "Host_FreeToLowMark" );

    Host_FreeStateAndWorld( true );
    Host_FreeToLowMark( true );

    // Clear out the mapversion so it's reset when the next level loads. Needed for changelevels.
    g_ServerGlobalVariables.mapversion = 0;

    COM_TimestampedLog( "sv.Clear()" );

    Clear();

    COM_TimestampedLog( "framesnapshotmanager->LevelChanged()" );

    // Clear out the state of the most recently sent packed entities from
    // the snapshot manager
    framesnapshotmanager->LevelChanged();

    // set map name and mapgroup name
	Q_FileBase( mapname, m_szBaseMapname, sizeof ( m_szBaseMapname ) );
    V_strcpy_safe( m_szMapname, mapname );
    
    if ( mapGroupName && mapGroupName[0] )
    {
        Q_strncpy( m_szMapGroupName, mapGroupName, sizeof( m_szMapGroupName ) );
    }

    // set startspot
    if (startspot)
    {
        Q_strncpy(m_szStartspot, startspot, sizeof( m_szStartspot ) );
    }
    else
    {
        m_szStartspot[0] = 0;
    }

    SV_FlushMemoryIfMarked();

    // Preload any necessary data from the xzps:
    g_pFileSystem->SetupPreloadData();
    g_pMDLCache->InitPreloadData( false );

    // Allocate server memory
    max_edicts = MAX_EDICTS;

    g_ServerGlobalVariables.maxEntities = max_edicts;
    g_ServerGlobalVariables.maxClients = GetMaxClients();
#ifndef DEDICATED
    g_ClientGlobalVariables.network_protocol = GetHostVersion();
#endif

    // Assume no entities beyond world and client slots
    num_edicts = GetMaxClients()+1;

    COM_TimestampedLog( "SV_AllocateEdicts" );

    SV_AllocateEdicts();
    g_ServerGlobalVariables.pEdicts = edicts;

    allowsignonwrites = true;

    serverclasses = 0;		// number of unique server classes
    serverclassbits = 0;		// log2 of serverclasses

    // Assign class ids to server classes here so we can encode temp ents into signon
    //  if needed
    AssignClassIds();

    COM_TimestampedLog( "Set up players" );

    // allocate player data, and assign the values into the edicts
    for ( i=0 ; i< GetClientCount() ; i++ )
    {
        CGameClient * cl = Client(i);

        // edict for a player is slot + 1, world = 0
        cl->edict = edicts + i + 1;
    
        // Setup up the edict
        InitializeEntityDLLFields( cl->edict );
    }

    COM_TimestampedLog( "Set up players(done)" );

    m_State = ss_loading;
    
    sv.SendReservationStatus( sv.kEReservationStatusSuccess );

    // Set initial time values.
    m_flTickInterval = host_state.interval_per_tick;
    m_nTickCount = (int)( 1.0 / host_state.interval_per_tick ) + 1; // Start at appropriate 1

    float flStartTimeOverride = -1.0f;
    flStartTimeOverride = CommandLine()->ParmValue( "-servertime", flStartTimeOverride );
    if ( flStartTimeOverride != -1.0f )
    {
        m_nTickCount = MAX( (int)( flStartTimeOverride / host_state.interval_per_tick ) + 1, 1 );
    }
    g_ServerGlobalVariables.tickcount = m_nTickCount;
    g_ServerGlobalVariables.curtime = GetTime();
    
    // [mhansen] Reset the host tick count so we can run in threaded mode without
    // complaints about commands being out of sync (as in every command)
    host_tickcount = g_ServerGlobalVariables.tickcount;

    // Load the world model.
    char szModelName[MAX_PATH];
    char szNameOnDisk[MAX_PATH];
    Q_snprintf( szModelName, sizeof( szModelName ), "maps/%s.bsp", mapname );
    GetMapPathNameOnDisk( szNameOnDisk, szModelName, sizeof( szNameOnDisk ) );
    g_pFileSystem->AddSearchPath( szNameOnDisk, "GAME", PATH_ADD_TO_HEAD );
    g_pFileSystem->BeginMapAccess();

#ifndef DEDICATED
	// Force reload all materials since BSP could have changed
// TODO:
// 	if ( modelloader )
// 		modelloader->UnloadUnreferencedModels();

	if ( materials )
		materials->ReloadMaterials();
#endif

    if ( !CommandLine()->FindParm( "-allowstalezip" ) )
    {
        if ( g_pFileSystem->FileExists( "stale.txt", "GAME" ) )
        {
            Warning( "This map is not final!!  Needs to be rebuilt without -keepstalezip and without -onlyents\n" );
        }
    }

    COM_TimestampedLog( "modelloader->GetModelForName(%s) -- Start", szModelName );

    host_state.SetWorldModel( modelloader->GetModelForName( szModelName, IModelLoader::FMODELLOADER_SERVER ) );
    if ( !host_state.worldmodel )
    {
        ConMsg( "Couldn't spawn server %s\n", szModelName );
        m_State = ss_dead;
        g_pFileSystem->EndMapAccess();
        return false;
    }

    COM_TimestampedLog( "modelloader->GetModelForName(%s) -- Finished", szModelName );

    if ( IsMultiplayer() && !IsGameConsole() )
    {
#ifndef DEDICATED
        EngineVGui()->UpdateProgressBar(PROGRESS_CRCMAP);
#endif
        // Server map CRC check.
        CRC32_Init(&worldmapCRC);
        if ( !CRC_MapFile( &worldmapCRC, szNameOnDisk ) )
        {
            ConMsg( "Couldn't CRC server map: %s\n", szNameOnDisk );
            m_State = ss_dead;
            g_pFileSystem->EndMapAccess();
            return false;
        }

#ifndef DEDICATED
        EngineVGui()->UpdateProgressBar(PROGRESS_CRCCLIENTDLL);
#endif

        // DLL CRC check.
        Q_snprintf( szDllName, sizeof( szDllName ), "bin\\client.dll" );
        Q_FixSlashes( szDllName );
        if ( !CRC_File( &clientDllCRC, szDllName ) )
        {
            clientDllCRC = 0xFFFFFFFF; // we don't require a CRC, its optional
        }
    }
    else
    {
        worldmapCRC	= 0;
        clientDllCRC = 0;
    }

    m_StringTables = networkStringTableContainerServer;

    COM_TimestampedLog( "SV_CreateNetworkStringTables" );

#ifndef DEDICATED
    EngineVGui()->UpdateProgressBar(PROGRESS_CREATENETWORKSTRINGTABLES);
#endif

    // Create network string tables ( including precache tables )
    SV_CreateNetworkStringTables( mapname );
    stringTableCRC = g_pStringTableDictionary->GetCRC();

    // Leave empty slots for models/sounds/generic (not for decals though)
    PrecacheModel( "", 0 );
    PrecacheGeneric( "", 0 );
    PrecacheSound( "", 0 );

    COM_TimestampedLog( "Precache world model (%s)", szModelName );

#ifndef DEDICATED
    EngineVGui()->UpdateProgressBar(PROGRESS_PRECACHEWORLD);
#endif
    // Add in world
    PrecacheModel( szModelName, RES_FATALIFMISSING | RES_PRELOAD, host_state.worldmodel );

    COM_TimestampedLog( "Precache brush models" );

    // Add world submodels to the model cache
    for ( i = 1 ; i < host_state.worldbrush->numsubmodels ; i++ )
    {
        // Add in world brush models
        char localmodel[5]; // inline model names "*1", "*2" etc
        Q_snprintf( localmodel, sizeof( localmodel ), "*%i", i );

        PrecacheModel( localmodel, RES_FATALIFMISSING | RES_PRELOAD, modelloader->GetModelForName( localmodel, IModelLoader::FMODELLOADER_SERVER ) );
    }

#ifndef DEDICATED
    EngineVGui()->UpdateProgressBar(PROGRESS_CLEARWORLD);
#endif
    COM_TimestampedLog( "SV_ClearWorld" );

    // Clear world interaction links
    // Loads and inserts static props
    SV_ClearWorld();

    //
    // load the rest of the entities
    //

    COM_TimestampedLog( "InitializeEntityDLLFields" );

    InitializeEntityDLLFields( edicts );
    edicts->ClearFree();

    g_ServerGlobalVariables.coop = ( coop.GetInt() != 0 );
    g_ServerGlobalVariables.deathmatch = ( !g_ServerGlobalVariables.coop && ( deathmatch.GetInt() != 0 ) );

    g_ServerGlobalVariables.mapname   = MAKE_STRING( m_szMapname );
    g_ServerGlobalVariables.startspot = MAKE_STRING( m_szStartspot );
    g_ServerGlobalVariables.mapGroupName = MAKE_STRING( m_szMapGroupName );

    GetTestScriptMgr()->CheckPoint( "map_load" );

    // set game event
    IGameEvent *event = g_GameEventManager.CreateEvent( "server_spawn" );
    if ( event )
    {
        event->SetString( "hostname", host_name.GetString() );
        // event->SetString( "address", net_local_adr.ToString( false ) );
        // event->SetInt(    "port", GetUDPPort() );
        event->SetString( "game", com_gamedir );
        event->SetString( "mapname", GetMapName() );
        event->SetInt(    "maxplayers", GetMaxClients() );
        event->SetInt(	  "password", 0 );				// TODO
#if defined( _WIN32 )
        event->SetString( "os", "WIN32" );
#elif defined ( LINUX )
        event->SetString( "os", "LINUX" );
#elif defined ( OSX )
        event->SetString( "os", "OSX" );
#elif defined ( _PS3 )
        event->SetString( "os", "PS3" );
#else
#error
#endif
        event->SetInt( "dedicated", IsDedicated() ? 1 : 0 );

        g_GameEventManager.FireEvent( event );
    }

    COM_TimestampedLog( "SV_SpawnServer -- Finished" );

    g_pFileSystem->EndMapAccess();
	SV_SetSteamCrashComment();
    return true;
}


void CGameServer::UpdateMasterServerPlayers()
{
    if ( !Steam3Server().SteamGameServer() )
        return;

    for ( int i=0; i < GetClientCount() ; i++ )
    {
        CGameClient *client = Client(i);
        
        if ( !client->IsConnected() )
            continue;

        CPlayerState *pl = serverGameClients->GetPlayerState( client->edict );
        if ( !pl )
            continue;

        if ( !client->m_SteamID.IsValid() )
            continue;

		extern bool CanShowHostTvStatus();
		if ( client->IsHLTV() && !CanShowHostTvStatus() )
			continue;

        Steam3Server().SteamGameServer()->BUpdateUserData( client->m_SteamID, client->GetClientName(), pl->score );
    }
}


//-----------------------------------------------------------------------------
// SV_IsSimulating
//-----------------------------------------------------------------------------
bool SV_IsSimulating( void )
{
    if ( sv.IsPaused() )
        return false;

#ifndef DEDICATED
    // Don't simulate in single player if console is down or the bug UI is active and we're in a game 
    if ( !sv.IsMultiplayer() )
    {
        if ( g_LostVideoMemory )
            return false;

        // Don't simulate in single player if console is down or the bug UI is active and we're in a game 
        if ( GetBaseLocalClient().IsActive() && ( Con_IsVisible() || EngineVGui()->ShouldPause() ) )
            return false;
    }
#endif //DEDICATED
    
    return true;
}

namespace CDebugOverlay
{
    extern void PurgeServerOverlays( void );
}

extern bool g_bIsVGuiBasedDedicatedServer;

//-----------------------------------------------------------------------------
// Purpose: Run physics code (simulating == false means we're paused, but we'll still
//  allow player usercmds to be processed
//-----------------------------------------------------------------------------
void SV_Think( bool bIsSimulating )
{
    VPROF( "SV_Physics" );
        
    if ( sv.IsDedicated() )
    {
        sv.UpdateReservedState();
        if ( sv.IsHibernating() )
        {
            // if we're hibernating, just sleep for a while and do not call server.dll to run a frame
            int nMilliseconds = sv_hibernate_ms.GetInt();
#ifndef DEDICATED // Non-Linux
            if ( g_bIsVGuiBasedDedicatedServer )
            {
                // Keep VGUi happy
                nMilliseconds = sv_hibernate_ms_vgui.GetInt();
            }
#endif
            NET_SleepUntilMessages( nMilliseconds );
            return;
        }
    }

    g_ServerGlobalVariables.tickcount   = sv.m_nTickCount;
    g_ServerGlobalVariables.curtime		= sv.GetTime();
    g_ServerGlobalVariables.frametime	= bIsSimulating ? host_state.interval_per_tick : 0;

    // in singleplayer only run think/simulation if localplayer is connected
#ifdef DEDICATED
    bIsSimulating =  bIsSimulating && sv.IsMultiplayer();
#else
    bIsSimulating =  bIsSimulating && ( sv.IsMultiplayer() || GetBaseLocalClient().IsActive() );
#endif

#ifndef DEDICATED
    CDebugOverlay::PurgeServerOverlays();
#endif

    g_pServerPluginHandler->GameFrame( bIsSimulating );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : simulating - 
//-----------------------------------------------------------------------------
void SV_PreClientUpdate(bool bIsSimulating )
{
    if ( !serverGameDLL )
        return;

    serverGameDLL->PreClientUpdate( bIsSimulating );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
/*
==================
SV_Frame

==================
*/
CFunctor *g_pDeferredServerWork;

void SV_FrameExecuteThreadDeferred()
{
    if ( g_pDeferredServerWork )
    {
        (*g_pDeferredServerWork)();
        delete g_pDeferredServerWork;
        g_pDeferredServerWork = NULL;
    }
}

void SV_SendClientUpdates( bool bIsSimulating, bool bSendDuringPause )
{
    bool bForcedSend = s_bForceSend;
    s_bForceSend = false;

    // ask game.dll to add any debug graphics
    SV_PreClientUpdate( bIsSimulating );

    // This causes network messages to be sent
    sv.SendClientMessages( bIsSimulating || bForcedSend );

    // tricky, increase stringtable tick at least one tick
    // so changes made after this point are not counted to this server
    // frame since we already send out the client snapshots
    networkStringTableContainerServer->SetTick( sv.m_nTickCount + 1 );
}

void SV_ProcessVoice( void )
{
    VPROF( "SV_ProcessVoice" );

    sv.ProcessVoice();
}

extern void PrintPropSkippedReport();

void SV_Frame( bool finalTick )
{
	PrintPropSkippedReport();
    VPROF( "SV_Frame" );
    SNPROF( "SV_Frame" );

    if ( serverGameDLL && finalTick )
    {
        serverGameDLL->Think( finalTick );
    }

    if ( !sv.IsActive() || !Host_ShouldRun() )
    {
        // Need to process LAN searches
        NET_ProcessSocket( NS_SERVER, &sv );
        return;
    }

    g_ServerGlobalVariables.frametime = host_state.interval_per_tick;

    bool bIsSimulating = SV_IsSimulating();
    bool bSendDuringPause = sv_noclipduringpause ? sv_noclipduringpause->GetBool() : false;

    // unlock sting tables to allow changes, helps to find unwanted changes (bebug build only)
    networkStringTableContainerServer->Lock( false );
    
    // Run any commands from client and play client Think functions if it is time.
    sv.RunFrame(); // read network input etc

    if ( sv.GetClientCount() > 0 )
    {	
        bool serverCanSimulate = ( serverGameDLL && !serverGameDLL->IsRestoring() ) ? true : false;

        if ( serverCanSimulate && ( bIsSimulating || bSendDuringPause ) )
        {
            sv.m_nTickCount++;

            networkStringTableContainerServer->SetTick( sv.m_nTickCount );
        }

        SV_Think( bIsSimulating );
    }
    else if ( sv.IsMultiplayer() )
    {
        SV_Think( false );	// let the game.dll systems think
    }

    // Send the results of movement and physics to the clients
    if ( finalTick )
    {
        if ( !IsEngineThreaded() || sv.IsMultiplayer() )
            SV_SendClientUpdates( bIsSimulating, bSendDuringPause );
        else
            g_pDeferredServerWork = CreateFunctor( SV_SendClientUpdates, bIsSimulating, bSendDuringPause );

    }

    // lock string tables
    networkStringTableContainerServer->Lock( true );

#if !defined(NO_STEAM)
    // let the steam auth server process new connections
    if ( sv.IsMultiplayer() || serverGameDLL->ShouldPreferSteamAuth() )
    {
        Steam3Server().RunFrame();
    }
#endif
}

void SV_SetSteamCrashComment( void )
{
	static bool s_bSteamApiWasInitialized = false;
	if ( Steam3Server().BIsActive() )
		s_bSteamApiWasInitialized = true;
	
	if ( sv.IsDedicated() && s_bSteamApiWasInitialized )
	{
		extern char g_minidumpinfo[ 4094 ];

		char osversion[ 256 ];
		osversion[ 0 ] = 0;
#if defined(WIN32)
		extern void DisplaySystemVersion( char *osversion, int maxlen );
		DisplaySystemVersion( osversion, sizeof( osversion ) );
#endif

		struct tm newtime;
		char tString[ 128 ];
		Plat_GetLocalTime( &newtime );
		Plat_GetTimeString( &newtime, tString, sizeof( tString ) );
		int tLen = Q_strlen( tString );
		if ( tLen > 0 && tString[ tLen - 1 ] == '\n' )
		{
			tString[ tLen - 1 ] = 0;
		}

		Q_snprintf( g_minidumpinfo, sizeof(g_minidumpinfo),
			"Map: %s  Group: %s  Ver: %d\n"\
			"Game: %s\n"\
			"Build: %i\n"\
			"OS: %s\n"\
			"Time: %s\n"\
			"cmdline:%s\n" \
			"protocol:%d\n",
			g_ServerGlobalVariables.mapname.ToCStr(), g_ServerGlobalVariables.mapGroupName.ToCStr(), g_ServerGlobalVariables.mapversion,
			com_gamedir, build_number(), osversion, tString, CommandLine()->GetCmdLine(),
			g_ServerGlobalVariables.network_protocol );

#ifndef NO_STEAM
		SteamAPI_SetMiniDumpComment( g_minidumpinfo );
#endif
	}
}

