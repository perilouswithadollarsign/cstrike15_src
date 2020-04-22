#include <string.h>
#include <stdio.h>
#include <cellstatus.h>
#include <sys/prx.h>
#include <sys/exportcplusplus.h>
#include <sys/ppu_thread.h>
#include "tier0/platform.h"
#include "logging.h"
#include "dbg.h"

#include "memalloc.h"
#include "platform.h"
#include "threadtools.h"
#include "ps3/ps3_win32stubs.h"
#include "unitlib/unitlib.h"
#include "tier0/fasttimer.h"
#include "tier0/mem.h"
#include "tier0/tslist.h"
#include "tier0/vatoms.h"
#include "tier0/vprof.h"
#include "tier0/miniprofiler.h"

#include "tls_ps3.h"
#include "ps3/ps3_helpers.h"
#include "ps3/ps3_console.h"

#ifdef _DEBUG
#define tier0_ps3 tier0_dbg
#else
#define tier0_ps3 tier0_rel
#endif


PS3_PRX_SYS_MODULE_INFO_FULLMACROREPLACEMENTHELPER( tier0 );
SYS_MODULE_START( _tier0_ps3_prx_entry );
SYS_MODULE_EXIT( _tier0_ps3_prx_exit );

SYS_LIB_DECLARE( tier0_ps3, SYS_LIB_AUTO_EXPORT | SYS_LIB_WEAK_IMPORT );

#ifndef _CERT
SYS_LIB_EXPORT( GetTLSGlobals, tier0_ps3 );
#endif
SYS_LIB_EXPORT( PS3_PrxGetModulesList, tier0_ps3 );

SYS_LIB_EXPORT( LoggingSystem_RegisterLoggingChannel, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_RegisterLoggingListener, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_ResetCurrentLoggingState, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_LogAssert, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_LogDirect, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_IsChannelEnabled, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_SetChannelSpewLevel, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_SetChannelSpewLevelByName, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_SetChannelSpewLevelByTag, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_SetGlobalSpewLevel, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_SetChannelFlags, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_SetLoggingResponsePolicy, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_AddTagToCurrentChannel, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_GetChannelFlags, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_SetChannelColor, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_GetChannelColor, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_GetNextChannelID, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_GetChannelCount, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_GetChannel, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_GetFirstChannelID, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_PushLoggingState, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_PopLoggingState, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_FindChannel, tier0_ps3 );
SYS_LIB_EXPORT( LoggingSystem_Log, tier0_ps3 );
SYS_LIB_EXPORTPICKUP_CPP_FUNC( "LoggingSystem_Log(int, LoggingSeverity_t, Color, char const*, ...)", tier0_ps3 );


SYS_LIB_EXPORT( ShouldUseNewAssertDialog, tier0_ps3 );
SYS_LIB_EXPORT( DoNewAssertDialog, tier0_ps3 );
SYS_LIB_EXPORT( GetCPUInformation, tier0_ps3 );
SYS_LIB_EXPORT( _ExitOnFatalAssert, tier0_ps3 );

#if !defined( DBGFLAG_STRINGS_STRIP )
SYS_LIB_EXPORT( COM_TimestampedLog, tier0_ps3 );
#endif
SYS_LIB_EXPORT( Plat_SetBenchmarkMode, tier0_ps3 );
SYS_LIB_EXPORT( Plat_IsInDebugSession, tier0_ps3 );
SYS_LIB_EXPORT( Plat_IsInBenchmarkMode, tier0_ps3 );
SYS_LIB_EXPORT( Plat_FloatTime, tier0_ps3 );
SYS_LIB_EXPORT( Plat_MSTime, tier0_ps3 );
SYS_LIB_EXPORT( Plat_GetClockStart, tier0_ps3 );
SYS_LIB_EXPORT( Plat_GetLocalTime, tier0_ps3 );
SYS_LIB_EXPORT( Plat_ConvertToLocalTime, tier0_ps3 );
SYS_LIB_EXPORT( Platform_gmtime, tier0_ps3 );
SYS_LIB_EXPORT( Plat_timegm, tier0_ps3 );
SYS_LIB_EXPORT( Plat_timezone, tier0_ps3 );
SYS_LIB_EXPORT( Plat_daylight, tier0_ps3 );
SYS_LIB_EXPORT( Plat_GetTimeString, tier0_ps3 );
SYS_LIB_EXPORT( Plat_DebugString, tier0_ps3 );
SYS_LIB_EXPORT( Plat_SetWindowTitle, tier0_ps3 );
SYS_LIB_EXPORT( Plat_GetModuleFilename, tier0_ps3 );
SYS_LIB_EXPORT( Plat_ExitProcess, tier0_ps3 );
SYS_LIB_EXPORT( Plat_GetCommandLine, tier0_ps3 );
SYS_LIB_EXPORT( CommandLine, tier0_ps3 );

