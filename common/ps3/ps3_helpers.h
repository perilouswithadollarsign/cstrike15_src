#ifndef __PS3_HELPERS__
#define __PS3_HELPERS__

#ifndef SN_TARGET_PS3
#error
#endif


#include <cellstatus.h>
#include <sys/paths.h>
#include <sys/prx.h>
#include <sys/synchronization.h>
#include <sys/ppu_thread.h>
#include <sys/timer.h>
#include "ppu_intrinsics.h"
#include <np.h>
#include <np/drm.h>

// Forward declarations

#ifndef _CERT
#define PS3PRXLOADDIAGNOSTIC printf
#else
#define PS3PRXLOADDIAGNOSTIC( ... ) ((void)0)
#endif

struct TLSGlobals;

// PS3 PRX load parameters structures

struct PS3_PrxLoadParametersBase_t
{
	int32_t cbSize;
	sys_prx_id_t sysPrxId;
	uint64_t uiFlags;
	uint64_t reserved[7];
};

struct PS3_PrxModuleEntry_t
{
	PS3_PrxModuleEntry_t *pNextModule;
	char chName[256];
	uint32_t uiRefCount;
	PS3_PrxLoadParametersBase_t prxParams[0];
};
extern "C" PS3_PrxModuleEntry_t ** PS3_PrxGetModulesList();


struct PS3_GcmSharedData;

// exported from tier0
extern "C" PS3_GcmSharedData *g_pGcmSharedData;


struct PS3_GcmSharedData
{
	void *m_pIoMemory;
	uint32_t m_nIoMemorySize;

	PS3_GcmSharedData()
	{
		memset(this, 0, sizeof(PS3_GcmSharedData));
	}
	
	// Thread for the QMS and Server (when host_thread_mode)
	// Thread wakes up on a semaphore and when it does so it checks for 
	// sv_runflag and then qms runFlag and does the right thing
	// This is hand coded because the PS3 scheduler sometimes (regardless of priorities)
	// pushes out the main thread to run the server if the server is it's own job.
	// So we create a single thread, one which we have the ability to explicitly
	// run the server and the qms
	// The semaphore is there so that we can sleep instead of exiting the job
	// Otherwise the run flags actually control if we run locklessly
	// CheckForServerRequest() is called eregularly on the QMS and so allows the
	// server to interrupt the QMS and run pretty much where we need to to.
	// 

	sys_ppu_thread_t	m_thread;
	sys_semaphore_t		m_semaphore;

	// Server

	volatile int		m_svRunFlag;
	volatile int		m_svDoneFlag;
	volatile int		m_numTicks;
	void	(*m_pfnAsyncServer)(int numTicks);

	// QMS

	volatile int		m_qmsRunFlag;
	volatile int		m_qmsDoneFlag;
	volatile void*		m_cmat;
	volatile void*      m_ptr;
	void		(*m_func)(void*, void*);

	// Audio

	volatile int		m_audioRunFlag;
	volatile int		m_audioDoneFlag;
	void		(*m_AudioFunc)(void);
	
	// Endframe Defrag

	volatile int		m_bDeFrag;

	// Create semaphore & thread
	
	void Init()
	{
		// Create Semaphore
		sys_semaphore_attribute_t attr;

		sys_semaphore_attribute_initialize(attr);
		int ret = sys_semaphore_create(&m_semaphore, &attr, 0, 2);		// No point in allowing > 2 posts
		if(ret != CELL_OK) 
		{
			printf("Unable to create QMS sem\n");
		}
	}

	void RunServer(void (*pfn)(int), int numticks)
	{

		// Set global data

		m_pfnAsyncServer = pfn;
		m_numTicks  = numticks;

		__lwsync();

		m_svRunFlag = 1;

		// Post semaphore incase trhead sleeps

		sys_semaphore_post(m_semaphore, 1);

	}

	void RunQMS(void (*func)(void* cmat, void* ptr), void* cmat, void* ptr )
	{
		m_func = func;
		m_cmat = cmat;
		m_ptr  = ptr;

		__lwsync();

		m_qmsRunFlag = 1;

		// Post semaphore incase trhead sleeps

		sys_semaphore_post(m_semaphore, 1);
	}

