
#include "audio_pch.h"

#include "matchmaking/imatchframework.h"
#include "tier2/tier2.h"
#include "fmtstr.h"

#include "audio/public/voice.h"

#if !defined( DEDICATED ) && ( defined( OSX ) || defined( _WIN32 ) || defined( LINUX ) ) && !defined( NO_STEAM )
#include "cl_steamauth.h"
#include "client.h"
#if defined( PS3SDK_INSTALLED ) 
#define PS3_CROSS_PLAY
#endif
#if !defined( CSTRIKE15 )
#if defined( PS3_CROSS_PLAY ) || defined ( _PS3 )
#include "ps3sceCelp8.h"
#define SOUND_PC_CELP_ENABLED
#endif 
#endif // CSTRIKE15
extern IVEngineClient *engineClient;
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CEngineVoiceStub *Audio_GetEngineVoiceStub()
{
	static CEngineVoiceStub s_EngineVoiceStub;
	return &s_EngineVoiceStub;
}


#if !defined( DEDICATED ) && ( defined( OSX ) || defined( _WIN32 ) || defined( LINUX ) ) && !defined( NO_STEAM )

ConVar snd_voice_echo( "snd_voice_echo", "0", FCVAR_DEVELOPMENTONLY );

// #define SND_VOICE_LOG_DEBUG
#ifdef SND_VOICE_LOG_DEBUG

ConVar snd_voice_log( "snd_voice_log", "0", FCVAR_DEVELOPMENTONLY ); // 1 = record, 2 = playback
enum SndVoiceLog_t
{
	SND_VOICE_LOG_RECORD_REMOTE = 1,
	SND_VOICE_LOG_TEST_PLAYBACK_LOG = 2,
	SND_VOICE_LOG_RECORD_REMOTE_11025 = 4,
	SND_VOICE_LOG_RECORD_LOCAL = 8,
	SND_VOICE_LOG_RECORD_LOCAL_11025 = 16,
};
CUtlBuffer g_bufSndVoiceLog;
CUtlBuffer g_bufSndVoiceLog11025;

CON_COMMAND( snd_voice_log_commit, "Commit voice log to file" )
{
	if ( ( args.ArgC() <= 1 ) || ( g_bufSndVoiceLog.TellMaxPut() <= 0 ) )
	{
		Msg( "Voice log size: %u/%u\n", g_bufSndVoiceLog.TellMaxPut(), g_bufSndVoiceLog11025.TellMaxPut() );
		return;
	}
	if ( !strcmp( "0", args.Arg( 1 ) ) )
	{
		Msg( "Voice log discarded\n" );
		g_bufSndVoiceLog.Purge();
		g_bufSndVoiceLog11025.Purge();
		return;
	}
	
	if ( g_pFullFileSystem->WriteFile( args.Arg( 1 ), NULL, g_bufSndVoiceLog ) )
	{
		Msg( "Voice log committed to file '%s', %u bytes\n", args.Arg(1), g_bufSndVoiceLog.TellMaxPut() );
		g_bufSndVoiceLog.Purge();
	}
	else
	{
		Warning( "Failed to commit voice log to file '%s', keeping %u bytes\n", args.Arg(1), g_bufSndVoiceLog.TellMaxPut() );
	}
	
	if ( g_pFullFileSystem->WriteFile( CFmtStr( "%s.11025", args.Arg( 1 ) ), NULL, g_bufSndVoiceLog11025 ) )
	{
		Msg( "Voice log committed to file '%s.11025', %u bytes\n", args.Arg(1), g_bufSndVoiceLog11025.TellMaxPut() );
		g_bufSndVoiceLog11025.Purge();
	}
	else
	{
		Warning( "Failed to commit voice log to file '%s.11025', keeping %u bytes\n", args.Arg(1), g_bufSndVoiceLog11025.TellMaxPut() );
	}
}

CON_COMMAND( snd_voice_log_load, "Load voice log file" )
{
	g_bufSndVoiceLog.Purge();
	if ( !g_pFullFileSystem->ReadFile( args.Arg( 1 ), NULL, g_bufSndVoiceLog ) )
	{
		Warning( "Failed to read voice log from file '%s'\n", args.Arg( 1 ) );
	}
	else
	{
		Msg( "Loaded voice log from file '%s'\n", args.Arg( 1 ) );
	}
}

CON_COMMAND( snd_voice_log_setsin, "Set voice log sin-wave" )
{
	g_bufSndVoiceLog.Purge();

	const int kNumSamples = 500000;

	g_bufSndVoiceLog.EnsureCapacity( kNumSamples * sizeof( int16 ) );
	g_bufSndVoiceLog.SeekPut( CUtlBuffer::SEEK_HEAD, kNumSamples * sizeof( int16 ) );

	float flSinFreq = 2.0/128.0;
	if ( args.ArgC() > 1 )
	{
		flSinFreq = atof( args.Arg( 1 ) );
		if ( flSinFreq <= 0.00001 )
			flSinFreq = 0.00001;
	}
	
	// PURELY FOR EXAMPLE: a basic sine wave test
	if ( 1 ) {
		for ( int q = 0 ; q < kNumSamples; ++ q )
		{
			float f = sin( flSinFreq * M_PI * q ) * 20000;
			( ( int16 * ) g_bufSndVoiceLog.Base() )[q] = f;
		}
	}
}

