// memlog.cpp : mines memory data from vxconsole logs (see game/client/c_memorylog.cpp)

#include "utlbuffer.h"

#include <conio.h>
#include <math.h>
#include <iostream>
#include <tchar.h>
#include <assert.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>

#define _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
#include <hash_map>
#include <algorithm>

#include "windows.h"

using namespace std;
using namespace stdext;

// We write this to output files on conversion, and test it on read to see if files need re-converting:
static const int MEMLOG_VERSION = 4;

enum DVDHosted_t
{
	// Is the game running FULLY off the DVD? (i.e. it reads no data from the HDD - no texture streaming)
	// If so, it will use more memory for audio/anim/texture data.
	DVDHOSTED_UNKNOWN = 0,
	DVDHOSTED_YES,
	DVDHOSTED_NO
};

enum Platform_t
{
	PLATFORM_UNKNOWN	= 0,
	PLATFORM_360		= 1,
	PLATFORM_PS3		= 2,
	PLATFORM_PC			= 3
};
const char *gPlatformNames[] = { "unknown", "360", "PS3", "PC" };

enum TrackStat_t
{
	TRACKSTAT_UNKNOWN = 0,
	TRACKSTAT_MINFREE_CPU = 1,
	TRACKSTAT_MINFREE_GPU = 2
};

// TODO: these will eventually vary by platform (certainly, these numbers are not useful on PC)
#define CPU_MEM_SIZE 512
#define GPU_MEM_SIZE 256

static const int MAX_LANG_LEN = 64;
struct CHeaderInfo
{
	CHeaderInfo() : memlogVersion( 0 ), dvdHosted( DVDHOSTED_UNKNOWN ), buildNumber(-1), platform( PLATFORM_UNKNOWN ) { language[0] = 0; }
	int         memlogVersion;
	DVDHosted_t dvdHosted;
	int         buildNumber;
	Platform_t  platform;
	char        language[MAX_LANG_LEN];
};

// Bitfield type used to filter logs w.r.t subsets of maps/players
class FilterBitfield
{
public:
	FilterBitfield( void ) { for ( int i = 0; i < NUM_INT64S; i++ ) bits[i] = 0; }

	void SetAll( void )								{	for ( int i = 0; i < NUM_INT64S; i++ ) bits[i] = -1;	}
	void Set( int bit )								{	bits[bit/64] |= ((__int64)1) << (bit&63);				}
	bool IsSet( int bit )							{	return !!( bits[bit/64] & ((__int64)1) << (bit&63) );	}
	bool Intersects( const FilterBitfield &other )	{	__int64 result = 0; for ( int i = 0; i < NUM_INT64S; i++ ) result |= ( bits[i] & other.bits[i] ); return !!result;	}
	bool operator ! ( void ) const					{	__int64 result = 0; for ( int i = 0; i < NUM_INT64S; i++ ) result |= bits[i]; return !result;	}

	static const int NUM_INT64S = 4;
	__int64 bits[NUM_INT64S];
};

class CStringList
{
public:
	static const int MAX_STRINGLIST_ENTRIES = 64*FilterBitfield::NUM_INT64S;
	~CStringList( void )
	{
		for ( int i = 0; i < m_Strings.Count(); i++ ) delete m_Strings[ i ];
	}
	int AddString( const char *string )
	{
		int existingIndex = StringToInt( string );
		if ( existingIndex != -1 )
			return existingIndex;
		if ( m_Strings.Count() == MAX_STRINGLIST_ENTRIES )
		{
			Assert( 0 );
			printf( "----ERROR: more than %d strings added to stringlist (e.g. '%s'), have to increase the size of FilterBitfield.bits!\n\n", MAX_STRINGLIST_ENTRIES, m_Strings[0] );
			return -1;
		}
		return m_Strings.AddToTail( strdup( string ) );
	}
	int StringToInt( const char *string )
	{
		// TODO: make this search faster (BUT we still need to be able to index into m_Strings and FilterBitfields... storing CUtlSymbolTable indices in m_String could work)
		for ( int i = 0; i < m_Strings.Count(); i++ )
		{
			if ( !stricmp( string, m_Strings[ i ] ) )
				return i;
		}
		return -1;
	}
	FilterBitfield SubstringToBitfield( const char *subString )
	{
		FilterBitfield bitField;
		if ( !subString || !subString[0] )
		{
			bitField.SetAll();
		}
		else
		{
			for ( int i = 0; i < m_Strings.Count(); i++ )
			{
				if ( V_stristr( m_Strings[ i ], subString ) )
					bitField.Set( i );
			}
		}
		return bitField;
	}
private:
	CUtlVector< const char *>m_Strings;
};

struct MapMin_t // See CLogFile::badMaps
{
	int		map;	// Map number
	float	minMem;	// Minimum memory observed in this map
};

struct CItem
{
	int   time;
	float freeMem;
	float gpuFree;
	int   globalMapIndex;
	int   logMapIndex;
	int   numBots;
	int   numPlayers;
	int   numLocalPlayers;
	int   numSpecators;
	bool  isServer;
	FilterBitfield playerBitfield; // Bits represent items in CLogFile::playerList
};

struct CLogFile
{
public:
	CLogFile( void ) {}
	CUtlVector<CItem>	entries;
	string				memoryLog;		// Path of the source memorylog file
	string				consoleLog;		// Path of the source vxconsole file
	ULONGLONG			modifyTime;		// Last modification time of the source vxconsole file
	bool				isActive;		// Last time we checked, the vxconsole file was still being written
	float				minFreeMem;		// Minimum free memory in any entry in this log
	float				minGPUFreeMem;	// Minimum free GPU memory in any entry in this log
	bool				isServer;		// True if the local box is the Server for any entry in this log
	bool				isSplitscreen;	// True if the local box is running splitscreen (>= 2 local players) for any entry in this log
	int					maxPlayers;		// Maximum players in any entry in this log
	CStringList			mapList;		// The maps present in this log
	CStringList			playerList;		// The players present in this log
	float				memorylogTick;	// Seconds between entries in this log
	CHeaderInfo			headerInfo;		// Various global data about the log (stuff that can't be computed from 'entries')
	CUtlVector<MapMin_t>badMaps;		// A temp list that accumulates maps in this log which pass the current filters
private:
	CLogFile( const CLogFile &other ) {}
};

struct CLogStats
{
	CLogStats( void )
	{
		for ( int i = 0; i < CStringList::MAX_STRINGLIST_ENTRIES; i++ )
		{
			mapMin[     i ] = CPU_MEM_SIZE;
			mapGPUMin[  i ] = GPU_MEM_SIZE;
			mapAverage[ i ] = 0.0f;
			mapSeconds[ i ] = 0;
			mapSamples[ i ] = 0;
		}
	}
	float        mapMin[     CStringList::MAX_STRINGLIST_ENTRIES ];
	float        mapGPUMin[  CStringList::MAX_STRINGLIST_ENTRIES ];
	float        mapAverage[ CStringList::MAX_STRINGLIST_ENTRIES ];
	float		 mapSeconds[ CStringList::MAX_STRINGLIST_ENTRIES ];
	unsigned int mapSamples[ CStringList::MAX_STRINGLIST_ENTRIES ];
};

static const char *gKnownMaps[] = {	"none",
									/*"credits", */

									/* L4D1
									"l4d_hospital01_apartment",
									"l4d_hospital02_subway",
									"l4d_hospital03_sewers",
									"l4d_hospital04_interior",
									"l4d_hospital05_rooftop",

									"l4d_airport01_greenhouse",
									"l4d_airport02_offices",
									"l4d_airport03_garage",
									"l4d_airport04_terminal",
									"l4d_airport05_runway",

									"l4d_farm01_hilltop",
									"l4d_farm02_traintunnel",
									"l4d_farm03_bridge",
									"l4d_farm04_barn",
									"l4d_farm05_cornfield",

									"l4d_smalltown01_caves",
									"l4d_smalltown02_drainage",
									"l4d_smalltown03_ranchhouse",
									"l4d_smalltown04_mainstreet",
									"l4d_smalltown05_houseboat",

									"l4d_vs_hospital01_apartment",
									"l4d_vs_hospital02_subway",
									"l4d_vs_hospital03_sewers",
									"l4d_vs_hospital04_interior",
									"l4d_vs_hospital05_rooftop",

									"l4d_vs_airport01_greenhouse",
									"l4d_vs_airport02_offices",
									"l4d_vs_airport03_garage",
									"l4d_vs_airport04_terminal",
									"l4d_vs_airport05_runway",

									"l4d_vs_farm01_hilltop",
									"l4d_vs_farm02_traintunnel",
									"l4d_vs_farm03_bridge",
									"l4d_vs_farm04_barn",
									"l4d_vs_farm05_cornfield",

									"l4d_vs_smalltown01_caves",
									"l4d_vs_smalltown02_drainage",
									"l4d_vs_smalltown03_ranchhouse",
									"l4d_vs_smalltown04_mainstreet",
									"l4d_vs_smalltown05_houseboat",

									"backgroundstreet", */
									
									/* L4D2
									"c1m1_hotel",
									"c1m2_streets",
									"c1m3_mall",
									"c1m4_atrium",

									"c2m1_highway",
									"c2m2_fairgrounds",
									"c2m3_coaster",
									"c2m4_barns",
									"c2m5_concert",

									"c3m1_plankcountry",
									"c3m2_swamp",
									"c3m3_shantytown",
									"c3m4_plantation",

									"c4m1_milltown_a",
									"c4m2_sugarmill_a",
									"c4m3_sugarmill_b",
									"c4m4_milltown_b",
									"c4m5_milltown_escape",

									"c5m1_waterfront",
									"c5m2_park",
									"c5m3_cemetery",
									"c5m4_quarter",
									"c5m5_bridge", */

									/* // Portal 2 SP
									"sp_a1_intro1",
									"sp_a1_intro2",
									"sp_a1_intro3",
									"sp_a1_intro4",
									"sp_a1_intro5",
									"sp_a1_intro6",
									"sp_a1_intro7",
									"sp_a1_wakeup",
									"sp_a2_intro",
									"sp_a2_laser_intro",
									"sp_a2_laser_stairs",
									"sp_a2_dual_lasers",
									"sp_a2_laser_over_goo",
									"sp_a2_catapult_intro",
									"sp_a2_trust_fling",
									"sp_a2_pit_flings",
									"sp_a2_fizzler_intro",
									"sp_a2_sphere_peek",
									"sp_a2_ricochet",
									"sp_a2_bridge_intro",
									"sp_a2_bridge_the_gap",
									"sp_a2_turret_intro",
									"sp_a2_laser_relays",
									"sp_a2_turret_blocker",
									"sp_a2_laser_vs_turret",
									"sp_a2_pull_the_rug",
									"sp_a2_column_blocker",
									"sp_a2_laser_chaining",
									"sp_a2_turret_tower",
									"sp_a2_triple_laser",
									"sp_a2_bts1",
									"sp_a2_bts2",
									"sp_a2_bts3",
									"sp_a2_bts4",
									"sp_a2_bts5",
									"sp_a2_bts6",
									"sp_a2_core",
									"sp_a3_00",
									"sp_a3_01",
									"sp_a3_03",
									"sp_a3_jump_intro",
									"sp_a3_bomb_flings",
									"sp_a3_crazy_box",
									"sp_a3_transition01",
									"sp_a3_speed_ramp",
									"sp_a3_speed_flings",
									"sp_a3_portal_intro",
									"sp_a3_end",
									"sp_a4_intro",
									"sp_a4_tb_intro",
									"sp_a4_tb_trust_drop",
									"sp_a4_tb_wall_button",
									"sp_a4_tb_polarity",
									"sp_a4_tb_catch",
									"sp_a4_stop_the_box",
									"sp_a4_laser_catapult",
									"sp_a4_laser_platform",
									"sp_a4_speed_tb_catch",
									"sp_a4_jump_polarity",
									"sp_a4_finale1",
									"sp_a4_finale2",
									"sp_a4_finale3",
									"sp_a4_finale4",

									// Portal 2 Co-op
									"mp_coop_start",
									"mp_coop_lobby_2",
									"mp_coop_doors",
									"mp_coop_race_2",
									"mp_coop_laser_2",
									"mp_coop_rat_maze",
									"mp_coop_laser_crusher",
									"mp_coop_teambts",
									"mp_coop_fling_3",
									"mp_coop_infinifling_train",
									"mp_coop_come_along",
									"mp_coop_fling_1",
									"mp_coop_catapult_1",
									"mp_coop_multifling_1",
									"mp_coop_fling_crushers",
									"mp_coop_fan",
									"mp_coop_wall_intro",
									"mp_coop_wall_2",
									"mp_coop_catapult_wall_intro",
									"mp_coop_wall_block",
									"mp_coop_catapult_2",
									"mp_coop_turret_walls",
									"mp_coop_turret_ball",
									"mp_coop_wall_5",
									"mp_coop_tbeam_redirect",
									"mp_coop_tbeam_drill",
									"mp_coop_tbeam_catch_grind_1",
									"mp_coop_tbeam_laser_1",
									"mp_coop_tbeam_polarity",
									"mp_coop_tbeam_polarity2",
									"mp_coop_tbeam_polarity3",
									"mp_coop_tbeam_maze",
									"mp_coop_tbeam_end",
									"mp_coop_paint_come_along",
									"mp_coop_paint_redirect",
									"mp_coop_paint_bridge",
									"mp_coop_paint_walljumps",
									"mp_coop_paint_speed_fling",
									"mp_coop_paint_red_racer",
									"mp_coop_paint_speed_catch",
									"mp_coop_paint_longjump_intro",
									"mp_coop_separation_1",
									"mp_coop_rocket_block",
									"mp_coop_race_3",
									"mp_coop_laser_1",
									"mp_coop_wall_1",
									"mp_coop_2guns_longjump_intro",
									"mp_coop_paint_crazy_box",
									"mp_coop_wall_6",
									"mp_coop_button_tower",
									"mp_coop_tbeam_fling_float_1",
									
									// Portal 2 demo
									"demo_intro",
									"demo_paint",
									"demo_underground", */

									// CSGO
									"cs_italy",
									"cs_office",
									"de_aztec",
									"de_dust2",
									"de_dust",
									"de_inferno",
									"de_nuke",
									"de_lake",
									"de_safehouse",
									"de_shorttrain"
									"de_sugarcane",
									"de_stmarc",
									"de_bank",
									"ar_shoots",
									"ar_baggage",
									"de_train",
									"training1",
									 };
