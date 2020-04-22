//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// sv_log.h
// Server logging functions

#ifndef SV_LOG_H
#define SV_LOG_H
#pragma once

#include <igameevents.h>
#include "netadr.h"

class CLog : public IGameEventListener2
{
public:
	CLog();
	virtual ~CLog();

public: // IGameEventListener Interface
	
	void FireGameEvent( IGameEvent *event );
	int  m_nDebugID;
	int	 GetEventDebugID( void );

public: 

	bool IsActive( void );	// true if logging is "on"
	void SetLoggingState( bool state );	// set the logging state to true (on) or false (off)
	
	bool UsingLogAddress( void );
	bool AddLogAddress( netadr_t addr, uint64 ullToken = 0ull );
	bool DelLogAddress( netadr_t addr );
	void DelAllLogAddress( void );
	void ListLogAddress( void );
	
	void Open( void );  // opens logging file
	void Close( void );	// closes logging file

	void Init( void );
	void Reset( void );	// reset all logging streams
	void Shutdown( void );
	
	void Printf( PRINTF_FORMAT_STRING const char *fmt, ... ) FMTFUNCTION( 2, 3 );	// log a line to log file
	void Print( const char * text );
	void PrintServerVars( void ); // prints server vars to log file

	void RunFrame();

private:

	bool m_bActive;		// true if we're currently logging

	struct LogAddressDestination_t
	{
		netadr_t m_adr;
		uint64 m_ullToken;
	};
	CUtlVector< LogAddressDestination_t >	m_LogAddrDestinations;		// Server frag log stream is sent to the address(es) in this list
	FileHandle_t			m_hLogFile;        // File where frag log is put.
	double					m_flLastLogFlush;
	bool					m_bFlushLog;
};

extern CLog g_Log;

#endif // SV_LOG_H
