//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Implementation of our SIMD function using generic c++ code and a struct. This
// implementation will not be especially fast, but gets us up fast on new platforms and also acts
// as an easy-to-understand reference implementation.
//
//==============================================================//


//---------------------------------------------------------------------
// Standard C (fallback/new platform) implementation (only there for compat - slow)
//---------------------------------------------------------------------

FORCEINLINE float SubFloat( const fltx4 & a, int idx )
{
	return a.m128_f32[ idx ];
}

FORCEINLINE float & SubFloat( fltx4 & a, int idx )
{
	return a.m128_f32[idx];
}

FORCEINLINE uint32 SubInt( const fltx4 & a, int idx )
{
	return a.m128_u32[idx];
}

FORCEINLINE uint32 & SubInt( fltx4 & a, int idx )
{
	return a.m128_u32[idx];
}

// Return one in the fastest way -- on the x360, faster even than loading.
FORCEINLINE fltx4 LoadZeroSIMD( void )
{
	return Four_Zeros;
}

// Return one in the fastest way -- on the x360, faster even than loading.
FORCEINLINE fltx4 LoadOneSIMD( void )
{
	return Four_Ones;
}

FORCEINLINE fltx4 SplatXSIMD( const fltx4 & a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = SubFloat( a, 0 );
	SubFloat( retVal, 1 ) = SubFloat( a, 0 );
	SubFloat( retVal, 2 ) = SubFloat( a, 0 );
	SubFloat( retVal, 3 ) = SubFloat( a, 0 );
	return retVal;
}

FORCEINLINE fltx4 SplatYSIMD( fltx4 a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = SubFloat( a, 1 );
	SubFloat( retVal, 1 ) = SubFloat( a, 1 );
	SubFloat( retVal, 2 ) = SubFloat( a, 1 );
	SubFloat( retVal, 3 ) = SubFloat( a, 1 );
	return retVal;
}

FORCEINLINE fltx4 SplatZSIMD( fltx4 a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = SubFloat( a, 2 );
	SubFloat( retVal, 1 ) = SubFloat( a, 2 );
	SubFloat( retVal, 2 ) = SubFloat( a, 2 );
	SubFloat( retVal, 3 ) = SubFloat( a, 2 );
	return retVal;
}

FORCEINLINE fltx4 SplatWSIMD( fltx4 a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = SubFloat( a, 3 );
	SubFloat( retVal, 1 ) = SubFloat( a, 3 );
	SubFloat( retVal, 2 ) = SubFloat( a, 3 );
	SubFloat( retVal, 3 ) = SubFloat( a, 3 );
	return retVal;
}

FORCEINLINE fltx4 SetXSIMD( const fltx4& a, const fltx4& x )
{
	fltx4 result = a;
	SubFloat( result, 0 ) = SubFloat( x, 0 );
	return result;
}

FORCEINLINE fltx4 SetYSIMD( const fltx4& a, const fltx4& y )
{
	fltx4 result = a;
	SubFloat( result, 1 ) = SubFloat( y, 1 );
	return result;
}

FORCEINLINE fltx4 SetZSIMD( const fltx4& a, const fltx4& z )
{
	fltx4 result = a;
	SubFloat( result, 2 ) = SubFloat( z, 2 );
	return result;
}

FORCEINLINE fltx4 SetWSIMD( const fltx4& a, const fltx4& w )
{
	fltx4 result = a;
	SubFloat( result, 3 ) = SubFloat( w, 3 );
	return result;
}

/// Set one component of a SIMD word with the given float value. 
/// This function is a template because the native implementation of
/// this on PPC platforms requires that the component be given as a 
/// compiler immediate -- not a function parameter, not a const function
/// parameter, not even a load from a const static array. It has to be
/// a real immediate. 
/// \param NCOMPONENT 0 is x, 1 is y, 2 is z, 3 is w.
/// \note This function is not particularly performant on any platform (because of 
///       the load from float), so prefer a masked assign from a fltx4 wherever
///       possible. 
template < unsigned int NCOMPONENT >
FORCEINLINE fltx4 SetComponentSIMD( const fltx4& a, float flValue )
{
	fltx4 result = a;
	SubFloat( result, NCOMPONENT ) = flValue;
	return result;
}


// a b c d -> b c d a
FORCEINLINE fltx4 RotateLeft( const fltx4 & a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = SubFloat( a, 1 );
	SubFloat( retVal, 1 ) = SubFloat( a, 2 );
	SubFloat( retVal, 2 ) = SubFloat( a, 3 );
	SubFloat( retVal, 3 ) = SubFloat( a, 0 );
	return retVal;
}

// a b c d -> c d a b
FORCEINLINE fltx4 RotateLeft2( const fltx4 & a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = SubFloat( a, 2 );
	SubFloat( retVal, 1 ) = SubFloat( a, 3 );
	SubFloat( retVal, 2 ) = SubFloat( a, 0 );
	SubFloat( retVal, 3 ) = SubFloat( a, 1 );
	return retVal;
}

#define BINOP(op) 														\
	fltx4 retVal;                                          				\
	SubFloat( retVal, 0 ) = ( SubFloat( a, 0 ) op SubFloat( b, 0 ) );	\
	SubFloat( retVal, 1 ) = ( SubFloat( a, 1 ) op SubFloat( b, 1 ) );	\
	SubFloat( retVal, 2 ) = ( SubFloat( a, 2 ) op SubFloat( b, 2 ) );	\
	SubFloat( retVal, 3 ) = ( SubFloat( a, 3 ) op SubFloat( b, 3 ) );	\
    return retVal;

