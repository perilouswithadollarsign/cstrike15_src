//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "audio_pch.h"
#include "tier1/circularbuffer.h"
#include "voice.h"
#include "voice_wavefile.h"
#include "r_efx.h"
#include "cdll_int.h"
#include "voice_gain.h"
#include "voice_mixer_controls.h"
#include "snd_dma.h"
#include "ivoicerecord.h"
#include "ivoicecodec.h"
#include "filesystem.h"
#include "../../filesystem_engine.h"
#include "tier1/utlbuffer.h"
#include "../../cl_splitscreen.h"
#include "vgui_baseui_interface.h"
#include "demo.h"

extern IVEngineClient *engineClient;

#if defined( _X360 )
#include "xauddefs.h"
#endif

#include "steam/steam_api.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static CSteamAPIContext g_SteamAPIContext;
static CSteamAPIContext *steamapicontext = NULL;

void Voice_EndChannel( int iChannel );
void VoiceTweak_EndVoiceTweakMode();
void EngineTool_OverrideSampleRate( int& rate );

// Special entity index used for tweak mode.
#define TWEAKMODE_ENTITYINDEX				-500

// Special channel index passed to Voice_AddIncomingData when in tweak mode.
#define TWEAKMODE_CHANNELINDEX				-100


// How long does the sign stay above someone's head when they talk?
#define SPARK_TIME		0.2

// How long a voice channel has to be inactive before we free it.
#define DIE_COUNTDOWN	0.5

// Size of the circular buffer.  This should be BIGGER than the pad time,
// or else a little burst of data right as we fil the buffer will cause
// us to have nowhere to put the data and overflow the buffer!
#define VOICE_RECEIVE_BUFFER_SECONDS		2.0

// If you can figure out how to get OSX to just compute this value (and use it as a template argument)
// in the circular buffer below.  Then go for it.
#define VOICE_RECEIVE_BUFFER_SIZE			88200
COMPILE_TIME_ASSERT( VOICE_RECEIVE_BUFFER_SIZE == VOICE_OUTPUT_SAMPLE_RATE * BYTES_PER_SAMPLE * VOICE_RECEIVE_BUFFER_SECONDS );

#define LOCALPLAYERTALKING_TIMEOUT			0.2f	// How long it takes for the client to decide the server isn't sending acks
													// of voice data back.

// true when using the speex codec
static bool g_bIsSpeex = false;

// If this is defined, then the data is converted to 8-bit and sent otherwise uncompressed.
// #define VOICE_SEND_RAW_TEST

// The format we sample voice in.
WAVEFORMATEX g_VoiceSampleFormat =
{
	WAVE_FORMAT_PCM,		// wFormatTag
	1,						// nChannels
	VOICE_OUTPUT_SAMPLE_RATE,					// nSamplesPerSec
	VOICE_OUTPUT_SAMPLE_RATE*2,					// nAvgBytesPerSec
	2,						// nBlockAlign
	16,						// wBitsPerSample
	sizeof(WAVEFORMATEX)	// cbSize
};


ConVar voice_loopback( "voice_loopback", "0", FCVAR_USERINFO );
ConVar voice_fadeouttime( "voice_fadeouttime", "0.0" );	// It fades to no sound at the tail end of your voice data when you release the key.
ConVar voice_threshold_delay( "voice_thresold_delay", "0.5" );

ConVar voice_record_steam( "voice_record_steam", "0", 0, "If true use Steam to record voice (not the engine codec)" );

ConVar voice_scale("voice_scale", "1.0", FCVAR_ARCHIVE | FCVAR_RELEASE, "Overall volume of voice over IP" );
ConVar voice_caster_scale( "voice_caster_scale", "1", FCVAR_ARCHIVE );


// Debugging cvars.
ConVar voice_profile( "voice_profile", "0" );
ConVar voice_showchannels( "voice_showchannels", "0" );	// 1 = list channels
															// 2 = show timing info, etc
ConVar voice_showincoming( "voice_showincoming", "0" );	// show incoming voice data

int Voice_SamplesPerSec()
{
	if ( voice_record_steam.GetBool() && steamapicontext && steamapicontext->SteamUser()  )
		return steamapicontext->SteamUser()->GetVoiceOptimalSampleRate();

	int rate = ( g_bIsSpeex ? VOICE_OUTPUT_SAMPLE_RATE_SPEEX : VOICE_OUTPUT_SAMPLE_RATE ); //g_VoiceSampleFormat.nSamplesPerSec;
	EngineTool_OverrideSampleRate( rate );
	return rate;
}

int Voice_AvgBytesPerSec()
{
	int rate = Voice_SamplesPerSec();
	
	return ( rate * g_VoiceSampleFormat.wBitsPerSample ) >> 3;
}

//-----------------------------------------------------------------------------
// Convar callback
//-----------------------------------------------------------------------------
void VoiceEnableCallback( IConVar *var, const char *pOldValue, float flOldValue )
{
	if ( ((ConVar *)var)->GetBool() )
	{
		Voice_ForceInit();
	}
}

ConVar voice_system_enable( "voice_system_enable", "1", FCVAR_ARCHIVE, "Toggle voice system.", VoiceEnableCallback );		// Globally enable or disable voice system.
ConVar voice_enable( "voice_enable", "1", FCVAR_ARCHIVE, "Toggle voice transmit and receive." );
ConVar voice_caster_enable( "voice_caster_enable", "0", FCVAR_ARCHIVE, "Toggle voice transmit and receive for casters. 0 = no caster, account number of caster to enable." );
ConVar voice_threshold( "voice_threshold", "4000", FCVAR_ARCHIVE | FCVAR_CLIENTDLL );

extern ConVar sv_voicecodec;

// Have it force your mixer control settings so waveIn comes from the microphone. 
// CD rippers change your waveIn to come from the CD drive
ConVar voice_forcemicrecord( "voice_forcemicrecord", "1", FCVAR_ARCHIVE );

int g_nVoiceFadeSamples		= 1;							// Calculated each frame from the cvar.
float g_VoiceFadeMul		= 1;							// 1 / (g_nVoiceFadeSamples - 1).

// While in tweak mode, you can't hear anything anyone else is saying, and your own voice data
// goes directly to the speakers.
bool g_bInTweakMode = false;
int g_VoiceTweakSpeakingVolume = 0;

bool g_bVoiceAtLeastPartiallyInitted = false;

// Timing info for each frame.
static double	g_CompressTime = 0;
static double	g_DecompressTime = 0;
static double	g_GainTime = 0;
static double	g_UpsampleTime = 0;

class CVoiceTimer
{
public:
	inline void		Start()
	{
		if( voice_profile.GetInt() )
		{
			m_StartTime = Plat_FloatTime();
		}
	}
	
	inline void		End(double *out)
	{
		if( voice_profile.GetInt() )
		{
			*out += Plat_FloatTime() - m_StartTime;
		}
	}

	double			m_StartTime;
};


static float	g_fLocalPlayerTalkingLastUpdateRealTime = 0.0f;
static bool		g_bLocalPlayerTalkingAck[ MAX_SPLITSCREEN_CLIENTS ];
static float	g_LocalPlayerTalkingTimeout[ MAX_SPLITSCREEN_CLIENTS ];


CSysModule *g_hVoiceCodecDLL = 0;

// Voice recorder. Can be waveIn, DSound, or whatever.
static IVoiceRecord *g_pVoiceRecord = NULL;
static IVoiceCodec  *g_pEncodeCodec = NULL;

static bool			g_bVoiceRecording = false;	// Are we recording at the moment?

// A high precision client-local timestamp that is assumed to progress in approximate realtime
// (in lockstep with any transmitted audio).  This is sent with voice packets, so that recipients
// can properly account for silence.
static uint32 s_nRecordingTimestamp_UncompressedSampleOffset;

/// Realtime time value corresponding to the above audio recoridng timestamp.  This realtime value
/// is used so that we can advance the audio timestamp approximately if we don't get called for a while.
/// if there's a gigantic gap, it probably really doesn't matter.  But for small gaps we'd like to
/// get the timing about right.
static double s_flRecordingTimestamp_PlatTime;

/// Make sure timestamnp system is ready to go.
static void VoiceRecord_CheckInitTimestamp()
{
	if ( s_flRecordingTimestamp_PlatTime == 0.0 && s_nRecordingTimestamp_UncompressedSampleOffset == 0 )
	{
		s_nRecordingTimestamp_UncompressedSampleOffset = 1;
		s_flRecordingTimestamp_PlatTime = Plat_FloatTime();
	}
}

/// Advance audio timestamp using the platform timer
static void VoiceRecord_ForceAdvanceSampleOffsetUsingPlatTime()
{
	VoiceRecord_CheckInitTimestamp();

	// Advance the timestamp forward
	double flNow = Plat_FloatTime();
	int nSamplesElapsed = ( flNow - s_flRecordingTimestamp_PlatTime ) * ( g_bIsSpeex ? VOICE_OUTPUT_SAMPLE_RATE_SPEEX : VOICE_OUTPUT_SAMPLE_RATE );
	if ( nSamplesElapsed > 0 )
		s_nRecordingTimestamp_UncompressedSampleOffset += (uint32)nSamplesElapsed;
	s_flRecordingTimestamp_PlatTime = flNow;
}

// Which "section" are we in?  A section is basically a segment of non-silence data that we might want to transmit.
static uint8 s_nRecordingSection;

/// Byte offset of compressed data, within the current section.  As per TCP-style sequence numbering conventions,
/// this matches the most recent sequence number we sent.
static uint32 s_nRecordingSectionCompressedByteOffset;

// Called when we know that we are currently in silence, or at the beginning or end
// of a non-silence section
static void VoiceRecord_MarkSectionBoundary()
{
	// We allow this function to be called redundantly.
	// Don't advance the section number unless we really need to
	if ( s_nRecordingSectionCompressedByteOffset > 0 || s_nRecordingSection == 0 )
	{
		++s_nRecordingSection;
		if ( s_nRecordingSection == 0 ) // never use section 0
			s_nRecordingSection = 1;
	}

	// Always reset byte offset
	s_nRecordingSectionCompressedByteOffset = 0;

	// Reset encoder state for the next real section with data, whenever that may be
	if ( g_pEncodeCodec )
		g_pEncodeCodec->ResetState();
}

