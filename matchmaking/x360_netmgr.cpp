//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mm_framework.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef _X360

#include "tier0/memdbgoff.h"

#include "fmtstr.h"
#include "protocol.h"
#include "proto_oob.h"
#include "netmessages.h"

#ifndef NETMSG_TYPE_BITS
#define NETMSG_TYPE_BITS 6
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



ConVar mm_net_channel_timeout( "mm_net_channel_timeout", "100", FCVAR_DEVELOPMENTONLY );


//
// Network manager for Xbox 360 network layer
//

CX360NetworkMgr::CX360NetworkMgr( IX360NetworkEvents *pListener, INetSupport::NetworkSocket_t eSocket ) :
	m_pListener( pListener ),
	m_eSocket( eSocket ),
	m_arrConnections( DefLessFunc( XUID ) ),
	m_eState( STATE_IDLE ),
	m_eUpdateResult( UPDATE_SUCCESS )
{
}

void CX360NetworkMgr::SetListener( IX360NetworkEvents *pListener )
{
	if ( m_eState == STATE_UPDATING )
	{
		DevMsg( "CX360NetworkMgr::SetListener during network update...\n" );
		if ( m_eUpdateResult == UPDATE_SUCCESS )
			m_eUpdateResult = UPDATE_LISTENER_CHANGED;
	}

	// We allow to set listener, we will just return the change
	// from inside the update loop
	m_pListener = pListener;
}

bool CX360NetworkMgr::IsUpdating() const
{
	return m_eState != STATE_IDLE;
}

CX360NetworkMgr::UpdateResult_t CX360NetworkMgr::Update()
{
	if ( !m_pListener )
		return UPDATE_SUCCESS;

	Assert( m_eState == STATE_IDLE );
	if ( m_eState != STATE_IDLE )
	{
		DevWarning( "CX360NetworkMgr::Update when not idle! (state = %d)\n", m_eState );
		return UPDATE_SUCCESS;
	}

	m_eState = STATE_UPDATING;
	m_eUpdateResult = UPDATE_SUCCESS;

	g_pMatchExtensions->GetINetSupport()->ProcessSocket( m_eSocket, this );

	//
	// Transmit all our active channels
	//
	for ( int idx = m_arrConnections.FirstInorder();
		idx != m_arrConnections.InvalidIndex(); )
	{
		XUID xuidKey = m_arrConnections.Key( idx );
		ConnectionInfo_t const ci = m_arrConnections.Element( idx ); // get a copy
		idx = m_arrConnections.NextInorder( idx );

		if ( !xuidKey && m_arrConnections.Count() > 1 )
			// skip host designation record if there are other records
			// there should be an alias for host too
			continue;

		// If it is a secure connection in LOST state, then shut it down right away
		if ( ci.m_xnaddr.inaOnline.s_addr )
		{
			if ( g_pMatchExtensions->GetIXOnline()->XNetGetConnectStatus( ci.m_inaddr ) == XNET_CONNECT_STATUS_LOST )
			{
				DevWarning( "CX360NetworkMgr::Update - Net channel %p is LOST!\n", ci.m_pNetChannel );
				ConnectionPeerClose( ci.m_xuid );

				// Notify listener
				if ( m_pListener )
				{
					m_pListener->OnX360NetDisconnected( ci.m_xuid );
				}

				continue;
			}
		}

		if ( ci.m_pNetChannel->IsTimedOut() )
		{
			DevWarning( "CX360NetworkMgr::Update - Net channel %p is timed out (%.1f s)!\n",
				ci.m_pNetChannel, mm_net_channel_timeout.GetFloat() );
			ConnectionPeerClose( ci.m_xuid );

			// Notify listener
			if ( m_pListener )
			{
				m_pListener->OnX360NetDisconnected( ci.m_xuid );
			}

			continue;
		}

		ci.m_pNetChannel->Transmit();
	}

	bool bSelfDestroy = ( m_eState == STATE_DESTROY_DEFERRED );
	m_eState = STATE_IDLE;

	if ( bSelfDestroy )
	{
		Destroy();
		return UPDATE_DESTROYED;
	}

	return m_eUpdateResult;
}

