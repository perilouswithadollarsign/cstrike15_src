//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HLTVCLIENT_H
#define HLTVCLIENT_H
#ifdef _WIN32
#pragma once
#endif

#include "baseclient.h"

class CHLTVServer;

class CHLTVClient : public CBaseClient
{
	typedef CBaseClient BaseClass;

public:
	CHLTVClient(int slot, CBaseServer *pServer);
	virtual ~CHLTVClient();

	// INetMsgHandler interface
	void ConnectionClosing( const char *reason );
	void ConnectionCrashed(const char *reason);

	void PacketStart(int incoming_sequence, int outgoing_acknowledged);
	void PacketEnd( void );

	void FileReceived( const char *fileName, unsigned int transferID, bool bIsReplayDemoFile = false );
	void FileRequested(const char *fileName, unsigned int transferID, bool bIsReplayDemoFile = false );
	void FileDenied(const char *fileName, unsigned int transferID, bool bIsReplayDemoFile = false );
	void FileSent(const char *fileName, unsigned int transferID, bool bIsReplayDemoFile = false );
	
	bool ProcessConnectionlessPacket( netpacket_t *packet );
	
	// IClient interface
	bool	ExecuteStringCommand( const char *s );
	void	SpawnPlayer( void );
	bool	ShouldSendMessages( void );
	bool	SendSnapshot( CClientFrame * pFrame ) OVERRIDE;
	bool	SendSignonData( void );
	
	void	SetRate( int nRate, bool bForce );
	void	SetUpdateRate( float fUpdaterate, bool bForce ); // override;
	void	UpdateUserSettings();
	
public: // IClientMessageHandlers
		
	
	virtual bool CLCMsg_RespondCvarValue( const CCLCMsg_RespondCvarValue& msg ) OVERRIDE;
	virtual bool CLCMsg_FileCRCCheck( const CCLCMsg_FileCRCCheck& msg ) OVERRIDE;

	virtual bool NETMsg_SetConVar( const CNETMsg_SetConVar& msg ) OVERRIDE;
	virtual bool NETMsg_PlayerAvatarData( const CNETMsg_PlayerAvatarData& msg ) OVERRIDE { return true; }
	
	virtual bool CLCMsg_Move( const CCLCMsg_Move& msg ) OVERRIDE;
	virtual bool CLCMsg_ClientInfo( const CCLCMsg_ClientInfo& msg ) OVERRIDE;
	virtual bool CLCMsg_VoiceData( const CCLCMsg_VoiceData& msg ) OVERRIDE;
	virtual bool CLCMsg_ListenEvents( const CCLCMsg_ListenEvents& msg ) OVERRIDE;

public:
	CClientFrame *GetDeltaFrame( int nTick );

protected:
	virtual bool	ProcessSignonStateMsg(int state, int spawncount) OVERRIDE;
	
public:
	CHLTVServer *m_pHLTV;
	int		m_nLastSendTick;	// last send tick, don't send ticks twice
	double	m_fLastSendTime;	// last net time we send a packet
	char	m_szPassword[128];	// client password
	double	m_flLastChatTime;	// last time user send a chat text
	bool	m_bNoChat;			// if true don't send chat message to this client
	char	m_szChatGroup[128];	// client password
};


#endif // HLTVCLIENT_H