static bool VoiceRecord_Start()
{
	// Update timestamp, so we can properly account for silence
	VoiceRecord_ForceAdvanceSampleOffsetUsingPlatTime();
	VoiceRecord_MarkSectionBoundary();

	if ( voice_record_steam.GetBool() && steamapicontext && steamapicontext->SteamUser() )
	{
		steamapicontext->SteamUser()->StartVoiceRecording();
		return true;
	}
	else if ( g_pVoiceRecord )
	{
		return g_pVoiceRecord->RecordStart();
	}
	return false;
}

static void VoiceRecord_Stop()
{
	// Update timestamp, so we can properly account for silence
	VoiceRecord_ForceAdvanceSampleOffsetUsingPlatTime();
	VoiceRecord_MarkSectionBoundary();

	if ( voice_record_steam.GetBool() && steamapicontext && steamapicontext->SteamUser() )
	{
		steamapicontext->SteamUser()->StopVoiceRecording();
	}
	else if ( g_pVoiceRecord )
	{
		return g_pVoiceRecord->RecordStop();
	}
}

// Hacked functions to create the inputs and codecs..
#ifdef _PS3
static IVoiceRecord*	CreateVoiceRecord_DSound(int nSamplesPerSec) { return NULL; }
#else
extern IVoiceRecord*	CreateVoiceRecord_DSound(int nSamplesPerSec);
#endif

ConVar voice_gain_rate( "voice_gain_rate", "1.0", FCVAR_NONE );
ConVar voice_gain_downward_multiplier( "voice_gain_downward_multiplier", "100.0", FCVAR_NONE );  // how quickly it will lower gain when it detects that the current gain value will cause clipping
ConVar voice_gain_target( "voice_gain_target", "32000", FCVAR_NONE );
ConVar voice_gain_max( "voice_gain_max", "35", FCVAR_NONE );

class CGainManager
{
public:
	
	CGainManager( void );

	void Apply( short *pBuffer, int buffer_size, bool bCaster );

private:

	double m_fTargetGain;
	double m_fCurrentGain;
};

CGainManager::CGainManager( void )
{
	m_fTargetGain = 1.0f;
	m_fCurrentGain = 1.0f;
}

void CGainManager::Apply( short *pSamples, int nSamples, bool bCaster )
{
	if ( nSamples == 0 )
		return;

	// Scan for peak
	int iPeak = 0;
	for ( int i = 0; i < nSamples; i++ )
	{
		int iSample = abs( pSamples[i] );
		iPeak = Max( iPeak, iSample );
	}

	if ( bCaster )
	{
		m_fTargetGain = ( voice_gain_target.GetFloat() * Clamp( voice_caster_scale.GetFloat(), 0.0f, 2.0f ) ) / (float)iPeak;
	}
	else
	{
		m_fTargetGain = ( voice_gain_target.GetFloat() * Clamp( voice_scale.GetFloat(), 0.0f, 2.0f ) ) / (float)iPeak;
	}

	double fMovementRate = voice_gain_rate.GetFloat();
	double fMaxGain = voice_gain_max.GetFloat();

	for ( int i = 0; i < nSamples; i++ )
	{
		int nSample = int( float( pSamples[i] ) * m_fCurrentGain );
		pSamples[i] = (short)Clamp( nSample, -32768, 32767 );

		// Adjust downward very very quickly to prevent clipping
		m_fCurrentGain += ( m_fTargetGain - m_fCurrentGain ) * fMovementRate * 0.0001 * ( ( m_fTargetGain < m_fCurrentGain ) ? voice_gain_downward_multiplier.GetFloat() : 1.0f );
		m_fCurrentGain = Clamp( m_fCurrentGain, 0.0, fMaxGain );
	}

	//Msg( "Peak: %d, Current Gain: %2.2f, TargetGain: %2.2f\n", iPeak, (float)m_fCurrentGain, (float)m_fTargetGain );
}


//
// Used for storing incoming voice data from an entity.
//
class CVoiceChannel
{
public:
									CVoiceChannel();

	// Called when someone speaks and a new voice channel is setup to hold the data.
	void							Init( int nEntity, float timePadding, bool bCaster = false );

public:
	int								m_iEntity;		// Number of the entity speaking on this channel (index into cl_entities).
													// This is -1 when the channel is unused.

	
	CSizedCircularBuffer
		<VOICE_RECEIVE_BUFFER_SIZE>	m_Buffer;		// Circular buffer containing the voice data.

	bool							m_bStarved;		// Set to true when the channel runs out of data for the mixer.
													// The channel is killed at that point.
	bool							m_bFirstPacket;		// Have we received any packets yet?

	float							m_TimePad;		// Set to TIME_PADDING when the first voice packet comes in from a sender.
													// We add time padding (for frametime differences)
													// by waiting a certain amount of time before starting to output the sound.
	double							m_flTimeFirstPacket;
	double							m_flTimeExpectedStart;

	int								m_nMinDesiredLeadSamples; // Healthy amount of buffering.  This is simply the time padding value passed to init, times the expected sample rate
	int								m_nMaxDesiredLeadSamples; // Excessive amount of buffering.  Too much more and we risk overflowing the buffer.

	IVoiceCodec						*m_pVoiceCodec;	// Each channel gets is own IVoiceCodec instance so the codec can maintain state.

	CGainManager					m_GainManager;

	CVoiceChannel					*m_pNext;
	
	bool							m_bProximity;
	int								m_nViewEntityIndex;
	int								m_nSoundGuid;

	uint8							m_nCurrentSection;
	uint32							m_nExpectedCompressedByteOffset;
	uint32							m_nExpectedUncompressedSampleOffset;
	short							m_nLastSample;

	bool							m_bCaster;
};


CVoiceChannel::CVoiceChannel()
{
	m_iEntity = -1;
	m_pVoiceCodec = NULL;
	m_nViewEntityIndex = -1;
	m_nSoundGuid = -1;
	m_bCaster = false;
}

void CVoiceChannel::Init( int nEntity, float timePadding, bool bCaster )
{
	m_iEntity = nEntity;
	m_bStarved = false;
	m_bFirstPacket = true;
	m_nLastSample = 0;
	m_Buffer.Flush();
	m_bCaster = bCaster;
	m_TimePad = timePadding;
	if ( m_TimePad <= 0.0f )
	{
		m_TimePad = FLT_EPSILON;  // Must have at least one update
	}

	// Don't aim to fill the buffer up too full, or we will overflow.
	// this buffer class does not grow, so we really don't ever want
	// it to get full.
	const float kflMaxTimePad = VOICE_RECEIVE_BUFFER_SECONDS * 0.8f;
	if ( m_TimePad > kflMaxTimePad )
	{
		Assert( m_TimePad < kflMaxTimePad );
		m_TimePad = kflMaxTimePad;
	}

	if ( g_bIsSpeex )
	{
		m_nMaxDesiredLeadSamples = int( kflMaxTimePad * VOICE_OUTPUT_SAMPLE_RATE_SPEEX );
		m_nMinDesiredLeadSamples = Max( 256, int( m_TimePad * VOICE_OUTPUT_SAMPLE_RATE_SPEEX ) );
	}
	else
	{
		m_nMaxDesiredLeadSamples = int( kflMaxTimePad * VOICE_OUTPUT_SAMPLE_RATE );
		m_nMinDesiredLeadSamples = Max( 256, int( m_TimePad * VOICE_OUTPUT_SAMPLE_RATE ) );
	}

	m_flTimeFirstPacket = Plat_FloatTime();
	m_flTimeExpectedStart = m_flTimeFirstPacket + m_TimePad;

	m_nCurrentSection = 0;
	m_nExpectedCompressedByteOffset = 0;
	m_nExpectedUncompressedSampleOffset = 0;

	if ( m_pVoiceCodec )
		m_pVoiceCodec->ResetState();
}



// Incoming voice channels.
CVoiceChannel g_VoiceChannels[VOICE_NUM_CHANNELS];


// These are used for recording the wave data into files for debugging.
#define MAX_WAVEFILEDATA_LEN	1024*1024
char *g_pUncompressedFileData = NULL;
int g_nUncompressedDataBytes = 0;
const char *g_pUncompressedDataFilename = NULL;

char *g_pDecompressedFileData = NULL;
int g_nDecompressedDataBytes = 0;
const char *g_pDecompressedDataFilename = NULL;

char *g_pMicInputFileData = NULL;
int g_nMicInputFileBytes = 0;
int g_CurMicInputFileByte = 0;
double g_MicStartTime;

static ConVar voice_writevoices( "voice_writevoices", "0", 0, "Saves each speaker's voice data into separate .wav files\n" );
class CVoiceWriterData
{
public:
	CVoiceWriterData() :
		m_pChannel( NULL ),
		m_nCount( 0 ),
		m_Buffer()
	{
	}

	CVoiceWriterData( const CVoiceWriterData &src )
	{
		m_pChannel = src.m_pChannel;
		m_nCount = src.m_nCount;
		m_Buffer.Clear();
		m_Buffer.Put( src.m_Buffer.Base(), src.m_Buffer.TellPut() );
	}

	static bool Less( const CVoiceWriterData &lhs, const CVoiceWriterData &rhs )
	{
		return lhs.m_pChannel < rhs.m_pChannel;
	}

	CVoiceChannel	*m_pChannel;
	int				m_nCount;
	CUtlBuffer		m_Buffer;
};

class CVoiceWriter
{
public:
	CVoiceWriter() :
		m_VoiceWriter( 0, 0, CVoiceWriterData::Less )
	{
	}

	void Flush()
	{
		for ( int i = m_VoiceWriter.FirstInorder(); i != m_VoiceWriter.InvalidIndex(); i = m_VoiceWriter.NextInorder( i ) )
		{
			CVoiceWriterData *data = &m_VoiceWriter[ i ];

			if ( data->m_Buffer.TellPut() <= 0 )
				continue;
			data->m_Buffer.Purge();
		}
	}