	void WaitForServer()
	{
		while (!m_svDoneFlag) sys_timer_usleep(60);
		m_svDoneFlag = 0;

	}

	void WaitForQMS()
	{
		while (!m_qmsDoneFlag) sys_timer_usleep(60);
		m_qmsDoneFlag = 0;

	}

	void CheckForServerRequest()
	{
		if (m_svRunFlag) 
		{
			m_svRunFlag   = 0;
			m_pfnAsyncServer(m_numTicks);
			m_svDoneFlag = 1;
		}
	}

	// Audio


	void RunAudio(void (*pfn)(void))
	{

		// Set global data

		m_AudioFunc = pfn;

		__lwsync();

		m_audioRunFlag = 1;

		// Post semaphore incase trhead sleeps

		sys_semaphore_post(m_semaphore, 1);

	}

	void WaitForAudio()
	{
		while (!m_audioDoneFlag) sys_timer_usleep(60);
		m_audioDoneFlag = 0;
	}

	void CheckForAudioRequest()
	{
		if (g_pGcmSharedData->m_audioRunFlag)
		{
			g_pGcmSharedData->m_audioRunFlag = 0;
			g_pGcmSharedData->m_AudioFunc();
			g_pGcmSharedData->m_audioDoneFlag = 1;
		}
	}

};


class CPs3ContentPathInfo;
struct PS3_LoadTier0_Parameters_t : public PS3_PrxLoadParametersBase_t
{
	typedef TLSGlobals * ( *PFNGETTLSGLOBALS )();
	PFNGETTLSGLOBALS pfnGetTlsGlobals; // [IN] [ from launcher_main to tier0 ]

	PS3_PrxModuleEntry_t **ppPrxModulesList; // [IN] [ from launcher_main: head of the list of loaded PRX modules ]
	
	CPs3ContentPathInfo *pPS3PathInfo; // [IN] [ from launcher_main to tier0 ]

	uint64_t fiosLaunchTime; // [IN] [ from launcher_main: the time when the launcher was launched, the baseline time ]

	uint32_t nCLNumber; // [IN] [ from launcher_main to tier0: the changelist number for this image (0 if unknown) ]
	
	void(*pfnPushMarker)( const char * pName );
	void(*pfnPopMarker)();
	void(*pfnSwapBufferMarker)();

	// Raw SPU libSN functions

	void (*snRawSPULockHandler) (void);
	void (*snRawSPUUnlockHandler) (void);
	void (*snRawSPUNotifyCreation) (unsigned int uID);
	void (*snRawSPUNotifyDestruction) (unsigned int uID);
	void (*snRawSPUNotifyElfLoad) (unsigned int uID, unsigned int uEntry, const char *pFileName);
	void (*snRawSPUNotifyElfLoadNoWait) (unsigned int uID, unsigned int uEntry, const char *pFileName);
	void (*snRawSPUNotifyElfLoadAbs) (unsigned int uID, unsigned int uEntry, const char *pFileName);
	void (*snRawSPUNotifyElfLoadAbsNoWait) (unsigned int uID, unsigned int uEntry, const char *pFileName);
	void (*snRawSPUNotifySPUStopped) (unsigned int uID);
	void (*snRawSPUNotifySPUStarted) (unsigned int uID);

	struct PS3_GcmSharedData *m_pGcmSharedData;

	typedef void ( *PFNSHUTDOWN )();
	PFNSHUTDOWN pfnTier0Shutdown; // [OUT] [ tier0 shutdown procedure to be invoked ]
};

struct PS3_LoadLauncher_Parameters_t : public PS3_PrxLoadParametersBase_t
{
	typedef int ( *PFNLAUNCHERMAIN )( int argc, char **argv );
	PFNLAUNCHERMAIN pfnLauncherMain; // [OUT] [ launcher entry point ]

	typedef void ( *PFNSHUTDOWN )();
	PFNSHUTDOWN pfnLauncherShutdown; // [OUT] [ launcher shutdown procedure to be invoked ]
};

