#include "movieobjects/dmetimeselection.h"
#include "interpolatortypes.h"
#include "datamodel/dmelementfactoryhelper.h"
// #include "dme_controls/RecordingState.h"

float ComputeInterpolationFactor( float flFactor, int nInterpolatorType );
float GetAmountForTime( DmeTime_t dmetime, const TimeSelection_t &times, const int nInterpolationTypes[ 2 ] );


IMPLEMENT_ELEMENT_FACTORY( DmeTimeSelection, CDmeTimeSelection );

void CDmeTimeSelection::OnConstruction()
{
	m_bEnabled.InitAndSet( this, "enabled", false );
	m_bRelative.InitAndSet( this, "relative", false );

	DmeTime_t one( 1.0f );

	m_falloff[ 0 ].InitAndSet( this, "falloff_left", -one );
	m_falloff[ 1 ].InitAndSet( this, "falloff_right", one );

	m_hold[ 0 ].Init( this, "hold_left" );
	m_hold[ 1 ].Init( this, "hold_right" );

	m_nFalloffInterpolatorType[ 0 ].InitAndSet( this, "interpolator_left", INTERPOLATE_LINEAR_INTERP );
	m_nFalloffInterpolatorType[ 1 ].InitAndSet( this, "interpolator_right", INTERPOLATE_LINEAR_INTERP );

	m_threshold.InitAndSet( this, "threshold", 0.0005f );

	m_resampleInterval.InitAndSet( this, "resampleinterval", DmeTime_t( 100 ) ); // 10 ms

	m_nRecordingState.InitAndSet( this, "recordingstate", 3 /*AS_PLAYBACK :  HACK THIS SHOULD MOVE TO A PUBLIC HEADER*/ );
}


void CDmeTimeSelection::OnDestruction()
{
}

float CDmeTimeSelection::AdjustFactorForInterpolatorType( float factor, int side )
{
	return ComputeInterpolationFactor( factor, GetFalloffInterpolatorType( side ) );
}


//-----------------------------------------------------------------------------
// per-type averaging methods
//-----------------------------------------------------------------------------
float CDmeTimeSelection::GetAmountForTime( DmeTime_t t, DmeTime_t curtime )
{
	Assert( IsEnabled() );

	TimeSelection_t times;
	times[ 0 ] = GetAbsFalloff( curtime, 0 );
	times[ 1 ] = GetAbsHold( curtime, 0 );
	times[ 2 ] = GetAbsHold( curtime, 1 );
	times[ 3 ] = GetAbsFalloff( curtime, 1 );

	int nInterpolatorTypes[ 2 ] = { m_nFalloffInterpolatorType[0], m_nFalloffInterpolatorType[1] };

	return ::GetAmountForTime( t, times, nInterpolatorTypes );
}

void CDmeTimeSelection::GetAlphaForTime( DmeTime_t t, DmeTime_t curtime, byte& alpha )
{
	Assert( IsEnabled() );

	byte minAlpha = 31;
	if ( alpha <= minAlpha )
		return;

	float f = GetAmountForTime( t, curtime );
	alpha = ( byte )( f * ( alpha - minAlpha ) + minAlpha );
	alpha = clamp( alpha, minAlpha, 255 );
}

int CDmeTimeSelection::GetFalloffInterpolatorType( int side ) const
{
	return m_nFalloffInterpolatorType[ side ];
}

void CDmeTimeSelection::SetFalloffInterpolatorType( int side, int interpolatorType )
{
	m_nFalloffInterpolatorType[ side ] = interpolatorType;
}

bool CDmeTimeSelection::IsEnabled() const
{
	return m_bEnabled;
}

void CDmeTimeSelection::SetEnabled( bool state )
{
	m_bEnabled = state;
}

bool CDmeTimeSelection::IsRelative() const
{
	return m_bRelative;
}

void CDmeTimeSelection::SetRelative( DmeTime_t time, bool state )
{
	Assert( !IsSuspicious( true ) );

	bool changed = m_bRelative != state;
	m_bRelative = state;
	if ( changed )
	{
		if ( state )
			ConvertToRelative( time );
		else 
			ConvertToAbsolute( time );
	}

	Assert( !IsSuspicious( true ) );
}

