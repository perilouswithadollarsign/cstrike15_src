//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef IRESOURCESYSTEM_H
#define IRESOURCESYSTEM_H

#ifdef _WIN32
#pragma once
#endif

#include "appframework/iappsystem.h"
#include "resourcefile/resourceintrospection.h"
#include "resourcefile/resourcedictionary.h"
#include "resourcefile/resourcetype.h"
#include "tier2/tier2.h"

				
//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IResourceTypeManager;


//-----------------------------------------------------------------------------
// A method used to allow clients to construct + destruct resources based
// on raw data. Needs to be installed into resource type managers based on
// resource type
//-----------------------------------------------------------------------------
abstract_class IResourceTypeConstructor
{
public:
	virtual bool Init( IResourceTypeManager *pTypeManager ) = 0;
	virtual void Shutdown() = 0;

	// Returns a structure type associated with the permanent data
	virtual void *AllocatePermanentData( const void *pDataFromDisk, size_t nDataSize ) = 0;
	virtual void DeallocatePermanentData( void *pData ) = 0;

	// Returns the default memory limit
	virtual size_t GetDefaultMemoryLimit() = 0;

	// Allows the system to compute the actual size of the constructed class
	// based on the permanent data in in the resource system. If the system
	// doesn't know/care, return 0 here.
	virtual size_t ComputeActualSize( void *pPermanentData ) = 0;

	// NOTE: Resource references in pResourceData have been fixed up to 
	// be resource handles at this point
	virtual void *Allocate( const void *pPermanentData, const void *pResourceData, size_t nDataSize, ResourceHandle_t hResourceHandle ) = 0;
	virtual void Deallocate( void *pClass ) = 0;

	// Fallbacks + errors..
	virtual void *GetErrorResource( const void *pPermanentData ) = 0;
	virtual void *GetFallback( const void *pPermanentData ) = 0;
};


//-----------------------------------------------------------------------------
// A resource dictionary to look up resources
//-----------------------------------------------------------------------------
abstract_class IResourceTypeManager
{
public:
	virtual ResourceHandle_t FindOrCreateResource( const char *pFileName, const char *pSubResourceName ) = 0;
	virtual ResourceHandle_t FindOrCreateProceduralResource( const char *pGroupName, const char *pResourceName, const void *pPermanentData, size_t nDataSize ) = 0;
	virtual void DeleteResource( ResourceHandle_t hResource ) = 0;
	virtual ResourceHandle_t FindResource( ResourceId_t nResourceId ) = 0;
	virtual ResourceType_t GetResourceType() = 0;
	virtual ResourceId_t GetResourceId( ResourceHandle_t hResource ) const = 0;

	// To bring resources in or out of memory
	virtual void CacheResource( ResourceHandle_t hResource ) = 0;
	virtual void UncacheResource( ResourceHandle_t hResource ) = 0;

	// Sets the amount of memory to restrict this resource type to using
	virtual void SetMemoryLimit( int nMemoryLimit ) = 0;

	// Returns the list of cached resources
	virtual int GetResourceCount() const = 0;
	virtual int GetResources( int nFirst, int nCount, ResourceHandle_t *pResources ) = 0;
};


