//========= Copyright © 1996-2004, Valve LLC, All rights reserved. ============
//
// Purpose: Sets of columns in queries
//
// $NoKeywords: $
//=============================================================================

#ifndef COLUMNSET_H
#define COLUMNSET_H

#ifdef _WIN32
#pragma once
#endif

namespace GCSDK
{

//-----------------------------------------------------------------------------
// Purpose: Sets of columns in queries
//-----------------------------------------------------------------------------
class CColumnSet
{
public:
	CColumnSet( const CRecordInfo *pRecordInfo );
	CColumnSet( const CRecordInfo *pRecordInfo, int nColumn );
	CColumnSet( const CColumnSet & rhs );
	CColumnSet & operator=( const CColumnSet & rhs );
	const CColumnSet operator+( const CColumnSet & rhs ) const;
	CColumnSet & operator+=( const CColumnSet & rhs );

	bool BAddColumn( int nColumn );
	bool BRemoveColumn( int nColumn );
	bool IsSet( int nColumn ) const;
	bool IsEmpty() const { return m_vecColumns.Count() ==  0;}

	uint32 GetColumnCount() const;
	int GetColumn( int nIndex ) const;
	const CColumnInfo & GetColumnInfo( int nIndex ) const;

	const CRecordInfo *GetRecordInfo() const { return m_pRecordInfo; }

	// putting column sets in messages
	void AddToMessage( CGCMsgBase *pMsg ) const;
	bool BParseFromMessage( CGCMsgBase *pMsg );

	void MakeEmpty();
	void MakeFull();
	void MakeInsertable();
	void MakeNoninsertable();
	void MakePrimaryKey();
	void MakeInverse( const CColumnSet & columnSet );

	template< typename TSchClass >
	static CColumnSet Empty();
	template< typename TSchClass >
	static CColumnSet Full();
	template< typename TSchClass >
	static CColumnSet Insertable();
	template< typename TSchClass >
	static CColumnSet Noninsertable();
	template< typename TSchClass >
	static CColumnSet PrimaryKey();
	static CColumnSet Inverse( const CColumnSet & columnSet );

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );
#endif

private:
	CUtlVector<int> m_vecColumns;
	const CRecordInfo *m_pRecordInfo;
};

// Usage notes:
//		The fields in a column set are order-dependent, and must match the order of the fields in
//		the query used to generate the data. The code that reads values doesn't do any fancy
//		name-matching and will copy values to incorrect locations silently if there is a
//		disagreement between the fields in the query and the fields in the column set.
//
// Examples:
//		// This is broken.
//		query	  = "SELECT * FROM Items";
//		columnSet = CSET_12_COL( CSchItem, individual_field_names );
//
//		// This is fixed.
//		query	  = "SELECT * FROM Items";
//		columnSet = CSET_FULL( ... );

#define FOR_EACH_COLUMN_IN_SET( columnSet, iterName )		for( uint32 iterName = 0; iterName < (columnSet).GetColumnCount(); iterName++ )

#define CSET_EMPTY( schClass ) CColumnSet::Empty<schClass>()
#define CSET_FULL( schClass ) CColumnSet::Full<schClass>()
#define CSET_INSERTABLE( schClass ) CColumnSet::Insertable<schClass>()
#define CSET_NONINSERTABLE( schClass ) CColumnSet::Noninsertable<schClass>()
#define CSET_PK( schClass ) CColumnSet::PrimaryKey<schClass>()

#define CSET_1_COL( schClass, col1 ) \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1 )

#define CSET_2_COL( schClass, col1, col2 ) \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col2 )

#define CSET_3_COL( schClass, col1, col2, col3 ) \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col2 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col3 )

#define CSET_4_COL( schClass, col1, col2, col3, col4 ) \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col2 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col3 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col4 )

#define CSET_5_COL( schClass, col1, col2, col3, col4, col5 ) \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col2 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col3 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col4 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col5 )

#define CSET_6_COL( schClass, col1, col2, col3, col4, col5, col6 ) \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col2 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col3 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col4 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col5 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col6 )

#define CSET_7_COL( schClass, col1, col2, col3, col4, col5, col6, col7 ) \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col2 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col3 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col4 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col5 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col6 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col7 )

#define CSET_8_COL( schClass, col1, col2, col3, col4, col5, col6, col7, col8 ) \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col2 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col3 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col4 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col5 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col6 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col7 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col8 )

#define CSET_9_COL( schClass, col1, col2, col3, col4, col5, col6, col7, col8, col9 ) \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col2 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col3 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col4 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col5 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col6 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col7 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col8 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col9 )

#define CSET_10_COL( schClass, col1, col2, col3, col4, col5, col6, col7, col8, col9, col10 ) \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col2 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col3 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col4 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col5 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col6 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col7 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col8 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col9 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col10 )

#define CSET_11_COL( schClass, col1, col2, col3, col4, col5, col6, col7, col8, col9, col10, col11 ) \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col2 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col3 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col4 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col5 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col6 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col7 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col8 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col9 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col10 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col11 )

#define CSET_12_COL( schClass, col1, col2, col3, col4, col5, col6, col7, col8, col9, col10, col11, col12 ) \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col1 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col2 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col3 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col4 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col5 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col6 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col7 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col8 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col9 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col10 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col11 ) + \
	CColumnSet( GSchemaFull().GetSchema( schClass::k_iTable ).GetRecordInfo(), schClass::col12 )

//-----------------------------------------------------------------------------
// Purpose: Returns an empty Column Set for a schema class
//-----------------------------------------------------------------------------
template< typename TSchClass >
CColumnSet CColumnSet::Empty()
{
	CColumnSet set( GSchemaFull().GetSchema( TSchClass::k_iTable ).GetRecordInfo() );
	return set;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a Column Set for a schema class which contains every field
//-----------------------------------------------------------------------------
template< typename TSchClass >
CColumnSet CColumnSet::Full()
{
	CColumnSet set( GSchemaFull().GetSchema( TSchClass::k_iTable ).GetRecordInfo() );
	set.MakeFull();
	return set;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a Column Set for a schema class which contains every 
//			insertable field
//-----------------------------------------------------------------------------
template< typename TSchClass >
CColumnSet CColumnSet::Insertable()
{
	CColumnSet set( GSchemaFull().GetSchema( TSchClass::k_iTable ).GetRecordInfo() );
	set.MakeInsertable();
	return set;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a Column Set for a schema class which contains every 
//			noninsertable field
//-----------------------------------------------------------------------------
template< typename TSchClass >
CColumnSet CColumnSet::Noninsertable()
{
	CColumnSet set( GSchemaFull().GetSchema( TSchClass::k_iTable ).GetRecordInfo() );
	set.MakeNoninsertable();
	return set;
}


//-----------------------------------------------------------------------------
// Purpose: Returns a Column Set for a schema class which contains every 
//			primary key field
//-----------------------------------------------------------------------------
template< typename TSchClass >
CColumnSet CColumnSet::PrimaryKey()
{
	CColumnSet set( GSchemaFull().GetSchema( TSchClass::k_iTable ).GetRecordInfo() );
	set.MakePrimaryKey();
	return set;
}

} // namespace GCSDK
#endif // COLUMNSET_H
