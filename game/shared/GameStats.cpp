//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "cbase.h"

#include "igamesystem.h"
#include "GameStats.h"
#include "tier1/utlstring.h"
#include "filesystem.h"
#include "tier1/utlbuffer.h"
#include "fmtstr.h"
#include "tier2/tier2.h"
#include "tier0/cpumonitoring.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"

#ifndef DEDICATED
#include "iregistry.h"
#endif

#include "tier1/utldict.h"
#include "tier0/icommandline.h"
#include "tier0/perfstats.h"
#include <time.h>
#ifdef GAME_DLL
#include "vehicle_base.h"
#endif 

#ifdef CLIENT_DLL
#include "vgui_int.h"
#include "igameresources.h"
#include "voice_status.h"
extern const ConVar *sv_cheats;
#ifndef NO_STEAM
#include "steamworks_gamestats_client.h"
#endif
#include "materialsystem/materialsystem_config.h"
#endif

#if defined( _X360 )
#include "xbox/xbox_win32stubs.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"



#define GAMESTATS_LOG_FILE "gamestats.log"
#define GAMESTATS_PATHID "MOD"

/*
#define ONE_DAY_IN_SECONDS 86400

// Lower threshold in debug for testing...
#if defined( _DEBUG )
#define WALKED_AWAY_FROM_KEYBOARD_SECONDS 15.0f   // 15 seconds of movement == might be paused
#else
#define WALKED_AWAY_FROM_KEYBOARD_SECONDS 300.0f   // 5 minutes of no movement == might be paused
#endif
*/

extern IUploadGameStats *gamestatsuploader;

static char s_szPseudoUniqueID[20] = "";

static inline char const *SafeString( char const *pStr )
{
	return ( pStr ) ? pStr : "?";
}

static CBaseGameStats s_GameStats_Singleton;
CBaseGameStats *gamestats = &s_GameStats_Singleton; //start out pointing at the basic version which does nothing by default
extern ConVar skill;
void OverWriteCharsWeHate( char *pStr );

bool StatsTrackingIsFullyEnabled( void );

class CGamestatsData
{
public:
	CGamestatsData()
	{
		m_pKVData = NULL;
		m_bHaveData = false;
		AllocData();
	}

	~CGamestatsData()
	{
		FreeData();
	}

	void AllocData()
	{
		FreeData();
		char buffer[ 128 ];
		Q_snprintf( buffer, sizeof( buffer ), "gamestats_" );
		UTIL_GetModDir( buffer + 10, sizeof( buffer ) - 10 );
		m_pKVData = new KeyValues( buffer );
	}
	void FreeData()
	{
		if ( m_pKVData != NULL )
		{
			m_pKVData->deleteThis();
			m_pKVData = NULL;
		}
	}

	KeyValues *m_pKVData;
	bool m_bHaveData;
};

//used to drive most of the game stat event handlers as well as track basic stats under the hood of CBaseGameStats
class CBaseGameStats_Driver : public CAutoGameSystemPerFrame
{
public:
	CBaseGameStats_Driver( void );
	
	typedef CAutoGameSystemPerFrame BaseClass;

	// IGameSystem overloads
	virtual bool Init();
	virtual void Shutdown();

	// Level init, shutdown
	virtual void LevelInitPreEntity();
	virtual void LevelShutdownPreEntity();
	// Called during game save
	virtual void OnSave();
	// Called during game restore, after the local player has connected and entities have been fully restored
	virtual void OnRestore();

	virtual void FrameUpdatePostEntityThink();

	void PossibleMapChange( void );

	void CollectData( StatSendType_t sendType );
	void SendData();
	void ResetData();
	bool AddBaseDataForSend( KeyValues *pKV, StatSendType_t sendType );

	StatsBufferRecord_t m_StatsBuffer[STATS_WINDOW_SIZE];
	bool m_bBufferFull;
	int m_nWriteIndex;
	double m_flLastRealTime;
	double m_flLastSampleTime;
	float m_flTotalTimeInLevels;
	int m_iNumLevels;
	bool m_bDidVoiceChat;	// Did the player use voice chat at ALL this map?

	// Track how often the CPU is reported as running at full speed (above kCPUMonitoringWarning1),
	// medium speed (above kCPUMonitoringWarning2) or low speed (other).
	int m_nCPUUnthrottledCount;
	int m_nCPUThrottle1Count;
	int m_nCPUThrottle2Count;
	// Track when we last received data.
	double m_lastCPUFrequencyTimestamp;

	// Generate a buffer that will keep trailing observations prior to adding them to the main m_StatsBuffer array
	StatsBufferRecord_t m_TrailingStatsBuffer[STATS_TRAILING_WINDOW_SIZE];
	int m_nTrailingWriteIndex;
	int m_nTrailingSamplesWritten;	
	

	template<class T> T AverageStat( T StatsBufferRecord_t::*field ) const
	{
		T sum = 0;
		int iMax = m_bBufferFull ? ARRAYSIZE( m_StatsBuffer ) : m_nWriteIndex;
		for( int i = 0; i < iMax; i++ )
			sum += m_StatsBuffer[i].*field;
		return sum / iMax;
	}

	template<class T> T MaxStat( T StatsBufferRecord_t::*field ) const
	{
		T maxsofar = -16000000;
		int iMax = m_bBufferFull ? ARRAYSIZE( m_StatsBuffer ) : m_nWriteIndex;
		for( int i = 0; i < iMax; i++ )
			maxsofar = MAX( maxsofar, m_StatsBuffer[i].*field );
		return maxsofar;
	}

	template<class T> T MinStat( T StatsBufferRecord_t::*field ) const
	{
		T minsofar = 16000000;
		int iMax = m_bBufferFull ? ARRAYSIZE( m_StatsBuffer ) : m_nWriteIndex;
		for( int i = 0; i < iMax; i++ )
			minsofar = MIN( minsofar, m_StatsBuffer[i].*field );
		return minsofar;
	}

	template<class T> T StdDevStat( T StatsBufferRecord_t::*field, T average ) const
	{
		T flDeviationsquared = 0;
		int iMax = m_bBufferFull ? ARRAYSIZE( m_StatsBuffer ) : m_nWriteIndex;

		// Compute std deviation
		for (int i = 0; i < iMax; i++)
		{
			T val = m_StatsBuffer[i].*field - average;
			flDeviationsquared += (val * val);
		}

		return ( ( iMax > 1 ) ? sqrt( (flDeviationsquared / T(iMax - 1) ) ) : 0 );
	}

	void SortFramerate()
	{
		int iMax = ( ( m_bBufferFull ) ? ARRAYSIZE( m_StatsBuffer ) : m_nWriteIndex ) - 1;

		// Sort members of m_StatsBuffer
		std::sort(&m_StatsBuffer[0],&m_StatsBuffer[iMax]);
	}

	float PercentileFramerate( int nPercent )  const
	{
	    int iMax = ( ( m_bBufferFull ) ? ARRAYSIZE( m_StatsBuffer ) : m_nWriteIndex ) - 1;

		// Return the first or last member if nPercent is out of bounds
		if ( nPercent >= 100 )
			return m_StatsBuffer[iMax].m_flFrameRate;
		if ( nPercent <= 0 )
			return m_StatsBuffer[0].m_flFrameRate;

		// Otherwise determine the desired index and return its field.	
		// NOTE: nPercent = 50 is close to (but is NEVER) the median - special code would be needed to handle odd-item arrays, or averaging two elements for even-item arrays.
		int nIdx = (iMax * nPercent) / 100;
		return m_StatsBuffer[nIdx].m_flFrameRate;
	}

	inline void AdvanceIndex( void )
	{
		m_nWriteIndex++;
		if ( m_nWriteIndex == STATS_WINDOW_SIZE )
		{
			m_nWriteIndex = 0;
			m_bBufferFull = true;
		}
	}