#endif // SND_VOICE_LOG_DEBUG

#define SOUND_PC_CELP_FREQ 8000

template < int nSOURCE, int nTARGET >
struct ResampleGeneric_t
{
public:
	ResampleGeneric_t() : m_sampLeftover( 0 ), m_flFractionalSamples( 0.0f ) { }

	uint32 Resample( const int16 *piSamples, uint32 numInSamples, int16 *poSamples )
	{
		if ( !poSamples )
			return ( 1 + ( numInSamples / nSOURCE ) ) * nTARGET;
		if ( !numInSamples )
			return 0;

		int numOutSamples = 0;
		const int16 *piSamplesEnd = piSamples + numInSamples;
		for ( int16 nextSamp = *( piSamples ++ ); ; m_flFractionalSamples += ((float) nSOURCE)/((float) nTARGET) )
		{
			// if we've passed a sample boundary, go onto the next sample
			while ( m_flFractionalSamples >= 1.0f )
			{
				m_flFractionalSamples -= 1.0f;
				m_sampLeftover = nextSamp;
				if ( piSamples < piSamplesEnd )
				{
					nextSamp = *( piSamples ++ );
				}
				else
				{
					return numOutSamples;
				}
			}

			*( poSamples ++ ) = Lerp( m_flFractionalSamples, m_sampLeftover,  nextSamp ); // left
			++ numOutSamples;
		}
	}

private:
	float m_flFractionalSamples;
	int16 m_sampLeftover;
};

typedef ResampleGeneric_t< SOUND_PC_CELP_FREQ, VOICE_OUTPUT_SAMPLE_RATE > Resample_CELP_to_PC_t; // 320 CELP samples = 441 PC samples
typedef ResampleGeneric_t< VOICE_OUTPUT_SAMPLE_RATE, SOUND_PC_CELP_FREQ > Resample_PC_to_CELP_t; // 441 PC samples = 320 PC samples

#ifdef SND_VOICE_LOG_DEBUG

CON_COMMAND( snd_voice_log_resample, "Resample voice log file" )
{
	g_bufSndVoiceLog.Purge();
	CUtlBuffer bufRaw;
	if ( !g_pFullFileSystem->ReadFile( args.Arg( 1 ), NULL, bufRaw ) )
	{
		Warning( "Failed to read voice log from file '%s'\n", args.Arg( 1 ) );
		return;
	}
	else
	{
		Msg( "Loaded voice log from file '%s'\n", args.Arg( 1 ) );
	}

	Resample_CELP_to_PC_t rcpt;
	g_bufSndVoiceLog.EnsureCapacity( bufRaw.TellPut() * 2 );
	uint32 numResamples = rcpt.Resample( (int16*)bufRaw.Base(), bufRaw.TellPut()/2, (int16*) g_bufSndVoiceLog.Base() );
	g_bufSndVoiceLog.SeekPut( CUtlBuffer::SEEK_HEAD, numResamples*2 );
	Msg( "Resampled voice log from %d to %d samples\n", bufRaw.TellPut()/2, numResamples );
}

CON_COMMAND( snd_voice_log_resample44, "Resample voice log file all the way up to 44100" )
{
	g_bufSndVoiceLog.Purge();
	CUtlBuffer bufRaw;
	if ( !g_pFullFileSystem->ReadFile( args.Arg( 1 ), NULL, bufRaw ) )
	{
		Warning( "Failed to read voice log from file '%s'\n", args.Arg( 1 ) );
		return;
	}
	else
	{
		Msg( "Loaded voice log from file '%s'\n", args.Arg( 1 ) );
	}

	Resample_CELP_to_PC_t rcpt;
	CUtlBuffer buf11025;
	buf11025.EnsureCapacity( bufRaw.TellPut() * 2 );
	uint32 numResamples = rcpt.Resample( (int16*)bufRaw.Base(), bufRaw.TellPut()/2, (int16*) buf11025.Base() );
	buf11025.SeekPut( CUtlBuffer::SEEK_HEAD, numResamples*2 );
	Msg( "Resampled voice log from %d to %d @11025 samples\n", bufRaw.TellPut()/2, numResamples );

	g_bufSndVoiceLog.EnsureCapacity( buf11025.TellPut() * 4 );
	int16 *pIn = (int16*) buf11025.Base();
	int16 *pInEnd = pIn + numResamples;
	int16 *pOut = (int16*) g_bufSndVoiceLog.Base();
	while ( pIn < pInEnd )
	{
		int16 a = *( pIn++ );
		int16 b = ( pIn < pInEnd ) ? *pIn : a;

		for ( int jj = 0; jj < 4; ++ jj )
		{
			*( pOut ++ ) = ( double( b ) * ( jj ) + double( a ) * ( 4 - jj ) ) / 4.0;
		}
	}
	g_bufSndVoiceLog.SeekPut( CUtlBuffer::SEEK_HEAD, numResamples*8 );
}

