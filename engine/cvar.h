//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#if !defined( CVAR_H )
#define CVAR_H
#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Forward declarations 
//-----------------------------------------------------------------------------
class ConVar;
class ConCommandBase;
class CCommand;
class CUtlBuffer;

#define COMMAND_COMPLETION_MAXITEMS		64
#define COMMAND_COMPLETION_ITEM_LENGTH	64

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CCvarUtilities
{
public:
	bool IsCommand( const CCommand &args, const int iSplitscreenSlot = -1 );

	// Writes lines containing "set variable value" for all variables
	// with the archive flag set to true.
	void WriteVariables( CUtlBuffer *buff, const int iSplitscreenSlot = -1, bool bSlotRequired = false, void *pConvarsList = 0 );

	// Returns the # of cvars with the server flag set.
	int	CountVariablesWithFlags( int flags );

	// Lists cvars to console
	void CvarList( const CCommand &args );

	// Prints help text for cvar
	void CvarHelp( const CCommand &args );

	// Revert all cvar values
	void CvarRevert( const CCommand &args );

	// Revert all cvar values
	void CvarDifferences( const CCommand &args );

	// Toggles a cvar on/off, or cycles through a set of values
	void CvarToggle( const CCommand &args );

	// Finds commands with a specified flag.
	void CvarFindFlags_f( const CCommand &args );

	// Enable cvars marked with FCVAR_DEVELOPMENTONLY
	void EnableDevCvars();

	int CvarFindFlagsCompletionCallback( const char *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] );


	void ResetConVarsToDefaultValues( const char *pMatchStr ); // reset all convars that start with pMatchStr to their default values

private:
	// just like Cvar_set, but optimizes out the search
	void SetDirect( ConVar *var, const char *value );

	bool IsValidToggleCommand( const char *cmd );
	bool IsHidden( const ConCommandBase	*var );
};

extern CCvarUtilities *ConVarUtilities;

// reset basic game convars to defaults. what is reset is specific to l4d
void ResetGameConVarsToDefaults( void );

#endif // CVAR_H
