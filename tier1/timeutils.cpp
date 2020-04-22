
#include "tier1/timeutils.h"

#include "tier0/dbg.h"
#include "tier1/utlbuffer.h"
#include "tier1/utlbufferutil.h"
#include "mathlib/mathlib.h"

#include <ctype.h>

#include <math.h>

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


////////////////////////////////////////////////////////////////////////////////////////
//
// DmeFramerate_t
//
// exact (rational) representation of common framerates - any integral or ntsc framerate
//
////////////////////////////////////////////////////////////////////////////////////////

DmeFramerate_t::DmeFramerate_t( float fps )
{
	SetFramerate( fps );
}

DmeFramerate_t::DmeFramerate_t( int fps /*= 0*/ ) :
	m_num( fps ), m_den( 10000 )
{
}

DmeFramerate_t::DmeFramerate_t( int nNumerator, int nDenominator ) : 
	m_num( nNumerator ), m_den( nDenominator * 10000 )
{
}

void DmeFramerate_t::SetFramerate( float flFrameRate )
{
	if ( IsIntegralValue( flFrameRate ) )
	{
		SetFramerate( RoundFloatToInt( flFrameRate ) );
	}
	else if ( IsIntegralValue( flFrameRate * 1001.0f / 1000.0f, 0.01f ) ) // 1001 is the ntsc divisor (30*1000/1001 = 29.97, etc)
	{
		SetFramerateNTSC( RoundFloatToInt( flFrameRate * 1001.0f / 1000.0f ) );
	}
	else
	{
		Assert( 0 );
		SetFramerate( RoundFloatToInt( flFrameRate ) );
	}
}

void DmeFramerate_t::SetFramerate( int fps )
{
	m_num = fps;
	m_den = 10000;
}

// other (uncommon) options besides 30(29.97 - ntsc video) are 24 (23.976 - ntsc film) and 60 (59.94 - ntsc progressive)
void DmeFramerate_t::SetFramerateNTSC( int multiplier /*= 30*/ )
{
	// ntsc = 30 fps * 1000 / 1001 
	//      = ( 30 / 10000 fptms ) * 1000 / 1001
	//      = 30 / 10010
	m_num = multiplier;
	m_den = 10010;
}

float DmeFramerate_t::GetFramesPerSecond() const
{
	return 10000.0f * m_num / float( m_den );
}

DmeTime_t DmeFramerate_t::GetTimePerFrame() const
{
	return DmeTime_t( m_den / m_num );
}


////////////////////////////////////////////////////////////////////////////////////////
//
// DmeTime_t
//
// representing time as integral tenths of a millisecond (tms)
//
////////////////////////////////////////////////////////////////////////////////////////

DmeTime_t::DmeTime_t( int frame, DmeFramerate_t framerate )
{
	int64 num = int64( framerate.m_num );
	int64 prod = frame * int64( framerate.m_den );
	// add signed offset to force integer truncation (towards 0) to give us truncation towards -inf
	if ( frame < 0 )
	{
		prod -= num - 1;
	}
	m_tms = int( prod / num ); // round tms towards 0
}


// float operators - comment these out to find potentially incorrect uses of DmeTime_t

DmeTime_t DmeTime_t::operator*=( float f )
{
	m_tms = int( floor( m_tms * f + 0.5f ) );
	return *this;
}

DmeTime_t DmeTime_t::operator/=( float f )
{
	m_tms = int( floor( m_tms / f + 0.5f ) );
	return *this;
}


// helper methods

void DmeTime_t::Clamp( DmeTime_t lo, DmeTime_t hi )
{
	m_tms = clamp( m_tms, lo.m_tms, hi.m_tms );
}

bool DmeTime_t::IsInRange( DmeTime_t lo, DmeTime_t hi ) const
{
	return m_tms >= lo.m_tms && m_tms < hi.m_tms;
}


// helper functions

float GetFractionOfTimeBetween( DmeTime_t t, DmeTime_t start, DmeTime_t end, bool bClamp /*= false*/ )
{
	return GetFractionOfTime( t - start, end - start, bClamp );
}

