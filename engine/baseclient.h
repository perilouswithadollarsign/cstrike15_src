//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef BASECLIENT_H
#define BASECLIENT_H
#ifdef _WIN32
#pragma once
#endif

#include <const.h>
#include <checksum_crc.h>
#include <iclient.h>
#include <protocol.h>
#include <iservernetworkable.h>
#include <bspfile.h>
#include <keyvalues.h>
#include <bitvec.h>
#include <igameevents.h>
#include "smartptr.h"
#include "userid.h"
#include "tier1/bitbuf.h"
#include "steam/steamclientpublic.h"
#include "tier1/utlarray.h"
#include "netmessages.h"

// class CClientFrame;
class CBaseServer;
class CClientFrame;
struct player_info_s;
class CFrameSnapshot;
class CEventInfo;
class CCommand;
struct NetMessageCvar_t;
class CHLTVServer;

struct Spike_t
{
public:
	Spike_t() : 
		m_nBits( 0 )
	{
		m_szDesc[ 0 ] = 0;
	}
	char	m_szDesc[ 64 ];
	int		m_nBits;
};

class CNetworkStatTrace
{
public:
	CNetworkStatTrace() : 
		m_nMinWarningBytes( 0 ), m_nStartBit( 0 ), m_nCurBit( 0 )
	{
	}
	int						m_nMinWarningBytes;
	int						m_nStartBit;
	int						m_nCurBit;
	CUtlVector< Spike_t >	m_Records;
};


class CBaseClient : public IGameEventListener2, public IClient
{
	typedef struct CustomFile_s
	{
		CRC32_t			crc;	//file CRC
		unsigned int	reqID;	// download request ID
	} CustomFile_t;

public:
	CBaseClient();
	virtual ~CBaseClient();

public:

	int			GetPlayerSlot() const { return m_nClientSlot; };
	int			GetUserID() const { return m_UserID; };
	const USERID_t	GetNetworkID() const;
	const char		*GetClientName() const { return m_Name; };
	INetChannel		*GetNetChannel() { return m_NetChannel; };
	IServer			*GetServer() { return (IServer*)m_Server; };
	CHLTVServer		*GetHltvServer();
	CHLTVServer		*GetAnyConnectedHltvServer();
	const char		*GetUserSetting(const char *cvar) const;
	const char		*GetNetworkIDString() const;
	uint64			GetClientXuid() const;
	const char		*GetFriendsName() const { return m_FriendsName; }
	void			UpdateName( const char *pszDefault );

	virtual	void	Connect(const char * szName, int nUserID, INetChannel *pNetChannel, bool bFakePlayer, CrossPlayPlatform_t clientPlatform, const CMsg_CVars *pVecCvars = NULL );
	virtual	void	Inactivate( void )OVERRIDE;
	virtual	void	Reconnect( void )OVERRIDE;
	virtual	void	Disconnect( const char *reason ) OVERRIDE;
	virtual bool	CheckConnect( void );
	virtual bool	ChangeSplitscreenUser( int nSplitScreenUserSlot );

	virtual	void	SetRate( int nRate, bool bForce );
	virtual	int		GetRate( void ) const;
	
	virtual void	SetUpdateRate( float fUpdateRate, bool bForce ); // override;
	virtual float	GetUpdateRate( void ) const; // override;

	virtual void	Clear( void );
	virtual void	DemoRestart( void ); // called when client started demo recording

	virtual	int		GetMaxAckTickCount() const;

	virtual bool	ExecuteStringCommand( const char *s );
	virtual bool	SendNetMsg( INetMessage &msg, bool bForceReliable = false, bool bVoice = false );
	
	virtual void	ClientPrintf ( PRINTF_FORMAT_STRING const char *fmt, ...) FMTFUNCTION( 2, 3 );

