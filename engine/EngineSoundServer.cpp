//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "engine/IEngineSound.h"
#include "tier0/dbg.h"
#include "quakedef.h"
#include "vox.h"
#include "server.h"
#include "sv_main.h"
#include "edict.h"
#include "sound.h"
#include "host.h"
#include "vengineserver_impl.h"
#include "enginesingleuserfilter.h"
#include "snd_audio_source.h"
#include "soundchars.h"
#include "tier0/vprof.h"
#ifndef DEDICATED
#include "vgui_baseui_interface.h"
#endif
#include "SoundEmitterSystem/isoundemittersystembase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
//
// Server-side implementation of the engine sound interface
//
//-----------------------------------------------------------------------------
class CEngineSoundServer : public IEngineSound
{
public:
	// constructor, destructor
	CEngineSoundServer();
	virtual ~CEngineSoundServer();

	virtual bool PrecacheSound( const char *pSample, bool bPreload, bool bIsUISound );
	virtual bool IsSoundPrecached( const char *pSample );
	virtual void PrefetchSound( const char *pSample );
	virtual bool IsLoopingSound( const char *pSample )
	{
		Warning( "Can't call IsLoopingSound from server\n" );
		return false;
	}

	virtual float GetSoundDuration( const char *pSample );  

	virtual int EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH iSoundEntryHash, const char *pSample, 
		float flVolume, float flAttenuation, int nSeed, int iFlags, int iPitch, 
		const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime = 0.0f, int speakerentity = -1 );

	virtual int EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH iSoundEntryHash, const char *pSample, 
		float flVolume, soundlevel_t iSoundLevel, int nSeed, int iFlags, int iPitch, 
		const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime = 0.0f, int speakerentity = -1 );

	virtual void EmitSentenceByIndex( IRecipientFilter& filter, int iEntIndex, int iChannel, int iSentenceIndex, 
		float flVolume, soundlevel_t iSoundLevel, int nSeed, int iFlags, int iPitch,
		const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime = 0.0f, int speakerentity = -1 );

	virtual void StopSound( int iEntIndex, int iChannel, const char *pSample, HSOUNDSCRIPTHASH nSoundEntryHash = SOUNDEMITTER_INVALID_HASH );

	virtual void StopAllSounds( bool bClearBuffers );

	// Set the room type for a player
	virtual void SetRoomType( IRecipientFilter& filter, int roomType );
	virtual void SetPlayerDSP( IRecipientFilter& filter, int dspType, bool fastReset );

	// emit an "ambient" sound that isn't spatialized - specify left/right volume
	// only available on the client, assert on server
	virtual int EmitAmbientSound( const char *pSample, float flVolume, int iPitch, int flags, float soundtime = 0.0f );

	virtual float GetDistGainFromSoundLevel( soundlevel_t soundlevel, float dist );

	// Client .dll only functions
	virtual int		GetGuidForLastSoundEmitted()
	{ 
		Warning( "Can't call GetGuidForLastSoundEmitted from server\n" );
		return 0; 
	}
	virtual bool	IsSoundStillPlaying( int guid )
	{ 
		Warning( "Can't call IsSoundStillPlaying from server\n" );
		return false; 
	}
	virtual void	StopSoundByGuid( int guid, bool bForce )	
	{ 
		Warning( "Can't call StopSoundByGuid from server\n" );
		return; 
	}

	// Retrieves list of all active sounds
	virtual void	GetActiveSounds( CUtlVector< SndInfo_t >& sndlist )
	{ 
		Warning( "Can't call GetActiveSounds from server\n" );
		return; 
	}

	// Set's master volume (0.0->1.0)
	virtual void	SetVolumeByGuid( int guid, float fvol )
	{
		Warning( "Can't call SetVolumeByGuid from server\n" );
		return; 
	}
	// return's sound's current elapsed time
	virtual float GetElapsedTimeByGuid( int guid)
	{
		Warning( "Can't call GetElapsedTimeByGuid from server\n" );
		return 0.0; 
	}
	virtual void	PrecacheSentenceGroup( const char *pGroupName )
	{
		VOX_PrecacheSentenceGroup( this, pGroupName );
	}
	virtual void	NotifyBeginMoviePlayback()
	{
		AssertMsg( 0, "Not supported" );
	}

	virtual void	NotifyEndMoviePlayback()
	{
		AssertMsg( 0, "Not supported" );
	}

	virtual bool	IsMoviePlaying()
	{
		AssertMsg( 0, "Not supported" );
		return false;
	}

	virtual bool	GetSoundChannelVolume( const char* sound, float &flVolumeLeft, float &flVolumeRight )
	{
		Warning( "Can't call GetSoundChannelVolume from server\n" );
		return false;
	}

	virtual bool GetPreventSound( )
	{
		AssertMsg( 0, "Not supported" );
		return false;
	}


	virtual void SetReplaySoundFade( float flReplayVolume ) { Assert( !"Client-only service" ); }
	virtual float GetReplaySoundFade()const { Assert( !"Client-only service" );  return 0.0f; }

	virtual void SetReplayMusicGain( float flReplayMusicGain ) { Assert( !"Client-only service" ); }
	virtual float GetReplayMusicGain()const { Assert( !"Client-only service" );  return 0.0f; }

