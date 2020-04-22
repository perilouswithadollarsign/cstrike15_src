//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "keys.h"
#include "cdll_engine_int.h"
#include "cmd.h"
#include "toolframework/itoolframework.h"
#include "toolframework/itoolsystem.h"
#include "tier1/utlbuffer.h"
#include "vgui_baseui_interface.h"
#include "tier2/tier2.h"
#include "inputsystem/iinputsystem.h"
#include "keyvalues.h"
#include "host.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "cheatcodes.h"
#include "baseclientstate.h" // splitscreen interface
#include "tier1/fmtstr.h"
#include "EngineSoundInternal.h"
#ifndef DEDICATED
#include "cl_splitscreen.h"
#endif

#if defined( INCLUDE_SCALEFORM )
#include "scaleformui/scaleformui.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar in_forceuser( "in_forceuser", "0", FCVAR_CHEAT, "Force user input to this split screen player." );

enum KeyUpTarget_t
{
	KEY_UP_ANYTARGET = 0,
	KEY_UP_ENGINE,
	KEY_UP_VGUI,
	KEY_UP_TOOLS,
	KEY_UP_CLIENT,
	KEY_UP_GAMEUI,
	KEY_UP_SCALEFORM,
	KEY_UP_OVERLAY,

	KEY_UP_TARGET_COUNT
};

struct KeyInfo_t
{
    char			*m_pKeyBinding;
	unsigned char	m_nKeyUpTarget : 4;	// see KeyUpTarget_t
	unsigned char	m_bKeyDown : 1;
};


//-----------------------------------------------------------------------------
// Current keypress state
//-----------------------------------------------------------------------------
struct KeyContext_t
{
	KeyContext_t()
	{
		// NOTE: If this compile-time assert fails, modify the bit count
		// in KeyInfo_t::m_nKeyUpTarget
		COMPILE_TIME_ASSERT( KEY_UP_TARGET_COUNT < 16 );

		Q_memset( m_pKeyInfo, 0, sizeof( m_pKeyInfo ) );
		m_bTrapMode = false;
		m_bDoneTrapping = false;
		m_nTrapKeyUp = BUTTON_CODE_INVALID;
		m_nTrapKey = BUTTON_CODE_INVALID;
	}

	KeyInfo_t		m_pKeyInfo[BUTTON_CODE_LAST];
	//-----------------------------------------------------------------------------
	// Trap mode is used by the keybinding UI
	//-----------------------------------------------------------------------------
	bool			m_bTrapMode;
	bool			m_bDoneTrapping;
	ButtonCode_t	m_nTrapKeyUp;
	ButtonCode_t	m_nTrapKey;
};

static KeyContext_t s_KeyContext;

//-----------------------------------------------------------------------------
// Can keys be passed to various targets?
//-----------------------------------------------------------------------------
static inline bool ShouldPassKeyUpToTarget( ButtonCode_t code, KeyUpTarget_t target )
{
	if (code < 0 || code >= BUTTON_CODE_LAST)
		return true;
	return ( s_KeyContext.m_pKeyInfo[code].m_nKeyUpTarget == target ) || ( s_KeyContext.m_pKeyInfo[code].m_nKeyUpTarget == KEY_UP_ANYTARGET );
}

ButtonCode_t GetSplitPlayerJoystickCode( ButtonCode_t bc )
{
#ifdef DEDICATED
	return bc;
#else
	if ( !IsJoystickCode( bc ) && !IsSteamControllerCode( bc ) )
		return bc;

	int nJoystick = GET_ACTIVE_SPLITSCREEN_SLOT();
	return ButtonCodeToJoystickButtonCode( bc, nJoystick );
#endif
}

void Key_SetBinding( ButtonCode_t keynum, const char *pBinding )
{
	char	*pNewBinding;
	int		l;
			
	if ( keynum == BUTTON_CODE_INVALID )
		return;

	keynum = GetSplitPlayerJoystickCode( keynum );

	// free old bindings
	if ( s_KeyContext.m_pKeyInfo[keynum].m_pKeyBinding )
	{
		// Exactly the same, don't re-bind and fragment memory
		if ( !Q_strcmp( s_KeyContext.m_pKeyInfo[keynum].m_pKeyBinding, pBinding ) )
			return;

		delete[] s_KeyContext.m_pKeyInfo[keynum].m_pKeyBinding;
		s_KeyContext.m_pKeyInfo[keynum].m_pKeyBinding = NULL;
	}
			
	// allocate memory for new binding
	l = Q_strlen( pBinding );	
	pNewBinding = (char *)new char[ l+1 ];
	Q_strncpy( pNewBinding, pBinding, l + 1 );
	pNewBinding[l] = 0;
	s_KeyContext.m_pKeyInfo[keynum].m_pKeyBinding = pNewBinding;	

#if defined( INCLUDE_SCALEFORM )
	if ( g_pScaleformUI )
	{
		g_pScaleformUI->UpdateBindingForButton( keynum, pBinding );
	}
#endif

#ifndef DEDICATED
	if ( g_ClientDLL )
	{
		g_ClientDLL->OnKeyBindingChanged( keynum, g_pInputSystem->ButtonCodeToString( (ButtonCode_t)keynum ), pNewBinding );
	}
#endif
}

