//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include <netmessages.h>
#include "hltvclientstate.h"
#include "hltvserver.h"
#include "quakedef.h"
#include "cl_main.h"
#include "host.h"
#include "dt_recv_eng.h"
#include "dt_common_eng.h"
#include "framesnapshot.h"
#include "clientframe.h"
#include "ents_shared.h"
#include "server.h"
#include "eiface.h"
#include "server_class.h"
#include "cdll_engine_int.h"
#include "sv_main.h"
#include "changeframelist.h"
#include "GameEventManager.h"
#include "dt_recv_decoder.h"
#include "utllinkedlist.h"
#include "cl_demo.h"
#include "sv_steamauth.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// copy message data from in to out buffer
#define CopyDataInToOut(msg)									\
	int	 size = PAD_NUMBER( Bits2Bytes(msg->m_nLength), 4);		\
	byte *buffer = (byte*) stackalloc( size );					\
	msg->m_DataIn.ReadBits( buffer, msg->m_nLength );			\
	msg->m_DataOut.StartWriting( buffer, size, msg->m_nLength );\
	
static void HLTV_Callback_InstanceBaseline( void *object, INetworkStringTable *stringTable, int stringNumber, char const *newString, void const *newData )
{
	// relink server classes to instance baselines
	CHLTVServer *pHLTV = (CHLTVServer*)object;
	pHLTV->m_ClientState.UpdateInstanceBaseline( stringNumber );
	pHLTV->LinkInstanceBaselines();
}

extern CUtlLinkedList< CRecvDecoder *, unsigned short > g_RecvDecoders;

extern	ConVar tv_autorecord;
static	ConVar tv_autoretry( "tv_autoretry", "1", FCVAR_RELEASE, "Relay proxies retry connection after network timeout" );
static	ConVar tv_timeout( "tv_timeout", "30", FCVAR_RELEASE, "GOTV connection timeout in seconds." );
		ConVar tv_snapshotrate("tv_snapshotrate", "32", FCVAR_RELEASE | FCVAR_REPLICATED, "Snapshots broadcasted per second" ); // for the best quality of replay, use 64
		ConVar tv_snapshotrate1( "tv_snapshotrate1", "32", FCVAR_RELEASE, "Snapshots broadcasted per second, GOTV[1]" ); // set this to 128 to record 128-tick server demo

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////



CHLTVClientState::CHLTVClientState(CHLTVServer *pHltvServer) : m_pHLTV( pHltvServer )
{
	m_pNewClientFrame = NULL;
	m_pCurrentClientFrame = NULL;
	m_bSaveMemory = false;

	eventid_hltv_status = -1;
	eventid_hltv_title = -1;
}

CHLTVClientState::~CHLTVClientState()
{

}

void CHLTVClientState::CopyNewEntity( 
	CEntityReadInfo &u,
	int iClass,
	int iSerialNum
	)
{
	ServerClass *pServerClass = SV_FindServerClass( iClass );
	Assert( pServerClass );
	
	ClientClass *pClientClass = GetClientClass( iClass );
	Assert( pClientClass );

	const int ent = u.m_nNewEntity;

	// copy class & serial
	CFrameSnapshot *pSnapshot = u.m_pTo->GetSnapshot();
	pSnapshot->m_pEntities[ent].m_nSerialNumber = iSerialNum;
	pSnapshot->m_pEntities[ent].m_pClass = pServerClass;

	// Get either the static or instance baseline.
	int nFromTick = 0;	// MOTODO get tick when baseline last changed

	SerializedEntityHandle_t oldbaseline = SERIALIZED_ENTITY_HANDLE_INVALID;

	PackedEntity *baseline = u.m_bAsDelta ? GetEntityBaseline( u.m_nBaseline, ent ) : NULL;

	if ( baseline && baseline->m_pClientClass == pClientClass )
	{
		oldbaseline = baseline->GetPackedData();
	}
	else
	{
		// Every entity must have a static or an instance baseline when we get here.
		ErrorIfNot(
			GetClassBaseline( iClass, &oldbaseline ),
			("HLTV_CopyNewEntity: GetDynamicBaseline(%d) failed.", iClass)
		);
	}

	// create new ChangeFrameList containing all properties set as changed
	int nFlatProps = SendTable_GetNumFlatProps( pServerClass->m_pTable );
	CChangeFrameList *pChangeFrame = NULL;
	
	if ( !m_bSaveMemory )
	{
		pChangeFrame = new CChangeFrameList( nFlatProps, nFromTick );
	}

	// Now make a PackedEntity and store the new packed data in there.
	PackedEntity *pPackedEntity = framesnapshotmanager->CreatePackedEntity( pSnapshot, ent );
	pPackedEntity->SetChangeFrameList( pChangeFrame );
	pPackedEntity->SetServerAndClientClass( pServerClass, pClientClass );

	// Make space for the baseline data.
	SerializedEntityHandle_t newbaseline = g_pSerializedEntities->AllocateSerializedEntity(__FILE__, __LINE__);

	CUtlVector< int > changedProps;

	RecvTable_ReadFieldList( pClientClass->m_pRecvTable, *u.m_pBuf, u.m_DecodeEntity, -1, false );

	// decode basline, is compressed against zero values 
	RecvTable_MergeDeltas( pClientClass->m_pRecvTable, oldbaseline, u.m_DecodeEntity, newbaseline, -1, &changedProps );

	// update change tick in ChangeFrameList
	if ( pChangeFrame )
	{
		pChangeFrame->SetChangeTick( changedProps.Base(), changedProps.Count(), pSnapshot->m_nTickCount );
	}

	if ( u.m_bUpdateBaselines )
	{
		SetEntityBaseline( (u.m_nBaseline==0)?1:0, pClientClass, u.m_nNewEntity, newbaseline );
	}

	pPackedEntity->CopyPackedData( newbaseline );

	// If ent doesn't think it's in PVS, signal that it is
	Assert( u.m_pTo->last_entity <= ent );
	u.m_pTo->last_entity = ent;
	u.m_pTo->transmit_entity.Set( ent );
}

