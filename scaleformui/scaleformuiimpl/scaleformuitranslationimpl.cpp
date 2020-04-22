//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//


#include "stdafx.h"
#include "tier1/keyvalues.h"
#include "vgui/ILocalize.h"
#include <vstdlib/vstrtools.h>
#include <vstdlib/ikeyvaluessystem.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#define HTML_BUFFER_CHARS 4086
#define MAX_KEYWORD_BUFFER_SIZE 128

// Undefining this for the Pro-Player demo:
// #define FORCE_BUTTON_GLYPHS


/*******************************************************************
 * how button glyphs work:
 *


1. in the translation key, the code looks for an @ sign with a number following ( withoutspaces ) and assumes the number is
   the font size.  So in Flash, you might enter #SFUI_textkey@15 in text with a 15 point font and #SFUI_textkey@25 in text
   with a 25 point font.

2. in the translated text, the code looks for keywords of the form ${keyword} and looks those keywords up in the
   g_buttonFunctionKeywords array below.  The position of the keyword in the array is interpreted as an
   IScaleformUI::ControllerButton::Enum, is used to place the matching button glyph into the string.  For example we can produce
   the glyph for the button normally used to cancel ( the B button on xbox ) by using the string ${cancel} in a translated string.

3. if no button was found to match the keyword, they keyword is matched against the functions currently bound to the buttons
   with the preceing '+' stripped.  That way you can use any of the bindable commands like "attack" or "forward"

4. if the controller button is currently not mapped, then we'll attempt to translate the keyword ( or failing that
   the keyword itself ) and output that as text.

5. if ControllerButton was found, then the button value is then used to lookup a string in the g_controllerButtonNames array.
	This string corresponds to the name of the button image in the sharedlib.swf flash file.

*/



// global buffer used to hold the string after translation and insertion of html codes
wchar_t g_htmlBuffer[HTML_BUFFER_CHARS + 1];

// global buffer used to return wchar_t results when the caller called with char
wchar_t g_wcharBuffer[HTML_BUFFER_CHARS + 1];


// html string used to replace the button mnemonic ( @ will get replaced with the button name )
#if defined( _PS3 ) // Wide strings are %ls on ps3
const wchar_t* g_htmlReplacementString = L"<img src='%ls' align='baseline' width='%d' height='%d' vspace='%d'/>";
#elif defined( POSIX )
const wchar_t* g_htmlReplacementString = L"<img src='%lls' align='baseline' width='%d' height='%d' vspace='%d'/>";
#else
const wchar_t* g_htmlReplacementString = L"<img src='%s' align='baseline' width='%d' height='%d' vspace='%d'/>";
#endif
const int g_defaultGlyphSize=23;
const int g_defaultGlyphOffset=-6;

// these are the names of the button images in sharedlib.swf
// these should be in the same order as IScaleformUI::ControllerButtons
// this will need to be changed for PS3


const wchar_t* g_controllerButtonImageNames[] = {
	L"XBoxA.png",
	L"XBoxB.png",
	L"XBoxX.png",
	L"XBoxY.png",

	L"XBoxLShoulder.png",
	L"XBoxRShoulder.png",
	L"XBoxLTrigger.png",
	L"XBoxRTrigger.png",

	L"XBoxDPadUp.png",
	L"XBoxDPadDown.png",
	L"XBoxDPadLeft.png",
	L"XBoxDPadRight.png",
	L"XBoxDPad.png",

	L"XBoxLStickUp.png",
	L"XBoxLStickDown.png",
	L"XBoxLStickLeft.png",
	L"XBoxLStickRight.png",
	L"XBoxLStickButton.png",
	L"XBoxLStick.png",

	L"XBoxRStickUp.png",
	L"XBoxRStickDown.png",
	L"XBoxRStickLeft.png",
	L"XBoxRStickRight.png",
	L"XBoxRStickButton.png",
	L"XBoxRStick.png",

	L"XBoxStart.png",
	L"XBoxBack.png",

	L"Undefined"
};


const wchar_t* g_controllerButtonImageNamesSony[] = {
	L"x.png",
	L"circle.png",
	L"square.png",
	L"triangle.png",

	L"left-1-shoulder.png",
	L"right-1-shoulder.png",
	L"left-2-shoulder.png",
	L"right-2-shoulder.png",

	L"d-pad-up.png",
	L"d-pad-down.png",
	L"d-pad-left.png",
	L"d-pad-right.png",
	L"d-pad.png",

	L"left-stick-up.png",
	L"left-stick-down.png",
	L"left-stick-left.png",
	L"left-stick-right.png",
	L"left-stick-button.png",
	L"left-stick.png",

	L"right-stick-up.png",
	L"right-stick-down.png",
	L"right-stick-left.png",
	L"right-stick-right.png",
	L"right-stick-button.png",
	L"right-stick.png",

	L"start.png",
	L"select.png",

	L"Undefined"
};

