#include "basetypes.h"
#include "commonmacros.h"
#include "soundsystem/lowlevel.h"
#include "tier1/uniqueid.h"
#include "mix.h"

#define DIRECTSOUND_VERSION 0x0800
#include "../thirdparty/dxsdk/include/dsound.h"
#pragma warning(disable : 4201)		// nameless struct/union
#include <ks.h>
#include <ksmedia.h>

static HRESULT (WINAPI *g_pDirectSoundCreate)(GUID FAR *lpGUID, LPDIRECTSOUND FAR *lplpDS, IUnknown FAR *pUnkOuter);

extern void ReleaseSurround(void);
static LPDIRECTSOUND pDS;
static LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;
//-----------------------------------------------------------------------------
// Purpose: Implementation of direct sound
//-----------------------------------------------------------------------------
class CAudioDirectSound2 : public IAudioDevice2
{
public:
	CAudioDirectSound2()
	{
		m_pName = "Windows DirectSound";
		m_nChannels = 2;
		m_nSampleBits = 16;
		m_nSampleRate = 44100;
		m_bIsActive = true;
		m_hWindow = NULL;
		m_hInstDS = 0;
		m_bIsHeadphone = false;
		m_bSupportsBufferStarvationDetection = false;
		m_bIsCaptureDevice = false;
		m_bPlayEvenWhenNotInFocus = false;
	}
	~CAudioDirectSound2( void );
	bool		Init( const audio_device_init_params_t &params );
	void		Shutdown( void );
	void		OutputBuffer( int nChannels, CAudioMixBuffer *pChannelArray );
	int			QueuedBufferCount();
	int			EmptyBufferCount();
	void		CancelOutput( void );
	void		WaitForComplete();
	const wchar_t	*GetDeviceID() const { return m_deviceID; }
	bool		SetShouldPlayWhenNotInFocus( bool bPlayEvenWhenNotInFocus ) OVERRIDE
	{
		if ( bPlayEvenWhenNotInFocus != m_bPlayEvenWhenNotInFocus )
			return false;
		return true;
	}

	// directsound handles this itself
	void		UpdateFocus( bool bWindowHasFocus ) {}
	void		ClearBuffer();
	void		OutputDebugInfo() const;

	inline int BytesPerSample() { return BitsPerSample()>>3; }


	// Singleton object
	static		CAudioDirectSound2 *m_pSingleton;

private:
	// no copies of this class ever
	CAudioDirectSound2( const CAudioDirectSound2 & ); 
	CAudioDirectSound2 & operator=( const CAudioDirectSound2 & ); 

	bool		LockDSBuffer( LPDIRECTSOUNDBUFFER pBuffer, DWORD **pdwWriteBuffer, DWORD *pdwSizeBuffer, const char *pBufferName, int lockFlags = 0 );
	int			GetOutputPosition();

	bool		SNDDMA_InitInterleaved( LPDIRECTSOUND lpDS, WAVEFORMATEX* lpFormat, const audio_device_init_params_t *pParams, int nChannelCount );

	int			m_nTotalBufferSizeBytes;					// size of a single hardware output buffer, in bytes
	int			m_nOneBufferInBytes;
	int			m_nBufferCount;
	int			m_nSubmitPosition;
	DWORD		m_nOutputBufferStartOffset;						// output buffer playback starting byte offset
	HINSTANCE	m_hInstDS;
	HWND		m_hWindow;
	wchar_t		m_deviceID[256];
	bool		m_bPlayEvenWhenNotInFocus;
};


struct dsound_list_t
{
	audio_device_description_t *m_pDeviceListOut;
	int m_nListMax;
	int m_nListCount;
};

