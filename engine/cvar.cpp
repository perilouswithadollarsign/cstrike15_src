//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include "cvar.h"
#include "gl_cvars.h"

#include "tier1/convar.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "client.h"
#include "server.h"
#include "GameEventManager.h"
#include "netmessages.h"
#include "sv_main.h"
#include "demo.h"
#include <ctype.h>
#include "vstdlib/vstrtools.h"
#ifdef POSIX
#include <wctype.h>
#endif
#ifdef _PS3
#include <ps3/ps3_console.h>
#endif

#ifndef DEDICATED
#include <vgui_controls/Controls.h>
#include <vgui/ILocalize.h>
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Singleton CCvarUtilities
//-----------------------------------------------------------------------------
static CCvarUtilities g_CvarUtilities;
CCvarUtilities *ConVarUtilities = &g_CvarUtilities;


//-----------------------------------------------------------------------------
// Purpose: Update clients/server when FCVAR_REPLICATED etc vars change
//-----------------------------------------------------------------------------
static void ConVarNetworkChangeCallback( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	ConVarRef var( pConVar );
	if ( !pOldValue )
	{
		if ( var.GetFloat() == flOldValue )
			return;
	}
	else
	{
		if ( !Q_strcmp( var.GetString(), pOldValue ) )
			return;
	}

	if ( var.IsFlagSet( FCVAR_USERINFO ) )
	{
#ifndef DEDICATED
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( var.GetSplitScreenPlayerSlot() );

		// Are we not a server, but a client and have a change?
		if ( GetLocalClient().IsConnected() )
		{
			// send changed cvar to server
			CNETMsg_SetConVar_t convar( var.GetBaseName(), var.GetString() );
			GetLocalClient().m_NetChannel->SendNetMsg( convar );
		}
#endif
	} 

	// Log changes to server variables

	// Print to clients
	if ( var.IsFlagSet( FCVAR_NOTIFY ) )
	{
		IGameEvent *event = g_GameEventManager.CreateEvent( "server_cvar" );

		if ( event )
		{
			event->SetString( "cvarname", var.GetName() );

			if ( var.IsFlagSet( FCVAR_PROTECTED ) )
			{
				event->SetString("cvarvalue", "***PROTECTED***" );
			}
			else
			{
				event->SetString("cvarvalue", var.GetString() );
			}

			g_GameEventManager.FireEvent( event );
		}
	}

	// Force changes down to clients (if running server)
	if ( var.IsFlagSet( FCVAR_REPLICATED ) && sv.IsActive() )
	{
		SV_ReplicateConVarChange( static_cast< ConVar* >( pConVar ), var.GetString() );
	}
}


//-----------------------------------------------------------------------------
// Implementation of the ICvarQuery interface
//-----------------------------------------------------------------------------
class CCvarQuery : public CBaseAppSystem< ICvarQuery >
{
public:

	bool m_bCallbackInstalled;


	CCvarQuery( void )
	{
		m_bCallbackInstalled = false;
	}

	virtual bool Connect( CreateInterfaceFn factory )
	{
		ICvar *pCVar = (ICvar*)factory( CVAR_INTERFACE_VERSION, 0 );
		if ( !pCVar )
			return false;

		pCVar->InstallCVarQuery( this );
		return true;
	}

	virtual InitReturnVal_t Init()
	{
		// If the value has changed, notify clients/server based on ConVar flags.
		// NOTE: this will only happen for non-FCVAR_NEVER_AS_STRING vars.
		// Also, this happened in SetDirect for older clients that don't have the
		// callback interface.
		if (! m_bCallbackInstalled )
		{
			m_bCallbackInstalled = true;
			g_pCVar->InstallGlobalChangeCallback( ConVarNetworkChangeCallback );
		}
		return INIT_OK;
	}

	virtual void Shutdown()
	{
		g_pCVar->RemoveGlobalChangeCallback( ConVarNetworkChangeCallback );
		m_bCallbackInstalled = false;
	}

	virtual void *QueryInterface( const char *pInterfaceName )
	{
		if ( !Q_stricmp( pInterfaceName, CVAR_QUERY_INTERFACE_VERSION ) )
			return (ICvarQuery*)this;
		return NULL;

	}

