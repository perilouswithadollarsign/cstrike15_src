//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef MM_TITLE_MAIN_H
#define MM_TITLE_MAIN_H
#ifdef _WIN32
#pragma once
#endif

extern InitReturnVal_t MM_Title_Init();
extern void MM_Title_Shutdown();

extern IMatchTitle *g_pIMatchTitle;
extern IMatchEventsSink *g_pIMatchTitleEventsSink;

extern IMatchTitleGameSettingsMgr *g_pIMatchTitleGameSettingsMgr;


//
// LINK_MATCHMAKING_LIB() macro must be included in the matchmaking.dll code
// to force all required matchmaking objects linked into the DLL.
//
extern void LinkMatchmakingLib();
#define LINK_MATCHMAKING_LIB() \
namespace { \
	static class CLinkMatchmakingLib { \
	public: \
		CLinkMatchmakingLib() { \
			LinkMatchmakingLib(); \
		} \
	} s_LinkHelper; \
};


#endif // MM_EVENTS_H
