//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAMEUISYSTEMSURFACE_H
#define GAMEUISYSTEMSURFACE_H

#ifdef _WIN32
#pragma once
#endif

#include "igameuisystemmgr.h"
#include "materialsystem/imaterialsystem.h"
#include "vgui_surfacelib/ifontsurface.h" 
#include "tier1/utldict.h"
#include "rendersystem/irenderdevice.h"

class CFontTextureCache;
class KeyValues;


//-----------------------------------------------------------------------------
// This class is the interface to the font and font texture, systems.
// Load fonts given by schemes into the systems using this class.
//-----------------------------------------------------------------------------
class CGameUISystemSurface : public IGameUISystemSurface
{

public:

	CGameUISystemSurface();
	~CGameUISystemSurface();

	InitReturnVal_t Init();
	void Shutdown();

	void PrecacheFontCharacters( FontHandle_t font, wchar_t *pCharacterString = NULL );

	FontHandle_t CreateFont();
	bool SetFontGlyphSet( FontHandle_t font, const char *windowsFontName, int tall, int weight, int blur, int scanlines, int flags, int nRangeMin = 0, int nRangeMax = 0 );
	int GetFontTall( FontHandle_t font );
	void GetCharABCwide( FontHandle_t font, int ch, int &a, int &b, int &c );
	int GetCharacterWidth( FontHandle_t font, int ch );
	const char *GetFontName( FontHandle_t font );

	bool AddCustomFontFile( const char *fontFileName );

	// Helper fxns for loading bitmap fonts
	bool AddBitmapFontFile( const char *fontFileName );
	void SetBitmapFontName( const char *pName, const char *pFontFilename );
	const char *GetBitmapFontName( const char *pName );
	bool SetBitmapFontGlyphSet( FontHandle_t font, const char *windowsFontName, float scalex, float scaley, int flags);

	void ClearTemporaryFontCache( void );

	// Causes fonts to get reloaded, etc.
	void ResetFontCaches();

	bool SupportsFontFeature( FontFeature_t feature );

	void DrawSetTextureRGBA( int id, const unsigned char* rgba, int wide, int tall ){}
	void DrawSetTextureRGBAEx( int id, const unsigned char* rgba, int wide, int tall, ImageFormat format ){}

	bool GetUnicodeCharRenderPositions( FontCharRenderInfo& info, Vector2D *pPositions );
 	IMaterial *GetTextureForChar( FontCharRenderInfo &info, float **texCoords );
	IMaterial *GetTextureAndCoordsForChar( FontCharRenderInfo &info, float *texCoords );

	// Used for debugging.
	void DrawFontTexture( int textureId, int xPos, int yPos );
	void DrawFontTexture( IRenderContext *pRenderContext, int textureId, int xPos, int yPos );
	
	IMaterial *GetMaterial( int textureId );
	HRenderTexture GetTextureHandle( int textureId );

	void GetProportionalBase( int &width, int &height ) { width = BASE_WIDTH; height = BASE_HEIGHT; }

	void SetLanguage( const char *pLanguage );
	const char *GetLanguage();


private:
	enum { BASE_HEIGHT = 480, BASE_WIDTH = 640 };
		
	bool m_bIsInitialized;
	
	CUtlVector< CUtlSymbol >	m_CustomFontFileNames;
	CUtlVector< CUtlSymbol >	m_BitmapFontFileNames;
	CUtlDict< int, int >		m_BitmapFontFileMapping;

};

extern CGameUISystemSurface *g_pGameUISystemSurface;




#endif // GAMEUISYSTEMSURFACE_H
