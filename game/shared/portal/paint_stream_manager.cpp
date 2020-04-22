//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
//=============================================================================//
#include "cbase.h"

#include <numeric>

#include "paint_stream_manager.h"
#include "paint_blobs_shared.h"
#include "paint_sprayer_shared.h"
#include "debugoverlay_shared.h"
#include "fmtstr.h"
#include "vprof.h"
#include "paint_stream_shared.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define VPROF_BUDGETGROUP_PAINTBLOB	_T("Paintblob")

const char* const CPaintStreamManager::m_pPaintMaterialNames[PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER] =
{
	"paintblobs/blob_surface_bounce",
	"paintblobs/blob_surface_stick", // FIXME: Bring this back for DLC2 "paintblobs/blob_surface_reflect",
	"paintblobs/blob_surface_speed",
	"paintblobs/blob_surface_portal",
	"paintblobs/blob_surface_erase"
};

const char* const CPaintStreamManager::s_SoundEffectNames[PAINT_IMPACT_EFFECT_COUNT] =
{
	"Paintblob.Impact",
	"Paintblob.ImpactDrip"
};

//Paint particle impact effect convars
ConVar paint_impact_particles_distance_threshold( "paint_impact_particles_distance_threshold", "20.0f", FCVAR_REPLICATED );
ConVar paint_impact_particles_duration( "paint_impact_particles_duration", "0.2f", FCVAR_REPLICATED );
ConVar paint_min_impact_particles( "paint_min_impact_particles", "20", FCVAR_REPLICATED );
ConVar paint_max_impact_particles( "paint_max_impact_particles", "50", FCVAR_REPLICATED );

ConVar paint_impact_accumulate_sound_distance_threshold( "paint_impact_accumulate_sound_distance_threshold", "128.0f", FCVAR_REPLICATED );
ConVar paint_impact_count_to_max_adjusted_volume( "paint_impact_count_to_max_adjusted_volume", "5", FCVAR_REPLICATED );
ConVar paint_impact_count_to_min_adjusted_pitch_after_full_volume( "paint_impact_count_to_min_adjusted_pitch_after_full_volume", "5", FCVAR_REPLICATED );
ConVar min_adjusted_pitch_percentage( "min_adjusted_pitch_percentage", "0.85", FCVAR_REPLICATED );

ConVar draw_paint_splat_particles( "draw_paint_splat_particles", "1", FCVAR_REPLICATED );

ConVar group_paint_impact_effects( "cl_group_paint_impact_effects", "1", FCVAR_REPLICATED );
ConVar debug_paint_impact_effects( "debug_paint_impact_effects", "0", FCVAR_REPLICATED );

ConVar blobs_paused("blobs_paused", "0", FCVAR_CHEAT | FCVAR_REPLICATED );

CPaintStreamManager PaintStreamManager( "PaintStreamManager" );

CPaintStreamManager::CPaintStreamManager( char const *name )
				: CAutoGameSystemPerFrame( name )
{
}


CPaintStreamManager::~CPaintStreamManager( void )
{
}


void CPaintStreamManager::LevelInitPreEntity()
{
	blobs_paused.SetValue( true );
}


void CPaintStreamManager::LevelInitPostEntity()
{
	blobs_paused.SetValue( false );
}


void CPaintStreamManager::LevelShutdownPostEntity()
{
	m_PaintImpactParticles.RemoveAll();

	if ( m_pBlobPool )
	{
		m_pBlobPool->Clear();
		delete m_pBlobPool;
		m_pBlobPool = NULL;
	}
}


void CPaintStreamManager::AllocatePaintBlobPool( int nMaxBlobs )
{
	int nMaxCount = ( nMaxBlobs ) ? nMaxBlobs : 250;
	// pre-allocate pool of blobs
	if ( !m_pBlobPool )
	{
#ifdef GAME_DLL
		m_pBlobPool = new CClassMemoryPool< CPaintBlob >( nMaxCount, CUtlMemoryPool::GROW_NONE );
#else
		if ( !engine->IsClientLocalToActiveServer() )
		{
			m_pBlobPool = new CClassMemoryPool< CPaintBlob >( nMaxCount, CUtlMemoryPool::GROW_NONE );
		}
#endif
	}
	else if ( ( m_pBlobPool->Size() / m_pBlobPool->BlockSize() ) != nMaxBlobs )
	{
		Assert( 0 );
		Warning( "CPaintStreamManager::AllocatePaintBlobPool is being called multiple times (for some reasons) with different pool sizes." );
	}
}