struct PS3_LoadAppSystemInterface_Parameters_t : public PS3_PrxLoadParametersBase_t
{
	typedef void* ( *PFNCREATEINTERFACE )( const char *pName, int *pReturnCode );
	
	PFNCREATEINTERFACE pfnCreateInterface; // [OUT] [ app system module create interface entry point ]
	uint64_t reserved[8];
};

inline int PS3_PrxLoad( const char *path, PS3_PrxLoadParametersBase_t *params )
{
	#define NP_POOL_SIZE (128*1024)
	static uint8_t np_pool[NP_POOL_SIZE];

	int			res;
	int			modres;
	sys_prx_id_t	id;

	if ( !params )
		return -1;

	PS3_PrxModuleEntry_t ** ppPrxModulesList = PS3_PrxGetModulesList();

	//
	// Walk the loaded list
	//

	for ( PS3_PrxModuleEntry_t *pEntry = *ppPrxModulesList; pEntry; pEntry = pEntry->pNextModule )
	{
		if ( strcmp( pEntry->chName, path ) )
			continue;

		++ pEntry->uiRefCount;
		memcpy( params, pEntry->prxParams, ( pEntry->prxParams->cbSize <= params->cbSize ) ? pEntry->prxParams->cbSize : params->cbSize );
		PS3PRXLOADDIAGNOSTIC("PRX MODULE ADDREF: %s [0x%08X] (refs=%u)\n", path, params->sysPrxId, pEntry->uiRefCount);
		return 0;
	}

	//
	// Load a new instance of PRX
	//


	params->sysPrxId = -1;

	// If sceNp wasn't already initalised then we need to un-init it after we're done here. If Steam is loaded
	// after this and it finds NP is already intialised it won't like it
	int npInit = sceNpInit( NP_POOL_SIZE, np_pool );
	SceNpDrmKey key = { 0x2B, 0x8E, 0xD3, 0xE4, 0xDF, 0xF1, 0x43, 0xA2, 0xA5, 0xD7, 0x4D, 0x8D, 0x89, 0x29, 0xC5, 0xF4 };
	sceNpDrmIsAvailable( &key, path );

	id = sys_prx_load_module(path, 0, NULL);
	if (id < CELL_OK)
	{
		PS3PRXLOADDIAGNOSTIC("sys_prx_load_module failed: %s [0x%08x]\n", path, id);
		return id;
	}
	PS3PRXLOADDIAGNOSTIC("PRX MODULE LOADED: %s [0x%08X]\n", path, id);
	
	if ( npInit != SCE_NP_ERROR_ALREADY_INITIALIZED )
	{
		sceNpTerm();
	}
	
	params->sysPrxId = id;
	res = sys_prx_start_module(id, params->cbSize, params, &modres, 0, NULL);
	if (res < CELL_OK)
	{
		PS3PRXLOADDIAGNOSTIC("sys_prx_start_module failed: %s [0x%08x]\n", path, res);
		return res;
	}
	PS3PRXLOADDIAGNOSTIC("PRX MODULE STARTED: %s [0x%08X 0x%08X]\n", path, id, res);

	//
	// Add to the loaded list
	//

	if ( void *pvEntry = malloc( sizeof( PS3_PrxModuleEntry_t ) + params->cbSize ) )
	{
		PS3_PrxModuleEntry_t *pPrxEntry = ( PS3_PrxModuleEntry_t * ) pvEntry;
		pPrxEntry->pNextModule = *ppPrxModulesList;
		*ppPrxModulesList = pPrxEntry;
		strncpy( pPrxEntry->chName, path, sizeof( pPrxEntry->chName ) );
		pPrxEntry->uiRefCount = 1;
		memcpy( pPrxEntry->prxParams, params, params->cbSize );
	}

	return modres;
}

