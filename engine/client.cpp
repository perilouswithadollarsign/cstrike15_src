//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "client_pch.h"
#include "networkstringtabledefs.h"
#include <checksum_md5.h>
#include <iregistry.h>
#include "userid.h"
#include "pure_server.h"
#include "netmessages.h"
#include "cl_demo.h"
#include "host_state.h"
#include "host.h"
#include "gl_matsysiface.h"
#include "vgui_baseui_interface.h"
#include "tier0/icommandline.h"
#include <proto_oob.h>
#include "checksum_engine.h"
#include "filesystem_engine.h"
#include "logofile_shared.h"
#include "sound.h"
#include "decal.h"
#include "networkstringtableclient.h"
#include "dt_send_eng.h"
#include "ents_shared.h"
#include "cl_ents_parse.h"
#include "cl_entityreport.h"
#include "MapReslistGenerator.h"
#include "DownloadListGenerator.h"
#include "GameEventManager.h"
#include "vgui_baseui_interface.h"
#include "clockdriftmgr.h"
#include "snd_audio_source.h"
#include "vgui_controls/Controls.h"
#include "vgui/ILocalize.h"
#include "download.h"
#include "checksum_engine.h"
#include "ModelInfo.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/materialsystem_config.h"
#include "tier1/fmtstr.h"
#include "cl_steamauth.h"
#include "matchmaking/imatchframework.h"
#include "audio/private/snd_sfx.h"
#include "tier0/platform.h"
#include "tier0/systeminformation.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar cl_timeout( "cl_timeout", "30", FCVAR_ARCHIVE, "After this many seconds without receiving a packet from the server, the client will disconnect itself"
#ifndef _DEBUG
	, true, 4, true, 30
#endif
	);
static ConVar cl_forcepreload( "cl_forcepreload", "0", FCVAR_ARCHIVE, "Whether we should force preloading.");
static ConVar cl_downloadfilter( "cl_downloadfilter", "all", FCVAR_ARCHIVE, "Determines which files can be downloaded from the server (all, none, nosounds)" );
static ConVar cl_download_demoplayer( "cl_download_demoplayer", "1", FCVAR_RELEASE, "Determines whether downloads of external resources are allowed during demo playback (0:no,1:workshop,2:all)" );

ConVar cl_debug_ugc_downloads( "cl_debug_ugc_downloads", "0", FCVAR_RELEASE );

extern ConVar sv_downloadurl;
extern ConVar sv_consistency;
extern ConVar cl_hideserverip;

extern bool g_bServerGameDLLGreaterThanV5;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CClientState::CClientState()
{
	m_bMarkedCRCsUnverified = false;
	demonum = -1;
	m_tickRemainder = 0;
	m_frameTime = 0;
	m_pAreaBits = NULL;
	m_hWaitForResourcesHandle = NULL;
	m_bUpdateSteamResources = false;
	m_bShownSteamResourceUpdateProgress = false;
	m_pPureServerWhitelist = NULL;
	m_bCheckCRCsWithServer = false;
	m_flLastCRCBatchTime = 0;
	m_nFriendsID = 0;
	m_FriendsName[ 0 ] = 0;
	m_flLastServerTickTime = -1.0f;
	lastoutgoingcommand = 0;
	chokedcommands = 0;
	last_command_ack = 0;
	last_server_tick = 0;
	command_ack = 0;
	m_nSoundSequence = 0;
	serverCRC = 0;
	serverClientSideDllCRC = 0;
	viewangles.Init();
	Q_memset( m_chAreaBits, 0, sizeof( m_chAreaBits ) );
	Q_memset( m_chAreaPortalBits, 0, sizeof( m_chAreaPortalBits ) );
	m_bAreaBitsValid = false;
	addangletotal = 0.0f;
	prevaddangletotal = 0.0f;
	cdtrack = 0;
	Q_memset( m_FriendsName, 0, sizeof( m_FriendsName ) );
	m_pModelPrecacheTable = NULL;
	m_pDynamicModelTable = NULL;
	m_pGenericPrecacheTable = NULL;
	m_pSoundPrecacheTable = NULL;
	m_pDecalPrecacheTable = NULL;
	m_pInstanceBaselineTable = NULL;
	m_pLightStyleTable = NULL;
	m_pUserInfoTable = NULL;
	m_pServerStartupTable = NULL;
	m_pDownloadableFileTable = NULL;
	m_bDownloadResources = false;
	m_bDownloadingUGCMap = false;
	insimulation = false;
	oldtickcount = 0;
	ishltv = false;
#if defined( REPLAY_ENABLED )
	isreplay = false;
#endif
	ResetHltvReplayState();
}

void CClientState::ResetHltvReplayState()
{
	m_nHltvReplayDelay = 0;
	m_nHltvReplayStopAt = 0;
	m_nHltvReplayStartAt = 0;
	m_nHltvReplaySlowdownBeginAt = 0;
	m_nHltvReplaySlowdownEndAt = 0;
	m_flHltvReplaySlowdownRate = 1.0f;
}

CClientState::~CClientState()
{
	if ( m_pPureServerWhitelist )
		m_pPureServerWhitelist->Release();
}

// HL1 CD Key
#define GUID_LEN 13

/*
=======================
CL_GetCDKeyHash()

Connections will now use a hashed cd key value
A LAN server will know not to allows more then xxx users with the same CD Key
=======================
*/
const char *CClientState::GetCDKeyHash( void )
{
	if ( IsPC() )
	{
		char szKeyBuffer[256]; // Keys are about 13 chars long.	
		static char szHashedKeyBuffer[64];
		int nKeyLength;
		bool bDedicated = false;

		MD5Context_t ctx;
		unsigned char digest[16]; // The MD5 Hash

		nKeyLength = Q_snprintf( szKeyBuffer, sizeof( szKeyBuffer ), "%s", registry->ReadString( "key", "" ) );

		if (bDedicated)
		{
			ConMsg("Key has no meaning on dedicated server...\n");
			return "";
		}

		if ( nKeyLength == 0 )
		{
			nKeyLength = 13;
			Q_strncpy( szKeyBuffer, "1234567890123", sizeof( szKeyBuffer ) );
			Assert( Q_strlen( szKeyBuffer ) == nKeyLength );

			DevMsg( "Missing CD Key from registry, inserting blank key\n" );

			registry->WriteString( "key", szKeyBuffer );
		}

		if (nKeyLength <= 0 ||
			nKeyLength >= 256 )
		{
			ConMsg("Bogus key length on CD Key...\n");
			return "";
		}

		// Now get the md5 hash of the key
		memset( &ctx, 0, sizeof( ctx ) );
		memset( digest, 0, sizeof( digest ) );
		
		MD5Init(&ctx);
		MD5Update(&ctx, (unsigned char*)szKeyBuffer, nKeyLength);
		MD5Final(digest, &ctx);
		Q_strncpy ( szHashedKeyBuffer, MD5_Print ( digest, sizeof( digest ) ), sizeof( szHashedKeyBuffer ) );
		return szHashedKeyBuffer;
	}

	return "12345678901234567890123456789012";
}

void CClientState::SendClientInfo( void )
{
	CCLCMsg_ClientInfo_t info;

	info.set_send_table_crc( SendTable_GetCRC() );
	info.set_server_count( m_nServerCount );
	info.set_is_hltv( false );
#if defined( REPLAY_ENABLED )
	info.set_is_replay( false );
#endif

#if !defined( NO_STEAM )
	info.set_friends_id( Steam3Client().SteamUser() ? Steam3Client().SteamUser()->GetSteamID().GetAccountID() : 0 );
#else
	info.set_friends_id( 0 );
#endif
	info.set_friends_name( m_FriendsName );

	CheckOwnCustomFiles(); // load & verfiy custom player files

	for ( int i=0; i< MAX_CUSTOM_FILES; i++ )
	{
		info.add_custom_files( m_nCustomFiles[i].crc );
	}
	
	m_NetChannel->SendNetMsg( info );
}

void CClientState::SendLoadingProgress( int nProgress )
{
	if ( !m_NetChannel || nProgress <= m_nLastProgressPercent )
	{
		return;
	}

	CCLCMsg_LoadingProgress_t info;
	info.set_progress( nProgress );
	m_nLastProgressPercent = nProgress;

	m_NetChannel->SendNetMsg( info );
}

void CClientState::SendServerCmdKeyValues( KeyValues *pKeyValues )
{
	if ( !pKeyValues )
		return;

	// Ensure keyvalues are deleted per contract obligations
	KeyValues::AutoDelete autodelete_pKeyValues( pKeyValues );
	
	if ( !m_NetChannel )
		return;

	CCLCMsg_CmdKeyValues_t msg;
	CmdKeyValuesHelper::CLCMsg_SetKeyValues( msg, pKeyValues );
	
	m_NetChannel->SendNetMsg( msg );
}

bool CClientState::SendNetMsg( INetMessage &msg, bool bForceReliable, bool bVoice )
{
	if ( m_NetChannel )
		return m_NetChannel->SendNetMsg( msg, bForceReliable, bVoice );
	else
		return false;
}


extern IVEngineClient *engineClient;