void CPaintStreamManager::RemoveAllPaintBlobs( void )
{
	for( int i = 0; i < IPaintStreamAutoList::AutoList().Count(); ++i )
	{
		CPaintStream *pStream = static_cast< CPaintStream* >( IPaintStreamAutoList::AutoList()[i] );
		if( pStream )
		{
			pStream->RemoveAllPaintBlobs();
		}
	}
}


CPaintBlob* CPaintStreamManager::AllocatePaintBlob( bool bSilent /*= false*/ )
{
	// don't create when blob is paused
	if ( blobs_paused.GetBool() )
	{
		return NULL;
	}

#ifdef CLIENT_DLL
	if ( bSilent )
	{
		return NULL;
	}
#endif

	return m_pBlobPool->Alloc();
}


void CPaintStreamManager::FreeBlob( CPaintBlob* pBlob )
{
	m_pBlobPool->Free( pBlob );
}


const char *CPaintStreamManager::GetPaintMaterialName( int type )
{
	return m_pPaintMaterialNames[ type ];
}


#ifdef CLIENT_DLL

void CPaintStreamManager::Update( float frametime )
{
	PaintStreamUpdate();

	//Update the particle and sound impact effects
	UpdatePaintImpactEffects( frametime, m_PaintImpactParticles );

	//Display a list of all the paint impact effects currently playing
	if( debug_paint_impact_effects.GetBool() )
	{
		int line = 6;
		float lineHeight = 0.015;

		float startX = 0.01f;
		float startY = 0.0f;

		CFmtStr msg;

		msg.sprintf( "Paint blob impact particles: %d", m_PaintImpactParticles.Count() );
		NDebugOverlay::ScreenText( startX, startY + (line * lineHeight), msg, 0, 255, 255, 255, NDEBUG_PERSIST_TILL_NEXT_SERVER );
		line++;
	}
}

#else

void CPaintStreamManager::PreClientUpdate( void )
{
	PaintStreamUpdate();

	//engine->Con_NPrintf( 0, "num blobs = %d", GetBlobCount() );

	//Update the particle and sound impact effects
	UpdatePaintImpactEffects( gpGlobals->frametime, m_PaintImpactParticles );

	//Display a list of all the paint impact effects currently playing
	if( debug_paint_impact_effects.GetBool() )
	{
		int line = 6;
		float lineHeight = 0.015;

		float startX = 0.01f;
		float startY = 0.0f;

		CFmtStr msg;

		msg.sprintf( "Paint blob impact particles: %d", m_PaintImpactParticles.Count() );
		NDebugOverlay::ScreenText( startX, startY + (line * lineHeight), msg, 0, 255, 255, 255, NDEBUG_PERSIST_TILL_NEXT_SERVER );
		line++;
	}
}

#endif


