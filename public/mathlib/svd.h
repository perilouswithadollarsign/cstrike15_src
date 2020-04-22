//////////////////////////////////////////////////////////////////////////
//
// Implementaiton of iterative 3x3 SVD without branches, using approximate Givens rotations,
// applied sequentially to every off-diagonal element. The same code can compile into scalar, SSE and AVX
// by templetizing on the Float data type. 
//

#include "ssemath.h"

namespace SVD
{
	inline fltx4 CmpLt( const fltx4 &a, const fltx4 &b ) { return CmpLtSIMD( a, b ); }
	inline bool CmpLt( float a, float b ) { return a < b ? true : false; }
	
	inline bool AllLe( const fltx4 &a, const fltx4 &b ){ return IsAllGreaterThanOrEq( b, a ); }
	inline bool AllLe( float a, float b ) { return a <= b; }


	template <typename Float >
	struct FloatTraits
	{
		typedef Float Bool;
	};
	template <>
	struct FloatTraits < float >
	{
		typedef bool Bool;
	};

	template< typename Float > inline Float Replicate( float a );

	template <> inline fltx4 Replicate< fltx4 >( float a )
	{
		return _mm_set1_ps( a );
	}

	template <> inline float Replicate< float >( float a ){ return a; }

	template <typename Float>
	class SymMatrix3;


	inline fltx4 RsqrtEst( const fltx4 &a ){ return ReciprocalSqrtEstSIMD( a ); }
	template <typename Float>
	inline Float Rsqrt( const Float &a )
	{
		fltx4 it = RsqrtEst( a );
		// a single Newton iteration; can repeat multiple times
		return Replicate<Float>( .5f ) * it * ( Replicate<Float>( 3.0f ) - ( a * it * it ) );
	}
	inline float RsqrtEst( float a ) 
	{
		float res;
		StoreUnalignedFloat( &res, RsqrtEst( LoadUnalignedFloatSIMD(&a)  ) ); 
		return res;
	}
	template <>
	inline float Rsqrt( const float &a ) { return 1.0f / sqrtf( a ); }

	inline fltx4 Select( const fltx4& a /*mask=0*/, const fltx4& b/*mask=1*/, const fltx4& mask )
	{
		// (((b ^ a) & mask)^a)
		return _mm_xor_ps( a, _mm_and_ps( mask, _mm_xor_ps( b, a ) ) );
	}

	inline float Select( float a, float b, bool mask )
	{
		return mask ? b : a;
	}


	template <typename Float>
	class Matrix3
	{
	public:
		Matrix3() {}
		Float m[ 3 ][ 3 ];

		Matrix3 < Float > operator * ( const Matrix3< Float > &other )const
		{
			Matrix3 < Float > res;
			for ( int i = 0; i < 3; ++i )
				for ( int j = 0; j < 3; ++j )
					res.m[ i ][ j ] = m[ i ][ 0 ] * other.m[ 0 ][ j ] + m[ i ][ 1 ] * other.m[ 1 ][ j ] + m[ i ][ 2 ] * other.m[ 2 ][ j ];
			return res;
		}
		Matrix3 < Float > operator - ( const Matrix3< Float > &other )const
		{
			Matrix3 < Float > res;
			for ( int i = 0; i < 3; ++i )
				for ( int j = 0; j < 3; ++j )
					res.m[ i ][ j ] = m[ i ][ j ] - other.m[ i ][ j ];
			return res;
		}
		explicit Matrix3< Float >( const SymMatrix3< Float > &other );
		Float FrobeniusNorm()const
		{
			Float sum = Replicate<Float>( 0.0f );
			for ( int i = 0; i < 3; ++i )
				for ( int j = 0; j < 3; ++j )
					sum += m[ i ][ j ] * m[ i ][ j ];
			return sum;
		}

		Float AtA( int i, int j )const
		{
			return m[ 0 ][ i ] * m[ 0 ][ j ] + m[ 1 ][ i ] * m[ 1 ][ j ] + m[ 2 ][ i ] * m[ 2 ][ j ];
		}