#pragma warning(disable:4996)		// suppress: deprecated use strncpy_s instead
static BOOL CALLBACK DSEnumCallback( LPGUID lpGuid, LPCSTR lpcstrDescription, LPCSTR lpcstrModule, LPVOID lpContext )
{
	dsound_list_t *pList = reinterpret_cast<dsound_list_t *>(lpContext);
	audio_device_description_t *pDesc = pList->m_pDeviceListOut + pList->m_nListCount;

	if ( pList->m_nListCount < pList->m_nListMax )
	{
		if ( !lpGuid )
		{
			V_memset( pDesc->m_deviceName, 0, sizeof(pDesc->m_deviceName) );
			pDesc->m_bIsDefault = true;
		}
		else
		{
			pDesc->m_bIsDefault = false;
			char tempString[256];
			UniqueIdToString( *reinterpret_cast<const UniqueId_t *>(lpGuid), tempString, sizeof(tempString) );
			for ( int i = 0; i < sizeof(UniqueId_t); i++ )
			{
				pDesc->m_deviceName[i] = tempString[i];
			}
		}
		pDesc->m_bIsAvailable = true;
		
		V_strncpy( pDesc->m_friendlyName, lpcstrDescription, sizeof(pDesc->m_friendlyName) );
		pDesc->m_nChannelCount = 2;
		pDesc->m_nSubsystemId = AUDIO_SUBSYSTEM_DSOUND;
		pList->m_nListCount++;
	}
	return 1;
}

extern HRESULT WINAPI DirectSoundEnumerateA(LPDSENUMCALLBACKA pDSEnumCallback, LPVOID pContext);

int Audio_EnumerateDSoundDevices( audio_device_description_t *pDeviceListOut, int listCount )
{
	HINSTANCE hInstDS = LoadLibrary("dsound.dll");
	HRESULT (WINAPI *pDirectSoundEnumerate)(LPDSENUMCALLBACKA pDSEnumCallback, LPVOID lpContext);
	pDirectSoundEnumerate = (long (WINAPI *)(LPDSENUMCALLBACKA ,LPVOID))GetProcAddress(hInstDS,"DirectSoundEnumerateA");

	dsound_list_t list;
	list.m_nListCount = 0;
	list.m_nListMax = listCount;
	list.m_pDeviceListOut = pDeviceListOut;
	pDirectSoundEnumerate( &DSEnumCallback, (LPVOID)&list );
	FreeLibrary( hInstDS );
	return list.m_nListCount;
}

//-----------------------------------------------------------------------------
// Class factory
//-----------------------------------------------------------------------------
IAudioDevice2 *Audio_CreateDSoundDevice( const audio_device_init_params_t &params )
{
	if ( !CAudioDirectSound2::m_pSingleton )
	{
		CAudioDirectSound2::m_pSingleton = new CAudioDirectSound2;
	}

	if ( CAudioDirectSound2::m_pSingleton->Init( params ) )
		return CAudioDirectSound2::m_pSingleton;

	delete CAudioDirectSound2::m_pSingleton;
	CAudioDirectSound2::m_pSingleton = NULL;

	Warning("Failed to initialize direct sound!\n");
	return NULL;
}


CAudioDirectSound2 *CAudioDirectSound2::m_pSingleton = NULL;
// ----------------------------------------------------------------------------- //
// Helpers.
// ----------------------------------------------------------------------------- //


CAudioDirectSound2::~CAudioDirectSound2( void )
{
	Shutdown();
	m_pSingleton = NULL;
}


static int GetWindowsSpeakerConfig()
{
	DWORD nSpeakerConfig = DSSPEAKER_STEREO;
	if (DS_OK == pDS->GetSpeakerConfig( &nSpeakerConfig ))
	{
		// split out settings
		nSpeakerConfig = DSSPEAKER_CONFIG(nSpeakerConfig);
		if ( nSpeakerConfig == DSSPEAKER_7POINT1_SURROUND )
			nSpeakerConfig = DSSPEAKER_7POINT1;
		if ( nSpeakerConfig == DSSPEAKER_5POINT1_SURROUND)
			nSpeakerConfig = DSSPEAKER_5POINT1;
		switch( nSpeakerConfig )
		{
		case DSSPEAKER_HEADPHONE:
			return 0;

		case DSSPEAKER_MONO:
		case DSSPEAKER_STEREO:
		default:
			return 2;

		case DSSPEAKER_QUAD:
			return 4;

		case DSSPEAKER_5POINT1:
			return 5;

		case DSSPEAKER_7POINT1:
			return 7;
		}
	}
	return 2;
}



