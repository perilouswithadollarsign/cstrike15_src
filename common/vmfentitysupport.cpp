//====== Copyright c 1996-2007, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "platform.h"
#include "utlvector.h"
#include "chunkfile.h"
#include "vmfentitysupport.h"

//////////////////////////////////////////////////////////////////////////
//
//  Additional custom save load handlers for entities
//
//////////////////////////////////////////////////////////////////////////

VmfSaveLoadHandlers & VmfGetSaveLoadHandlers()
{
	static VmfSaveLoadHandlers s_arrHandlers;
	return s_arrHandlers;
}

void VmfInstallMapEntitySaveLoadHandler( IMapEntitySaveLoadHandler *pHandler )
{
	VmfGetSaveLoadHandlers().AddToTail( pHandler );
}

void VmfUninstallMapEntitySaveLoadHandler( IMapEntitySaveLoadHandler *pHandler )
{
	VmfSaveLoadHandlers &arr = VmfGetSaveLoadHandlers();
	for ( int k = 0; k < arr.Count(); ++ k )
	{
		if ( arr.Element( k ) == pHandler )
		{
			arr.Remove( k );
			return;
		}
	}
}

struct VmfSaveLoadHandlerInfo_t
{
	IMapEntity_Type_t *pEntity;
	IMapEntitySaveLoadHandler *pHandler;
};

static ChunkFileResult_t VmfMapEntitySaveLoadHandlerCallback( CChunkFile *pFile, VmfSaveLoadHandlerInfo_t *pInfo )
{
	pInfo->pHandler->SetCurrentEntity( pInfo->pEntity );
	return pInfo->pHandler->LoadVMF( pFile );
}

static ChunkFileResult_t MapEntityLoadKVHandlerCallback( const char *szKey, const char *szValue, VmfSaveLoadHandlerInfo_t *pInfo )
{
	// pInfo->pHandler->SetCurrentEntity( pInfo->pEntity );
	return pInfo->pHandler->LoadKeyValue( szKey, szValue );
}

void VmfAddMapEntityHandlers( CChunkHandlerMap *pHandlerMap, IMapEntity_Type_t *pEntity )
{
	// Add all custom handlers
	VmfSaveLoadHandlers &arrHandlers = VmfGetSaveLoadHandlers();
	static CUtlVector< VmfSaveLoadHandlerInfo_t > arrHandlerInfos;
	arrHandlerInfos.SetCount( arrHandlers.Count() );
	for ( int idxHandler = 0; idxHandler < arrHandlers.Count(); ++ idxHandler )
	{
		IMapEntitySaveLoadHandler *pHandler = arrHandlers.Element( idxHandler );
		VmfSaveLoadHandlerInfo_t &info = arrHandlerInfos.Element( idxHandler );
		info.pEntity = pEntity;
		info.pHandler = pHandler;
		pHandlerMap->AddHandler( pHandler->GetCustomSectionName(), (ChunkHandler_t)VmfMapEntitySaveLoadHandlerCallback, &info );
	}
}

ChunkFileResult_t VmfSaveVmfEntityHandlers( CChunkFile *pFile,
											IMapEntity_Type_t *pEntity,
											IMapEntity_SaveInfo_t *pSaveInfo )
{
	ChunkFileResult_t eResult = ChunkFile_Ok;
	VmfSaveLoadHandlers &arrHandlers = VmfGetSaveLoadHandlers();
	for ( int idxHandler = 0;
		(eResult == ChunkFile_Ok) && ( idxHandler < arrHandlers.Count() );
		++ idxHandler )
	{
		IMapEntitySaveLoadHandler *pHandler = arrHandlers.Element( idxHandler );
		pHandler->SetCurrentEntity( pEntity );
		eResult = pHandler->SaveVMF( pFile, pSaveInfo );
	}
	return eResult;
}



void IMapEntitySaveLoadHandler::SetCurrentEntity( IMapEntity_Type_t *pEntity )
{
}

ChunkFileResult_t IMapEntitySaveLoadHandler::SaveVMF( CChunkFile *pFile, IMapEntity_SaveInfo_t *pSaveInfo )
{
	return ChunkFile_Ok;
}

ChunkFileResult_t IMapEntitySaveLoadHandler::LoadVMF( CChunkFile *pFile )
{
	VmfSaveLoadHandlerInfo_t info;
	// info.pEntity = pEntity;
	info.pHandler = this;

	ChunkFileResult_t eResult = LoadKeyValueBegin( pFile );
	if ( eResult == ChunkFile_Ok )
	{
		eResult = pFile->ReadChunk( (KeyHandler_t)MapEntityLoadKVHandlerCallback, &info );
	}

	return LoadKeyValueEnd( pFile, eResult );
}

ChunkFileResult_t IMapEntitySaveLoadHandler::LoadKeyValueBegin( CChunkFile *pFile )
{
	return ChunkFile_Ok;
}

ChunkFileResult_t IMapEntitySaveLoadHandler::LoadKeyValue( const char *szKey, const char *szValue )
{
	return ChunkFile_Ok;
}

ChunkFileResult_t IMapEntitySaveLoadHandler::LoadKeyValueEnd( CChunkFile *pFile, ChunkFileResult_t eLoadResult )
{
	return eLoadResult;
}