//-----------------------------------------------------------------------------
// Methods related to managing resources
//-----------------------------------------------------------------------------
abstract_class IResourceSystem : public IAppSystem
{
public:
	// Used to indicate globals to update to the current frame counter
	// This counter is controlled by the resource system + set to the
	// resource system's current frame
	virtual void RegisterFrameCounter( uint32 *pFrameCounter ) = 0;
	virtual void UnregisterFrameCounter( uint32 *pFrameCounter ) = 0;

	// Allows various systems to hook in resource type constructors and class bindings
	virtual void InstallResourceConstructor( ResourceType_t nType, const char *pResourceManagerType, IResourceTypeConstructor *pConstructor ) = 0;
	virtual void RemoveResourceConstructor( IResourceTypeConstructor *pConstructor ) = 0;

	// FIXME: Should this be in IResourceSystem or CResourceSystem?
	virtual void InstallSchemaClassBinding( class CSchemaClassBindingBase *pBinding ) = 0;

	// Indicates globals controlled by the client which specify the frame
	// that the clients have finished with, thereby telling the resource system
	// what resources it can free.
	virtual void RegisterFinishedFrameCounter( uint32 *pFrameCounter ) = 0;
	virtual void UnregisterFinishedFrameCounter( uint32 *pFrameCounter ) = 0;
	void MarkFinishedFrameCounter( uint32 *pFrameCounter );

	// Methods related to resource introspection. Should these go into a separate interface?
	virtual const CResourceStructIntrospection* FindStructIntrospection( ResourceStructureId_t id ) const = 0;
	virtual const CResourceStructIntrospection* FindStructIntrospection( const char *pStructName ) const = 0;
	virtual const CResourceEnumIntrospection* FindEnumIntrospection( ResourceStructureId_t id ) const = 0;
	virtual const CResourceEnumIntrospection* FindEnumIntrospection( const char *pEnumName ) const = 0;
	virtual const CResourceTypedefIntrospection* FindTypedefIntrospection( ResourceStructureId_t id ) const = 0;
	virtual const CResourceTypedefIntrospection* FindTypedefIntrospection( const char *pTypedefName ) const = 0;

	// Returns the value associated with a particular enumeration
	virtual bool FindEnumeratedValue( void *pValue, const char *pEnumName, const char *pEnumValueName, int nDefaultValue ) const = 0;
	virtual const char *FindEnumerationName( const char *pEnumName, int nValue, const char *pDefaultName ) const = 0;

	// TODO: Possibly change this to use a StructIntrospectionHandle_t
	virtual const CResourceStructIntrospection* FindStructIntrospectionByBlockType( ResourceBlockId_t nBlockType ) const = 0;
	virtual const CResourceStructIntrospection* FindStructIntrospectionForResourceType( ResourceType_t nType ) const = 0;
	virtual const CResourceStructIntrospection* FindPermanentStructIntrospectionForResourceType( ResourceType_t nType ) const = 0;

	virtual bool UnpackIntrospectedBlock( const void *pResourceData, size_t nSrcDataSize,
										  const CResourceIntrospection *pSrcIntro, const CResourceStructIntrospection* pSrcStructIntro,
										  const CResourceStructIntrospection* pDstStructIntro,
										  void const **pOutResult, int *pOutResultSize, IntrospectionCompatibilityType_t* pOutCompat ) const = 0;

	// Utility methods
	virtual void GetResourceMapping( const char *pDmeElementType, ResourceType_t *pType, ResourceStructureId_t *pId ) = 0;
	virtual int GetFieldSize( ResourceFieldType_t nType ) const = 0;
	virtual int GetFieldAlignment( ResourceFieldType_t nType ) const = 0;
	virtual const char* GetFieldName( ResourceFieldType_t nType ) const = 0;

	// Methods related to a resource dictionary

	// NOTE: This will become obsolete at some point I believe? It loads a map
	// of resource IDs and possible resource IDs from the specified files.
	virtual void LoadResourceManifest( const char *pFileName ) = 0;

	// Returns a resource manager for a particular resource type
	virtual IResourceTypeManager *GetResourceManager( ResourceType_t nType ) = 0;

	// Frame update.. resources are cached in or out here
	virtual void FrameUpdate() = 0;

	// Block until all type dictionaries are loaded. 
	// NOTE: This call is not necessary to make if you are running the game
	// It's only necessary in tools that use IFileSystem instead of IAsyncFileSystem.
	virtual void BlockUntilAllVTDsLoaded() = 0;

	// Returns the resource manager associated with a particular resource data type
	template < class T > IResourceTypeManager *GetResourceManagerForType( );
};

template < class T > 
inline IResourceTypeManager *IResourceSystem::GetResourceManagerForType( )
{
	return GetResourceManager( ResourceTypeInfo_t< T >::ResourceType() );
}

inline void IResourceSystem::MarkFinishedFrameCounter( uint32 *pFrameCounter )
{
	*pFrameCounter = g_nResourceFrameCount;
}


#endif // IRESOURCESYSTEM_H
