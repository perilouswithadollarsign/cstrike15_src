//===== Copyright (c) Valve Corporation, All rights reserved. ======//
//
// Purpose: PS3-specific font code that for historical reasons is hidden 
//          in the materialsystem.
//
//==================================================================//

#include "pch_materialsystem.h"

#ifndef _PS3
#define MATSYS_INTERNAL
#endif

#include "cmaterialsystem.h"

#include "colorspace.h"
#include "materialsystem/materialsystem_config.h"
#include "materialsystem/imaterialproxyfactory.h"
#include "IHardwareConfigInternal.h"
#include "shadersystem.h"
#include "texturemanager.h"
#include "shaderlib/ShaderDLL.h"
#include "tier1/callqueue.h"
#include "vstdlib/jobthread.h"
#include "cmatnullrendercontext.h"
#include "datacache/iresourceaccesscontrol.h"
#include "filesystem/IQueuedLoader.h"
#include "filesystem/IXboxInstaller.h"
#include "cdll_int.h"

#include <cell/sysmodule.h>
#include <cell/font.h>
#include <cell/fontft.h>
#include "../common/vgui_surfacelib/vguifont.h"

#ifdef _PS3
#include "ps3_pathinfo.h"
#endif

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"

static const CellFontLibrary *Fontlib();

// in case I make HPS3FONT something other than just a pointer
static inline CellFont *CellFontFromHandle( HPS3FONT &h ) 
{
	return (CellFont *)h;
}


wchar_t g_wchFallbackGlyph = L'_';

static inline bool ShouldFallBackToSystemFontForWChar( wchar_t wch, int &nFontTallAdjustment )
{
	if ( ( wch == 0x00AE /*(R)*/ ) || ( wch == 0x2122 /*(TM)*/ ) )
	{
		nFontTallAdjustment = 1;
		return true;
	}
	return false;
}

#define TEXTURE_PAGE_HEIGHT	128 // copied from FontTextureCache.cpp, which for some reason hardcodes this. because there's multiple implementations 
// of the font cache strewn throughout various libraries all linked into the vgui dll, I didn't feel safe changing
// it to just use larger textures.


class CFallbackFont: public CellFont
{
protected:
	int m_nInitError;
	int m_nTall;
	CellFontHorizontalLayout m_hlayout;
public:
	CFallbackFont(): m_nInitError(-1), m_nTall( -1 )
	{

	}

	void Init()
	{
		if( m_nInitError == CELL_OK )
		{
			// already initialized
			return ;
		}
		CellFontType fontType;
		fontType.type = CELL_FONT_TYPE_GOTHIC_JAPANESE_CJK_LATIN_SET;
		fontType.map = CELL_FONT_MAP_UNICODE;
		m_nInitError = cellFontOpenFontset( Fontlib(), &fontType, this );
		if( m_nInitError == CELL_OK )
		{
			Warning("Cannot open fallback font, error 0x%X\n", m_nInitError );
		}
	}

	CellFontHorizontalLayout *GetHorizontalLayout()
	{
		return &m_hlayout;
	}

	int SetTall( int nTall )
	{
		if( m_nInitError != CELL_OK )
		{
			return m_nInitError;
		}

		if( nTall == m_nTall )
		{
			return CELL_OK;
		}

		float pointSize = nTall * 0.79f;

		cellFontSetScalePixel( this, pointSize, pointSize );
		// sometimes we ask for bigger fonts than the packer can handle. Also, because of accents etc, the font may poke out of the requested tall value.
		// The packer is for some reason hardcoded to a maximum height of 128.
		cellFontGetHorizontalLayout( this, &m_hlayout );
		if ( m_hlayout.lineHeight + m_hlayout.effectHeight > TEXTURE_PAGE_HEIGHT )
		{
			Warning("FAILSAFE: Fallback Font at '%d' tall is actually %d pixels high, but max texture page is %d. Shrinking to ", 
				nTall, (int)(m_hlayout.lineHeight + m_hlayout.effectHeight), TEXTURE_PAGE_HEIGHT );

			do
			{
				pointSize *= 0.9;
				cellFontSetScalePixel( this, nTall, nTall );
				cellFontGetHorizontalLayout( this, &m_hlayout );
			}
			while ( ( m_hlayout.lineHeight + m_hlayout.effectHeight > TEXTURE_PAGE_HEIGHT ) && ( pointSize >= 1 ) );

			Warning( "%.2f\n", pointSize );
		}
		m_nTall = nTall;
		return CELL_OK;
	}


