//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//


#ifndef _LEADERBOARDS_H_
#define _LEADERBOARDS_H_

class ILeaderboardRequestQueue;
extern class ILeaderboardRequestQueue *g_pLeaderboardRequestQueue;

class ILeaderboardRequestQueue
{
public:
	virtual void Request( KeyValues *pRequest ) = 0;
	virtual void Update() = 0;
};

#endif // _LEADERBOARDS_H_
