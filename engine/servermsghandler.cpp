//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef DEDICATED
#include "screen.h"
#include "cl_main.h"
#include "iprediction.h"
#include "proto_oob.h"
#include "demo.h"
#include "tier0/icommandline.h"
#include "ispatialpartitioninternal.h"
#include "GameEventManager.h"
#include "cdll_engine_int.h"
#include "voice.h"
#include "host_cmd.h"
#include "server.h"
#include "convar.h"
#include "dt_recv_eng.h"
#include "dt_common_eng.h"
#include "LocalNetworkBackdoor.h"
#include "vox.h"
#include "sound.h"
#include "r_efx.h"
#include "r_local.h"
#include "decal_private.h"
#include "vgui_baseui_interface.h"
#include "host_state.h"
#include "cl_ents_parse.h"
#include "eiface.h"
#include "server.h"
#include "cl_demoactionmanager.h"
#include "decal.h"
#include "r_decal.h"
#include "materialsystem/imaterial.h"
#include "EngineSoundInternal.h"
#include "master.h"
#include "ivideomode.h"
#include "download.h"
#include "GameUI/IGameUI.h"
#if defined( REPLAY_ENABLED )
#include "replayhistorymanager.h"
#endif
#include "cl_demo.h"

#include "audio_pch.h"

#include "paint.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IVEngineClient *engineClient;

extern CNetworkStringTableContainer *networkStringTableContainerClient;
extern CNetworkStringTableContainer *networkStringTableContainerServer;

static ConVar cl_allowupload ( "cl_allowupload", "1", FCVAR_ARCHIVE, "Client uploads customization files" );
static ConVar cl_voice_filter( "cl_voice_filter", "", 0, "Filter voice by name substring" ); // filter incoming voice data
ConVar cl_voice_hltv_buffer_time("cl_voice_hltv_buffer_time", "0.3", 0, "Amount of time between receiving voice data and playing the audio in hltv");
ConVar cl_voice_buffer_time("cl_voice_buffer_time", "0.04", 0, "Amount of time between receiving voice data and playing the audio");
extern ConCommand quit;

void CClientState::ConnectionClosing( const char * reason )
{
	// if connected, shut down host
	if ( m_nSignonState > SIGNONSTATE_NONE )
	{
		ConMsg( "Disconnect: %s.\n", reason );
		if ( !Q_stricmp( reason, INVALID_STEAM_TICKET ) )
		{
			g_eSteamLoginFailure = STEAMLOGINFAILURE_BADTICKET;
		}
		else if ( !Q_stricmp( reason, INVALID_STEAM_LOGON ) )
		{
			g_eSteamLoginFailure = STEAMLOGINFAILURE_NOSTEAMLOGIN;
		}
		else if ( !Q_stricmp( reason, INVALID_STEAM_LOGGED_IN_ELSEWHERE ) )
		{
			g_eSteamLoginFailure = STEAMLOGINFAILURE_LOGGED_IN_ELSEWHERE;
		}
		else if ( !Q_stricmp( reason, INVALID_STEAM_VACBANSTATE ) )
		{
			g_eSteamLoginFailure = STEAMLOGINFAILURE_VACBANNED;
		}
		else
		{
			g_eSteamLoginFailure = STEAMLOGINFAILURE_NONE;
		}

		// If the reason is a localized string, pass it raw.
		if ( reason && reason[0] == '#' )
			COM_ExplainDisconnection( true, "%s", reason );
		else
			COM_ExplainDisconnection( true, "Disconnect: %s.\n", reason );

		SCR_EndLoadingPlaque();
		Host_Disconnect(true);

		if ( (reason != NULL) && (Q_stricmp( reason, "Server shutting down" ) == 0) && //if disconnect reason is server shutdown
			(CommandLine()->FindParm( "-quitonservershutdown" ) != 0) ) //and we want to quit the game whenever the server shuts down (assists quick iteration)
		{
			Host_Shutdown(); //quit the game
		}
	}
}


