//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//


#include "tier0/dbg.h"
#include "tier0/icommandline.h"

#include "tier1/strtools.h"
#include "tier1/checksum_crc.h"
#include "tier1/keyvalues.h"
#include "tier1/utlbuffer.h"

#include "tier2/tier2.h"

#include "mathlib/mathlib.h"

#include "const.h"

#include "inetmsghandler.h"
#include "appframework/iappsystemgroup.h"
#include "matchmaking/imatchframework.h"

#include <memdbgon.h>


#ifdef _GAMECONSOLE

class CMoviePlayer_IMatchEventsSubscription : public IMatchEventsSubscription
{
	virtual void Subscribe( IMatchEventsSink *pSink ) {}
	virtual void Unsubscribe( IMatchEventsSink *pSink ) {}

	virtual void BroadcastEvent( KeyValues *pEvent ) { pEvent->deleteThis(); }

	virtual void RegisterEventData( KeyValues *pEventData ) {}
	virtual KeyValues * GetEventData( char const *szEventDataKey ) { return NULL; }
}
g_CMoviePlayer_IMatchEventsSubscription;

class CMoviePlayer_MatchFramework : public CTier2AppSystem< IMatchFramework >
{
public:
	// Run frame of the matchmaking framework
	virtual void RunFrame() {}


	// Get matchmaking extensions
	virtual IMatchExtensions * GetMatchExtensions() { return NULL; }

	// Get events container
	virtual IMatchEventsSubscription * GetEventsSubscription() { return &g_CMoviePlayer_IMatchEventsSubscription; }

	// Get the matchmaking title interface
	virtual IMatchTitle * GetMatchTitle() { return NULL; }

	// Get the match session interface of the current match framework type
	virtual IMatchSession * GetMatchSession() { return NULL; }

	// Get the network msg encode/decode factory
	virtual IMatchNetworkMsgController * GetMatchNetworkMsgController() { return NULL; }

	// Get the match system
	virtual IMatchSystem * GetMatchSystem() { return NULL; }
	
	// Send the key values back to the server
	virtual void ApplySettings( KeyValues *keyValues) {}

	// Entry point to create session
	virtual void CreateSession( KeyValues *pSettings ) {}

	// Entry point to match into a session
	virtual void MatchSession( KeyValues *pSettings ) {}

	// Accept invite
	virtual void AcceptInvite( int iController );

	// Close the session
	virtual void CloseSession() {}

	virtual bool IsOnlineGame( void ) { return false; }

	// Called by the client to notify matchmaking that it should update matchmaking properties based
	// on player distribution among the teams.
	virtual void UpdateTeamProperties( KeyValues *pTeamProperties ) {}


	// IAppSystem
public:
	// Here's where the app systems get to learn about each other 
	virtual bool Connect( CreateInterfaceFn factory ) { return true; }
	virtual void Disconnect() {}

	// Here's where systems can access other interfaces implemented by this object
	// Returns NULL if it doesn't implement the requested interface
	virtual void *QueryInterface( const char *pInterfaceName ) { return NULL; }

	// Init, shutdown
	virtual InitReturnVal_t Init() { return INIT_OK; }
	virtual void Shutdown() {}
}
g_CMoviePlayer_MatchFramework;
IMatchFramework *g_pMoviePlayer_MatchFramework = &g_CMoviePlayer_MatchFramework;

void CMoviePlayer_MatchFramework::AcceptInvite( int iController )
{
#ifdef _X360
	// Collect our session data
	XINVITE_INFO inviteInfo = {0};
	DWORD dwError = XInviteGetAcceptedInfo( iController, &inviteInfo );
	if ( dwError != ERROR_SUCCESS )
	{
		return;
	}

	DWORD dwTitleIDrequired = 0;

	dwTitleIDrequired = 0x45410912; // PORTAL2 RETAIL ID

	if ( !dwTitleIDrequired )
		Error( "TitleID is not defined for game invite during movie!\n" );

	if ( dwTitleIDrequired && inviteInfo.dwTitleID != dwTitleIDrequired )
		return;

	// Store off the session ID and mark the invite as accepted internally
	XBX_SetInvitedUserId( iController );
	XBX_SetInvitedUserXuid( inviteInfo.xuidInvitee );
	XBX_SetInviteSessionId( inviteInfo.hostInfo.sessionID );
#endif
}

#endif