static inline void HLTV_CopyExitingEnt( CEntityReadInfo &u )
{
	if ( !u.m_bAsDelta )  // Should never happen on a full update.
	{
		Assert(0); // GetBaseLocalClient().validsequence = 0;
		ConMsg( "WARNING: CopyExitingEnt on full update.\n" );
		u.m_UpdateType = Failed;	// break out
		return;
	}

	CFrameSnapshot *pFromSnapshot =	u.m_pFrom->GetSnapshot();	// get from snapshot
	
	const int ent = u.m_nOldEntity;
	
	CFrameSnapshot *pSnapshot = u.m_pTo->GetSnapshot(); // get to snapshot
	
	// copy ent handle, serial numbers & class info
	Assert( ent < pFromSnapshot->m_nNumEntities );
	pSnapshot->m_pEntities[ent] = pFromSnapshot->m_pEntities[ent];
	
	Assert( pSnapshot->m_pEntities[ent].m_pPackedData != INVALID_PACKED_ENTITY_HANDLE );

	// increase PackedEntity reference counter
	PackedEntity *pEntity =	framesnapshotmanager->GetPackedEntity( *pSnapshot, ent );
	Assert( pEntity );
	pEntity->m_ReferenceCount++;


	Assert( u.m_pTo->last_entity <= ent );
	
	// mark flags as received
	u.m_pTo->last_entity = ent;
	u.m_pTo->transmit_entity.Set( ent );
}


//-----------------------------------------------------------------------------
// Purpose: A svc_signonnum has been received, perform a client side setup
// Output : void CL_SignonReply
//-----------------------------------------------------------------------------
bool CHLTVClientState::SetSignonState ( int state, int count, const CNETMsg_SignonState *msg )
{
	//	ConDMsg ("CL_SignonReply: %i\n", GetBaseLocalClient().signon);

	if ( !CBaseClientState::SetSignonState( state, count, msg ) )
		return false;
	
	Assert ( m_nSignonState == state );

	switch ( m_nSignonState )
	{
		case SIGNONSTATE_CHALLENGE	:	break;
		case SIGNONSTATE_CONNECTED	:	{
											// allow longer timeout
											m_NetChannel->SetTimeout( SIGNON_TIME_OUT );

											m_NetChannel->Clear();
											// set user settings (rate etc)
											CNETMsg_SetConVar_t convars;
											Host_BuildUserInfoUpdateMessage( 0, convars.mutable_convars(), false );

											// also set all the userinfo vars that we will be modifying for accurate tracking
											SetLocalInfoConvarsForUpstreamConnection( *convars.mutable_convars(), true );

											m_NetChannel->SendNetMsg( convars );
										}
										break;

		case SIGNONSTATE_NEW		:	SendClientInfo();
										break;

		case SIGNONSTATE_PRESPAWN	:	break;
		
		case SIGNONSTATE_SPAWN		:	m_pHLTV->SignonComplete();
										break;

		case SIGNONSTATE_FULL		:	m_NetChannel->SetTimeout( tv_timeout.GetFloat() );
										// start new recording if autorecord is enabled
										if ( tv_autorecord.GetBool() )
										{
											m_pHLTV->m_DemoRecorder.StartAutoRecording();
											m_NetChannel->SetDemoRecorder( m_pHLTV->m_DemoRecorder.GetDemoRecorder() );
										}
										break;

		case SIGNONSTATE_CHANGELEVEL:	m_pHLTV->Changelevel( true );
										m_NetChannel->SetTimeout( SIGNON_TIME_OUT );  // allow 5 minutes timeout
										break;
	}

	if ( m_nSignonState >= SIGNONSTATE_CONNECTED )
	{
		// tell server that we entered now that state
		CNETMsg_SignonState_t signonState(  m_nSignonState, count );
		m_NetChannel->SendNetMsg( signonState );
	}

	return true;
}

void CHLTVClientState::SendClientInfo( void )
{
	CCLCMsg_ClientInfo_t info;

	info.set_send_table_crc( SendTable_GetCRC() );
	info.set_server_count( m_nServerCount );
	info.set_is_hltv( true );
#if defined( REPLAY_ENABLED )
	info.set_is_replay( false );
#endif
	info.set_friends_id( 0 );
	// info.set_friends_name( "" );

	// CheckOwnCustomFiles(); // load & verfiy custom player files

	// for ( int i=0; i< MAX_CUSTOM_FILES; i++ )
	//	info.add_custom_files( "" );

	m_NetChannel->SendNetMsg( info );
}


