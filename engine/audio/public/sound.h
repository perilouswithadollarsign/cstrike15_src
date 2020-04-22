//===== Copyright  1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: client sound i/o functions
//
//===========================================================================//
#ifndef SOUND_H
#define SOUND_H
#ifdef _WIN32
#pragma once
#endif

#include "basetypes.h"
#include "datamap.h"
#include "mathlib/vector.h"
#include "mathlib/mathlib.h"
#include "tier1/strtools.h"
#include "soundflags.h"
#include "utlvector.h"
#include "engine/SndInfo.h"
#include "cdll_int.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"

DECLARE_LOGGING_CHANNEL( LOG_SOUND_OPERATOR_SYSTEM );

#define MAX_SFX  2048

#define AUDIOSOURCE_CACHE_ROOTDIR	"maps/soundcache"

class CSfxTable;
enum soundlevel_t;
struct SoundInfo_t;
struct AudioState_t;
struct channel_t;
class IFileList;

void S_Init (void);
void S_Shutdown (void);
bool S_IsInitted();

void S_StopAllSounds(bool clear);
bool S_GetPreventSound( void );

#if !USE_AUDIO_DEVICE_V1
struct audio_device_description_t;
void S_GetAudioDeviceList( CUtlVector<audio_device_description_t> &audioList );
#endif

class CAudioState
{
public:
	CAudioState() {}

	bool IsAnyPlayerUnderwater() const;

	AudioState_t &GetPerUser( int nSlot = -1 );
	const AudioState_t &GetPerUser( int nSlot = -1 ) const;
private:

	AudioState_t m_PerUser[ MAX_SPLITSCREEN_CLIENTS ];
};

void S_Update( const CAudioState *pAudioState );
void S_ExtraUpdate (void);
void S_ClearBuffer (void);
void S_BlockSound (void);
void S_UnblockSound (void);
void S_UpdateWindowFocus( bool bWindowHasFocus );
float S_GetMasterVolume( void );
void S_SoundFade( float percent, float holdtime, float intime, float outtime );
void S_OnLoadScreen(bool value);
void S_EnableThreadedMixing( bool bEnable );
void S_EnableMusic( bool bEnable );

void S_PreventSound(bool bSetting);

