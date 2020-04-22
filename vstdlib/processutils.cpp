//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#if !defined( _X360 )
#include <windows.h>
#endif
#include "vstdlib/iprocessutils.h"
#include "tier1/utllinkedlist.h"
#include "tier1/utlstring.h"
#include "tier1/utlbuffer.h"
#include "tier1/tier1.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


class CProcess;

class CProcessPipeRead : public IPipeRead
{
// IPipeRead overrides.
public:
	virtual int GetNumBytesAvailable();
	virtual void ReadAvailable( CUtlString &sStr, int nMaxBytes );
	virtual void ReadAvailable( CUtlBuffer *pOutBuffer, int nMaxBytes );
	virtual void Read( CUtlString &sStr, int nMaxBytes );
	virtual void ReadLine( CUtlString &sStr );

public:
	bool IsValid() const;

	// This reads in whatever data is available in the pipe and caches it.
	void CacheOutput();

	// Returns true when there is data in m_hRead, false otherwise.
	bool WaitForOutput();

	int GetActualProcessOutputSize();
	int GetProcessOutput( void *pBuf, int nBytes );
	int GetActualProcessOutput( void *pBuf, int nBufLen );

public:
	CProcess *m_pProcess;

	HANDLE m_hRead;	// Comes from ProcessInfo_t::m_hChildStdoutRd or m_hChildStderrRd.
	
	// This is stored if they wait for the process to exit. Even though they haven't read the data yet,
	// they can still call ReadAvailable() or GetNumBytesAvailable() on stdio after the process was terminated.
	CUtlBuffer m_CachedOutput;
};


struct ProcessInfo_t
{
	HANDLE m_hChildStdinRd;
	HANDLE m_hChildStdinWr;
	
	HANDLE m_hChildStdoutRd;
	HANDLE m_hChildStdoutWr;

	HANDLE m_hChildStderrRd;
	HANDLE m_hChildStderrWr;

	HANDLE m_hProcess;
	CUtlString m_CommandLine;
	int m_fFlags;	// PROCESSSTART_xxx.
};

class CProcessUtils;

class CProcess : public IProcess
{
public:
	CProcess( CProcessUtils *pProcessUtils, const ProcessInfo_t &info );

// IProcess overrides.
public:
	virtual void Release();
	virtual void Abort();
	virtual bool IsComplete();
	virtual int WaitUntilComplete();

	virtual int WriteStdin( char *pBuf, int nBufLen );
	virtual IPipeRead* GetStdout();
	virtual IPipeRead* GetStderr();

	virtual int GetExitCode();

public:
	ProcessInfo_t m_Info;

	CProcessPipeRead m_StdoutRead;
	CProcessPipeRead m_StderrRead;

	CProcessUtils *m_pProcessUtils;
	intp m_nProcessesIndex;	// Index into CProcessUtils::m_Processes.
};


//-----------------------------------------------------------------------------
// At the moment, we can only run one process at a time 
//-----------------------------------------------------------------------------
class CProcessUtils : public CTier1AppSystem< IProcessUtils >
{
	typedef CTier1AppSystem< IProcessUtils > BaseClass;

public:
	CProcessUtils() {}

	// Inherited from IAppSystem
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// Inherited from IProcessUtils
	virtual IProcess* StartProcess( const char *pCommandLine, int fFlags, const char *pWorkingDir );
	virtual IProcess* StartProcess( int argc, const char **argv, int fFlags, const char *pWorkingDir );
	virtual int SimpleRunProcess( const char *pCommandLine, const char *pWorkingDir, CUtlString *pStdout );

public:
	void OnProcessDelete( CProcess *pProcess );

private:

	// creates the process, adds it to the list and writes the windows HANDLE into info.m_hProcess
	CProcess* CreateProcess( ProcessInfo_t &info, int fFlags, const char *pWorkingDir );

	CUtlFixedLinkedList< CProcess* >	m_Processes;
	bool m_bInitialized;
};


//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
static CProcessUtils s_ProcessUtils;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CProcessUtils, IProcessUtils, PROCESS_UTILS_INTERFACE_VERSION, s_ProcessUtils );