const int gNumKnownMaps = ARRAYSIZE( gKnownMaps );

static const char *gIgnoreMaps[] = {"devtest",

									/*"test_box",
									"test_box2",
									"nav_test",
									"transition_test01",
									"transition_test02",

									"c2m4_concert",
									"c5m2_cemetery",
									"c5m3_quarter",
									"c5m4_bridge"*/ };
const int gNumIgnoreMaps = ARRAYSIZE( gIgnoreMaps );

CUtlVector< const char * > gMapNames;	// List of encountered map names
typedef hash_map<string, int> CMapHash;	// For quickly determining is a string is in gMapNames
CMapHash gMapHash;

typedef hash_map<string, CLogFile*> CLogFiles;

const int FILTER_SIZE = 64;

struct Config
{
	char  sourcePath[ _MAX_PATH ];		// Where we're getting logs from
	char  prevCommandLine[ _MAX_PATH ];	// Previously entered command
	bool  recurse;						// Recurse the tree under 'sourcePath'
	bool  update;						// Re-convert vxconsole logs, if they're newer than their corresponding memorylogs
	bool  updateActive;					// Re-convert vxconsole logs, 
	bool  forceUpdate;					// Re-convert vxconsole logs, even if they've been converted before

	// Console mode:
	bool  consoleMode;					// Accept commands from the user 'till they quit
	bool  load;							// Load new logs from 'sourcePath'
	bool  unload;						// Unload all logs from 'sourcePath'
	bool  unloadAll;					// Unload all logs
	bool  quitting;						// We're done, exit the app
	bool  help;							// User wants to see the help text

	// Memory tracking (CSV files to plot memory stats over time)
	char  trackFile[MAX_PATH];			// Tracking file to update from the current filter set
	char  trackColumn[32];				// New column to add to the tracking file
	TrackStat_t trackStat;				// Stat to track (min free CPU memory, min free GPU memory...)

	// Data analysis filters (per-log-entry)
	float dangerLimit;					// Spew log entries in which memory drops below this limit (in MB)
	int   minPlayers;					// Spew log entries with at least this many concurrent players
	int   maxPlayers;					// Spew log entries with at most this many concurrent players
	bool  isSplitscreen;				// Spew log entries in which the machine has more than one local player
	bool  isSinglescreen;				// Spew log entries in which the machine has AT MOST one local player
	char  mapFilter[FILTER_SIZE];		// Spew log entries with map names which contain this substring
	char  playerFilter[FILTER_SIZE];	// Spew log entries with player names which contain this substring

	// Data analysis filters (per-log-file)
	int   dangerTime;					// Spew logs in which memory drops below the danger limit within this many seconds
	int   duration;						// Spew logs in which the timer reaches this many seconds
	int   minAge;						// Spew logs updated at least this many seconds ago
	int   maxAge;						// Spew logs updated at most this many seconds ago
	bool  isServer;						// Spew logs in which the machine is a listen server at least once
	bool  isClient;						// Spew logs in which the machine is NEVER a listen server
	bool  isActive;						// Spew logs currently being written
	DVDHosted_t dvdHosted;				// Spew logs matching this DVD hosted state (UNKNOWN means accept all)
	char  languageFilter[FILTER_SIZE];	// Spew logs with a language which contain this substring
	Platform_t platform;				// Spew logs generated from the specified platform (UNKNOWN means accept all)
};
Config gConfig;

// 10 million 100-nanosecond intervals per second in FILETIME
const ULONGLONG ONE_SECOND = 10000000LL;

int AddNewMapName( const char *name )
{
	char *nameCopy = strdup( name ); // Leak
	strlwr( nameCopy );
	if ( gMapHash.find( string( nameCopy ) ) != gMapHash.end() )
	{
		printf( "ERROR: duplicate map name in gMapNames!!!! (%s)\n", nameCopy );
		printf( "Aborting... press any key to exit\n" );
		DebuggerBreakIfDebugging();
		getchar();
		exit( 1 );
	}
	int index = gMapNames.AddToTail( nameCopy );
	gMapHash[ string( nameCopy ) ] = index;
	return index;
}

bool ParseLineForDVDHostedInfo( string &line, CHeaderInfo &headerInfo )
{
	// Game spews text like this on startup:
	//
	//		Xbox Launched From DVD.
	//		Install Status:
	//		Version: 746360 (english) (Xbox|PS3|PC)
	//		DVD Hosted: Enabled				<---- this is the important line
	//		Progress: 0/1200 MB
	//		Active Image: 746360
	//
	// We use that to determine if we are running SOLELY off the DVD (no HDD access, no texture streaming),
	// in which case textures/audio/anims take more memory, so the minimum free memory watermark is lower.
	// UPDATE: these memory effects apply to L4D/L4D2 but not PORTAL2 (it doesn't do texture streaming)

	if ( headerInfo.dvdHosted == DVDHOSTED_UNKNOWN )
	{
		const char *cursor = line.c_str();
		if ( strstr( cursor, "Device\\Harddisk" ) )
		{
			// Running from the HDD (this test works for old images without the "DVD Hosted:" spew)
			headerInfo.dvdHosted = DVDHOSTED_NO;
			return true;
		}
		else if ( strstr( cursor, "DVD Hosted:" ) )
		{
			headerInfo.dvdHosted = strstr( cursor, "Enabled" ) ? DVDHOSTED_YES : DVDHOSTED_NO;
			return true;
		}
	}
	return false;
}

bool ParseLineForBuildNumberLanguageAndPlatformInfo( string &line, CHeaderInfo &headerInfo )
{
	// Game spews text like this on startup:
	//
	//		Xbox Launched From DVD.
	//		Install Status:
	//		Version: 746360 (english) (Xbox|PS3|PC)				<---- this is the important line
	//		DVD Hosted: Enabled
	//		Progress: 0/1200 MB
	//		Active Image: 746360
	//
	// We use that to determine current build number and language.

	if ( headerInfo.buildNumber == -1 )
	{
		const char *cursor = line.c_str();
		cursor = strstr( cursor, "Version:" );
		if ( cursor )
		{
			static char language[1024], platform[1024];
			if ( 3 == sscanf( cursor, "Version: %d (%s (%s", &headerInfo.buildNumber, &(language[0]), &(platform[0]) ) )
			{
				strncpy( &(headerInfo.language[0]), &(language[0]), MAX_LANG_LEN );
				char *closingBrace = strstr( headerInfo.language, ")" );
				if ( closingBrace ) *closingBrace = 0;
				strlwr( &( headerInfo.language[0] ) );

				closingBrace = strstr( platform, ")" );
				if ( closingBrace ) *closingBrace = 0;
				if      ( !stricmp( platform, "Xbox" ) )
					headerInfo.platform = PLATFORM_360;
				else if ( !stricmp( platform, "PS3" ) )
					headerInfo.platform = PLATFORM_PS3;
				else if ( !stricmp( platform, "PC" ) )
					headerInfo.platform = PLATFORM_PC;

				return true;
			}
			headerInfo.buildNumber = -1;
			headerInfo.language[0] = 0;
			headerInfo.platform = PLATFORM_UNKNOWN;
		}
	}
	return false;
}

bool ParseLineForHeaderInfo( string &line, CHeaderInfo &headerInfo )
{
	if ( ParseLineForDVDHostedInfo( line, headerInfo ) )
		return true;

	if ( ParseLineForBuildNumberLanguageAndPlatformInfo( line, headerInfo ) )
		return true;

	return false;
}

