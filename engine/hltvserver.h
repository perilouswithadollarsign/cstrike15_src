//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef HLTVSERVER_H
#define HLTVSERVER_H
#ifdef _WIN32
#pragma once
#endif

#include "baseserver.h"
#include "hltvclient.h"
#include "hltvdemo.h"
#include "hltvbroadcast.h"
#include "hltvclientstate.h"
#include "clientframe.h"
#include "networkstringtable.h"
#include <ihltv.h>
#include <convar.h>

#define HLTV_BUFFER_VOICE			0	// player voice data
#define HLTV_BUFFER_SOUNDS			1	// unreliable sounds
#define HLTV_BUFFER_TEMPENTS		2	// temporary/event entities
#define	HLTV_BUFFER_RELIABLE		3	// reliable messages
#define HLTV_BUFFER_UNRELIABLE		4	// unreliable messages
#define HLTV_BUFFER_MAX				5	// end marker

// proxy dispatch modes
#define DISPATCH_MODE_OFF			0
#define DISPATCH_MODE_AUTO			1
#define DISPATCH_MODE_ALWAYS		2

extern ConVar tv_debug;

class CHLTVFrame : public CClientFrame
{
public:
	CHLTVFrame();
	virtual ~CHLTVFrame();

	void	Reset(); // resets all data & buffers
	void	FreeBuffers();
	void	AllocBuffers();
	bool	HasData();
	void	CopyHLTVData( const CHLTVFrame &frame );
	virtual bool IsMemPoolAllocated() { return false; }

	uint GetMemSize()const;
public:

	// message buffers:
	bf_write	m_Messages[HLTV_BUFFER_MAX];
};

struct CFrameCacheEntry_s
{
	CClientFrame* pFrame;
	int	nTick;
};

class CDeltaEntityCache
{
	struct DeltaEntityEntry_s
	{
		DeltaEntityEntry_s *pNext;
		int	nDeltaTick;
		int nBits;
	};

public:
	CDeltaEntityCache();
	~CDeltaEntityCache();

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
class IHLTVDirector;

class CHLTVServer : public IGameEventListener2, public CBaseServer, public CClientFrameManager, public IHLTVServer, public IDemoPlayer
{
	friend class CHLTVClientState;

	typedef CBaseServer BaseClass;

public:
	CHLTVServer( uint nInstance, float flSnapshotRate );
	virtual ~CHLTVServer();

public: // CBaseServer interface:
	void	Init (bool bIsDedicated);
	void	Shutdown( void );
	void	Clear( void );
	bool	IsHLTV( void ) const { return true; };
	bool	IsMultiplayer( void ) const { return true; };
	void	FillServerInfo(CSVCMsg_ServerInfo &serverinfo);
	void	GetNetStats( float &avgIn, float &avgOut );
	int		GetChallengeType ( const ns_address &adr );
	const char *GetName( void ) const;
	const char *GetPassword() const;
	const char *GetHltvRelayPassword() const;
	IClient *ConnectClient ( const ns_address &adr, int protocol, int challenge, int authProtocol, 
		const char *name, const char *password, const char *hashedCDkey, int cdKeyLen,
		CUtlVector< CCLCMsg_SplitPlayerConnect_t * > & splitScreenClients, bool isClientLowViolence, CrossPlayPlatform_t clientPlatform,
		const byte *pbEncryptionKey, int nEncryptionKeyIndex ) OVERRIDE;

	virtual bool GetRedirectAddressForConnectClient( const ns_address &adr, CUtlVector< CCLCMsg_SplitPlayerConnect_t* > & splitScreenClients, ns_address *pNetAdrRedirect ) OVERRIDE;

	bool	CheckHltvPasswordMatch( const char *szPasswordProvidedByClient, const char *szServerRequiredPassword, CSteamID steamidClient );