	void Shutdown()
	{
		if( m_nInitError == CELL_OK )
		{
			int nError = cellFontCloseFont( this );
			if( nError != CELL_OK )
			{
				Warning("Cannot close fallback font, error 0x%X\n", nError );
			}
		}
	}
};

CFallbackFont g_fallbackFont;


void CMaterialSystem::InitializePS3Fonts()
{
	m_vExtantFonts.EnsureCapacity(8);
}



class CTempFontHandle
{
public:
	CMaterialSystem * m_pMaterialSystem;
	MaterialLock_t m_hLock;
	CellFont * m_pNewFont;
public:
	CTempFontHandle( CMaterialSystem * pMaterialSystem):
		m_pMaterialSystem( pMaterialSystem )
	{
		m_hLock = pMaterialSystem->Lock();
		m_pNewFont = new CellFont;
	}
	CellFont* DetachFont()
	{
		CellFont * pFont = m_pNewFont;
		m_pNewFont = NULL;
		return pFont;
	}
	~CTempFontHandle()
	{
		if( m_pNewFont )
		{
			delete m_pNewFont;
		}
		m_pMaterialSystem->Unlock( m_hLock );
	}
};




HPS3FONT CMaterialSystem::OpenTrueTypeFont( const char *pFontname, int tall, int style )
{
	CTempFontHandle hTemp( this );

	static int sUniquePositiveNumber = 0;

	// need absolute disk path, look in resource then platform
	char fontpath[MAX_PATH] = {0};
	
	struct fontTable_t
	{
		const char	*pFontName;
		const char	*pPath;
		bool		m_bRestrictiveLoadIntoMemory;
		bool		m_bAlwaysLoadIntoMemory;
	};

	int nCellError;
	if( !V_stricmp( pFontname, "system" ) )
	{
		CellFontType fontType;
		fontType.type = CELL_FONT_TYPE_GOTHIC_JAPANESE_CJK_LATIN_SET;
		fontType.map = CELL_FONT_MAP_UNICODE;
		nCellError = cellFontOpenFontset( Fontlib(), &fontType, hTemp.m_pNewFont );
	}
	else if( !V_stricmp( pFontname, "japanese cjk latin" ) )
	{
		CellFontType fontType;
		fontType.type = CELL_FONT_TYPE_GOTHIC_JAPANESE_CJK_LATIN_SET;
		fontType.map = CELL_FONT_MAP_UNICODE;
		nCellError = cellFontOpenFontset( Fontlib(), &fontType, hTemp.m_pNewFont );
	}
	else
	{

		// explicit mapping required, dvd searching too expensive
		static fontTable_t fontToFilename[] = 
		{
	#include "shaderapidx9/fontremaptable.h"
		};

		// remap typeface to diskname
		char const *pDiskname = NULL;
		for ( int i=0; i<ARRAYSIZE( fontToFilename ); i++ )
		{
			if ( !V_stricmp( pFontname, fontToFilename[i].pFontName ) )
			{
				pDiskname = fontToFilename[i].pPath;
				break;
			}
		}
		if ( !pDiskname )
		{
			// not found
			DevWarning( "PS3: True Type Font: '%s' unknown.\n", pFontname );
		}
		else
		{
			Q_snprintf( fontpath, ARRAYSIZE( fontpath ), "%s/%s", g_pPS3PathInfo->GameImagePath(), pDiskname );
		}

		// if we failed to find the font, substitute a system font
		if ( fontpath[0] == 0 )
		{
			CellFontType fontType;
			fontType.type = CELL_FONT_TYPE_GOTHIC_JAPANESE_CJK_LATIN_SET;
			fontType.map = CELL_FONT_MAP_UNICODE;
			nCellError = cellFontOpenFontset( Fontlib(), &fontType, hTemp.m_pNewFont );
		}
		else
		{
			nCellError = cellFontOpenFontFile(
				Fontlib(),
				reinterpret_cast<uint8_t *>(fontpath),
				0,
				++sUniquePositiveNumber,
				hTemp.m_pNewFont );
		}
	}
	CellFont *pNewFont = NULL;
	if ( nCellError != CELL_FONT_OK )
	{
		Warning( "could not open font '%s': %x\n", pFontname, nCellError );
		return false;
	}
	else
	{
		pNewFont = hTemp.DetachFont(); // do not release the font structure
		m_vExtantFonts.AddToTail( pNewFont );
	}

	float pointSize = tall * 0.79f;

	cellFontSetScalePixel( pNewFont, pointSize, pointSize );
	// sometimes we ask for bigger fonts than the packer can handle. Also, because of accents etc, the font may poke out of the requested tall value.
	// The packer is for some reason hardcoded to a maximum height of 128.
	CellFontHorizontalLayout hlayout;
	cellFontGetHorizontalLayout( pNewFont, &hlayout );
	if ( hlayout.lineHeight + hlayout.effectHeight > TEXTURE_PAGE_HEIGHT )
	{
		Warning("FAILSAFE: %s at '%d' tall is actually %d pixels high, but max texture page is %d. Shrinking to ", 
			pFontname, tall, (int)(hlayout.lineHeight + hlayout.effectHeight), TEXTURE_PAGE_HEIGHT );

		do
		{
			pointSize *= 0.9;
			cellFontSetScalePixel( pNewFont, tall, tall );
			cellFontGetHorizontalLayout( pNewFont, &hlayout );
		}
		while ( ( hlayout.lineHeight + hlayout.effectHeight > TEXTURE_PAGE_HEIGHT ) && (pointSize >= 1) );
		
		Warning( "%.2f\n", pointSize );
	}

	/*
	nCellError = cellFontGetHorizontalLayout( &m_sFont, &m_HorizontalLayout );
	AssertMsg2( nCellError == CELL_OK, "Could not get horizontal layout for font %s (%x)\n", m_szName.String(), nCellError );
	*/
	
	/*
	nCellError = cellFontGetVerticalLayout( &m_sFont, &m_VerticalLayout );
	AssertMsg2( nCellError == CELL_OK, "Could not get vertical layout for font %s (%x)\n", m_szName.String(), nCellError );
	*/

	return pNewFont;
}

