//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef ISCHEME_H
#define ISCHEME_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui/vgui.h"
#include "tier1/interface.h"
#include "tier1/utlsymbol.h"

class Color;
class KeyValues;
class ISchemeSurface;

namespace vgui
{

typedef unsigned long HScheme;
typedef unsigned long HTexture;

class IBorder;
class IImage;


//-----------------------------------------------------------------------------
// Purpose: Holds all panel rendering data
//			This functionality is all wrapped in the Panel::GetScheme*() functions
//-----------------------------------------------------------------------------
class IScheme : public IBaseInterface
{
public:
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

	// gets a string from the default settings section
	virtual const char *GetResourceString(const char *stringName) = 0;

	// returns a pointer to an existing border
	virtual IBorder *GetBorder(const char *borderName) = 0;

	// returns a pointer to an existing font
	virtual HFont GetFont(const char *fontName, bool proportional = false) = 0;

	// inverse font lookup
	virtual char const *GetFontName( const HFont& font ) = 0;

	// colors
	virtual Color GetColor(const char *colorName, Color defaultColor) = 0;

	// Gets at the scheme's short name
	virtual const char *GetName() const = 0;
	// Gets at the scheme's resource file name
	virtual const char *GetFileName() const = 0;
};



class ISchemeManager: public IBaseInterface
{
public:
	// loads a scheme from a file
	// first scheme loaded becomes the default scheme, and all subsequent loaded scheme are derivitives of that
	virtual HScheme LoadSchemeFromFile(const char *fileName, const char *tag) = 0;

	// reloads the scheme from the file - should only be used during development
	virtual void ReloadSchemes() = 0;

	// reloads scheme fonts
	virtual void ReloadFonts( int inScreenTall = -1 ) = 0;

	// returns a handle to the default (first loaded) scheme
	virtual HScheme GetDefaultScheme() = 0;

	// returns a handle to the scheme identified by "tag"
	virtual HScheme GetScheme(const char *tag) = 0;

	// returns a pointer to an image
	virtual IImage *GetImage(const char *imageName, bool hardwareFiltered) = 0;
	virtual HTexture GetImageID(const char *imageName, bool hardwareFiltered) = 0;

	// This can only be called at certain times, like during paint()
	// It will assert-fail if you call it at the wrong time...

	// FIXME: This interface should go away!!! It's an icky back-door
	// If you're using this interface, try instead to cache off the information
	// in ApplySchemeSettings
	virtual IScheme *GetIScheme( HScheme scheme ) = 0;

	// unload all schemes
	virtual void Shutdown( bool full = true ) = 0;

	// gets the proportional coordinates for doing screen-size independant panel layouts
	// use these for font, image and panel size scaling (they all use the pixel height of the display for scaling)
	virtual int GetProportionalScaledValue( int normalizedValue) = 0;
	virtual int GetProportionalNormalizedValue(int scaledValue) = 0;

	// loads a scheme from a file
	// first scheme loaded becomes the default scheme, and all subsequent loaded scheme are derivitives of that
	virtual HScheme LoadSchemeFromFileEx( VPANEL sizingPanel, const char *fileName, const char *tag) = 0;
	// gets the proportional coordinates for doing screen-size independant panel layouts
	// use these for font, image and panel size scaling (they all use the pixel height of the display for scaling)
	virtual int GetProportionalScaledValueEx( HScheme scheme, int normalizedValue ) = 0;
	virtual int GetProportionalNormalizedValueEx( HScheme scheme, int scaledValue ) = 0;

	// Returns true if image evicted, false otherwise
	virtual bool DeleteImage( const char *pImageName ) = 0;

	virtual ISchemeSurface *GetSurface() = 0;

	virtual void SetLanguage( const char *pLanguage ) = 0;
	virtual const char *GetLanguage() = 0;
};


} // namespace vgui


#endif // ISCHEME_H
