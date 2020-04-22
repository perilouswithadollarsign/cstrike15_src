//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DMETIMESELECTION_H
#define DMETIMESELECTION_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"
#include "movieobjects/dmetimeselectiontimes.h"

enum RecordingState_t;

class CDmeTimeSelection : public CDmElement
{
	DEFINE_ELEMENT( CDmeTimeSelection, CDmElement );

public:
	bool				IsEnabled() const;
	void				SetEnabled( bool state );

	bool				IsRelative() const;
	void				SetRelative( DmeTime_t time, bool state );

	DmeTime_t			GetAbsFalloff( DmeTime_t time, int side ) const;
	DmeTime_t			GetAbsHold( DmeTime_t time, int side ) const;

	DmeTime_t			GetRelativeFalloff( DmeTime_t time, int side ) const;
	DmeTime_t			GetRelativeHold( DmeTime_t time, int side ) const;

	void				SetAbsFalloff( DmeTime_t time, int side, DmeTime_t absfallofftime );
	void				SetAbsHold   ( DmeTime_t time, int side, DmeTime_t absholdtime );

	int					GetFalloffInterpolatorType( int side ) const;
	void				SetFalloffInterpolatorType( int side, int interpolatorType );

	void				GetAlphaForTime( DmeTime_t t, DmeTime_t curtime, byte &alpha );
	float				GetAmountForTime( DmeTime_t t, DmeTime_t curtime );
	float				AdjustFactorForInterpolatorType( float factor, int side );

	void				CopyFrom( const CDmeTimeSelection &src );
	
	void				GetAbsTimes( DmeTime_t time, DmeTime_t pTimes[TS_TIME_COUNT] ) const;

	void				GetCurrent( DmeTime_t pTimes[TS_TIME_COUNT] ) const;
	void				SetCurrent( const TimeSelection_t &times );

	float				GetThreshold() const;
	void				SetThreshold( float threshold );

	DmeTime_t			GetResampleInterval() const;
	void				SetResampleInterval( DmeTime_t resampleInterval );

	void				SetRecordingState( RecordingState_t state );
	RecordingState_t	GetRecordingState() const;

	void				GetTimeSelectionTimes( DmeTime_t curtime, DmeTime_t t[ TS_TIME_COUNT ] ) const;
	void				SetTimeSelectionTimes( DmeTime_t curtime, DmeTime_t t[ TS_TIME_COUNT ] );

	// Does selection extend to 'infinity' in this side?
	bool				IsInfinite( int side ) const;
	void				SetInfinite( int side );

	bool				IsFullyInfinite() const;
	bool				IsEitherInfinite() const;

	void				GetInfinite( bool bInfinite[ 2 ] ) const;

	DmeTime_t			GetAbsTime( DmeTime_t time, int tsType ) const;
	DmeTime_t			GetRelativeTime( DmeTime_t time, int tsType ) const;
	void				SetAbsTime( DmeTime_t time, int tsType, DmeTime_t absTime );

	// helper to see if any of the times are really close to DME_MAXTIME/DME_MINTIME but not exactly on them
	bool				IsSuspicious( bool bCheckHoldAndFalloff = false );

private:
	CDmeTimeSelection & operator =( const CDmeTimeSelection& src ) { Assert( 0 ); }

	void				ConvertToRelative( DmeTime_t time );
	void				ConvertToAbsolute( DmeTime_t time );

	CDmaVar< bool > m_bEnabled;
	CDmaVar< bool > m_bRelative;
	// These are all offsets from the "current" head position in seconds, or they are absolute times if not using relative mode
	CDmaTime m_falloff[ 2 ];
	CDmaTime m_hold[ 2 ];
	CDmaVar< int > m_nFalloffInterpolatorType[ 2 ];
	CDmaVar< float > m_threshold;
	CDmaTime m_resampleInterval;
	CDmaVar< int > m_nRecordingState;
};

#endif // DMETIMESELECTION_H