void CPaintStreamManager::PaintStreamUpdate()
{
	VPROF_BUDGET( "CPaintStreamManager::PaintStreamUpdate", VPROF_BUDGETGROUP_PAINTBLOB );

#ifdef CLIENT_DLL
	// if the client is local to server, only update render bounds
	// let the server do all the work.
	if ( engine->IsClientLocalToActiveServer() )
	{
		for ( int i = 0; i < IPaintStreamAutoList::AutoList().Count(); ++i )
		{
			CPaintStream *pStream = static_cast< CPaintStream* >( IPaintStreamAutoList::AutoList()[i] );
			if ( pStream )
			{
				pStream->UpdateRenderBoundsAndOriginWorldspace();
			}
		}
		return;
	}
#endif

	// remove dead blobs from beam list before we set dead blobs to NULL
	CTrigger_TractorBeam_Shared::RemoveDeadBlobsFromBeams();

	// we want to update blob collision for all blobs at once
	PaintBlobVector_t allBlobs;

	// preupdate (delete blobs from streams)
	for ( int i = 0; i < IPaintStreamAutoList::AutoList().Count(); ++i )
	{
		CPaintStream *pStream = static_cast< CPaintStream* >( IPaintStreamAutoList::AutoList()[i] );
		if ( pStream )
		{
			pStream->PreUpdateBlobs();

			int numCurrentBlobs = allBlobs.Count();
			int numNewBlobs = pStream->GetBlobList().Count();
			allBlobs.EnsureCount( allBlobs.Count() + numNewBlobs );
			V_memcpy( allBlobs.Base() + numCurrentBlobs, pStream->GetBlobList().Base(), numNewBlobs * sizeof( CBasePaintBlob* ) );
		}
	}

	
	// copy blobs from all stream and update all of them
	if ( blobs_paused.GetBool() )
	{
		for ( int i=0; i<allBlobs.Count(); ++i )
		{
			allBlobs[i]->SetLastUpdateTime( gpGlobals->curtime );
		}
	}
	else
	{
		// update all blobs
		PaintBlobUpdate( allBlobs );
	}

	// post update
	for ( int i = 0; i < IPaintStreamAutoList::AutoList().Count(); ++i )
	{
		CPaintStream *pStream = static_cast< CPaintStream* >( IPaintStreamAutoList::AutoList()[i] );
		if ( pStream )
		{
			pStream->PostUpdateBlobs();
		}
	}

	// remove blobs that change beams to correct blob list in beam
	CTrigger_TractorBeam_Shared::RemoveBlobsFromPreviousBeams();
}


void CPaintStreamManager::UpdatePaintImpactEffects( float flDeltaTime, PaintBlobImpactEffectVector_t &paintImpactEffects )
{
	//Update the paint blob impact effects
	for( int i = 0; i < paintImpactEffects.Count(); ++i )
	{
		PaintBlobImpactEffect_t *pEffect = &paintImpactEffects[i];

		if ( pEffect )
		{
			//Decrement the timer of the effect
			pEffect->flTime -= flDeltaTime;

			//Remove the effect if it has finished playing
			if( pEffect->flTime <= 0.0f )
			{
				paintImpactEffects.FastRemove( i-- );
			}
		}
	}
}

void CPaintStreamManager::CreatePaintImpactParticles( const Vector &vecPosition, const Vector &vecNormal, int paintType )
{
	//Check if the impact particle effect should be played
	if( ShouldPlayImpactEffect( vecPosition,
								m_PaintImpactParticles,
								paint_min_impact_particles.GetInt(),
								paint_max_impact_particles.GetInt(),
								paint_impact_particles_distance_threshold.GetFloat() * paint_impact_particles_distance_threshold.GetFloat() ) )
	{
		PlayPaintImpactParticles( vecPosition, vecNormal, paintType );
	}
}


bool CPaintStreamManager::ShouldPlayImpactEffect( const Vector& vecPosition, PaintBlobImpactEffectVector_t &paintImpactEffects, int minEffects, int maxEffects, float flDistanceThresholdSqr )
{
	if( !group_paint_impact_effects.GetBool() )
	{
		return true;
	}

	int iImpactEffectCount = paintImpactEffects.Count();

	//If we are below the min threshold then play the paint impact effect
	if( iImpactEffectCount < minEffects )
	{
		return true;
	}
	//Don't play any paint impact effect if we are above the max
	else if( iImpactEffectCount >= maxEffects )
	{
		return false;
	}

	int iEffectIndex = 0;

	//Don't play the effect if it's too close to another paint impact effect
	for ( iEffectIndex = 0; iEffectIndex < iImpactEffectCount; ++iEffectIndex )
	{
		PaintBlobImpactEffect_t *pEffect = &paintImpactEffects[iEffectIndex];

		if ( pEffect )
		{
			//Check if this effect is too close to a effect already playing
			if ( vecPosition.DistToSqr( pEffect->vecPosition ) < ( flDistanceThresholdSqr ) )
			{
				return false;
			}
		}
	}

	//OK to play the effect
	return true;
}


struct SplatParticlesForPaint_t
{
	int nPaintType;
	const char *lpszParticleSystemName;
};