void CX360NetworkMgr::Destroy()
{
	// If we are being destroyed in the middle of update loop, then defer the actual destruction
	if ( m_eState == STATE_UPDATING )
	{
		DevMsg( "CX360NetworkMgr::Destroy is deferred...\n" );
		m_pListener = NULL;
		m_eState = STATE_DESTROY_DEFERRED;
		return;
	}
	
	Assert( m_eState == STATE_IDLE );
	DevMsg( "CX360NetworkMgr::Destroy is deleting this object [%p].\n", this );

	// Shutdown all the connections with peers
	while ( m_arrConnections.Count() > 0 )
	{
		ConnectionInfo_t const &ci = m_arrConnections.Element( m_arrConnections.FirstInorder() );
		XUID xuidRemote = ci.m_xuid;
		ConnectionPeerClose( xuidRemote );
	}

	delete this;
}

void CX360NetworkMgr::DebugPrint()
{
	DevMsg( "CX360NetworkMgr\n" );
	DevMsg( "    state:       %d\n", m_eState );
	DevMsg( "    connections: %d\n", m_arrConnections.Count() );
	for ( int k = m_arrConnections.FirstInorder(), j = 0;
		k != m_arrConnections.InvalidIndex();
		++j, k = m_arrConnections.NextInorder( k ) )
	{
		ConnectionInfo_t const *ci;
		ci = &m_arrConnections.Element( k );
		DevMsg( "    connection%d: %llx [%llx] = channel %p\n", j,
			m_arrConnections.Key( k ),
			ci->m_xuid, ci->m_pNetChannel );
	}
}

//
// Declaration of the X360 network message
//

class CX360NetworkMessageBase : public CNetMessage
{
public:											
	virtual bool			ReadFromBuffer( bf_read &buffer ) { Assert( 0 ); return false; }
	virtual bool			WriteToBuffer( bf_write &buffer ) const { Assert( 0 ); return false; }
	virtual const char		*ToString() const { return GetName(); }

	virtual int				GetType() const { return 20; }	// should fit in 5 bits
	virtual const char		*GetName() const { return "CX360NetworkMessage";}
	virtual size_t			GetSize() const { return sizeof( *this ); }
};

class CX360NetworkMessageSend : public CX360NetworkMessageBase
{
public:
	explicit CX360NetworkMessageSend( KeyValues *pData ) : m_pData( pData ) {}

	virtual bool WriteToBuffer( bf_write &buffer ) const;

public:
	KeyValues *m_pData;
};

class CX360NetworkMessageRecv : public CX360NetworkMessageBase
{
public:
	explicit CX360NetworkMessageRecv( CX360NetworkMgr *pMgr ) : m_pMgr( pMgr ), m_pData( NULL ) {}
	~CX360NetworkMessageRecv();

	virtual bool ReadFromBuffer( bf_read &buffer );
	virtual bool Process();

public:
	CX360NetworkMgr *m_pMgr;
	KeyValues *m_pData;

	struct GrowStorage_t
	{
		GrowStorage_t() : m_nBytes( 0 ), m_pbData( NULL ) {}
		~GrowStorage_t() { delete [] m_pbData; m_nBytes = 0; m_pbData = NULL; }
		void Grow( int nBytes )
		{
			if ( m_nBytes < nBytes )
			{
				delete [] m_pbData;
				m_pbData = new unsigned char[ m_nBytes = nBytes ];
			}
		}

		int m_nBytes;
		unsigned char *m_pbData;
	};
	
	GrowStorage_t m_gsKeyValues;
	GrowStorage_t m_gsBinaryData;
};

