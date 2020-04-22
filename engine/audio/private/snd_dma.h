//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SND_DMA_H
#define SND_DMA_H
#ifdef _WIN32
#pragma once
#endif


extern ConVar snd_obscured_gain_db;
extern ConVar snd_showstart;
extern ConVar snd_refdb;
extern ConVar snd_refdist;
extern float snd_refdb_dist_mult;
extern bool snd_initialized;
extern int g_snd_trace_count;

// convert sound db level to approximate sound source radius,
// used only for determining how much of sound is obscured by world

#define SND_RADIUS_MAX		(20.0 * 12.0)	// max sound source radius
#define SND_RADIUS_MIN		(2.0 * 12.0)	// min sound source radius

#define SND_DB_MAX				140.0	// max db of any sound source
#define SND_DB_MED				90.0	// db at which compression curve changes
#define SND_DB_MIN				60.0	// min db of any sound source

inline float dB_To_Radius ( float db )
{
	return SND_RADIUS_MIN + (SND_RADIUS_MAX - SND_RADIUS_MIN) * (db - SND_DB_MIN) / (SND_DB_MAX - SND_DB_MIN);
}


class CScratchPad;
const int MASK_BLOCK_AUDIO = CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW;

#define SNDLVL_TO_DIST_MULT( sndlvl ) ( sndlvl ? ((snd_refdb_dist_mult / FastPow10( (float)sndlvl / 20 )) / snd_refdist.GetFloat()) : 0 )
#define DIST_MULT_TO_SNDLVL( dist_mult ) (soundlevel_t)(int)( dist_mult ? ( 20 * log10( (float)(snd_refdb_dist_mult / (dist_mult * snd_refdist.GetFloat()) )) ) : 0 )

float SND_GetGainFromMult( float gain, float dist_mult, vec_t dist );
float SND_GetDspMix( channel_t *pchannel, int idist, float flSndlvl );
bool SND_ChannelOkToTrace( channel_t *ch );
float SND_GetGainObscured( int nSlot, gain_t *gs, const channel_t *ch, const Vector &vecListenerOrigin, bool fplayersound, bool flooping, bool bAttenuated, bool bOkayToTrace, Vector *pOrigin = NULL );
float S_GetDashboarMusicMixValue( );
float SND_GetFacingDirection( channel_t *pChannel, const Vector &vecListenerOrigin, const QAngle &source_angles );
void SND_MergeVolumes( const float build_volumes[ MAX_SPLITSCREEN_CLIENTS ][CCHANVOLUMES/2], float volumes[CCHANVOLUMES/2] );
void ConvertListenerVectorTo2D( Vector *pvforward, const Vector *pvright );
void ChannelSetVolTargets( channel_t *pch, float *pvolumes, int ivol_offset, int cvol );

void S_StartSoundEntryByHash( HSOUNDSCRIPTHASH nHandle );
channel_t *S_FindChannelByScriptHash( HSOUNDSCRIPTHASH nHandle );
void S_StopChannel( channel_t *pChannel );

channel_t *S_FindChannelByGuid( int guid );

bool SND_IsInGame();
float SND_FadeToNewGain( gain_t *gs, const channel_t *ch, float gain_new );
bool SND_IsLongWave( channel_t pChannel );
float dB_To_Gain( float );



#endif // SND_DMA_H
