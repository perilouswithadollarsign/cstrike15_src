//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DMEBOOKMARK_H
#define DMEBOOKMARK_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"

class CDmeBookmark : public CDmElement
{
	DEFINE_ELEMENT( CDmeBookmark, CDmElement );

public:
	const char *GetNote() const { return m_Note; }
	DmeTime_t GetTime() const { return m_Time; }
	DmeTime_t GetDuration() const { return m_Duration; }

	void SetNote( const char *pNote ) { m_Note = pNote; }
	void SetTime( DmeTime_t time ) { m_Time = time; }
	void SetDuration( DmeTime_t duration ) { m_Duration = duration; }

private:
	CDmaString m_Note;
	CDmaTime m_Time;
	CDmaTime m_Duration;
};

class CDmeBookmarkSet : public CDmElement
{
	DEFINE_ELEMENT( CDmeBookmarkSet, CDmElement );

public:
	const CDmaElementArray< CDmeBookmark > &GetBookmarks() const;
	CDmaElementArray< CDmeBookmark > &GetBookmarks();

	void ScaleBookmarkTimes( float scale );

private:
	CDmaElementArray< CDmeBookmark >	m_Bookmarks; // "bookmarks"
};

#endif // DMEBOOKMARK_H