// names that will be shown in place of the button glyphs

const wchar_t* g_controllerButtonPCNames[] = {

	L" [SPACE] ",
	L" [ESC] ",
	NULL,
	NULL,

	NULL,
	NULL,
	NULL,
	NULL,

	L" [UP] ",
	L" [DOWN] ",
	L" [LEFT] ",
	L" [RIGHT] ",
	NULL,

	L" [UP] ",
	L" [DOWN] ",
	L" [LEFT] ",
	L" [RIGHT] ",
	NULL,
	NULL,

	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,

	NULL,
	NULL,
};


// these are the names to embed in strings.  ie. ${dpad}
const wchar_t* g_buttonFunctionKeywords[] = {

	// ** these should be in the order of the IScaleformUI::ControllerButton::Enum
	// and the length of the array should be an even multiple of IScaleformUI::ControllerButton::NumButtons
	// because the location of the keyword in this array is %'d by NumButtons to get a valid IScaleformUI::ControllerButton value

	// these are the generic names

	L"confirm",
	L"cancel",
	L"west",
	L"north",

	L"lshoulder",
	L"rshoulder",
	L"ltrigger",
	L"rtrigger",

	L"dpadup",
	L"dpaddown",
	L"dpadleft",
	L"dpadright",
	L"dpad",

	L"lstickup",
	L"lstickdown",
	L"lstickleft",
	L"lstickright",
	L"lstickbutton",
	L"lstick",

	L"rstickup",
	L"rstickdown",
	L"rstickleft",
	L"rstickright",
	L"rstickbutton",
	L"rstick",

	L"start",
	L"altstart",


	// these are the xbox names
	L"xboxa",				// Confirm,
	L"xboxb",               // Cancel,
	L"xboxx",               // West,
	L"xboxy",               // North,

	L"xboxlb",              // LShoulder,
	L"xboxrb",              // RShoulder,
	L"xboxlt",              // LTrigger,
	L"xboxrt",              // RTrigger,

	L"",                    // DPadUp,
	L"",                    // DPadDown,
	L"",                    // DPadLeft,
	L"",                    // DPadRight,
	L"",                    // DPad,

	L"",                    // LStickUp,
	L"",                    // LStickDown,
	L"",                    // LStickLeft,
	L"",                    // LStickRight,
	L"",                    // LStickButton,
	L"",                    // LStick,

	L"",                    // RStickUp,
	L"",                    // RStickDown,
	L"",                    // RStickLeft,
	L"",                    // RStickRight,
	L"",                    // RStickButton,
	L"",                    // RStick,

	L"xboxstart",           // Start,
	L"xboxback",            // AltStart,


	// and these are the ps3 names

	L"ps3x",				// Confirm,
	L"ps3circle",           // Cancel,
	L"ps3square",           // West,
	L"ps3triangle",         // North,

	L"ps3l1",               // LShoulder,
	L"ps3r1",               // RShoulder,
	L"ps3l2",               // LTrigger,
	L"ps3r2",               // RTrigger,

	L"",                    // DPadUp,
	L"",                    // DPadDown,
	L"",                    // DPadLeft,
	L"",                    // DPadRight,
	L"",                    // DPad,

	L"",                    // LStickUp,
	L"",                    // LStickDown,
	L"",                    // LStickLeft,
	L"",                    // LStickRight,
	L"ps3l3",               // LStickButton,
	L"",                    // LStick,

	L"",                    // RStickUp,
	L"",                    // RStickDown,
	L"",                    // RStickLeft,
	L"",                    // RStickRight,
	L"ps3r3",               // RStickButton,
	L"",                    // RStick,

	L"ps3start",            // Start,
	L"ps3select",           // AltStart,


	NULL, //this is what "undefined" keywords will return
};