void CClientState::ConnectionCrashed( const char * reason )
{
	// if connected, shut down host
	if ( m_nSignonState > SIGNONSTATE_NONE )
	{

		// If the reason is a localized string, pass it raw.
		if(reason && reason[0] == '#')
			COM_ExplainDisconnection( true, "%s", reason );
		else
			COM_ExplainDisconnection( true, "Disconnect: %s.\n", reason );

		SCR_EndLoadingPlaque();
		Host_Disconnect(true);
	}
}


void CClientState::FileRequested(const char *fileName, unsigned int transferID, bool isReplayDemoFile )
{
	ConMsg( "File '%s' requested from server %s.\n", fileName, m_NetChannel->GetAddress() );

	if ( !cl_allowupload.GetBool() )
	{
		ConMsg( "File uploading disabled.\n" );
		m_NetChannel->DenyFile( fileName, transferID, isReplayDemoFile );
		return;
	}

	// TODO check if file valid for uploading
	m_NetChannel->SendFile( fileName, transferID, isReplayDemoFile );
}

void CClientState::FileReceived( const char * fileName, unsigned int transferID, bool isReplayDemoFile )
{
	// check if the client donwload manager requested this file
	CL_FileReceived( fileName, transferID, isReplayDemoFile );
}

void CClientState::FileDenied(const char *fileName, unsigned int transferID, bool isReplayDemoFile )
{
	// check if the file download manager requested that file
	CL_FileDenied( fileName, transferID, isReplayDemoFile );
}


void CClientState::PacketStart( int incoming_sequence, int outgoing_acknowledged	)
{
	// Ack'd incoming messages.
	m_nCurrentSequence = incoming_sequence;
	command_ack = outgoing_acknowledged;
}


void CClientState::PacketEnd()
{
	//
	// we don't know if it is ok to save a demo message until
	// after we have parsed the frame
	//

	// Play any sounds we received this packet
	CL_DispatchSounds();
	
	// Did we get any messages this tick (i.e., did we call PreEntityPacketReceived)?
	if ( GetServerTickCount() != m_nDeltaTick )
		return;

	// How many commands total did we run this frame
	int commands_acknowledged = command_ack - last_command_ack;

//	COM_Log( "GetBaseLocalClient().log", "Server ack'd %i commands this frame\n", commands_acknowledged );

	//Msg( "%i/%i CL_PostReadMessages:  last ack %i most recent %i acked %i commands\n", 
	//	host_framecount, GetBaseLocalClient().tickcount,
	//	GetBaseLocalClient().last_command_ack, 
	//	GetBaseLocalClient().netchan->outgoing_sequence - 1,
	//	commands_acknowledged );

	// Highest command parsed from messages
	last_command_ack = command_ack;
	last_server_tick = GetServerTickCount();
	
	// Let prediction copy off pristine data and report any errors, etc.
	g_pClientSidePrediction->PostNetworkDataReceived( commands_acknowledged );

	demoaction->DispatchEvents();
}

void CClientState::Disconnect( bool bShowMainMenu )
{
	CBaseClientState::Disconnect( bShowMainMenu );

	if ( m_bSplitScreenUser )
		return;

	// clear any map hacks
	{
		static ConVarRef map_wants_save_disable( "map_wants_save_disable" );
		map_wants_save_disable.SetValue( 0 );
	}

	// stop any demo activities
	demoplayer->StopPlayback();
	demorecorder->StopRecording();

#if defined( REPLAY_ENABLED )
	extern IReplayHistoryManager *g_pClientReplayHistoryManager;
	g_pClientReplayHistoryManager->StopDownloads();
#endif

	S_StopAllSounds( true );
	
	R_DecalTermAll();

	if ( m_nMaxClients > 1 )
	{
		if ( EngineVGui()->IsConsoleVisible() == false )
		{
			// start progress bar immediately for multiplayer level transitions
			EngineVGui()->EnabledProgressBarForNextLoad();
		}
	}

	CL_ClearState();

	// End any in-progress downloads
	CL_HTTPStop_f();

	// stop loading progress bar 
	if ( bShowMainMenu )
	{
		SCR_EndLoadingPlaque();
	}

	// notify game ui dll of out-of-in-game status
	EngineVGui()->NotifyOfServerDisconnect();

	HostState_OnClientDisconnected();

	// if we played a demo from the startdemos list, play next one
	if ( GetBaseLocalClient().demonum != -1 )
	{
		CL_NextDemo();
	}
}


