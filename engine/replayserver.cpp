//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// replayserver.cpp: implementation of the CReplayServer class.
//
//////////////////////////////////////////////////////////////////////
#if defined( REPLAY_ENABLED )
#include <server_class.h>
#include <inetmessage.h>
#include <tier0/vprof.h>
#include <keyvalues.h>
#include <edict.h>
#include <eiface.h>
#include <PlayerState.h>
#include <ireplaydirector.h>
#include <time.h>

#include "replayserver.h"
#include "sv_client.h"
#include "replayclient.h"
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
#include "con_nprint.h"
#include "tier0/icommandline.h"
#include "client_class.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern CNetworkStringTableContainer *networkStringTableContainerClient;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CReplayServer *replay = NULL;

static void replay_title_changed_f( IConVar *var, const char *pOldString, float flOldValue )
{
	if ( replay && replay->IsActive() )
	{
		replay->BroadcastLocalTitle();
	}
}

static void replay_name_changed_f( IConVar *var, const char *pOldValue, float flOldValue )
{
	Steam3Server().NotifyOfServerNameChange();
}

static ConVar replay_maxclients( "replay_maxclients", "128", 0, "Maximum client number on Replay server.",
							  true, 0, true, 255 );

ConVar replay_autorecord( "replay_autorecord", "1", 0, "Automatically records all games as Replay demos." );
ConVar replay_name( "replay_name", "Replay", 0, "Replay host name", replay_name_changed_f );
static ConVar replay_password( "replay_password", "", FCVAR_NOTIFY | FCVAR_PROTECTED | FCVAR_DONTRECORD, "Replay password for all clients" );

static ConVar replay_overridemaster( "replay_overridemaster", "0", 0, "Overrides the Replay master root address." );
static ConVar replay_dispatchmode( "replay_dispatchmode", "1", 0, "Dispatch clients to relay proxies: 0=never, 1=if appropriate, 2=always" );
ConVar replay_transmitall( "replay_transmitall", "1", FCVAR_REPLICATED, "Transmit all entities (not only director view)" );
ConVar replay_debug( "replay_debug", "0", 0, "Replay debug info." );
ConVar replay_title( "replay_title", "Replay", 0, "Set title for Replay spectator UI", replay_title_changed_f );
static ConVar replay_deltacache( "replay_deltacache", "2", 0, "Enable delta entity bit stream cache" );
static ConVar replay_relayvoice( "replay_relayvoice", "1", 0, "Relay voice data: 0=off, 1=on" );
ConVar replay_movielength( "replay_movielength", "60", FCVAR_REPLICATED | FCVAR_DONTRECORD, "Replay movie length in seconds" );
ConVar replay_demolifespan( "replay_demolifespan", "5", FCVAR_REPLICATED | FCVAR_DONTRECORD, "The number of days allowed to pass before cleaning up replay demos.", true, 1, true, 30 );
ConVar replay_cleanup_time( "replay_cleanup_time", "1", FCVAR_REPLICATED | FCVAR_DONTRECORD, "The Replay system will periodically remove stale (never downloaded) .dem files from disk.  This variable represents the "
						   "amount of time (in hours) between each cleanup.", true, 1, true, 24 );

CReplayDeltaEntityCache::CReplayDeltaEntityCache()
{
	Q_memset( m_Cache, 0, sizeof(m_Cache) );
	m_nTick = 0;
	m_nMaxEntities = 0;
	m_nCacheSize = 0;
}

CReplayDeltaEntityCache::~CReplayDeltaEntityCache()
{
	Flush();
}

void CReplayDeltaEntityCache::Flush()
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

void CReplayDeltaEntityCache::SetTick( int nTick, int nMaxEntities )
{
	if ( nTick == m_nTick )
		return;

	Flush();

	m_nCacheSize = replay_deltacache.GetInt() * 1024;

	if ( m_nCacheSize <= 0 )
		return;

	m_nMaxEntities = MIN(nMaxEntities,MAX_EDICTS);
	m_nTick = nTick;
}

unsigned char* CReplayDeltaEntityCache::FindDeltaBits( int nEntityIndex, int nDeltaTick, int &nBits )
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

void CReplayDeltaEntityCache::AddDeltaBits( int nEntityIndex, int nDeltaTick, int nBits, bf_write *pBuffer )
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

void CReplayServer::FreeClientRecvTables()
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
void CReplayServer::InitClientRecvTables()
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
			Msg("REPLAY_InitRecvTableMgr: failed to allocate client class %s.\n", pCur->m_pNetworkName );
			return;
		}
	}

	RecvTable_Init( m_pRecvTables, m_nRecvTables );
}



