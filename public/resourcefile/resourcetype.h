//===== Copyright c 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: Describes our resource format. Resource files consist of two files:
// The first is a resource descriptor file containing various blocks of
// arbitrary data, including a resource dictionary describing raw data which
// is stored in a second, parallel file.
//
// $NoKeywords: $
//===========================================================================//

#ifndef RESOURCETYPE_H
#define RESOURCETYPE_H
#pragma once

#include "resourcefile/schema.h"
#include "tier0/threadtools.h"
#include "tier2/tier2.h" // g_nResourceFrameCount defn

//-----------------------------------------------------------------------------
// Enum definitions
//-----------------------------------------------------------------------------
schema enum ResourceTypeEngine_t
{
	RESOURCE_TYPE_NONE = -1,

	//! schemaName = "TextureBits_t"
	//! className = "CTextureBits"
	//! handlePrefix = "RenderTexture2"
	RESOURCE_TYPE_TEXTURE = 0,

	//! schemaName = "ParticleSystemDefinition_t"; handlePrefix = "ParticleSystem"
	RESOURCE_TYPE_PARTICLE_SYSTEM,

	//! schemaName = "Sheet_t"; className = "CSheet"; handlePrefix = "Sheet"
	RESOURCE_TYPE_SHEET,

	//! schemaName = "RenderBufferBits_t"; className = "CRenderBufferBits"; handlePrefix = "RenderBuffer"
	RESOURCE_TYPE_RENDER_BUFFER,

	//! schemaName = "Renderable_t"; className = "CRenderable"; handlePrefix = "Renderable"
	RESOURCE_TYPE_RENDERABLE,

	//! schemaName = "WorldNode_t"; className = "CWorldNode"; handlePrefix = "WorldNode"
	RESOURCE_TYPE_WORLD_NODE,

	//! schemaName = "World_t"; className = "CWorld"; handlePrefix = "World"
	RESOURCE_TYPE_WORLD,

	RESOURCE_TYPE_COUNT,
	RESOURCE_FIRST_MOD_TYPE = 0x80000000,
};

//-----------------------------------------------------------------------------
// Resource type.. typedeffed because we can have engine or mod types
//-----------------------------------------------------------------------------
typedef int ResourceType_t;


//-----------------------------------------------------------------------------
// Resource binding flags
//-----------------------------------------------------------------------------
enum ResourceBindingFlags_t
{
	RESOURCE_BINDING_CACHED = 0x1,
	RESOURCE_BINDING_ERROR = 0x2,
	RESOURCE_BINDING_PERMANENT = 0x4,	// Will never be uncached
	RESOURCE_BINDING_ANONYMOUS = 0x8,	// Will never be in the map

	RESOURCE_BINDING_FIRST_UNUSED_FLAG = 0x10,
};


//-----------------------------------------------------------------------------
// Resource binding
// NOTE: This is a private implementational detail of the resource system
// and should never be used directly by client code
//-----------------------------------------------------------------------------
struct ResourceBindingBase_t
{
	void *m_pData;							// Needs to be a pointer type or a handle (RenderTextureHandle_t etc)
	mutable uint32 m_nLastBindFrame;		// When's the last frame this was used?
	uint32 m_nFlags;						// Flags
	mutable CInterlockedInt m_nRefCount;	// How many resource pointers point to this resource?
};

template< class T >
struct ResourceBinding_t : public ResourceBindingBase_t
{
	template< class S > friend class CStrongHandle;
	template< class S > friend const S* ResourceHandleToData( const ResourceBinding_t< S > *hResource, bool bUnsafe );
	template< class S > friend int ResourceAddRef( const ResourceBinding_t< S > *hResource );
	template< class S > friend int ResourceRelease( const ResourceBinding_t< S > *hResource );
};


// This is a 'weak reference' and can only be used if it's referenced
// somewhere and if it's the appropriate frame
template< class T > FORCEINLINE const T* ResourceHandleToData( const ResourceBinding_t< T > *hResource, bool bUnsafe = false )
{
	Assert( !hResource || bUnsafe || hResource->m_nRefCount > 0 );
	Assert( !hResource || bUnsafe || ( hResource->m_nFlags & RESOURCE_BINDING_PERMANENT ) || ( hResource->m_nLastBindFrame == g_nResourceFrameCount ) );
	return hResource ? ( const T* )hResource->m_pData : NULL;
}

template< class T > FORCEINLINE int ResourceAddRef( const ResourceBinding_t< T > *hResource )
{
	return ++hResource->m_nRefCount;
}

