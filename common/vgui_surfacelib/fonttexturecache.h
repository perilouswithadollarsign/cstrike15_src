//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef FONTTEXTURECACHE_H
#define FONTTEXTURECACHE_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_surfacelib/fontmanager.h"
#include "utlrbtree.h"
#include "utlmap.h"
#include "bitmap/texturepacker.h"

class ITexture;

#define MAX_COMMON_CHARS	256

//-----------------------------------------------------------------------------
// Purpose: manages texture memory for unicode fonts
//-----------------------------------------------------------------------------
class CFontTextureCache
{
public:
	CFontTextureCache();
	~CFontTextureCache();

	void SetPrefix( const char *pTexturePagePrefix );

	// returns a texture ID and a pointer to an array of 4 texture coords for the given character & font
	// generates+uploads more texture if necessary
	bool GetTextureForChar( FontHandle_t font, FontDrawType_t type, wchar_t wch, int *textureID, float **texCoords );
	// This function copies the texcoords out from the static into a preallocated passed in arg. 
	bool GetTextureAndCoordsForChar( FontHandle_t font, FontDrawType_t type, wchar_t wch, int *textureID, float *texCoords );

	// for each character in an array (not assumed to be a NULL-terminated string), returns a
	// texture ID and a pointer to an array of 4 texture coords for the given character & font
	// generates+uploads more texture if necessary
	bool GetTextureForChars( FontHandle_t font, FontDrawType_t type, wchar_t *wch, int *textureID, float **texCoords, int numChars = 1 );

	// clears the cache
	void Clear();

private:

	ITexture *AllocateNewPage( char *pTextureName );

	// hold the common characters
	struct charDetail_t
	{
		int page;
		float texCoords[4];
	};
	struct CommonChar_t
	{
		charDetail_t	details[MAX_COMMON_CHARS];
	};

	// a single character in the cache
	typedef unsigned short HCacheEntry;
	struct CacheEntry_t
	{
		FontHandle_t font;
		wchar_t wch;
		short pad;
		int page;
		float texCoords[4];
	};
	
	struct CacheMapEntry_t
	{
		Rect_t rc;
		bool bInUse;
	};

	// a single texture page
	struct Page_t
	{
	public:
		Page_t()
		{
			pPackedFontTextureCache = NULL;
		}

		short textureID[FONT_DRAW_TYPE_COUNT];
		CTexturePacker *pPackedFontTextureCache;	 // the character mapping cache to use for this page.
	};

	// allocates a new page for a given character
	bool AllocatePageForChar(int charWide, int charTall, int &pageIndex, int &drawX, int &drawY, int &twide, int &ttall);

	// Creates font materials
	void CreateFontMaterials( Page_t &page, ITexture *pFontTexture, bool bitmapFont = false );

	CommonChar_t *m_CommonCharCache[384];

	static bool CacheEntryLessFunc(const CacheEntry_t &lhs, const CacheEntry_t &rhs);
	CUtlRBTree< CacheEntry_t, HCacheEntry >	m_CharCache;

	// cache
	typedef CUtlVector<Page_t> FontPageList_t;
	FontPageList_t m_PageList;
	
	int	m_CurrPage;
	CUtlMap< FontHandle_t, Page_t >	m_FontPages;

	// Prefix to use when this cache creates font pages
	CUtlString m_TexturePagePrefix;
};


#endif // FONTTEXTURECACHE_H


