//--------------------------------------------------------------------------------------------------
// qhMath.inl
//
// Copyright(C) 2011 by D. Gregorius. All rights reserved.
//--------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------------------
// qhMath	
//--------------------------------------------------------------------------------------------------
inline qhReal qhSin( qhReal Rad )
	{
	return sin( Rad );
	}


//--------------------------------------------------------------------------------------------------
inline qhReal qhCos( qhReal Rad )
	{
	return cos( Rad );
	}


//--------------------------------------------------------------------------------------------------
inline qhReal qhTan( qhReal Rad )
	{
	return tan( Rad );
	}


//--------------------------------------------------------------------------------------------------
inline qhReal qhArcSin( qhReal X )
	{
	return asin( X );
	}


//--------------------------------------------------------------------------------------------------
inline qhReal qhArcCos( qhReal X )
	{
	return acos( X );
	}


//--------------------------------------------------------------------------------------------------
inline qhReal qhArcTan( qhReal X )
	{
	return atan( X );
	}

//--------------------------------------------------------------------------------------------------
inline qhReal qhArcTan2( qhReal Y, qhReal X )
	{
	return atan2( Y, X );
	}


//--------------------------------------------------------------------------------------------------
inline qhReal qhAbs( qhReal X )
	{
	return X >= 0 ? X : -X;
	}


//--------------------------------------------------------------------------------------------------
inline qhReal qhSqrt( qhReal X )
	{
	return sqrt( X );
	}


//--------------------------------------------------------------------------------------------------
template< typename T > inline T qhMin( T X, T Y )
	{
	return X < Y ? X : Y;
	}


//--------------------------------------------------------------------------------------------------
template< typename T > inline T qhMax( T X, T Y )
	{
	return X > Y ? X : Y;
	}


//--------------------------------------------------------------------------------------------------
template< typename T > inline T qhClamp( T X, T Min, T Max )
	{
	return qhMax( Min, qhMin( X, Max ) );
	}


//--------------------------------------------------------------------------------------------------
// qhVector	
//--------------------------------------------------------------------------------------------------
inline qhVector3::qhVector3( void )
	{

	}


//--------------------------------------------------------------------------------------------------
inline qhVector3::qhVector3( qhReal _X, qhReal _Y, qhReal _Z )
	: X( _X )  
	, Y( _Y )  
	, Z( _Z )  
	{

	}


//--------------------------------------------------------------------------------------------------
inline qhVector3::qhVector3( const qhReal* _V )
	: X( _V[ 0 ] )  
	, Y( _V[ 1 ] )  
	, Z( _V[ 2 ] )  
	{

	}


