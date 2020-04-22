//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: DLL interface for low-level sound utilities
//
//===========================================================================//

#include "soundsystem/isoundsystem.h"
#include "filesystem.h"
#include "tier1/strtools.h"
#include "tier1/convar.h"
#include "mathlib/mathlib.h"
#include "soundsystem/snd_device.h"
#include "datacache/idatacache.h"
#include "soundchars.h"
#include "tier1/utldict.h"
#include "snd_wave_source.h"
#include "snd_dev_wave.h"
#include "tier2/tier2.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// External interfaces
//-----------------------------------------------------------------------------
IAudioDevice *g_pAudioDevice = NULL;


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
int g_nSoundFrameCount = 0;


//-----------------------------------------------------------------------------
// Purpose: DLL interface for low-level sound utilities
//-----------------------------------------------------------------------------
class CSoundSystem : public CTier2AppSystem< ISoundSystem >
{
	typedef CTier2AppSystem< ISoundSystem > BaseClass;

public:
	// Inherited from IAppSystem
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	void		Update( float dt );
	void		Flush( void );

	CAudioSource *FindOrAddSound( const char *filename );
	CAudioSource *LoadSound( const char *wavfile );
	void		PlaySound( CAudioSource *source, float volume, CAudioMixer **ppMixer );

	bool		IsSoundPlaying( CAudioMixer *pMixer );
	CAudioMixer *FindMixer( CAudioSource *source );

	void		StopAll( void );
	void		StopSound( CAudioMixer *mixer );

	void		GetAudioDevices(CUtlVector< audio_device_description_t >& deviceListOut) const;


private:
	struct CSoundFile
	{
		char				filename[ 512 ];
		CAudioSource		*source;
		long				filetime;
	};

	IAudioDevice *m_pAudioDevice;
	float		m_flElapsedTime;
	CUtlVector < CSoundFile > m_ActiveSounds;
};


//-----------------------------------------------------------------------------
// Singleton interface
//-----------------------------------------------------------------------------
static CSoundSystem s_SoundSystem;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CSoundSystem, ISoundSystem, SOUNDSYSTEM_INTERFACE_VERSION, s_SoundSystem );


//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CSoundSystem::Connect( CreateInterfaceFn factory )
{
	if ( !BaseClass::Connect( factory ) )
		return false;

	g_pDataCache = (IDataCache*)factory( DATACACHE_INTERFACE_VERSION, NULL );
	g_pSoundSystem = this;
	return (g_pFullFileSystem != NULL) && (g_pDataCache != NULL);
}

void CSoundSystem::Disconnect()
{
	g_pSoundSystem = NULL;
	g_pDataCache = NULL;
	BaseClass::Disconnect();
}


//-----------------------------------------------------------------------------
// Query interface
//-----------------------------------------------------------------------------
void *CSoundSystem::QueryInterface( const char *pInterfaceName )
{
	if (!Q_strncmp(	pInterfaceName, SOUNDSYSTEM_INTERFACE_VERSION, Q_strlen(SOUNDSYSTEM_INTERFACE_VERSION) + 1))
		return (ISoundSystem*)this;

	return NULL;
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
InitReturnVal_t CSoundSystem::Init()
{
	InitReturnVal_t nRetVal = BaseClass::Init();
	if ( nRetVal != INIT_OK )
		return nRetVal;

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );
	m_flElapsedTime = 0.0f;
	m_pAudioDevice = Audio_CreateWaveDevice();
	if ( !m_pAudioDevice->Init() )
		return INIT_FAILED;

	return INIT_OK;
}