int ConvertItem( string &line, CItem &result, const CItem &prevItem, CHeaderInfo *headerInfo, const char *fileName, int lineNum, vector<int> *truncated, CStringList *mapList, CStringList *playerList, bool skipIgnored )
{
	// Accumulate header lines as we go through the log
	if ( headerInfo && ParseLineForHeaderInfo( line, *headerInfo ) )
		return -1;

	// Special-case 'out of memory' lines:
	const char *start = strstr( line.c_str(), "*** OUT OF MEMORY!" );
	if ( start )
	{
		// Replace this with a fake "zero memory free" line
		static char fakeLine[1024];
		_snprintf( fakeLine, sizeof( fakeLine ), "[MEMORYLOG] Time:%6d | Free:%6.2f | GPU:%6.2f | %s | Map: %-32s | Bots:%2d | Players:  0",
				   prevItem.time, 0.0f, 0.0f, prevItem.isServer ? "Server" : "Client",
				   ( ( prevItem.globalMapIndex < 0 ) ? "none" : gMapNames[ prevItem.globalMapIndex ] ), prevItem.numBots );
		printf( "---- OUT OF MEMORY on line %d, in  %s\n\n", lineNum, fileName );
		line = fakeLine;
	}

	start = strstr( line.c_str(), "[MEMORYLOG] " );
	if ( !start )
		return -1;

	// Get rid of line endings returned by GetLine()
	while ( ( line[ line.size() - 1 ] == '\r' ) || ( line[ line.size() - 1 ] == '\n' ) )
		line.resize( line.size() - 1 );

	int time;
	float freeMem;
	float GPUFree;
	char hostType[33];
	char mapName[33];
	int numBots;
	int numPlayers;
	CMapHash::iterator it;
	int numRead = sscanf(	start,
							"[MEMORYLOG] Time:%d | Free: %f | GPU: %f | %s | Map: %s | Bots: %d | Players: %d",
							&time,
							&freeMem,
							&GPUFree,
							hostType,
							mapName,
							&numBots,
							&numPlayers );
	if ( numRead != 7 )
		goto error;

	bool isServer;
	if ( !stricmp( hostType, "Server" ) )
		isServer = true;
	else if ( !stricmp( hostType, "Client" ) )
		isServer = false;
	else
		goto error;

	int globalMapIndex = -1;
	strlwr( mapName );
	it = gMapHash.find( mapName );
	if ( it != gMapHash.end() )
	{
		globalMapIndex = it->second;
		if ( skipIgnored && ( globalMapIndex < gNumIgnoreMaps ) )
			return -1;
	}
	else
	{
		// Add the new name to gMapNames and gMapHash
		globalMapIndex = AddNewMapName( mapName );

		// This message notifies us when new maps appear on the horizon (e.g. 'credits'!)
		printf( "----WARNING: unrecognised map name '%s' on line %d, in  %s\n\n", mapName, lineNum, fileName );
	}

	// Also add the map name to a per-logfile list
	int logMapIndex = mapList ? mapList->AddString( mapName ) : -1;

	// Iterate over players
	int numLocalPlayers = 0, numSpectators = 0, numNamedBots = 0;
	const char *playerStart = start;
	for ( int i = 0; i < numPlayers; i++ )
	{
		playerStart = strstr( playerStart, ", " );
		if ( !playerStart )
			goto playerError;
		playerStart += 2;

		const char *playerEnd = playerStart;
		while ( playerEnd[0] && ( playerEnd[0] != '|' ) && ( playerEnd[0] != ',' ) ) playerEnd++;
		if ( playerList )
		{
			// Build a vector of player name references
			string playerName( playerStart, ( playerEnd - playerStart ) );
			strlwr( (char*)playerName.c_str() );
			int playerIndex = playerList->AddString( playerName.c_str() );
			result.playerBitfield.Set( playerIndex );
		}
		// Track how many local/spectator players there are
		while ( playerEnd[0] == '|' )
		{
			playerEnd++;
			if ( !strncmp( playerEnd, "LOCAL", 5 ) )
			{
				numLocalPlayers++;
				playerEnd += 5;
			}
			else if ( !strncmp( playerEnd, "SPEC", 4 ) )
			{
				numSpectators++;
				playerEnd += 4;
			}
			else if ( !strncmp( playerEnd, "BOT", 3 ) )
			{
				numNamedBots++;
				playerEnd += 3;
			}
			else
				goto playerError;
			if ( playerEnd[0] && ( playerEnd[0] != ',' ) && ( playerEnd[0] != '|' ) && ( playerEnd[0] != ' ' ) )
				goto playerError;
		}
	}
	Assert( !numNamedBots || ( numNamedBots == numBots ) );
	if ( numNamedBots && ( numNamedBots != numBots ) )
		goto playerError;

	if ( false )
	{
playerError:
		// Don't quit on errors in the player list, keep the data we managed to get (usually truncated lines in versus games)
		if ( strlen( line.c_str() ) == 264 )
		{
			// TODO: Somewhere, lines are getting capped at 264 chars (don't think it's in VXConsole, it's max command len is 4096)
			if ( truncated ) truncated->push_back( lineNum );
		}
		else
		{
			printf( "----ERROR: unrecognised [MEMORYLOG] syntax on line %d, in  %s\n", lineNum, fileName );
			printf( "    \"%s\"\n\n", line.c_str() );
		}
	}

	result.freeMem			= freeMem;
	result.gpuFree			= GPUFree;
	result.time				= time;
	result.globalMapIndex	= globalMapIndex;
	result.logMapIndex		= logMapIndex;
	result.numBots			= numBots;
	result.numPlayers		= numPlayers;
	result.numLocalPlayers	= numLocalPlayers;
	result.numSpecators		= numSpectators;
	result.isServer			= isServer;

	return ( start - line.c_str() );

error:
	printf( "----ERROR: unrecognised [MEMORYLOG] syntax on line %d, in  %s\n", lineNum, fileName );
	printf( "    \"%s\"\n\n", line.c_str() );
	return -1;
}

bool ReadFile( const string &fileName, CUtlBuffer &buffer )
{
	FILE *fp = fopen( fileName.c_str(), "rb" );
	if (!fp)
		return false;

	fseek( fp, 0, SEEK_END );
	int nFileLength = ftell( fp );
	fseek( fp, 0, SEEK_SET );

	buffer.EnsureCapacity( nFileLength );
	int nBytesRead = fread( buffer.Base(), 1, nFileLength, fp );
	fclose( fp );

	buffer.SeekPut( CUtlBuffer::SEEK_HEAD, nBytesRead );

	return true;
}

bool WriteFile( const string &fileName, CUtlBuffer &buffer, CUtlBuffer *header )
{
	FILE *fp = fopen( fileName.c_str(), "wb" );
	if ( !fp )
		return false;
	if ( header )
		fwrite( header->Base(), 1, header->TellPut(), fp );
	fwrite( buffer.Base(), 1, buffer.TellPut(), fp );
	fclose( fp );
	return true;
}

char *SpewHeaderSummary( char *buffer, int bufferSize, const CHeaderInfo &headerInfo )
{
	const char *dvdHosted = ( headerInfo.dvdHosted == DVDHOSTED_UNKNOWN ) ? " - " : ( ( headerInfo.dvdHosted == DVDHOSTED_YES ) ? "DVD" : "HDD" );
	bool languageKnown = !!strcmp( headerInfo.language, "unknown" );
	_snprintf( buffer, bufferSize, "%7d %4s %10s ", headerInfo.buildNumber, dvdHosted, languageKnown ? headerInfo.language : "  --  " );
	return buffer;
}

void WriteHeaderInfoToBuffer( CUtlBuffer &buffer, CHeaderInfo &headerInfo )
{
	buffer.Clear();
	buffer.Printf( "[HDR]:version=%d\n",     MEMLOG_VERSION );
	buffer.Printf( "[HDR]:dvdhosted=%s\n", ( headerInfo.dvdHosted == DVDHOSTED_UNKNOWN ) ? "unknown" : ( ( headerInfo.dvdHosted == DVDHOSTED_YES ) ? "yes" : "no" ) );
	buffer.Printf( "[HDR]:build=%d\n",       headerInfo.buildNumber );
	buffer.Printf( "[HDR]:language=%s\n",  ( headerInfo.language && headerInfo.language[0] ) ? headerInfo.language : "unknown" );
	buffer.Printf( "[HDR]:platform=%s\n",    gPlatformNames[ headerInfo.platform ] );
}

void InitItem( CItem &item )
{
	item.time			 = 0;
	item.freeMem		 = CPU_MEM_SIZE;
	item.gpuFree		 = GPU_MEM_SIZE;
	item.globalMapIndex	 = gMapHash[ string( "backgroundstreet" ) ];
	item.logMapIndex	 = -1;
	item.numBots		 = 0;
	item.numPlayers		 = 0;
	item.numLocalPlayers = 0;
	item.numSpecators	 = 0;
	item.isServer		 = false;
}

bool ConvertVXConsoleLog( string &consoleLog, string &memoryLog )
{
	CUtlBuffer iBuffer(  0, 0, CUtlBuffer::TEXT_BUFFER|CUtlBuffer::CONTAINS_CRLF );
	CUtlBuffer oBuffer(  0, 0, CUtlBuffer::TEXT_BUFFER|CUtlBuffer::CONTAINS_CRLF );
	CUtlBuffer ohBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER|CUtlBuffer::CONTAINS_CRLF );

	if ( !ReadFile( consoleLog, iBuffer ) )
	{
		printf( "----ERROR: Failed to read file %s\n\n", consoleLog.c_str() );
		return false;
	}

	bool isEmpty = true;
	int lineNum = 1;
	vector<int> truncated;
	CHeaderInfo headerInfo;
	CItem prevItem;
	InitItem( prevItem );
	while ( iBuffer.IsValid() )
	{
		static char lineBuf[ 10000 ];
		iBuffer.GetLine( lineBuf, sizeof( lineBuf ) );
		// CUtlBuffer.GetLine() returns TWO lines for lines ending in '\r\n' :o/
		if ( lineBuf[0] == '\n' ) continue;
		if ( !lineBuf[0] && ( iBuffer.TellGet() != iBuffer.TellPut() ) )
		{
			// CUtlBuffer.GetLine() can fail if there are non-ASCII characters in the log. If that happens
			// (which it does - vxconsole bug?), we'll get a zero-length line, so unstick the buffer:
			iBuffer.GetChar();
			continue;
		}

		CItem item;
		string line = lineBuf;
		int start = ConvertItem( line, item, prevItem, &headerInfo, consoleLog.c_str(), lineNum, &truncated, NULL, NULL, false );
		if ( start >= 0 )
		{
			// If the line is valid, copy it to the output file
			oBuffer.Put( ( line.c_str() + start ), ( line.size() - start ) );
			oBuffer.Put( "\n", 1 );
			isEmpty = false;
			prevItem = item;
		}
		lineNum++;
	}

	// Write the headerInfo to a buffer
	WriteHeaderInfoToBuffer( ohBuffer, headerInfo );

	if ( truncated.size() )
	{
		printf( "----WARNING: [MEMORYLOG] text truncated in  %s\n", consoleLog.c_str() );
		printf( "----         Truncated lines: %d", truncated[0] );
		for ( unsigned int i = 1; i < truncated.size(); i++ ) printf( ", %d", truncated[i] );
		printf( "\n" );
	}

	// Write the output file, with zero or more items in it
	if ( !WriteFile( memoryLog, oBuffer, &ohBuffer ) )
	{
		printf( "----ERROR: Failed to write file %s\n\n", memoryLog.c_str() );
		return false;
	}
	
	return !isEmpty;
}

CLogFile &InitLogFile( CLogFiles &results, const string &memoryLog, const string &consoleLog, const ULONGLONG &modifyTime, bool isActive )
{
	CLogFile *result = new CLogFile();
	pair<CLogFiles::iterator,bool> insertion = results.insert( make_pair( memoryLog, result ) );
	if ( !insertion.second || ( insertion.first == results.end() ) )
	{
		printf( "----ERROR: hash_map insertion failed...?\n\n" );
		insertion.first = results.find( memoryLog );
		if ( insertion.first == results.end() )
			printf( "----ERROR: hash_map totally b0rked, gonna crash...\n\n" );
	}
	result->memoryLog		= memoryLog;
	result->consoleLog		= consoleLog;
	result->modifyTime		= modifyTime;
	result->isActive		= isActive;
	result->minFreeMem		= CPU_MEM_SIZE;
	result->minGPUFreeMem	= GPU_MEM_SIZE;
	result->isServer		= false;
	result->isSplitscreen	= false;
	result->maxPlayers		= 0;
	return *result;
}

bool StringToPlatformName( const char *name, Platform_t &platform )
{
	for ( int i = 0; i < ARRAYSIZE( gPlatformNames ); i++ )
	{
		if ( !strnicmp( name, gPlatformNames[ i ], strlen( gPlatformNames[ i ] ) ) )
		{
			platform = (Platform_t)i;
			return true;
		}
	}
	return false;
}

