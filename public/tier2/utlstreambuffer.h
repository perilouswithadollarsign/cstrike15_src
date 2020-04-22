//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
// Serialization/unserialization buffer
//=============================================================================//

#ifndef UTLSTREAMBUFFER_H
#define UTLSTREAMBUFFER_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlbuffer.h"
#include "filesystem.h"


//-----------------------------------------------------------------------------
// Command parsing..
//-----------------------------------------------------------------------------
class CUtlStreamBuffer : public CUtlBuffer
{
	typedef CUtlBuffer BaseClass;

public:
	// See CUtlBuffer::BufferFlags_t for flags
	CUtlStreamBuffer( );
	CUtlStreamBuffer( const char *pFileName, const char *pPath, int nFlags = 0, bool bDelayOpen = false, int nOpenFileFlags = 0 );
	~CUtlStreamBuffer();

	// Open the file. normally done in constructor
	void Open( const char *pFileName, const char *pPath, int nFlags, int nOpenFileFlags = 0 );

	// close the file. normally done in destructor
	void Close();

	// Is the file open?
	bool IsOpen() const;

	// try flushing the file
	bool TryFlushToFile( int nFlushToFileBytes );

private:
	// error flags
	enum
	{
		FILE_OPEN_ERROR = MAX_ERROR_FLAG << 1,
	};

	// Overflow functions
	bool StreamPutOverflow( int nSize );
	bool StreamGetOverflow( int nSize );

	// Grow allocation size to fit requested size
	void GrowAllocatedSize( int nSize );

	// Reads bytes from the file; fixes up maxput if necessary and null terminates
	int ReadBytesFromFile( int nBytesToRead, int nReadOffset );

	FileHandle_t OpenFile( const char *pFileName, const char *pPath, int nOpenFileFlags );

	FileHandle_t m_hFileHandle;

	// cached for delayed open
	char	*m_pFileName;
	char	*m_pPath;
	int		m_nOpenFileFlags;
};


#endif // UTLSTREAMBUFFER_H

