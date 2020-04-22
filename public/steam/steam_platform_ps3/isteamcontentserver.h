//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: interface to steam for game servers
//
//=============================================================================

#ifndef ISTEAMCONTENTSERVER_H
#define ISTEAMCONTENTSERVER_H
#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Purpose: Functions for authenticating users via Steam to download content
//-----------------------------------------------------------------------------
class ISteamContentServer
{
public:
	// connection functions
	virtual void LogOn( uint32 uContentServerID ) = 0;
	virtual void LogOff() = 0;
	virtual bool BLoggedOn() = 0;

	// user authentication functions
	virtual bool SendClientContentAuthRequest( CSteamID steamID, uint32 uContentID, uint64 ulSessionToken, bool bTokenPresent ) = 0;

	// Check a Steam3-created content ticket. Does not need to connect to Steam3 to validate it,
	// so this is a backup for when the above call is not available (disconnected).
	virtual bool BCheckTicket( CSteamID steamID, uint32 uContentID, const void *pvTicketData, uint32 cubTicketLength ) = 0;
	
	// some sort of status stuff here eventually
	
};

#define STEAMCONTENTSERVER_INTERFACE_VERSION "SteamContentServer002"


// callbacks
#pragma pack( push, 8 )

// client has been approved to download the content
struct CSClientApprove_t
{
	enum { k_iCallback = k_iSteamContentServerCallbacks + 1 };
	CSteamID m_SteamID;
	uint32 m_uContentID;
};


// client has been denied to connection to this game server
struct CSClientDeny_t
{
	enum { k_iCallback = k_iSteamContentServerCallbacks + 2 };
	CSteamID m_SteamID;
	uint32 m_uContentID;
	EDenyReason m_eDenyReason;
};

#pragma pack( pop )

#endif // ISTEAMGAMESERVER_H