bool ReadHeaderLine( const char *lineBuf, CHeaderInfo &headerInfo )
{
	static const char HEADER_PREFIX[]		= "[HDR]:";
	static const int  HEADER_PREFIX_LEN		= sizeof( HEADER_PREFIX ) - 1;
	static const char KEY_VERSION[]			= "version=";
	static const int  KEY_VERSION_LEN		= sizeof( KEY_VERSION ) - 1;
	static const char KEY_DVD_HOSTED[]		= "dvdhosted=";
	static const int  KEY_DVD_HOSTED_LEN	= sizeof( KEY_DVD_HOSTED ) - 1;
	static const char KEY_BUILDNUM[]		= "build=";
	static const int  KEY_BUILDNUM_LEN		= sizeof( KEY_BUILDNUM ) - 1;
	static const char KEY_LANGUAGE[]		= "language=";
	static const int  KEY_LANGUAGE_LEN		= sizeof( KEY_LANGUAGE ) - 1;
	static const char KEY_PLATFORM[]		= "platform=";
	static const int  KEY_PLATFORM_LEN		= sizeof( KEY_PLATFORM ) - 1;
	const char *cursor = lineBuf;

	if ( strncmp( cursor, HEADER_PREFIX, HEADER_PREFIX_LEN ) )
		return false;

	cursor += HEADER_PREFIX_LEN;
	if ( !strncmp( cursor, KEY_VERSION, KEY_VERSION_LEN ) )
	{
		cursor += KEY_VERSION_LEN;
		headerInfo.memlogVersion = atoi( cursor );
		return true;
	}
	else if ( !strncmp( cursor, KEY_DVD_HOSTED, KEY_DVD_HOSTED_LEN ) )
	{
		cursor += KEY_DVD_HOSTED_LEN;
		if ( !strncmp( cursor, "yes", 3 ) )
		{
			headerInfo.dvdHosted = DVDHOSTED_YES;
		}
		else if ( !strncmp( cursor, "no", 2 ) )
		{
			headerInfo.dvdHosted = DVDHOSTED_NO;
		}
		return true;
	}
	else if ( !strncmp( cursor, KEY_BUILDNUM, KEY_BUILDNUM_LEN ) )
	{
		cursor += KEY_BUILDNUM_LEN;
		headerInfo.buildNumber = atoi( cursor );
		return true;
	}
	else if ( !strncmp( cursor, KEY_LANGUAGE, KEY_LANGUAGE_LEN ) )
	{
		cursor += KEY_LANGUAGE_LEN;
		strncpy( headerInfo.language, cursor, MAX_LANG_LEN );
		char *eol = strstr( headerInfo.language, "\n" );
		if ( eol ) *eol = 0;
		return true;
	}
	else if ( !strncmp( cursor, KEY_PLATFORM, KEY_PLATFORM_LEN ) )
	{
		cursor += KEY_PLATFORM_LEN;
		if ( StringToPlatformName( cursor, headerInfo.platform ) )
			return true;
	}

	printf( "----ERROR: Unrecognised header line: (%s)\n\n", lineBuf );

	return true;
}

int ReadMemoryLog( string &memoryLog, string &consoleLog, CLogFiles & results, const ULONGLONG &modifyTime, bool isActive, bool checkVersion )
{
	CUtlBuffer iBuffer( 0, 0, CUtlBuffer::TEXT_BUFFER|CUtlBuffer::CONTAINS_CRLF );

	if ( !ReadFile( memoryLog.c_str(), iBuffer ) )
	{
		printf( "----ERROR: Failed to open input file %s\n\n", memoryLog.c_str() );
		return 0;
	}

	CLogFile &result	= InitLogFile( results, memoryLog, consoleLog, modifyTime, isActive );
	bool bOrderError	= false;
	bool bReallyEmpty	= true;
	bool bDoneHeader	= false;
	int  lineNum		= 1;
	int  lastTime		= 0;
	CItem prevItem;
	InitItem( prevItem );
	while ( iBuffer.IsValid() )
	{
		// create+add an item
		static char lineBuf[ 10000 ];
		iBuffer.GetLine( lineBuf, sizeof( lineBuf ) );
		// CUtlBuffer.GetLine() returns TWO lines for lines ending in '\r\n' :o/
		if ( lineBuf[0] == '\n' ) continue;

		if ( !bDoneHeader )
		{
			// Read one or more header lines at the top of the memorylog file
			if ( ReadHeaderLine( lineBuf, result.headerInfo ) )
			{
				lineNum++;
				continue;
			}
			bDoneHeader = true;

			// Check file version
			if ( ( result.headerInfo.memlogVersion < MEMLOG_VERSION ) && checkVersion )
			{
				printf( "----Log is from an old version, re-converting...\n" );
				CLogFile *oldLog = results.find( memoryLog )->second;
				if ( oldLog )
				{
					delete oldLog;
					results.erase( memoryLog );
				}
				return -1;
			}
			else if ( result.headerInfo.memlogVersion > MEMLOG_VERSION )
			{
				printf( "----ERROR: memory log file is newer than this version of memlog.exe!\n\n" );
			}
		}

		CItem item;
		string line = lineBuf;
		if ( ConvertItem( line, item, prevItem, NULL, memoryLog.c_str(), lineNum, NULL, &result.mapList, &result.playerList, true ) >= 0 )
		{
			bReallyEmpty = false;
			if ( item.globalMapIndex >= 0 )
			{
				result.entries.AddToTail( item );
				prevItem = item;
			}
			bOrderError = bOrderError || ( item.time < lastTime );
			lastTime = item.time;
		}
		lineNum++;
	}

	// VXConsole may have concatenated logs from multiple runs - DO NOT WANT!
	if ( bOrderError )
		printf( "----ERROR: '[MEMORYLOG]' timestamps are out-of-order! Log file may contain multiple runs of the game...\n\n" );

	// It's ok to load logs with [MEMORYLOG] lines only for invalid maps, but
	// logs with no [MEMORYLOG] lines should be zero size and not loaded at all.
	// UPDATE: the header info might still be useful, so we load 'em anyway...
	if ( bReallyEmpty )
		printf( "----Empty memorylog file.\n" );

	// Compute a few aggregate stats in order to speed up log filtering:
	int   noneMap		= gMapHash[ string( "none" ) ];
	int   menuMap		= gMapHash[ string( "backgroundstreet" ) ];
	int   creditsMap	= gMapHash[ string( "credits" ) ];
	float memorylogTick	= 0;
	for ( int i = 0; i < result.entries.Count(); i++ )
	{
		CItem &item = result.entries[ i ];
		result.minFreeMem    = min( result.minFreeMem, item.freeMem );
		result.minGPUFreeMem = min( result.minGPUFreeMem, item.gpuFree );
		result.maxPlayers    = max( result.maxPlayers, item.numPlayers );
		if ( item.numLocalPlayers > 1 )
			result.isSplitscreen = true;
		if ( item.isServer && ( item.globalMapIndex != noneMap ) && ( item.globalMapIndex != menuMap ) && ( item.globalMapIndex != creditsMap ) )
			result.isServer = true;
		if ( i > 0 ) // Average the intervals between memorylog entries
			memorylogTick += ( item.time - result.entries[ i - 1 ].time );
	}
	if ( result.entries.Count() > 1 )
		memorylogTick /= ( result.entries.Count() - 1 );
	result.memorylogTick = memorylogTick;

	return 1;
}

ULONGLONG WriteTime( WIN32_FIND_DATA &fileData )
{
	return ( ((ULONGLONG)fileData.ftLastWriteTime.dwHighDateTime ) << 32 ) | (ULONGLONG)fileData.ftLastWriteTime.dwLowDateTime;
}

/*ULONGLONG CreateTime( WIN32_FIND_DATA &fileData )
{
	return ( ((ULONGLONG)fileData.ftCreationTime.dwHighDateTime ) << 32 ) | (ULONGLONG)fileData.ftCreationTime.dwLowDateTime;
}*/

ULONGLONG SystemTime( void )
{
	SYSTEMTIME	LocalSysTime;
	FILETIME	LocalFileTime;
	FILETIME	UTCFileTime;
	BOOL		success = TRUE;

	GetLocalTime( &LocalSysTime );
	success = SystemTimeToFileTime( &LocalSysTime, &LocalFileTime );
	success = ( LocalFileTimeToFileTime( &LocalFileTime, &UTCFileTime ) || success );

	if ( !success )
	{
		static int errCount = 0;
		if ( !errCount++ )
		{
			printf( "----ERROR: error generating system time... all logs will be considered 'active'\n\n" );
			DebuggerBreak();
		}
		return 0;
	}

	return ( ((ULONGLONG)UTCFileTime.dwHighDateTime ) << 32 ) | (ULONGLONG)UTCFileTime.dwLowDateTime;
}

bool IsActive( ULONGLONG lastWriteTime )
{
	return ( SystemTime() < ( lastWriteTime + 60*ONE_SECOND ) );
}

void Tick( void )
{
	// Let the user know we're still working (recursing through fileserver is sloooow)
	SYSTEMTIME systemTime;
	GetLocalTime( &systemTime );
	static int lastTime = -10;
	if ( ( ( systemTime.wSecond % 10 ) == 0 ) && ( systemTime.wSecond != lastTime ) )
	{
		printf( "%02d:%02d:%02d--------------------------------------------------------------------------------------------\n", systemTime.wHour, systemTime.wMinute, systemTime.wSecond );
		lastTime = systemTime.wSecond;
	}
}

