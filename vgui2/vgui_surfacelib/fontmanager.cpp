//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include <locale.h>
#include "vgui_surfacelib/BitmapFont.h"
#include "vgui_surfacelib/fontmanager.h"
#include "convar.h"
#include <vgui/ISurface.h>
#include <tier0/dbg.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

static CFontManager s_FontManager;

#ifdef WIN32
extern bool s_bSupportsUnicode;
#endif

#if !defined( _X360 )
#define MAX_INITIAL_FONTS	100
#else
#define MAX_INITIAL_FONTS	1
#endif

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CFontManager &FontManager()
{
	return s_FontManager;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFontManager::CFontManager()
{
	// add a single empty font, to act as an invalid font handle 0
	m_FontAmalgams.EnsureCapacity( MAX_INITIAL_FONTS );
	m_FontAmalgams.AddToTail();
	m_Win32Fonts.EnsureCapacity( MAX_INITIAL_FONTS );

#ifdef LINUX
        FT_Error error = FT_Init_FreeType( &library ); 
        if ( error )
                Error( "Unable to initalize freetype library, is it installed?" );
		pFontDataHelper = NULL;
#endif

	// setup our text locale
	setlocale( LC_CTYPE, "" );
	setlocale( LC_TIME, "" );
	setlocale( LC_COLLATE, "" );
	setlocale( LC_MONETARY, "" );

	m_pFileSystem = NULL;
	m_pMaterialSystem = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: language setting for font fallbacks
//-----------------------------------------------------------------------------
void CFontManager::SetLanguage(const char *language)
{
	Q_strncpy(m_szLanguage, language, sizeof(m_szLanguage));
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CFontManager::GetLanguage()
{ 
	return m_szLanguage;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFontManager::~CFontManager()
{
	ClearAllFonts();
#ifdef LINUX
        FT_Done_FreeType( library );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: frees the fonts
//-----------------------------------------------------------------------------
void CFontManager::ClearAllFonts()
{
	// free the fonts
	for (int i = 0; i < m_Win32Fonts.Count(); i++)
	{
		delete m_Win32Fonts[i];
	}
	m_Win32Fonts.RemoveAll();

	m_FontAmalgams.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
vgui::HFont CFontManager::CreateFont()
{
	int i = m_FontAmalgams.AddToTail();
	return i;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the valid glyph ranges for a font created by CreateFont()
//-----------------------------------------------------------------------------
bool CFontManager::SetFontGlyphSet(HFont font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	return SetFontGlyphSet( font, windowsFontName, tall, weight, blur, scanlines, flags, 0, 0);
}

//-----------------------------------------------------------------------------
// Purpose: Sets the valid glyph ranges for a font created by CreateFont()
//-----------------------------------------------------------------------------
bool CFontManager::SetFontGlyphSet(HFont font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags, int nRangeMin, int nRangeMax)
{
	// ignore all but the first font added
	// need to rev vgui versions and change the name of this function
	if ( m_FontAmalgams[font].GetCount() > 0 )
	{
		// clear any existing fonts
		m_FontAmalgams[font].RemoveAll();
	}

	bool bForceSingleFont = false;
	if ( IsX360() )
	{
		//-----//
		// 360 //
		//-----//

		// AV - The 360 must use the same size font for 0-255 and 256-0xFFFF regardless of the font since the
		//      fontAmalgam can only deal with a consistent font height for all fonts in a single amalgam. We
		//      could change this if we forced all fonts within a single amalgam to have the same height with
		//      the font baselines aligned, but even then the fonts wouldn't look great, because different
		//      fonts set to the same size don't necessarily have the same height visually. We need to revisit
		//      this before shipping l4d2 on the PC!
		bForceSingleFont = true;

		// discovered xbox only allows glyphs from these languages from the foreign fallback font
		// prefer to have the entire range of chars from the font so UI doesn't suffer from glyph disparity
		if ( !V_stricmp( windowsFontName, "toolbox" ) || !V_stricmp( windowsFontName, "courier new" ) )
		{
			// toolbox stays as-is
			// courier new is an internal debug font, not part of customer UI, need it stay as is
		}
		else
		{
			bool bUseFallback = false;
			if ( !V_stricmp( m_szLanguage, "portuguese" ) ||
				 !V_stricmp( m_szLanguage, "polish" ) )
			{
				static ConVarRef mat_xbox_iswidescreen( "mat_xbox_iswidescreen" );
				static ConVarRef mat_xbox_ishidef( "mat_xbox_ishidef" );

				// we can support these languages with our desired fonts in hidef/widescreen modes only
				// we must fallback to the more legible font in the lowdef or non-widescreen
				bUseFallback = !( mat_xbox_iswidescreen.GetBool() && mat_xbox_ishidef.GetBool() );
			}

			if ( bUseFallback ||
				!V_stricmp( m_szLanguage, "japanese" ) ||
				!V_stricmp( m_szLanguage, "korean" ) ||
				!V_stricmp( m_szLanguage, "schinese" ) ||
				!V_stricmp( m_szLanguage, "tchinese" ) ||
				!V_stricmp( m_szLanguage, "russian" ) )
			{
				// these languages must use the font that has their glyphs
				// these language require a high degree of legibility
				windowsFontName = GetForeignFallbackFontName();
			}
		}
	}
	else
	{
		//----//
		// PC //
		//----//

		// AV - The PC has the same issues caused by multiple fonts in a single amalgam with different font
		//      heights...see comment above. Given the available languages in Steam, the languages below
		//      were illegible at 1024x768. Resolutions of 800x600 and 640x480 are a complete mess for all
		//      languages, including English. We probably need to fallback to Tahoma for all languages when
		//      the vertical resolution < 720. This will probably be the next check-in, but we need to evaluate
		//      this further tomorrow.

		if ( !V_stricmp( windowsFontName, "toolbox" ) || !V_stricmp( windowsFontName, "courier new" ) )
		{
			// toolbox stays as-is
			// courier new is an internal debug font, not part of customer UI, need it stay as is
		}
		else
		{
			// These languages are illegible @ vertical resolutions <= 768
			if ( !V_stricmp( m_szLanguage, "korean" ) ||
				 !V_stricmp( m_szLanguage, "schinese" ) ||
				 !V_stricmp( m_szLanguage, "tchinese" ) ||
				 !V_stricmp( m_szLanguage, "russian" ) ||
				 !V_stricmp( m_szLanguage, "thai" ) ||
				 !V_stricmp( m_szLanguage, "japanese" ) ||
				 !V_stricmp( m_szLanguage, "czech" ) )
			{
				windowsFontName = GetForeignFallbackFontName();
				bForceSingleFont = true;
			}
		}
	}

	// AV - If we actually want to support multiple fonts within an amalgam, we need a change here. Currently,
	//      the code will use winFont for 0-255 and pExtendedFont for 256-0xFFFF! But since the functions for
	//      getting the font height from the amalgam can only return one height, the heights of the two fonts
	//      need to be identical. This isn't trivial because even if we query both fonts for their height
	//      first, that won't force their baselines within the font pages to be aligned. So we would have to
	//      do something much more complicated where we loop over all characters in each font to find the
	//      absolute ascent and descent above/below the baseline and then when we create the font, align both
	//      fonts to the shared baseline with a shared height. But even with the font baselines aligned with a
	//      shared height, the fonts still wouldn't look great, because different fonts set to the same size
	//      don't necessarily have the same height visually. We need to revisit this before shipping l4d2 on the PC!
	//      And there are still issues with what I'm suggesting here because when we ask the font API what
	//      the maxHeight, maxAscent, maxDescent is, we get inconsistent results, so I'm not even sure we could
	//      successfully align the baseline of two fonts in the font pages.

	font_t *winFont = CreateOrFindWin32Font( windowsFontName, tall, weight, blur, scanlines, flags );

	// cycle until valid english/extended font support has been created
	do
	{
		// add to the amalgam
		if ( bForceSingleFont || IsFontForeignLanguageCapable( windowsFontName ) )
		{
			if ( winFont )
			{
				// font supports the full range of characters
				m_FontAmalgams[font].AddFont( winFont, 0x0000, 0xFFFF );
				return true;
			}
		}
		else
		{
			// font cannot provide glyphs and just supports the normal range
			// redirect to a font that can supply glyps
			const char *localizedFontName = GetForeignFallbackFontName();
			if ( winFont && !stricmp( localizedFontName, windowsFontName ) )
			{
				// it's the same font and can support the full range
				m_FontAmalgams[font].AddFont( winFont, 0x0000, 0xFFFF );
				return true;
			}

			// create the extended support font
			font_t *pExtendedFont = CreateOrFindWin32Font( localizedFontName, tall, weight, blur, scanlines, flags );
			if ( winFont && pExtendedFont )
			{
				// use the normal font for english characters, and the extended font for the rest
				int nMin = 0x0000, nMax = 0x00FF;

				// did we specify a range?
				if ( nRangeMin > 0 || nRangeMax > 0 )
				{
					nMin = nRangeMin;
					nMax = nRangeMax;

					// make sure they're in the correct order
					if ( nMin > nMax )
					{
						int nTemp = nMin;
						nMin = nMax;
						nMax = nTemp;
					}
				}

				if ( nMin > 0 )
				{
					m_FontAmalgams[font].AddFont( pExtendedFont, 0x0000, nMin - 1 );
				}

				m_FontAmalgams[font].AddFont( winFont, nMin, nMax );

				if ( nMax < 0xFFFF )
				{
					m_FontAmalgams[font].AddFont( pExtendedFont, nMax + 1, 0xFFFF );
				}

				return true;
			}
			else if ( pExtendedFont )
			{
				// the normal font failed to create
				// just use the extended font for the full range
				m_FontAmalgams[font].AddFont( pExtendedFont, 0x0000, 0xFFFF );
				return true;
			}
		}
		// no valid font has been created, so fallback to a different font and try again
	} 
	while ( NULL != ( windowsFontName = GetFallbackFontName( windowsFontName ) ) );

	// nothing successfully created
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: adds glyphs to a font created by CreateFont()
//-----------------------------------------------------------------------------
bool CFontManager::SetBitmapFontGlyphSet(HFont font, const char *windowsFontName, float scalex, float scaley, int flags)
{
	if ( m_FontAmalgams[font].GetCount() > 0 )
	{
		// clear any existing fonts
		m_FontAmalgams[font].RemoveAll();
	}

	CBitmapFont *winFont = CreateOrFindBitmapFont( windowsFontName, scalex, scaley, flags );
	if ( winFont )
	{
		// bitmap fonts are only 0-255
		m_FontAmalgams[font].AddFont( winFont, 0x0000, 0x00FF );
		return true;
	}

	// nothing successfully created
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a new win32 font, or reuses one if possible
//-----------------------------------------------------------------------------
font_t *CFontManager::CreateOrFindWin32Font(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	// see if we already have the win32 font
	font_t *winFont = NULL;
	int i;
	for (i = 0; i < m_Win32Fonts.Count(); i++)
	{
		if (m_Win32Fonts[i]->IsEqualTo(windowsFontName, tall, weight, blur, scanlines, flags))
		{
			winFont = m_Win32Fonts[i];
			break;
		}
	}

	// create the new win32font if we didn't find it
	if (!winFont)
	{
		MEM_ALLOC_CREDIT();

		i = m_Win32Fonts.AddToTail();
#ifdef LINUX
		int memSize = 0;
		void *pchFontData = pFontDataHelper( windowsFontName, memSize );
		if ( pchFontData )
		{
			m_Win32Fonts[i] = new font_t();
			if (m_Win32Fonts[i]->CreateFromMemory( windowsFontName, pchFontData, memSize, tall, weight, blur, scanlines, flags))
			{
				// add to the list
				winFont = m_Win32Fonts[i];
			}
			else
			{
				// failed to create, remove
				delete m_Win32Fonts[i];
				m_Win32Fonts.Remove(i);
				return NULL;
			}
		}
		else
		{
#endif
		m_Win32Fonts[i] = new font_t();
		if (m_Win32Fonts[i]->Create(windowsFontName, tall, weight, blur, scanlines, flags))
		{
			// add to the list
			winFont = m_Win32Fonts[i];
		}
		else
		{
			// failed to create, remove
			delete m_Win32Fonts[i];
			m_Win32Fonts.Remove(i);
			return NULL;
		}
#ifdef LINUX
		}
#endif
	}

	return winFont;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a new win32 font, or reuses one if possible
//-----------------------------------------------------------------------------
CBitmapFont *CFontManager::CreateOrFindBitmapFont(const char *windowsFontName, float scalex, float scaley, int flags)
{
	// see if we already have the font
	CBitmapFont *winFont = NULL;
	int i;
	for ( i = 0; i < m_Win32Fonts.Count(); i++ )
	{
		font_t *font = m_Win32Fonts[i];

		// Only looking for bitmap fonts
		int testflags = font->GetFlags();
		if ( !( testflags & FONTFLAG_BITMAP ) )
		{
			continue;
		}

		CBitmapFont *bitmapFont = reinterpret_cast< CBitmapFont* >( font );
		if ( bitmapFont->IsEqualTo( windowsFontName, scalex, scaley, flags ) )
		{
			winFont = bitmapFont;
			break;
		}
	}

	// create the font if we didn't find it
	if ( !winFont )
	{
		MEM_ALLOC_CREDIT();

		i = m_Win32Fonts.AddToTail();

		CBitmapFont *bitmapFont = new CBitmapFont();
		if ( bitmapFont->Create( windowsFontName, scalex, scaley, flags ) )
		{
			// add to the list
			m_Win32Fonts[i] = bitmapFont;
			winFont = bitmapFont;
		}
		else
		{
			// failed to create, remove
			delete bitmapFont;
			m_Win32Fonts.Remove(i);
			return NULL;
		}
	}

	return winFont;
}

//-----------------------------------------------------------------------------
// Purpose: sets the scale of a bitmap font
//-----------------------------------------------------------------------------
void CFontManager::SetFontScale(vgui::HFont font, float sx, float sy)
{
	m_FontAmalgams[font].SetFontScale( sx, sy );
}

const char *CFontManager::GetFontName( HFont font )
{
	// ignore the amalgam of disparate char ranges, assume the first font
	return m_FontAmalgams[font].GetFontName( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: gets the windows font for the particular font in the amalgam
//-----------------------------------------------------------------------------
font_t *CFontManager::GetFontForChar( vgui::HFont font, wchar_t wch )
{
	return m_FontAmalgams[font].GetFontForChar(wch);
}

//-----------------------------------------------------------------------------
// Purpose: returns the abc widths of a single character
//-----------------------------------------------------------------------------
void CFontManager::GetCharABCwide(HFont font, int ch, int &a, int &b, int &c)
{
	font_t *winFont = m_FontAmalgams[font].GetFontForChar(ch);
	if (winFont)
	{
		winFont->GetCharABCWidths(ch, a, b, c);
	}
	else
	{
		// no font for this range, just use the default width
		a = c = 0;
		b = m_FontAmalgams[font].GetFontMaxWidth();
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the max height of a font
//-----------------------------------------------------------------------------
int CFontManager::GetFontTall(HFont font)
{
	return m_FontAmalgams[font].GetFontHeight();
}

//-----------------------------------------------------------------------------
// Purpose: returns the ascent of a font
//-----------------------------------------------------------------------------
int CFontManager::GetFontAscent(HFont font, wchar_t wch)
{
	font_t *winFont = m_FontAmalgams[font].GetFontForChar(wch);
	if ( winFont )
	{
		return winFont->GetAscent();
	}
	else
	{
		return 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CFontManager::IsFontAdditive(HFont font)
{
	return ( m_FontAmalgams[font].GetFlags( 0 ) & FONTFLAG_ADDITIVE ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CFontManager::IsBitmapFont(HFont font)
{
	// A FontAmalgam is either some number of non-bitmap fonts, or a single bitmap font - so this check is valid
	return ( m_FontAmalgams[font].GetFlags( 0 ) & FONTFLAG_BITMAP ) ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: returns the pixel width of a single character
//-----------------------------------------------------------------------------
int CFontManager::GetCharacterWidth(HFont font, int ch)
{
	if ( !iswcntrl( ch ) )
	{
		int a, b, c;
		GetCharABCwide(font, ch, a, b, c);
		return (a + b + c);
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: returns the area of a text string, including newlines
//-----------------------------------------------------------------------------
void CFontManager::GetTextSize(HFont font, const wchar_t *text, int &wide, int &tall)
{
	wide = 0;
	tall = 0;
	
	if (!text)
		return;

	// AV - Calling GetFontTall() for an amalgam with multiple fonts will return
	//      the font height of the first font only! We should be doing something like:
	//
	//          tall = FontManager().GetFontForChar( font, text[0] )->GetHeight();
	//
	//      but that's a little hacky since we're only looking at the first character!
	//      Same goes for the calls to GetFontTall() in the for loop below
	tall = GetFontTall(font);

	float xx = 0;
	char chBefore = 0;
	char chAfter = 0;
	for (int i = 0; ; i++)
	{
		wchar_t ch = text[i];
		if (ch == 0)
		{
			break;
		}

		chAfter = text[i+1];

		if (ch == '\n')
		{
			// AV - See note above about calling this instead: tall += FontManager().GetFontForChar( font, text[0] )->GetHeight();
			tall += GetFontTall(font);

			xx=0;
		}
		else if (ch == '&')
		{
			// underscore character, so skip
		}
		else
		{
			float flWide, flabcA, flabcC;
			GetKernedCharWidth( font, ch, chBefore, chAfter, flWide, flabcA, flabcC );
			xx += flWide;
			if (xx > wide)
			{
				wide = ceil(xx);
			}
		}
		chBefore = ch;
	}
}

// font validation functions
struct FallbackFont_t
{
	const char *font;
	const char *fallbackFont;
};

const char *g_szValidAsianFonts[] = { 
#ifdef WIN32
	"Marlett",
#else
	"Helvetica",
#endif
	NULL };

// list of how fonts fallback
FallbackFont_t g_FallbackFonts[] =
{
	{ "Times New Roman", "Courier New" },
	{ "Courier New", "Courier" },
	{ "Verdana", "Arial" },
	{ "Trebuchet MS", "Arial" },
#ifdef WIN32
	{ "Tahoma", NULL },
	{ NULL, "Tahoma" },		// every other font falls back to this
#else
	{ "Tahoma", "Helvetica" },
	{ "Helvetica", NULL },
	{ NULL, "Helvetica" }		// every other font falls back to this

#endif
};

//-----------------------------------------------------------------------------
// Purpose: returns true if the font is in the list of OK asian fonts
//-----------------------------------------------------------------------------
bool CFontManager::IsFontForeignLanguageCapable(const char *windowsFontName)
{
	if ( IsX360() )
	{
		return false;
	}

	for (int i = 0; g_szValidAsianFonts[i] != NULL; i++)
	{
		if (!stricmp(g_szValidAsianFonts[i], windowsFontName))
			return true;
	}

	// typeface isn't supported by asian languages
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: fallback fonts
//-----------------------------------------------------------------------------
const char *CFontManager::GetFallbackFontName(const char *windowsFontName)
{
	int i;
	for ( i = 0; g_FallbackFonts[i].font != NULL; i++ )
	{
		if (!stricmp(g_FallbackFonts[i].font, windowsFontName))
			return g_FallbackFonts[i].fallbackFont;
	}

	// the ultimate fallback
	return g_FallbackFonts[i].fallbackFont;
}

struct Win98ForeignFallbackFont_t
{
	const char *language;
	const char *fallbackFont;
};

// list of how fonts fallback
Win98ForeignFallbackFont_t g_Win98ForeignFallbackFonts[] =
{
	{ "russian", "system" },
	{ "japanese", "win98japanese" },
	{ "thai", "system" },
#ifdef WIN32
	{ NULL, "Tahoma" },		// every other font falls back to this
#else
	{ NULL, "Helvetica" },		// every other font falls back to this	
#endif
};

//-----------------------------------------------------------------------------
// Purpose: specialized fonts
//-----------------------------------------------------------------------------
const char *CFontManager::GetForeignFallbackFontName()
{
#ifdef WIN32
	if ( s_bSupportsUnicode )
	{
		if ( IsX360() )
		{
			return "arial unicode ms";
		}

		// tahoma has all the necessary characters for asian/russian languages for winXP/2K+
		return "Tahoma";
	}
#endif
	
	int i;
	for (i = 0; g_Win98ForeignFallbackFonts[i].language != NULL; i++)
	{
		if (!stricmp(g_Win98ForeignFallbackFonts[i].language, m_szLanguage))
			return g_Win98ForeignFallbackFonts[i].fallbackFont;
	}

	// the ultimate fallback
	return g_Win98ForeignFallbackFonts[i].fallbackFont;
}

#if defined( _X360 )
bool CFontManager::GetCachedXUIMetrics( const char *pFontName, int tall, int style, XUIFontMetrics *pFontMetrics, XUICharMetrics charMetrics[256] )
{
	// linear lookup is good enough
	CUtlSymbol fontSymbol = pFontName;
	bool bFound = false;
	int i;
	for ( i = 0; i < m_XUIMetricCache.Count(); i++ )
	{
		if ( m_XUIMetricCache[i].fontSymbol == fontSymbol && m_XUIMetricCache[i].tall == tall && m_XUIMetricCache[i].style == style )
		{
			bFound = true;
			break;
		}
	}
	if ( !bFound )
	{
		return false;
	}

	// get from the cache
	*pFontMetrics = m_XUIMetricCache[i].fontMetrics;
	V_memcpy( charMetrics, m_XUIMetricCache[i].charMetrics, 256 * sizeof( XUICharMetrics ) );
	return true;
}
#endif

#if defined( _X360 )
void CFontManager::SetCachedXUIMetrics( const char *pFontName, int tall, int style, XUIFontMetrics *pFontMetrics, XUICharMetrics charMetrics[256] )
{
	MEM_ALLOC_CREDIT();

	int i = m_XUIMetricCache.AddToTail();

	m_XUIMetricCache[i].fontSymbol = pFontName;
	m_XUIMetricCache[i].tall = tall;
	m_XUIMetricCache[i].style = style;
	m_XUIMetricCache[i].fontMetrics = *pFontMetrics;
	V_memcpy( m_XUIMetricCache[i].charMetrics, charMetrics, 256 * sizeof( XUICharMetrics ) );
}
#endif

void CFontManager::ClearTemporaryFontCache()
{
#if defined( _X360 )
	COM_TimestampedLog( "ClearTemporaryFontCache(): Start" );

	m_XUIMetricCache.Purge();

	// many fonts are blindly precached by vgui and never used
	// font will re-open if glyph is actually requested
	for ( int i = 0; i < m_Win32Fonts.Count(); i++ )
	{
		m_Win32Fonts[i]->CloseResource();
	}

	COM_TimestampedLog( "ClearTemporaryFontCache(): Finish" );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: returns the max height of a font
//-----------------------------------------------------------------------------
bool CFontManager::GetFontUnderlined( HFont font )
{
	return m_FontAmalgams[font].GetUnderlined();
}

void CFontManager::GetKernedCharWidth( vgui::HFont font, wchar_t ch, wchar_t chBefore, wchar_t chAfter, float &wide, float &flabcA, float &flabcC )
{
	wide = 0.0f;
	flabcA = 0.0f;
	
	Assert( font != vgui::INVALID_FONT );
	if ( font == vgui::INVALID_FONT )
		return;
		
	font_t *pFont = m_FontAmalgams[font].GetFontForChar(ch);
	if ( !pFont )
	{
		// no font for this range, just use the default width
		flabcA = 0.0f;
		wide = m_FontAmalgams[font].GetFontMaxWidth();
		return;
	}
	
	if ( m_FontAmalgams[font].GetFontForChar( chBefore ) != pFont )
		chBefore = 0;
	
	if ( m_FontAmalgams[font].GetFontForChar( chAfter ) != pFont )
		chAfter = 0;
	
	pFont->GetKernedCharWidth( ch, chBefore, chAfter, wide, flabcA, flabcC );
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void CFontManager::Validate( CValidator &validator, char *pchName )
{
	validator.Push( "CFontManager", this, pchName );

	ValidateObj( m_FontAmalgams );
	for ( int iFont = 0; iFont < m_FontAmalgams.Count(); iFont++ )
	{
		ValidateObj( m_FontAmalgams[iFont] );
	}

	ValidateObj( m_Win32Fonts );
	for ( int iWin32Font = 0; iWin32Font < m_Win32Fonts.Count(); iWin32Font++ )
	{
		ValidatePtr( m_Win32Fonts[ iWin32Font ] );
	}

	validator.Pop();
}
#endif // DBGFLAG_VALIDATE