	void Finish()
	{
		if ( !g_pSoundServices->IsConnected() )
		{
			Flush();
			return;
		}

		for ( int i = m_VoiceWriter.FirstInorder(); i != m_VoiceWriter.InvalidIndex(); i = m_VoiceWriter.NextInorder( i ) )
		{
			CVoiceWriterData *data = &m_VoiceWriter[ i ];
			
			if ( data->m_Buffer.TellPut() <= 0 )
				continue;

			int index = data->m_pChannel - g_VoiceChannels;
			Assert( index >= 0 && index < ARRAYSIZE( g_VoiceChannels ) );

			char path[ MAX_PATH ];
			Q_snprintf( path, sizeof( path ), "%s/voice", g_pSoundServices->GetGameDir() );
			g_pFileSystem->CreateDirHierarchy( path );

			char fn[ MAX_PATH ];
			Q_snprintf( fn, sizeof( fn ), "%s/pl%02d_slot%d-time%d.wav", path, index, data->m_nCount, (int)g_pSoundServices->GetClientTime() );

			WriteWaveFile( fn, (const char *)data->m_Buffer.Base(), data->m_Buffer.TellPut(), g_VoiceSampleFormat.wBitsPerSample, g_VoiceSampleFormat.nChannels, Voice_SamplesPerSec() );

			Msg( "Writing file %s\n", fn );

			++data->m_nCount;
			data->m_Buffer.Purge();
		}
	}


	void AddDecompressedData( CVoiceChannel *ch, const byte *data, size_t datalen )
	{
		if ( !voice_writevoices.GetBool() )
			return;

		CVoiceWriterData search;
		search.m_pChannel = ch;
		int idx = m_VoiceWriter.Find( search ); 
		if ( idx == m_VoiceWriter.InvalidIndex() )
		{
			idx = m_VoiceWriter.Insert( search );
		}

		CVoiceWriterData *slot = &m_VoiceWriter[ idx ];
		slot->m_Buffer.Put( data, datalen );
	}
private:

	CUtlRBTree< CVoiceWriterData > m_VoiceWriter;
};

static CVoiceWriter g_VoiceWriter;

inline void ApplyFadeToSamples(short *pSamples, int nSamples, int fadeOffset, float fadeMul)
{
	for(int i=0; i < nSamples; i++)
	{
		float percent = (i+fadeOffset) * fadeMul;
		pSamples[i] = (short)(pSamples[i] * (1 - percent));
	}
}


bool Voice_Enabled( void )
{
	return voice_enable.GetBool();
}

bool Voice_CasterEnabled( uint32 uCasterAccountID )
{
	return ( uCasterAccountID == ( uint32 )( voice_caster_enable.GetInt() ) );
}

void Voice_SetCaster( uint32 uCasterAccountID )
{
	voice_caster_enable.SetValue( ( int )( uCasterAccountID ) );
}

bool Voice_SystemEnabled( void )
{
	return voice_system_enable.GetBool();
}

ConVar voice_buffer_debug( "voice_buffer_debug", "0" );

int Voice_GetOutputData(
	const int iChannel,			//! The voice channel it wants samples from.
	char *copyBufBytes,			//! The buffer to copy the samples into.
	const int copyBufSize,		//! Maximum size of copyBuf.
	const int samplePosition,	//! Which sample to start at.
	const int sampleCount		//! How many samples to get.
)
{
	CVoiceChannel *pChannel = &g_VoiceChannels[iChannel];	
	short *pCopyBuf = (short*)copyBufBytes;


	int maxOutSamples = copyBufSize / BYTES_PER_SAMPLE;

	// Find out how much we want and get it from the received data channel.	
	CCircularBuffer *pBuffer = &pChannel->m_Buffer;
	int nReadAvail = pBuffer->GetReadAvailable();
	int nBytesToRead = MIN(MIN(nReadAvail, (int)maxOutSamples), sampleCount * BYTES_PER_SAMPLE);
	int nSamplesGotten = pBuffer->Read(pCopyBuf, nBytesToRead) / BYTES_PER_SAMPLE;

	if ( voice_buffer_debug.GetBool() )
	{
		Msg( "%.2f: Voice_GetOutputData channel %d avail %d bytes, tried %d bytes, got %d samples\n", Plat_FloatTime(), iChannel, nReadAvail, nBytesToRead, nSamplesGotten );
	}

	// Are we at the end of the buffer's data? If so, fade data to silence so it doesn't clip.
	int readSamplesAvail = pBuffer->GetReadAvailable() / BYTES_PER_SAMPLE;
	if(readSamplesAvail < g_nVoiceFadeSamples)
	{
		if ( voice_buffer_debug.GetBool() )
		{
			Msg( "%.2f: Voice_GetOutputData channel %d applying fade\n", Plat_FloatTime(), iChannel );
		}

		int bufferFadeOffset = MAX((readSamplesAvail + nSamplesGotten) - g_nVoiceFadeSamples, 0);
		int globalFadeOffset = MAX(g_nVoiceFadeSamples - (readSamplesAvail + nSamplesGotten), 0);
		
		ApplyFadeToSamples(
			&pCopyBuf[bufferFadeOffset], 
			nSamplesGotten - bufferFadeOffset,
			globalFadeOffset,
			g_VoiceFadeMul);
	}
	
	// If there weren't enough samples in the received data channel, 
	//   pad it with a copy of the most recent data, and if there 
	//   isn't any, then use zeros.
	if ( nSamplesGotten < sampleCount )
	{
		if ( voice_buffer_debug.GetBool() )
		{
			Msg( "%.2f: Voice_GetOutputData channel %d padding!\n", Plat_FloatTime(), iChannel );
		}

		int wantedSampleCount = MIN( sampleCount, maxOutSamples );
		int nAdditionalNeeded = (wantedSampleCount - nSamplesGotten);
		if ( nSamplesGotten > 0 )
		{
			short *dest = (short *)&pCopyBuf[ nSamplesGotten ];
			int nSamplesToDuplicate = MIN( nSamplesGotten, nAdditionalNeeded );
			const short *src = (short *)&pCopyBuf[ nSamplesGotten - nSamplesToDuplicate ];

			Q_memcpy( dest, src, nSamplesToDuplicate * BYTES_PER_SAMPLE );

			if ( voice_buffer_debug.GetBool() )
			{
				Msg( "duplicating %d samples\n", nSamplesToDuplicate );
			}

			nAdditionalNeeded -= nSamplesToDuplicate;
			if ( nAdditionalNeeded > 0  )
			{
				dest = (short *)&pCopyBuf[ nSamplesGotten + nSamplesToDuplicate ];
				Q_memset(dest, 0, nAdditionalNeeded * BYTES_PER_SAMPLE);

				if ( voice_buffer_debug.GetBool() )
				{
					Msg( "zeroing %d samples\n", nAdditionalNeeded );
				}

				Assert( ( nAdditionalNeeded + nSamplesGotten + nSamplesToDuplicate ) == wantedSampleCount );
			}
		}
		else
		{
			Q_memset( &pCopyBuf[ nSamplesGotten ], 0, nAdditionalNeeded * BYTES_PER_SAMPLE );

			if ( voice_buffer_debug.GetBool() )
			{
				Msg( "no buffer data, zeroing all %d samples\n", nAdditionalNeeded );
			}

		}
		nSamplesGotten = wantedSampleCount;
	}

	// If the buffer is out of data, mark this channel to go away.
	if(pBuffer->GetReadAvailable() == 0)
	{
		if ( voice_buffer_debug.GetBool() )
		{
			Msg( "%.2f: Voice_GetOutputData channel %d starved!\n", Plat_FloatTime(), iChannel );
		}
		pChannel->m_bStarved = true;
	}

	if(voice_showchannels.GetInt() >= 2)
	{
		Msg("Voice - mixed %d samples from channel %d\n", nSamplesGotten, iChannel);
	}

	VoiceSE_MoveMouth(pChannel->m_iEntity, (short*)copyBufBytes, nSamplesGotten);
	return nSamplesGotten;
}


void Voice_OnAudioSourceShutdown( int iChannel )
{
	Voice_EndChannel( iChannel );
}


// ------------------------------------------------------------------------ //
// Internal stuff.
// ------------------------------------------------------------------------ //

CVoiceChannel* GetVoiceChannel(int iChannel, bool bAssert=true)
{
	if(iChannel < 0 || iChannel >= VOICE_NUM_CHANNELS)
	{
		if(bAssert)
		{
			Assert(false);
		}
		return NULL;
	}
	else
	{
		return &g_VoiceChannels[iChannel];
	}
}

//char g_pszCurrentVoiceCodec[256];
//int g_iCurrentVoiceVersion = -1;