		void SetIdentity()
		{
			m[ 0 ][ 0 ] = m[ 1 ][ 1 ] = m[ 2 ][ 2 ] = Replicate<Float>( 1.0f );
			m[ 0 ][ 1 ] = m[ 1 ][ 0 ] = m[ 0 ][ 2 ] = m[ 2 ][ 0 ] = m[ 2 ][ 1 ] = m[ 1 ][ 2 ] = Replicate<Float>( 0.0f );
		}
		void SetZero()
		{
			memset( this, 0, sizeof( *this ) );
		}
		Float ColLenSqr( int j )const
		{
			return m[ 0 ][ j ] * m[ 0 ][ j ] + m[ 1 ][ j ] * m[ 1 ][ j ] + m[ 2 ][ j ] * m[ 2 ][ j ];
		}
		Float Det()const
		{
			return -( m[ 0 ][ 2 ]*m[ 1 ][ 1 ]*m[ 2 ][ 0 ] ) + m[ 0 ][ 1 ]*m[ 1 ][ 2 ]*m[ 2 ][ 0 ] + m[ 0 ][ 2 ]*m[ 1 ][ 0 ]*m[ 2 ][ 1 ] - m[ 0 ][ 0 ]*m[ 1 ][ 2 ]*m[ 2 ][ 1 ] - m[ 0 ][ 1 ]*m[ 1 ][ 0 ]*m[ 2 ][ 2 ] + m[ 0 ][ 0 ]*m[ 1 ][ 1 ]*m[ 2 ][ 2 ];
		}
	};


	template <typename Float>
	inline Matrix3 < Float > MulT( const Matrix3< Float > &u, const Matrix3< Float > &vt )
	{
		Matrix3 < Float > res;
		for ( int i = 0; i < 3; ++i )
			for ( int j = 0; j < 3; ++j )
				res.m[ i ][ j ] = u.m[ i ][ 0 ] * vt.m[ j ][ 0 ] + u.m[ i ][ 1 ] * vt.m[ j ][ 1 ] + u.m[ i ][ 2 ] * vt.m[ j ][ 2 ];
		return res;
	}


	template <typename Float>
	Float OrthogonalityError( const Matrix3<Float> &m )
	{
		Float result = Replicate<Float>( 0.0f );
		for ( int i = 0; i < 2; ++i )
		{
			for ( int j = i + 1; j < 3; ++j )
			{
				Float dot = m.m[ 0 ][ i ] * m.m[ 0 ][ j ] + m.m[ 1 ][ i ] * m.m[ 1 ][ j ] + m.m[ 2 ][ i ] * m.m[ 2 ][ j ];
				result += dot * dot;
			}
		}
		return result;
	}

	template <typename Float >
	class DiagMatrix3
	{
	public:
		Float m[ 3 ];
	};

	template <typename Float>
	class SymMatrix3
	{
	public:
		enum Index_t
		{
			a00, a10, a11, a20, a21, a22, Count, a01 = a10, a02 = a20, a12 = a21
		};

		Float m[ 6 ];
		Float &m00() { return m[ a00 ]; }
		Float &m01() { return m[ a01 ]; }
		Float &m02() { return m[ a02 ]; }
		Float &m11() { return m[ a11 ]; }
		Float &m12() { return m[ a12 ]; }
		Float &m22() { return m[ a22 ]; }

		Float OffDiagNorm()const { return m[ a01 ] * m[ a01 ] + m[ a21 ] * m[ a21 ] + m[ a02 ] * m[ a02 ]; }
		Float DiagNorm()const { return m[ a00 ] * m[ a00 ] + m[ a11 ] * m[ a11 ] + m[ a22 ] * m[ a22 ]; }
	};


	template < typename Float >
	Matrix3< Float >::Matrix3( const SymMatrix3< Float > &other )
	{
		m[ 0 ][ 0 ] = other.m[ other.a00 ];
		m[ 0 ][ 1 ] = m[ 1 ][ 0 ] = other.m[ other.a01 ];
		m[ 1 ][ 1 ] = other.m[ other.a11 ];
		m[ 2 ][ 0 ] = m[ 0 ][ 2 ] = other.m[ other.a01 ];
		m[ 2 ][ 1 ] = m[ 1 ][ 2 ] = other.m[ other.a01 ];
		m[ 2 ][ 2 ] = other.m[ other.a22 ];
	}


