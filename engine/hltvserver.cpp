//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// hltvserver.cpp: implementation of the CHLTVServer class.
//
//////////////////////////////////////////////////////////////////////

#include <server_class.h>
#include <inetmessage.h>
#include <tier0/vprof.h>
#include <keyvalues.h>
#include <edict.h>
#include <eiface.h>
#include <PlayerState.h>
#include <ihltvdirector.h>
#include <time.h>

#include "hltvserver.h"
#include "sv_client.h"
#include "hltvclient.h"
#include "server.h"
#include "sv_main.h"
#include "framesnapshot.h"
#include "networkstringtable.h"
#include "cmodel_engine.h"
#include "dt_recv_eng.h"
#include "cdll_engine_int.h"
#include "GameEventManager.h"
#include "host.h"
#include "dt_common_eng.h"
#include "baseautocompletefilelist.h"
#include "sv_steamauth.h"
#include "tier0/icommandline.h"
#include "mathlib/IceKey.H"
#include "tier1/fmtstr.h"
#include "serializedentity.h"
#include "changeframelist.h"
#include "ihltv.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar tv_snapshotrate, tv_snapshotrate1;

extern CNetworkStringTableContainer *networkStringTableContainerClient;

//we do not want our snapshot frames to be mixed in with the default snapshot frames
static const uint32 knHLTVSnapshotSet = CFrameSnapshotManager::knDefaultSnapshotSet + 1;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CHLTVServer *g_pHltvServer[ HLTV_SERVER_MAX_COUNT ] = { NULL, NULL };
bool IsHltvActive()
{
	CActiveHltvServerIterator hltv;
	if ( hltv )
		return true; // found at least one HLTV active
	else
		return false;
}

static void tv_title_changed_f( IConVar *var, const char *pOldString, float flOldValue )
{
	for ( CActiveHltvServerIterator hltv; hltv; hltv.Next() )
	{
		hltv->BroadcastLocalTitle();	// broadcast hltv_title to the respective clients of this HLTV server
	}
}

static void tv_name_changed_f( IConVar *var, const char *pOldValue, float flOldValue )
{
	Steam3Server().NotifyOfServerNameChange();
}

ConVar tv_maxclients( "tv_maxclients", "128", FCVAR_RELEASE, "Maximum client number on GOTV server.",
							  true, 0, true, 255 );
ConVar tv_maxclients_relayreserved( "tv_maxclients_relayreserved", "0", FCVAR_RELEASE, "Reserves a certain number of GOTV client slots for relays.",
	true, 0, true, 255 );

ConVar tv_autorecord( "tv_autorecord", "0", FCVAR_RELEASE, "Automatically records all games as GOTV demos." );
void OnTvBroadcast( IConVar *var, const char *pOldValue, float flOldValue );
ConVar tv_broadcast( "tv_broadcast", "0", FCVAR_RELEASE, "Automatically broadcasts all games as GOTV demos through Steam.", OnTvBroadcast );
ConVar tv_broadcast1( "tv_broadcast1", "0", FCVAR_RELEASE, "Automatically broadcasts all games as GOTV[1] demos through Steam.", OnTvBroadcast );
ConVar tv_name( "tv_name", "GOTV", FCVAR_RELEASE, "GOTV host name", tv_name_changed_f );
static ConVar tv_password( "tv_password", "", FCVAR_NOTIFY | FCVAR_PROTECTED | FCVAR_DONTRECORD | FCVAR_RELEASE, "GOTV password for all clients" );
static ConVar tv_advertise_watchable( "tv_advertise_watchable", "0", FCVAR_NOTIFY | FCVAR_PROTECTED | FCVAR_DONTRECORD | FCVAR_RELEASE, "GOTV advertises the match as watchable via game UI, clients watching via UI will not need to type password" );

static ConVar tv_overridemaster( "tv_overridemaster", "0", FCVAR_RELEASE, "Overrides the GOTV master root address." );
static ConVar tv_dispatchmode( "tv_dispatchmode", "1", FCVAR_RELEASE, "Dispatch clients to relay proxies: 0=never, 1=if appropriate, 2=always" );
static ConVar tv_dispatchweight( "tv_dispatchweight", "1.25", FCVAR_RELEASE, "Dispatch clients to relay proxies based on load, 1.25 will prefer for every 4 local clients to put 5 clients on every connected relay" );
ConVar tv_transmitall( "tv_transmitall", "1", FCVAR_REPLICATED | FCVAR_RELEASE, "Transmit all entities (not only director view)" );
ConVar tv_debug( "tv_debug", "0", FCVAR_RELEASE, "GOTV debug info." );
ConVar tv_title( "tv_title", "GOTV", FCVAR_RELEASE, "Set title for GOTV spectator UI", tv_title_changed_f );
static ConVar tv_deltacache( "tv_deltacache", "2", FCVAR_RELEASE, "Enable delta entity bit stream cache" );
static ConVar tv_relayvoice( "tv_relayvoice", "1", FCVAR_RELEASE, "Relay voice data: 0=off, 1=on" );
static ConVar tv_encryptdata_key( "tv_encryptdata_key", "", FCVAR_RELEASE, "When set to a valid key communication messages will be encrypted for GOTV" );
static ConVar tv_encryptdata_key_pub( "tv_encryptdata_key_pub", "", FCVAR_RELEASE, "When set to a valid key public communication messages will be encrypted for GOTV" );

static ConVar tv_window_size( "tv_window_size", "16.0", FCVAR_NONE, "Specifies the number of seconds worth of frames that the tv replay system should keep in memory. Increasing this greatly increases the amount of memory consumed by the TV system" );
static ConVar tv_enable_delta_frames( "tv_enable_delta_frames", "1", FCVAR_RELEASE, "Indicates whether or not the tv should use delta frames for storage of intermediate frames. This takes more CPU but significantly less memory." );
extern ConVar spec_replay_enable;
extern ConVar spec_replay_message_time;
ConVar	spec_replay_leadup_time( "spec_replay_leadup_time", "5.3438", FCVAR_RELEASE | FCVAR_REPLICATED, "Replay time in seconds before the highlighted event" );

ConVar tv_broadcast_url( "tv_broadcast_url", "http://localhost:8080", FCVAR_RELEASE, "URL of the broadcast relay" );


CHLTVServer::SHLTVDeltaFrame_t::SHLTVDeltaFrame_t() :
m_pRelativeFrame( NULL ),
	m_pSourceFrame( NULL ),
	m_pClientFrame( NULL ),
	m_nNumValidEntities( 0 ),
	m_nTotalEntities( 0 ),
	m_pEntities( NULL ),
	m_pNewerDeltaFrame( NULL )
{}

CHLTVServer::SHLTVDeltaFrame_t::~SHLTVDeltaFrame_t()
{
	delete [] m_pCopyEntities;
	m_pCopyEntities = NULL;

	//delete the client frame
	delete m_pClientFrame;
	m_pClientFrame = NULL;

	//delete our entity linked list
	while( m_pEntities )
	{
		SHLTVDeltaEntity_t* pDel = m_pEntities;
		m_pEntities = m_pEntities->m_pNext;
		delete pDel;
	}

	if( m_pRelativeFrame )
		m_pRelativeFrame->ReleaseReference();

	if( m_pSourceFrame )
		m_pSourceFrame->ReleaseReference();
}

CHLTVServer::SHLTVDeltaEntity_t::SHLTVDeltaEntity_t() :
	m_SerializedEntity( SERIALIZED_ENTITY_HANDLE_INVALID ),
	m_pServerClass( NULL ),
	m_pNewRecipients( NULL ),
	m_pNext( NULL )
{
}

CHLTVServer::SHLTVDeltaEntity_t::~SHLTVDeltaEntity_t()
{
	//free any recipients if we allocated them
	delete [] m_pNewRecipients;

	if( (m_SerializedEntity != knNoPackedData) && ( m_SerializedEntity != SERIALIZED_ENTITY_HANDLE_INVALID ) )
		delete ( CSerializedEntity* )m_SerializedEntity;
}


CDeltaEntityCache::CDeltaEntityCache()
{
	Q_memset( m_Cache, 0, sizeof(m_Cache) );
	m_nTick = 0;
	m_nMaxEntities = 0;
	m_nCacheSize = 0;
}

CDeltaEntityCache::~CDeltaEntityCache()
{
	Flush();
}

void CDeltaEntityCache::Flush()
{
	if ( m_nMaxEntities != 0 )
	{
		// at least one entity was set
		for ( int i=0; i<m_nMaxEntities; i++ )
		{
			if ( m_Cache[i] != NULL )
			{
				free( m_Cache[i] );
				m_Cache[i] = NULL;
			}
		}

		m_nMaxEntities = 0;
	}

	m_nCacheSize = 0;
}

void CDeltaEntityCache::SetTick( int nTick, int nMaxEntities )
{
	if ( nTick == m_nTick )
		return;

	Flush();

	m_nCacheSize = tv_deltacache.GetInt() * 1024;

	if ( m_nCacheSize <= 0 )
		return;

	m_nMaxEntities = MIN(nMaxEntities,MAX_EDICTS);
	m_nTick = nTick;
}

unsigned char* CDeltaEntityCache::FindDeltaBits( int nEntityIndex, int nDeltaTick, int &nBits )
{
	nBits = -1;
	
	if ( nEntityIndex < 0 || nEntityIndex >= m_nMaxEntities )
		return NULL;

	DeltaEntityEntry_s *pEntry = m_Cache[nEntityIndex];

	while  ( pEntry )
	{
		if ( pEntry->nDeltaTick == nDeltaTick )
		{
			nBits = pEntry->nBits;
			return (unsigned char*)(pEntry) + sizeof(DeltaEntityEntry_s);		
		}
		else
		{
			// keep searching entry list
			pEntry = pEntry->pNext;
		}
	}
	
	return NULL;
}

void CDeltaEntityCache::AddDeltaBits( int nEntityIndex, int nDeltaTick, int nBits, bf_write *pBuffer )
{
	if ( nEntityIndex < 0 || nEntityIndex >= m_nMaxEntities || m_nCacheSize <= 0 )
		return;

	int	nBufferSize = PAD_NUMBER( Bits2Bytes(nBits), 4);

	DeltaEntityEntry_s *pEntry = m_Cache[nEntityIndex];

	if ( pEntry == NULL )
	{
		if ( (int)(nBufferSize+sizeof(DeltaEntityEntry_s)) > m_nCacheSize )
			return;  // way too big, don't even create an entry

		pEntry = m_Cache[nEntityIndex] = (DeltaEntityEntry_s *) malloc( m_nCacheSize );
	}
	else
	{
		char *pEnd = (char*)(pEntry) + m_nCacheSize;	// end marker

		while( pEntry->pNext )
		{
			pEntry = pEntry->pNext;
		}

		int entrySize = sizeof(DeltaEntityEntry_s) + PAD_NUMBER( Bits2Bytes(pEntry->nBits), 4);

		DeltaEntityEntry_s *pNew = (DeltaEntityEntry_s*)((char*)(pEntry) + entrySize);

		if ( ((char*)(pNew) + sizeof(DeltaEntityEntry_s) + nBufferSize) > pEnd )
			return;	// data wouldn't fit into cache anymore, don't add new entries

		pEntry->pNext = pEntry = pNew;
	}

	pEntry->pNext = NULL; // link to next
	pEntry->nDeltaTick = nDeltaTick;
	pEntry->nBits = nBits;
	
	if ( nBits > 0 )
	{
		bf_read  inBuffer; 
		inBuffer.StartReading( pBuffer->GetData(), pBuffer->m_nDataBytes, pBuffer->GetNumBitsWritten() );
		bf_write outBuffer( (char*)(pEntry) + sizeof(DeltaEntityEntry_s), nBufferSize );
		outBuffer.WriteBitsFromBuffer( &inBuffer, nBits );
	}
}

						  
static RecvTable* FindRecvTable( const char *pName, RecvTable **pRecvTables, int nRecvTables )
{
	for ( int i=0; i< nRecvTables; i++ )
	{
		if ( !Q_strcmp( pName, pRecvTables[i]->GetName() ) )
			return pRecvTables[i];
	}

	return NULL;
}

static RecvTable* AddRecvTableR( SendTable *sendt, RecvTable **pRecvTables, int &nRecvTables )
{
	RecvTable *recvt = FindRecvTable( sendt->m_pNetTableName, pRecvTables, nRecvTables );

	if ( recvt )
		return recvt;	// already in list

	if ( sendt->m_nProps > 0 )
	{
		RecvProp *receiveProps = new RecvProp[sendt->m_nProps];

		for ( int i=0; i < sendt->m_nProps; i++ )
		{
			// copy property data

			SendProp * sp = sendt->GetProp( i );
			RecvProp * rp = &receiveProps[i];

			rp->m_pVarName	= sp->m_pVarName;
			rp->m_RecvType	= sp->m_Type;
			
			if ( sp->IsExcludeProp() )
			{
				// if prop is excluded, give different name
				rp->m_pVarName = "IsExcludedProp";
			}

			if ( sp->IsInsideArray() )
			{
				rp->SetInsideArray();
				rp->m_pVarName = "InsideArrayProp"; // give different name
			}
			
			if ( sp->GetType() == DPT_Array )
			{
				Assert ( sp->GetArrayProp() == sendt->GetProp( i-1 ) );
				Assert( receiveProps[i-1].IsInsideArray() );
				
				rp->SetArrayProp( &receiveProps[i-1] );
				rp->InitArray( sp->m_nElements, sp->m_ElementStride );
			}

			if ( sp->GetType() == DPT_DataTable )
			{
				// recursive create
				Assert ( sp->GetDataTable() );
				RecvTable *subTable = AddRecvTableR( sp->GetDataTable(), pRecvTables, nRecvTables );
				rp->SetDataTable( subTable );
			}
		}

		recvt = new RecvTable( receiveProps, sendt->m_nProps, sendt->m_pNetTableName );
	}
	else
	{
		// table with no properties
		recvt = new RecvTable( NULL, 0, sendt->m_pNetTableName );
	}

	pRecvTables[nRecvTables] = recvt;
	nRecvTables++;

	return recvt;
}

void CHLTVServer::FreeClientRecvTables()
{
	for ( int i=0; i< m_nRecvTables; i++ )
	{
		RecvTable *rt = m_pRecvTables[i];

		// delete recv table props
		if ( rt->m_pProps )
		{
			Assert( rt->m_nProps > 0 );
			delete [] rt->m_pProps;
		}

		// delete the table itself
		delete rt;
		
	}

	Q_memset( m_pRecvTables, 0, sizeof( m_pRecvTables ) );
	m_nRecvTables = 0;
}

// creates client receive tables from server send tables
void CHLTVServer::InitClientRecvTables()
{
	ServerClass* pCur = NULL;
	
	if ( ClientDLL_GetAllClasses() != NULL )
		return; //already initialized

	// first create all SendTables
	for ( pCur = serverGameDLL->GetAllServerClasses(); pCur; pCur=pCur->m_pNext )
	{
		// create receive table from send table.
		AddRecvTableR( pCur->m_pTable, m_pRecvTables, m_nRecvTables );

		ErrorIfNot( 
			m_nRecvTables < ARRAYSIZE( m_pRecvTables ), 
			("AddRecvTableR: overflowed MAX_DATATABLES")
			);
	}

	// now register client classes 
	for ( pCur = serverGameDLL->GetAllServerClasses(); pCur; pCur=pCur->m_pNext )
	{
		ErrorIfNot( 
			m_nRecvTables < ARRAYSIZE( m_pRecvTables ), 
			("ClientDLL_InitRecvTableMgr: overflowed MAX_DATATABLES")
			);

		// find top receive table for class
		RecvTable * recvt = FindRecvTable( pCur->m_pTable->GetName(), m_pRecvTables, m_nRecvTables );

		Assert ( recvt );
		
		// register class, constructor addes clientClass to g_pClientClassHead list
		ClientClass * clientclass = new ClientClass( pCur->m_pNetworkName, NULL, NULL, recvt );

		if ( !clientclass	)
		{
			Msg("HLTV_InitRecvTableMgr: failed to allocate client class %s.\n", pCur->m_pNetworkName );
			return;
		}
	}

	RecvTable_Init( m_pRecvTables, m_nRecvTables );
}



CHLTVFrame::CHLTVFrame()
{
	
}

CHLTVFrame::~CHLTVFrame()
{
	FreeBuffers();
}

void CHLTVFrame::Reset( void )
{
	for ( int i=0; i<HLTV_BUFFER_MAX; i++ )
	{
		m_Messages[i].Reset();
	}
}

bool CHLTVFrame::HasData( void )
{
	for ( int i=0; i<HLTV_BUFFER_MAX; i++ )
	{
		if ( m_Messages[i].GetNumBitsWritten() > 0 )
			return true;
	}

	return false;
}

void CHLTVFrame::CopyHLTVData( const CHLTVFrame &frame )
{
	// copy reliable messages
	int bits = frame.m_Messages[HLTV_BUFFER_RELIABLE].GetNumBitsWritten();

	if ( bits > 0 )
	{
		int bytes = PAD_NUMBER( Bits2Bytes(bits), 4 );
		m_Messages[HLTV_BUFFER_RELIABLE].StartWriting( new char[ bytes ], bytes, bits );
		Q_memcpy( m_Messages[HLTV_BUFFER_RELIABLE].GetData(), frame.m_Messages[HLTV_BUFFER_RELIABLE].GetData(), bytes );
	}

	// copy unreliable messages
	bits = frame.m_Messages[HLTV_BUFFER_UNRELIABLE].GetNumBitsWritten();
	bits += frame.m_Messages[HLTV_BUFFER_TEMPENTS].GetNumBitsWritten();
	bits += frame.m_Messages[HLTV_BUFFER_SOUNDS].GetNumBitsWritten();

	if ( bits > 0 )
	{
		// collapse all unreliable buffers in one
		int bytes = PAD_NUMBER( Bits2Bytes(bits), 4 );
		m_Messages[HLTV_BUFFER_UNRELIABLE].StartWriting( new char[ bytes ], bytes );
		m_Messages[HLTV_BUFFER_UNRELIABLE].WriteBits( frame.m_Messages[HLTV_BUFFER_UNRELIABLE].GetData(), frame.m_Messages[HLTV_BUFFER_UNRELIABLE].GetNumBitsWritten() ); 
		m_Messages[HLTV_BUFFER_UNRELIABLE].WriteBits( frame.m_Messages[HLTV_BUFFER_TEMPENTS].GetData(), frame.m_Messages[HLTV_BUFFER_TEMPENTS].GetNumBitsWritten() ); 
		m_Messages[HLTV_BUFFER_UNRELIABLE].WriteBits( frame.m_Messages[HLTV_BUFFER_SOUNDS].GetData(), frame.m_Messages[HLTV_BUFFER_SOUNDS].GetNumBitsWritten() ); 
	}

	if ( tv_relayvoice.GetBool() )
	{
		int nVoiceBits = frame.m_Messages[ HLTV_BUFFER_VOICE ].GetNumBitsWritten();
		if ( nVoiceBits > 0 )
		{
			int nVoiceBytes = PAD_NUMBER( Bits2Bytes( nVoiceBits ), 4 );
			m_Messages[ HLTV_BUFFER_VOICE ].StartWriting( new char[ nVoiceBytes ], nVoiceBytes );
			m_Messages[ HLTV_BUFFER_VOICE ].WriteBits( frame.m_Messages[ HLTV_BUFFER_VOICE ].GetData(), frame.m_Messages[ HLTV_BUFFER_VOICE ].GetNumBitsWritten() );
		}
	}
}