// this table maps from the ControllerButton enum to button code defines
int g_controllerButtonToButtonCodeLookup[] =
{
	KEY_XBUTTON_A,
	KEY_XBUTTON_B,
	KEY_XBUTTON_X,
	KEY_XBUTTON_Y,

	KEY_XBUTTON_LEFT_SHOULDER,
	KEY_XBUTTON_RIGHT_SHOULDER,
	KEY_XBUTTON_LTRIGGER,						// ZAXIS POSITIVE
	KEY_XBUTTON_RTRIGGER,						// ZAXIS NEGATIVE

	KEY_XBUTTON_UP,	// POV buttons
	KEY_XBUTTON_DOWN,
	KEY_XBUTTON_LEFT,
    KEY_XBUTTON_RIGHT,
	-1, //dpad

	KEY_XSTICK1_UP,								// YAXIS NEGATIVE
	KEY_XSTICK1_DOWN,							// YAXIS POSITIVE
	KEY_XSTICK1_LEFT,							// XAXIS NEGATIVE
	KEY_XSTICK1_RIGHT,	// XAXIS POSITIVE
	KEY_XBUTTON_STICK1,
	-1, //lstick

	KEY_XSTICK2_UP,								// YAXIS NEGATIVE
	KEY_XSTICK2_DOWN,							// YAXIS POSITIVE
	KEY_XSTICK2_LEFT,							// XAXIS NEGATIVE
	KEY_XSTICK2_RIGHT,	// XAXIS POSITIVE
	KEY_XBUTTON_STICK2,
	-1, //rstick


	KEY_XBUTTON_START,
	KEY_XBUTTON_BACK,
};


/************************
 * locally defined functions
 */

// looks for keyword in g_buttonFunctionKeywords and returns -1 if not found

int MapKeywordToTable( const wchar_t* keyword, const wchar_t** keywordTable )
{
	int result = -1;
	int i = 0;

	while ( *keywordTable )
	{
		if ( **keywordTable && ( V_wcscmp( keyword, *keywordTable ) == 0 ) )
		{
			result = i;
			break;
		}
		keywordTable++;
		i++;
	}

	return result;
}

int CopyKeyword( wchar_t*dest, const wchar_t*src, int destSize )
{
	int copied = 0;

	while ( destSize && *src && *src != L'}' )
	{
		*dest++ = *src++;
		copied++;
		destSize--;
	}

	if ( destSize && copied )
	{
		*dest = 0;
	}

	return copied;

}

int PlaceStringInOutput( const wchar_t* theString, int outindex, bool lengthPreChecked )
{
	if ( theString &&  ( lengthPreChecked || V_wcslen( theString ) < ( HTML_BUFFER_CHARS - outindex ) ) )
	{
		while ( *theString )
		{
			g_htmlBuffer[outindex++] = *theString++;
		}
	}

	return outindex;
}

int PlaceGlyphHTMLIntoOutput( const wchar_t* buttonName, int fontSize, int outindex )
{
	int imageSize = g_defaultGlyphSize;
	int imageOffset = g_defaultGlyphOffset;

	if ( fontSize )
	{
		imageSize = fontSize + ( fontSize >> 2 );
		imageOffset = -fontSize >> 2;
	}

	int res = outindex + V_snwprintf( &g_htmlBuffer[outindex], HTML_BUFFER_CHARS - outindex, g_htmlReplacementString, buttonName, imageSize, imageSize, imageOffset );
	return res;
}

const wchar_t *GetSonyGlyph( int buttonIndex )
{
	if ( buttonIndex == 0 || buttonIndex == 1 )
	{
		// This key's meaning changes depending on the territory: X Button is "confirm/accept/advance" in North America;
		//		CIRCLE button is "confirm/accept/advance" in SCEJ and Asia

		// xcontroller.cpp has already checked this setting on init, and written it into the keyvalues system:
		if ( KeyValuesSystem()->GetKeyValuesExpressionSymbol( "INPUTSWAPAB" ) )
		{
			return g_controllerButtonImageNamesSony[ ( buttonIndex == 0 ) ? 1 : 0 ];			
		}
	}

	return g_controllerButtonImageNamesSony[buttonIndex];
}

// englyphonate

