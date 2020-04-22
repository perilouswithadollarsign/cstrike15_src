//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include <stdio.h>
#include <math.h>

#include "vgui/IScheme.h"
#include "vgui_surfacelib/fontmanager.h"
#include "vgui/vgui.h"

#include "keyvalues.h"
#include "vgui/ISurface.h"
#include "vgui/IPanel.h"
#include "vgui/ISystem.h"
#include "vstdlib/ikeyvaluessystem.h"

#include "vgui/ILocalize.h"
#include "utlvector.h"
#include "utlrbtree.h"
#include "VGUI_Border.h"
#include "ScalableImageBorder.h"
#include "ImageBorder.h"
#include "vgui_internal.h"
#include "bitmap.h"
#include "filesystem.h"
#include "generichash.h"
#include "vprof.h"
#include "utlsortvector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;
#define FONT_ALIAS_NAME_LENGTH 64

//-----------------------------------------------------------------------------
// Purpose: Implementation of global scheme interface
//-----------------------------------------------------------------------------
class CScheme : public IScheme
{
public:
	CScheme();

	// gets a string from the default settings section
	virtual const char *GetResourceString(const char *stringName);

	// returns a pointer to an existing border
	virtual IBorder *GetBorder(const char *borderName);

	// returns a pointer to an existing font
	virtual HFont GetFont(const char *fontName, bool proportional);

	// m_pkvColors
	virtual Color GetColor( const char *colorName, Color defaultColor);

	virtual void ReloadFontGlyphs( int inScreenTall = -1 );

	void Shutdown( bool full );
	void LoadFromFile( VPANEL sizingPanel, const char *pFilename, const char *tag, KeyValues *inKeys );

	// Gets at the scheme's name
	const char *GetName() const { return m_tag.String(); }
	const char *GetFileName() const { return m_fileName.String(); }

	char const *GetFontName( const HFont& font );

	VPANEL		GetSizingPanel() { return m_SizingPanel; }

	void SpewFonts();

	bool GetFontRange( const char *fontname, int &nMin, int &nMax );
	void SetFontRange( const char *fontname, int nMin, int nMax );

	void SetActive( bool bActive );
	bool IsActive() const;

private:
	const char *LookupSchemeSetting(const char *pchSetting);
	const char *GetMungedFontName( const char *fontName, const char *scheme, bool proportional );
	void LoadFonts();
	void AddFontHelper( const char *kvfontname,  bool proportional ); // a small helper func to LoadFonts (simplifies a loop there)
	void LoadBorders();
	HFont FindFontInAliasList( const char *fontName );
	int GetMinimumFontHeightForCurrentLanguage( const char *pLanguage = NULL );

	void	AddCriticalFont( const char *pFontName, KeyValues *pKV );
	void	PrecacheCriticalFontGlyphs( const char *pLanguage );

	bool		m_bActive;
	CUtlString m_fileName;
	CUtlString m_tag;

	KeyValues *m_pData;
	KeyValues *m_pkvBaseSettings;
	KeyValues *m_pkvColors;

	struct SchemeBorder_t
	{
		IBorder *border;
		int borderSymbol;
		bool bSharedBorder;
	};
	CUtlVector<SchemeBorder_t> m_BorderList;
	IBorder  *m_pBaseBorder;	// default border to use if others not found
	KeyValues *m_pkvBorders;

	CUtlVector<fontalias_t>	m_FontAliases;
	VPANEL m_SizingPanel;
	int			m_nScreenWide;
	int			m_nScreenTall;
	unsigned int	m_nLastLoadedLanguage; ///< hash off the last language we loaded, so we don't load it again redundantly.

	CUtlVector<fontrange_t> m_FontRanges;

	struct CriticalFont_t
	{
		CriticalFont_t()
		{
			m_bPrecached = false;
			m_bCommonChars = false;
			m_bUppercase = false;
			m_bLowercase = false;
			m_bNumbers = false;
			m_bPunctuation = false;
			m_bExtendedChars = false;
			m_bAsianChars = false;
			m_bSkipIfAsian = true;
			m_bRussianChars = false;
		}

		bool		m_bPrecached;
		bool		m_bCommonChars;
		bool		m_bUppercase;
		bool		m_bLowercase;
		bool		m_bNumbers;
		bool		m_bPunctuation;
		bool		m_bExtendedChars;
		bool		m_bAsianChars;
		bool		m_bSkipIfAsian;
		bool		m_bRussianChars;

		CUtlString	m_FontName;
		CUtlString	m_ExplicitChars;

	};
	CUtlVector< CriticalFont_t > m_CriticalFonts;

	struct CriticalFontMap_t
	{
		CriticalFont_t *m_pCriticalFont;
		HFont			m_hFont;
		int				m_nTall;
	};

	class CCriticalFontLess
	{
	public:
		bool Less( const CriticalFontMap_t &src1, const CriticalFontMap_t &src2, void *pCtx );
	};

};

//-----------------------------------------------------------------------------
// Purpose: Implementation of global scheme interface
//-----------------------------------------------------------------------------
class CSchemeManager : public ISchemeManager
{
public:
	CSchemeManager();
	~CSchemeManager();

	// loads a scheme from a file
	// first scheme loaded becomes the default scheme, and all subsequent loaded scheme are derivitives of that
	// tag is friendly string representing the name of the loaded scheme
	virtual HScheme LoadSchemeFromFile(const char *pFilename, const char *tag);
	// first scheme loaded becomes the default scheme, and all subsequent loaded scheme are derivitives of that
	virtual HScheme LoadSchemeFromFileEx( VPANEL sizingPanel, const char *fileName, const char *tag);

	// reloads the schemes from the file
	virtual void ReloadSchemes();
	virtual void ReloadFonts( int inScreenTall = -1 );

	// returns a handle to the default (first loaded) scheme
	virtual HScheme GetDefaultScheme();

	// returns a handle to the scheme identified by "tag"
	virtual HScheme GetScheme(const char *tag);

	// returns a pointer to an image
	virtual IImage *GetImage(const char *imageName, bool hardwareFiltered);
	virtual HTexture GetImageID(const char *imageName, bool hardwareFiltered);

	virtual IScheme *GetIScheme( HScheme scheme );

	virtual void Shutdown( bool full );

	// gets the proportional coordinates for doing screen-size independant panel layouts
	// use these for font, image and panel size scaling (they all use the pixel height of the display for scaling)
	virtual int GetProportionalScaledValue( int normalizedValue );
	virtual int GetProportionalNormalizedValue( int scaledValue );

	// gets the proportional coordinates for doing screen-size independant panel layouts
	// use these for font, image and panel size scaling (they all use the pixel height of the display for scaling)
	virtual int GetProportionalScaledValueEx( HScheme scheme, int normalizedValue );
	virtual int GetProportionalNormalizedValueEx( HScheme scheme, int scaledValue );

	virtual bool DeleteImage( const char *pImageName );

	// gets the proportional coordinates for doing screen-size independant panel layouts
	// use these for font, image and panel size scaling (they all use the pixel height of the display for scaling)
	int GetProportionalScaledValueEx( CScheme *pScheme, int normalizedValue );
	int GetProportionalNormalizedValueEx( CScheme *pScheme, int scaledValue );

	void SpewFonts();

	virtual ISchemeSurface *GetSurface(); 

	virtual void SetLanguage( const char *pLanguage );
	virtual const char *GetLanguage();

private:

	int GetProportionalScaledValue_( int rootWide, int rootTall, int normalizedValue );
	int GetProportionalNormalizedValue_( int rootWide, int rootTall, int scaledValue );

	// Search for already-loaded schemes
	HScheme FindLoadedScheme(const char *fileName);

	CUtlVector<CScheme *> m_Schemes;

	static const char *s_pszSearchString;
	struct CachedBitmapHandle_t
	{
		Bitmap *pBitmap;
	};
	static bool BitmapHandleSearchFunc(const CachedBitmapHandle_t &, const CachedBitmapHandle_t &);
	CUtlRBTree<CachedBitmapHandle_t, int> m_Bitmaps;
};

const char *CSchemeManager::s_pszSearchString = NULL;