struct StartSoundParams_t
{
	StartSoundParams_t() :
		m_nSoundScriptHash( SOUNDEMITTER_INVALID_HASH ),
		m_pSoundEntryName( NULL ),
		staticsound( false ),
		userdata( 0 ),
		soundsource( 0 ), 
		entchannel( CHAN_AUTO ), 
		pSfx( 0 ), 
		bUpdatePositions( true ),
		fvol( 1.0f ),  
		soundlevel( SNDLVL_NORM ), 
		flags( SND_NOFLAGS ), 
		pitch( PITCH_NORM ), 
		fromserver( false ),
		delay( 0.0f ),
		speakerentity( -1 ),
		bToolSound( false ),
		initialStreamPosition( 0 ),
		skipInitialSamples( 0 ),
		m_nQueuedGUID( UNINT_GUID ),
		m_bIsScriptHandle( false ),
		m_pOperatorsKV( NULL ),
		opStackElapsedTime( 0.0f ),
		opStackElapsedStopTime( 0.0f ),
		m_bDelayedStart( false ),
		m_bInEyeSound( false ),
		m_bHRTFFollowEntity( false ),
		m_bHRTFBilinear( false ),
		m_bHRTFLock( false )
	{
		origin.Init();
		direction.Init();
	}
	void Copy( StartSoundParams_t &destParams )
	{
		destParams.userdata = userdata;
		destParams.soundsource = soundsource;
		destParams.entchannel = entchannel;
		destParams.pSfx = pSfx;
		VectorCopy( origin, destParams.origin );
		VectorCopy( direction, destParams.direction );
		destParams.fvol = fvol;
		destParams.soundlevel = soundlevel;
		destParams.flags = flags;
		destParams.pitch = pitch; 
		destParams.delay = delay;
		destParams.speakerentity = speakerentity;
		destParams.initialStreamPosition = initialStreamPosition;
		destParams.skipInitialSamples = skipInitialSamples;
		destParams.m_nQueuedGUID = m_nQueuedGUID;
		destParams.m_nSoundScriptHash = m_nSoundScriptHash;
		destParams.m_pSoundEntryName = m_pSoundEntryName;
		destParams.m_pOperatorsKV = m_pOperatorsKV;
		destParams.opStackElapsedTime = opStackElapsedTime;
		destParams.opStackElapsedStopTime = opStackElapsedStopTime;
		destParams.staticsound = staticsound;
		destParams.bUpdatePositions = bUpdatePositions;
		destParams.fromserver = fromserver;
		destParams.bToolSound = bToolSound;
		destParams.m_bIsScriptHandle = m_bIsScriptHandle;
		destParams.m_bDelayedStart = m_bDelayedStart;
		destParams.m_bInEyeSound = m_bInEyeSound;
		destParams.m_bHRTFFollowEntity = m_bHRTFFollowEntity;
		destParams.m_bHRTFBilinear = m_bHRTFBilinear;
		destParams.m_bHRTFLock = m_bHRTFLock;
	}
	void CopyNewFromParams( StartSoundParams_t &destParams )
	{
		destParams.userdata = userdata;
		// 		destParams.soundsource = soundsource;
		// 		destParams.entchannel = entchannel;
		destParams.pSfx = pSfx;
		VectorCopy( origin, destParams.origin );
		VectorCopy( direction,destParams.direction );
		destParams.fvol = fvol;
		destParams.soundlevel = soundlevel;
		destParams.flags = flags;
		destParams.pitch = pitch; 
		destParams.delay = delay;
		destParams.speakerentity = speakerentity;
		// 		destParams.initialStreamPosition = initialStreamPosition;
		// 		destParams.skipInitialSamples = skipInitialSamples;
		// 		destParams.m_nQueuedGUID = m_nQueuedGUID;
		// 		destParams.m_nSoundScriptHash = m_nSoundScriptHash;
		// 		destParams.m_pSoundEntryName = m_pSoundEntryName;
		// 		destParams.m_pOperatorsKV = m_pOperatorsKV;
		// 		destParams.opStackElapsedTime = opStackElapsedTime;
		// 		destParams.opStackElapsedStopTime = opStackElapsedStopTime;
		destParams.staticsound = staticsound;
		destParams.bUpdatePositions = bUpdatePositions;
		destParams.fromserver = fromserver;
		destParams.bToolSound = bToolSound;
		destParams.m_bIsScriptHandle =m_bIsScriptHandle;
		destParams.m_bInEyeSound = m_bInEyeSound;
		destParams.m_bHRTFFollowEntity = m_bHRTFFollowEntity;
		destParams.m_bHRTFBilinear = m_bHRTFBilinear;
		destParams.m_bHRTFLock = m_bHRTFLock;

		/*		destParams.m_bDelayedStart = m_bDelayedStart;*/
	}
	int				userdata;
    int				soundsource;
	int				entchannel;
	CSfxTable		*pSfx;
	Vector			origin; 
	Vector			direction; 
	float			fvol;
	soundlevel_t	soundlevel;
	int				flags;
	int				pitch; 
	float			delay;
	int				speakerentity;
	int				initialStreamPosition;
	int				skipInitialSamples;
	int				m_nQueuedGUID;
	HSOUNDSCRIPTHASH m_nSoundScriptHash;
	const char		*m_pSoundEntryName;
	KeyValues		*m_pOperatorsKV;
	float			opStackElapsedTime;
	float			opStackElapsedStopTime;

	bool			staticsound : 1;
	bool			bUpdatePositions : 1;
	bool			fromserver : 1;
	bool			bToolSound : 1;
	bool			m_bIsScriptHandle : 1;
	bool			m_bDelayedStart : 1;
	bool			m_bInEyeSound : 1;
	bool			m_bHRTFFollowEntity : 1;
	bool			m_bHRTFBilinear : 1;
	bool			m_bHRTFLock : 1;

	static const int	UNINT_GUID = -1;
	static const int	GENERATE_GUID = -2;		// Generate GUID regardless of the other vol and pitch flags.
};

int S_StartSoundEntry( StartSoundParams_t &pStartParams, int nSeed, bool bFromQueue = false );
int S_StartSound( StartSoundParams_t& params );
void S_StopSound ( int entnum, int entchannel );
enum clocksync_index_t
{
	CLOCK_SYNC_CLIENT = 0,
	CLOCK_SYNC_SERVER,
	NUM_CLOCK_SYNCS
};