#define IBINOP(op) 														\
	fltx4 retVal;														\
	SubInt( retVal, 0 ) = ( SubInt( a, 0 ) op SubInt ( b, 0 ) );		\
	SubInt( retVal, 1 ) = ( SubInt( a, 1 ) op SubInt ( b, 1 ) );		\
	SubInt( retVal, 2 ) = ( SubInt( a, 2 ) op SubInt ( b, 2 ) );		\
	SubInt( retVal, 3 ) = ( SubInt( a, 3 ) op SubInt ( b, 3 ) );		\
    return retVal;

FORCEINLINE fltx4 AddSIMD( const fltx4 & a, const fltx4 & b )
{
	BINOP(+);
}

FORCEINLINE fltx4 SubSIMD( const fltx4 & a, const fltx4 & b )				// a-b
{
	BINOP(-);
};

FORCEINLINE fltx4 MulSIMD( const fltx4 & a, const fltx4 & b )				// a*b
{
	BINOP(*);
}

FORCEINLINE fltx4 DivSIMD( const fltx4 & a, const fltx4 & b )				// a/b
{
	BINOP(/);
}


FORCEINLINE fltx4 MaddSIMD( const fltx4 & a, const fltx4 & b, const fltx4 & c )				// a*b + c
{
	return AddSIMD( MulSIMD(a,b), c );
}

FORCEINLINE fltx4 MsubSIMD( const fltx4 & a, const fltx4 & b, const fltx4 & c )				// c - a*b
{
	return SubSIMD( c, MulSIMD(a,b) );
};


FORCEINLINE fltx4 SinSIMD( const fltx4 &radians )
{
	fltx4 result;
	SubFloat( result, 0 ) = sin( SubFloat( radians, 0 ) );
	SubFloat( result, 1 ) = sin( SubFloat( radians, 1 ) );
	SubFloat( result, 2 ) = sin( SubFloat( radians, 2 ) );
	SubFloat( result, 3 ) = sin( SubFloat( radians, 3 ) );
	return result;
}

FORCEINLINE void SinCos3SIMD( fltx4 &sine, fltx4 &cosine, const fltx4 &radians )
{
	SinCos( SubFloat( radians, 0 ), &SubFloat( sine, 0 ), &SubFloat( cosine, 0 ) );
	SinCos( SubFloat( radians, 1 ), &SubFloat( sine, 1 ), &SubFloat( cosine, 1 ) );
	SinCos( SubFloat( radians, 2 ), &SubFloat( sine, 2 ), &SubFloat( cosine, 2 ) );
}

FORCEINLINE void SinCosSIMD( fltx4 &sine, fltx4 &cosine, const fltx4 &radians )
{
	SinCos( SubFloat( radians, 0 ), &SubFloat( sine, 0 ), &SubFloat( cosine, 0 ) );
	SinCos( SubFloat( radians, 1 ), &SubFloat( sine, 1 ), &SubFloat( cosine, 1 ) );
	SinCos( SubFloat( radians, 2 ), &SubFloat( sine, 2 ), &SubFloat( cosine, 2 ) );
	SinCos( SubFloat( radians, 3 ), &SubFloat( sine, 3 ), &SubFloat( cosine, 3 ) );
}

FORCEINLINE fltx4 ArcSinSIMD( const fltx4 &sine )
{
	fltx4 result;
	SubFloat( result, 0 ) = asin( SubFloat( sine, 0 ) );
	SubFloat( result, 1 ) = asin( SubFloat( sine, 1 ) );
	SubFloat( result, 2 ) = asin( SubFloat( sine, 2 ) );
	SubFloat( result, 3 ) = asin( SubFloat( sine, 3 ) );
	return result;
}

FORCEINLINE fltx4 ArcCosSIMD( const fltx4 &cs )
{
	fltx4 result;
	SubFloat( result, 0 ) = acos( SubFloat( cs, 0 ) );
	SubFloat( result, 1 ) = acos( SubFloat( cs, 1 ) );
	SubFloat( result, 2 ) = acos( SubFloat( cs, 2 ) );
	SubFloat( result, 3 ) = acos( SubFloat( cs, 3 ) );
	return result;
}

// tan^1(a/b) .. ie, pass sin in as a and cos in as b
FORCEINLINE fltx4 ArcTan2SIMD( const fltx4 &a, const fltx4 &b )
{
	fltx4 result;
	SubFloat( result, 0 ) = atan2( SubFloat( a, 0 ), SubFloat( b, 0 ) );
	SubFloat( result, 1 ) = atan2( SubFloat( a, 1 ), SubFloat( b, 1 ) );
	SubFloat( result, 2 ) = atan2( SubFloat( a, 2 ), SubFloat( b, 2 ) );
	SubFloat( result, 3 ) = atan2( SubFloat( a, 3 ), SubFloat( b, 3 ) );
	return result;
}

FORCEINLINE fltx4 MaxSIMD( const fltx4 & a, const fltx4 & b )				// max(a,b)
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = max( SubFloat( a, 0 ), SubFloat( b, 0 ) );
	SubFloat( retVal, 1 ) = max( SubFloat( a, 1 ), SubFloat( b, 1 ) );
	SubFloat( retVal, 2 ) = max( SubFloat( a, 2 ), SubFloat( b, 2 ) );
	SubFloat( retVal, 3 ) = max( SubFloat( a, 3 ), SubFloat( b, 3 ) );
	return retVal;
}

