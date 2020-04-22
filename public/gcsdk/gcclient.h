//====== Copyright (c), Valve Corporation, All rights reserved. =======
//
// Purpose: Holds the CGCClient class
//
//=============================================================================

#ifndef GCCLIENT_H
#define GCCLIENT_H
#ifdef _WIN32
#pragma once
#endif

#include "steam/steam_api.h"
#include "jobmgr.h"
#include "sharedobject.h"

class ISteamGameCoordinator;
struct GCMessageAvailable_t;
class CTestEvent;

namespace GCSDK
{


//-----------------------------------------------------------------------------
// Purpose: Interface for communicating with the GC
//-----------------------------------------------------------------------------
class CGCClient
{
public:
	CGCClient( bool bGameserver = false );
	virtual ~CGCClient( );

	/// Call once at program startup
	bool BInit( uint32 unVersion, ISteamClient *pSteamClient, HSteamUser hSteamUser, HSteamPipe hSteamPipe );

	/// Cleanup
	void Uninit( );

	/// Call this to perform periodic service
	bool BMainLoop( uint64 ulLimitMicroseconds, uint64 ulFrameTimeMicroseconds = 0 );

	/// Set current session need state value that is sent in the HELLO message to
	/// determine login priority.  At this generic level we don't know what the
	/// game-specific client states mean, and  which states imply a need for a
	/// GC session, so you need to tell us that, too.  This decides whether we are
	/// aggressive at sending HELLO messages to try to establish the connection
	/// or not.
	void SetSessionNeed( uint32 nSessionNeed, bool bWantSession );

	/// Launcher value.  Sent in the HELLO message
	void SetLauncherType( uint32 nLauncherType ) { m_nLauncherType = nLauncherType; }

	/// Steam datagram port, for servers.  Sent in the HELLO message
	void SetServerSteamdatagramPort( uint16 usPort ) { m_usSteamdatagramPort = usPort; }

	CJobMgr &GetJobMgr() { return m_JobMgr; }

	/// Send a message to the GC.
	bool BSendMessage( uint32 unMsgType, const uint8 *pubData, uint32 cubData );
	bool BSendMessage( const CGCMsgBase& msg );
	bool BSendMessage( const CProtoBufMsgBase& msg );

	/// Locate a given shared object from the cache
	CSharedObject *FindSharedObject( SOID_t ID, const CSharedObject & soIndex );

	/// Find a shared object cache for the specified user.  Optionally, the cache will be
	/// created if it doesn't not currently exist.
	CGCClientSharedObjectCache *FindSOCache( SOID_t ID, bool bCreateIfMissing = true );

	/// Find a set of shared object caches for a specific type of SOID
	/// returns true if any were found
	typedef CUtlVectorFixedGrowable< CGCClientSharedObjectCache *, 1 > ClientSOCacheVec_t;
	bool FindSOCacheByType( uint32 type, ClientSOCacheVec_t &cacheList );

	/// Adds a listener to the shared object cache for the specified Steam ID.
	///
	/// @see CGCClientSharedObjectCache::AddListener
	bool AddSOCacheListener( ISharedObjectListener *pListener );

	/// Removes a listener for the shared object cache for the specified Steam ID.
	/// Returns true if we were listening and were successfully removed, false
	/// otherwise
	///
	/// @see CGCClientSharedObjectCache::RemoveListener
	bool RemoveSOCacheListener( ISharedObjectListener *pListener );

	void DispatchSOCreated( SOID_t owner, const CSharedObject *pObject, ESOCacheEvent eEvent );
	void DispatchSOUpdated( SOID_t owner, const CSharedObject *pObject, ESOCacheEvent eEvent );
	void DispatchSODestroyed( SOID_t owner, const CSharedObject *pObject, ESOCacheEvent eEvent );
	void DispatchSOCacheSubscribed( SOID_t owner, ESOCacheEvent eEvent );
	void DispatchSOCacheUnsubscribed( SOID_t owner, ESOCacheEvent eEvent );

	typedef CUtlVector< ISharedObjectListener * > SharedObjectListensersVec_t;
	const SharedObjectListensersVec_t & GetListeners() const { return m_vecListeners; }