uint CHLTVFrame::GetMemSize()const
{
	uint nSize = sizeof( *this );
	for ( int i = 0; i < ARRAYSIZE( m_Messages ); ++i )
		nSize += m_Messages[ i ].GetMaxNumBits() / 8;
	return nSize;
}


void CHLTVFrame::AllocBuffers( void )
{
	// allocate buffers for input frame
	for ( int i=0; i < HLTV_BUFFER_MAX; i++ )
	{
		Assert( m_Messages[i].GetBasePointer() == NULL );
		m_Messages[i].StartWriting( new char[NET_MAX_PAYLOAD], NET_MAX_PAYLOAD);
	}
}

void CHLTVFrame::FreeBuffers( void )
{
	for ( int i=0; i<HLTV_BUFFER_MAX; i++ )
	{
		bf_write &msg = m_Messages[i];

		if ( msg.GetBasePointer() )
		{
			delete[] msg.GetBasePointer();
			msg.StartWriting( NULL, 0 );
		}
	}
}

CHLTVServer::CHLTVServer( uint nInstanceIndex, float flSnapshotRate )
	: m_DemoRecorder( this )
	, m_Broadcast( this )
	, m_ClientState( this )
	, m_nInstanceIndex( nInstanceIndex )
	, m_flSnapshotRate( flSnapshotRate )
{
	m_flTickInterval = 0.03;
	m_MasterClient = NULL;
	m_Server = NULL;
	m_Director = NULL;
	m_nFirstTick = -1;
	m_nLastTick = 0;
	m_CurrentFrame = NULL;
	m_nViewEntity = 0;
	m_nPlayerSlot = 0;
	m_bSignonState = false;
	m_flStartTime = 0;
	m_flFPS = 0;
	m_nGameServerMaxClients = 0;
	m_fNextSendUpdateTime = 0;
	Q_memset( m_pRecvTables, 0, sizeof( m_pRecvTables ) );
	m_nRecvTables = 0;
	m_vPVSOrigin.Init();
	m_nStartTick = 0;
	m_bPlayingBack = false;
	m_bPlaybackPaused = false;
	m_flPlaybackRateModifier = 0;
	m_nSkipToTick = 0;
	m_bMasterOnlyMode = false;
	Assert( m_ClientState.m_pHLTV == this ); // constructor of ClientState should've set thie HLTV. 
	m_nGlobalSlots = 0;
	m_nGlobalClients = 0;
	m_nGlobalProxies = 0;
	m_nExternalTotalViewers = 0;
	m_nExternalLinkedViewers = 0;

	m_nDebugID = EVENT_DEBUG_ID_INIT;

	m_pOldestDeltaFrame = NULL;
	m_pNewestDeltaFrame = NULL;

	m_pLastSourceSnapshot = NULL;
	m_pLastTargetSnapshot = NULL;
}

CHLTVServer::~CHLTVServer()
{
	m_nDebugID = EVENT_DEBUG_ID_SHUTDOWN;

	if ( m_nRecvTables > 0 )
	{
		RecvTable_Term();
		FreeClientRecvTables();
	}

	// make sure everything was destroyed
	Assert( m_CurrentFrame == NULL );
	Assert( CountClientFrames() == 0 );
}

void CHLTVServer::SetMaxClients( int number )
{
	// allow max clients 0 in HLTV
	m_nMaxclients = clamp( number, 0, ABSOLUTE_PLAYER_LIMIT );
}

void CHLTVServer::StartMaster(CGameClient *client)
{
	m_nExternalTotalViewers = 0;
	m_nExternalLinkedViewers = 0;
	Clear();  // clear old settings & buffers

	if ( !client )
	{
		ConMsg("GOTV client not found.\n");
		return;
	}

	m_Director = serverGameDirector;	

	if ( !m_Director )
	{
		ConMsg("Mod doesn't support GOTV. No director module found.\n");
		return;
	}

	m_MasterClient = client;
	m_MasterClient->m_bIsHLTV = true;
	m_MasterClient->m_pHltvSlaveServer = this;  // Master client needs to know which server (with which tickrate) it's sending packets
#if defined( REPLAY_ENABLED )
	m_MasterClient->m_bIsReplay = false;
#endif
	// let game.dll know that we are the HLTV client
	Assert( serverGameClients );

	CPlayerState *player = serverGameClients->GetPlayerState( m_MasterClient->edict );
	player->hltv = true;

	m_Server = (CGameServer*)m_MasterClient->GetServer();

	// set default user settings
	m_MasterClient->m_ConVars->SetString( "name", tv_name.GetString() );
	m_MasterClient->m_ConVars->SetString( "cl_team", "1" );
	m_MasterClient->m_ConVars->SetString( "rate", va( "%d", DEFAULT_RATE ) );
	m_MasterClient->m_ConVars->SetString( "cl_updaterate", va( "%f", GetSnapshotRate() ) ); // this may not be necessary...
	m_MasterClient->m_ConVars->SetString( "cl_cmdrate", "64" );
	m_MasterClient->m_ConVars->SetString( "cl_interp_ratio", "1.0" );
	m_MasterClient->m_ConVars->SetString( "cl_predict", "0" );

	m_nViewEntity = m_MasterClient->GetPlayerSlot() + 1;
	m_nPlayerSlot = m_MasterClient->GetPlayerSlot();

	// copy server settings from m_Server

	m_nGameServerMaxClients = m_Server->GetMaxClients(); // maxclients is different on proxy (128)
	serverclasses	= m_Server->serverclasses;
	serverclassbits	= m_Server->serverclassbits;
	worldmapCRC		= m_Server->worldmapCRC;
	clientDllCRC	= m_Server->clientDllCRC;
	m_flTickInterval= m_Server->GetTickInterval();

	// allocate buffers for input frame
	m_HLTVFrame.AllocBuffers();
			
	InstallStringTables();

	// activate director in game.dll
	m_Director->AddHLTVServer( this ); // we RemoveHLTVServer later instead of setting it to NULL now

	// register as listener for mod specific events
	const char **modevents = m_Director->GetModEvents();

	int j = 0;
	while ( modevents[j] != NULL )
	{
		const char *eventname = modevents[j];

		CGameEventDescriptor *descriptor = g_GameEventManager.GetEventDescriptor( eventname );

		if ( descriptor )
		{
			g_GameEventManager.AddListener( this, descriptor, CGameEventManager::CLIENTSTUB );
		}
		else
		{
			DevMsg("CHLTVServer::StartMaster: game event %s not found.\n", eventname );
		}

		j++;
	}
	
	// copy signon buffers
	m_Signon.StartWriting( m_Server->m_Signon.GetBasePointer(), m_Server->m_Signon.m_nDataBytes, 
		m_Server->m_Signon.GetNumBitsWritten() );

	Q_strncpy( m_szMapname, m_Server->m_szMapname, sizeof(m_szMapname) );
	Q_strncpy( m_szSkyname, m_Server->m_szSkyname, sizeof(m_szSkyname) );

	NET_ListenSocket( m_Socket, true );	// activated HLTV TCP socket

	m_MasterClient->ExecuteStringCommand( "spectate" ); // become a spectator

	m_MasterClient->UpdateUserSettings(); // make sure UserInfo is correct

	// hack reduce signontick by one to catch changes made in the current tick
	m_MasterClient->m_nSignonTick--;	

	if ( m_bMasterOnlyMode )
	{
		// we allow only one client in master only mode
		tv_maxclients.SetValue( MIN(1,tv_maxclients.GetInt()) );
	}

	SetMaxClients( tv_maxclients.GetInt() );

	m_bSignonState = false; //master proxy is instantly connected

	m_nSpawnCount++;

	m_flStartTime = net_time;

	m_State = ss_active;

	// stop any previous recordings
	StopRecording();

	// start new recording if autorecord is enabled
	if ( tv_autorecord.GetBool() )
	{
		m_DemoRecorder.StartAutoRecording();
	}
	m_Broadcast.OnMasterStarted();
	if ( GetIndexedConVar( tv_broadcast, GetInstanceIndex() ).GetBool() )
	{
		StartBroadcast();
	}

	m_DemoEventWriteBuffer.StartWriting( m_DemoEventsBuffer, ARRAYSIZE( m_DemoEventsBuffer) );

	ReconnectClients();
}


void CHLTVServer::StartBroadcast()
{
	m_Broadcast.StartRecording( tv_broadcast_url.GetString() );
}

void CHLTVServer::StartDemo( const char *filename )
{

}

bool CHLTVServer::DispatchToRelay( CHLTVClient *pClient )
{
	if ( tv_dispatchmode.GetInt() <= DISPATCH_MODE_OFF )
		return false; // don't redirect
	
	CBaseClient	*pBestProxy = NULL;
	float fBestRatio = 1.0f;

	// find best relay proxy
	for ( int i = 0; i < GetClientCount(); i++ )
	{
		CBaseClient *pProxy = m_Clients[ i ];

		// check all known proxies
		if ( !pProxy->IsConnected() || !pProxy->IsHLTV() || (pClient == pProxy) )
			continue;

		int slots = Q_atoi( pProxy->GetUserSetting( "hltv_slots" ) );
		int clients = Q_atoi( pProxy->GetUserSetting( "hltv_clients" ) );

		// skip overloaded proxies or proxies with no slots at all
		if ( (clients > slots) || slots <= 0 )
			continue;

		// calc clients/slots ratio for this proxy
		float ratio = ((float)(clients))/((float)slots);

		if ( ratio < fBestRatio )
		{
			fBestRatio = ratio;
			pBestProxy = pProxy;
		}
	}

	if ( pBestProxy == NULL )
	{
		if ( tv_dispatchmode.GetInt() == DISPATCH_MODE_ALWAYS )
		{
			// we are in always forward mode, drop client if we can't forward it
			pClient->Disconnect("No GOTV relay available");
			return true;
		}
		else
		{
			// just let client connect to this proxy
			return false;
		}
	}

	// check if client should stay on this relay server unless we are the master,
	// masters always prefer to send clients to relays
	if ( (tv_dispatchmode.GetInt() == DISPATCH_MODE_AUTO) && (GetMaxClients() > 0) )
	{
		// ratio = clients/slots. give relay proxies 25% bonus
		int numSlots = GetMaxClients();
		if ( tv_maxclients_relayreserved.GetInt() > 0 )
			numSlots -= tv_maxclients_relayreserved.GetInt();
		numSlots = MAX( 0, numSlots );

		int numClients = GetNumClients();
		if ( numClients > numSlots )
			numSlots = numClients;

		float flDispatchWeight = tv_dispatchweight.GetFloat();
		if ( flDispatchWeight <= 1.01 )
			flDispatchWeight = 1.01;
		float myRatio = ((float)numClients/(float)numSlots) * flDispatchWeight;

		myRatio = MIN( myRatio, 1.0f ); // clamp to 1

		// if we have a better local ratio then other proxies, keep this client here
		if ( myRatio < fBestRatio )
			return false;	// don't redirect
	}
	
	const char *pszRelayAddr = pBestProxy->GetUserSetting("hltv_addr");

	if ( !pszRelayAddr )
		return false;
	

	ConMsg( "Redirecting spectator %s to GOTV relay %s\n", 
		pClient->GetNetChannel()->GetAddress(), 
		pszRelayAddr );

	// first tell that client that we are a SourceTV server,
	// otherwise it's might ignore the "connect" command
	CSVCMsg_ServerInfo_t serverInfo;
	FillServerInfo( serverInfo ); 
	serverInfo.set_is_redirecting_to_proxy_relay( true );
	pClient->SendNetMsg( serverInfo, true );
	
	// tell the client to connect to this new address
	CNETMsg_StringCmd_t cmdMsg( va("connect %s\n", pszRelayAddr ) ) ;
	pClient->SendNetMsg( cmdMsg, true );
    		
 	// increase this proxies client number in advance so this proxy isn't used again next time
	int clients = Q_atoi( pBestProxy->GetUserSetting( "hltv_clients" ) );
	pBestProxy->SetUserCVar( "hltv_clients", va("%d", clients+1 ) );
	
	return true;
}

void CHLTVServer::ConnectRelay(const char *address)
{
	m_nExternalTotalViewers = 0;
	m_nExternalLinkedViewers = 0;

	if ( m_ClientState.IsConnected() )
	{
		// do not try to reconnect to old connection
		m_ClientState.m_Remote.RemoveAll();

		// disconnect first
		m_ClientState.Disconnect();

		Changelevel( true ); // inactivate clients
	}

	// connect to new server
	m_ClientState.Connect( address, address, "HLTVConnectRelay" );
}

void CHLTVServer::StartRelay()
{
	if ( !m_ClientState.IsConnected() && !IsPlayingBack() )
	{
		DevMsg("StartRelay: not connected.\n");
		Shutdown();
		return;
	}

	Clear();  // clear old settings & buffers

	if ( m_nRecvTables == 0 ) 
	{
		// must be done only once since Mod never changes
		InitClientRecvTables();
	}

	m_HLTVFrame.AllocBuffers();

	m_StringTables = &m_NetworkStringTables;
	
	SetMaxClients( tv_maxclients.GetInt() );

	m_bSignonState = true;

	m_flStartTime = net_time;

	m_State = ss_loading;

	m_nSpawnCount++;
}

int	CHLTVServer::GetHLTVSlot( void )
{
	return m_nPlayerSlot;
}

float CHLTVServer::GetOnlineTime( void )
{
	return MAX(0, net_time - m_flStartTime);
}

void CHLTVServer::GetLocalStats( int &proxies, int &slots, int &clients )
{
	int numSlots = GetMaxClients();
	if ( tv_maxclients_relayreserved.GetInt() > 0 )
		numSlots -= tv_maxclients_relayreserved.GetInt();
	numSlots = MAX( 0, numSlots );

	int numClients = GetNumClients();
	if ( numClients > numSlots )
		numSlots = numClients;

	proxies = GetNumProxies();
	clients = numClients;
	slots = numSlots;
}

void CHLTVServer::GetRelayStats( int &proxies, int &slots, int &clients )
{
	proxies = slots = clients = 0;

	for (int i=0 ; i < GetClientCount() ; i++ )
	{
		CBaseClient *client = m_Clients[ i ];

		if ( !client->IsConnected() || !client->IsHLTV() )
			continue;

		proxies += Q_atoi( client->GetUserSetting( "hltv_proxies" ) );
		slots += Q_atoi( client->GetUserSetting( "hltv_slots" ) );
		clients += Q_atoi( client->GetUserSetting( "hltv_clients" ) );
	}
}

void CHLTVServer::GetGlobalStats( int &proxies, int &slots, int &clients )
{
	// the master proxy is the only one that really has all data to generate
	// global stats
	if ( IsMasterProxy() )
	{
		GetRelayStats( m_nGlobalProxies, m_nGlobalSlots, m_nGlobalClients );

		int numSlots = GetMaxClients();
		if ( tv_maxclients_relayreserved.GetInt() > 0 )
			numSlots -= tv_maxclients_relayreserved.GetInt();
		numSlots = MAX( 0, numSlots );
		
		int numClients = GetNumClients();
		if ( !numClients && m_Broadcast.IsRecording() )
		{	// Always consider broadcast stream as a non-zero amount of clients
			numClients = 1;
		}

		if ( numClients > numSlots )
			numSlots = numClients;

		m_nGlobalSlots += numSlots;
		m_nGlobalClients += numClients;
	}

	// if this is a relay proxies, global data comes via the 
	// wire from the master proxy
	proxies = m_nGlobalProxies;
	slots = m_nGlobalSlots;
	clients = m_nGlobalClients;
}

void CHLTVServer::GetExternalStats( int &numExternalTotalViewers, int &numExternalLinkedViewers )
{
	numExternalTotalViewers = m_nExternalTotalViewers;
	numExternalLinkedViewers = m_nExternalLinkedViewers;
}

void CHLTVServer::UpdateHltvExternalViewers( uint32 numTotalViewers, uint32 numLinkedViewers )
{
	if ( !IsMasterProxy() )
		return;
	if ( !IsActive() )
		return;

	m_nExternalTotalViewers = numTotalViewers;
	m_nExternalLinkedViewers = numLinkedViewers;
}

const netadr_t *CHLTVServer::GetRelayAddress( void )
{
	if ( IsMasterProxy() )
	{
		return &net_local_adr; // TODO wrong port
	}
	else if ( m_ClientState.m_NetChannel )
	{
		const ns_address &adr = m_ClientState.m_NetChannel->GetRemoteAddress();
		if ( adr.IsType<netadr_t>() )
			return &adr.AsType<netadr_t>();
	}

	return NULL;
}

bool CHLTVServer::IsMasterProxy( void )
{
	return ( m_MasterClient != NULL );
}

bool CHLTVServer::IsTVRelay()
{
	return !IsMasterProxy();
}

bool CHLTVServer::IsDemoPlayback( void )
{
	return false;
}

void CHLTVServer::BroadcastLocalTitle( CHLTVClient *client ) 
{
	IGameEvent *event = g_GameEventManager.CreateEvent( "hltv_title", true );

	if ( !event )
		return;

	event->SetString( "text", tv_title.GetString() );

	CSVCMsg_GameEvent_t eventMsg;

	eventMsg.SetReliable( true );

	// create bit stream from KeyValues
	if ( !g_GameEventManager.SerializeEvent( event, &eventMsg ) )
	{
		DevMsg("CHLTVServer: failed to serialize title '%s'.\n", event->GetName() );
		g_GameEventManager.FreeEvent( event );
		return;
	}

	if ( client )
	{
		client->SendNetMsg( eventMsg );
	}
	else
	{
		for ( int i = 0; i < m_Clients.Count(); i++ )
		{
			client = Client(i);

			if ( !client->IsActive() || client->IsHLTV() )
				continue;

			client->SendNetMsg( eventMsg );
		}
	}

	g_GameEventManager.FreeEvent( event );
}