	virtual bool	GetClassBaseline( ServerClass *pClass, SerializedEntityHandle_t *pHandle);

public:
	void	FireGameEvent(IGameEvent *event);
	int		m_nDebugID;
	int		GetEventDebugID( void );

public: // IHLTVServer interface:
	IServer	*GetBaseServer( void );
	IHLTVDirector *GetDirector( void );
	int		GetHLTVSlot( void ); // return entity index-1 of HLTV in game
	float	GetOnlineTime( void ); // seconds since broadcast started
	void	GetLocalStats( int &proxies, int &slots, int &clients ); 
	void	GetGlobalStats( int &proxies, int &slots, int &clients );
	void	GetExternalStats( int &numExternalTotalViewers, int &numExternalLinkedViewers );
	void	GetRelayStats( int &proxies, int &slots, int &clients );

	bool	IsMasterProxy( void ); // true, if this is the HLTV master proxy
	bool	IsTVRelay();			// true if we're running a relay (i.e. this is the opposite of IsMasterProxy()).
	bool	IsDemoPlayback( void ); // true if this is a HLTV demo

	const netadr_t *GetRelayAddress( void ); // returns relay address

	void	BroadcastEvent(IGameEvent *event);
	virtual void	StopRecording( const CGameInfo *pGameInfo = NULL )		{	StopRecordingAndFreeFrames( false, pGameInfo ); }

	//similar to the standard stop recording, but this allows for specifying a tick before which frames can be dropped. This is very useful for
	//shutdown to prevent memory spikes
	void	StopRecordingAndFreeFrames( bool bFreeClientFrames, const CGameInfo *pGameInfo = NULL );

	virtual bool		IsRecording();
    virtual const char* GetRecordingDemoFilename();

	virtual void		StartAutoRecording();

public: // IDemoPlayer interface
	CDemoFile *GetDemoFile();
	int		GetPlaybackStartTick( void );
	int		GetPlaybackTick( void );

	bool	StartPlayback( const char *filename, bool bAsTimeDemo, CDemoPlaybackParameters_t const *pPlaybackParameters, int );
	CDemoPlaybackParameters_t const * GetDemoPlaybackParameters() OVERRIDE { return NULL; }

	bool	IsPlayingBack( void )const; // true if demo loaded and playing back
	bool	IsPlaybackPaused( void )const; // true if playback paused
	bool	IsPlayingTimeDemo( void ) const { return false; } // true if playing back in timedemo mode
	bool	IsSkipping( void ) const { return false; }; // true, if demo player skiiping trough packets
	bool	CanSkipBackwards( void ) const { return true; } // true if demoplayer can skip backwards

	void	SetPlaybackTimeScale( float timescale ); // sets playback timescale
	float	GetPlaybackTimeScale( void ); // get playback timescale

	void	PausePlayback( float seconds ) {}; 
	void	SkipToTick( int tick, bool bRelative, bool bPause ) {};  
	void	SkipToImportantTick( const DemoImportantTick_t *pTick ) {};
	void	ResumePlayback( void ) {}; 
	void	StopPlayback( void ) {};	
	void	InterpolateViewpoint() {};
	netpacket_t *ReadPacket( void ) { return NULL; }

	void	ResetDemoInterpolation( void ) {};

	void SetPacketReadSuspended( bool bSuspendPacketReading ) {};

	void	SetImportantEventData( const KeyValues *pData ) {};
	int		FindNextImportantTick( int nCurrentTick, const char *pEventName = NULL ) { return -1; } // -1 = no next important tick
	int		FindPreviousImportantTick( int nCurrentTick, const char *pEventName = NULL ) { return -1; } // -1 = no previous important tick
	const DemoImportantTick_t *GetImportantTick( int nIndex ) { return NULL; }
	const DemoImportantGameEvent_t *GetImportantGameEvent( const char *pszEventName ) { return NULL; }
	void	ListImportantTicks( void ) {};
	void	ListHighlightData( void ) {};
	void	SetHighlightXuid( uint64 xuid, bool bLowlights ) {};