#endif // SND_VOICE_LOG_DEBUG

class CEngineVoiceSteam : public IEngineVoice
{
public:
	CEngineVoiceSteam();
	~CEngineVoiceSteam();

public:
	virtual bool IsHeadsetPresent( int iController );
	virtual bool IsLocalPlayerTalking( int iController );

	virtual void AddPlayerToVoiceList( XUID xPlayer, int iController, uint64 uiFlags );
	virtual void RemovePlayerFromVoiceList( XUID xPlayer, int iController );

	virtual void GetRemoteTalkers( int *pNumTalkers, XUID *pRemoteTalkers );

	virtual bool VoiceUpdateData( int iController );
	virtual void GetVoiceData( int iController, const byte **ppvVoiceDataBuffer, unsigned int *pnumVoiceDataBytes );
	virtual void VoiceResetLocalData( int iController );

	virtual void SetPlaybackPriority( XUID remoteTalker, int iController, int iAllowPlayback );
	virtual void PlayIncomingVoiceData( XUID xuid, const byte *pbData, unsigned int dwDataSize, const bool *bAudiblePlayers = NULL );

	virtual void RemoveAllTalkers();

protected:
	void AudioInitializationUpdate();
	void UpdateHUDVoiceStatus();
	bool IsPlayerTalking( XUID xuid );

public:
	bool m_bLocalVoice[ XUSER_MAX_COUNT ];
	struct RemoteTalker_t
	{
		XUID m_xuid;
		uint64 m_uiFlags;
		float m_flLastTalkTimestamp;
	};
	CUtlVector< RemoteTalker_t > m_arrRemoteVoice;
	bool m_bVoiceForPs3;
	int FindRemoteTalker( XUID xuid )
	{
		for ( int k = 0; k < m_arrRemoteVoice.Count(); ++ k )
			if ( m_arrRemoteVoice[k].m_xuid == xuid )
				return k;
		return m_arrRemoteVoice.InvalidIndex();
	}
	bool m_bInitializedAudio;
	byte m_pbVoiceData[ 18*1024 * XUSER_MAX_COUNT ];
	float m_flLastTalkingTimestamp;

#if defined( PS3_CROSS_PLAY ) || defined ( _PS3 )	
#ifdef SOUND_PC_CELP_ENABLED
	sceCelp8encHandle m_sceCelp8encHandle;
	sceCelp8decHandle m_sceCelp8decHandle;
	CUtlVectorFixed< byte, SCE_CELP8ENC_INPUT_SIZE > m_bufEncLeftover;
	CUtlVectorFixed< byte, SCE_CELP8DEC_INPUT_SIZE > m_bufDecLeftover;
#endif // SOUND_PC_CELP_ENABLED
	Resample_CELP_to_PC_t m_resampleCelp2Pc;
	Resample_PC_to_CELP_t m_resamplePc2Celp;
#endif
};

CEngineVoiceSteam::CEngineVoiceSteam()
{
	memset( m_bLocalVoice, 0, sizeof( m_bLocalVoice ) );
	memset( m_pbVoiceData, 0, sizeof( m_pbVoiceData ) );
	m_bInitializedAudio = false;
	m_bVoiceForPs3 = false;
	m_flLastTalkingTimestamp = 0.0f;

#if defined( PS3_CROSS_PLAY ) || defined ( _PS3 )
#ifdef SOUND_PC_CELP_ENABLED
	m_sceCelp8encHandle = NULL;
	sceCelp8encAttr encQueryAttr;
	if ( sceCelp8encQueryAttr( &encQueryAttr ) < 0 )
	{
		Warning( "ERROR: Failed to configure PS3 voice encoder!\n" );
	}
	else
	{
		DevMsg( "PS3 voice encoder version %d.%d.%d.%d [%u]\n",
			(encQueryAttr.verNumber&0xff000000)>>24,
			(encQueryAttr.verNumber&0xff0000)>>16,
			(encQueryAttr.verNumber&0xff00)>>8,
			(encQueryAttr.verNumber&0xff),
			encQueryAttr.memSize );
		m_sceCelp8encHandle = malloc( encQueryAttr.memSize );
		if ( m_sceCelp8encHandle )
			sceCelp8encInitInstance( m_sceCelp8encHandle );
	}
#endif

#ifdef SOUND_PC_CELP_ENABLED
	m_sceCelp8decHandle = NULL;
	sceCelp8decAttr decQueryAttr;
	if ( sceCelp8decQueryAttr( &decQueryAttr ) < 0 )
	{
		Warning( "ERROR: Failed to configure PS3 voice decoder!\n" );
	}
	else
	{
		DevMsg( "PS3 voice decoder version %d.%d.%d.%d [%u]\n",
			(decQueryAttr.verNumber&0xff000000)>>24,
			(decQueryAttr.verNumber&0xff0000)>>16,
			(decQueryAttr.verNumber&0xff00)>>8,
			(decQueryAttr.verNumber&0xff),
			decQueryAttr.memSize );
		m_sceCelp8decHandle = malloc( decQueryAttr.memSize );
		if ( m_sceCelp8decHandle )
			sceCelp8decInitInstance( m_sceCelp8decHandle );
	}
#endif
#endif
}

