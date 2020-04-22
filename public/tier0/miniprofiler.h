//========= Copyright c 1996-2008, Valve Corporation, All rights reserved. ============
// This will contain things like helper functions to measure tick count of small pieces
// of code precisely; or measure performance counters (L2 misses, mispredictions etc.)
// on different hardware.
//=====================================================================================

// this class defines a section of code to measure

#ifndef TIER0_MINIPROFILER_HDR
#define TIER0_MINIPROFILER_HDR

#include "microprofiler.h"


#define ENABLE_MINI_PROFILER 0 // ENABLE_MICRO_PROFILER

class CMiniProfiler;
class CLinkedMiniProfiler;

extern "C"
{
#if defined( STATIC_LINK )
	#define MINIPROFILER_DLL_LINKAGE extern
#elif defined( TIER0_DLL_EXPORT )
	#define MINIPROFILER_DLL_LINKAGE DLL_EXPORT
#else
	#define MINIPROFILER_DLL_LINKAGE DLL_IMPORT
#endif
	MINIPROFILER_DLL_LINKAGE CMiniProfiler *g_pLastMiniProfiler;
	MINIPROFILER_DLL_LINKAGE CMiniProfiler *g_pRootMiniProfiler;
	MINIPROFILER_DLL_LINKAGE CLinkedMiniProfiler *g_pGlobalMiniProfilers;
	MINIPROFILER_DLL_LINKAGE CLinkedMiniProfiler *g_pAssertMiniProfilers;
	MINIPROFILER_DLL_LINKAGE uint32 g_nMiniProfilerFrame;
	MINIPROFILER_DLL_LINKAGE void PublishAll( CLinkedMiniProfiler * pList, uint32 nHistoryMax );
	MINIPROFILER_DLL_LINKAGE CMiniProfiler* PushMiniProfilerTS( CMiniProfiler *pProfiler );
	MINIPROFILER_DLL_LINKAGE void PopMiniProfilerTS( CMiniProfiler *pProfiler );
	MINIPROFILER_DLL_LINKAGE void AppendMiniProfilerToList( CLinkedMiniProfiler *pProfiler, CLinkedMiniProfiler **ppList );
	MINIPROFILER_DLL_LINKAGE void RemoveMiniProfilerFromList( CLinkedMiniProfiler *pProfiler );
}

#if ENABLE_MINI_PROFILER

typedef CMicroProfilerSample CMiniProfilerSample;
class CMiniProfiler: public CMicroProfiler
{
protected:
	uint64 m_numTimeBaseTicksInCallees; // this is the time to subtract from m_numTimeBaseTicks to get the "exclusive" time in this block
public:
	CMiniProfiler()
	{
		Reset();
	}

	explicit CMiniProfiler( const CMicroProfiler &profiler ): 
		CMicroProfiler( profiler ),
		m_numTimeBaseTicksInCallees( 0 )
	{

	}

	CMiniProfiler &operator=( const CMiniProfiler &other )
	{
		*(CMicroProfiler *)this = (const CMicroProfiler &)other;
		m_numTimeBaseTicksInCallees = other.m_numTimeBaseTicksInCallees;
		return *this;
	}

	void SubCallee( int64 numTimeBaseTicks )
	{
		m_numTimeBaseTicksInCallees += numTimeBaseTicks;
	}

	void Reset()
	{
		CMicroProfiler::Reset();
		m_numTimeBaseTicksInCallees = 0;
	}

	uint64 GetNumTimeBaseTicksInCallees() const
	{
		return m_numTimeBaseTicksInCallees;
	}

	int64 GetNumTimeBaseTicksExclusive( ) const
	{
		return m_numTimeBaseTicks - m_numTimeBaseTicksInCallees;
	}


	void Accumulate( const CMiniProfiler &other )
	{
		CMicroProfiler::Accumulate( other );
		m_numTimeBaseTicksInCallees += other.m_numTimeBaseTicksInCallees;
	}

	DLL_CLASS_EXPORT  void Publish( const char *szMessage, ... );
};

#else

class CMiniProfilerSample
{
public:
	int GetElapsed()const
	{
		return 0;
	}
};
class CMiniProfiler: public CMicroProfiler
{
public:
	CMiniProfiler() {}
	explicit CMiniProfiler( const CMicroProfiler &profiler ) :
		CMicroProfiler( profiler ){}
	CMiniProfiler &operator=( const CMiniProfiler &other ) { return *this; }
	void SubCallee( int64 numTimeBaseTicks ) {}
	uint64 GetNumTimeBaseTicksInCallees() const { return 0; }
	int64 GetNumTimeBaseTicksExclusive() const { return 0; }
	void Accumulate( const CMiniProfiler &other ) {}
	void Reset() {}
	void Damp( int shift = 1 ) {}
	void Publish( const char *szMessage, ... ) {}
};

