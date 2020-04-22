//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//
#if !defined( CLIENT_H )
#define CLIENT_H
#ifdef _WIN32
#pragma once
#endif

#include <qlimits.h>
#include <utllinkedlist.h>
#include <convar.h>
#include <checksum_crc.h>
#include <protocol.h>
#include <cdll_int.h>
#include <globalvars_base.h>
#include <soundflags.h>
#include <utlvector.h>
#include "host.h"
#include "event_system.h"
#include "precache.h"
#include "baseclientstate.h"
#include "clientframe.h"
#include "netmessages.h"



struct model_t;

class ClientClass;
class CSfxTable;
class CPureServerWhitelist;

#define	MAX_DEMOS		32
#define	MAX_DEMONAME	32

struct AddAngle
{
	float total;
	float starttime;
};

class CQueuedEntityMessage
{
public:

	bool operator==( const CQueuedEntityMessage &val ) const 
	{ 
		return this->m_msg.ent_index() == val.m_msg.ent_index(); 
	}

	CSVCMsg_EntityMsg m_msg;
};

//-----------------------------------------------------------------------------
// Purpose: CClientState should hold all pieces of the client state
//   The client_state_t structure is wiped completely at every server signon
//-----------------------------------------------------------------------------
class CClientState : public CBaseClientState, public CClientFrameManager
{
	typedef CBaseClientState BaseClass;

	typedef struct CustomFile_s
	{
		CRC32_t			crc;	//file CRC
		unsigned int	reqID;	// download request ID
	} CustomFile_t;

public:
	CClientState();
	void ResetHltvReplayState();
	~CClientState();

public: // IConnectionlessPacketHandler interface:
		
	bool ProcessConnectionlessPacket(struct netpacket_s *packet);

public: // CBaseClientState overrides:
	void Disconnect(bool bShowMainMenu);
	virtual void ConnectionStart( INetChannel *chan ) OVERRIDE;
	virtual void ConnectionStop() OVERRIDE;
	virtual void FullConnect( const ns_address &adr, int nEncryptionKey ) OVERRIDE;
	bool SetSignonState ( int state, int count, const CNETMsg_SignonState *msg );
	void PacketStart(int incoming_sequence, int outgoing_acknowledged);
	void PacketEnd( void );
	void FileReceived( const char *fileName, unsigned int transferID, bool isReplayDemoFile );
	void FileRequested(const char *fileName, unsigned int transferID, bool isReplayDemoFile );
	void FileDenied(const char *fileName, unsigned int transferID, bool isReplayDemoFile );
	void ConnectionCrashed( const char * reason );
	void ConnectionClosing( const char * reason );
	const char *GetCDKeyHash( void );
	void SetFriendsID( uint friendsID, const char *friendsName );
	void SendClientInfo( void );
	void SendLoadingProgress( int nProgress );
	void SendServerCmdKeyValues( KeyValues *pKeyValues );
	bool SendNetMsg( INetMessage &msg, bool bForceReliable = false, bool bVoice = false );

	void InstallStringTableCallback( char const *tableName );
	bool HookClientStringTable( char const *tableName );
	bool InstallEngineStringTableCallback( char const *tableName );

	void StartUpdatingSteamResources();
	void CheckUpdatingSteamResources();
	void CheckFileCRCsWithServer();
	void FinishSignonState_New();
	void ConsistencyCheck(bool bForce);
	void RunFrame();

	void ReadDeletions( CEntityReadInfo &u );
	void ReadPacketEntities( CEntityReadInfo &u );

	void SendQueuedEntityMessage( int iEntity );
	void RemoveQueuedMessageForEntity( int iEntity );

	// In case the client DLL is using the old interface to set area bits,
	// copy what they've passed to us into the m_chAreaBits array (and 0xFF-out the m_chAreaPortalBits array).
	void UpdateAreaBits_BackwardsCompatible();

	// Used to be pAreaBits.
	unsigned char** GetAreaBits_BackwardCompatibility();

	int GetHltvReplayDelay()const { return m_nHltvReplayDelay; }
	float GetHltvReplayTimeScale()const;
	void StopHltvReplay();

public: // IServerMessageHandlers
	