static bool s_bClientWaitingForHltvReplayTick = false;

bool CClientState::NETMsg_Tick( const CNETMsg_Tick& msg )
{
	if ( g_ClientDLL )
	{
		// Notify the client that we are just about to tick over.
		g_ClientDLL->OnTickPre( host_tickcount );
	}

	int tick = msg.tick();

	{
		if ( m_nHltvReplayDelay )
		{
			if ( !msg.hltv_replay_flags() )
				DevMsg( "%d. Msg_Tick %d cl:delayed sv:real-time\n", GetClientTickCount(), tick );
		}
		else
		{
			if ( msg.hltv_replay_flags() )
				DevMsg( "%d. Msg_Tick %d cl:real-time sv:replay\n", GetClientTickCount(), tick );
		}
	}

	if ( m_nHltvReplayDelay && !msg.hltv_replay_flags() )
	{
		// client is in hltv replay state, but server doesn't remember it - maybe there was a connection issue or something. 
		DevMsg( "Inconsistent Client Replay state: tick without replay during replay. Force Stop Replay.\n" );
		s_bClientWaitingForHltvReplayTick = true;
		StopHltvReplay();
	}

	m_NetChannel->SetRemoteFramerate(
		CNETMsg_Tick_t::FrametimeToFloat( msg.host_computationtime() ),
		CNETMsg_Tick_t::FrametimeToFloat( msg.host_computationtime_std_deviation() ),
		CNETMsg_Tick_t::FrametimeToFloat( msg.host_framestarttime_std_deviation() ) );

	m_ClockDriftMgr.SetServerTick( tick );

	// Remember this for GetLastTimeStamp().
	m_flLastServerTickTime = tick * host_state.interval_per_tick;

	// Use the server tick while reading network data (used for interpolation samples, etc).
	g_ClientGlobalVariables.tickcount = tick;	
	g_ClientGlobalVariables.curtime = tick * host_state.interval_per_tick;
	g_ClientGlobalVariables.frametime = (tick - oldtickcount) * host_state.interval_per_tick;	// We used to call GetFrameTime() here, but 'insimulation' is always
	// true so we have this code right in here to keep it simple.

	if ( s_bClientWaitingForHltvReplayTick && g_ClientDLL )
	{
		if ( m_nHltvReplayDelay )
		{
			// we're starting replay. Clean up decals from the future
			R_DecalTermNew( host_state.worldmodel->brush.pShared, tick );
		}
		g_ClientDLL->OnHltvReplayTick();
		s_bClientWaitingForHltvReplayTick = false;
	}

	return true;
}

bool CClientState::NETMsg_StringCmd( const CNETMsg_StringCmd& msg )
{
	// Even though this is just forwarding to the base class, do not remove this function.
	// There are multiple implementations of CClientState and the one in cl_null.cpp
	// stubs this function out as a simple 'return true;'.
	return BaseClass::NETMsg_StringCmd( msg );
}

bool CClientState::SVCMsg_ServerInfo( const CSVCMsg_ServerInfo& msg )
{
	// Reset client state
	CL_ClearState();

	if ( !CBaseClientState::SVCMsg_ServerInfo( msg ) )
	{
		Disconnect(true);
		return false;
	}

	if ( demoplayer->IsPlayingBack() )
	{
		// Because a server doesn't run during
		// demoplayback, but the decal system relies on this...
		m_nServerCount = gHostSpawnCount;    
	}
	else
	{
		// tell demo recorder that new map is loaded and we are receiving
		// it's signon data (will be written into extra demo header file)
		demorecorder->SetSignonState( SIGNONSTATE_NEW );
	}

	// is server a HLTV proxy ?
	ishltv = msg.is_hltv();

#if defined( REPLAY_ENABLED )
	// is server a replay proxy ?
	isreplay = msg.is_replay();
#endif

	// The CRC of the server map must match the CRC of the client map. or else
	//  the client is probably cheating.
	serverCRC = msg.map_crc();
	// The client side DLL CRC check.
	serverClientSideDllCRC = msg.client_crc();

	g_ClientGlobalVariables.maxClients = m_nMaxClients;
	g_ClientGlobalVariables.network_protocol = msg.protocol();

#ifdef SHARED_NET_STRING_TABLES
	// use same instance of StringTableContainer as the server does
	m_StringTableContainer = networkStringTableContainerServer;
	CL_HookClientStringTables();
#else
	// use own instance of StringTableContainer
	m_StringTableContainer = networkStringTableContainerClient;
#endif

	CL_ReallocateDynamicData( m_nMaxClients );

	if ( sv.IsPaused() )
	{
		if ( msg.tick_interval() != host_state.interval_per_tick )
		{
			Host_Error( "Expecting interval_per_tick %f, got %f\n", 
				host_state.interval_per_tick, msg.tick_interval() );
			return false;
		}
	}
	else
	{
		host_state.interval_per_tick = msg.tick_interval();
	}

	// Re-init hud video, especially if we changed game directories
	ClientDLL_HudVidInit();

	// Don't verify the map and player .mdl crc's until after any missing resources have
	// been downloaded.  This will still occur before requesting the rest of the signon.


	gHostSpawnCount = m_nServerCount;

	videomode->MarkClientViewRectDirty();	// leave intermission full screen
	return true;
}

