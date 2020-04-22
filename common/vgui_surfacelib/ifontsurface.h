//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef IFONTSURFACE_H
#define IFONTSURFACE_H

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/vector2d.h"  // must be before the namespace line

#ifdef CreateFont
#undef CreateFont
#endif


// returns true if the surface supports minimize & maximize capabilities
// Numbered this way to prevent interface change in surface.
enum FontFeature_t
{
	FONT_FEATURE_ANTIALIASED_FONTS	= 1,
	FONT_FEATURE_DROPSHADOW_FONTS	= 2,
	FONT_FEATURE_OUTLINE_FONTS	= 6,
};

// adds to the font
enum FontFlags_t
{
	FONTFLAG_NONE,
	FONTFLAG_ITALIC			= 0x001,
	FONTFLAG_UNDERLINE		= 0x002,
	FONTFLAG_STRIKEOUT		= 0x004,
	FONTFLAG_SYMBOL			= 0x008,
	FONTFLAG_ANTIALIAS		= 0x010,
	FONTFLAG_GAUSSIANBLUR	= 0x020,
	FONTFLAG_ROTARY			= 0x040,
	FONTFLAG_DROPSHADOW		= 0x080,
	FONTFLAG_ADDITIVE		= 0x100,
	FONTFLAG_OUTLINE		= 0x200,
	FONTFLAG_CUSTOM			= 0x400,		// custom generated font - never fall back to asian compatibility mode
	FONTFLAG_BITMAP			= 0x800,		// compiled bitmap font - no fallbacks
};

enum FontDrawType_t
{
	// Use the "additive" value from the scheme file
	FONT_DRAW_DEFAULT = 0,

	// Overrides
	FONT_DRAW_NONADDITIVE,
	FONT_DRAW_ADDITIVE,

	FONT_DRAW_TYPE_COUNT = 2,
};	


struct FontVertex_t
{
	FontVertex_t() {}
	FontVertex_t( const Vector2D &pos, const Vector2D &coord = Vector2D( 0, 0 ) )
	{
		m_Position = pos;
		m_TexCoord = coord;
	}
	void Init( const Vector2D &pos, const Vector2D &coord = Vector2D( 0, 0 ) )
	{
		m_Position = pos;
		m_TexCoord = coord;
	}
	
	Vector2D m_Position;
	Vector2D m_TexCoord;
};

typedef unsigned long FontHandle_t;

struct FontCharRenderInfo
{
	// Text pos
	int				x, y;
	// Top left and bottom right
	// This is now a pointer to an array maintained by the surface, to avoid copying the data on the 360
	FontVertex_t	*verts;
	int				textureId;
	int				abcA;
	int				abcB;
	int				abcC;
	int				fontTall;
	FontHandle_t	currentFont;
	// In:
	FontDrawType_t	drawType;
	wchar_t			ch;

	// Out
	bool			valid;
	// In/Out (true by default)
	bool			shouldclip;
};	



#endif // IFONTSURFACE_H