//-----------------------------------------------------------------------------
// Returns the last error that occurred
//-----------------------------------------------------------------------------
char *GetErrorString( char *pBuf, int nBufLen )
{
	FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, pBuf, nBufLen, NULL );
	char *p = strchr(pBuf, '\r');	// get rid of \r\n
	if(p) 
	{
		p[0] = 0;
	}
	return pBuf;
}


// ------------------------------------------------------------------------------------- //
// CProcessPipeRead implementation.
// ------------------------------------------------------------------------------------- //

bool CProcessPipeRead::IsValid() const
{
	return m_hRead != INVALID_HANDLE_VALUE;
}

int CProcessPipeRead::GetNumBytesAvailable()
{
	return GetActualProcessOutputSize() + m_CachedOutput.TellPut();
}

void CProcessPipeRead::ReadAvailable( CUtlString &sStr, int32 nMaxBytes )
{
	int nBytesAvailable = GetNumBytesAvailable();
	nBytesAvailable = MIN( nBytesAvailable, nMaxBytes );

	sStr.SetLength( nBytesAvailable );	// If nBytesAvailable != 0, this auto allocates an extra byte for the null terminator.
	if ( nBytesAvailable > 0 )
	{
		char *pOut = sStr.Get();
		int nBytesGotten = GetProcessOutput( pOut, nBytesAvailable );
		Assert( nBytesGotten == nBytesAvailable );

		// Null-terminate it.
		pOut[nBytesGotten] = 0;
	}
}

void CProcessPipeRead::ReadAvailable( CUtlBuffer *pOutBuffer, int nMaxBytes )
{
	int nBytesAvailable = GetNumBytesAvailable();
	nBytesAvailable = MIN( nBytesAvailable, nMaxBytes );

	if ( nBytesAvailable > 0 )
	{
		char *pOut = (char*)pOutBuffer->AccessForDirectRead(nBytesAvailable+1);
		int nBytesGotten = GetProcessOutput( pOut, nBytesAvailable );
		Assert( nBytesGotten == nBytesAvailable );

		// Null-terminate it.
		pOut[nBytesGotten] = 0;
	}
}



void CProcessPipeRead::Read( CUtlString &sStr, int32 nBytes )
{
	sStr.SetLength( 0 );

	int nBytesLeftToRead = nBytes;
	while ( nBytesLeftToRead > 0 )
	{
		int nAvail = GetNumBytesAvailable();
		if ( nAvail == 0 )
		{
			if ( m_pProcess->IsComplete() )
			{
				return;
			}
			else
			{
				WaitForOutput();
				continue;
			}
		}

		int nToRead = MIN( nBytesLeftToRead, nAvail );

		// Read as much as we need and add it to the string.
		CUtlString sTemp;
		ReadAvailable( sTemp, nToRead );
		Assert( sTemp.Length() == nToRead );
		sStr += sTemp;

		nBytesLeftToRead -= nToRead;
	}
}

void CProcessPipeRead::ReadLine( CUtlString &sStr )
{
	sStr.SetLength( 0 );
	while ( 1 )
	{
		// Wait for output if there's nothing left in our cached output.
		if ( m_CachedOutput.GetBytesRemaining() == 0 && !WaitForOutput() )
			return;

		CacheOutput();

		char *pStr = (char*)m_CachedOutput.PeekGet();
		int nBytes = m_CachedOutput.GetBytesRemaining();
		
		// No more stuff available and the process is dead?
		if ( nBytes == 0 && m_pProcess->IsComplete() )
			break;

		int i;
		for ( i=0; i < nBytes; i++ )
		{
			if ( pStr[i] == '\n' )
				break;
		}

		if ( i < nBytes )
		{
			// We hit a line ending.
			int nBytesToRead = i;
			if ( nBytesToRead > 0 && pStr[nBytesToRead-1] == '\r' )
				--nBytesToRead;

			CUtlString sTemp;
			sTemp.SetDirect( pStr, nBytesToRead );
			sStr += sTemp;

			m_CachedOutput.SeekGet( CUtlBuffer::SEEK_CURRENT, i+1 );	// Use i here because it'll be at the \n, not the \r\n.
			if ( m_CachedOutput.GetBytesRemaining() == 0 )
				m_CachedOutput.Purge();

			return;
		}
	}
}