//-----------------------------------------------------------------------------
// Purpose: A svc_signonnum has been received, perform a client side setup
// Output : void CL_SignonReply
//-----------------------------------------------------------------------------
bool CClientState::SetSignonState ( int state, int count, const CNETMsg_SignonState *msg )
{
	int nOldSignonState = m_nSignonState;

	ResetHltvReplayState();

	if ( !CBaseClientState::SetSignonState( state, count, msg ) )
	{
		CL_Retry();
		return false;
	}

	// ConDMsg ("Signon state: %i\n", state );

	COM_TimestampedLog( "CClientState::SetSignonState: start %i", state );

	switch ( m_nSignonState )
	{
		case SIGNONSTATE_CHALLENGE	:	
			m_bMarkedCRCsUnverified = false;	// Remember that we just connected to a new server so it'll 
												// reverify any necessary file CRCs on this server.
			EngineVGui()->UpdateProgressBar(PROGRESS_SIGNONCHALLENGE);
			break;

		case SIGNONSTATE_CONNECTED :	
			{
				EngineVGui()->UpdateProgressBar(PROGRESS_SIGNONCONNECTED);
				
				// make sure it's turned off when connecting
				EngineVGui()->HideDebugSystem();

				SCR_BeginLoadingPlaque ();
				// Clear channel and stuff
				m_NetChannel->Clear();

				// allow longer timeout
				m_NetChannel->SetTimeout( SIGNON_TIME_OUT );
				m_NetChannel->SetMaxBufferSize( true, NET_MAX_PAYLOAD );
				
				// set user settings (rate etc)
				CNETMsg_SetConVar_t convars;
				Host_BuildUserInfoUpdateMessage( m_nSplitScreenSlot, convars.mutable_convars(), false );
				m_NetChannel->SendNetMsg( convars );
			}
			break;

		case SIGNONSTATE_NEW :	
			{
				EngineVGui()->UpdateProgressBar(PROGRESS_SIGNONNEW);

				if ( cl_download_demoplayer.GetBool() || !demoplayer->IsPlayingBack() )
				{
					// When playing back a demo we need to suspend packet reading here
					if ( demoplayer->IsPlayingBack() )
					{
						demoplayer->SetPacketReadSuspended( true );
					}

					// start making sure we have all the specified resources
					StartUpdatingSteamResources();
				}
				else
				{
					// during demo playback dont try to download resource
					FinishSignonState_New();
				}

				// don't tell the server yet that we've entered this state
				COM_TimestampedLog( "CClientState::SetSignonState: end %i", state );
				return true;
			}
			break;

		case SIGNONSTATE_PRESPAWN	:
			EngineVGui()->UpdateProgressBar(PROGRESS_SENDSIGNONDATA);
			m_nSoundSequence = 1;	// reset sound sequence number after receiving signon sounds
			break;
		
		case SIGNONSTATE_SPAWN :
			{
				extern float NET_GetFakeLag();
				Assert( g_ClientDLL );

				EngineVGui()->UpdateProgressBar(PROGRESS_SIGNONSPAWN);

				// Tell client .dll about the transition
				char mapname[256];
				CL_SetupMapName( modelloader->GetName( host_state.worldmodel ), mapname, sizeof( mapname ) );

				COM_TimestampedLog( "LevelInitPreEntity: start %i", state );
				// enable prediction if the server isn't local or the user is requesting fake lag
				g_ClientGlobalVariables.m_bRemoteClient = !Host_IsLocalServer() || (NET_GetFakeLag() != 0.0f);
				g_ClientDLL->LevelInitPreEntity(mapname);
				COM_TimestampedLog( "LevelInitPreEntity: end %i", state );

				audiosourcecache->LevelInit( mapname );

				// stop recording demo header
				demorecorder->SetSignonState( SIGNONSTATE_SPAWN );
			}
			break;

		case SIGNONSTATE_FULL:
			{
				CL_FullyConnected();
				if ( !m_NetChannel )
					return false; // disconnected during connection
				m_NetChannel->SetTimeout( cl_timeout.GetFloat() );
				m_NetChannel->SetMaxBufferSize( true, NET_MAX_DATAGRAM_PAYLOAD );

				HostState_OnClientConnected();
				
				// If we came through a level transition with splitscreen guys, then we need to force their 
				//  signon state to be SIGNONSTATE_FULL, too
				FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
				{
					CBaseClientState &cl = GetLocalClient( hh );
					if ( &cl == this )
						continue;
					cl.m_nSignonState = SIGNONSTATE_FULL;
				}
			}
			break;

		case SIGNONSTATE_CHANGELEVEL:
			m_NetChannel->SetTimeout( SIGNON_TIME_OUT );  // allow 5 minutes timeout
			m_nLastProgressPercent = -1;
			if ( m_nMaxClients > 1 )
			{
				// start progress bar immediately for multiplayer level transitions
				EngineVGui()->EnabledProgressBarForNextLoad();
			}
			SCR_BeginLoadingPlaque( msg->map_name().c_str() );
			if ( m_nMaxClients > 1 )
			{
				EngineVGui()->UpdateProgressBar(PROGRESS_CHANGELEVEL);
			}
			break;
	}

	COM_TimestampedLog( "CClientState::SetSignonState: end %i", state );

	KeyValues *pEvent = new KeyValues( "OnEngineClientSignonStateChange" );
	pEvent->SetInt( "slot", m_nSplitScreenSlot );
	if ( m_bServerConnectionRedirect && nOldSignonState >= SIGNONSTATE_CONNECTED && state < SIGNONSTATE_CONNECTED )
		nOldSignonState = state; // during server redirect attempt to keep the MMS session
	pEvent->SetInt( "old", nOldSignonState );
	pEvent->SetInt( "new", state );
	pEvent->SetInt( "count", count );
	g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( pEvent );

	if ( m_nSignonState >= SIGNONSTATE_CONNECTED )
	{
		// tell server that we entered now that state
		CNETMsg_SignonState_t msgSignonState( m_nSignonState, count);
		m_NetChannel->SendNetMsg( msgSignonState );
	}

	return true;
}

bool CClientState::HookClientStringTable( char const *tableName )
{
	INetworkStringTable *table = GetStringTable( tableName );
	if ( !table )
	{
		// If engine takes a pass, allow client dll to hook in its callbacks
		if ( g_ClientDLL )
		{
			g_ClientDLL->InstallStringTableCallback( tableName );
		}
        return false;
	}

	// Hook Model Precache table
	if ( !Q_strcasecmp( tableName, MODEL_PRECACHE_TABLENAME ) )
	{
		m_pModelPrecacheTable = table;
		return true;
	}

	if ( !Q_strcasecmp( tableName, GENERIC_PRECACHE_TABLENAME ) )
	{
		m_pGenericPrecacheTable = table;
		return true;
	}

	if ( !Q_strcasecmp( tableName, SOUND_PRECACHE_TABLENAME ) )
	{
		m_pSoundPrecacheTable = table;
		return true;
	}

	if ( !Q_strcasecmp( tableName, DECAL_PRECACHE_TABLENAME ) )
	{
		// Cache the id
		m_pDecalPrecacheTable = table;
		return true;
	}

	if ( !Q_strcasecmp( tableName, INSTANCE_BASELINE_TABLENAME ) )
	{
		// Cache the id
		m_pInstanceBaselineTable = table;
		return true;
	}

	if ( !Q_strcasecmp( tableName, LIGHT_STYLES_TABLENAME ) )
	{
		// Cache the id
		m_pLightStyleTable = table;
		return true;
	}

	if ( !Q_strcasecmp( tableName, USER_INFO_TABLENAME ) )
	{
		// Cache the id
		m_pUserInfoTable = table;
		return true;
	}

	if ( !Q_strcasecmp( tableName, SERVER_STARTUP_DATA_TABLENAME ) )
	{
		// Cache the id
		m_pServerStartupTable = table;
		return true;
	}

	if ( !Q_strcasecmp( tableName, DOWNLOADABLE_FILE_TABLENAME ) )
	{
		// Cache the id
		m_pDownloadableFileTable = table;
		return true;
	}

	if ( !Q_strcasecmp( tableName, DYNAMIC_MODEL_TABLENAME ) )
	{
		m_pDynamicModelTable = table;
		return true;
	}

	// If engine takes a pass, allow client dll to hook in its callbacks
	g_ClientDLL->InstallStringTableCallback( tableName );

	return false;
}

bool CClientState::InstallEngineStringTableCallback( char const *tableName )
{
	INetworkStringTable *table = GetStringTable( tableName );

	if ( !table )
		return false;

	// Hook Model Precache table
	if ( !Q_strcasecmp( tableName, MODEL_PRECACHE_TABLENAME ) )
	{
		table->SetStringChangedCallback( NULL, Callback_ModelChanged );
		return true;
	}

	if ( !Q_strcasecmp( tableName, GENERIC_PRECACHE_TABLENAME ) )
	{
		// Install the callback
		table->SetStringChangedCallback( NULL, Callback_GenericChanged );
		return true;
	}

	if ( !Q_strcasecmp( tableName, SOUND_PRECACHE_TABLENAME ) )
	{
		// Install the callback
		table->SetStringChangedCallback( NULL, Callback_SoundChanged );
		return true;
	}

	if ( !Q_strcasecmp( tableName, DECAL_PRECACHE_TABLENAME ) )
	{
		// Install the callback
		table->SetStringChangedCallback( NULL, Callback_DecalChanged );
		return true;
	}

	if ( !Q_strcasecmp( tableName, INSTANCE_BASELINE_TABLENAME ) )
	{
		// Install the callback (already done above)
		table->SetStringChangedCallback( NULL, Callback_InstanceBaselineChanged );
		return true;
	}

	if ( !Q_strcasecmp( tableName, LIGHT_STYLES_TABLENAME ) )
	{
		return true;
	}

	if ( !Q_strcasecmp( tableName, USER_INFO_TABLENAME ) )
	{
		// Install the callback
		table->SetStringChangedCallback( NULL, Callback_UserInfoChanged );
		return true;
	}

	if ( !Q_strcasecmp( tableName, SERVER_STARTUP_DATA_TABLENAME ) )
	{
		return true;
	}

	if ( !Q_strcasecmp( tableName, DOWNLOADABLE_FILE_TABLENAME ) )
	{
		return true;
	}

	if ( !Q_strcasecmp( tableName, DYNAMIC_MODEL_TABLENAME ) )
	{
		table->SetStringChangedCallback( NULL, Callback_DynamicModelChanged );
		m_pDynamicModelTable = table;
		return true;
	}

	// The the client.dll have a shot at it
	return false;
}

