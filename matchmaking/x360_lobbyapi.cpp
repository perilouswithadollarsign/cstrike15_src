//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_framework.h"

#include "fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef _X360



//
// Dormant operations
//

struct OverlappedDormant_t
{
	XOVERLAPPED *m_pOverlapped;
	void ( *m_pfnCompletion )( XOVERLAPPED *, void * );
	void *m_pvParam;
};
static CUtlVector< OverlappedDormant_t > s_arrOverlappedDormants;

struct ScheduledCall_t
{
	float m_flTimeToCall;
	void ( *m_pfnCall )( ScheduledCall_t *pThis );
};
static CUtlVector< ScheduledCall_t * > s_arrScheduledCalls;

static CUtlVector< IDormantOperation * > s_arrDormantInterfaces;

XOVERLAPPED * MMX360_NewOverlappedDormant( void (*pfnCompletion)( XOVERLAPPED *, void * ), void *pvParam )
{
	XOVERLAPPED *pOverlapped = new XOVERLAPPED;
	memset( pOverlapped, 0, sizeof( XOVERLAPPED ) );
	OverlappedDormant_t od = { pOverlapped, pfnCompletion, pvParam };
	s_arrOverlappedDormants.AddToTail( od );
	return pOverlapped;
}

void MMX360_RegisterDormant( IDormantOperation *pDormant )
{
	if ( !pDormant )
		return;

	if ( s_arrDormantInterfaces.Find( pDormant ) != s_arrDormantInterfaces.InvalidIndex() )
		return;

	s_arrDormantInterfaces.AddToTail( pDormant );
}

void MMX360_UpdateDormantOperations()
{
	int nQueuedOverlapped = s_arrOverlappedDormants.Count();
	for ( int k = 0; k < s_arrOverlappedDormants.Count(); ++ k )
	{
		OverlappedDormant_t od = s_arrOverlappedDormants[k];
		if ( XHasOverlappedIoCompleted( od.m_pOverlapped ) )
		{
			s_arrOverlappedDormants.FastRemove( k -- );
			if ( od.m_pfnCompletion )
			{
				(od.m_pfnCompletion)( od.m_pOverlapped, od.m_pvParam );
			}
			delete od.m_pOverlapped;
		}
	}
	if ( nQueuedOverlapped && !s_arrOverlappedDormants.Count() )
	{
		DevMsg( 2, "MMX360_UpdateDormantOperations finished all overlapped calls.\n" );
	}

	int nQueuedScheduledCalls = s_arrScheduledCalls.Count();
	for ( int k = 0; k < s_arrScheduledCalls.Count(); ++ k )
	{
		ScheduledCall_t *pCall = s_arrScheduledCalls[k];
		if ( Plat_FloatTime() > pCall->m_flTimeToCall )
		{
			s_arrScheduledCalls.FastRemove( k -- );
			( pCall->m_pfnCall )( pCall );
		}
	}
	if ( nQueuedScheduledCalls && !s_arrScheduledCalls.Count() )
	{
		DevMsg( 2, "MMX360_UpdateDormantOperations finished all scheduled calls.\n" );
	}

	int nDormantInterfaces = s_arrDormantInterfaces.Count();
	for ( int k = 0; k < s_arrDormantInterfaces.Count(); ++ k )
	{
		IDormantOperation *pDormant = s_arrDormantInterfaces[k];
		bool bKeepRunning = pDormant->UpdateDormantOperation();
		if ( !bKeepRunning )
			s_arrDormantInterfaces.Remove( k -- );
	}
	if ( nDormantInterfaces && !s_arrDormantInterfaces.Count() )
	{
		DevMsg( 2, "MMX360_UpdateDormantOperations finished all dormant interfaces.\n" );
	}
}

int MMX360_GetUserCtrlrIndex( XUID xuid )
{
	if ( !xuid )
		return -1;

	for ( int k = 0; k < ( int ) XBX_GetNumGameUsers(); ++ k )
	{
		int iCtrlr = XBX_GetUserId( k );
		XUID xuidLocal = 0;
		if ( ERROR_SUCCESS == XUserGetXUID( iCtrlr, &xuidLocal ) )
		{
			if ( xuidLocal == xuid )
				return iCtrlr;
		}
		XUSER_SIGNIN_INFO xsi;
		if ( ERROR_SUCCESS == XUserGetSigninInfo( iCtrlr, XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY, &xsi ) )
		{
			if ( xsi.xuid == xuid )
				return iCtrlr;
		}
		if ( ERROR_SUCCESS == XUserGetSigninInfo( iCtrlr, XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY, &xsi ) )
		{
			if ( xsi.xuid == xuid )
				return iCtrlr;
		}
	}

	return -1;
}

void MMX360_Helper_DataToString( void const *pvData, int numBytes, char *pchBuffer )
{
	for ( const unsigned char *pb = ( const unsigned char * ) pvData, *pbEnd = pb + numBytes;
		pb < pbEnd; ++ pb )
	{
		*( pchBuffer ++ ) = '0' + ( unsigned char )( pb[0] & 0x0Fu );
		*( pchBuffer ++ ) = '0' + ( unsigned char )( ( pb[0] >> 4 ) & 0x0Fu );
	}
	*pchBuffer = 0;
}

void MMX360_Helper_DataFromString( void *pvData, int numBytes, char const *pchBuffer )
{
	if ( !pchBuffer )
		goto parse_error;

	for ( unsigned char *pb = ( unsigned char * ) pvData, *pbEnd = pb + numBytes;
		pb < pbEnd; ++ pb )
	{
		if ( !pchBuffer[0] || !pchBuffer[1] )
			goto parse_error;

		pb[0] = ( unsigned char )( ( pchBuffer[0] - '0' ) & 0x0Fu ) |
			( unsigned char )( ( ( pchBuffer[1] - '0' ) & 0x0Fu ) << 4 );

		pchBuffer += 2;
	}
	return;

parse_error:
	memset( pvData, 0, numBytes );
}

void MMX360_SessionInfoToString( XSESSION_INFO const &xsi, char *pchBuffer )
{
	MMX360_Helper_DataToString( &xsi, sizeof( xsi ), pchBuffer );
}

