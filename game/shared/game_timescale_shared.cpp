//============ Copyright (c) Valve Corporation, All rights reserved. ============
#include "cbase.h"
#include "game_timescale_shared.h"
#include "usermessages.h"

#ifdef CLIENT_DLL
	#include "c_user_message_register.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


CGameTimescale g_GameTimescale;
CGameTimescale* GameTimescale() { return &g_GameTimescale; }

CGameTimescale::CGameTimescale( void ) : CAutoGameSystemPerFrame( "CGameTimescale" )
{
	m_flStartBlendRealtime = 0.0f;
}

CGameTimescale::~CGameTimescale()
{
}


bool CGameTimescale::Init()
{
	ResetTimescale();

	return true;
}

void CGameTimescale::Shutdown()
{
	ResetTimescale();
}

#ifdef CLIENT_DLL
	void CGameTimescale::Update( float frametime )
	{
		UpdateTimescale();
	}
#else
	void CGameTimescale::FrameUpdatePostEntityThink()
	{
		UpdateTimescale();
	}
#endif

void CGameTimescale::LevelInitPostEntity()
{
	ResetTimescale();
}

void CGameTimescale::LevelShutdownPostEntity()
{
	ResetTimescale();
}

void CGameTimescale::SetCurrentTimescale( float flTimescale )
{
	if ( m_flCurrentTimescale == flTimescale && m_flCurrentTimescale == engine->GetTimescale() )
		return;

	// No ramp in/out, just set it!
	m_flDesiredTimescale = flTimescale;
	m_flCurrentTimescale = m_flDesiredTimescale;

	m_flDurationRealTimeSeconds = 0.0f;
	m_nInterpolatorType = INTERPOLATOR_LINEAR;

	m_flStartBlendTime = 0.0f;
	m_flStartBlendRealtime = 0.0f;

#ifndef CLIENT_DLL
	engine->SetTimescale( m_flCurrentTimescale );

	// Pass the change info to the client so it can do prediction
	CReliableBroadcastRecipientFilter filter;
	CCSUsrMsg_CurrentTimescale msg;
	msg.set_cur_timescale( m_flCurrentTimescale );
	SendUserMessage( filter, CS_UM_CurrentTimescale, msg );
#endif
}

void CGameTimescale::SetDesiredTimescaleAtTime( float flDesiredTimescale, float flDurationRealTimeSeconds /*= 0.0f*/, Interpolators_e nInterpolatorType /*= INTERPOLATOR_LINEAR*/, float flStartBlendTime /*= 0.0f*/ )
{
	SetDesiredTimescale( flDesiredTimescale, flDurationRealTimeSeconds, nInterpolatorType, MAX( 0.0f, flStartBlendTime - gpGlobals->curtime ) );
}

void CGameTimescale::SetDesiredTimescale( float flDesiredTimescale, float flDurationRealTimeSeconds /*= 0.0f*/, Interpolators_e nInterpolatorType /*= INTERPOLATOR_LINEAR*/, float flDelayRealtime /*= 0.0f*/ )
{
	if ( m_flDesiredTimescale == flDesiredTimescale )
		return;

	m_flDesiredTimescale = flDesiredTimescale;
	m_flDurationRealTimeSeconds = flDurationRealTimeSeconds;
	m_nInterpolatorType = nInterpolatorType;

	m_flStartBlendTime = gpGlobals->curtime + flDelayRealtime * m_flCurrentTimescale;

	if ( gpGlobals->curtime >= m_flStartBlendTime )
	{
		m_flStartBlendRealtime = gpGlobals->realtime;
	}
	else
	{
		m_flStartBlendRealtime = 0.0f;
	}

	m_flStartTimescale = m_flCurrentTimescale;

#ifndef CLIENT_DLL
	// Pass the change info to the client so it can do prediction
	CReliableBroadcastRecipientFilter filter;
	CCSUsrMsg_DesiredTimescale msg;
	msg.set_desired_timescale( m_flDesiredTimescale );
	msg.set_duration_realtime_sec( m_flDurationRealTimeSeconds );
	msg.set_interpolator_type( m_nInterpolatorType );
	msg.set_start_blend_time( m_flStartBlendTime );
	SendUserMessage( filter, CS_UM_DesiredTimescale, msg );
#endif
}


void CGameTimescale::UpdateTimescale( void )
{
	if ( engine->IsPaused() )
		return;

	if ( m_flCurrentTimescale != m_flDesiredTimescale )
	{
		if ( gpGlobals->curtime >= m_flStartBlendTime )
		{
			if ( m_flStartBlendRealtime == 0.0f )
			{
				m_flStartBlendRealtime = gpGlobals->realtime;
			}

			float flInterp = 1.0f;

			if ( m_flDurationRealTimeSeconds > 0.0f )
			{
				flInterp = MIN( 1.0f, ( gpGlobals->realtime - m_flStartBlendRealtime ) / m_flDurationRealTimeSeconds );
			}

			switch ( m_nInterpolatorType )
			{
			case INTERPOLATOR_ACCEL:
				flInterp = Bias( flInterp, 0.25f );
				break;

			case INTERPOLATOR_DEACCEL:
				flInterp = Bias( flInterp, 0.75f );
				break;

			case INTERPOLATOR_EASE_IN_OUT:
				flInterp = Gain( flInterp, 0.75f );
				break;
			}

			m_flCurrentTimescale = m_flStartTimescale * ( 1.0f - flInterp ) + m_flDesiredTimescale * flInterp;
		}
	}

	if ( m_flCurrentTimescale != engine->GetTimescale() )
	{
		engine->SetTimescale( m_flCurrentTimescale );
	}
}

void CGameTimescale::ResetTimescale( void )
{
	m_flDesiredTimescale = 1.0f;
	m_flCurrentTimescale = 1.0f;

	m_flDurationRealTimeSeconds = 0.0f;
	m_nInterpolatorType = INTERPOLATOR_LINEAR;

	m_flStartBlendTime = 0.0f;
	m_flStartBlendRealtime = 0.0f;

	engine->SetTimescale( 1.0f );
}


#ifdef CLIENT_DLL

bool __MsgFunc_CurrentTimescale( const CCSUsrMsg_CurrentTimescale &msg )
{
	GameTimescale()->SetCurrentTimescale( msg.cur_timescale() );

	return true;
}
USER_MESSAGE_REGISTER( CurrentTimescale );

bool __MsgFunc_DesiredTimescale( const CCSUsrMsg_DesiredTimescale &msg )
{
	float flDesiredTimescale = msg.desired_timescale();
	float flDurationRealTimeSeconds = msg.duration_realtime_sec();
	CGameTimescale::Interpolators_e nInterpolatorType = static_cast< CGameTimescale::Interpolators_e >( msg.interpolator_type() );
	float flStartBlendTime = msg.start_blend_time();

	GameTimescale()->SetDesiredTimescaleAtTime( flDesiredTimescale, flDurationRealTimeSeconds, nInterpolatorType, flStartBlendTime );

	return true;
}
USER_MESSAGE_REGISTER( DesiredTimescale );

#endif //#ifdef CLIENT_DLL