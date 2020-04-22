//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef IREPLAY_H
#define IREPLAY_H

#ifdef _WIN32
#pragma once
#endif

#include "interface.h"

class IServer;
class IReplayDirector;
class IGameEvent;
struct netadr_s;

//-----------------------------------------------------------------------------
// Interface the Replay module exposes to the engine
//-----------------------------------------------------------------------------
#define INTERFACEVERSION_REPLAYSERVER	"ReplayServer001"

class IReplayServer : public IBaseInterface
{
public:
	virtual	~IReplayServer() {}

	virtual	IServer	*GetBaseServer( void ) = 0; // get Replay base server interface
	virtual	IReplayDirector *GetDirector( void ) = 0;	// get director interface
	virtual	int		GetReplaySlot( void ) = 0; // return entity index-1 of Replay in game
	virtual float	GetOnlineTime( void ) = 0; // seconds since broadcast started

	virtual void	BroadcastEvent(IGameEvent *event) = 0; // send a director command to all specs
};

#endif
