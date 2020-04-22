//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "soundsystem.h"
#include "tier2/riff.h"
#include "filesystem.h"
#include "tier1/strtools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Implements Audio IO on the engine's COMMON filesystem
//-----------------------------------------------------------------------------
class COM_IOReadBinary : public IFileReadBinary
{
public:
	FileHandle_t open( const char *pFileName );
	int read( void *pOutput, int size, FileHandle_t file );
	void seek( FileHandle_t file, int pos );
	unsigned int tell( FileHandle_t file );
	unsigned int size( FileHandle_t file );
	void close( FileHandle_t file );
};


// prepend sound/ to the filename -- all sounds are loaded from the sound/ directory
FileHandle_t COM_IOReadBinary::open( const char *pFileName )
{
	char namebuffer[512];
	FileHandle_t hFile;

	Q_strncpy(namebuffer, "sound", sizeof( namebuffer ) );

	//HACK HACK HACK  the server is sending back sound names with slashes in front... 
	if (pFileName[0]!='/')
	{
		Q_strncat(namebuffer,"/", sizeof( namebuffer ), COPY_ALL_CHARACTERS );
	}

	Q_strncat( namebuffer, pFileName, sizeof( namebuffer ), COPY_ALL_CHARACTERS );

	hFile = g_pFullFileSystem->Open( namebuffer, "rb", "GAME" );

	return hFile;
}

int COM_IOReadBinary::read( void *pOutput, int size, FileHandle_t file )
{
	if ( !file )
		return 0;

	return g_pFullFileSystem->Read( pOutput, size, file );
}

void COM_IOReadBinary::seek( FileHandle_t file, int pos )
{
	if ( !file )
		return;

	g_pFullFileSystem->Seek( file, pos, FILESYSTEM_SEEK_HEAD );
}

unsigned int COM_IOReadBinary::tell( FileHandle_t file )
{
	if ( !file )
		return 0;
	return g_pFullFileSystem->Tell( file );
}

unsigned int COM_IOReadBinary::size( FileHandle_t file )
{
	if (!file)
		return 0;
	return g_pFullFileSystem->Size( file );
}

void COM_IOReadBinary::close( FileHandle_t file )
{
	if (!file)
		return;

	g_pFullFileSystem->Close( file );
}

static COM_IOReadBinary io;
IFileReadBinary *g_pSndIO = &io;