const wchar_t* ScaleformUIImpl::ReplaceGlyphKeywordsWithHTML( const wchar_t* pin, int fontSize, bool bForceControllerGlyph )
{
	if ( !pin )
		return NULL;

	// see if there is an instance of ${ in the string.  If there is, it means
	// we need to replace a keyword with the html to display the button glyph

	const wchar_t* pinwalk = pin;

	bool needsReplacement = false;

	while ( *pinwalk )
	{
		if ( ( *pinwalk == L'$' ) && ( pinwalk[1] == L'{' ) )
		{
			needsReplacement = true;
			break;
		}
		pinwalk++;
	}

	// if there aren't any keywords in this string we can simply return the original string
	if ( !needsReplacement )
	{
		return pin;
	}

	wchar_t keywordBuffer[MAX_KEYWORD_BUFFER_SIZE];

	pinwalk = pin;
	int outindex = 0;
	bool bEscaping = false;

	// we're going to copy the pin string into g_htmlBuffer, but when we find a keyword, we're going to replace
	// the keyword with the html in g_htmlReplacementString ( and in that string we'll insert the correct glyph image name
	// and size

	while ( *pinwalk && outindex < HTML_BUFFER_CHARS )
	{
		// check for the escapement character ( a backslash, just like c )
		if ( !bEscaping && *pinwalk == L'\\' )
		{
			bEscaping = true;
			pinwalk++;

		}
		else if ( bEscaping || *pinwalk != L'$' )
		{
			// the keyword begins with ${, so we can skip it if this char isn't $
			g_htmlBuffer[outindex++] = *pinwalk++;
			bEscaping = false;
		}
		else
		{
			// likewise skip if the character after a '$' is not '{'
			pinwalk++;
			if ( *pinwalk == L'{' )
			{
				pinwalk++;

				// we've found the start of a keyword '${' now we'll copy the keyword
				// out of pin and into keywordBuffer so we can make it lowercase and look it up

				int charsCopied = CopyKeyword( keywordBuffer, pinwalk, MAX_KEYWORD_BUFFER_SIZE );

				// if this keyword is too big ( forgotten } maybe ) or there is no keyword, then cancel replacement
				// and pass back the input string, since the user may want to see that something is wrong
				if ( charsCopied == MAX_KEYWORD_BUFFER_SIZE || !charsCopied )
				{
					return pin;
				}
				else
				{
					// otherwise advance our position in the input string
					pinwalk += charsCopied;

					// the '}' is not copied, so we have to advance past that too
					// Because of how CopyKeyword works, we may be at the end of the string
					// so don't just blindly add 1

					if ( *pinwalk == L'}' )
						pinwalk++;
				}

				if ( keywordBuffer[0] == L'*' )
				{
					// this isn't a keyword, it's actually the name of the glyph
					// so we don't have to do all the lookup and everything
					outindex = PlaceGlyphHTMLIntoOutput( &keywordBuffer[1], fontSize, outindex );
				}
				else
				{
					V_wcslower( keywordBuffer );

					const wchar_t *pSteamControllerGlyph = NULL;

					if ( g_pInputSystem->GetCurrentInputDevice() == INPUT_DEVICE_STEAM_CONTROLLER && steamapicontext != NULL && steamapicontext->SteamController() != NULL )
					{
						char strKeyword[MAX_KEYWORD_BUFFER_SIZE];
						V_wcstostr( keywordBuffer, MAX_KEYWORD_BUFFER_SIZE, strKeyword, MAX_KEYWORD_BUFFER_SIZE );

						const char *pKeyword = strKeyword;
						if ( *pKeyword == '+' )
						{
							++pKeyword;
						}

						if ( V_stricmp( pKeyword, "cancel" ) == 0 )
						{
							pKeyword = "menu_cancel";
						}
						else if ( V_stricmp( pKeyword, "confirm" ) == 0 )
						{
							pKeyword = "menu_select";
						}

						ControllerDigitalActionHandle_t actionHandle = steamapicontext->SteamController()->GetDigitalActionHandle( pKeyword );

						if ( actionHandle != 0 )
						{
							ControllerHandle_t handles[MAX_STEAM_CONTROLLERS];
							int nControllers = steamapicontext->SteamController()->GetConnectedControllers( handles );
							for ( int i = 0; i < nControllers; ++i )
							{
								ControllerDigitalActionData_t data = steamapicontext->SteamController()->GetDigitalActionData( handles[ i ], actionHandle );

								EControllerActionOrigin handleOrigins[ STEAM_CONTROLLER_MAX_ORIGINS ];
								int nOrigins = steamapicontext->SteamController()->GetDigitalActionOrigins( handles[ i ], steamapicontext->SteamController()->GetCurrentActionSet( handles[ i ] ), actionHandle, handleOrigins );

								for ( int j = 0; j < nOrigins; ++j )
								{
									if ( handleOrigins[ j ] != k_EControllerActionOrigin_None )
									{
										switch( handleOrigins[ j ] )
										{
										case k_EControllerActionOrigin_A:     pSteamControllerGlyph = L"XBoxA.png"; break;
										case k_EControllerActionOrigin_B:     pSteamControllerGlyph = L"XBoxB.png"; break;
										case k_EControllerActionOrigin_X:     pSteamControllerGlyph = L"XBoxX.png"; break;
										case k_EControllerActionOrigin_Y:     pSteamControllerGlyph = L"XBoxY.png"; break;
										case k_EControllerActionOrigin_LeftBumper:     pSteamControllerGlyph = L"XBoxLShoulder.png"; break;
										case k_EControllerActionOrigin_RightBumper:    pSteamControllerGlyph = L"XBoxRShoulder.png"; break;
										case k_EControllerActionOrigin_LeftTrigger_Click:   pSteamControllerGlyph = L"XBoxLTrigger.png"; break;
										case k_EControllerActionOrigin_RightTrigger_Click:  pSteamControllerGlyph = L"XBoxRTrigger.png"; break;
										case k_EControllerActionOrigin_LeftGrip:     pSteamControllerGlyph = L"XBoxLShoulder.png"; break;
										case k_EControllerActionOrigin_RightGrip:    pSteamControllerGlyph = L"XBoxRShoulder.png"; break;
										case k_EControllerActionOrigin_Start: pSteamControllerGlyph = L"XBoxStart.png"; break;
										}
									}
								}
							}
						}
					}

					// look up the keyword in the g_buttonFunctionKeyword array.

					IScaleformUI::ControllerButton::Enum bt;
					ButtonCode_t buttonCode;

					int functionIndex = MapKeywordToTable( keywordBuffer, g_buttonFunctionKeywords );

					if ( functionIndex != -1 )
					{
						// if the functionIndex is not -1, it means the keyword was the name of a controller button
						// and we have to mod it because there are three different sets of names in that table
						// a generic, and xbox, and a ps3, so there are more names than buttons.  Modding brings the
						// number into the correct range
						bt = ( IScaleformUI::ControllerButton::Enum ) ( functionIndex % IScaleformUI::ControllerButton::NumButtons );
						if ( bt != IScaleformUI::ControllerButton::Undefined )
						{
							buttonCode = ( ButtonCode_t )g_controllerButtonToButtonCodeLookup[bt];
						}
						else
						{
							buttonCode = BUTTON_CODE_INVALID;
						}
					}
					else
					{
						// if the keyword is not a button name, then see if it is the name
						// of a command that's currently bound to one of the controller buttons
						// this will return ControllerButton::Undefined if the command is not mapped
						// to a controller button
						buttonCode = LookupButtonFromBinding( keywordBuffer, bForceControllerGlyph );
						bt = ValveButtonToControllerButton( buttonCode );
					}

					if ( pSteamControllerGlyph != NULL )
					{
						outindex = PlaceGlyphHTMLIntoOutput( pSteamControllerGlyph, fontSize, outindex );
					}
					else if ( bt == IScaleformUI::ControllerButton::Undefined )
					{
						const wchar_t* translated = NULL;
						wchar_t keyboardKeyName[256];

						if ( buttonCode == BUTTON_CODE_INVALID )
						{
							// if the button is actually undefined ( maybe no button is bound to the function for example )
							// then just insert the localized function name instead

							if (m_bShowActionNameIfUnbound)
							{
								wchar_t commandInBrackets[256];

								V_snwprintf( commandInBrackets, ARRAYSIZE( commandInBrackets ), L"[%s]", LocalizeCommand( keywordBuffer ));
								translated = commandInBrackets;
							}
							else
							{
								translated = L"";
							}
						}
						else
						{
							const char* keyName = g_pInputSystem->ButtonCodeToString( buttonCode );
							if ( keyName && *keyName )
							{
								char keyboardNameBuffer[256];
								V_snprintf(keyboardNameBuffer, sizeof(keyboardNameBuffer), "[%s]", keyName);
								V_strtowcs( keyboardNameBuffer, -1, keyboardKeyName, sizeof( keyboardKeyName ) );
								V_wcsupr( keyboardKeyName );

								translated = keyboardKeyName;
							}
						}

						if ( translated )
						{
							outindex = PlaceStringInOutput( translated, outindex, false );
						}
						else
						{
							outindex = PlaceStringInOutput( keywordBuffer, outindex, false );
						}
					}
					else if ( !IsSetToControllerUI( SF_SS_SLOT(  m_pEngine->GetActiveSplitScreenPlayerSlot() ) ) &&
							  !bForceControllerGlyph &&
							  g_controllerButtonPCNames[bt] != NULL )
					{
						outindex = PlaceStringInOutput( g_controllerButtonPCNames[bt], outindex, false );
					}
					else
					{
						// otherwise construct the correct HTML to display the button glyph and put that into the output buffer
						outindex = PlaceGlyphHTMLIntoOutput( ( IsPS3() || SFINST.GetForcePS3() ) ? GetSonyGlyph(bt) : g_controllerButtonImageNames[bt], fontSize, outindex );
					}
				}

			}
			else
			{
				// if we found a '$' but it was not followed by a '{', we still
				// need to insert the '$'
				g_htmlBuffer[outindex++] = L'$';

			}
		}
	}

	g_htmlBuffer[outindex] = 0;

	return &g_htmlBuffer[0];

}

