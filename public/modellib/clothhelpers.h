//====== Copyright © Valve Corporation, All rights reserved. =======
#ifndef MODELLIB_CLOTHHELPERS
#define MODELLIB_CLOTHHELPERS


#include "tier1/utlstring.h"
#include "tier1/utlhashtable.h"
#include "tier1/utlvector.h"

class CClothBoneMap
{
public:
	CClothBoneMap();
	~CClothBoneMap();

	void RegisterBone( const char *pName, int nBone );
public:
	struct Patch_t
	{
		Patch_t(): m_nColumnCount( 0 ), m_nRowCount( 0 ) {}
		int m_nColumnCount;
		int m_nRowCount;
		CUtlString m_Name;
	public:
		void Insert( int nRow, int nColumn, int nBone );
		int GetBone( int nRow, int nColumn );
	protected:
		CUtlHashtable< uint, int > m_Bones; // ( Row << 16 ) | Column  =>  Bone name
	};
public:
	Patch_t *GetPatch( int nPatch ) { return m_Patches[nPatch]; }
	int GetPatchCount() const { return m_Patches.Count(); }

protected:
	CUtlVector< Patch_t* > m_Patches;
protected:
	Patch_t *GetPatch( const char *pName, const char *pNameEnd );
};




class CReverseParser
{
public:
	CReverseParser( const char *pString ): m_pString( pString ), m_pEnd( pString + V_strlen( pString ) ) {}

	int MatchInt();
	char ReadChar( );
	bool IsValid() { return m_pEnd != NULL; }
	const char *GetEnd() const { return m_pEnd; }
	CUtlString GetRemainder()const;

protected:
	const char * m_pString;
	const char * m_pEnd;
};



inline Vector GetScaleVector( const matrix3x4_t &tm )
{
	Vector vScale( tm.GetColumn( X_AXIS ).Length( ), tm.GetColumn( Y_AXIS ).Length( ), tm.GetColumn( Z_AXIS ).Length( ) );
	return vScale;
}


inline bool IsGoodWorldTransform( const matrix3x4_t &tm, float flExpectedScale = 1.0f, float flTolerance = 0.001f )
{
	Vector vScale = GetScaleVector( tm );
	return tm.IsValid( ) && ( vScale - Vector( flExpectedScale, flExpectedScale, flExpectedScale ) ).Length( ) < flTolerance && tm.GetOrthogonalityError() < flTolerance;
}



//-----------------------------------------------------------------------------
// functions for dealing with scale
//-----------------------------------------------------------------------------
inline matrix3x4_t ScaleMatrix3x3( const matrix3x4_t &transform, float flScale )
{
	if ( flScale == 1.0f )
		return transform;

	matrix3x4_t out;

	for ( int i = 0; i < 3; ++i )
	{
		for ( int j = 0; j < 3; ++j )
		{
			out[ i ][ j ] = transform[ i ][ j ] * flScale;
		}
		out[ i ][ 3 ] = transform[ i ][ 3 ];
	}

	return out;
}

inline void Set3x3( matrix3x4a_t &dest, const matrix3x4a_t &src )
{
	dest.m_flMatVal[ 0 ][ 0 ] = src.m_flMatVal[ 0 ][ 0 ];
	dest.m_flMatVal[ 0 ][ 1 ] = src.m_flMatVal[ 0 ][ 1 ];
	dest.m_flMatVal[ 0 ][ 2 ] = src.m_flMatVal[ 0 ][ 2 ];
	dest.m_flMatVal[ 1 ][ 0 ] = src.m_flMatVal[ 1 ][ 0 ];
	dest.m_flMatVal[ 1 ][ 1 ] = src.m_flMatVal[ 1 ][ 1 ];
	dest.m_flMatVal[ 1 ][ 2 ] = src.m_flMatVal[ 1 ][ 2 ];
	dest.m_flMatVal[ 2 ][ 0 ] = src.m_flMatVal[ 2 ][ 0 ];
	dest.m_flMatVal[ 2 ][ 1 ] = src.m_flMatVal[ 2 ][ 1 ];
	dest.m_flMatVal[ 2 ][ 2 ] = src.m_flMatVal[ 2 ][ 2 ];
}

inline void Set3x3( matrix3x4a_t &dest, const matrix3x4a_t &src, const Vector &vNewOrigin )
{
	Set3x3( dest, src );
	dest.SetOrigin( vNewOrigin );
}




inline matrix3x4_t AlignX( matrix3x4a_t &tm, const Vector &vNewX, const Vector &vOrigin )
{
	float flNewXLen = vNewX.Length( );
	if ( flNewXLen > 0.03f ) // if the new X axis is not well-defined, it makes little sense to adjust the base
	{
		Quaternion q = RotateBetween( tm.GetColumn( X_AXIS ).Normalized( ), vNewX / flNewXLen );
		matrix3x4_t rot = QuaternionMatrix( q ), rotated = rot * tm;
		AssertDbg( CrossProduct( rotated.GetColumn( X_AXIS ), vNewX ).Length( ) < 0.002f * vNewX.Length() );
		AssertDbg( rotated.GetOrthogonalityError() < 0.001f );
		rotated.SetOrigin( vOrigin );
		return rotated;
	}
	else
	{
		return tm;
	}
}


inline matrix3x4_t Descale( const matrix3x4a_t &tm )
{
	float flInvScale = 1.0f / tm.GetColumn( X_AXIS ).Length();
	return ScaleMatrix3x3( tm, flInvScale );
}


#endif 
