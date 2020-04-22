//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: Shared object based on a CBaseRecord subclass
//
//=============================================================================

#ifndef PROTOBUFSHAREDOBJECT_H
#define PROTOBUFSHAREDOBJECT_H
#ifdef _WIN32
#pragma once
#endif

#include "google/protobuf/descriptor.h"
#include "tier1/keyvalues.h"

#if defined( GC ) && defined( DEBUG )
#include "gcbase.h"
#endif

namespace google
{
	namespace protobuf
	{
		class Message;
	}
}

namespace GCSDK
{

//----------------------------------------------------------------------------
// Purpose: Base class for CProtoBufSharedObject. This is where all the actual
//			code lives.
//----------------------------------------------------------------------------
class CProtoBufSharedObjectBase : public CSharedObject
{
public:
	typedef CSharedObject BaseClass;

	virtual bool BParseFromMessage( const CUtlBuffer & buffer ) OVERRIDE;
	virtual bool BParseFromMessage( const std::string &buffer ) OVERRIDE;
	virtual bool BUpdateFromNetwork( const CSharedObject & objUpdate ) OVERRIDE;

	virtual bool BAddToMessage( std::string *pBuffer ) const OVERRIDE;
	virtual bool BAddDestroyToMessage( std::string *pBuffer ) const OVERRIDE;

	virtual bool BIsKeyLess( const CSharedObject & soRHS ) const ;
	virtual void Copy( const CSharedObject & soRHS );
	virtual void Dump() const OVERRIDE;


#ifdef GC

	virtual bool BParseFromMemcached( CUtlBuffer & buffer ) OVERRIDE;
	virtual bool BAddToMemcached( CUtlBuffer & bufOutput ) const OVERRIDE;
#endif //GC

	// Static helpers
	static bool SerializeToBuffer( const ::google::protobuf::Message & msg, CUtlBuffer & bufOutput );
	static void Dump( const ::google::protobuf::Message & msg );

protected:
	virtual ::google::protobuf::Message *GetPObject() = 0;
	const ::google::protobuf::Message *GetPObject() const { return const_cast<CProtoBufSharedObjectBase *>(this)->GetPObject(); }

private:
	static ::google::protobuf::Message *BuildDestroyToMessage( const ::google::protobuf::Message & msg );
};


//----------------------------------------------------------------------------
// Purpose: Template for making a shared object that uses a specific protobuf
//			message class for its wire protocol and in-memory representation.
//----------------------------------------------------------------------------
template< typename Message_t, int nTypeID >
class CProtoBufSharedObject : public CProtoBufSharedObjectBase
{
public:
	~CProtoBufSharedObject()
	{
#if defined( GC ) && defined( DEBUG )
		// Ensure this SO is not in any cache, or we have an error. We must provide the type since it is a virutal function otherwise
		Assert( !GGCBase()->IsSOCached( this, nTypeID ) );
#endif
	}

	virtual int GetTypeID() const { return nTypeID; }

	Message_t & Obj() { return m_msgObject; }
	const Message_t & Obj() const { return m_msgObject; }

	typedef Message_t SchObjectType_t;
public:
	const static int k_nTypeID = nTypeID;

protected:
	::google::protobuf::Message *GetPObject() { return &m_msgObject; }

private:
	Message_t m_msgObject;
};

//----------------------------------------------------------------------------
// Purpose: Special protobuf shared object that caches its serialized form
//----------------------------------------------------------------------------
template< typename Message_t, int nTypeID >
class CProtoBufCachedSharedObject : public CProtoBufSharedObject< Message_t, nTypeID >
{
#ifdef GC
public:
	virtual bool BAddToMessage( std::string *pBuffer ) const OVERRIDE
	{
		UpdateCache();
		*pBuffer = m_cachedSerialize;
		return true;
	}

	void ClearCache()  // You must call this when your object changes or your object won't update on the client!
	{
		m_cachedSerialize.clear();
	}

private:
	void UpdateCache() const
	{
		if ( m_cachedSerialize.empty() )
		{
			Obj().SerializeToString( &m_cachedSerialize );
		}
	}

	mutable std::string m_cachedSerialize;
#endif //GC
};

} // GCSDK namespace

#endif //PROTOBUFSHAREDOBJECT_H
