//====== Copyright (c), Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef GCBASE_H
#define GCBASE_H
#ifdef _WIN32
#pragma once
#endif

#include "gamecoordinator/igamecoordinator.h"
#include "gamecoordinator/igamecoordinatorhost.h"
#include "tier1/utlallocation.h"
#include "gcmsg.h"
#include "jobmgr.h"
#include "tier1/thash.h"
#include "tier1/utlsortvector.h"
#include "http.h"
#include "language.h"
#include "accountdetails.h"

class CGCMsgGetSystemStatsResponse;

namespace GCSDK
{

class CGCSession;
class CGCUserSession;
class CGCGSSession;
class CGCSharedObjectCache;
class CSharedObject;
class CAccountDetails;

struct PackageLicense_t
{
	uint32 m_unPackageID;
	RTime32 m_rtimeCreated;
};


class CGCBase : public IGameCoordinator
{
public:
	CGCBase( );
	virtual ~CGCBase();

	// Hooks to extend the base behaviors of the IGameCoordinator interface
	virtual bool OnInit() { return true; }
	virtual bool OnMainLoopOncePerFrame( CLimitTimer &limitTimer ) { return false; }
	virtual bool OnMainLoopUntilFrameCompletion( CLimitTimer &limitTimer ) { return false; }
	virtual void OnUninit() {}
	// returns true if this function handled the message
	virtual bool OnMessageFromClient( const CSteamID & senderID, uint32 unMsgType, void *pubData, uint32 cubData ) { return false; }
	virtual void OnValidate( CValidator &validator, const char *pchName ) {}
	virtual const char *LocalizeToken( const char *pchToken, ELanguage eLanguage, bool bReturnTokenIfNotFound = true ) { return ""; }
	
	// Life cycle management functions

	// Called to do any yielding initialization work before reporting as fully operational
	virtual bool BYieldingFinishStartup() = 0;
	
	// Called to do any yielding work immediately after becoming full operational
	virtual bool BYieldingPostStartup() = 0;

	// Call to report that we're fully operational
	void SetStartupComplete( bool bSuccess );

	// Called to do any yielding work before reporting as ready to shutdown
	virtual void YieldingGracefulShutdown() = 0;


	virtual CGCUserSession *CreateUserSession( const CSteamID & steamID, CGCSharedObjectCache *pSOCache ) const;
	virtual CGCGSSession *CreateGSSession( const CSteamID & steamID, CGCSharedObjectCache *pSOCache, uint32 unServerAddr, uint16 usServerPort ) const;
	virtual void YieldingSessionStartPlaying( CGCUserSession *pSession ) {}
	virtual void YieldingSessionStopPlaying( CGCUserSession *pSession ) {}
	virtual void YieldingSessionStartServer( CGCGSSession *pSession ) {}
	virtual void YieldingSessionStopServer( CGCGSSession *pSession ) {}
	virtual void YieldingSOCacheLoaded( CGCSharedObjectCache *pSOCache );

	virtual void YieldingPreTestSetup()	{}

	// cache management
	CGCSharedObjectCache *YieldingGetLockedSOCache( const CSteamID &steamID );
	CGCSharedObjectCache *YieldingFindOrLoadSOCache( const CSteamID &steamID );
	CGCSharedObjectCache *FindSOCache( const CSteamID & steamID );					// non-yielding, but may return NULL if the cache exists but is not loaded
	bool UnloadUnusedCaches( uint32 unMaxCacheCount, CLimitTimer *pLimitTimer = NULL );
	void YieldingReloadCache( CGCSharedObjectCache *pSOCache );
	virtual CGCSharedObjectCache *CreateSOCache( const CSteamID &steamID );

	CGCUserSession *YieldingGetLockedUserSession( const CSteamID & steamID );
	CGCUserSession *FindUserSession( const CSteamID & steamID ) const;