void MMX360_SessionInfoFromString( XSESSION_INFO &xsi, char const *pchBuffer )
{
	MMX360_Helper_DataFromString( &xsi, sizeof( xsi ), pchBuffer );
}

void MMX360_XnaddrToString( XNADDR const &xsi, char *pchBuffer )
{
	MMX360_Helper_DataToString( &xsi, sizeof( xsi ), pchBuffer );
}

void MMX360_XnaddrFromString( XNADDR &xsi, char const *pchBuffer )
{
	MMX360_Helper_DataFromString( &xsi, sizeof( xsi ), pchBuffer );
}

//
// Active gameplay sessions stack
//
// Only one session can be considered active gameplay session
// and we will consider the last session that received XSessionStart
//
struct CX360ActiveGameplaySession_t
{
	CX360ActiveGameplaySession_t( HANDLE hHandle )
	{
		m_hHandle = hHandle;
		m_flLastFlushTime = Plat_FloatTime();
		m_bNeedsFlush = false;
	}

	HANDLE m_hHandle;
	float m_flLastFlushTime;
	bool m_bNeedsFlush;
};
static CUtlVector< CX360ActiveGameplaySession_t > s_arrActiveGameplaySessions;

CX360ActiveGameplaySession_t * MMX360_GetActiveGameplaySession()
{
	return s_arrActiveGameplaySessions.Count() ? &s_arrActiveGameplaySessions.Tail() : NULL;
}


//
// Session stats flush scheduled call
//

struct ScheduledCall_FlushSessionStats : public ScheduledCall_t
{
	ScheduledCall_FlushSessionStats()
	{
		m_flTimeToCall = Plat_FloatTime() + 0.1f;
		m_pfnCall = Callback;
	}

protected:
	void Run()
	{
		// Walk all the scheduled sessions and XSessionFlushStats them if needed
		for ( int k = 0; k < s_arrActiveGameplaySessions.Count(); ++ k )
		{
			CX360ActiveGameplaySession_t &ags = s_arrActiveGameplaySessions[k];
			if ( !ags.m_bNeedsFlush )
				continue;

			if ( Plat_FloatTime() < ags.m_flLastFlushTime + 6*60 )
				// TCR: cannot flush within 5 minutes of last flush (use 6 minutes just in case)
				continue;

			// Schedule an overlapped flush
			ags.m_flLastFlushTime = Plat_FloatTime();
			ags.m_bNeedsFlush = false;

			DevMsg( "ScheduledCall_FlushSessionStats for session %p (@%.2f)\n", ags.m_hHandle, ags.m_flLastFlushTime );
			g_pMatchExtensions->GetIXOnline()->XSessionFlushStats( ags.m_hHandle, MMX360_NewOverlappedDormant() );
		}
		delete this;
	}
	static void Callback( ScheduledCall_t *pThis )
	{
		( ( ScheduledCall_FlushSessionStats * )( pThis ) )->Run();
	}
};

//
// Leaderboard async requests implementation
//

class CLeaderboardX360_Writer : public IX360LeaderboardBatchWriter
{
public:
	explicit CLeaderboardX360_Writer( XUID xuidGamer );

public:
	virtual void AddProperty( int dwViewId, XUSER_PROPERTY const &xp );
	virtual void WriteBatchAndDestroy();
	
protected:
	bool WriteBatch();
	void Destroy();
	
	static void OnXSessionWriteStatsCompleted( XOVERLAPPED *, void *pvThis )
	{
		( ( CLeaderboardX360_Writer * )pvThis )->Destroy();
	}

public:
	CUtlVector< XSESSION_VIEW_PROPERTIES > m_arrLbViews;
	CUtlVectorFixed< XUSER_PROPERTY, 64 > m_arrLbProperties;
	HANDLE m_hSession;
	XUID m_xuidGamer;
};

IX360LeaderboardBatchWriter * MMX360_CreateLeaderboardBatchWriter( XUID xuidGamer )
{
	if ( !MMX360_GetActiveGameplaySession() )
	{
		Warning( "MMX360_CreateLeaderboardBatchWriter called with no active gameplay session!\n" );
		Assert( 0 );
		return NULL;
	}

	return new CLeaderboardX360_Writer( xuidGamer );
}

CLeaderboardX360_Writer::CLeaderboardX360_Writer( XUID xuidGamer )
{
	CX360ActiveGameplaySession_t *ags = MMX360_GetActiveGameplaySession();
	m_hSession = ags ? ags->m_hHandle : NULL;
	m_xuidGamer = xuidGamer;
}

void CLeaderboardX360_Writer::AddProperty( int dwViewId, XUSER_PROPERTY const &xp )
{
	if ( m_arrLbProperties.Count() >= 63 )
	{
		Warning( "CLeaderboardX360_Writer: exceeded number of properties for update!\n" );
		return;
	}

	m_arrLbProperties.AddMultipleToTail( 1, &xp );
	XUSER_PROPERTY *xProp = &m_arrLbProperties.Tail();

	if ( !m_arrLbViews.Count() || m_arrLbViews.Tail().dwViewId != ( DWORD ) dwViewId )
	{
		m_arrLbViews.AddMultipleToTail( 1 );

		XSESSION_VIEW_PROPERTIES *xView = &m_arrLbViews.Tail();
		xView->dwViewId = dwViewId;
		xView->dwNumProperties = 0;
		xView->pProperties = xProp;
	}

	++ m_arrLbViews.Tail().dwNumProperties;
}