bool CProcessPipeRead::WaitForOutput()
{
	if ( m_hRead == INVALID_HANDLE_VALUE )
	{
		Assert( false );
		return false;
	}

	while ( GetActualProcessOutputSize() == 0 )
	{
		if ( m_pProcess->IsComplete() )
			return false;
		else
			ThreadSleep( 1 );
	}

	return true;
}

void CProcessPipeRead::CacheOutput()
{
	int nBytes = GetActualProcessOutputSize();
	if ( nBytes == 0 )
		return;

	int nPut = m_CachedOutput.TellPut();
	m_CachedOutput.EnsureCapacity( nPut + nBytes );
	
	int nBytesRead = GetActualProcessOutput( (char*)m_CachedOutput.PeekPut(), nBytes );
	Assert( nBytesRead == nBytes );

	m_CachedOutput.SeekPut( CUtlBuffer::SEEK_HEAD, nPut + nBytesRead );
}

//-----------------------------------------------------------------------------
// Methods used to read	output back from a process
//-----------------------------------------------------------------------------
int CProcessPipeRead::GetActualProcessOutputSize()
{
	if ( m_hRead == INVALID_HANDLE_VALUE )
	{
		Assert( false );
		return 0;
	}

	DWORD dwCount = 0;
	if ( !PeekNamedPipe( m_hRead, NULL, NULL, NULL, &dwCount, NULL ) )
	{
		char buf[ 512 ];
		Warning( "Could not read from pipe associated with command %s\n"
			"Windows gave the error message:\n   \"%s\"\n",
			m_pProcess->m_Info.m_CommandLine.Get(), GetErrorString( buf, sizeof(buf) ) );
		return 0;
	}

	return (int)dwCount;
}

int CProcessPipeRead::GetProcessOutput( void *pBuf, int nBytes )
{
	int nCachedBytes = MIN( nBytes, m_CachedOutput.TellPut() );
	int nPipeBytes = nBytes - nCachedBytes;

	// Read from the cached buffer.
	m_CachedOutput.Get( pBuf, nCachedBytes );
	if ( m_CachedOutput.GetBytesRemaining() == 0 )
	{
		m_CachedOutput.Purge();
	}

	// Read from the pipe.
	int nPipedBytesRead = GetActualProcessOutput( (char*)pBuf + nCachedBytes, nPipeBytes );
	return nCachedBytes + nPipedBytesRead;
}

int CProcessPipeRead::GetActualProcessOutput( void *pBuf, int nBufLen )
{
	if ( m_hRead == INVALID_HANDLE_VALUE )
	{
		Assert( false );
		return 0;
	}

	// ReadFile can deadlock in a blaze of awesomeness if you ask for 0 bytes.
	if ( nBufLen == 0 )
		return 0;

	DWORD nBytesRead = 0;
	BOOL bSuccess = ReadFile( m_hRead, pBuf, nBufLen, &nBytesRead, NULL );

	if ( bSuccess )
	{
		// ReadFile -should- block until it gets the number of bytes requested OR until
		// the process is complete. So if it didn't return the # of bytes requested,
		// then make sure the process is complete.
		if ( (int)nBytesRead != nBufLen )
		{
			Assert( m_pProcess->IsComplete() );
		}
	}
	else
	{
		Assert( false );
	}

	return (int)nBytesRead;
}


// ------------------------------------------------------------------------------------- //
// CProcess implementation.
// ------------------------------------------------------------------------------------- //

CProcess::CProcess( CProcessUtils *pProcessUtils, const ProcessInfo_t &info )
{
	m_pProcessUtils = pProcessUtils;
	m_Info = info;
	
	m_StdoutRead.m_pProcess = this;
	m_StdoutRead.m_hRead = info.m_hChildStdoutRd;

	m_StderrRead.m_pProcess = this;
	m_StderrRead.m_hRead = info.m_hChildStderrRd;
}


void CProcess::Release()
{
	if ( !( m_Info.m_fFlags & STARTPROCESS_NOAUTOKILL ) )
	{
		Abort();
	}

	ProcessInfo_t& info = m_Info;
	CloseHandle( info.m_hChildStderrRd );
	CloseHandle( info.m_hChildStderrWr );
	CloseHandle( info.m_hChildStdinRd );
	CloseHandle( info.m_hChildStdinWr );
	CloseHandle( info.m_hChildStdoutRd );
	CloseHandle( info.m_hChildStdoutWr );

	m_pProcessUtils->OnProcessDelete( this );
	delete this;
}