void _ProcessLogFiles(	const char *path, CLogFiles &results,
						int &numLoaded, int &numUnloaded, int &numConverted, int &numUpdated, int &numActive )
{
	WIN32_FIND_DATA fileData;
	HANDLE handle;

	Tick();

	if ( gConfig.load )
	{
		// Convert 'vxconsole_*.log' files
		static char pathBuf[ _MAX_PATH ]; // No recursion in this loop
		_snprintf( pathBuf, sizeof( pathBuf ), "%svxconsole_*.log", path );
		handle = FindFirstFile( pathBuf, &fileData );
		if ( handle != INVALID_HANDLE_VALUE )
		{
			do
			{
				_snprintf( pathBuf, sizeof( pathBuf ) , "%s%s", path, fileData.cFileName );
				string consoleLog = pathBuf;

				// Does this 'vxconsole_*.log' have a corresponding 'memorylog_*.log'?
				static char prefix[] = "vxconsole_";
				char *suffix = fileData.cFileName + sizeof( prefix ) - 1;
				_snprintf( pathBuf, sizeof( pathBuf ) , "%smemorylog_%s", path, suffix );
				string memoryLog = pathBuf;
				WIN32_FIND_DATA fileData2;
				HANDLE handle2 = FindFirstFile( memoryLog.c_str(), &fileData2 );

				// Convert all logs if asked to do a forced update
				bool bNeedsConverting = gConfig.forceUpdate;
				bool isActive = ( handle2 != INVALID_HANDLE_VALUE ) && IsActive( WriteTime( fileData ) );
				if ( !bNeedsConverting )
				{
					if ( isActive )
					{
						// Only convert currently-active logs if that is specifically requested
						bNeedsConverting = gConfig.updateActive;
						if ( !gConfig.updateActive )
							printf( "--Skipping update of ACTIVE log %s\n", consoleLog.c_str() );
						numActive++;
					}
					else if ( handle2 == INVALID_HANDLE_VALUE )
					{
						// Convert logs we haven't converted before
						bNeedsConverting = true;
					}
					else if ( gConfig.update )
					{
						// We have been asked to reconvert logs that have been updated
						bNeedsConverting = WriteTime( fileData ) > WriteTime( fileData2 );
					}
				}

				if ( bNeedsConverting )
				{
					// Create/update the memory log
					bool bUpdating = ( handle2 != INVALID_HANDLE_VALUE );
					printf( "--%s %s\n", ( bUpdating ? "Updating" : "Converting" ), consoleLog.c_str() );
					if ( isActive )
						printf( "----Log is ACTIVE\n" );
					ConvertVXConsoleLog( consoleLog, memoryLog );
					numConverted += bUpdating ? 0 : 1;
					numUpdated   += bUpdating ? 1 : 0;
				}
				FindClose( handle2 );

				Tick();
			}
			while( FindNextFile( handle, &fileData ) );
			FindClose( handle );
		}
	}

	// Note that update/updateActive/forceUpdate imply 'load' (it's confusing otherwise)
	if ( gConfig.load || gConfig.unload )
	{
		// Read in 'memorylog_*.log' files
		static char pathBuf[ _MAX_PATH ]; // No recursion in this loop
		_snprintf( pathBuf, sizeof( pathBuf ), "%smemorylog_*.log", path );
		handle = FindFirstFile( pathBuf, &fileData );
		if ( handle != INVALID_HANDLE_VALUE )
		{
			do
			{
				// Compute full memorylog_*/vxconsole_* paths
				string memoryLog = path;
				memoryLog += fileData.cFileName;
				strlwr( (char *)memoryLog.c_str() );

				// Is this memory log already in memory?
				CLogFiles::iterator it = results.find( memoryLog );
				bool bLoaded = ( it != results.end() );

				// Re-load active logs if requested
				if ( bLoaded && it->second->isActive && gConfig.updateActive )
				{
					CLogFile *oldLog = it->second;
					if ( oldLog ) delete oldLog;
					results.erase( it );
					bLoaded = false;
				}

				if ( gConfig.load && !bLoaded )
				{
					// Get the timestamp for the corresponding vxconsole log (if there is one)
					char *suffix = fileData.cFileName + strlen( "memorylog_" );
					_snprintf( pathBuf, sizeof( pathBuf ) , "%svxconsole_%s", path, suffix );
					string consoleLog = pathBuf;
					strlwr( (char *)consoleLog.c_str() );
					WIN32_FIND_DATA fileData2;
					HANDLE handle2 = FindFirstFile( consoleLog.c_str(), &fileData2 );
					ULONGLONG timeStamp = ( handle2 != INVALID_HANDLE_VALUE ) ? WriteTime( fileData2 ) : WriteTime( fileData );
					bool      isActive  = ( handle2 != INVALID_HANDLE_VALUE ) ? IsActive( WriteTime( fileData2 ) ) : false;
					if ( handle2 == INVALID_HANDLE_VALUE ) consoleLog = "";
					FindClose( handle2 );

					// Load each memorylog file, and add it to the results
					printf( "--Loading %s\n", memoryLog.c_str() );
					if ( isActive )
						printf( "----Log is ACTIVE, %s\n", ( gConfig.updateActive | gConfig.forceUpdate ) ? "loaded up-to-date memorylog data" : "loading old memorylog data" );
					bool checkVersion = true, ignoreVersion = false;
					int retVal = ReadMemoryLog( memoryLog, consoleLog, results, timeStamp, isActive, checkVersion );
					if ( retVal == -1 )
					{
						// Memorylog is out of date, re-convert it
						numUpdated += ConvertVXConsoleLog( consoleLog, memoryLog ) ? 1 : 0;
						printf( "----Loading re-converted log\n", memoryLog.c_str() );
						retVal = ReadMemoryLog( memoryLog, consoleLog, results, timeStamp, isActive, ignoreVersion );
					}
					if ( retVal > 0 )
						numLoaded++;
				}
				else if ( gConfig.unload && bLoaded )
				{
					// Unload this memorylog file
					printf( "--Unloading %s\n", memoryLog.c_str() );
					results.erase( memoryLog );
					numUnloaded++;
				}

				Tick();
			}
			while( FindNextFile( handle, &fileData ) );
			FindClose( handle );
		}
	}

	if ( gConfig.recurse )
	{
		// Recurse to subdirectories
		char *searchPath = new char[ _MAX_PATH ]; // Avoid blowing the stack due to recursion
		_snprintf( searchPath, _MAX_PATH, "%s*.*", path );
		handle = FindFirstFile( searchPath, &fileData );
		if ( handle != INVALID_HANDLE_VALUE )
		{
			do
			{
				if ( ( fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) &&
					 ( fileData.cFileName[ 0 ] != '.' ) )
				{
					char *subDir = new char[ _MAX_PATH ]; // Avoid blowing the stack due to recursion
					_snprintf( subDir, _MAX_PATH, "%s%s\\", path, fileData.cFileName );
					_ProcessLogFiles( subDir, results, numLoaded, numUnloaded, numConverted, numUpdated, numActive );
					delete[] subDir;
				}
			}
			while( FindNextFile( handle, &fileData ) );
		}
		FindClose( handle );
		delete[] searchPath;
	}
}

void ProcessLogFiles( const char *path, CLogFiles &results )
{
	if ( gConfig.unloadAll )
	{
		CLogFiles::iterator it;
		for ( it = results.begin(); it != results.end(); it++ )
		{
			printf( "--Unloading  %s\n", it->first.c_str() );
			delete it->second;
		}
		printf( "\n" );
		printf( "%d logs unloaded\n", results.size() );
		printf( "\n" );
		results.clear();
		return;
	}

	if ( !path || !path[0] )
		return;

	if ( gConfig.load || gConfig.unload )
	{
		int numLoaded = 0, numUnloaded = 0, numConverted = 0, numUpdated = 0, numEmpty = 0, numActive = 0;
		_ProcessLogFiles( gConfig.sourcePath, results, numLoaded, numUnloaded, numConverted, numUpdated, numActive );
		for ( CLogFiles::iterator it = results.begin(); it != results.end(); it++ )
		{
			numEmpty += it->second->entries.Count() ? 0 : 1;
		}
		printf( "\n" );

		/* TODO:
		 * numLoaded is confusing... not sure what it means
		 * I updated a bunch of logs from fileserver and saw this:
		 * 	2 logs loaded
		 * 	26 new logs converted
		 * 	12 logs re-converted
		 * 	7 active logs (converted)
		 * 	(1435 empty log files)
		 * in light of that, I have no idea what "logs loaded" means...
		 * hmm, it seems to occur for empty logs!
		 * then I ran "r u a" a second time (without copying over any more files) and it found another empty log... and listed logs loaded as 1
		 * then I did it again and it loaded no logs...
		 * weird
		 */

		if ( numLoaded )	printf( "%d logs loaded\n", numLoaded );
		if ( numUnloaded )	printf( "%d logs unloaded\n", numUnloaded );
		if ( numConverted )	printf( "%d new logs converted\n", numConverted );
		if ( numUpdated )	printf( "%d logs re-converted\n", numUpdated );
		if ( numActive )	printf( "%d active logs %s\n", numActive, gConfig.updateActive ? "(converted)" : "(not converted)" );
		if ( numEmpty )		printf( "(%d empty log files)\n", numEmpty );
		printf( "\n" );
	}
}

int trim( string &s )
{
	// Remove whitespace from the head+tail of the string
	size_t oldlen = s.length(), head = 0, tail = 0;
	while( ( head < oldlen ) && V_isspace( s[head]          ) ) head++;
	while( ( tail < oldlen ) && V_isspace( s[oldlen-1-tail] ) ) tail++;
	size_t numRemoved = head + tail;
	if ( !numRemoved )
		return 0;
	s = s.substr( head, MAX(0,oldlen-numRemoved) ); // The 'MAX' fixes the case where the string is ALL whitespace :)
	return numRemoved;
}

struct TrackSort_t
{
	int line;
	float minFree;
};
int TrackSortFunc( const void *a, const void *b )
{
	TrackSort_t *A = (TrackSort_t *)a, *B = (TrackSort_t *)b;
	return ( A->minFree < B->minFree ) ? -1 : +1;
}

void NormalizeCSVRowLengths( vector< vector< string > > &lines )
{
	unsigned int nMaxLineLength = 0;
	for ( unsigned int i = 0; i < lines.size(); i++ )
	{
		nMaxLineLength = max( nMaxLineLength, lines[i].size() );
	}
	for ( unsigned int i = 0; i < lines.size(); i++ )
	{
		string empty = i ? "" : "unknown";
		vector<string> &line = lines[i];
		while ( line.size() < nMaxLineLength )
		{
			line.push_back( empty );
		}
	}
}

bool UpdateTrackFile(	const char *trackFile, const char *trackColumn,
						unsigned int *mapSamples, float *mapMin, float *mapGPUMin )
{
	// Make a backup before we start
	CUtlBuffer fileBuffer;
	string fileName = trackFile;
	if ( ReadFile( fileName, fileBuffer ) )
	{
		int nCharsToExtension = V_stristr( trackFile, ".csv" ) - trackFile;
		Assert( nCharsToExtension > 0 );
		string backupName = fileName.substr( 0, nCharsToExtension ) + "_bak.csv";
		if ( !WriteFile( backupName, fileBuffer, NULL ) )
		{
			printf( "ERROR: could not write backup tracking .CSV file (%s), aborting!\n\n", backupName.c_str() );
			return false;
		}
	}

	// Now read the .CSV into a vector of vectors of strings
	const string MAP_NAME_HEADER = "Map Name";
	unsigned int nMaxRowLen = 0;
	vector<vector<string>> lines;
	CMapHash mapsInCSV;
	ifstream file;
	file.open( trackFile );
	if ( file.is_open() )
	{
		while ( !file.eof() )
		{
			string lineString;
			getline( file, lineString );
			if ( !lineString.size() )
				break;

			// Each line is a vector of strings:
			string item;
			vector<string> line;
			stringstream lineStream( lineString );
			while ( !lineStream.eof() )
			{
				getline( lineStream, item, ',' );
				trim( item ); // avoid duplicates differing only by whitespace
				line.push_back( item );
			}

			nMaxRowLen = max( nMaxRowLen, line.size() );
			lines.push_back( line );
		}
		file.close();
	}

	// Init an empty .CSV file:
	if ( !nMaxRowLen || !lines.size() )
	{
		vector<string> headerLine;
		headerLine.push_back( MAP_NAME_HEADER );
		lines.clear();
		lines.push_back( headerLine );
	}

	// Validate the .CSV file
	for ( unsigned int i = 0; i < lines.size(); i++ )
	{
		string &firstItem = lines[ i ][ 0 ];
		if ( ( ( i == 0 ) && ( firstItem != MAP_NAME_HEADER ) ) || !firstItem.size() )
		{
			printf( "ERROR: first item on line %d in tracking .CSV file (%s) is invalid, aborting!\n\n", i+1, trackFile );
			return false;
		}

		// Make a note of all the maps found in the file
		if ( i > 0 )
		{
			if ( mapsInCSV.find( firstItem ) != mapsInCSV.end() ) 
			{
				printf( "ERROR: map '%s' occurs more than once in tracking .CSV file (%s), aborting!\n\n", firstItem.c_str(), trackFile );
				return false;
			}
			mapsInCSV[ firstItem ] = i;
		}
	}

	// Normalize row lengths
	NormalizeCSVRowLengths( lines );

	// Add a new column
	vector<string> &headerLine = lines[0];
	if ( trackColumn[0] )
	{
		headerLine.push_back( string( trackColumn ) );
	}
	else
	{
		ostringstream columnName;
		columnName << "column_";
		columnName << headerLine.size();
		headerLine.push_back( columnName.str() );
	}

	// Add data to the .CSV file
	bool trackGPUMem = ( gConfig.trackStat == TRACKSTAT_MINFREE_GPU );
	unsigned int numMaps = gMapNames.Count();
	for ( unsigned int i = gNumIgnoreMaps; i < numMaps; i++ )
	{
		string empty = "", mapName = gMapNames[ i ];
		if ( mapsInCSV.find( mapName ) == mapsInCSV.end() )
		{
			// This map is not present in the .CSV file, so add a new row
			vector<string> newLine;
			newLine.push_back( mapName );
			while ( newLine.size() < nMaxRowLen )
			{
				newLine.push_back( empty );
			}
			lines.push_back( newLine );
			mapsInCSV[ mapName ] = lines.size() - 1;
		}

		// Add the new data sample to the end of this line
		int nMapLine = mapsInCSV[ mapName ];
		vector<string> &line = lines[ nMapLine ];

		if ( mapSamples[ i ] )
		{
			// Add the worst case sample for this map
			ostringstream minFree;
			minFree << ( trackGPUMem ? mapGPUMin[ i ] : mapMin[ i ] );
			line.push_back( minFree.str() );
		}
		else
		{
			// Add an empty entry for this map
			// (it makes diffing/sorting/combining/comparing these CSVs easier if they all contain the same set of maps)
			line.push_back( empty );
		}
	}

	// Normalize line lengths again, in case some maps weren't updated:
	NormalizeCSVRowLengths( lines );

	// Sort the .CSV file so the maps with the 'worst' most recent data point are at the top
	// (maps missing recent data points are sorted to the top, 'older' maps being biased higher)
	vector< TrackSort_t > sortedLines;
	for ( unsigned int i = 1; i < lines.size(); i++ )
	{
		int memSize = trackGPUMem ? GPU_MEM_SIZE : CPU_MEM_SIZE;
		TrackSort_t trackSort = { i, memSize };
		vector<string> &line = lines[ i ];
		for ( unsigned int j = line.size()-1; j > 0; j-- )
		{
			if ( 1 == sscanf( line[j].c_str(), "%f", &trackSort.minFree ) )
			{
				trackSort.minFree -= memSize*(line.size()-(j+1)); // The older the data, the higher up the list it goes
				sortedLines.push_back( trackSort );
				break;
			}
			if ( j == 1 )
			{
				//printf( "ERROR: no valid datapoints for map '%s' in tracking .CSV file (%s)\n\n", line[0].c_str(), trackFile );
				sortedLines.push_back( trackSort );
			}
		}
	}
	qsort( &sortedLines[0], sortedLines.size(), sizeof(TrackSort_t), TrackSortFunc );

	// Write out the updated file
	ofstream output;
	output.open( trackFile );
	if ( !output.is_open() )
	{
		printf( "ERROR: could write to tracking .CSV file (%s), aborting!\n\n", trackFile );
		return false;
	}
	for ( unsigned int i = 0; i < lines.size(); i++ )
	{
		int nLine = ( i == 0 ) ? 0 : sortedLines[i-1].line;
		vector<string> &line = lines[ nLine ];
		for ( unsigned int j = 0; j < line.size(); j++ )
		{
			if ( j > 0 ) output.write( ",", 1 );
			output.write( line[j].c_str(), line[j].size() );
		}
		if ( i < (lines.size()-1) ) output.write( "\n", 1 );
	}
	output.close();

	return true;
}

