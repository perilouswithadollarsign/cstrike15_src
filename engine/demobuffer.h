//======= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======
//
//===============================================================================

#ifndef DEMOBUFFER_H
#define DEMOBUFFER_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "utlbuffer.h"
#include "utllinkedlist.h"
#include <tier2/utlstreambuffer.h>
#include "interface.h"
#include "demofile/demoformat.h"

//-----------------------------------------------------------------------------
struct DemoBufferInitParams_t
{
	virtual ~DemoBufferInitParams_t() {}	// Need this to make polymorphic
};

struct StreamDemoBufferInitParams_t : public DemoBufferInitParams_t
{
	StreamDemoBufferInitParams_t( char const* filename, char const* path, int flags, int openfileflags ) : pFilename( filename ), pszPath( path ), nFlags( flags ), nOpenFileFlags( openfileflags ) {}
	char const*	pFilename;
	char const*	pszPath;
	int			nFlags;
	int			nOpenFileFlags;
};

struct MemoryDemoBufferInitParams_t : public DemoBufferInitParams_t
{
	MemoryDemoBufferInitParams_t( int maxsize ) : nMaxSize( maxsize ) {}
	int			nMaxSize;	// TODO: make use of this parameter
};


//-----------------------------------------------------------------------------
// Demo buffer interface - allows for one implementation of the demo format
// (demofile.cpp) with a common interface for writing to disk or writing to
// a buffer in memory.
//-----------------------------------------------------------------------------
class IDemoBuffer : public IBaseInterface
{
public:
	virtual bool				Init( DemoBufferInitParams_t const& params ) = 0;

	virtual void				NotifySignonComplete() = 0;

	virtual bool				IsInitialized() const = 0;
	virtual bool				IsValid() const = 0;

	virtual void				WriteHeader( const void *pData, int nSize ) = 0;

	virtual void				NotifyBeginFrame() = 0;
	virtual void				NotifyEndFrame() = 0;

	// Change where I'm writing (put)/reading (get)
	virtual void				SeekPut( bool bAbsolute, int offset ) = 0;
	virtual void				SeekGet( bool bAbsolute, int offset ) = 0;

	// Where am I writing (put)/reading (get)?
	virtual int					TellPut( ) const = 0;
	virtual int					TellGet( ) const = 0;

	// What's the most I've ever written?
	virtual int					TellMaxPut( ) const = 0;

	virtual char				GetChar( ) = 0;
	virtual unsigned char		GetUnsignedChar( ) = 0;
	virtual int					GetInt( ) = 0;
	virtual void				Get( void* pMem, int size ) = 0;

	virtual void				PutChar( char c ) = 0;
	virtual void				PutUnsignedChar( unsigned char uc ) = 0;
	virtual void				PutInt( int i ) = 0;
	virtual void				Put( const void* pMem, int size ) = 0;

	virtual void				WriteTick( int nTick ) = 0;

	virtual void				UpdateStartTick( int& nStartTick ) const = 0;
	virtual void				DumpToFile( char const* pFilename, const demoheader_t &header ) const = 0;
};

IDemoBuffer *CreateDemoBuffer( bool bMemoryBuffer, const DemoBufferInitParams_t &params );

#endif // DEMOBUFFER_H