	CGCGSSession *YieldingGetLockedGSSession( const CSteamID & steamID );
	CGCGSSession *YieldingFindOrCreateGSSession( const CSteamID & steamID, uint32 unServerAddr, uint16 usServerPort, const uint8 *pubVarData, uint32 cubVarData );
	CGCGSSession *FindGSSession( const CSteamID & steamID ) const;

	CGCSession *YieldingRequestSession( const CSteamID & steamID );
	bool BYieldingIsOnline( const CSteamID & steamID );
	bool BYieldingSendHTTPRequest( CHTTPRequest *pRequest, CHTTPResponse *pResponse );
	bool BYieldingSendHTTPRequest( CHTTPRequest *pRequest, KeyValues *pkvResponse );
	bool BSendWebApiRegistration();

	int GetSOCacheCount() const;

#ifdef DEBUG
	bool IsSOCached( CSharedObject *pObj, int nTypeID );
#endif

	int GetUserSessionCount() const;
	int GetGSSessionCount() const;

	void SetIsShuttingDown( bool bIsShuttingDown ) { m_bIsShuttingDown = true; }
	bool GetIsShuttingDown() const { return m_bIsShuttingDown; }

	// Iterate over sessions
	// WARNING: Don't yield between GetFirst*/GetNext*.  Use caution when using
	// these any time you might have tens of thousands of sessions to iterate through.
	CGCUserSession **GetFirstUserSession()								{ return m_hashUserSessions.PvRecordFirst(); }
	CGCUserSession **GetNextUserSession( CGCUserSession **pUserSession )		{ return m_hashUserSessions.PvRecordNext( pUserSession ); }
	CGCGSSession **GetFirstGSSession()								{ return m_hashGSSessions.PvRecordFirst(); }
	CGCGSSession **GetNextGSSession( CGCGSSession **pGSSession )		{ return m_hashGSSessions.PvRecordNext( pGSSession ); }

	typedef CUtlSortVector< CGCGSSession *, CUtlSortVectorDefaultLess< CGCGSSession * >, CCopyableUtlVector< CGCGSSession * > > CGCGSSessionSortedVector_t;
	CGCGSSessionSortedVector_t const *GetGSSessionVectorByType( int serverType );

	// This can cause the server pointer to change between lists, call it here
	void SetGSServerType( CGCGSSession *pGSSession, int serverType );

	AppId_t GetAppID() const { return m_unAppID; }
	bool BIsStartupComplete() const { return m_bStartupComplete; }
	CJobMgr &GetJobMgr() { return m_JobMgr; }

	CSteamID YieldingGuessSteamIDFromInput( const char *pchInput );
	bool BYieldingRecordSupportAction( const CSteamID & actorID, const CSteamID & targetID, const char *pchData, const char *pchNote );
	void PostAlert( EAlertType eAlertType, bool bIsCritical, const char *pchAlertText, const CUtlVector< CUtlString > *pvecExtendedInfo = NULL, bool bAlsoSpew = true );
	CAccountDetails *YieldingGetAccountDetails( const CSteamID & steamID );
	bool BYieldingGetAccountLicenses( const CSteamID & steamID, CUtlVector< PackageLicense_t > & vecPackages );
	bool BYieldingLookupAccount( EAccountFindType eFindType, const char *pchInput, CUtlVector< CSteamID > *prSteamIDs );
	bool BYieldingAddFreeLicense( const CSteamID & steamID, uint32 unPackageID, uint32 unIPPublic = 0, const char *pchStoreCountryCode = NULL );

	bool BSendGCMsgToClient( const CSteamID & steamIDTarget, const CGCMsgBase& msg );
	bool BSendGCMsgToClient( const CSteamID & steamIDTarget, const IProtoBufMsg& msg );
	bool BSendSystemMessage( const CGCMsgBase& msg );
	bool BSendSystemMessage( const IProtoBufMsg& msg );

	bool BSendGCInterMessage( AppId_t unAppID, const IProtoBufMsg &msg );
	bool BReplyToMessage( CGCMsgBase &msgOut, const CGCMsgBase &msgIn );
	bool BReplyToMessage( IProtoBufMsg &msgOut, const IProtoBufMsg &msgIn );

