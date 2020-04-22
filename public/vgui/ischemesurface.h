//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef ISCHEMESURFACE_H
#define ISCHEMESURFACE_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui_surfacelib/ifontsurface.h"
#include "appframework/iappsystem.h"

class IMaterial; 

//-----------------------------------------------------------------------------
// Purpose: Wraps functions that the scheme system needs access to.
//-----------------------------------------------------------------------------
abstract_class ISchemeSurface : public IAppSystem
{
public:
	virtual InitReturnVal_t Init() = 0;

	// creates an empty handle to a vgui font.  windows fonts can be add to this via SetFontGlyphSet().
	virtual FontHandle_t CreateFont() = 0;
	virtual bool SetFontGlyphSet( FontHandle_t font, const char *windowsFontName, int tall, int weight, 
		int blur, int scanlines, int flags, int nRangeMin = 0, int nRangeMax = 0) = 0;
	virtual const char *GetFontName( FontHandle_t font ) = 0;
	virtual int GetFontTall( FontHandle_t font ) = 0;
	virtual int GetCharacterWidth( FontHandle_t font, int ch ) = 0;
	virtual void GetCharABCwide( FontHandle_t font, int ch, int &a, int &b, int &c ) = 0;

	// Custom font support
	virtual bool AddCustomFontFile(const char *fontFileName) = 0;

	// Bitmap Font support
	virtual bool AddBitmapFontFile(const char *fontFileName) = 0;
	virtual void SetBitmapFontName( const char *pName, const char *pFontFilename ) = 0;
	virtual const char *GetBitmapFontName( const char *pName ) = 0;
	virtual bool SetBitmapFontGlyphSet( FontHandle_t font, const char *windowsFontName, float scalex, 
		float scaley, int flags) = 0;

	virtual bool SupportsFontFeature( FontFeature_t feature ) = 0;

	virtual void GetScreenSize(int &wide, int &tall) = 0;

	// Gets the base resolution used in proportional mode
	virtual void GetProportionalBase( int &width, int &height ) = 0;

	// Functions used by game ui editor.
	virtual IMaterial *GetTextureForChar( FontCharRenderInfo &info, float **texCoords )
	{ 
		Assert(0);
		return NULL; 
	}

	// Returns an array of the 4 render positions for the character.
	virtual bool GetUnicodeCharRenderPositions( FontCharRenderInfo& info, Vector2D *pPositions )
	{ 
		Assert(0);
		return false; 
	}

	virtual IMaterial *GetMaterial( int textureId )
	{ 
		Assert(0);
		return NULL; 
	}

	virtual void SetLanguage( const char *pLanguage ) = 0;
	virtual const char *GetLanguage() = 0;

	virtual void PrecacheFontCharacters( FontHandle_t font, wchar_t *pCharacters ) = 0;
};


#endif // ISCHEMESURFACE_H