bool CClientState::SVCMsg_ClassInfo( const CSVCMsg_ClassInfo& msg )
{
	if ( msg.create_on_client() )
	{
		if ( !demoplayer->IsPlayingBack() )
		{
			// Create all of the send tables locally
			DataTable_CreateClientTablesFromServerTables();

			// Now create all of the server classes locally, too
			DataTable_CreateClientClassInfosFromServerClasses( this );

			// store the current data tables in demo file to make sure
			// they are the same during playback 
			demorecorder->RecordServerClasses( serverGameDLL->GetAllServerClasses() );
		}

		LinkClasses();	// link server and client classes
	}
	else
	{
		CBaseClientState::SVCMsg_ClassInfo( msg );
	}
	
	bool bAllowMismatches = ( g_pClientDemoPlayer && g_pClientDemoPlayer->IsPlayingBack() );
	if ( !RecvTable_CreateDecoders( serverGameDLL->GetStandardSendProxies(), bAllowMismatches ) ) // create receive table decoders
	{
		Host_EndGame( true, "CL_ParseClassInfo_EndClasses: CreateDecoders failed.\n" );
		return false;
	}

	if ( !demoplayer->IsPlayingBack() )
	{
		CLocalNetworkBackdoor::InitFastCopy();
	}

	return true;
}

bool CClientState::SVCMsg_SetPause( const CSVCMsg_SetPause& msg )
{
	CBaseClientState::SVCMsg_SetPause( msg );

	return true;
}

bool CClientState::SVCMsg_VoiceInit( const CSVCMsg_VoiceInit& msg )
{
#if !defined( NO_VOICE )
	if( msg.codec().size() == 0 )
	{
		Voice_Deinit();
	}
	else
	{
#define SPEEX_QUALITY 4
		Voice_Init( msg.codec().c_str(), msg.has_version() ? msg.version() : SPEEX_QUALITY );
	}
#endif
	return true;
}

ConVar voice_debugfeedback( "voice_debugfeedback", "0" );

bool CClientState::SVCMsg_VoiceData( const CSVCMsg_VoiceData &msg )
{
#if defined ( _GAMECONSOLE )

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		if ( msg.client() == GetLocalClient( i ).m_nPlayerSlot )
			return true;
	}

	ConVarRef voice_verbose( "voice_verbose" );
	if ( voice_verbose.GetBool() )
	{
		Msg( "* CClientState::ProcessVoiceData: playing SVC_VoiceData from %s with %u bytes\n", msg.GetNetChannel()->GetAddress(), msg.voice_data_size() );
	}

	Audio_GetXVoice()->PlayIncomingVoiceData( msg.xuid(), (const byte*)&msg.voice_data[0], msg.voice_data_size(), bAudible );
	if ( voice_debugfeedback.GetBool() )
	{
		Msg( "%f Received voice from: %d [%d bytes]\n", realtime, msg->m_nFromClient + 1, dwLength );
	}
	return true;
#endif

