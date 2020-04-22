//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef GAMESTATS_H
#define GAMESTATS_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utldict.h"
#include "tier1/utlbuffer.h"
#include "igamesystem.h"

const int GAMESTATS_VERSION = 1;

enum StatSendType_t
{
	STATSEND_LEVELSHUTDOWN,
	STATSEND_APPSHUTDOWN
};

struct StatsBufferRecord_t
{
	float m_flFrameRate;									// fps
	float m_flServerPing;									// client ping to server
	float m_flMainThreadTimeMS;								// time in ms taken by the main thread
	float m_flMainThreadWaitTimeMS;							// time in ms the main thread is waiting for the render thread (cf EndFrame)
	float m_flRenderThreadTimeMS;							// time in ms taken by the render thread
	float m_flRenderThreadWaitTimeMS;						// time in ms waiting for the gpu (time spent in ForceHardwareSync)

	bool operator< ( const StatsBufferRecord_t &a) const
	{
		return m_flFrameRate < a.m_flFrameRate;
	}
};



class CFpsHistory
{
public:
	enum FpsEnum_t 
	{
		FPS_120,
		FPS_90,
		FPS_60,
		FPS_45,
		FPS_30,
		FPS_20,
		FPS_10,
		FPS_LOW,
		FPS_BIN_COUNT
	};

	enum HistoryBufSizeEnum_t { HISTORY_BUF_SIZE = 32 };
	struct Rec_t
	{
		float m_flFps;
		float m_flCpuWait;
	};

	CFpsHistory()
	{
		V_memset( this, 0, sizeof( *this ) );
	}

	uint8 Update( float flFps, float flCpuWait );

	uint8 operator []( int i ) const 
	{
		return m_History[ ( m_nNext + HISTORY_BUF_SIZE - 1 - i ) % HISTORY_BUF_SIZE ];
	}


	int m_nNext;
	uint8 m_History[ HISTORY_BUF_SIZE ];

};


class CFpsHistogram
{
public:
	enum FpsEnum_t
	{
		FPS_BIN_COUNT = CFpsHistory::FPS_BIN_COUNT
	};

	CFpsHistogram()
	{
		Reset();
	}
	void Reset()
	{
		V_memset( this, 0, sizeof( *this ) );
	}
	void Update( uint8 nBin )
	{
		m_nFps[ nBin & ( FPS_BIN_COUNT - 1 ) ]++;
		m_nCpuWaits[ nBin & ( FPS_BIN_COUNT - 1 ) ] += ( nBin >> 7 );
	}
	int64 Encode()const;
protected:
	uint32 m_nFps[ FPS_BIN_COUNT ];
	uint32 m_nCpuWaits[ FPS_BIN_COUNT ];
};


class CFpsSelectiveHistogram: public CFpsHistogram
{
public:
	CFpsSelectiveHistogram()
	{
		m_nLookAhead = 0;
		m_nLookBack = 0;
	}

	void Reset()
	{
		V_memset( this, 0, sizeof( *this ) );
	}

	void Fire( int nLookAhead, int nLookBack, const CFpsHistory &history )
	{
		// get the prerecorded stats
		if ( m_nLookAhead < nLookAhead )
		{
			m_nLookAhead = nLookAhead;
		}
		int nCatchUp = MIN( nLookBack, m_nLookBack );
		// exhaust back-stats buffer
		for ( int i = 0; i < nCatchUp; ++i )
		{
			CFpsHistogram::Update( history[ i ] );
		}

		m_nLookBack = 0;
	}

	void Update( uint8 nBin )
	{
		if ( m_nLookAhead > 0 )
		{
			m_nLookAhead--;
			CFpsHistogram::Update( nBin );
		}
		else
		{
			++m_nLookBack;
		}
	}
	int GetInactiveCount() const { return m_nLookBack; }
	void ResetInactiveCount() { m_nLookBack = 0; }

protected:
	int m_nLookAhead; // this is how many more frames we need to collect statistics (stay active)
	int m_nLookBack; // this is how many frames we skipped (did not collect statistics)
};


enum PerfStatsEventEnum_t
{
	PERF_STATS_SMOKE,
	PERF_STATS_BULLET,
	PERF_STATS_PLAYER,
	PERF_STATS_PLAYER_SPAWN,
	PERF_STATS_EVENT_COUNT
};