	template <typename Float >
	class SinCos
	{
	public:
		Float s, c;
		SinCos() {}
		SinCos( const Float &_c, const Float &_s ) : c( _c ), s( _s ) {}

		SinCos< Float> DoubleAngle()const
		{
			SinCos< Float> res;
			res.s = Replicate<Float>( 2.0f ) * s * c;
			res.c = c * c - s * s;
			return res;
		}
	};

	template <typename Float>
	class Quaternion
	{
	public:
		Float x, y, z, w;

		void SetIdentity()
		{
			x = y = z = Replicate<float>( 0.0f );
			w = Replicate<float>( 1.0f );
		}

		Quaternion<Float> operator * ( const Float &f )const
		{
			Quaternion< Float > res;
			res.x = x * f;
			res.y = y * f;
			res.z = z * f;
			res.w = w * f;
			return res;
		}

		Float LengthSqr() const
		{
			return x * x + y * y + z * z + w * w;
		}
	};

	template <typename Float>
	Matrix3<Float> QuaternionMatrix( const Quaternion<Float> &q )
	{
		Matrix3<Float> matrix;
		const Float one = Replicate<Float>( 1.0f ), two = Replicate<Float>( 2.0f );
		matrix.m[ 0 ][ 0 ] = one - two * q.y * q.y - two * q.z * q.z;
		matrix.m[ 1 ][ 0 ] = two * q.x * q.y + two * q.w * q.z;
		matrix.m[ 2 ][ 0 ] = two * q.x * q.z - two * q.w * q.y;
		matrix.m[ 0 ][ 1 ] = two * q.x * q.y - two * q.w * q.z;
		matrix.m[ 1 ][ 1 ] = one - two * q.x * q.x - two * q.z * q.z;
		matrix.m[ 2 ][ 1 ] = two * q.y * q.z + two * q.w * q.x;
		matrix.m[ 0 ][ 2 ] = two * q.x * q.z + two * q.w * q.y;
		matrix.m[ 1 ][ 2 ] = two * q.y * q.z - two * q.w * q.x;
		matrix.m[ 2 ][ 2 ] = one - two * q.x * q.x - two * q.y * q.y;
		return matrix;
	}

	template <typename Float >
	inline SymMatrix3< Float > AtA( const Matrix3< Float > &a )
	{
		SymMatrix3< Float > res;
		res.m[ res.a00 ] = a.AtA( 0, 0 );
		res.m[ res.a10 ] = a.AtA( 1, 0 );
		res.m[ res.a11 ] = a.AtA( 1, 1 );
		res.m[ res.a20 ] = a.AtA( 2, 0 );
		res.m[ res.a21 ] = a.AtA( 2, 1 );
		res.m[ res.a22 ] = a.AtA( 2, 2 );
		return res;
	}


