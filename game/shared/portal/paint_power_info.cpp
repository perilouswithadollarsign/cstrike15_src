//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the PaintPowerInfo structure for storing information about
//			the paint powers used.
//
//=============================================================================//

#include "cbase.h"
#include "debugoverlay_shared.h"
#include "paint_power_info.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//BEGIN_SEND_TABLE_NOBASE( PaintPowerInfo_t, DT_PaintPowerInfo_t )
//	SendPropEHandle( SENDINFO( m_HandleToOther ) ),
//	SendPropVector( SENDINFO( m_SurfaceNormal ) ),
//END_SEND_TABLE()

#if defined( CLIENT_DLL )
BEGIN_PREDICTION_DATA_NO_BASE( PaintPowerInfo_t )

	DEFINE_PRED_FIELD( m_HandleToOther, FIELD_EHANDLE, 0 ),
	DEFINE_PRED_FIELD( m_SurfaceNormal, FIELD_VECTOR, 0 ),
	DEFINE_PRED_FIELD( m_ContactPoint, FIELD_VECTOR, 0 ),
	DEFINE_PRED_FIELD( m_State, FIELD_INTEGER, 0 ),
	DEFINE_PRED_FIELD( m_PaintPowerType, FIELD_INTEGER, 0 ),
	DEFINE_PRED_FIELD( m_IsOnThinSurface, FIELD_BOOLEAN, 0 )

END_PREDICTION_DATA()
#endif

IMPLEMENT_NULL_SIMPLE_DATADESC( PaintPowerInfo_t );

const Vector DEFAULT_PAINT_SURFACE_NORMAL = Vector(0.0f, 0.0f, 1.0f);	// Flat ground

PaintPowerInfo_t::PaintPowerInfo_t()
	: m_SurfaceNormal( DEFAULT_PAINT_SURFACE_NORMAL ),
	  m_ContactPoint( 0.0f, 0.0f, 0.0f ),
	  m_PaintPowerType( NO_POWER ),
	  m_HandleToOther( 0 ),
	  m_State( INACTIVE_PAINT_POWER ),
	  m_IsOnThinSurface( false )
{}


PaintPowerInfo_t::PaintPowerInfo_t( const Vector &normal,
									const Vector &contactPt,
									CBaseEntity* pOther,
									PaintPowerType power,
									bool isOnThinSurface )
	: m_SurfaceNormal( normal ),
	  m_ContactPoint( contactPt ),
	  m_PaintPowerType( power ),
	  m_State( INACTIVE_PAINT_POWER ),
	  m_IsOnThinSurface( isOnThinSurface )
{

	m_HandleToOther.Set( pOther );
}

#define PAINT_POWER_CPP_INLINE


PAINT_POWER_CPP_INLINE bool AreSamePower( const PaintPowerInfo_t& powerA, const PaintPowerInfo_t& powerB )
{
	return powerA.m_PaintPowerType == powerB.m_PaintPowerType &&
		   powerA.m_HandleToOther == powerB.m_HandleToOther &&
		   AlmostEqual( DotProduct( powerA.m_SurfaceNormal, powerB.m_SurfaceNormal ), 1.f );	
}


PAINT_POWER_CPP_INLINE bool AreDifferentPowers( const PaintPowerInfo_t& powerA, const PaintPowerInfo_t& powerB )
{
	return !AreSamePower( powerA, powerB );
}


PAINT_POWER_CPP_INLINE bool IsSpeedPower( const PaintPowerInfo_t& power )
{
	return power.m_PaintPowerType == SPEED_POWER;
}


PAINT_POWER_CPP_INLINE bool IsBouncePower( const PaintPowerInfo_t& power )
{
	return power.m_PaintPowerType == BOUNCE_POWER;
}


PAINT_POWER_CPP_INLINE bool IsReflectPower( const PaintPowerInfo_t& power )
{
	return power.m_PaintPowerType == REFLECT_POWER;
}


PAINT_POWER_CPP_INLINE bool IsPortalPower( const PaintPowerInfo_t& power )
{
	return power.m_PaintPowerType == PORTAL_POWER;
}