#if defined( _GAMECONSOLE )
	virtual void UnloadSound( const char *pSample )
	{
		AssertMsg( 0, "Not supported" );
	}
#endif

private:
	void EmitSoundInternal( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH iSoundEntryHash, const char *pSample, 
		float flVolume, soundlevel_t iSoundLevel, int nSeed, int iFlags, int iPitch, 
		const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime = 0.0f, int speakerentity = -1 );
};


//-----------------------------------------------------------------------------
// Client-server neutral sound interface accessor
//-----------------------------------------------------------------------------
static CEngineSoundServer s_EngineSoundServer;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CEngineSoundServer, IEngineSound, 
	IENGINESOUND_SERVER_INTERFACE_VERSION, s_EngineSoundServer );

IEngineSound *EngineSoundServer()
{
	return &s_EngineSoundServer;
}



//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CEngineSoundServer::CEngineSoundServer()
{
}

CEngineSoundServer::~CEngineSoundServer()
{
}


//-----------------------------------------------------------------------------
// Precache a particular sample
//-----------------------------------------------------------------------------
bool CEngineSoundServer::PrecacheSound( const char *pSample, bool bPreload, bool bIsUISound )
{
#ifndef DEDICATED
	EngineVGui()->UpdateProgressBar( PROGRESS_DEFAULT ); 
#endif
	int		i;

	if ( pSample && TestSoundChar( pSample, CHAR_SENTENCE ) )
	{
		return true;
	}

	if ( pSample[0] <= ' ' )
	{
		Host_Error( "CEngineSoundServer::PrecacheSound:  Bad string: %s", pSample );
	}
	
	// add the sound to the precache list
	// Start at 1, since 0 is used to indicate an error in the sound precache
	i = SV_FindOrAddSound( pSample, bPreload );
	if ( i >= 0 )
		return true;

	Host_Error( "CEngineSoundServer::PrecacheSound: '%s' overflow", pSample );
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSample - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CEngineSoundServer::IsSoundPrecached( const char *pSample )
{
	if ( pSample && TestSoundChar(pSample, CHAR_SENTENCE) )
	{
		return true;
	}

	int idx = SV_SoundIndex( pSample );
	if ( idx == -1 )
	{
		return false;
	}

	return true;
}

void CEngineSoundServer::PrefetchSound( const char *pSample )
{
	if ( pSample && TestSoundChar(pSample, CHAR_SENTENCE) )
	{
		return;
	}

	int idx = SV_SoundIndex( pSample );
	if ( idx == -1 )
	{
		return;
	}

	// Tell clients to prefetch the sound
	CSVCMsg_Prefetch_t msg;
	msg.set_sound_index( idx );

	sv.BroadcastMessage( msg, true, false );
}

//-----------------------------------------------------------------------------
// Stops a sound
//-----------------------------------------------------------------------------
void CEngineSoundServer::EmitSoundInternal( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH iSoundEntryHash, const char *pSample, 
	float flVolume, soundlevel_t iSoundLevel, int nSeed, int iFlags, int iPitch, 
	const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime /*= 0.0f*/, int speakerentity /*=-1*/ )
{
	AssertMsg( pDirection == NULL, "Direction specification not currently supported on server sounds" );
	AssertMsg( bUpdatePositions, "Non-updated positions not currently supported on server sounds" );

	if (flVolume < 0 || flVolume > 1)
	{
		Warning ("EmitSound: %s volume out of bounds = %f\n", pSample, flVolume);
		return;
	}

	if (iSoundLevel < MIN_SNDLVL_VALUE || iSoundLevel > MAX_SNDLVL_VALUE)
	{
		Warning ("EmitSound: %s soundlevel out of bounds = %d\n", pSample, iSoundLevel);
		return;
	}

	if (iPitch < 0 || iPitch > 255)
	{
		Warning ("EmitSound: %s pitch out of bounds = %i\n", pSample, iPitch);
		return;
	}

	edict_t *pEdict = (iEntIndex >= 0) ? &sv.edicts[iEntIndex] : NULL; 
	SV_StartSound( filter, pEdict, iChannel, pSoundEntry, iSoundEntryHash, pSample, flVolume, iSoundLevel,
		iFlags, iPitch, pOrigin, soundtime, speakerentity, pUtlVecOrigins, nSeed );
}


//-----------------------------------------------------------------------------
// Plays a sentence
//-----------------------------------------------------------------------------
void CEngineSoundServer::EmitSentenceByIndex( IRecipientFilter& filter, int iEntIndex, int iChannel, 
	int iSentenceIndex, float flVolume, soundlevel_t iSoundLevel, int nSeed, int iFlags, int iPitch,
	const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime /*= 0.0f*/, int speakerentity /*= -1*/ )
{
	if ( iSentenceIndex >= 0 )
	{
		char pName[8];
		Q_snprintf( pName, sizeof(pName), "!%d", iSentenceIndex );
		EmitSoundInternal( filter, iEntIndex, iChannel, NULL, SOUNDEMITTER_INVALID_HASH, pName, flVolume, iSoundLevel, nSeed,
			iFlags, iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity );
	}
}


//-----------------------------------------------------------------------------
// Emits a sound
//-----------------------------------------------------------------------------
int CEngineSoundServer::EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH iSoundEntryHash, const char *pSample, 
	float flVolume, float flAttenuation, int nSeed, int iFlags, int iPitch, 
	const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime /*= 0.0f*/, int speakerentity /*= -1*/ )
{
	VPROF( "CEngineSoundServer::EmitSound" );
	EmitSound( filter, iEntIndex, iChannel, pSoundEntry, iSoundEntryHash, pSample, flVolume, ATTN_TO_SNDLVL( flAttenuation ), nSeed, iFlags, 
		iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity );
	return -1;	// Negative for unsupported feature.
}

