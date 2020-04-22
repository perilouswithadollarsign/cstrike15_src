//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: A blood spray effect to expose successful hits.
//
//=============================================================================//

#include "cbase.h"
#include "precache_register.h"
#include "fx_sparks.h"
#include "iefx.h"
#include "c_te_effect_dispatch.h"
#include "particles_ez.h"
#include "decals.h"
#include "engine/IEngineSound.h"
#include "fx_quad.h"
#include "engine/ivdebugoverlay.h"
#include "shareddefs.h"
#include "fx_blood.h"
#include "view.h"
#include "c_cs_player.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


PRECACHE_REGISTER_BEGIN( GLOBAL, PrecacheEffectCSBloodSpray )
PRECACHE( PARTICLE_SYSTEM, "blood_impact_basic" )
PRECACHE( PARTICLE_SYSTEM, "blood_impact_heavy" )
PRECACHE( PARTICLE_SYSTEM, "blood_impact_medium" )
PRECACHE( PARTICLE_SYSTEM, "blood_impact_light" )
PRECACHE( PARTICLE_SYSTEM, "blood_impact_light_headshot" )
PRECACHE_REGISTER_END()

static ConVar damage_impact_heavy( "damage_impact_heavy", "40", FCVAR_NONE, "Damage ABOVE this value is considered heavy damage" );
static ConVar damage_impact_medium( "damage_impact_medium", "20", FCVAR_NONE, "Damage BELOW this value is considered light damage" );

class CHitEffectRamp
{
public:
	float m_flDamageAmount;
	
	float m_flMinAlpha;
	float m_flMaxAlpha;

	float m_flMinSize;
	float m_flMaxSize;

	float m_flMinVelocity;
	float m_flMaxVelocity;
};


void InterpolateRamp( 
	const CHitEffectRamp &a,
	const CHitEffectRamp &b,
	CHitEffectRamp &out,
	int iDamage )
{
	float t = RemapVal( iDamage, a.m_flDamageAmount, b.m_flDamageAmount, 0, 1 );

	out.m_flMinAlpha = FLerp( a.m_flMinAlpha, b.m_flMinAlpha, t );
	out.m_flMaxAlpha = FLerp( a.m_flMaxAlpha, b.m_flMaxAlpha, t );
	out.m_flMinAlpha = clamp( out.m_flMinAlpha, 0, 255 );
	out.m_flMaxAlpha = clamp( out.m_flMaxAlpha, 0, 255 );

	out.m_flMinSize = FLerp( a.m_flMinSize, b.m_flMinSize, t );
	out.m_flMaxSize = FLerp( a.m_flMaxSize, b.m_flMaxSize, t );

	out.m_flMinVelocity = FLerp( a.m_flMinVelocity, b.m_flMinVelocity, t );
	out.m_flMaxVelocity = FLerp( a.m_flMaxVelocity, b.m_flMaxVelocity, t );
}


void FX_HitEffectSmoke(
	CSmartPtr<CBloodSprayEmitter> pEmitter, 
	int iDamage,
	const Vector &vEntryPoint,
	const Vector &vDirection,
	float flScale)
{
	SimpleParticle newParticle;
	PMaterialHandle hMaterial = ParticleMgr()->GetPMaterial( "effects/blood_gore" );

	// These parameters create a ramp based on how much damage the shot did.
	CHitEffectRamp ramps[2] =
	{
		{ 
			0, 
			30,		// min/max alpha
			70, 
			0.5,	// min/max size
			1,
			0,		// min/max velocity (not used here)
			0
		},
		
		{ 
			50, 
			30,		// min/max alpha 
			70,
			1,		// min/max size
			2,
			0,		// min/max velocity (not used here)
			0
		}
	};

	CHitEffectRamp interpolatedRamp;
	InterpolateRamp( 
		ramps[0], 
		ramps[1], 
		interpolatedRamp,
		iDamage );

	for ( int i=0; i < 2; i++ )
	{
		SimpleParticle &newParticle = *pEmitter->AddSimpleParticle( hMaterial, vEntryPoint, 0, 0 );

		newParticle.m_flLifetime = 0.0f;
		newParticle.m_flDieTime	= 3.0f;

		newParticle.m_uchStartSize	= random->RandomInt( 
			interpolatedRamp.m_flMinSize, 
			interpolatedRamp.m_flMaxSize ) * flScale;
		newParticle.m_uchEndSize	= newParticle.m_uchStartSize * 4;
		
		newParticle.m_vecVelocity = Vector( 0, 0, 5 ) + RandomVector( -2, 2 );
		newParticle.m_uchStartAlpha	= random->RandomInt( 
			interpolatedRamp.m_flMinSize,
			interpolatedRamp.m_flMaxSize );
		newParticle.m_uchEndAlpha	= 0;

		newParticle.m_flRoll			= random->RandomFloat( 0, 360 );
		newParticle.m_flRollDelta	= random->RandomFloat( -1, 1 );

		newParticle.m_iFlags = SIMPLE_PARTICLE_FLAG_NO_VEL_DECAY;

		float colorRamp = random->RandomFloat( 0.5f, 1.25f );

		newParticle.m_uchColor[0] = MIN( 1.0f, colorRamp ) * 255.0f;
		newParticle.m_uchColor[1] = MIN( 1.0f, colorRamp ) * 255.0f;
		newParticle.m_uchColor[2] = MIN( 1.0f, colorRamp ) * 255.0f;
	}	
}


