//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Engine specific CRC functions
//
//=============================================================================//


#include "checksum_engine.h"
#include "bspfile.h"
#include "filesystem.h"
#include "filesystem_engine.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: For proxy protecting
// Turns the passed data into a 
// single byte block CRC based on current network sequence number.
// Input  : *base - 
//			length - 
//			sequence - 
// Output : byte COM_BlockSequenceCRCByte
//-----------------------------------------------------------------------------
byte COM_BlockSequenceCRCByte( byte *base, int length, int sequence )
{
	CRC32_t crc;
	byte *p;
	byte chkb[60 + 4];

	if (sequence < 0)
	{
		Sys_Error("sequence < 0, in COM_BlockSequenceCRCByte\n");
	}

	CRC32_t entry;
	entry = CRC32_GetTableEntry( ( sequence + 1 ) % ( 256 * sizeof(CRC32_t) - 4 ) );
	p = (byte *)&entry;

	// Use up to the first 60 bytes of data and a 4 byte value from the 
	//  CRC lookup table (divided into the sequence)

	if (length > 60)
	{
		length = 60;
	}
	
	memcpy (chkb, base, length);

	chkb[length+0] = p[0];
	chkb[length+1] = p[1];
	chkb[length+2] = p[2];
	chkb[length+3] = p[3];

	length += 4;

	// Compute a crc based on the buffer.
	CRC32_Init(&crc);
	CRC32_ProcessBuffer(&crc, chkb, length);
	CRC32_Final(&crc);
	
	// Chop down to byte size
	return (byte)(crc & 0xFF);
}

#if defined(_X360)
const int CRC_BUFSIZE = 16384;
#else
const int CRC_BUFSIZE = 65536;
#endif

// YWB:  5/18
/*
===================
bool CRC_File(unsigned short *crcvalue, char *pszFileName)

  Computes CRC for given file.  If there is an error opening/reading the file, returns false,
  otherwise returns true and sets the crc value passed to it.  The value should be initialized
  with CRC_Init
 ==================
 */
bool CRC_File(CRC32_t *crcvalue, const char *pszFileName)
{
	// Always re-init the CRC buffer
	CRC32_Init( crcvalue );

	FileHandle_t fp;
	byte chunk[CRC_BUFSIZE];
	int nBytesRead;
	
	int nSize;

	nSize = COM_OpenFile(pszFileName, &fp);
	if ( !fp || ( nSize == -1 ) )
		return FALSE;

	// Now read in chunks
	while (nSize > 0)
	{
		if (nSize > sizeof(chunk))
			nBytesRead = g_pFileSystem->Read(chunk, sizeof(chunk), fp);
		else
			nBytesRead = g_pFileSystem->Read(chunk, nSize, fp);

		// If any data was received, CRC it.
		if (nBytesRead > 0)
		{
			nSize -= nBytesRead;
			CRC32_ProcessBuffer(crcvalue, chunk, nBytesRead);
		}

		// We we are end of file, break loop and return
		if ( g_pFileSystem->EndOfFile( fp ) )
		{
			g_pFileSystem->Close( fp );
			fp = 0;
			break;
		}
		// If there was a disk error, indicate failure.
		else if ( !g_pFileSystem->IsOk(fp) )
		{
			if ( fp )
				g_pFileSystem->Close(fp);
			return FALSE;
		}
	}	

	if ( fp )
		g_pFileSystem->Close(fp);
	return TRUE;
}

static BSPHeader_t *g_pMapHeader = NULL;

int __cdecl LumpCompare( const void *pElem0, const void *pElem1 )
{
	const int *pLump0 = (const int *)pElem0;
	const int *pLump1 = (const int *)pElem1;
	return g_pMapHeader->lumps[*pLump0].fileofs - g_pMapHeader->lumps[*pLump1].fileofs;
}

// YWB:  5/18
/*
===================
bool CRC_MapFile(unsigned short *crcvalue, char *pszFileName)

  Computes CRC for given map file.  If there is an error opening/reading the file, returns false,
  otherwise returns true and sets the crc value passed to it.  The value should be initialized
  with CRC_Init

  For map (.bsp) files, the entity lump is not included in the CRC.
  //FIXME make this work
 ==================
 */

ConVar debug_map_crc( "debug_map_crc", "0", FCVAR_RELEASE, "Prints CRC for each map lump loaded" );

