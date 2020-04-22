//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
//=============================================================================//

#include "cbase.h"

#include "shot_manipulator.h"
#include "paint_sprayer_shared.h"
#include "debugoverlay_shared.h"
#include "paint_stream_manager.h"

#ifdef CLIENT_DLL
	#include "c_paint_sprayer.h"
#else
	#include "paint_sprayer.h"
#endif

ConVar debug_paint_sprayer_cone("debug_paint_sprayer_cone", "0", FCVAR_REPLICATED | FCVAR_CHEAT);
ConVar max_noisy_blobs_per_second("max_noisy_blobs_per_second", "5.0f", FCVAR_REPLICATED | FCVAR_CHEAT);

float const MAX_SPRAYER_SPREAD_ANGLE = 89.f;

void CPaintSprayer::SprayPaint( float flDeltaTime )
{
	CPaintStream *pPaintStream = m_hPaintStream;
	if ( !pPaintStream )
		return;

	//Don't spray silent blobs on the clients
	if( m_bSilent && gpGlobals->IsClient() )
	{
		return;
	}

	if( flDeltaTime <= 0.0f )
	{
		return;
	}

	Vector vecSprayDir;
	AngleVectors( GetAbsAngles(), &vecSprayDir );

	m_flAccumulatedTime += flDeltaTime;

	const float fireDeltaTime = 1.0f / m_flBlobsPerSecond;
	int blobsFired = 0;

	while ( m_flAccumulatedTime > fireDeltaTime )
	{
		m_flAccumulatedTime -= fireDeltaTime;

		if ( pPaintStream->GetBlobsCount() >= m_nMaxBlobCount )
			continue;

		CPaintBlob *pBlob = FirePaintBlob( GetAbsOrigin(),
												GetAbsOrigin(),
												GetAbsVelocity(),
												vecSprayDir,
												m_PaintPowerType,
												m_flBlobSpreadRadius,
												m_flBlobSpreadAngle,
												m_flMinSpeed,
												m_flMaxSpeed,
												m_flStreakPercentage,
												m_flMinStreakTime,
												m_flMaxStreakTime,
												m_flMinStreakSpeedDampen,
												m_flMaxStreakSpeedDampen, 
												m_bSilent,
												m_bDrawOnly,
												pPaintStream,
												m_nBlobRandomSeed );

		if( pBlob )
		{
			m_flPercentageSinceLastNoisyBlob += m_flNoisyBlobPercentage;
			if( m_flPercentageSinceLastNoisyBlob >= 1.0f )
			{
				m_flPercentageSinceLastNoisyBlob = 0.0f;
				pBlob->SetShouldPlaySound( !m_bSilent );
			}
		}

		pPaintStream->AddPaintBlob( pBlob );

		++m_nBlobRandomSeed;
		++blobsFired;
	}

	// Note: Assume that if the sprayer is dripping, it's not spawning more than
	//		 one blob per frame and also that, if it did, we wouldn't want more
	//		 than one drip sound playing from the same position in a frame or within
	//		 a fraction of a second in general. That takes this conditional out of
	//		 the loop above.
#ifdef GAME_DLL
	if( !m_bSilent && blobsFired > 0 &&
		m_nAmbientSound == PAINT_SPRAYER_SOUND_DRIP )
	{
		CBroadcastRecipientFilter filter;
		CBaseEntity::EmitSound( filter, entindex(), "Paint.Drip", &GetAbsOrigin() );
	}
#endif
}