bool FilterLogFile( CLogFile &logFile, CLogStats &filteredLogStats )
{
	// TODO: these filters are very inconsistent, rework it to allow more arbitrary filter specification
	//       like "include:numplayers:1-4" or "exclude:numplayers:5-8"

	// Filter based on aggregate log properties
	if ( gConfig.dangerLimit && ( logFile.minFreeMem > gConfig.dangerLimit ) && ( logFile.minGPUFreeMem > gConfig.dangerLimit ) )
		return false; // Log contains no entries with memory below the danger limit
	if ( gConfig.isServer && !logFile.isServer )
		return false; // Log doesn't contain any entries where the local machine is the Server
	if ( gConfig.isClient &&  logFile.isServer )
		return false; // Log contains entries where the local machine is the Server
	if ( gConfig.isSplitscreen && !logFile.isSplitscreen )
		return false; // Log doesn't contain any entries where the local machine is running Splitscreen
	if ( gConfig.minPlayers && ( logFile.maxPlayers < gConfig.minPlayers ) )
		return false; // Log contains no entries with the requisite number of players

	ULONGLONG systemTime = SystemTime();
	if ( gConfig.minAge && ( logFile.modifyTime > ( systemTime - gConfig.minAge*ONE_SECOND ) ) )
		return false; // Too recent
	if ( gConfig.maxAge && ( logFile.modifyTime < ( systemTime - gConfig.maxAge*ONE_SECOND ) ) )
		return false; // Too old
	if ( gConfig.isActive && !logFile.isActive )
		return false; // Not active (not currently being written)

	if ( ( gConfig.dvdHosted == DVDHOSTED_YES ) && ( logFile.headerInfo.dvdHosted != DVDHOSTED_YES ) )
		return false; // Using the HDD
	if ( ( gConfig.dvdHosted == DVDHOSTED_NO  ) && ( logFile.headerInfo.dvdHosted != DVDHOSTED_NO  ) )
		return false; // Only using the DVD

	if ( gConfig.languageFilter[0] )
	{
		if ( !strstr( logFile.headerInfo.language, gConfig.languageFilter ) )
			return false; // Did not play in the specified language
	}

	if ( gConfig.platform != PLATFORM_UNKNOWN )
	{
		if ( logFile.headerInfo.platform != gConfig.platform )
			return false; // Log file not for the specified platform
	}

	FilterBitfield mapFilter = logFile.mapList.SubstringToBitfield( gConfig.mapFilter );
	if ( !mapFilter )
		return false; // Log doesn't contain the specified map(s)
	FilterBitfield playerFilter = logFile.playerList.SubstringToBitfield( gConfig.playerFilter );
	if ( !playerFilter )
		return false; // Log doesn't contain the specified player(s)


	// The log's aggregate properties passed the filters, so now filter individual log entries
	int dangerTime			= -1;
	int numPassingEntries	= 0;
	int duration			= 0;
	for ( int i = 0; i < logFile.entries.Count(); i++ )
	{
		CItem &item = logFile.entries[ i ];

		if ( gConfig.dangerLimit && ( item.freeMem > gConfig.dangerLimit ) )
			continue; // Filter out entries above the danger limit

		if ( dangerTime == -1 )
			dangerTime = item.time;
		if ( gConfig.dangerTime && ( dangerTime > gConfig.dangerTime ) )
			return false; // Log fails if no entries pass the danger limit soon enough

		// NOTE: we don't filter individual entries by isServer, since we want to see the effects of playing maps AFTER being a server

		if ( gConfig.isSplitscreen && ( item.numLocalPlayers <= 1 ) )
			continue; // Filter out single-screen entries

		if ( gConfig.isSinglescreen && ( item.numLocalPlayers >= 2 ) )
			continue; // Filter out splitscreen entries

		if ( gConfig.mapFilter[0] && !( mapFilter.IsSet( item.logMapIndex ) ) )
			continue; // Filter out entries not on the specified map(s)

		if ( gConfig.minPlayers && ( item.numPlayers < gConfig.minPlayers ) )
			continue; // Filter out entries with too few players

		if ( gConfig.maxPlayers && ( item.numPlayers > gConfig.maxPlayers ) )
			continue; // Filter out entries with too many players

		if ( gConfig.playerFilter[0] && !( playerFilter.Intersects( item.playerBitfield ) ) )
			continue; // Filter out entries not containing the specified player(s)

		// Ok, this entry passes all filters, so update the filtered aggregate stats
		filteredLogStats.mapMin[     item.logMapIndex ]  = min( item.freeMem, filteredLogStats.mapMin[    item.logMapIndex ] );
		filteredLogStats.mapGPUMin[  item.logMapIndex ]  = min( item.gpuFree, filteredLogStats.mapGPUMin[ item.logMapIndex ] );
		filteredLogStats.mapAverage[ item.logMapIndex ] += item.freeMem;
		filteredLogStats.mapSeconds[ item.logMapIndex ] += logFile.memorylogTick;
		filteredLogStats.mapSamples[ item.logMapIndex ] ++;
		duration = max( duration, item.time ); // Only update this for entries passing the filters
		numPassingEntries++;
	}

	if ( numPassingEntries == 0 )
		return false; // Log contains no lines passing the filters

	if ( gConfig.duration && ( duration < gConfig.duration ) )
		return false; // App run time too short (at filter-passing entries)

	for ( int i = 0; i < CStringList::MAX_STRINGLIST_ENTRIES; i++ )
	{
		// Compute final per-map averages (for passing entries)
		if ( filteredLogStats.mapSamples[ i ] > 0 )
			filteredLogStats.mapAverage[ i ] /= filteredLogStats.mapSamples[ i ];
	}

	// We have a winner!
	return true;
}

bool HasFilter( void )
{
	// Are we filtering the logs in any meaningful way?
	return ( gConfig.dangerLimit || gConfig.dangerTime || gConfig.duration || gConfig.minAge || gConfig.maxAge || gConfig.minPlayers || gConfig.maxPlayers ||
			 gConfig.isServer || gConfig.isClient || gConfig.isSplitscreen || gConfig.isSinglescreen || gConfig.isActive || ( gConfig.dvdHosted != DVDHOSTED_UNKNOWN ) ||
			 gConfig.mapFilter[0] || gConfig.playerFilter[0] || gConfig.languageFilter[0] || ( gConfig.platform != PLATFORM_UNKNOWN ) );
}

void PrintStats( CLogFiles &results )
{
	int numMaps = gMapNames.Count();
	if ( ( results.size() == 0 ) || ( numMaps == 0 ) )
	{
		printf( "Aggregate stats\n" );
		printf( "---------------\n" );
		printf( "No log files loaded\n" );
		return;
	}

	if ( gConfig.isActive )
	{
		// Update which of our loaded logs are still active
		for ( CLogFiles::iterator logIt = results.begin(); logIt != results.end(); logIt++ )
		{
			CLogFile &logFile = *logIt->second;
			if ( logFile.isActive )
			{
				// If it was active when we loaded it, is it still?
				WIN32_FIND_DATA fileData;
				HANDLE handle = FindFirstFile( logFile.consoleLog.c_str(), &fileData );
				if ( ( handle == INVALID_HANDLE_VALUE ) || !IsActive( WriteTime( fileData ) ) )
				{
					logFile.isActive = false;
				}
			}
		}
	}

	typedef multimap<ULONGLONG, const CLogFile *> CFilteredLogs;
	CFilteredLogs filteredLogs;

	float        *mapMin     = new float[ numMaps ];
	float        *mapGPUMin  = new float[ numMaps ];
	float        *mapAverage = new float[ numMaps ];
	float		 *mapSeconds = new float[ numMaps ];
	unsigned int *mapSamples = new unsigned int[ numMaps ];
	unsigned int *mapLogs    = new unsigned int[ numMaps ];
	for ( int i = 0; i < numMaps; i++ )
	{
		mapMin[i]     = CPU_MEM_SIZE;
		mapGPUMin[i]  = GPU_MEM_SIZE;
		mapAverage[i] = 0.0f;
		mapSeconds[i] = 0;
		mapSamples[i] = 0;
		mapLogs[i]    = 0;
	}
	for ( CLogFiles::iterator logIt = results.begin(); logIt != results.end(); logIt++ )
	{
		CLogFile &logFile = *logIt->second;
		CLogStats filteredLogStats;

		// Run the log file through our filters
		logFile.badMaps.Purge();
		if ( FilterLogFile( logFile, filteredLogStats ) )
		{
			// Score! Use the data from this log to update per-map aggregate stats:
			for ( int i = 0; i < numMaps; i++ )
			{
				int index = logFile.mapList.StringToInt( gMapNames[ i ] ); // Index into this logfile's map list
				if ( ( index >= 0 ) && ( filteredLogStats.mapSamples[ index ] > 0 ) )
				{
					mapMin[     i ]  = min( mapMin[ i ], filteredLogStats.mapMin[ index ] );
					mapGPUMin[  i ]  = min( mapGPUMin[ i ], filteredLogStats.mapGPUMin[ index ] );
					mapAverage[ i ] += filteredLogStats.mapAverage[ index ]*filteredLogStats.mapSamples[ index ];
					mapSeconds[ i ] += filteredLogStats.mapSeconds[ index ];
					mapSamples[ i ] += filteredLogStats.mapSamples[ index ];
						mapLogs[ i ]++;
					// Build a list of the 'bad' maps in this log file, for spewage below:
					MapMin_t mapMin = { i,  filteredLogStats.mapMin[ index ] };
					logFile.badMaps.AddToTail( mapMin );
				}
			}

			// Add this to the list of logs to spew:
			filteredLogs.insert( make_pair( logFile.modifyTime, &logFile ) );
		}
	}

	if ( gConfig.trackFile[0] )
	{
		UpdateTrackFile( gConfig.trackFile, gConfig.trackColumn, mapSamples, mapMin, mapGPUMin );
	}

	// Spew the names of logs passing our filters (multimap-sorted by log last-modified time):
// TODO: will this spew scroll faster if we print multiple lines at a time?
	if ( HasFilter() )
	{
		printf( "\n\nLog files which pass filters ( command-line \"%s\" )\n", gConfig.prevCommandLine );
		printf( "----------------------------\n" );
		CFilteredLogs::iterator it;
		for ( it = filteredLogs.begin(); it != filteredLogs.end(); it++ )
		{
			static char headerString[1024];
			SpewHeaderSummary( headerString, ARRAYSIZE( headerString ), it->second->headerInfo );
			printf( "  [%s]  %s\n", headerString, it->second->consoleLog.c_str()[0] ? it->second->consoleLog.c_str() : it->second->memoryLog.c_str() );
			// Spew a little summary of all offending maps in this log file (makes associating map issues w/ logs much faster)
			printf( "   " );
			const CUtlVector<MapMin_t> &badMaps = it->second->badMaps;
			for ( int i = 0; i < badMaps.Count(); i++ ) printf( " %4.1fMB:%-28s|", badMaps[i].minMem, gMapNames[ badMaps[i].map ] );
			printf( "\n" );
		}
		printf( "\n" );
	}

	// NOTE: empty log files will always fail filters, unless you set "danger:512" (TODO: not sure this workaround works any more...)
	printf( "Aggregate stats ( command-line \"%s\" )\n", gConfig.prevCommandLine );
	printf( "--------------- (%d logs pass filters, of %d loaded)\n", filteredLogs.size(), results.size() );
	int totalSeconds = 0;
	if ( filteredLogs.size() )
	{
		for ( int i = 0; i < numMaps; i++ )
		{
			if ( mapSamples[ i ] )
			{
				mapAverage[ i ] /= mapSamples[ i ]; // Compute the final per-map average
				totalSeconds += mapSeconds[ i ];
				printf( "Map %-32s %4d logs, %6d samples, min %6.2fMB, average %6.2fMB\n",
						gMapNames[ i ], mapLogs[ i ], mapSamples[ i ], mapMin[ i ], mapAverage[ i ] );
			}
		}
	}
	int days  = (int)( totalSeconds / 86400 );
	totalSeconds -= days*86400;
	int hours = (int)( totalSeconds / 3600 );
	totalSeconds -= hours*3600;
	int mins  = (int)( totalSeconds / 60 );
	printf( " Total play time represented by these samples: %dd:%2dh:%2dm\n", days, hours, mins );

	delete mapMin;
	delete mapGPUMin;
	delete mapAverage;
	delete mapSamples;
	delete mapSeconds;
	delete mapLogs;
}

