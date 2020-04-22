//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef _MATCHSYSTEM_H_
#define _MATCHSYSTEM_H_

#include "mm_framework.h"

#include "utlvector.h"

enum PacketTargetType
{
	PTT_PLAYER = 0,
	PTT_GAME,
};

bool IsValidXNKID( XNKID xnkid );

class CMatchSystem : public IMatchSystem
{
public:
	CMatchSystem();
	~CMatchSystem();

public:
	void Update();

	//IMatchSystem
public:
	virtual IPlayerManager * GetPlayerManager();

	virtual IMatchVoice * GetMatchVoice();

	virtual IServerManager * GetUserGroupsServerManager();

	virtual ISearchManager * CreateGameSearchManager( KeyValues *pParams );

	virtual IDatacenter * GetDatacenter();

	virtual IDlcManager * GetDlcManager();
};

extern CMatchSystem *g_pMatchSystem;

#endif