inline int PS3_PrxUnload( sys_prx_id_t id )
{
	PS3_PrxModuleEntry_t ** ppPrxModulesList = PS3_PrxGetModulesList();

	//
	// Walk the loaded list
	//

	for ( PS3_PrxModuleEntry_t *pEntry = *ppPrxModulesList, **ppFromPrevEntry = ppPrxModulesList;
		  pEntry; ppFromPrevEntry = &pEntry->pNextModule, pEntry = pEntry->pNextModule )
	{
		if ( pEntry->prxParams->sysPrxId != id )
			continue;

		if ( -- pEntry->uiRefCount )
		{
			PS3PRXLOADDIAGNOSTIC("PRX MODULE RELREF: %s [0x%08X] (refs=%u)\n", pEntry->chName, pEntry->prxParams->sysPrxId, pEntry->uiRefCount);
			return 0;
		}
		else
		{
			PS3PRXLOADDIAGNOSTIC("PRX MODULE UNLOAD: %s [0x%08X]\n", pEntry->chName, pEntry->prxParams->sysPrxId, pEntry->uiRefCount);
			*ppFromPrevEntry = pEntry->pNextModule;
			free( pEntry );
			break;
		}
	}

	//
	// Perform the system unload process
	//

	int			modres;
	int			res;

	res = sys_prx_stop_module(id, 0, NULL, &modres, 0, NULL);
	if (res < CELL_OK)
	{
		PS3PRXLOADDIAGNOSTIC("sys_prx_stop_module failed: id=0x%08x, 0x%08x\n", id, res);
		return res;
	}
	PS3PRXLOADDIAGNOSTIC("PRX MODULE STOPPED: id=0x%08x, 0x%08x\n", id, res);
	
	res = sys_prx_unload_module(id, 0, NULL);
	if (res < CELL_OK)
	{
		PS3PRXLOADDIAGNOSTIC("sys_prx_unload_module failed: id=0x%08X, 0x%08x\n", id, res);
		return res;
	}
	PS3PRXLOADDIAGNOSTIC("PRX MODULE UNLOADED: id=0x%08x, 0x%08x\n", id, res);

	return modres;
}

//////////////////////////////////////////////////////////////////////////
//
// DEFINITION OF BASIC APPSYSTEM PRX IMPLEMENTATION
//
//
#include "ps3_changelistver.h"
#ifndef PS3CLVERMAJOR
#define PS3CLVERMAJOR ((APPCHANGELISTVERSION / 256) % 256)
#define PS3CLVERMINOR  (APPCHANGELISTVERSION % 256)
#endif

#ifdef _DEBUG
#define PS3_PRX_SYS_MODULE_INFO_FULLMACROREPLACEMENTHELPER(ps3modulename) SYS_MODULE_INFO( ps3modulename##_dbg, 0, PS3CLVERMAJOR, PS3CLVERMINOR)
#else
#define PS3_PRX_SYS_MODULE_INFO_FULLMACROREPLACEMENTHELPER(ps3modulename) SYS_MODULE_INFO( ps3modulename##_rel, 0, PS3CLVERMAJOR, PS3CLVERMINOR)
#endif

#define PS3_PRX_SYS_MODULE_START_NAME_FULLMACROREPLACEMENTHELPER(ps3modulename)  _##ps3modulename##_ps3_prx_entry

#define PS3_PRX_APPSYSTEM_MODULE( ps3modulename ) \
 \
PS3_PRX_SYS_MODULE_INFO_FULLMACROREPLACEMENTHELPER( ps3modulename ); \
SYS_MODULE_START( PS3_PRX_SYS_MODULE_START_NAME_FULLMACROREPLACEMENTHELPER( ps3modulename ) ); \
 \
extern "C" int PS3_PRX_SYS_MODULE_START_NAME_FULLMACROREPLACEMENTHELPER( ps3modulename )( unsigned int args, void *pArg ) \
{ \
	Assert( args >= sizeof( PS3_LoadAppSystemInterface_Parameters_t ) ); \
	PS3_LoadAppSystemInterface_Parameters_t *pParams = reinterpret_cast< PS3_LoadAppSystemInterface_Parameters_t * >( pArg ); \
	Assert( pParams->cbSize >= sizeof( PS3_LoadAppSystemInterface_Parameters_t ) ); \
	pParams->pfnCreateInterface = CreateInterface; \
	return SYS_PRX_RESIDENT; \
} \

//
//
// END DEFINITION OF BASIC APPSYSTEM PRX IMPLEMENTATION
//
//
//////////////////////////////////////////////////////////////////////////


#endif // __PS3_HELPERS__
