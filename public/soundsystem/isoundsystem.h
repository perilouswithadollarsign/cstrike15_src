//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef ISOUNDSYSTEM_H
#define ISOUNDSYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "appframework/iappsystem.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IAudioDevice;
class CAudioSource;
class CAudioMixer;


//-----------------------------------------------------------------------------
// Sound handle
//-----------------------------------------------------------------------------
typedef unsigned short AudioSourceHandle_t;
enum
{
	AUDIOSOURCEHANDLE_INVALID = (AudioSourceHandle_t)~0
};


//-----------------------------------------------------------------------------
// Flags for FindAudioSource
//-----------------------------------------------------------------------------
enum FindAudioSourceFlags_t
{
	FINDAUDIOSOURCE_NODELAY = 0x1,
	FINDAUDIOSOURCE_PREFETCH = 0x2,
	FINDAUDIOSOURCE_PLAYONCE = 0x4,
};

#include "soundsystem/audio_mix.h"

/* filter types */
enum audio_filter_type_t
{
	FILTER_LOWPASS = 0, /* low pass filter */
	FILTER_HIGHPASS, /* High pass filter */
	FILTER_BANDPASS, /* band pass filter */
	FILTER_NOTCH, /* Notch Filter */
	FILTER_PEAKING_EQ, /* Peaking band EQ filter */
	FILTER_LOW_SHELF, /* Low shelf filter */
	FILTER_HIGH_SHELF /* High shelf filter */
};

class IAudioMix
{
public:
	virtual ~IAudioMix() {}
	virtual void Process( CAudioMixState *pState ) = 0;

	CAudioMixBuffer *m_pOutput;
	int				m_nOutputChannelCount;
};

class CAudioMixState;
class CAudioMixDescription;

abstract_class ISoundSystem2
{
public:
	// NOTE: This is the new sound device architecture.  It is a separate standalone piece of tech that does not interact with ISoundSystem's normal
	// entry points.  Eventually these entry points will replace many of the entry points of ISoundSystem and this interface will transition to 
	// the new architecture.  This is just here for prototyping and testing.

	// NULL chooses default
	virtual IAudioDevice2 *CreateDevice( const audio_device_init_params_t *pParams ) = 0;

	virtual void DestroyDevice( IAudioDevice2 *pDevice ) = 0;
	virtual int EnumerateDevices( int nSubsystem, audio_device_description_t *pDeviceListOut, int nListCount ) = 0;
	virtual void HandleDeviceErrors( IAudioDevice2 *pDevice ) = 0;

	virtual IAudioMix *CreateMix( const CAudioMixDescription *pMixDescription ) = 0;
	virtual void DestroyMix( IAudioMix *pMix ) = 0;
	virtual CAudioProcessor *CreateFilter( audio_filter_type_t filterType, float fldbGain, float flCenterFrequency, float flBandWidth ) = 0;
	virtual CAudioProcessor *CreateMonoDSP( int nEffect, dspglobalvars_t *pGlobals ) = 0;
};

//-----------------------------------------------------------------------------
// Purpose: DLL interface for low-level sound utilities
//-----------------------------------------------------------------------------
#define SOUNDSYSTEM_INTERFACE_VERSION "SoundSystem001"

abstract_class ISoundSystem : public IAppSystem
{
public:
	virtual void		Update( float time ) = 0;
	virtual void		Flush( void ) = 0;

	virtual CAudioSource *FindOrAddSound( const char *filename ) = 0;
	virtual CAudioSource *LoadSound( const char *wavfile ) = 0;

	virtual void		PlaySound( CAudioSource *source, float volume, CAudioMixer **ppMixer ) = 0;

	virtual bool		IsSoundPlaying( CAudioMixer *pMixer ) = 0;
	virtual CAudioMixer *FindMixer( CAudioSource *source ) = 0;

	virtual void		StopAll( void ) = 0;
	virtual void		StopSound( CAudioMixer *mixer ) = 0;

	virtual void		GetAudioDevices(CUtlVector< audio_device_description_t >& deviceListOut) const = 0;
};



#endif // ISOUNDSYSTEM_H
