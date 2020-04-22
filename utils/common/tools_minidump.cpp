//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <windows.h>
#include "tier0/minidump.h"
#include "tools_minidump.h"


static bool g_bToolsWriteFullMinidumps = false;
static ToolsExceptionHandler g_pCustomExceptionHandler = NULL;


// --------------------------------------------------------------------------------- //
// Internal helpers.
// --------------------------------------------------------------------------------- //

static void ToolsExceptionFilter( uint uStructuredExceptionCode, ExceptionInfo_t * pExceptionInfo, const char *pszFilenameSuffix )
{
	// Non VMPI workers write a minidump and show a crash dialog like normal.
	uint32 iType = MINIDUMP_Normal;
	if ( g_bToolsWriteFullMinidumps )
	{
		iType |= MINIDUMP_WithDataSegs | MINIDUMP_WithIndirectlyReferencedMemory;
	}
		
	WriteMiniDumpUsingExceptionInfo( uStructuredExceptionCode, pExceptionInfo, iType, pszFilenameSuffix );
}


static void ToolsExceptionFilter_Custom( uint uStructuredExceptionCode, ExceptionInfo_t * pExceptionInfo, const char *pszFilenameSuffix )
{
	// Run their custom handler.
	g_pCustomExceptionHandler( uStructuredExceptionCode, pExceptionInfo );
}


// --------------------------------------------------------------------------------- //
// Interface functions.
// --------------------------------------------------------------------------------- //

void EnableFullMinidumps( bool bFull )
{
	g_bToolsWriteFullMinidumps = bFull;
}


void SetupDefaultToolsMinidumpHandler()
{
	MinidumpSetUnhandledExceptionFunction( ToolsExceptionFilter );
}


void SetupToolsMinidumpHandler( ToolsExceptionHandler fn )
{
	g_pCustomExceptionHandler = fn;
	MinidumpSetUnhandledExceptionFunction( ToolsExceptionFilter_Custom );
}