DmeTime_t CDmeTimeSelection::GetAbsFalloff( DmeTime_t time, int side ) const
{
	if ( IsInfinite( side ) )
	{
		return m_falloff[ side ];
	}
	return m_bRelative ? m_falloff[ side ].Get() + time : m_falloff[ side ];
}

DmeTime_t CDmeTimeSelection::GetAbsHold( DmeTime_t time, int side ) const
{
	if ( IsInfinite( side ) )
	{
		return m_hold[ side ];
	}
	return m_bRelative ? m_hold[ side ].Get() + time : m_hold[ side ];
}

DmeTime_t CDmeTimeSelection::GetRelativeFalloff( DmeTime_t time, int side ) const
{
	if ( IsInfinite( side ) )
	{
		return m_falloff[ side ];
	}
	return m_bRelative ? m_falloff[ side ] : m_falloff[ side ].Get() - time;
}

DmeTime_t CDmeTimeSelection::GetRelativeHold( DmeTime_t time, int side ) const
{
	if ( IsInfinite( side ) )
	{
		return m_hold[ side ];
	}
	return m_bRelative ? m_hold[ side ] : m_hold[ side ].Get() - time;
}

void CDmeTimeSelection::ConvertToRelative( DmeTime_t time )
{
	Assert( !IsSuspicious( true ) );

	for ( int side = 0; side < 2; ++side )
	{
		if ( !IsInfinite( side ) )
		{
			m_falloff[ side ] -= time;
			m_hold[ side ] -= time;
		}
	}

	Assert( !IsSuspicious( true ) );
}

void CDmeTimeSelection::ConvertToAbsolute( DmeTime_t time )
{
	Assert( !IsSuspicious( true ) );

	for ( int side = 0; side < 2; ++side )
	{
		if ( !IsInfinite( side ) )
		{
			m_falloff[ side ] += time;
			m_hold[ side ] += time;
		}
	}

	Assert( !IsSuspicious( true ) );
}

void CDmeTimeSelection::SetAbsFalloff( DmeTime_t time, int side, DmeTime_t absfallofftime )
{
	// If going to infinite edge, don't need to remember the time delta in relative mode, so zero it
	if ( absfallofftime == DMETIME_MAXTIME ||
		 absfallofftime == DMETIME_MINTIME )
	{
		time = DMETIME_ZERO;
	}

	m_falloff[ side ] = m_bRelative ? absfallofftime - time : absfallofftime;
	Assert( !IsSuspicious() );
}

void CDmeTimeSelection::SetAbsHold( DmeTime_t time, int side, DmeTime_t absholdtime )
{
	// If going to infinite edge, don't need to remember the time delta in relative mode, so zero it
	if ( absholdtime == DMETIME_MAXTIME ||
		absholdtime == DMETIME_MINTIME )
	{
		time = DMETIME_ZERO;
	}

	m_hold[ side ] = m_bRelative ? absholdtime - time : absholdtime;
	Assert( !IsSuspicious() );
}

void CDmeTimeSelection::CopyFrom( const CDmeTimeSelection& src )
{
	m_bEnabled = src.m_bEnabled;
	m_bRelative = src.m_bRelative;
	m_threshold = src.m_threshold;

	for ( int i = 0 ; i < 2; ++i )
	{
		m_falloff[ i ] = src.m_falloff[ i ];
		m_hold[ i ] = src.m_hold[ i ];
		m_nFalloffInterpolatorType[ i ] = src.m_nFalloffInterpolatorType[ i ];
	}

	Assert( !IsSuspicious( true ) );

	m_nRecordingState = src.m_nRecordingState;
}

