//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef MM_EVENTS_H
#define MM_EVENTS_H
#ifdef _WIN32
#pragma once
#endif

#include "mm_framework.h"
#include "utlvector.h"

class CMatchEventsSubscription : public IMatchEventsSubscription
{
	// Methods of IMatchEventsSubscription
public:
	virtual void Subscribe( IMatchEventsSink *pSink );
	virtual void Unsubscribe( IMatchEventsSink *pSink );

	virtual void BroadcastEvent( KeyValues *pEvent );

	virtual void RegisterEventData( KeyValues *pEventData );
	virtual KeyValues * GetEventData( char const *szEventDataKey );

public:
	bool IsBroacasting() const { return m_bBroadcasting; }
	void Shutdown();

public:
	CMatchEventsSubscription();
	~CMatchEventsSubscription();

protected:
	CUtlVector< IMatchEventsSink * > m_arrSinks;
	CUtlVector< int > m_arrRefCount;
	CUtlVector< int > m_arrIteratorsOutstanding;

	bool m_bBroadcasting;
	bool m_bAllowNestedBroadcasts;
	CUtlVector< KeyValues * > m_arrQueuedEvents;
	CUtlVector< KeyValues * > m_arrEventData;
	CUtlVector< KeyValues * > m_arrSentEvents;
};

// Match events subscription singleton
extern CMatchEventsSubscription *g_pMatchEventsSubscription;

#endif // MM_EVENTS_H