template< class T > FORCEINLINE int ResourceRelease( const ResourceBinding_t< T > *hResource )
{
	Assert( hResource->m_nRefCount > 0 );
	return --hResource->m_nRefCount;
}


//-----------------------------------------------------------------------------
// Declares a resource pointer
//-----------------------------------------------------------------------------
typedef const ResourceBindingBase_t *ResourceHandle_t;
#define RESOURCE_HANDLE_INVALID ( (ResourceHandle_t)0 )


//-----------------------------------------------------------------------------
// Forward declaration
//-----------------------------------------------------------------------------
template< class T > class CStrongHandle;


//-----------------------------------------------------------------------------
// Helpers used to define resource types + associated structs + dm elements
//-----------------------------------------------------------------------------
template< class ResourceStruct_t >
struct ResourceTypeInfo_t
{
	enum { RESOURCE_TYPE = RESOURCE_TYPE_NONE };
	typedef ResourceStruct_t Schema_t;
	typedef ResourceStruct_t Class_t;
	static ResourceType_t ResourceType() { return RESOURCE_TYPE_NONE; }
	static const char *SchemaName() { return "unknown"; }
	static const char *ClassName() { return "unknown"; }
};

#define DEFINE_RESOURCE_TYPE( _schema, _resourceType )			\
	template <> struct ResourceTypeInfo_t< _schema >			\
	{															\
		enum { RESOURCE_TYPE = _resourceType };					\
		typedef _schema Schema_t;								\
		typedef _schema Class_t;								\
		static ResourceType_t ResourceType() { return _resourceType; }	\
		static const char *SchemaName() { return #_schema; }	\
		static const char *ClassName() { return #_schema; }		\
	};

#define DEFINE_RESOURCE_CLASS_TYPE( _schema, _class, _resourceType )	\
	template <> struct ResourceTypeInfo_t< _schema >			\
	{															\
		enum { RESOURCE_TYPE = _resourceType };					\
		typedef _schema Schema_t;								\
		typedef _class Class_t;									\
		static ResourceType_t ResourceType() { return _resourceType; }	\
		static const char *SchemaName() { return #_schema; }	\
		static const char *ClassName() { return #_class; }		\
	};															\
	template <> struct ResourceTypeInfo_t< _class >				\
	{															\
		enum { RESOURCE_TYPE = _resourceType };					\
		typedef _schema Schema_t;								\
		typedef _class Class_t;									\
		static ResourceType_t ResourceType() { return _resourceType; }	\
		static const char *SchemaName() { return #_schema; }	\
		static const char *ClassName() { return #_class; }		\
	};															\

//-----------------------------------------------------------------------------
// This contains an external resource reference
//-----------------------------------------------------------------------------
typedef const struct ResourceBindingBase_t *ResourceHandle_t;

template <typename T>
class CResourceReference
{
private:
	union
	{
		int64 m_nResourceId;			// On-disk format
		ResourceHandle_t m_hResource;	// Run-time format. ResourceSystem is responsible for conversion
	};

public:
	// Tool-time only
	void WriteReference( uint32 nId )
	{
		m_nResourceId = nId;
	}

	void operator=( uint32 nId )
	{
		m_nResourceId = nId;
	}

	bool operator==( int nZero ) const
	{
		Assert( nZero == 0 );
		return m_nResourceId == 0;
	}

	bool IsNull()const
	{
		return m_nResourceId == 0;
	}

	const ResourceBinding_t< T > *GetHandle() const
	{
		// validate
		Assert( g_pResourceSystem );

		const ResourceBinding_t< T > *pBinding = (const ResourceBinding_t< T > *)m_hResource;
		pBinding->m_nLastBindFrame = g_nResourceFrameCount;
		return pBinding;
	}

	const T* GetPtr() const
	{
		// validate
		Assert( g_pResourceSystem );

		const ResourceBinding_t< T > *pBinding = (const ResourceBinding_t< T > *)m_hResource;
		pBinding->m_nLastBindFrame = g_nResourceFrameCount;
		return ResourceHandleToData( pBinding );
	}

	operator const T* ()const
	{
		return GetPtr();
	}

	const T* operator->() const
	{
		return GetPtr();
	}

	// NOTE: The following two methods should only be used during resource fixup on load
	uint32 GetResourceId() const
	{
		return m_nResourceId;
	}

	void SetHandle( ResourceHandle_t hResource )
	{
		m_hResource = hResource;
	}
};


#endif // RESOURCETYPE_H
