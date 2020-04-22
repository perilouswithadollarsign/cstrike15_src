//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmebookmark.h"
#include "tier0/dbg.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Class factory 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeBookmark, CDmeBookmark );


//-----------------------------------------------------------------------------
// Constructor, destructor 
//-----------------------------------------------------------------------------
void CDmeBookmark::OnConstruction()
{
	m_Time.InitAndSet( this, "time", DMETIME_ZERO );
	m_Duration.InitAndSet( this, "duration", DMETIME_ZERO );
	m_Note.Init( this, "note" );
}

void CDmeBookmark::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Class factory 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeBookmarkSet, CDmeBookmarkSet );


//-----------------------------------------------------------------------------
// Constructor, destructor 
//-----------------------------------------------------------------------------
void CDmeBookmarkSet::OnConstruction()
{
	m_Bookmarks.Init( this, "bookmarks" );
}

void CDmeBookmarkSet::OnDestruction()
{
}

const CDmaElementArray< CDmeBookmark > &CDmeBookmarkSet::GetBookmarks() const
{
	return m_Bookmarks;
}

CDmaElementArray< CDmeBookmark > &CDmeBookmarkSet::GetBookmarks()
{
	return m_Bookmarks;
}

void CDmeBookmarkSet::ScaleBookmarkTimes( float scale )
{
	int nBookmarks = m_Bookmarks.Count();
	for ( int i = 0; i < nBookmarks; ++i )
	{
		CDmeBookmark *pBookmark = m_Bookmarks[ i ];
		if ( !pBookmark )
			continue;

		pBookmark->SetTime    ( pBookmark->GetTime    () * scale );
		pBookmark->SetDuration( pBookmark->GetDuration() * scale );
	}
}
