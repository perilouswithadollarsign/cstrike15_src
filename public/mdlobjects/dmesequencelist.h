//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =====//
//
// A list of DmeSequences's
//
//===========================================================================//


#ifndef DMESEQUENCELIST_H
#define DMESEQUENCELIST_H


#ifdef _WIN32
#pragma once
#endif


#include "datamodel/dmattributevar.h"
#include "mdlobjects/dmemdllist.h"
#include "tier1/utlvector.h"


//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------
class CDmeSequenceBase;
class CDmeIkChain;


//-----------------------------------------------------------------------------
// A class representing a list of sequences
//-----------------------------------------------------------------------------
class CDmeSequenceList : public CDmeMdlList
{
	DEFINE_ELEMENT( CDmeSequenceList, CDmeMdlList );

public:
	virtual CDmAttribute *GetListAttr() { return m_Sequences.GetAttribute(); }

	CDmaElementArray< CDmeSequenceBase> m_Sequences;
	CDmaElementArray< CDmeIkChain > m_eIkChainList;

	// Returns a sorted of the sequences in the m_Sequences attribute in order of priority
	// Sequences that are referred to come before the sequences that refer to them
	void GetSortedSequenceList( CUtlVector< CDmeSequenceBase * > &sortedSequenceList ) const;

	// Sorts the sequences in the m_Sequences attribute in order of priority
	// Sequences that are referred to come before the sequences that refer to them
	void SortSequences();
};


#endif // DMESEQUENCELIST_H
