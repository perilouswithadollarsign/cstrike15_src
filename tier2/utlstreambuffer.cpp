//====== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
// Serialization/unserialization buffer
//=============================================================================//

#include "tier2/utlstreambuffer.h"
#include "tier2/tier2.h"
#include "filesystem.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// default stream chunk size
//-----------------------------------------------------------------------------
enum
{
	DEFAULT_STREAM_CHUNK_SIZE = 16 * 1024
};


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CUtlStreamBuffer::CUtlStreamBuffer( ) : BaseClass( DEFAULT_STREAM_CHUNK_SIZE, DEFAULT_STREAM_CHUNK_SIZE, 0 )
{
	SetUtlBufferOverflowFuncs( &CUtlStreamBuffer::StreamGetOverflow, &CUtlStreamBuffer::StreamPutOverflow );
	m_hFileHandle = FILESYSTEM_INVALID_HANDLE;
	m_pFileName = NULL;
	m_pPath = NULL;
}

CUtlStreamBuffer::CUtlStreamBuffer( const char *pFileName, const char *pPath, int nFlags, bool bDelayOpen, int nOpenFileFlags ) :
	BaseClass( DEFAULT_STREAM_CHUNK_SIZE, DEFAULT_STREAM_CHUNK_SIZE, nFlags )
{
	if ( nFlags & TEXT_BUFFER )
	{
		Warning( "CUtlStreamBuffer does not support TEXT_BUFFER's use CUtlBuffer\n" );
		Assert( 0 );
		m_Error	|= FILE_OPEN_ERROR;
		return;
	}

	SetUtlBufferOverflowFuncs( &CUtlStreamBuffer::StreamGetOverflow, &CUtlStreamBuffer::StreamPutOverflow );

	if ( bDelayOpen )
	{
		int nFileNameLen = Q_strlen( pFileName );
		m_pFileName = new char[ nFileNameLen + 1 ];
		Q_strcpy( m_pFileName, pFileName );

		if ( pPath )
		{
			int nPathLen = Q_strlen( pPath );
			m_pPath = new char[ nPathLen + 1 ];
			Q_strcpy( m_pPath, pPath );
		}
		else
		{
			m_pPath = new char[ 1 ];
			m_pPath[0] = 0;
		}
		
		m_nOpenFileFlags = nOpenFileFlags;
		m_hFileHandle = FILESYSTEM_INVALID_HANDLE;
	}
	else
	{
		m_pFileName = NULL;
		m_pPath = NULL;
		m_nOpenFileFlags = 0;
		m_hFileHandle = OpenFile( pFileName, pPath, nOpenFileFlags );
		if ( m_hFileHandle == FILESYSTEM_INVALID_HANDLE )
		{
			m_Error |= FILE_OPEN_ERROR;
			return;
		}
	}

	if ( IsReadOnly() )
	{
		// NOTE: MaxPut may not actually be this exact size for text files;
		// it could be slightly less owing to the /r/n -> /n conversion
		m_nMaxPut = g_pFullFileSystem->Size( m_hFileHandle );

		// Read in the first bytes of the file
		if ( Size() > 0 )
		{
			int nSizeToRead = MIN( Size(), m_nMaxPut );
			ReadBytesFromFile( nSizeToRead, 0 );
		}
	}
}


void CUtlStreamBuffer::Close()
{
	if ( !IsReadOnly() )
	{
		// Write the final bytes
		int nBytesToWrite = TellPut() - m_nOffset;
		if ( nBytesToWrite > 0 )
		{
			if ( ( m_hFileHandle == FILESYSTEM_INVALID_HANDLE ) && m_pFileName )
			{
				m_hFileHandle = OpenFile( m_pFileName, m_pPath, m_nOpenFileFlags );
			}
			if ( m_hFileHandle != FILESYSTEM_INVALID_HANDLE )
			{
				if ( g_pFullFileSystem )
					g_pFullFileSystem->Write( Base(), nBytesToWrite, m_hFileHandle );
			}
		}
	}

	if ( m_hFileHandle != FILESYSTEM_INVALID_HANDLE )
	{
		if ( g_pFullFileSystem )
			g_pFullFileSystem->Close( m_hFileHandle );
		m_hFileHandle = FILESYSTEM_INVALID_HANDLE;
	}

	if ( m_pFileName )
	{
		delete[] m_pFileName;
		m_pFileName = NULL;
	}

	if ( m_pPath )
	{
		delete[] m_pPath;
		m_pPath = NULL;
	}

	m_Error = 0;
}

CUtlStreamBuffer::~CUtlStreamBuffer()
{
	Close();
}