CReplayFrame::CReplayFrame()
{
	
}

CReplayFrame::~CReplayFrame()
{
	FreeBuffers();
}

void CReplayFrame::Reset( void )
{
	for ( int i=0; i<REPLAY_BUFFER_MAX; i++ )
	{
		m_Messages[i].Reset();
	}
}

bool CReplayFrame::HasData( void )
{
	for ( int i=0; i<REPLAY_BUFFER_MAX; i++ )
	{
		if ( m_Messages[i].GetNumBitsWritten() > 0 )
			return true;
	}

	return false;
}

void CReplayFrame::CopyReplayData( CReplayFrame &frame )
{
	// copy reliable messages
	int bits = frame.m_Messages[REPLAY_BUFFER_RELIABLE].GetNumBitsWritten();

	if ( bits > 0 )
	{
		int bytes = PAD_NUMBER( Bits2Bytes(bits), 4 );
		m_Messages[REPLAY_BUFFER_RELIABLE].StartWriting( new char[ bytes ], bytes, bits );
		Q_memcpy( m_Messages[REPLAY_BUFFER_RELIABLE].GetBasePointer(), frame.m_Messages[REPLAY_BUFFER_RELIABLE].GetBasePointer(), bytes );
	}

	// copy unreliable messages
	bits = frame.m_Messages[REPLAY_BUFFER_UNRELIABLE].GetNumBitsWritten();
	bits += frame.m_Messages[REPLAY_BUFFER_TEMPENTS].GetNumBitsWritten();
	bits += frame.m_Messages[REPLAY_BUFFER_SOUNDS].GetNumBitsWritten();

	if ( replay_relayvoice.GetBool() )
		bits += frame.m_Messages[REPLAY_BUFFER_VOICE].GetNumBitsWritten();
	
	if ( bits > 0 )
	{
		// collapse all unreliable buffers in one
		int bytes = PAD_NUMBER( Bits2Bytes(bits), 4 );
		m_Messages[REPLAY_BUFFER_UNRELIABLE].StartWriting( new char[ bytes ], bytes );
		m_Messages[REPLAY_BUFFER_UNRELIABLE].WriteBits( frame.m_Messages[REPLAY_BUFFER_UNRELIABLE].GetData(), frame.m_Messages[REPLAY_BUFFER_UNRELIABLE].GetNumBitsWritten() ); 
		m_Messages[REPLAY_BUFFER_UNRELIABLE].WriteBits( frame.m_Messages[REPLAY_BUFFER_TEMPENTS].GetData(), frame.m_Messages[REPLAY_BUFFER_TEMPENTS].GetNumBitsWritten() ); 
		m_Messages[REPLAY_BUFFER_UNRELIABLE].WriteBits( frame.m_Messages[REPLAY_BUFFER_SOUNDS].GetData(), frame.m_Messages[REPLAY_BUFFER_SOUNDS].GetNumBitsWritten() ); 

		if ( replay_relayvoice.GetBool() )
			m_Messages[REPLAY_BUFFER_UNRELIABLE].WriteBits( frame.m_Messages[REPLAY_BUFFER_VOICE].GetData(), frame.m_Messages[REPLAY_BUFFER_VOICE].GetNumBitsWritten() ); 
	}
}

void CReplayFrame::AllocBuffers( void )
{
	// allocate buffers for input frame
	for ( int i=0; i < REPLAY_BUFFER_MAX; i++ )
	{
		Assert( m_Messages[i].GetBasePointer() == NULL );
		m_Messages[i].StartWriting( new char[NET_MAX_PAYLOAD], NET_MAX_PAYLOAD);
	}
}

void CReplayFrame::FreeBuffers( void )
{
	for ( int i=0; i<REPLAY_BUFFER_MAX; i++ )
	{
		bf_write &msg = m_Messages[i];

		if ( msg.GetBasePointer() )
		{
			delete[] msg.GetBasePointer();
			msg.StartWriting( NULL, 0 );
		}
	}
}

CReplayServer::CReplayServer()
:	m_DemoRecorder( this )	
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
	m_nGlobalSlots = 0;
	m_nGlobalClients = 0;
	m_nGlobalProxies = 0;
	m_vecClientsDownloading.ClearAll();
}

CReplayServer::~CReplayServer()
{
	if ( m_nRecvTables > 0 )
	{
		RecvTable_Term();
		FreeClientRecvTables();
	}

	// make sure everything was destroyed
	Assert( m_CurrentFrame == NULL );
	Assert( CountClientFrames() == 0 );
}