bool CX360NetworkMessageSend::WriteToBuffer( bf_write &buffer ) const
{
	buffer.WriteUBitLong( GetType(), NETMSG_TYPE_BITS );

	buffer.WriteLong( g_pMatchExtensions->GetINetSupport()->GetEngineBuildNumber() );

	CUtlBuffer bufData;
	bufData.ActivateByteSwapping( !CByteswap::IsMachineBigEndian() );
	m_pData->WriteAsBinary( bufData );

	buffer.WriteLong( bufData.TellMaxPut() );
	buffer.WriteBytes( bufData.Base(), bufData.TellMaxPut() );
	
	if ( KeyValues *kvBinary = m_pData->FindKey( "binary" ) )
	{
		void *pvData = kvBinary->GetPtr( "ptr", NULL );
		int nSize = kvBinary->GetInt( "size", 0 );

		if ( pvData && nSize )
		{
			buffer.WriteLong( nSize );
			if ( !buffer.WriteBytes( pvData, nSize ) )
				return false;
		}
		else
		{
			buffer.WriteLong( 0 );
		}
	}
	else
	{
		buffer.WriteLong( 0 );
	}

	return !buffer.IsOverflowed();
}

bool CX360NetworkMessageRecv::ReadFromBuffer( bf_read &buffer )
{
	if ( buffer.ReadLong() != g_pMatchExtensions->GetINetSupport()->GetEngineBuildNumber() )
		return false;

	int nDataLen = buffer.ReadLong();
	if ( nDataLen <= 0 )
		return false;

	m_gsKeyValues.Grow( nDataLen );
	if ( !buffer.ReadBytes( m_gsKeyValues.m_pbData, nDataLen ) )
		return false;

	if ( !m_pData )
		m_pData = new KeyValues( "" );
	else
		m_pData->Clear();

	CUtlBuffer bufData( m_gsKeyValues.m_pbData, nDataLen, CUtlBuffer::READ_ONLY );
	bufData.ActivateByteSwapping( !CByteswap::IsMachineBigEndian() );
	if ( !m_pData->ReadAsBinary( bufData ) )
	{
		m_pData->Clear();
		return false;
	}

	int nBinaryBytes = buffer.ReadLong();
	if ( nBinaryBytes > 0 )
	{
		m_gsBinaryData.Grow( nBinaryBytes );
		if ( !buffer.ReadBytes( m_gsBinaryData.m_pbData, nBinaryBytes ) )
			return false;

		if ( KeyValues *kvPtr = m_pData->FindKey( "binary/ptr" ) )
			kvPtr->SetPtr( "", m_gsBinaryData.m_pbData );
	}

	return !buffer.IsOverflowed();
}

CX360NetworkMessageRecv::~CX360NetworkMessageRecv()
{
	if ( m_pData )
		m_pData->deleteThis();
	m_pData = NULL;
}

bool CX360NetworkMessageRecv::Process()
{
	m_pMgr->OnConnectionMessage( m_pData );
	return true;
}

void CX360NetworkMgr::ConnectionMessageHandler_t::ConnectionStart( INetChannel *chan )
{
	m_pChannel = chan;

	CX360NetworkMessageRecv *pMsg = new CX360NetworkMessageRecv( m_pMgr );
	m_pChannel->RegisterMessage( pMsg );
}

void CX360NetworkMgr::ConnectionPeerSendMessage( KeyValues *pMsg )
{
	DevMsg( 2, "[NET]  ->  ConnectionPeerSendMessage\n" );
	KeyValuesDumpAsDevMsg( pMsg, 1, 2 );

	for ( int idx = m_arrConnections.FirstInorder();
		idx != m_arrConnections.InvalidIndex();
		idx = m_arrConnections.NextInorder( idx ) )
	{
		XUID xuidKey = m_arrConnections.Key( idx );
		ConnectionInfo_t const &ci = m_arrConnections.Element( idx );
		if ( !xuidKey && m_arrConnections.Count() > 1 )
			// skip host designation record if there are other records
			// there should be an alias for host too
			continue;

		CX360NetworkMessageSend msg( pMsg );
		bool bVoice = !Q_stricmp( pMsg->GetName(), "SysSession::Voice" ); // marks the message as voice-channel-VDP-data

		ci.m_pNetChannel->SendNetMsg( msg, msg.IsReliable(), bVoice );
		ci.m_pNetChannel->Transmit();

		DevMsg( 2, "          ->  %llx (%p : %s)\n", ci.m_xuid, ci.m_pNetChannel,
			ci.m_pNetChannel ? ci.m_pNetChannel->GetRemoteAddress().ToString() : "" );
	}
}

