//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Forward server log lines to remote listeners
//
//=============================================================================//
#include "cbase.h"
#include "server_log_http_dispatcher.h"
#include "gameinterface.h"
#include "matchmaking/imatchframework.h"
#include "engine/inetsupport.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CServerLogDestination
{
public:
	CServerLogDestination( const char* szDestAddress )
	{
		m_sURI.Set( szDestAddress );
		m_flTimeout = 10.0f;
		m_hHTTPRequestHandle = INVALID_HTTPREQUEST_HANDLE;
		m_bIsFinished = false;
		m_unMaxUpdateStringSizeBytes = 250 * k_nKiloByte;
		m_lluUniqueToken = 0llu;
		static ConVarRef rev_LogAddrSecret( "logaddress_token_secret" );
		const char* szSecret = rev_LogAddrSecret.GetString();
		// HACK: Same code in sv_log, but not worth plumbing that through to share for this kinda feature... Doesn't really matter if they drift
		if ( szSecret && !StringIsEmpty( szSecret ) )
		{
			CRC64_ProcessBuffer( &m_lluUniqueToken, szSecret, V_strlen( szSecret ) );
			if ( !m_lluUniqueToken )
				m_lluUniqueToken = 1;
		}
		Reset();
	}
	~CServerLogDestination()
	{
		if ( m_hHTTPRequestHandle )
			steamgameserverapicontext->SteamHTTP()->ReleaseHTTPRequest( m_hHTTPRequestHandle );
	}
	
	bool BRequestPending( void ) const { return m_hHTTPRequestHandle != INVALID_HTTPREQUEST_HANDLE; }
	bool BIsFinished( void ) const { return m_bIsFinished;  }
	bool SendUpdate( int32 iFromTIck, int32 iToTick, CServerLogHTTPDispatcher::LogLinesList_t::IndexType_t idxEndOfUpdate, const char* pLogLines, size_t unUpdateStrLenBytes );
	CServerLogHTTPDispatcher::LogLinesList_t::IndexType_t LastUpdateIndex() const { return m_idxLastAcknowledgedUpdate; }
	void Reset( void ) // Called during cleanup between maps or at shutdown
	{
		m_idxPendingUpdate = CServerLogHTTPDispatcher::LogLinesList_t::InvalidIndex();
		m_idxLastAcknowledgedUpdate = CServerLogHTTPDispatcher::LogLinesList_t::InvalidIndex();

		if ( m_hHTTPRequestHandle )
			steamgameserverapicontext->SteamHTTP()->ReleaseHTTPRequest( m_hHTTPRequestHandle );
		m_hHTTPRequestHandle = NULL;
	}

	size_t MaxUpdateStringSizeBytes() const { return m_unMaxUpdateStringSizeBytes; }
	void DumpStatusToConsole( void ) const;
private:
	void Steam_OnHTTPRequestCompleted( HTTPRequestCompleted_t *p, bool bError );

	CUtlString m_sURI; // Base url for this destination

	// Keep track of the linked list node from the last successful update so we can continue where we left off
	CServerLogHTTPDispatcher::LogLinesList_t::IndexType_t m_idxLastAcknowledgedUpdate;
	CServerLogHTTPDispatcher::LogLinesList_t::IndexType_t m_idxPendingUpdate; 

	HTTPRequestHandle m_hHTTPRequestHandle; // Pending request if any
	float m_flTimeout; // Timeout passed to steamhttp callbacks
	size_t m_unMaxUpdateStringSizeBytes;
	bool m_bIsFinished; // This listener is done for some reason, remove us from queue next update
	uint64 m_lluUniqueToken; // Some unique string set before adding a listener to be sent along with the request. Here to match behavior of udp system.
	// SteamHTTP members for callback handling
	CCallResult< CServerLogDestination, HTTPRequestCompleted_t > m_CallbackOnHTTPRequestCompleted;
};

static CServerLogHTTPDispatcher g_ServerLogHTTPDispatcher;
CServerLogHTTPDispatcher* GetServerLogHTTPDispatcher( void ) { return &g_ServerLogHTTPDispatcher; }

CON_COMMAND( logaddress_add_http, "Set URI of a listener to receive logs via http post. Wrap URI in double quotes." )
{
	if ( args.ArgC() != 2 )
	{
		ConMsg( "logaddress_add_http: Invalid parameters, must be URI of a listener to receive logs wrapped in quotes, eg \"http://127.0.0.1:3001\"\n" );
		return;
	}

	GetServerLogHTTPDispatcher()->AddListener( args[ 1 ] );
}