void CHLTVServer::BroadcastLocalChat( const char *pszChat, const char *pszGroup )
{
	IGameEvent *event = g_GameEventManager.CreateEvent( "hltv_chat", true );

	if ( !event )
		return;
	
	event->SetString( "text", pszChat );

	CSVCMsg_GameEvent_t eventMsg;

	eventMsg.SetReliable( false );

	// create bit stream from KeyValues
	if ( !g_GameEventManager.SerializeEvent( event, &eventMsg ) )
	{
		DevMsg("CHLTVServer: failed to serialize chat '%s'.\n", event->GetName() );
		g_GameEventManager.FreeEvent( event );
		return;
	}

	for ( int i = 0; i < m_Clients.Count(); i++ )
	{
		CHLTVClient *cl = Client(i);

		if ( !cl->IsActive() || !cl->IsSpawned() || cl->IsHLTV() )
			continue;

		// if this is a spectator chat message and client disabled it, don't show it
		if ( Q_strcmp( cl->m_szChatGroup, pszGroup) || cl->m_bNoChat )
			continue;

		cl->SendNetMsg( eventMsg );
	}

	g_GameEventManager.FreeEvent( event );
}

void CHLTVServer::BroadcastEventLocal( IGameEvent *event, bool bReliable )
{
	CSVCMsg_GameEvent_t eventMsg;

	eventMsg.SetReliable( bReliable );

	// create bit stream from KeyValues
	if ( !g_GameEventManager.SerializeEvent( event, &eventMsg ) )
	{
		DevMsg("CHLTVServer: failed to serialize local event '%s'.\n", event->GetName() );
		return;
	}

	for ( int i = 0; i < m_Clients.Count(); i++ )
	{
		CHLTVClient *cl = Client(i);

		if ( !cl->IsActive() || !cl->IsSpawned() || cl->IsHLTV() )
			continue;

		if ( !cl->SendNetMsg( eventMsg ) )
		{
			if ( eventMsg.IsReliable() )
			{
				DevMsg( "BroadcastMessage: Reliable broadcast message overflow for client %s", cl->GetClientName() );
			}
		}
	}

	if ( tv_debug.GetBool() )
		Msg("SourceTV broadcast local event: %s\n", event->GetName() );
}

void CHLTVServer::BroadcastEvent(IGameEvent *event)
{
	CSVCMsg_GameEvent_t eventMsg;

	// create bit stream from KeyValues
	if ( !g_GameEventManager.SerializeEvent( event, &eventMsg ) )
	{
		DevMsg("CHLTVServer: failed to serialize event '%s'.\n", event->GetName() );
		return;
	}

	BroadcastMessage( eventMsg, true, true );

	if ( m_DemoRecorder.IsRecording() || m_Broadcast.IsRecording() )
	{
		eventMsg.WriteToBuffer( m_DemoEventWriteBuffer );
	}

	if ( tv_debug.GetBool() )
		Msg("SourceTV broadcast event: %s\n", event->GetName() );
}
 
bool CHLTVServer::IsRecording()
{
	return m_DemoRecorder.IsRecording();
}

const char* CHLTVServer::GetRecordingDemoFilename()
{
	return m_DemoRecorder.GetDemoFilename();
}

void CHLTVServer::StartAutoRecording()
{
	return m_DemoRecorder.StartAutoRecording();
}

void CHLTVServer::FireGameEvent(IGameEvent *event)
{
	if ( !IsActive() )
		return;

	CSVCMsg_GameEvent_t eventMsg;

	// create bit stream from KeyValues
	if ( g_GameEventManager.SerializeEvent( event, &eventMsg ) )
	{
		SendNetMsg( eventMsg );
	}
	else
	{
		DevMsg("CHLTVServer::FireGameEvent: failed to serialize event '%s'.\n", event->GetName() );
	}
}

int CHLTVServer::GetEventDebugID( void )
{
	return m_nDebugID;
}

//Whenever we're done recording a demo, go through all the remaining frames and write them too.
//This is done because now the demo recorded is recuring the CURRENT frame instead of the broadcast frame
void CHLTVServer::StopRecordingAndFreeFrames( bool bFreeFrames, const CGameInfo *pGameInfo)
{
	// We are only stopping demo recorder. Broadcast does not stop on changelevel, for example. If broadcast is activated, it stays activated until stopped.
	if( !m_CurrentFrame || !m_DemoRecorder.IsRecording() )
	{
		//we aren't recording, just clean up if requested
		if( bFreeFrames )
		{
			DeleteClientFrames( -1 );
			FreeAllDeltaFrames();
			m_CurrentFrame = NULL;
		}

		return;
	}

	int nStartingTick = m_CurrentFrame->tick_count;
	CHLTVFrame *pFrame = static_cast< CHLTVFrame *> (m_CurrentFrame->m_pNext );

	//clear our current frame since we are flushing all of our frames
	if( bFreeFrames )
		m_CurrentFrame = NULL;

	//flush any remaining current frames that we have
	while ( pFrame )
	{
		// restore string tables for this time
		int nFrameTick = pFrame->tick_count;
		RestoreTick( pFrame->tick_count );
		m_DemoRecorder.WriteFrame( pFrame );
		pFrame = static_cast< CHLTVFrame *> (pFrame->m_pNext );

		//and clean up the memory for this frame if we need to
		if( bFreeFrames )
			DeleteClientFrames( nFrameTick );
	}

	//now we need to handle the delta frames, freeing as we go to avoid a memory explosion
	while( m_pOldestDeltaFrame )
	{
		CHLTVFrame* pExpandedFrame = m_pOldestDeltaFrame->m_pClientFrame;
		int nExpandFrameTick = pExpandedFrame->tick_count;

		ExpandDeltaFramesToTick( nExpandFrameTick );
		RestoreTick( nExpandFrameTick );
		m_DemoRecorder.WriteFrame( pExpandedFrame );

		//and clean up the memory for this frame if we need to
		if( bFreeFrames )
			DeleteClientFrames( nExpandFrameTick );
	}

	// restore our string tables to what they were to begin with
	RestoreTick( nStartingTick );

	return m_DemoRecorder.StopRecording( pGameInfo );
}

bool CHLTVServer::ShouldUpdateMasterServer()
{
	// If the main game server is active, then we let it update Steam with the server info.
	return !sv.IsActive();
}

CBaseClient *CHLTVServer::CreateNewClient(int slot )
{
	return new CHLTVClient( slot, this );
}

void CHLTVServer::InstallStringTables( void )
{
#ifndef SHARED_NET_STRING_TABLES

	int numTables = m_Server->m_StringTables->GetNumTables();

	m_StringTables = &m_NetworkStringTables;

	Assert( m_StringTables->GetNumTables() == 0); // must be empty

	m_StringTables->AllowCreation( true );
	
	// master hltv needs to keep a list of changes for all table items
	m_StringTables->EnableRollback( true );

	for ( int i =0; i<numTables; i++)
	{
		// iterate through server tables
		CNetworkStringTable *serverTable = 
			(CNetworkStringTable*)m_Server->m_StringTables->GetTable( i );

		if ( !serverTable )
			continue;

		// get matching client table
		CNetworkStringTable *hltvTable = 
			(CNetworkStringTable*)m_StringTables->CreateStringTable(
				serverTable->GetTableName(),
				serverTable->GetMaxStrings(),
				serverTable->GetUserDataSize(),
				serverTable->GetUserDataSizeBits(),
				serverTable->IsUsingDictionary() ? NSF_DICTIONARY_ENABLED : NSF_NONE
				);

		if ( !hltvTable )
		{
			DevMsg("SV_InstallHLTVStringTableMirrors! Missing client table \"%s\".\n ", serverTable->GetTableName() );
			continue;
		}

		// make hltv table an exact copy of server table
		hltvTable->CopyStringTable( serverTable ); 

		// link hltv table to server table
		serverTable->SetMirrorTable( m_nInstanceIndex, hltvTable ); // there may be multiple mirror tables (up to the number of HLTV servers), it's easier to manage than removing the mirror tables from the correct places at the correct times
	}

	m_StringTables->AllowCreation( false );

#endif
}

void CHLTVServer::UninstallStringTables( void )
{
	if ( !m_Server )
		return;
	int numTables = m_Server->m_StringTables->GetNumTables();

	for ( int i = 0; i < numTables; i++ )
	{
		// iterate through server tables
		CNetworkStringTable *serverTable =
			( CNetworkStringTable* )m_Server->m_StringTables->GetTable( i );

		if ( !serverTable )
			continue;

		serverTable->SetMirrorTable( m_nInstanceIndex, NULL ); // alternatively, we could find serverTable again and remove mirror table by pointer. It would be a bit more fragile and slower.
	}
}

void CHLTVServer::RestoreTick( int tick )
{
#ifndef SHARED_NET_STRING_TABLES

	// only master proxy delays time
	if ( !IsMasterProxy() )
		return;

	int numTables = m_StringTables->GetNumTables();

	for ( int i =0; i<numTables; i++)
	{
			// iterate through server tables
		CNetworkStringTable *pTable = (CNetworkStringTable*) m_StringTables->GetTable( i );
		pTable->RestoreTick( tick );
	}

#endif
}

void CHLTVServer::UserInfoChanged( int nClientIndex )
{
	// don't change UserInfo table, it keeps the infos of the original players
}

bool CHLTVServer::GetClassBaseline( ServerClass *pClass, SerializedEntityHandle_t *pHandle)
{
	// if we are the master proxy the game server has our baseline data
	if ( m_Server )
	{
		return m_Server->GetClassBaseline( pClass, pHandle );
	}

	// otherwise we have it from the hltvclientstate
	return m_ClientState.GetClassBaseline( pClass->m_ClassID, pHandle );
}

void CHLTVServer::LinkInstanceBaselines( void )
{	
	// Forces to update m_pInstanceBaselineTable.
	AUTO_LOCK_FM( g_svInstanceBaselineMutex );
	GetInstanceBaselineTable(); 

	Assert( m_pInstanceBaselineTable );
		
	// update all found server classes 
	for ( ServerClass *pClass = serverGameDLL->GetAllServerClasses(); pClass; pClass=pClass->m_pNext )
	{
		char idString[32];
		Q_snprintf( idString, sizeof( idString ), "%d", pClass->m_ClassID );

		// Ok, make a new instance baseline so they can reference it.
		int index  = m_pInstanceBaselineTable->FindStringIndex( idString );
			
		if ( index != -1 )
		{
			pClass->m_InstanceBaselineIndex = index;
		}
		else
		{
			pClass->m_InstanceBaselineIndex = INVALID_STRING_INDEX;
		}
	}
}

/* CHLTVServer::GetOriginFromPackedEntity is such a bad, bad hack.

extern float DecodeFloat(SendProp const *pProp, bf_read *pIn);

Vector CHLTVServer::GetOriginFromPackedEntity(PackedEntity* pe)
{
	Vector origin; origin.Init();

	SendTable *pSendTable = pe->m_pSendTable;

	// recursively go down until BaseEntity sendtable
	while ( Q_strcmp( pSendTable->GetName(), "DT_BaseEntity") )
	{
		SendProp *pProp = pSendTable->GetProp( 0 ); // 0 = baseclass
		pSendTable = pProp->GetDataTable();
	}

	for ( int i=0; i < pSendTable->GetNumProps(); i++ )
	{
		SendProp *pProp = pSendTable->GetProp( i );

		if ( Q_strcmp( pProp->GetName(), "m_vecOrigin" ) == 0 )
		{
			Assert( pProp->GetType() == DPT_Vector );
		
			bf_read buf( pe->LockData(), Bits2Bytes(pe->GetNumBits()), pProp->GetOffset() );

			origin[0] = DecodeFloat(pProp, &buf);
			origin[1] = DecodeFloat(pProp, &buf);
			origin[2] = DecodeFloat(pProp, &buf);

			break;
		}
	}

	return origin;
} */

CHLTVEntityData *FindHLTVDataInSnapshot( CFrameSnapshot * pSnapshot, int iEntIndex )
{
	int a = 0;
	int z = pSnapshot->m_nValidEntities-1;

	if ( iEntIndex < pSnapshot->m_pValidEntities[a] ||
		 iEntIndex > pSnapshot->m_pValidEntities[z] )
		 return NULL;
	
	while ( a < z )
	{
		int m = (a+z)/2;

		int index = pSnapshot->m_pValidEntities[m];

		if ( index == iEntIndex )
			return &pSnapshot->m_pHLTVEntityData[m];
		
		if ( iEntIndex > index )
		{
			if ( pSnapshot->m_pValidEntities[z] == iEntIndex )
				return &pSnapshot->m_pHLTVEntityData[z];

			if ( a == m )
				return NULL;

			a = m;
		}
		else
		{
			if ( pSnapshot->m_pValidEntities[a] == iEntIndex )
				return &pSnapshot->m_pHLTVEntityData[a];

			if ( z == m )
				return NULL;

			z = m;
		}
	}

	return NULL;
}

void CHLTVServer::EntityPVSCheck( CClientFrame *pFrame )
{
	byte PVS[PAD_NUMBER( MAX_MAP_CLUSTERS,8 ) / 8];
	int nPVSSize = (GetCollisionBSPData()->numclusters + 7) / 8;

	// setup engine PVS
	SV_ResetPVS( PVS, nPVSSize );

	CFrameSnapshot * pSnapshot = pFrame->GetSnapshot();	

	Assert ( pSnapshot->m_pHLTVEntityData != NULL );

	int nDirectorEntity = m_Director->GetPVSEntity();
    	
	if ( pSnapshot && nDirectorEntity > 0 )
	{
		CHLTVEntityData *pHLTVData = FindHLTVDataInSnapshot( pSnapshot, nDirectorEntity );

		if ( pHLTVData )
		{
			m_vPVSOrigin.x = pHLTVData->origin[0];
			m_vPVSOrigin.y = pHLTVData->origin[1];
			m_vPVSOrigin.z = pHLTVData->origin[2];
		}
	}
	else
	{
		m_vPVSOrigin = m_Director->GetPVSOrigin();
	}


	SV_AddOriginToPVS( m_vPVSOrigin );

	// know remove all entities that aren't in PVS
	int entindex = -1;

	while ( true )
	{
		entindex = pFrame->transmit_entity.FindNextSetBit( entindex+1 );

		if ( entindex < 0 )
			break;
			
		// is transmit_always is set ->  no PVS check
		if ( pFrame->transmit_always->Get(entindex) )
		{
			pFrame->last_entity = entindex;
			continue;
		}

		CHLTVEntityData *pHLTVData = FindHLTVDataInSnapshot( pSnapshot, entindex );

		if ( !pHLTVData )
			continue;

		unsigned int nNodeCluster = pHLTVData->m_nNodeCluster;

		// check if node or cluster is in PVS

		if ( nNodeCluster & (1<<31) )
		{
			// it's a node SLOW
			nNodeCluster &= ~(1<<31);
			if ( CM_HeadnodeVisible( nNodeCluster, PVS, nPVSSize ) )
			{
				pFrame->last_entity = entindex;
				continue;
			}
		}
		else
		{
			// it's a cluster QUICK
			if ( PVS[nNodeCluster >> 3] & (1 << (nNodeCluster & 7)) )
			{
				pFrame->last_entity = entindex;
				continue;
			}
		}

 		// entity is not in PVS, remove from transmit_entity list
		pFrame->transmit_entity.Clear( entindex );
	}
}

void CHLTVServer::SignonComplete()
{
	Assert ( !IsMasterProxy() );
	
	m_bSignonState = false;

	LinkInstanceBaselines();

	if ( tv_debug.GetBool() )
		Msg("GOTV signon complete.\n" );
}

CFrameSnapshot* CHLTVServer::CloneDeltaSnapshot( const CFrameSnapshot *pCopySnapshot )
{
	//and create a new snapshot for this to reference, of which will not have entity data
	CFrameSnapshot* pNewSnapshot = framesnapshotmanager->CreateEmptySnapshot(
#ifdef DEBUG_SNAPSHOT_REFERENCES
		"CHLTVServer::CloneDeltaSnapshot",
#endif
		pCopySnapshot->m_nTickCount, 0, knHLTVSnapshotSet );

	//copy over snapshot data...
	pNewSnapshot->m_iExplicitDeleteSlots = pCopySnapshot->m_iExplicitDeleteSlots;
	pNewSnapshot->m_nTickCount = pCopySnapshot->m_nTickCount;

	//note that we do not copy over the valid entities list. We only copy over info for valid entities, so it is already properly collapsed, and we can save memory by ignoring this
	pNewSnapshot->m_pValidEntities = NULL;
	pNewSnapshot->m_nValidEntities = 0;

	//we don't need to copy over the events. The HLTV frame will actually copy over all the bits from the frame for the temporary entities, so we do not need to serialize this
	pNewSnapshot->m_nTempEntities = 0;
	pNewSnapshot->m_pTempEntities = NULL;

	return pNewSnapshot;
}

//the bit buffer assumes that all buffers it reads/writes are 4 byte aligned, which the serialized entity enforces, so this function given a number of bits, will ensure that the proper size
//in bytes for the buffer is met
static int GetBitBufBytes( int nNumBits )
{
	return ( ( nNumBits + 31 ) / 32) * 4;
}