	template <typename Float >
	inline SymMatrix3< Float > QtAQ( const Matrix3< Float > &q, const SymMatrix3< Float > &a )
	{
		SymMatrix3< Float > res;
		res.m[ res.a00 ] = q.m[ 0 ][ 0 ] * ( a.m[ a.a00 ] * q.m[ 0 ][ 0 ] + a.m[ a.a01 ] * q.m[ 1 ][ 0 ] + a.m[ a.a02 ] * q.m[ 2 ][ 0 ] ) +
			q.m[ 1 ][ 0 ] * ( a.m[ a.a01 ] * q.m[ 0 ][ 0 ] + a.m[ a.a11 ] * q.m[ 1 ][ 0 ] + a.m[ a.a12 ] * q.m[ 2 ][ 0 ] ) +
			q.m[ 2 ][ 0 ] * ( a.m[ a.a02 ] * q.m[ 0 ][ 0 ] + a.m[ a.a12 ] * q.m[ 1 ][ 0 ] + a.m[ a.a22 ] * q.m[ 2 ][ 0 ] );
		res.m[ res.a01 ] =
			q.m[ 0 ][ 1 ] * ( a.m[ a.a00 ] * q.m[ 0 ][ 0 ] + a.m[ a.a01 ] * q.m[ 1 ][ 0 ] + a.m[ a.a02 ] * q.m[ 2 ][ 0 ] ) +
			q.m[ 1 ][ 1 ] * ( a.m[ a.a01 ] * q.m[ 0 ][ 0 ] + a.m[ a.a11 ] * q.m[ 1 ][ 0 ] + a.m[ a.a12 ] * q.m[ 2 ][ 0 ] ) + ( a.m[ a.a02 ] * q.m[ 0 ][ 0 ] + a.m[ a.a12 ] * q.m[ 1 ][ 0 ] +
			a.m[ a.a22 ] * q.m[ 2 ][ 0 ] ) * q.m[ 2 ][ 1 ];
		res.m[ res.a02 ] =
			q.m[ 0 ][ 2 ] * ( a.m[ a.a00 ] * q.m[ 0 ][ 0 ] + a.m[ a.a01 ] * q.m[ 1 ][ 0 ] + a.m[ a.a02 ] * q.m[ 2 ][ 0 ] ) +
			q.m[ 1 ][ 2 ] * ( a.m[ a.a01 ] * q.m[ 0 ][ 0 ] + a.m[ a.a11 ] * q.m[ 1 ][ 0 ] + a.m[ a.a12 ] * q.m[ 2 ][ 0 ] ) + ( a.m[ a.a02 ] * q.m[ 0 ][ 0 ] + a.m[ a.a12 ] * q.m[ 1 ][ 0 ] +
			a.m[ a.a22 ] * q.m[ 2 ][ 0 ] ) * q.m[ 2 ][ 2 ];
		res.m[ res.a11 ] =
			q.m[ 0 ][ 1 ] * ( a.m[ a.a00 ] * q.m[ 0 ][ 1 ] + a.m[ a.a01 ] * q.m[ 1 ][ 1 ] + a.m[ a.a02 ] * q.m[ 2 ][ 1 ] ) +
			q.m[ 1 ][ 1 ] * ( a.m[ a.a01 ] * q.m[ 0 ][ 1 ] + a.m[ a.a11 ] * q.m[ 1 ][ 1 ] + a.m[ a.a12 ] * q.m[ 2 ][ 1 ] ) +
			q.m[ 2 ][ 1 ] * ( a.m[ a.a02 ] * q.m[ 0 ][ 1 ] + a.m[ a.a12 ] * q.m[ 1 ][ 1 ] + a.m[ a.a22 ] * q.m[ 2 ][ 1 ] );
		res.m[ res.a12 ] =
			q.m[ 0 ][ 2 ] * ( a.m[ a.a00 ] * q.m[ 0 ][ 1 ] + a.m[ a.a01 ] * q.m[ 1 ][ 1 ] + a.m[ a.a02 ] * q.m[ 2 ][ 1 ] ) +
			q.m[ 1 ][ 2 ] * ( a.m[ a.a01 ] * q.m[ 0 ][ 1 ] + a.m[ a.a11 ] * q.m[ 1 ][ 1 ] + a.m[ a.a12 ] * q.m[ 2 ][ 1 ] ) + ( a.m[ a.a02 ] * q.m[ 0 ][ 1 ] + a.m[ a.a12 ] * q.m[ 1 ][ 1 ] +
			a.m[ a.a22 ] * q.m[ 2 ][ 1 ] ) * q.m[ 2 ][ 2 ];
		res.m[ res.a22 ] =
			q.m[ 0 ][ 2 ] * ( a.m[ a.a00 ] * q.m[ 0 ][ 2 ] + a.m[ a.a01 ] * q.m[ 1 ][ 2 ] + a.m[ a.a02 ] * q.m[ 2 ][ 2 ] ) +
			q.m[ 1 ][ 2 ] * ( a.m[ a.a01 ] * q.m[ 0 ][ 2 ] + a.m[ a.a11 ] * q.m[ 1 ][ 2 ] + a.m[ a.a12 ] * q.m[ 2 ][ 2 ] ) +
			q.m[ 2 ][ 2 ] * ( a.m[ a.a02 ] * q.m[ 0 ][ 2 ] + a.m[ a.a12 ] * q.m[ 1 ][ 2 ] + a.m[ a.a22 ] * q.m[ 2 ][ 2 ] );
		return res;
	}