//-----------------------------------------------------------------------------
// Purpose: search function for stored bitmaps
//-----------------------------------------------------------------------------
bool CSchemeManager::BitmapHandleSearchFunc(const CachedBitmapHandle_t &lhs, const CachedBitmapHandle_t &rhs)
{
	// a NULL bitmap indicates to use the search string instead
	if (lhs.pBitmap && rhs.pBitmap)
	{
		return stricmp(lhs.pBitmap->GetName(), rhs.pBitmap->GetName()) > 0;
	}
	else if (lhs.pBitmap)
	{
		return stricmp(lhs.pBitmap->GetName(), s_pszSearchString) > 0;
	}
	return stricmp(s_pszSearchString, rhs.pBitmap->GetName()) > 0;
}



CSchemeManager g_Scheme;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CSchemeManager, ISchemeManager, VGUI_SCHEME_INTERFACE_VERSION, g_Scheme);


namespace vgui
{
vgui::ISchemeManager *g_pScheme = &g_Scheme;
vgui::ISchemeManager *g_pSchemeManager = &g_Scheme; // a much better name for this global, used inside this file only for now.
} // namespace vgui

CON_COMMAND( vgui_spew_fonts, "" )
{
	g_Scheme.SpewFonts();
}
 
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CSchemeManager::CSchemeManager()
{
	// 0th element is null, since that would be an invalid handle
	CScheme *nullScheme = new CScheme();
	m_Schemes.AddToTail(nullScheme);
	m_Bitmaps.SetLessFunc(&BitmapHandleSearchFunc);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CSchemeManager::~CSchemeManager()
{
	int i;
	for ( i = 0; i < m_Schemes.Count(); i++ )
	{
		delete m_Schemes[i];
	}
	m_Schemes.RemoveAll();

	for ( i = 0; i < m_Bitmaps.MaxElement(); i++ )
	{
		if (m_Bitmaps.IsValidIndex(i))
		{
			delete m_Bitmaps[i].pBitmap;
		}
	}
	m_Bitmaps.RemoveAll();

	Shutdown( false );
}

//-----------------------------------------------------------------------------
// Purpose: reloads the scheme from the file
//-----------------------------------------------------------------------------
void CSchemeManager::ReloadSchemes()
{
	int count = m_Schemes.Count();
	Shutdown( false );
	
	// reload the scheme
	for (int i = 1; i < count; i++)
	{
		LoadSchemeFromFile(m_Schemes[i]->GetFileName(), m_Schemes[i]->GetName());
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reload the fonts in all schemes
//-----------------------------------------------------------------------------
void CSchemeManager::ReloadFonts( int inScreenTall )
{
	for (int i = 1; i < m_Schemes.Count(); i++)
	{
		m_Schemes[i]->ReloadFontGlyphs( inScreenTall );
	}
}

//-----------------------------------------------------------------------------
// Converts the handle into an interface
//-----------------------------------------------------------------------------
IScheme *CSchemeManager::GetIScheme( HScheme scheme )
{
	if ( scheme >= (unsigned long)m_Schemes.Count() )
	{
		AssertOnce( !"Invalid scheme requested." );
		return NULL;
	}
	else
	{
		return m_Schemes[scheme];
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSchemeManager::Shutdown( bool full )
{
	// Full shutdown kills the null scheme
	for( int i = full ? 0 : 1; i < m_Schemes.Count(); i++ )
	{
		m_Schemes[i]->Shutdown( full );
	}

	if ( full )
	{
		m_Schemes.RemoveAll();
	}
}

//-----------------------------------------------------------------------------
// Purpose: loads a scheme from disk
//-----------------------------------------------------------------------------
HScheme CSchemeManager::FindLoadedScheme( const char *pFilename )
{
	VPROF_2( "CSchemeManager::FindLoadedScheme", VPROF_BUDGETGROUP_OTHER_VGUI, false, 0 );

	// normalize the filename, can't trust outside callers or scripts
	// otherwise silly duplicate build out occurs
	char fileName[MAX_PATH];
	Q_strncpy( fileName, pFilename, sizeof( fileName ) );
	V_FixSlashes( fileName );

	// Find the scheme in the list of already loaded schemes
	for (int i = 1; i < m_Schemes.Count(); i++)
	{
		char const *schemeFileName = m_Schemes[i]->GetFileName();
		if ( !V_stricmp( schemeFileName, fileName ) )
		{
			return i;
		}
	}

	return 0;
}

// first scheme loaded becomes the default scheme, and all subsequent loaded scheme are derivitives of that
HScheme CSchemeManager::LoadSchemeFromFileEx( VPANEL sizingPanel, const char *pFilename, const char *tag)
{
	VPROF("CSchemeManager::LoadSchemeFromFile");

	// Look to see if we've already got this scheme...
	HScheme hScheme = FindLoadedScheme( pFilename );
	CScheme *pScheme = NULL;

	if ( hScheme != 0 )
	{
		pScheme = m_Schemes[ hScheme ];
		if ( pScheme->IsActive() )
		{
			// found active scheme
			if ( IsPC() )
			{
				pScheme->ReloadFontGlyphs();
			}
			return hScheme;
		}
	}
	
	KeyValues *pKVData = new KeyValues( "Scheme" );

	pKVData->UsesEscapeSequences( true );	// VGUI uses this
	
	{
		VPROF_2( "CSchemeManager::LoadSchemeFromFileEx -> LoadFromFile", VPROF_BUDGETGROUP_OTHER_VGUI, false, 0 );

		// look first in skins directory
		bool bResult = pKVData->LoadFromFile( g_pFullFileSystem, pFilename, "SKIN" );
		if ( !bResult )
		{
			bResult = pKVData->LoadFromFile( g_pFullFileSystem, pFilename, "GAME" );
			if ( !bResult )
			{
				// look in any directory
				bResult = pKVData->LoadFromFile( g_pFullFileSystem, pFilename, NULL );
			}
		}

		if ( !bResult )
		{
			pKVData->deleteThis();
			return 0;
		}
	}
	
	if ( hScheme == 0 )
	{
		// not using an existing inactive scheme
		// add new scheme
		pScheme = new CScheme();
		hScheme = m_Schemes.AddToTail( pScheme );
	}

	if ( IsPC() )
	{
		ConVarRef cl_hud_minmode( "cl_hud_minmode", true );
		if ( cl_hud_minmode.IsValid() && cl_hud_minmode.GetBool() )
		{
			pKVData->ProcessResolutionKeys( "_minmode" );
		}
	}

	{
		VPROF_2( "pScheme->LoadFromFile", VPROF_BUDGETGROUP_OTHER_VGUI, false, 0 );
		pScheme->SetActive( true );
		pScheme->LoadFromFile( sizingPanel, pFilename, tag, pKVData );
	}

	return hScheme;
}

//-----------------------------------------------------------------------------
// Purpose: loads a scheme from disk
//-----------------------------------------------------------------------------
HScheme CSchemeManager::LoadSchemeFromFile(const char *fileName, const char *tag)
{
	return LoadSchemeFromFileEx( 0, fileName, tag );
}

//-----------------------------------------------------------------------------
// Purpose: returns a handle to the default (first loaded) scheme
//-----------------------------------------------------------------------------
HScheme CSchemeManager::GetDefaultScheme()
{
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: returns a handle to the scheme identified by "tag"
//-----------------------------------------------------------------------------
HScheme CSchemeManager::GetScheme(const char *tag)
{
	for (int i=1;i<m_Schemes.Count();i++)
	{
		if ( !stricmp(tag,m_Schemes[i]->GetName()) )
		{
			return i;
		}
	}
	return 1; // default scheme
}

int CSchemeManager::GetProportionalScaledValue_( int rootWide, int rootTall, int normalizedValue )
{
	int proH, proW;
	GetSurface()->GetProportionalBase( proW, proH );
	double scale = (double)rootTall / (double)proH;

	return (int)( normalizedValue * scale );
}

int CSchemeManager::GetProportionalNormalizedValue_( int rootWide, int rootTall, int scaledValue )
{
	int proH, proW;
	GetSurface()->GetProportionalBase( proW, proH );
	float scale = (float)rootTall / (float)proH;

	return (int)( scaledValue / scale );
}

//-----------------------------------------------------------------------------
// Purpose: converts a value into proportional mode
//-----------------------------------------------------------------------------
int CSchemeManager::GetProportionalScaledValue(int normalizedValue)
{
	int wide, tall;
	GetSurface()->GetScreenSize( wide, tall );
	return GetProportionalScaledValue_( wide, tall, normalizedValue );
}

//-----------------------------------------------------------------------------
// Purpose: converts a value out of proportional mode
//-----------------------------------------------------------------------------
int CSchemeManager::GetProportionalNormalizedValue(int scaledValue)
{
	int wide, tall;
	GetSurface()->GetScreenSize( wide, tall );
	return GetProportionalNormalizedValue_( wide, tall, scaledValue );
}

// gets the proportional coordinates for doing screen-size independant panel layouts
// use these for font, image and panel size scaling (they all use the pixel height of the display for scaling)
int CSchemeManager::GetProportionalScaledValueEx( CScheme *pScheme, int normalizedValue )
{
	VPANEL sizing = pScheme->GetSizingPanel();
	if ( !sizing )
	{
		return GetProportionalScaledValue( normalizedValue );
	}

	int w, h;
	g_pIPanel->GetSize( sizing, w, h );
	return GetProportionalScaledValue_( w, h, normalizedValue );
}

int CSchemeManager::GetProportionalNormalizedValueEx( CScheme *pScheme, int scaledValue )
{
	VPANEL sizing = pScheme->GetSizingPanel();
	if ( !sizing )
	{
		return GetProportionalNormalizedValue( scaledValue );
	}

	int w, h;
	g_pIPanel->GetSize( sizing, w, h );
	return GetProportionalNormalizedValue_( w, h, scaledValue );
}

int CSchemeManager::GetProportionalScaledValueEx( HScheme scheme, int normalizedValue )
{
	IScheme *pscheme = GetIScheme( scheme );
	if ( !pscheme )
	{
		Assert( 0 );
		return GetProportionalScaledValue( normalizedValue );
	}

	CScheme *p = static_cast< CScheme * >( pscheme );
	return GetProportionalScaledValueEx( p, normalizedValue );
}

int CSchemeManager::GetProportionalNormalizedValueEx( HScheme scheme, int scaledValue )
{
	IScheme *pscheme = GetIScheme( scheme );
	if ( !pscheme )
	{
		Assert( 0 );
		return GetProportionalNormalizedValue( scaledValue );
	}

	CScheme *p = static_cast< CScheme * >( pscheme );
	return GetProportionalNormalizedValueEx( p, scaledValue );
}

void CSchemeManager::SpewFonts( void )
{
	for ( int i = 1; i < m_Schemes.Count(); i++ )
	{
		m_Schemes[i]->SpewFonts();
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to an image
//-----------------------------------------------------------------------------
IImage *CSchemeManager::GetImage(const char *imageName, bool hardwareFiltered)
{
	if ( !imageName || strlen(imageName) <= 0 ) // frame icons and the like are in the scheme file and may not be defined, so if this is null then fail silently
	{
		return NULL; 
	}

	// set up to search for the bitmap
	CachedBitmapHandle_t searchBitmap;
	searchBitmap.pBitmap = NULL;

	// Prepend 'vgui/'. Resource files try to load images assuming they live in the vgui directory.
	// Used to do this in Bitmap::Bitmap, moved so that the s_pszSearchString is searching for the
	// filename with 'vgui/' already added.
	char szFileName[256];
	if ( Q_stristr( imageName, ".pic" ) )
	{
		Q_snprintf( szFileName, sizeof(szFileName), "%s", imageName );
	}
	else
	{
		Q_snprintf( szFileName, sizeof(szFileName), "vgui/%s", imageName );
	}

	s_pszSearchString = szFileName;
	int i = m_Bitmaps.Find( searchBitmap );
	if (m_Bitmaps.IsValidIndex( i ) )
	{
		return m_Bitmaps[i].pBitmap;
	}

	// couldn't find the image, try and load it
	CachedBitmapHandle_t hBitmap = { new Bitmap( szFileName, hardwareFiltered ) };
	m_Bitmaps.Insert( hBitmap );
	return hBitmap.pBitmap;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
HTexture CSchemeManager::GetImageID(const char *imageName, bool hardwareFiltered)
{
	IImage *img = GetImage(imageName, hardwareFiltered);
	return ((Bitmap *)img)->GetID();
}

//-----------------------------------------------------------------------------
// Delete a managed image
//-----------------------------------------------------------------------------
bool CSchemeManager::DeleteImage( const char *pImageName )
{
	if ( !pImageName )
	{
		// nothing to do
		return false;
	}

	// set up to search for the bitmap
	CachedBitmapHandle_t searchBitmap;
	searchBitmap.pBitmap = NULL;

	// Prepend 'vgui/'. Resource files try to load images assuming they live in the vgui directory.
	// Used to do this in Bitmap::Bitmap, moved so that the s_pszSearchString is searching for the
	// filename with 'vgui/' already added.
	char szFileName[256];
	if ( Q_stristr( pImageName, ".pic" ) )
	{
		Q_snprintf( szFileName, sizeof(szFileName), "%s", pImageName );
	}
	else
	{
		Q_snprintf( szFileName, sizeof(szFileName), "vgui/%s", pImageName );
	}
	s_pszSearchString = szFileName;

	int i = m_Bitmaps.Find( searchBitmap );
	if ( !m_Bitmaps.IsValidIndex( i ) )
	{
		// not found
		return false;
	}
		
	// no way to know if eviction occured, assume it does
	m_Bitmaps[i].pBitmap->Evict();
	delete  m_Bitmaps[i].pBitmap;	
	m_Bitmaps.RemoveAt( i );

	return true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
ISchemeSurface *CSchemeManager::GetSurface() 
{ 
	return g_pSchemeSurface; 
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CSchemeManager::SetLanguage( const char *pLanguage )
{ 
	GetSurface()->SetLanguage( pLanguage );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CSchemeManager::GetLanguage()
{ 
	return GetSurface()->GetLanguage();
}

//-----------------------------------------------------------------------------
// Purpose: Table of scheme file entries, translation from old goldsrc schemes to new src schemes
//-----------------------------------------------------------------------------
struct SchemeEntryTranslation_t
{
	const char *pchNewEntry;
	const char *pchOldEntry;
	const char *pchDefaultValue;
};
SchemeEntryTranslation_t g_SchemeTranslation[] =
{
	{ "Border.Bright",					"BorderBright",		"200 200 200 196" },	// the lit side of a control
	{ "Border.Dark"						"BorderDark",		"40 40 40 196" },		// the dark/unlit side of a control
	{ "Border.Selection"				"BorderSelection",	"0 0 0 196" },			// the additional border color for displaying the default/selected button

	{ "Button.TextColor",				"ControlFG",		"White" },
	{ "Button.BgColor",					"ControlBG",		"Blank" },
	{ "Button.ArmedTextColor",			"ControlFG" },
	{ "Button.ArmedBgColor",			"ControlBG" },
	{ "Button.DepressedTextColor",		"ControlFG" },
	{ "Button.DepressedBgColor",		"ControlBG" },
	{ "Button.FocusBorderColor",		"0 0 0 255" },

	{ "CheckButton.TextColor",			"BaseText" },
	{ "CheckButton.SelectedTextColor",	"BrightControlText" },
	{ "CheckButton.BgColor",			"CheckBgColor" },
	{ "CheckButton.Border1",  			"CheckButtonBorder1" },
	{ "CheckButton.Border2",  			"CheckButtonBorder2" },
	{ "CheckButton.Check",				"CheckButtonCheck" },

	{ "ComboBoxButton.ArrowColor",		"LabelDimText" },
	{ "ComboBoxButton.ArmedArrowColor",	"MenuButton/ArmedArrowColor" },
	{ "ComboBoxButton.BgColor",			"MenuButton/ButtonBgColor" },
	{ "ComboBoxButton.DisabledBgColor",	"ControlBG" },

	{ "Frame.TitleTextInsetX",			NULL,		"32" },
	{ "Frame.ClientInsetX",				NULL,		"8" },
	{ "Frame.ClientInsetY",				NULL,		"6" },
	{ "Frame.BgColor",					"BgColor" },
	{ "Frame.OutOfFocusBgColor",		"BgColor" },
	{ "Frame.FocusTransitionEffectTime",NULL,		"0" },
	{ "Frame.TransitionEffectTime",		NULL,		"0" },
	{ "Frame.AutoSnapRange",			NULL,		"8" },
	{ "FrameGrip.Color1",				"BorderBright" },
	{ "FrameGrip.Color2",				"BorderSelection" },
	{ "FrameTitleButton.FgColor",		"TitleButtonFgColor" },
	{ "FrameTitleButton.BgColor",		"TitleButtonBgColor" },
	{ "FrameTitleButton.DisabledFgColor",	"TitleButtonDisabledFgColor" },
	{ "FrameTitleButton.DisabledBgColor",	"TitleButtonDisabledBgColor" },
	{ "FrameSystemButton.FgColor",		"TitleBarBgColor" },
	{ "FrameSystemButton.BgColor",		"TitleBarBgColor" },
	{ "FrameSystemButton.Icon",			"TitleBarIcon" },
	{ "FrameSystemButton.DisabledIcon",	"TitleBarDisabledIcon" },
	{ "FrameTitleBar.Font",				NULL,		"Default" },
	{ "FrameTitleBar.TextColor",		"TitleBarFgColor" },
	{ "FrameTitleBar.BgColor",			"TitleBarBgColor" },
	{ "FrameTitleBar.DisabledTextColor","TitleBarDisabledFgColor" },
	{ "FrameTitleBar.DisabledBgColor",	"TitleBarDisabledBgColor" },

	{ "GraphPanel.FgColor",				"BrightControlText" },
	{ "GraphPanel.BgColor",				"WindowBgColor" },

	{ "Label.TextDullColor",			"LabelDimText" },
	{ "Label.TextColor",				"BaseText" },
	{ "Label.TextBrightColor",			"BrightControlText" },
	{ "Label.SelectedTextColor",		"BrightControlText" },
	{ "Label.BgColor",					"LabelBgColor" },
	{ "Label.DisabledFgColor1",			"DisabledFgColor1" },
	{ "Label.DisabledFgColor2",			"DisabledFgColor2" },

	{ "ListPanel.TextColor",				"WindowFgColor" },
	{ "ListPanel.TextBgColor",				"Menu/ArmedBgColor" },
	{ "ListPanel.BgColor",					"ListBgColor" },
	{ "ListPanel.SelectedTextColor",		"ListSelectionFgColor" },
	{ "ListPanel.SelectedBgColor",			"Menu/ArmedBgColor" },
	{ "ListPanel.SelectedOutOfFocusBgColor","SelectionBG2" },
	{ "ListPanel.EmptyListInfoTextColor",	"LabelDimText" },
	{ "ListPanel.DisabledTextColor",		"LabelDimText" },
	{ "ListPanel.DisabledSelectedTextColor","ListBgColor" },

	{ "Menu.TextColor",					"Menu/FgColor" },
	{ "Menu.BgColor",					"Menu/BgColor" },
	{ "Menu.ArmedTextColor",			"Menu/ArmedFgColor" },
	{ "Menu.ArmedBgColor",				"Menu/ArmedBgColor" },
	{ "Menu.TextInset",					NULL,		"6" },

	{ "Panel.FgColor",					"FgColor" },
	{ "Panel.BgColor",					"BgColor" },

	{ "ProgressBar.FgColor",				"BrightControlText" },
	{ "ProgressBar.BgColor",				"WindowBgColor" },

	{ "PropertySheet.TextColor",			"FgColorDim" },
	{ "PropertySheet.SelectedTextColor",	"BrightControlText" },
	{ "PropertySheet.TransitionEffectTime",	NULL,		"0" },

	{ "RadioButton.TextColor",			"FgColor" },
	{ "RadioButton.SelectedTextColor",	"BrightControlText" },

	{ "RichText.TextColor",				"WindowFgColor" },
	{ "RichText.BgColor",				"WindowBgColor" },
	{ "RichText.SelectedTextColor",		"SelectionFgColor" },
	{ "RichText.SelectedBgColor",		"SelectionBgColor" },

	{ "ScrollBar.Wide",					NULL,		"19" },

	{ "ScrollBarButton.FgColor",			"DimBaseText" },
	{ "ScrollBarButton.BgColor",			"ControlBG" },
	{ "ScrollBarButton.ArmedFgColor",		"BaseText" },
	{ "ScrollBarButton.ArmedBgColor",		"ControlBG" },
	{ "ScrollBarButton.DepressedFgColor",	"BaseText" },
	{ "ScrollBarButton.DepressedBgColor",	"ControlBG" },

	{ "ScrollBarSlider.FgColor",				"ScrollBarSlider/ScrollBarSliderFgColor" },
	{ "ScrollBarSlider.BgColor",				"ScrollBarSlider/ScrollBarSliderBgColor" },

	{ "SectionedListPanel.HeaderTextColor",	"SectionTextColor" },
	{ "SectionedListPanel.HeaderBgColor",	"BuddyListBgColor" },
	{ "SectionedListPanel.DividerColor",	"SectionDividerColor" },
	{ "SectionedListPanel.TextColor",		"BuddyButton/FgColor1" },
	{ "SectionedListPanel.BrightTextColor",	"BuddyButton/ArmedFgColor1" },
	{ "SectionedListPanel.BgColor",			"BuddyListBgColor" },
	{ "SectionedListPanel.SelectedTextColor",			"BuddyButton/ArmedFgColor1" },
	{ "SectionedListPanel.SelectedBgColor",				"BuddyButton/ArmedBgColor" },
	{ "SectionedListPanel.OutOfFocusSelectedTextColor",	"BuddyButton/ArmedFgColor2" },
	{ "SectionedListPanel.OutOfFocusSelectedBgColor",	"SelectionBG2" },

	{ "Slider.NobColor",			"SliderTickColor" },
	{ "Slider.TextColor",			"Slider/SliderFgColor" },
	{ "Slider.TrackColor",			"SliderTrackColor"},
	{ "Slider.DisabledTextColor1",	"DisabledFgColor1" },
	{ "Slider.DisabledTextColor2",	"DisabledFgColor2" },

	{ "TextEntry.TextColor",		"WindowFgColor" },
	{ "TextEntry.BgColor",			"WindowBgColor" },
	{ "TextEntry.CursorColor",		"TextCursorColor" },
	{ "TextEntry.DisabledTextColor","WindowDisabledFgColor" },
	{ "TextEntry.DisabledBgColor",	"ControlBG" },
	{ "TextEntry.SelectedTextColor","SelectionFgColor" },
	{ "TextEntry.SelectedBgColor",	"SelectionBgColor" },
	{ "TextEntry.OutOfFocusSelectedBgColor",	"SelectionBG2" },
	{ "TextEntry.FocusEdgeColor",	"BorderSelection" },

	{ "ToggleButton.SelectedTextColor",	"BrightControlText" },

	{ "Tooltip.TextColor",			"BorderSelection" },
	{ "Tooltip.BgColor",			"SelectionBG" },

	{ "TreeView.BgColor",			"ListBgColor" },

	{ "WizardSubPanel.BgColor",		"SubPanelBgColor" },
};

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CScheme::CScheme()
{
	m_pData = NULL;
	m_pkvBaseSettings = NULL;
	m_pkvColors = NULL;

	m_pBaseBorder = NULL;	// default border to use if others not found
	m_pkvBorders = NULL;
	m_SizingPanel = 0;
	m_nScreenWide = -1;
	m_nScreenTall = -1;
	m_nLastLoadedLanguage = 0;

	SetActive( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *CScheme::GetResourceString(const char *stringName)
{
	return m_pkvBaseSettings->GetString(stringName);
}


//-----------------------------------------------------------------------------
// Purpose: loads a scheme from from disk into memory
//-----------------------------------------------------------------------------
void CScheme::LoadFromFile( VPANEL sizingPanel, const char *pFilename, const char *inTag, KeyValues *inKeys )
{
	COM_TimestampedLog( "CScheme::LoadFromFile( %s )", pFilename );

	// the filename is user for lookup comparison purposes
	// must store it normalized
	m_fileName = pFilename;
	V_FixSlashes( m_fileName.Get() );
	
	m_SizingPanel = sizingPanel;

	m_pData = inKeys;
	m_pkvBaseSettings = m_pData->FindKey("BaseSettings", true);
	m_pkvColors = m_pData->FindKey("Colors", true);

	// override the scheme name with the tag name
	KeyValues *name = m_pData->FindKey("Name", true);
	name->SetString("Name", inTag);

	if ( inTag )
	{
		m_tag = inTag;
	}
	else
	{
		Error( "You need to name the scheme (%s)!", m_fileName.String() );
		m_tag = "default";
	}

	// translate format from goldsrc scheme to new scheme
	for (int i = 0; i < ARRAYSIZE(g_SchemeTranslation); i++)
	{
		if (!m_pkvBaseSettings->FindKey(g_SchemeTranslation[i].pchNewEntry, false))
		{
			const char *pchColor;

			if (g_SchemeTranslation[i].pchOldEntry)
			{
				pchColor = LookupSchemeSetting(g_SchemeTranslation[i].pchOldEntry);
			}
			else
			{
				pchColor = g_SchemeTranslation[i].pchDefaultValue;
			}

			Assert( pchColor );

			m_pkvBaseSettings->SetString(g_SchemeTranslation[i].pchNewEntry, pchColor);
		}
	}

	// need to copy tag before loading fonts
	LoadFonts();
	LoadBorders();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CScheme::GetFontRange( const char *fontname, int &nMin, int &nMax )
{
	for ( int i = 0 ; i < m_FontRanges.Count() ; i++ )
	{
		if ( Q_stricmp( m_FontRanges[i]._fontName.String(), fontname ) == 0 )
		{
			nMin = m_FontRanges[i]._min;
			nMax = m_FontRanges[i]._max;
			return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CScheme::SetFontRange( const char *fontname, int nMin, int nMax )
{
	for ( int i = 0 ; i < m_FontRanges.Count() ; i++ )
	{
		if ( Q_stricmp( m_FontRanges[i]._fontName.String(), fontname ) == 0 )
		{
			m_FontRanges[i]._min = nMin;
			m_FontRanges[i]._max = nMax;

			return;
		}
	}

	// not already in our list
	int iIndex = m_FontRanges.AddToTail();
	
	m_FontRanges[iIndex]._fontName = fontname;
	m_FontRanges[iIndex]._min = nMin;
	m_FontRanges[iIndex]._max = nMax;
}

//-----------------------------------------------------------------------------
// Purpose: adds all the font specifications to the surface
//-----------------------------------------------------------------------------
void CScheme::LoadFonts()
{
	char language[64];
	bool bValid = false;

	// get our language
	if ( IsPC() )
	{
		bValid = vgui::g_pSystem->GetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Language", language, sizeof( language ) - 1 );
	}
	else if ( IsGameConsole() )
	{
		Q_strncpy( language, XBX_GetLanguageString(), sizeof( language ) );
		bValid = true;
	}

	if ( !bValid )
	{
		Q_strncpy( language, "english", sizeof( language ) );
	}

	// add our custom fonts
	for (KeyValues *kv = m_pData->FindKey("CustomFontFiles", true)->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey())
	{
		const char *fontFile = kv->GetString();
		if (fontFile && *fontFile)
		{
			g_pSchemeManager->GetSurface()->AddCustomFontFile( fontFile );
		}
		else
		{
			// we have a block to read
			int nRangeMin = 0, nRangeMax = 0;
			const char *pszName = NULL;
			bool bUseRange = false;

			for ( KeyValues *pData = kv->GetFirstSubKey(); pData != NULL; pData = pData->GetNextKey() )
			{
				const char *pszKey = pData->GetName();
				if ( !Q_stricmp( pszKey, "font" ) )
				{
					fontFile = pData->GetString();
				}
				else if ( !Q_stricmp( pszKey, "name" ) )
				{
					pszName = pData->GetString();
				}
				else
				{
					// we must have a language
					if ( Q_stricmp( language, pszKey ) == 0 ) // matches the language we're running?
					{
						// get the range
						KeyValues *pRange = pData->FindKey( "range" );
						if ( pRange )
						{
							bUseRange = true;
							sscanf( pRange->GetString(), "%x %x", &nRangeMin, &nRangeMax );

							if ( nRangeMin > nRangeMax )
							{
								int nTemp = nRangeMin;
								nRangeMin = nRangeMax;
								nRangeMax = nTemp;
							}
						}
					}
				}
			}

			if ( fontFile && *fontFile )
			{
				g_pSchemeManager->GetSurface()->AddCustomFontFile( fontFile );

				if ( bUseRange )
				{
					SetFontRange( pszName, nRangeMin, nRangeMax );
				}
			}
		}
	}

	// add bitmap fonts
	for (KeyValues *kv = m_pData->FindKey("BitmapFontFiles", true)->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey())
	{
		const char *fontFile = kv->GetString();
		if (fontFile && *fontFile)
		{
			bool bSuccess = g_pSchemeManager->GetSurface()->AddBitmapFontFile( fontFile );
			if ( bSuccess )
			{
				// refer to the font by a user specified symbol
				g_pSchemeManager->GetSurface()->SetBitmapFontName( kv->GetName(), fontFile );
			}
		}
	}

	// create the fonts
	for (KeyValues *kv = m_pData->FindKey("Fonts", true)->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey())
	{
		// check to see if the font has been specified as having ONLY a proportional or ONLY a nonproportional
		// version. (if not, the default is to make both a normal font, then a proportional one)
		const char *proportionality = kv->GetString( "isproportional" );
		if ( proportionality && proportionality[0] != 0 )
		{
			if ( V_strcmp( proportionality, "only" ) == 0 )
			{	// ONLY a proportional version
				AddFontHelper( kv->GetName(), true );
			}
			else if ( V_strcmp( proportionality, "no" ) == 0 )
			{	// ONLY a nonproportional version
				AddFontHelper( kv->GetName(), false );
			}
			else for ( int i = 0; i < 2; i++ ) // else default behavior (both) 
			{
				// create the base font
				bool proportionalFont = i ? true : false;
				AddFontHelper( kv->GetName(), proportionalFont );
			}
		}
		else for ( int i = 0; i < 2; i++ )
		{
			// create the base font
			bool proportionalFont = i ? true : false;
			AddFontHelper( kv->GetName(), proportionalFont );
			/*
			const char *fontName = GetMungedFontName( kv->GetName(), m_tag.String(), proportionalFont ); // first time it adds a normal font, and then a proportional one
			HFont font = g_pSchemeManager->GetSurface()->CreateFont();
			int j = m_FontAliases.AddToTail();
			m_FontAliases[j]._fontName = fontName;
			m_FontAliases[j]._trueFontName = kv->GetName();
			m_FontAliases[j]._font = font;
			m_FontAliases[j].m_bProportional = proportionalFont;
			*/
		}
	}

	// add critical font section
	for ( KeyValues *kv = m_pData->FindKey( "CriticalFonts", true )->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey() )
	{
		const char *pFontFile = kv->GetName();
		if ( pFontFile && *pFontFile )
		{
			AddCriticalFont( pFontFile, kv );
		}
	}

	// load in the font glyphs
	ReloadFontGlyphs();

	PrecacheCriticalFontGlyphs( language );
}

void CScheme::AddFontHelper( const char *kvfontname, bool proportional ) // a small helper func to LoadFonts (simplifies a loop there)
{
	const char *fontName = GetMungedFontName( kvfontname, m_tag.String(), proportional );
	HFont font = g_pSchemeManager->GetSurface()->CreateFont();
	int j = m_FontAliases.AddToTail();
	m_FontAliases[j]._fontName = fontName;
	m_FontAliases[j]._trueFontName = kvfontname;
	m_FontAliases[j]._font = font;
	m_FontAliases[j].m_bProportional = proportional;
}

//-----------------------------------------------------------------------------
// Purpose: Reloads all scheme fonts
//-----------------------------------------------------------------------------
void CScheme::ReloadFontGlyphs( int inScreenTall )
{
	VPROF_2( "CScheme::ReloadFontGlyphs", VPROF_BUDGETGROUP_OTHER_VGUI, false, 0 );

	COM_TimestampedLog( "ReloadFontGlyphs(): Start [%s]", GetFileName() );

	int nScreenWide, nScreenTall;
	// get our current resolution
	if ( m_SizingPanel != 0 )
	{
		g_pIPanel->GetSize( m_SizingPanel, nScreenWide, nScreenTall );
	}
	else
	{
		g_pSchemeManager->GetSurface()->GetScreenSize( nScreenWide, nScreenTall );
	}

	// if the screen resolution has changed, go on to reload the fonts. Otherwise it's redundant.
	const char * const pCurrentLanguage = g_Scheme.GetLanguage();
	unsigned int nHashOfCurrentLanguage = HashString( pCurrentLanguage );
	if ( nScreenWide == m_nScreenWide && nScreenTall == m_nScreenTall && nHashOfCurrentLanguage == m_nLastLoadedLanguage )
	{
		return;
	}
	else
	{
		m_nScreenWide = nScreenWide;
		m_nScreenTall = nScreenTall;
		m_nLastLoadedLanguage = nHashOfCurrentLanguage;
		// and keep going:
	}

	// check our language; some have minimum sizes
	int minimumFontHeight = GetMinimumFontHeightForCurrentLanguage( pCurrentLanguage );

	// add the data to all the fonts
	KeyValues *fonts = m_pData->FindKey("Fonts", true);
	for (int i = 0; i < m_FontAliases.Count(); i++)
	{
		// for ease of debugging
		const char *pTrueFontName = m_FontAliases[i]._trueFontName.String();

		KeyValues *kv = fonts->FindKey( pTrueFontName, true);
	
		// walk through creating adding the first matching glyph set to the font
		for (KeyValues *fontdata = kv->GetFirstSubKey(); fontdata != NULL; fontdata = fontdata->GetNextKey())
		{
			// skip over an "isproportional" key if present
			const static int nIsProportional = KeyValuesSystem()->GetSymbolForString( "isproportional", true );
			if ( fontdata->GetNameSymbol() == nIsProportional )
				continue;

			// skip over fonts not meant for this resolution
			int fontYResMin = 0, fontYResMax = 0;
			sscanf(fontdata->GetString("yres", ""), "%d %d", &fontYResMin, &fontYResMax);
			if (fontYResMin)
			{
				if (!fontYResMax)
				{
					fontYResMax = fontYResMin;
				}
				// check the range
				if (m_nScreenTall < fontYResMin || m_nScreenTall > fontYResMax)
					continue;
			}

			int flags = 0;
			if (fontdata->GetInt( "italic" ))
			{
				flags |= FONTFLAG_ITALIC;
			}
			if (fontdata->GetInt( "underline" ))
			{
				flags |= FONTFLAG_UNDERLINE;
			}
			if (fontdata->GetInt( "strikeout" ))
			{
				flags |= FONTFLAG_STRIKEOUT;
			}
			if (fontdata->GetInt( "symbol" ))
			{
				flags |= FONTFLAG_SYMBOL;
			}
			if ( fontdata->GetInt( "antialias" ) && g_pSchemeManager->GetSurface()->SupportsFontFeature( FONT_FEATURE_ANTIALIASED_FONTS ) )
			{
				flags |= FONTFLAG_ANTIALIAS;
			}
			if ( fontdata->GetInt( "dropshadow" ) && g_pSchemeManager->GetSurface()->SupportsFontFeature( FONT_FEATURE_DROPSHADOW_FONTS ) )
			{
				flags |= FONTFLAG_DROPSHADOW;
			}
			if ( fontdata->GetInt( "outline" ) && g_pSchemeManager->GetSurface()->SupportsFontFeature( FONT_FEATURE_OUTLINE_FONTS ) )
			{
				flags |= FONTFLAG_OUTLINE;
			}
			if (fontdata->GetInt( "custom" ))
			{
				flags |= FONTFLAG_CUSTOM;
			}
			if (fontdata->GetInt( "bitmap" ))
			{
				flags |= FONTFLAG_BITMAP;
			}
			if (fontdata->GetInt( "rotary" ))
			{
				flags |= FONTFLAG_ROTARY;
			}
			if (fontdata->GetInt( "additive" ))
			{
				flags |= FONTFLAG_ADDITIVE;
			}

			int tall = fontdata->GetInt( "tall" );
			int blur = fontdata->GetInt( "blur" );
			int scanlines = fontdata->GetInt( "scanlines" );
			float scalex = fontdata->GetFloat( "scalex", 1.0f );
			float scaley = fontdata->GetFloat( "scaley", 1.0f );

			// only grow this font if it doesn't have a resolution filter specified
			// ALFRED - allow proportional to still take effect for the far end of font selection
			if ( ( ( !fontYResMin && !fontYResMax ) || fontYResMax > 4000 ) && m_FontAliases[i].m_bProportional )
			{
				tall = g_Scheme.GetProportionalScaledValueEx( this, tall );
				blur = g_Scheme.GetProportionalScaledValueEx( this, blur );
				scanlines = g_Scheme.GetProportionalScaledValueEx( this, scanlines ); 
				scalex = g_Scheme.GetProportionalScaledValueEx( this, scalex * 10000.0f ) * 0.0001f;
				scaley = g_Scheme.GetProportionalScaledValueEx( this, scaley * 10000.0f ) * 0.0001f;
			}

			// clip the font size so that fonts can't be too big
			if ( tall > 127 )
			{
				tall = 127;
			}

			// check our minimum font height
			if ( tall < minimumFontHeight )
			{
				tall = minimumFontHeight;
			}
			
			if ( flags & FONTFLAG_BITMAP )
			{
				if ( !scalex || !scaley )
				{
					// something is malformed with the scheme specification for the font
					AssertMsg1( 0, "Bad scale values for %s\n", pTrueFontName );
				}

				// add the new set
				g_pSchemeManager->GetSurface()->SetBitmapFontGlyphSet(
					m_FontAliases[i]._font,
					g_pSchemeManager->GetSurface()->GetBitmapFontName( fontdata->GetString( "name" ) ), 
					scalex,
					scaley,
					flags);
			}
			else
			{
				if ( !tall )
				{
					// something is malformed with the scheme specification for the font
					// this causes Xbox XUI to fault, 0 sized fonts mst be fixed!!! 
					Warning( "Bad Tall value for %s\n", pTrueFontName );
				}

				int nRangeMin, nRangeMax;

				if ( GetFontRange( fontdata->GetString( "name" ), nRangeMin, nRangeMax ) )
				{
					// add the new set
					g_pSchemeManager->GetSurface()->SetFontGlyphSet(
						m_FontAliases[i]._font,
						fontdata->GetString( "name" ), 
						tall, 
						fontdata->GetInt( "weight" ), 
						blur,
						scanlines,
						flags,
						nRangeMin,
						nRangeMax);					
				}
				else
				{
					// add the new set
					g_pSchemeManager->GetSurface()->SetFontGlyphSet(
						m_FontAliases[i]._font,
						fontdata->GetString( "name" ), 
						tall, 
						fontdata->GetInt( "weight" ), 
						blur,
						scanlines,
						flags);
				}
			}

			// don't add any more
			break;
		}
	}

	COM_TimestampedLog( "ReloadFontGlyphs(): End" );
}

//-----------------------------------------------------------------------------
// Purpose: create the Border objects from the scheme data
//-----------------------------------------------------------------------------
void CScheme::LoadBorders()
{
	m_pkvBorders = m_pData->FindKey("Borders", true);
	{for ( KeyValues *kv = m_pkvBorders->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey())
	{
		if (kv->GetDataType() == KeyValues::TYPE_STRING)
		{
			// it's referencing another border, ignore for now
		}
		else
		{
			int i = m_BorderList.AddToTail();

			IBorder *border = NULL;
			const char *pszBorderType = kv->GetString( "bordertype", NULL );
			if ( pszBorderType && pszBorderType[0] )
			{
				if ( !stricmp(pszBorderType,"image") )
				{
					border = new ImageBorder();
				}
				else if ( !stricmp(pszBorderType,"scalable_image") )
				{
					border = new ScalableImageBorder();
				}
				else
				{
					Assert(0);
					// Fall back to the base border type. See below.
					pszBorderType = NULL;
				}
			}

			if ( !pszBorderType || !pszBorderType[0] )
			{
				border = new Border();
			}

			border->SetName(kv->GetName());
			border->ApplySchemeSettings(this, kv);

			m_BorderList[i].border = border;
			m_BorderList[i].bSharedBorder = false;
			m_BorderList[i].borderSymbol = kv->GetNameSymbol();
		}
	}}

	// iterate again to get the border references
	for ( KeyValues *kv = m_pkvBorders->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey())
	{
		if (kv->GetDataType() == KeyValues::TYPE_STRING)
		{
			// it's referencing another border, find it
			Border *border = (Border *)GetBorder(kv->GetString());
//			const char *str = kv->GetName();
			Assert(border);

			// add an entry that just references the existing border
			int i = m_BorderList.AddToTail();
			m_BorderList[i].border = border;
			m_BorderList[i].bSharedBorder = true;
			m_BorderList[i].borderSymbol = kv->GetNameSymbol();
		}
	}
	
	m_pBaseBorder = GetBorder("BaseBorder");
}

void CScheme::SpewFonts( void )
{
	Msg( "Scheme: %s (%s)\n", GetName(), GetFileName() );
	for ( int i = 0; i < m_FontAliases.Count(); i++ )
	{
		Msg( "  %2d: HFont:0x%8.8x, %s, %s, font:%s, tall:%d\n", 
			i, 
			m_FontAliases[i]._font,
			m_FontAliases[i]._trueFontName.String(), 
			m_FontAliases[i]._fontName.String(),
			g_pSchemeManager->GetSurface()->GetFontName( m_FontAliases[i]._font ), 
			g_pSchemeManager->GetSurface()->GetFontTall( m_FontAliases[i]._font ) );
	}
}



//-----------------------------------------------------------------------------
// Purpose: kills all the schemes
//-----------------------------------------------------------------------------
void CScheme::Shutdown( bool full )
{
	SetActive( false );

	for (int i = 0; i < m_BorderList.Count(); i++)
	{
		// delete if it's not shared
		if (!m_BorderList[i].bSharedBorder)
		{
			IBorder *border = m_BorderList[i].border;
			delete border;
		}
	}

	m_pBaseBorder = NULL;
	m_BorderList.RemoveAll();
	m_pkvBorders = NULL;

	m_FontRanges.RemoveAll();
	m_FontAliases.RemoveAll();

	if ( m_pData)
	{
		m_pData->deleteThis();
		m_pData = NULL;
	}

	if ( full )
	{
		delete this;
	}
}



//-----------------------------------------------------------------------------
// Purpose: returns a pointer to an existing border
//-----------------------------------------------------------------------------
IBorder *CScheme::GetBorder(const char *borderName)
{
	int symbol = KeyValuesSystem()->GetSymbolForString(borderName);
	for (int i = 0; i < m_BorderList.Count(); i++)
	{
		if (m_BorderList[i].borderSymbol == symbol)
		{
			return m_BorderList[i].border;
		}
	}

	return m_pBaseBorder;
}

//-----------------------------------------------------------------------------
// Finds a font in the alias list
//-----------------------------------------------------------------------------
HFont CScheme::FindFontInAliasList( const char *pFontName )
{
	// FIXME: Slow!!!
	for (int i = m_FontAliases.Count(); --i >= 0; )
	{
		// exposed out for debugging ease
		const char *pAliasName = m_FontAliases[i]._fontName.String();
		if ( !strnicmp( pFontName, pAliasName, FONT_ALIAS_NAME_LENGTH ) )
		{
			return m_FontAliases[i]._font;
		}
	}

	// No dice
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : font - 
// Output : char const
//-----------------------------------------------------------------------------
char const *CScheme::GetFontName( const HFont& font )
{
	for (int i = m_FontAliases.Count(); --i >= 0; )
	{
		HFont fnt = (HFont)m_FontAliases[i]._font;
		if ( fnt == font )
			return m_FontAliases[i]._trueFontName.String();
	}

	return "<Unknown font>";
}

//-----------------------------------------------------------------------------
// Purpose: returns a pointer to an existing font, proportional=false means use default
//-----------------------------------------------------------------------------
HFont CScheme::GetFont( const char *fontName, bool proportional )
{
	// First look in the list of aliases...
	return FindFontInAliasList( GetMungedFontName( fontName, m_tag.String(), proportional ) );
}

//-----------------------------------------------------------------------------
// Purpose: returns a char string of the munged name this font is stored as in the font manager
//-----------------------------------------------------------------------------
const char *CScheme::GetMungedFontName( const char *fontName, const char *scheme, bool proportional )
{
	static char mungeBuffer[ 64 ];
	if ( scheme )
	{
		Q_snprintf( mungeBuffer, sizeof( mungeBuffer ), "%s%s-%s", fontName, scheme, proportional ? "p" : "no" );
	}
	else
	{ 
		Q_snprintf( mungeBuffer, sizeof( mungeBuffer ), "%s-%s", fontName, proportional ? "p" : "no" ); // we don't want the "(null)" snprintf appends
	}
	return mungeBuffer;
}

//-----------------------------------------------------------------------------
// Purpose: Gets a color from the scheme file
//-----------------------------------------------------------------------------
Color CScheme::GetColor(const char *colorName, Color defaultColor)
{
	const char *pchT = LookupSchemeSetting(colorName);
	if (!pchT)
		return defaultColor;

	int r, g, b, a = 0;
	if (sscanf(pchT, "%d %d %d %d", &r, &g, &b, &a) >= 3)
		return Color(r, g, b, a);

	return defaultColor;
}

//-----------------------------------------------------------------------------
// Purpose: recursively looks up a setting
//-----------------------------------------------------------------------------
const char *CScheme::LookupSchemeSetting(const char *pchSetting)
{
	// try parse out the color
	int r, g, b, a = 0;
	int res = sscanf(pchSetting, "%d %d %d %d", &r, &g, &b, &a);
	if (res >= 3)
	{
		return pchSetting;
	}

	// check the color area first
	const char *colStr = m_pkvColors->GetString(pchSetting, NULL);
	if (colStr)
		return colStr;

	// check base settings
	colStr = m_pkvBaseSettings->GetString(pchSetting, NULL);
	if (colStr)
	{
		return LookupSchemeSetting(colStr);
	}

	return pchSetting;
}

//-----------------------------------------------------------------------------
// Purpose: gets the minimum font height for the current language
//-----------------------------------------------------------------------------
int CScheme::GetMinimumFontHeightForCurrentLanguage( const char *pLanguage )
{
	char language[64];
	bool bValid = false;
	if ( IsPC() )
	{
		if ( pLanguage )
		{
			Q_strncpy( language, pLanguage, sizeof( language ) );
			bValid = true;
		}
		else
		{
			bValid = g_pSystem->GetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Language", language, sizeof(language)-1 );
		}
	}
	else if ( IsGameConsole() )
	{
		Q_strncpy( language, XBX_GetLanguageString(), sizeof( language ) );
		bValid = true;
	}

	if ( bValid )
	{
		if (!stricmp(language, "korean")
			|| !stricmp(language, "koreana")
			|| !stricmp(language, "tchinese")
			|| !stricmp(language, "schinese")
			|| !stricmp(language, "japanese"))
		{
			// the bitmap-based fonts for these languages simply don't work with a pt. size of less than 9 (13 pixels)
			return 13;
		}

		if ( !stricmp(language, "thai" ) )
		{
			// thai has problems below 18 pts
			return 18;
		}
	}

	// no special minimum height required
	return 0;
}

void CScheme::SetActive( bool bActive )
{
	m_bActive = bActive;
}

bool CScheme::IsActive() const
{
	return m_bActive;
}

void CScheme::AddCriticalFont( const char *pFontName, KeyValues *pKV )
{
	int i = -1;
	for ( i = m_CriticalFonts.Count() - 1; i >= 0; i-- )
	{
		if ( !V_stricmp( m_CriticalFonts[i].m_FontName.Get(), pFontName ) )
		{
			// found
			break;
		}
	}

	if ( i == -1 )
	{
		i = m_CriticalFonts.AddToTail();
	}

	m_CriticalFonts[i].m_FontName = pFontName;
	m_CriticalFonts[i].m_bCommonChars = pKV->GetBool( "commonchars" );
	m_CriticalFonts[i].m_bUppercase = pKV->GetBool( "uppercase" );
	m_CriticalFonts[i].m_bLowercase = pKV->GetBool( "lowercase" );
	m_CriticalFonts[i].m_bNumbers = pKV->GetBool( "numbers" );
	m_CriticalFonts[i].m_bPunctuation = pKV->GetBool( "punctuation" );
	m_CriticalFonts[i].m_bExtendedChars = pKV->GetBool( "extendedchars" );
	m_CriticalFonts[i].m_bAsianChars = pKV->GetBool( "asianchars" );
	m_CriticalFonts[i].m_bSkipIfAsian = pKV->GetBool( "skipifasian", true );
	m_CriticalFonts[i].m_ExplicitChars = pKV->GetString( "explicit" );
	m_CriticalFonts[i].m_bRussianChars = ( pKV->GetString( "russianchars" ) != NULL );
}

bool CScheme::CCriticalFontLess::Less( const CriticalFontMap_t &src1, const CriticalFontMap_t &src2, void *pCtx )
{
	return src1.m_nTall < src2.m_nTall;
}

void CScheme::PrecacheCriticalFontGlyphs( const char *pLanguage )
{
	if ( !IsGameConsole() )
	{
		// only game consoles need to prefetch their glyphs
		// pc has much faster glyph rasterization and needs to support resoultion changes ect
		return;
	}

	if ( !m_CriticalFonts.Count() )
		return;

	// use the caller's supplied language or fallback
	if ( !pLanguage || !pLanguage[0] )
	{
		pLanguage = g_pSchemeManager->GetLanguage();
	}

	bool bIsAsianLanguage = false;
	bool bIsRussianLanguage = false;

	wchar_t *pAsianFrequenceSequence = NULL;
	if ( pLanguage && pLanguage[0] )
	{
		if ( !V_stricmp( pLanguage, "japanese" ) ||
			!V_stricmp( pLanguage, "korean" ) ||
			!V_stricmp( pLanguage, "schinese" ) ||
			!V_stricmp( pLanguage, "tchinese" ) )
		{
			bIsAsianLanguage = true;
			pAsianFrequenceSequence = g_pVGuiLocalize->GetAsianFrequencySequence( pLanguage );
		}
		else if ( !V_stricmp( pLanguage, "russian" ) )
		{
			bIsRussianLanguage = true;
		}
	}
	
	float flStartTime = Plat_FloatTime();

	// sort critical fonts by size
	CUtlSortVector< CriticalFontMap_t, CCriticalFontLess > sortedFonts;
	for ( int i = m_CriticalFonts.Count() - 1; i >= 0; i-- )
	{
		if ( m_CriticalFonts[i].m_bPrecached )
		{
			// once marked, never can hit again, regardless of validity
			// this is a brutal operation that we want to happen only once at startup
			return;
		}

		if ( bIsAsianLanguage && m_CriticalFonts[i].m_bSkipIfAsian )
		{
			// only precache the ones marked to precache during an asian language
			continue;
		}

		// need to do non-proportional and proportional
		for ( int j = 0; j < 2; j++ )
		{
			HFont hFont = GetFont( m_CriticalFonts[i].m_FontName.Get(), ( j != 0 ) );
			if ( hFont == INVALID_FONT )
			{
				continue;
			}

			CriticalFontMap_t fontMap;
			fontMap.m_pCriticalFont = &m_CriticalFonts[i];
			fontMap.m_hFont = hFont;
			fontMap.m_nTall = g_pSchemeManager->GetSurface()->GetFontTall( hFont );
			sortedFonts.Insert( fontMap );
		}
	}

	for ( int i = sortedFonts.Count() - 1; i >= 0; i-- )
	{
		CriticalFont_t *pCriticalFont = sortedFonts[i].m_pCriticalFont;
		HFont hFont = sortedFonts[i].m_hFont;

		DevMsg( "Precaching: %s, tall:%d\n", pCriticalFont->m_FontName.Get(), sortedFonts[i].m_nTall );

		pCriticalFont->m_bPrecached = true;

		wchar_t	szUnicode[512];
		szUnicode[0] = 0;

		if ( !pCriticalFont->m_ExplicitChars.IsEmpty() )
		{
			if ( pCriticalFont->m_ExplicitChars[0] == '#' )
			{
				wchar_t *pLocalizedString = g_pVGuiLocalize->Find( pCriticalFont->m_ExplicitChars.String() );
				if ( pLocalizedString && pLocalizedString[0] )
				{
					V_wcsncpy( szUnicode, pLocalizedString, sizeof( szUnicode ) );
				}
			}
			else
			{
				g_pVGuiLocalize->ConvertANSIToUnicode( pCriticalFont->m_ExplicitChars.String(), szUnicode, sizeof( szUnicode ) );
			}
		}

		if ( bIsAsianLanguage && pCriticalFont->m_bAsianChars )
		{
			if ( pAsianFrequenceSequence && pAsianFrequenceSequence[0] )
			{
				g_pSchemeManager->GetSurface()->PrecacheFontCharacters( hFont, pAsianFrequenceSequence );
			}
		}

		if ( pCriticalFont->m_bCommonChars || pCriticalFont->m_bUppercase )
		{
			int wLen = V_wcslen( szUnicode );
			V_wcsncpy( szUnicode + wLen, L"ABCDEFGHIJKLMNOPQRSTUVWXYZ", sizeof( szUnicode ) - 2 * wLen );
		}
		if ( pCriticalFont->m_bCommonChars || pCriticalFont->m_bLowercase )
		{
			int wLen = V_wcslen( szUnicode );
			V_wcsncpy( szUnicode + wLen, L"abcdefghijklmnopqrstuvwxyz", sizeof( szUnicode ) - 2 * wLen );
		}
		if ( pCriticalFont->m_bCommonChars || pCriticalFont->m_bNumbers )
		{
			int wLen = V_wcslen( szUnicode );
			V_wcsncpy( szUnicode + wLen, L"0123456789", sizeof( szUnicode ) - 2 * wLen );
		}
		if ( pCriticalFont->m_bCommonChars || pCriticalFont->m_bPunctuation )
		{
			int wLen = V_wcslen( szUnicode );
			V_wcsncpy( szUnicode + wLen, L",.!:-/% ", sizeof( szUnicode ) - 2 * wLen );
		}

		if ( pCriticalFont->m_bCommonChars || pCriticalFont->m_bExtendedChars )
		{
			wchar_t *pExtendedChars = g_pVGuiLocalize->Find( "#GameUI_Language_ExtendedChars" );
			if ( pExtendedChars && pExtendedChars[0] )
			{
				// add in any extended chars, this picks up a language's accents, etc
				// english would be empty
				int wLen = V_wcslen( szUnicode );
				V_wcsncpy( szUnicode + wLen, pExtendedChars, sizeof( szUnicode ) - 2 * wLen );
			}
		}

		if ( bIsRussianLanguage && pCriticalFont->m_bRussianChars )
		{
			if ( pCriticalFont->m_bUppercase )
			{
				wchar_t *pUpperRussianChars = g_pVGuiLocalize->Find( "#GameUI_Language_Russian_Uppercase" );
				if ( pUpperRussianChars && pUpperRussianChars[0] )
				{
					// add in russian uppercase
					int wLen = V_wcslen( szUnicode );
					V_wcsncpy( szUnicode + wLen, pUpperRussianChars, sizeof( szUnicode ) - 2 * wLen );
				}
			}
			if ( pCriticalFont->m_bLowercase )
			{
				wchar_t *pLowerRussianChars = g_pVGuiLocalize->Find( "#GameUI_Language_Russian_Lowercase" );
				if ( pLowerRussianChars && pLowerRussianChars[0] )
				{
					// add in russian lowercase
					int wLen = V_wcslen( szUnicode );
					V_wcsncpy( szUnicode + wLen, pLowerRussianChars, sizeof( szUnicode ) - 2 * wLen );
				}
			}
		}

		if ( szUnicode[0] )
		{			
			g_pSchemeManager->GetSurface()->PrecacheFontCharacters( hFont, szUnicode );
		}
	}

	DevMsg( "PrecacheCriticalFontGlyphs( %s ): Took %.2f seconds\n", pLanguage, Plat_FloatTime()- flStartTime );
}