	virtual bool NETMsg_Tick( const CNETMsg_Tick& msg ) OVERRIDE;
	virtual bool NETMsg_StringCmd( const CNETMsg_StringCmd& msg ) OVERRIDE;

	virtual bool SVCMsg_ServerInfo( const CSVCMsg_ServerInfo& msg ) OVERRIDE;
	virtual bool SVCMsg_ClassInfo( const CSVCMsg_ClassInfo& msg ) OVERRIDE;
	virtual bool SVCMsg_SetPause( const CSVCMsg_SetPause& msg ) OVERRIDE;
	virtual bool SVCMsg_VoiceInit( const CSVCMsg_VoiceInit& msg ) OVERRIDE;
	virtual bool SVCMsg_VoiceData( const CSVCMsg_VoiceData& msg ) OVERRIDE;
	virtual bool SVCMsg_FixAngle( const CSVCMsg_FixAngle& msg ) OVERRIDE;
	virtual bool SVCMsg_Prefetch( const CSVCMsg_Prefetch& msg ) OVERRIDE;
	virtual bool SVCMsg_CrosshairAngle( const CSVCMsg_CrosshairAngle& msg ) OVERRIDE;
	virtual bool SVCMsg_BSPDecal( const CSVCMsg_BSPDecal& msg ) OVERRIDE;
	virtual bool SVCMsg_UserMessage( const CSVCMsg_UserMessage& msg ) OVERRIDE;
	virtual bool SVCMsg_PaintmapData( const CSVCMsg_PaintmapData& msg ) OVERRIDE;
	virtual bool SVCMsg_GameEvent( const CSVCMsg_GameEvent& msg ) OVERRIDE;
	virtual bool SVCMsg_TempEntities( const CSVCMsg_TempEntities& msg ) OVERRIDE;
	virtual bool SVCMsg_PacketEntities( const CSVCMsg_PacketEntities &msg ) OVERRIDE;
	virtual bool SVCMsg_Sounds( const CSVCMsg_Sounds& msg ) OVERRIDE;
	virtual bool SVCMsg_EntityMsg( const CSVCMsg_EntityMsg& msg ) OVERRIDE;
	virtual bool SVCMsg_HltvReplay( const CSVCMsg_HltvReplay &msg );

	CNetMessageBinder m_SVCMsgHltvReplay;

public:

	float		m_flLastServerTickTime;		// the timestamp of last message
	bool		insimulation;

	int			oldtickcount;		// previous tick
	float		m_tickRemainder;	// client copy of tick remainder
	float		m_frameTime;		// dt of the current frame

	int			lastoutgoingcommand;// Sequence number of last outgoing command
	int			chokedcommands;		// number of choked commands
	int			last_command_ack;	// last command sequence number acknowledged by server
	int			last_server_tick;	// same update pattern as last_command_ack, but with server ticks
	int			command_ack;		// current command sequence acknowledged by server
	int			m_nSoundSequence;	// current processed reliable sound sequence number
	int			m_nLastProgressPercent;	// last progress percent sent to server
	
	//
	// information that is static for the entire time connected to a server
	//
	bool		ishltv;			// true if HLTV server/demo
#if defined( REPLAY_ENABLED )
	bool		isreplay;		// true if Replay server/demo
#endif

	CRC32_t		serverCRC;              // To determine if client is playing hacked .map. (entities lump is skipped)
	CRC32_t		serverClientSideDllCRC; // To determine if client is playing on a hacked client dll.
	
	unsigned char	m_chAreaBits[MAX_AREA_STATE_BYTES];
	unsigned char	m_chAreaPortalBits[MAX_AREA_PORTAL_STATE_BYTES];
	bool			m_bAreaBitsValid; // Have the area bits been set for this level yet?
	
// refresh related state
	QAngle		viewangles;
	CUtlVector< AddAngle >	addangle;
	float		addangletotal;
	float		prevaddangletotal;
	int			cdtrack;			// cd audio

	CustomFile_t	m_nCustomFiles[MAX_CUSTOM_FILES]; // own custom files CRCs

	uint		m_nFriendsID;
	char		m_FriendsName[MAX_PLAYER_NAME_LENGTH];