/*
===================
Key_Unbind_f
===================
*/
CON_COMMAND( unbind, "Unbind a key." )
{
	ButtonCode_t b;

	if ( args.ArgC() != 2 )
	{
		ConMsg( "unbind <key> : remove commands from a key\n" );
		return;
	}
	
	b = g_pInputSystem->StringToButtonCode( args[1] );
	if ( b == BUTTON_CODE_INVALID )
	{
		ConMsg( "\"%s\" isn't a valid key\n", args[1] );
		return;
	}

	if ( b == KEY_ESCAPE )
	{
		ConMsg( "Can't unbind ESCAPE key\n" );
		return;
	}

	Key_SetBinding( b, "" );
}

CON_COMMAND( unbindall, "Unbind all keys." )
{
	int		i;
	
	for ( i=0; i<BUTTON_CODE_LAST; i++ )
	{
		if ( !s_KeyContext.m_pKeyInfo[i].m_pKeyBinding )
			continue;
		
		// Don't ever unbind escape or console key
		if ( i == KEY_ESCAPE )	
			continue;

		if ( i == KEY_BACKQUOTE )
			continue;

		Key_SetBinding( (ButtonCode_t)i, "" );
	}
}

CON_COMMAND( unbindalljoystick, "Unbind all joystick keys." )
{
	int		i;

	for ( i = 0; i < BUTTON_CODE_LAST; i++ )
	{
		if ( !s_KeyContext.m_pKeyInfo[i].m_pKeyBinding )
			continue;

		// Don't ever unbind escape or console key
		if ( i == KEY_ESCAPE )	
			continue;

		if ( i == KEY_BACKQUOTE )
			continue;

		if ( IsJoystickCode( ( ButtonCode_t )i ) )
		{
			Key_SetBinding( (ButtonCode_t)i, "" );
		}
	}
}

CON_COMMAND( unbindallmousekeyboard, "Unbind all mouse / keyboard keys." )
{
	int		i;

	for ( i = 0; i < BUTTON_CODE_LAST; i++ )
	{
		if ( !s_KeyContext.m_pKeyInfo[i].m_pKeyBinding )
			continue;

		// Don't ever unbind escape or console key
		if ( i == KEY_ESCAPE )	
			continue;

		if ( i == KEY_BACKQUOTE )
			continue;

		if ( IsMouseCode( ( ButtonCode_t )i ) || IsKeyCode( ( ButtonCode_t )i ) )
		{
			Key_SetBinding( (ButtonCode_t)i, "" );
		}
	}
}


#ifndef DEDICATED
CON_COMMAND_F( escape, "Escape key pressed.", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	EngineVGui()->HideGameUI();
}
#endif



/*
===================
Key_Bind_f
===================
*/
static void BindHelper( const CCommand &args )
{
	int i, c;
	ButtonCode_t b;
	char cmd[1024];
	
	c = args.ArgC();

	if ( c != 2 && c != 3 )
	{
		ConMsg( "bind <key> [command] : attach a command to a key\n" );
		return;
	}

	b = g_pInputSystem->StringToButtonCode( args[1] );
	if ( b == BUTTON_CODE_INVALID )
	{
		ConMsg( "\"%s\" isn't a valid key\n", args[1] );
		return;
	}

	if ( c == 2 )
	{
		b = GetSplitPlayerJoystickCode( b );

		if (s_KeyContext.m_pKeyInfo[b].m_pKeyBinding)
		{
			ConMsg( "\"%s\" = \"%s\"\n", args[1], s_KeyContext.m_pKeyInfo[b].m_pKeyBinding );
		}
		else
		{
			ConMsg( "\"%s\" is not bound\n", args[1] );
		}
		return;
	}

	if ( b == KEY_ESCAPE )
	{
		Q_strncpy( cmd, "cancelselect", sizeof( cmd ) );
	}
	else
	{	
		// copy the rest of the command line
		cmd[0] = 0;		// start out with a null string
		for ( i=2 ; i< c ; i++ )
		{
			if (i > 2)
			{
				Q_strncat( cmd, " ", sizeof( cmd ), COPY_ALL_CHARACTERS );
			}
			Q_strncat( cmd, args[i], sizeof( cmd ), COPY_ALL_CHARACTERS );
		}
	}
	Key_SetBinding( b, cmd );
}

CON_COMMAND( bind, "Bind a key." )
{
	BindHelper( args );
}

CON_COMMAND( bind_osx, "Bind a key for OSX only." )
{
	if ( IsOSX() )
		BindHelper( args );
}


/*
============
GetDefaultKeyBindings

Returns a KeyValues object holding default keybindings.  Caller is responsible for freeing
the KeyValues object.
============
*/
KeyValues *GetDefaultKeyBindings( void )
{
	KeyValues *defaults = new KeyValues( "defaults" );

	FileHandle_t f;
	char szFileName[ _MAX_PATH ];
	char token[ 1024 ];
	char szKeyName[ 256 ];

	// read kb_def file to get default key binds
	Q_snprintf( szFileName, sizeof( szFileName ), "%skb_def" PLATFORM_EXT ".lst", SCRIPT_DIR );
	f = g_pFileSystem->Open( szFileName, "r");
	if ( !f )
	{
		ConMsg( "Couldn't open kb_def.lst\n" );
		return defaults;
	}

	// read file into memory
	int size = g_pFileSystem->Size(f);
	char *startbuf = new char[ size + 1 ];
	g_pFileSystem->Read( startbuf, size, f );
	g_pFileSystem->Close( f );
	startbuf[size] = 0;

	const char *buf = startbuf;
	while ( 1 )
	{
		buf = COM_ParseFile( buf, token, sizeof( token ) );
		if ( strlen( token ) <= 0 )
			break;
		Q_strncpy ( szKeyName, token, sizeof( szKeyName ) );

		buf = COM_ParseFile( buf, token, sizeof( token ) );
		if ( strlen( token ) <= 0 )  // Error
			break;

		defaults->SetString( token, szKeyName );
	}
	delete [] startbuf;		// cleanup on the way out

	return defaults;
}

