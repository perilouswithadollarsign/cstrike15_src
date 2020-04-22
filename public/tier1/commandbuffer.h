//===== Copyright © 1996-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//


#ifndef COMMANDBUFFER_H
#define COMMANDBUFFER_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"

#include "tier1/utllinkedlist.h"
#include "tier1/convar.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CUtlBuffer;


//-----------------------------------------------------------------------------
// Invalid command handle
//-----------------------------------------------------------------------------
typedef intp CommandHandle_t;
enum
{
	COMMAND_BUFFER_INVALID_COMMAND_HANDLE = 0
};

//-----------------------------------------------------------------------------
// A command buffer class- a queue of argc/argv based commands associated
// with a particular time
//-----------------------------------------------------------------------------
class CCommandBuffer
{
public:
	// Constructor, destructor
	CCommandBuffer( );
	~CCommandBuffer();

    // Inserts text into the command buffer
	bool AddText( const char *pText, cmd_source_t cmdSource = kCommandSrcUserInput, int nTickDelay = 0 );

	// Used to iterate over all commands appropriate for the current time
	void BeginProcessingCommands( int nDeltaTicks );
	bool DequeueNextCommand( /*out*/ CCommand* pCommand );
	void EndProcessingCommands();

	// Are we in the middle of processing commands?
	bool IsProcessingCommands();

	// Delays all queued commands to execute at a later time
	void DelayAllQueuedCommands( int nTickDelay );

	// Indicates how long to delay when encoutering a 'wait' command
	void SetWaitDelayTime( int nTickDelay );

	// Returns a handle to the next command to process
	// (useful when inserting commands into the buffer during processing
	// of commands to force immediate execution of those commands,
	// most relevantly, to implement a feature where you stream a file
	// worth of commands into the buffer, where the file size is too large
	// to entirely contain in the buffer).
    CommandHandle_t GetNextCommandHandle();

	// Specifies a max limit of the args buffer. For unittesting. Size == 0 means use default
	void LimitArgumentBufferSize( int nSize );

	void SetWaitEnabled( bool bEnable )		{ m_bWaitEnabled = bEnable; }
	bool IsWaitEnabled( void )				{ return m_bWaitEnabled; }

private:
	enum
	{
		ARGS_BUFFER_LENGTH = 8192,
	};

	struct Command_t
	{
		int m_nTick;
		int m_nFirstArgS;
		int m_nBufferSize;
		cmd_source_t m_source;
	};

	// Insert a command into the command queue at the appropriate time
	void InsertCommandAtAppropriateTime( intp hCommand );
						   
	// Insert a command into the command queue
	// Only happens if it's inserted while processing other commands
	void InsertImmediateCommand( intp hCommand );

	// Insert a command into the command queue
	bool InsertCommand( const char *pArgS, int nCommandSize, int nTick, cmd_source_t cmdSource );

	// Returns the length of the next command, as well as the offset to the next command
	void GetNextCommandLength( const char *pText, int nMaxLen, int *pCommandLength, int *pNextCommandOffset );

	// Compacts the command buffer
	void Compact();

	// Parses argv0 out of the buffer
	bool ParseArgV0( CUtlBuffer &buf, char *pArgv0, int nMaxLen, const char **pArgs );

	char	m_pArgSBuffer[ ARGS_BUFFER_LENGTH ];
	int		m_nLastUsedArgSSize;
	int		m_nArgSBufferSize;
	CUtlFixedLinkedList< Command_t >	m_Commands;
	int		m_nCurrentTick;
	int		m_nLastTickToProcess;
	int		m_nWaitDelayTicks;
	intp	m_hNextCommand;
	int		m_nMaxArgSBufferLength;
	bool	m_bIsProcessingCommands;
	bool	m_bWaitEnabled;
};

#endif // COMMANDBUFFER_H