CON_COMMAND( logaddress_delall_http, "Remove all http listeners from the dispatch list." )
{
	GetServerLogHTTPDispatcher()->RemoveAllListeners();
}

CON_COMMAND( logaddress_list_http, "List all URIs currently receiving server logs" )
{
	GetServerLogHTTPDispatcher()->DumpListenersToConsole();
}

CServerLogHTTPDispatcher::CServerLogHTTPDispatcher():
	CAutoGameSystemPerFrame( "ServerLogHTTPDispatcher" )
{ 
	Reset();
}

void CServerLogHTTPDispatcher::BuildTimestampString( void )
{
	double flTimeSpentLogging = Plat_FloatTime() - m_loggingStartPlatTime;
	tm now;
	Plat_ConvertToLocalTime( ( m_localTimeLoggingStart + (uint32)flTimeSpentLogging ), &now );
	uint32 msecs = ( flTimeSpentLogging - ( uint32 )flTimeSpentLogging ) * 1000;
	m_strTimeStampPrefix.Format( "%02u/%02u/%04u - %02u:%02u:%02u.%03u",
										now.tm_mon + 1, now.tm_mday, 1900 + now.tm_year,
										now.tm_hour, now.tm_min, now.tm_sec, msecs );
}

bool CServerLogHTTPDispatcher::LogForHTTPListeners( const char* szLogLine )
{
	if ( !szLogLine || StringIsEmpty( szLogLine ) )
		return false;

	if ( m_strLogLinesThisTick.IsEmpty() )
	{
		BuildTimestampString();
	}

	m_strLogLinesThisTick.AppendFormat( "%s - %s", m_strTimeStampPrefix.Get(), szLogLine );
	return true;
}

void CServerLogHTTPDispatcher::AddListener( const char* szURI )
{
	m_vecListeners.AddToTail( new CServerLogDestination( szURI ) );
}

void CServerLogHTTPDispatcher::RemoveAllListeners( void )
{
	m_vecListeners.PurgeAndDeleteElements();
}

void CServerLogHTTPDispatcher::DumpListenersToConsole( void ) const
{
	FOR_EACH_VEC( m_vecListeners, i )
	{
		CServerLogDestination *pDest = m_vecListeners[ i ];
		if ( pDest )
		{
			pDest->DumpStatusToConsole();
			ConMsg( "\tlast acknowledged tick %d\n", m_llLogPerTick[ pDest->LastUpdateIndex() ].iTick );
		}
	}
}

void CServerLogHTTPDispatcher::Shutdown()
{
	Reset();
	m_vecListeners.PurgeAndDeleteElements();
}

void CServerLogHTTPDispatcher::LevelInitPreEntity()
{
#if 0 // autoadd self for debugging
	if ( m_vecListeners.Count() == 0 )
		engine->ServerCommand( "log on;logaddress_add_http \"http://127.0.0.1:3000\"\n" );
#endif
	m_localTimeLoggingStart = Plat_GetTime();
	m_loggingStartPlatTime = Plat_FloatTime();
}

void CServerLogHTTPDispatcher::LevelShutdownPreEntity()
{
	Reset();
}

void CServerLogHTTPDispatcher::Reset( void )
{
	m_strServerLog.Clear();
	m_llLogPerTick.Purge();
	m_strLogLinesThisTick.Clear();

	FOR_EACH_VEC( m_vecListeners, i )
	{
		m_vecListeners[i]->Reset();
	}
#if defined DBGFLAG_ASSERT
	m_dbgLastAllocedHandle = LogLinesList_t::InvalidIndex();
#endif 
}