	template <typename Float >
	inline SymMatrix3< Float > QAQt( const Matrix3< Float > &q, const SymMatrix3< Float > &a )
	{
		SymMatrix3< Float > res;
		res.m[ res.a00 ] = q.m[ 0 ][ 0 ] * ( a.m[ a.a00 ] * q.m[ 0 ][ 0 ] + a.m[ a.a01 ] * q.m[ 0 ][ 1 ] + a.m[ a.a02 ] * q.m[ 0 ][ 2 ] ) +
			q.m[ 0 ][ 1 ] * ( a.m[ a.a01 ] * q.m[ 0 ][ 0 ] + a.m[ a.a11 ] * q.m[ 0 ][ 1 ] + a.m[ a.a12 ] * q.m[ 0 ][ 2 ] ) +
			q.m[ 0 ][ 2 ] * ( a.m[ a.a02 ] * q.m[ 0 ][ 0 ] + a.m[ a.a12 ] * q.m[ 0 ][ 1 ] + a.m[ a.a22 ] * q.m[ 0 ][ 2 ] );
		res.m[ res.a01 ] =
			q.m[ 1 ][ 0 ] * ( a.m[ a.a00 ] * q.m[ 0 ][ 0 ] + a.m[ a.a01 ] * q.m[ 0 ][ 1 ] + a.m[ a.a02 ] * q.m[ 0 ][ 2 ] ) +
			q.m[ 1 ][ 1 ] * ( a.m[ a.a01 ] * q.m[ 0 ][ 0 ] + a.m[ a.a11 ] * q.m[ 0 ][ 1 ] + a.m[ a.a12 ] * q.m[ 0 ][ 2 ] ) + ( a.m[ a.a02 ] * q.m[ 0 ][ 0 ] + a.m[ a.a12 ] * q.m[ 0 ][ 1 ] +
			a.m[ a.a22 ] * q.m[ 0 ][ 2 ] ) * q.m[ 1 ][ 2 ];
		res.m[ res.a02 ] =
			q.m[ 2 ][ 0 ] * ( a.m[ a.a00 ] * q.m[ 0 ][ 0 ] + a.m[ a.a01 ] * q.m[ 0 ][ 1 ] + a.m[ a.a02 ] * q.m[ 0 ][ 2 ] ) +
			q.m[ 2 ][ 1 ] * ( a.m[ a.a01 ] * q.m[ 0 ][ 0 ] + a.m[ a.a11 ] * q.m[ 0 ][ 1 ] + a.m[ a.a12 ] * q.m[ 0 ][ 2 ] ) + ( a.m[ a.a02 ] * q.m[ 0 ][ 0 ] + a.m[ a.a12 ] * q.m[ 0 ][ 1 ] +
			a.m[ a.a22 ] * q.m[ 0 ][ 2 ] ) * q.m[ 2 ][ 2 ];
		res.m[ res.a11 ] =
			q.m[ 1 ][ 0 ] * ( a.m[ a.a00 ] * q.m[ 1 ][ 0 ] + a.m[ a.a01 ] * q.m[ 1 ][ 1 ] + a.m[ a.a02 ] * q.m[ 1 ][ 2 ] ) +
			q.m[ 1 ][ 1 ] * ( a.m[ a.a01 ] * q.m[ 1 ][ 0 ] + a.m[ a.a11 ] * q.m[ 1 ][ 1 ] + a.m[ a.a12 ] * q.m[ 1 ][ 2 ] ) +
			q.m[ 1 ][ 2 ] * ( a.m[ a.a02 ] * q.m[ 1 ][ 0 ] + a.m[ a.a12 ] * q.m[ 1 ][ 1 ] + a.m[ a.a22 ] * q.m[ 1 ][ 2 ] );
		res.m[ res.a12 ] =
			q.m[ 2 ][ 0 ] * ( a.m[ a.a00 ] * q.m[ 1 ][ 0 ] + a.m[ a.a01 ] * q.m[ 1 ][ 1 ] + a.m[ a.a02 ] * q.m[ 1 ][ 2 ] ) +
			q.m[ 2 ][ 1 ] * ( a.m[ a.a01 ] * q.m[ 1 ][ 0 ] + a.m[ a.a11 ] * q.m[ 1 ][ 1 ] + a.m[ a.a12 ] * q.m[ 1 ][ 2 ] ) + ( a.m[ a.a02 ] * q.m[ 1 ][ 0 ] + a.m[ a.a12 ] * q.m[ 1 ][ 1 ] +
			a.m[ a.a22 ] * q.m[ 1 ][ 2 ] ) * q.m[ 2 ][ 2 ];
		res.m[ res.a22 ] =
			q.m[ 2 ][ 0 ] * ( a.m[ a.a00 ] * q.m[ 2 ][ 0 ] + a.m[ a.a01 ] * q.m[ 2 ][ 1 ] + a.m[ a.a02 ] * q.m[ 2 ][ 2 ] ) +
			q.m[ 2 ][ 1 ] * ( a.m[ a.a01 ] * q.m[ 2 ][ 0 ] + a.m[ a.a11 ] * q.m[ 2 ][ 1 ] + a.m[ a.a12 ] * q.m[ 2 ][ 2 ] ) +
			q.m[ 2 ][ 2 ] * ( a.m[ a.a02 ] * q.m[ 2 ][ 0 ] + a.m[ a.a12 ] * q.m[ 2 ][ 1 ] + a.m[ a.a22 ] * q.m[ 2 ][ 2 ] );
		return res;
	}


