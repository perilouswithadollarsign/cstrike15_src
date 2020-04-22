//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef REPLAYCLIENT_H
#define REPLAYCLIENT_H
#ifdef _WIN32
#pragma once
#endif

#include "baseclient.h"

class CReplayServer;

class CReplayClient : public CBaseClient
{
	typedef CBaseClient BaseClass;

public:
	CReplayClient(int slot, CBaseServer *pServer);
	virtual ~CReplayClient();

	// INetMsgHandler interface
	void ConnectionClosing( const char *reason );
	void ConnectionCrashed(const char *reason);

	void PacketStart(int incoming_sequence, int outgoing_acknowledged);
	void PacketEnd( void );

	void FileReceived( const char *fileName, unsigned int transferID, bool isReplayDemoFile );
	void FileRequested(const char *fileName, unsigned int transferID, bool isReplayDemoFile );
	void FileDenied(const char *fileName, unsigned int transferID, bool isReplayDemoFile );
	void FileSent(const char *fileName, unsigned int transferID, bool isReplayDemoFile );
	
	bool ProcessConnectionlessPacket( netpacket_t *packet );
	
	// IClient interface
	bool	ExecuteStringCommand( const char *s );
	void	SpawnPlayer( void );
	bool	ShouldSendMessages( void );
	bool	SendSnapshot( CClientFrame * pFrame );
	bool	SendSignonData( void );
	
	void	SetRate( int nRate, bool bForce );
	void	SetUpdateRate( float fUpdateRate, bool bForce );
	void	UpdateUserSettings();
	
public: // IClientMessageHandlers
	
	virtual bool NETMsg_SetConVar( const CNETMsg_SetConVar& msg ) OVERRIDE;

	virtual bool CLCMsg_ClientInfo( const CCLCMsg_ClientInfo& msg ) OVERRIDE;
	virtual bool CLCMsg_Move( const CCLCMsg_Move& msg ) OVERRIDE;
	virtual bool CLCMsg_VoiceData( const CCLCMsg_VoiceData& msg ) OVERRIDE;
	virtual bool CLCMsg_ListenEvents( const CCLCMsg_ListenEvents& msg ) OVERRIDE;
	
	virtual bool CLCMsg_RespondCvarValue( const CCLCMsg_RespondCvarValue& msg ) OVERRIDE;
	virtual bool CLCMsg_FileCRCCheck( const CCLCMsg_FileCRCCheck& msg ) OVERRIDE;

public:
	CClientFrame *GetDeltaFrame( int nTick );
	
public:
	int		m_nLastSendTick;	// last send tick, don't send ticks twice
	double	m_fLastSendTime;	// last net time we send a packet
	char	m_szPassword[64];	// client password
	double	m_flLastChatTime;	// last time user send a chat text
	bool	m_bNoChat;			// if true don't send chat message to this client
	char	m_szChatGroup[64];	// client password
	CReplayServer *m_pReplay;
};


#endif // REPLAYCLIENT_H