void CReplayServer::SetMaxClients( int number )
{
	// allow max clients 0 in Replay
	m_nMaxclients = clamp( number, 0, ABSOLUTE_PLAYER_LIMIT );
}

void CReplayServer::StartMaster(CGameClient *client)
{
	Clear();  // clear old settings & buffers

	if ( !client )
	{
		ConMsg("Replay client not found.\n");
		return;
	}

	m_Director = serverReplayDirector;	

	if ( !m_Director )
	{
		ConMsg("Mod doesn't support Replay. No director module found.\n");
		return;
	}

	m_MasterClient = client;
	m_MasterClient->m_bIsHLTV = false;
	m_MasterClient->m_bIsReplay = true;

	// let game.dll know that we are the Replay client
	Assert( serverGameClients );

	CPlayerState *player = serverGameClients->GetPlayerState( m_MasterClient->edict );
	player->replay = true;

	m_Server = (CGameServer*)m_MasterClient->GetServer();

	// set default user settings
	m_MasterClient->m_ConVars->SetString( "name", replay_name.GetString() );
	m_MasterClient->m_ConVars->SetString( "cl_team", "1" );
	m_MasterClient->m_ConVars->SetString( "rate", va( "%d", DEFAULT_RATE ) );
	m_MasterClient->m_ConVars->SetString( "cl_updaterate", "32" );
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
	m_ReplayFrame.AllocBuffers();
			
	InstallStringTables();

	// activate director in game.dll
	m_Director->SetReplayServer( this );

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
			DevMsg("CReplayServer::StartMaster: game event %s not found.\n", eventname );
		}

		j++;
	}
	
	// copy signon buffers
	m_Signon.StartWriting( m_Server->m_Signon.GetBasePointer(), m_Server->m_Signon.m_nDataBytes, 
		m_Server->m_Signon.GetNumBitsWritten() );

	Q_strncpy( m_szMapname, m_Server->m_szMapname, sizeof(m_szMapname) );
	Q_strncpy( m_szSkyname, m_Server->m_szSkyname, sizeof(m_szSkyname) );

	NET_ListenSocket( m_Socket, true );	// activated Replay TCP socket

	m_MasterClient->ExecuteStringCommand( "spectate" ); // become a spectator

	m_MasterClient->UpdateUserSettings(); // make sure UserInfo is correct

	// hack reduce signontick by one to catch changes made in the current tick
	m_MasterClient->m_nSignonTick--;	

	if ( m_bMasterOnlyMode )
	{
		// we allow only one client in master only mode
		replay_maxclients.SetValue( MIN(1,replay_maxclients.GetInt()) );
	}

	SetMaxClients( replay_maxclients.GetInt() );

	m_bSignonState = false; //master proxy is instantly connected

	m_nSpawnCount++;

	m_flStartTime = net_time;

	m_State = ss_active;

	// stop any previous recordings
	m_DemoRecorder.StopRecording();

	// start new recording if autorecord is enabled
	if ( replay_autorecord.GetBool() )
	{
		m_DemoRecorder.StartAutoRecording();
	}

	ReconnectClients();
}

void CReplayServer::StartDemo(const char *filename)
{

}

bool CReplayServer::DispatchToRelay( CReplayClient *pClient )
{
	if ( replay_dispatchmode.GetInt() <= DISPATCH_MODE_OFF )
		return false; // don't redirect
	
	CBaseClient	*pBestProxy = NULL;
	float fBestRatio = 1.0f;

	// find best relay proxy
	for (int i=0; i < GetClientCount(); i++ )
	{
		CBaseClient *pProxy = m_Clients[ i ];

		// check all known proxies
		if ( !pProxy->IsConnected() || !pProxy->IsReplay() || (pClient == pProxy) )
			continue;

		int slots = Q_atoi( pProxy->GetUserSetting( "replay_slots" ) );
		int clients = Q_atoi( pProxy->GetUserSetting( "replay_clients" ) );

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
		if ( replay_dispatchmode.GetInt() == DISPATCH_MODE_ALWAYS )
		{
			// we are in always forward mode, drop client if we can't forward it
			pClient->Disconnect("No Replay relay available");
			return true;
		}
		else
		{
			// just let client connect to this proxy
			return false;
		}
	}

	// check if client should stay on this relay server
	if ( (replay_dispatchmode.GetInt() == DISPATCH_MODE_AUTO) && (GetMaxClients() > 0) )
	{
		// ratio = clients/slots. give relay proxies 25% bonus
		float myRatio = ((float)GetNumClients()/(float)GetMaxClients()) * 1.25f;

		myRatio = MIN( myRatio, 1.0f ); // clamp to 1

		// if we have a better local ratio then other proxies, keep this client here
		if ( myRatio < fBestRatio )
			return false;	// don't redirect
	}
	
	const char *pszRelayAddr = pBestProxy->GetUserSetting("replay_addr");

	if ( !pszRelayAddr )
		return false;
	

	ConMsg( "Redirecting spectator %s to Replay relay %s\n", 
		pClient->GetNetChannel()->GetAddress(), 
		pszRelayAddr );

	// first tell that client that we are a Replay server,
	// otherwise it's might ignore the "connect" command
	CSVCMsg_ServerInfo_t serverInfo;	
	FillServerInfo( serverInfo ); 
	pClient->SendNetMsg( serverInfo, true );
	
	// tell the client to connect to this new address
	CNETMsg_StringCmd_t cmdMsg( va("connect %s\n", pszRelayAddr ) ) ;
	pClient->SendNetMsg( cmdMsg, true );
    		
 	// increase this proxies client number in advance so this proxy isn't used again next time
	int clients = Q_atoi( pBestProxy->GetUserSetting( "replay_clients" ) );
	pBestProxy->SetUserCVar( "replay_clients", va("%d", clients+1 ) );
	
	return true;
}

