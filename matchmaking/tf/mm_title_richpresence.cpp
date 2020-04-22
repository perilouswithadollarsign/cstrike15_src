//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_title_richpresence.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static void SetAllUsersContext( DWORD dwContextId, DWORD dwValue, bool bAsync = true )
{
#ifdef _X360
	for ( int k = 0; k < ( int ) XBX_GetNumGameUsers(); ++ k )
	{
		if ( XBX_GetUserIsGuest( k ) )
			continue;
		int iCtrlr = XBX_GetUserId( k );
		if ( bAsync )
			XUserSetContextEx( iCtrlr, dwContextId, dwValue, MMX360_NewOverlappedDormant() );
		else
			XUserSetContext( iCtrlr, dwContextId, dwValue );
	}
#endif
}

static void SetAllUsersProperty( DWORD dwPropertyId, DWORD cbValue, void const *pvValue )
{
#ifdef _X360
	for ( int k = 0; k < ( int ) XBX_GetNumGameUsers(); ++ k )
	{
		if ( XBX_GetUserIsGuest( k ) )
			continue;
		int iCtrlr = XBX_GetUserId( k );
		XUserSetPropertyEx( iCtrlr, dwPropertyId, cbValue, pvValue, MMX360_NewOverlappedDormant() );
	}
#endif
}

KeyValues * MM_Title_RichPresence_PrepareForSessionCreate( KeyValues *pSettings )
{
	return NULL;
}

void MM_Title_RichPresence_Update( KeyValues *pFullSettings, KeyValues *pUpdatedSettings )
{	
}

void MM_Title_RichPresence_PlayersChanged( KeyValues *pFullSettings )
{
}