bool CAudioDirectSound2::Init( const audio_device_init_params_t &params )
{
	DSBUFFERDESC	dsbuf;
	DSBCAPS			dsbcaps;
	WAVEFORMATEX	format;
	WAVEFORMATEX	pformat; 
	HRESULT			hresult;
	bool			primary_format_set = false;

	// if a specific device was requested use that one, otherwise use the default (NULL means default)
	LPGUID pGUID = NULL;
	UniqueId_t overrideGUID;
	m_bPlayEvenWhenNotInFocus = params.m_bPlayEvenWhenNotInFocus;
	if ( params.m_bOverrideDevice )
	{
		char tempString[ Q_ARRAYSIZE(params.m_overrideDeviceName) ];
		int nLen = V_wcslen( params.m_overrideDeviceName );
		if ( nLen > 0 )
		{
			for ( int i = 0; i < nLen+1; i++ )
			{
				tempString[i] = params.m_overrideDeviceName[i] & 0xFF;
			}
			UniqueIdFromString( &overrideGUID, tempString );
			pGUID = &(( GUID &)overrideGUID);
			V_wcscpy_safe( m_deviceID, params.m_overrideDeviceName );
		}
	}
	if ( !pGUID )
	{
		V_memset( m_deviceID, 0, sizeof(m_deviceID) );
	}

	if (!m_hInstDS)
	{
		m_hInstDS = LoadLibrary("dsound.dll");
		if (m_hInstDS == NULL)
		{
			Warning( "Couldn't load dsound.dll\n");
			return false;
		}

		g_pDirectSoundCreate = (long (__stdcall *)(struct _GUID *,struct IDirectSound ** ,struct IUnknown *))GetProcAddress(m_hInstDS,"DirectSoundCreate");
		if (!g_pDirectSoundCreate)
		{
			Warning( "Couldn't get DS proc addr\n");
			return false;
		}
	}

	while ((hresult = g_pDirectSoundCreate(pGUID, &pDS, NULL)) != DS_OK)
	{
		if (hresult == DSERR_ALLOCATED)
		{
			Warning("DirectSound hardware in use, can't initialize!\n");
			return false;
		}
		Warning("DirectSound Create failed.\n");
		return false;
	}

	int	nSpeakerConfig = -1;

	if ( params.m_bOverrideSpeakerConfig )
	{
		nSpeakerConfig = params.m_nOverrideSpeakerConfig;
	}
	else
	{
		nSpeakerConfig = GetWindowsSpeakerConfig();
	}
	if ( nSpeakerConfig == 0 )
	{
		m_bIsHeadphone = true;
	}

	// NOTE: This is basically the same as SpeakerConfigToChannelCount() but we 
	// have to set primary channels correctly and DirectSound maps 7.1 to 5.1 
	// at the moment (seems like it could support it easily though)
	int nPrimaryChannels = 2;
	switch( nSpeakerConfig )
	{
		// stereo
	case 0:
	case 2:
	default:
		nPrimaryChannels = 2;
		m_nChannels = 2;		// secondary buffers should have same # channels as primary
		break;

		// surround, use mono 3d primary buffer
		// quad surround
	case 4:
		nPrimaryChannels = 1;
		m_nChannels = 4;
		break;
	case 5:
	case 7:
		nPrimaryChannels = 1;
		m_nChannels = 6;
		break;
	}

	V_memset( &format, 0, sizeof(format) );
	format.wFormatTag		= WAVE_FORMAT_PCM;
    format.nChannels		= nPrimaryChannels;			
    format.wBitsPerSample	= BitsPerSample();
    format.nSamplesPerSec	= SampleRate();
    format.nBlockAlign		= format.nChannels * format.wBitsPerSample / 8;
    format.cbSize			= 0;
    format.nAvgBytesPerSec	= format.nSamplesPerSec * format.nBlockAlign; 

	DSCAPS dscaps;
	V_memset( &dscaps, 0, sizeof(dscaps) );
	dscaps.dwSize = sizeof(dscaps);
	if (DS_OK != pDS->GetCaps(&dscaps))
	{
		Warning( "Couldn't get DS caps\n");
	}

	if (dscaps.dwFlags & DSCAPS_EMULDRIVER)
	{
		Warning( "No DirectSound driver installed\n");
		Shutdown();
		return false;
	}

	m_hWindow = (HWND)params.m_pWindowHandle;
	DWORD dwCooperativeLevel = DSSCL_EXCLUSIVE;
	if (DS_OK != pDS->SetCooperativeLevel( m_hWindow, dwCooperativeLevel ) )
	{
		Warning( "Set coop level failed\n");
		Shutdown();
		return false;
	}

	// get access to the primary buffer, if possible, so we can set the
	// sound hardware format
	V_memset( &dsbuf, 0, sizeof(dsbuf) );
	dsbuf.dwSize = sizeof(DSBUFFERDESC);
	dsbuf.dwFlags = DSBCAPS_PRIMARYBUFFER;
	if ( m_nChannels > 2 )
	{
		dsbuf.dwFlags |= DSBCAPS_CTRL3D;
		Assert( nPrimaryChannels == 1 );
	}
	dsbuf.dwBufferBytes = 0;
	dsbuf.lpwfxFormat = NULL;

	V_memset( &dsbcaps, 0, sizeof(dsbcaps) );
	dsbcaps.dwSize = sizeof(dsbcaps);

	if ( 1 )
	{
		if (DS_OK == pDS->CreateSoundBuffer(&dsbuf, &pDSPBuf, NULL))
		{
			pformat = format;

			if (DS_OK != pDSPBuf->SetFormat(&pformat))
			{
			}
			else
			{
				primary_format_set = true;
			}
		}
	}

	bool bRet = SNDDMA_InitInterleaved( pDS, &format, &params, m_nChannels );

	// number of mono samples output buffer may hold
	m_nSubmitPosition = 0;

	return bRet;
}

