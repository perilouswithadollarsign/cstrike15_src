//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose:	See c_memorylog.h
//
//=============================================================================//

#include "cbase.h"
#include "inetchannelinfo.h"
#include "shaderapi/gpumemorystats.h"
#include "tier1/fmtstr.h"
#include "c_memorylog.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#ifdef C_MEMORYLOG_TICKING_ENABLED

ConVar memorylog_tick( "memorylog_tick", "20", 0, "Set to N to spew free memory to the console every N seconds to be captured by logging (0 disables)." );
ConVar memorylog_mem_dump( "memorylog_mem_dump", "1", 0 );
ConVar memorylog_spewhunkusage( "memorylog_spewhunkusage", "0", 0, "Set to 1 to spew hunk memory usage info after map load" );

// Memory log auto game system instantiation
C_MemoryLog g_MemoryLog;

const char *GetMapName( void )
{
	static char mapName[32];
	mapName[0] = 0;
	if ( engine->GetLevelNameShort() )
		V_strncpy( mapName, engine->GetLevelNameShort(), sizeof( mapName ) );
	if ( !mapName[ 0 ] )
		V_strncpy( mapName, "none", sizeof( mapName ) );
	return mapName;
}

void Cert_DebugString( const char * psz )
{
	// Defined so we can spew this in a CERT build (useful for double-checking memory near the end of the project)
#if defined( _X360 )
	// TODO: Disabled since we saw a couple of hangs inside OutputDebugStringA inside here: XBX_OutputDebugString( psz );
	Msg( "%s", psz );
#elif defined( _WIN32 )
	::OutputDebugStringA( psz );
#elif defined(_PS3)
	printf( "%s", psz );
#endif
}

bool C_MemoryLog::Init( void )
{
	// Spew the current date+time on startup, to help associate logs with crashdumps
	struct tm time;
	Plat_GetLocalTime( &time );
	Cert_DebugString( CFmtStr( "!MEMORYLOG! %4d/%02d/%02d %02d:%02d:%02d\n", time.tm_year+1900, time.tm_mon+1, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec ) );
	// Spew on the first frame after init
	m_fLastSpewTime = -memorylog_tick.GetFloat();
	// Set up a string we can search for in full heap crashdumps:
	memset( m_nRecentFreeMem, 0, sizeof( m_nRecentFreeMem ) );
	memcpy( m_nRecentFreeMem, "maryhadatinylamb", 16 );
	return true;
}