#endif






class CLinkedMiniProfiler: public CMiniProfiler
{
public:
#if ENABLE_MINI_PROFILER
	CLinkedMiniProfiler *m_pNext, **m_ppPrev;
	const char *m_pName;
	const char *m_pLocation;
	CMiniProfiler *m_pLastParent;     // for dynamic tracking of an approximate call tree
	CMiniProfiler *m_pDeclaredParent; // for static tracking of the logical dependency tree
	//uint32 m_nId;
#endif
public:
	CLinkedMiniProfiler( const char *pName, CLinkedMiniProfiler**ppList = &g_pGlobalMiniProfilers, CMiniProfiler *pDeclaredParent = NULL )
	{
#if ENABLE_MINI_PROFILER
		m_pName = pName;
		m_pLocation = NULL;
		// NOTE: m_pNext and m_ppPrev have to be NULLs the first time around. This constructor can be called multiple times
		//       from multiple threads, and there's no way to ensure the constructor isn't called twice. CNetworkGameServerBase::SV_PackEntity() 
		//       is an example of the function that enters from 2 threads and collides with itself in this constructor
		AppendMiniProfilerToList( this, ppList );
		m_pLastParent = NULL;
		m_pDeclaredParent = pDeclaredParent;
#endif
	}

	CLinkedMiniProfiler( const char *pName, const char *pLocation )
	{
#if ENABLE_MINI_PROFILER
		m_pName = pName;
		m_pLocation = pLocation;
		// NOTE: m_pNext and m_ppPrev have to be NULLs the first time around. This constructor can be called multiple times
		//       from multiple threads, and there's no way to ensure the constructor isn't called twice. CNetworkGameServerBase::SV_PackEntity() 
		//       is an example of the function that enters from 2 threads and collides with itself in this constructor
		AppendMiniProfilerToList( this, &g_pGlobalMiniProfilers );
		m_pLastParent = NULL;
		m_pDeclaredParent = NULL;
#endif
	}

#if ENABLE_MINI_PROFILER
	CLinkedMiniProfiler *GetNext() { return m_pNext; }
	const char *GetName() const { return m_pName; }
	const char *GetLocation( )const { return m_pLocation; }
#else
	CLinkedMiniProfiler *GetNext() { return NULL; }
	const char *GetName() const { return "DISABLED"; }
	const char *GetLocation( )const { return NULL; }
#endif

	~CLinkedMiniProfiler()
	{
#if ENABLE_MINI_PROFILER
		RemoveMiniProfilerFromList( this );
#endif
	}
	void Publish( uint nHistoryMax );
	void PurgeHistory();
};

class CAssertMiniProfiler: public CLinkedMiniProfiler
{
public:
	CAssertMiniProfiler( const char *pFunction, const char *pFile, int nLine ): CLinkedMiniProfiler( pFunction, &g_pAssertMiniProfilers )
	{
		m_pFile = pFile;
		m_nLine = nLine;
		m_bAsserted = false;
	}
	~CAssertMiniProfiler()
	{
	}
	operator bool& () { return m_bAsserted; }
	void operator = ( bool bAsserted ){ m_bAsserted = bAsserted; }
	void operator = ( int bAsserted ) { m_bAsserted = (bAsserted != 0); }

public:
	const char *m_pFile;
	int m_nLine;

public:
	bool m_bAsserted;
};



template < class Sampler, bool bThreadSafe >
class TMiniProfilerGuard: public Sampler
{
#if ENABLE_MINI_PROFILER
	CMiniProfiler *m_pProfiler, *m_pLastProfiler;
	int m_numCallsToAdd;
#endif
public:
	void Begin( CMiniProfiler *pProfiler, int numCallsToAdd )
	{
#if ENABLE_MINI_PROFILER
		m_numCallsToAdd = numCallsToAdd;
		m_pProfiler = pProfiler;
		if ( bThreadSafe )
		{
			m_pLastProfiler = PushMiniProfilerTS( pProfiler );
		}
		else
		{
			m_pLastProfiler = g_pLastMiniProfiler;
			g_pLastMiniProfiler = pProfiler;
		}
#endif
	}

	void SetCallCount( int numCalls )
	{
#if ENABLE_MINI_PROFILER
		m_numCallsToAdd = numCalls;
#endif
	}
	void AddCallCount( int addCalls )
	{
#if ENABLE_MINI_PROFILER
		m_numCallsToAdd += addCalls;
#endif
	}

	TMiniProfilerGuard( CLinkedMiniProfiler *pProfiler, int numCallsToAdd = 1 )
	{
		Begin( pProfiler, numCallsToAdd );
#if ENABLE_MINI_PROFILER
		pProfiler->m_pLastParent = m_pLastProfiler;
#endif
	}

