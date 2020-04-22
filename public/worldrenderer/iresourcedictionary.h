//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef IRESOURCEDICTIONARY_H
#define IRESOURCEDICTIONARY_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/interface.h"
#include "filesystem.h"
#include "tier1/utlstringmap.h"

//-----------------------------------------------------------------------------
// Methods related to resources
//-----------------------------------------------------------------------------
abstract_class IResourceDictionary
{
public:
	virtual char *GetPageFile() = 0;
	virtual bool Init( unsigned char *pData, int nTransientMemoryBytes, BVHBuilderParams_t &buildParams, bool bToolMode = false ) = 0;
	virtual void Destroy( IRenderDevice *pDevice ) = 0;
	virtual bool LoadEntrySerial( void *pMemory, uint64 nMemorySize, CBVHDictionaryEntry &entry ) = 0;
	virtual int GetNumResources() = 0;
	virtual CBVHDictionaryEntry &GetEntry( int i ) = 0;
	virtual int GetNumLayoutDescs() = 0;
	virtual BVHInputLayoutDesc_t &GetLayoutDesc( int i ) = 0;
	virtual RenderInputLayout_t &TranslateInputLayout( int iLayout ) = 0;
	virtual void EvictNode( IRenderDevice *pDevice, IBVHNode* pNode ) = 0;
	virtual void CalculateNodeResidency( IBVHNode *pNode ) = 0;

	// Tools entry points
	virtual bool LoadNodeSerial( IRenderDevice *pDevice, IRenderContext *pContext, IBVHNode *pNode, bool bLoadIAData ) = 0;
	virtual bool UpdateEntrySerial( void *pMemory, uint64 nMemorySize, uint64 nOffsetFromEntry, CBVHDictionaryEntry &entry ) = 0;
	virtual void ShiftEntries( uint64 nByteOffset ) = 0;
	virtual void WriteEntries( FileHandle_t fp ) = 0;
	virtual void CollectEntries( CUtlVector<CBVHDictionaryEntry> &resourceEntries, CUtlVector<IResourceDictionary*> &dictionaryList, CUtlStringMap<int> &instanceDictionary, int *pResourceRemap ) = 0;
	virtual uint64 GetNonEntryDataSize() = 0;
	virtual void WriteNonEntryData( int32 nNewResources, char *pNewPageFile, FileHandle_t fp ) = 0;
	virtual void AddStaticEntries( CBVHDictionaryEntry *pEntries, int nEntries ) = 0;
	virtual int AddEntry( uint8 nFlags, uint64 nSizeBytes, int nResourceType, char *pName, bool bInstanced ) = 0;
	virtual void ByteSwapDictionary( CUtlBuffer *pOutBuffer, char *pNewPageFile ) = 0;
	virtual void ByteSwapResourceFile( FileHandle_t fp ) = 0;
};

#endif