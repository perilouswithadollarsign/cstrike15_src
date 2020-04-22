//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: This abstracts the various hardware dependent implementations of sound
//			At the time of this writing there are Windows WAVEOUT, Direct Sound,
//			and Null implementations.
//
//=====================================================================================//

#ifndef SND_DEVICE_H
#define SND_DEVICE_H
#pragma once

#include "snd_fixedint.h"
#include "snd_mix_buf.h"

// sound engine rate defines
#if 0 // def _PS3
#define SOUND_DMA_SPEED		48000		// hardware playback rate
#else
#define SOUND_DMA_SPEED		44100		// hardware playback rate
#endif

#define SOUND_11k			11025		// 11khz sample rate
#define SOUND_22k			22050		// 22khz sample rate
#define SOUND_44k			44100		// 44khz sample rate
#define SOUND_ALL_RATES		1			// mix all sample rates
 
#define SOUND_MIX_WET		0			// mix only samples that don't have channel set to 'dry' or 'speaker' (default)
#define SOUND_MIX_DRY		1			// mix only samples with channel set to 'dry' (ie: music)
#define SOUND_MIX_SPEAKER	2			// mix only samples with channel set to 'speaker'

#define	SOUND_BUSS_ROOM			(1<<0)		// mix samples using channel dspmix value (based on distance from player)
#define SOUND_BUSS_FACING		(1<<1)		// mix samples using channel dspface value (source facing)
#define	SOUND_BUSS_FACINGAWAY	(1<<2)		// mix samples using 1-dspface
#define SOUND_BUSS_SPEAKER		(1<<3)		// mix ch->bspeaker samples in mono to speaker buffer
#define SOUND_BUSS_DRY			(1<<4)		// mix ch->bdry samples into dry buffer

class Vector;
struct channel_t;

#if defined(_WIN32) || defined(_WIN64)
#define USE_AUDIO_DEVICE_V1 1
#endif

#if USE_AUDIO_DEVICE_V1
// General interface to an audio device
abstract_class IAudioDevice
{
public:
	virtual ~IAudioDevice() {}

	// Detect the sound hardware and create a compatible device
	// NOTE: This should NEVER fail.  There is a function called Audio_GetNullDevice
	// which will create a "null" device that makes no sound.  If we can't create a real 
	// sound device, this will return a device of that type.  All of the interface
	// functions can be called on the null device, but it will not, of course, make sound.
	static IAudioDevice *AutoDetectInit();

	// This initializes the sound hardware.  true on success, false on failure
	virtual bool		Init( void ) = 0;
	// This releases all sound hardware
	virtual void		Shutdown( void ) = 0;
	// stop outputting sound, but be ready to resume on UnPause
	virtual void		Pause( void ) = 0;
	// return to normal operation after a Pause()
	virtual void		UnPause( void ) = 0;

	// Called before painting channels, must calculated the endtime and return it (once per frame)
	virtual int64		PaintBegin( float, int64 soundtime, int64 paintedtime ) = 0;
	virtual void PaintEnd() {}

	// replaces SNDDMA_GetDMAPos, gets the output sample position for tracking
	virtual int			GetOutputPosition( void ) = 0;

	// Fill the output buffer with silence (e.g. during pause)
	virtual void		ClearBuffer( void ) = 0;

	virtual void		TransferSamples( int end ) = 0;

	// device parameters
	virtual int			DeviceSampleCount( void ) = 0;	// Total samples in buffer

	inline const char *Name() { return m_pName; }
	inline int ChannelCount() { return m_nChannels; }
	inline int BitsPerSample() { return m_nSampleBits; }
	inline int SampleRate() { return m_nSampleRate; }

	virtual bool IsSurround() { return m_nChannels > 2 ? true : false; }
	virtual bool IsSurroundCenter() { return m_nChannels > 4 ? true : false; }
	inline bool IsActive() { return m_bIsActive; }
	inline bool IsHeadphone() { return m_bIsHeadphone; } // mixing makes some choices differently for stereo vs headphones, expose that here.

	inline int	DeviceSampleBytes() { return BitsPerSample() / 8; }

protected:
	// NOTE: Derived classes MUST initialize these before returning a device from a factory
	const char *m_pName;
	int			m_nChannels;
	int			m_nSampleBits;
	int			m_nSampleRate;
	bool		m_bIsActive;
	bool		m_bIsHeadphone;
};

extern IAudioDevice *Audio_GetNullDevice( void );
#else
#include "soundsystem/lowlevel.h"
inline IAudioDevice2 *Audio_GetNullDevice()
{
	return Audio_CreateNullDevice();
}
#endif

#endif // SND_DEVICE_H
