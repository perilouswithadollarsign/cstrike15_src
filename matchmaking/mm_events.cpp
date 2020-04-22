//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_events.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CMatchEventsSubscription::CMatchEventsSubscription() :
	m_bAllowNestedBroadcasts( false ),
	m_bBroadcasting( false )
{
	;
}

CMatchEventsSubscription::~CMatchEventsSubscription()
{
	;
}

static CMatchEventsSubscription g_MatchEventsSubscription;
CMatchEventsSubscription *g_pMatchEventsSubscription = &g_MatchEventsSubscription;

//
// Implementation
//

void CMatchEventsSubscription::Subscribe( IMatchEventsSink *pSink )
{
	if ( !pSink )
		return;

	int idx = m_arrSinks.Find( pSink );
	if ( m_arrSinks.IsValidIndex( idx ) )
	{
		Assert( m_arrRefCount.IsValidIndex( idx ) );
		Assert( m_arrRefCount[ idx ] > 0 );
		
		++ m_arrRefCount[ idx ];
	}
	else
	{
		m_arrSinks.AddToTail( pSink );
		m_arrRefCount.AddToTail( 1 );
		
		Assert( m_arrSinks.Count() == m_arrRefCount.Count() );
	}
}

void CMatchEventsSubscription::Unsubscribe( IMatchEventsSink *pSink )
{
	if ( !pSink )
		return;

	int idx = m_arrSinks.Find( pSink );
	Assert( m_arrSinks.IsValidIndex( idx ) );
	if ( !m_arrSinks.IsValidIndex( idx ) )
		return;

	Assert( m_arrRefCount.IsValidIndex( idx ) );
	Assert( m_arrRefCount[ idx ] > 0 );

	-- m_arrRefCount[ idx ];
	if ( m_arrRefCount[ idx ] <= 0 )
	{
		m_arrSinks.Remove( idx );
		m_arrRefCount.Remove( idx );

		Assert( m_arrSinks.Count() == m_arrRefCount.Count() );

		// Update outstanding iterators that are beyond removal point
		for ( int k = 0; k < m_arrIteratorsOutstanding.Count(); ++ k )
		{
			if ( m_arrIteratorsOutstanding[k] >= idx )
				-- m_arrIteratorsOutstanding[k];
		}
	}
}

void CMatchEventsSubscription::Shutdown()
{
	m_bBroadcasting = true;	// Blocks all BroadcastEvent calls from being dispatched!
}

void CMatchEventsSubscription::BroadcastEvent( KeyValues *pEvent )
{
	//
	// Network raw packet decryption
	//
	if ( !Q_stricmp( "OnNetLanConnectionlessPacket", pEvent->GetName() ) )
	{
		if ( void * pRawPacket = pEvent->GetPtr( "rawpkt" ) )
		{
			netpacket_t *pkt = ( netpacket_t * ) pRawPacket;
			pEvent->deleteThis();
			
			g_pConnectionlessLanMgr->ProcessConnectionlessPacket( pkt );
			return;
		}
	}

	//
	// Broadcasting events is reliable even when subscribers get added
	// or removed during broadcasts, or even broadcast nested events.
	//
	// Nested broadcasts are not allowed because it messes up the perception
	// of event timeline by external listeners, nested broadcast is enqueued
	// instead and broadcast after the original event has been broadcast to
	// all subscribers.
	//

	if ( m_bBroadcasting )
	{
		if ( !m_bAllowNestedBroadcasts )
		{
			m_arrQueuedEvents.AddToTail( pEvent );
			return;
		}
	}

	m_bBroadcasting = true;

	KeyValues::AutoDelete autoDeleteEvent( pEvent );
	m_arrSentEvents.AddToHead( pEvent );

	// Internally events are cracked before external subscribers
	g_pMMF->OnEvent( pEvent );

	// iterate subscribers
	for ( m_arrIteratorsOutstanding.AddToTail( 0 );
		  m_arrIteratorsOutstanding.Tail() < m_arrSinks.Count();
		  ++ m_arrIteratorsOutstanding.Tail() )
	{
		int i = m_arrIteratorsOutstanding.Tail();
		IMatchEventsSink *pSink = m_arrSinks[ i ];
		
		Assert( m_arrRefCount.IsValidIndex( i ) );
		Assert( m_arrRefCount[i] > 0 );
		Assert( pSink );

		pSink->OnEvent( pEvent );
	}
	m_arrIteratorsOutstanding.RemoveMultipleFromTail( 1 );

	m_bBroadcasting = false;

	//
	// Broadcast queued events
	//
	if ( m_arrQueuedEvents.Count() > 0 )
	{
		KeyValues *pQueued = m_arrQueuedEvents.Head();
		m_arrQueuedEvents.RemoveMultipleFromHead( 1 );
		BroadcastEvent( pQueued );
		return;
	}

	//
	// No more queued events left, clean up registered event data
	//
	for ( int k = 0; k < m_arrEventData.Count(); ++ k )
	{
		KeyValues *pRegistered = m_arrEventData[k];
		pRegistered->deleteThis();
	}
	m_arrEventData.Purge();
	m_arrSentEvents.Purge();
}

void CMatchEventsSubscription::RegisterEventData( KeyValues *pEventData )
{
	Assert( pEventData );
	if ( !pEventData )
		return;

	Assert( m_bBroadcasting );

	char const *szEventDataKey = pEventData->GetName();
	Assert( szEventDataKey && *szEventDataKey );
	if ( !szEventDataKey || !*szEventDataKey )
		return;

	for ( int k = 0; k < m_arrEventData.Count(); ++ k )
	{
		KeyValues *&pRegistered = m_arrEventData[k];
		if ( !Q_stricmp( szEventDataKey, pRegistered->GetName() ) )
		{
			pRegistered->deleteThis();
			pRegistered = pEventData;
			return;
		}
	}

	m_arrEventData.AddToTail( pEventData );
}

KeyValues * CMatchEventsSubscription::GetEventData( char const *szEventDataKey )
{
	Assert( m_bBroadcasting );

	for ( int k = 0; k < m_arrEventData.Count(); ++ k )
	{
		KeyValues *pRegistered = m_arrEventData[k];
		if ( !Q_stricmp( szEventDataKey, pRegistered->GetName() ) )
			return pRegistered;
	}

	for ( int k = 0; k < m_arrSentEvents.Count(); ++ k )
	{
		KeyValues *pRegistered = m_arrSentEvents[k];
		if ( !Q_stricmp( szEventDataKey, pRegistered->GetName() ) )
			return pRegistered;
	}

	return NULL;
}