SYS_LIB_EXPORT( Plat_GetPagedPoolInfo, tier0_ps3 );
SYS_LIB_EXPORT( Plat_GetMemPageSize, tier0_ps3 );

SYS_LIB_EXPORT( _AssertValidStringPtr, tier0_ps3 );
SYS_LIB_EXPORT( _AssertValidReadPtr, tier0_ps3 );
SYS_LIB_EXPORT( _AssertValidWritePtr, tier0_ps3 );

SYS_LIB_EXPORT( MemAllocScratch, tier0_ps3 );
SYS_LIB_EXPORT( MemFreeScratch, tier0_ps3 );
SYS_LIB_EXPORT( ZeroMemory, tier0_ps3 );

#if !defined( DBGFLAG_STRINGS_STRIP )
SYS_LIB_EXPORT( Msg, tier0_ps3 );
SYS_LIB_EXPORT( Warning, tier0_ps3 );
SYS_LIB_EXPORT( DevMsg, tier0_ps3 );
SYS_LIB_EXPORTPICKUP_CPP_FUNC( "DevMsg(int, char const*, ...)", tier0_ps3 );
SYS_LIB_EXPORT( DevWarning, tier0_ps3 );
SYS_LIB_EXPORTPICKUP_CPP_FUNC( "DevWarning(int, char const*, ...)", tier0_ps3 );
#endif
SYS_LIB_EXPORT( Error, tier0_ps3 );
#if !defined( DBGFLAG_STRINGS_STRIP )
SYS_LIB_EXPORT( ConMsg, tier0_ps3 );
SYS_LIB_EXPORT( ConDMsg, tier0_ps3 );
SYS_LIB_EXPORT( ConColorMsg, tier0_ps3 );
#endif

SYS_LIB_EXPORT( ThreadSetAffinity, tier0_ps3 );
SYS_LIB_EXPORT( ThreadGetCurrentId, tier0_ps3 );
SYS_LIB_EXPORT( ThreadGetPriority, tier0_ps3 );
SYS_LIB_EXPORT( ThreadSetPriority, tier0_ps3 );
SYS_LIB_EXPORT( ThreadSetDebugName, tier0_ps3 );
SYS_LIB_EXPORT( ThreadSleep, tier0_ps3 );
SYS_LIB_EXPORT( ThreadInMainThread, tier0_ps3 );
SYS_LIB_EXPORT( ThreadGetCurrentHandle, tier0_ps3 );
SYS_LIB_EXPORT( ReleaseThreadHandle, tier0_ps3 );
SYS_LIB_EXPORT( CreateSimpleThread, tier0_ps3 );
SYS_LIB_EXPORT( SetThreadedLoadLibraryFunc, tier0_ps3 );
SYS_LIB_EXPORT( GetThreadedLoadLibraryFunc, tier0_ps3 );
SYS_LIB_EXPORT( ThreadJoin, tier0_ps3 );

SYS_LIB_EXPORT( vtune, tier0_ps3 );
SYS_LIB_EXPORT( PublishAllMiniProfilers, tier0_ps3 );

SYS_LIB_EXPORT( RunTSQueueTests, tier0_ps3 );
SYS_LIB_EXPORT( RunTSListTests, tier0_ps3 );

SYS_LIB_EXPORT( GetVAtom, tier0_ps3 );

