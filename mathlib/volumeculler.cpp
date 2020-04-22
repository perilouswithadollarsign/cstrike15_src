//========= Copyright © 1996-2012, Valve Corporation, All rights reserved. ============//
//
//
//=====================================================================================//

#include <math.h>
#include <float.h>	// needed for flt_epsilon
#include "basetypes.h"
#include "tier0/dbg.h"
#include "mathlib/vector4d.h"
#include "mathlib/vector.h"
#include "mathlib/volumeculler.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Returns true if the AABB is completely within the frustum.
// Basic scalar approach derived from "Real Time Rendering" 2nd edition section 13.13.3.
// TODO: Replace this a function similar to CFrustum::CheckBoxInline().
static inline bool AABBInsideFrustum( const fltx4 *pPlanes, FLTX4 vCenter4, FLTX4 vDiagonal4 )
{
	fltx4 mp0 = Dot4SIMD( vCenter4, pPlanes[0] );
	fltx4 mp1 = Dot4SIMD( vCenter4, pPlanes[1] );
	fltx4 mp2 = Dot4SIMD( vCenter4, pPlanes[2] );
	fltx4 mp3 = Dot4SIMD( vCenter4, pPlanes[3] );
	fltx4 mp4 = Dot4SIMD( vCenter4, pPlanes[4] );
	fltx4 mp5 = Dot4SIMD( vCenter4, pPlanes[5] );

	fltx4 np0 = Dot3SIMD( vDiagonal4, AbsSIMD( pPlanes[0] ) );
	fltx4 np1 = Dot3SIMD( vDiagonal4, AbsSIMD( pPlanes[1] ) );
	fltx4 np2 = Dot3SIMD( vDiagonal4, AbsSIMD( pPlanes[2] ) );
	fltx4 np3 = Dot3SIMD( vDiagonal4, AbsSIMD( pPlanes[3] ) );
	fltx4 np4 = Dot3SIMD( vDiagonal4, AbsSIMD( pPlanes[4] ) );
	fltx4 np5 = Dot3SIMD( vDiagonal4, AbsSIMD( pPlanes[5] ) );

	fltx4 s0 = SubSIMD( mp0, np0 );
	fltx4 s1 = SubSIMD( mp1, np1 );     
	fltx4 s2 = SubSIMD( mp2, np2 );     
	fltx4 s3 = SubSIMD( mp3, np3 );     
	fltx4 s4 = SubSIMD( mp4, np4 );     
	fltx4 s5 = SubSIMD( mp5, np5 );     

	fltx4 minS = MinSIMD( MinSIMD( MinSIMD( MinSIMD( MinSIMD( s0, s1 ), s2 ), s3 ), s4 ), s5 );

	if ( IsAnyNegative( minS ) )
	{
		return false;
	}

	// completely inside
	return true;
}

// Returns true if the AABB either touches or is completely within a convex volume defined by X planes.
// Same basic approach as above.
// TODO: Replace this a function similar to CFrustum::CheckBoxInline().
static inline bool AABBTouchesOrInsideVolume( const fltx4 *pPlanes, uint nNumPlanes, FLTX4 vCenter4, FLTX4 vDiagonal4 )
{
	fltx4 minA = Four_Ones;
	for ( uint i = 0; i < nNumPlanes; ++i )
	{
		fltx4 np = Dot3SIMD( vDiagonal4, AbsSIMD( pPlanes[i] ) );
		fltx4 mp = Dot4SIMD( vCenter4, pPlanes[i] );
		fltx4 a = AddSIMD( np, mp );     
		minA = MinSIMD( minA, a );
	}
	if ( IsAnyNegative( minA ) )
	{
		return false;
	}
	return true;
}