void InitMapHash( void )
{
	for ( int i = 0; i < gNumIgnoreMaps; i++ ) AddNewMapName( gIgnoreMaps[ i ] );
	for ( int i = 0; i < gNumKnownMaps;  i++ ) AddNewMapName( gKnownMaps[ i ] );
}

const char *CleanPath( const char *path )
{
	strncpy( gConfig.sourcePath, path, sizeof( gConfig.sourcePath ) );
	strlwr( gConfig.sourcePath );
	int pathLen = (int)strlen( gConfig.sourcePath );
	if ( pathLen && ( gConfig.sourcePath[ pathLen - 1 ] != '\\' ) && ( gConfig.sourcePath[ pathLen - 1 ] != '/' ) )
	{
		strncat( gConfig.sourcePath, "\\", sizeof( gConfig.sourcePath ) );
	}
	return gConfig.sourcePath;
}

void Usage()
{
	printf( "\n" );
	printf( "memlog mines vxconsole logs for memory data\n" );
	printf( "\n" );
	printf( "  USAGE: memlog [options] <folder>\n" );
	printf( "\n" );
	printf( "Input is a folder. memlog will convert all vxconsole logs in that folder into\n" );
	printf( "memlog files (prefix 'memorylog_'). It will then output aggregate memory data for\n" );
	printf( "those files.\n" );
	printf( "\n" );
	printf( "options:\n" );
	printf( "[-c|console]           run in console mode (see below)\n" );
	printf( "[-r|recurse]           recurse the input folder tree\n" );
	printf( "[-u|update]            reconvert vxconsole logs that have been updated\n" );
	printf( "[-a|updateactive]      reconvert vxconsole logs that are still being updated\n" );
	printf( "[-f|force]             reconvert all vxconsole logs\n" );
	printf( "\n" );
	printf( "tracking:\n" );
	printf( "[-track:<file>]        update a CSV file which tracks memory stats over time\n" );
	printf( "                       the filename must end with one of these suffices:\n" );
	printf( "                         '_MinFreeCPU' - track minimium free CPU memory\n" );
	printf( "                         '_MinFreeGPU' - track minimium free GPU memory\n" );
	printf( "[-trackcol:<name>]     'name' is the new column to add (rows are maps)\n" );
	printf( "\n" );
	printf( "analysis:    (spews data passing these filters - all default to off)\n" );
	printf( "    ----the following options are applied per log entry----\n" );
	printf( "[-danger:N]            pass log entries in which memory is below N kilobytes\n" );
	printf( "[-minplayers:N]        pass log entries with at least this many concurrent players\n" );
	printf( "[-maxplayers:N]        pass log entries with at most this many concurrent players\n" );
	printf( "[-issinglescreen]      pass log entries in which the local box has 1 player\n" );
	printf( "[-issplitscreen]       pass log entries in which the local box has 2 players\n" );
	printf( "[-map:<name>]          pass log entries with map names containing this substring\n" );
	printf( "[-player:<name>]       pass log entries with player names containing this substring\n" );
	printf( "    ----the following options are applied per log file----\n" );
	printf( "[-dangertime:N]        pass logs dropping below 'danger' within this many minutes\n" );
	printf( "[-duration:X]          pass logs in which the timer reaches this many minutes\n" );
	printf( "[-minage:N]            pass logs updated at least this many hours ago\n" );
	printf( "[-maxage:N]            pass logs updated at most this many hours ago\n" );
	printf( "[-isserver]            pass logs in which the local box is a listen server at least once\n" );
	printf( "[-isclient]            pass logs in which the local box is NEVER a listen server\n" );
	printf( "[-isactive]            pass logs that are still being updated\n" );
	printf( "[-isdvdonly]           pass logs in which the local box is fully DVD hosted\n" );
	printf( "[-isusinghdd]          pass logs in which the local box is using the HDD\n" );
	printf( "[-language:<name>]     pass logs with the language containing this substring\n" );
	printf( "[-platform:<name>]     pass logs generated on this platform ('360', 'PS3', 'PC')\n" );
	printf( "\n" );
	printf( "\n" );
	printf( "Console mode:\n" );
	printf( "\n" );
	printf( "  USAGE: [options] [folder]\n" );
	printf( "\n" );
	printf( "In console mode, memlog keeps running until told to quit, allowing the user to\n" );
	printf( "perform any number of log mining operations without having to re-load all the logs\n" );
	printf( "each time. Commands in console mode are the same as the above command-line\n" );
	printf( "options (without the leading '-'). Enter a set of commands, in any order (followed\n" );
	printf( "by a folder path if appropriate), and hit enter. If you don't specify a path, the\n" );
	printf( "most recently entered path will be used.\n" );
	printf( "\n" );
	printf( "console-mode-specific commands:\n" );
	printf( "[load]                 load more memory logs, from the specified folder\n" );
	printf( "[unload]               unload memory logs in the specified folder\n" );
	printf( "[unloadall]            unload all memory logs\n" );
	printf( "\n" );
}

void InitConfig( bool bCommandLine = false )
{
	if ( bCommandLine )
	{
		// These persist after the first set of commands:
		gConfig.sourcePath[0]	= 0;
		gConfig.consoleMode		= false;
	}

	gConfig.recurse				= false;
	gConfig.update				= false;
	gConfig.updateActive		= false;
	gConfig.forceUpdate			= false;

	gConfig.load				= false;
	gConfig.unload				= false;
	gConfig.unloadAll			= false;
	gConfig.quitting			= false;
	gConfig.help				= false;

	// 'track' updates a memory tracking file, but only do it when asked
	gConfig.trackFile[0]		= 0;
	gConfig.trackColumn[0]		= 0;
	gConfig.trackStat			= TRACKSTAT_UNKNOWN;

	// Default all filters off (no analysis)
	gConfig.dangerLimit			= 0;
	gConfig.dangerTime			= 0;
	gConfig.duration			= 0;
	gConfig.minAge				= 0;
	gConfig.maxAge				= 0;
	gConfig.minPlayers			= 0;
	gConfig.maxPlayers			= 0;
	gConfig.isServer			= false;
	gConfig.isClient			= false;
	gConfig.isSplitscreen		= false;
	gConfig.isSinglescreen		= false;
	gConfig.isActive			= false;
	gConfig.dvdHosted			= DVDHOSTED_UNKNOWN;
	gConfig.mapFilter[0]		= 0;
	gConfig.playerFilter[0]		= 0;
	gConfig.languageFilter[0]	= 0;
	gConfig.platform			= PLATFORM_UNKNOWN;
}