extern float S_ComputeDelayForSoundtime( float soundtime, clocksync_index_t syncIndex );


void S_StopSoundByGuid( int guid, bool bForceSync = false );
float S_SoundDuration( channel_t * pChannel );
float S_SoundDurationByGuid( int guid );
int S_GetGuidForLastSoundEmitted();
bool S_IsSoundStillPlaying( int guid );
bool S_GetSoundChannelVolume( const char* sound, float &flVolumeLeft, float &flVolumeRight );
void S_GetActiveSounds( CUtlVector< SndInfo_t >& sndlist );
void S_SetVolumeByGuid( int guid, float fvol );
float S_GetElapsedTime( const channel_t * pChannel );
float S_GetElapsedTimeByGuid( int guid );
bool S_IsLoopingSoundByGuid( int guid );
void S_ReloadSound( const char *pSample );
float S_GetMono16Samples( const char *pszName, CUtlVector< short >& sampleList );

CSfxTable *S_DummySfx( const char *name );
CSfxTable *S_PrecacheSound (const char *sample );
void S_PrefetchSound( char const *name, bool bPlayOnce );
void S_MarkUISound( CSfxTable *pSfx );
void S_ReloadFilesInList( IFileList *pFilesToReload );

vec_t S_GetNominalClipDist();

extern bool TestSoundChar(const char *pch, char c);
extern char *PSkipSoundChars(const char *pch);

#include "soundchars.h"

// for recording movies
void SND_MovieStart( void );
void SND_MovieEnd( void );

//-------------------------------------

int S_GetCurrentStaticSounds( SoundInfo_t *pResult, int nSizeResult, int entchannel );

//-----------------------------------------------------------------------------

float S_GetGainFromSoundLevel( soundlevel_t soundlevel, vec_t dist );

struct musicsave_t
{
	DECLARE_SIMPLE_DATADESC();

	char	songname[ 128 ];
	int		sampleposition;
	short	master_volume;
};

void S_GetCurrentlyPlayingMusic( CUtlVector< musicsave_t >& list );
void S_RestartSong( const musicsave_t *song );

struct channelsave
{
	DECLARE_SIMPLE_DATADESC();

	char soundName[64];
	Vector origin;
	soundlevel_t soundLevel;
	int soundSource;
	int entChannel;
	int pitch;
	float opStackElapsedTime;
	float opStackElapsedStopTime;
	short masterVolume;
};

typedef CUtlVector< channelsave > ChannelSaveVector;

void S_GetActiveSaveRestoreChannels( ChannelSaveVector& channelSaves );
void S_RestartChannel( channelsave const& channelSave );


bool S_DSPGetCurrentDASRoomNew(void);
bool S_DSPGetCurrentDASRoomChanged(void);
bool S_DSPGetCurrentDASRoomSkyAbove(void);
float S_DSPGetCurrentDASRoomSkyPercent(void);

enum setmixer_t
{
	MIXER_SET = 0,
	MIXER_MULT
};

void S_SetMixGroupOfCurrentMixer( const char *szgroupname, const char *szparam, float val, int setMixerType );
int S_GetMixGroupIndex( const char *pMixGroupName );
int S_GetMixLayerIndex(const char *szmixlayername);
void S_SetMixLayerLevel(int index, float level);
void S_SetMixLayerTriggerFactor( const char *pMixLayerName, const char *pMixGroupName, float flFactor );
void S_SetMixLayerTriggerFactor( int nMixLayerIndex, int nMixGroupIndex, float flFactor );

// global pitch scale
void S_SoundSetPitchScale( float flPitchScale );
float S_SoundGetPitchScale( void );

bool S_SOSSetOpvarFloat( const char *pOpVarName, float flValue );
bool S_SOSGetOpvarFloat( const char *pOpVarName, float &flValue );

void S_ValidateSoundCache( char const *pchWavFile );

#if defined( _GAMECONSOLE )
void S_UnloadSound( const char *pName );
#endif

void S_PurgeSoundsDueToLanguageChange();

#endif // SOUND_H