//-----------------------------------------------------------------------------
// Open the file. normally done in constructor
//-----------------------------------------------------------------------------
void CUtlStreamBuffer::Open( const char *pFileName, const char *pPath, int nFlags, int nOpenFileFlags )
{
	if ( IsOpen() )
	{
		Close();
	}

	m_Get = 0;
	m_Put = 0;
	m_nTab = 0;
	m_nOffset = 0;
	m_Flags = nFlags;
	m_hFileHandle = OpenFile( pFileName, pPath, nOpenFileFlags );
	if ( m_hFileHandle == FILESYSTEM_INVALID_HANDLE )
	{
		m_Error |= FILE_OPEN_ERROR;
		return;
	}

	if ( IsReadOnly() )
	{
		// NOTE: MaxPut may not actually be this exact size for text files;
		// it could be slightly less owing to the /r/n -> /n conversion
		m_nMaxPut = g_pFullFileSystem->Size( m_hFileHandle );

		// Read in the first bytes of the file
		if ( Size() > 0 )
		{
			int nSizeToRead = MIN( Size(), m_nMaxPut );
			ReadBytesFromFile( nSizeToRead, 0 );
		}
	}
	else
	{
		if ( m_Memory.NumAllocated() != 0 )
		{
			m_nMaxPut = -1;
			AddNullTermination( m_Put );
		}
		else
		{
			m_nMaxPut = 0;
		}
	}
}


//-----------------------------------------------------------------------------
// Is the file open?
//-----------------------------------------------------------------------------
bool CUtlStreamBuffer::IsOpen() const
{
	if ( m_hFileHandle != FILESYSTEM_INVALID_HANDLE )
		return true;

	// Delayed open case
	return ( m_pFileName != 0 );
}


//-----------------------------------------------------------------------------
// Grow allocation size to fit requested size
//-----------------------------------------------------------------------------
void CUtlStreamBuffer::GrowAllocatedSize( int nSize )
{
	int nNewSize = Size();
	if ( nNewSize < nSize + 1 )
	{
		while ( nNewSize < nSize + 1 )
		{
			nNewSize += DEFAULT_STREAM_CHUNK_SIZE; 
		}
		m_Memory.Grow( nNewSize - Size() );
	}
}