float GetFractionOfTime( DmeTime_t t, DmeTime_t duration, bool bClamp /*= false*/  )
{
	if ( duration == DMETIME_ZERO )
		return 0.0f;

	if ( bClamp )
	{
		t.Clamp( DMETIME_ZERO, duration );
	}
	return t.m_tms / float( duration.m_tms );
}

int FrameForTime( DmeTime_t t, DmeFramerate_t framerate )
{
	return t.CurrentFrame( framerate );
}


// framerate-dependent conversions to/from frames

int DmeTime_t::CurrentFrame( DmeFramerate_t framerate, RoundStyle_t roundStyle /*=ROUND_DOWN*/ ) const
{
	int64 den = int64( framerate.m_den );
	int64 num = int64( framerate.m_num );
	int64 prod = int64( m_tms ) * num;

	// times within this range are considered on a frame: (frame*den/num - 1, frame*den/num]
	// this follows from the truncation towards -inf behavior of the frame,framerate constructor above
	// the following logic is there to ensure the above rule,
	// while working around the truncation towards 0 behavior of integer divide

	if ( m_tms < 0 )
	{
		if ( roundStyle == ROUND_NEAREST )
			return int( ( prod - den/2 + num ) / den );
		if ( roundStyle == ROUND_DOWN )
			return int( ( prod - den + num ) / den );
	}
	else
	{
		if ( roundStyle == ROUND_NEAREST )
			return int( ( prod + den/2 ) / den ); // this is intentionally not symmetric with the negative case, s.t. nearest always rounds up at half-frames (rather than always towards 0)
		if ( roundStyle == ROUND_UP )
			return int( ( prod + den - num ) / den );
		if ( roundStyle == ROUND_DOWN )
			return int( ( prod + num ) / den );
	}

	return int( prod / den );
}

DmeTime_t DmeTime_t::TimeAtCurrentFrame( DmeFramerate_t framerate, RoundStyle_t roundStyle /*=ROUND_DOWN*/ ) const
{
	int frame = CurrentFrame( framerate, roundStyle );
	return DmeTime_t( frame, framerate );
}
DmeTime_t DmeTime_t::TimeAtNextFrame( DmeFramerate_t framerate ) const
{
	// since we always round towards -inf, go to next frame whether we're on a frame or not
	int frame = CurrentFrame( framerate, ROUND_DOWN );
	return DmeTime_t( frame + 1, framerate );
}
DmeTime_t DmeTime_t::TimeAtPrevFrame( DmeFramerate_t framerate ) const
{
	int frame = CurrentFrame( framerate, ROUND_UP );
	return DmeTime_t( frame - 1, framerate ); // we're exactly on a frame
}


int DmeTime_t::RoundSecondsToTMS( float sec )
{
	return (int)floor( 10000.0f * sec + 0.5f ); // round at half-tms boundary
}

int DmeTime_t::RoundSecondsToTMS( double sec )
{
	return (int)floor( 10000.0 * sec + 0.5 ); // round at half-tms boundary
}


bool Serialize( CUtlBuffer &buf, const DmeTime_t &src )
{
	int tms = src.GetTenthsOfMS();

	if ( buf.IsText() )
	{
#if 1
		double tms = src.GetTenthsOfMS();
		buf.Printf( "%.04f", tms / 10000 );
#else
		int tms = src.GetTenthsOfMS();
		uint postms = tms; // can't just negate tms, since -INT_MIN == INT_MIN
		if ( tms < 0 )
		{
			buf.PutChar( '-' );
			postms = -tms;
		}
		int seconds   = postms / 10000;
		int remainder = postms % 10000;
		buf.Printf( "%d.%04d", seconds, remainder );
#endif
	}
	else
	{
		buf.PutInt( tms );
	}
	return buf.IsValid();
}

