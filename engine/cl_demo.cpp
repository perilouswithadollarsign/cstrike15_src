//========= Copyright (c) Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "client_pch.h"
#include "enginestats.h"
#include "iprediction.h"
#include "cl_demo.h"
#include "cl_demoactionmanager.h"
#include "cl_pred.h"

#include "baseautocompletefilelist.h"
#include "demofile/demoformat.h"
#include "gl_matsysiface.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "tier0/icommandline.h"
#include "vengineserver_impl.h"
#include "console.h"
#include "dt_common_eng.h"
#include "gl_model_private.h"
#include "decal.h"
#include "icliententitylist.h"
#include "icliententity.h"
#include "cl_demouipanel.h"
#include "materialsystem/materialsystem_config.h"
#include "tier2/tier2.h"
#include "vgui_baseui_interface.h"
#include "con_nprint.h"
#include "networkstringtableclient.h"
#include "host_cmd.h"
#include "matchmaking/imatchframework.h"
#include "tier0/perfstats.h"
#include "GameEventManager.h"
#include "tier1/bitbuf.h"
#include "net_chan.h"
#include "tier1/characterset.h"
#include "cl_steamauth.h"

#if !defined DEDICATED
#include "sound.h"
#endif

#if IsPlatformWindowsPC()
#define WIN32_LEAN_AND_MEAN
#undef INVALID_HANDLE_VALUE
#include <winsock2.h> // gethostname
#elif !IsGameConsole()
#include <sys/unistd.h> // gethostname
#endif

#ifdef DEDICATED
#include "server.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef _GAMECONSOLE
// Disable demos on consoles by default, to avoid unwanted memory allocations, file I/O and computation
#define ENABLE_DEMOS_BY_DEFAULT false
#else
#define ENABLE_DEMOS_BY_DEFAULT true
#endif

static ConVar demo_recordcommands( "demo_recordcommands", "1", FCVAR_CHEAT, "Record commands typed at console into .dem files." );
static ConVar demo_quitafterplayback( "demo_quitafterplayback", "0", 
#if defined( ALLOW_TEXT_MODE )
	FCVAR_RELEASE,
#else
	0,
#endif
	"Quits game after demo playback." 
);
extern ConVar demo_debug;
static ConVar demo_interpolateview( "demo_interpolateview", "1", 0, "Do view interpolation during dem playback." );
static ConVar demo_pauseatservertick( "demo_pauseatservertick", "0", 0, "Pauses demo playback at server tick" );
static ConVar demo_enabledemos( "demo_enabledemos", ENABLE_DEMOS_BY_DEFAULT ? "1" : "0", 0, "Enable recording demos (must be set true before loading a map)" );
extern ConVar demo_strict_validation;

// singeltons:
static char g_pStatsFile[MAX_OSPATH] = { NULL };
static bool s_bBenchframe = false;

static CDemoRecorder s_ClientDemoRecorder;
CDemoRecorder *g_pClientDemoRecorder = &s_ClientDemoRecorder;
IDemoRecorder *demorecorder = g_pClientDemoRecorder;

static CDemoPlayer s_ClientDemoPlayer;
CDemoPlayer *g_pClientDemoPlayer = &s_ClientDemoPlayer;
IDemoPlayer *demoplayer = g_pClientDemoPlayer;

extern CNetworkStringTableContainer *networkStringTableContainerClient;
CUtlVector<RegisteredDemoCustomDataCallbackPair_t> g_RegisteredDemoCustomDataCallbacks;

// This is the number of units under which we are allowed to interpolate, otherwise pop.
// This fixes problems with in-level transitions.
static ConVar demo_interplimit( "demo_interplimit", "4000", 0, "How much origin velocity before it's considered to have 'teleported' causing interpolation to reset." );
static ConVar demo_avellimit( "demo_avellimit", "2000", 0, "Angular velocity limit before eyes considered snapped for demo playback." );

#define DEMO_HEADER_FILE	"demoheader.tmp"

// Fast forward convars
static ConVar demo_fastforwardstartspeed( "demo_fastforwardstartspeed", "2", 0, "Go this fast when starting to hold FF button." );
static ConVar demo_fastforwardfinalspeed( "demo_fastforwardfinalspeed", "20", 0, "Go this fast when starting to hold FF button." );
static ConVar demo_fastforwardramptime( "demo_fastforwardramptime", "5", 0, "How many seconds it takes to get to full FF speed." );

// highlight convars
static ConVar demo_highlight_timebefore( "demo_highlight_timebefore", "6", 0, "How many seconds before highlight event to stop fast forwarding." );
static ConVar demo_highlight_timeafter( "demo_highlight_timeafter", "4", 0, "How many seconds after highlight event to start fast forwarding." );
static ConVar demo_highlight_fastforwardspeed( "demo_highlight_fastforwardspeed", "10", 0, "Speed to use when fast forwarding to highlights." );
static ConVar demo_highlight_skipthreshold( "demo_highlight_skipthreshold", "10", 0, "Number of seconds between previous highlight event and round start that will fast forward instead of skipping." );

float scr_demo_override_fov = 0.0f;

// Defined in engine
static ConVar cl_interpolate( "cl_interpolate", "1", FCVAR_RELEASE, "Enables or disables interpolation on listen servers or during demo playback" );

void SetPlaybackParametersLockFirstPersonAccountID( uint32 nAccountID );
void CL_ScanDemoDone( const char *pszMode );

//-----------------------------------------------------------------------------
// Purpose: Implements IDemo and handles demo file i/o
// Demos are more or less driven off of network traffic, but there are a few
//  other kinds of data items that are also included in the demo file:  specifically
//  commands that the client .dll itself issued to the engine are recorded, though they
//  probably were not the result of network traffic.
// At the start of a connection to a map/server, all of the signon, etc. network packets
//  are buffered.  This allows us to actually decide to start recording the demo at a later
//  time.  Once we actually issue the recording command, we don't actually start recording 
//  network traffic, but instead we ask the server for an "uncompressed" packet (otherwise
//  we wouldn't be able to deal with the incoming packets during playback because we'd be missing the
//  data to delta from ) and go into a waiting state.  Once an uncompressed packet is received, 
//  we unset the waiting state and start recording network packets from that point forward.
// Demo's record the elapsed time based on the current client clock minus the time the demo was started
// During playback, the elapsed time for playback ( based on the host_time, which is subject to the
//  host_frametime cvar ) is compared with the elapsed time on the message from the demo file.  
// If it's not quite time for the message yet, the demo input stream is rewound
// The demo system sits at the point where the client is checking to see if any network messages
//  have arrived from the server.  If the message isn't ready for processing, the demo system
//  just responds that there are no messages waiting and the client continues on
// Once a true network message with entity data is read from the demo stream, a couple of other
//  actions occur.  First, the timestamp in the demo file and the view origin/angles corresponding
//  to the message are cached off.  Then, we search ahead (into the future) to find out the next true network message
//  we are going to read from the demo file.  We store of it's elapsed time and view origin/angles
// Every frame that the client is rendering, even if there is no data from the demo system,
//  the engine asks the demo system to compute an interpolated origin and view angles.  This
//  is done by taking the current time on the host and figuring out how far that puts us between
//  the last read origin from the demo file and the time when we'll actually read out and use the next origin
// We use Quaternions to avoid gimbel lock on interpolating the view angles
// To make a movie recorded at a fixed frame rate you would simply set the host_framerate to the
//  desired playback fps ( e.g., 0.02 == 50 fps ), then issue the startmovie command, and then
//  play the demo.  The demo system will think that the engine is running at 50 fps and will pull
//  messages accordingly, even though movie recording kills the actually framerate.
// It will also render frames with render origin/angles interpolated in-between the previous and next origins
//  even if the recording framerate was not 50 fps or greater.  The interpolation provides a smooth visual playback 
//  of the demo information to the client without actually adding latency to the view position (because we are
//  looking into the future for the position, not buffering the past data ).
//-----------------------------------------------------------------------------

bool IsControlCommand( unsigned char cmd )
{
	return ( (cmd == dem_signon) || (cmd == dem_stop) ||
			 (cmd == dem_synctick) || (cmd == dem_datatables ) ||
			 (cmd == dem_stringtables) );
}


// Puts a flashing overlay on the screen during demo recording/playback
static ConVar cl_showdemooverlay( "cl_showdemooverlay", "0", 0, "How often to flash demo recording/playback overlay (0 - disable overlay, -1 - show always)" );

class DemoOverlay
{
public:
	DemoOverlay();
	~DemoOverlay();

public:
	void Tick();
	void DrawOverlay( float fSetting );

protected:
	float m_fLastTickTime;
	float m_fLastTickOverlay;
	enum Overlay { OVR_NONE = 0, OVR_REC = 1 << 1, OVR_PLAY = 1 << 2 };
	bool m_bTick;
	int m_maskDrawnOverlay;
} g_DemoOverlay;

DemoOverlay::DemoOverlay() :
	m_fLastTickTime( 0.f ), m_fLastTickOverlay( 0.f ), m_bTick( false ), m_maskDrawnOverlay( OVR_NONE )
{
}

DemoOverlay::~DemoOverlay()
{
}

void DemoOverlay::Tick()
{
	if ( !m_bTick )
	{
		m_bTick = true;

		float const fRealTime = Sys_FloatTime();
		if ( m_fLastTickTime != fRealTime )
		{
			m_fLastTickTime = fRealTime;

			float const fDelta = m_fLastTickTime - m_fLastTickOverlay;
			float const fSettingDelta = cl_showdemooverlay.GetFloat();

			if ( fSettingDelta <= 0.f ||
				fDelta >= fSettingDelta )
			{
				m_fLastTickOverlay = m_fLastTickTime;
				DrawOverlay( fSettingDelta );
			}
		}

		m_bTick = false;
	}
}

void DemoOverlay::DrawOverlay( float fSetting )
{
	int maskDrawnOverlay = OVR_NONE;

	if ( fSetting < 0.f )
	{
		// Keep drawing
		maskDrawnOverlay =
			( demorecorder->IsRecording() ? OVR_REC : 0 ) |
			( demoplayer->IsPlayingBack() ? OVR_PLAY : 0 );
	}
	else if ( fSetting == 0.f )
	{
		// None
		maskDrawnOverlay = OVR_NONE;
	}
	else
	{
		// Flash
		maskDrawnOverlay = ( !m_maskDrawnOverlay ) ? (
			( demorecorder->IsRecording() ? OVR_REC : 0 ) |
			( demoplayer->IsPlayingBack() ? OVR_PLAY : 0 )
			) : OVR_NONE;
	}

	int const idx = 1;

	if ( OVR_NONE == maskDrawnOverlay &&
		 OVR_NONE != m_maskDrawnOverlay )
	{
		con_nprint_s xprn;
		memset( &xprn, 0, sizeof( xprn ) );
		xprn.index = idx;
		xprn.time_to_live = -1;
		Con_NXPrintf( &xprn, "" );
	}
	
	if ( OVR_PLAY & maskDrawnOverlay )
	{
		con_nprint_s xprn;
		memset( &xprn, 0, sizeof( xprn ) );
		xprn.index = idx;
		xprn.color[0] = 0.f;
		xprn.color[1] = 1.f;
		xprn.color[2] = 0.f;
		xprn.fixed_width_font = true;
		xprn.time_to_live = ( fSetting > 0.f ) ? fSetting : 1.f;
		Con_NXPrintf( &xprn, "  PLAY   " );
	}
	
	if ( OVR_REC & maskDrawnOverlay )
	{
		con_nprint_s xprn;
		memset( &xprn, 0, sizeof( xprn ) );
		xprn.index = idx;
		xprn.color[0] = 1.f;
		xprn.color[1] = 0.f;
		xprn.color[2] = 0.f;
		xprn.fixed_width_font = true;
		xprn.time_to_live = ( fSetting > 0.f ) ? fSetting : 1.f;
		Con_NXPrintf( &xprn, "   REC   " );
	}

	m_maskDrawnOverlay = maskDrawnOverlay;
}
			

//-----------------------------------------------------------------------------
// Purpose: Mark whether we are waiting for the first uncompressed update packet
// Input  : waiting - 
//-----------------------------------------------------------------------------
void CDemoRecorder::SetSignonState(SIGNONSTATE state)
{
	if ( demoplayer->IsPlayingBack() )
		return;

	if ( !demo_enabledemos.GetBool() )
		return;

	if ( state == SIGNONSTATE_NEW )
	{
		if ( m_DemoFile.IsOpen() )
		{
			// we are already recording a demo file
			CloseDemoFile();

			// prepare for recording next demo
			m_nDemoNumber++; 
		}

		StartupDemoHeader();
	}
	else if ( state == SIGNONSTATE_SPAWN )
	{
		// close demo file header when this packet is finished
		m_bCloseDemoFile = true;
	}
	else if ( state == SIGNONSTATE_FULL )
	{
		if ( m_bRecording )
		{
			StartupDemoFile();
		}
	}
}

int CDemoRecorder::GetRecordingTick( void )
{
	if ( GetBaseLocalClient().m_nMaxClients > 1 )
	{
		return TIME_TO_TICKS( net_time ) - m_nStartTick;
	}
	else
	{
		return GetBaseLocalClient().GetClientTickCount() - m_nStartTick;
	}
}