int CEngineSoundServer::EmitSound( IRecipientFilter& filter, int iEntIndex, int iChannel, const char *pSoundEntry, HSOUNDSCRIPTHASH iSoundEntryHash, const char *pSample, 
	float flVolume, soundlevel_t iSoundLevel, int nSeed, int iFlags, int iPitch, 
	const Vector *pOrigin, const Vector *pDirection, CUtlVector< Vector >* pUtlVecOrigins, bool bUpdatePositions, float soundtime /*= 0.0f*/, int speakerentity /*= -1*/ )
{
	VPROF( "CEngineSoundServer::EmitSound" );
	if ( pSample && TestSoundChar(pSample, CHAR_SENTENCE) )
	{
		int iSentenceIndex = -1;
		VOX_LookupString( PSkipSoundChars(pSample), &iSentenceIndex );
		if (iSentenceIndex >= 0)
		{
			EmitSentenceByIndex( filter, iEntIndex, iChannel, iSentenceIndex, flVolume,
				iSoundLevel, nSeed, iFlags, iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity );
		}
		else
		{
			DevWarning( 2, "Unable to find %s in sentences.txt\n", PSkipSoundChars(pSample) );
		}
	}
	else
	{
		EmitSoundInternal( filter, iEntIndex, iChannel, pSoundEntry, iSoundEntryHash, pSample, flVolume, iSoundLevel, nSeed,
			iFlags, iPitch, pOrigin, pDirection, pUtlVecOrigins, bUpdatePositions, soundtime, speakerentity );
	}
	return -1;	// Negative for unsupported feature.
}

