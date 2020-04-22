//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Device Common Routines
//
//=====================================================================================//

#ifndef SND_DEV_COMMON_H
#define SND_DEV_COMMON_H
#pragma once

void DEBUG_DrawPanCurves(void);
#if USE_AUDIO_DEVICE_V1

class CAudioDeviceBase : public IAudioDevice
{
public:	
	virtual bool		IsActive( void ) { return false; }
	virtual bool		Init( void ) { return false; }
	virtual void		Shutdown( void ) {}
	virtual void		Pause( void ) {}
	virtual void		UnPause( void ) {}

	virtual int64		PaintBegin( float, int64 soundtime, int64 paintedtime ) { return 0; }
	virtual void		PaintEnd( void ) {}

	virtual int			GetOutputPosition( void ) { return 0; }
	virtual void		ClearBuffer( void ) {}


	// virtual void		TransferSamples( int64 end ) {}
	
	virtual int			DeviceSampleCount( void )	{ return 0; }
};
#endif

extern void		Device_MixUpsample( int sampleCount, int filtertype );
extern void		Device_ApplyDSPEffects( int idsp, portable_samplepair_t *pbuffront, portable_samplepair_t *pbufrear, portable_samplepair_t *pbufcenter, int samplecount );
extern void		Device_SpatializeChannel( int nSlot, float volume[CCHANVOLUMES/2], int master_vol, const Vector& sourceDir, float gain, float mono, int nWavType );
extern void		Device_SpatializeChannel( int nSlot, float volume[CCHANVOLUMES/2], const Vector& sourceDir, float mono, float flRearToStereoScale = 0.75 );
extern void		Device_Mix8Mono( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );
extern void		Device_Mix8Stereo( channel_t *pChannel, char *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );
extern void		Device_Mix16Mono( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );
extern void		Device_Mix16Stereo( channel_t *pChannel, short *pData, int outputOffset, int inputOffset, fixedint rateScaleFix, int outCount, int timecompress );

#endif // SND_DEV_COMMON_H
