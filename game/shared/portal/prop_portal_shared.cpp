//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "prop_portal_shared.h"
#include "portal_shareddefs.h"
#include "portal_placement.h"
#include "weapon_portalgun_shared.h"


#if defined( GAME_DLL )
#include "baseprojector.h"
#else
#include "c_baseprojectedentity.h"
typedef C_BaseProjectedEntity CBaseProjectedEntity;
#endif

CUtlVector<CProp_Portal *> CProp_Portal_Shared::AllPortals;

extern ConVar sv_portal_placement_never_fail;

void CProp_Portal::PlacePortal( const Vector &vOrigin, const QAngle &qAngles, PortalPlacementResult_t eResult, bool bDelay /*= false*/ )
{
	Vector vOldOrigin = GetLocalOrigin();
	QAngle qOldAngles = GetLocalAngles();

	Vector vNewOrigin = vOrigin;
	QAngle qNewAngles = qAngles;

#if !defined( PORTAL2 )
	UTIL_TestForOrientationVolumes( qNewAngles, vNewOrigin, this );
#endif // PORTAL2

	if ( PortalPlacementSucceeded( eResult ) == false && sv_portal_placement_never_fail.GetBool() == false )
	{
		// Prepare fizzle
		m_vDelayedPosition = vOrigin;
		m_qDelayedAngles = qAngles;

		// Translate the fizzle type
		// FIXME: This can go away, we don't care about the fizzle type anymore -- jdw
		switch( eResult )
		{
		case PORTAL_PLACEMENT_CANT_FIT:
			m_iDelayedFailure = PORTAL_FIZZLE_CANT_FIT;
			break;

		case PORTAL_PLACEMENT_OVERLAP_LINKED:
			m_iDelayedFailure = PORTAL_FIZZLE_OVERLAPPED_LINKED;
			break;

		case PORTAL_PLACEMENT_INVALID_VOLUME:
			m_iDelayedFailure = PORTAL_FIZZLE_BAD_VOLUME;
			break;

		case PORTAL_PLACEMENT_INVALID_SURFACE:
			m_iDelayedFailure = PORTAL_FIZZLE_BAD_SURFACE;
			break;

		case PORTAL_PLACEMENT_CLEANSER:
			m_iDelayedFailure = PORTAL_FIZZLE_CLEANSER;
			break;

		default:
		case PORTAL_PLACEMENT_PASSTHROUGH_SURFACE:
			m_iDelayedFailure = PORTAL_FIZZLE_NONE;
			break;
		}

		return;
	}

	m_vDelayedPosition = vNewOrigin;
	m_qDelayedAngles = qNewAngles;
	m_iDelayedFailure = PORTAL_FIZZLE_SUCCESS;

	if ( bDelay == false )
	{
		NewLocation( vNewOrigin, qNewAngles );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Runs when a fired portal shot reaches it's destination wall. Detects current placement valididty state.
//-----------------------------------------------------------------------------
void CProp_Portal::DelayedPlacementThink( void )
{
	Vector vOldOrigin = m_ptOrigin; //GetLocalOrigin();
	QAngle qOldAngles = m_qAbsAngle; //GetLocalAngles();

	Vector vForward;
	AngleVectors( m_qDelayedAngles, &vForward );

	// Check if something made the spot invalid mid flight
	// Bad surface and near fizzle effects take priority
	if ( m_iDelayedFailure != PORTAL_FIZZLE_BAD_SURFACE && m_iDelayedFailure != PORTAL_FIZZLE_NEAR_BLUE && m_iDelayedFailure != PORTAL_FIZZLE_NEAR_RED )
	{
		if ( IsPortalOverlappingOtherPortals( this, m_vDelayedPosition, m_qDelayedAngles, GetHalfWidth(), GetHalfHeight() ) )
		{
			m_iDelayedFailure = PORTAL_FIZZLE_OVERLAPPED_LINKED;
		}
		else if ( IsPortalIntersectingNoPortalVolume( m_vDelayedPosition, m_qDelayedAngles, vForward, GetHalfWidth(), GetHalfHeight() ) )
		{
#if defined GAME_DLL
			RANDOM_CEG_TEST_SECRET_PERIOD( 29, 83 )
#endif
			m_iDelayedFailure = PORTAL_FIZZLE_BAD_VOLUME;
		}
	}

	if ( sv_portal_placement_never_fail.GetBool() )
	{
		m_iDelayedFailure = PORTAL_FIZZLE_SUCCESS;
	}

	DoFizzleEffect( m_iDelayedFailure );

	if ( m_iDelayedFailure != PORTAL_FIZZLE_SUCCESS )
	{
		// It didn't successfully place
		return;
	}

	// Do effects at old location if it was active
	if ( GetOldActiveState() )
	{
		DoFizzleEffect( PORTAL_FIZZLE_CLOSE, false );
	}

#if defined( GAME_DLL )
	CWeaponPortalgun *pPortalGun = dynamic_cast<CWeaponPortalgun*>( m_hPlacedBy.Get() );

	if( pPortalGun )
	{
		CPortal_Player *pFiringPlayer = dynamic_cast<CPortal_Player *>( pPortalGun->GetOwner() );
		if( pFiringPlayer )
		{
			pFiringPlayer->IncrementPortalsPlaced( IsPortal2() );

			// Placement successful, fire the output
			m_OnPlacedSuccessfully.FireOutput( pPortalGun, this );

		}
	}
#endif

	// Move to new location
	NewLocation( m_vDelayedPosition, m_qDelayedAngles );

#if defined( GAME_DLL )
	// Test for our surface moving out from behind us
	SetContextThink( &CProp_Portal::TestRestingSurfaceThink, gpGlobals->curtime + 0.1f, s_szTestRestingSurfaceThinkContext );
	
	CBaseProjector::TestAllForProjectionChanges();
#else
	CBaseProjectedEntity::TestAllForProjectionChanges();
#endif
}

// default to sane-looking but incorrect portal height for CEG - Updated in constructor
bool CProp_Portal::ms_DefaultPortalSizeInitialized = false; // for CEG protection
float CProp_Portal::ms_DefaultPortalHalfWidth = DEFAULT_PORTAL_HALF_WIDTH;
float CProp_Portal::ms_DefaultPortalHalfHeight = 0.25 * DEFAULT_PORTAL_HALF_HEIGHT;

//NULL portal will return default width/height
void CProp_Portal::GetPortalSize( float &fHalfWidth, float &fHalfHeight, CProp_Portal *pPortal )
{
	if( pPortal )
	{
		fHalfWidth = pPortal->GetHalfWidth();
		fHalfHeight = pPortal->GetHalfHeight();
	}
	else
	{
		fHalfWidth = ms_DefaultPortalHalfWidth;
		fHalfHeight = ms_DefaultPortalHalfHeight;
	}
}



void CProp_Portal::SetFiredByPlayer( CBasePlayer *pPlayer )
{
	m_hFiredByPlayer = pPlayer;
	if( pPlayer )
	{
		SetPlayerSimulated( pPlayer );
	}
	else
	{
		UnsetPlayerSimulated();
	}
}

extern ConVar sv_gravity;

float CProp_Portal::GetMinimumExitSpeed( bool bPlayer, bool bEntranceOnFloor, bool bExitOnFloor, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity )
{
	if( bExitOnFloor )
	{
		if( bPlayer )
		{
			return 300.0f;
		}
		else
		{
			return bEntranceOnFloor ? 225.0f : 50.0f;
		}
	}
	else if( bPlayer )
	{
		//bExitOnFloor means the portal is facing almost entirely up, just because it's false doesn't mean the portal isn't facing significantly up
		//We also need to solve the case where the player's AABB rotates in such a way that we pull the ground out from under them
		if( m_vForward.z > 0.5f ) //forward facing up by at least 30 degrees
		{
			float fGravity = GetGravity();
			if ( fGravity != 0.0f )
			{
				fGravity *= sv_gravity.GetFloat();
			}
			else
			{
				fGravity = sv_gravity.GetFloat();
			}

			if( fGravity != 0.0f )
			{
				//Assuming our current velocity is zero. What's the minimum portal-forward velocity to perch the player on the bottom edge of the portal?
				Vector vPerchPoint = m_ptOrigin - (m_vUp * GetHalfHeight()); //a point along the bottom edge of the portal, horizontally centered
				Vector vPlayerExtents = (((CPortal_Player *)pEntity)->GetHullMaxs() - ((CPortal_Player *)pEntity)->GetHullMins()) * 0.5f;
				//Vector vPlayerCenterToPerch = vPerchPoint - vEntityCenterAtExit;
				Vector vTestBBoxPoint = vEntityCenterAtExit;
				//vTestBBoxPoint.x += Sign( vPlayerCenterToPerch.x ) * vPlayerExtents.x;
				//vTestBBoxPoint.y += Sign( vPlayerCenterToPerch.y ) * vPlayerExtents.y;
				vTestBBoxPoint.z -= vPlayerExtents.z;

				
				Vector vTestToPerch = vPerchPoint - vTestBBoxPoint;
				vTestToPerch -= vTestToPerch.Dot( m_vRight ) * m_vRight; //Project test vector onto horizontal center, so all x/y dist to perch point is actually distance to perch line
				float fHorzTestToPerch = vTestToPerch.Length2D();
				float fHorzVelocityComponent = m_vForward.Length2D(); //the portion of our velocity axis that will move us horizontally toward the perch
			

				float fRoot1, fRoot2;
				if( SolveQuadratic( (m_vForward.z * (-2.0f)) * ((fHorzTestToPerch * fHorzVelocityComponent) - (vTestToPerch.z*m_vForward.z)), 0, fHorzTestToPerch * fHorzTestToPerch * fGravity, fRoot1, fRoot2 ) )
				{
					float fMax = MAX( fRoot1, fRoot2 );
					if( fMax > 0.0f )
					{
						if( fMax > 300.0f ) //cap out at floor/floor minimum
							return 300.0f;
						else
							return fMax;
					}
				}
			}	
		}
	}

	return BaseClass::GetMinimumExitSpeed( bPlayer, bEntranceOnFloor, bExitOnFloor, vEntityCenterAtExit, pEntity );
}

float CProp_Portal::GetMaximumExitSpeed( bool bPlayer, bool bEntranceOnFloor, bool bExitOnFloor, const Vector &vEntityCenterAtExit, CBaseEntity *pEntity )
{
	return 1000.0f;
}