#define STATS_WINDOW_SIZE ( 60 * 10 )						// # of records to hold
#define STATS_TRAILING_WINDOW_SIZE ( 30 )					// # of records to hold in trailing window prior to stat recording
#define STATS_RECORD_INTERVAL 1								// # of seconds between data grabs. 2 * 300 = every 10 minutes

class CGameStats;

void UpdatePerfStats( void );
void FirePerfStatsEvent( PerfStatsEventEnum_t nEvent, int nLookAhead = CFpsHistory::HISTORY_BUF_SIZE, int nLookBack = CFpsHistory::HISTORY_BUF_SIZE );
void SetGameStatsHandler( CGameStats *pGameStats );

class CBasePlayer;
class CPropVehicleDriveable;
class CTakeDamageInfo;

#ifdef GAME_DLL

#define GAMESTATS_STANDARD_NOT_SAVED 0xFEEDBEEF

enum GameStatsVersions_t
{
	GAMESTATS_FILE_VERSION_OLD = 001,
	GAMESTATS_FILE_VERSION_OLD2,
	GAMESTATS_FILE_VERSION_OLD3,
	GAMESTATS_FILE_VERSION_OLD4,
	GAMESTATS_FILE_VERSION_OLD5,
	GAMESTATS_FILE_VERSION
};

struct BasicGameStatsRecord_t
{
public:
	BasicGameStatsRecord_t() :
	  m_nCount( 0 ),
		  m_nSeconds( 0 ),
		  m_nCommentary( 0 ),
		  m_nHDR( 0 ),
		  m_nCaptions( 0 ),
		  m_bSteam( true ),
		  m_bCyberCafe( false ),
		  m_nDeaths( 0 )
	  {
		  Q_memset( m_nSkill, 0, sizeof( m_nSkill ) );
	  }

	  void		Clear();

	  void		SaveToBuffer( CUtlBuffer& buf );
	  bool		ParseFromBuffer( CUtlBuffer& buf, int iBufferStatsVersion );

	  // Data
public:
	int			m_nCount;
	int			m_nSeconds;

	int			m_nCommentary;
	int			m_nHDR;
	int			m_nCaptions;
	int			m_nSkill[ 3 ];
	bool		m_bSteam;
	bool		m_bCyberCafe;
	int			m_nDeaths;
};

struct BasicGameStats_t
{
public:
	BasicGameStats_t() :
		  m_nSecondsToCompleteGame( 0 ),
		  m_bSteam( true ),
		  m_bCyberCafe( false ),
		  m_nDXLevel( 0 )
	  {
	  }

	  void						Clear();

	  void						SaveToBuffer( CUtlBuffer& buf );
	  bool						ParseFromBuffer( CUtlBuffer& buf, int iBufferStatsVersion );

	  BasicGameStatsRecord_t	*FindOrAddRecordForMap( char const *mapname );

	  // Data
public:
	int							m_nSecondsToCompleteGame; // 0 means they haven't finished playing yet

	BasicGameStatsRecord_t		m_Summary;			// Summary record
	CUtlDict< BasicGameStatsRecord_t, unsigned short > m_MapTotals;
	bool						m_bSteam;
	bool						m_bCyberCafe;
	int							m_nDXLevel;
};
#endif // GAME_DLL

class CBaseGameStats 
{
public:
	CBaseGameStats();

	// override this to declare what format you want to send.  New products should use new format.
	virtual bool UseOldFormat() 
	{ 
#ifdef GAME_DLL
		return true;		// servers by default send old format for backward compat
#else
		return false;		// clients never used old format so no backward compat issues, they use new format by default
#endif
	}

	// Implement this if you support new format gamestats.
	// Return true if you added data to KeyValues, false if you have no data to report
	virtual bool AddDataForSend( KeyValues *pKV, StatSendType_t sendType ) { return false; }

	// These methods used for new format gamestats only and control when data gets sent.
	virtual bool ShouldSendDataOnLevelShutdown()
	{
		// by default, servers send data at every level change and clients don't
#ifdef GAME_DLL
		return true;
#else
		return false;
#endif
	}
	virtual bool ShouldSendDataOnAppShutdown()
	{
		// by default, clients send data at app shutdown and servers don't
#ifdef GAME_DLL
		return false;
#else
		return true;
#endif
	}

	virtual void Event_Init( void );
	virtual void Event_Shutdown( void );
	virtual void Event_MapChange( const char *szOldMapName, const char *szNewMapName );
	virtual void Event_LevelInit( void );
	virtual void Event_LevelShutdown( float flElapsed );
	virtual void Event_SaveGame( void );
	virtual void Event_LoadGame( void );

