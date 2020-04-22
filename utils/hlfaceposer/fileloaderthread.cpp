//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#include "tier0/platform.h"
#include "cbase.h"
#include "sentence.h"
#include "wavefile.h"
#include "tier2/riff.h"
#include "filesystem.h"
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include "IFileLoader.h"

bool SceneManager_LoadSentenceFromWavFileUsingIO( char const *wavfile, CSentence& sentence, IFileReadBinary& io );

//-----------------------------------------------------------------------------
// Purpose: Implements the RIFF i/o interface on stdio
//-----------------------------------------------------------------------------
class ThreadIOReadBinary : public IFileReadBinary
{
public:
	FileHandle_t open( const char *pFileName )
	{
		char filename[ 512 ];
		// POSSIBLE BUG:  THIS MIGHT NOT BE THREAD SAFE!!!
		filesystem->RelativePathToFullPath( pFileName, "GAME", filename, sizeof( filename ) );
		return (FileHandle_t)_open( filename, _O_BINARY | _O_RDONLY );
	}

	int read( void *pOutput, int size, FileHandle_t file )
	{
		if ( !file )
			return 0;

		return _read( (int)(intp)file, pOutput, size );
	}

	void seek( FileHandle_t file, int pos )
	{
		if ( !file )
			return;

		_lseek( (int)(intp)file, pos, SEEK_SET );
	}

	unsigned int tell( FileHandle_t file )
	{
		if ( !file )
			return 0;

		return _tell( (int)(intp)file );
	}

	unsigned int size( FileHandle_t file )
	{
		if ( !file )
			return 0;

		long curpos = this->tell( file );
		_lseek( (int)(intp)file, 0, SEEK_END );
		int s = this->tell( file );
		_lseek( (int)(intp)file, curpos, SEEK_SET );

		return s;
	}

	void close( FileHandle_t file )
	{
		if ( !file )
			return;

		_close( (int)(intp)file );
	}
};

//-----------------------------------------------------------------------------
// Purpose: All wavefile I/O occurs on a thread
//-----------------------------------------------------------------------------
class CFileLoaderThread : public IFileLoader
{
public:
	struct SentenceRequest
	{
		SentenceRequest()
		{
			filename[ 0 ] = 0;
			sentence.Reset();
			wavefile = NULL;
			valid	= false;
		}

		bool		valid;
		char		filename[ 256 ];
		CSentence	sentence;

		CWaveFile	*wavefile;
	};

	// Construction
							CFileLoaderThread( void );
	virtual 				~CFileLoaderThread( void );

	// Sockets add/remove themselves via their constructor
	virtual void			AddWaveFilesToThread( CUtlVector< CWaveFile * >& wavefiles );

	// Retrieve handle to shutdown event
	virtual HANDLE			GetShutdownHandle( void );

	// Caller should call lock before accessing any of these methods and unlock afterwards!!!
	virtual	int				ProcessCompleted();

	int						DoThreadWork();

	virtual	void			Start();

	virtual int				GetPendingLoadCount();
private:
	// Critical section used for synchronizing access to wavefile list
	CThreadFastMutex		m_Mutex;

	// List of wavefiles we are listening on
	CUtlVector< SentenceRequest	* > m_FileList;

	CUtlVector< SentenceRequest * > m_Pending;
	CUtlVector< SentenceRequest	* > m_Completed;
	// Thread handle
	HANDLE					m_hThread;
	// Thread id
	DWORD					m_nThreadId;
	// Event to set when we want to tell the thread to shut itself down
	HANDLE					m_hShutdown;

	ThreadIOReadBinary		m_ThreadIO;

	int						m_nTotalAdds;
	int						m_nTotalCompleted;

	CInterlockedInt			m_nTotalPending;
	CInterlockedInt			m_nTotalProcessed;

	HANDLE					m_hNewItems;
};

// Singleton handler
static CFileLoaderThread g_WaveLoader;
extern IFileLoader *fileloader = &g_WaveLoader;