FORCEINLINE fltx4 MinSIMD( const fltx4 & a, const fltx4 & b )				// min(a,b)
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = min( SubFloat( a, 0 ), SubFloat( b, 0 ) );
	SubFloat( retVal, 1 ) = min( SubFloat( a, 1 ), SubFloat( b, 1 ) );
	SubFloat( retVal, 2 ) = min( SubFloat( a, 2 ), SubFloat( b, 2 ) );
	SubFloat( retVal, 3 ) = min( SubFloat( a, 3 ), SubFloat( b, 3 ) );
	return retVal;
}

FORCEINLINE fltx4 AndSIMD( const fltx4 & a, const fltx4 & b )				// a & b
{
	IBINOP(&);
}

FORCEINLINE fltx4 AndNotSIMD( const fltx4 & a, const fltx4 & b )			// ~a & b
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ~SubInt( a, 0 ) & SubInt( b, 0 );
	SubInt( retVal, 1 ) = ~SubInt( a, 1 ) & SubInt( b, 1 );
	SubInt( retVal, 2 ) = ~SubInt( a, 2 ) & SubInt( b, 2 );
	SubInt( retVal, 3 ) = ~SubInt( a, 3 ) & SubInt( b, 3 );
	return retVal;
}

FORCEINLINE fltx4 XorSIMD( const fltx4 & a, const fltx4 & b )				// a ^ b
{
	IBINOP(^);
}

FORCEINLINE fltx4 OrSIMD( const fltx4 & a, const fltx4 & b )				// a | b
{
	IBINOP(|);
}

FORCEINLINE fltx4 NegSIMD(const fltx4 &a) // negate: -a
{
	fltx4 retval;
	SubFloat( retval, 0 ) = -SubFloat( a, 0 );
	SubFloat( retval, 1 ) = -SubFloat( a, 1 );
	SubFloat( retval, 2 ) = -SubFloat( a, 2 );
	SubFloat( retval, 3 ) = -SubFloat( a, 3 );

	return retval;
}

FORCEINLINE bool IsAllZeros( const fltx4 & a )								// all floats of a zero?
{
	return	( SubFloat( a, 0 ) == 0.0 ) &&
		( SubFloat( a, 1 ) == 0.0 ) &&
		( SubFloat( a, 2 ) == 0.0 ) &&
		( SubFloat( a, 3 ) == 0.0 ) ;
}


// for branching when a.xyzw > b.xyzw
FORCEINLINE bool IsAllGreaterThan( const fltx4 &a, const fltx4 &b )
{
	return	SubFloat(a,0) > SubFloat(b,0) &&
		SubFloat(a,1) > SubFloat(b,1) &&
		SubFloat(a,2) > SubFloat(b,2) &&
		SubFloat(a,3) > SubFloat(b,3);
}

// for branching when a.xyzw >= b.xyzw
FORCEINLINE bool IsAllGreaterThanOrEq( const fltx4 &a, const fltx4 &b )
{
	return	SubFloat(a,0) >= SubFloat(b,0) &&
		SubFloat(a,1) >= SubFloat(b,1) &&
		SubFloat(a,2) >= SubFloat(b,2) &&
		SubFloat(a,3) >= SubFloat(b,3);
}

// For branching if all a.xyzw == b.xyzw
FORCEINLINE bool IsAllEqual( const fltx4 & a, const fltx4 & b )
{
	return	SubFloat(a,0) == SubFloat(b,0) &&
		SubFloat(a,1) == SubFloat(b,1) &&
		SubFloat(a,2) == SubFloat(b,2) &&
		SubFloat(a,3) == SubFloat(b,3);
}

// For branching if a.x == b.x || a.y == b.y || a.z == b.z || a.w == b.w
FORCEINLINE bool IsAnyEqual( const fltx4 & a, const fltx4 & b )
{
	return	SubFloat(a,0) == SubFloat(b,0) ||
			SubFloat(a,1) == SubFloat(b,1) ||
			SubFloat(a,2) == SubFloat(b,2) ||
			SubFloat(a,3) == SubFloat(b,3);
}

FORCEINLINE int TestSignSIMD( const fltx4 & a )								// mask of which floats have the high bit set
{
	int nRet = 0;

	nRet |= ( SubInt( a, 0 ) & 0x80000000 ) >> 31; // sign(x) -> bit 0
	nRet |= ( SubInt( a, 1 ) & 0x80000000 ) >> 30; // sign(y) -> bit 1
	nRet |= ( SubInt( a, 2 ) & 0x80000000 ) >> 29; // sign(z) -> bit 2
	nRet |= ( SubInt( a, 3 ) & 0x80000000 ) >> 28; // sign(w) -> bit 3

	return nRet;
}

FORCEINLINE bool IsAnyNegative( const fltx4 & a )							// (a.x < 0) || (a.y < 0) || (a.z < 0) || (a.w < 0)
{
	return (0 != TestSignSIMD( a ));
}

FORCEINLINE fltx4 CmpEqSIMD( const fltx4 & a, const fltx4 & b )				// (a==b) ? ~0:0
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ( SubFloat( a, 0 ) == SubFloat( b, 0 )) ? ~0 : 0;
	SubInt( retVal, 1 ) = ( SubFloat( a, 1 ) == SubFloat( b, 1 )) ? ~0 : 0;
	SubInt( retVal, 2 ) = ( SubFloat( a, 2 ) == SubFloat( b, 2 )) ? ~0 : 0;
	SubInt( retVal, 3 ) = ( SubFloat( a, 3 ) == SubFloat( b, 3 )) ? ~0 : 0;
	return retVal;
}

