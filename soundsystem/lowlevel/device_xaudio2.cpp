#include "basetypes.h"
#include "commonmacros.h"

#if !defined( _X360 ) && defined( WIN32 )
#define _WIN32_DCOM
#include <windows.h>
#endif
#include <xaudio2.h>
#include "mix.h"
#include "soundsystem/lowlevel.h"


static IXAudio2 *g_pXAudio2 = NULL;
static int g_XAudio2Refcount = 0;
extern CInterlockedInt g_nDetectedAudioError;
extern CInterlockedInt g_nDetectedBufferStarvation;
//-----------------------------------------------------------------------------
// Purpose: Implementation of XAudio2 device for source2
//-----------------------------------------------------------------------------
class CAudioXAudio2 : public IAudioDevice2, public IXAudio2VoiceCallback
{
public:
	CAudioXAudio2()
	{
		g_XAudio2Refcount++;

		m_pName = "XAudio2 Device";
		m_nChannels = 2;
		m_bIsHeadphone = false;
		m_bSupportsBufferStarvationDetection = true;
		m_bIsCaptureDevice = false;
		m_nSampleBits = 16;
		m_nSampleRate = 44100;
		m_bIsActive = true;
		m_pMasterVoice = NULL;
		m_pSourceVoice = NULL;
		m_pBuffer = NULL;
		m_nBufferSizeBytes = 0;
		m_nBufferCount = 0;
		m_nSubmitIndex = 0;
		m_nActiveBuffers = 0;
		m_nBufferErrors = 0;
		m_bHasFocus = true;
		m_bVoiceStarted = false;
	}
	~CAudioXAudio2( void );
	bool		Init( const audio_device_init_params_t &params, int nDeviceIndex );

	void		Shutdown( void ) OVERRIDE;
	void		OutputBuffer( int nChannels, CAudioMixBuffer *pChannelArray ) OVERRIDE;
	const wchar_t *GetDeviceID() const OVERRIDE  { return m_deviceID; }
	int			QueuedBufferCount() OVERRIDE;
	int			EmptyBufferCount() OVERRIDE;
	void		CancelOutput() OVERRIDE;
	void		WaitForComplete() OVERRIDE;
	void		UpdateFocus( bool bWindowHasFocus ) OVERRIDE;
	void		ClearBuffer() OVERRIDE {}
	void		OutputDebugInfo() const OVERRIDE;
	bool		SetShouldPlayWhenNotInFocus( bool bPlayEvenWhenNotInFocus ) OVERRIDE
	{
		m_savedParams.m_bPlayEvenWhenNotInFocus = bPlayEvenWhenNotInFocus;
		return true;
	}

	inline int BytesPerSample() { return BitsPerSample()>>3; }
	// Singleton object

	// IXAudio2VoiceCallback
	// Called just before this voice's processing pass begins.
	virtual void __stdcall OnVoiceProcessingPassStart( UINT32 nBytesRequired ) OVERRIDE {}
	virtual void __stdcall OnVoiceProcessingPassEnd() OVERRIDE {}
	virtual void __stdcall OnStreamEnd() OVERRIDE {}
	virtual void __stdcall OnBufferStart( void* pBufferContext ) OVERRIDE 
	{
	}
	virtual void __stdcall OnBufferEnd( void* pBufferContext ) OVERRIDE 
	{
		Assert( m_nActiveBuffers > 0 );
		m_nActiveBuffers--;
		if ( m_nActiveBuffers == 0 )
		{
			m_nBufferErrors++;
			if ( m_nBufferErrors > 10 )
			{
				g_nDetectedBufferStarvation++;
			}
		}
	}
	virtual void __stdcall OnLoopEnd( void* pBufferContext ) OVERRIDE {}
	virtual void __stdcall OnVoiceError( void* pBufferContext, HRESULT nError ) OVERRIDE 
	{
		g_nDetectedAudioError = 1;
		Warning("Xaudio2 Voice Error %x\n", uint(nError) );
	}

private:

	// no copies of this class ever
	CAudioXAudio2( const CAudioXAudio2 & ); 
	CAudioXAudio2 & operator=( const CAudioXAudio2 & ); 

	int	SamplesPerBuffer() { return MIX_BUFFER_SIZE; }
	int BytesPerBuffer() { return m_nBufferSizeBytes; }

	IXAudio2MasteringVoice	*m_pMasterVoice;
	IXAudio2SourceVoice		*m_pSourceVoice;

	short		*m_pBuffer;
	int			m_nBufferSizeBytes;					// size of a single hardware output buffer, in bytes
	int			m_nBufferCount;
	int			m_nSubmitIndex;
	int			m_nBufferErrors;