	void UpdatePerfStats( void )
	{
		double flCurTime = Plat_FloatTime();

		if ( ( m_flLastRealTime > 0.0f ) &&
				( flCurTime > m_flLastRealTime ) )
		{				
			float flFps = 1.0 / ( flCurTime - m_flLastRealTime );
			float flCpuWait = g_PerfStats.m_Slots[ PERF_STATS_SLOT_END_FRAME ].m_PrevFrameTime.GetMillisecondsF();
			if ( g_pGameRules ) // are we in the game? if not, it's not worth collecting histogram info
			{
				uint8 nBin = m_FpsHistory.Update( flFps, flCpuWait );
				m_FpsHistogram.Update( nBin );
				for ( int nEvent = 0; nEvent < PERF_STATS_EVENT_COUNT; ++nEvent )
				{
					m_FpsHistogramGame[ nEvent ].Update( nBin );
				}
			}

			m_TrailingStatsBuffer[m_nTrailingWriteIndex].m_flFrameRate = flFps;

			m_nTrailingWriteIndex++;
			if ( m_nTrailingWriteIndex == STATS_TRAILING_WINDOW_SIZE )
				m_nTrailingWriteIndex = 0;
				
			if ( m_nTrailingSamplesWritten < STATS_TRAILING_WINDOW_SIZE )
				m_nTrailingSamplesWritten++;
		}

		if ( ( m_flLastSampleTime >= 0.0f ) && 
			( flCurTime - m_flLastSampleTime >= STATS_RECORD_INTERVAL ) )
		{
			if ( ( m_flLastRealTime > 0 ) && ( flCurTime > m_flLastRealTime ) )
			{
			
				StatsBufferRecord_t &stat = m_StatsBuffer[m_nWriteIndex];

				float flSumFrameRate = 0;
				for ( int i = 0; i < m_nTrailingSamplesWritten; i++ )
				{
					flSumFrameRate += m_TrailingStatsBuffer[i].m_flFrameRate;
				}
				float flFrameRate = flSumFrameRate / m_nTrailingSamplesWritten; 
				stat.m_flFrameRate = flFrameRate;

				m_nTrailingSamplesWritten = 0;
				m_nTrailingWriteIndex = 0;

#ifdef CLIENT_DLL
				// The stat system isn't designed to handle split screen players. Until it get's 
				// redesigned, let's take the first player's split screen ping, since all other stats
				// will be based on the first player
				IGameResources *gr = GameResources();
				int ping = 0;
				C_BasePlayer *pPlayer =	C_BasePlayer::GetLocalPlayer( FirstValidSplitScreenSlot() );
				if ( pPlayer && gr )
				{
					ping = gr->GetPing( pPlayer->entindex() );
				}
				stat.m_flServerPing = ping;
#endif

				stat.m_flMainThreadTimeMS		= g_PerfStats.m_Slots[PERF_STATS_SLOT_MAINTHREAD].m_PrevFrameTime.GetMillisecondsF();
				stat.m_flMainThreadWaitTimeMS	= g_PerfStats.m_Slots[PERF_STATS_SLOT_END_FRAME].m_PrevFrameTime.GetMillisecondsF();
				stat.m_flRenderThreadTimeMS		= g_PerfStats.m_Slots[PERF_STATS_SLOT_RENDERTHREAD].m_PrevFrameTime.GetMillisecondsF();
				stat.m_flRenderThreadWaitTimeMS	= g_PerfStats.m_Slots[PERF_STATS_SLOT_FORCE_HARDWARE_SYNC].m_PrevFrameTime.GetMillisecondsF();

				AdvanceIndex();
				m_flLastSampleTime = flCurTime;
			}
		}
		if ( m_flLastSampleTime == -1 )
		{
			m_flLastSampleTime = flCurTime;
		}
		m_flLastRealTime = flCurTime;

#ifdef CLIENT_DLL
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		{
			m_bDidVoiceChat |= GetClientVoiceMgr()->IsLocalPlayerSpeaking( FirstValidSplitScreenSlot() );
		}
#endif

		const CPUFrequencyResults results = GetCPUFrequencyResults( true );
		if ( results.m_timeStamp > m_lastCPUFrequencyTimestamp )
		{
			m_lastCPUFrequencyTimestamp = results.m_timeStamp;
			// Increment the appropriate counter for how much the CPU is throttled (if at all)
			if ( results.m_percentage > kCPUMonitoringWarning1 )
				++m_nCPUUnthrottledCount;
			else if ( results.m_percentage > kCPUMonitoringWarning2 )
				++m_nCPUThrottle1Count;
			else
				++m_nCPUThrottle2Count;
		}
	}


	void FirePerfStatsEvent( PerfStatsEventEnum_t nEvent, int nLookAhead = CFpsHistory::HISTORY_BUF_SIZE, int nLookBack = CFpsHistory::HISTORY_BUF_SIZE )
	{
		m_FpsHistogramGame[ nEvent ].Fire( nLookAhead, nLookBack, m_FpsHistory );
	}

	CUtlString		m_PrevMapName; //used to track "OnMapChange" events
	int				m_iLoadedVersion;
	char			m_szLoadedUserID[ 17 ];	// GUID

	bool			m_bEnabled; //false if incapable of uploading or the user doesn't want to enable stat tracking
	bool			m_bShuttingDown;
	bool			m_bInLevel;
	bool			m_bFirstLevel;
	time_t			m_tLastUpload;

	float			m_flLevelStartTime;

	bool			m_bStationary;
	float			m_flLastMovementTime;
	CUserCmd		m_LastUserCmd;
	bool			m_bGamePaused;
	float			m_flPauseStartTime;

	CGamestatsData	*m_pGamestatsData;

	CFpsHistory m_FpsHistory;
	CFpsHistogram m_FpsHistogram;
	CFpsSelectiveHistogram m_FpsHistogramGame[ PERF_STATS_EVENT_COUNT ];
};
static CBaseGameStats_Driver CBGSDriver;

void UpdatePerfStats( void )
{
	CBGSDriver.UpdatePerfStats();
}

void FirePerfStatsEvent( PerfStatsEventEnum_t nEvent, int nLookAhead, int nLookBack )
{
	CBGSDriver.FirePerfStatsEvent( nEvent, nLookAhead, nLookBack );
}

uint8 CFpsHistory::Update( float flFps, float flCpuWait )
{
	uint8 nBin = 0;
	if ( flFps >= 45 )
	{
		if ( flFps >= 90 )
		{
			if ( flFps >= 120 )
			{
				nBin = FPS_120;
			}
			else
			{
				nBin = FPS_90;
			}
		}
		else
		{
			if ( flFps >= 60 )
			{
				nBin = FPS_60;
			}
			else
			{
				nBin = FPS_45;
			}
		}
	}
	else
	{
		if ( flFps >= 20 )
		{
			if ( flFps >= 30 )
			{
				nBin = FPS_30;
			}
			else
			{
				nBin = FPS_20;
			}
		}
		else
		{
			if ( flFps >= 10 )
			{
				nBin = FPS_10;
			}
			else
			{
				nBin = FPS_LOW;
			}
		}
	}

	if ( flCpuWait > 0.5f )
	{
		nBin |= 0x80;
	}

	return nBin;
}

int64 CFpsHistogram::Encode()const
{
	uint nMaxBin = 0;
	for ( int i = 0; i < FPS_BIN_COUNT; ++i )
	{
		nMaxBin = Max( nMaxBin, m_nFps[ i ] );
	}

	if ( nMaxBin == 0 )
		return 0;

	int64 nEncode = 0;
	for ( int i = 0; i < FPS_BIN_COUNT / 2; ++i )
	{
		nEncode <<= 4;
		if ( uint nDenominator = m_nFps[ i * 2 ] + m_nFps[ i * 2 + 1 ] )
		{
			uint nNormalized = ( 15ll * ( m_nCpuWaits[ i * 2 ] + m_nCpuWaits[ i * 2 + 1 ] ) + nDenominator / 2 ) / nDenominator;
			nEncode |= nNormalized;
		}
	}
	for ( int i = 0; i < FPS_BIN_COUNT; ++i )
	{
		nEncode <<= 6;
		uint nNormalized = ( ( 0x3Fll * m_nFps[ i ] + nMaxBin / 2 ) / nMaxBin ); // round to nearest
		nEncode |= nNormalized;
	}
	return nEncode;
}


CBaseGameStats_Driver::CBaseGameStats_Driver( void ) :
	BaseClass( "CGameStats" ),
	m_iLoadedVersion( -1 ),
	m_bEnabled( false ),
	m_bShuttingDown( false ),
	m_bInLevel( false ),
	m_bFirstLevel( true ),
	m_flLevelStartTime( 0.0f ),
	m_bStationary( false ),
	m_flLastMovementTime( 0.0f ),
	m_bGamePaused( false ),
	m_pGamestatsData( NULL ),
	m_bBufferFull( false ),
	m_nWriteIndex( 0 ),
	m_flLastRealTime( -1 ),
	m_flLastSampleTime( -1 ),
	m_flTotalTimeInLevels( 0 ),
	m_iNumLevels( 0 ),
	m_bDidVoiceChat( false ),
	m_nTrailingWriteIndex( 0 ),
	m_nTrailingSamplesWritten( 0 )
{
	m_szLoadedUserID[0] = 0;
	m_tLastUpload = 0;
	m_LastUserCmd.Reset();
}

static FileHandle_t g_LogFileHandle = FILESYSTEM_INVALID_HANDLE;

CBaseGameStats::CBaseGameStats() :
	m_bLogging( false ),
	m_bLoggingToFile( false )
{
}

bool CBaseGameStats::StatTrackingAllowed( void )
{
	return CBGSDriver.m_bEnabled;
}

// Don't care about vcr hooks here...
#undef localtime
#undef asctime

#include <time.h>