//given a tick that the time changed on, and the current value along with the previous properties, this will determine which properties
//have changed, and create a serialized entity handle that contains just the delta properties
static SerializedEntityHandle_t CreateDeltaProperties( int nTick, const PackedEntity* pCurrPacked, const PackedEntity *pDeltaBase )
{
	//the actual max properties is HUGE, and we only need deltas, so to avoid blowing up the stack, we use a smaller buffer size
	static const int knMaxChangedProps = 4 * 1024;
	uint16 nChangedPropIndices[ knMaxChangedProps ];

	//get our source serialized entity data
	const CSerializedEntity* pCurrProps = ( const CSerializedEntity* )pCurrPacked->GetPackedData();

	//if we have no delta base to encode from, we just want the entire structure
	if( !pDeltaBase )
	{
		CSerializedEntity *pNewSerialized = new CSerializedEntity( );
		pNewSerialized->Copy( *pCurrProps );
		return ( SerializedEntityHandle_t )pNewSerialized;
	}

	//we have a delta, so lets build only the properties that vary based upon the tick time
	const int nNumProps = pCurrProps->GetFieldCount();

	//first pass, determine all the memory that we'll need for everything
	const CChangeFrameList *pChangeList = pCurrPacked->GetChangeFrameList();

	int nNumChangedProps = 0;
	int nChangedPropDataBits = 0;

	for( int nCurrProp = 0; nCurrProp < nNumProps; ++nCurrProp )
	{
		CFieldPath PropIndex = pCurrProps->GetFieldPath( nCurrProp );

		if( pChangeList->DidPropChangeAfterTick( nTick, PropIndex ) )
		{
			//this property changed, so add it into our list
			AssertMsg( nNumChangedProps < knMaxChangedProps, "Error: Overflow in modified properties for delta compression. This limit should be increased" );
			nChangedPropIndices[ nNumChangedProps ] = nCurrProp;
			nNumChangedProps++;
			nChangedPropDataBits += pCurrProps->GetFieldDataSizeInBits( nCurrProp );
		}
	}

	//if we have no changed properties, excellent, we can just bail and not have to spend memory
	if( nNumChangedProps == 0 )
		return SERIALIZED_ENTITY_HANDLE_INVALID;

	//we have changed props, so go ahead and allocate a buffer to hold the memory that we are going to setup
	CSerializedEntity *pNewSerialized = new CSerializedEntity( );
	pNewSerialized->SetupPackMemory( nNumChangedProps, nChangedPropDataBits );

	//setup the readers and writers
	bf_read SrcPropData;
	pCurrProps->StartReading( SrcPropData );
	bf_write OutPropData;
	pNewSerialized->StartWriting( OutPropData );

	//and now run through and copy over the data that we need
	for( int nOutputProp = 0; nOutputProp < nNumChangedProps; ++nOutputProp )
	{
		int nDataOffset, nNextOffset;
		CFieldPath PropIndex;
		pCurrProps->GetField( nChangedPropIndices[ nOutputProp ], PropIndex, &nDataOffset, &nNextOffset );

		//record this information in our buffer
		pNewSerialized->SetFieldPath( nOutputProp, PropIndex );
		pNewSerialized->SetFieldDataBitOffset( nOutputProp, OutPropData.GetNumBitsWritten() );

		//copy over the contents that we care about (which are bit aligned)
		SrcPropData.Seek( nDataOffset );
		OutPropData.WriteBitsFromBuffer( &SrcPropData, nNextOffset - nDataOffset );
	}

	//make sure our size calculations line up
	Assert( ( uint32 )OutPropData.GetNumBitsWritten() == pNewSerialized->GetFieldDataBitCount() );

	//we have our final property set now
	return ( SerializedEntityHandle_t )pNewSerialized;
}

//given a current and previous packed data, this will determine if the entity data changed, and if so, will setup the new recipient list with the appropraite information
static CSendProxyRecipients* CopyRecipientList( const PackedEntity* pCurrFramePacked, const PackedEntity* pPrevFramePacked )
{
	if( ( pPrevFramePacked != NULL ) && pCurrFramePacked->CompareRecipients( CUtlMemory< CSendProxyRecipients >( pPrevFramePacked->GetRecipients(), pPrevFramePacked->GetNumRecipients() ) ) )
	{
		//they match, so we don't need a delta buffer
		return NULL;
	}

	//if we got down here there is a mismatch, so just copy the new list
	const int nCurrRecipients = pCurrFramePacked->GetNumRecipients();
	CSendProxyRecipients* pCopyDest = new CSendProxyRecipients [ nCurrRecipients ];
	const CSendProxyRecipients* pCopySrc = pCurrFramePacked->GetRecipients();

	for( int nCopyRecipient = 0; nCopyRecipient < nCurrRecipients; ++nCopyRecipient )
		pCopyDest[ nCopyRecipient ] = pCopySrc[ nCopyRecipient ];

	return pCopyDest;
}

void CHLTVServer::CreateDeltaFrameEntities( SHLTVDeltaFrame_t *pOutputEntities, const CFrameSnapshot *pCurrFrame, const CFrameSnapshot *pPrevFrame )
{
	//determine how many ints we need to store our bit list
	const uint32 nNumCopyInts = ( pCurrFrame->m_nNumEntities + 31 ) / 32;

	//allocate memory for us to actually store the objects
	pOutputEntities->m_nTotalEntities		= pCurrFrame->m_nNumEntities;
	pOutputEntities->m_nNumValidEntities	= pCurrFrame->m_nValidEntities;
	pOutputEntities->m_pEntities			= NULL;
	pOutputEntities->m_pCopyEntities		= new uint32 [ nNumCopyInts ];
	memset( pOutputEntities->m_pCopyEntities, 0, sizeof(uint32) * nNumCopyInts );

	//the index into our current previous frame so that we can find old versions of the object to delta against
	const uint16 *pPrevEntityIndex		= ( pPrevFrame ) ? pPrevFrame->m_pValidEntities : NULL;
	const uint16 *pPrevEndEntityIndex	= ( pPrevFrame ) ? pPrevEntityIndex + pPrevFrame->m_nValidEntities : NULL;

	//the tick that we want to compare changes against
	int nChangeTick = ( pPrevFrame ) ? pPrevFrame->m_nTickCount : -1;

	//the current tail of our linked list
	SHLTVDeltaEntity_t** pListTail = &pOutputEntities->m_pEntities;

	//don't iterate through the entities, but instead iterate through our valid entity list, which indexes into our entities so we can
	//skip invalid entities
	for( int nValidEntity = 0; nValidEntity < pCurrFrame->m_nValidEntities; ++nValidEntity )
	{
		const int nEntityIndex					= pCurrFrame->m_pValidEntities[ nValidEntity ];
		const CFrameSnapshotEntry *pSrcEntity	= pCurrFrame->m_pEntities + nEntityIndex;

		//see if we can find a previous entity in order to diff ourself against
		bool bReuseOriginal = false;
		const PackedEntity* pPrevFramePacked = NULL;
		for( ; pPrevEntityIndex != pPrevEndEntityIndex; ++pPrevEntityIndex )
		{
			//see if our entity is higher than our current one, if so, we need to stop searching and let our current frame list catch up
			if( *pPrevEntityIndex > nEntityIndex )
				break;

			//if the slot matches, then we may have a match
			if( *pPrevEntityIndex == nEntityIndex )
			{
				const CFrameSnapshotEntry *pPrevEntity = &pPrevFrame->m_pEntities[ *pPrevEntityIndex ];

				//we found a match. See if it is actually the same object
				if( pPrevEntity->m_nSerialNumber == pSrcEntity->m_nSerialNumber )
				{
					//same object, so we can reuse the data, but first see if it is identical
					if( pPrevEntity->m_pPackedData == pSrcEntity->m_pPackedData )
					{
						//it is identical, we can just reuse it
						bReuseOriginal = true;
					}
					else
					{
						//different, so cache the data
						pPrevFramePacked = framesnapshotmanager->GetPackedEntity( *( const_cast< CFrameSnapshot* >(pPrevFrame) ), nEntityIndex );
					}
				}

				break;
			}
		}

		//first off, see if this is an entity that hasn't changed at all
		if( bReuseOriginal )
		{
			//this is the same entity, so just copy it forward
			pOutputEntities->m_pCopyEntities[ nEntityIndex / 32 ] |= ( 1 << ( nEntityIndex % 32 ) );			
			//skip the normal handling
			continue;
		}

		//allocate our new entity and add it to our list
		SHLTVDeltaEntity_t *pOutEntity			= new SHLTVDeltaEntity_t;
		*pListTail = pOutEntity;
		pListTail = &pOutEntity->m_pNext;

		//copy over all the state we can from this entity
		pOutEntity->m_nSerialNumber = pSrcEntity->m_nSerialNumber;
		pOutEntity->m_pServerClass	= pSrcEntity->m_pClass;
		pOutEntity->m_nSourceIndex	= nEntityIndex;

		//handle the situation where there is no packed data (occurs occasionally)
		if( pSrcEntity->m_pPackedData == INVALID_PACKED_ENTITY_HANDLE )
		{
			pOutEntity->m_SerializedEntity = SHLTVDeltaEntity_t::knNoPackedData;
		}
		else
		{
			//along with the data from the packed portion of the entity
			const PackedEntity *pSrcPacked = framesnapshotmanager->GetPackedEntity( *( const_cast< CFrameSnapshot* >(pCurrFrame) ), nEntityIndex );

			//this is a new packed entity that we'll need to construct on the other side
			pOutEntity->m_nSnapshotCreationTick		= pSrcPacked->GetSnapshotCreationTick( );

			//build up the property list for this based upon how we want to encode it (absolute, relative, etc)
			pOutEntity->m_SerializedEntity = CreateDeltaProperties( nChangeTick, pSrcPacked, pPrevFramePacked );

			//update our recipient list
			pOutEntity->m_nNumRecipients = ( uint16 )pSrcPacked->GetNumRecipients();
			pOutEntity->m_pNewRecipients = CopyRecipientList( pSrcPacked, pPrevFramePacked );
		}
	}
}

void CHLTVServer::AddNewDeltaFrame( CClientFrame *pClientFrame )
{
	//see if this support is disabled
	if( !tv_enable_delta_frames.GetBool() 
#if HLTV_REPLAY_ENABLED
		|| spec_replay_enable.GetBool() // <sergiy> delta frames with small delay make no sense because we'll effectively only compress a second or two ( the min delay )of the packets. Enabling replay means the first 20 seconds of frames must be decompressed. Maybe we'll implement compression between 20 and 100 seconds for competitive sometimes, but for now it seems ok to leave frames uncompressed if we want replay
#endif
		)
	{
		//we aren't running delta frames, so make sure we decompress any we might currently have, and fall back to normal
		ExpandDeltaFramesToTick( -1 );
		AddNewFrame( pClientFrame );
		return;
	}

	//track the performance of this frame
	VPROF_BUDGET( "CHLTVServer::AddNewDeltaFrame", "HLTV" );
	Assert( pClientFrame->tick_count > m_nLastTick );

	//if we don't have any frames encoded, we will be doing an absolute encode, so we can just store the frame direct (saves a lot of time and allocations)
	if( m_pLastSourceSnapshot == NULL )
	{
		Assert( ( m_pLastTargetSnapshot == NULL ) && ( m_nFirstTick < 0 ) );

		//add the frame. This way it will be ready immediately, and we don't have to duplicate all of the setup work
		AddNewFrame( pClientFrame );

		//use this frame as our encoding relative frame, and our delta that we will build on later as well
		m_pLastSourceSnapshot = pClientFrame->GetSnapshot();
		m_pLastSourceSnapshot->AddReference();
		m_pLastTargetSnapshot = pClientFrame->GetSnapshot();
		m_pLastTargetSnapshot->AddReference();

		return;
	}

	//update internal state based upon the incoming frames (this should match AddNewFrame)
	m_nLastTick = pClientFrame->tick_count;
	m_HLTVFrame.SetSnapshot( pClientFrame->GetSnapshot() );
	m_HLTVFrame.tick_count = pClientFrame->tick_count;
	m_HLTVFrame.last_entity = pClientFrame->last_entity;
	m_HLTVFrame.transmit_entity = pClientFrame->transmit_entity;

	//first off allocate our holding frame and client information
	SHLTVDeltaFrame_t *pNewDeltaFrame = new SHLTVDeltaFrame_t;
	pNewDeltaFrame->m_pNewerDeltaFrame = NULL;
	pNewDeltaFrame->m_pRelativeFrame = NULL;

	//Uncomment these lines if you want to enable validation support
	{
		//pNewDeltaFrame->m_pSourceFrame = pClientFrame->GetSnapshot();
		//pNewDeltaFrame->m_pSourceFrame->AddReference();
	}

	//if we have a previously encoded frame, we want to use that as our relative baseline
	if( m_pLastTargetSnapshot )
	{
		pNewDeltaFrame->m_pRelativeFrame = m_pLastTargetSnapshot;
		pNewDeltaFrame->m_pRelativeFrame->AddReference();
	}

	//create our new frame and copy all the data over
	pNewDeltaFrame->m_pClientFrame = new CHLTVFrame( );
	pNewDeltaFrame->m_pClientFrame->CopyFrame( *pClientFrame );
	pNewDeltaFrame->m_pClientFrame->CopyHLTVData( m_HLTVFrame );

	//create a copy of the snapshot that we can work with
	CFrameSnapshot *pNewSnapshot = CloneDeltaSnapshot( pClientFrame->GetSnapshot() );

	//now we need to create our delta encoded objects
	CreateDeltaFrameEntities( pNewDeltaFrame, pClientFrame->GetSnapshot(), m_pLastSourceSnapshot );

	//transfer ownership of this snapshot over to the client frame (meaning we don't need our reference)
	pNewDeltaFrame->m_pClientFrame->SetSnapshot(pNewSnapshot);
	pNewSnapshot->ReleaseReference();

	//link ourself into the list and make sure the newest link matches
	if( m_pNewestDeltaFrame )
		m_pNewestDeltaFrame->m_pNewerDeltaFrame = pNewDeltaFrame;
	m_pNewestDeltaFrame = pNewDeltaFrame;
	//and if our list was empty, our new frame is now also the oldest
	if( !m_pOldestDeltaFrame )
		m_pOldestDeltaFrame = pNewDeltaFrame;

	// reset HLTV frame for recording next messages etc.
	m_HLTVFrame.Reset();
	m_HLTVFrame.SetSnapshot( NULL );

	//update our references to point to our latest snapshots for subsequent encodes/decodes
	if( m_pLastSourceSnapshot )
		m_pLastSourceSnapshot->ReleaseReference();
	if( m_pLastTargetSnapshot )
		m_pLastTargetSnapshot->ReleaseReference();

	m_pLastSourceSnapshot = pClientFrame->GetSnapshot();
	m_pLastSourceSnapshot->AddReference();
	m_pLastTargetSnapshot = pNewSnapshot;
	m_pLastTargetSnapshot->AddReference();
}

//given a baseline property list and a delta from that, this will expand the delta serialized entity to contain a full property set
static void BuildMergedPropertySet( CSerializedEntity* pDelta, const CSerializedEntity* pBase, CChangeFrameList* pChangeTimes, int nChangeTick )
{
	int nDataBlockSizeBits = pBase->GetFieldDataBitCount();
	const int nNumFields = pBase->GetFieldCount();

	//see if we can take an optimized path where we can memcpy over our baseline and just overlay
	bool bCanMemCpySrc = true;

	{
		int nOverrideIndex = 0;
		CFieldPath NextOverridePath = pDelta->GetFieldPath( 0 );

		for( int nCurrField = 0; nCurrField < nNumFields; ++nCurrField )
		{
			//see if this is overridden
			CFieldPath FieldPath= pBase->GetFieldPath( nCurrField );
			if( FieldPath == NextOverridePath )
			{
				//we have a property change that we are applying, so update the change tick time
				pChangeTimes->SetChangeTick( FieldPath, nChangeTick );

				//use our modified size, minus our base size to determine the change in size from this override
				int nBaseSize = pBase->GetFieldDataSizeInBits( nCurrField );
				int nModifiedSize = pDelta->GetFieldDataSizeInBits( nOverrideIndex );

				//see if this has changed the layout to the point where we have to re lay out the base fields
				if( nModifiedSize != nBaseSize )
				{
					bCanMemCpySrc = false;
					nDataBlockSizeBits += ( nModifiedSize - nBaseSize );
				}

				//and advance to our next override
				nOverrideIndex++;
				if( nOverrideIndex < pDelta->GetFieldCount( ) )
					NextOverridePath = pDelta->GetFieldPath( nOverrideIndex );
			}		
		}
	}

	//create a stack entity to take the new merged results
	CSerializedEntity Merged;
	Merged.SetupPackMemory( nNumFields, nDataBlockSizeBits );

	if(bCanMemCpySrc)
	{
		memcpy( Merged.GetFieldData(), pBase->GetFieldData(), Bits2Bytes( pBase->GetFieldDataBitCount() ) );
	}

	//now merge
	{
		//setup the readers and writers
		bf_read BaseData, ModifiedData;
		pBase->StartReading( BaseData);
		pDelta->StartReading( ModifiedData );
		bf_write OutPropData;
		Merged.StartWriting( OutPropData );

		int nOverrideIndex = 0;
		CFieldPath NextOverridePath = pDelta->GetFieldPath( 0 );

		for( int nCurrField = 0; nCurrField < nNumFields; ++nCurrField )
		{
			//see if this is overridden
			CFieldPath FieldPath = pBase->GetFieldPath( nCurrField );

			//update our storage info
			Merged.SetFieldPath( nCurrField, FieldPath );
			Merged.SetFieldDataBitOffset( nCurrField, OutPropData.GetNumBitsWritten() );

			if( FieldPath == NextOverridePath )
			{				
				int nModifiedSize = pDelta->GetFieldDataSizeInBits( nOverrideIndex );
				OutPropData.WriteBitsFromBuffer( &ModifiedData, nModifiedSize );	

				//and advance to our next override
				nOverrideIndex++;
				if( nOverrideIndex < pDelta->GetFieldCount( ) )
					NextOverridePath = pDelta->GetFieldPath( nOverrideIndex );
			}	
			else
			{
				//use the baseline value
				int nDataStart	= pBase->GetFieldDataBitOffset( nCurrField );
				int nDataEnd	= pBase->GetFieldDataBitEndOffset( nCurrField );

				if( !bCanMemCpySrc )
				{
					BaseData.Seek( nDataStart );
					OutPropData.WriteBitsFromBuffer( &BaseData, nDataEnd - nDataStart );
				}
				else
				{
					OutPropData.SeekToBit( nDataEnd );
				}
			}			
		}
	}

	//now we can discard our results of the delta, and just use that object with the contents of our merged
	Merged.Swap( *pDelta );
}

static void CompareArray( int nOldElements, const void* pOldMem, int nNewElements, const void* pNewMem, int nElementSize )
{
	//to avoid warnings when asserts aren't disabled
#ifdef DBGFLAG_ASSERT

	Assert( nOldElements == nNewElements );

	int nNumBytes = nOldElements * nElementSize;
	const uint8* pByteOld		= ( const uint8* )pOldMem;
	const uint8* pByteNew		= ( const uint8* )pNewMem;

	//per byte check so it is more apparent where things went wrong
	for( int nCurrByte = 0; nCurrByte < nNumBytes; ++nCurrByte )
	{
		Assert( pByteOld[ nCurrByte ] == pByteNew[ nCurrByte ] );
	}

#endif
}