void CProcess::Abort()
{
	if ( !IsComplete() )
	{
		TerminateProcess( m_Info.m_hProcess, 1 );
	}
}

bool CProcess::IsComplete()
{
	HANDLE h = m_Info.m_hProcess;
	return ( WaitForSingleObject( h, 0 ) != WAIT_TIMEOUT );
}

int CProcess::WaitUntilComplete()
{
	ProcessInfo_t &info = m_Info;

	if ( info.m_hChildStdoutRd == INVALID_HANDLE_VALUE )
	{
		WaitForSingleObject( info.m_hProcess, INFINITE );
	}
	else
	{
		// NOTE: The called process can block during writes to stderr + stdout
		// if the pipe buffer is empty. Therefore, waiting INFINITE is not
		// possible here. We must queue up messages received to allow the 
		// process to continue
		while ( WaitForSingleObject( info.m_hProcess, 50 ) == WAIT_TIMEOUT )
		{
			m_StdoutRead.CacheOutput();
			if ( m_StderrRead.IsValid() )
				m_StderrRead.CacheOutput();
		}
	}

	return GetExitCode();
}

int CProcess::WriteStdin( char *pBuf, int nBufLen )
{
	ProcessInfo_t& info = m_Info;
	if ( info.m_hChildStdinWr == INVALID_HANDLE_VALUE )
	{
		Assert( false );
		return 0;
	}

	DWORD nBytesWritten = 0;
	if ( WriteFile( info.m_hChildStdinWr, pBuf, nBufLen, &nBytesWritten, NULL ) )
		return (int)nBytesWritten;
	else
		return 0;
}

IPipeRead* CProcess::GetStdout()
{
	return &m_StdoutRead;
}

IPipeRead* CProcess::GetStderr()
{
	return &m_StderrRead;
}

int CProcess::GetExitCode()
{
	ProcessInfo_t &info = m_Info;
	DWORD nExitCode;
	BOOL bOk = GetExitCodeProcess( info.m_hProcess, &nExitCode );
	if ( !bOk || nExitCode == STILL_ACTIVE )
		return -1;
	
	return nExitCode;
}


//-----------------------------------------------------------------------------
// Initialize, shutdown process system
//-----------------------------------------------------------------------------
InitReturnVal_t CProcessUtils::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	m_bInitialized = true;
	return INIT_OK;
}

void CProcessUtils::Shutdown()
{
	Assert( m_bInitialized );
	Assert( m_Processes.Count() == 0 );
	
	// Delete any lingering processes.
	while ( m_Processes.Count() > 0 )
	{
		m_Processes[ m_Processes.Head() ]->Release();
	}

	m_bInitialized = false;
	return BaseClass::Shutdown();
}


void CProcessUtils::OnProcessDelete( CProcess *pProcess )
{
	m_Processes.Remove( pProcess->m_nProcessesIndex );
}

CProcess *CProcessUtils::CreateProcess( ProcessInfo_t &info, int fFlags, const char *pWorkingDir )
{
	STARTUPINFO si;
	memset(&si, 0, sizeof si);
	si.cb = sizeof(si);
	if ( fFlags & STARTPROCESS_CONNECTSTDPIPES )
	{
		si.dwFlags = STARTF_USESTDHANDLES;
		si.hStdInput = info.m_hChildStdinRd;
		si.hStdError = info.m_hChildStderrWr;
		si.hStdOutput = info.m_hChildStdoutWr;
	}

	DWORD dwCreateProcessFlags = 0;
	if ( !( fFlags & STARTPROCESS_SHARE_CONSOLE ) )
	{
		dwCreateProcessFlags |= DETACHED_PROCESS;
	}

	PROCESS_INFORMATION pi;
	if ( ::CreateProcess( NULL, info.m_CommandLine.Get(), NULL, NULL, TRUE, dwCreateProcessFlags, NULL, pWorkingDir, &si, &pi ) )
	{
		info.m_hProcess = pi.hProcess;

		CProcess *pProcess = new CProcess( this, info );
		
		pProcess->m_nProcessesIndex = m_Processes.AddToTail( pProcess );
		return pProcess;
	}

	char buf[ 512 ];
	Warning( "Could not execute the command:\n   %s\n"
		"Windows gave the error message:\n   \"%s\"\n",
		info.m_CommandLine.Get(), GetErrorString( buf, sizeof(buf) ) );

	return NULL;
}