	virtual	bool	IsConnected( void ) const { return m_nSignonState >= SIGNONSTATE_CONNECTED; };	
	virtual	bool	IsSpawned( void ) const { return m_nSignonState >= SIGNONSTATE_NEW; };	
	virtual	bool	IsActive( void ) const { return m_nSignonState == SIGNONSTATE_FULL; };
	virtual	bool	IsFakeClient( void ) const { return m_bFakePlayer; };
	virtual	bool	IsHLTV( void ) const { return m_bIsHLTV; }
#if defined( REPLAY_ENABLED )
	virtual bool	IsReplay( void ) const { return m_bIsReplay; }
#else
	virtual bool	IsReplay( void ) const { return false; }
#endif  // REPLAY_ENABLED
	// Is an actual human player or splitscreen player (not a bot and not a HLTV slot)
	virtual bool	IsHumanPlayer() const;
	virtual	bool	IsHearingClient( int index ) const { return false; };
	virtual	bool	IsProximityHearingClient( int index ) const { return false; };
	virtual	bool	IsLowViolenceClient( void ) const { return m_bLowViolence; }

	virtual void	SetMaxRoutablePayloadSize( int nMaxRoutablePayloadSize );

	virtual bool	IsSplitScreenUser( void ) const { return m_bSplitScreenUser; }
	virtual CrossPlayPlatform_t GetClientPlatform() const { return m_ClientPlatform; }

public: // Message Handlers
	
	bool NETMsg_Tick( const CNETMsg_Tick& msg );
	bool NETMsg_StringCmd( const CNETMsg_StringCmd& msg );
	bool NETMsg_SignonState( const CNETMsg_SignonState& msg );
	virtual bool NETMsg_PlayerAvatarData( const CNETMsg_PlayerAvatarData& msg );
	virtual bool NETMsg_SetConVar( const CNETMsg_SetConVar& msg );

	virtual bool CLCMsg_ClientInfo( const CCLCMsg_ClientInfo& msg );
	virtual bool CLCMsg_Move( const CCLCMsg_Move& msg ) { Assert( 0 ); return true; }
	virtual bool CLCMsg_VoiceData( const CCLCMsg_VoiceData& msg ) { Assert( 0 ); return true; }

	bool CLCMsg_BaselineAck( const CCLCMsg_BaselineAck& msg );
	virtual bool CLCMsg_ListenEvents( const CCLCMsg_ListenEvents& msg );
	virtual bool CLCMsg_RespondCvarValue( const CCLCMsg_RespondCvarValue& msg ) { Assert( 0 ); return true; }
	bool CLCMsg_LoadingProgress( const CCLCMsg_LoadingProgress& msg );	
	bool CLCMsg_SplitPlayerConnect( const CCLCMsg_SplitPlayerConnect& msg );
	virtual bool CLCMsg_FileCRCCheck( const CCLCMsg_FileCRCCheck& msg ) { Assert( 0 ); return true; }
	virtual bool CLCMsg_CmdKeyValues( const CCLCMsg_CmdKeyValues& msg );
	virtual bool CLCMsg_HltvReplay( const CCLCMsg_HltvReplay &msg ) { return false; }

	virtual bool SVCMsg_UserMessage( const CSVCMsg_UserMessage &msg ) { return true; }


	enum
	{
		NETMSG_Tick,
		NETMSG_StringCmd,
		NETMSG_SetConVar,
		NETMSG_SignonState,
		NETMSG_ClientInfo,
		NETMSG_Move,
		NETMSG_VoiceData,
		NETMSG_BaselineAck,
		NETMSG_ListenEvents,
		NETMSG_RespondCvarValue,
		NETMSG_SplitPlayerConnect,
		NETMSG_FileCRCCheck,
		NETMSG_LoadingProgress,
		NETMSG_CmdKeyValues,
		NETMSG_PlayerAvatarData,
		NETMSG_HltvReplay,
		NETMSG_UserMessage,
		NETMSG_Max
	};

	CUtlArray< CNetMessageBinder, NETMSG_Max > m_NetMessages;

	virtual void	ConnectionStart(INetChannel *chan) OVERRIDE;
	virtual void	ConnectionStop()OVERRIDE;

public: // IGameEventListener
	virtual void	FireGameEvent( IGameEvent *event ) OVERRIDE { FireGameEvent( event, false ); }
	void FireGameEvent( IGameEvent *event, bool bPassthrough );
	int				m_nDebugID;
	virtual int		GetEventDebugID( void );

public:

	virtual	bool	UpdateAcknowledgedFramecount(int tick);
	virtual bool	ShouldSendMessages( void );
	virtual void	UpdateSendState( void );
			void	ForceFullUpdate( void ) { UpdateAcknowledgedFramecount(-1); }
	
	virtual bool	FillUserInfo( player_info_s &userInfo );
	virtual void	UpdateUserSettings();
	virtual void	WriteGameSounds(bf_write &buf, int nMaxSounds);
	
	virtual CClientFrame *GetDeltaFrame( int nTick );
	virtual bool	SendSnapshot( CClientFrame *pFrame );
	virtual bool	SendServerInfo( void );
	virtual void	OnSteamServerLogonSuccess( uint32 externalIP );
	virtual bool	SendSignonData( void );
	virtual void	SpawnPlayer( void );
	virtual void	ActivatePlayer( void );
	virtual void	SetName( const char * name );
	virtual void	SetUserCVar( const char *cvar, const char *value);
	virtual void	FreeBaselines();
	virtual bool	IgnoreTempEntity( CEventInfo *event );
	
	void			SetSteamID( const CSteamID &steamID );

	int				GetSignonState() const { return m_nSignonState; }
	void			SetSignonState( int nState );

	bool			IsTracing() const;
	void			SetTraceThreshold( int nThreshold );
	void			TraceNetworkData( bf_write &msg, PRINTF_FORMAT_STRING char const *fmt, ... ) FMTFUNCTION( 3, 4 );
	void			TraceNetworkMsg( int nBits, PRINTF_FORMAT_STRING char const *fmt, ... ) FMTFUNCTION( 3, 4 );

	bool			IsFullyAuthenticated( void ) { return m_bFullyAuthenticated; }
	void			SetFullyAuthenticated( void ) { m_bFullyAuthenticated = true; }

	void			SplitScreenDisconnect( const CCommand &args );

	void			DisconnectSplitScreenUser( CBaseClient *pSplitClient );

	void			ApplyConVars( const CMsg_CVars& list, bool bCreateIfNotExisting );
	
	void			FillSignOnFullServerInfo( class CNETMsg_SignonState_t& state );
	bool			IsSplitScreenPartner( const CBaseClient *pOther ) const;
	virtual IClient	*GetSplitScreenOwner() { return m_pAttachedTo; }
	
	virtual int		GetNumPlayers();
	virtual bool	StartHltvReplay( const HltvReplayParams_t &params ) OVERRIDE { return false; } // not implemented for most clients
	virtual void	StopHltvReplay() OVERRIDE { }
	virtual int		GetHltvReplayDelay() OVERRIDE { return 0; }
	virtual bool	CanStartHltvReplay() OVERRIDE { return false; }
	virtual void	ResetReplayRequestTime() OVERRIDE { }
	virtual CBaseClient *GetPropCullClient() { return this; }
	void			OverrideSignonStateTransparent( int nState ){ m_nSignonState = nState; }
protected:
	virtual bool	ProcessSignonStateMsg(int state, int spawncount);
	virtual void	PerformDisconnection( const char *pReason );

	void			OnPlayerAvatarDataChanged();

private:	
	void			OnRequestFullUpdate( char const *pchReason );
	int				GetAvailableSplitScreenSlot() const;
	void			SendFullConnectEvent();

public:

	// One of the subservient split screen users?
	bool			m_bSplitScreenUser;
	bool			m_bSplitAllowFastDisconnect;
	int				m_nSplitScreenPlayerSlot;
	CBaseClient		*m_SplitScreenUsers[ MAX_SPLITSCREEN_CLIENTS ];
	CBaseClient		*m_pAttachedTo; // If this is a split screen user, this is the "master"
	bool			m_bSplitPlayerDisconnecting;

	// Array index in svs.clients:
	int				m_nClientSlot;	
	// entity index of this client (different from clientSlot+1 in HLTV and Replay mode):
	int				m_nEntityIndex;	
	