	CInterlockedInt	m_nActiveBuffers;

	// for error recovery
	audio_device_init_params_t m_savedParams;
	int		m_nSavedPreferred;
	wchar_t	m_deviceID[256];
	char	m_displayName[256];
	bool	m_bHasFocus;
	bool	m_bVoiceStarted;
};


class CXAudioCallbacks : public IXAudio2EngineCallback
{
public:
	CXAudioCallbacks() : m_bRegistered(false) {}

	// IXAudio2EngineCallback
	// ------------------------------------
	STDMETHOD_(void, OnProcessingPassStart) (THIS);
	STDMETHOD_(void, OnProcessingPassEnd) (THIS);
	// Called in the event of a critical system error which requires XAudio2
	// to be closed down and restarted.  The error code is given in Error.
	STDMETHOD_(void, OnCriticalError) (THIS_ HRESULT Error);
	// ------------------------------------

	void Init( IXAudio2 *pInterface )
	{
		pInterface->RegisterForCallbacks( this );
		m_bRegistered = true;
	}
	void Shutdown( IXAudio2 *pInterface )
	{
		if ( m_bRegistered )
		{
			pInterface->UnregisterForCallbacks( this );
			m_bRegistered = false;
		}
	}

	bool m_bRegistered;
};

void CXAudioCallbacks::OnProcessingPassStart() {}
void CXAudioCallbacks::OnProcessingPassEnd() {}

void CXAudioCallbacks::OnCriticalError( HRESULT nError ) 
{
	g_nDetectedAudioError = 1;
	Warning("Xaudio2 Error %x\n", uint(nError) );
}

static CXAudioCallbacks g_XAudioErrors;

static bool InitXAudio()
{
	if ( !g_pXAudio2 )
	{
		HRESULT hr;
		InitCOM();
#ifdef _DEBUG
		// UNDONE: Figure out why this fails - I believe it requires a separate install
		if ( FAILED(hr = XAudio2Create( &g_pXAudio2, XAUDIO2_DEBUG_ENGINE, XAUDIO2_DEFAULT_PROCESSOR ) ) )
#endif
		if ( FAILED(hr = XAudio2Create( &g_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR ) ) )
			return false;

		g_XAudioErrors.Init( g_pXAudio2 );

// if someone calls CoFreeUnusedLibrariesEx (some shell extensions do this), then XAudio2 will get freed, go ahead an load the library explicitly
// to prevent this unloading
#if defined(PLATFORM_WINDOWS_PC)
#ifdef _DEBUG
		const char *pDLLName = "XAudioD2_7.dll";
#else
		const char *pDLLName = "XAudio2_7.dll";
#endif
		// The DLL is already loaded, check the name to make sure we aren't loading an additional DLL
		if ( GetModuleHandle( pDLLName ) )
		{
			LoadLibrary( pDLLName );
		}
#endif
	}
	return true;
}

static void ShutdownXAudio()
{
	if ( g_pXAudio2 )
	{
		g_XAudioErrors.Shutdown( g_pXAudio2 );
		g_pXAudio2->Release();
		g_pXAudio2 = NULL;
		ShutdownCOM();
	}
}

// enumerate the available devices so the app can select one
// fills out app-supplied list & returns count of available devices.  If the list is too small, the count
// will signal the app to call again with a larger list
int Audio_EnumerateXAudio2Devices( audio_device_description_t *pDeviceListOut, int nListCount )
{
	if ( !InitXAudio() )
		return 0;

	UINT32 nDeviceCountWindows = 0;
	HRESULT hr = g_pXAudio2->GetDeviceCount(&nDeviceCountWindows);
	Assert( hr == S_OK );
	if ( hr != S_OK )
		return 0;
	int nDeviceCount = (int)nDeviceCountWindows;
	XAUDIO2_DEVICE_DETAILS deviceDetails;

	bool bWroteDefault = false;
	int nMaxChannelDevice = -1;

	// now get each device's details that will fit into the supplied list
	int nIterateCount = min(nListCount, nDeviceCount);
	for ( int i = 0; i < nIterateCount; i++ )
	{
		g_pXAudio2->GetDeviceDetails(i,&deviceDetails);
		V_wcscpy_safe( pDeviceListOut[i].m_deviceName, deviceDetails.DeviceID );
		V_wcstostr( deviceDetails.DisplayName, -1, pDeviceListOut[i].m_friendlyName, sizeof(pDeviceListOut[i].m_friendlyName) );
		pDeviceListOut[i].m_nChannelCount = deviceDetails.OutputFormat.Format.nChannels;
		pDeviceListOut[i].m_bIsDefault = false;
		pDeviceListOut[i].m_bIsAvailable = true;
		pDeviceListOut[i].m_nSubsystemId = AUDIO_SUBSYSTEM_XAUDIO;

		
		// anything marked as default game device will be selected by default
		if ( deviceDetails.Role & DefaultGameDevice )
		{
			pDeviceListOut[i].m_bIsDefault = true;
			bWroteDefault = true;
		}
		if ( nMaxChannelDevice < 0 || deviceDetails.OutputFormat.Format.nChannels > pDeviceListOut[nMaxChannelDevice].m_nChannelCount )
		{
			nMaxChannelDevice = i;
		}
	}
	// no default?  Select first device with max # of channels
	// If there are no channels then nMaxChannelDevice will be negative so we need to
	// check before using it as an index.
	if ( !bWroteDefault && nMaxChannelDevice >= 0 && nMaxChannelDevice < nListCount )
	{
		pDeviceListOut[nMaxChannelDevice].m_bIsDefault = true;
	}

	return nIterateCount;
}

