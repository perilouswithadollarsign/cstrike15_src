#include "CegClientWrapper.h"

#if defined( CLIENT_DLL ) || defined( GAME_DLL )	// shared
#include "cbase.h"									// prerequisite for more than one of the below headers
#include "bspflags.h"								// defines SURF_NOPORTAL and SURF_NOPAINT

#if defined ( CSTRIKE15 )
	
#endif // defined CSTRIKE15

#endif // defined CLIENT_DLL or GAME_DLL

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#if defined( PROFILE_CEG )

CAverageCycleCounter	allCEG;
CAverageCycleCounter	allTestSecret;
CAverageCycleCounter	allSelfCheck;
CAverageCycleCounter	allProtectMember;
CAverageCycleCounter	allProtectVirtual;

#endif // defined( PROFILE_CEG )

#if defined( CLIENT_DLL )			// client-side only

void Init_GCVs()
{
	// function calls to prime statics and ensure CEG init hitches happen before game is interactive
	InitHudAllowTextChatFlag();
	InitUiAllowProperTintFlag();	
}

CEG_DEFINE_CONSTANT_FUNCTION( HudAllowTextChatFlag,		ALLOW_TEXTCHAT_FLAG );
CEG_DEFINE_CONSTANT_FUNCTION( UiAllowProperTintFlag,	ALLOW_PROPER_TINT_FLAG );


#elif defined( GAME_DLL )			// server-side only

void Init_GCVs()
{

}

#endif // defined( GAME_DLL )