void CMaterialSystem::CloseTrueTypeFont( HPS3FONT hFont )
{
	MaterialLock_t hLock = Lock();

	// find the font in the vector
	CellFont *pFont = CellFontFromHandle( hFont );
	cellFontCloseFont( pFont );
	if ( m_vExtantFonts.FindAndFastRemove( pFont ) )
	{
		delete pFont; 
	}
	else
	{
		AssertMsg1( false, "Tried to remove a font %x which wasn't in the material system's library\n", hFont );
	}


	Unlock( hLock );
}


bool CMaterialSystem::GetTrueTypeFontMetrics( HPS3FONT hFont, int nFallbackTall, wchar_t wchFirst, wchar_t wchLast, CPS3FontMetrics * RESTRICT pFontMetrics, CPS3CharMetrics * RESTRICT pCharMetrics )
{
	MaterialLock_t hLock = Lock();

	int numChars = wchLast - wchFirst + 1;
	V_memset( pCharMetrics, 0, numChars * sizeof( CPS3CharMetrics ) );
	CellFont * RESTRICT pFont = CellFontFromHandle( hFont );

	int nCellError = cellFontGetHorizontalLayout( pFont, pFontMetrics );
	AssertMsg1( nCellError == CELL_OK, "Could not get horizontal layout for font (%x)\n",  nCellError );
	// we will still need to go and fill in the max width once we've got the data for all the characters
	float fMaxWidth = 0;
	
	for ( int i = 0; i < numChars; i++ )
	{
		int nMetricsError = CELL_FONT_ERROR_FATAL;
		int nFallbackFontTallAdjustment = 0;
		if ( !ShouldFallBackToSystemFontForWChar( wchFirst + i, nFallbackFontTallAdjustment ) )
		{
			nMetricsError = cellFontGetCharGlyphMetrics( pFont, wchFirst + i, pCharMetrics + i );
		}
		if( nMetricsError != CELL_OK )
		{
			// try the fallback font
			int nFallbackError = g_fallbackFont.SetTall( nFallbackTall + nFallbackFontTallAdjustment );
			if( nFallbackError == CELL_OK )
			{
				nMetricsError = cellFontGetCharGlyphMetrics( &g_fallbackFont, wchFirst + i, pCharMetrics + i );
			}
		}

		if( nMetricsError  != CELL_OK )
		{
			nMetricsError = cellFontGetCharGlyphMetrics( pFont, g_wchFallbackGlyph, pCharMetrics + i );
		}

		if ( nMetricsError == CELL_OK )
		{
			fMaxWidth = fmax( fMaxWidth, pCharMetrics[i].Horizontal.advance + 1 );
		}
		else
		{
			memset( pCharMetrics+i, 0, sizeof(*pCharMetrics) );
		}
	}

	pFontMetrics->fMaxWidth = fMaxWidth;

	Unlock( hLock );
	return nCellError == CELL_OK;
}