	// Purpose: Returns true if the commands can be aliased to one another
	//  Either game/client .dll shared with engine, 
	//  or game and client dll shared and marked FCVAR_REPLICATED
	virtual bool AreConVarsLinkable( const ConVar *child, const ConVar *parent )
	{
		// Both parent and child must be marked replicated for this to work
		bool repchild = child->IsFlagSet( FCVAR_REPLICATED );
		bool repparent = parent->IsFlagSet( FCVAR_REPLICATED );

		if ( repchild && repparent )
		{
			// Never on protected vars
			if ( child->IsFlagSet( FCVAR_PROTECTED ) || parent->IsFlagSet( FCVAR_PROTECTED ) )
			{
				ConMsg( "FCVAR_REPLICATED can't also be FCVAR_PROTECTED (%s)\n", child->GetName() );
				return false;
			}

			// Only on ConVars
			if ( child->IsCommand() || parent->IsCommand() )
			{
				ConMsg( "FCVAR_REPLICATED not valid on ConCommands (%s)\n", child->GetName() );
				return false;
			}

			// One must be in client .dll and the other in the game .dll, or both in the engine
			if ( child->IsFlagSet( FCVAR_GAMEDLL ) && !parent->IsFlagSet( FCVAR_CLIENTDLL ) )
			{
				ConMsg( "For FCVAR_REPLICATED, ConVar must be defined in client and game .dlls (%s)\n", child->GetName() );
				return false;
			}

			if ( child->IsFlagSet( FCVAR_CLIENTDLL ) && !parent->IsFlagSet( FCVAR_GAMEDLL ) )
			{
				ConMsg( "For FCVAR_REPLICATED, ConVar must be defined in client and game .dlls (%s)\n", child->GetName() );
				return false;
			}

			// Allowable
			return true;
		}

		// Otherwise need both to allow linkage
		if ( repchild || repparent )
		{
			ConMsg( "Both ConVars must be marked FCVAR_REPLICATED for linkage to work (%s)\n", child->GetName() );
			return false;
		}

		if ( parent->IsFlagSet( FCVAR_CLIENTDLL ) )
		{
			ConMsg( "Parent cvar in client.dll not allowed (%s)\n", child->GetName() );
			return false;
		}

		if ( parent->IsFlagSet( FCVAR_GAMEDLL ) )
		{
			ConMsg( "Parent cvar in server.dll not allowed (%s)\n", child->GetName() );
			return false;
		}

		return true;
	}
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CCvarQuery s_CvarQuery;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CCvarQuery, ICvarQuery, CVAR_QUERY_INTERFACE_VERSION, s_CvarQuery );
void InstallConVarHook( void )
{
	s_CvarQuery.Init();
}



