//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//


#ifndef _GAME_H_
#define _GAME_H_

#include "mm_framework.h"
#include "matchmaking/igame.h"
#include "utlvector.h"

//In an idea world game is manageing all of the aspects of the game session, however in our case 
//it will serve as an interface into the engine and matchmaking to get the information we need.
class CMatchGame : public IMatchGame
{
public:
	CMatchGame();

	~CMatchGame();

	//IMatchGame

	virtual void Reset();

	virtual XNKID GetXNKID();

	virtual int GetNumPlayers();

	virtual IPlayer * GetPlayer(int index);

	virtual void AddPlayer( XUID xuid, int pExperience[ MATCH_MAX_DIFFICULTIES ] = NULL, int pSkills[ MATCH_MAX_DIFFICULTIES ][ MATCH_MAX_SKILL_FIELDS ] = NULL );

	virtual void RemovePlayer( XUID xuid );

	virtual void RemoveAllPlayers( );

	virtual int GetAggregateExperience( );

	virtual int GetAggregateSkill( int iSkillType );

private:

	typedef CUtlVector< IPlayer * > IPlayerList_t;
	IPlayerList_t m_PlayerList;
};

#endif