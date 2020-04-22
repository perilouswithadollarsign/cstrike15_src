//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Forward server log lines to remote listeners. Holds all log lines
// for this map, up to a maximum. 
//
//=============================================================================//

#pragma once

class CServerLogHTTPDispatcher : public CAutoGameSystemPerFrame
{
public:
	CServerLogHTTPDispatcher();
	bool LogForHTTPListeners( const char* szLogLine );
	void AddListener( const char* szURI );
	void RemoveAllListeners( void );
	void DumpListenersToConsole( void ) const;

	// base class overrides
	virtual void Shutdown() OVERRIDE;
	virtual void LevelInitPreEntity() OVERRIDE;
	virtual void LevelShutdownPreEntity() OVERRIDE;

	// Dispatch to listeners that need updates
	// Runs once per server frame, even if server is paused (ie not ticking)
	virtual void PreClientUpdate() OVERRIDE;

	struct LogLineStartForTick_t
	{
		LogLineStartForTick_t( int32 _iTick, size_t _unOffsetStart, size_t _unLength ) :
			iTick( _iTick ), unOffsetStart( _unOffsetStart ), unLength( _unLength ) { }

		int32 iTick;
		size_t unOffsetStart; // Offset from start of m_strServerLog storage
		size_t unLength; // Length of lines for this tick in characters
	};
	typedef CUtlLinkedList< LogLineStartForTick_t > LogLinesList_t;

private:
	void Reset( void );
	void BuildTimestampString( void );

	CUtlVector< class CServerLogDestination* > m_vecListeners; // addresses requesting the server log with server tick they last ack'd

	LogLinesList_t m_llLogPerTick; // Linked list of log lines recorded for a tick. Has offsets in to m_strServerLog storage to find the start of the lines.
	CUtlStringBuilder m_strLogLinesThisTick; // Log lines being gathered this tick

	CUtlStringBuilder m_strServerLog; // String of all log lines since last map transition. 

#if defined DBGFLAG_ASSERT // DBG only safety check, nobody should be editing the list while we still have listeners holding on to LL handles
	LogLinesList_t::IndexType_t m_dbgLastAllocedHandle;
#endif 
	time_t	m_localTimeLoggingStart;
	double m_loggingStartPlatTime;
	CUtlString m_strTimeStampPrefix;

};

CServerLogHTTPDispatcher* GetServerLogHTTPDispatcher( void );