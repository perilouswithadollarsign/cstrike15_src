//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#if !defined( _GAMECONSOLE ) && defined( _WIN32 )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "vgui_surfacelib/fonttexturecache.h"
#include "tier1/keyvalues.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterial.h"
#include "tier1/utlbuffer.h"
#include "fmtstr.h"
#include "vgui_surfacelib/texturedictionary.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define TEXTURE_PAGE_WIDTH	256
#define TEXTURE_PAGE_HEIGHT	256


ConVar vgui_show_glyph_miss( "vgui_show_glyph_miss", "0", FCVAR_DEVELOPMENTONLY );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFontTextureCache::CFontTextureCache() : m_CharCache( 0, 256, CacheEntryLessFunc )
{
	V_memset( m_CommonCharCache, 0, sizeof( m_CommonCharCache ) );
	Clear();
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFontTextureCache::~CFontTextureCache()
{
	Clear();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFontTextureCache::SetPrefix( const char *pTexturePagePrefix )
{
	m_TexturePagePrefix = pTexturePagePrefix;
}


//-----------------------------------------------------------------------------
// Purpose: Resets the cache
//-----------------------------------------------------------------------------
void CFontTextureCache::Clear()
{
	// remove all existing data
	m_CharCache.RemoveAll();

	for ( int i = 0; i < m_PageList.Count(); ++i )
	{
		if ( m_PageList[i].pPackedFontTextureCache )
		{
			delete m_PageList[i].pPackedFontTextureCache;
		}
	}
	m_PageList.RemoveAll();

	m_CurrPage = -1;
	
	m_FontPages.RemoveAll();
	m_FontPages.SetLessFunc( DefLessFunc( FontHandle_t ) );

	for ( int i = 0; i < ARRAYSIZE( m_CommonCharCache ); i++ )
	{
		delete m_CommonCharCache[i];
		m_CommonCharCache[i] = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: comparison function for cache entries
//-----------------------------------------------------------------------------
bool CFontTextureCache::CacheEntryLessFunc( CacheEntry_t const &lhs, CacheEntry_t const &rhs )
{
	uint64 lhsLookupID = ( ((uint64)lhs.font) << 32 ) | ((uint64)lhs.wch);
	uint64 rhsLookupID = ( ((uint64)rhs.font) << 32 ) | ((uint64)rhs.wch);

	return lhsLookupID < rhsLookupID;	
}

//-----------------------------------------------------------------------------
// Purpose: returns the texture info for the given char & font
//-----------------------------------------------------------------------------
bool CFontTextureCache::GetTextureForChar( FontHandle_t font, FontDrawType_t type, wchar_t wch, int *textureID, float **texCoords )
{
	// Ask for just one character
	return GetTextureForChars( font, type, &wch, textureID, texCoords, 1 );
}

//-----------------------------------------------------------------------------
// Purpose: returns the texture info for the given char & font
// This function copies in the texcoords out from the static into a preallocated passed in arg.
//-----------------------------------------------------------------------------
bool CFontTextureCache::GetTextureAndCoordsForChar( FontHandle_t font, FontDrawType_t type, wchar_t wch, int *textureID, float *texCoords )
{
	// Ask for just one character
	float *textureCoords = NULL;
	bool bSuccess = GetTextureForChars( font, type, &wch, textureID, &textureCoords, 1 );
	if ( textureCoords )
	{
		texCoords[0] = textureCoords[0];
		texCoords[1] = textureCoords[1];
		texCoords[2] = textureCoords[2];
		texCoords[3] = textureCoords[3];
	}

	return bSuccess;
}

//-----------------------------------------------------------------------------
// Purpose: returns the texture info for the given chars & font
//-----------------------------------------------------------------------------
bool CFontTextureCache::GetTextureForChars( FontHandle_t hFont, FontDrawType_t type, wchar_t *wch, int *textureID, float **texCoords, int numChars )
{
	Assert( wch && textureID && texCoords );
	Assert( numChars >= 1 );

	if ( type == FONT_DRAW_DEFAULT )
	{
		type = FontManager().IsFontAdditive( hFont ) ? FONT_DRAW_ADDITIVE : FONT_DRAW_NONADDITIVE;
	}

	int typePage = (int)type - 1;
	typePage = clamp( typePage, 0, (int)FONT_DRAW_TYPE_COUNT - 1 );

	if ( FontManager().IsBitmapFont( hFont ) )
	{
		const int MAX_BITMAP_CHARS = 256;
		if ( numChars > MAX_BITMAP_CHARS )
		{
			// Increase MAX_BITMAP_CHARS
			Assert( 0 );
			return false;
		}
	
		for ( int i = 0; i < numChars; i++ )
		{
			static float	sTexCoords[ 4*MAX_BITMAP_CHARS ];
			CBitmapFont		*pWinFont;
			float			left, top, right, bottom;
			int				index;
			Page_t			*pPage;
			
			pWinFont = reinterpret_cast< CBitmapFont* >( FontManager().GetFontForChar( hFont, wch[i] ) );
			if ( !pWinFont )
			{
				// bad font handle
				return false;
			}

			// get the texture coords
			pWinFont->GetCharCoords( wch[i], &left, &top, &right, &bottom );
			sTexCoords[i*4 + 0] = left;
			sTexCoords[i*4 + 1] = top;
			sTexCoords[i*4 + 2] = right;
			sTexCoords[i*4 + 3] = bottom;

			// find font handle in our list of ready pages
			index = m_FontPages.Find( hFont );
			if ( index == m_FontPages.InvalidIndex() )
			{
				// not found, create the texture id and its materials
				index = m_FontPages.Insert( hFont );
				pPage = &m_FontPages.Element( index );

				for (int type = 0; type < FONT_DRAW_TYPE_COUNT; ++type )
				{
					pPage->textureID[type] = TextureDictionary()->CreateTexture( false );
				}
				CreateFontMaterials( *pPage, pWinFont->GetTexturePage(), true );
			}

			texCoords[i] = &(sTexCoords[ i*4 ]);
			textureID[i] = m_FontPages.Element( index ).textureID[typePage];
		}
	}
	else
	{
		font_t *pWinFont = FontManager().GetFontForChar( hFont, wch[0] );
		if ( !pWinFont )
		{
			return false;
		}

		struct newPageEntry_t
		{
			int	page;	// The font page a new character will go in
			int	drawX;	// X location within the font page
			int	drawY;	// Y location within the font page
		};
		
		// Determine how many characters need to have their texture generated
		newChar_t *newChars	= (newChar_t *)stackalloc( numChars*sizeof( newChar_t ) );
		newPageEntry_t *newEntries = (newPageEntry_t *)stackalloc( numChars*sizeof( newPageEntry_t ) );
		int numNewChars = 0;
		int maxNewCharTexels = 0;
		int totalNewCharTexels = 0;
		
		for ( int i = 0; i < numChars; i++ )
		{
			wchar_t wideChar = wch[i];

			int *pCachePage;
			float *pCacheCoords;

			// profiling dicatated that avoiding the naive font/char RB lookup was beneficial
			// instead waste a little memory to get all the western language chars to be direct
			if ( IsGameConsole() && wideChar < MAX_COMMON_CHARS && hFont < ARRAYSIZE( m_CommonCharCache ) )
			{
				// dominant amount of simple chars are instant direct lookup
				CommonChar_t *pCommonChars = m_CommonCharCache[hFont];
				if ( !pCommonChars )
				{
					// missing
					if ( pWinFont != FontManager().GetFontForChar( hFont, wideChar ) )
					{
						// all characters in string must come out of the same font
						return false;
					}
					
					// init and insert
					pCommonChars = new CommonChar_t;
					memset( pCommonChars, 0, sizeof( CommonChar_t ) );
					m_CommonCharCache[hFont] = pCommonChars;
				}
				pCachePage = &pCommonChars->details[wideChar].page;
				pCacheCoords = pCommonChars->details[wideChar].texCoords;
			}
			else
			{
				// for console only, either more fonts than expected (> 256 fonts!) or not a simple integer
				// want to keep this a direct lookup and not a search (which defeats the perf gain)
				AssertMsgOnce( !IsGameConsole() || hFont < ARRAYSIZE( m_CommonCharCache ), "CFontTextureCache: Unexpected hFont out-of-range\n" );
	
				// extended chars are a costlier lookup
				// page and char form a unique key to find in cache
				CacheEntry_t cacheItem;
				cacheItem.font = hFont;
				cacheItem.wch = wideChar;
				HCacheEntry cacheHandle = m_CharCache.Find( cacheItem );
				if ( !m_CharCache.IsValidIndex( cacheHandle ) )
				{
					// missing
					if ( pWinFont != FontManager().GetFontForChar( hFont, wideChar ) )
					{
						// all characters in string must come out of the same font
						return false;
					}

					// init and insert
					cacheItem.texCoords[0] = 0;
					cacheItem.texCoords[1] = 0;
					cacheItem.texCoords[2] = 0;
					cacheItem.texCoords[3] = 0;
					cacheHandle = m_CharCache.Insert( cacheItem );
					Assert( m_CharCache.IsValidIndex( cacheHandle ) );
				}
				pCachePage = &m_CharCache[cacheHandle].page;
				pCacheCoords = m_CharCache[cacheHandle].texCoords;
			}

			if ( pCacheCoords[2] == 0 && pCacheCoords[3] == 0 )
			{
				// invalid page, setup for page allocation
				// get the char details
				int a, b, c;
				pWinFont->GetCharABCWidths( wideChar, a, b, c );
				int fontWide = MAX( b, 1 );
				int fontTall = MAX( pWinFont->GetHeight(), 1 );
				if ( pWinFont->GetUnderlined() )
				{
					fontWide += ( a + c );
				}

				// Get a texture to render into
				int page, drawX, drawY, twide, ttall;
				if ( !AllocatePageForChar( fontWide, fontTall, page, drawX, drawY, twide, ttall ) )
				{
					return false;
				}

				// accumulate data to pass to GetCharsRGBA below
				newEntries[numNewChars].page	= page;
				newEntries[numNewChars].drawX	= drawX;
				newEntries[numNewChars].drawY	= drawY;
				newChars[numNewChars].wch		= wideChar;
				newChars[numNewChars].fontWide	= fontWide;
				newChars[numNewChars].fontTall	= fontTall;
				newChars[numNewChars].offset	= 4*totalNewCharTexels;
				totalNewCharTexels += fontWide*fontTall;
				maxNewCharTexels = MAX( maxNewCharTexels, fontWide*fontTall );
				numNewChars++;

				// the 0.5 texel offset is done in CMatSystemTexture::SetMaterial()
				pCacheCoords[0] = (float)( (double)drawX / ((double)twide) );
				pCacheCoords[1] = (float)( (double)drawY / ((double)ttall) );
				pCacheCoords[2] = (float)( (double)(drawX + fontWide) / (double)twide );
				pCacheCoords[3] = (float)( (double)(drawY + fontTall) / (double)ttall );
			
				*pCachePage = page;
			}
			
			// give data to caller
			textureID[i] = m_PageList[*pCachePage].textureID[typePage];
			texCoords[i] = pCacheCoords;
		}

		// Generate texture data for all newly-encountered characters
		if ( numNewChars > 0 )
		{
			if ( vgui_show_glyph_miss.GetBool() )
			{
				char *pMissString = (char *)stackalloc( numNewChars * sizeof( char ) );
				char *pString = pMissString;
				for ( int i = 0; i < numNewChars; i++ )
				{
					// build a string representative enough for debugging puproses
					wchar_t	wch = newChars[i].wch;
					if ( V_isprint( wch ) )
					{
						*pString++ = (char)wch;
					}
					else
					{
						*pString++ = '?';
					}
				}
				*pString = '\0';
				
				const char *pMsg = CFmtStr( "Glyph Miss: FontHandle_t:0x%8.8x (%s), %s (0x%x)\n", (int)hFont, pWinFont->GetName(), pMissString, pMissString[0] );
				if ( IsGameConsole() )
				{
					// valid on xbox, and really want this spew treated like console spew
					Warning( "%s", pMsg );
				}
				else
				{
					// debugger output only, to prevent any reentrant glyph miss as a result of spewing
					Plat_DebugString( pMsg );
				}
			}

			if ( IsGameConsole() && numNewChars > 1 )
			{
				MEM_ALLOC_CREDIT();

				// Use the 360 fast path that generates multiple characters at once
				int newCharDataSize = totalNewCharTexels*4;
				CUtlBuffer newCharData( 0, newCharDataSize, CUtlBuffer::READ_ONLY );
				unsigned char *pRGBA = (unsigned char *)newCharData.Base();
#if defined( _X360 ) || defined( _PS3 )
				pWinFont->GetCharsRGBA( newChars, numNewChars, pRGBA );
#endif
				// Copy the data into our font pages
				for ( int i = 0; i < numNewChars; i++ )
				{
					newChar_t		&newChar = newChars[i];
					newPageEntry_t	&newEntry = newEntries[i];

					// upload the new sub texture 
					// NOTE: both textureIDs reference the same ITexture, so we're ok
					unsigned char *characterRGBA = pRGBA + newChar.offset;
					TextureDictionary()->SetSubTextureRGBA( m_PageList[newEntry.page].textureID[typePage], newEntry.drawX, newEntry.drawY, characterRGBA, newChar.fontWide, newChar.fontTall );
				}
			}
			else
			{
				// create a buffer for new characters to be rendered into
				int nByteCount = maxNewCharTexels * 4;
				unsigned char *pRGBA = (unsigned char *)stackalloc( nByteCount * sizeof( unsigned char ) );

				// Generate characters individually
				for ( int i = 0; i < numNewChars; i++ )
				{
					newChar_t		&newChar	= newChars[i];
					newPageEntry_t	&newEntry	= newEntries[i];

					// render the character into the buffer
					Q_memset( pRGBA, 0, nByteCount );
					pWinFont->GetCharRGBA( newChar.wch, newChar.fontWide, newChar.fontTall, pRGBA );

					// Make the char white if we are in source 2
					if ( !g_pMaterialSystem )
					{
						for ( int i = 0; i < nByteCount; i += 4 )
						{
							pRGBA[i+0] = pRGBA[i+1] = pRGBA[i+2] = 255;
						}
					}

					// upload the new sub texture 
					// NOTE: both textureIDs reference the same ITexture, so we're ok
					TextureDictionary()->SetSubTextureRGBA( m_PageList[newEntry.page].textureID[typePage], newEntry.drawX, newEntry.drawY, pRGBA, newChar.fontWide, newChar.fontTall );
				}
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Creates font materials
//-----------------------------------------------------------------------------
void CFontTextureCache::CreateFontMaterials( Page_t &page, ITexture *pFontTexture, bool bitmapFont )
{
	// The normal material
	KeyValues *pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetInt( "$vertexcolor", 1 );
	pVMTKeyValues->SetInt( "$vertexalpha", 1 );
	pVMTKeyValues->SetInt( "$ignorez", 1 );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$translucent", 1 );
	pVMTKeyValues->SetString( "$basetexture", pFontTexture->GetName() );
	CUtlString materialName = m_TexturePagePrefix + "__fontpage";
	Assert( g_pMaterialSystem );
	IMaterial *pMaterial = g_pMaterialSystem->CreateMaterial( materialName, pVMTKeyValues );
	pMaterial->Refresh();

	int typePageNonAdditive = (int)FONT_DRAW_NONADDITIVE-1;
	TextureDictionary()->BindTextureToMaterial( page.textureID[typePageNonAdditive], pMaterial );
	pMaterial->DecrementReferenceCount();

	// The additive material
	pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	pVMTKeyValues->SetInt( "$vertexcolor", 1 );
	pVMTKeyValues->SetInt( "$vertexalpha", 1 );
	pVMTKeyValues->SetInt( "$ignorez", 1 );
	pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	pVMTKeyValues->SetInt( "$translucent", 1 );
	pVMTKeyValues->SetInt( "$additive", 1 );
	pVMTKeyValues->SetString( "$basetexture", pFontTexture->GetName() );

	CUtlString addmaterialName = m_TexturePagePrefix + "__fontpage_additive";
	pMaterial = g_pMaterialSystem->CreateMaterial( addmaterialName.String(), pVMTKeyValues );
 	pMaterial->Refresh();

	int typePageAdditive = (int)FONT_DRAW_ADDITIVE-1;
	if ( bitmapFont )
	{
		TextureDictionary()->BindTextureToMaterial( page.textureID[typePageAdditive], pMaterial );
	}
	else
	{
		TextureDictionary()->BindTextureToMaterialReference( page.textureID[typePageAdditive], page.textureID[typePageNonAdditive], pMaterial);
	}
	pMaterial->DecrementReferenceCount();
}

//-----------------------------------------------------------------------------
// Purpose: allocates a new page for a given character
//-----------------------------------------------------------------------------
bool CFontTextureCache::AllocatePageForChar( int charWide, int charTall, int &pageIndex, int &drawX, int &drawY, int &twide, int &ttall )
{
	// Catch the case where the glyph is too tall for the page
	if ( charTall > TEXTURE_PAGE_HEIGHT )
		return false;

	// See if there is room in the last page for this character
	pageIndex = m_CurrPage;

	bool bNeedsNewPage = true;
	int nodeIndex = -1;
	Rect_t glpyhRect;
	glpyhRect.x = 0;
	glpyhRect.y = 0;
	glpyhRect.width = charWide;
	glpyhRect.height = charTall; 

	if ( pageIndex > -1 )
	{
		// Let's use r/b tree to find a good spot.	
		nodeIndex = m_PageList[pageIndex].pPackedFontTextureCache->InsertRect( glpyhRect );
		bNeedsNewPage = ( nodeIndex == -1 );
	}
	
	if ( bNeedsNewPage )
	{
		// allocate a new page
		pageIndex = m_PageList.AddToTail();
		Page_t &newPage = m_PageList[pageIndex];
		m_CurrPage = pageIndex;

		for (int i = 0; i < FONT_DRAW_TYPE_COUNT; ++i )
		{
			newPage.textureID[i] = TextureDictionary()->CreateTexture( true );
		}
		newPage.pPackedFontTextureCache = new CTexturePacker( TEXTURE_PAGE_WIDTH, TEXTURE_PAGE_HEIGHT, ( IsGameConsole() ? 0 : 1 ) );

		nodeIndex = newPage.pPackedFontTextureCache->InsertRect( glpyhRect );
		Assert( nodeIndex != -1 );


		static int nFontPageId = 0;
		char pTextureName[64];
		Q_snprintf( pTextureName, 64, "%s__font_page_%d", m_TexturePagePrefix.String(), nFontPageId );
		++nFontPageId;

		MEM_ALLOC_CREDIT();
		if ( g_pMaterialSystem )
		{
			ITexture *pTexture = AllocateNewPage( pTextureName );
			CreateFontMaterials( newPage, pTexture );
			pTexture->DecrementReferenceCount();
		}

		if ( IsPC() || !IsDebug() )
		{
			// clear the texture from the inital checkerboard to black
			// allocate for 32bpp format
			int nByteCount = TEXTURE_PAGE_WIDTH * TEXTURE_PAGE_HEIGHT * 4;
			CUtlMemory<unsigned char> mRGBA;
			mRGBA.EnsureCapacity( nByteCount );

			//Q_memset( mRGBA.Base(), 0, nByteCount );

			// Clear to white, full alpha.
			for ( int i = 0; i < nByteCount; i += 4 )
			{
				mRGBA[i+0] = mRGBA[i+1] = mRGBA[i+2] = 255;
				mRGBA[i+3] = 0;
			}

			int typePageNonAdditive = (int)(FONT_DRAW_NONADDITIVE)-1;
			TextureDictionary()->SetTextureRGBAEx( newPage.textureID[typePageNonAdditive], ( const char* )mRGBA.Base(), 
				TEXTURE_PAGE_WIDTH, TEXTURE_PAGE_HEIGHT, IMAGE_FORMAT_RGBA8888, k_ETextureScalingPointSample );

			
			// Note, in rendersystem2 we do not have materials, as we actually have 2 diff textures.
			if ( !g_pMaterialSystem )
			{
				int typePageAdditive = (int)(FONT_DRAW_ADDITIVE)-1;
				newPage.textureID[typePageAdditive] = newPage.textureID[typePageNonAdditive];
			}
		}
	}

	// output the position
	Page_t &page = m_PageList[ pageIndex ];
	Assert( nodeIndex != -1 );
	const CTexturePacker::TreeEntry_t &newEntry = page.pPackedFontTextureCache->GetEntry( nodeIndex );
	drawX = newEntry.rc.x;
	drawY = newEntry.rc.y;
	twide = TEXTURE_PAGE_WIDTH;
	ttall = TEXTURE_PAGE_HEIGHT;
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: allocates a new page 
//-----------------------------------------------------------------------------
ITexture *CFontTextureCache::AllocateNewPage( char *pTextureName )
{
	Assert( g_pMaterialSystem );
	ITexture *pTexture = g_pMaterialSystem->CreateProceduralTexture( 
	pTextureName, 
	TEXTURE_GROUP_VGUI, 
	TEXTURE_PAGE_WIDTH, 
	TEXTURE_PAGE_HEIGHT, 
	IMAGE_FORMAT_RGBA8888, 
	TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | 
	TEXTUREFLAGS_NOMIP | TEXTUREFLAGS_NOLOD | TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_SINGLECOPY );

	return pTexture;
}