FORCEINLINE fltx4 CmpGtSIMD( const fltx4 & a, const fltx4 & b )				// (a>b) ? ~0:0
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ( SubFloat( a, 0 ) > SubFloat( b, 0 )) ? ~0 : 0;
	SubInt( retVal, 1 ) = ( SubFloat( a, 1 ) > SubFloat( b, 1 )) ? ~0 : 0;
	SubInt( retVal, 2 ) = ( SubFloat( a, 2 ) > SubFloat( b, 2 )) ? ~0 : 0;
	SubInt( retVal, 3 ) = ( SubFloat( a, 3 ) > SubFloat( b, 3 )) ? ~0 : 0;
	return retVal;
}

FORCEINLINE fltx4 CmpGeSIMD( const fltx4 & a, const fltx4 & b )				// (a>=b) ? ~0:0
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ( SubFloat( a, 0 ) >= SubFloat( b, 0 )) ? ~0 : 0;
	SubInt( retVal, 1 ) = ( SubFloat( a, 1 ) >= SubFloat( b, 1 )) ? ~0 : 0;
	SubInt( retVal, 2 ) = ( SubFloat( a, 2 ) >= SubFloat( b, 2 )) ? ~0 : 0;
	SubInt( retVal, 3 ) = ( SubFloat( a, 3 ) >= SubFloat( b, 3 )) ? ~0 : 0;
	return retVal;
}

FORCEINLINE fltx4 CmpLtSIMD( const fltx4 & a, const fltx4 & b )				// (a<b) ? ~0:0
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ( SubFloat( a, 0 ) < SubFloat( b, 0 )) ? ~0 : 0;
	SubInt( retVal, 1 ) = ( SubFloat( a, 1 ) < SubFloat( b, 1 )) ? ~0 : 0;
	SubInt( retVal, 2 ) = ( SubFloat( a, 2 ) < SubFloat( b, 2 )) ? ~0 : 0;
	SubInt( retVal, 3 ) = ( SubFloat( a, 3 ) < SubFloat( b, 3 )) ? ~0 : 0;
	return retVal;
}

FORCEINLINE fltx4 CmpLeSIMD( const fltx4 & a, const fltx4 & b )				// (a<=b) ? ~0:0
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ( SubFloat( a, 0 ) <= SubFloat( b, 0 )) ? ~0 : 0;
	SubInt( retVal, 1 ) = ( SubFloat( a, 1 ) <= SubFloat( b, 1 )) ? ~0 : 0;
	SubInt( retVal, 2 ) = ( SubFloat( a, 2 ) <= SubFloat( b, 2 )) ? ~0 : 0;
	SubInt( retVal, 3 ) = ( SubFloat( a, 3 ) <= SubFloat( b, 3 )) ? ~0 : 0;
	return retVal;
}

FORCEINLINE fltx4 CmpInBoundsSIMD( const fltx4 & a, const fltx4 & b )		// (a <= b && a >= -b) ? ~0 : 0
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = ( SubFloat( a, 0 ) <= SubFloat( b, 0 ) && SubFloat( a, 0 ) >= -SubFloat( b, 0 ) ) ? ~0 : 0;
	SubInt( retVal, 1 ) = ( SubFloat( a, 1 ) <= SubFloat( b, 1 ) && SubFloat( a, 1 ) >= -SubFloat( b, 1 ) ) ? ~0 : 0;
	SubInt( retVal, 2 ) = ( SubFloat( a, 2 ) <= SubFloat( b, 2 ) && SubFloat( a, 2 ) >= -SubFloat( b, 2 ) ) ? ~0 : 0;
	SubInt( retVal, 3 ) = ( SubFloat( a, 3 ) <= SubFloat( b, 3 ) && SubFloat( a, 3 ) >= -SubFloat( b, 3 ) ) ? ~0 : 0;
	return retVal;
}

///\name Functions which perform comparisons, resulting in a float value of either 0.0 or 1.0 (as opposed to resulting in a 32-bit integer mask ).
///@{
FORCEINLINE fltx4 Cmp01EqSIMD( const fltx4 & a, const fltx4 & b )				// (a==b) ? 1.0:0
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = ( SubFloat( a, 0 ) == SubFloat( b, 0 )) ? 1.0 : 0;
	SubFloat( retVal, 1 ) = ( SubFloat( a, 1 ) == SubFloat( b, 1 )) ? 1.0 : 0;
	SubFloat( retVal, 2 ) = ( SubFloat( a, 2 ) == SubFloat( b, 2 )) ? 1.0 : 0;
	SubFloat( retVal, 3 ) = ( SubFloat( a, 3 ) == SubFloat( b, 3 )) ? 1.0 : 0;
	return retVal;
}

FORCEINLINE fltx4 Cmp01GtSIMD( const fltx4 & a, const fltx4 & b )				// (a>b) ? 1.0:0
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = ( SubFloat( a, 0 ) > SubFloat( b, 0 )) ? 1.0 : 0;
	SubFloat( retVal, 1 ) = ( SubFloat( a, 1 ) > SubFloat( b, 1 )) ? 1.0 : 0;
	SubFloat( retVal, 2 ) = ( SubFloat( a, 2 ) > SubFloat( b, 2 )) ? 1.0 : 0;
	SubFloat( retVal, 3 ) = ( SubFloat( a, 3 ) > SubFloat( b, 3 )) ? 1.0 : 0;
	return retVal;
}