bool CLeaderboardX360_Writer::WriteBatch()
{
	if ( !m_arrLbViews.Count() )
		return false;

	CX360ActiveGameplaySession_t &ags = *MMX360_GetActiveGameplaySession();
	if ( !&ags || !ags.m_hHandle )
	{
		Warning( "CLeaderboardX360_Writer::WriteBatch called without active gameplay session!\n" );
		Assert( 0 );
		return false;
	}

	if ( ags.m_hHandle != m_hSession )
	{
		Warning( "CLeaderboardX360_Writer::WriteBatch called when active gameplay session changed!\n" );
		Assert( 0 );
		return false;
	}

	DWORD ret = g_pMatchExtensions->GetIXOnline()->XSessionWriteStats( ags.m_hHandle,
		m_xuidGamer, m_arrLbViews.Count(), m_arrLbViews.Base(),
		MMX360_NewOverlappedDormant( OnXSessionWriteStatsCompleted, this ) );
	if ( ret != ERROR_SUCCESS && ret != ERROR_IO_PENDING )
	{
		Warning( "XSessionWriteStats failed, code %d, xuid=%llx, numViews=%d, numProps=%d\n",
			ret, m_xuidGamer, m_arrLbViews.Count(), m_arrLbProperties.Count() );
		return false;
	}

	// Mark the session as needing a flush when the TCR allows us to flush session stats
	ags.m_bNeedsFlush = true;
	DevMsg( "XSessionWriteStats for session %p (%.2f, last flush @%.2f)\n", ags.m_hHandle, Plat_FloatTime(), ags.m_flLastFlushTime );
	return true;
}

void CLeaderboardX360_Writer::Destroy()
{
	m_hSession = NULL;
	delete this;

	// Submit a scheduled call to flush session stats for all previously written stats that might need a flush
	DevMsg( "ScheduledCall_FlushSessionStats @%.2f\n", Plat_FloatTime() );
	s_arrScheduledCalls.AddToTail( new ScheduledCall_FlushSessionStats );
}

void CLeaderboardX360_Writer::WriteBatchAndDestroy()
{
	if ( !WriteBatch() )
		Destroy();
}


//
// Basic async operation on a lobby
//

class CX360LobbyAsyncOperation : public IX360LobbyAsyncOperation, public IDormantOperation
{
public:
	CX360LobbyAsyncOperation() : m_eState( AOS_RUNNING ), m_result( 0 ), m_pCancelOverlappedJob( NULL )
	{
		memset( &m_xOverlapped, 0, sizeof( m_xOverlapped ) );
	}

	virtual ~CX360LobbyAsyncOperation()
	{
		;
	}

	//
	// IMatchAsyncOperation
	//
public:
	// Poll if operation has completed
	virtual bool IsFinished() { return m_eState >= AOS_ABORTED; }

	// Operation state
	virtual AsyncOperationState_t GetState() { return m_eState; }

	// Retrieve a generic completion result for simple operations
	// that return simple results upon success,
	// results are operation-specific, may result in undefined behavior
	// if operation is still in progress.
	virtual uint64 GetResult() { return m_result; }

	// Request operation to be aborted
	virtual void Abort();

	// Release the operation interface and all resources
	// associated with the operation. Operation callbacks
	// will not be called after Release. Operation object
	// cannot be accessed after Release.
	virtual void Release();

	//
	// Overrides
	//
public:
	virtual CX360LobbyObject const & GetLobby() { return m_lobby; }
	virtual void Update();
	virtual void OnFinished( DWORD dwRetCode, DWORD dwResult );

	virtual bool UpdateDormantOperation();

public:
	AsyncOperationState_t m_eState;
	XOVERLAPPED m_xOverlapped;
	uint64 m_result;
	CX360LobbyObject m_lobby;
	CJob *m_pCancelOverlappedJob;
};

void CX360LobbyAsyncOperation::Abort()
{
	Update();
	if ( m_eState != AOS_RUNNING )
		return;

	m_eState = AOS_ABORTING;

	m_pCancelOverlappedJob = ThreadExecute( MMX360_CancelOverlapped, &m_xOverlapped );
	m_eState = AOS_ABORTED;
}

void CX360LobbyAsyncOperation::Release()
{
	if ( !IsFinished() )
	{
		Abort();
	}

	if ( m_pCancelOverlappedJob )
		MMX360_RegisterDormant( this );
	else
		delete this;
}

void CX360LobbyAsyncOperation::Update()
{
	if ( IsFinished() )
		return;

	// Check if the operation has completed
	if ( !XHasOverlappedIoCompleted( &m_xOverlapped ) )
		return;

	DWORD dwResult;
	DWORD dwRetCode = XGetOverlappedResult( &m_xOverlapped, &dwResult, FALSE );
	
	m_eState = ( dwRetCode == ERROR_SUCCESS ) ? AOS_SUCCEEDED : AOS_FAILED;

	OnFinished( dwRetCode, dwResult );
}

bool CX360LobbyAsyncOperation::UpdateDormantOperation()
{
	if ( !m_pCancelOverlappedJob->IsFinished() )
		return true; // keep running dormant

	m_pCancelOverlappedJob->Release();
	m_pCancelOverlappedJob = NULL;

	delete this;
	return false;	// destroyed object, remove from dormant list
}

void CX360LobbyAsyncOperation::OnFinished( DWORD dwRetCode, DWORD dwResult )
{
	m_result = dwResult;
}

class CX360LobbyQosOperation : public CX360LobbyAsyncOperation
{
public:
	CX360LobbyQosOperation() : m_pXnQos( NULL ) {}

public:
	virtual void Abort();
	virtual void Update();

public:
	XNQOS *m_pXnQos;
};

void CX360LobbyQosOperation::Abort()
{
	Update();
	if ( m_eState != AOS_RUNNING )
		return;

	g_pMatchExtensions->GetIXOnline()->XNetQosRelease( m_pXnQos );
	m_pXnQos = NULL;
	m_eState = AOS_ABORTED;
}

void CX360LobbyQosOperation::Update()
{
	if ( IsFinished() )
		return;

	// Check if the QOS is not running
	if ( !m_pXnQos )
	{
		m_eState = AOS_FAILED;
		OnFinished( ERROR_SUCCESS, ERROR_SUCCESS );
		return;
	}

	// Check if still pending
	if ( m_pXnQos->cxnqosPending > 0 )
		return;

	bool bSuccess = true;
	BYTE uNeedFlags = XNET_XNQOSINFO_TARGET_CONTACTED | XNET_XNQOSINFO_DATA_RECEIVED;
	for ( uint k = 0; k < m_pXnQos->cxnqos; ++ k )
	{
		XNQOSINFO &xqi = m_pXnQos->axnqosinfo[k];
		if ( ( ( xqi.bFlags & uNeedFlags ) != uNeedFlags) ||
			( xqi.bFlags & XNET_XNQOSINFO_TARGET_DISABLED ) ||
			!xqi.cbData || !xqi.pbData )
		{
			bSuccess = false;
			break;
		}
	}

	m_eState = ( bSuccess ) ? AOS_SUCCEEDED : AOS_FAILED;
	DWORD dwCode = ( bSuccess ) ? ERROR_SUCCESS : ERROR_FILE_NOT_FOUND;
	
	OnFinished( dwCode, dwCode );

	// Release the QOS
	g_pMatchExtensions->GetIXOnline()->XNetQosRelease( m_pXnQos );
	m_pXnQos = NULL;
}

