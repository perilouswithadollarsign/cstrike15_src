//====== Copyright c 1996-2007, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef VMFENTITYSUPPORT_H
#define VMFENTITYSUPPORT_H
#ifdef _WIN32
#pragma once
#endif

class IMapEntity_Type_t;
class IMapEntity_SaveInfo_t;

class CChunkFile;
class CChunkHandlerMap;
enum ChunkFileResult_t;

//
// Custom load/save handlers
//
class IMapEntitySaveLoadHandler
{
public:
	virtual char const *GetCustomSectionName() = 0;

public:
	virtual void SetCurrentEntity( IMapEntity_Type_t *pEntity ); // Default implementation does nothing

public:
	virtual ChunkFileResult_t SaveVMF( CChunkFile *pFile, IMapEntity_SaveInfo_t *pSaveInfo ); // Default implementation does nothing
	virtual ChunkFileResult_t LoadVMF( CChunkFile *pFile );	// Default implementation falls through to key values calls

public:
	virtual ChunkFileResult_t LoadKeyValueBegin( CChunkFile *pFile ); // Default implementation does nothing
	virtual ChunkFileResult_t LoadKeyValue( const char *szKey, const char *szValue ); // Default implementation does nothing
	virtual ChunkFileResult_t LoadKeyValueEnd( CChunkFile *pFile, ChunkFileResult_t eLoadResult ); // Default implementation returns the given load result
};

typedef CUtlVector< IMapEntitySaveLoadHandler * > VmfSaveLoadHandlers;
VmfSaveLoadHandlers & VmfGetSaveLoadHandlers();

void VmfInstallMapEntitySaveLoadHandler( IMapEntitySaveLoadHandler *pHandler );
void VmfUninstallMapEntitySaveLoadHandler( IMapEntitySaveLoadHandler *pHandler );

void VmfAddMapEntityHandlers( CChunkHandlerMap *pHandlerMap, IMapEntity_Type_t *pEntity );
ChunkFileResult_t VmfSaveVmfEntityHandlers( CChunkFile *pFile, IMapEntity_Type_t *pEntity, IMapEntity_SaveInfo_t *pSaveInfo );

#endif // #ifndef VMFENTITYSUPPORT_H
