//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef MM_FRAMEWORK_H
#define MM_FRAMEWORK_H
#ifdef _WIN32
#pragma once
#endif


#include "tier0/dbg.h"
#include "tier0/icommandline.h"

#include "tier1/strtools.h"
#include "tier1/checksum_crc.h"
#include "tier1/keyvalues.h"
#include "tier1/utlbuffer.h"
#include "tier1/fmtstr.h"

#include "mathlib/mathlib.h"

#include "const.h"

#include "inetmsghandler.h"
#include "appframework/IAppSystemGroup.h"
#include "matchmaking/imatchframework.h"
#include "igameevents.h"

#include "tier2/tier2.h"
#include "vstdlib/jobthread.h"

#include "extkeyvalues.h"
#include "steam_apihook.h"


class CMatchFramework;


#include "mm_extensions.h"
#include "mm_events.h"
#include "mm_voice.h"
#include "mm_session.h"
#include "mm_netmgr.h"

#include "matchsystem.h"
#include "playermanager.h"
#include "servermanager.h"
#include "searchmanager.h"
#include "datacenter.h"
#include "mm_dlc.h"

enum MatchFrameworkInviteFlags_t
{
	// Indicates that invite was received using console mechanisms (XMB/boot/etc.)
	MM_INVITE_FLAG_CONSOLE	= ( 1 << 0 ),
	// Indicates that the game was booted through a Steam invite
	MM_INVITE_FLAG_PCBOOT	= ( 1 << 1 ),
};


class CMatchFramework :
	public CTier2AppSystem< IMatchFramework >,
	public IMatchEventsSink
{
	// Methods of IAppSystem
public:
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// Methods of IMatchFramework
public:
	// Run frame of the matchmaking framework
	virtual void RunFrame();


	// Get matchmaking extensions
	virtual IMatchExtensions * GetMatchExtensions();

	// Get events container
	virtual IMatchEventsSubscription * GetEventsSubscription();

	// Get the matchmaking title interface
	virtual IMatchTitle * GetMatchTitle();

	// Get the match session interface of the current match framework type
	virtual IMatchSession * GetMatchSession();

	// Get the network msg encode/decode factory
	virtual IMatchNetworkMsgController * GetMatchNetworkMsgController();

	// Get the match system
	virtual IMatchSystem * GetMatchSystem();
	
	// Send the key values back to the server
	virtual void ApplySettings( KeyValues* keyValues );

	// Entry point to create session
	virtual void CreateSession( KeyValues *pSettings );

	// Entry point to match into a session
	virtual void MatchSession( KeyValues *pSettings );

	// Accept invite
	virtual void AcceptInvite( int iController );

	// Close the session
	virtual void CloseSession();

	// Checks to see if the current game is being played online ( as opposed to locally against bots )
	virtual bool IsOnlineGame( void );

	// Called by the client to notify matchmaking that it should update matchmaking properties based
	// on player distribution among the teams.
	virtual void UpdateTeamProperties( KeyValues *pTeamProperties );

	//
	// IMatchEventsSink
	//
public:
	virtual void OnEvent( KeyValues *pEvent );

	// Additional matchmaking title-defined interface
public:
	virtual IMatchTitleGameSettingsMgr * GetMatchTitleGameSettingsMgr();

public:
	void SetCurrentMatchSession( IMatchSessionInternal *pNewMatchSession );
	uint64 GetLastInviteFlags();

protected:
	void RunFrame_Invite();

public:
	CMatchFramework();
	~CMatchFramework();

protected:
	IMatchSessionInternal *m_pMatchSession;

	bool m_bJoinTeamSession;
	KeyValues *m_pTeamSessionSettings;
};

extern CMatchFramework *g_pMMF;

extern const char *COM_GetModDirectory();
extern bool IsLocalClientConnectedToServer();

#endif // MM_FRAMEWORK_H