template < typename TBase = CX360LobbyAsyncOperation >
class CX360AsyncOperationDelegating : public TBase
{
public:
	typedef CX360AsyncOperationDelegating<TBase> TX360AsyncOperationDelegating;
	typedef TBase TX360AsyncOperationDelegatingBase;

public:
	CX360AsyncOperationDelegating() : m_pDelegate( NULL )
	{
	}

	//
	// IMatchAsyncOperation
	//
public:
	// Poll if operation has completed
	virtual bool IsFinished() { return m_pDelegate ? m_pDelegate->IsFinished() : TBase::IsFinished(); }

	// Operation state
	virtual AsyncOperationState_t GetState() { return m_pDelegate ? m_pDelegate->GetState() : TBase::GetState(); }

	// Retrieve a generic completion result for simple operations
	// that return simple results upon success,
	// results are operation-specific, may result in undefined behavior
	// if operation is still in progress.
	virtual uint64 GetResult() { return m_pDelegate ? m_pDelegate->GetResult() : TBase::GetResult(); }

	// Request operation to be aborted
	virtual void Abort()
	{
		if ( m_pDelegate )
			m_pDelegate->Abort();
		else
			TBase::Abort();
	}

	// Release the operation interface and all resources
	// associated with the operation. Operation callbacks
	// will not be called after Release. Operation object
	// cannot be accessed after Release.
	virtual void Release()
	{
		if ( m_pDelegate )
			m_pDelegate->Release();
		m_pDelegate = NULL;
		TBase::Release();
	}

	//
	// Overrides
	//
public:
	virtual CX360LobbyObject const & GetLobby()
	{
		if ( m_pDelegate )
			return m_pDelegate->GetLobby();
		else
			return TBase::GetLobby();
	}
	virtual void Update()
	{
		if ( m_pDelegate )
			m_pDelegate->Update();
		else
			TBase::Update();
	}

public:
	IX360LobbyAsyncOperation *m_pDelegate;
};


//
// Deleting a lobby
//

class CX360LobbyAsyncOperation_LobbyDelete : public CX360LobbyAsyncOperation
{
public:
	explicit CX360LobbyAsyncOperation_LobbyDelete( CX360LobbyObject &lobby )
	{
		m_lobby = lobby;

		// Submit XSessionDelete
		DWORD ret = g_pMatchExtensions->GetIXOnline()->XSessionDelete( m_lobby.m_hHandle, &m_xOverlapped );

		if ( ret != ERROR_IO_PENDING )
		{
			DevWarning( "XSessionDelete failed (code 0x%08X)\n", ret );
			Assert( 0 );

			m_eState = AOS_FAILED;
			m_result = ret;
		}
	}

	virtual void Abort()
	{
		DevWarning( "XSessionDelete cannot be aborted!\n" );
		Assert( 0 );
	}

	virtual void OnFinished( DWORD dwRetCode, DWORD dwResult )
	{
		CX360LobbyAsyncOperation::OnFinished( dwRetCode, dwResult );
		CloseHandle( m_lobby.m_hHandle );
	}
};

void MMX360_LobbyDelete( CX360LobbyObject &lobby, IX360LobbyAsyncOperation **ppOperation )
{
	Assert( lobby.m_hHandle );
	Assert( ppOperation );

	if ( ppOperation )
		*ppOperation = NULL;
	else
		return;

	// Release QOS listener
	g_pMatchExtensions->GetIXOnline()->XNetQosListen( &lobby.m_xiInfo.sessionID, 0, 0, 0, XNET_QOS_LISTEN_RELEASE );

	if ( lobby.m_bXSessionStarted )
	{
		DevWarning( "LobbyDelete called on an active gameplay session, forcing XSessionEnd!\n" );
		// TODO: investigate whether this is not resulting in any side-effects in arbitrated ranked games
		// especially when the host leaves the game.
		MMX360_LobbySetActiveGameplayState( lobby, false, NULL );
	}
	
	*ppOperation = new CX360LobbyAsyncOperation_LobbyDelete( lobby );
	lobby = CX360LobbyObject();
}


//
// Describing lobby flags
//

CX360LobbyFlags_t MMX360_DescribeLobbyFlags( KeyValues *pSettings, bool bHost, bool bWantLocked )
{
	CX360LobbyFlags_t fl = {0};
	
	char const *szLock = pSettings->GetString( "system/lock", NULL );
	char const *szNetwork = pSettings->GetString( "system/network", "LIVE" );
	char const *szAccess = pSettings->GetString( "system/access", "public" );
	char const *szNetFlag = pSettings->GetString( "system/netflag", NULL );
	int numSlots = pSettings->GetInt( "members/numSlots", XBX_GetNumGameUsers() );

	// Gametype
	fl.m_dwGameType = X_CONTEXT_GAME_TYPE_STANDARD;

	// Flags
	if ( !Q_stricmp( szNetwork, "LIVE" ) )
	{
		fl.m_dwFlags = XSESSION_CREATE_LIVE_MULTIPLAYER_STANDARD;

		if ( szNetFlag )
		{
			if ( !Q_stricmp( szNetFlag, "teamlobby" ) )
			{
				fl.m_dwFlags = XSESSION_CREATE_USES_PRESENCE | XSESSION_CREATE_USES_PEER_NETWORK;
			}
			else if ( !Q_stricmp( szNetFlag, "teamlink" ) )
			{
				fl.m_dwFlags = /*XSESSION_CREATE_USES_STATS |*/ XSESSION_CREATE_USES_MATCHMAKING | XSESSION_CREATE_USES_PEER_NETWORK;
			}
		}
	}
	else if ( !Q_stricmp( szNetwork, "lan" ) )
	{
		fl.m_dwFlags = XSESSION_CREATE_SYSTEMLINK;
	}
	else
	{
		fl.m_dwFlags = XSESSION_CREATE_SINGLEPLAYER_WITH_STATS;
	}

	// Whether session can be disabled in XUI
	fl.m_bCanLockJoins = ( ( fl.m_dwFlags & XSESSION_CREATE_USES_PRESENCE ) == XSESSION_CREATE_USES_PRESENCE );

	// Hosting
	if ( bHost )
	{
		fl.m_dwFlags |= XSESSION_CREATE_HOST;
	}

	bool bPublic = true;
	if ( fl.m_bCanLockJoins )
	{
		if ( bWantLocked || ( szLock && *szLock && !IsX360() ) )
		{
			bPublic = false;
			fl.m_dwFlags |= ( XSESSION_CREATE_INVITES_DISABLED | XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED | XSESSION_CREATE_JOIN_IN_PROGRESS_DISABLED );
		}
		else if ( !Q_stricmp( "private", szAccess ) )
		{
			bPublic = false;
			fl.m_dwFlags |= XSESSION_CREATE_JOIN_VIA_PRESENCE_DISABLED;
		}
		else if ( !Q_stricmp( "friends", szAccess ) )
		{
			fl.m_dwFlags |= XSESSION_CREATE_JOIN_VIA_PRESENCE_FRIENDS_ONLY;
		}
	}

	fl.m_numPublicSlots = bPublic ? numSlots : 0;
	fl.m_numPrivateSlots = bPublic ? 0 : numSlots;

	return fl;
}