bool ParseOption( const char *option )
{
	// Make everything lower-case for simplicity
	strlwr( (char *)option );

	if ( option[0] == '-' )
		option++;

	// Console mode
	{
		if ( !strcmp( option, "c" ) || !strcmp( option, "console" ) )
		{
			gConfig.consoleMode = true;
			return true;
		}

		if ( !strcmp( option, "quit" ) || !strcmp( option, "exit" ) )
		{
			gConfig.quitting = true;
			return true;
		}

		if ( !strcmp( option, "help" ) || !strcmp( option, "h" ) || !strcmp( option, "usage" ) || !strcmp( option, "?" ) )
		{
			gConfig.help = true;
			return true;
		}
	}

	// Data analysis (filters)
	{
		int dangerLimit;
		if ( sscanf( option, "danger:%d", &dangerLimit ) == 1 )
		{
			if ( dangerLimit > 0 )
			{
				gConfig.dangerLimit = dangerLimit / 1024.0f; // KB -> MB
				return true;
			}
			printf( "!'danger' must be > 0!\n" );
			return false;
		}

		int dangerTime;
		if ( sscanf( option, "dangertime:%d", &dangerTime ) == 1 )
		{
			if ( dangerTime > 0 )
			{
				gConfig.dangerTime = dangerTime*60; // Minutes
				return true;
			}
			printf( "!'dangertime' must be > 0!\n" );
			return false;
		}

		int duration;
		if ( sscanf( option, "duration:%d", &duration ) == 1 )
		{
			if ( duration > 0 )
			{
				gConfig.duration = duration*60; // Minutes
				return true;
			}
			printf( "!'duration' must be > 0!\n" );
			return false;
		}

		int minAge;
		if ( sscanf( option, "minage:%d", &minAge ) == 1 )
		{
			if ( minAge >= 0 )
			{
				gConfig.minAge = minAge*3600; // Hours
				return true;
			}
			printf( "!'minage' must be >= 0!\n" );
			return false;
		}

		int maxAge;
		if ( sscanf( option, "maxage:%d", &maxAge ) == 1 )
		{
			if ( maxAge >= 0 )
			{
				gConfig.maxAge = maxAge*3600; // Hours
				return true;
			}
			printf( "!'maxage' must be >= 0!\n" );
			return false;
		}

		int minPlayers;
		if ( sscanf( option, "minplayers:%d", &minPlayers ) == 1 )
		{
			if ( minPlayers >= 1 )
			{
				gConfig.minPlayers = minPlayers;
				return true;
			}
			printf( "!'minplayers' must be >= 1!\n" );
			return false;
		}

		int maxPlayers;
		if ( sscanf( option, "maxplayers:%d", &maxPlayers ) == 1 )
		{
			if ( maxPlayers >= 1 )
			{
				gConfig.maxPlayers = maxPlayers;
				return true;
			}
			printf( "!'maxplayers' must be >= 1!\n" );
			return false;
		}

		if ( !strcmp( option, "isclient" ) )
		{
			if ( gConfig.isServer )
			{
				printf( "!'isclient' and 'isserver' are mutually exclusive!\n" );
				return false;
			}
			gConfig.isClient = true;
			return true;
		}

		if ( !strcmp( option, "isserver" ) )
		{
			if ( gConfig.isClient )
			{
				printf( "!'isclient' and 'isserver' are mutually exclusive!\n" );
				return false;
			}
			gConfig.isServer = true;
			return true;
		}

		if ( !strcmp( option, "issplitscreen" ) )
		{
			if ( gConfig.isSinglescreen )
			{
				printf( "!'issinglescreen' and 'issplitscreen' are mutually exclusive!\n" );
				return false;
			}
			gConfig.isSplitscreen = true;
			return true;
		}

		if ( !strcmp( option, "issinglescreen" ) )
		{
			if ( gConfig.isSplitscreen )
			{
				printf( "!'issinglescreen' and 'issplitscreen' are mutually exclusive!\n" );
				return false;
			}
			gConfig.isSinglescreen = true;
			return true;
		}

		if ( !strcmp( option, "isactive" ) )
		{
			gConfig.isActive = true;
			return true;
		}

		if ( !strcmp( option, "isdvdonly" ) )
		{
			if ( gConfig.dvdHosted == DVDHOSTED_NO )
			{
				printf( "!'isdvdonly' and 'isusinghdd' are mutually exclusive!\n" );
				return false;
			}
			gConfig.dvdHosted = DVDHOSTED_YES;
			return true;
		}

		if ( !strcmp( option, "isusinghdd" ) )
		{
			if ( gConfig.dvdHosted == DVDHOSTED_YES )
			{
				printf( "!'isdvdonly' and 'isusinghdd' are mutually exclusive!\n" );
				return false;
			}
			gConfig.dvdHosted = DVDHOSTED_NO;
			return true;
		}

		char mapFilter[ FILTER_SIZE ] = "";
		if ( sscanf( option, "map:%32s", mapFilter ) == 1 )
		{
			if ( mapFilter[0] )
			{
				strncpy( gConfig.mapFilter, mapFilter, sizeof( gConfig.mapFilter ) );
				return true;
			}
			return false;
		}

		char playerFilter[ FILTER_SIZE ] = "";
		if ( sscanf( option, "player:%32s", playerFilter ) == 1 )
		{
			if ( playerFilter[0] )
			{
				strncpy( gConfig.playerFilter, playerFilter, sizeof( gConfig.playerFilter ) );
				return true;
			}
			return false;
		}

		char languageFilter[ FILTER_SIZE ] = "";
		if ( sscanf( option, "language:%32s", languageFilter ) == 1 )
		{
			if ( languageFilter[0] )
			{
				strncpy( gConfig.languageFilter, languageFilter, sizeof( gConfig.languageFilter ) );
				return true;
			}
			return false;
		}

		char platformFilter[ 16 ] = "";
		if ( sscanf( option, "platform:%16s", platformFilter ) == 1 )
		{
			return StringToPlatformName( platformFilter, gConfig.platform );
		}
	}

	// File processing
	{
		if ( !strcmp( option, "r" ) || !stricmp( option, "recurse" ) )
		{
			gConfig.recurse = true;
			return true;
		}

		if ( !strcmp( option, "u" ) || !stricmp( option, "update" ) )
		{
			if ( gConfig.unload || gConfig.unloadAll )
			{
				printf( "!'update' is mututally exclusive with 'unload'!\n" );
				return false;
			}
			gConfig.update = true;
			gConfig.load = true; // Less confusing if update implies load
			return true;
		}

		if ( !strcmp( option, "a" ) || !stricmp( option, "updateactive" ) )
		{
			if ( gConfig.unload || gConfig.unloadAll )
			{
				printf( "!'updateactive' is mututally exclusive with 'unload'!\n" );
				return false;
			}
			gConfig.updateActive = true;
			gConfig.update = true; // Less confusing if updateActive implies update
			gConfig.load = true; // Less confusing if update implies load
			return true;
		}

		if ( !strcmp( option, "f" ) || !stricmp( option, "force" ) )
		{
// TODO: these error checks should really be a post-step - this depends on ordering (i.e. we don't get a message if force appears *before* unload)
			if ( gConfig.unload || gConfig.unloadAll )
			{
				printf( "!'force' is mututally exclusive with 'unload'!\n" );
				return false;
			}
			gConfig.load = true; // Less confusing if update implies load
			gConfig.forceUpdate = true;
			return true;
		}

		if ( !strcmp( option, "load" ) )
		{
			if ( gConfig.unload || gConfig.unloadAll )
			{
				printf( "!'load' is mututally exclusive with 'unload'!\n" );
				return false;
			}
			gConfig.load = true;
			return true;
		}

		if ( !strcmp( option, "unload" ) )
		{
			if ( gConfig.load )
			{
				printf( "!'load' is mututally exclusive with 'unload'!\n" );
				return false;
			}
			gConfig.unload = true;
			return true;
		}

		if ( !strcmp( option, "unloadall" ) )
		{
			if ( gConfig.load )
			{
				printf( "!'load' is mututally exclusive with 'unloadall'!\n" );
				return false;
			}
			gConfig.unloadAll = true;
			return true;
		}

		COMPILE_TIME_ASSERT( sizeof( gConfig.trackFile ) == MAX_PATH );
		if ( 1 == sscanf( option, "track:%260s", gConfig.trackFile ) )
		{
			int nStrLen = strlen( gConfig.trackFile );
			const char *ext = V_stristr( gConfig.trackFile, ".csv" );
			if ( !ext || ( ( ext - &gConfig.trackFile[0] ) != ( nStrLen - 4 ) ) || ( nStrLen <= 4 ) )
			{
				printf( "!'track' must specify a .csv file!\n" );
				return false;
			}
			// Filename suffix determines the stat tracked in the file
			if ( V_stristr( gConfig.trackFile, "_MinFreeCPU" ) )
				gConfig.trackStat = TRACKSTAT_MINFREE_CPU;
			else if ( V_stristr( gConfig.trackFile, "_MinFreeGPU" ) )
				gConfig.trackStat = TRACKSTAT_MINFREE_GPU;
			else
			{
				printf( "!'track' .csv file must end with a valid stat type suffix!\n" );
				return false;
			}
			return true;
		}

		COMPILE_TIME_ASSERT( sizeof( gConfig.trackColumn ) == 32 );
		if ( 1 == sscanf( option, "trackcol:%32s", gConfig.trackColumn ) )
		{
			return true;
		}
	}

	return false;
}

bool ParseCommandLine( int argc, _TCHAR* argv[], Config &config )
{
	bool bCommandLine = true;
	InitConfig( bCommandLine );

	// Cache off the command-line:
	gConfig.prevCommandLine[0] = 0;
	for ( int i = 1; i < argc; i++ )
	{
		strcat( gConfig.prevCommandLine, argv[i] );
		strcat( gConfig.prevCommandLine, " " );
	}

	int numOptions = 1;
	while ( argv[numOptions] && argv[numOptions][0] == '-' )
	{
		if ( !ParseOption( argv[numOptions] ) )
		{
			printf( "ERROR: invalid command-line option '%s'!\n\n\n", argv[numOptions] );
			Usage();
			return false;
		}
		numOptions++;
	}

	if ( numOptions == argc )
	{
		if ( !gConfig.consoleMode )
		{
			printf( "ERROR: no folder path specified!\n\n\n" );
			Usage();
			return false;
		}
		CleanPath( "" );
	}
	else
	{
		gConfig.load = true;
		CleanPath( argv[numOptions] );
	}

	return true;
}

void ExtractTokens( char *buffer, vector<string> &tokens )
{
	char *context = NULL;
	char *token   = strtok_s( buffer, " ", &context );
	while( token )
	{
		tokens.push_back( string( token ) );
		token = strtok_s( NULL, " ", &context );
	}
}

bool ParseConsoleCommand( void )
{
	if ( !gConfig.consoleMode )
		return false;

	while( true )
	{
		// Loop until the user inputs a command without errors
		bool bError = false;

		// NOTE: this remembers the last folder path used
		InitConfig();

		printf( "\n\n\n\n" );
		printf( "> current folder path: '%s'\n", gConfig.sourcePath[0] ? gConfig.sourcePath : "<none>" );
		printf( "> Enter a command to process:\n" );
		printf( ">\n" );
		printf( "-> " );
		char buffer[4*_MAX_PATH] = "";
		// TODO: (?while debugging?) this interferes with command prompt cut'n'paste (QuickEdit gets around it):
		cin.getline( buffer, sizeof( buffer ) );
		strcpy( gConfig.prevCommandLine, buffer );
		vector<string> commands;
		ExtractTokens( buffer, commands );

		if ( commands.size() < 1 )
			bError = true;

		for ( unsigned int i = 0; i < commands.size(); i++ )
		{
			string & command = commands[ i ];
			if ( !ParseOption( command.c_str() ) )
			{
				// If parsing files, the last item should be the folder path
				if ( ( i == ( commands.size() - 1 ) ) &&
					 ( gConfig.load || gConfig.unload ) )
				{
					CleanPath( command.c_str() );
				}
				else
				{
					printf( "\nInvalid command '%s'\n\n", command.c_str() );
					bError = true;
				}
			}
		}

		if ( ( gConfig.load || gConfig.unload ) && !gConfig.sourcePath[0] )
		{
			printf( "\nYou must specify a path in order to use '%s'\n\n", gConfig.load ? "load" : "unload" );
			bError = true;
		}

		if ( gConfig.quitting )
			return false;

		if ( gConfig.help )
		{
			Usage();
		}
		else if ( !bError )
		{
			printf( ">\n" );
			printf( "> current folder path: '%s'\n", gConfig.sourcePath[0] ? gConfig.sourcePath : "<none>" );
			printf( "> Processing command...\n" );
			printf( "\n" );
			return true;
		}
	}
}

// NOTE: this app doesn't bother with little things like freeing memory - enjoy!
int _tmain(int argc, _TCHAR* argv[])
{
	InitMapHash();

	// Grab command-line options
	if ( !ParseCommandLine( argc, argv, gConfig ) )
		return 1;

	CLogFiles results;
	do
	{
		// Process log files
//  TODO:  aggregate error messages and spew them at the end, so it's easier to notice+read them (list of: " 'log' plus all errors for 'log' ", for every 'log' with errors)
		ProcessLogFiles( gConfig.sourcePath, results );

		// Print stats gathered from the log files
		PrintStats( results );
	}
	// Continue processing commands (console mode) if requested
	while( ParseConsoleCommand() );

// TODO: add a new option to filter on the 'memory dip' during a map - i.e. the biggest reduction in free memory during a map (always ignore the first ?2? entries for a given map - find the biggest dips to see if 2 is right... ideally, we want to synch up with charlie's numbers for this to be useful)

// TODO: quantize [MEMORYLOG] timestamps (set 'next time' rather than 'prev time' -> :00, :20, :40) so spew aligns for all clients in a game (well, it wouldn't really be synchronized, depending on how long they sat at their respective menus...)

// TODO: spew aggregate memlog data
//      - worst-cases
//      - av/min/max per map, per machine, globally
//        probably want to ignore early high results, concentrate on longer-term numbers (after X times or minutes on a map)
//        maybe spew results for "at least x minutes", for several different values of x (15, 30, 60, 90, 120...)
//      - correlate mem with: play time, map loads, numplayers, listen Vs dedicated server,
//                            player join/quits, team death/restarts, campaign starts/ends, exit to menu...
//       o load entries for a log file
//       o create aggregates for a log file for various criteria
//       o combine aggregates across all files

// TODO: would also like to parse SBH spew
// store values in the header (want maxima, per-alloc-size & per-heap)....
//		- detect start of an SBH dump and write a func to process it (take note of Toms recent change to the spew - therell be two types of spew out there)
// think about detecting the main menu by looking for adjacent 'none' lines...
//		- post-process the log and convert all reasonable 'none's into 'menu's
//		- convert runs with a full minute of 'none' at either end
//		- menu items must also be "client" and have zero players

	if ( IsDebuggerPresent() )
	{
		printf( "\n\nPress any key to exit...\n" );
		_getche();
	}

	return 0;
}
