//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: To accomplish X360 TCR 22, we need to call present ever 66msec
// at least during loading screens.	This amazing hack will do it
// by overriding the allocator to tick it every so often.
//
// $NoKeywords: $
//===========================================================================//

#include "LoadScreenUpdate.h"
#include "tier0/memalloc.h"
#include "tier1/delegates.h"
#include "tier0/threadtools.h"
#include "tier2/tier2.h"
#include "materialsystem/imaterialsystem.h"
#include "tier0/dbg.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Used to tick the loading screen every so often
//-----------------------------------------------------------------------------
class CLoaderMemAlloc : public IMemAlloc
{
	// Methods of IMemAlloc
public:
	virtual void *Alloc( size_t nSize );
	virtual void *Realloc( void *pMem, size_t nSize );
	virtual void Free( void *pMem );
	DELEGATE_TO_OBJECT_2( void *,	Expand_NoLongerSupported, void *, size_t, m_pMemAlloc );
	virtual void *Alloc( size_t nSize, const char *pFileName, int nLine );
	virtual void *Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine );
	virtual void  Free( void *pMem, const char *pFileName, int nLine );
#ifdef MEMALLOC_SUPPORTS_ALIGNED_ALLOCATIONS
	DELEGATE_TO_OBJECT_2( void *,	AllocAlign, size_t, size_t, m_pMemAlloc );
	DELEGATE_TO_OBJECT_4( void *,	AllocAlign, size_t, size_t, const char *, int, m_pMemAlloc );
	DELEGATE_TO_OBJECT_3( void *,	ReallocAlign, void *, size_t, size_t, m_pMemAlloc );