void CServerLogHTTPDispatcher::PreClientUpdate()
{
	if ( m_vecListeners.Count() == 0 )
		return;

	// Can't do any work before we have steamhttp
	if ( steamgameserverapicontext == NULL || !steamgameserverapicontext->SteamHTTP() || !steamgameserverapicontext->SteamGameServer() )
		return;

	AssertOnce( engine->IsLogEnabled() );
	static bool bWarnOnce = false;
	if ( !engine->IsLogEnabled() && !bWarnOnce )
	{
		Warning( "Server log http listener registered, but logging is not turned on for this server! Start recording the log before registering any http destinations\n" );
		bWarnOnce = true;
		return;
	}

	// If we've crossed a tick boundary, add the spew for that period to the in-memory log
	int32 iTickLastLogged = m_llLogPerTick.Tail() != LogLinesList_t::InvalidIndex() ? m_llLogPerTick [ m_llLogPerTick.Tail() ].iTick : 0;
	if ( gpGlobals->tickcount > iTickLastLogged && !m_strLogLinesThisTick.IsEmpty() )
	{
		// Record offset from start of our log string for this tick's lines and its length. 
		LogLinesList_t::IndexType_t newIdx = m_llLogPerTick.AddToTail( LogLineStartForTick_t ( gpGlobals->tickcount, m_strServerLog.Length(), m_strLogLinesThisTick.Length() ) );

		// Never reuse handles in this LL unless we tear down the log between levels or otherwise know there are no listeners holding on to them
		NOTE_UNUSED( newIdx );
#if defined DBGFLAG_ASSERT
		Assert( newIdx > m_dbgLastAllocedHandle || m_dbgLastAllocedHandle == LogLinesList_t::InvalidIndex() );
		m_dbgLastAllocedHandle = newIdx;
#endif

		// Append this tick's lines to the log and clear
		m_strServerLog.Append( m_strLogLinesThisTick.Get() );
		// Don't free memory for this as it's a frequently used bucket. Keep alloc at whatever high water mark its reached until level transition
		m_strLogLinesThisTick.SetLength( 0 ); 
	
		// Mark this as the most recent tick for any updates being sent below
		iTickLastLogged = gpGlobals->tickcount;
	}

	if ( m_llLogPerTick.Tail() != LogLinesList_t::InvalidIndex() )
	{
		FOR_EACH_VEC_BACK( m_vecListeners, i )
		{
			CServerLogDestination* pDest = m_vecListeners[ i ];
			if ( pDest->BRequestPending() ) // Skip requests still waiting to hear back or time out
				continue;

			if ( pDest->BIsFinished() ) // Clean up listeners no longer needing updates
			{
				delete m_vecListeners[ i ];
				m_vecListeners.Remove( i );
				continue;
			}

			// If we have new lines for this listener, send an update
			LogLinesList_t::IndexType_t idxLast = pDest->LastUpdateIndex();
			if ( idxLast == m_llLogPerTick.Tail() )
				continue; // they have the latest

			LogLinesList_t::IndexType_t idxFrom = LogLinesList_t::InvalidIndex();
			if ( idxLast == LogLinesList_t::InvalidIndex() )
				idxFrom = m_llLogPerTick.Head(); // never acknowledged an update
			else
				idxFrom = m_llLogPerTick.Next( idxLast ); // start with the next entry after their last acknowledgment 
			Assert( idxFrom != LogLinesList_t::InvalidIndex() );

			LogLineStartForTick_t &updateStart = m_llLogPerTick[ idxFrom ];
			Assert( updateStart.iTick <= iTickLastLogged ); 

			LogLinesList_t::IndexType_t idxTo = idxFrom;
			size_t unUpdateStringSize = updateStart.unLength;
			while ( unUpdateStringSize < pDest->MaxUpdateStringSizeBytes() && m_llLogPerTick.Next( idxTo ) != LogLinesList_t::InvalidIndex() )
			{
				idxTo = m_llLogPerTick.Next( idxTo );
				unUpdateStringSize += m_llLogPerTick[ idxTo ].unLength;
			}
			
			Assert( idxFrom != LogLinesList_t::InvalidIndex() && idxTo != LogLinesList_t::InvalidIndex() );
			pDest->SendUpdate( updateStart.iTick, m_llLogPerTick[idxTo].iTick, idxTo , m_strServerLog.Get() + updateStart.unOffsetStart, unUpdateStringSize );
		}
	}
}

