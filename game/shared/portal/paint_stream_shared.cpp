//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
//=============================================================================//
#include "cbase.h"

#include "paint_stream_shared.h"
#include "paint_stream_manager.h"

#include <functional>
#include <algorithm>

#include "paint_power_user.h"
#include "vstdlib/jobthread.h"

#ifdef CLIENT_DLL

#include "collisionutils.h"


#include "c_paint_stream.h"
#include "c_paintblob.h"

#else

#include "paint_stream.h"
#include "cpaintblob.h"

#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define VPROF_BUDGETGROUP_PAINTBLOB	_T("Paintblob")


ConVar max_sound_channels_per_paint_stream("max_sound_channels_per_paint_stream", "7", FCVAR_REPLICATED | FCVAR_CHEAT);


IMPLEMENT_SHAREDCLASS_DT( CPaintStream )
	SharedProp( m_sharedBlobData )
	SharedProp( m_sharedBlobDataMutex )
END_SHARED_TABLE()


void CPaintStream::UpdateOnRemove()
{
	RemoveAllPaintBlobs();

	BaseClass::UpdateOnRemove();
}


void CPaintStream::RemoveAllPaintBlobs( void )
{
#ifdef CLIENT_DLL
	RemoveFromLeafSystem();
#endif
}


unsigned int CPaintStream::GetBlobsCount() const
{
	return m_blobs.Count();
}


CPaintBlob* CPaintStream::GetBlob( int id )
{
	return m_blobs[id];
}


struct ShouldNotDeleteBlob_t : std::unary_function< CPaintBlob*, bool >
{
	inline bool operator()( const CPaintBlob* pBlob ) const
	{
		return !pBlob->ShouldDeleteThis();
	}
};


void CPaintStream::RemoveDeadBlobs()
{
	CPaintBlob** begin = GetBegin( m_blobs );
	CPaintBlob** end = GetEnd( m_blobs );
	CPaintBlob** middle = std::partition( begin, end, ShouldNotDeleteBlob_t() );
	for( CPaintBlob** i = middle; i != end; ++i )
	{
		PaintStreamManager.FreeBlob( *i );
	}

	int numRemoved = end - middle;
	m_blobs.RemoveMultipleFromTail( numRemoved );
}


struct TimeElapsed : public std::unary_function<TimeStamp, bool>
{
	TimeStamp m_CurrentTime;

	TimeElapsed( TimeStamp currentTime ) : m_CurrentTime( currentTime ) {}

	inline bool operator()( TimeStamp time ) const
	{
		return time <= m_CurrentTime;
	}
};

void CPaintStream::QueuePaintEffect()
{
#ifdef GAME_DLL
	// if not listen server, don't do anything
	if ( engine->IsDedicatedServer() )
		return;
#else
	// if we are listen server, don't do anything on client
	if ( engine->IsClientLocalToActiveServer() )
		return;
#endif

	// Update how many channels are in use
	TimeElapsed timeElapsedPred( gpGlobals->curtime );
	TimeStamp* begin = m_UsedChannelTimestamps.Base();
	TimeStamp* end = begin + m_UsedChannelTimestamps.Count();
	TimeStamp* newEnd = std::remove_if( begin, end, timeElapsedPred );
	m_UsedChannelTimestamps.RemoveMultipleFromTail( end - newEnd );

	// Try to queue up effects for each impact that occurred
	PaintImpactEffect const impactEffect = (m_nRenderMode == BLOB_RENDER_BLOBULATOR) ? PAINT_STREAM_IMPACT_EFFECT : PAINT_DRIP_IMPACT_EFFECT;
	int const maxChannels = max_sound_channels_per_paint_stream.GetInt();

	CUtlVectorFixedGrowable<Vector, 32> soundPositions;

	for ( int i = 0; i < m_blobs.Count(); ++i )
	{
		CPaintBlob *pBlob = m_blobs[i];
		if ( pBlob->ShouldPlayEffect() && !pBlob->IsSilent() )
		{
			PaintStreamManager.CreatePaintImpactParticles( pBlob->GetPosition(), pBlob->GetContactNormal(), m_nPaintType );
			if( pBlob->ShouldPlaySound() )
				soundPositions.AddToTail( pBlob->GetPosition() );
		}
	}

	if( soundPositions.Count() != 0 )
		PaintStreamManager.PlayMultiplePaintImpactSounds( m_UsedChannelTimestamps, maxChannels, soundPositions, impactEffect );
}


void CPaintStream::PreUpdateBlobs()
{
	RemoveDeadBlobs();
	DebugDrawBlobs();
}


void CPaintStream::PostUpdateBlobs()
{
	RemoveTeleportedThisFrameBlobs();

	UpdateRenderBoundsAndOriginWorldspace();
	QueuePaintEffect();

#ifdef GAME_DLL
	AddPaintToDatabase();
	UpdateBlobSharedData();
#endif

	// reset blobs teleported this frame flag
	ResetBlobsTeleportedThisFrame();
}


const Vector& CPaintStream::WorldAlignMins() const
{
	return m_vCachedWorldMins;
}


const Vector& CPaintStream::WorldAlignMaxs() const
{
	return m_vCachedWorldMaxs;
}


struct TeleportedThisFrameBlob_t : std::unary_function< CPaintBlob*, bool >
{
	TeleportedThisFrameBlob_t( int nMaxTeleportationCount ) : m_nMaxTeleportationCount( nMaxTeleportationCount )
	{
	}

	inline bool operator()( const CPaintBlob* pBlob ) const
	{
		return pBlob->HasBlobTeleportedThisFrame() && ( pBlob->GetTeleportationCount() >= m_nMaxTeleportationCount );
	}

	int m_nMaxTeleportationCount;
};


//-----------------------------------------------------------------------------------------------
// Purpose: Remove blobs that have teleported this frame if the stream reaches max blob count
//-----------------------------------------------------------------------------------------------
void CPaintStream::RemoveTeleportedThisFrameBlobs()
{
	if ( m_nRenderMode == BLOB_RENDER_FAST_SPHERE )
	{
		Assert( m_blobs.Count() <= m_nMaxBlobCount );
		if ( m_blobs.Count() == m_nMaxBlobCount )
		{
			CPaintBlob** begin = GetBegin( m_blobs );
			CPaintBlob** end = GetEnd( m_blobs );
			CPaintBlob** middle = std::partition( begin, end, TeleportedThisFrameBlob_t( 4 ) );
			for( CPaintBlob** i = begin; i != middle; ++i )
			{
				PaintStreamManager.FreeBlob( *i );
			}

			int numRemoved = middle - begin;
			m_blobs.RemoveMultipleFromHead( numRemoved );
		}
	}
}


void CPaintStream::ResetBlobsTeleportedThisFrame()
{
	for ( int i=0; i<m_blobs.Count(); ++i )
	{
		m_blobs[i]->SetBlobTeleportedThisFrame( false );
	}
}