int CFileLoaderThread::DoThreadWork()
{
	int i;
	// Check for shutdown event
	if ( WAIT_OBJECT_0 == WaitForSingleObject( GetShutdownHandle(), 0 ) )
	{
		return 0;
	}

	// No changes to list right now
	{
		AUTO_LOCK_FM( m_Mutex );
		// Move new items to work list
		int newItems = m_FileList.Count();
		for ( i = 0; i < newItems; i++ )
		{
			// Move to pending and issue async i/o calls
			m_Pending.AddToHead( m_FileList[ i ] );

			m_nTotalPending++;
		}
		m_FileList.RemoveAll();
		// Done adding new work items
	}

	int remaining = m_Pending.Count();
	if ( !remaining )
		return 1;

	int workitems = remaining; // min( remaining, 1000 );

	CUtlVector< SentenceRequest * > transfer;

	for ( i = 0; i < workitems; i++ )
	{
		SentenceRequest *r = m_Pending[ 0 ];
		m_Pending.Remove( 0 );

		transfer.AddToTail( r );
		
		// Do the work
		m_nTotalProcessed++;

		bool load = false;
		{
			AUTO_LOCK_FM( m_Mutex );
			load = !r->wavefile->HasLoadedSentenceInfo();
		}
		
		if ( load )
		{
			r->valid = SceneManager_LoadSentenceFromWavFileUsingIO( r->filename, r->sentence, m_ThreadIO );
		}
		else
		{
			r->valid = true;
		}

		if ( WaitForSingleObject( m_hNewItems, 0 ) == WAIT_OBJECT_0 )
		{
			ResetEvent( m_hNewItems );
			break;
		}
	}

	// Now move to completed list
	{
		AUTO_LOCK_FM( m_Mutex );
		int c = transfer.Count();

		for ( i = 0; i < c; ++i )
		{
			SentenceRequest *r = transfer[ i ];
			if ( r->valid )
			{
			
				m_nTotalCompleted++;
				

				m_Completed.AddToTail( r );
			}
			else
			{
				delete r;
			}
		}
	}
	return 1;
}

int CFileLoaderThread::ProcessCompleted()
{
	AUTO_LOCK_FM( m_Mutex );
	int c = m_Completed.Count();
	for ( int i = c - 1; i >= 0 ; i-- )
	{
		SentenceRequest *r = m_Completed[ i ];

		if ( !r->wavefile->HasLoadedSentenceInfo() )
		{
			r->wavefile->SetThreadLoadedSentence( r->sentence );
		}

		delete r;
	}
	m_Completed.RemoveAll();
	return c;
}


//-----------------------------------------------------------------------------
// Purpose: Main winsock processing thread
// Input  : threadobject - 
// Output : static DWORD WINAPI
//-----------------------------------------------------------------------------
static DWORD WINAPI FileLoaderThreadFunc( LPVOID threadobject )
{
	// Get pointer to CFileLoaderThread object
	CFileLoaderThread *wavefilethread = ( CFileLoaderThread * )threadobject;
	Assert( wavefilethread );
	if ( !wavefilethread )
	{
		return 0;
	}

	// Keep looking for data until shutdown event is triggered
	while ( 1 )
	{
		if( !wavefilethread->DoThreadWork() )
			break;

		// Yield a small bit of time to main app
		Sleep( 10 );
	}

	ExitThread( 0 );

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Construction
//-----------------------------------------------------------------------------
CFileLoaderThread::CFileLoaderThread( void )
{
	m_nTotalAdds = 0;
	m_nTotalProcessed = 0;
	m_nTotalCompleted = 0;
	m_nTotalPending = 0;

	m_hShutdown	= CreateEvent( NULL, TRUE, FALSE, NULL );
	Assert( m_hShutdown );

	m_hThread = NULL;

	m_hNewItems = CreateEvent( NULL, TRUE, FALSE, NULL );

	Start();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFileLoaderThread::Start()
{
	m_hThread = CreateThread( NULL, 0, FileLoaderThreadFunc, (void *)this, 0, &m_nThreadId );
	Assert( m_hThread );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFileLoaderThread::~CFileLoaderThread( void )
{
	{
		AUTO_LOCK_FM( m_Mutex );
		SetEvent( m_hShutdown );
		Sleep( 2 );
		TerminateThread( m_hThread, 0 );
	}

	// Kill the wavefile
//!! need to validate this line
//	Assert( !m_FileList );

	CloseHandle( m_hThread );

	CloseHandle( m_hShutdown );

	CloseHandle( m_hNewItems );
}

//-----------------------------------------------------------------------------
// Purpose: Returns handle of shutdown event
// Output : HANDLE
//-----------------------------------------------------------------------------
HANDLE CFileLoaderThread::GetShutdownHandle( void )
{
	return m_hShutdown;
}

//-----------------------------------------------------------------------------
// Purpose: Locks object and adds wavefile to thread
// Input  : *wavefile - 
//-----------------------------------------------------------------------------
void CFileLoaderThread::AddWaveFilesToThread( CUtlVector< CWaveFile * >& wavefiles )
{
	AUTO_LOCK_FM( m_Mutex );
	int c = wavefiles.Count();
	for ( int i = 0; i < c; i++ )
	{
		SentenceRequest *request = new SentenceRequest;
		request->wavefile = wavefiles[ i ];
		Q_strncpy( request->filename, request->wavefile->GetFileName(), sizeof( request->filename ) );
	
		m_FileList.AddToTail( request );

		m_nTotalAdds++;
	}

	SetEvent( m_hNewItems );
}

int CFileLoaderThread::GetPendingLoadCount()
{
	return m_nTotalPending - m_nTotalProcessed;
}