bool CServerLogDestination::SendUpdate( int32 iFromTick, int32 iToTick, CServerLogHTTPDispatcher::LogLinesList_t::IndexType_t idxEndOfUpdate, const char* pLogLines, size_t unUpdateStrLenBytes )
{
	m_hHTTPRequestHandle = steamgameserverapicontext->SteamHTTP()->CreateHTTPRequest( k_EHTTPMethodPOST, m_sURI.Get() );
	steamgameserverapicontext->SteamHTTP()->SetHTTPRequestNetworkActivityTimeout( m_hHTTPRequestHandle, m_flTimeout );
	steamgameserverapicontext->SteamHTTP()->SetHTTPRequestHeaderValue( m_hHTTPRequestHandle, "X-Tick-Start", CNumStr( iFromTick ).String() );
	steamgameserverapicontext->SteamHTTP()->SetHTTPRequestHeaderValue( m_hHTTPRequestHandle, "X-Tick-End", CNumStr( iToTick ).String() );
	steamgameserverapicontext->SteamHTTP()->SetHTTPRequestHeaderValue( m_hHTTPRequestHandle, "X-SteamID", steamgameserverapicontext->SteamGameServer()->GetSteamID().Render() );
	INetSupport::ServerInfo_t serverInfo;
	if ( INetSupport *pINetSupport = ( INetSupport * )g_pMatchFramework->GetMatchExtensions()->GetRegisteredExtensionInterface( INETSUPPORT_VERSION_STRING ) )
	{
		pINetSupport->GetServerInfo( &serverInfo );
		steamgameserverapicontext->SteamHTTP()->SetHTTPRequestHeaderValue( m_hHTTPRequestHandle, "X-Server-Addr", serverInfo.m_netAdr.ToString() );
	}
	steamgameserverapicontext->SteamHTTP()->SetHTTPRequestHeaderValue( m_hHTTPRequestHandle, "X-Timestamp", CNumStr( Plat_MSTime() ).String() );
	if ( m_lluUniqueToken )
		steamgameserverapicontext->SteamHTTP()->SetHTTPRequestHeaderValue( m_hHTTPRequestHandle, "X-Server-Unique-Token", CNumStr( m_lluUniqueToken ).String() );
	steamgameserverapicontext->SteamHTTP()->SetHTTPRequestRawPostBody( m_hHTTPRequestHandle, "text/plain", ( uint8* )pLogLines, unUpdateStrLenBytes );
	SteamAPICall_t hCall = NULL;
	if ( m_hHTTPRequestHandle && steamgameserverapicontext->SteamHTTP()->SendHTTPRequest( m_hHTTPRequestHandle, &hCall ) && hCall )
	{
		m_CallbackOnHTTPRequestCompleted.Set( hCall, this, &CServerLogDestination::Steam_OnHTTPRequestCompleted );
		m_idxPendingUpdate = idxEndOfUpdate;
	}
	else
	{
		if ( m_hHTTPRequestHandle )
			steamgameserverapicontext->SteamHTTP()->ReleaseHTTPRequest( m_hHTTPRequestHandle );
		m_hHTTPRequestHandle = NULL;

		Steam_OnHTTPRequestCompleted( NULL, true );
		return false;
	}

	return true;
}

void CServerLogDestination::DumpStatusToConsole( void ) const
{
	ConMsg( "%s - %s pending request\n", m_sURI.Get(), BRequestPending() ? "has" : "no" );
}

void CServerLogDestination::Steam_OnHTTPRequestCompleted( HTTPRequestCompleted_t *p, bool bError )
{
	if ( !m_hHTTPRequestHandle || !p || ( p->m_hRequest != m_hHTTPRequestHandle ) )
		return;

	bool bSuccess = !bError && p->m_eStatusCode == k_EHTTPStatusCode200OK;
	if ( bSuccess )
	{
		m_idxLastAcknowledgedUpdate = m_idxPendingUpdate;
	}
	else
	{
		switch ( p->m_eStatusCode )
		{
			case k_EHTTPStatusCodeInvalid:
			{
				Msg( "Internal HTTP error posting to url %s\n", m_sURI.Get() );
			}
			break;
			case k_EHTTPStatusCode410Gone:
			{
				Msg( "Listener %s sent code 410, removing from update list.\n", m_sURI.Get() );
				m_bIsFinished = true;
			}
			break;

			case k_EHTTPStatusCode205ResetContent:
				m_idxLastAcknowledgedUpdate = CServerLogHTTPDispatcher::LogLinesList_t::InvalidIndex(); // Destination is requesting entire log again
			break;

			default:
			{
				Msg( "Error posting to url %s, error code %d\n", m_sURI.Get(), p->m_eStatusCode );
			}
			break;
		}
	}

	m_idxPendingUpdate = CServerLogHTTPDispatcher::LogLinesList_t::InvalidIndex();
	steamgameserverapicontext->SteamHTTP()->ReleaseHTTPRequest( p->m_hRequest );
	m_hHTTPRequestHandle = NULL;
}