bool Voice_Init(const char *pCodecName, int iVersion )
{
	if ( voice_system_enable.GetInt() == 0 )
	{
		return false;
	}

//	if ( V_strncmp( g_pszCurrentVoiceCodec, pCodecName, sizeof( g_pszCurrentVoiceCodec ) ) == 0 && g_iCurrentVoiceVersion == iVersion )
//		return true;

	EngineVGui()->UpdateProgressBar( PROGRESS_DEFAULT );

	Voice_Deinit();

	g_bVoiceAtLeastPartiallyInitted = true;

	if(!VoiceSE_Init())
		return false;

	if ( V_strcmp( pCodecName, "vaudio_speex" ) == 0 )
	{
		g_bIsSpeex = true;
		g_VoiceSampleFormat.nSamplesPerSec = VOICE_OUTPUT_SAMPLE_RATE_SPEEX;
		g_VoiceSampleFormat.nAvgBytesPerSec = VOICE_OUTPUT_SAMPLE_RATE_SPEEX * 2;
	}
	else
	{
		g_bIsSpeex = false;
		g_VoiceSampleFormat.nSamplesPerSec = VOICE_OUTPUT_SAMPLE_RATE;
		g_VoiceSampleFormat.nAvgBytesPerSec = VOICE_OUTPUT_SAMPLE_RATE * 2;
	}

	EngineVGui()->UpdateProgressBar( PROGRESS_DEFAULT );

#ifdef OSX
	IVoiceRecord* CreateVoiceRecord_AudioQueue(int sampleRate);
	g_pVoiceRecord = CreateVoiceRecord_AudioQueue( Voice_SamplesPerSec() );
	//g_pVoiceRecord = NULL;
	if ( !g_pVoiceRecord )
#endif
		// Get the voice input device.
	g_pVoiceRecord = CreateVoiceRecord_DSound( Voice_SamplesPerSec() );
	if( !g_pVoiceRecord )
	{
		Msg( "Unable to initialize DirectSoundCapture. You won't be able to speak to other players." );
	}

	if ( steamapicontext == NULL )
	{
		steamapicontext = &g_SteamAPIContext;
		steamapicontext->Init();
	}
	
	EngineVGui()->UpdateProgressBar( PROGRESS_DEFAULT );

	// Get the codec.
	CreateInterfaceFn createCodecFn;
	//
	// We must explicitly check codec DLL strings against valid codecs
	// to avoid remote code execution by loading a module supplied in server string
	// See security issue disclosed 12-Jan-2016
	//
	if (   !V_strcmp( pCodecName, "vaudio_celt" )
		|| !V_strcmp( pCodecName, "vaudio_speex" )
		|| !V_strcmp( pCodecName, "vaudio_miles" ) )
	{
		g_hVoiceCodecDLL = FileSystem_LoadModule( pCodecName );
	}
	else
	{
		g_hVoiceCodecDLL = NULL;
	}

	EngineVGui()->UpdateProgressBar( PROGRESS_DEFAULT );

	if ( !g_hVoiceCodecDLL || (createCodecFn = Sys_GetFactory(g_hVoiceCodecDLL)) == NULL ||
		 (g_pEncodeCodec = (IVoiceCodec*)createCodecFn(pCodecName, NULL)) == NULL || !g_pEncodeCodec->Init( iVersion ) )
	{
		Msg("Unable to load voice codec '%s'. Voice disabled.\n", pCodecName);
		Voice_Deinit();
		return false;
	}

	for(int i=0; i < VOICE_NUM_CHANNELS; i++)
	{
		CVoiceChannel *pChannel = &g_VoiceChannels[i];

		EngineVGui()->UpdateProgressBar( PROGRESS_DEFAULT );

		if((pChannel->m_pVoiceCodec = (IVoiceCodec*)createCodecFn(pCodecName, NULL)) == NULL || !pChannel->m_pVoiceCodec->Init( iVersion ))
		{
			Voice_Deinit();
			return false;
		}
	}

	EngineVGui()->UpdateProgressBar( PROGRESS_DEFAULT );

	InitMixerControls();

	if( voice_forcemicrecord.GetInt() )
	{
		if( g_pMixerControls )
			g_pMixerControls->SelectMicrophoneForWaveInput();
	}

//	V_strncpy( g_pszCurrentVoiceCodec, pCodecName, sizeof( g_pszCurrentVoiceCodec ) );
//	g_iCurrentVoiceVersion = iVersion;

	return true;
}


void Voice_EndChannel(int iChannel)
{
	Assert(iChannel >= 0 && iChannel < VOICE_NUM_CHANNELS);

	CVoiceChannel *pChannel = &g_VoiceChannels[iChannel];
	
	if ( pChannel->m_iEntity != -1 )
	{
		int iEnt = pChannel->m_iEntity;
		pChannel->m_iEntity = -1;

		if ( pChannel->m_bProximity == true )
		{
			VoiceSE_EndChannel( iChannel, iEnt );
		}
		else
		{
			VoiceSE_EndChannel( iChannel, pChannel->m_nViewEntityIndex );
		}
		
		g_pSoundServices->OnChangeVoiceStatus( iEnt, -1, false );
		VoiceSE_CloseMouth( iEnt );

		pChannel->m_nViewEntityIndex = -1;
		pChannel->m_nSoundGuid = -1;

		// If the tweak mode channel is ending
	}
}


void Voice_EndAllChannels()
{
	for(int i=0; i < VOICE_NUM_CHANNELS; i++)
	{
		Voice_EndChannel(i);
	}
}

bool EngineTool_SuppressDeInit();

void Voice_Deinit()
{
	// This call tends to be expensive and when voice is not enabled it will continually
	// call in here, so avoid the work if possible.
	if( !g_bVoiceAtLeastPartiallyInitted )
		return;

	if ( EngineTool_SuppressDeInit() )
		return;

//	g_pszCurrentVoiceCodec[0] = 0;
//	g_iCurrentVoiceVersion = -1;

	Voice_EndAllChannels();

	Voice_RecordStop();

	for(int i=0; i < VOICE_NUM_CHANNELS; i++)
	{
		CVoiceChannel *pChannel = &g_VoiceChannels[i];
		
		if(pChannel->m_pVoiceCodec)
		{
			pChannel->m_pVoiceCodec->Release();
			pChannel->m_pVoiceCodec = NULL;
		}
	}

	if(g_pEncodeCodec)
	{
		g_pEncodeCodec->Release();
		g_pEncodeCodec = NULL;
	}

	if(g_hVoiceCodecDLL)
	{
		FileSystem_UnloadModule(g_hVoiceCodecDLL);
		g_hVoiceCodecDLL = NULL;
	}

	if(g_pVoiceRecord)
	{
		g_pVoiceRecord->Release();
		g_pVoiceRecord = NULL;
	}

	VoiceSE_Term();

	g_bVoiceAtLeastPartiallyInitted = false;
}

bool Voice_GetLoopback()
{
	return !!voice_loopback.GetInt();
}


void Voice_LocalPlayerTalkingAck( int iSsSlot )
{
	iSsSlot = clamp( iSsSlot, 0, MAX_SPLITSCREEN_CLIENTS - 1 );

	if( !g_bLocalPlayerTalkingAck[ iSsSlot ] )
	{
		// Tell the client DLL when this changes.
		g_pSoundServices->OnChangeVoiceStatus( -2, iSsSlot, TRUE );
	}

	g_bLocalPlayerTalkingAck[ iSsSlot ] = true;
	g_LocalPlayerTalkingTimeout[ iSsSlot ] = 0;
}


void Voice_UpdateVoiceTweakMode()
{
	// Tweak mode just pulls data from the voice stream, and does nothing with it
	if(!g_bInTweakMode || !g_pVoiceRecord)
		return;

	char uchVoiceData[16384];
	bool bFinal = false;
	VoiceFormat_t format;
	Voice_GetCompressedData(uchVoiceData, sizeof(uchVoiceData), bFinal, &format );
}


bool Voice_Idle(float frametime)
{
	if( voice_system_enable.GetInt() == 0 )
	{
		Voice_Deinit();
		return false;
	}
		
	float fTimeDiff = Plat_FloatTime() - g_fLocalPlayerTalkingLastUpdateRealTime;

	if ( fTimeDiff < frametime )
	{
		// Not enough time has passed... don't update
		return false;
	}

	// Set how much time has passed since the last update
	frametime = MIN( fTimeDiff, frametime * 2.0f );	// Cap how much time can pass at 2 tick sizes

	// Remember when we last updated
	g_fLocalPlayerTalkingLastUpdateRealTime = Plat_FloatTime();

	for ( int k = 0; k < MAX_SPLITSCREEN_CLIENTS; ++ k )
	{
		if(g_bLocalPlayerTalkingAck[k])
		{
			g_LocalPlayerTalkingTimeout[k] += frametime;
			if(g_LocalPlayerTalkingTimeout[k] > LOCALPLAYERTALKING_TIMEOUT)
			{
				g_bLocalPlayerTalkingAck[k] = false;

				// Tell the client DLL.
				g_pSoundServices->OnChangeVoiceStatus(-2, k, FALSE);
			}
		}
	}

	// Precalculate these to speedup the voice fadeout.
	g_nVoiceFadeSamples = MAX((int)(voice_fadeouttime.GetFloat() * ( g_bIsSpeex ? VOICE_OUTPUT_SAMPLE_RATE_SPEEX : VOICE_OUTPUT_SAMPLE_RATE ) ), 2);
	g_VoiceFadeMul = 1.0f / (g_nVoiceFadeSamples - 1);

	if(g_pVoiceRecord)
		g_pVoiceRecord->Idle();

	// If we're in voice tweak mode, feed our own data back to us.
	Voice_UpdateVoiceTweakMode();

	// Age the channels.
	int nActive = 0;
	for(int i=0; i < VOICE_NUM_CHANNELS; i++)
	{
		CVoiceChannel *pChannel = &g_VoiceChannels[i];
		
		if(pChannel->m_iEntity != -1)
		{
			if(pChannel->m_bStarved)
			{
				// Kill the channel. It's done playing.
				Voice_EndChannel(i);
				pChannel->m_nSoundGuid = -1;
			}
			else if ( pChannel->m_nSoundGuid < 0 )
			{

				// Sound is not currently playing.  Should it be?
				// Start it if enough time has elapsed, or if we have
				// enough buffered.
				pChannel->m_TimePad -= frametime;
				int nDesiredLeadBytes = pChannel->m_nMinDesiredLeadSamples*BYTES_PER_SAMPLE;
				if( pChannel->m_TimePad <= 0 || pChannel->m_Buffer.GetReadAvailable() >= nDesiredLeadBytes )
				{

					double flNow = Plat_FloatTime();
					float flEpasedSinceFirstPacket = flNow - pChannel->m_flTimeFirstPacket;
					if ( voice_showincoming.GetBool() )
					{
						Msg( "%.2f: Starting channel %d.  %d bytes buffered, %.0fms elapsed.  (%d samples more than desired, %.0fms later than expected)\n",
							flNow, i,
							pChannel->m_Buffer.GetReadAvailable(), flEpasedSinceFirstPacket * 1000.0f,
							pChannel->m_Buffer.GetReadAvailable() - nDesiredLeadBytes, ( flNow - pChannel->m_flTimeExpectedStart ) * 1000.0f );
					}

					// Start its audio.
					FORCE_DEFAULT_SPLITSCREEN_PLAYER_GUARD;

					pChannel->m_nViewEntityIndex = g_pSoundServices->GetViewEntity( 0 );
					pChannel->m_nSoundGuid = VoiceSE_StartChannel( i, pChannel->m_iEntity, pChannel->m_bProximity, pChannel->m_nViewEntityIndex );
					if ( pChannel->m_nSoundGuid <= 0 )
					{
						// couldn't allocate a sound channel for this voice data
						Voice_EndChannel(i);
						pChannel->m_nSoundGuid = -1;
					}
					else
					{
						g_pSoundServices->OnChangeVoiceStatus( pChannel->m_iEntity, -1, true );
					
						VoiceSE_InitMouth(pChannel->m_iEntity);
					}
				}

				++nActive;
			}
		}
	}

	if(nActive == 0)
		VoiceSE_EndOverdrive();

	VoiceSE_Idle(frametime);

	// voice_showchannels.
	if( voice_showchannels.GetInt() >= 1 )
	{
		for(int i=0; i < VOICE_NUM_CHANNELS; i++)
		{
			CVoiceChannel *pChannel = &g_VoiceChannels[i];
			
			if(pChannel->m_iEntity == -1)
				continue;

			Msg("Voice - chan %d, ent %d, bufsize: %d\n", i, pChannel->m_iEntity, pChannel->m_Buffer.GetReadAvailable());
		}
	}

	// Show profiling data?
	if( voice_profile.GetInt() )
	{
		Msg("Voice - compress: %7.2fu, decompress: %7.2fu, gain: %7.2fu, upsample: %7.2fu, total: %7.2fu\n", 
			g_CompressTime*1000000.0, 
			g_DecompressTime*1000000.0, 
			g_GainTime*1000000.0, 
			g_UpsampleTime*1000000.0,
			(g_CompressTime+g_DecompressTime+g_GainTime+g_UpsampleTime)*1000000.0
			);

		g_CompressTime = g_DecompressTime = g_GainTime = g_UpsampleTime = 0;
	}

	return true;
}


