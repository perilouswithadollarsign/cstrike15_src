//===== Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//


#include "quakedef.h"						 
#include "zone.h"
#include "demo.h"
#include "filesystem.h"
#include "filesystem_engine.h"
#include "eiface.h"
#include "server.h"
#include "sys.h"
#include "cl_splitscreen.h"
#include "baseautocompletefilelist.h"
#include "tier0/icommandline.h"
#include "tier1/utlbuffer.h"
#include "gl_cvars.h"
#include "tier0/memalloc.h"
#include "netmessages.h"
#include "client.h"
#include "sv_plugin.h"
#include "tier1/commandbuffer.h"
#include "cvar.h"
#include "vstdlib/random.h"
#include "tier1/utldict.h"
#include "tier0/etwprof.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


// This denotes an execution marker in the command stream.
#define CMDSTR_ADD_EXECUTION_MARKER "[$&*,`]"


#ifdef _DEBUG
ConVar cl_debug_respect_cheat_vars( "cl_debug_respect_cheat_vars", "0", 0, "(debug builds only) - when set to 0, the client can change cheat vars." );
#endif


extern ConVar sv_allow_wait_command;


#define	MAX_ALIAS_NAME	32
#define MAX_COMMAND_LENGTH 1024

struct cmdalias_t
{
	cmdalias_t	*next;
	char		name[ MAX_ALIAS_NAME ];
	char		*value;
};

static cmdalias_t	*cmd_alias = NULL;

static CCommandBuffer s_CommandBuffer[ CBUF_COUNT ];
static CThreadFastMutex s_CommandBufferMutex;
CUtlStringList m_WhitelistedConvars;
#define LOCK_COMMAND_BUFFER() AUTO_LOCK(s_CommandBufferMutex)

static FileAssociationInfo g_FileAssociations[] =
{
	{ ".dem", "playdemo" },
	{ ".sav", "load" },
	{ ".bsp", "map" },
};


// Client -> Server command throttling
// FIXME: Perhaps kForwardedCommandQuota_nCommandsPerSecond should instead be some fraction /
//        amount below sv_quota_stringcmdspersecond.  Right now that variable isn't networked,
//        so we just cap at the previous server 'throttle' value (after which commands were
//        discarded).  The new behavior kicks you from the server if you overflow, so it's
//        important to be significantly below that -- we don't throttle commands issued
//        by client code, only via the user via console/keybind input.
static const int kForwardedCommandQuota_nCommandsPerSecond = 16;
static double gForwardedCommandQuota_flTimeStart = -1.0;
static int gForwardedCommandQuota_nCount = 0;


//=============================================================================
// These functions manage a list of execution markers that we use to verify
// special commands in the command buffer.
//=============================================================================

static CUtlVector<int> g_ExecutionMarkers;

static int CreateExecutionMarker()
{
	if ( g_ExecutionMarkers.Count() > 2048 )
		g_ExecutionMarkers.Remove( 0 );

	int i = g_ExecutionMarkers.AddToTail( RandomInt( 0, 1<<30 ) );
	return g_ExecutionMarkers[i];
}

static bool FindAndRemoveExecutionMarker( int iCode )
{
	int i = g_ExecutionMarkers.Find( iCode );
	if ( i == g_ExecutionMarkers.InvalidIndex() )
		return false;
	
	g_ExecutionMarkers.Remove( i );
	return true;
}



//-----------------------------------------------------------------------------
// Used to allow cheats even if cheats aren't theoretically allowed
//-----------------------------------------------------------------------------
static bool g_bRPTActive = false;
void Cmd_SetRptActive( bool bActive )
{
	g_bRPTActive = bActive;
}

bool Cmd_IsRptActive()
{
	return g_bRPTActive;
}


//=============================================================================


