//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Main control for any streaming sound output device.
//
//===========================================================================//

#include "audio_pch.h"
#include "host.h"
#include "time.h"
#include "const.h"
#include "cdll_int.h"
#include "sound.h"
#include "client_class.h"
#include "icliententitylist.h"
#include "tier1/fmtstr.h"
#include "con_nprint.h"
#include "tier0/icommandline.h"
#include "vox_private.h"
#include "../../traceinit.h"
#include "../../cmd.h"
#include "toolframework/itoolframework.h"
#include "vstdlib/random.h"
#include "vstdlib/jobthread.h"
#include "vaudio/ivaudio.h"
#include "../../client.h"
#include "../../cl_main.h"
#include "tier3/tier3.h"
#include "utldict.h"
#include "mempool.h"
#include "../../enginetrace.h"			// for traceline
#include "../../public/bspflags.h"		// for traceline
#include "../../public/gametrace.h"		// for traceline
#include "vphysics_interface.h"		// for surface props
#include "../../ispatialpartitioninternal.h"	// for entity enumerator
#include "../../debugoverlay.h"
#include "icliententity.h"
#include "../../cmodel_engine.h"
#include "../../staticpropmgr.h"
#include "../../server.h"
#include "edict.h"
#include "../../pure_server.h"
#include "filesystem/IQueuedLoader.h"
#include "filesystem/IXboxInstaller.h"
#include "voice.h"
#include "snd_dma.h"
#include "snd_mixgroups.h"
#include "../../cl_splitscreen.h"
#include "../../common/blackbox_helper.h"
#include "snd_op_sys/sos_system.h"
#include "snd_dev_common.h"
#include "tier1/utlhashtable.h"

#include "cl_steamauth.h"

#include <vgui/ISurface.h>


#if defined( _X360 )
#include "xbox/xbox_console.h"
#include "xmp.h"
#include "avi/ibik.h"
extern IBik *bik;
#elif defined( _PS3 )
#include "ps3/ps3_console.h"
#include "snd_ps3_mp3dec.h"
void HandleRemainingFrameInfos( int nMp3DecoderSlot, bool bBlocking );
#include "avi/ibik.h"
extern IBik *bik;
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

///////////////////////////////////
// DEBUGGING
//
// Turn this on to print channel output msgs.
//
//#define DEBUG_CHANNELS


ConVar snd_sos_show_client_rcv("snd_sos_show_client_rcv", "0", FCVAR_CHEAT);
ConVar snd_sos_allow_dynamic_chantype( "snd_sos_allow_dynamic_chantype", IsPlatformX360() ? "1" : "1" );

//Controls whether we use HRTF (phonon) audio for sounds marked to use it.
ConVar snd_use_hrtf("snd_use_hrtf", "1", FCVAR_ARCHIVE);
ConVar snd_hrtf_lerp_min_distance("snd_hrtf_lerp_min_distance", "0.0", FCVAR_CHEAT);
ConVar snd_hrtf_lerp_max_distance("snd_hrtf_lerp_max_distance", "0.0", FCVAR_CHEAT);

BEGIN_DEFINE_LOGGING_CHANNEL( LOG_SOUND_OPERATOR_SYSTEM, "SoundOperatorSystem", LCF_CONSOLE_ONLY, LS_MESSAGE );
ADD_LOGGING_CHANNEL_TAG( "SoundOperatorSystem" );
END_DEFINE_LOGGING_CHANNEL();

extern ConVar dsp_spatial;
extern IPhysicsSurfaceProps	*physprops;
extern IVEngineClient *engineClient;

static void S_Play( const CCommand &args );
static void S_PlayHRTF( const CCommand & args );
static void S_PlayVol( const CCommand &args );
void S_SoundList(void);
static void S_Say ( const CCommand &args );
void S_Update_(float);
void S_StopAllSounds(bool clear);
void S_StopAllSoundsC(void);
bool S_GetPreventSound( void );
void S_ShutdownMixThread();
const char *GetClientClassname( SoundSource soundsource );
void S_PreventSound(bool bSetting);

float SND_GetGainObscured( int nSlot, gain_t *gs, const channel_t *ch, const Vector &vecListenerOrigin, bool fplayersound, bool flooping, bool bAttenuated, bool bOkayToTrace, Vector *pOrigin );
void DSP_ChangePresetValue( int idsp, int channel, int iproc, float value );
bool DSP_CheckDspAutoEnabled( void );
void DSP_SetDspAuto( int dsp_preset );
float dB_To_Radius ( float db );
int dsp_room_GetInt ( void );

void ChannelSetVolTargets( channel_t *pch, float *pvolumes, int ivol_offset, int cvol );
void ChannelUpdateVolXfade( channel_t *pch );
void ChannelClearVolumes( channel_t *pch );
float VOX_GetChanVol(channel_t *ch);
void ConvertListenerVectorTo2D( Vector *pvforward, const Vector *pvright );
int ChannelGetMaxVol( channel_t *pch );
bool S_IsMusic( channel_t *pChannel );
bool S_ShouldSaveRestore( channel_t const* pChannel );

// Forceably ends voice tweak mode (only occurs during snd_restart
void VoiceTweak_EndVoiceTweakMode();
bool VoiceTweak_IsStillTweaking();
// Only does anything for voice tweak channel so if view entity changes it doesn't fade out to zero volume
void Voice_Spatialize( channel_t *channel );
extern float g_flReplayMusicGain;

static ConVar snd_mergemethod( "snd_mergemethod", "1", 0, "Sound merge method (0 == sum and clip, 1 == max, 2 == avg)." );
static ConVar snd_report_start_sound( "snd_report_start_sound", "0", FCVAR_CHEAT, "If set to 1, report all sounds played with S_StartSound(). The sound may not end up being played (if error occurred for example). Use snd_showstart to see the sounds that are really played.\n" );
ConVar snd_report_stop_sound( "snd_report_stop_sound", "0", FCVAR_CHEAT, "If set to 1, report all sounds stopped with S_StopSound().\n" );
ConVar snd_report_loop_sound( "snd_report_loop_sound", "0", FCVAR_CHEAT, "If set to 1, report all sounds that just looped.\n" );
ConVar snd_report_format_sound( "snd_report_format_sound", "0", FCVAR_CHEAT, "If set to 1, report all sound formats.\n" );
ConVar snd_report_verbose_error( "snd_report_verbose_error", "0", FCVAR_CHEAT, "If set to 1, report more error found when playing sounds.\n" );

static ConVar snd_hrtf_distance_behind("snd_hrtf_distance_behind", "100", FCVAR_ARCHIVE, "HRTF calculations will calculate the player as being this far behind the camera\n");


// store all played sounds for eliminating unplayed sounds for optimizations
ConVar snd_store_filepaths("snd_store_filepaths", "");
CUtlDict <int, int> g_StoreFilePaths;

enum ESndMergeMethod
{
	SND_MERGE_SUMANDCLIP = 0,
	SND_MERGE_MAX,
	SND_MERGE_AVG,
	SND_MERGE_COUNT
};
static ESndMergeMethod g_SndMergeMethod;

// =======================================================================
// Internal sound data & structures
// =======================================================================

ConVar snd_max_same_sounds( "snd_max_same_sounds", "4", FCVAR_CHEAT );
ConVar snd_max_same_weapon_sounds( "snd_max_same_weapon_sounds", "3", FCVAR_CHEAT );

CScratchPad g_scratchpad;

channel_t   channels[MAX_CHANNELS];

int	total_channels = MAX_DYNAMIC_CHANNELS;
static int nShowDynamicChannelMax = 0;
static int nShowStaticChannelMax = 0;

CActiveChannels g_ActiveChannels;

static double g_LastSoundFrame = 0.0f;		// last full frame of sound
static double g_LastMixTime = 0.0f;			// last time we did mixing
static float g_EstFrameTime = 0.1f;			// estimated frame time running average

// x360 override to fade out game music when the user is playing music through the dashboard
static float g_DashboardMusicMixValue = 1.0f;
static float g_DashboardMusicMixTarget = 1.0f;
const float g_DashboardMusicFadeRate = 0.5f;	// Fades one half full-scale volume per second (two seconds for complete fadeout)

float S_GetDashboarMusicMixValue()
{
	return g_DashboardMusicMixValue;
}

// This is a hack to prevent audio from being referenced during a load.
bool g_bPreventSound = false;

// global pitch scale
static float g_flPitchScale = 1.0f;

// this is used to enable/disable music playback on x360 when the user selects his own soundtrack to play
void S_EnableMusic( bool bEnable )
{
	if ( bEnable )
	{
		g_DashboardMusicMixTarget = 1.0f;
	}
	else
	{
		g_DashboardMusicMixTarget = 0.0f;
	}
}

CThreadMutex g_SndMutex;
#define THREAD_LOCK_SOUND() AUTO_LOCK( g_SndMutex )
CThreadFastMutex g_ActiveSoundListMutex;


void CActiveChannels::Add( channel_t *pChannel )
{
	Assert( pChannel->activeIndex == 0 );
	m_list[m_count] = pChannel - channels;
	m_count++;
	pChannel->activeIndex = m_count;
}

void CActiveChannels::Remove( channel_t *pChannel )
{
	if ( pChannel->activeIndex == 0 )
		return;
	int activeIndex = pChannel->activeIndex - 1;
	Assert( activeIndex >= 0 && activeIndex < m_count );
	Assert( pChannel == &channels[m_list[activeIndex]] );
	m_count--;
	// Not the last one?  Swap the last one with this one and fix its index
	if ( activeIndex < m_count )
	{
		m_list[activeIndex] = m_list[m_count];
		channels[m_list[activeIndex]].activeIndex = activeIndex+1;
	}
	pChannel->activeIndex = 0;
}


void CActiveChannels::GetActiveChannels( CChannelList &list ) const
{
	list.m_count = m_count;
	if ( m_count )
	{
		Q_memcpy( list.m_list, m_list, sizeof(m_list[0])*m_count );
	}
	list.m_hasSpeakerChannels = true;
	list.m_has11kChannels = true;
	list.m_has22kChannels = true;
	list.m_has44kChannels = true;
	list.m_hasDryChannels = true;
}

void CActiveChannels::CopyActiveSounds( CUtlVector<activethreadsound_t> &list ) const
{
	list.SetCount( m_count );
	for ( int i = 0; i < m_count; i++ )
	{
		list[i].m_nGuid = channels[m_list[i]].guid;
		list[i].m_flElapsedTime = 0.0f;
		CAudioMixer *pMixer = channels[m_list[i]].pMixer;
		if ( pMixer )
		{
			float flDivisor = ( pMixer->GetSource()->SampleRate() * channels[m_list[i]].pitch * 0.01f );
			if( flDivisor > 0.0f )
			{
				list[i].m_flElapsedTime = pMixer->GetSamplePosition() / flDivisor;
			}
		}
	}
}

channel_t * CActiveChannels::FindActiveChannelByGuid( int guid ) const
{
	for ( int i = 0; i < m_count; i++ )
	{
		channel_t *pChannel = &channels[ m_list[ i ] ];
		if ( pChannel->guid == guid )
		{
			return pChannel;
		}
	}
	return NULL;
}

void CActiveChannels::DumpChannelInfo( CUtlBuffer &buf )
{
	char nameBuf[ MAX_PATH ];
	for ( int i = 0; i < m_count; i++ )
	{
		channel_t *pChannel = &channels[ m_list[ i ] ];
		if ( pChannel->sfx != NULL )
		{
			buf.Printf( "%d. ch=%d %s p=%.2f,%.2f,%.2f v=%d s=%d l=%d  \n", i, m_list[ i ], pChannel->sfx->getname( nameBuf, sizeof(nameBuf) ), 
						pChannel->origin[0], pChannel->origin[1], pChannel->origin[2], pChannel->master_vol, pChannel->soundsource, pChannel->sfx->pSource->IsLooped() );
		}
	}
}

void CActiveChannels::Init()
{
	m_count = 0;
}

bool				snd_initialized = false;

Vector				listener_origin[ MAX_SPLITSCREEN_CLIENTS ];
Vector		listener_forward[ MAX_SPLITSCREEN_CLIENTS ];
Vector				listener_right[ MAX_SPLITSCREEN_CLIENTS ];
static Vector		listener_up[ MAX_SPLITSCREEN_CLIENTS ];
static bool			s_bIsListenerUnderwater;
static vec_t		sound_nominal_clip_dist=SOUND_NORMAL_CLIP_DIST;

// @TODO (toml 05-08-02): put this somewhere more reasonable
vec_t S_GetNominalClipDist()
{
	return sound_nominal_clip_dist;
}

#if USE_AUDIO_DEVICE_V1
int64			g_soundtime = 0;		// sample PAIRS output since start
double			g_soundtimeerror = 0.0;  // Error in sound time (used for synchronizing movie output sound to host_time)
#endif
int64  			g_paintedtime = 0; 		// sample PAIRS mixed since start

float			g_ClockSyncArray[NUM_CLOCK_SYNCS] = {0};
int64			g_SoundClockPaintTime[NUM_CLOCK_SYNCS] = {0};

// default 30ms
ConVar snd_delay_sound_shift( "snd_delay_sound_shift", "0.03" );
// this forces the clock to resync on the next delayed/sync sound
void S_SyncClockAdjust( clocksync_index_t syncIndex )
{
	g_ClockSyncArray[syncIndex] = 0;
	g_SoundClockPaintTime[syncIndex] = 0;
}

float S_ComputeDelayForSoundtime( float soundtime, clocksync_index_t syncIndex )
{
	// reset clock and return 0
	if ( g_ClockSyncArray[syncIndex] == 0 )
	{
		// Put the current time marker one tick back to impose a minimum delay on the first sample
		// this shifts the drift over so the sounds are more likely to delay (rather than skip)
		// over the burst
		// NOTE: The first sound after a sync MUST have a non-zero delay for the delay channel
		// detection logic to work (otherwise we keep resetting the clock)
		g_ClockSyncArray[syncIndex] = soundtime - host_state.interval_per_tick;
		g_SoundClockPaintTime[syncIndex] = g_paintedtime;
	}

	// how much time has passed in the game since we did a clock sync?
	float gameDeltaTime = soundtime - g_ClockSyncArray[syncIndex];

	// how many samples have been mixed since we did a clock sync?
	int paintedSamples = g_paintedtime - g_SoundClockPaintTime[syncIndex];
	int dmaSpeed = g_AudioDevice->SampleRate();
	int gameSamples = (gameDeltaTime * dmaSpeed);
	int delaySamples = gameSamples - paintedSamples;
	float delay = delaySamples / float(dmaSpeed);

	if ( gameDeltaTime < 0 || fabs(delay) > 0.200f )
	{
		// Note that the equations assume a correlation between game time and real time
		// some kind of clock error.  This can happen with large host_timescale or when the 
		// framerate hitches drastically (game time is a smaller clamped value wrt real time).  
		// The current sync estimate has probably drifted due to this or some other problem, recompute.
		//Msg("Clock ERROR!: %.2f %.2f\n", gameDeltaTime, delay);
		S_SyncClockAdjust(syncIndex);
		return 0;
	}
	return delay + snd_delay_sound_shift.GetFloat();
}

static	int		s_buffers = 0;
static	int		s_oldsampleOutCount = 0;
static	float	s_lastsoundtime = 0.0f;

bool s_bOnLoadScreen = false;

static CClassMemoryPool< CSfxTable > s_SoundPool( MAX_SFX );
struct SfxDictEntry
{
	CSfxTable *pSfx;
};

static CUtlMap< FileNameHandle_t, SfxDictEntry > s_Sounds( 0, 0, DefLessFunc( FileNameHandle_t ) );

CThreadFastMutex g_SoundMapMutex;

class CDummySfx : public CSfxTable
{
public:
	virtual const char *getname( char *pBuf, size_t bufLen )
	{
		V_strncpy( pBuf, name, bufLen );
		return pBuf;
	}

	void setname( const char *pName )
	{
		Q_strncpy( name, pName, sizeof( name ) );
		OnNameChanged(name);
	}

private:
	char name[MAX_PATH];
};

static CDummySfx dummySfx;

CSfxTable *S_DummySfx( const char *name )
{
	dummySfx.setname( name );
	return &dummySfx;
}


// returns true if ok to procede with TraceRay calls
bool SND_IsInGame( void )
{
	return GetBaseLocalClient().IsActive();
}


CSfxTable::CSfxTable()
{
	m_namePoolIndex = s_Sounds.InvalidIndex();
	pSource = NULL;
	m_bUseErrorFilename = false;
	m_bIsUISound = false;
	m_bIsMusic = false;
	m_bIsLateLoad = false;
	m_bMixGroupsCached = false;
	m_bIsCreatedByQueuedLoader = false;
	m_pDebugName = NULL;
}


void CSfxTable::SetNamePoolIndex( int index )
{
	m_namePoolIndex = index;
	char nameBuf[MAX_PATH];
	if ( m_namePoolIndex != s_Sounds.InvalidIndex() )
	{
		OnNameChanged(getname(nameBuf,sizeof(nameBuf)));
	}
#ifdef _DEBUG
	m_pDebugName = strdup( getname(nameBuf, sizeof(nameBuf)) );
#endif
}

extern int g_cgrouprules;

void CSfxTable::OnNameChanged( const char *pName )
{
	if ( pName && g_cgrouprules )
	{
		char szString[MAX_PATH];
		Q_strncpy( szString, pName, sizeof(szString) );
		Q_FixSlashes( szString, '/' );
		V_strlower( szString );
		m_mixGroupCount = MXR_GetMixGroupListFromDirName( szString, m_mixGroupList, ARRAYSIZE(m_mixGroupList) );
		m_bIsMusic = false;
		for ( int i = 0; i < m_mixGroupCount; i++ )
		{
			if ( MXR_IsMusicGroup( m_mixGroupList[i] ) )
			{
				m_bIsMusic = true;
				break;
			}
		}
		m_bMixGroupsCached = true;
	}
	else
	{
		m_mixGroupCount = 0;
		m_bMixGroupsCached = false;
	}
}

//-----------------------------------------------------------------------------
// Returns the decorated name. Cannot be used nested more than a few levels.
//-----------------------------------------------------------------------------
const char *CSfxTable::getname( char *pBuf, size_t bufLen )
{
	if ( s_Sounds.InvalidIndex() != m_namePoolIndex )
	{
		// based on pix capture, prior version using va() causing extra copies, was too costly
		// purposely doing it here
		// using va() was also very risky, naive code could easily get pointer contents changed
		g_pFileSystem->String( s_Sounds.Key( m_namePoolIndex ), pBuf, bufLen );
		return pBuf;
	}
	return NULL;
}

FileNameHandle_t CSfxTable::GetFileNameHandle()
{
	if ( s_Sounds.InvalidIndex() != m_namePoolIndex )
	{
		return s_Sounds.Key( m_namePoolIndex );
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// Returns the file name, sound prefix chars are stripped
//-----------------------------------------------------------------------------
const char *CSfxTable::GetFileName( char *pOutBuf, size_t bufLen )
{
	if ( IsGameConsole() && m_bUseErrorFilename )
	{
		// Redirecting error sounds to a valid empty wave, prevents a bad loading retry pattern during gameplay
		// which may event sounds skipped by preload, because they don't exist.
		return "common/null.wav";
	}

	const char *pName = getname(pOutBuf, bufLen);
	return pName ? PSkipSoundChars( pName ) : NULL;	
}

bool CSfxTable::IsPrecachedSound()
{
	char nameBuf[MAX_PATH];
	const char *pName = getname(nameBuf, sizeof(nameBuf));

	if ( sv.IsActive() )
	{
		// Server uses zero to mark invalid sounds
		return sv.LookupSoundIndex( pName ) != 0 ? true : false;
	}

	// Client uses -1
	// WE SHOULD FIX THIS!!!
	return ( GetBaseLocalClient().LookupSoundIndex( pName ) != -1 ) ? true : false;
}

float g_DuckScale = 1.0f;
int g_DuckScaleInt256 = 256;

// Structure used for fading in and out client sound volume.
typedef struct
{
	float		initial_percent;

	// How far to adjust client's volume down by.
	float		percent;  

	// GetHostTime() when we started adjusting volume
	float		starttime;   

	// # of seconds to get to faded out state
	float		fadeouttime; 
    // # of seconds to hold
	float		holdtime;  
	// # of seconds to restore
	float		fadeintime;
} soundfade_t;

static soundfade_t soundfade;  // Client sound fading singleton object
float g_flReplaySoundFade = 0.0f;
float g_flReplayMusicGain = 1.0f;

// 0)headphones 2)stereo speakers 4)quad 5)5point1 
// autodetected from windows settings
ConVar snd_surround( "snd_surround_speakers", "-1" );
#if USE_AUDIO_DEVICE_V1
ConVar snd_legacy_surround( "snd_legacy_surround", "0", FCVAR_ARCHIVE );
#endif

ConVar snd_noextraupdate( "snd_noextraupdate", "0" );
ConVar snd_show( "snd_show", "0", FCVAR_CHEAT, "Show sounds info");
void OnSndShowEdgeChanged( IConVar *var, const char *pOldValue, float flOldValue );
ConVar snd_show_print( "snd_show_print", "0", FCVAR_CHEAT, "Print to console the sounds that are normally printed on screen only. 1 = print to console and to screen; 2 = print only to console", OnSndShowEdgeChanged );
ConVar snd_show_filter( "snd_show_filter", "", FCVAR_CHEAT, "Limit debug sounds to those containing this substring" );
ConVar snd_find_channel( "snd_find_channel", "", 0, "Scan every channel to find the corresponding sound." );
ConVar snd_visualize ("snd_visualize", "0", FCVAR_CHEAT, "Show sounds location in world" );
ConVar snd_pitchquality( "snd_pitchquality", "1", FCVAR_ARCHIVE );		// 1) use high quality pitch shifters

// master volume
static ConVar volume( "volume", "1.0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE, "Sound volume", true, 0.0f, true, 1.0f );

// since the volume convar is manipulated by the UI it needs to be 0-1, this is a lower level control to limit that to a smaller
// range if necessary. On X360, we need to set this value to 0.5 to get similar level as the other X360 games.
// On PS3 and PC however, a value of 1.0 matches the other games. This is mostly for the game engine as it is not used by the movie.
static ConVar ui_volume_scale( "ui_volume_scale", IsPlatformX360() ? "0.5" : "1.0" );
// Similar knob for the movies.
// These values were given by Mike Morasky after various testing. 
ConVar movie_volume_scale( "movie_volume_scale", IsPlatformPS3() ? "0.9" : "1.0" );

// user configurable music volume - NOTE there is no music submix so this is pre-multiplied into each channel
ConVar snd_musicvolume_multiplier_inoverlay( "snd_musicvolume_multiplier_inoverlay", "0.1", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE, "Music volume multiplier when Steam Overlay is active", true, 0.0f, true, 1.0f );

ConVar snd_musicvolume( "snd_musicvolume", "0.7", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE, "Overall music volume", true, 0.0f, true, 1.0f );
ConVar snd_menumusic_volume( "snd_menumusic_volume", "1.0", FCVAR_ARCHIVE | FCVAR_RELEASE, "Relative volume of the main menu music." );
ConVar snd_roundstart_volume( "snd_roundstart_volume", "1.0", FCVAR_ARCHIVE | FCVAR_RELEASE, "Relative volume of round start music." );
ConVar snd_roundend_volume( "snd_roundend_volume", "1.0", FCVAR_ARCHIVE | FCVAR_RELEASE, "Relative volume of round end music." );
ConVar snd_mapobjective_volume( "snd_mapobjective_volume", "1.0", FCVAR_ARCHIVE | FCVAR_RELEASE, "Relative volume of map objective music." );
ConVar snd_tensecondwarning_volume( "snd_tensecondwarning_volume", "1.0", FCVAR_ARCHIVE | FCVAR_RELEASE, "Relative volume of ten second warning music." );
ConVar snd_deathcamera_volume("snd_deathcamera_volume", "1.0", FCVAR_ARCHIVE | FCVAR_RELEASE, "Relative volume of the death camera music.");

ConVar snd_mixahead( "snd_mixahead", "0.1", FCVAR_ARCHIVE );
ConVar snd_delay_for_choreo_enabled( "snd_delay_for_choreo_enabled", "1", 0, "Enables update of delay for choreo to compensate for IO latency." );
ConVar snd_delay_for_choreo_reset_after_N_milliseconds( "snd_delay_for_choreo_reset_after_N_milliseconds", "500", 0, "Resets the choreo latency after N milliseconds of VO not playing. Default is 500 ms." );

float	g_fDelayForChoreo = 0.0f;					// Delay in seconds added to VCD VO due to IO latency.
uint32	g_nDelayForChoreoLastCheckInMs = 0;			// Used to reset the choreo latency (if last check time + snd_delay_for_choreo_reset_after_N_milliseconds is greater than current time, and no choreo sound is playing, we can reset).
int		g_nDelayForChoreoNumberOfSoundsPlaying = 0;	// Number of choreo sound currently playing. Has to be zero for the reset of the latency to occur.

ConVar snd_mix_async( "snd_mix_async", "0" );
#ifdef _DEBUG
static ConCommand snd_mixvol("snd_mixvol", MXR_DebugSetMixGroupVolume, "Set named Mixgroup to mix volume.");
#endif

extern ConVar host_threaded_sound;

// vaudio DLL
IVAudio *vaudio = NULL;
CSysModule *g_pVAudioModule = NULL;

//-----------------------------------------------------------------------------
// Resource loading for sound
//-----------------------------------------------------------------------------
class CResourcePreloadSound : public CResourcePreload
{
public:
	CResourcePreloadSound()
	{
	}

	virtual void PrepareForCreate( bool bSameMap )
	{
		if ( !bSameMap )
		{
			// cannot support dynamic nature of sounds changing across maps due to deep fragmentation
			// always purge, tear all the sounds away, and put them back
			PurgeAllSounds();
		}
	}

	virtual bool CreateResource( const char *pName )
	{
		CSfxTable *pSfx = S_PrecacheSound( pName );
		if ( !pSfx )
		{
			return false;
		}
		return true;
	}

private:
	void PurgeAllSounds()
	{
		bool bSpew = ( g_pQueuedLoader->GetSpewDetail() & LOADER_DETAIL_PURGES ) != 0;
		char nameBuf[MAX_PATH];

		for ( int i = s_Sounds.FirstInorder(); i != s_Sounds.InvalidIndex(); i = s_Sounds.NextInorder( i ) )
		{
			CSfxTable *pSfx = s_Sounds[i].pSfx;
			if ( pSfx && pSfx->pSource )
			{
				if ( !pSfx->m_bIsCreatedByQueuedLoader )
				{
					// never purge sounds we do not own
					if ( bSpew )
					{
						Msg( "CResourcePreloadSound: Skipping: %s\n", pSfx->GetFileName(nameBuf, sizeof(nameBuf)) );
					}
					continue;
				}

				// sound was not part of preload, purge it
				if ( bSpew )
				{
					Msg( "CResourcePreloadSound: Purging: %s\n", pSfx->GetFileName(nameBuf, sizeof(nameBuf)) );
				}

				pSfx->pSource->CacheUnload();
				delete pSfx->pSource;
				pSfx->pSource = NULL;
			}
		}

		wavedatacache->Flush( true );
	}
};
static CResourcePreloadSound s_ResourcePreloadSound;

//-----------------------------------------------------------------------------
// Purpose: 
// Output : float
//-----------------------------------------------------------------------------
float S_GetMasterVolume( void )
{
	float scale = 1.0f;
	if ( soundfade.percent != 0 )
	{
		scale = clamp( (float)soundfade.percent / 100.0f, 0.0f, 1.0f );
		scale = 1.0f - scale;
	}
	return volume.GetFloat() * scale * ui_volume_scale.GetFloat() * ( 1.0f - g_flReplaySoundFade );
}


void S_SoundInfo_f(void)
{
#if !USE_AUDIO_DEVICE_V1
	g_AudioDevice->OutputDebugInfo();
#endif

	if ( !g_AudioDevice->IsActive() )
	{
		Msg( "Sound system not active\n" );
		return;
	}
	Msg( "total_channels: %d\n", total_channels);
	char nameBuf[MAX_PATH];

	if ( IsGameConsole() )
	{
		// dump a glimpse of the mixing state
		CChannelList list;
		g_ActiveChannels.GetActiveChannels( list );

		Msg( "\nActive Channels: %d\n", list.Count() );
		for ( int i = 0; i < list.Count(); i++ )
		{
			channel_t *pChannel = list.GetChannel( i );
			Msg( "Channel:%2d Mixer:%p %s\n", list.GetChannelIndex( i ), pChannel->pMixer, pChannel->sfx->GetFileName( nameBuf, sizeof( nameBuf ) ) );
		}
	}
	else
	{
		for (int i = MAX_DYNAMIC_CHANNELS; i<total_channels; i++)
		{
			channel_t *ch = &channels[i];
			if (ch->sfx != NULL)
			{
				Msg( "  %d: %s\n", i, ch->sfx->getname(nameBuf, sizeof(nameBuf)) );
			}
		}
	}
}

#if !USE_AUDIO_DEVICE_V1

static void OnSndVarChanged( IConVar *pVar, const char *pOldString, float flOldValue );
ConVar snd_mute_losefocus("snd_mute_losefocus", "1", FCVAR_ARCHIVE);
static ConVar windows_speaker_config("windows_speaker_config", "-1", FCVAR_RELEASE|FCVAR_ARCHIVE);
static ConVar sound_device_override( "sound_device_override", "", 0, "ID of the sound device to use" );

// maintain a list of available audio devices
static CAudioDeviceList g_AudioDeviceList;
static audio_device_init_params_t g_AudioDeviceInitParams;
static bool g_bRestartAudio = false;
void OnSndVarChanged( IConVar *pVar, const char *pOldString, float flOldValue )
{
	if ( !g_AudioDevice )
		return;

	ConVarRef var(pVar);
	// restart sound system so the change takes effect
	if ( var.GetInt() != int(flOldValue) || pVar == &sound_device_override )
	{
		if ( pVar == &snd_surround )
		{
			windows_speaker_config.SetValue( var.GetInt() );
		}
		if ( pVar == &snd_mute_losefocus )
		{
			// if the device can handle this, no need to restart
			if ( g_AudioDevice->SetShouldPlayWhenNotInFocus( !var.GetBool() ) )
				return;
		}
		g_bRestartAudio = true;
	}
}

void GetAudioDeviceList(CUtlVector<audio_device_description_t>& v)
{
	v = g_AudioDeviceList.m_list;
}

// this checks for device errors or configuration changes
void S_CheckDevice()
{
	// any errors?
	bool bRestart = Audio_PollErrorEvents() || g_bRestartAudio;
	g_bRestartAudio = false;
	// current device removed?  New default device installed?
	if ( g_AudioDevice && g_AudioDeviceList.UpdateDeviceList() )
	{
		const wchar_t *pDeviceToCreate = g_AudioDeviceList.GetDeviceToCreate( g_AudioDeviceInitParams );
		const wchar_t *pCurrent = g_AudioDevice->GetDeviceID();
		if ( !g_AudioDevice->IsActive() || V_wcscmp( pDeviceToCreate, pCurrent ) )
		{
			bRestart = true;
		}
	}

	// error or device change, restart audio
	if ( bRestart )
	{
		g_pSoundServices->RestartSoundSystem();
	}
}

void S_GetAudioDeviceList( CUtlVector<audio_device_description_t> &audioList )
{
	audioList.RemoveAll();
	if ( g_AudioDeviceList.m_nSubsystem == AUDIO_SUBSYSTEM_XAUDIO && g_AudioDeviceList.m_list.Count() > 0 )
	{
		audioList.AddToTail();
		audioList[0].InitAsNullDevice();
		V_sprintf_safe( audioList[0].m_friendlyName, "#OS_Default_Device" );
		audioList[0].m_nSubsystemId = AUDIO_SUBSYSTEM_XAUDIO;
		audioList[0].m_bIsAvailable = true;
	}
	for ( int i = 0; i < g_AudioDeviceList.m_list.Count(); i++ )
	{

		if ( g_AudioDeviceList.m_list[i].m_bIsAvailable )

		{
			audioList.AddToTail(g_AudioDeviceList.m_list[i]);
		}
	}
}


eSubSystems_t GetDefaultAudioSubsystem()
{
	eSubSystems_t nSubsystem = AUDIO_SUBSYSTEM_XAUDIO;
#if IS_WINDOWS_PC
	if ( CommandLine()->CheckParm( "-directsound" ) )
	{
		nSubsystem = AUDIO_SUBSYSTEM_DSOUND;
	}
#endif
	return nSubsystem;
}

CON_COMMAND( sound_device_list, "Lists all available audio devices." )
{
	g_AudioDeviceList.UpdateDeviceList();

	int nDeviceCount = g_AudioDeviceList.m_list.Count();
	Msg( "Found %d available audio devices\n", nDeviceCount );
	for ( int i = 0; i < nDeviceCount; i++ )
	{
		char deviceId[256];
		V_wcstostr( g_AudioDeviceList.m_list[i].m_deviceName, -1, deviceId, sizeof(deviceId) );
		Msg( "%d) %s (%d output channels) [%s]", i+1, g_AudioDeviceList.m_list[i].m_friendlyName, g_AudioDeviceList.m_list[i].m_nChannelCount, deviceId );
		if ( g_AudioDeviceList.m_list[i].m_bIsDefault )
		{
			Msg( " ** DEFAULT DEVICE **" );
		}
		Msg("\n");
	}
}

#endif

/*
================
S_Startup
================
*/

void S_Startup( void )
{
	if ( !snd_initialized )
		return;

	static bool bFirst = true;

	if ( bFirst )
	{
#if !USE_AUDIO_DEVICE_V1
		snd_mute_losefocus.InstallChangeCallback( &OnSndVarChanged );
		sound_device_override.InstallChangeCallback( &OnSndVarChanged );
		snd_surround.InstallChangeCallback( &OnSndVarChanged );
#endif

#if IS_WINDOWS_PC
		SetupWindowsMixerPreferences();
#endif
		bFirst = false;
	}

	if ( !g_AudioDevice )
	{
#if USE_AUDIO_DEVICE_V1
		g_AudioDevice = IAudioDevice::AutoDetectInit();
		if ( !g_AudioDevice )
		{
			Error( "Unable to init audio" );
		}
#else
		extern HWND* pmainwindow;

		eSubSystems_t nSubsystem = GetDefaultAudioSubsystem();
		g_AudioDeviceList.BuildDeviceList( nSubsystem );
		Assert( g_AudioDeviceList.IsValid() );

		g_AudioDeviceInitParams.Defaults();
		g_AudioDeviceInitParams.m_bPlayEvenWhenNotInFocus = !snd_mute_losefocus.GetBool();
		int nSpeakerConfig = windows_speaker_config.GetInt();
		if ( nSpeakerConfig >= 0 )
		{
			g_AudioDeviceInitParams.OverrideSpeakerConfig( nSpeakerConfig );
		}
		g_AudioDeviceInitParams.m_pWindowHandle = *pmainwindow;
		// enough buffer to mix 150ms (+1 buffer to round up)
		g_AudioDeviceInitParams.m_nOutputBufferCount = (int(0.150f * SOUND_DMA_SPEED) / MIX_BUFFER_SIZE) + 1;
		const char *pUser = sound_device_override.GetString();
		audio_device_description_t *pDevice = g_AudioDeviceList.FindDeviceById( pUser );
		if ( pDevice )
		{
			g_AudioDeviceInitParams.OverrideDevice( pDevice );
		}
		g_AudioDevice = g_AudioDeviceList.CreateDevice( g_AudioDeviceInitParams );
#endif
	}
}

static ConCommand play("play", S_Play, "Play a sound.", FCVAR_SERVER_CAN_EXECUTE );
static ConCommand play_hrtf("play_hrtf", S_PlayHRTF, "Play a sound with HRTF spatialization.", FCVAR_SERVER_CAN_EXECUTE);
static ConCommand playflush( "playflush", S_Play, "Play a sound, reloading from disk in case of changes." );
static ConCommand playvol( "playvol", S_PlayVol, "Play a sound at a specified volume." );
static ConCommand speak( "speak", S_Say, "Play a constructed sentence." );
static ConCommand stopsound( "stopsound", S_StopAllSoundsC, 0, FCVAR_CHEAT);		// Marked cheat because it gives an advantage to players minimizing ambient noise.
static ConCommand soundlist( "soundlist", S_SoundList, "List all known sounds." );
static ConCommand soundinfo( "soundinfo", S_SoundInfo_f, "Describe the current sound device." );

bool IsValidSampleRate( int rate )
{
	return rate == SOUND_11k || rate == SOUND_22k || rate == SOUND_44k;
}


void VAudioInit()
{
	if ( IsPC() && !g_pVAudioModule )
	{
		if ( !IsPosix() )
		{
			g_pFileSystem->GetLocalCopy( "mss32.dll" ); // vaudio_miles.dll will load this...
		}
		
		g_pVAudioModule = FileSystem_LoadModule( "vaudio_miles" );
		if ( g_pVAudioModule )
		{
			CreateInterfaceFn vaudioFactory = Sys_GetFactory( g_pVAudioModule );
			vaudio = (IVAudio *)vaudioFactory( VAUDIO_INTERFACE_VERSION, NULL );
		}
	}
}
/*
================
S_Init
================
*/
#ifdef _PS3
// On PS3 sound can only initialize once
enum Ps3SoundState_t
{
	PS3_SOUND_NOT_INITIALIZED,
	PS3_SOUND_INITIALIZED,
	PS3_SOUND_SHUTDOWN
};
static Ps3SoundState_t s_ePs3SoundState = PS3_SOUND_NOT_INITIALIZED;
#endif

void S_Init( void )
{
#ifdef _PS3
	if ( s_ePs3SoundState == PS3_SOUND_NOT_INITIALIZED )
	{
		s_ePs3SoundState = PS3_SOUND_INITIALIZED;
	}
	else
	{
		if ( s_ePs3SoundState != PS3_SOUND_INITIALIZED )
		{
			Warning( "ERROR: PS3 sound system cannot be initialized again (state %d)!\n", s_ePs3SoundState );
		}
		return;
	}
#endif

	if ( sv.IsDedicated() )
	{
		TRACEINIT( audiosourcecache->Init( host_parms.memsize >> 2 ), audiosourcecache->Shutdown() );
		return;
	}

	DevMsg( "Sound Initialization: Start\n" );

	// KDB: init sentence array
	TRACEINIT( VOX_Init(), VOX_Shutdown() );	

	if ( IsPC() )
	{
		VAudioInit();
	}

#ifdef _PS3
	// even if we do have sound, do we still have to Init mp3dec ? E.g. because it's logically a decoder, not a sound service. It's not clear.
	for ( int i = 0 ; i < NUMBER_OF_MP3_DECODER_SLOTS ; ++i )
	{
		g_mp3dec[i].Init();
	}
#endif

	if ( CommandLine()->CheckParm( "-nosound" ) )
	{
		g_AudioDevice = Audio_GetNullDevice();
		return;
	}
	
	snd_initialized = true;

	g_ActiveChannels.Init();
	S_Startup();

	MIX_InitAllPaintbuffers();

	SND_InitScaletable();

	MXR_LoadAllSoundMixers();

	g_pSoundOperatorSystem->Init();

	S_StopAllSounds( true );

	TRACEINIT( audiosourcecache->Init( host_parms.memsize >> 2 ), audiosourcecache->Shutdown() );

	AllocDsps( true );

	if ( IsGameConsole() )
	{
		g_pQueuedLoader->InstallLoader( RESOURCEPRELOAD_SOUND, &s_ResourcePreloadSound );
	}

	DevMsg( "Sound Initialization: Finish, Sampling Rate: %i\n", g_AudioDevice->SampleRate() );

#ifdef _X360
	BOOL bPlaybackControl;
	// get initial state of the x360 media player
	if ( XMPTitleHasPlaybackControl( &bPlaybackControl ) == ERROR_SUCCESS )
	{
		S_EnableMusic(bPlaybackControl!=0);
	}
#if defined( BINK_ENABLED_FOR_CONSOLE ) && defined(BINK_VIDEO)
	bik->HookXAudio();
#endif
#endif

#if defined( _PS3 ) && defined( BINK_ENABLED_FOR_CONSOLE )
	bik->SetPS3SoundDevice( g_AudioDevice->DeviceChannels() );
#endif  // _PS3 && BINK_ENABLED_FOR_CONSOLE

}

void DumpFilePaths(const char *filename);

void ShutdownPhononThread();

// =======================================================================
// Shutdown sound engine
// =======================================================================
void S_Shutdown(void)
{
#ifdef _PS3
	if ( s_ePs3SoundState == PS3_SOUND_INITIALIZED )
	{
		s_ePs3SoundState = PS3_SOUND_SHUTDOWN;
		Msg( "PS3 sound system is shutting down...\n" );
	}
	else
	{
		Warning( "ERROR: PS3 sound system cannot shutdown again (state %d)!\n", s_ePs3SoundState );
		return;
	}
#endif

	if ( !sv.IsDedicated() )
	{

#if !defined( _X360 )
		if ( VoiceTweak_IsStillTweaking() )
		{
			VoiceTweak_EndVoiceTweakMode();
		}
#endif

		// dump a complete list of audio files played during this game
#ifndef _PS3
		if ( IsPC() && snd_store_filepaths.GetString()[ 0 ])
		{
			/*time_t ltime;
			time(&ltime);
			localtime(&ltime);*/

#ifdef WIN32
			SYSTEMTIME time;
			GetLocalTime(&time);
		
			char filename[64];
			Q_snprintf( filename, 64, "soundlog_%i_%02i_%02i_%02i_%02i.txt", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute );
#else
			time_t timet = time( NULL );
			struct tm *tm  = localtime( &timet );
			char filename[32];
			Q_snprintf( filename, 32, "soundlog_%i_%02i_%02i_%02i_%02i.txt", tm->tm_year, tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min );
			
#endif
			DumpFilePaths(filename);
		}
#endif

		S_StopAllSounds( true );
		S_ShutdownMixThread();
		ShutdownPhononThread();



		SNDDMA_Shutdown();

		for ( int i = s_Sounds.FirstInorder(); i != s_Sounds.InvalidIndex(); i = s_Sounds.NextInorder( i ) )
		{
			if ( s_Sounds[i].pSfx )
			{
				delete s_Sounds[i].pSfx->pSource;
				s_Sounds[i].pSfx->pSource = NULL;
			}
		}
		s_Sounds.RemoveAll();
		s_SoundPool.Clear();

		// release DSP resources
		FreeDsps( true );

		MXR_ReleaseMemory();

		g_pSoundOperatorSystem->Shutdown();

		// release sentences resources
		TRACESHUTDOWN( VOX_Shutdown() );
		
		if ( IsPC() )
		{
			// shutdown vaudio
			if ( vaudio )
				delete vaudio;

			FileSystem_UnloadModule( g_pVAudioModule );
			g_pVAudioModule = NULL;
			vaudio = NULL;
		}

		MIX_FreeAllPaintbuffers();
		snd_initialized = false;
		g_paintedtime = 0;
#if USE_AUDIO_DEVICE_V1
		g_soundtime = 0;
		g_soundtimeerror = 0.0;
#endif
		s_buffers = 0;
		s_oldsampleOutCount = 0;
		s_lastsoundtime = 0.0f;
#if !defined( _X360 )
		Voice_Deinit();
#endif
	}

	TRACESHUTDOWN( audiosourcecache->Shutdown() );
#ifdef _PS3
	for ( int i = 0 ; i < NUMBER_OF_MP3_DECODER_SLOTS ; ++i )
	{
		HandleRemainingFrameInfos( i, true );
		g_mp3dec[i].Shutdown();
	}
#endif
}

bool S_IsInitted()
{
	return snd_initialized;
}

// =======================================================================
// Load a sound
// =======================================================================

//-----------------------------------------------------------------------------
// Find or Alloc sfx based on name.
// On Alloc, sets optional pInCache to 0.
// On Find, sets optional pInCache to 1 if resident, otherwise 0.
//-----------------------------------------------------------------------------
CSfxTable *S_FindName( const char *szName, int *pInCache )
{
	int			i;
	CSfxTable	*sfx = NULL;
	char		szBuff[MAX_PATH];
	const char	*pName;

	if ( !szName )
	{
		Error( "S_FindName: NULL\n" );
	}

	pName = szName;
	if ( IsGameConsole() )
	{
		Q_strncpy( szBuff, pName, sizeof( szBuff ) );
		int len = Q_strlen( szBuff )-4;
		if ( len > 0 && !Q_strnicmp( szBuff+len, ".mp3", 4 ) )
		{
			// convert unsupported .mp3 to .wav
			Q_strcpy( szBuff+len, ".wav" );
		}
		pName = szBuff;

		if ( pName[0] == CHAR_STREAM )
		{
			// streaming (or not) is hardcoded to alternate criteria
			// prevent the same sound from creating disparate instances
			pName++;
		}
	}

	AUTO_LOCK( g_SoundMapMutex );

	// see if already loaded
	FileNameHandle_t fnHandle = g_pFileSystem->FindOrAddFileName( pName );
	i = s_Sounds.Find( fnHandle );
	if ( i != s_Sounds.InvalidIndex() )
	{
		sfx = s_Sounds[i].pSfx;
		Assert( sfx );
		if ( pInCache )
		{
			// indicate whether or not sound is currently in the cache.
			*pInCache = ( sfx->pSource && sfx->pSource->IsCached() ) ? 1 : 0;
		}
		return sfx;
	}
	else
	{
		SfxDictEntry entry;
		entry.pSfx = ( CSfxTable * )s_SoundPool.Alloc();

		Assert( entry.pSfx );

		i = s_Sounds.Insert( fnHandle, entry );
		sfx = s_Sounds[i].pSfx;

		sfx->SetNamePoolIndex( i );
		sfx->pSource = NULL;

		if ( pInCache )
		{
			*pInCache = 0;
		}
	}
	return sfx;
}

//-----------------------------------------------------------------------------
// S_LoadSound
//
// Check to see if wave data is in the cache. If so, return pointer to data.
// If not, allocate cache space for wave data, load wave file into temporary heap
// space, and dump/convert file data into cache.
//-----------------------------------------------------------------------------
double g_flAccumulatedSoundLoadTime = 0.0f;
CAudioSource *S_LoadSound( CSfxTable *pSfx, channel_t *ch, SoundError &soundError )
{
	VPROF( "S_LoadSound" );
	char nameBuf[MAX_PATH];
	soundError = SE_OK;

	const char *pSndName = pSfx->getname(nameBuf, sizeof(nameBuf));
	if ( !pSndName )
	{
		soundError = SE_CANT_GET_NAME;
		return NULL;
	}

	const char *pSndFilename = PSkipSoundChars( pSndName );

	if ( !pSfx->pSource )
	{
		if ( IsGameConsole() )
		{
			if ( SND_IsInGame() && !g_pQueuedLoader->IsMapLoading() )
			{
				// sound should be present (due to reslists), but NOT allowing a load hitch during gameplay 
				// loading a sound during gameplay is a bad experience, causes a very expensive sync i/o to fetch the header
				// and in the case of a memory wave, the actual audio data
				bool bFound = false;
				if ( !pSfx->m_bIsLateLoad )
				{
					if ( pSndName != pSndFilename )
					{
						// the sound might already exist as an undecorated audio source
						FileNameHandle_t fnHandle = g_pFileSystem->FindOrAddFileName( pSndFilename );
						int i = s_Sounds.Find( fnHandle );
						if ( i != s_Sounds.InvalidIndex() )
						{
							CSfxTable *pOtherSfx = s_Sounds[i].pSfx;
							Assert( pOtherSfx );
							CAudioSource *pOtherSource = pOtherSfx->pSource;
							if ( pOtherSource && pOtherSource->IsCached() )
							{
								// Can safely let the "load" continue because the headers are expected to be in the preload
								// that are now persisted and the wave data cache will find an existing audio buffer match,
								// so no sync i/o should occur from either.
								bFound = true;
							}
						}
					}

					if ( !bFound )
					{
						// warn once
						DevWarning( "[Sound] S_LoadSound: Late load '%s', skipping.\n", pSndName ); 
						pSfx->m_bIsLateLoad = true;
					}
				}

				if ( !bFound )
				{
					soundError = SE_SKIPPED;
					return NULL;
				}
			}
			else if ( pSfx->m_bIsLateLoad )
			{
				// outside of gameplay, let the load happen
				pSfx->m_bIsLateLoad = false;
			}
		}

		double st = Plat_FloatTime();

		bool bStream = false;
		bool bUserVox = false;

		// sound chars can explicitly categorize usage
		bStream = TestSoundChar( pSndName, CHAR_STREAM );
		if ( !bStream )
		{
			bUserVox = TestSoundChar( pSndName, CHAR_USERVOX );
		}

		// stream music
		if ( !bStream && !bUserVox )
		{
			bStream = V_stristr( pSndName, "music" ) != NULL;
		}

		// override streaming
		if ( IsGameConsole() )
		{
			// these are the ONLY non-streaming static sounds
			const char *s_CriticalSounds[] = 
			{
				"common/",
				"items/",
				"ui/",
				"weapons/",
				"vfx/fizzler_lp_01",
				"player/player_fall_whoosh_lp_01",
				"ambient/machines/portalgun_rotate_loop1"
			};

			// can further refine critical sounds and ensure these stream
			const char *s_NonCriticalSounds[] = 
			{
				// forcing the streamer to do more work all these static sounds now stream
				// freed memory devoted to more textures
				"player/footsteps",
#if defined( CSTRIKE15 )
				"weapons/",
#endif
				"gamestartup",
			};

			// stream everything but critical sounds
			bStream = true;
			char cleanName[MAX_PATH];
			V_strncpy( cleanName, pSndFilename, sizeof( cleanName ) );
			V_FixSlashes( cleanName, '/' );
			for ( int i = 0; bStream && i < ARRAYSIZE( s_CriticalSounds ); i++ )
			{
				if ( StringHasPrefix( cleanName, s_CriticalSounds[i] ) )
				{
					// never stream these, regardless of sound chars
					bStream = false;
				}
			}

			// some broad classified critical sounds can actually stream
			for ( int i = 0; !bStream && i < ARRAYSIZE( s_NonCriticalSounds ); i++ )
			{
				if ( V_stristr( cleanName, s_NonCriticalSounds[i] ) )
				{
					bStream = true;
				}
			}

#if defined( _X360 )
			// shutdown streaming sounds ONLY during the main menu while the installer might go active or is active
			if ( bStream && V_stristr( cleanName, "music/mainmenu" ) &&
				g_pXboxInstaller->IsInstallEnabled() && !g_pXboxInstaller->IsFullyInstalled() )
			{
				// installer only runs during main menu UI
				// cannot stream at all during installer
				// force this background ui music to not stream
				bStream = false;
			}
#endif
		}

		if ( bStream )
		{
			// setup as a streaming resource
			pSfx->pSource = Audio_CreateStreamedWave( pSfx );
		}
		else
		{
			if ( bUserVox )
			{
				if ( !IsGameConsole() )
				{
					pSfx->pSource = Voice_SetupAudioSource( ch->soundsource, ch->entchannel );
				}
				else
				{
					// not supporting
					Assert( 0 );
				}
			}
			else
			{
				// load all into memory directly
				pSfx->pSource = Audio_CreateMemoryWave( pSfx );
			}
		}
		
		if ( IsGameConsole() )
		{
			// need to track these
			pSfx->m_bIsCreatedByQueuedLoader = g_pQueuedLoader->IsMapLoading();
		}

		double ed = Plat_FloatTime();
		g_flAccumulatedSoundLoadTime += ( ed - st );
	}
	else 
	{
		pSfx->pSource->CheckAudioSourceCache();
	}

	if ( !pSfx->pSource )
	{
		soundError = SE_NO_SOURCE_SETUP;
		return NULL;
	}

	// first time to load?  Create the mixer
	if ( ch && !ch->pMixer )
	{
		ch->pMixer = pSfx->pSource->CreateMixer(ch->initialStreamPosition, ch->skipInitialSamples, ch->flags.m_bUpdateDelayForChoreo, soundError, ch->wavtype == CHAR_HRTF ? &ch->hrtf : nullptr);
		if ( !ch->pMixer )
		{
			return NULL;
		}
	}

	return pSfx->pSource;
}

//-----------------------------------------------------------------------------
//	S_PrecacheSound
//
//	Reserve space for the name of the sound in a global array.
//	Load the data for the non-streaming sound. Streaming sounds
//	defer loading of data until just before playback.
//-----------------------------------------------------------------------------
CSfxTable *S_PrecacheSound( const char *name )
{
	if ( !g_AudioDevice )
		return NULL;

	if ( !g_AudioDevice->IsActive() )
		return NULL;

	CSfxTable *sfx = S_FindName( name, NULL );
	if ( sfx )
	{
		// cache sound
		SoundError soundError;
		S_LoadSound( sfx, NULL, soundError );
	}
	else
	{
		Assert( !"S_PrecacheSound:  Failed to create sfx" );
	}

	return sfx;
}


void S_InternalReloadSound( CSfxTable *sfx )
{
	if ( !sfx || !sfx->pSource )
		return;

	sfx->pSource->CacheUnload();

	delete sfx->pSource;
	sfx->pSource = NULL;

	char pExt[10];
	char nameBuf[MAX_PATH];
	Q_ExtractFileExtension( sfx->getname(nameBuf,sizeof(nameBuf)), pExt, sizeof(pExt) );
	int nSource = !Q_stricmp( pExt, "mp3" ) ? CAudioSource::AUDIO_SOURCE_MP3 : CAudioSource::AUDIO_SOURCE_WAV;
//	audiosourcecache->RebuildCacheEntry( nSource, sfx->IsPrecachedSound(), sfx );
	audiosourcecache->GetInfo( nSource, sfx->IsPrecachedSound(), sfx ); // Do a size/date check and rebuild the cache entry if necessary.
}


//-----------------------------------------------------------------------------
//	Refresh a sound in the cache
//-----------------------------------------------------------------------------
void S_ReloadSound( const char *name )
{
	if ( IsGameConsole() )
	{
		// not supporting
		Assert( 0 );
		return;
	}

	if ( !g_AudioDevice )
		return;

	if ( !g_AudioDevice->IsActive() )
		return;

	CSfxTable *sfx = S_FindName( name, NULL );
#ifdef _DEBUG
	if ( sfx )
	{
		char nameBuf[MAX_PATH];
		Assert( Q_stricmp( sfx->getname(nameBuf, sizeof(nameBuf)), name ) == 0 );
	}
#endif
	
	S_InternalReloadSound( sfx );
}


// See comments on CL_HandlePureServerWhitelist for details of what we're doing here.
void S_ReloadFilesInList( IFileList *pFilesToReload )
{
	if ( !IsPC() )
		return;

	S_StopAllSounds( true );
	wavedatacache->Flush();
	
	// Reload any sounds that are:
	//	 a) not from the Steam caches
	//	 b) not in the whitelist
	int iLast = s_Sounds.LastInorder();
	for ( int i = s_Sounds.FirstInorder(); i != iLast; i = s_Sounds.NextInorder( i ) )
	{
		FileNameHandle_t fnHandle = s_Sounds.Key( i );
		char filename[MAX_PATH * 3];
		if ( !g_pFileSystem->String( fnHandle, filename, sizeof( filename ) ) )
		{
			Assert( !"S_HandlePureServerWhitelist - can't get a filename." );
			continue;
		}
	
		// If the file isn't cached in yet, then the filesystem hasn't touched its file, so don't bother.
		CSfxTable *sfx = s_Sounds[i].pSfx;
		if ( sfx )
		{
			char fullFilename[MAX_PATH*2];
			if ( IsSoundChar( filename[0] ) )
				Q_snprintf( fullFilename, sizeof( fullFilename ), "sound/%s", &filename[1] );
			else
				Q_snprintf( fullFilename, sizeof( fullFilename ), "sound/%s", filename );
			
			S_InternalReloadSound( sfx );
		}
	}
}

//-----------------------------------------------------------------------------
// Unfortunate confusing terminology.
// Here prefetching means hinting to the audio source (which may be a stream)
// to get its async data in flight.
//-----------------------------------------------------------------------------
void S_PrefetchSound( char const *name, bool bPlayOnce )
{
	CSfxTable	*sfx;

	if ( !g_AudioDevice )
		return;

	if ( !g_AudioDevice->IsActive() )
		return;

	sfx = S_FindName( name, NULL );
	if ( sfx )
	{
		// cache sound
		SoundError soundError;
		S_LoadSound( sfx, NULL, soundError );
	}

	if ( !sfx || !sfx->pSource )
	{
		return;
	}

	// hint the sound to start loading
	sfx->pSource->Prefetch();

	if ( bPlayOnce )
	{
		sfx->pSource->SetPlayOnce( true );
	}
}

void S_MarkUISound( CSfxTable *pSfx )
{
	pSfx->m_bIsUISound = true;
}

unsigned int RemainingSamples( channel_t *pChannel )
{
	if ( !pChannel || !pChannel->sfx || !pChannel->sfx->pSource )
		return 0;

	unsigned int timeleft = pChannel->sfx->pSource->SampleCount();

	if ( pChannel->sfx->pSource->IsLooped() )
	{
		return pChannel->sfx->pSource->SampleRate();
	}

	if ( pChannel->pMixer )
	{
		timeleft -= pChannel->pMixer->GetSamplePosition();
	}

	return timeleft;
}

// chooses the voice stealing algorithm
ConVar voice_steal("voice_steal", "2");

float ClosestListenerDistSqr( const Vector &check )
{
	float bestDSqr = FLT_MAX;
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		float distSqr = check.DistToSqr( listener_origin[ hh ] );
		if ( distSqr < bestDSqr )
		{
			bestDSqr = distSqr;
		}
	}
	return bestDSqr;
}
/*
=================
SND_StealDynamicChannel
Select a channel from the dynamic channel allocation area.  For the given entity, 
override any other sound playing on the same channel (see code comments below for
exceptions).
=================
*/
channel_t *SND_StealDynamicChannel(SoundSource soundsource, int entchannel, const Vector &origin, CSfxTable *sfx)
{
	int canSteal[MAX_DYNAMIC_CHANNELS];
	int canStealCount = 0;

	int sameSoundCount = 0;
	unsigned int sameSoundRemaining = 0xFFFFFFFF;
	int sameSoundIndex = -1;
	int sameVol = 0xFFFF;
	int availableChannel = -1;
	bool bDelaySame = false;

	// first pass to replace sounds on same ent/channel, and search for free or stealable channels otherwise
	for ( int ch_idx = 0; ch_idx < MAX_DYNAMIC_CHANNELS ; ++ch_idx )
	{
		channel_t *ch = &channels[ch_idx];
		
		if ( ch->activeIndex )
		{
			bool canStealThisChannel = ch->entchannel != CHAN_STREAM && ch->entchannel != CHAN_VOICE && ch->entchannel < CHAN_USER_BASE;
			canStealThisChannel = canStealThisChannel && (ch->entchannel < CHAN_VOICE_BASE || ch->entchannel >= CHAN_VOICE_BASE + VOICE_NUM_CHANNELS);
			// channel CHAN_AUTO never overrides sounds on same channel
			if ( entchannel != CHAN_AUTO )
			{
				int checkChannel = entchannel;
				if ( checkChannel == -1 )
				{
					if ( canStealThisChannel )
					{
						checkChannel = ch->entchannel;
					}
				}
				// delayed channels are never overridden
				if ( !ch->flags.delayed_start && 
					ch->soundsource == soundsource && 
					(soundsource != -1) && 
					ch->entchannel == checkChannel )
				{
					return ch;	// always override sound from same entity
				}
			}

			// Never steal the channel of a streaming sound that is currently playing or
			// voice over IP data that is playing or any sound on CHAN_VOICE( acting )
			if ( !canStealThisChannel )
				continue;

			// don't let monster sounds override player sounds
			if ( g_pSoundServices->IsPlayer( ch->soundsource ) && !g_pSoundServices->IsPlayer(soundsource) )
				continue;

			if ( ch->sfx == sfx )
			{
				int maxVolume = ChannelGetMaxVol( ch );
				// TERROR: prevent DSP from causing weapon sounds to be skipped
				if ( entchannel == CHAN_WEAPON )
				{
					maxVolume = 255;

					// TERROR: Allow each player's weapons to be stolen individually
					if ( ch->soundsource != soundsource )
					{
						continue;
					}
				}

				bDelaySame = ch->flags.delayed_start ? true : bDelaySame;
				sameSoundCount++;
			
				unsigned int remaining = RemainingSamples(ch);


				if ( maxVolume < sameVol || (maxVolume == sameVol && remaining < sameSoundRemaining) )
				{
					sameSoundIndex = ch_idx;
					sameVol = maxVolume;
					sameSoundRemaining = remaining;
				}
			}
			canSteal[canStealCount++] = ch_idx;
		}
		else
		{
			if ( availableChannel < 0 )
			{
				availableChannel = ch_idx;
			}
		}
	}

	
	// Limit the number of times a given sfx/wave can play simultaneously
	if ( voice_steal.GetInt() > 1 && sameSoundIndex >= 0 )
	{
		// if sounds of this type are normally delayed, then add an extra slot for stealing
		// NOTE: In HL2 these are usually NPC gunshot sounds - and stealing too soon will cut
		// them off early.  This is a safe heuristic to avoid that problem.  There's probably a better
		// long-term solution involving only counting channels that are actually going to play (delay included)
		// at the same time as this one.
		int maxSameSounds = bDelaySame ? snd_max_same_sounds.GetInt() + 1 : snd_max_same_sounds.GetInt();
		if ( entchannel == CHAN_WEAPON )
		{
			maxSameSounds = bDelaySame ? snd_max_same_weapon_sounds.GetInt() + 1 : snd_max_same_weapon_sounds.GetInt();
		}
		float distSqr = 0.0f;
		if ( sfx->pSource )
		{
			if ( sfx->pSource->IsLooped() )
			{
				maxSameSounds = 3;
			}

			distSqr = ClosestListenerDistSqr( origin );
		}

		// don't play more than N copies of the same sound, steal the quietest & closest one otherwise
		if ( sameSoundCount >= maxSameSounds )
		{
			channel_t *ch = &channels[sameSoundIndex];
			// you're already playing a closer version of this sound, don't steal
			if ( distSqr > 0.0f && 
				ClosestListenerDistSqr( ch->origin ) < distSqr && 
				entchannel != CHAN_WEAPON )
				return NULL;

			// Msg("Sound playing %d copies, stole %s (%d) %i, %i, %u\n", sameSoundCount, ch->sfx->getname(), sameVol, ch->soundsource, soundsource, RemainingSamples(ch) );
	
			return ch;
		}
	}

	// if there's a free channel, just take that one - don't steal
	if ( availableChannel >= 0 )
		return &channels[availableChannel];

	// Still haven't found a suitable channel, so choose the one with the least amount of time left to play
	float life_left = FLT_MAX;
	int first_to_die = -1;
	bool bAllowVoiceSteal = voice_steal.GetBool();

	for ( int i = 0; i < canStealCount; i++ )
	{
		int ch_idx = canSteal[i];
		channel_t *ch = &channels[ch_idx];
		float timeleft = 0;
		if ( bAllowVoiceSteal )
		{
			// TERROR: don't steal looped sounds
			if ( ch->sfx && ch->sfx->pSource )
			{
				if ( ch->sfx->pSource->IsLooped() )
					continue;
			}

			int maxVolume = ChannelGetMaxVol( ch );
			if ( maxVolume < 5 )
			{
				//Msg("Sound quiet, stole %s for %s\n", ch->sfx->getname(), sfx->getname() );
				return ch;
			}

			if ( ch->sfx && ch->sfx->pSource )
			{
				unsigned int sampleCount = RemainingSamples( ch );
				timeleft = (float)sampleCount / (float)ch->sfx->pSource->SampleRate();
			}

			// TERROR: bias weapon sounds longer so they don't get stolen when possible
			if ( ch->entchannel == CHAN_WEAPON )
			{
				timeleft += 5.0f;
			}
		}
		else
		{
			// UNDONE: Kill this when voice_steal 0,1,2 has been tested
			// UNDONE: This is the old buggy code that we're trying to replace
			if ( ch->sfx )
			{
				// basically steals the first one you come to
				timeleft = 1;	//ch->end - paintedtime
			}
		}

		if ( timeleft < life_left )
		{
			life_left = timeleft;
			first_to_die = ch_idx;
		}
	}
	if ( first_to_die >= 0 )
	{
		//Msg("Stole %s, timeleft %d\n", channels[first_to_die].sfx->getname(), life_left );
		return &channels[first_to_die];
	}

	return NULL;
}

channel_t *SND_PickDynamicChannel(SoundSource soundsource, int entchannel, const Vector &origin, CSfxTable *sfx)
{
	channel_t *pChannel = SND_StealDynamicChannel( soundsource, entchannel, origin, sfx );
	if ( !pChannel )
		return NULL;

	if ( pChannel->sfx )
	{
		// Don't restart looping sounds for the same entity
		CAudioSource *pSource = pChannel->sfx->pSource;
		if ( pSource )
		{
			if ( pSource->IsLooped() )
			{
				if ( pChannel->soundsource == soundsource && pChannel->entchannel == entchannel && pChannel->sfx == sfx )
				{
					// same looping sound, same ent, same channel, don't restart the sound
					return NULL;
				}
			}
		}
		// be sure and release previous channel
		// if sentence.
		//	("Stealing channel from %s\n", channels[first_to_die].sfx->getname() );
		if ( snd_report_verbose_error.GetBool() )
		{
			char sndname[MAX_PATH];
			Msg( "%s(%d): Stealing channel from sound '%s'.\n", __FILE__, __LINE__, pChannel->sfx->GetFileName( sndname, sizeof( sndname ) ) );
		}
		S_FreeChannel(pChannel);
	}
	return pChannel;
}

  			

/*
=====================
SND_PickStaticChannel
=====================
Pick an empty channel from the static sound area, or allocate a new
channel.  Only fails if we're at max_channels (128!!!) or if 
we're trying to allocate a channel for a stream sound that is 
already playing.

*/
channel_t *SND_PickStaticChannel(int soundsource, CSfxTable *pSfx)
{
	int i;
	channel_t *ch = NULL;

	// Check for replacement sound, or find the best one to replace
 	for (i = MAX_DYNAMIC_CHANNELS; i<total_channels; i++)
		if (channels[i].sfx == NULL)
			break;

	if (i < total_channels) 
	{
		// reuse an empty static sound channel
		ch = &channels[i];
	}
	else
	{
// 		const int MAX_CHANNELS_MESSAGE = (3 * ( MAX_CHANNELS ) ) / 4;
// 		if ( total_channels > MAX_CHANNELS_MESSAGE )
// 		{
// 			DevMsg( "Warning: more than 3/4 of all static channels have been used: > %d\n", total_channels - MAX_DYNAMIC_CHANNELS );

			// no empty slots, alloc a new static sound channel
			if (total_channels == MAX_CHANNELS)
			{
				Warning( "Error: Total static audio channels have been used: %d ", MAX_CHANNELS - MAX_DYNAMIC_CHANNELS );
				for (i = MAX_DYNAMIC_CHANNELS; i < total_channels; i++)
				{
					char buff[4096];
					channels[i].sfx->GetFileName(buff, sizeof(buff));
					Warning("%d, %s ", i, buff);
				}

				Warning("\n");
				

				static bool bFirst = true;
				if ( bFirst )
				{
					bFirst = false;
					S_SoundInfo_f();
				}
				return NULL;
			}
//		}

		// get a channel for the static sound
		ch = &channels[total_channels];
		if ( snd_report_verbose_error.GetBool() )
		{
			Msg( "%s(%d): Recycle channel index %d.\n", __FILE__, __LINE__, total_channels );
		}
		total_channels++;
	}
	return ch;
}

// &7DL Re-enabled, called from snd_dev_sdl.cpp
void S_SpatializeChannel( int nSlot, int volume[CCHANVOLUMES/2], int master_vol, const Vector *psourceDir, float gain, float mono )
{
	float lscale, rscale, scale;
	vec_t dotRight;
	Vector sourceDir = *psourceDir;

	dotRight = DotProduct(listener_right[ nSlot ], sourceDir);

	// clear volumes
	for (int i = 0; i < CCHANVOLUMES/2; i++)
		volume[i] = 0;

	if (mono > 0.0)
	{
		// sound has radius, within which spatialization becomes mono:

		// mono is 0.0 -> 1.0, from radius 100% to radius 50%

		// at radius * 0.5, dotRight is 0 (ie: sound centered left/right)
		// at radius * 1.0, dotRight == dotRight

		dotRight   *= (1.0 - mono);
	}

	rscale = 1.0 + dotRight;
	lscale = 1.0 - dotRight;

 // add in distance effect
	scale = gain * rscale / 2;
	volume[IFRONT_RIGHT] = (int) (master_vol * scale);

	scale = gain * lscale / 2;
	volume[IFRONT_LEFT] = (int) (master_vol * scale);

	volume[IFRONT_RIGHT] = clamp( volume[IFRONT_RIGHT], 0, 255 );
	volume[IFRONT_LEFT] = clamp( volume[IFRONT_LEFT], 0, 255 );

}

bool S_IsPlayerVoice( channel_t *pChannel )
{
	CSfxTable *sfx = pChannel->sfx;
	if ( !sfx )
		return false;

	CAudioSource *source = sfx->pSource;
	if ( !source )
		return false;

	return source->IsPlayerVoice();
}

bool S_IsMusic( channel_t *pChannel )
{
	if ( !pChannel->flags.bdry )
		return false;

	CSfxTable *sfx = pChannel->sfx;
	if ( !sfx )
		return false;

	CAudioSource *source = sfx->pSource;
	if ( !source )
		return false;

	// Don't save restore looping sounds as you can end up with an entity restarting them again and have 
	//  them accumulate, etc.
	if ( source->IsLooped() )
		return false;

	CAudioMixer *pMixer = pChannel->pMixer;
	if ( !pMixer )
		return false;

	if ( sfx->m_bIsMusic )
		return true;
	bool bIsMp3 = ( source->GetType() == CAudioSource::AUDIO_SOURCE_MP3 ) ? true : false;

	for ( int i = 0; i < 8; i++ )
	{
		if ( pChannel->mixgroups[i] != -1 )
		{
			char *pGroupName = MXR_GetGroupnameFromId( pChannel->mixgroups[i] );
			// HACK: Consider mp3s playing in the UI as music since the only cases of that 
			// currently are startup music files e.g. #ui/gamestartup1.mp3
			if ( !Q_strcmp( pGroupName, "Music" ) || (bIsMp3 && !Q_stricmp(pGroupName,"UI")))
			{
				return true;
			}
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: For save/restore of currently playing music
// Input  : list - 
//-----------------------------------------------------------------------------
void S_GetCurrentlyPlayingMusic( CUtlVector< musicsave_t >& musiclist )
{
	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );
	for ( int i = 0; i < list.Count(); i++ )
	{
		channel_t *pChannel = &channels[list.GetChannelIndex(i)];
		if ( !S_IsMusic( pChannel ) )
			continue;

		musicsave_t song;
		char nameBuf[MAX_PATH];
		Q_strncpy( song.songname, pChannel->sfx->getname(nameBuf,sizeof(nameBuf)), sizeof( song.songname ) );
		song.sampleposition = pChannel->pMixer->GetPositionForSave();
		song.master_volume = pChannel->master_vol;

		musiclist.AddToTail( song );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *song - 
//-----------------------------------------------------------------------------
void S_RestartSong( const musicsave_t *song )
{
	Assert( song );

	// Start the song
	CSfxTable *pSound = S_PrecacheSound( song->songname );
	if ( pSound )
	{
		StartSoundParams_t params;
		params.staticsound = true;
		params.soundsource = SOUND_FROM_WORLD;
		params.entchannel = CHAN_STATIC;
		params.pSfx = pSound;
		params.origin = vec3_origin;
		params.fvol = ( (float)song->master_volume / 255.0f );
		params.soundlevel = SNDLVL_NONE;
		params.flags = SND_NOFLAGS;
		params.pitch = PITCH_NORM;
		params.initialStreamPosition = song->sampleposition;

		S_StartSound( params );

		if ( IsPC() )
		{
			// Now find the channel this went on and skip ahead in the mixer
			for (int i = 0; i < total_channels; i++)
			{
				channel_t *ch = &channels[i]; 

				if ( !ch->pMixer ||
					 !ch->pMixer->GetSource() )
				{
					continue;
				}

				if ( ch->pMixer->GetSource() != pSound->pSource )
				{
					continue;
				}

				ch->pMixer->SetPositionFromSaved( song->sampleposition );
				break;
			}
		}
	}
}

bool S_ShouldSaveRestore( channel_t const* pChannel )
{
	// Invalid sound or audio source
	if( !pChannel->sfx || !pChannel->sfx->pSource )
		return false;

	// No mixer
	if( !pChannel->pMixer )
		return false;

	return pChannel->flags.m_bShouldSaveRestore;
}

void S_GetActiveSaveRestoreChannels( ChannelSaveVector& channelSaves )
{
	CChannelList channelList;
	g_ActiveChannels.GetActiveChannels( channelList );
	for( int i = 0; i < channelList.Count(); ++i )
	{
		channel_t const& channel = channels[channelList.GetChannelIndex(i)];
		if( S_ShouldSaveRestore( &channel ) )
		{
			channelsave channelSave;
			if( channel.m_nSoundScriptHash != SOUNDEMITTER_INVALID_HASH )
				V_strcpy( channelSave.soundName, g_pSoundEmitterSystem->GetSoundNameForHash( channel.m_nSoundScriptHash ) );
			else
				channel.sfx->getname( channelSave.soundName, sizeof( channelSave.soundName ) );

			channelSave.origin = channel.origin;
			channelSave.soundLevel = static_cast<soundlevel_t>( static_cast<int>( channel.m_flSoundLevel ) );
			channelSave.soundSource = channel.soundsource;
			channelSave.entChannel = channel.entchannel;
			channelSave.masterVolume = channel.master_vol;
			channelSave.pitch = channel.basePitch;

			if( channel.m_pStackList )
			{
				// Note: According to Mike Morasky, elapsed time should only matter for the update stack, so if
				//		 the stack list doesn't contain one, its value won't matter on restore.
				const CSosOperatorStack* pUpdateStack = channel.m_pStackList->GetStack( CSosOperatorStack::SOS_UPDATE );
				channelSave.opStackElapsedTime = pUpdateStack != NULL ? pUpdateStack->GetElapsedTime() : 0.0f;
				channelSave.opStackElapsedStopTime = channel.m_pStackList->GetElapsedStopTime();
			}
			else
			{
				channelSave.opStackElapsedTime = S_GetElapsedTime( &channel );
				channelSave.opStackElapsedStopTime = 0.0f;
			}
			
			channelSaves.AddToTail( channelSave );
		}
	}
}

channel_t* S_FindDuplicateChannel( StartSoundParams_t const& params )
{
	THREAD_LOCK_SOUND();

	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );
	channel_t* pDuplicateChannel = NULL;

	bool const shouldIgnoreName = (params.flags & SND_IGNORE_NAME) != 0;
	bool const isScriptSound = params.m_bIsScriptHandle && !shouldIgnoreName;
	float maxElapsedTime = -1.0f;

	for( int i = 0; i < list.Count(); ++i )
	{
		channel_t &currentChannel = channels[list.GetChannelIndex(i)];

		bool const soundMatches = isScriptSound ? currentChannel.m_nSoundScriptHash == params.m_nSoundScriptHash : 
								  shouldIgnoreName || currentChannel.sfx == params.pSfx;

		// If this is the same sound from the same source on the same entity channel
		if( currentChannel.soundsource == params.soundsource && 
			currentChannel.entchannel == params.entchannel &&
			soundMatches )
		{
			float const timeElapsed = S_GetElapsedTime( &currentChannel );

			// If this isn't a script sound or is the oldest one that isn't stopping
			if( !isScriptSound || 
				( currentChannel.m_pStackList && !currentChannel.m_pStackList->IsStopping() && timeElapsed > maxElapsedTime) )
			{
				// Consider this the duplicate
				maxElapsedTime = timeElapsed;
				pDuplicateChannel = &currentChannel;
			}
		}
	}

	return pDuplicateChannel;
}

void S_RestartChannel( channelsave const& channelSave )
{
	// Start the channel
	CSfxTable* pSound = S_PrecacheSound( channelSave.soundName );
	if( pSound )
	{
		StartSoundParams_t params;
		params.soundsource = channelSave.soundSource;
		params.entchannel = channelSave.entChannel;
		params.pSfx = pSound;
		params.origin = channelSave.origin;
		params.soundlevel = channelSave.soundLevel;
		params.pitch = channelSave.pitch;
		params.fvol = channelSave.masterVolume / 255.0f;
		params.m_nSoundScriptHash = g_pSoundEmitterSystem->HashSoundName( channelSave.soundName );
		params.m_bIsScriptHandle = params.m_nSoundScriptHash != SOUNDEMITTER_INVALID_HASH;
		params.flags = SND_CHANGE_VOL | SND_CHANGE_PITCH;
		params.flags |= params.m_bIsScriptHandle ? SND_IS_SCRIPTHANDLE : 0;
		params.opStackElapsedTime = channelSave.opStackElapsedTime;
		params.opStackElapsedStopTime = channelSave.opStackElapsedStopTime;
		params.delay = -params.opStackElapsedTime;	// For non-script entries (currently not saved), this will simply be the elapsed time
		params.staticsound = params.entchannel == CHAN_STATIC ? true : false;

		channel_t* pDuplicateChannel = S_FindDuplicateChannel( params );
		if( pDuplicateChannel != NULL )
			S_StopChannel( pDuplicateChannel );

		S_StartSoundEntry( params, -1, false );
	}
}

soundlevel_t SND_GetSndlvl ( channel_t *pchannel );

// calculate ammount of sound to be mixed to dsp, based on distance from listener


ConVar dsp_dist_min("dsp_dist_min", "0.0", FCVAR_DEMO|FCVAR_CHEAT);		// range at which sounds are mixed at dsp_mix_min
ConVar dsp_dist_max("dsp_dist_max", "1440.0", FCVAR_DEMO|FCVAR_CHEAT);	// range at which sounds are mixed at dsp_mix_max	

ConVar dsp_mix_min("dsp_mix_min", "0.2", FCVAR_DEMO|FCVAR_CHEAT );		// dsp mix at dsp_dist_min distance "near"
ConVar dsp_mix_max("dsp_mix_max", "0.8", FCVAR_DEMO|FCVAR_CHEAT );		// dsp mix at dsp_dist_max distance "far"
ConVar dsp_db_min("dsp_db_min", "80", FCVAR_DEMO|FCVAR_CHEAT );			// sounds with sndlvl below this get dsp_db_mixdrop % less dsp mix
ConVar dsp_db_mixdrop("dsp_db_mixdrop", "0.5", FCVAR_DEMO|FCVAR_CHEAT );	// sounds with sndlvl below dsp_db_min get dsp_db_mixdrop % less mix

float	DSP_ROOM_MIX	= 1.0;	// mix volume of dsp_room sounds when added back to 'dry' sounds
float	DSP_NOROOM_MIX	= 1.0;	// mix volume of facing + facing away sounds. added to dsp_room_mix sounds

extern ConVar dsp_off;

// returns 0-1.0 dsp mix value.  If sound source is at a range >= DSP_DIST_MAX, return a mix value of
// DSP_MIX_MAX.  This mix value is used later to determine wet/dry mix ratio of sounds.

// This ramp changes with db level of sound source,  and is set in the dsp room presets by room size
// empirical data: 0.78 is nominal mix for sound 100% at far end of room, 0.24 is mix for sound 25% into room

float SND_GetDspMix( channel_t *pchannel, int idist, float flSndlvl)
{
	float mix;
	float dist = (float)idist;
	float dist_min = dsp_dist_min.GetFloat();
	float dist_max = dsp_dist_max.GetFloat();
	float mix_min;
	float mix_max;

	// only set dsp mix_min & mix_max when sound is first started

	if ( pchannel->dsp_mix_min < 0 && pchannel->dsp_mix_max < 0 )
	{
		mix_min  = dsp_mix_min.GetFloat();		// set via dsp_room preset
		mix_max  = dsp_mix_max.GetFloat();		// set via dsp_room preset

		// set mix_min & mix_max based on db level of sound:
		// sounds below dsp_db_min decrease dsp_mix_min & dsp_mix_max by N%
		// ie: quiet sounds get less dsp mix than loud sounds
		soundlevel_t sndlvl_min = (soundlevel_t)(dsp_db_min.GetInt());

		if (( (int) flSndlvl) <= sndlvl_min)
		{
			mix_min *= dsp_db_mixdrop.GetFloat();
			mix_max *= dsp_db_mixdrop.GetFloat();
		}

		pchannel->dsp_mix_min = mix_min;
		pchannel->dsp_mix_max = mix_max;
	}
	else
	{
		mix_min = pchannel->dsp_mix_min;
		mix_max = pchannel->dsp_mix_max;
	}

	// dspmix is 0 (100% mix to facing buffer) if dsp_off

	if ( dsp_off.GetInt() )
		return 0.0;

	// linear ramp - get dry mix %

	// dist: 0->(max - min)

	dist = clamp( dist, dist_min, dist_max ) - dist_min;

	// dist: 0->1.0

	dist = dist / (dist_max - dist_min);

	// mix: min->max

	mix = ((mix_max - mix_min) * dist) + mix_min;

	return mix;
}

float SND_GetDspMix( channel_t *pchannel, int idist)
{
	// doppler wavs are mixed dry

	if ( pchannel->wavtype == CHAR_DOPPLER )
		return 0.0;

	soundlevel_t sndlvl = SND_GetSndlvl( pchannel );
	return SND_GetDspMix( pchannel, idist, sndlvl );

}

// calculate crossfade between wav left (close sound) and wav right (far sound) based on
// distance fron listener

ConVar snd_dvar_dist_min( "snd_dvar_dist_min", "240" /* (20.0  * 12.0) */, FCVAR_CHEAT, "Play full 'near' sound at this distance" );
ConVar snd_dvar_dist_max( "snd_dvar_dist_max", "1320" /* (110.0  * 12.0) */, FCVAR_CHEAT, "Play full 'far' sound at this distance" );
#define DVAR_MIX_MIN	0.0
#define DVAR_MIX_MAX	1.0

// calculate mixing parameter for CHAR_DISTVAR wavs
// returns 0 - 1.0, 1.0 is 100% far sound (wav right)

float SND_GetDistanceMix( channel_t *pchannel, int idist)
{
	float mix;
	float dist = (float)idist;
	
	// doppler wavs are 100% near - their spatialization is calculated later.

	if ( pchannel->wavtype == CHAR_DOPPLER || pchannel->wavtype == CHAR_DIRSTEREO )
		return 0.0;

	// linear ramp - get dry mix %
	
	// dist 0->(max - min)

	dist = clamp( dist, snd_dvar_dist_min.GetFloat(), snd_dvar_dist_max.GetFloat() ) - snd_dvar_dist_min.GetFloat();

	// dist 0->1.0

	dist = dist / (snd_dvar_dist_max.GetFloat() - snd_dvar_dist_min.GetFloat());

	// mix min->max

	mix = ((DVAR_MIX_MAX - DVAR_MIX_MIN) * dist) + DVAR_MIX_MIN;
	
	return mix;
}

// given facing direction of source, and channel, 
// return -1.0 - 1.0, where -1.0 is source facing away from listener
// and 1.0 is source facing listener


float SND_GetFacingDirection( channel_t *pChannel, const Vector &vecListenerOrigin, const QAngle &source_angles )
{
	Vector SF;				// sound source forward direction unit vector
	Vector SL;				// sound -> listener unit vector
	float dotSFSL;

	// no facing direction unless wavtyp CHAR_DIRECTIONAL

	// this won't get used anyway if it's not directional
// 	if ( pChannel->wavtype != CHAR_DIRECTIONAL )
// 		return 1.0;
	
	VectorSubtract(vecListenerOrigin, pChannel->origin, SL);
	VectorNormalize(SL);

	// compute forward vector for sound entity

	AngleVectors( source_angles, &SF, NULL, NULL );

	// dot source forward unit vector with source to listener unit vector to get -1.0 - 1.0 facing.
	// ie: projection of SF onto SL

	dotSFSL = DotProduct( SF, SL );
		
	return dotSFSL;
}

// calculate point of closest approach - caller must ensure that the 
// forward facing vector of the entity playing this sound points in exactly the direction of 
// travel of the sound. ie: for bullets or tracers, forward vector must point in traceline direction.
// return true if sound is to be played, false if sound cannot be heard (shot away from player)

bool SND_GetClosestPoint( channel_t *pChannel, const Vector &vecListenerOrigin, QAngle &source_angles, Vector &vnearpoint )
{
	// S - sound source origin
	// L - listener origin
	
	Vector SF;				// sound source forward direction unit vector
	Vector SL;				// sound -> listener vector
	Vector SD;				// sound->closest point vector
	vec_t dSLSF;			// magnitude of project of SL onto SF

	// P = SF (SF . SL) + S

	// only perform this calculation for doppler wavs

	if ( pChannel->wavtype != CHAR_DOPPLER )
		return false;

	// get vector 'SL' from sound source to listener

	VectorSubtract(vecListenerOrigin, pChannel->origin, SL);

	// compute sound->forward vector 'SF' for sound entity

	AngleVectors( source_angles, &SF );
	VectorNormalize( SF );
	
	dSLSF = DotProduct( SL, SF );
	if ( dSLSF <= 0 && !toolframework->IsToolRecording() )
	{
		// source is pointing away from listener, don't play anything
		// unless we're recording in the tool, since we may play back from in front of the source
		return false;
	}
		
	// project dSLSF along forward unit vector from sound source
	
	VectorMultiply( SF, dSLSF, SD );

	// output vector - add SD to sound source origin

	VectorAdd( SD, pChannel->origin, vnearpoint );

	return true;
}


// given point of nearest approach and sound source facing angles, 
// return vector pointing into quadrant in which to play 
// doppler left wav (incomming) and doppler right wav (outgoing).

// doppler left is point in space to play left doppler wav
// doppler right is point in space to play right doppler wav

// Also modifies channel pitch based on distance to nearest approach point

#define DOPPLER_DIST_LEFT_TO_RIGHT	(4*12)		// separate left/right sounds by 4'

#define DOPPLER_DIST_MAX			(20*12)		// max distance - causes min pitch
#define DOPPLER_DIST_MIN			(1*12)		// min distance - causes max pitch
#define DOPPLER_PITCH_MAX			1.5			// max pitch change due to distance
#define DOPPLER_PITCH_MIN			0.25		// min pitch change due to distance

#define DOPPLER_RANGE_MAX			(10*12)		// don't play doppler wav unless within this range
												// UNDONE: should be set by caller!

static void SND_GetDopplerPoints( channel_t *pChannel, const Vector &vecListenerOrigin, QAngle &source_angles, Vector &vnearpoint, Vector &source_doppler_left, Vector &source_doppler_right)
{
	Vector SF;			// direction sound source is facing (forward)
	Vector LN;			// vector from listener to closest approach point
	Vector DL;
	Vector DR;

	// nearpoint is closest point of approach, when playing CHAR_DOPPLER sounds

	// SF is normalized vector in direction sound source is facing

	AngleVectors( source_angles, &SF );
	VectorNormalize( SF );

	// source_doppler_left - location in space to play doppler left wav (incomming)
	// source_doppler_right	- location in space to play doppler right wav (outgoing)
	
	VectorMultiply( SF, -1*DOPPLER_DIST_LEFT_TO_RIGHT, DL );
	VectorMultiply( SF, DOPPLER_DIST_LEFT_TO_RIGHT, DR );

	VectorAdd( vnearpoint, DL, source_doppler_left );
	VectorAdd( vnearpoint, DR, source_doppler_right );
	
	// set pitch of channel based on nearest distance to listener

	// LN is vector from listener to closest approach point

	VectorSubtract(vnearpoint, vecListenerOrigin, LN);

	float pitch;
	float dist = VectorLength( LN );
	
	// dist varies 0->1

	dist = clamp(dist, DOPPLER_DIST_MIN, DOPPLER_DIST_MAX);
	dist = (dist - DOPPLER_DIST_MIN) / (DOPPLER_DIST_MAX - DOPPLER_DIST_MIN);

	// pitch varies from max to min

	pitch = DOPPLER_PITCH_MAX - dist * (DOPPLER_PITCH_MAX - DOPPLER_PITCH_MIN);
	
	pChannel->basePitch = (int)(pitch * 100.0);
}

// console variables used to construct gain curve - don't change these!

extern ConVar snd_foliage_db_loss; 
extern ConVar snd_gain;

extern ConVar snd_gain_max;
extern ConVar snd_gain_min;

ConVar snd_showstart( "snd_showstart", "0", FCVAR_CHEAT );	// showstart always skips info on player footsteps!
												// 1 - show sound name, channel, volume, time 
												// 2 - show dspmix, distmix, dspface, l/r/f/r vols
												// 3 - show sound origin coords
												// 4 - show gain of dsp_room
												// 5 - show dB loss due to obscured sound
												// 6 - reserved
												// 7 - show 2 and total gain & dist in ft. to sound source
												// snd_showstart reports only the sounds being actively played. To see sounds that should be played but may have been discarded (for errors or other reasons), use snd_report_start_sound. 

#define SND_GAIN_PLAYER_WEAPON_DB 2.0	// increase player weapon gain by N dB

// dB = 20 log (amplitude/32768)		0 to -90.3dB
// amplitude = 32768 * 10 ^ (dB/20)		0 to +/- 32768
// gain = amplitude/32768				0 to 1.0

float Gain_To_dB ( float gain )
{
	float dB = 20 * log ( gain );
	return dB;
}

float dB_To_Gain ( float dB )
{
	float gain = powf (10, dB / 20.0);
	return gain;
}

float Gain_To_Amplitude ( float gain )
{
	return gain * 32768;
}

float Amplitude_To_Gain ( float amplitude )
{
	return amplitude / 32768;
}

soundlevel_t SND_GetSndlvl ( channel_t *pchannel )
{
	return DIST_MULT_TO_SNDLVL( pchannel->dist_mult );
}


// The complete gain calculation, with SNDLVL given in dB is:
//
// GAIN = 1/dist * snd_refdist * 10 ^ ( ( SNDLVL - snd_refdb - (dist * snd_foliage_db_loss / 1200)) / 20 )
// 
//		for gain > SND_GAIN_THRESH, start curve smoothing with
//
// GAIN = 1 - 1 / (Y * GAIN ^ SND_GAIN_POWER)
// 
//		 where Y = -1 / ( (SND_GAIN_THRESH ^ SND_GAIN_POWER) * (SND_GAIN_THRESH - 1) )
//

float SND_GetGainFromMult( float gain, float dist_mult, vec_t dist );

// NOTE: This is to eliminate the effect of "volume" on the distance falloff curve
// NOTE: may NOT BE TRUE, only effects compression?? and only from master volume control!
ConVar snd_preGainDistFalloff( "snd_pre_gain_dist_falloff", "1", FCVAR_CHEAT );	// showstart always skips info on player footsteps!

// gain curve construction
static float SND_GetMusicVolumeGainMultiplierInOverlay()
{
	if ( sv.IsDedicated() )
		return 1.0f;

	static float s_flMusicVolumeOverlayMultiplierPrevious = 1.0f;
	static float s_flMusicVolumeOverlayMultiplierTarget = 1.0f;
	static double s_flLastUpdateTime = Plat_FloatTime();
	static bool s_bOverlayActiveLastKnown = false;

	double flTimeNow = Plat_FloatTime();
	if ( flTimeNow - s_flLastUpdateTime > 0.1 )
	{
		// Update every 0.1 sec
		static ConVarRef cl_embedded_stream_video_playing( "cl_embedded_stream_video_playing" );
		bool bInClientVideoPlaying = ( cl_embedded_stream_video_playing.IsValid() && cl_embedded_stream_video_playing.GetBool() );
		bool bCurrentlyActive = Steam3Client().IsGameOverlayActive() || bInClientVideoPlaying;
		if ( bCurrentlyActive != s_bOverlayActiveLastKnown )
		{
			s_flMusicVolumeOverlayMultiplierPrevious = ( flTimeNow > s_flLastUpdateTime + 1.0 ) ? s_flMusicVolumeOverlayMultiplierTarget : ( s_flMusicVolumeOverlayMultiplierPrevious + ( flTimeNow - s_flLastUpdateTime ) * ( s_flMusicVolumeOverlayMultiplierTarget - s_flMusicVolumeOverlayMultiplierPrevious ) );
			s_flLastUpdateTime = flTimeNow;
			s_bOverlayActiveLastKnown = bCurrentlyActive;
			s_flMusicVolumeOverlayMultiplierTarget = bCurrentlyActive ? ( bInClientVideoPlaying ? 0.0f : snd_musicvolume_multiplier_inoverlay.GetFloat() ) : 1.0f;
		}
	}
	return ( flTimeNow > s_flLastUpdateTime + 1.0 ) ? s_flMusicVolumeOverlayMultiplierTarget : ( s_flMusicVolumeOverlayMultiplierPrevious + ( flTimeNow - s_flLastUpdateTime ) * ( s_flMusicVolumeOverlayMultiplierTarget - s_flMusicVolumeOverlayMultiplierPrevious ) );
}
float SND_GetGain( int nSlot, gain_t *gs, const channel_t *ch, const Vector &vecListenerOrigin, bool fplayersound, bool fmusicsound, bool flooping, vec_t dist, bool bAttenuated, bool bOkayToTrace )
{
	VPROF_( "SND_GetGain", 2, VPROF_BUDGETGROUP_OTHER_SOUND, false, BUDGETFLAG_OTHER );

	if ( ch->flags.m_bCompatibilityAttenuation )
	{
		// Convert to the original attenuation value.
		soundlevel_t soundlevel = DIST_MULT_TO_SNDLVL( ch->dist_mult );
		float flAttenuation = SNDLVL_TO_ATTN( soundlevel );

		// Now get the goldsrc dist_mult and use the same calculation it uses in SND_Spatialize.
		// Straight outta Goldsrc!!!
		vec_t sound_nominal_clip_dist = 1000.0;
		float flGoldsrcDistMult = flAttenuation / sound_nominal_clip_dist;
		dist *= flGoldsrcDistMult;
		float flReturnValue = 1.0f - dist;
		flReturnValue = clamp( flReturnValue, 0, 1 );
		return flReturnValue;
	}
	else
	{
		float gain = 1.0;

		// without volume free falloff, our overall gain changes falloff curve shape
		// which in my opinion is not good
		// pre-falloff gain
		if(!snd_preGainDistFalloff.GetInt())
		{
			gain = snd_gain.GetFloat();

			if ( fmusicsound )
			{
				gain = gain * snd_musicvolume.GetFloat();
				gain = gain * g_DashboardMusicMixValue;
				gain = gain * g_flReplayMusicGain;
				gain = gain * SND_GetMusicVolumeGainMultiplierInOverlay();
			}
		}

		// get soundlevel / distance based gain falloff 
		if ( ch->dist_mult && ch->wavtype != CHAR_DIRSTEREO)
		{
			gain = SND_GetGainFromMult( gain, ch->dist_mult, dist );
		}

		// post-falloff gain
		if(snd_preGainDistFalloff.GetInt())
		{
			gain *= snd_gain.GetFloat();
			if ( fmusicsound )
			{
				gain = gain * snd_musicvolume.GetFloat();
				gain = gain * g_DashboardMusicMixValue;
				gain = gain * g_flReplayMusicGain;
				gain = gain * SND_GetMusicVolumeGainMultiplierInOverlay();
			}
		}


		if ( fplayersound )
		{
			
			// player weapon sounds get extra gain - this compensates
			// for npc distance effect weapons which mix louder as L+R into L,R
			// Hack.

			if ( ch->entchannel == CHAN_WEAPON )
				gain = gain * dB_To_Gain( SND_GAIN_PLAYER_WEAPON_DB );
		}

		// modify gain if sound source not visible to player
		if(ch->wavtype != CHAR_DIRSTEREO)
		{
			gain = gain * SND_GetGainObscured( nSlot, gs, ch, vecListenerOrigin, fplayersound, flooping, bAttenuated, bOkayToTrace, NULL );
		}
		if (snd_showstart.GetInt() == 6)
		{
			DevMsg( "(gain %1.3f : dist ft %1.1f) ", gain, (float)dist/12.0 );
			snd_showstart.SetValue(5);	// display once
		}

		return gain; 
	}
}



// always ramp channel gain changes over time
// returns ramped gain, given new target gain

#define SND_GAIN_FADE_TIME	0.25		// xfade seconds between obscuring gain changes

float SND_FadeToNewGain( gain_t *gs, const channel_t *ch, float gain_new )
{

	if ( gain_new == -1.0 )
	{
		// if -1 passed in, just keep fading to existing target

		gain_new = gs->ob_gain_target;
	}

	// if first time updating, store new gain into gain & target, return
	// if gain_new is close to existing gain, store new gain into gain & target, return

	if ( ch->flags.bfirstpass || (fabs (gain_new - gs->ob_gain) < 0.01))
	{
		gs->ob_gain			= gain_new;
		gs->ob_gain_target	= gain_new;
		gs->ob_gain_inc		= 0.0;
		return gain_new;
	}

	// set up new increment to new target
	
	float frametime = g_pSoundServices->GetHostFrametime();
	float speed;
	speed = ( frametime / SND_GAIN_FADE_TIME ) * (gain_new - gs->ob_gain);

	gs->ob_gain_inc = fabs(speed);

	// gs->ob_gain_inc = fabs(gain_new - gs->ob_gain) / 10.0;
	
	gs->ob_gain_target = gain_new;

	// if not hit target, keep approaching
	
	if ( fabs( gs->ob_gain - gs->ob_gain_target ) > 0.01 )
	{
		gs->ob_gain = Approach( gs->ob_gain_target, gs->ob_gain, gs->ob_gain_inc );
	}
	else
	{
		// close enough, set gain = target
		gs->ob_gain = gs->ob_gain_target;
	}

	return gs->ob_gain;
}

#define SND_TRACE_UPDATE_MAX  2			// max of N channels may be checked for obscured source per frame

int g_snd_trace_count = 0;		// total tracelines for gain obscuring made this frame

// All new sounds must traceline once,
// but cap the max number of tracelines performed per frame
// for longer or looping sounds to SND_TRACE_UPDATE_MAX.

bool SND_ChannelOkToTrace( channel_t *ch )
{
	// always trace first time sound is spatialized (doesn't update counter)

	if ( ch->flags.bfirstpass )
	{
		ch->flags.bTraced = true;
		return true;
	}

	// if already traced max channels this frame, return
	if ( g_snd_trace_count >= SND_TRACE_UPDATE_MAX )
		return false;

	// ok to trace if this sound hasn't yet been traced in this round

	if ( ch->flags.bTraced )
		return false;

	// set flag - don't traceline this sound again until all others have
	// been traced

	ch->flags.bTraced = true;

	return true;
}

// determine if we need to reset all flags for traceline limiting - 
// this happens if we hit a frame whein no tracelines occur ie: all currently 
// playing sounds are blocked.

void SND_ChannelTraceReset( void )
{
	if ( g_snd_trace_count )
		return;

	// if no tracelines performed this frame, then reset all 
	// trace flags

	for (int i = 0; i < total_channels; i++)
		channels[i].flags.bTraced = false; 
}

bool SND_IsLongWave( const channel_t *pChannel )
{
	// force it to look like everything is streaming, like on the consoles
	// this gets used in 2 places, if the volume is 0.0 for some reason
	// and to test if getgainobscured should function
#ifdef PORTAL2 
	return true;
#endif

	CAudioSource *pSource = pChannel->sfx ? pChannel->sfx->pSource : NULL;
	if ( pSource )
	{
		if ( pSource->IsStreaming() )
			return true;

	// UNDONE: Do this on long wave files too?
#if 0
		float length = (float)pSource->SampleCount() / (float)pSource->SampleRate();
		if ( length > 0.75f )
			return true;
#endif
	}

	return false;
}


ConVar snd_obscured_gain_db( "snd_obscured_gain_dB", "-2.70", FCVAR_CHEAT ); // dB loss due to obscured sound source

// drop gain on channel if sound emitter obscured by
// world, unbroken windows, closed doors, large solid entities etc.

float SND_GetGainObscured( int nSlot, gain_t *gs, const channel_t *ch, const Vector &vecListenerOrigin, bool fplayersound, bool flooping, bool bAttenuated, bool bOkayToTrace, Vector *pOrigin )
{
	float gain = 1.0;
	int count = 1;
	float snd_gain_db;					// dB loss due to obscured sound source

	// Unattenuated sounds don't get obscured.
	if ( !bAttenuated )
		return 1.0f;

	if ( fplayersound )
		return gain;

	// During signon just apply regular state machine since world hasn't been
	//  created or settled yet...

	if ( !SND_IsInGame() )
	{
		if ( !toolframework->InToolMode() )
		{
			gain = SND_FadeToNewGain( gs, ch, -1.0 );
		}

		return gain;
	}

	// don't do gain obscuring more than once on short one-shot sounds

	if ( !ch->flags.bfirstpass && !ch->flags.isSentence && !flooping && !SND_IsLongWave(ch) )
	{
		gain = SND_FadeToNewGain( gs, ch, -1.0 );
		return gain;
	}

	snd_gain_db = snd_obscured_gain_db.GetFloat();

	// if long or looping sound, process N channels per frame - set 'processed' flag, clear by
	// cycling through all channels - this maintains a cap on traces per frame

	if ( !bOkayToTrace )
	{
		// just keep updating fade to existing target gain - no new trace checking

		gain = SND_FadeToNewGain( gs, ch, -1.0 );
		return gain;
	}

	// set up traceline from player eyes to sound emitting entity origin
	Vector endpoint;
	if( pOrigin )
	{
		// it's been passed in by an operator
		endpoint = *pOrigin;
	}
	else
	{
		endpoint = ch->origin;
	}
	
	trace_t tr;
	CTraceFilterWorldOnly filter;	// UNDONE: also test for static props?
	Ray_t ray;
	ray.Init( MainViewOrigin( nSlot ), endpoint );
	g_pEngineTraceClient->TraceRay( ray, MASK_BLOCK_AUDIO, &filter, &tr );
	// total traces this frame
	g_snd_trace_count++;				

	if (tr.DidHit() && tr.fraction < 0.99)
	{
		// can't see center of sound source:
		// build extents based on dB sndlvl of source,
		// test to see how many extents are visible,
		// drop gain by snd_gain_db per extent hidden

		Vector endpoints[4];
		soundlevel_t sndlvl = DIST_MULT_TO_SNDLVL( ch->dist_mult );
		float radius;
		Vector vsrc_forward;
		Vector vsrc_right;
		Vector vsrc_up;
		Vector vecl;
		Vector vecr;
		Vector vecl2;
		Vector vecr2;
		int i;

		// get radius
		
		if ( ch->radius > 0 )
			radius = ch->radius;
		else
			radius = dB_To_Radius( sndlvl);		// approximate radius from soundlevel
		
		// set up extent endpoints - on upward or downward diagonals, facing player

		for (i = 0; i < 4; i++)
			endpoints[i] = endpoint;

		// vsrc_forward is normalized vector from sound source to listener

		VectorSubtract( vecListenerOrigin, endpoint, vsrc_forward );
		VectorNormalize( vsrc_forward );
		VectorVectors( vsrc_forward, vsrc_right, vsrc_up );

		VectorAdd( vsrc_up, vsrc_right, vecl );
		
		// if src above listener, force 'up' vector to point down - create diagonals up & down

		if ( endpoint.z > vecListenerOrigin.z + (10 * 12) )
			vsrc_up.z = -vsrc_up.z;

		VectorSubtract( vsrc_up, vsrc_right, vecr );
		VectorNormalize( vecl );
		VectorNormalize( vecr );

		// get diagonal vectors from sound source 

		vecl2 = radius * vecl;
		vecr2 = radius * vecr;
		vecl = (radius / 2.0) * vecl;
		vecr = (radius / 2.0) * vecr;

		// endpoints from diagonal vectors

		endpoints[0] += vecl;
		endpoints[1] += vecr;
		endpoints[2] += vecl2;
		endpoints[3] += vecr2;

		// drop gain for each point on radius diagonal that is obscured

		for (count = 0, i = 0; i < 4; i++)
		{
			// UNDONE: some endpoints are in walls - in this case, trace from the wall hit location

			Ray_t ray;
			ray.Init( MainViewOrigin( nSlot ), endpoints[i] );
			g_pEngineTraceClient->TraceRay( ray, MASK_BLOCK_AUDIO, &filter, &tr );

			if (tr.DidHit() && tr.fraction < 0.99 && !tr.startsolid )
			{
				count++;	// skip first obscured point: at least 2 points + center should be obscured to hear db loss
				if (count > 1)
					gain = gain * dB_To_Gain( snd_gain_db );
			}
		}
	}

	
	if ( flooping && snd_showstart.GetInt() == 7)
	{
		static float g_drop_prev = 0;
		float drop = (count-1) * snd_gain_db;

		if (drop != g_drop_prev)
		{
			DevMsg( "dB drop: %1.4f \n", drop);
			g_drop_prev = drop;
		}
	}

	// crossfade to new gain

	gain = SND_FadeToNewGain( gs, ch, gain );

	return gain;
}


struct snd_spatial_t
{
	int chan;			// 0..4 cycles through up to 5 channels
	int cycle;			// 0..2 cycles through 3 vectors per channel
	int dist[5][3];		// stores last 3 channel distance values [channel][cycle]

	float value_prev[5];	// previous value per channel

	double last_change;
};

bool g_ssp_init = false;
snd_spatial_t g_ssp;

// return 0..1 percent difference between a & b

float PercentDifference( float a, float b )
{
	float vp;

	if (!(int)a && !(int)b)
		return 0.0;

	if (!(int)a || !(int)b)
		return 1.0;

	if (a > b)
		vp = b / a;
	else
		vp = a / b;

	return (1.0 - vp);
}

// NOTE: Do not change SND_WALL_TRACE_LEN without also changing PRC_MDY6 delay value in snd_dsp.cpp!

#define SND_WALL_TRACE_LEN (100.0*12.0)		// trace max of 100' = max of 100 milliseconds of linear delay
#define SND_SPATIAL_WAIT	(0.25)			// seconds to wait between traces

// change mod delay value on chan 0..3 to v (inches)

void DSP_SetSpatialDelay( int chan, float v )
{
	// remap delay value 0..1200 to 1.0 to -1.0 for modulation

	float value = ( v / SND_WALL_TRACE_LEN) - 1.0;					// -1.0...0
	value = value * 2.0;											// -2.0...0
	value += 1.0;													// -1.0...1.0 (0...1200)
	value *= -1.0;													// 1.0...-1.0 (0...1200)

	// assume first processor in dsp_spatial is the modulating delay unit for DSP_ChangePresetValue

	int iproc = 0;

	DSP_ChangePresetValue( idsp_spatial, chan, iproc, value );		
/*

	if (chan & 0x01)
		DevMsg("RDly: %3.0f \n", v/12 );
	else
		DevMsg("LDly: %3.0f \n", v/12 );
*/
}

// use non-feedback delay to stereoize (or make quad, or quad + center) the mono dsp_room fx, 
// This simulates the average sum of delays caused by reflections
// from the left and right walls relative to the player.  The average delay
// difference between left & right wall is (l + r)/2.  This becomes the average
// delay difference between left & right ear. 
// call at most once per frame to update player->wall spatial delays
bool SND_IsListenerValid()
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		if ((listener_origin[ hh ]		== vec3_origin) && 
			(listener_forward[ hh ]		== vec3_origin) &&
			(listener_right[ hh ]		== vec3_origin) &&
			(listener_up[ hh ]			== vec3_origin) )
		{
			continue;
		}
		return true;
	}

	return false;
}

void SND_SetSpatialDelays()
{
	VPROF("SoundSpatialDelays");
	float dist = FLT_MAX, v, vp;
	Vector v_dir, v_dir2;
	int chan_max = (g_AudioDevice->IsSurround() ? 4 : 2) + (g_AudioDevice->IsSurroundCenter() ? 1 : 0);  // 2, 4, 5 channels
	
	// use listener_forward2d, which doesn't change when player looks up/down.
	// init struct if 1st time through
	if ( !g_ssp_init )
	{
		Q_memset(&g_ssp, 0, sizeof(snd_spatial_t));
		g_ssp_init = true;
	}

	// return if dsp_spatial is 0
	
	if ( !dsp_spatial.GetInt() ) 
		return;
	
	// if listener has not been updated, do nothing
	if ( !SND_IsListenerValid() )
		return;

	if ( !SND_IsInGame() )
		return;


	// get time
	
	double dtime = g_pSoundServices->GetHostTime();
	
	// compare to previous time - if starting new check - don't check for new room until timer expires

	if (!g_ssp.chan && !g_ssp.cycle)
	{
		if (fabs(dtime - g_ssp.last_change) < SND_SPATIAL_WAIT)
			return;
	}


	// cycle through forward, left, rearward vectors, averaging to get left/right delay
	// count[chan][cycle] 0,1 0,2 0,3   1,1 1,2 1,3    2,1 2,2 2,3 ...

	g_ssp.cycle++;
	
	if (g_ssp.cycle == 3)
	{
		g_ssp.cycle = 0;

		// cycle through front left, front right, rear left, rear right, front center delays

		g_ssp.chan++;
	
		if (g_ssp.chan >= chan_max )
			g_ssp.chan = 0;
	}

	FOR_EACH_VALID_SPLITSCREEN_PLAYER( nSlot )
	{
		// HACK FOR NOW, THIS ONLY WORKS ON THE FIRST PLAYER!!!!!
		//xxxFIXMESPLITSCREEN
		if ( nSlot >= 1 )
			break;

		Vector listener_forward2d;
		Vector vecListenerRight = listener_right[ nSlot ];
		ConvertListenerVectorTo2D( &listener_forward2d, &vecListenerRight );

		// set up traceline from player eyes to surrounding walls

		switch( g_ssp.chan )
		{
		default:
		case 0: // front left: trace max 100' 'cone' to player's left
			if ( g_AudioDevice->IsSurround() )
			{
				// 4-5 speaker case - front left
				v_dir = (-vecListenerRight + listener_forward2d) / 2.0;
				v_dir = g_ssp.cycle ? (g_ssp.cycle == 1 ? -vecListenerRight * 0.5: listener_forward2d * 0.5) : v_dir;
			}
			else
			{
				// 2 speaker case - left
				v_dir = vecListenerRight * -1.0;
				v_dir2 = g_ssp.cycle ? (g_ssp.cycle == 1 ? listener_forward2d * 0.5 : -listener_forward2d * 0.5) : v_dir;
				v_dir = (v_dir + v_dir2) / 2.0;
			}
			break;

		case 1: // front right: trace max 100' 'cone' to player's right
			if ( g_AudioDevice->IsSurround() )
			{
				// 4-5 speaker case - front right
				v_dir = (vecListenerRight + listener_forward2d) / 2.0;
				v_dir = g_ssp.cycle ? (g_ssp.cycle == 1 ? vecListenerRight * 0.5: listener_forward2d * 0.5) : v_dir;
			}
			else
			{
				// 2 speaker case - right
				v_dir = vecListenerRight;
				v_dir2 = g_ssp.cycle ? (g_ssp.cycle == 1 ? listener_forward2d * 0.5 : -listener_forward2d * 0.5) : v_dir;
				v_dir = (v_dir + v_dir2) / 2.0;
			}
			break;

		case 2: // rear left: trace max 100' 'cone' to player's rear left
			v_dir = (vecListenerRight + listener_forward2d) / -2.0;
			v_dir = g_ssp.cycle ? (g_ssp.cycle == 1 ? -vecListenerRight * 0.5 : -listener_forward2d * 0.5) : v_dir;
			break;

		case 3: // rear right: trace max 100' 'cone' to player's rear right
			v_dir = (vecListenerRight - listener_forward2d) / 2.0;
			v_dir = g_ssp.cycle ? (g_ssp.cycle == 1 ? vecListenerRight * 0.5: -listener_forward2d * 0.5) : v_dir;
			break;
			
		case 4: // front center: trace max 100' 'cone' to player's front
			v_dir = listener_forward2d;
			v_dir2 = g_ssp.cycle ? (g_ssp.cycle == 1 ? vecListenerRight * 0.15 : -vecListenerRight * 0.15) : v_dir;
			v_dir = (v_dir + v_dir2);
			break;
		}

		Vector endpoint;	
		trace_t tr;
		CTraceFilterWorldOnly filter;

		endpoint = MainViewOrigin( nSlot ) + v_dir * SND_WALL_TRACE_LEN;
		Ray_t ray;
		ray.Init( MainViewOrigin( nSlot ), endpoint );
		g_pEngineTraceClient->TraceRay( ray, MASK_BLOCK_AUDIO, &filter, &tr );

		float checkDist = SND_WALL_TRACE_LEN;

		if ( tr.DidHit() )
		{
			checkDist = VectorLength( tr.endpos - MainViewOrigin( nSlot ) );	
		}

		if ( checkDist < dist )
		{
			dist = checkDist;
		}
	}
		
	g_ssp.dist[g_ssp.chan][g_ssp.cycle] = dist;

	// set new result in dsp_spatial delay params when all delay values have been filled in

	if (!g_ssp.cycle && !g_ssp.chan)
	{
		// update delay for each channel

		for (int chan = 0; chan < chan_max; chan++)
		{
			// compute average of 3 traces per channel

			v = (g_ssp.dist[chan][0] + g_ssp.dist[chan][1] + g_ssp.dist[chan][2]) / 3.0;
			vp = g_ssp.value_prev[chan];

			// only change if 10% difference from previous

			if ((vp != v) && int(v) && (PercentDifference( v, vp ) >= 0.1))		
			{
				// update when we have data for all L/R && RL/RR channels...

				if (chan & 0x1)
				{
					float vr = fpmin( v, (50*12.0f) );
					float vl = fpmin(g_ssp.value_prev[chan-1], (50*12.0f));

/* UNDONE: not needed, now that this applies only to dsp 'room' buffer

					// ensure minimum separation = average distance to walls

					float dmin = (vl + vr) / 2.0;		// average distance to walls
					float d = vl - vr;					// l/r separation

					// if separation is less than average, increase min

					if (abs(d) < dmin/2)
					{
						if (vl > vr)
							vl += dmin/2 - d;
						else
							vr += dmin/2 - d;
					}
*/
					DSP_SetSpatialDelay(chan-1, vl);
					DSP_SetSpatialDelay(chan, vr);
				}
				
				// update center chan

				if (chan == 4)
				{
					float vl = fpmin( v, (50*12.0f) );
					DSP_SetSpatialDelay(chan, vl);
				}
			}
			
			g_ssp.value_prev[chan] = v;

		}

		// update wait timer now that all values have been checked

		g_ssp.last_change = dtime;		
	}	
}

// Dsp Automatic Selection:

//	a) enabled by setting dsp_room to DSP_AUTOMATIC.  Subsequently, dsp_automatic is the actual dsp value for dsp_room.
//	b) disabled by setting dsp_room to anything else

//	c) while enabled, detection nodes are placed as player moves into a new space
//     i.	at each node, a new dsp setting is calculated and dsp_automatic is set to an appropriate preset
//     ii.	new nodes are set when player moves out of sight of previous node
//     iii. moving into line of sight of a detection node causes closest node to player to set dsp_automatic

// see void DAS_CheckNewRoomDSP() for main entrypoint

ConVar das_debug( "adsp_debug", "0", FCVAR_ARCHIVE );
												// >0: draw blue dsp detection node location
												// >1: draw green room trace height detection bars
												// 3: draw yellow horizontal trace bars for room width/depth detection
												// 4: draw yellow upward traces for height detection
												// 5: draw teal box around all props around player
												// 6: draw teal box around room as detected

#define DAS_CWALLS				20				// # of wall traces to save for calculating room dimensions
#define DAS_ROOM_TRACE_LEN		(400.0*12.0)	// max size of trace to check for room dimensions

#define DAS_AUTO_WAIT	0.25					// wait min of n seconds between dsp_room changes and update checks

#define DAS_WIDTH_MIN	0.4						// min % change in avg width of any wall pair to cause new dsp
#define DAS_REFL_MIN	0.5						// min % change in avg refl of any wall to cause new dsp
#define DAS_SKYHIT_MIN	0.8						// min % change in # of sky hits per wall

#define DAS_DIST_MIN	(4.0 * 12.0)			// min distance between room dsp changes
#define DAS_DIST_MAX	(40.0 * 12.0)			// max distance to preserve room dsp changes

#define DAS_DIST_MIN_OUTSIDE	(6.0 * 12.0)	// min distance between room dsp changes outside
#define DAS_DIST_MAX_OUTSIDE	(100.0 * 12.0)	// max distance to preserve room dsp changes outside

#define IVEC_DIAG_UP	8						// start of diagonal up vectors
#define IVEC_UP			18						// up vector
#define IVEC_DOWN		19						// down vector

#define DAS_REFLECTIVITY_NORM	0.5
#define DAS_REFLECTIVITY_SKY	0.0

// auto dsp room struct

struct das_room_t
{
	int   dist[DAS_CWALLS];		// distance in units from player to axis aligned and diagonal walls
	float reflect[DAS_CWALLS];	// acoustic reflectivity per wall
	float skyhits[DAS_CWALLS];	// every sky hit adds 0.1
	Vector hit[DAS_CWALLS];		// location of trace hit on wall - used for calculating average centers
	Vector norm[DAS_CWALLS];	// wall normal at hit location

	Vector vplayer;				// 'frozen' location above player's head

	Vector vplayer_eyes;		// 'frozen' location player's eyes

	int width_max;				// max width
	int length_max;				// max length
	int height_max;				// max height
		
	float refl_avg;				// running average of reflectivity of all walls
	float refl_walls[6];		// left,right,front,back,ceiling,floor reflectivities	

	float sky_pct;				// percent of sky hits
	
	Vector room_mins;			// room bounds
	Vector room_maxs;

	double last_dsp_change;		// time since last dsp change

	float diffusion;			// 0..1.0 check radius (avg of width_avg) for # of props - scale diffusion based on # found
	short iwall;					// cycles through walls 0..5, ensuring only one trace per frame
	short ent_count;	 			// count of entities found in radius
	bool bskyabove;				// true if sky found above player (ie: outside)
	bool broomready;			// true if all distances are filled in and room is ready to check
	short lowceiling;				// if non-zero, ceiling directly above player if < 112 units
};

// dsp detection node

struct das_node_t
{
	Vector vplayer;				// position

	bool fused;					// true if valid node
	bool fseesplayer;			// true if node sees player on last check
	short dsp_preset;				// preset
		
	int range_min;				// min,max detection ranges
	int range_max;
	
	int dist;					// last distance to player

	// room parameters when node was created:

	das_room_t room;
};

#define DAS_CNODES	40					// keep around last n nodes - must be same as DSP_CAUTO_PRESETS!!!

das_node_t g_das_nodes[DAS_CNODES];		// all dsp detection nodes
das_node_t *g_pdas_last_node = NULL;	// last node that saw player

int g_das_check_next;					// next node to check
int g_das_store_next;					// next place to store node
bool g_das_all_checked;					// true if all nodes checked
int g_das_checked_count;				// count of nodes checked in latest pass

das_room_t g_das_room;					// room detector


bool g_bdas_room_init = 0;
bool g_bdas_init_nodes = 0;
bool g_bdas_create_new_node = 0;

bool DAS_TraceNodeToPlayer( das_room_t *proom, das_node_t *pnode );
void DAS_InitAutoRoom( das_room_t *proom);
void DAS_DebugDrawTrace ( trace_t *ptr, int r, int g, int b, float duration, int imax );

Vector g_das_vec3[DAS_CWALLS];	// trace vectors to walls, ceiling, floor

// for engine api
// new and changed rooms are only reset by the api function call
// thus they really mean "new since the last check", not ideal
bool g_current_das_room_changed = false;
bool g_current_das_room_new = false;

// these are updated regardless of api calls
bool g_current_das_room_sky_above = false;
float g_current_das_room_sky_percent = false;

void DAS_StoreRoomVarsAPI(das_room_t *pdas_room)
{
	g_current_das_room_sky_above = pdas_room->bskyabove;
	g_current_das_room_sky_percent = pdas_room->sky_pct;
}


bool S_DSPGetCurrentDASRoomNew(void)
{
	bool newRoom = g_current_das_room_new;
	g_current_das_room_new = false;
	return newRoom;
}
bool S_DSPGetCurrentDASRoomChanged(void)
{
	bool changedRoom = g_current_das_room_changed;
	g_current_das_room_changed = false;
	return changedRoom;
}
bool S_DSPGetCurrentDASRoomSkyAbove(void)
{
	return g_current_das_room_sky_above;
}
float S_DSPGetCurrentDASRoomSkyPercent(void)
{
	return g_current_das_room_sky_percent;
}
void DAS_InitNodes( void )
{
	Q_memset(g_das_nodes, 0, sizeof(das_node_t) * DAS_CNODES);
	g_das_check_next = 0;
	g_das_store_next = 0;
	g_das_all_checked = 0;
	g_das_checked_count = 0;

	// init all rooms

	for (int i = 0; i < DAS_CNODES; i++)
		DAS_InitAutoRoom( &(g_das_nodes[i].room) );

	// init trace vectors
	// set up trace vectors for max, min width
	float vl = DAS_ROOM_TRACE_LEN;
	float vlu = DAS_ROOM_TRACE_LEN * 0.52;
	float vlu2 = DAS_ROOM_TRACE_LEN * 0.48;	// don't use 'perfect' diagonals

	g_das_vec3[0].Init(vl, 0.0, 0.0);				// x left
	g_das_vec3[1].Init(-vl, 0.0, 0.0);				// x right

	g_das_vec3[2].Init(0.0, vl, 0.0);				// y front
	g_das_vec3[3].Init(0.0, -vl, 0.0);				// y back

	g_das_vec3[4].Init(-vlu, vlu2, 0.0);			// diagonal front left
	g_das_vec3[5].Init(vlu, -vlu2, 0.0);			// diagonal rear right

	g_das_vec3[6].Init(vlu, vlu2, 0.0);				// diagonal front right
	g_das_vec3[7].Init(-vlu, -vlu2, 0.0);			// diagonal rear left

	// set up trace vectors for max height - on x=y diagonal

	g_das_vec3[8].Init(vlu, vlu2, vlu/2.0);			// front right up A x,y,z/2		(IVEC_DIAG_UP)
	g_das_vec3[9].Init(vlu, vlu2, vlu);				// front right up B x,y,z
	g_das_vec3[10].Init(vlu/2.0, vlu2/2.0, vlu);	// front right up C x/2,y/2,z

	g_das_vec3[11].Init(-vlu, -vlu2, vlu/2.0);		// rear left up A -x,-y,z/2
	g_das_vec3[12].Init(-vlu, -vlu2, vlu);			// rear left up B -x,-y,z
	g_das_vec3[13].Init(-vlu/2.0, -vlu2/2.0, vlu);	// rear left up C -x/2,-y/2,z

	// set up trace vectors for max height - on x axis & y axis

	g_das_vec3[14].Init(-vlu, 0, vlu);				// left up B -x,0,z
	g_das_vec3[15].Init(0, vlu/2.0, vlu);			// front up C -x/2,0,z

	g_das_vec3[16].Init(0, -vlu, vlu);				// rear up B x,0,z
	g_das_vec3[17].Init(vlu/2.0, 0, vlu);			// right up C x/2,0,z

	g_das_vec3[18].Init(0.0, 0.0, vl);				// up	(IVEC_UP)
	g_das_vec3[19].Init(0.0, 0.0, -vl);				// down (IVEC_DOWN)
}

void DAS_InitAutoRoom( das_room_t *proom)
{
		Q_memset(proom, 0, sizeof (das_room_t));
}

// reset all nodes for next round of visibility checks between player & nodes

void DAS_ResetNodes( void )
{
	for (int i = 0; i < DAS_CNODES; i++)
	{
		g_das_nodes[i].fseesplayer = false;
		g_das_nodes[i].dist = 0;
	}

	g_das_all_checked = false;
	g_das_checked_count = 0;
	g_bdas_create_new_node = false;
}

ConCommand adsp_reset_nodes("adsp_reset_nodes", DAS_ResetNodes);

// utility function - return next index, wrap at max

int DAS_GetNextIndex( int *pindex, int max )
{
	int i = *pindex;
	int j;

	j = i+1;
	if ( j >= max )
		j = 0;

	*pindex = j;

	return i;
}

// returns true if dsp node is within range of player

bool DAS_NodeInRange( das_room_t *proom, das_node_t *pnode )
{
	float dist;

	dist = VectorLength( proom->vplayer - pnode->vplayer );

	// player can still see previous room selection point, and it's less than n feet away, 
	// then flag this node as visible

	pnode->dist = dist;
	
	return ( dist <= pnode->range_max );
}

// update next valid node - set up internal node state if it can see player
// called once per frame
// returns true if all nodes have been checked

bool DAS_CheckNextNode( das_room_t *proom )
{
	int i, j;

	if ( g_das_all_checked )
		return true;

	// find next valid node

	for (j = 0; j < DAS_CNODES; j++)
	{
		// track number of nodes checked

		g_das_checked_count++;

		// get next node in range to check

		i = DAS_GetNextIndex( &g_das_check_next, DAS_CNODES );

		if ( g_das_nodes[i].fused && DAS_NodeInRange( proom, &(g_das_nodes[i]) ) )
		{
			// trace to see if player can still see node, 
			// if so stop checking

			if ( DAS_TraceNodeToPlayer( proom, &(g_das_nodes[i]) ))
				goto checknode_exit;
		}
	}

checknode_exit:

	// flag that all nodes have been checked

	if ( g_das_checked_count >= DAS_CNODES )
		g_das_all_checked = true;	

	return g_das_all_checked;
}


int DAS_GetNextNodeIndex()
{
	return g_das_store_next;
}
// store new node for room

void DAS_StoreNode( das_room_t *proom, int dsp_preset)
{
	// overwrite node in cyclic list
	
	int i = DAS_GetNextIndex( &g_das_store_next, DAS_CNODES );

	g_das_nodes[i].dsp_preset = dsp_preset;
	g_das_nodes[i].fused = true;
	g_das_nodes[i].vplayer = proom->vplayer;

	// calculate node scanning range_max based on room size
	
	if ( !proom->bskyabove )
	{
		// inside range - halls & tunnels have nodes every 5*width
		g_das_nodes[i].range_max = fpmin(DAS_DIST_MAX, MIN(proom->width_max * 5, proom->length_max) ); 
		g_das_nodes[i].range_min = DAS_DIST_MIN;
	}
	else
	{
		// outside range
		g_das_nodes[i].range_max = DAS_DIST_MAX_OUTSIDE;
		g_das_nodes[i].range_min = DAS_DIST_MIN_OUTSIDE;
	}

	g_das_nodes[i].fseesplayer = false;
	g_das_nodes[i].dist = 0;

	g_das_nodes[i].room = *proom;

	// update last node visible as this node

	g_pdas_last_node = &(g_das_nodes[i]);
}

// check all updated nodes,
// return dsp_preset of largest node (by area) that can see player
// return -1 if no preset found

// NOTE: outside nodes can't see player if player is inside and vice versa
// foutside is true if player is outside

int DAS_GetDspPreset( bool foutside )
{
	int dsp_preset = -1;

	int i;
	// int dist_min = 100000;
	int area_max = 0;
	int area;

	// find node that represents room with greatest floor area, return its preset.
	
	for (i = 0; i < DAS_CNODES; i++)
	{
		if (g_das_nodes[i].fused && g_das_nodes[i].fseesplayer)
		{
			area = (g_das_nodes[i].room.width_max * g_das_nodes[i].room.length_max);
			
			if ( g_das_nodes[i].room.bskyabove == foutside )
			{
				if (area > area_max)
				{
					area_max = area;
					dsp_preset = g_das_nodes[i].dsp_preset;

					// save pointer to last node that saw player

					g_pdas_last_node = &(g_das_nodes[i]);
				}
			}			
/*		

			// find nearest node, return its preset

			if (g_das_nodes[i].dist < dist_min)
			{
				if ( g_das_nodes[i].room.bskyabove == foutside )
				{
					dist_min = g_das_nodes[i].dist;
					dsp_preset = g_das_nodes[i].dsp_preset;

					// save pointer to last node that saw player

					g_pdas_last_node = &(g_das_nodes[i]);
		
				}
			}
*/
		}
	}
	
	return dsp_preset;
}

// custom trace filter: 
// a) never hit player or monsters or entities
// b) always hit world, or moveables or static props

class CTraceFilterDAS : public ITraceFilter
{
public:
	bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		IClientUnknown *pUnk = static_cast<IClientUnknown*>(pHandleEntity);
		IClientEntity *pEntity;

		if ( !pUnk )
			return false;

		// don't hit non-collideable props

		if ( StaticPropMgr()->IsStaticProp( pHandleEntity ) )
		{

			ICollideable *pCollide = StaticPropMgr()->GetStaticProp( pHandleEntity);
			if (!pCollide)
				return false;
		}

		// don't hit any ents

		pEntity = pUnk->GetIClientEntity();
		
		if ( pEntity )
			return false;

		return true;
	}

	virtual TraceType_t	GetTraceType() const
	{
		return TRACE_EVERYTHING_FILTER_PROPS;
	}
};

#define DAS_TRACE_MASK		(CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW)

// returns true if clear line exists between node and player
// if node can see player, sets up node distance and flag fseesplayer

bool DAS_TraceNodeToPlayer( das_room_t *proom, das_node_t *pnode )
{
	trace_t trP;
	CTraceFilterDAS filterP;
	bool fseesplayer = false;
	float dist;
	Ray_t ray;
	ray.Init( proom->vplayer, pnode->vplayer );
	
	g_pEngineTraceClient->TraceRay( ray, DAS_TRACE_MASK, &filterP, &trP );
	dist = VectorLength( proom->vplayer - pnode->vplayer );

	// player can still see previous room selection point, and it's less than n feet away, 
	// then flag this node as visible

	if ( !trP.DidHit() && (dist <= DAS_DIST_MAX) )
	{
		fseesplayer = true;
		pnode->dist = dist;
	}
	
	pnode->fseesplayer = fseesplayer;

	return fseesplayer;
}

// update room boundary maxs, mins

void DAS_SetRoomBounds( das_room_t *proom, Vector &hit, bool bheight )
{
	Vector maxs, mins;

	maxs = proom->room_maxs;
	mins = proom->room_mins;

	if (!bheight)
	{
		if (hit.x > maxs.x)
			maxs.x = hit.x;

		if (hit.x < mins.x)
			mins.x = hit.x;

		if (hit.z > maxs.z)
			maxs.z = hit.z;

		if (hit.z < mins.z)
			mins.z = hit.z;
	}

	if (bheight)
	{
		if (hit.y > maxs.y)
			maxs.y = hit.y;

		if (hit.y < mins.y)
			mins.y = hit.y;
	}

	proom->room_maxs = maxs;
	proom->room_mins = mins;
}

// when all walls are updated, calculate max length, width, height, reflectivity, sky hit%, room center
// returns true if room parameters are in good location to place a node
// returns false if room parameters are not in good location to place a node
// note: false occurs if up vector doesn't hit sky, but one or more up diagonal vectors do hit sky

#ifdef PORTAL2
ConVar  das_process_overhang_spaces( "das_process_overhang_spaces", "1" );
#else
ConVar  das_process_overhang_spaces( "das_process_overhang_spaces", "0" );
#endif

bool DAS_CalcRoomProps( das_room_t *proom )
{
	int length_max = 0;
	int width_max = 0;
	int height_max = 0;
	int dist[4];
	float area1, area2;
	int height;
	int i;
	int j;
	int k;

	if( das_process_overhang_spaces.GetInt() != 1 )
	{

		bool b_diaghitsky = false;

		// reject this location if up vector doesn't hit sky, but 
		// one or more up diagonals do hit sky - 
		// in this case, player is under a slight overhang, narrow bridge, or
		// standing just inside a window or doorway. keep looking for better node location
		

	 	for (i = IVEC_DIAG_UP; i < IVEC_UP; i++)
	 	{
	 		if (proom->skyhits[i] > 0.0)
	 			b_diaghitsky = true;
	 	}

		if (b_diaghitsky && !(proom->skyhits[IVEC_UP] > 0.0))
			return false;
	}

	// get all distance pairs

	for (i = 0; i < IVEC_DIAG_UP; i+=2)
		dist[i/2] = proom->dist[i] + proom->dist[i+1];	// 1st pair is width
	
	// if areas differ by more than 25%
	// select the pair with the greater area

	// if areas do not differ by more than 25%, select the pair with the 
	// longer measured distance. Filters incorrect selection due to diagonals.

	area1 = (float)(dist[0] * dist[1]);
	area2 = (float)(dist[2] * dist[3]);

	area1 = (int)area1 == 0 ? 1.0 : area1;
	area2 = (int)area2 == 0 ? 1.0 : area2;
	
	if ( PercentDifference(area1, area2) > 0.25 )
	{
		// areas are more than 25% different - select pair with greater area

		j = area1 > area2 ? 0 : 2;
	}
	else
	{
		// select pair with longer measured distance

		int k = 0; // index to max dist
		int dmax = 0;

		for (i = 0; i < 4; i++)
		{
			if (dist[i] > dmax)
			{
				dmax = dist[i];
				k = i;
			}
		}

		j = k > 1 ? 2 : 0;
	}

	
	// width is always the smaller of the dimensions

	width_max = MIN (dist[j], dist[j+1]);
	length_max = MAX (dist[j], dist[j+1]);

	// get max height

	for (i = IVEC_DIAG_UP; i < IVEC_DOWN; i++)
	{
		height = proom->dist[i];

		if (height > height_max)
			height_max = height;
	}

	proom->length_max = length_max;
	proom->width_max = width_max;
	proom->height_max = height_max;
			
	// get room max,min from chosen width, depth
	// 0..3 or 4..7

	for ( i = j*2; i < 4+(j*2); i++)
		DAS_SetRoomBounds( proom, proom->hit[i], false );
	
	// get room height min from down trace

	proom->room_mins.z = proom->hit[IVEC_DOWN].z;

	// reset room height max to player trace height

	proom->room_maxs.z = proom->vplayer.z;

	// draw box around room max,min

	if (das_debug.GetInt() == 6)
	{
		// draw box around all objects detected
		Vector maxs = proom->room_maxs;
		Vector mins = proom->room_mins;
		Vector orig = (maxs + mins) / 2.0;
		Vector absMax = maxs - orig;
		Vector absMin = mins - orig;

		CDebugOverlay::AddBoxOverlay( orig, absMax, absMin, vec3_angle, 255, 0, 255, 0, 60.0f );
	}
	// calculate average reflectivity

	float refl = 0.0;

	// average reflectivity for walls

	// 0..3 or 4..7

	for ( k = 0, i = j*2; i < 4+(j*2); i++, k++)
	{
		refl += proom->reflect[i];
		proom->refl_walls[k] = proom->reflect[i];
	}
	
	// assume ceiling is open

	proom->refl_walls[4] = 0.0;	 

	// get ceiling reflectivity, if any non zero

	for ( i = IVEC_DIAG_UP; i < IVEC_DOWN; i++)
	{
		if (proom->reflect[i] == 0.0)
		{
			// if any upward trace hit sky, exit;
			// ceiling reflectivity is 0.0

			proom->refl_walls[4] = 0.0;

			i = IVEC_DOWN;	// exit loop
		}
		else
		{

			// upward trace didn't hit sky, keep checking

			proom->refl_walls[4] = proom->reflect[i];	
		}
	}

	// add in ceiling reflectivity, if any
	
	refl += proom->refl_walls[4];

	// get floor reflectivity
		
	refl += proom->reflect[IVEC_DOWN];
	proom->refl_walls[5] = proom->reflect[IVEC_DOWN];

	proom->refl_avg = refl / 6.0;

	// calculate sky hit percent for this wall

	float sky_pct = 0.0;

	// 0..3 or 4..7

	for ( i = j*2; i < 4+(j*2); i++)
		sky_pct += proom->skyhits[i];
	
	for ( i = IVEC_DIAG_UP; i < IVEC_DOWN; i++)
	{
		if (proom->skyhits[i] > 0.0)
		{
			// if any upward trace hit sky, exit loop
			sky_pct += proom->skyhits[i];
			i = IVEC_DOWN;
		}
	}

	// get floor skyhit

	sky_pct += proom->skyhits[IVEC_DOWN];

	proom->sky_pct = sky_pct;

	// check for sky above
	proom->bskyabove = false;

	for (i = IVEC_DIAG_UP; i < IVEC_DOWN; i++)
	{
		if (proom->skyhits[i] > 0.0)
			proom->bskyabove = true;
	}

	return true;
}

// return true if trace hit solid
// return false if trace hit sky or didn't hit anything

bool DAS_HitSolid( trace_t *ptr )
{
	// if hit nothing return false

	if (!ptr->DidHit())
		return false;
	
	// if hit sky, return false (not solid)
	if (ptr->surface.flags & SURF_SKY)
		return false;

	return true;
}

// returns true if trace hit sky

bool DAS_HitSky( trace_t *ptr )
{
	if (ptr->DidHit() && (ptr->surface.flags & SURF_SKY))
		return true;
	if (!ptr->DidHit() )
	{
		float dz = ptr->endpos.z - ptr->startpos.z;
		if ( dz > 200*12.0f )
			return true;
	}
	return false;
}


bool DAS_ScanningForHeight( das_room_t *proom )
{
	return (proom->iwall >= IVEC_DIAG_UP);
}

bool DAS_ScanningForWidth( das_room_t *proom )
{
	return (proom->iwall < IVEC_DIAG_UP);
}

bool DAS_ScanningForFloor( das_room_t *proom )
{
	return (proom->iwall == IVEC_DOWN);
}

ConVar das_door_height("adsp_door_height", "112"); // standard door height hl2
ConVar das_wall_height("adsp_wall_height", "128"); // standard wall height hl2
ConVar das_low_ceiling("adsp_low_ceiling", "108"); // low ceiling height hl2


// set origin for tracing out to walls to point above player's head
// allows calculations over walls and floor obstacles, and above door openings

// WARNING: the current settings are optimal for skipping floor and ceiling clutter,
// and for detecting rooms without 'looking' through doors or windows. Don't change these cvars for hl2!

void DAS_SetTraceHeight( das_room_t *proom, trace_t *ptrU, trace_t *ptrD )
{
	// NOTE: when tracing down through player's box, endpos and startpos are reversed and 
	// startsolid and allsolid are true.

	int zup = abs(ptrU->endpos.z - ptrU->startpos.z);		// height above player's head
	int zdown = abs(ptrD->endpos.z - ptrD->startpos.z);		// distance to floor from player's head
	int h;
	h = zup + zdown;
	
	int door_height = das_door_height.GetInt();
	int wall_height = das_wall_height.GetInt();
	int low_ceiling = das_low_ceiling.GetInt();
	
	if (h > low_ceiling && h <= wall_height)
	{
		// low ceiling - trace out just above standard door height @ 112
		if (h > door_height)
			proom->vplayer.z = fpmin(ptrD->endpos.z, ptrD->startpos.z) + door_height + 1;	
		else
			proom->vplayer.z = fpmin(ptrD->endpos.z, ptrD->startpos.z) + h - 1;
	}
	else if ( h > wall_height )
	{
		// tall ceiling - trace out over standard walls @ 128

		proom->vplayer.z = fpmin(ptrD->endpos.z, ptrD->startpos.z) + wall_height + 1;
	}
	else
	{
		// very low ceiling, trace out from just below ceiling
		proom->vplayer.z = fpmin(ptrD->endpos.z, ptrD->startpos.z) + h - 1;
		proom->lowceiling = h;
	}		

	Assert (proom->vplayer.z <= ptrU->endpos.z);

	if (das_debug.GetInt() > 1)
	{
		// draw line to height, and between floor and ceiling

		CDebugOverlay::AddLineOverlay( ptrD->endpos, ptrU->endpos, 0, 255, 0, 255, false, 20 );
		
		Vector mins;
		Vector maxs;
		mins.Init(-1,-1,-2.0);
		maxs.Init(1,1,0);

		CDebugOverlay::AddBoxOverlay( proom->vplayer, mins, maxs, vec3_angle, 255, 0, 0, 0, 20 );

		CDebugOverlay::AddBoxOverlay( ptrU->endpos, mins, maxs, vec3_angle, 0, 255, 0, 0, 20 );
		CDebugOverlay::AddBoxOverlay( ptrD->endpos, mins, maxs, vec3_angle, 0, 255, 0, 0, 20 );

	}
}


// we still want to test for new dsp even if jumping in portal2
#ifdef PORTAL2
ConVar das_max_z_trace_length( "das_max_z_trace_length", "100000", FCVAR_NONE, "Maximum height of player and still test for adsp" );
#else
ConVar das_max_z_trace_length( "das_max_z_trace_length", "72", FCVAR_NONE, "Maximum height of player and still test for adsp"  );
#endif

// prepare room struct for new round of checks:
// clear out struct,
// init trace height origin by finding space above player's head
// returns true if player is in valid position to begin checks from 
bool DAS_StartTraceChecks( das_room_t *proom )
{
	// starting new check: store player position, init maxs, mins
	// HACK FOR SPLITSCREEN
	int nSlot = 0;

	proom->vplayer_eyes = MainViewOrigin( nSlot );
	proom->vplayer = MainViewOrigin( nSlot );

	proom->height_max = 0;
	proom->width_max = 0;
	proom->length_max = 0;
	proom->room_maxs.Init (0.0, 0.0, 0.0);
	proom->room_mins.Init (10000.0, 10000.0, 10000.0);

	proom->lowceiling = 0;

	// find point between player's head and ceiling - trace out to walls from here

	trace_t trU, trD;	
	CTraceFilterDAS filterU, filterD;

	Vector v_dir = g_das_vec3[IVEC_DOWN];	// down - find floor

	Vector endpoint = proom->vplayer + v_dir;

	Ray_t ray;
	ray.Init( proom->vplayer, endpoint );
	
	g_pEngineTraceClient->TraceRay( ray,  DAS_TRACE_MASK, &filterD, &trD );

	// if player jumping or in air, don't continue

 	if ( trD.DidHit() && ( abs(trD.endpos.z - trD.startpos.z) > das_max_z_trace_length.GetFloat() ) )
	{
 		return false;
	}

	v_dir = g_das_vec3[IVEC_UP];			// up - find ceiling

	endpoint = proom->vplayer + v_dir;

	ray.Init( proom->vplayer, endpoint );

	g_pEngineTraceClient->TraceRay( ray, DAS_TRACE_MASK, &filterU, &trU );

	// if down trace hits floor, set trace height, otherwise default is player eye location

	if ( DAS_HitSolid( &trD) )
		DAS_SetTraceHeight( proom, &trU, &trD );
	
	return true;
}

void DAS_DebugDrawTrace ( trace_t *ptr, int r, int g, int b, float duration, int imax)
{

	// das_debug == 3: draw horizontal trace bars for room width/depth detection
	// das_debug == 4: draw upward traces for height detection

	if (das_debug.GetInt() != imax)
		return;

	CDebugOverlay::AddLineOverlay( ptr->startpos, ptr->endpos, r, g, b, 255, false, duration );
	
	Vector mins;
	Vector maxs;
	mins.Init(-1,-1,-2.0);
	maxs.Init(1,1,0);

	CDebugOverlay::AddBoxOverlay( ptr->endpos, mins, maxs, vec3_angle, r, g, b, 0, duration );

}

// wall surface data

struct das_surfdata_t
{
	float dist;				// distance to player
	float reflectivity;		// acoustic reflectivity of material on surface
	Vector hit;				// trace hit location
	Vector norm;			// wall normal at hit location
};

// trace hit wall surface, get info about surface and store in surfdata struct
// if scanning for height, bounce a second trace off of ceiling and get dist to floor

void DAS_GetSurfaceData( das_room_t *proom, trace_t *ptr, das_surfdata_t *psurfdata )
{

	float dist;				// distance to player
	float reflectivity;		// acoustic reflectivity of material on surface
	Vector hit;				// trace hit location
	Vector norm;			// wall normal at hit location
	surfacedata_t *psurf;

	psurf = physprops->GetSurfaceData( ptr->surface.surfaceProps );
	
	reflectivity = psurf ? psurf->audio.reflectivity : DAS_REFLECTIVITY_NORM;

	// keep wall hit location and normal, to calc room bounds and center

	norm = ptr->plane.normal;

	// get length to hit location

	dist = VectorLength(ptr->endpos - ptr->startpos);

	// if started tracing from within player box, startpos & endpos may be flipped

	if (ptr->endpos.z >= ptr->startpos.z)
		hit = ptr->endpos;	
	else
		hit = ptr->startpos;

	// if checking for max height by bouncing several vectors off of ceiling:
	// ignore returned normal from 1st bounce, just search straight down from trace hit location

	if ( DAS_ScanningForHeight( proom ) && !DAS_ScanningForFloor( proom ) )
	{
		trace_t tr2;
		CTraceFilterDAS filter2;

		norm.Init(0.0, 0.0, -1.0);

		Vector endpoint = hit + ( norm * DAS_ROOM_TRACE_LEN );
		
		Ray_t ray;
		ray.Init( hit, endpoint );

		g_pEngineTraceClient->TraceRay( ray, DAS_TRACE_MASK, &filter2, &tr2 );

		//DAS_DebugDrawTrace( &tr2, 255, 255, 0, 10, 1);

		if (tr2.DidHit())
		{
			// get distance between surfaces

			dist = VectorLength(tr2.endpos - tr2.startpos);
		}
	}

	// set up surface struct and return

	psurfdata->dist = dist;
	psurfdata->hit = hit;
	psurfdata->norm = norm;
	psurfdata->reflectivity = reflectivity;

}


// algorithm for detecting approximate size of space around player. Handles player in corner & non-axis aligned rooms.
// also handles player on catwalk or player under small bridge/overhang.  
// The goal is to only change the dsp room description if the the player moves into 
// a space which is SIGNIFICANTLY different from the previously set dsp space.

// save player position. find a point above player's head and trace out from here.

// from player position, get max width and max length:

// from player position, 
// a) trace x,-x, y,-y axes 
// b) trace xy, -xy, x-y, -x-y diagonals
// c) select largest room size detected from max width, max length


// from player position, get height
// a) trace out along front-up (or left-up, back-up, right-up), save hit locations
// b) trace down -z from hit locations
// c) save max height

// when max width, max length, max height all updated, get new player position

// get average room size & wall materials:
// update averages with one traceline per frame only
// returns true if room is fully updated and ready to check

bool DAS_UpdateRoomSize( das_room_t *proom )
{
	Vector endpoint;
	Vector startpoint;
	Vector v_dir;
	int iwall;
	bool bskyhit = false;
	das_surfdata_t surfdata;

	// do nothing if room already fully checked

	if ( proom->broomready )
		return true;

	// cycle through all walls, floor, ceiling
	// get wall index 

	iwall = proom->iwall;
	
	// get height above player and init proom for new round of checks

	if (iwall == 0)
	{
		if (!DAS_StartTraceChecks( proom ))
			return false;		// bad location to check room - player is jumping etc.
	}

	// get trace vector

	v_dir = g_das_vec3[iwall];

	// trace out from trace origin, in axis-aligned direction or along diagonals

	// if looking for max height, trace from top of player's eyes

	if ( DAS_ScanningForHeight( proom ) )
	{
		startpoint = proom->vplayer_eyes;
		endpoint = proom->vplayer_eyes + v_dir;
	}
	else
	{
		startpoint = proom->vplayer;
		endpoint = proom->vplayer + v_dir;
	}

	// try less expensive world-only trace first (no props, no ents - just try to hit walls)

	trace_t tr;
	CTraceFilterWorldOnly filter;

	Ray_t ray;
	ray.Init( startpoint, endpoint );

	g_pEngineTraceClient->TraceRay( ray, CONTENTS_SOLID, &filter, &tr );

	// if didn't hit world, or we hit sky when looking horizontally,
	// retrace, this time including props

	if ( !DAS_HitSolid( &tr ) && DAS_ScanningForWidth( proom ) )
	{
		CTraceFilterDAS filterDas;

		ray.Init( startpoint, endpoint );
		g_pEngineTraceClient->TraceRay( ray, DAS_TRACE_MASK, &filterDas, &tr );
	}
	
	if (das_debug.GetInt() > 2)
	{
		// draw trace lines

		if ( DAS_HitSolid( &tr ) )
			DAS_DebugDrawTrace( &tr, 0, 255, 255, 10, DAS_ScanningForHeight( proom ) + 3);	
		else
			DAS_DebugDrawTrace( &tr, 255, 0, 0, 10, DAS_ScanningForHeight( proom ) + 3);	// red lines if sky hit or no hit
	}

	// init surface data with defaults, in case we didn't hit world

	surfdata.dist			= DAS_ROOM_TRACE_LEN;
	surfdata.reflectivity	= DAS_REFLECTIVITY_SKY;	// assume sky or open area
	surfdata.hit			= endpoint;				// trace hit location
	surfdata.norm			= -v_dir;

	// check for sky hits

	if ( DAS_HitSky( &tr ) )
	{
		bskyhit = true;

		if ( DAS_ScanningForWidth( proom ) )
			// ignore horizontal sky hits for distance calculations
			surfdata.dist = 1.0;
		else
			surfdata.dist = surfdata.dist; // debug
	}

	// get length of trace if it hit world

	// if hit solid and not sky (tr.DidHit() && !bskyhit)
	// get surface information

	if ( DAS_HitSolid( &tr) )
		DAS_GetSurfaceData( proom, &tr, &surfdata );
	
	// store surface data

	proom->dist[iwall]		= surfdata.dist;
	proom->reflect[iwall]	= clamp(surfdata.reflectivity, 0.0, 1.0);
	proom->skyhits[iwall]	= bskyhit ? 0.1 : 0.0;
	proom->hit[iwall]		= surfdata.hit;
	proom->norm[iwall]		= surfdata.norm;

	// update wall counter

	proom->iwall++;
	
	if (proom->iwall == DAS_CWALLS)
	{
		bool b_good_node_location;

		// calculate room mins, maxs, reflectivity etc

		b_good_node_location = DAS_CalcRoomProps( proom );

		// reset wall counter

		proom->iwall = 0;
		proom->broomready = b_good_node_location;	// room ready to check if good node location

		return b_good_node_location;	
	}

	return false;			// room not yet fully updated
}

// create entity enumerator for counting ents & summing volume of ents in room

class CDasEntEnum : public IPartitionEnumerator
{
	public:
	int m_count;		// # of ents in space
	float m_volume;		// space occupied by ents

	public:
	
	void Reset()
	{
		m_count = 0;
		m_volume = 0.0;
	}

	// called with each handle...

	IterationRetval_t EnumElement( IHandleEntity *pHandleEntity )
	{		
		float vol;

		// get bounding box of entity
		// Generate a collideable
		
		ICollideable *pCollideable = g_pEngineTraceClient->GetCollideable( pHandleEntity );

		if ( !pCollideable )
			return ITERATION_CONTINUE;

		// Check for solid

		if ( !IsSolid( pCollideable->GetSolid(), pCollideable->GetSolidFlags() ) )
			return ITERATION_CONTINUE;
		
		m_count++;
		
		// compute volume of space occupied by entity
		Vector mins = pCollideable->OBBMins();
		Vector maxs = pCollideable->OBBMaxs();
		
		vol = fabs((maxs.x - mins.x) * (maxs.y - mins.y) * (maxs.z - mins.z));

		m_volume += vol;	// add to total vol

		if (das_debug.GetInt() == 5)
		{
			// draw box around all objects detected

			Vector orig = pCollideable->GetCollisionOrigin();
			CDebugOverlay::AddBoxOverlay( orig, mins, maxs, pCollideable->GetCollisionAngles(), 255, 0, 255, 0, 60.0f );
		}

		return ITERATION_CONTINUE;
	}
};

// determine # of solid ents/props within detected room boundaries
// and set diffusion based on count of ents and spatial volume of ents

void DAS_SetDiffusion( das_room_t *proom )
{
	// BRJ 7/12/05
	// This was commented out because the y component of proom->room_mins, proom->room_maxs was never
	// being computed, causing a bogus box to be sent to the partition system. The results of
	// this computation (namely the diffusion + ent_count fields of das_room_t) were never being used. 
	// Therefore, we'll avoid the enumeration altogether

	proom->diffusion = 0.0f;
	proom->ent_count = 0;

	/*
	CDasEntEnum enumerator;
	SpatialPartitionListMask_t mask = PARTITION_CLIENT_SOLID_EDICTS;	// count only solid ents in room
	int count;
	float vol;
	float volroom;
	float dfn;
	
	enumerator.Reset();
	
	SpatialPartition()->EnumerateElementsInBox(mask, proom->room_mins, proom->room_maxs, true, &enumerator );	
	
	count = enumerator.m_count;
	vol = enumerator.m_volume;

	// compute diffusion from volume
	
	// how much space around player is filled with props?

	volroom = (proom->room_maxs.x - proom->room_mins.x) * (proom->room_maxs.y - proom->room_mins.y) * (proom->room_maxs.z - proom->room_mins.z);
	volroom = fabs(volroom);

	if ( !(int)volroom )
		volroom = 1.0;

	dfn = vol / volroom;		// % of total volume occupied by props

	dfn = clamp (dfn, 0.0, 1.0);

	proom->diffusion = dfn;
	proom->ent_count = count;
	*/
}

// debug routine to display current room params

void DAS_DisplayRoomDEBUG( das_room_t *proom, bool fnew, float preset )
{
	float dx,dy,dz;
	Vector ctr;
	float count;

	if (das_debug.GetInt() == 0)
		return;

	dx = proom->length_max / 12.0;
	dy = proom->width_max / 12.0;
	dz = proom->height_max / 12.0;
	
	float refl = proom->refl_avg;
	
	count = (float)(proom->ent_count);
	float fsky = (proom->bskyabove ? 1.0 : 0.0);

	if (fnew)
		DevMsg( "NEW DSP NODE: size:(%.0f,%.0f) height:(%.0f) dif %.4f : refl %.4f : cobj: %.0f : sky %.0f \n", dx, dy, dz, proom->diffusion, refl, count, fsky);

	if (!fnew && preset < 0.0)
		return;

	if (preset >= 0.0)
	{
		if (proom == NULL)
			return;

		DevMsg( "DSP PRESET: %.0f size:(%.0f,%.0f) height:(%.0f) dif %.4f : refl %.4f : cobj: %.0f : sky %.0f \n", preset, dx, dy, dz, proom->diffusion, refl, count, fsky);
		return;
	}

	// draw box around new node location

	Vector mins;
	Vector maxs;
	mins.Init(-8,-8,-16);
	maxs.Init(8,8,0);

	CDebugOverlay::AddBoxOverlay( proom->vplayer, mins, maxs, vec3_angle, 0, 0, 255, 0, 1000.0f );

	// draw red box around node origin

	mins.Init(-0.5,-0.5,-1.0);
	maxs.Init(0.5,0.5,0);

	CDebugOverlay::AddBoxOverlay( proom->vplayer, mins, maxs, vec3_angle, 255, 0, 0, 0, 1000.0f );

	CDebugOverlay::AddTextOverlay( proom->vplayer, 0, 10, 1.0, "DSP NODE" );
}

// check newly calculated room parameters against current stored params.
// if different, return true.
// NOTE: only call when all proom params have been calculated.
// return false if this is not a good location for creating a new node

bool DAS_CheckNewRoom( das_room_t *proom )
{	
	bool bnewroom;
	float dw,dw2,dr,ds,dh;
	int cchanged = 0;
	das_room_t *proom_prev = NULL;
	Vector2D v2d;
	Vector v3d;
	float dist;


	// player can't see previous node, determine if this is a good place to lay down 
	// a new node.  Get room at last seen node for comparison

	if (g_pdas_last_node)
		proom_prev = &(g_pdas_last_node->room);

	// no previous room node saw player, go create new room node

	if (!proom_prev)
	{
		bnewroom = true;
		goto check_ret;
	}

	// if player not at least n feet from last node, return false
	
	v3d = proom->vplayer - proom_prev->vplayer;
	v2d.Init(v3d.x, v3d.y);

	dist = Vector2DLength(v2d);

	if (dist <= DAS_DIST_MIN)
		return false;

	// see if room size has changed significantly since last node

	bnewroom = true;

	dw = 0.0;
	dw2 = 0.0;
	dh = 0.0;
	dr = 0.0;

	if ( proom_prev->width_max != 0 )
		dw = (float)proom->width_max / (float)proom_prev->width_max;	// max width delta

	if ( proom_prev->length_max != 0 )
		dw2 = (float)proom->length_max / (float)proom_prev->length_max;	// max length delta

	if ( proom_prev->height_max != 0 )
		dh = (float)proom->height_max / (float)proom_prev->height_max;	// max height delta

	if ( proom_prev->refl_avg != 0.0 )
		dr = proom->refl_avg / proom_prev->refl_avg;					// reflectivity delta

	ds = fabs( proom->sky_pct - proom_prev->sky_pct);					// sky hits delta

	if (dw > 1.0) dw = 1.0 / dw;
	if (dw2 > 1.0) dw = 1.0 / dw2;
	if (dh > 1.0) dh = 1.0 / dh;
	if (dr > 1.0) dr = 1.0 / dr;

	if ( (1.0 - dw) >= DAS_WIDTH_MIN )
		cchanged++;
		
	if ( (1.0 - dw2) >= DAS_WIDTH_MIN )
		cchanged++;

//	if ( (1.0 - dh) >= DAS_WIDTH_MIN )	// don't change room based on height change
//		cchanged++;

	// new room only if at least 1 changed

	if (cchanged >= 1)
		goto check_ret;

//	if ( (1.0 - dr) >= DAS_REFL_MIN )	// don't change room based on reflectivity change
//		goto check_ret;

//	if (ds >= DAS_SKYHIT_MIN )
//		goto check_ret;

	// new room if sky above changes state

	if (proom->bskyabove != proom_prev->bskyabove)
		goto check_ret;

	// room didn't change significantly, return false

	bnewroom = false;


check_ret:
	
	if ( bnewroom )
	{
		// if low ceiling detected < 112 units, and max height is > low ceiling height by 20%, discard - no change
		// this detects player in doorway, under pipe or narrow bridge
		
		if ( proom->lowceiling && (proom->lowceiling < proom->height_max))
		{
			float h = (float)(proom->lowceiling) / (float)proom->height_max;

			if (h < 0.8)
				return false;
		}

		DAS_SetDiffusion( proom );
	}

	DAS_DisplayRoomDEBUG( proom, bnewroom, -1.0 );

	return bnewroom;
}


extern int DSP_ConstructPreset( bool bskyabove, int width, int length, int height, float fdiffusion, float freflectivity, float *psurf_refl, int inode, int cnodes);

// select new dsp_room based on size, wall materials
// (or modulate params for current dsp)
// returns new preset # for dsp_automatic


int DAS_GetRoomDSP( das_room_t *proom, int inode )
{
	
	// preset constructor
	// call dsp module with params, get dsp preset back

	bool bskyabove		= proom->bskyabove;
	int width			= proom->width_max;
	int length			= proom->length_max;
	int height			= proom->height_max;
	float fdiffusion	= proom->diffusion;
	float freflectivity = proom->refl_avg;
	float surf_refl[6];

	// fill array of surface reflectivities - for left,right,front,back,ceiling,floor

	for (int i = 0; i < 6; i++)
		surf_refl[i] = proom->refl_walls[i];	
	
	return DSP_ConstructPreset( bskyabove, width, length, height, fdiffusion, freflectivity, surf_refl, inode, DAS_CNODES);

}

// main entry point: call once per frame to update dsp_automatic
// for automatic room detection.  dsp_room must be set to DSP_AUTOMATIC to enable.
// NOTE: this routine accumulates traceline information over several frames - it
// never traces more than 3 times per call, and normally just once per call.

void DAS_CheckNewRoomDSP()
{
	VPROF("DAS_CheckNewRoomDSP");
	das_room_t *proom = &g_das_room;
	int dsp_preset;
	bool bRoom_ready = false;

	// if listener has not been updated, do nothing
	if ( !SND_IsListenerValid() )
		return;

	if ( !SND_IsInGame() )
		return;

	// make sure we init nodes & vectors first time this is called

	if ( !g_bdas_init_nodes )
	{
		g_bdas_init_nodes = 1;
		DAS_InitNodes();
	}

	if ( !DSP_CheckDspAutoEnabled())
	{
		// make sure room params are reinitialized each time autoroom is selected

		g_bdas_room_init = 0;		
		return;
	}

	if ( !g_bdas_room_init )
	{
		g_bdas_room_init = 1;

		DAS_InitAutoRoom( proom );
	}

	// get time
	
	double dtime = g_pSoundServices->GetHostTime();
	
	// compare to previous time - don't check for new room until timer expires
	// ie: wait at least DAS_AUTO_WAIT seconds betweeen preset changes

	if ( fabs(dtime - proom->last_dsp_change) < DAS_AUTO_WAIT )
		return;

	// first, update room size parameters, see if room is ready to check - if room is updated, return true right away

	// 3 traces per frame while accumulating room size info

	for (int i = 0 ; i < 3; i++)
		bRoom_ready = DAS_UpdateRoomSize( proom );

	if (!bRoom_ready)
		return;
	
	// new room defaults to false
	//g_current_das_room_new = false;	
	//g_current_das_room_changed = false;	

	if ( !g_bdas_create_new_node )
	{
		// next, check all nodes for line of sight to player - if all checked, return true right away

		if ( !DAS_CheckNextNode( proom ) )
		{
			// check all nodes first

			return;
		}

		// find out if any previously stored nodes can see player,
		// if so, get closest node's dsp preset

		dsp_preset = DAS_GetDspPreset( proom->bskyabove );

		if (dsp_preset != -1)
		{
			// an existing node can see player - just set preset and return
				
			if (dsp_preset != dsp_room_GetInt())
			{		
				// changed preset, so update timestamp

				proom->last_dsp_change = g_pSoundServices->GetHostTime();
				
				if (g_pdas_last_node)
				{
					DAS_DisplayRoomDEBUG( &(g_pdas_last_node->room), false, (float)dsp_preset );
					//memcpy(&g_current_adsp_auto_params, &(g_pdas_last_node->room), sizeof(g_current_adsp_auto_params));

					// if it's changed is not new?	
					g_current_das_room_changed = true;
					g_current_das_room_new = false;
					DAS_StoreRoomVarsAPI(&(g_pdas_last_node->room));
				}
			}

			DSP_SetDspAuto( dsp_preset );

			goto check_new_room_exit;
		} 
	}

	g_bdas_create_new_node = true;

	// no nodes can see player, need to try to create a new one

	// check for 'new' room around player

	
	if ( DAS_CheckNewRoom( proom ) )
	{
		// new room found - update dsp_automatic

		dsp_preset = DAS_GetRoomDSP( proom, DAS_GetNextNodeIndex() );

		DSP_SetDspAuto( dsp_preset );
				
		// changed preset, so update timestamp

		proom->last_dsp_change = g_pSoundServices->GetHostTime();

		// save room as new node

		DAS_StoreNode( proom, dsp_preset );

		g_current_das_room_new = true;
		DAS_StoreRoomVarsAPI(proom);

		goto check_new_room_exit;
	}

check_new_room_exit:

	// reset new node creation flag - start checking for visible nodes again

	g_bdas_create_new_node = false;

	// reset room checking flag - start checking room around player again

	proom->broomready = false;

	// reset node checking flag - start checking nodes around player again

	DAS_ResetNodes();

	return;
}

//
//
//

// remap contents of volumes[] arrary if sound originates from player, or is music, and is 100% 'mono' 
// ie: same volume in all channels

void RemapPlayerOrMusicVols(  channel_t *ch, float volumes[CCHANVOLUMES/2], bool fplayersound, bool fmusicsound, float mono )
{
	VPROF_("RemapPlayerOrMusicVols", 2, VPROF_BUDGETGROUP_OTHER_SOUND, false, BUDGETFLAG_OTHER );

	if ( !fplayersound && !fmusicsound )
		return;	// no remapping

	if ( ch->flags.bSpeaker )
		return; // don't remap speaker sounds rebroadcast on player

	// get total volume

	float vol_total = 0.0;
	int k;

	for (k = 0; k < CCHANVOLUMES/2; k++)
		vol_total += (float)volumes[k];

	if ( !g_AudioDevice->IsSurround() )
	{
		if (mono < 1.0)
			return;

		// remap 2 chan non-spatialized versions of player and music sounds
		// note: this is required to keep volumes same as 4 & 5 ch cases!
		
		float vol_dist_music[] =  {1.0, 1.0};	// FL, FR music volumes
		float vol_dist_player[] = {1.0, 1.0};	// FL, FR player volumes
		float *pvol_dist;

		pvol_dist = (fplayersound ? vol_dist_player : vol_dist_music);

		for (k = 0; k < 2; k++)
			volumes[k] = clamp(vol_total * pvol_dist[k], 0, 255);

		return;
	}

	// surround sound configuration...

	if ( fplayersound ) // && (ch->bstereowav && ch->wavtype != CHAR_DIRECTIONAL && ch->wavtype != CHAR_DISTVARIANT) )
	{
		// NOTE: player sounds also get n% overall volume boost.
		
		//float vol_dist5[]   = {0.29, 0.29, 0.09, 0.09, 0.63};	// FL, FR, RL, RR, FC - 5 channel (mono source) volume distribution		
		//float vol_dist5st[] = {0.29, 0.29, 0.09, 0.09, 0.63};	// FL, FR, RL, RR, FC - 5 channel (stereo source) volume distribution
		
		float vol_dist5[]   = {0.30, 0.30, 0.09, 0.09, 0.59};	// FL, FR, RL, RR, FC - 5 channel (mono source) volume distribution		
		float vol_dist5st[] = {0.30, 0.30, 0.09, 0.09, 0.59};	// FL, FR, RL, RR, FC - 5 channel (stereo source) volume distribution
		
		float vol_dist4[]   = {0.50, 0.50, 0.15, 0.15, 0.00};	// FL, FR, RL, RR, 0  - 4 channel (mono source) volume distribution
		float vol_dist4st[] = {0.50, 0.50, 0.15, 0.15, 0.00};	// FL, FR, RL, RR, 0  - 4 channel (stereo source)volume distribution

		float *pvol_dist;
		
		if ( ch->flags.bstereowav && (ch->wavtype == CHAR_OMNI || ch->wavtype == CHAR_SPATIALSTEREO || ch->wavtype == 0 || ch->wavtype == CHAR_DIRSTEREO))
		{
			pvol_dist = (g_AudioDevice->IsSurroundCenter() ? vol_dist5st : vol_dist4st);	
		}
		else
		{
			pvol_dist = (g_AudioDevice->IsSurroundCenter() ? vol_dist5 : vol_dist4);
		}

		for (k = 0; k < 5; k++)
			volumes[k] = clamp(vol_total * pvol_dist[k], 0, 255);

		return;
	}

	// Special case for music in surround mode

	if ( fmusicsound )
	{
		float vol_dist5[] = {0.5, 0.5, 0.25, 0.25, 0.0};	// FL, FR, RL, RR, FC - 5 channel distribution
		float vol_dist4[] = {0.5, 0.5, 0.25, 0.25, 0.0};	// FL, FR, RL, RR, 0  - 4 channel distribution
		float *pvol_dist;

		pvol_dist = (g_AudioDevice->IsSurroundCenter() ? vol_dist5 : vol_dist4);

		for (k = 0; k < 5; k++)
			volumes[k] = clamp(vol_total * pvol_dist[k], 0, 255);

		return;
	}

	return;
}

void SND_MergeVolumes( const int build_volumes[ MAX_SPLITSCREEN_CLIENTS ][CCHANVOLUMES/2], int volumes[CCHANVOLUMES/2] )
{
	// Three methods
	// Sum and clamp == 0
	// Use max == 1
	// Use avg == 2

	for ( int v = 0; v < CCHANVOLUMES/2; ++v )
	{
		int val = 0;
		int count = 0;
		int maxVal = INT_MIN;
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			int check = build_volumes[ hh ][ v ];
			if ( check > maxVal )
				maxVal = check;
			val += check;
			++count;
		}

		switch ( g_SndMergeMethod )
		{
		default:
		case SND_MERGE_SUMANDCLIP:
			{
				volumes[ v ] = MIN( val, 255 );
			}
			break;
		case SND_MERGE_MAX:
			{
				volumes[ v ] = maxVal;
			}
			break;
		case SND_MERGE_AVG:
			{
				if ( count > 0 )
				{
					volumes[ v ] = val / count;
				}
				else
				{
					volumes[ v ] = 0;
				}
			}
			break;
		}
	}
}

// float version
void SND_MergeVolumes( const float build_volumes[ MAX_SPLITSCREEN_CLIENTS ][CCHANVOLUMES/2], float volumes[CCHANVOLUMES/2] )
{
	// Three methods
	// Sum and clamp == 0
	// Use max == 1
	// Use avg == 2

	for ( int v = 0; v < CCHANVOLUMES/2; ++v )
	{
		float val = 0;
		float count = 0;
		float maxVal = 0.0;
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			float check = build_volumes[ hh ][ v ];
			if ( check > maxVal )
				maxVal = check;
			val += check;
			++count;
		}

		switch ( g_SndMergeMethod )
		{
		default:
		case SND_MERGE_SUMANDCLIP:
			{
				volumes[ v ] = MIN( val, 1.0 );
			}
			break;
		case SND_MERGE_MAX:
			{
				volumes[ v ] = maxVal;
			}
			break;
		case SND_MERGE_AVG:
			{
				if ( count > 0 )
				{					
					volumes[ v ] = val / count;
				}
				else
				{
					volumes[ v ] = 0.0;
				}
			}
			break;
		}
	}
}

static CInterlockedInt s_nSoundGuid = 0;
static int s_nMaxQueuedGUID = 0;	// Max GUID to go through the queue. Used to optimize some tests.
static CUtlVector<activethreadsound_t> g_ActiveSoundsLastUpdate( 0, MAX_CHANNELS );

static int SND_GetGUID()
{
	int nextGUID = ++s_nSoundGuid;
	if ( nextGUID < 0 )
	{
		s_nSoundGuid = nextGUID = 1;			// No point having negative GUIDs
	}
	return nextGUID;
}

void SND_ActivateChannel( channel_t *pChannel, int nGUID )
{
	Q_memset( pChannel, 0, sizeof(*pChannel) );
	g_ActiveChannels.Add( pChannel );
	pChannel->guid = nGUID;
	pChannel->hrtf.lerp = 0.0f;
}

bool IsSoundSourceViewEntity( int soundsource )
{
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		if ( soundsource == g_pSoundServices->GetViewEntity( hh ) )
			return true;
	}
	return false;
}

ConVar voice_minimum_gain("voice_minimum_gain", "0.5");


/*
=================
SND_ExecuteUpdateOperators
=================
*/
ConVar snd_sos_exec_when_paused( "snd_sos_exec_when_paused", "1" );

void SND_ExecuteUpdateOperators( channel_t *ch )
{
	// don't execute operators if game is paused
	if( g_pSoundServices->IsGamePaused() && !snd_sos_exec_when_paused.GetInt() )
	{
		return;
	}

	// sanity check
	if( !ch->m_pStackList || ( ch->m_pStackList && !ch->m_pStackList->HasStack( CSosOperatorStack::SOS_UPDATE ) ) )
	{
		return;
	}

	VPROF( "SND_ExecuteUpdateOperators" );
	
	//////////////////////////////////////////////////////////////////////////
	// set all scratch pad settings
	//////////////////////////////////////////////////////////////////////////
	// setup scratchpad
	g_scratchpad.SetPerExecution( ch, NULL );

#if !defined( _X360 )
	// Currently we don't process voice channels via operators
	if ( ch->sfx && 
	ch->sfx->pSource && 
	ch->sfx->pSource->GetType() == CAudioSource::AUDIO_SOURCE_VOICE )
	{
		Log_Warning( LOG_SOUND_OPERATOR_SYSTEM, "Voice channel attempting to be processed by operators" );
		// Voice_Spatialize( ch );
	}
#endif


	//////////////////////////////////////////////////////////////////////////
	// Execute operators
	//////////////////////////////////////////////////////////////////////////
	ch->m_pStackList->Execute( CSosOperatorStack::SOS_UPDATE, ch, &g_scratchpad );


	// ------------------------- post process stuff ----------------------------

	// prevent left/right/front/rear/center volumes from changing too quickly & producing pops
	ChannelUpdateVolXfade( ch );

	// end of first time spatializing sound
	if ( SND_IsInGame() || toolframework->InToolMode() )
	{
		ch->flags.bfirstpass = false;
	}

}           


/*
=================
SND_Spatialize
=================
*/
void SND_Spatialize(channel_t *ch)
{
	VPROF( "SND_Spatialize" );

	if (ch->wavtype == CHAR_HRTF)
	{
		Vector origin;
		IClientEntity *pEnt = ch->hrtf.follow_entity ? entitylist->GetClientEntity(ch->soundsource) : nullptr;
		if (pEnt != nullptr)
		{
			origin = pEnt->GetRenderOrigin();
		}
		else
		{
			origin = ch->origin;
		}

		if (ch->hrtf.debug_lock_position == false)
		{
			QAngle listener_angles;

			//Calculate the listener origin as some distance behind the camera ('snd_hrtf_distance_behind') as this
			//gives better results for HRTF. For nearby sounds we want to make it closer to the camera position
			//so sounds behind us don't sound like they are in front.
			float distance_behind = snd_hrtf_distance_behind.GetFloat();
			if ( distance_behind < 0.0f )
			{
				distance_behind = 0.0f;
			}

			if ( distance_behind > 100.0f )
			{
				distance_behind = 100.0f;
			}

			Vector listener_origin_modified = listener_origin[0] - distance_behind*listener_forward[0];
			const float dist_to_sound = MIN((listener_origin_modified - origin).Length(), (listener_origin[0] - origin).Length());
			if ( dist_to_sound < distance_behind )
			{
				listener_origin_modified = listener_origin[0] - dist_to_sound*listener_forward[0];
			}

			// sound_pos is really sound_pos_listener_relative
			Vector sound_pos = listener_origin_modified - origin;
			VectorAngles(listener_forward[0], listener_angles);

			matrix3x4_t mat;

			AngleMatrix(listener_angles, mat);
			sound_pos = mat.TransformVectorByInverse(sound_pos);

			VectorNormalize(sound_pos);

			//Swizzle our co-ordinate system to Phonon's.
			ch->hrtf.vec.x = sound_pos.y;
			ch->hrtf.vec.y = -sound_pos.z;
			ch->hrtf.vec.z = sound_pos.x;
		}

		//Give some reasonable default behavior for lerping off hrtf when
		//close to the sound. Can always be overridden in operator stacks.
		Vector diff = listener_origin[0] - origin;
		float fDistance = sqrt(diff[0] * diff[0] + diff[1] * diff[1] + diff[2] * diff[2]);

		const float fMinDistance = snd_hrtf_lerp_min_distance.GetFloat();
		const float fMaxDistance = snd_hrtf_lerp_max_distance.GetFloat();
		if (fDistance < fMinDistance)
		{
			ch->hrtf.lerp = 0.0f;
		}
		else if (fDistance > fMaxDistance)
		{
			ch->hrtf.lerp = 1.0f; //snd_hrtf_ratio.GetFloat();
		}
		else
		{
			ch->hrtf.lerp = 1.0f; //snd_hrtf_ratio.GetFloat() * (fDistance - fMinDistance) / (fMaxDistance - fMinDistance);
		}
	}

	// process via operators only
	if( ch->m_pStackList && ch->m_pStackList->HasStack( CSosOperatorStack::SOS_UPDATE ) )
	{
		SND_ExecuteUpdateOperators( ch );
		return;
	}

	// This will be -1 if it's a sound that's merged at the channel volume level across the players

    vec_t dist;
    Vector source_vec[ MAX_SPLITSCREEN_CLIENTS ];
	Vector source_vec_DL;
	Vector source_vec_DR;
	Vector source_doppler_left;
	Vector source_doppler_right;
	int dopplerSlot = -1;
	bool fdopplerwav = false;
	float gain;
	float scale = 1.0;
	bool fplayersound = false;
	bool fmusicsound = false;
	float mono = 0.0;
	bool bAttenuated = true;
	bool bOkayToTrace = false;

	ch->dspface = 1.0;				// default facing direction: always facing player
	ch->dspmix = 0;					// default mix 0% dsp_room fx
	ch->distmix = 0;				// default 100% left (near) wav

#if !defined( _X360 )
	if ( ch->sfx && 
		ch->sfx->pSource && 
		ch->sfx->pSource->GetType() == CAudioSource::AUDIO_SOURCE_VOICE )
	{
		Voice_Spatialize( ch );
	}
#endif

	// For Splitscreen this is the average position, a total hack!!!
	Vector blended_listener_origin( 0, 0, 0 );
	int count = 0;
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( nSlot )
	{
		blended_listener_origin += listener_origin[ nSlot ];
		++count;
	}
	if ( count > 1 )
	{
		blended_listener_origin /= (float)count;
	}

	if ( ch->wavtype == CHAR_RADIO || ( IsSoundSourceViewEntity( ch->soundsource ) && !toolframework->InToolMode() ) || ( ch->sfx && ch->sfx->pSource && ch->sfx->pSource->GetType() == CAudioSource::AUDIO_SOURCE_VOICE))
	{
		// sounds coming from listener actually come from a short distance directly in front of listener
		// in tool mode however, the view entity is meaningless, since we're viewing from arbitrary locations in space
		fplayersound = true;
	}

	// music has separate mix properties, detect it
	if ( ch->sfx && ch->sfx->m_bIsMusic )
	{
		fmusicsound = true;
		fplayersound = false;

	}

	// map gain through global mixer by soundtype
	// stores into channel for use later
	int last_mixgroupid;
	mixervalues_t mixValues;
	MXR_GetVolFromMixGroup( ch, &mixValues, &last_mixgroupid );

	// apply mixer levels to channel to carry through operations
	// restored at the end of the function
	float saveChannelDistMult = ch->dist_mult;
	float soundlevel = (float)DIST_MULT_TO_SNDLVL(ch->dist_mult);
	soundlevel *= mixValues.level;
	ch->dist_mult = SNDLVL_TO_DIST_MULT((int)soundlevel);

	// update channel's position in case ent that made the sound is moving.
	QAngle source_angles;
	source_angles.Init(0.0, 0.0, 0.0);
	Vector vEntOrigin = ch->origin;
	
	bool looping = false;

	CAudioSource *pSource = ch->sfx ? ch->sfx->pSource : NULL;
	if ( pSource )
	{
		looping = pSource->IsLooped();
	}


	
	SpatializationInfo_t si;
	char nameBuf[MAX_PATH];
	si.info.Set( 
		ch->soundsource,
		ch->entchannel,
		ch->sfx ? ch->sfx->getname(nameBuf,sizeof(nameBuf)) : "",
		ch->origin,
		ch->direction,
		ch->master_vol,
		DIST_MULT_TO_SNDLVL( ch->dist_mult ),
		looping,
		ch->pitch,
		blended_listener_origin, // HACK FOR SPLITSCREEN, only client\c_func_tracktrain.cpp(100):		CalcClosestPointOnLine( info.info.vListenerOrigin, vecStart, vecEnd, *info.pOrigin, &t );  every looked at listener origin in this structure...
		ch->speakerentity,
		0 ); // unspecified index is fine

	// csgo
	bool bIsMenuMusic = false;
	if( ch->sfx->m_bIsMusic && V_stristr( nameBuf, "mainmenu" ) )
	{
		bIsMenuMusic = true;
	}

	si.type = SpatializationInfo_t::SI_INSPATIALIZATION;
	si.pOrigin = &vEntOrigin;
	si.pAngles = &source_angles;
	si.pflRadius = NULL;
	if ( ch->soundsource != 0 && ch->radius == 0 )
	{
		si.pflRadius = &ch->radius;
	}

	CUtlVector< Vector > utlVecMultiOrigins;
	si.m_pUtlVecMultiOrigins = &utlVecMultiOrigins;
	si.m_pUtlVecMultiAngles = NULL;

	{
		VPROF_("SoundServices->GetSoundSpatializtion", 2, VPROF_BUDGETGROUP_OTHER_SOUND, false, BUDGETFLAG_OTHER );
		g_pSoundServices->GetSoundSpatialization( ch->soundsource, si );	
	}

	if ( ch->flags.bUpdatePositions )
	{
		AngleVectors( source_angles, &ch->direction );
		ch->origin = vEntOrigin;
	}
	else
	{
		VectorAngles( ch->direction, source_angles );
	}

	if ( IsPC() && ch->userdata != 0 )
	{
		g_pSoundServices->GetToolSpatialization( ch->userdata, ch->guid, si );
		if ( ch->flags.bUpdatePositions )
		{
			AngleVectors( source_angles, &ch->direction );
			ch->origin = vEntOrigin;
		}
	}
	
	fdopplerwav = ((ch->wavtype == CHAR_DOPPLER) && !fplayersound);
	if ( fdopplerwav )
	{
		VPROF_( "SND_Spatialize doppler", 2, VPROF_BUDGETGROUP_OTHER_SOUND, false, BUDGETFLAG_OTHER );
			
		// along sound source forward direction (doppler wavs)
		// calculate point of closest approach for CHAR_DOPPLER wavs, replace source_vec

		float bestDist = FLT_MAX;
		Vector nearestPoint;
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( nSlot )
		{
			Vector nearPoint = ch->origin;		// default nearest sound approach point
			if ( SND_GetClosestPoint( ch, listener_origin[ nSlot ], source_angles, nearPoint ) )
			{
				float dist = (nearPoint - listener_origin[nSlot]).Length();
				if ( dist < bestDist )
				{
					dopplerSlot = nSlot;
					bestDist = dist;
					nearestPoint = nearPoint;
				}
			}
		}

		// if doppler sound was 'shot' away from all listeners, don't play it
		if ( dopplerSlot < 0 )
		{
			goto ClearAllVolumes;
		}

		// find location of doppler left & doppler right points

		SND_GetDopplerPoints( ch, listener_origin[ dopplerSlot ], source_angles, nearestPoint, source_doppler_left, source_doppler_right);

		// source_vec_DL is vector from listener to doppler left point
		// source_vec_DR is vector from listener to doppler right point

		VectorSubtract(source_doppler_left, listener_origin[ dopplerSlot ], source_vec_DL );
		VectorSubtract(source_doppler_right, listener_origin[ dopplerSlot ], source_vec_DR );

		// normalized vectors to left and right doppler locations
		dist = VectorNormalize( source_vec_DL );
		VectorNormalize( source_vec_DR );

		// don't play doppler if out of range
		// unless recording in the tool, since we may play back in range
		if ( dist > DOPPLER_RANGE_MAX && !toolframework->IsToolRecording() )
			goto ClearAllVolumes;
	}
	else
	{
		// source_vec is vector from listener to sound source
		dist = FLT_MAX;

		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			if ( fplayersound )
			{
				// Hack for now
				// get 2d forward direction vector, ignoring pitch angle
				Vector listener_forward2d;

				ConvertListenerVectorTo2D( &listener_forward2d, &listener_right[ hh ] );

				// player sounds originate from 1' in front of player, 2d
				VectorMultiply(listener_forward2d, 12.0, source_vec[ hh ] );
			}
			else
			{
				VectorSubtract(ch->origin, listener_origin[ hh ], source_vec[ hh ]);
			}

			// normalize source_vec and get distance from listener to source
			float checkDist = VectorNormalize( source_vec[ hh ] );
			if ( checkDist < dist )
			{
				dist = checkDist;
			}
		}
	}
	
	// calculate dsp mix based on distance to listener & sound level (linear approximation)
	// ... and sound mixer contribution
	if (ch->wavtype != CHAR_DIRSTEREO)
	{
		ch->dspmix = mixValues.dsp * SND_GetDspMix( ch, dist );
	}

	// calculate sound source facing direction for CHAR_DIRECTIONAL wavs
	if ( !fplayersound )
	{
		ch->dspface = SND_GetFacingDirection( ch, blended_listener_origin, source_angles );
		
		// calculate mixing parameter for CHAR_DISTVAR wavs
		ch->distmix = SND_GetDistanceMix( ch, dist );
	}

	// for sounds with a radius, spatialize left/right/front/rear evenly within the radius
	if ( ch->radius > 0 && dist < ch->radius && !fdopplerwav )
	{
		float interval = ch->radius * 0.5;
		mono = dist - interval;
		if ( mono < 0.0 )
			mono = 0.0;
		mono /= interval;
		// mono is 0.0 -> 1.0 from radius 100% to radius 50%
		mono = 1.0 - mono;
	}

	// don't pan sounds with no attenuation
	if ( ch->dist_mult <= 0 && !fdopplerwav && !( ch->wavtype == CHAR_DIRSTEREO))
	{
		// sound is centered left/right/front/back
		mono = 1.0;
		bAttenuated = false;
	}

	if ( ch->wavtype == CHAR_OMNI )
	{
		// omni directional sound sources are mono mix, all speakers
		// ie: they only attenuate by distance, not by source direction.
		mono = 1.0;
		bAttenuated = false;
	}

	// calculate gain based on distance, atmospheric attenuation, interposed objects
	// perform compression as gain approaches 1.0

	bOkayToTrace = SND_ChannelOkToTrace( ch );

	// TODO: get mixer values before this and eliminate volume effect dist fall off
	gain = 0.0f;
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		// In theory, due to obscured object traces, the two SS views might have different gains...  generally doesn't occur, but I did catch it in the debugger a few times
		float usegain = SND_GetGain( hh, &ch->gain[ hh ], ch, listener_origin[ hh ], fplayersound, fmusicsound, looping, dist, bAttenuated, bOkayToTrace );
		if ( usegain > gain )
		{
			gain = usegain;
		}
	}
	if( bIsMenuMusic )
		gain = gain * snd_menumusic_volume.GetFloat();


#if !defined( _X360 )
	if ( ch->sfx && 
		ch->sfx->pSource && 
		ch->sfx->pSource->GetType() == CAudioSource::AUDIO_SOURCE_VOICE )
	{
		gain = MAX(gain, voice_minimum_gain.GetFloat());
	}
#endif

	// map gain through global mixer by soundtype
//	int last_mixgroupid;
//	gain *= MXR_GetVolFromMixGroup( ch, &last_mixgroupid );
	gain *= mixValues.volume;

	// if playing a word, get volume scale of word - scale gain		
	scale = VOX_GetChanVol(ch);

	gain *= scale;

	// save spatialized volume and mixgroupid for display later
	ch->last_mixgroupid = last_mixgroupid;

	if ( fdopplerwav )
	{
		// we've already picked the best doppler listener in the code above, so only spaitilize for that player
		// don't merge volumes because there is only one set of volumes here.

		VPROF_("SND_Spatialize doppler", 2, VPROF_BUDGETGROUP_OTHER_SOUND, false, BUDGETFLAG_OTHER );
		// fill out channel volumes for both doppler sound source locations
		float volumes[CCHANVOLUMES/2];

		Device_SpatializeChannel( dopplerSlot, volumes, ch->master_vol, source_vec_DL, gain, mono, int(ch->wavtype) );
		// load volumes into channel as crossfade targets
		ChannelSetVolTargets( ch, volumes, IFRONT_LEFT, CCHANVOLUMES/2 );

		// right doppler location
		Device_SpatializeChannel( dopplerSlot, volumes, ch->master_vol, source_vec_DR, gain, mono, int(ch->wavtype) );
		// load volumes into channel as crossfade targets
		ChannelSetVolTargets( ch, volumes, IFRONT_LEFTD, CCHANVOLUMES/2 );
	}
	else
	{
		VPROF( "SND_Spatialize no-doppler" );

		// fill out channel volumes for single sound source location
		float volumes[CCHANVOLUMES/2];
		{
			float build_volumes[ MAX_SPLITSCREEN_CLIENTS ][CCHANVOLUMES/2] = { 0 };
			FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
			{
				Device_SpatializeChannel( hh, build_volumes[ hh ], ch->master_vol, source_vec[ hh ], gain, mono, int(ch->wavtype) );
			}
			SND_MergeVolumes( build_volumes, volumes );
		}
		
		// Special case for stereo sounds originating from player in surround mode
		// and special case for music: remap volumes directly to channels.
		RemapPlayerOrMusicVols( ch, volumes, fplayersound, fmusicsound, mono );

		// in dirstereo we perform a 'reflection' for the other channel to fill space
		if ( ch->wavtype == CHAR_DIRSTEREO )
		{
			float volumeOpposite[CCHANVOLUMES/2];
			// currently center is unused
			volumeOpposite[IFRONT_CENTER] = 0;
			if ( g_AudioDevice->IsSurround() )
			{
				volumeOpposite[IFRONT_LEFT] = volumes[IREAR_RIGHT];
				volumeOpposite[IFRONT_RIGHT] = volumes[IREAR_LEFT];
				volumeOpposite[IREAR_LEFT] = volumes[IFRONT_RIGHT];
				volumeOpposite[IREAR_RIGHT] = volumes[IFRONT_LEFT];
			}
			else
			{
				volumeOpposite[IFRONT_LEFT] = volumes[IFRONT_RIGHT];
				volumeOpposite[IFRONT_RIGHT] = volumes[IFRONT_LEFT];
				volumeOpposite[IREAR_LEFT] = 0;
				volumeOpposite[IREAR_RIGHT] = 0;
			}

			// clamp to 3 to fool the volume clippers so we don't skip mixing partial channels (lower level mix code doesn't distinguish this yet)
			for ( int i = 0; i < IFRONT_LEFTD; i++ )
			{
				int nMax = MAX(volumes[i], volumeOpposite[i]);
				if ( nMax )
				{
					volumes[i] = MAX(volumes[i],3);
					volumeOpposite[i] = MAX(volumeOpposite[i],3);
				}
			}
			ChannelSetVolTargets( ch, volumeOpposite, IFRONT_LEFTD, CCHANVOLUMES/2 );
		}

		// load volumes into channel as crossfade volume targets
		ChannelSetVolTargets( ch, volumes, IFRONT_LEFT, CCHANVOLUMES/2 );
	}


	// prevent left/right/front/rear/center volumes from changing too quickly & producing pops
	ChannelUpdateVolXfade( ch );

	// end of first time spatializing sound
	if ( SND_IsInGame() || toolframework->InToolMode() )
	{
		ch->flags.bfirstpass = false;
	}

	// calculate total volume solely for display and ducking later
	ch->last_vol = gain * (ch->master_vol/255.0);

	// restore dist_mult
	ch->dist_mult = saveChannelDistMult;

	return;

ClearAllVolumes:

	// Clear all volumes and return. 
	// This shuts the sound off permanently.
	ChannelClearVolumes( ch );

	// end of first time spatializing sound
	ch->flags.bfirstpass = false;

	// restore dist_mult
	ch->dist_mult = saveChannelDistMult;
}           

ConVar snd_defer_trace("snd_defer_trace","1");
void SND_SpatializeFirstFrameNoTrace( channel_t *pChannel)
{
	// Don't do this in tools mode since if we are scrubbing time, all of the sounds will come it at a low volume
	if ( snd_defer_trace.GetBool() && 
		!toolframework->InToolMode() )
	{
		// set up tracing state to be non-obstructed
		pChannel->flags.bfirstpass = false;
		pChannel->flags.bTraced = true;
		for ( int i = 0; i < MAX_SPLITSCREEN_CLIENTS; ++i )
		{
			pChannel->gain[ i ].ob_gain = 1.0;
			pChannel->gain[ i ].ob_gain_inc = 1.0;
			pChannel->gain[ i ].ob_gain_target = 1.0;
		}
		// now spatialize without tracing
		SND_Spatialize(pChannel);
		// now reset tracing state to firstpass so the trace gets done on next spatialize
		for ( int i = 0; i < MAX_SPLITSCREEN_CLIENTS; ++i )
		{
			pChannel->gain[ i ].ob_gain = 0.0;
			pChannel->gain[ i ].ob_gain_inc = 0.0;
			pChannel->gain[ i ].ob_gain_target = 0.0;
		}
		pChannel->flags.bfirstpass = true;
		pChannel->flags.bTraced = false;
	}
	else
	{
		for ( int i = 0; i < MAX_SPLITSCREEN_CLIENTS; ++i )
		{
			pChannel->gain[ i ].ob_gain = 0.0;
			pChannel->gain[ i ].ob_gain_inc = 0.0;
			pChannel->gain[ i ].ob_gain_target = 0.0;
		}
		pChannel->flags.bfirstpass = true;
		pChannel->flags.bTraced = false;
		SND_Spatialize(pChannel);
	}
}

void PrintSoundFileName( const char *pText1, CSfxTable *pSfx, const char * pText2 = NULL )
{
	char nameBuf[MAX_PATH];
	char const *pfn = "(Unknown)";
	if ( pSfx != NULL )
	{
		pfn = pSfx->GetFileName( nameBuf, sizeof(nameBuf) );
		if ( pfn == NULL )
		{
			pfn = "(null)";
		}
	}

	if ( pText2 == NULL )
	{
		pText2 = "";
	}
	Warning( "[Sound] %s(\"%s\") called. %s\n", pText1, pfn, pText2 );
}

void PrintSoundFileName( const char *pText1, const char *pFileName, CSfxTable *pSfx, const char * pText2 = NULL )
{
	if ( pText2 == NULL )
	{
		pText2 = "";
	}
	Warning( "[Sound] %s(\"%s\") called. %s\n", pText1, pFileName, pText2 );
}

void PrintChannel( const char *pText1, const char *pFileName, channel_t * pChannel, const char *pText2 = NULL )
{
	int nIndex = pChannel - &channels[ 0 ];
	Assert( ( nIndex >= 0 ) && ( nIndex < MAX_CHANNELS ) );

	Msg( "Channel - Index: %d - Guid: %d.\n", nIndex, pChannel->guid );
	PrintSoundFileName( pText1, pFileName, pChannel->sfx, pText2 );
}

void PrintChannel( const char *pText1, channel_t * pChannel, const char *pText2 = NULL )
{
	int nIndex = pChannel - &channels[ 0 ];
	Assert( ( nIndex >= 0 ) && ( nIndex < MAX_CHANNELS ) );

	Msg( "Channel - Index: %d - Guid: %d.\n", nIndex, pChannel->guid );
	PrintSoundFileName( pText1, pChannel->sfx, pText2 );
}

void PrintChannelInfo( channel_t * pChannel )
{
	int nIndex = pChannel - &channels[ 0 ];
	Assert( ( nIndex >= 0 ) && ( nIndex < MAX_CHANNELS ) );

	unsigned int sampleCount = RemainingSamples( pChannel );
	float timeleft = (float)sampleCount / (float)pChannel->sfx->pSource->SampleRate();
	bool bLooping = pChannel->sfx->pSource->IsLooped();

	char nameBuf[MAX_PATH];
	Msg( "index(%03d) guid(% 4d) l(% 3d) c(% 3d) r(% 3d) rl(% 3d) rr(% 3d) vol(% 3d) ent(% 3d) pos(% 6.2f % 6.2f % 6.2f) timeleft(% 2.2f) pitch(% 2.2f) looped(%d) %s\n", 
		nIndex,
		pChannel->guid,
		(int)pChannel->fvolume[IFRONT_LEFT],
		(int)pChannel->fvolume[IFRONT_CENTER],
		(int)pChannel->fvolume[IFRONT_RIGHT],
		(int)pChannel->fvolume[IREAR_LEFT],
		(int)pChannel->fvolume[IREAR_RIGHT],
		pChannel->master_vol,
		pChannel->soundsource,
		pChannel->origin[0],
		pChannel->origin[1],
		pChannel->origin[2],
		timeleft,
		pChannel->pitch,
		bLooping,
		pChannel->sfx->getname(nameBuf, sizeof(nameBuf)));
}

// Stops a channel.
// Returns true if the stop is delayed, false if it has been applied within the function.
enum StopChannelResult
{
	SCR_Done,
	SCR_Delayed,
	//SCR_Failed,		// Not used for the moment
};
StopChannelResult S_StopChannelUnlocked( channel_t *pChannel )
{
	if ( snd_report_stop_sound.GetBool() )
	{
		PrintSoundFileName( "S_StopChannelUnlocked", pChannel->sfx, "Stopping sound." );
	}

	if( pChannel->m_pStackList )
	{
		pChannel->m_pStackList->StopStacks( SOS_STOP_NORM );
		return SCR_Delayed;
	}
	else
	{
		S_FreeChannel( pChannel );
		return SCR_Done;
	}
}

void S_StopChannel( channel_t *pChannel )
{
	THREAD_LOCK_SOUND();
	S_StopChannelUnlocked( pChannel );
}

// search through all channels for a channel that matches this
// soundsource, entchannel and sfx, and perform alteration on channel
// as indicated by 'flags' parameter. If shut down request and
// sfx contains a sentence name, shut off the sentence.
// returns TRUE if sound was altered,
// returns FALSE if sound was not found (sound is not playing)

int S_AlterChannel( StartSoundParams_t &pParams )
{
	THREAD_LOCK_SOUND();

	int soundsource = pParams.soundsource;
	int entchannel = pParams.entchannel;
	CSfxTable *sfx = pParams.pSfx;
	int pitch = pParams.pitch;
	int flags = pParams.flags;

	int vol = clamp( (int)( pParams.fvol * 255.0f ), 0, 255 );

	int ch_idx;
	char nameBuf[MAX_PATH];

	if ( TestSoundChar(sfx->getname(nameBuf, sizeof(nameBuf)), CHAR_SENTENCE) )
	{
		// This is a sentence name.
		// For sentences: assume that the entity is only playing one sentence
		// at a time, so we can just shut off
		// any channel that has ch->isentence >= 0 and matches the
		// soundsource.

		CChannelList list;
		g_ActiveChannels.GetActiveChannels( list );
		for ( int i = 0; i < list.Count(); i++ )
		{
			ch_idx = list.GetChannelIndex(i);
			if (channels[ch_idx].soundsource == soundsource
				&& channels[ch_idx].entchannel == entchannel
				&& channels[ch_idx].sfx != NULL )
			{
				if (flags & SND_CHANGE_PITCH)
				{
					channels[ch_idx].basePitch = pitch;
				}
				if (flags & SND_CHANGE_VOL)
				{
					channels[ch_idx].master_vol = vol;
				}
				if ( flags & SND_STOP )
				{
					S_StopChannelUnlocked( &channels[ch_idx] );
				}
			
				return TRUE;
			}
		}
		// channel not found
		if ( snd_report_verbose_error.GetBool() )
		{
			Msg( "%s(%d): Channel not found for sound '%s'.\n", __FILE__, __LINE__, nameBuf );
		}
		return FALSE;

	}

	// regular sound or streaming sound
	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );

	bool bSuccess = false;

	//
	// Because operators can now "fade out" ie: stopping not stopped" we can have the same
	// sound entry on the same entity in the state of "stopping" so we must dismiss these.
	// we will stop the channel that matches and has elapsed the most time
	// THIS IS TEMPORARY AND SHOULD BE MOVED TO AN OPERATOR DEFINABLE ACTION AND/OR ALWAYS BE GUID BASED
	//
	// Volume and pitch are similarly ignoring "stopping" channels as there are currently
	// no systems in place that "stop" a channel then change it's volume or pitch (through traditional methods)
	// This definitely needs to be addressed.
	///

	// separate path for script handles
	// NOTE: SND_IGNORE_NAME uses old system regardless
	if( pParams.m_bIsScriptHandle && ( flags & SND_IGNORE_NAME ) == 0 )
	{
		float flMaxElapsed = -1.0;
		int nMaxElapseedIndex = -1;

		// find oldest matching, non-stopping entry
		for ( int i = 0; i < list.Count(); i++ )
		{
			ch_idx = list.GetChannelIndex(i);

			// current matching criteria
			if( channels[ch_idx].soundsource == soundsource && 
				channels[ch_idx].entchannel == entchannel &&
				channels[ch_idx].m_nSoundScriptHash == pParams.m_nSoundScriptHash )
			{
				if( ( &channels[ch_idx] )->m_pStackList &&
					!( ( ( &channels[ch_idx] )->m_pStackList)->IsStopping() ) )
				{
					// acquire max elapsed, not-stopping channel
					float flElapsed = S_GetElapsedTimeByGuid( channels[ch_idx].guid );
					if( flElapsed > flMaxElapsed )
					{
						flMaxElapsed = flElapsed;
						nMaxElapseedIndex = ch_idx;
					}
				}
			}
		}
		// stopping oldest matching entry 
		if( nMaxElapseedIndex > -1 )
		{
			channel_t *pChannel = &channels[ nMaxElapseedIndex ];

			if ( flags & SND_STOP )
			{
				S_StopChannelUnlocked( pChannel );
			}
			if ( flags & SND_CHANGE_PITCH )
			{
				pChannel->basePitch = pitch;
			}
			if ( flags & SND_CHANGE_VOL )
			{
				pChannel->master_vol = vol;
			}
			return true;
		}
	} 
	else // almost the same as the original system
	{
		for ( int i = 0; i < list.Count(); i++ )
		{
			ch_idx = list.GetChannelIndex(i);

			if ( (channels[ch_idx].soundsource == soundsource || (channels[ch_idx].flags.m_bInEyeSound && pParams.m_bInEyeSound)) && 
				 ( ( flags & SND_IGNORE_NAME ) || 
				   (channels[ch_idx].entchannel == entchannel && channels[ch_idx].sfx == sfx )))
			{

				if ( flags & SND_CHANGE_PITCH )
				{
					channels[ch_idx].basePitch = pitch;
				}
				if ( flags & SND_CHANGE_VOL )
				{
					channels[ch_idx].master_vol = vol;
				}
				if ( flags & SND_STOP )
				{
					S_StopChannelUnlocked( &channels[ch_idx] );
				}
				if ( ( flags & SND_IGNORE_NAME ) == 0 )
					return TRUE;
				else
					bSuccess = true;
			}
		}
	}
	return ( bSuccess ) ? ( TRUE ) : ( FALSE );
}

int S_AlterChannelByGuid( StartSoundParams_t &pParams )
{
	THREAD_LOCK_SOUND();

	int guid = pParams.m_nQueuedGUID;
	int pitch = pParams.pitch;
	int flags = pParams.flags;
	int vol = clamp( (int)( pParams.fvol * 255.0f ), 0, 255 );


	channel_t *pChannel = S_FindChannelByGuid(guid);
	if ( pChannel )
	{
		if (flags & SND_CHANGE_PITCH)
		{
			pChannel->basePitch = pitch;
		}
		if (flags & SND_CHANGE_VOL)
		{
			pChannel->master_vol = vol;
		}
		if (flags & SND_STOP)
		{
			S_StopChannelUnlocked( pChannel );
		}
		return true;
	}

	if ( snd_report_verbose_error.GetBool() )
	{
		Msg( "%s(%d): Channel not found for sound guid '%d'.\n", __FILE__, __LINE__, guid );
	}
	return false;
}

static void S_IsDopplerWave( char const *pchSoundName, int soundsource, bool &bPlayerSound, bool &bDopplerWave )
{
	bDopplerWave = false;
	bPlayerSound = ( IsSoundSourceViewEntity( soundsource ) && !toolframework->InToolMode() );
	if ( !bPlayerSound )
	{
		return;
	}
	bDopplerWave = TestSoundChar( pchSoundName, CHAR_DOPPLER );
}

// set channel flags during initialization based on 
// source name

void S_SetChannelWavtype( channel_t *target_chan, const char *pSndName )
{	
	// if 1st or 2nd character of name is CHAR_DRYMIX, sound should be mixed dry with no dsp (ie: music)
	target_chan->flags.bdry = TestSoundChar( pSndName, CHAR_DRYMIX );
	target_chan->flags.bfast_pitch = TestSoundChar( pSndName, CHAR_FAST_PITCH );

	// get sound spatialization encoding	
	target_chan->wavtype = 0;

	if ( TestSoundChar( pSndName, CHAR_DOPPLER ) )
		target_chan->wavtype = CHAR_DOPPLER;
	
	if ( TestSoundChar( pSndName, CHAR_DIRECTIONAL ) )
		target_chan->wavtype = CHAR_DIRECTIONAL;

	if ( TestSoundChar( pSndName, CHAR_DISTVARIANT ) )
		target_chan->wavtype = CHAR_DISTVARIANT;

	if ( TestSoundChar( pSndName, CHAR_OMNI ) )
		target_chan->wavtype = CHAR_OMNI;

	if ( TestSoundChar( pSndName, CHAR_SPATIALSTEREO ) )
		target_chan->wavtype = CHAR_SPATIALSTEREO;

	if ( TestSoundChar( pSndName, CHAR_DIRSTEREO ) )
		target_chan->wavtype = CHAR_DIRSTEREO;

	if (snd_use_hrtf.GetBool() && TestSoundChar(pSndName, CHAR_HRTF) && (!IsSoundSourceViewEntity(target_chan->soundsource) || target_chan->hrtf.debug_lock_position))
	{
		target_chan->wavtype = CHAR_HRTF;
		target_chan->hrtf.lerp = 1.0; //snd_hrtf_ratio.GetFloat();
	}

	if ( TestSoundChar( pSndName, CHAR_RADIO ) )
		target_chan->wavtype = CHAR_RADIO;
}

// Sets bstereowav flag in channel if source is true stereo wav
// sets default wavtype for stereo wavs to CHAR_DISTVARIANT - 
// ie: sound varies with distance (left is close, right is far)
// Must be called after S_SetChannelWavtype

void S_SetChannelStereo( channel_t *target_chan, CAudioSource *pSource )
{
	if ( !pSource )
	{
		target_chan->flags.bstereowav = false;
		return;
	}
	
	// returns true only if source data is a stereo wav file. 
	// ie: mp3, voice, sentence are all excluded.

	target_chan->flags.bstereowav = pSource->IsStereoWav();

	// Default stereo wavtype:

	// just player standard stereo wavs on player entity - no override.

	if ( IsSoundSourceViewEntity( target_chan->soundsource ) )
		return;
	
	// default wavtype for stereo wavs is OMNI - except for drymix or sounds with 0 attenuation

	if ( target_chan->flags.bstereowav && !target_chan->wavtype && !target_chan->flags.bdry && target_chan->dist_mult )
		// target_chan->wavtype = CHAR_DISTVARIANT;
		target_chan->wavtype = CHAR_OMNI;
}

// =======================================================================
// Channel volume management routines:

// channel volumes crossfade between values over time
// to prevent pops due to rapid spatialization changes
// =======================================================================

// return true if all volumes and target volumes for channel are less/equal to 'vol'

bool BChannelLowVolume( channel_t *pch, float vol_min )
{
	float max = -1;
	float max_target = -1;
	float vol;
	float vol_target;

	for (int i = 0; i < CCHANVOLUMES; i++)
	{
		vol = pch->fvolume[i];
		vol_target = pch->fvolume_target[i];

		if (vol > max)
			max = vol;

		if (vol_target > max_target)
			max_target = vol_target;
	}
		
	return (max <= vol_min && max_target <= vol_min);
}

// Get the loudest actual volume for a channel (not counting targets).
float ChannelLoudestCurVolume( const channel_t * RESTRICT pch )
{
	float loudest = pch->fvolume[0];
	for (int i = 1; i < CCHANVOLUMES; i++)
	{
		loudest = fpmax(loudest, pch->fvolume[i]);
	}
	return loudest;
}

// clear all volumes, targets, crossfade increments

void ChannelClearVolumes( channel_t *pch )
{
	for (int i = 0; i < CCHANVOLUMES; i++)
	{
		pch->fvolume[i] = 0.0;
		pch->fvolume_target[i] = 0.0;
		pch->fvolume_inc[i] = 0.0;
	}
}

// return current volume as integer

int ChannelGetVol( channel_t *pch, int ivol )
{
	Assert(ivol < CCHANVOLUMES);
	return (int)(pch->fvolume[ivol]);	
}

// return maximum current output volume 

int ChannelGetMaxVol( channel_t *pch )
{
	float max = 0.0;
	
	for (int i = 0; i < CCHANVOLUMES; i++)
	{
		if (pch->fvolume[i] > max)
			max = pch->fvolume[i];
	}

	return (int)max;
}

// set current volume (clears crossfading - instantaneous value change)

void ChannelSetVol( channel_t *pch, int ivol, int vol )
{
	Assert(ivol < CCHANVOLUMES);
	
	pch->fvolume[ivol] = (float)(iclamp(vol, 0, 255));	

	pch->fvolume_target[ivol] = pch->fvolume[ivol];
	pch->fvolume_inc[ivol] = 0.0;
}

// copy current channel volumes into target array, starting at ivol, copying cvol entries

void ChannelCopyVolumes( channel_t *pch, float *pvolume_dest, int ivol_start, int cvol )
{
	Assert (ivol_start < CCHANVOLUMES);
	Assert (ivol_start + cvol <= CCHANVOLUMES);

	if ( ( ivol_start == 0 ) && ( cvol == CCHANVOLUMES ) )
	{
		// This is the path executed in most cases
		// Unroll by hand so the code can be optimized to reduce LHS a bit (due to float to int conversion)
		// I.e. if the compiler does a proper job, we will only pay for the first LHS
		pvolume_dest[0] = pch->fvolume[0];
		pvolume_dest[1] = pch->fvolume[1];
		pvolume_dest[2] = pch->fvolume[2];
		pvolume_dest[3] = pch->fvolume[3];
		pvolume_dest[4] = pch->fvolume[4];
		pvolume_dest[5] = pch->fvolume[5];
		pvolume_dest[6] = pch->fvolume[6];
		pvolume_dest[7] = pch->fvolume[7];
		pvolume_dest[8] = pch->fvolume[8];
		pvolume_dest[9] = pch->fvolume[9];
		pvolume_dest[10] = pch->fvolume[10];
		pvolume_dest[11] = pch->fvolume[11];
	}
	else
	{
		for (int i = 0; i < cvol; i++)
			pvolume_dest[i] = pch->fvolume[i + ivol_start];
	}
}

// volume has hit target, shut off crossfading increment

inline void ChannelStopVolXfade( channel_t *pch, int ivol )
{
	pch->fvolume[ivol] = pch->fvolume_target[ivol];
	pch->fvolume_inc[ivol] = 0.0;
}

// Once the correct parameters are determined, we can bake them in if we want (and if there is a noticeable performance overhead)
#if 0
#define	VOL_XFADE_TIME	0.070	
#define VOL_INCR_MAX	20.0
#define VOL_NO_XFADE	5.0
#else
ConVar snd_vol_xfade_time( "snd_vol_xfade_time", "0.070", 0, "Channel volume cross-fade time in seconds." );
ConVar snd_vol_xfade_incr_max( "snd_vol_xfade_incr_max", "20.0", 0, "Never change volume by more than +/-N units per frame during cross-fade." );
ConVar snd_vol_no_xfade( "snd_vol_no_xfade", "5.0", 0, "If current and target volumes are close, don't cross-fade." );
ConVar snd_vol_xfade_speed_multiplier_for_doppler( "snd_vol_xfade_speed_multiplier_for_doppler", "1", 0, "Doppler effect is extremely sensible to volume variation. To reduce the pops, the cross-fade has to be very slow." );

#define	VOL_XFADE_TIME	snd_vol_xfade_time.GetFloat()
#define VOL_INCR_MAX	snd_vol_xfade_incr_max.GetFloat()
#define VOL_NO_XFADE	snd_vol_no_xfade.GetFloat()
#endif

// set volume target and volume increment (for crossfade) for channel & speaker
void ChannelSetVolTarget( channel_t *pch, int ivol, float volume_target )
{
	float frametime = g_pSoundServices->GetHostFrametime();
	float speed;
	float vol_target = (float)(clamp(volume_target, 0, 255));
	float vol_current;

	Assert(ivol < CCHANVOLUMES);
	
	// set volume target
	pch->fvolume_target[ivol] = vol_target;

	// current volume
	vol_current = pch->fvolume[ivol];

	float fMultiplier = 1.0f;
	if ( ( pch->wavtype == CHAR_DIRSTEREO ) || ( pch->wavtype == CHAR_DOPPLER ) )
	{
		// CHAR_DIRSTEREO uses Doppler under the hood. Reduce the speed for these.
		fMultiplier = snd_vol_xfade_speed_multiplier_for_doppler.GetFloat();
	}

	// if first time spatializing, set target = volume with no crossfade
	// if current & target volumes are close - don't bother crossfading

	if ( pch->flags.bfirstpass || (fabs(vol_target - vol_current) < VOL_NO_XFADE * fMultiplier)) 
	{
		// set current volume = target, no increment

		ChannelStopVolXfade( pch, ivol);
		return;
	}

	// get crossfade increment 'speed' (volume change per frame)

	speed = ( frametime / VOL_XFADE_TIME ) * (vol_target - vol_current);

	// make sure we never increment by more than +/- VOL_INCR_MAX volume units per frame
	
	speed = clamp(speed, -VOL_INCR_MAX, VOL_INCR_MAX) * fMultiplier;

	pch->fvolume_inc[ivol] = speed;	
}


// set volume targets, using array pvolume as source volumes.
// set into channel volumes starting at ivol_offset index
// set cvol volumes

void ChannelSetVolTargets( channel_t *pch, float *pvolumes, int ivol_offset, int cvol )
{
	float volume_target;

	Assert(ivol_offset + cvol <= CCHANVOLUMES);

	for (int i = 0; i < cvol; i++)
	{
		volume_target = pvolumes[i];
		if (volume_target < 2.0f)
		{
			volume_target -= 2.0f - volume_target;
		}

		volume_target = clamp( volume_target, 0, 255 );

		ChannelSetVolTarget( pch, ivol_offset + i, volume_target );
	}
}


// Call once per frame, per channel:
// update all volume crossfades, from fvolume -> fvolume_target
// if current volume reaches target, set increment to 0

void ChannelUpdateVolXfade( channel_t *pch )
{
	float fincr;

	for (int i = 0; i < CCHANVOLUMES; i++)
	{
		fincr = pch->fvolume_inc[i];

		if (fincr != 0.0)
		{
			pch->fvolume[i] += fincr;

			// test for hit target

			if (fincr > 0.0)
			{
				if (pch->fvolume[i] >= pch->fvolume_target[i])
					ChannelStopVolXfade( pch, i );
			}
			else
			{
				if (pch->fvolume[i] <= pch->fvolume_target[i])
					ChannelStopVolXfade( pch, i );
			}
		}
	}
}

void DumpFilePaths(const char *filename)
{
	// Don't Write to internal storage on the 360
	if ( IsGameConsole() )
		return;

	// Generate a new .cfg file.
	char		szFileName[MAX_PATH];
	CUtlBuffer	configBuff( 0, 0, CUtlBuffer::TEXT_BUFFER);

	char computername[ 64 ];
	Q_memset( computername, 0, sizeof( computername ) );
#if defined ( _WIN32 )
	DWORD length = sizeof( computername ) - 1;
	if ( !GetComputerName( computername, &length ) )
	{
		Q_strncpy( computername, "???", sizeof( computername )  );
	}
#elif defined( _PS3 )
	Q_strncpy( computername, "PS3", sizeof( computername ) );
#else
	if ( gethostname( computername, sizeof(computername) ) == -1 )
	{
		Q_strncpy( computername, "Linux????", sizeof( computername ) );
	}
	computername[sizeof(computername)-1] = '\0';
#endif
	// todo: morasky, ugly, fix  this and make generic!
//	Q_snprintf( szFileName, sizeof(szFileName), "\\\\fileserver\\User\\portal2\\soundlogs\\%s_%s", computername, filename );
	Q_snprintf( szFileName, sizeof(szFileName), "%s\\%s_%s", snd_store_filepaths.GetString(), computername, filename );
//	g_pFileSystem->CreateDirHierarchy( "\\fileserver\\User\\portal2\\soundlogs\\", NULL );
	g_pFileSystem->CreateDirHierarchy( snd_store_filepaths.GetString(), NULL );
	if ( g_pFileSystem->FileExists( szFileName, NULL ) && !g_pFileSystem->IsFileWritable( szFileName, NULL ) )
	{
		ConMsg( "Soundlog file %s is read-only!!\n", szFileName );
		return;
	}

	for (int i = 0; i < g_StoreFilePaths.Count(); i++)
	{
		configBuff.Printf( "%s %i, ", g_StoreFilePaths.GetElementName( i ), g_StoreFilePaths[i] );
	}

	if ( !configBuff.TellMaxPut() )
	{
		// nothing to write
		return;
	}

	// make a persistent copy that async will use and free
	char *tempBlock = new char[configBuff.TellMaxPut()];
	Q_memcpy( tempBlock, configBuff.Base(), configBuff.TellMaxPut() );

	// async write the buffer, and then free it
	g_pFileSystem->AsyncWrite( szFileName, tempBlock, configBuff.TellMaxPut(), true );

	
	ConMsg( "snd_dump_filepaths: Wrote %s\n", szFileName );

}

void S_DumpFilePaths( const CCommand &args )
{
	if ( args.ArgC() != 2)
	{
		// if dsp_parms with no arguments, reload entire preset file
		DevMsg("Error: Filepath arg required\n");
		return;
	}

	const char *filename = args[1];
	Assert( filename && filename [ 0 ] );
	DumpFilePaths(filename);
}

static ConCommand dump_file_paths( "snd_dump_filepaths", S_DumpFilePaths );
static ConVar snd_filter( "snd_filter", "", FCVAR_CHEAT );

// This function is capable of starting both static and dynamic sounds
static int S_StartSound_Immediate( StartSoundParams_t& params )
{
	if ( !g_AudioDevice || !g_AudioDevice->IsActive() )
		return 0;

	// handle queued updates
	if ( ( params.m_nQueuedGUID > 0 ) && ( params.flags & ( SND_STOP | SND_CHANGE_VOL | SND_CHANGE_PITCH ) ) )
	{
		if ( S_AlterChannelByGuid( params ) )
			return 0;
	}

	if ( !params.pSfx )
	{
		if ( snd_report_verbose_error.GetBool() )
		{
			Msg( "%s(%d): params.pSfx is NULL.\n", __FILE__, __LINE__ );
		}
		return 0;
	}

	char sndname[ MAX_PATH ];
	params.pSfx->getname(sndname, sizeof(sndname));

	if ( g_bPreventSound )
	{
		// We must respect the prevention, this is likely the loading state where
		// the mixer cannot be allowed to operate.
		DevWarning( "Starting sound '%s' while system disabled.\n", sndname );
		return 0;
	}

	
#ifndef NO_TOOLFRAMEWORK
	if ( toolframework->InToolMode() )
	{	
		// If the active tool does not want game sounds to be played, return if the sound did not originate from a tool.
		if ( !toolframework->ShouldGamePlaySounds() && !params.bToolSound )
			return 0;
	}
#endif

	if ( snd_filter.GetString()[ 0 ] && !Q_stristr( sndname, snd_filter.GetString() ) )
	{
		return 0;
	}

	// storing file paths for complete list of sounds used in game
	if ( IsPC() && snd_store_filepaths.GetString()[ 0 ] )
	{
		if( CommandLine()->FindParm("-playtest") != 0 &&
			!( params.flags & ( SND_STOP | SND_CHANGE_VOL | SND_CHANGE_PITCH ) ) )
		{
			int i = g_StoreFilePaths.Find( sndname );
			if ( !g_StoreFilePaths.IsValidIndex( i ) )
			{
				g_StoreFilePaths.Insert( sndname, 1 );
			}
			else
			{
				g_StoreFilePaths[i] = g_StoreFilePaths[i] + 1; 
			}
		}
	}

#if defined( _X360 )
	if ( !engineClient->IsConnected() && g_pXboxInstaller->IsInstallEnabled() && !g_pXboxInstaller->IsFullyInstalled() )
	{
		// prevent ANY audio streaming during main menu while the install might go active or is occurring
		// static memory sounds are fine
		if ( params.pSfx->pSource && params.pSfx->pSource->IsStreaming() )
		{
			DevWarning( "Ignoring streaming sound '%s' while installer may become active.\n", sndname );
			return 0;
		}
	}
#endif

	// Override the entchannel to CHAN_STREAM if this is a non-voice stream sound.
	if ( !params.staticsound &&
		TestSoundChar( sndname, CHAR_STREAM ) && params.entchannel != CHAN_VOICE )
	{
		params.entchannel = CHAN_STREAM;
	}

	int vol = clamp( (int)( params.fvol * 255.0f ), 0, 255 );

	int nSndShowStart = snd_showstart.GetInt();
	if ( ( params.flags & SND_STOP ) && ( nSndShowStart > 0 ) )
	{
		DevMsg( "S_StartSound: %s Stopped.\n", sndname );
	}

	THREAD_LOCK_SOUND();

	if ( params.flags & ( SND_STOP | SND_CHANGE_VOL | SND_CHANGE_PITCH ) )
	{
		if ( S_AlterChannel( params ) || ( params.flags & SND_STOP ) )
			return 0;
	}

	if ( params.pitch == 0 )
	{
		DevMsg( "Warning: S_StartSound (%s) Ignored, called with pitch 0\n", sndname );
		return 0;
	}

	// First, make sure the sound source entity is even in the PVS.
	float flSoundRadius = 0.0f;	

	bool looping = false;

	SpatializationInfo_t si;
	si.info.Set( 
		params.soundsource,
		params.entchannel,
		params.pSfx ? sndname : "",
		params.origin,
		params.direction,
		vol,
		params.soundlevel,
		looping,
		params.pitch,
		listener_origin[ 0 ],
		params.speakerentity,
		0 );

	si.type = SpatializationInfo_t::SI_INCREATION;

	Vector vEntOrigin = params.origin;

	si.pOrigin = &vEntOrigin;
	si.pAngles = NULL;
	si.pflRadius = &flSoundRadius;

	CUtlVector< Vector > utlVecMultiOrigins;
	si.m_pUtlVecMultiOrigins = &utlVecMultiOrigins;
	si.m_pUtlVecMultiAngles = NULL;


	// Morasky: why it doesn't spatialize for dynamic? (because is could be thrown out immediatelly?)
	//          why it doesn't use an updated position for starting?
	channel_t *ch = NULL;
	if ( params.staticsound || ( params.m_bIsScriptHandle && !snd_sos_allow_dynamic_chantype.GetInt() ) )
	{
		g_pSoundServices->GetSoundSpatialization( params.soundsource, si );
		ch = SND_PickStaticChannel( params.soundsource, params.pSfx ); 
		if( !ch )
		{
			DevMsg("Error: Sound %s failed to allocate a static channel and will not play\n", sndname );
		}
	}
	else
	{
		// pick a channel to play on
		ch = SND_PickDynamicChannel( params.soundsource, params.entchannel, params.origin, params.pSfx );
// 		if( !ch )
// 		{
// 			DevMsg("Error: Sound %s failed to allocate a dynamic channel and will not play\n", sndname );
// 		}
	}

	if ( !ch )
	{
		if ( snd_report_verbose_error.GetBool() )
		{
			Msg( "%s(%d): Could not pick channel for sound '%s'.\n", __FILE__, __LINE__, sndname );
		}
		return 0;
	}

	bool bIsSentence = TestSoundChar( sndname, CHAR_SENTENCE );

	int nGUID = ( params.m_nQueuedGUID > 0 ) ? params.m_nQueuedGUID : SND_GetGUID();
	
	// clear all channel memory and set guid
	SND_ActivateChannel( ch, nGUID );
	ChannelClearVolumes( ch );

	if ( ( (*snd_find_channel.GetString()) != '\0' ) && ( Q_stristr( sndname, snd_find_channel.GetString() ) != 0 ) )
	{
		// This is a sound we are interested in. Display some useful information.
		PrintChannel( "FoundChannel", sndname, ch, "from ConVar snd_find_channel." );
	}

	// Default save/restore to disabled
	ch->flags.m_bShouldSaveRestore = false;

	ch->hrtf.follow_entity = params.m_bHRTFFollowEntity;
	ch->hrtf.bilinear_filtering = params.m_bHRTFBilinear;
	ch->hrtf.debug_lock_position = params.m_bHRTFLock;

	if (ch->hrtf.debug_lock_position)
	{
		ch->hrtf.vec = params.origin;
	}

	//-----------------------------------------------------------------------------
	// initialize operators for this channel and execute start stack if possible
	//-----------------------------------------------------------------------------
	
	CSosOperatorStackList *pStackList = NULL;
	if( params.m_bIsScriptHandle )
	{
		stack_data_t stackData;
		stackData.m_pOperatorsKV = params.m_pOperatorsKV;
		stackData.m_nSoundScriptHash = params.m_nSoundScriptHash;
		stackData.m_nGuid = ch->guid;
		stackData.m_flStartTime = g_pSoundServices->GetHostTime() - params.opStackElapsedTime;

		pStackList = S_InitChannelOperators( stackData );
		ch->m_pStackList = pStackList;

		if( pStackList )
		{
// 			pStackList->SetChannelGuid( ch->guid );
// 			pStackList->SetStartTime( g_pSoundServices->GetHostTime() - params.opStackElapsedTime );
// 			pStackList->SetScriptHash( params.m_nSoundScriptHash );
			if( params.opStackElapsedStopTime > 0.0f )
				pStackList->SetStopTime( g_pSoundServices->GetHostTime() - params.opStackElapsedStopTime );
		}
	}
	ch->m_nSoundScriptHash = params.m_nSoundScriptHash;

	ch->userdata = params.userdata;
	ch->initialStreamPosition = params.initialStreamPosition;
	ch->skipInitialSamples = params.skipInitialSamples;

	if ( IsPC() && ch->userdata != 0 )
	{
		g_pSoundServices->GetToolSpatialization( ch->userdata, ch->guid, si );
	}

#ifdef DEBUG_CHANNELS
	{
		char szTmp[128];
		Q_snprintf( szTmp, sizeof( szTmp ), "Sound %s playing on Dynamic game channel %d\n", sndname, IWavstreamOfCh( ch ) );
		Plat_DebugString(szTmp);
	}
#endif

	CAudioSource *pSource = NULL;

	ch->flags.isSentence = false;
	ch->sfx = params.pSfx;

	VectorCopy( params.origin, ch->origin );
	VectorCopy( params.direction, ch->direction );

	// never update positions if source entity is 0
	ch->flags.bUpdatePositions = params.bUpdatePositions && ( params.soundsource == 0 ? 0 : 1 );
	ch->master_vol = vol;
	ch->flags.m_bCompatibilityAttenuation = SNDLEVEL_IS_COMPATIBILITY_MODE( params.soundlevel );
	if ( ch->flags.m_bCompatibilityAttenuation )
	{
		// Translate soundlevel from its 'encoded' value to a real soundlevel that we can use in the sound system.
		params.soundlevel = SNDLEVEL_FROM_COMPATIBILITY_MODE( params.soundlevel );
	}

	ch->m_flSoundLevel = params.soundlevel; // currently only used to get mixgroup
	ch->dist_mult = SNDLVL_TO_DIST_MULT( params.soundlevel );
	ch->soundsource = params.soundsource;
	S_SetChannelWavtype( ch, sndname );
	ch->basePitch = params.pitch;
	ch->entchannel = params.entchannel;
	ch->flags.fromserver = params.fromserver;
	ch->speakerentity = params.speakerentity;
	ch->flags.m_bShouldPause = (params.flags & SND_SHOULDPAUSE) ? 1 : 0;
	ch->flags.delayed_start = params.m_bDelayedStart;
	ch->flags.m_bUpdateDelayForChoreo = ( params.flags & SND_UPDATE_DELAY_FOR_CHOREO ) != 0;
	ch->flags.m_bInEyeSound = params.m_bInEyeSound;
	// initialize dsp room mixing params
	ch->dsp_mix_min = -1;
	ch->dsp_mix_max = -1;
	// set the default radius
	ch->radius = flSoundRadius;
	ch->m_nSoundScriptHash = params.m_nSoundScriptHash;

	// If the sound is from a speaker, and it's looping, ignore it.
	ch->flags.bSpeaker = (params.flags & SND_SPEAKER) ? 1 : 0;
	if ( ch->flags.bSpeaker )
	{
		if ( params.pSfx->pSource && params.pSfx->pSource->IsLooped() )
		{
			if ( ( nSndShowStart > 0 && 
				nSndShowStart < 7 && 
				nSndShowStart != 4 )
				|| snd_report_verbose_error.GetBool() )
			{
				Msg( "%s(%d): Speaker entity ignored for looping sound '%s'.\n", __FILE__, __LINE__, sndname );
			}
			S_FreeChannel( ch );
			return 0;
		}
	}


	// This should load a mixer object for the sound, too
	if ( bIsSentence )
	{
		// This is a sentence, link words to play in sequence.
		// NOTE: sentence names stored in the cache lookup are pre-pended with a '!'.  Sentence names stored in the
		// sentence file do not have a leading '!'. 
		VOX_LoadSound( ch, PSkipSoundChars( sndname ) );
	}
	else
	{
		// load regular or stream sound
		SoundError soundError;
		pSource = S_LoadSound( params.pSfx, ch, soundError );
		if ( pSource && !IsValidSampleRate( pSource->SampleRate() ) )
		{
			Warning( "S_StartSound: Invalid sample rate (%d) for sound '%s'.\n", pSource->SampleRate(), sndname );
		}
		if ( !pSource && !params.pSfx->m_bIsLateLoad )
		{
			// Display the text about missing sound only the first time, but other texts every time...
			const char * pText = "";
			switch ( soundError )
			{
			case SE_NO_STREAM_BUFFER:
				pText = "No stream buffers are available.";
				break;
			case SE_NO_SOURCE_SETUP:
				// If there was no source, it is probably because the sound was missing to begin with.
				// Pass through
			case SE_FILE_NOT_FOUND:
				{
					static CUtlRBTree< FileNameHandle_t > s_MissingSounds( 0, 0, DefLessFunc( FileNameHandle_t ) );
					FileNameHandle_t h;
					h = g_pFileSystem->FindOrAddFileName( sndname );
					if ( ( s_MissingSounds.Find( h ) == s_MissingSounds.InvalidIndex() ) || snd_report_verbose_error.GetBool() )
					{
						s_MissingSounds.Insert( h );
						pText = "File is missing from disk/repository.";
					}
					else
					{
						pText = NULL;	// Do not display anything if already reported as missing...
					}
				}
				break;
			case SE_CANT_GET_NAME:
				pText = "Can't get name";
				break;
			case SE_SKIPPED:
				pText = "Skipped.";
				break;
			case SE_CANT_CREATE_MIXER:
				pText = "Can't create mixer.";
				break;
			}
			if ( pText != NULL )
			{
				Warning( "[Sound] S_StartSound(): Failed to load sound '%s'. %s\n", sndname, pText );
			}
		}
		ch->flags.isSentence = false;

		//Dry mix voice chat.
		if ( pSource && pSource->GetType() == CAudioSource::AUDIO_SOURCE_VOICE )
		{
			ch->flags.bdry = true;
		}
	}

	if ( !ch->pMixer )
	{
		// couldn't load sounds' data, or sentence has 0 words (not an error)
		if ( snd_report_verbose_error.GetBool() )
		{
			Msg( "%s(%d): Channel does not have a mixer for sound '%s'.\n", __FILE__, __LINE__, sndname );
		}
		S_FreeChannel( ch );
		return 0;
	}

	S_SetChannelStereo( ch, pSource );

	if (nSndShowStart == 5)
	{
		// display gain once only
		snd_showstart.SetValue( 6 );		
		nSndShowStart = 6;
	}

	// get sound type before we spatialize
	MXR_GetMixGroupFromSoundsource( ch );

	// skip the trace on the first spatialization.  This channel may be stolen
	// by another sound played this frame.  Defer the trace to the mix loop
	SND_SpatializeFirstFrameNoTrace( ch );

	// Init client entity mouth movement vars
	ch->flags.m_bIgnorePhonemes = ( params.flags & SND_IGNORE_PHONEMES ) != 0;
	SND_InitMouth( ch );

	// Morasky: below needs to be changed/eliminated for operator stack based sounds

	// If a client can't hear a sound when they FIRST receive the StartSound message,
	// the client will never be able to hear that sound. This is so that out of 
	// range sounds don't fill the playback buffer.  For streaming sounds, we bypass this optimization.
	if ( !params.staticsound && BChannelLowVolume( ch, 0 ) && !toolframework->IsToolRecording() )
	{
		// Looping sounds don't use this optimization because they should stick around until they're killed.
		// Also bypass for speech (GetSentence)
		if ( !params.pSfx->pSource || (!params.pSfx->pSource->IsLooped() && !params.pSfx->pSource->GetSentence()) )
		{
			// if this is long sound, play the whole thing.
			if (!SND_IsLongWave( ch ))
			{
				// DevMsg("S_StartDynamicSound: spatialized to 0 vol & ignored %s", sndname);
				if ( snd_report_verbose_error.GetBool() )
				{
					Msg( "%s(%d): Sound '%s' spatialized to volume 0. Ignored.\n", __FILE__, __LINE__, sndname );
				}
				S_FreeChannel( ch );
				return 0;		// not audible at all
			}
		}
	}

	bool bIsMusic = S_IsMusic( ch );

	// apply global pitch scale to non-music sounds
	if ( !bIsMusic && g_flPitchScale != 1.0f )
	{
		if ( !S_IsPlayerVoice( ch ) )
		{
			int new_pitch = clamp( float(params.pitch) * g_flPitchScale, 0.0f, 255.0f );
			params.pitch = new_pitch;
			ch->basePitch = new_pitch;
		}
	}

	// Pre-startup delay.  Compute # of samples over which to mix in zeros from data source before
	// actually reading first set of samples
	if ( params.delay != 0.0f )
	{
		Assert( ch->sfx );
		Assert( ch->sfx->pSource );

		float rate = ch->sfx->pSource->SampleRate();
		int delaySamples = (int)( params.delay * rate * params.pitch * 0.01f );
		ch->pMixer->SetStartupDelaySamples( delaySamples );
		if ( params.delay > 0 )
		{
			ch->pMixer->SetStartupDelaySamples( delaySamples );
			ch->flags.delayed_start = true;
		}
		else
		{
			int skipSamples = -delaySamples;
			if ( ch->sfx->pSource->GetType() != CAudioSource::AUDIO_SOURCE_MP3)
			{
				// For MP3, SampleCount() is inaccurate (it returns size in bytes of the Mp3 file, not the number of samples).
				// It makes this whole test incorrect. Also MP3 does not support correctly looping either. Don't optimize for MP3 here.
				int totalSamples = ch->sfx->pSource->SampleCount();
				if ( ch->sfx->pSource->IsLooped() )
				{
					skipSamples = skipSamples % totalSamples;
				}

				if ( skipSamples >= totalSamples )
				{
					if ( snd_report_verbose_error.GetBool() )
					{
						Msg( "%s(%d): Negative delay greater than sound length for sound '%s'.\n", __FILE__, __LINE__, sndname );
					}
					S_FreeChannel( ch );
					return 0;
				}
			}

			ch->pitch = ch->basePitch * 0.01f;
			ch->pMixer->SkipSamples( ch, skipSamples, rate, 0 );
			for ( int i = 0; i < MAX_SPLITSCREEN_CLIENTS; ++i )
			{
				gain_t *gs = &ch->gain[ i ];
				gs->ob_gain_target	= 1.0f;
				gs->ob_gain			= 1.0f;
				gs->ob_gain_inc		= 0.0f;
			}
			ch->flags.bfirstpass = false;
			ch->flags.delayed_start = true;
		}
	}

	if ( params.staticsound && S_IsMusic( ch ) )
	{
		// See if we have "music" of same name playing from "world" which means we save/restored this sound already.  If so,
		//  kill the new version and update the soundsource
		CChannelList list;
		g_ActiveChannels.GetActiveChannels( list );
		for ( int i = 0; i < list.Count(); i++ )
		{
			channel_t *pChannel = list.GetChannel(i);
			// Don't mess with the channel we just created, of course
			if ( ch == pChannel )
				continue;
			if ( ch->sfx != pChannel->sfx )
				continue;
			if ( pChannel->soundsource != SOUND_FROM_WORLD )
				continue;
			if ( !S_IsMusic( pChannel ) )
				continue;

			if ( snd_report_verbose_error.GetBool() )
			{
				Msg( "%s(%d): Hooking duplicate restored song track %s\n", __FILE__, __LINE__, sndname );
			}

			// the new channel will have an updated soundsource and probably
			// has an updated pitch or volume since we are receiving this sound message
			// after the sound has started playing (usually a volume change)
			// copy that data out of the source
			pChannel->soundsource = ch->soundsource;
			pChannel->master_vol = ch->master_vol;
			pChannel->basePitch = ch->basePitch;
			pChannel->pitch = ch->pitch;
			S_FreeChannel( ch );
			return 0;
		}
	}

	if (nSndShowStart > 0 && nSndShowStart < 7 && nSndShowStart != 4)
	{
		DevMsg( "%s %s : src %d : channel %d : %d dB : vol %.2f : time %.3f\n", 
			params.staticsound ? "StaticSound" : "DynamicSound",
			sndname, params.soundsource, params.entchannel, params.soundlevel, params.fvol, g_pSoundServices->GetHostTime() );
		if (nSndShowStart == 2 || nSndShowStart == 5)
			DevMsg( "\t dspmix %1.2f : distmix %1.2f : dspface %1.2f : lvol %1.2f : cvol %1.2f : rvol %1.2f : rlvol %1.2f : rrvol %1.2f\n", 
			ch->dspmix, ch->distmix, ch->dspface, 
			ch->fvolume[IFRONT_LEFT], ch->fvolume[IFRONT_CENTER], ch->fvolume[IFRONT_RIGHT], ch->fvolume[IREAR_LEFT], ch->fvolume[IREAR_RIGHT] );
		if (nSndShowStart == 3)
			DevMsg( "\t x: %4f y: %4f z: %4f\n", ch->origin.x, ch->origin.y, ch->origin.z );

		if ( snd_visualize.GetInt() )
		{
			CDebugOverlay::AddTextOverlay( ch->origin, 2.0f, sndname );
		}
	}

	g_pSoundServices->OnSoundStarted( ch->guid, params, sndname );

	return ch->guid;
}

static bool S_ShouldSplitSound( const StartSoundParams_t &params )
{
	if ( !params.pSfx )
		return false;

	// Only certain sound types need to be spatialized separately
	char nameBuf[MAX_PATH];
	char const *pchSoundName = params.pSfx->getname(nameBuf, sizeof(nameBuf));
	if ( TestSoundChar( pchSoundName, CHAR_DOPPLER ) ||
		TestSoundChar( pchSoundName, CHAR_DIRECTIONAL ) ||
		TestSoundChar( pchSoundName, CHAR_DISTVARIANT ) )
	{
		return true;
	}
	return false;
}

CTSQueue< StartSoundParams_t >	g_QueuedSounds;
static int S_StartSound_( StartSoundParams_t& params )
{
	VPROF_( "S_StartSound_", 0, VPROF_BUDGETGROUP_OTHER_SOUND, false, BUDGETFLAG_OTHER );	

	if ( host_threaded_sound.GetInt() )
	{
		// queue the sounds up, drained when viable
		// this also solves not losing inter-loading sounds from network events
		// these queue up and get drained when loading is completed
		if ( g_AudioDevice && g_AudioDevice->IsActive() && ( params.pSfx || params.m_nQueuedGUID > 0 ) )
		{
			bool bGenerateGuid = ( params.m_nQueuedGUID == StartSoundParams_t::GENERATE_GUID );
			if ( params.m_nQueuedGUID == StartSoundParams_t::UNINT_GUID )
			{
				// Generate guid for unambiguous start command, not changes.
				bGenerateGuid = ( ( params.flags & ( SND_STOP | SND_CHANGE_VOL | SND_CHANGE_PITCH ) ) == 0 );
			}
			if ( bGenerateGuid )
			{
				params.m_nQueuedGUID = SND_GetGUID();
			}
			g_QueuedSounds.PushItem( params );
			return params.m_nQueuedGUID;
		}
	}

	return S_StartSound_Immediate( params );
}

int S_StartSound( StartSoundParams_t& params )
{
	// bump the guid here at the earliest possible pre-failure point
	SND_GetGUID();

	// In all cases, we are getting the filename so we can test null.wav
	char nameBuf[MAX_PATH];
	char const *pfn = "(Unknown)";
	if ( params.pSfx != NULL )
	{
		pfn = params.pSfx->GetFileName( nameBuf, sizeof(nameBuf) );
		if ( pfn == NULL )
		{
			pfn = "(null)";
		}
	}

	// Skip the null sound, no reason to waste channels and a bunch of CPU cycles for no reason, don't even report it.
	const char NULL_SOUND[] = "common/null.wav";
	if ( V_stricmp( pfn, NULL_SOUND ) == 0 )
	{
		return 0;
	}

	bool bReport = snd_report_start_sound.GetBool();
	if ( bReport || IsPC() )
	{
		if ( bReport )
		{
			const char * pLooping = "";
			if ( snd_report_loop_sound.GetBool() )
			{
				if ( params.pSfx->pSource != NULL )
				{
					bool bIsLooped = params.pSfx->pSource->IsLooped();
					pLooping = bIsLooped ? "Looping." : "Not looping.";
				}
				else
				{
					pLooping = "CAudioSource is NULL.";
				}
			}

			const char * pFormat = "";
			if ( snd_report_format_sound.GetBool() )
			{
				if ( params.pSfx->pSource != NULL )
				{
					switch ( params.pSfx->pSource->Format() )
					{
					case WAVE_FORMAT_ADPCM:	pFormat = "ADPCM."; break;
					case WAVE_FORMAT_PCM:	pFormat = "PCM."; break;
					case WAVE_FORMAT_XMA:	pFormat = "XMA."; break;
					case WAVE_FORMAT_TEMP:	pFormat = "Fake-MP3."; break;
					case WAVE_FORMAT_MP3:	pFormat = "MP3."; break;
					default: pFormat = "Unknown format."; break;
					}
				}
				else
				{
					if ( pLooping[0] == '\0' )
					{
						// Don't want to write the same text twice.
						pFormat = "CAudioSource is NULL.";
					}
				}
			}			

			Warning( "[Sound] S_StartSound(\"%s\") called. Flags: %d. %s%s\n", pfn, params.flags, pLooping, pFormat );
		}
		if ( IsPC() )
		{
			BlackBox_Record( "wav", "%s", pfn );
		}
	}

	if ( params.flags & SND_UPDATE_DELAY_FOR_CHOREO )
	{
		params.delay = 0;		// If we update for choreo, there is no real need to have a delay (usually few ms before or after).
								// We try to synchronize each sentence after the other and in some cases where the IO latency is a bit high,
								// the accumulated error may actually make very small sounds disappear or sound bad.
	}

	if ( IsGameConsole() && params.delay < 0 && !params.initialStreamPosition && params.pSfx && params.pSfx->pSource )
	{
		// calculate an initial stream position from the expected sample position
		float rate = params.pSfx->pSource->SampleRate();
		int nSamplePosition = (int)( -params.delay * rate * params.pitch * 0.01f );

#ifdef PORTAL2
		// We only use this for Portal 2, as we may want to keep the other behavior when there are machine guns involved.
		const int DONT_SKIP_N_SAMPLES = (int)( rate / 20.0f );		// Let's not skip if less than 1/20th of a second
		if ( nSamplePosition <= DONT_SKIP_N_SAMPLES)
		{
			// Nothing to skip
			params.delay = 0;
		}
		else
#endif
		{
			int nResult = params.pSfx->pSource->SampleToStreamPosition( nSamplePosition );

			// Here are the various possibilities for consoles:
			//	nResult < 0
			//	- For XMA or MP3 with no seek-table, we don't get an initial stream position but we will use the delay to skip the samples.
			//	nResult >= 0
			//	- For XMA with seek-table, we need initial stream position and no delay.
			//  - For WAV file,  we need the initial stream position and a delay (but it will not skip the samples and use SetStartSample() instead).

			if ( nResult >= 0 )
			{
				params.initialStreamPosition = nResult;
				if ( params.pSfx->pSource->Format() == WAVE_FORMAT_XMA )
				{
					// As stated above, if we are in XMA and we got a stream position we need to remove the delay
					params.delay = 0;
					params.m_bDelayedStart = true;
				}
			}
			else
			{
				// If the feature is not supported, we are going to use the other model (skipping the first samples).

				// To avoid the higher I/O requirement (see comment below), we are going to skip samples when we play the sound (instead of ahead of time).
				// In many cases, the delay is actually rather small, so we can actually hide it as part of the normal process.
				// It happens because a lot of sounds are played with a delay to fix timing differences between server and client (think machine gun).
				// However it is applied on many sounds (like VO) where it does not make much sense.

				// For a 32 Kb block, compressed with MP3 (or XMA without seek-table), the compression ratio is around 8x,
				// so a normal block read will contain around 130K mono samples, 65K stereo samples.
				//
				// Thus we can safely skip the first 32K stereo samples as part of the normal process without incurring more I/O pressure.
				// The call to SkipSamples() below is much heavier.

				// MP3 has actually better facility than XMA. It knows the number of samples per frame, and thus can avoid the SPU decoding on PS3. 
				// With XMA, each 2048 bytes block have N samples, so we need to decode one at a time.
				// However if we push too many samples to skip to later (when we play the sound), we could end up  with the sound being delayed with video
				// say after a save in the Portal 2 container ride. So it is better in this case to forcibly skip the samples when we play the sound.

				const int SAFE_NUMBER_OF_SAMPLES_TO_SKIP = params.pSfx->pSource->IsStereoWav() ? 16000 : 32000;
				if ( nSamplePosition < SAFE_NUMBER_OF_SAMPLES_TO_SKIP )
				{
					params.delay = 0;
					params.skipInitialSamples = nSamplePosition;
					params.m_bDelayedStart = true;
				}

				// Although it works on PS3 and X360, the downside is that it has a higher I/O requirement at the beginning of the sound
				// and a much bigger requirement if we have a lot to skip (it is as if we are playing the sound very quickly).
				// On PS3 MP3, it is acceptable, especially as we are using the HDD to store sounds.
				// On X360, it sounds bad the first second, however most VO sound will have seek table, so will support the feature above.
				// As of today, PS3 MP3 does not support seek table.

				// Keep the delay here, it will be used later.
			}
		}
	}

	// It mixes once with volumes for all split players consolidated into a single set of speaker volumes
	return S_StartSound_( params );
}


// SoundEntry handling

void S_CompareSoundParams( StartSoundParams_t &pStartParams , CSoundParameters &pScriptParams )
{
	if( pStartParams.entchannel != pScriptParams.channel )
	{
		Log_Warning( LOG_SOUND_OPERATOR_SYSTEM, "Warning: SoundEntry %s has differing emitter and script channels: emitter %i : script %i\n",
										pStartParams.m_pSoundEntryName,
										pStartParams.entchannel,
										pScriptParams.channel );
	}
	if( pStartParams.delay != pScriptParams.delay_msec )
	{
		Log_Warning( LOG_SOUND_OPERATOR_SYSTEM, "Warning: SoundEntry %s has differing emitter and script delay times: emitter %f : script %d\n",
										pStartParams.m_pSoundEntryName,
										pStartParams.delay,
										pScriptParams.delay_msec );
	}
}


ConVar snd_sos_show_block_debug("snd_sos_show_block_debug", "0", FCVAR_CHEAT, "Spew data about the list of block entries." );

int S_StartSoundEntry( StartSoundParams_t &pStartParams, int nSeed, bool bFromPrestart )
{
	if ( CommandLine()->CheckParm( "-nosound" ) )
	{
		return 0;
	}

	if ( !g_pSoundEmitterSystem )
	{
		DevWarning("Error: SoundEmitterSystem not initialized in engine!");
		return 0;
	}

	if( !g_pSoundEmitterSystem->IsValidHash( pStartParams.m_nSoundScriptHash ))
	{
		DevMsg( "Error: Invalid SoundEntry hash %i received on client", pStartParams.m_nSoundScriptHash );
		return 0;
	}

	// ----------------------------------------------------
	// TODO: Morasky
	// Need to get either the model name networked so we can
	// identify gender or actually network the gender itself
	// ----------------------------------------------------

	// Try to deduce the actor's gender
	gender_t gender = GENDER_NONE;
#if 0
// 
// 	IClientEntity *pClientEntity = NULL;
// 	if ( entitylist )
// 	{
// 		pClientEntity = entitylist->GetClientEntity( pStartParams.soundsource );
// 		if ( pClientEntity )
// 		{
// 			char const *actorModel = STRING( pClientEntity->GetModelName() );
// 			if( actorModel )
// 			{
// 				gender = g_pSoundEmitterSystem->GetActorGender( actorModel );
// 			}
// 		}
// 	}
#endif

	pStartParams.m_pSoundEntryName = g_pSoundEmitterSystem->GetSoundNameForHash( pStartParams.m_nSoundScriptHash );

	if ( !pStartParams.m_pSoundEntryName )
	{
		DevWarning( "Error: Unable to get SoundEntry name for entry : %i", pStartParams.m_nSoundScriptHash );
		return 0;
	}

	CSoundParameters pScriptParams;
	pScriptParams.m_nRandomSeed = nSeed;

	if ( !g_pSoundEmitterSystem->GetParametersForSoundEx( pStartParams.m_pSoundEntryName, pStartParams.m_nSoundScriptHash, pScriptParams, gender, true ) )
	{
		DevWarning("Error: Unable to get parameters for soundentry %s", pStartParams.m_pSoundEntryName );
		return 0;
	}

	if ( !pScriptParams.soundname[0] )
		return 0;

// 	if ( !Q_strncasecmp( pScriptParams.soundname, "vo", 2 ) &&
// 		!( pScriptParams.channel == CHAN_STREAM ||
// 		pScriptParams.channel == CHAN_VOICE ) )
// 	{
// 		DevMsg( "EmitSound:  Voice wave file %s doesn't specify CHAN_VOICE or CHAN_STREAM for sound %s\n",
// 			pScriptParams.soundname, pStartParams.m_pSoundEntryName );
// 	}

	if( pScriptParams.m_pOperatorsKV )
	{
		pStartParams.m_pOperatorsKV = pScriptParams.m_pOperatorsKV;
	}

	pStartParams.m_bHRTFBilinear = pScriptParams.m_bHRTFBilinear;
	pStartParams.m_bHRTFFollowEntity = pScriptParams.m_bHRTFFollowEntity;

	// ----------------------------------------------------
	// debug sanity checking
	// ----------------------------------------------------
	if( snd_sos_show_client_rcv.GetInt() )
	{
		Log_Msg( LOG_SOUND_OPERATOR_SYSTEM, Color( 180, 256, 180, 255 ),
			"Client: Received SoundEntry: %i : %s : %s : operators: %s: seed: %i\n",
							pStartParams.m_nSoundScriptHash,
							pStartParams.m_pSoundEntryName,
							pScriptParams.soundname,
							pScriptParams.m_pOperatorsKV ? "true" : "false",
							nSeed );

	}


#if 0
	S_CompareSoundParams( pStartParams , pScriptParams );
#endif

	// only block and execute start stack if an actual "start" message
	if ( !( pStartParams.flags & SND_STOP || 
			pStartParams.flags & SND_CHANGE_PITCH ||
			pStartParams.flags & SND_CHANGE_VOL ) )
	{

		// check for a blocked entry
		CSosEntryMatch sosEntryMatch;
		V_strncpy( sosEntryMatch.m_nMatchString1, pStartParams.m_pSoundEntryName, sizeof( sosEntryMatch.m_nMatchString1 ) );
	//	V_strncpy( sosEntryMatch.m_nMatchString2, pScriptParams.soundname, sizeof( sosEntryMatch.m_nMatchString2 ) );
		sosEntryMatch.m_nMatchInt1 = pScriptParams.channel;
		sosEntryMatch.m_nMatchInt2 = pStartParams.soundsource;
		if( g_pSoundOperatorSystem->m_sosEntryBlockList.HasAMatch( &sosEntryMatch ) )
		{
			if( snd_sos_show_block_debug.GetInt() )
			{
				Log_Msg(LOG_SOUND_OPERATOR_SYSTEM, Color( 180, 256, 180, 255 ), "Entry Blocked: %s\n", pStartParams.m_pSoundEntryName );
			}
			return 0;
		}

		stack_data_t stackData;
		stackData.m_nSoundScriptHash = pScriptParams.m_hSoundScriptHash;
		stackData.m_pOperatorsKV = pScriptParams.m_pOperatorsKV;	
		g_scratchpad.SetPerExecution( NULL, &pStartParams );

		if( !bFromPrestart )
		{

			CSosOperatorStack *pCueStack = S_GetStack( CSosOperatorStack::SOS_CUE, stackData );

			if( pCueStack )
			{

				pCueStack->Execute(  NULL, &g_scratchpad );
	 			if( snd_sos_show_operator_prestart.GetInt() )
	 			{
	 				pCueStack->Print( 0 );
	 			}

				pCueStack->Shutdown();
				delete pCueStack;

				if( g_scratchpad.m_bBlockStart )
				{
					return 0;
				}
				if( g_scratchpad.m_flDelayToQueue > 0.0 )
				{
					g_pSoundOperatorSystem->QueueStartEntry( pStartParams, g_scratchpad.m_flDelayToQueue, true );
					return 0;
				}
			}
		}

		CSosOperatorStack *pStartStack = S_GetStack( CSosOperatorStack::SOS_START, stackData );

		if( pStartStack )
		{
			pStartStack->SetScriptHash( pScriptParams.m_hSoundScriptHash );

			pStartStack->Execute(  NULL, &g_scratchpad );
			if( snd_sos_show_operator_start.GetInt() )
			{
				const char *pFilterString = snd_sos_show_operator_entry_filter.GetString();
				if( !pFilterString || !pFilterString[0] || ( pFilterString && pFilterString[0] && V_stristr( pStartParams.m_pSoundEntryName, pFilterString ) )) 
				{
					pStartStack->Print( 0 );
				}
			}
			pStartStack->Shutdown();
			delete pStartStack;

			pStartParams.delay = g_scratchpad.m_flDelay;

			// this gets set to true in "SetPerExecution" or the start_stack
			if( g_scratchpad.m_bBlockStart )
			{
				return 0;
			}
		}
	}

	CSfxTable *pSound = S_PrecacheSound( pScriptParams.soundname );

	if (!pSound)
		return 0;

	pStartParams.pSfx = pSound;

	return S_StartSound( pStartParams );
}


// Restart all the sounds on the specified channel
inline bool IsChannelLooped( int iChannel )
{
	return (channels[iChannel].sfx &&
			channels[iChannel].sfx->pSource && 
			channels[iChannel].sfx->pSource->IsLooped() );
}

int S_GetCurrentStaticSounds( SoundInfo_t *pResult, int nSizeResult, int entchannel )
{
	int nSlot = 0;

	int nSpaceRemaining = nSizeResult;
	char nameBuf[MAX_PATH];
	for (int i = MAX_DYNAMIC_CHANNELS; i < total_channels && nSpaceRemaining; i++)
	{
		if ( channels[i].entchannel == entchannel && channels[i].sfx )
		{
			pResult->Set( channels[i].soundsource, 
				channels[i].entchannel, 
				channels[i].sfx->getname(nameBuf, sizeof(nameBuf)), 
				channels[i].origin,
				channels[i].direction,
				( (float)channels[i].master_vol / 255.0 ),
				DIST_MULT_TO_SNDLVL( channels[i].dist_mult ),
				IsChannelLooped( i ),
				channels[i].basePitch,
				listener_origin[ nSlot ],
				channels[i].speakerentity,
				0 ); // unspecified soundfile index is fine here

			pResult++;
			nSpaceRemaining--;
		}
	}
	return (nSizeResult - nSpaceRemaining);
}


// Stop all sounds for entity on a channel.
void S_StopSound(int soundsource, int entchannel)
{
	THREAD_LOCK_SOUND();
	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );
	for ( int i = 0; i < list.Count(); i++ )
	{
		channel_t *pChannel = list.GetChannel(i);
		if (pChannel->soundsource == soundsource
			&& pChannel->entchannel == entchannel)
		{
			S_StopChannelUnlocked( pChannel );
		}
	}
}

channel_t *S_FindChannelByGuid( int guid )
{
	return g_ActiveChannels.FindActiveChannelByGuid( guid );
}

//-----------------------------------------------------------------------------
channel_t *S_FindChannelByScriptHash( HSOUNDSCRIPTHASH nHandle )
{
	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );
	for ( int i = 0; i < list.Count(); i++ )
	{
		channel_t *pChannel = list.GetChannel(i);
		if ( pChannel->m_nSoundScriptHash == nHandle )
		{
			return pChannel;
		}
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : guid - 
//-----------------------------------------------------------------------------
void S_StopSoundByGuid( int guid, bool bForceSync )
{
	if ( host_threaded_sound.GetInt() && !bForceSync )
	{
		// queued sounds, must also queue stops, volume & pitch changes
		StartSoundParams_t params;
		params.flags = SND_STOP;
		params.m_nQueuedGUID = guid;
		g_QueuedSounds.PushItem( params );
		return;
	}
	THREAD_LOCK_SOUND();
	while ( true )
	{
		channel_t *pChannel = S_FindChannelByGuid( guid );
		if( pChannel )
		{
			if ( S_StopChannelUnlocked( pChannel ) == SCR_Delayed )
			{
				// Because it is delayed, the channel is still there, have to stop the loop now
				break;
			}
		}
		else
		{
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gets the sound duration.
// Input  : pChannel - Channel to get the duration from.
// Output : The sound duration.
//-----------------------------------------------------------------------------
float S_SoundDuration( channel_t * pChannel )
{
	if ( !pChannel || !pChannel->sfx )
		return 0.0f;

	// NOTE: Looping sounds will return the length of a single loop
	// Use S_IsLoopingSoundByGuid to see if they are looped

	return AudioSource_GetSoundDuration( pChannel->sfx ) / ( pChannel->basePitch * 0.01f );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : guid - 
//-----------------------------------------------------------------------------
float S_SoundDurationByGuid( int guid )
{
	THREAD_LOCK_SOUND();
	channel_t *pChannel = S_FindChannelByGuid( guid );
	return S_SoundDuration( pChannel );
}

//-----------------------------------------------------------------------------
// Is this sound a looping sound?
//-----------------------------------------------------------------------------
bool S_IsLoopingSoundByGuid( int guid )
{
	channel_t *pChannel = S_FindChannelByGuid( guid );
	if ( !pChannel || !pChannel->sfx )
		return false;

	return( pChannel->sfx->pSource->IsLooped() );
}


//-----------------------------------------------------------------------------
// Purpose: Note that the guid is preincremented, so we can just return the current value as the "last sound" indicator
// Input  :  - 
// Output : int
//-----------------------------------------------------------------------------
int S_GetGuidForLastSoundEmitted()
{
	return s_nSoundGuid;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : guid - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool S_IsSoundStillPlaying( int guid )
{
	// sound was submitted after last channel queue went into the mix, won't have a channel yet
	THREAD_LOCK_SOUND();
	if ( host_threaded_sound.GetBool() )
	{ 
		AUTO_LOCK( g_ActiveSoundListMutex );
		if ( guid > s_nMaxQueuedGUID )
		{
			// If the GUID is greater than the last set of queued GUID, it means the sound has probably not made through the active sound list yet
			// We assume that it is still playing. This is not accurate though if the caller passed a bogus GUID.
			return true;
		}
		// don't need to lock the sound mutex if we use this list
		for ( int i = 0; i < g_ActiveSoundsLastUpdate.Count(); i++ )
		{
			if ( guid == g_ActiveSoundsLastUpdate[i].m_nGuid )
				return true;
		}
		return false;
	}
	channel_t *pChannel = S_FindChannelByGuid( guid );
	return pChannel != NULL ? true : false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : guid - 
//			fvol - 
//-----------------------------------------------------------------------------
void S_SetVolumeByGuid( int guid, float fvol )
{
	if ( host_threaded_sound.GetInt() )
	{
		// queued sounds, must also queue stops, volume & pitch changes
		StartSoundParams_t params;
		params.flags = SND_CHANGE_VOL;
		params.fvol = fvol;
		params.m_nQueuedGUID = guid;
		g_QueuedSounds.PushItem( params );
		return;
	}

	if ( channel_t *pChannel = S_FindChannelByGuid( guid ) )
	{
		pChannel->master_vol = 255.0f * clamp( fvol, 0.0f, 1.0f );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Gets the elapsed time.
// Input  : pChannel - the channel to get the elapsed time from.
// Output : The elapsed time.
//-----------------------------------------------------------------------------
float S_GetElapsedTime( const channel_t * pChannel )
{
	if ( !pChannel )
		return 0.0f;

	CAudioMixer *mixer = pChannel->pMixer;
	if ( !mixer )
		return 0.0f;

	CAudioSource * pSource = mixer->GetSource();
	if ( !pSource )
		return 0.0f;

	float divisor = ( pSource->SampleRate() * pChannel->pitch * 0.01f );
	if( divisor <= 0.0f )
		return 0.0f;

	float elapsed = mixer->GetSamplePosition() / divisor;
	return elapsed;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : guid - 
// Output : float 
//-----------------------------------------------------------------------------
float S_GetElapsedTimeByGuid( int guid )
{
	if ( host_threaded_sound.GetBool() )
	{
		AUTO_LOCK( g_ActiveSoundListMutex );
		if ( guid < s_nMaxQueuedGUID )
		{
			// The guid is in the range of GUIDs from current active sounds, let's do the look-up.

			// don't need to lock the sound mutex if we use this list
			for ( int i = 0; i < g_ActiveSoundsLastUpdate.Count(); i++ )
			{
				if ( guid == g_ActiveSoundsLastUpdate[i].m_nGuid )
					return g_ActiveSoundsLastUpdate[i].m_flElapsedTime;
			}
		}
		// We did not find the GUID, that's an error condition returning 0.0f.
		// Or we did not access it yet from the thread sound, in that case the elapsed time of the sound is 0.0f.
		return 0.0f;
	}

	THREAD_LOCK_SOUND();
	channel_t *pChannel = S_FindChannelByGuid( guid );
	return S_GetElapsedTime( pChannel );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : sndlist - 
//-----------------------------------------------------------------------------
void S_GetActiveSounds( CUtlVector< SndInfo_t >& sndlist )
{
	THREAD_LOCK_SOUND();
	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );
	for ( int i = 0; i < list.Count(); i++ )
	{
		channel_t *ch = list.GetChannel(i);

		SndInfo_t info;

		info.m_nGuid			= ch->guid;
		info.m_filenameHandle	= ch->sfx ? ch->sfx->GetFileNameHandle() : NULL;
		info.m_nSoundSource		= ch->soundsource;
		info.m_nChannel			= ch->entchannel;
		// If a sound is being played through a speaker entity (e.g., on a monitor,), this is the
		//  entity upon which to show the lips moving, if the sound has sentence data
		info.m_nSpeakerEntity	= ch->speakerentity;
		info.m_flVolume			= (float)ch->master_vol / 255.0f;
		info.m_flLastSpatializedVolume = ch->last_vol;
		// Radius of this sound effect (spatialization is different within the radius)
		info.m_flRadius			= ch->radius;
		info.m_nPitch			= ch->basePitch;
		info.m_pOrigin			= &ch->origin;
		info.m_pDirection		= &ch->direction;

		// if true, assume sound source can move and update according to entity
		info.m_bUpdatePositions = ch->flags.bUpdatePositions;
		// true if playing linked sentence
		info.m_bIsSentence		= ch->flags.isSentence;
		// if true, bypass all dsp processing for this sound (ie: music)	
		info.m_bDryMix			= ch->flags.bdry;
		// true if sound is playing through in-game speaker entity.
		info.m_bSpeaker			= ch->flags.bSpeaker;
		// for snd_show, networked sounds get colored differently than local sounds
		info.m_bFromServer		= ch->flags.fromserver; 

		sndlist.AddToTail( info );
	}
}

void S_StopAllSounds( bool bClear )
{
	THREAD_LOCK_SOUND();
	int		i;

	if ( !g_AudioDevice )
		return;

	if ( !g_AudioDevice->IsActive() )
		return;

	total_channels = MAX_DYNAMIC_CHANNELS;	// no statics


	DevMsg( 1, "Stopping All Sounds...\n" );
	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );
	for ( i = 0; i < list.Count(); i++ )
	{
		char nameBuf[MAX_PATH];
		channel_t *pChannel = list.GetChannel( i );
		char *pName = nameBuf;
		if ( pChannel->sfx )
		{
			pChannel->sfx->getname( nameBuf, sizeof( nameBuf ) );
		}
		else
		{
			pName = "Unknown";
		}
		DevMsg( 1, "Stopping: Channel:%2d %s\n", list.GetChannelIndex( i ), pName );

		S_FreeChannel( pChannel );
	}
	// flush the mouth update queue
	SND_MouthUpdateAll();
	g_QueuedSounds.Purge();

	// sound operator system stuff
	g_pSoundOperatorSystem->ClearSubSystems();

	Q_memset( channels, 0, MAX_CHANNELS * sizeof(channel_t) );

	if ( bClear )
	{
		S_ClearBuffer();
	}

	// Clear any remaining soundfade
	memset( &soundfade, 0, sizeof( soundfade ) );

	Assert( g_ActiveChannels.GetActiveCount() == 0 );
}

void S_PreventSound( bool bSetting )
{
	g_bPreventSound = bSetting;
}
bool S_GetPreventSound( void )
{
	return g_bPreventSound;
}

void S_StopAllSoundsC( void )
{
	S_StopAllSounds( true );
}

void S_OnLoadScreen( bool value )
{
	s_bOnLoadScreen = value;
}

void S_ClearBuffer( void )
{
	if ( !g_AudioDevice )
		return;

	g_AudioDevice->ClearBuffer();
	DSP_ClearState();
	MIX_ClearAllPaintBuffers( PAINTBUFFER_SIZE, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : percent - 
//			holdtime - 
//			intime - 
//			outtime - 
//-----------------------------------------------------------------------------
void S_SoundFade( float percent, float holdtime, float intime, float outtime )
{
	soundfade.starttime				= g_pSoundServices->GetHostTime();  

	soundfade.initial_percent		= percent;       
	soundfade.fadeouttime			= outtime;    
	soundfade.holdtime				= holdtime;   
	soundfade.fadeintime			= intime;
}

//-----------------------------------------------------------------------------
// Purpose: Modulates sound volume on the client.
//-----------------------------------------------------------------------------
void S_UpdateSoundFade(void)
{
	float	totaltime;
	float	f;
	// Determine current fade value.

	// Assume no fading remains
	soundfade.percent = 0;  

	totaltime = soundfade.fadeouttime + soundfade.fadeintime + soundfade.holdtime;

	float elapsed = g_pSoundServices->GetHostTime() - soundfade.starttime;

	// Clock wrapped or reset (BUG) or we've gone far enough
	if ( elapsed < 0.0f || elapsed >= totaltime || totaltime <= 0.0f )
	{
		return;
	}

	// We are in the fade time, so determine amount of fade.
	if ( soundfade.fadeouttime > 0.0f && ( elapsed < soundfade.fadeouttime ) )
	{
		// Ramp up
		f = elapsed / soundfade.fadeouttime;
	}
	// Inside the hold time
	else if ( elapsed <= ( soundfade.fadeouttime + soundfade.holdtime ) )
	{
		// Stay
		f = 1.0f;
	}
	else
	{
		// Ramp down
		f = ( elapsed - ( soundfade.fadeouttime + soundfade.holdtime ) ) / soundfade.fadeintime;
		// backward interpolated...
		f = 1.0f - f;
	}

	// Spline it.
	f = SimpleSpline( f );
	f = clamp( f, 0.0f, 1.0f );

	soundfade.percent = soundfade.initial_percent * f;
}


//=============================================================================

// Global Voice Ducker - enabled in vcd scripts, when characters deliver important dialog.  Overrides all
// other mixer ducking, and ducks all other sounds except dialog.

ConVar snd_ducktovolume( "snd_ducktovolume", "0.55", FCVAR_ARCHIVE );
ConVar snd_duckerattacktime( "snd_duckerattacktime", "0.5", FCVAR_ARCHIVE );
ConVar snd_duckerreleasetime( "snd_duckerreleasetime", "2.5", FCVAR_ARCHIVE );
ConVar snd_duckerthreshold("snd_duckerthreshold", "0.15", FCVAR_ARCHIVE );
ConVar snd_ducking_off("snd_ducking_off", "1", FCVAR_ARCHIVE );

static void S_UpdateVoiceDuck( int voiceChannelCount, int voiceChannelMaxVolume, float frametime )
{
	if( !snd_ducking_off.GetInt() )
	{
		float volume_when_ducked = snd_ducktovolume.GetFloat();
		int volume_threshold = (int)(snd_duckerthreshold.GetFloat() * 255.0);

		float duckTarget = 1.0;
		if ( voiceChannelCount > 0 )
		{
			voiceChannelMaxVolume = clamp(voiceChannelMaxVolume, 0, 255);
			
			// duckTarget = RemapVal( voiceChannelMaxVolume, 0, 255, 1.0, volume_when_ducked );
		
			// KB: Change: ducker now active if any character is speaking above threshold volume.
			// KB: Active ducker drops all volumes to volumes * snd_duckvolume

			if ( voiceChannelMaxVolume > volume_threshold )
				duckTarget = volume_when_ducked;
		}
		float rate = ( duckTarget < g_DuckScale ) ? snd_duckerattacktime.GetFloat() : snd_duckerreleasetime.GetFloat();
		g_DuckScale = Approach( duckTarget, g_DuckScale, frametime * ((1-volume_when_ducked) / rate) );
		g_DuckScaleInt256 = g_DuckScale * 256.0f;
	}
	else
	{
		g_DuckScale = 1.0;
		g_DuckScaleInt256 = 256;
	}
}

// set 2d forward vector, given 3d right vector.
// NOTE: this should only be used for a listener forward
// vector from a listener right vector. It is not a general use routine.

void ConvertListenerVectorTo2D( Vector *pvforward, const Vector *pvright )
{
	// get 2d forward direction vector, ignoring pitch angle
	QAngle angles2d;
	Vector source2d;
	Vector listener_forward2d;

	source2d = *pvright;
	source2d.z = 0.0;

	VectorNormalize(source2d);

	// convert right vector to euler angles (yaw & pitch)

	VectorAngles(source2d, angles2d);

	// get forward angle of listener

	angles2d[PITCH]	= 0;
	angles2d[YAW] += 90; // rotate 90 ccw
	angles2d[ROLL] = 0;
	
	if (angles2d[YAW] >= 360)
		angles2d[YAW] -= 360;

	AngleVectors(angles2d, &listener_forward2d);

	VectorNormalize(listener_forward2d);

	*pvforward = listener_forward2d;
}

// If this is nonzero, we will only spatialize some of the static 
// channels each frame. The round robin will spatialize 1 / (2 ^ x) 
// of the spatial channels each frame.
ConVar snd_spatialize_roundrobin( "snd_spatialize_roundrobin", "0", FCVAR_NONE, "Lowend optimization: if nonzero, spatialize only a fraction of sound channels each frame. 1/2^x of channels will be spatialized per frame." );

// draw a curve of the db based volume falloff
ConVar snd_debug_gaincurve( "snd_debug_gaincurve", "0", FCVAR_NONE, "Visualize sound gain fall off" );
ConVar snd_debug_gaincurvevol( "snd_debug_gaincurvevol", "1.0", FCVAR_NONE, "Visualize sound gain fall off" );

void DEBUG_drawGainCurve(void)
{
	CUtlVector<float>	gainList;

	float startY = .03;
	float totalY = .4;
	float startX = .03;
	float minGain = .015;
	int maxEntries = 800;
	float stepDist = 12;
	for(int i = 0; i < maxEntries; i++)
	{
//		float gain = SND_GetGainFromMult( float gain, float dist_mult, vec_t dist )
		float gain = SND_GetGainFromMult( snd_debug_gaincurvevol.GetFloat(), SNDLVL_TO_DIST_MULT(snd_debug_gaincurve.GetInt()), stepDist * (float)i );

		if(gain < minGain)
			break;
	//	gainList.AddToTail((float)(maxEntries - i) / ((float) maxEntries));
		gainList.AddToTail(gain);

	}

	int count = gainList.Count();
	char str[32];
	sprintf(str, "%i ft", 0);  
	CDebugOverlay::AddScreenTextOverlay(0, 0, .1, 0, 255, 0, 255,  str);

	sprintf(str, "%i ft", count / 2); 
	CDebugOverlay::AddScreenTextOverlay(.5, 0, .1, 0, 255, 0, 255,  str);

	sprintf(str, "%i ft", count / 4); 
	CDebugOverlay::AddScreenTextOverlay(.25, 0, .1, 0, 255, 0, 255,  str);

	sprintf(str, "%i ft", count / 2 + count / 4); 
	CDebugOverlay::AddScreenTextOverlay(.75, 0, .1, 0, 255, 0, 255,  str);

	sprintf(str, "%i ft", count); 
	CDebugOverlay::AddScreenTextOverlay(.95, 0, .1, 0, 255, 0, 255,  str);


	sprintf(str, "1.0"); 
	CDebugOverlay::AddScreenTextOverlay(0, startY, .1, 0, 255, 0, 255,  str);

	sprintf(str, "0.75"); 
	CDebugOverlay::AddScreenTextOverlay(0, startY + (totalY * .25), .1, 0, 255, 0, 255,  str);

	sprintf(str, "0.5"); 
	CDebugOverlay::AddScreenTextOverlay(0, startY + (totalY * .5), .1, 0, 255, 0, 255,  str);

	sprintf(str, "0.25"); 
	CDebugOverlay::AddScreenTextOverlay(0, startY + (totalY * .75), .1, 0, 255, 0, 255,  str);


	sprintf(str, "0.0"); 
	CDebugOverlay::AddScreenTextOverlay(0, startY + totalY , .1, 0, 255, 0, 255,  str);

	for(int i = 0; i < count; i++)
	{
		CDebugOverlay::AddScreenTextOverlay(startX + (float)(((float)i)*(float)(1.0 / (float)count)), startY + (totalY - (gainList[i]*totalY)), .1, 0, 255, 0, 255,  "+");

		CDebugOverlay::AddScreenTextOverlay(startX + (float)(((float)i)*(float)(1.0 / (float)count)), startY, .1, 255, 0, 0, 255,  "-");
		CDebugOverlay::AddScreenTextOverlay(startX + (float)(((float)i)*(float)(1.0 / (float)count)), startY + totalY, .1, 255, 0, 0, 255,  "-");

		CDebugOverlay::AddScreenTextOverlay(startX,  startY + (totalY - (gainList[i]*totalY)), .1, 255, 0, 0, 255,  "+");

	}
}

 ConVar snd_debug_panlaw( "snd_debug_panlaw", "0", FCVAR_CHEAT, "Visualize panning crossfade curves" );

void S_StartQueuedSounds()
{
	if ( !g_QueuedSounds.Count() || g_bPreventSound )
	{
		// empty or not ready to drain the queued yet
		return;
	}

	// Update the max queued GUID only if it is greater, modifying an old sound should not change the behavior of this value
	int lastGUID = s_nMaxQueuedGUID;
	while ( 1 )
	{
		StartSoundParams_t soundParams;
		if ( !g_QueuedSounds.PopItem( &soundParams ) )
		{
			break;
		}
		int nGuid = S_StartSound_Immediate( soundParams );
		lastGUID = MAX( lastGUID, nGuid );
	}
	s_nMaxQueuedGUID = lastGUID;
}

static CUtlHashtable< HSOUNDSCRIPTHASH, int > s_SoundsPrintedLastUpdate;
void OnSndShowEdgeChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	s_SoundsPrintedLastUpdate.Purge();
}
/*
============
S_Update

Called once each time through the main loop
============
*/
void S_Update( const CAudioState *pAudioState )
{
	VPROF_BUDGET( "S_Update", VPROF_BUDGETGROUP_OTHER_SOUND );

	int			i;
	channel_t	*ch;
	channel_t	*combine;

#if !USE_AUDIO_DEVICE_V1
	S_CheckDevice();
#endif

	// check for errors and handle them
	if ( !g_AudioDevice->IsActive() )
		return;

	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	// update soundoperator system before doing anything that uses
	g_pSoundOperatorSystem->Update();

	// start queued sounds
	if ( host_threaded_sound.GetInt() )
	{
		S_StartQueuedSounds();
	}

	g_SndMutex.Lock();
	if ( host_threaded_sound.GetInt() )
	{
		AUTO_LOCK( g_ActiveSoundListMutex );
		g_ActiveChannels.CopyActiveSounds( g_ActiveSoundsLastUpdate );
	}

	g_SndMergeMethod = (ESndMergeMethod)clamp( snd_mergemethod.GetInt(), 0, SND_MERGE_COUNT - 1 );

	// Update any client side sound fade
	S_UpdateSoundFade();
	
	// pipe the mouth events to the client
	SND_MouthUpdateAll();

	// should make this all access matrix vectors instead of euler trig operations?
	if ( pAudioState )
	{
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			VectorCopy( pAudioState->GetPerUser( hh ).m_Origin, listener_origin[ hh ] );
			AngleVectors( pAudioState->GetPerUser( hh ).m_Angles, &listener_forward[ hh ], &listener_right[ hh ], &listener_up[ hh ] ); 
		}
		s_bIsListenerUnderwater = pAudioState->IsAnyPlayerUnderwater();
	}
	else
	{
		FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
		{
			VectorCopy( vec3_origin, listener_origin[ hh ] );
			VectorCopy( vec3_origin, listener_forward[ hh ] );
			VectorCopy( vec3_origin, listener_right[ hh ] );
			VectorCopy( vec3_origin, listener_up[ hh ] );
		}
		s_bIsListenerUnderwater = false;
	}

	// making copies for the operator system
	int count = 0;
	g_scratchpad.m_vBlendedListenerOrigin.Init( 0.0, 0.0, 0.0 );
	FOR_EACH_VALID_SPLITSCREEN_PLAYER( hh )
	{
		VectorCopy( listener_origin[ hh ], g_scratchpad.m_vPlayerOrigin[hh] );
		VectorCopy( listener_forward[ hh ], g_scratchpad.m_vPlayerForward[hh] );
		VectorCopy( listener_right[ hh ], g_scratchpad.m_vPlayerRight[hh]  );
		VectorCopy( listener_up[ hh ], g_scratchpad.m_vPlayerUp[hh]  );
		g_scratchpad.m_vBlendedListenerOrigin += g_scratchpad.m_vPlayerOrigin[ hh ];
		++count;
	}

	//////////////////////////////////////////////////////////////////////////
	// For Splitscreen this is the average position, a total hack!!!
	// used in getting client state, etc. (AND facing..??)
	//////////////////////////////////////////////////////////////////////////
	if ( count > 1 )
	{
		g_scratchpad.m_vBlendedListenerOrigin /= (float)count;
	}


	combine = NULL;

	int voiceChannelCount = 0;
	int voiceChannelMaxVolume = 0;

	// visualizer for distance falloff curve
	if ( snd_debug_gaincurve.GetInt() )
	{
		DEBUG_drawGainCurve();
	}
	// visualizer for distance falloff curve
	if ( snd_debug_panlaw.GetInt() )
	{
		DEBUG_DrawPanCurves();
	}
	// reset traceline counter for this frame
	g_snd_trace_count = 0;

	// calculate distance to nearest walls, update dsp_spatial
	// updates one wall only per frame (one trace per frame)
	SND_SetSpatialDelays();

	// updates dsp_room if automatic room detection enabled
	DAS_CheckNewRoomDSP();

	// update mix group solo status
	MXR_SetSoloActive();

	// update spatialization for static and dynamic sounds	
	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );

	if ( snd_spatialize_roundrobin.GetInt() == 0 )
	{
		// spatialize each channel each time
		for ( i = 0; i < list.Count(); i++ )
		{
			ch = list.GetChannel(i);
			Assert( ch->sfx );
			Assert( ch->activeIndex > 0 );

			// respatialize channel
			SND_Spatialize( ch );

			if ( ch->sfx->pSource && ch->sfx->pSource->IsVoiceSource() )
			{
				voiceChannelCount++;
				int iThisChannelMaxVol = ChannelGetMaxVol( ch );
				voiceChannelMaxVolume = MAX( voiceChannelMaxVolume, iThisChannelMaxVol );
			}
		}
	}
	else
	{
		static unsigned int s_roundrobin = 0 ; ///< number of times this function is called.
		///< used instead of host_frame because that number
		///< isn't necessarily available here (sez Yahn).

		// lowend performance improvement: spatialize only some channels each frame.
		unsigned int robinmask = (1 << snd_spatialize_roundrobin.GetInt()) - 1;

		// now do static channels
		for ( i = 0 ; i < list.Count() ; ++i )
		{
			ch = list.GetChannel(i);
			Assert(ch->sfx);
			Assert(ch->activeIndex > 0);

			// need to check bfirstpass because sound tracing may have been deferred
			if ( ch->flags.bfirstpass || (robinmask & s_roundrobin) == ( i & robinmask ) )
			{
				SND_Spatialize(ch);         // respatialize channel
			}

			if ( ch->sfx->pSource && ch->sfx->pSource->IsVoiceSource() )
			{
				voiceChannelCount++;
				int iThisChannelMaxVol = ChannelGetMaxVol( ch );
				voiceChannelMaxVolume = MAX( voiceChannelMaxVolume, iThisChannelMaxVol );
			}
		}

		++s_roundrobin;
	}

	SND_ChannelTraceReset();

	// check if stacks associated with channels were stopped
	// do this before new stops happen and don't get updated
	// MORASKY: introduces yet another 1 frame delay
	CChannelList stopList;
	g_ActiveChannels.GetActiveChannels( stopList );
	// spatialize each channel each time
	for ( i = 0; i < stopList.Count(); i++ )
	{
		ch = stopList.GetChannel(i);
		if( ch->m_pStackList && ch->m_pStackList->IsStopped() )
		{
			S_FreeChannel( ch );
		}
	}

	// drain updated sos start queue
	g_pSoundOperatorSystem->StartQueuedEntries();

	// drain updated sos start queue
	g_pSoundOperatorSystem->StopQueuedChannels();



	// set new target for voice ducking
	float frametime = g_pSoundServices->GetHostFrametime();
	S_UpdateVoiceDuck( voiceChannelCount, voiceChannelMaxVolume, frametime );

	// update x360 music volume
	g_DashboardMusicMixValue = Approach( g_DashboardMusicMixTarget, g_DashboardMusicMixValue, g_DashboardMusicFadeRate * frametime );

	//
	// debugging output
	//

	g_pSoundOperatorSystem->DEBUG_ShowTrackList( );
	g_pSoundOperatorSystem->DEBUG_ShowOpvarList( );

	if ( snd_show.GetInt() || snd_show_print.GetInt())
	{
		static int s_nPrintTickCount = 0; s_nPrintTickCount++;// = g_ClientGlobalVariables.tickcount;
		con_nprint_t np;
		np.time_to_live = 2.0f;
		np.fixed_width_font = true;

		int numActiveChannels = g_ActiveChannels.GetActiveCount();
		int total = 0;

		int sndsurround = snd_surround.GetInt();

		np.index = 0;
		np.color[0] = 1.0;
		np.color[1] = 1.0;
		np.color[2] = 1.0;
		Con_NXPrintf ( &np, "Total Channels: %i", numActiveChannels);

		char nameBuf[ 256 ];

		for ( int i = 0; i < list.Count(); i++ )
		{
			channel_t *ch = list.GetChannel(i);
			if ( !ch->sfx )
				continue;

			if( snd_show.GetInt() == 2 && ch->entchannel >= CHAN_STATIC )
			{
				continue;
			}
			else if( snd_show.GetInt() == 3 && ch->entchannel != CHAN_STATIC )
			{
				continue;
			}

			char nSoundEntryName[64] = "";
			if( ch->m_nSoundScriptHash != SOUNDEMITTER_INVALID_HASH )
			{
				const char *pSoundEntryName = g_pSoundEmitterSystem->GetSoundNameForHash( ch->m_nSoundScriptHash );
				if( pSoundEntryName )
				{
					if ( const char *pFilter = snd_show_filter.GetString() )
					{
						if ( *pFilter && !V_stristr( pSoundEntryName, pFilter ) )
						{
							continue;
						}
					}
					V_strncpy( nSoundEntryName, pSoundEntryName, sizeof(nSoundEntryName) );
				}
			}
			np.index = total + 2;
			if ( ch->flags.fromserver )
			{
				np.color[0] = 1.0;
				np.color[1] = 0.8;
				np.color[2] = 0.1;
			}
			else
			{
				np.color[0] = 0.1;
				np.color[1] = 0.9;
				np.color[2] = 1.0;
			}

			unsigned int sampleCount = RemainingSamples( ch );
			float timeleft = (float)sampleCount / (float)ch->sfx->pSource->SampleRate();
			bool bLooping = ch->sfx->pSource->IsLooped();
			if (snd_show.GetInt())
			{
				if (ch->wavtype == CHAR_HRTF)
				{
					const float hdist = sqrt(ch->hrtf.vec.x*ch->hrtf.vec.x + ch->hrtf.vec.z*ch->hrtf.vec.z);
					const float yaw = -VEC_RAD2DEG(atan2(-ch->hrtf.vec.x, -ch->hrtf.vec.z));
					const float pitch = VEC_RAD2DEG(atan2(ch->hrtf.vec.y, hdist));
					
					Con_NXPrintf(&np, "%s %02i hrtf(%.2f %.2f %.2f) yaw(%.2f) pitch(%.2f) xfade(%.2f) vol(%.2f %.2f) ent(%03d) pos(%6d %6d %6d) timeleft(%f) looped(%d) %50s",
						nSoundEntryName,
						total + 1,
						ch->hrtf.vec.x,
						ch->hrtf.vec.y,
						ch->hrtf.vec.z,
						yaw,
						pitch,
						ch->hrtf.lerp,
						ch->fvolume_target[0], ch->fvolume_target[1],
						ch->soundsource,
						(int)ch->origin[0],
						(int)ch->origin[1],
						(int)ch->origin[2],
						timeleft,
						bLooping,
						ch->sfx->getname(nameBuf, sizeof(nameBuf)));
				}
				else if ( sndsurround < 4 )
				{
					Con_NXPrintf( &np, "%s %02i l(%.02f) r(%.02f) vol(%03d) ent(%03d) pos(%6d %6d %6d) timeleft(%f) looped(%d) %50s",
						nSoundEntryName,
						total + 1,
						ch->fvolume[ IFRONT_LEFT ],
						ch->fvolume[ IFRONT_RIGHT ],
						ch->master_vol,
						ch->soundsource,
						( int )ch->origin[ 0 ],
						( int )ch->origin[ 1 ],
						( int )ch->origin[ 2 ],
						timeleft,
						bLooping,
						ch->sfx->getname( nameBuf, sizeof( nameBuf ) ) );
				}
				else
				{
					Con_NXPrintf( &np, "%s %02i l(%.02f) c(%.02f) r(%.02f) rl(%.02f) rr(%.02f) vol(%03d) ent(%03d) pos(%6d %6d %6d) timeleft(%f) looped(%d) %50s",
						nSoundEntryName,
						total + 1,
						ch->fvolume[ IFRONT_LEFT ], 
						ch->fvolume[ IFRONT_CENTER ],
						ch->fvolume[ IFRONT_RIGHT ],
						ch->fvolume[ IREAR_LEFT ],
						ch->fvolume[ IREAR_RIGHT ],
						ch->master_vol,
						ch->soundsource,
						( int )ch->origin[ 0 ],
						( int )ch->origin[ 1 ],
						( int )ch->origin[ 2 ],
						timeleft,
						bLooping,
						ch->sfx->getname( nameBuf, sizeof( nameBuf ) ) );
				}
			}

			if ( snd_visualize.GetInt() )
			{
				CDebugOverlay::AddTextOverlay( ch->origin, 0.05f, ch->sfx->getname(nameBuf, sizeof(nameBuf)) );
			}
#ifndef DEDICATED
			if ( snd_show_print.GetInt() )
			{
				bool bPrint = false;
				// did we print this sound last frame? If we didn't, then print it now
				int nFind = s_SoundsPrintedLastUpdate.Find( ch->m_nSoundScriptHash );
				if ( nFind == s_SoundsPrintedLastUpdate.InvalidHandle() )
				{
					bPrint = true;
					s_SoundsPrintedLastUpdate.Insert( ch->m_nSoundScriptHash, s_nPrintTickCount );
				}
				else
				{
					int &nLastPlayed = s_SoundsPrintedLastUpdate.Element( nFind );
					if ( uint( s_nPrintTickCount - nLastPlayed ) > uint( snd_show_print.GetInt() ) )
					{
						bPrint = true;
					}
					nLastPlayed = s_nPrintTickCount; 
				}

				if ( bPrint )
				{
					if (ch->wavtype == CHAR_HRTF)
					{
						const float hdist = sqrt(ch->hrtf.vec.x*ch->hrtf.vec.x + ch->hrtf.vec.z*ch->hrtf.vec.z);
						const float yaw = -VEC_RAD2DEG(atan2(-ch->hrtf.vec.x, -ch->hrtf.vec.z));
						const float pitch = VEC_RAD2DEG(atan2(ch->hrtf.vec.y, hdist));

						Msg( "%32s hrtf lerp(%.2f) yaw(%.2f) pitch(%.2f) %02i vol(%03d) ent(%03d) pos(%6d %6d %6d) timeleft(%f) looped(%d) %50s\n",
							nSoundEntryName,
							ch->hrtf.lerp,
							yaw,
							pitch,
							total + 1,
							ch->master_vol,
							ch->soundsource,
							( int )ch->origin[ 0 ],
							( int )ch->origin[ 1 ],
							( int )ch->origin[ 2 ],
							timeleft,
							bLooping,
							ch->sfx->getname( nameBuf, sizeof( nameBuf ) ) );
					}
					else
					if ( sndsurround < 4 )
					{
						Msg( "%s %02i l(%03d) r(%03d) vol(%03d) ent(%03d) pos(%6d %6d %6d) timeleft(%f) looped(%d) %50s\n",
							nSoundEntryName,
							total + 1,
							( int )ch->fvolume[ IFRONT_LEFT ],
							( int )ch->fvolume[ IFRONT_RIGHT ],
							ch->master_vol,
							ch->soundsource,
							( int )ch->origin[ 0 ],
							( int )ch->origin[ 1 ],
							( int )ch->origin[ 2 ],
							timeleft,
							bLooping,
							ch->sfx->getname( nameBuf, sizeof( nameBuf ) ) );
					}
					else
					{
						Msg( "%s %02i l(%03d) c(%03d) r(%03d) rl(%03d) rr(%03d) vol(%03d) ent(%03d) pos(%6d %6d %6d) timeleft(%f) looped(%d) %50s\n",
							nSoundEntryName,
							total + 1,
							( int )ch->fvolume[ IFRONT_LEFT ],
							( int )ch->fvolume[ IFRONT_CENTER ],
							( int )ch->fvolume[ IFRONT_RIGHT ],
							( int )ch->fvolume[ IREAR_LEFT ],
							( int )ch->fvolume[ IREAR_RIGHT ],
							ch->master_vol,
							ch->soundsource,
							( int )ch->origin[ 0 ],
							( int )ch->origin[ 1 ],
							( int )ch->origin[ 2 ],
							timeleft,
							bLooping,
							ch->sfx->getname( nameBuf, sizeof( nameBuf ) ) );
					}
				}
			}
#endif

			++total;
		}

		while ( total <= 128 )
		{
			Con_NPrintf( total + 2, "" );
			total++;
		}
	}

	g_SndMutex.Unlock();

	if ( s_bOnLoadScreen )
		return;

	// not time to update yet?
	double tNow = Plat_FloatTime();

	// this is the last time we ran a sound frame
	g_LastSoundFrame = tNow;
	// this is the last time we did mixing (extraupdate also advances this if it mixes)
	g_LastMixTime = tNow;
	// mix some sound
	// try to stay at least one frame + mixahead ahead in the mix.
	g_EstFrameTime = (g_EstFrameTime * 0.9f) + (g_pSoundServices->GetHostFrametime() * 0.1f);
	S_Update_( g_EstFrameTime + snd_mixahead.GetFloat() );
}


void S_DumpClientSounds( )
{
	con_nprint_t np;
	np.time_to_live = 2.0f;
	np.fixed_width_font = true;

	int total = 0;
	char nameBuf[MAX_PATH];

	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );
	for ( int i = 0; i < list.Count(); i++ )
	{
		channel_t *ch = list.GetChannel(i);
		if ( !ch->sfx )
			continue;

		unsigned int sampleCount = RemainingSamples( ch );
		float timeleft = (float)sampleCount / (float)ch->sfx->pSource->SampleRate();
		bool bLooping = ch->sfx->pSource->IsLooped();
		const char *pszclassname = GetClientClassname(ch->soundsource);

		Msg( "%02i %s l(%03d) c(%03d) r(%03d) rl(%03d) rr(%03d) vol(%03d) pos(%6d %6d %6d) timeleft(%f) looped(%d) %50s chan:%d ent(%03d):%s\n", 
			total+ 1, 
			ch->flags.fromserver ? "SERVER" : "CLIENT",
			(int)ch->fvolume[IFRONT_LEFT], 
			(int)ch->fvolume[IFRONT_CENTER],
			(int)ch->fvolume[IFRONT_RIGHT], 
			(int)ch->fvolume[IREAR_LEFT], 
			(int)ch->fvolume[IREAR_RIGHT], 
			ch->master_vol,
			(int)ch->origin[0],
			(int)ch->origin[1],
			(int)ch->origin[2],
			timeleft,
			bLooping, 
			ch->sfx->getname(nameBuf, sizeof(nameBuf)),
			ch->entchannel,
			ch->soundsource,
			pszclassname ? pszclassname : "NULL" );

		total++;
	}
}
CON_COMMAND( snd_dumpclientsounds, "Dump sounds to console" )
{
	S_DumpClientSounds();
// 	con_nprint_t np;
// 	np.time_to_live = 2.0f;
// 	np.fixed_width_font = true;
// 
// 	int total = 0;
// 	char nameBuf[MAX_PATH];
// 
// 	CChannelList list;
// 	g_ActiveChannels.GetActiveChannels( list );
// 	for ( int i = 0; i < list.Count(); i++ )
// 	{
// 		channel_t *ch = list.GetChannel(i);
// 		if ( !ch->sfx )
// 			continue;
// 
// 		unsigned int sampleCount = RemainingSamples( ch );
// 		float timeleft = (float)sampleCount / (float)ch->sfx->pSource->SampleRate();
// 		bool bLooping = ch->sfx->pSource->IsLooped();
// 		const char *pszclassname = GetClientClassname(ch->soundsource);
// 
// 		Msg( "%02i %s l(%03d) c(%03d) r(%03d) rl(%03d) rr(%03d) vol(%03d) pos(%6d %6d %6d) timeleft(%f) looped(%d) %50s chan:%d ent(%03d):%s\n", 
// 			total+ 1, 
// 			ch->flags.fromserver ? "SERVER" : "CLIENT",
// 			(int)ch->fvolume[IFRONT_LEFT], 
// 			(int)ch->fvolume[IFRONT_CENTER],
// 			(int)ch->fvolume[IFRONT_RIGHT], 
// 			(int)ch->fvolume[IREAR_LEFT], 
// 			(int)ch->fvolume[IREAR_RIGHT], 
// 			ch->master_vol,
// 			(int)ch->origin[0],
// 			(int)ch->origin[1],
// 			(int)ch->origin[2],
// 			timeleft,
// 			bLooping, 
// 			ch->sfx->getname(nameBuf, sizeof(nameBuf)),
// 			ch->entchannel,
// 			ch->soundsource,
// 			pszclassname ? pszclassname : "NULL" );
// 
// 		total++;
// 	}
}

ConVar snd_show_channel_count( "snd_show_channel_count", "0", FCVAR_NONE, "Show the current count of channel types." );
//-----------------------------------------------------------------------------
// Set g_soundtime to number of full samples that have been transfered out to hardware
// since start.
//-----------------------------------------------------------------------------

void DEBUG_ShowChannelCount( void )
{
	if (snd_show_channel_count.GetInt() == 0)
		return;

	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );
	int nStaticNum = 0;
	int nDynamicNum = 0;

	for ( int i = 0; i < list.Count(); i++ )
	{
		int ch_idx = list.GetChannelIndex(i);
		if( ch_idx < MAX_DYNAMIC_CHANNELS )
		{
			nDynamicNum++;
		}
		else
		{
			nStaticNum++;
		}
	}
	if( nDynamicNum >  nShowDynamicChannelMax )
	{
		nShowDynamicChannelMax = nDynamicNum;
	}
	if( nStaticNum >  nShowStaticChannelMax )
	{
		nShowStaticChannelMax = nStaticNum;
	}

	int r, g, b, a;
	r = g = b = 200;
	a = 255;
	if( nStaticNum > MAX_CHANNELS - MAX_DYNAMIC_CHANNELS - 10 )
	{
		r = 255;
	}
	char chanStr[128];
	sprintf( chanStr, "STATIC CHANNEL COUNT: %i : %i", nStaticNum, nShowStaticChannelMax );
	CDebugOverlay::AddScreenTextOverlay( 0.01, 0.4, 0.01, r, g, b, a, chanStr );

	if( nDynamicNum > MAX_DYNAMIC_CHANNELS - 10 )
	{
		r = 255;
	}
	else
	{
		r = 200;
	}

	sprintf( chanStr, "DYNAMIC CHANNEL COUNT: %i : %i", nDynamicNum, nShowDynamicChannelMax );
	CDebugOverlay::AddScreenTextOverlay( 0.01, 0.45, 0.01, r, g, b, a, chanStr );

	if( nStaticNum >= MAX_CHANNELS - MAX_DYNAMIC_CHANNELS || nDynamicNum >= MAX_DYNAMIC_CHANNELS )
	{
		S_DumpClientSounds();
	}

}

#if USE_AUDIO_DEVICE_V1

//-----------------------------------------------------------------------------
// Set g_soundtime to number of full samples that have been transfered out to hardware
// since start.
//-----------------------------------------------------------------------------
void GetSoundTime(void)
{
	// Make them 64 bits so calculation is done in 64 bits.
	int64	fullsamples;
	int64	sampleOutCount;

	// size of output buffer in *full* 16 bit samples
	// A 2 channel device has a *full* sample consisting of a 16 bit LR pair.
	// A 1 channel device has a *full* sample consiting of a 16 bit single sample.
	fullsamples = g_AudioDevice->DeviceSampleCount() / g_AudioDevice->ChannelCount();

	// NOTE: it is possible to miscount buffers if it has wrapped twice between
	// calls to S_Update.  However, since the output buffer size is > 1 second of sound, 
	// this should only occur for framerates lower than 1hz

	// sampleOutCount is counted in 16 bit *full* samples, of number of samples output to hardware
	// for current output buffer
	sampleOutCount = g_AudioDevice->GetOutputPosition();
	if ( sampleOutCount < s_oldsampleOutCount )
	{
		// buffer wrapped
		s_buffers++;
	}

	s_oldsampleOutCount = sampleOutCount;

	if ( cl_movieinfo.IsRecording() )
	{
		// in movie, just mix one frame worth of sound
		float t = g_pSoundServices->GetHostTime();
		if ( s_lastsoundtime != t )
		{
			double flSamples = (double)g_pSoundServices->GetHostFrametime() * (double)g_AudioDevice->SampleRate();
			int nSamples = (int)flSamples;
			double flSampleError = flSamples - (double)nSamples;
			g_soundtimeerror += flSampleError;
			if ( fabs( g_soundtimeerror ) > 1.0 )
			{
				 int nErrorSamples = (int)g_soundtimeerror;
				 g_soundtimeerror -= (double)nErrorSamples;
				 nSamples += nErrorSamples;
			}
			 
			g_soundtime += nSamples;
			s_lastsoundtime = t;
		}
	}
	else
	{
		// g_soundtime indicates how many *full* samples have actually been
		// played out to dma
		g_soundtime = s_buffers*fullsamples + sampleOutCount;
	}
}
#endif

void S_ExtraUpdate( void )
{
	if ( IsGameConsole() )
		return;

	if ( !g_AudioDevice || !g_pSoundServices )
		return;

	if ( !g_AudioDevice->IsActive() )
		return;
	
	if ( s_bOnLoadScreen )
		return;

	if ( snd_noextraupdate.GetInt() || cl_movieinfo.IsRecording() )
		return;		// don't pollute timings

	// If listener position and orientation has not yet been updated (ie: no call to S_Update since level load)
	// then don't mix.  Important - mixing with listener at 'false' origin causes
	// some sounds to incorrectly spatialize to 0 volume, killing them before they can play.
	if ( !SND_IsListenerValid() )
		return;
	VPROF_BUDGET( "CEngineClient::Sound_ExtraUpdate()", VPROF_BUDGETGROUP_OTHER_SOUND );

	// Only mix if you have used up 90% of the mixahead buffer
	double tNow = Plat_FloatTime();
	float delta = (tNow - g_LastMixTime);
	// we know we were at least snd_mixahead seconds ahead of the output the last time we did mixing
	// if we're not close to running out just exit to avoid small mix batches
	if ( delta > 0 && delta < (snd_mixahead.GetFloat() * 0.9f) )
		return;
	g_LastMixTime = tNow;

	g_pSoundServices->OnExtraUpdate();

	// Shouldn't have to do any work here if your framerate hasn't dropped
	S_Update_( snd_mixahead.GetFloat() );
}

extern void DEBUG_StartSoundMeasure(int type, int samplecount );
extern void DEBUG_StopSoundMeasure(int type, int samplecount );

void S_Update_Guts( float mixAheadTime )
{
	VPROF( "S_Update_Guts" );

	DEBUG_StartSoundMeasure(4, 0);
#if USE_AUDIO_DEVICE_V1
	// Update our perception of audio time.
	// 'g_soundtime' tells how many samples have
	// been played out of the dma buffer since sound system startup.
	// 'g_paintedtime' indicates how many samples we've actually mixed
	// and sent to the dma buffer since sound system startup.
	GetSoundTime();

//	if ( g_soundtime > g_paintedtime )
//	{
//		// if soundtime > paintedtime, then the dma buffer
//		// has played out more sound than we've actually
//		// mixed.  We need to call S_Update_ more often.
//
//		DevMsg ("S_Update_ : Underflow\n"); 
//		paintedtime = g_soundtime;		
//	}
//	(kdb) above code doesn't handle underflow correctly 
//	should actually zero out the paintbuffer to advance to the new
//	time.

	// mix ahead of current position
	int64 endtime = g_AudioDevice->PaintBegin( mixAheadTime, g_soundtime, g_paintedtime );

	int samples = endtime - g_paintedtime;
	samples = samples < 0 ? 0 : samples;
	if ( samples )
	{
		THREAD_LOCK_SOUND();

		DEBUG_StartSoundMeasure( 2, samples );

		MIX_PaintChannels( endtime, s_bIsListenerUnderwater );

		MXR_DebugShowMixVolumes();

		MXR_UpdateAllDuckerVolumes();

		DEBUG_ShowChannelCount( );

		DEBUG_StopSoundMeasure( 2, 0 );

	}
	g_AudioDevice->PaintEnd();
	DEBUG_StopSoundMeasure( 4, samples );
#else
	THREAD_LOCK_SOUND();
	uint nTotal = 0;
	// compute how much audio time is queued up waiting for output
	int nQueuedSamples = g_AudioDevice->QueuedBufferCount() * MIX_BUFFER_SIZE;
	float flQueuedTime = nQueuedSamples * SECONDS_PER_SAMPLE;

	// we want to stay "mixAheadTime" ahead of the audio buffer, how much additional audio do we need to mix?
	float flNeededTime = mixAheadTime - flQueuedTime;
	if ( flNeededTime > 0 )
	{
		// round up to the number of buffers needed to mix
		int nAvailBuffers = g_AudioDevice->EmptyBufferCount();
		int nMixBuffers = 1 + ( flNeededTime / (MIX_BUFFER_SIZE * SECONDS_PER_SAMPLE) );
		
		// clamp to available buffers
		nMixBuffers = Min( nMixBuffers, nAvailBuffers );
		// now mix & output each buffer
		for ( int i = 0; i < nMixBuffers; i++ )
		{
			uint nSamples = MIX_BUFFER_SIZE;
			int nEndTime = g_paintedtime + nSamples;
			// handle wraparound
			if ( nEndTime < g_paintedtime )
			{
				g_paintedtime = 0;
				nEndTime = nSamples;
			}
			nTotal += nSamples;
			DEBUG_StartSoundMeasure( 2, nSamples );

			MIX_PaintChannels( nEndTime, s_bIsListenerUnderwater );

			MXR_DebugShowMixVolumes();

			MXR_UpdateAllDuckerVolumes();

			DEBUG_StopSoundMeasure( 2, 0 );
		}
	}

	DEBUG_StopSoundMeasure( 4, nTotal );
#endif

}

#if !defined( _X360 )
#define THREADED_MIX_TIME 33
#else
#define THREADED_MIX_TIME XMA_POLL_RATE
#endif

ConVar snd_ShowThreadFrameTime( "snd_ShowThreadFrameTime", "0" );

bool g_bMixThreadExit;
ThreadHandle_t g_hMixThread;
void S_Update_Thread()
{
	float frameTime = THREADED_MIX_TIME * 0.001f;
	double lastFrameTime = Plat_FloatTime();

	while ( !g_bMixThreadExit )
	{
		// mixing (for 360) needs to be updated at a steady rate
		// large update times causes the mixer to demand more audio data
		// the 360 decoder has finite latency and cannot fulfill spike requests
		double t0 = Plat_FloatTime();
		S_Update_Guts( frameTime + snd_mixahead.GetFloat() );
		int updateTime = ( Plat_FloatTime() - t0 ) * 1000.0f;

		// try to maintain a steadier rate by compensating for fluctuating mix times
		int sleepTime = THREADED_MIX_TIME - updateTime;
		if ( sleepTime > 0 )
		{
			ThreadSleep( sleepTime );
		}

		// mimic a frametime needed for sound update
		double t1 = Plat_FloatTime();
		frameTime = t1 - lastFrameTime;
		lastFrameTime = t1;

		if ( snd_ShowThreadFrameTime.GetBool() )
		{
			Msg( "S_Update_Thread: frameTime: %d ms\n", (int)( frameTime * 1000.0f ) );
		}
	}
}

void S_ShutdownMixThread()
{
	if ( g_hMixThread )
	{
		g_bMixThreadExit = true;
		ThreadJoin( g_hMixThread );
		ReleaseThreadHandle( g_hMixThread );
		g_hMixThread = NULL;
	}
}

void StartPhononThread();

void S_Update_( float mixAheadTime )
{
	if (snd_use_hrtf.GetBool())
	{
		StartPhononThread();
	}

	if ( !snd_mix_async.GetBool() )
	{
		S_ShutdownMixThread();
		S_Update_Guts( mixAheadTime );
	}
	else
	{
		if ( !g_hMixThread )
		{
			g_bMixThreadExit = false;
			g_hMixThread = ThreadExecuteSolo( "SndMix", S_Update_Thread );
			if ( IsX360() )
			{
				ThreadSetAffinity( g_hMixThread, XBOX_PROCESSOR_5 );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Threaded mixing enable. Purposely hiding enable/disable details.
//-----------------------------------------------------------------------------
void S_EnableThreadedMixing( bool bEnable )
{
	if ( snd_mix_async.GetBool() != bEnable )
	{
		snd_mix_async.SetValue( bEnable );
	}
}

/*
===============================================================================

console functions

===============================================================================
*/
extern void DSP_DEBUGSetParams(int ipreset, int iproc, float *pvalues, int cparams);
extern void DSP_DEBUGReloadPresetFile( void );

void S_DspParms( const CCommand &args )
{
	if ( args.ArgC() == 1)
	{
		// if dsp_parms with no arguments, reload entire preset file

		 DSP_DEBUGReloadPresetFile();

		 return;
	}

	if ( args.ArgC() < 4 )
	{
		Msg( "Usage: dsp_parms PRESET# PROC# param0 param1 ...up to param15 \n" );
		return;
	}
	
	int cparam = MIN( args.ArgC() - 4, 16);

	float params[16];
	Q_memset( params, 0, sizeof(float) * 16 );

	// get preset & proc
	int idsp, iproc;
	idsp = Q_atof( args[1] );
	iproc = Q_atof( args[2] );

	// get params
	for (int i = 0; i < cparam; i++)
	{
		params[i] = Q_atof( args[i+4] );
	}

	// set up params & switch preset
	DSP_DEBUGSetParams(idsp, iproc, params, cparam);
}

static ConCommand dsp_parm("dsp_reload", S_DspParms, "", FCVAR_CHEAT );

void S_Play( const char *pszName, bool flush = false )
{
	int			inCache;
	char		szName[256];
	CSfxTable	*pSfx;
	
	Q_strncpy( szName, pszName, sizeof( szName ) );
	if ( !Q_strrchr( pszName, '.' ) )
	{
		Q_strncat( szName, ".wav", sizeof( szName ), COPY_ALL_CHARACTERS );
	}

	pSfx = S_FindName( szName, &inCache );
	if ( inCache && flush )
	{
		pSfx->pSource->CacheUnload();
	}

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	StartSoundParams_t params;
	params.staticsound = false;
	params.soundsource = g_pSoundServices->GetViewEntity( nSlot );
	params.entchannel = CHAN_REPLACE;
	params.pSfx = pSfx;
	params.origin = listener_origin[ nSlot ];
	params.fvol = 1.0f;
	params.soundlevel = SNDLVL_NONE;
	params.flags = 0;
	params.pitch = PITCH_NORM;

	S_StartSound( params );
}

static void S_Play( const CCommand &args )
{
	bool bFlush = !Q_stricmp( args[0], "playflush" );
	for ( int i = 1; i < args.ArgC(); ++i )
	{
		S_Play( args[i], bFlush );
	}
}

static void S_PlayHRTF(const CCommand& args)
{
	if (args.ArgC() != 5)
	{
		DevMsg("Usage: play_hrtf sound x y z\n");
		return;
	}

	char nameBuf[4096];
	::Q_snprintf(nameBuf, sizeof(nameBuf), "~%s", args[1]);

	const char* pszName = nameBuf;
	Vector origin;
	origin[0] = Q_atof(args[2]);
	origin[1] = Q_atof(args[3]);
	origin[2] = Q_atof(args[4]);

	int			inCache;
	char		szName[256];
	CSfxTable	*pSfx;

	Q_strncpy(szName, pszName, sizeof(szName));
	if (!Q_strrchr(pszName, '.'))
	{
		Q_strncat(szName, ".wav", sizeof(szName), COPY_ALL_CHARACTERS);
	}

	pSfx = S_FindName(szName, &inCache);

	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	StartSoundParams_t params;
	params.staticsound = false;
	params.soundsource = g_pSoundServices->GetViewEntity(nSlot);
	params.entchannel = CHAN_REPLACE;
	params.pSfx = pSfx;
	params.origin = origin;
	params.fvol = 1.0f;
	params.soundlevel = SNDLVL_NONE;
	params.flags = 0;
	params.pitch = PITCH_NORM;
	params.m_bHRTFLock = true;
	params.m_bInEyeSound = false;

	S_StartSound(params);
}

static void S_PlayVol( const CCommand &args )
{
	static int hash=543;
	float vol;
	char name[256];
	CSfxTable *pSfx;
	
	for ( int i = 1; i<args.ArgC(); i += 2 )
	{
		if ( !Q_strrchr( args[i], '.') )
		{
			Q_strncpy( name, args[i], sizeof( name ) );
			Q_strncat( name, ".wav", sizeof( name ), COPY_ALL_CHARACTERS );
		}
		else
		{
			Q_strncpy( name, args[i], sizeof( name ) );
		}

		pSfx = S_PrecacheSound( name );
		vol = Q_atof( args[i+1] );

		int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

		StartSoundParams_t params;
		params.staticsound = false;
		params.soundsource = hash++;
		params.entchannel = CHAN_AUTO;
		params.pSfx = pSfx;
		params.origin = listener_origin[ nSlot ];
		params.fvol = vol;
		params.soundlevel = SNDLVL_NONE;
		params.flags = 0;
		params.pitch = PITCH_NORM;

		S_StartSound( params );
	}
}

static void S_PlayDelay( const CCommand &args )
{
	if ( args.ArgC() != 3 )
	{
		Msg( "Usage:  playdelay delay_in_msec (negative to skip ahead) soundname\n" );
		return;
	}

	char szName[256];
	CSfxTable *pSfx;

	float delay = Q_atof( args[ 1 ] );
	
	Q_strncpy(szName, args[ 2 ], sizeof( szName ) );
	if ( !Q_strrchr( args[ 2 ], '.' ) )
	{
		Q_strncat( szName, ".wav", sizeof( szName ), COPY_ALL_CHARACTERS );
	}

	pSfx = S_FindName( szName, NULL );
	
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

	StartSoundParams_t params;
	params.staticsound = false;
	params.soundsource = g_pSoundServices->GetViewEntity( nSlot );
	params.entchannel = CHAN_REPLACE;
	params.pSfx = pSfx;
	params.origin = listener_origin[ nSlot ];
	params.fvol = 1.0f;
	params.soundlevel = SNDLVL_NONE;
	params.flags = 0;
	params.pitch = PITCH_NORM;
	params.delay = delay;

	S_StartSound( params );

}
static ConCommand sndplaydelay( "sndplaydelay", S_PlayDelay );


#if defined( _GAMECONSOLE )
void S_UnloadSound( const char *pName )
{
	CSfxTable *pSfx = S_FindName( pName, NULL );
	if ( pSfx && pSfx->pSource )
	{
		pSfx->pSource->CacheUnload();
		delete pSfx->pSource;
		pSfx->pSource = NULL;
	}
}
#endif

void S_PurgeSoundsDueToLanguageChange()
{
	DevMsg( "S_PurgeSoundsDueToLanguageChange()\n" );

	for ( int i = s_Sounds.FirstInorder(); i != s_Sounds.InvalidIndex(); i = s_Sounds.NextInorder( i ) )
	{
		CSfxTable *pSfx = s_Sounds[i].pSfx;
		if ( pSfx && pSfx->pSource )
		{
			if ( pSfx->m_bIsUISound || pSfx->m_bIsMusic )
				continue;

			// will skip past any prefix chars
			char filename[MAX_PATH];
			const char *pFilename = pSfx->GetFileName( filename, sizeof( filename ) );
			if ( !pFilename )
				continue;

			V_FixSlashes( filename, '/' );

			if ( StringHasPrefix( pFilename, "ui/" ) || StringHasPrefix( pFilename, "common/" ) || StringHasPrefix( pFilename, "music/" ) )
			{
				continue;
			}

			pSfx->pSource->CacheUnload();
			delete pSfx->pSource;
			pSfx->pSource = NULL;
		}
	}
}

static bool SortByNameLessFunc( const int &lhs, const int &rhs )
{
	CSfxTable *pSfx1 = s_Sounds[lhs].pSfx;
	CSfxTable *pSfx2 = s_Sounds[rhs].pSfx;
	char nameBuf1[MAX_PATH];
	char nameBuf2[MAX_PATH];

	return CaselessStringLessThan( pSfx1->getname(nameBuf1,sizeof(nameBuf1)), pSfx2->getname(nameBuf2,sizeof(nameBuf2)) );
}

void S_SoundList(void)
{
	CSfxTable		*sfx;
	CAudioSource	*pSource;
	int				size, total;
	char			nameBuf[MAX_PATH];

	total = 0;
	for ( int i = s_Sounds.FirstInorder(); i != s_Sounds.InvalidIndex(); i = s_Sounds.NextInorder( i ) )
	{
		sfx = s_Sounds[i].pSfx;
		pSource = sfx->pSource;
		if ( !pSource )
			continue;

		size = pSource->SampleSize() * pSource->SampleCount();
		total += size;

		if ( pSource->IsLooped() )
		{
			Msg( "L" );
		}
		else
		{
			Msg( " " );
		}
		Msg( "(%2db) %6i : %s\n", pSource->SampleSize(),  size, sfx->getname(nameBuf,sizeof(nameBuf)));
	}

	Msg( "Total: %.2f MB\n", (float)total/(1024.0f * 1024.0f) );
}

#if defined( _X360 ) || defined( _PS3 )
CON_COMMAND( vx_soundlist, "Dump sounds to VXConsole" )
{
	CSfxTable		*sfx;
	CAudioSource	*pSource;
	int				dataSize;
	char			*pFormatStr;
	int				sampleRate;
	int				sampleBits;
	int				streamed;
	int				looped;
	int				channels;
	int				numSamples;
	int				quality;

	int numSounds = s_Sounds.Count();
	xSoundList_t* pSoundList = new xSoundList_t[numSounds];

	int i = 0;
	char nameBuf[MAX_PATH];
	for ( int iSrcSound=s_Sounds.FirstInorder(); iSrcSound != s_Sounds.InvalidIndex(); iSrcSound = s_Sounds.NextInorder( iSrcSound ) )
	{
		dataSize = -1;
		sampleRate = -1;
		sampleBits = -1;
		pFormatStr = "???";
		streamed = -1;
		looped = -1;
		channels = -1;
		numSamples = -1;
		quality = -1;

		sfx = s_Sounds[iSrcSound].pSfx;
		pSource = sfx->pSource;
		if ( pSource && pSource->IsCached() )
		{
			numSamples = pSource->SampleCount();
			dataSize = pSource->DataSize();
			sampleRate = pSource->SampleRate();
			streamed = pSource->IsStreaming();
			looped = pSource->IsLooped();
			channels = pSource->IsStereoWav() ? 2 : 1;
			quality = pSource->GetQuality();

			switch ( pSource->Format() )
			{
			case WAVE_FORMAT_ADPCM:
				pFormatStr = "ADPCM";
				sampleBits = 16;
				break;
			case WAVE_FORMAT_PCM:
				pFormatStr = "PCM";
				sampleBits = (pSource->SampleSize() * 8)/channels;
				break;
			case WAVE_FORMAT_XMA:
				pFormatStr = "XMA";
				sampleBits = 16;
				break;
			case WAVE_FORMAT_MP3:
			case WAVE_FORMAT_TEMP:
				pFormatStr = "MP3";
				sampleBits = 16;
				break;
			default:
				pFormatStr = "Unknown";
				sampleBits = 16;
				break;
			}
		}

		V_strncpy( pSoundList[i].name, sfx->getname(nameBuf, sizeof(nameBuf)), sizeof( pSoundList[i].name ) );
		V_strncpy( pSoundList[i].formatName, pFormatStr, sizeof( pSoundList[i].formatName ) );
		pSoundList[i].rate = sampleRate;
		pSoundList[i].bits = sampleBits;
		pSoundList[i].channels = channels;
		pSoundList[i].looped = looped;
		pSoundList[i].dataSize = dataSize;
		pSoundList[i].numSamples = numSamples;
		pSoundList[i].streamed = streamed;
		pSoundList[i].quality = quality;
		++i;
	}

	XBX_rSoundList( numSounds, pSoundList );
	delete [] pSoundList;
}
#endif

extern unsigned g_snd_time_debug;
extern unsigned g_snd_call_time_debug;
extern unsigned g_snd_count_debug;
extern unsigned g_snd_samplecount;
extern unsigned g_snd_frametime;
extern unsigned g_snd_frametime_total;
extern int g_snd_profile_type;

// start measuring sound perf, 100 reps
// type 1 - dsp, 2 - mix, 3 - load sound, 4 - all sound
// set type via ConVar snd_profile

void DEBUG_StartSoundMeasure(int type, int samplecount )
{
	if (type != g_snd_profile_type)
		return;

	if (samplecount)
		g_snd_samplecount += samplecount;

	g_snd_call_time_debug = Plat_MSTime();
}

// show sound measurement after 25 reps - show as % of total frame
// type 1 - dsp, 2 - mix, 3 - load sound, 4 - all sound

// BUGBUG: snd_profile 4 reports a lower average because it's average cost
// PER CALL and most calls (via SoundExtraUpdate()) don't do any work and 
// bring the average down.  If you want an average PER FRAME instead, it's generally higher.
void DEBUG_StopSoundMeasure(int type, int samplecount )
{
	if (type != g_snd_profile_type)
		return;

	if (samplecount)
		g_snd_samplecount += samplecount;

	// add total time since last frame

	g_snd_frametime_total += Plat_MSTime() - g_snd_frametime;

	// performance timing

	g_snd_time_debug += Plat_MSTime() - g_snd_call_time_debug;

	if (++g_snd_count_debug >= 100)
	{
		switch (g_snd_profile_type)
		{
		case 1: 
			Msg("dsp: (%2.2f) millisec   ", ((float)g_snd_time_debug) / 100.0); 
			Msg("(%2.2f) pct of frame \n", 100.0 * ((float)g_snd_time_debug) / ((float)g_snd_frametime_total)); 
			break;
		case 2: 
			Msg("mix+dsp:(%2.2f) millisec   ", ((float)g_snd_time_debug) / 100.0);
			Msg("(%2.2f) pct of frame \n", 100.0 * ((float)g_snd_time_debug) / ((float)g_snd_frametime_total)); 
			break;
		case 3: 
			//if ( (((float)g_snd_time_debug) / 100.0) < 0.01 )
			//	break;
			Msg("snd load: (%2.2f) millisec   ", ((float)g_snd_time_debug) / 100.0); 
			Msg("(%2.2f) pct of frame \n", 100.0 * ((float)g_snd_time_debug) / ((float)g_snd_frametime_total)); 
			break;
		case 4: 
			Msg("sound: (%2.2f) millisec   ", ((float)g_snd_time_debug) / 100.0); 
			Msg("(%2.2f) pct of frame (%d samples) \n", 100.0 * ((float)g_snd_time_debug) / ((float)g_snd_frametime_total), g_snd_samplecount); 
			break;
		}
		
		g_snd_count_debug = 0;
		g_snd_time_debug = 0;
		g_snd_samplecount = 0;	
		g_snd_frametime_total = 0;
	}

	g_snd_frametime = Plat_MSTime();
}

#ifndef LINUX
extern ConVar dsp_room;
#endif


// speak a sentence from console; works by passing in "!sentencename"
// or "sentence"
static void S_Say( const CCommand &args )
{
#ifndef LINUX

	CSfxTable *pSfx;

	if ( !g_AudioDevice->IsActive() )
		return;

	char sound[256];
	Q_strncpy( sound, args[1], sizeof( sound ) );		
	
	// DEBUG - test performance of dsp code
	if ( !Q_stricmp( sound, "dsp" ) )
	{
		unsigned time;
		int i;
		int count = 10000;
		int idsp; 

		for (i = 0; i < PAINTBUFFER_SIZE; i++)
		{
			g_paintbuffer[i].left = RandomInt(0,2999);
			g_paintbuffer[i].right = RandomInt(0,2999);
		}

		Msg ("Start profiling 10,000 calls to DSP\n");
		
		idsp = dsp_room.GetInt();
		
		// get system time

		time = Plat_MSTime();
		
		for (i = 0; i < count; i++)
		{
			// SX_RoomFX(PAINTBUFFER_SIZE, TRUE, TRUE);

			DSP_Process(idsp, g_paintbuffer, NULL, NULL, PAINTBUFFER_SIZE);

		}
		// display system time delta 
		Msg("%d milliseconds \n", Plat_MSTime() - time);
		return;
	} 
	
	if ( !Q_stricmp(sound, "paint") )
	{
		unsigned time;
		int count = 10000;
		static int hash=543;
		int64 psav = g_paintedtime;

		Msg ("Start profiling MIX_PaintChannels\n");
		
		pSfx = S_PrecacheSound("ambience/labdrone1.wav");

		int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
		StartSoundParams_t params;
		params.staticsound = false;
		params.soundsource = hash++;
		params.entchannel = CHAN_AUTO;
		params.pSfx = pSfx;
		params.origin = listener_origin[ nSlot ];
		params.fvol = 1.0f;
		params.soundlevel = SNDLVL_NONE;
		params.flags = 0;
		params.pitch = PITCH_NORM;

		S_StartSound( params );

		// get system time
		time = Plat_MSTime();

		// paint a boatload of sound

		MIX_PaintChannels( g_paintedtime + 512*count, s_bIsListenerUnderwater );		

		// display system time delta 
		Msg("%d milliseconds \n", Plat_MSTime() - time);
		g_paintedtime = psav;
		return;
	}

	// DEBUG
	if ( !TestSoundChar( sound, CHAR_SENTENCE ) )
	{
		// build a fake sentence name, then play the sentence text

		Q_strncpy(sound, "xxtestxx ", sizeof( sound ) );
		Q_strncat(sound, args[1], sizeof( sound ), COPY_ALL_CHARACTERS );

		int addIndex = g_Sentences.AddToTail();
		sentence_t *pSentence = &g_Sentences[addIndex];
		pSentence->pName = sound;
		pSentence->length = 0;

		// insert null terminator after sentence name
		sound[8] = 0;

		pSfx = S_PrecacheSound ("!xxtestxx");
		if (!pSfx)
		{
			Msg ("S_Say: can't cache %s\n", sound);
			return;
		}

		int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
		StartSoundParams_t params;
		params.staticsound = false;
		params.soundsource = g_pSoundServices->GetViewEntity( nSlot );
		params.entchannel = CHAN_REPLACE;
		params.pSfx = pSfx;
		params.origin = vec3_origin;
		params.fvol = 1.0f;
		params.soundlevel = SNDLVL_NONE;
		params.flags = 0;
		params.pitch = PITCH_NORM;

		S_StartSound ( params );
		
		// remove last
		g_Sentences.Remove( g_Sentences.Count() - 1 );
	}
	else
	{
		pSfx = S_FindName(sound, NULL);
		if (!pSfx)
		{
			Msg ("S_Say: can't find sentence name %s\n", sound);
			return;
		}

		int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
		StartSoundParams_t params;
		params.staticsound = false;
		params.soundsource = g_pSoundServices->GetViewEntity( nSlot );
		params.entchannel = CHAN_REPLACE;
		params.pSfx = pSfx;
		params.origin = vec3_origin;
		params.fvol = 1.0f;
		params.soundlevel = SNDLVL_NONE;
		params.flags = 0;
		params.pitch = PITCH_NORM;

		S_StartSound( params );
	}
#endif // LINUX
}


float S_GetMono16Samples( const char *pszName, CUtlVector< short >& sampleList )
{
	CSfxTable *pSfx = S_PrecacheSound( PSkipSoundChars( pszName ) );
	if ( !pSfx )
		return 0.0f;

	CAudioSource *pWave = pSfx->pSource;
	if ( !pWave )
		return 0.0f;

	int nType = pWave->GetType();
	if ( nType != CAudioSource::AUDIO_SOURCE_WAV )
		return 0.0f;

	SoundError soundError;
	CAudioMixer *pMixer = pWave->CreateMixer( 0, 0, false, soundError, nullptr );
	if ( !pMixer )
		return 0.0f;

	float duration = AudioSource_GetSoundDuration( pSfx );

	// Determine start/stop positions
	int totalsamples = (int)( duration * pWave->SampleRate() );
	if ( totalsamples <= 0 )
		return 0;

	bool bStereo = pWave->IsStereoWav();
	int mix_sample_size = pMixer->GetMixSampleSize();
	int nNumChannels = bStereo ? 2 : 1;

	char *pData = NULL;

	int pos = 0;
	int remaining = totalsamples;
	while ( remaining > 0 )
	{
		int blockSize = MIN( remaining, 1000 );

		char copyBuf[AUDIOSOURCE_COPYBUF_SIZE];
		int copied = pWave->GetOutputData( (void **)&pData, pos, blockSize, copyBuf );
		if ( !copied )
		{
			break;
		}

		remaining -= copied;
		pos += copied;

		// Now get samples out of output data
		switch ( nNumChannels )
		{
		default:
		case 1:
			{
				for ( int i = 0; i < copied; ++i )
				{
					int offset = i * mix_sample_size;

					short sample = 0;
					if ( mix_sample_size == 1 )
					{
						char s = *( char * )( pData + offset );
						// Upscale it to fit into a short
						sample = s << 8;
					}
					else if ( mix_sample_size == 2 )
					{
						sample = *( short * )( pData + offset );
					}
					else if ( mix_sample_size == 4 )
					{
						// Not likely to have 4 bytes mono!!!
						Assert( 0 );

						int s = *( int * )( pData + offset );
						sample = s >> 16;
					}
					else
					{
						Assert( 0 );
					}

					sampleList.AddToTail( sample );
				}
			}
			break;

		case 2:
			{
				for ( int i = 0; i < copied; ++i )
				{
					int offset = i * mix_sample_size;

					short left = 0;
					short right = 0;
					
					if ( mix_sample_size == 1 )
					{
						// Not possible!!!, must be at least 2 bytes!!!
						Assert( 0 );

						char v = *( char * )( pData + offset );
						left = right = ( v << 8 );
					}
					else if ( mix_sample_size == 2 )
					{
						// One byte per channel
						left  = (short)( ( *(char *)( pData + offset ) ) << 8 );
						right = (short)( ( *(char *)( pData + offset + 1 ) ) << 8 );
					}
					else if ( mix_sample_size == 4 )
					{
						// 2 bytes per channel
						left = *( short * )( pData + offset );
						right = *( short * )( pData + offset + 2 );
					}
					else
					{
						Assert( 0 );
					}

					short sample = ( left + right ) >> 1;
					sampleList.AddToTail( sample );
				}
			}
			break;
		}
	}

	delete pMixer;

	return duration;
}

//-----------------------------------------------------------------------------
// Get left and right channel volume for a particular sound
//-----------------------------------------------------------------------------
bool S_GetSoundChannelVolume( const char* sound, float &flVolumeLeft, float &flVolumeRight )
{
	THREAD_LOCK_SOUND();
	
	char buf[MAX_PATH];
	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );
	for ( int i = 0; i < list.Count(); i++ )
	{
		channel_t* ch = list.GetChannel(i);
		Assert( ch->sfx );
		Assert( ch->activeIndex > 0 );

		ch->sfx->GetFileName( buf, MAX_PATH );
		Q_FixSlashes( buf, '/' );

		if ( !Q_stricmp( buf, sound ) )
		{
			flVolumeLeft = ch->fvolume[IFRONT_LEFT];
			flVolumeRight = ch->fvolume[IFRONT_RIGHT];
			return true;
		}
	}

	return false;
}

void S_SoundSetPitchScale( float flPitchScale )
{
	g_flPitchScale = flPitchScale;
}

float S_SoundGetPitchScale( void )
{
	return g_flPitchScale;
}

CON_COMMAND( snd_print_channel_by_index, "Prints the content of a channel from its index. snd_print_channel_by_index <index>." )
{
	if ( args.ArgC() != 2 )
	{
		Warning( "Incorrect usage of snd_print_channel_by_index. Pass the index from 0 to %d.\n", MAX_CHANNELS - 1 );
		return;
	}

	int nIndex = atoi( args.Arg( 1 ) );
	if ( ( nIndex < 0 ) || ( nIndex >= MAX_CHANNELS ) )
	{
		Warning( "Incorrect usage of snd_print_channel_by_index. Pass the index from 0 to %d.\n", MAX_CHANNELS - 1 );
		return;
	}

	AUTO_LOCK( g_SndMutex );
	channel_t *pChannel = &channels[ nIndex ];
	PrintChannel( "PrintChannel", pChannel );
}

CON_COMMAND( snd_print_channel_by_guid, "Prints the content of a channel from its guid. snd_print_channel_by_guid <guid>." )
{
	if ( args.ArgC() != 2 )
	{
		Warning( "Incorrect usage of snd_print_channel_by_guid. Pass the guid.\n" );
		return;
	}

	int nGuid = atoi( args.Arg( 1 ) );

	AUTO_LOCK( g_SndMutex );
	channel_t *pChannel = NULL;
	for ( int i = 0 ; i < MAX_CHANNELS ; ++i )
	{
		if ( channels[i].guid == nGuid )
		{
			pChannel = &channels[i];
			break;
		}
	}

	if ( pChannel == NULL )
	{
		Warning( "Could not find the channel with the guid: %d\n", nGuid );
		return;
	}
	PrintChannel( "PrintChannel", pChannel );
}

CON_COMMAND( snd_print_channels, "Prints all the active channel.")
{
	AUTO_LOCK( g_SndMutex );

	int nNumActiveChannels = g_ActiveChannels.GetActiveCount();
	Msg( "Total Channels: %d\n", nNumActiveChannels);

	CChannelList list;
	g_ActiveChannels.GetActiveChannels( list );
	for ( int i = 0; i < list.Count(); i++ )
	{
		channel_t *pChannel = list.GetChannel(i);
		if ( pChannel->sfx == NULL )
		{
			continue;
		}

		PrintChannelInfo( pChannel );
	}
}

CON_COMMAND( snd_set_master_volume, "Sets the master volume for a channel. snd_set_master_volume <guid> <mastervolume>." )
{
	if ( args.ArgC() != 3 )
	{
		Warning( "Incorrect usage of snd_set_master_volume. snd_set_master_volume <guid> <mastervolume>.\n" );
		return;
	}

	int nGuid = atoi( args.Arg( 1 ) );
	int nVolume = atoi( args.Arg( 2 ) );

	AUTO_LOCK( g_SndMutex );

	channel_t *pChannel = NULL;
	for ( int i = 0 ; i < MAX_CHANNELS ; ++i )
	{
		if ( channels[i].guid == nGuid )
		{
			pChannel = &channels[i];
			break;
		}
	}

	if ( pChannel == NULL )
	{
		Warning( "Could not find the channel with the guid: %d\n", nGuid );
		return;
	}

	// Do we have to do more than that?
	pChannel->master_vol = nVolume;
}