int	CReplayServer::GetReplaySlot( void )
{
	return m_nPlayerSlot;
}

float CReplayServer::GetOnlineTime( void )
{
	return MAX(0, net_time - m_flStartTime);
}
void CReplayServer::BroadcastLocalTitle( CReplayClient *client )
{
	IGameEvent *event = g_GameEventManager.CreateEvent( "replay_title", true );

	if ( !event )
		return;

	event->SetString( "text", replay_title.GetString() );

	CSVCMsg_GameEvent_t eventMsg;

	eventMsg.SetReliable( true );

	// create bit stream from KeyValues
	if ( !g_GameEventManager.SerializeEvent( event, &eventMsg ) )
	{
		DevMsg("CReplayServer: failed to serialize title '%s'.\n", event->GetName() );
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

			if ( !client->IsActive() || client->IsReplay() )
				continue;

			client->SendNetMsg( eventMsg );
		}
	}

	g_GameEventManager.FreeEvent( event );
}

void CReplayServer::BroadcastLocalChat( const char *pszChat, const char *pszGroup )
{
	IGameEvent *event = g_GameEventManager.CreateEvent( "replay_chat", true );

	if ( !event )
		return;
	
	event->SetString( "text", pszChat );

	CSVCMsg_GameEvent_t eventMsg;

	eventMsg.SetReliable( false );

	// create bit stream from KeyValues
	if ( !g_GameEventManager.SerializeEvent( event, &eventMsg ) )
	{
		DevMsg("CReplayServer: failed to serialize chat '%s'.\n", event->GetName() );
		g_GameEventManager.FreeEvent( event );
		return;
	}

	for ( int i = 0; i < m_Clients.Count(); i++ )
	{
		CReplayClient *cl = Client(i);

		if ( !cl->IsActive() || !cl->IsSpawned() || cl->IsReplay() )
			continue;

		// if this is a spectator chat message and client disabled it, don't show it
		if ( Q_strcmp( cl->m_szChatGroup, pszGroup) || cl->m_bNoChat )
			continue;

		cl->SendNetMsg( eventMsg );
	}

	g_GameEventManager.FreeEvent( event );
}

void CReplayServer::BroadcastEventLocal( IGameEvent *event, bool bReliable )
{
	CSVCMsg_GameEvent_t eventMsg;

	eventMsg.SetReliable( bReliable );

	// create bit stream from KeyValues
	if ( !g_GameEventManager.SerializeEvent( event, &eventMsg ) )
	{
		DevMsg("CReplayServer: failed to serialize local event '%s'.\n", event->GetName() );
		return;
	}

	for ( int i = 0; i < m_Clients.Count(); i++ )
	{
		CReplayClient *cl = Client(i);

		if ( !cl->IsActive() || !cl->IsSpawned() || cl->IsReplay() )
			continue;

		if ( !cl->SendNetMsg( eventMsg ) )
		{
			if ( eventMsg.IsReliable() )
			{
				DevMsg( "BroadcastMessage: Reliable broadcast message overflow for client %s", cl->GetClientName() );
			}
		}
	}

	if ( replay_debug.GetBool() )
		Msg("Replay broadcast local event: %s\n", event->GetName() );
}