//-----------------------------------------------------------------------------
// Class factory
//-----------------------------------------------------------------------------
IAudioDevice2 *Audio_CreateXAudio2Device( const audio_device_init_params_t &params )
{
	if ( !InitXAudio() )
		return NULL;

	int nPreferredDevice = 0;
	UINT32 nDeviceCountWindows = 0;
	int nCount = 0;
	HRESULT hr = g_pXAudio2->GetDeviceCount(&nDeviceCountWindows);
	CUtlVector<audio_device_description_t> desc;
	if ( hr == S_OK )
	{
		desc.SetCount( nDeviceCountWindows );

		nCount = Audio_EnumerateXAudio2Devices( desc.Base(), desc.Count() );
	}
	// If there are no devices then device Init will fail. We might as well
	// fail early.
	// This was happening when running test_source2.bat in a loop and
	// disconnecting partway through the past -- as soon as the machine was
	// running headless the enumeration would return zero devices.
	if ( !nCount )
		return NULL;
	for ( int i = 0; i < nCount; i++ )
	{
		if ( desc[i].m_bIsDefault )
		{
			nPreferredDevice = i;
		}
	}
	if ( params.m_bOverrideDevice )
	{
		for ( int i = 0; i < nCount; i++ )
		{
			if ( !V_wcscmp( desc[i].m_deviceName, params.m_overrideDeviceName ) )
			{
				nPreferredDevice = i;
				break;
			}
		}
	}

	CAudioXAudio2 *pDevice = new CAudioXAudio2;

	if ( pDevice->Init( params, nPreferredDevice ) )
		return pDevice;

	delete pDevice;
	return NULL;
}

CAudioXAudio2::~CAudioXAudio2()
{
	Shutdown();
	if ( m_pBuffer )
	{
		MemAlloc_FreeAligned( m_pBuffer );
	}
	g_XAudio2Refcount--;
	if ( !g_XAudio2Refcount )
	{
		ShutdownXAudio();
	}
}


bool CAudioXAudio2::Init( const audio_device_init_params_t &params, int nDeviceIndex )
{
	HRESULT hr;
	// NOTE: sample rate was XAUDIO2_DEFAULT_SAMPLERATE
	if ( FAILED(hr = g_pXAudio2->CreateMasteringVoice( &m_pMasterVoice, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, nDeviceIndex, NULL ) ) )
		return false;

	XAUDIO2_DEVICE_DETAILS deviceDetails;
	g_pXAudio2->GetDeviceDetails( nDeviceIndex, &deviceDetails );
	V_wcscpy_safe( m_deviceID, deviceDetails.DeviceID );
	V_wcstostr( deviceDetails.DisplayName, -1, m_displayName, sizeof(m_displayName) );
	// save for error recovery
	m_nSavedPreferred = nDeviceIndex;
	m_savedParams = params;

	XAUDIO2_VOICE_DETAILS details;
	m_pMasterVoice->GetVoiceDetails( &details );
	WAVEFORMATEX    wfx = { 0 };

    // setup the format structure
	{
		int nChannels = details.InputChannels;
		if ( params.m_bOverrideSpeakerConfig )
		{
			nChannels = SpeakerConfigValueToChannelCount( params.m_nOverrideSpeakerConfig );
			if ( params.m_nOverrideSpeakerConfig == 0 )
			{
				m_bIsHeadphone = true;
			}
		}
		m_nChannels = nChannels;
	}

    wfx.wFormatTag				= WAVE_FORMAT_PCM; 
	wfx.nChannels				= m_nChannels;
    wfx.nSamplesPerSec			= SampleRate();
    wfx.wBitsPerSample			= BitsPerSample(); 
    wfx.nBlockAlign				= wfx.wBitsPerSample / 8 * wfx.nChannels;
    wfx.nAvgBytesPerSec			= wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize					= 0;
    if( FAILED( hr = g_pXAudio2->CreateSourceVoice( &m_pSourceVoice, &wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO, this ) ) )
    {
		return false;
    }

	m_nBufferCount = params.m_nOutputBufferCount;
	int nBufferSize = MIX_BUFFER_SIZE * m_nChannels * BytesPerSample();
	m_nBufferSizeBytes = nBufferSize;
	
	m_nChannels = wfx.nChannels;
	Assert( m_nChannels <= SOUND_DEVICE_MAX_CHANNELS );

	m_pBuffer = (short *)MemAlloc_AllocAligned( nBufferSize * m_nBufferCount, 16 );
	m_nActiveBuffers = 0;
	m_nSubmitIndex = 0;
	m_bVoiceStarted = false;
	return true;
}

