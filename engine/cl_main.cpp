//===== Copyright  1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#include "client_pch.h"
#include "sound.h"
#include <inetchannel.h>
#include <time.h>
#include "checksum_engine.h"
#include "con_nprint.h"
#include "r_local.h"
#include "gl_lightmap.h"
#include "console.h"
#include "traceinit.h"
#include "cl_demo.h"
#include "cdll_engine_int.h"
#include "debugoverlay.h"
#include "filesystem_engine.h"
#include "icliententity.h"
#include "dt_recv_eng.h"
#include "vgui_baseui_interface.h"
#include "testscriptmgr.h"
#include <tier0/vprof.h>
#include <proto_oob.h>
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "gl_matsysiface.h"
#include "staticpropmgr.h"
#include "ispatialpartitioninternal.h"
#include "cbenchmark.h"
#include "vox.h"
#include "LocalNetworkBackdoor.h"
#include <tier0/icommandline.h>
#include "GameEventManager.h"
#include "host_saverestore.h"
#include "ivideomode.h"
#include "host_phonehome.h"
#include "decal.h"
#include "sv_rcon.h"
#include "cl_rcon.h"
#include "vgui_baseui_interface.h"
#include "snd_audio_source.h"
#include "iregistry.h"
#include "sys.h"
#include <vstdlib/random.h>
#include "tier0/etwprof.h"
#include "SteamInterface.h"
#include "sys_dll.h"
#include "avi/iavi.h"
#include "cl_steamauth.h"
#include "filesystem/IQueuedLoader.h"
#include "matchmaking/imatchframework.h"
#include "tier2/tier2.h"
#include "host_state.h"
#include "enginethreads.h"
#include "vgui/ISystem.h"
#include "pure_server.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "LoadScreenUpdate.h"
#include "tier0/systeminformation.h"
#include "steam/steam_api.h"
#include "SourceAppInfo.h"
#include "cl_texturelistpanel.h"
#include "enginethreads.h"
#include "tier1/characterset.h"
#include "const.h"
#include "audio/private/snd_sfx.h"
#include "MapReslistGenerator.h"

#ifdef _X360
#include "xbox/xbox_launch.h"
#endif
#if defined( REPLAY_ENABLED )
#include "replayhistorymanager.h"
#endif

#include "ConfigManager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IVEngineClient *engineClient;

void R_UnloadSkys( void );
void CL_ResetEntityBits( void );
void EngineTool_UpdateScreenshot();

// If we get more than 250 messages in the incoming buffer queue, dump any above this #
#define MAX_INCOMING_MESSAGES		250
// Size of command send buffer
#define MAX_CMD_BUFFER				4000

CGlobalVarsBase g_ClientGlobalVariables( true );
AVIHandle_t g_hCurrentAVI = AVIHANDLE_INVALID;

extern ConVar rcon_password;
extern ConVar host_framerate;
extern ConVar cl_clanid;

ConVar sv_unlockedchapters( "sv_unlockedchapters", "1", FCVAR_ARCHIVE, "Highest unlocked game chapter." );

static ConVar tv_nochat	( "tv_nochat", "0", FCVAR_ARCHIVE | FCVAR_USERINFO, "Don't receive chat messages from other GOTV spectators" );
// ZOID:  Disabled cl_LocalNetworkBackdoor from the cell optimization code, Dussault is going to fix this later
static ConVar cl_LocalNetworkBackdoor( "cl_localnetworkbackdoor", "1", 0, "Enable network optimizations for single player games." );
static ConVar cl_ignorepackets( "cl_ignorepackets", "0", FCVAR_CHEAT, "Force client to ignore packets (for debugging)." );
static ConVar cl_playback_screenshots( "cl_playback_screenshots", "0", 0, "Allows the client to playback screenshot and jpeg commands in demos." );

// Exposing connectivity trouble as a convar so we can display info in the client
ConVar cl_connection_trouble_info( "cl_connection_trouble_info", "", FCVAR_HIDDEN, "How long until we timeout on our network connection because of connectivity loss (empty if no problem)" );

MovieInfo_t cl_movieinfo;

// FIXME: put these on hunk?
dlight_t		cl_dlights[MAX_DLIGHTS];
dlight_t		cl_elights[MAX_ELIGHTS];
CFastPointLeafNum g_DLightLeafAccessors[MAX_DLIGHTS];
CFastPointLeafNum g_ELightLeafAccessors[MAX_ELIGHTS];
int	g_ActiveDLightIndex[MAX_DLIGHTS];
int g_ActiveELightIndex[MAX_ELIGHTS];
int g_nNumActiveDLights = 0;
int g_nNumActiveELights = 0;

extern bool g_bClearingClientState;

bool cl_takesnapshot = false;
bool cl_takejpeg = false;

static int cl_jpegquality = DEFAULT_JPEG_QUALITY;
static ConVar jpeg_quality( "jpeg_quality", "90", 0, "jpeg screenshot quality." );

static int	cl_snapshotnum = 0;
static char cl_snapshotname[MAX_OSPATH];
static char cl_snapshot_subdirname[MAX_OSPATH];
char cl_snapshot_fullpathname[MAX_OSPATH];

static ConVar cl_retire_low_priority_lights( "cl_retire_low_priority_lights", "0", 0, "Low priority dlights are replaced by high priority ones" );

// Must match game .dll definition
// HACK HACK FOR E3 -- Remove this after E3
#define	HIDEHUD_ALL			( 1<<2 )



// 
// This is called when a client receives the whitelist from a pure server (on map change).
// Each pure server (and each map on the server) has a whitelist that says which files a 
// client is allowed to load off disk. When the client gets the whitelist, it must
// flush out any files that it has loaded previously that were NOT in the Steam cache.
//
// -- pseudocode --
// for all loaded resources (models/sounds/materials/scripts)
//	 for each file related to this resource
//     if (file is not in whitelist)
//       if (file was loaded off disk instead of coming from the Steam cache)
//         flush the file
//
// Note: It could also check in here that the on-disk file is actually different 
//       than the Steam one. If it happens to have the same CRC, then there's no need
//       to do all the flushing.
//
void CL_HandlePureServerWhitelist( CPureServerWhitelist *pWhitelist )
{
	// Free the old whitelist and get the new one.
	if ( GetBaseLocalClient().m_pPureServerWhitelist )
		GetBaseLocalClient().m_pPureServerWhitelist->Release();
		
	GetBaseLocalClient().m_pPureServerWhitelist = pWhitelist;
	
	IFileList *pForceMatchList = NULL;
	IFileList *pAllowFromDiskList = NULL;

	if ( pWhitelist )
	{
		pForceMatchList = pWhitelist->GetForceMatchList();
		pAllowFromDiskList = pWhitelist->GetAllowFromDiskList();
	}
	
	if ( !IsPC() )
	{
		if ( pForceMatchList )
			pForceMatchList->Release();
		
		if ( pAllowFromDiskList )
			pAllowFromDiskList->Release();
		
		return;
	}

	// we wont reload any files.
	IFileList *pFilesToReload;
	g_pFileSystem->RegisterFileWhitelist( pForceMatchList, pAllowFromDiskList, &pFilesToReload );

	GetBaseLocalClient().m_bCheckCRCsWithServer = true;
}

void CL_PrintWhitelistInfo()
{
	if ( GetBaseLocalClient().m_pPureServerWhitelist )
	{
		if ( GetBaseLocalClient().m_pPureServerWhitelist->IsInFullyPureMode() )
		{
			Msg( "The server is using sv_pure = 2.\n" );
		}
		else
		{
			Msg( "The server is using sv_pure = 1.\n" );
			GetBaseLocalClient().m_pPureServerWhitelist->PrintWhitelistContents();
		}
	}
	else
	{		
		Msg( "The server is using sv_pure = 0 (no whitelist).\n" );
	}	
}

// Console command to force a whitelist on the system.
#ifdef _DEBUG
void whitelist_f( const CCommand &args )
{
	int pureLevel = 2;
	if ( args.ArgC() == 2 )
	{
		pureLevel = atoi( args[1] );
	}
	else
	{
		Warning( "Whitelist 0, 1, or 2\n" );
	}

	if ( pureLevel == 0 )
	{
		Warning( "whitelist 0: CL_HandlePureServerWhitelist( NULL )\n" );
		CL_HandlePureServerWhitelist( NULL );
	}
	else
	{
		CPureServerWhitelist *pWhitelist = CPureServerWhitelist::Create( g_pFileSystem );
		if( pureLevel == 2 )
		{
			Warning( "whitelist 2: pWhitelist->EnableFullyPureMode()\n" );
			pWhitelist->EnableFullyPureMode();
		}
		else
		{
			Warning( "whitelist 1: loading pure_server_whitelist.txt\n" );
			KeyValues *kv = new KeyValues( "" );
			bool bLoaded = kv->LoadFromFile( g_pFileSystem, "pure_server_whitelist.txt", "game" );
			if ( bLoaded )
				bLoaded = pWhitelist->LoadFromKeyValues( kv );

			if ( !bLoaded )
				Warning( "Error loading pure_server_whitelist.txt\n" );
			
			kv->deleteThis();
		}
		
		CL_HandlePureServerWhitelist( pWhitelist );
		pWhitelist->Release();
	}
}
ConCommand whitelist( "whitelist", whitelist_f );
#endif


const CPrecacheUserData* CL_GetPrecacheUserData( INetworkStringTable *table, int index )
{
	int testLength;
	const CPrecacheUserData *data = ( CPrecacheUserData * )table->GetStringUserData( index, &testLength );
	if ( data )
	{
		ErrorIfNot( 
			testLength == sizeof( *data ),
			("CL_GetPrecacheUserData(%d,%d) - length (%d) invalid.", table->GetTableId(), index, testLength)
		);

	}
	return data;
}