//-----------------------------------------------------------------------------
// Options for compilation
//-----------------------------------------------------------------------------
IProcess* CProcessUtils::StartProcess( const char *pCommandLine, int fFlags, const char *pWorkingDir )
{
	Assert( m_bInitialized );

	ProcessInfo_t info;
	info.m_CommandLine = pCommandLine;

	if ( !(fFlags & STARTPROCESS_CONNECTSTDPIPES) )
	{
		info.m_hChildStderrRd = info.m_hChildStderrWr = INVALID_HANDLE_VALUE;
		info.m_hChildStdinRd = info.m_hChildStdinWr = INVALID_HANDLE_VALUE;
		info.m_hChildStdoutRd = info.m_hChildStdoutWr = INVALID_HANDLE_VALUE;

		return CreateProcess( info, fFlags, pWorkingDir );
	}

    SECURITY_ATTRIBUTES saAttr; 

    // Set the bInheritHandle flag so pipe handles are inherited.
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 


	DWORD nPipeSize = 0; // default size

	if ( fFlags & STARTPROCESS_FATPIPES )
	{
		nPipeSize = 1024*1024;
	}

    // Create a pipe for the child's STDOUT. 
    if ( CreatePipe( &info.m_hChildStdoutRd, &info.m_hChildStdoutWr, &saAttr, nPipeSize ) )
	{
		if ( CreatePipe( &info.m_hChildStdinRd, &info.m_hChildStdinWr, &saAttr, nPipeSize ) )
		{
			BOOL bSuccess = false;
			if ( fFlags & STARTPROCESS_SEPARATE_STDERR )
			{
				bSuccess = CreatePipe( &info.m_hChildStderrRd, &info.m_hChildStderrWr, &saAttr, nPipeSize );
			}
			else
			{
				bSuccess = DuplicateHandle( GetCurrentProcess(), info.m_hChildStdoutWr, GetCurrentProcess(), 
					&info.m_hChildStderrWr, 0, TRUE, DUPLICATE_SAME_ACCESS );

				info.m_hChildStderrRd = INVALID_HANDLE_VALUE;
			}

			if ( bSuccess )
			{
				IProcess *pProcess = CreateProcess( info, fFlags, pWorkingDir );
				if ( pProcess )
					return pProcess;

				CloseHandle( info.m_hChildStderrRd );
				CloseHandle( info.m_hChildStderrWr );
			}
			CloseHandle( info.m_hChildStdinRd );
			CloseHandle( info.m_hChildStdinWr );
		}
		CloseHandle( info.m_hChildStdoutRd );
		CloseHandle( info.m_hChildStdoutWr );
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Start up a process
//-----------------------------------------------------------------------------
IProcess* CProcessUtils::StartProcess( int argc, const char **argv, int fFlags, const char *pWorkingDir )
{
	CUtlString commandLine;
	for ( int i = 0; i < argc; ++i )
	{
		commandLine += argv[i];
		if ( i != argc-1 )
		{
			commandLine += " ";
		}
	}
	return StartProcess( commandLine.Get(), fFlags, pWorkingDir );
}


int CProcessUtils::SimpleRunProcess( const char *pCommandLine, const char *pWorkingDir, CUtlString *pStdout )
{
	int nFlags = 0;
	if ( pStdout )
		nFlags |= STARTPROCESS_CONNECTSTDPIPES;

	IProcess *pProcess = StartProcess( pCommandLine, nFlags, pWorkingDir );
	if ( !pProcess )
		return -1;

	int nExitCode = pProcess->WaitUntilComplete();
	if ( pStdout )
	{
		pProcess->GetStdout()->Read( *pStdout );
	}
	
	pProcess->Release();
	return nExitCode;
}


// Translate '\r\n' to '\n'.
void TranslateLinefeedsToUnix( char *pBuffer )
{
	char *pOut = pBuffer;
	while ( *pBuffer )
	{
		if ( pBuffer[0] == '\r' && pBuffer[1] == '\n' )
		{
			*pOut = '\n';
			++pBuffer;
		}
		else
		{
			*pOut = *pBuffer;
		}
		++pBuffer;
		++pOut;
	}
	*pOut = 0;
}