CEngineVoiceSteam::~CEngineVoiceSteam()
{
#if defined( PS3_CROSS_PLAY ) || defined ( _PS3 )
#ifdef SOUND_PC_CELP_ENABLED
	if ( m_sceCelp8encHandle )
		free( m_sceCelp8encHandle );
	m_sceCelp8encHandle = NULL;

	if ( m_sceCelp8decHandle )
		free( m_sceCelp8decHandle );
	m_sceCelp8decHandle = NULL;
#endif // SOUND_PC_CELP_ENABLED
#endif
}

bool CEngineVoiceSteam::IsHeadsetPresent( int iController )
{
	return false;
}

bool CEngineVoiceSteam::IsLocalPlayerTalking( int iController )
{
#ifdef _PS3
	EVoiceResult res = Steam3Client().SteamUser()->GetAvailableVoice( NULL, NULL );
#else
	EVoiceResult res = Steam3Client().SteamUser()->GetAvailableVoice( NULL, NULL, 0 );
#endif
	switch ( res )
	{
	case k_EVoiceResultOK:
	case k_EVoiceResultNoData:
		return true;
	default:
		return ( ( Plat_FloatTime() - m_flLastTalkingTimestamp ) <= 0.2f );
	}
}

bool CEngineVoiceSteam::IsPlayerTalking( XUID uid )
{
	if ( !g_pMatchFramework->GetMatchSystem()->GetMatchVoice()->CanPlaybackTalker( uid ) )
		return false;
	
	for ( int k = 0; k < m_arrRemoteVoice.Count(); ++ k )
	{
		if ( m_arrRemoteVoice[k].m_xuid == uid )
		{
			return ( ( Plat_FloatTime() - m_arrRemoteVoice[k].m_flLastTalkTimestamp ) < 0.2 );
		}
	}
	return false;
}

void CEngineVoiceSteam::AddPlayerToVoiceList( XUID xPlayer, int iController, uint64 uiFlags )
{
	if ( !xPlayer && iController >= 0 && iController < XUSER_MAX_COUNT )
	{
		// Add local player
		m_bLocalVoice[ iController ] = true;
		AudioInitializationUpdate();
		
		if ( snd_voice_echo.GetBool() )
		{
#if defined( PS3_CROSS_PLAY ) || defined ( _PS3 )		
			m_bVoiceForPs3 = (snd_voice_echo.GetInt() == 2);
#endif
			Steam3Client().SteamUser()->StartVoiceRecording();
		}
	}

	if ( xPlayer )
	{
		if ( FindRemoteTalker( xPlayer ) == m_arrRemoteVoice.InvalidIndex() )
		{
			RemoteTalker_t rt = { xPlayer, uiFlags, 0 };
			m_arrRemoteVoice.AddToTail( rt );
#if defined( PS3_CROSS_PLAY ) || defined ( _PS3 )			
			m_bVoiceForPs3 = !!(uiFlags & ENGINE_VOICE_FLAG_PS3);
#endif
			AudioInitializationUpdate();
			// Steam3Client().SteamUser()->StartVoiceRecording();
		}
	}
}

void CEngineVoiceSteam::RemovePlayerFromVoiceList( XUID xPlayer, int iController )
{
	if ( !xPlayer && iController >= 0 && iController < XUSER_MAX_COUNT )
	{
		// Remove local player
		m_bLocalVoice[ iController ] = false;
		AudioInitializationUpdate();
		Steam3Client().SteamUser()->StopVoiceRecording();
	}

	if ( xPlayer )
	{
		int idx = FindRemoteTalker( xPlayer );
		if ( idx != m_arrRemoteVoice.InvalidIndex() )
		{
			m_arrRemoteVoice.FastRemove( idx );
			AudioInitializationUpdate();
		}
	}
}

void CEngineVoiceSteam::GetRemoteTalkers( int *pNumTalkers, XUID *pRemoteTalkers )
{
	if ( pNumTalkers )
		*pNumTalkers = m_arrRemoteVoice.Count();

	if ( pRemoteTalkers )
	{
		for ( int k = 0; k < m_arrRemoteVoice.Count(); ++ k )
			pRemoteTalkers[k] = m_arrRemoteVoice[k].m_xuid;
	}
}