	void OnGCMessageAvailable( GCMessageAvailable_t *pCallback );
	ISteamGameCoordinator *GetSteamGameCoordinator() { return m_pSteamGameCoordinator; }

	virtual void Test_AddEvent( CTestEvent *pEvent )	{}
	virtual void Test_CacheSubscribed( SOID_t ID ) {}

	void NotifySOCacheUnsubscribed( SOID_t ID );
	void NotifyResubscribedUpToDate( SOID_t ID );

	/// Send a HELLO message to the GC now.
	void SendHello();

	// Called when we receive a welcome message, to sync up our SO caches with the
	// what the GC is telling us we have.
	void ProcessSOCacheSubscribedMsg( const CMsgSOCacheSubscribed &msg );

	/// Simulate inability to connect to DOTA's GC.
	/// (But allow us to connect to Steam.)
	bool GetSimulateGCConnectionFailure() const { return m_bSimulateGCConnectionFailure; }
	void SetSimulateGCConnectionFailure( bool bForcedFailure );

	/// Called when any messages times out.  When this happens, it's usually
	/// safe to assume that the connection has been interrupted, and we should
	/// renegotiate.
	void MessageReplyTimedOut( uint32 nExpectedMsg, uint nTimeoutSecs );

	//
	// Logon queue stats.
	//
	// These return negative if quantities are not known.
	// All of these numbers can only be valid if we're in the
	// GCConnectionStatus_NO_SESSION_IN_LOGON_QUEUE state.  However,
	// just because we're in that state does NOT mean that they will be
	// available!
	int GetLogonQueuePosition() const { return m_nLogonQueuePosition; }
	int GetLogonQueueSize() const { return m_nLogonQueueSize; }
	int GetLogonQueueEstimatedSecondsRemaining() const;
	int GetLogonQueueApproxWaitSeconds() const;

protected:

	ISteamUser *m_pSteamUser;
	ISteamGameServer *m_pSteamGameserver;
	ISteamGameCoordinator *m_pSteamGameCoordinator;
	CUtlMemory<uint8> m_memMsg;

	// local job handling
	CJobMgr m_JobMgr;

	// Shared object caches
	typedef CUtlMap<SOID_t, CGCClientSharedObjectCache *> MapSOCache_t;
	MapSOCache_t m_mapSOCache;

	SharedObjectListensersVec_t m_vecListeners;

	uint64 m_timeLastSendHello;
	uint64 m_timeReceivedConnectionStatus;
	uint64 m_timeLoggedOn;
	uint32 m_unVersion;
	const bool m_bGameserver;
	bool m_bSimulateGCConnectionFailure;
	uint32 m_nSessionNeed;
	uint32 m_nLastSessionNeed; // last session need state sent / received from the GC
	bool m_bWantSession;
	uint32 m_nLauncherType;
	uint16 m_usSteamdatagramPort;

	int m_nLogonQueuePosition;
	int m_nLogonQueueSize;
	uint64 m_timeLogonQueueApproxTimeEnteredQueue;
	uint64 m_timeLogonQueueEstimatedTimeExitQueue;

	void ClearLogonQueueStats();

	void UpdateLogonState();
	void ThinkConnection();
	void DispatchPacket( IMsgNetPacket *pMsgNetPacket );

	// Steam callback for getting notified about messages available. Not part of the class
	// in Steam builds because we use the TestClientManager instead of steam_api.dll in Steam 
#ifndef STEAM
	CCallback< CGCClient, GCMessageAvailable_t, false > m_callbackGCMessageAvailable;
	STEAM_CALLBACK( CGCClient, OnSteamServersDisconnected, SteamServersDisconnected_t, m_CallbackSteamServersDisconnected );
	STEAM_CALLBACK( CGCClient, OnSteamServerConnectFailure, SteamServerConnectFailure_t, m_CallbackSteamServerConnectFailure );
	STEAM_CALLBACK( CGCClient, OnSteamServersConnected, SteamServersConnected_t, m_CallbackSteamServersConnected );
#endif

};

//utility to make defining client jobs more straight forward
#define GC_REG_CLIENT_JOB( JobClass, Msg )	\
	GC_REG_JOB( GCSDK::CGCClient, JobClass, #JobClass, Msg )


} // namespace GCSDK

#endif // GCCLIENT_H
