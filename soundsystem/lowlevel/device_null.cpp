#include "basetypes.h"
#include "commonmacros.h"
#include "soundsystem/lowlevel.h"
#include "soundsystem/audio_mix.h"

CInterlockedInt g_nDetectedAudioError(0);
CInterlockedInt g_nDetectedBufferStarvation( 0 );
uint g_nDeviceStamp = 0x100;

class CAudioDeviceNull2 : public IAudioDevice2
{
public:
	CAudioDeviceNull2()
	{
		m_pName = "Sound Disabled";
		m_nChannels = 2;
		m_nSampleBits = 16;
		m_nSampleRate = int(MIX_DEFAULT_SAMPLING_RATE);
		m_bIsActive = false;
		m_bIsHeadphone = false;
		m_bSupportsBufferStarvationDetection = false;
		m_bIsCaptureDevice = false;
	}

	virtual ~CAudioDeviceNull2() {}
	virtual void		OutputBuffer( int nChannels, CAudioMixBuffer *pChannelArray ) {}
	virtual void		Shutdown() {}
	virtual int			QueuedBufferCount() { return 0; }
	virtual int			EmptyBufferCount() { return 0; }
	virtual void		CancelOutput() {}
	virtual void		WaitForComplete() {}
	virtual void		UpdateFocus( bool bWindowHasFocus ) {}
	virtual void		ClearBuffer() {}
	virtual const wchar_t	*GetDeviceID() const
	{
		static wchar_t deviceID[4] = {0};
		return deviceID;
	}
	virtual void		OutputDebugInfo() const 
	{
		Msg( "Sound Disabled.\n" );
	}
	virtual bool		SetShouldPlayWhenNotInFocus( bool bPlayEvenWhenNotInFocus ) OVERRIDE
	{
		return true;
	}
};


IAudioDevice2 *Audio_CreateNullDevice()
{
	return new CAudioDeviceNull2;
}

int Audio_EnumerateDevices( eSubSystems_t nSubsystem, audio_device_description_t *pDeviceListOut, int nListCount )
{
	int nDeviceCount = 0;
	switch( nSubsystem )
	{
#ifdef IS_WINDOWS_PC
	case AUDIO_SUBSYSTEM_XAUDIO:
		nDeviceCount = Audio_EnumerateXAudio2Devices( pDeviceListOut, nListCount );
		break;
	case AUDIO_SUBSYSTEM_DSOUND:
		nDeviceCount = Audio_EnumerateDSoundDevices( pDeviceListOut, nListCount );
		break;
#endif
#ifdef POSIX
	case AUDIO_SUBSYSTEM_SDL:
		nDeviceCount = Audio_EnumerateSDLDevices( pDeviceListOut, nListCount );
		break;
#endif
	case AUDIO_SUBSYSTEM_NULL:
		nDeviceCount = 1;
		if ( nListCount > 0 )
		{
			pDeviceListOut[0].InitAsNullDevice();
			V_strcpy_safe( pDeviceListOut[0].m_friendlyName, "Sound Disabled" );
		}
		break;
	}
	return nDeviceCount;
}

int CAudioDeviceList::FindDeviceById( const wchar_t *pId, finddevice_t nFind )
{
	for ( int i = 0; i < m_list.Count(); i++ )
	{
		if ( nFind == FIND_AVAILABLE_DEVICE_ONLY && !m_list[i].m_bIsAvailable )
			continue;
		if ( !V_wcscmp( pId, m_list[i].m_deviceName ) )
			return i;
	}
	return -1;
}


audio_device_description_t *CAudioDeviceList::FindDeviceById( const char *pId )
{
	wchar_t tempName[256];
	V_strtowcs( pId, -1, tempName, Q_ARRAYSIZE(tempName) );
	int nDevice = FindDeviceById( tempName, FIND_AVAILABLE_DEVICE_ONLY );
	if ( nDevice >= 0 && nDevice < m_list.Count() )
		return &m_list[nDevice];
	return NULL;
}


void CAudioDeviceList::BuildDeviceList( eSubSystems_t nPreferredSubsystem )
{
	m_nDeviceStamp = g_nDeviceStamp;
	audio_device_description_t initList[32];
	int nCount = Audio_EnumerateDevices( nPreferredSubsystem, initList, Q_ARRAYSIZE(initList) );

	// No XAudio2?  Fall back to direct sound.
	if ( nCount == 0 && nPreferredSubsystem == AUDIO_SUBSYSTEM_XAUDIO )
	{
		nPreferredSubsystem = AUDIO_SUBSYSTEM_DSOUND;
		nCount = Audio_EnumerateDevices( nPreferredSubsystem, initList, Q_ARRAYSIZE(initList) );
	}

	m_nSubsystem = nPreferredSubsystem;
	// No sound devices?  Add a NULL device.
	if ( nCount == 0 )
	{
		nCount = Audio_EnumerateDevices( AUDIO_SUBSYSTEM_NULL, initList, Q_ARRAYSIZE(initList) );
	}
	if ( !m_list.Count() )
	{
		m_list.CopyArray( initList, nCount );
		for ( int i = 0; i < m_list.Count(); i++ )
		{
			m_list[i].m_bIsAvailable = true;
		}
	}
	else
	{
		for ( int i = 0; i < m_list.Count(); i++ )
		{
			m_list[i].m_bIsAvailable = false;
			m_list[i].m_bIsDefault = false;
		}
		for ( int i = 0; i < nCount; i++ )
		{
			int nIndex = FindDeviceById( initList[i].m_deviceName, FIND_ANY_DEVICE );
			if ( nIndex >= 0 )
			{
				m_list[nIndex] = initList[i];
			}
			else
			{
				m_list.AddToTail( initList[i] );
			}
		}
	}
	UpdateDefaultDevice();
}

