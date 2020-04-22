//====== Copyright 1996-2010, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef TLS_PS3_H
#define TLS_PS3_H

#ifdef _WIN32
#pragma once
#endif

#if defined(_PS3) || defined (SN_TARGET_PS3)

#define MAX_TLS_VALUES			32	//I just checked in ep2 and we were only using 14 so have reduced to a max of 32 - RP

struct TLSGlobals
{
	// TLS values/flags
	int nThreadLocalStateIndex;
	void *TLSValues[ MAX_TLS_VALUES ];
	bool  TLSFlags[ MAX_TLS_VALUES ];
	bool bWaitObjectsCreated;
	sys_semaphore_t WaitObjectsSemaphore;
	void* pCurThread;
	int nThreadID;

	// Engine TLS data (zip/console/splitslot)
	unsigned int uiEngineZipLastErrorZ;
	bool bEngineConsoleIsInSpew;
	void* pEngineSplitSlot;

	// Malloc debugging TLS data
	void* pMallocDbgInfoStack;
	int nMallocDbgInfoStackDepth;

	// Filesystem read filename buffer
	char* pFileSystemReadFilename;

	// Material system render context
	void* pMaterialSystemRenderContext;

	// Physics virtual mesh frame locks
	void* pPhysicsVirtualMeshFrameLocks;
	
	// [MAIN] this will get set to true if the game quit normally; otherwise, an error screen will be displayed to account for "dirty disk" situations
	bool bNormalQuitRequested;
};

#ifndef _CERT
extern "C" TLSGlobals *GetTLSGlobals();
#else
#define GetTLSGlobals() reinterpret_cast< TLSGlobals * >( (unsigned int)__reg(13) - 0x7000 )
#endif


#define g_nThreadLocalStateIndex	GetTLSGlobals()->nThreadLocalStateIndex
#define gTLSValues					GetTLSGlobals()->TLSValues
#define gTLSFlags					GetTLSGlobals()->TLSFlags
#define gbWaitObjectsCreated		GetTLSGlobals()->bWaitObjectsCreated
#define gWaitObjectsSemaphore		GetTLSGlobals()->WaitObjectsSemaphore
#define g_pCurThread				GetTLSGlobals()->pCurThread
#define g_nThreadID					GetTLSGlobals()->nThreadID


// stuffed here just to save having a tiny h file

class CPs3ContentPathInfo;
extern CPs3ContentPathInfo *g_pPS3PathInfo;

#endif // defined(_PS3) || defined (SN_TARGET_PS3)

#endif // TLS_PS3_H