	void StatsLog( PRINTF_FORMAT_STRING char const *fmt, ... );
	
	// This is the first call made, so that we can "subclass" the CBaseGameStats based on gamedir as needed (e.g., ep2 vs. episodic)
	virtual CBaseGameStats *OnInit( CBaseGameStats *pCurrentGameStats, char const *gamedir ) { return pCurrentGameStats; }

	// Frees up data from gamestats and resets it to a clean state.
	virtual void Clear( void );

	virtual bool StatTrackingEnabledForMod( void ) { return false; } //Override this to turn on the system. Stat tracking is disabled by default and will always be disabled at the user's request
	static bool StatTrackingAllowed( void ); //query whether stat tracking is possible and warranted by the user
	virtual bool HaveValidData( void ) { return true; } // whether we currently have an interesting enough data set to upload.  Called at upload time; if false, data is not uploaded.

	virtual bool ShouldTrackStandardStats( void ) { return true; } //exactly what was tracked for EP1 release
	
	//Get mod specific strings used for tracking, defaults should work fine for most cases
	virtual const char *GetStatSaveFileName( void );
	virtual const char *GetStatUploadRegistryKeyName( void );
	const char *GetUserPseudoUniqueID( void );

	virtual bool UserPlayedAllTheMaps( void ) { return false; } //be sure to override this to determine user completion time

#ifdef GAME_DLL
	virtual void Event_PlayerKilled( CBasePlayer *pPlayer, const CTakeDamageInfo &info );	
	virtual void Event_PlayerConnected( CBasePlayer *pBasePlayer );
	virtual void Event_PlayerDisconnected( CBasePlayer *pBasePlayer );
	virtual void Event_PlayerDamage( CBasePlayer *pBasePlayer, const CTakeDamageInfo &info );
	virtual void Event_PlayerKilledOther( CBasePlayer *pAttacker, CBaseEntity *pVictim, const CTakeDamageInfo &info );
	virtual void Event_Credits();
	virtual void Event_Commentary();
	virtual void Event_CrateSmashed();
	virtual void Event_Punted( CBaseEntity *pObject );
	virtual void Event_PlayerTraveled( CBasePlayer *pBasePlayer, float distanceInInches, bool bInVehicle, bool bSprinting );
	virtual void Event_WeaponFired( CBasePlayer *pShooter, bool bPrimary, char const *pchWeaponName );
	virtual void Event_WeaponHit( CBasePlayer *pShooter, bool bPrimary, char const *pchWeaponName, const CTakeDamageInfo &info );
	virtual void Event_FlippedVehicle( CBasePlayer *pDriver, CPropVehicleDriveable *pVehicle );
	virtual void Event_PreSaveGameLoaded( char const *pSaveName, bool bInGame );
	virtual void Event_PlayerEnteredGodMode( CBasePlayer *pBasePlayer );
	virtual void Event_PlayerEnteredNoClip( CBasePlayer *pBasePlayer );
	virtual void Event_DecrementPlayerEnteredNoClip( CBasePlayer *pBasePlayer );
	virtual void Event_IncrementCountedStatistic( const Vector& vecAbsOrigin, char const *pchStatisticName, float flIncrementAmount );	
	virtual void Event_WindowShattered( CBasePlayer *pPlayer );

	//custom data to tack onto existing stats if you're not doing a complete overhaul
	virtual void AppendCustomDataToSaveBuffer( CUtlBuffer &SaveBuffer ) { } //custom data you want thrown into the default save and upload path
	virtual void LoadCustomDataFromBuffer( CUtlBuffer &LoadBuffer ) { }; //when loading the saved stats file, this will point to where you started saving data to the save buffer

	virtual void LoadingEvent_PlayerIDDifferentThanLoadedStats( void ); //Only called if you use the base SaveToFileNOW() and LoadFromFile() functions. Used in case you want to keep/invalidate data that was just loaded. 

	virtual bool LoadFromFile( void ); //called just before Event_Init()
	virtual bool SaveToFileNOW( bool bForceSyncWrite = false ); //saves buffers to their respective files now, returns success or failure
	virtual bool UploadStatsFileNOW( void ); //uploads data to the CSER now, returns success or failure