	const char *GetPath() const { return m_sPath; }
	virtual const char *GetSteamAPIKey();

	void QueueStartPlaying( const CSteamID & steamID, const CSteamID & gsSteamID, uint32 unServerAddr, uint16 usServerPort, const uint8 *pubVarData, uint32 cubVarData );
	void YieldingExecuteNextStartPlaying();
	bool BIsInLogonSurge() const;

	void YieldingStopPlaying( const CSteamID & steamID );
	void YieldingStartGameserver( const CSteamID & steamID, uint32 unServerAddr, uint16 usServerPort, const uint8 *pubVarData, uint32 cubVarData );
	void YieldingStopGameserver( const CSteamID & steamID );
	bool BIsSOCacheBeingLoaded( const CSteamID & steamID ) const { return m_rbtreeSOCachesBeingLoaded.Find( steamID ) != m_rbtreeSOCachesBeingLoaded.InvalidIndex(); }

	bool BYieldingLockSteamID( const CSteamID &steamID );
	bool BYieldingLockSteamIDPair( const CSteamID &steamIDA, const CSteamID &steamIDB );
	bool BLockSteamIDImmediate( const CSteamID &steamID );
	void UnlockSteamID( const CSteamID &steamID );
	bool IsSteamIDLocked( const CSteamID &steamID );
	bool IsSteamIDLockedByJob( const CSteamID &steamID, const CJob *pJob );
	bool IsSteamIDUnlockedOrLockedByCurJob( const CSteamID &steamID );
	CJob *PJobHoldingLock( const CSteamID &steamID );

	const CLock *FindLock( const CSteamID &steamID );
	void DumpLocks( bool bFull, int nMax = 10 );
	void DumpJobs( int nMax ) const;
	virtual void Dump() const;
	void VerifySOCacheLRU();

	void SetProfilingEnabled( bool bEnabled );
	void SetDumpVprofImbalances( bool bEnabled );
	bool GetVprofImbalances();

	bool YieldingWritebackDirtyCaches( uint32 unSecondToDelayWrite );
	void AddCacheToWritebackQueue( CGCSharedObjectCache *pSOCache );

	bool BYieldingRetrieveCacheVersion( CGCSharedObjectCache *pSOCache );
	void AddCacheToVersionChangedList( CGCSharedObjectCache *pSOCache );

	const char *GetCDNURL() const;

	struct GCMemcachedBuffer_t
	{
		const void *m_pubData;
		int m_cubData;
	};

	struct GCMemcachedGetResult_t
	{
		bool m_bKeyFound;					// true if the key was found
		CUtlAllocation m_bufValue;			// the value of the key
	};
	
	// Memcached access
	bool BMemcachedSet( const char *pKey, const ::google::protobuf::Message &protoBufObj );
	bool BMemcachedDelete( const char *pKey );
	bool BYieldingMemcachedGet( const char *pKey, ::google::protobuf::Message &protoBufObj );
	bool BMemcachedSet( const CUtlString &strKey, const CUtlBuffer &buf );
	bool BMemcachedSet( const CUtlVector<CUtlString> &vecKeys, const CUtlVector<GCMemcachedBuffer_t> &vecValues );
	bool BMemcachedDelete( const CUtlString &strKey );
	bool BMemcachedDelete( const CUtlVector<CUtlString> &vecKeys );
	bool BYieldingMemcachedGet( const CUtlString &strKey, GCMemcachedGetResult_t &result );
	bool BYieldingMemcachedGet( const CUtlVector<CUtlString> &vecKeys, CUtlVector<GCMemcachedGetResult_t> &vecResults );

	// IP location
	bool BYieldingGetIPLocations( CUtlVector<uint32> &vecIPs, CUtlVector<CIPLocationInfo> &infos );

