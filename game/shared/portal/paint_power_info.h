//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Declares the PaintPowerInfo structure for storing information about
//			the paint powers used.
//
//=============================================================================//

#ifndef PAINT_POWER_INFO_H
#define PAINT_POWER_INFO_H

#include "paint_color_manager.h"

enum PaintPowerState
{
	ACTIVATING_PAINT_POWER,
	ACTIVE_PAINT_POWER,
	DEACTIVATING_PAINT_POWER,
	INACTIVE_PAINT_POWER
};


//=============================================================================
// struct PaintPowerInfo
// Purpose: Holds the necessary information for using a paint power.
// Note: This will change quite a bit once the paint tech is implemented.
//=============================================================================
struct PaintPowerInfo_t
{
	DECLARE_SIMPLE_DATADESC();
	DECLARE_PREDICTABLE();
	DECLARE_CLASS_NOBASE( PaintPowerInfo_t );
	DECLARE_EMBEDDED_NETWORKVAR();

	Vector m_SurfaceNormal;				// Normal to the surface the paint is on
	Vector m_ContactPoint;				// Contact point on the surface
	PaintPowerType m_PaintPowerType;	// Paint power at this point on the surface
	CBaseHandle m_HandleToOther;		// Handle to the other entity
	PaintPowerState m_State;			// Current state of the power
	bool m_IsOnThinSurface;				// The power is on a thin surface

	PaintPowerInfo_t();
	PaintPowerInfo_t( const Vector& normal,
					  const Vector& contactPt,
					  CBaseEntity* pOther,
					  PaintPowerType power = NO_POWER,
					  bool isOnThinSurface = false );
};


//=============================================================================
// Helper Functions and Functors
//=============================================================================
extern bool AreSamePower( const PaintPowerInfo_t& powerA, const PaintPowerInfo_t& powerB );
extern bool AreDifferentPowers( const PaintPowerInfo_t& powerA, const PaintPowerInfo_t& powerB );
extern bool IsSpeedPower( const PaintPowerInfo_t& power );
extern bool IsBouncePower( const PaintPowerInfo_t& power );
extern bool IsStickPower( const PaintPowerInfo_t& power );
extern bool IsNoPower( const PaintPowerInfo_t& power );
extern bool IsActivatingPower( const PaintPowerInfo_t& power );
extern bool IsActivePower( const PaintPowerInfo_t& power );
extern bool IsDeactivatingPower( const PaintPowerInfo_t& power );
extern bool IsInactivePower( const PaintPowerInfo_t& power );
char const *const PowerTypeToString( const PaintPowerInfo_t& powerInfo );
char const *const PowerTypeToString( PaintPowerType type );
char const *const PowerStateToString( const PaintPowerInfo_t& powerInfo );
char const *const PowerStateToString( PaintPowerState state );
void PrintPowerInfoDebugMsg( const PaintPowerInfo_t& powerInfo );
void DrawPaintPowerContactInfo( const PaintPowerInfo_t& powerInfo, const Color& color, float duration, bool noDepthTest );

int DescendingPaintPriorityCompare( const PaintPowerInfo_t* a, const PaintPowerInfo_t* b );
int AscendingPaintPriorityCompare( const PaintPowerInfo_t* a, const PaintPowerInfo_t* b );

#endif // ifndef PAINT_POWER_INFO_H
