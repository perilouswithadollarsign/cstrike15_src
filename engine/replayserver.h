//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef REPLAYSERVER_H
#define REPLAYSERVER_H
#ifdef _WIN32
#pragma once
#endif

#include "baseserver.h"
#include "replayclient.h"
#include "replaydemo.h"
#include "clientframe.h"
#include "networkstringtable.h"
#include "replayhistorymanager.h"
#include "dt_recv.h"
#include <ireplay.h>
#include <convar.h>

#define REPLAY_BUFFER_DIRECTOR		0	// director commands
#define	REPLAY_BUFFER_RELIABLE		1	// reliable messages
#define REPLAY_BUFFER_UNRELIABLE	2	// unreliable messages
#define REPLAY_BUFFER_VOICE			3	// player voice data
#define REPLAY_BUFFER_SOUNDS		4	// unreliable sounds
#define REPLAY_BUFFER_TEMPENTS		5	// temporary/event entities
#define REPLAY_BUFFER_MAX			6	// end marker

// proxy dispatch modes
#define DISPATCH_MODE_OFF			0
#define DISPATCH_MODE_AUTO			1
#define DISPATCH_MODE_ALWAYS		2

extern ConVar replay_debug;

class CReplayFrame : public CClientFrame
{
public:
	CReplayFrame();
	virtual ~CReplayFrame();

	void	Reset(); // resets all data & buffers
	void	FreeBuffers();
	void	AllocBuffers();
	bool	HasData();
	void	CopyReplayData( CReplayFrame &frame );
	virtual bool IsMemPoolAllocated() { return false; }

public:

	// message buffers:
	bf_write	m_Messages[REPLAY_BUFFER_MAX];
};

struct CReplayFrameCacheEntry_s
{
	CClientFrame* pFrame;
	int	nTick;
};

class CReplayDeltaEntityCache
{
	struct DeltaEntityEntry_s
	{
		DeltaEntityEntry_s *pNext;
		int	nDeltaTick;
		int nBits;
	};

public:
	CReplayDeltaEntityCache();
	~CReplayDeltaEntityCache();

	void SetTick( int nTick, int nMaxEntities );
	unsigned char* FindDeltaBits( int nEntityIndex, int nDeltaTick, int &nBits );
	void AddDeltaBits( int nEntityIndex, int nDeltaTick, int nBits, bf_write *pBuffer );
	void Flush();

protected:
	int	m_nTick;	// current tick
	int	m_nMaxEntities;	// max entities = length of cache
	int m_nCacheSize;
	DeltaEntityEntry_s* m_Cache[MAX_EDICTS]; // array of pointers to delta entries
};


class CGameClient;
class CGameServer;
class IReplayDirector;
class IReplayHistoryManager;

class CReplayServer : public IGameEventListener2, public CBaseServer, public CClientFrameManager, public IReplayServer
{
	typedef CBaseServer BaseClass;

public:
	CReplayServer();
	virtual ~CReplayServer();

public: // CBaseServer interface:
	virtual bool	IsMultiplayer() const OVERRIDE { return true; };
	virtual bool	IsReplay() const OVERRIDE { return true; };
	virtual void	Init( bool bIsDedicated ) OVERRIDE;
	virtual void	Clear() OVERRIDE;
	virtual void	Shutdown() OVERRIDE;
	virtual void	FillServerInfo(CSVCMsg_ServerInfo &serverinfo) OVERRIDE;
	virtual void	GetNetStats( float &avgIn, float &avgOut ) OVERRIDE;
	virtual int		GetChallengeType ( const ns_address &adr ) OVERRIDE;
	virtual const char *GetName() const OVERRIDE;
	virtual const char *GetPassword() const OVERRIDE;
	IClient *ConnectClient ( const ns_address &adr, int protocol, int challenge, int authProtocol, 
		const char *name, const char *password, const char *hashedCDkey, int cdKeyLen,
		CUtlVector< CCLCMsg_SplitPlayerConnect_t * > & splitScreenClients, bool isClientLowViolence, CrossPlayPlatform_t clientPlatform,
		const byte *pbEncryptionKey, int nEncryptionKeyIndex ) OVERRIDE;

public: // IGameEventListener2 interface:
	void	FireGameEvent( IGameEvent *event );
	int		m_nDebugID;
	int		GetEventDebugID( void );

public: // IReplayServer interface:
	virtual IServer	*GetBaseServer();
	virtual IReplayDirector *GetDirector();
	virtual int		GetReplaySlot(); // return entity index-1 of Replay in game
	virtual float	GetOnlineTime(); // seconds since broadcast started
//	virtual void	GetLocalStats( int &proxies, int &slots, int &clients ); 