bool CEngineVoiceSteam::VoiceUpdateData( int iController )
{
#ifdef SND_VOICE_LOG_DEBUG
	if ( snd_voice_log.GetInt() == SND_VOICE_LOG_TEST_PLAYBACK_LOG )
	{
		PlayIncomingVoiceData( 2, NULL, 0, NULL );
		return false;
	}
#endif // SND_VOICE_LOG_DEBUG

#ifdef _PS3
	EVoiceResult res = Steam3Client().SteamUser()->GetAvailableVoice( NULL, NULL );
#else
	EVoiceResult res = Steam3Client().SteamUser()->GetAvailableVoice( NULL, NULL, 0 );
#endif
	bool bResult = ( res == k_EVoiceResultOK );
	if ( bResult )
		m_flLastTalkingTimestamp = Plat_FloatTime();
	UpdateHUDVoiceStatus();
	return bResult;
}

void CEngineVoiceSteam::UpdateHUDVoiceStatus( void )
{
	for ( int iClient = 0; iClient < GetBaseLocalClient().m_nMaxClients; iClient++ )
	{
		// Information about local client if it's a local client speaking
		bool bLocalClient = false;
		int iSsSlot = -1;
		int iCtrlr = -1;

		// Detection if it's a local client
		for ( DWORD k = 0; k < XBX_GetNumGameUsers(); ++ k )
		{
			CClientState &cs = GetLocalClient( k );
			if ( cs.m_nPlayerSlot == iClient )
			{
				bLocalClient = true;
				iSsSlot = k;
				iCtrlr = XBX_GetUserId( iSsSlot );
			}
		}

		// Convert into index and XUID
		int iIndex = iClient + 1;
		XUID xid =  NULL;
		player_info_t infoClient;
		if ( engineClient->GetPlayerInfo( iIndex, &infoClient ) )
		{
			xid = infoClient.xuid;
		}

		if ( !xid )
			// No XUID means no VOIP
		{
			g_pSoundServices->OnChangeVoiceStatus( iIndex, -1, false );
			if ( bLocalClient )
				g_pSoundServices->OnChangeVoiceStatus( iIndex, iSsSlot, false );
			continue;
		}

		// Determine talking status
		bool bTalking = false;

		if ( bLocalClient )
		{
			//Make sure the player's own "remote" label is not on.
			g_pSoundServices->OnChangeVoiceStatus( iIndex, -1, false );
			iIndex = -1; // issue notification as ent=-1
			bTalking = IsLocalPlayerTalking( iCtrlr );
		}
		else
		{
			bTalking = IsPlayerTalking( xid );
		}

		g_pSoundServices->OnChangeVoiceStatus( iIndex, iSsSlot, bTalking );
	}
}