// BEGIN PICKUP
// You must run build_prxexport_snc.bat AFTER building tier0 to regenerate prxexport.c, and THEN rebuild tier0 again for the changes to take effect.
// This is required to get C++ class exports working
SYS_LIB_EXPORTPICKUP_CLASS( "CThreadSpinRWLock@", tier0_ps3 );
SYS_LIB_EXPORTPICKUP_CLASS( "CThread@", tier0_ps3 );
SYS_LIB_EXPORTPICKUP_CLASS( "CWorkerThread@", tier0_ps3 );
SYS_LIB_EXPORTPICKUP_CLASS( "CVProfile@", tier0_ps3 );
SYS_LIB_EXPORTPICKUP_CLASS( "CVProfNode@", tier0_ps3 );
// END PICKUP

SYS_LIB_EXPORT_VAR( g_pMemAllocInternalPS3, tier0_ps3 );
SYS_LIB_EXPORT( GetCurThreadPS3, tier0_ps3 );
SYS_LIB_EXPORT( SetCurThreadPS3, tier0_ps3 );
SYS_LIB_EXPORT( AllocateThreadID, tier0_ps3 );
SYS_LIB_EXPORT( FreeThreadID, tier0_ps3 );

#ifdef VPROF_ENABLED
SYS_LIB_EXPORT_VAR( g_VProfCurrentProfile, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_VProfSignalSpike, tier0_ps3 );
#endif
SYS_LIB_EXPORT_VAR( g_ClockSpeed, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_dwClockSpeed, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_ClockSpeedMicrosecondsMultiplier, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_ClockSpeedMillisecondsMultiplier, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_ClockSpeedSecondsMultiplier, tier0_ps3 );



// Event handling
SYS_LIB_EXPORT( XBX_NotifyCreateListener, tier0_ps3 );
SYS_LIB_EXPORT( XBX_QueueEvent, tier0_ps3 );
SYS_LIB_EXPORT( XBX_ProcessEvents, tier0_ps3 );
SYS_LIB_EXPORT( XBX_DispatchEventsQueue, tier0_ps3 );

// Accessors
SYS_LIB_EXPORT( XBX_GetLanguageString, tier0_ps3 );
SYS_LIB_EXPORT( XBX_IsLocalized, tier0_ps3 );
SYS_LIB_EXPORT( XBX_IsAudioLocalized, tier0_ps3 );
SYS_LIB_EXPORT( XBX_GetNextSupportedLanguage, tier0_ps3 );
SYS_LIB_EXPORT( XBX_IsRestrictiveLanguage, tier0_ps3 );
SYS_LIB_EXPORT( XBX_GetImageChangelist, tier0_ps3 );

//
// Storage devices management
//
SYS_LIB_EXPORT( XBX_ResetStorageDeviceInfo, tier0_ps3 );
SYS_LIB_EXPORT( XBX_DescribeStorageDevice, tier0_ps3 );
SYS_LIB_EXPORT( XBX_MakeStorageContainerRoot, tier0_ps3 );

SYS_LIB_EXPORT( XBX_GetStorageDeviceId, tier0_ps3 );
SYS_LIB_EXPORT( XBX_SetStorageDeviceId, tier0_ps3 );

//
// Information about game primary user
//
SYS_LIB_EXPORT( XBX_GetPrimaryUserId, tier0_ps3 );
SYS_LIB_EXPORT( XBX_SetPrimaryUserId, tier0_ps3 );

SYS_LIB_EXPORT( XBX_GetPrimaryUserIsGuest, tier0_ps3 );
SYS_LIB_EXPORT( XBX_SetPrimaryUserIsGuest, tier0_ps3 );

//
// Disabling and enabling input from controllers
//
SYS_LIB_EXPORT( XBX_ResetUserIdSlots, tier0_ps3 );
SYS_LIB_EXPORT( XBX_ClearUserIdSlots, tier0_ps3 );

//
// Mapping between game slots and controllers
//
SYS_LIB_EXPORT( XBX_GetUserId, tier0_ps3 );
SYS_LIB_EXPORT( XBX_GetSlotByUserId, tier0_ps3 );
SYS_LIB_EXPORT( XBX_SetUserId, tier0_ps3 );
SYS_LIB_EXPORT( XBX_ClearSlot, tier0_ps3 );
SYS_LIB_EXPORT( XBX_ClearUserId, tier0_ps3 );

