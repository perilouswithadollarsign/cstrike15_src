//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// cmd.h -- Command buffer and command execution
// Any number of commands can be added in a frame, from several different sources.
// Most commands come from either keybindings or console line input, but remote
// servers can also send across commands and entire text files can be execed.
// 
// The + command line options are also added to the command buffer.
// 
// The game starts with a Cbuf_AddText ("exec quake.rc\n"); Cbuf_Execute ();
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef CMD_H
#define CMD_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/commandbuffer.h" // cmd_source_t

// REI 7/18/2016:
//
// The main execution point for console commands is in Cmd_ExecuteCommand().  There is also a
// backdoor via Cmd_Dispatch which skips the access controls and network forwarding in
// Cmd_ExecuteCommand().
//
// In addition, there is some special game/server specific code for executing commands from the
// client that skips the normal CON_COMMAND parsing / access control / execution path.  See
// section (2) below.
//
// There are 4 main paths to executing console commands, and most CON_COMMANDs fit into one
// or more of these paths based on what flags they have set.  ConVars are similar to ConCommands
// but have some additional access restrictions; in particular, FCVAR_GAMEDLL vars cannot
// be executed on the server from the client, and FCVAR_REPLICATED variables are transferred from
// the server to the client automatically, not via networked console commands. (REI FIXME: Should
// probably unify access controls between console vars and console commands.)
//
// 1) No access-control flags set.  Generally can be executed locally on either client or server,
//    but see exceptions below.
//
//    Mostly handled in cmd.cpp
//    Usually created via Cbuf_AddText/CBuf_InsertText in cmd.cpp
//    Executed in a batch via Cbuf_Execute -> Cmd_ExecuteCommand, also in cmd.cpp
//
// 2) FCVAR_GAMEDLL / "special" non-CON_COMMAND commands. The client cannot execute these locally,
//    and always forwards these commands to the server which will execute those commands on the client's
//    behalf.  The "special" commands are also not executable at the server console AFAICT.
//
//    Sent by any CNetMsg_StringCommand; the canonical way to send one is
//    via Cmd_ForwardToServer() in cmd.cpp, or via engine->ServerCmd() in cdll_engine_int.cpp
//
//    Received by CBaseClient::NETMsg_StringCmd in baseclient.cpp and forwarded
//    to CGameClient::ExecuteStringCommand in sv_client.cpp.  This either dispatches them to
//    Cmd_Dispatch (direct execution, kind of scary? shouldn't it use Cmd_ExecuteCommand?)
//    for normal CON_COMMANDs, or sends them to ::ClientCommand in server/client.cpp for
//    other commands which are just matched via giant if(strcmp) branches.
//
//    An example from CS:GO cs_gamerules.cpp: 
// 			if ( FStrEq( args[0], "tr_map_show_exit_door_msg" ) )
// 			{
// 				IGameEvent * event = gameeventmanager->CreateEvent( "tr_exit_hint_trigger" );
// 				if ( event )
// 					gameeventmanager->FireEvent( event );
// 				return true;
// 			}
//    (Note that this command could be executed by any client at any time by just typing
//    "tr_map_show_exit_door_msg" into the console)
//
//    There's also a brief excursion to server plugins in case they want to override any commands
//    as well.
//
//    WARNING: FCVAR_GAMEDLL is automatically set on *every* console var/command defined in the
//             sever DLL.  This means that in the absence of further intervention, *every* client
//             can execute those commands, regardless of your intent.  The common workaround to
//             this is to include this check in your console commands:
//                 if ( !UTIL_IsCommandIssuedByServerAdmin() ) return;
//    Note than convars do not have this vulnerability; FCVAR_GAMEDLL convars are not modifyable
//    by clients by default.  I am not sure about the reasoning behind this discrepancy.  I
//    believe we should have another flag FCVAR_CLIENT_CAN_EXECUTE as a parallel for
//    FCVAR_SERVER_CAN_EXECUTE, but that would be a pretty drastic change at this moment and
//    certainly would fork CS:GO from other Source games.
//
// 3) FCVAR_SERVER_CAN_EXECUTE.  Commands which the client will execute on behalf of the server
//    if requested.  Note that the server is allowed to call FCVAR_CHEAT commands that the
//    client could not execute themselves (that is, commands with both FCVAR_SERVER_CAN_EXECUTE 
//    *and* FCVAR_CHEAT are executable on the client *only* when requested by the server).
//
//    Sent by any CNETMsg_StringCmd; Two canonical ways to send this message:
//    - SV_ExecuteRemoteCommand in sv_main.cpp
//    - g_pVEngineServer->ClientCommand in vengineserver_impl.cpp (important
//      note: "ClientCommand" here is different from "ClientCmd" below!!)
//
//    Received by CBaseClientState::NETMsg_StringCmd in baseclientstate.cpp
//    -> CBaseClientState::InternalProcessStringCommand in baseclientstate.cpp
//    -> CBuf_AddText() (the normal way to put things into the console)
//
//    Note that in the absence of other access controls, these will also be in class (1),
//    executable from the console on both server and client.
//
// 4) FCVAR_CLIENTCMD_CAN_EXECUTE.  Commands sent from code in the client, to other code in
//    the client.  I am not sure of the purpose of this restriction, since the client code
//    has full access to all console commands if it wants.
//
//    Generally sent via engine->ClientCmd() on the client.  Can call *any* command by using
//    engine->ClientCmd_Unrestricted() or the immediate form engine->ExecuteClientCmd() instead.
//
//    Received via the normal Cmd_ExecuteCommand path in cmd.cpp.  In particular this means
//    that calls to these commands will be delated until command processing happens.
//
//    Note that in the absence of other access controls, these will also be in class (1),
//    executable from the console on both server and client.  An interesting combination is
//    FCVAR_CLIENTCMD_CAN_EXECUTE -> FCVAR_GAMEDLL, which will cause calls to
//    engine->ClientCmd() to redirect to the server when those commands are processed.
//
// There are a few other access-control mechanisms (most notably FCVAR_CHEAT) which disable
// access to certain commands on almost all paths unless specific engine states are met.  I
// am not covering those here.
//
// Note that despite the name, not all of these commands are executed via the console.  Many
// items in classes (1) and (2) are executed via keybinds on the client, or via automatically
// executed config files on either client or server.  Paths (3) and (4) are really only
// accessible via code.
// 
//


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CCommand;
class ConCommandBase;