void CEngineVoiceSteam::GetVoiceData( int iController, const byte **ppvVoiceDataBuffer, unsigned int *pnumVoiceDataBytes )
{
	const int size = ARRAYSIZE( m_pbVoiceData ) / XUSER_MAX_COUNT;
	byte *pbVoiceData = m_pbVoiceData + iController * ARRAYSIZE( m_pbVoiceData ) / XUSER_MAX_COUNT;
	*ppvVoiceDataBuffer = pbVoiceData;
	
	EVoiceResult res = k_EVoiceResultOK;
	if ( !m_bVoiceForPs3 )
	{
#ifdef _PS3
		res = Steam3Client().SteamUser()->GetVoice( true, pbVoiceData, size, pnumVoiceDataBytes, false, NULL, 0, NULL );
#else
		res = Steam3Client().SteamUser()->GetVoice( true, pbVoiceData, size, pnumVoiceDataBytes, false, NULL, 0, NULL, 0 );
#endif
	}
#if defined( PS3_CROSS_PLAY ) || defined ( _PS3 )	
	else
	{
#ifdef _PS3
		res = Steam3Client().SteamUser()->GetVoice( true, pbVoiceData, size, pnumVoiceDataBytes, false, NULL, 0, NULL );
#else
		res = Steam3Client().SteamUser()->GetVoice( true, pbVoiceData, size, pnumVoiceDataBytes, false, NULL, 0, NULL, 0 );
#endif

#if defined( SOUND_PC_CELP_ENABLED )
		if ( !m_sceCelp8encHandle )
			res = k_EVoiceResultNotRecording;
#endif

		int16 *pbUncompressedVoiceData = ( int16* ) stackalloc( 11025*2 );
		unsigned int numUncompressedVoiceBytes = 11025*2;
		if ( res == k_EVoiceResultOK )
		{
#ifdef _PS3
			res = Steam3Client().SteamUser()->DecompressVoice( pbVoiceData, *pnumVoiceDataBytes, pbUncompressedVoiceData, numUncompressedVoiceBytes, &numUncompressedVoiceBytes );
#else
			res = Steam3Client().SteamUser()->DecompressVoice( pbVoiceData, *pnumVoiceDataBytes, pbUncompressedVoiceData, numUncompressedVoiceBytes, &numUncompressedVoiceBytes, 11025 );
#endif
		}

		if ( res == k_EVoiceResultOK )
		{
			uint32 numOutSamples = m_resamplePc2Celp.Resample( pbUncompressedVoiceData, numUncompressedVoiceBytes/2, ( int16* ) pbVoiceData );
			*pnumVoiceDataBytes = numOutSamples * 2;

#ifdef SND_VOICE_LOG_DEBUG
			if ( snd_voice_log.GetInt() & SND_VOICE_LOG_RECORD_LOCAL_11025 )
			{
				g_bufSndVoiceLog11025.Put( pbUncompressedVoiceData, numUncompressedVoiceBytes );
			}
			if ( snd_voice_log.GetInt() & SND_VOICE_LOG_RECORD_LOCAL )
			{
				g_bufSndVoiceLog.Put( pbVoiceData, numOutSamples * 2 );
			}
#endif // SND_VOICE_LOG_DEBUG

#if defined( SOUND_PC_CELP_ENABLED )
			byte *pbSrc = pbVoiceData;
			byte *pbDst = pbVoiceData;
			byte *pbSrcEnd = pbSrc + *pnumVoiceDataBytes;

			while ( pbSrc < pbSrcEnd )
			{
				// Copy src data into encoding buffer, advance src
				int numBytesRoomForEncode = SCE_CELP8ENC_INPUT_SIZE - m_bufEncLeftover.Count();
				numBytesRoomForEncode = MIN( numBytesRoomForEncode, pbSrcEnd - pbSrc );
				m_bufEncLeftover.AddMultipleToTail( numBytesRoomForEncode, pbSrc );
				pbSrc += numBytesRoomForEncode;

				// If we have sufficient number of bytes for encoding, then encode, advance dst
				if ( m_bufEncLeftover.Count() == SCE_CELP8ENC_INPUT_SIZE )
				{
					byte encBuffer[ SCE_CELP8ENC_OUTPUT_SIZE ] = {0};
					int numBytesGenerated = 0;
					int encResult = sceCelp8encEncode( m_sceCelp8encHandle, m_bufEncLeftover.Base(), encBuffer, &numBytesGenerated );
					if ( encResult < 0 )
						numBytesGenerated = 0;
					if ( numBytesGenerated > 0 )
					{
						V_memcpy( pbDst, encBuffer, numBytesGenerated );
						pbDst += numBytesGenerated;
					}
					m_bufEncLeftover.RemoveAll();
				}
				else
					break;
			}
			// Set the number of bytes after encoding process
			*pnumVoiceDataBytes = pbDst - pbVoiceData;

#endif
		}
		if ( res != k_EVoiceResultOK )
		{
			*pnumVoiceDataBytes = 0;
			*ppvVoiceDataBuffer = NULL;
			return;
		}
	}
#endif

	// On PC respect user push-to-talk setting and don't transmit voice
	// if push-to-talk key is not held
	static ConVarRef voice_system_enable( "voice_system_enable" ); // voice system is initialized
	static ConVarRef voice_enable( "voice_enable" ); // mute all player voice data, and don't send voice data for this player
	static ConVarRef voice_vox( "voice_vox" ); // open mic
	static ConVarRef voice_ptt( "voice_ptt" ); // mic ptt release time
	if ( !voice_system_enable.GetBool() || !voice_enable.GetBool() )
		goto prevent_voice_comm;
	if ( !voice_vox.GetInt() )
	{
		float flPttReleaseTime = voice_ptt.GetFloat();
		if ( flPttReleaseTime && ( ( Plat_FloatTime() - flPttReleaseTime ) > 1.0f ) )
		{
			// User is in push-to-talk mode and released the talk key over a second ago
			// don't transmit any voice in this case
			goto prevent_voice_comm;
		}
	}
	
	switch ( res )
	{
	case k_EVoiceResultNoData:
	case k_EVoiceResultOK:
		return;
	default:
prevent_voice_comm:
		*pnumVoiceDataBytes = 0;
		*ppvVoiceDataBuffer = NULL;
		return;
	}
}

void CEngineVoiceSteam::VoiceResetLocalData( int iController )
{
	const int size = ARRAYSIZE( m_pbVoiceData ) / XUSER_MAX_COUNT;
	byte *pbVoiceData = m_pbVoiceData + iController * ARRAYSIZE( m_pbVoiceData ) / XUSER_MAX_COUNT;
	memset( pbVoiceData, 0, size );
}

void CEngineVoiceSteam::SetPlaybackPriority( XUID remoteTalker, int iController, int iAllowPlayback )
{
	;
}

