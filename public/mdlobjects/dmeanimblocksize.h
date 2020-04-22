//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ====
//
// Dme $animblocksize
//
//===========================================================================


#ifndef DMEANIMBLOCKSIZE_H
#define DMEANIMBLOCKSIZE_H


#ifdef _WIN32
#pragma once
#endif


#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"


//-----------------------------------------------------------------------------
// A class representing an attachment point
//-----------------------------------------------------------------------------
class CDmeAnimBlockSize : public CDmElement
{
	DEFINE_ELEMENT( CDmeAnimBlockSize, CDmElement );

public:
	enum AnimBlockStorageType_t
	{
		ANIMBLOCKSTORAGETYPE_LOWRES = 0,
		ANIMBLOCKSTORAGETYPE_HIRES = 1
	};

	CDmaVar< int >	m_nSize;	
	CDmaVar< bool >	m_bStall;	
	CDmaVar< int >	m_nStorageType;	
};


#endif // DMEANIMBLOCKSIZE_H