//--------------------------------------------------------------------------------------------------
inline qhVector3::operator qhReal*( void )
	{
	return &X;
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3::operator const qhReal*( void ) const
	{
	return &X;
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3& qhVector3::operator*=( const qhVector3& V )
	{
	X *= V.X;
	Y *= V.Y;
	Z *= V.Z;

	return *this;
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3& qhVector3::operator+=( const qhVector3& V )
	{
	X += V.X;
	Y += V.Y;
	Z += V.Z;

	return *this;
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3& qhVector3::operator-=( const qhVector3& V )
	{
	X -= V.X;
	Y -= V.Y;
	Z -= V.Z;

	return *this;
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3& qhVector3::operator*=( qhReal S )
	{
	X *= S;
	Y *= S;
	Z *= S;

	return *this;
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3& qhVector3::operator/=( qhReal S )
	{
	X /= S;
	Y /= S;
	Z /= S;

	return *this;
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhVector3::operator+( void ) const
	{
	return *this;
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhVector3::operator-( void ) const
	{
	return qhVector3( -X, -Y, -Z );
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 operator*( const qhMatrix3& M, const qhVector3& V )
{
	return M.C1 * V.X + M.C2 * V.Y + M.C3 * V.Z;
}


//--------------------------------------------------------------------------------------------------
inline qhVector3 operator*( const qhQuaternion& Q, const qhVector3& V )
	{
	QH_ASSERT( qhAbs( 1.0f - qhLength( Q ) ) < 100.0f * FLT_EPSILON );
	return V + 2 * qhCross( Q.V(), qhCross( Q.V(), V ) + Q.S() * V );
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 operator*( const qhTransform& T, const qhVector3& V )
{
	return T.Rotation * V + T.Translation;
}


//--------------------------------------------------------------------------------------------------
inline qhVector3 operator*( const qhVector3& V1, const qhVector3& V2 )
	{
	return qhVector3( V1.X * V2.X, V1.Y * V2.Y, V1.Z * V2.Z );
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 operator+( const qhVector3& V1, const qhVector3& V2 )
	{
	return qhVector3( V1.X + V2.X, V1.Y + V2.Y, V1.Z + V2.Z );
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 operator-( const qhVector3& V1, const qhVector3& V2 )
	{
	return qhVector3( V1.X - V2.X, V1.Y - V2.Y, V1.Z - V2.Z );
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 operator*( qhReal S, const qhVector3& V )
	{
	return qhVector3( S * V.X, S * V.Y, S * V.Z );
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 operator*( const qhVector3& V, qhReal S )
	{
	return qhVector3( V.X * S, V.Y * S, V.Z * S );
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 operator/( const qhVector3& V, qhReal S )
	{
	return qhVector3( V.X / S, V.Y / S, V.Z / S );
	}


//--------------------------------------------------------------------------------------------------
inline bool operator==( const qhVector3& V1, const qhVector3& V2 )
	{
	return V1.X == V2.X && V1.Y == V2.Y && V1.Z == V2.Z;
	}


//--------------------------------------------------------------------------------------------------
inline bool operator!=( const qhVector3& V1, const qhVector3& V2 )
	{
	return V1.X != V2.X || V1.Y != V2.Y || V1.Z != V2.Z;
	}


//--------------------------------------------------------------------------------------------------
inline void qhStore( qhReal Dst[ 3 ], const qhVector3& V )
	{
	Dst[ 0 ] = V.X;
	Dst[ 1 ] = V.Y;
	Dst[ 2 ] = V.Z;
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhMul( const qhMatrix3& M, const qhVector3& V )
{
	return M.C1 * V.X + M.C2 * V.Y + M.C3 * V.Z;
}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhTMul( const qhMatrix3& M, const qhVector3& V )
{
	qhVector3 Out;
	Out.X = qhDot( M.C1, V );
	Out.Y = qhDot( M.C2, V );
	Out.Z = qhDot( M.C3, V );

	return Out;
}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhMul( const qhQuaternion& Q, const qhVector3& V )
{
	QH_ASSERT( qhAbs( qhReal( 1 ) - qhLength( Q ) ) < qhReal( 100 ) * QH_REAL_EPSILON );
	return V + 2.0f * qhCross( Q.V(), qhCross( Q.V(), V ) + Q.S() * V );
}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhTMul( const qhQuaternion& Q, const qhVector3& V )
{
	QH_ASSERT( qhAbs( qhReal( 1 ) - qhLength( Q ) ) < qhReal( 100 ) * QH_REAL_EPSILON );
	return V - 2.0f * qhCross( Q.V(), Q.S() * V - qhCross( Q.V(), V ) );
}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhMul( const qhTransform& T, const qhVector3& V )
{
	return qhMul( T.Rotation, V ) + T.Translation;
}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhTMul( const qhTransform& T, const qhVector3& V )
{
	return qhTMul( T.Rotation, V - T.Translation );
}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhMul( const qhVector3& V1, const qhVector3& V2 )
	{
	return qhVector3( V1.X * V2.X, V1.Y * V2.Y, V1.Z * V2.Z );
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhAdd( const qhVector3& V1, const qhVector3& V2 )
	{
	return qhVector3( V1.X + V2.X, V1.Y + V2.Y, V1.Z + V2.Z );
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhSub( const qhVector3& V1, const qhVector3& V2 )
	{
	return qhVector3( V1.X - V2.X, V1.Y - V2.Y, V1.Z - V2.Z );
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhCross( const qhVector3& V1, const qhVector3& V2 )
	{
	qhReal X = V1.Y * V2.Z - V1.Z * V2.Y;
	qhReal Y = V1.Z * V2.X - V1.X * V2.Z;
	qhReal Z = V1.X * V2.Y - V1.Y * V2.X;

	return qhVector3( X, Y, Z );
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhScale( const qhVector3& V, qhReal S )
	{
	return qhVector3( V.X * S, V.Y * S, V.Z * S );
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhNormalize( const qhVector3& V )
	{
	qhReal Length = qhLength( V );
	return qhVector3( V.X / Length, V.Y / Length, V.Z / Length );
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhNegate( const qhVector3& V )
	{
	return qhVector3( -V.X, -V.Y, -V.Z );
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhAbs( const qhVector3& V )
	{
	qhVector3 Out;
	Out.X = qhAbs( V.X );
	Out.Y = qhAbs( V.Y );
	Out.Z = qhAbs( V.Z );

	return Out;
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhMin( const qhVector3& V1, const qhVector3& V2 )
	{
	qhVector3 Out;
	Out.X = qhMin( V1.X, V2.X );
	Out.Y = qhMin( V1.Y, V2.Y );
	Out.Z = qhMin( V1.Z, V2.Z );

	return Out;
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhMax( const qhVector3& V1, const qhVector3& V2 )
	{
	qhVector3 Out;
	Out.X = qhMax( V1.X, V2.X );
	Out.Y = qhMax( V1.Y, V2.Y );
	Out.Z = qhMax( V1.Z, V2.Z );

	return Out;
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhClamp( const qhVector3& V, const qhVector3& Min, const qhVector3& Max )
	{
	qhVector3 Out;
	Out.X = qhClamp( V.X, Min.X, Max.X );
	Out.Y = qhClamp( V.Y, Min.Y, Max.Y );
	Out.Z = qhClamp( V.Z, Min.Z, Max.Z );

	return Out;
	}


//--------------------------------------------------------------------------------------------------
inline qhReal qhDot( const qhVector3& V1, const qhVector3& V2 )
	{
	return V1.X * V2.X + V1.Y * V2.Y + V1.Z * V2.Z;
	}


//--------------------------------------------------------------------------------------------------
inline qhReal qhLength( const qhVector3& V )
	{
	return sqrt( qhDot( V, V ) );
	}


//--------------------------------------------------------------------------------------------------
inline qhReal qhLengthSq( const qhVector3& V )
	{
	return qhDot( V, V );
	}


//--------------------------------------------------------------------------------------------------
inline qhReal qhDistance( const qhVector3& V1, const qhVector3& V2 )
	{
	return qhLength( V1 - V2 );
	}


//--------------------------------------------------------------------------------------------------
inline qhReal qhDistanceSq( const qhVector3& V1, const qhVector3& V2 )
	{
	return qhLengthSq( V1 - V2 );
	}


//--------------------------------------------------------------------------------------------------
inline qhReal qhDet( const qhVector3& V1, const qhVector3& V2, const qhVector3& V3 )
	{
	return qhDot( V1, qhCross( V2, V3 ) );
	}


//--------------------------------------------------------------------------------------------------
inline int qhMinElement( const qhVector3& V )
	{
	return V.X < V.Y ? ( V.X < V.Z ? 0 : 2 ) : ( V.Y < V.Z ? 1 : 2 );
	}


//--------------------------------------------------------------------------------------------------
inline int qhMaxElement( const qhVector3& V )
	{
	return V.X < V.Y ? ( V.Y < V.Z ? 2 : 1 ) : ( V.X < V.Z ? 2 : 0 );
	}


//--------------------------------------------------------------------------------------------------
// qhMatrix3
//--------------------------------------------------------------------------------------------------
inline qhMatrix3::qhMatrix3( void )
	{

	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3::qhMatrix3( qhReal A11, qhReal A22, qhReal A33 )
	: C1(  A11, 0.0f, 0.0f )
	, C2( 0.0f,  A22, 0.0f )
	, C3( 0.0f, 0.0f,  A33 )
	{

	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3::qhMatrix3( const qhVector3& C1, const qhVector3& C2, const qhVector3& C3 )
	: C1( C1 )
	, C2( C2 )
	, C3( C3 )
	{

	}

//--------------------------------------------------------------------------------------------------
inline qhMatrix3& qhMatrix3::operator*=( const qhMatrix3& M )
	{
	*this = qhMul( *this, M );
	
	return *this;
	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3& qhMatrix3::operator+=( const qhMatrix3& M )
	{
	C1 += M.C1;
	C2 += M.C2;
	C3 += M.C3;

	return *this;
	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3& qhMatrix3::operator-=( const qhMatrix3& M )
	{
	C1 -= M.C1;
	C2 -= M.C2;
	C3 -= M.C3;

	return *this;
	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3& qhMatrix3::operator*=( qhReal F )
	{
	C1 *= F;
	C2 *= F;
	C3 *= F;

	return *this;
	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3& qhMatrix3::operator/=( qhReal F )
	{
	C1 /= F;
	C2 /= F;
	C3 /= F;

	return *this;
	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3 qhMatrix3::operator+( void ) const
	{
	return *this;
	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3 qhMatrix3::operator-( void ) const
	{
	qhMatrix3 Out;
	Out.C1 = -C1;
	Out.C2 = -C2;
	Out.C3 = -C3;

	return Out;
	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3 operator*( const qhMatrix3& M1, const qhMatrix3& M2 )
	{
	qhMatrix3 Out;
	Out.C1 = M1 * M2.C1;
	Out.C2 = M1 * M2.C2;
	Out.C3 = M1 * M2.C3;

	return Out;
	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3 operator+( const qhMatrix3& M1, const qhMatrix3& M2 )
	{
	qhMatrix3 Out;
	Out.C1 = M1.C1 + M2.C1;
	Out.C2 = M1.C2 + M2.C2;
	Out.C3 = M1.C3 + M2.C3;

	return Out;
	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3 operator-( const qhMatrix3& M1, const qhMatrix3& M2 )
	{
	qhMatrix3 Out;
	Out.C1 = M1.C1 - M2.C1;
	Out.C2 = M1.C2 - M2.C2;
	Out.C3 = M1.C3 - M2.C3;

	return Out;
	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3 operator*( qhReal F, const qhMatrix3& M )
	{
	qhMatrix3 Out;
	Out.C1 = F * M.C1;
	Out.C2 = F * M.C2;
	Out.C3 = F * M.C3;

	return Out;
	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3 operator*( const qhMatrix3& M, qhReal F )
	{
	qhMatrix3 Out;
	Out.C1 = M.C1 * F;
	Out.C2 = M.C2 * F;
	Out.C3 = M.C3 * F;

	return Out;
	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3 operator/( const qhMatrix3& M, qhReal F )
	{
	qhMatrix3 Out;
	Out.C1 = M.C1 / F;
	Out.C2 = M.C2 / F;
	Out.C3 = M.C3 / F;

	return Out;
	}


//--------------------------------------------------------------------------------------------------
inline bool operator==( const qhMatrix3& M1, const qhMatrix3& M2 )
	{
	return M1.C1 == M2.C1 && M1.C2 == M2.C2 && M1.C3 == M2.C3;
	}


//--------------------------------------------------------------------------------------------------
inline bool operator!=( const qhMatrix3& M1, const qhMatrix3& M2 )
	{
	return M1.C1 != M2.C1 || M1.C2 != M2.C2 || M1.C3 != M2.C3;
	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3 qhMul( const qhMatrix3& M1, const qhMatrix3& M2 )
	{
	qhMatrix3 Out;
	Out.C1 = M1 * M2.C1;
	Out.C2 = M1 * M2.C2;
	Out.C3 = M1 * M2.C3;

	return Out;
	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3 qhTMul( const qhMatrix3& M1, const qhMatrix3& M2 )
	{
	QH_ASSERT( 0 );
	return qhMatrix3();
	}

//--------------------------------------------------------------------------------------------------
inline qhMatrix3 qhTranspose( const qhMatrix3& M )
	{
	qhMatrix3 Out;
	Out.C1 = qhVector3( M.C1.X, M.C2.X, M.C3.X );
	Out.C2 = qhVector3( M.C1.Y, M.C2.Y, M.C3.Y );
	Out.C3 = qhVector3( M.C1.Z, M.C2.Z, M.C3.Z );

	return Out;
	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3 qhInvert( const qhMatrix3& M )
	{
	qhReal Det = qhDet( M );
	if ( Det > 1000.0f * FLT_MIN )
		{
		qhMatrix3 Out;
		Out.C1 = qhCross( M.C2, M.C3 ) / Det;
		Out.C2 = qhCross( M.C3, M.C1 ) / Det;
		Out.C3 = qhCross( M.C1, M.C2 ) / Det;

		return qhTranspose( Out );
		}

	return QH_MAT3_ZERO;
	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3 qhInvertT( const qhMatrix3& M )
	{
	qhReal Det = qhDet( M );
	if ( Det > 1000.0f * FLT_MIN )
		{
		qhMatrix3 Out;
		Out.C1 = qhCross( M.C2, M.C3 ) / Det;
		Out.C2 = qhCross( M.C3, M.C1 ) / Det;
		Out.C3 = qhCross( M.C1, M.C2 ) / Det;

		return Out;
		}

	return QH_MAT3_ZERO;
	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3 qhConvert( const qhQuaternion& Q )
	{
	qhReal XX = Q.X * Q.X;
	qhReal YY = Q.Y * Q.Y;
	qhReal ZZ = Q.Z * Q.Z;

	qhReal XY = Q.X * Q.Y;
	qhReal XZ = Q.X * Q.Z;
	qhReal XW = Q.X * Q.W;
	qhReal YZ = Q.Y * Q.Z;
	qhReal YW = Q.Y * Q.W;
	qhReal ZW = Q.Z * Q.W;

	qhMatrix3 Out;
	Out.C1 = qhVector3( 1.0f - 2.0f * ( YY + ZZ ), 2.0f * ( XY + ZW ), 2.0f * ( XZ - YW ) );
	Out.C2 = qhVector3( 2.0f * ( XY - ZW ), 1.0f - 2.0f * ( XX + ZZ ), 2.0f * ( YZ + XW ) );
	Out.C3 = qhVector3( 2.0f * ( XZ + YW ), 2.0f * ( YZ - XW ), 1.0f - 2.0f * ( XX + YY ) );

	return Out;
	}


//--------------------------------------------------------------------------------------------------
inline qhMatrix3 qhSkew( const qhVector3& V )
	{
	qhMatrix3 Out;
	Out.C1 = qhVector3( 0, V.Z, -V.Y );
	Out.C2 = qhVector3( -V.Z, 0, V.X );
	Out.C3 = qhVector3( V.Y, -V.X, 0 );

	return Out;
	}


//--------------------------------------------------------------------------------------------------
inline qhReal qhTrace( const qhMatrix3& M )
	{
	return M.C1.X + M.C2.Y + M.C3.Z;
	}


//--------------------------------------------------------------------------------------------------
inline qhReal qhDet( const qhMatrix3& M )
	{
	return qhDot( M.C1, qhCross( M.C2, M.C3 ) );
	}


//--------------------------------------------------------------------------------------------------
// qhQuaternion
//--------------------------------------------------------------------------------------------------
inline qhQuaternion::qhQuaternion( void )
	{

	}


//--------------------------------------------------------------------------------------------------
inline qhQuaternion::qhQuaternion( qhReal _X, qhReal _Y, qhReal _Z, qhReal _W )
	: X( _X )
	, Y( _Y )
	, Z( _Z )
	, W( _W )
	{

	}


//--------------------------------------------------------------------------------------------------
inline qhQuaternion::qhQuaternion( const qhVector3& V, qhReal S )
	: X( V.X )
	, Y( V.Y )
	, Z( V.Z )
	, W( S )
	{

	}


//--------------------------------------------------------------------------------------------------
inline qhQuaternion::qhQuaternion( const qhReal* Q )
	: X( Q[ 0 ] )
	, Y( Q[ 1 ] )
	, Z( Q[ 2 ] )
	, W( Q[ 3 ] )
	{

	}


//--------------------------------------------------------------------------------------------------
inline qhQuaternion operator*( const qhQuaternion& Q1, const qhQuaternion& Q2 )
{
	qhVector3 V = qhCross( Q1.V(), Q2.V() ) + Q2.V() * Q1.S() + Q1.V() * Q2.S();
	qhReal S = Q1.S() * Q2.S() - qhDot( Q1.V(), Q2.V() );

	return qhQuaternion( V, S );
}


//--------------------------------------------------------------------------------------------------
inline qhQuaternion qhRotation( const qhVector3& V1, const qhVector3& V2 )
	{
	qhQuaternion Out;

	qhVector3 M = 0.5f * ( V1 + V2 );
	if ( qhLengthSq( M ) > FLT_EPSILON * FLT_EPSILON )
		{
		qhVector3 V = qhCross( V1, M );
		qhReal S = qhDot( V1, M );

		Out = qhQuaternion( V, S );
		}
	else
		{
		// Anti-parallel: Use a perpendicular vector
		if ( qhAbs( V1.X ) > 0.5f )
			{
			Out.X = V1.Y;
			Out.Y = -V1.X;
			Out.Z = 0.0f;
			}
		else
			{
			Out.X = 0.0f;
			Out.Y = V1.Z;
			Out.Z = -V1.Y;
			}

		Out.W = 0.0f;
		}

	// The algorithm is simplified and made more accurate by normalizing at the end
	return qhNormalize( Out );
	}


//--------------------------------------------------------------------------------------------------
inline qhQuaternion qhRotationX( float Rad )
	{
	qhReal Sin = qhSin( qhReal( 0.5 ) * Rad );
	qhReal Cos = qhCos( qhReal( 0.5 ) * Rad );
	
	qhVector3 V = qhVector3( Sin, 0.0f, 0.0f );
	qhReal S = Cos;

	return qhQuaternion( V, S );
	}


//--------------------------------------------------------------------------------------------------
inline qhQuaternion qhRotationY( float Rad )
	{
	qhReal Sin = qhSin( qhReal( 0.5 ) * Rad );
	qhReal Cos = qhCos( qhReal( 0.5 ) * Rad );

	qhVector3 V = qhVector3( 0.0f, Sin, 0.0f );
	qhReal S = Cos;

	return qhQuaternion( V, S );
	}


//--------------------------------------------------------------------------------------------------
inline qhQuaternion qhRotationZ( float Rad )
	{
	qhReal Sin = qhSin( qhReal( 0.5 ) * Rad );
	qhReal Cos = qhCos( qhReal( 0.5 ) * Rad );

	qhVector3 V = qhVector3( 0.0f, 0.0f, Sin );
	qhReal S = Cos;

	return qhQuaternion( V, S );
	}

//--------------------------------------------------------------------------------------------------
inline qhQuaternion qhConjugate( const qhQuaternion& Q )
	{
	qhVector3 V = -Q.V();
	qhReal S = Q.S();

	return qhQuaternion( V, S );
	}


//--------------------------------------------------------------------------------------------------
inline qhQuaternion qhNormalize( const qhQuaternion& Q )
	{
	qhReal Length = qhLength( Q );
	if ( Length > qhReal( 1000.0 ) * FLT_MIN )
		{
		qhQuaternion Out;
		Out.X = Q.X / Length;
		Out.Y = Q.Y / Length;
		Out.Z = Q.Z / Length;
		Out.W = Q.W / Length;

		return Out;
		}

	return QH_QUAT_ZERO;
	}


//--------------------------------------------------------------------------------------------------
inline qhReal qhDot( const qhQuaternion& Q1, const qhQuaternion& Q2 )
	{
	return Q1.X * Q2.X + Q1.Y * Q2.Y + Q1.Z * Q2.Z + Q1.W * Q2.W;
	}


//--------------------------------------------------------------------------------------------------
inline qhReal qhLength( const qhQuaternion& Q )
	{
	return qhSqrt( Q.X * Q.X + Q.Y * Q.Y + Q.Z * Q.Z + Q.W * Q.W );
	}


//--------------------------------------------------------------------------------------------------
inline qhReal qhLengthSq( const qhQuaternion& Q )
	{
	return Q.X * Q.X + Q.Y * Q.Y + Q.Z * Q.Z + Q.W * Q.W;
	}


//--------------------------------------------------------------------------------------------------
// qhPlane
//--------------------------------------------------------------------------------------------------
inline qhPlane::qhPlane( void )
	{

	}


//--------------------------------------------------------------------------------------------------
inline qhPlane::qhPlane( const qhVector3& _Normal, qhReal _Offset )
	: Normal( _Normal )
	, Offset( _Offset )
	{
	
	}


//--------------------------------------------------------------------------------------------------
inline qhPlane::qhPlane( const qhVector3& _Normal, const qhVector3& _Point )
	: Normal( _Normal )
	, Offset( qhDot( _Normal, _Point ) )
	{

	}


//--------------------------------------------------------------------------------------------------
inline qhPlane::qhPlane( const qhVector3& Point1, const qhVector3& Point2, const qhVector3& Point3 )
	{
	qhVector3 Edge1 = Point2 - Point1;
	qhVector3 Edge2 = Point3 - Point1;

	Normal = qhCross( Edge1, Edge2 );
	Offset = qhDot( Normal , Point1 ); 
	}


//--------------------------------------------------------------------------------------------------
inline void qhPlane::Negate( void )
	{
	Normal = -Normal;
	Offset = -Offset;
	}


//--------------------------------------------------------------------------------------------------
inline void qhPlane::Normalize( void )
	{
	qhReal Length = qhLength( Normal );
	Normal /= Length;
	Offset /= Length;
	}


//--------------------------------------------------------------------------------------------------
inline void qhPlane::Translate( const qhVector3& Translation )
	{
	Offset += qhDot( Normal, Translation );
	}

	
//--------------------------------------------------------------------------------------------------
inline qhReal qhPlane::Distance( const qhVector3& Point ) const
	{
	return qhDot( Normal, Point ) - Offset;
	}


//--------------------------------------------------------------------------------------------------
inline void qhStore( qhReal Dst[ 4 ], const qhPlane& Plane )
	{
	Dst[ 0 ] = Plane.Normal.X;
	Dst[ 1 ] = Plane.Normal.Y;
	Dst[ 2 ] = Plane.Normal.Z;
	Dst[ 3 ] = Plane.Offset;
	}



//--------------------------------------------------------------------------------------------------
// qhBounds3
//--------------------------------------------------------------------------------------------------
inline qhBounds3::qhBounds3( void )
	{

	}


//--------------------------------------------------------------------------------------------------
inline qhBounds3::qhBounds3( const qhVector3& Min, const qhVector3& Max )
	: Min( Min )
	, Max( Max )
	{

	}


//--------------------------------------------------------------------------------------------------
inline qhBounds3& qhBounds3::operator+=( const qhVector3& Point )
	{
	Min = qhMin( Min, Point );
	Max = qhMax( Max, Point );

	return *this;
	}


//--------------------------------------------------------------------------------------------------
inline qhBounds3& qhBounds3::operator+=( const qhBounds3& Bounds )
	{
	Min = qhMin( Min, Bounds.Min );
	Max = qhMax( Max, Bounds.Max );

	return *this;
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhBounds3::GetCenter( void ) const
	{
	return 0.5f * ( Max + Min );
	}


//--------------------------------------------------------------------------------------------------
inline qhVector3 qhBounds3::GetExtent( void ) const
	{
	return 0.5f * ( Max - Min );
	}


//--------------------------------------------------------------------------------------------------
inline qhReal qhBounds3::GetVolume( void ) const
{
	qhVector3 Diagonal = Max - Min;
	return Diagonal.X * Diagonal.Y * Diagonal.Z;
}


//--------------------------------------------------------------------------------------------------
inline qhBounds3 operator+( const qhBounds3& Bounds1, const qhBounds3& Bounds2 )
	{
	qhBounds3 Out;
	Out.Min = qhMin( Bounds1.Min, Bounds2.Min );
	Out.Max = qhMax( Bounds1.Max, Bounds2.Max );

	return Out;
	}


//--------------------------------------------------------------------------------------------------
inline bool operator==( const qhBounds3& Bounds1, const qhBounds3& Bounds2 )
	{
	return Bounds1.Min == Bounds2.Min && Bounds1.Max == Bounds2.Max;
	}	


//--------------------------------------------------------------------------------------------------
inline bool operator!=( const qhBounds3& Bounds1, const qhBounds3& Bounds2 )
	{
	return Bounds1.Min != Bounds2.Min || Bounds1.Max != Bounds2.Max;
	}