void CBaseGameStats::StatsLog( char const *fmt, ... )
{
	if ( !m_bLogging && !m_bLoggingToFile )
		return;

	char buf[ 2048 ];
	va_list argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( buf, sizeof( buf ), fmt, argptr );
	va_end( argptr );

	// Prepend timestamp and spew it

	// Prepend the time.
	time_t aclock;
	time( &aclock );
	struct tm *newtime = localtime( &aclock );

	char timeString[ 128 ];
	Q_strncpy( timeString, asctime( newtime ), sizeof( timeString ) );
	// Get rid of the \n.
	char *pEnd = strstr( timeString, "\n" );
	if ( pEnd )
	{
		*pEnd = 0;
	}

	if ( m_bLogging )
	{
		DevMsg( "[GS %s - %7.2f] %s", timeString, gpGlobals->realtime, buf );
	}

	if ( m_bLoggingToFile )
	{
		if ( FILESYSTEM_INVALID_HANDLE == g_LogFileHandle )
		{
			g_LogFileHandle = filesystem->Open( GAMESTATS_LOG_FILE, "a", GAMESTATS_PATHID );
		}

		if ( FILESYSTEM_INVALID_HANDLE != g_LogFileHandle )
		{
			filesystem->FPrintf( g_LogFileHandle, "[GS %s - %7.2f] %s", timeString, gpGlobals->realtime, buf );
			filesystem->Flush( g_LogFileHandle );
		}
	}
}

static char s_szSaveFileName[256] = "";
static char s_szStatUploadRegistryKeyName[256] = "";

const char *CBaseGameStats::GetStatSaveFileName( void )
{
	AssertMsg( s_szSaveFileName[0] != '\0', "Don't know what file to save stats to." );
	return s_szSaveFileName;
}

const char *CBaseGameStats::GetStatUploadRegistryKeyName( void )
{
	AssertMsg( s_szStatUploadRegistryKeyName[0] != '\0', "Don't know the registry key to use to mark stats uploads." );
	return s_szStatUploadRegistryKeyName;
}

const char *CBaseGameStats::GetUserPseudoUniqueID( void )
{
	AssertMsg( s_szPseudoUniqueID[0] != '\0', "Don't have a pseudo unique ID." );
	return s_szPseudoUniqueID;
}

void CBaseGameStats::Event_Init( void )
{
#ifdef GAME_DLL
	SetSteamStatistic( filesystem->IsSteam() );
	SetCyberCafeStatistic( gamestatsuploader->IsCyberCafeUser() );
	SetDXLevelStatistic( g_pMaterialSystemHardwareConfig->GetDXSupportLevel() );
	++m_BasicStats.m_Summary.m_nCount;

	StatsLog( "CBaseGameStats::Event_Init [%dth session]\n", m_BasicStats.m_Summary.m_nCount );
#endif // GAME_DLL
}

void CBaseGameStats::Event_Shutdown( void )
{
#ifdef GAME_DLL
	StatsLog( "CBaseGameStats::Event_Shutdown [%dth session]\n", m_BasicStats.m_Summary.m_nCount );

	StatsLog( "\n====================================================================\n\n" );
#endif
}

void CBaseGameStats::Event_MapChange( const char *szOldMapName, const char *szNewMapName )
{
	StatsLog( "CBaseGameStats::Event_MapChange to [%s]\n", szNewMapName );
}

void CBaseGameStats::Event_LevelInit( void )
{
#ifdef GAME_DLL
	StatsLog( "CBaseGameStats::Event_LevelInit [%s]\n", CBGSDriver.m_PrevMapName.String() );

	BasicGameStatsRecord_t *map = gamestats->m_BasicStats.FindOrAddRecordForMap( CBGSDriver.m_PrevMapName.String() );
	++map->m_nCount;

	// HACK HACK:  Punching this hole through only works in single player!!!
	if ( gpGlobals->maxClients == 1 )
	{
		ConVarRef closecaption( "closecaption" );
		if( closecaption.IsValid() )
			SetCaptionsStatistic( closecaption.GetBool() );

		SetHDRStatistic( gamestatsuploader->IsHDREnabled() );

		SetSkillStatistic( skill.GetInt() );
		SetSteamStatistic( filesystem->IsSteam() );
		SetCyberCafeStatistic( gamestatsuploader->IsCyberCafeUser() );
	}
#endif // GAME_DLL
}

void CBaseGameStats::Event_LevelShutdown( float flElapsed )
{
#ifdef GAME_DLL
	BasicGameStatsRecord_t *map = m_BasicStats.FindOrAddRecordForMap( CBGSDriver.m_PrevMapName.String() );
	Assert( map );
	map->m_nSeconds += (int)flElapsed;
	gamestats->m_BasicStats.m_Summary.m_nSeconds += (int)flElapsed;

	StatsLog( "CBaseGameStats::Event_LevelShutdown [%s] %.2f elapsed %d total\n", CBGSDriver.m_PrevMapName.String(), flElapsed, gamestats->m_BasicStats.m_Summary.m_nSeconds );
#endif // GAME_DLL
}

void CBaseGameStats::Event_SaveGame( void )
{
	StatsLog( "CBaseGameStats::Event_SaveGame [%s]\n", CBGSDriver.m_PrevMapName.String() );
}

void CBaseGameStats::Event_LoadGame( void )
{
#ifdef GAME_DLL
	char const *pchSaveFile = engine->GetMostRecentlyLoadedFileName();
	StatsLog( "CBaseGameStats::Event_LoadGame [%s] from %s\n", CBGSDriver.m_PrevMapName.String(), pchSaveFile );
#endif
}

#ifdef GAME_DLL

void CBaseGameStats::Event_PlayerKilled( CBasePlayer *pPlayer, const CTakeDamageInfo &info )
{
	++m_BasicStats.m_Summary.m_nDeaths;

	if( CBGSDriver.m_bInLevel )
	{
		BasicGameStatsRecord_t *map = m_BasicStats.FindOrAddRecordForMap( CBGSDriver.m_PrevMapName.String() );
		++map->m_nDeaths;
		StatsLog( "  Player died %dth time in level [%s]!!!\n", map->m_nDeaths, CBGSDriver.m_PrevMapName.String() );
	}
	else
	{
		StatsLog( "  Player died, but not in a level!!!\n" );
		Assert( 0 );
	}

	StatsLog( "CBaseGameStats::Event_PlayerKilled [%s] [%dth death]\n", pPlayer->GetPlayerName(), m_BasicStats.m_Summary.m_nDeaths );
}

void CBaseGameStats::Event_Commentary()
{
	if( CBGSDriver.m_bInLevel )
	{
		BasicGameStatsRecord_t *map = m_BasicStats.FindOrAddRecordForMap( CBGSDriver.m_PrevMapName.String() );
		++map->m_nCommentary;
	}

	++m_BasicStats.m_Summary.m_nCommentary;

	StatsLog( "CBaseGameStats::Event_Commentary [%d]\n", m_BasicStats.m_Summary.m_nCommentary );
}

void CBaseGameStats::Event_Credits()
{
	StatsLog( "CBaseGameStats::Event_Credits\n" );

	float elapsed = 0.0f;
	if( CBGSDriver.m_bInLevel )
	{
		elapsed = gpGlobals->realtime - CBGSDriver.m_flLevelStartTime;
	}

	if( elapsed < 0.0f )
	{
		Assert( 0 );
		Warning( "EVENT_CREDITS with negative elapsed time (rt %f starttime %f)\n", gpGlobals->realtime, CBGSDriver.m_flLevelStartTime );
		elapsed = 0.0f;
	}

	// Only set this one time!!!
	if( gamestats->m_BasicStats.m_nSecondsToCompleteGame == 0 )
	{
		if( gamestats->UserPlayedAllTheMaps() )
		{
			gamestats->m_BasicStats.m_nSecondsToCompleteGame = elapsed + gamestats->m_BasicStats.m_Summary.m_nSeconds;
			gamestats->SaveToFileNOW();
		}
	}
}

void CBaseGameStats::Event_CrateSmashed()
{
	StatsLog( "CBaseGameStats::Event_CrateSmashed\n" );
}

void CBaseGameStats::Event_Punted( CBaseEntity *pObject )
{
	StatsLog( "CBaseGameStats::Event_Punted [%s]\n", pObject->GetClassname() );
}

void CBaseGameStats::Event_PlayerTraveled( CBasePlayer *pBasePlayer, float distanceInInches, bool bInVehicle, bool bSprinting )
{
}

void CBaseGameStats::Event_FlippedVehicle( CBasePlayer *pDriver, CPropVehicleDriveable *pVehicle )
{
	StatsLog( "CBaseGameStats::Event_FlippedVehicle [%s] flipped [%s]\n", pDriver->GetPlayerName(), pVehicle->GetClassname() );
}

// Called before .sav file is actually loaded (player should still be in previous level, if any)
void CBaseGameStats::Event_PreSaveGameLoaded( char const *pSaveName, bool bInGame )
{
	StatsLog( "CBaseGameStats::Event_PreSaveGameLoaded [%s] %s\n", pSaveName, bInGame ? "in-game" : "at console" );
}

