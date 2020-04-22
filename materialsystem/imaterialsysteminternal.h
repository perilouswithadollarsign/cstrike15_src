//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef IMATERIALSYSTEMINTERNAL_H
#define IMATERIALSYSTEMINTERNAL_H

#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/imaterialsystem.h"
#include "tier1/callqueue.h"
#include "tier1/memstack.h"
#include "tier1/fmtstr.h"

#include "tier0/vprof_telemetry.h"
#include "tier0/perfstats.h"

class IMaterialInternal;

//-----------------------------------------------------------------------------
// Special call queue that knows (a) single threaded access, and (b) all
// functions called after last function added
//-----------------------------------------------------------------------------

class CMatCallQueue
{
public:
	CMatCallQueue()
	{
		static int nStackCount = 0;
		CFmtStr stackName( "CMatCallQueue.m_Allocator[%d]", nStackCount++ );
#ifdef DEDICATED
		m_Allocator.Init( (const char *)stackName, 2*1024, 0, 0, 4 );
#else
		m_Allocator.Init( (const char *)stackName, ( IsGameConsole() || IsPlatformPosix() ) ? 2*1024*1024 : 8*1024*1024, 64*1024, 256*1024, 4 );
#endif
		m_FunctorFactory.SetAllocator( &m_Allocator );
		m_pHead = m_pTail = NULL;
	}

	size_t GetMemoryUsed()
	{
		return m_Allocator.GetUsed();
	}

	int Count()
	{
		int i = 0;
		Elem_t *pCurrent = m_pHead;
		while ( pCurrent )
		{
			i++;
			pCurrent = pCurrent->pNext;
		}
		return i;
	}

	template <typename T>
	T &Copy( T &item )
	{
		T *pCopy = (T *)m_Allocator.Alloc( sizeof(T) );
		memcpy( pCopy, &item, sizeof(T) );
		return *pCopy;
	}

	template <typename T>
	T *CopyArray( T *p, int n )
	{
		T *pCopy = (T *)m_Allocator.Alloc( sizeof(T) * n );
		memcpy( (void *)pCopy, p, sizeof(T) * n );
		return pCopy;
	}

	template <const char *>
	const char *Copy( const char *psz )
	{
		int len = V_strlen( psz );
		char *pCopy = (char *)m_Allocator.Alloc( len + 1 );
		memcpy( pCopy, psz, len + 1 );
		return pCopy;

	}

	void CallQueued()
	{
		TM_ZONE_PLOT( TELEMETRY_LEVEL1, "RenderThread",  TELEMETRY_ZONE_PLOT_SLOT_2 );
		PERF_STATS_BLOCK( "RenderThread", PERF_STATS_SLOT_RENDERTHREAD );
		if ( !m_pHead )
		{
			return;
		}

		CFunctor *pFunctor;

		Elem_t *pCurrent = m_pHead;
		while ( pCurrent )
		{
			pFunctor = pCurrent->GetFunctor();
#ifdef _DEBUG
			if ( pFunctor->m_nUserID == m_nBreakSerialNumber)
			{
				m_nBreakSerialNumber = (unsigned)-1;
			}
#endif
			if ( pCurrent->pNext )
			{
				PREFETCH360( pCurrent->pNext, 0 );
				PREFETCH360( pCurrent->pNext, 128 );
			}
			(*pFunctor)();
			pFunctor->~CFunctor(); // no need to ref count, we're alone here...
			pCurrent = pCurrent->pNext;
		}
#ifdef DEBUG_MATCALLQUEUE
		static int prevHigh = 0;
		if ( m_Allocator.GetUsed() > prevHigh )
		{
			Msg( "***%d\n", m_Allocator.GetUsed() );
			prevHigh = m_Allocator.GetUsed();
		}
#endif
		m_Allocator.FreeAll( false );
		m_pHead = m_pTail = NULL;
	}

	void QueueFunctor( CFunctor *pFunctor )
	{
		Assert( pFunctor );
		m_Allocator.Alloc( sizeof(Elem_t) );
		QueueFunctorInternal( m_FunctorFactory.CreateRefCountingFunctor( pFunctor, &CFunctor::operator() ) );
	}

	void Flush()
	{
		if ( !m_pHead )
		{
			return;
		}

		CFunctor *pFunctor;

		Elem_t *pCurrent = m_pHead;
		while ( pCurrent )
		{
			pFunctor = pCurrent->GetFunctor();
			pFunctor->Release();
			pCurrent = pCurrent->pNext;
		}

		m_Allocator.FreeAll( false );
		m_pHead = m_pTail = NULL;
	}

