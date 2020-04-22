//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "audio_pch.h"
#include "snd_mp3_source.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CAudioSourceMP3::CAudioSourceMP3( CSfxTable *pSfx )
{
	m_sampleRate = 44100;
	m_pSfx = pSfx;
	m_refCount = 0;

	m_dataStart = 0;

	char nameBuf[MAX_PATH];
	FileHandle_t file = g_pSndIO->open( pSfx->GetFileName( nameBuf, sizeof(nameBuf) ) );
	if ( (intp)file != -1 )
	{
		m_dataSize = g_pSndIO->size( file );
		g_pSndIO->close( file );
	}
	else
	{
		m_dataSize = 0;
	}


	m_nCachedDataSize = 0;
	m_bIsPlayOnce = false;
	m_bIsSentenceWord = false;
}

CAudioSourceMP3::CAudioSourceMP3( CSfxTable *pSfx, CAudioSourceCachedInfo *info )
{
	m_sampleRate = 44100;
	m_pSfx = pSfx;
	m_refCount = 0;

	m_dataSize = info->DataSize();
	m_dataStart = info->DataStart();

	m_nCachedDataSize = 0;
	m_bIsPlayOnce = false;
}

CAudioSourceMP3::~CAudioSourceMP3()
{
}

// mixer's references
void CAudioSourceMP3::ReferenceAdd( CAudioMixer * )
{
	m_refCount++;
}

