//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include "vgui_surfacelib/linuxfont.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <tier0/dbg.h>
#include <vgui/ISurface.h>
#include <utlbuffer.h>
#include <fontconfig/fontconfig.h>
#include "materialsystem/imaterialsystem.h"

#include "vgui_surfacelib/fontmanager.h"
#include "FontEffects.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace {

//Due to different font rendering approaches on different platforms, we have
//to apply custom tweaks to fonts on Linux to make them render as desired.
struct MetricsTweaks_t
{
	const char *m_windowsFontName;
	int m_tallAdjust;
};

MetricsTweaks_t GetFontMetricsTweaks(const char* windowsFontName)
{
	static const MetricsTweaks_t FontMetricTweaks[] =
	{
		{ "Stubble bold", -5 },
	};

	for( int i = 0; i != Q_ARRAYSIZE( FontMetricTweaks ); ++i )
	{
		if ( !Q_stricmp( windowsFontName, FontMetricTweaks[i].m_windowsFontName ) )
		{
			return FontMetricTweaks[i];
		}
	}

	static const MetricsTweaks_t DefaultMetricTweaks = { NULL, 0 };
	return DefaultMetricTweaks;
}

// Freetype uses a lot of fixed float values that are 26.6 splits of a 32 bit word.
// to make it an int, shift down the 6 bits and round up if the high bit of the 6
// bits was set.
inline int32_t FIXED6_2INT(int32_t x)   { return ( (x>>6) + ( (x&0x20) ? (x<0 ? -1 : 1) : 0) ); }
inline float   FIXED6_2FLOAT(int32_t x) { return (float)x / 64.0f; }
inline int32_t INT_2FIXED6(int32_t x)   { return x << 6; }

}