void C_MemoryLog::Spew( void )
{
	// Spew "time | free mem | GPU free | listen/not | map | bots | players",
	// so we can use console logs to correlate memory leaks with playtest patterns.

	if ( memorylog_tick.GetFloat() <= 0 )
		return; // Disabled

	int time = (int)( Plat_FloatTime() + 0.5f );

	// NOTE: freeMem can be negative on PS3 (you can use more memory on a devkit than a retail kit has)
	int usedMemory, freeMemory;
	g_pMemAlloc->GlobalMemoryStatus( (size_t *)&usedMemory, (size_t *)&freeMemory );

	// Determine if this machine is the server
	INetChannelInfo *pNetInfo = engine->GetNetChannelInfo();
	bool listen = ( pNetInfo && ( pNetInfo->IsLoopback() || V_strstr( pNetInfo->GetAddress(), "127.0.0.1" ) ) );

	const char *mapName = GetMapName();
	char playerNames[ 512 ] = "", memoryLogLine[768] = "";
	int  numBots    = 0;
	int  numPlayers = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		player_info_s playerInfo;
		CBasePlayer *player = UTIL_PlayerByIndex( i );
		if ( player && player->IsPlayer() && engine->GetPlayerInfo( i, &playerInfo ) )
		{
			char playerName[64];
			V_snprintf(	playerName, sizeof( playerName ), ", %s%s%s%s",
						playerInfo.name,
						player->IsObserver() ? "|SPEC" : "",
						C_BasePlayer::IsLocalPlayer( player ) ? "|LOCAL" : "",
						playerInfo.fakeplayer ? "|BOT" : "" );
			V_strcat( playerNames, playerName, sizeof( playerNames ) );
			if ( playerInfo.fakeplayer )
				numBots++;
			else
				numPlayers++;
		}
	}

	// Get GPU free memory (TODO: the current PS3 allocator's nGPUMemFree isn't very useful, it's a high watermark)
	GPUMemoryStats stats;
	materials->GetGPUMemoryStats( stats );
	unsigned int GPUFree = stats.nGPUMemFree;
	V_snprintf( memoryLogLine, ARRAYSIZE(memoryLogLine),
			"[MEMORYLOG] Time:%6d | Free:%6.2f | GPU:%6.2f | %s | Map: %-32s | Bots:%2d | Players: %2d%s\n",
			time,
			freeMemory / ( 1024.0f*1024.0f ),
			GPUFree / ( 1024.0f*1024.0f ),
			listen ? "Server" : "Client",
			mapName,
			numBots, numPlayers, playerNames );
	Cert_DebugString( memoryLogLine );


	// Spew generic memory stats into a freeform [MEMORYLOG2] line
	GenericMemoryStat_t *pMemStats = NULL, *pMemStats2 = NULL;
	int nMemStats  = g_pMemAlloc->GetGenericMemoryStats( &pMemStats );
	int nMemStats2 = engine->GetGenericMemoryStats( &pMemStats2 );
	if ( ( pMemStats && nMemStats ) || ( pMemStats2 && nMemStats2 ) )
	{
		// Spew N items per line, so they don't get truncated:
		const int ITEMS_PER_LINE = 4;
		int nStart = 0;
		while( nStart < ( nMemStats + nMemStats2 ) )
		{
			char *pCursor = memoryLogLine;
			V_snprintf( pCursor, ARRAYSIZE(memoryLogLine), "[MEMORYLOG2][%6d](Map:%s)", time, mapName );
			pCursor += V_strlen( pCursor );
			int nItem = 0;
			for ( int i = 0; pMemStats && ( i < nMemStats ); i++, nItem++ )
			{
				if ( ( nItem < nStart ) || ( nItem >= ( nStart + ITEMS_PER_LINE ) ) ) continue;
				int nAvail = ( ARRAYSIZE(memoryLogLine) - 1 ) - ( pCursor - memoryLogLine );
				V_snprintf( pCursor, nAvail, " [%s:%d]", pMemStats[i].name, pMemStats[i].value );
				pCursor += V_strlen( pCursor );
			}
			for ( int i = 0; pMemStats2 && ( i < nMemStats2 ); i++, nItem++ )
			{
				if ( ( nItem < nStart ) || ( nItem >= ( nStart + ITEMS_PER_LINE ) ) ) continue;
				int nAvail = ( ARRAYSIZE(memoryLogLine) - 1 ) - ( pCursor - memoryLogLine );
				V_snprintf( pCursor, nAvail, " [%s:%d]", pMemStats2[i].name, pMemStats2[i].value );
				pCursor += V_strlen( pCursor );
			}

			pCursor[0] = '\n';
			pCursor[1] = 0;
			Cert_DebugString( memoryLogLine );
			nStart += ITEMS_PER_LINE;
		}
	}


	// For now, also add a line tracking the behaviour of the GPU VB/IB allocator:
	//engine->ClientCmd( "gpu_buffer_allocator_spew brief" );

	// Keep the last N free memory values in an array, for inspection in full heap crashdumps
	for ( int i = 255; i > 4; i-- ) m_nRecentFreeMem[ i ] = m_nRecentFreeMem[ i - 1 ];
	m_nRecentFreeMem[ 4 ] = freeMemory;

	if ( memorylog_mem_dump.GetBool() )
	{
#if defined( _MEMTEST )
		g_pMemAlloc->SetStatsExtraInfo( mapName, CFmtStr( "%d %d %d %d %d %d %d",
			stats.nGPUMemSize, stats.nGPUMemFree, stats.nTextureSize, stats.nRTSize, stats.nVBSize, stats.nIBSize, stats.nUnknown ) );
#else
		V_snprintf( memoryLogLine, ARRAYSIZE(memoryLogLine),
			"[MEMORYSTATUS] [%s] RSX memory: total %.1fkb, free %.1fkb, textures %.1fkb, render targets %.1fkb, vertex buffers %.1fkb, index buffers %.1fkb, unknown %.1fkb\n",
			mapName, stats.nGPUMemSize/1024.0f, stats.nGPUMemFree/1024.0f, stats.nTextureSize/1024.0f, stats.nRTSize/1024.0f, stats.nVBSize/1024.0f, stats.nIBSize/1024.0f, stats.nUnknown/1024.0f );
		Cert_DebugString( memoryLogLine );
		// TODO: it would be good to get at least per-module totals from the release allocator
#endif
		g_pMemAlloc->DumpStatsFileBase( mapName );
		Msg( "\nDatacache reports:\n" );
		g_pDataCache->OutputReport( DC_SUMMARY_REPORT, NULL );
	}
}

void C_MemoryLog::Update( float frametime )
{
	float curTime = Plat_FloatTime();
	if ( ( curTime - m_fLastSpewTime ) >= memorylog_tick.GetFloat() )
	{
		m_fLastSpewTime = curTime;
		Spew();
		float spewTime = Plat_FloatTime() - curTime;
		if ( spewTime > 0.15f*memorylog_tick.GetFloat() )
		{
			// If Spew() takes a long time, account for it (otherwise keep memorylog ticks 'aligned')
			m_fLastSpewTime += spewTime;
		}
	}
}

void C_MemoryLog::LevelInitPostEntity( void )
{
	// Spew on the first frame after map load
	m_fLastSpewTime = -memorylog_tick.GetFloat();

	// Spew hunk memory usage after map load
	if ( memorylog_spewhunkusage.GetBool() )
	{
		engine->ClientCmd( "hunk_print_allocations" );
	}

	if ( memorylog_mem_dump.GetBool() )
	{
		// Do an autosave to generate savegame data size spew during batch runs of the game:
		engine->ClientCmd( "autosave" );
	}
}

void C_MemoryLog::LevelShutdownPreEntity( void )
{
	// Spew in case we don't make it to the next map
	Spew();
}

#endif // C_MEMORYLOG_TICKING_ENABLED
