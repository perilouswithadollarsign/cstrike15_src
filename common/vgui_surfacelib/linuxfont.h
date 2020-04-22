//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef LINUXFONT_H
#define LINUXFONT_H

#include "utlrbtree.h"
#include <tier0/memdbgoff.h>
#include <tier0/memdbgon.h>
#include "tier1/strtools.h"
#include "tier1/utlstring.h"

#include <ft2build.h>
#include FT_FREETYPE_H


//-----------------------------------------------------------------------------
// Purpose: encapsulates a OSX font
//-----------------------------------------------------------------------------
class CLinuxFont
{
public:
	CLinuxFont();
	~CLinuxFont();

	// creates the font from windows.  returns false if font does not exist in the OS.
	virtual bool Create(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags);

	// writes the char into the specified 32bpp texture
	virtual void GetCharRGBA( wchar_t ch, int rgbaWide, int rgbaTall, unsigned char *rgba);

	// returns true if the font is equivalent to that specified
	virtual bool IsEqualTo(const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags);

	// returns true only if this font is valid for use
	virtual bool IsValid();

	// gets the abc widths for a character
	virtual void GetCharABCWidths(int ch, int &a, int &b, int &c);

	// set the font to be the one to currently draw with in the gdi
	void *SetAsActiveFont( void *glContext );

	// returns the height of the font, in pixels
	virtual int GetHeight();

	// returns the ascent of the font, in pixels (ascent=units above the base line)
	virtual int GetAscent();

	// returns the maximum width of a character, in pixels
	virtual int GetMaxCharWidth();

	// returns the flags used to make this font
	virtual int GetFlags();

	// returns true if this font is underlined
	virtual bool GetUnderlined() { return m_bUnderlined; }
	
	// gets the name of this font
	const char *GetName() { return m_szName.String(); }

	// gets the weight of the font
	virtual int GetWeight() { return m_iWeight; }

	// gets the width of ch given its position around before and after chars
	virtual void GetKernedCharWidth( wchar_t ch, wchar_t chBefore, wchar_t chAfter, float &wide, float &abcA, float &abcC );

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, char *pchName );
#endif

	virtual bool CreateFromMemory(const char *windowsFontName, void *data, int size, int tall, int weight, int blur, int scanlines, int flags);


protected:
        CUtlString m_szName;
        int m_iTall;
        int m_iWeight;
        int m_iFlags;
        bool m_bAntiAliased;
        bool m_bRotary;
        bool m_bAdditive;
        int m_iDropShadowOffset;
        bool m_bUnderlined;
        int m_iOutlineSize;

        int m_iHeight;
        int m_iMaxCharWidth;
        int m_iAscent;

        int m_iScanLines;
        int m_iBlur;
        float *m_pGaussianDistribution;
        
private:
	void InitMetrics();



	static void CreateFontList();
        // abc widths
        struct abc_t
        {
                short b;
                char a;
                char c;
        };

        // cache for storing asian abc widths (since it's too big too just store them all)
        struct abc_cache_t
        {
                wchar_t wch;
                abc_t abc;
        };

	
	CUtlRBTree<abc_cache_t, unsigned short> m_ExtendedABCWidthsCache;
	static bool ExtendedABCWidthsCacheLessFunc(const abc_cache_t &lhs, const abc_cache_t &rhs);

	// cache for storing asian abc widths (since it's too big too just store them all)
	struct kernedSize
	{
		float wide;
		// float abcA; //$ Not yet used...
		// float abcC;	// 
	};
	
	struct kerned_abc_cache_t
	{
		wchar_t wch;
		wchar_t wchBefore;
		wchar_t wchAfter;
		kernedSize abc;
	};
	
	CUtlRBTree<kerned_abc_cache_t, unsigned short> m_ExtendedKernedABCWidthsCache;
	static bool ExtendedKernedABCWidthsCacheLessFunc(const kerned_abc_cache_t &lhs, const kerned_abc_cache_t &rhs);

	
	int m_rgiBitmapSize[2];
	unsigned char *m_pBuf;	// pointer to buffer for use when generated bitmap versions of a texture
	
	FT_Face face;

	struct font_name_entry
	{
		font_name_entry()
		{
			m_pchFile = NULL;
			m_pchFriendlyName = NULL;
		}
		
		char *m_pchFile;
		char *m_pchFriendlyName;
		bool operator<( const font_name_entry &rhs ) const
		{
			return V_stricmp( rhs.m_pchFriendlyName, m_pchFriendlyName ) > 0;
		}
	};
	static CUtlRBTree< font_name_entry > m_FriendlyNameCache;
	static bool ms_bSetFriendlyNameCacheLessFunc;
};

#endif // OSXFONT_H