bool CLinuxFont::ms_bSetFriendlyNameCacheLessFunc = false;
CUtlRBTree< CLinuxFont::font_name_entry > CLinuxFont::m_FriendlyNameCache;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CLinuxFont::CLinuxFont() : m_ExtendedABCWidthsCache(256, 0, &ExtendedABCWidthsCacheLessFunc),
m_ExtendedKernedABCWidthsCache( 256, 0, &ExtendedKernedABCWidthsCacheLessFunc )
{
	m_iTall = 0;
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
	if ( !ms_bSetFriendlyNameCacheLessFunc )
	{
		ms_bSetFriendlyNameCacheLessFunc = true;
		SetDefLessFunc( m_FriendlyNameCache );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CLinuxFont::~CLinuxFont()
{
}


//-----------------------------------------------------------------------------
// Purpose: build a map of friendly (char *) name to crazy ATSU bytestream, so we can ask for "Tahoma" and actually load it
//-----------------------------------------------------------------------------
void CLinuxFont::CreateFontList()
{
	if ( m_FriendlyNameCache.Count() > 0 ) 
		return;

	if(!FcInit()) 
		return;
    FcConfig *config;
    FcPattern *pat;
    FcObjectSet *os;
    FcFontSet *fontset;
    int i;
    char *file;
	const char *name;

    config = FcConfigGetCurrent();
	FcConfigAppFontAddDir(config, "platform/vgui/fonts");
    pat = FcPatternCreate();
    os = FcObjectSetCreate();
    FcObjectSetAdd(os, FC_FILE);
    FcObjectSetAdd(os, FC_FULLNAME);
    FcObjectSetAdd(os, FC_FAMILY);
    FcObjectSetAdd(os, FC_SCALABLE);
    fontset = FcFontList(config, pat, os);
    if(!fontset) 
		return;
    for(i = 0; i < fontset->nfont; i++) 
	{
        FcBool scalable;

        if ( FcPatternGetBool(fontset->fonts[i], FC_SCALABLE, 0, &scalable) == FcResultMatch && !scalable )
            continue;


        if ( FcPatternGetString(fontset->fonts[i], FC_FAMILY, 0, (FcChar8**)&name) != FcResultMatch )
			continue;
		if ( FcPatternGetString(fontset->fonts[i], FC_FILE, 0, (FcChar8**)&file) != FcResultMatch )
			continue;
		
		font_name_entry entry;
        entry.m_pchFile = (char *)malloc( Q_strlen(file) + 1 );
        entry.m_pchFriendlyName = (char *)malloc( Q_strlen(name) +1);
        Q_memcpy( entry.m_pchFile, file, Q_strlen(file) + 1 );
        Q_memcpy( entry.m_pchFriendlyName, name, Q_strlen(name) +1);
        m_FriendlyNameCache.Insert( entry );

		// substitute Vera Sans for Tahoma on X
		if ( !V_stricmp( name, "Bitstream Vera Sans" ) )
		{
			name = "Tahoma";
			entry.m_pchFile = (char *)malloc( Q_strlen(file) + 1 );
			entry.m_pchFriendlyName = (char *)malloc( Q_strlen(name) +1);
			Q_memcpy( entry.m_pchFile, file, Q_strlen(file) + 1 );
			Q_memcpy( entry.m_pchFriendlyName, name, Q_strlen(name) +1);
			m_FriendlyNameCache.Insert( entry );

			name = "Verdana";
			entry.m_pchFile = (char *)malloc( Q_strlen(file) + 1 );
			entry.m_pchFriendlyName = (char *)malloc( Q_strlen(name) +1);
			Q_memcpy( entry.m_pchFile, file, Q_strlen(file) + 1 );
			Q_memcpy( entry.m_pchFriendlyName, name, Q_strlen(name) +1);
			m_FriendlyNameCache.Insert( entry );

			name = "Lucidia Console";
			entry.m_pchFile = (char *)malloc( Q_strlen(file) + 1 );
			entry.m_pchFriendlyName = (char *)malloc( Q_strlen(name) +1);
			Q_memcpy( entry.m_pchFile, file, Q_strlen(file) + 1 );
			Q_memcpy( entry.m_pchFriendlyName, name, Q_strlen(name) +1);
			m_FriendlyNameCache.Insert( entry );
		}
    }

    FcFontSetDestroy(fontset);
    FcObjectSetDestroy(os);
    FcPatternDestroy(pat);
}

static FcPattern* FontMatch(const char* type, FcType vtype, const void* value,
                            ...)
{
    va_list ap;
    va_start(ap, value);

    FcPattern* pattern = FcPatternCreate();

    for (;;) {
        FcValue fcvalue;
        fcvalue.type = vtype;
        switch (vtype) {
            case FcTypeString:
                fcvalue.u.s = (FcChar8*) value;
                break;
            case FcTypeInteger:
                fcvalue.u.i = (int) value;
                break;
            default:
                Assert(!"FontMatch unhandled type");
        }
        FcPatternAdd(pattern, type, fcvalue, 0);

        type = va_arg(ap, const char *);
        if (!type)
            break;
        // FcType is promoted to int when passed through ...
        vtype = static_cast<FcType>(va_arg(ap, int));
        value = va_arg(ap, const void *);
    };
    va_end(ap);

    FcConfigSubstitute(0, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result;
    FcPattern* match = FcFontMatch(0, pattern, &result);
    FcPatternDestroy(pattern);

    return match;
}

bool CLinuxFont::CreateFromMemory(const char *windowsFontName, void *data, int size, int tall, int weight, int blur, int scanlines, int flags)
{
	// setup font properties
	m_szName = windowsFontName;
	m_iTall = tall;
	m_iWeight = weight;
	m_iFlags = flags;
	m_bAntiAliased = flags & FONTFLAG_ANTIALIAS;
	m_bUnderlined = flags & FONTFLAG_UNDERLINE;
	m_iDropShadowOffset = (flags & FONTFLAG_DROPSHADOW) ? 1 : 0;
	m_iOutlineSize = (flags & FONTFLAG_OUTLINE) ? 1 : 0;
	m_iBlur = blur;
	m_iScanLines = scanlines;
	m_bRotary = flags & FONTFLAG_ROTARY;
	m_bAdditive = flags & FONTFLAG_ADDITIVE;

	FT_Error error = FT_New_Memory_Face( FontManager().GetFontLibraryHandle(), (FT_Byte *)data, size, 0, &face );
	if ( error == FT_Err_Unknown_File_Format ) 
	{
		return false;
	} 
	else if ( error ) 
	{ 
		return false;
	} 

	InitMetrics();
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: creates the font from windows.  returns false if font does not exist in the OS.
//-----------------------------------------------------------------------------
bool CLinuxFont::Create(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	// setup font properties
	m_szName = windowsFontName;
	m_iTall = tall;
	m_iWeight = weight;
	m_iFlags = flags;
	m_bAntiAliased = flags & FONTFLAG_ANTIALIAS;
	m_bUnderlined = flags & FONTFLAG_UNDERLINE;
	m_iDropShadowOffset = (flags & FONTFLAG_DROPSHADOW) ? 1 : 0;
	m_iOutlineSize = (flags & FONTFLAG_OUTLINE) ? 1 : 0;
	m_iBlur = blur;
	m_iScanLines = scanlines;
	m_bRotary = flags & FONTFLAG_ROTARY;
	m_bAdditive = flags & FONTFLAG_ADDITIVE;

	CreateFontList();

	const char *pchFontName = windowsFontName;
	if ( !Q_stricmp( pchFontName, "Tahoma" ) )
		pchFontName = "Bitstream Vera Sans";
    const int italic = flags & FONTFLAG_ITALIC ? FC_SLANT_ITALIC : FC_SLANT_ROMAN;
    FcPattern* match = FontMatch(FC_FAMILY, FcTypeString, pchFontName,
                                 FC_WEIGHT, FcTypeInteger, FC_WEIGHT_NORMAL,
                                 FC_SLANT, FcTypeInteger, italic,
                                 NULL);

 	if (!match)
    {
		AssertMsg1( false, "Unable to find font named %s\n", windowsFontName );
        m_szName = "";
        return false;
    }
	else
	{
		FcChar8* filename;
		if ( FcPatternGetString(match, FC_FILE, 0, &filename) != FcResultMatch )
		{
			AssertMsg1( false, "Unable to find font named %s\n", windowsFontName );
		    m_szName = "";
		    FcPatternDestroy(match);
		    return false;
		}
	
		FT_Error error = FT_New_Face( FontManager().GetFontLibraryHandle(), (const char *)filename, 0, &face );

		// Only destroy the pattern at this point so that "filename" is pointing
		// to valid memory
		FcPatternDestroy(match);

		if ( error == FT_Err_Unknown_File_Format )
		{
			return false;
		} 
		else if ( error ) 
		{ 
			return false;
		} 

		if ( face->charmap == nullptr )
		{
			FT_Error error = FT_Select_Charmap( face, FT_ENCODING_APPLE_ROMAN );
			if ( error )
			{
				FT_Done_Face( face );
				face = NULL;
	
				Msg( "Font %s has no valid charmap\n", windowsFontName );
				return false;
			}
		}
	}

	InitMetrics();
	return true;
}

void CLinuxFont::InitMetrics()
{
	const MetricsTweaks_t metricTweaks = GetFontMetricsTweaks( m_szName );
	
	FT_Set_Pixel_Sizes( face, 0, m_iTall + metricTweaks.m_tallAdjust );

	m_iAscent = FIXED6_2INT( face->size->metrics.ascender );
	m_iMaxCharWidth = FIXED6_2INT( face->size->metrics.max_advance );

	const int fxpHeight = face->size->metrics.height + INT_2FIXED6( m_iDropShadowOffset + 2 * m_iOutlineSize );
	m_iHeight = FIXED6_2INT( fxpHeight );

	// calculate our gaussian distribution for if we're blurred
	if (m_iBlur > 1)
	{
		m_pGaussianDistribution = new float[m_iBlur * 2 + 1];
		double sigma = 0.683 * m_iBlur;
		for (int x = 0; x <= (m_iBlur * 2); x++)
		{
			int val = x - m_iBlur;
			m_pGaussianDistribution[x] = (float)(1.0f / sqrt(2 * 3.14 * sigma * sigma)) * pow(2.7, -1 * (val * val) / (2 * sigma * sigma));

			// brightening factor
			m_pGaussianDistribution[x] *= 1;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: writes the char into the specified 32bpp texture
//-----------------------------------------------------------------------------
void CLinuxFont::GetCharRGBA(wchar_t ch, int rgbaWide, int rgbaTall, unsigned char *prgba )
{
	bool bShouldAntialias = m_bAntiAliased;
	// filter out 
	if ( ch > 0x00FF && !(m_iFlags & FONTFLAG_CUSTOM) )
	{
		bShouldAntialias = false;
	}
	
	FT_Error error = FT_Load_Char( face,ch, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL); 
	if ( error )
		return;

	int glyph_index = FT_Get_Char_Index( face, ch );
	error = FT_Load_Glyph( face, glyph_index, FT_LOAD_RENDER );
	if ( error )
	{
		fprintf( stderr, "Error in FL_Load_Glyph: %x\n", error );
		return;
	}

	FT_GlyphSlot slot = face->glyph;
	uint32 nSkipRows = ( m_iAscent - slot->bitmap_top );
	if ( nSkipRows )
       nSkipRows--;
	if ( nSkipRows > rgbaTall )
		return;

	unsigned char *rgba = prgba + ( nSkipRows * rgbaWide * 4 );
	FT_Bitmap bitmap = face->glyph->bitmap;

	Assert( bitmap.rows <= rgbaTall );
	Assert( rgbaWide >= bitmap.width + m_iBlur );
	if ( bitmap.width == 0 )
		return;

	/* now draw to our target surface */
	for ( int y = 0; y < MIN( bitmap.rows, rgbaTall ); y++ )
	{
		for ( int x = 0; x < bitmap.width; x++ )
		{
			int rgbaOffset = 4*(x + m_iBlur); // +(rgbaTall-y-1)*rgbaWide*4
			rgba[ rgbaOffset]   =  255;
			rgba[ rgbaOffset+1] =  255;
			rgba[ rgbaOffset+2] =  255;
			rgba[ rgbaOffset+3] =  bitmap.buffer[ x + y*bitmap.width ];
		}
		rgba += ( rgbaWide*4 );
	}

	// apply requested effects in specified order
	ApplyDropShadowToTexture( rgbaWide, rgbaTall, prgba, m_iDropShadowOffset );
	ApplyOutlineToTexture( rgbaWide, rgbaTall, prgba, m_iOutlineSize );
	ApplyGaussianBlurToTexture( rgbaWide, rgbaTall, prgba, m_iBlur );
	ApplyScanlineEffectToTexture( rgbaWide, rgbaTall, prgba, m_iScanLines );
	ApplyRotaryEffectToTexture( rgbaWide, rgbaTall, prgba, m_bRotary );
}

void CLinuxFont::GetKernedCharWidth( wchar_t ch, wchar_t chBefore, wchar_t chAfter, float &wide, float &abcA, float &abcC )
{
	abcA = abcC = wide = 0.0f;

	// look for it in the cache
	kerned_abc_cache_t finder = { ch, chBefore, chAfter };
	
	unsigned short iKerned = m_ExtendedKernedABCWidthsCache.Find(finder);
	if (m_ExtendedKernedABCWidthsCache.IsValidIndex(iKerned))
	{
		abcA = 0; //$ NYI. m_ExtendedKernedABCWidthsCache[iKerned].abc.abcA;
		abcC = 0; //$ NYI. m_ExtendedKernedABCWidthsCache[iKerned].abc.abcC;
		wide = m_ExtendedKernedABCWidthsCache[iKerned].abc.wide;
		return;
	}

    FT_UInt       glyph_index;
	FT_Bool       use_kerning;
	FT_UInt       previous;
	int32_t       iFxpPenX;
	 
	iFxpPenX = 0;
	wide = 0;
	
	use_kerning = FT_HAS_KERNING( face );
	previous    = chBefore;
	
	/* convert character code to glyph index */
	glyph_index = FT_Get_Char_Index( face, ch );
	
	/* retrieve kerning distance and move pen position */
	if ( use_kerning && previous && glyph_index )
	{
		FT_Vector  delta;
		 
		FT_Get_Kerning( face, previous, glyph_index,
						FT_KERNING_DEFAULT, &delta );
		 
		iFxpPenX += delta.x;
	}
	 
	/* load glyph image into the slot (erase previous one) */
	int error = FT_Load_Glyph( face, glyph_index, FT_LOAD_DEFAULT );
	if ( error )
	{
		fprintf( stderr, "Error in FL_Load_Glyph: %x\n", error );
	}
	 
	FT_GlyphSlot slot = face->glyph;
	iFxpPenX += slot->advance.x;
	 
	if ( FIXED6_2INT(iFxpPenX) > wide )
		wide = FIXED6_2INT(iFxpPenX);
		
	//$ NYI: finder.abc.abcA = abcA;
	//$ NYI: finder.abc.abcC = abcC;
	finder.abc.wide = wide;
	m_ExtendedKernedABCWidthsCache.Insert(finder);
}

//-----------------------------------------------------------------------------
// Purpose: gets the abc widths for a character
//-----------------------------------------------------------------------------
void CLinuxFont::GetCharABCWidths(int ch, int &a, int &b, int &c)
{
	Assert(IsValid());

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

	a = b = c = 0;

	FT_Error error = FT_Load_Char( face,ch, 0 ); 
	if ( error )
		return;

	FT_Glyph_Metrics metrics = face->glyph->metrics;

	finder.abc.a = metrics.horiBearingX/64 - m_iBlur - m_iOutlineSize;
	finder.abc.b = metrics.width/64 + ((m_iBlur + m_iOutlineSize) * 2) + m_iDropShadowOffset;
	finder.abc.c = (metrics.horiAdvance-metrics.horiBearingX-metrics.width)/64 - m_iBlur - m_iDropShadowOffset - m_iOutlineSize;

	m_ExtendedABCWidthsCache.Insert(finder);

	a = finder.abc.a;
	b = finder.abc.b;
	c = finder.abc.c;	
}
							   

//-----------------------------------------------------------------------------
// Purpose: returns true if the font is equivalent to that specified
//-----------------------------------------------------------------------------
bool CLinuxFont::IsEqualTo(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
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
bool CLinuxFont::IsValid()
{
	if ( !m_szName.IsEmpty() )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: returns the height of the font, in pixels
//-----------------------------------------------------------------------------
int CLinuxFont::GetHeight()
{
	assert(IsValid());
	return m_iHeight;
}

//-----------------------------------------------------------------------------
// Purpose: returns the ascent of the font, in pixels (ascent=units above the base line)
//-----------------------------------------------------------------------------
int CLinuxFont::GetAscent()
{
	assert(IsValid());
	return m_iAscent;
}

//-----------------------------------------------------------------------------
// Purpose: returns the maximum width of a character, in pixels
//-----------------------------------------------------------------------------
int CLinuxFont::GetMaxCharWidth()
{
	assert(IsValid());
	return m_iMaxCharWidth;
}

//-----------------------------------------------------------------------------
// Purpose: returns the flags used to make this font, used by the dynamic resizing code
//-----------------------------------------------------------------------------
int CLinuxFont::GetFlags()
{
	return m_iFlags;
}

//-----------------------------------------------------------------------------
// Purpose: Comparison function for abc widths storage
//-----------------------------------------------------------------------------
bool CLinuxFont::ExtendedABCWidthsCacheLessFunc(const abc_cache_t &lhs, const abc_cache_t &rhs)
{
	return lhs.wch < rhs.wch;
}

//-----------------------------------------------------------------------------
// Purpose: Comparison function for abc widths storage
//-----------------------------------------------------------------------------
bool CLinuxFont::ExtendedKernedABCWidthsCacheLessFunc(const kerned_abc_cache_t &lhs, const kerned_abc_cache_t &rhs)
{
	return lhs.wch < rhs.wch || ( lhs.wch == rhs.wch && lhs.wchBefore < rhs.wchBefore ) 
	|| ( lhs.wch == rhs.wch && lhs.wchBefore == rhs.wchBefore && lhs.wchAfter < rhs.wchAfter );
}

void *CLinuxFont::SetAsActiveFont( void *cglContext )
{
	Assert( false );
	return NULL;
}


#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void CLinuxFont::Validate( CValidator &validator, const char *pchName )
{
	validator.Push( "CLinuxFont", this, pchName );

	m_ExtendedABCWidthsCache.Validate( validator, "m_ExtendedABCWidthsCache" );
	m_ExtendedKernedABCWidthsCache.Validate( validator, "m_ExtendedKernedABCWidthsCache" );
	validator.ClaimMemory( m_pGaussianDistribution );

	validator.Pop();
}
#endif // DBGFLAG_VALIDATE
