//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#if !defined( HOST_H )
#define HOST_H
#ifdef _WIN32
#pragma once
#endif

#include "convar.h"
#ifdef _PS3
#include "tls_ps3.h"
#endif


#define SCRIPT_DIR			"scripts/"

struct model_t;
struct AudioState_t;
class KeyValues;
class CMsg_CVars;

class CCommonHostState
{
public:
	CCommonHostState() : worldmodel( NULL ), worldbrush( NULL ), interval_per_tick( 0.0f ), max_splitscreen_players( 1 ), max_splitscreen_players_clientdll( 1 ) {}

	// cl_entitites[0].model
	model_t					*worldmodel;	
	struct worldbrushdata_t *worldbrush;
	// Tick interval for game
	float					interval_per_tick;		
	// 1, unless a game supports split screen, then probably 2 or 4 (4 is the max allowable)
	int						max_splitscreen_players; 
	// This is the # the client .dll thinks is the max, it might be > max_splitscreen_players in -tools mode, etc.
	int						max_splitscreen_players_clientdll;
	void					SetWorldModel( model_t *pModel );
};

extern CCommonHostState host_state;

//=============================================================================
// the host system specifies the base of the directory tree, the mod + base mod
// and the amount of memory available for the program to use
struct engineparms_t
{
	engineparms_t() : basedir( NULL ), mod( NULL ), game( NULL ), memsize( 0u ) {}

	char	*basedir;	// Executable directory ("c:/program files/half-life 2", for example)
	char	*mod;		// Mod name ("cstrike", for example)
	char	*game;		// Root game name ("hl2", for example, in the case of cstrike)
	unsigned int	memsize;
};
extern engineparms_t host_parms;


//-----------------------------------------------------------------------------
// Human readable methods to get at engineparms info
//-----------------------------------------------------------------------------
inline const char *GetCurrentMod()
{
	return host_parms.mod;
}

inline const char *GetCurrentGame()
{
	return host_parms.game;
}

inline const char *GetBaseDirectory()
{
	return host_parms.basedir;
}


//=============================================================================

//
// host
// FIXME, move all this crap somewhere else
//

// when we are using -fork, and we are the parent controlling process, we want to avoid a bunch of stuff, such as registering ourselves with the xlsp master or steam, etc
#define FORK_ID_PARENT_PROCESS -1
extern int g_nForkID;
extern int g_nSocketToParentProcess;						// -1 if we are not a child, otherwise its a socket that talks to the parent process

FORCEINLINE bool IsChildProcess( void )
{
	return g_nForkID > 0;
}
#ifdef _LINUX
void SendStringToParentProcess( char const *pMsg );
#endif



extern	ConVar		developer;
extern	bool		host_initialized;		// true if into command execution
extern	float		host_frametime;
extern  float		host_frametime_unbounded;
extern  float		host_frametime_unscaled;
extern  float		host_frametime_stddeviation;
extern  float		host_framestarttime_stddeviation;
extern	float		host_frameendtime_computationduration;
extern	int			host_framecount;	// incremented every frame, never reset
extern	double		realtime;			// not bounded in any way, changed at
// start of every frame, never reset
void Host_Error (PRINTF_FORMAT_STRING const char *error, ...) FMTFUNCTION( 1, 2 );
void Host_EndGame (bool bShowMainMenu, PRINTF_FORMAT_STRING const char *message, ...) FMTFUNCTION( 2, 3 );

// user message
#define MAX_USER_MSG_BITS 12
#define MAX_USER_MSG_DATA ( ( 1 << ( MAX_USER_MSG_BITS - 3 ) ) - 1 )

// build info
// day counter from Sep 30 2003
extern int build_number( void );


// Choke local client's/server's packets?
extern  ConVar		host_limitlocal;      
// Print a debug message when the client or server cache is missed
extern	ConVar		host_showcachemiss;

//#if !defined( LINUX )
extern bool			g_bInEditMode;
extern bool			g_bInCommentaryMode;
//#endif

extern bool			g_bLowViolence;
extern KeyValues*	g_pLaunchOptions;

// Returns true if host is not single stepping/pausing through code/
// FIXME:  Remove from final, retail version of code.
bool Host_ShouldRun( void );
void Host_FreeToLowMark( bool server );
void Host_FreeStateAndWorld( bool server );
void Host_Disconnect( bool bShowMainMenu );
void Host_RunFrame( float time );
void Host_DumpMemoryStats( void );
void Host_UpdateMapList( void );
float Host_GetSoundDuration( const char *pSample );
bool Host_IsSinglePlayerGame( void );
bool Host_IsLocalServer();
int Host_GetServerCount( void );
bool Host_AllowQueuedMaterialSystem( bool bAllow );
void Host_EnsureHostNameSet();
void Host_BeginThreadedSound();

// Force the voice stuff to send a packet out.
// bFinal is true when the user is done talking.
void CL_SendVoicePacket(bool bFinal);

bool Host_IsSecureServerAllowed();
void Host_DisallowSecureServers();
bool Host_AllowLoadModule( const char *pFilename, const char *pPathID, bool bAllowUnknown );

// Accumulated filtered time on machine ( i.e., host_framerate can alter host_time )
extern float host_time;

struct NetMessageCvar_t;

void Host_BuildConVarUpdateMessage( CMsg_CVars *rCvarList, int flags, bool nonDefault );
void		Host_BuildUserInfoUpdateMessage( int nSplitScreenSlot, CMsg_CVars *rCvarList, bool nonDefault );
char const *Host_CleanupConVarStringValue( char const *invalue );
void		Host_SetAudioState( const AudioState_t &audioState );

bool CheckVarRange_Generic( ConVar *pVar, int minVal, int maxVal );

// Total ticks run
extern int	host_tickcount;
// Number of ticks being run this frame
extern int	host_frameticks;
// Which tick are we currently on for this frame
extern int	host_currentframetick;

// PERFORMANCE INFO
#define MIN_FPS         0.1         // Host minimum fps value for maxfps.
#define MAX_FPS         1000.0        // Upper limit for maxfps.

#define MAX_FRAMETIME	0.1
#define MIN_FRAMETIME	0.001
#define MAX_TOOLS_FRAMETIME	2.0
#define MIN_TOOLS_FRAMETIME	0.001

#define TIME_TO_TICKS( dt )		( (int)( 0.5f + (float)(dt) / host_state.interval_per_tick ) )
#define TICKS_TO_TIME( dt )		( host_state.interval_per_tick * (float)(dt) )

// Set by the game DLL to tell us to do the same timing tricks as timedemo.
extern bool g_bDedicatedServerBenchmarkMode;
extern uint GetSteamAppID();

#include "steam/steamclientpublic.h"
extern EUniverse GetSteamUniverse();

// 
// \return true iff PS3 (and only PS3) and Quitting (on user request or disk eject or other critical condition when we absolutely must quit within 10 seconds)
//

// 
// \return true iff PS3 (and only PS3) and Quitting (on user request or disk eject or other critical condition when we absolutely must quit within 10 seconds)
//
inline bool IsPS3QuitRequested()
{
#ifdef _PS3
	return GetTLSGlobals()->bNormalQuitRequested;
#else
	// if not on PS3, do not disturb the old logic of host_state which has a lot of dependencies, because other platforms do not require the game to quit cleanly
	return false; 
#endif
}



#endif // HOST_H

