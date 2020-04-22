//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#define SUPPORT_CUSTOM_FONT_FORMAT

#ifdef SUPPORT_CUSTOM_FONT_FORMAT
	#define _WIN32_WINNT 0x0500
#endif

#include "gameuisystemsurface.h"
#include "gameuisystemmgr.h"
#include "vgui_surfacelib/fontmanager.h"
#include "vgui_surfacelib/texturedictionary.h"
#include "vgui_surfacelib/fonttexturecache.h"
#include "vgui/isystem.h"
#include "tier1/utlbuffer.h"
#include "tier1/keyvalues.h"
#include "materialsystem/imesh.h"
#include "gameuischeme.h"
#include "vgui_controls/Controls.h"

#include "rendersystem/irendercontext.h"
#include "rendersystem/vertexdata.h"
#include "rendersystem/indexdata.h"
#include "GameLayer.h"

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif
#include "xbox/xboxstubs.h"

#include "valvefont.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Helper function for getting the language the game ui should use.
//-----------------------------------------------------------------------------
static void HelperGetLanguage( char *pLanguageBuf, int bufSize )
{
	bool bValid = false;
	if ( IsPC() )
	{
		bValid = vgui::system()->GetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Language", pLanguageBuf, bufSize - 1 );	
	}
	else
	{
		Q_strncpy( pLanguageBuf, XBX_GetLanguageString(), bufSize );
		bValid = true;
	}

	if ( !bValid )
	{
		Q_strncpy( pLanguageBuf, "english", bufSize );
	}
}

//-----------------------------------------------------------------------------
// Globals...
//-----------------------------------------------------------------------------
static CFontTextureCache g_FontTextureCache;

