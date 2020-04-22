//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef IRESOURCECOMPILER_H
#define IRESOURCECOMPILER_H

#ifdef _WIN32
#pragma once
#endif

#include "appframework/IAppSystem.h"
#include "resourcefile/resourcetype.h"
#include "resourcefile/resourcedictionary.h"
#include "resourcefile/resourceintrospection.h"

				
//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;
class CResourceStream;
class CUtlBuffer;


//-----------------------------------------------------------------------------
// Used for resource registration
//-----------------------------------------------------------------------------
struct RegisterResourceInfo_t
{
	ResourceType_t m_nType;
	ResourceId_t m_nId;
	uint32 m_nFlags;

	// Describes cacheable data
	uint32	m_nDataOffset;
	ResourceCompressionType_t m_nCompressionType;
	uint32	m_nDataSize;
	uint32	m_nUncompressedDataSize;

	// Describes permanent data
	uint32  m_nPermanentDataOffset;
	uint32	m_nPermanentDataSize;
};


//-----------------------------------------------------------------------------
// Resource compiler
//-----------------------------------------------------------------------------
abstract_class IResourceCompilerRegistry
{
public:
	virtual void RegisterResource( const RegisterResourceInfo_t &info ) = 0;
	virtual ResourceId_t RegisterResourceReference( ResourceType_t nType, const char *pFileName, const char *pResourceSubName ) = 0;
	virtual void RegisterUsedType( const char *pStructName, bool bPermanentData ) = 0;
	virtual void RegisterUsedType( ResourceStructureId_t id, bool bPermanentData ) = 0;
	virtual bool CompileResource( ResourceType_t nResourceType, CUtlBuffer &buf, const char *pElementFileName, CResourceStream *pPermanentStream, CResourceStream *pDataStream ) = 0; 
};


//-----------------------------------------------------------------------------
// Resource compiler
//-----------------------------------------------------------------------------
abstract_class IResourceCompiler : public IAppSystem
{
public:
	// This version of compile resource will potentially read multiple files
	// in a gather operation
	virtual bool CompileResource( const char *pFullPath, IResourceCompilerRegistry *pRegistry, CResourceStream *pPermanentStream, CResourceStream *pDataStream ) = 0;

	// This version of compile resource has already had the gather applied to it
	// and the data in the utlbuffer is all the data it will ever need
	virtual bool CompileResource( CUtlBuffer &buf, const char *pFullPath, IResourceCompilerRegistry *pRegistry, CResourceStream *pPermanentStream, CResourceStream *pDataStream ) = 0;
};

#define RESOURCE_COMPILER_INTERFACE_VERSION "RESOURCE_COMPILER_001"


#endif // IRESOURCECOMPILER_H