//
// Lobby creation
//

class CX360LobbyAsyncOperation_LobbyCreate : public CX360LobbyAsyncOperation
{
public:
	explicit CX360LobbyAsyncOperation_LobbyCreate( CX360LobbyFlags_t const &fl )
	{
		// Set the required contexts
		for ( int k = 0; k < ( int ) XBX_GetNumGameUsers(); ++ k )
		{
			int iCtrlr = XBX_GetUserId( k );
			XUserSetContext( iCtrlr, X_CONTEXT_GAME_TYPE, fl.m_dwGameType );
		}

		// Submit XSessionCreate
		DWORD ret = g_pMatchExtensions->GetIXOnline()->XSessionCreate( fl.m_dwFlags, XBX_GetPrimaryUserId(), fl.m_numPublicSlots, fl.m_numPrivateSlots,
			&m_lobby.m_uiNonce, &m_lobby.m_xiInfo, &m_xOverlapped, &m_lobby.m_hHandle );

		if ( ret != ERROR_IO_PENDING )
		{
			DevWarning( "XSessionCreate failed (code 0x%08X)\n", ret );
			Assert( 0 );

			m_eState = AOS_FAILED;
			m_result = ret;
		}
	}

	virtual void OnFinished( DWORD dwRetCode, DWORD dwResult )
	{
		CX360LobbyAsyncOperation::OnFinished( dwRetCode, dwResult );
		if ( GetState() == AOS_SUCCEEDED )
		{
			// Enable QOS listener when session creation succeeds
			g_pMatchExtensions->GetIXOnline()->XNetQosListen( &m_lobby.m_xiInfo.sessionID, 0, 0, 0, XNET_QOS_LISTEN_ENABLE );
		}
	}
};

void MMX360_LobbyCreate( KeyValues *pSettings, IX360LobbyAsyncOperation **ppOperation )
{
	Assert( pSettings );
	Assert( ppOperation );

	if ( ppOperation )
		*ppOperation = NULL;
	else
		return;
	
	if ( !pSettings )
		return;

	// Determine parameters
	CX360LobbyFlags_t fl = MMX360_DescribeLobbyFlags( pSettings, true, true );
	if ( KeyValues *pKv = g_pMMF->GetMatchTitleGameSettingsMgr()->PrepareForSessionCreate( pSettings ) )
	{
		pKv->deleteThis();
	}

	// Allocate our lobby object
	*ppOperation = new CX360LobbyAsyncOperation_LobbyCreate( fl );
}


//
// Lobby connect operation
//

class CX360LobbyAsyncOperation_LobbyConnect : public CX360LobbyAsyncOperation
{
public:
	explicit CX360LobbyAsyncOperation_LobbyConnect( KeyValues *pHostSettings, XSESSION_INFO const &xsi )
	{
		m_lobby.m_xiInfo = xsi;

		// Prepare for lobby create
		CX360LobbyFlags_t fl = MMX360_DescribeLobbyFlags( pHostSettings, false, true );
		if ( KeyValues *pKv = g_pMMF->GetMatchTitleGameSettingsMgr()->PrepareForSessionCreate( pHostSettings ) )
		{
			pKv->deleteThis();
		}

		// Set the required contexts
		for ( int k = 0; k < ( int ) XBX_GetNumGameUsers(); ++ k )
		{
			int iCtrlr = XBX_GetUserId( k );
			XUserSetContext( iCtrlr, X_CONTEXT_GAME_TYPE, fl.m_dwGameType );
		}

		// Submit XSessionCreate
		DWORD ret = g_pMatchExtensions->GetIXOnline()->XSessionCreate( fl.m_dwFlags, XBX_GetPrimaryUserId(), fl.m_numPublicSlots, fl.m_numPrivateSlots,
			&m_lobby.m_uiNonce, &m_lobby.m_xiInfo, &m_xOverlapped, &m_lobby.m_hHandle );

		if ( ret != ERROR_IO_PENDING )
		{
			DevWarning( "XSessionCreate failed (code 0x%08X)\n", ret );
			Assert( 0 );

			m_eState = AOS_FAILED;
			m_result = ret;
		}
	}
};