	#define DEFINE_MATCALLQUEUE_NONMEMBER_QUEUE_CALL(N) \
		template <typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
		void QueueCall(FUNCTION_RETTYPE (*pfnProxied)( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
			m_Allocator.Alloc( sizeof(Elem_t) ); \
			QueueFunctorInternal( m_FunctorFactory.CreateFunctor( pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N ) ); \
		}

		//-------------------------------------

	#define DEFINE_MATCALLQUEUE_MEMBER_QUEUE_CALL(N) \
		template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
		void QueueCall(OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
			m_Allocator.Alloc( sizeof(Elem_t) ); \
			QueueFunctorInternal( m_FunctorFactory.CreateFunctor( pObject, pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N ) ); \
		}

		//-------------------------------------

	#define DEFINE_MATCALLQUEUE_CONST_MEMBER_QUEUE_CALL(N) \
		template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
		void QueueCall(OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) const FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
			m_Allocator.Alloc( sizeof(Elem_t) ); \
			QueueFunctorInternal( m_FunctorFactory.CreateFunctor( pObject, pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N ) ); \
		}

		//-------------------------------------

	FUNC_GENERATE_ALL( DEFINE_MATCALLQUEUE_NONMEMBER_QUEUE_CALL );
	FUNC_GENERATE_ALL( DEFINE_MATCALLQUEUE_MEMBER_QUEUE_CALL );
	FUNC_GENERATE_ALL( DEFINE_MATCALLQUEUE_CONST_MEMBER_QUEUE_CALL );

private:
	void QueueFunctorInternal( CFunctor *pFunctor )
	{
#ifdef _DEBUG
		pFunctor->m_nUserID = m_nCurSerialNumber++;
#endif
		MEM_ALLOC_CREDIT_( "CMatCallQueue.m_Allocator" );
		// Caller is expected to have preallocated Elem_t entry immediately prior to functor
		Elem_t *pNew = ((Elem_t *)pFunctor) - 1;
		if ( m_pTail )
		{
			m_pTail->pNext = pNew;
			m_pTail = pNew;
		}
		else
		{
			m_pHead = m_pTail = pNew;
		}
		pNew->pNext = NULL;
	}

	struct Elem_t
	{
		Elem_t *pNext;
		CFunctor *GetFunctor() { return (CFunctor *)(this + 1 ); }
	};

	Elem_t *m_pHead;
	Elem_t *m_pTail;

	CMemoryStack m_Allocator;
	CCustomizedFunctorFactory<CMemoryStack, CRefCounted1<CFunctor, CRefCountServiceDestruct< CRefST > > > m_FunctorFactory;
	unsigned m_nCurSerialNumber;
	unsigned m_nBreakSerialNumber;
};

#define MATCONFIG_FLAGS_SUPPORT_EDITOR ( 1 << 0 )
#define MATCONFIG_FLAGS_SUPPORT_GBUFFER ( 1 << 1 )
//-----------------------------------------------------------------------------
// Additional interfaces used internally to the library
//-----------------------------------------------------------------------------
abstract_class IMaterialSystemInternal : public IMaterialSystem
{
public:
	// Returns the current material
	virtual IMaterial* GetCurrentMaterial() = 0;

	virtual int GetLightmapPage( void ) = 0;

	// Gets the maximum lightmap page size...
	virtual int GetLightmapWidth( int lightmap ) const = 0;
	virtual int GetLightmapHeight( int lightmap ) const = 0;

	virtual ITexture *GetLocalCubemap( void ) = 0;

//	virtual bool RenderZOnlyWithHeightClipEnabled( void ) = 0;
	virtual void ForceDepthFuncEquals( bool bEnable ) = 0;
	virtual enum MaterialHeightClipMode_t GetHeightClipMode( void ) = 0;

	// FIXME: Remove? Here for debugging shaders in CShaderSystem
	virtual void AddMaterialToMaterialList( IMaterialInternal *pMaterial ) = 0;
	virtual void RemoveMaterial( IMaterialInternal *pMaterial ) = 0;
	virtual void RemoveMaterialSubRect( IMaterialInternal *pMaterial ) = 0;
	virtual bool InFlashlightMode() const = 0;
	virtual bool IsCascadedShadowMapping() const = 0;

	// Can we use editor materials?
	virtual bool CanUseEditorMaterials() const = 0;
	virtual int GetConfigurationFlags( void ) const = 0;
	virtual const char *GetForcedTextureLoadPathID() = 0;

	virtual CMatCallQueue *GetRenderCallQueue() = 0;

	virtual void UnbindMaterial( IMaterial *pMaterial ) = 0;
	virtual uint GetRenderThreadId() const = 0 ;
};


#endif // IMATERIALSYSTEMINTERNAL_H