/********************************
 * scaleformuiimpl methods
 */

void ScaleformUIImpl::InitTranslationImpl( void )
{
	m_bShowActionNameIfUnbound = true;
	RemoveKeyBindings();

	// some commands ( like +attack ) have localized, friendly names.  We're going to
	// load up the localization keys for those commands but looking in the file that's
	// used to set up the button binding screen

	KeyValues* pkeyValueFile = new KeyValues( "options" );

	// Load the config data
	if ( pkeyValueFile )
	{
		pkeyValueFile->LoadFromFile( g_pFullFileSystem, "scripts/controller_options.txt", "game" );

		for ( KeyValues* piter = pkeyValueFile->GetFirstTrueSubKey(); piter; piter = piter->GetNextTrueSubKey() )
		{
			const char* command = piter->GetString( "command", "" );

			if ( command && *command )
			{
				const char* name = piter->GetString( "name", "" );

				if ( name && *name )
				{
					if ( *command == '+' )
					{
						command++;
					}

					int length = V_strlen( command ) + 1;

					wchar_t* commandString = new wchar_t[length];

					V_UTF8ToUnicode( command, commandString, sizeof( wchar_t ) * length );
					V_wcslower( commandString );
					m_LocalizableCommandNames.AddToTail( commandString );

					length = V_strlen( name ) + 1;

					char* keyString = new char[length];
					V_strcpy( keyString, name );

					m_LocalizableCommandKeys.AddToTail( keyString );
				}
			}
		}

		pkeyValueFile->deleteThis();
	}
}