void CClientState::InstallStringTableCallback( char const *tableName )
{
	// Let engine hook callbacks before we read in any data values at all
	if ( !InstallEngineStringTableCallback( tableName ) )
	{
		// If engine takes a pass, allow client dll to hook in its callbacks
		g_ClientDLL->InstallStringTableCallback( tableName );
	}
}

bool CClientState::IsPaused() const
{
	return m_bPaused || ( g_LostVideoMemory && Host_IsSinglePlayerGame() ) ||
		!host_initialized || 
		demoplayer->IsPlaybackPaused() ||
		EngineVGui()->ShouldPause();
}

float CClientState::GetTime() const
{
	int nTickCount = GetClientTickCount();
	float flTickTime = nTickCount * host_state.interval_per_tick;
	float flResult;
	
	// Timestamps are rounded to exact tick during simulation
	if ( insimulation )
	{
		return flTickTime;
	}
	
#if defined(_X360) || defined( _PS3 )
	// This function is called enough under ComputeLightingState to make this little cache worthwhile [10/6/2010 tom]
	static float lastResult;
	static int lastTick;
	if ( lastTick == nTickCount )
	{
		return lastResult;
	}
	lastTick = nTickCount;
#endif

	// Tracker 77931:  If the game is paused, then lock the client clock at the previous tick boundary 
	//  (otherwise we'll keep interpolating through the "remainder" time causing the paused characters
	//  to twitch like they have the shakes)
	// TODO:  Since this rounds down on the frame we paused, we could see a slight backsliding.  We could remember the last "remainder" before pause and re-use it and 
	//  set insimulation == false to be more exact.  We'd still have to deal with the timing difference between
	//  when pause/unpause happens on the server versus the client
	if ( GetBaseLocalClient().IsPaused() )
	{
		// Go just before next tick
		flResult = flTickTime + host_state.interval_per_tick - 0.00001f;
	}
	else
	{
		flResult = flTickTime + m_tickRemainder;
	}

#if defined(_X360) || defined( _PS3 )
	lastResult = flResult;
#endif
	return flResult;
}

float CClientState::GetFrameTime() const
{
	if ( CClockDriftMgr::IsClockCorrectionEnabled() )
	{
		return IsPaused() ? 0 : m_frameTime;
	}
	else
	{
		if ( insimulation )
		{
			int nElapsedTicks = ( GetClientTickCount() - oldtickcount );
			return nElapsedTicks * host_state.interval_per_tick;
		}
		else
		{
			return IsPaused() ? 0 : m_frameTime;
		}
	}
}

float CClientState::GetClientInterpAmount()
{
	// we need client cvar cl_interp_ratio
	static const ConVar *s_cl_interp_ratio = NULL;
	if ( !s_cl_interp_ratio )
	{
		s_cl_interp_ratio = g_pCVar->FindVar( "cl_interp_ratio" );
		if ( !s_cl_interp_ratio )
			return 0.1f;
	}
	static const ConVar *s_cl_interp = NULL;
	if ( !s_cl_interp )
	{
		s_cl_interp = g_pCVar->FindVar( "cl_interp" );
		if ( !s_cl_interp )
			return 0.1f;
	}
		
	float flInterpRatio = s_cl_interp_ratio->GetFloat();
	float flInterp = s_cl_interp->GetFloat();

	const ConVar_ServerBounded *pBounded = dynamic_cast<const ConVar_ServerBounded*>( s_cl_interp_ratio );
	if ( pBounded )
		flInterpRatio = pBounded->GetFloat();
	//#define FIXME_INTERP_RATIO
	return MAX( flInterpRatio / cl_updaterate->GetFloat(), flInterp );
}

//-----------------------------------------------------------------------------
// Purpose: // Clear all the variables in the CClientState.
//-----------------------------------------------------------------------------
void CClientState::Clear( void )
{
	CBaseClientState::Clear();

	m_pModelPrecacheTable = NULL;
	m_pDynamicModelTable = NULL;
	m_pGenericPrecacheTable = NULL;
	m_pSoundPrecacheTable = NULL;
	m_pDecalPrecacheTable = NULL;
	m_pInstanceBaselineTable = NULL;
	m_pLightStyleTable = NULL;
	m_pUserInfoTable = NULL;
	m_pServerStartupTable = NULL;
	m_pAreaBits = NULL;
	
	// Clear all download vars.
	m_pDownloadableFileTable = NULL;
	m_hWaitForResourcesHandle = NULL;
	m_bUpdateSteamResources = false;
	m_bShownSteamResourceUpdateProgress = false;
	m_bDownloadResources = false;
	m_bDownloadingUGCMap = false;
	m_modelIndexLoaded = -1;
	m_lastModelPercent = -1;

	DeleteClientFrames( -1 ); // clear all
		
	viewangles.Init();
	m_flLastServerTickTime = 0.0f;
	oldtickcount = 0;
	insimulation = false;

	addangle.RemoveAll();
	addangletotal = 0.0f;
	prevaddangletotal = 0.0f;

	memset(model_precache, 0, sizeof(model_precache));
	memset(sound_precache, 0, sizeof(sound_precache));
	ishltv = false;
#if defined( REPLAY_ENABLED )
	isreplay = false;
#endif
	cdtrack = 0;
	serverCRC = 0;
	serverClientSideDllCRC = 0;
	last_command_ack = 0;
	last_server_tick = 0;
	command_ack = 0;
	m_nSoundSequence = 0;

	// make sure the client isn't active anymore, but stay
	// connected if we are.
	if ( m_nSignonState > SIGNONSTATE_CONNECTED )
	{
		m_nSignonState = SIGNONSTATE_CONNECTED;
	}
}

void CClientState::ClearSounds()
{
	int c = ARRAYSIZE( sound_precache );
	for ( int i = 0; i < c; ++i )
	{
		sound_precache[ i ].SetSound( NULL );
	}
}

bool CClientState::ProcessConnectionlessPacket( netpacket_t *packet )
{
	Assert( packet );

	return CBaseClientState::ProcessConnectionlessPacket( packet );
}

void CClientState::ConnectionStart( INetChannel *chan )
{
	CBaseClientState::ConnectionStart( chan );
	m_SVCMsgHltvReplay.Bind< CSVCMsg_HltvReplay_t >( chan, UtlMakeDelegate( this, &CClientState::SVCMsg_HltvReplay ) );
}

void CClientState::ConnectionStop(  )
{
	CBaseClientState::ConnectionStop( );
	m_SVCMsgHltvReplay.Unbind();
}



float CClientState::GetHltvReplayTimeScale()const
{
	extern ConVar spec_replay_rate_base;
	if ( m_nHltvReplayDelay )
	{
		int nCurrentTick = GetClientTickCount();
		if ( nCurrentTick >= m_nHltvReplaySlowdownBeginAt && nCurrentTick < m_nHltvReplaySlowdownEndAt )
			return spec_replay_rate_base.GetFloat() * m_flHltvReplaySlowdownRate;
		else
			return spec_replay_rate_base.GetFloat();
	}
	return 1.0f;
}


float CL_GetHltvReplayTimeScale()
{
	return GetBaseLocalClient().GetHltvReplayTimeScale();
}

void CClientState::StopHltvReplay()
{
	ForceFullUpdate( "Force StopHltvReplay on client" );
	m_nHltvReplayDelay = 0;
	m_nHltvReplayStopAt = 0;
	m_nHltvReplayStartAt = 0;
	if ( g_ClientDLL )
	{
		CSVCMsg_HltvReplay msg;
		g_ClientDLL->OnHltvReplay( msg );
	}
}