	TMiniProfilerGuard( CMiniProfiler *pProfiler, int numCallsToAdd = 1 )
	{
		Begin( pProfiler, numCallsToAdd );
	}
	~TMiniProfilerGuard( )
	{
#if ENABLE_MINI_PROFILER
		int64 nElapsed = GetElapsed( );
		m_pProfiler->Add( nElapsed, m_numCallsToAdd );
		m_pLastProfiler->SubCallee( nElapsed );
		if ( bThreadSafe )
		{
			PopMiniProfilerTS( m_pLastProfiler );
		}
		else
		{
			g_pLastMiniProfiler = m_pLastProfiler;
		}
#endif
	}
};




typedef TMiniProfilerGuard<CMiniProfilerSample, false> CMiniProfilerGuardFast; // default guard uses rdtsc
typedef TMiniProfilerGuard<CMiniProfilerSample, true> CMiniProfilerGuard, CMiniProfilerGuardTS; // default guard uses rdtsc



class CMiniProfilerGuardSimple: public CMiniProfilerSample
{
#if ENABLE_MINI_PROFILER
	CMiniProfiler *m_pProfiler, *m_pLastProfiler;
#endif
public:

	CMiniProfilerGuardSimple( CLinkedMiniProfiler *pProfiler )
	{
#if ENABLE_MINI_PROFILER
		m_pProfiler = pProfiler;
		pProfiler->m_pLastParent = m_pLastProfiler = PushMiniProfilerTS( pProfiler ); // = g_pLastMiniProfiler; g_pLastMiniProfiler = pProfiler;
#endif
	}

	~CMiniProfilerGuardSimple()
	{
#if ENABLE_MINI_PROFILER
		// Note: we measure both push and pop as if they belonged to the measured function
		PopMiniProfilerTS( m_pLastProfiler ); // g_pLastMiniProfiler = m_pLastProfiler;
		int64 nElapsed = GetElapsed( ); 
		m_pProfiler->Add( nElapsed );
		m_pLastProfiler->SubCallee( nElapsed );
#endif
	}
};


class CMiniProfilerAntiGuard: public CMiniProfilerSample
{
#if ENABLE_MINI_PROFILER
	CMiniProfiler *m_pProfiler;
#endif
public:
	CMiniProfilerAntiGuard( CMiniProfiler *pProfiler )
	{
#if ENABLE_MINI_PROFILER
		m_pProfiler = pProfiler;
#endif
	}
	~CMiniProfilerAntiGuard( )
	{
#if ENABLE_MINI_PROFILER
		m_pProfiler->Add( -GetElapsed( ) );
#endif
	}
};

#define MPROF_VAR_NAME_INTERNAL_CAT(a, b)	a##b
#define MPROF_VAR_NAME_INTERNAL( a, b )		MPROF_VAR_NAME_INTERNAL_CAT( a, b )
#define MPROF_VAR_NAME( a )					MPROF_VAR_NAME_INTERNAL( a, __LINE__ )
#define MPROF_TO_STRING_0( a ) #a
#define MPROF_TO_STRING( a ) MPROF_TO_STRING_0(a)

#ifdef PROJECTNAME
#define MPROF_PROJECTNAME_STRING MPROF_TO_STRING(PROJECTNAME)
#else
#define MPROF_PROJECTNAME_STRING ""
#endif

#if ENABLE_MINI_PROFILER
#define MPROF_GR( item, group ) static CLinkedMiniProfiler MPROF_VAR_NAME(miniProfilerNode)( ( item ), group "\n" __FUNCTION__ "\n" __FILE__ "\n" MPROF_TO_STRING( __LINE__ ) "\n" MPROF_PROJECTNAME_STRING );	CMiniProfilerGuardSimple MPROF_VAR_NAME( miniProfilerGuard )( &MPROF_VAR_NAME( miniProfilerNode ) )
#else
#define MPROF_GR( item, group ) 
#endif

#define MPROF_NODE( name, item, group ) CLinkedMiniProfiler miniprofiler_##name( ( item ), group "\n\n" __FILE__ "\n" MPROF_TO_STRING( __LINE__ ) "\n" MPROF_PROJECTNAME_STRING );
#define MPROF_AUTO( name ) CMiniProfilerGuardSimple MPROF_VAR_NAME(miniProfiler_##name##_auto)( &miniprofiler_##name )
#define MPROF_AUTO_FAST( name ) CMicroProfilerGuard MPROF_VAR_NAME(microProfiler_##name##_auto)( &miniprofiler_##name )
#define MPROF_AUTO_FAST_COUNT( NAME, COUNT ) CMicroProfilerGuardWithCount MPROF_VAR_NAME(microProfiler_##NAME##_auto)( &miniprofiler_##NAME, COUNT )

#define MPROF( item ) MPROF_GR( item, "" )
#define MPROF_FN() MPROF( __FUNCTION__ ) 


#endif