void CAudioXAudio2::Shutdown()
{
	if ( m_pMasterVoice )
	{
		if ( m_pSourceVoice )
		{
			m_pSourceVoice->Stop();
			m_pSourceVoice->DestroyVoice();
			m_pSourceVoice = nullptr;
			m_bVoiceStarted = false;
		}
		m_pMasterVoice->DestroyVoice();
		m_pMasterVoice = nullptr;
	}
}

void CAudioXAudio2::OutputDebugInfo() const
{
	Msg( "XAudio2 Sound Device: %s\n", m_displayName );
	Msg( "Channels:\t%d\n", ChannelCount() );
	Msg( "Bits/Sample:\t%d\n", BitsPerSample() );
	Msg( "Rate:\t\t%d\n", SampleRate() );
}

void CAudioXAudio2::OutputBuffer( int nChannels, CAudioMixBuffer *pChannelArray )
{
	// start the voice as soon as we have data to output
	if ( !m_bVoiceStarted )
	{
		m_pSourceVoice->Start();
		m_bVoiceStarted = true;
	}
	int nBufferSize = BytesPerBuffer();
	short *pWaveData = m_pBuffer + ( m_nSubmitIndex * (nBufferSize>>1) );

	XAUDIO2_BUFFER buffer = {0};
    buffer.pAudioData = (BYTE *)pWaveData;
    buffer.Flags = 0;
    buffer.AudioBytes = nBufferSize;

	if ( nChannels == 2 && nChannels == m_nChannels )
	{
		ConvertFloat32Int16_Clamp_Interleave2( pWaveData, pChannelArray[0].m_flData, pChannelArray[1].m_flData, MIX_BUFFER_SIZE );
	}
	else 
	{
		ConvertFloat32Int16_Clamp_InterleaveStride( pWaveData, m_nChannels, MIX_BUFFER_SIZE, pChannelArray[0].m_flData, nChannels, MIX_BUFFER_SIZE );
	}

	m_nActiveBuffers++;
    m_pSourceVoice->SubmitSourceBuffer( &buffer );
	m_nSubmitIndex = ( m_nSubmitIndex + 1 ) % m_nBufferCount;
}

int CAudioXAudio2::QueuedBufferCount()
{
	// NOTE: If callbacks work on all clients then we do not need to do the potentially expensive GetState() call
	// we already know if buffers are retired
	// UNDONE: If this is causing problems, just change to #if 0 - the other code in this changelist will not interact with anything
#if 1
	return m_nActiveBuffers;
#else
	XAUDIO2_VOICE_STATE state;
	m_pSourceVoice->GetState( &state );
	return state.BuffersQueued;
#endif
}

int	CAudioXAudio2::EmptyBufferCount()
{
	return m_nBufferCount - QueuedBufferCount();
}

void CAudioXAudio2::CancelOutput()
{
	m_pSourceVoice->FlushSourceBuffers();
}


void CAudioXAudio2::WaitForComplete()
{
	m_pSourceVoice->Discontinuity();
	while( QueuedBufferCount() )
	{
		ThreadSleep(0);
	}
}

void CAudioXAudio2::UpdateFocus( bool bWindowHasFocus )
{
	if ( m_pMasterVoice && !m_savedParams.m_bPlayEvenWhenNotInFocus )
	{
		if ( bWindowHasFocus != m_bHasFocus )
		{
			m_bHasFocus = bWindowHasFocus;

			float flVolume = 1.0f;
			m_pMasterVoice->GetVolume( &flVolume );
			m_pMasterVoice->SetVolume( bWindowHasFocus ? 1.0f : 0.0f );
		}
	}
}