void CClientState::FullConnect( const ns_address &adr, int nEncryptionKey )
{
	CBaseClientState::FullConnect( adr, nEncryptionKey );
	m_NetChannel->SetDemoRecorder( g_pClientDemoRecorder );
	m_NetChannel->SetDataRate( cl_rate->GetFloat() );

	// Not in the demo loop now
	demonum = -1;		
	
	// We don't have a backed up cmd history yet
	lastoutgoingcommand = -1;

	// we didn't send commands yet
	chokedcommands = 0;
	
	// Report connection success.
	if ( !adr.IsLoopback() )
	{
		ConMsg( "Connected to %s\n", cl_hideserverip.GetInt()>0 ? "<ip hidden>" : ns_address_render( adr ).String() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : model_t
//-----------------------------------------------------------------------------
model_t *CClientState::GetModel( int index )
{
	if ( !m_pModelPrecacheTable )
	{
		return NULL;
	}

	if ( index <= 0 )
	{
		return NULL;
	}

	if ( index >= m_pModelPrecacheTable->GetNumStrings() )
	{
		Assert( 0 ); // model index for unkown model requested
		return NULL;
	}

	CPrecacheItem *p = &model_precache[ index ];
	model_t *m = p->GetModel();
	if ( m )
	{
		return m;
	}

	char const *name = m_pModelPrecacheTable->GetString( index );

	if ( host_showcachemiss.GetBool() )
	{
		ConDMsg( "client model cache miss on %s\n", name );
	}

	m = modelloader->GetModelForName( name, IModelLoader::FMODELLOADER_CLIENT );
	if ( !m )
	{
		const CPrecacheUserData *data = CL_GetPrecacheUserData( m_pModelPrecacheTable, index );
		if ( data && ( data->flags & RES_FATALIFMISSING ) )
		{
			COM_ExplainDisconnection( true, "Cannot continue without model %s, disconnecting\n", name );
			Host_Disconnect(true);
		}
	}

	p->SetModel( m );
	return m;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : int -- note -1 if missing
//-----------------------------------------------------------------------------
int CClientState::LookupModelIndex( char const *name )
{
	if ( !m_pModelPrecacheTable )
	{
		return -1;
	}
	int idx = m_pModelPrecacheTable->FindStringIndex( name );
	return ( idx == INVALID_STRING_INDEX ) ? -1 : idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//			*name - 
//-----------------------------------------------------------------------------
void CClientState::SetModel( int tableIndex )
{
	if ( !m_pModelPrecacheTable )
	{
		return;
	}

	// Bogus index
	if ( tableIndex < 0 || tableIndex >= m_pModelPrecacheTable->GetNumStrings() )
	{
		return;
	}

	CPrecacheItem *p = &model_precache[ tableIndex ];
	const CPrecacheUserData *data = CL_GetPrecacheUserData( m_pModelPrecacheTable, tableIndex );

	bool bLoadNow = ( data && ( data->flags & RES_PRELOAD ) ) || IsGameConsole();
	if ( CommandLine()->FindParm( "-nopreload" ) ||	CommandLine()->FindParm( "-nopreloadmodels" ))
	{
		bLoadNow = false;
	}
	else if ( cl_forcepreload.GetInt() || CommandLine()->FindParm( "-preload" ) )
	{
		bLoadNow = true;
	}

	if ( bLoadNow )
	{
		char const *name = m_pModelPrecacheTable->GetString( tableIndex );
		int lenModelName = V_strlen( name );
		if ( demoplayer->IsPlayingBack() && ( lenModelName > 4 ) && !V_stricmp( name + lenModelName - 4, ".bsp" ) )
			name = m_szLevelName;	// For demo playback we force the client bsp which may differ from precache table
		p->SetModel( modelloader->GetModelForName( name, IModelLoader::FMODELLOADER_CLIENT ) );
	}
	else
	{
		p->SetModel( NULL );
	}

	// log the file reference, if necessary
	if (MapReslistGenerator().IsEnabled())
	{
		char const *name = m_pModelPrecacheTable->GetString( tableIndex );
		int lenModelName = V_strlen( name );
		if ( demoplayer->IsPlayingBack() && ( lenModelName > 4 ) && !V_stricmp( name + lenModelName - 4, ".bsp" ) )
			name = m_szLevelName;	// For demo playback we force the client bsp which may differ from precache table
		MapReslistGenerator().OnModelPrecached( name );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : model_t
//-----------------------------------------------------------------------------
char const *CClientState::GetGeneric( int index )
{
	if ( !m_pGenericPrecacheTable )
	{
		Warning( "Can't GetGeneric( %d ), no precache table [no level loaded?]\n", index );
		return "";
	}

	if ( index <= 0 )
		return "";

	if ( index >= m_pGenericPrecacheTable->GetNumStrings() )
	{
		return "";
	}

	CPrecacheItem *p = &generic_precache[ index ];
	char const *g = p->GetGeneric();
	return g;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : int -- note -1 if missing
//-----------------------------------------------------------------------------
int CClientState::LookupGenericIndex( char const *name )
{
	if ( !m_pGenericPrecacheTable )
	{
		Warning( "Can't LookupGenericIndex( %s ), no precache table [no level loaded?]\n", name );
		return -1;
	}
	int idx = m_pGenericPrecacheTable->FindStringIndex( name );
	return ( idx == INVALID_STRING_INDEX ) ? -1 : idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//			*name - 
//-----------------------------------------------------------------------------
void CClientState::SetGeneric( int tableIndex )
{
	if ( !m_pGenericPrecacheTable )
	{
		Warning( "Can't SetGeneric( %d ), no precache table [no level loaded?]\n", tableIndex );
		return;
	}
	// Bogus index
	if ( tableIndex < 0 || 
		 tableIndex >= m_pGenericPrecacheTable->GetNumStrings() )
	{
		return;
	}

	char const *name = m_pGenericPrecacheTable->GetString( tableIndex );
	CPrecacheItem *p = &generic_precache[ tableIndex ];
	p->SetGeneric( name );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : char const
//-----------------------------------------------------------------------------
char const *CClientState::GetSoundName( int index )
{
	if ( index <= 0 || !m_pSoundPrecacheTable )
		return "";

	if ( index >= m_pSoundPrecacheTable->GetNumStrings() )
	{
		return "";
	}

	char const *name = m_pSoundPrecacheTable->GetString( index );
	return name;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : model_t
//-----------------------------------------------------------------------------
CSfxTable *CClientState::GetSound( int index )
{
	if ( index <= 0 || !m_pSoundPrecacheTable )
		return NULL;

	if ( index >= m_pSoundPrecacheTable->GetNumStrings() )
	{
		return NULL;
	}

	CPrecacheItem *p = &sound_precache[ index ];
	CSfxTable *s = p->GetSound();
	if ( s )
		return s;

	char const *name = m_pSoundPrecacheTable->GetString( index );

	if ( host_showcachemiss.GetBool() )
	{
		ConDMsg( "client sound cache miss on %s\n", name );
	}

	s = S_PrecacheSound( name );

	p->SetSound( s );
	return s;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : int -- note -1 if missing
//-----------------------------------------------------------------------------
int CClientState::LookupSoundIndex( char const *name )
{
	if ( !m_pSoundPrecacheTable )
		return -1;

	int idx = m_pSoundPrecacheTable->FindStringIndex( name );
	return ( idx == INVALID_STRING_INDEX ) ? -1 : idx;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//			*name - 
//-----------------------------------------------------------------------------
void CClientState::SetSound( int tableIndex )
{
	// Bogus index
	if ( !m_pSoundPrecacheTable )
		return;

	if ( tableIndex < 0 || tableIndex >= m_pSoundPrecacheTable->GetNumStrings() )
	{
		return;
	}

	CPrecacheItem *p = &sound_precache[ tableIndex ];
	const CPrecacheUserData *data = CL_GetPrecacheUserData( m_pSoundPrecacheTable, tableIndex );

	bool bLoadNow = ( data && ( data->flags & RES_PRELOAD ) ) || IsGameConsole();
	if ( CommandLine()->FindParm( "-nopreload" ) ||	CommandLine()->FindParm( "-nopreloadsounds" ))
	{
		bLoadNow = false;
	}
	else if ( cl_forcepreload.GetInt() || CommandLine()->FindParm( "-preload" ) )
	{
		bLoadNow = true;
	}

	if ( bLoadNow )
	{
		char const *name = m_pSoundPrecacheTable->GetString( tableIndex );
		CSfxTable *pSfxTable = S_PrecacheSound( name );
		if ( ( pSfxTable != NULL ) && pSfxTable->m_bIsLateLoad )
		{
			DevWarning( "    CClientState::SetSound() created the late loading.\n" );
		}
		p->SetSound( pSfxTable );
	}
	else
	{
		p->SetSound( NULL );
	}

	// log the file reference, if necssary
	if (MapReslistGenerator().IsEnabled())
	{
		char const *name = m_pSoundPrecacheTable->GetString( tableIndex );
		MapReslistGenerator().OnSoundPrecached( name );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : model_t
//-----------------------------------------------------------------------------
char const *CClientState::GetDecalName( int index )
{
	if ( index <= 0 || !m_pDecalPrecacheTable )
	{
		return NULL;
	}

	if ( index >= m_pDecalPrecacheTable->GetNumStrings() )
	{
		return NULL;
	}

	CPrecacheItem *p = &decal_precache[ index ];
	char const *d = p->GetDecal();
	return d;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
//			*name - 
//-----------------------------------------------------------------------------
void CClientState::SetDecal( int tableIndex )
{
	if ( !m_pDecalPrecacheTable )
		return;

	if ( tableIndex < 0 || 
		 tableIndex >= m_pDecalPrecacheTable->GetNumStrings() )
	{
		return;
	}

	char const *name = m_pDecalPrecacheTable->GetString( tableIndex );
	CPrecacheItem *p = &decal_precache[ tableIndex ];
	p->SetDecal( name );

	Draw_DecalSetName( tableIndex, (char *)name );
}


//-----------------------------------------------------------------------------
// Purpose: sets friends info locally to be sent to other users
//-----------------------------------------------------------------------------
void CClientState::SetFriendsID( uint friendsID, const char *friendsName )
{
	m_nFriendsID = friendsID;
	Q_strncpy( m_FriendsName, friendsName, sizeof(m_FriendsName) );
}


void CClientState::CheckOthersCustomFile( CRC32_t crcValue )
{
	if ( crcValue == 0 )
		return; // not a valid custom file

	extern ConVar cl_allowdownload;
	if ( !cl_allowdownload.GetBool() )
		return; // client doesn't want to download anything

	CCustomFilename filehex( crcValue );

	if ( g_pFileSystem->FileExists( filehex.m_Filename ) )
		return; // we already have this file (assuming the CRC is correct)

	// we don't have it, request download from server
	m_NetChannel->RequestFile( filehex.m_Filename, false );
}

void CClientState::AddCustomFile( int slot, const char *resourceFile)
{
	if ( Q_strlen(resourceFile) <= 0 )
		return; // no resource file given

	if ( !COM_IsValidPath( resourceFile ) )
	{
		Msg("Customization file '%s' has invalid path.\n", resourceFile  );
		return;
	}

	if ( slot < 0 || slot >= MAX_CUSTOM_FILES )
		return; // wrong slot

	if ( !g_pFileSystem->FileExists( resourceFile ) )
	{
		DevMsg("Couldn't find customization file '%s'.\n", resourceFile );
		return; // resource file doesn't exits
	}

	if ( g_pFileSystem->Size( resourceFile ) > MAX_CUSTOM_FILE_SIZE )
	{
		Msg("Customization file '%s' is too big ( >%i bytes).\n", resourceFile, MAX_CUSTOM_FILE_SIZE );
		return; // resource file doesn't exits
	}

	CRC32_t crcValue;

	// Compute checksum of resource file
	CRC_File( &crcValue, resourceFile );

	// Copy it into materials/downloads if it's not there yet, so the server doesn't have to 
	// transmit the file back to us.
	bool bCopy = true;
	CCustomFilename filehex( crcValue );
	if ( g_pFileSystem->FileExists( filehex.m_Filename ) )
	{
		// check if existing file already has same CRC, 
		// then we don't need to copy it anymore
		CRC32_t test;
		CRC_File( &test, filehex.m_Filename );
		if ( test == crcValue )
			bCopy = false;
	}

	if ( bCopy )
	{
		// Copy it over under the new name
		COM_CopyFile( resourceFile, filehex.m_Filename );

		if ( !g_pFileSystem->FileExists( filehex.m_Filename ) )
		{
			Warning( "CacheCustomFiles: can't copy '%s' to '%s'.\n", resourceFile, filehex.m_Filename );
			return;
		}
	}

	/* Finally, validate the VTF file. TODO
	CUtlVector<char> fileData;
	if ( LogoFile_ReadFile( crcValue, fileData ) )
	{
		bValid = true;
	}
	else
	{
		Warning( "CL_LogoFile_OnConnect: logo file '%s' invalid.\n", logotexture );
	} */

	m_nCustomFiles[slot].crc = crcValue; // first slot is logo
	m_nCustomFiles[slot].reqID = 0;

}

void CClientState::CheckOwnCustomFiles()
{
	// clear file CRCs
	Q_memset( m_nCustomFiles, 0, sizeof(m_nCustomFiles) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CClientState::DumpPrecacheStats( const char * name )
{
	if ( !name || !name[0] )
	{
		ConMsg( "Can only dump stats when active in a level\n" );
		return;
	}

	CPrecacheItem *items = NULL;
	
	if ( !Q_strcmp(MODEL_PRECACHE_TABLENAME, name ) )
	{
		items = model_precache;
	}
	else if ( !Q_strcmp(GENERIC_PRECACHE_TABLENAME, name ) )
	{
		items = generic_precache;
	}
	else if ( !Q_strcmp(SOUND_PRECACHE_TABLENAME, name ) )
	{
		items = sound_precache;
	}
	else if ( !Q_strcmp(DECAL_PRECACHE_TABLENAME, name ) )
	{
		items = decal_precache;
	}

	INetworkStringTable *table = GetStringTable( name );

	if ( !items || !table)
	{
		ConMsg( "Precache table '%s' not found.\n", name );
		return;
	}

	int count =  table->GetNumStrings();
	int maxcount = table->GetMaxStrings();

	ConMsg( "\n" );
	ConMsg( "Precache table %s:  %i of %i slots used\n", table->GetTableName(),
		count, maxcount );

	for ( int i = 0; i < count; i++ )
	{
		char const *name = table->GetString( i );
		CPrecacheItem *slot = &items[ i ];
		const CPrecacheUserData *p = CL_GetPrecacheUserData( table, i );

		if ( !name || !slot || !p )
			continue;

		ConMsg( "%03i:  %s (%s):   ",
			i,
			name, 
			GetFlagString( p->flags ) );


		if ( slot->GetReferenceCount() == 0 )
		{
			ConMsg( " never used\n" );
		}
		else
		{
			ConMsg( " %i refs, first %.2f mru %.2f\n",
				slot->GetReferenceCount(), 
				slot->GetFirstReference(), 
				slot->GetMostRecentReference() );
		}
	}

	ConMsg( "\n" );
}

void CClientState::ReadDeletions( CEntityReadInfo &u )
{
	VPROF( "ReadDeletions" );
	int nBase = -1;
	int nCount = u.m_pBuf->ReadUBitVar();
	for ( int i = 0; i < nCount; ++i )
	{
		int nDelta = u.m_pBuf->ReadUBitVar();
		int nSlot = nBase + nDelta;

		Assert( !u.m_pTo->transmit_entity.Get( nSlot ) );

		CL_DeleteDLLEntity( nSlot, "ReadDeletions" );

		nBase = nSlot;
	}
}

inline static UpdateType DetermineUpdateType( CEntityReadInfo &u, int oldEntity )
{
	if ( !u.m_bIsEntity || ( u.m_nNewEntity > oldEntity ) )
	{
		// If we're at the last entity, preserve whatever entities followed it in the old packet.
		// If newnum > oldnum, then the server skipped sending entities that it wants to leave the state alone for.
		if ( !u.m_pFrom	 || ( oldEntity > u.m_pFrom->last_entity ) )
		{
			return Finished;
		}

		// Preserve entities until we reach newnum (ie: the server didn't send certain entities because
		// they haven't changed).
	}
	else
	{
		if( u.m_UpdateFlags & FHDR_ENTERPVS )
		{
			return EnterPVS;
		}
		else if( u.m_UpdateFlags & FHDR_LEAVEPVS )
		{
			return LeavePVS;
		}
		return DeltaEnt;
	}

	return PreserveEnt;
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

void CClientState::ReadPacketEntities( CEntityReadInfo &u )
{
	// Loop until there are no more entities to read

	bool bRecord = cl_entityreport.GetBool();

	int oldEntity = u.m_nOldEntity;
	oldEntity = u.GetNextOldEntity(u.m_nOldEntity);
	UpdateType updateType = u.m_UpdateType;

	while ( updateType < Finished )
	{
		u.m_nHeaderCount--;

		u.m_bIsEntity = ( u.m_nHeaderCount >= 0 ) ? true : false;

		if ( u.m_bIsEntity  )
		{
			CL_ParseDeltaHeader( u );
		}

		for ( updateType = PreserveEnt; updateType == PreserveEnt; )
		{
			// Figure out what kind of an update this is.
			updateType = DetermineUpdateType(u, oldEntity);
			switch( updateType )
			{
			case EnterPVS:	
				{
					int iClass = u.m_pBuf->ReadUBitLong( m_nServerClassBits );

					int iSerialNum = u.m_pBuf->ReadUBitLong( NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS );
					u.m_nOldEntity = oldEntity;
					CL_CopyNewEntity( u, iClass, iSerialNum );

					if ( u.m_nNewEntity == oldEntity ) // that was a recreate
					{
						oldEntity = u.GetNextOldEntity(oldEntity);
					}
				}
				break;

			case LeavePVS:
				{
					if ( !u.m_bAsDelta )
					{
						Assert(0); // GetBaseLocalClient().validsequence = 0;
						ConMsg( "WARNING: LeavePVS on full update" );
						updateType = Failed;	// break out
					}
					else
					{
						Assert( !u.m_pTo->transmit_entity.Get( oldEntity ) );

						if ( u.m_UpdateFlags & FHDR_DELETE )
						{
							CL_DeleteDLLEntity( oldEntity, "ReadLeavePVS" );
						}

						oldEntity = u.GetNextOldEntity(oldEntity);
					}
				}
				break;

			case DeltaEnt:
				{
					u.m_nOldEntity = oldEntity;
					CL_CopyExistingEntity( u );
					oldEntity = u.GetNextOldEntity(oldEntity);
				}
				break;

			case PreserveEnt:
				{
					if ( !u.m_bAsDelta )  // Should never happen on a full update.
					{
						updateType = Failed;	// break out
					}
					else
					{
						Assert( u.m_pFrom->transmit_entity.Get(oldEntity) );

						// copy one of the old entities over to the new packet unchanged
						if ( u.m_nNewEntity < 0 || u.m_nNewEntity >= MAX_EDICTS )
						{
							Host_Error ("CL_ReadPreserveEnt: u.m_nNewEntity == MAX_EDICTS");
						}

						u.m_pTo->last_entity = oldEntity;
						u.m_pTo->transmit_entity.Set( oldEntity );

						if ( bRecord )
						{
							CL_RecordEntityBits( oldEntity, 0 );
						}

						oldEntity = u.GetNextOldEntity(oldEntity);
					}
				}
				break;

			default:
				break;
			}
		}
	}
	u.m_nOldEntity = oldEntity;
	u.m_UpdateType = updateType;

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

//-----------------------------------------------------------------------------
// Purpose: Starts checking that all the necessary files are local
//-----------------------------------------------------------------------------
void CClientState::StartUpdatingSteamResources()
{
	if ( IsX360() )
	{
		return;
	}

	// we can only do this when in SIGNONSTATE_NEW, 
	// since the completion of this triggers the continuation of SIGNONSTATE_NEW
	Assert(m_nSignonState == SIGNONSTATE_NEW);

	// make sure we have all the necessary resources locally before continuing
	m_hWaitForResourcesHandle = g_pFileSystem->WaitForResources(m_szLevelNameShort);
	m_bUpdateSteamResources = true;
	m_bShownSteamResourceUpdateProgress = false;
	m_bDownloadResources = false;
	m_bDownloadingUGCMap = false; 
}

// INFESTED_DLL
bool g_bASW_Waiting_For_Map_Build = false;

CON_COMMAND( asw_engine_finished_building_map, "Notify engine that we've finished building a map" )
{
	g_bASW_Waiting_For_Map_Build = false;
}

//-----------------------------------------------------------------------------
// Purpose: checks to see if we're done updating files
//-----------------------------------------------------------------------------
void CClientState::CheckUpdatingSteamResources()
{
	if ( IsX360() )
	{
		return;
	}

	VPROF_BUDGET( "CheckUpdatingSteamResources", VPROF_BUDGETGROUP_STEAM );

	if (m_bUpdateSteamResources)
	{
		bool bComplete = false;
		float flProgress = 0.0f;
		g_pFileSystem->GetWaitForResourcesProgress(m_hWaitForResourcesHandle, &flProgress, &bComplete);

		if (bComplete)
		{
			m_hWaitForResourcesHandle = NULL;
			m_bUpdateSteamResources = false;
			m_bDownloadResources = false;
			m_bDownloadingUGCMap = false;

			if ( m_pDownloadableFileTable )
			{
				bool allowDownloads = true;
				bool allowSoundDownloads = true;

				if ( !Q_strcasecmp( cl_downloadfilter.GetString(), "none" ) )
				{
					allowDownloads = allowSoundDownloads = false;
				}
				else if ( !Q_strcasecmp( cl_downloadfilter.GetString(), "nosounds" ) )
				{
					allowSoundDownloads = false;
				}

				if ( allowDownloads )
				{
					char extension[4];
					for ( int i=0; i<m_pDownloadableFileTable->GetNumStrings(); ++i )
					{
						const char *fname = m_pDownloadableFileTable->GetString( i );

						if ( !allowSoundDownloads )
						{
							Q_ExtractFileExtension( fname, extension, sizeof( extension ) );
							if ( !Q_strcasecmp( extension, "wav" ) || !Q_strcasecmp( extension, "mp3" ) )
							{
								continue;
							}
						}

						// If this is a community map we're loading download from the workshop cdn instead of server.
						char bufFileName[MAX_PATH];
						V_FixupPathName( bufFileName, sizeof( bufFileName ), fname );

						EngineVGui()->UpdateProgressBar(PROGRESS_PROCESSSERVERINFO);

						if ( m_unUGCMapFileID != 0 )
						{
							int lenBufFileName = V_strlen( bufFileName );
							if ( !V_stricmp( bufFileName, m_szLevelName ) || // if UGC map file ID is set and the resource is the bsp then download the client level
								( ( lenBufFileName > 4 ) && !V_stricmp( bufFileName + lenBufFileName - 4, ".bsp" ) ) )
							{
								g_ClientDLL->DownloadCommunityMapFile( m_unUGCMapFileID );
								m_bDownloadingUGCMap = true;
								if ( cl_debug_ugc_downloads.GetBool() )
									Msg( "CheckUpdatingSteamResources: downloading UGC map file '%s' id %llu\n", m_szLevelName, m_unUGCMapFileID );

								continue;
							}
							
							if ( ( lenBufFileName > 4 ) && !V_stricmp( bufFileName + lenBufFileName - 4, ".nav" ) )
								continue; // UGC maps always have nav embedded in the bsp
						}

						if ( demoplayer->IsPlayingBack() && ( cl_download_demoplayer.GetInt() < 2 ) )
							continue; // demo playback doesn't need to download all the resources
								
						if ( cl_debug_ugc_downloads.GetBool() )
							Msg( "CheckUpdatingSteamResources: downloading file '%s'\n", bufFileName );

						// INFESTED_DLL
						static char gamedir[MAX_OSPATH];
						Q_FileBase( com_gamedir, gamedir, sizeof( gamedir ) );
						if ( !Q_stricmp( gamedir, "infested" ) )
						{							
							// if we're trying to download a randomly generated map, instead request the map layout file
							const char *pExt = V_GetFileExtension( fname );
							if ( !Q_stricmp( pExt, "bsp" ) )
							{
								char mapLayoutFName[256];
								Q_snprintf( mapLayoutFName, sizeof( mapLayoutFName ), "%s", fname );
								int extPos = pExt - fname;
								Q_snprintf( mapLayoutFName + extPos, sizeof( mapLayoutFName ) - extPos, "layout" );

								if ( StringHasPrefix( fname + 5, "gridrandom" ) || StringHasPrefix( fname + 5, "output" ) ) 
								{
									CL_QueueDownload( mapLayoutFName );
								}
								else
								{
									// if we're downloading a non-random map, make sure we're not waiting for a map build
									g_bASW_Waiting_For_Map_Build = false;
									CL_QueueDownload( fname );
								}
							}
							else
							{
								CL_QueueDownload( fname );
							}
						}
						else
						{
							CL_QueueDownload( fname );
						}
					}
				}

				if ( CL_GetDownloadQueueSize() || g_bASW_Waiting_For_Map_Build || m_bDownloadingUGCMap )
				{
					// make sure the loading dialog is up
					EngineVGui()->StartCustomProgress();
					EngineVGui()->ActivateGameUI();
					m_bDownloadResources = true;
				}
				else
				{
					m_bDownloadResources = false;
					FinishSignonState_New();
				}
			}
			else
			{
				Host_Error( "Invalid download file table." );
			}
		}
		else if (flProgress > 0.0f)
		{
			if (!m_bShownSteamResourceUpdateProgress)
			{
				// make sure the loading dialog is up
				EngineVGui()->StartCustomProgress();
				EngineVGui()->ActivateGameUI();
				m_bShownSteamResourceUpdateProgress = true;
			}

			// change it to be updating steam resources
			EngineVGui()->UpdateSecondaryProgressBar( flProgress, (flProgress < 1.0f) ? g_pVGuiLocalize->FindSafe("#Valve_UpdatingSteamResources") : L"" );
		}
	}

	if ( m_bDownloadResources || m_bDownloadingUGCMap )
	{
		// Check on any HTTP downloads in progress
		bool stillDownloading = CL_DownloadUpdate();
		if ( m_bDownloadingUGCMap )
		{
			float progress = g_ClientDLL->GetUGCFileDownloadProgress( m_unUGCMapFileID );
			if ( progress == 1.0f || progress < 0.0f )
				m_bDownloadingUGCMap = false; // stop waiting if we're done, or on error.

			if ( cl_debug_ugc_downloads.GetBool() )
				Msg( "CheckUpdatingSteamResources: Downloading UGC Map.... %f%%\n", 100.0f*progress );

			wchar_t wszPercent[ 10 ];
			V_snwprintf( wszPercent, ARRAYSIZE( wszPercent ), L"%d%%",  (int)(100*progress) );
			wchar_t wszWideBuff[ 128 ];
			g_pVGuiLocalize->ConstructString( wszWideBuff, sizeof( wszWideBuff ), g_pVGuiLocalize->Find( "#SFUI_Loading_UGCMap_Progress" ), 1, wszPercent );

			// change it to be updating steam resources
			EngineVGui()->UpdateSecondaryProgressBar( progress, ( (progress > 0.0f) && (progress < 1.0f) ) ? wszWideBuff : L"" );
		}

		if ( !stillDownloading && !g_bASW_Waiting_For_Map_Build && !m_bDownloadingUGCMap )
		{
			m_bDownloadResources = false;
			FinishSignonState_New();

			// Setting to blank will clear it
			EngineVGui()->UpdateSecondaryProgressBar( 1, L"" );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: At a certain rate, this function will verify any unverified
// file CRCs with the server.
//-----------------------------------------------------------------------------
void CClientState::CheckFileCRCsWithServer()
{
	VPROF_( "CheckFileCRCsWithServer", 1, VPROF_BUDGETGROUP_OTHER_NETWORKING, false, BUDGETFLAG_CLIENT );
	const float flBatchInterval = 1.0f / 5.0f;
	const int nBatchSize = 5;

	// Don't do this yet..
	if ( !m_bCheckCRCsWithServer )
		return;

	if ( m_nSignonState != SIGNONSTATE_FULL )
		return;

	// Only send a batch every so often.
	float flCurTime = Plat_FloatTime();
	if ( (flCurTime - m_flLastCRCBatchTime) < flBatchInterval )
		return;

	m_flLastCRCBatchTime = flCurTime;

	CUnverifiedFileHash rgUnverifiedFiles[nBatchSize];
	int count = g_pFileSystem->GetUnverifiedFileHashes( rgUnverifiedFiles, ARRAYSIZE( rgUnverifiedFiles ) );
	if ( count == 0 )
		return;

	// Send the messages to the server.
	for ( int i=0; i < count; i++ )
	{
		CCLCMsg_FileCRCCheck_t crcCheck;
		CCLCMsg_FileCRCCheck_t::SetPath( crcCheck, rgUnverifiedFiles[i].m_PathID );
		CCLCMsg_FileCRCCheck_t::SetFileName( crcCheck, rgUnverifiedFiles[i].m_Filename );
		crcCheck.set_file_fraction( rgUnverifiedFiles[i].m_nFileFraction );
		crcCheck.set_md5( (void*)(rgUnverifiedFiles[i].m_FileHash.m_md5contents.bits), MD5_DIGEST_LENGTH );
		crcCheck.set_crc ( CRC32_ConvertToUnsignedLong( &rgUnverifiedFiles[i].m_FileHash.m_crcIOSequence ) );
		crcCheck.set_file_hash_type ( rgUnverifiedFiles[i].m_FileHash.m_eFileHashType );
		crcCheck.set_file_len ( rgUnverifiedFiles[i].m_FileHash.m_cbFileLen );
		crcCheck.set_pack_file_number( rgUnverifiedFiles[i].m_FileHash.m_nPackFileNumber );
		crcCheck.set_pack_file_id( rgUnverifiedFiles[i].m_FileHash.m_PackFileID );

		m_NetChannel->SendNetMsg( crcCheck );
	}
}


//-----------------------------------------------------------------------------
// Purpose: sanity-checks the variables in a VMT file to prevent the client from
// making player etc. textures that glow or show through walls etc.  Anything
// other than $baseTexture and $bumpmap is hereby verboten.
//-----------------------------------------------------------------------------
bool CheckSimpleMaterial( IMaterial *pMaterial )
{
	if ( !pMaterial )
		return false;

	const char *name = pMaterial->GetShaderName();
	if ( Q_strncasecmp( name, "VertexLitGeneric", 16 ) &&
		 Q_strncasecmp( name, "UnlitGeneric", 12 ) &&
		 Q_strncasecmp( name, "Infected", 8 ) )
	{
		return false;
	}

	if ( pMaterial->GetMaterialVarFlag( MATERIAL_VAR_IGNOREZ ) )
		return false;

	if ( pMaterial->GetMaterialVarFlag( MATERIAL_VAR_WIREFRAME ) )
		return false;

	if ( pMaterial->GetMaterialVarFlag( MATERIAL_VAR_SELFILLUM ) )
		return false;

	if ( pMaterial->GetMaterialVarFlag( MATERIAL_VAR_ADDITIVE ) )
		return false;

	if ( pMaterial->GetMaterialVarFlag( MATERIAL_VAR_NOFOG ) )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: find a filename in the string table, ignoring case and slash mismatches.  Returns the index, or INVALID_STRING_INDEX if not found.
//-----------------------------------------------------------------------------
int FindFilenameInStringTable( INetworkStringTable *table, const char *searchFname )
{
	char searchFilename[MAX_PATH];
	char tableFilename[MAX_PATH];

	Q_strncpy( searchFilename, searchFname, MAX_PATH );
	Q_FixSlashes( searchFilename );

	for ( int i=0; i<table->GetNumStrings(); ++i )
	{
		const char *tableFname = table->GetString( i );
		Q_strncpy( tableFilename, tableFname, MAX_PATH );
		Q_FixSlashes( tableFilename );

		if ( !Q_strcasecmp( searchFilename, tableFilename ) )
		{
			return i;
		}
	}

	return INVALID_STRING_INDEX;
}

//-----------------------------------------------------------------------------
// Purpose: find a filename in the string table, ignoring case and slash mismatches.
// Returns the consistency type, with CONSISTENCY_NONE being a Not Found result.
//-----------------------------------------------------------------------------
ConsistencyType GetFileConsistencyType( INetworkStringTable *table, const char *searchFname )
{
	int index = FindFilenameInStringTable( table, searchFname );
	if ( index == INVALID_STRING_INDEX )
	{
		return CONSISTENCY_NONE;
	}

	int length = 0;
	unsigned char *userData = NULL;
	userData = (unsigned char *)table->GetStringUserData( index, &length );
	if ( userData && length == sizeof( ExactFileUserData ) )
	{
		switch ( userData[0] )
		{
		case CONSISTENCY_EXACT:
		case CONSISTENCY_SIMPLE_MATERIAL:
			return (ConsistencyType)userData[0];
		default:
			return CONSISTENCY_NONE;
		}
	}
	else
	{
		return CONSISTENCY_NONE;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Does a CRC check compared to the CRC stored in the user data.
//-----------------------------------------------------------------------------
bool CheckCRCs( unsigned char *userData, int length, const char *filename )
{
	if ( userData && length == sizeof( ExactFileUserData ) )
	{
		if ( userData[0] != CONSISTENCY_EXACT && userData[0] != CONSISTENCY_SIMPLE_MATERIAL )
		{
			return false;
		}

		ExactFileUserData *exactFileData = (ExactFileUserData *)userData;

		CRC32_t crc;
		if ( !CRC_File( &crc, filename ) )
		{
			return false;
		}

		return ( crc == exactFileData->crc );
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: completes the SIGNONSTATE_NEW state
//-----------------------------------------------------------------------------
void CClientState::FinishSignonState_New()
{
	// make sure we're still in the right signon state
	if (m_nSignonState != SIGNONSTATE_NEW)
		return;

	if ( demoplayer->IsPlayingBack() )
	{
		demoplayer->SetPacketReadSuspended( false );
	}

	VPROF_BUDGET( "FinishSignonState_New", VPROF_BUDGETGROUP_STEAM );


	if ( !m_bMarkedCRCsUnverified )
	{
		// Mark all file CRCs unverified once per server. We may have verified CRCs for certain files on
		// the previous server, but we need to reverify them on the new server.
		m_bMarkedCRCsUnverified = true;
		g_pFileSystem->MarkAllCRCsUnverified();
	}

	// Check for a new whitelist. It's good to do it early in the connection process here because if we wait until later,
	// the client may have loaded some files w/o the proper whitelist restrictions and we'd have to reload them.
	m_bCheckCRCsWithServer = false;	// Don't check CRCs yet.. wait until we got a whitelist and cleaned out our files based on it to send CRCs.
	CL_CheckForPureServerWhitelist();
	
	// Verify the map and player .mdl crc's now that we've finished downloading missing resources (maps etc)
	if ( !CL_CheckCRCs( m_szLevelName ) )
	{
		Host_Error( "Unabled to verify map %s\n", ( m_szLevelName && m_szLevelName[0] ) ? m_szLevelName : "unknown" );
		return;
	}

	// Don't load the client if we don't own the game
	if ( NET_IsMultiplayer() && Steam3Client().SteamApps() && !Steam3Client().SteamApps()->BIsSubscribed() )
	{
		Host_Error( "Steam ownership check failed.\n" );
		return;
	} 

	COM_TimestampedLog( "CL_InstallAndInvokeClientStringTableCallbacks" );
	CL_InstallAndInvokeClientStringTableCallbacks();
	
#if 0

	// HACK!!!!  For use only on PC not yet using a whitelist!
	// install hooks
	if ( IsPC() && 	( m_nMaxClients > 1 ) )
	{
		m_pModelPrecacheTable->SetStringChangedCallback( NULL, Callback_ModelChanged );

		int nTableCount = m_StringTableContainer->GetNumTables();
		for ( int iTable =0; iTable < nTableCount; ++iTable )
		{
			// iterate through server tables
			CNetworkStringTable *pTable = (CNetworkStringTable*)m_StringTableContainer->GetTable( iTable );
			if ( !pTable )
				continue;

			pfnStringChanged pCallbackFunction = pTable->GetCallback();
			if ( pCallbackFunction )
				for ( int iString = 0; iString < pTable->GetNumStrings(); ++iString )
				{
					int userDataSize;
					const void *pUserData = pTable->GetStringUserData( iString, &userDataSize );
					(*pCallbackFunction)( NULL, pTable, iString, pTable->GetString( iString ), pUserData );
				}
		}

		materials->CacheUsedMaterials();
	}

#endif

	COM_TimestampedLog( "materials->CacheUsedMaterials" );

	materials->CacheUsedMaterials();

	COM_TimestampedLog( "ConsistencyCheck" );
	// force a consistency check
	ConsistencyCheck( true );
	
	COM_TimestampedLog( "CL_RegisterResources" );
	CL_RegisterResources();

	// Done with all resources, issue prespawn command.
	// Include server count in case server disconnects and changes level during d/l

	// Tell rendering system we have a new set of models.
	R_LevelInit();

	EngineVGui()->UpdateProgressBar(PROGRESS_SENDCLIENTINFO);
	if ( !m_NetChannel )
		return;
	
	SendClientInfo();

	CL_SetSteamCrashComment();

	// tell server that we entered now that state
	CNETMsg_SignonState_t msgSignonState( m_nSignonState, m_nServerCount );
	m_NetChannel->SendNetMsg( msgSignonState );
}


//-----------------------------------------------------------------------------
// Purpose: run a file consistency check if enforced by server
//-----------------------------------------------------------------------------
void CClientState::ConsistencyCheck(bool bChanged )
{
	VPROF_BUDGET( "CClientState::ConsistencyCheck", VPROF_BUDGETGROUP_OTHER_FILESYSTEM );

	// get the default config for the current card as a starting point.
	// server must have sent us this table
	if ( !m_pDownloadableFileTable )
		return;

	// no checks during single player or demo playback
	if( (m_nMaxClients == 1) || demoplayer->IsPlayingBack() )
		return;

	// only if we are connected
	if ( !IsConnected() )
		return;

	// only if enforce by server
	if ( !sv_consistency.GetBool() )
		return;

	// check if material configuration changed
	static MaterialSystem_Config_t s_LastConfig;
	MaterialSystem_Config_t newConfig = materials->GetCurrentConfigForVideoCard();

	if ( Q_memcmp( &s_LastConfig, &newConfig, sizeof(MaterialSystem_Config_t) ) )
	{
		// remember last config we tested
		s_LastConfig = newConfig;
		bChanged = true;
	}

	if ( !bChanged )
		return;

	char errorFilenameBuf[MAX_PATH] = "";

	// check CRCs and model sizes
	Color red(  200,  20,  20, 255 );
	Color blue( 100, 100, 200, 255 );
	for ( int i=0; i<m_pDownloadableFileTable->GetNumStrings(); ++i )
	{
		int length = 0;
		unsigned char *userData = NULL;
		userData = (unsigned char *)m_pDownloadableFileTable->GetStringUserData( i, &length );
		const char *filename = m_pDownloadableFileTable->GetString( i );

		// [FTrepte] Ignore the CRC check for Counter-Strike 15.
		// $FIXME: Is this the right thing to do or should we fix endianness and content issues
		// that may be causing this not to match between the PC server and Xbox client?

		//
		// CRC Check
		//
		if ( userData && userData[0] == CONSISTENCY_EXACT && length == sizeof( ExactFileUserData ) )
		{
#if !defined( CSTRIKE15 )
			if ( !CheckCRCs( userData, length, filename ) )
			{
				ConColorMsg( red, "Bad CRC for %s\n", filename );
				V_strncpy( errorFilenameBuf, filename, sizeof( errorFilenameBuf ) );
			}
#endif
		}

		//
		// Bounds Check
		//
		// This is simply asking for the model's mins and maxs.  Also, it checks each material referenced
		// by the model, to make sure it doesn't ignore Z, isn't overbright, etc.
		//
		// TODO: Animations and facial expressions can still pull verts out past this.
		//
		else if ( userData && userData[0] == CONSISTENCY_BOUNDS && length == sizeof( ModelBoundsUserData ) )
		{
			ModelBoundsUserData *boundsData = (ModelBoundsUserData *)userData;
			model_t *pModel = modelloader->GetModelForName( filename, IModelLoader::FMODELLOADER_CLIENT );
			if ( !pModel )
			{
				ConColorMsg( red, "Can't find model for %s\n", filename );
				V_strncpy( errorFilenameBuf, filename, sizeof( errorFilenameBuf ) );
			}
			else
			{
				// [FTrepte] It seems that the boundsData is endian-swapped when connecting to the PC server in CClientState::ConsistencyCheck.
				if (IsX360())
				{
					CByteswap swap;
					swap.ActivateByteSwapping( true );

					float minx = boundsData->mins.x;
					swap.SwapBufferToTargetEndian<float>(&boundsData->mins.x, &minx, 1);

					float miny = boundsData->mins.y;
					swap.SwapBufferToTargetEndian<float>(&boundsData->mins.y, &miny, 1);

					float minz = boundsData->mins.z;
					swap.SwapBufferToTargetEndian<float>(&boundsData->mins.z, &minz, 1);

					float maxx = boundsData->maxs.x;
					swap.SwapBufferToTargetEndian<float>(&boundsData->maxs.x, &maxx, 1);

					float maxy = boundsData->maxs.y;
					swap.SwapBufferToTargetEndian<float>(&boundsData->maxs.y, &maxy, 1);

					float maxz = boundsData->maxs.z;
					swap.SwapBufferToTargetEndian<float>(&boundsData->maxs.z, &maxz, 1);
				}					

				if ( pModel->mins.x < boundsData->mins.x ||
					pModel->mins.y < boundsData->mins.y ||
					pModel->mins.z < boundsData->mins.z )
				{
					ConColorMsg( red, "Model %s exceeds mins (%.1f %.1f %.1f vs. %.1f %.1f %.1f)\n", filename,
						pModel->mins.x, pModel->mins.y, pModel->mins.z,
						boundsData->mins.x, boundsData->mins.y, boundsData->mins.z);
					V_strncpy( errorFilenameBuf, filename, sizeof( errorFilenameBuf ) );
				}
				if ( pModel->maxs.x > boundsData->maxs.x ||
					pModel->maxs.y > boundsData->maxs.y ||
					pModel->maxs.z > boundsData->maxs.z )
				{
					ConColorMsg( red, "Model %s exceeds maxs (%.1f %.1f %.1f vs. %.1f %.1f %.1f)\n", filename,
						pModel->maxs.x, pModel->maxs.y, pModel->maxs.z,
						boundsData->maxs.x, boundsData->maxs.y, boundsData->maxs.z);
					V_strncpy( errorFilenameBuf, filename, sizeof( errorFilenameBuf ) );
				}

				// Check each texture
				IMaterial *materials[ 128 ];
				int materialCount = Mod_GetModelMaterials( pModel, ARRAYSIZE( materials ), materials );

				for ( int j = 0; j<materialCount; ++j )
				{
					IMaterial *pMaterial = materials[j];

					if ( !CheckSimpleMaterial( pMaterial ) )
					{
						// Try reloading the material:
						pMaterial->RecomputeStateSnapshots();
						if ( !CheckSimpleMaterial( pMaterial ) )
						{						
							ConColorMsg( red, "Model %s has a bad texture %s\n", filename, pMaterial->GetName() );
							V_strncpy( errorFilenameBuf, filename, sizeof( errorFilenameBuf ) );
							break;
						}
					}
				}
			}
		}
	}

	if ( *errorFilenameBuf )
	{
		COM_ExplainDisconnection( true, "Server is enforcing consistency for this file:\n%s\n", errorFilenameBuf );
		Host_Error( "Server is enforcing file consistency for %s\n", errorFilenameBuf );
	}
}

void CClientState::UpdateAreaBits_BackwardsCompatible()
{
	if ( m_pAreaBits )
	{
		memcpy( m_chAreaBits, m_pAreaBits, sizeof( m_chAreaBits ) );
		
		// The whole point of adding this array was that the client could react to closed portals.
		// If they're using the old interface to set area portal bits, then we use the old 
		// behavior of assuming all portals are open on the clent.
		memset( m_chAreaPortalBits, 0xFF, sizeof( m_chAreaPortalBits ) );

		m_bAreaBitsValid = true;
	}
}


unsigned char** CClientState::GetAreaBits_BackwardCompatibility()
{
	return &m_pAreaBits;
}


void CClientState::RunFrame()
{
	CBaseClientState::RunFrame();

	// Since cl_rate is a virtualized cvar, make sure to pickup changes in it.
	if ( m_NetChannel )
		m_NetChannel->SetDataRate( cl_rate->GetFloat() );

	ConsistencyCheck( false );

	// Check if paged pool is low ( < 8% free )
	static bool s_bLowPagedPoolMemoryWarning = false;
	PAGED_POOL_INFO_t ppi;
	if ( ( SYSCALL_SUCCESS == Plat_GetPagedPoolInfo( &ppi ) ) &&
		( ( ppi.numPagesFree * 12 ) < ( ppi.numPagesUsed + ppi.numPagesFree ) ) )
	{
		con_nprint_t np;
		np.time_to_live = 1.0;
		np.index = 1;
		np.fixed_width_font = false;
		np.color[ 0 ] = 1.0;
		np.color[ 1 ] = 0.2;
		np.color[ 2 ] = 0.0;
		Con_NXPrintf( &np, "WARNING:  OS Paged Pool Memory Low" );

		// Also print a warning to console
		static float s_flLastWarningTime = 0.0f;
		if ( !s_bLowPagedPoolMemoryWarning ||
			 ( Plat_FloatTime() - s_flLastWarningTime > 3.0f ) )	// print a warning no faster than once every 3 sec
		{
			s_bLowPagedPoolMemoryWarning = true;
			s_flLastWarningTime = Plat_FloatTime();
			Warning( "OS Paged Pool Memory Low!\n" );
			Warning( "  Currently using %d pages (%d Kb) of total %d pages (%d Kb total)\n",
				ppi.numPagesUsed, ppi.numPagesUsed * Plat_GetMemPageSize(),
				( ppi.numPagesFree + ppi.numPagesUsed ), ( ppi.numPagesFree + ppi.numPagesUsed ) * Plat_GetMemPageSize() );
			Warning( "  Please see http://support.steampowered.com for more information.\n" );
		}
	}
	else if ( s_bLowPagedPoolMemoryWarning )
	{
		s_bLowPagedPoolMemoryWarning = false;
		Msg( "Info: OS Paged Pool Memory restored - currently %d pages free (%d Kb) of total %d pages (%d Kb total).\n",
			ppi.numPagesFree, ppi.numPagesFree * Plat_GetMemPageSize(),
			( ppi.numPagesFree + ppi.numPagesUsed ), ( ppi.numPagesFree + ppi.numPagesUsed ) * Plat_GetMemPageSize() );
	}

}