void ScaleformUIImpl::ShowActionNameWhenActionIsNotBound( bool value )
{
	m_bShowActionNameIfUnbound = value;
}

void ScaleformUIImpl::ShutdownTranslationImpl( void )
{
	int i = m_LocalizableCommandNames.Count();

	while ( i-- )
	{
		delete[] m_LocalizableCommandNames[i];
		delete[] m_LocalizableCommandKeys[i];
	}

	m_LocalizableCommandNames.Purge();
	m_LocalizableCommandKeys.Purge();
}

void ScaleformUIImpl::RemoveKeyBindings( void )
{
	V_memset( &m_wcControllerButtonToBindingTable[0][0], 0, sizeof( m_wcControllerButtonToBindingTable ) );
}

void ScaleformUIImpl::BindCommandToControllerButton( ButtonCode_t code, const char* pbinding )
{
	if ( pbinding && *pbinding == '+' )
	{
		pbinding++;
	}

	if ( pbinding && *pbinding )
	{
		V_UTF8ToUnicode( pbinding, &m_wcControllerButtonToBindingTable[code][0], sizeof( wchar_t ) * MAX_BOUND_COMMAND_LENGTH );
		V_wcslower( &m_wcControllerButtonToBindingTable[code][0] );
	}
	else
	{
		m_wcControllerButtonToBindingTable[code][0] = 0;
	}

}

void ScaleformUIImpl::RefreshKeyBindings( void )
{
	// clear our bindings

	RemoveKeyBindings();

	// if we haven't connected to gameui, then just exit

	if ( !m_pGameUIFuncs )
	{
		return;
	}

	// go through each of the buttons on the controller

	for ( int buttonCode = KEY_FIRST; buttonCode < BUTTON_CODE_LAST; buttonCode++ )
	{
		// now the we have a ButtonCode, copy the name of the command ( if there is one ) into our table
		BindCommandToControllerButton( ( ButtonCode_t )buttonCode, m_pGameUIFuncs->GetBindingForButtonCode( ( ButtonCode_t ) buttonCode ) );

	}

}