FORCEINLINE fltx4 Cmp01GeSIMD( const fltx4 & a, const fltx4 & b )				// (a>=b) ? 1.0:0
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = ( SubFloat( a, 0 ) >= SubFloat( b, 0 )) ? 1.0 : 0;
	SubFloat( retVal, 1 ) = ( SubFloat( a, 1 ) >= SubFloat( b, 1 )) ? 1.0 : 0;
	SubFloat( retVal, 2 ) = ( SubFloat( a, 2 ) >= SubFloat( b, 2 )) ? 1.0 : 0;
	SubFloat( retVal, 3 ) = ( SubFloat( a, 3 ) >= SubFloat( b, 3 )) ? 1.0 : 0;
	return retVal;
}

FORCEINLINE fltx4 Cmp01LtSIMD( const fltx4 & a, const fltx4 & b )				// (a<b) ? 1.0:0
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = ( SubFloat( a, 0 ) < SubFloat( b, 0 )) ? 1.0 : 0;
	SubFloat( retVal, 1 ) = ( SubFloat( a, 1 ) < SubFloat( b, 1 )) ? 1.0 : 0;
	SubFloat( retVal, 2 ) = ( SubFloat( a, 2 ) < SubFloat( b, 2 )) ? 1.0 : 0;
	SubFloat( retVal, 3 ) = ( SubFloat( a, 3 ) < SubFloat( b, 3 )) ? 1.0 : 0;
	return retVal;
}

FORCEINLINE fltx4 Cmp01LeSIMD( const fltx4 & a, const fltx4 & b )				// (a<=b) ? 1.0:0
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = ( SubFloat( a, 0 ) <= SubFloat( b, 0 )) ? 1.0 : 0;
	SubFloat( retVal, 1 ) = ( SubFloat( a, 1 ) <= SubFloat( b, 1 )) ? 1.0 : 0;
	SubFloat( retVal, 2 ) = ( SubFloat( a, 2 ) <= SubFloat( b, 2 )) ? 1.0 : 0;
	SubFloat( retVal, 3 ) = ( SubFloat( a, 3 ) <= SubFloat( b, 3 )) ? 1.0 : 0;
	return retVal;
}

FORCEINLINE fltx4 Cmp01InBoundsSIMD( const fltx4 & a, const fltx4 & b )		// (a <= b && a >= -b) ? 1.0 : 0
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = ( SubFloat( a, 0 ) <= SubFloat( b, 0 ) && SubFloat( a, 0 ) >= -SubFloat( b, 0 ) ) ? 1.0 : 0;
	SubFloat( retVal, 1 ) = ( SubFloat( a, 1 ) <= SubFloat( b, 1 ) && SubFloat( a, 1 ) >= -SubFloat( b, 1 ) ) ? 1.0 : 0;
	SubFloat( retVal, 2 ) = ( SubFloat( a, 2 ) <= SubFloat( b, 2 ) && SubFloat( a, 2 ) >= -SubFloat( b, 2 ) ) ? 1.0 : 0;
	SubFloat( retVal, 3 ) = ( SubFloat( a, 3 ) <= SubFloat( b, 3 ) && SubFloat( a, 3 ) >= -SubFloat( b, 3 ) ) ? 1.0 : 0;
	return retVal;
}


//@}



// simd conditional. for example, a simd version of "( x > 0 ) ? a : b" would be expressed as
// "MaskedAssign( CmpGtSIMD( x, Four_Zeros ), a, b )". A typical use is to conditionally update
// subfiles of a fltx4 based upon some test.
FORCEINLINE fltx4 MaskedAssign( const fltx4 & ReplacementMask, const fltx4 & NewValue, const fltx4 & OldValue )
{
	return OrSIMD(
		AndSIMD( ReplacementMask, NewValue ),
		AndNotSIMD( ReplacementMask, OldValue ) );
}

FORCEINLINE fltx4 ReplicateX4( float flValue )					//  a,a,a,a
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = flValue;
	SubFloat( retVal, 1 ) = flValue;
	SubFloat( retVal, 2 ) = flValue;
	SubFloat( retVal, 3 ) = flValue;
	return retVal;
}

/// replicate a single 32 bit integer value to all 4 components of an m128
FORCEINLINE fltx4 ReplicateIX4( int nValue )
{
	fltx4 retVal;
	SubInt( retVal, 0 ) = nValue;
	SubInt( retVal, 1 ) = nValue;
	SubInt( retVal, 2 ) = nValue;
	SubInt( retVal, 3 ) = nValue;
	return retVal;

}