bool AABBTouches( const fourplanes_t *planes, const fltx4 &fl4Center, const fltx4 &fl4Extents )
{
	fltx4 centerx = SplatXSIMD(fl4Center);
	fltx4 centery = SplatYSIMD(fl4Center);
	fltx4 centerz = SplatZSIMD(fl4Center);
	fltx4 extx = SplatXSIMD(fl4Extents);
	fltx4 exty = SplatYSIMD(fl4Extents);
	fltx4 extz = SplatZSIMD(fl4Extents);

	// compute the dot product of the normal and the farthest corner
	for ( int i = 0; i < 2; i++ )
	{
		fltx4 xTotalBack = AddSIMD( MulSIMD( planes[i].nX, centerx ), MulSIMD(planes[i].nXAbs, extx ) );
		fltx4 yTotalBack = AddSIMD( MulSIMD( planes[i].nY, centery ), MulSIMD(planes[i].nYAbs, exty ) );
		fltx4 zTotalBack = AddSIMD( MulSIMD( planes[i].nZ, centerz ), MulSIMD(planes[i].nZAbs, extz ) );
		fltx4 dotBack = AddSIMD( xTotalBack, AddSIMD(yTotalBack, zTotalBack) );
		// if plane of the farthest corner is behind the plane, then the box is completely outside this plane
		if  ( IsVector4LessThan( dotBack, planes[i].dist ) )
			return false;
	}
	return true;
}

bool CVolumeCuller::CheckBox( const VectorAligned &mins, const VectorAligned &maxs ) const
{
	m_Stats.m_nTotalAABB++;
	
	if ( m_bCullSmallObjects )
	{
		VectorAligned diag( maxs - mins );
		// Not really box volume - hacked so one function is useful on zero thickness boxes too.
		float flVol = ( diag.x * diag.x ) + ( diag.y * diag.y ) + ( diag.z * diag.z );
		if ( flVol < m_flSmallObjectCullVolumeThreshold )
			return false;
	}

	fltx4 vMins4 = LoadAlignedSIMD( &mins.x );
	fltx4 vMaxs4 = LoadAlignedSIMD( &maxs.x );

	// Converts from 3D interval to center/diagonal form.
	fltx4 vCenter4 = MulSIMD( AddSIMD( vMaxs4, vMins4 ), Four_PointFives );
	fltx4 vDiagonal4 = SubSIMD( vMaxs4, vCenter4 );  

	// Ensure vCenter.w is 1.0f.
	vCenter4 = SetWSIMD( vCenter4, Four_Ones );

	if ( m_bHasBaseFrustum )
	{
		if ( !AABBTouches( m_baseplanes, vCenter4, vDiagonal4 ) )
			return false;
	}   

	if ( m_bHasExclusionFrustum )
	{
		if ( AABBInsideFrustum( m_ExclusionFrustumPlanes, vCenter4, vDiagonal4 ) )
			return false;
	}   

	if ( m_nNumInclusionVolumePlanes )
	{
		if ( !AABBTouchesOrInsideVolume( m_InclusionVolumePlanes, m_nNumInclusionVolumePlanes, vCenter4, vDiagonal4 ) )
			return false;
	}
	
	m_Stats.m_nTotalAABBPassed++;

	return true;
}

bool CVolumeCuller::CheckBox( const Vector &mins, const Vector &maxs ) const
{
	m_Stats.m_nTotalAABB++;

	if ( m_bCullSmallObjects )
	{
		Vector diag( maxs - mins );
		// Not really box volume - hacked so one function is useful on zero thickness boxes too.
		float flVol = ( diag.x * diag.x ) + ( diag.y * diag.y ) + ( diag.z * diag.z );
		if ( flVol < m_flSmallObjectCullVolumeThreshold )
			return false;
	}

	fltx4 vMins4 = LoadUnalignedSIMD( &mins.x );
	fltx4 vMaxs4 = LoadUnalignedSIMD( &maxs.x );

	// Converts from 3D interval to center/diagonal form.
	fltx4 vCenter4 = MulSIMD( AddSIMD( vMaxs4, vMins4 ), Four_PointFives );
	fltx4 vDiagonal4 = SubSIMD( vMaxs4, vCenter4 );  

	// Ensure vCenter.w is 1.0f.
	vCenter4 = SetWSIMD( vCenter4, Four_Ones );

	if ( m_bHasBaseFrustum )
	{
		if ( !AABBTouches( m_baseplanes, vCenter4, vDiagonal4 ) )
			return false;
	}   

	if ( m_bHasExclusionFrustum )
	{
		if ( AABBInsideFrustum( m_ExclusionFrustumPlanes, vCenter4, vDiagonal4 ) )
			return false;
	}   

	if ( m_nNumInclusionVolumePlanes )
	{
		if ( !AABBTouchesOrInsideVolume( m_InclusionVolumePlanes, m_nNumInclusionVolumePlanes, vCenter4, vDiagonal4 ) )
			return false;
	}

	m_Stats.m_nTotalAABBPassed++;

	return true;
}

