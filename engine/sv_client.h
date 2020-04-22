//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// CGameClient: represents a player client in a game server
//
//===========================================================================//

#ifndef SV_CLIENT_H
#define SV_CLIENT_H

#ifdef _WIN32
#pragma once
#endif

#include "const.h"
#include "bitbuf.h"
#include "net.h"
#include "checksum_crc.h"
#include "event_system.h"
#include "utlvector.h"
#include "bitvec.h"
#include "protocol.h"
#include <inetmsghandler.h>
#include "baseclient.h"
#include "clientframe.h"
#include <soundinfo.h>

struct HltvReplayStats_t
{
	enum FailEnum_t
	{
		FAILURE_ALREADY_IN_REPLAY,
		FAILURE_TOO_FREQUENT,
		FAILURE_NO_FRAME,
		FAILURE_NO_FRAME2,
		FAILURE_CANNOT_MATCH_DELAY,
		FAILURE_FRAME_NOT_READY,
		NUM_FAILURES
	};
	uint nClients;
	uint nStartRequests;
	uint nSuccessfulStarts;
	uint nStopRequests;
	uint nAbortStopRequests;
	uint nUserCancels;
	uint nFullReplays;
	uint nNetAbortReplays;
	uint nFailedReplays[NUM_FAILURES];
	HltvReplayStats_t()
	{
		Reset();
	}

	void Reset()
	{
		V_memset( this, 0, sizeof( *this ) );
		nClients = 1;
	}
	void operator += ( const HltvReplayStats_t &other )
	{
		this->nClients			+= other.nClients;
		this->nStartRequests	+= other.nStartRequests;
		this->nSuccessfulStarts	+= other.nSuccessfulStarts;
		this->nStopRequests		+= other.nStopRequests;
		this->nUserCancels		+= other.nUserCancels;
		this->nAbortStopRequests += other.nAbortStopRequests;
		this->nFullReplays		+= other.nFullReplays;
		this->nNetAbortReplays += other.nNetAbortReplays;
		for ( int i = 0; i < NUM_FAILURES; ++i )
			this->nFailedReplays[i] += other.nFailedReplays[i];
	}
	const char *AsString()const;
};

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class	INetChannel;
class	CClientFrame;
class	CFrameSnapshot;
class	CClientMsgHandler;
struct	edict_t;
struct	SoundInfo_t;
class	KeyValues;
class	CHLTVServer;
class	CReplayServer;
class	CPerClientLogoInfo;
class	CCommand;
class 	CSVCMsg_Sounds;
class	CHLTVClient;

//-----------------------------------------------------------------------------
// CGameClient: represents a player client in a game server
//-----------------------------------------------------------------------------
class CGameClient : public CBaseClient, public CClientFrameManager
{
	typedef CBaseClient BaseClass;

public:
	CGameClient(int slot, CBaseServer *pServer);
	~CGameClient();

	// INetMsgHandler interface
	void ConnectionClosing( const char *reason );
	void ConnectionCrashed(const char *reason);
	
	void PacketStart	(int incoming_sequence, int outgoing_acknowledged);
	void PacketEnd( void );
	
	void FileReceived( const char *fileName, unsigned int transferID, bool bIsReplayDemoFile = false );
	void FileRequested(const char *fileName, unsigned int transferID, bool bIsReplayDemoFile = false );
	void FileDenied( const char *fileName, unsigned int transferID, bool bIsReplayDemoFile = false );
	void FileSent( const char *fileName, unsigned int transferID, bool bIsReplayDemoFile = false );
	
	bool ProcessConnectionlessPacket( netpacket_t *packet );

	// IClient interface
	void	Connect(const char * szName, int nUserID, INetChannel *pNetChannel, bool bFakePlayer, CrossPlayPlatform_t clientPlatform, const CMsg_CVars *pVecCvars ) OVERRIDE;
	void	Inactivate( void );
	void	Reconnect( void );
	void	Disconnect( const char *reason ) OVERRIDE;
	bool	CheckConnect( void );
	
	void	SetRate( int nRate, bool bForce );
	void	SetUpdateRate( float fUpdateRate, bool bForce ); // override;
	
	virtual	bool	IsHearingClient( int index ) const;
	virtual	bool	IsProximityHearingClient( int index ) const;

	void	Clear( void );

	bool	SendNetMsg( INetMessage &msg, bool bForceReliable = false, bool bVoice = false );
	bool	ExecuteStringCommand( const char *s );
	// IGameEventListener
	void	FireGameEvent( IGameEvent *event ) OVERRIDE;

public: // IClientMessageHandlers
	
	virtual bool CLCMsg_ClientInfo( const CCLCMsg_ClientInfo& msg ) OVERRIDE;
	virtual bool CLCMsg_Move( const CCLCMsg_Move& msg ) OVERRIDE;
	virtual bool CLCMsg_VoiceData( const CCLCMsg_VoiceData& msg ) OVERRIDE;

