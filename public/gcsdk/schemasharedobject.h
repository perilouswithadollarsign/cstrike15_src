//====== Copyright ©, Valve Corporation, All rights reserved. =======
//
// Purpose: Shared object based on a CBaseRecord subclass
//
//=============================================================================

#ifndef SCHEMASHAREDOBJECT_H
#define SCHEMASHAREDOBJECT_H
#ifdef _WIN32
#pragma once
#endif

#ifndef GC
#error "CSchemaSharedObject is only intended for use on GC-only shared objects. It shouldn't be included on the client"
#endif

#if defined( DEBUG )
#include "gcbase.h"
#endif

namespace GCSDK
{

//----------------------------------------------------------------------------
// Purpose: Contains helper functions to deal with passed in CRecordInfo,
//			so that a CRecordInfo can be built on the fly
//----------------------------------------------------------------------------
class CSchemaSharedObjectHelper
{
public:
	static bool BYieldingAddInsertToTransaction( CSQLAccess & sqlAccess, CRecordBase *pRecordBase );
	static bool BYieldingAddWriteToTransaction( CSQLAccess & sqlAccess, CRecordBase *pRecordBase, const CColumnSet & csDatabaseDirty );
	static bool BYieldingAddRemoveToTransaction( CSQLAccess & sqlAccess, CRecordBase *pRecordBase );
	static bool BYieldingAddToDatabase( CRecordBase *pRecordBase );
	static bool BYieldingReadFromDatabase( CRecordBase *pRecordBase );
	static void Dump( const CRecordBase *pRecordBase );
};

//----------------------------------------------------------------------------
// Purpose: Base class for CSchemaSharedObject. This is where all the actual
//			code lives.
//----------------------------------------------------------------------------
class CSchemaSharedObjectBase : public CSharedObject
{
public:
	typedef CSharedObject BaseClass;

	CSchemaSharedObjectBase( CRecordInfo *pRecordInfo ) : BaseClass() {}

	virtual bool BYieldingAddInsertToTransaction( CSQLAccess & sqlAccess );
	virtual bool BYieldingAddWriteToTransaction( CSQLAccess & sqlAccess, const CUtlVector< int > &fields );
	virtual bool BYieldingAddRemoveToTransaction( CSQLAccess & sqlAccess );
	virtual bool BYieldingAddToDatabase();
	virtual bool BYieldingReadFromDatabase() ;

	// schema shared objects are GC-only and are never sent to clients.
	virtual bool BIsNetworked() const { return false; }
	virtual bool BAddCreateToMessage( CUtlBuffer & bufCreate ) const { return false; }
	virtual bool BAddFullCreateToMessage( CUtlBuffer & bufCreate ) const { return false; }
	virtual bool BAddUpdateToMessage( CUtlBuffer & bufUpdate, const CUtlVector< int > &fields ) const { return false; }
	virtual bool BAddDestroyToMessage( CUtlBuffer & bufDestroy ) const { return false; }
	virtual bool BParseCreateFromMessage( const CUtlBuffer & bufCreate ) { return false; }
	virtual bool BUpdateFromNetwork( const CSharedObject & objUpdate, const CUtlBuffer & bufUpdate ) { return false; }
	virtual bool BIsKeyLess( const CSharedObject & soRHS ) const ;
	virtual void Copy( const CSharedObject & soRHS );
	virtual void Dump() const;
//	virtual bool BIsNetworkDirty() const { return false; }
//	virtual void MakeNetworkClean() {}

	const CRecordInfo * GetRecordInfo() const;


#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const char *pchName );
#endif
protected:
	virtual CRecordBase *GetPObject() = 0;
	const CRecordBase *GetPObject() const { return const_cast<CSchemaSharedObjectBase *>(this)->GetPObject(); }
	bool BParseFieldsFromMessage( CGCMsgBase *pMsg, const CColumnSet & csFields );
	CColumnSet GetDatabaseDirtyColumnSet( const CUtlVector< int > &fields ) const;

//	virtual bool BIsFieldDatabaseDirty( int nField ) const = 0;
//	virtual CColumnSet GetDirtyColumnSet( ) const = 0;

private:
};


//----------------------------------------------------------------------------
// Purpose: Template for making a shared object that uses a specific Schema
//			class for its data store.
//----------------------------------------------------------------------------
template< typename SchObject_t, int nTypeID >
class CSchemaSharedObject : public CSchemaSharedObjectBase
{
public:
	CSchemaSharedObject() 
		: CSchemaSharedObjectBase( GCSDK::GSchemaFull().GetSchema( SchObject_t::k_iTable ).GetRecordInfo() )
	{
	}

	~CSchemaSharedObject()
	{
#ifdef DEBUG
		// Ensure this SO is not in any cache, or we have an error.
		Assert( !GGCBase()->IsSOCached( this, nTypeID ) );
#endif
	}

	virtual int GetTypeID() const { return nTypeID; }

	SchObject_t & Obj() { return m_schObject; }
	const SchObject_t & Obj() const { return m_schObject; }

	typedef SchObject_t SchObjectType_t;
public:
	const static int k_nTypeID = nTypeID;
	const static int k_iFieldMax = SchObject_t::k_iFieldMax;

protected:
	CRecordBase *GetPObject() { return &m_schObject; }
private:
	SchObject_t m_schObject;
};


} // namespace GCSDK


#endif //SCHEMASHAREDOBJECT_H