void CEngineVoiceSteam::PlayIncomingVoiceData( XUID xuid, const byte *pbData, unsigned int dwDataSize, const bool *bAudiblePlayers /* = NULL */ )
{
	int idxRemoteTalker = 0;
	for ( DWORD dwSlot = 0; dwSlot < XBX_GetNumGameUsers(); ++ dwSlot )
	{
#ifdef _GAMECONSOLE
		int iCtrlr = XBX_GetUserId( dwSlot );
#else
		int iCtrlr = dwSlot;
#endif
		IPlayerLocal *pPlayer = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( iCtrlr );
		if ( pPlayer && pPlayer->GetXUID() == xuid )
		{
			//Hack: Don't play stuff that comes from ourselves.
			if ( snd_voice_echo.GetBool() )
			{
				if ( !m_arrRemoteVoice.Count() )
				{
					RemoteTalker_t rt = { 0, m_bVoiceForPs3 ? ENGINE_VOICE_FLAG_PS3 : 0, 0 };
					m_arrRemoteVoice.AddToTail( rt );
				}
				goto playvoice;
			}
			return;
		}
	}

#ifdef SND_VOICE_LOG_DEBUG
	if ( snd_voice_log.GetInt() == SND_VOICE_LOG_TEST_PLAYBACK_LOG )
	{
		if ( !m_arrRemoteVoice.Count() )
		{
			RemoteTalker_t rt = { 0, m_bVoiceForPs3 ? ENGINE_VOICE_FLAG_PS3 : 0, 0 };
			m_arrRemoteVoice.AddToTail( rt );
		}
	}
	else
#endif
	{
		// Make sure voice playback is allowed for the specified user
		if ( !g_pMatchFramework->GetMatchSystem()->GetMatchVoice()->CanPlaybackTalker( xuid ) )
			return;

		// Find registered remote talker
		idxRemoteTalker = FindRemoteTalker( xuid );
		if ( idxRemoteTalker == m_arrRemoteVoice.InvalidIndex() )
			return;
	}

playvoice:
	// Uncompress the voice data
	char pbUncompressedVoice[ 11025 * 2 ];
	uint32 numUncompressedBytes = 0;

#ifdef SND_VOICE_LOG_DEBUG
	if ( snd_voice_log.GetInt() == SND_VOICE_LOG_TEST_PLAYBACK_LOG )
	{
		const int nVoiceLogFreq = 11025;
		const int nVoiceLogSamples = 441;

		static float s_flLastTime = 0;
		static int s_nBufferPosition = 0;
		if ( g_bufSndVoiceLog.TellPut() <= nVoiceLogSamples )
			return;

		if ( !s_flLastTime )
		{
			s_flLastTime = Plat_FloatTime();
			return;
		}

		// See how many bytes we can send, assuming 16,000 bytes/sec (8000Hz), rounding to nearest 320 bytes
		float flCurTime = Plat_FloatTime();
		if ( flCurTime - s_flLastTime > 1.0f )
		{
			// drop frames
			s_flLastTime = Plat_FloatTime();
			return;
		}
		int numSoundFrames = nVoiceLogFreq * ( flCurTime - s_flLastTime ) / nVoiceLogSamples;
		if ( numSoundFrames <= 0 )
			return;

		// Advance time pointer
		s_flLastTime += numSoundFrames * nVoiceLogSamples /  float( nVoiceLogFreq );

		int16 *piWritePos = ( int16 * ) pbUncompressedVoice;
		while ( numSoundFrames --> 0 )
		{
			// See if we need to reset buffer position
			if ( s_nBufferPosition + nVoiceLogSamples > g_bufSndVoiceLog.TellPut() )
				s_nBufferPosition = 0;

			// uint32 numOutSamples = m_resampleCelp2Pc.Resample( ( ( const int16 * ) g_bufSndVoiceLog.Base() ) + ( s_nBufferPosition / 2 ), 160, piWritePos );
					uint32 numOutSamples = nVoiceLogSamples;
					V_memcpy( piWritePos, ( ( const int16 * ) g_bufSndVoiceLog.Base() ) + ( s_nBufferPosition / sizeof( int16 ) ), nVoiceLogSamples* sizeof( int16 ) );
			piWritePos += numOutSamples;
			numUncompressedBytes += numOutSamples * 2;
			s_nBufferPosition += nVoiceLogSamples * sizeof( int16 );
		}
		if ( !numUncompressedBytes )
			return;
	}
	else
#endif // SND_VOICE_LOG_DEBUG
	if ( !(m_arrRemoteVoice[idxRemoteTalker].m_uiFlags & ENGINE_VOICE_FLAG_PS3) )
	{
#ifdef _PS3
		EVoiceResult res = Steam3Client().SteamUser()->DecompressVoice( const_cast< byte * >( pbData ), dwDataSize,
			pbUncompressedVoice, sizeof( pbUncompressedVoice ), &numUncompressedBytes );
#else
		EVoiceResult res = Steam3Client().SteamUser()->DecompressVoice( const_cast< byte * >( pbData ), dwDataSize,
			pbUncompressedVoice, sizeof( pbUncompressedVoice ), &numUncompressedBytes, 11025 );
#endif

		if ( res != k_EVoiceResultOK )
			return;
	}
#if defined( PS3_CROSS_PLAY ) || defined ( _PS3 )	
#ifdef SOUND_PC_CELP_ENABLED
	else if ( m_sceCelp8decHandle )
	{
		int numCelpCompressedDecodedBufferMaxBytes = SOUND_PC_CELP_FREQ * 2;
		byte *pbCelpCompressedDecodedBuffer = ( byte * ) stackalloc( numCelpCompressedDecodedBufferMaxBytes );
		byte *pbSrc = const_cast< byte * >( pbData ), *pbDst = pbCelpCompressedDecodedBuffer;
		byte *pbSrcEnd = pbSrc + dwDataSize, *pbDstEnd = pbDst + numCelpCompressedDecodedBufferMaxBytes;
		while ( pbSrc < pbSrcEnd )
		{
			// Copy src data into decoding buffer, advance src
			int numBytesRoomForDecode = SCE_CELP8DEC_INPUT_SIZE - m_bufDecLeftover.Count();
			numBytesRoomForDecode = MIN( numBytesRoomForDecode, pbSrcEnd - pbSrc );
			m_bufDecLeftover.AddMultipleToTail( numBytesRoomForDecode, pbSrc );
			pbSrc += numBytesRoomForDecode;

			// If we have sufficient number of bytes for encoding, then encode, advance dst
			if ( m_bufDecLeftover.Count() == SCE_CELP8DEC_INPUT_SIZE )
			{
				byte decBuffer[ SCE_CELP8DEC_OUTPUT_SIZE ] = {0};
				int numBytesGenerated = SCE_CELP8DEC_OUTPUT_SIZE;
#ifdef SOUND_PC_CELP_ENABLED
				int decResult = sceCelp8decDecode( m_sceCelp8decHandle, m_bufDecLeftover.Base(), decBuffer, 0 );
#else
				int decResult = -1;
#endif
#ifdef SND_VOICE_LOG_DEBUG
				if ( snd_voice_log.GetInt() & SND_VOICE_LOG_RECORD_REMOTE )
				{
					g_bufSndVoiceLog.Put( decBuffer, numBytesGenerated );
				}
#endif

				if ( decResult < 0 )
					numBytesGenerated = 0;
				if ( ( numBytesGenerated > 0 ) && ( pbDst + numBytesGenerated <= pbDstEnd ) )
				{
					// even if we have no room to store decoded data, keep decoding
					// to keep decoder state consistent
					V_memcpy( pbDst, decBuffer, numBytesGenerated );
					pbDst += numBytesGenerated;
				}
				m_bufDecLeftover.RemoveAll();
			}
			else
				break;
		}
		// Set the number of bytes after encoding process
		numUncompressedBytes = pbDst - pbCelpCompressedDecodedBuffer;

		uint32 numOutSamples = m_resampleCelp2Pc.Resample( ( const int16 * ) pbCelpCompressedDecodedBuffer, numUncompressedBytes/2, ( int16 * ) pbUncompressedVoice );
		numUncompressedBytes = numOutSamples * 2;
		if ( !numOutSamples )
			return;

#ifdef SND_VOICE_LOG_DEBUG
		if ( snd_voice_log.GetInt() & SND_VOICE_LOG_RECORD_REMOTE_11025 )
		{
			g_bufSndVoiceLog11025.Put( pbUncompressedVoice, numUncompressedBytes );
		}
#endif // SND_VOICE_LOG_DEBUG
	}
#endif // SOUND_PC_CELP_ENABLED
#endif // PS3_CROSS_PLAY  || _PS3

	// Voice channel index
	int idxVoiceChan = idxRemoteTalker;

	int nChannel = Voice_GetChannel( idxVoiceChan );
	if ( nChannel == VOICE_CHANNEL_ERROR )
	{
		// Create a channel in the voice engine and a channel in the sound engine for this guy.
		nChannel = Voice_AssignChannel( idxVoiceChan, false, 0.0f );
	}

	// Give the voice engine the data (it in turn gives it to the mixer for the sound engine).
	if ( nChannel != VOICE_CHANNEL_ERROR )
	{
		Voice_AddIncomingData( nChannel, reinterpret_cast<const char*>(pbData), dwDataSize, 0, 0, 0, VoiceFormat_Steam );
		m_arrRemoteVoice[idxRemoteTalker].m_flLastTalkTimestamp = Plat_FloatTime();
	}
}

