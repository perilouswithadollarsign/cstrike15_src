//====== Copyright (c), Valve Corporation, All rights reserved. =======
//
// Purpose: Provides access to SQL at a high level
//
//=============================================================================

#ifndef SQLACCESS_H
#define SQLACCESS_H
#ifdef _WIN32
#pragma once
#endif

#include "gcsdk/gcsqlquery.h"

#include "tier0/memdbgon.h"

namespace GCSDK
{
class CGCSQLQuery;
class CGCSQLQueryGroup;
class CColumnSet;
class CRecordType;

//-----------------------------------------------------------------------------
// Purpose: Provides access to SQL at a high level
//-----------------------------------------------------------------------------
class CSQLAccess
{
public:
	CSQLAccess( ESchemaCatalog eSchemaCatalog = k_ESchemaCatalogMain );
	~CSQLAccess( );

	bool BBeginTransaction( const char *pchName );
	bool BCommitTransaction( bool bAllowEmpty = false );
	void RollbackTransaction();
	bool BInTransaction( ) const { return m_bInTransaction; }

	bool BYieldingExecute( const char *pchName, const char *pchSQLCommand, uint32 *pcRowsAffected = NULL, bool bSpewOnError = true );
	bool BYieldingExecuteString( const char *pchName, const char *pchSQLCommand, CFmtStr1024 *psResult, uint32 *pcRowsAffected = NULL );
	bool BYieldingExecuteScalarInt( const char *pchName, const char *pchSQLCommand, int *pnResult, uint32 *pcRowsAffected = NULL );
	bool BYieldingExecuteScalarIntWithDefault( const char *pchName, const char *pchSQLCommand, int *pnResult, int iDefaultValue, uint32 *pcRowsAffected = NULL );
	bool BYieldingExecuteScalarUint32( const char *pchName, const char *pchSQLCommand, uint32 *punResult, uint32 *pcRowsAffected = NULL );
	bool BYieldingExecuteScalarUint32WithDefault( const char *pchName, const char *pchSQLCommand, uint32 *punResult, uint32 unDefaultValue, uint32 *pcRowsAffected = NULL );
	bool BYieldingWipeTable( int iTable );

	template <typename TReturn, typename TCast>
	bool BYieldingExecuteSingleResult( const char *pchName, const char *pchSQLCommand, EGCSQLType eType, TReturn *pResult, uint32 *pcRowsAffected );
	template <typename TReturn, typename TCast>
	bool BYieldingExecuteSingleResultWithDefault( const char *pchName, const char *pchSQLCommand, EGCSQLType eType, TReturn *pResult, TReturn defaultValue, uint32 *pcRowsAffected );

	// manipulating CRecordBase (i.e. CSch...) objects in the database
	bool BYieldingInsertRecord( CRecordBase *pRecordBase );
	bool BYieldingInsertWithIdentity( CRecordBase* pRecordBase ) ;
	bool BYieldingReadRecordWithWhereColumns( CRecordBase *pRecord, const CColumnSet & readSet, const CColumnSet & whereSet );

	template< typename SchClass_t >
	bool BYieldingReadRecordFromPK( SchClass_t *pRecord );
	template< typename SchClass_t>
	bool BYieldingReadMultipleRecordsWithWhereColumns( CUtlVector< SchClass_t > *pvecRecords, const CColumnSet & whereSet, CUtlVector< SchClass_t > *pvecUnmatchedRecords = NULL );
	template< typename SchClass_t>
	bool BYieldingReadMultipleRecordsWithWhereColumns( CUtlVector< SchClass_t > *pvecRecords, const CColumnSet & readSet, const CColumnSet & whereSet, CUtlVector< SchClass_t > *pvecUnmatchedRecords = NULL );
	template< typename SchClass_t>
	bool BYieldingReadRecordsWithWhereClause( CUtlVector< SchClass_t > *pvecRecords, const char *pchWhereClause, const CColumnSet & readSet, const char *pchTopClause = NULL );
	template< typename SchClass_t>
	bool BYieldingReadRecordsWithQuery( CUtlVector< SchClass_t > *pvecRecords, const char *sQuery, const CColumnSet & readSet );
	bool BYieldingUpdateRecord( const CRecordBase &record, const CColumnSet & whereColumns, const CColumnSet & updateColumns );
	bool BYieldingDeleteRecord( const CRecordBase & record, const CColumnSet & whereColumns );