#if !defined( NO_VOICE )
	if ( voice_debugfeedback.GetBool() )
	{
		Msg( "Received voice from: %d\n", msg.client() + 1 );
	}

	int iEntity = msg.client() + 1;
	if ( iEntity == (m_nPlayerSlot + 1) )
	{ 
		Voice_LocalPlayerTalkingAck( m_nSplitScreenSlot );
	}

	player_info_t playerinfo;
	engineClient->GetPlayerInfo( iEntity, &playerinfo );

	CSteamID voicePlayer( playerinfo.xuid );

	if ( Q_strlen( cl_voice_filter.GetString() ) > 0 && Q_strstr( playerinfo.name, cl_voice_filter.GetString() ) == NULL )
		return true;

	// Data length can be zero when the server is just acking a client's voice data.
	if ( msg.voice_data().size() == 0 )
		return true;

	if ( !Voice_SystemEnabled() )
		return true;

	bool bIsCaster = msg.has_caster() && msg.caster();

	// if this voice data is for a caster that is not enabled, then bail
	if ( bIsCaster && !Voice_CasterEnabled( voicePlayer.GetAccountID() ) )
		return true;

	// if voice is enabled or it's a caster and caster voice is enabled.
	if ( Voice_Enabled() || ( Voice_CasterEnabled( voicePlayer.GetAccountID() ) && bIsCaster ) )
	{
		// Have we already initialized the channels for this guy?
		int nChannel = Voice_GetChannel( iEntity );
		if ( nChannel == VOICE_CHANNEL_ERROR )
		{
			// Create a channel in the voice engine and a channel in the sound engine for this guy.
			float flBufferTime = GetBaseLocalClient().ishltv ? cl_voice_hltv_buffer_time.GetFloat() : cl_voice_buffer_time.GetFloat();
			nChannel = Voice_AssignChannel( iEntity, msg.proximity(), bIsCaster, flBufferTime );
			if ( nChannel == VOICE_CHANNEL_ERROR )
			{
				// If they used -nosound, then it's not a problem.
				if ( S_IsInitted() )
				{
					ConDMsg( "ProcessVoiceData: Voice_AssignChannel failed for client %d!\n", iEntity - 1 );
				}
				return true;
			}
		}

		// Give the voice engine the data (it in turn gives it to the mixer for the sound engine).
		Voice_AddIncomingData( nChannel, 
							   &msg.voice_data()[0], 
							   msg.voice_data().size(),
							   msg.has_section_number() ? msg.section_number() : 0,
							   msg.has_sequence_bytes() ? msg.sequence_bytes() : 0,
							   msg.has_uncompressed_sample_offset() ? msg.uncompressed_sample_offset() : 0,
							   ( msg.has_format() && ( msg.format() == VOICEDATA_FORMAT_STEAM ) ) ? VoiceFormat_Steam : VoiceFormat_Engine );
	}

#endif
	return true;
};

bool CClientState::SVCMsg_Prefetch( const CSVCMsg_Prefetch& msg )
{
	char const *soundname = GetBaseLocalClient().GetSoundName( msg.sound_index() );
	if ( soundname && soundname [ 0 ] )
	{
		EngineSoundClient()->PrefetchSound( soundname );
	}
	return true;
}

bool CClientState::SVCMsg_Sounds( const CSVCMsg_Sounds& msg )
{
	SoundInfo_t defaultSound;

	SoundInfo_t *pDeltaSound = &defaultSound;
	SoundInfo_t sound;

	int nNumSounds = msg.sounds_size();
	for ( int i=0; i<nNumSounds; i++ )
	{
		const CSVCMsg_Sounds::sounddata_t& SoundData = msg.sounds( i );

		sound.ReadDelta( pDeltaSound, SoundData );

		pDeltaSound = &sound;	// copy delta values

		if ( msg.reliable_sound() )
		{
			// client is incrementing the reliable sequence numbers itself
			m_nSoundSequence = ( m_nSoundSequence + 1 ) & SOUND_SEQNUMBER_MASK;
			Assert ( sound.nSequenceNumber == 0 );
			sound.nSequenceNumber = m_nSoundSequence;
		}

		// Add all received sounds to sorted queue (sounds may arrive in multiple messages), 
		//  will be processed after all packets have been completely parsed
		CL_AddSound( sound );
	}

	// check given length against read bits
	return true;
}