	bool	ScanDemo( const char* filename, const char* pszMode ) { return false; };

public:
	void	StartMaster(CGameClient *client); // start HLTV server as master proxy
	void	ConnectRelay(const char *address); // connect to other HLTV proxy
	void	StartDemo(const char *filename); // starts playing back a demo file
	void	StartBroadcast();
	void    StartRelay( void ); // start HLTV server as relay proxy
	bool	SendNetMsg( INetMessage &msg, bool bForceReliable = false, bool bVoice = false );
	bool	NETMsg_PlayerAvatarData( const CNETMsg_PlayerAvatarData& msg );
	void	RunFrame();
	void	SetMaxClients( int number );
	void	Changelevel( bool bInactivateClients );

	void	UserInfoChanged( int nClientIndex );
	void	SendClientMessages ( bool bSendSnapshots );
	bool	SendClientMessages( CHLTVClient *pClient );
	CClientFrame *AddNewFrame( CClientFrame * pFrame ); // add new frame, returns HLTV's copy
	void	SignonComplete( void );
	void	LinkInstanceBaselines( void );
	void	BroadcastEventLocal( IGameEvent *event, bool bReliable ); // broadcast event but not to relay proxies
	void	BroadcastLocalChat( const char *pszChat, const char *pszGroup ); // broadcast event but not to relay proxies
	void	BroadcastLocalTitle( CHLTVClient *client = NULL ); // NULL = broadcast to all
	bool	DispatchToRelay( CHLTVClient *pClient);
	bf_write *GetBuffer( int nBuffer);
	CClientFrame *GetDeltaFrame( int nTick );
	CClientFrame *ExpandAndGetClientFrame( int nTick, bool bExact );

	inline  CHLTVClient* Client( int i ) { return static_cast<CHLTVClient*>(m_Clients[i]); }

	//called to add a new frame to HLTV by the server, which will be encoded as a delta frame instead of a raw frame, which will then be held in transit and expanded once
	//it is time to be replayed to clients (helps to save a huge amount of memory for long delays)
	void	AddNewDeltaFrame( CClientFrame *pClientFrame );

	void UpdateHltvExternalViewers( uint32 numTotalViewers, uint32 numLinkedViewers );

	void DumpMem();

	uint GetInstanceIndex()const { return m_nInstanceIndex; }
	float GetSnapshotRate()const { return m_flSnapshotRate; }
	void FixupConvars( CNETMsg_SetConVar_t &convars );
protected:
	virtual bool ShouldUpdateMasterServer();

private:
	CBaseClient *CreateNewClient( int slot );
	void		UpdateTick( void );
	void		UpdateStats( void );
	void		InstallStringTables( void );
	void		UninstallStringTables( void );
	void		RestoreTick( int tick );
	void		EntityPVSCheck( CClientFrame *pFrame );
	void		InitClientRecvTables();
	void		FreeClientRecvTables();
	void		ReadCompleteDemoFile();
	void		ResyncDemoClock();


	//when frames come in, we delta compress them to strip out all of the state that hasn't changed. This is necessary since otherwise the HLTV will have to hold ALL state for all frames
	//that are buffered, which is far too costly
	struct SHLTVDeltaEntity_t
	{
		//a constant used to denote that no packed data is associated with this object
		static const SerializedEntityHandle_t	knNoPackedData = (SerializedEntityHandle_t)-1;

		SHLTVDeltaEntity_t();
		~SHLTVDeltaEntity_t();

		//the original index that we came from
		uint16						m_nSourceIndex;
		//how many proxy recipients should be associated with this entity (note this can be non-zero but have a NULL pointer indicating no value changes)
		uint16						m_nNumRecipients;
		//the serial number of this entity (this might be able to be 16 bits which would be nice)
		int32						m_nSerialNumber;	
		// This is the tick this PackedEntity was created on (-1 means reuse the previous one)
		int32						m_nSnapshotCreationTick;
		//the class that this entity maps to
		ServerClass					*m_pServerClass;	
		//pointer to the new recipient values if different
		CSendProxyRecipients		*m_pNewRecipients;
		//the delta encoded properties that have changed since the last tick. Note that this can be NULL or NoPackedData to represent that certain parts of the reconstructed class are missing
		SerializedEntityHandle_t	m_SerializedEntity;
		//the next entity in our list
		SHLTVDeltaEntity_t			*m_pNext;
	};