//development utility to take two snapshots and compare them for equivalence. This is used to test whether or not the new snapshot matches the original one that was the basis for compression
static void CompareSnapshot( const CFrameSnapshot* pOldSnapshot, const CFrameSnapshot* pNewSnapshot, int nChangePropTick )
{
	//to avoid warnings when asserts aren't disabled
#ifdef DBGFLAG_ASSERT

	//first off compare the valid entity list
	CompareArray( pOldSnapshot->m_nValidEntities, pOldSnapshot->m_pValidEntities, pNewSnapshot->m_nValidEntities, pNewSnapshot->m_pValidEntities, sizeof( uint16 ) );
	CompareArray( pOldSnapshot->m_iExplicitDeleteSlots.Count(), pOldSnapshot->m_iExplicitDeleteSlots.Base(), pNewSnapshot->m_iExplicitDeleteSlots.Count(), pNewSnapshot->m_iExplicitDeleteSlots.Base(), sizeof( int ) );

	//now each element
	for( int nCurrEntity = 0; nCurrEntity < pOldSnapshot->m_nValidEntities; ++nCurrEntity )
	{
		int nEntityIndex = pOldSnapshot->m_pValidEntities[ nCurrEntity ];

		//get info about each entity and compare
		const CFrameSnapshotEntry* pOldEntity = &pOldSnapshot->m_pEntities[ nEntityIndex ];
		const CFrameSnapshotEntry* pNewEntity = &pNewSnapshot->m_pEntities[ nEntityIndex ];

		Assert( pOldEntity->m_nSerialNumber == pNewEntity->m_nSerialNumber );
		Assert( pOldEntity->m_pClass == pNewEntity->m_pClass );
		Assert( ( pOldEntity->m_pPackedData != INVALID_PACKED_ENTITY_HANDLE ) == ( pNewEntity->m_pPackedData != INVALID_PACKED_ENTITY_HANDLE ) );

		//now check the packed data if applicable
		if( pOldEntity->m_pPackedData )
		{
			const PackedEntity* pOldPacked = framesnapshotmanager->GetPackedEntity( *( const_cast< CFrameSnapshot* >(pOldSnapshot) ), nEntityIndex );
			const PackedEntity* pNewPacked = framesnapshotmanager->GetPackedEntity( *( const_cast< CFrameSnapshot* >(pNewSnapshot) ), nEntityIndex );

			Assert( pOldPacked->m_nEntityIndex == pNewPacked->m_nEntityIndex );
			Assert( pOldPacked->GetSnapshotCreationTick() == pNewPacked->GetSnapshotCreationTick() );
			Assert( pOldPacked->ShouldCheckCreationTick() == pNewPacked->ShouldCheckCreationTick() );
			Assert( pOldPacked->m_pServerClass == pNewPacked->m_pServerClass );
			Assert( pOldPacked->m_pClientClass == pNewPacked->m_pClientClass );

			//compare the recipients
			CompareArray( pOldPacked->GetNumRecipients(), pOldPacked->GetRecipients(), pNewPacked->GetNumRecipients(), pNewPacked->GetRecipients(), sizeof( CSendProxyRecipients ) );

			//compare the change list
			{
				int nNumProps = pOldPacked->GetChangeFrameList()->GetNumProps();
				for( int nCurrProp = 0; nCurrProp < nNumProps; ++nCurrProp )
				{
					//see if this is a property that should have changed this frame (we have to do this since we lose some precision on the change flags during reconstruction)
					if( pOldPacked->GetChangeFrameList()->GetPropTick( nCurrProp ) >= nChangePropTick )
					{
						Assert( pNewPacked->GetChangeFrameList()->GetPropTick( nCurrProp ) >= nChangePropTick );
					}
				}
			}

			//now compare properties
			Assert( ( pOldPacked->GetPackedData() == SERIALIZED_ENTITY_HANDLE_INVALID ) == ( pNewPacked->GetPackedData() == SERIALIZED_ENTITY_HANDLE_INVALID ) );
			if( pOldPacked->GetPackedData() != SERIALIZED_ENTITY_HANDLE_INVALID )
			{
				const CSerializedEntity* pOldProps = ( const CSerializedEntity* )pOldPacked->GetPackedData();
				const CSerializedEntity* pNewProps = ( const CSerializedEntity* )pNewPacked->GetPackedData();

				CompareArray( pOldProps->GetFieldCount(), pOldProps->GetFieldPaths(), pNewProps->GetFieldCount(), pNewProps->GetFieldPaths(), sizeof( short ) );
				CompareArray( pOldProps->GetFieldCount(), pOldProps->GetFieldDataBitOffsets(), pNewProps->GetFieldCount(), pNewProps->GetFieldDataBitOffsets(), sizeof( int ) );
				CompareArray( pOldProps->GetFieldDataBitCount() / 8, pOldProps->GetFieldData(), pNewProps->GetFieldDataBitCount() / 8, pNewProps->GetFieldData(), sizeof( uint8 ) );
			}
		}
	}
#endif
}


size_t CHLTVServer::SHLTVDeltaFrame_t::GetMemSize()const
{
	uint nSize = sizeof( *this );
	if ( CFrameSnapshot *pSnapshot = m_pClientFrame->GetSnapshot() )
	{
		nSize += pSnapshot->GetMemSize();
	}
	return nSize;
}

size_t CFrameSnapshot::GetMemSize()const
{
	size_t nSize = sizeof( *this );
	nSize += m_nNumEntities * sizeof( CFrameSnapshotEntry ) + m_nValidEntities * sizeof( *m_pValidEntities );
	return nSize;
}

void CHLTVServer::ExpandDeltaFrameToFullFrame( SHLTVDeltaFrame_t *pDeltaFrame )
{
	//track the performance of this frame
	VPROF_BUDGET( "CHLTVServer::ExpandDeltaFrameToFullFrame", "HLTV" );

	CFrameSnapshot *pSnapshot = pDeltaFrame->m_pClientFrame->GetSnapshot();

	//we need to construct our full entity list in our snapshot
	pSnapshot->m_pEntities		= new CFrameSnapshotEntry[ pDeltaFrame->m_nTotalEntities ];
	pSnapshot->m_nNumEntities	= pDeltaFrame->m_nTotalEntities;

	//and create our valid entity list, which is just a one to one index map
	pSnapshot->m_pValidEntities = new uint16 [ pDeltaFrame->m_nNumValidEntities ];
	pSnapshot->m_nValidEntities = pDeltaFrame->m_nNumValidEntities;

	//the index into our current previous frame so that we can find old versions of the object to delta against
	const CFrameSnapshot* pPrevFrame	= pDeltaFrame->m_pRelativeFrame;
	const uint32 nPrevFrameEntities		= ( pPrevFrame ) ? pPrevFrame->m_nNumEntities : 0;

	//the time tick that we should use for all of our change flags. Since the HLTV server can skip frames, be conservative in our change
	//tick settings and use the frame after our last frame if we have one (effectively collapsing change times backwards), and if not, just our current tick count
	const int nChangePropTick = ( pPrevFrame ) ? pPrevFrame->m_nTickCount + 1 : pSnapshot->m_nTickCount;

	//the bit list that indicates which entities to just copy forward
	const uint32* RESTRICT pCopyEntities		= pDeltaFrame->m_pCopyEntities;
	const uint32 nTotalEntities					= pDeltaFrame->m_nTotalEntities;

	SHLTVDeltaEntity_t *pCurrDeltaEntity	= pDeltaFrame->m_pEntities;
	uint16* RESTRICT pCurrOutValidEntity	= pSnapshot->m_pValidEntities;

	//run through each of our valid entities
	for( uint32 nCurrEntity = 0; nCurrEntity < nTotalEntities; ++nCurrEntity )
	{
		CFrameSnapshotEntry *pCurrEntity	= pSnapshot->m_pEntities + nCurrEntity;

		//see if we just need to copy this forward
		if( pCopyEntities[ nCurrEntity / 32 ] & ( 1 << ( nCurrEntity % 32 ) ) )
		{
			const CFrameSnapshotEntry *pPrevEntity = &pPrevFrame->m_pEntities[ nCurrEntity ];

			//all we need to do is copy the previous to the current
			pCurrEntity->m_nSerialNumber	= pPrevEntity->m_nSerialNumber;
			pCurrEntity->m_pClass			= pPrevEntity->m_pClass;
			pCurrEntity->m_pPackedData		= pPrevEntity->m_pPackedData;

			//and make sure to reference our packed data so it won't go away on us
			if( pCurrEntity->m_pPackedData != INVALID_PACKED_ENTITY_HANDLE )
			{
				PackedEntity* pPacked = framesnapshotmanager->GetPackedEntity( pCurrEntity->m_pPackedData );
				pPacked->m_ReferenceCount++;
			}

			//and add this entity to the valid entity list
			*pCurrOutValidEntity = ( uint16 )nCurrEntity;
			pCurrOutValidEntity++;			
		}
		//otherwise, see if it is our delta entity that we encoded
		else if( pCurrDeltaEntity && ( nCurrEntity == pCurrDeltaEntity->m_nSourceIndex ) )
		{
			//we have matched this delta entity, so move onto the next
			SHLTVDeltaEntity_t* pDeltaEntity = pCurrDeltaEntity;
			pCurrDeltaEntity = pCurrDeltaEntity->m_pNext;

			//setup our valid list to index into this slot
			*pCurrOutValidEntity = ( uint16 )nCurrEntity;
			pCurrOutValidEntity++;

			//copy over the raw data
			pCurrEntity->m_nSerialNumber	= pDeltaEntity->m_nSerialNumber;
			pCurrEntity->m_pClass			= pDeltaEntity->m_pServerClass;
			pCurrEntity->m_pPackedData		= INVALID_PACKED_ENTITY_HANDLE;

			//see if there is no packed data associated with this class
			if( pDeltaEntity->m_SerializedEntity == SHLTVDeltaEntity_t::knNoPackedData )
				continue;

			//see if we can find a previous entity in order to diff ourself against
			PackedEntityHandle_t PrevPackedHandle = INVALID_PACKED_ENTITY_HANDLE;
			PackedEntity *pPrevPacked = NULL;

			if( nCurrEntity < nPrevFrameEntities )
			{
				const CFrameSnapshotEntry *pPrevEntity = &pPrevFrame->m_pEntities[ nCurrEntity ];
				if( ( pPrevEntity->m_nSerialNumber == pDeltaEntity->m_nSerialNumber ) &&  ( pPrevEntity->m_pClass == pDeltaEntity->m_pServerClass ) )
				{
					PrevPackedHandle	= pPrevEntity->m_pPackedData;
					if( PrevPackedHandle != INVALID_PACKED_ENTITY_HANDLE )
						pPrevPacked	= framesnapshotmanager->GetPackedEntity( PrevPackedHandle );
				}
			}

			//now the packed entity contents
			PackedEntity* pNewPacked = framesnapshotmanager->CreateLocalPackedEntity( pSnapshot, pDeltaEntity->m_nSourceIndex );

			pNewPacked->SetServerAndClientClass( pDeltaEntity->m_pServerClass, NULL );
			pNewPacked->SetSnapshotCreationTick( pDeltaEntity->m_nSnapshotCreationTick );		

			//update our entities (which we either have stored, or we need to take from the previous frame)
			if( pDeltaEntity->m_pNewRecipients )
			{
				pNewPacked->SetRecipients( CUtlMemory< CSendProxyRecipients >( pDeltaEntity->m_pNewRecipients, pDeltaEntity->m_nNumRecipients ) );
			}
			else if( pDeltaEntity->m_nNumRecipients > 0 )
			{
				//sanity check that they didn't change, and that we have valid frame of reference
				Assert( pPrevPacked && ( pPrevPacked->GetNumRecipients() == pDeltaEntity->m_nNumRecipients ) );
				pNewPacked->SetRecipients( CUtlMemory< CSendProxyRecipients >( pPrevPacked->GetRecipients(), pPrevPacked->GetNumRecipients() ) );
			}

			//and handle expanding out our serialized values. Either we can have a new object (just steal the properties), no changes (copy the original properties) or a diff
			//(merge the properties)
			if( !pPrevPacked )
			{
				//creation, so just use ours
				pNewPacked->SetPackedData( pDeltaEntity->m_SerializedEntity );
				pDeltaEntity->m_SerializedEntity = SERIALIZED_ENTITY_HANDLE_INVALID;
				//setup a change list for all of our possible properties that is set to our creation time
				CChangeFrameList* pChangeList = new CChangeFrameList( SendTable_GetNumFlatProps( pDeltaEntity->m_pServerClass->m_pTable ), pDeltaEntity->m_nSnapshotCreationTick );
				pNewPacked->SetChangeFrameList( pChangeList );
			}
			else
			{
				const CSerializedEntity* pPrevProps = ( const CSerializedEntity* )pPrevPacked->GetPackedData();

				//copy over our base change list so we can update the times that the properties changed
				CChangeFrameList* pChangeList = new CChangeFrameList( *pPrevPacked->GetChangeFrameList() );
				pNewPacked->SetChangeFrameList( pChangeList );

				//if we don't have a serialized entity, we can just copy our previous (much faster)
				if(pDeltaEntity->m_SerializedEntity == SERIALIZED_ENTITY_HANDLE_INVALID)
				{			
					CSerializedEntity* pNewProps = new CSerializedEntity( );
					pNewProps->Copy( *pPrevProps );
					pNewPacked->SetPackedData( ( SerializedEntityHandle_t )pNewProps );
				}
				else
				{
					//we have to merge our new values onto our old ones
					CSerializedEntity* pDeltaProps = ( CSerializedEntity* )pDeltaEntity->m_SerializedEntity;
					BuildMergedPropertySet( pDeltaProps, pPrevProps, pChangeList, nChangePropTick );
					//transfer ownership of our properties over to this object
					pNewPacked->SetPackedData( ( SerializedEntityHandle_t ) pDeltaProps );
					pDeltaEntity->m_SerializedEntity = SERIALIZED_ENTITY_HANDLE_INVALID;
				}			
			}
		}
		else
		{
			//we don't have this object on this frame, so clear it out
			pCurrEntity->m_nSerialNumber	= -1;
			pCurrEntity->m_pClass			= NULL;
			pCurrEntity->m_pPackedData		= INVALID_PACKED_ENTITY_HANDLE;
		}
	}	

	//Sanity check our expansion
	if( pDeltaFrame->m_pSourceFrame )
		CompareSnapshot( pDeltaFrame->m_pSourceFrame, pDeltaFrame->m_pClientFrame->GetSnapshot(), nChangePropTick );
}

void CHLTVServer::ExpandDeltaFramesToTick( int nTick )
{
	//just expand and add all frames until we find one that is of a later tick
	while ( m_pOldestDeltaFrame )
	{
		SHLTVDeltaFrame_t *pFrame = m_pOldestDeltaFrame;

		//see if our oldest is still too new to decompress (-1 means decompress them all)
		if( ( nTick != -1 ) && ( pFrame->m_pClientFrame->tick_count > nTick ) )
			break;

		//expand the frame
		ExpandDeltaFrameToFullFrame( pFrame );

		//now add this into our frame list
		AddClientFrame( pFrame->m_pClientFrame );

		//give up ownership of it since someone else is now holding onto it
		pFrame->m_pClientFrame = NULL;

		//remove this frame from our list
		m_pOldestDeltaFrame = m_pOldestDeltaFrame->m_pNewerDeltaFrame;
		if( m_pOldestDeltaFrame == NULL )
			m_pNewestDeltaFrame = NULL;

		//and nuke the memory
		delete pFrame;
	}
}

void CHLTVServer::FreeAllDeltaFrames( )
{
	while( m_pOldestDeltaFrame )
	{
		//advance to the next list entry
		SHLTVDeltaFrame_t* pCurrFrame = m_pOldestDeltaFrame;
		m_pOldestDeltaFrame = pCurrFrame->m_pNewerDeltaFrame;

		//free everything about this frame
		delete pCurrFrame;
	}

	//and make sure to completely reset our list
	m_pNewestDeltaFrame = NULL;
}


CClientFrame *CHLTVServer::AddNewFrame( CClientFrame *clientFrame )
{
	VPROF_BUDGET( "CHLTVServer::AddNewFrame", "HLTV" );

	Assert ( clientFrame );
	Assert( clientFrame->tick_count > m_nLastTick );

	m_nLastTick = clientFrame->tick_count;

	m_HLTVFrame.SetSnapshot( clientFrame->GetSnapshot() );
	m_HLTVFrame.tick_count = clientFrame->tick_count;	
	m_HLTVFrame.last_entity = clientFrame->last_entity;
	m_HLTVFrame.transmit_entity = clientFrame->transmit_entity;

	// remember tick of first valid frame
	if ( m_nFirstTick < 0 )
	{
		m_nFirstTick = clientFrame->tick_count;
		m_nTickCount = m_nFirstTick;
		
		if ( !IsMasterProxy() )
		{
			Assert ( m_State == ss_loading );
			m_State = ss_active; // we are now ready to go

			ReconnectClients();
			
			ConMsg("GOTV relay active (%d)\n", GetInstanceIndex() ); // there should only be one relay
			
			Steam3Server().Activate();
			Steam3Server().SendUpdatedServerDetails();

			if ( serverGameDLL )
			{
				serverGameDLL->GameServerSteamAPIActivated( true );
			}
		}
		else
		{
			ConMsg("GOTV[%d] broadcast active.\n", GetInstanceIndex() );
		}
	}
	
	CHLTVFrame *hltvFrame = new CHLTVFrame;

	// copy tickcount & entities from client frame
	hltvFrame->CopyFrame( *clientFrame );

	//copy rest (messages, tempents) from current HLTV frame
	hltvFrame->CopyHLTVData( m_HLTVFrame );
	
	// add frame to HLTV server
	int nClientFrameCount = AddClientFrame( hltvFrame );

	// Only keep the number of packets required to satisfy tv_delay at our tv snapshot rate
	static ConVarRef tv_delay( "tv_delay" );

	float flTvDelayToKeep  = tv_delay.GetFloat();
	if ( spec_replay_enable.GetBool() )
		flTvDelayToKeep = Max( flTvDelayToKeep, spec_replay_message_time.GetFloat() + spec_replay_leadup_time.GetFloat() );

	extern ConVar tv_snapshotrate;
	int numFramesToKeep = 2 * ( ( 1 + Max( 1.0f, flTvDelayToKeep ) ) * int( m_flSnapshotRate ) );
	if ( numFramesToKeep < MAX_CLIENT_FRAMES )
		numFramesToKeep = MAX_CLIENT_FRAMES;
	while ( nClientFrameCount > numFramesToKeep )
	{
		RemoveOldestFrame();
		-- nClientFrameCount;
	}

	// reset HLTV frame for recording next messages etc.
	m_HLTVFrame.Reset();
	m_HLTVFrame.SetSnapshot( NULL );

	return hltvFrame;
}



