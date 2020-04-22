//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef CLIENTFRAME_H
#define CLIENTFRAME_H
#ifdef _WIN32
#pragma once
#endif

#include <bitvec.h>
#include <const.h>
#include <tier1/mempool.h>

class CFrameSnapshot;

#define MAX_CLIENT_FRAMES	128

class CClientFrame
{
public:

	CClientFrame( void );
	CClientFrame( int tickcount );
	CClientFrame( CFrameSnapshot *pSnapshot );
	virtual ~CClientFrame();
	void Init( CFrameSnapshot *pSnapshot );
	void Init( int tickcount );

	// Accessors to snapshots. The data is protected because the snapshots are reference-counted.
	inline CFrameSnapshot*	GetSnapshot() const { return m_pSnapshot; };
	void					SetSnapshot( CFrameSnapshot *pSnapshot );
	void					CopyFrame( CClientFrame &frame );
	virtual bool		IsMemPoolAllocated() { return true; }

public:

	// State of entities this frame from the POV of the client.
	int					last_entity;	// highest entity index
	int					tick_count;	// server tick of this snapshot
	CClientFrame*		m_pNext;

	// Used by server to indicate if the entity was in the player's pvs
	CBitVec<MAX_EDICTS>	transmit_entity; // if bit n is set, entity n will be send to client
	CBitVec<MAX_EDICTS>	*from_baseline;	// if bit n is set, this entity was send as update from baseline
	CBitVec<MAX_EDICTS>	*transmit_always; // if bit is set, don't do PVS checks before sending (HLTV only)

private:

	// Index of snapshot entry that stores the entities that were active and the serial numbers
	// for the frame number this packed entity corresponds to
	// m_pSnapshot MUST be private to force using SetSnapshot(), see reference counters
	CFrameSnapshot		*m_pSnapshot;
};

// TODO substitute CClientFrameManager with an intelligent structure (Tree, hash, cache, etc)
class CClientFrameManager
{
public:
	CClientFrameManager(void)  : m_ClientFramePool( MAX_CLIENT_FRAMES, CUtlMemoryPool::GROW_SLOW ) {	m_Frames = NULL; }
	virtual ~CClientFrameManager(void) { DeleteClientFrames(-1); }

	int				AddClientFrame( CClientFrame *pFrame ); // returns current count
	CClientFrame	*GetClientFrame( int nTick, bool bExact = true );
	void			DeleteClientFrames( int nTick );	// -1 for all
	int				CountClientFrames( void );	// returns number of frames in list
	void			RemoveOldestFrame( void );  // removes the oldest frame in list
	bool			DeleteUnusedClientFrame( CClientFrame* pFrame );
	int				GetOldestTick()const{ return m_Frames ? m_Frames->tick_count : 0; }

	CClientFrame*	AllocateFrame();
	CClientFrame*	AllocateAndInitFrame( int nTick );

private:
	void			FreeFrame( CClientFrame* pFrame );

	CClientFrame	*m_Frames;		// updates can be delta'ed from here
	CClassMemoryPool< CClientFrame >	m_ClientFramePool;
};

#endif // CLIENTFRAME_H
