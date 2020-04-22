//========= Copyright ©, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef GCSQLQUERY_H
#define GCSQLQUERY_H
#ifdef _WIN32
#pragma once
#endif

#include "gamecoordinator/igcsqlquery.h"
#include "refcount.h"
#include "tier0/memdbgon.h"

namespace GCSDK
{

struct GCSQLBindParam_t
{
	EGCSQLType	m_eType;
	uint8	*m_pubData;
	size_t		m_cubData;
};


class CGCSQLQuery 
{
public:
	CGCSQLQuery();
	virtual ~CGCSQLQuery();

	void SetCommand( const char *pchCommand ) { m_sCommand = pchCommand; }

	void AddBindParam( const char *pchValue )
	{
		AddBindParamRaw( k_EGCSQLType_String, (byte *)pchValue, Q_strlen( pchValue ) );
	}

	void AddBindParam( const int16 nValue )
	{
		AddBindParamRaw( k_EGCSQLType_int16, (byte *)&nValue, sizeof( nValue ) );
	}

	void AddBindParam( const uint16 uValue )
	{
		AddBindParamRaw( k_EGCSQLType_int16, (byte *)&uValue, sizeof( uValue ) );
	}

	void AddBindParam( const int32 nValue )
	{
		AddBindParamRaw( k_EGCSQLType_int32, (byte *)&nValue, sizeof( nValue ) );
	}

	void AddBindParam( const uint32 uValue )
	{
		AddBindParamRaw( k_EGCSQLType_int32, (byte *)&uValue, sizeof( uValue ) );
	}

	void AddBindParam( const uint64 ulValue )
	{
		AddBindParamRaw( k_EGCSQLType_int64, (byte *)&ulValue, sizeof( ulValue ) );
	}

	void AddBindParam( const uint8 *ubValue, const int cubValue )
	{
		AddBindParamRaw( k_EGCSQLType_Blob, (byte *)ubValue, cubValue );
	}

	void AddBindParam( const float fValue )
	{
		AddBindParamRaw( k_EGCSQLType_float, (byte *)&fValue, sizeof ( fValue ) );
	}

	void AddBindParam( const double dValue )
	{
		AddBindParamRaw( k_EGCSQLType_double, (byte *)&dValue, sizeof ( dValue ) );
	}

	void ClearParams();

	// this is used internally to bind a field with its type. You probably want 
	// some version of AddBindParam instead of this.
	void AddBindParamRaw( EGCSQLType eType, const byte *pubData, uint32 cubData );

	// ------- Interface implementation from IGCSQLQuery -----

	// get the null-terminated query string itself
	virtual const char *PchCommand() { return m_sCommand.Get(); }

	// gets the parameter data
	virtual uint32 CnParams() { return m_vecParams.Count(); }
	virtual EGCSQLType EParamType( uint32 uIndex ) { return m_vecParams[uIndex].m_eType; }
	virtual byte *PubParam( uint32 uIndex ) { return m_vecParams[uIndex].m_pubData; }
	virtual uint32 CubParam( uint32 uIndex ) { return m_vecParams[uIndex].m_cubData; }

private:

	CUtlString						m_sCommand;
	CUtlVector< GCSQLBindParam_t >	m_vecParams;
};

class CGCSQLQueryGroup : public IGCSQLQuery, public CRefCount
{
	// create query groups on the heap with alloc
	CGCSQLQueryGroup();

	// destroy query groups by releasing them
	virtual ~CGCSQLQueryGroup();
public:
	static CGCSQLQueryGroup *Alloc() { return new CGCSQLQueryGroup(); }
		
	void AddQuery( CGCSQLQuery *pQuery );
	void SetName( const char *sName );

	// returns the number of statements in the transaction 
	// represented by this query object
	virtual uint32 GetStatementCount() { return m_vecQueries.Count(); }

	// returns a string that represents where in the GC this
	// query came from.  Usually this is FILE_AND_LINE.
	virtual const char *PchName() { return m_sName; }

	// get the null-terminated query string itself
	virtual const char *PchCommand( uint32 unStatement ) { return m_vecQueries[unStatement]->PchCommand(); }

	// gets the parameter data
	virtual uint32 CnParams( uint32 unStatement ) { return m_vecQueries[unStatement]->CnParams(); }
	virtual EGCSQLType EParamType( uint32 unStatement, uint32 uIndex ) { return m_vecQueries[unStatement]->EParamType( uIndex ); }
	virtual byte *PubParam( uint32 unStatement, uint32 uIndex )  { return m_vecQueries[unStatement]->PubParam( uIndex ); }
	virtual uint32 CubParam( uint32 unStatement, uint32 uIndex )  { return m_vecQueries[unStatement]->CubParam( uIndex ); };

	// reports the result
	virtual void SetResults( IGCSQLResultSetList *pResults );
	IGCSQLResultSetList *GetResults() { return m_pResults; }

	// clears all the queries in the query group and resets its name
	void Clear();
private:
	CUtlVector< CGCSQLQuery * >		m_vecQueries;
	CUtlString						m_sName;
	IGCSQLResultSetList				*m_pResults;
};

} // namespace GCSDK

#include "tier0/memdbgoff.h"

#endif // GCSQLQUERY_H