// encapsulates the ps3 fontlib renderer class with a RAII interface
// so it binds/unbinds automatically
class CFontRendererRAII
{
public:
	CFontRendererRAII( CellFont *pBoundFont );
	~CFontRendererRAII();


	bool IsValid() { return m_nInitSuccess == CELL_FONT_OK; }
protected:

	CellFontRenderer m_Renderer;
	CellFont *m_pFont;
	int m_nInitSuccess;
};

CFontRendererRAII::CFontRendererRAII( CellFont *pBoundFont ) : m_pFont(NULL)
{
	CellFontRendererConfig config;
	CellFontRendererConfig_initialize( &config );

	// proper setup here

	m_nInitSuccess = cellFontCreateRenderer( Fontlib(), &config, &m_Renderer );
	if ( m_nInitSuccess != CELL_FONT_OK )
	{
		return ; // we have failed. we are failures. the stench of failure hangs upon us.
	}

	// connect the renderer to the font
	m_nInitSuccess = cellFontBindRenderer( pBoundFont, &m_Renderer );
	if ( m_nInitSuccess != CELL_FONT_OK )
	{
		return ; // we have failed. we are failures. the stench of failure hangs upon us.
	}

	m_pFont = pBoundFont;
}

CFontRendererRAII::~CFontRendererRAII()
{
	if ( m_pFont )
	{
		cellFontUnbindRenderer( m_pFont );
	}
	cellFontDestroyRenderer( &m_Renderer );
}