bool CBaseGameStats::SaveToFileNOW( bool bForceSyncWrite /* = false */ )
{
	if ( !StatsTrackingIsFullyEnabled() )
		return false;

	// this code path is only for old format stats.  Products that use new format take a different path.
	if ( !gamestats->UseOldFormat() )
		return false;

	CUtlBuffer buf;
	buf.PutShort( GAMESTATS_FILE_VERSION );
	buf.Put( s_szPseudoUniqueID, 16 );

	if( ShouldTrackStandardStats() )
		m_BasicStats.SaveToBuffer( buf ); 
	else
		buf.PutInt( GAMESTATS_STANDARD_NOT_SAVED );

	gamestats->AppendCustomDataToSaveBuffer( buf );

	char fullpath[ 512 ] = { 0 };
	if ( filesystem->FileExists( GetStatSaveFileName(), GAMESTATS_PATHID ) )
	{
		filesystem->RelativePathToFullPath( GetStatSaveFileName(), GAMESTATS_PATHID, fullpath, sizeof( fullpath ) );
	}
	else
	{
		// filename is local to game dir for Steam, so we need to prepend game dir for regular file save
		char gamePath[256];
		engine->GetGameDir( gamePath, 256 );
		Q_StripTrailingSlash( gamePath );
		Q_snprintf( fullpath, sizeof( fullpath ), "%s/%s", gamePath, GetStatSaveFileName() );
		Q_strlower( fullpath );
		Q_FixSlashes( fullpath );
	}

	// StatsLog( "SaveToFileNOW '%s'\n", fullpath );

	if( CBGSDriver.m_bShuttingDown || bForceSyncWrite ) //write synchronously
	{
		filesystem->WriteFile( fullpath, GAMESTATS_PATHID, buf );

		StatsLog( "Shut down wrote to '%s'\n", fullpath );
	}
	else
	{
		// Allocate memory for async system to use (and free afterward!!!)
		size_t nBufferSize = buf.TellPut();
		void *pMem = malloc(nBufferSize);
		CUtlBuffer statsBuffer( pMem, nBufferSize );
		statsBuffer.Put( buf.Base(), nBufferSize );

		// Write data async
		filesystem->AsyncWrite( fullpath, statsBuffer.Base(), statsBuffer.TellPut(), true, false );
	}

	return true;
}

void CBaseGameStats::Event_PlayerConnected( CBasePlayer *pBasePlayer )
{
	StatsLog( "CBaseGameStats::Event_PlayerConnected [%s]\n", pBasePlayer->GetPlayerName() );
}

void CBaseGameStats::Event_PlayerDisconnected( CBasePlayer *pBasePlayer )
{
	StatsLog( "CBaseGameStats::Event_PlayerDisconnected [%s]\n", pBasePlayer->GetPlayerName() );
}

void CBaseGameStats::Event_PlayerDamage( CBasePlayer *pBasePlayer, const CTakeDamageInfo &info )
{
	//StatsLog( "CBaseGameStats::Event_PlayerDamage [%s] took %.2f damage\n", pBasePlayer->GetPlayerName(), info.GetDamage() );
}

void CBaseGameStats::Event_PlayerKilledOther( CBasePlayer *pAttacker, CBaseEntity *pVictim, const CTakeDamageInfo &info )
{
	StatsLog( "CBaseGameStats::Event_PlayerKilledOther [%s] killed [%s]\n", pAttacker->GetPlayerName(), pVictim->GetClassname() );
}

void CBaseGameStats::Event_WeaponFired( CBasePlayer *pShooter, bool bPrimary, char const *pchWeaponName )
{
	StatsLog( "CBaseGameStats::Event_WeaponFired [%s] %s weapon [%s]\n", pShooter->GetPlayerName(), bPrimary ? "primary" : "secondary", pchWeaponName );
}

void CBaseGameStats::Event_WeaponHit( CBasePlayer *pShooter, bool bPrimary, char const *pchWeaponName, const CTakeDamageInfo &info )
{
	StatsLog( "CBaseGameStats::Event_WeaponHit [%s] %s weapon [%s] damage [%f]\n", pShooter->GetPlayerName(), bPrimary ? "primary" : "secondary", pchWeaponName, info.GetDamage() );
}

void CBaseGameStats::Event_PlayerEnteredGodMode( CBasePlayer *pBasePlayer )
{
	StatsLog( "CBaseGameStats::Event_PlayerEnteredGodMode [%s] entered GOD mode\n", pBasePlayer->GetPlayerName() );
}

void CBaseGameStats::Event_PlayerEnteredNoClip( CBasePlayer *pBasePlayer )
{
	StatsLog( "CBaseGameStats::Event_PlayerEnteredNoClip [%s] entered NOCLIPe\n", pBasePlayer->GetPlayerName() );
}

void CBaseGameStats::Event_DecrementPlayerEnteredNoClip( CBasePlayer *pBasePlayer )
{
	StatsLog( "CBaseGameStats::Event_DecrementPlayerEnteredNoClip [%s] decrementing NOCLIPe\n", pBasePlayer->GetPlayerName() );
}

void CBaseGameStats::Event_IncrementCountedStatistic( const Vector& vecAbsOrigin, char const *pchStatisticName, float flIncrementAmount )
{
	StatsLog( "Incrementing %s by %f at pos (%d, %d, %d)\n", pchStatisticName, flIncrementAmount, (int)vecAbsOrigin.x, (int)vecAbsOrigin.y, (int)vecAbsOrigin.z );
}

// [dwenger] Needed for CS window-breaking stat
void CBaseGameStats::Event_WindowShattered( CBasePlayer *pPlayer )
{
	StatsLog( "In Event_WindowShattered\n" );
}

bool CBaseGameStats::UploadStatsFileNOW( void )
{
	if( !StatsTrackingIsFullyEnabled() || !HaveValidData() || !gamestats->UseOldFormat() )
		return false;

	if ( !filesystem->FileExists( gamestats->GetStatSaveFileName(), GAMESTATS_PATHID ) )
	{
		return false;
	}

	int curtime = Plat_FloatTime();

	CBGSDriver.m_tLastUpload = curtime;

	// Update the registry
#ifndef DEDICATED
	IRegistry *reg = InstanceRegistry( "Steam" );
	Assert( reg );
	reg->WriteInt( GetStatUploadRegistryKeyName(), CBGSDriver.m_tLastUpload );
	ReleaseInstancedRegistry( reg );
#endif

	CUtlBuffer buf;
	filesystem->ReadFile( GetStatSaveFileName(), GAMESTATS_PATHID, buf );
	unsigned int uBlobSize = buf.TellPut();
	if ( uBlobSize == 0 )
	{
		return false;
	}

	const void *pvBlobData = ( const void * )buf.Base();

	if( gamestatsuploader )
	{
		return gamestatsuploader->UploadGameStats( "",
												   1,
												   uBlobSize,
												   pvBlobData );
	}

	return false;
}


void CBaseGameStats::LoadingEvent_PlayerIDDifferentThanLoadedStats( void )
{
	StatsLog( "CBaseGameStats::LoadingEvent_PlayerIDDifferentThanLoadedStats\n" );
}


bool CBaseGameStats::LoadFromFile( void )
{
	if ( filesystem->FileExists( gamestats->GetStatSaveFileName(), GAMESTATS_PATHID ) )
	{
		char fullpath[ 512 ];
		filesystem->RelativePathToFullPath( gamestats->GetStatSaveFileName(), GAMESTATS_PATHID, fullpath, sizeof( fullpath ) );
		StatsLog( "Loading stats from '%s'\n", fullpath );
	}
	
	CUtlBuffer buf; 
	if ( filesystem->ReadFile( gamestats->GetStatSaveFileName(), GAMESTATS_PATHID, buf ) )
	{
		bool bRetVal = true;

		int version = buf.GetShort();
		if ( version > GAMESTATS_FILE_VERSION )
			return false; //file is beyond our comprehension

		// Set global parse version
		CBGSDriver.m_iLoadedVersion = version;

		buf.Get( CBGSDriver.m_szLoadedUserID, 16 );
		CBGSDriver.m_szLoadedUserID[ sizeof( CBGSDriver.m_szLoadedUserID ) - 1 ] = 0;

		if ( s_szPseudoUniqueID[ 0 ] != 0 )
		{			
			if ( Q_stricmp( CBGSDriver.m_szLoadedUserID, s_szPseudoUniqueID ) )
			{
				//UserID changed, blow away log!!!
				filesystem->RemoveFile( gamestats->GetStatSaveFileName(), GAMESTATS_PATHID );
				filesystem->RemoveFile( GAMESTATS_LOG_FILE, GAMESTATS_PATHID );
				Warning( "Userid changed, clearing stats file\n" );
				CBGSDriver.m_szLoadedUserID[0] = '\0';
				CBGSDriver.m_iLoadedVersion = -1;
				gamestats->m_BasicStats.Clear();
				gamestats->LoadingEvent_PlayerIDDifferentThanLoadedStats();
				bRetVal = false;
			}
		
			if ( version <= GAMESTATS_FILE_VERSION_OLD5 )
			{
				gamestats->m_BasicStats.Clear();
				bRetVal = false;
			}
			else
			{
				// Peek ahead in buffer to see if we have the "no default stats" secret flag set.
				int iCheckForStandardStatsInFile = *( int * )buf.PeekGet();
				bool bValid = true;

				if ( iCheckForStandardStatsInFile != GAMESTATS_STANDARD_NOT_SAVED )
				{
					//the GAMESTATS_STANDARD_NOT_SAVED flag coincides with user completion time, rewind so the gamestats parser can grab it
					bValid = gamestats->m_BasicStats.ParseFromBuffer( buf, version );
				}
				else
				{
					// skip over the flag
					buf.GetInt();
				}

				if( !bValid )
				{
					m_BasicStats.Clear();
				}

				if( ( buf.TellPut() - buf.TellGet() ) != 0 ) //more data left, must be custom data
				{
					gamestats->LoadCustomDataFromBuffer( buf );
				}
			}
		}

		return bRetVal;
	}
	else
	{
		filesystem->RemoveFile( GAMESTATS_LOG_FILE, GAMESTATS_PATHID );
	}

	return false;	
}