// Round towards positive infinity
FORCEINLINE fltx4 CeilSIMD( const fltx4 &a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = ceil( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = ceil( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = ceil( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = ceil( SubFloat( a, 3 ) );
	return retVal;

}

// Round towards negative infinity
FORCEINLINE fltx4 FloorSIMD( const fltx4 &a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = floor( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = floor( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = floor( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = floor( SubFloat( a, 3 ) );
	return retVal;

}

FORCEINLINE fltx4 SqrtEstSIMD( const fltx4 & a )				// sqrt(a), more or less
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = sqrt( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = sqrt( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = sqrt( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = sqrt( SubFloat( a, 3 ) );
	return retVal;
}

FORCEINLINE fltx4 SqrtSIMD( const fltx4 & a )					// sqrt(a)
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = sqrt( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = sqrt( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = sqrt( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = sqrt( SubFloat( a, 3 ) );
	return retVal;
}

FORCEINLINE fltx4 ReciprocalSqrtEstSIMD( const fltx4 & a )		// 1/sqrt(a), more or less
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / sqrt( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = 1.0 / sqrt( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = 1.0 / sqrt( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = 1.0 / sqrt( SubFloat( a, 3 ) );
	return retVal;
}

FORCEINLINE fltx4 ReciprocalSqrtEstSaturateSIMD( const fltx4 & a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / sqrt( SubFloat( a, 0 ) != 0.0f ? SubFloat( a, 0 ) : FLT_EPSILON );
	SubFloat( retVal, 1 ) = 1.0 / sqrt( SubFloat( a, 1 ) != 0.0f ? SubFloat( a, 1 ) : FLT_EPSILON );
	SubFloat( retVal, 2 ) = 1.0 / sqrt( SubFloat( a, 2 ) != 0.0f ? SubFloat( a, 2 ) : FLT_EPSILON );
	SubFloat( retVal, 3 ) = 1.0 / sqrt( SubFloat( a, 3 ) != 0.0f ? SubFloat( a, 3 ) : FLT_EPSILON );
	return retVal;
}

FORCEINLINE fltx4 ReciprocalSqrtSIMD( const fltx4 & a )			// 1/sqrt(a)
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / sqrt( SubFloat( a, 0 ) );
	SubFloat( retVal, 1 ) = 1.0 / sqrt( SubFloat( a, 1 ) );
	SubFloat( retVal, 2 ) = 1.0 / sqrt( SubFloat( a, 2 ) );
	SubFloat( retVal, 3 ) = 1.0 / sqrt( SubFloat( a, 3 ) );
	return retVal;
}

FORCEINLINE fltx4 ReciprocalEstSIMD( const fltx4 & a )			// 1/a, more or less
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / SubFloat( a, 0 );
	SubFloat( retVal, 1 ) = 1.0 / SubFloat( a, 1 );
	SubFloat( retVal, 2 ) = 1.0 / SubFloat( a, 2 );
	SubFloat( retVal, 3 ) = 1.0 / SubFloat( a, 3 );
	return retVal;
}

FORCEINLINE fltx4 ReciprocalSIMD( const fltx4 & a )				// 1/a
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / SubFloat( a, 0 );
	SubFloat( retVal, 1 ) = 1.0 / SubFloat( a, 1 );
	SubFloat( retVal, 2 ) = 1.0 / SubFloat( a, 2 );
	SubFloat( retVal, 3 ) = 1.0 / SubFloat( a, 3 );
	return retVal;
}

/// 1/x for all 4 values.
/// 1/0 will result in a big but NOT infinite result
FORCEINLINE fltx4 ReciprocalEstSaturateSIMD( const fltx4 & a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / (SubFloat( a, 0 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 0 ));
	SubFloat( retVal, 1 ) = 1.0 / (SubFloat( a, 1 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 1 ));
	SubFloat( retVal, 2 ) = 1.0 / (SubFloat( a, 2 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 2 ));
	SubFloat( retVal, 3 ) = 1.0 / (SubFloat( a, 3 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 3 ));
	return retVal;
}

FORCEINLINE fltx4 ReciprocalSaturateSIMD( const fltx4 & a )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = 1.0 / (SubFloat( a, 0 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 0 ));
	SubFloat( retVal, 1 ) = 1.0 / (SubFloat( a, 1 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 1 ));
	SubFloat( retVal, 2 ) = 1.0 / (SubFloat( a, 2 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 2 ));
	SubFloat( retVal, 3 ) = 1.0 / (SubFloat( a, 3 ) == 0.0f ? FLT_EPSILON : SubFloat( a, 3 ));
	return retVal;
}

/// 2^x for all values (the antilog)
FORCEINLINE fltx4 PowerOfTwoSIMD( const fltx4 &toPower )
{
	fltx4 retVal;
	SubFloat( retVal, 0 ) = powf( 2, SubFloat(toPower, 0) );
	SubFloat( retVal, 1 ) = powf( 2, SubFloat(toPower, 1) );
	SubFloat( retVal, 2 ) = powf( 2, SubFloat(toPower, 2) );
	SubFloat( retVal, 3 ) = powf( 2, SubFloat(toPower, 3) );

	return retVal;
}

/// horizontal 3d dotproduct
FORCEINLINE fltx4 Dot3SIMD( const fltx4 &a, const fltx4 &b )
{
	float flDot = SubFloat( a, 0 ) * SubFloat( b, 0 ) +
		SubFloat( a, 1 ) * SubFloat( b, 1 ) + 
		SubFloat( a, 2 ) * SubFloat( b, 2 );
	return ReplicateX4( flDot );
}

/// horizontal 4d dotproduct
FORCEINLINE fltx4 Dot4SIMD( const fltx4 &a, const fltx4 &b )
{
	float flDot = SubFloat( a, 0 ) * SubFloat( b, 0 ) +
		SubFloat( a, 1 ) * SubFloat( b, 1 ) + 
		SubFloat( a, 2 ) * SubFloat( b, 2 ) +
		SubFloat( a, 3 ) * SubFloat( b, 3 );
	return ReplicateX4( flDot );
}

/// Clamps the components of a vector to a specified minimum and maximum range.
FORCEINLINE fltx4 ClampVectorSIMD( FLTX4 in, FLTX4 min, FLTX4 max)
{
	return MaxSIMD( min, MinSIMD( max, in ) );
}

/// Squelch the w component of a vector to +0.0.  Most efficient when you say a = SetWToZeroSIMD(a)
/// (avoids a copy)
FORCEINLINE fltx4 SetWToZeroSIMD( const fltx4 & a )
{
	fltx4 retval;
	retval = a;
	SubFloat( retval, 0 ) = 0;
	return retval;
}

FORCEINLINE fltx4 LoadUnalignedSIMD( const void *pSIMD )
{
	return *( reinterpret_cast< const fltx4 *> ( pSIMD ) );
}

FORCEINLINE fltx4 LoadUnaligned3SIMD( const void *pSIMD )
{
	return *( reinterpret_cast< const fltx4 *> ( pSIMD ) );
}

/// load a single unaligned float into the x component of a SIMD word
FORCEINLINE fltx4 LoadUnalignedFloatSIMD( const float *pFlt )
{
	fltx4 retval;
	SubFloat( retval, 0 ) = *pFlt;
	return retval;
}

FORCEINLINE fltx4 LoadAlignedSIMD( const void *pSIMD )
{
	return *( reinterpret_cast< const fltx4 *> ( pSIMD ) );
}

/// for the transitional class -- load a 3-by VectorAligned and squash its w component
FORCEINLINE fltx4 LoadAlignedSIMD( const VectorAligned & pSIMD )
{
	fltx4 retval = LoadAlignedSIMD(pSIMD.Base());
	// squelch w
	SubInt( retval, 3 ) = 0;
	return retval;
}

FORCEINLINE void StoreAlignedSIMD( float *pSIMD, const fltx4 & a )
{
	*( reinterpret_cast< fltx4 *> ( pSIMD ) ) = a;
}

FORCEINLINE void StoreUnalignedSIMD( float *pSIMD, const fltx4 & a )
{
	*( reinterpret_cast< fltx4 *> ( pSIMD ) ) = a;
}

FORCEINLINE void StoreUnaligned3SIMD( float *pSIMD, const fltx4 & a )
{
	*pSIMD     = SubFloat(a, 0);
	*(pSIMD+1) = SubFloat(a, 1);
	*(pSIMD+2) = SubFloat(a, 2);
}

/// strongly typed -- syntactic castor oil used for typechecking as we transition to SIMD
FORCEINLINE void StoreAligned3SIMD( VectorAligned * RESTRICT pSIMD, const fltx4 & a )
{
	StoreAlignedSIMD(pSIMD->Base(),a);
}

/// Store the x,y,z components of the four FLTX4 parameters
// into the four consecutive Vectors pDestination[0], pDestination[1], pDestination[2],
// pDestination[3] The Vectors are assumed
/// to be unaligned.
FORCEINLINE void StoreFourUnalignedVector3SIMD( fltx4 a, fltx4 b, fltx4	c, FLTX4 d, // first three passed by copy (deliberate)
											   Vector * const pDestination )
{
	StoreUnaligned3SIMD( pDestination->Base(), a );
	StoreUnaligned3SIMD( (pDestination+1)->Base(), b );
	StoreUnaligned3SIMD( (pDestination+2)->Base(), c );
	StoreUnaligned3SIMD( (pDestination+3)->Base(), d );
}

// Store the x,y,z components of the four FLTX4 parameters
// into the four consecutive Vectors:
//    pDestination ,  pDestination + 1,  pDestination + 2,  pDestination + 3
// The Vectors are assumed to start on an ALIGNED address, that is, 
// pDestination is 16-byte aligned (thhough obviously pDestination+1 is not).
FORCEINLINE void StoreFourAlignedVector3SIMD( fltx4 a, fltx4 b, fltx4	c, FLTX4 d, // first three passed by copy (deliberate)
											 Vector * const pDestination )
{
	StoreUnaligned3SIMD( pDestination->Base(), a );
	StoreUnaligned3SIMD( (pDestination+1)->Base(), b );
	StoreUnaligned3SIMD( (pDestination+2)->Base(), c );
	StoreUnaligned3SIMD( (pDestination+3)->Base(), d );
}


FORCEINLINE void TransposeSIMD( fltx4 & x, fltx4 & y, fltx4 & z, fltx4 & w )
{

#define SWAP_FLOATS( _a_, _ia_, _b_, _ib_ )				\
	{													\
		float tmp = SubFloat( _a_, _ia_ );				\
		SubFloat( _a_, _ia_ ) = SubFloat( _b_, _ib_ );	\
		SubFloat( _b_, _ib_ ) = tmp;					\
	}

	SWAP_FLOATS( x, 1, y, 0 );
	SWAP_FLOATS( x, 2, z, 0 );
	SWAP_FLOATS( x, 3, w, 0 );
	SWAP_FLOATS( y, 2, z, 1 );
	SWAP_FLOATS( y, 3, w, 1 );
	SWAP_FLOATS( z, 3, w, 2 );
}

/// find the lowest component of a.x, a.y, a.z, and replicate it to the whole return value.
FORCEINLINE fltx4 FindLowestSIMD3( const fltx4 & a )
{
	float lowest = min( min( SubFloat(a, 0), SubFloat(a, 1) ), SubFloat(a, 2));
	return ReplicateX4(lowest);
}

/// find the highest component of a.x, a.y, a.z, and replicate it to the whole return value.
FORCEINLINE fltx4 FindHighestSIMD3( const fltx4 & a )
{
	float highest = max( max( SubFloat(a, 0), SubFloat(a, 1) ), SubFloat(a, 2));
	return ReplicateX4(highest);
}

/// Fixed-point conversion and save as SIGNED INTS.  pDest->x = Int (vSrc.x) note: some
/// architectures have means of doing fixed point conversion when the fix depth is specified as an
/// immediate.. but there is no way to guarantee an immediate as a parameter to function like this.
FORCEINLINE void ConvertStoreAsIntsSIMD(intx4 * RESTRICT pDest, const fltx4 &vSrc)
{
	(*pDest)[0] = SubFloat(vSrc, 0);
	(*pDest)[1] = SubFloat(vSrc, 1);
	(*pDest)[2] = SubFloat(vSrc, 2);
	(*pDest)[3] = SubFloat(vSrc, 3);
}

///@group INTEGER SIMD OPERATIONS {

/// splat all components of a vector to a signed immediate int number.
FORCEINLINE fltx4 IntSetImmediateSIMD( int nValue )
{
	fltx4 retval;
	SubInt( retval, 0 ) = SubInt( retval, 1 ) = SubInt( retval, 2 ) = SubInt( retval, 3) = nValue;
	return retval;
}

/// Load 4 aligned words into a SIMD register
FORCEINLINE i32x4 LoadAlignedIntSIMD(const void * RESTRICT pSIMD)
{
	return *( reinterpret_cast< const i32x4 *> ( pSIMD ) );
}

/// Load 4 unaligned words into a SIMD register
FORCEINLINE i32x4 LoadUnalignedIntSIMD( const void * RESTRICT pSIMD)
{
	return *( reinterpret_cast< const i32x4 *> ( pSIMD ) );
}

/// save into four words, 16-byte aligned
FORCEINLINE void StoreAlignedIntSIMD( int32 *pSIMD, const fltx4 & a )
{
	*( reinterpret_cast< i32x4 *> ( pSIMD ) ) = a;
}

FORCEINLINE void StoreAlignedIntSIMD( intx4 &pSIMD, const fltx4 & a )
{
	*( reinterpret_cast< i32x4 *> ( pSIMD.Base() ) ) = a;
}

FORCEINLINE void StoreUnalignedIntSIMD( int32 *pSIMD, const fltx4 & a )
{
	*( reinterpret_cast< i32x4 *> ( pSIMD ) ) = a;
}

/// Load four consecutive uint16's, and turn them into floating point numbers.  This function isn't
/// especially fast and could be made faster if anyone is using it heavily.
FORCEINLINE fltx4 LoadAndConvertUint16SIMD( const uint16 *pInts )
{
	fltx4 retval;
	SubFloat( retval, 0 ) = pInts[0];
	SubFloat( retval, 1 ) = pInts[1];
	SubFloat( retval, 2 ) = pInts[2];
	SubFloat( retval, 3 ) = pInts[3];
}


/// Take a fltx4 containing fixed-point uints and return them as single precision floats. No fixed
/// point conversion is done.
FORCEINLINE fltx4 UnsignedIntConvertToFltSIMD( const u32x4 &vSrcA )
{
	Assert(0);			/* pc has no such operation */
	fltx4 retval;
	SubFloat( retval, 0 ) = ( (float) SubInt( vSrcA, 0 ) );
	SubFloat( retval, 1 ) = ( (float) SubInt( vSrcA, 1 ) );
	SubFloat( retval, 2 ) = ( (float) SubInt( vSrcA, 2 ) );
	SubFloat( retval, 3 ) = ( (float) SubInt( vSrcA, 3 ) );
	return retval;
}


#if 0				/* pc has no such op */
// Take a fltx4 containing fixed-point sints and 
// return them as single precision floats. No 
// fixed point conversion is done.
FORCEINLINE fltx4 SignedIntConvertToFltSIMD( const i32x4 &vSrcA )
{
	fltx4 retval;
	SubFloat( retval, 0 ) = ( (float) (reinterpret_cast<int32 *>(&vSrcA.m128_s32[0])) );
	SubFloat( retval, 1 ) = ( (float) (reinterpret_cast<int32 *>(&vSrcA.m128_s32[1])) );
	SubFloat( retval, 2 ) = ( (float) (reinterpret_cast<int32 *>(&vSrcA.m128_s32[2])) );
	SubFloat( retval, 3 ) = ( (float) (reinterpret_cast<int32 *>(&vSrcA.m128_s32[3])) );
	return retval;
}


///  works on fltx4's as if they are four uints.  the first parameter contains the words to be
///  shifted, the second contains the amount to shift by AS INTS
///
///  for i = 0 to 3
///  shift = vSrcB_i*32:(i*32)+4
///  vReturned_i*32:(i*32)+31 = vSrcA_i*32:(i*32)+31 << shift
FORCEINLINE i32x4 IntShiftLeftWordSIMD(const i32x4 &vSrcA, const i32x4 &vSrcB)
{
	i32x4 retval;
	SubInt(retval, 0) = SubInt(vSrcA, 0) << SubInt(vSrcB, 0);
	SubInt(retval, 1) = SubInt(vSrcA, 1) << SubInt(vSrcB, 1);
	SubInt(retval, 2) = SubInt(vSrcA, 2) << SubInt(vSrcB, 2);
	SubInt(retval, 3) = SubInt(vSrcA, 3) << SubInt(vSrcB, 3);


	return retval;
}
//@} 