void CHLTVClientState::SendPacket()
{
	if ( !IsConnected() )
		return;

	if ( ( net_time < m_flNextCmdTime ) || !m_NetChannel->CanPacket() )  
		return;
	
	if ( IsActive() )
	{
		CNETMsg_Tick_t tick( m_nDeltaTick, host_frameendtime_computationduration, host_frametime_stddeviation, host_framestarttime_stddeviation );
		m_NetChannel->SendNetMsg( tick );
	}

	m_NetChannel->SendDatagram( NULL );

	if ( IsActive() )
	{
		// use full update rate when active
		float commandInterval = (2.0f/3.0f) / m_pHLTV->GetSnapshotRate();
		float maxDelta = MIN( host_state.interval_per_tick, commandInterval );
		float delta = clamp( net_time - m_flNextCmdTime, 0.0f, maxDelta );
		m_flNextCmdTime = net_time + commandInterval - delta;
	}
	else
	{
		// during signon process send only 5 packets/second
		m_flNextCmdTime = net_time + ( 1.0f / 5.0f );
	}
}

bool CHLTVClientState::NETMsg_StringCmd(const CNETMsg_StringCmd& msg)
{
	CNETMsg_StringCmd_t stringcmd( msg.command().c_str() );
	stringcmd.SetReliable( m_NetChannel->WasLastMessageReliable() );
	return m_pHLTV->SendNetMsg( stringcmd ); // relay to server
}

bool CHLTVClientState::NETMsg_SetConVar(const CNETMsg_SetConVar& msg)
{
	if ( !CBaseClientState::NETMsg_SetConVar( msg ) )
		return false;

	CNETMsg_SetConVar_t sendmsg;
	sendmsg.CopyFrom( msg );
	if ( sendmsg.convars().cvars_size() )
	{	// Make sure convars are expanded using dictionary
		for ( int iCV = 0; iCV < sendmsg.convars().cvars_size(); ++iCV )
		{
			CMsg_CVars::CVar *convar = sendmsg.mutable_convars()->mutable_cvars( iCV );
			NetMsgExpandCVarUsingDictionary( convar );
		}
	}
	sendmsg.SetReliable( m_NetChannel->WasLastMessageReliable() );
	return m_pHLTV->SendNetMsg( sendmsg ); // relay to server
}

bool CHLTVClientState::NETMsg_PlayerAvatarData( const CNETMsg_PlayerAvatarData& msg )
{
	// Don't chain to the base client implementation
	return m_pHLTV->NETMsg_PlayerAvatarData( msg ); // relay to server
}

void CHLTVClientState::Clear()
{
	CBaseClientState::Clear();

	m_pNewClientFrame = NULL;
	m_pCurrentClientFrame = NULL;

	eventid_hltv_status = -1;
	eventid_hltv_title = -1;
}

bool CHLTVClientState::SVCMsg_ServerInfo( const CSVCMsg_ServerInfo& msg )
{
	// Reset client state
	Clear();

	// is server a HLTV proxy or demo file ?
	if ( !m_pHLTV->IsPlayingBack() )
	{
		if ( !msg.is_hltv() )
		{
			ConMsg ( "Server (%s) is not a GOTV proxy.\n", m_NetChannel->GetAddress() );
			Disconnect();
			return false; 
		}	
	}

	// tell HLTV relay to clear everything
	m_pHLTV->StartRelay();

	// Process the message
	if ( !CBaseClientState::SVCMsg_ServerInfo( msg ) )
	{
		Disconnect();
		return false;
	}

	m_StringTableContainer = m_pHLTV->m_StringTables;

	Assert( m_StringTableContainer->GetNumTables() == 0); // must be empty

#ifndef SHARED_NET_STRING_TABLES
	// relay uses normal string tables without a change history
	m_StringTableContainer->EnableRollback( false );
#endif

	// copy setting from HLTV client to HLTV server 
	m_pHLTV->m_nGameServerMaxClients = m_nMaxClients;
	m_pHLTV->serverclasses		= m_nServerClasses;
	m_pHLTV->serverclassbits	= m_nServerClassBits;
	m_pHLTV->m_nPlayerSlot		= m_nPlayerSlot;

	// copy other settings to HLTV server
	m_pHLTV->worldmapCRC		= msg.map_crc();
	m_pHLTV->clientDllCRC		= msg.client_crc();
	m_pHLTV->stringTableCRC		= CRC32_ConvertFromUnsignedLong( msg.string_table_crc() );
	m_pHLTV->m_flTickInterval	= msg.tick_interval();

	host_state.interval_per_tick = msg.tick_interval();

	Q_strncpy( m_pHLTV->m_szMapname, msg.map_name().c_str(), sizeof(m_pHLTV->m_szMapname) );
	Q_strncpy( m_pHLTV->m_szSkyname, msg.sky_name().c_str(), sizeof(m_pHLTV->m_szSkyname) );

	return true;
}

bool CHLTVClientState::SVCMsg_ClassInfo( const CSVCMsg_ClassInfo& msg )
{
	if ( !msg.create_on_client() )
	{
		ConMsg("HLTV SendTable CRC differs from server.\n");
		Disconnect();
		return false;
	}

#ifdef _HLTVTEST
	RecvTable_Term( false );
#endif

	// Create all of the send tables locally
	DataTable_CreateClientTablesFromServerTables();

	// Now create all of the server classes locally, too
	DataTable_CreateClientClassInfosFromServerClasses( this );

	LinkClasses();	// link server and client classes

#if defined(DEDICATED)
	bool bAllowMismatches = false;
#else
	bool bAllowMismatches = ( g_pClientDemoPlayer && g_pClientDemoPlayer->IsPlayingBack() );
#endif
	if ( !RecvTable_CreateDecoders( serverGameDLL->GetStandardSendProxies(), bAllowMismatches ) ) // create receive table decoders
	{
		Host_EndGame( true, "CL_ParseClassInfo_EndClasses: CreateDecoders failed.\n" );
		return false;
	}

	return true;
}

