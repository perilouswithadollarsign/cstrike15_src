//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DMETIMESELECTIONTIMES_H
#define DMETIMESELECTIONTIMES_H
#ifdef _WIN32
#pragma once
#endif

enum TimeSelectionTimes_t
{
	TS_LEFT_FALLOFF = 0,
	TS_LEFT_HOLD,
	TS_RIGHT_HOLD,
	TS_RIGHT_FALLOFF,

	TS_TIME_COUNT,
};


struct TimeSelection_t
{
	TimeSelection_t()
	{
		m_Times[ TS_LEFT_FALLOFF ]	= DMETIME_INVALID;
		m_Times[ TS_LEFT_HOLD ]		= DMETIME_INVALID;
		m_Times[ TS_RIGHT_HOLD ]	= DMETIME_INVALID;
		m_Times[ TS_RIGHT_FALLOFF]	= DMETIME_INVALID;
	}

	const DmeTime_t &operator[]( int index ) const
	{
		Assert( index < TS_TIME_COUNT );
		return m_Times[ index ];
	}

	DmeTime_t &operator[]( int index )
	{
		Assert( index < TS_TIME_COUNT );
		return m_Times[ index ];		
	}

	bool operator==( const TimeSelection_t &rhs )
	{
		for ( int i = 0; i < TS_TIME_COUNT; ++i )
		{
			if ( m_Times[ i ] != rhs.m_Times[ i ] )
				return false;
		}
		return true;
	}

	bool operator!=( const TimeSelection_t &rhs )
	{
		return !operator==( rhs );
	}

	DmeTime_t m_Times[ TS_TIME_COUNT ];
};


// NOTE: _side == 0 means left, == 1 means right
#define TS_FALLOFF( _side ) ( ( TS_TIME_COUNT - (_side) ) & 0x3 )
#define TS_HOLD( _side ) ( TS_LEFT_HOLD + (_side) )


#endif // DMETIMESELECTIONTIMES_H