void FX_HitEffectBloodSpray(
	CSmartPtr<CBloodSprayEmitter> pEmitter, 
	int iDamage,
	const Vector &vEntryPoint, 
	const Vector &vSprayNormal,
	const char *pMaterialName,
	float flLODDistance,
	float flDistanceScale,
	float flScale,
	float flSpeed )
{
	PMaterialHandle hMaterial = ParticleMgr()->GetPMaterial( pMaterialName );
	SimpleParticle *pParticle;

	float color[3] = { 1.0, 0, 0 };

	Vector up( 0, 0, 1 );
	Vector right = up.Cross( vSprayNormal );
	VectorNormalize( right );

	// These parameters create a ramp based on how much damage the shot did.
	CHitEffectRamp ramps[2] =
	{
		{ 
			0, 
			80,		// min/max alpha
			128, 
			flScale/2,// min/max size
			flScale,
			10,		// min/max velocity
			20
		},
		
		{ 
			50, 
			80,		// min/max alpha 
			128,
			flScale/2,// min/max size
			flScale,
			30,		// min/max velocity
			60
		}
	};

	CHitEffectRamp interpolatedRamp;
	InterpolateRamp( 
		ramps[0], 
		ramps[1], 
		interpolatedRamp,
		iDamage );

	for ( int i = 0; i < 6; i++ )
	{
		// Originate from within a circle '2 * scale' inches in diameter.
		Vector offset = vEntryPoint + ( flScale * vSprayNormal * 0.5 );
		offset += right * random->RandomFloat( -1, 1 ) * flScale;
		offset += up * random->RandomFloat( -1, 1 ) * flScale;

		pParticle = pEmitter->AddSimpleParticle( hMaterial, offset, 0, 0 );

		if ( pParticle != NULL )
		{
			pParticle->m_flLifetime = 0.0f;
			pParticle->m_flDieTime	= random->RandomFloat( 0.7f, 1.3f);

			// All the particles are between red and white. The whiter the particle is, the slower it goes.
			float whiteness = random->RandomFloat( 0.1, 0.7 );
			float speedFactor = 1 - whiteness;

			float spread = 0.5f;
			pParticle->m_vecVelocity.Random( -spread, spread );
			pParticle->m_vecVelocity += vSprayNormal * random->RandomInt( interpolatedRamp.m_flMinVelocity, interpolatedRamp.m_flMaxVelocity ) * flSpeed * speedFactor;

			float colorRamp = random->RandomFloat( 0.5f, 0.75f ) + flLODDistance;
			
			pParticle->m_uchColor[0]	= MIN( 1.0f, color[0] * colorRamp ) * 255.0f;
			pParticle->m_uchColor[1]	= MIN( 1.0f, whiteness * colorRamp ) * 255.0f;
			pParticle->m_uchColor[2]	= MIN( 1.0f, whiteness * colorRamp ) * 255.0f;
			
			pParticle->m_uchStartSize	= random->RandomFloat( interpolatedRamp.m_flMinSize, interpolatedRamp.m_flMaxSize ) * flDistanceScale;
			pParticle->m_uchEndSize		= pParticle->m_uchStartSize * 4 * flDistanceScale;
			
			pParticle->m_uchStartAlpha	= random->RandomInt( interpolatedRamp.m_flMinAlpha, interpolatedRamp.m_flMaxAlpha );
			pParticle->m_uchEndAlpha	= 0;
			
			pParticle->m_flRoll			= random->RandomInt( 0, 360 );
			pParticle->m_flRollDelta	= random->RandomFloat( -4.0f, 4.0f );
		}
	}
}