bool Unserialize( CUtlBuffer &buf, DmeTime_t &dest )
{
	if ( buf.IsText() )
	{
		buf.EatWhiteSpace();
#if 1
		double tms = buf.GetDouble() * 10000;
		if ( !buf.IsValid() )
			return false;

		if ( tms < INT_MIN || tms > INT_MAX )
			return false;

		dest.SetTenthsOfMS( ( int )floor( tms + 0.5 ) );
#else
		char str[ 16 ];
		buf.GetString( str, sizeof( str ) );
		if ( !buf.IsValid() )
			return false;

		char *p = str;

		bool bNegative = *p == '-';
		if ( bNegative )
		{
			++p;
		}

		bool bSeenDigit = false;
		bool bOverflow = false;
		int seconds = 0;
		while ( isdigit( *p ) )
		{
			seconds = seconds * 10 + *p++ - '0';
			bSeenDigit = true;
			bOverflow = seconds > INT_MAX / 10000; // once this goes invalid, it stays that way, so no extra check is needed
		}

		int remainder = 0;
		if ( *p == '.' )
		{
			++p;

			int multiplier = 1000;
			while ( isdigit( *p ) )
			{
				remainder += multiplier * ( *p++ - '0' );
				multiplier /= 10;
				bSeenDigit = true;
			}
		}

		uint tms = seconds * 10000 + remainder;
		if ( bOverflow || !bSeenDigit || ( tms > ( bNegative ? ( uint )-INT_MIN : INT_MAX ) ) )
			return false;

		dest.SetTenthsOfMS( bNegative ? -tms : tms );
#endif
		return true;
	}

	int tms = buf.GetInt();
	if ( !buf.IsValid() )
		return false;

	dest.SetTenthsOfMS( tms );
	return true;
}


////////////////////////////////////////////////////////////////////////////////
// DmeTime_t serialization/unserialization tests
////////////////////////////////////////////////////////////////////////////////

#if 0 // not as necessary now that CUtlBuffer::GetDouble() works

class CTestTimeSerialization
{
public:

	CTestTimeSerialization()
	{
		TestSerialization( DMETIME_INVALID );
		TestSerialization( DMETIME_MINTIME );
		TestSerialization( DMETIME_MAXTIME );
		TestSerialization( DMETIME_MINDELTA );
		TestSerialization( DMETIME_ZERO );
		TestSerialization( "214748.3647" );
		TestSerialization( "214748.3648", NULL, false );
		TestSerialization( "500000.0000", NULL, false );
		TestSerialization( "-214748.3648" );
		TestSerialization( "-214748.3649", NULL, false );
		TestSerialization( "-500000.0000", NULL, false );
		TestSerialization( "1.2", "1.2000" );
		TestSerialization( "1", "1.0000" );
		TestSerialization( "1.", "1.0000" );
		TestSerialization( ".2", "0.2000" );
		TestSerialization( "-1.2", "-1.2000" );
		TestSerialization( "-1.", "-1.0000" );
		TestSerialization( "-1", "-1.0000" );
		TestSerialization( "-.2", "-0.2000" );
		TestSerialization( "1.23456", "1.2345" );
		TestSerialization( "-1.23456", "-1.2345" );
	}

	void TestSerialization( DmeTime_t time )
	{
		CUtlBuffer buf( 0, 20, CUtlBuffer::TEXT_BUFFER );
		Serialize( buf, time );

		DmeTime_t test;
		if ( !Unserialize( buf, test ) )
		{
			Msg( "TestUnserialize: %d failed\n", time.GetTenthsOfMS() );
			return;
		}

		if ( time != test )
		{
			Msg( "TestUnserialize: %d != %d\n", test.GetTenthsOfMS(), time.GetTenthsOfMS() );
		}
	}

	void TestSerialization( const char *pStr, const char *pExpectedStr = NULL, bool bExpectedSuccess = true )
	{
		CUtlBuffer buf( pStr, V_strlen( pStr ) + 1, CUtlBuffer::TEXT_BUFFER | CUtlBuffer::READ_ONLY );
		DmeTime_t time;
		if ( !Unserialize( buf, time ) )
		{
			if ( bExpectedSuccess )
			{
				Msg( "TestUnserialize: %s failed\n", pStr );
			}
			return;
		}

		CUtlBuffer testbuf( 0, 20, CUtlBuffer::TEXT_BUFFER );
		Serialize( testbuf, time );

		char pTestStr[ 20 ];
		testbuf.GetString( pTestStr, sizeof( pTestStr ) );

		if ( !pExpectedStr )
		{
			pExpectedStr = pStr;
		}

		if ( V_strcmp( pTestStr, pExpectedStr ) )
		{
			Msg( "TestUnserialize: %s != %s\n", pTestStr, pExpectedStr );
		}
	}
};

CTestTimeSerialization g_testTimeSerialization;

#endif