//-----------------------------------------------------------------------------
// Just translates BindToggle <key> <cvar> into: bind <key> "increment var <cvar> 0 1 1"
//-----------------------------------------------------------------------------
CON_COMMAND( BindToggle, "Performs a bind <key> \"increment var <cvar> 0 1 1\"" )
{
	if( args.ArgC() <= 2 )
	{
		ConMsg( "BindToggle <key> <cvar>: invalid syntax specified\n" );
		return;
	}

	char newCmd[MAX_COMMAND_LENGTH];
	Q_snprintf( newCmd, sizeof(newCmd), "bind %s \"incrementvar %s 0 1 1\"\n", args[1], args[2] );

	Cbuf_InsertText( Cbuf_GetCurrentPlayer(), newCmd, args.Source() );
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
void Cbuf_Init()
{
	// Wait for 1 execute time
	for ( int i = 0; i < CBUF_COUNT; ++i )
	{
		s_CommandBuffer[ i ].SetWaitDelayTime( 1 );
	}
}

void Cbuf_Shutdown()
{
}

ECommandTarget_t Cbuf_GetCurrentPlayer()
{
	return ( ECommandTarget_t )( GET_ACTIVE_SPLITSCREEN_SLOT() );
}

//-----------------------------------------------------------------------------
// Clears the command buffer
//-----------------------------------------------------------------------------
void Cbuf_Clear( ECommandTarget_t eTarget )
{
	s_CommandBuffer[ eTarget ].SetWaitDelayTime( 1 );
}


//-----------------------------------------------------------------------------
// Adds command text at the end of the buffer
//-----------------------------------------------------------------------------
void Cbuf_AddText( ECommandTarget_t eTarget, const char *pText, cmd_source_t cmdSource, int nTickDelay )
{
	LOCK_COMMAND_BUFFER();
	if ( !s_CommandBuffer[ eTarget ].AddText( pText, cmdSource, nTickDelay ) )
	{
		ConMsg( "Cbuf_AddText: buffer overflow\n" );
	}
}


//-----------------------------------------------------------------------------
// Adds command text at the beginning of the buffer
//-----------------------------------------------------------------------------
void Cbuf_InsertText( ECommandTarget_t eTarget, const char *pText, cmd_source_t cmdSource, int nTickDelay )
{
	LOCK_COMMAND_BUFFER();
	// NOTE: This operation is only allowed when the command buffer
	// is in the middle of processing. If this assertion never triggers,
	// it's safe to eliminate Cbuf_InsertText altogether.
	// Otherwise, I have to add a feature to CCommandBuffer
	Assert( s_CommandBuffer[ eTarget ].IsProcessingCommands() );
	Cbuf_AddText( eTarget, pText, cmdSource, nTickDelay );
}

bool Cbuf_IsProcessingCommands( ECommandTarget_t eTarget )
{
	LOCK_COMMAND_BUFFER();
	return s_CommandBuffer[ eTarget ].IsProcessingCommands();
}

void Cbuf_AddExecutionMarker( ECommandTarget_t eTarget, ECmdExecutionMarker marker )
{
	int iMarkerCode = CreateExecutionMarker();
	
	// CMDCHAR_ADD_EXECUTION_MARKER tells us there's a special execution thing here.
	// (char)marker tells it what to turn on
	// iRandomCode is for security, so only our code can stuff this command into the buffer.
	char str[512];
	Q_snprintf( str, sizeof( str ), ";%s %c %d;", CMDSTR_ADD_EXECUTION_MARKER, (char)marker, iMarkerCode );
	
	Cbuf_AddText( eTarget, str, kCommandSrcCode );
}


//-----------------------------------------------------------------------------
// Executes commands in the buffer
//-----------------------------------------------------------------------------
static void Cbuf_ExecuteCommand( ECommandTarget_t eTarget, const CCommand &args )
{
	// Add the command text to the ETW stream to give better context to traces.
	ETWMark( args.GetCommandString() );

	// execute the command line
	const ConCommandBase *pCmd = Cmd_ExecuteCommand( eTarget, args );

#if !defined(DEDICATED)
	if ( pCmd && !pCmd->IsFlagSet( FCVAR_DONTRECORD ) )
	{
		demorecorder->RecordCommand( args.GetCommandString() );
	}
#endif
}


//-----------------------------------------------------------------------------
// Executes commands in the buffer
//-----------------------------------------------------------------------------
void Cbuf_Execute()
{
	VPROF("Cbuf_Execute");

	if ( !ThreadInMainThread() )
	{
		Warning( "Executing command outside main loop thread\n" );
		ExecuteOnce( DebuggerBreakIfDebugging() );
	}

	LOCK_COMMAND_BUFFER();

#if !defined( SPLIT_SCREEN_STUBS )
	int nSaveIndex = GET_ACTIVE_SPLITSCREEN_SLOT();
	bool bSaveResolvable = SET_LOCAL_PLAYER_RESOLVABLE( __FILE__, __LINE__, false );
#endif

	for ( int i = 0; i < CBUF_COUNT; ++i )
	{
		// If text was added with Cbuf_AddText and then Cbuf_Execute gets called from within handler, we're going
		//  to execute the new commands anyway, so we can ignore this extra execute call here.
		if ( s_CommandBuffer[ i ].IsProcessingCommands() )
			continue;

		// For player slots, force the correct context
		if ( i >= CBUF_FIRST_PLAYER && 
			 i < ( CBUF_FIRST_PLAYER + host_state.max_splitscreen_players ) )
		{
			SET_ACTIVE_SPLIT_SCREEN_PLAYER_SLOT( i );
			SET_LOCAL_PLAYER_RESOLVABLE( __FILE__, __LINE__, true );
		}
		else
		{
			SET_ACTIVE_SPLIT_SCREEN_PLAYER_SLOT( 0 );
			SET_LOCAL_PLAYER_RESOLVABLE( __FILE__, __LINE__, bSaveResolvable );
		}

		// NOTE: The command buffer knows about execution time related to commands,
		// but since HL2 doesn't, we're going to spoof the command time to simply
		// be the the number of times Cbuf_Execute is called.
		s_CommandBuffer[ i ].BeginProcessingCommands( 1 );
		CCommand nextCommand;

		while ( s_CommandBuffer[ i ].DequeueNextCommand( &nextCommand ) )
		{
			Cbuf_ExecuteCommand( ( ECommandTarget_t )i, nextCommand );
		}
		s_CommandBuffer[ i ].EndProcessingCommands( );
	}

	SET_ACTIVE_SPLIT_SCREEN_PLAYER_SLOT( nSaveIndex );
	SET_LOCAL_PLAYER_RESOLVABLE( __FILE__, __LINE__, bSaveResolvable );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *param - 
// Output : static char const
//-----------------------------------------------------------------------------
static char const *Cmd_TranslateFileAssociation(char const *param )
{
	static char sz[ 512 ];
	char *retval = NULL;

	char temp[ 512 ];
	V_strcpy_safe( temp, param );
	Q_FixSlashes( temp );
	Q_strlower( temp );

	const char *extension = V_GetFileExtension(temp);
	// must have an extension to map
	if (!extension)
		return retval;

	int c = ARRAYSIZE( g_FileAssociations );
	for ( int i = 0; i < c; i++ )
	{
		FileAssociationInfo& info = g_FileAssociations[ i ];
		
		if ( ! Q_strcmp( extension, info.extension+1 ) && 
			 ! CommandLine()->FindParm(va( "+%s", info.command_to_issue ) ) )
		{
			// Translate if haven't already got one of these commands			
			V_strcpy_safe( sz, temp );
			Q_FileBase( sz, temp, sizeof( sz ) );

			Q_snprintf( sz, sizeof( sz ), "%s %s", info.command_to_issue, temp );
			retval = sz;
			break;
		}		
	}

	// return null if no translation, otherwise return commands
	return retval;
}

//-----------------------------------------------------------------------------
// Purpose: Adds command line parameters as script statements
// Commands lead with a +, and continue until a - or another +
// Also automatically converts .dem, .bsp, and .sav files to +playdemo etc command line options
// hl2 +cmd amlev1
// hl2 -nosound +cmd amlev1
// Output : void Cmd_StuffCmds_f
//-----------------------------------------------------------------------------
CON_COMMAND( stuffcmds, "Parses and stuffs command line + commands to command buffer." )
{		
	if ( args.ArgC() != 1 )
	{
		ConMsg( "stuffcmds : execute command line parameters\n" );
		return;
	}

	MEM_ALLOC_CREDIT();

	CUtlBuffer build( 0, 0, CUtlBuffer::TEXT_BUFFER );

	// arg[0] is the executable name
	for ( int i=1; i < CommandLine()->ParmCount(); i++ )
	{
		const char *szParm = CommandLine()->GetParm(i);
		if (!szParm) continue;

		if (szParm[0] == '-') 
		{
			// skip -XXX options and eat their args
			const char *szValue = CommandLine()->ParmValue(szParm);
			if ( szValue ) i++;
			continue;
		}
		if (szParm[0] == '+')
		{
			// convert +XXX options and stuff them into the build buffer
			const char *szValue = CommandLine()->ParmValue(szParm);
			if (szValue)
			{
				// Special case for +map parameter on the command line to support a second argument
				char const *szSecondParameterUsed = NULL;
				if ( !Q_stricmp( "+map", szParm ) &&
					( CommandLine()->ParmCount() > ( i + 2 ) ) &&
					CommandLine()->GetParm( i + 2 ) )
				{
					char const *szAppendParameter = CommandLine()->GetParm( i + 2 );
					if ( ( szAppendParameter[0] != '+' ) &&
						 ( szAppendParameter[0] != '-' ) )
					{
						szSecondParameterUsed = szAppendParameter;
						build.PutString( va("%s %s %s\n", szParm+1, szValue, szSecondParameterUsed ) );
						++ i; // eat one parameter we used for map name
						++ i; // eat another parameter that was second appended parameter
					}
				}

				if ( !szSecondParameterUsed )
				{	// If we didn't use the second parameter, then just append command value to execution buffer
					build.PutString( va("%s %s\n", szParm+1, szValue ) );
					i++;
				}
			}
			else
			{
				build.PutString(szParm+1);
				build.PutChar('\n');
			}
		}
		else 
		{
			// singleton values, convert to command
			char const *translated = Cmd_TranslateFileAssociation( CommandLine()->GetParm( i ) );
			if (translated)
			{
				build.PutString(translated);
				build.PutChar('\n');
			}
		}
	}

	build.PutChar( '\0' );
		
	if ( build.TellPut() > 1 )
	{
		Cbuf_InsertText( Cbuf_GetCurrentPlayer(), (char *)build.Base(), args.Source() );
	}
}



bool IsValidFileExtension( const char *pszFilename )
{
	if ( !pszFilename )
	{
		return false;
	}

	if ( Q_strstr( pszFilename, ".exe" ) ||
		 Q_strstr( pszFilename, ".vbs" ) ||
		 Q_strstr( pszFilename, ".com" ) ||
		 Q_strstr( pszFilename, ".bat" ) ||
		 Q_strstr( pszFilename, ".dll" ) ||
		 Q_strstr( pszFilename, ".ini" ) ||
		 Q_strstr( pszFilename, ".gcf" ) ||
		 Q_strstr( pszFilename, ".sys" ) ||
		 Q_strstr( pszFilename, ".blob" ) )
	{
		return false;
	}

	return true;
}

bool IsWhiteListedCmd( const char *pszCmd )
{
	if ( m_WhitelistedConvars.Count() == 0 )
	{
		const char *svfileName = "bspconvar_whitelist.txt";
		KeyValues *pKV_wl = new KeyValues( "convars" );
		if ( pKV_wl->LoadFromFile( g_pFullFileSystem, svfileName, "GAME" ) )
		{
			KeyValuesDumpAsDevMsg( pKV_wl );
			for ( KeyValues *sub = pKV_wl->GetFirstSubKey(); sub; sub = sub->GetNextKey() )
			{
				m_WhitelistedConvars.CopyAndAddToTail( sub->GetName() );
			}
		}
		else
		{
			DevMsg( "Failed to cache %s\n", svfileName );
			return false;
		}
	}

	for ( int i = 0; i < m_WhitelistedConvars.Count(); ++i )
	{
		if ( !Q_stricmp(m_WhitelistedConvars[i], pszCmd) )
		{
			return true;
		}
	}

	return false;
}

/*
===============
Cmd_Exec_f
===============
*/

void _Cmd_Exec_f( const CCommand &args, bool bOnlyIfExists, bool bUseWhitelist = false )
{
	LOCK_COMMAND_BUFFER();
	char	*f;
	const char	*s;
	char	fileName[MAX_OSPATH];

	int argc = args.ArgC();
	if ( argc < 2 )
	{
		ConMsg( "%s <filename> [path id]: execute a script file\n", args[ 0 ] );
		return;
	}

	s = args[ 1 ];
	DevMsg( "Execing config: %s\n", s );

	// Optional path ID. * means no path ID.
	const char *pPathID = NULL;
	if ( argc >= 3 )
	{
		pPathID = args[ 2 ];
	}
	else
	{
		pPathID = "*";
	}

	if ( !Q_stricmp( pPathID, "T" ) )
	{
		// Has an extension already?
		Q_snprintf( fileName, sizeof( fileName ), "T:/cfg/%s", s );
	}
	else
	{
		// Ensure it has an extension
		Q_snprintf( fileName, sizeof( fileName ), "//%s/cfg/%s", pPathID, s );
		Q_DefaultExtension( fileName, ".cfg", sizeof( fileName ) );
		
		// check path validity
		if ( !COM_IsValidPath( fileName ) )
		{
			ConMsg( "%s %s: invalid path.\n", args[ 0 ], fileName );
			return;
		}
	}

	// check for invalid file extensions
	if ( !IsValidFileExtension( fileName ) )
	{
		ConMsg( "%s %s: invalid file type.\n", args[ 0 ], fileName );
		return;
	}

	// 360 doesn't need to do costly existence checks
	if ( IsPC() && g_pFileSystem->FileExists( fileName ) )
	{
		// don't want to exec files larger than 1 MB
		// probably not a valid file to exec
		unsigned int size = g_pFileSystem->Size( fileName );
		if ( size > 1*1024*1024 )
		{
			ConMsg( "%s %s: file size larger than 1 MB!\n", args[ 0 ], s );
			return;
		}
	}

	char buf[16384];
	int len;
	f = (char *)COM_LoadStackFile( fileName, buf, sizeof( buf ), len );
	if ( !f )
	{
		if ( !V_stristr( s, "autoexec.cfg" ) && !V_stristr( s, "joystick.cfg" ) && !V_stristr( s, "game.cfg" ))
		{
			// File doesn't exist, fail silently?
			if ( !bOnlyIfExists )
			{
				ConMsg( "%s: couldn't exec %s\n", args[ 0 ], s );
			}
		}
		return;
	}
	
	char *original_f = f;
	ConDMsg( "execing %s\n", s );
	
	ECommandTarget_t eTarget = CBUF_FIRST_PLAYER;

	// A bit of hack, but find the context (probably CBUF_SERVER) who is executing commands!
	for ( int i = 0; i < CBUF_COUNT; ++i )
	{
		if ( s_CommandBuffer[ i ].IsProcessingCommands() )
		{
			eTarget = (ECommandTarget_t)i;
			break;
		}
	}

	CCommandBuffer &rCommandBuffer = s_CommandBuffer[ eTarget ];

	// check to make sure we're not going to overflow the cmd_text buffer
	int hCommand = rCommandBuffer.GetNextCommandHandle();

	KeyValues *pKV_wl = new KeyValues( "convars" );

	// Execute each command immediately
	const char *pszDataPtr = f;
	while( pszDataPtr )
	{
		// parse a line out of the source
		pszDataPtr = COM_ParseLine( pszDataPtr );

		// no more tokens
		if ( Q_strlen( com_token ) <= 0 )
			continue;

		Cbuf_InsertText( eTarget, com_token, args.Source() );

		// Execute all commands provoked by the current line read from the file
		while ( rCommandBuffer.GetNextCommandHandle() != hCommand )
		{
			CCommand execCommand;

			if( rCommandBuffer.DequeueNextCommand( &execCommand ) )
			{
				bool bFoundConvar = true;
				if ( bUseWhitelist )
				{
					bFoundConvar = IsWhiteListedCmd( *execCommand.ArgV() );
				}

				if ( bFoundConvar )
					Cbuf_ExecuteCommand( eTarget, execCommand );
			}
			else
			{
				Assert( 0 );
				break;
			}
		}
	}

	if ( pKV_wl )
	{
		pKV_wl->deleteThis();
		pKV_wl = NULL;
	}

	if ( f != buf )
	{
		// Hack for VCR playback. vcrmode allocates the memory but doesn't use the debug memory allocator,
		// so we don't want to free what it allocated.
		if ( f == original_f )
		{
			free( f );
		}
	}
}

void Cmd_Exec_f( const CCommand &args )
{
	_Cmd_Exec_f( args, false );
}

void Cmd_ExecIfExists_f( const CCommand &args )
{
	_Cmd_Exec_f( args, true );
}

void Cmd_ExecWithWhiteList_f( const CCommand &args )
{
	_Cmd_Exec_f( args, false, true );
}

/*
===============
Cmd_Echo_f

Just prints the rest of the line to the console
===============
*/
CON_COMMAND_F( echo, "Echo text to console.", FCVAR_SERVER_CAN_EXECUTE )
{
	int argc = args.ArgC();
	for ( int i=1; i<argc; i++ )
	{
		ConMsg ("%s ", args[i] );
	}
	ConMsg ("\n");
}

/*
===============
Cmd_Alias_f

Creates a new command that executes a command string (possibly ; seperated)
===============
*/
CON_COMMAND( alias, "Alias a command." )
{
	cmdalias_t	*a;
	char		cmd[MAX_COMMAND_LENGTH];
	int			i, c;
	const char	*s;

	int argc = args.ArgC();
	if ( argc == 1 )
	{
		ConMsg ("Current alias commands:\n");
		for (a = cmd_alias ; a ; a=a->next)
		{
			ConMsg ("%s : %s\n", a->name, a->value);
		}
		return;
	}

	s = args[1];
	if ( Q_strlen(s) >= MAX_ALIAS_NAME )
	{
		ConMsg ("Alias name is too long\n");
		return;
	}

// copy the rest of the command line
	cmd[0] = 0;		// start out with a null string
	c = argc;
	for (i=2 ; i< c ; i++)
	{
		V_strcat_safe( cmd, args[i] );
		if (i != c)
		{
			V_strcat_safe( cmd, " " );
		}
	}
	V_strcat_safe( cmd, "\n" );

	// if the alias already exists, reuse it
	for (a = cmd_alias ; a ; a=a->next)
	{
		if (!Q_strcmp(s, a->name))
		{
			if ( !Q_strcmp( a->value, cmd ) )		// Re-alias the same thing
				return;

			delete[] a->value;
			break;
		}
	}

	if (!a)
	{
		ConCommandBase *pCommandExisting = g_pCVar->FindCommandBase( s );
		if ( pCommandExisting )
		{
			ConMsg( "Cannot alias an existing %s\n", pCommandExisting->IsCommand() ? "concommand" : "convar" );
			return;
		}

		a = (cmdalias_t *)new cmdalias_t;
		a->next = cmd_alias;
		cmd_alias = a;
	}
	V_strcpy_safe ( a->name, s );	

	a->value = COM_StringCopy(cmd);
}

/*
===============
Runs a command only if that command exits in the bspwhitelist
===============
*/
CON_COMMAND( whitelistcmd, "Runs a whitelisted command." )
{
	Cmd_ForwardToServerWithWhitelist( args );
}

/*
=============================================================================

					COMMAND EXECUTION

=============================================================================
*/

int				cmd_clientslot = -1;


//-----------------------------------------------------------------------------
// Purpose: 
// Output : void Cmd_Init
//-----------------------------------------------------------------------------
CON_COMMAND( cmd, "Forward command to server." )
{
	Cmd_ForwardToServer( args );
}

CON_COMMAND_AUTOCOMPLETEFILE( exec, Cmd_Exec_f, "Execute script file.", "cfg", cfg );
CON_COMMAND_AUTOCOMPLETEFILE( execifexists, Cmd_ExecIfExists_f, "Execute script file if file exists.", "cfg", cfg );
CON_COMMAND_AUTOCOMPLETEFILE( execwithwhitelist, Cmd_ExecWithWhiteList_f, "Execute script file, only execing convars on a whitelist.", "cfg", cfg );




void Cmd_Init( void )
{
	Sys_CreateFileAssociations( ARRAYSIZE( g_FileAssociations ), g_FileAssociations );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Cmd_Shutdown( void )
{
	// TODO, cleanup
	while ( cmd_alias )
	{
		cmdalias_t *next = cmd_alias->next;
		delete cmd_alias->value;	// created by StringCopy()
		delete cmd_alias;
		cmd_alias = next;
	}
}


//-----------------------------------------------------------------------------
// FIXME: Remove this! This is a temporary hack to deal with backward compat
//-----------------------------------------------------------------------------
void Cmd_Dispatch( const ConCommandBase *pCommand, const CCommand &command )
{
	ConCommand *pConCommand = const_cast<ConCommand*>( static_cast<const ConCommand*>( pCommand ) );
	pConCommand->Dispatch( command );
}


static void HandleExecutionMarker( const char *pCommand, const char *pMarkerCode )
{
	int iMarkerCode = atoi( pMarkerCode );
	
	// Validate..
	if ( FindAndRemoveExecutionMarker( iMarkerCode ) )
	{
		// Ok, now it's validated, so do the command.
		// REI CSGO: We no longer use execution markers, but I'm leaving this mechanism in here
		ECmdExecutionMarker command = (ECmdExecutionMarker)(pCommand[0]);

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable: 4065) // switch statement contains 'default' but no 'case' labels
#endif // _WIN32

		switch(command)
		{
		default:
			Warning( "Unrecognized execution marker '%c'\n", pCommand[0] );
		}

#ifdef _WIN32
#pragma warning(pop)
#endif
	}
	else
	{
		static int cnt = 0;
		if ( ++cnt < 3 )
			Warning( "Invalid execution marker code.\n" );
	}
}

static bool ShouldPreventServerCommand( const CCommand& args, const ConCommandBase *pCommand )
{
	// If the command didn't come from the server, then we aren't filtering it here.
	if ( args.Source() != kCommandSrcNetServer )
		return false;

	// Server can execute any command on client if this is a single player game.
	if ( Host_IsSinglePlayerGame() )
		return false;

	// If we don't understand the command, and it came from a server command, then forward it back to the server.
	// Lots of server plugins use this.  They use engine->ClientCommand() to have a client execute a command that 
	// they have hooked on the server. We disabled it once and they freaked. It IS redundant since they could just
	// code the call to their command on the server, but they complained loudly enough that we're adding it back in
	// since there's no exploit that we know of by allowing it.
	if ( !pCommand )
		return false;

	// If the command is marked to be executable by the server, allow it.
	if ( pCommand->IsFlagSet( FCVAR_SERVER_CAN_EXECUTE ) )
		return false;

	// Otherwise we are filtering the command.  Print a warning to the client console (the server shouldn't be
	// sending commands that it isn't allowed to execute on the client)
	Warning( "FCVAR_SERVER_CAN_EXECUTE prevented server running command: %s\n", args.GetCommandString() );
	return true;
}

		
static bool ShouldPreventClientCommand( const CCommand& args, const ConCommandBase *pCommand )
{
	// If the command didn't come from ClientCmd(), we don't filter it here.
	if ( args.Source() != kCommandSrcClientCmd )
		return false;

	// Commands we don't recognize aren't prevented here (they will get forwarded to the server)
	if ( !pCommand )
		return false;

	// Commands that are explicitly marked as executable by ClientCmd() aren't filtered
	if ( pCommand->IsFlagSet( FCVAR_CLIENTCMD_CAN_EXECUTE ) )
		return false;

	// Otherwise we are going to filter the command.  Check if we should warn the user about this:

	// If this command is in the game DLL, don't mention it because we're going to forward this
	// request to the server and let the server handle it.
	if ( !pCommand->IsFlagSet( FCVAR_GAMEDLL ) )
	{
		Warning( "FCVAR_CLIENTCMD_CAN_EXECUTE prevented running command: %s\n", args.GetCommandString() );
	}

	return true;
}


//-----------------------------------------------------------------------------
// A complete command line has been parsed, so try to execute it
// FIXME: lookupnoadd the token to speed search?
//-----------------------------------------------------------------------------
const ConCommandBase *Cmd_ExecuteCommand( ECommandTarget_t eTarget, const CCommand &command, int nClientSlot )
{	
	// execute the command line
	if ( !command.ArgC() )
		return NULL;		// no tokens
	
	// First, check for execution markers.
	if ( Q_strcmp( command[0], CMDSTR_ADD_EXECUTION_MARKER ) == 0 )
	{
		if ( command.ArgC() == 3 )
		{
			HandleExecutionMarker( command[1], command[2] );
		}
		else
		{
			Warning( "WARNING: INVALID EXECUTION MARKER.\n" );
		}
		
		return NULL;
	}

	// check alias
	cmdalias_t *a;
	for ( a=cmd_alias; a; a=a->next )
	{
		if ( !Q_strcasecmp( command[0], a->name ) )
		{
			Cbuf_InsertText( Cbuf_GetCurrentPlayer(), a->value, command.Source() );
			return NULL;
		}
	}
	
	cmd_clientslot = nClientSlot;

	// check ConCommands
	const ConCommandBase *pCommand = g_pCVar->FindCommandBase( command[0] );

	// If we prevent a server command due to FCVAR_SERVER_CAN_EXECUTE not being set, then we get out immediately.
	if ( ShouldPreventServerCommand( command, pCommand ) )
		return NULL;

	// FIXME: Why do we treat convars differently than commands here?
	if ( pCommand && pCommand->IsCommand() )
	{
		if ( !ShouldPreventClientCommand( command, pCommand ) && pCommand->IsCommand() )
		{
			bool isServerCommand = 
				// Command is marked for execution on the server.
				pCommand->IsFlagSet( FCVAR_GAMEDLL )
				// Not received over the network
				&& ( command.Source() != kCommandSrcNetClient && command.Source() != kCommandSrcNetServer )
				// Not HLDS
				&& !sv.IsDedicated();

			// Hook to allow game .dll to figure out who type the message on a listen server
			if ( serverGameClients )
			{
				// We're actually the server, so set it up locally
				if ( sv.IsActive() )
				{
					g_pServerPluginHandler->SetCommandClient( -1 );

	#ifndef DEDICATED
					// Special processing for listen server player
					if ( isServerCommand )
					{
						if ( splitscreen->IsLocalPlayerResolvable() )
						{
							g_pServerPluginHandler->SetCommandClient( GetLocalClient().m_nPlayerSlot );
						}
						else
						{
							g_pServerPluginHandler->SetCommandClient( GetBaseLocalClient().m_nPlayerSlot );
						}
					}
	#endif
				}
				// We're not the server, but we've been a listen server (game .dll loaded)
				//  forward this command tot he server instead of running it locally if we're still
				//  connected
				// Otherwise, things like "say" won't work unless you quit and restart
				else if ( isServerCommand )
				{
	#ifndef DEDICATED
					if ( GetBaseLocalClient().IsConnected() )
					{
						Cmd_ForwardToServer( command );
						return NULL;
					}
	#endif
					// It's a server command, but we're not connected to a server.  Don't try to execute it.
					return NULL;
				}
			}

			// Allow cheat commands in debug, or multiplayer with sv_cheats on
			if ( pCommand->IsFlagSet( FCVAR_CHEAT ) )
			{
				if ( !CanCheat() )
				{
					// But.. if the server is allowed to run this command and the server DID run this command, then let it through.
					// (used by soundscape_flush)
					if ( command.Source() != kCommandSrcNetServer || !pCommand->IsFlagSet( FCVAR_SERVER_CAN_EXECUTE ) )
					{
						if ( Host_IsSinglePlayerGame() )
						{
							Msg( "This game doesn't allow cheat command %s in single player, unless you have sv_cheats set to 1.\n", pCommand->GetName() );
						}
						else
						{
							Msg( "Can't use cheat command %s in multiplayer, unless the server has sv_cheats set to 1.\n", pCommand->GetName() );
						}
						return NULL;
					}
				}
			}

			if ( pCommand->IsFlagSet( FCVAR_SPONLY ) )
			{
				if ( !Host_IsSinglePlayerGame() )
				{
					Msg( "Can't use command %s in multiplayer.\n", pCommand->GetName() );
					return NULL;
				}
			}

			if ( pCommand->IsFlagSet( FCVAR_DEVELOPMENTONLY ) )
			{
				Msg( "Unknown command \"%s\"\n", pCommand->GetName() );
				return NULL;
			}

			Cmd_Dispatch( pCommand, command );
			return pCommand;
		}
	}

	// check cvars
	if ( ConVarUtilities->IsCommand( command, ( int )eTarget ) )
		return pCommand;

	#ifndef DEDICATED
	// forward the command line to the server, so the entity DLL can parse it
	if ( command.Source() != kCommandSrcNetClient )
	{
		if ( GetBaseLocalClient().IsConnected() )
		{
			Cmd_ForwardToServer( command );
			return NULL;
		}
	}
	#endif

	Msg( "Unknown command \"%s\"\n", command[0] );
	return NULL;
}

const char* Cmd_AliasToCommandString( const char* szAliasName )
{
	if ( !szAliasName )
		return NULL;

	for ( cmdalias_t* a = cmd_alias; a; a = a->next )
	{
		if ( !Q_strcasecmp( szAliasName, a->name ) )
		{
			return a->value;
		}
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Sends the entire command line over to the server
//-----------------------------------------------------------------------------
void Cmd_ForwardToServer( const CCommand &args, bool bReliable )
{
	// YWB 6/3/98 Don't forward if this is a dedicated server
#ifndef DEDICATED
	char str[1024];

	// no command to forward
	if ( args.ArgC() == 0 )
		return;

	// Special case: "cmd whatever args..." is forwarded as "whatever args...";
	// in this case we strip "cmd" from the input.
	if ( Q_strcasecmp( args[0], "cmd" ) == 0 )
		V_strcpy_safe( str, args.ArgS() );
	else
		V_strcpy_safe( str, args.GetCommandString() );
	
	extern IBaseClientDLL *g_ClientDLL;
	if ( demoplayer->IsPlayingBack() && g_ClientDLL )
	{
		// Not really connected, but can let client dll trap it
		g_ClientDLL->OnCommandDuringPlayback( str );
	}
	else
	{
		// Throttle user-input commands but not commands issued by code
		if ( args.Source() == kCommandSrcUserInput )
		{
			if(realtime - gForwardedCommandQuota_flTimeStart >= 1.0)
			{
				// reset quota
				gForwardedCommandQuota_flTimeStart = realtime;
				gForwardedCommandQuota_nCount = 0;
			}

			// Add 1 to quota used
			gForwardedCommandQuota_nCount++;

			// If we are over quota commands per second, dump this on the floor.
			// If we spam the server with too many commands, it will kick us.
			if ( gForwardedCommandQuota_nCount > kForwardedCommandQuota_nCommandsPerSecond )
			{
				ConMsg( "Ignoring command '%s': too many server commands issued per second\n", str );
				return;
			}
		}

		GetLocalClient().SendStringCmd( str );
	}
#endif
}

//-----------------------------------------------------------------------------
// Sends the entire command line over to the server only if it is whitelisted
//-----------------------------------------------------------------------------
void Cmd_ForwardToServerWithWhitelist( const CCommand &args, bool bReliable )
{
 	int argc = args.ArgC();
	char str[1024];
	str[0] = 0;

	if ( argc > 1 && args[1] && IsWhiteListedCmd( args[1] ) )
	{
		V_strcat_safe( str, args.ArgS() );
		Cbuf_AddText( CBUF_SERVER, str );
	}
}
