//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Header: $
// $NoKeywords: $
//
// Material editor
//=============================================================================

#ifndef MKSUTIL_H
#define MKSUTIL_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utllinkedlist.h"
#include "tier1/utlstring.h"

//-----------------------------------------------------------------------------
// The application object
//-----------------------------------------------------------------------------
class CMksUtil 
{
public:

	enum EntryType_t 
	{
		ENTRYTYPE_SEQUENCE,
		ENTRYTYPE_FRAME,
		ENTRYTYPE_MAX
	};

	struct sMKSInfo
	{
		int entryType;
		int sequenceNumber;
		const char *pFrameName; // array?
		float displayTime;
	};

	void Init( const char *objectName );
	
	CUtlString GetName();
	void CreateNewSequenceEntry();
	void CreateNewFrameEntry( const char* pFrameName, float displayTime = 1.0f );

	void WriteFile();

private:

	CUtlLinkedList< sMKSInfo > m_MksEntries;
	int m_SequenceCount;
	CUtlString m_Name;
};

#endif // MKSUTIL_H