void CReplayServer::BroadcastEvent(IGameEvent *event)
{
	CSVCMsg_GameEvent_t eventMsg;

	// create bit stream from KeyValues
	if ( !g_GameEventManager.SerializeEvent( event, &eventMsg ) )
	{
		DevMsg("CReplayServer: failed to serialize event '%s'.\n", event->GetName() );
		return;
	}

	BroadcastMessage( eventMsg, true, true );

	if ( replay_debug.GetBool() )
		Msg("Replay broadcast event: %s\n", event->GetName() );
}

void CReplayServer::FireGameEvent(IGameEvent *event)
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
		DevMsg("CReplayServer::FireGameEvent: failed to serialize event '%s'.\n", event->GetName() );
	}
}

int CReplayServer::GetEventDebugID()
{
	return m_nDebugID;
}

bool CReplayServer::ShouldUpdateMasterServer()
{
	if ( IsUsingMasterLegacyMode() )
	{
		// In the legacy master server updater, we have to call the updater with our CBaseServer* for it to work right.
		return true;
	}
	else
	{
		// If the main game server is active, then we let it update Steam with the server info.
		return !sv.IsActive();
	}
}

CBaseClient *CReplayServer::CreateNewClient(int slot )
{
	return new CReplayClient( slot, this );
}

void CReplayServer::InstallStringTables( void )
{
#ifndef SHARED_NET_STRING_TABLES

	int numTables = m_Server->m_StringTables->GetNumTables();

	m_StringTables = &m_NetworkStringTables;

	Assert( m_StringTables->GetNumTables() == 0); // must be empty

	m_StringTables->AllowCreation( true );
	
	// master replay needs to keep a list of changes for all table items
	m_StringTables->EnableRollback( true );

	for ( int i =0; i<numTables; i++)
	{
		// iterate through server tables
		CNetworkStringTable *serverTable = 
			(CNetworkStringTable*)m_Server->m_StringTables->GetTable( i );

		if ( !serverTable )
			continue;

		// get matching client table
		CNetworkStringTable *replayTable = 
			(CNetworkStringTable*)m_StringTables->CreateStringTable(
				serverTable->GetTableName(),
				serverTable->GetMaxStrings(),
				serverTable->GetUserDataSize(),
				serverTable->GetUserDataSizeBits(),
				serverTable->IsUsingDictionary() ? NSF_DICTIONARY_ENABLED : NSF_NONE
				);

		if ( !replayTable )
		{
			DevMsg("SV_InstallReplayStringTableMirrors! Missing client table \"%s\".\n ", serverTable->GetTableName() );
			continue;
		}

		// make replay table an exact copy of server table
		replayTable->CopyStringTable( serverTable ); 

		// link replay table to server table
		serverTable->SetMirrorTable( replayTable );
	}

	m_StringTables->AllowCreation( false );

#endif
}

void CReplayServer::RestoreTick( int tick )
{
#ifndef SHARED_NET_STRING_TABLES

	int numTables = m_StringTables->GetNumTables();

	for ( int i =0; i<numTables; i++)
	{
			// iterate through server tables
		CNetworkStringTable *pTable = (CNetworkStringTable*) m_StringTables->GetTable( i );
		pTable->RestoreTick( tick );
	}

#endif
}

void CReplayServer::UserInfoChanged( int nClientIndex )
{
	// don't change UserInfo table, it keeps the infos of the original players
}

void CReplayServer::LinkInstanceBaselines( void )
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