	struct SHLTVDeltaFrame_t
	{
		SHLTVDeltaFrame_t();
		~SHLTVDeltaFrame_t();

		//a bit list that contains all of the entities that should just be copied forward
		uint32				*m_pCopyEntities;

		//the client frame which contains a custom snap shot that has been allocated
		CHLTVFrame			*m_pClientFrame;

		//the entities that we have delta compressed, these will be expanded into the snapshot when needed (we only store entries for valid ones)
		SHLTVDeltaEntity_t	*m_pEntities;
		uint32				m_nNumValidEntities;
		//the total number of entities that were in the list so we can expand and have the necessary gaps
		uint32				m_nTotalEntities;

		//our previous snapshot that we delta compressed from, used to expand out the properties when needed
		CFrameSnapshot		*m_pRelativeFrame;

		//This points to the original snapshot it was generated from. This is intended only for development testing/validation
		CFrameSnapshot		*m_pSourceFrame;

		//the next frame in our list (newest is at the tail of the list)
		SHLTVDeltaFrame_t	*m_pNewerDeltaFrame;

		size_t GetMemSize()const;
	};

	//the newest delta frame that we've encoded
	SHLTVDeltaFrame_t		*m_pOldestDeltaFrame;
	SHLTVDeltaFrame_t		*m_pNewestDeltaFrame;

	//the last frame that we took a snapshot from (so we can delta encode subsequent ones)
	CFrameSnapshot			*m_pLastSourceSnapshot;
	CFrameSnapshot			*m_pLastTargetSnapshot;

	//given a source snapshot, this will create a copy of it for the delta encoding, so it has all data except the entities
	CFrameSnapshot*		CloneDeltaSnapshot( const CFrameSnapshot *pCopySnapshot );

	//given a current frame being encoded, and a previous frame that was last encoded, this will handle creating a packed list of the entities and adding this list to
	//the provided HLTV delta frame
	void				CreateDeltaFrameEntities( SHLTVDeltaFrame_t* pOutputEntities, const CFrameSnapshot *pCurrFrame, const CFrameSnapshot *pPrevFrame );

	//given a delta frame, this will expand the snapshot to be a full absolute snapshot
	void				ExpandDeltaFrameToFullFrame( SHLTVDeltaFrame_t *pDeltaFrame );

	//called to handle converting all queued delta frames to full frames up to the specified tick (inclusive). This will handle removing them from
	//the delta queue and adding them to the client frame list
	void				ExpandDeltaFramesToTick( int nTick );

	//called to free all delta frames that are queued
	void				FreeAllDeltaFrames( );

	virtual IDemoStream *GetDemoStream() OVERRIDE { return &m_DemoFile; }
public:

	CGameClient		*m_MasterClient;		// if != NULL, this is the master HLTV 
	CHLTVClientState m_ClientState;
	CHLTVDemoRecorder m_DemoRecorder;			// HLTV demo object for recording and playback
	CHLTVBroadcast	m_Broadcast;
	CGameServer		*m_Server;		// pointer to source server (sv.)
	IHLTVDirector	*m_Director;	// HTLV director exported by game.dll	
	int				m_nFirstTick;	// first known server tick;
	int				m_nLastTick;	// last tick from AddFrame()
	CHLTVFrame		*m_CurrentFrame; // current delayed HLTV frame
	int				m_nViewEntity;	// the current entity HLTV is tracking
	int				m_nPlayerSlot;	// slot of HLTV client on game server
	CHLTVFrame		m_HLTVFrame;	// all incoming messages go here until Snapshot is made

	bool			m_bSignonState;	// true if connecting to server
	float			m_flStartTime;
	float			m_flFPS;		// FPS the proxy is running;
	int				m_nGameServerMaxClients; // max clients on game server
	float			m_fNextSendUpdateTime;	// time to send next HLTV status messages 
	RecvTable		*m_pRecvTables[MAX_DATATABLES];
	int				m_nRecvTables;
	Vector			m_vPVSOrigin; 
	bool			m_bMasterOnlyMode;