SYS_LIB_EXPORT( XBX_GetUserIsGuest, tier0_ps3 );
SYS_LIB_EXPORT( XBX_SetUserIsGuest, tier0_ps3 );

//
// Number of game users
//
SYS_LIB_EXPORT( XBX_GetNumGameUsers, tier0_ps3 );
SYS_LIB_EXPORT( XBX_SetNumGameUsers, tier0_ps3 );

//
// Invite related functions
//
SYS_LIB_EXPORT( XBX_GetInviteSessionId, tier0_ps3 );
SYS_LIB_EXPORT( XBX_SetInviteSessionId, tier0_ps3 );
SYS_LIB_EXPORT( XBX_GetInvitedUserXuid, tier0_ps3 );
SYS_LIB_EXPORT( XBX_SetInvitedUserXuid, tier0_ps3 );
SYS_LIB_EXPORT( XBX_GetInvitedUserId, tier0_ps3 );
SYS_LIB_EXPORT( XBX_SetInvitedUserId, tier0_ps3 );

//
// VXConsole
//
SYS_LIB_EXPORT( ValvePS3ConsoleInit, tier0_ps3 );
SYS_LIB_EXPORT( ValvePS3ConsoleShutdown, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_pValvePS3Console, tier0_ps3 );
SYS_LIB_EXPORT( EncodeBinaryToString, tier0_ps3 );
SYS_LIB_EXPORT( DecodeBinaryFromString, tier0_ps3 );





// PS3 system info
CPs3ContentPathInfo *g_pPS3PathInfo = NULL;
SYS_LIB_EXPORT_VAR( g_pPS3PathInfo, tier0_ps3 );


typedef void * ( *GetProcAddressFunc )( void *pUnused, const char *pFuncName );

#ifndef _CERT
TLSGlobals * ( *g_pfnElfGetTlsGlobals )();
extern "C" TLSGlobals *GetTLSGlobals()
{
	Assert( g_pfnElfGetTlsGlobals || !"Accessing TLS from global constructors? Illegal on PS3!" );
	return g_pfnElfGetTlsGlobals();
}
#endif

extern "C"
{

void(*g_pfnPushMarker)( const char * pName );
void(*g_pfnPopMarker)();
void(*g_pfnSwapBufferMarker)();



void (*g_snRawSPULockHandler) (void);
void (*g_snRawSPUUnlockHandler) (void);
void (*g_snRawSPUNotifyCreation) (unsigned int uID);
void (*g_snRawSPUNotifyDestruction) (unsigned int uID);
void (*g_snRawSPUNotifyElfLoad) (unsigned int uID, unsigned int uEntry, const char *pFileName);
void (*g_snRawSPUNotifyElfLoadNoWait) (unsigned int uID, unsigned int uEntry, const char *pFileName);
void (*g_snRawSPUNotifyElfLoadAbs) (unsigned int uID, unsigned int uEntry, const char *pFileName);
void (*g_snRawSPUNotifyElfLoadAbsNoWait) (unsigned int uID, unsigned int uEntry, const char *pFileName);
void (*g_snRawSPUNotifySPUStopped) (unsigned int uID);
void (*g_snRawSPUNotifySPUStarted) (unsigned int uID);

PS3_GcmSharedData *g_pGcmSharedData = NULL;

};

SYS_LIB_EXPORT_VAR( g_pfnPushMarker, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_pfnPopMarker, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_pfnSwapBufferMarker, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_pGcmSharedData, tier0_ps3 );

SYS_LIB_EXPORT_VAR( g_pPhysicsMiniProfilers, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_pOtherMiniProfilers, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_pLastMiniProfiler, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_nMiniProfilerFrame, tier0_ps3 );

SYS_LIB_EXPORT_VAR( g_snRawSPULockHandler				, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_snRawSPUUnlockHandler				, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_snRawSPUNotifyCreation			, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_snRawSPUNotifyDestruction			, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_snRawSPUNotifyElfLoad				, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_snRawSPUNotifyElfLoadNoWait		, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_snRawSPUNotifyElfLoadAbs			, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_snRawSPUNotifyElfLoadAbsNoWait	, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_snRawSPUNotifySPUStopped			, tier0_ps3 );
SYS_LIB_EXPORT_VAR( g_snRawSPUNotifySPUStarted			, tier0_ps3 );

