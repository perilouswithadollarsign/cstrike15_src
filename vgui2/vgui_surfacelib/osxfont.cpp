//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <tier0/dbg.h>
#include <vgui/ISurface.h>
#include <tier0/mem.h>
#include <utlbuffer.h>
#include <vstdlib/vstrtools.h>

#include "filesystem.h"
#include "vgui_surfacelib/osxfont.h"
#include "FontEffects.h"

#define DEBUG_FONT_CREATE 0

struct MetricsTweaks_t
{
	const char *m_windowsFontName;
	int m_sizeAdjust;
	float m_ascentMultiplier;
	float m_descentMultiplier;
	float m_leadingMultiplier;
};

static const MetricsTweaks_t g_defaultMetricTweaks = { NULL, 0, 1.0, 1.0, 1.0 };// -2, 1.0, 1.0, 1.0 };

static MetricsTweaks_t g_FontMetricTweaks[] =
{
	{ "Helvetica", 0, 1.0, 1.0, 1.05 },
	{ "Helvetica Bold", 0, 1.0, 1.0, 1.0 },
	{ "HL2cross", 0, 0.8, 1.0, 1.1},
	{ "Counter-Strike Logo", 0, 1.0, 1.0, 1.1},
	{ "TF2", -2, 1.0, 1.0, 1.0 },
	{ "TF2 Professor", -2, 1.0, 1.1, 1.1 },
	{ "TF2 Build", -2, 1.0, 1.0, 1.0 },
	{ "UniversLTStd-BoldCn", 0, 1.4, 1.0, 0.8 },	
	{ "UniversLTStd-Cn", 0, 1.2, 1.0, 1.0 },	
	//{ "TF2 Secondary", -2, 1.0, 1.0, 1.0 },
	//	{ "Verdana", 0, 1.25, 1.0, 1.0 },
};

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
COSXFont::COSXFont() : m_ExtendedABCWidthsCache(256, 0, &ExtendedABCWidthsCacheLessFunc), 
m_ExtendedKernedABCWidthsCache( 256, 0, &ExtendedKernedABCWidthsCacheLessFunc )
{
	m_iTall = 0;
  m_iAscent = 0;
  m_iDescent = 0;
	m_iWeight = 0;
	m_iFlags = 0;
	m_iMaxCharWidth = 0;
	m_bAntiAliased = false;
	m_bUnderlined = false;
	m_iBlur = 0;
	m_pGaussianDistribution = NULL;
	m_iScanLines = 0;
	m_bRotary = false;
	m_bAdditive = false;
	m_ContextRef = 0;
	m_pContextMemory = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
COSXFont::~COSXFont()
{
	if ( m_ContextRef )
	{
		CGContextRelease( m_ContextRef );
	}
	
	if ( m_pContextMemory )
		delete [] m_pContextMemory;
	
}

//-----------------------------------------------------------------------------
// Purpose: creates the font from windows.  returns false if font does not exist in the OS.
//-----------------------------------------------------------------------------
bool COSXFont::Create(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	// setup font properties
	m_szName = windowsFontName;
	m_iTall = tall;
	m_iWeight = weight;
	m_iFlags = flags;
	m_bAntiAliased = flags & FONTFLAG_ANTIALIAS;
#if 0
// the font used in portal2 looks ok (better, in fact) anti-aliased when small, 
	if ( tall < 20 )
	{
		m_bAntiAliased = false;
	}
#endif
	
	m_bUnderlined = flags & FONTFLAG_UNDERLINE;
	m_iDropShadowOffset = (flags & FONTFLAG_DROPSHADOW) ? 1 : 0;
	m_iOutlineSize = (flags & FONTFLAG_OUTLINE) ? 1 : 0;
	m_iBlur = blur;
	m_iScanLines = scanlines;
	m_bRotary = flags & FONTFLAG_ROTARY;
	m_bAdditive = flags & FONTFLAG_ADDITIVE;

	char sCustomPath[1024];
	Q_snprintf( sCustomPath, sizeof( sCustomPath ), "./platform/vgui/fonts/%s.ttf", windowsFontName );

	if ( g_pFullFileSystem->FileExists( sCustomPath ) )
	 {
		CFStringRef path = CFStringCreateWithCString( NULL, windowsFontName, kCFStringEncodingUTF8 );
		CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path, kCFURLPOSIXPathStyle, false);
		CGDataProviderRef dataProvider = CGDataProviderCreateWithURL( url );
		CGFontRef cgFont = CGFontCreateWithDataProvider( dataProvider );
		m_hFont = CTFontCreateWithGraphicsFont( cgFont, tall, nullptr, nullptr );
		CFRelease( cgFont );
		CFRelease( dataProvider );
		CFRelease( url );
		CFRelease( path );

		CTFontCopyCharacterSet(m_hFont);
	}
	else
	{

		const void *pKeys[2];
		const void *pValues[2];

		float fCTWeight = ( (float)( weight - 400 ) / 500.0f );
		pKeys[0] = kCTFontWeightTrait;
		pValues[0] = CFNumberCreate( NULL, kCFNumberFloatType, &fCTWeight );
		float fCTSlant = ( flags & FONTFLAG_ITALIC ) != 0 ? 1.0f : 0.0f;
		pKeys[1] = kCTFontSlantTrait;
		pValues[1] = CFNumberCreate( NULL, kCFNumberFloatType, &fCTSlant );

		CFDictionaryRef pTraitsDict = CFDictionaryCreate( NULL, pKeys, pValues, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		if ( !pTraitsDict )
		{
			goto Fail;
		}

		CFRelease( (CFNumberRef)pValues[0] );
		CFRelease( (CFNumberRef)pValues[1] );
    
		pKeys[0] = kCTFontNameAttribute;
		pValues[0] = CFStringCreateWithCString( NULL, windowsFontName, kCFStringEncodingUTF8 );
		pKeys[1] = kCTFontTraitsAttribute;
		pValues[1] = pTraitsDict;

		CFDictionaryRef pDescDict;

		pDescDict = CFDictionaryCreate( NULL, pKeys, pValues, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

		CFRelease( (CFStringRef)pValues[0] );
		CFRelease( pTraitsDict );
        
		if ( !pDescDict )
		{
			goto Fail;
		}
	
		CTFontDescriptorRef pFontDesc;

		pFontDesc = CTFontDescriptorCreateWithAttributes( pDescDict );

		CFRelease( pDescDict );

		if ( !pFontDesc )
		{
			goto Fail;
		}

		// Fudge the size of the font to something reasonable.
		m_hFont = CTFontCreateWithFontDescriptor( pFontDesc, int(tall*0.85), NULL );

		CFRelease( pFontDesc );
	}

	if ( !m_hFont )
	{
		goto Fail;
	}

	CGRect bbox;

	bbox = CTFontGetBoundingBox( m_hFont );

	m_iAscent = ceil( CTFontGetAscent( m_hFont ) );
	// The bounding box height seems to be overly large so use
	// ascent plus descent.
	m_iHeight = m_iAscent + ceil( CTFontGetDescent( m_hFont ) ) + m_iDropShadowOffset + 2 * m_iOutlineSize;
	m_iMaxCharWidth = ceil( bbox.size.width ) + 2 * m_iOutlineSize;
	
	uint bytesPerRow;

	bytesPerRow = m_iMaxCharWidth * 4;
	m_pContextMemory = new char[ (int)bytesPerRow * m_iHeight ];
	memset( m_pContextMemory, 0x0, (int)( bytesPerRow * m_iHeight) );

	CGColorSpaceRef colorSpace;

	colorSpace = CGColorSpaceCreateDeviceRGB();
	m_ContextRef = CGBitmapContextCreate( m_pContextMemory, m_iMaxCharWidth, m_iHeight,
										  8,
										  bytesPerRow,
										  colorSpace,
										  kCGImageAlphaPremultipliedLast );
	CGColorSpaceRelease( colorSpace );
	if ( !m_ContextRef )
	{
		goto Fail;
	}
	
	CGContextSetAllowsAntialiasing( m_ContextRef, m_bAntiAliased );
	CGContextSetShouldAntialias( m_ContextRef, m_bAntiAliased );
	CGContextSetTextDrawingMode( m_ContextRef, kCGTextFill ); 
	CGContextSetRGBStrokeColor( m_ContextRef, 1.0, 1.0, 1.0, 1.0 );
	CGContextSetLineWidth( m_ContextRef, 1 );

	return true;

Fail:
	return false;
}


void COSXFont::GetKernedCharWidth( wchar_t ch, wchar_t chBefore, wchar_t chAfter, float &wide, float &abcA, float &abcC )
{
	int a,b,c;
	GetCharABCWidths(ch, a, b, c );
	wide = ( a + b + c );
	abcA = a;
	abcC = c;
}

static bool GetGlyphsForCharacter( CTFontRef hFont, wchar_t ch, CGGlyph* pGlyphs )
{
    UniChar pUniChars[2];
    pUniChars[0] = ch;
    pUniChars[1] = 0;
	
    if ( !CTFontGetGlyphsForCharacters( hFont, pUniChars, pGlyphs, 1 ) )
    {
        char str[2];
        str[0] = (char)ch;
        str[1] = 0;

        CFStringRef s = CFStringCreateWithCString(nullptr, str, kTextEncodingUnicodeDefault);
        pGlyphs[0] = CTFontGetGlyphWithName(hFont, s);
        CFRelease( s );
        if ( !pGlyphs[0] )
        {
            return false;
        }
    }

    return true;
}

//-----------------------------------------------------------------------------
// Purpose: writes the char into the specified 32bpp texture
//-----------------------------------------------------------------------------
void COSXFont::GetCharRGBA( wchar_t ch, int rgbaWide, int rgbaTall, unsigned char *rgba )
{
    wchar_t pWchars[1];
    pWchars[0] = (wchar_t)ch;

    CGGlyph pGlyphs[1];

    if ( !GetGlyphsForCharacter( m_hFont, ch, pGlyphs ) )
    {
        AssertMsg( false, "CTFontGetGlyphsForCharacters failed" );
        return;
    }

	CGRect rect = { { 0, 0 }, { m_iMaxCharWidth, m_iHeight } };
	CGContextClearRect( m_ContextRef, rect );
	
	CGRect pBounds[1];
	
	CTFontGetBoundingRectsForGlyphs( m_hFont, kCTFontDefaultOrientation, pGlyphs, pBounds, 1 );

    CGPoint pPositions[1];

	// The character will be drawn offset by the 'A' distance so adjust
	// it back as this routine only wants the core bits.
    pPositions[0].x = m_iOutlineSize;
	// The DrawGlyphs coordinate system puts zero Y at the bottom of
	// the bitmap and puts the text baseline at zero Y so push
	// it up to place characters where we expect them.
	pPositions[0].y = ( m_iHeight - m_iAscent ) - m_iOutlineSize;
	
    CTFontDrawGlyphs( m_hFont, pGlyphs, pPositions, 1, m_ContextRef );
	
	CGContextFlush( m_ContextRef );
	
	char *pContextData = (char *)CGBitmapContextGetData( m_ContextRef );
	
	uint8 *pchPixelData = rgba;
	for ( int y = 0; y < rgbaTall; y++ )
	{
		char *row = pContextData + y * m_iMaxCharWidth * 4;
		for ( int x = 0; x < rgbaWide; x++ )
		{
			if ( row[0] || row[1] || row[2] || row[3] )
			{
				pchPixelData[0] = 0xff;
				pchPixelData[1] = 0xff;
				pchPixelData[2] = 0xff;
				pchPixelData[3] = row[3];
			}
			else
			{
				pchPixelData[0] = 0;
				pchPixelData[1] = 0;
				pchPixelData[2] = 0;
				pchPixelData[3] = 0;
			}
			row += 4;
			pchPixelData += 4;
		}
	}

	// Draw top and bottom bars for character placement debugging.
#if FORCE_CHAR_BOX_BOUNDS
	pchPixelData = rgba;
	for ( int x = 0; x < rgbaWide; x++ )
	{
		pchPixelData[0] = 0;
		pchPixelData[1] = 0;
		pchPixelData[2] = 0;
		pchPixelData[3] = 0xff;
		pchPixelData += 4;
	}
	pchPixelData = rgba + ( rgbaTall - 1 ) * rgbaWide * 4;
	for ( int x = 0; x < rgbaWide; x++ )
	{
		pchPixelData[0] = 0;
		pchPixelData[1] = 0;
		pchPixelData[2] = 0;
		pchPixelData[3] = 0xff;
		pchPixelData += 4;
	}
#endif
	
	// apply requested effects in specified order
	ApplyDropShadowToTexture( rgbaWide, rgbaTall, rgba, m_iDropShadowOffset );
	ApplyOutlineToTexture( rgbaWide, rgbaTall, rgba, m_iOutlineSize );
	ApplyGaussianBlurToTexture( rgbaWide, rgbaTall, rgba, m_iBlur );
	ApplyScanlineEffectToTexture( rgbaWide, rgbaTall, rgba, m_iScanLines );
	ApplyRotaryEffectToTexture( rgbaWide, rgbaTall, rgba, m_bRotary );
}

//-----------------------------------------------------------------------------
// Purpose: gets the abc widths for a character
//-----------------------------------------------------------------------------
void COSXFont::GetCharABCWidths(int ch, int &a, int &b, int &c)
{
	Assert(IsValid());
	
	// look for it in the cache
	abc_cache_t finder = { (wchar_t)ch };
	
	uint16 i = m_ExtendedABCWidthsCache.Find(finder);
	if (m_ExtendedABCWidthsCache.IsValidIndex(i))
	{
		a = m_ExtendedABCWidthsCache[i].abc.a;
		b = m_ExtendedABCWidthsCache[i].abc.b;
		c = m_ExtendedABCWidthsCache[i].abc.c;
		return;
	}

	a = 0;
	b = 0;
	c = 0;

    wchar_t pWchars[1];

    pWchars[0] = (wchar_t)ch;

    CGGlyph pGlyphs[1];

    if ( !GetGlyphsForCharacter( m_hFont, ch, pGlyphs ) )
    {
        AssertMsg( false, "CTFontGetGlyphsForCharacters failed" );
        return;
    }

    CGSize pAdvances[1];

    CTFontGetAdvancesForGlyphs( m_hFont, kCTFontDefaultOrientation, pGlyphs, pAdvances, 1 );

    CGRect pBounds[1];
    
    CTFontGetBoundingRectsForGlyphs( m_hFont, kCTFontDefaultOrientation, pGlyphs, pBounds, 1 );

    a = 0;
    b = ceil(pAdvances->width);
    c = 0;
	finder.abc.a = a;
	finder.abc.b = b;
	finder.abc.c = c;
	m_ExtendedABCWidthsCache.Insert( finder );
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the font is equivalent to that specified
//-----------------------------------------------------------------------------
bool COSXFont::IsEqualTo(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	if (!Q_stricmp(windowsFontName, m_szName.String() ) 
		&& m_iTall == tall
		&& m_iWeight == weight
		&& m_iBlur == blur
		&& m_iFlags == flags)
		return true;
	
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: returns true only if this font is valid for use
//-----------------------------------------------------------------------------
bool COSXFont::IsValid()
{
	if ( !m_szName.IsEmpty() && m_szName.String()[0] )
		return true;
	
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: returns the height of the font, in pixels
//-----------------------------------------------------------------------------
int COSXFont::GetHeight()
{
	assert(IsValid());
	return m_iHeight;
}

//-----------------------------------------------------------------------------
// Purpose: returns the ascent of the font, in pixels (ascent=units above the base line)
//-----------------------------------------------------------------------------
int COSXFont::GetAscent()
{
	assert(IsValid());
	return m_iAscent;
}

//-----------------------------------------------------------------------------
// Purpose: returns the ascent of the font, in pixels (ascent=units above the base line)
//-----------------------------------------------------------------------------
int COSXFont::GetDescent()
{
	assert(IsValid());
	return m_iDescent;
}

//-----------------------------------------------------------------------------
// Purpose: returns the maximum width of a character, in pixels
//-----------------------------------------------------------------------------
int COSXFont::GetMaxCharWidth()
{
	assert(IsValid());
	return m_iMaxCharWidth;
}

//-----------------------------------------------------------------------------
// Purpose: returns the flags used to make this font, used by the dynamic resizing code
//-----------------------------------------------------------------------------
int COSXFont::GetFlags()
{
	return m_iFlags;
}

//-----------------------------------------------------------------------------
// Purpose: Comparison function for abc widths storage
//-----------------------------------------------------------------------------
bool COSXFont::ExtendedABCWidthsCacheLessFunc(const abc_cache_t &lhs, const abc_cache_t &rhs)
{
	return lhs.wch < rhs.wch;
}

//-----------------------------------------------------------------------------
// Purpose: Comparison function for abc widths storage
//-----------------------------------------------------------------------------
bool COSXFont::ExtendedKernedABCWidthsCacheLessFunc(const kerned_abc_cache_t &lhs, const kerned_abc_cache_t &rhs)
{
	return lhs.wch < rhs.wch || ( lhs.wch == rhs.wch && lhs.wchBefore < rhs.wchBefore ) 
	|| ( lhs.wch == rhs.wch && lhs.wchBefore == rhs.wchBefore && lhs.wchAfter < rhs.wchAfter );
}


void *COSXFont::SetAsActiveFont( CGContextRef cgContext )
{
	CGContextSelectFont ( cgContext, m_szName.String(), m_iHeight, kCGEncodingMacRoman);
	return NULL;
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void COSXFont::Validate( CValidator &validator, char *pchName )
{
	validator.Push( "COSXFont", this, pchName );
	
	m_ExtendedABCWidthsCache.Validate( validator, "m_ExtendedABCWidthsCache" );
	m_ExtendedKernedABCWidthsCache.Validate( validator, "m_ExtendedKernedABCWidthsCache" );
	validator.ClaimMemory( m_pGaussianDistribution );
	
	validator.Pop();
}
#endif // DBGFLAG_VALIDATE