bool Voice_IsRecording()
{
	return g_bVoiceRecording;
}


bool Voice_RecordStart(
	const char *pUncompressedFile, 
	const char *pDecompressedFile,
	const char *pMicInputFile)
{
	if(!g_pEncodeCodec)
		return false;

	g_VoiceWriter.Flush();

	Voice_RecordStop();

	if(pMicInputFile)
	{
		int a, b, c;
		ReadWaveFile(pMicInputFile, g_pMicInputFileData, g_nMicInputFileBytes, a, b, c);
		g_CurMicInputFileByte = 0;
		g_MicStartTime = Plat_FloatTime();
	}

	if(pUncompressedFile)
	{
		g_pUncompressedFileData = new char[MAX_WAVEFILEDATA_LEN];
		g_nUncompressedDataBytes = 0;
		g_pUncompressedDataFilename = pUncompressedFile;
	}

	if(pDecompressedFile)
	{
		g_pDecompressedFileData = new char[MAX_WAVEFILEDATA_LEN];
		g_nDecompressedDataBytes = 0;
		g_pDecompressedDataFilename = pDecompressedFile;
	}

	g_bVoiceRecording = false;
	if(g_pVoiceRecord)
	{
		g_bVoiceRecording = VoiceRecord_Start();
		if(g_bVoiceRecording)
		{
			g_pSoundServices->OnChangeVoiceStatus(-1, GET_ACTIVE_SPLITSCREEN_SLOT(), TRUE);		// Tell the client DLL.
		}
	}

	return g_bVoiceRecording;
}


bool Voice_RecordStop()
{
	// Write the files out for debugging.
	if(g_pMicInputFileData)
	{
		delete [] g_pMicInputFileData;
		g_pMicInputFileData = NULL;
	}

	if(g_pUncompressedFileData)
	{
		WriteWaveFile(g_pUncompressedDataFilename, g_pUncompressedFileData, g_nUncompressedDataBytes, g_VoiceSampleFormat.wBitsPerSample, g_VoiceSampleFormat.nChannels, Voice_SamplesPerSec() );
		delete [] g_pUncompressedFileData;
		g_pUncompressedFileData = NULL;
	}

	if(g_pDecompressedFileData)
	{
		WriteWaveFile(g_pDecompressedDataFilename, g_pDecompressedFileData, g_nDecompressedDataBytes, g_VoiceSampleFormat.wBitsPerSample, g_VoiceSampleFormat.nChannels, Voice_SamplesPerSec() );
		delete [] g_pDecompressedFileData;
		g_pDecompressedFileData = NULL;
	}
	
	g_VoiceWriter.Finish();

	VoiceRecord_Stop();

	if(g_bVoiceRecording)
	{
		g_pSoundServices->OnChangeVoiceStatus(-1, GET_ACTIVE_SPLITSCREEN_SLOT(), FALSE);		// Tell the client DLL.
	}

	g_bVoiceRecording = false;
	return(true);
}

static float s_flThresholdDecayTime = 0.0f;


int Voice_GetCompressedData(char *pchDest, int nCount, bool bFinal, VoiceFormat_t *pOutFormat, uint8 *pnOutSectionNumber, uint32 *pnOutSectionSequenceNumber, uint32 *pnOutUncompressedSampleOffset )
{
	double flNow = Plat_FloatTime();

	// Make sure timestamp is initialized
	VoiceRecord_CheckInitTimestamp();

	// Here we protect again a weird client usage pattern where they don't call this function for a while.
	// If that happens, advance the timestamp.
	float flSecondsElapsed = flNow - s_flRecordingTimestamp_PlatTime;
	if ( flSecondsElapsed > 2.0 )
	{
		Warning( "Voice_GetCompressedData not called for %.1fms; manually advancing uncompressed sample offset and starting a new section\n", flSecondsElapsed * 1000.0f );
		VoiceRecord_ForceAdvanceSampleOffsetUsingPlatTime();
		VoiceRecord_MarkSectionBoundary();
	}
	s_flRecordingTimestamp_PlatTime = flNow;

	// Assume failure
	if ( pnOutSectionNumber )
		*pnOutSectionNumber = 0;
	if ( pnOutSectionSequenceNumber )
		*pnOutSectionSequenceNumber = 0;
	if ( pnOutUncompressedSampleOffset )
		*pnOutUncompressedSampleOffset = 0;

	if ( voice_record_steam.GetBool() && steamapicontext && steamapicontext->SteamUser() )
	{
		uint32 cbCompressedWritten = 0;
		uint32 cbCompressed = 0;
//		uint32 cbUncompressed = 0;
		EVoiceResult result = steamapicontext->SteamUser()->GetAvailableVoice( &cbCompressed, NULL, 0 );
		if ( result == k_EVoiceResultOK )
		{
			result = steamapicontext->SteamUser()->GetVoice( true, pchDest, nCount, &cbCompressedWritten, false, NULL, 0, NULL, 0 );

			g_pSoundServices->OnChangeVoiceStatus( -3, GET_ACTIVE_SPLITSCREEN_SLOT(), true );
		}
		else
		{
			g_pSoundServices->OnChangeVoiceStatus( -3, GET_ACTIVE_SPLITSCREEN_SLOT(), false );
		}
		if ( pOutFormat )
		{
			*pOutFormat = VoiceFormat_Steam;
		}

		if ( cbCompressedWritten > 0 )
		{
			s_nRecordingSectionCompressedByteOffset += cbCompressedWritten;

			if ( pnOutSectionNumber )
				*pnOutSectionNumber = s_nRecordingSection;
			if ( pnOutSectionSequenceNumber )
				*pnOutSectionSequenceNumber = s_nRecordingSectionCompressedByteOffset;

			// !FIXME! Uncompressed sample offset doesn't work right now with the Steam codec.
			// We'd have to get the uncompressed audio in order to advance it properly.
			//if ( pnOutUncompressedSampleOffset )
			//	*pnOutUncompressedSampleOffset = s_nRecordingTimestamp_UncompressedSampleOffset;
			// s_nRecordingTimestamp_UncompressedSampleOffset += xxxxx
		}

		return cbCompressedWritten;
	}

	IVoiceCodec *pCodec = g_pEncodeCodec;
	if( g_pVoiceRecord && pCodec )
	{

		static ConVarRef voice_vox( "voice_vox" );
		static ConVarRef voice_chat_bubble_show_volume( "voice_chat_bubble_show_volume" );
		static ConVarRef voice_vox_current_peak( "voice_vox_current_peak" );

		// Get uncompressed data from the recording device
		short tempData[8192];
		int samplesWanted = MIN(nCount/BYTES_PER_SAMPLE, (int)sizeof(tempData)/BYTES_PER_SAMPLE);
		int gotten = g_pVoiceRecord->GetRecordedData(tempData, samplesWanted);

		// If they want to get the data from a file instead of the mic, use that.
		if(g_pMicInputFileData)
		{
			int nShouldGet = (flNow - g_MicStartTime) * Voice_SamplesPerSec();
			gotten = MIN(sizeof(tempData)/BYTES_PER_SAMPLE, MIN(nShouldGet, (g_nMicInputFileBytes - g_CurMicInputFileByte) / BYTES_PER_SAMPLE));
			memcpy(tempData, &g_pMicInputFileData[g_CurMicInputFileByte], gotten*BYTES_PER_SAMPLE);
			g_CurMicInputFileByte += gotten * BYTES_PER_SAMPLE;
			g_MicStartTime = flNow;
		}

		// Check for detecting levels
		if ( !g_pMicInputFileData && gotten && ( voice_vox.GetBool() || g_VoiceTweakAPI.IsStillTweaking() || voice_chat_bubble_show_volume.GetBool() ) )
		{
			// TERROR: If the voice data is essentially silent, don't transmit
			short *pData = tempData;
			int averageData = 0;
			int minData = 16384;
			int maxData = -16384;
			for ( int i=0; i<gotten; ++i )
			{
				short val = *pData;
				averageData += val;
				minData = MIN( val, minData );
				maxData = MAX( val, maxData );
				++pData;
			}
			averageData /= gotten;
			int deltaData = maxData - minData;

			voice_vox_current_peak.SetValue( deltaData );

			if ( voice_vox.GetBool() || g_VoiceTweakAPI.IsStillTweaking() )
			{
				if ( deltaData < voice_threshold.GetFloat() )
				{
					if ( s_flThresholdDecayTime < flNow )
					{
						g_pSoundServices->OnChangeVoiceStatus( -1, GET_ACTIVE_SPLITSCREEN_SLOT(), false );

						// End the current section, if any
						VoiceRecord_MarkSectionBoundary();
						s_nRecordingTimestamp_UncompressedSampleOffset += gotten;
						return 0;
					}
				}
				else
				{
					g_pSoundServices->OnChangeVoiceStatus( -1, GET_ACTIVE_SPLITSCREEN_SLOT(), true );

					// Pad out our threshold clipping so words aren't clipped together
					s_flThresholdDecayTime = flNow + voice_threshold_delay.GetFloat();
				}
			}
		}

#ifdef VOICE_SEND_RAW_TEST
		int nCompressedBytes = MIN( gotten, nCount );
		for ( int i=0; i < nCompressedBytes; i++ )
		{
			pchDest[i] = (char)(tempData[i] >> 8);
		}
#else			
		int nCompressedBytes = pCodec->Compress((char*)tempData, gotten, pchDest, nCount, !!bFinal);
#endif

		// Write to our file buffers..
		if(g_pUncompressedFileData)
		{
			int nToWrite = MIN(gotten*BYTES_PER_SAMPLE, MAX_WAVEFILEDATA_LEN - g_nUncompressedDataBytes);
			memcpy(&g_pUncompressedFileData[g_nUncompressedDataBytes], tempData, nToWrite);
			g_nUncompressedDataBytes += nToWrite;
		}

		// TERROR: -3 signals that we're talking
		// !FIXME! @FD: I think this is wrong.  it's possible for us to get some data, but just
		// not have enough for the compressor to spit out a packet.  But I'm afraid to make this
		// change so close to TI, so I'm just making a note in case we revisit this.  I'm
		// not sure that it matters.
		g_pSoundServices->OnChangeVoiceStatus( -3, GET_ACTIVE_SPLITSCREEN_SLOT(), (nCompressedBytes > 0) );
		if ( pOutFormat )
		{
			*pOutFormat = VoiceFormat_Engine;
		}

		if ( nCompressedBytes > 0 )
		{
			s_nRecordingSectionCompressedByteOffset += nCompressedBytes;

			if ( pnOutSectionNumber )
				*pnOutSectionNumber = s_nRecordingSection;
			if ( pnOutSectionSequenceNumber )
				*pnOutSectionSequenceNumber = s_nRecordingSectionCompressedByteOffset;
			if ( pnOutUncompressedSampleOffset )
				*pnOutUncompressedSampleOffset = s_nRecordingTimestamp_UncompressedSampleOffset;
		}

		// Advance uncompressed sample number.  Note that if we feed a small number of samples into the compressor,
		// it might not actually return compressed data, until we hit a complete packet.
		// !KLUDGE! Here we are assuming a specific compression properties!
		if ( g_bIsSpeex )
		{
			// speex compresses 160 samples into 20 bytes with our settings (quality 4, which is quality 6 internally)
			int nPackets = nCompressedBytes / 20;
			Assert( nCompressedBytes == nPackets * 20 );
			s_nRecordingTimestamp_UncompressedSampleOffset += nPackets*160;
		}
		else
		{
			// celt compresses 512 samples into 64 bytes with our settings
			int nPackets = nCompressedBytes / 64;
			Assert( nCompressedBytes == nPackets * 64 );
			s_nRecordingTimestamp_UncompressedSampleOffset += nPackets*512;
		}

		// If they are telling us this is the last packet (and they are about to stop recording),
		// then believe them
		if ( bFinal )
			VoiceRecord_MarkSectionBoundary();

		return nCompressedBytes;
	}
	else
	{
		// TERROR: -3 signals that we're silent
		g_pSoundServices->OnChangeVoiceStatus( -3, GET_ACTIVE_SPLITSCREEN_SLOT(), false );
		VoiceRecord_MarkSectionBoundary();
		return 0;
	}
}