char const * CX360NetworkMgr::ConnectionPeerGetAddress( XUID xuidRemote )
{
	int idxConn = m_arrConnections.Find( xuidRemote );
	if ( idxConn == m_arrConnections.InvalidIndex() )
		return NULL;

	ConnectionInfo_t const &ci = m_arrConnections.Element( idxConn );
	if ( !ci.m_pNetChannel )
		return NULL;

	return ci.m_pNetChannel->GetRemoteAddress().ToString( true );
}

//
// Network communication implementation
//

void CX360NetworkMgr::ConnectionPeerSendConnectionless( XUID xuidRemote, KeyValues *pMsg )
{
	// Client should have started an active connection
	int idxConn = m_arrConnections.Find( xuidRemote );
	if ( idxConn == m_arrConnections.InvalidIndex() )
	{
		DevWarning( "CX360NetworkMgr::ConnectionPeerSendConnectionless( xuidRemote = %llx ) XUID is not registered!\n",
			xuidRemote );
		Assert( 0 );
		return;
	}

	DevMsg( 2, "[NET]  ->  ConnectionPeerSendConnectionless( %llx )\n", xuidRemote );
	KeyValuesDumpAsDevMsg( pMsg, 1, 2 );

	ConnectionInfo_t const &ci = m_arrConnections.Element( idxConn );

	g_pConnectionlessLanMgr->SendPacket( pMsg, ci.m_pNetChannel->GetRemoteAddress().ToString(),
		m_eSocket );
}

bool CX360NetworkMgr::ProcessConnectionlessPacket( netpacket_t *packet )
{
	Assert( m_pListener );

	// Unpack key values
	if ( KeyValues *pMsg = g_pConnectionlessLanMgr->UnpackPacket( packet ) )
	{
		KeyValues::AutoDelete autodelete( pMsg );

		DevMsg( 2, "[NET]  <-  OnX360NetConnectionlessPacket( %s )\n", packet->from.ToString() );
		KeyValuesDumpAsDevMsg( pMsg, 1, 2 );

		if ( m_pListener && m_pListener->OnX360NetConnectionlessPacket( packet, pMsg ) )
			return true;
	}

	// Otherwise this is an unwanted packet, prevent any more packets from this peer
	ConnectionPeerClose( packet );
	return false;
}

void CX360NetworkMgr::OnConnectionMessage( KeyValues *pMsg )
{
	Assert( m_pListener );

	DevMsg( 2, "[NET]  <-  OnConnectionMessage\n" );
	KeyValuesDumpAsDevMsg( pMsg, 1, 2 );

	if ( m_pListener )
	{
		m_pListener->OnX360NetPacket( pMsg );
	}
}

void CX360NetworkMgr::OnConnectionClosing( INetChannel *pNetChannel )
{
	// This is called in several cases:
	//		- we called pNetChannel->Shutdown (we wouldn't find this channel then)
	//		- remote side shut down the channel (we should still find this channel)
	//		- we crashed while pumping buffered messages (we should still find this channel)

	ConnectionInfo_t const *ci = NULL;
	for ( int idx = m_arrConnections.FirstInorder();
		!ci && idx != m_arrConnections.InvalidIndex();
		idx = m_arrConnections.NextInorder( idx ) )
	{
		ConnectionInfo_t const &record = m_arrConnections.Element( idx );
		if ( record.m_pNetChannel == pNetChannel )
			ci = &record;
	}

	if ( !ci )
	{
		// This is a notification from inside Shutdown initiated by us, ignore
		return;
	}

	// Otherwise something went wrong with the connection and we
	// have no other option, but close it and release secure association
	XUID xuidRemote = ci->m_xuid;
	DevMsg( "CX360NetworkMgr::OnConnectionClosing( xuidRemote = %llx, ip = %s )\n",
		xuidRemote, pNetChannel->GetRemoteAddress().ToString( true ) );
	ConnectionPeerClose( xuidRemote );

	// Notify listener
	if ( m_pListener )
	{
		m_pListener->OnX360NetDisconnected( xuidRemote );
	}
}

