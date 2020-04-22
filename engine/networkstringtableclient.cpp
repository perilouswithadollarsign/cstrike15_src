//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "quakedef.h"
#include "networkstringtable.h"
#include "client.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifndef SHARED_NET_STRING_TABLES
 	static CNetworkStringTableContainer s_NetworkStringTableClient;
	CNetworkStringTableContainer *networkStringTableContainerClient = &s_NetworkStringTableClient;

	// Expose to client .dll
	EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CNetworkStringTableContainer, INetworkStringTableContainer, INTERFACENAME_NETWORKSTRINGTABLECLIENT, s_NetworkStringTableClient );
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CL_PrintStringTables( void )
{
#ifndef DEDICATED
	if ( GetBaseLocalClient().m_StringTableContainer )
	{
		GetBaseLocalClient().m_StringTableContainer->Dump();
	}
#endif
}

