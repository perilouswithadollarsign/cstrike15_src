#ifndef FUNFACTMGR_H
#define FUNFACTMGR_H
#ifdef _WIN32
#pragma once
#endif

#include "GameEventListener.h"
#include "funfact_cs.h"
#include "utlmap.h"
#include "cs_shareddefs.h"

class FunFactEvaluator;

class CCSFunFactMgr : public CAutoGameSystemPerFrame, public CGameEventListener
{
public:
	CCSFunFactMgr();
	~CCSFunFactMgr();

	virtual bool Init();
	virtual void Shutdown();
	virtual void Update( float frametime );

	bool GetRoundEndFunFact( int iWinningTeam, e_RoundEndReason iRoundResult, FunFact& funfact );

protected:
	float ScoreFunFact( const FunFact& funfact );
	void FireGameEvent( IGameEvent *event );

private:

	// Weights for all players. Updated every round
	// index 0 is for "all players" funfacts, and has the same cooldown behavior as for individual players
	float m_playerCooldown[MAX_PLAYERS + 1];	

	struct FunFactDatabaseEntry
	{
		const FunFactEvaluator* pEvaluator;
		int iOccurrences;
		float fCooldown;
	};
	CUtlMap<int, FunFactDatabaseEntry> m_funFactDatabase;
	int m_numRounds;
};

#endif

