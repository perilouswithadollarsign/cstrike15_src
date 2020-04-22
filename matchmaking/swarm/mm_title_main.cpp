//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_title.h"
#include "../mm_title_main.h"

#include "matchext_swarm.h"

#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


LINK_MATCHMAKING_LIB();


static CMatchTitle g_MatchTitle;
CMatchTitle *g_pMatchTitle = &g_MatchTitle;

IMatchTitle *g_pIMatchTitle = g_pMatchTitle;
IMatchEventsSink *g_pIMatchTitleEventsSink = g_pMatchTitle;



//
// Init / shutdown
//

InitReturnVal_t MM_Title_Init()
{
	return g_pMatchTitle->Init();
}

void MM_Title_Shutdown()
{
	if ( g_pMatchTitle )
		g_pMatchTitle->Shutdown();
}

