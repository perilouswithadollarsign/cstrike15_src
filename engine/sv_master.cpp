//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "server_pch.h"
#include "server.h"
#include "master.h"
#include "proto_oob.h"
#include "sv_main.h" // SV_GetFakeClientCount()
#include "tier0/icommandline.h"
#include "FindSteamServers.h"
#include "filesystem_engine.h"
#include "sv_steamauth.h"
#include "hltvserver.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

bool g_bEnableMasterServerUpdater = true;

static void SvSearchKeyChangeCallback( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	sv.UpdateGameData();
	if ( sv.IsActive() )
	{
		Cbuf_AddText( CBUF_SERVER, "heartbeat\n" );
	}
}

ConVar sv_search_key( "sv_search_key",
#if defined( _PS3 )
						"csgo-ps3-rc0",
#elif defined( SERVER_XLSP ) || defined( _X360 )
						"csgo-x360-rc1",
#else
						"",
#endif
						  FCVAR_RELEASE, "When searching for a dedicated server from lobby, restrict search to only dedicated servers having the same sv_search_key.", SvSearchKeyChangeCallback );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void Heartbeat_f()
{
	if ( g_nForkID != FORK_ID_PARENT_PROCESS )								// if we are the process parent, we won't advertise ourself to steam / xlsp
	{
		if( Steam3Server().SteamGameServer() )
		{
			Steam3Server().SteamGameServer()->ForceHeartbeat();
		}
	}
}


static ConCommand heartbeat( "heartbeat", Heartbeat_f, "Force heartbeat of master servers", 0 );