//-----------------------------------------------------------------------------
// Commit some of the stream to disk when we overflow.
//-----------------------------------------------------------------------------
bool CUtlStreamBuffer::StreamPutOverflow( int nSize )
{
	if ( !IsValid() || IsReadOnly() )
		return false;

	// Make sure the allocated size is at least as big as the requested size
	if ( nSize > 0 )
	{
		GrowAllocatedSize( nSize + 2 );
	}

	// m_nOffset represents the location in the virtual buffer of m_Memory[0].
	// Compute the number of bytes that we've buffered up in memory so that we know what to write to disk.
	int nBytesToWrite = TellPut() - m_nOffset;
	if ( ( nBytesToWrite > 0 ) || ( nSize < 0 ) )
	{
		if ( m_hFileHandle == FILESYSTEM_INVALID_HANDLE )
		{
			m_hFileHandle = OpenFile( m_pFileName, m_pPath, m_nOpenFileFlags );
		}
	}

	// Write out the data that we have buffered if we have any.
	if ( nBytesToWrite > 0 )
	{
		int nBytesWritten = g_pFullFileSystem->Write( Base(), nBytesToWrite, m_hFileHandle );
		if ( nBytesWritten != nBytesToWrite )
			return false;

		// Set the offset to the current Put location to indicate that the buffer is now empty.
		m_nOffset = TellPut();
	}

	if ( nSize < 0 )
	{
		m_nOffset = -nSize-1;
		g_pFullFileSystem->Seek( m_hFileHandle, m_nOffset, FILESYSTEM_SEEK_HEAD );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Commit some of the stream to disk upon requests
//-----------------------------------------------------------------------------
bool CUtlStreamBuffer::TryFlushToFile( int nFlushToFileBytes )
{
	if ( !IsValid() || IsReadOnly() || ( m_Error & PUT_OVERFLOW ) )
		return false;

	// m_nOffset represents the location in the virtual buffer of m_Memory[0].
	// Compute the number of bytes that we've buffered up in memory so that we know what to write to disk.
	int nBytesToWrite = TellPut() - m_nOffset;
	if ( nFlushToFileBytes < nBytesToWrite )
		nBytesToWrite = nFlushToFileBytes;	// cannot write more than what we have, but can flush beginning of buffer up to certain amount

	if ( nBytesToWrite <= 0 )
		return true;	// nothing buffered to write

	if ( m_hFileHandle == FILESYSTEM_INVALID_HANDLE )
	{
		m_hFileHandle = OpenFile( m_pFileName, m_pPath, m_nOpenFileFlags );
	}

	// Write out the data that we have buffered if we have any.
	int nBytesWritten = g_pFullFileSystem->Write( Base(), nBytesToWrite, m_hFileHandle );
	if ( nBytesWritten != nBytesToWrite )
	{
		m_Error |= PUT_OVERFLOW;	// Flag the buffer the same way as if put overflow callback failed
		return false;				// Let caller know about file IO failure
	}

	// Move the uncommitted data over and advance the offset up by as many bytes
	memmove( Base(), ( const char* ) Base() + nBytesWritten, ( TellPut() - m_nOffset ) - nBytesWritten + 1 ); // + 1 byte for null termination character
	m_nOffset += nBytesWritten;

	return true;
}



//-----------------------------------------------------------------------------
// Reads bytes from the file; fixes up maxput if necessary and null terminates
//-----------------------------------------------------------------------------
int CUtlStreamBuffer::ReadBytesFromFile( int nBytesToRead, int nReadOffset )
{
	if ( m_hFileHandle == FILESYSTEM_INVALID_HANDLE )
	{
		if ( !m_pFileName )
		{
			Warning( "File has not been opened!\n" );
			Assert(0);
			return 0;
		}

		m_hFileHandle = OpenFile( m_pFileName, m_pPath, m_nOpenFileFlags );
		if ( m_hFileHandle == FILESYSTEM_INVALID_HANDLE )
		{
			Error( "Unable to read file %s!\n", m_pFileName );
			return 0;
		}
		if ( m_nOffset != 0 )
		{
			g_pFullFileSystem->Seek( m_hFileHandle, m_nOffset, FILESYSTEM_SEEK_HEAD );
		}
	}

	char *pReadPoint = (char*)Base() + nReadOffset;
	int nBytesRead = g_pFullFileSystem->Read( pReadPoint, nBytesToRead, m_hFileHandle );
	if ( nBytesRead != nBytesToRead )
	{
		// Since max put is a guess at the start, 
		// we need to shrink it based on the actual # read
		if ( m_nMaxPut > TellGet() + nReadOffset + nBytesRead )
		{
			m_nMaxPut = TellGet() + nReadOffset + nBytesRead;
		}
	}

	if ( nReadOffset + nBytesRead < Size() )
	{
		// This is necessary to deal with auto-NULL terminiation
		pReadPoint[nBytesRead] = 0;
	}

	return nBytesRead;
}


//-----------------------------------------------------------------------------
// Load up more of the stream when we overflow
//-----------------------------------------------------------------------------
bool CUtlStreamBuffer::StreamGetOverflow( int nSize )
{
	if ( !IsValid() || !IsReadOnly() )
		return false;

	// Shift the unread bytes down
	// NOTE: Can't use the partial overlap path if we're seeking. We'll 
	// get negative sizes passed in if we're seeking.
	int nUnreadBytes;
	bool bHasPartialOverlap = ( nSize >= 0 ) && ( TellGet() >= m_nOffset ) && ( TellGet() <= m_nOffset + Size() );
	if ( bHasPartialOverlap )
	{
		nUnreadBytes = Size() - ( TellGet() - m_nOffset );
		if ( ( TellGet() != m_nOffset ) && ( nUnreadBytes > 0 ) )
		{
			memmove( Base(), (const char*)Base() + TellGet() - m_nOffset, nUnreadBytes );
		}
	}
	else
	{
		m_nOffset = TellGet();
		g_pFullFileSystem->Seek( m_hFileHandle, m_nOffset, FILESYSTEM_SEEK_HEAD );
		nUnreadBytes = 0;
	}

	// Make sure the allocated size is at least as big as the requested size
	if ( nSize > 0 )
	{
		GrowAllocatedSize( nSize );
	}

	int nBytesToRead = Size() - nUnreadBytes;
	int nBytesRead = ReadBytesFromFile( nBytesToRead, nUnreadBytes );
	if ( nBytesRead == 0 )
		return false;

	m_nOffset = TellGet();
	return ( nBytesRead + nUnreadBytes >= nSize ); 
}


//-----------------------------------------------------------------------------
// open file unless already failed to open
//-----------------------------------------------------------------------------
FileHandle_t CUtlStreamBuffer::OpenFile( const char *pFileName, const char *pPath, int nOpenFileFlags )
{
	if ( m_Error & FILE_OPEN_ERROR )
		return FILESYSTEM_INVALID_HANDLE;

	char options[ 3 ] = "xx";
	options[ 0 ] = IsReadOnly() ? 'r' : 'w';
	options[ 1 ] = IsText() && !ContainsCRLF() ? 't' : 'b';

	return g_pFullFileSystem->OpenEx( pFileName, options, nOpenFileFlags, pPath );
}
