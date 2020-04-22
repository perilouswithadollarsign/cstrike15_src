//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "clientalphaproperty.h"
#include "const.h"
#include "iclientshadowmgr.h"
#include "iclientunknown.h"
#include "iclientrenderable.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Client alpha property starts here
//-----------------------------------------------------------------------------
CClientAlphaProperty::CClientAlphaProperty( )
{
	m_nRenderFX = kRenderFxNone;
	m_nRenderMode = kRenderNormal;
	m_nDesyncOffset = 0;
	m_hShadowHandle = CLIENTSHADOW_INVALID_HANDLE;
	m_nAlpha = 255;
	m_flFadeScale = 0.0f;	// By default, things don't fade out automagically
	m_nDistFadeStart = 0;
	m_nDistFadeEnd = 0;
	m_bAlphaOverride = false;
	m_bShadowAlphaOverride = false;
	m_nDistanceFadeMode = CLIENT_ALPHA_DISTANCE_FADE_USE_CENTER;
}

void CClientAlphaProperty::Init( IClientUnknown *pUnk )
{
	m_pOuter = pUnk;
}

IClientUnknown*	CClientAlphaProperty::GetIClientUnknown()
{
	return m_pOuter;
}

void CClientAlphaProperty::SetShadowHandle( ClientShadowHandle_t hShadowHandle )
{
	m_hShadowHandle = hShadowHandle;
}

void CClientAlphaProperty::SetAlphaModulation( uint8 a )
{
	m_nAlpha = a;
}

void CClientAlphaProperty::EnableAlphaModulationOverride( bool bEnable )
{
	m_bAlphaOverride = bEnable;
}

void CClientAlphaProperty::EnableShadowAlphaModulationOverride( bool bEnable )
{
	m_bShadowAlphaOverride = bEnable;
}

// Sets an FX function
void CClientAlphaProperty::SetRenderFX( RenderFx_t nRenderFx, RenderMode_t nRenderMode, float flStartTime, float flDuration )
{
	bool bStartTimeUnspecified = ( flStartTime == FLT_MAX );
	bool bRenderFxChanged = ( m_nRenderFX != nRenderFx );

	switch( nRenderFx )
	{
	case kRenderFxFadeIn:
	case kRenderFxFadeOut:
		Assert( !bStartTimeUnspecified || !bRenderFxChanged );
		if ( bStartTimeUnspecified )
		{
			flStartTime = gpGlobals->curtime;
		}
		break;

	case kRenderFxFadeSlow:
	case kRenderFxSolidSlow:
		Assert( !bStartTimeUnspecified || !bRenderFxChanged );
		if ( bStartTimeUnspecified )
		{
			flStartTime = gpGlobals->curtime;
		}
		flDuration = 4.0f;
		break;

	case kRenderFxFadeFast:
	case kRenderFxSolidFast:
		Assert( !bStartTimeUnspecified || !bRenderFxChanged );
		if ( bStartTimeUnspecified )
		{
			flStartTime = gpGlobals->curtime;
		}
		flDuration = 1.0f;
		break;
	}

	m_nRenderMode = nRenderMode;
	m_nRenderFX = nRenderFx;
	if ( bRenderFxChanged || !bStartTimeUnspecified )
	{
		m_flRenderFxStartTime = flStartTime;
		m_flRenderFxDuration = flDuration;
	}
}

void CClientAlphaProperty::SetDesyncOffset( int nOffset )
{
	m_nDesyncOffset = nOffset;
}

void CClientAlphaProperty::SetDistanceFadeMode( ClientAlphaDistanceFadeMode_t nFadeMode )
{
	// Necessary since m_nDistanceFadeMode is stored in 1 bit
	COMPILE_TIME_ASSERT( CLIENT_ALPHA_DISTANCE_FADE_MODE_COUNT <= ( 1 << CLIENT_ALPHA_DISTANCE_FADE_MODE_BIT_COUNT ) );
	m_nDistanceFadeMode = nFadeMode;
}


// Sets fade parameters
void CClientAlphaProperty::SetFade( float flGlobalFadeScale, float flDistFadeStart, float flDistFadeEnd )
{
	if( flDistFadeStart > flDistFadeEnd )
	{
		V_swap( flDistFadeStart, flDistFadeEnd );
	}

	// If a negative value is provided for the min fade distance, then base it off the max.
	if( flDistFadeStart < 0 )
	{
		flDistFadeStart = flDistFadeEnd + flDistFadeStart;
		if( flDistFadeStart < 0 )
		{
			flDistFadeStart = 0;
		}
	}

	Assert( flDistFadeStart >= 0 && flDistFadeStart <= 65535 );
	Assert( flDistFadeEnd >= 0 && flDistFadeEnd <= 65535 );

	m_nDistFadeStart = (uint16)flDistFadeStart;
	m_nDistFadeEnd = (uint16)flDistFadeEnd;
	m_flFadeScale = flGlobalFadeScale;
}