bool CX360NetworkMgr::ConnectionPeerOpenPassive( XUID xuidRemote, netpacket_t *pktIncoming, XNKID *pxnkidSession /*= NULL*/ )
{
	// Check if we already have the corresponding connection open, close it if it is open
	if ( m_arrConnections.Find( xuidRemote ) != m_arrConnections.InvalidIndex() )
	{
		DevWarning( "CX360NetworkMgr::ConnectionPeerOpenPassive( xuidRemote = %llx, ip = %s ) attempted on a duplicate XUID!\n",
			xuidRemote, pktIncoming->from.ToString() );
		
		ConnectionPeerClose( xuidRemote );

		// Notify listener
		if ( m_pListener )
		{
			m_pListener->OnX360NetDisconnected( xuidRemote );
		}

		DevWarning( "CX360NetworkMgr::ConnectionPeerOpenPassive proceeding after stale duplicate connection closed...\n" );
	}

	//
	// XNetInAddrToXnAddr
	//
	XNADDR xnaddrRemote = {0};
	XNKID xnkidRemote = {0};
	IN_ADDR inaddrRemote = {0};
	inaddrRemote.s_addr = pktIncoming->from.GetIPNetworkByteOrder();
	
	if ( pxnkidSession )
		xnkidRemote = *pxnkidSession;
	
	if ( pxnkidSession ) // if ( pxnkidSession && !XNetXnKidIsSystemLink( pxnkidSession ) )
	{
		if ( int err = g_pMatchExtensions->GetIXOnline()->XNetInAddrToXnAddr( inaddrRemote, &xnaddrRemote, &xnkidRemote ) )
		{
			DevWarning( "CX360NetworkMgr::ConnectionPeerOpenPassive( xuidRemote = %llx, ip = %s ) failed to resolve XNADDR ( code 0x%08X )\n",
				xuidRemote, pktIncoming->from.ToString(), err );
			return false;
		}
	}
	else
	{
		xnaddrRemote.ina = inaddrRemote;
		xnaddrRemote.wPortOnline = pktIncoming->from.GetPort();
	}
	
	if ( pxnkidSession )
		*pxnkidSession = xnkidRemote;

	//
	// Register the connection internally
	//
	ConnectionMessageHandler_t *pHandler = new ConnectionMessageHandler_t( this, NULL );

	INetChannel *pChannel = g_pMatchExtensions->GetINetSupport()->CreateChannel(
		m_eSocket, pktIncoming->from,
		CFmtStr( "MM:%llx", xuidRemote ), pHandler );

	ConnectionInfo_t ci;
	ci.m_xuid = xuidRemote;
	ci.m_inaddr = inaddrRemote;
	ci.m_xnaddr = xnaddrRemote;
	ci.m_pNetChannel = pChannel;
	ci.m_pHandler = pHandler;
	
	m_arrConnections.Insert( xuidRemote, ci );
	
	DevMsg( "CX360NetworkMgr::ConnectionPeerOpenPassive( xuidRemote = %llx, ip = %s ) succeeded (pNetChannel = %p).\n",
		xuidRemote, pktIncoming->from.ToString(), pChannel );
	return true;
}