void CDmeTimeSelection::GetAbsTimes( DmeTime_t time, DmeTime_t pTimes[TS_TIME_COUNT] ) const
{
	if ( m_bRelative )
	{
		pTimes[TS_LEFT_FALLOFF ] = GetRelativeFalloff( time, 0 );
		pTimes[TS_LEFT_HOLD    ] = GetRelativeHold( time, 0 );
		pTimes[TS_RIGHT_HOLD   ] = GetRelativeHold( time, 1 );
		pTimes[TS_RIGHT_FALLOFF] = GetRelativeFalloff( time, 1 );
		return;
	}
	pTimes[TS_LEFT_FALLOFF ] = m_falloff[ 0 ];
	pTimes[TS_LEFT_HOLD    ] = m_hold   [ 0 ];
	pTimes[TS_RIGHT_HOLD   ] = m_hold   [ 1 ];
	pTimes[TS_RIGHT_FALLOFF] = m_falloff[ 1 ];
}

void CDmeTimeSelection::GetCurrent( DmeTime_t pTimes[TS_TIME_COUNT] ) const
{
	pTimes[TS_LEFT_FALLOFF ] = m_falloff[ 0 ];
	pTimes[TS_LEFT_HOLD    ] = m_hold   [ 0 ];
	pTimes[TS_RIGHT_HOLD   ] = m_hold   [ 1 ];
	pTimes[TS_RIGHT_FALLOFF] = m_falloff[ 1 ];
}

void CDmeTimeSelection::SetCurrent( const TimeSelection_t &times )
{
	m_falloff[ 0 ] = times[ TS_LEFT_FALLOFF ];
	m_hold   [ 0 ] = times[ TS_LEFT_HOLD ];
	m_hold   [ 1 ] = times[ TS_RIGHT_HOLD ];
	m_falloff[ 1 ] = times[ TS_RIGHT_FALLOFF ];

	Assert( !IsSuspicious( true ) );
}

float CDmeTimeSelection::GetThreshold() const
{
	return m_threshold;
}

void CDmeTimeSelection::SetThreshold( float threshold )
{
	m_threshold = threshold;
}

DmeTime_t CDmeTimeSelection::GetResampleInterval() const
{
	return m_resampleInterval.Get();
}

void CDmeTimeSelection::SetResampleInterval( DmeTime_t resampleInterval )
{
	m_resampleInterval.Set( resampleInterval );
}

void CDmeTimeSelection::SetRecordingState( RecordingState_t state )
{
	m_nRecordingState = ( int )state;
}

RecordingState_t CDmeTimeSelection::GetRecordingState() const
{
	return ( RecordingState_t )m_nRecordingState.Get();
}

void CDmeTimeSelection::GetTimeSelectionTimes( DmeTime_t curtime, DmeTime_t t[ TS_TIME_COUNT ] ) const
{
	t[0] = GetAbsFalloff( curtime, 0 );
	t[1] = GetAbsHold   ( curtime, 0 );
	t[2] = GetAbsHold   ( curtime, 1 );
	t[3] = GetAbsFalloff( curtime, 1 );
}


void CDmeTimeSelection::SetTimeSelectionTimes( DmeTime_t curtime, DmeTime_t t[ TS_TIME_COUNT ] )
{
	SetAbsFalloff( curtime, 0, t[0] );
	SetAbsHold   ( curtime, 0, t[1] );
	SetAbsHold   ( curtime, 1, t[2] );
	SetAbsFalloff( curtime, 1, t[3] );

	Assert( !IsSuspicious( true ) );
}

bool CDmeTimeSelection::IsInfinite( int side ) const
{
	if ( side == 0 )
	{
		return m_hold[ side ] == DMETIME_MINTIME;
	}
	else if ( side == 1 )
	{
		return m_hold[ side ] == DMETIME_MAXTIME;
	}

	// Shouldn't get here
	Assert( 0 );
	return false;
}

void CDmeTimeSelection::GetInfinite( bool bInfinite[ 2 ] ) const
{
	bInfinite[ 0 ] = IsInfinite( 0 );
	bInfinite[ 1 ] = IsInfinite( 1 );
}

bool CDmeTimeSelection::IsFullyInfinite() const
{
	return ( m_hold[ 0 ] == DMETIME_MINTIME ) && ( m_hold[ 1 ] == DMETIME_MAXTIME );
}