IScaleformUI::ControllerButton::Enum ScaleformUIImpl::ValveButtonToControllerButton( ButtonCode_t b )
{
	if ( b == KEY_NONE || b == BUTTON_CODE_INVALID )
		return IScaleformUI::ControllerButton::Undefined;

	for ( int i = 0; i < IScaleformUI::ControllerButton::NumButtons; i++ )
	{
		if ( g_controllerButtonToButtonCodeLookup[i] == b )
			return ( IScaleformUI::ControllerButton::Enum )i;
	}

	return IScaleformUI::ControllerButton::Undefined;
}


void ScaleformUIImpl::UpdateBindingForButton( ButtonCode_t bt, const char* pbinding )
{
	BindCommandToControllerButton( bt, pbinding );
}


const wchar_t* ScaleformUIImpl::LocalizeCommand( const wchar_t* command )
{
	int i = 0;
	int len = ( int ) m_LocalizableCommandNames.Count();

	while ( i < len )
	{
		if ( !V_wcscmp( command, m_LocalizableCommandNames[i] ) )
		{
			return g_pVGuiLocalize->Find( m_LocalizableCommandKeys[i] );
		}
		i++;
	}

	return command;
}

ButtonCode_t ScaleformUIImpl::LookupButtonFromBinding( const wchar_t* binding, bool bForceControllerLookup )
{
	if ( *binding == L'+' )
		binding++;

	int slot = m_pEngine->GetActiveSplitScreenPlayerSlot();

	int firstKey;
	int lastKey;
	bool isController = false;


	if ( bForceControllerLookup || IsSetToControllerUI( SF_SS_SLOT( slot ) ) )
	{
		isController = true;
		firstKey = ButtonCodeToJoystickButtonCode( JOYSTICK_FIRST, slot );
		lastKey = ButtonCodeToJoystickButtonCode( KEY_XSTICK2_UP, slot );
	}
	else
	{
		firstKey = KEY_FIRST;
		lastKey = MOUSE_LAST;
	}

	for ( int i = firstKey; i <= lastKey; i++ )
	{
		if ( !V_wcscmp( binding, &m_wcControllerButtonToBindingTable[i][0] ) )
		{
			return GetBaseButtonCode( ( ButtonCode_t )i );
		}
	}

	if ( !V_wcscmp( binding, L"buymenu" ) && isController )
	{
		return LookupButtonFromBinding( L"use" );
	}
	else
	{
		return BUTTON_CODE_INVALID;
	}
}

/*********************************************
 * This function currently uses a global static buffer to make calling it simpler.
 * This means, of course, that the result must be used or copied immediately, and that
 * it is not thread safe.
 *
 * To solve this problem, all these functions should probably be changed to accept a
 * buffer from the caller.
 */

const wchar_t* ScaleformUIImpl::ReplaceGlyphKeywordsWithHTML( const char* text, int fontSize, bool bForceControllerGlyph  )
{
	int stringLength = ( V_strlen( text ) + 1 ) * sizeof( wchar_t );

	if ( stringLength > HTML_BUFFER_CHARS )
		stringLength = HTML_BUFFER_CHARS;

	V_UTF8ToUnicode( text, g_wcharBuffer, stringLength );

	return ReplaceGlyphKeywordsWithHTML( g_wcharBuffer, fontSize, bForceControllerGlyph );
}


