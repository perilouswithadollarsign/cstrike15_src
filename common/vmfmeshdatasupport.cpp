//====== Copyright c 1996-2007, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "platform.h"
#include "utlvector.h"
#include "utlbuffer.h"
#include "chunkfile.h"
#include "utlencode.h"
#include "fgdlib/wckeyvalues.h"
#include "vmfmeshdatasupport.h"


CVmfMeshDataSupport_SaveLoadHandler::CVmfMeshDataSupport_SaveLoadHandler()
{
}

CVmfMeshDataSupport_SaveLoadHandler::~CVmfMeshDataSupport_SaveLoadHandler()
{
	NULL;
}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::WriteDataChunk( CChunkFile *pFile, char const *szHash )
{
	ChunkFileResult_t eResult;
	eResult = pFile->BeginChunk( GetCustomSectionName() );
	if ( eResult != ChunkFile_Ok )
		return eResult;

	// Write out our data version
	char szModelDataVer[ 16 ] = {0};
	sprintf( szModelDataVer, "%d", GetCustomSectionVer() );
	eResult = pFile->WriteKeyValue( "version", szModelDataVer );
	if ( eResult != ChunkFile_Ok )
		return eResult;

	// Write our hash
	eResult = pFile->WriteKeyValue( "hash", szHash );
	if ( eResult != ChunkFile_Ok )
		return eResult;

	// Write out additional data
	eResult = OnFileDataWriting( pFile, szHash );
	if ( eResult != ChunkFile_Ok )
		return eResult;

	return pFile->EndChunk();
}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::OnFileDataWriting( CChunkFile *pFile, char const *szHash )
{
	return ChunkFile_Ok;
}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::WriteBufferData( CChunkFile *pFile, CUtlBuffer &bufData, char const *szPrefix )
{
	int numEncBytes = KvEncoder::GuessEncodedLength( bufData.TellPut() );
	CUtlBuffer bufEncoded;
	bufEncoded.EnsureCapacity( numEncBytes );

	if ( !KvEncoder::Encode( bufData, bufEncoded ) )
		return ChunkFile_Fail;
	numEncBytes = bufEncoded.TellPut();

	// Now we have the encoded data, split it into blocks
	int numBytesPerLine = KEYVALUE_MAX_VALUE_LENGTH - 2;
	int numLines = (numEncBytes + numBytesPerLine - 1 ) / numBytesPerLine;
	int numLastLineBytes = numEncBytes % numBytesPerLine;
	if ( !numLastLineBytes )
		numLastLineBytes = numBytesPerLine;

	// Key buffer
	char chKeyBuf[ KEYVALUE_MAX_KEY_LENGTH ] = {0};
	char chKeyValue[ KEYVALUE_MAX_VALUE_LENGTH ] = {0};

	// Write the data
	ChunkFileResult_t eResult;

	sprintf( chKeyBuf, "%s_ebytes", szPrefix );
	eResult = pFile->WriteKeyValueInt( chKeyBuf, numEncBytes );
	if ( eResult != ChunkFile_Ok )
		return eResult;

	sprintf( chKeyBuf, "%s_rbytes", szPrefix );
	eResult = pFile->WriteKeyValueInt( chKeyBuf, bufData.TellPut() );
	if ( eResult != ChunkFile_Ok )
		return eResult;

	sprintf( chKeyBuf, "%s_lines", szPrefix );
	eResult = pFile->WriteKeyValueInt( chKeyBuf, numLines );
	if ( eResult != ChunkFile_Ok )
		return eResult;

	for ( int ln = 0; ln < numLines; ++ ln )
	{
		int lnLen = ( ln < (numLines - 1) ) ? numBytesPerLine : numLastLineBytes;

		sprintf( chKeyBuf, "%s_ln_%d", szPrefix, ln );
		sprintf( chKeyValue, "%.*s", lnLen, ( char * ) bufEncoded.Base() + ln * numBytesPerLine );
		
		eResult = pFile->WriteKeyValue( chKeyBuf, chKeyValue );
		if ( eResult != ChunkFile_Ok )
			return eResult;
	}

	return ChunkFile_Ok;

}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::LoadKeyValueBegin( CChunkFile *pFile )
{
	m_eLoadState = LOAD_VERSION;
	m_iLoadVer = 0;
	m_hLoadHeader.sHash[0] = 0;
	LoadInitHeader();

	return ChunkFile_Ok;
}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::LoadKeyValue( const char *szKey, const char *szValue )
{
	switch ( m_eLoadState )
	{
	case LOAD_VERSION:
		return LoadKeyValue_Hdr( szKey, szValue );
	
	default:
		switch ( m_iLoadVer )
		{
		case 1:
			return LoadKeyValue_Ver1( szKey, szValue );
		default:
			return ChunkFile_Fail;
		}
	}
}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::LoadKeyValue_Hdr( const char *szKey, const char *szValue )
{
	switch ( m_eLoadState )
	{
	case LOAD_VERSION:
		if ( stricmp( szKey, "version" ) )
			return ChunkFile_Fail;
		m_iLoadVer = atoi( szValue );

		switch ( m_iLoadVer )
		{
		case 1:
			m_eLoadState = LOAD_HASH;
			return ChunkFile_Ok;
		default:
			return ChunkFile_Fail;
		}
	
	default:
		return ChunkFile_Fail;
	}
}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::LoadKeyValue_Ver1( const char *szKey, const char *szValue )
{
	const char *szKeyName = szKey;
	const int nPrefixLen = 3;

	switch ( m_eLoadState )
	{
	case LOAD_HASH:
		if ( stricmp( szKey, "hash" ) )
			return ChunkFile_Fail;
		strncpy( m_hLoadHeader.sHash, szValue, sizeof( m_hLoadHeader.sHash ) );
		m_eLoadState = LOAD_PREFIX;
		break;

	case LOAD_PREFIX:
		sprintf( m_hLoadHeader.sPrefix, "%.*s", nPrefixLen, szKey );
		if ( strlen( szKey ) < 4 )
			return ChunkFile_Fail;
		if ( szKey[3] != '_' )
			return ChunkFile_Fail;
		m_eLoadState = LOAD_HEADER;
		// fall-through

	case LOAD_HEADER:
		if ( strnicmp( m_hLoadHeader.sPrefix, szKey, nPrefixLen ) )
			return ChunkFile_Fail;
		if ( szKey[3] != '_' )
			return ChunkFile_Fail;
	
		szKeyName = szKey + 4;
		if ( !stricmp( szKeyName, "ebytes" ) )
			m_hLoadHeader.numEncBytes = atoi( szValue );
		else if ( !stricmp( szKeyName, "rbytes" ) )
			m_hLoadHeader.numBytes = atoi( szValue );
		else if ( !stricmp( szKeyName, "lines" ) )
			m_hLoadHeader.numLines = atoi( szValue );
		else
			return ChunkFile_Fail;

		if ( !LoadHaveHeader() )
			break;

		m_eLoadState = LOAD_DATA;
		return LoadHaveLines( 0 );

	case LOAD_DATA:
		if ( strnicmp( m_hLoadHeader.sPrefix, szKey, nPrefixLen ) )
			return ChunkFile_Fail;
		if ( szKey[3] != '_' )
			return ChunkFile_Fail;

		szKeyName = szKey + 4;
		if ( strnicmp( szKeyName, "ln", 2 ) || szKeyName[2] != '_' )
			return ChunkFile_Fail;

		szKeyName += 3;
		{
			int iLineNum = atoi( szKeyName );
			if ( iLineNum != m_hLoadHeader.numHaveLines )
				return ChunkFile_Fail;

			m_bufLoadData.Put( szValue, strlen( szValue ) );

			return LoadHaveLines( iLineNum + 1 );
		}

	default:
		return ChunkFile_Fail;
	}

	return ChunkFile_Ok;
}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::LoadKeyValueEnd( CChunkFile *pFile, ChunkFileResult_t eLoadResult )
{
	if ( eLoadResult != ChunkFile_Ok )
		return eLoadResult;

	if ( m_eLoadState == LOAD_VERSION )
		return ChunkFile_Ok;

	switch ( m_iLoadVer )
	{
	case 1:
		switch ( m_eLoadState )
		{
		case LOAD_HASH:
		case LOAD_PREFIX:
			return ChunkFile_Ok;

		default:
			return ChunkFile_Fail;
		}

	default:
		return ChunkFile_Fail;
	}
}