bool CClientState::SVCMsg_FixAngle( const CSVCMsg_FixAngle &msg )
{
	const CMsgQAngle& angle = msg.angle();
	QAngle qangle( angle.x(), angle.y(), angle.z() );

	for (int i=0 ; i<3 ; i++)
	{
		// Clamp between -180 and 180
		if (qangle[i]>180)
		{
			qangle[i] -= 360;
		}
	}

	if ( msg.relative() )
	{
		// Update running counter
		addangletotal += qangle[YAW];

		AddAngle a;
		a.total = addangletotal;
		a.starttime = m_flLastServerTickTime;

		addangle.AddToTail( a );
	}
	else
	{

		viewangles = qangle;
	}

	return true;
}

bool CClientState::SVCMsg_CrosshairAngle( const CSVCMsg_CrosshairAngle& msg )
{
	const CMsgQAngle& angle = msg.angle();
	const QAngle qangle( angle.x(), angle.y(), angle.z() );

	g_ClientDLL->SetCrosshairAngle( qangle );

	return true;
}

bool CClientState::SVCMsg_BSPDecal( const CSVCMsg_BSPDecal& msg )
{
	model_t	* model;

	if ( msg.entity_index() )
	{
		model = GetModel( msg.model_index() );
	}
	else
	{
		model = host_state.worldmodel;
		if ( !model )
		{
			Warning( "ProcessBSPDecal:  Trying to project on world before host_state.worldmodel is set!!!\n" );
		}
	}

	if ( model == NULL )
	{
		IMaterial *mat = Draw_DecalMaterial( msg.decal_texture_index() );
		char const *matname = "???";
		if ( mat )
		{
			matname = mat->GetName();
		}

		Warning( "Warning! Static BSP decal (%s), on NULL model index %i for entity index %i.\n", 
			matname,
			msg.model_index(), 
			msg.entity_index() );
		return true;
	}

	if (r_decals.GetInt())
	{
		const CMsgVector& pos = msg.pos();
		const Vector vecPos( pos.x(), pos.y(), pos.z() );

		g_pEfx->DecalShoot(
			msg.decal_texture_index(),
			msg.entity_index(),
			model,
			vec3_origin,
			vec3_angle,
			vecPos,
			NULL,
			msg.low_priority() ? 0 : FDECAL_PERMANENT );
	}

	return true;
}

bool CClientState::SVCMsg_GameEvent(const CSVCMsg_GameEvent& msg)
{
	IGameEvent *event = g_GameEventManager.UnserializeEvent( msg );

	if ( !event )
	{
		DevMsg("CClientState::ProcessGameEvent: UnserializeKeyValue failed.\n" );
		return true;
	}

	if ( msg.passthrough() == 1 )
	{
		// this should only come to clients while they have replay in progress
		event->SetBool( "realtime_passthrough", true );
	}

	g_GameEventManager.FireEventClientSide( event );

	return true;
}

bool CClientState::SVCMsg_UserMessage( const CSVCMsg_UserMessage &msg)
{
	if ( !g_ClientDLL->DispatchUserMessage( msg.msg_type(), msg.passthrough(), msg.msg_data().size(), &msg.msg_data()[0] ) )
	{
		ConMsg( "Couldn't dispatch user message (%i)\n", msg.msg_type() );
		return false;
	}

	return true;
}


bool CClientState::SVCMsg_EntityMsg( const CSVCMsg_EntityMsg& msg )
{
	// Look up entity
	IClientNetworkable *entity = entitylist->GetClientNetworkable( msg.ent_index() );

	if ( !entity )
	{
		int idx = queuedmessage.AddToTail();

		CQueuedEntityMessage *pMessage = &queuedmessage[idx];
		pMessage->m_msg.CopyFrom( msg );	
		return true;
	}

	// route to entity 
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	bf_read entMsg;
	entMsg.StartReading( msg.ent_data().data(), msg.ent_data().size() );
	entity->ReceiveMessage( msg.class_id(), entMsg );

	return true;
}