void CHLTVClientState::PacketEnd( void )
{
	// did we get a snapshot with this packet ?
	if ( m_pNewClientFrame )
	{
		// if so, add a new frame to HLTV
		m_pCurrentClientFrame = m_pHLTV->AddNewFrame( m_pNewClientFrame );
		delete m_pNewClientFrame; // release own refernces
		m_pNewClientFrame = NULL;
	}
}

bool CHLTVClientState::HookClientStringTable( char const *tableName )
{
	INetworkStringTable *table = GetStringTable( tableName );
	if ( !table )
		return false;

	// Hook instance baseline table
	if ( !Q_strcasecmp( tableName, INSTANCE_BASELINE_TABLENAME ) )
	{
		table->SetStringChangedCallback( m_pHLTV,  HLTV_Callback_InstanceBaseline );
		return true;
	}

	return false;
}

void CHLTVClientState::InstallStringTableCallback( char const *tableName )
{
	INetworkStringTable *table = GetStringTable( tableName );

	if ( !table )
		return;

	// Hook instance baseline table
	if ( !Q_strcasecmp( tableName, INSTANCE_BASELINE_TABLENAME ) )
	{
		table->SetStringChangedCallback( m_pHLTV,  HLTV_Callback_InstanceBaseline );
		return;
	}
}

bool CHLTVClientState::SVCMsg_SetView( const CSVCMsg_SetView& msg )
{
	if ( !CBaseClientState::SVCMsg_SetView( msg ) )
		return false;

	CSVCMsg_SetView_t sendmsg;
	sendmsg.CopyFrom( msg );
	sendmsg.SetReliable( m_NetChannel->WasLastMessageReliable() );
	return m_pHLTV->SendNetMsg( sendmsg );
}

bool CHLTVClientState::SVCMsg_VoiceInit( const CSVCMsg_VoiceInit& msg )
{
	CSVCMsg_VoiceInit_t sendmsg;
	sendmsg.CopyFrom( msg );
	sendmsg.SetReliable( m_NetChannel->WasLastMessageReliable() );
	return m_pHLTV->SendNetMsg( sendmsg ); // relay to server
}

bool CHLTVClientState::SVCMsg_VoiceData( const CSVCMsg_VoiceData &msg )
{   									 
	CSVCMsg_VoiceData_t sendmsg;
	sendmsg.CopyFrom( msg );
	sendmsg.SetReliable( m_NetChannel->WasLastMessageReliable() );
	return m_pHLTV->SendNetMsg( sendmsg ); // relay to server
}

bool CHLTVClientState::SVCMsg_EncryptedData( const CSVCMsg_EncryptedData& msg )
{   									 
	CSVCMsg_EncryptedData_t sendmsg;
	sendmsg.CopyFrom( msg );
	sendmsg.SetReliable( m_NetChannel->WasLastMessageReliable() );
	return m_pHLTV->SendNetMsg( sendmsg ); // relay to server
}

bool CHLTVClientState::SVCMsg_Sounds(const CSVCMsg_Sounds& msg)
{
	CSVCMsg_Sounds_t sendmsg;
	sendmsg.CopyFrom( msg );
	sendmsg.SetReliable( m_NetChannel->WasLastMessageReliable() );
	return m_pHLTV->SendNetMsg( sendmsg ); // relay to server
}

bool CHLTVClientState::SVCMsg_Prefetch( const CSVCMsg_Prefetch& msg )
{
	CSVCMsg_Prefetch_t sendmsg;
	sendmsg.CopyFrom( msg );
	sendmsg.SetReliable( m_NetChannel->WasLastMessageReliable() );
	return m_pHLTV->SendNetMsg( sendmsg ); // relay to server
}

bool CHLTVClientState::SVCMsg_FixAngle( const CSVCMsg_FixAngle& msg )
{
	CSVCMsg_FixAngle_t sendmsg;
	sendmsg.CopyFrom( msg );
	sendmsg.SetReliable( m_NetChannel->WasLastMessageReliable() );
	return m_pHLTV->SendNetMsg( sendmsg ); // relay to server
}

bool CHLTVClientState::SVCMsg_CrosshairAngle( const CSVCMsg_CrosshairAngle& msg )
{
	CSVCMsg_CrosshairAngle_t sendmsg;
	sendmsg.CopyFrom( msg );
	sendmsg.SetReliable( m_NetChannel->WasLastMessageReliable() );
	return m_pHLTV->SendNetMsg( sendmsg ); // relay to server
}

bool CHLTVClientState::SVCMsg_BSPDecal( const CSVCMsg_BSPDecal& msg )
{
	CSVCMsg_BSPDecal_t sendmsg;
	sendmsg.CopyFrom( msg );
	sendmsg.SetReliable( m_NetChannel->WasLastMessageReliable() );
	return m_pHLTV->SendNetMsg( sendmsg ); // relay to server
}

