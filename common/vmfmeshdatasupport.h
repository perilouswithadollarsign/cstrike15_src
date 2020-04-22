//====== Copyright c 1996-2007, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef VMFMESHDATASUPPORT_H
#define VMFMESHDATASUPPORT_H
#ifdef _WIN32
#pragma once
#endif

//////////////////////////////////////////////////////////////////////////
//
// Special implementation of custom load/save chunks for entities
//
//////////////////////////////////////////////////////////////////////////

#include "vmfentitysupport.h"

class CVmfMeshDataSupport_SaveLoadHandler : public IMapEntitySaveLoadHandler
{
public:
	CVmfMeshDataSupport_SaveLoadHandler();
	~CVmfMeshDataSupport_SaveLoadHandler();

public:
	virtual int GetCustomSectionVer() { return 1; }

public:
	virtual void SetCurrentEntity( IMapEntity_Type_t *pEntity ) { m_pEntity = pEntity; }

public:
	virtual ChunkFileResult_t LoadKeyValueBegin( CChunkFile *pFile );
	virtual ChunkFileResult_t LoadKeyValue( const char *szKey, const char *szValue );
	virtual ChunkFileResult_t LoadKeyValueEnd( CChunkFile *pFile, ChunkFileResult_t eLoadResult );

protected:
	ChunkFileResult_t LoadKeyValue_Hdr( const char *szKey, const char *szValue );
	ChunkFileResult_t LoadKeyValue_Ver1( const char *szKey, const char *szValue );

protected:
	ChunkFileResult_t WriteDataChunk( CChunkFile *pFile, char const *szHash );
	ChunkFileResult_t WriteBufferData( CChunkFile *pFile, CUtlBuffer &bufData, char const *szPrefix );
	void LoadInitHeader();
	bool LoadHaveHeader();
	ChunkFileResult_t LoadHaveLines( int numHaveLines );
	ChunkFileResult_t LoadSaveFullData();

protected:
	virtual ChunkFileResult_t OnFileDataLoaded( CUtlBuffer &bufData ) = 0;
	virtual ChunkFileResult_t OnFileDataWriting( CChunkFile *pFile, char const *szHash );

protected:
	enum State
	{
		LOAD_VERSION = 0,
		LOAD_HDR_END
	};
	enum StateVer1
	{
		LOAD_HASH = LOAD_HDR_END + 1,
		LOAD_PREFIX,
		LOAD_HEADER,
		LOAD_DATA
	};
	int m_eLoadState;
	int m_iLoadVer;

	struct Header_t
	{
		char sHash[ MAX_PATH ];
		char sPrefix[ MAX_PATH ];
		int numBytes;
		int numEncBytes;
		int numLines;
		int numHaveLines;
	}
	m_hLoadHeader;

	CUtlBuffer m_bufLoadData;
	IMapEntity_Type_t *m_pEntity;
};

#endif // #ifndef VMFMESHDATASUPPORT_H