void CAudioSourceMP3::ReferenceRemove( CAudioMixer * )
{
	m_refCount--;
	if ( m_refCount == 0 && IsPlayOnce() )
	{
		SetPlayOnce( false ); // in case it gets used again
		CacheUnload();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CAudioSourceMP3::IsAsyncLoad()
{
	if ( !m_AudioCacheHandle.IsValid() )
	{
		m_AudioCacheHandle.Get( GetType(), m_pSfx->IsPrecachedSound(), m_pSfx, &m_nCachedDataSize );
	}
		
	// If there's a bit of "cached data" then we don't have to lazy/async load (we still async load the remaining data,
	//  but we run from the cache initially)
	return ( m_nCachedDataSize > 0 ) ? false : true;
}

// check reference count, return true if nothing is referencing this
bool CAudioSourceMP3::CanDelete( void )
{
	return m_refCount > 0 ? false : true;
}

bool CAudioSourceMP3::GetStartupData()
{
	char nameBuf[MAX_PATH];
	FileHandle_t file = g_pSndIO->open( m_pSfx->GetFileName( nameBuf, sizeof(nameBuf) ) );
	if ( !file )
	{
		return false;
	}

	m_dataSize = (int)g_pSndIO->size( file );
	g_pSndIO->close( file );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CAudioSourceMP3::GetType()
{
	return AUDIO_SOURCE_MP3;
}

void CAudioSourceMP3::GetCacheData( CAudioSourceCachedInfo *info )
{
	info->SetSampleRate( m_sampleRate );
	info->SetDataStart( 0 );

	GetStartupData();
	// Data size gets computed in GetStartupData!!!
	info->SetDataSize( m_dataSize );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
char const *CAudioSourceMP3::GetFileName( char *pOutBuf, size_t bufLen )
{
	return m_pSfx ? m_pSfx->GetFileName(pOutBuf, bufLen) : "NULL m_pSfx";
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAudioSourceMP3::CheckAudioSourceCache()
{
	Assert( m_pSfx );

	if ( !m_pSfx->IsPrecachedSound() )
	{
		return;
	}

	// This will "re-cache" this if it's not in this level's cache already
	m_AudioCacheHandle.Get( GetType(), true, m_pSfx, &m_nCachedDataSize );
}

//-----------------------------------------------------------------------------
// Purpose: NULL the wave data pointer (we haven't loaded yet)
//-----------------------------------------------------------------------------
CAudioSourceMP3Cache::CAudioSourceMP3Cache( CSfxTable *pSfx ) : 
	CAudioSourceMP3( pSfx )
{
	m_hCache = 0;
}

CAudioSourceMP3Cache::CAudioSourceMP3Cache( CSfxTable *pSfx, CAudioSourceCachedInfo *info ) :
	CAudioSourceMP3( pSfx, info )
{
	m_hCache = 0;

	m_dataSize = info->DataSize();
	m_dataStart = info->DataStart();
}

//-----------------------------------------------------------------------------
// Purpose: Free any wave data we've allocated
//-----------------------------------------------------------------------------
CAudioSourceMP3Cache::~CAudioSourceMP3Cache( void )
{
	CacheUnload();
}

int CAudioSourceMP3Cache::GetCacheStatus( void )
{
	bool bCacheValid;
	int loaded = wavedatacache->IsDataLoadCompleted( m_hCache, &bCacheValid ) ? AUDIO_IS_LOADED : AUDIO_NOT_LOADED;
	if ( !bCacheValid )
	{
		char nameBuf[MAX_PATH];
		wavedatacache->RestartDataLoad( &m_hCache, m_pSfx->GetFileName(nameBuf, sizeof(nameBuf)), m_dataSize, m_dataStart );
	}
	return loaded;
}


void CAudioSourceMP3Cache::CacheLoad( void )
{
	// Commence lazy load?
	if ( m_hCache != 0 )
	{
		GetCacheStatus();
		return;
	}

	char nameBuf[MAX_PATH];
	m_hCache = wavedatacache->AsyncLoadCache( m_pSfx->GetFileName(nameBuf, sizeof(nameBuf)), m_dataSize, m_dataStart );
}

void CAudioSourceMP3Cache::CacheUnload( void )
{
	if ( m_hCache != 0 )
	{
		wavedatacache->Unload( m_hCache );
	}
}

char *CAudioSourceMP3Cache::GetDataPointer( void )
{
	char *pMP3Data = NULL;
	bool dummy = false;

	if ( m_hCache == 0 )
	{
		CacheLoad();
	}
	char nameBuf[MAX_PATH];

	wavedatacache->GetDataPointer( 
		m_hCache, 
		m_pSfx->GetFileName(nameBuf, sizeof(nameBuf)), 
		m_dataSize, 
		m_dataStart, 
		(void **)&pMP3Data, 
		0, 
		&dummy );

	return pMP3Data;
}

int CAudioSourceMP3Cache::GetOutputData( void **pData, int64 samplePosition, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] )
{
	// how many bytes are available ?
	int totalSampleCount = m_dataSize - samplePosition;

	// may be asking for a sample out of range, clip at zero
	if ( totalSampleCount < 0 )
		totalSampleCount = 0;

	// clip max output samples to max available
	if ( sampleCount > totalSampleCount )
		sampleCount = totalSampleCount;

	// if we are returning some samples, store the pointer
	if ( sampleCount )
	{
		// Starting past end of "preloaded" data, just use regular cache
		if ( samplePosition >= m_nCachedDataSize )
		{
			*pData = GetDataPointer();
		}
		else
		{
			// Start async loader if we haven't already done so
			CacheLoad();

			// Return less data if we are about to run out of uncached data
			if ( samplePosition + sampleCount >= m_nCachedDataSize )
			{
				sampleCount = m_nCachedDataSize - samplePosition;
			}

			// Point at preloaded/cached data from .cache file for now
			*pData = GetCachedDataPointer();
		}

		if ( *pData )
		{
			*pData = (char *)*pData + samplePosition;
		}
		else
		{
			// Out of data or file i/o problem
			sampleCount = 0;
		}
	}

	return sampleCount;
}

CAudioMixer	*CAudioSourceMP3Cache::CreateMixer( int initialStreamPosition, int skipInitialSamples, bool bUpdateDelayForChoreo, SoundError &soundError, hrtf_info_t *pHRTFVector )
{
	CAudioMixer *pMixer = CreateMP3Mixer( CreateWaveDataMemory(*this), &m_sampleRate );
	if ( pMixer )
	{
		ReferenceAdd( pMixer );
		soundError = SE_OK;
	}
	else
	{
		soundError = SE_CANT_CREATE_MIXER;
	}
	return pMixer;
}


CAudioSourceStreamMP3::CAudioSourceStreamMP3( CSfxTable *pSfx ) :
	CAudioSourceMP3( pSfx )
{
}

CAudioSourceStreamMP3::CAudioSourceStreamMP3( CSfxTable *pSfx, CAudioSourceCachedInfo *info ) :
	CAudioSourceMP3( pSfx, info )
{
	m_dataSize = info->DataSize();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CAudioSourceStreamMP3::Prefetch()
{
	char nameBuf[MAX_PATH];
	PrefetchDataStream( m_pSfx->GetFileName(nameBuf, sizeof(nameBuf)), 0, m_dataSize );
}

CAudioMixer	*CAudioSourceStreamMP3::CreateMixer(int intialStreamPosition, int initialSkipSamples, bool bUpdateDelayForChoreo, SoundError &soundError, hrtf_info_t *pHRTFVector )
{
	char nameBuf[MAX_PATH];
	// BUGBUG: Source constructs the IWaveData, mixer frees it, fix this?
	IWaveData *pWaveData = CreateWaveDataStream(*this, static_cast<IWaveStreamSource *>(this), m_pSfx->GetFileName(nameBuf, sizeof(nameBuf)), 0, m_dataSize, m_pSfx, 0, 0, soundError );
	if ( pWaveData )
	{
		CAudioMixer *pMixer = CreateMP3Mixer( pWaveData, &m_sampleRate );
		if ( pMixer )
		{
			ReferenceAdd( pMixer );
			soundError = SE_OK;
			return pMixer;
		}

		// no mixer but pWaveData was deleted in mixer's destructor 
		// so no need to delete
	}
	soundError = SE_CANT_CREATE_MIXER;
	return NULL;
}

int	CAudioSourceStreamMP3::GetOutputData( void **pData, int64 samplePosition, int sampleCount, char copyBuf[AUDIOSOURCE_COPYBUF_SIZE] )
{
	return 0;
}

bool Audio_IsMP3( const char *pName )
{
	int len = strlen(pName);
	if ( len > 4 )
	{
		if ( !Q_strnicmp( &pName[len - 4], ".mp3", 4 ) )
		{
			return true;
		}
	}
	return false;
}


CAudioSource *Audio_CreateStreamedMP3( CSfxTable *pSfx )
{
	CAudioSourceStreamMP3 *pMP3 = NULL; 	
	CAudioSourceCachedInfo *info = audiosourcecache->GetInfo( CAudioSource::AUDIO_SOURCE_MP3, pSfx->IsPrecachedSound(), pSfx );
	if ( info && info->DataSize() != 0 )
	{
		pMP3 = new CAudioSourceStreamMP3( pSfx, info );
	}
	else
	{
		pMP3 = new CAudioSourceStreamMP3( pSfx );
	}
	return pMP3;
}


CAudioSource *Audio_CreateMemoryMP3( CSfxTable *pSfx )
{
	CAudioSourceMP3Cache *pMP3 = NULL;
	CAudioSourceCachedInfo *info = audiosourcecache->GetInfo( CAudioSource::AUDIO_SOURCE_MP3, pSfx->IsPrecachedSound(), pSfx );
	if ( info )
	{
		pMP3 = new CAudioSourceMP3Cache( pSfx, info );
	}
	else
	{
		pMP3 = new CAudioSourceMP3Cache( pSfx );
	}
	return pMP3;
}