CPaintBlob* FirePaintBlob( const Vector& vecSourcePosition,
					  const Vector& vecOldSourcePosition,
					  const Vector& vecSourceVelocity,
					  const Vector& vecSprayDir,
					  int paintType,
					  float flBlobSpreadRadius,
					  float flBlobSpreadAngle,
					  float flMinSpeed,
					  float flMaxSpeed,
					  float flBlobStreakPercent,
					  float flMinStreakTime,
					  float flMaxStreakTime,
					  float flMinStreakSpeedDampen,
					  float flMaxStreakSpeedDampen,
					  bool bSilent,
					  bool bDrawOnly,
					  CBaseEntity *pOwner,
					  int nRandomSeed /*= 0*/ )
{
#ifdef CLIENT_DLL
	// if the client is listen server, don't create blobs twice, we don't want to double the work
	if ( engine->IsClientLocalToActiveServer() )
		return NULL;
#endif

	// set random seed
	RandomSeed( nRandomSeed );

	// clamp spread angle
	flBlobSpreadAngle = clamp( flBlobSpreadAngle, 0.f, MAX_SPRAYER_SPREAD_ANGLE );

	// random position inside the circle area
	Vector vecCircleRight, vecCircleUp;
	VectorVectors( vecSprayDir, vecCircleRight, vecCircleUp );
	vecCircleUp.NormalizeInPlace();
	VMatrix matRotate;
	MatrixBuildRotationAboutAxis( matRotate, vecSprayDir, RandomFloat( 0.f, 360.f ) );
	Vector vecBlobFirePos = vecSourcePosition + RandomFloat( 0.f, flBlobSpreadRadius ) * ( matRotate * vecCircleUp );

	// compute cone origin
	float flDistanceToConeOrigin = 1.f;
	Vector vecConeOrigin;
	if ( AlmostEqual( flBlobSpreadAngle, 0.f ) )
	{
		vecConeOrigin = vecBlobFirePos - flDistanceToConeOrigin * vecSprayDir.Normalized();
	}
	else
	{
		flDistanceToConeOrigin = flBlobSpreadRadius / tanf( DEG2RAD( flBlobSpreadAngle ) );
		vecConeOrigin = vecSourcePosition - flDistanceToConeOrigin * vecSprayDir.Normalized();
	}

	// compute direction from random position
	Vector vecBlobFireDir = vecBlobFirePos - vecConeOrigin;
	if ( AlmostEqual( flBlobSpreadRadius, 0.f ) )
	{
		float flSpread = sin( DEG2RAD( flBlobSpreadAngle )/2.0f );
		Vector vecSpraySpread( flSpread, flSpread, flSpread );
		CShotManipulator shotManipulator( vecSprayDir );
		vecBlobFireDir = shotManipulator.ApplySpread( vecSpraySpread );
	}
	vecBlobFireDir.NormalizeInPlace();

	if ( debug_paint_sprayer_cone.GetBool() )
	{
		QAngle debugAngle;
		VectorAngles( vecSprayDir, debugAngle );
#ifdef CLIENT_DLL
		NDebugOverlay::Circle( vecSourcePosition, debugAngle, flBlobSpreadRadius, 255, 0, 0, 128, true, 0.1f );
		NDebugOverlay::Line( vecBlobFirePos, vecBlobFirePos + 20 * vecBlobFireDir, 255, 255, 0, true, 1.f );
#else
		NDebugOverlay::Circle( vecSourcePosition, debugAngle, flBlobSpreadRadius, 0, 255, 0, 128, true, 0.1f );
		NDebugOverlay::Line( vecBlobFirePos, vecBlobFirePos + 20 * vecBlobFireDir, 0, 255, 255, true, 1.f );
#endif
	}

	Vector vecBlobVelocity = vecBlobFireDir * RandomFloat( flMinSpeed, flMaxSpeed ) + vecSourceVelocity;

	//The streaking values of the blob
	float flStreakTime = 0.0f;
	float flStreakSpeedDampen = 0.0f;
	bool bShouldStreak = ( RandomFloat( 0.0f, 1.0f ) * 100.0f ) <= flBlobStreakPercent;
	if( bShouldStreak )
	{
		flStreakTime = RandomFloat( flMinStreakTime, flMaxStreakTime );
		flStreakSpeedDampen = RandomFloat( flMinStreakSpeedDampen, flMaxStreakSpeedDampen );
	}

	CPaintBlob *pBlob = PaintStreamManager.AllocatePaintBlob( bSilent );
	if ( pBlob )
	{
		pBlob->Init( vecBlobFirePos, vecBlobVelocity, paintType, flStreakTime, flStreakSpeedDampen, pOwner, bSilent, bDrawOnly );
	}

	return pBlob;
}
