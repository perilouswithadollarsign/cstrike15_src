//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implementation of vgui::TextImage control
//
// $NoKeywords: $
//=============================================================================//

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#ifdef _PS3
#include <wctype.h>
#endif
#include <assert.h>

#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui/IInput.h>
#include <vgui/ILocalize.h>
#include <keyvalues.h>

#include <vgui_controls/TextImage.h>
#include <vgui_controls/Controls.h>

#include "tier0/dbg.h"
// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

// enable this define if you want unlocalized strings logged to files unfound.txt and unlocalized.txt
// #define LOG_UNLOCALIZED_STRINGS

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
TextImage::TextImage(const char *text) : Image()
{
	_utext = NULL;
	_textBufferLen = 0;
	_font = INVALID_FONT; 
	_fallbackFont = INVALID_FONT;
	_unlocalizedTextSymbol = INVALID_STRING_INDEX;
	_drawWidth = 0;
	_textBufferLen = 0;
	_textLen = 0;
	m_bNoShortcutSyntax = false;
	m_bWrap = false;
	m_bWrapCenter = false;
	m_LineBreaks.RemoveAll();
	m_LineXIndent.RemoveAll();
	m_pwszEllipsesPosition = NULL;
	m_bUseFallbackFont = false;
	m_bRenderUsingFallbackFont = false;
	m_bAllCaps = false;
	m_bUseAsianWordWrapping = false;
	m_bRecalculateTruncation = true;
	
	SetText(text); // set the text.
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
TextImage::TextImage(const wchar_t *wszText) : Image()
{
	_utext = NULL;
	_textBufferLen = 0;
	_font = INVALID_FONT;
	_fallbackFont = INVALID_FONT;
	_unlocalizedTextSymbol = INVALID_STRING_INDEX;
	_drawWidth = 0;
	_textBufferLen = 0;
	_textLen = 0;
	m_bNoShortcutSyntax = false;
	m_bWrap = false;
	m_bWrapCenter = false;
	m_LineBreaks.RemoveAll();
	m_LineXIndent.RemoveAll();
	m_bUseFallbackFont = false;
	m_bRenderUsingFallbackFont = false;
	m_bAllCaps = false;
	m_bUseAsianWordWrapping = false;
	m_bRecalculateTruncation = true;

	SetText(wszText); // set the text.
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
TextImage::~TextImage()
{
	delete [] _utext;
}

//-----------------------------------------------------------------------------
// Purpose: takes the string and looks it up in the localization file to convert it to unicode
//-----------------------------------------------------------------------------
void TextImage::SetText(const char *text)
{
	if (!text)
	{
		text = "";
	}

	// check for localization
	if (*text == '#')
	{
		// try lookup in localization tables
		_unlocalizedTextSymbol = g_pVGuiLocalize->FindIndex(text + 1);
		
		if (_unlocalizedTextSymbol != INVALID_STRING_INDEX)
		{
			wchar_t *unicode = g_pVGuiLocalize->GetValueByIndex(_unlocalizedTextSymbol);
			SetText(unicode);
			return;
		}
		else
		{
			// could not find string
			// debug code for logging unlocalized strings
#if defined(LOG_UNLOCALIZED_STRINGS)
			if (*text)
			{
				// write out error to unfound.txt log file
				static bool first = true;
				FILE *f;
				if (first)
				{
					first = false;
					f = fopen("unfound.txt", "wt");
				}
				else
				{
					f = fopen("unfound.txt", "at");
				}

				if (f)
				{
					fprintf(f, "\"%s\"\n", text);
					fclose(f);
				}
			}
#endif // LOG_UNLOCALIZED_STRINGS
		}
	}
	else
	{
		// debug code for logging unlocalized strings
#if defined(LOG_UNLOCALIZED_STRINGS)
		if (text[0])
		{
			// setting a label to be ANSI text, write out error to unlocalized.txt log file
			static bool first = true;
			FILE *f;
			if (first)
			{
				first = false;
				f = fopen("unlocalized.txt", "wt");
			}
			else
			{
				f = fopen("unlocalized.txt", "at");
			}
			if (f)
			{
				fprintf(f, "\"%s\"\n", text);
				fclose(f);
			}
		}
#endif // LOG_UNLOCALIZED_STRINGS
	}

	// convert the ansi string to unicode and use that
	wchar_t unicode[1024];
	g_pVGuiLocalize->ConvertANSIToUnicode(text, unicode, sizeof(unicode));
	SetText(unicode);
}

//-----------------------------------------------------------------------------
// Purpose: Sets the width that the text can be.
//-----------------------------------------------------------------------------
void TextImage::SetDrawWidth(int width)
{
	_drawWidth = width;
	m_bRecalculateTruncation = true;
}

//-----------------------------------------------------------------------------
// Purpose: Gets the width that the text can be.
//-----------------------------------------------------------------------------
void TextImage::GetDrawWidth(int &width)
{
	width = _drawWidth;
}

//-----------------------------------------------------------------------------
// Purpose: sets unicode text directly
//-----------------------------------------------------------------------------
void TextImage::SetText(const wchar_t *unicode, bool bClearUnlocalizedSymbol)
{
	if ( bClearUnlocalizedSymbol )
	{
		// Clear out unlocalized text symbol so that changing dialog variables
		// doesn't stomp over the custom unicode string we're being set to.
		_unlocalizedTextSymbol = INVALID_STRING_INDEX;
	}

	if (!unicode)
	{
		unicode = L"";
	}

	// reallocate the buffer if necessary
	_textLen = (short)wcslen(unicode);
	if (_textLen >= _textBufferLen)
	{
		delete [] _utext;
		_textBufferLen = (short)(_textLen + 1);
		_utext = new wchar_t[_textBufferLen];
	}

	m_LineBreaks.RemoveAll();
	m_LineXIndent.RemoveAll();

	// store the text as unicode
	wcscpy(_utext, unicode);

	m_bRecalculateTruncation = true;
}

//-----------------------------------------------------------------------------
// Purpose: Gets the text in the textImage
//-----------------------------------------------------------------------------
void TextImage::GetText(char *buffer, int bufferSize)
{
	g_pVGuiLocalize->ConvertUnicodeToANSI(_utext, buffer, bufferSize);

	if ( m_bAllCaps )
	{
		// Uppercase all the letters
		for ( int i = Q_strlen( buffer ); i >= 0; --i )
		{
			buffer[ i ] = toupper( buffer[ i ] );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gets the text in the textImage
//-----------------------------------------------------------------------------
void TextImage::GetText(wchar_t *buffer, int bufLenInBytes)
{
	V_wcsncpy(buffer, _utext, bufLenInBytes );

	if ( m_bAllCaps )
	{
		// Uppercase all the letters
		for ( int i = Q_wcslen( buffer ) - 1; i >= 0; --i )
		{
			buffer[ i ] = towupper( buffer[ i ] );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void TextImage::GetUnlocalizedText(char *buffer, int bufferSize)
{
	if (_unlocalizedTextSymbol != INVALID_STRING_INDEX)
	{
		const char *text = g_pVGuiLocalize->GetNameByIndex(_unlocalizedTextSymbol);
		buffer[0] = '#';
		Q_strncpy(buffer + 1, text, bufferSize - 1);
		buffer[bufferSize-1] = 0;
	}
	else
	{
		GetText(buffer, bufferSize);
	}
}

//-----------------------------------------------------------------------------
// Purpose: unlocalized text symbol
//-----------------------------------------------------------------------------
StringIndex_t TextImage::GetUnlocalizedTextSymbol()
{
	return _unlocalizedTextSymbol;
}

//-----------------------------------------------------------------------------
// Purpose: Changes the current font
//-----------------------------------------------------------------------------
void TextImage::SetFont(HFont font)
{
	_font = font;
	m_bRecalculateTruncation = true;
}

//-----------------------------------------------------------------------------
// Purpose: Gets the font of the text.
//-----------------------------------------------------------------------------
HFont TextImage::GetFont()
{
	if ( m_bRenderUsingFallbackFont )
	{
		return _fallbackFont;
	}

	return _font;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the size of the TextImage. This is directly tied to drawWidth
//-----------------------------------------------------------------------------
void TextImage::SetSize(int wide, int tall)
{
	Image::SetSize(wide, tall);
	_drawWidth = wide;
	m_bRecalculateTruncation = true;
}

//-----------------------------------------------------------------------------
// Purpose: Draw the Image on screen.
//-----------------------------------------------------------------------------
void TextImage::Paint()
{
	int wide, tall;
	GetSize(wide, tall);
		
	if (!_utext || GetFont() == INVALID_FONT )
		return;

	if (m_bRecalculateTruncation)
	{
		if ( m_bWrap || m_bWrapCenter )
		{
			RecalculateNewLinePositions();
		}

		RecalculateEllipsesPosition();
	}

	DrawSetTextColor(GetColor());
	HFont font = GetFont();
	DrawSetTextFont(font);

	int lineHeight = surface()->GetFontTall(font);
	int x = 0, y = 0;
	int iIndent = 0;

	int px, py;
	GetPos(px, py);

	int currentLineBreak = 0;

	if ( m_bWrapCenter && m_LineXIndent.Count() )
	{
		x = m_LineXIndent[0];
	}

	for (wchar_t *wsz = _utext; *wsz != 0; wsz++)
	{
		wchar_t ch = wsz[0];

		if ( m_bAllCaps )
		{
			ch = towupper( ch );
		}

		// check for special characters
		if (ch == '\r')
		{
			// ignore, just use \n for newlines
			continue;
		}
		else if (ch == '\n')
		{
			// newline
			iIndent++;
			if ( m_bWrapCenter && iIndent < m_LineXIndent.Count() )
			{
				x = m_LineXIndent[iIndent];
			}
			else
			{
				x = 0;
			}
			y += lineHeight;
			continue;
		}
		else if ((ch == '&') && !m_bNoShortcutSyntax)
		{
			// "&&" means draw a single ampersand, single one is a shortcut character
			if (wsz[1] == '&')
			{
				// just move on and draw the second ampersand
				wsz++;
			}
			else
			{
				// draw the underline, then continue to the next character without moving forward
#ifdef VGUI_DRAW_HOTKEYS_ENABLED
				surface()->DrawSetTextPos(x + px, y + py);
				surface()->DrawUnicodeChar('_');
#endif
				continue;
			}
		}

		// see if we've hit the truncated portion of the string
		if (wsz == m_pwszEllipsesPosition)
		{
			// string is truncated, draw ellipses
			for (int i = 0; i < 3; i++)
			{
				surface()->DrawSetTextPos(x + px, y + py);
				surface()->DrawUnicodeChar('.');
				x += surface()->GetCharacterWidth(font, '.');
			}
			break;
		}

		if (currentLineBreak != m_LineBreaks.Count())
		{
			if (wsz == m_LineBreaks[currentLineBreak])
			{
				// newline
				iIndent++;
				if ( m_bWrapCenter && iIndent < m_LineXIndent.Count() )
				{
					x = m_LineXIndent[iIndent];
				}
				else
				{
					x = 0;
				}

				y += lineHeight;
				currentLineBreak++;
			}
		}

		// Underlined text wants to draw the spaces anyway
#if defined( PLATFORM_POSIX ) && !defined( _PS3 )
		float xPos = x;
    
		wchar_t chBefore = 0;
		wchar_t chAfter = 0;
		if ( wsz > _utext )
			chBefore = wsz[-1];
		chAfter = wsz[1];
		if ( m_bAllCaps )
		{
			chBefore = towupper( chBefore );
			chAfter = towupper( chAfter );
		}
		float flWide = 0.0f, flabcA = 0.0f, flabcC = 0.0f;
		surface()->GetKernedCharWidth( font, ch, chBefore, chAfter, flWide, flabcA, flabcC );
        x += floor( flabcA + flWide + flabcC + 0.6f );
    
        //printf( "moving pen to %f,%d\n", floor( xPos + flabcA + 0.6f ) + px, y + py ); 
		surface()->DrawSetTextPos( floor( xPos + flabcA + 0.6f ) + px, y + py );
        //printf( "drawing '%C' - (%f,%f,%f) - %f pixels\n", ch, flabcA, flWide, flabcC, floor( flabcA + flWide + 0.6f ) ); 
		surface()->DrawUnicodeChar(ch);
#else
		surface()->DrawSetTextPos( x + px, y + py);
		surface()->DrawUnicodeChar(ch);
		x+= surface()->GetCharacterWidth(font, ch);
#endif
	}

	// Useful debugging
	/*
	DrawSetColor(GetColor());
	DrawOutlinedRect( 0,0, _drawWidth, tall );
	*/
}

//-----------------------------------------------------------------------------
// Purpose: Get the size of a text string in pixels
//-----------------------------------------------------------------------------
void TextImage::GetTextSize(int &wide, int &tall)
{
	wide = 0;
	tall = 0;
	int maxWide = 0;
	const wchar_t *text = _utext;

	HFont font = _font;
	if ( font == INVALID_FONT )
		return;

	if ( m_bWrap || m_bWrapCenter )
	{
		RecalculateNewLinePositions();
	}

	// For height, use the remapped font
	int fontHeight = surface()->GetFontTall(GetFont());
	tall = fontHeight;

	int textLen = wcslen(text);
	for (int i = 0; i < textLen; i++)
	{
		wchar_t ch = text[i];
		
		if ( m_bAllCaps )
		{
			ch = towupper( ch );
		}

		// handle stupid special characters, these should be removed
		if ( (ch == '&') && !m_bNoShortcutSyntax && (text[i + 1] != 0) )
		{
			continue;
		}
	
		int nCharWidth;
#if defined( PLATFORM_POSIX ) && !defined( _PS3 )
		wchar_t chBefore = 0;
		wchar_t chAfter = 0;
		if ( i > 0 )
			chBefore = text[i-1];
		chAfter = text[i+1];
		float flWide = 0.0f, flabcA, flabcC;
		surface()->GetKernedCharWidth( font, ch, chBefore, chAfter, flWide, flabcA, flabcC );
        // don't include a negative C measure here - over estimate slightly how long the string might be
        flWide = flabcA + flWide; 
        if ( flabcC > 0.0 )
          flWide += flabcC;
        nCharWidth = floor( flWide + 0.6f );
#else
		int a, b, c;
		surface()->GetCharABCwide(font, ch, a, b, c);
		nCharWidth = (a + b + c);
#endif
		wide += nCharWidth;

		if (ch == '\n')
		{
			tall += fontHeight;
			if (wide > maxWide) 
			{
				maxWide=wide;
			}
			wide=0; // new line, wide is reset...
		}

		if ( m_bWrap || m_bWrapCenter )
		{
			for (int j=0; j<m_LineBreaks.Count(); j++)
			{
				if ( &text[i] == m_LineBreaks[j] )
				{
					tall += fontHeight;
					if ( wide > maxWide ) 
					{
						maxWide = wide;
					}

					wide = nCharWidth; // new line, wide is reset...			
				}
			}
		}
		
	}
#ifdef PLATFORM_OSX
	wide += 2;
	if ( textLen < 3 )
		wide += 3;
#endif
	if (wide < maxWide)
	{ 
		// maxWide only gets set if a newline is in the label
		wide = maxWide;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the size of the text in the image
//-----------------------------------------------------------------------------
void TextImage::GetContentSize(int &wide, int &tall)
{
	GetTextSize(wide, tall);
}

//-----------------------------------------------------------------------------
// Purpose: Resize the text image to the content size
//-----------------------------------------------------------------------------
void TextImage::ResizeImageToContent()
{
    int wide, tall;
    GetContentSize(wide, tall);
    SetSize(wide, tall);
}

//-----------------------------------------------------------------------------
// Purpose: Recalculates line breaks
//-----------------------------------------------------------------------------
void TextImage::RecalculateNewLinePositions()
{
	HFont font = GetFont();
	
	float charWidth;
	float x = 0;
	
	//int wordStartIndex = 0;
	wchar_t *wordStartIndex = _utext;
	float wordLength = 0;
	bool hasWord = false;
	bool justStartedNewLine = true;
	bool wordStartedOnNewLine = true;
	
	int startChar = 0;
	
	// clear the line breaks list
	m_LineBreaks.RemoveAll();
	m_LineXIndent.RemoveAll();
	
	// handle the case where this char is a new line, in that case
	// we have already taken its break index into account above so skip it.
	if (_utext[startChar] == '\r' || _utext[startChar] == '\n') 
	{
		startChar++;
	}

	// If we're using Asian word wrap rules, we need to know if the current potential line break is due to a space
	// or because the rules allow us to break between the previous character and the current character.
	bool bStartAsianWordHere = m_bUseAsianWordWrapping;
		
	// loop through all the characters	
	for (wchar_t *wsz = &_utext[startChar]; *wsz != 0; wsz++)
	{
		wchar_t ch = wsz[0];

		if ( m_bAllCaps )
		{
			ch = towupper( ch );
		}

		// line break only on whitespace characters
		if ( !isbreakablewspace(ch) )
		{
			if ( !hasWord || bStartAsianWordHere ) // Asian word-breaking doesn't require spaces between "words"
			{
				// Start a new word
				wordStartIndex = wsz;
				hasWord = true;
				wordStartedOnNewLine = justStartedNewLine;
				wordLength = 0;
			}
			//else append to the current word
		}
		else
		{
			// whitespace/punctuation character
			// end the word
			hasWord = false;
		}

		// Can we start a new word after the current character?
		bStartAsianWordHere = m_bUseAsianWordWrapping && AsianWordWrap::CanBreakAfter( wsz );

		// get the width
#if defined( PLATFORM_POSIX ) && !defined( _PS3 )
		wchar_t chBefore = 0;
		wchar_t chAfter = 0;
		if ( wsz > _utext )
			chBefore = wsz[-1];
		chAfter = wsz[1];
		float flWide = 0.0f, flabcA = 0.0f, flabcC = 0.0f;
		surface()->GetKernedCharWidth( font, ch, chBefore, chAfter, flWide, flabcA, flabcC );
		charWidth = floor(flWide + flabcA + flabcC + 0.6f);
#else
		charWidth = surface()->GetCharacterWidth(font, ch);
#endif
		if (!iswcntrl(ch))
		{
			justStartedNewLine = false;
		}
				
		// check to see if the word is past the end of the line [wordStartIndex, i)
		if ((x + charWidth) > _drawWidth || ch == '\r' || ch == '\n')
		{
			justStartedNewLine = true;
			hasWord = false;
			
			if (ch == '\r' || ch == '\n')
			{
				// set the break at the current character
				//don't do this, paint will manually wrap on newline chars
	//			m_LineBreaks.AddToTail(i);
			}
			else if (wordStartedOnNewLine)
			{
				// word is longer than a line, so set the break at the current cursor
				m_LineBreaks.AddToTail(wsz);
			}
			else
			{
				// set it at the last word Start
				m_LineBreaks.AddToTail(wordStartIndex);
				
				// just back to reparse the next line of text
				// ywb 8/1/07:  Back off one extra char since the 'continue' will increment wsz for us by one in the for loop
				wsz = wordStartIndex - 1;
			}
			
			// reset word length
			wordLength = 0;
			x = 0;
			continue;
		}
		
		// add to the size
		x += charWidth;
		wordLength += charWidth;
	}

	RecalculateCenterWrapIndents();
}

//-----------------------------------------------------------------------------
// Purpose: Calculates where the text should be truncated
//-----------------------------------------------------------------------------
void TextImage::RecalculateEllipsesPosition()
{
	m_bRecalculateTruncation = false;
	m_pwszEllipsesPosition = NULL;

	//don't insert ellipses on wrapped strings
	if ( m_bWrap || m_bWrapCenter )
		return;

	// don't truncate strings with newlines
	if (wcschr(_utext, '\n') != NULL)
		return;

	if ( _drawWidth == 0 )
	{
		int h;
		GetSize( _drawWidth, h );
	}
	
	for ( int check = 0; check < (m_bUseFallbackFont ? 2 : 1); ++check )
	{
		HFont font = GetFont();
		if ( check == 1 && _fallbackFont != INVALID_FONT )
		{
			m_pwszEllipsesPosition = NULL;
			font = _fallbackFont;
			m_bRenderUsingFallbackFont = true;
		}
		
		int ellipsesWidth = 3 * surface()->GetCharacterWidth(font, '.');
		int x = 0;

		for (wchar_t *wsz = _utext; *wsz != 0; wsz++)
		{
			wchar_t ch = wsz[0];

			if ( m_bAllCaps )
			{
				ch = towupper( ch );
			}

			// check for special characters
			if (ch == '\r')
			{
				// ignore, just use \n for newlines
				continue;
			}
			else if ((ch == '&') && !m_bNoShortcutSyntax)
			{
				// "&&" means draw a single ampersand, single one is a shortcut character
				if (wsz[1] == '&')
				{
					// just move on and draw the second ampersand
					wsz++;
				}
				else
				{
					continue;
				}
			}

#if defined( PLATFORM_POSIX ) && !defined( _PS3 )
			wchar_t chBefore = 0;
			wchar_t chAfter = 0;
			if ( wsz > _utext )
				chBefore = wsz[-1];
			chAfter = wsz[1];
			float flWide = 0.0f, flabcA = 0.0f, flabcC = 0.0f;
			surface()->GetKernedCharWidth( font, ch, chBefore, chAfter, flWide, flabcA, flabcC );
			float len = floor(flWide + flabcA + flabcC + 0.6f);
#else
			float len = surface()->GetCharacterWidth(font, ch);
#endif

			// don't truncate the first character
			if (wsz == _utext)
			{
				x += len;
				continue;
			}

			if (x + len + ellipsesWidth > _drawWidth)
			{
				// potential have an ellipses, see if the remaining characters will fit
				int remainingLength = len;
				for (const wchar_t *rwsz = wsz + 1; *rwsz != 0; rwsz++)
				{
#if defined( PLATFORM_POSIX ) && !defined( _PS3 )
                  wchar_t chBefore = 0;
                  wchar_t chAfter = 0;
                  if ( rwsz > _utext )
                    chBefore = *(rwsz-1);
                  chAfter = *(rwsz+1);
                  float flWide = 0.0f, flabcA = 0.0f, flabcC = 0.0f;
                  surface()->GetKernedCharWidth( font, *rwsz, chBefore, chAfter, flWide, flabcA, flabcC );
                  remainingLength += floor(flWide + flabcA + flabcC + 0.6f);
#else
					remainingLength += surface()->GetCharacterWidth(font, *rwsz);
#endif
				}

				if (x + remainingLength > _drawWidth)
				{
					// looks like we've got an ellipses situation
					m_pwszEllipsesPosition = wsz;
					break;
				}
			}

			x += len;
		}

		// Didn't need ellipses...
		if ( !m_pwszEllipsesPosition )
			break;
	}
}

void TextImage::SetUseAsianWordWrapping()
{
	static bool bCheckForAsianLanguage = false;
	static bool bIsAsianLanguage = false;

	if ( !bCheckForAsianLanguage )
	{
		bCheckForAsianLanguage = true;
		const char *pLanguage = scheme()->GetLanguage();
		if ( pLanguage )
		{
			if ( !V_stricmp( pLanguage, "japanese" ) ||
				!V_stricmp( pLanguage, "schinese" ) ||
				!V_stricmp( pLanguage, "tchinese" ) )
			{
				bIsAsianLanguage = true;
			}
		}
	}

	m_bUseAsianWordWrapping = bIsAsianLanguage;
}

void TextImage::SetWrap( bool bWrap )
{
	m_bWrap = bWrap;
	if ( m_bWrap )
	{
		SetUseAsianWordWrapping();
	}
}

void TextImage::SetCenterWrap( bool bWrap )
{
	m_bWrapCenter = bWrap;
	if ( m_bWrapCenter )
	{
		SetUseAsianWordWrapping();
	}
}

void TextImage::SetNoShortcutSyntax( bool bNoShortcutSyntax )
{
	m_bNoShortcutSyntax = bNoShortcutSyntax;
}


void TextImage::SetUseFallbackFont( bool bState, HFont hFallback )
{
	m_bUseFallbackFont = bState;
	_fallbackFont = hFallback;
}

void TextImage::SetAllCaps( bool bAllCaps )
{
	m_bAllCaps = bAllCaps;
}

void TextImage::ResizeImageToContentMaxWidth( int nMaxWidth )
{
	_drawWidth = nMaxWidth;
	// Since we might have to use the "fallback" font, go ahead and recalc the ellipses state first to see if that's the case
	// NOTE:  I think there may be a race condition lurking here, but this seems to work.
	if ( m_bRecalculateTruncation )
	{
		if ( m_bWrap || m_bWrapCenter )
		{
			RecalculateNewLinePositions();
		}

		RecalculateEllipsesPosition();
	}

	ResizeImageToContent();
}

//-----------------------------------------------------------------------------
// Purpose: For center wrapping of multi-line text images, determines the indent each line needs to be centered
//-----------------------------------------------------------------------------
void TextImage::RecalculateCenterWrapIndents()
{
	m_LineXIndent.RemoveAll();

	if ( !m_bWrapCenter )
		return;

	if (!_utext || GetFont() == INVALID_FONT )
		return;

	HFont font = GetFont();
	int px, py;
	GetPos(px, py);

	int currentLineBreak = 0;
	float iCurLineW = 0;

	for (wchar_t *wsz = _utext; *wsz != 0; wsz++)
	{
		wchar_t ch = wsz[0];

		if ( m_bAllCaps )
		{
			ch = towupper( ch );
		}

		// check for special characters
		if (ch == '\r')
		{
			// ignore, just use \n for newlines
			continue;
		}
		else if (ch == '\n')
		{
			int iIdx = m_LineXIndent.AddToTail();
			m_LineXIndent[iIdx] = (_drawWidth - iCurLineW) * 0.5;

			iCurLineW = 0;
			continue;
		}
		else if ((ch == '&') && !m_bNoShortcutSyntax)
		{
			// "&&" means draw a single ampersand, single one is a shortcut character
			if (wsz[1] == '&')
			{
				// just move on and draw the second ampersand
				wsz++;
			}
			else
			{
				// draw the underline, then continue to the next character without moving forward
				continue;
			}
		}

		// Don't need to check ellipses, they're not used when wrapping

		if (currentLineBreak != m_LineBreaks.Count())
		{
			if (wsz == m_LineBreaks[currentLineBreak])
			{
				int iIdx = m_LineXIndent.AddToTail();
				m_LineXIndent[iIdx] = (_drawWidth - iCurLineW) * 0.5;

				iCurLineW = 0;
				currentLineBreak++;
			}
		}

#if defined( PLATFORM_POSIX ) && !defined( _PS3 )
		wchar_t chBefore = 0;
		wchar_t chAfter = 0;
		if ( wsz > _utext )
			chBefore = wsz[-1];
		chAfter = wsz[1];
		float flWide = 0.0f, flabcA = 0.0f, flabcC = 0.0f;
		surface()->GetKernedCharWidth( font, ch, chBefore, chAfter, flWide, flabcA, flabcC );
		iCurLineW += floor(flWide + flabcA + flabcC + 0.6f);
#else
		iCurLineW += surface()->GetCharacterWidth(font, ch);
#endif
	}	

	// Add the final line
	int iIdx = m_LineXIndent.AddToTail();
	m_LineXIndent[iIdx] = (_drawWidth - iCurLineW) * 0.5;
}

void TextImage::SetBounds( int x, int y, int w, int h )
{
	SetPos( x, y );
	SetSize( w, h );
}

void TextImage::AddColorChange( Color col, int iTextStreamIndex )
{
	label_colorchange_t tmpChange;
	tmpChange.color = col;
	tmpChange.textStreamIndex = iTextStreamIndex;
	m_ColorChangeStream.Insert( tmpChange );
}
