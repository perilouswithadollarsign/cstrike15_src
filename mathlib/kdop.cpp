//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "mathlib/ssemath.h"
#include "mathlib/ssequaternion.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

// get the kdop vectors for k=32
#include "dopvectors.h"


void KDop32_t::AddPointSet( Vector const *pPoints, int nPnts )
{
	for( int i = 0; i < nPnts; i++ )
	{
		fltx4 fl4PntX = ReplicateX4( pPoints->x );
		fltx4 fl4PntY = ReplicateX4( pPoints->y );
		fltx4 fl4PntZ = ReplicateX4( pPoints->z );
		
		for( int c = 0; c < 4; c++ )
		{
			fltx4 fl4Dot = AddSIMD( AddSIMD( MulSIMD( fl4PntX, g_KDop32XDirs[c] ), MulSIMD( fl4PntY, g_KDop32YDirs[c] ) ),
									MulSIMD( fl4PntZ, g_KDop32ZDirs[c] ) );
			
			m_Mins[c] = MinSIMD( fl4Dot, m_Mins[c] );
			m_Maxes[c] = MaxSIMD( fl4Dot, m_Maxes[c] );
		}
		pPoints++;
	}
}


void KDop32_t::CreateFromPointSet( Vector const *pPoints, int nPnts )
{
	Init();
	AddPointSet( pPoints, nPnts );
}