class CX360LobbyAsyncOperation_LobbyQosAndConnect : public CX360AsyncOperationDelegating< CX360LobbyQosOperation >
{
public:
	explicit CX360LobbyAsyncOperation_LobbyQosAndConnect( XSESSION_INFO const &xsi ) :
		m_xsiQosInfo( xsi )
	{
		// Perform the QOS lookup
		m_pXnaddr = &m_xsiQosInfo.hostAddress;
		m_pXnkid = &m_xsiQosInfo.sessionID;
		m_pXnkey = &m_xsiQosInfo.keyExchangeKey;
		INT ret = g_pMatchExtensions->GetIXOnline()->XNetQosLookup( 1, &m_pXnaddr, &m_pXnkid, &m_pXnkey,
			0, NULL, NULL, 2, 0, 0, NULL,
			&m_pXnQos );

		if ( 0 != ret )
		{
			DevWarning( "XNetQosLookup failed (code 0x%08X)\n", ret );
			Assert( 0 );

			m_eState = AOS_FAILED;
			m_result = ERROR_NO_SUCH_PRIVILEGE;
			return;
		}
	}

public:
	virtual void OnFinished( DWORD dwRetCode, DWORD dwResult )
	{
		TX360AsyncOperationDelegating::OnFinished( dwRetCode, dwResult );
		if ( m_eState == AOS_SUCCEEDED && !m_pDelegate )
		{
			// We successfully contacted the QOS host and obtained the data
			XNQOSINFO &xqi = m_pXnQos->axnqosinfo[0];

			MM_GameDetails_QOS_t gd = { xqi.pbData, xqi.cbData, xqi.wRttMedInMsecs };
			KeyValues *pHostGameDetails = g_pMatchFramework->GetMatchNetworkMsgController()->UnpackGameDetailsFromQOS( &gd );
			KeyValues::AutoDelete autodelete_pHostGameDetails( pHostGameDetails );

			// Output unpacked game details from host
			KeyValuesDumpAsDevMsg( pHostGameDetails, 1, 2 );

			m_pDelegate = new CX360LobbyAsyncOperation_LobbyConnect( pHostGameDetails, m_xsiQosInfo );
			return;
		}

		Assert( !m_pDelegate );
		m_eState = AOS_FAILED;
	}

protected:
	XSESSION_INFO m_xsiQosInfo;
	XNADDR const *m_pXnaddr;
	XNKID const *m_pXnkid;
	XNKEY const *m_pXnkey;
};

class CX360LobbyAsyncOperation_LobbyDiscoverAndQosAndConnect : public CX360AsyncOperationDelegating< CX360LobbyAsyncOperation >
{
public:
	explicit CX360LobbyAsyncOperation_LobbyDiscoverAndQosAndConnect( uint64 const &uiSessionId )
	{
		XNKID xnkid = ( XNKID const & ) uiSessionId;
		int iCtrlr = XBX_GetPrimaryUserId();

		DWORD dwSize = 0;
		DWORD ret = g_pMatchExtensions->GetIXOnline()->XSessionSearchByID( xnkid, iCtrlr, &dwSize, NULL, NULL );
		if( ret != ERROR_INSUFFICIENT_BUFFER )
		{
			DevWarning( "XSessionSearchByID failed to determine size (code 0x%08X)\n", ret );
			Assert( 0 );

			m_eState = AOS_FAILED;
			m_result = ret;
			return;
		}

		// Allocate buffer
		m_bufSearchResultHeader.EnsureCapacity( dwSize );
		ZeroMemory( m_bufSearchResultHeader.Base(), dwSize );

		ret = g_pMatchExtensions->GetIXOnline()->XSessionSearchByID( xnkid, iCtrlr, &dwSize,
			(XSESSION_SEARCHRESULT_HEADER *) m_bufSearchResultHeader.Base(), 
			&m_xOverlapped );
		if( ret != ERROR_IO_PENDING )
		{
			DevWarning( "XSessionSearchByID failed to initiate search (code 0x%08X)\n", ret );
			Assert( 0 );

			m_eState = AOS_FAILED;
			m_result = ret;
			return;
		}
	}

public:
	virtual void OnFinished( DWORD dwRetCode, DWORD dwResult )
	{
		TX360AsyncOperationDelegating::OnFinished( dwRetCode, dwResult );
		if ( m_eState == AOS_SUCCEEDED && !m_pDelegate )
		{
			// We successfully resolved our XNKID
			XSESSION_SEARCHRESULT_HEADER *xsh = ( XSESSION_SEARCHRESULT_HEADER * ) m_bufSearchResultHeader.Base();

			if( xsh->dwSearchResults >= 1)
			{
				m_lobby.m_xiInfo = xsh->pResults[0].info;
				m_pDelegate = new CX360LobbyAsyncOperation_LobbyQosAndConnect( m_lobby.m_xiInfo );
				return;
			}
		}
		
		Assert( !m_pDelegate );
		m_eState = AOS_FAILED;
	}

protected:
	CUtlBuffer m_bufSearchResultHeader;
};

void MMX360_LobbyConnect( KeyValues *pSettings, IX360LobbyAsyncOperation **ppOperation )
{
	Assert( pSettings );
	Assert( ppOperation );

	if ( ppOperation )
		*ppOperation = NULL;
	else
		return;

	if ( !pSettings )
		return;

	// Parse the information from the settings
	uint64 uiSessionID = pSettings->GetUint64( "options/sessionid", 0ull );
	char const *szSessionInfo = pSettings->GetString( "options/sessioninfo", NULL );
	KeyValues *pSessionHostData = ( KeyValues * ) pSettings->GetPtr( "options/sessionHostData", NULL );

	// Handle the case when we have session info and it is up to date
	XSESSION_INFO xsi = {0};
	if ( szSessionInfo )
		MMX360_SessionInfoFromString( xsi, szSessionInfo );

	if ( ( const uint64 & ) xsi.sessionID != ( const uint64 & ) uiSessionID )
	{
		// Need to discover session info first
		*ppOperation = new CX360LobbyAsyncOperation_LobbyDiscoverAndQosAndConnect( uiSessionID );
		return;
	}
	
	if ( !pSessionHostData && !Q_stricmp( "LIVE", pSettings->GetString( "system/network" ) ) )
	{
		// QOS and connect using the host-supplied information
		*ppOperation = new CX360LobbyAsyncOperation_LobbyQosAndConnect( xsi );
		return;
	}

	// Otherwise use the client-settings hoping that they match the host-settings
	if ( !pSessionHostData )
		pSessionHostData = pSettings;

	// Connect immediately with session information that we have
	*ppOperation = new CX360LobbyAsyncOperation_LobbyConnect( pSessionHostData, xsi );
}