// Different HLTV servers may have e.g. different snapshot rates. 
// This is a chance for HLTV server to patch up some convars for its clients (like the tv_snapshotrate)
void CHLTVServer::FixupConvars( CNETMsg_SetConVar_t &convars )
{
	if ( GetSnapshotRate() != tv_snapshotrate.GetFloat() ) 
	{
		char rate[ 32 ];
		V_snprintf( rate, sizeof( rate ), "%g", GetSnapshotRate() );
		bool bReplaced = false;
		for ( int i = 0; i < convars.convars().cvars_size(); ++i )
		{
			if ( convars.convars().cvars( i ).name() == "tv_snapshotrate" )
			{
				convars.mutable_convars()->mutable_cvars( i )->set_value( rate );
				bReplaced = true;
				break;
			}
		}
		if ( !bReplaced )
		{
			CMsg_CVars_CVar *pCVar = convars.mutable_convars()->add_cvars();
			pCVar->set_name( "tv_snapshotrate" );
			pCVar->set_value( rate );
		}
	}
}

void CHLTVServer::SendClientMessages( bool bSendSnapshots )
{
	// build individual updates
	for ( int i=0; i< m_Clients.Count(); i++ )
	{
		CHLTVClient* client = Client(i);
		
		// Update Host client send state...
		if ( !client->ShouldSendMessages() )
		{
			continue;
		}

		// Append the unreliable data (player updates and packet entities)
		if ( m_CurrentFrame && client->IsActive() )
		{
			// don't send same snapshot twice
			client->SendSnapshot( m_CurrentFrame );
		}
		else
		{
			// Connected, but inactive, just send reliable, sequenced info.
			client->m_NetChannel->Transmit();
		}

		client->UpdateSendState();
		client->m_fLastSendTime = net_time;
	}
}


bool CHLTVServer::SendClientMessages( CHLTVClient *client )
{
	// Update Host client send state...
	if ( !client->ShouldSendMessages() )
	{
		return false;
	}

	// Append the unreliable data (player updates and packet entities)
	if ( m_CurrentFrame && client->IsActive() )
	{

		// don't send same snapshot twice
		client->SendSnapshot( m_CurrentFrame );
	}
	else
	{
		// Connected, but inactive, just send reliable, sequenced info.
		client->m_NetChannel->Transmit();
	}

	client->UpdateSendState();
	client->m_fLastSendTime = net_time;
	return true;
}



void CHLTVServer::UpdateStats( void )
{
	if ( m_fNextSendUpdateTime > net_time )
		return;

	m_fNextSendUpdateTime = net_time + 8.0f;
	
	// fire game event for everyone
	IGameEvent *event = NULL; 

	if ( !IsMasterProxy() && !m_ClientState.IsConnected() )
	{
		// we are disconnected from SourceTV server
		event = g_GameEventManager.CreateEvent( "hltv_message", true );

		if ( !event )
			return;

		event->SetString( "text", "#GOTV_Reconnecting" );
	}
	else 
	{
		int proxies = 0, slots = 0, clients = 0;
		
		for ( CActiveHltvServerIterator hltv; hltv; hltv.Next() )
		{
			int nLocalProxyCount, nLocalSlotCount, nLocalClientCount;
			hltv->GetGlobalStats( nLocalProxyCount,nLocalSlotCount,nLocalClientCount );
			proxies += nLocalProxyCount;
			slots += nLocalSlotCount;
			clients += nLocalClientCount;
		}

		event = g_GameEventManager.CreateEvent( "hltv_status", true );

		if ( !event )
			return;

//
//		There's no reason to ever record IP addresses in GOTV demo
//
// 		char address[32];
// 
// 		if ( IsMasterProxy() || tv_overridemaster.GetBool() )
// 		{
// 			// broadcast own address
// 			Q_snprintf( address, sizeof(address), "%s:%u", net_local_adr.ToString(true), GetUDPPort() );
// 		}
// 		else
// 		{
// 			// forward address
// 			Q_snprintf( address, sizeof(address), "%s", m_RootServer.ToString() );
// 		}
// 
// 		event->SetString( "master", address );
		event->SetInt( "clients", clients );
		event->SetInt( "slots", slots);
		event->SetInt( "proxies", proxies );

		int numExternalTotalViewers, numExternalLinkedViewers;
		GetExternalStats( numExternalTotalViewers, numExternalLinkedViewers );
		event->SetInt( "externaltotal", numExternalTotalViewers );
		event->SetInt( "externallinked", numExternalLinkedViewers );
	}

	if ( IsMasterProxy() )
	{
		// as a master fire event for every one
		g_GameEventManager.FireEvent( event );
	}
	else
	{
		// as a relay proxy just broadcast event
		BroadcastEvent( event );
	}

}

bool CHLTVServer::NETMsg_PlayerAvatarData( const CNETMsg_PlayerAvatarData& msg )
{
	PlayerAvatarDataMap_t::IndexType_t idxData = m_mapPlayerAvatarData.Find( msg.accountid() );
	if ( idxData != m_mapPlayerAvatarData.InvalidIndex() )
	{
		delete m_mapPlayerAvatarData.Element( idxData );
		m_mapPlayerAvatarData.RemoveAt( idxData );
	}

	CNETMsg_PlayerAvatarData_t *pHtlvDataCopy = new CNETMsg_PlayerAvatarData_t;
	pHtlvDataCopy->CopyFrom( msg );
	m_mapPlayerAvatarData.Insert( pHtlvDataCopy->accountid(), pHtlvDataCopy );

	// Enqueue this message for all fully connected clients immediately
	for ( int iClient = 0; iClient < GetClientCount(); ++iClient )
	{
		CBaseClient *pClient = dynamic_cast< CBaseClient * >( GetClient( iClient ) );
		if ( !pClient->IsActive() )
			continue;

		if ( INetChannel *pNetChannel = pClient->GetNetChannel() )
		{
			pNetChannel->EnqueueVeryLargeAsyncTransfer( *pHtlvDataCopy );
		}
	}

	if ( m_DemoRecorder.IsRecording() )
	{
		m_DemoRecorder.RecordPlayerAvatar( pHtlvDataCopy );
	}

	return true;
}

bool CHLTVServer::SendNetMsg( INetMessage &msg, bool bForceReliable, bool bVoice )
{
	//
	// When sending messages to HLTV client we encrypt some messages with encryption key
	//
	if ( serverGameDLL &&
		( *tv_encryptdata_key.GetString() || *tv_encryptdata_key_pub.GetString() ) &&
		( msg.GetType() != svc_EncryptedData ) )
	{
		EncryptedMessageKeyType_t eKeyType = serverGameDLL->GetMessageEncryptionKey( &msg );
		char const *szEncryptionKey = "";
		switch ( eKeyType )
		{
		case kEncryptedMessageKeyType_Private:
			szEncryptionKey = tv_encryptdata_key.GetString();
			break;
		case kEncryptedMessageKeyType_Public:
			szEncryptionKey = tv_encryptdata_key_pub.GetString();
			break;
		}
		if ( szEncryptionKey && *szEncryptionKey )
		{
			CSVCMsg_EncryptedData_t encryptedMessage;
			if ( !CmdEncryptedDataMessageCodec::SVCMsg_EncryptedData_EncryptMessage( encryptedMessage, &msg, szEncryptionKey ) )
				return false;
			encryptedMessage.set_key_type( eKeyType );
			return SendNetMsg( encryptedMessage, true, false ); // recurse and send the generated messages as reliable
		}
	}

	//
	// Special message handling for avatar data
	//
	if ( msg.GetType() == net_PlayerAvatarData )
	{
		CNETMsg_PlayerAvatarData const *pPlayerAvatarData = dynamic_cast< CNETMsg_PlayerAvatarData * >( &msg );
		if ( !pPlayerAvatarData )
			return false;

		return NETMsg_PlayerAvatarData( *pPlayerAvatarData );
	}

	//
	// Send the actual outgoing message
	//
	if ( m_bSignonState	)
	{
		return msg.WriteToBuffer( m_Signon );
	}

	int buffer = HLTV_BUFFER_UNRELIABLE;	// default destination

	if ( msg.IsReliable() )
	{
		buffer = HLTV_BUFFER_RELIABLE;
	}
	else if ( msg.GetType() == svc_Sounds )
	{
		buffer = HLTV_BUFFER_SOUNDS;
	}
	else if ( msg.GetType() == svc_VoiceData )
	{
		buffer = HLTV_BUFFER_VOICE;
	}
	else if ( msg.GetType() == svc_TempEntities )
	{
		buffer = HLTV_BUFFER_TEMPENTS;
	}

	// anything else goes to the unreliable bin
	return msg.WriteToBuffer( m_HLTVFrame.m_Messages[buffer] );
}

bf_write *CHLTVServer::GetBuffer( int nBuffer )
{
	if ( nBuffer < 0 || nBuffer >= HLTV_BUFFER_MAX )
		return NULL;

	return &m_HLTVFrame.m_Messages[nBuffer];
}

IServer *CHLTVServer::GetBaseServer()
{
	return (IServer*)this;
}

IHLTVDirector *CHLTVServer::GetDirector()
{
	return m_Director;
}

CClientFrame *CHLTVServer::GetDeltaFrame( int nTick )
{
	if ( !tv_deltacache.GetBool() )
		return GetClientFrame( nTick ); //expensive

	AUTO_LOCK_FM( m_FrameCacheMutex ); // we need to lock frame cache because we're potentially modifying it from multiple sendPacket threads
	// TODO make that a utlmap
	FOR_EACH_VEC( m_FrameCache, iFrame )
	{
		if ( m_FrameCache[iFrame].nTick == nTick )
			return m_FrameCache[iFrame].pFrame;
	}

	int i = m_FrameCache.AddToTail();

	CFrameCacheEntry_s &entry = m_FrameCache[i];

	entry.nTick = nTick;
	entry.pFrame = GetClientFrame( nTick ); //expensive

	return entry.pFrame;
}


CClientFrame *CHLTVServer::ExpandAndGetClientFrame( int nTick, bool bExact )
{
	ExpandDeltaFramesToTick( nTick );
	return GetClientFrame( nTick, bExact );
}


void CHLTVServer::RunFrame()
{
	VPROF_BUDGET( "CHLTVServer::RunFrame", "HLTV" );

	// update network time etc
	NET_RunFrame( Plat_FloatTime() );

	if ( m_ClientState.m_nSignonState > SIGNONSTATE_NONE )
	{
		// process data from net socket
		NET_ProcessSocket( m_ClientState.m_Socket, &m_ClientState );

		m_ClientState.RunFrame();
		
		m_ClientState.SendPacket();
	}

	// check if HLTV server if active
	if ( !IsActive() )
		return;

	if ( host_frametime > 0 )
	{
		m_flFPS = m_flFPS * 0.99f + 0.01f/host_frametime;
	}

	if ( IsPlayingBack() )
		return;

	// get current tick time for director module and restore 
	// world (stringtables, framebuffers) as they were at this time
	UpdateTick();

	// Run any commands from client and play client Think functions if it is time.
	CBaseServer::RunFrame();

	UpdateStats();

	SendClientMessages( true );

	// Update the Steam server if we're running a relay.
	if ( !sv.IsActive() )
		Steam3Server().RunFrame();
	
	UpdateMasterServer();
}

void CHLTVServer::UpdateTick( void )
{
	VPROF_BUDGET( "CHLTVServer::UpdateTick", "HLTV" );

	if ( m_nFirstTick < 0 )
	{
		m_nTickCount = 0;
		m_CurrentFrame = NULL;
		return;
	}

	// set tick time to last frame added
	int nNewTick = m_nLastTick;

	if ( IsMasterProxy() )
	{
		// get tick from director, he decides delay etc
		nNewTick = Max( m_nFirstTick, m_Director->GetDirectorTick() );
#if HLTV_REPLAY_ENABLED
		if ( spec_replay_enable.GetBool() )
		{
			nNewTick = Max< int >( nNewTick, m_nLastTick - ( spec_replay_message_time.GetFloat() + spec_replay_leadup_time.GetFloat() ) / m_flTickInterval );
		}
#endif
	}

	//handle expanding any delta frames we have accumulated up to this point
	ExpandDeltaFramesToTick( nNewTick );

	// the the closest available frame
	CHLTVFrame *newFrame = (CHLTVFrame*) GetClientFrame( nNewTick, false );

	if ( newFrame == NULL )
		return; // we dont have a new frame
	
	if ( m_CurrentFrame == newFrame )
		return;	// current frame didn't change

	m_CurrentFrame = newFrame;
	m_nTickCount = m_CurrentFrame->tick_count;
	
	if ( IsMasterProxy() )
	{
		// now do master proxy stuff

		// restore string tables for this time
		RestoreTick( m_nTickCount );

		// remove entities out of current PVS
		if ( tv_transmitall.GetBool() == false )
		{
			EntityPVSCheck( m_CurrentFrame );	
		}

		if ( ( m_DemoRecorder.IsRecording() || m_Broadcast.IsRecording() ) && m_CurrentFrame )
		{
			if ( m_DemoRecorder.IsRecording() )
				m_DemoRecorder.WriteFrame( m_CurrentFrame, &m_DemoEventWriteBuffer );
			if ( m_Broadcast.IsRecording() )
				m_Broadcast.WriteFrame( m_CurrentFrame, &m_DemoEventWriteBuffer );
			m_DemoEventWriteBuffer.Reset();
		}
	}
	else
	{
		// delta entity cache works only for relay proxies
		m_DeltaCache.SetTick( m_CurrentFrame->tick_count, m_CurrentFrame->last_entity+1 );
	}

	int removeTick = m_nTickCount - tv_window_size.GetFloat() / m_flTickInterval; // keep 16 seconds buffer

	if ( removeTick > 0 )
	{
		DeleteClientFrames( removeTick );
	}

	m_FrameCache.RemoveAll();
}

const char *CHLTVServer::GetName( void ) const
{
	return tv_name.GetString();
}

void CHLTVServer::FillServerInfo(CSVCMsg_ServerInfo &serverinfo)
{
	CBaseServer::FillServerInfo( serverinfo );

	serverinfo.set_player_slot( m_nPlayerSlot ); // all spectators think they're the HLTV client
	serverinfo.set_max_clients( m_nGameServerMaxClients );
}

void CHLTVServer::Clear( void )
{
	CBaseServer::Clear();

	m_Director = NULL;
	m_MasterClient = NULL;
	m_ClientState.Clear();
	m_Server = NULL;
	m_nFirstTick = -1;
	m_nLastTick = 0;
	m_nTickCount = 0;
	m_nPlayerSlot = 0;
	m_flStartTime = 0.0f;
	m_nViewEntity = 1;
	m_nGameServerMaxClients = 0;
	m_fNextSendUpdateTime = 0.0f;

	Changelevel( false );
}

void CHLTVServer::Init(bool bIsDedicated)
{
	CBaseServer::Init( bIsDedicated );

	m_Socket = NS_HLTV + m_nInstanceIndex; 
	COMPILE_TIME_ASSERT(NS_HLTV1 == NS_HLTV + 1 );
	
	// check if only master proxy is allowed, no broadcasting
	if ( CommandLine()->FindParm("-tvmasteronly") )
	{
		m_bMasterOnlyMode = true;
	}
}

void CHLTVServer::Changelevel( bool bInactivateClients )
{
	m_Broadcast.StopRecording();// We can't broadcast after level change, because the broadcast manifest (which includes map name) is immutable during broadcast.
	StopRecordingAndFreeFrames( true );

	if ( bInactivateClients )
	{
		InactivateClients();
	}

	m_CurrentFrame = NULL;

	m_HLTVFrame.FreeBuffers();
	m_vPVSOrigin.Init();

	DeleteClientFrames( -1 );

	m_DeltaCache.Flush();
	m_FrameCache.RemoveAll();


	//free any frames that we may have had outstanding
	FreeAllDeltaFrames();

	//release any snapshots that we were referencing for delta frame construction
	if( m_pLastSourceSnapshot )
	{
		m_pLastSourceSnapshot->ReleaseReference();
		m_pLastSourceSnapshot = NULL;
	}
	if( m_pLastTargetSnapshot )
	{
		m_pLastTargetSnapshot->ReleaseReference();
		m_pLastTargetSnapshot = NULL;
	}

	// Free all avatar data
	m_mapPlayerAvatarData.PurgeAndDeleteElements();
}

void CHLTVServer::GetNetStats( float &avgIn, float &avgOut )
{
	CBaseServer::GetNetStats( avgIn, avgOut	);

	if ( m_ClientState.IsActive() )
	{
		avgIn += m_ClientState.m_NetChannel->GetAvgData(FLOW_INCOMING);
		avgOut += m_ClientState.m_NetChannel->GetAvgData(FLOW_OUTGOING);
	}
}

void CHLTVServer::Shutdown( void )
{
	m_nExternalTotalViewers = 0;
	m_nExternalLinkedViewers = 0;

	//stop any recording, and free our client frame list
	m_Broadcast.StopRecording();
	StopRecordingAndFreeFrames( true );
	UninstallStringTables();

	if ( IsMasterProxy() )
	{
		if ( m_MasterClient )
			m_MasterClient->Disconnect( "GOTV stop." );

		if ( m_Director )
			m_Director->RemoveHLTVServer( this );
	}
	else
	{
		// do not try to reconnect to old connection
		m_ClientState.m_Remote.RemoveAll();

		m_ClientState.Disconnect();
	}

	g_GameEventManager.RemoveListener( this );

	CBaseServer::Shutdown();
}