void CSoundSystem::Shutdown()
{
	Msg( "Removing %i sounds\n", m_ActiveSounds.Count() );
	for ( int i = 0 ; i < m_ActiveSounds.Count(); i++ )
	{
		CSoundFile *p = &m_ActiveSounds[ i ];
		Msg( "Removing sound:  %s\n", p->filename );
		delete p->source;
	}

	m_ActiveSounds.RemoveAll();

	if ( m_pAudioDevice )
	{
		m_pAudioDevice->Shutdown();
		delete m_pAudioDevice;
	}

	BaseClass::Shutdown();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CAudioSource *CSoundSystem::FindOrAddSound( const char *filename )
{
	CSoundFile *s;

	int i;
	for ( i = 0; i < m_ActiveSounds.Count(); i++ )
	{
		s = &m_ActiveSounds[ i ];
		Assert( s );
		if ( !stricmp( s->filename, filename ) )
		{
			long filetime = g_pFullFileSystem->GetFileTime( filename );
			if ( filetime != s->filetime )
			{
				Msg( "Reloading sound %s\n", filename );
				delete s->source;
				s->source = LoadSound( filename );
				s->filetime = filetime;
			}
			return s->source;
		}
	}

	i = m_ActiveSounds.AddToTail();
	s = &m_ActiveSounds[ i ];
	strcpy( s->filename, filename );
	s->source = LoadSound( filename );
	s->filetime = g_pFullFileSystem->GetFileTime( filename );

	return s->source;
}

CAudioSource *CSoundSystem::LoadSound( const char *wavfile )
{
	if ( !m_pAudioDevice )
		return NULL;

	CAudioSource *wave = AudioSource_Create( wavfile );
	return wave;
}

void CSoundSystem::PlaySound( CAudioSource *source, float volume, CAudioMixer **ppMixer )
{
	if ( ppMixer )
	{
		*ppMixer = NULL;
	}

	if ( m_pAudioDevice )
	{
		CAudioMixer *mixer = source->CreateMixer();
		if ( ppMixer )
		{
			*ppMixer = mixer;
		}
		mixer->SetVolume( volume );
		m_pAudioDevice->AddSource( mixer );
	}
}

void CSoundSystem::Update( float dt )
{
//	closecaptionmanager->PreProcess( g_nSoundFrameCount );

	if ( m_pAudioDevice )
	{
		m_pAudioDevice->Update( m_flElapsedTime );
	}

//	closecaptionmanager->PostProcess( g_nSoundFrameCount, dt );

	m_flElapsedTime += dt;
	g_nSoundFrameCount++;
}

void CSoundSystem::Flush( void )
{
	if ( m_pAudioDevice )
	{
		m_pAudioDevice->Flush();
	}
}

void CSoundSystem::StopAll( void )
{
	if ( m_pAudioDevice )
	{
		m_pAudioDevice->StopSounds();
	}
}

void CSoundSystem::StopSound( CAudioMixer *mixer )
{
	int idx = m_pAudioDevice->FindSourceIndex( mixer );
	if ( idx != -1 )
	{
		m_pAudioDevice->FreeChannel( idx );
	}
}

static eSubSystems_t GetDefaultAudioSubsystem()
{
	eSubSystems_t nSubsystem = AUDIO_SUBSYSTEM_XAUDIO;
#if IS_WINDOWS_PC
	if (CommandLine()->CheckParm("-directsound"))
	{
		nSubsystem = AUDIO_SUBSYSTEM_DSOUND;
	}
#endif
	return nSubsystem;
}


void CSoundSystem::GetAudioDevices(CUtlVector< audio_device_description_t >& deviceListOut) const
{
	CAudioDeviceList list;
	eSubSystems_t nSubsystem = GetDefaultAudioSubsystem();

	list.BuildDeviceList(nSubsystem);

	deviceListOut = list.m_list;
}

bool CSoundSystem::IsSoundPlaying( CAudioMixer *pMixer )
{
	if ( !m_pAudioDevice || !pMixer )
		return false;

	//
	int index = m_pAudioDevice->FindSourceIndex( pMixer );
	if ( index != -1 )
		return true;

	return false;
}

CAudioMixer *CSoundSystem::FindMixer( CAudioSource *source )
{
	if ( !m_pAudioDevice )
		return NULL;

	return m_pAudioDevice->GetMixerForSource( source );
}