	static bool AppendLump( int nMaxLumpCount, CUtlBuffer &SaveBuffer, unsigned short iLump, unsigned short iLumpCount, size_t nSize, void *pData );
	static bool GetLumpHeader( int nMaxLumpCount, CUtlBuffer &LoadBuffer, unsigned short &iLump, unsigned short &iLumpCount, bool bPermissive = false );
	static void LoadLump( CUtlBuffer &LoadBuffer, unsigned short iLumpCount, size_t nSize, void *pData );

	//default save behavior is to save on level shutdown, and game shutdown
	virtual bool AutoSave_OnInit( void ) { return false; }
	virtual bool AutoSave_OnShutdown( void ) { return true; }
	virtual bool AutoSave_OnMapChange( void ) { return false; }
	virtual bool AutoSave_OnLevelInit( void ) { return false; }
	virtual bool AutoSave_OnLevelShutdown( void ) { return true; }

	//default upload behavior is to upload on game shutdown
	virtual bool AutoUpload_OnInit( void ) { return false; }
	virtual bool AutoUpload_OnShutdown( void ) { return true; }
	virtual bool AutoUpload_OnMapChange( void ) { return false; }
	virtual bool AutoUpload_OnLevelInit( void ) { return false; }
	virtual bool AutoUpload_OnLevelShutdown( void ) { return false; }

	// Helper for builtin stuff
	void SetSteamStatistic( bool bUsingSteam );
	void SetCyberCafeStatistic( bool bIsCyberCafeUser );
	void SetHDRStatistic( bool bHDREnabled );
	void SetCaptionsStatistic( bool bClosedCaptionsEnabled );
	void SetSkillStatistic( int iSkillSetting );
	void SetDXLevelStatistic( int iDXLevel );
#endif // GAMEDLL
public:
#ifdef GAME_DLL
	BasicGameStats_t m_BasicStats; //exposed in case you do a complete overhaul and still want to save it
#endif
	bool			m_bLogging : 1;
	bool			m_bLoggingToFile : 1;
};

#ifdef GAME_DLL

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &SaveBuffer - 
//			iLump - 
//			iLumpCount - 
//-----------------------------------------------------------------------------
inline bool CBaseGameStats::AppendLump( int nMaxLumpCount, CUtlBuffer &SaveBuffer, unsigned short iLump, unsigned short iLumpCount, size_t nSize, void *pData )
{
	// Verify the lump index.
	Assert( ( iLump > 0 ) && ( iLump < nMaxLumpCount ) );

	if ( !( ( iLump > 0 ) && ( iLump < nMaxLumpCount ) ) )
		return false;

	// Check to see if we have any elements to save.
	if ( iLumpCount <= 0 )
		return false;

	// Write the lump id and element count.
	SaveBuffer.PutUnsignedShort( iLump );
	SaveBuffer.PutUnsignedShort( iLumpCount );

	size_t nTotalSize = iLumpCount * nSize;
	SaveBuffer.Put( pData, nTotalSize );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &LoadBuffer - 
//			&iLump - 
//			&iLumpCount - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
inline bool CBaseGameStats::GetLumpHeader( int nMaxLumpCount, CUtlBuffer &LoadBuffer, unsigned short &iLump, unsigned short &iLumpCount, bool bPermissive /*= false*/ )
{
	// Get the lump id and element count.
	iLump = LoadBuffer.GetUnsignedShort();
	if ( !LoadBuffer.IsValid() )
	{
		// check for EOF
		return false;
	}
	iLumpCount = LoadBuffer.GetUnsignedShort();

	if ( bPermissive )
		return true;

	// Verify the lump index.
	Assert( ( iLump > 0 ) && ( iLump < nMaxLumpCount ) );
	if ( !( ( iLump > 0 ) && ( iLump < nMaxLumpCount ) ) )
	{
		return false;
	}

	// Check to see if we have any elements to save.
	if ( iLumpCount <= 0 )
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Loads 1 or more lumps of raw data
// Input  : &LoadBuffer - buffer to be read from
//			iLumpCount - # of lumps to read
//			nSize - size of each lump
//			pData - where to store the data
//-----------------------------------------------------------------------------
inline void CBaseGameStats::LoadLump( CUtlBuffer &LoadBuffer, unsigned short iLumpCount, size_t nSize, void *pData )
{
	LoadBuffer.Get( pData, iLumpCount * nSize );
}

#endif // GAME_DLL

extern CBaseGameStats *gamestats; //starts out pointing at a singleton of the class above, overriding this in any constructor should work for replacing it

#endif // GAMESTATS_H