#endif // GAME_DLL

bool CBaseGameStats_Driver::Init()
{
	const char *pGameDir = CommandLine()->ParmValue( "-game", "hl2" );

	//standardizing is a good thing
	char szLoweredGameDir[256];
	Q_strncpy( szLoweredGameDir, pGameDir, sizeof( szLoweredGameDir ) );
	Q_strlower( szLoweredGameDir );

	gamestats = gamestats->OnInit( gamestats, szLoweredGameDir );

	//determine constant strings needed for saving and uploading
	Q_strncpy( s_szSaveFileName, szLoweredGameDir, sizeof( s_szSaveFileName ) );
	Q_strncat( s_szSaveFileName, "_gamestats.dat", sizeof( s_szSaveFileName ) );

	Q_strncpy( s_szStatUploadRegistryKeyName, "GameStatsUpload_", sizeof( s_szStatUploadRegistryKeyName ) );
	Q_strncat( s_szStatUploadRegistryKeyName, szLoweredGameDir, sizeof( s_szStatUploadRegistryKeyName ) );

	gamestats->m_bLoggingToFile = CommandLine()->FindParm( "-gamestatsloggingtofile" ) ? true : false;
	gamestats->m_bLogging = CommandLine()->FindParm( "-gamestatslogging" ) ? true : false;

	if ( gamestatsuploader )
	{
		m_bEnabled = gamestatsuploader->IsGameStatsLoggingEnabled();
		if ( m_bEnabled )
		{
			gamestatsuploader->GetPseudoUniqueId( s_szPseudoUniqueID, sizeof( s_szPseudoUniqueID ) );
		}
	}

	ResetData();

#ifdef GAME_DLL
	if ( StatsTrackingIsFullyEnabled() )
	{
		// FIXME: Load m_tLastUpload from registry and save it back out, too
#ifndef DEDICATED
		IRegistry *reg = InstanceRegistry( "Steam" );
		Assert( reg );
		m_tLastUpload = reg->ReadInt( gamestats->GetStatUploadRegistryKeyName(), 0 );
		ReleaseInstancedRegistry( reg );
#endif
		//load existing stats
		gamestats->LoadFromFile();
	}
#endif // GAME_DLL
		
	if ( s_szPseudoUniqueID[ 0 ] != 0 )
	{			
		gamestats->Event_Init();
#ifdef GAME_DLL
		if ( gamestats->UseOldFormat() )
		{
			if( gamestats->AutoSave_OnInit() )
				gamestats->SaveToFileNOW();

			if( gamestats->AutoUpload_OnInit() )
				gamestats->UploadStatsFileNOW();
		}
#endif
	}
	else
	{
		m_bEnabled = false; //unable to generate a pseudo-unique ID, disable tracking
	}

	return true;
}


void CBaseGameStats_Driver::Shutdown()
{
	m_bShuttingDown = true;

	gamestats->Event_Shutdown();

	if ( gamestats->UseOldFormat() )
	{
#ifdef GAME_DLL
		if( gamestats->AutoSave_OnShutdown() )
			gamestats->SaveToFileNOW();

		if( gamestats->AutoUpload_OnShutdown() )
			gamestats->UploadStatsFileNOW();
#endif // GAME_DLL
	}
	else
	{
		// code path for new format game stats
		if ( gamestats->ShouldSendDataOnAppShutdown() )
		{
			CollectData( STATSEND_APPSHUTDOWN );
			SendData();
		}
	}
	if ( FILESYSTEM_INVALID_HANDLE != g_LogFileHandle )
	{
		filesystem->Close( g_LogFileHandle );
		g_LogFileHandle = FILESYSTEM_INVALID_HANDLE;
	}

	if ( m_pGamestatsData != NULL )
	{
#ifdef CLIENT_DLL 
		engine->SetGamestatsData( NULL );
#endif
		delete m_pGamestatsData;
		m_pGamestatsData = NULL;
	}
}

void CBaseGameStats_Driver::PossibleMapChange( void )
{
#ifdef GAME_DLL
	//detect and copy map changes
	if ( Q_stricmp( m_PrevMapName.String(), STRING( gpGlobals->mapname ) ) )
	{
		MEM_ALLOC_CREDIT();

		CUtlString PrevMapBackup = m_PrevMapName;

		m_PrevMapName = STRING( gpGlobals->mapname );

		gamestats->Event_MapChange( PrevMapBackup.String(), STRING( gpGlobals->mapname ) );

		if ( gamestats->UseOldFormat() )
		{
			if( gamestats->AutoSave_OnMapChange() )
				gamestats->SaveToFileNOW();

			if( gamestats->AutoUpload_OnMapChange() )
				gamestats->UploadStatsFileNOW();
		}
	}
#endif
}



void CBaseGameStats_Driver::LevelInitPreEntity()
{
	m_bInLevel = true;
	m_bFirstLevel = false;

	if ( Q_stricmp( s_szPseudoUniqueID, "unknown" ) == 0 )
	{
		// "unknown" means this is a dedicated server and we weren't able to generate a unique ID (e.g. Linux server).
		// Change the unique ID to be a hash of IP & port.  We couldn't do this earlier because IP is not known until level
		// init time.
		ConVar *hostip = cvar->FindVar( "hostip" );
		ConVar *hostport = cvar->FindVar( "hostport" );
		if ( hostip && hostport )
		{
			int crcInput[2];
			crcInput[0] = hostip->GetInt();
			crcInput[1] = hostport->GetInt();
			if ( crcInput[0] && crcInput[1] )
			{
				CRC32_t crc = CRC32_ProcessSingleBuffer( crcInput, sizeof( crcInput ) );
				Q_snprintf( s_szPseudoUniqueID, ARRAYSIZE( s_szPseudoUniqueID ), "H:%x", ( int ) crc );
			}
		}
	}

	PossibleMapChange();

	m_flPauseStartTime = 0.0f;
	m_flLevelStartTime = gpGlobals->realtime;

	gamestats->Event_LevelInit();

#ifdef GAME_DLL
	if ( gamestats->UseOldFormat() )
	{
		if( gamestats->AutoSave_OnLevelInit() )
			gamestats->SaveToFileNOW();

		if( gamestats->AutoUpload_OnLevelInit() )
			gamestats->UploadStatsFileNOW();
	}
#endif
}


void CBaseGameStats_Driver::LevelShutdownPreEntity()
{
	float flElapsed = gpGlobals->realtime - m_flLevelStartTime;

	if ( flElapsed < 0.0f )
	{
		Assert( 0 );
		Warning( "EVENT_LEVELSHUTDOWN:  with negative elapsed time (rt %f starttime %f)\n", gpGlobals->realtime, m_flLevelStartTime );
		flElapsed = 0.0f;
	}

	//Assert( m_bInLevel ); //so, apparently shutdowns can happen before inits

#ifdef GAME_DLL
	if ( m_bInLevel && ( gpGlobals->eLoadType != MapLoad_Background ) )
#else
	if ( m_bInLevel )
#endif
	{
		m_flTotalTimeInLevels += flElapsed;
		m_iNumLevels ++;

		gamestats->Event_LevelShutdown( flElapsed );

		if ( gamestats->UseOldFormat() )
		{
#ifdef GAME_DLL
			if( gamestats->AutoSave_OnLevelShutdown() )
				gamestats->SaveToFileNOW( true );

			if( gamestats->AutoUpload_OnLevelShutdown() )
				gamestats->UploadStatsFileNOW();
#endif
		}
		else
		{
			// code path for new format game stats
			CollectData( STATSEND_LEVELSHUTDOWN );
			if ( gamestats->ShouldSendDataOnLevelShutdown() )
			{
				SendData();
			}	
		}
		m_bInLevel = false;	
	}
}

