//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SND_WAVE_DATA_H
#define SND_WAVE_DATA_H
#ifdef _WIN32
#pragma once
#endif

#include "snd_audio_source.h"

//-----------------------------------------------------------------------------
// Purpose: Linear iterator over source data.
//			Keeps track of position in source, and maintains necessary buffers
//-----------------------------------------------------------------------------
abstract_class IWaveData
{
public:
	virtual						~IWaveData( void ) {}
	virtual CAudioSource		&Source( void ) = 0;
	virtual int					ReadSourceData( void **pData, int64 sampleIndex, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] ) = 0;
	virtual bool				IsReadyToMix() = 0;
	virtual void				UpdateLoopPosition( int nLoopPosition ) = 0;
};

abstract_class IWaveStreamSource
{
public:
	virtual int64	UpdateLoopingSamplePosition( int64 samplePosition ) = 0;
	virtual void	UpdateSamples( char *pData, int sampleCount ) = 0;
	virtual int		GetLoopingInfo( int *pLoopBlock, int *pNumLeadingSamples, int *pNumTrailingSamples ) = 0;
};

class IFileReadBinary;
class CSfxTable;

extern IWaveData *CreateWaveDataStream( CAudioSource &source, IWaveStreamSource *pStreamSource, const char *pFileName, int dataStart, int dataSize, CSfxTable *pSfx, int startOffset, int skipInitialSamples, SoundError &soundError );
extern IWaveData *CreateWaveDataMemory( CAudioSource &source );
extern IWaveData *CreateWaveDataHRTF(IWaveData* pData, hrtf_info_t* dir);

void PrefetchDataStream( const char *pFileName, int dataOffset, int dataSize );

#endif // SND_WAVE_DATA_H