void CEngineVoiceSteam::RemoveAllTalkers()
{
	memset( m_bLocalVoice, 0, sizeof( m_bLocalVoice ) );
	m_arrRemoteVoice.RemoveAll();
	AudioInitializationUpdate();
}

void CEngineVoiceSteam::AudioInitializationUpdate()
{
	bool bHasTalkers = ( m_arrRemoteVoice.Count() > 0 );
	for ( int k = 0; k < ARRAYSIZE( m_bLocalVoice ); ++ k )
	{
		if ( m_bLocalVoice[k] )
		{
			bHasTalkers = true;
			break;
		}
	}

	// Initialized already
	if ( bHasTalkers == m_bInitializedAudio )
		return;

	// Clear out voice buffers
	memset( m_pbVoiceData, 0, sizeof( m_pbVoiceData ) );

	// Init or deinit voice system
	if ( bHasTalkers )
	{
		Voice_ForceInit();
	}
	else
	{
		Voice_Deinit();
	}

	m_bInitializedAudio = bHasTalkers;
}


IEngineVoice *Audio_GetEngineVoiceSteam()
{
	static CEngineVoiceSteam s_EngineVoiceSteam;
	return &s_EngineVoiceSteam;
}

#else

IEngineVoice *Audio_GetEngineVoiceSteam()
{
	return Audio_GetEngineVoiceStub();
}

#endif
