//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: PS3 support for true type fonts.
//
//=====================================================================================//

#if 0  // clipped for now

#include "materialsystem/imaterialsystem.h"
#include "vgui_surfacelib/Win32Font.h"
#include "vgui_surfacelib/FontManager.h"
#include "../materialsystem/ifont.h"
#include "FontEffects.h"
#include <vgui/ISurface.h>

CWin32Font::CWin32Font() : m_ExtendedABCWidthsCache(256, 0, &ExtendedABCWidthsCacheLessFunc)
{
    m_pFont = NULL;
}

CWin32Font::~CWin32Font()
{
	//FontManager().MaterialSystem()->CloseTrueTypeFont(m_pFont);

    m_pFont = NULL;
}

// Create the font
bool CWin32Font::Create(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	// setup font properties
	m_iTall = tall;
	m_iWeight = weight;
	m_iFlags = flags;
	m_bAntiAliased = (flags & FONTFLAG_ANTIALIAS) ? 1 : 0;
	m_iDropShadowOffset = (flags & FONTFLAG_DROPSHADOW) ? 1 : 0;
	m_iOutlineSize = (flags & FONTFLAG_OUTLINE) ? 1 : 0;
	m_iBlur = blur;
	m_iScanLines = scanlines;
	m_bRotary = (flags & FONTFLAG_ROTARY) ? 1 : 0;
	m_bAdditive = (flags & FONTFLAG_ADDITIVE) ? 1 : 0;
	m_szName = windowsFontName;

	// if the weight is greater that 400, set the style to bold (cf win32font_x360)
	// By default use the regular style
	m_iWeight = 0x10000	;	// ONE16Dot16	1.0f	(F16Dot16 format used by font fusion)
	if ( weight > 400 )
	{
		m_iWeight = 5L << 14;		// 1.25		(F16Dot16 format used by font fusion)
	}
	
	// Open the font
	ExecuteNTimes( 5, Warning( "Fonts dont work on PS3\n" ) );
    //m_pFont = FontManager().MaterialSystem()->OpenTrueTypeFont(windowsFontName, tall, m_iWeight);
    if(m_pFont == NULL)
    {
		Warning("Failed to open font %s\n", windowsFontName);
        return false;
    }

	// Store the font parameters
	m_iHeight = m_pFont->GetMaxHeight();
	m_iAscent = m_pFont->GetAscent();
	m_iMaxCharWidth = m_pFont->GetMaxWidth();

    // Setup ABC cache
	// get char spacing
	// a is space before character (can be negative)
	// b is the width of the character
	// c is the space after the character
    memset(m_ABCWidthsCache, 0, sizeof(m_ABCWidthsCache));
    for(int i = 0; i < ABCWIDTHS_CACHE_SIZE; i++)
    {
        int a,b,c;
        a = 0;
        b = 0;
        c = 0;
        m_pFont->GetCharABCWidth(i, a, b, c);

		m_ABCWidthsCache[i].a = a - m_iBlur;
		m_ABCWidthsCache[i].b = b + m_iBlur*2;
		m_ABCWidthsCache[i].c = c - m_iBlur;
    }

	// many fonts are blindly precached by vgui and never used
	// save memory and don't hold font open, re-open if glyph actually requested used during draw
	Assert( 0 );
	//FontManager().MaterialSystem()->CloseTrueTypeFont( m_pFont );
	m_pFont = NULL;

	return true;
}

// Render the font to a buffer
void CWin32Font::GetCharRGBA(wchar_t ch, int rgbaWide, int rgbaTall, unsigned char *rgba)
{
    if ( ch == '\t' )
	{
		// tabs don't draw
		return;
	}

	if ( !m_pFont )
	{
		// demand request for font glyph, re-create font
		Assert( 0 );
		//m_pFont = FontManager().MaterialSystem()->OpenTrueTypeFont(GetName(), m_iTall, m_iWeight);
	}

	int a, c, wide, tall;
	GetCharABCWidths( ch, a, wide, c );
	tall = m_iHeight;

	m_pFont->RenderToBuffer(ch, m_iBlur, rgbaWide, rgbaTall, rgba);

	// apply requested effects in specified order
	//ApplyDropShadowToTexture( rgbaX, rgbaY, rgbaWide, rgbaTall, wide, tall, rgba, m_iDropShadowOffset );
	//ApplyOutlineToTexture( rgbaX, rgbaY, rgbaWide, rgbaTall, wide, tall, rgba, m_iOutlineSize );
	ApplyGaussianBlurToTexture( rgbaWide, rgbaTall, rgba, m_iBlur );
	ApplyScanlineEffectToTexture( rgbaWide, rgbaTall, rgba, m_iScanLines );
	//ApplyRotaryEffectToTexture( rgbaX, rgbaY, rgbaWide, rgbaTall, rgba, m_bRotary );
}

bool CWin32Font::IsEqualTo(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	if ( !stricmp(windowsFontName, m_szName.String() ) 
		&& m_iTall == tall
		&& m_iWeight == weight
		&& m_iBlur == blur
		&& m_iScanLines == scanlines)
	{
        return true;
	}

    return false;
}

bool CWin32Font::IsValid()
{
    return true;
}

// Font metrics
void CWin32Font::GetCharABCWidths(int ch, int &a, int &b, int &c)
{
    Assert( IsValid() );
    if (ch < ABCWIDTHS_CACHE_SIZE)
    {
        // use the cache entry
        a = m_ABCWidthsCache[ch].a;
        b = m_ABCWidthsCache[ch].b;
        c = m_ABCWidthsCache[ch].c;
    }
    else
    {
        // look for it in the cache
        abc_cache_t finder = { (wchar_t)ch };

        unsigned short i = m_ExtendedABCWidthsCache.Find(finder);
        if (m_ExtendedABCWidthsCache.IsValidIndex(i))
        {
            a = m_ExtendedABCWidthsCache[i].abc.a;
            b = m_ExtendedABCWidthsCache[i].abc.b;
            c = m_ExtendedABCWidthsCache[i].abc.c;

            return;
        }

		if(!m_pFont)
		{
			//BRAD: In some instances (e.g. when you try and create an EAOnline account) it tries
			//		to call GetCharABCWidths before GetCharRGBA so m_pFont is not opened.
			// demand request for font glyph, re-create font
			Assert( 0 );
			//m_pFont = FontManager().MaterialSystem()->OpenTrueTypeFont(GetName(), m_iTall, m_iWeight);

		}
        m_pFont->GetCharABCWidth(ch, a, b, c);

        // add to the cache
        finder.abc.a = a;
        finder.abc.b = b;
        finder.abc.c = c;
        m_ExtendedABCWidthsCache.Insert(finder);
    }
}

int CWin32Font::GetHeight()
{
    return m_iHeight;
}

int CWin32Font::GetAscent()
{
    return m_iAscent;
}

int CWin32Font::GetMaxCharWidth()
{
    return m_iMaxCharWidth;
}

int CWin32Font::GetFlags()
{
    return m_iFlags;
}

bool CWin32Font::ExtendedABCWidthsCacheLessFunc(const abc_cache_t &lhs, const abc_cache_t &rhs)
{
    return lhs.wch < rhs.wch;
}

#endif