//-----------------------------------------------------------------------------
// Purpose: setup the demo flag, split from CL_IsHL2Demo so CL_IsHL2Demo can be inline
//-----------------------------------------------------------------------------
static bool s_bIsHL2Demo = false;
void CL_InitHL2DemoFlag()
{
#if defined(_GAMECONSOLE)
	s_bIsHL2Demo = false;
#else
	static bool initialized = false;
	if ( !initialized )
	{
#ifndef NO_STEAM
		if ( Steam3Client().SteamApps() && !Q_stricmp( COM_GetModDirectory(), "hl2" ) && g_pFileSystem->IsSteam() )
		{
			initialized = true;

			// if user didn't buy HL2 yet, this must be the free demo
			s_bIsHL2Demo = !Steam3Client().SteamApps()->BIsSubscribedApp( GetAppSteamAppId( k_App_HL2 ) );
		}
#endif

		if ( !Q_stricmp( COM_GetModDirectory(), "hl2" ) && CommandLine()->CheckParm( "-demo" ) ) 
		{
			s_bIsHL2Demo = true;
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the user is playing the HL2 Demo (rather than the full game)
//-----------------------------------------------------------------------------
bool CL_IsHL2Demo()
{
	CL_InitHL2DemoFlag();
	return s_bIsHL2Demo;
}

static bool s_bIsPortalDemo = false;
void CL_InitPortalDemoFlag()
{
#if defined(_GAMECONSOLE) || defined( NO_STEAM )
	s_bIsPortalDemo = false;
#else
	static bool initialized = false;
	if ( !initialized )
	{
		if ( Steam3Client().SteamApps() && !Q_stricmp( COM_GetModDirectory(), "portal" ) && g_pFileSystem->IsSteam() )
		{
			initialized = true;
		
			// if user didn't buy Portal yet, this must be the free demo
			s_bIsPortalDemo = !Steam3Client().SteamApps()->BIsSubscribedApp( GetAppSteamAppId( k_App_PORTAL ) );
		}
		
		if ( !Q_stricmp( COM_GetModDirectory(), "portal" ) && CommandLine()->CheckParm( "-demo" ) ) 
		{
			s_bIsPortalDemo = true;
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Returns true if the user is playing the Portal Demo (rather than the full game)
//-----------------------------------------------------------------------------
bool CL_IsPortalDemo()
{
	CL_InitPortalDemoFlag();
	return s_bIsPortalDemo;
}


//-----------------------------------------------------------------------------
// Purpose: If the client is in the process of connecting and the GetBaseLocalClient().signon hits
//  is complete, make sure the client thinks its totally connected.
//-----------------------------------------------------------------------------
void CL_CheckClientState( void )
{
	// Setup the local network backdoor (we do this each frame so it can be toggled on and off).
	bool useBackdoor = cl_LocalNetworkBackdoor.GetInt() && 
						(GetBaseLocalClient().m_NetChannel ? GetBaseLocalClient().m_NetChannel->IsLoopback() : false) &&
						sv.IsActive() &&
						!demorecorder->IsRecording() &&
						!demoplayer->IsPlayingBack() &&
						Host_IsSinglePlayerGame();

	CL_SetupLocalNetworkBackDoor( useBackdoor );
}



//-----------------------------------------------------------------------------
// bool CL_CheckCRCs( const char *pszMap )
//-----------------------------------------------------------------------------
bool CL_CheckCRCs( const char *pszMap )
{
	if ( IsGameConsole() )
	{
		// Console does not need to CRC map/dlls (slows loading), closed data system
		return true;
	}

	// If we are on PC cross-playing with a console, then don't perform CRC check
	if ( !GetBaseLocalClient().serverCRC && !GetBaseLocalClient().serverClientSideDllCRC )
		return true;

	VPROF_BUDGET( "CL_CheckCRCs", VPROF_BUDGETGROUP_STEAM );

	CRC32_t mapCRC;        // If this is the worldmap, CRC agains server's map

	// Don't verify CRC if we are running a local server (i.e., we are playing single player, or we are the server in multiplay
	if ( sv.IsActive() ) // Single player
		return true;

	CRC32_Init(&mapCRC);
	if ( !CRC_MapFile( &mapCRC, pszMap ) )
	{
		// Does the file exist?
		FileHandle_t fp = 0;
		int nSize = -1;

		nSize = COM_OpenFile( pszMap, &fp );
		if ( fp )
			g_pFileSystem->Close( fp );

		if ( nSize != -1 )
		{
			COM_ExplainDisconnection( true, "Couldn't CRC map %s, disconnecting\n", pszMap);
		}
		else
		{   
			COM_ExplainDisconnection( true, "Missing map %s, disconnecting\n", pszMap);
		}

		Host_Error( "Disconnected" );
		return false;
	}

	// Hacked map
	if ( GetBaseLocalClient().serverCRC != mapCRC )
	{
		if ( demoplayer && demoplayer->IsPlayingBack() )
		{
			Msg( "Client map CRC mismatch, %lu local != %lu demo.\n", mapCRC, GetBaseLocalClient().serverCRC );
		}
		else
		{
			COM_ExplainDisconnection( true, "Your map [%s] differs from the server's.\n", pszMap );
			Host_Error( "Disconnected" );
			return false;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : nMaxClients - 
//-----------------------------------------------------------------------------
void CL_ReallocateDynamicData( int maxclients )
{
	Assert( entitylist );
	if ( entitylist )
	{
		entitylist->SetMaxEntities( MAX_EDICTS );
	}
}

/*
=================
CL_ReadPackets

Updates the local time and reads/handles messages on client net connection.
=================
*/
ConVar net_earliertempents( "net_earliertempents", "0", FCVAR_CHEAT );
void CL_ReadPackets ( bool bFinalTick )
{
	VPROF_BUDGET( "CL_ReadPackets", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	// Before parsing any messages, assume we're in split player "slot 0"
	splitscreen->SetActiveSplitScreenPlayerSlot( 0 );
	splitscreen->SetLocalPlayerIsResolvable( __FILE__, __LINE__, false );

	if ( !Host_ShouldRun() )
		return;
	
	splitscreen->SetLocalPlayerIsResolvable( __FILE__, __LINE__, true );

	CClientState &cl = GetBaseLocalClient();

	// If we're fully connected, but still showing loading plaque, tick it once per frame
	if ( cl.IsActive() && scr_drawloading )
	{
		EngineVGui()->UpdateProgressBar( PROGRESS_DEFAULT );
	}

	// update client times/tick
	cl.oldtickcount = cl.GetServerTickCount();
	if ( !cl.IsPaused() )
	{
		cl.SetClientTickCount( cl.GetClientTickCount() + 1 );
		
		// While clock correction is off, we have the old behavior of matching the client and server clocks.
		if ( !CClockDriftMgr::IsClockCorrectionEnabled() )
			cl.SetServerTickCount( cl.GetClientTickCount() );

		g_ClientGlobalVariables.tickcount = cl.GetClientTickCount();
		g_ClientGlobalVariables.curtime = cl.GetTime();
	}
	// 0 or tick_rate if simulating
	g_ClientGlobalVariables.frametime = cl.GetFrameTime();

	// read packets, if any in queue
	if ( demoplayer->IsPlayingBack() && cl.m_NetChannel )
	{
		// process data from demo file
		cl.m_NetChannel->ProcessPlayback();
	}
	else
	{
		if ( !cl_ignorepackets.GetInt() )
		{
			// process data from net socket
			NET_ProcessSocket( NS_CLIENT, &cl );

			if( net_earliertempents.GetBool() )
			{
				CL_FireEvents();
			}
		}
	}

	// check timeout, but not if running _DEBUG engine
	bool bAllowTimeout = true;
#if defined( _DEBUG )
	bAllowTimeout = false;
#endif

	// Only check on final frame because that's when the server might send us a packet in single player.  This avoids
	//  a bug where if you sit in the game code in the debugger then you get a timeout here on resuming the engine
	//  because the timestep is > 1 tick because of the debugging delay but the server hasn't sent the next packet yet.  ywb 9/5/03
	if (  bFinalTick &&
		 !demoplayer->IsPlayingBack() &&
		 cl.IsConnected() )
	{
		bool bDisconnected = false;

		if ( cl.m_NetChannel )
		{
			if ( bAllowTimeout && cl.m_NetChannel->IsTimedOut() )
			{
				bDisconnected = true;
				ConMsg ("\nServer connection timed out.\n");

				// Show the vgui dialog on timeout
				COM_ExplainDisconnection( false, "Connection to server timed out.");
			}
			// Check for Steam mediated socket disconnection
			else if ( cl.m_NetChannel->IsRemoteDisconnected() )
			{
				bDisconnected = true;
				ConMsg ("\nServer shutting down\n");

				// Show the vgui dialog on timeout
				COM_ExplainDisconnection( false, "Server shutting down");
			}
		}

		if ( bDisconnected )
		{
			if ( IsPC() )
			{
				EngineVGui()->ShowErrorMessage();
			}
	
			Host_Disconnect (true);
			return;
		}
	}

	splitscreen->SetLocalPlayerIsResolvable( __FILE__, __LINE__, false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CL_ClearState ( void )
{
	// clear out the current whitelist
	CL_HandlePureServerWhitelist( NULL );

	CL_TextureListPanel_ClearState();

	CL_ResetEntityBits();

	R_UnloadSkys();

	// clear decal index directories
	Decal_Init();

	StaticPropMgr()->LevelShutdownClient();

	// shutdown this level in the client DLL
	if ( g_ClientDLL )
	{
		if ( host_state.worldmodel )
		{
			char mapname[256];
			CL_SetupMapName( modelloader->GetName( host_state.worldmodel ), mapname, sizeof( mapname ) );
			phonehome->Message( IPhoneHome::PHONE_MSG_MAPEND, mapname );
		}
		audiosourcecache->LevelShutdown();
		g_ClientDLL->LevelShutdown();
	}

	R_LevelShutdown();
	
	if ( g_pLocalNetworkBackdoor )
		g_pLocalNetworkBackdoor->ClearState();

	// clear other arrays	
	memset (cl_dlights, 0, sizeof(cl_dlights));
	memset (cl_elights, 0, sizeof(cl_elights));
	g_bActiveDlights = false;
	g_bActiveElights = false;
	r_dlightchanged = 0;
	r_dlightactive = 0;

	int i;
	for ( i=0; i<MAX_DLIGHTS; ++i )
	{
		g_DLightLeafAccessors[i].Reset();
	}

	for ( i=0; i<MAX_ELIGHTS; ++i )
	{
		g_ELightLeafAccessors[i].Reset();
	}

	g_bClearingClientState = true;

	// Wipe the hunk ( unless the server is active )
	// Make sure world is set if we have the models stringtable
	// This fixes a bug where you would fail to connect to a server due to, e.g., sv_consistency failure, 
	// The client would Host_Error.  The CM_FreeMap would have been called, but the worldmodel itself would not have
	//  been unreferenced, which would make it possible to not load the collision data for the next connection, causing a crash.
	model_t *pWorldModel = GetBaseLocalClient().GetModel( 1 );
	if ( pWorldModel && !host_state.worldmodel )
	{
		host_state.SetWorldModel( pWorldModel );
	}

	g_bClearingClientState = false;

	Host_FreeStateAndWorld( false );
	Host_FreeToLowMark( false );

	// Wipe the remainder of the structure.
	GetBaseLocalClient().Clear();
}

//-----------------------------------------------------------------------------
// Purpose: Used for sorting sounds
// Input  : &sound1 - 
//			&sound2 - 
// Output : static bool
//-----------------------------------------------------------------------------
static bool CL_SoundMessageLessFunc( SoundInfo_t const &sound1, SoundInfo_t const &sound2 )
{
	return sound1.nSequenceNumber < sound2.nSequenceNumber;
}

static CUtlRBTree< SoundInfo_t, int > g_SoundMessages( 0, 0, CL_SoundMessageLessFunc );
#ifndef LINUX
extern ConVar snd_show;
#endif

//-----------------------------------------------------------------------------
// Purpose: Add sound to queue
// Input  : sound - 
//-----------------------------------------------------------------------------
void CL_AddSound( const SoundInfo_t &sound )
{
	g_SoundMessages.Insert( sound );
}

void CL_SndShow( const char *pName, const SoundInfo_t &pSound )
{

#ifndef LINUX
	if ( snd_show.GetInt() >= 2 )
	{
		DevMsg( "%i (seq %i) %s : src %d : ch %d : %d dB : vol %.2f : time %.3f (%.4f delay) @%.1f %.1f %.1f\n", 
			host_framecount,
			pSound.nSequenceNumber,
			pName,
			pSound.nEntityIndex, 
			pSound.nChannel, 
			pSound.Soundlevel, 
			pSound.fVolume, 
			GetBaseLocalClient().GetTime(),
			pSound.fDelay,
			pSound.vOrigin.x,
			pSound.vOrigin.y,
			pSound.vOrigin.z );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Play sound packet
// Input  : sound - 
//-----------------------------------------------------------------------------
void CL_DispatchSound( const SoundInfo_t &sound )
{

	StartSoundParams_t params;

	// we always want to do this when this flag is set - even if the delay is zero we need to precisely
	// schedule this sound
	if ( sound.nFlags & SND_DELAY )
	{
		// anything adjusted less than 100ms forward was probably scheduled this frame
		if ( fabs(sound.fDelay) < 0.100f )
		{
			float soundtime = GetBaseLocalClient().m_flLastServerTickTime + sound.fDelay;
			// this adjusts for host_thread_mode or any other cases where we're running more than one
			// tick at a time, but we get network updates on the first tick
			soundtime -= ((g_ClientGlobalVariables.simTicksThisFrame-1) * host_state.interval_per_tick);
#if 0
			static float lastSoundTime = 0;
			Msg("[%.3f] Play %s at %.3f\n", soundtime - lastSoundTime, name, soundtime );
			lastSoundTime = soundtime;
#endif
			// this sound was networked over from the server, use server clock
			params.delay = S_ComputeDelayForSoundtime( soundtime, CLOCK_SYNC_SERVER );
			if ( params.delay < 0 )
			{
				params.delay = 0;
			}
		}
		else
		{
			params.delay = sound.fDelay;
		}
	}


	// copy emitter params
	params.staticsound = (sound.nChannel == CHAN_STATIC) ? true : false;
	params.soundsource = sound.nEntityIndex;
	params.entchannel = params.staticsound ? CHAN_STATIC : sound.nChannel;
	params.origin = sound.vOrigin;
	params.fvol = sound.fVolume;
	params.soundlevel = sound.Soundlevel;
	params.flags = sound.nFlags;
	params.pitch = sound.nPitch;
	params.fromserver = true;
	params.delay = sound.fDelay;
	params.speakerentity = sound.nSpeakerEntity;
	params.m_bIsScriptHandle = ( sound.nFlags & SND_IS_SCRIPTHANDLE ) ? true : false ;

	// handle soundentries separately
	if ( params.m_bIsScriptHandle )
	{
		// Don't actually play sounds if playing a demo and skipping ahead
		// but always stop sounds
		if ( demoplayer->IsSkipping() && !(sound.nFlags&SND_STOP) )
		{
			return;
		}
		params.m_nSoundScriptHash = ( HSOUNDSCRIPTHASH ) sound.nSoundNum;
		S_StartSoundEntry( params, sound.nRandomSeed, false );
		return;
	}

	// get actual soundfile for old style
	CSfxTable *pSfx;
	char name[ MAX_QPATH ];
	name[ 0 ] = 0;
	if ( sound.bIsSentence )
	{
		// make dummy sfx for sentences
		const char *pSentenceName = VOX_SentenceNameFromIndex( sound.nSoundNum );
		if ( !pSentenceName )
		{
			pSentenceName = "";
		}
		Q_snprintf( name, sizeof( name ), "%c%s", CHAR_SENTENCE, pSentenceName );
		pSfx = S_DummySfx( name );
	}
	else
	{
		pSfx = GetBaseLocalClient().GetSound( sound.nSoundNum );
		if ( ( pSfx != NULL ) && pSfx->m_bIsLateLoad )
		{
			DevMsg("    Entity '%d' created the late load.\n", sound.nEntityIndex );
		}
		Q_strncpy( name, GetBaseLocalClient().GetSoundName( sound.nSoundNum ), sizeof( name ) );
	}
	params.pSfx = pSfx;

	CL_SndShow( name, sound );
	
	// Don't actually play sounds if playing a demo and skipping ahead
	// but always stop sounds
	if ( demoplayer->IsSkipping() && !(sound.nFlags&SND_STOP) )
	{
		return;
	}
	S_StartSound( params );
}

//-----------------------------------------------------------------------------
// Purpose: Called after reading network messages to play sounds encoded in the network packet
//-----------------------------------------------------------------------------
void CL_DispatchSounds( void )
{
	int i;
	// Walk list in sequence order
	i = g_SoundMessages.FirstInorder();
	while ( i != g_SoundMessages.InvalidIndex() )
	{
		SoundInfo_t const *msg = &g_SoundMessages[ i ];
		Assert( msg );
		if ( msg )
		{
			// Play the sound
			CL_DispatchSound( *msg );
		}
		i = g_SoundMessages.NextInorder( i );
	}

	// Reset the queue each time we empty it!!!
	g_SoundMessages.RemoveAll();
}


//-----------------------------------------------------------------------------
// Retry last connection (e.g., after we enter a password)
//-----------------------------------------------------------------------------
void CL_Retry()
{
	CClientState &cl =GetBaseLocalClient();
	if ( cl.m_Remote.Count() <= 0 )
	{
		ConMsg( "Can't retry, no previous connection\n" );
		return;
	}

	ConMsg( "Commencing connection retry to " );
	CUtlString cnx;
	for ( int i = 0; i < cl.m_Remote.Count(); ++i )
	{
		const Remote_t &remote = cl.m_Remote.Get( i );
		ConMsg( "%s(%s)", remote.m_szAlias.String(), remote.m_szRetryAddress.String() );
		//cnx += va( "\"%s\" ", remote.m_szRetryAddress.String() );
		cnx += va( "%s ", remote.m_szRetryAddress.String() );
	}
	ConMsg( "\n" );

	Cbuf_AddText( Cbuf_GetCurrentPlayer(), va( "connect %s\n", cnx.String() ) );
}

//REMOVED FOR PORTAL2:
CON_COMMAND_F( retry, "Retry connection to last server.", FCVAR_DONTRECORD | FCVAR_SERVER_CAN_EXECUTE | FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	CL_Retry();
}


void SplitString( char const *pchString, CUtlVector< CUtlString > &list )
{
	characterset_t breakOnSpaces;
	CharacterSetBuild( &breakOnSpaces, " " );

	char token[ 1024 ] = { 0 };

	const char *pIn = pchString;
	char *pOut = token;
	while ( *pIn && 
		( ( pOut - token ) < sizeof( token ) - 1 ) )
	{
		if ( IN_CHARACTERSET( breakOnSpaces, *pIn ) )
		{
			*pOut = 0;
			list.AddToTail( CUtlString( token ) );
			pOut = token;
			// Skip the space
			++pIn;
			continue;
		}
		*pOut++ = *pIn++;
	}

	*pOut = 0;
	if ( Q_strlen( token ) > 0 )
	{
		list.AddToTail( CUtlString( token ) );
	}
}

/*
=====================
CL_Connect_f

User command to connect to server
=====================
*/
CON_COMMAND_F( connect, "Connect to specified server.", FCVAR_DONTRECORD )
{
	if ( args.ArgC() < 2 )
	{
		ConMsg( "Usage:  connect <server>\n" );
		return;
	}

	if ( !net_time && !NET_IsMultiplayer() )
	{
		ConMsg( "Deferring connect command!\n" );
		return;
	}

	char const *pchArgs = args.ArgS();
	CUtlVector< CUtlString > argValues;
	SplitString( pchArgs, argValues );

	if ( argValues.Count() != 1 && 
		argValues.Count() != 2 &&
		argValues.Count() != 3 )
	{
		ConMsg( "connect:  can't parse '%s'\n", pchArgs );
		return;
	}
	
	// If it's not a single player connection to "localhost", initialize networking & stop listenserver
	if ( !StringHasPrefixCaseSensitive( argValues[ 0 ].String(), "localhost" ) )
	{
		Host_Disconnect(false);	

		// allow remote
		NET_SetMultiplayer( true );		

		// start progress bar immediately for remote connection
		EngineVGui()->EnabledProgressBarForNextLoad();

		SCR_BeginLoadingPlaque();

		EngineVGui()->UpdateProgressBar(PROGRESS_BEGINCONNECT);
	}
	else
	{
		// we are connecting/reconnecting to local game
		// so don't stop listenserver 
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
			GetLocalClient().Disconnect( false );
		}
	}
	

	// When using '+connect ip:port' in a Steam URL 'steam://run/730//+connect ip:port', we can't use the : character. 
	// Support using '?' instead and replace all '?' with ':' in the string.

	char strAddress[2][64];
	int addressCount = 0;

	const char* szJoinType = "UnknownJoinType";
	for ( int i = 0; i < argValues.Count(); i++ )
	{
		// Any params starting with dash are join type
		if ( argValues[ i ].String()[0] == '-' && argValues[ i ].String()[1] )
		{
			szJoinType = argValues[ i ].String()+1;
		}
		else
		{
			if ( addressCount >= 2 )
				continue; 

			V_strncpy( strAddress[ i ], argValues[ i ].String(), sizeof( strAddress[ i ] ));
			char* qmindex = strAddress[ i ];

			addressCount++;
			while ( ( qmindex = strchr( qmindex ,'?') ) != NULL )
			{
				*qmindex = ':';
			}
		}
	}
	
	

	GetBaseLocalClient().Connect( strAddress[ 0 ], addressCount == 2 ? strAddress[ 1 ] : strAddress[ 0 ], szJoinType );

	// Reset error conditions
	gfExtendedError = false;
}

/*
=====================
CL_Connect_SplitScreen_f

User command to connect to server with multiple players
=====================
*/
CON_COMMAND_F( connect_splitscreen, "Connect to specified server. With multiple players.", FCVAR_DONTRECORD | FCVAR_RELEASE | FCVAR_HIDDEN )
{
	if ( args.ArgC() < 3 )
	{
		ConMsg( "Usage:  connect <server> <# of players>\n" );
		return;
	}

	char const *pchArgs = args.ArgS();
	CUtlVector< CUtlString > argValues;
	SplitString( pchArgs, argValues );

	if ( argValues.Count() != 2 && 
		argValues.Count() != 3 )
	{
		ConMsg( "connect_splitscreen:  can't parse '%s'\n", pchArgs );
		return;
	}

	int last = argValues.Count() - 1;

	int numPlayers = Q_atoi( argValues[ last ].String() );
	if ( numPlayers <= 0 )
	{
		ConMsg( "Must have at least one player.\n" );
		return;
	}
	if ( numPlayers > host_state.max_splitscreen_players )
	{
		ConMsg( "Too many players\n" );
		return;
	}	

	

	// If it's not a single player connection to "localhost", initialize networking & stop listenserver
	if ( !StringHasPrefixCaseSensitive( argValues[ 0 ].String(), "localhost" ) )
	{
		if ( !IsGameConsole() )
			return;

		Host_Disconnect(false);	

		// allow remote
		NET_SetMultiplayer( true );		

		// start progress bar immediately for remote connection
		EngineVGui()->EnabledProgressBarForNextLoad();

		SCR_BeginLoadingPlaque();

		EngineVGui()->UpdateProgressBar(PROGRESS_BEGINCONNECT);
	}
	else
	{
		// we are connecting/reconnecting to local game
		// so don't stop listenserver 
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
			GetLocalClient().Disconnect( false );
		}
	}

	const char* szJoinType = "UnknownJoinType";
	for ( int i = 0; i < argValues.Count(); i++ )
	{
		// Any params starting with dash are join type
		if ( argValues[ i ].String()[0] == '-' && argValues[ i ].String()[1] )
		{
			szJoinType = argValues[ i ].String()+1;
		}
	}

	GetBaseLocalClient().ConnectSplitScreen( argValues[ 0 ].String(), argValues.Count() == 3 ? argValues[ 1 ].String() : argValues[ 0 ].String(), numPlayers, szJoinType );

	// Reset error conditions
	gfExtendedError = false;
}

#if defined( _X360 ) && !defined( _CERT )
//-----------------------------------------------------------------------------
// Caller must provide secure address string.  Establishes connection.
// Returns TRUE if successful. Non-shipping development helper only.
//-----------------------------------------------------------------------------
static bool XLSP_StartConnection( const char *pXLSPAddress, char *pOutAddress, int outAddressSize, unsigned int timeout = 10 * 1000 )
{
	// remove the possible port
	int nPort = -1;
	char cleanAddress[128];
	V_strncpy( cleanAddress, pXLSPAddress, sizeof( cleanAddress ) );
	char *pPort = strchr( cleanAddress, ':' );
	if ( pPort )
	{
		*pPort++ = '\0';
		if ( pPort[0] )
		{
			nPort = atoi( pPort );
		}
	}

	IN_ADDR xlspAddress;
	int ip4[4];
	sscanf( cleanAddress, "%d.%d.%d.%d", &ip4[0], &ip4[1], &ip4[2], &ip4[3] );
	xlspAddress.S_un.S_un_b.s_b1 = ip4[0];
	xlspAddress.S_un.S_un_b.s_b2 = ip4[1];
	xlspAddress.S_un.S_un_b.s_b3 = ip4[2];
	xlspAddress.S_un.S_un_b.s_b4 = ip4[3];

	// track and free last known secure address
	static IN_ADDR s_lastSecureAddress;
	if ( s_lastSecureAddress.S_un.S_addr != 0x0 )
	{
		XNetUnregisterInAddr( s_lastSecureAddress );
		s_lastSecureAddress.S_un.S_addr = 0x0;
	}

	IN_ADDR secureAddress;
	DWORD dwResult = XNetServerToInAddr( xlspAddress, g_pMatchFramework->GetMatchTitle()->GetTitleServiceID(), &secureAddress );
	if ( dwResult != ERROR_SUCCESS )
	{
		Warning( "Failed to resolve XLSP secure address for %d.%d.%d.%d\n", 
			xlspAddress.S_un.S_un_b.s_b1,
			xlspAddress.S_un.S_un_b.s_b2,
			xlspAddress.S_un.S_un_b.s_b3,
			xlspAddress.S_un.S_un_b.s_b4 );
		return false;
	}

	s_lastSecureAddress = secureAddress;

	Msg( "Attempting connection to XLSP title server using, XLSP:%d.%d.%d.%d -> Secure:%d.%d.%d.%d\n", 
		xlspAddress.S_un.S_un_b.s_b1,
		xlspAddress.S_un.S_un_b.s_b2,
		xlspAddress.S_un.S_un_b.s_b3,
		xlspAddress.S_un.S_un_b.s_b4,
		secureAddress.S_un.S_un_b.s_b1,
		secureAddress.S_un.S_un_b.s_b2,
		secureAddress.S_un.S_un_b.s_b3,
		secureAddress.S_un.S_un_b.s_b4 );

	// Connect to server
	int iResult;
	if ( ( iResult = XNetConnect( secureAddress ) ) != 0 )
	{
		Warning( "XNetConnect: failed with %d.\n", iResult );
		return false;
	}

	// Wait for the SG connection to complete
	unsigned long startTime = Plat_MSTime();
	while ( ( dwResult = XNetGetConnectStatus( secureAddress ) ) == XNET_CONNECT_STATUS_PENDING )
	{
		if ( ( Plat_MSTime() - startTime ) > timeout )
		{
			Warning( "XNetConnect: Timeout, took longer than %ums to complete.\n", timeout );
			return false;
		}
		Sleep( 100 );
	}

	if ( dwResult != XNET_CONNECT_STATUS_CONNECTED )
	{
		Warning( "XNetGetConnectStatus: connection failed with %d.\n", dwResult );
		return false;
	}

	if ( nPort != -1 )
	{
		V_snprintf( 
			pOutAddress, 
			outAddressSize, 
			"%d.%d.%d.%d:%d",
			secureAddress.S_un.S_un_b.s_b1,
			secureAddress.S_un.S_un_b.s_b2,
			secureAddress.S_un.S_un_b.s_b3,
			secureAddress.S_un.S_un_b.s_b4,
			nPort );
	} 
	else
	{
		V_snprintf( 
			pOutAddress,
			outAddressSize,
			"%d.%d.%d.%d",
			secureAddress.S_un.S_un_b.s_b1,
			secureAddress.S_un.S_un_b.s_b2,
			secureAddress.S_un.S_un_b.s_b3,
			secureAddress.S_un.S_un_b.s_b4 );
	}

	Msg( "Connected to XLSP title server.\n" );
	return true;
}
#endif

#if defined( _X360 ) && !defined( _CERT )
//-----------------------------------------------------------------------------
// Assume non-numeric name string address represents XLSP because system link
// can only be IP4. Resolve name to secure IP4.
// Returns -1 if error, Returns 0 if not XLSP, Returns 1 if valid.
// Non-shipping development helper only.
//-----------------------------------------------------------------------------
static int XLSP_NameToAddress( const char *pAddress, char *pOutAddress, int outAddressSize )
{
	// want string names, but not this one
	if ( StringHasPrefixCaseSensitive( pAddress, "localhost" ) )
	{
		return 0;
	}

	// do not use DNS, not trying to resolve the name
	netadr_t netadr;
	netadr.SetFromString( pAddress, false );
	if ( netadr.IsBaseAdrValid() )
	{
		// not a string name, ignore
		return 0;
	}

	// remove the possible port
	int nPort = -1;
	char cleanAddress[128];
	V_strncpy( cleanAddress, pAddress, sizeof( cleanAddress ) );
	char *pPort = strchr( cleanAddress, ':' );
	if ( pPort )
	{
		*pPort++ = '\0';
		if ( pPort[0] )
		{
			nPort = atoi( pPort );
		}
	}

	XTITLE_SERVER_INFO serverInfos[XTITLE_SERVER_MAX_LSP_INFO];
	V_memset( serverInfos, 0, sizeof( serverInfos ) );

	// get a title server enumerator
	IN_ADDR serverAddr = { 0 };
	bool bFound = false;
	DWORD dwBufferSize = 0;
	HANDLE hServerEnum = INVALID_HANDLE_VALUE;
	DWORD dwServerCount = 0;
	DWORD dwResult = XTitleServerCreateEnumerator( NULL, XTITLE_SERVER_MAX_LSP_INFO, &dwBufferSize, &hServerEnum );
	if ( dwResult == ERROR_SUCCESS )
	{
		// synchronous population
		dwResult = XEnumerate( hServerEnum, serverInfos, sizeof( serverInfos ), &dwServerCount, NULL );
		CloseHandle( hServerEnum );
		if ( dwResult == ERROR_SUCCESS )
		{
			// iterate all known servers, try and find match
			for ( unsigned int i = 0; i < dwServerCount; i++ )
			{
				// get the clean name
				char cleanName[XTITLE_SERVER_MAX_SERVER_INFO_LEN];
				V_strncpy( cleanName, serverInfos[i].szServerInfo, sizeof( cleanName ) );
				char *pArgs = V_stristr( cleanName, "**" );
				if ( pArgs )
				{
					*pArgs = '\0';
				}
				if ( !V_stricmp( cleanAddress, cleanName ) )
				{
					serverAddr = serverInfos[i].inaServer;
					bFound = true;
					break;
				}
			}
		}
	}
	if ( !bFound )
	{
		return -1;
	}

	if ( nPort != -1 )
	{
		V_snprintf( 
			pOutAddress, 
			outAddressSize, 
			"%d.%d.%d.%d:%d",
			serverAddr.S_un.S_un_b.s_b1,
			serverAddr.S_un.S_un_b.s_b2,
			serverAddr.S_un.S_un_b.s_b3,
			serverAddr.S_un.S_un_b.s_b4,
			nPort );
	} 
	else
	{
		V_snprintf( 
			pOutAddress,
			outAddressSize,
			"%d.%d.%d.%d",
			serverAddr.S_un.S_un_b.s_b1,
			serverAddr.S_un.S_un_b.s_b2,
			serverAddr.S_un.S_un_b.s_b3,
			serverAddr.S_un.S_un_b.s_b4 );
	}

	// success
	return 1;
}
#endif

#if defined( _X360 ) && !defined( _CERT )
//  Non-shipping development helper only.
CON_COMMAND( xlsp_list_servers, "Spew XLSP title servers" )
{
	XTITLE_SERVER_INFO serverInfos[XTITLE_SERVER_MAX_LSP_INFO];
	V_memset( serverInfos, 0, sizeof( serverInfos ) );

	// get a title server enumerator
	DWORD dwBufferSize = 0;
	HANDLE hServerEnum = INVALID_HANDLE_VALUE;
	DWORD dwResult = XTitleServerCreateEnumerator( NULL, XTITLE_SERVER_MAX_LSP_INFO, &dwBufferSize, &hServerEnum );
	if ( dwResult != ERROR_SUCCESS )
	{
		Warning( "XTitleServerCreateEnumerator: failed with 0x%0x.\n", dwResult );
		return;
	}

	// synchronous population
	DWORD dwServerCount = 0;
	dwResult = XEnumerate( hServerEnum, serverInfos, sizeof( serverInfos ), &dwServerCount, NULL );
	CloseHandle( hServerEnum );
	if ( dwResult != ERROR_SUCCESS || !dwServerCount )
	{
		Msg( "No XLSP servers found.\n" );
		return;
	}

	// iterate and spew
	Msg( "\nXLSP Title Servers:\n" );
	for ( DWORD i = 0; i < dwServerCount; ++i )
	{
		// decode private additional args
		char *pToken = V_stristr( serverInfos[i].szServerInfo, "**" );
		if ( !pToken )
		{
			// bad non-conformant syntax, unknown
			continue;
		}
		*pToken = '\0';
		pToken += 2;

		// change to whitespace for easy tokenization
		char argString[XTITLE_SERVER_MAX_SERVER_INFO_LEN];
		V_strncpy( argString, pToken, sizeof( argString ) );
		pToken = argString;
		while ( 1 )
		{
			pToken = strchr( pToken, '_' );
			if ( !pToken )
			{
				break;
			}
			*pToken++ = ' ';
		}

		// get the port and range
		int nPort = 0;
		int nNumPorts = 0;
		int nMasterPort = 0;
		int nNumMasterPorts = 0;
		sscanf( argString, "%d %d %d %d", &nMasterPort, &nNumMasterPorts, &nPort, &nNumPorts );

		// send an async QOS probe to XLSP SG and wait
		DWORD serviceID = g_pMatchFramework->GetMatchTitle()->GetTitleServiceID();
		XNQOS *pXNQOS = NULL;
		DWORD wRttMinInMsecs = 0;
		DWORD wRttMedInMsecs = 0;
		DWORD dwUpBitsPerSec = 0;
		DWORD dwDnBitsPerSec = 0;
		dwResult = XNetQosLookup( 0, NULL, NULL, NULL, 1, &serverInfos[i].inaServer, &serviceID, 8, 0, 0, NULL, &pXNQOS );
		if ( dwResult == ERROR_SUCCESS )
		{
			while ( pXNQOS->cxnqosPending != 0 )
			{
				Sleep( 100 );
			}
			if ( pXNQOS->axnqosinfo[0].bFlags & XNET_XNQOSINFO_COMPLETE )
			{
				// meaningful results
				wRttMinInMsecs = pXNQOS->axnqosinfo[0].wRttMinInMsecs;
				wRttMedInMsecs = pXNQOS->axnqosinfo[0].wRttMedInMsecs;
				dwUpBitsPerSec = pXNQOS->axnqosinfo[0].dwUpBitsPerSec;
				dwDnBitsPerSec = pXNQOS->axnqosinfo[0].dwDnBitsPerSec;
			}
			
			XNetQosRelease( pXNQOS );
			pXNQOS = NULL;
		}

		// spew
		Msg( "Virtual IP4        RTT AvgRTT Upstream Dnstream\n" );
		Msg( "--------------- ------ ------ -------- --------\n" );
		Msg( "%3d.%3d.%3d.%3d %4dms %4dms %4dkbps %4dkbps \"%s\"\n", 
			serverInfos[i].inaServer.S_un.S_un_b.s_b1,
			serverInfos[i].inaServer.S_un.S_un_b.s_b2, 
			serverInfos[i].inaServer.S_un.S_un_b.s_b3,
			serverInfos[i].inaServer.S_un.S_un_b.s_b4,
			wRttMinInMsecs,
			wRttMedInMsecs,
			dwUpBitsPerSec/1024,
			dwDnBitsPerSec/1024,
			serverInfos[i].szServerInfo );
		Msg( "   Title Server Ports:  [%d..%d]\n",  nPort, nPort + nNumPorts - 1 );
		Msg( "   Master Server Ports: [%d..%d]\n",  nMasterPort, nMasterPort + nNumMasterPorts - 1 );
	}
}
#endif

#if defined( _X360 ) && !defined( _CERT )
//  Non-shipping development helper only.
CON_COMMAND_F( connect_xlsp, "Direct connection to specified xlsp server.", FCVAR_DONTRECORD )
{
	// must have xlsp port specifier
	if ( args.ArgC() < 4 || args.Arg( 2 )[0] != ':' )
	{
		ConMsg( "Usage: connect_xlsp <xlsp_server:port> [# of players]\n" );
		return;
	}

	// the tokenizer breaks the address up if a port is provided, we need to reassemble.
	char address[128];
	V_strcpy_safe( address, args.Arg( 1 ) );
	Q_strcat( address, args.Arg( 2 ), sizeof( address ) );
	Q_strcat( address, args.Arg( 3 ), sizeof( address ) );

	int numPlayers = 1;
	if ( args.ArgC() >= 5 )
	{
		numPlayers = atoi( args.Arg( 4 ) );
	}

	// due to dev purposes, do xlsp resolve synchronously
	char xlspAddress[128];
	char secureAddress[128];
	int result = XLSP_NameToAddress( address, xlspAddress, sizeof( xlspAddress ) );
	if ( result > 0 )
	{
		// have valid xlsp, establish connection
		if ( !XLSP_StartConnection( xlspAddress, secureAddress, sizeof( secureAddress ) ) )
		{
			// failed
			Warning( "Could not connect to XLSP server %s.\n", address );
			return;
		}
		// address has been convoluted to the XLSP secure private address
		V_strncpy( address, secureAddress, sizeof( address ) );
	}
	else if ( result == 0 )
	{
		Warning( "Not an XLSP server address %s.\n", address );
		return; 
	}
	else
	{
		Warning( "No XLSP server found matching %s.\n", address );
		return;
	}

	CCommand newArgs;
	newArgs.Tokenize( CFmtStr( "connect_splitscreen %s %d", secureAddress, numPlayers ) );
	connect_splitscreen( newArgs );
}
#endif

//-----------------------------------------------------------------------------
// Takes the map name, strips path and extension
//-----------------------------------------------------------------------------
void CL_SetupMapName( const char* pName, char* pFixedName, int maxlen )
{
	const char* pSlash = strrchr( pName, '\\' );
	const char* pSlash2 = strrchr( pName, '/' );
	if (pSlash2 > pSlash)
		pSlash = pSlash2;
	if (pSlash)
		++pSlash;
	else 
		pSlash = pName;

	Q_strncpy( pFixedName, pSlash, maxlen );
	char* pExt = strchr( pFixedName, '.' );
	if (pExt)
		*pExt = 0;
}


CPureServerWhitelist* CL_LoadWhitelist( INetworkStringTable *pTable, const char *pName )
{
	// If there is no entry for the pure server whitelist, then sv_pure is off and the client can do whatever it wants.
	int iString = pTable->FindStringIndex( pName );
	if ( iString == INVALID_STRING_INDEX )
		return NULL;

	int dataLen; 
	const void *pData = pTable->GetStringUserData( iString, &dataLen );
	if ( pData )
	{
		CUtlBuffer buf( pData, dataLen, CUtlBuffer::READ_ONLY );
		
		CPureServerWhitelist *pWhitelist = CPureServerWhitelist::Create( g_pFullFileSystem );
		pWhitelist->Decode( buf );
		return pWhitelist;
	}
	else
	{
		return NULL;
	}
}


void CL_CheckForPureServerWhitelist()
{
#ifdef DISABLE_PURE_SERVER_STUFF
	return;
#endif

	// Don't do sv_pure stuff in SP games or HLTV/replay
	if ( GetBaseLocalClient().m_nMaxClients <= 1 || GetBaseLocalClient().ishltv
#ifdef REPLAY_ENABLED
		|| GetBaseLocalClient().isreplay
#endif // ifdef REPLAY_ENABLED
		)
		return;
	
	CPureServerWhitelist *pWhitelist = NULL;
	if ( GetBaseLocalClient().m_pServerStartupTable )
		pWhitelist = CL_LoadWhitelist( GetBaseLocalClient().m_pServerStartupTable, "PureServerWhitelist" );
		
	if ( pWhitelist )
	{
		if ( pWhitelist->IsInFullyPureMode() )
			Msg( "Got pure server whitelist: sv_pure = 2.\n" );
		else
			Msg( "Got pure server whitelist: sv_pure = 1.\n" );
		
		CL_HandlePureServerWhitelist( pWhitelist );
	}
	else
	{		
		Msg( "No pure server whitelist. sv_pure = 0\n" );
		CL_HandlePureServerWhitelist( NULL );
	}
}


int CL_GetServerQueryPort()
{
	// Yes, this is ugly getting this data out of a string table. Would be better to have it in our network protocol,
	// but we don't have a way to change the protocol without breaking things for people.
	if ( !GetBaseLocalClient().m_pServerStartupTable )
		return 0;
		
	int iString = GetBaseLocalClient().m_pServerStartupTable->FindStringIndex( "QueryPort" );
	if ( iString == INVALID_STRING_INDEX )
		return 0;
		
	int dataLen; 
	const void *pData = GetBaseLocalClient().m_pServerStartupTable->GetStringUserData( iString, &dataLen );
	if ( pData && dataLen == sizeof( int ) )
		return *((const int*)pData);
	else
		return 0;
}

/*
==================
CL_RegisterResources

Clean up and move to next part of sequence.
==================
*/
void CL_RegisterResources( void )
{
	// All done precaching.
	host_state.SetWorldModel( GetBaseLocalClient().GetModel( 1 ) );
	if ( !host_state.worldmodel )
	{
		Host_Error( "CL_RegisterResources:  host_state.worldmodel/GetBaseLocalClient().GetModel( 1 )==NULL\n" );
	}

	// Force main window to repaint... (only does something if running shaderapi
	videomode->InvalidateWindow();
}

#ifndef DEDICATED
class CEngineReliableAvatarCallback_t
{
public:
	CEngineReliableAvatarCallback_t() : m_steamID( Steam3Client().SteamUser()->GetSteamID() )
		, m_CallbackPersonaStateChanged( this, &CEngineReliableAvatarCallback_t::Steam_OnPersonaStateChanged )
		, m_CallbackAvatarImageLoaded( this, &CEngineReliableAvatarCallback_t::Steam_OnAvatarImageLoaded )
	{
	}

	void UploadMyOwnAvatarToGameServer();

private:
	STEAM_CALLBACK( CEngineReliableAvatarCallback_t, Steam_OnPersonaStateChanged, PersonaStateChange_t, m_CallbackPersonaStateChanged );
	STEAM_CALLBACK( CEngineReliableAvatarCallback_t, Steam_OnAvatarImageLoaded, AvatarImageLoaded_t, m_CallbackAvatarImageLoaded );

	CSteamID m_steamID;
};
void CEngineReliableAvatarCallback_t::Steam_OnAvatarImageLoaded( AvatarImageLoaded_t *pParam )
{
	if ( pParam && ( pParam->m_steamID == m_steamID ) )
		UploadMyOwnAvatarToGameServer();
}

void CEngineReliableAvatarCallback_t::Steam_OnPersonaStateChanged( PersonaStateChange_t *pParam )
{
	if ( pParam && ( pParam->m_nChangeFlags & k_EPersonaChangeAvatar ) && ( pParam->m_ulSteamID == m_steamID.ConvertToUint64() ) )
		UploadMyOwnAvatarToGameServer();
}
void CEngineReliableAvatarCallback_t::UploadMyOwnAvatarToGameServer()
{
	if ( GetBaseLocalClient().IsActive() )
	{
		// Kick off upload for our own avatar data now
		if ( CNETMsg_PlayerAvatarData_t *pMsgMyAvatarData = GetBaseLocalClient().AllocOwnPlayerAvatarData() )
		{
			GetBaseLocalClient().m_NetChannel->EnqueueVeryLargeAsyncTransfer( *pMsgMyAvatarData );
			delete pMsgMyAvatarData;
		}
	}
}
#endif

void CL_FullyConnected( void )
{
	CETWScope timer( "CL_FullyConnected" );

	EngineVGui()->UpdateProgressBar( PROGRESS_FULLYCONNECTED );

	// This has to happen HERE. ***** PRIOR TO g_pQueuedLoader->EndMapLoading() *****
	// in phase 3, because it is in this phase
	// that raycasts against the world is supported (owing to the fact
	// that the world entity has been created by this point)
	StaticPropMgr()->LevelInitClient();
	
	if ( IsGameConsole() )
	{
		// Notify the loader the end of the loading context, preloads are about to be purged
		g_pQueuedLoader->EndMapLoading( false );

		if ( IsPS3() )
		{
			// PS3 static prop lighting (legacy async IO still in flight catching
			// non reslist-lighting buffers) is writing data into raw pointers
			// to RSX memory which have been acquired before material system
			// switches to multithreaded mode. During switch to multithreaded
			// mode RSX moves its memory so pointers become invalid and thus
			// all IO must be finished and callbacks fired before
			// Host_AllowQueuedMaterialSystem
			g_pFullFileSystem->AsyncFinishAll();
		}
	}
		
	// loading completed

	// flush client-side dynamic models that have no refcount
	modelloader->FlushDynamicModels();

	// can NOW safely purge unused models and their data hierarchy (materials, shaders, etc)
	modelloader->PurgeUnusedModels();

#ifdef _PS3
	g_pMaterialSystem->CompactRsxLocalMemory( "FULLY CONNECTED0" );
#endif

	// loading is complete, hint the view model cache to its initial state and evict everybody
	// this is a safety catch for any view models that leaked in, the eviction should be a near no-op
	modelloader->EvictAllWeaponsFromModelCache( true );

	// Purge the preload stores, oreder is critical
	g_pMDLCache->ShutdownPreloadData();

	// NOTE: purposely disabling...
	// THIS IS A BAD THING, so purposely disabled... it saves a few MB that otherwise prevents a bunch of hitchy sync i/o during gameplay.
	// Also, memory spike causing issues, so preload's stay in
	// This may show up on the memory users, and the preloads can be made smaller, or maybe accidentally too large, but ditching them is not the right thing to do.
	// g_pFileSystem->DiscardPreloadData();

	// ***************************************************************
	// NO MORE PRELOAD DATA AVAILABLE PAST THIS POINT!!!
	// ***************************************************************

 	g_ClientDLL->LevelInitPostEntity();

	// communicate to tracker that we're in a game
	// WHY ARE WE USING NETWORK BYTE ORDER HERE?
	uint32 ip = 0;
	uint16 port = 0;
	ns_address remoteAdr = GetBaseLocalClient().m_NetChannel->GetRemoteAddress();
	if ( remoteAdr.IsType<netadr_t>() )
	{
		ip = remoteAdr.AsType<netadr_t>().GetIPNetworkByteOrder();
		port = remoteAdr.AsType<netadr_t>().GetPort();
		if (!port)
		{
			ip = net_local_adr.GetIPNetworkByteOrder();
			port = net_local_adr.GetPort();
		}
	}
	else
	{
#ifdef _DEBUG
		Warning( "CL_FullyConnected - NotifyOfServerConnect assumes remote host is always IPv4 address, but we're connected to %s\n", ns_address_render( remoteAdr ).String() );
#endif
	}

	int iQueryPort = CL_GetServerQueryPort();
	EngineVGui()->NotifyOfServerConnect(com_gamedir, ip, port, iQueryPort);

	GetTestScriptMgr()->CheckPoint( "FinishedMapLoad" );

	EngineVGui()->UpdateProgressBar( PROGRESS_READYTOPLAY );

	if ( ( !IsX360() && !IsPS3() ) || GetBaseLocalClient().m_nMaxClients == 1 )
	{
		// Need this to persist for multiplayer respawns, 360 can't reload
		// [mhansen] PS3 is failing to reload also. We should probably figure out why that is
		// but keeping it around should fix it for now.
		CM_DiscardEntityString();
	}

	g_pMDLCache->EndMapLoad();

#if defined( _MEMTEST )
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), "mem_dump\n" );
#endif

	if ( developer.GetInt() > 0 )
	{
		CClientState &cl = GetBaseLocalClient();

		ConDMsg( "Signon traffic \"%s\":  incoming %s [%d pkts], outgoing %s [%d pkts]\n",
			GetBaseLocalClient().m_NetChannel->GetName(),
			Q_pretifymem( cl.m_NetChannel->GetTotalData( FLOW_INCOMING ), 3 ),
			cl.m_NetChannel->GetTotalPackets( FLOW_INCOMING ),
			Q_pretifymem( cl.m_NetChannel->GetTotalData( FLOW_OUTGOING ), 3 ),
			cl.m_NetChannel->GetTotalPackets( FLOW_OUTGOING ) );
	}

	if ( IsX360() )
	{
		// Reset material system temporary memory (once loading is complete), ready for in-map use
		bool bOnLevelShutdown = false;
		materials->ResetTempHWMemory( bOnLevelShutdown );
	}

	// matsys gets a hint that loading operations have ceased and gameplay is about to start
	g_pMaterialSystem->OnLevelLoadingComplete();

	// allow normal screen updates
	SCR_EndLoadingPlaque();
	EndLoadingUpdates();

#ifdef _PS3
	g_pMaterialSystem->CompactRsxLocalMemory( "FULLY CONNECTED" );
#endif

	// background maps are for main menu UI, QMS not needed or used, easier context
	if ( !engineClient->IsLevelMainMenuBackground() )
	{
		// map load complete, safe to allow QMS
		ConVarRef mat_queue_mode( "mat_queue_mode" );
		if ( mat_queue_mode.GetInt() != 0 )
		{
			Host_AllowQueuedMaterialSystem( true );
		}
	}

	// This is a Hack, but we need to suppress rendering for a bit in single player to let values settle on the client
	if ( (GetBaseLocalClient().m_nMaxClients == 1) && !demoplayer->IsPlayingBack() )
	{
		scr_nextdrawtick = host_tickcount + TIME_TO_TICKS( 0.25f );
	}

#ifdef _X360
	// At the conclusion of loading with a valid user check for a valid controller connection.
	// If it's been lost, then we need to pop our game UI up
	for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
	{
		int iController = XBX_GetUserId( k );
		if ( iController != XBX_INVALID_USER_ID )
		{
			XINPUT_CAPABILITIES caps;
			if ( XInputGetCapabilities( iController, XINPUT_FLAG_GAMEPAD, &caps ) == ERROR_DEVICE_NOT_CONNECTED )
			{
				EngineVGui()->ActivateGameUI();
				break;
			}
		}
	}
#endif

	// Now that we're connected, toggle the clan tag so it gets sent to the server
	//ConVarRef cl_clanid( "cl_clanid" );
	//const char *pClanID = cl_clanid.GetString();
	//cl_clanid.SetValue( 0 );
	//cl_clanid.SetValue( id );

	SplitScreenConVarRef varOption( "cl_clanid" );
	const char *pClanID = varOption.GetString( 0 );
	varOption.SetValue( 0, pClanID );

	g_pMemAlloc->CompactHeap();

	extern double g_flAccumulatedModelLoadTime;
	extern double g_flAccumulatedSoundLoadTime;
	extern double g_flAccumulatedModelLoadTimeStudio;
	extern double g_flAccumulatedModelLoadTimeVCollideSync;
	extern double g_flAccumulatedModelLoadTimeVCollideAsync;
	extern double g_flAccumulatedModelLoadTimeVirtualModel;
	extern double g_flAccumulatedModelLoadTimeStaticMesh;
	extern double g_flAccumulatedModelLoadTimeBrush;
	extern double g_flAccumulatedModelLoadTimeSprite;
	extern double g_flAccumulatedModelLoadTimeMaterialNamesOnly;
//	extern double g_flLoadStudioHdr;

	COM_TimestampedLog( "Sound Loading time %.4f", g_flAccumulatedSoundLoadTime );
	COM_TimestampedLog( "Model Loading time %.4f", g_flAccumulatedModelLoadTime );
	COM_TimestampedLog( "  Model Loading time studio %.4f", g_flAccumulatedModelLoadTimeStudio );
	COM_TimestampedLog( "    Model Loading time GetVCollide %.4f -sync", g_flAccumulatedModelLoadTimeVCollideSync );
	COM_TimestampedLog( "    Model Loading time GetVCollide %.4f -async", g_flAccumulatedModelLoadTimeVCollideAsync );
	COM_TimestampedLog( "    Model Loading time GetVirtualModel %.4f", g_flAccumulatedModelLoadTimeVirtualModel );
	COM_TimestampedLog( "    Model loading time Mod_GetModelMaterials only %.4f", g_flAccumulatedModelLoadTimeMaterialNamesOnly );
	COM_TimestampedLog( "  Model Loading time world %.4f", g_flAccumulatedModelLoadTimeBrush );
	COM_TimestampedLog( "  Model Loading time sprites %.4f", g_flAccumulatedModelLoadTimeSprite );
	COM_TimestampedLog( "  Model Loading time meshes %.4f", g_flAccumulatedModelLoadTimeStaticMesh );
//	COM_TimestampedLog( "    Model Loading time meshes studiohdr load %.4f", g_flLoadStudioHdr );

	COM_TimestampedLog( "*** Map Load Complete" );

	// The reslist generator can't start its timeout until the level loading is absolutely finished
	MapReslistGenerator().OnFullyConnected();

	if ( CommandLine()->FindParm( "-profilemapload" ) )
	{
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "quit\n" );
	}

	if ( CommandLine()->FindParm( "-interactivecaster" ) )
	{
		engineClient->ClientCmd( "cameraman_request" );
	}

#ifndef DEDICATED
	// Register a listener that will be uploading our own avatar data to the game server
	static CEngineReliableAvatarCallback_t s_EngineReliableAvatarCallback;
	s_EngineReliableAvatarCallback.UploadMyOwnAvatarToGameServer();
#endif
}

/*
=====================
CL_NextDemo

Called to play the next demo in the demo loop
=====================
*/
void CL_NextDemo (void)
{
	char	str[1024];

	if (GetBaseLocalClient().demonum == -1)
		return;		// don't play demos

	SCR_BeginLoadingPlaque ();

	if (!GetBaseLocalClient().demos[GetBaseLocalClient().demonum][0] || GetBaseLocalClient().demonum == MAX_DEMOS)
	{
		GetBaseLocalClient().demonum = 0;
		if (!GetBaseLocalClient().demos[GetBaseLocalClient().demonum][0])
		{
			scr_disabled_for_loading = false;

			ConMsg ("No demos listed with startdemos\n");
			GetBaseLocalClient().demonum = -1;
			return;
		}
	}

	Q_snprintf (str,sizeof( str ), "playdemo %s", GetBaseLocalClient().demos[GetBaseLocalClient().demonum]);
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), str );
	GetBaseLocalClient().demonum++;
}

ConVar cl_screenshotname( "cl_screenshotname", "", 0, "Custom Screenshot name" );

// We'll take a snapshot at the next available opportunity
void CL_TakeScreenshot(const char *name)
{
	cl_takesnapshot = true;
	cl_snapshot_fullpathname[0] = 0;
	cl_takejpeg = false;

	if ( name != NULL )
	{
		Q_strncpy( cl_snapshotname, name, sizeof( cl_snapshotname ) );		
	}
	else
	{
		cl_snapshotname[0] = 0;

		if ( Q_strlen( cl_screenshotname.GetString() ) > 0 )
		{
			Q_snprintf( cl_snapshotname, sizeof( cl_snapshotname ), "%s", cl_screenshotname.GetString() );		
		}
	}

	cl_snapshot_subdirname[0] = 0;
}

CON_COMMAND_F( screenshot, "Take a screenshot.", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	GetTestScriptMgr()->SetWaitCheckPoint( "screenshot" );

	// Don't playback screenshots unless specifically requested.	
	if ( demoplayer->IsPlayingBack() && !cl_playback_screenshots.GetBool() )
		return;

	if( args.ArgC() == 2 )
	{
		CL_TakeScreenshot( args[ 1 ] );
	}
	else
	{
		CL_TakeScreenshot( NULL );
	}
}

CON_COMMAND_F( devshots_screenshot, "Used by the -makedevshots system to take a screenshot. For taking your own screenshots, use the 'screenshot' command instead.", FCVAR_DONTRECORD )
{
	CL_TakeScreenshot( NULL );

	// See if we got a subdirectory to store the devshots in
	if ( args.ArgC() == 2 )
	{
		Q_strncpy( cl_snapshot_subdirname, args[1], sizeof( cl_snapshot_subdirname ) );		

		// Use the first available shot in each subdirectory
		cl_snapshotnum = 0;
	}
}

// We'll take a snapshot at the next available opportunity
void CL_TakeJpeg(const char *name, int quality)
{
	// Don't playback screenshots unless specifically requested.	
	if ( demoplayer->IsPlayingBack() && !cl_playback_screenshots.GetBool() )
		return;
	
	cl_takesnapshot = true;
	cl_snapshot_fullpathname[0] = 0;
	cl_takejpeg = true;
	cl_jpegquality = clamp( quality, 1, 100 );

	if ( name != NULL )
	{
		Q_strncpy( cl_snapshotname, name, sizeof( cl_snapshotname ) );		
	}
	else
	{
		cl_snapshotname[0] = 0;
	}
}

CON_COMMAND( jpeg, "Take a jpeg screenshot:  jpeg <filename> <quality 1-100>." )
{
	if( args.ArgC() >= 2 )
	{
		if ( args.ArgC() == 3 )
		{
			CL_TakeJpeg( args[ 1 ], Q_atoi( args[2] ) );
		}
		else
		{
			CL_TakeJpeg( args[ 1 ], jpeg_quality.GetInt() );
		}
	}
	else
	{
		CL_TakeJpeg( NULL, jpeg_quality.GetInt() );
	}
}

static ConVar host_syncfps( "host_syncfps", "0", FCVAR_DEVELOPMENTONLY, "Synchronize real render time to host_framerate if possible." );

// [mhansen] Added a screen shot dump when the fps gets low
static ConVar fps_screenshot_threshold("fps_screenshot_threshold", "-1", FCVAR_CHEAT, "Dump a screenshot when the FPS drops below the given value.");
static ConVar fps_screenshot_frequency("fps_screenshot_frequency", "10", FCVAR_CHEAT, "While the fps is below the threshold we will dump a screen shot this often in seconds (i.e. 10 = screen shot every 10 seconds when under the given fps.)");

void CL_TakeSnapshotAndSwap()
{
	bool bReadPixelsFromFrontBuffer = g_pMaterialSystemHardwareConfig->ReadPixelsFromFrontBuffer();
	if ( bReadPixelsFromFrontBuffer )
	{
		Shader_SwapBuffers();
	}

	// [mhansen] Added a screen shot dump when the fps gets low
	if ( fps_screenshot_threshold.GetInt() > 0 )
	{
		static float sLastRealTime = 0.0f;
		float realFrameTime = realtime - sLastRealTime;
		sLastRealTime = realtime;

		static float timer = 0.0f;

		float screenshotFrameTime = 1.0f / fps_screenshot_threshold.GetFloat();
		if ( realFrameTime > screenshotFrameTime && realFrameTime < 10.0f )
		{
			if ( timer >= 0.0f )
			{
				// Figure out a good name
				//Q_snprintf( cl_snapshotname, sizeof( cl_snapshotname ), "fps_%.2f", (1.0f / realFrameTime) );	
				cl_takesnapshot = true;

				timer -= fps_screenshot_frequency.GetFloat();
			}
		}

		if (timer < 0.0f)
		{
			timer += realFrameTime;
		}
	}

	if (cl_takesnapshot)
	{
		bool bEnabled = materials->AllowThreading( false, g_nMaterialSystemThread );
		char base[MAX_OSPATH];
		char filename[MAX_OSPATH];
		IClientEntity *world = entitylist->GetClientEntity( 0 );

		g_pFileSystem->CreateDirHierarchy( "screenshots", "DEFAULT_WRITE_PATH" );
		if ( cl_snapshot_fullpathname[0] )
		{
			char dir[MAX_OSPATH];
			V_ExtractFilePath( cl_snapshot_fullpathname, dir, MAX_OSPATH );
			g_pFileSystem->CreateDirHierarchy( dir );
		}

		if ( world && world->GetModel() )
		{
			Q_FileBase( modelloader->GetName( ( model_t *)world->GetModel() ), base, sizeof( base ) );

			if ( PLATFORM_EXT[0] )
			{
				// map name has an additional extension
				V_StripExtension( base, base, sizeof( base ) );
			}
		}
		else
		{
			Q_strncpy( base, "Snapshot", sizeof( base ) );
		}

		char extension[MAX_OSPATH];
		Q_snprintf( extension, sizeof( extension ), "%s.%s", GetPlatformExt(), cl_takejpeg ? "jpg" : "tga" );

		// Using a subdir? If so, create it
		if ( cl_snapshot_subdirname[0] )
		{
			Q_snprintf( filename, sizeof( filename ), "screenshots/%s/%s", base, cl_snapshot_subdirname );
			g_pFileSystem->CreateDirHierarchy( filename, "DEFAULT_WRITE_PATH" );
		}

		if ( cl_snapshotname[0] )
		{
#if defined( PLATFORM_X360 )
			if ( ( tolower( cl_snapshotname[0] ) == 'd' ) && ( cl_snapshotname[1] == ':' ) && ( cl_snapshotname[2] == '\\' ) )
			{
				// Filename begins with "d:\" on X360, so assume the user knows what they're doing and has specified the full absolute filename.
				V_strcpy( filename, cl_snapshotname );
				remove( filename );
			}
			else
#endif
			{
				Q_strncpy( base, cl_snapshotname, sizeof( base ) );
				Q_snprintf( filename, sizeof( filename ), "screenshots/%s%s", base, extension );

				// See if the base file already exists and rename if so and if possible.
				if( g_pFileSystem->GetFileTime( filename ) != 0 )
				{
					char renamedfile[MAX_OSPATH];
				
					// Only loop until we hit 9999 (incremented to 10000 afterwards).  More screen shots than that, out of luck.
					for( int iNumber = 0; iNumber < 10000 ; iNumber++)
					{
						Q_snprintf( renamedfile, sizeof( renamedfile ), "screenshots/%s_%04d%s", base, iNumber, extension );
						
						// GetFileTime() is used as a file exists check
						if( !g_pFileSystem->GetFileTime( renamedfile ) )
						{
							g_pFileSystem->RenameFile(filename, renamedfile, "DEFAULT_WRITE_PATH");
							break;
						}
					}

					// Note: If we have 10000 files, just overwrite the base entry
				}
			}

			cl_screenshotname.SetValue( "" );
		}
		else
		{
			while( 1 )
			{
				if ( cl_snapshot_subdirname[0] )
				{
					Q_snprintf( filename, sizeof( filename ), "screenshots/%s/%s/%s%04d%s", base, cl_snapshot_subdirname, base, cl_snapshotnum++, extension  );
				}
				else
				{
					Q_snprintf( filename, sizeof( filename ), "screenshots/%s%04d%s", base, cl_snapshotnum++, extension  );
				}

				if( !g_pFileSystem->GetFileTime( filename, "DEFAULT_WRITE_PATH" ) )
				{
					// woo hoo!  The file doesn't exist already, so use it.
					break;
				}
			}
		}

		if ( cl_snapshot_fullpathname[0] )
		{
			V_strncpy( filename, cl_snapshot_fullpathname, MAX_OSPATH );
			cl_snapshot_fullpathname[0] = 0;
		}

		if ( cl_takejpeg )
		{
			videomode->TakeSnapshotJPEG( filename, cl_jpegquality );
			g_ServerRemoteAccess.UploadScreenshot( filename );
		}
		else
		{
			videomode->TakeSnapshotTGA( filename );
		}
		cl_takesnapshot = false;
		GetTestScriptMgr()->CheckPoint( "screenshot" );
		materials->AllowThreading( bEnabled, g_nMaterialSystemThread );
	}

	// If recording movie and the console is totally up, then write out this frame to movie file.
	if ( cl_movieinfo.IsRecording() && !Con_IsVisible() && !scr_drawloading )
	{
		videomode->WriteMovieFrame( cl_movieinfo );
		++cl_movieinfo.movieframe;
	}

	static double flLastTime = 0.0f;

	bool bSyncFramerate = false;
	if ( host_syncfps.GetBool() &&
		host_framerate.GetFloat() != 0.0f )
	{
		bSyncFramerate = true;
	}

	if ( bSyncFramerate )
	{
		double curtime = Plat_FloatTime();

		double dt = MIN( curtime - flLastTime, MAX_FRAMETIME );
		double desiredInterval = 1.0 / fabsf( host_framerate.GetFloat() );

		double flSleepInterval = desiredInterval - dt;
		if ( flSleepInterval > 0.0f )
		{
			Sys_Sleep( (int)( flSleepInterval * 1000.0f ) );
		}
	}

	if( !bReadPixelsFromFrontBuffer )
	{
		Shader_SwapBuffers();
	}

	if ( bSyncFramerate )
	{
		flLastTime = Plat_FloatTime();
	}

	// take a screenshot for savegames if necessary
	saverestore->UpdateSaveGameScreenshots();

	// take screenshot for bx movie maker
	EngineTool_UpdateScreenshot();
}

static float s_flPreviousHostFramerate = 0;
void CL_StartMovie( const char *filename, int flags, int nWidth, int nHeight, float flFrameRate, int jpeg_quality )
{
	Assert( g_hCurrentAVI == AVIHANDLE_INVALID );

	// StartMove depends on host_framerate not being 0.
	s_flPreviousHostFramerate = host_framerate.GetFloat();
	host_framerate.SetValue( flFrameRate );

	cl_movieinfo.Reset();
	Q_strncpy( cl_movieinfo.moviename, filename, sizeof( cl_movieinfo.moviename ) );
	cl_movieinfo.type = flags;
	cl_movieinfo.jpeg_quality = jpeg_quality;

	if ( cl_movieinfo.DoAVI() || cl_movieinfo.DoAVISound() )
	{
// HACK:  THIS MUST MATCH snd_device.h.  Should be exposed more cleanly!!!
#define SOUND_DMA_SPEED		44100		// hardware playback rate

		AVIParams_t params;
		Q_strncpy( params.m_pFileName, filename, sizeof( params.m_pFileName ) );
		Q_strncpy( params.m_pPathID, "MOD", sizeof( params.m_pPathID ) );
		params.m_nNumChannels = 2;
		params.m_nSampleBits = 16;
		params.m_nSampleRate = SOUND_DMA_SPEED;
		params.m_nWidth = nWidth;
		params.m_nHeight = nHeight;

		if ( IsIntegralValue( flFrameRate ) )
		{
			params.m_nFrameRate = RoundFloatToInt( flFrameRate );
			params.m_nFrameScale = 1;
		}
		else if ( IsIntegralValue( flFrameRate * 1001.0f / 1000.0f ) ) // 1001 is the ntsc divisor (30*1000/1001 = 29.97, etc)
		{
			params.m_nFrameRate = RoundFloatToInt( flFrameRate * 1001 );
			params.m_nFrameScale = 1001;
		}
		else
		{
			// arbitrarily choosing 1000 as the divisor
			params.m_nFrameRate = RoundFloatToInt( flFrameRate * 1000 );
			params.m_nFrameScale = 1000;
		}

		g_hCurrentAVI = avi->StartAVI( params );
	}

	SND_MovieStart();
}

void CL_EndMovie()
{
	if ( !CL_IsRecordingMovie() )
		return;

	host_framerate.SetValue( s_flPreviousHostFramerate );
	s_flPreviousHostFramerate = 0.0f;

	SND_MovieEnd();
	
	if ( cl_movieinfo.DoAVI() || cl_movieinfo.DoAVISound() )
	{
		avi->FinishAVI( g_hCurrentAVI );
		g_hCurrentAVI = AVIHANDLE_INVALID;
	}

	cl_movieinfo.Reset();
}

bool CL_IsRecordingMovie()
{
	return cl_movieinfo.IsRecording();
}

/*
===============
CL_StartMovie_f

Sets the engine up to dump frames
===============
*/

CON_COMMAND_F( startmovie, "Start recording movie frames.", FCVAR_DONTRECORD )
{
	if( args.ArgC() < 2 )
	{
		ConMsg( "startmovie <filename>\n [\n" );
		ConMsg( " (default = TGAs + .wav file)\n" );
		ConMsg( " avi = AVI + AVISOUND\n" );
		ConMsg( " raw = TGAs + .wav file, same as default\n" );
		ConMsg( " tga = TGAs\n" );
		ConMsg( " jpg/jpeg = JPegs\n" );
		ConMsg( " wav = Write .wav audio file\n" );
		ConMsg( " jpeg_quality nnn = set jpeq quality to nnn (range 1 to 100), default %d\n", DEFAULT_JPEG_QUALITY );
		ConMsg( " ]\n" );
		ConMsg( "e.g.:  startmovie testmovie jpg wav jpeg_qality 75\n" );
		ConMsg( "Using AVI will bring up a dialog for choosing the codec, which may not show if you are running the engine in fullscreen mode!\n" );
		return;
	}

	if ( CL_IsRecordingMovie() )
	{
		ConMsg( "Already recording movie!\n" );
		return;
	}

	int flags = MovieInfo_t::FMOVIE_TGA | MovieInfo_t::FMOVIE_WAV;
	int jpeg_quality = DEFAULT_JPEG_QUALITY;

	if ( args.ArgC() > 2 )
	{
		flags = 0;
		for ( int i = 2; i < args.ArgC(); ++i )
		{
			if ( !Q_stricmp( args[ i ], "avi" ) )
			{
				flags |= MovieInfo_t::FMOVIE_AVI | MovieInfo_t::FMOVIE_AVISOUND;
			}
			if ( !Q_stricmp( args[ i ], "raw" ) )
			{
				flags |= MovieInfo_t::FMOVIE_TGA | MovieInfo_t::FMOVIE_WAV;
			}
			if ( !Q_stricmp( args[ i ], "tga" ) )
			{
				flags |= MovieInfo_t::FMOVIE_TGA;
			}
			if ( !Q_stricmp( args[ i ], "jpeg" ) || !Q_stricmp( args[ i ], "jpg" ) )
			{
				flags &= ~MovieInfo_t::FMOVIE_TGA;
				flags |= MovieInfo_t::FMOVIE_JPG;
			}
			if ( !Q_stricmp( args[ i ], "jpeg_quality" ) )
			{
				jpeg_quality = clamp( Q_atoi( args[ ++i ] ), 1, 100 );
			}
			if ( !Q_stricmp( args[ i ], "wav" ) )
			{
				flags |= MovieInfo_t::FMOVIE_WAV;
			}
			
		}
	}

	if ( flags == 0 )
	{
		Warning( "Missing or unknown recording types, must specify one or both of 'avi' or 'raw'\n" );
		return;
	}

	float flFrameRate = host_framerate.GetFloat();
	if ( flFrameRate == 0.0f )
	{
		flFrameRate = 30.0f;
	}
	CL_StartMovie( args[ 1 ], flags, videomode->GetModeWidth(), videomode->GetModeHeight(), flFrameRate, jpeg_quality );
	ConMsg( "Started recording movie, frames will record after console is cleared...\n" );
}


//-----------------------------------------------------------------------------
// Ends frame dumping
//-----------------------------------------------------------------------------
CON_COMMAND_F( endmovie, "Stop recording movie frames.", FCVAR_DONTRECORD )
{
	if( !CL_IsRecordingMovie() )
	{
		ConMsg( "No movie started.\n" );
	}
	else
	{
		CL_EndMovie();
		ConMsg( "Stopped recording movie...\n" );
	}
}

/*
=====================
CL_Rcon_f

  Send the rest of the command line over as
  an unconnected command.
=====================
*/
CON_COMMAND_F( rcon, "Issue an rcon command.", FCVAR_DONTRECORD )
{
	char	message[1024];   // Command message
	char    szParam[ 256 ];
	message[0] = 0;
	for (int i=1 ; i<args.ArgC() ; i++)
	{
		const char *pParam = args[i];
		// put quotes around empty arguments so we can pass things like this: rcon sv_password ""
		// otherwise the "" on the end is lost
		if ( strchr( pParam, ' ' ) || ( Q_strlen( pParam ) == 0 ) )
		{
			Q_snprintf( szParam, sizeof( szParam ), "\"%s\"", pParam );
			Q_strncat( message, szParam, sizeof( message ), COPY_ALL_CHARACTERS );
		}
		else
		{
			Q_strncat( message, pParam, sizeof( message ), COPY_ALL_CHARACTERS );
		}
		if ( i != ( args.ArgC() - 1 ) )
		{
			Q_strncat (message, " ", sizeof( message ), COPY_ALL_CHARACTERS);
		}
	}

	RCONClient().SendCmd( message );
}


CON_COMMAND_F( box, "Draw a debug box.", FCVAR_CHEAT )
{
	if( args.ArgC() != 7 )
	{
		ConMsg ("box x1 y1 z1 x2 y2 z2\n");
		return;
	}

	Vector mins, maxs;
	for (int i = 0; i < 3; ++i)
	{
		mins[i] = atof(args[i + 1]); 
		maxs[i] = atof(args[i + 4]); 
	}
	CDebugOverlay::AddBoxOverlay( vec3_origin, mins, maxs, vec3_angle, 255, 0, 0, 0, 100 );
}

/*
==============
CL_View_f  

Debugging changes the view entity to the specified index
===============
*/
CON_COMMAND_F( cl_view, "Set the view entity index.", FCVAR_CHEAT )
{
	int nNewView;

	if( args.ArgC() != 2 )
	{
		ConMsg ("cl_view entity#\nCurrent %i\n", GetLocalClient().GetViewEntity() );
		return;
	}

	// Only valid in single player!!!
	if ( GetBaseLocalClient().m_nMaxClients > 1 )
		return;

	nNewView = atoi( args[1] );
	if (!nNewView)
		return;

	if ( nNewView > entitylist->GetHighestEntityIndex() )
		return;

	GetLocalClient().m_nViewEntity = nNewView;
	videomode->MarkClientViewRectDirty(); // Force recalculation
	ConMsg("View entity set to %i\n", nNewView);
}

bool IsLowPriorityLight( int key )
{
	return ( key >= LIGHT_INDEX_LOW_PRIORITY );
}

static int CL_AllocLightFromArray( dlight_t *pLights, int lightCount, int key )
{
	int		i;

	// first look for an exact key match
	if (key)
	{
		for ( i = 0; i < lightCount; i++ )
		{
			if (pLights[i].key == key)
				return i;
		}
	}

	// then look for anything else
	for ( i = 0; i < lightCount; i++ )
	{
		if (pLights[i].die < GetBaseLocalClient().GetTime())
			return i;
	}

	if ( cl_retire_low_priority_lights.GetBool() && !IsLowPriorityLight( key ) )
	{
		// find the smallest radius low priority light
		int iSmallest = -1;
		float fSmallestRadius = 0;
		for ( i = 1; i < lightCount; i++ )	// skip light zero since it's the default overflow one
		{
			if ( IsLowPriorityLight( pLights[i].key ) )
			{
				if ( fSmallestRadius == 0 || pLights[i].radius < fSmallestRadius )
				{
					fSmallestRadius = pLights[i].radius;
					iSmallest = i;
				}
			}
		}
		if (iSmallest != -1)
		{
			return iSmallest;
		}
	}

	return 0;
}

bool g_bActiveDlights = false;
bool g_bActiveElights = false;
/*
===============
CL_AllocDlight

===============
*/
dlight_t *CL_AllocDlight (int key)
{
	int i = CL_AllocLightFromArray( cl_dlights, MAX_DLIGHTS, key );
	dlight_t *dl = &cl_dlights[i];
	R_MarkDLightNotVisible( i );
	memset (dl, 0, sizeof(*dl));
	dl->key = key;
	r_dlightchanged |= (1 << i);
	r_dlightactive |= (1 << i);
	g_bActiveDlights = true;
	return dl;
}


/*
===============
CL_AllocElight

===============
*/
dlight_t *CL_AllocElight (int key)
{
	int i = CL_AllocLightFromArray( cl_elights, MAX_ELIGHTS, key );
	dlight_t *el = &cl_elights[i];
	memset (el, 0, sizeof(*el));
	el->key = key;
	g_bActiveElights = true;
	return el;
}


/*
===============
CL_DecayLights

===============
*/
void CL_DecayLights (void)
{
	float		time;
	
	time = GetBaseLocalClient().GetFrameTime();
	if ( time <= 0.0f )
		return;

	CL_UpdateDAndELights( true );
}

void CL_UpdateDAndELights( bool bUpdateDecay )
{
	int			i;
	dlight_t	*dl;
	float		time;

	time = GetBaseLocalClient().GetFrameTime();

	g_bActiveDlights = false;
	g_bActiveElights = false;
	dl = cl_dlights;

	r_dlightchanged = 0;
	r_dlightactive = 0;
	g_nNumActiveDLights = 0;

	for ( i=0 ; i<MAX_DLIGHTS ; i++, dl++ )
	{
		if ( !dl->IsRadiusGreaterThanZero() )
		{
			R_MarkDLightNotVisible( i );
			continue;
		}

		if ( dl->die < GetBaseLocalClient().GetTime() )
		{
			r_dlightchanged |= (1 << i);
			dl->radius = 0;
		}
		else if ( dl->decay && bUpdateDecay )
		{
			r_dlightchanged |= (1 << i);

			dl->radius -= time*dl->decay;
			if ( dl->radius < 0 )
			{
				dl->radius = 0;
			}
		}

		if ( dl->IsRadiusGreaterThanZero() )
		{
			g_bActiveDlights = true;
			r_dlightactive |= (1 << i);
			g_ActiveDLightIndex[g_nNumActiveDLights] = i;
			g_nNumActiveDLights++;
		}
		else
		{
			R_MarkDLightNotVisible( i );
		}
	}

	g_nNumActiveELights = 0;
	dl = cl_elights;
	for ( i=0 ; i<MAX_ELIGHTS ; i++, dl++ )
	{
		if ( !dl->IsRadiusGreaterThanZero() )
			continue;

		if ( dl->die < GetBaseLocalClient().GetTime() )
		{
			dl->radius = 0;
			continue;
		}

		if ( bUpdateDecay )
		{
			dl->radius -= time*dl->decay;
		}

		if ( dl->radius < 0 )
		{
			dl->radius = 0;
		}
		if ( dl->IsRadiusGreaterThanZero() )
		{
			g_bActiveElights = true;
			g_ActiveELightIndex[g_nNumActiveELights] = i;
			g_nNumActiveELights++;
		}
	}
}

void CL_ExtraMouseUpdate( float frametime )
{
	if ( !Host_ShouldRun() )
		return;

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		// Not ready for commands yet.
		if ( !GetLocalClient().IsActive() )
			continue;

		// Don't create usercmds here during playback, they were encoded into the packet already
		if ( demoplayer->IsPlayingBack() && !GetLocalClient().ishltv
#	ifdef REPLAY_ENABLED
			&& !GetLocalClient().isreplay
#	endif
			)
			continue;

		// Have client .dll create and store usercmd structure
		g_ClientDLL->ExtraMouseSample( frametime, 
			!GetLocalClient().m_bPaused 
		);
	}
}

/*
=================
CL_SendMove

Constructs the movement command and sends it to the server if it's time.
=================
*/
void CL_SendMove( void )
{
	int nextcommandnr = GetBaseLocalClient().lastoutgoingcommand + GetBaseLocalClient().chokedcommands + 1;
	int nChokedCommands = GetBaseLocalClient().chokedcommands;

	// send the client update packet
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );

		if ( splitscreen->IsDisconnecting( i ) )
			continue;

		bf_write DataOut;
		byte data[ MAX_CMD_BUFFER ];
		CCLCMsg_Move_t moveMsg;
		
		DataOut.StartWriting( data, sizeof( data ) );

		// Determine number of backup commands to send along
		int cl_cmdbackup = 2;
		int nBackupCommands = clamp( cl_cmdbackup, 0, MAX_BACKUP_COMMANDS );

		// How many real new commands have queued up
		int nNewCommands = clamp( nChokedCommands + 1, 0, MAX_NEW_COMMANDS );

		moveMsg.set_num_backup_commands( nBackupCommands );
		moveMsg.set_num_new_commands( nNewCommands );

		int numcmds = nNewCommands + nBackupCommands;
		int from = -1;	// first command is deltaed against zeros 
		bool bOK = true;

		for ( int to = nextcommandnr - numcmds + 1; to <= nextcommandnr; ++to )
		{
			bool isnewcmd = to >= (nextcommandnr - nNewCommands + 1);

			// first valid command number is 1
			bOK = bOK && g_ClientDLL->WriteUsercmdDeltaToBuffer( i, &DataOut, from, to, isnewcmd );
			from = to;
		}

		if ( bOK )
		{
			moveMsg.set_data( ( const char * )DataOut.GetData(), DataOut.GetNumBytesWritten() );

			// only write message if all CUserCmds were written correctly, otherwise parsing would fail
			GetLocalClient().m_NetChannel->SendNetMsg( moveMsg );
		}
	}
}

void CL_Move(float accumulated_extra_samples, bool bFinalTick )
{
	CClientState &cl = GetBaseLocalClient();

	if ( !cl.IsConnected() )
		return;

	if ( !Host_ShouldRun() )
		return;

	// only send packets on the final tick in one engine frame
	bool bSendPacket = true;	

	// Don't create usercmds here during playback, they were encoded into the packet already
	if ( demoplayer->IsPlayingBack() )
	{
		if ( cl.ishltv 
#	ifdef REPLAY_ENABLED
			|| cl.isreplay 
#	endif // ifdef REPLAY_ENABLED
			)
		{
			// still do it when playing back a HLTV/replay demo
			bSendPacket = false;
		}
		else
		{
            return;
		}
	}

	// don't send packets if update time not reached or chnnel still sending
	// in loopback mode don't send only if host_limitlocal is enabled

	if ( ( !cl.m_NetChannel->IsLoopback() || host_limitlocal.GetInt() ) &&
		 ( ( net_time < cl.m_flNextCmdTime ) || !cl.m_NetChannel->CanPacket()  || !bFinalTick ) )
	{
		bSendPacket = false;
	}

	if ( cl.IsActive() )
	{
		VPROF( "CL_Move" );

		int nextcommandnr = cl.lastoutgoingcommand + cl.chokedcommands + 1;

		// Have client .dll create and store CUserCmd structure(s)
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
			if ( splitscreen->IsDisconnecting( i ) )
				continue;

			g_ClientDLL->CreateMove( 
				nextcommandnr, 
				host_state.interval_per_tick - accumulated_extra_samples,
				!cl.IsPaused()
			);

			// Store new usercmd to dem file
			if ( demorecorder->IsRecording() )
			{
				// Back up one because we've incremented outgoing_sequence each frame by 1 unit
				demorecorder->RecordUserInput( nextcommandnr );
			}
		}

		if ( bSendPacket )
		{
			CL_SendMove();
		}
		else
		{
			// netchannel will increase internal outgoing sequence number too
			cl.m_NetChannel->SetChoked();	
			// Mark command as held back so we'll send it next time
			cl.chokedcommands++;
		}
	}

	if ( !bSendPacket )
		return;

		// Request non delta compression if high packet loss, show warning message
	bool hasProblem = cl.m_NetChannel->IsTimingOut() && !demoplayer->IsPlayingBack() &&	cl.IsActive();

	// check timeout, but not if running _DEBUG engine
	bool bAllowTimeout = true;
#if defined( _DEBUG )
	bAllowTimeout = false;
#endif

	//
	// See sfhud_autodisconnect.cpp for parsing in:
	// void SFHudAutodisconnect::ProcessInput( void )
	//

	// Request non delta compressed update if high packet loss
	// show warning message/UI
	if ( hasProblem )
	{
#if !defined( CSTRIKE15 )
		con_nprint_t np;
		np.time_to_live = 1.0;
		np.index = 2;
		np.fixed_width_font = false;
		np.color[ 0 ] = 1.0;
		np.color[ 1 ] = 0.2;
		np.color[ 2 ] = 0.2;
		
		Con_NXPrintf( &np, "WARNING:  Connection Problem" );
#endif

		if ( bAllowTimeout )
		{
			float flTimeOut = cl.m_NetChannel->GetTimeoutSeconds();
			Assert( flTimeOut != -1.0f );
			float flRemainingTime = MAX( flTimeOut - cl.m_NetChannel->GetTimeSinceLastReceived(), 0.0f );

			// write time until connection is dropped to a convar
			cl_connection_trouble_info.SetValue( CFmtStr( "disconnect(%0.3f)", flRemainingTime ) );

#if !defined( CSTRIKE15 )
			np.index = 3;
			Con_NXPrintf( &np, "Auto-disconnect in %.1f seconds", flRemainingTime );
#endif

			EngineVGui()->NeedConnectionProblemWaitScreen();
		}

		// sets m_nDeltaTick to -1
		cl.ForceFullUpdate( "connection problem" ); 
	}
	else
	{
		// We are no longer timing out
		float flLastTransientProblemTime = 0.0f;
		if ( cl_connection_trouble_info.GetString()[0] == '@' )
			flLastTransientProblemTime = V_atof( cl_connection_trouble_info.GetString() + 1 );

		float flTimeNow = Plat_FloatTime();	// See if sufficient time elapsed since we started showing a problem info (don't change the error on user too quickly)
		if ( ( flLastTransientProblemTime < flTimeNow - 0.5f ) || ( flLastTransientProblemTime > flTimeNow + 0.5f ) )
		{
			// Are we experiencing packet loss? (router dropping packets, need to slow down!)
			float const flPacketLoss = cl.m_NetChannel->GetAvgLoss( FLOW_INCOMING );
			// ... or are we choking (insufficient bandwidth, get better internet!)
			float const flPacketChoke = cl.m_NetChannel->GetAvgChoke( FLOW_INCOMING );
			if ( flPacketLoss > 0 )
				cl_connection_trouble_info.SetValue( CFmtStr( "@%.03f:loss(%.05f)", flTimeNow, flPacketLoss ) );
			else if ( flPacketChoke > 0 )
				cl_connection_trouble_info.SetValue( CFmtStr( "@%.03f:choke(%.05f)", flTimeNow, flPacketChoke ) );
			else if ( cl_connection_trouble_info.GetString()[ 0 ] )
				cl_connection_trouble_info.SetValue( "" );
		}
	}

	if ( cl.IsActive() )
	{
		int tick = cl.m_nDeltaTick;
		CNETMsg_Tick_t mymsg( tick, host_frameendtime_computationduration, host_frametime_stddeviation, host_framestarttime_stddeviation );
		if ( cl.GetHltvReplayDelay() )
		{
			mymsg.set_hltv_replay_flags( 1 ); // signal that this ack is from hltv replay
		}
		cl.m_NetChannel->SendNetMsg( mymsg );
	}

	//COM_Log( "cl.log", "Sending command number %i(%i) to server\n", cl.m_NetChan->m_nOutSequenceNr, GetLocalClient().m_NetChan->m_nOutSequenceNr & CL_UPDATE_MASK );

	// Remember outgoing command that we are sending
	cl.lastoutgoingcommand = cl.m_NetChannel->SendDatagram( NULL );

	cl.chokedcommands = 0;

	// calc next packet send time

	if ( cl.IsActive() )
	{
		// use full update rate when active
		float commandInterval = 1.0f / cl_cmdrate->GetFloat();
		float maxDelta = MIN( host_state.interval_per_tick, commandInterval );
        float delta = clamp( net_time - cl.m_flNextCmdTime, 0.0f, maxDelta );
		cl.m_flNextCmdTime = net_time + commandInterval - delta;
	}
	else
	{
		// during signon process send only 5 packets/second
		cl.m_flNextCmdTime = net_time + ( 1.0f / 5.0f );
	}

}

#define TICK_INTERVAL			(host_state.interval_per_tick)
#define ROUND_TO_TICKS( t )		( TICK_INTERVAL * TIME_TO_TICKS( t ) )

void CL_LatchInterpolationAmount()
{
	CClientState &cl = GetBaseLocalClient();

	if ( !cl.IsConnected() )
		return;

	float dt = cl.m_NetChannel->GetTimeSinceLastReceived();
	float flClientInterpolationAmount = ROUND_TO_TICKS( cl.GetClientInterpAmount() );

	float flInterp = 0.0f;
	if ( flClientInterpolationAmount > 0.001 )
	{
		flInterp = clamp( dt / flClientInterpolationAmount, 0.0f, 3.0f );
	}
	cl.m_NetChannel->SetInterpolationAmount( flInterp );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pMessage - 
//-----------------------------------------------------------------------------
void CL_HudMessage( const char *pMessage )
{
	if ( g_ClientDLL )
	{
		g_ClientDLL->HudText( pMessage );
	}
}

CON_COMMAND_F( cl_showents, "Dump entity list to console.", FCVAR_CHEAT )
{
	for ( int i = 0; i < entitylist->GetMaxEntities(); i++ )
	{
		char entStr[256], classStr[256];
		IClientNetworkable *pEnt;

		if((pEnt = entitylist->GetClientNetworkable(i)) != NULL)
		{
			entStr[0] = 0;
			Q_snprintf(classStr, sizeof( classStr ), "'%s'", pEnt->GetClientClass()->m_pNetworkName);
		}
		else
		{
			Q_snprintf(entStr, sizeof( entStr ), "(missing), ");
			Q_snprintf(classStr, sizeof( classStr ), "(missing)");
		}

		if ( pEnt )
			ConMsg("Ent %3d: %s class %s\n", i, entStr, classStr);
	}
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the background level should be loaded on startup
//-----------------------------------------------------------------------------
bool CL_ShouldLoadBackgroundLevel( const CCommand &args )
{
	// portal2 is not using a background map
	return false;

	if ( CommandLine()->CheckParm( "-nostartupmenu" ) )
		return false;
	if ( CommandLine()->CheckParm("-makereslists") )
		return false;

	if ( InEditMode() )
		return false;

	if ( args.ArgC() == 2 )
	{
		// presence of args identifies an end-of-game situation
		if ( IsGameConsole() )
		{
			// Console needs to get UI in the correct state to transition to the Background level
			// from the credits.
			return true;
		}

		if ( !Q_stricmp( args[1], "force" ) )
		{
			// Adrian: Have to do this so the menu shows up if we ever call this while in a level.
			Host_Disconnect( true );
			// pc can't get into background maps fast enough, so just show main menu
			return false;
		}

		if ( !Q_stricmp( args[1], "playendgamevid" ) )
		{
			// Bail back to the menu and play the end game video.
			CommandLine()->AppendParm( "-endgamevid", NULL ); 
			CommandLine()->RemoveParm( "-recapvid" );
			HostState_Restart();
			return false;
		}

		if ( !Q_stricmp( args[1], "playrecapvid" ) )
		{
			// Bail back to the menu and play the recap video
			CommandLine()->AppendParm( "-recapvid", NULL ); 
			CommandLine()->RemoveParm( "-endgamevid" );
			HostState_Restart();
			return false;
		}
	}

	// don't load the map if we're going straight into a level
	if ( CommandLine()->CheckParm("+map") ||
		CommandLine()->CheckParm("+connect") ||
		CommandLine()->CheckParm("+playdemo") ||
		CommandLine()->CheckParm("+timedemo") ||
		CommandLine()->CheckParm("+timedemoquit") ||
		CommandLine()->CheckParm("+load") )
		return false;

	// nothing else is going on, so load the startup level
	return true;
}

#define DEFAULT_BACKGROUND_NAME	"background01"

int CL_GetBackgroundLevelIndex( int nNumChapters )
{
	int iChapterIndex = sv_unlockedchapters.GetInt();
	iChapterIndex = MIN( iChapterIndex, nNumChapters );
	if ( iChapterIndex <= 0 )
	{
		// expected to be [1..N]
		iChapterIndex = 1;
	}

	return iChapterIndex;
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of the background level to load
//-----------------------------------------------------------------------------
void CL_GetBackgroundLevelName( char *pszBackgroundName, int bufSize, bool bMapName )
{
	Q_strncpy( pszBackgroundName, DEFAULT_BACKGROUND_NAME, bufSize );

	KeyValues *pChapterFile = new KeyValues( pszBackgroundName );

	if ( pChapterFile->LoadFromFile( g_pFileSystem, "scripts/ChapterBackgrounds.txt" ) )
	{
		KeyValues *pChapterRoot = pChapterFile;

		const char *szChapterIndex;
		int nNumChapters = 1;
		KeyValues *pChapters = pChapterFile->GetNextKey();
		if ( bMapName && pChapters )
		{
			const char *pszName = pChapters->GetName();
			if ( pszName && pszName[0] && StringHasPrefix( pszName, "BackgroundMaps" ) )
			{
				pChapterRoot = pChapters;
				pChapters = pChapters->GetFirstSubKey();
			}
			else
			{
				pChapters = NULL;
			}
		}
		else
		{
			pChapters = NULL;
		}

		if ( !pChapters )
		{
			pChapters = pChapterFile->GetFirstSubKey();
		}

		// Find the highest indexed chapter
		while ( pChapters )
		{
			szChapterIndex = pChapters->GetName();

			if ( szChapterIndex )
			{
				int nChapter = atoi(szChapterIndex);

				if( nChapter > nNumChapters )
					nNumChapters = nChapter;
			}
			
			pChapters = pChapters->GetNextKey();
		}	

		int nChapterToLoad = CL_GetBackgroundLevelIndex( nNumChapters );

		// Find the chapter background with this index
		char buf[4];
		Q_snprintf( buf, sizeof(buf), "%d", nChapterToLoad );
		KeyValues *pLoadChapter = pChapterRoot->FindKey(buf);

		// Copy the background name
		if ( pLoadChapter )
		{
			Q_strncpy( pszBackgroundName, pLoadChapter->GetString(), bufSize );
		}
	}

	pChapterFile->deleteThis();
}

//-----------------------------------------------------------------------------
// A random number at startup (sequential for 360), drives the product
// startup screen choice.
//-----------------------------------------------------------------------------
#define NUM_STARTUP_IMAGES	2
unsigned int CL_GetStartupIndex()
{
	static unsigned int nWhich = 0;
	if ( !nWhich )
	{	
		// once set, stays set to the same value for this entire session
		int nOverride = CommandLine()->ParmValue( "-startup", 0 );
		if ( nOverride > 0 )
		{
			nWhich = clamp( nOverride, 1, NUM_STARTUP_IMAGES );
		}
		else
		{
			nWhich = Plat_GetClockStart();
			nWhich = ( nWhich % NUM_STARTUP_IMAGES ) + 1;
		}
	}
	return nWhich;
}

//-----------------------------------------------------------------------------
// Isolated startup from menu backgrounds, which are not used for startup but
// may be used for different cases. Needs to be the same for everybody, isolated here.
//-----------------------------------------------------------------------------
void CL_GetStartupImage( char *pOutBuffer, int nOutBufferSize )
{
#if defined( CSTRIKE15)
	// CStrike15 uses a specific startup image instead of the random image.
	// CSGO always uses a widescreen format image, regardless of the screen resolution,
	// to match how the Scaleform background is drawn.  CVideoMode_Common::DrawStartupGraphic
	// takes care of repositioning and scaling this image to match the method
	// used in Scaleform.
	V_strncpy( pOutBuffer, "console/background01_widescreen", nOutBufferSize );
#else
	const AspectRatioInfo_t &aspectRatioInfo = materials->GetAspectRatioInfo();
	int nWhich = CL_GetStartupIndex();
	V_snprintf( pOutBuffer, nOutBufferSize, "console/portal2_product_%d%s", nWhich, ( aspectRatioInfo.m_bIsWidescreen ? "_widescreen" : "" ) );
#endif // CSTRIKE15
}

//-----------------------------------------------------------------------------
// Purpose: Callback to open the game menus
//-----------------------------------------------------------------------------
void CL_CheckToDisplayStartupMenus( const CCommand &args )
{
	if ( CL_ShouldLoadBackgroundLevel( args ) )
	{
		char szBackgroundName[256];
		CL_GetBackgroundLevelName( szBackgroundName, sizeof(szBackgroundName), true );

		char cmd[256];
		Q_snprintf( cmd, sizeof(cmd), "map_background %s\n", szBackgroundName );
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), cmd );
	}
}

static float s_fDemoRevealGameUITime = -1;
float s_fDemoPlayMusicTime = -1;
static bool s_bIsRavenHolmn = false;
//-----------------------------------------------------------------------------
// Purpose: run the special demo logic when transitioning from the trainstation levels
//----------------------------------------------------------------------------
void CL_DemoTransitionFromTrainstation()
{
	// kick them out to GameUI instead and bring up the chapter page with raveholm unlocked
	sv_unlockedchapters.SetValue(6); // unlock ravenholm
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), "sv_cheats 1; fadeout 1.5; sv_cheats 0;");
	Cbuf_Execute();
	s_fDemoRevealGameUITime = Sys_FloatTime() + 1.5;
	s_bIsRavenHolmn = false;
}

void CL_DemoTransitionFromRavenholm()
{
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), "sv_cheats 1; fadeout 2; sv_cheats 0;");
	Cbuf_Execute();
	s_fDemoRevealGameUITime = Sys_FloatTime() + 1.9;
	s_bIsRavenHolmn = true;
}