SplatParticlesForPaint_t paintSplatCallbacks[] =
{
	{ BOUNCE_POWER,	"paint_splat_bounce_01" },
	{ REFLECT_POWER,"paint_splat_stick_01" }, // FIXME: Bring this back for DLC2 { REFLECT_POWER,"paint_splat_reflect_01" },
	{ SPEED_POWER,	"paint_splat_speed_01" },
	{ PORTAL_POWER,	"paint_splat_erase_01" },
	{ NO_POWER,		"paint_splat_erase_01" },
};

void PaintSplatEffect( const Vector& vecPosition, const Vector& vecNormal, int paintType )
{
	Assert( paintType >= 0 && paintType < ARRAYSIZE( paintSplatCallbacks ) );
	Assert( paintSplatCallbacks[paintType].nPaintType == paintType );

	QAngle angle;
	VectorAngles( -vecNormal, angle );

	CBasePlayer *pPlayer = NULL;
#ifdef GAME_DLL
	if ( !engine->IsDedicatedServer() )
	{
		pPlayer = UTIL_GetLocalPlayerOrListenServerHost();
	}
#else
	pPlayer = GetSplitScreenViewPlayer();
#endif

	if ( pPlayer )
	{
		CSingleUserRecipientFilter filter( pPlayer );
		DispatchParticleEffect( paintSplatCallbacks[paintType].lpszParticleSystemName, vecPosition, angle, NULL, -1, &filter );
	}
}


void CPaintStreamManager::PlayPaintImpactParticles( const Vector &vecPosition, const Vector &vecNormal, int paintType )
{
	//Play the particle effect for the impact
	if( draw_paint_splat_particles.GetBool() )
	{
		PaintSplatEffect( vecPosition, vecNormal, paintType );
	}

	//Add the effect to the list
	int iEffectIndex = m_PaintImpactParticles.AddToTail();
	m_PaintImpactParticles[iEffectIndex].vecPosition = vecPosition;
	m_PaintImpactParticles[iEffectIndex].flTime = paint_impact_particles_duration.GetFloat();
}


float CPaintStreamManager::PlayPaintImpactSound( const Vector &vecPosition, PaintImpactEffect impactEffect )
{
	//Emit the sound for the impact
#ifdef GAME_DLL
	CBasePlayer *pRecipient = UTIL_GetLocalPlayerOrListenServerHost();
	if ( pRecipient == NULL )
	{
		return 0.0f;
	}
	CSingleUserRecipientFilter filter( pRecipient );
#else
	CLocalPlayerFilter filter;
#endif

	const char* soundName = GetPaintSoundEffectName( impactEffect );
	CBaseEntity::EmitSound( filter, 0, soundName, &vecPosition );
	return CBaseEntity::GetSoundDuration( soundName, NULL );
}

typedef CUtlVectorFixedGrowable< Vector, 16 > AccumulatedSoundPositionVector;
struct AccumulatedImpactSound
{
	CSoundParameters soundParams;
	AccumulatedSoundPositionVector positions;
	float volumeIncreasePerImpact;
	int pitchDecreasePerFullVolumeImpact;
	int minAdjustedPitch;

	void Initialize( const Vector& center, const char* soundName );
};

void AccumulatedImpactSound::Initialize( const Vector& center, const char* soundName )
{
	positions.AddToTail( center );

	if( CBaseEntity::GetParametersForSound( soundName, soundParams, NULL ) )
	{
		const int impactsToMinPitch = paint_impact_count_to_min_adjusted_pitch_after_full_volume.GetInt();
		minAdjustedPitch = min_adjusted_pitch_percentage.GetFloat() * soundParams.pitch + 0.5f;
		const int deltaToMinPitch = soundParams.pitch - minAdjustedPitch;
		pitchDecreasePerFullVolumeImpact = static_cast<float>(deltaToMinPitch) / impactsToMinPitch + 0.5f;
		const int impactsToFullVolume = paint_impact_count_to_max_adjusted_volume.GetInt();
		const float deltaToFullVolume = VOL_NORM - soundParams.volume;
		volumeIncreasePerImpact = deltaToFullVolume / impactsToFullVolume;
	}
	else
	{
		Assert(!"GetParametersForSound() failed.");
	}
}

