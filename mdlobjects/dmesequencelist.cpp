//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =====//
//
// A list of DmeSequences's
//
//===========================================================================//


#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmesequence.h"
#include "mdlobjects/dmesequencelist.h"
#include "mdlobjects/dmeik.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSequenceList, CDmeSequenceList );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceList::OnConstruction()
{
	m_Sequences.Init( this, "sequences" );
	m_eIkChainList.Init( this, "ikChainList" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSequenceList::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Returns a sorted of the sequences in the m_Sequences attribute in order of priority
// Sequences that are referred to come before the sequences that refer to them
//-----------------------------------------------------------------------------
void CDmeSequenceList::GetSortedSequenceList( CUtlVector< CDmeSequenceBase * > &sortedSequenceList ) const
{
	sortedSequenceList.RemoveAll();

	const int nSequenceCount = m_Sequences.Count();

	sortedSequenceList.EnsureCapacity( nSequenceCount );

	for ( int i = 0; i < nSequenceCount; ++i )
	{
		CDmeSequenceBase *pDmeSequenceBase = m_Sequences[i];
		if ( !pDmeSequenceBase )
			continue;

		sortedSequenceList.AddToTail( pDmeSequenceBase );
	}

	qsort( sortedSequenceList.Base(), sortedSequenceList.Count(), sizeof( CDmeSequenceBase * ), CDmeSequenceBase::QSortFunction );
}


//-----------------------------------------------------------------------------
// Sorts the sequences in the m_Sequences attribute in order of priority
// Sequences that are referred to come before the sequences that refer to them
//-----------------------------------------------------------------------------
void CDmeSequenceList::SortSequences()
{
	CUtlVector< CDmeSequenceBase * > sortedSequenceList;

	GetSortedSequenceList( sortedSequenceList );

	m_Sequences.RemoveAll();
	m_Sequences.EnsureCount( sortedSequenceList.Count() );
	for ( int i = 0; i < m_Sequences.Count(); ++i )
	{
		m_Sequences.Set( i, sortedSequenceList[i] );
	}
}