//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
// Purpose: Assigns a channel to an entity by searching for either a channel
//			already assigned to that entity or picking the least recently used
//			channel. If the LRU channel is picked, it is flushed and all other
//			channels are aged.
// Input  : nEntity - entity number to assign to a channel.
// Output : A channel index to which the entity has been assigned.
//------------------------------------------------------------------------------
int Voice_AssignChannel(int nEntity, bool bProximity, bool bCaster, float timePadding )
{
	// See if a channel already exists for this entity and if so, just return it.
	int iFree = -1;
	for(int i=0; i < VOICE_NUM_CHANNELS; i++)
	{
		CVoiceChannel *pChannel = &g_VoiceChannels[i];

		if(pChannel->m_iEntity == nEntity)
		{
			return i;
		}
		else if(pChannel->m_iEntity == -1 && pChannel->m_pVoiceCodec)
		{
			pChannel->m_pVoiceCodec->ResetState();
			iFree = i;
			break;
		}
	}

	// If they're all used, then don't allow them to make a new channel.
	if(iFree == -1)
	{
		return VOICE_CHANNEL_ERROR;
	}

	CVoiceChannel *pChannel = &g_VoiceChannels[iFree];
	pChannel->Init( nEntity, timePadding, bCaster );
	pChannel->m_bProximity = bProximity;
	VoiceSE_StartOverdrive();

	return iFree;
}


//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
// Purpose: Determines which channel has been assigened to a given entity.
// Input  : nEntity - entity number.
// Output : The index of the channel assigned to the entity, VOICE_CHANNEL_ERROR
//			if no channel is currently assigned to the given entity.
//------------------------------------------------------------------------------
int Voice_GetChannel(int nEntity)
{
	for(int i=0; i < VOICE_NUM_CHANNELS; i++)
		if(g_VoiceChannels[i].m_iEntity == nEntity)
			return i;

	return VOICE_CHANNEL_ERROR;
}


static void UpsampleIntoBuffer(
	const short *pSrc,
	int nSrcSamples,
	CCircularBuffer *pBuffer,
	int nDestSamples)
{
	if ( nDestSamples == nSrcSamples )
	{
		// !FIXME! This function should accept a const pointer!
		pBuffer->Write( const_cast<short*>( pSrc ), nDestSamples*sizeof(short) );
	}
	else
	{
		for ( int i = 0 ; i < nDestSamples ; ++i )
		{
			double flSrc = (double)nSrcSamples * i / nDestSamples;
			int iSample = (int)flSrc;
			double frac = flSrc - floor(flSrc);
			int iSampleNext = Min( iSample + 1, nSrcSamples - 1 );

			double val1 = pSrc[iSample];
			double val2 = pSrc[iSampleNext];
			short newSample = (short)(val1 + (val2 - val1) * frac);
			pBuffer->Write(&newSample, sizeof(newSample));
		}
	}
}

//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
// Purpose: Adds received voice data to 
// Input  : 
// Output : 
//------------------------------------------------------------------------------