	virtual bool CLCMsg_RespondCvarValue( const CCLCMsg_RespondCvarValue& msg ) OVERRIDE;
	virtual bool CLCMsg_FileCRCCheck( const CCLCMsg_FileCRCCheck& msg ) OVERRIDE;
	virtual bool CLCMsg_CmdKeyValues( const CCLCMsg_CmdKeyValues& msg ) OVERRIDE;

	virtual bool CLCMsg_HltvReplay( const CCLCMsg_HltvReplay &msg ) OVERRIDE;

	virtual bool SVCMsg_UserMessage( const CSVCMsg_UserMessage &msg ) OVERRIDE;
	
public:

	void	UpdateUserSettings( void );
	bool	UpdateAcknowledgedFramecount(int tick);

	void	WriteGameSounds( bf_write &buf, int nMaxSounds );
	virtual bool	ProcessSignonStateMsg(int state, int spawncount);
	bool	SendSnapshot( CClientFrame *pFrame ) OVERRIDE;
	bool	SendHltvReplaySnapshot( CClientFrame * pFrame );
	bool	ShouldSendMessages( void );
	void	SpawnPlayer( void );
	bool	SendSignonData( void );
	void	ActivatePlayer( void );
	
	void	SetupHltvFrame( int nServerTick );
	void	SetupPackInfo( CFrameSnapshot *pSnapshot );
	void	SetupPrevPackInfo();
	
	void	DownloadCustomizations();
	void	WriteViewAngleUpdate( void );
	CClientFrame *GetDeltaFrame( int nTick );
	CClientFrame *GetSendFrame();
	void	SendSound( SoundInfo_t &sound, bool isReliable );
	void	GetReplayData( int& ticks, int& entity);
	bool	IgnoreTempEntity( CEventInfo *event );
	const CCheckTransmitInfo* GetPrevPackInfo();
	virtual bool StartHltvReplay( const HltvReplayParams_t &params ) OVERRIDE;
	virtual bool CanStartHltvReplay() OVERRIDE;
	virtual void ResetReplayRequestTime() OVERRIDE;
	void StepHltvReplayStatus( int nServerTick );
	virtual int GetHltvReplayDelay() OVERRIDE { return m_nHltvReplayDelay; }
	virtual const char * GetHltvReplayStatus()const OVERRIDE { return m_HltvReplayStats.AsString(); }
	virtual void StopHltvReplay() OVERRIDE;
	bool	IsHltvReplay() { return m_nHltvReplayDelay > 0; }
	//CHLTVServer *GetCurrentHltvReplayServerSource() { return IsHltvReplay() ? m_pHltvReplayServer : NULL; }
	virtual CBaseClient *GetPropCullClient() OVERRIDE;
protected:
	virtual void	PerformDisconnection( const char *pReason );

private:
	bool	IsEngineClientCommand( const CCommand &args ) const;
	int		FillSoundsMessage( CSVCMsg_Sounds &msg, int nMaxSounds );
			
public:

	bool								m_bVoiceLoopback; // if true, client wants own voice loopback
	CPlayerBitVec						m_VoiceStreams;	  // Which other clients does this guy's voice stream go to?
	CPlayerBitVec						m_VoiceProximity; // Should we use proximity for this guy?

	int						m_LastMovementTick;	// for move commands

	int						m_nSoundSequence;	// increases with each reliable sound

	// Identity information.
	edict_t					*edict;				// EDICT_NUM(clientnum+1)
	CUtlVector<SoundInfo_t>	m_Sounds;			// game sounds
		
	const edict_t			*m_pViewEntity;		// View Entity (camera or the client itself)

	CClientFrame			*m_pCurrentFrame;	// last added frame

	CCheckTransmitInfo		m_PackInfo;

	bool					m_bIsInReplayMode;
	CCheckTransmitInfo		m_PrevPackInfo;		// Used to speed up CheckTransmit.
	CBitVec<MAX_EDICTS>		m_PrevTransmitEdict;
	float					m_flTimeClientBecameFullyConnected;
	double					m_flLastClientCommandQuotaStart;
	int						m_numClientCommandsInQuota;

	int					m_nHltvReplayDelay;
	CHLTVServer			*m_pHltvReplayServer;
	int					m_nHltvReplayStopAt;
	int					m_nHltvReplayStartAt;
	int					m_nHltvReplaySlowdownBeginAt;
	int					m_nHltvReplaySlowdownEndAt;
	float				m_flHltvReplaySlowdownRate;
	int					m_nHltvLastSendTick;	// last send tick, don't send ticks twice
	float				m_flHltvLastReplayRequestTime; // tick at which the last time replay was requested

	CUtlVector< INetMessage * > m_HltvQueuedMessages; // it'd probably be more optimal, memory-wise, to make a linked list, but a vector is more readable and debuggable


	HltvReplayStats_t	m_HltvReplayStats;
};

#endif // SV_CLIENT_H