const char *GetSuggestedBinding( const char *command, KeyValues *defaults )
{
	if ( !defaults )
		return NULL;

	const char *suggestedKeyString = defaults->GetString( command, NULL );
	if ( suggestedKeyString )
	{
		return suggestedKeyString;
	}

	// If no exact match, scan for substring matches
	for ( KeyValues *keybind = defaults->GetFirstSubKey(); keybind != NULL; keybind = keybind->GetNextKey() )
	{
		const char *suggestedKeyString = keybind->GetString();
		const char *suggestedCommand = keybind->GetName();

		const char *subStr = V_stristr( suggestedCommand, command );
		if ( subStr != NULL )
		{
			return suggestedKeyString;
		}
	}

	return NULL;
}

/*
============
Key_ForceBind_f

Find empty keys for the incoming command strings and bind them
============
*/
void Key_ForceBind_f( const CCommand &args )
{
	int argc = args.ArgC();
	if ( argc < 2 )
		return;

	KeyValues *defaults = GetDefaultKeyBindings();
	Color boundColor( 0, 255, 64, 255 );
	Color unboundColor( 255, 0, 64, 255 );

	for ( int arg = 1; arg<argc; ++arg )
	{
		const char *command = args[arg];
		const char *currentKeyBinding = Key_NameForBinding(command);
		const char *suggestedKeyString = GetSuggestedBinding(command, defaults);

		if ( !suggestedKeyString && currentKeyBinding )
		{
			// if we don't have a suggested keybind, unbind the command
			ButtonCode_t key = g_pInputSystem->StringToButtonCode( currentKeyBinding );
			if ( key == - 1 || key == KEY_ESCAPE )
			{
				continue;
			}

			ConColorMsg( boundColor, "Unbound obsolete command \"%s\"\n", command );
			Key_SetBinding( key, "" );
			continue;
		}

		if ( currentKeyBinding != NULL )
			continue;	// Already bound

		if ( suggestedKeyString == NULL )
			continue;	// Was a key to be unbound

		ButtonCode_t suggestedKeynum = g_pInputSystem->StringToButtonCode( suggestedKeyString );

		const char *suggestedBind = Key_BindingForKey( suggestedKeynum );
		if ( suggestedKeynum != -1 && (suggestedBind == NULL || *suggestedBind == '\0') )
		{
			ConColorMsg( boundColor, "Bound \"%s\" to key %s\n", command, suggestedKeyString );
			Key_SetBinding(suggestedKeynum, command);
		}
		else
		{
			// Their suggestion was taken, so we need to find a key since we can't return empty handed.
			int newTry = suggestedKeynum + 1;
			if ( newTry > KEY_SCROLLLOCKTOGGLE )
				newTry = KEY_0;
			while( newTry != suggestedKeynum )
			{
				const char *newString = g_pInputSystem->ButtonCodeToString( (ButtonCode_t)newTry );
				const char *newTryBind = Key_BindingForKey( (ButtonCode_t)newTry );
				if ( newTryBind == NULL || *newTryBind == '\0' )	// Never set, or unset.
				{
					ConColorMsg( boundColor, "Bound \"%s\" to key %s\n", command, newString );
					Key_SetBinding( (ButtonCode_t)newTry, command );
					break;
				}

				newTry++;
				if ( newTry > KEY_SCROLLLOCKTOGGLE )
					newTry = KEY_0;
			}
		}

		if ( Key_NameForBinding(command) == NULL )
		{
			ConColorMsg( unboundColor, "Unable to bind \"%s\" to a key\n", command );
		}
	}

	defaults->deleteThis();
}
static ConCommand forcebind("forcebind",Key_ForceBind_f, "Bind a command to an available key. (forcebind command opt:suggestedKey)" );

/*
============
Key_CountBindings

Count number of lines of bindings we'll be writing
============
*/
int  Key_CountBindings( void )
{
	int		i;
	int		c = 0;

	for ( i = 0; i < BUTTON_CODE_LAST; i++ )
	{
		if ( !s_KeyContext.m_pKeyInfo[i].m_pKeyBinding || !s_KeyContext.m_pKeyInfo[i].m_pKeyBinding[0] )
			continue;

		c++;
	}

	return c;
}

