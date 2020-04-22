//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "bitbuf.h"
#include "bitbuf_errorhandler.h"
#include "tier0/dbg.h"
#include "tier0/threadtools.h"
#include "utlsymbol.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CUtlSymbolTable g_ErrorNames[BITBUFERROR_NUM_ERRORS];

// This is needed to make bf_write / bf_read thread safe;
// this error case is expected to happen rarely.
static CThreadRWLock g_ErrorNamesLock;

void EngineBitBufErrorHandler( BitBufErrorType errorType, const char *pDebugName )
{
	if ( !pDebugName )
	{
		pDebugName = "(unknown)";
	}

	// Only print an error a couple times.
	g_ErrorNamesLock.LockForRead();
	CUtlSymbol sym = g_ErrorNames[ errorType ].Find( pDebugName );
	g_ErrorNamesLock.UnlockRead();

	if ( UTL_INVAL_SYMBOL == sym )
	{
		g_ErrorNamesLock.LockForWrite();
		g_ErrorNames[ errorType ].AddString( pDebugName );
		g_ErrorNamesLock.UnlockWrite();

		if ( errorType == BITBUFERROR_VALUE_OUT_OF_RANGE )
		{
			Warning( "Error in bitbuf [%s]: out of range value. Debug in bitbuf_errorhandler.cpp\n", pDebugName );
		}
		else if ( errorType == BITBUFERROR_BUFFER_OVERRUN )
		{
			Warning( "Error in bitbuf [%s]: buffer overrun. Debug in bitbuf_errorhandler.cpp\n", pDebugName );
		}
	}

	Assert( 0 );
}


void InstallBitBufErrorHandler()
{
	SetBitBufErrorHandler( EngineBitBufErrorHandler );
}






