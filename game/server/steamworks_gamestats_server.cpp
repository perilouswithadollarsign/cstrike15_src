//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Uploads KeyValue stats to the new SteamWorks gamestats system.
//
//=============================================================================

#include "cbase.h"
#include "cdll_int.h"
#include "tier2/tier2.h"
#include <time.h>

#include "gameinterface.h"
#include "steam/isteamutils.h"
#include "steamworks_gamestats_server.h"
#include "icommandline.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

static CSteamWorksGameStatsServer g_SteamWorksGameStatsServer;

extern ConVar developer;

//-----------------------------------------------------------------------------
// Purpose: Returns a reference to the global object
//-----------------------------------------------------------------------------
CSteamWorksGameStatsServer& GetSteamWorksGameStatsServer()
{
	return g_SteamWorksGameStatsServer;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor. Sets up the steam callbacks accordingly depending on client/server dll 
//-----------------------------------------------------------------------------
CSteamWorksGameStatsServer::CSteamWorksGameStatsServer() : BaseClass( "CSteamWorksGameStatsServer", "steamworks_sessionid_server" )
{
	Reset();
}

void CSteamWorksGameStatsServer::AddSessionIDsToTable( int iTableID )
{
	// The session of this server.
	WriteInt64ToTable( m_SessionID, iTableID, "ServerSessionID" );
}


//-----------------------------------------------------------------------------
// Purpose: Uploads any end of session rows.
//-----------------------------------------------------------------------------
void CSteamWorksGameStatsServer::WriteSessionRow()
{
	m_SteamWorksInterface = GetInterface();
	if ( !m_SteamWorksInterface )
		return;

	m_SteamWorksInterface->AddSessionAttributeInt64( m_SessionID, "ServerSessionID", m_SessionID );
	m_SteamWorksInterface->AddSessionAttributeString( m_SessionID, "ServerIP", m_pzServerIP );
	m_SteamWorksInterface->AddSessionAttributeString( m_SessionID, "ServerName", m_pzHostName );
	m_SteamWorksInterface->AddSessionAttributeString( m_SessionID, "StartMap", m_pzMapStart );

	BaseClass::WriteSessionRow();
}


//-----------------------------------------------------------

EGameStatsAccountType CSteamWorksGameStatsServer::GetGameStatsAccountType()
{
	return engine->IsDedicatedServer() ? k_EGameStatsAccountType_SteamGameServer : k_EGameStatsAccountType_Steam;
}