void Voice_AddIncomingData(
	int nChannel,
	const char *pchData,
	int nCount,
	uint8 nSectionNumber,
	uint32 nSectionSequenceNumber,
	uint32 nUncompressedSampleOffset,
	VoiceFormat_t format
) {
	CVoiceChannel *pChannel;

	if((pChannel = GetVoiceChannel(nChannel)) == NULL || !pChannel->m_pVoiceCodec)
	{
		return;
	}

	if ( voice_showincoming.GetBool() )
	{
		Msg( "%.2f: Received voice channel=%2d: section=%4d seq=%8d time=%8d bytes=%4d, buffered=%5d\n",
			Plat_FloatTime(), nChannel, nSectionNumber, nSectionSequenceNumber, nUncompressedSampleOffset, nCount, pChannel->m_Buffer.GetReadAvailable() );
	}

	// Get byte offset at the *start* of the packet.
	uint32 nPacketByteOffsetWithinSection = nSectionSequenceNumber - (uint32)nCount;

	// If we have previously been starved, but now are adding more data,
	// then we need to reset the buffer back to a good state.  Don't try
	// to fill it up now.  What should ordinarily happen when the buffer
	// gets starved is that we should kill the channel, and any new data that
	// comes in gets assigned a new channel.  But if this channel is marked
	// as having gotten starved out, and we are adding new data to it, then
	// we have not yet killed it.  So just insert some silence.
	bool bFillWithSilenceToCatchUp = false;
	if ( pChannel->m_bStarved )
	{
		if ( voice_showincoming.GetBool() )
		{
			Warning( "%.2f: Received voice channel=%2d: section=%4d seq=%8d time=%8d bytes=%4d reusing buffer after starvation.  Padding with silence to reset buffering.\n",
			Plat_FloatTime(), nChannel, nSectionNumber, nSectionSequenceNumber, nUncompressedSampleOffset, nCount );
		}

		bFillWithSilenceToCatchUp = true;
	}

	// Check section and sequence numbers, see if there was a dropped packet or maybe a gap of silence that was not transmitted
	int nSampleOffsetGap = 0; // NOTE: measured in uncompressed rate (samplespersec below), NOT the data rate we send to the mixer, which is VOICE_OUTPUT_SAMPLE_RATE
	int nLostBytes = 0;
	if ( nSectionNumber != 0 ) // new format message?  (This will be zero on matches before around 7/11/2014)
	{
		if ( nSectionNumber != pChannel->m_nCurrentSection )
		{
			pChannel->m_nExpectedCompressedByteOffset = 0;
			pChannel->m_pVoiceCodec->ResetState();
		}

		// Check if the sample pointer is not the exact next thing we expected, then we might need to insert some silence.
		// We'll handle the fact that the gap might have been due to a lost packet and not silence, and other degenerate
		// cases, later
		nSampleOffsetGap = nUncompressedSampleOffset - pChannel->m_nExpectedUncompressedSampleOffset;
	}
	else
	{
		Assert( nUncompressedSampleOffset == 0 ); // section number and uncompressed sample offset were added in the same protocol change.  How could we have one without the other?
	}

	// If this is the first packet, or we were starved and getting rebooted, then
	// force a reset.  Otherwise, check if we lost a packet
	if ( pChannel->m_bStarved || pChannel->m_bFirstPacket )
	{
		pChannel->m_pVoiceCodec->ResetState();
		nLostBytes = 0;
		nSampleOffsetGap = 0;
	}
	else if ( pChannel->m_nExpectedCompressedByteOffset != nPacketByteOffsetWithinSection )
	{
		if ( nSectionSequenceNumber != 0 ) // old voice packets don't have sequence numbers
			nLostBytes = nPacketByteOffsetWithinSection - pChannel->m_nExpectedCompressedByteOffset;

		// Check if the sequence number is significantly out of whack, then something went
		// pretty badly wrong, or we have a bug.  Don't try to handle this gracefully,
		// just insert a little silence, and reset
		if ( nLostBytes < 0 || nLostBytes > nCount*4 + 1024 )
		{
			Warning( "%.2f: Received voice channel=%2d: section=%4d seq=%8d time=%8d bytes=%4d LOST %d bytes?  (Offset %d, expected %d)\n",
				Plat_FloatTime(), nChannel, nSectionNumber, nSectionSequenceNumber, nUncompressedSampleOffset, nCount,
				nLostBytes, pChannel->m_nExpectedCompressedByteOffset, nPacketByteOffsetWithinSection );
			nLostBytes = 0;
			pChannel->m_pVoiceCodec->ResetState();
			bFillWithSilenceToCatchUp = true;
		}
		else
		{
			// Sequence number skipped by a reasonable amount, indicating a small amount of lost data,
			// which is totally normal.  Only spew if we're debugging this.
			if ( voice_showincoming.GetBool() )
			{
				Warning( "      LOST %d bytes.  (Expected %u, got %u)\n", nLostBytes, pChannel->m_nExpectedCompressedByteOffset, nPacketByteOffsetWithinSection );
			}
		}
	}

	// Decompress.
	short decompressedBuffer[11500];
	COMPILE_TIME_ASSERT( BYTES_PER_SAMPLE == sizeof(decompressedBuffer[0]) );
	int nDecompressedSamplesForDroppedPacket = 0;
	int nDecompressedSamplesForThisPacket = 0;

#ifdef VOICE_SEND_RAW_TEST
		for ( int i=0; i < nCount; i++ )
			decompressedBuffer[i] = pchData[i] << 8;
		nDecompressedSamplesForThisPacket = nCount

#else

	const int nDesiredSampleRate = g_bIsSpeex ? VOICE_OUTPUT_SAMPLE_RATE_SPEEX : VOICE_OUTPUT_SAMPLE_RATE;

	int samplesPerSec;

	if ( format == VoiceFormat_Steam )
	{
		uint32 nBytesWritten = 0;
		EVoiceResult result = steamapicontext->SteamUser()->DecompressVoice( pchData, nCount, decompressedBuffer, sizeof(decompressedBuffer), &nBytesWritten, nDesiredSampleRate );

		if ( result == k_EVoiceResultOK )
		{
			nDecompressedSamplesForThisPacket = nBytesWritten / BYTES_PER_SAMPLE;
		}
		else
		{
			Warning( "%.2f: Voice_AddIncomingData channel %d Size %d failed to decompress steam data result %d\n", Plat_FloatTime(), nChannel, nCount, result );
		}

		samplesPerSec = nDesiredSampleRate;
	}
	else
	{

		char *decompressedDest = (char*)decompressedBuffer;
		int nDecompressBytesRemaining = sizeof(decompressedBuffer);

		// First, if we lost some data, let the codec know.
		if ( nLostBytes > 0 )
		{
			nDecompressedSamplesForDroppedPacket = pChannel->m_pVoiceCodec->Decompress( NULL, nLostBytes, decompressedDest, nDecompressBytesRemaining );
			int nDecompressedBytesForDroppedPacket = nDecompressedSamplesForDroppedPacket * BYTES_PER_SAMPLE;
			decompressedDest += nDecompressedBytesForDroppedPacket;
			nDecompressBytesRemaining -= nDecompressedBytesForDroppedPacket;
		}

		// Now decompress the actual data
		nDecompressedSamplesForThisPacket = pChannel->m_pVoiceCodec->Decompress( pchData, nCount, decompressedDest, nDecompressBytesRemaining );
		if ( nDecompressedSamplesForThisPacket <= 0 )
		{
			Warning( "%.2f: Voice_AddIncomingData channel %d Size %d engine failed to decompress\n", Plat_FloatTime(), nChannel, nCount );
			nDecompressedSamplesForThisPacket = 0;
		}
		samplesPerSec = g_VoiceSampleFormat.nSamplesPerSec;
		EngineTool_OverrideSampleRate( samplesPerSec );
	}

#endif

	int nDecompressedSamplesTotal = nDecompressedSamplesForDroppedPacket + nDecompressedSamplesForThisPacket;
	int nDecompressedBytesTotal = nDecompressedSamplesTotal * BYTES_PER_SAMPLE;

	pChannel->m_GainManager.Apply( decompressedBuffer, nDecompressedSamplesTotal, pChannel->m_bCaster );

	// We might need to fill with some silence.  Calculate the number of samples we need to fill.
	// Note that here we need to be careful not to confuse the network transmission reference
	// rate with the rate of data sent to the mixer.  (At the time I write this, they are the same,
	// but that might change in the future.)
	int nSamplesOfSilenceToInsertToMixer = 0; // mixer rate
	if ( nSampleOffsetGap != 0 )
	{

		//
		// Check for some things going way off the rails
		//

		// If it's already negative, then something went haywire.
		if ( nSampleOffsetGap < 0 )
		{
			// This is weird.  The sample number moved backwards.
			Warning( "%.2f: Received voice channel=%2d: section=%4d seq=%8d time=%8d bytes=%4d, timestamp moved backwards (%d).  Expected %u, received %u.\n",
				Plat_FloatTime(), nChannel, nSectionNumber, nSectionSequenceNumber, nUncompressedSampleOffset, nCount,
				nSampleOffsetGap, pChannel->m_nExpectedCompressedByteOffset, nUncompressedSampleOffset );
		}
		else
		{
			// If we dropped a packet, this would totally explain the gap.
			nSampleOffsetGap -= nDecompressedSamplesForDroppedPacket;
			if ( nSampleOffsetGap < 0 )
			{
				Warning( "%.2f: Received voice channel=%2d: section=%4d seq=%8d time=%8d bytes=%4d, timestamp moved backwards (%d) after synthesizing dropped packet.  Expected %u+%u = %u, received %u.\n",
					Plat_FloatTime(), nChannel, nSectionNumber, nSectionSequenceNumber, nUncompressedSampleOffset, nCount,
					nSampleOffsetGap,
					pChannel->m_nExpectedCompressedByteOffset,
					nDecompressedSamplesForDroppedPacket,
					pChannel->m_nExpectedCompressedByteOffset + nDecompressedSamplesForDroppedPacket,
					nUncompressedSampleOffset );
			}
		}

		// Is the gap massively larger than we should reasonably expect?
		// this probably indicates something is wrong or we have a bug.
		if ( nSampleOffsetGap > VOICE_RECEIVE_BUFFER_SECONDS * samplesPerSec )
		{
			Warning( "%.2f: Received voice channel=%2d: section=%4d seq=%8d time=%8d bytes=%4d, timestamp moved backwards (%d) after synthesizing dropped packet.  Expected %u+%u = %u, received %u.\n",
				Plat_FloatTime(), nChannel, nSectionNumber, nSectionSequenceNumber, nUncompressedSampleOffset, nCount,
				nSampleOffsetGap,
				pChannel->m_nExpectedCompressedByteOffset,
				nDecompressedSamplesForDroppedPacket,
				pChannel->m_nExpectedCompressedByteOffset + nDecompressedSamplesForDroppedPacket,
				nUncompressedSampleOffset );
		}
		else if ( nSampleOffsetGap > 0 )
		{
			// A relatively small positive gap, which means we actually want to insert silence.
			// This is the normal situation.

			// Convert from the network reference rate to the mixer rate
			nSamplesOfSilenceToInsertToMixer = nSampleOffsetGap * samplesPerSec / nDesiredSampleRate;

			// Only spew about this if we're logging
			if ( voice_showincoming.GetBool() )
			{
				Msg( "    Timestamp gap of %d (%u -> %u).  Will insert %d samples of silence\n", nSampleOffsetGap, pChannel->m_nExpectedUncompressedSampleOffset, nUncompressedSampleOffset, nSamplesOfSilenceToInsertToMixer );
			}
		}
	}

	// Convert from voice decompression rate to the rate we send to the mixer.
	int nDecompressedSamplesAtMixerRate = nDecompressedSamplesTotal * samplesPerSec / nDesiredSampleRate;

	// Check current buffer state do some calculations on how much we could fit, and how
	// much would get us to our ideal amount
	int nBytesBuffered = pChannel->m_Buffer.GetReadAvailable();
	int nSamplesBuffered = nBytesBuffered / BYTES_PER_SAMPLE;
	int nMaxBytesToWrite = pChannel->m_Buffer.GetWriteAvailable();
	int nMaxSamplesToWrite = nMaxBytesToWrite / BYTES_PER_SAMPLE;
	int nSamplesNeededToReachMinDesiredLeadTime = Max( pChannel->m_nMinDesiredLeadSamples - nSamplesBuffered, 0 );
	int nSamplesNeededToReachMaxDesiredLeadTime = Max( pChannel->m_nMaxDesiredLeadSamples - nSamplesBuffered, 0 );
	int nSamplesOfSilenceMax = Max( 0, nMaxSamplesToWrite - nDecompressedSamplesAtMixerRate );
	int nSamplesOfSilenceToReachMinDesiredLeadTime = Clamp( nSamplesNeededToReachMinDesiredLeadTime - nDecompressedSamplesAtMixerRate, 0, nSamplesOfSilenceMax );
	int nSamplesOfSilenceToReachMaxDesiredLeadTime = Clamp( nSamplesNeededToReachMaxDesiredLeadTime - nDecompressedSamplesAtMixerRate, 0, nSamplesOfSilenceMax );
	Assert( nSamplesOfSilenceToReachMinDesiredLeadTime <= nSamplesOfSilenceToReachMaxDesiredLeadTime );
	Assert( nSamplesOfSilenceToReachMaxDesiredLeadTime <= nSamplesOfSilenceMax );

	// Check if something went wrong with a previous batch of audio in this buffer,
	// and we should just try to reset the buffering to a healthy position by
	// filling with silence.
	if ( bFillWithSilenceToCatchUp && nSamplesOfSilenceToReachMinDesiredLeadTime > nSamplesOfSilenceToInsertToMixer )
		nSamplesOfSilenceToInsertToMixer = nSamplesOfSilenceToReachMinDesiredLeadTime;

	// Limit silence samples
	if ( nSamplesOfSilenceToInsertToMixer > nSamplesOfSilenceMax )
		nSamplesOfSilenceToInsertToMixer = nSamplesOfSilenceMax;

	// Insert silence, if necessary
	if ( nSamplesOfSilenceToInsertToMixer > 0 )
	{
		// Check if out buffer lead time is not where we want it to be, then silence
		// is a great opportunity to stretch things a bit and get us back where we'd like.
		// This does change the timing slightly, but that is a far preferable change than
		// later the buffer draining and us outputting distorted audio.
		float kMaxStretch = 1.2f;
		if ( nSamplesOfSilenceToInsertToMixer < nSamplesOfSilenceToReachMinDesiredLeadTime )
		{
			nSamplesOfSilenceToInsertToMixer = Min( int( nSamplesOfSilenceToInsertToMixer * kMaxStretch ), nSamplesOfSilenceToReachMinDesiredLeadTime );
		}
		else if ( nSamplesOfSilenceToInsertToMixer > nSamplesOfSilenceToReachMaxDesiredLeadTime )
		{
			float kMinStretch = 1.0 / kMaxStretch;
			nSamplesOfSilenceToInsertToMixer = Max( int( nSamplesOfSilenceToInsertToMixer * kMinStretch ), nSamplesOfSilenceToReachMaxDesiredLeadTime );
		}

		if ( voice_showincoming.GetBool() )
		{
			Msg( "    Actually inserting %d samples of silence\n", nSamplesOfSilenceToInsertToMixer );
		}

		// OK, we know how much silence we're going to insert.  Before we insert silence,
		// we're going to try to make a nice transition back down to zero, in case
		// the last data didn't end near zero.  (Highly likely if we dropped a packet.)
		// This prevents a pop.

		int nDesiredSamplesToRamp = nDesiredSampleRate / 500; // 2ms
		int nSamplesToRamp = Min( nDesiredSamplesToRamp, nSamplesOfSilenceToInsertToMixer );
		for ( int i = 1 ; i <= nSamplesToRamp ; ++i ) // No need to duplicate the previous sample.  But make sure we end at zero
		{
			// Compute interpolation parameter
			float t = float(i) / float(nSamplesToRamp);

			// Smoothstep
			t = 3.0f * t*t - 2.0 * t*t*t;

			short val = short( pChannel->m_nLastSample * ( 1.0f - t ) );
			pChannel->m_Buffer.Write( &val, sizeof(val) );
		}

		// Fill with silence
		int nSilenceSamplesRemaining = nSamplesOfSilenceToInsertToMixer - nSamplesToRamp;
		short zero = 0;
		while ( nSilenceSamplesRemaining > 0 )
		{
			pChannel->m_Buffer.Write( &zero, sizeof(zero) );
			--nSilenceSamplesRemaining;
		}

		pChannel->m_nLastSample = 0;

		nSamplesNeededToReachMinDesiredLeadTime -= nSamplesOfSilenceToInsertToMixer;
		nSamplesNeededToReachMaxDesiredLeadTime -= nSamplesOfSilenceToInsertToMixer;
	}

	if ( nDecompressedSamplesTotal > 0 )
	{

		// Upsample the actual voice data into the dest buffer. We could do this in a mixer but it complicates the mixer.
		UpsampleIntoBuffer(
			decompressedBuffer, 
			nDecompressedSamplesTotal, 
			&pChannel->m_Buffer, 
			nDecompressedSamplesAtMixerRate );

		// Save off the value of the last sample, in case the next bit of data is missing and we need to transition out.
		pChannel->m_nLastSample = decompressedBuffer[nDecompressedSamplesTotal-1];

		// Write to our file buffer..
		if(g_pDecompressedFileData)
		{											  
			int nToWrite = MIN(nDecompressedSamplesTotal*BYTES_PER_SAMPLE, MAX_WAVEFILEDATA_LEN - g_nDecompressedDataBytes);
			memcpy(&g_pDecompressedFileData[g_nDecompressedDataBytes], decompressedBuffer, nToWrite);
			g_nDecompressedDataBytes += nToWrite;
		}

		g_VoiceWriter.AddDecompressedData( pChannel, (const byte *)decompressedBuffer, nDecompressedBytesTotal );
	}

	// Check if our circular buffer is totally full, then that's bad.
	// The circular buffer is a fixed size, and overflow is not
	// graceful.  This really should never happen, except when skipping a lot of frames in a demo.
	if ( pChannel->m_Buffer.GetWriteAvailable() <= 0 )
	{
		if ( demoplayer && demoplayer->IsPlayingBack() )
		{
			// well, this is normal: demo is being played back and large chunks of it may be skipped at a time
		}
		else
		{
			Warning( "Voice channel %d circular buffer overflow!\n", nChannel );
		}
	}

	// Save state for next time
	pChannel->m_nCurrentSection = nSectionNumber;
	pChannel->m_nExpectedCompressedByteOffset = nSectionSequenceNumber;
	pChannel->m_nExpectedUncompressedSampleOffset = nUncompressedSampleOffset + nDecompressedSamplesForThisPacket;
	pChannel->m_bFirstPacket = false;
	pChannel->m_bStarved = false;	// This only really matters if you call Voice_AddIncomingData between the time the mixer
									// asks for data and Voice_Idle is called.
}