bool CDmeTimeSelection::IsEitherInfinite() const
{
	return ( m_hold[ 0 ] == DMETIME_MINTIME ) || ( m_hold[ 1 ] == DMETIME_MAXTIME );
}

void CDmeTimeSelection::SetInfinite( int side )
{
	if ( side == 0 )
	{
		m_hold[ side ] = DMETIME_MINTIME;
		m_falloff[ side ] = DMETIME_MINTIME;
	}
	else if ( side == 1 )
	{
		m_hold[ side ] = DMETIME_MAXTIME;
		m_falloff[ side ] = DMETIME_MAXTIME;
	}
	else
	{
		Assert( 0 );
	}
}

bool CDmeTimeSelection::IsSuspicious( bool bCheckHoldAndFalloff /*= false*/ )
{
	DmeTime_t t[ TS_TIME_COUNT ];
	GetAbsTimes( DMETIME_ZERO, t );
	DmeTime_t bounds[ 2 ] =
	{
		( DMETIME_MINTIME + DmeTime_t( 1000.0f ) ),
		( DMETIME_MAXTIME - DmeTime_t( 1000.0f ) )
	};
	for ( int i = 0; i < 4 ; ++i )
	{
		if ( t[ i ] == DMETIME_MINTIME ||
			 t[ i ] == DMETIME_MAXTIME )
			continue;

		if ( t[ i ] < bounds[ 0 ] || 
			 t[ i ] > bounds[ 1 ] )
			return true;
	}

	if ( bCheckHoldAndFalloff )
	{
		// Also check for mismatched edges if infinite
		bool bEdgesInfinite[ 4 ] =
		{
			t[ TS_LEFT_FALLOFF ] == DMETIME_MINTIME,
			t[ TS_LEFT_HOLD ] == DMETIME_MINTIME,
			t[ TS_RIGHT_HOLD ] == DMETIME_MAXTIME,
			t[ TS_RIGHT_FALLOFF ] == DMETIME_MAXTIME,
		};

		if ( ( bEdgesInfinite[ 0 ] ^ bEdgesInfinite[ 1 ] ) ||
			 ( bEdgesInfinite[ 2 ] ^ bEdgesInfinite[ 3 ] ) )
		{
			return true;
		} 
	}

 	return false;
}

DmeTime_t CDmeTimeSelection::GetAbsTime( DmeTime_t time, int tsType ) const
{
	switch ( tsType )
	{
	default:
		break;
	case TS_LEFT_FALLOFF:
		return GetAbsFalloff( time, 0 );
	case TS_LEFT_HOLD:
		return GetAbsHold( time, 0 );
	case TS_RIGHT_HOLD:
		return GetAbsHold( time, 1 );
	case TS_RIGHT_FALLOFF:
		return GetAbsFalloff( time, 1 );
	}
	Assert( 0 );
	return DMETIME_ZERO;
}

DmeTime_t CDmeTimeSelection::GetRelativeTime( DmeTime_t time, int tsType ) const
{
	switch ( tsType )
	{
	default:
		break;
	case TS_LEFT_FALLOFF:
		return GetRelativeFalloff( time, 0 );
	case TS_LEFT_HOLD:
		return GetRelativeHold( time, 0 );
	case TS_RIGHT_HOLD:
		return GetRelativeHold( time, 1 );
	case TS_RIGHT_FALLOFF:
		return GetRelativeFalloff( time, 1 );
	}
	Assert( 0 );
	return DMETIME_ZERO;
}

void CDmeTimeSelection::SetAbsTime( DmeTime_t time, int tsType, DmeTime_t absTime )
{
	switch ( tsType )
	{
	default:
		Assert( 0 );
		break;
	case TS_LEFT_FALLOFF:
		SetAbsFalloff( time, 0, absTime );  
		break;
	case TS_LEFT_HOLD:
		SetAbsHold( time, 0, absTime );
		break;
	case TS_RIGHT_HOLD:
		SetAbsHold( time, 1, absTime );
		break;
	case TS_RIGHT_FALLOFF:
		SetAbsFalloff( time, 1, absTime );
		break;
	}

	Assert( !IsSuspicious() );
}
