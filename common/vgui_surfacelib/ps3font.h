//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:  console support for fonts
//
// $NoKeywords: $
//=============================================================================//

#ifndef PS3FONT_H
#define PS3FONT_H

#ifdef GetCharABCWidths
#undef GetCharABCWidths
#endif

#include "UtlRBTree.h"
#include "tier1/UtlSymbol.h"
#include "vguifont.h"

#if defined(_PS3)
	#if defined(HFONT)
	#error HFONT defined twice, which breaks the kooky typedef in ps3font.h
	#else
	typedef void * HPS3FONT ;
	#endif
#include <cell/fontFT.h>
#endif

struct CPS3FontMetrics : public CellFontHorizontalLayout
{
	float fMaxWidth; // must be initialized externally
	inline CPS3FontMetrics() : fMaxWidth(NAN) {};
};

// has some inline accessors to translate from PS3 members to XUI ones
struct CPS3CharMetrics : public CellFontGlyphMetrics
{
	inline int A() { return 0; }
	inline int B() { return ceilf( Horizontal.advance ); }
	inline int C() { return 0; }
};

//-----------------------------------------------------------------------------
// Purpose: encapsulates a windows font
//-----------------------------------------------------------------------------

class CPS3Font
{
public:
	CPS3Font();
	~CPS3Font();

	// creates the font from windows.  returns false if font does not exist in the OS.
	virtual bool Create(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags);

	// writes the char into the specified 32bpp texture
	virtual void GetCharsRGBA( newChar_t *newChars, int numNewChars, unsigned char *pRGBA );
	virtual void GetCharRGBA( wchar_t ch, int rgbaWide, int rgbaTall, unsigned char *pRGBA );

	// returns true if the font is equivalent to that specified
	virtual bool IsEqualTo(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags);

	// returns true only if this font is valid for use
	virtual bool IsValid();

	// gets the abc widths for a character
	virtual void GetCharABCWidths(int ch, int &a, int &b, int &c);
	virtual void GetKernedCharWidth( wchar_t ch, wchar_t chBefore, wchar_t chAfter, float &wide, float &abcA, float &abcC );

	// returns the height of the font, in pixels
	virtual int GetHeight();

	// returns the ascent of the font, in pixels (ascent=units above the base line)
	virtual int GetAscent();

	// returns the maximum width of a character, in pixels
	virtual int GetMaxCharWidth();

	// returns the flags used to make this font
	virtual int GetFlags();

	// returns true if this font is underlined
	bool GetUnderlined() { return m_bUnderlined; }

	// gets the name of this font
	const char *GetName() { return m_szName.String(); }
	
	// dump the system resource, if it's open
	void CloseResource();

	enum FontStyleFlags_t
	{
		kFONT_STYLE_NORMAL = 0,
		kFONT_STYLE_ITALIC = 2,
		kFONT_STYLE_UNDERLINE = 4,
		kFONT_STYLE_BOLD = 8,
	};

private:

#if !defined( _GAMECONSOLE )
	HFONT			m_hFont;
	HDC				m_hDC;
	HBITMAP			m_hDIB;
#elif defined ( _PS3 )
	HPS3FONT		m_hFont;
#elif defined( _X360 )
	HXUIFONT		m_hFont;
	HDC				m_hDC;
#endif

	// pointer to buffer for use when generated bitmap versions of a texture
	unsigned char	*m_pBuf;

protected:
	CUtlSymbol		m_szName;

	short			m_iTall;

	int				m_iWeight;

	unsigned short	m_iFlags;
	unsigned short	m_iScanLines;
	unsigned short	m_iBlur;
	unsigned short	m_rgiBitmapSize[2];

	unsigned int	m_iHeight : 8;
	unsigned int	m_iMaxCharWidth : 8;
	unsigned int	m_iAscent : 8;
	unsigned int	m_iDropShadowOffset : 1;
	unsigned int	m_iOutlineSize : 1;
	unsigned int	m_bAntiAliased : 1;
	unsigned int	m_bRotary : 1;
	unsigned int	m_bAdditive : 1;
	unsigned int	m_bUnderlined : 1; //30

private:
	// abc widths
	struct abc_t
	{
		short b;
		char a;
		char c;
	};

	// cache for additional or asian characters (since it's too big too just store them all)
	struct abc_cache_t
	{
		wchar_t wch;
		abc_t abc;
	};
	CUtlRBTree<abc_cache_t, unsigned short> m_ExtendedABCWidthsCache;
	static bool ExtendedABCWidthsCacheLessFunc(const abc_cache_t &lhs, const abc_cache_t &rhs);

	// First range of characters are automatically cached
#if defined( _PS3 )
	enum { ABCWIDTHS_CACHE_SIZE = 256 }; // was 128
	abc_t m_ABCWidthsCache[ABCWIDTHS_CACHE_SIZE];
#elif defined( _X360 )
	// 360 requires all possible characters during font init
	enum { ABCWIDTHS_CACHE_SIZE = 256 };
	abc_t m_ABCWidthsCache[ABCWIDTHS_CACHE_SIZE];
#endif
};

#endif // PS3FONT_H