//-----------------------------------------------------------------------------
//
// CVar utilities begins here
//  
//-----------------------------------------------------------------------------
static bool IsAllSpaces( const wchar_t *str )
{
	const wchar_t *p = str;
	while ( p && *p )
	{
		if ( !iswspace( *p ) )
			return false;

		++p;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *var - 
//			*value - 
//-----------------------------------------------------------------------------
void CCvarUtilities::SetDirect( ConVar *var, const char *value )
{
	const char *pszValue;
	char szNew[ 1024 ];

	// Bail early if we're trying to set a FCVAR_USERINFO cvar on a dedicated server
	if ( var->IsFlagSet( FCVAR_USERINFO ) )
	{
		if ( sv.IsDedicated() )
		{
			return;
		}
	} 

	pszValue = value;
	// This cvar's string must only contain printable characters.
	// Strip out any other crap.
	// We'll fill in "empty" if nothing is left
	if ( var->IsFlagSet( FCVAR_PRINTABLEONLY ) )
	{
		wchar_t unicode[ 512 ];
#ifndef DEDICATED
		if ( sv.IsDedicated() )
		{
			// Dedicated servers don't have g_pVGuiLocalize, so fall back
			V_UTF8ToUnicode( pszValue, unicode, sizeof( unicode ) );
		}
		else
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( pszValue, unicode, sizeof( unicode ) );
		}
#else
		V_UTF8ToUnicode( pszValue, unicode, sizeof( unicode ) );
#endif
		wchar_t newUnicode[ 512 ];

		const wchar_t *pS;
		wchar_t *pD;

		// Clear out new string
		newUnicode[0] = L'\0';

		pS = unicode;
		pD = newUnicode;

		// Step through the string, only copying back in characters that are printable
		while ( *pS )
		{
			if ( iswcntrl( *pS ) || *pS == '~' )
			{
				pS++;
				continue;
			}

			*pD++ = *pS++;
		}

		// Terminate the new string
		*pD = L'\0';

		// If it's empty or all spaces, then insert a marker string
		if ( !wcslen( newUnicode ) || IsAllSpaces( newUnicode ) )
		{
			wcsncpy( newUnicode, L"#empty", ( sizeof( newUnicode ) / sizeof( wchar_t ) ) - 1 );
			newUnicode[ ( sizeof( newUnicode ) / sizeof( wchar_t ) ) - 1 ] = L'\0';
		}

#ifndef DEDICATED
		if ( sv.IsDedicated() )
		{
			V_UnicodeToUTF8( newUnicode, szNew, sizeof( szNew ) );
		}
		else
		{
			g_pVGuiLocalize->ConvertUnicodeToANSI( newUnicode, szNew, sizeof( szNew ) );
		}
#else
		V_UnicodeToUTF8( newUnicode, szNew, sizeof( szNew ) );
#endif
		// Point the value here.
		pszValue = szNew;
	}

	if ( var->IsFlagSet( FCVAR_NEVER_AS_STRING ) )
	{
		var->SetValue( (float)atof( pszValue ) );
	}
	else
	{
		var->SetValue( pszValue );
	}
}



//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------

// If you are changing this, please take a look at IsValidToggleCommand()
bool CCvarUtilities::IsCommand( const CCommand &args, const int iSplitscreenSlot /*= -1*/ )
{
	int c = args.ArgC();
	if ( c == 0 )
		return false;

	ConVar			*v;

	// check variables
	v = g_pCVar->FindVar( args[0] );
	if ( !v )
		return false;

	// adjust for split screen convars for slots 2, 3, 4
	if ( iSplitscreenSlot > 0 && v->IsFlagSet( FCVAR_SS ) )
	{
		char buf[512];
		Q_snprintf( buf, sizeof(buf), "%s%d", args[0], iSplitscreenSlot+1 );
		v = g_pCVar->FindVar( buf );
	}

	if ( !v )
		return false;

	// NOTE: Not checking for 'HIDDEN' here so we can actually set hidden convars
	if ( v->IsFlagSet(FCVAR_DEVELOPMENTONLY) )
		return false;

	// perform a variable print or set
	if ( c == 1 )
	{
		ConVar_PrintDescription( v );
		return true;
	}

	if ( v->IsFlagSet( FCVAR_SPONLY ) )
	{
#ifndef DEDICATED
		// Connected to server?
		if ( GetBaseLocalClient().IsConnected() )
		{
			// Is it not a single player game?
			if ( GetBaseLocalClient().m_nMaxClients > 1 )
			{
				ConMsg( "Can't set %s in multiplayer\n", v->GetName() );
				return true;
			}
		}
#endif
	}

	if ( v->IsFlagSet( FCVAR_NOT_CONNECTED ) )
	{
#ifndef DEDICATED
		// Connected to server?
		if ( GetBaseLocalClient().IsConnected() )
		{
			extern IBaseClientDLL *g_ClientDLL;
			if ( v->IsFlagSet( FCVAR_USERINFO ) && g_ClientDLL && g_ClientDLL->IsConnectedUserInfoChangeAllowed( v ) )
			{
				// Client.dll is allowing the convar change
			}
			else
			{
				ConMsg( "Can't change %s when playing, disconnect from the server or switch team to spectators\n", v->GetName() );
				return true;
			}
		}
#endif
	}

	// Allow cheat commands in singleplayer, debug, or multiplayer with sv_cheats on
	if ( v->IsFlagSet( FCVAR_CHEAT ) )
	{
		if ( !Host_IsSinglePlayerGame() && !CanCheat() 
#if !defined(DEDICATED)
			&& !GetBaseLocalClient().ishltv
#if defined( REPLAY_ENABLED )
			&& !GetBaseLocalClient().isreplay
#endif
			&& !demoplayer->IsPlayingBack() 
#endif
			)
		{
			ConMsg( "Can't use cheat cvar %s in multiplayer, unless the server has sv_cheats set to 1.\n", v->GetName() );
			return true;
		}
	}

	// Text invoking the command was typed into the console, decide what to do with it
	//  if this is a replicated ConVar, except don't worry about restrictions if playing a .dem file
	if ( v->IsFlagSet( FCVAR_REPLICATED ) 
#if !defined(DEDICATED)
		&& !demoplayer->IsPlayingBack()
#endif
		)
	{
#ifndef DEDICATED
		// If not running a server but possibly connected as a client, then
		//  if the message came from console, don't process the command
		if ( !sv.IsActive()
			 && !sv.IsLoading()
			 && GetBaseLocalClient().IsConnected()
		   )
		{
			ConMsg( "Can't change replicated ConVar %s from console of client, only server operator can change its value\n", v->GetName() );
			return true;
		}
#endif
	}

	// Note that we don't want the tokenized list, send down the entire string
	// except for surrounding quotes
	char remaining[1024];
	const char *pArgS = args.ArgS();
	int nLen = Q_strlen( pArgS );
	bool bIsQuoted = pArgS[0] == '\"';
	if ( !bIsQuoted )
	{
		Q_strncpy( remaining, args.ArgS(), sizeof(remaining) );
	}
	else
	{
		--nLen;
		Q_strncpy( remaining, &pArgS[1], sizeof(remaining) );
	}

	// Now strip off any trailing spaces
	char *p = remaining + nLen - 1;
	while ( p >= remaining )
	{
		if ( *p > ' ' )
			break;

		*p-- = 0;
	}

	// Strip off ending quote
	if ( bIsQuoted && p >= remaining )
	{
		if ( *p == '\"' )
		{
			*p = 0;
		}
	}

	SetDirect( v, remaining );
	return true;
}

// This is a band-aid copied directly from IsCommand().  
bool CCvarUtilities::IsValidToggleCommand( const char *cmd )
{
	ConVar			*v;

	// check variables
	v = g_pCVar->FindVar ( cmd );
	if (!v)
	{
		ConMsg( "%s is not a valid cvar\n", cmd );
		return false;
	}

	if ( v->IsFlagSet(FCVAR_DEVELOPMENTONLY) || v->IsFlagSet(FCVAR_HIDDEN) )
		return false;

	if ( v->IsFlagSet( FCVAR_SPONLY ) )
	{
#ifndef DEDICATED
		// Connected to server?
		if ( GetBaseLocalClient().IsConnected() )
		{
			// Is it not a single player game?
			if ( GetBaseLocalClient().m_nMaxClients > 1 )
			{
				ConMsg( "Can't set %s in multiplayer\n", v->GetName() );
				return false;
			}
		}
#endif
	}

	if ( v->IsFlagSet( FCVAR_NOT_CONNECTED ) )
	{
#ifndef DEDICATED
		// Connected to server?
		if ( GetBaseLocalClient().IsConnected() )
		{
			extern IBaseClientDLL *g_ClientDLL;
			if ( v->IsFlagSet( FCVAR_USERINFO ) && g_ClientDLL && g_ClientDLL->IsConnectedUserInfoChangeAllowed( v ) )
			{
				// Client.dll is allowing the convar change
			}
			else
			{
				ConMsg( "Can't change %s when playing, disconnect from the server or switch team to spectators\n", v->GetName() );
				return false;
			}
		}
#endif
	}

	// Allow cheat commands in singleplayer, debug, or multiplayer with sv_cheats on
	if ( v->IsFlagSet( FCVAR_CHEAT ) )
	{
		if ( !Host_IsSinglePlayerGame() && !CanCheat() 
#if !defined(DEDICATED)
			&& !demoplayer->IsPlayingBack() 
#endif
			)
		{
			ConMsg( "Can't use cheat cvar %s in multiplayer, unless the server has sv_cheats set to 1.\n", v->GetName() );
			return false;
		}
	}

	// Text invoking the command was typed into the console, decide what to do with it
	//  if this is a replicated ConVar, except don't worry about restrictions if playing a .dem file
	if ( v->IsFlagSet( FCVAR_REPLICATED ) 
#if !defined(DEDICATED)
		&& !demoplayer->IsPlayingBack()
#endif
		)
	{
#ifndef DEDICATED
		// If not running a server but possibly connected as a client, then
		//  if the message came from console, don't process the command
		if ( !sv.IsActive()
			 && !sv.IsLoading()
			 && GetBaseLocalClient().IsConnected() 
		   )
		{
			ConMsg( "Can't change replicated ConVar %s from console of client, only server operator can change its value\n", v->GetName() );
			return false;
		}
#endif
	}

	return true;
}

void CCvarUtilities::ResetConVarsToDefaultValues( const char *pMatchStr )
{
	ICvar::Iterator iter( g_pCVar );
	for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
	{
		ConCommandBase *var = iter.Get();
		if ( var->IsCommand() )
			continue;
		
		ConVar *cv = (ConVar *)var;
		
		if ( ( ! pMatchStr ) || 							// null pattern match?
			 ( memcmp( pMatchStr, cv->GetName(), strlen( pMatchStr ) ) == 0 ) // first chars match
			)
		{
			cv->Revert();
		}
		
	}
}


static bool CVarSortFunc( ConVar * const &lhs, ConVar * const &rhs )
{
	return ( CaselessStringLessThan( lhs->GetName(), rhs->GetName() ) );
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *f - 
//-----------------------------------------------------------------------------
void CCvarUtilities::WriteVariables( CUtlBuffer *buff, const int iSplitscreenSlot /*= -1*/, bool bSlotRequired /* = false */, void *pConvarsListVoid /*= NULL*/ )
{
	CUtlRBTree< ConVar *, int > sorted( 0, 0, CVarSortFunc );
	CUtlVector< ConVar * > *pConvarsList = (CUtlVector< ConVar * > *)pConvarsListVoid;

	ICvar::Iterator iter( g_pCVar );
	for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
	{
		ConCommandBase *var = iter.Get();
		if ( var->IsCommand() )
			continue;

		ConVar *cv = (ConVar *)var;

		bool archive = cv->IsFlagSet( IsGameConsole() ? FCVAR_ARCHIVE_GAMECONSOLE : FCVAR_ARCHIVE );
		if ( archive )
		{
			if ( iSplitscreenSlot >= 0 )
			{
				bool bSlotSpecificConvar = false;

				if ( cv->IsFlagSet( FCVAR_SS ) )
				{
					// only valid for the 0'th player
					if ( iSplitscreenSlot != 0 )
					{
						continue;
					}

					bSlotSpecificConvar = true;
				}

				if ( cv->IsFlagSet( FCVAR_SS_ADDED ) )
				{
					// which added player is this relevant to
					CSplitScreenAddedConVar *pCheck = dynamic_cast< CSplitScreenAddedConVar * >( cv );
					if ( pCheck && pCheck->GetSplitScreenPlayerSlot() != iSplitscreenSlot )
					{
						continue;
					}

					bSlotSpecificConvar = true;
				}

				if ( bSlotRequired != bSlotSpecificConvar )
				{
					continue;
				}
			}

			sorted.Insert( cv );
		}
	}

	for ( int i = sorted.FirstInorder(); i != sorted.InvalidIndex(); i = sorted.NextInorder( i ) )
	{
		ConVar *var = sorted[ i ];

		// If we are saving per-controller, we always want to write the base name ( joy_inverty as opposed to joy_inverty2 )
		const char *name = ( iSplitscreenSlot >= 0 ) ? var->GetBaseName() : var->GetName();
		DevMsg( 2, "%s \"%s\"\n", name, var->GetString() );

		if ( buff )
			buff->Printf( "%s \"%s\"\n", name, var->GetString() );

		if ( pConvarsList )
			pConvarsList->AddToTail( var );
	}
}

static void Cmd_SetPlayer( int slot, const CCommand &args )
{
	if ( slot >= host_state.max_splitscreen_players )
	{
		DevMsg( 1, "ignore:  %d '%s'\n", slot, args.ArgS() );
		return;
	}

	// Strip the cmdN and pass the rest of the command to the appropriate slot
	Cbuf_AddText( (ECommandTarget_t)slot, args.ArgS() );
}

CON_COMMAND( cmd1, "sets userinfo string for split screen player in slot 1" )
{
	Cmd_SetPlayer( 0, args );
}
CON_COMMAND( cmd2, "sets userinfo string for split screen player in slot 2" )
{
	Cmd_SetPlayer( 1, args );
}
CON_COMMAND( cmd3, "sets userinfo string for split screen player in slot 3" )
{
	Cmd_SetPlayer( 2, args );
}
CON_COMMAND( cmd4, "sets userinfo string for split screen player in slot 4" )
{
	Cmd_SetPlayer( 3, args );
}

static char *StripTabsAndReturns( const char *inbuffer, char *outbuffer, int outbufferSize )
{
	char *out = outbuffer;
	const char *i = inbuffer;
	char *o = out;

	out[ 0 ] = 0;

	while ( *i && o - out < outbufferSize - 1 )
	{
		if ( *i == '\n' ||
			*i == '\r' ||
			*i == '\t' )
		{
			*o++ = ' ';
			i++;
			continue;
		}
		if ( *i == '\"' )
		{
			*o++ = '\'';
			i++;
			continue;
		}

		*o++ = *i++;
	}

	*o = '\0';

	return out;
}

static char *StripQuotes( const char *inbuffer, char *outbuffer, int outbufferSize )
{	
	char *out = outbuffer;
	const char *i = inbuffer;
	char *o = out;

	out[ 0 ] = 0;

	while ( *i && o - out < outbufferSize - 1 )
	{
		if ( *i == '\"' )
		{
			*o++ = '\'';
			i++;
			continue;
		}

		*o++ = *i++;
	}

	*o = '\0';

	return out;
}

struct ConVarFlags_t
{
	int			bit;
	const char	*desc;
	const char	*shortdesc;
};

#define CONVARFLAG( x, y )	{ FCVAR_##x, #x, #y }

static ConVarFlags_t g_ConVarFlags[]=
{
	//	CONVARFLAG( UNREGISTERED, "u" ),
	CONVARFLAG( ARCHIVE, "a" ),
	CONVARFLAG( SPONLY, "sp" ),
	CONVARFLAG( GAMEDLL, "sv" ),
	CONVARFLAG( CHEAT, "cheat" ),
	CONVARFLAG( USERINFO, "user" ),
	CONVARFLAG( NOTIFY, "nf" ),
	CONVARFLAG( PROTECTED, "prot" ),
	CONVARFLAG( PRINTABLEONLY, "print" ),
	CONVARFLAG( UNLOGGED, "log" ),
	CONVARFLAG( NEVER_AS_STRING, "numeric" ),
	CONVARFLAG( REPLICATED, "rep" ),
	CONVARFLAG( DEMO, "demo" ),
	CONVARFLAG( DONTRECORD, "norecord" ),
	CONVARFLAG( SERVER_CAN_EXECUTE, "server_can_execute" ),
	CONVARFLAG( CLIENTCMD_CAN_EXECUTE, "clientcmd_can_execute" ),
	CONVARFLAG( CLIENTDLL, "cl" ),
	CONVARFLAG( SS, "ss" ),
	CONVARFLAG( SS_ADDED, "ss_added" ),
	CONVARFLAG( DEVELOPMENTONLY, "dev_only" ),
};

static void PrintListHeader( FileHandle_t& f )
{
	char csvflagstr[ 1024 ];

	csvflagstr[ 0 ] = 0;

	int c = ARRAYSIZE( g_ConVarFlags );
	for ( int i = 0 ; i < c; ++i )
	{
		char csvf[ 64 ];

		ConVarFlags_t & entry = g_ConVarFlags[ i ];
		Q_snprintf( csvf, sizeof( csvf ), "\"%s\",", entry.desc );
		Q_strncat( csvflagstr, csvf, sizeof( csvflagstr ), COPY_ALL_CHARACTERS );
	}

	g_pFileSystem->FPrintf( f,"\"%s\",\"%s\",%s,\"%s\"\n", "Name", "Value", csvflagstr, "Help Text" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *var - 
//			*f - 
//-----------------------------------------------------------------------------
static void PrintCvar( const ConVar *var, bool logging, FileHandle_t& f )
{
	char flagstr[ 128 ];
	char csvflagstr[ 1024 ];

	flagstr[ 0 ] = 0;
	csvflagstr[ 0 ] = 0;

	int c = ARRAYSIZE( g_ConVarFlags );
	for ( int i = 0 ; i < c; ++i )
	{
		char f[ 32 ];
		char csvf[ 64 ];

		ConVarFlags_t & entry = g_ConVarFlags[ i ];
		if ( var->IsFlagSet( entry.bit ) )
		{
			Q_snprintf( f, sizeof( f ), ", %s", entry.shortdesc );

			Q_strncat( flagstr, f, sizeof( flagstr ), COPY_ALL_CHARACTERS );

			Q_snprintf( csvf, sizeof( csvf ), "\"%s\",", entry.desc );
		}
		else
		{
			Q_snprintf( csvf, sizeof( csvf ), "," );
		}

		Q_strncat( csvflagstr, csvf, sizeof( csvflagstr ), COPY_ALL_CHARACTERS );
	}


	char valstr[ 32 ];
	char tempbuff[128];

	// Clean up integers
	if ( var->GetInt() == (int)var->GetFloat() )   
	{
		Q_snprintf(valstr, sizeof( valstr ), "%-8i", var->GetInt() );
	}
	else
	{
		Q_snprintf(valstr, sizeof( valstr ), "%-8.3f", var->GetFloat() );
	}

	// Print to console
	ConMsg( "%-40s : %-8s : %-16s : %s\n", var->GetName(), valstr, flagstr, StripTabsAndReturns( var->GetHelpText(), tempbuff, sizeof(tempbuff) ) );

	if ( logging )
	{
		g_pFileSystem->FPrintf( f,"\"%s\",\"%s\",%s,\"%s\"\n", var->GetName(), valstr, csvflagstr, StripQuotes( var->GetHelpText(), tempbuff, sizeof(tempbuff) ) );
	}
}

static void PrintCommand( const ConCommand *cmd, bool logging, FileHandle_t& f )
{
	// Print to console
	char tempbuff[128];
	ConMsg ("%-40s : %-8s : %-16s : %s\n",cmd->GetName(), "cmd", "", StripTabsAndReturns( cmd->GetHelpText(), tempbuff, sizeof(tempbuff) ) );
	if ( logging )
	{
		char emptyflags[ 256 ];

		emptyflags[ 0 ] = 0;

		int c = ARRAYSIZE( g_ConVarFlags );
		for ( int i = 0; i < c; ++i )
		{
			char csvf[ 64 ];
			Q_snprintf( csvf, sizeof( csvf ), "," );
			Q_strncat( emptyflags, csvf, sizeof( emptyflags ), COPY_ALL_CHARACTERS );
		}
		// Names staring with +/- need to be wrapped in single quotes
		char name[ 256 ];
		Q_snprintf( name, sizeof( name ), "%s", cmd->GetName() );
		if ( name[ 0 ] == '+' || name[ 0 ] == '-' )
		{
			Q_snprintf( name, sizeof( name ), "'%s'", cmd->GetName() );
		}
		char tempbuff[128];
		g_pFileSystem->FPrintf( f, "\"%s\",\"%s\",%s,\"%s\"\n", name, "cmd", emptyflags, StripQuotes( cmd->GetHelpText(), tempbuff, sizeof(tempbuff) ) );
	}
}

static bool ConCommandBaseLessFunc( const ConCommandBase * const &lhs, const ConCommandBase * const &rhs )
{ 
	const char *left = lhs->GetName();
	const char *right = rhs->GetName();

	if ( *left == '-' || *left == '+' )
		left++;
	if ( *right == '-' || *right == '+' )
		right++;

	return ( Q_stricmp( left, right ) < 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : void CCvar::CvarList_f
//-----------------------------------------------------------------------------
void CCvarUtilities::CvarList( const CCommand &args )
{
	const ConCommandBase	*var;	// Temporary Pointer to cvars
	int iArgs;						// Argument count
	const char *partial = NULL;		// Partial cvar to search for...
									// E.eg
	int ipLen = 0;					// Length of the partial cvar

	FileHandle_t f = FILESYSTEM_INVALID_HANDLE;         // FilePointer for logging
	bool bLogging = false;
	// Are we logging?
	iArgs = args.ArgC();		// Get count

	// Print usage?
	if ( iArgs == 2 && !Q_strcasecmp( args[1],"?" ) )
	{
		ConMsg( "cvarlist:  [log logfile] [ partial ]\n" );
		return;         
	}

	if ( !Q_strcasecmp( args[1],"log" ) && iArgs >= 3 )
	{
		char fn[256];
		Q_snprintf( fn, sizeof( fn ), "%s", args[2] );
		f = g_pFileSystem->Open( fn,"wb" );
		if ( f )
		{
			bLogging = true;
		}
		else
		{
			ConMsg( "Couldn't open '%s' for writing!\n", fn );
			return;
		}

		if ( iArgs == 4 )
		{
			partial = args[ 3 ];
			ipLen = Q_strlen( partial );
		}
	}
	else
	{
		partial = args[ 1 ];   
		ipLen = Q_strlen( partial );
	}

	// Banner
	ConMsg( "cvar list\n--------------\n" );

	CUtlRBTree< const ConCommandBase * > sorted( 0, 0, ConCommandBaseLessFunc );

	// Loop through cvars...
	ICvar::Iterator iter( g_pCVar );
	for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
	{
		ConCommandBase *var = iter.Get();
		bool print = false;

		if ( var->IsFlagSet(FCVAR_DEVELOPMENTONLY) || var->IsFlagSet(FCVAR_HIDDEN) )
			continue;

		if (partial)  // Partial string searching?
		{
			if ( !Q_strncasecmp( var->GetName(), partial, ipLen ) )
			{
				print = true;
			}
		}
		else		  
		{
			print = true;
		}

		if ( !print )
			continue;

		sorted.Insert( var );
	}

	if ( bLogging )
	{
		PrintListHeader( f );
	}
	for ( int i = sorted.FirstInorder(); i != sorted.InvalidIndex(); i = sorted.NextInorder( i ) )
	{
		var = sorted[ i ];
		if ( var->IsCommand() )
		{
			PrintCommand( (ConCommand *)var, bLogging, f );
		}
		else
		{
			PrintCvar( (ConVar *)var, bLogging, f );
		}
	}


	// Show total and syntax help...
	if ( partial && partial[0] )
	{
		ConMsg("--------------\n%3i convars/concommands for [%s]\n", sorted.Count(), partial );
	}
	else
	{
		ConMsg("--------------\n%3i total convars/concommands\n", sorted.Count() );
	}

	if ( bLogging )
	{
		g_pFileSystem->Close( f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Removes the FCVAR_DEVELOPMENTONLY flag from all cvars, making them accessible
//-----------------------------------------------------------------------------
void CCvarUtilities::EnableDevCvars()
{
	// Loop through cvars...
	ICvar::Iterator iter( g_pCVar );
	for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
	{
		ConCommandBase *var = iter.Get();
		// remove flag from all cvars
		var->RemoveFlags( FCVAR_DEVELOPMENTONLY );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
// Output : int
//-----------------------------------------------------------------------------
int CCvarUtilities::CountVariablesWithFlags( int flags )
{
	int c = 0;
	ICvar::Iterator iter( g_pCVar );
	for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
	{
		ConCommandBase *var = iter.Get();
		if ( var->IsCommand() )
			continue;

		if ( var->IsFlagSet( flags ) )
		{
			++c;
		}
	}

	return c;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCvarUtilities::CvarHelp( const CCommand &args )
{
	const char *search;
	const ConCommandBase *var;

	if ( args.ArgC() != 2 )
	{
		ConMsg( "Usage:  help <cvarname>\n" );
		return;
	}

	// Get name of var to find
	search = args[1];

	// Search for it
	var = g_pCVar->FindCommandBase( search );
	if ( !var )
	{
		ConMsg( "help:  no cvar or command named %s\n", search );
		return;
	}

	// Show info
	ConVar_PrintDescription( var );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CCvarUtilities::CvarDifferences( const CCommand &args )
{
	ICvar::Iterator iter( g_pCVar );
	for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
	{
		ConCommandBase *var = iter.Get();
		if ( var->IsCommand( ) )
			continue;
		if ( var->IsFlagSet(FCVAR_DEVELOPMENTONLY) || var->IsFlagSet(FCVAR_HIDDEN) )
			continue;

		if ( !Q_stricmp( ((ConVar *)var)->GetDefault(), ((ConVar *)var)->GetString() ) )
			continue;

		ConVar_PrintDescription( (ConVar *)var );	
	}
}


//-----------------------------------------------------------------------------
// Purpose: Toggles a cvar on/off, or cycles through a set of values
//-----------------------------------------------------------------------------
void CCvarUtilities::CvarToggle( const CCommand &args )
{
	int i;

	int c = args.ArgC();
	if ( c < 2 )
	{
		ConMsg( "Usage:  toggle <cvarname> [value1] [value2] [value3]...\n" );
		return;
	}

	ConVar *var = g_pCVar->FindVar( args[1] );
	
	if ( !IsValidToggleCommand( args[1] ) )
	{
		return;
	}

	if ( c == 2 )
	{
		// just toggle it on and off
		var->SetValue( !var->GetBool() );
		ConVar_PrintDescription( var );
	}
	else
	{
		// look for the current value in the command arguments
		for( i = 2; i < c; i++ )
		{
			if ( !Q_strcmp( var->GetString(), args[ i ] ) )
				break;
		}

		// choose the next one
		i++;

		// if we didn't find it, or were at the last value in the command arguments, use the 1st argument
		if ( i >= c )
		{
			i = 2;
		}

		var->SetValue( args[ i ] );
		ConVar_PrintDescription( var );
	}
}

int CCvarUtilities::CvarFindFlagsCompletionCallback( const char *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	int flagC = ARRAYSIZE( g_ConVarFlags );
	char const *pcmd = "findflags ";
	int len = Q_strlen( partial );

	if ( len < Q_strlen( pcmd ) )
	{
		int i = 0;
		for ( ; i < MIN( flagC, COMMAND_COMPLETION_MAXITEMS ); i++ )
		{
			Q_snprintf( commands[ i ], sizeof( commands[ i ] ), "%s %s", pcmd, g_ConVarFlags[i].desc );
			Q_strlower( commands[ i ] );
		}
		return i;
	}

	char const *pSub = partial + Q_strlen( pcmd );
	int nSubLen = Q_strlen( pSub );

	int values = 0;
	for ( int i=0; i < flagC; ++i )
	{
		if ( Q_strnicmp( g_ConVarFlags[i].desc, pSub, nSubLen ) )
			continue;

		Q_snprintf( commands[ values ], sizeof( commands[ values ] ), "%s %s", pcmd, g_ConVarFlags[i].desc );
		Q_strlower( commands[ values ] );
		++values;

		if ( values >= COMMAND_COMPLETION_MAXITEMS )
			break;
	}
	return values;
}

void CCvarUtilities::CvarFindFlags_f( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		ConMsg( "Usage:  findflags <string>\n" );
		ConMsg( "Available flags to search for: \n" );

		for ( int i=0; i < ARRAYSIZE( g_ConVarFlags ); i++ )
		{
			ConMsg( "   - %s\n", g_ConVarFlags[i].desc );
		}
		return;
	}

	// Get substring to find
	const char *search = args[1];

	// Loop through vars and print out findings
	ICvar::Iterator iter( g_pCVar );
	for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
	{
		ConCommandBase *var = iter.Get();
		if ( var->IsFlagSet(FCVAR_DEVELOPMENTONLY) || var->IsFlagSet(FCVAR_HIDDEN) )
			continue;

		for ( int j=0; j < ARRAYSIZE( g_ConVarFlags ); ++j )
		{
			if ( !var->IsFlagSet( g_ConVarFlags[j].bit ) )
				continue;
			
			if ( Q_stricmp( g_ConVarFlags[j].desc, search ) )
				continue;

			ConVar_PrintDescription( var );	
		}
	}	
}

int FindFlagsCompletionCallback( const char *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	return ConVarUtilities->CvarFindFlagsCompletionCallback( partial, commands );
}

//-----------------------------------------------------------------------------
// Purpose: Hook to command
//-----------------------------------------------------------------------------
CON_COMMAND_F_COMPLETION( findflags, "Find concommands by flags.", 0, FindFlagsCompletionCallback )
{
	ConVarUtilities->CvarFindFlags_f( args );
}


//-----------------------------------------------------------------------------
// Purpose: Hook to command
//-----------------------------------------------------------------------------
CON_COMMAND( cvarlist, "Show the list of convars/concommands." )
{
	ConVarUtilities->CvarList( args );
}


//-----------------------------------------------------------------------------
// Purpose: Print help text for cvar
//-----------------------------------------------------------------------------
CON_COMMAND( help, "Find help about a convar/concommand." )
{
	ConVarUtilities->CvarHelp( args );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( differences, "Show all convars which are not at their default values." )
{
	ConVarUtilities->CvarDifferences( args );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( toggle, "Toggles a convar on or off, or cycles through a set of values." )
{
	ConVarUtilities->CvarToggle( args );
}


void ResetGameConVarsToDefaults( void )
{
#if defined( LEFT4DEAD )
	ConVarRef testprocess( "test_progression_loop" );
	ConVarUtilities->ResetConVarsToDefaultValues( "z_" );
	if ( ! testprocess.GetInt() )
	{
		ConVarUtilities->ResetConVarsToDefaultValues( "sb_" );
	}
	ConVarUtilities->ResetConVarsToDefaultValues( "survivor_" );
	ConVarUtilities->ResetConVarsToDefaultValues( "director_" );
	ConVarUtilities->ResetConVarsToDefaultValues( "intensity_" );
	ConVarUtilities->ResetConVarsToDefaultValues( "rescue_" );
	ConVarUtilities->ResetConVarsToDefaultValues( "tongue_" );
	ConVarUtilities->ResetConVarsToDefaultValues( "inferno_" );
	ConVarUtilities->ResetConVarsToDefaultValues( "boomer_" );
	ConVarUtilities->ResetConVarsToDefaultValues( "hunter_" );
	ConVarUtilities->ResetConVarsToDefaultValues( "smoker_" );
	ConVarUtilities->ResetConVarsToDefaultValues( "tank_" );
	ConVarUtilities->ResetConVarsToDefaultValues( "nav_" );
#endif
}

CON_COMMAND_F( reset_gameconvars, "Reset a bunch of game convars to default values", FCVAR_CHEAT )
{
	ResetGameConVarsToDefaults();
}

//-----------------------------------------------------------------------------
// Purpose: Send the cvars to VXConsole
//-----------------------------------------------------------------------------
#if defined( USE_VXCONSOLE )
CON_COMMAND( getcvars, "" )
{
	{
		// avoid noisy requests
		// outer logic to prevent multiple requests more complicated than doing just this
#if defined( _X360 )
		bool bConnected = XBX_IsConsoleConnected();
		if ( !bConnected )
		{
			return;
		}
#endif
		static float s_flLastPublishTime = 0;
		if ( s_flLastPublishTime && Plat_FloatTime() < s_flLastPublishTime + 2.0f )
		{
			return;
		}
		s_flLastPublishTime = Plat_FloatTime();
	}

#if defined( _X360 )
	// get the version from the image
	// regardles of where the image came from (DVD, HDD) this cracks the embedded version info
	int nVersion = 0;
	if ( !IsCert() )
	{
		HANDLE hFile = CreateFile( "d:\\version.xtx", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
		if ( hFile != INVALID_HANDLE_VALUE )
		{
			DWORD nFileSize = GetFileSize( hFile, NULL );
			if ( nFileSize != (DWORD)-1 && nFileSize > 0 )
			{
				char versionData[1024];
				DWORD nBufferSize = MIN( nFileSize, sizeof( versionData ) - 1 );			
				DWORD nBytesRead = 0;
				BOOL bResult = ReadFile( hFile, versionData, nBufferSize, &nBytesRead, NULL );
				if ( bResult )
				{
					versionData[nBytesRead] = '\0';
					nVersion = atoi( versionData );
				}
			}
			CloseHandle( hFile );
		}
	}

	XBX_rVersion( nVersion );
#endif

	// Host_Init() will be calling us again, so defer this expensive operation until then
	if ( host_initialized )
	{
#ifdef _PS3
		if ( g_pValvePS3Console )
		{
			g_pCVar->PublishToVXConsole();
		}
#else
		g_pCVar->PublishToVXConsole();
#endif
	}
}
#endif