	template <typename Float >
	inline SymMatrix3< Float > QAQt( const Matrix3< Float > &q, const DiagMatrix3< Float > &a )
	{
		SymMatrix3< Float > res;
		res.m[ res.a00 ] = q.m[ 0 ][ 0 ] * a.m[ 0 ] * q.m[ 0 ][ 0 ] +
			q.m[ 0 ][ 1 ] *  a.m[ 1 ] * q.m[ 0 ][ 1 ]  +
			q.m[ 0 ][ 2 ] *  a.m[ 2 ] * q.m[ 0 ][ 2 ] ;
		res.m[ res.a01 ] =
			q.m[ 1 ][ 0 ] * ( a.m[ 0 ] * q.m[ 0 ][ 0 ]) +
			q.m[ 1 ][ 1 ] * ( a.m[ 1 ] * q.m[ 0 ][ 1 ]) + ( a.m[ 2 ] * q.m[ 0 ][ 2 ] ) * q.m[ 1 ][ 2 ];
		res.m[ res.a02 ] =
			q.m[ 2 ][ 0 ] * ( a.m[ 0 ] * q.m[ 0 ][ 0 ]) +
			q.m[ 2 ][ 1 ] * ( a.m[ 1 ] * q.m[ 0 ][ 1 ] ) + ( a.m[ 2 ] * q.m[ 0 ][ 2 ] ) * q.m[ 2 ][ 2 ];
		res.m[ res.a11 ] =
			q.m[ 1 ][ 0 ] * ( a.m[ 0 ] * q.m[ 1 ][ 0 ]) +
			q.m[ 1 ][ 1 ] * ( a.m[ 1 ] * q.m[ 1 ][ 1 ]) +
			q.m[ 1 ][ 2 ] * ( a.m[ 2 ] * q.m[ 1 ][ 2 ] );
		res.m[ res.a12 ] =
			q.m[ 2 ][ 0 ] * ( a.m[ 0 ] * q.m[ 1 ][ 0 ]) +
			q.m[ 2 ][ 1 ] * (  a.m[ 1 ] * q.m[ 1 ][ 1 ] ) + ( a.m[ 2 ] * q.m[ 1 ][ 2 ] ) * q.m[ 2 ][ 2 ];
		res.m[ res.a22 ] =
			q.m[ 2 ][ 0 ] * ( a.m[ 0 ] * q.m[ 2 ][ 0 ] ) +
			q.m[ 2 ][ 1 ] * ( a.m[ 1 ] * q.m[ 2 ][ 1 ] ) +
			q.m[ 2 ][ 2 ] * ( a.m[ 2 ] * q.m[ 2 ][ 2 ] );
		return res;
	}

