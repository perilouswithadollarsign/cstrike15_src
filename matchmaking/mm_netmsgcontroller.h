//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef MM_NETMSGCONTROLLER_H
#define MM_NETMSGCONTROLLER_H
#ifdef _WIN32
#pragma once
#endif

#include "mm_framework.h"

class CMatchNetworkMsgControllerBase : public IMatchNetworkMsgController
{
	// Methods of IMatchNetworkMsgController
public:
	// To determine host Quality-of-Service
	virtual MM_QOS_t GetQOS();

	virtual KeyValues * GetActiveServerGameDetails( KeyValues *pRequest );

	virtual KeyValues * UnpackGameDetailsFromQOS( MM_GameDetails_QOS_t const *pvQosReply );
	virtual KeyValues * UnpackGameDetailsFromSteamLobby( uint64 uiLobbyID );

	virtual void PackageGameDetailsForQOS( KeyValues *pSettings, CUtlBuffer &buf );

	virtual KeyValues * PackageGameDetailsForReservation( KeyValues *pSettings );

public:
	CMatchNetworkMsgControllerBase();
	~CMatchNetworkMsgControllerBase();
};

// Match title singleton
extern CMatchNetworkMsgControllerBase *g_pMatchNetMsgControllerBase;

#endif // MM_NETMSGCONTROLLER_H