	void AddRecordParameters( const CRecordBase &record, const CColumnSet & columnSet );

	void AddBindParam( const char *pchValue );
	void AddBindParam( const int16 nValue );
	void AddBindParam( const uint16 uValue );
	void AddBindParam( const int32 nValue );
	void AddBindParam( const uint32 uValue );
	void AddBindParam( const uint64 ulValue );
	void AddBindParam( const uint8 *ubValue, const int cubValue );
	void AddBindParam( const float fValue );
	void AddBindParam( const double dValue );
	void ClearParams();
	IGCSQLResultSetList *GetResults();

	uint32 GetResultSetCount();
	uint32 GetResultSetRowCount( uint32 unResultSet );
	CSQLRecord GetResultRecord( uint32 unResultSet, uint32 unRow );

private:
	enum EReadSingleResultResult
	{
		eReadSingle_Error,			// something went wrong in the DB or the data was in a format we didn't expect
		eReadSingle_ResultFound,	// we found a single result and copied the value -- all is well!
		eReadSingle_UseDefault,		// we didn't find any results but we specified a value in advance for this case
	};

	EReadSingleResultResult BYieldingExecuteSingleResultDataInternal( const char *pchName, const char *pchSQLCommand, EGCSQLType eType, uint8 **pubData, uint32 *punSize, uint32 *pcRowsAffected, bool bHasDefaultValue );

private:

	CGCSQLQuery *CurrentQuery();
	ESchemaCatalog m_eSchemaCatalog;
	CGCSQLQuery *m_pCurrentQuery;
	CGCSQLQueryGroup *m_pQueryGroup;
	bool m_bInTransaction;
};

#define FOR_EACH_SQL_RESULT( sqlAccess, resultSet, record ) \
	for( CSQLRecord record = (sqlAccess).GetResultRecord( resultSet, 0 ); record.IsValid(); record.NextRow() )


//-----------------------------------------------------------------------------
// Purpose: templatized version of querying for a single value
//-----------------------------------------------------------------------------
template <typename TReturn, typename TCast>
bool CSQLAccess::BYieldingExecuteSingleResult( const char *pchName, const char *pchSQLCommand, EGCSQLType eType, TReturn *pResult, uint32 *pcRowsAffected )
{
	uint8 *pubData;
	uint32 cubData;
	if( CSQLAccess::BYieldingExecuteSingleResultDataInternal( pchName, pchSQLCommand, eType, &pubData, &cubData, pcRowsAffected, false ) != eReadSingle_ResultFound )
		return false;

	*pResult = *( (TCast *)pubData );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: templatized version of querying for a single value
//-----------------------------------------------------------------------------
template <typename TReturn, typename TCast>
bool CSQLAccess::BYieldingExecuteSingleResultWithDefault( const char *pchName, const char *pchSQLCommand, EGCSQLType eType, TReturn *pResult, TReturn defaultValue, uint32 *pcRowsAffected )
{
	uint8 *pubData;
	uint32 cubData;
	EReadSingleResultResult eResult = CSQLAccess::BYieldingExecuteSingleResultDataInternal( pchName, pchSQLCommand, eType, &pubData, &cubData, pcRowsAffected, true );

	if ( eResult == eReadSingle_Error )
		return false;
	
	if ( eResult == eReadSingle_ResultFound )
	{
		*pResult = *( (TCast *)pubData );
	}
	else
	{
		Assert( eResult == eReadSingle_UseDefault );
		*pResult = defaultValue;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Reads the record with a given PK.
// Input:	pRecordBase - record to read
// Output:	true if successful, false otherwise
//-----------------------------------------------------------------------------
template < typename SchClass_t >
bool CSQLAccess::BYieldingReadRecordFromPK( SchClass_t *pRecord )
{
	CColumnSet csetWhere = CColumnSet::PrimaryKey< SchClass_t >();
	CColumnSet csetRead = CColumnSet::Inverse( csetWhere );
	return BYieldingReadRecordWithWhereColumns( pRecord, csetRead, csetWhere );
}


//-----------------------------------------------------------------------------
// Purpose: Reads multiple records from the database based on the where columns
//			filled in for each record. If the record is not found in the database 
//			it will be removed from pvecRecords. If pvecUnmatchedRecords is
//			provided, it will be populated with the unmatched records removed
//			from pvecRecords
// Input:	pvecRecords - The records to fill in from the database
//			whereSet - The set of columns to query on
//			(optional) pvecUnmatchedRecords - A vector to hold records which
//				are not found in the database
// Output:	true if successful, false otherwise
//-----------------------------------------------------------------------------
template< typename SchClass_t>
bool CSQLAccess::BYieldingReadMultipleRecordsWithWhereColumns( CUtlVector< SchClass_t > *pvecRecords, 
															  const CColumnSet & whereSet, 
															  CUtlVector< SchClass_t > *pvecUnmatchedRecords /* = NULL */ )
{
	CColumnSet readSet( GSchemaFull().GetSchema( SchClass_t::k_iTable ).GetRecordInfo() );
	readSet.MakeInverse( whereSet );
	return BYieldingReadMultipleRecordsWithWhereColumns( pvecRecords, readSet, whereSet, pvecUnmatchedRecords );
}


//-----------------------------------------------------------------------------
// Purpose: Reads multiple records from the database based on the where columns
//			filled in for each record. If the record is not found in the database 
//			it will be removed from pvecRecords. If pvecUnmatchedRecords is
//			provided, it will be populated with the unmatched records removed
//			from pvecRecords
// Input:	pvecRecords - The records to fill in from the database
//			readSet - The set of columns to fill in
//			whereSet - The set of columns to query on
//			(optional) pvecUnmatchedRecords - A vector to hold records which
//				are not found in the database
// Output:	true if successful, false otherwise
//-----------------------------------------------------------------------------
template< typename SchClass_t>
bool CSQLAccess::BYieldingReadMultipleRecordsWithWhereColumns( CUtlVector< SchClass_t > *pvecRecords, 
															   const CColumnSet & readSet, 
															   const CColumnSet & whereSet, 
															   CUtlVector< SchClass_t > *pvecUnmatchedRecords /* = NULL */ )
{
	AssertMsg( !BInTransaction(),  "BYieldingReadMultipleRecordsWithWhereColumns is not supported in a transaction" );
	if( BInTransaction() )
		return false;

	Assert( !readSet.IsEmpty() );
	if ( readSet.IsEmpty() )
		return false;

	if ( pvecUnmatchedRecords )
	{
		pvecUnmatchedRecords->RemoveAll();
	}

	// Build the query we'll use for each record
	CFmtStr1024 sStatement, sWhere;
	BuildSelectStatementText( &sStatement, readSet );
	BuildWhereClauseText( &sWhere, whereSet );
	sStatement.Append( " WHERE " );
	sStatement.Append( sWhere );

	BBeginTransaction( CFmtStr1024( "BYieldingReadMultipleRecordsWithWhereColumns() - %s", sStatement.Access() ) );

	// Batch this query for each record
	FOR_EACH_VEC( *pvecRecords, i )
	{
		AddRecordParameters( pvecRecords->Element( i ), whereSet );
		if( !BYieldingExecute( NULL, sStatement ) )
			return false;
	}

	// Actually run the query
	if ( !BCommitTransaction() )
		return false;

	Assert( GetResultSetCount() == (uint32)pvecRecords->Count() );
	if ( GetResultSetCount() != (uint32)pvecRecords->Count() )
		return false;

	// Get the results. Reading backwards because if a record doesn't find a match we'll
	// remove it from the list
	FOR_EACH_VEC_BACK( *pvecRecords, i )
	{
		// make sure the types are the same
		IGCSQLResultSet *pResultSet = m_pQueryGroup->GetResults()->GetResultSet( i );
		Assert( pResultSet->GetRowCount() <= 1 );
		if ( pResultSet->GetRowCount() > 1 )
			return false;
		
		if( pResultSet->GetRowCount() == 1 )
		{
			// We have a record in this set, read it in
			FOR_EACH_COLUMN_IN_SET( readSet, nColumnIndex )
			{
				EGCSQLType eRecordType = readSet.GetColumnInfo( nColumnIndex ).GetType();
				EGCSQLType eResultType = pResultSet->GetColumnType( nColumnIndex );

				Assert( eResultType == eRecordType );
				if( eRecordType != eResultType )
					return false;
			}

			CSQLRecord sqlRecord = GetResultRecord( i, 0 );

			FOR_EACH_COLUMN_IN_SET( readSet, nColumnIndex )
			{
				uint8 *pubData;
				uint32 cubData;

				DbgVerify( sqlRecord.BGetColumnData( nColumnIndex, &pubData, (int*)&cubData ) );
				DbgVerify( pvecRecords->Element( i ).BSetField( readSet.GetColumn( nColumnIndex ), pubData, cubData ) );
			}
		}
		else
		{
			// This record did not match, remove it and add it to pvecUnmatchedRecords if needed
			if ( pvecUnmatchedRecords )
			{
				pvecUnmatchedRecords->AddToTail( pvecRecords->Element( i ) );
			}

			pvecRecords->Remove( i );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Reads a list of records from the DB according to the specified where
//			clause
// Input:	pRecordBase - record to insert
// Output:	true if successful, false otherwise
//-----------------------------------------------------------------------------
template< typename SchClass_t>
bool CSQLAccess::BYieldingReadRecordsWithWhereClause( CUtlVector< SchClass_t > *pvecRecords, const char *pchWhereClause, const CColumnSet & readSet, const char *pchTopClause )
{
	AssertMsg( !BInTransaction(),  "BYieldingReadRecordsWithWhereClause is not supported in a transaction" );
	if( BInTransaction() )
		return false;

	Assert( !readSet.IsEmpty() );
	CFmtStr1024 sStatement;
	BuildSelectStatementText( &sStatement, readSet, pchTopClause );
	Assert( pchWhereClause && *pchWhereClause );
	if( !pchWhereClause || !(*pchWhereClause) )
		return false;

	CUtlString sFullStatement = sStatement.Access();
	sFullStatement += " WHERE ";
	sFullStatement += pchWhereClause;

	return BYieldingReadRecordsWithQuery< SchClass_t >( pvecRecords, sFullStatement, readSet );
}


//-----------------------------------------------------------------------------
// Purpose: Inserts a new record into the DB and reads non-insertable fields back
//			into the record.
// Input:	pRecordBase - record to insert
// Output:	true if successful, false otherwise
//-----------------------------------------------------------------------------
template< typename SchClass_t>
bool CSQLAccess::BYieldingReadRecordsWithQuery( CUtlVector< SchClass_t > *pvecRecords, const char *sQuery, const CColumnSet & readSet )
{
	AssertMsg( !BInTransaction(),  "BYieldingReadRecordsWithQuery is not supported in a transaction" );
	if( BInTransaction() )
		return false;

	Assert(!readSet.IsEmpty() );
	if( !BYieldingExecute( NULL, sQuery ) )
		return false;

	Assert( GetResultSetCount() == 1 );
	if ( GetResultSetCount() != 1 )
		return false;

	// make sure the types are the same
	IGCSQLResultSet *pResultSet = m_pQueryGroup->GetResults()->GetResultSet( 0 );
	return CopyResultToSchVector( pResultSet, readSet, pvecRecords );
}

} // namespace GCSDK

#include "tier0/memdbgoff.h"

#endif // SQLACCESS_H