	netadr_t		m_RootServer;		// HLTV root server
	int				m_nGlobalSlots;
	int				m_nGlobalClients;
	int				m_nGlobalProxies;
	int				m_nExternalTotalViewers;
	int				m_nExternalLinkedViewers;

	char			m_DemoEventsBuffer[NET_MAX_DATAGRAM_PAYLOAD];
	bf_write		m_DemoEventWriteBuffer;

	CNetworkStringTableContainer m_NetworkStringTables;

	CDeltaEntityCache				m_DeltaCache;
	CUtlVector<CFrameCacheEntry_s>	m_FrameCache;
	CThreadFastMutex				m_FrameCacheMutex; // locks frame cache

	typedef CUtlMap< uint32, CNETMsg_PlayerAvatarData_t *, int, CDefLess< uint32 > > PlayerAvatarDataMap_t;
	PlayerAvatarDataMap_t m_mapPlayerAvatarData;

	// demoplayer stuff:
	CDemoFile		m_DemoFile;		// for demo playback
	int				m_nStartTick;
	democmdinfo_t	m_LastCmdInfo;
	bool			m_bPlayingBack;
	bool			m_bPlaybackPaused; // true if demo is paused right now
	float			m_flPlaybackRateModifier;
	int				m_nSkipToTick;	// skip to tick ASAP, -1 = off
	const uint		m_nInstanceIndex;
	float			m_flSnapshotRate;
};

enum { HLTV_SERVER_MAX_COUNT = 2 };
extern CHLTVServer *g_pHltvServer[ HLTV_SERVER_MAX_COUNT ];	// The global HLTV server/object. NULL on xbox.
extern bool IsHltvActive();

// given the con-command arguments, selects one or more hltv servers and enumerates them, iterator (of a vector of HLTV servers) style
class CActiveHltvServerSelector
{
protected:
	int m_nIndex, m_nMask;
public:
	CActiveHltvServerSelector( const CCommand &args );

	operator CHLTVServer* ( )
	{
		return m_nIndex < HLTV_SERVER_MAX_COUNT ? g_pHltvServer[ m_nIndex ] : NULL;
	}
	CHLTVServer *operator->()
	{
		return m_nIndex < HLTV_SERVER_MAX_COUNT ? g_pHltvServer[ m_nIndex ] : NULL;
	}
	bool Next()
	{
		if ( m_nIndex >= HLTV_SERVER_MAX_COUNT )
			return false;
		while ( ++m_nIndex < HLTV_SERVER_MAX_COUNT )
		{
			CHLTVServer *hltv = g_pHltvServer[ m_nIndex ];
			if ( ( ( 1 << m_nIndex ) & m_nMask ) && hltv && hltv->IsActive() )
				return true;
		}
		return false; // no more active HLTV instances
	}
	uint GetIndex()const { return m_nIndex; }
protected:
};

template <bool bActiveOnly >
class THltvServerIterator
{
protected:
	int m_nIndex;
public:
	THltvServerIterator( ) { m_nIndex = -1; Next(); }

	operator CHLTVServer* ( )
	{
		return m_nIndex < HLTV_SERVER_MAX_COUNT ? g_pHltvServer[ m_nIndex ] : NULL;
	}
	CHLTVServer *operator->( )
	{
		return m_nIndex < HLTV_SERVER_MAX_COUNT ? g_pHltvServer[ m_nIndex ] : NULL;
	}
	bool Next()
	{
		if ( m_nIndex >= HLTV_SERVER_MAX_COUNT )
			return false;
		while ( ++m_nIndex < HLTV_SERVER_MAX_COUNT )
		{
			CHLTVServer *hltv = g_pHltvServer[ m_nIndex ];
			if ( hltv )
			{
				if ( bActiveOnly )
				{
					if ( hltv->IsActive() )
						return true;
				}
				else
				{
					return true;
				}
			}
		}
		return false; // no more active HLTV instances
	}
	uint GetIndex()const { return m_nIndex; }
protected:
};

typedef THltvServerIterator<true> CActiveHltvServerIterator;
typedef THltvServerIterator<false> CHltvServerIterator;


#endif // HLTVSERVER_H
