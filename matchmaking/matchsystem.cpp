//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "mm_framework.h"

#ifdef _X360
#include "xonline.h"
#else
#include "xbox/xboxstubs.h"
#endif

#include "matchsystem.h"
#include "playermanager.h"
#include "servermanager.h"
#include "datacenter.h"

#ifndef SWDS
#include "searchmanager.h"
#include "leaderboards.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


static CMatchSystem s_MatchSystem;
CMatchSystem *g_pMatchSystem = &s_MatchSystem;

bool IsValidXNKID( XNKID xnkid )
{
	for(int i = 0; i < 8; ++i)
	{
		if(xnkid.ab[i])
			return true;
	}
	return false;
}

CMatchSystem::CMatchSystem()
{
}

CMatchSystem::~CMatchSystem()
{
}

IPlayerManager * CMatchSystem::GetPlayerManager()
{
	return g_pPlayerManager;
}

IMatchVoice * CMatchSystem::GetMatchVoice()
{
	return g_pMatchVoice;
}


IServerManager * CMatchSystem::GetUserGroupsServerManager()
{
	return g_pServerManager;
}

ISearchManager * CMatchSystem::CreateGameSearchManager( KeyValues *pParams )
{
#ifndef SWDS
	return new CSearchManager( pParams );
#else
	return NULL;
#endif
}

IDatacenter * CMatchSystem::GetDatacenter()
{
	return g_pDatacenter;
}

IDlcManager * CMatchSystem::GetDlcManager()
{
	return g_pDlcManager;
}

void CMatchSystem::Update()
{
	if ( g_pPlayerManager )
		g_pPlayerManager->Update();

	if ( g_pServerManager )
		g_pServerManager->Update();

#ifndef SWDS
	CSearchManager::UpdateAll();

	if ( g_pLeaderboardRequestQueue )
		g_pLeaderboardRequestQueue->Update();
#endif

	if ( g_pDatacenter )
		g_pDatacenter->Update();

	if ( g_pDlcManager )
		g_pDlcManager->Update();
}