bool CRC_MapFile(CRC32_t *crcvalue, const char *pszFileName)
{
	FileHandle_t fp;
	byte chunk[CRC_BUFSIZE];
	int nBytesRead;
	BSPHeader_t	header;
	int nSize;
	lump_t *curLump;
	long startOfs;

	nSize = COM_OpenFile(pszFileName, &fp);
	if ( !fp || ( nSize == -1 ) )
		return false;

	startOfs = g_pFileSystem->Tell(fp);

	// Don't CRC the header.
	if (g_pFileSystem->Read(&header, sizeof(BSPHeader_t), fp) == 0)
	{
		ConMsg("Could not read BSP header for map [%s].\n", pszFileName);
		g_pFileSystem->Close(fp);
		return false;
	}

	if ( header.m_nVersion < MINBSPVERSION || header.m_nVersion > BSPVERSION )
	{
		g_pFileSystem->Close(fp);
		ConMsg("Map [%s] has incorrect BSP version (%i should be %i).\n", pszFileName, header.m_nVersion, BSPVERSION);
		return false;
	}

	if ( IsX360() )
	{
		// 360 bsp's store the pc checksum in the flags lump header
		g_pFileSystem->Close(fp);
		*crcvalue = header.lumps[LUMP_MAP_FLAGS].version;
		return true;
	}

	static char gamedir[MAX_OSPATH];
	Q_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );

	g_pMapHeader = &header;
	int lumpList[HEADER_LUMPS];
	for ( int i = 0; i < HEADER_LUMPS; i++ )
	{
		lumpList[i] = i;
	}
	qsort( lumpList, HEADER_LUMPS, sizeof(lumpList[0]), LumpCompare );

	CRC32_t lumpCRC;
	
	// CRC across all lumps except for the Entities lump
	for (int i = 0; i < HEADER_LUMPS; i++)
	{
		int l = lumpList[i];
		if (l == LUMP_ENTITIES)
			continue;

		CRC32_Init(&lumpCRC);

		// INFESTED_DLL - Alien Swarm wants a more relaxed CRC check so each client can compile the map themselves
		if ( !Q_stricmp( gamedir, "infested" ) )
		{
			if ( l == LUMP_LIGHTING ||
				l == LUMP_DISPINFO ||
				l == LUMP_GAME_LUMP ||
				l == LUMP_PRIMITIVES ||
				l == LUMP_LEAF_AMBIENT_INDEX_HDR ||
				l == LUMP_LEAF_AMBIENT_INDEX ||
				l == LUMP_LEAF_AMBIENT_LIGHTING_HDR ||
				l == LUMP_LEAF_AMBIENT_LIGHTING ||
				l == LUMP_PAKFILE ||
				l == LUMP_PHYSCOLLIDE ||
				l == LUMP_TEXINFO ||
				l == LUMP_PHYSDISP )
			continue;
		}

		curLump = &header.lumps[l];
		nSize = curLump->filelen;
		if ( nSize <= 0 )
			continue;

		g_pFileSystem->Seek( fp, startOfs + curLump->fileofs, FILESYSTEM_SEEK_HEAD );

		// Now read chunks
		while (nSize > 0)
		{
			if (nSize > sizeof(chunk))
				nBytesRead = g_pFileSystem->Read(chunk, sizeof(chunk), fp);
			else
				nBytesRead = g_pFileSystem->Read(chunk, nSize, fp);

			// If any data was received, CRC it.
			if (nBytesRead > 0)
			{
				nSize -= nBytesRead;
				CRC32_ProcessBuffer(crcvalue, chunk, nBytesRead);

				if ( debug_map_crc.GetBool() )
				{
					CRC32_ProcessBuffer(&lumpCRC, chunk, nBytesRead);
				}
			}

			// If there was a disk error, indicate failure.
			if ( !g_pFileSystem->IsOk(fp) )
			{
				if ( fp )
					g_pFileSystem->Close(fp);
				return false;
			}
		}	

		if ( debug_map_crc.GetBool() )
		{
			Msg( "Lump %d crc %lu\n", l, lumpCRC );
		}
	}
	
	if ( fp )
		g_pFileSystem->Close(fp);

	if ( debug_map_crc.GetBool() )
	{
		Msg( "Map file '%s' CRC = %lu\n", pszFileName, *crcvalue );
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : digest[16] - 
//			*pszFileName - 
//			bSeed - 
//			seed[4] - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool MD5_Hash_File(unsigned char digest[16], char *pszFileName, bool bSeed /* = FALSE */, unsigned int seed[4] /* = NULL */)
{
	FileHandle_t fp;
	byte chunk[1024];
	int nBytesRead;
	MD5Context_t ctx;
	
	int nSize;

	nSize = COM_OpenFile( pszFileName, &fp );
	if ( !fp || ( nSize == -1 ) )
		return false;

	memset(&ctx, 0, sizeof(MD5Context_t));

	MD5Init(&ctx);

	if (bSeed)
	{
		// Seed the hash with the seed value
		MD5Update( &ctx, (const unsigned char *)&seed[0], 16 );
	}

	// Now read in 1K chunks
	while (nSize > 0)
	{
		if (nSize > 1024)
			nBytesRead = g_pFileSystem->Read(chunk, 1024, fp);
		else
			nBytesRead = g_pFileSystem->Read(chunk, nSize, fp);

		// If any data was received, CRC it.
		if (nBytesRead > 0)
		{
			nSize -= nBytesRead;
			MD5Update(&ctx, chunk, nBytesRead);
		}

		// We we are end of file, break loop and return
		if ( g_pFileSystem->EndOfFile( fp ) )
		{
			g_pFileSystem->Close( fp );
			fp = NULL;
			break;
		}
		// If there was a disk error, indicate failure.
		else if ( !g_pFileSystem->IsOk(fp) )
		{
			if ( fp )
				g_pFileSystem->Close(fp);
			return FALSE;
		}
	}	

	if ( fp )
		g_pFileSystem->Close(fp);

	MD5Final(digest, &ctx);

	return TRUE;
}