int CClientAlphaProperty::ComputeRenderEffectBlend( int nRenderEffect ) const
{
	int nBlend = 0;
	float flOffset = ((int)m_nDesyncOffset) * 363.0;// Use ent index to de-sync these fx

	switch( nRenderEffect ) 
	{
	case kRenderFxPulseSlowWide:
		nBlend = m_nAlpha + 0x40 * sin( gpGlobals->curtime * 2 + flOffset );	
		break;

	case kRenderFxPulseFastWide:
		nBlend = m_nAlpha + 0x40 * sin( gpGlobals->curtime * 8 + flOffset );
		break;

	case kRenderFxPulseFastWider:
		nBlend = ( 0xff * fabs(sin( gpGlobals->curtime * 12 + flOffset ) ) );
		break;

	case kRenderFxPulseSlow:
		nBlend = m_nAlpha + 0x10 * sin( gpGlobals->curtime * 2 + flOffset );
		break;

	case kRenderFxPulseFast:
		nBlend = m_nAlpha + 0x10 * sin( gpGlobals->curtime * 8 + flOffset );
		break;

	case kRenderFxFadeOut:
	case kRenderFxFadeFast:
	case kRenderFxFadeSlow:
		{
			float flElapsed = gpGlobals->curtime - m_flRenderFxStartTime;
			float flVal = RemapValClamped( flElapsed, 0, m_flRenderFxDuration, m_nAlpha, 0 );
			flVal = clamp( flVal, 0, 255 );
			nBlend = (int)flVal;
		}
		break;

	case kRenderFxFadeIn:
	case kRenderFxSolidFast:
	case kRenderFxSolidSlow:
		{
			float flElapsed = gpGlobals->curtime - m_flRenderFxStartTime;
			float flVal = RemapValClamped( flElapsed, 0, m_flRenderFxDuration, 0, m_nAlpha );
			flVal = clamp( flVal, 0, 255 );
			nBlend = (int)flVal;
		}
		break;

	case kRenderFxStrobeSlow:
		nBlend = 20 * sin( gpGlobals->curtime * 4 + flOffset );
		nBlend = ( nBlend < 0 ) ? 0 : m_nAlpha;
		break;

	case kRenderFxStrobeFast:
		nBlend = 20 * sin( gpGlobals->curtime * 16 + flOffset );
		nBlend = ( nBlend < 0 ) ? 0 : m_nAlpha;
		break;

	case kRenderFxStrobeFaster:
		nBlend = 20 * sin( gpGlobals->curtime * 36 + flOffset );
		nBlend = ( nBlend < 0 ) ? 0 : m_nAlpha;
		break;

	case kRenderFxFlickerSlow:
		nBlend = 20 * (sin( gpGlobals->curtime * 2 ) + sin( gpGlobals->curtime * 17 + flOffset ));
		nBlend = ( nBlend < 0 ) ? 0 : m_nAlpha;
		break;

	case kRenderFxFlickerFast:
		nBlend = 20 * (sin( gpGlobals->curtime * 16 ) + sin( gpGlobals->curtime * 23 + flOffset ));
		nBlend = ( nBlend < 0 ) ? 0 : m_nAlpha;
		break;

	case kRenderFxNone:
	default:
		nBlend = ( m_nRenderMode == kRenderNormal ) ? 255 : m_nAlpha;
		break;	
	}
	return nBlend;
}

//-----------------------------------------------------------------------------
// Computes alpha value based on render fx
//-----------------------------------------------------------------------------
uint8 CClientAlphaProperty::ComputeRenderAlpha( ) const
{
	if ( m_nRenderMode == kRenderNone || m_nRenderMode == kRenderEnvironmental )
		return 0;

	int nBlend = 0;

	if ( m_nRenderFX > kRenderNone && m_nRenderFX < kRenderFxMax )
	{
		nBlend = ComputeRenderEffectBlend( m_nRenderFX );
	}
	else
	{
		nBlend = ( m_nRenderMode == kRenderNormal ) ? 255 : m_nAlpha;
	}

	if ( m_bAlphaOverride )
	{
		nBlend = m_pOuter->GetClientRenderable()->OverrideAlphaModulation( m_nAlpha );
	}
	nBlend = clamp( nBlend, 0, 255 );

	return nBlend;
}
