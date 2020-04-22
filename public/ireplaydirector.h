//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef IREPLAYDIRECTOR_H
#define IREPLAYDIRECTOR_H
#ifdef _WIN32
#pragma once
#endif

class IReplayServer;
class KeyValues;
class Vector;

#define INTERFACEVERSION_REPLAYDIRECTOR			"ReplayDirector001"

class IReplayDirector
{
public:
	virtual	~IReplayDirector() {}

	virtual bool	IsActive( void ) = 0; // true if director is active

	virtual void	SetReplayServer( IReplayServer *Replay ) = 0; // give the director the engine Replay interface 
	virtual IReplayServer* GetReplayServer( void ) = 0; // get current Replay server interface
	
	virtual int		GetDirectorTick( void ) = 0;	// get current broadcast tick from director
	virtual int		GetPVSEntity( void ) = 0; // get current view entity (PVS), 0 if coords are used
	virtual Vector	GetPVSOrigin( void ) = 0; // get current PVS origin
	virtual float	GetDelay( void ) = 0; // returns current delay in seconds

	virtual const char**	GetModEvents() = 0;
};

#endif // IREPLAYDIRECTOR_H