// OPTIMIZATION OPPORTUNITY:
// This can be made to run on the RSX instead of the PPU, if necessary.
bool CMaterialSystem::GetTrueTypeGlyphs( HPS3FONT hFont, int nFallbackTall, int numChars, wchar_t *pWch, int *pOffsetX, int *pOffsetY, int * RESTRICT pWidth, int * RESTRICT pHeight, unsigned char *pRGBA, int *pRGBAOffset )
{
	VPROF( "CMaterialSystem::GetTrueTypeGlyphs" );

	MaterialLock_t hLock = Lock();
	
	// Figure out the size of surface/texture we need to allocate
	int rtWidth = 0;
	int rtHeight = 0;
	for ( int i = 0; i < numChars; i++ )
	{
		//rtWidth += pWidth[i];
		rtWidth = MAX( rtWidth, pWidth[i] );
		rtHeight = MAX( rtHeight, pHeight[i] );
	}

	/*
	// per resolve() restrictions
	rtWidth = AlignValue( rtWidth, 32 );
	rtHeight = AlignValue( rtHeight, 32 );
	*/

	if ( rtWidth == 0 || rtHeight == 0 )
	{
		AssertMsg2( false, "Tried to draw a %dx%d glyph! What are you trying to pull?!\n", rtWidth, rtHeight );
		Unlock( hLock );
		return false;
	}

	CellFont *pFont = CellFontFromHandle( hFont );
	CFontRendererRAII renderer( pFont ), *pFallbackRenderer = NULL;

	CellFontRenderSurface surface;
	CellFontGlyphMetrics     metrics;
	CellFontImageTransInfo   transInfo;
	CellFontHorizontalLayout hlayoutSlowWeShouldMoveThisIntoTheFontClass;
	cellFontRenderSurfaceInit( &surface, NULL, rtWidth, 1, rtWidth, rtHeight ); // the surface doesn't actually use the memory pointer (go figure)
	cellFontRenderSurfaceSetScissor( &surface, 0, 0, rtWidth, rtHeight );
	cellFontGetHorizontalLayout( pFont, &hlayoutSlowWeShouldMoveThisIntoTheFontClass );
	const int baselineY = floor(hlayoutSlowWeShouldMoveThisIntoTheFontClass.baseLineY);

	// struct XUIRect { int x, int y, int w, int h }; 

	// Draw the characters, stepping across the texture
	// int xCursor = 0;
	unsigned int successfulWrites = 0;
	for ( int i = 0; i < numChars; i++)
	{
		/*
		// FIXME: the drawRect params don't make much sense (should use "(xCursor+pWidth[i]), pHeight[i]", but then some characters disappear!)
		XUIRect drawRect = XUIRect( xCursor + pOffsetX[i], pOffsetY[i], rtWidth, rtHeight );
		cellFontRenderSurfaceSetScissor( &surface, drawRect.x, drawRect.y, drawRect.w, drawRect.h );
		wchar_t	text[2] = { pWch[i], 0 };
		XuiDrawText( m_hDC, text, XUI_FONT_STYLE_NORMAL|XUI_FONT_STYLE_SINGLE_LINE|XUI_FONT_STYLE_NO_WORDWRAP, 0, &drawRect ); 
		xCursor += pWidth[i];
		*/
		// draw one glyph to the surface
		int ps3suc = CELL_FONT_ERROR_FATAL;
		int nFallbackFontTallAdjustment = 0;
		if ( !ShouldFallBackToSystemFontForWChar( pWch[i], nFallbackFontTallAdjustment ) )
		{
			ps3suc = cellFontRenderCharGlyphImage( pFont, pWch[i], &surface, 0, 0, &metrics, &transInfo );
		}
		if ( ps3suc != CELL_OK ) 
		{
			if( CELL_OK == g_fallbackFont.SetTall( nFallbackTall + nFallbackFontTallAdjustment ) )
			{
				if( !pFallbackRenderer )
					pFallbackRenderer = new CFontRendererRAII( &g_fallbackFont );
				ps3suc = cellFontRenderCharGlyphImage( &g_fallbackFont, pWch[i], &surface, 0, 0, &metrics, &transInfo );
			}
		}

		if( ps3suc != CELL_OK )
		{
			ps3suc = cellFontRenderCharGlyphImage( pFont, g_wchFallbackGlyph, &surface, 0, 0, &metrics, &transInfo );
		}

		if ( ps3suc == CELL_OK ) 
		{

			// blit the glyph to the bitmap
			const int ibw = transInfo.imageWidthByte;
			const int iBearingY = ceilf(metrics.Horizontal.bearingY); // start the conversion early to avoid a LHS
			int destPitch = pWidth[i]*4; // distance between one row and the next IN BYTES
			unsigned char *pLinear = pRGBA + pRGBAOffset[i];
			
			// Clear out all pixels to black, since the source glyph size may not match the destination buffer size perfectly
			memset( pLinear, 0, pHeight[i] * destPitch );

			if ( transInfo.imageHeight==0 || transInfo.imageWidth==0 )
			{
				// this can happen for space or control characters
				// just blank out the destination box
				continue; // bail out and go to next character !
			}

			// work out the size of the region to copy -- which is the smaller of the source or destination buffer.
			// in theory these two should be identical, but the PS3 OS tends to overallocate space slightly
			// in its render surface, and sometimes the VGUI font code is expecting a blank column of pixels
			// where the PS3 generates none.
			// also we'll try to align the font glyph to the bottom of the destination rect, rather than
			// the top.
			int numLeftPadPixels = ( int )floorf( metrics.Horizontal.bearingX );
			numLeftPadPixels = MAX( 0, numLeftPadPixels );
			int iteratorMaxColumn = MIN( pWidth[i] - numLeftPadPixels, transInfo.imageWidth );
			int iteratorMaxRow = MIN( pHeight[i], transInfo.imageHeight );
			unsigned char *pLinearAligned = pLinear; // work out where to start writing into the image so that chars are aligned to bottom of rect

			pLinearAligned = pLinear + MAX(0, baselineY-iBearingY)*destPitch;
			// for ( int row = 0 ; row < transInfo.imageHeight ; ++row )
			for ( int row = 0 ; row < iteratorMaxRow; ++row )
			{
				uint32 * pRowStart = reinterpret_cast<uint32 *>(pLinearAligned + (row*destPitch)); // we will write the RGBA one word at a time
				for ( int col = 0 ; col < iteratorMaxColumn; ++col )
				{
					// white characters; only the alpha mask is copied from the rendered glyph
					// exception: totally absent regions are marked as just black
					uint8 a = transInfo.Image[ row * ibw + col ];
					*(pRowStart + col + numLeftPadPixels) = a ? 0xFFFFFF00 | a : 0 ;
				}
			}

#if 0
			if ( FILE *f = fopen( "/app_home/dbgfonts.txt", "a+" ) )
			{
				fprintf( f, "% 4d %C  [%dx%d]\n",
					pWch[i], pWch[i],
					transInfo.imageWidth, transInfo.imageHeight
					);
				fprintf( f, "        [%d]\n",
					pWidth[i]
					);
				fprintf( f, "        %0.5f %0.5f %0.5f\n",
					metrics.Horizontal.bearingX, metrics.width, metrics.Horizontal.advance
					);
				for ( int row = 0; row < MAX( MAX( pHeight[i], ceilf( metrics.height ) ), transInfo.imageHeight ); ++ row )
				{
					for ( int col = 0; col < MAX( pWidth[i], ceilf( metrics.Horizontal.advance ) ); ++ col )
					{
						uint8 a = transInfo.Image[ row * ibw + col ];
						fprintf( f, a ? ( (a>0x20) ? "X" : "*" ) : " " );
					}
					fprintf( f, "|\n" );
				}
				fprintf( f, "-----------------------------\n" );
				fclose(f);
			}
#endif

#if 0		// draw a line around the outer border for debugging
			for ( int row = 0 ; row < pHeight[i]; ++row )
			{
				*(reinterpret_cast<uint32 *>(pLinear + (row*destPitch))+0) = 0xFFFFFFFF;
				*(reinterpret_cast<uint32 *>(pLinear + (row*destPitch))+pWidth[i]-1) = 0xFFFFFFFF;
			}
			// top and bottom row
			memset( pLinear, 0xFF, pWidth[i] * 4 );
			memset( (pLinear + ((pHeight[i]-1)*destPitch)), 0xFF, pWidth[i] * 4 );
#endif

			successfulWrites++;
		}
		else
		{
			AssertMsg2( false, "Couldn't draw u'%x', err %x\n", (unsigned int) *(pWch+i), ps3suc );
		}
	}

	if( pFallbackRenderer )
		delete pFallbackRenderer;

	Unlock( hLock );
	return successfulWrites > 0;
}

