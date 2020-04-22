//========= Copyright © , Valve Corporation, All rights reserved. ============//

#ifndef DBGINPUT_HDR
#define DBGINPUT_HDR

#include "threadtools.h"
#ifdef _PS3
#include "sys/tty.h"
#endif

class CDebugInputThread: public CThread
{
public:
	CThreadMutex m_mx;
	CUtlString m_inputString;
	bool m_bStop;
	
	CDebugInputThread()
	{
		m_bStop = false;
	}
	~CDebugInputThread()
	{
	
	}
	
	void Stop()
	{
		m_bStop = true;
		CThread::Stop();
	}
	
	virtual int Run( void )
	{
#ifdef _PS3	
		char buf[1000];
		uint read;
		while( !m_bStop && CELL_OK == sys_tty_read( SYS_TTYP3 , buf, sizeof(buf) - 1, &read ) )
		{
			m_mx.Lock();
			buf[ MIN( read, sizeof( buf ) - 1 ) ] = '\0';
			m_inputString = buf;
			m_mx.Unlock();
		}
#endif
		return 0;
	}
};

extern CDebugInputThread * g_pDebugInputThread;

#endif