	// Stats
	virtual void SystemStats_Update( CGCMsgGetSystemStatsResponse &msgStats );

protected:
	virtual bool BYieldingLoadSOCache( CGCSharedObjectCache *pSOCache );
	void RemoveSOCache( const CSteamID & steamID );

	void AddCacheToLRU( CGCSharedObjectCache * pSOCache );
	void RemoveCacheFromLRU( CGCSharedObjectCache * pSOCache );

	void SetStartupComplete() { m_bStartupComplete = true; }
private:

	static void AssertCallbackFunc( const char *pchFile, int nLine, const char *pchMessage );
	void UpdateSOCacheVersions();

	// this is called from ExecuteNextStartPlaying and nowhere else
	void YieldingStartPlaying( const CSteamID & steamID, const CSteamID & gsSteamID, uint32 unServerAddr, uint16 usServerPort, CUtlBuffer *pVarData );

	// Base behaviors of the IGameCoordinator interface. These are not overridable.
	// They each call the On* version below, which is overridable by subclasses
	virtual bool BInit( AppId_t unAppID, const char *pchAppPath, IGameCoordinatorHost *pHost );
	virtual bool BMainLoopOncePerFrame( uint64 ulLimitMicroseconds );
	virtual bool BMainLoopUntilFrameCompletion( uint64 ulLimitMicroseconds );
	virtual void Shutdown();
	virtual void Uninit();
	virtual void MessageFromClient( const CSteamID & senderID, uint32 unMsgType, void *pubData, uint32 cubData );
	virtual void Validate( CValidator &validator, const char *pchName );
	virtual void SQLResults( GID_t gidContextID );

	// profiling
	bool m_bStartProfiling;
	bool m_bStopProfiling;
	bool m_bDumpVprofImbalances;

	// local job handling
	CJobMgr m_JobMgr;
	CGCWGJobMgr m_wgJobMgr;

	// session tracking
	CTHash<CGCUserSession *, uint64> m_hashUserSessions;
	CTHash<CGCGSSession *, uint64> m_hashGSSessions;
	typedef CUtlMap< int, CGCGSSessionSortedVector_t > CGCGSSessionServerTypeMap_t;
	CGCGSSessionServerTypeMap_t m_GSSessionsByTypeMap;
	CUtlMap< uint64, CGCGSSession*, int > m_mapGSSessionsByNetAddress;

	// Shared object caches
	CUtlMap<CSteamID, CGCSharedObjectCache *, int> m_mapSOCache;
	CUtlVector< CGCSharedObjectCache * >m_vecCacheWritebacks;
	CUtlLinkedList< CSteamID, uint32> m_listCachesToUnload;
	CUtlRBTree<CSteamID, int > m_rbtreeSOCachesBeingLoaded;
	CUtlRBTree<CSteamID, int > m_rbtreeSOCachesWithDirtyVersions;

	// steamID locks
	CTHash<CLock, CSteamID>	m_hashSteamIDLocks;

	// Account Details
	CAccountDetailsManager	m_AccountDetailsManager;

	// State
	AppId_t					m_unAppID;
	CUtlString				m_sPath;
	IGameCoordinatorHost	*m_pHost;
	bool					m_bStartupComplete;
	bool					m_bIsShuttingDown;

	struct StartPlayingWork_t 
	{
		CSteamID m_steamID;
		CSteamID m_gsSteamID;
		uint32 m_unServerAddr;
		uint16 m_usServerPort;
		CUtlBuffer *m_pVarData;
	};
	CUtlLinkedList< StartPlayingWork_t, int > m_llStartPlaying;
	int m_nStartPlayingJobCount;

	// URL to use for our app's CDN'd images
	mutable CUtlString m_sCDNURL;
};

extern CGCBase *GGCBase();

EResult YieldingSendWebAPIRequest( CSteamAPIRequest &request, KeyValues *pKVResponse, CUtlString &errMsg, bool b200MeansSuccess );

} // namespace GCSDK
#endif // GCBASE_H