#endif
	DELEGATE_TO_OBJECT_4( void*,	Expand_NoLongerSupported, void *, size_t, const char *, int, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1( size_t,	GetSize, void *, m_pMemAlloc );
	DELEGATE_TO_OBJECT_2V(			PushAllocDbgInfo, const char *, int, m_pMemAlloc );
	DELEGATE_TO_OBJECT_0V(			PopAllocDbgInfo, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1( int32,	CrtSetBreakAlloc, int32, m_pMemAlloc );
	DELEGATE_TO_OBJECT_2( int,		CrtSetReportMode, int, int, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1( int,		CrtIsValidHeapPointer, const void *, m_pMemAlloc );
	DELEGATE_TO_OBJECT_3( int,		CrtIsValidPointer, const void *, unsigned int, int, m_pMemAlloc );
	DELEGATE_TO_OBJECT_0( int,		CrtCheckMemory, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1( int,		CrtSetDbgFlag, int, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1V(			CrtMemCheckpoint, _CrtMemState *, m_pMemAlloc );
	DELEGATE_TO_OBJECT_0V(			DumpStats, m_pMemAlloc );
	DELEGATE_TO_OBJECT_2V(			DumpStatsFileBase, const char *, IMemAlloc::DumpStatsFormat_t, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1( size_t,	ComputeMemoryUsedBy, const char *, m_pMemAlloc );
	DELEGATE_TO_OBJECT_2( void*,	CrtSetReportFile, int, void*, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1( void*,	CrtSetReportHook, void*, m_pMemAlloc );
	DELEGATE_TO_OBJECT_5( int,		CrtDbgReport, int, const char *, int, const char *, const char *, m_pMemAlloc );
	DELEGATE_TO_OBJECT_0( int,		heapchk, m_pMemAlloc );
	DELEGATE_TO_OBJECT_0( bool,		IsDebugHeap, m_pMemAlloc );
	DELEGATE_TO_OBJECT_2V(			GetActualDbgInfo, const char *&, int &, m_pMemAlloc );
	DELEGATE_TO_OBJECT_5V(			RegisterAllocation, const char *, int, size_t, size_t, unsigned, m_pMemAlloc );
	DELEGATE_TO_OBJECT_5V(			RegisterDeallocation, const char *, int, size_t, size_t, unsigned, m_pMemAlloc );
	DELEGATE_TO_OBJECT_0( int,		GetVersion, m_pMemAlloc );
	DELEGATE_TO_OBJECT_0V(			CompactHeap, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1( MemAllocFailHandler_t, SetAllocFailHandler, MemAllocFailHandler_t, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1V(			DumpBlockStats, void *, m_pMemAlloc );
	DELEGATE_TO_OBJECT_2V(			SetStatsExtraInfo, const char *, const char *, m_pMemAlloc );
	DELEGATE_TO_OBJECT_0( size_t,	MemoryAllocFailed, m_pMemAlloc );
	DELEGATE_TO_OBJECT_0V(			CompactIncremental, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1V(			OutOfMemory, size_t, m_pMemAlloc );
	DELEGATE_TO_OBJECT_2V(			GlobalMemoryStatus, size_t *, size_t *, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1( IVirtualMemorySection *, AllocateVirtualMemorySection, size_t, m_pMemAlloc );
	DELEGATE_TO_OBJECT_1( int,		GetGenericMemoryStats, GenericMemoryStat_t **, m_pMemAlloc );

	virtual uint32 GetDebugInfoSize() { return 0; }
	virtual void SaveDebugInfo( void *pvDebugInfo ) { }
	virtual void RestoreDebugInfo( const void *pvDebugInfo ) {}	
	virtual void InitDebugInfo( void *pvDebugInfo, const char *pchRootFileName, int nLine ) {}

	virtual void *RegionAlloc( int region, size_t nSize );
	virtual void *RegionAlloc( int region, size_t nSize, const char *pFileName, int nLine );

	// Other public methods
public:
	CLoaderMemAlloc();
	void Start( MaterialNonInteractiveMode_t mode );
	void Stop();
	void AbortDueToShutdown();
	void Pause( bool bPause );

	// Check if we need to call swap. Do so if necessary.
	void CheckSwap( );

private:
	IMemAlloc	*m_pMemAlloc;
	bool		m_bPaused;
};


//-----------------------------------------------------------------------------
// Activate, deactivate loadermemalloc
//-----------------------------------------------------------------------------
static bool s_bLoadingUpdatesEnabled = true;
static CLoaderMemAlloc s_LoaderMemAlloc;

void AbortLoadingUpdatesDueToShutdown()
{
	if ( IsGameConsole() && s_bLoadingUpdatesEnabled )
	{
		s_LoaderMemAlloc.AbortDueToShutdown();
	}
	s_bLoadingUpdatesEnabled = false;
}

void BeginLoadingUpdates( MaterialNonInteractiveMode_t mode )
{
	if ( IsGameConsole() && s_bLoadingUpdatesEnabled )
	{
		s_LoaderMemAlloc.Start( mode );
	}
}

void RefreshScreenIfNecessary()
{
	if ( IsGameConsole() && s_bLoadingUpdatesEnabled )
	{
		s_LoaderMemAlloc.CheckSwap();
	}
}

void PauseLoadingUpdates( bool bPause )
{
	if ( IsGameConsole() && s_bLoadingUpdatesEnabled )
	{
		s_LoaderMemAlloc.Pause( bPause );
	}
}

void EndLoadingUpdates()
{
	if ( IsGameConsole() && s_bLoadingUpdatesEnabled )
	{
		s_LoaderMemAlloc.Stop();
	}
}

static int LoadLibraryThreadFunc()
{
	RefreshScreenIfNecessary();
	return 15;
}


//-----------------------------------------------------------------------------
// Used to tick the loading screen every so often
//-----------------------------------------------------------------------------
CLoaderMemAlloc::CLoaderMemAlloc() 
{
	m_pMemAlloc = 0;
	m_bPaused = false;
}

void CLoaderMemAlloc::Start( MaterialNonInteractiveMode_t mode )
{
	AssertFatal( ThreadInMainThread() );
	if ( g_pMemAlloc == this || ( mode == MATERIAL_NON_INTERACTIVE_MODE_NONE ) )
		return;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->EnableNonInteractiveMode( mode );

	if ( mode == MATERIAL_NON_INTERACTIVE_MODE_STARTUP )
	{
		SetThreadedLoadLibraryFunc( LoadLibraryThreadFunc );
	}

	// NOTE: This is necessary to avoid a one-frame black flash
	// since Present is what copies the back buffer into the temp buffer
	if ( mode == MATERIAL_NON_INTERACTIVE_MODE_LEVEL_LOAD )
	{
		extern void V_RenderVGuiOnly( void );
		V_RenderVGuiOnly();
	}

	m_pMemAlloc = g_pMemAlloc;
	g_pMemAlloc = this;
	m_bPaused = false;
}

void CLoaderMemAlloc::Stop()
{
	AssertFatal( ThreadInMainThread() );

	if ( g_pMemAlloc != this )
		return;

	g_pMemAlloc = m_pMemAlloc;
	m_bPaused = false;

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->EnableNonInteractiveMode( MATERIAL_NON_INTERACTIVE_MODE_NONE );
	SetThreadedLoadLibraryFunc( NULL );
}

void CLoaderMemAlloc::AbortDueToShutdown()
{
	AssertFatal( ThreadInMainThread() );

	if ( g_pMemAlloc != this )
		return;

	g_pMemAlloc = m_pMemAlloc;
	m_bPaused = false;
}

void CLoaderMemAlloc::Pause( bool bPause )
{
	if ( g_pMemAlloc != this )
		return;

	m_bPaused = bPause;
}

//-----------------------------------------------------------------------------
// Check if we need to call swap. Do so if necessary.
//-----------------------------------------------------------------------------
void CLoaderMemAlloc::CheckSwap( )
{
	if ( g_pMemAlloc != this || m_bPaused )
		return;

	g_pMaterialSystem->RefreshFrontBufferNonInteractive();
}

//-----------------------------------------------------------------------------
// Hook allocations, render when appropriate
//-----------------------------------------------------------------------------
void *CLoaderMemAlloc::Alloc( size_t nSize )
{
	CheckSwap();
	return m_pMemAlloc->IndirectAlloc( nSize );
}

void *CLoaderMemAlloc::Realloc( void *pMem, size_t nSize )
{
	CheckSwap();
	return m_pMemAlloc->Realloc( pMem, nSize );
}

void CLoaderMemAlloc::Free( void *pMem )
{
	CheckSwap();
	m_pMemAlloc->Free( pMem );
}

void *CLoaderMemAlloc::Alloc( size_t nSize, const char *pFileName, int nLine )
{
	CheckSwap();
	return m_pMemAlloc->IndirectAlloc( nSize, pFileName, nLine );
}

void *CLoaderMemAlloc::Realloc( void *pMem, size_t nSize, const char *pFileName, int nLine )
{
	CheckSwap();
	return m_pMemAlloc->Realloc( pMem, nSize, pFileName, nLine );
}

void  CLoaderMemAlloc::Free( void *pMem, const char *pFileName, int nLine )
{
	CheckSwap();
	m_pMemAlloc->Free( pMem, pFileName, nLine );
}

void *CLoaderMemAlloc::RegionAlloc( int region, size_t nSize )
{
	CheckSwap();
	return m_pMemAlloc->RegionAlloc( region, nSize );
}

void *CLoaderMemAlloc::RegionAlloc( int region, size_t nSize, const char *pFileName, int nLine )
{
	CheckSwap();
	return m_pMemAlloc->RegionAlloc( region, nSize, pFileName, nLine );
}