	template <typename Float >
	void PerformGivensRotation2x2( const SinCos<Float> &res, Float &a11, Float &a12, Float &a22 )
	{
		const Float two = Replicate<Float>( 2.0f );
		Float cc = res.c * res.c, ss = res.s * res.s, cs = res.c * res.s;
		Float b11 = cc * a11 + two * cs * a12 + ss * a22;
		Float b12 = cs * ( a22 - a11 ) + ( cc - ss ) * a12;
		Float b22 = ss * a11 - two * cs * a12 + cc * a22;

		a11 = b11;
		a12 = b12;
		a22 = b22;
	}


	template <typename Float>
	void UnperformGivensRotation3x3( const SinCos<Float> &r, Float &a00, Float &a01, Float &a11, Float &a02, Float &a12 )
	{
		const Float two = Replicate<Float>( 2.0f );

		Float b00 = a00 * r.c * r.c - r.s * ( two * a01 * r.c - a11 * r.s );
		Float b01 = r.c*( a01 * r.c + a00 * r.s ) - r.s * ( a11 * r.c + a01 * r.s );
		Float b11 = a11 * r.c * r.c + r.s * ( two * a01 * r.c + a00 * r.s );
		Float b02 = a02 *r.c - a12 * r.s;
		Float b12 = a12 *r.c + a02 * r.s;

		a00 = b00;
		a01 = b01;
		a11 = b11;
		a02 = b02;
		a12 = b12;
	}

	template <typename Float>
	void PerformGivensRotation3x3( const SinCos<Float> &r, Float &a00, Float &a01, Float &a11, Float &a02, Float &a12 )
	{
		const Float two = Replicate<Float>( 2.0f );

		Float b00 = a00 * r.c * r.c + r.s * ( two * a01 * r.c + a11 * r.s );
		Float b01 = r.c*( a01 *r.c - a00 * r.s ) + r.s * ( a11 *r.c - a01 * r.s );
		Float b11 = a11 * r.c *r.c - r.s * ( two * a01 * r.c - a00 * r.s );
		Float b02 = a02 *r.c + a12 * r.s;
		Float b12 = a12 *r.c - a02 * r.s;

		a00 = b00;
		a01 = b01;
		a11 = b11;
		a02 = b02;
		a12 = b12;
	}

	inline SinCos< float > ComputeGivensRotation( float a11, float a12, float a22 )
	{
		float theta = fabsf( a11 - a22 ) > 1e-6f ? 0.5f * atanf( 2 * a12 / ( a11 - a22 ) ) : 3.14159265358979323846f / 4;
		SinCos< float >res( cosf( theta ), sinf( theta ) );
#ifdef _DEBUG
		PerformGivensRotation2x2( res, a11, a12, a22 );
		Assert( fabsf( a12 ) < 0.001f * ( 1 + fabsf( a11 ) + fabsf( a22 ) ) );
#endif
		return res;
	}

	template <typename Float >
	inline SinCos< Float> ApproximateGivensRotation( const Float & a11, const Float & a12, const Float & a22 )
	{
		const Float two = Replicate<Float>( 2.0f );
		Float ch = two * ( a11 - a22 );
		Float sh = a12;
		typename FloatTraits<Float>::Bool b = CmpLt( Replicate<Float>( 5.82842712474619f ) * sh*sh, ch*ch );
		Float r2 = ch*ch + sh *sh;
		typename FloatTraits<Float>::Bool bZero = CmpLt( r2, Replicate<Float>( 1e-12f ) );
		Float omega = RsqrtEst( r2 );
		SinCos<Float>res;
		res.s = Select( Replicate<Float>( 0.3826834323650897717284599840304f ), omega * sh, b );
		res.c = Select( Replicate<Float>( 0.92387953251128675612818318939679f ), omega * ch, b );

		res.s = Select( res.s, Replicate<Float>( 0.0f ), bZero ); // todo: replace with And
		res.c = Select( res.c, Replicate<Float>( 1.0f ), bZero );

		return res;
	}