CDemoFile *CHLTVServer::GetDemoFile()
{
	return &m_DemoFile;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHLTVServer::IsPlayingBack( void )const
{
	return m_bPlayingBack;
}

bool CHLTVServer::IsPlaybackPaused()const
{
	return m_bPlaybackPaused;	
}

float CHLTVServer::GetPlaybackTimeScale()
{
	return m_flPlaybackRateModifier;
}

void CHLTVServer::SetPlaybackTimeScale(float timescale)
{
	m_flPlaybackRateModifier = timescale;
}

void CHLTVServer::ResyncDemoClock()
{
	m_nStartTick = host_tickcount;
}

int CHLTVServer::GetPlaybackStartTick( void )
{
	return m_nStartTick;
}

int	CHLTVServer::GetPlaybackTick( void )
{
	return host_tickcount - m_nStartTick;
}


bool CHLTVServer::StartPlayback( const char *filename, bool bAsTimeDemo, CDemoPlaybackParameters_t const *pPlaybackParameters, int nStartingTick )
{
	Clear();

	if ( !m_DemoFile.Open( filename, true )  )
	{
		return false;
	}

	// Read in the m_DemoHeader
	demoheader_t *dh = m_DemoFile.ReadDemoHeader( pPlaybackParameters );

	if ( !dh )
	{
		ConMsg( "Failed to read demo header.\n" );
		m_DemoFile.Close();
		return false;
	}
	
	// create a fake channel with a NULL address (no encryption)
	m_ClientState.m_NetChannel = NET_CreateNetChannel( NS_CLIENT, NULL, "DEMO", &m_ClientState, NULL, false );

	if ( !m_ClientState.m_NetChannel )
	{
		ConMsg( "CDemo::Play: failed to create demo net channel\n" );
		m_DemoFile.Close();
		return false;
	}
	
	m_ClientState.m_NetChannel->SetTimeout( -1.0f );	// never timeout


	// Now read in the directory structure.

	m_bPlayingBack = true;

	ConMsg( "Reading complete demo file at once...\n");

	double start = Plat_FloatTime();

	ReadCompleteDemoFile();

	double diff; diff = Plat_FloatTime() - start;
	ConMsg( "Reading time :%.4f\n", diff );

	NET_RemoveNetChannel( m_ClientState.m_NetChannel, true );
	m_ClientState.m_NetChannel = NULL;

	return true;
}

void CHLTVServer::ReadCompleteDemoFile()
{
	int			tick = 0;
	byte		cmd = dem_signon;
	char		buffer[NET_MAX_PAYLOAD];
	netpacket_t	demoPacket;

	// setup demo packet data buffer
	Q_memset( &demoPacket, 0, sizeof(demoPacket) );
	demoPacket.from.SetAddrType( NSAT_NETADR );
	demoPacket.from.AsType<netadr_t>().SetType( NA_LOOPBACK);
	
	while ( true )
	{
		int nPlayerSlot = 0;
		m_DemoFile.ReadCmdHeader( cmd, tick, nPlayerSlot );

		// COMMAND HANDLERS
		switch ( cmd )
		{
		case dem_synctick:
			ResyncDemoClock();
			break;
		case dem_stop:
			// MOTODO we finished reading the file
			return ;
		case dem_consolecmd:
			{
#ifndef DEDICATED
				ACTIVE_SPLITSCREEN_PLAYER_GUARD( nPlayerSlot );
#endif
				CNETMsg_StringCmd_t cmdmsg( m_DemoFile.ReadConsoleCommand() );
				m_ClientState.NETMsg_StringCmd( cmdmsg );
			}
			break;
		case dem_datatables:
			{
				ALIGN4 char data[64*1024] ALIGN4_POST;
				bf_read buf( "dem_datatables", data, sizeof(data) );
				
				m_DemoFile.ReadNetworkDataTables( &buf );
				buf.Seek( 0 );

				// support for older engine demos
				if ( !DataTable_LoadDataTablesFromBuffer( &buf, m_DemoFile.m_DemoHeader.demoprotocol ) )
				{
					Host_Error( "Error parsing network data tables during demo playback." );
				}
			}
			break;
		case dem_stringtables:
			{
				void *data = malloc( 512*1024 ); // X360TBD: How much memory is really needed here?
				bf_read buf( "dem_stringtables", data, 512*1024 );

				m_DemoFile.ReadStringTables( &buf );
				buf.Seek( 0 );

				if ( !networkStringTableContainerClient->ReadStringTables( buf ) )
				{
					Host_Error( "Error parsing string tables during demo playback." );
				}

				free( data );
			}
			break;
		case dem_usercmd:
			{
#ifndef DEDICATED
				ACTIVE_SPLITSCREEN_PLAYER_GUARD( nPlayerSlot );
#endif
				char buffer[256];
				int  length = sizeof(buffer);
				m_DemoFile.ReadUserCmd( buffer, length );
				// MOTODO HLTV must store user commands too
			}
			break;
		case dem_signon:
		case dem_packet:
			{
				int inseq, outseqack = 0;

				m_DemoFile.ReadCmdInfo( m_LastCmdInfo );	// MOTODO must be stored somewhere
				m_DemoFile.ReadSequenceInfo( inseq, outseqack );

				int length = m_DemoFile.ReadRawData( buffer, sizeof(buffer) );

				if ( length > 0 )
				{
					// succsessfully read new demopacket
					demoPacket.received = realtime;
					demoPacket.size = length;
					demoPacket.message.StartReading( buffer,  length );

					m_ClientState.m_NetChannel->ProcessPacket( &demoPacket, false );
				}
			}
	
			break;
		}
	}
}

#define DEBUG_GOTV_RELAY_LOCAL 0
#if DEBUG_GOTV_RELAY_LOCAL
ConVar debug_gotv_relay_whitelist_ports( "debug_gotv_relay_whitelist_ports", "[27005][27006][27007]" ); // run 3 game servers first
#endif
int	CHLTVServer::GetChallengeType ( const ns_address &adr )
{
	if ( serverGameDLL && serverGameDLL->IsValveDS() )
	{
#if DEBUG_GOTV_RELAY_LOCAL
		if ( strstr( debug_gotv_relay_whitelist_ports.GetString(), CFmtStr( "[%u]", adr.GetPort() ) ) )
			return PROTOCOL_HASHEDCDKEY; // When debugging on local machine only make exception for relay proxies, clients actually auth via SteamID
#else
		extern bool IsHltvRelayProxyWhitelisted( ns_address const &adr );
		if ( IsHltvRelayProxyWhitelisted( adr ) )
			return PROTOCOL_HASHEDCDKEY; // HLTV makes an exception for the requesting relay proxy address
#endif

		return CBaseServer::GetChallengeType( adr );
	}

	return PROTOCOL_HASHEDCDKEY; // HLTV doesn't need Steam authentication
}

static bool Helper_HLTV_VerifyOfficialPassword( const char *szPassword )
{
	if ( !szPassword || !szPassword[0] )
		return false;
	if ( Q_strlen( szPassword ) != 32 )
		return false;
	char chSignHash[16] = {0};
	Q_snprintf( chSignHash, ARRAYSIZE( chSignHash ), "%08lX", CRC32_ProcessSingleBuffer( szPassword, 24 ) );
	return ( 0 == Q_strncmp( chSignHash, szPassword + 24, 8 ) );
}

static char const *Helper_HLTV_GenerateUniquePassword()
{
	return "HLTV Official Password Must Be Encrypted";
}

#if 0
CON_COMMAND( debug_make_hltv_encrypted_password, "" )
{
	char const *szPasswordProvidedByClient = args.Arg( 1 );
	if ( !szPasswordProvidedByClient || !*szPasswordProvidedByClient || ( Q_strlen( szPasswordProvidedByClient ) != 32 ) )
	{
		Warning( "Bad password!\n" );
		return;
	}

	char chClientHash[64]={0};
	Q_snprintf( chClientHash, ARRAYSIZE( chClientHash ), "%08X%08X%08X",
		CRC32_ProcessSingleBuffer( szPasswordProvidedByClient, 32 ),
		CRC32_ProcessSingleBuffer( szPasswordProvidedByClient + 10, 22 ),
		CRC32_ProcessSingleBuffer( szPasswordProvidedByClient + 20, 12 ) );
	Q_snprintf( chClientHash + 24, ARRAYSIZE( chClientHash ) - 24, "%08X",
		CRC32_ProcessSingleBuffer( chClientHash, 24 ) );

	Msg( "{%s}->{%s}\n", szPasswordProvidedByClient, chClientHash );
}
#endif

bool CHLTVServer::CheckHltvPasswordMatch( const char *szPasswordProvidedByClient, const char *szServerRequiredPassword, CSteamID steamidClient )
{
	// Official servers must have a special encrypted password
	if ( serverGameDLL && serverGameDLL->IsValveDS() )
	{
		if ( !Helper_HLTV_VerifyOfficialPassword( szServerRequiredPassword ) )
		{
			ExecuteNTimes( 3, Warning( "WARNING: %s (%s)\n", Helper_HLTV_GenerateUniquePassword(), szServerRequiredPassword ? szServerRequiredPassword : "none" ) );
			return false; // without the encrypted password clients cannot connect
		}

		if ( !szPasswordProvidedByClient || !szPasswordProvidedByClient[0] )
			return false;	// server requires a password, but client didn't provide a password

		// Enforce client password length to be 32 characters
		if ( Q_strlen( szPasswordProvidedByClient ) == 32 )
		{
			// Compute a client password hash
			char chClientHash[64]={0};
			Q_snprintf( chClientHash, ARRAYSIZE( chClientHash ), "%08lX%08lX%08lX",
				CRC32_ProcessSingleBuffer( szPasswordProvidedByClient, 32 ),
				CRC32_ProcessSingleBuffer( szPasswordProvidedByClient + 10, 22 ),
				CRC32_ProcessSingleBuffer( szPasswordProvidedByClient + 20, 12 ) );
			if ( !Q_strncmp( chClientHash, szServerRequiredPassword, 24 ) )
				return true;
		}
	}
	else
	{
		if ( !szServerRequiredPassword || !szServerRequiredPassword[0] || !Q_stricmp( szServerRequiredPassword, "none" ) )
			return true;	// server doesn't require a password, allow client
		if ( !szPasswordProvidedByClient || !szPasswordProvidedByClient[0] )
			return false;	// server requires a password, but client didn't provide a password
		if ( !Q_strcmp( szPasswordProvidedByClient, szServerRequiredPassword ) )
			return true;	// compare passwords
	}

	//
	// Check if client is connecting via watchable reservation
	//
	if ( tv_advertise_watchable.GetBool() && sv.IsReserved() && sv.GetReservationCookie() &&
		szPasswordProvidedByClient && ( Q_strlen( szPasswordProvidedByClient ) == 64 ) )
	{
		// Decode client TV watchable password ascii
		unsigned char chEncryptedPassword[ 32 + 1 ] = {0};
		for ( int k = 0; k < 32; ++ k )
		{
			char chScan[5] = { '0', 'x', szPasswordProvidedByClient[2*k], szPasswordProvidedByClient[2*k+1], 0 };
			uint32 uiByte = 0;
			sscanf( chScan, "0x%02X", &uiByte );
			chEncryptedPassword[k] = uiByte;
		}

		// Decrypt the encoded password
		IceKey iceKey( 2 );
		if ( iceKey.keySize() == 16 )
		{
			iceKey.set( ( unsigned char * ) CFmtStr( "%016llX", sv.GetReservationCookie() ).Access() );
			char chDecryptedPassword[ 32 + 1 ] = {0};
			for ( int k = 0; k < 32; k += iceKey.blockSize() )
			{
				iceKey.decrypt( chEncryptedPassword + k, ( unsigned char * ) chDecryptedPassword + k );
			}
			if ( !Q_strcmp( chDecryptedPassword, CFmtStr( "WATCH100%08X%016llX", steamidClient.GetAccountID(), sv.GetReservationCookie() ).Access() ) )
				return true;
		}
	}

	return false;
}

const char *CHLTVServer::GetPassword() const
{
	const char *password = tv_password.GetString();

	// Official servers must have a special encrypted password
	if ( serverGameDLL && serverGameDLL->IsValveDS() && !Helper_HLTV_VerifyOfficialPassword( password ) )
		return Helper_HLTV_GenerateUniquePassword();

	// if password is empty or "none", return NULL
	if ( !password[0] || !Q_stricmp(password, "none" ) )
	{
		return NULL;
	}

	return password;
}

const char *CHLTVServer::GetHltvRelayPassword() const
{
	static ConVarRef tv_relaypassword( "tv_relaypassword" );
	const char *password = tv_relaypassword.GetString();

	if ( serverGameDLL && serverGameDLL->IsValveDS() && !Helper_HLTV_VerifyOfficialPassword( password ) )
		return Helper_HLTV_GenerateUniquePassword();

	// if password is empty or "none", return NULL
	if ( !password[0] || !Q_stricmp(password, "none" ) )
	{
		return NULL;
	}

	return password;
}

IClient *CHLTVServer::ConnectClient ( const ns_address &adr, int protocol, int challenge, int authProtocol, 
									 const char *name, const char *password, const char *hashedCDkey, int cdKeyLen,
									CUtlVector< CCLCMsg_SplitPlayerConnect_t * > & splitScreenClients, bool isClientLowViolence, CrossPlayPlatform_t clientPlatform,
									const byte *pbEncryptionKey, int nEncryptionKeyIndex )
{
	IClient	*client = (CHLTVClient*)CBaseServer::ConnectClient( 
		adr, protocol, challenge,authProtocol, name, password, hashedCDkey, cdKeyLen, splitScreenClients, isClientLowViolence, clientPlatform,
		pbEncryptionKey, nEncryptionKeyIndex );

	if ( client )
	{
		// remember password
		CHLTVClient *pHltvClient = (CHLTVClient*)client;
		Q_strncpy( pHltvClient->m_szPassword, password, sizeof(pHltvClient->m_szPassword) );
	}

	return client;
}

bool CHLTVServer::GetRedirectAddressForConnectClient( const ns_address &adr, CUtlVector< CCLCMsg_SplitPlayerConnect_t* > & splitScreenClients, ns_address *pNetAdrRedirect )
{
	bool bConnectingClientIsTvRelay = false;

	if ( splitScreenClients.Count() )
	{
		const CMsg_CVars& convars = splitScreenClients[0]->convars();
		for ( int i = 0; i< convars.cvars_size(); ++i )
		{
			const char *cvname = NetMsgGetCVarUsingDictionary( convars.cvars(i) );
			const char *value = convars.cvars(i).value().c_str();
				
			if ( stricmp( cvname, "tv_relay" ) )
				continue;

			bConnectingClientIsTvRelay = ( value[0] == '1' );
			break;
		}
	}

	if ( bConnectingClientIsTvRelay )
		return false;

	// This is a human spectator, check if we should dispatch them down the chain
	//
	// The section below is largely a copy of DispatchToRelay function
	//

	if ( tv_dispatchmode.GetInt() <= DISPATCH_MODE_OFF )
		return false; // don't redirect
	
	CBaseClient	*pBestProxy = NULL;
	float fBestRatio = 1.0f;

	// find best relay proxy
	for (int i=0; i < GetClientCount(); i++ )
	{
		CBaseClient *pProxy = m_Clients[ i ];

		// check all known proxies
		if ( !pProxy->IsConnected() || !pProxy->IsHLTV() )
			continue;

		int slots = Q_atoi( pProxy->GetUserSetting( "hltv_slots" ) );
		int clients = Q_atoi( pProxy->GetUserSetting( "hltv_clients" ) );

		// skip overloaded proxies or proxies with no slots at all
		if ( (clients > slots) || slots <= 0 )
			continue;

		// calc clients/slots ratio for this proxy
		float ratio = ((float)(clients))/((float)slots);

		if ( ratio < fBestRatio )
		{
			fBestRatio = ratio;
			pBestProxy = pProxy;
		}
	}

	if ( pBestProxy == NULL )
	{
		if ( tv_dispatchmode.GetInt() == DISPATCH_MODE_ALWAYS )
		{
			// we are in always forward mode, drop client if we can't forward it
			pNetAdrRedirect->Clear();
			RejectConnection( adr, "No GOTV relay available" );
			return true;
		}
		else
		{
			// just let client connect to this proxy
			return false;
		}
	}

	// check if client should stay on this relay server unless we are the master,
	// masters always prefer to send clients to relays
	if ( (tv_dispatchmode.GetInt() == DISPATCH_MODE_AUTO) && (GetMaxClients() > 0) )
	{
		// ratio = clients/slots. give relay proxies 25% bonus
		int numSlots = GetMaxClients();
		if ( tv_maxclients_relayreserved.GetInt() > 0 )
			numSlots -= tv_maxclients_relayreserved.GetInt();
		numSlots = MAX( 0, numSlots );

		int numClients = GetNumClients();
		if ( numClients > numSlots )
			numSlots = numClients;

		float flDispatchWeight = tv_dispatchweight.GetFloat();
		if ( flDispatchWeight <= 1.01 )
			flDispatchWeight = 1.01;
		float myRatio = ((float)numClients/(float)numSlots) * flDispatchWeight;

		myRatio = MIN( myRatio, 1.0f ); // clamp to 1

		// if we have a better local ratio then other proxies, keep this client here
		if ( myRatio < fBestRatio )
			return false;	// don't redirect
	}

	CFmtStr fmtAdditionalInfo;
	const char *pszRelayAddr = pBestProxy->GetUserSetting( "hltv_addr" );
	if ( !pszRelayAddr )
		return false;

	// If the client is attempting a connection over SDR and the relay downstream allows
	// connection over SDR, then redirect to SDR port instead
	switch ( adr.GetAddressType() )
	{
	case NSAT_PROXIED_CLIENT:
		if ( const char *pszRelaySdrAddr = pBestProxy->GetUserSetting("hltv_sdr") )
		{
			if ( *pszRelaySdrAddr )
			{
				ns_address nsadrsdr;
				if ( nsadrsdr.SetFromString( pszRelaySdrAddr ) && ( nsadrsdr.GetAddressType() == NSAT_PROXIED_GAMESERVER ) )
				{
					// Ensure that client gets a ticket for the new SDR address
					// and that the game server allows redirect
					if ( serverGameDLL->IsValveDS() && serverGameDLL->OnEngineClientProxiedRedirect(
						adr.m_steamID.GetSteamID().ConvertToUint64(), pszRelaySdrAddr, pszRelayAddr ) )
					{
						//
						// Build a P2P HLTV channel SDR address for client redirect
						//
						fmtAdditionalInfo.AppendFormat( " @ %s SDR:%d", pszRelayAddr, nsadrsdr.m_steamID.GetSteamChannel() );
						nsadrsdr.m_steamID.SetSteamChannel( STEAM_P2P_HLTV );
						ns_address_render nsadrRendered( nsadrsdr );
						char *pchStackCopy = ( char * ) stackalloc( 1 + V_strlen( nsadrRendered.String() ) );
						V_strcpy( pchStackCopy, nsadrRendered.String() );
						pszRelayAddr = pchStackCopy;
					}
				}
			}
		}
		break;
	}
	
	ConMsg( "Redirecting spectator connect packet from %s to GOTV relay %s%s\n",
		ns_address_render( adr ).String(), 
		pszRelayAddr, fmtAdditionalInfo.Access() );

	// tell the client to connect to this new address
	pNetAdrRedirect->SetFromString( pszRelayAddr );
    		
 	// increase this proxies client number in advance so this proxy isn't used again next time
	int clients = Q_atoi( pBestProxy->GetUserSetting( "hltv_clients" ) );
	pBestProxy->SetUserCVar( "hltv_clients", va("%d", clients+1 ) );
	
	return true;
}

CON_COMMAND( tv_status, "Show GOTV server status." ) 
{
	int		slots, proxies,	clients;
	float	in, out;
	char	gd[MAX_OSPATH];

	Q_FileBase( com_gamedir, gd, sizeof( gd ) );

	for ( CActiveHltvServerSelector hltv( args ); hltv; hltv.Next() )
	{
		hltv->GetNetStats( in, out );

		in /= 1024; // as KB
		out /= 1024;

		ConMsg( "--- GOTV[%u] Status ---\n", hltv.GetIndex() );
		ConMsg( "Online %s, FPS %.1f, Version %i (%s)\n",
			COM_FormatSeconds( hltv->GetOnlineTime() ), hltv->m_flFPS, build_number(),
#if defined( _WIN32 )
			"Win32"
#else
			"Linux"
#endif
			);

		if ( hltv->IsDemoPlayback() )
		{
			ConMsg( "Playing Demo File \"%s\"\n", "TODO demo file name" );
		}
		else if ( hltv->IsMasterProxy() )
		{
			ConMsg( "Master \"%s\", delay %.0f, rate %.1f\n", hltv->GetName(), hltv->GetDirector()->GetDelay(), hltv->GetSnapshotRate() );
		}
		else // if ( m_Server->IsRelayProxy() )
		{
			if ( hltv->GetRelayAddress() )
			{
				ConMsg( "Relay \"%s\", connect to %s\n", hltv->GetName(), hltv->GetRelayAddress()->ToString() );
			}
			else
			{
				ConMsg( "Relay \"%s\", not connect.\n", hltv->GetName() );
			}
		}

		ConMsg( "Game Time %s, Mod \"%s\", Map \"%s\", Players %i\n", COM_FormatSeconds( hltv->GetTime() ),
			gd, hltv->GetMapName(), hltv->GetNumPlayers() );

		ConMsg( "Local IP %s:%i, KB/sec In %.1f, Out %.1f\n",
			net_local_adr.ToString( true ), hltv->GetUDPPort(), in, out );

		hltv->GetLocalStats( proxies, slots, clients );

		ConMsg( "Local Slots %i, Spectators %i, Proxies %i\n",
			slots, clients - proxies, proxies );

		hltv->GetGlobalStats( proxies, slots, clients );

		ConMsg( "Total Slots %i, Spectators %i, Proxies %i\n",
			slots, clients - proxies, proxies );

		hltv->GetExternalStats( slots, clients );
		if ( slots > 0 )
		{
			if ( clients > 0 )
				ConMsg( "Streaming spectators %i, linked to Steam %i\n", slots, clients );
			else
				ConMsg( "Streaming spectators %i\n", slots );
		}

		if ( hltv->m_DemoRecorder.IsRecording() )
		{
			ConMsg( "Recording to \"%s\", length %s.\n", hltv->m_DemoRecorder.GetDemoFilename(),
				COM_FormatSeconds( host_state.interval_per_tick * hltv->m_DemoRecorder.GetRecordingTick() ) );
		}

		if ( hltv->m_Broadcast.IsRecording() )
		{
			ConMsg( "Broadcasting\n" );
		}

		ConMsg( "\n" );

		extern ConVar host_name;
		ConMsg( "hostname: %s\n", host_name.GetString() );
		// the header for the status rows
		ConMsg( "# userid name uniqueid connected ping loss state rate adr\n" );
		for ( int j = 0; j < hltv->GetClientCount(); j++ )
		{
			IClient	*client = hltv->GetClient( j );
			if ( !client || !client->IsConnected() )
				continue; // not connected yet, maybe challenging

			extern void Host_Status_PrintClient( IClient *client, bool bShowAddress, void( *print ) ( const char *fmt, ... ) );
			Host_Status_PrintClient( client, true, ConMsg );
		}
	}
	ConMsg( "#end\n" );
}

CON_COMMAND( sv_getinfo, "Show user info of a connected client" )
{
	if ( args.ArgC() < 4 )
	{
		ConMsg( "Usage:  userinfo_show [sv|tv0|tv1] [id] [var]\n" );
		return;
	}

	CBaseServer *psv = &sv;
	if ( char const *szTvN = StringAfterPrefix( args.Arg( 1 ), "tv" ) )
	{
		int nTV = V_atoi( szTvN );
		nTV = clamp( nTV, 0, HLTV_SERVER_MAX_COUNT - 1 );
		psv = g_pHltvServer[ nTV ];
		if ( !psv )
		{
			ConMsg( "TV%d not active\n", nTV );
			return;
		}
	}
	else if ( !psv )
	{
		ConMsg( "Main server not active\n" );
		return;
	}

	int nClientID = V_atoi( args.Arg( 2 ) );
	if ( nClientID < 0 || nClientID >= psv->GetClientCount() )
	{
		ConMsg( "Found %d clients on server\n", psv->GetClientCount() );
		return;
	}

	IClient *pClient = psv->GetClient( nClientID );
	if ( !pClient || !pClient->IsConnected() )
	{
		ConMsg( "Client #%d is %s\n", nClientID, pClient ? "not connected" : "null" );
		return;
	}

	const char *pszVar = args.Arg( 3 );
	const char *pszValue = pClient->GetUserSetting( pszVar );
	ConMsg( "Client #%d '%s'<%s> '%s'='%s'\n", nClientID, pClient->GetClientName(), pClient->GetNetworkIDString(), pszVar, pszValue );
}

CON_COMMAND( tv_relay, "Connect to GOTV server and relay broadcast." )
{
	if ( args.ArgC() < 2 )
	{
		ConMsg( "Usage:  tv_relay <ip:port> [-instance <inst>]\n" );
		return;
	}

	const char *address = args.ArgS();

	// If it's not a single player connection to "localhost", initialize networking & stop listenserver
	if ( StringHasPrefixCaseSensitive( address, "localhost" ) )
	{
		ConMsg( "GOTV can't connect to localhost.\n" );
		return;
	}

	int nHltvIndex = clamp( args.FindArgInt( "-instance", 0 ), 0, HLTV_SERVER_MAX_COUNT - 1 );
 	CHLTVServer * & hltv = g_pHltvServer[ nHltvIndex ];

	if ( !hltv )
	{
		hltv = new CHLTVServer( nHltvIndex, ( nHltvIndex ? tv_snapshotrate1.GetFloat() : tv_snapshotrate.GetFloat() ) );
		hltv->Init( NET_IsDedicated() );
	}

	if ( hltv->m_bMasterOnlyMode )
	{
		ConMsg("GOTV[%d] in Master-Only mode.\n", nHltvIndex );
		return;
	}

	// If the main server instance is running then we want to re-use it's
	// logged on anonymous SteamID
	if ( sv.IsDedicated() && sv.IsActive() )
		sv.FlagForSteamIDReuseAfterShutdown();

	// shutdown anything else
	Host_Disconnect( false );

	// start networking
	NET_Init( NET_IsDedicated() );
	NET_SetMultiplayer( true );	

	hltv->ConnectRelay( address );
}

CON_COMMAND( tv_stop, "Stops the GOTV broadcast [-instance <inst> ]" )
{
	for ( CActiveHltvServerSelector hltv( args ); hltv; hltv.Next() )
	{
		int nClients = hltv->GetNumClients();

		hltv->Shutdown();

		ConMsg( "GOTV[%u] stopped, %i clients disconnected.\n", hltv.GetIndex(), nClients );
	}
}

CON_COMMAND( tv_retry, "Reconnects the GOTV relay proxy " )
{
	for ( CActiveHltvServerSelector hltv( args ); hltv; hltv.Next() )
	{
		if ( hltv->m_bMasterOnlyMode )
		{
			ConMsg( "GOTV[%u] in Master-Only mode.\n", hltv.GetIndex() );
			return;
		}

		if ( !hltv->m_ClientState.m_Remote.Count() )
		{
			ConMsg( "Can't retry, no previous GOTV[%u] connection\n", hltv.GetIndex() );
			return;
		}

		ConMsg( "Commencing GOTV[%u] connection retry to %s\n", hltv.GetIndex(), hltv->m_ClientState.m_Remote.Get( 0 ).m_szRetryAddress.String() );
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), va( "tv_relay %s\n", hltv->m_ClientState.m_Remote.Get( 0 ).m_szRetryAddress.String() ) );
	}
}