bool CHLTVClientState::SVCMsg_GameEvent( const CSVCMsg_GameEvent& msg )
{
	const char *pszName = msg.event_name().c_str();

	bool bDontForward = false;

	if ( msg.eventid() == eventid_hltv_status || Q_strcmp( pszName, "hltv_status" ) == 0 )
	{
		IGameEvent *event = g_GameEventManager.UnserializeEvent( msg );
		m_pHLTV->m_nGlobalSlots = event->GetInt("slots");
		m_pHLTV->m_nGlobalProxies = event->GetInt("proxies");
		m_pHLTV->m_nGlobalClients = event->GetInt("clients");
		m_pHLTV->m_nExternalTotalViewers = event->GetInt( "externaltotal" );
		m_pHLTV->m_nExternalLinkedViewers = event->GetInt( "externallinked" );
		
		char const *szMasterAddress = event->GetString("master");
		if ( szMasterAddress && *szMasterAddress )
			m_pHLTV->m_RootServer.SetFromString( szMasterAddress );
		else if ( m_pHLTV->m_RootServer.IsValid() )
			m_pHLTV->m_RootServer.Clear();

		g_GameEventManager.FreeEvent( event );
		bDontForward = true;

		// make sure we update GC information now that we updated HLTV status
		if ( serverGameDLL && Steam3Server().GetGSSteamID().IsValid() )
			serverGameDLL->UpdateGCInformation();
	}
	else if ( msg.eventid() == eventid_hltv_title || Q_strcmp( pszName, "hltv_title" ) == 0 )
	{
		// ignore title messages
		bDontForward = true;
	}

	if ( bDontForward )
		return true;

	// forward event
	CSVCMsg_GameEvent_t sendmsg;
	sendmsg.CopyFrom( msg );
	sendmsg.SetReliable( m_NetChannel->WasLastMessageReliable() );

	return m_pHLTV->SendNetMsg( sendmsg ); // relay to server
}

bool CHLTVClientState::SVCMsg_GameEventList( const CSVCMsg_GameEventList& msg )
{
	if ( !CBaseClientState::SVCMsg_GameEventList( msg ) )
		return false;

	// cache off the eventids for hltv_status and hltv_title
	CGameEventDescriptor *pDescriptor_hltv_status = g_GameEventManager.GetEventDescriptor( "hltv_status" );
	CGameEventDescriptor *pDescriptor_hltv_title = g_GameEventManager.GetEventDescriptor( "hltv_title" );

	if ( pDescriptor_hltv_status )
	{
		eventid_hltv_status = pDescriptor_hltv_status->eventid;
	}
	if ( pDescriptor_hltv_title )
	{
		eventid_hltv_title = pDescriptor_hltv_title->eventid;
	}

	CSVCMsg_GameEventList_t sendmsg;
	sendmsg.CopyFrom( msg );
	sendmsg.SetReliable( m_NetChannel->WasLastMessageReliable() );
	return m_pHLTV->SendNetMsg( sendmsg ); // relay to server
}

bool CHLTVClientState::SVCMsg_UserMessage( const CSVCMsg_UserMessage& msg )
{
	CSVCMsg_UserMessage_t sendmsg;
	sendmsg.CopyFrom( msg );
	sendmsg.SetReliable( m_NetChannel->WasLastMessageReliable() );
	return m_pHLTV->SendNetMsg( sendmsg ); // relay to server
}

bool CHLTVClientState::SVCMsg_EntityMsg( const CSVCMsg_EntityMsg& msg )
{
	CSVCMsg_EntityMsg_t sendmsg;
	sendmsg.CopyFrom( msg );
	sendmsg.SetReliable( m_NetChannel->WasLastMessageReliable() );
	return m_pHLTV->SendNetMsg( sendmsg ); // relay to server
}

bool CHLTVClientState::SVCMsg_Menu( const CSVCMsg_Menu& msg )
{
	CSVCMsg_Menu_t sendmsg;
	sendmsg.CopyFrom( msg );
	sendmsg.SetReliable( m_NetChannel->WasLastMessageReliable() );
	return m_pHLTV->SendNetMsg( sendmsg ); // relay to server
}

bool CHLTVClientState::SVCMsg_PacketEntities( const CSVCMsg_PacketEntities &msg )
{
	CClientFrame *oldFrame = NULL;

#ifdef _HLTVTEST
	if ( g_RecvDecoders.Count() == 0 )
		return false;
#endif

	if ( msg.is_delta() )
	{
		if ( GetServerTickCount() == msg.delta_from() )
		{
			Host_Error( "Update self-referencing, connection dropped.\n" );
			return false;
		}

		// Otherwise, mark where we are valid to and point to the packet entities we'll be updating from.
		oldFrame = m_pHLTV->GetClientFrame( msg.delta_from() );
	}

	// create new empty snapshot
	CFrameSnapshot* pSnapshot = framesnapshotmanager->CreateEmptySnapshot(
#ifdef DEBUG_SNAPSHOT_REFERENCES
		"CHLTVClientState::SVCMsg_PacketEntities",
#endif
		GetServerTickCount(), msg.max_entries() );

	Assert( m_pNewClientFrame == NULL );
	
	m_pNewClientFrame = new CClientFrame( pSnapshot );

	Assert( msg.baseline() >= 0 && msg.baseline() < 2 );

	if ( msg.update_baseline() )
	{
		// server requested to use this snapshot as baseline update
		int nUpdateBaseline = (msg.baseline() == 0) ? 1 : 0;
		CopyEntityBaseline( msg.baseline(), nUpdateBaseline );

		// send new baseline acknowledgement(as reliable)
		CCLCMsg_BaselineAck_t baseline;
		baseline.set_baseline_tick( GetServerTickCount() );
		baseline.set_baseline_nr( msg.baseline() );
		m_NetChannel->SendNetMsg( baseline, true );
	}

	// copy classes and serial numbers from current frame
	if ( m_pCurrentClientFrame )
	{
		CFrameSnapshot* pLastSnapshot = m_pCurrentClientFrame->GetSnapshot();
		CFrameSnapshotEntry *pEntry = pSnapshot->m_pEntities;
		CFrameSnapshotEntry *pLastEntry = pLastSnapshot->m_pEntities;

		Assert( pLastSnapshot->m_nNumEntities <= pSnapshot->m_nNumEntities );

		for ( int i = 0; i<pLastSnapshot->m_nNumEntities; i++ )
		{
			pEntry->m_nSerialNumber = pLastEntry->m_nSerialNumber; 
			pEntry->m_pClass = pLastEntry->m_pClass;

			pEntry++;
			pLastEntry++;
		}
	}

	CEntityReadInfo u;
	bf_read entityBuf( &msg.entity_data()[0], msg.entity_data().size() );
	u.m_pBuf = &entityBuf;
	u.m_pFrom = oldFrame;
	u.m_pTo = m_pNewClientFrame;
	u.m_bAsDelta = msg.is_delta();
	u.m_nHeaderCount = msg.updated_entries();
	u.m_nBaseline = msg.baseline();
	u.m_bUpdateBaselines = msg.update_baseline();

	ReadPacketEntities( u );

	// adjust reference count to be 1
	pSnapshot->ReleaseReference();

	return CBaseClientState::SVCMsg_PacketEntities( msg );
}

