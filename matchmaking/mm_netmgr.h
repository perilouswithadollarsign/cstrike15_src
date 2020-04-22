//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef MM_NETMGR_H
#define MM_NETMGR_H
#ifdef _WIN32
#pragma once
#endif

class CConnectionlessLanMgr;

#include "mm_framework.h"

class CConnectionlessLanMgr : public IConnectionlessPacketHandler
{
	//
	// IConnectionlessPacketHandler
	//
public:
	virtual bool ProcessConnectionlessPacket( netpacket_t *packet );

public:
	void Update();
	void SendPacket( KeyValues *msg, char const *szAddress = NULL, INetSupport::NetworkSocket_t eSock
#ifdef _X360
		= INetSupport::NS_SOCK_SYSTEMLINK
#else
		= INetSupport::NS_SOCK_CLIENT
#endif
		);
	KeyValues * UnpackPacket( netpacket_t *packet );

public:
	CConnectionlessLanMgr();
	~CConnectionlessLanMgr();

protected:
	CUtlBuffer m_buffer;
};

// Match events subscription singleton
extern CConnectionlessLanMgr *g_pConnectionlessLanMgr;

#endif // MM_EVENTS_H