static int load_libfont_module()
{
	int ret;
	ret = cellSysmoduleLoadModule( CELL_SYSMODULE_FONT );
	if ( ret == CELL_OK ) {
		ret = cellSysmoduleLoadModule( CELL_SYSMODULE_FREETYPE_TT );
		if ( ret == CELL_OK ) {
			ret = cellSysmoduleLoadModule( CELL_SYSMODULE_FONTFT );
			if ( ret == CELL_OK ) {
				return CELL_OK; // Success
			}
			// Error handling as follows (Unload all loads)
			cellSysmoduleUnloadModule( CELL_SYSMODULE_FREETYPE_TT );
		}
		cellSysmoduleUnloadModule( CELL_SYSMODULE_FONT );
	}
	return ret; // Error end
}


static void* fonts_malloc( void*obj, uint32_t size )
{
	(void)obj;
	return malloc( size );
}
static void  fonts_free( void*obj, void*p )
{
	(void)obj;
	free( p );
}
static void* fonts_realloc( void*obj, void* p, uint32_t size )
{
	(void)obj;
	return realloc( p, size );
}
static void* fonts_calloc( void*obj, uint32_t numb, uint32_t blockSize )
{
	(void)obj;
	return calloc( numb, blockSize );
}


static int s_FontLibraryRefCount = 0;
static uint32 *s_FontFileCache = NULL;
static const CellFontLibrary *s_FontLibrary = NULL;
static CellFontEntry *s_UserFontEntries = NULL;
bool CMaterialSystem::PS3InitFontLibrary( unsigned fontFileCacheSizeInBytes, unsigned maxNumFonts )
{
	if ( s_FontLibraryRefCount > 0 )
	{ 
		// don't need to load the font library, just add one to refcount
		++s_FontLibraryRefCount;
		return true;
	}
	else if ( s_FontLibraryRefCount < 0 )
	{
		Error( "Font library refcount is %d!\n", s_FontLibraryRefCount );
	}
	else 
	{
		++s_FontLibraryRefCount;
	}

	VPROF("CMaterialSystem::PS3InitFontLibrary");
	if ( s_FontFileCache )
	{
		Warning("Tried to init font library twice without intervening unload\n");
		return false;
	}
	int ret;
	if ( (ret=load_libfont_module()) != CELL_OK )
	{
		Warning("Could not load font library: %x\n", ret);
	}
	maxNumFonts = maxNumFonts ? maxNumFonts : 1;

	if (s_FontFileCache)
	{
		Assert(false); // wasn't cleaned up?!
		delete[] s_FontFileCache;
	}
	if (s_UserFontEntries)
	{
		Assert(false); // wasn't cleaned up?!
		delete[] s_UserFontEntries;
	}
	s_FontFileCache = new uint32[fontFileCacheSizeInBytes >> 2];
	s_UserFontEntries = new CellFontEntry[maxNumFonts];
	
	CellFontConfig config;
	config.FileCache.buffer = s_FontFileCache;
	config.FileCache.size = fontFileCacheSizeInBytes;
	config.userFontEntrys = s_UserFontEntries;
	config.userFontEntryMax = maxNumFonts;

	ret = cellFontInit(&config);
	if ( ret == CELL_OK )
	{
		// can override malloc/free here:
		CellFontLibraryConfigFT ftConfig;
		CellFontLibraryConfigFT_initialize( &ftConfig );

		ftConfig.MemoryIF.Object  = NULL;
		ftConfig.MemoryIF.Malloc  = fonts_malloc;
		ftConfig.MemoryIF.Free    = fonts_free;
		ftConfig.MemoryIF.Realloc = fonts_realloc;
		ftConfig.MemoryIF.Calloc  = fonts_calloc;
		ret = cellFontInitLibraryFreeType( &ftConfig, &s_FontLibrary );
		if ( ret == CELL_OK )
		{
			g_fallbackFont.Init( );
			return true;
		}
		else
		{
			Warning( "cellFontInitLibraryFreeType failed, %x\n", ret );
			return false;
		}
	}
	else
	{
		Warning( "CellFontInit failed, %x\n", ret );
		return false;
	}
}

