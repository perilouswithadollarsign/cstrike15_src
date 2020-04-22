//======= Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ======
//
//	Forwards game events to remote IP address. This is to facilitate the recording
//	of the play tests and to be able to index into the recorded video based on events.
//
//===============================================================================


#ifndef GAMEEVENTTRANSMITTER_H
#define GAMEEVENTTRANSMITTER_H
#ifdef _WIN32
#pragma once
#endif

#include "igameevents.h"
#include "tier1/netadr.h"

class CGameEventTransmitter
{
public:
	CGameEventTransmitter();
	~CGameEventTransmitter();
	
	bool	Init();
	void	TransmitGameEvent( IGameEvent *event );
	bool	SetIPAndPort( const char *address );
	
private:

	netadr_t m_Adr;

};

extern CGameEventTransmitter &g_GameEventTransmitter;

#endif // GAMEEVENTTRANSMITTER_H
