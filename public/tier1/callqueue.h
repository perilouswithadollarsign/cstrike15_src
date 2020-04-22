//========== Copyright © 2006, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#ifndef CALLQUEUE_H
#define CALLQUEUE_H

#include "tier0/tslist.h"
#include "functors.h"
#include "vstdlib/jobthread.h"

#if defined( _WIN32 )
#pragma once
#endif

//-----------------------------------------------------
// Avert thy eyes! Imagine rather:
//
// void QueueCall( <function>, [args1, [arg2,]...]
// void QueueCall( <object>, <function>, [args1, [arg2,]...]
// void QueueRefCall( <object>, <<function>, [args1, [arg2,]...]
//-----------------------------------------------------

#define DEFINE_CALLQUEUE_NONMEMBER_QUEUE_CALL(N) \
	template <typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
	void QueueCall(FUNCTION_RETTYPE (*pfnProxied)( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
		QueueFunctorInternal( CreateFunctor( pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N ) ); \
		}

//-------------------------------------

#define DEFINE_CALLQUEUE_MEMBER_QUEUE_CALL(N) \
	template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
	void QueueCall(OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
		QueueFunctorInternal( CreateFunctor( pObject, pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N ) ); \
		}

//-------------------------------------

#define DEFINE_CALLQUEUE_CONST_MEMBER_QUEUE_CALL(N) \
	template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
	void QueueCall(OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) const FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
		QueueFunctorInternal( CreateFunctor( pObject, pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N ) ); \
		}

//-------------------------------------

#define DEFINE_CALLQUEUE_REF_COUNTING_MEMBER_QUEUE_CALL(N) \
	template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
	void QueueRefCall(OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
		QueueFunctorInternal( CreateRefCountingFunctor( pObject, pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N ) ); \
		}

//-------------------------------------

#define DEFINE_CALLQUEUE_REF_COUNTING_CONST_MEMBER_QUEUE_CALL(N) \
	template <typename OBJECT_TYPE_PTR, typename FUNCTION_CLASS, typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
	void QueueRefCall(OBJECT_TYPE_PTR pObject, FUNCTION_RETTYPE ( FUNCTION_CLASS::*pfnProxied )( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) const FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
		QueueFunctorInternal( CreateRefCountingFunctor( pObject, pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N ) ); \
		\
		}

#define FUNC_GENERATE_QUEUE_METHODS() \
	FUNC_GENERATE_ALL( DEFINE_CALLQUEUE_NONMEMBER_QUEUE_CALL ); \
	FUNC_GENERATE_ALL( DEFINE_CALLQUEUE_MEMBER_QUEUE_CALL ); \
	FUNC_GENERATE_ALL( DEFINE_CALLQUEUE_CONST_MEMBER_QUEUE_CALL );\
	FUNC_GENERATE_ALL( DEFINE_CALLQUEUE_REF_COUNTING_MEMBER_QUEUE_CALL ); \
	FUNC_GENERATE_ALL( DEFINE_CALLQUEUE_REF_COUNTING_CONST_MEMBER_QUEUE_CALL )

//-----------------------------------------------------

template <typename QUEUE_TYPE = CTSQueue<CFunctor *> >
class CCallQueueT
{
public:
	CCallQueueT()
		: m_bNoQueue( false )
	{
#ifdef _DEBUG
		m_nCurSerialNumber = 0;
		m_nBreakSerialNumber = (unsigned)-1;
#endif
	}

	void DisableQueue( bool bDisable )
	{
		if ( m_bNoQueue == bDisable )
		{
			return;
		}
		if ( !m_bNoQueue )
			CallQueued();

		m_bNoQueue = bDisable;
	}

	bool IsDisabled() const
	{
		return m_bNoQueue;
	}

	int Count()
	{
		return m_queue.Count();
	}

	void CallQueued()
	{
		if ( !m_queue.Count() )
		{
			return;
		}

		m_queue.PushItem( NULL );

		CFunctor *pFunctor = NULL;

		while ( m_queue.PopItem( &pFunctor ) && pFunctor != NULL )
		{
#ifdef _DEBUG
			if ( pFunctor->m_nUserID == m_nBreakSerialNumber)
			{
				m_nBreakSerialNumber = (unsigned)-1;
			}
#endif
			(*pFunctor)();
			pFunctor->Release();
		}

	}

	void ParallelCallQueued( IThreadPool *pPool = NULL )
	{
		if ( ! pPool ) 
		{
			pPool = g_pThreadPool;
		}
		int nNumThreads = 1;

		if ( pPool )
		{
			nNumThreads = MIN( pPool->NumThreads(), MAX( 1, Count() ) );
		}
		
		if ( nNumThreads < 2 )
		{
			CallQueued();
		}
		else
		{
			int *pDummy = NULL;
			ParallelProcess( pPool, pDummy, nNumThreads, this, &CCallQueueT<>::ExecuteWrapper );
		}
	}

	void QueueFunctor( CFunctor *pFunctor )
	{
		Assert( pFunctor );
		QueueFunctorInternal( RetAddRef( pFunctor ) );
	}

	void Flush()
	{
		m_queue.PushItem( NULL );

		CFunctor *pFunctor;

		while ( m_queue.PopItem( &pFunctor ) && pFunctor != NULL )
		{
			pFunctor->Release();
		}
	}

	FUNC_GENERATE_QUEUE_METHODS();

private:
	void ExecuteWrapper( int &nDummy )						// to match paralell process function template
	{
		CallQueued();
	}

	void QueueFunctorInternal( CFunctor *pFunctor )
	{
		if ( !m_bNoQueue )
		{
#ifdef _DEBUG
			pFunctor->m_nUserID = m_nCurSerialNumber++;
#endif
			m_queue.PushItem( pFunctor );
		}
		else
		{
			(*pFunctor)();
			pFunctor->Release();
		}
	}

	QUEUE_TYPE m_queue;
	bool m_bNoQueue;
	unsigned m_nCurSerialNumber;
	unsigned m_nBreakSerialNumber;
};

class CCallQueue : public CCallQueueT<>
{
};

//-----------------------------------------------------
// Optional interface that can be bound to concrete CCallQueue
//-----------------------------------------------------

class ICallQueue
{
public:
	void QueueFunctor( CFunctor *pFunctor )
	{
		// [mhansen] If we grab an extra reference here then this functor never gets released.
		// That usually isn't too bad because the memory the functor is allocated in is cleared and
		// reused every frame.  But, if the functor contains a CUtlDataEnvelope with more than
		// 4 bytes of data it allocates its own memory and, in that case, we need the destructor to
		// be called to free it.  So, we don't want to grab the "extra" reference here.
		// The net result should be that after this call the pFunctor has a ref count of 1 (which
		// is held by the functor it gets nested in, which is stuck in the call queue) and after it
		// is executed the owning functor is destructed which causes it to release the reference
		// and this functor is then freed.  This happens in imatersysteminternal.h:112 where the
		// destructor is called explictly for the owning functor: pFunctor->~CFunctor();
		//QueueFunctorInternal( RetAddRef( pFunctor ) );
		QueueFunctorInternal( pFunctor );
	}

	FUNC_GENERATE_QUEUE_METHODS();

private:
	virtual void QueueFunctorInternal( CFunctor *pFunctor ) = 0;
};

#endif // CALLQUEUE_H
