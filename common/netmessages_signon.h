//========= Copyright © Valve Corporation, All rights reserved. ============//
#ifndef NETMESSAGES_SIGNON_HDR
#define NETMESSAGES_SIGNON_HDR
// When we integarte Yahn's CL 1791453 from dota, this will go back to network_connection.proto 
enum SIGNONSTATE
{
	SIGNONSTATE_NONE		= 0,	// no state yet; about to connect
	SIGNONSTATE_CHALLENGE	= 1,	// client challenging server; all OOB packets
	SIGNONSTATE_CONNECTED	= 2,	// client is connected to server; netchans ready
	SIGNONSTATE_NEW			= 3,	// just got serverinfo and string tables
	SIGNONSTATE_PRESPAWN	= 4,	// received signon buffers
	SIGNONSTATE_SPAWN		= 5,	// ready to receive entity packets
	SIGNONSTATE_FULL		= 6,	// we are fully connected; first non-delta packet received
	SIGNONSTATE_CHANGELEVEL	= 7,	// server is changing level; please wait
};

#endif // NETMESSAGES_SIGNON_HDR