//
// Host migration implementation
//

struct CX360LobbyMigrateHandleImpl
{
	CX360LobbyMigrateOperation_t *m_pListener;
	XSESSION_INFO m_xsi;

	static void Finished( XOVERLAPPED *pxOverlapped, void *pvThis )
	{
		CX360LobbyMigrateHandleImpl *pHandle = ( CX360LobbyMigrateHandleImpl * ) pvThis;
		if ( pHandle->m_pListener )
		{
			XGetOverlappedResult( pxOverlapped, &pHandle->m_pListener->m_ret, FALSE );
			pHandle->m_pListener->m_pLobby->m_xiInfo = pHandle->m_xsi;
			pHandle->m_pListener->m_bFinished = true;
		}
		delete pHandle;
	}
};

CX360LobbyMigrateHandle_t MMX360_LobbyMigrateHost( CX360LobbyObject &lobby, CX360LobbyMigrateOperation_t *pOperation )
{
	if ( !XNetXnKidIsSystemLink( &lobby.m_xiInfo.sessionID ) )
	{
		CX360LobbyMigrateHandleImpl *pHandleImpl = new CX360LobbyMigrateHandleImpl;
		pHandleImpl->m_pListener = pOperation;
		pHandleImpl->m_xsi = lobby.m_xiInfo;
		if ( pOperation )
		{
			pOperation->m_bFinished = false;
			pOperation->m_ret = ERROR_IO_PENDING;
			pOperation->m_pLobby = &lobby;
		}

		// Submit XSessionMigrateHost
		DWORD ret = g_pMatchExtensions->GetIXOnline()->XSessionMigrateHost( lobby.m_hHandle, XBX_GetPrimaryUserId(),
			&pHandleImpl->m_xsi,
			MMX360_NewOverlappedDormant( CX360LobbyMigrateHandleImpl::Finished, pHandleImpl ) );

		if ( ret != ERROR_SUCCESS && ret != ERROR_IO_PENDING )
		{
			DevWarning( "XSessionMigrateHost(hosting) failed (code 0x%08X)\n", ret );
			Assert( 0 );
		}

		return pHandleImpl;
	}
	else
	{
		DevWarning( "XSessionMigrateHost(hosting) unavailable because the session is SystemLink\n" );
		return NULL;
	}
}

CX360LobbyMigrateHandle_t MMX360_LobbyMigrateClient( CX360LobbyObject &lobby, XSESSION_INFO const &xsiNewHost, CX360LobbyMigrateOperation_t *pOperation )
{
	if ( !XNetXnKidIsSystemLink( &lobby.m_xiInfo.sessionID ) )
	{
		CX360LobbyMigrateHandleImpl *pHandleImpl = new CX360LobbyMigrateHandleImpl;
		pHandleImpl->m_pListener = pOperation;
		pHandleImpl->m_xsi = xsiNewHost;
		if ( pOperation )
		{
			pOperation->m_bFinished = false;
			pOperation->m_ret = ERROR_IO_PENDING;
			pOperation->m_pLobby = &lobby;
		}

		// Submit XSessionMigrateHost
		DWORD ret = g_pMatchExtensions->GetIXOnline()->XSessionMigrateHost( lobby.m_hHandle, XUSER_INDEX_NONE,
			&pHandleImpl->m_xsi,
			MMX360_NewOverlappedDormant( CX360LobbyMigrateHandleImpl::Finished, pHandleImpl ) );

		if ( ret != ERROR_SUCCESS && ret != ERROR_IO_PENDING )
		{
			DevWarning( "XSessionMigrateHost(client) failed (code 0x%08X)\n", ret );
			Assert( 0 );
		}

		return pHandleImpl;
	}
	else
	{
		DevWarning( "XSessionMigrateHost(client) unavailable because the session is SystemLink\n" );
		return NULL;
	}
}

CX360LobbyMigrateOperation_t * MMX360_LobbyMigrateSetListener( CX360LobbyMigrateHandle_t hMigrateCall, CX360LobbyMigrateOperation_t *pOperation )
{
	CX360LobbyMigrateHandleImpl *pHandle = ( CX360LobbyMigrateHandleImpl * ) hMigrateCall;
	CX360LobbyMigrateOperation_t *pOldListener = NULL;
	if ( pHandle )
	{
		pOldListener = pHandle->m_pListener;
		if ( pOperation && pHandle->m_pListener )
			*pOperation = *pHandle->m_pListener;
		pHandle->m_pListener = pOperation;
	}
	return pOldListener;
}


//
// Join and Leave implementation
//

static DWORD MMX360_Helper_LobbyJoin( CX360LobbyObject &lobby, XUID xuid )
{
	static const BOOL bPrivate = TRUE;

	int iCtrlr = MMX360_GetUserCtrlrIndex( xuid );
	DevMsg( "Session %llx: ADDED %llx%s\n", lobby.m_uiNonce, xuid, ( iCtrlr >= 0 ) ? " local" : "" );
	
	if ( iCtrlr >= 0 )
	{
		DWORD *pdwUserIndex = new DWORD( iCtrlr );
		return g_pMatchExtensions->GetIXOnline()->XSessionJoinLocal( lobby.m_hHandle,
			1, pdwUserIndex, &bPrivate,
			MMX360_NewOverlappedDormant( OnCompleted_DeleteData< DWORD >, pdwUserIndex ) );
	}
	else
	{
		XUID *pXuids = new XUID( xuid );
		return g_pMatchExtensions->GetIXOnline()->XSessionJoinRemote( lobby.m_hHandle,
			1, pXuids, &bPrivate,
			MMX360_NewOverlappedDormant( OnCompleted_DeleteData< XUID >, pXuids ) );
	}
}