void FX_HitEffectBloodSplatter(
	CSmartPtr<CBloodSprayEmitter> pTrailEmitter, 
	int iDamage,
	const Vector &vExitPoint, 
	const Vector &vSplatterNormal,
	float flLODDistance )
{
	float flScale = 4;

	pTrailEmitter->SetSortOrigin( vExitPoint );
	PMaterialHandle	hMaterial = ParticleMgr()->GetPMaterial( "effects/blood_drop" );

	Vector up( 0, 0, 1 );
	Vector right = up.Cross( vSplatterNormal );
	VectorNormalize( right );

	// These parameters create a ramp based on how much damage the shot did.
	CHitEffectRamp ramps[2] =
	{
		{ 
			0, 
			0,		// min/max alpha
			75, 
			1.5f,  // min/max size
			2.0f,
			25.0f * flScale,		// min/max velocity
			35.0f * flScale
		},
		
		{ 
			50, 
			0,		// min/max alpha 
			140,
			1.5f,// min/max size
			2.0f,
			65.0f * flScale,		// min/max velocity
			75.0f * flScale
		}
	};

	CHitEffectRamp interpolatedRamp;
	InterpolateRamp( 
		ramps[0], 
		ramps[1], 
		interpolatedRamp,
		iDamage );


	for ( int i = 0; i < 20; i++ )
	{
		// Originate from within a circle 'scale' inches in diameter.
		Vector offset = vExitPoint;
		offset += right * random->RandomFloat( -0.15f, 0.15f ) * flScale;
		offset += up * random->RandomFloat( -0.15f, 0.15f ) * flScale;

		SimpleParticle *tParticle = (SimpleParticle*)pTrailEmitter->AddSimpleParticle( 
			hMaterial, 
			vExitPoint,
			random->RandomFloat( 0.225f, 0.35f ),
			random->RandomFloat( interpolatedRamp.m_flMinSize, interpolatedRamp.m_flMaxSize )
			);

		if ( tParticle == NULL )
			break;

		Vector offDir = vSplatterNormal + RandomVector( -0.05f, 0.05f );

		tParticle->m_vecVelocity = offDir * random->RandomFloat( interpolatedRamp.m_flMinVelocity, interpolatedRamp.m_flMaxVelocity );

		tParticle->m_iFlags = SIMPLE_PARTICLE_FLAG_NO_VEL_DECAY;
		tParticle->m_uchColor[0]	= 150;
		tParticle->m_uchColor[1]	= 0;
		tParticle->m_uchColor[2]	= 0;
		tParticle->m_uchStartAlpha = interpolatedRamp.m_flMaxAlpha / 2;
		tParticle->m_uchEndAlpha = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : origin - 
//			normal - 
//			scale - 
//-----------------------------------------------------------------------------
void FX_CS_BloodSpray( const Vector &origin, const Vector &normal, float flDamage )
{
	if ( UTIL_IsLowViolence() )
		return;

	static ConVar *violence_hblood = cvar->FindVar( "violence_hblood" );
	if ( violence_hblood && !violence_hblood->GetBool() )
		return;

	// Use the new particle system
	Vector dir = normal;/* * RandomVector( -0.05f, 0.05f )*/;
	Vector offset = origin + ( normal );

	QAngle vecAngles;
	VectorAngles( dir, vecAngles );

	const char *pEffectName;
	if ( flDamage > damage_impact_heavy.GetInt() )
	{
	      pEffectName = "blood_impact_heavy";
	}
	else if ( flDamage >= damage_impact_medium.GetInt()  )
	{
		pEffectName = "blood_impact_medium";
	}
	else if ( flDamage > 1 )
	{
		pEffectName = "blood_impact_light";
	}
	else
	{
		pEffectName = "blood_impact_light_headshot";
	}

	DispatchParticleEffect( pEffectName, offset, vecAngles );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bloodtype - 
//			r - 
//			g - 
//			b - 
//-----------------------------------------------------------------------------
void GetBloodColorForTeam( int iTeam, unsigned char &r, unsigned char &g, unsigned char &b )
{

}

//-----------------------------------------------------------------------------
// Purpose: Intercepts the blood spray message.
//-----------------------------------------------------------------------------
void CSBloodSprayCallback( const CEffectData &data )
{
	FX_CS_BloodSpray( data.m_vOrigin, data.m_vNormal, data.m_flMagnitude );
}

DECLARE_CLIENT_EFFECT( csblood, CSBloodSprayCallback );
