//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#if !defined( INETMSGHANDLER_H )
#define INETMSGHANDLER_H
#ifdef _WIN32
#pragma once
#endif

#include "inetmessage.h"

class	INetChannel;
typedef struct netpacket_s netpacket_t;

class IConnectionlessPacketHandler
{
public:
	virtual	~IConnectionlessPacketHandler( void ) {};

	virtual bool ProcessConnectionlessPacket( netpacket_t *packet ) = 0;	// process a connectionless packet
};


#endif // INETMSGHANDLER_H