bool CHLTVClientState::SVCMsg_TempEntities( const CSVCMsg_TempEntities &msg )
{
	CSVCMsg_TempEntities_t copy;
	copy.CopyFrom( msg );
	copy.SetReliable( m_NetChannel->WasLastMessageReliable() );
	return m_pHLTV->SendNetMsg( copy ); // relay to server
}

bool CHLTVClientState::SVCMsg_PaintmapData( const CSVCMsg_PaintmapData& msg )
{
	CSVCMsg_PaintmapData_t sendmsg;
	sendmsg.CopyFrom( msg );
	sendmsg.SetReliable( m_NetChannel->WasLastMessageReliable() );
	return m_pHLTV->SendNetMsg( sendmsg ); // relay to server
}


void CHLTVClientState::ReadEnterPVS( CEntityReadInfo &u )
{
	int iClass = u.m_pBuf->ReadUBitLong( m_nServerClassBits );

	int iSerialNum = u.m_pBuf->ReadUBitLong( NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS );

	CopyNewEntity( u, iClass, iSerialNum );

	if ( u.m_nNewEntity == u.m_nOldEntity ) // that was a recreate
		u.NextOldEntity();
}

void CHLTVClientState::ReadLeavePVS( CEntityReadInfo &u )
{
	// do nothing, this entity was removed
	Assert( !u.m_pTo->transmit_entity.Get(u.m_nOldEntity) );

	if ( u.m_UpdateFlags & FHDR_DELETE )
	{
		CFrameSnapshot *pSnapshot = u.m_pTo->GetSnapshot();
		CFrameSnapshotEntry *pEntry = &pSnapshot->m_pEntities[u.m_nOldEntity];

		// clear entity references
		pEntry->m_nSerialNumber = -1;
		pEntry->m_pClass = NULL;
		Assert( pEntry->m_pPackedData == INVALID_PACKED_ENTITY_HANDLE );
	}

	u.NextOldEntity();

}

void CHLTVClientState::ReadDeltaEnt( CEntityReadInfo &u )
{
	const int i = u.m_nNewEntity;
	CFrameSnapshot *pFromSnapshot =	u.m_pFrom->GetSnapshot();

	CFrameSnapshot *pSnapshot = u.m_pTo->GetSnapshot();

	Assert( i < pFromSnapshot->m_nNumEntities );
	pSnapshot->m_pEntities[i] = pFromSnapshot->m_pEntities[i];
	
	PackedEntity *pToPackedEntity = framesnapshotmanager->CreatePackedEntity( pSnapshot, i );

	// WARNING! get pFromPackedEntity after new pPackedEntity has been created, otherwise pointer may be wrong
	PackedEntity *pFromPackedEntity = framesnapshotmanager->GetPackedEntity( *pFromSnapshot, i );

	pToPackedEntity->SetServerAndClientClass( pFromPackedEntity->m_pServerClass, pFromPackedEntity->m_pClientClass );

	// create a copy of the pFromSnapshot ChangeFrameList
	CChangeFrameList* pChangeFrame = pFromPackedEntity->GetChangeFrameList()->Copy();
	pToPackedEntity->SetChangeFrameList( pChangeFrame );

	// Make space for the baseline data.
	SerializedEntityHandle_t fromEntity = pFromPackedEntity->GetPackedData();

	SerializedEntityHandle_t outEntity = g_pSerializedEntities->AllocateSerializedEntity(__FILE__, __LINE__);

	CUtlVector< int > changedProps;

	RecvTable_ReadFieldList( pToPackedEntity->m_pClientClass->m_pRecvTable, *u.m_pBuf, u.m_DecodeEntity, -1, false );

	// decode baseline, is compressed against zero values 
	RecvTable_MergeDeltas( pToPackedEntity->m_pClientClass->m_pRecvTable,
		fromEntity, u.m_DecodeEntity, outEntity, -1, &changedProps );

	// update change tick in ChangeFrameList
	if ( pChangeFrame )
	{
		pChangeFrame->SetChangeTick( changedProps.Base(), changedProps.Count(), pSnapshot->m_nTickCount );
	}

	// store as normal
	pToPackedEntity->SetPackedData( outEntity );

	u.m_pTo->last_entity = u.m_nNewEntity;
	u.m_pTo->transmit_entity.Set( u.m_nNewEntity );

	u.NextOldEntity();
}