void *CMaterialSystem::PS3GetFontLibPtr() 
{
	return (void*)s_FontLibrary;
}

void CMaterialSystem::PS3DumpFontLibrary()
{
	if ( s_FontLibraryRefCount <= 0 )
	{
		Warning("Font library refcount is %d during unload!\n", s_FontLibraryRefCount);
		return; 
	}
	else if ( s_FontLibrary == NULL )
	{
		Warning("\t!!!FAILSAFE!!!\nPS3 font library refcount %d but was somehow unloaded!\n", s_FontLibraryRefCount);
		s_FontLibraryRefCount = 0;
	}
	else if ( --s_FontLibraryRefCount > 0 )
	{	// refcount is still greater than one, don't need to unload
		return;
	}

	// Warning(		"Font library was dumped with %d fonts left open.\n", m_vExtantFonts.Count() );
	if ( s_FontLibrary )
	{
		g_fallbackFont.Shutdown();

		cellFontEndLibrary( s_FontLibrary );
		cellFontEnd();

		delete[] s_FontFileCache;
		s_FontFileCache = NULL;

		// handled by cellFontEndLibrary: // delete s_FontLibrary;
		s_FontLibrary = NULL;

		delete[] s_UserFontEntries;
		s_UserFontEntries = NULL;


		cellSysmoduleUnloadModule( CELL_SYSMODULE_FONTFT );
		cellSysmoduleUnloadModule( CELL_SYSMODULE_FREETYPE_TT );
		cellSysmoduleUnloadModule( CELL_SYSMODULE_FONT );

	}
}

static const CellFontLibrary *Fontlib()
{
	return s_FontLibrary;
}