bool CClientState::SVCMsg_PacketEntities( const CSVCMsg_PacketEntities &msg )
{
	CL_PreprocessEntities(); // setup client prediction

	if ( !msg.is_delta() )
	{
		// Delta too old or is initial message
		// we can start recording now that we've received an uncompressed packet
		demorecorder->SetSignonState( SIGNONSTATE_FULL );

		// Tell prediction that we're recreating entities due to an uncompressed packet arriving
		if ( g_pClientSidePrediction  )
		{
			g_pClientSidePrediction->OnReceivedUncompressedPacket();
		}
	}
	else
	{
		if ( m_nDeltaTick == -1  )
		{
			// we requested a full update but still got a delta compressed packet. ignore it.
			return true;
		}
	}
	
	TRACE_PACKET(( "CL Receive (%d <-%d)\n", m_nCurrentSequence, msg.delta_from() ));
	TRACE_PACKET(( "CL Num Ents (%d)\n", msg.updated_entries() ));

	if ( g_pLocalNetworkBackdoor )
	{
		if ( m_nSignonState == SIGNONSTATE_SPAWN  )
		{	
			// We are done with signon sequence.
			SetSignonState( SIGNONSTATE_FULL, m_nServerCount, NULL );
		}

		// ignore message, all entities are transmitted using fast local memcopy routines
		m_nDeltaTick = GetServerTickCount();
		return true;
	}

	if ( !CL_ProcessPacketEntities( msg ) )
		return false;

	return CBaseClientState::SVCMsg_PacketEntities( msg );
}


bool CClientState::SVCMsg_TempEntities( const CSVCMsg_TempEntities &msg )
{
	bool bReliable = msg.reliable();

	float fire_time = GetBaseLocalClient().GetTime();

	// delay firing temp ents by cl_interp in multiplayer or demoplayback
	if ( GetBaseLocalClient().m_nMaxClients > 1 || demoplayer->IsPlayingBack() )
	{
		float flInterpAmount = GetClientInterpAmount();
		fire_time += flInterpAmount;
	}

	int numEntries = msg.num_entries();
	if ( numEntries == 0 )
	{
		bReliable = true;
		numEntries = 1;
	}

	int flags = bReliable ? FEV_RELIABLE : 0;

	// Don't actually queue unreliable events if playing a demo and skipping ahead
	if ( !bReliable && demoplayer->IsSkipping() )
	{
		return true;
	}

	bf_read buffer( &msg.entity_data()[0], msg.entity_data().size() );

	int classID = -1;
	void *from = NULL;
	C_ServerClassInfo *pServerClass = NULL;
	ClientClass *pClientClass = NULL;
	ALIGN4 unsigned char data[CEventInfo::MAX_EVENT_DATA] ALIGN4_POST;
	bf_write toBuf( data, sizeof(data) );
	CEventInfo *ei = NULL;

	CUtlFixedLinkedList< CEventInfo > &eventList = GetBaseLocalClient().events;

	class CAutoCleanupHandle
	{
	public:
		explicit CAutoCleanupHandle( SerializedEntityHandle_t handle ) : m_Handle( handle )
		{
		}
		~CAutoCleanupHandle()
		{
			g_pSerializedEntities->ReleaseSerializedEntity( m_Handle );
		}
	private:
		SerializedEntityHandle_t m_Handle;
	};

	SerializedEntityHandle_t incoming = g_pSerializedEntities->AllocateSerializedEntity(__FILE__, __LINE__);
	CAutoCleanupHandle cleanup( incoming );

	for (int i = 0; i < numEntries; i++ )
	{
		float delay = 0.0f;

		if ( buffer.ReadOneBit() )
		{
			delay = (float)buffer.ReadSBitLong( 8 ) / 100.0f;
		}

		toBuf.Reset();

		SerializedEntityHandle_t merged = g_pSerializedEntities->AllocateSerializedEntity(__FILE__, __LINE__);

		if ( buffer.ReadOneBit() )
		{
			from = NULL; // full update

			classID = buffer.ReadUBitLong( m_nServerClassBits ); // classID 

			// Look up the client class, etc.

			// Match the server classes to the client classes.
			pServerClass = m_pServerClasses ? &m_pServerClasses[ classID - 1 ] : NULL;

			if ( !pServerClass )
			{
				DevMsg("CL_QueueEvent: missing server class info for %i.\n", classID - 1 );
				return false;
			}

			// See if the client .dll has a handler for this class
			pClientClass = FindClientClass( pServerClass->m_ClassName );

			if ( !pClientClass || !pClientClass->m_pRecvTable )
			{
				DevMsg("CL_QueueEvent: missing client receive table for %s.\n", pServerClass->m_ClassName );
				return false;
			}

			// Incoming data
			RecvTable_ReadFieldList( pClientClass->m_pRecvTable, buffer, incoming, -1, false );
			RecvTable_MergeDeltas( pClientClass->m_pRecvTable, SERIALIZED_ENTITY_HANDLE_INVALID, incoming, merged );
		}
		else
		{
			Assert( eventList.Tail() != eventList.InvalidIndex() );

			CEventInfo *previous = &eventList[ eventList.Tail() ];

			RecvTable_ReadFieldList( pClientClass->m_pRecvTable, buffer, incoming, -1, false );
			RecvTable_MergeDeltas( pClientClass->m_pRecvTable, previous->m_Packed, incoming, merged );
		}

		// Add a slot
		ei = &GetBaseLocalClient().events[ GetBaseLocalClient().events.AddToTail() ];

		Assert( ei );

		ei->classID			= classID;
		ei->fire_delay		= fire_time + delay;
		ei->flags			= flags;
		ei->pClientClass	= pClientClass;
		ei->m_Packed		= merged;
	}

	return true;
}