void BuildRecipientList( CUtlVector< edict_t * >& list, const IRecipientFilter& filter )
{
	int c = filter.GetRecipientCount();
	for ( int i = 0; i < c; i++ )
	{
		int playerindex = filter.GetRecipientIndex( i );
		if ( playerindex < 1 || playerindex > sv.GetClientCount() )
			continue;

		CGameClient *cl = sv.Client( playerindex - 1 );
		// Never output to bots
		if ( cl->IsFakeClient() )
			continue;

		if ( !cl->IsSpawned() )
			continue;

		list.AddToTail( cl->edict );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			roomType - 
//-----------------------------------------------------------------------------
void CEngineSoundServer::SetRoomType( IRecipientFilter& filter, int roomType )
{
	CUtlVector< edict_t * > players;
	BuildRecipientList( players, filter );

	for ( int i = 0 ; i < players.Count(); i++ )
	{
		g_pVEngineServer->ClientCommand( players[ i ], "room_type %i\n", roomType );
	}
}

// Set the dsp preset for a player (client only)
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			dspType - 
//-----------------------------------------------------------------------------
void CEngineSoundServer::SetPlayerDSP( IRecipientFilter& filter, int dspType, bool fastReset )
{
	Assert( !fastReset );
	if ( fastReset )
	{
		Warning( "SetPlayerDSP:  fastReset only valid from client\n" );
	}

	CUtlVector< edict_t * > players;
	BuildRecipientList( players, filter );

	for ( int i = 0 ; i < players.Count(); i++ )
	{
		g_pVEngineServer->ClientCommand( players[ i ], "dsp_player %i\n", dspType );

		KeyValues *kvClientCmd = new KeyValues( "dsp_player" );
		kvClientCmd->SetInt( NULL, dspType );
		g_pVEngineServer->ClientCommandKeyValues( players[ i ], kvClientCmd );
	}
}

void CEngineSoundServer::StopAllSounds(bool bClearBuffers)
{
	AssertMsg( 0, "Not supported" );
}
// bool CEngineSoundServer::GetPreventSound( void )
// {
// 	AssertMsg( 0, "Not supported" );
// }

int CEngineSoundServer::EmitAmbientSound( const char *pSample, float flVolume, int iPitch, int flags, float soundtime /*= 0.0f*/ )
{
	AssertMsg( 0, "Not supported" );
	return 0;
}

//-----------------------------------------------------------------------------
// Stops a sound
//-----------------------------------------------------------------------------
void CEngineSoundServer::StopSound( int iEntIndex, int iChannel, const char *pSample, HSOUNDSCRIPTHASH nSoundEntryHash )
{
	CEngineRecipientFilter filter;
	filter.AddAllPlayers();
	filter.MakeReliable();

	int nFlags = SND_STOP | ( nSoundEntryHash != SOUNDEMITTER_INVALID_HASH ? SND_IS_SCRIPTHANDLE : 0 );

	EmitSound( filter, iEntIndex, iChannel, pSample, nSoundEntryHash, pSample, 0, SNDLVL_NONE, 0, nFlags, PITCH_NORM,
				NULL, NULL, NULL, true );
}



float SV_GetSoundDuration( const char *pSample )
{
#ifdef DEDICATED
	return 0; // TODO: make this return a real value (i.e implement an OS independent version of the sound code)
#else
	return AudioSource_GetSoundDuration( pSample );
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSample - 
// Output : float
//-----------------------------------------------------------------------------
float CEngineSoundServer::GetSoundDuration( const char *pSample )
{
	return Host_GetSoundDuration( pSample );
}

float CEngineSoundServer::GetDistGainFromSoundLevel( soundlevel_t soundlevel, float dist )
{
	return S_GetGainFromSoundLevel( soundlevel, dist );
}

/*
//-----------------------------------------------------------------------------
// FIXME: Move into the CEngineSoundServer class?
//-----------------------------------------------------------------------------
void Host_RestartAmbientSounds()
{
	if (!sv.active)
	{
		return;
	}
	
	
#ifndef DEDICATED
	const int 	NUM_INFOS = 64;
	SoundInfo_t soundInfo[NUM_INFOS];

	int nSounds = S_GetCurrentStaticSounds( soundInfo, NUM_INFOS, CHAN_STATIC );
	
	for ( int i = 0; i < nSounds; i++)
	{
		if (soundInfo[i].looping && 
			soundInfo[i].entity != -1 )
		{
			Msg("Restarting sound %s...\n", soundInfo[i].name);
			S_StopSound(soundInfo[i].entity, soundInfo[i].channel);
			CEngineRecipientFilter filter;
			filter.AddAllPlayers();

			SV_StartSound( filter, EDICT_NUM(soundInfo[i].entity), 
						   CHAN_STATIC, 
						   soundInfo[i].name, 
						   soundInfo[i].volume,
						   soundInfo[i].soundlevel,
						   0,                    // @Q (toml 05-09-02): Is this correct, or will I need to squirrel away the original flags?
						   soundInfo[i].pitch );
		}
	}
#endif
} */