void CHLTVClientState::ReadPreserveEnt( CEntityReadInfo &u )
{
	// copy one of the old entities over to the new packet unchanged
	if ( u.m_nNewEntity < 0 || u.m_nNewEntity >= MAX_EDICTS )
	{
		Host_Error ("CL_ReadPreserveEnt: u.m_nNewEntity == MAX_EDICTS");
	}

	HLTV_CopyExitingEnt( u );
	
	u.NextOldEntity();
}

void CHLTVClientState::ReadDeletions( CEntityReadInfo &u )
{
	int nBase = -1;
	int nCount = u.m_pBuf->ReadUBitVar();
	for ( int i = 0; i < nCount; ++i )
	{
		int nDelta = u.m_pBuf->ReadUBitVar();
		int nSlot = nBase + nDelta;

		Assert( !u.m_pTo->transmit_entity.Get( nSlot ) );

		CFrameSnapshot *pSnapshot = u.m_pTo->GetSnapshot();
		CFrameSnapshotEntry *pEntry = &pSnapshot->m_pEntities[nSlot];

		// clear entity references
		pEntry->m_nSerialNumber = -1;
		pEntry->m_pClass = NULL;
		Assert( pEntry->m_pPackedData == INVALID_PACKED_ENTITY_HANDLE );
		
		nBase = nSlot;
	}
}

// Returns false if you should stop reading entities.
inline static bool CL_DetermineUpdateType( CEntityReadInfo &u )
{
	if ( !u.m_bIsEntity || ( u.m_nNewEntity > u.m_nOldEntity ) )
	{
		// If we're at the last entity, preserve whatever entities followed it in the old packet.
		// If newnum > oldnum, then the server skipped sending entities that it wants to leave the state alone for.
		if ( !u.m_pFrom	 || ( u.m_nOldEntity > u.m_pFrom->last_entity ) )
		{
			Assert( !u.m_bIsEntity );
			u.m_UpdateType = Finished;
			return false;
		}

		// Preserve entities until we reach newnum (ie: the server didn't send certain entities because
		// they haven't changed).
		u.m_UpdateType = PreserveEnt;
	}
	else
	{
		if( u.m_UpdateFlags & FHDR_ENTERPVS )
		{
			u.m_UpdateType = EnterPVS;
		}
		else if( u.m_UpdateFlags & FHDR_LEAVEPVS )
		{
			u.m_UpdateType = LeavePVS;
		}
		else
		{
			u.m_UpdateType = DeltaEnt;
		}
	}

	return true;
}

static inline void CL_ParseDeltaHeader( CEntityReadInfo &u )
{
	u.m_UpdateFlags = FHDR_ZERO;

#ifdef DEBUG_NETWORKING
	int startbit = u.m_pBuf->GetNumBitsRead();
#endif
	u.m_nNewEntity = u.m_nHeaderBase + 1 + u.m_pBuf->ReadUBitVar();


	u.m_nHeaderBase = u.m_nNewEntity;

	// leave pvs flag
	if ( u.m_pBuf->ReadOneBit() == 0 )
	{
		// enter pvs flag
		if ( u.m_pBuf->ReadOneBit() != 0 )
		{
			u.m_UpdateFlags |= FHDR_ENTERPVS;
		}
	}
	else
	{
		u.m_UpdateFlags |= FHDR_LEAVEPVS;

		// Force delete flag
		if ( u.m_pBuf->ReadOneBit() != 0 )
		{
			u.m_UpdateFlags |= FHDR_DELETE;
		}
	}
	// Output the bitstream...
#ifdef DEBUG_NETWORKING
	int lastbit = u.m_pBuf->GetNumBitsRead();
	{
		void	SpewBitStream( unsigned char* pMem, int bit, int lastbit );
		SpewBitStream( (byte *)u.m_pBuf->m_pData, startbit, lastbit );
	}
#endif
}


void CHLTVClientState::ReadPacketEntities( CEntityReadInfo &u )
{
	// Loop until there are no more entities to read

	u.NextOldEntity();

	while ( u.m_UpdateType < Finished )
	{
		u.m_nHeaderCount--;

		u.m_bIsEntity = ( u.m_nHeaderCount >= 0 ) ? true : false;

		if ( u.m_bIsEntity  )
		{
			CL_ParseDeltaHeader( u );
		}

		u.m_UpdateType = PreserveEnt;

		while( u.m_UpdateType == PreserveEnt )
		{
			// Figure out what kind of an update this is.
			if( CL_DetermineUpdateType( u ) )
			{
				switch( u.m_UpdateType )
				{
				case EnterPVS:		ReadEnterPVS( u );
					break;

				case LeavePVS:		ReadLeavePVS( u );
					break;

				case DeltaEnt:		ReadDeltaEnt( u );
					break;

				case PreserveEnt:	ReadPreserveEnt( u );
					break;

				default:			DevMsg(1, "ReadPacketEntities: unknown updatetype %i\n", u.m_UpdateType );
					break;
				}
			}
		}
	}

	// Now process explicit deletes 
	if ( u.m_bAsDelta && u.m_UpdateType == Finished )
	{
		ReadDeletions( u );
	}

	// Something didn't parse...
	if ( u.m_pBuf->IsOverflowed() )							
	{	
		Host_Error ( "CL_ParsePacketEntities:  buffer read overflow\n" );
	}

	// If we get an uncompressed packet, then the server is waiting for us to ack the validsequence
	// that we got the uncompressed packet on. So we stop reading packets here and force ourselves to
	// send the clc_move on the next frame.

	if ( !u.m_bAsDelta )
	{
		m_flNextCmdTime = 0.0; // answer ASAP to confirm full update tick
	} 
}