	virtual void	BroadcastEvent( IGameEvent *event );

public: // CBaseServer overrides:
	virtual void	SetMaxClients( int number );
	virtual void	UserInfoChanged( int nClientIndex );
	virtual void	SendClientMessages ( bool bSendSnapshots );

public:
	void			StartMaster(CGameClient *client); // start Replay server as master proxy
	void			StartDemo(const char *filename); // starts playing back a demo file
	bool			SendNetMsg( INetMessage &msg, bool bForceReliable = false );
	void			RunFrame();
	void			Changelevel();
	CClientFrame *AddNewFrame( CClientFrame * pFrame ); // add new frame, returns Replay's copy
	void	LinkInstanceBaselines();
	void	BroadcastEventLocal( IGameEvent *event, bool bReliable ); // broadcast event but not to relay proxies
	void	BroadcastLocalChat( const char *pszChat, const char *pszGroup ); // broadcast event but not to relay proxies
	void	BroadcastLocalTitle( CReplayClient *client = NULL ); // NULL = broadcast to all
	bool	DispatchToRelay( CReplayClient *pClient);
	bf_write *GetBuffer( int nBuffer);
	CClientFrame *GetDeltaFrame( int nTick );

	inline CReplayClient* Client( int i ) { return static_cast<CReplayClient*>(m_Clients[i]); }

protected:
	virtual bool ShouldUpdateMasterServer();
	
private:
	CBaseClient *CreateNewClient( int slot );
	void		UpdateTick();
	void		InstallStringTables();
	void		RestoreTick( int tick );
	void		EntityPVSCheck( CClientFrame *pFrame );
	void		InitClientRecvTables();
	void		FreeClientRecvTables();
	void		ResyncDemoClock();
		
public:
	CGameClient		*m_MasterClient;		// if != NULL, this is the master Replay 
	CReplayDemoRecorder m_DemoRecorder;			// Replay demo object for recording and playback
	CGameServer		*m_Server;		// pointer to source server (sv.)
	IReplayDirector	*m_Director;	// Replay director exported by game.dll	
	int				m_nFirstTick;	// first known server tick;
	int				m_nLastTick;	// last tick from AddFrame()
	CReplayFrame	*m_CurrentFrame; // current delayed Replay frame
	int				m_nViewEntity;	// the current entity Replay is tracking
	int				m_nPlayerSlot;	// slot of Replay client on game server
	CReplayFrame	m_ReplayFrame;	// all incoming messages go here until Snapshot is made

	bool			m_bSignonState;	// true if connecting to server
	float			m_flStartTime;
	float			m_flFPS;		// FPS the proxy is running;
	int				m_nGameServerMaxClients; // max clients on game server
	float			m_fNextSendUpdateTime;	// time to send next Replay status messages 
	RecvTable		*m_pRecvTables[MAX_DATATABLES];
	int				m_nRecvTables;
	Vector			m_vPVSOrigin; 
	bool			m_bMasterOnlyMode;

	netadr_t		m_RootServer;		// Replay root server
	int				m_nGlobalSlots;
	int				m_nGlobalClients;
	int				m_nGlobalProxies;

	CNetworkStringTableContainer m_NetworkStringTables;

	CReplayDeltaEntityCache					m_DeltaCache;
	CUtlVector<CReplayFrameCacheEntry_s>	m_FrameCache;

	// demoplayer stuff:
	CDemoFile		m_DemoFile;		// for demo playback
	int				m_nStartTick;
	democmdinfo_t	m_LastCmdInfo;
	bool			m_bPlayingBack;
	bool			m_bPlaybackPaused; // true if demo is paused right now
	float			m_flPlaybackRateModifier;
	int				m_nSkipToTick;	// skip to tick ASAP, -1 = off

	// TODO: Index by SteamID
	CBitVec< 64 >	m_vecClientsDownloading;		// Indexed by client slot - each bit represents whether the given client is currently downloading

	friend class CReplayClientState;
};

extern CReplayServer *replay;	// The global Replay server/object. NULL on xbox.

#endif // REPLAYSERVER_H
