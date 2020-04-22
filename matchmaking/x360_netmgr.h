//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef X360_NETMGR_H
#define X360_NETMGR_H
#ifdef _X360
#ifdef _WIN32
#pragma once
#endif


abstract_class IX360NetworkEvents
{
public:
	//
	// A packet has arrived and we have created a secure association for the IN_ADDR,
	// converted to XNADDR and paired it with our registered XNKID/XNKEY.
	//
	// Handler must either:
	//
	//	- open peer connection in passive mode
	//			we have already completed a secure key exchange (we are in CONNECTED
	//			state), just use ConnectionPeerOpenPassive
	//			and return true from OnX360NetConnectionlessPacket
	//
	// or:
	//	- return false, in which case the secure association will be
	//			unregistered and the remote side end of the secure connection
	//			will get into unrecoverable LOST state
	//
	virtual bool OnX360NetConnectionlessPacket( netpacket_t *pkt, KeyValues *msg ) = 0;
	
	//
	// A packet has been received from a remote peer and successfully unpacked,
	// connection is still alive.
	//
	virtual void OnX360NetPacket( KeyValues *msg ) = 0;

	//
	// Connection with remote peer has been disconnected, connection object
	// and all connection records have already been removed, secure association
	// unregistered.
	//
	virtual void OnX360NetDisconnected( XUID xuidRemote ) = 0;
};

class CX360NetworkMgr : public IConnectionlessPacketHandler
{
public:
	explicit CX360NetworkMgr( IX360NetworkEvents *pListener, INetSupport::NetworkSocket_t eSocket );

public:
	void SetListener( IX360NetworkEvents *pListener );

public:
	enum UpdateResult_t { UPDATE_SUCCESS, UPDATE_LISTENER_CHANGED, UPDATE_DESTROYED };
	UpdateResult_t Update();
	void Destroy();
	void DebugPrint();

	bool IsUpdating() const;

public:
	bool ConnectionPeerOpenPassive( XUID xuidRemote, netpacket_t *pktIncoming, XNKID *pxnkidSession = NULL );
	bool ConnectionPeerOpenActive( XUID xuidRemote, XSESSION_INFO const &xRemote );
	
	void ConnectionPeerUpdateXuid( XUID xuidRemoteOld, XUID xuidRemoteNew );

	void ConnectionPeerClose( XUID xuidRemote );
	void ConnectionPeerClose( netpacket_t *pktIncoming );

	void ConnectionPeerSendConnectionless( XUID xuidRemote, KeyValues *pMsg );
	void ConnectionPeerSendMessage( KeyValues *pMsg );

	char const * ConnectionPeerGetAddress( XUID xuidRemote );

	//
	// IConnectionlessPacketHandler
	//
protected:
	virtual bool ProcessConnectionlessPacket( netpacket_t *packet );

	//
	// INetChannelHandler-delegates
	//
public:
	void OnConnectionClosing( INetChannel *pNetChannel );
	void OnConnectionMessage( KeyValues *pMsg );

protected:
	IX360NetworkEvents *m_pListener;
	INetSupport::NetworkSocket_t m_eSocket;

protected:
	struct ConnectionMessageHandler_t : public INetChannelHandler
	{
		explicit ConnectionMessageHandler_t( CX360NetworkMgr *pMgr, INetChannel *pChannel ) : m_pMgr( pMgr ), m_pChannel( pChannel ) {}

	public:
		virtual void ConnectionStart(INetChannel *chan);
		virtual void ConnectionClosing(const char *reason) { m_pMgr->OnConnectionClosing( m_pChannel ); }
		virtual void ConnectionCrashed(const char *reason) { ConnectionClosing( reason ); }
		virtual void PacketStart(int incoming_sequence, int outgoing_acknowledged) {}
		virtual void PacketEnd( void ) {}
		virtual void FileRequested(const char *fileName, unsigned int transferID, bool isReplayDemoFile) {} // other side request a file for download
		virtual void FileReceived(const char *fileName, unsigned int transferID, bool isReplayDemoFile) {} // we received a file
		virtual void FileDenied(const char *fileName, unsigned int transferID, bool isReplayDemoFile) {}	// a file request was denied by other side
		virtual void FileSent(const char *fileName, unsigned int transferID, bool isReplayDemoFile) {}	// we sent a file
		virtual bool ChangeSplitscreenUser( int nSplitScreenUserSlot ) { return true; }

	public:
		CX360NetworkMgr *m_pMgr;
		INetChannel *m_pChannel;
	};

	struct ConnectionInfo_t
	{
		XUID m_xuid;
		IN_ADDR m_inaddr;
		XNADDR m_xnaddr;
		INetChannel *m_pNetChannel;
		ConnectionMessageHandler_t *m_pHandler;
	};
	CUtlMap< XUID, ConnectionInfo_t > m_arrConnections;
	enum State_t
	{
		STATE_IDLE,			// network mgr is idle
		STATE_UPDATING,		// network mgr is in the middle of an update frame function
		STATE_DESTROY_DEFERRED,	// network mgr has been destroyed while in the middle of an update
	} m_eState;
	UpdateResult_t m_eUpdateResult;
};


#endif
#endif