bool CX360NetworkMgr::ConnectionPeerOpenActive( XUID xuidRemote, XSESSION_INFO const &xRemote )
{
	// Check if we already have the corresponding connection open
	if ( m_arrConnections.Find( xuidRemote ) != m_arrConnections.InvalidIndex() )
	{
		DevWarning( "CX360NetworkMgr::ConnectionPeerOpenActive( xuidRemote = %llx ) attempted on a duplicate XUID!\n",
			xuidRemote );
		Assert( 0 );
		return false;
	}

	//
	// XNetXnAddrToInAddr
	//
	XNADDR xnaddrRemote = xRemote.hostAddress;
	IN_ADDR inaddrRemote;
	if ( 1 ) // if ( !XNetXnKidIsSystemLink( &xRemote.sessionID ) )
	{
		if ( int err = g_pMatchExtensions->GetIXOnline()->XNetXnAddrToInAddr( &xRemote.hostAddress, &xRemote.sessionID, &inaddrRemote ) )
		{
			DevWarning( "CX360NetworkMgr::ConnectionPeerOpenActive( xuidRemote = %llx ) failed to resolve XNADDR ( code 0x%08X )\n",
				xuidRemote, err );
			return false;
		}

		// Initiate secure connection and key exchange
		if ( int err = g_pMatchExtensions->GetIXOnline()->XNetConnect( inaddrRemote ) )
		{
			DevWarning( "CX360NetworkMgr::ConnectionPeerOpenActive( xuidRemote = %llx ) failed to start key exchange ( code 0x%08X )\n",
				xuidRemote, err );
			g_pMatchExtensions->GetIXOnline()->XNetUnregisterInAddr( inaddrRemote );	// need to unregister inaddr, otherwise we will leak a secure association
			return false;
		}
	}
	else
	{
		inaddrRemote = xnaddrRemote.ina;
		xnaddrRemote.inaOnline.s_addr = 0;
	}

	//
	// Register the connection internally
	//
	netadr_t inetAddr;
	inetAddr.SetType( NA_IP );
	inetAddr.SetIPAndPort( inaddrRemote.s_addr, 0 );

	ConnectionMessageHandler_t *pHandler = new ConnectionMessageHandler_t( this, NULL );

	INetChannel *pChannel = g_pMatchExtensions->GetINetSupport()->CreateChannel(
		m_eSocket, inetAddr,
		CFmtStr( "MM:XNKID:%llx", ( const uint64 & ) xRemote.sessionID ), pHandler );

	ConnectionInfo_t ci;
	ci.m_xuid = xuidRemote;
	ci.m_inaddr = inaddrRemote;
	ci.m_xnaddr = xnaddrRemote;
	ci.m_pNetChannel = pChannel;
	ci.m_pHandler = pHandler;

	m_arrConnections.Insert( xuidRemote, ci );

	DevMsg( "CX360NetworkMgr::ConnectionPeerOpenActive( xuidRemote = %llx, xnkid = %llx, ip = %s ) succeeded (pNetChannel = %p), connection %s.\n",
		xuidRemote, ( const uint64 & ) xRemote.sessionID, inetAddr.ToString( true ), pChannel,
		XNetXnKidIsSystemLink( &xRemote.sessionID ) ? "LAN" : "SecureXnet" );

	// Caller is required to transmit a first connectionless packet on net channel address
	// to poke through secure handshaking
	pChannel->SetTimeout( mm_net_channel_timeout.GetFloat() );
	// pChannel->Transmit();

	return true;
}