typedef CUtlVectorFixedGrowable< AccumulatedImpactSound, 32 > AccumulatedImpactSoundVector;

void CPaintStreamManager::PlayMultiplePaintImpactSounds( TimeStampVector& channelTimeStamps, int maxChannels, const PaintImpactPositionVector& positions, PaintImpactEffect impactEffect )
{
	const char* soundName = GetPaintSoundEffectName( impactEffect );

	Assert( positions.Count() > 0 && soundName != NULL );

	const int maxChannelsToAdd = imax( maxChannels - channelTimeStamps.Count(), 0 );
	if( positions.Count() > 0 && soundName != NULL && maxChannelsToAdd > 0 )
	{
		AccumulatedImpactSoundVector accumulatedSounds;
		accumulatedSounds.AddToTail();
		accumulatedSounds.Tail().Initialize( positions.Head(), soundName );

		const float maxRadiusSq = Sqr( paint_impact_accumulate_sound_distance_threshold.GetFloat() );

		// For each position
		for( int positionIndex = 1; positionIndex < positions.Count(); ++positionIndex )
		{
			// Check if the position is close enough to the center of an accumulated impact sound
			// Note: "Center" is just the first position in the list.
			const Vector& soundPosition = positions[positionIndex];
			bool positionAccumulated = false;
			for( int accumSoundIndex = 0; accumSoundIndex < accumulatedSounds.Count(); ++accumSoundIndex )
			{
				AccumulatedImpactSound& sound = accumulatedSounds[accumSoundIndex];
				const Vector& center = sound.positions.Head();
				if( (center - soundPosition).LengthSqr() < maxRadiusSq )
				{
					sound.positions.AddToTail( soundPosition );
					int& pitch = sound.soundParams.pitch; 
					float& volume = sound.soundParams.volume;
					const int adjustedPitch = pitch - isel( VOL_NORM - volume, sound.pitchDecreasePerFullVolumeImpact, 0 );
					pitch = imax( adjustedPitch, sound.minAdjustedPitch );
					volume = fpmin( volume + sound.volumeIncreasePerImpact, VOL_NORM ); 
					positionAccumulated = true;
					break;
				}
			}

			if( !positionAccumulated && accumulatedSounds.Count() < maxChannelsToAdd )
			{
				accumulatedSounds.AddToTail();
				accumulatedSounds.Tail().Initialize( soundPosition, soundName );
			}
		}

		// Play each accumulated sound
		for( int accumSoundIndex = 0; accumSoundIndex < accumulatedSounds.Count(); ++accumSoundIndex )
		{
			AccumulatedImpactSound& sound = accumulatedSounds[accumSoundIndex];

			// Find the average position
			const AccumulatedSoundPositionVector& soundPositions = sound.positions;
			Vector averagedCenter = std::accumulate( soundPositions.Base(), soundPositions.Base() + soundPositions.Count(), vec3_origin );
			averagedCenter /= soundPositions.Count();

			// Emit the sound
			EmitSound_t emitParams( sound.soundParams );
			emitParams.m_pOrigin = &averagedCenter;
			const float duration = PlayPaintImpactSound( emitParams );
		
			// Update the number of used channels
			channelTimeStamps.AddToTail( gpGlobals->curtime + duration );
		}
	}
}

const char* CPaintStreamManager::GetPaintSoundEffectName( unsigned int impactEffect )
{
	return impactEffect < PAINT_IMPACT_EFFECT_COUNT ? s_SoundEffectNames[impactEffect] : NULL;
}

float CPaintStreamManager::PlayPaintImpactSound( const EmitSound_t& emitParams )
{
	//Emit the sound for the impact
#ifdef GAME_DLL
	CBasePlayer *pRecipient = UTIL_GetLocalPlayerOrListenServerHost();
	if ( pRecipient == NULL )
	{
		return 0.0f;
	}
	CSingleUserRecipientFilter filter( pRecipient );
#else
	CLocalPlayerFilter filter;
#endif

	CBaseEntity::EmitSound( filter, 0, emitParams );
	return CBaseEntity::GetSoundDuration( emitParams.m_pSoundName, NULL ); // This will generate a "should use game_sounds.txt" warning, but the sound name comes from game_sounds.txt. The warning is benign.
}