void CVmfMeshDataSupport_SaveLoadHandler::LoadInitHeader()
{
	m_hLoadHeader.sPrefix[0] = 0;
	m_hLoadHeader.numLines = -1;
	m_hLoadHeader.numBytes = -1;
	m_hLoadHeader.numEncBytes = -1;
	m_hLoadHeader.numHaveLines = 0;
}

bool CVmfMeshDataSupport_SaveLoadHandler::LoadHaveHeader()
{
	return m_hLoadHeader.numLines >= 0 && m_hLoadHeader.numBytes >= 0 && m_hLoadHeader.numEncBytes >= 0;
}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::LoadHaveLines( int numHaveLines )
{
	if ( !numHaveLines )
	{
		m_bufLoadData.EnsureCapacity( m_hLoadHeader.numEncBytes );
		m_bufLoadData.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );
	}

	m_hLoadHeader.numHaveLines = numHaveLines;
	if ( m_hLoadHeader.numHaveLines < m_hLoadHeader.numLines )
		return ChunkFile_Ok;
	if ( m_hLoadHeader.numHaveLines > m_hLoadHeader.numLines )
		return ChunkFile_Fail;
	
	ChunkFileResult_t eRes = LoadSaveFullData();
	if ( eRes != ChunkFile_Ok )
		return eRes;

	LoadInitHeader();
	m_eLoadState = LOAD_PREFIX;
	return ChunkFile_Ok;
}

ChunkFileResult_t CVmfMeshDataSupport_SaveLoadHandler::LoadSaveFullData()
{
	// The filename
	CUtlBuffer bufBytes;
	bufBytes.EnsureCapacity( m_hLoadHeader.numBytes + 0x10 );
	if ( !KvEncoder::Decode( m_bufLoadData, bufBytes ) )
		return ChunkFile_Fail;

	return OnFileDataLoaded( bufBytes );
}