/* CReplayServer::GetOriginFromPackedEntity is such a bad, bad hack.

extern float DecodeFloat(SendProp const *pProp, bf_read *pIn);

Vector CReplayServer::GetOriginFromPackedEntity(PackedEntity* pe)
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

CReplayEntityData *FindReplayDataInSnapshot( CFrameSnapshot * pSnapshot, int iEntIndex )
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
			return &pSnapshot->m_pReplayEntityData[m];
		
		if ( iEntIndex > index )
		{
			if ( pSnapshot->m_pValidEntities[z] == iEntIndex )
				return &pSnapshot->m_pReplayEntityData[z];

			if ( a == m )
				return NULL;

			a = m;
		}
		else
		{
			if ( pSnapshot->m_pValidEntities[a] == iEntIndex )
				return &pSnapshot->m_pReplayEntityData[a];

			if ( z == m )
				return NULL;

			z = m;
		}
	}

	return NULL;
}

void CReplayServer::EntityPVSCheck( CClientFrame *pFrame )
{
	byte PVS[PAD_NUMBER( MAX_MAP_CLUSTERS,8 ) / 8];
	int nPVSSize = (GetCollisionBSPData()->numclusters + 7) / 8;

	// setup engine PVS
	SV_ResetPVS( PVS, nPVSSize );

	CFrameSnapshot * pSnapshot = pFrame->GetSnapshot();	

	Assert ( pSnapshot->m_pReplayEntityData != NULL );

	int nDirectorEntity = m_Director->GetPVSEntity();
    	
	if ( pSnapshot && nDirectorEntity > 0 )
	{
		CReplayEntityData *pReplayData = FindReplayDataInSnapshot( pSnapshot, nDirectorEntity );

		if ( pReplayData )
		{
			m_vPVSOrigin.x = pReplayData->origin[0];
			m_vPVSOrigin.y = pReplayData->origin[1];
			m_vPVSOrigin.z = pReplayData->origin[2];
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

		CReplayEntityData *pReplayData = FindReplayDataInSnapshot( pSnapshot, entindex );

		if ( !pReplayData )
			continue;

		unsigned int nNodeCluster = pReplayData->m_nNodeCluster;

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

CClientFrame *CReplayServer::AddNewFrame( CClientFrame *clientFrame )
{
	VPROF_BUDGET( "CReplayServer::AddNewFrame", "Replay" );

	Assert ( clientFrame );
	Assert( clientFrame->tick_count > m_nLastTick );

	m_nLastTick = clientFrame->tick_count;

	m_ReplayFrame.SetSnapshot( clientFrame->GetSnapshot() );
	m_ReplayFrame.tick_count = clientFrame->tick_count;	
	m_ReplayFrame.last_entity = clientFrame->last_entity;
	m_ReplayFrame.transmit_entity = clientFrame->transmit_entity;

	// remember tick of first valid frame
	if ( m_nFirstTick < 0 )
	{
		m_nFirstTick = clientFrame->tick_count;
		m_nTickCount = m_nFirstTick;
	}
	
	CReplayFrame *replayFrame = new CReplayFrame;

	// copy tickcount & entities from client frame
	replayFrame->CopyFrame( *clientFrame );

	//copy rest (messages, tempents) from current Replay frame
	replayFrame->CopyReplayData( m_ReplayFrame );
	
	// add frame to Replay server
	AddClientFrame( replayFrame );

	if ( m_DemoRecorder.IsRecording() )
	{
		m_DemoRecorder.WriteFrame( &m_ReplayFrame );
	}

	// reset Replay frame for recording next messages etc.
	m_ReplayFrame.Reset();
	m_ReplayFrame.SetSnapshot( NULL );

	return replayFrame;
}

void CReplayServer::SendClientMessages ( bool bSendSnapshots )
{
	// build individual updates
	for ( int i=0; i< m_Clients.Count(); i++ )
	{
		CReplayClient* client = Client(i);
		
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

bool CReplayServer::SendNetMsg( INetMessage &msg, bool bForceReliable )
{
	if ( m_bSignonState	)
	{
		return msg.WriteToBuffer( m_Signon );
	}

	int buffer = REPLAY_BUFFER_UNRELIABLE;	// default destination

	if ( msg.IsReliable() )
	{
		buffer = REPLAY_BUFFER_RELIABLE;
	}
	else if ( msg.GetType() == svc_Sounds )
	{
		buffer = REPLAY_BUFFER_SOUNDS;
	}
	else if ( msg.GetType() == svc_VoiceData )
	{
		buffer = REPLAY_BUFFER_VOICE;
	}
	else if ( msg.GetType() == svc_TempEntities )
	{
		buffer = REPLAY_BUFFER_TEMPENTS;
	}

	// anything else goes to the unreliable bin
	return msg.WriteToBuffer( m_ReplayFrame.m_Messages[buffer] );
}

bf_write *CReplayServer::GetBuffer( int nBuffer )
{
	if ( nBuffer < 0 || nBuffer >= REPLAY_BUFFER_MAX )
		return NULL;

	return &m_ReplayFrame.m_Messages[nBuffer];
}

IServer *CReplayServer::GetBaseServer()
{
	return (IServer*)this;
}

IReplayDirector *CReplayServer::GetDirector()
{
	return m_Director;
}

CClientFrame *CReplayServer::GetDeltaFrame( int nTick )
{
	if ( !replay_deltacache.GetBool() )
		return GetClientFrame( nTick ); //expensive

	// TODO make that a utlmap
	FOR_EACH_VEC( m_FrameCache, iFrame )
	{
		if ( m_FrameCache[iFrame].nTick == nTick )
			return m_FrameCache[iFrame].pFrame;
	}

	int i = m_FrameCache.AddToTail();

	CReplayFrameCacheEntry_s &entry = m_FrameCache[i];

	entry.nTick = nTick;
	entry.pFrame = GetClientFrame( nTick ); //expensive

	return entry.pFrame;
}

void CReplayServer::RunFrame()
{
	VPROF_BUDGET( "CReplayServer::RunFrame", "Replay" );

	// update network time etc
	NET_RunFrame( Plat_FloatTime() );

	// check if Replay server if active
	if ( !IsActive() )
		return;

	if ( host_frametime > 0 )
	{
		m_flFPS = m_flFPS * 0.99f + 0.01f/host_frametime;
	}

	// get current tick time for director module and restore 
	// world (stringtables, framebuffers) as they were at this time
	UpdateTick();

	// Run any commands from client and play client Think functions if it is time.
	CBaseServer::RunFrame();

	SendClientMessages( true );

	// Update the Steam server if we're running a relay.
	if ( !sv.IsActive() )
		Steam3Server().RunFrame();
	
	UpdateMasterServer();

//	::Con_NPrintf( 9 , "server time: %d", host_tickcount );
//	::Con_NPrintf( 10, "replay time: %d", m_DemoRecorder.GetRecordingTick() );
}

void CReplayServer::UpdateTick( void )
{
	VPROF_BUDGET( "CReplayServer::UpdateTick", "Replay" );

	if ( m_nFirstTick < 0 )
	{
		m_nTickCount = 0;
		m_CurrentFrame = NULL;
		return;
	}

	// HACK: I'm not so sure this is the right place for this, but essentially the start tick needs to 
	// represent the "oldest" tick in the cache of frames for a replay demo - for a regular demo, it need
	// not be modified here.  NOTE: If this is a regular demo, this call does nothing.
	m_DemoRecorder.m_DemoFile.UpdateStartTick( m_DemoRecorder.m_nStartTick );

	// set tick time to last frame added
	int nNewTick = m_nLastTick;

	// get tick from director, he decides delay etc
	nNewTick = MAX( m_nFirstTick, m_Director->GetDirectorTick() );

	// the the closest available frame
	CReplayFrame *newFrame = (CReplayFrame*) GetClientFrame( nNewTick, false );

	if ( newFrame == NULL )
		return; // we dont have a new frame
	
	if ( m_CurrentFrame == newFrame )
		return;	// current frame didn't change

	m_CurrentFrame = newFrame;
	m_nTickCount = m_CurrentFrame->tick_count;
	
	// restore string tables for this time
	RestoreTick( m_nTickCount );

	// remove entities out of current PVS
	if ( !replay_transmitall.GetBool() )
	{
		EntityPVSCheck( m_CurrentFrame );	
	}

	int removeTick = m_nTickCount - 16.0f/m_flTickInterval; // keep 16 second buffer

	if ( removeTick > 0 )
	{
		DeleteClientFrames( removeTick );
	}

	m_FrameCache.RemoveAll();
}

const char *CReplayServer::GetName( void ) const
{
	return replay_name.GetString();
}

void CReplayServer::FillServerInfo(CSVCMsg_ServerInfo &serverinfo)
{
	CBaseServer::FillServerInfo( serverinfo );

	serverinfo.set_player_slot( m_nPlayerSlot ); // all spectators think they're the Replay client
	serverinfo.set_max_clients( m_nGameServerMaxClients );
}

void CReplayServer::Clear( void )
{
	CBaseServer::Clear();

	m_Director = NULL;
	m_MasterClient = NULL;
	m_Server = NULL;
	m_nFirstTick = -1;
	m_nLastTick = 0;
	m_nTickCount = 0;
	m_CurrentFrame = NULL;
	m_nPlayerSlot = 0;
	m_flStartTime = 0.0f;
	m_nViewEntity = 1;
	m_nGameServerMaxClients = 0;
	m_fNextSendUpdateTime = 0.0f;
	m_ReplayFrame.FreeBuffers();
	m_vPVSOrigin.Init();
	m_vecClientsDownloading.ClearAll();
		
	DeleteClientFrames( -1 );

	m_DeltaCache.Flush();
	m_FrameCache.RemoveAll();
}

void CReplayServer::Init(bool bIsDedicated)
{
	CBaseServer::Init( bIsDedicated );

	m_Socket = NS_REPLAY;
	
	// check if only master proxy is allowed, no broadcasting
	if ( CommandLine()->FindParm("-tvmasteronly") )
	{
		m_bMasterOnlyMode = true;
	}
}

void CReplayServer::Changelevel()
{
	m_DemoRecorder.StopRecording();

	InactivateClients();

	DeleteClientFrames(-1);

	m_CurrentFrame = NULL;
}

void CReplayServer::GetNetStats( float &avgIn, float &avgOut )
{
	CBaseServer::GetNetStats( avgIn, avgOut	);
}

void CReplayServer::Shutdown( void )
{
	m_DemoRecorder.StopRecording(); // if recording, stop now

	if ( m_MasterClient )
		m_MasterClient->Disconnect( "Replay stop." );

	if ( m_Director )
		m_Director->SetReplayServer( NULL );

	g_GameEventManager.RemoveListener( this );

	CBaseServer::Shutdown();
}

int	CReplayServer::GetChallengeType ( netadr_t &adr )
{
	return PROTOCOL_HASHEDCDKEY; // Replay doesn't need Steam authentication
}

const char *CReplayServer::GetPassword() const
{
	const char *password = replay_password.GetString();

	// if password is empty or "none", return NULL
	if ( !password[0] || !Q_stricmp(password, "none" ) )
	{
		return NULL;
	}

	return password;
}

IClient *CReplayServer::ConnectClient ( const ns_address &adr, int protocol, int challenge, int authProtocol, 
									    const char *name, const char *password, const char *hashedCDkey, int cdKeyLen,
									    CUtlVector< CCLCMsg_SplitPlayerConnect_t * > & splitScreenClients, bool isClientLowViolence, CrossPlayPlatform_t clientPlatform,
										const byte *pbEncryptionKey, int nEncryptionKeyIndex )
{
	IClient	*client = (CReplayClient*)CBaseServer::ConnectClient( 
		adr, protocol, challenge,authProtocol, name, password, hashedCDkey, cdKeyLen, splitScreenClients, isClientLowViolence, clientPlatform,
		pbEncryptionKey, nEncryptionKeyIndex );

	if ( client )
	{
		// remember password
		CReplayClient *pReplayClient = (CReplayClient*)client;
		Q_strncpy( pReplayClient->m_szPassword, password, sizeof(pReplayClient->m_szPassword) );
	}

	return client;
}

CON_COMMAND( replay_status, "Show Replay server status." ) 
{
	float	in, out;
	char	gd[MAX_OSPATH];

	Q_FileBase( com_gamedir, gd, sizeof( gd ) );

	if ( !replay || !replay->IsActive() )
	{
		ConMsg("Replay not active.\n" );
		return;
	}

	replay->GetNetStats( in, out );

	in /= 1024; // as KB
	out /= 1024;

	ConMsg("--- Replay Status ---\n");
	ConMsg("Online %s, FPS %.1f, Version %i (%s)\n", 
		COM_FormatSeconds( replay->GetOnlineTime() ), replay->m_flFPS, build_number(),

#ifdef _WIN32
		"Win32" );
#else
		"Linux" );
#endif

	ConMsg("Master \"%s\", delay %.0f\n", replay->GetName(), replay->GetDirector()->GetDelay() );

	ConMsg("Game Time %s, Mod \"%s\", Map \"%s\", Players %i\n", COM_FormatSeconds( replay->GetTime() ),
		gd, replay->GetMapName(), replay->GetNumPlayers() );

	ConMsg("Local IP %s:%i, KB/sec In %.1f, Out %.1f\n",
		net_local_adr.ToString( true ), replay->GetUDPPort(), in ,out );

	if ( replay->m_DemoRecorder.IsRecording() )
	{
		ConMsg("Recording to \"%s\", length %s.\n", replay->m_DemoRecorder.GetDemoFile()->m_szFileName, 
			COM_FormatSeconds( host_state.interval_per_tick * replay->m_DemoRecorder.GetRecordingTick() ) );
	}		
}

CON_COMMAND( replay_record, "Starts Replay demo recording." )
{
#if 0
	AssertMsg( 0, "Use replay_autorecord 1!" );
#else
	if ( !replay || !replay->IsActive() )
	{
		ConMsg("Replay not active.\n" );
		return;
	}

	if ( replay->m_DemoRecorder.IsRecording() )
	{
		ConMsg("Replay already recording to %s.\n", replay->m_DemoRecorder.GetDemoFile()->m_szFileName );
		return;
	}

	replay->m_DemoRecorder.StartAutoRecording();
#endif
}

CON_COMMAND( replay_stoprecord, "Stops Replay demo recording." )
{
	if ( !replay || !replay->IsActive() )
	{
		ConMsg("Replay not active.\n" );
		return;
	}
	
	replay->m_DemoRecorder.StopRecording();
}

#endif