void CBaseGameStats_Driver::OnSave()
{
	gamestats->Event_SaveGame();
}


void CBaseGameStats_Driver::CollectData( StatSendType_t sendType )
{
	MEM_ALLOC_CREDIT();

	// The base stat system holds memory until "APPSHUTDOWN" which on the Xbox isn't good.
	// When on the Xbox, neither server nor clients collect stats, but the base system still does.
#ifdef _X360
	return;
#endif

	CGamestatsData *pGamestatsData = NULL;
#ifdef GAME_DLL
	// for server, check with the engine to see if there already a gamestats data container registered.  (There will be if there is a client
	// running in the same process.)
	pGamestatsData = engine->GetGamestatsData();
	if ( pGamestatsData )
	{
		// use the registered gamestats container, so free the one we allocated
		if ( m_pGamestatsData != NULL )
		{
			delete m_pGamestatsData;
			m_pGamestatsData = NULL;
		}
	}
	else
	{
		pGamestatsData = m_pGamestatsData;
	}
#else
	pGamestatsData = m_pGamestatsData;
#endif
	Assert( pGamestatsData || //valid stats
			(!gpGlobals->IsClient() && (sendType == STATSEND_APPSHUTDOWN) && gamestats->ShouldSendDataOnLevelShutdown()) ); //or already sent off stats during level shutdown (now in app shutdown)
    
    // If a server is told to send stats on STATSEND_APPSHUTDOWN, we need to check to make sure we still have a valid object.
	if ( !pGamestatsData )
		return;

	KeyValues *pKV = pGamestatsData->m_pKVData;

	int iAppID = engine->GetAppID();
	pKV->SetInt( "appid", iAppID );

	switch ( sendType )
	{
	case STATSEND_LEVELSHUTDOWN:
		{
			// make a map node in the KeyValues to use for this level
			char szMap[MAX_PATH+1]="";
#ifdef CLIENT_DLL	
			Q_FileBase( engine->GetLevelName(), szMap, ARRAYSIZE( szMap ) );
#else
			Q_strncpy( szMap, gpGlobals->mapname.ToCStr(), ARRAYSIZE( szMap ) );
#endif // CLIENT_DLL
			if ( !szMap[0] )
			{
				Q_FileBase( MapName(), szMap, ARRAYSIZE( szMap ) );
			}

			KeyValues *pKVMap = new KeyValues( "map" );
			pKV->AddSubKey( pKVMap );
			pKVMap->SetString( "mapname", szMap );
			pKV = pKVMap;


		}
		break;
	case STATSEND_APPSHUTDOWN:
		break;
	default:
		Assert( false );
		break;
	}

	// add common data
	pGamestatsData->m_bHaveData |= AddBaseDataForSend( pKV, sendType );
    
#if defined(CLIENT_DLL) && !defined(NO_STEAM)
#if !defined( _GAMECONSOLE )
	// At the end of every map, clients submit their perfdata for the map
	if ( sendType == STATSEND_LEVELSHUTDOWN && pGamestatsData && pGamestatsData->m_bHaveData )
	{
		// dgoodenough - Remove this for now, since this fails on PS3
		// PS3_BUILDFIX
		GetSteamWorksGameStatsClient().AddClientPerfData( pGamestatsData->m_pKVData );
	}
	// Time to reset ClientPerfData so we're ready for future recording. We clear the data
	// whether we submitted or not so that perf data has a clear meaning.
	ResetData();
    // The ResetData call realloced m_pGamestatsData. Need to point pGamestatsData to the new m_pGamestatsData !
    pGamestatsData = m_pGamestatsData;
#endif
	if ( sendType == STATSEND_LEVELSHUTDOWN )
	{
#if !defined( _GAMECONSOLE )
		KeyValues *pKVFileStats = new KeyValues( "FileSystemStats" );
		filesystem->GetVPKFileStatisticsKV( pKVFileStats );
		GetSteamWorksGameStatsClient().AddVPKLoadStats( pKVFileStats );
		GetSteamWorksGameStatsClient().AddVPKFileLoadErrorData( pKVFileStats );
		pKVFileStats->deleteThis();
		
		// On Server Disconnect, session is ended later than when a match restarts. This allows ClientPerfData and VPK stats to report properly
		// Alternately you could place ClientPerfData and VPK in the cs_game_disconnected event, but this event may fire more frequently than we want for CPD/VPK.
		GetSteamWorksGameStatsClient().EndSession();
		GetSteamWorksGameStatsClient().ResetServerState();
#endif
	}
#endif
	// add game-specific data
	pGamestatsData->m_bHaveData |= gamestats->AddDataForSend( pKV, sendType );
}
#ifndef CLIENT_DLL
ConVar gamestats_file_output_directory( "gamestats_file_output_directory", "", FCVAR_NONE, "When -gamestatsfileoutputonly is specified, file will be emitted here instead of to modpath\n" );
#endif
void CBaseGameStats_Driver::SendData()
{
	// if we don't own the data container or there's no valid data, nothing to do
	if ( !m_pGamestatsData || !m_pGamestatsData->m_bHaveData )
		return;

	// save the data to a buffer
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	m_pGamestatsData->m_pKVData->RecursiveSaveToFile( buf, 0 );

#ifdef CLIENT_DLL
	const bool bOutDirOverriden = false;
	const char *szOverrideDirectory = NULL;
#else
	const bool bOutDirOverriden = gamestats_file_output_directory.GetRawValue().m_StringLength > 1;
	const char *szOverrideDirectory = gamestats_file_output_directory.GetString();
#endif
	if ( CommandLine()->FindParm( "-gamestatsfileoutputonly" ) || bOutDirOverriden )
	{
		// write file for debugging
		if ( !bOutDirOverriden ) // if there isn't an override path, put it in the game dir.
		{
			const char szFileName[] = "gamestats.dat";
			filesystem->WriteFile( szFileName, GAMESTATS_PATHID, buf );
		}
		else
		{
			char szPathName[MAX_PATH] = {0};
			V_strncat( szPathName, szOverrideDirectory, MAX_PATH );
			V_AppendSlash( szPathName, MAX_PATH );
			int pathLen = V_strlen( szPathName );
			
			char szFileName[MAX_PATH] = {0};
			struct tm timeinfo;
			Plat_GetLocalTime( &timeinfo );
			int fileLen = V_snprintf( szFileName, MAX_PATH, "%s__%2d_%3d_%2d_%2d_%2d.txt", s_szPseudoUniqueID, 
				timeinfo.tm_year % 100, timeinfo.tm_yday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec );

			Assert( pathLen + fileLen < MAX_PATH );	fileLen;

			V_strncat( szPathName + pathLen, szFileName, MAX_PATH - pathLen );

			filesystem->WriteFile( szPathName, NULL, buf );
		}
	}
	else
	{
		// upload the file to Steam
		if ( gamestatsuploader )
			gamestatsuploader->UploadGameStats( "", 1, buf.TellPut(), buf.Base() );
	}	

	ResetData();
}

