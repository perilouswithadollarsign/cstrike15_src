//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "vgui_surfacelib/FontAmalgam.h"
#include "vgui_surfacelib/ifontsurface.h"
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFontAmalgam::CFontAmalgam()
{
	m_Fonts.EnsureCapacity( 4 );
	m_iMaxHeight = 0;
	m_iMaxWidth = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFontAmalgam::~CFontAmalgam()
{
}

//-----------------------------------------------------------------------------
// Purpose: adds a font to the amalgam
//-----------------------------------------------------------------------------
void CFontAmalgam::AddFont(font_t *pFont, int lowRange, int highRange)
{
	int i = m_Fonts.AddToTail();

	m_Fonts[i].pWin32Font = pFont;
	m_Fonts[i].lowRange = lowRange;
	m_Fonts[i].highRange = highRange;

	m_iMaxHeight = MAX(pFont->GetHeight(), m_iMaxHeight);
	m_iMaxWidth = MAX(pFont->GetMaxCharWidth(), m_iMaxWidth);
}

//-----------------------------------------------------------------------------
// Purpose: clears the fonts
//-----------------------------------------------------------------------------
void CFontAmalgam::RemoveAll()
{
	// clear out
	m_Fonts.RemoveAll();
	m_iMaxHeight = 0;
	m_iMaxWidth = 0;
}

//-----------------------------------------------------------------------------
// Purpose: returns the font for the given character
//-----------------------------------------------------------------------------
font_t *CFontAmalgam::GetFontForChar(int ch)
{
	for (int i = 0; i < m_Fonts.Count(); i++)
	{
		if ( ch >= m_Fonts[i].lowRange && ch <= m_Fonts[i].highRange )
		{
			Assert( m_Fonts[i].pWin32Font->IsValid() );
			return m_Fonts[i].pWin32Font;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: sets the scale of the font
//-----------------------------------------------------------------------------
void CFontAmalgam::SetFontScale(float sx, float sy)
{
	if (!m_Fonts.Count())
		return;

	// Make sure this is a bitmap font!
	if ( GetFlags( 0 ) & FONTFLAG_BITMAP )
	{
		reinterpret_cast< CBitmapFont* >( m_Fonts[0].pWin32Font )->SetScale( sx, sy );
	}
	else
	{
		Warning( "%s: Can't set font scale on a non-bitmap font!\n", m_Fonts[0].pWin32Font->GetName() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the max height of the font set
//-----------------------------------------------------------------------------
int CFontAmalgam::GetFontHeight()
{
	if (!m_Fonts.Count())
	{
		return m_iMaxHeight;
	}
	return m_Fonts[0].pWin32Font->GetHeight();
}

//-----------------------------------------------------------------------------
// Purpose: returns the maximum width of a character in a font
//-----------------------------------------------------------------------------
int CFontAmalgam::GetFontMaxWidth()
{
	return m_iMaxWidth;
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of the font that is loaded
//-----------------------------------------------------------------------------
const char *CFontAmalgam::GetFontName( int i )
{	
	if ( m_Fonts.Count() && m_Fonts[i].pWin32Font && m_Fonts[i].pWin32Font->IsValid() )
	{
		return m_Fonts[i].pWin32Font->GetName();
	}
	
	return "";
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of the font that is loaded
//-----------------------------------------------------------------------------
int CFontAmalgam::GetFlags(int i)
{	
	if ( m_Fonts.Count() && m_Fonts[i].pWin32Font )
	{
		return m_Fonts[i].pWin32Font->GetFlags();
	}
	else
	{
		return 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the number of fonts this amalgam contains
//-----------------------------------------------------------------------------
int CFontAmalgam::GetCount()
{		
	return m_Fonts.Count();
}


//-----------------------------------------------------------------------------
// Purpose: returns the max height of the font set
//-----------------------------------------------------------------------------
bool CFontAmalgam::GetUnderlined()
{
	if (!m_Fonts.Count())
	{
		return false;
	}
	return m_Fonts[0].pWin32Font->GetUnderlined();
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void CFontAmalgam::Validate( CValidator &validator, char *pchName )
{
	validator.Push( "CFontAmalgam", this, pchName );

	ValidateObj( m_Fonts );

	validator.Pop();
}
#endif // DBGFLAG_VALIDATE