typedef enum
{
	// These should be single character printable things, like this:
	//     eCmdExecutionMarker_Enable_FCVAR_SERVER_CAN_EXECUTE = 'a',
} ECmdExecutionMarker;

//-----------------------------------------------------------------------------
// Initialization, shutdown of the command buffer
//-----------------------------------------------------------------------------
void Cbuf_Init (void);
void Cbuf_Shutdown( void );

typedef enum 
{
	CBUF_FIRST_PLAYER = 0,
	CBUF_LAST_PLAYER = MAX_SPLITSCREEN_CLIENTS - 1,
	CBUF_SERVER = CBUF_LAST_PLAYER + 1,

	CBUF_COUNT,
} ECommandTarget_t;

//-----------------------------------------------------------------------------
// Clears the command buffer
//-----------------------------------------------------------------------------
void Cbuf_Clear( ECommandTarget_t eTarget );


//-----------------------------------------------------------------------------
// as new commands are generated from the console or keybindings,
// the text is added to the end of the command buffer.
//-----------------------------------------------------------------------------
void Cbuf_AddText ( ECommandTarget_t eTarget, const char *text, cmd_source_t source = kCommandSrcCode, int nTickDelay = 0 );


//-----------------------------------------------------------------------------
// when a command wants to issue other commands immediately, the text is
// inserted at the beginning of the buffer, before any remaining unexecuted
// commands.
//-----------------------------------------------------------------------------
void Cbuf_InsertText( ECommandTarget_t eTarget, const char *text, cmd_source_t source = kCommandSrcCode, int nTickDelay = 0 );


// These allow you to create blocks in the command stream where certain rules apply.
// ONLY use Cbuf_AddText in between execution markers. If you use Cbuf_InsertText,
// it will put that stuff before the execution marker and the execution marker won't apply.
//
// cl_restrict_server_commands uses this. It inserts a marker that says, "don't run 
// anything unless it's marked with FCVAR_SERVER_CAN_EXECUTE", then inserts some commands,
// then removes the execution marker. That way, ANYTIME Cbuf_Execute() is called, 
// it will apply the cl_restrict_server_commands rules correctly.
void Cbuf_AddExecutionMarker( ECommandTarget_t eTarget, ECmdExecutionMarker marker );


// Pulls off \n terminated lines of text from the command buffer and sends
// them through Cmd_ExecuteString.  Stops when the buffer is empty.
// Normally called once per frame, but may be explicitly invoked.
// Do not call inside a command function!
//-----------------------------------------------------------------------------
void Cbuf_Execute();

ECommandTarget_t Cbuf_GetCurrentPlayer();

//===========================================================================

/*

Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.

Commands can come from three sources, but the handler functions may choose
to dissallow the action or forward it to a remote server if the source is
not apropriate.

*/

// FIXME: Move these into a field of CCommand?
extern int			cmd_clientslot;

//-----------------------------------------------------------------------------
// Initialization, shutdown
//-----------------------------------------------------------------------------
void Cmd_Init (void);
void Cmd_Shutdown( void );


//-----------------------------------------------------------------------------
// Executes a command given a CCommand argument structure
//-----------------------------------------------------------------------------
const ConCommandBase *Cmd_ExecuteCommand( ECommandTarget_t eTarget, const CCommand &command, int nClientSlot = -1 );


//-----------------------------------------------------------------------------
// Dispatches a command with the requested arguments
//-----------------------------------------------------------------------------
void Cmd_Dispatch( const ConCommandBase *pCommand, const CCommand &args );


//-----------------------------------------------------------------------------
// adds the current command line as a clc_stringcmd to the client message.
// things like godmode, noclip, etc, are commands directed to the server,
// so when they are typed in at the console, they will need to be forwarded.
// If bReliable is true, it goes into cls.netchan.message.
// If bReliable is false, it goes into cls.datagram.
//-----------------------------------------------------------------------------
void Cmd_ForwardToServer( const CCommand &args, bool bReliable = true );

void Cmd_ForwardToServerWithWhitelist( const CCommand &args, bool bReliable = true );


// Used to allow cheats even if cheats aren't theoretically allowed
void Cmd_SetRptActive( bool bActive );
bool Cmd_IsRptActive();

const char* Cmd_AliasToCommandString( const char* szAliasName );

bool Cbuf_IsProcessingCommands( ECommandTarget_t eTarget );

#endif // CMD_H