	template <typename Float >
	void PerformGivensRotationQuaternion( const SinCos<Float> &res, Float &x, Float &y, Float &z, Float &w )
	{
		//const Float two = Replicate<Float>( 2.0f );
		Float xNew = res.c * x + res.s * w, yNew = res.c * y + res.s * z, zNew = res.c * z - res.s * y, wNew = res.c * w - res.s * x;
		x = xNew;
		y = yNew;
		z = zNew;
		w = wNew;
	}

	template <typename Float >
	class SvdIterator
	{
	public:
		SvdIterator(){}
		
		Quaternion < Float > q;
		SymMatrix3<Float> ata;

		void Init( const Matrix3<Float> a )
		{
			q.SetIdentity();
			ata = AtA( a );
		}


		void Iterate( int nIterations, float flEpsilon = 0.0f )
		{
			SinCos< Float> r;

			//SymMatrix3<Float> inv0 = QAQt( QuaternionMatrix( q ), ata ), origAta = AtA( a );

			for ( int i = 0; i < nIterations; ++i )
			{
				r = ApproximateGivensRotation( ata.m[ ata.a00 ], ata.m[ ata.a10 ], ata.m[ ata.a11 ] );
				Float sumErrors = r.s * r.s;
				PerformGivensRotation3x3( r.DoubleAngle(), ata.m[ ata.a00 ], ata.m[ ata.a10 ], ata.m[ ata.a11 ], ata.m[ ata.a20 ], ata.m[ ata.a21 ] );
				PerformGivensRotationQuaternion( r, q.z, q.x, q.y, q.w );
				//SymMatrix3<Float> inv1 = QAQt( QuaternionMatrix( q ), ata );

				r = ApproximateGivensRotation( ata.m[ ata.a11 ], ata.m[ ata.a21 ], ata.m[ ata.a22 ] );
				sumErrors += r.s * r.s;
				PerformGivensRotation3x3( r.DoubleAngle(), ata.m[ ata.a11 ], ata.m[ ata.a21 ], ata.m[ ata.a22 ], ata.m[ ata.a01 ], ata.m[ ata.a02 ] );
				PerformGivensRotationQuaternion( r, q.x, q.y, q.z, q.w );
				//SymMatrix3<Float> inv2 = QAQt( QuaternionMatrix( q ), ata );

				r = ApproximateGivensRotation( ata.m[ ata.a22 ], ata.m[ ata.a20 ], ata.m[ ata.a00 ] );
				sumErrors += r.s * r.s;
				PerformGivensRotation3x3( r.DoubleAngle(), ata.m[ ata.a22 ], ata.m[ ata.a02 ], ata.m[ ata.a00 ], ata.m[ ata.a12 ], ata.m[ ata.a10 ] );
				PerformGivensRotationQuaternion( r, q.y, q.z, q.x, q.w );
				//SymMatrix3<Float> inv3 = QAQt( QuaternionMatrix( q ), ata );
				if ( AllLe( sumErrors, Replicate<Float>( flEpsilon ) ) )
					break; // early out
			}
		}

		Matrix3< Float > ComputeV()const { return QuaternionMatrix( q * Rsqrt( q.LengthSqr() ) ); }
	};

	inline float PseudoInverse( float fl ) { return fabsf( fl ) < FLT_EPSILON ? 0 : 1.0f / fl; }

	inline SymMatrix3< float > PseudoInverse( const SymMatrix3< float > &cov )
	{
		SvdIterator< float > si;
		si.q.SetIdentity();
		si.ata = cov;
		si.Iterate( 5 );
		DiagMatrix3< float > pseudoInverseDiag;
		pseudoInverseDiag.m[ 0 ] = PseudoInverse( si.ata.m00() );
		pseudoInverseDiag.m[ 1 ] = PseudoInverse( si.ata.m11() );
		pseudoInverseDiag.m[ 2 ] = PseudoInverse( si.ata.m22() );
		return QAQt( si.ComputeV(), pseudoInverseDiag );
	}
}