void CDemoRecorder::ResyncDemoClock()
{
	if ( GetBaseLocalClient().m_nMaxClients > 1 )
	{
		m_nStartTick = TIME_TO_TICKS( net_time );
	}
	else
	{
		m_nStartTick = GetBaseLocalClient().GetClientTickCount();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : info - 
//-----------------------------------------------------------------------------
void CDemoRecorder::GetClientCmdInfo( democmdinfo_t& cmdInfo )
{
	for ( int hh = 0; hh < host_state.max_splitscreen_players; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
		democmdinfo_t::Split_t &info = cmdInfo.u[ hh ];

		info.flags		= FDEMO_NORMAL;

	if( m_bResetInterpolation )
	{
		info.flags |= FDEMO_NOINTERP;
		m_bResetInterpolation = false;
	}

		g_pClientSidePrediction->GetViewOrigin( info.viewOrigin );
	#ifndef DEDICATED
		info.viewAngles = GetLocalClient().viewangles;
	#endif
		g_pClientSidePrediction->GetLocalViewAngles( info.localViewAngles );

		// Nothing by default
		info.viewOrigin2.Init();
		info.viewAngles2.Init();
		info.localViewAngles2.Init();
	}

	m_bResetInterpolation = false;
}

void CDemoRecorder::WriteSplitScreenPlayers()
{
	char		data[NET_MAX_PAYLOAD];
	bf_write	msg;

	msg.StartWriting( data, NET_MAX_PAYLOAD );
	msg.SetDebugName( "DemoFileWriteSplitScreenPlayers" );

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( i )
	{
		if ( i == 0 )
			continue;

		CSVCMsg_SplitScreen_t ss;
		ss.set_type( MSG_SPLITSCREEN_ADDUSER );
		ss.set_slot( i );
		ss.set_player_index( splitscreen->GetSplitScreenPlayerEntity( i ) );

		ss.WriteToBuffer( msg );
	}

	WriteMessages( msg );
}

void CDemoRecorder::WriteBSPDecals()
{
	decallist_t	*decalList = (decallist_t*)malloc( sizeof(decallist_t) * Draw_DecalMax() );
	
	int decalcount = DecalListCreate( decalList );

	char		data[NET_MAX_PAYLOAD];
	bf_write	msg;

	msg.StartWriting( data, NET_MAX_PAYLOAD );
	msg.SetDebugName( "DemoFileWriteBSPDecals" );

	for ( int i = 0; i < decalcount; i++ )
	{
		decallist_t *entry = &decalList[ i ];

		CSVCMsg_BSPDecal_t decal;

		bool found = false;

		IClientEntity *clientEntity = entitylist->GetClientEntity( entry->entityIndex );

		if ( !clientEntity )
			continue;

		
		const model_t * pModel = clientEntity->GetModel();

		decal.mutable_pos()->set_x( entry->position.x );
		decal.mutable_pos()->set_y( entry->position.y );
		decal.mutable_pos()->set_z( entry->position.z );
		decal.set_entity_index( entry->entityIndex );
		decal.set_decal_texture_index( Draw_DecalIndexFromName( entry->name, &found ) );
		decal.set_model_index( pModel ? GetBaseLocalClient().LookupModelIndex( modelloader->GetName( pModel ) ) : 0 );
		decal.WriteToBuffer( msg );
	}

	WriteMessages( msg );
	
	free( decalList );
}

void CDemoRecorder::RecordServerClasses( ServerClass *pClasses )
{
	MEM_ALLOC_CREDIT();

	if ( !m_DemoFile.IsOpen() )
		return;

	char *pBigBuffer;
	CUtlBuffer bigBuff;

	int buffSize = DEMO_RECORD_BUFFER_SIZE;
	// keep temp large allocations off of stack
	bigBuff.EnsureCapacity( buffSize );
	pBigBuffer = (char*)bigBuff.Base();

	bf_write buf( "CDemoRecorder::RecordServerClasses", pBigBuffer, buffSize );

	// Send SendTable info.
	DataTable_WriteSendTablesBuffer( pClasses, &buf );

	// Send class descriptions.
	DataTable_WriteClassInfosBuffer( pClasses, &buf );

	if ( buf.GetNumBitsLeft() <= 0 )
	{
		Sys_Error( "unable to record server classes\n" );
	}

	// Now write the buffer into the demo file
	m_DemoFile.WriteNetworkDataTables( &buf, GetRecordingTick() );
}

void CDemoRecorder::RecordStringTables()
{
	MEM_ALLOC_CREDIT();

	if ( !m_DemoFile.IsOpen() )
		return;

	char *pBigBuffer;
	CUtlBuffer bigBuff;

	int buffSize = DEMO_RECORD_BUFFER_SIZE;
	// keep temp large allocations off of stack
	bigBuff.EnsureCapacity( buffSize );
	pBigBuffer = (char*)bigBuff.Base();

	bf_write buf( pBigBuffer, buffSize );

	networkStringTableContainerClient->WriteStringTables( buf );

	if ( buf.GetNumBitsLeft() <= 0 )
	{
		Sys_Error( "unable to record server classes\n" );
	}

	// Now write the buffer into the demo file
	m_DemoFile.WriteStringTables( &buf, GetRecordingTick() );
}

void CDemoRecorder::RecordCustomData( int iCallbackIndex, const void *pData, size_t iDataLength )
{
	if ( !m_DemoFile.IsOpen() )
		return;

	m_DemoFile.WriteCustomData( iCallbackIndex, pData, iDataLength, GetRecordingTick() );
}


void CDemoRecorder::RecordUserInput( int cmdnumber )
{
	if ( !m_DemoFile.IsOpen() )
		return;

	char buffer[256];
	bf_write msg( "CDemo::WriteUserCmd", buffer, sizeof(buffer) );

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	g_ClientDLL->EncodeUserCmdToBuffer( nSlot, msg, cmdnumber );

	m_DemoFile.WriteUserCmd( cmdnumber, buffer, msg.GetNumBytesWritten(), GetRecordingTick(), nSlot );
}

void CDemoRecorder::ResetDemoInterpolation( void )
{
	m_bResetInterpolation = true;
}


//-----------------------------------------------------------------------------
// Purpose: saves all cvars falgged with FVAR_DEMO to demo file
//-----------------------------------------------------------------------------
void CDemoRecorder::WriteDemoCvars()
{
	if ( !m_DemoFile.IsOpen() )
		return;

	ICvar::Iterator iter( g_pCVar );
	for ( iter.SetFirst() ; iter.IsValid() ; iter.Next() )
	{
		ConCommandBase *var = iter.Get();
		if ( var->IsCommand() )
			continue;

		const ConVar *cvar = ( const ConVar * )var;

		if ( !cvar->IsFlagSet( FCVAR_DEMO ) )
			continue;

		char cvarcmd[MAX_OSPATH];

		V_sprintf_safe( cvarcmd,"%s \"%s\"", cvar->GetName(), Host_CleanupConVarStringValue( cvar->GetString() ) );

		m_DemoFile.WriteConsoleCommand( cvarcmd, GetRecordingTick(), 0 );
	}
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *cmdname - 
//-----------------------------------------------------------------------------
void CDemoRecorder::RecordCommand( const char *cmdstring )
{
	if ( !IsRecording() )
		return;

	if ( !cmdstring || !cmdstring[0] )
		return;

	if ( !demo_recordcommands.GetInt() )
		return;

	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	m_DemoFile.WriteConsoleCommand( cmdstring, GetRecordingTick(), GET_ACTIVE_SPLITSCREEN_SLOT() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoRecorder::StartupDemoHeader( void )
{
	CloseDemoFile();	// make sure it's closed

	// Note: this is replacing tmpfile()
	if ( !m_DemoFile.Open( DEMO_HEADER_FILE, false ) )
	{
		ConDMsg ("ERROR: couldn't open temporary header file.\n");
		return;
	}

	m_bIsDemoHeader = true;

	Assert( m_MessageData.GetBasePointer() == NULL );

	// setup writing data buffer
	m_MessageData.StartWriting( new unsigned char[NET_MAX_PAYLOAD], NET_MAX_PAYLOAD );
	m_MessageData.SetDebugName( "DemoHeaderWriteBuffer" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoRecorder::StartupDemoFile( void )
{
	if ( !m_bRecording )
		return;

	// Already recording!!!
	if ( m_DemoFile.IsOpen() )
		return;

	if ( !demo_enabledemos.GetBool() )
	{
		Warning( "DEMO: cannot start recording a demo (set 'demo_enabledemos' to 1 and restart the map to enable demos)\n" );
		return;
	}

	char demoFileName[MAX_OSPATH];

	if ( m_nDemoNumber <= 1 )
	{
		V_sprintf_safe( demoFileName, "%s.dem", m_szDemoBaseName );
	}
	else
	{
		V_sprintf_safe( demoFileName, "%s_%i.dem", m_szDemoBaseName, m_nDemoNumber );
	}

	if ( !m_DemoFile.Open( demoFileName, false ) )
		return;

	// open demo header file containing sigondata
	FileHandle_t hDemoHeader = g_pFileSystem->OpenEx( DEMO_HEADER_FILE, "rb", IsGameConsole() ? FSOPEN_NEVERINPACK : 0 );
	if ( hDemoHeader == FILESYSTEM_INVALID_HANDLE )
	{
		ConMsg ("StartupDemoFile: couldn't open demo file header.\n");
		return;
	}

	Assert( m_MessageData.GetBasePointer() == NULL );

	// setup writing data buffer
	m_MessageData.StartWriting( new unsigned char[NET_MAX_PAYLOAD], NET_MAX_PAYLOAD );
	m_MessageData.SetDebugName( "DemoFileWriteBuffer" );

	// fill demo header info
	demoheader_t *dh = &m_DemoFile.m_DemoHeader;
	V_memset(dh, 0, sizeof(demoheader_t));

	dh->demoprotocol = DEMO_PROTOCOL;
	dh->networkprotocol = GetHostVersion();
	V_strcpy_safe(dh->demofilestamp, DEMO_HEADER_ID );

	V_FileBase( modelloader->GetName( host_state.worldmodel ), dh->mapname, sizeof( dh->mapname ) );

	char szGameDir[MAX_OSPATH];
	V_strcpy_safe(szGameDir, com_gamedir );
	V_FileBase( szGameDir, dh->gamedirectory, sizeof( dh->gamedirectory ) );

	V_strcpy_safe( dh->servername, GetBaseLocalClient().m_Remote.Get( 0 ).m_szRetryAddress );
	V_strcpy_safe( dh->clientname, cl_name.GetString() );

	
	// goto end	of demo header 
	g_pFileSystem->Seek(hDemoHeader, 0, FILESYSTEM_SEEK_TAIL);
	// get size	signon data size
	dh->signonlength = g_pFileSystem->Tell(hDemoHeader);
	// go back to start
	g_pFileSystem->Seek(hDemoHeader, 0, FILESYSTEM_SEEK_HEAD);
	
	// write demo file header info
	m_DemoFile.WriteDemoHeader();
	
	// copy signon data from header file to demo file
	m_DemoFile.WriteFileBytes( hDemoHeader, dh->signonlength );

	// close but keep header file, we might need it for a second record
	g_pFileSystem->Close( hDemoHeader );
	
	m_nFrameCount = 0;
	m_bIsDemoHeader = false;
		
	ResyncDemoClock(); // reset demo clock
		
	// tell client to sync demo clock too 
	m_DemoFile.WriteCmdHeader( dem_synctick, 0, 0 );

	//write out the custom data callback table if we have any entries
	int iCustomDataCallbacks = g_RegisteredDemoCustomDataCallbacks.Count();
	if( iCustomDataCallbacks != 0 )
	{
		size_t iCombinedStringLength = 0;
		for( int i = 0; i != iCustomDataCallbacks; ++i )
		{
			iCombinedStringLength += V_strlen( STRING( g_RegisteredDemoCustomDataCallbacks[i].szSaveID ) ) + 1;
		}

		size_t iTotalDataSize = iCombinedStringLength + sizeof( int );
		uint8 *pWriteBuffer = (uint8 *)stackalloc( iTotalDataSize );
		uint8 *pWrite = pWriteBuffer;

		iCustomDataCallbacks = LittleDWord( iCustomDataCallbacks );
		*(int *)pWrite = iCustomDataCallbacks;
		pWrite += sizeof( int );
		for( int i = 0; i != iCustomDataCallbacks; ++i )
		{
			size_t iStringLength = V_strlen( STRING( g_RegisteredDemoCustomDataCallbacks[i].szSaveID ) ) + 1;
			memcpy( pWrite, STRING( g_RegisteredDemoCustomDataCallbacks[i].szSaveID ), iStringLength );
			pWrite += iStringLength;
		}

		Assert( pWrite == (pWriteBuffer + iTotalDataSize) );

		m_DemoFile.WriteCustomData( -1, pWriteBuffer, iTotalDataSize, 0 );
	}
	
	RecordStringTables();

	// Demo playback should read this as an incoming message.
	WriteDemoCvars(); // save all cvars marked with FCVAR_DEMO

	WriteBSPDecals();

	// Dump all accumulated avatar data messages into the starting portion of the demo file
	CNETMsg_PlayerAvatarData_t *pMsgMyOwnAvatarData = GetBaseLocalClient().AllocOwnPlayerAvatarData();
	FOR_EACH_MAP_FAST( GetBaseLocalClient().m_mapPlayerAvatarData, iData )
	{
		CNETMsg_PlayerAvatarData_t &msgPlayerAvatarData = *GetBaseLocalClient().m_mapPlayerAvatarData.Element( iData );

		// if the server authoritative data overrides local version of avatar data then don't write local version
		if ( pMsgMyOwnAvatarData && ( msgPlayerAvatarData.accountid() == pMsgMyOwnAvatarData->accountid() ) )
		{
			delete pMsgMyOwnAvatarData;
			pMsgMyOwnAvatarData = NULL;
		}

		byte		buffer[ NET_MAX_PAYLOAD ];
		bf_write	bfWrite( "CDemoRecorder::NETMsg_PlayerAvatarData", buffer, sizeof( buffer ) );
		msgPlayerAvatarData.WriteToBuffer( bfWrite );

		WriteMessages( bfWrite );
	}
	if ( pMsgMyOwnAvatarData )
	{
		byte		buffer[ NET_MAX_PAYLOAD ];
		bf_write	bfWrite( "CDemoRecorder::NETMsg_PlayerAvatarData", buffer, sizeof( buffer ) );
		pMsgMyOwnAvatarData->WriteToBuffer( bfWrite );

		WriteMessages( bfWrite );

		delete pMsgMyOwnAvatarData;
		pMsgMyOwnAvatarData = NULL;
	}

	g_ClientDLL->HudReset();

	if ( splitscreen->GetNumSplitScreenPlayers() > 1 )
	{
		WriteSplitScreenPlayers();
	}

	//  tell server that we started recording a demo
	GetBaseLocalClient().SendStringCmd( "demorestart" );

	ConMsg ("Recording to %s...\n", demoFileName);
}

CDemoRecorder::CDemoRecorder()
{
}

CDemoRecorder::~CDemoRecorder()
{
	CloseDemoFile();	
}

CDemoFile *CDemoRecorder::GetDemoFile()
{
	return &m_DemoFile;
}

void CDemoRecorder::ResumeRecording()
{

}

void CDemoRecorder::PauseRecording()
{

}


void CDemoRecorder::CloseDemoFile()
{
	if ( m_DemoFile.IsOpen())
	{
		if ( !m_bIsDemoHeader )
		{
			// Demo playback should read this as an incoming message.
			m_DemoFile.WriteCmdHeader( dem_stop, GetRecordingTick(), 0 );

			// update demo header infos
			m_DemoFile.m_DemoHeader.playback_ticks	= GetRecordingTick();
			m_DemoFile.m_DemoHeader.playback_time	= host_state.interval_per_tick * GetRecordingTick();
			m_DemoFile.m_DemoHeader.playback_frames = m_nFrameCount;

			// go back to header and write demoHeader with correct time and #frame again
			m_DemoFile.WriteDemoHeader();

			ConMsg ("Completed demo, recording time %.1f, game frames %i.\n", 
				m_DemoFile.m_DemoHeader.playback_time, m_DemoFile.m_DemoHeader.playback_frames );
		}

		if ( demo_debug.GetInt() )
		{
			ConMsg ("Closed demo file, %i bytes.\n", m_DemoFile.GetSize() );
		}

		m_DemoFile.Close();
	}

	m_bCloseDemoFile = false;
	m_bIsDemoHeader = false;

	// clear writing data buffer
	if ( m_MessageData.GetBasePointer() )
	{
		delete [] m_MessageData.GetBasePointer();
		m_MessageData.StartWriting( NULL, 0 );
	}
}

void CDemoRecorder::RecordMessages(bf_read &data, int bits)
{
	if ( m_MessageData.GetBasePointer() && (bits>0) )
	{
		m_MessageData.WriteBitsFromBuffer( &data, bits );

		Assert( !m_MessageData.IsOverflowed() );
	}
}

void CDemoRecorder::RecordPacket()
{
	WriteMessages( m_MessageData );

	m_MessageData.Reset(); // clear message buffer
	
	if ( m_bCloseDemoFile )
	{
		CloseDemoFile();
	}
}

void CDemoRecorder::WriteMessages( bf_write &message )
{
	if ( !m_DemoFile.IsOpen() )
		return;

	int len = message.GetNumBytesWritten();

	if (len <= 0)
		return;

	// fill last bits in last byte with NOP if necessary
	int nRemainingBits = message.GetNumBitsWritten() % 8;
	if ( nRemainingBits > 0 &&  nRemainingBits <= (8-NETMSG_TYPE_BITS) )
	{
		CNETMsg_NOP_t nop;
		nop.WriteToBuffer( message );
	}

	Assert( len < NET_MAX_MESSAGE );

	// if signondata read as fast as possible, no rewind
	// and wait for packet time
	unsigned char cmd = m_bIsDemoHeader ? dem_signon : dem_packet;

	if ( cmd == dem_packet )
	{
		m_nFrameCount++;
	}

	// write command & time
	m_DemoFile.WriteCmdHeader( cmd, GetRecordingTick(), 0 ); 
	
	democmdinfo_t info;
	// Snag current info
	GetClientCmdInfo( info );
		
	// Store it
	m_DemoFile.WriteCmdInfo( info );
		
	// write network channel sequencing infos
	int nOutSequenceNr, nInSequenceNr, nOutSequenceNrAck;
	GetBaseLocalClient().m_NetChannel->GetSequenceData( nOutSequenceNr, nInSequenceNr, nOutSequenceNrAck );
	m_DemoFile.WriteSequenceInfo( nInSequenceNr, nOutSequenceNrAck );
	
	// Output the messge buffer.
	m_DemoFile.WriteRawData( (char*) message.GetBasePointer(), len );

	if ( demo_debug.GetInt() >= 1 )
	{
		Msg( "Writing demo message %i bytes at file pos %i\n", len, m_DemoFile.GetCurPos( false ) );
	}
}

//-----------------------------------------------------------------------------
// Purpose: stop recording a demo
//-----------------------------------------------------------------------------
void CDemoRecorder::StopRecording( const CGameInfo *pGameInfo )
{
	if ( !IsRecording() )
	{
		return;
	}

	if ( m_MessageData.GetBasePointer() )
	{
		delete[] m_MessageData.GetBasePointer();
		m_MessageData.StartWriting( NULL, 0);
	}

	CloseDemoFile();
	
	m_bRecording = false;
	m_nDemoNumber = 0;

	g_DemoOverlay.Tick();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			track - 
//-----------------------------------------------------------------------------
void CDemoRecorder::StartRecording( const char *name, bool bContinuously )
{
	V_strcpy_safe( m_szDemoBaseName, name );
	
	m_bRecording		 = true;
	m_nDemoNumber		 = 1;
	m_bResetInterpolation = false;


	g_DemoOverlay.Tick();

	// request a full game update from server 
	GetBaseLocalClient().ForceFullUpdate( "recording demo" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDemoRecorder::IsRecording( void )
{
	g_DemoOverlay.Tick();

	return m_bRecording;
}



//
// Demo highlight helpers
//

struct DemoUserInfo_t
{
	DemoUserInfo_t()
		: userID( -1 )
		, name( NULL )
	{
	}
	int userID;
	char *name;
	CSteamID steamID;
};

CUtlVector< DemoUserInfo_t > s_demoUserInfo;

void Callback_DemoScanUserInfoChanged( void *object, INetworkStringTable *stringTable, int stringNumber, const char *newString, const void *newData )
{
	// stringnumber == player slot

	player_info_t *player = (player_info_t*)newData;

	if ( !player )
	{
		if ( s_demoUserInfo.Count() > stringNumber )
		{
			// clear out the entry in our user info array
			s_demoUserInfo[ stringNumber ].userID = -1;
			s_demoUserInfo[ stringNumber ].name = NULL;
			s_demoUserInfo[ stringNumber ].steamID.SetFromUint64( 0 );
		}
		return; // player left the game
	}

	CByteswap byteswap;
	byteswap.SetTargetBigEndian( true );
	byteswap.SwapFieldsToTargetEndian( player );

	if ( s_demoUserInfo.Count() <= stringNumber )
	{
		s_demoUserInfo.SetCountNonDestructively( stringNumber + 1 );
	}

	DemoUserInfo_t &entry = s_demoUserInfo.Element( stringNumber );

	entry.userID = player->userID;
	entry.name = player->name;
	entry.steamID.SetFromUint64( player->xuid );
}

void GetExtraDeathEventInfo( KeyValues *pKeys )
{
	int nAttackerID = pKeys->GetInt( "attacker" );
	int nVictimID = pKeys->GetInt( "userid" );
	int nAssisterID = pKeys->GetInt( "assister" );

	FOR_EACH_VEC( s_demoUserInfo, i )
	{
		if ( nAttackerID == s_demoUserInfo[ i ].userID )
		{
			pKeys->SetString( "attacker_name", s_demoUserInfo[ i ].name );
			pKeys->SetUint64( "attacker_xuid", s_demoUserInfo[ i ].steamID.ConvertToUint64() );
		}
		else if ( nVictimID == s_demoUserInfo[ i ].userID )
		{
			pKeys->SetString( "victim_name", s_demoUserInfo[ i ].name );
			pKeys->SetUint64( "victim_xuid", s_demoUserInfo[ i ].steamID.ConvertToUint64() );
		}
		else if ( nAssisterID == s_demoUserInfo[ i ].userID )
		{
			pKeys->SetString( "assister_name", s_demoUserInfo[ i ].name );
			pKeys->SetUint64( "assister_xuid", s_demoUserInfo[ i ].steamID.ConvertToUint64() );
		}
	}
}

void GetExtraEventInfo( KeyValues *pKeys )
{
	int nUserID = pKeys->GetInt( "userid" );
	FOR_EACH_VEC( s_demoUserInfo, i )
	{
		if ( nUserID == s_demoUserInfo[ i ].userID )
		{
			pKeys->SetString( "player_name", s_demoUserInfo[ i ].name );
			pKeys->SetUint64( "player_xuid", s_demoUserInfo[ i ].steamID.ConvertToUint64() );
		}
	}
}

static int s_nMaxViewers = 0;
static int s_nMaxExternalTotal = 0;
static int s_nMaxExternalLinked = 0;
static int s_nMaxCombinedViewers = 0;

void GetEventInfoForScan( const char *pszMode, KeyValues *pKeys )
{
	if ( V_strcasecmp( pszMode, "hltv_status" ) == 0 )
	{
		int nClients = pKeys->GetInt( "clients", -1 );
		int nProxies = pKeys->GetInt( "proxies", -1 );
		int nExternalTotal = pKeys->GetInt( "externaltotal", -1 );
		int nExternalLinked = pKeys->GetInt( "externallinked", -1 );

		Msg( " GOTV Viewers: %d  External Viewers: %d  Linked: %d Combined Viewers: %d \n", nClients - nProxies, nExternalTotal, nExternalLinked, ( nClients - nProxies ) + nExternalTotal );

		if ( ( nClients - nProxies ) > s_nMaxViewers )
		{
			s_nMaxViewers = nClients - nProxies;
		}
		if ( nExternalTotal > s_nMaxExternalTotal )
		{
			s_nMaxExternalTotal = nExternalTotal;
		}
		if ( nExternalLinked > s_nMaxExternalLinked )
		{
			s_nMaxExternalLinked = nExternalLinked;
		}
		if ( ( ( nClients - nProxies ) + nExternalTotal ) > s_nMaxCombinedViewers )
		{
			s_nMaxCombinedViewers = ( nClients - nProxies ) + nExternalTotal;
		}
	}
}


bool CheckKeyXuid( const CSteamID &steamID, KeyValues *pKeys, const char* pKeyWithXuid )
{
	CSteamID playerSteamID( pKeys->GetUint64( pKeyWithXuid ) );
	if ( steamID.GetAccountID() == playerSteamID.GetAccountID() )
	{
		return true;
	}

	return false;
}

int GetPlayerIndex( const CSteamID &steamID )
{
	FOR_EACH_VEC( s_demoUserInfo, i )
	{
		if ( s_demoUserInfo[ i ].steamID.GetAccountID() == steamID.GetAccountID() )
		{
			return i + 1;
		}
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: Called when a demo file runs out, or the user starts a game
// Output : void CDemo::StopPlayback
//-----------------------------------------------------------------------------
void CDemoPlayer::StopPlayback( void )
{
	if ( !IsPlayingBack() )
		return;

	demoaction->StopPlaying();

	m_DemoFile.Close();
	m_bPlayingBack = false;
	m_bPlaybackPaused = false;
	m_flAutoResumeTime = 0.0f;

	if ( m_bTimeDemo )
	{
		g_EngineStats.EndRun();

		if ( !s_bBenchframe )
		{
			WriteTimeDemoResults();
		}
		else
		{
			mat_norendering.SetValue( 0 );
		}

		m_bTimeDemo = false;
	}
	else
	{
		int framecount = host_framecount - m_nTimeDemoStartFrame;
		float demotime = Sys_FloatTime() - m_flTimeDemoStartTime;

		if ( demotime > 0.0f )
		{
			DevMsg( "Demo playback finished ( %.1f seconds, %i render frames, %.2f fps).\n", demotime, framecount, framecount/demotime);
		}

	}
	
	m_flPlaybackRateModifier = 1.0f;

	delete[] m_DemoPacket.data;
	m_DemoPacket.data = NULL;

	scr_demo_override_fov = 0.0f;

	if ( demo_quitafterplayback.GetBool() )
	{
		Cbuf_AddText( Cbuf_GetCurrentPlayer(), "quit\n" );
	}

	g_ClientDLL->OnDemoPlaybackStop();

	FOR_EACH_VEC( m_ImportantTicks, i )
	{
		if ( m_ImportantTicks[ i ].pKeys )
		{
			m_ImportantTicks[ i ].pKeys->deleteThis();
		}
	}
	m_ImportantTicks.RemoveAll();
	m_ImportantGameEvents.RemoveAll();
	m_highlights.RemoveAll();
	m_nCurrentHighlight = -1;
	m_highlightSteamID.SetFromUint64( 0 );
	m_nHighlightPlayerIndex = -1;
	m_bScanMode = false;
	m_szScanMode[0] = 0;
}





#define SKIP_TO_TICK_FLAG uint32( uint32( 0x88 ) << 24 )

bool CDemoPlayer::IsSkipping( void )const
{
	return m_bPlayingBack && ( ( m_nSkipToTick != -1 ) || g_ClientDLL->ShouldSkipEvidencePlayback( m_pPlaybackParameters ) );
}

int CDemoPlayer::GetTotalTicks(void)
{
	return m_DemoFile.m_DemoHeader.playback_ticks; // == m_DemoFile.GetTotalTicks();		
}

void CDemoPlayer::SetPacketReadSuspended( bool bSuspendPacketReading )
{
	if ( m_bPacketReadSuspended == bSuspendPacketReading )
		return; // same state

	m_bPacketReadSuspended = bSuspendPacketReading;
	if ( !m_bPacketReadSuspended )
		ResyncDemoClock(); // Make sure we resync demo clock when we resume packet reading
}

void CDemoPlayer::SkipToTick( int tick, bool bRelative, bool bPause )
{
	if ( m_bPacketReadSuspended )
		return; // demo ticks and host ticks aren't resync'd when packet read is suspended

	if ( bRelative )
	{
		tick = GetPlaybackTick() + tick;
	}

	if ( tick < 0 )
		return;

	if ( tick < GetPlaybackTick() )
	{
		if ( m_pPlaybackParameters && m_pPlaybackParameters->m_bAnonymousPlayerIdentity )
		{
			Msg( "Going backwards not available in Overwatch!\n" );
			return;
		}
		RestartPlayback();

#if 0 // old way
		// we have to reload the whole demo file
		// we need to create a temp copy of the filename
		char fileName[MAX_OSPATH];
		V_strcpy_safe( fileName, m_DemoFile.m_szFileName );

		StopPlayback();

		// disconnect before reloading demo, to avoid sometimes loading into game instead of demo
		GetBaseLocalClient().Disconnect(false);

		// reload current demo file
		StartPlayback( fileName, m_bTimeDemo, NULL );

		// Make sure the proper skipping occurs after reload
		if ( tick > 0 )
			tick |= SKIP_TO_TICK_FLAG;
#endif
	}

	if ( tick != GetPlaybackTick() )
	{
		m_nSkipToTick = tick;
	}

	if ( bPause || m_bPlaybackPaused )
	{
		int nTicksPerFrame = m_DemoFile.GetTicksPerFrame( );
		m_nTickToPauseOn = tick + nTicksPerFrame;
		m_bSavedInterpolateState = cl_interpolate.GetBool();
		cl_interpolate.SetValue( 0 );
		ResumePlayback();
	}
}

void CDemoPlayer::SkipToImportantTick( const DemoImportantTick_t *pTick )
{
	if ( m_bPacketReadSuspended )
		return; // demo ticks and host ticks aren't resync'd when packet read is suspended

	int nStartTick = GetPlaybackTick();
	int nTargetTick = pTick->nPreviousTick;

	int nTicksBeforeEvent = m_ImportantGameEvents[ pTick->nImportanGameEventIndex ].flSeekTimeBefore / host_state.interval_per_tick;
	if ( nTicksBeforeEvent > 0 )
	{
		int nTargetTick = pTick->nTick - nTicksBeforeEvent;

		// handle case where desired next tick is close to where we already are and going to 'ticks before event' would cause us to seek backwards.
		// instead we won't skip at all
		if ( pTick->nTick >= nStartTick && nTargetTick < nStartTick )
		{
			nTargetTick = nStartTick;
		}

		if ( nTargetTick < 0 )
		{
			nTargetTick = 0;
		}
	}

	if ( nTargetTick < nStartTick )
	{
		if ( m_pPlaybackParameters && m_pPlaybackParameters->m_bAnonymousPlayerIdentity )
		{
			Msg( "Going backwards not available in Overwatch!\n" );
			return;
		}
		RestartPlayback();
	}

	if ( nTargetTick != nStartTick )
	{
		m_nSkipToTick = nTargetTick;
	}

	if ( m_bPlaybackPaused )
	{
		// this code sets things up so that we will pause once we reach the tick.
		int nTicksPerFrame = m_DemoFile.GetTicksPerFrame();
		m_nTickToPauseOn = pTick->nTick + nTicksPerFrame;
		// turn interpolation off for the time between seeking and playing until pause state
		m_bSavedInterpolateState = cl_interpolate.GetBool();
		cl_interpolate.SetValue( 0 );
		ResumePlayback();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Read in next demo message and send to local client over network channel, if it's time.
// Output : bool 
//-----------------------------------------------------------------------------
bool CDemoPlayer::ParseAheadForInterval( int curtick, int intervalticks )
{
	int			tick = 0;
	int			dummy;
	byte		cmd = dem_stop;

	democmdinfo_t	nextinfo;

	long		starting_position = m_DemoFile.GetCurPos( true );

	// remove all entrys older than 32 ticks
	while ( m_DestCmdInfo.Count() > 0 )
	{
		DemoCommandQueue& entry = m_DestCmdInfo[ 0 ];

		if ( entry.tick >= (curtick - 32)  )
			break;

		if ( entry.filepos >= starting_position )
			break;

		 m_DestCmdInfo.Remove( 0 );
	}

	if ( m_bTimeDemo ) 
		return false;

	while ( true )
	{
		// skip forward to the next dem_packet or dem_signon
		bool swallowmessages = true;
		do
		{
			int nPlayerSlot = 0;
			m_DemoFile.ReadCmdHeader( cmd, tick, nPlayerSlot );

			// COMMAND HANDLERS
			switch ( cmd )
			{
			case dem_synctick:
			case dem_stop:
				{
					m_DemoFile.SeekTo( starting_position, true );
					return false;
				}
				break;
			case dem_consolecmd:
				{
					ACTIVE_SPLITSCREEN_PLAYER_GUARD( nPlayerSlot );
					m_DemoFile.ReadConsoleCommand();
				}
				break;
			case dem_datatables:
				{
					m_DemoFile.ReadNetworkDataTables( NULL );
				}
				break;
			case dem_usercmd:
				{
					ACTIVE_SPLITSCREEN_PLAYER_GUARD( nPlayerSlot );
					m_DemoFile.ReadUserCmd( NULL, dummy );
				}
				break;
			case dem_customdata:
				{
					m_DemoFile.ReadCustomData( NULL, NULL );
				}
				break;			
			case dem_stringtables:
				{
					m_DemoFile.ReadStringTables( NULL );
				}
				break;
			default:
				{
					swallowmessages = false;
				}
				break;
			}
		}
		while ( swallowmessages );

		int curpos = m_DemoFile.GetCurPos( true );

		// we read now a dem_packet
		m_DemoFile.ReadCmdInfo( nextinfo );
		m_DemoFile.ReadSequenceInfo( dummy, dummy ); 
		m_DemoFile.ReadRawData( NULL, 0 );

		DemoCommandQueue entry;
		entry.info = nextinfo;
		entry.tick = tick;
		entry.filepos = curpos;

		int i = 0;
		int c =  m_DestCmdInfo.Count();
		for ( ; i < c; ++i )
		{
			if (  m_DestCmdInfo[ i ].filepos == entry.filepos )
				break; // cmdinfo is already in list
		}

		if ( i >= c )
		{
			// add cmdinfo to list
			if ( c > 0 )
			{
				if (  m_DestCmdInfo[ c - 1 ].tick > tick )
				{
					 m_DestCmdInfo.RemoveAll();
				}
			}

			m_DestCmdInfo.AddToTail( entry );
		}

		if ( ( tick - curtick ) > intervalticks )
			break;
	}

	m_DemoFile.SeekTo( starting_position, true );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Read in next demo message and send to local client over network channel, if it's time.
// Output : bool 
//-----------------------------------------------------------------------------
netpacket_t *CDemoPlayer::ReadPacket( void )
{
	int			tick = 0;
	byte		cmd = dem_signon;
	long		curpos = 0;

	if ( ! m_DemoFile.IsOpen() )
	{
		m_bPlayingBack = false;
		Host_EndGame( true, "Tried to read a demo message with no demo file\n" );
		return NULL;
	}

	// If game is still shutting down, then don't read any demo messages from file quite yet
	if ( HostState_IsGameShuttingDown() )
	{
		return NULL;
	}

	Assert( IsPlayingBack() );

	if ( m_nTimeDemoCurrentFrame >= 0 )
	{
		// don't scan when doing overwatch
		if ( !m_pPlaybackParameters || !m_pPlaybackParameters->m_bAnonymousPlayerIdentity )
		{	
			GetImportantGameEventIDs();
			ScanForImportantTicks();
			if ( m_bScanMode && m_ImportantTicks.Count() > 0 )
			{
				CL_ScanDemoDone( m_szScanMode );
				return NULL;
			}
			if ( m_bDoHighlightScan )
			{
				BuildHighlightList();
			}
		}
	}

	// External editor has paused playback
	if ( CheckPausedPlayback() )
		return NULL;

	// handle highlights
	if ( m_nCurrentHighlight != -1 && !IsSkipping() )
	{
		int nCurrentTick = GetPlaybackTick();

		if ( nCurrentTick >= m_highlights[ m_nCurrentHighlight ].nPlayToTick )
		{
			m_nCurrentHighlight++;
			if ( m_nCurrentHighlight >= m_highlights.Count() )
			{
				m_nCurrentHighlight = -1;
				SetPlaybackTimeScale( 1.0f );
				g_ClientDLL->ShowHighlightSkippingMessage( false );
				m_pPlaybackParameters = NULL;
				return NULL;
			}
		}

		const DemoHighlightEntry_t &highlight = m_highlights[ m_nCurrentHighlight ];

		SetPlaybackParametersLockFirstPersonAccountID( highlight.unAccountID );
		
		// deal with skipping and fast forwarding
		if ( highlight.nSeekToTick != -1 && nCurrentTick < highlight.nSeekToTick )
		{
			g_ClientDLL->ShowHighlightSkippingMessage( true, nCurrentTick, highlight.nSeekToTick, highlight.nPlayToTick );
			m_nSkipToTick = highlight.nSeekToTick;
		}
		else
		{
			if ( nCurrentTick < highlight.nFastForwardToTick )
			{
				g_ClientDLL->ShowHighlightSkippingMessage( true, nCurrentTick, highlight.nSeekToTick, highlight.nPlayToTick );
				SetPlaybackTimeScale( demo_highlight_fastforwardspeed.GetFloat() );
			}
			else
			{
				SetPlaybackTimeScale( 1.0f );
				g_ClientDLL->ShowHighlightSkippingMessage( false, nCurrentTick, highlight.nSeekToTick, highlight.nPlayToTick );
			}
		}
	}

	bool bStopReading = false;
	
	while ( !bStopReading )
	{
		curpos = m_DemoFile.GetCurPos( true );

		int nPlayerSlot = 0;
		m_DemoFile.ReadCmdHeader( cmd, tick, nPlayerSlot );

		m_nPacketTick = tick;

		if ( m_nTickToPauseOn != -1 && tick >= m_nTickToPauseOn )
		{
			m_nTickToPauseOn = -1;
			PausePlayback( -1 );
			cl_interpolate.SetValue( m_bSavedInterpolateState ? 1 : 0 );
		}

		if ( m_nCurrentHighlight != -1 && IsSkipping() )
		{
			if ( m_highlights[ m_nCurrentHighlight ].nSeekToTick != -1 && tick >= m_highlights[ m_nCurrentHighlight ].nSeekToTick )
			{
				m_nSkipToTick = -1;
			}
		}

		// always read control commands 
		if ( !IsControlCommand( cmd ) )
		{
			int playbacktick = GetPlaybackTick();

			if ( !m_bTimeDemo )
			{
				// Time demo ignores clocks and tries to synchronize frames to what was recorded
				//  I.e., while frame is the same, read messages, otherwise, skip out.
				// If we're still signing on, then just parse messages until fully connected no matter what
				if ( GetBaseLocalClient().IsActive() &&
					(tick > playbacktick) && !IsSkipping() )
				{
					// is not time yet
					bStopReading = true;
				}
			}
			else
			{
				if ( m_nTimeDemoCurrentFrame == host_framecount )
				{
					if ( !IsSkipping() )
					{
						// If we are playing back a timedemo, and we've already passed on a 
						//  frame update for this host_frame tag, then we'll just skip this mess
						bStopReading = true;
					}
				}
			}

			if ( bStopReading )
			{
				demoaction->Update( false, playbacktick, TICKS_TO_TIME( playbacktick )  );
				m_DemoFile.SeekTo( curpos, true ); // go back to start of current demo command
				return NULL;   // Not time yet, dont return packet data.
			}
		}

		// COMMAND HANDLERS
		switch ( cmd )
		{
		case dem_synctick:
			{
				if ( demo_debug.GetBool() )
				{
					Msg( "%d dem_synctick\n", tick );
				}

				ResyncDemoClock();
				m_nRestartFilePos = m_DemoFile.GetCurPos( true );

				// Once demo clock got resync-ed we can go ahead and
				// perform skipping logic normally
				if ( ( m_nSkipToTick != -1 ) &&
					 ( ( m_nSkipToTick & SKIP_TO_TICK_FLAG ) == SKIP_TO_TICK_FLAG ) )
				{
					m_nSkipToTick &= ~SKIP_TO_TICK_FLAG;
				}
			}
			break;
		case dem_stop:
			{
				if ( demo_debug.GetBool() )
				{
					Msg( "%d dem_stop\n", tick );
				}

				if ( g_pMatchFramework )
				{
					g_pMatchFramework->GetEventsSubscription()->BroadcastEvent( new KeyValues( "OnDemoFileEndReached" ) );
				}

				FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
				{
					ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );
					GetBaseLocalClient().Disconnect(true);
				}
				return NULL;
			}
			break;
		case dem_consolecmd:
			{
				ACTIVE_SPLITSCREEN_PLAYER_GUARD( nPlayerSlot );
				const char * command = m_DemoFile.ReadConsoleCommand();

				if ( demo_debug.GetBool() )
				{
					Msg( "%d dem_consolecmd [%s]\n", tick, command );
				}

				Cbuf_AddText( Cbuf_GetCurrentPlayer(), command, kCommandSrcDemoFile );
				Cbuf_Execute();
			}
			break;
		case dem_datatables:
			{
				if ( demo_debug.GetBool() )
				{
					Msg( "%d dem_datatables\n", tick );
				}

				void *data = malloc( DEMO_RECORD_BUFFER_SIZE ); // X360TBD: How much memory is really needed here?
				bf_read buf( "dem_datatables", data, DEMO_RECORD_BUFFER_SIZE );
				m_DemoFile.ReadNetworkDataTables( &buf );
				buf.Seek( 0 );								// re-read data

				// support for older engine demos
				if ( !DataTable_LoadDataTablesFromBuffer( &buf, m_DemoFile.m_DemoHeader.demoprotocol ) )
				{
					Host_Error( "Error parsing network data tables during demo playback." );
				}
				free( data );
			}
			break;
		case dem_stringtables:
			{
				void *data = malloc( DEMO_RECORD_BUFFER_SIZE ); // X360TBD: How much memory is really needed here?
				bf_read buf( "dem_stringtables", data, DEMO_RECORD_BUFFER_SIZE );
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

				ACTIVE_SPLITSCREEN_PLAYER_GUARD( nPlayerSlot );
				if ( demo_debug.GetBool() )
				{
					Msg( "%d dem_usercmd\n", tick );
				}

				char buffer[256];
				int  length = sizeof(buffer);
				int outgoing_sequence = m_DemoFile.ReadUserCmd( buffer, length );

				// put it into a bitbuffer 
				bf_read msg( "CDemo::ReadUserCmd", buffer, length );

				g_ClientDLL->DecodeUserCmdFromBuffer( nPlayerSlot, msg, outgoing_sequence );

				// Note, we need to have the current outgoing sequence correct so we can do prediction
				//  correctly during playback
				GetBaseLocalClient().lastoutgoingcommand = outgoing_sequence;		
			}
			break;
		case dem_customdata:
			{
				int iCallbackIndex;
				uint8 *pData;
				int iSize = m_DemoFile.ReadCustomData( &iCallbackIndex, &pData );

				if( iCallbackIndex == -1 )
				{
					//special handler, this is the data that fills in the rest of the data
					uint8 *pParse = pData;
					int iTableEntries = *(int *)pParse;
					pParse += sizeof( int );
					iTableEntries = LittleDWord( iTableEntries );
					m_CustomDataCallbackMap.SetSize( iTableEntries );

					for( int i = 0; i != iTableEntries; ++i )
					{
						m_CustomDataCallbackMap[i].name = (char *)pParse;
						pParse += V_strlen( (char *)pParse ) + 1;

						//now that we have the name, map that to a registered callback function
						int j;
						for( j = 0; j != g_RegisteredDemoCustomDataCallbacks.Count(); ++j )
						{
							if( V_stricmp( m_CustomDataCallbackMap[i].name.Get(), STRING( g_RegisteredDemoCustomDataCallbacks[j].szSaveID ) ) == 0 )
							{
								//match
								m_CustomDataCallbackMap[i].pCallback = g_RegisteredDemoCustomDataCallbacks[j].pCallback;
								break;
							}
						}
						Assert( j != g_RegisteredDemoCustomDataCallbacks.Count() ); //found a match
					}
					Assert( pParse == (pData + iSize) ); //properly parsed the table
				}
				else
				{
					//send it off
					Assert( iCallbackIndex < m_CustomDataCallbackMap.Count() );
					Assert( m_CustomDataCallbackMap[iCallbackIndex].pCallback != NULL );

					if( m_CustomDataCallbackMap[iCallbackIndex].pCallback != NULL )
						m_CustomDataCallbackMap[iCallbackIndex].pCallback( pData, iSize );
					else
						Warning( "Unable to decode custom demo data, callback \"%s\" not found.\n", m_CustomDataCallbackMap[iCallbackIndex].name.Get() );
				}
			}
			break;
		default:
			{
				bStopReading = true;

				if ( IsSkipping() )
				{
					// adjust playback host_tickcount when skipping
					m_nStartTick = host_tickcount - tick;
				}
			}
			break;
		}
	}

	if ( cmd == dem_packet )
	{
		// remember last frame we read a dem_packet update
		m_nTimeDemoCurrentFrame = host_framecount;
	}
	
	int inseq, outseqack, outseq = 0;

	m_DemoFile.ReadCmdInfo( m_LastCmdInfo );

	m_DemoFile.ReadSequenceInfo( inseq, outseqack );
	GetBaseLocalClient().m_NetChannel->SetSequenceData( outseq, inseq, outseqack );

	int length = m_DemoFile.ReadRawData( (char*)m_DemoPacket.data,  NET_MAX_PAYLOAD );

	if ( demo_debug.GetBool() )
	{
		Msg( "%d network packet [%d]\n", tick, length );
	}

	if ( length > 0 )
	{
		// succsessfully read new demopacket
		m_DemoPacket.received = realtime;
		m_DemoPacket.size = length;
		m_DemoPacket.message.StartReading( m_DemoPacket.data,  m_DemoPacket.size );
	
		if ( demo_debug.GetInt() >= 1 )
		{
			Msg( "Demo message, tick %i, %i bytes\n", GetPlaybackTick(), length );
		}
	}

	// Try and jump ahead one frame
	m_bInterpolateView = ParseAheadForInterval( tick, 8 );

	// ConMsg( "Reading message for %i : %f skip %i\n", m_nFrameCount, fElapsedTime, forceskip ? 1 : 0 );

	// Skip a few ticks before doing any timing
	if ( (m_nTimeDemoStartFrame < 0) && GetPlaybackTick() > 100 )
	{
		m_nTimeDemoStartFrame = host_framecount;
		m_flTimeDemoStartTime = Sys_FloatTime();
		m_flTotalFPSVariability = 0.0f;

		if ( m_bTimeDemo )
		{
			g_EngineStats.BeginRun();

			g_PerfStats.Reset();
		}
	}

	if ( m_nSnapshotTick > 0 && m_nSnapshotTick <= GetPlaybackTick() )
	{
		const char *filename = "benchframe";

		if ( m_SnapshotFilename[0] )
			filename = m_SnapshotFilename;

		CL_TakeScreenshot( filename ); // take a screenshot
		m_nSnapshotTick = 0;

		if ( s_bBenchframe )
		{
			Cbuf_AddText( Cbuf_GetCurrentPlayer(), "stopdemo\n" );
		}
	}

	return &m_DemoPacket;
}

void CDemoPlayer::InterpolateDemoCommand( int nSlot, int targettick, DemoCommandQueue& prev, DemoCommandQueue& next )
{
	CUtlVector< DemoCommandQueue >& list = m_DestCmdInfo;
	int c = list.Count();

	prev.info.Reset();
	next.info.Reset();

	if ( c < 2 )
	{
		// we need at least two entries to interpolate
		return; 
	}

	int i = 0;
	int savedI = -1;

	DemoCommandQueue *entry1 = &list[ i ];
	DemoCommandQueue *entry2 = &list[ i+1 ];

	while ( true )
	{
		if ( (entry1->tick <= targettick) && (entry2->tick > targettick) )
		{
			// Means we hit a FDEMO_NOINTERP along the way to now
			if ( savedI != -1 )
			{
				prev = list[ savedI ];
				next = list[ savedI + 1 ];
			}
			else
			{
				prev = *entry1;
				next = *entry2;
			}
			return;
		}

		// If any command between the previous target and now has the FDEMO_NOINTERP, we need to stop at the command just before that (entry), so we save off the I
		// We can't just return since we need to see if we actually get to a spanning pair (though we always should).  Also, we only latch this final interp spot on
		///  the first FDEMO_NOINTERP we see
		if ( savedI == -1 &&
			 entry2->tick > m_nPreviousTick &&
			 entry2->tick <= targettick &&
			 entry2->info.u[ nSlot ].flags & FDEMO_NOINTERP )
		{
			savedI = i;
		}

		if ( i+2 == c )
			break;

		i++;
		entry1 = &list[ i ];
		entry2 = &list[ i+1 ];
	}	
}

static ConVar demo_legacy_rollback( "demo_legacy_rollback", "1", 0, "Use legacy view interpolation rollback amount in demo playback." );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDemoPlayer::InterpolateViewpoint( void )
{
	if ( !IsPlayingBack() )
		return;

	democmdinfo_t outinfo;
	outinfo.Reset();

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( hh );

		bool bHasValidData =
			 m_LastCmdInfo.u[ hh ].viewOrigin != vec3_origin ||
			 m_LastCmdInfo.u[ hh ].viewAngles != vec3_angle ||
			 m_LastCmdInfo.u[ hh ].localViewAngles != vec3_angle ||
			 m_LastCmdInfo.u[ hh ].flags != 0;

		int nTargetTick = GetPlaybackTick();

		// Player view needs to be one tick interval in the past like the client DLL entities
		if ( GetBaseLocalClient().m_nMaxClients == 1 )
		{
			if ( demo_legacy_rollback.GetBool() )
			{
				nTargetTick -= TIME_TO_TICKS( GetBaseLocalClient().GetClientInterpAmount() ) + 1;
			}
			else
			{
				nTargetTick -= 1;
			}
		}

		float vel = 0.0f;
		float angVel = 0.0f; 
		if ( m_bInterpolateView && demo_interpolateview.GetBool() && bHasValidData )
		{
			DemoCommandQueue prev, next;
			float frac = 0.0f;

			prev.info = m_LastCmdInfo;
			prev.tick = -1;
			next.info = m_LastCmdInfo;
			next.tick = -1;

			// Determine current time slice
			
			InterpolateDemoCommand( hh, nTargetTick, prev, next );

			float dt = TICKS_TO_TIME(next.tick-prev.tick);

			frac = (TICKS_TO_TIME(nTargetTick-prev.tick)+GetBaseLocalClient().m_tickRemainder)/dt;

			frac = clamp( frac, 0.0f, 1.0f );

			// Now interpolate
			Vector delta;

			Vector startorigin = prev.info.u[ hh ].GetViewOrigin();
			Vector destorigin = next.info.u[ hh ].GetViewOrigin();

			// check for teleporting - since there can be multiple cmd packets between a game frame,
			// we need to check from the last actually ran command to see if there was a teleport
			VectorSubtract( destorigin, m_LastCmdInfo.u[ hh ].GetViewOrigin(), delta );
			float distmoved = delta.Length();
			
			if ( dt > 0.0f )
			{
				vel = distmoved / dt;
			}

			if ( dt > 0.0f )
			{
				QAngle startang = prev.info.u[ hh ].GetLocalViewAngles();
				QAngle destang = next.info.u[ hh ].GetLocalViewAngles();
		
				for ( int i = 0; i < 3; ++i )
				{
					float dAng = AngleNormalizePositive( destang[ i ] ) - AngleNormalizePositive( startang[ i ] );
					dAng = AngleNormalize( dAng );
					float aVel = fabs( dAng ) / dt;
					if ( aVel > angVel )
					{
						angVel = aVel;
					}
				}
			}

			// FIXME: This should be velocity based maybe?
			if ( (vel > demo_interplimit.GetFloat()) || 
				 (angVel > demo_avellimit.GetFloat() ) ||
				m_bResetInterpolation )
			{
				m_bResetInterpolation = false;

				// it's a teleport, just let it happen naturally next frame
				// setting frac to 1.0 (like it was previously) would just mean that we
				// are teleporting a frame ahead of when we should
				outinfo.u[ hh ].viewOrigin = m_LastCmdInfo.u[ hh ].GetViewOrigin();
				outinfo.u[ hh ].viewAngles = m_LastCmdInfo.u[ hh ].GetViewAngles();
				outinfo.u[ hh ].localViewAngles = m_LastCmdInfo.u[ hh ].GetLocalViewAngles();
			}
			else
			{
				outinfo.u[ hh ].viewOrigin = startorigin + frac * ( destorigin - startorigin );

				Quaternion src, dest;
				Quaternion result;

				AngleQuaternion( prev.info.u[ hh ].GetViewAngles(), src );
				AngleQuaternion( next.info.u[ hh ].GetViewAngles(), dest );
				QuaternionSlerp( src, dest, frac, result );

				QuaternionAngles( result, outinfo.u[ hh ].viewAngles );

				AngleQuaternion( prev.info.u[ hh ].GetLocalViewAngles(), src );
				AngleQuaternion( next.info.u[ hh ].GetLocalViewAngles(), dest );
				QuaternionSlerp( src, dest, frac, result );

				QuaternionAngles( result, outinfo.u[ hh ].localViewAngles );
			}
		}
		else if ( bHasValidData )
		{
			// don't interpolate, just copy values
			outinfo.u[ hh ].viewOrigin = m_LastCmdInfo.u[ hh ].GetViewOrigin();
			outinfo.u[ hh ].viewAngles = m_LastCmdInfo.u[ hh ].GetViewAngles();
			outinfo.u[ hh ].localViewAngles = m_LastCmdInfo.u[ hh ].GetLocalViewAngles();
		}

		m_nPreviousTick = nTargetTick;

		// let any demo system override view ( drive, editor, smoother etc)
		bHasValidData |= OverrideView( outinfo );

		if ( !bHasValidData )
			continue; // no validate data & no override, exit

		g_pClientSidePrediction->SetViewOrigin( outinfo.u[ hh ].viewOrigin );
		g_pClientSidePrediction->SetViewAngles( outinfo.u[ hh ].viewAngles );
		g_pClientSidePrediction->SetLocalViewAngles( outinfo.u[ hh ].localViewAngles );
#ifndef DEDICATED
		VectorCopy( outinfo.u[ hh ].viewAngles, GetLocalClient().viewangles );
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDemoPlayer::IsPlayingTimeDemo( void )const
{
	return m_bTimeDemo && m_bPlayingBack;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CDemoPlayer::IsPlayingBack( void )const 
{
	return m_bPlayingBack;
}

CDemoPlayer::CDemoPlayer()
{
	m_flAutoResumeTime = 0.0f;
	m_flPlaybackRateModifier = 1.0f;
	m_bTimeDemo = false;	
	m_nTimeDemoStartFrame = -1;	
	m_flTimeDemoStartTime = 0.0f;	
	m_flTotalFPSVariability = 0.0f;
	m_nTimeDemoCurrentFrame = -1; 
	m_nPacketTick = 0; // pulling together with broadcast
	m_bPlayingBack = false;
	m_bPlaybackPaused = false;
	m_nSkipToTick = -1;
	m_nSnapshotTick = 0;
	m_SnapshotFilename[0] = 0;
	m_bResetInterpolation = false;
	m_nPreviousTick = 0;
	m_pPlaybackParameters = NULL;
	m_bPacketReadSuspended = false;
	m_nRestartFilePos = -1;
	m_pImportantEventData = NULL;
	m_nTickToPauseOn = -1;
	m_bSavedInterpolateState = true;
	m_highlightSteamID.SetFromUint64( 0 );
	m_nHighlightPlayerIndex = -1;
	m_bDoHighlightScan = false;
	m_nCurrentHighlight = -1;
	m_bLowlightsMode = false;
	m_bScanMode = false;
	m_szScanMode[0] = 0;
}

CDemoPlayer::~CDemoPlayer()
{
	StopPlayback();
	if ( g_ClientDLL )
	{
		g_ClientDLL->OnDemoPlaybackStop();
	}
}

CDemoPlaybackParameters_t const * CDemoPlayer::GetDemoPlaybackParameters()
{
	return m_bPlayingBack ? m_pPlaybackParameters : NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Start's demo playback
// Input  : *name - 
//-----------------------------------------------------------------------------
bool CDemoPlayer::StartPlayback( const char *filename, bool bAsTimeDemo, CDemoPlaybackParameters_t const *pPlaybackParameters, int nStartingTick )
{
	m_pPlaybackParameters = pPlaybackParameters;
	m_bPacketReadSuspended = false;
	SCR_BeginLoadingPlaque();

	// Disconnect from server or stop running one
	int oldn = GetBaseLocalClient().demonum;
	GetBaseLocalClient().demonum = -1;
	Host_Disconnect( false );
	GetBaseLocalClient().demonum = oldn;

	m_bPlayingBack = true;
	if ( !m_DemoFile.Open( filename, true ) )
	{
		m_bPlayingBack = false;
		GetBaseLocalClient().demonum = -1;		// stop demo loop
		return false;
	}

	// Read in the m_DemoHeader
	demoheader_t *dh = m_DemoFile.ReadDemoHeader( pPlaybackParameters );
	if ( !dh )
	{
		m_bPlayingBack = false;
		ConMsg( "Failed to read demo header.\n" );
		m_DemoFile.Close();
		GetBaseLocalClient().demonum = -1;
		return false;
	}

	if ( dh->playback_frames == 0 && dh->playback_ticks == 0 && dh->playback_time == 0 )
	{
		ConMsg( "Demo %s is incomplete\n", filename );
	}
	else
	{
		ConMsg( "Playing demo from %s.\n", filename );
	}

	FOR_EACH_VEC( m_ImportantTicks, i )
	{
		if ( m_ImportantTicks[ i ].pKeys )
		{
			m_ImportantTicks[ i ].pKeys->deleteThis();
		}
	}
	m_ImportantTicks.RemoveAll();
	m_ImportantGameEvents.RemoveAll();
	m_highlights.RemoveAll();
	m_nCurrentHighlight = -1;

	// Now read in the directory structure.
	m_nSkipToTick = nStartingTick; // reset skip-to-tick, otherwise it remains stale from old skipping
	if ( nStartingTick != -1 )
	{
		m_nSkipToTick |= SKIP_TO_TICK_FLAG;
	}
	GetBaseLocalClient().m_nSignonState= SIGNONSTATE_CONNECTED;

	ResyncDemoClock(); 

	// create a fake channel with a NULL address (no encryption keys in demos)
	GetBaseLocalClient().m_NetChannel = NET_CreateNetChannel( NS_CLIENT, NULL, "DEMO", &GetBaseLocalClient(), NULL, false );

	if ( !GetBaseLocalClient().m_NetChannel )
	{
		ConMsg ("CDemo::Play: failed to create demo net channel\n" );
		m_DemoFile.Close();
		GetBaseLocalClient().demonum = -1;		// stop demo loop
		Host_Disconnect(true);
		return false;
	}
	
	GetBaseLocalClient().m_NetChannel->SetTimeout( -1.0f );	// never timeout
	
	V_memset( &m_DemoPacket, 0, sizeof(m_DemoPacket) );

	// setup demo packet data buffer
	m_DemoPacket.data = new unsigned char[ NET_MAX_PAYLOAD ];
	m_DemoPacket.from.SetAddrType( NSAT_NETADR );
	m_DemoPacket.from.m_adr.SetType( NA_LOOPBACK );
		
	GetBaseLocalClient().chokedcommands = 0;
	GetBaseLocalClient().lastoutgoingcommand = -1;
	GetBaseLocalClient().m_flNextCmdTime = net_time;

	m_bTimeDemo = bAsTimeDemo;
	m_nTimeDemoCurrentFrame = -1;
	m_nTimeDemoStartFrame = -1;
	m_nPacketTick = 0;

	if ( m_bTimeDemo )
	{
		SeedRandomNumberGenerator( true );
	}

	demoaction->StartPlaying( filename );

	// m_bFastForwarding = false;
	m_flAutoResumeTime = 0.0f;
	m_flPlaybackRateModifier = 1.0f;
	demoplayer = this;

	scr_demo_override_fov = 0.0f;

	return true;
}


 


bool CDemoPlayer::ScanDemo( const char *filename, const char* pszMode )
{
	m_bScanMode = true;
	V_strcpy_safe( m_szScanMode, pszMode );

	return StartPlayback( filename, false, NULL );
}

void CDemoPlayer::RestartPlayback( void )
{
	if ( m_nRestartFilePos != -1 )
	{
		m_DemoFile.SeekTo( m_nRestartFilePos, true );
		ResyncDemoClock();

		GetBaseLocalClient().DeleteClientFrames( -1 );
		GetBaseLocalClient().SetFrameTime( 0 );
		GetBaseLocalClient().chokedcommands = 0;
		GetBaseLocalClient().lastoutgoingcommand = -1;
		GetBaseLocalClient().m_flNextCmdTime = net_time;
		GetBaseLocalClient().events.RemoveAll();

#ifndef DEDICATED
		S_StopAllSounds( true );
		g_ClientDLL->OnDemoPlaybackRestart();
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : flCurTime - 
//-----------------------------------------------------------------------------
void CDemoPlayer::MarkFrame( float flFPSVariability )
{
	m_flTotalFPSVariability += flFPSVariability;
}

void ComputeTimedemoResultsFilename( CFmtStr &fileName, CFmtStr &dateString )
{
	// Write the results to a CSV file.
	// If specified on the command-line, write to a specific benchmark path.
	// Include the GPU name and host computer name for convenient sorting in Explorer.


	// Compute a date+time string
	struct tm time;
	Plat_GetLocalTime( &time );
	dateString.sprintf( "%04d_%02d_%02d__%02d_%02d_%02d", time.tm_year+1900, time.tm_mon+1, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec );

	// Compute prettified GPU name
	MaterialAdapterInfo_t info;
	materials->GetDisplayAdapterInfo( materials->GetCurrentAdapter(), info );
	CUtlString gpuName( info.m_pDriverName );
	const char *pVendors[] = { "nvidia", "ati" };
	for ( int i = 0; i < ARRAYSIZE( pVendors ); i++ )
	{
		const char *pVendor = V_stristr( gpuName.Get(), pVendors[i] );
		if ( pVendor )
		{
			int nVendorLen = V_strlen( pVendors[i] );
			if ( pVendor[nVendorLen] == ' ' ) nVendorLen++;
			CUtlString tail = pVendor + nVendorLen;
			gpuName.SetLength( pVendor - gpuName.Get() );
			gpuName += tail;
			break;
		}
	}
	// Now replace any non-alphanumerics with an underscore
	char *pGPUName = gpuName.Get();
	for ( int i = 0; i < gpuName.Length(); i++ )
	{
		if ( !V_isalnum( pGPUName[i] ) )
			pGPUName[i] = '_';
	}

	// Compute the local computer's name
	char host[256] = "";
	DWORD length = sizeof( host ) - 1;
#if !IsGameConsole()
	if ( gethostname( host, length ) < 0 )
#endif
	{
		// Use dateString as a fallback, if we can't get the host name
		V_strncpy( host, dateString.Access(), length );
	}
	host[ length ] = '\0';

	// Get the destination path (default to the gamedir)
	CUtlString benchmarkPath = CommandLine()->ParmValue( "-benchmark_path" );
	if ( benchmarkPath.Length() && !IsOSX() )  // Don't bother on Mac, we can't write to an smb share trivially
	{
		benchmarkPath.StripTrailingSlash();
		V_FixSlashes( benchmarkPath.Get() );
	}
	else
	{
		benchmarkPath = ".";
	}

	// Slap it all together
	fileName.sprintf( "%s%sSourceBench_%s_%s.csv", benchmarkPath.Get(), CORRECT_PATH_SEPARATOR_S, gpuName.Get(), host );
}

void CDemoPlayer::WriteTimeDemoResults( void )
{
	int frames = MAX( 1, ( (host_framecount - m_nTimeDemoStartFrame) - 1 ) );
	float time = MAX( 1, ( Sys_FloatTime() - m_flTimeDemoStartTime ) );
	float flVariability = m_flTotalFPSVariability / (float)frames;
	ConMsg( "%i frames %5.3f seconds %5.2f fps (%5.2f ms/f) %5.3f fps variability\n", frames, time, frames/time, 1000*time/frames, flVariability );


	// Open the file (write the CSV to a benchmark path, if specified on the command-line, and name after the GPU and host computer)
	CFmtStr fileName, dateString;
	ComputeTimedemoResultsFilename( fileName, dateString );
	bool bEmptyFile = !g_pFileSystem->FileExists( fileName.Access() );
	FileHandle_t fileHandle = g_pFileSystem->Open( fileName.Access(), "a+" );
	if ( fileHandle == FILESYSTEM_INVALID_HANDLE )
	{
		Warning( "DEMO: Failed to open %s!\n", fileName.Access() );
		return;
	}
	bEmptyFile = !g_pFileSystem->Size( fileHandle );

	// Write the header line when starting a new file
	if( bEmptyFile )
	{
		g_pFileSystem->FPrintf( fileHandle, "Benchmark Results\n\n" );
		g_pFileSystem->FPrintf( fileHandle, "demofile," );
		g_pFileSystem->FPrintf( fileHandle, "frame data csv," );
		g_pFileSystem->FPrintf( fileHandle, "fps," );
		g_pFileSystem->FPrintf( fileHandle, "fps variability," );
		g_pFileSystem->FPrintf( fileHandle, "total sec," );
		g_pFileSystem->FPrintf( fileHandle, "avg ms," );
		for ( int lp = 0; lp < PERF_STATS_SLOT_MAX; ++lp )
		{
			g_pFileSystem->FPrintf( fileHandle, "%s (avg ms),", g_PerfStats.m_Slots[lp].m_pszName );
		}
		g_pFileSystem->FPrintf( fileHandle, "width," );
		g_pFileSystem->FPrintf( fileHandle, "height," );
		g_pFileSystem->FPrintf( fileHandle, "msaa," );
		g_pFileSystem->FPrintf( fileHandle, "aniso," );
		g_pFileSystem->FPrintf( fileHandle, "picmip," );
		g_pFileSystem->FPrintf( fileHandle, "numframes," );
		g_pFileSystem->FPrintf( fileHandle, "dxlevel," );
		g_pFileSystem->FPrintf( fileHandle, "backbuffer," );
		g_pFileSystem->FPrintf( fileHandle, "cmdline," );
		g_pFileSystem->FPrintf( fileHandle, "driver," );
		g_pFileSystem->FPrintf( fileHandle, "vendor id," );
		g_pFileSystem->FPrintf( fileHandle, "device id," );
		//g_pFileSystem->FPrintf( fileHandle, "subsys id," );
		//g_pFileSystem->FPrintf( fileHandle, "revision," );
		//g_pFileSystem->FPrintf( fileHandle, "shaderdll," );
		g_pFileSystem->FPrintf( fileHandle, "sound," );
		g_pFileSystem->FPrintf( fileHandle, "vsync," );
		g_pFileSystem->FPrintf( fileHandle, "gpu_level," );
		g_pFileSystem->FPrintf( fileHandle, "cpu_level," );
		g_pFileSystem->FPrintf( fileHandle, "date," );
		g_pFileSystem->FPrintf( fileHandle, "csm enabled," );
		g_pFileSystem->FPrintf( fileHandle, "csm quality," );
		g_pFileSystem->FPrintf( fileHandle, "fxaa," );
		g_pFileSystem->FPrintf( fileHandle, "motionblur," );
		g_pFileSystem->FPrintf( fileHandle, "\n" );
	}


	// Append a new line of data
	static ConVarRef gpu_level( "gpu_level" );
	static ConVarRef cpu_level( "cpu_level" );
	static ConVarRef mat_vsync( "mat_vsync" );
	static ConVarRef mat_antialias( "mat_antialias" );
	static ConVarRef mat_forceaniso( "mat_forceaniso" );
	static ConVarRef mat_picmip( "mat_picmip" );
	static ConVarRef cl_csm_enabled( "cl_csm_enabled" );
	static ConVarRef csm_quality_level( "csm_quality_level" );
	static ConVarRef mat_software_aa_strength( "mat_software_aa_strength" );
	static ConVarRef mat_motion_blur_enabled( "mat_motion_blur_enabled" );
	
	int width, height;
	MaterialAdapterInfo_t info;
	CMatRenderContextPtr pRenderContext( materials );
	pRenderContext->GetWindowSize( width, height );
	ImageFormat backBufferFormat = materials->GetBackBufferFormat();
	materials->GetDisplayAdapterInfo( materials->GetCurrentAdapter(), info );

	g_pFileSystem->Seek( fileHandle, 0, FILESYSTEM_SEEK_TAIL );
	g_pFileSystem->FPrintf( fileHandle, "%s,", m_DemoFile.m_szFileName );
	g_pFileSystem->FPrintf( fileHandle, "%s,", g_pStatsFile );
	g_pFileSystem->FPrintf( fileHandle, "%5.1f,", frames/time );
	g_pFileSystem->FPrintf( fileHandle, "%5.1f,", flVariability );
	g_pFileSystem->FPrintf( fileHandle, "%5.1f,", time );
	g_pFileSystem->FPrintf( fileHandle, "%5.3f,", 1000*time/frames );
	for ( int lp = 0; lp < PERF_STATS_SLOT_MAX; ++lp )
	{
		g_pFileSystem->FPrintf( fileHandle, "%6.3f,", g_PerfStats.m_Slots[lp].m_AccTotalTime.GetMillisecondsF() / g_PerfStats.m_nFrames );
	}
	g_pFileSystem->FPrintf( fileHandle, "%i,", width );
	g_pFileSystem->FPrintf( fileHandle, "%i,", height );
	g_pFileSystem->FPrintf( fileHandle, "%i,", MAX( 1, mat_antialias.GetInt() ) );
	g_pFileSystem->FPrintf( fileHandle, "%i,", mat_forceaniso.GetInt() );
	g_pFileSystem->FPrintf( fileHandle, "%i,", mat_picmip.GetInt() );
	g_pFileSystem->FPrintf( fileHandle, "%i,", frames );
	g_pFileSystem->FPrintf( fileHandle, "%s,", COM_DXLevelToString( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() ) );
	g_pFileSystem->FPrintf( fileHandle, "%s,", ImageLoader::GetName( backBufferFormat ) );
	g_pFileSystem->FPrintf( fileHandle, "%s,", CommandLine()->GetCmdLine() );
	g_pFileSystem->FPrintf( fileHandle, "%s,", info.m_pDriverName );
	g_pFileSystem->FPrintf( fileHandle, "0x%x,", info.m_VendorID );
	g_pFileSystem->FPrintf( fileHandle, "0x%x,", info.m_DeviceID );
	//g_pFileSystem->FPrintf( fileHandle, "0x%x,", info.m_SubSysID );
	//g_pFileSystem->FPrintf( fileHandle, "0x%x,", info.m_Revision );
	//g_pFileSystem->FPrintf( fileHandle, "%s,", g_pMaterialSystemHardwareConfig->GetShaderDLLName() );
	g_pFileSystem->FPrintf( fileHandle, "%s,", CommandLine()->CheckParm( "-nosound" ) ? "disabled" : "enabled" );
	g_pFileSystem->FPrintf( fileHandle, "%s,", CommandLine()->CheckParm( "-mat_vsync" ) || mat_vsync.GetBool() ? "enabled" : "disabled" );
	g_pFileSystem->FPrintf( fileHandle, "%d,", gpu_level.GetInt() );
	g_pFileSystem->FPrintf( fileHandle, "%d,", cpu_level.GetInt() );
	g_pFileSystem->FPrintf( fileHandle, "%s,", dateString.Access() );
	g_pFileSystem->FPrintf( fileHandle, "%i,", cl_csm_enabled.GetInt() );
	g_pFileSystem->FPrintf( fileHandle, "%i,", csm_quality_level.GetInt() );
	g_pFileSystem->FPrintf( fileHandle, "%i,", mat_software_aa_strength.GetInt() );
	g_pFileSystem->FPrintf( fileHandle, "%i,", mat_motion_blur_enabled.GetInt() );
	g_pFileSystem->FPrintf( fileHandle, "\n" );
	g_pFileSystem->Close( fileHandle );
}


void CDemoPlayer::PausePlayback( float seconds  )
{
	m_bPlaybackPaused = true;
	
	if ( seconds > 0.0f )
	{
		// Use true clock since everything else is frozen
		m_flAutoResumeTime = Sys_FloatTime() + seconds;
	}
	else
	{
		m_flAutoResumeTime = 0.0f;
	}
}

void CDemoPlayer::ResumePlayback()
{
	m_bPlaybackPaused = false;
	m_flAutoResumeTime = 0.0f;
}

bool CDemoPlayer::CheckPausedPlayback()
{
	if ( m_bPacketReadSuspended )
		return true; // When packet reading is suspended it trumps all other states

	if ( demo_pauseatservertick.GetInt() > 0 )
	{
		if ( GetBaseLocalClient().GetServerTickCount() >= demo_pauseatservertick.GetInt() )
		{
			PausePlayback( -1 );
			m_nSkipToTick = -1;
			demo_pauseatservertick.SetValue( 0 );
			Msg( "Demo paused at server tick %i\n", GetBaseLocalClient().GetServerTickCount() );
		}
	}
	
	if ( IsSkipping() )
	{
		if ( ( m_nSkipToTick > GetPlaybackTick() ) ||
			 ( ( m_nSkipToTick & SKIP_TO_TICK_FLAG ) == SKIP_TO_TICK_FLAG ) )
		{
			// we are skipping
			return false;
		}
		else
		{
			// we can't skip back (or finished skipping), so disable skipping
			m_nSkipToTick = -1;
		}
	}

	if ( !IsPlaybackPaused() )
		return false;

	if ( m_bPlaybackPaused )
	{
		if ( (m_flAutoResumeTime > 0.0f) &&
			 (Sys_FloatTime() >= m_flAutoResumeTime) )
		{
			// it's time to unpause replay
			ResumePlayback();
		}
	}

	return m_bPlaybackPaused;
}

bool CDemoPlayer::IsPlaybackPaused()const
{
	if ( !IsPlayingBack() )
		return false;

	// never pause while reading signon data
	if ( m_nTimeDemoCurrentFrame < 0 )
		return false;

	// If skipping then do not pretend paused
	if ( IsSkipping() )
		return false;
	
	return m_bPlaybackPaused;
}

int CDemoPlayer::GetPlaybackStartTick( void )
{
	return m_nStartTick;
}

int CDemoPlayer::GetPlaybackTick( void )
{
	return host_tickcount - m_nStartTick;
}

int CDemoPlayer::GetPlaybackDeltaTick( void )
{
	return host_tickcount - m_nStartTick;
}

int CDemoPlayer::GetPacketTick()
{
	return m_nPacketTick;
}

void CDemoPlayer::ResyncDemoClock()
{
	m_nStartTick = host_tickcount;
	m_nPreviousTick = m_nStartTick;
}

float CDemoPlayer::GetPlaybackTimeScale()
{
	return m_flPlaybackRateModifier;
}

void CDemoPlayer::SetPlaybackTimeScale(float timescale)
{
	m_flPlaybackRateModifier = timescale;
}

void CDemoPlayer::SetBenchframe( int tick, const char *filename )
{
	m_nSnapshotTick = tick;

	if ( filename )
	{
		V_strcpy_safe( m_SnapshotFilename, filename );
	}
}

void CDemoPlayer::SetImportantEventData( const KeyValues *pData )
{
	m_pImportantEventData = pData->MakeCopy();
}

void CDemoPlayer::GetImportantGameEventIDs()
{
	if ( m_ImportantGameEvents.Count() == 0 )
	{
		KeyValues *pCurrentEvent = m_pImportantEventData->GetFirstSubKey();
		while( pCurrentEvent != NULL )
		{
			const char *pEventName = pCurrentEvent->GetName();
			CGameEventDescriptor *descriptor = g_GameEventManager.GetEventDescriptor( pEventName );
			
			if ( descriptor )
			{
				DemoImportantGameEvent_t event;
				event.nEventID = descriptor->eventid;
				event.pszEventName = pEventName;
				event.pszUIName = pCurrentEvent->GetString( "uiname" );
				event.flSeekTimeBefore = pCurrentEvent->GetFloat( "seek_time_before", 0.0f );
				event.flSeekForwardOffset = pCurrentEvent->GetFloat( "seek_back_offset", 0.0f );
				event.flSeekBackwardOffset = pCurrentEvent->GetFloat( "seek_forward_offset", 0.0f );
				event.bScanOnly = pCurrentEvent->GetBool( "scanonly" );
				m_ImportantGameEvents.AddToTail( event );
			}
			else
			{
				Msg( "GetImportantGameEventIDs: Couldn't get descriptor for %s\n", pEventName );
			}

			pCurrentEvent = pCurrentEvent->GetNextKey();
		}
	}
}

void CDemoPlayer::SetHighlightXuid( uint64 xuid, bool bLowlights )
{
	m_highlightSteamID.SetFromUint64( xuid );
	if ( xuid != 0 )
	{
		m_bDoHighlightScan = true;
		m_bLowlightsMode = bLowlights;
	}
	g_ClientDLL->SetDemoPlaybackHighlightXuid( xuid, bLowlights );
}

void ParseEventKeys( CSVCMsg_GameEvent_t *msg, CGameEventDescriptor *pDescriptor, const char *pszEventName, KeyValues **ppKeys )
{
	int nKeyCount = msg->keys().size();
	if ( nKeyCount > 0 )
	{
		// build proper key values from descriptor plus message keys
		*ppKeys = new KeyValues( pszEventName );
		if ( *ppKeys )
		{
			KeyValues *pDescriptorKey =	pDescriptor->keys->GetFirstSubKey();
			for( int nKey = 0; nKey < nKeyCount; nKey++ )
			{
				const CSVCMsg_GameEvent::key_t& KeyValue = msg->keys( nKey );
				if( KeyValue.has_val_string() )
				{
					(*ppKeys)->SetString( pDescriptorKey->GetName(), KeyValue.val_string().c_str() );
				}
				else if( KeyValue.has_val_float() )
				{
					(*ppKeys)->SetFloat( pDescriptorKey->GetName(), KeyValue.val_float() );
				}
				else if( KeyValue.has_val_long() )
				{
					(*ppKeys)->SetInt( pDescriptorKey->GetName(), KeyValue.val_long() );
				}
				else if( KeyValue.has_val_short() )
				{
					(*ppKeys)->SetInt( pDescriptorKey->GetName(), KeyValue.val_short() );
				}
				else if( KeyValue.has_val_byte() )
				{
					(*ppKeys)->SetInt( pDescriptorKey->GetName(), KeyValue.val_byte() );
				}
				else if( KeyValue.has_val_bool() )
				{
					(*ppKeys)->SetBool( pDescriptorKey->GetName(), KeyValue.val_bool() );
				}
				else if( KeyValue.has_val_uint64() )
				{
					(*ppKeys)->SetUint64( pDescriptorKey->GetName(), KeyValue.val_uint64() );
				}

				pDescriptorKey = pDescriptorKey->GetNextKey();
			}
		}
	}
	else
	{
		(*ppKeys) = NULL;
	}
}

void CDemoPlayer::ScanForImportantTicks()
{
	if ( m_ImportantTicks.Count() > 0 || m_ImportantGameEvents.Count() == 0 )
		return;

	s_demoUserInfo.RemoveAll();

	// setup a string table for use while scanning
	CNetworkStringTableContainer demoScanStringTables;
	demoScanStringTables.AllowCreation( true );
	
	int numTables = networkStringTableContainerClient->GetNumTables();
	for ( int i =0; i<numTables; i++)
	{
		// iterate through server tables
		CNetworkStringTable *serverTable = 
			(CNetworkStringTable*)networkStringTableContainerClient->GetTable( i );

		if ( !serverTable )
			continue;

		// get matching client table
		CNetworkStringTable *demoTable = 
			(CNetworkStringTable*)demoScanStringTables.CreateStringTable(
				serverTable->GetTableName(),
				serverTable->GetMaxStrings(),
				serverTable->GetUserDataSize(),
				serverTable->GetUserDataSizeBits(),
				serverTable->IsUsingDictionary() ? NSF_DICTIONARY_ENABLED : NSF_NONE
				);

		if ( !demoTable )
		{
			DevMsg("CDemoPlayer::ScanForImportantTicks: failed to create table \"%s\".\n ", serverTable->GetTableName() );
			continue;
		}

		if ( V_strcasecmp( demoTable->GetTableName(), USER_INFO_TABLENAME ) == 0 )
		{
			demoTable->SetStringChangedCallback( NULL, Callback_DemoScanUserInfoChanged );
		}

		// make demo scan table an exact copy of server table
		demoTable->CopyStringTable( serverTable ); 
	}

	demoScanStringTables.AllowCreation( false );

	m_nHighlightPlayerIndex = GetPlayerIndex( m_highlightSteamID );

	democmdinfo_t	info;
	int				dummy;
	char			buf[ NET_MAX_PAYLOAD ];

	long			starting_position = m_DemoFile.GetCurPos( true );

	m_DemoFile.SeekTo( 0, true );

	// Read in the m_DemoHeader
	demoheader_t *dh = m_DemoFile.ReadDemoHeader( m_pPlaybackParameters );
	if ( !dh )
	{
		ConMsg( "Failed to read demo header while scanning for important ticks.\n" );
		m_DemoFile.SeekTo( starting_position, true );
		return;
	}
 
	int previousTick = 0, nDemoPackets = 0;
	bool demofinished = false;
	while ( !demofinished )
	{
		int			tick = 0;
		byte		cmd;

		bool swallowmessages = true;
		do
		{
			int nPlayerSlot = 0;
			m_DemoFile.ReadCmdHeader( cmd, tick, nPlayerSlot );

			// COMMAND HANDLERS
			switch ( cmd )
			{
			case dem_synctick:
				break;
			case dem_stop:
				{
					swallowmessages = false;
					demofinished = true;
				}
				break;
			case dem_consolecmd:
				{
					ACTIVE_SPLITSCREEN_PLAYER_GUARD( nPlayerSlot );
					m_DemoFile.ReadConsoleCommand();
				}
				break;
			case dem_datatables:
				{
					m_DemoFile.ReadNetworkDataTables( NULL );
				}
				break;
			case dem_stringtables:
				{
					void *data = malloc( DEMO_RECORD_BUFFER_SIZE );
					bf_read buf( "dem_stringtables", data, DEMO_RECORD_BUFFER_SIZE );
					m_DemoFile.ReadStringTables( &buf );
					buf.Seek( 0 );
								
					if ( !demoScanStringTables.ReadStringTables( buf ) )
					{
						Host_Error( "Error parsing string tables during demo scan." );
					}
					free( data );
				}
				break;
			case dem_usercmd:
				{
					ACTIVE_SPLITSCREEN_PLAYER_GUARD( nPlayerSlot );
					m_DemoFile.ReadUserCmd( NULL, dummy );
					
				}
				break;
			case dem_packet:
				nDemoPackets++;
				// fall through
			default:
				{
					swallowmessages = false;
				}
				break;
			}
		}
		while ( swallowmessages );

		if ( demofinished )
		{
			break;
		}

		m_DemoFile.ReadCmdInfo( info );
		m_DemoFile.ReadSequenceInfo( dummy, dummy ); 

		int length = m_DemoFile.ReadRawData( buf,  NET_MAX_PAYLOAD );

		if ( length > 0 )
		{
			bf_read message;
			message.StartReading( buf,  length );

			CNetChan *pNetChannel = ( CNetChan * ) GetBaseLocalClient().m_NetChannel;

			while ( true )
			{
				if ( message.IsOverflowed() )
				{
					Msg( "Packet message is overflowed while scanning for important ticks!\n" );
					break;
				}	

				// Are we at the end?
				if ( message.GetNumBitsLeft() < 8 ) // Minimum bits for message header encoded using VarInt32
				{
					break;
				}

				// see if we have a registered message object for this type
				unsigned char cmd = message.ReadVarInt32();
				INetMessageBinder *pMsgBind = pNetChannel->FindMessageBinder( cmd, 0 );
				if ( pMsgBind )
				{
					INetMessage	*netmsg = pMsgBind->CreateFromBuffer( message );
					if ( netmsg && netmsg->GetType() == svc_GameEvent ) // only care about game events
					{
						CSVCMsg_GameEvent_t *msg = static_cast< CSVCMsg_GameEvent_t * >( netmsg );

						CGameEventDescriptor *pDescriptor = g_GameEventManager.GetEventDescriptor( msg->eventid() );
						for ( int nEvent = 0; nEvent < m_ImportantGameEvents.Count(); nEvent++ )
						{
							if ( msg->eventid() == m_ImportantGameEvents[ nEvent ].nEventID )
							{
								DemoImportantTick_t importantTick;
								importantTick.nTick = tick;
								importantTick.nPreviousTick = previousTick;
								importantTick.nImportanGameEventIndex = nEvent;
								importantTick.bCanDirectSeek = false;
								importantTick.pKeys = NULL;
								bool bIgnore = false;

								ParseEventKeys( msg, pDescriptor, m_ImportantGameEvents[ nEvent ].pszEventName, &importantTick.pKeys );

								if ( !m_ImportantGameEvents[ nEvent ].bScanOnly )
								{
									if ( V_strcasecmp( m_ImportantGameEvents[ nEvent ].pszEventName, "player_death" ) == 0 )
									{
										GetExtraDeathEventInfo( importantTick.pKeys );
									}
									else if ( V_strcasecmp( m_ImportantGameEvents[ nEvent ].pszEventName, "bomb_defused" ) == 0 ||
											  V_strcasecmp( m_ImportantGameEvents[ nEvent ].pszEventName, "bomb_planted" ) == 0 )
									{
										if ( m_bLowlightsMode )
											bIgnore = true;
										else
											GetExtraEventInfo( importantTick.pKeys );
									}

									if ( !bIgnore )
										m_ImportantTicks.AddToTail( importantTick );
								}
								else
								{
									if ( m_bScanMode && V_strcasecmp( m_ImportantGameEvents[nEvent].pszEventName, m_szScanMode ) == 0 )
									{
										GetEventInfoForScan( m_szScanMode, importantTick.pKeys );
									}
									importantTick.pKeys->deleteThis(); // cleanup keys since we aren't saving this tick
								}
								break;
							}
						}
					}
					else if ( netmsg && netmsg->GetType() == svc_UpdateStringTable )
					{
						CSVCMsg_UpdateStringTable_t *msg = static_cast< CSVCMsg_UpdateStringTable_t * >( netmsg );

						CNetworkStringTable *table = (CNetworkStringTable*)
							demoScanStringTables.GetTable( msg->table_id() );

						bf_read data( &msg->string_data()[0], msg->string_data().size() );
						table->ParseUpdate( data, msg->num_changed_entries() );
					}
					delete netmsg;
				}
			}
		}
		previousTick = tick;
	}

	demoScanStringTables.RemoveAllTables();

	m_DemoFile.SeekTo( starting_position, true );

	if ( m_DemoFile.m_DemoHeader.playback_time == 0.0f && m_DemoFile.m_DemoHeader.playback_ticks == 0 && m_DemoFile.m_DemoHeader.playback_frames == 0 )
	{
		// this is a corrupt/incomplete file
		if ( !demo_strict_validation.GetInt() && previousTick > 1 && nDemoPackets > 1 )
		{
			m_DemoFile.m_DemoHeader.playback_ticks = previousTick;
			if ( m_nTickToPauseOn == -1 )
			{
				m_nTickToPauseOn = previousTick;
			}
			m_DemoFile.m_DemoHeader.playback_frames = nDemoPackets - 1;
			m_DemoFile.m_DemoHeader.playback_time = previousTick * host_state.interval_per_tick;

			// and we are allowed to "heal" it
			Msg( "Attempting to heal incomplete demo file: assuming %d ticks, %d frames, %.2f seconds @%.0fHz\n", m_DemoFile.m_DemoHeader.playback_ticks, m_DemoFile.m_DemoHeader.playback_frames, m_DemoFile.m_DemoHeader.playback_time, 1.0f / host_state.interval_per_tick );
		}
	}
}

void CDemoPlayer::BuildHighlightList()
{
	if ( m_bDoHighlightScan && m_highlightSteamID.ConvertToUint64() != 0 )
	{
		m_highlights.RemoveAll();

		DemoHighlightEntry_t entry;
		int nTicksAfterEvent = demo_highlight_timeafter.GetFloat() / host_state.interval_per_tick;
		int nTicksBeforeEvent = demo_highlight_timebefore.GetFloat() / host_state.interval_per_tick;

		const char *szKeyToCheck = m_bLowlightsMode ? "victim_xuid" : "attacker_xuid";

		int nImportantTickIndex = FindNextImportantTickByXuid( 0, m_highlightSteamID );
		while ( nImportantTickIndex != -1 )
		{
			if ( CheckKeyXuid( m_highlightSteamID, m_ImportantTicks[ nImportantTickIndex ].pKeys, szKeyToCheck ) ||
				 CheckKeyXuid( m_highlightSteamID, m_ImportantTicks[ nImportantTickIndex ].pKeys, "player_xuid" ) )
			{
				entry.nSeekToTick = 
				entry.nFastForwardToTick = Max( m_ImportantTicks[ nImportantTickIndex ].nPreviousTick - nTicksBeforeEvent, 0 ); // no need to play before the beginning of the stream
				entry.nPlayToTick = m_ImportantTicks[ nImportantTickIndex ].nTick + nTicksAfterEvent;
				entry.nActualFirstEventTick = entry.nActualLastEventTick = m_ImportantTicks[ nImportantTickIndex ].nTick;
				entry.nNumEvents = 1;

				if ( m_bLowlightsMode )
				{
					CSteamID attackerSteamID( m_ImportantTicks[ nImportantTickIndex ].pKeys->GetUint64( "attacker_xuid" ) );
					entry.unAccountID = attackerSteamID.GetAccountID();
				}
				else
				{
					entry.unAccountID = m_highlightSteamID.GetAccountID();
				}

				// check if next event is close enough to this one to just include in this highlight entry
				int nNextImportantTickIndex = FindNextImportantTickByXuid( m_ImportantTicks[ nImportantTickIndex ].nTick + 1, m_highlightSteamID );
				while ( nNextImportantTickIndex != -1 )
				{
					if ( CheckKeyXuid( m_highlightSteamID, m_ImportantTicks[ nNextImportantTickIndex ].pKeys, szKeyToCheck ) ||
						 CheckKeyXuid( m_highlightSteamID, m_ImportantTicks[ nNextImportantTickIndex ].pKeys, "player_xuid" ) )
					{
						if ( m_ImportantTicks[ nNextImportantTickIndex ].nPreviousTick - nTicksBeforeEvent <= entry.nPlayToTick )
						{
							entry.nPlayToTick = m_ImportantTicks[ nNextImportantTickIndex ].nTick + nTicksAfterEvent;
							entry.nActualLastEventTick = m_ImportantTicks[ nNextImportantTickIndex ].nTick;
							entry.nNumEvents++;
							nImportantTickIndex = nNextImportantTickIndex;
						}
						else
						{
							break;
						}
					}
					nNextImportantTickIndex = FindNextImportantTickByXuid( m_ImportantTicks[ nNextImportantTickIndex ].nTick + 1, m_highlightSteamID );
				}
			
				m_highlights.AddToTail( entry );
			}

			nImportantTickIndex = FindNextImportantTickByXuid( m_ImportantTicks[ nImportantTickIndex ].nTick + 1, m_highlightSteamID );
		}

		// when we cross a round start between highlights, we include the round start in the highlight
		// the playback will fast forward from the round start until nearby the highlight
		if ( 0 )
		for ( int i = 1; i < m_highlights.Count(); i++ )
		{
			int nTicksSkipToRoundThreshold = demo_highlight_skipthreshold.GetFloat() / host_state.interval_per_tick;
			int nRoundTick = FindPreviousImportantTick( m_highlights[ i ].nFastForwardToTick, "round_start" );
			if ( nRoundTick != -1 && ( m_ImportantTicks[ nRoundTick ].nTick - m_highlights[ i - 1 ].nPlayToTick ) > nTicksSkipToRoundThreshold )
			{
				m_highlights[ i ].nSeekToTick = m_ImportantTicks[ nRoundTick ].nPreviousTick;
			}
		}

		if ( m_highlights.Count() > 0 )
		{
			// add on a seek to the end of the match and play from their to the end of the demo, this will show the scoreboard
			int nEndMatchTickIndex = demoplayer->FindPreviousImportantTick( m_DemoFile.m_DemoHeader.playback_ticks, "announce_phase_end" );
			if ( nEndMatchTickIndex != -1 )
			{
				int nTicksBeforeMatchEnd = 1.0f / host_state.interval_per_tick;
				entry.nSeekToTick = m_ImportantTicks[ nEndMatchTickIndex ].nPreviousTick - nTicksBeforeMatchEnd;
				entry.nFastForwardToTick = m_ImportantTicks[ nEndMatchTickIndex ].nPreviousTick - nTicksBeforeMatchEnd;
				entry.nPlayToTick = m_ImportantTicks[ nEndMatchTickIndex ].nTick;
				entry.nActualFirstEventTick = entry.nActualLastEventTick = m_ImportantTicks[ nEndMatchTickIndex ].nTick;
				entry.nNumEvents = 0;
				entry.unAccountID = m_highlightSteamID.GetAccountID();

				m_highlights.AddToTail( entry );
			}

			m_nCurrentHighlight = 0;
		}
		else
		{
			// didn't find any highlights, so kill the parameters so the demo will play normally
			m_pPlaybackParameters = NULL;
		}
		m_bDoHighlightScan = false;
	}
}

int CDemoPlayer::FindNextImportantTick( int nCurrentTick, const char *pEventName /* = NULL */ )
{
	FOR_EACH_VEC( m_ImportantTicks, i )
	{
		if ( m_ImportantTicks[ i ].nTick > nCurrentTick )
		{
			if ( pEventName == NULL || V_stricmp( m_ImportantGameEvents[ m_ImportantTicks[ i ].nImportanGameEventIndex ].pszEventName, pEventName ) == 0 )
			{
				return i;
			}
		}
	}

	return -1;
}

int CDemoPlayer::FindPreviousImportantTick( int nCurrentTick, const char *pEventName /* = NULL */ )
{
	FOR_EACH_VEC_BACK( m_ImportantTicks, i )
	{
		if ( m_ImportantTicks[ i ].nTick < nCurrentTick )
		{
			if ( pEventName == NULL || V_stricmp( m_ImportantGameEvents[ m_ImportantTicks[ i ].nImportanGameEventIndex ].pszEventName, pEventName ) == 0 )
			{
				return i;
			}
		}
	}

	return -1;
}

int CDemoPlayer::FindNextImportantTickByXuidAndEvent( int nCurrentTick, const CSteamID &steamID, const char *pKeyWithXuid, const char *pEventName /* = NULL */ )
{
	FOR_EACH_VEC( m_ImportantTicks, i )
	{
		if ( m_ImportantTicks[ i ].nTick > nCurrentTick )
		{
			CSteamID compareSteamID( m_ImportantTicks[ i ].pKeys->GetUint64( pKeyWithXuid ) );
			if ( ( steamID.GetAccountID() == compareSteamID.GetAccountID() ) && ( pEventName == NULL || V_stricmp( m_ImportantGameEvents[ m_ImportantTicks[ i ].nImportanGameEventIndex ].pszEventName, pEventName ) == 0 ) )
			{
				return i;
			}
		}
	}

	return -1;
}

int CDemoPlayer::FindNextImportantTickByXuid( int nCurrentTick, const CSteamID &steamID )
{
	FOR_EACH_VEC( m_ImportantTicks, i )
	{
		if ( m_ImportantTicks[ i ].nTick > nCurrentTick )
		{
			CSteamID playerSteamID( m_ImportantTicks[ i ].pKeys->GetUint64( "player_xuid" ) );
			if ( steamID.GetAccountID() == playerSteamID.GetAccountID() )
			{
				return i;
			}
			CSteamID attackerSteamID( m_ImportantTicks[ i ].pKeys->GetUint64( "attacker_xuid" ) );
			if ( steamID.GetAccountID() == attackerSteamID.GetAccountID() )
			{
				return i;
			}
			CSteamID victimSteamID( m_ImportantTicks[ i ].pKeys->GetUint64( "victim_xuid" ) );
			if ( steamID.GetAccountID() == victimSteamID.GetAccountID() )
			{
				return i;
			}
		}
	}

	return -1;
}

int CDemoPlayer::FindPreviousImportantTickByXuidAndEvent( int nCurrentTick, const CSteamID &steamID, const char *pKeyWithXuid, const char *pEventName /* = NULL */ )
{
	FOR_EACH_VEC_BACK( m_ImportantTicks, i )
	{
		if ( m_ImportantTicks[ i ].nTick < nCurrentTick )
		{
			CSteamID compareSteamID( m_ImportantTicks[ i ].pKeys->GetUint64( pKeyWithXuid ) );
			if ( ( steamID.GetAccountID() == compareSteamID.GetAccountID() ) && ( pEventName == NULL || V_stricmp( m_ImportantGameEvents[ m_ImportantTicks[ i ].nImportanGameEventIndex ].pszEventName, pEventName ) == 0 ) )
			{
				return i;
			}
		}
	}

	return -1;
}

const DemoImportantTick_t *CDemoPlayer::GetImportantTick( int nIndex )
{
	if ( m_ImportantTicks.IsValidIndex( nIndex ) )
	{
		return &m_ImportantTicks[ nIndex ];
	}

	return NULL;
}

const DemoImportantGameEvent_t *CDemoPlayer::GetImportantGameEvent( const char *pszEventName )
{
	FOR_EACH_VEC( m_ImportantGameEvents, i )
	{
		if ( V_strcasecmp( m_ImportantGameEvents[ i ].pszEventName, pszEventName ) == 0 )
		{
			return &m_ImportantGameEvents[ i ];
		}
	}

	return NULL;
}

void CDemoPlayer::ListImportantTicks()
{
	Msg( "Important Ticks in demo:\n" );
	FOR_EACH_VEC( m_ImportantTicks, i )
	{
		if ( V_strcasecmp( m_ImportantGameEvents[ m_ImportantTicks[ i ].nImportanGameEventIndex ].pszEventName, "player_death" ) == 0 )
		{
			if ( m_ImportantTicks[ i ].pKeys->GetBool( "headshot" ) )
			{
				Msg( "Tick: %d : %s killed %s with a headshot using a %s\n", m_ImportantTicks[ i ].nTick, m_ImportantTicks[ i ].pKeys->GetString( "attacker_name" ), 
					m_ImportantTicks[ i ].pKeys->GetString( "victim_name" ), m_ImportantTicks[ i ].pKeys->GetString( "weapon" ) );
			}
			else
			{
				Msg( "Tick: %d : %s killed %s using a %s\n", m_ImportantTicks[ i ].nTick, m_ImportantTicks[ i ].pKeys->GetString( "attacker_name" ), 
					m_ImportantTicks[ i ].pKeys->GetString( "victim_name" ), m_ImportantTicks[ i ].pKeys->GetString( "weapon" ) );
			}
		}
		else
		{
			Msg( "Tick: %d  Event: %s \n", m_ImportantTicks[ i ].nTick, m_ImportantGameEvents[ m_ImportantTicks[ i ].nImportanGameEventIndex ].pszEventName );
		}
	}
}

void CDemoPlayer::ListHighlightData()
{
	if ( m_highlights.Count() > 0 )
	{
		Msg( "Highlights in demo:\n" );
		FOR_EACH_VEC( m_highlights, i )
		{
			Msg( "highlight: %d >> %d -> %d (%d) (%d,%d)\n", m_highlights[ i ].nSeekToTick, m_highlights[ i ].nFastForwardToTick,  m_highlights[ i ].nPlayToTick, 
				m_highlights[ i ].nNumEvents, m_highlights[ i ].nActualFirstEventTick, m_highlights[ i ].nActualLastEventTick );
		}
	}
	else
	{
		Msg( "No Highlights.\n" );
	}
}

static bool ComputeNextIncrementalDemoFilename( char *name, int namesize )
{
	FileHandle_t test;
	
	test = g_pFileSystem->Open( name, "rb" );
	if ( FILESYSTEM_INVALID_HANDLE == test )
	{
		// file doesn't exist, so we can use that 
		return true;
	}
	g_pFileSystem->Close( test );

	char basename[ MAX_OSPATH ];

	V_StripExtension( name, basename, sizeof( basename ) );

	// Start looking for a valid name
	int i = 0;
	for ( i = 0; i < 1000; i++ )
	{
		char newname[ MAX_OSPATH ];
		V_sprintf_safe( newname, "%s%03i.dem", basename, i );

		test = g_pFileSystem->Open( newname, "rb" );
		if ( FILESYSTEM_INVALID_HANDLE == test )
		{
			V_strncpy( name, newname, namesize );
			return true;
		}
		g_pFileSystem->Close( test );
	}

	ConMsg( "Unable to find a valid incremental demo filename for %s, try clearing the directory of %snnn.dem\n", name, basename );
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: List the contents of a demo file.
//-----------------------------------------------------------------------------
void CL_ListDemo_f( const CCommand &args )
{
	// Find the file
	char name[MAX_OSPATH];

	V_sprintf_safe(name, "%s", args[1]);
	
	V_DefaultExtension( name, ".dem", sizeof( name ) );

	ConMsg ("Demo contents for %s:\n", name);

	CDemoFile demofile;

	if ( !demofile.Open( name, true ) )
	{
		ConMsg ("ERROR: couldn't open.\n");
		return;
	}

	demofile.ReadDemoHeader( NULL );

	demoheader_t *header = &demofile.m_DemoHeader;

	if ( !header )
	{
		ConMsg( "Failed reading demo header.\n" );
		demofile.Close();
		return;
	}
	
	if ( V_strcmp( header->demofilestamp, DEMO_HEADER_ID ) )
	{
		ConMsg( "%s is not a valid demo file\n", name);
		return;
	}

	ConMsg("Network protocol: %i\n", header->networkprotocol);
	ConMsg("Demo version    : %i\n", header->demoprotocol);
	ConMsg("Server name     : %s\n", header->servername);
	ConMsg("Map name        : %s\n", header->mapname);
	ConMsg("Game            : %s\n", header->gamedirectory);
	ConMsg("Player name     : %s\n", header->clientname);
	ConMsg("Time            : %.1f\n", header->playback_time);
	ConMsg("Ticks           : %i\n", header->playback_ticks);
	ConMsg("Frames          : %i\n", header->playback_frames);
	ConMsg("Signon size     : %i\n", header->signonlength);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( stop, "Finish recording demo." )
{
	if ( !demorecorder->IsRecording() )
	{
		ConDMsg ("Not recording a demo.\n");
		return;
	}

	char name[ MAX_OSPATH ] = "Cannot stop recording now";

	if ( !g_ClientDLL->CanStopRecordDemo( name, sizeof( name ) ) )
	{
		ConMsg( "%s\n", name );	// re-use name as the error string if the client prevents us from stopping the demo
		return;
	}

	demorecorder->StopRecording();

	// Notify the client
	g_ClientDLL->OnDemoRecordStop();
}

static void DemoRecord( char const *pchDemoFileName, bool incremental )
{
	if ( g_ClientDLL == NULL )
	{
		ConMsg( "Can't record on dedicated server.\n" );
		return;
	}	

	if ( demorecorder->IsRecording() )
	{
		ConMsg ("Already recording.\n");
		return;
	}

	if ( demoplayer->IsPlayingBack() )
	{
		ConMsg ("Can't record during demo playback.\n");
		return;
	}

	// check path first
	if ( !COM_IsValidPath( pchDemoFileName ) )
	{
		ConMsg( "record %s: invalid path.\n", pchDemoFileName );
		return;
	}

	char name[ MAX_OSPATH ] = "Cannot record now";

	if ( !g_ClientDLL->CanRecordDemo( name, sizeof( name ) ) )
	{
		ConMsg( "%s\n", name );	// re-use name as the error string if the client prevents us from starting a demo
		return;
	}

	// remove .dem extentsion if user added it
	V_StripExtension( pchDemoFileName, name, sizeof( name ) );

	if ( incremental )
	{
		// If file exists, construct a better name
		if ( !ComputeNextIncrementalDemoFilename( name, sizeof( name ) ) )
		{
			return;
		}
	}

	// Notify polisher of record
	g_ClientDLL->OnDemoRecordStart( name );

	// Record it
	demorecorder->StartRecording( name, incremental );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND_F( record, "Record a demo.", FCVAR_DONTRECORD )
{
	if ( args.ArgC() != 2 && args.ArgC() != 3 )
	{
		ConMsg ("record <demoname> [incremental]\n");
		return;
	}

	bool incremental = false;
	if ( args.ArgC() == 3 )
	{
		if ( !V_stricmp( args[2], "incremental" ) )
		{
			incremental = true;
		}
	}
	
	DemoRecord( args[ 1 ], incremental );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND_F( _record, "Record a demo incrementally.", FCVAR_DONTRECORD )
{
	if ( g_ClientDLL == NULL )
	{
		ConMsg ("Can't record on dedicated server.\n");
		return;
	}	

	if ( args.ArgC() != 2 )
	{
		ConMsg ("_record <demoname>\n");
		return;
	}

	DemoRecord( args[ 1 ], true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
static CDemoPlaybackParameters_t s_DemoPlaybackParams; // make sure these parameters are available throughout demo playback
void SetPlaybackParametersLockFirstPersonAccountID( uint32 nAccountID )
{
	s_DemoPlaybackParams.m_uiLockFirstPersonAccountID = nAccountID;
}


void CL_PlayDemo_f( const CCommand &passed_args )
{
	demoplayer->StopPlayback();
	
	// disconnect before loading demo, to avoid sometimes loading into game instead of demo
	GetBaseLocalClient().Disconnect( false );

	// need to re-tokenize the args without the default breakset
	// the default splits out and of {}(): into separate args along with splitting on spaces
	// that makes it impossible to rebuild a file path that has a mixture of those characters in them
	characterset_t breakset;
	CharacterSetBuild( &breakset, "" );
	CCommand args;
	args.Tokenize( passed_args.GetCommandString(), passed_args.Source(), &breakset );

	if ( args.ArgC() < 2 )
	{
		ConMsg ("playdemo <demoname> <steamid>: plays a demo file. If steamid is given, then play highlights of that player \n");
		return;
	}

	if ( !net_time && !NET_IsMultiplayer() )
	{
		ConMsg( "Deferring playdemo command!\n" );
		return;
	}

	int nEndNameArg = args.ArgC() - 1;

	int nStartRound = 0;
	const char *szCurArg = args[ nEndNameArg ];
	const char *szStartRoundPrefix = "startround:";
	if ( char* szStartRoundParam = V_strstr( szCurArg, szStartRoundPrefix ) )
	{
		szStartRoundParam += strlen( szStartRoundPrefix );
		nStartRound = V_atoi( szStartRoundParam );
		nEndNameArg--;
	}

	// first check if last arg is "lowlights" and set flag
	bool bLowlights = false;
	if ( V_stricmp( args[ nEndNameArg ], "lowlights" ) == 0 )
	{
		bLowlights = true;
		nEndNameArg--;
	}

	// then check the last (or next to last if last was lowlights) to get the steam ID
	// steam IDs  are numbers only, we just check for digits in all of it
	int nSteamIDArg = nEndNameArg;

	bool bSteamID_self = !V_strcmp( args[ nSteamIDArg ], "self" );

	if ( bSteamID_self )
	{
		nEndNameArg--;
	}
	else
	{
		for ( int i = 0; i < V_strlen( args[ nSteamIDArg ] ); i++ )
		{
			if ( !V_isdigit( args[ nSteamIDArg ][ 0 ] ) )
			{
				nSteamIDArg = -1;
				break;
			}
		}
		if ( nSteamIDArg != -1 )
		{
			nEndNameArg--;
		}
		else if ( bLowlights )
		{
			ConMsg( "Warning: lowlights argument given without valid steam id, ignoring.\n" );
			bLowlights = false;
		}
	}

	// now take all remaining args and build them back into a path (will be multiple args if it has spaces or contains a ':'
	char name[ MAX_OSPATH ];
	if ( nEndNameArg > 1 )
	{
		V_strcpy_safe( name, args[ 1 ] );
		for ( int i = 2; i <= nEndNameArg; i++ )
		{
			V_strcat_safe( name, " " );
			V_strcat_safe( name, args[ i ] );
		}
	}
	else
	{
		V_strcpy_safe( name, args[ 1 ] );
	}

	// see if there is a starting tick attached to the filename (filename@####)
	int nStartingTick = -1;
	char *pTemp = V_strstr( name, "@" );
	if ( pTemp != NULL )
	{
		Assert( nStartRound == 0 ); // Don't specify both of these, start round will stomp
		nStartingTick = V_atoi(&pTemp[ 1 ]);
		if ( nStartingTick <= 0 )
		{
			nStartingTick = -1;
		}
		pTemp[0] = 0;
	}

	// set current demo player to client demo player
	demoplayer = g_pClientDemoPlayer;

	if ( demoplayer->IsPlayingBack() )
	{
		demoplayer->StopPlayback();
	}

	// disconnect before loading demo, to avoid sometimes loading into game instead of demo
	GetBaseLocalClient().Disconnect( false );

	CDemoPlaybackParameters_t *pParams = NULL;

	if ( nSteamIDArg != -1 )
	{
		CSteamID steamID;
		if ( bSteamID_self )
		{
			if ( ISteamUser* pSteamUser = Steam3Client().SteamUser() )
			{
				steamID = pSteamUser->GetSteamID();
			}
			else
			{
				ConMsg( "Cannot obtain user id\n" );
				return;
			}
		}
		else
		{
			steamID = CSteamID( args[ nSteamIDArg ] );
		}
		demoplayer->SetHighlightXuid( steamID.ConvertToUint64(), bLowlights );

		V_memset( &s_DemoPlaybackParams, 0, sizeof( s_DemoPlaybackParams ) );

		s_DemoPlaybackParams.m_uiHeaderPrefixLength = 0;
		s_DemoPlaybackParams.m_bAnonymousPlayerIdentity = false;
		s_DemoPlaybackParams.m_uiLockFirstPersonAccountID = steamID.GetAccountID();
		s_DemoPlaybackParams.m_numRoundSkip = 0;
		s_DemoPlaybackParams.m_numRoundStop = 999;
		s_DemoPlaybackParams.m_bSkipWarmup = false;
		pParams = &s_DemoPlaybackParams;
	}
	else if ( nStartRound > 0 )
	{
		s_DemoPlaybackParams.m_uiHeaderPrefixLength = 0;
		s_DemoPlaybackParams.m_bAnonymousPlayerIdentity = false;
		s_DemoPlaybackParams.m_uiLockFirstPersonAccountID = 0;
		s_DemoPlaybackParams.m_numRoundSkip = nStartRound - 1; 
		s_DemoPlaybackParams.m_numRoundStop = 999;
		s_DemoPlaybackParams.m_bSkipWarmup = true;
		pParams = &s_DemoPlaybackParams;
		demoplayer->SetHighlightXuid( 0, false );
	}
	else
	{
		demoplayer->SetHighlightXuid( 0, false );
	}

	//
	// open the demo file
	//
	V_DefaultExtension( name, ".dem", sizeof( name ) );

	if ( demoplayer != g_pClientDemoPlayer )
	{
		demoplayer->StopPlayback();
		demoplayer = g_pClientDemoPlayer;
	}

	if ( g_pClientDemoPlayer->StartPlayback( name, false, pParams, nStartingTick ) )
	{
		// Remove extension
		char basename[ MAX_OSPATH ];
		V_StripExtension( name, basename, sizeof( basename ) );
		g_ClientDLL->OnDemoPlaybackStart( basename );
	}
	else
	{
		SCR_EndLoadingPlaque();
	}
}

void CL_ScanDemo_f(const CCommand &args)
{
	if (args.ArgC() < 2)
	{
		ConMsg("scandemo <demoname>: scans a demo file.\n");
		return;
	}

	// set current demo player to client demo player
	demoplayer = g_pClientDemoPlayer;

	if (demoplayer->IsPlayingBack())
	{
		demoplayer->StopPlayback();
	}

	// disconnect before loading demo, to avoid sometimes loading into game instead of demo
	GetBaseLocalClient().Disconnect(false);

	demoplayer->SetHighlightXuid( 0, false );

	//
	// open the demo file
	//
	char name[MAX_OSPATH];
	V_strcpy_safe( name, args[ 1 ] );
	V_DefaultExtension( name, ".dem", sizeof( name ) );

	s_nMaxViewers = 0;
	s_nMaxExternalTotal = 0;
	s_nMaxExternalLinked = 0;
	s_nMaxCombinedViewers = 0;

	if ( demoplayer->ScanDemo( name, "hltv_status" ) )
	{
		// Remove extension
		char basename[ MAX_OSPATH ];
		V_StripExtension( name, basename, sizeof( basename ) );
		g_ClientDLL->OnDemoPlaybackStart( basename );
	}
	else
	{
		SCR_EndLoadingPlaque();
	}
}

void CL_ScanDemoDone( const char *pszMode )
{
	if ( V_strcasecmp( pszMode, "hltv_status" ) == 0 )
	{
		Msg( "Max GOTV Viewers: %d  Max External Viewers: %d  Max External Linked: %d Max Combined Viewers: %d \n", s_nMaxViewers, s_nMaxExternalTotal, s_nMaxExternalLinked, s_nMaxCombinedViewers );
	}

	demoplayer->StopPlayback();
	GetBaseLocalClient().Disconnect( false );
}

void CL_PlayOverwatchEvidence_f( const CCommand &args )
{
	if ( args.ArgC() != 3 )
	{
		DevMsg( "playoverwatchevidence syntax error.\n" );
		return;
	}

	//
	// Validate the header
	//
	char const *szCaseKey = args[1];
	char name[ MAX_OSPATH ];
	V_strcpy_safe( name, args[2] );

	if ( !g_pFullFileSystem->FileExists( name ) )
	{
		DevMsg( "playoverwatchevidence no file.\n" );
		return;
	}

	CUtlBuffer bufHeader;
	if ( !g_pFullFileSystem->ReadFile( name, NULL, bufHeader, 128 ) )
	{
		DevMsg( "playoverwatchevidence read file error.\n" );
		return;
	}
	if ( bufHeader.TellMaxPut() != 128 )
	{
		DevMsg( "playoverwatchevidence header of invalid size.\n" );
		return;
	}

	static CDemoPlaybackParameters_t params; // make sure these parameters are available throughout demo playback
	V_memset( &params, 0, sizeof( params ) );
	params.m_uiHeaderPrefixLength = 128;
	if ( !g_ClientDLL->ValidateSignedEvidenceHeader( szCaseKey, bufHeader.Base(), &params ) )
		return;

	// set current demo player to client demo player
	demoplayer = g_pClientDemoPlayer;
	//
	// open the demo file
	//
	if ( g_pClientDemoPlayer->StartPlayback( name, false, &params ) )
	{
		// Remove extension
		char basename[ MAX_OSPATH ];
		V_StripExtension( name, basename, sizeof( basename ) );
		g_ClientDLL->OnDemoPlaybackStart( basename );
	}
	else
	{
		SCR_EndLoadingPlaque();
	}
}


void CL_TimeDemo_Helper( const char *pDemoName, const char *pStatsFileName, const char *pVProfStatsFileName )
{
	V_strncpy( g_pStatsFile, pStatsFileName ? pStatsFileName : "UNKNOWN", sizeof( g_pStatsFile ) );

	// set current demo player to client demo player
	demoplayer = g_pClientDemoPlayer;

	// open the demo file
	char name[ MAX_OSPATH ];
	V_strcpy_safe(name, pDemoName );
	V_DefaultExtension( name, ".dem", sizeof( name ) );

	if( pVProfStatsFileName )
	{
		g_EngineStats.EnableVProfStatsRecording( pVProfStatsFileName );
	}

	if ( !g_pClientDemoPlayer->StartPlayback( name, true, NULL ) )
	{
		SCR_EndLoadingPlaque();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CL_TimeDemo_f( const CCommand &args )
{
	if ( args.ArgC() < 2 || args.ArgC() > 3 )
	{
		ConMsg ("timedemo <demoname> <optional stats.txt> : gets demo speeds, writing perf resutls to the optional stats.txt\n");
		return;
	}
	CL_TimeDemo_Helper( args[1], ( args.ArgC() >= 3 ) ? args[2] : NULL, NULL );
}

void CL_TimeDemo_VProfRecord_f( const CCommand &args )
{
	if ( args.ArgC() != 3  )
	{
		ConMsg ("timedemo_vprofrecord <demoname> <vprof stats filename> : gets demo speeds, recording perf data to a vprof stats file\n");
		return;
	}
	CL_TimeDemo_Helper( args[1], NULL, args[2] );
}

void CL_TimeDemoQuit_f( const CCommand &args )
{
	demo_quitafterplayback.SetValue( 1 );
	CL_TimeDemo_f( args );
}

void CL_BenchFrame_f( const CCommand &args )
{
	if ( args.ArgC() != 4 )
	{
		ConMsg ("benchframe <demoname> <frame> <tgafilename>: takes a snapshot of a particular frame in a demo\n");
		return;
	}

	g_pClientDemoPlayer->SetBenchframe( MAX( 0, atoi( args[2] ) ), args[3] );

	s_bBenchframe = true;

	mat_norendering.SetValue( 1 );
	
	// set current demo player to client demo player
	demoplayer = g_pClientDemoPlayer;
	
	// open the demo file
	char name[ MAX_OSPATH ];
	V_strcpy_safe(name, args[1] );
	V_DefaultExtension( name, ".dem", sizeof( name ) );

	if ( !g_pClientDemoPlayer->StartPlayback( name, true, NULL ) )
	{
		SCR_EndLoadingPlaque();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CON_COMMAND( vtune, "Controls VTune's sampling." )
{
	if ( args.ArgC() != 2 )
	{
		ConMsg ("vtune \"pause\" | \"resume\" : Suspend or resume VTune's sampling.\n");
		return;
	}
	
	if( !V_strcasecmp( args[1], "pause" ) )
	{
		if(!vtune(false))
		{
			ConMsg("Failed to find \"VTPause()\" in \"vtuneapi.dll\".\n");
			return;
		}

		ConMsg("VTune sampling paused.\n");
	}

	else if( !V_strcasecmp( args[1], "resume" ) )
	{
		if(!vtune(true))
		{
			ConMsg("Failed to find \"VTResume()\" in \"vtuneapi.dll\".\n");
			return;
		}
		
		ConMsg("VTune sampling resumed.\n");
	}

	else
	{
		ConMsg("Unknown vtune option.\n");
	}

}


CON_COMMAND_AUTOCOMPLETEFILE( playdemo, CL_PlayDemo_f, "Play a recorded demo file (.dem ).", NULL, dem );
CON_COMMAND_AUTOCOMPLETEFILE( scandemo, CL_ScanDemo_f, "Scan a recorded demo file (.dem ) for specific game events and dump data.", NULL, dem );
CON_COMMAND_EXTERN_F( playoverwatchevidence, CL_PlayOverwatchEvidence_f, "Play evidence for an overwatch case.", FCVAR_HIDDEN );
CON_COMMAND_AUTOCOMPLETEFILE( timedemo, CL_TimeDemo_f, "Play a demo and report performance info.", NULL, dem );
CON_COMMAND_AUTOCOMPLETEFILE( timedemoquit, CL_TimeDemoQuit_f, "Play a demo, report performance info, and then exit", NULL, dem );
CON_COMMAND_AUTOCOMPLETEFILE( listdemo, CL_ListDemo_f, "List demo file contents.", NULL, dem );
CON_COMMAND_AUTOCOMPLETEFILE( benchframe, CL_BenchFrame_f, "Takes a snapshot of a particular frame in a time demo.", NULL, dem );
CON_COMMAND_AUTOCOMPLETEFILE( timedemo_vprofrecord, CL_TimeDemo_VProfRecord_f, "Play a demo and report performance info.  Also record vprof data for the span of the demo", NULL, dem );


CON_COMMAND( demo_pause, "Pauses demo playback." )
{
	float seconds = -1.0;

	if ( args.ArgC() == 2 )
	{
		seconds = atof( args[1] );
	}

	demoplayer->PausePlayback( seconds );
}

CON_COMMAND( demo_resume, "Resumes demo playback." )
{
	demoplayer->ResumePlayback();
}

CON_COMMAND( demo_togglepause, "Toggles demo playback." )
{
	if ( !demoplayer->IsPlayingBack() )
		return;
	
	if ( demoplayer->IsPlaybackPaused() )
	{
		demoplayer->ResumePlayback();
	}
	else
	{
		demoplayer->PausePlayback( -1 );
	}
}

CON_COMMAND( demo_goto, "Skips to location in demo." )
{
	bool bRelative = false;
	bool bPause = false;

	if ( args.ArgC() < 2 )
	{
		Msg( "Syntax: demo_goto <tick> [relative] [pause]\n" );
		Msg( "  eg: 'demo_gototick 6666' or 'demo_gototick 25%' or 'demo_gototick 42min'\n" );

		if ( demoplayer && demoplayer->IsPlayingBack() )
		{
			IDemoStream *pDemoStream = demoplayer->GetDemoStream();
			float flTotalTime = TICKS_TO_TIME( pDemoStream->GetTotalTicks() ) / 60.0f;

			Msg( "  Currently playing %d of %d ticks. Minutes:%.2f File:%s\n",
				demoplayer->GetPlaybackTick(), pDemoStream->GetTotalTicks(),
				flTotalTime, pDemoStream->GetUrl() );
		}
		return;
	}

	int iArg = 1;
	const char *strTick = args[ iArg++ ];
	int nTick = atoi( strTick );

	// If they gave us "50%" or "50 %", then head to percentage of the file.
	bool bIsPct = !!strchr( strTick, '%' );
	bool bIsMinutes = !!strchr( strTick, 'm' );
	if ( !bIsPct && ( args[ iArg ][ 0 ] == '%' ) )
	{
		iArg++;
		bIsPct = true;
	}
	else if ( !bIsMinutes && ( args[ iArg ][ 0 ] == 'm' ) )
	{
		iArg++;
		bIsMinutes = true;
	}

	if ( bIsPct )
	{
		nTick = Clamp( nTick, 0, 100 ) * demoplayer->GetDemoStream()->GetTotalTicks() / 100;
	}
	else if ( bIsMinutes )
	{
		nTick = Clamp( 60 * TIME_TO_TICKS( nTick ), 0, demoplayer->GetDemoStream()->GetTotalTicks() - 100 );
	}

	for ( ; iArg < args.ArgC(); iArg++ )
	{
		switch ( toupper( args[ iArg ][ 0 ] ) )
		{
		case 'R':
			bRelative = true;
			break;
		case 'P':
			bPause = true;
			break;
		}
	}

	demoplayer->SkipToTick( nTick, bRelative, bPause );
}

CON_COMMAND( demo_gototick, "Skips to a tick in demo." )
{
	demo_goto( args );
}

CON_COMMAND( demo_info, "Print information about currently playing demo." )
{
	if ( !demoplayer->IsPlayingBack() )
	{
		Msg( "Error - Not currently playing back a demo.\n" );
		return;
	}

	ConMsg("Demo contents for %s:\n", demoplayer->GetDemoStream()->GetUrl());
}
CON_COMMAND( demo_timescale, "Sets demo replay speed." )
{
	float fScale = 1.0f;

	if ( args.ArgC() == 2 )
	{
		fScale = atof( args[1] );
		fScale = clamp( fScale, 0.0f, 100.0f );
	}

	demoplayer->SetPlaybackTimeScale( fScale );
}

CON_COMMAND( demo_listimportantticks, "List all important ticks in the demo." )
{
	demoplayer->ListImportantTicks();
}

CON_COMMAND( demo_listhighlights, "List all highlights data for the demo." )
{
	demoplayer->ListHighlightData();
}

bool CDemoPlayer::OverrideView( democmdinfo_t& info )
{
#if !defined( LINUX )
	if ( g_pDemoUI && g_pDemoUI->OverrideView( info, GetPlaybackTick() ) )
		return true;

	if ( demoaction && demoaction->OverrideView( info, GetPlaybackTick() ) )
		return true;
#endif
	return false;
}

void CDemoPlayer::ResetDemoInterpolation( void )
{
	m_bResetInterpolation = true;
}