const wchar_t* ScaleformUIImpl::Translate( const char *key, bool* pIsHTML )
{
	// first strip out any font size hints

	const wchar_t* replaced = NULL;
	const wchar_t* translated = NULL;
	int fontSize = 0;
	const char* actualKey = key;
	
	if ( pIsHTML )
	{
		*pIsHTML = false;
	}

	// if the key doesn't start with '#' then the g_pVGuiLocalize->Find function
	// is going to return null anyway, so don't bother doing any work
	if ( key[0] == '#' )
	{
		// if this is a translation key, then look to see if it has a '@' sign
		// with a following point size.  If it does, then we'll make all the glyphs
		// in this string that size

		int len = V_strlen( key );

		if ( V_strnchr( key, '@', len + 1 ) )
		{
			char *asciiString = ( char * ) stackalloc( len + 1 );

			char *pdwalk;
			const char* pswalk;
			bool bEscaping = false;

			// this is a little weird, but we only want to escape the '@' sign here.
			// we don't want to remove the escapements for other things because
			// that will lead to inconsistent behavior ( since we'd only remove the escapements
			// if the string started with # and had a @ in it )

			pswalk = key;
			pdwalk = asciiString;
			while ( *pswalk )
			{
				if ( !bEscaping && *pswalk == '\\' )
				{
					bEscaping = true;
					pswalk++;
				}
				else if ( *pswalk != '@' )
				{
					if ( bEscaping )
						*pdwalk++ = '\\';
					*pdwalk++ = *pswalk++;
					bEscaping = false;
				}
				else if ( bEscaping )
				{
					*pdwalk++ = *pswalk++;
					bEscaping = false;
				}
				else
				{
					break;
				}
			}

			if ( bEscaping )
			{
				*pdwalk++ = '\\';
			}
				
			*pdwalk = 0;
			actualKey = asciiString;

			if ( *pswalk == '@' )
			{
				pswalk++;
			}

			if ( *pswalk )
			{
				fontSize = atoi( pswalk );

				if ( fontSize < 0 )
					fontSize = 0;
			}
		}

		translated = g_pVGuiLocalize->Find( actualKey );
	}

	bool bShowConsoleButtonIcons = false;
	// we only show console bindings on PS3 when you have a MKB hooked up because most of the nav UI is either bad or unfinished for MKB 
	if ( IsPS3() && g_pInputSystem->IsDeviceReadingInput( INPUT_DEVICE_KEYBOARD_MOUSE ) )
	{
		bShowConsoleButtonIcons = true;
	}

	if ( translated )
	{
		replaced = ReplaceGlyphKeywordsWithHTML( translated, fontSize, bShowConsoleButtonIcons ); 

		if ( pIsHTML )
		{
			*pIsHTML = ( bool ) ( replaced != translated );
		}
	}
	else
	{
		replaced = ReplaceGlyphKeywordsWithHTML( actualKey, fontSize, bShowConsoleButtonIcons );
		if ( pIsHTML )
		{
			*pIsHTML = true;
		}
	}

	return replaced;
}


static const wchar_t *g_pCharsToBeReplaced = L"\\<>&\'\"$#";

static const wchar_t *g_pReplacementStrings[] = {
	L"&#92;",
	L"&lt;",
	L"&gt;",
	L"&amp;",
	L"&apos;",
	L"&quot;",
	L"&#36;",
	L"&#35;"
};


void ScaleformUIImpl::MakeStringSafe( const wchar_t* oldName, wchar_t* newName, int destBufferSize )
{
	const wchar_t* pfound;
	wchar_t* pout = newName;
	int charsLeft = ( destBufferSize / sizeof(wchar_t) ) - 1;
	bool bEncounteredIllegalCharacters = false;

	// throw a zero at the end just in case
	newName[charsLeft] = L'\0';

	for( const wchar_t *p=oldName; *p != 0 && charsLeft > 0; p++ )
	{
		// If we are about to write first character and it is a hash then skip it because it is reserved for localization
		if ( ( pout == newName ) && ( *p == L'#' ) )
		{
			bEncounteredIllegalCharacters = true;
			continue;
		}

		// Check the character for replacement sequences
		pfound = wcschr( g_pCharsToBeReplaced, *p );

		if ( pfound )
		{
			int index = pfound - g_pCharsToBeReplaced;
			int replacementLength = wcslen( g_pReplacementStrings[index] );
			if ( replacementLength <= charsLeft )
			{
				V_wcsncpy( pout, g_pReplacementStrings[index], charsLeft * sizeof( wchar_t ) );
				charsLeft -= replacementLength;
				pout += replacementLength;
			}
			else
				break;
		}
		else
		{
#if 1
			if ( *p )
#else
			if ( iswprint( *p ) || ( *p == L'\t' ) )
#endif
			{
				*pout++ = *p;
				charsLeft--;
			}
			else
			{
				bEncounteredIllegalCharacters = true;
			}
		}
	}

	// If we didn't write any safe characters, but encountered illegal characters then write at least a question-mark
	if ( ( pout == newName ) && ( charsLeft > 0 ) && bEncounteredIllegalCharacters )
	{
		*pout++ = L'?';
		charsLeft--;
	}

	if ( charsLeft >= 0 ) // 0 is okay because we started charsLeft off as one too small
		*pout = 0;
}

void ScaleformUIImpl::DecodeButtonandSlotFromButtonCode( ButtonCode_t inCode, ButtonCode_t &outCode, int &outSlot )
{
	outCode = GetBaseButtonCode( inCode );

	if ( !IsJoystickCode( inCode ) )
	{
		outSlot = m_iKeyboardSlot;
	}
	else
	{
		outSlot = GetJoystickForCode( inCode );
	}
}