static DWORD MMX360_Helper_LobbyLeave( CX360LobbyObject &lobby, XUID xuid )
{
	int iCtrlr = MMX360_GetUserCtrlrIndex( xuid );
	DevMsg( "Session %llx: LEAVE %llx%s\n", lobby.m_uiNonce, xuid, ( iCtrlr >= 0 ) ? " local" : "" );
	
	if ( iCtrlr >= 0 )
	{
		DWORD *pdwUserIndex = new DWORD( iCtrlr );
		return g_pMatchExtensions->GetIXOnline()->XSessionLeaveLocal( lobby.m_hHandle,
			1, pdwUserIndex,
			MMX360_NewOverlappedDormant( OnCompleted_DeleteData< DWORD >, pdwUserIndex ) );
	}
	else
	{
		XUID *pXuids = new XUID( xuid );
		return g_pMatchExtensions->GetIXOnline()->XSessionLeaveRemote( lobby.m_hHandle,
			1, pXuids,
			MMX360_NewOverlappedDormant( OnCompleted_DeleteData< XUID >, pXuids ) );
	}
}

static void MMX360_Helper_LobbyJoinLeaveMembers( KeyValues *pSettings, CX360LobbyObject &lobby,
												 DWORD (*pfn)( CX360LobbyObject&, XUID ),
												 int idxMachineStart, int idxMachineEnd )
{
	if ( !pSettings )
		return;

	KeyValues *kvMembers = pSettings->FindKey( "members" );
	if ( !kvMembers )
		return;

	int numMachines = kvMembers->GetInt( "numMachines", 0 );
	
	if ( idxMachineEnd < 0 || idxMachineEnd >= numMachines )
		idxMachineEnd = numMachines - 1;

	for ( int k = idxMachineStart; k <= idxMachineEnd; ++ k )
	{
		KeyValues *kvMachine = kvMembers->FindKey( CFmtStr( "machine%d", k ) );
		if ( !kvMachine )
			continue;

		int numPlayers = kvMachine->GetInt( "numPlayers", 0 );
		for ( int j = 0; j < numPlayers; ++ j )
		{
			KeyValues *kvPlayer = kvMachine->FindKey( CFmtStr( "player%d", j ) );
			if ( !kvPlayer )
				continue;

			XUID xuid = kvPlayer->GetUint64( "xuid", 0ull );
			if ( !xuid )
				continue;

			DWORD ret = pfn( lobby, xuid );
			if ( ERROR_SUCCESS != ret && ERROR_IO_PENDING != ret )
			{
				DevWarning( "XSessionJoin/Leave (pfn=%p) failed (code 0x%08X)!\n", pfn, ret );
				Assert( 0 );
			}
		}
	}
}

void MMX360_LobbyJoinMembers( KeyValues *pSettings, CX360LobbyObject &lobby, int idxMachineStart, int idxMachineEnd )
{
	MMX360_Helper_LobbyJoinLeaveMembers( pSettings, lobby, MMX360_Helper_LobbyJoin, idxMachineStart, idxMachineEnd );
}

void MMX360_LobbyLeaveMembers( KeyValues *pSettings, CX360LobbyObject &lobby, int idxMachineStart, int idxMachineEnd )
{
	MMX360_Helper_LobbyJoinLeaveMembers( pSettings, lobby, MMX360_Helper_LobbyLeave, idxMachineStart, idxMachineEnd );
}

bool MMX360_LobbySetActiveGameplayState( CX360LobbyObject &lobby, bool bActive, char const *szSecureServerAddress )
{
	if ( bActive == lobby.m_bXSessionStarted )
	{
		DevWarning( "LobbySetActiveGameplayState called in same state '%s', ignored!\n",
			bActive ? "active" : "inactive" );
		Assert( bActive != lobby.m_bXSessionStarted );
		return false;
	}

	DWORD ret = ERROR_SUCCESS;
	if ( bActive )
	{
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnProfilesWriteOpportunity", "reason", "sessionstart"
			) );

		ret = g_pMatchExtensions->GetIXOnline()->XSessionStart( lobby.m_hHandle, 0, MMX360_NewOverlappedDormant() );

		// Parse the address
		if ( szSecureServerAddress )
		{
			if ( char const *szExternalPeer = StringAfterPrefix( szSecureServerAddress, "SESSIONINFO " ) )
			{
				MMX360_SessionInfoFromString( lobby.m_xiExternalPeer, szExternalPeer );
			}
			else
			{
				netadr_t na;
				na.SetFromString( szSecureServerAddress );
				lobby.m_inaSecure.s_addr = na.GetIPNetworkByteOrder();
			}
		}
	}
	else
	{
		g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues(
			"OnProfilesWriteOpportunity", "reason", "sessionend"
			) );

		// Asynchronous call to XSessionEnd
		ret = g_pMatchExtensions->GetIXOnline()->XSessionEnd( lobby.m_hHandle, MMX360_NewOverlappedDormant() );

		if ( lobby.m_inaSecure.s_addr )
		{
			g_pMatchExtensions->GetIXOnline()->XNetUnregisterInAddr( lobby.m_inaSecure );
			lobby.m_inaSecure.s_addr = 0;
		}

		if ( 0ull != ( const uint64 & ) lobby.m_xiExternalPeer.sessionID )
		{
			g_pMatchExtensions->GetIXOnline()->XNetUnregisterKey( &lobby.m_xiExternalPeer.sessionID );
			memset( &lobby.m_xiExternalPeer, 0, sizeof( lobby.m_xiExternalPeer ) );
		}
	}

	if ( ret != ERROR_SUCCESS && ret != ERROR_IO_PENDING )
	{
		DevWarning( "LobbySetActiveGameplayState failed to become '%s', code = 0x%08X!\n",
			bActive ? "active" : "inactive", ret );
		return false;
	}
	else
	{
		lobby.m_bXSessionStarted = bActive;

		// Update active gameplay sessions stack
		CX360ActiveGameplaySession_t ags( lobby.m_hHandle );
		for ( int k = 0; k < s_arrActiveGameplaySessions.Count(); ++ k )
		{
			CX360ActiveGameplaySession_t const &x = s_arrActiveGameplaySessions[k];
			if ( x.m_hHandle == ags.m_hHandle )
			{
				s_arrActiveGameplaySessions.Remove( k -- );
			}
		}
		
		if ( lobby.m_bXSessionStarted )
		{
			s_arrActiveGameplaySessions.AddToTail( ags );
		}

		return true;
	}
}

void MMX360_CancelOverlapped( XOVERLAPPED *pxOverlapped )
{
	g_pMatchExtensions->GetIXOnline()->XCancelOverlapped( pxOverlapped );
}

#endif
