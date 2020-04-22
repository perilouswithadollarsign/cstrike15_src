//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
// A simple 3D integer vector class.
//
//===============================================================================

#ifndef INTVECTOR3D_H
#define INTVECTOR3D_H

#if defined( COMPILER_MSVC )
#pragma once
#endif

//-----------------------------------------------------------------------------
// A simple, 3-component, 32-bit integer vector.
//
// Use when SIMD versions aren't appropriate (e.g. for branch-heavy code, 
// when readability/ease-of-use trump performance).
//-----------------------------------------------------------------------------
class IntVector3D
{
public:
	int x, y, z;

	IntVector3D() { }
	IntVector3D( int nX, int nY, int nZ ) : x( nX ), y( nY ), z( nZ ) { }
	explicit IntVector3D( int nReplicate ) : x( nReplicate ), y( nReplicate ), z( nReplicate ) { }
	explicit IntVector3D( Vector v, float flEpsilon ) : x( v.x + flEpsilon ), y( v.y + flEpsilon ), z( v.z + flEpsilon ) { }

	Vector ToVector() const { return Vector( x, y, z ); }

	IntVector3D operator+( const IntVector3D &rhs ) const
	{
		return IntVector3D( x + rhs.x, y + rhs.y, z + rhs.z );
	}

	IntVector3D operator-( const IntVector3D &rhs ) const
	{
		return IntVector3D( x - rhs.x, y - rhs.y, z - rhs.z );
	}

	IntVector3D operator-() const
	{
		return IntVector3D( -x, -y, -z );
	}

	IntVector3D operator*( int n ) const
	{
		return IntVector3D( n * x, n * y, n * z );
	}

	IntVector3D operator*( const IntVector3D &rhs ) const
	{
		return IntVector3D( x * rhs.x, y * rhs.y, z * rhs.z );
	}

	IntVector3D operator/( int n ) const
	{
		return IntVector3D( x / n, y / n, z / n );
	}

	IntVector3D operator%( int n ) const
	{
		return IntVector3D( x % n, y % n, z % n );
	}

	IntVector3D& operator+=( const IntVector3D &rhs )
	{
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		return *this;
	}

	IntVector3D& operator-=( const IntVector3D &rhs )
	{
		x -= rhs.x;
		y -= rhs.y;
		z -= rhs.z;
		return *this;
	}
	
	IntVector3D& operator*=( int n )
	{
		x *= n;
		y *= n;
		z *= n;
		return *this;
	}

	IntVector3D& operator/=( int n )
	{
		x /= n;
		y /= n;
		z /= n;
		return *this;
	}

	IntVector3D& operator%=( int n )
	{
		x %= n;
		y %= n;
		z %= n;
		return *this;
	}

	bool operator==( const IntVector3D &rhs ) const
	{
		return x == rhs.x && y == rhs.y && z == rhs.z;
	}

	bool operator!=( const IntVector3D &rhs ) const
	{
		return x != rhs.x || y != rhs.y || z != rhs.z;
	}

	const int& operator[]( const int i ) const
	{
		Assert( i >= 0 && i < 3 );
		return ( ( int * )this )[i];
	}

	int& operator[]( const int i )
	{
		Assert( i >= 0 && i < 3 );
		return ( ( int * )this )[i];
	}

	int Dot( const IntVector3D &rhs ) const
	{
		return x * rhs.x + y * rhs.y + z * rhs.z;
	}

	int LengthSqr() const
	{
		return x * x + y * y + z * z;
	}

	bool StrictlyGreater( const IntVector3D &rhs ) const
	{
		return x > rhs.x && y > rhs.y && z > rhs.z;
	}

	bool StrictlyGreaterOrEqual( const IntVector3D &rhs ) const
	{
		return x >= rhs.x && y >= rhs.y && z >= rhs.z;
	}

	bool StrictlyLess( const IntVector3D &rhs ) const
	{
		return x < rhs.x && y < rhs.y && z < rhs.z;
	}

	bool StrictlyLessOrEqual( const IntVector3D &rhs ) const
	{
		return x <= rhs.x && y <= rhs.y && z <= rhs.z;
	}

	bool AnyGreater( const IntVector3D &rhs ) const
	{
		return x > rhs.x || y > rhs.y || z > rhs.z;
	}

	bool AnyGreaterOrEqual( const IntVector3D &rhs ) const
	{
		return x >= rhs.x || y >= rhs.y || z >= rhs.z;
	}

	bool AnyLess( const IntVector3D &rhs ) const
	{
		return x < rhs.x || y < rhs.y || z < rhs.z;
	}

	bool AnyLessOrEqual( const IntVector3D &rhs ) const
	{
		return x <= rhs.x || y <= rhs.y || z <= rhs.z;
	}
};

inline IntVector3D Max( const IntVector3D &lhs, const IntVector3D &rhs )
{
	return IntVector3D( MAX( lhs.x, rhs.x ), MAX( lhs.y, rhs.y ), MAX( lhs.z, rhs.z ) );
}

inline IntVector3D Min( const IntVector3D &rhs, const IntVector3D &lhs )
{
	return IntVector3D( MIN( lhs.x, rhs.x ), MIN( lhs.y, rhs.y ), MIN( lhs.z, rhs.z ) );
}

static const IntVector3D INT_VECTOR3_ORIGIN( 0, 0, 0 );

#endif // INTVECTOR3D_H