/*
============
Key_WriteBindings

Writes lines containing "bind key value"
============
*/
void Key_WriteBindings( CUtlBuffer &buf, const int iSplitscreenSlot /*= -1*/ )
{
	int		i;

	CUtlVector< CUtlString > deferred[ MAX_SPLITSCREEN_CLIENTS ];
	for ( i = 0 ; i < BUTTON_CODE_LAST ; i++ )
	{
		char const *pszBinding = s_KeyContext.m_pKeyInfo[i].m_pKeyBinding;

		if ( !pszBinding || !*pszBinding)
			continue;

		char const *pszKeyName = g_pInputSystem->ButtonCodeToString( (ButtonCode_t)i );

		int nSlot = GetJoystickForCode( (ButtonCode_t)i );

		if ( iSplitscreenSlot >= 0 )
		{
			// only bind commands for this controller, don't use cmd2
			if ( iSplitscreenSlot != nSlot )
				continue;

			buf.Printf( "bind \"%s\" \"%s\"\n", pszKeyName, pszBinding );
		}
		else
		{
			if ( nSlot == 0 )
			{
				buf.Printf( "bind \"%s\" \"%s\"\n", pszKeyName, pszBinding );
			}
			else
			{
				CUtlString str;
				str = CFmtStrN< 2048 >( "cmd%d bind \"%s\" \"%s\"\n", nSlot + 1, pszKeyName, pszBinding );
				deferred[ nSlot ].AddToTail( str );
			}
		}		
	}

	for ( int nSlot = 1; nSlot < host_state.max_splitscreen_players; ++nSlot )
	{
		int c = deferred[ nSlot ].Count();
		for ( int i = 0; i < c; ++i )
		{
			buf.Printf( "%s", deferred[ nSlot ][ i ].String() );
		}
	}
}


/*
============
Key_NameForBinding

Returns the keyname to which a binding string is bound.  E.g., if 
TAB is bound to +use then searching for +use will return "TAB"
============
*/

static bool IsKeyBoundedToBinding( int i, const char* pBind )
{
	char *pszBinding = s_KeyContext.m_pKeyInfo[i].m_pKeyBinding;

	if ( pszBinding && *pszBinding )
	{
		if ( !strchr( pszBinding, ';' ) )
		{
			// Not a list of commands, do it the easy way
			if ( pszBinding[ 0 ] == '+' )
			{
				++pszBinding;
			}

			if ( !Q_strcasecmp( pszBinding, (char *)pBind ) )
			{
				return true;
			}
		}
		else
		{
			// Tokenize the binding name (could be more than one binding)
			char szBinding[ 256 ];
			Q_strncpy( szBinding, pszBinding, sizeof( szBinding ) );

			pszBinding = strtok( szBinding, ";" );

			while ( pszBinding )
			{
				if ( pszBinding[ 0 ] == '+' )
				{
					++pszBinding;
				}

				if ( !Q_strcasecmp( pszBinding, (char *)pBind ) )
				{
					return true;
				}

				pszBinding = strtok( NULL, ";" );
			}
		}
	}

	return false;
}

const char *Key_NameForBinding( const char *pBinding, int userId, int iStartCount, BindingLookupOption_t nFlags )
{
	int i = Key_CodeForBinding( pBinding, userId, iStartCount, nFlags );

	if( pBinding && i != BUTTON_CODE_INVALID )
	{
		const char *pBind = pBinding;

		if ( pBinding[0] == '+' )
		{
			++pBind;
		}

		if ( IsKeyBoundedToBinding( i, pBind ) )
		{
			return g_pInputSystem->ButtonCodeToString( (ButtonCode_t)i );
		}
	}

	return NULL;
}


/*
============
Key_CodeForBinding

Returns the keycode to which a binding string is bound.  E.g., if 
TAB is bound to +use then searching for +use will return KEY_TAB
============
*/
int Key_CodeForBinding( const char *pBinding, int userId, int iStartCount, BindingLookupOption_t nFlags )
{
	if( pBinding )
	{
		const char *pBind = pBinding;

		if ( pBinding[0] == '+' )
		{
			++pBind;
		}

		int iCount = 0;

		if ( IsPC() || userId < 0 || nFlags == BINDINGLOOKUP_ALL )
		{
			for( int i=0 ; i < BUTTON_CODE_LAST ; i++ )
			{
				if( IsKeyBoundedToBinding( i, pBind ) )
				{
					if ( nFlags == BINDINGLOOKUP_ALL ||
						( nFlags == BINDINGLOOKUP_STEAMCONTROLLER_ONLY && ( i >= STEAMCONTROLLER_FIRST && i <= STEAMCONTROLLER_LAST ) ) ||
						 ( nFlags == BINDINGLOOKUP_KEYBOARD_ONLY && ( i < JOYSTICK_FIRST || i > JOYSTICK_LAST ) && ( i < STEAMCONTROLLER_FIRST || i > STEAMCONTROLLER_LAST ) ) || 
						 ( nFlags == BINDINGLOOKUP_JOYSTICK_ONLY && ( i >= JOYSTICK_FIRST && i <= JOYSTICK_LAST ) ) )
					{
						if ( iCount == iStartCount )
						{
							return i;
						}
						else
						{
							++iCount;
						}
					}
				}
			}
		}
		else
		{
			int first = userId * JOYSTICK_MAX_BUTTON_COUNT + JOYSTICK_FIRST_BUTTON;
			int last = first + JOYSTICK_MAX_BUTTON_COUNT;

			for( int i = first; i < last; ++i )
			{
				if( IsKeyBoundedToBinding( i, pBind ) )
				{
					if ( iCount == iStartCount )
					{
						return i;
					}
					else
					{
						++iCount;
					}
				}
			}

			first = userId * JOYSTICK_POV_BUTTON_COUNT + JOYSTICK_FIRST_POV_BUTTON;
			last  = first + JOYSTICK_POV_BUTTON_COUNT;
			for( int i = first; i < last; ++i )
			{
				if( IsKeyBoundedToBinding( i, pBind ) )
				{
					if ( iCount == iStartCount )
					{
						return i;
					}
					else
					{
						++iCount;
					}
				}
			}
		
			first = userId * JOYSTICK_AXIS_BUTTON_COUNT + JOYSTICK_FIRST_AXIS_BUTTON;
			last  = first + JOYSTICK_AXIS_BUTTON_COUNT;
			for( int i = first; i < last; ++i )
			{
				if( IsKeyBoundedToBinding( i, pBind ) )
				{
					if ( iCount == iStartCount )
					{
						return i;
					}
					else
					{
						++iCount;
					}
				}
			}
			
			first = userId * STEAMCONTROLLER_MAX_BUTTON_COUNT + STEAMCONTROLLER_FIRST_BUTTON;
			last = first + STEAMCONTROLLER_MAX_BUTTON_COUNT;

			for( int i = first; i < last; ++i )
			{
				if( IsKeyBoundedToBinding( i, pBind ) )
				{
					if ( iCount == iStartCount )
					{
						return i;
					}
					else
					{
						++iCount;
					}
				}
			}

			first = userId * STEAMCONTROLLER_AXIS_BUTTON_COUNT + STEAMCONTROLLER_FIRST_AXIS_BUTTON;
			last  = first + STEAMCONTROLLER_AXIS_BUTTON_COUNT;
			for( int i = first; i < last; ++i )
			{
				if( IsKeyBoundedToBinding( i, pBind ) )
				{
					if ( iCount == iStartCount )
					{
						return i;
					}
					else
					{
						++iCount;
					}
				}
			}
			
		}

		// Xbox 360 controller: Handle the dual bindings for duck and zoom
		if ( !Q_stricmp( "duck", pBind ) )
			return Key_CodeForBinding( "toggle_duck", userId, iCount );

		if ( !Q_stricmp( "zoom", pBind ) )
			return Key_CodeForBinding( "toggle_zoom", userId, iCount );

	}

	return BUTTON_CODE_INVALID;
}