bool CBaseGameStats_Driver::AddBaseDataForSend( KeyValues *pKV, StatSendType_t sendType )
{
	switch ( sendType )
	{
	case STATSEND_APPSHUTDOWN:
#ifdef CLIENT_DLL
		if ( m_iNumLevels > 0 )
		{
			// add playtime data
			KeyValues *pKVData = new KeyValues( "playtime" );
			pKVData->SetInt( "TotalLevelTime", m_flTotalTimeInLevels );
			pKVData->SetInt( "NumLevels", m_iNumLevels );
			pKV->AddSubKey( pKVData );
			return true;
		}
#endif
		break;
	case STATSEND_LEVELSHUTDOWN:
#ifdef CLIENT_DLL
		const int MIN_PERF_STAT_COLLECT_TIME = 60 * 5;	// send perf data if we have collected at least this many seconds
		bool bSend = m_bBufferFull || m_nWriteIndex >= MIN_PERF_STAT_COLLECT_TIME;
		if ( bSend )
		{
			// add perf data
			KeyValues *pKVPerf = new KeyValues( "perfdata" );

			SortFramerate();

			float flAverageFrameRate = AverageStat( &StatsBufferRecord_t::m_flFrameRate );
			float flMinFrameRate =	PercentileFramerate( 20 ); 
			float flMaxFrameRate =	PercentileFramerate( 80 ); 
			float flStandardDeviationFrameRate = StdDevStat( &StatsBufferRecord_t::m_flFrameRate, flAverageFrameRate );

			pKVPerf->SetFloat( "AvgFPS", flAverageFrameRate );
			pKVPerf->SetFloat( "MinFPS", flMinFrameRate );
			pKVPerf->SetFloat( "MaxFPS", flMaxFrameRate );
			pKVPerf->SetFloat( "StdDevFPS", flStandardDeviationFrameRate );
			// if encoded histogram is 0, it just means we have no samples, so we don't really need to report it (save on bandwidth)
			if ( uint64 nHist = m_FpsHistogram.Encode() )
			{
				pKVPerf->SetUint64( "FrameHistAll", nHist );
			}
			if ( uint64 nHistSmoke = m_FpsHistogramGame[ PERF_STATS_SMOKE ].Encode() )
			{
				pKVPerf->SetUint64( "FrameHistGame1", nHistSmoke );
			}
			if ( uint64 nHistBullet = m_FpsHistogramGame[ PERF_STATS_BULLET ].Encode() )
			{
				pKVPerf->SetUint64( "FrameHistGame2", nHistBullet );
			}
			if ( uint64 nHistPlayer = m_FpsHistogramGame[ PERF_STATS_PLAYER ].Encode() )
			{
				pKVPerf->SetUint64( "FrameHistGame3", nHistPlayer );
			}
			if ( uint64 nHistPlayerSpawn = m_FpsHistogramGame[ PERF_STATS_PLAYER_SPAWN ].Encode() )
			{
				pKVPerf->SetUint64( "FrameHistGame4", nHistPlayerSpawn );
			}
			m_FpsHistogram.Reset();
			m_FpsHistogramGame[ PERF_STATS_SMOKE ].Reset();
			m_FpsHistogramGame[ PERF_STATS_BULLET ].Reset();
			m_FpsHistogramGame[ PERF_STATS_PLAYER ].Reset();
			m_FpsHistogramGame[ PERF_STATS_PLAYER_SPAWN ].Reset();

			// Determine the min/avg/max Server Ping and store the results 
			float flAverageServerPing = AverageStat( &StatsBufferRecord_t::m_flServerPing );
			pKVPerf->SetFloat( "AvgServerPing", flAverageServerPing );

			// Avg/StdDev main thread time
			float flAvgMainThreadTime = AverageStat( &StatsBufferRecord_t::m_flMainThreadTimeMS );
			float flStdDevMainThreadTime = StdDevStat( &StatsBufferRecord_t::m_flMainThreadTimeMS, flAvgMainThreadTime );
			pKVPerf->SetFloat( "AvgMainThreadTime", flAvgMainThreadTime );
			pKVPerf->SetFloat( "StdDevMainThreadTime", flStdDevMainThreadTime );

			// Avg/StdDev main thread wait time (time waiting for the render thread)
			float flAvgMainThreadWaitTime = AverageStat( &StatsBufferRecord_t::m_flMainThreadWaitTimeMS );
			float flStdDevMainThreadWaitTime = StdDevStat( &StatsBufferRecord_t::m_flMainThreadWaitTimeMS, flAvgMainThreadWaitTime );
			pKVPerf->SetFloat( "AvgMainThreadWaitTime", flAvgMainThreadWaitTime );
			pKVPerf->SetFloat( "StdDevMainThreadWaitTime", flStdDevMainThreadWaitTime );

			// Avg/StdDev render thread time
			float flAvgRenderThreadTime = AverageStat( &StatsBufferRecord_t::m_flRenderThreadTimeMS );
			float flStdDevRenderThreadTime = StdDevStat( &StatsBufferRecord_t::m_flRenderThreadTimeMS, flAvgRenderThreadTime );
			pKVPerf->SetFloat( "AvgRenderThreadTime", flAvgRenderThreadTime );
			pKVPerf->SetFloat( "StdDevRenderThreadTime", flStdDevRenderThreadTime );

			// Avg/StdDev render thread wait time (time waiting for the gpu, ForceHardwareSync)
			float flAvgRenderThreadWaitTime = AverageStat( &StatsBufferRecord_t::m_flRenderThreadWaitTimeMS );
			float flStdDevRenderThreadWaitTime = StdDevStat( &StatsBufferRecord_t::m_flRenderThreadWaitTimeMS, flAvgRenderThreadWaitTime );
			pKVPerf->SetFloat( "AvgRenderThreadWaitTime", flAvgRenderThreadWaitTime );
			pKVPerf->SetFloat( "StdDevRenderThreadWaitTime", flStdDevRenderThreadWaitTime );

			int nCPUThrottlingSamples = m_nCPUUnthrottledCount + m_nCPUThrottle1Count + m_nCPUThrottle2Count;
			if ( nCPUThrottlingSamples )
			{
				float fThrottledToLevel1 = m_nCPUThrottle1Count * 100.0f / nCPUThrottlingSamples;
				float fThrottledToLevel2 = m_nCPUThrottle2Count * 100.0f / nCPUThrottlingSamples;
				pKVPerf->SetFloat( "PercentageCPUThrottledToLevel1", fThrottledToLevel1 );
				pKVPerf->SetFloat( "PercentageCPUThrottledToLevel2", fThrottledToLevel2 );
			}

			pKV->AddSubKey( pKVPerf );
		}

		// Only keeping track of using voice in a multiplayer game
		if ( g_pGameRules && g_pGameRules->IsMultiplayer() )
		{
			pKV->SetInt( "UsedVoice", m_bDidVoiceChat );
		}

		extern ConVar closecaption;
		pKV->SetInt( "Caption", closecaption.GetInt() );

		// Set language to "unknown", then check to see if we can obtain the language.
		char language[64] = "unknown";
		if ( engine )
		{
			engine->GetUILanguage( language, sizeof( language ) );
			pKV->SetInt( "BugSub", engine->GetBugSubmissionCount() );
			engine->ClearBugSubmissionCount();
		}
		pKV->SetString( "Language", language );

		if ( materials )
		{
			const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
			pKV->SetBool( "Windowed", config.Windowed() );
			pKV->SetBool( "WindowedNoBorder", config.NoWindowBorder() );
		}

		// We want to know if a user is running with cheats on so that we can filter out their data
		// depending on what we're looking for.
		if ( NULL != sv_cheats  )
		{
			pKV->SetInt( "Cheats", sv_cheats->GetInt() );			
		}

		
		int mapTime = gpGlobals->realtime - m_flLevelStartTime;
		pKV->SetInt( "MapTime", mapTime );

		// reset buffer for next level. This is also done by ResetData().
		m_bBufferFull = false;
		m_nWriteIndex = 0;
		// Reset last realtime timestamp so that we don't compute the fps 
		// the first time we re-enter UpdatePerfStats
		m_flLastRealTime	= -1.0f;
		m_flLastSampleTime	= -1.0f;

		return bSend;
#endif
		break;
	}

	return false;
}


void CBaseGameStats_Driver::ResetData()
{
#ifdef GAME_DLL
	// on the server, if there is a gamestats data container registered (by a client in the same process), they're in charge of resetting it, nothing for us to do
	if ( engine->GetGamestatsData() != NULL )
		return;
#endif

	MEM_ALLOC_CREDIT();

	if ( m_pGamestatsData != NULL )
	{
		delete m_pGamestatsData;
		m_pGamestatsData = NULL;
	}

	m_bBufferFull = false;
	m_nWriteIndex = 0;
	m_bDidVoiceChat = false;

	// Reset the CPU throttling statistics.
	m_nCPUUnthrottledCount = 0;
	m_nCPUThrottle1Count = 0;
	m_nCPUThrottle2Count = 0;
	// Track when we last received data.
	m_lastCPUFrequencyTimestamp = 0.0;

	m_pGamestatsData = new CGamestatsData();
	KeyValues *pKV = m_pGamestatsData->m_pKVData;
	pKV->SetInt( "IsPc", IsPC() );
	pKV->SetInt( "version", GAMESTATS_VERSION );
	pKV->SetString( "srcid", s_szPseudoUniqueID );

#ifdef CLIENT_DLL
	const CPUInformation &cpu = GetCPUInformation();
	OverWriteCharsWeHate( cpu.m_szProcessorID );
	pKV->SetString( "CPUID", cpu.m_szProcessorID );
	pKV->SetFloat( "CPUGhz", cpu.m_Speed * ( 1.0 / 1.0e9 ) );
	pKV->SetUint64( "CPUModel", cpu.m_nModel );
	pKV->SetUint64( "CPUFeatures0", cpu.m_nFeatures[ 0 ] );
	pKV->SetUint64( "CPUFeatures1", cpu.m_nFeatures[ 1 ] );
	pKV->SetUint64( "CPUFeatures2", cpu.m_nFeatures[ 2 ] );
	pKV->SetInt( "NumCores", cpu.m_nPhysicalProcessors );

	MaterialAdapterInfo_t gpu;
	materials->GetDisplayAdapterInfo( materials->GetCurrentAdapter(), gpu );

	CMatRenderContextPtr pRenderContext( materials );
	int dest_width,dest_height;
	pRenderContext->GetRenderTargetDimensions( dest_width, dest_height );

	if ( gpu.m_pDriverName )
	{
		OverWriteCharsWeHate( gpu.m_pDriverName );
	}
	pKV->SetString( "GPUDrv", SafeString( gpu.m_pDriverName ) );
	pKV->SetInt( "GPUVendor", gpu.m_VendorID );
	pKV->SetInt( "GPUDeviceID", gpu.m_DeviceID );
	pKV->SetString( "GPUDriverVersion", CFmtStr( "%d.%d", gpu.m_nDriverVersionHigh, gpu.m_nDriverVersionLow ) );
	pKV->SetInt( "DxLvl", g_pMaterialSystemHardwareConfig->GetDXSupportLevel() );
	pKV->SetInt( "Width", dest_width );
	pKV->SetInt( "Height", dest_height );
	pKV->SetInt( "IsSplitScreen", VGui_IsSplitScreen() );
	
	engine->SetGamestatsData( m_pGamestatsData );

#endif
}