PAINT_POWER_CPP_INLINE bool IsNoPower( const PaintPowerInfo_t& power )
{
	return power.m_PaintPowerType == NO_POWER;
}


PAINT_POWER_CPP_INLINE bool IsActivatingPower( const PaintPowerInfo_t& power )
{
	return power.m_State == ACTIVATING_PAINT_POWER;
}


PAINT_POWER_CPP_INLINE bool IsActivePower( const PaintPowerInfo_t& power )
{
	return power.m_State == ACTIVE_PAINT_POWER;
}


PAINT_POWER_CPP_INLINE bool IsDeactivatingPower( const PaintPowerInfo_t& power )
{
	return power.m_State == DEACTIVATING_PAINT_POWER;
}


PAINT_POWER_CPP_INLINE bool IsInactivePower( const PaintPowerInfo_t& power )
{
	return power.m_State == INACTIVE_PAINT_POWER;
}


char const *const PowerTypeToString( const PaintPowerInfo_t& powerInfo )
{
	return PowerTypeToString( powerInfo.m_PaintPowerType );
}


char const *const PowerTypeToString( PaintPowerType type )
{
	switch( type )
	{
		case BOUNCE_POWER:
			return "Bounce";
			
		case SPEED_POWER:
			return "Speed";

		case REFLECT_POWER:
			return "Speed";// FIXME: Bring this back for DLC2 "Reflect";

		case PORTAL_POWER:
			return "Portal";

		case NO_POWER:
			return "No"; // Yes

		default:
			return "Invalid power or hasn't been added to PowerTypeToString() in paint_power_info.cpp";
	}
}


char const *const PowerStateToString( const PaintPowerInfo_t& powerInfo )
{
	return PowerStateToString( powerInfo.m_State );
}


char const *const PowerStateToString( PaintPowerState state )
{
	switch( state )
	{
		case ACTIVATING_PAINT_POWER:
			return "Activating";

		case ACTIVE_PAINT_POWER:
			return "Active";

		case DEACTIVATING_PAINT_POWER:
			return "Deactivating";

		case INACTIVE_PAINT_POWER:
			return "Inactive";

		default:
			return "Invalid state or hasn't been added to PowerStateToString() in paint_power_info.cpp";
	}
}


void PrintPowerInfoDebugMsg( const PaintPowerInfo_t& powerInfo )
{
	DevMsg( "Contact Point:\t(%f, %f, %f)\n", XYZ(powerInfo.m_ContactPoint) );
	DevMsg( "Surface Normal:\t(%f, %f, %f)\n", XYZ(powerInfo.m_SurfaceNormal) );
	DevMsg( "Paint Power: %s Power\n", PowerTypeToString(powerInfo.m_PaintPowerType) );

	// Non-const because CBaseEntity::GetClassname() is non-const because someone is lame.
	// Rebuilding every project in main to fix a const-correctness mistake is incredibly annoying.
	CBaseEntity* pOther;
	pOther = powerInfo.m_HandleToOther.Get() != NULL ? EntityFromEntityHandle( powerInfo.m_HandleToOther.Get() ) : NULL;
	DevMsg( "Other Class: %s\n", pOther != NULL ? pOther->GetClassname() : "Null" );
	DevMsg( "State: %s\n", PowerStateToString( powerInfo.m_State ) );
}


void DrawPaintPowerContactInfo( const PaintPowerInfo_t& powerInfo, const Color& color, float duration, bool noDepthTest )
{
	NDebugOverlay::Sphere( powerInfo.m_ContactPoint, 5.0f, color.r(), color.g(), color.b(), noDepthTest, duration );
	NDebugOverlay::Line( powerInfo.m_ContactPoint, powerInfo.m_ContactPoint + 20.0f * powerInfo.m_SurfaceNormal, color.r(), color.g(), color.b(), noDepthTest, duration );
}


int DescendingPaintPriorityCompare( const PaintPowerInfo_t* a, const PaintPowerInfo_t* b )
{
	return a->m_PaintPowerType - b->m_PaintPowerType;
}

int AscendingPaintPriorityCompare( const PaintPowerInfo_t* a, const PaintPowerInfo_t* b )
{
	return b->m_PaintPowerType - a->m_PaintPowerType;
}