bool CAudioDeviceList::UpdateDeviceList()
{
	if ( m_nDeviceStamp == g_nDeviceStamp )
		return false;

	BuildDeviceList( m_nSubsystem );
	return true;
}

void CAudioDeviceList::UpdateDefaultDevice()
{
#if IS_WINDOWS_PC
	// BUG: DirectSound devices use a different string format for GUIDs.  Fix so this works?
	wchar_t deviceName[256];
	if ( GetWindowsDefaultAudioDevice( deviceName, sizeof(deviceName ) ) )
	{
		int nIndex = FindDeviceById( deviceName, FIND_AVAILABLE_DEVICE_ONLY );
		if ( nIndex >= 0 )
		{
			m_nDefaultDevice = nIndex;
			return;
		}
	}
#endif
	m_nDefaultDevice = -1;
	int nFirst = -1;
	for ( int i = 0; i < m_list.Count(); i++ )
	{
		if ( m_list[i].m_bIsAvailable )
		{
			if ( nFirst < 0 )
			{
				nFirst = i;
			}
			if ( m_list[i].m_bIsDefault )
			{
				m_nDefaultDevice = i;
				break;
			}
		}
	}
	if ( m_nDefaultDevice < 0 )
	{
		m_nDefaultDevice = nFirst;
	}
}

IAudioDevice2 *CAudioDeviceList::CreateDevice( audio_device_init_params_t &params )
{
	Assert( IsValid() );
	int nSubsystem = m_nSubsystem;

#if !defined( _GAMECONSOLE )
	if ( params.m_bOverrideDevice )
	{
		nSubsystem = params.m_nOverrideSubsystem;
	}
#endif
#if IS_WINDOWS_PC
	// try xaudio2
	if ( nSubsystem == AUDIO_SUBSYSTEM_XAUDIO )
	{
		IAudioDevice2 *pDevice = Audio_CreateXAudio2Device( params );
		if ( pDevice )
			return pDevice;
		Warning("Failed to initialize XAudio2 device!\n");
		nSubsystem = AUDIO_SUBSYSTEM_DSOUND;
	}

	if ( nSubsystem == AUDIO_SUBSYSTEM_DSOUND )
	{
		// either we were asked for dsound or we failed to create xaudio2, try dsound
		IAudioDevice2 *pDevice = Audio_CreateDSoundDevice( params );
		if ( pDevice )
			return pDevice;
		Warning("Failed to initialize DirectSound device!\n");
		nSubsystem = AUDIO_SUBSYSTEM_NULL;
	}
#endif

#ifdef POSIX
	nSubsystem = AUDIO_SUBSYSTEM_SDL;

	if ( nSubsystem == AUDIO_SUBSYSTEM_SDL )
	{
		IAudioDevice2 *pDevice = Audio_CreateSDLDevice( params );
		if ( pDevice )
			return pDevice;

		Warning("Failed to initialize SDL device!\n");
		nSubsystem = AUDIO_SUBSYSTEM_NULL;
	}
#endif

	// failed
	return Audio_CreateNullDevice();
}

const wchar_t *CAudioDeviceList::GetDeviceToCreate( audio_device_init_params_t &params )
{
	if ( params.m_bOverrideDevice )
	{
		int nIndex = FindDeviceById( params.m_overrideDeviceName, FIND_AVAILABLE_DEVICE_ONLY );
		if ( nIndex >= 0 )
		{
			return m_list[nIndex].m_deviceName;
		}
	}
	Assert( m_nDefaultDevice >= 0 && m_nDefaultDevice < m_list.Count() );
	return m_list[m_nDefaultDevice].m_deviceName;
}


int SpeakerConfigValueToChannelCount( int nSpeakerConfig )
{
	// headphone or stereo
	switch( nSpeakerConfig )
	{
	case -1:
		return 0;
	case 0: // headphone
	case 2: // stereo
		return 2;
	case 4: // quad surround
		return 4;

	case 5: // 5.1 surround
		return 6;

	case 7: // 7.1 surround 
		return 8;
	}

	// doesn't map to anything, return stereo
	AssertMsg( 0, "Bad speaker config requested\n");
	return 2;
}

int ChannelCountToSpeakerConfigValue( int nChannelCount, bool bIsHeadphone )
{
	switch( nChannelCount )
	{
	case 2:
		return bIsHeadphone ? 0 : 2;
	case 4:
		return 4;
	case 6:
		return 5;
	case 8:
		return 7;
	}
	AssertMsg(0, "Bad channel count\n");
	return 2;
}

bool Audio_PollErrorEvents()
{
	int nError = g_nDetectedAudioError.InterlockedExchange(0);

	return nError != 0;
}