//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
CGameUISystemSurface g_GameUISystemSurface;
CGameUISystemSurface *g_pGameUISystemSurface = &g_GameUISystemSurface;

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CGameUISystemSurface::CGameUISystemSurface()
{
	m_bIsInitialized = false;
	g_FontTextureCache.SetPrefix( "ingameui" );
}

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CGameUISystemSurface::~CGameUISystemSurface()
{
	m_CustomFontFileNames.RemoveAll();
	m_BitmapFontFileNames.RemoveAll();
	m_BitmapFontFileMapping.RemoveAll();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
InitReturnVal_t CGameUISystemSurface::Init()
{
	if ( m_bIsInitialized )
		return INIT_OK;


	// fonts initialization
	char language[64];
	HelperGetLanguage( language, 64 );
	FontManager().SetLanguage( language );
	m_bIsInitialized = true;

	return INIT_OK;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUISystemSurface::Shutdown( void )
{
	// Release all textures
	TextureDictionary()->DestroyAllTextures();


#if !defined( _GAMECONSOLE )

	HMODULE gdiModule = NULL;

#ifdef SUPPORT_CUSTOM_FONT_FORMAT
	; // on custom font format Windows takes care of cleaning up the font when the process quits
#else
 	// release any custom font files
	// use newer function if possible
	gdiModule = ::LoadLibrary( "gdi32.dll" );
	typedef int (WINAPI *RemoveFontResourceExProc)(LPCTSTR, DWORD, PVOID);
	RemoveFontResourceExProc pRemoveFontResourceEx = NULL;
	if ( gdiModule )
	{
		pRemoveFontResourceEx = (RemoveFontResourceExProc)::GetProcAddress(gdiModule, "RemoveFontResourceExA");
	}

	for (int i = 0; i < m_CustomFontFileNames.Count(); i++)
 	{
		if ( pRemoveFontResourceEx )
		{
			// dvs: Keep removing the font until we get an error back. After consulting with Microsoft, it appears
			// that RemoveFontResourceEx must sometimes be called multiple times to work. Doing this insures that
			// when we load the font next time we get the real font instead of Ariel.
			int nRetries = 0;
			while ( (*pRemoveFontResourceEx)(m_CustomFontFileNames[i].String(), 0x10, NULL) && ( nRetries < 10 ) )
			{
				nRetries++;
				Msg( "Removed font resource %s on attempt %d.\n", m_CustomFontFileNames[i].String(), nRetries );
			}
		}
		else
		{
			// dvs: Keep removing the font until we get an error back. After consulting with Microsoft, it appears
			// that RemoveFontResourceEx must sometimes be called multiple times to work. Doing this insures that
			// when we load the font next time we get the real font instead of Ariel.
			int nRetries = 0;
			while ( ::RemoveFontResource(m_CustomFontFileNames[i].String()) && ( nRetries < 10 ) )
			{
				nRetries++;
				Msg( "Removed font resource %s on attempt %d.\n", m_CustomFontFileNames[i].String(), nRetries );
			}
		}
 	}
#endif // SUPPORT_CUSTOM_FONT_FORMAT

#endif

 	m_CustomFontFileNames.RemoveAll();
	m_BitmapFontFileNames.RemoveAll();
	m_BitmapFontFileMapping.RemoveAll();

#if !defined( _GAMECONSOLE )
	if ( gdiModule )
	{
		::FreeLibrary(gdiModule);
	}
#endif

}


//-----------------------------------------------------------------------------
// Purpose: adds a custom font file (supports valve .vfont files)
//-----------------------------------------------------------------------------
bool CGameUISystemSurface::AddCustomFontFile( const char *fontFileName )
{
	if ( IsGameConsole() )
	{
		// custom fonts are not supported (not needed) on xbox, all .vfonts are offline converted to ttfs
		// ttfs are mounted/handled elsewhere
		return true;
	}

	char fullPath[MAX_PATH];
	bool bFound = false;
	// windows needs an absolute path for ttf
	bFound = g_pFullFileSystem->GetLocalPath( fontFileName, fullPath, sizeof( fullPath ) ) ? true : false;
	if ( !bFound )
	{
		Warning( "Couldn't find custom font file '%s'\n", fontFileName );
		return false;
	}

	// only add if it's not already in the list
	Q_strlower( fullPath );
	CUtlSymbol sym( fullPath );
	int i;
	for ( i = 0; i < m_CustomFontFileNames.Count(); i++ )
	{
		if ( m_CustomFontFileNames[i] == sym )
			break;
	}
	if ( !m_CustomFontFileNames.IsValidIndex( i ) )
	{
	 	m_CustomFontFileNames.AddToTail( fullPath );

		if ( IsPC() )
		{
#ifdef SUPPORT_CUSTOM_FONT_FORMAT
			// We don't need the actual file on disk
#else
			// make sure it's on disk
			// only do this once for each font since in steam it will overwrite the
			// registered font file, causing windows to invalidate the font
			g_pFullFileSystem->GetLocalCopy( fullPath );
#endif
		}
	}

#if !defined( _GAMECONSOLE )

#ifdef SUPPORT_CUSTOM_FONT_FORMAT
	// Just load the font data, decrypt in memory and register for this process
	CUtlBuffer buf;
	if ( !g_pFullFileSystem->ReadFile( fontFileName, NULL, buf ) )
	{
		Msg( "Failed to load custom font file '%s'\n", fontFileName );
		return false;
	}

	if ( !ValveFont::DecodeFont( buf ) )
	{
		Msg( "Failed to parse custom font file '%s'\n", fontFileName );
		return false;
	}

	DWORD dwNumFontsRegistered = 0;
	HANDLE hRegistered = NULL;
	hRegistered = ::AddFontMemResourceEx( buf.Base(), buf.TellPut(), NULL, &dwNumFontsRegistered );

	if ( !hRegistered )
	{
		Msg( "Failed to register custom font file '%s'\n", fontFileName );
		return false;
	}

	return hRegistered != NULL;
#else
	// try and use the optimal custom font loader, will makes sure fonts are unloaded properly
	// this function is in a newer version of the gdi library (win2k+), so need to try get it directly
	bool successfullyAdded = false;
	HMODULE gdiModule = ::LoadLibrary( "gdi32.dll" );
	if (gdiModule)
	{
		typedef int (WINAPI *AddFontResourceExProc)(LPCTSTR, DWORD, PVOID);
		AddFontResourceExProc pAddFontResourceEx = (AddFontResourceExProc)::GetProcAddress( gdiModule, "AddFontResourceExA" );
		if (pAddFontResourceEx)
		{
			int result = (*pAddFontResourceEx)(fullPath, 0x10, NULL);
			if (result > 0)
			{
				successfullyAdded = true;
			}
		}
		::FreeLibrary(gdiModule);
	}

	// add to windows
	bool success = successfullyAdded || (::AddFontResource(fullPath) > 0);
	if ( !success )
	{
		Msg( "Failed to load custom font file '%s'\n", fullPath );
	}
	Assert( success );
	return success;
#endif // SUPPORT_CUSTOM_FONT_FORMAT

#else
	return true;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: adds a bitmap font file
//-----------------------------------------------------------------------------
bool CGameUISystemSurface::AddBitmapFontFile( const char *fontFileName )
{
	bool bFound = false;
	bFound = ( ( g_pFullFileSystem->GetDVDMode() == DVDMODE_STRICT ) || g_pFullFileSystem->FileExists( fontFileName, IsGameConsole() ? "GAME" : NULL ) );
	if ( !bFound )
	{
		Msg( "Couldn't find bitmap font file '%s'\n", fontFileName );
		return false;
	}
	char path[MAX_PATH];
	Q_strncpy( path, fontFileName, MAX_PATH );

	// only add if it's not already in the list
	Q_strlower( path );
	CUtlSymbol sym( path );
	int i;
	for ( i = 0; i < m_BitmapFontFileNames.Count(); i++ )
	{
		if ( m_BitmapFontFileNames[i] == sym )
			break;
	}
	if ( !m_BitmapFontFileNames.IsValidIndex( i ) )
	{
	 	m_BitmapFontFileNames.AddToTail( path );

		if ( IsPC() )
		{
			// make sure it's on disk
			// only do this once for each font since in steam it will overwrite the
			// registered font file, causing windows to invalidate the font
			g_pFullFileSystem->GetLocalCopy( path );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUISystemSurface::SetBitmapFontName( const char *pName, const char *pFontFilename )
{
	char fontPath[MAX_PATH];
	Q_strncpy( fontPath, pFontFilename, MAX_PATH );
	Q_strlower( fontPath );

	CUtlSymbol sym( fontPath );
	for ( int i = 0; i < m_BitmapFontFileNames.Count(); i++ )
	{
		if ( m_BitmapFontFileNames[i] == sym )
		{
			// found it, update the mapping
			int index = m_BitmapFontFileMapping.Find( pName );
			if ( !m_BitmapFontFileMapping.IsValidIndex( index ) )
			{
				index = m_BitmapFontFileMapping.Insert( pName );	
			}
			m_BitmapFontFileMapping.Element( index ) = i;
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CGameUISystemSurface::GetBitmapFontName( const char *pName )
{
	// find it in the mapping symbol table
	int index = m_BitmapFontFileMapping.Find( pName );
	if ( index == m_BitmapFontFileMapping.InvalidIndex() )
	{
		return "";
	}

	return m_BitmapFontFileNames[m_BitmapFontFileMapping.Element( index )].String();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUISystemSurface::ClearTemporaryFontCache( void )
{
	FontManager().ClearTemporaryFontCache();
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
const char *CGameUISystemSurface::GetFontName( FontHandle_t font )
{
	return FontManager().GetFontName( font );
}

//-----------------------------------------------------------------------------
// Causes fonts to get reloaded, etc. 
//-----------------------------------------------------------------------------
void CGameUISystemSurface::ResetFontCaches()
{
	// Don't do this on x360!!!
	if ( IsGameConsole() )
		return;

	// clear font texture cache
	g_FontTextureCache.Clear();

	g_pGameUISchemeManager->ReloadFonts();
}


//-----------------------------------------------------------------------------
// Purpose: cap bits
// Warning: if you change this, make sure the SurfaceV28 wrapper above reports
//          the correct capabilities.
//-----------------------------------------------------------------------------
bool CGameUISystemSurface::SupportsFontFeature( FontFeature_t feature )
{
	switch ( feature )
	{
		case FONT_FEATURE_ANTIALIASED_FONTS:
		case FONT_FEATURE_DROPSHADOW_FONTS:
			return true;

		case FONT_FEATURE_OUTLINE_FONTS:
			if ( IsGameConsole() )
				return false;
			return true;

		default:
			return false;
	};
}


//-----------------------------------------------------------------------------
// Purpose: Force a set of characters to be rendered into the font page.
//-----------------------------------------------------------------------------
void CGameUISystemSurface::PrecacheFontCharacters( FontHandle_t font, wchar_t *pCharacterString )
{
	wchar_t *pCommonChars = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,.!:-/%";
	if ( !pCharacterString || !pCharacterString[0] )
	{
		// use the common chars, alternate languages are not handled
		pCharacterString = pCommonChars;
	}

	int numChars = 0;
	while( pCharacterString[ numChars ] )
	{
		numChars++;
	}
	int *pTextureIDs_ignored = (int *)_alloca( numChars*sizeof( int ) );
	float **pTexCoords_ignored = (float **)_alloca( numChars*sizeof( float * ) );
	g_FontTextureCache.GetTextureForChars( font, FONT_DRAW_DEFAULT, pCharacterString, pTextureIDs_ignored, pTexCoords_ignored, numChars );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUISystemSurface::DrawFontTexture( int textureId, int xPos, int yPos )
{
	Assert( g_pMaterialSystem );
	IMaterial *pMaterial = GetMaterial( textureId );
	if (!pMaterial)
	{
		return;
	}

	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	IMesh *pMesh = pRenderContext->GetDynamicMesh( true, NULL, NULL, pMaterial );

	if ( !pMesh )
		return;


	unsigned char textColor[4];
	textColor[0] = 255;
	textColor[1] = 255;
	textColor[2] = 255;
	textColor[3] = 255;

	int x = xPos;
	int y = yPos;
	int wide;
	int tall;
	TextureDictionary()->GetTextureSize( textureId, wide, tall );


	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	meshBuilder.Position3f( x, y, 0 );
	meshBuilder.Color4ubv( textColor );
	meshBuilder.TexCoord2f( 0, 0, 0 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.Position3f( x + wide, y, 0 );
	meshBuilder.Color4ubv( textColor );
	meshBuilder.TexCoord2f( 0, 1, 0 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.Position3f( x + wide, y + tall, 0 );
	meshBuilder.Color4ubv( textColor );
	meshBuilder.TexCoord2f( 0, 1, 1 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.Position3f( x, y + tall, 0 );
	meshBuilder.Color4ubv( textColor );
	meshBuilder.TexCoord2f( 0, 0, 1 );
	meshBuilder.AdvanceVertexF<VTX_HAVEPOS | VTX_HAVECOLOR, 1>();

	meshBuilder.End();
	pMesh->Draw();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUISystemSurface::DrawFontTexture( IRenderContext *pRenderContext, int textureId, int xPos, int yPos )
{
	HRenderTexture fontTextureHandle = g_pGameUISystemSurface->GetTextureHandle( textureId );
	pRenderContext->BindTexture( 0, fontTextureHandle );
	
	VertexColor_t textColor( 255, 255, 255, 255);

	int x = xPos;
	int y = yPos;
	int wide;
	int tall;
	TextureDictionary()->GetTextureSize( textureId, wide, tall );

	CDynamicVertexData< GameUIVertex_t > vb( pRenderContext, 4, "gamelayer2", "game_controls2" );
	vb.Lock();

	vb->m_vecPosition.Init( x, y, 0.0f );
	vb->m_color = textColor;
	vb->m_vecTexCoord.Init( 0, 0 );
	vb.AdvanceVertex();

	vb->m_vecPosition.Init( x + wide, y, 0.0f );
	vb->m_color = textColor;
	vb->m_vecTexCoord.Init( 1, 0 );
	vb.AdvanceVertex();

	vb->m_vecPosition.Init( x+ wide, y + tall, 0.0f );
	vb->m_color = textColor;
	vb->m_vecTexCoord.Init( 1, 1 );
	vb.AdvanceVertex();

	vb->m_vecPosition.Init( x, y + tall, 0.0f );
	vb->m_color = textColor;
	vb->m_vecTexCoord.Init( 0, 1 );
	vb.AdvanceVertex();

	vb.Unlock();
	vb.Bind( 0, 0 );


	CDynamicIndexData< uint16 > ib( pRenderContext, 6, "gamelayer", "game_controls" );
	ib.Lock();

	ib.Index( 0 );
	ib.Index( 1 );
	ib.Index( 2 );

	ib.Index( 0);
	ib.Index( 2 );
	ib.Index( 3 );

	ib.Unlock();
	ib.Bind( 0 );

	pRenderContext->DrawIndexed( RENDER_PRIM_TRIANGLES, 0, 6 );

}

//-----------------------------------------------------------------------------
// Purpose: Gets the material and texture coords for this char.
// Set up info.drawtype and set the font first.
//-----------------------------------------------------------------------------
IMaterial *CGameUISystemSurface::GetTextureForChar( FontCharRenderInfo &info, float **texCoords )
{
	bool bSuccess = g_FontTextureCache.GetTextureForChar( info.currentFont, info.drawType, info.ch, &info.textureId, texCoords );
	if ( !bSuccess )
	{															   
		return NULL; 
	}

	return TextureDictionary()->GetTextureMaterial( info.textureId );
}

//-----------------------------------------------------------------------------
// Purpose: Gets the material and texture coords for this char.
// Set up info.drawtype and set the font first.
// This call doesnt use the static in the font cache to store the texcoords.
//-----------------------------------------------------------------------------
IMaterial *CGameUISystemSurface::GetTextureAndCoordsForChar( FontCharRenderInfo &info, float *texCoords )
{
	bool bSuccess = g_FontTextureCache.GetTextureAndCoordsForChar( info.currentFont, info.drawType, info.ch, &info.textureId, texCoords );
	if ( !bSuccess )
	{															   
		return NULL; 
	}

	return TextureDictionary()->GetTextureMaterial( info.textureId );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CGameUISystemSurface::GetUnicodeCharRenderPositions( FontCharRenderInfo& info, Vector2D *pPositions  )
{
	info.valid = false;

	if ( !info.currentFont )
	{
		return info.valid;
	}

	info.valid = true;
	info.fontTall = GetFontTall( info.currentFont );

	GetCharABCwide( info.currentFont, info.ch, info.abcA, info.abcB, info.abcC );
	bool bUnderlined = FontManager().GetFontUnderlined( info.currentFont );
	
	// Do prestep before generating texture coordinates, etc.
	if ( !bUnderlined )
	{
		info.x += info.abcA;
	}

	// get the character texture from the cache
	info.textureId = 0;
	float *texCoords = NULL;
	if ( !g_FontTextureCache.GetTextureForChar( info.currentFont, info.drawType, info.ch, &info.textureId, &texCoords ) )
	{
		info.valid = false;
		return info.valid;
	}

	int fontWide = info.abcB;
	if ( bUnderlined )
	{
		fontWide += ( info.abcA + info.abcC );
		info.x-= info.abcA;
	}

	pPositions[0].x  = info.x;
	pPositions[0].y  = info.y;
	pPositions[1].x = info.x + fontWide;
	pPositions[1].y = info.y;
	pPositions[2].x = info.x + fontWide;
	pPositions[2].y = info.y + info.fontTall;
	pPositions[3].x = info.x;
	pPositions[3].y = info.y + info.fontTall;

	return info.valid;
}

//-----------------------------------------------------------------------------
// Purpose: adds glyphs to a font created by CreateFont()
//-----------------------------------------------------------------------------
bool CGameUISystemSurface::SetBitmapFontGlyphSet( FontHandle_t font, const char *windowsFontName, float scalex, float scaley, int flags)
{
	return FontManager().SetBitmapFontGlyphSet( font, windowsFontName, scalex, scaley, flags );
}

//-----------------------------------------------------------------------------
// Purpose: creates a new empty font
//-----------------------------------------------------------------------------
FontHandle_t CGameUISystemSurface::CreateFont()
{
	return FontManager().CreateFont();
}

//-----------------------------------------------------------------------------
// Purpose: adds glyphs to a font created by CreateFont()
//-----------------------------------------------------------------------------
bool CGameUISystemSurface::SetFontGlyphSet( FontHandle_t font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags, int nRangeMin, int nRangeMax)
{
	return FontManager().SetFontGlyphSet( font, windowsFontName, tall, weight, blur, scanlines, 
		flags, nRangeMin, nRangeMax );
}

//-----------------------------------------------------------------------------
// Purpose: returns the max height of a font
//-----------------------------------------------------------------------------
int CGameUISystemSurface::GetFontTall( FontHandle_t font )
{
	return FontManager().GetFontTall( font );
}

//-----------------------------------------------------------------------------
// Purpose: returns the abc widths of a single character
// This is used by text classes to handle kerning.
//-----------------------------------------------------------------------------
void CGameUISystemSurface::GetCharABCwide( FontHandle_t font, int ch, int &a, int &b, int &c )
{
	FontManager().GetCharABCwide( font, ch, a, b, c );
}

//-----------------------------------------------------------------------------
// Purpose: returns the pixel width of a single character
//-----------------------------------------------------------------------------
int CGameUISystemSurface::GetCharacterWidth( FontHandle_t font, int ch )
{
	return FontManager().GetCharacterWidth( font, ch );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
IMaterial *CGameUISystemSurface::GetMaterial( int textureId )
{
	return TextureDictionary()->GetTextureMaterial( textureId );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
HRenderTexture CGameUISystemSurface::GetTextureHandle( int textureId )
{
	return TextureDictionary()->GetTextureHandle( textureId );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUISystemSurface::SetLanguage( const char *pLanguage )
{ 
	FontManager().SetLanguage( pLanguage );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CGameUISystemSurface::GetLanguage()
{ 
	return FontManager().GetLanguage();
}






















