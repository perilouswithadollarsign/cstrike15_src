//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: PS3 support for TrueType Fonts as hastily bastardized from Xbox 360 code. 
// On 360, the only solution is to use XUI
// to mount the TTF, and rasterize glyph into a render target. XUI does not support
// rasterization directly to a system memory region.
// On PS3 that is not a problem, but rather than reimplement this whole class, 
// the minimal-code-change approach is just to patch some functions in shaderapi, 
// even though we don't need to do the work in shaderapi. I hang my head in shame
// for this unworthy laziness/haste.
//
//=====================================================================================//

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <tier0/dbg.h>
#include <vgui/ISurface.h>
#include <tier0/mem.h>
#include <utlbuffer.h>
#include "filesystem.h"
#include "materialsystem/imaterialsystem.h"
#include "FontEffects.h"
#include "vgui_surfacelib/vguifont.h"
#include "vgui_surfacelib/FontManager.h"




// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool s_bSupportsUnicode = true;

//-----------------------------------------------------------------------------
// Determine possible style from parameters.
//-----------------------------------------------------------------------------
int GetStyleFromParameters( int iFlags, int iWeight )
{
	// Available xbox TTF styles are very restricted.
	int style = CPS3Font::kFONT_STYLE_NORMAL;
	if ( iFlags & FONTFLAG_ITALIC )
		style |= CPS3Font::kFONT_STYLE_ITALIC;
	if ( iFlags & FONTFLAG_UNDERLINE )
		style |= CPS3Font::kFONT_STYLE_UNDERLINE;
	if ( iWeight > 400 )
		style |= CPS3Font::kFONT_STYLE_BOLD;
	return style;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CPS3Font::CPS3Font() : m_ExtendedABCWidthsCache( 256, 0, &ExtendedABCWidthsCacheLessFunc )
{
	m_szName = UTL_INVAL_SYMBOL;
	m_iTall = 0;
	m_iWeight = 0;
	m_iHeight = 0;
	m_iAscent = 0;
	m_iFlags = 0;
	m_iMaxCharWidth = 0;
	m_hFont = NULL;
	m_bAntiAliased = false;
	m_bUnderlined = false;
	m_iBlur = 0;
	m_iScanLines = 0;
	m_bRotary = false;
	m_bAdditive = false;
	m_rgiBitmapSize[0] = 0;
	m_rgiBitmapSize[1] = 0;

	s_bSupportsUnicode = true;

	Q_memset( m_ABCWidthsCache, 0, sizeof( m_ABCWidthsCache ) );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CPS3Font::~CPS3Font()
{
	if ( m_hFont != NULL )
	{
		// many fonts are blindly precached by vgui and never used
		// save memory and don't hold font open, re-open if glyph actually requested used during draw
		if ( IMaterialSystem *pMS = FontManager().MaterialSystem() )
			pMS->CloseTrueTypeFont( m_hFont );
	} 
	m_hFont = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Creates the font.
//-----------------------------------------------------------------------------
bool CPS3Font::Create( const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags )
{
	// setup font properties
	m_iTall = tall;
	m_iWeight = weight;
	m_iFlags = flags;
	m_bAntiAliased = (flags & FONTFLAG_ANTIALIAS) ? 1 : 0;
	m_bUnderlined = (flags & FONTFLAG_UNDERLINE) ? 1 : 0;
	m_iDropShadowOffset = (flags & FONTFLAG_DROPSHADOW) ? 1 : 0;
	m_iOutlineSize = (flags & FONTFLAG_OUTLINE) ? 1 : 0;
	m_iBlur = blur;
	m_iScanLines = scanlines;
	m_bRotary = (flags & FONTFLAG_ROTARY) ? 1 : 0;
	m_bAdditive = (flags & FONTFLAG_ADDITIVE) ? 1 : 0;

	int style = GetStyleFromParameters( flags, weight );

	// must support > 128, there are characters in this range in the custom fonts
	COMPILE_TIME_ASSERT( ABCWIDTHS_CACHE_SIZE == 256 );

	CPS3FontMetrics fontMetrics;
	CPS3CharMetrics charMetrics[ABCWIDTHS_CACHE_SIZE];

	// many redundant requests are made that are actually the same font metrics
	// find it in the metric cache first based on the true specific keys
	if ( !FontManager().GetCachedPS3Metrics( windowsFontName, tall, style, &fontMetrics, charMetrics ) )
	{
		if ( !m_hFont )
		{
			m_hFont = FontManager().MaterialSystem()->OpenTrueTypeFont( windowsFontName, tall, style );
			// no you're not seeing double
			if ( !m_hFont )
			{
				return false;
			}
		} 

		// get the predominant font metrics now [1-255], the extended set [256-65535] is on-demand
		FontManager().MaterialSystem()->GetTrueTypeFontMetrics( m_hFont, m_iTall, 1, 255, &fontMetrics, &charMetrics[1] );

		// getting the metrics is an expensive i/o operation, cache results
		FontManager().SetCachedPS3Metrics( windowsFontName, tall, style, &fontMetrics, charMetrics );
	}

	m_szName = windowsFontName;

	m_iHeight = fontMetrics.lineHeight + fontMetrics.effectHeight + m_iDropShadowOffset + 2 * m_iOutlineSize;
	m_iMaxCharWidth = ceilf(fontMetrics.fMaxWidth);
	m_iAscent = fontMetrics.baseLineY;

	// determine cell bounds
	m_rgiBitmapSize[0] = m_iMaxCharWidth + m_iOutlineSize * 2;
	m_rgiBitmapSize[1] = m_iHeight;

	// get char spacing
	// a is space before character (can be negative)
	// b is the width of the character
	// c is the space after the character
	Assert( ABCWIDTHS_CACHE_SIZE <= 256 );
	Q_memset( m_ABCWidthsCache, 0, sizeof( m_ABCWidthsCache ) );

	for ( int i = 1; i < ABCWIDTHS_CACHE_SIZE; i++ )
	{
		int a,b,c;

		/*
		// Determine real a,b,c mapping from XUI Character Metrics
		a = charMetrics[i].fMinX - 1; // Add one column of padding to make up for font rendering blurring into left column (and adjust in b)
		b = charMetrics[i].fMaxX - charMetrics[i].fMinX + 1;
		c = charMetrics[i].fAdvance - charMetrics[i].fMaxX; // NOTE: We probably should add a column here, but it's rarely needed in our current fonts so we're opting to save memory instead
		*/
		a = charMetrics[i].A();
		b = charMetrics[i].B();
		c = charMetrics[i].C();

		// Widen for blur, outline, and shadow. Need to widen b and reduce a and c.
		m_ABCWidthsCache[i].a = a - m_iOutlineSize;
		m_ABCWidthsCache[i].b = b + ( ( m_iBlur + m_iOutlineSize ) * 2 ) + m_iDropShadowOffset;
		m_ABCWidthsCache[i].c = c - 2*m_iBlur - m_iDropShadowOffset - m_iOutlineSize;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: generates texture data (written into appropriate font page subrects) for multiple chars
//-----------------------------------------------------------------------------
void CPS3Font::GetCharsRGBA( newChar_t *newChars, int numNewChars, unsigned char *pRGBA )
{
	if ( !m_hFont )
	{
		// demand request for font glyph, re-create font
		int style = GetStyleFromParameters( m_iFlags, m_iWeight );
		m_hFont = FontManager().MaterialSystem()->OpenTrueTypeFont( GetName(), m_iTall, style );
	}

	wchar_t	*pWch		= (wchar_t *)_alloca( numNewChars*sizeof(wchar_t) );
	int	*pOffsetX		= (int *)_alloca( numNewChars*sizeof(int) );
	int	*pOffsetY		= (int *)_alloca( numNewChars*sizeof(int) );
	int	*pWidth			= (int *)_alloca( numNewChars*sizeof(int) );
	int	*pHeight		= (int *)_alloca( numNewChars*sizeof(int) );
	int	*pRGBAOffset	= (int *)_alloca( numNewChars*sizeof(int) );
	for ( int i = 0; i < numNewChars; i++ )
	{
		int a, c, wide;
		GetCharABCWidths( newChars[i].wch, a, wide, c );
		pWch[i]			= newChars[i].wch;
		pOffsetX[i]		= -a;
		pOffsetY[i]		= 0;
		pWidth[i]		= newChars[i].fontWide;
		pHeight[i]		= newChars[i].fontTall;
		pRGBAOffset[i]	= newChars[i].offset;
	}
	if ( !FontManager().MaterialSystem()->GetTrueTypeGlyphs( m_hFont, m_iTall, numNewChars, pWch, pOffsetX, pOffsetY, pWidth, pHeight, pRGBA, pRGBAOffset ) )
	{
		// failure
		return;
	}

	for ( int i = 0; i < numNewChars; i++ )
	{
		// apply requested effects in specified order
		unsigned char *pCharRGBA = pRGBA + newChars[i].offset;
		ApplyDropShadowToTexture( newChars[i].fontWide, newChars[i].fontTall, pCharRGBA, m_iDropShadowOffset );
		ApplyOutlineToTexture( newChars[i].fontWide, newChars[i].fontTall, pCharRGBA, m_iOutlineSize );
		ApplyGaussianBlurToTexture( newChars[i].fontWide, newChars[i].fontTall, pCharRGBA, m_iBlur );
		ApplyScanlineEffectToTexture( newChars[i].fontWide, newChars[i].fontTall, pCharRGBA, m_iScanLines );
		ApplyRotaryEffectToTexture( newChars[i].fontWide, newChars[i].fontTall, pCharRGBA, m_bRotary );
	}
}

//-----------------------------------------------------------------------------
// Purpose: writes the char into the specified 32bpp texture at specified rect
//-----------------------------------------------------------------------------
void CPS3Font::GetCharRGBA( wchar_t ch, int rgbaWide, int rgbaTall, unsigned char *pRGBA )
{
	newChar_t newChar;
	newChar.wch = ch;
	newChar.fontWide = rgbaWide;
	newChar.fontTall = rgbaTall;
	newChar.offset = 0;
	GetCharsRGBA( &newChar, 1, pRGBA );
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the font is equivalent to that specified
//-----------------------------------------------------------------------------
bool CPS3Font::IsEqualTo(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	// do an true comparison that accounts for non-supported behaviors that gets remapped
	// avoids creating fonts that are graphically equivalent, though specified differently
	if ( !stricmp( windowsFontName, m_szName.String() ) &&
		m_iTall == tall && 
		m_iBlur == blur &&
		m_iScanLines == scanlines )
	{
		// only these flags affect the font glyphs
		int validFlags = FONTFLAG_DROPSHADOW | 
			FONTFLAG_OUTLINE | 
			FONTFLAG_ROTARY |
			FONTFLAG_ITALIC |
			FONTFLAG_UNDERLINE;
		if ( ( m_iFlags & validFlags ) == ( flags & validFlags ) )
		{
			if ( GetStyleFromParameters( m_iFlags, m_iWeight ) == GetStyleFromParameters( flags, weight ) )
			{
				// the font is equivalent
				return true;
			}
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns true only if this font is valid for use
//-----------------------------------------------------------------------------
bool CPS3Font::IsValid()
{
	if ( m_szName != UTL_INVAL_SYMBOL )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: gets the abc widths for a character
//-----------------------------------------------------------------------------
void CPS3Font::GetCharABCWidths( int ch, int &a, int &b, int &c )
{
	Assert( IsValid() );

	if ( ch < ABCWIDTHS_CACHE_SIZE )
	{
		// use the cache entry
		a = m_ABCWidthsCache[ch].a;
		b = m_ABCWidthsCache[ch].b;
		c = m_ABCWidthsCache[ch].c;
	}
	else
	{
		// look for it in the extended cache
		abc_cache_t finder = { (wchar_t)ch };
		unsigned short i = m_ExtendedABCWidthsCache.Find( finder );
		if ( m_ExtendedABCWidthsCache.IsValidIndex( i ) )
		{
			a = m_ExtendedABCWidthsCache[i].abc.a;
			b = m_ExtendedABCWidthsCache[i].abc.b;
			c = m_ExtendedABCWidthsCache[i].abc.c;
			return;
		}

		// not in the cache, get from system
		// getting the metrics is an expensive i/o operation
		if ( !m_hFont )
		{
			// demand request for font metrics, re-open font
			int style = GetStyleFromParameters( m_iFlags, m_iWeight );
			m_hFont = FontManager().MaterialSystem()->OpenTrueTypeFont( GetName(), m_iTall, style );
		}

		if ( m_hFont )
		{
			CPS3FontMetrics fontMetrics;
			CPS3CharMetrics charMetrics;
			FontManager().MaterialSystem()->GetTrueTypeFontMetrics( m_hFont, m_iTall, ch, ch, &fontMetrics, &charMetrics );

			/*
			// Determine real a,b,c mapping from XUI Character Metrics
			a = charMetrics.fMinX - 1; // Add one column of padding to make up for font rendering blurring into left column (and adjust in b)
			b = charMetrics.fMaxX - charMetrics.fMinX + 1;
			c = charMetrics.fAdvance - charMetrics.fMaxX; // NOTE: We probably should add a column here, but it's rarely needed in our current fonts so we're opting to save memory instead
			*/

			a = charMetrics.A();
			b = charMetrics.B();
			c = charMetrics.C();

			// Widen for blur, outline, and shadow. Need to widen b and reduce a and c.
			a = a - m_iOutlineSize;
			b = b + ( ( m_iBlur + m_iOutlineSize ) * 2 ) + m_iDropShadowOffset;
			c = c - 2*m_iBlur - m_iDropShadowOffset - m_iOutlineSize;
		}
		else
		{
			a = 0;
			b = 0;
			c = 0;
		}

		// add to the cache
		finder.abc.a = a;
		finder.abc.b = b;
		finder.abc.c = c;
		m_ExtendedABCWidthsCache.Insert( finder );
	}
}

void CPS3Font::GetKernedCharWidth( wchar_t ch, wchar_t chBefore, wchar_t chAfter, float &wide, float &abcA, float &abcC )
{
	int a, b, c;
	GetCharABCWidths( ch, a, b, c );
	wide = a+b+c;
	abcA = a;
	abcC = c;
}

//-----------------------------------------------------------------------------
// Purpose: returns the height of the font, in pixels
//-----------------------------------------------------------------------------
int CPS3Font::GetHeight()
{
	Assert( IsValid() );
	return m_iHeight;
}

//-----------------------------------------------------------------------------
// Purpose: returns the ascent of the font, in pixels (ascent=units above the base line)
//-----------------------------------------------------------------------------
int CPS3Font::GetAscent()
{
	Assert( IsValid() );
	return m_iAscent;
}

//-----------------------------------------------------------------------------
// Purpose: returns the maximum width of a character, in pixels
//-----------------------------------------------------------------------------
int CPS3Font::GetMaxCharWidth()
{
	Assert( IsValid() );
	return m_iMaxCharWidth;
}

//-----------------------------------------------------------------------------
// Purpose: returns the flags used to make this font, used by the dynamic resizing code
//-----------------------------------------------------------------------------
int CPS3Font::GetFlags()
{
	Assert( IsValid() );
	return m_iFlags;
}


//-----------------------------------------------------------------------------
// Purpose: Comparison function for abc widths storage
//-----------------------------------------------------------------------------
bool CPS3Font::ExtendedABCWidthsCacheLessFunc( const abc_cache_t &lhs, const abc_cache_t &rhs )
{
	return lhs.wch < rhs.wch;
}

void CPS3Font::CloseResource()
{
	if ( !m_hFont )
	{
		return;
	}

	// many fonts are blindly precached by vgui and never used
	// save memory and don't hold font open, re-open if glyph actually requested used during draw
	FontManager().MaterialSystem()->CloseTrueTypeFont( m_hFont );
	m_hFont = NULL;
}