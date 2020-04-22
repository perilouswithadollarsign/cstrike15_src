//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef R_AREAPORTAL_H
#define R_AREAPORTAL_H
#ifdef _WIN32
#pragma once
#endif


#include "utlvector.h"
#include "gl_model_private.h"


class Frustum_t;


// Used to clip area portals. The coordinates here are in normalized
// view space (-1,-1) - (1,1)
// BUGBUG: NOTE!!!: These are left->right, and bottom->top, so a full rect is left=-1,top=1,right=1,bottom=-1
class CPortalRect
{
public:
	float	left, top, right, bottom;
};


// ---------------------------------------------------------------------------- //
// Functions.
// ---------------------------------------------------------------------------- //

// Copies GetBaseLocalClient().pAreaBits, finds the area the viewer is in, and figures out what
// other areas are visible. The new bits are placed in g_RenderAreaBits.
void R_SetupAreaBits( int iForceViewLeaf = -1, const VisOverrideData_t* pVisData = NULL, float *pWaterReflectionHeight = NULL );

// Ask if an area is visible to the renderer.
unsigned char R_IsAreaVisible( int area );

void R_Areaportal_LevelInit();
void R_Areaportal_LevelShutdown();

inline bool CullNodeSIMD( const Frustum_t &frustum, mnode_t *pNode )
{
	fltx4 center4 = LoadAlignedSIMD( pNode->m_vecCenter );
	fltx4 centerx = SplatXSIMD(center4);
	fltx4 centery = SplatYSIMD(center4);
	fltx4 centerz = SplatZSIMD(center4);
	fltx4 extents4 = LoadAlignedSIMD( pNode->m_vecHalfDiagonal );
	fltx4 extx = SplatXSIMD(extents4);
	fltx4 exty = SplatYSIMD(extents4);
	fltx4 extz = SplatZSIMD(extents4);

	// compute the dot product of the normal and the farthest corner
	for ( int i = 0; i < 2; i++ )
	{
		fltx4 xTotalBack = AddSIMD( MulSIMD( frustum.planes[i].nX, centerx ), MulSIMD(frustum.planes[i].nXAbs, extx ) );
		fltx4 yTotalBack = AddSIMD( MulSIMD( frustum.planes[i].nY, centery ), MulSIMD(frustum.planes[i].nYAbs, exty ) );
		fltx4 zTotalBack = AddSIMD( MulSIMD( frustum.planes[i].nZ, centerz ), MulSIMD(frustum.planes[i].nZAbs, extz ) );
		fltx4 dotBack = AddSIMD( xTotalBack, AddSIMD(yTotalBack, zTotalBack) );
		// if plane of the farthest corner is behind the plane, then the box is completely outside this plane
#if defined( _X360 )
		if  ( !XMVector3GreaterOrEqual( dotBack, frustum.planes[i].dist ) )
			return true;
#elif defined( _PS3 )
		if ( vec_any_lt( dotBack, frustum.planes[i].dist ) )
			return true;
#else
		fltx4 isOut = ( fltx4 ) CmpLtSIMD( dotBack, frustum.planes[i].dist );
		if ( IsAnyTrue( isOut ) )
			return true;
#endif
	}
	return false; 
}

// Decides if the node can be seen through the area portals (ie: if you're
// looking out a window with an areaportal in it, this will clip out the
// stuff to the sides).
bool R_CullNode( mnode_t *pNode );
const Frustum_t* GetAreaFrustum( int area );
// get a list of all area frustums
int GetAllAreaFrustums( Frustum_t **pFrustumList, int listMax );
bool R_ShouldUseAreaFrustum( int area );

// ---------------------------------------------------------------------------- //
// Globals.
// ---------------------------------------------------------------------------- //

extern ConVar r_DrawPortals;

// Used when r_DrawPortals is on. Draws the screen space rects for each portal.
extern CUtlVector<CPortalRect> g_PortalRects;



// ---------------------------------------------------------------------------- //
// Inlines.
// ---------------------------------------------------------------------------- //

inline unsigned char R_IsAreaVisible( int area )
{
	extern unsigned char g_RenderAreaBits[32];
	return g_RenderAreaBits[area>>3] & GetBitForBitnum(area&7);
}

#endif // R_AREAPORTAL_H