void CL_DemoTransitionFromTestChmb()
{
	Cbuf_AddText( Cbuf_GetCurrentPlayer(), "sv_cheats 1; fadeout 2; sv_cheats 0;");
	Cbuf_Execute();
	s_fDemoRevealGameUITime = Sys_FloatTime() + 1.9;	
}


//-----------------------------------------------------------------------------
// Purpose: make the gameui appear after a certain interval
//----------------------------------------------------------------------------
void V_RenderVGuiOnly();
bool V_CheckGamma();
void CL_DemoCheckGameUIRevealTime( ) 
{
	if ( s_fDemoRevealGameUITime > 0 )
	{
		if ( s_fDemoRevealGameUITime < Sys_FloatTime() )
		{
			s_fDemoRevealGameUITime = -1;

			SCR_BeginLoadingPlaque();
			Cbuf_AddText( Cbuf_GetCurrentPlayer(), "disconnect;");

			CCommand args;
			CL_CheckToDisplayStartupMenus( args );

			s_fDemoPlayMusicTime = Sys_FloatTime() + 1.0;
		}
	}

	if ( s_fDemoPlayMusicTime > 0 )
	{
		V_CheckGamma();
		V_RenderVGuiOnly();
		if ( s_fDemoPlayMusicTime < Sys_FloatTime() )
		{
			s_fDemoPlayMusicTime = -1;
			EngineVGui()->ActivateGameUI();

			if ( CL_IsHL2Demo() )
			{
				if ( s_bIsRavenHolmn )
				{
					Cbuf_AddText( Cbuf_GetCurrentPlayer(), "play music/ravenholm_1.mp3;" );
				}
// 				else
// 				{
// 					EngineVGui()->ShowNewGameDialog(6);// bring up the new game dialog in game UI
// 				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: setup a debug string that is uploaded on crash
//----------------------------------------------------------------------------

char g_minidumpinfo[ 4094 ] = {0};
PAGED_POOL_INFO_t g_pagedpoolinfo = { 0 };
#if !defined( NO_STEAM )
extern bool g_bV3SteamInterface;
#endif
void DisplaySystemVersion( char *osversion, int maxlen );

void CL_SetPagedPoolInfo()
{
	if ( IsGameConsole() )
		return;
#if !defined( _GAMECONSOLE ) && !defined(NO_STEAM) && !defined(DEDICATED)
	Plat_GetPagedPoolInfo( &g_pagedpoolinfo );
#endif
}

void CL_SetSteamCrashComment()
{
	if ( IsGameConsole() )
		return;

	char map[ 80 ];
	char videoinfo[ 2048 ];
	char misc[ 256 ];
	char driverinfo[ 2048 ];
	char osversion[ 256 ];

	map[ 0 ] = 0;
	driverinfo[ 0 ] = 0;
	videoinfo[ 0 ] = 0;
	misc[ 0 ] = 0;
	osversion[ 0 ] = 0;

	if ( host_state.worldmodel )
	{
		CL_SetupMapName( modelloader->GetName( host_state.worldmodel ), map, sizeof( map ) );
	}

	DisplaySystemVersion( osversion, sizeof( osversion ) );

	MaterialAdapterInfo_t info;
	materials->GetDisplayAdapterInfo( materials->GetCurrentAdapter(), info );

	const char *dxlevel = "Unk";
	if ( g_pMaterialSystemHardwareConfig )
	{
		dxlevel = COM_DXLevelToString( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() ) ;
	}

	// Make a string out of the high part and low parts of driver version
	char szDXDriverVersion[ 64 ];
	Q_snprintf( szDXDriverVersion, sizeof( szDXDriverVersion ), "%ld.%ld.%ld.%ld", 
		( long )( info.m_nDriverVersionHigh>>16 ), 
		( long )( info.m_nDriverVersionHigh & 0xffff ), 
		( long )( info.m_nDriverVersionLow>>16 ), 
		( long )( info.m_nDriverVersionLow & 0xffff ) );

	Q_snprintf( driverinfo, sizeof(driverinfo), "Driver Name:  %s\nDriver Version: %s\nVendorId / DeviceId:  0x%x / 0x%x\nSubSystem / Rev:  0x%x / 0x%x\nDXLevel:  %s\nVid:  %i x %i",
		info.m_pDriverName,
		szDXDriverVersion,
		info.m_VendorID,
		info.m_DeviceID,
		info.m_SubSysID,
		info.m_Revision,
		dxlevel ? dxlevel : "Unk",
		videomode->GetModeWidth(), videomode->GetModeHeight() );

	ConVarRef mat_picmip( "mat_picmip" );
	ConVarRef mat_forceaniso( "mat_forceaniso" );
	ConVarRef mat_antialias( "mat_antialias" );
	ConVarRef mat_aaquality( "mat_aaquality" );
	ConVarRef r_shadowrendertotexture( "r_shadowrendertotexture" );
	ConVarRef r_flashlightdepthtexture( "r_flashlightdepthtexture" );
#ifndef _X360
	ConVarRef csm_quality_level( "csm_quality_level" );
	ConVarRef r_waterforceexpensive( "r_waterforceexpensive" );
#endif
	ConVarRef r_waterforcereflectentities( "r_waterforcereflectentities" );
	ConVarRef mat_vsync( "mat_vsync" );
	ConVarRef r_rootlod( "r_rootlod" );
	ConVarRef mat_motion_blur_enabled( "mat_motion_blur_enabled" );
	ConVarRef mat_queue_mode( "mat_queue_mode" );
	ConVarRef mat_triplebuffered( "mat_triplebuffered" );

#ifdef _X360
	Q_snprintf( videoinfo, sizeof(videoinfo), "picmip: %i forceaniso: %i antialias: %i (%i) vsync: %i rootlod: %i\nshadowrendertotexture: %i r_flashlightdepthtexture %i"\
				"waterforcereflectentities: %i mat_motion_blur_enabled: %i mat_triplebuffered: %i",
				mat_picmip.GetInt(), mat_forceaniso.GetInt(), mat_antialias.GetInt(), mat_aaquality.GetInt(), mat_vsync.GetInt(), r_rootlod.GetInt(), r_shadowrendertotexture.GetInt(),
				r_flashlightdepthtexture.GetInt(), r_waterforcereflectentities.GetInt(), mat_motion_blur_enabled.GetInt(), mat_triplebuffered.GetInt() );
#else
	Q_snprintf( videoinfo, sizeof(videoinfo), "picmip: %i\nforceaniso: %i\nantialias: %i (%i)\nvsync: %i\nrootlod: %i\nshadowrendertotexture: %i\nr_flashlightdepthtexture %i\n"\
				"waterforceexpensive: %i\nwaterforcereflectentities: %i\nmat_motion_blur_enabled: %i\nmat_queue_mode %i\nmat_triplebuffered: %i\ncsm_quality_level: %i",
				mat_picmip.GetInt(), mat_forceaniso.GetInt(), mat_antialias.GetInt(), mat_aaquality.GetInt(), mat_vsync.GetInt(), r_rootlod.GetInt(), r_shadowrendertotexture.GetInt(),
				r_flashlightdepthtexture.GetInt(), r_waterforceexpensive.GetInt(), r_waterforcereflectentities.GetInt(), mat_motion_blur_enabled.GetInt(), mat_queue_mode.GetInt(), 
				mat_triplebuffered.GetInt(), csm_quality_level.GetInt() );
#endif
	int latency = 0;
	if ( GetBaseLocalClient().m_NetChannel )
	{
		latency = (int)( 1000.0f * GetBaseLocalClient().m_NetChannel->GetAvgLatency( FLOW_OUTGOING ) );
	}

	Q_snprintf( misc, sizeof( misc ), "skill:%i rate %i update %i cmd %i latency %i msec", 
		skill.GetInt(),
		cl_rate->GetInt(),
		(int)cl_updaterate->GetFloat(),
		(int)cl_cmdrate->GetFloat(),
		latency
	);

	const char *pNetChannel = "Not Connected";
	if ( GetBaseLocalClient().m_NetChannel )
	{
		pNetChannel = GetBaseLocalClient().m_NetChannel->GetAddress();
	}

	CL_SetPagedPoolInfo();

	char am_pm[] = "AM";
	struct tm newtime;
	Plat_GetLocalTime( &newtime );
	if( newtime.tm_hour > 12 )        /* Set up extension. */
		Q_strncpy( am_pm, "PM", sizeof( am_pm ) );
	if( newtime.tm_hour > 12 )        /* Convert from 24-hour */
		newtime.tm_hour -= 12;    /*   to 12-hour clock.  */
	if( newtime.tm_hour == 0 )        /*Set hour to 12 if midnight. */
		newtime.tm_hour = 12;

	char tString[ 128 ];
	Plat_GetTimeString( &newtime, tString, sizeof( tString ) );

	int tLen = Q_strlen( tString );
	if ( tLen > 0 && tString[ tLen - 1 ] == '\n' )
	{
		tString[ tLen - 1 ] = 0;
	}

	Q_snprintf( g_minidumpinfo, sizeof(g_minidumpinfo),
			"Map: %s\n"\
			"Game: %s\n"\
			"Build: %i\n"\
			"OS: %s\n"\
			"Misc: %s\n"\
			"Net: %s\n"\
			"Time: %s\n"\
			"cmdline:%s\n"\
			"protocol:%s\n"
			"driver: %s\n"\
			"video: %s\n"\
			"PP PAGES: used: %d, free %d\n",
			map, com_gamedir, build_number(), osversion, misc, pNetChannel, tString, CommandLine()->GetCmdLine(), Sys_GetVersionString(), 
			driverinfo, videoinfo, g_pagedpoolinfo.numPagesUsed, (int)g_pagedpoolinfo.numPagesFree );

#ifndef NO_STEAM
	SteamAPI_SetMiniDumpComment( g_minidumpinfo );
#endif
}


//
// register commands
//
static ConCommand startupmenu( "startupmenu", &CL_CheckToDisplayStartupMenus, "Opens initial menu screen and loads the background bsp, but only if no other level is being loaded, and we're not in developer mode." );

ConVar cl_language( "cl_language", "english", FCVAR_HIDDEN, "Language (from Steam API)" );
void CL_InitLanguageCvar()
{
	// !! bug do i need to do something linux-wise here.
	char language[64];

	// Fallback to English
	V_strncpy( language, "english", sizeof( language ) );
	
	if ( IsPC() )
	{
#if !defined( NO_STEAM )
		if ( CommandLine()->CheckParm( "-language" ) )
		{
			Q_strncpy( language, CommandLine()->ParmValue( "-language", "english"), sizeof( language ) );
		}
		else
		{
			// Use steam client language
			memset( language, 0, sizeof( language ) );
#ifdef PLATFORM_WINDOWS
			vgui::system()->GetRegistryString( "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Language", language, sizeof( language ) - 1 );
			if ( Q_strlen( language ) == 0 || Q_stricmp( language, "unknown" ) == 0 )
			{
				Q_strncpy( language, "english", sizeof( language ) );
			}
#elif defined(OSX)
			if ( Steam3Client().SteamApps() )
			{
				// just follow the language steam wants you to be
				const char *lang = Steam3Client().SteamApps()->GetCurrentGameLanguage();
				if ( lang && Q_strlen(lang) )
				{
					Q_strncpy( language, lang, sizeof( language ) );
				}
				else 
					Q_strncpy( language, "english", sizeof( language ) );
			}
			else 
			{
				Q_strncpy( language, "english", sizeof( language ) );
			}
#endif			
		}
#endif
	}
	else
	{
		Q_strncpy( language, XBX_GetLanguageString(), sizeof( language ) );
	}
	cl_language.SetValue( language );
}

void CL_ChangeCloudSettingsCvar( IConVar *var, const char *pOldValue, float flOldValue )
{
	// !! bug do i need to do something linux-wise here.
	if ( IsPC() && Steam3Client().SteamUtils() )
	{
#ifdef PLATFORM_WINDOWS
		char szRegistryKeyLocation[ 256 ];
		Q_snprintf( szRegistryKeyLocation, sizeof( szRegistryKeyLocation ), "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Apps\\%d\\Cloud", Steam3Client().SteamUtils()->GetAppID() );

		ConVarRef ref( var->GetName() );
		vgui::system()->SetRegistryInteger( szRegistryKeyLocation, ref.GetInt() );
#endif
	}
}

ConVar cl_cloud_settings( "cl_cloud_settings", "-1", FCVAR_HIDDEN, "Cloud enabled from (from HKCU\\Software\\Valve\\Steam\\Apps\\appid\\Cloud)", CL_ChangeCloudSettingsCvar );
void CL_InitCloudSettingsCvar()
{
	if ( IsPC()	&& Steam3Client().SteamUtils() )
	{
		int iCloudSettings = -1;
#ifdef PLATFORM_WINDOWS
		char szRegistryKeyLocation[ 256 ];
		Q_snprintf( szRegistryKeyLocation, sizeof( szRegistryKeyLocation ), "HKEY_CURRENT_USER\\Software\\Valve\\Steam\\Apps\\%d\\Cloud", Steam3Client().SteamUtils()->GetAppID() );

		bool bFound = vgui::system()->GetRegistryInteger( szRegistryKeyLocation, iCloudSettings );

		if ( !bFound )
		{
			#ifndef PORTAL2
			// No key yet, use the uninitialized value
			iCloudSettings = -1;
			#else
			// Portal 2 will cloud everything by default if no registry key
			iCloudSettings = STEAMREMOTESTORAGE_CLOUD_ALL;
			#endif
		}

		#if defined( CSTRIKE15 )
		// Cloud isn't used in CS:GO for now
		// This may eventually become optional, but we need to wipe out people's settings
		// So that it's opt-in for existing players in the future
		iCloudSettings = 0;
		#endif
#else
		iCloudSettings = STEAMREMOTESTORAGE_CLOUD_ALL;
#endif
		
		cl_cloud_settings.SetValue( iCloudSettings );
	}
	else
	{
		// If not on PC or steam not available, set to 0 to make sure no replication occurs or is attempted
		cl_cloud_settings.SetValue( 0 );
	}
}


/*
=================
CL_Init
=================
*/
void CL_Init( void )
{	
	for ( int hh = 0; hh < host_state.max_splitscreen_players; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		GetLocalClient().Clear();
	}

	CL_InitLanguageCvar();
	CL_InitCloudSettingsCvar();

#if defined( REPLAY_ENABLED )
	g_pClientReplayHistoryManager->Init();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CL_Shutdown( void )
{
#if defined( REPLAY_ENABLED )
	g_pClientReplayHistoryManager->Shutdown();
#endif
}

CON_COMMAND_F( cl_fullupdate, "Forces the server to send a full update packet", FCVAR_CHEAT )
{
	GetLocalClient().ForceFullUpdate( "cl_fullupdate command" );
}


#ifdef _DEBUG

CON_COMMAND( cl_download, "Downloads a file from server." )
{
	if ( args.ArgC() != 2 )
		return;

	if ( !GetBaseLocalClient().m_NetChannel )
		return;
	
	GetBaseLocalClient().m_NetChannel->RequestFile( args[ 1 ], false ); // just for testing stuff
}

#endif // _DEBUG


CON_COMMAND_F( setinfo, "Adds a new user info value", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	if ( args.ArgC() != 3 )
	{
		Msg("Syntax: setinfo <key> <value>\n");
		return;
	}

	const char *name = args[ 1 ];
	const char *value = args[ 2 ];

	ConCommandBase *pCommand = g_pCVar->FindCommandBase( name );

	ConVarRef sv_cheats( "sv_cheats" );


	if ( pCommand )
	{
		if ( pCommand->IsCommand() )		
		{
			Msg("Name %s is already registered as console command\n", name );
			return;
		}

		if ( !pCommand->IsFlagSet(FCVAR_USERINFO) )
		{
			Msg("Convar %s is already registered but not as user info value\n", name );
			return;
		}

		if ( pCommand->IsFlagSet( FCVAR_NOT_CONNECTED ) )
		{
#ifndef DEDICATED
			// Connected to server?
			if ( GetBaseLocalClient().IsConnected() )
			{
				extern IBaseClientDLL *g_ClientDLL;
				if ( pCommand->IsFlagSet( FCVAR_USERINFO ) && g_ClientDLL && g_ClientDLL->IsConnectedUserInfoChangeAllowed( NULL ) )
				{
					// Client.dll is allowing the convar change
				}
				else
				{
					ConMsg( "Can't change %s when playing, disconnect from the server or switch team to spectators\n", pCommand->GetName() );
					return;
				}
			}
#endif
		}

		if ( IsPC() )
		{
#if !defined(NO_STEAM)
			EUniverse eUniverse = GetSteamUniverse();
			if ( (( eUniverse != k_EUniverseBeta ) && ( eUniverse != k_EUniverseDev )) && pCommand->IsFlagSet( FCVAR_DEVELOPMENTONLY ) )
				return;
#endif 
		}

		if ( pCommand->IsFlagSet( FCVAR_CHEAT ) && sv_cheats.GetBool() == 0  )
		{
			Msg("Convar %s is marked as cheat and cheats are off\n", name );
			return;
		}
	}
	else
	{
		// cvar not found, create it now
		char *pszString = new char[Q_strlen( name ) + 1];
		Q_strcpy( pszString, name );

		pCommand = new ConVar( pszString, "", FCVAR_USERINFO, "Custom user info value" );
	}

	ConVar *pConVar = (ConVar*)pCommand;

	pConVar->SetValue( value );

	if ( GetBaseLocalClient().IsConnected() )
	{
		// send changed cvar to server
		CNETMsg_SetConVar_t convar( name, pConVar->GetString() );
		GetBaseLocalClient().m_NetChannel->SendNetMsg( convar );
	}
}


CON_COMMAND( cl_precacheinfo, "Show precache info (client)." )
{
	if ( args.ArgC() == 2 )
	{
		GetBaseLocalClient().DumpPrecacheStats( args[ 1 ] );
		return;
	}
	
	// Show all data
	GetBaseLocalClient().DumpPrecacheStats( MODEL_PRECACHE_TABLENAME );
	GetBaseLocalClient().DumpPrecacheStats( DECAL_PRECACHE_TABLENAME );
	GetBaseLocalClient().DumpPrecacheStats( SOUND_PRECACHE_TABLENAME );
	GetBaseLocalClient().DumpPrecacheStats( GENERIC_PRECACHE_TABLENAME );
}


CON_COMMAND_F( con_min_severity, "Minimum severity level for messages sent to any logging channel: LS_MESSAGE=0, LS_WARNING=1, LS_ASSERT=2, LS_ERROR=3.", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	if ( args.ArgC() > 1 )
	{
		int nSeverity = V_atoi( args[1] );
		clamp( nSeverity, LS_MESSAGE, LS_HIGHEST_SEVERITY );
		LoggingSystem_SetGlobalSpewLevel( static_cast<LoggingSeverity_t>( nSeverity ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *object - 
//			stringTable - 
//			stringNumber - 
//			*newString - 
//			*newData - 
//-----------------------------------------------------------------------------
void Callback_ModelChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData )
{
	if ( stringTable == GetBaseLocalClient().m_pModelPrecacheTable )
	{
		// Index 0 is always NULL, just ignore it
		// Index 1 == the world, don't 
		if ( stringNumber >= 1 )
		{
			GetBaseLocalClient().SetModel( stringNumber );
		}
	}
	else
	{
		Assert( 0 ) ; // Callback_*Changed called with wrong stringtable
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *object - 
//			stringTable - 
//			stringNumber - 
//			*newString - 
//			*newData - 
//-----------------------------------------------------------------------------
void Callback_GenericChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData )
{
	if ( stringTable == GetBaseLocalClient().m_pGenericPrecacheTable )
	{
		// Index 0 is always NULL, just ignore it
		if ( stringNumber >= 1 )
		{
			GetBaseLocalClient().SetGeneric( stringNumber );
		}
	}
	else
	{
		Assert( 0 ) ; // Callback_*Changed called with wrong stringtable
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *object - 
//			stringTable - 
//			stringNumber - 
//			*newString - 
//			*newData - 
//-----------------------------------------------------------------------------
void Callback_SoundChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData )
{
	if ( stringTable == GetBaseLocalClient().m_pSoundPrecacheTable )
	{
		// Index 0 is always NULL, just ignore it
		if ( stringNumber >= 1 )
		{
			GetBaseLocalClient().SetSound( stringNumber );
		}
	}
	else
	{
		Assert( 0 ) ; // Callback_*Changed called with wrong stringtable
	}
}

void Callback_DecalChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData )
{
	if ( stringTable == GetBaseLocalClient().m_pDecalPrecacheTable )
	{
		GetBaseLocalClient().SetDecal( stringNumber );
	}
	else
	{
		Assert( 0 ) ; // Callback_*Changed called with wrong stringtable
	}
}


void Callback_InstanceBaselineChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData )
{
	Assert( stringTable == GetBaseLocalClient().m_pInstanceBaselineTable );
	GetLocalClient().UpdateInstanceBaseline( stringNumber );
}

void Callback_UserInfoChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData )
{
	Assert( stringTable == GetBaseLocalClient().m_pUserInfoTable );

	// stringnumber == player slot

	player_info_t *player = (player_info_t*)newData;

	if ( !player )
		return; // player left the game

	// request custom user files if necessary
	for ( int i=0; i<MAX_CUSTOM_FILES; i++ )
	{
		GetBaseLocalClient().CheckOthersCustomFile( player->customFiles[i] );
	}

	// fire local client event game event
	IGameEvent * event = g_GameEventManager.CreateEvent( "player_info" );

	if ( event )
	{
		event->SetInt( "userid", player->userID );
		event->SetInt( "friendsid", player->friendsID );
		event->SetUint64( "xuid", player->xuid );
		event->SetInt( "index", stringNumber );
		event->SetString( "name", player->name );
		event->SetString( "networkid", player->guid );
		event->SetBool( "bot", player->fakeplayer );

		g_GameEventManager.FireEventClientSide( event );
	}
}

void Callback_DynamicModelChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData )
{
	extern IVModelInfoClient *modelinfoclient;
	if ( modelinfoclient )
	{
		modelinfoclient->OnDynamicModelStringTableChanged( stringNumber, newString, newData );
	}
}

void CL_HookClientStringTables()
{
	// install hooks
	int numTables = GetBaseLocalClient().m_StringTableContainer->GetNumTables();

	for ( int i =0; i<numTables; i++)
	{
		// iterate through server tables
		CNetworkStringTable *pTable = 
			(CNetworkStringTable*)GetBaseLocalClient().m_StringTableContainer->GetTable( i );

		if ( !pTable )
			continue;

		GetBaseLocalClient().HookClientStringTable( pTable->GetTableName() );
	}
}
// Installs the all, and invokes cb for all existing items
void CL_InstallAndInvokeClientStringTableCallbacks()
{
	VPROF_BUDGET( "CL_InstallAndInvokeClientStringTableCallbacks", VPROF_BUDGETGROUP_OTHER_NETWORKING );

	// install hooks
	int numTables = GetBaseLocalClient().m_StringTableContainer->GetNumTables();

	for ( int i =0; i<numTables; i++)
	{
		// iterate through server tables
		CNetworkStringTable *pTable = 
			(CNetworkStringTable*)GetBaseLocalClient().m_StringTableContainer->GetTable( i );

		if ( !pTable )
			continue;

		pfnStringChanged pOldFunction = pTable->GetCallback();

		GetBaseLocalClient().InstallStringTableCallback( pTable->GetTableName() );

		pfnStringChanged pNewFunction = pTable->GetCallback();
		if ( !pNewFunction )
			continue;

		// We already had it installed (e.g., from client .dll) so all of the callbacks have been called and don't need a second dose
		if ( pNewFunction == pOldFunction )
			continue;

		COM_TimestampedLog( "String Table Callbacks %s - Start", pTable->GetTableName() );

		for ( int j = 0; j < pTable->GetNumStrings(); ++j )
		{
			if ( !( j % 25 ) )
			{
				EngineVGui()->UpdateProgressBar(PROGRESS_DEFAULT);
			}

			int userDataSize;
			const void *pUserData = pTable->GetStringUserData( j, &userDataSize );
			(*pNewFunction)( NULL, pTable, j, pTable->GetString( j ), pUserData );
		}

		COM_TimestampedLog( "String Table Callbacks %s - Finish", pTable->GetTableName() );
	}
}