bool CClientState::SVCMsg_PaintmapData( const CSVCMsg_PaintmapData& msg )
{
	int nDword = ( Bits2Bytes( msg.paintmap().size() ) + 3 ) / 4;
	CUtlVector< uint32 > data;
	data.SetCount( nDword );

	bf_read dataIn;
	dataIn.ReadBits( const_cast<char*>(msg.paintmap().data()), msg.paintmap().size() );
	
	//handle endian issue between platforms
	CByteswap swap;
	swap.ActivateByteSwapping( !CByteswap::IsMachineBigEndian() );
	swap.SwapBufferToTargetEndian( data.Base(), data.Base(), nDword );

	if ( data.Count() > 0 )
	{
		g_PaintManager.LoadPaintmapDataRLE( data );
	}

	return true;
}


bool CClientState::SVCMsg_HltvReplay( const CSVCMsg_HltvReplay &msg )
{
	VPROF( "HltvReplayStart" );
	DevMsg( "%d. Msg_HltvReplay %s->%s\n", GetClientTickCount(), m_nHltvReplayDelay ? "replay" : "real-time", msg.delay() ? "replay" : "real-time" );
	int nWasDelay = m_nHltvReplayDelay;
	m_nHltvReplayDelay = msg.delay();
	m_nHltvReplayStopAt = msg.replay_stop_at();
	m_nHltvReplayStartAt = msg.replay_start_at();
	float flRate = msg.replay_slowdown_rate();
	if ( flRate > 0 )
	{
		m_nHltvReplaySlowdownBeginAt = msg.replay_slowdown_begin();
		m_nHltvReplaySlowdownEndAt = msg.replay_slowdown_end();
		m_flHltvReplaySlowdownRate = flRate;
	}
	else
	{
		m_nHltvReplaySlowdownBeginAt = 0;
		m_nHltvReplaySlowdownEndAt = 0;
		m_flHltvReplaySlowdownRate = 1.0f;
	}
	if ( g_ClientDLL )
	{
		g_ClientDLL->OnHltvReplay( msg );
		s_bClientWaitingForHltvReplayTick = true; // waiting for a tick message after the hltv replay either starts or stops: it will carry the new tick to time replay fades etc.
	}
	if ( !m_nHltvReplayDelay != !nWasDelay )
	{
		// clean up frame history, because we're about to start new history.
		// this is only useful when we're switching the timeline between HLTV and non-HLTV streams: 
		// the same frame encodes different information in these two modes, and we can't decode delta frames in one timeline based on the full frames in another.
		// It's tolerable from networking perspective because every time we switch the mode, the server forces a full frame update immediately after this message.
		// It's NOT VALID to delete client frames when we don't switch the modes, because the server WILL NOT send a full frame update
		DeleteClientFrames( -1 ); 
	}
	return true;
}


#endif // swds