CON_COMMAND( tv_record, "Starts GOTV demo recording [-instance <inst> ]" )
{
	if ( args.ArgC() < 2 )
	{
		ConMsg( "Usage:  tv_record  <filename> [-instance <inst> ]\n" );
		return;
	}

	int nHltvInstance = clamp( args.FindArgInt( "-instance", 0 ), 0, HLTV_SERVER_MAX_COUNT );
	CHLTVServer * hltv = g_pHltvServer[ nHltvInstance ];
	if ( hltv && hltv->IsActive() )
	{
		if ( !hltv->IsMasterProxy() )
		{
			ConMsg( "GOTV[%u]: Only GOTV Master can record demos instantly.\n", nHltvInstance );
			return;
		}

		if ( hltv->m_DemoRecorder.IsRecording() )
		{
			ConMsg( "GOTV[%u] already recording to %s.\n", nHltvInstance, hltv->m_DemoRecorder.GetDemoFilename() );
			return;
		}

		// check path first
		if ( !COM_IsValidPath( args[ 1 ] ) )
		{
			ConMsg( "record %s: invalid path.\n", args[ 1 ] );
			return;
		}

		char name[ MAX_OSPATH ];

		Q_strncpy( name, args[ 1 ], sizeof( name ) );

		// add .dem if not already set by user
		Q_DefaultExtension( name, ".dem", sizeof( name ) );

		bool bConflict = false;
		for ( CHltvServerIterator other; other; other.Next() )
		{
			CHLTVServer *pOtherHltvServer = other;
			if ( pOtherHltvServer != hltv && pOtherHltvServer->IsRecording() && !V_stricmp( pOtherHltvServer->GetRecordingDemoFilename(), name ) )
			{
				Warning( "Cannot record on GOTV[%d]: another GOTV[%d] is currently recording into that file\n", nHltvInstance, pOtherHltvServer->GetInstanceIndex() );
				bConflict = true;
			}
		}

		if ( !bConflict )
		{
			hltv->m_DemoRecorder.StartRecording( name, false );
		}
	}
	else
	{
		ConMsg( "GOTV[%d] is not active\n", nHltvInstance );
	}
}


// tv_broadcast change callback
void OnTvBroadcast( )
{
	for ( CActiveHltvServerIterator hltv; hltv; hltv.Next() )
	{
		if ( GetIndexedConVar( tv_broadcast, hltv.GetIndex() ).GetBool() )
		{
			if ( hltv->IsTVRelay() )
			{
				Warning( "GOTV[%d] is a relay.", hltv.GetIndex() );
			}
			else
			{
				if ( !hltv->m_Broadcast.IsRecording() )
				{
					hltv->StartBroadcast();
					ConMsg( "Broadcast on GOTV[%d] started\n", hltv.GetIndex() );
				}
				else
				{
					ConMsg( "Broadcast on GOTV[%d] is already active\n", hltv.GetIndex() );
				}
			}
		}
		else
		{
			if ( hltv->m_Broadcast.IsRecording() )
			{
				hltv->m_Broadcast.StopRecording();
				ConMsg( "Broadcast on GOTV[%d] stopped\n", hltv.GetIndex() );
			}
			else
			{
				ConMsg( "Broadcast on GOTV[%d] is not active\n", hltv.GetIndex() );
			}
		}
	}
}
void OnTvBroadcast( IConVar *var, const char *pOldValue, float flOldValue ) { OnTvBroadcast(); }


CON_COMMAND( tv_broadcast_status, "Print out broadcast status" )
{
	int nActiveServers = 0, nBroadcastingServers = 0;
	for ( CActiveHltvServerIterator hltv; hltv; hltv.Next() )
	{
		nActiveServers++;
		if ( hltv->m_Broadcast.IsRecording() )
		{
			nBroadcastingServers++;
			Msg( "GOTV[%d] is broadcasting to %s: ", hltv.GetIndex(), hltv->m_Broadcast.GetUrl() );
			hltv->m_Broadcast.DumpStats();
		}
	}
	if ( !nBroadcastingServers )
	{
		// print something
		if ( nActiveServers )
			Msg( "GOTV is not broadcasting\n" );
		else
			Msg( "GOTV is not active\n" );
	}
}

// tv_stopbroadcast is effectively accomplished by tv_broadcast 0

CON_COMMAND( tv_stoprecord, "Stops GOTV demo recording [-instance <inst> ]" )
{
	for ( CActiveHltvServerSelector hltv( args ); hltv; hltv.Next() )
	{
		//this is painful, but we need to expand all of the delta frames before we stop recording. That means this console command will cause a big spike in memory
		hltv->StopRecording();
	}
}



CON_COMMAND( tv_clients, "Shows list of connected GOTV clients [-instance <inst> ]" )
{
	for ( CActiveHltvServerSelector hltv( args ); hltv; hltv.Next() )
	{
		int nCount = 0;
		ConMsg( "GOTV[%u]\n", hltv.GetIndex() );

		for ( int i = 0; i < hltv->GetClientCount(); i++ )
		{
			CHLTVClient *client = hltv->Client( i );
			INetChannel *netchan = client->GetNetChannel();

			if ( !netchan )
				continue;

			bool bClientIsHLTV = client->IsHLTV();
			char const *szClientHLTVRedirect = bClientIsHLTV ? client->GetUserSetting( "hltv_addr" ) : NULL;

			ConMsg( "ID: %i, \"%s\"%s, Time %s, %s%s%s, In %.1f, Out %.1f.\n",
				client->GetUserID(),
				client->GetClientName(),
				bClientIsHLTV ? " (Relay)" : "",
				COM_FormatSeconds( netchan->GetTimeConnected() ),
				netchan->GetAddress(),
				bClientIsHLTV ? ( szClientHLTVRedirect ? " redirecting to " : " BAD REDIRECT ADDR" ) : "",
				( bClientIsHLTV && szClientHLTVRedirect ) ? szClientHLTVRedirect : "",
				netchan->GetAvgData( FLOW_INCOMING ) / 1024,
				netchan->GetAvgData( FLOW_OUTGOING ) / 1024 );

			nCount++;
		}

		ConMsg( "--- Total %i connected clients ---\n", nCount );
	}
}




CON_COMMAND( tv_msg, "Send a screen message to all clients [-instance <inst> ]" )
{
	for ( CActiveHltvServerSelector hltv( args ); hltv; hltv.Next() )
	{
		IGameEvent *msg = g_GameEventManager.CreateEvent( "hltv_message", true );

		if ( msg )
		{
			msg->SetString( "text", args.ArgS() );
			hltv->BroadcastEventLocal( msg, false );
			g_GameEventManager.FreeEvent( msg );
		}
	}
}


CActiveHltvServerSelector::CActiveHltvServerSelector( const CCommand &args )
{
	const char *pInstance = args.FindArg( "-instance" );
	m_nIndex = HLTV_SERVER_MAX_COUNT; // by default, iterator is empty/invalid
	m_nMask = 0;

	if ( ( pInstance && !V_stricmp( pInstance, "all" ) ) || args.FindArg( "-all" ) )
	{
		// iterate all active instances
		m_nMask = ( 1 << HLTV_SERVER_MAX_COUNT ) - 1;
		m_nIndex = -1;
		Next();
		if ( m_nIndex >= HLTV_SERVER_MAX_COUNT )
		{
			ConMsg( "No active GOTV instances at this time.\n" );
		}
	}
	else
	{
		int nInstance = pInstance ? V_atoi( pInstance ) : 0; // the default is GOTV[0]
		if ( nInstance >= HLTV_SERVER_MAX_COUNT )
		{
			ConMsg( "GOTV[%d] index out of range.\n", nInstance );
		}
		else if ( g_pHltvServer[ nInstance ] && g_pHltvServer[ nInstance ]->IsActive() )
		{
			m_nMask = 1 << nInstance;
			m_nIndex = nInstance; // valid GOTV instance found
		}
		else
		{
			ConMsg( "GOTV[%d] not active.\n", nInstance );
		}
	}
}






CON_COMMAND( tv_mem, "hltv memory statistics" )
{
	bool bActive = false;
	for ( CActiveHltvServerIterator hltv; hltv; hltv.Next() )
	{
		hltv->DumpMem();
		bActive = true;
	}
	if ( !bActive )
		Msg( "No active GOTV servers found\n" );
}

void CHLTVServer::DumpMem()
{
	Msg( "GOTV[%d] memory consumption:", m_nInstanceIndex );
	uint nHltvFrameSize = 0, nHltvFrameCount = 0;
	for ( CHLTVFrame *pFrame = m_CurrentFrame; pFrame; pFrame = static_cast< CHLTVFrame * > ( pFrame->m_pNext ) )
	{
		nHltvFrameSize += pFrame->GetMemSize();
		nHltvFrameCount++;
	}
	Msg( "%4u  Hltv Frames: %10s\n", nHltvFrameCount, V_pretifynum( nHltvFrameSize ) );

	uint nDeltaFrameCount = 0, nDeltaFrameSize = 0;
	for ( SHLTVDeltaFrame_t *pFrame = m_pOldestDeltaFrame; pFrame; pFrame = pFrame->m_pNewerDeltaFrame )
	{
		nDeltaFrameSize += pFrame->GetMemSize();
		nDeltaFrameCount++;
	}
	Msg( "%4u Delta Frames: %10s\n", nDeltaFrameCount, V_pretifynum( nDeltaFrameSize ) );
	// dump packed entity sizes from framesnapshotmanager?
}

#ifndef DEDICATED
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void EditDemo_f( const CCommand &args )
{
	if ( args.ArgC() < 2 )
	{
		Msg ("editdemo <demoname> [-instance <inst>]: edits a demo\n");
		return;
	}

	CActiveHltvServerSelector hltv( args );
	if ( hltv )
	{
		// set current demo player to client demo player
		demoplayer = hltv;

		//
		// open the demo file
		//
		char name[ MAX_OSPATH ];

		Q_strncpy( name, args[ 1 ], sizeof( name ) );

		Q_DefaultExtension( name, ".dem", sizeof( name ) );

		hltv->m_ClientState.m_bSaveMemory = true;

		hltv->StartPlayback( name, false, NULL, -1 );
	}
}

CON_COMMAND_AUTOCOMPLETEFILE( editdemo, EditDemo_f, "Edit a recorded demo file (.dem ).", NULL, dem );
#endif
