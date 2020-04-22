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

#include "gameuischeme.h"
#include "gameuisystemsurface.h"
#include "vgui/ISystem.h"
#include "tier1/utlbuffer.h"
#include "gameuisystemmgr.h"
#include "tier1/KeyValues.h"
#include "vgui_surfacelib/fontmanager.h"
#include "vgui/isurface.h"
#include "vgui_controls/controls.h"  // has system() 

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif
#include "xbox/xboxstubs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



#define FONT_ALIAS_NAME_LENGTH 64

CGameUISchemeManager g_GameUISchemeManager;
CGameUISchemeManager *g_pGameUISchemeManager = &g_GameUISchemeManager;

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
// Constructor
//-----------------------------------------------------------------------------
CGameUISchemeManager::CGameUISchemeManager()
{
	// 0th element is null, since that would be an invalid handle
	CGameUIScheme *nullScheme = new CGameUIScheme();
	m_Schemes.AddToTail(nullScheme);
	Assert( g_pGameUISystemSurface );
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CGameUISchemeManager::~CGameUISchemeManager()
{
	int i;
	for ( i = 0; i < m_Schemes.Count(); i++ )
	{
		delete m_Schemes[i];
		m_Schemes[i] = NULL;
	}
	m_Schemes.RemoveAll();

	Shutdown( false );
}

//-----------------------------------------------------------------------------
// Purpose: Reloads the schemes from the files
//-----------------------------------------------------------------------------
void CGameUISchemeManager::ReloadSchemes()
{
	int count = m_Schemes.Count();
	Shutdown( false );
	
	// reload the scheme
	for (int i = 1; i < count; i++)
	{
		LoadSchemeFromFile( m_Schemes[i]->GetFileName(), m_Schemes[i]->GetName() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Reload the fonts in all schemes
//-----------------------------------------------------------------------------
void CGameUISchemeManager::ReloadFonts( int inScreenTall )
{
	for (int i = 1; i < m_Schemes.Count(); i++)
	{
		m_Schemes[i]->ReloadFontGlyphs( inScreenTall );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameUISchemeManager::Shutdown( bool full )
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
// Purpose: Find an already loaded scheme
//-----------------------------------------------------------------------------
IGameUIScheme * CGameUISchemeManager::FindLoadedScheme( const char *pFilename )
{
	// Find the scheme in the list of already loaded schemes
	for ( int i = 1; i < m_Schemes.Count(); i++ )
	{
		char const *schemeFileName = m_Schemes[i]->GetFileName();
		if ( !stricmp( schemeFileName, pFilename ) )
			return m_Schemes[i];
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// First scheme loaded becomes the default scheme, 
// and all subsequent loaded scheme are derivatives of that
//-----------------------------------------------------------------------------
IGameUIScheme * CGameUISchemeManager::LoadSchemeFromFileEx( const char *pFilename, const char *tag )
{
	// Look to see if we've already got this scheme...
	CGameUIScheme * hScheme = ( CGameUIScheme * ) FindLoadedScheme( pFilename );
	if ( hScheme )
	{
		if ( hScheme->IsActive() )
		{
			if ( IsPC() )
			{
				hScheme->ReloadFontGlyphs();
			}
			return hScheme;
		}
	}
	else
	{
		hScheme = new CGameUIScheme();
		m_Schemes.AddToTail( hScheme );
	}

	KeyValues *data;
	data = new KeyValues("Scheme");

	data->UsesEscapeSequences( true );	// VGUI uses this
	
	// look first in skins directory
	bool result = data->LoadFromFile( g_pFullFileSystem, pFilename, "SKIN" );
	if (!result)
	{
		result = data->LoadFromFile( g_pFullFileSystem, pFilename, "GAME" );
		if ( !result )
		{
			// look in any directory
			result = data->LoadFromFile( g_pFullFileSystem, pFilename, NULL );
		}
	}

	if (!result)
	{
		data->deleteThis();
		return 0;
	}
	
	if ( IsPC() )
	{
		ConVarRef cl_hud_minmode( "cl_hud_minmode", true );
		if ( cl_hud_minmode.IsValid() && cl_hud_minmode.GetBool() )
		{
			data->ProcessResolutionKeys( "_minmode" );
		}
	}

	hScheme->SetActive( true );
	hScheme->LoadFromFile( pFilename, tag, data );

	return hScheme;
}

//-----------------------------------------------------------------------------
// Purpose: loads a scheme from disk
//-----------------------------------------------------------------------------
IGameUIScheme * CGameUISchemeManager::LoadSchemeFromFile( const char *fileName, const char *tag )
{
	return LoadSchemeFromFileEx( fileName, tag );
}

//-----------------------------------------------------------------------------
// Purpose: returns a handle to the default (first loaded) scheme
//-----------------------------------------------------------------------------
IGameUIScheme * CGameUISchemeManager::GetDefaultScheme()
{
	if ( m_Schemes.Count() >= 2 )
		return m_Schemes[1];
	else if ( m_Schemes.Count() > 0 )
		return m_Schemes[0];
	else
		return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: returns a handle to the scheme identified by "tag"
//-----------------------------------------------------------------------------
IGameUIScheme * CGameUISchemeManager::GetScheme( const char *tag )
{
	for ( int i=1; i<m_Schemes.Count(); i++ )
	{
		if ( !stricmp( tag, m_Schemes[i]->GetName() ) )
		{
			return m_Schemes[i];
		}
	}
	return GetDefaultScheme(); // default scheme
}

//-----------------------------------------------------------------------------
// gets the proportional coordinates for doing screen-size independant panel layouts
// use these for font, image and panel size scaling (they all use the pixel height of the display for scaling)
//-----------------------------------------------------------------------------
int CGameUISchemeManager::GetProportionalScaledValueEx( CGameUIScheme *pScheme, int normalizedValue, int screenTall )
{
	return GetProportionalScaledValue( normalizedValue, screenTall );
}

//-----------------------------------------------------------------------------
// Purpose: converts a value into proportional mode
//-----------------------------------------------------------------------------
int CGameUISchemeManager::GetProportionalScaledValue( int normalizedValue, int screenTall )
{
	int tall;
	if (  screenTall == -1 ) 
	{
		g_pGameUISystemMgrImpl->GetScreenHeightForFontLoading( tall );
	}
	else
	{
		tall = screenTall;
	}
	return GetProportionalScaledValue_( tall, normalizedValue );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CGameUISchemeManager::GetProportionalScaledValue_( int screenTall, int normalizedValue )
{
	int baseWide, baseTall;
	g_pGameUISystemSurface->GetProportionalBase( baseWide, baseTall );
	double scale = (double)screenTall / (double)baseTall;

	return (int)( normalizedValue * scale );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUISchemeManager::SpewFonts( void )
{
	for ( int i = 1; i < m_Schemes.Count(); i++ )
	{
		m_Schemes[i]->SpewFonts();
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUISchemeManager::SetLanguage( const char *pLanguage )
{  
	g_pGameUISystemSurface->SetLanguage( pLanguage );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CGameUISchemeManager::GetLanguage()
{ 
	return g_pGameUISystemSurface->GetLanguage();
}
















//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CGameUIScheme::CGameUIScheme()
{
	m_pFileName = "";
	m_pTag = "";
	m_pData = NULL;
	SetActive( false );
}

CGameUIScheme::~CGameUIScheme()
{
}

//-----------------------------------------------------------------------------
// Purpose: loads a scheme from from disk into memory
//-----------------------------------------------------------------------------
void CGameUIScheme::LoadFromFile( const char *pFilename, const char *inTag, KeyValues *inKeys )
{
	COM_TimestampedLog( "CScheme::LoadFromFile( %s )", pFilename );

	m_pFileName = pFilename;
	m_pData = inKeys;

	// override the scheme name with the tag name
	KeyValues *name = m_pData->FindKey( "Name", true );
	name->SetString("Name", inTag);

	if ( inTag )
	{
		m_pTag = inTag;
	}
	else
	{
		Error( "You need to name the scheme (%s)!", m_pFileName.Get() );
		m_pTag = "default";
	}

	LoadFonts();
}

//-----------------------------------------------------------------------------
// Purpose: Set the range of character values this font can be used on
//-----------------------------------------------------------------------------
bool CGameUIScheme::GetFontRange( const char *fontname, int &nMin, int &nMax )
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
// Purpose: Get the range of character values this font can be used on
//-----------------------------------------------------------------------------
void CGameUIScheme::SetFontRange( const char *fontname, int nMin, int nMax )
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
void CGameUIScheme::LoadFonts()
{
	// get our language
	const char *pLanguage = g_GameUISchemeManager.GetLanguage();


	// add our custom fonts
	for ( KeyValues *kv = m_pData->FindKey("CustomFontFiles", true)->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey() )
	{
		const char *fontFile = kv->GetString();
		if (fontFile && *fontFile)
		{
			g_pGameUISystemSurface->AddCustomFontFile( fontFile );
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
					if ( Q_stricmp( pLanguage, pszKey ) == 0 ) // matches the language we're running?
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
				g_pGameUISystemSurface->AddCustomFontFile( fontFile );

				if ( bUseRange )
				{
					SetFontRange( pszName, nRangeMin, nRangeMax );
				}
			}
		}
	}

	// add bitmap fonts
	for ( KeyValues *kv = m_pData->FindKey("BitmapFontFiles", true)->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey() )
	{
		const char *fontFile = kv->GetString();
		if (fontFile && *fontFile)
		{
			bool bSuccess = g_pGameUISystemSurface->AddBitmapFontFile( fontFile );
			if ( bSuccess )
			{
				// refer to the font by a user specified symbol
				g_pGameUISystemSurface->SetBitmapFontName( kv->GetName(), fontFile );
			}
		}
	}

	// create the fonts
	for (KeyValues *kv = m_pData->FindKey("Fonts", true)->GetFirstSubKey(); kv != NULL; kv = kv->GetNextKey())
	{
		for ( int i = 0; i < 2; i++ )
		{
			// create the base font
			bool proportionalFont = i ? true : false;
			const char *fontName = GetMungedFontName( kv->GetName(), m_pTag.Get(), proportionalFont ); // first time it adds a normal font, and then a proportional one
			FontHandle_t font = g_pGameUISystemSurface->CreateFont();
			int j = m_FontAliases.AddToTail();
			m_FontAliases[j]._fontName = fontName;
			m_FontAliases[j]._trueFontName = kv->GetName();
			m_FontAliases[j]._font = font;
			m_FontAliases[j].m_bProportional = proportionalFont;
		}
	}

	// load in the font glyphs
	ReloadFontGlyphs();
}

//-----------------------------------------------------------------------------
// Purpose: Reloads all scheme fonts
// Supply a language if you don't want to use the one in the registry.
//-----------------------------------------------------------------------------
void CGameUIScheme::ReloadFontGlyphs( int inScreenTall )
{
	int screenTall;
	// get our current resolution
	if ( inScreenTall == -1 )
	{
		//int screenWide;
		g_pGameUISystemMgrImpl->GetScreenHeightForFontLoading( screenTall );
	}
	else // this will make fonts be the correct size in the editor, where the stage size is the screen size.
	{
		screenTall = inScreenTall;
	}
	
	// check our language; some have minimum sizes
	int minimumFontHeight = GetMinimumFontHeightForCurrentLanguage( g_GameUISchemeManager.GetLanguage() );

	// add the data to all the fonts
	KeyValues *fonts = m_pData->FindKey( "Fonts", true );
	for (int i = 0; i < m_FontAliases.Count(); i++)
	{
		KeyValues *kv = fonts->FindKey( m_FontAliases[i]._trueFontName.String(), true );
	
		// walk through creating adding the first matching glyph set to the font
		for ( KeyValues *fontdata = kv->GetFirstSubKey(); fontdata != NULL; fontdata = fontdata->GetNextKey() )
		{
			
			// skip over fonts not meant for this resolution
			int fontYResMin = 0, fontYResMax = 0;
			sscanf( fontdata->GetString( "yres", "" ), "%d %d", &fontYResMin, &fontYResMax );
			if ( fontYResMin )
			{
				if ( !fontYResMax )
				{
					fontYResMax = fontYResMin;
				}
				// check the range
				if ( screenTall < fontYResMin || screenTall > fontYResMax )
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
			if (fontdata->GetInt( "antialias" ) && g_pGameUISystemSurface->SupportsFontFeature( FONT_FEATURE_ANTIALIASED_FONTS ) )
			{
				flags |= FONTFLAG_ANTIALIAS;
			}
			if (fontdata->GetInt( "dropshadow" ) && g_pGameUISystemSurface->SupportsFontFeature( FONT_FEATURE_DROPSHADOW_FONTS ) )
			{
				flags |= FONTFLAG_DROPSHADOW;
			}
			if (fontdata->GetInt( "outline" ) && g_pGameUISystemSurface->SupportsFontFeature( FONT_FEATURE_OUTLINE_FONTS ) )
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

			int fontTall = fontdata->GetInt( "tall" );
			int blur = fontdata->GetInt( "blur" );
			int scanlines = fontdata->GetInt( "scanlines" );
			float scalex = fontdata->GetFloat( "scalex", 1.0f );
			float scaley = fontdata->GetFloat( "scaley", 1.0f );

			// only grow this font if it doesn't have a resolution filter specified
			if ( ( !fontYResMin && !fontYResMax ) && m_FontAliases[i].m_bProportional )
			{
				fontTall = g_GameUISchemeManager.GetProportionalScaledValueEx( this, fontTall, screenTall );
				blur = g_GameUISchemeManager.GetProportionalScaledValueEx( this, blur );
				scanlines = g_GameUISchemeManager.GetProportionalScaledValueEx( this, scanlines ); 
				scalex = g_GameUISchemeManager.GetProportionalScaledValueEx( this, scalex * 10000.0f ) * 0.0001f;
				scaley = g_GameUISchemeManager.GetProportionalScaledValueEx( this, scaley * 10000.0f ) * 0.0001f;
			}

			// clip the font size so that fonts can't be too big
			if ( fontTall > 127 )
			{
				fontTall = 127;
			}

			// check our minimum font height
			if ( fontTall < minimumFontHeight )
			{
				fontTall = minimumFontHeight;
			}
			
			if ( flags & FONTFLAG_BITMAP )
			{
				// add the new set
				g_pGameUISystemSurface->SetBitmapFontGlyphSet(
					m_FontAliases[i]._font,
					g_pGameUISystemSurface->GetBitmapFontName( fontdata->GetString( "name" ) ), 
					scalex,
					scaley,
					flags);
			}
			else
			{
				int nRangeMin, nRangeMax;
				if ( GetFontRange( fontdata->GetString( "name" ), nRangeMin, nRangeMax ) )
				{
					// add the new set
					g_pGameUISystemSurface->SetFontGlyphSet(
						m_FontAliases[i]._font, fontdata->GetString( "name" ), 
						fontTall, fontdata->GetInt( "weight" ), blur, scanlines, flags, nRangeMin, nRangeMax);					
				}
				else
				{
					// add the new set
					g_pGameUISystemSurface->SetFontGlyphSet( m_FontAliases[i]._font, fontdata->GetString( "name" ), 
						fontTall, fontdata->GetInt( "weight" ), blur, scanlines, flags);
				}
			}

			// don't add any more
			break;
		}
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGameUIScheme::SpewFonts( void )
{
	Msg( "Game UI Scheme: %s (%s)\n", GetName(), GetFileName() );
	for ( int i = 0; i < m_FontAliases.Count(); i++ )
	{
		Msg( "  %2d: HFont:0x%8.8x, %s, %s, font:%s, tall:%d\n", 
			i, 
			m_FontAliases[i]._font,
			m_FontAliases[i]._trueFontName.String(), 
			m_FontAliases[i]._fontName.String(),
			g_pGameUISystemSurface->GetFontName( m_FontAliases[i]._font ), 
			g_pGameUISystemSurface->GetFontTall( m_FontAliases[i]._font ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Kills the scheme
//-----------------------------------------------------------------------------
void CGameUIScheme::Shutdown( bool full )
{
	SetActive( false );

	m_FontRanges.RemoveAll();

	if (m_pData)
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
// Finds a font in the alias list
//-----------------------------------------------------------------------------
FontHandle_t CGameUIScheme::FindFontInAliasList( const char *fontName )
{
	// FIXME: Slow!!!
	for (int i = m_FontAliases.Count(); --i >= 0; )
	{
		if ( !strnicmp( fontName, m_FontAliases[i]._fontName.String(), FONT_ALIAS_NAME_LENGTH ) )
			return m_FontAliases[i]._font;
	}

	// No dice
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : font - 
// Output : char const
//-----------------------------------------------------------------------------
char const *CGameUIScheme::GetFontName( const FontHandle_t &font )
{
	for (int i = m_FontAliases.Count(); --i >= 0; )
	{
		FontHandle_t fnt = ( FontHandle_t )m_FontAliases[i]._font;
		if ( fnt == font )
			return m_FontAliases[i]._trueFontName.String();
	}

	return "<Unknown font>";
}

//-----------------------------------------------------------------------------
// material Setting methods 
//-----------------------------------------------------------------------------
FontHandle_t CGameUIScheme::GetFont( const char *fontName, bool proportional )
{
	// First look in the list of aliases...
	return FindFontInAliasList( GetMungedFontName( fontName, m_pTag.Get(), proportional ) );
}

//-----------------------------------------------------------------------------
// Return the vgui name of the font that is the same truefont name and one size up.
// bUp is if true you want the next size up or the next size down.
//-----------------------------------------------------------------------------
FontHandle_t CGameUIScheme::GetFontNextSize( bool bUp, const char *fontName, bool proportional )
{
	FontHandle_t currentFontHandle = GetFont( fontName, proportional );
	// Current font name wasn't found.
	if ( currentFontHandle == 0 )
	{
		return currentFontHandle;
	}

	int currentFontTall = g_pGameUISystemSurface->GetFontTall( currentFontHandle );
	const char* pCurrentFontName = g_pGameUISystemSurface->GetFontName( currentFontHandle );

	// Now find the closest font with this name that is the smallest size up
	int sizeDifference = INT_MAX;
	FontHandle_t closestFontHandle = 0;
	for (int i = m_FontAliases.Count(); --i > 0; )
	{
		if ( proportional != m_FontAliases[i].m_bProportional )
			continue;

		FontHandle_t testFontHandle = m_FontAliases[i]._font;
		const char* pTestFontName = g_pGameUISystemSurface->GetFontName( testFontHandle );
		if ( currentFontHandle != testFontHandle ) 
		{
			if ( !Q_stricmp( pCurrentFontName, pTestFontName ) ) // font name is the same.
			{
				int thisFontTall = g_pGameUISystemSurface->GetFontTall( testFontHandle );
				
				int diff;
				if ( bUp ) // get next size up
				{
					diff = thisFontTall - currentFontTall;	
				}
				else // get next size down.
				{
					diff = currentFontTall - thisFontTall;
				}

				if ( diff > 0 && diff < sizeDifference )
				{
					sizeDifference = diff;
					closestFontHandle = testFontHandle;
				}
			}
		}
	}

	if ( closestFontHandle != 0 )
	{
		return closestFontHandle;
	}
	else
	{
		return currentFontHandle;
	}
	
}

//-----------------------------------------------------------------------------
// Purpose: returns a char string of the munged name this font is stored as in the font manager
//-----------------------------------------------------------------------------
const char *CGameUIScheme::GetMungedFontName( const char *fontName, const char *scheme, bool proportional )
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
// Purpose: gets the minimum font height for the current language
//-----------------------------------------------------------------------------
int CGameUIScheme::GetMinimumFontHeightForCurrentLanguage( const char *pLanguage )
{
	char language[64];
	bool bValid;
	if ( IsPC() )
	{
		if ( pLanguage )
		{
			Q_strncpy( language, pLanguage, sizeof(language)-1 );
			bValid = true;
		}
		else
		{
			bValid = vgui::system()->GetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Language", language, sizeof(language)-1 );
		}
	}
	else
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

void CGameUIScheme::SetActive( bool bActive )
{
	m_bActive = bActive;
}

bool CGameUIScheme::IsActive() const
{
	return m_bActive;
}