	int				m_UserID;			// identifying number on server
	char			m_Name[MAX_PLAYER_NAME_LENGTH];			// for printing to other people
	char			m_GUID[SIGNED_GUID_LEN + 1]; // the clients CD key
	CNETMsg_PlayerAvatarData_t m_msgAvatarData;	// Client avatar

	CSteamID		m_SteamID;			// This is valid when the client is authenticated
	
	uint32			m_nFriendsID;		// client's friends' ID
	char			m_FriendsName[MAX_PLAYER_NAME_LENGTH];

	KeyValues		*m_ConVars;			// stores all client side convars
	bool			m_bConVarsChanged;	// true if convars updated and not changes process yet
	bool			m_bSendServerInfo;	// true if we need to send server info packet to start connect
	CBaseServer		*m_Server;			// pointer to server object
	bool			m_bIsHLTV;			// if this a HLTV proxy ?
	CHLTVServer		*m_pHltvSlaveServer;
#if defined( REPLAY_ENABLED )
	bool			m_bIsReplay;		// if this is a Replay proxy ?
#endif

	// Client sends this during connection, so we can see if
	//  we need to send sendtable info or if the .dll matches
	CRC32_t			m_nSendtableCRC;

	// a client can have couple of cutomized files distributed to all other players
	CustomFile_t	m_nCustomFiles[MAX_CUSTOM_FILES];
	int				m_nFilesDownloaded;	// counter of how many files we downloaded from this client

	//===== NETWORK ============
	INetChannel		*m_NetChannel;		// The client's net connection.

private:
	int				m_nSignonState;		// connection state

public:
	int				m_nDeltaTick;		// -1 = no compression.  This is where the server is creating the
										// compressed info from.
	int				m_nStringTableAckTick; // Highest tick acked for string tables (usually m_nDeltaTick, except when it's -1)
	int				m_nSignonTick;		// tick the client got his signon data
	CSmartPtr<CFrameSnapshot,CRefCountAccessorLongName> m_pLastSnapshot;	// last send snapshot
	int				m_nLoadingProgress;	// 0..100 progress, only valid during loading

	CFrameSnapshot	*m_pBaseline;			// current entity baselines as a snapshot
	int				m_nBaselineUpdateTick;	// last tick we send client a update baseline signal or -1
	CBitVec<MAX_EDICTS>	m_BaselinesSent;	// baselines sent with last update
	int				m_nBaselineUsed;		// 0/1 toggling flag, singaling client what baseline to use
	
		
	// This is used when we send out a nodelta packet to put the client in a state where we wait 
	// until we get an ack from them on this packet.
	// This is for 3 reasons:
	// 1. A client requesting a nodelta packet means they're screwed so no point in deluging them with data.
	//    Better to send the uncompressed data at a slow rate until we hear back from them (if at all).
	// 2. Since the nodelta packet deletes all client entities, we can't ever delta from a packet previous to it.
	// 3. It can eat up a lot of CPU on the server to keep building nodelta packets while waiting for
	//    a client to get back on its feet.
	int				m_nForceWaitForTick;
	
	bool			m_bFakePlayer;		// JAC: This client is a fake player controlled by the game DLL
	bool			m_bReceivedPacket;	// true, if client received a packet after the last send packet
	bool			m_bLowViolence;		// true if client is in low-violence mode (L4D server needs to know)

	bool			m_bFullyAuthenticated;

	// The datagram is written to after every frame, but only cleared
	// when it is sent out to the client.  overflow is tolerated.

	// Time when we should send next world state update ( datagram )
	double         m_fNextMessageTime;   
	// Default time to wait for next message
	float          m_fSnapshotInterval;

	CrossPlayPlatform_t m_ClientPlatform;

	// Keep these as class members instead of a local variable so that we don't reallocate the
	// strings and their buffers every time.
	CSVCMsg_TempEntities_t m_tempentsmsg;
	CSVCMsg_PacketEntities_t m_packetmsg;

private:
	void				StartTrace( bf_write &msg );
	void				EndTrace( bf_write &msg );


	CNetworkStatTrace	m_Trace;
};



#endif // BASECLIENT_H