bool CVolumeCuller::CheckBoxCenterHalfDiagonal( const VectorAligned &center, const VectorAligned &halfDiagonal ) const
{
	m_Stats.m_nTotalCenterHalfDiagonal++;

	fltx4 vCenter4 = LoadAlignedSIMD( &center.x );
	fltx4 vDiagonal4 = LoadAlignedSIMD( &halfDiagonal.x );
		
	// Ensure vCenter.w is 1.0f.
	vCenter4 = SetWSIMD( vCenter4, Four_Ones );

	if ( m_bHasBaseFrustum )
	{
		if ( !AABBTouches( m_baseplanes, vCenter4, vDiagonal4 ) )
			return false;
	}   

	if ( m_bHasExclusionFrustum )
	{
		if ( AABBInsideFrustum( m_ExclusionFrustumPlanes, vCenter4, vDiagonal4 ) )
			return false;
	}   

	if ( m_nNumInclusionVolumePlanes )
	{
		if ( !AABBTouchesOrInsideVolume( m_InclusionVolumePlanes, m_nNumInclusionVolumePlanes, vCenter4, vDiagonal4 ) )
			return false;
	}

	m_Stats.m_nTotalCenterHalfDiagonalPassed++;

	return true;
}

void CVolumeCuller::SetExclusionFrustumPlanes( const VPlane *pPlanes )
{
	COMPILE_TIME_ASSERT( sizeof( VPlane ) == sizeof( fltx4 ) );

	if ( !pPlanes )
	{
		m_bHasExclusionFrustum = false;
	}
	else
	{
		for ( int i = 0; i < cNumExclusionFrustumPlanes; ++i )
		{
			// Convert VPlane to plane equation form.
			reinterpret_cast< Vector4D & >( m_ExclusionFrustumPlanes[i] ).Init( pPlanes[i].m_Normal.x, pPlanes[i].m_Normal.y, pPlanes[i].m_Normal.z, -pPlanes[i].m_Dist );
		}
		m_bHasExclusionFrustum = true;
	}
}

void CVolumeCuller::SetBaseFrustumPlanes( const VPlane *pPlanes )
{
	COMPILE_TIME_ASSERT( sizeof( VPlane ) == sizeof( fltx4 ) );

	if ( !pPlanes )
	{
		m_bHasBaseFrustum = false;
	}
	else
	{
		m_baseplanes[0].Set4Planes( pPlanes );
		m_baseplanes[1].Set2Planes( pPlanes + 4 );
		m_bHasBaseFrustum = true;
	}
}

void CVolumeCuller::GetBaseFrustumPlanes( VPlane *pBasePlanes ) const
{
	m_baseplanes[0].Get4Planes( pBasePlanes );
	m_baseplanes[1].Get2Planes( pBasePlanes + 4 );
}

void CVolumeCuller::SetInclusionVolumePlanes( const VPlane *pPlanes, uint nNumPlanes )
{
	Assert( nNumPlanes <= cMaxInclusionVolumePlanes );
	nNumPlanes = MIN( nNumPlanes, cMaxInclusionVolumePlanes );
	m_nNumInclusionVolumePlanes = nNumPlanes;

	for ( uint i = 0; i < nNumPlanes; ++i )
	{
		// Convert VPlane to plane equation form.
		reinterpret_cast< Vector4D & >( m_InclusionVolumePlanes[i] ).Init( pPlanes[i].m_Normal.x, pPlanes[i].m_Normal.y, pPlanes[i].m_Normal.z, -pPlanes[i].m_Dist );
	}
}