PS3_PrxModuleEntry_t ** g_ppPrxModuleEntryList;
extern "C" PS3_PrxModuleEntry_t ** PS3_PrxGetModulesList()
{
	return g_ppPrxModuleEntryList;
}

static u32_t g_nCLNumber = 0;
extern "C" u32_t XBX_GetImageChangelist()
{
	return g_nCLNumber;
}

extern void VirtualMemoryManager_Shutdown();
void Tier0_ShutdownCallback()
{
	Warning( "[PS3 SYSTEM] TIER0 IS SHUTTING DOWN @ %.3f\n", Plat_FloatTime() );

	// Shutdown virtual memory
	VirtualMemoryManager_Shutdown();

	// Shutdown memory allocator hooks
	malloc_managed_size mms;
	mms.current_inuse_size = 0x12345678;
	mms.current_system_size = 0x09ABCDEF;
	mms.max_system_size = 0;
	(void) malloc_stats( &mms );
}

extern "C" int _tier0_ps3_prx_entry( unsigned int args, void *pArg )
{
	Assert( args >= sizeof( PS3_LoadTier0_Parameters_t ) );

	PS3_LoadTier0_Parameters_t *pParams = reinterpret_cast< PS3_LoadTier0_Parameters_t * >( pArg );
	Assert( pParams->cbSize >= sizeof( PS3_LoadTier0_Parameters_t ) );

	// copy the launch time to use as the baseline time
	extern int64_t g_fiosLaunchTime;
	g_fiosLaunchTime = pParams->fiosLaunchTime;

	if ( pParams->ppPrxModulesList )
	{
		g_ppPrxModuleEntryList = pParams->ppPrxModulesList;
	}
	g_pPS3PathInfo = pParams->pPS3PathInfo;
	g_pfnPopMarker = pParams->pfnPopMarker;
	g_pfnPushMarker = pParams->pfnPushMarker;
	g_pfnSwapBufferMarker = pParams->pfnSwapBufferMarker;
	g_pGcmSharedData = pParams->m_pGcmSharedData;
	g_nCLNumber = pParams->nCLNumber;

	// snlilb.h

#ifndef _CERT

	g_snRawSPULockHandler = pParams->snRawSPULockHandler;
	g_snRawSPUUnlockHandler = pParams->snRawSPUUnlockHandler;
	g_snRawSPUNotifyCreation = pParams->snRawSPUNotifyCreation;
	g_snRawSPUNotifyDestruction = pParams->snRawSPUNotifyDestruction;
	g_snRawSPUNotifyElfLoad = pParams->snRawSPUNotifyElfLoad;
	g_snRawSPUNotifyElfLoadNoWait = pParams->snRawSPUNotifyElfLoadNoWait;
	g_snRawSPUNotifyElfLoadAbs = pParams->snRawSPUNotifyElfLoadAbs;
	g_snRawSPUNotifyElfLoadAbsNoWait = pParams->snRawSPUNotifyElfLoadAbsNoWait;
	g_snRawSPUNotifySPUStopped = pParams->snRawSPUNotifySPUStopped;
	g_snRawSPUNotifySPUStarted = pParams->snRawSPUNotifySPUStarted;

#endif


	// Return tier0 shutdown callback
	pParams->pfnTier0Shutdown = Tier0_ShutdownCallback;

	if ( pParams->pfnGetTlsGlobals )
	{
		#ifndef _CERT
		g_pfnElfGetTlsGlobals = pParams->pfnGetTlsGlobals;
		#else
		// Validate that TLS globals are located correctly in PRX
		// modules and main ELF binary:
		// See description of quick TLS access implementation in
		// memoverride.cpp comments
		if ( GetTLSGlobals() != pParams->pfnGetTlsGlobals() )
			Error( "<vitaliy>: TLS globals location mismatch!\n" );
		#endif
	}

    return SYS_PRX_RESIDENT;
}

extern "C" int _tier0_ps3_prx_exit( unsigned int args, void *pArg )
{
	return SYS_PRX_STOP_OK;
}

void _tier0_ps3_prx_required_for_linking()
{
}