const char *Key_BindingForKey( ButtonCode_t code )
{
	if ( code < 0 || code > BUTTON_CODE_LAST )
		return NULL;
	return s_KeyContext.m_pKeyInfo[ code ].m_pKeyBinding;
}

CON_COMMAND( key_listboundkeys, "List bound keys with bindings." )
{
	int i;

	for (i=0 ; i<BUTTON_CODE_LAST ; i++)
	{
		const char *pBinding = Key_BindingForKey( (ButtonCode_t)i );
		if ( !pBinding || !pBinding[0] )
			continue;

		if ( !IsJoystickCode( (ButtonCode_t)i ) && !IsSteamControllerCode( (ButtonCode_t)i ) )
		{
			ConMsg( "\"%s\" = \"%s\"\n", g_pInputSystem->ButtonCodeToString( (ButtonCode_t)i ), pBinding );
		}
		else
		{
			int nSlot = GetJoystickForCode( (ButtonCode_t)i );
			ConMsg( "[%d:%d]\"%s\" = \"%s\"\n", i, nSlot, g_pInputSystem->ButtonCodeToString( (ButtonCode_t)i ), pBinding );
		}
	}
}

CON_COMMAND( key_findbinding, "Find key bound to specified command string." )
{
	if ( args.ArgC() != 2 )
	{
		ConMsg( "usage:  key_findbinding substring\n" );
		return;
	}

	const char *substring = args[1];
	if ( !substring || !substring[ 0 ] )
	{
		ConMsg( "usage:  key_findbinding substring\n" );
		return;
	}

	int i;

	for (i=0 ; i<BUTTON_CODE_LAST ; i++)
	{
		const char *pBinding = Key_BindingForKey( (ButtonCode_t)i );
		if ( !pBinding || !pBinding[0] )
			continue;

		if ( Q_strstr( pBinding, substring ) )
		{
			if ( !IsJoystickCode( (ButtonCode_t)i ) )
			{
				ConMsg( "\"%s\" = \"%s\"\n",
					g_pInputSystem->ButtonCodeToString( (ButtonCode_t)i ), pBinding );
			}
			else
			{
				int nSlot = GetJoystickForCode( (ButtonCode_t)i );
				ConMsg( "[%d] \"%s\" = \"%s\"\n",
					nSlot, g_pInputSystem->ButtonCodeToString( (ButtonCode_t)i ), pBinding );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Initialization, shutdown 
//-----------------------------------------------------------------------------
void Key_Init (void)
{
	ReadCheatCommandsFromFile( "scripts/cheatcodes.txt" );
	ReadCheatCommandsFromFile( "scripts/mod_cheatcodes.txt" );
}

void Key_Shutdown( void )
{
	for ( int i = 0; i < ARRAYSIZE( s_KeyContext.m_pKeyInfo ); ++i )
	{
		delete[] s_KeyContext.m_pKeyInfo[ i ].m_pKeyBinding;
		s_KeyContext.m_pKeyInfo[ i ].m_pKeyBinding = NULL;
	}

	ClearCheatCommands();
}


//-----------------------------------------------------------------------------
// Purpose: Starts trap mode (used for keybinding UI)
//-----------------------------------------------------------------------------
void Key_StartTrapMode( void )
{
	if ( s_KeyContext.m_bTrapMode )
		return;

	Assert( !s_KeyContext.m_bDoneTrapping && s_KeyContext.m_nTrapKeyUp == BUTTON_CODE_INVALID );

	s_KeyContext.m_bDoneTrapping = false;
	s_KeyContext.m_bTrapMode = true;
	s_KeyContext.m_nTrapKeyUp = BUTTON_CODE_INVALID;
}


//-----------------------------------------------------------------------------
// We're done trapping once the first key is hit
//-----------------------------------------------------------------------------
bool Key_CheckDoneTrapping( ButtonCode_t& code )
{
	if ( s_KeyContext.m_bTrapMode )
		return false;

	if ( !s_KeyContext.m_bDoneTrapping )
		return false;

	code = s_KeyContext.m_nTrapKey;
	s_KeyContext.m_nTrapKey = BUTTON_CODE_INVALID;

	// Reset since we retrieved the results
	s_KeyContext.m_bDoneTrapping = false;
	return true;
}


//-----------------------------------------------------------------------------
// Filter out trapped keys
//-----------------------------------------------------------------------------
static bool FilterTrappedKey( ButtonCode_t code, bool bDown )
{
	// After we've trapped a key, we want to capture the button up message for that key
	if ( s_KeyContext.m_nTrapKeyUp == code && !bDown )
	{
		s_KeyContext.m_nTrapKeyUp = BUTTON_CODE_INVALID;
		return true;
	}

	// Only key down events are trapped
	if ( s_KeyContext.m_bTrapMode && bDown )
	{
		s_KeyContext.m_nTrapKey		= code;
		s_KeyContext.m_bTrapMode     = false;
		s_KeyContext.m_bDoneTrapping = true;
		s_KeyContext.m_nTrapKeyUp	= code;
		return true;
	}

	return false;
}


#ifndef DEDICATED
//-----------------------------------------------------------------------------
// Lets tools have a whack at key events
//-----------------------------------------------------------------------------
static bool HandleToolKey( const InputEvent_t &event )
{
	// Tools are not given typed messages
	if ( event.m_nType == IE_KeyTyped || event.m_nType == IE_KeyCodeTyped )
		return false;

	IToolSystem *toolsys = toolframework->GetTopmostTool();
	return toolsys && toolsys->TrapKey( (ButtonCode_t)event.m_nData, ( event.m_nType != IE_ButtonReleased ) );
}


//-----------------------------------------------------------------------------
// Lets vgui have a whack at key events
//-----------------------------------------------------------------------------
static bool HandleVGuiKey( const InputEvent_t &event )
{
	bool bDown = ( event.m_nType == IE_ButtonPressed ) || ( event.m_nType == IE_ButtonDoubleClicked );
	if ( bDown && IsGameConsole() )
	{
		ButtonCode_t code = (ButtonCode_t)event.m_nData;
		LogKeyPress( code );
		CheckCheatCodes();
	}

	return EngineVGui()->Key_Event( event );
}


#if defined( INCLUDE_SCALEFORM )
//-----------------------------------------------------------------------------
// Lets scaleform have a whack at key events
//-----------------------------------------------------------------------------
static bool HandleScaleformKey( const InputEvent_t &event )
{
	ButtonCode_t code = ( ButtonCode_t ) event.m_nData;

	bool bDown = ( event.m_nType == IE_ButtonPressed ) || ( event.m_nType == IE_ButtonDoubleClicked );
	if ( bDown )
	{
		if ( IsX360() )
		{
			LogKeyPress( code );
			CheckCheatCodes();
		}


	#if defined ( CSTRIKE15 )

		if ( !g_ClientDLL->IsLoadingScreenRaised() && !g_ClientDLL->IsChatRaised() && !g_ClientDLL->IsBindMenuRaised() && !g_ClientDLL->IsRadioPanelRaised() && !g_ClientDLL->IsTeamMenuRaised() )
	#endif
		{

			const char * szBinding = s_KeyContext.m_pKeyInfo[ code ].m_pKeyBinding;
	
			if ( szBinding && szBinding[0] )
			{
				// Add entries to this list if you want to PREVENT scaleform from filtering them
				static const char *szNoFilterList[] = 
				{
					"screenshot",
				};

				static const int kNumNoFilterEntries = sizeof( szNoFilterList ) / sizeof( szNoFilterList[0] );

				for ( int idx=0; idx < kNumNoFilterEntries; ++idx )
				{
					if ( StringHasPrefix( szBinding, szNoFilterList[idx] ) )
					{
						return false;
					}
				}

				// Only filter these binds when the GameUI is active
				static const char *szNoFilterListGameUI[] = 
				{
					"messagemode",
					"messagemode2",
					"+showscores",
					"togglescores",
					"+voicerecord",
				};

				static const int kNumNoFilterEntriesGameUI = sizeof( szNoFilterListGameUI ) / sizeof( szNoFilterListGameUI[0] );

				if ( !EngineVGui()->IsGameUIVisible() )
				{
					for ( int idx=0; idx < kNumNoFilterEntriesGameUI; ++idx )
					{
						if ( StringHasPrefix( szBinding, szNoFilterListGameUI[idx] ) )
						{
							return false;
						}
					}
				}
			}
		}
	}

	return g_pScaleformUI->HandleInputEvent( event );
}
#endif

//-----------------------------------------------------------------------------
// Lets the client have a whack at key events
//-----------------------------------------------------------------------------
static bool HandleClientKey( const InputEvent_t &event )
{
	// KeyTyped events are not handled by the client
	if ( ( event.m_nType == IE_KeyCodeTyped ) || ( event.m_nType == IE_KeyTyped ) )
		return false;

	bool bDown = event.m_nType != IE_ButtonReleased;
	ButtonCode_t code = (ButtonCode_t)event.m_nData;

	if ( g_ClientDLL && g_ClientDLL->IN_KeyEvent( bDown ? 1 : 0, code, s_KeyContext.m_pKeyInfo[ code ].m_pKeyBinding ) == 0 )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
// Lets the new Game UI system have a whack at key events
//-----------------------------------------------------------------------------
static bool HandleGameUIKey( const InputEvent_t &event )
{
	if ( g_ClientDLL && g_ClientDLL->HandleGameUIEvent( event ) )
		return true;

	return false;
}
#endif


//-----------------------------------------------------------------------------
// Lets the engine have a whack at key events
//-----------------------------------------------------------------------------
static bool HandleEngineKey( const InputEvent_t &event )
{
	// KeyTyped events are not handled by the client
	if ( ( event.m_nType == IE_KeyCodeTyped ) || ( event.m_nType == IE_KeyTyped ) )
		return false;

	bool bDown = event.m_nType != IE_ButtonReleased;
	ButtonCode_t code = (ButtonCode_t)event.m_nData;

	// Warn about unbound keys 
	if ( IsPC() && bDown )
	{
		if ( IsJoystickCode( code ) &&
			!IsJoystickAxisCode( code ) && 
			!s_KeyContext.m_pKeyInfo[code].m_pKeyBinding )
		{
			ConDMsg( "[joy %d]%s is unbound.\n", GetJoystickForCode( code ), g_pInputSystem->ButtonCodeToString( code ) );
		}
	}

	// key up events only generate commands if the game key binding is
	// a button command (leading + sign).  These will occur even in console mode,
	// to keep the character from continuing an action started before a console
	// switch.  Button commands include the kenum as a parameter, so multiple
	// downs can be matched with ups
	char *kb = s_KeyContext.m_pKeyInfo[ code ].m_pKeyBinding;
	if ( !kb || !kb[0] )
		return false;

#if !defined( DEDICATED )
	// Prevent keybindings from doing funky stuff unless in game (except toggleconsole)
	if ( Q_stricmp( kb, "toggleconsole" ) )
	{
		extern IVEngineClient *engineClient;
		if ( engineClient && !engineClient->IsConnected() )
			return false; // prevent the keybinding from being processed without a game
	}
#endif

	char cmd[1024];
	if ( !bDown )
	{
		if ( kb[0] == '+' )
		{
			Q_snprintf( cmd, sizeof( cmd ), "-%s %i\n", kb+1, code );
			Cbuf_AddText( Cbuf_GetCurrentPlayer(), cmd, kCommandSrcUserInput );
			return true;
		}
		return false;
	}


	// Send to the interpreter
	if (kb[0] == '+')
	{
		// button commands add keynum as a parm
		Q_snprintf( cmd, sizeof( cmd ), "%s %i\n", kb, code );
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), cmd, kCommandSrcUserInput );
		return true;
	}

	// Swallow console toggle if any modifier keys are down if it's bound to toggleconsole (the default)
	if ( !Q_stricmp( kb, "toggleconsole" ) )
	{
		if ( s_KeyContext.m_pKeyInfo[KEY_LALT].m_bKeyDown || s_KeyContext.m_pKeyInfo[KEY_LSHIFT].m_bKeyDown || s_KeyContext.m_pKeyInfo[KEY_LCONTROL].m_bKeyDown ||
			s_KeyContext.m_pKeyInfo[KEY_RALT].m_bKeyDown || s_KeyContext.m_pKeyInfo[KEY_RSHIFT].m_bKeyDown || s_KeyContext.m_pKeyInfo[KEY_RCONTROL].m_bKeyDown )
			return false;
	}

	Cbuf_AddText( Cbuf_GetCurrentPlayer(), kb, kCommandSrcUserInput );
	return true;
}



//-----------------------------------------------------------------------------
// Helper function to make sure key down/key up events go to the right places
//-----------------------------------------------------------------------------
typedef bool (*FilterKeyFunc_t)( const InputEvent_t &event );

static bool FilterKey( const InputEvent_t &event, KeyUpTarget_t target, FilterKeyFunc_t func )
{
	// Don't pass the key up or keytyped messages to this target if some other system wants it
	ButtonCode_t code = (ButtonCode_t)event.m_nData;
	bool bFilterableEvent = ( event.m_nType == IE_ButtonReleased ) || 
		( event.m_nType == IE_KeyTyped ) || ( event.m_nType == IE_KeyCodeTyped );
	if ( bFilterableEvent && !ShouldPassKeyUpToTarget( code, target ) )
		return false;

	// Try to process the input message
	bool bFiltered = func( event );

	// If we filtered it, then we need to get the key up event
	if ( ( event.m_nType == IE_ButtonPressed ) || ( event.m_nType == IE_ButtonDoubleClicked ) )
	{
		if ( bFiltered )
		{
			Assert( s_KeyContext.m_pKeyInfo[code].m_nKeyUpTarget == KEY_UP_ANYTARGET );
			s_KeyContext.m_pKeyInfo[code].m_nKeyUpTarget = target;
		}
	}
	else if ( event.m_nType == IE_ButtonReleased ) // Up case
	{
		if ( s_KeyContext.m_pKeyInfo[code].m_nKeyUpTarget == target )
		{
			s_KeyContext.m_pKeyInfo[code].m_nKeyUpTarget = KEY_UP_ANYTARGET;
			bFiltered = true;
		}
		else
		{
			// NOTE: It is illegal to trap up key events. The system will do it for us
			Assert( !bFiltered );
		}
	}

	// We do nothing special for the KeyTyped/KeyCodeTyped cases; they do not change state

	return bFiltered;
}

inline bool IsESC( const InputEvent_t &event )
{
	switch ( event.m_nType )
	{
	case IE_ButtonPressed:
	case IE_ButtonReleased:
	case IE_ButtonDoubleClicked:
	case IE_KeyCodeTyped:
		{
			return ( (ButtonCode_t)event.m_nData == KEY_ESCAPE );
		}
		break;

	case IE_KeyTyped:
		{
			// ASCII Escape character
			return ( event.m_nData == 27 );
		}
		break;
	}
	return false;
}

void Key_Event( const InputEvent_t &event )
{
#ifdef DEDICATED
	return;
#else
	ASSERT_NO_REENTRY();

	ButtonCode_t code = (ButtonCode_t)event.m_nData;

	int nSplitScreenPlayerSlot = 0;
	if ( !IsJoystickCode( code ) && !IsSteamControllerCode( code ) )
	{
		if ( splitscreen->IsValidSplitScreenSlot( in_forceuser.GetInt() ) )
		{
			nSplitScreenPlayerSlot = in_forceuser.GetInt();
		}
	}
	else
	{
		nSplitScreenPlayerSlot = GetJoystickForCode( code );
	}

	ACTIVE_SPLITSCREEN_PLAYER_GUARD( nSplitScreenPlayerSlot );

	// Don't handle key ups if the key's already up. 
	// NOTE: This should already be taken care of by the input system
	// NOTE ALSO: KeyTyped and KeyCodeTyped messages are going to come through here also
	// Only the VGui and the new GameUI system should accept them; and only if 
	// they accepted the keydown event
	if ( ( event.m_nType != IE_KeyCodeTyped ) && ( event.m_nType != IE_KeyTyped ) )
	{
		bool bDown = ( event.m_nType == IE_ButtonPressed ) || ( event.m_nType == IE_ButtonDoubleClicked );
		Assert( s_KeyContext.m_pKeyInfo[code].m_bKeyDown != bDown );
		if ( s_KeyContext.m_pKeyInfo[code].m_bKeyDown == bDown )
			return;

		s_KeyContext.m_pKeyInfo[code].m_bKeyDown = bDown;

		// Deal with trapped keys
		if ( FilterTrappedKey( code, bDown ) )
			return;
	}

	// Make sure vgui is initialzied
	if ( !EngineVGui()->IsInitialized() )
		return;

	// Keep vgui's notion of which keys are down up-to-date regardless of filtering
	// Necessary because vgui has multiple input contexts, so vgui can't directly
	// ask the input system for this information.
	// QUESTION: Should GameUI do the same thing?
	EngineVGui()->UpdateButtonState( event );

	if ( IsPC() && EngineVGui()->IsGameUIVisible() && scr_drawloading && IsESC( event ) )
	{
		// prevent ESC key during loading
		return;
	}

	// Let tools have a whack at keys
	if ( FilterKey( event, KEY_UP_TOOLS, HandleToolKey ) )
		return;	

	// If we happen to be checking the MAGICAL ESC key then 
	// let's have the client check first since we do magical things
	// with this key otherwise.
	// [jason] Bypass the ButtonPress event for the BACKQUOTE key ("~") so that HandleGameUIKey can intercept it and bring up the Console Window
	if ( !IsESC( event ) && !(event.m_nType == IE_ButtonPressed && code == KEY_BACKQUOTE) )
	{
		// Let vgui have a whack at keys
		if ( FilterKey( event, KEY_UP_VGUI, HandleVGuiKey ) )
			return;

#if defined( INCLUDE_SCALEFORM )
		// scaleform goes first
		if ( FilterKey( event, KEY_UP_SCALEFORM, HandleScaleformKey ) )
			return;
#endif
	}
#if defined ( CSTRIKE15 )
	else if ( g_ClientDLL->IsChatRaised() || g_ClientDLL->IsBindMenuRaised() )
	{
		if ( FilterKey( event, KEY_UP_SCALEFORM, HandleScaleformKey ) )
			return;
	}

#endif

	// Let the new GameUI system have a whack at keys
	if ( FilterKey( event, KEY_UP_GAMEUI, HandleGameUIKey ) )
		return;

	// Let the client have a whack at keys
	if ( FilterKey( event, KEY_UP_CLIENT, HandleClientKey ) )
		return;

	// Ok the client wants nothing to do with the magical ESC key
	// let's see if VGUI wants to do something with it.
	if ( IsESC( event ) )
	{
#if defined( INCLUDE_SCALEFORM ) 
		bool bAllowScaleformFilter = true;
		static ConVarRef cv_console_window_open( "console_window_open" );
		static ConVarRef cv_server_browser_dialog_open( "server_browser_dialog_open" );
		if ( ( cv_console_window_open.GetBool() ) ||
			 ( cv_server_browser_dialog_open.GetBool() ) ||
			 ( EngineSoundClient() && EngineSoundClient()->IsMoviePlaying() ) )
		{
			// make closing the console window, server browser dialog, or exiting a movie priority
			bAllowScaleformFilter = false;
		}

		if ( bAllowScaleformFilter && FilterKey( event, KEY_UP_SCALEFORM, HandleScaleformKey ) )
			return;

#endif // INCLUDE_SCALEFORM

		// Let vgui have a whack at keys
		if ( FilterKey( event, KEY_UP_VGUI, HandleVGuiKey ) )
			return;
	}

	// Finally, let the engine deal. Here's where keybindings occur.
	FilterKey( event, KEY_UP_ENGINE, HandleEngineKey );
#endif
}