int CHLTVClientState::GetConnectionRetryNumber() const
{
	if ( tv_autoretry.GetBool() )
	{
		// in autoretry mode try extra long
		return CL_CONNECTION_RETRIES * 4;
	}

	return CBaseClientState::GetConnectionRetryNumber();
}

void CHLTVClientState::ConnectionCrashed(const char *reason)
{
	CBaseClientState::ConnectionCrashed( reason );

	if ( tv_autoretry.GetBool() && m_Remote.Count() > 0 )
	{
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), va( "tv_relay %s\n", m_Remote.Get( 0 ).m_szRetryAddress.String() ) );
	}
}

void CHLTVClientState::ConnectionClosing( const char *reason )
{
	CBaseClientState::ConnectionClosing( reason );

	if ( tv_autoretry.GetBool() && m_Remote.Count() > 0 )
	{
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), va( "tv_relay %s\n", m_Remote.Get( 0 ).m_szRetryAddress.String() ) );
	}
}

void CHLTVClientState::Disconnect( bool bShowMainMenu /* = true */ )
{
	CBaseClientState::Disconnect( bShowMainMenu );

	// Update hibernation state on the main server which controls hibernation and shutdown criteria
	sv.UpdateHibernationState();

	// Shutdown if we are an official relay
	if ( serverGameDLL && serverGameDLL->IsValveDS() && !sv.IsActive() )
	{
		Msg( "Official relay disconnected and shutting down...\n" );
		Cbuf_AddText( CBUF_SERVER, "quit;\n" );
	}
}

void CHLTVClientState::RunFrame()
{
	CBaseClientState::RunFrame();

	if ( m_NetChannel && m_NetChannel->IsTimedOut() && IsConnected() )
	{
		ConMsg ("\nGOTV connection timed out.\n");
		Disconnect();
		return;
	}

	UpdateStats();
}

void CHLTVClientState::SetLocalInfoConvarsForUpstreamConnection( CMsg_CVars &cvars, bool bMaxSlots )
{
	int proxies, slots, clients;

	m_pHLTV->GetRelayStats( proxies, slots, clients );

	extern ConVar tv_maxclients_relayreserved;
	int numSlots = m_pHLTV->GetMaxClients();
	if ( !numSlots && bMaxSlots )
	{
		extern ConVar tv_maxclients;
		numSlots = tv_maxclients.GetInt();
	}
	if ( tv_maxclients_relayreserved.GetInt() > 0 )
		numSlots -= tv_maxclients_relayreserved.GetInt();
	numSlots = MAX( 0, numSlots );

	int numClients = m_pHLTV->GetNumClients();
	if ( numClients > numSlots )
		numSlots = numClients;

	proxies += 1; // add self to number of proxies
	slots += numSlots;	// add own slots
	clients += numClients; // add own clients

	// let server know that we are a proxy server and all our stats
	NetMsgSetCVarUsingDictionary( cvars.add_cvars(), "tv_relay", "1" );
	NetMsgSetCVarUsingDictionary( cvars.add_cvars(), "hltv_proxies", va( "%d", proxies ) );
	NetMsgSetCVarUsingDictionary( cvars.add_cvars(), "hltv_clients", va( "%d", clients ) );
	NetMsgSetCVarUsingDictionary( cvars.add_cvars(), "hltv_slots", va( "%d", slots ) );
	
	static ConVarRef ipname_relay( "ip_relay" );	// Override relay IP for NAT hosts
	netadr_t netAdrHltvRelay( net_local_adr );
	if ( ipname_relay.IsValid() && ipname_relay.GetString()[0] )
	{
		netadr_t netAdrIpRelay;
		NET_StringToAdr( ipname_relay.GetString(), &netAdrIpRelay );
		if ( !netAdrIpRelay.IsLoopback() && !netAdrIpRelay.IsLocalhost() &&
			netAdrIpRelay.IsBaseAdrValid() )
		{
			// Good address specified in ipname_relay
			netAdrHltvRelay = netAdrIpRelay;
		}
	}
	NetMsgSetCVarUsingDictionary( cvars.add_cvars(), "hltv_addr", va( "%s:%u", netAdrHltvRelay.ToString( true ), m_pHLTV->GetUDPPort() ) );

	static ConVarRef sv_steamdatagramtransport_port( "sv_steamdatagramtransport_port" );
	if ( serverGameDLL->IsValveDS() && sv_steamdatagramtransport_port.GetInt() > 0 )
	{
		ns_address nsadrsdr;
		nsadrsdr.SetAddrType( NSAT_PROXIED_GAMESERVER );
		nsadrsdr.m_steamID.SetFromSteamID( Steam3Server().GetGSSteamID(), sv_steamdatagramtransport_port.GetInt() );
		NetMsgSetCVarUsingDictionary( cvars.add_cvars(), "hltv_sdr", ns_address_render( nsadrsdr ).String() );
	}
}

void CHLTVClientState::UpdateStats()
{
	if ( m_nSignonState < SIGNONSTATE_FULL )
	{
		m_fNextSendUpdateTime = 0.0f;
		return;
	}

	if ( m_fNextSendUpdateTime > net_time )
		return;

	m_fNextSendUpdateTime = net_time + 8.0f;

	CNETMsg_SetConVar_t conVars;
	SetLocalInfoConvarsForUpstreamConnection( *conVars.mutable_convars() );
	m_NetChannel->SendNetMsg( conVars );
}
	