void CX360NetworkMgr::ConnectionPeerUpdateXuid( XUID xuidRemoteOld, XUID xuidRemoteNew )
{
	Assert( !xuidRemoteOld != !xuidRemoteNew ); // either of the XUIDs must be NULL

	int idxOldRecord = m_arrConnections.Find( xuidRemoteOld );
	Assert( idxOldRecord != m_arrConnections.InvalidIndex() );

	int idxNewRecord = m_arrConnections.Find( xuidRemoteNew );
	// Assert( idxNewRecord == m_arrConnections.InvalidIndex() );

	if ( idxOldRecord != m_arrConnections.InvalidIndex() &&
		 idxNewRecord == m_arrConnections.InvalidIndex() )
	{
		// Adding an alias to a peer network connection:
		// - either found out a host xuid after anonymous connect by NULL xuid
		// - or another client migrated to become a new host and adding a NULL xuid alias for that client
		ConnectionInfo_t &ci = m_arrConnections.Element( idxOldRecord );
		ci.m_xuid = xuidRemoteNew ? xuidRemoteNew : xuidRemoteOld;

		ConnectionInfo_t const ciAlias = ci;	// need to keep a copy on the stack in case insert reallocates memory
		m_arrConnections.Insert( xuidRemoteNew, ciAlias );

		DevMsg( "CX360NetworkMgr::ConnectionPeerUpdateXuid: xuidRemote = %llx + %llx, ip = %s, pNetChannel = %p.\n",
			xuidRemoteOld, xuidRemoteNew, ciAlias.m_pNetChannel->GetRemoteAddress().ToString(), ciAlias.m_pNetChannel );
	}
	else if ( idxOldRecord != m_arrConnections.InvalidIndex() &&
			  idxNewRecord != m_arrConnections.InvalidIndex() &&
			  !xuidRemoteNew )
	{
		// Handling a case when client migrated to become a new host and needs a NULL xuid alias,
		// but the old host is still registered in network manager with a NULL xuid alias:
		// just overwrite the existing NULL alias to point to the new host
		ConnectionInfo_t &ciOld = m_arrConnections.Element( idxOldRecord );
		ConnectionInfo_t &ciNew = m_arrConnections.Element( idxNewRecord );

		DevMsg( "CX360NetworkMgr::ConnectionPeerUpdateXuid: xuidRemote = %llx + %llx, ip = %s, pNetChannel = %p (alias override for host).\n",
			xuidRemoteOld, xuidRemoteNew, ciOld.m_pNetChannel->GetRemoteAddress().ToString(), ciOld.m_pNetChannel );

		ciNew = ciOld;
	}
	else
	{
		DevWarning( "CX360NetworkMgr::ConnectionPeerUpdateXuid: ERROR: xuidRemote = %llx (%s) + %llx (%s)!\n",
			xuidRemoteOld, ( ( idxOldRecord != m_arrConnections.InvalidIndex() ) ? "valid" : "missing" ),
			xuidRemoteNew, ( ( idxNewRecord != m_arrConnections.InvalidIndex() ) ? "valid" : "missing" ) );
		Assert( 0 );
	}
}

void CX360NetworkMgr::ConnectionPeerClose( netpacket_t *pktIncoming )
{
	IN_ADDR inaddrRemote;
	inaddrRemote.s_addr = pktIncoming->from.GetIPNetworkByteOrder();

	g_pMatchExtensions->GetIXOnline()->XNetUnregisterInAddr( inaddrRemote );
}

void CX360NetworkMgr::ConnectionPeerClose( XUID xuidRemote )
{
	//
	// Find the connection record
	//
	int idx = m_arrConnections.Find( xuidRemote );
	if ( !m_arrConnections.IsValidIndex( idx ) )
	{
		DevWarning( "CX360NetworkMgr::ConnectionPeerClose( xuidRemote = %llx ) failed: not registered!\n",
			xuidRemote );
		Assert( 0 );
		return;
	}

	ConnectionInfo_t const ci = m_arrConnections[ idx ]; // get a copy

	//
	// Remove the connection record and host identifier if it refers to the same machine
	//
	m_arrConnections.RemoveAt( idx );

	Assert( ci.m_xuid == xuidRemote || !ci.m_xuid || !xuidRemote );		// NULL record identifies host and only it can be duplicated
	m_arrConnections.Remove( ci.m_xuid );

	int idxHost = m_arrConnections.Find( 0ull );
	if ( m_arrConnections.IsValidIndex( idxHost ) )
	{
		ConnectionInfo_t const &ciHost = m_arrConnections.Element( idxHost );
		if ( ciHost.m_xuid == xuidRemote )
			m_arrConnections.RemoveAt( idxHost );
	}

	//
	// Cleanup resources associated with the connection
	//
	netadr_t inetAddr = ci.m_pNetChannel->GetRemoteAddress();
	ci.m_pNetChannel->Shutdown( "" );
	delete ci.m_pHandler;
	
	if ( ci.m_xnaddr.inaOnline.s_addr )
	{
		g_pMatchExtensions->GetIXOnline()->XNetUnregisterInAddr( ci.m_inaddr );
	}

	DevMsg( "CX360NetworkMgr::ConnectionPeerClose: xuidRemote = %llx, ip = %s, pNetChannel = %p.\n",
		ci.m_xuid, inetAddr.ToString(), ci.m_pNetChannel );
}

#endif