void CAudioDirectSound2::Shutdown( void )
{
	if (pDSBuf)
	{
		pDSBuf->Stop();
		pDSBuf->Release();
	}

	// only release primary buffer if it's not also the mixing buffer we just released
	if (pDSPBuf && (pDSBuf != pDSPBuf))
	{
		pDSPBuf->Release();
	}

	if ( pDS && m_hWindow )
	{
		pDS->SetCooperativeLevel( m_hWindow, DSSCL_NORMAL);
		pDS->Release();
	}

	pDS = NULL;
	pDSBuf = NULL;
	pDSPBuf = NULL;

	if ( m_hInstDS )
	{
		FreeLibrary( m_hInstDS );
		m_hInstDS = NULL;
	}

	if ( this == CAudioDirectSound2::m_pSingleton )
	{
		CAudioDirectSound2::m_pSingleton = NULL;
	}
}

void CAudioDirectSound2::OutputDebugInfo() const
{
	Msg( "Direct Sound Device\n" );
	Msg( "Channels:\t%d\n", ChannelCount() );
	Msg( "Bits/Sample:\t%d\n", BitsPerSample() );
	Msg( "Rate:\t\t%d\n", SampleRate() );
}

void CAudioDirectSound2::CancelOutput( void )
{
	if (pDSBuf)
	{
		DWORD	dwSize;
		DWORD	*pData;
		int		reps;
		HRESULT	hresult;

		reps = 0;
		while ((hresult = pDSBuf->Lock(0, m_nTotalBufferSizeBytes, (void**)&pData, &dwSize, NULL, NULL, 0)) != DS_OK)
		{
			if (hresult != DSERR_BUFFERLOST)
			{
				Msg("S_ClearBuffer: DS::Lock Sound Buffer Failed\n");
				return;
			}

			if (++reps > 10000)
			{
				Msg("S_ClearBuffer: DS: couldn't restore buffer\n");
				return;
			}
		}

		V_memset(pData, 0, dwSize);

		pDSBuf->Unlock(pData, dwSize, NULL, 0);
	}
}

