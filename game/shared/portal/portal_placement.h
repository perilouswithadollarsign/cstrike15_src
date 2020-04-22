//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef PORTAL_PLACEMENT_H
#define PORTAL_PLACEMENT_H
#ifdef _WIN32
#pragma once
#endif

struct CPortalCornerFitData;
#include "portal_shareddefs.h"
#include "CegClientWrapper.h"

#if defined( CLIENT_DLL )
class C_Prop_Portal;
typedef C_Prop_Portal CProp_Portal;
#else
class CProp_Portal;
#endif

bool FitPortalOnSurface( const CProp_Portal *pIgnorePortal, Vector &vOrigin, const Vector &vForward, const Vector &vRight, 
						 const Vector &vTopEdge, const Vector &vBottomEdge, const Vector &vRightEdge, const Vector &vLeftEdge, 
						 PortalPlacedBy_t ePlacedBy, ITraceFilter *pTraceFilterPortalShot, 
						 float fHalfWidth, float fHalfHeight,
						 int iRecursions = 0, const CPortalCornerFitData *pPortalCornerFitData = 0, const int *p_piIntersectionIndex = 0, const int *piIntersectionCount = 0 );
bool IsPortalIntersectingNoPortalVolume( const Vector &vOrigin, const QAngle &qAngles, const Vector &vForward, float fHalfWidth, float fHalfHeight );
PortalPlacementResult_t IsPortalOverlappingOtherPortals( const CProp_Portal *pIgnorePortal, const Vector &vOrigin, const QAngle &qAngles, float fHalfWidth, float fHalfHeight, bool bFizzleAll = false, bool bFizzlePartnerPortals = false );
PortalPlacementResult_t VerifyPortalPlacement( const CProp_Portal *pIgnorePortal, Vector &vOrigin, QAngle &qAngles, float fHalfWidth, float fHalfHeight, PortalPlacedBy_t ePlacedBy );
PortalPlacementResult_t VerifyPortalPlacementAndFizzleBlockingPortals( const CProp_Portal *pIgnorePortal, Vector &vOrigin, QAngle &qAngles, float fHalfWidth, float fHalfHeight, PortalPlacedBy_t ePlacedBy );
bool PortalPlacementSucceeded( PortalPlacementResult_t eResult );
bool IsNoPortalMaterial( const trace_t &tr );
PortalSurfaceType_t PortalSurfaceType( const trace_t& tr );
bool IsOnPortalPaint( const trace_t &tr );
void InitSurfNoPortalFlag();
void InitPortalPaintPowerValue();

#endif // PORTAL_PLACEMENT_H