#if DEAD
//------------------ Copyright (c) 1999 Valve, LLC. ----------------------------
// Purpose: Flushes a given receive channel.
// Input  : nChannel - index of channel to flush.
//------------------------------------------------------------------------------
void Voice_FlushChannel(int nChannel)
{
	if ((nChannel < 0) || (nChannel >= VOICE_NUM_CHANNELS))
	{
		Assert(false);
		return;
	}

	g_VoiceChannels[nChannel].m_Buffer.Flush();
}
#endif


//------------------------------------------------------------------------------
// IVoiceTweak implementation.
//------------------------------------------------------------------------------

int VoiceTweak_StartVoiceTweakMode()
{
	// If we're already in voice tweak mode, return an error.
	if ( g_bInTweakMode )
	{
		Assert(!"VoiceTweak_StartVoiceTweakMode called while already in tweak mode.");
		return 0;
	}

	if ( g_pEncodeCodec == NULL )
	{
		Voice_Init( sv_voicecodec.GetString(), VOICE_CURRENT_VERSION );
	}

	g_bInTweakMode = true;
	Voice_RecordStart(NULL, NULL, NULL);

	return 1;
}

void VoiceTweak_EndVoiceTweakMode()
{
	if(!g_bInTweakMode)
	{
		Assert(!"VoiceTweak_EndVoiceTweakMode called when not in tweak mode.");
		return;
	}

	static ConVarRef voice_vox( "voice_vox" );

	if ( !voice_vox.GetBool() )
	{
		Voice_RecordStop();
	}

	g_bInTweakMode = false;
}

void VoiceTweak_SetControlFloat(VoiceTweakControl iControl, float flValue)
{
	if(!g_pMixerControls)
		return;

	if(iControl == MicrophoneVolume)
	{
		g_pMixerControls->SetValue_Float(IMixerControls::MicVolume, flValue);
	}
	else if ( iControl == MicBoost )
	{
		g_pMixerControls->SetValue_Float( IMixerControls::MicBoost, flValue );
	}
	else if(iControl == OtherSpeakerScale)
	{
		voice_scale.SetValue( flValue );

		// this forces all voice channels to use the new voice_scale value instead of waiting for the next network update
		for(int i=0; i < VOICE_NUM_CHANNELS; i++)
		{
			CVoiceChannel *pChannel = &g_VoiceChannels[i];
			if ( pChannel && pChannel->m_iEntity > -1 )
			{
				pChannel->Init( pChannel->m_iEntity, pChannel->m_TimePad );
			}
		}
	}
}

void Voice_ForceInit()
{
	if ( voice_system_enable.GetBool())
	{
		Voice_Init( sv_voicecodec.GetString(), VOICE_CURRENT_VERSION );
	}
}

float VoiceTweak_GetControlFloat(VoiceTweakControl iControl)
{
	if (!g_pMixerControls && voice_system_enable.GetBool())
	{
		Voice_Init( sv_voicecodec.GetString(), VOICE_CURRENT_VERSION );
	}

	if(!g_pMixerControls)
		return 0;

	if(iControl == MicrophoneVolume)
	{
		float value = 1;
		g_pMixerControls->GetValue_Float(IMixerControls::MicVolume, value);
		return value;
	}
	else if(iControl == OtherSpeakerScale)
	{
		return voice_scale.GetFloat();
	}
	else if(iControl == SpeakingVolume)
	{
		return g_VoiceTweakSpeakingVolume * 1.0f / 32768;
	}
	else if ( iControl == MicBoost )
	{
		float flValue = 1;
		g_pMixerControls->GetValue_Float( IMixerControls::MicBoost, flValue );
		return flValue;
	}
	else
	{
		return 1;
	}
}

bool VoiceTweak_IsStillTweaking()
{
	return g_bInTweakMode;
}

bool VoiceTweak_IsControlFound(VoiceTweakControl iControl)
{
	if (!g_pMixerControls && voice_system_enable.GetBool())
	{
		Voice_Init( sv_voicecodec.GetString(), VOICE_CURRENT_VERSION );
	}

	if(!g_pMixerControls)
		return false;

	if(iControl == MicrophoneVolume)
	{
		float fDummy;
		return g_pMixerControls->GetValue_Float(IMixerControls::MicVolume,fDummy);
	}

	return true;
}

void Voice_Spatialize( channel_t *channel )
{
	// do nothing now
}

IVoiceTweak g_VoiceTweakAPI =
{
	VoiceTweak_StartVoiceTweakMode,
	VoiceTweak_EndVoiceTweakMode,
	VoiceTweak_SetControlFloat,
	VoiceTweak_GetControlFloat,
	VoiceTweak_IsStillTweaking,
	VoiceTweak_IsControlFound,
};