bool CAudioDirectSound2::SNDDMA_InitInterleaved( LPDIRECTSOUND lpDS, WAVEFORMATEX* lpFormat, const audio_device_init_params_t *pParams, int nChannelCount )
{
	WAVEFORMATEXTENSIBLE    wfx = { 0 } ;     // DirectSoundBuffer wave format (extensible)

    // set the channel mask and number of channels based on the command line parameter
    if( nChannelCount == 2 )
    {
        wfx.Format.nChannels = 2;
        wfx.dwChannelMask = KSAUDIO_SPEAKER_STEREO;   // SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    }
    else if( nChannelCount == 4 )
    {
        wfx.Format.nChannels = 4;
        wfx.dwChannelMask = KSAUDIO_SPEAKER_QUAD;     // SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
    }
    else if( nChannelCount == 6 )
    {
        wfx.Format.nChannels = 6;
        wfx.dwChannelMask = KSAUDIO_SPEAKER_5POINT1;  // SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
    }
    else
    {
        return false;
    }

    // setup the extensible structure
	int nBytesPerSample = lpFormat->wBitsPerSample / 8;
    wfx.Format.wFormatTag             = WAVE_FORMAT_EXTENSIBLE; 
  //wfx.Format.nChannels              = SET ABOVE 
    wfx.Format.nSamplesPerSec         = lpFormat->nSamplesPerSec;
    wfx.Format.wBitsPerSample         = lpFormat->wBitsPerSample; 
    wfx.Format.nBlockAlign            = nBytesPerSample * wfx.Format.nChannels;
    wfx.Format.nAvgBytesPerSec        = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
    wfx.Format.cbSize                 = 22; // size from after this to end of extensible struct. sizeof(WORD + DWORD + GUID)
    wfx.Samples.wValidBitsPerSample   = lpFormat->wBitsPerSample;
  //wfx.dwChannelMask                 = SET ABOVE BASED ON COMMAND LINE PARAMETERS
    wfx.SubFormat                     = KSDATAFORMAT_SUBTYPE_PCM;

    // setup the DirectSound
    DSBUFFERDESC            dsbdesc = { 0 };  // DirectSoundBuffer descriptor	
    dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	DWORD nBaseFlags = pParams->m_bPlayEvenWhenNotInFocus ? DSBCAPS_GLOBALFOCUS : 0;

	m_nOneBufferInBytes = ( MIX_BUFFER_SIZE * nBytesPerSample * nChannelCount );
	m_nBufferCount = pParams->m_nOutputBufferCount + 1;
	m_nTotalBufferSizeBytes = m_nBufferCount * m_nOneBufferInBytes;
	dsbdesc.dwBufferBytes = m_nTotalBufferSizeBytes;
 
    dsbdesc.lpwfxFormat = (WAVEFORMATEX*)&wfx;

	bool bSuccess = false;
	for ( int i = 0; i < 3; i++ )
	{
		switch(i)
		{
		case 0:
			dsbdesc.dwFlags = nBaseFlags | DSBCAPS_LOCHARDWARE;
			break;
		case 1:
			dsbdesc.dwFlags = nBaseFlags | DSBCAPS_LOCSOFTWARE;
			break;
		case 2:
			dsbdesc.dwFlags = nBaseFlags;
			break;
		}

		if(!FAILED(lpDS->CreateSoundBuffer(&dsbdesc, &pDSBuf, NULL)))
		{
			bSuccess = true;
			break;
		}
	}
	if ( !bSuccess )
	{
		dsbdesc.dwFlags = nBaseFlags;
		if(FAILED(lpDS->CreateSoundBuffer(&dsbdesc, &pDSBuf, NULL)))
		{
			wfx.Format.wFormatTag = WAVE_FORMAT_PCM;
			wfx.Format.cbSize = 0;
			dsbdesc.dwFlags = DSBCAPS_LOCSOFTWARE | nBaseFlags;
			HRESULT hr = lpDS->CreateSoundBuffer(&dsbdesc, &pDSBuf, NULL); 
			if(FAILED(hr))
			{
				printf("Failed %d\n", hr );
				return false;
			}
		}
	}

	DWORD dwSize = 0, dwWrite;
	DWORD *pBuffer = 0;
	if ( !LockDSBuffer( pDSBuf, &pBuffer, &dwSize, "DS_INTERLEAVED", DSBLOCK_ENTIREBUFFER ) )
		return false;

	m_nChannels = wfx.Format.nChannels;
	V_memset( pBuffer, 0, dwSize );

	pDSBuf->Unlock(pBuffer, dwSize, NULL, 0);
	
	// Make sure mixer is active (this was moved after the zeroing to avoid popping on startup -- at least when using the dx9.0b debug .dlls)
	pDSBuf->Play(0, 0, DSBPLAY_LOOPING);

	pDSBuf->Stop();
	pDSBuf->GetCurrentPosition(&m_nOutputBufferStartOffset, &dwWrite);

	pDSBuf->Play(0, 0, DSBPLAY_LOOPING);

	return true;
}

bool CAudioDirectSound2::LockDSBuffer( LPDIRECTSOUNDBUFFER pBuffer, DWORD **pdwWriteBuffer, DWORD *pdwSizeBuffer, const char *pBufferName, int lockFlags )
{
	if ( !pBuffer )
		return false;
	HRESULT hr;
	int reps = 0;
	while ((hr = pBuffer->Lock(0, m_nTotalBufferSizeBytes, (void**)pdwWriteBuffer, pdwSizeBuffer, 
		NULL, NULL, lockFlags)) != DS_OK)
	{
		if (hr != DSERR_BUFFERLOST)
		{
			Msg ("DS::Lock Sound Buffer Failed %s\n", pBufferName);
			return false;
		}

		if (++reps > 10000)
		{
			Msg ("DS:: couldn't restore buffer %s\n", pBufferName);
			return false;
		}
	}
	return true;
}

void CAudioDirectSound2::OutputBuffer( int nMixChannelCount, CAudioMixBuffer *pMixChannels )
{
	HRESULT hr;
	int nDeviceChannelCount = ChannelCount();
	int nOutputSize = BytesPerSample() * nDeviceChannelCount * MIX_BUFFER_SIZE;
	void *pBuffer0=NULL;
	void *pBuffer1=NULL;
	DWORD nSize0, nSize1;
	int nReps = 0;
	while ( (hr = pDSBuf->Lock( m_nSubmitPosition, nOutputSize, &pBuffer0, &nSize0, &pBuffer1, &nSize1, 0 )) != DS_OK )
	{
		if ( hr == DSERR_BUFFERLOST )
		{
			if ( ++nReps < 10000 )
				continue;
		}
		Msg ("DS::Lock Sound Buffer Failed\n");
		return;
	}

	int nStart = 0;
	if ( pBuffer0 )
	{
		short *pOut = (short *)pBuffer0;
		int nSamplesOut = nSize0 / (nDeviceChannelCount*2);

		if ( nMixChannelCount == 2 && nMixChannelCount == nDeviceChannelCount )
		{
			ConvertFloat32Int16_Clamp_Interleave2( pOut, pMixChannels[0].m_flData, pMixChannels[1].m_flData, nSamplesOut );
		}
		else 
		{
			ConvertFloat32Int16_Clamp_InterleaveStride( pOut, nDeviceChannelCount, MIX_BUFFER_SIZE, pMixChannels[0].m_flData, nMixChannelCount, nSamplesOut );
		}

		nStart = nSamplesOut;
	}
	if ( pBuffer1 )
	{
		short *pOut = (short *)pBuffer1;
		int nSamplesOut = nSize1 / (nDeviceChannelCount*2);
		if ( nMixChannelCount == 2 && nMixChannelCount == nDeviceChannelCount )
		{
			ConvertFloat32Int16_Clamp_Interleave2( pOut, pMixChannels[0].m_flData, pMixChannels[1].m_flData, nSamplesOut );
		}
		else 
		{
			ConvertFloat32Int16_Clamp_InterleaveStride( pOut, nDeviceChannelCount, MIX_BUFFER_SIZE, pMixChannels[0].m_flData, nMixChannelCount, nSamplesOut );
		}
	}
	pDSBuf->Unlock(pBuffer0, nSize0, pBuffer1, nSize1);
	m_nSubmitPosition += nOutputSize;
	m_nSubmitPosition %= m_nTotalBufferSizeBytes;
}

int CAudioDirectSound2::QueuedBufferCount()
{
	int nStart, nCurrent;
	DWORD dwCurrentPlayCursor;

	// get size in bytes of output buffer
	const int nSizeInBytes = m_nTotalBufferSizeBytes; 
	// multi-channel interleaved output buffer 
	// get byte offset of playback cursor in output buffer
	HRESULT hr = pDSBuf->GetCurrentPosition(&dwCurrentPlayCursor, NULL);
	if ( hr != S_OK )
		return m_nBufferCount;

	if ( dwCurrentPlayCursor > DWORD(m_nTotalBufferSizeBytes) )
	{
		// BUGBUG: ??? what do do here?
		DebuggerBreakIfDebugging();
		dwCurrentPlayCursor %= m_nTotalBufferSizeBytes;
	}
	nStart = (int) m_nOutputBufferStartOffset;
	nCurrent = (int) dwCurrentPlayCursor;

	// get 16 bit samples played, relative to buffer starting offset
	int delta = m_nSubmitPosition - nCurrent;
	if ( delta < 0 )
	{
		delta += nSizeInBytes;
	}

	int nSamples = delta / (ChannelCount() * BytesPerSample());
	int nBuffers = nSamples / MIX_BUFFER_SIZE;
	//int nTotalBuffers = (m_nTotalBufferSizeBytes/ (ChannelCount() * BytesPerSample())) / MIX_BUFFER_SIZE;
	//Msg("%d buffers remain %d total\n", nBuffers, nTotalBuffers);
	if ( nBuffers == 0 )
	{
		//Msg("cursor %d, submit %d, relative %d\n", nCurrent, m_nSubmitPosition, delta );
	}
	return nBuffers;
}

int	CAudioDirectSound2::EmptyBufferCount()
{
	return (m_nBufferCount-1) - QueuedBufferCount();
}

void CAudioDirectSound2::WaitForComplete()
{
	while ( QueuedBufferCount() )
	{
		ThreadSleep(0);
	}
}

void CAudioDirectSound2::ClearBuffer( void )
{
	DWORD dwSize = 0;
	DWORD *pBuffer = 0;
	if ( LockDSBuffer( pDSBuf, &pBuffer, &dwSize, "DS_INTERLEAVED", DSBLOCK_ENTIREBUFFER ) )
	{
		V_memset( pBuffer, 0, dwSize );
		pDSBuf->Unlock(pBuffer, dwSize, NULL, 0);
	}
}
