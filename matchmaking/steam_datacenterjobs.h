//========= Copyright © Valve Corporation, All rights reserved. =======================//
//
// Purpose: Jobs for communicating with the custom Steam backend (Game Coordinator)
//
//=====================================================================================//

#ifndef MATCHMAKING_STEAM_DATACENTER_JOBS_H
#define MATCHMAKING_STEAM_DATACENTER_JOBS_H

#if !defined( NO_STEAM ) && !defined( NO_STEAM_GAMECOORDINATOR )

#include "gcsdk/gcclientsdk.h"


//-----------------------------------------------------------------------------
// Purpose: Sends an update of title-global stats to the GC
//-----------------------------------------------------------------------------
class CGCClientJobUpdateStats : public GCSDK::CGCClientJob
{
public:
	CGCClientJobUpdateStats( KeyValues *pKVStats );
	~CGCClientJobUpdateStats();
	virtual bool BYieldingRunGCJob();

private:
	KeyValues *m_pKVCmd;
};


//-----------------------------------------------------------------------------
// Purpose: Retrieves the global state from the GC
//-----------------------------------------------------------------------------
class CGCClientJobDataRequest : public GCSDK::CGCClientJob
{
public:
	CGCClientJobDataRequest( );
	~CGCClientJobDataRequest( );

	virtual bool BYieldingRunGCJob();

	bool		BComplete()	const	{ return m_bComplete; }
	bool		BSuccess()	const	{ return m_bSuccess; }
	KeyValues	*GetResults()		{ return m_pKVResults; }
	void		Finish()			{ m_bWaitForRead = false; }

private:
	KeyValues   *m_pKVRequest;
	KeyValues   *m_pKVResults;
	bool		m_bComplete;
	bool		m_bSuccess;
	bool		m_bWaitForRead;
};

#endif

#endif