void CBaseGameStats_Driver::OnRestore()
{
	PossibleMapChange();

	gamestats->Event_LoadGame();
}


void CBaseGameStats_Driver::FrameUpdatePostEntityThink()
{
	bool bGamePaused = ( gpGlobals->frametime == 0.0f );

	if ( !m_bInLevel )
	{
		m_flPauseStartTime = 0.0f;
	}
	else if ( m_bGamePaused != bGamePaused )
	{
		if ( bGamePaused )
		{
			m_flPauseStartTime = gpGlobals->realtime;
		}
		else if ( m_flPauseStartTime != 0.0f )
		{
			float flPausedTime = gpGlobals->realtime - m_flPauseStartTime;
			if ( flPausedTime < 0.0f )
			{
				Assert( 0 );
				Warning( "Game paused time showing up negative (rt %f pausestart %f)\n", gpGlobals->realtime, m_flPauseStartTime );
				flPausedTime = 0.0f;
			}

			// Remove this from global time counters

			//			Msg( "Pause:  adding %f to level starttime\n", flPausedTime );

			m_flLevelStartTime += flPausedTime;
			m_flPauseStartTime = 0.0f;

			//			Msg( "Paused for %.2f seconds\n", flPausedTime );
		}
		m_bGamePaused = bGamePaused;
	}
}

bool StatsTrackingIsFullyEnabled( void )
{
	return CBGSDriver.m_bEnabled && gamestats->StatTrackingEnabledForMod();
}

void CBaseGameStats::Clear( void )
{
#ifdef GAME_DLL
	gamestats->m_BasicStats.Clear();
#endif
}

//-----------------------------------------------------------------------------
// Nukes any dangerous characters and replaces w/space char
//-----------------------------------------------------------------------------
void OverWriteCharsWeHate( char *pStr )
{
	while( *pStr )
	{
		switch( *pStr )
		{
		case '\n':
		case '\r':
		case '\\':
		case '\"':
		case '\'':
		case '\032':
		case ';':
			*pStr = ' ';
		}
		pStr++;
	}
}

#ifdef GAME_DLL

void CBaseGameStats::SetSteamStatistic( bool bUsingSteam )
{
	if( CBGSDriver.m_bFirstLevel )
	{
		m_BasicStats.m_Summary.m_bSteam = bUsingSteam;
	}

	if( CBGSDriver.m_bInLevel )
	{
		BasicGameStatsRecord_t *map = m_BasicStats.FindOrAddRecordForMap( CBGSDriver.m_PrevMapName.String() );
		map->m_bSteam = bUsingSteam;
	}

	m_BasicStats.m_bSteam = bUsingSteam;
}

void CBaseGameStats::SetCyberCafeStatistic( bool bIsCyberCafeUser )
{
	if( CBGSDriver.m_bFirstLevel )
	{
		m_BasicStats.m_Summary.m_bCyberCafe = bIsCyberCafeUser;
	}

	if( CBGSDriver.m_bInLevel )
	{
		BasicGameStatsRecord_t *map = m_BasicStats.FindOrAddRecordForMap( CBGSDriver.m_PrevMapName.String() );
		map->m_bCyberCafe = bIsCyberCafeUser;
	}

	m_BasicStats.m_bCyberCafe = bIsCyberCafeUser;
}

void CBaseGameStats::SetHDRStatistic( bool bHDREnabled )
{
	if( bHDREnabled )
	{
		if( CBGSDriver.m_bInLevel )
		{
			BasicGameStatsRecord_t *map = m_BasicStats.FindOrAddRecordForMap( CBGSDriver.m_PrevMapName.String() );
			++map->m_nHDR;
		}

		if( CBGSDriver.m_bFirstLevel )
		{
			++m_BasicStats.m_Summary.m_nHDR;
		}
	}
}

void CBaseGameStats::SetCaptionsStatistic( bool bClosedCaptionsEnabled )
{
	if( CBGSDriver.m_bInLevel )
	{
		BasicGameStatsRecord_t *map = m_BasicStats.FindOrAddRecordForMap( CBGSDriver.m_PrevMapName.String() );
		++map->m_nCaptions;
	}

	if( CBGSDriver.m_bFirstLevel )
	{
		++m_BasicStats.m_Summary.m_nCaptions;
	}
}

void CBaseGameStats::SetSkillStatistic( int iSkillSetting )
{
	int skill = clamp( iSkillSetting, 1, 3 ) - 1;

	if( CBGSDriver.m_bInLevel )
	{
		BasicGameStatsRecord_t *map = m_BasicStats.FindOrAddRecordForMap( CBGSDriver.m_PrevMapName.String() );
		++map->m_nSkill[ skill ];
	}

	if ( CBGSDriver. m_bFirstLevel )
	{
		++m_BasicStats.m_Summary.m_nSkill[ skill ];
	}
}

void CBaseGameStats::SetDXLevelStatistic( int iDXLevel )
{
	m_BasicStats.m_nDXLevel = iDXLevel;
}

static void CC_ResetGameStats( const CCommand &args )
{
#if defined ( TF_DLL ) || defined ( TF_CLIENT_DLL )
	// Disabled this until we fix the TF gamestat crashes that result
	return;
#endif

	if ( !UTIL_IsCommandIssuedByServerAdmin() )
		return;

	gamestats->Clear();
	gamestats->SaveToFileNOW();
	gamestats->StatsLog( "CC_ResetGameStats : Server cleared game stats\n" );
}

static ConCommand resetGameStats("_resetgamestats", CC_ResetGameStats, "Erases current game stats and writes out a blank stats file", 0 );

class CPointGamestatsCounter : public CPointEntity
{
public:
	DECLARE_CLASS( CPointGamestatsCounter, CPointEntity );
	DECLARE_DATADESC();

	CPointGamestatsCounter();

protected:

	void InputSetName( inputdata_t &inputdata );
	void InputIncrement( inputdata_t &inputdata );

	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );
private:

	string_t		m_strStatisticName;
	bool			m_bDisabled;
};

BEGIN_DATADESC( CPointGamestatsCounter )

	DEFINE_KEYFIELD( m_strStatisticName, FIELD_STRING, "Name" ),
	DEFINE_FIELD( m_bDisabled, FIELD_BOOLEAN ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_STRING, "SetName", InputSetName ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "Increment", InputIncrement ),

	DEFINE_INPUTFUNC( FIELD_VOID, "Enable", InputEnable ),
	DEFINE_INPUTFUNC( FIELD_VOID, "Disable", InputDisable ),

END_DATADESC()

LINK_ENTITY_TO_CLASS( point_gamestats_counter, CPointGamestatsCounter )


CPointGamestatsCounter::CPointGamestatsCounter() :
	m_strStatisticName( NULL_STRING ),
	m_bDisabled( false )
{
}

//-----------------------------------------------------------------------------
// Purpose: Changes name of statistic
//-----------------------------------------------------------------------------
void CPointGamestatsCounter::InputSetName( inputdata_t &inputdata )
{
	m_strStatisticName = inputdata.value.StringID();
}

//-----------------------------------------------------------------------------
// Purpose: Changes name of statistic
//-----------------------------------------------------------------------------
void CPointGamestatsCounter::InputIncrement( inputdata_t &inputdata )
{
	if ( m_bDisabled )
		return;

	if ( NULL_STRING == m_strStatisticName )
	{
		DevMsg( 1, "CPointGamestatsCounter::InputIncrement:  No stat name specified for point_gamestats_counter @%f, %f, %f [ent index %d]\n",
			GetAbsOrigin().x, GetAbsOrigin().y, GetAbsOrigin().z, entindex() );
		return;
	}

	gamestats->Event_IncrementCountedStatistic( GetAbsOrigin(), STRING( m_strStatisticName ), inputdata.value.Float() );
}

void CPointGamestatsCounter::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

void CPointGamestatsCounter::InputDisable( inputdata_t &inputdata )
{
	m_bDisabled = true;
}

#endif // GAME_DLL