	CUtlFixedLinkedList< CEventInfo > events;	// list of received events

// demo loop control
	int			demonum;		                  // -1 = don't play demos
	char		demos[MAX_DEMOS][MAX_DEMONAME];	  // when not playing

	CUtlVector< CQueuedEntityMessage >	queuedmessage;
public:

	// If 'insimulation', returns the time (in seconds) at the client's current tick.
	// Otherwise, returns the exact client clock.
	float				GetTime() const;
	
	
	bool				IsPaused() const;

	float				GetFrameTime( void ) const;
	void				SetFrameTime( float dt ) { m_frameTime = dt; }

	float				GetClientInterpAmount();		// Formerly cl_interp, now based on cl_interp_ratio and cl_updaterate.
		
	void				Clear( void );

	void				DumpPrecacheStats(  const char * name );

	// Public API to models
	model_t				*GetModel( int index );
	void				SetModel( int tableIndex );
	int					LookupModelIndex( char const *name );

	// Public API to generic
	char const			*GetGeneric( int index );
	void				SetGeneric( int tableIndex );
	int					LookupGenericIndex( char const *name );

	// Public API to sounds
	CSfxTable			*GetSound( int index );
	char const			*GetSoundName( int index );
	void				SetSound( int tableIndex );
	int					LookupSoundIndex( char const *name );
	void				ClearSounds();

	// Public API to decals
	char const			*GetDecalName( int index );
	void				SetDecal( int tableIndex );

	// customization files code
	void				CheckOwnCustomFiles(); // load own custom file
	void				CheckOthersCustomFile(CRC32_t crc); // check if we have to download custom files from server
	void				AddCustomFile( int slot, const char *resourceFile);

public:


	INetworkStringTable *m_pModelPrecacheTable;	
	INetworkStringTable *m_pDynamicModelTable;	
	INetworkStringTable *m_pGenericPrecacheTable;	
	INetworkStringTable *m_pSoundPrecacheTable;
	INetworkStringTable *m_pDecalPrecacheTable;
	INetworkStringTable *m_pInstanceBaselineTable;
	INetworkStringTable *m_pLightStyleTable;
	INetworkStringTable *m_pUserInfoTable;
	INetworkStringTable *m_pServerStartupTable;
	INetworkStringTable *m_pDownloadableFileTable;
	
	CPrecacheItem		model_precache[ MAX_MODELS ];
	CPrecacheItem		generic_precache[ MAX_GENERIC ];
	CPrecacheItem		sound_precache[ MAX_SOUNDS ];
	CPrecacheItem		decal_precache[ MAX_BASE_DECALS ];

	WaitForResourcesHandle_t m_hWaitForResourcesHandle;
	bool m_bUpdateSteamResources;
	bool m_bShownSteamResourceUpdateProgress;
	bool m_bDownloadResources;
	bool m_bDownloadingUGCMap;
	bool m_bCheckCRCsWithServer;
	float m_flLastCRCBatchTime;

	int m_modelIndexLoaded;
	int m_lastModelPercent;

	// This is only kept around to print out the whitelist info if sv_pure is used.	
	CPureServerWhitelist *m_pPureServerWhitelist;

	int m_nHltvReplayDelay;
	int m_nHltvReplayStopAt;
	int m_nHltvReplayStartAt;
	int m_nHltvReplaySlowdownBeginAt;
	int m_nHltvReplaySlowdownEndAt;
	float m_flHltvReplaySlowdownRate;
private:
	
	// Note: This is only here for backwards compatibility. If it is set to something other than NULL,
	// then we'll copy its contents into m_chAreaBits in UpdateAreaBits_BackwardsCompatible.
	byte		*m_pAreaBits;
	
	// Set to false when we first connect to a server and true later on before we
	// respond to a new whitelist.
	bool		m_bMarkedCRCsUnverified;
};  //CClientState

#ifndef DEDICATED
extern	CClientState	&GetLocalClient( int nSlot = -1 );
extern CClientState		&GetBaseLocalClient(); // The player in the first slot (all split users depend on this one)
#endif


#ifndef DEDICATED
extern CGlobalVarsBase g_ClientGlobalVariables;
#endif

extern bool g_bClientGameDLLGreaterThanV13;

#endif // CLIENT_H
