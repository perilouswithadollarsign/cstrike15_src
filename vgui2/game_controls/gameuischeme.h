//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAMEUISCHEME_H
#define GAMEUISCHEME_H

#ifdef _WIN32
#pragma once
#endif

#include "igameuisystemmgr.h"
#include "vgui_surfacelib/ifontsurface.h"
#include "tier1/utlstring.h"
//#include "color.h"
#include "tier1/utlsymbol.h"


class CGameUISystemSurface;
class KeyValues;


//-----------------------------------------------------------------------------
// Game UI version of a vgui scheme
//-----------------------------------------------------------------------------
class CGameUIScheme : public IGameUIScheme
{
public:

	CGameUIScheme();
	~CGameUIScheme();

	FontHandle_t GetFont( const char *fontName, bool proportional = false );
	FontHandle_t GetFontNextSize( bool bUp, const char *fontName, bool proportional = false );
	//Color GetColor( const char *colorName, Color defaultColor ){ return Color( 255, 255, 255, 255 ); }

	// These fxns are very similar to CScheme.
	void Shutdown( bool full );
	void LoadFromFile( const char *pFilename, const char *inTag, KeyValues *inKeys );

	// Gets at the scheme's name
	const char *GetName() { return m_pTag; }
	const char *GetFileName() { return m_pFileName; }

	char const *GetFontName( const FontHandle_t &font );

	void ReloadFontGlyphs( int inScreenTall = -1 );

	void SpewFonts();

	bool GetFontRange( const char *fontname, int &nMin, int &nMax );
	void SetFontRange( const char *fontname, int nMin, int nMax );	
	
	void SetActive( bool bActive );
	bool IsActive() const;

private:
	const char *GetMungedFontName( const char *fontName, const char *scheme, bool proportional );
	void LoadFonts();
	int GetMinimumFontHeightForCurrentLanguage( const char *pLanguage = NULL );
	FontHandle_t FindFontInAliasList( const char *fontName );

	bool m_bActive;
	CUtlString m_pFileName;
	CUtlString m_pTag;

	KeyValues *m_pData;

#pragma pack(1)
	struct fontalias_t
	{
		CUtlSymbol _fontName;
		CUtlSymbol _trueFontName;
		unsigned short _font : 15;
		unsigned short m_bProportional : 1;
	};
#pragma pack()

	struct fontrange_t
	{
		CUtlSymbol _fontName;
		int _min;
		int _max;
	};

	CUtlVector< fontrange_t > m_FontRanges;
	CUtlVector< fontalias_t > m_FontAliases;
	
};



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CGameUISchemeManager : public IGameUISchemeMgr
{
public:
	CGameUISchemeManager();
	~CGameUISchemeManager();

	// loads a scheme from a file
	// first scheme loaded becomes the default scheme, and all subsequent loaded scheme are derivitives of that
	// tag is friendly string representing the name of the loaded scheme
	IGameUIScheme * LoadSchemeFromFile( const char *fileName, const char *tag );

	// reloads the schemes from the file
	void ReloadSchemes();

	// reloads scheme fonts
	void ReloadFonts( int inScreenTall = -1 );

	// returns a handle to the default (first loaded) scheme
	IGameUIScheme * GetDefaultScheme();

	// returns a handle to the scheme identified by "tag"
	IGameUIScheme * GetScheme( const char *tag );

	void Shutdown( bool full );

	// gets the proportional coordinates for doing screen-size independant panel layouts
	// use these for font, image and panel size scaling (they all use the pixel height of the display for scaling)
	int GetProportionalScaledValue( int normalizedValue, int screenTall = -1 );
	int GetProportionalNormalizedValue( int scaledValue ){ return 1; }

	// first scheme loaded becomes the default scheme, and all subsequent loaded scheme are derivitives of that
	IGameUIScheme * LoadSchemeFromFileEx( const char *pFilename, const char *tag );
	// gets the proportional coordinates for doing screen-size independant panel layouts
	// use these for font, image and panel size scaling (they all use the pixel height of the display for scaling)
	int GetProportionalScaledValueEx( IGameUIScheme * scheme, int normalizedValue, int screenTall = -1 ){ return 0; }
	int GetProportionalNormalizedValueEx( IGameUIScheme * scheme, int scaledValue ){ return 0; }

	// gets the proportional coordinates for doing screen-size independant panel layouts
	// use these for font, image and panel size scaling (they all use the pixel height of the display for scaling)
	int GetProportionalScaledValueEx( CGameUIScheme *pScheme, int normalizedValue, int screenTall = -1 );
	int GetProportionalNormalizedValueEx( CGameUIScheme *pScheme, int scaledValue );

	void SpewFonts();

	void SetLanguage( const char *pLanguage );
	const char *GetLanguage();

private:

	int GetProportionalScaledValue_( int screenTall, int normalizedValue );

	// Search for already-loaded schemes
	IGameUIScheme * FindLoadedScheme(const char *pFilename);

	CUtlVector< CGameUIScheme * > m_Schemes;
};

extern CGameUISchemeManager *g_pGameUISchemeManager;



#endif // GAMEUISCHEME_H
