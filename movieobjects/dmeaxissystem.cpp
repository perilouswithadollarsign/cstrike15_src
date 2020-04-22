//============ Copyright (c) Valve Corporation, All rights reserved. ==========
//
//=============================================================================


#include "datamodel/dmelementfactoryhelper.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmelog.h"
#include "movieobjects/dmemesh.h"
#include "movieobjects/dmemodel.h"
#include "movieobjects/dmevertexdata.h"
#include "tier1/fmtstr.h"


#include "movieobjects/dmeaxissystem.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeAxisSystem, CDmeAxisSystem );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static CDmeAxisSystem::Axis_t GetAbsAxisAndSign(
	int &nSign, const CDmeAxisSystem::Axis_t eAxis )
{
	nSign = ( eAxis < 0 ) ? -1 : 1;
	return static_cast< CDmeAxisSystem::Axis_t >( abs( eAxis ) );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static CDmeAxisSystem::ForwardParity_t GetAbsForwardParityAndSign(
	int &nSign, const CDmeAxisSystem::ForwardParity_t eForwardParity )
{
	nSign = ( eForwardParity < 0 ) ? -1 : 1;
	return static_cast< CDmeAxisSystem::ForwardParity_t >( abs( eForwardParity ) );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static CDmeAxisSystem::Axis_t ComputeAbsForwardAxisAndSign(
	int &nSign,
	const CDmeAxisSystem::Axis_t eUpAxis,
	const CDmeAxisSystem::ForwardParity_t eForwardParity )
{
	Assert( CDmeAxisSystem::IsValid( eUpAxis, eForwardParity ) );

	int nUpAxisSign = 0;
	const CDmeAxisSystem::Axis_t eAbsUpAxis = GetAbsAxisAndSign( nUpAxisSign, eUpAxis );
	AssertDbg( eAbsUpAxis >= CDmeAxisSystem::AS_AXIS_X && eAbsUpAxis <= CDmeAxisSystem::AS_AXIS_Z );

	const CDmeAxisSystem::ForwardParity_t eAbsForwardParity = GetAbsForwardParityAndSign( nSign, eForwardParity );
	AssertDbg( eAbsForwardParity >= CDmeAxisSystem::AS_PARITY_EVEN && eAbsForwardParity <= CDmeAxisSystem::AS_PARITY_ODD );

	// eAxisParityMap[Axis_t - 1][ ForwardParityType_t - 1 ] gives parity axis
	static const CDmeAxisSystem::Axis_t eAxisParityMap[][2] = {
		{ CDmeAxisSystem::AS_AXIS_Y, CDmeAxisSystem::AS_AXIS_Z },	// Up X
		{ CDmeAxisSystem::AS_AXIS_X, CDmeAxisSystem::AS_AXIS_Z },	// Up Y
		{ CDmeAxisSystem::AS_AXIS_X, CDmeAxisSystem::AS_AXIS_Y }	// Up Z
	};

	const CDmeAxisSystem::Axis_t nAbsForwardAxis = eAxisParityMap[ eAbsUpAxis - 1 ][ eAbsForwardParity - 1 ];
	AssertDbg( nAbsForwardAxis != eAbsUpAxis );

	return nAbsForwardAxis;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static CDmeAxisSystem::Axis_t ComputeAbsLeftAxisAndSign(
	int &nSign,
	const CDmeAxisSystem::Axis_t eUpAxis,
	const CDmeAxisSystem::ForwardParity_t eForwardParity,
	const CDmeAxisSystem::CoordSys_t eCoordSys )
{
	Assert( CDmeAxisSystem::IsValid( eUpAxis, eForwardParity, eCoordSys ) );

	int nUpAxisSign = 0;
	const CDmeAxisSystem::Axis_t eAbsUpAxis = GetAbsAxisAndSign( nUpAxisSign, eUpAxis );
	AssertDbg( nUpAxisSign == -1 || nUpAxisSign == 1 );
	AssertDbg( eAbsUpAxis >= CDmeAxisSystem::AS_AXIS_X && eAbsUpAxis <= CDmeAxisSystem::AS_AXIS_Z );

	int nForwardAxisSign = 0;
	const CDmeAxisSystem::Axis_t eAbsForwardAxis = ComputeAbsForwardAxisAndSign( nForwardAxisSign, eUpAxis, eForwardParity );
	AssertDbg( eAbsForwardAxis >= CDmeAxisSystem::AS_AXIS_X && eAbsForwardAxis <= CDmeAxisSystem::AS_AXIS_Z );
	AssertDbg( nForwardAxisSign == -1 || nForwardAxisSign == 1 );

	AssertDbg( eAbsForwardAxis != eAbsUpAxis );

	// Chart of cross products, COL x ROW, 24 possibilities as parallel vectors are not allowed
	// NOTE: The 3x3 matrices are the same across both diagonals and the two sets of 3x3's are
	//       simply the negative image of each other
	//
	//            +=====+=====+=====+=====+=====+=====+
	//            | -X  | -Y  | -Z  |  X  |  Y  |  Z  |
	//            +=====+=====+=====+=====+=====+=====+
	//
	//  +=====+   +-----+-----+-----+-----+-----+-----+
	//  | -X  |   |  .  |  Z  | -Y  |  .  | -Z  |  Y  |
	//  +-----+   +-----+-----+-----+-----+-----+-----+
	//  | -Y  |   | -Z  |  .  |  X  |  Z  |  .  | -X  |
	//  +-----+   +-----+-----+-----+-----+-----+-----+
	//  | -Z  |   |  Y  | -X  |  .  | -Y  |  X  |  .  |
	//  +-----+   +-----+-----+-----+-----+-----+-----+
	//  |  X  |   |  .  | -Z  |  Y  |  .  |  Z  | -Y  |
	//  +-----+   +-----+-----+-----+-----+-----+-----+
	//  |  Y  |   |  Z  |  .  | -X  | -Z  |  .  |  X  |
	//  +-----+   +-----+-----+-----+-----+-----+-----+
	//  |  Z  |   | -Y  |  X  |  .  |  Y  | -X  |  .  |
	//  +=====+   +-----+-----+-----+-----+-----+-----+

	// The 3x3 matrix from the above table without sign, sign is broken down in next table
	// 0's are invalid cases, index [up - 1][forward - 1]
	static const CDmeAxisSystem::Axis_t eAxisUpForwardMap[3][3] = {
		{ (CDmeAxisSystem::Axis_t)0,	CDmeAxisSystem::AS_AXIS_Z,	CDmeAxisSystem::AS_AXIS_Y },
		{ CDmeAxisSystem::AS_AXIS_Z,	(CDmeAxisSystem::Axis_t)0,	CDmeAxisSystem::AS_AXIS_X },
		{ CDmeAxisSystem::AS_AXIS_Y,	CDmeAxisSystem::AS_AXIS_X,	(CDmeAxisSystem::Axis_t)0 }
	};

	// The signs from the lower right 3x3 case (positive axis x positive axis)
	// 0's are invalid cases, index [up - 1][forward - 1]
	static const int nSignUpForwardMap[3][3] = {
		{  0,  1, -1 },
		{ -1,  0,  1 },
		{  1, -1,  0 }
	};

	const CDmeAxisSystem::Axis_t eAbsLeftAxis = eAxisUpForwardMap[ eAbsUpAxis - 1 ][ eAbsForwardAxis - 1 ];
	AssertDbg( eAbsLeftAxis >= CDmeAxisSystem::AS_AXIS_X && eAbsLeftAxis <= CDmeAxisSystem::AS_AXIS_Z );
	AssertDbg( eAbsLeftAxis != eAbsUpAxis );
	AssertDbg( eAbsLeftAxis != eAbsForwardAxis );

	nSign = nSignUpForwardMap[ eAbsUpAxis - 1 ][ eAbsForwardAxis - 1 ];

	// If up and forward are not the same sign, then sign is reversed from table
	if ( nUpAxisSign != nForwardAxisSign )
	{
		nSign = -nSign;
	}

	// If left handed, reverse sign of axis
	if ( eCoordSys == CDmeAxisSystem::AS_LEFT_HANDED )
	{
		nSign = -nSign;
	}

	return eAbsLeftAxis;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeAxisSystem::OnConstruction()
{
	// Initialize to Maya Y Up
	m_nUpAxis.InitAndSet( this, "upAxis", AS_AXIS_Y );
	m_nForwardParity.InitAndSet( this, "forwardParity", AS_PARITY_ODD );
	m_nCoordSys.InitAndSet( this, "coordSys", AS_RIGHT_HANDED );

	Assert( IsValid() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeAxisSystem::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeAxisSystem::Init( Axis_t eUpAxis, ForwardParity_t eForwardParity, CoordSys_t eCoordSys /*= AS_RIGHT_HANDED */ )
{
	if ( !IsValid( eUpAxis, eForwardParity, eCoordSys ) )
	{
		AssertMsg( false, "Invalid Initialization of CDmeAxisSystem" );
		return false;
	}

	m_nUpAxis = eUpAxis;
	m_nForwardParity = eForwardParity;
	m_nCoordSys = eCoordSys;

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeAxisSystem::Init( PredefinedAxisSystem ePredefinedAxisSystem )
{
	Axis_t eUpAxis = static_cast< Axis_t >( m_nUpAxis.Get() );
	ForwardParity_t eForwardParity = static_cast< ForwardParity_t >( m_nForwardParity.Get() );
	CoordSys_t eCoordSys = static_cast< CoordSys_t >( m_nCoordSys.Get() );

	if ( !GetPredefinedAxisSystem( eUpAxis, eForwardParity, eCoordSys, ePredefinedAxisSystem ) )
		return false;

	return Init( eUpAxis, eForwardParity, eCoordSys );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeAxisSystem::IsValid() const
{
	return IsValid(
		static_cast< Axis_t >( m_nUpAxis.Get() ),
		static_cast< ForwardParity_t >( m_nForwardParity.Get() ),
		static_cast< CoordSys_t >( m_nCoordSys.Get() ) );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeAxisSystem::IsValid( Axis_t nUpAxis, ForwardParity_t eForwardParity, CoordSys_t eCoordSys )
{
	if ( nUpAxis == 0 || nUpAxis < AS_AXIS_NZ || nUpAxis > AS_AXIS_Z )
		return false;

	if ( eForwardParity == 0 || eForwardParity < AS_PARITY_NODD || eForwardParity > AS_PARITY_ODD )
		return false;

	if ( eCoordSys < AS_RIGHT_HANDED || eCoordSys > AS_LEFT_HANDED )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeAxisSystem::GetPredefinedAxisSystem(
	Axis_t &eUpAxis,
	ForwardParity_t &eForwardParity,
	CoordSys_t &eCoordSys,
	PredefinedAxisSystem ePredefinedAxisSystem )
{
	if ( ePredefinedAxisSystem < AS_VALVE_ENGINE || ePredefinedAxisSystem > AS_3DSMAX )
		return false;

	static int predefinedAxisSystemList[][3] = {
		{  AS_AXIS_Z,	 AS_PARITY_EVEN,	AS_RIGHT_HANDED },	// AS_VALVE_ENGINE	+Z, +X
		{  AS_AXIS_Z,	-AS_PARITY_ODD,		AS_RIGHT_HANDED },	// AS_SMD			+Z, -Y
		{  AS_AXIS_Y,	 AS_PARITY_ODD,		AS_RIGHT_HANDED },	// AS_MAYA_YUP		+Y, +Z
		{  AS_AXIS_Z,	-AS_PARITY_ODD,		AS_RIGHT_HANDED },	// AS_MAYA_ZUP		+Z, -Y
		{  AS_AXIS_Y,	 AS_PARITY_ODD,		AS_RIGHT_HANDED },	// AS_MODO			+Y, +Z
		{  AS_AXIS_Z,	-AS_PARITY_ODD,		AS_RIGHT_HANDED }	// AS_3DSMAX		+Z, -Y
	};

	COMPILE_TIME_ASSERT( AS_VALVE_ENGINE == 0 );
	COMPILE_TIME_ASSERT( AS_SMD == 1 );
	COMPILE_TIME_ASSERT( AS_MAYA_YUP == 2 );
	COMPILE_TIME_ASSERT( AS_MAYA_ZUP == 3 );
	COMPILE_TIME_ASSERT( AS_MODO_YUP == 4 );
	COMPILE_TIME_ASSERT( AS_3DSMAX == 5 );
	COMPILE_TIME_ASSERT( AS_3DSMAX + 1 == ARRAYSIZE( predefinedAxisSystemList ) );

	eUpAxis = static_cast< Axis_t >( predefinedAxisSystemList[ePredefinedAxisSystem][0] );
	eForwardParity = static_cast< ForwardParity_t >( predefinedAxisSystemList[ePredefinedAxisSystem][1] );
	eCoordSys = static_cast< CoordSys_t >( predefinedAxisSystemList[ePredefinedAxisSystem][2] );

	Assert( IsValid( eUpAxis, eForwardParity, eCoordSys ) );

	return true;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeAxisSystem::IsEqual( PredefinedAxisSystem ePredefinedAxisSystem ) const
{
	Axis_t eAUpAxis;
	ForwardParity_t eAForwardParity;
	CoordSys_t eACoordSys;

	if ( !GetPredefinedAxisSystem( eAUpAxis, eAForwardParity, eACoordSys, ePredefinedAxisSystem ) )
		return false;

	const Axis_t eBUpAxis = GetUpAxis();
	const ForwardParity_t eBForwardParity = GetForwardParity();
	const CoordSys_t eBCoordSys = GetCoordSys();

	return ( eBUpAxis == eAUpAxis ) && ( eBForwardParity == eAForwardParity ) && ( eBCoordSys == eACoordSys );

}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeAxisSystem::Axis_t CDmeAxisSystem::GetUpAxis() const
{
	return static_cast< Axis_t >( m_nUpAxis.Get() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeAxisSystem::ForwardParity_t CDmeAxisSystem::GetForwardParity() const
{
	return static_cast< ForwardParity_t >( m_nForwardParity.Get() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeAxisSystem::CoordSys_t CDmeAxisSystem::GetCoordSys() const
{
	return static_cast< CoordSys_t >( m_nCoordSys.Get() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeAxisSystem::ComputeMatrix(
	matrix3x4a_t &mMatrix, const PredefinedAxisSystem ePredefinedAxisSystem )
{
	Axis_t eUpAxis;
	ForwardParity_t eForwardParity;
	CoordSys_t eCoordSys;

	GetPredefinedAxisSystem( eUpAxis, eForwardParity, eCoordSys, ePredefinedAxisSystem );
	ComputeMatrix( mMatrix, eUpAxis, eForwardParity, eCoordSys );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeAxisSystem::GetConversionMatrix(
	matrix3x4a_t &mMat,
	PredefinedAxisSystem eFromAxisSystem,
	PredefinedAxisSystem eToAxisSystem )
{
	Axis_t eFromUpAxis;
	ForwardParity_t eFromForwardParity;
	CoordSys_t eFromCoordSys;

	GetPredefinedAxisSystem( eFromUpAxis, eFromForwardParity, eFromCoordSys, eFromAxisSystem );
	Assert( IsValid( eFromUpAxis, eFromForwardParity, eFromCoordSys ) );

	Axis_t eToUpAxis;
	ForwardParity_t eToForwardParity;
	CoordSys_t eToCoordSys;

	GetPredefinedAxisSystem( eToUpAxis, eToForwardParity, eToCoordSys, eToAxisSystem );
	Assert( IsValid( eToUpAxis, eToForwardParity, eToCoordSys ) );

	GetConversionMatrix( mMat,
		eFromUpAxis, eFromForwardParity, eFromCoordSys,
		eToUpAxis, eToForwardParity, eToCoordSys );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeAxisSystem::GetConversionMatrix(
	matrix3x4a_t &mMat,
	Axis_t eFromUpAxis, ForwardParity_t eFromForwardParity,
	Axis_t eToUpAxis, ForwardParity_t eToForwardParity )
{
	GetConversionMatrix( mMat,
		eFromUpAxis, eFromForwardParity, AS_RIGHT_HANDED,
		eToUpAxis, eToForwardParity, AS_RIGHT_HANDED );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeAxisSystem::GetConversionMatrix(
	matrix3x4a_t &mMat,
	Axis_t eFromUpAxis, ForwardParity_t eFromForwardParity, CoordSys_t eFromCoordSys,
	Axis_t eToUpAxis, ForwardParity_t eToForwardParity, CoordSys_t eToCoordSys )
{
	matrix3x4a_t mFrom;
	ComputeMatrix( mFrom, eFromUpAxis, eFromForwardParity, eFromCoordSys );

	matrix3x4a_t mTo;
	ComputeMatrix( mTo, eToUpAxis, eToForwardParity, eToCoordSys );

	// Matrix is guaranteed to be a rotation matrix (orthonormal upper 3x3) with no translation
	// so in this case, Transpose is the same as Inverse
	matrix3x4a_t mFromInv;
	MatrixTranspose( mFrom, mFromInv );

	MatrixMultiply( mTo, mFromInv, mMat );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CUtlString CDmeAxisSystem::GetAxisString(
	Axis_t eUpAxis,
	ForwardParity_t eForwardParity,
	CoordSys_t eCoordSys )
{
	int nUSign = 0;
	const int nU = GetAbsAxisAndSign( nUSign, eUpAxis );
	int nFSign = 0;
	const int nF = ::ComputeAbsForwardAxisAndSign( nFSign, eUpAxis, eForwardParity );
	int nLSign = 0;
	const int nL = ::ComputeAbsLeftAxisAndSign( nLSign, eUpAxis, eForwardParity, eCoordSys );

	const char *szAxis[] = { "x", "y", "z" };

	return CUtlString( CFmtStr( "u_%s%s_f_%s%s_l_%s%s",
		nUSign < 0 ? "n" : "", szAxis[nU - 1],
		nFSign < 0 ? "n" : "", szAxis[nF - 1],
		nLSign < 0 ? "n" : "", szAxis[nL - 1] ).Get() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeAxisSystem::Axis_t CDmeAxisSystem::GetAbsUpAxisAndSign( int &nSign ) const
{
	return ::GetAbsAxisAndSign( nSign, static_cast< Axis_t >( m_nUpAxis.Get() ) );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeAxisSystem::ForwardParity_t CDmeAxisSystem::GetAbsForwardParityAndSign( int &nSign ) const
{
	return ::GetAbsForwardParityAndSign( nSign, GetForwardParity() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeAxisSystem::Axis_t CDmeAxisSystem::ComputeAbsForwardAxisAndSign( int &nSign ) const
{
	Assert( IsValid() );
	return ::ComputeAbsForwardAxisAndSign( nSign, GetUpAxis(), GetForwardParity() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
CDmeAxisSystem::Axis_t CDmeAxisSystem::ComputeLeftAxis( int &nSign ) const
{
	Assert( IsValid() );
	return ::ComputeAbsLeftAxisAndSign( nSign, GetUpAxis(), GetForwardParity(), GetCoordSys() );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeAxisSystem::ComputeMatrix(
	matrix3x4a_t &mMatrix,
	const Axis_t eUpAxis,
	const ForwardParity_t eForwardParity,
	const CoordSys_t eCoordSys )
{
	Assert( IsValid( eUpAxis, eForwardParity, eCoordSys ) );

	int nUpAxisSign = 0;
	const Axis_t eAbsUpAxis = ::GetAbsAxisAndSign( nUpAxisSign, eUpAxis );
	AssertDbg( nUpAxisSign == -1 || nUpAxisSign == 1 );
	AssertDbg( eAbsUpAxis >= AS_AXIS_X && eAbsUpAxis <= AS_AXIS_Z );

	int nForwardAxisSign = 0;
	const Axis_t eAbsForwardAxis = ::ComputeAbsForwardAxisAndSign( nForwardAxisSign, eUpAxis, eForwardParity );
	AssertDbg( eAbsForwardAxis >= AS_AXIS_X && eAbsForwardAxis <= AS_AXIS_Z );
	AssertDbg( nForwardAxisSign == -1 || nForwardAxisSign == 1 );

	AssertDbg( eAbsForwardAxis != eAbsUpAxis );

	int nLeftAxisSign = 0;
	const Axis_t eAbsLeftAxis = ::ComputeAbsLeftAxisAndSign( nLeftAxisSign, eUpAxis, eForwardParity, eCoordSys );
	AssertDbg( eAbsLeftAxis >= AS_AXIS_X && eAbsLeftAxis <= AS_AXIS_Z );
	AssertDbg( nLeftAxisSign == -1 || nLeftAxisSign == 1 );
	AssertDbg( eAbsLeftAxis != eAbsUpAxis );
	AssertDbg( eAbsLeftAxis != eAbsForwardAxis );

	// flVectorList[nAbsAxis - 1][( nSign + 1 ) / 2][]
	static const float flVectorList[][2][3] = {
		{
			{ -1.0f,  0.0f,  0.0f },	// -X
			{  1.0f,  0.0f,  0.0f }		//  X
		},
		{
			{  0.0f, -1.0f,  0.0f },	// -Y
			{  0.0f,  1.0f,  0.0f }		//  Y
		},
		{
			{  0.0f,  0.0f, -1.0f },	// -Z
			{  0.0f,  0.0f,  1.0f }		//  Z
		}
	};

	const int nUpSignIndex = ( nUpAxisSign + 1 ) / 2;
	const int nForwardSignIndex = ( nForwardAxisSign + 1 ) / 2;
	const int nLeftSignIndex = ( nLeftAxisSign + 1 ) / 2;

	// Is this a bad idea?
	const Vector &vUp = *reinterpret_cast< const Vector * >( flVectorList[eAbsUpAxis - 1][nUpSignIndex] );
	const Vector &vForward = *reinterpret_cast< const Vector * >( flVectorList[eAbsForwardAxis - 1][nForwardSignIndex] );
	const Vector &vLeft = *reinterpret_cast< const Vector * >( flVectorList[eAbsLeftAxis - 1][nLeftSignIndex] );

	MatrixInitialize/*FLU*/( mMatrix, vec3_origin, vForward, vLeft, vUp );
}