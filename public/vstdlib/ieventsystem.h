//========== Copyright (c) Valve Corporation, All rights reserved. ==========//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef IEVENTSYSTEM_H
#define IEVENTSYSTEM_H

#ifdef _WIN32
#pragma once
#endif

#include "appframework/iappsystem.h"
#include "tier0/basetypes.h"
#include "tier1/functors.h"

//-----------------------------------------------------------------------------
// Event queues are used to allow listeners to decide at what point they
// want to deal with event processing
//-----------------------------------------------------------------------------
DECLARE_POINTER_HANDLE( EventQueue_t );
#define EVENT_QUEUE_HANDLE_INVALID ( (EventQueue_t)0 )

DECLARE_POINTER_HANDLE( EventId_t );
#define EVENT_ID_INVALID ( (EventId_t)0 )


//-----------------------------------------------------------------------------
// Global interface for posting/listening to events
//
// Usage for registering/unregistering listeners and posting events:
// DEFINE_EVENT2_WITHNAMES( TestEvent, int, x, int, y );
// void OnTestEvent( const int &x, const int &y );
// void f()
// {
//		hTest = g_pEventSystem->CreateEventQueue();
//		TestEvent::RegisterFunc( hTest, OnTestEvent );
//			or TestEvent::RegisterFunc( hTest, myClassPtr, &CMyClass::OnTestEvent );
//		TestEvent::Post( 100, 200 );
//		TestEvent::PostToListener( myClassPtr, 100, 200 );
//		g_pEventSystem->ProcessEvents( hTest );
//		TestEvent::UnregisterFunc( hTest, OnTestEvent );
//		g_pEventSystem->DestroyEventQueue( hTest );
// }
//
// Note that the arguments of the event handlers are always const references
// to the arguments specified in the event declaration to avoid extra copies
// of the data.
//
// Also note that it's possible to post an event to a specific listener
// using PostToListener. Just specify the class name as the first argument.
//
// Shit gets funky when you pass pointers to events. You need to be very careful
// that the pointer is guaranteed to be valid until the messages are being posted.
// Also, because everything is a const reference, the function prototypes
// for stuff that accepts pointers looks like this strangeness:
//
// DEFINE_EVENT2_WITHNAMES( TestEvent, int, x, char *, pName );
// void OnTestEvent( const int &x, char * const & pName );
//
// Also note: You cannot register or unregister while in the middle of processing events
//
// NOTE: We use EventClass::RegisterMemberFunc / EventClass:RegisterFunc
// instead of overloading EventClass::Register to get more sane-looking
// error message from the compiler if it happens to be passed an invalid
// event handler.
//
// From a perf standpoint, registering + unregistering listeners, and 
// destroying event queues all can cause thread stalls. Do them with
// low frequency.
//-----------------------------------------------------------------------------
abstract_class IEventSystem : public IAppSystem
{
public:
	// Creates an event queue.
	// Creation can occur at any time. Destruction can happen at any time
	// provided you're not simultaneously processing events or registering/unregistering
	// listeners on the same thread
	virtual EventQueue_t CreateEventQueue() = 0;
	virtual void DestroyEventQueue( EventQueue_t hQueue ) = 0;

	// Processess queued events for a event queue
	virtual void ProcessEvents( EventQueue_t hQueue ) = 0;

	// Ignore that macro magic! It allows us to call PostEvent on an arbitrary event id
	// with arbitrary arguments. Useful to allow systems to post event ids told to
	// it by external systems
	inline void PostEvent( EventId_t nEventId )
	{
		CFunctorData *pData = CreateFunctorData( );
		PostEventInternal( nEventId, EVENT_QUEUE_HANDLE_INVALID, NULL, pData );
	}

	inline void PostEvent( EventId_t nEventId, EventQueue_t hQueue )
	{
		CFunctorData *pData = CreateFunctorData( );
		PostEventInternal( nEventId, hQueue, NULL, pData );
	}

	inline void PostEventToListener( EventId_t nEventId, const void *pListener )
	{
		CFunctorData *pData = CreateFunctorData( );
		PostEventInternal( nEventId, EVENT_QUEUE_HANDLE_INVALID, pListener, pData );
	}

	inline void PostEventToListener( EventId_t nEventId, EventQueue_t hQueue, const void *pListener )
	{
		CFunctorData *pData = CreateFunctorData( );
		PostEventInternal( nEventId, hQueue, pListener, pData );
	}

	#define DEFINE_POST_EVENT(N) \
		template < FUNC_SOLO_TEMPLATE_ARG_PARAMS_##N > \
		inline void PostEvent( EventId_t nEventId FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
			CFunctorData *pData = CreateFunctorData( FUNC_CALL_ARGS_##N ); \
			PostEventInternal( nEventId, EVENT_QUEUE_HANDLE_INVALID, NULL, pData ); \
		} \
		template < FUNC_SOLO_TEMPLATE_ARG_PARAMS_##N > \
		inline void PostEvent( EventId_t nEventId, EventQueue_t hQueue FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
			CFunctorData *pData = CreateFunctorData( FUNC_CALL_ARGS_##N ); \
			PostEventInternal( nEventId, hQueue, NULL, pData ); \
		} \
		template < FUNC_SOLO_TEMPLATE_ARG_PARAMS_##N > \
		inline void PostEventToListener( EventId_t nEventId, const void *pListener FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
			CFunctorData *pData = CreateFunctorData( FUNC_CALL_ARGS_##N ); \
			PostEventInternal( nEventId, EVENT_QUEUE_HANDLE_INVALID, pListener, pData ); \
		} \
		template < FUNC_SOLO_TEMPLATE_ARG_PARAMS_##N > \
		inline void PostEventToListener( EventId_t nEventId, EventQueue_t hQueue, const void *pListener FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
			CFunctorData *pData = CreateFunctorData( FUNC_CALL_ARGS_##N ); \
			PostEventInternal( nEventId, hQueue, pListener, pData ); \
		}
	FUNC_GENERATE_ALL_BUT0( DEFINE_POST_EVENT );
	#undef DEFINE_POST_EVENT

private:
	// NOTE: These are not meant to be called directly. See the comment above
	// the IEventSystem class definition for how to post/register/unregister events
	virtual EventId_t RegisterEvent( const char *pEventName ) = 0;
	virtual void PostEventInternal( EventId_t nEventId, EventQueue_t hQueue, const void *pListener, CFunctorData *pData ) = 0;
	virtual void RegisterListener( EventId_t nEventId, EventQueue_t hQueue, CFunctorCallback *pCallback ) = 0;
	virtual void UnregisterListener( EventId_t nEventId, EventQueue_t hQueue, CFunctorCallback *pCallback ) = 0;

	#define DEFINE_FRIEND_CLASSES(N) \
		template < typename Event_t FUNC_TEMPLATE_ARG_PARAMS_##N > friend class CEventSignature##N
	FUNC_GENERATE_ALL( DEFINE_FRIEND_CLASSES );
	#undef DEFINE_FRIEND_CLASSES
};


//-----------------------------------------------------------------------------
// Ignore that man behind the curtain!
//-----------------------------------------------------------------------------
#define DEFINE_EVENT_INTERNAL(N) \
	template < typename Event_t FUNC_TEMPLATE_ARG_PARAMS_##N > \
	class CEventSignature##N \
	{ \
	public: \
		static inline void Post( FUNC_PROXY_ARG_FORMAL_PARAMS_##N ) \
		{ \
			CFunctorData *pData = CreateFunctorData( FUNC_CALL_ARGS_##N ); \
			g_pEventSystem->PostEventInternal( Event_t::GetEventId(), EVENT_QUEUE_HANDLE_INVALID, NULL, pData ); \
		} \
		static inline void Post( EventQueue_t hQueue FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
			CFunctorData *pData = CreateFunctorData( FUNC_CALL_ARGS_##N ); \
			g_pEventSystem->PostEventInternal( Event_t::GetEventId(), hQueue, NULL, pData ); \
		} \
		static inline void PostToListener( const void *pListener FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
			CFunctorData *pData = CreateFunctorData( FUNC_CALL_ARGS_##N ); \
			g_pEventSystem->PostEventInternal( Event_t::GetEventId(), EVENT_QUEUE_HANDLE_INVALID, pListener, pData ); \
		} \
		static inline void PostToListener( EventQueue_t hQueue, const void *pListener FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
			CFunctorData *pData = CreateFunctorData( FUNC_CALL_ARGS_##N ); \
			g_pEventSystem->PostEventInternal( Event_t::GetEventId(), hQueue, pListener, pData ); \
		} \
		static inline void RegisterFunc( EventQueue_t hQueue, void (*pfnProxied)( FUNC_PROXY_ARG_FORMAL_PARAMS_##N ) ) \
		{ \
			CFunctorCallback *pCallback = CreateFunctorCallback( pfnProxied ); \
			g_pEventSystem->RegisterListener( Event_t::GetEventId(), hQueue, pCallback ); \
		} \
		static inline void UnregisterFunc( EventQueue_t hQueue, void (*pfnProxied)( FUNC_PROXY_ARG_FORMAL_PARAMS_##N ) ) \
		{ \
			CFunctorCallback *pCallback = CreateFunctorCallback( pfnProxied ); \
			g_pEventSystem->UnregisterListener( Event_t::GetEventId(), hQueue, pCallback ); \
		} \
		template < class OBJECT_TYPE_PTR > \
		static inline void RegisterMemberFunc( EventQueue_t hQueue, OBJECT_TYPE_PTR *pClass, void (OBJECT_TYPE_PTR::*pfnProxied)( FUNC_PROXY_ARG_FORMAL_PARAMS_##N ) ) \
		{ \
			CFunctorCallback *pCallback = CreateFunctorCallback( pClass, pfnProxied ); \
			g_pEventSystem->RegisterListener( Event_t::GetEventId(), hQueue, pCallback ); \
		} \
		template < class OBJECT_TYPE_PTR > \
		static inline void UnregisterMemberFunc( EventQueue_t hQueue, OBJECT_TYPE_PTR *pClass, void (OBJECT_TYPE_PTR::*pfnProxied)( FUNC_PROXY_ARG_FORMAL_PARAMS_##N ) ) \
		{ \
			CFunctorCallback *pCallback = CreateFunctorCallback( pClass, pfnProxied ); \
			g_pEventSystem->UnregisterListener( Event_t::GetEventId(), hQueue, pCallback ); \
		} \
		static EventId_t RegisterEvent( const char *pEventName ) \
		{ \
			return g_pEventSystem->RegisterEvent( pEventName ); \
		} \
	};

FUNC_GENERATE_ALL( DEFINE_EVENT_INTERNAL )

#define DEFINE_EVENTID_INTERNAL( _eventName ) \
	public: \
		static EventId_t GetEventId() \
		{ \
			static EventId_t s_nEventId = EVENT_ID_INVALID; \
			if ( s_nEventId == EVENT_ID_INVALID ) \
			{ \
				s_nEventId = RegisterEvent( #_eventName ); \
			} \
			return s_nEventId; \
		}

#define DEFINE_EVENT( _eventName ) class _eventName : public CEventSignature0< _eventName > { DEFINE_EVENTID_INTERNAL( _eventName ); }
#define DEFINE_EVENT1( _eventName, _arg1 ) class _eventName : public CEventSignature1< _eventName, _arg1 > { DEFINE_EVENTID_INTERNAL( _eventName ); }
#define DEFINE_EVENT2( _eventName, _arg1, _arg2 ) class _eventName : public CEventSignature2< _eventName, _arg1, _arg2 > { DEFINE_EVENTID_INTERNAL( _eventName ); }
#define DEFINE_EVENT3( _eventName, _arg1, _arg2, _arg3 ) class _eventName : public CEventSignature3< _eventName, _arg1, _arg2, _arg3 > { DEFINE_EVENTID_INTERNAL( _eventName ); }
#define DEFINE_EVENT4( _eventName, _arg1, _arg2, _arg3, _arg4 ) class _eventName : public CEventSignature4< _eventName, _arg1, _arg2, _arg3, _arg4 > { DEFINE_EVENTID_INTERNAL( _eventName ); }
#define DEFINE_EVENT5( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5 ) class _eventName : public CEventSignature5< _eventName, _arg1, _arg2, _arg3, _arg4, _arg5 > { DEFINE_EVENTID_INTERNAL( _eventName ); }
#define DEFINE_EVENT6( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6 ) class _eventName : public CEventSignature6< _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6 > { DEFINE_EVENTID_INTERNAL( _eventName ); }
#define DEFINE_EVENT7( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7 ) class _eventName : public CEventSignature7< _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7 > { DEFINE_EVENTID_INTERNAL( _eventName ); }
#define DEFINE_EVENT8( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8 ) class _eventName : public CEventSignature8< _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8 > { DEFINE_EVENTID_INTERNAL( _eventName ); }
#define DEFINE_EVENT9( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8, _arg9 ) class _eventName : public CEventSignature9< _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8, _arg9 > { DEFINE_EVENTID_INTERNAL( _eventName ); }
#define DEFINE_EVENT10( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8, _arg9, _arg10 ) class _eventName : public CEventSignature10< _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8, _arg9, _arg10 > { DEFINE_EVENTID_INTERNAL( _eventName ); }
#define DEFINE_EVENT11( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8, _arg9, _arg10, _arg11 ) class _eventName : public CEventSignature11< _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8, _arg9, _arg10, _arg11 > { DEFINE_EVENTID_INTERNAL( _eventName ); }
#define DEFINE_EVENT12( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8, _arg9, _arg10, _arg11, _arg12 ) class _eventName : public CEventSignature12< _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8, _arg9, _arg10, _arg11, _arg12 > { DEFINE_EVENTID_INTERNAL( _eventName ); }
#define DEFINE_EVENT13( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8, _arg9, _arg10, _arg11, _arg12, _arg13 ) class _eventName : public CEventSignature13< _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8, _arg9, _arg10, _arg11, _arg12, _arg13 > { DEFINE_EVENTID_INTERNAL( _eventName ); }
#define DEFINE_EVENT14( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8, _arg9, _arg10, _arg11, _arg12, _arg13, _arg14 ) class _eventName : public CEventSignature14< _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8, _arg9, _arg10, _arg11, _arg12, _arg13, _arg14 > { DEFINE_EVENTID_INTERNAL( _eventName ); }

#define DEFINE_EVENT1_WITHNAMES( _eventName, _arg1, _arg1Name ) \
		DEFINE_EVENT1( _eventName, _arg1 )
#define DEFINE_EVENT2_WITHNAMES( _eventName, _arg1, _arg1Name, _arg2, _arg2Name ) \
		DEFINE_EVENT2( _eventName, _arg1, _arg2 )
#define DEFINE_EVENT3_WITHNAMES( _eventName, _arg1, _arg1Name, _arg2, _arg2Name, _arg3, _arg3Name ) \
		DEFINE_EVENT3( _eventName, _arg1, _arg2, _arg3 )
#define DEFINE_EVENT4_WITHNAMES( _eventName, _arg1, _arg1Name, _arg2, _arg2Name, _arg3, _arg3Name, _arg4, _arg4Name ) \
		DEFINE_EVENT4( _eventName, _arg1, _arg2, _arg3, _arg4 )
#define DEFINE_EVENT5_WITHNAMES( _eventName, _arg1, _arg1Name, _arg2, _arg2Name, _arg3, _arg3Name, _arg4, _arg4Name, _arg5, _arg5Name ) \
		DEFINE_EVENT5( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5 )
#define DEFINE_EVENT6_WITHNAMES( _eventName, _arg1, _arg1Name, _arg2, _arg2Name, _arg3, _arg3Name, _arg4, _arg4Name, _arg5, _arg5Name, _arg6, _arg6Name ) \
		DEFINE_EVENT6( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6 )
#define DEFINE_EVENT7_WITHNAMES( _eventName, _arg1, _arg1Name, _arg2, _arg2Name, _arg3, _arg3Name, _arg4, _arg4Name, _arg5, _arg5Name, _arg6, _arg6Name, _arg7, _arg7Name ) \
		DEFINE_EVENT7( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7 )
#define DEFINE_EVENT8_WITHNAMES( _eventName, _arg1, _arg1Name, _arg2, _arg2Name, _arg3, _arg3Name, _arg4, _arg4Name, _arg5, _arg5Name, _arg6, _arg6Name, _arg7, _arg7Name, _arg8, _arg8Name ) \
		DEFINE_EVENT8( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8 )
#define DEFINE_EVENT9_WITHNAMES( _eventName, _arg1, _arg1Name, _arg2, _arg2Name, _arg3, _arg3Name, _arg4, _arg4Name, _arg5, _arg5Name, _arg6, _arg6Name, _arg7, _arg7Name, _arg8, _arg8Name, _arg9, _arg9Name ) \
		DEFINE_EVENT9( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8, _arg9 )
#define DEFINE_EVENT10_WITHNAMES( _eventName, _arg1, _arg1Name, _arg2, _arg2Name, _arg3, _arg3Name, _arg4, _arg4Name, _arg5, _arg5Name, _arg6, _arg6Name, _arg7, _arg7Name, _arg8, _arg8Name, _arg9, _arg9Name, _arg10, _arg10Name ) \
		DEFINE_EVENT10( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8, _arg9, _arg10 )
#define DEFINE_EVENT11_WITHNAMES( _eventName, _arg1, _arg1Name, _arg2, _arg2Name, _arg3, _arg3Name, _arg4, _arg4Name, _arg5, _arg5Name, _arg6, _arg6Name, _arg7, _arg7Name, _arg8, _arg8Name, _arg9, _arg9Name, _arg10, _arg10Name, _arg11, _arg11Name ) \
		DEFINE_EVENT11( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8, _arg9, _arg10, _arg11 )
#define DEFINE_EVENT12_WITHNAMES( _eventName, _arg1, _arg1Name, _arg2, _arg2Name, _arg3, _arg3Name, _arg4, _arg4Name, _arg5, _arg5Name, _arg6, _arg6Name, _arg7, _arg7Name, _arg8, _arg8Name, _arg9, _arg9Name, _arg10, _arg10Name, _arg11, _arg11Name, _arg12, _arg12Name ) \
		DEFINE_EVENT12( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8, _arg9, _arg10, _arg11, _arg12 )
#define DEFINE_EVENT13_WITHNAMES( _eventName, _arg1, _arg1Name, _arg2, _arg2Name, _arg3, _arg3Name, _arg4, _arg4Name, _arg5, _arg5Name, _arg6, _arg6Name, _arg7, _arg7Name, _arg8, _arg8Name, _arg9, _arg9Name, _arg10, _arg10Name, _arg11, _arg11Name, _arg12, _arg12Name, _arg13, _arg13Name ) \
		DEFINE_EVENT13( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8, _arg9, _arg10, _arg11, _arg12, _arg13 )
#define DEFINE_EVENT14_WITHNAMES( _eventName, _arg1, _arg1Name, _arg2, _arg2Name, _arg3, _arg3Name, _arg4, _arg4Name, _arg5, _arg5Name, _arg6, _arg6Name, _arg7, _arg7Name, _arg8, _arg8Name, _arg9, _arg9Name, _arg10, _arg10Name, _arg11, _arg11Name, _arg12, _arg12Name, _arg13, _arg13Name, _arg14, _arg14Name ) \
		DEFINE_EVENT14( _eventName, _arg1, _arg2, _arg3, _arg4, _arg5, _arg6, _arg7, _arg8, _arg9, _arg10, _arg11, _arg12, _arg13, _arg14 )

#endif // IEVENTSYSTEM_H
