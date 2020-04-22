//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: Declares the base class for paint power users that are props.
//
//===========================================================================//
#ifndef PROP_PAINT_POWER_USER_H
#define PROP_PAINT_POWER_USER_H

#include "vphysics/friction.h"
#include "vphysics/constraints.h"
#include "player_pickup.h"
#include "paintable_entity.h"

#ifndef CLIENT_DLL
#include "portal/weapon_physcannon.h"
#endif

#include "portal_util_shared.h"
#include "portal_base2d_shared.h"

#include "paint_power_user.h"
#include "stick_partner.h"

#include "material_index_data_ops_proxy.h"

char const* const PROP_PAINT_POWER_USER_DATA_CLASS_NAME = "PropPaintPowerUser";

char const* const UPDATE_PAINT_POWER_CONTEXT = "UpdatePaintPowers";

const float PROP_PAINT_POWER_USER_PICKUP_DROP_TIME = 0.5f;

//=============================================================================
// class PropPaintPowerUser
// Purpose: Base class for props which use paint powers.
//=============================================================================
template< typename BasePropType >
class PropPaintPowerUser : public PaintPowerUser< CPaintableEntity< BasePropType > > // Derive from PaintPowerUser but add CPaintableEntity.
{
	DECLARE_CLASS( PropPaintPowerUser< BasePropType >, PaintPowerUser< CPaintableEntity< BasePropType > > );
	DECLARE_DATADESC();
	static const datamap_t DataMapInit();

public:
	//-------------------------------------------------------------------------
	// Constructor/Virtual Destructor
	//-------------------------------------------------------------------------
	PropPaintPowerUser();
	virtual ~PropPaintPowerUser();

	//-------------------------------------------------------------------------
	// Prop Overrides
	//-------------------------------------------------------------------------
	virtual void Spawn();
	virtual void VPhysicsCollision( int index, gamevcollisionevent_t *pEvent );
	virtual void VPhysicsUpdate( IPhysicsObject *pPhysics );
	virtual void UpdatePaintPowersFromContacts();

	//-------------------------------------------------------------------------
	// Paintable Entity Overrides
	//-------------------------------------------------------------------------
	virtual void Paint( PaintPowerType type, const Vector& worldContactPt );

protected:
	int m_nOriginalMaterialIndex;									// Cached physics material index (it changes)
	int	m_PrePaintedPower;											// Power to start with on load

	typedef typename BaseClass::PaintPowerInfoVector BaseClass_PaintPowerInfoVector;

	virtual void ChooseActivePaintPowers( BaseClass_PaintPowerInfoVector& activePowers );

	static int GetSpeedMaterialIndex();

private:
	//-------------------------------------------------------------------------
	// Private Data
	//-------------------------------------------------------------------------
	bool m_bHeldByPlayer;
	float m_flPickedUpTime;

	//-------------------------------------------------------------------------
	// Paint Power Effects
	//-------------------------------------------------------------------------
	virtual PaintPowerState ActivateSpeedPower( PaintPowerInfo_t& powerInfo );
	virtual PaintPowerState UseSpeedPower( PaintPowerInfo_t& powerInfo );
	virtual PaintPowerState DeactivateSpeedPower( PaintPowerInfo_t& powerInfo );

	virtual PaintPowerState ActivateBouncePower( PaintPowerInfo_t& powerInfo );
	virtual PaintPowerState UseBouncePower( PaintPowerInfo_t& powerInfo );
	virtual PaintPowerState DeactivateBouncePower( PaintPowerInfo_t& powerInfo );
};


//=============================================================================
// PropPaintPowerUser Implementation
//=============================================================================

// OMFG HACK: Define the data description table. The current macros don't work with templatized classes.
// OMFG TODO: Write a generic macro to work with templatized classes.
template< typename BasePropType >
datamap_t PropPaintPowerUser<BasePropType>::m_DataMap = PropPaintPowerUser<BasePropType>::DataMapInit();

template< typename BasePropType >
datamap_t* PropPaintPowerUser<BasePropType>::GetDataDescMap()
{
	return &m_DataMap;
}


template< typename BasePropType >
datamap_t* PropPaintPowerUser<BasePropType>::GetBaseMap()
{
	datamap_t *pResult;
	DataMapAccess((BaseClass *)NULL, &pResult);
	return pResult;
}


template< typename BasePropType >
const datamap_t PropPaintPowerUser<BasePropType>::DataMapInit()
{
	typedef PropPaintPowerUser<BasePropType> classNameTypedef;
	static CDatadescGeneratedNameHolder nameHolder(PROP_PAINT_POWER_USER_DATA_CLASS_NAME);
	static typedescription_t dataDesc[] =
	{
		DEFINE_KEYFIELD( m_PrePaintedPower, FIELD_INTEGER, "PaintPower" ),
		DEFINE_CUSTOM_FIELD( m_nOriginalMaterialIndex, &GetMaterialIndexDataOpsProxy() )
	};

	datamap_t dataMap = { dataDesc, SIZE_OF_ARRAY(dataDesc), PROP_PAINT_POWER_USER_DATA_CLASS_NAME, PropPaintPowerUser<BasePropType>::GetBaseMap() };
	return dataMap;
}


template< typename BasePropType >
PropPaintPowerUser<BasePropType>::PropPaintPowerUser()
	: m_PrePaintedPower(NO_POWER),
	  m_flPickedUpTime( 0.0f ),
	  m_bHeldByPlayer( false )
{
}


template< typename BasePropType >
PropPaintPowerUser<BasePropType>::~PropPaintPowerUser()
{
}


template< typename BasePropType >
void PropPaintPowerUser<BasePropType>::Spawn()
{
	BaseClass::Spawn();

	// Store our material index
	IPhysicsObject* pPhysObject = this->VPhysicsGetObject();
	if( pPhysObject )
	{
		m_nOriginalMaterialIndex = pPhysObject->GetMaterialIndex();
	}

	this->AddFlag( FL_AFFECTED_BY_PAINT );
	if( m_PrePaintedPower != NO_POWER )
	{
		this->Paint( (PaintPowerType)m_PrePaintedPower, vec3_origin );
	}
}


template< typename BasePropType >
void PropPaintPowerUser<BasePropType>::VPhysicsCollision( int index, gamevcollisionevent_t *pEvent )
{
	if( engine->HasPaintmap() )
	{
		CBaseEntity* pOther = pEvent->pEntities[!index];

		PaintPowerInfo_t contact;

		// Get data out of the event
		Vector vNormal, vPoint;
		pEvent->pInternalData->GetSurfaceNormal( vNormal );
		pEvent->pInternalData->GetContactPoint( vPoint );

		// Fill out contact info
		contact.m_SurfaceNormal = -vNormal;
		contact.m_ContactPoint = vPoint;
		contact.m_HandleToOther.Set( pOther );

		// Add info to paint power info
		this->AddSurfacePaintPowerInfo( contact, 0 );
	}

	BaseClass::VPhysicsCollision( index, pEvent );
}

template< typename BasePropType >
void PropPaintPowerUser<BasePropType>::VPhysicsUpdate( IPhysicsObject *pPhysics )
{
	if( engine->HasPaintmap() )
	{
		UpdatePaintPowersFromContacts();
	}

	BaseClass::VPhysicsUpdate( pPhysics );
}

template< typename BasePropType >
void PropPaintPowerUser<BasePropType>::UpdatePaintPowersFromContacts()
{
	//If the prop is held by a player
	if( GetPlayerHoldingEntity( this ) )
	{
		//If the prop was not already held by a player
		if( !m_bHeldByPlayer )
		{
			m_bHeldByPlayer = true;

			//Set the timer
			m_flPickedUpTime = gpGlobals->curtime;
		}
	}
	else
	{
		m_bHeldByPlayer = false;
		m_flPickedUpTime = 0.0f;
	}

	IPhysicsObject* pPhysObject = this->VPhysicsGetObject();
	if( pPhysObject )
	{
		IPhysicsFrictionSnapshot* pSnapShot = pPhysObject->CreateFrictionSnapshot();
		while( pSnapShot->IsValid() )
		{
			PaintPowerInfo_t contact;

			IPhysicsObject *pOther = pSnapShot->GetObject(1);
			CBaseEntity *pOtherEntity = static_cast<CBaseEntity *>(pOther->GetGameData());
			Assert(pOtherEntity);

			if( pOtherEntity != NULL )
			{
				// Get data out of the event
				Vector vNormal, vPoint;
				pSnapShot->GetSurfaceNormal( vNormal );
				pSnapShot->GetContactPoint( vPoint );

				// Fill out contact info
				contact.m_SurfaceNormal = -vNormal;
				contact.m_ContactPoint = vPoint;
				contact.m_HandleToOther.Set( pOtherEntity );

				// Add info to paint power info
				this->AddSurfacePaintPowerInfo( contact, 0 );
			}

			pSnapShot->NextFrictionData();
		}

		pPhysObject->DestroyFrictionSnapshot( pSnapShot );

		// Figure out paint powers
		this->UpdatePaintPowers();
	}
	else
	{
		// Clear all current data
		this->ClearSurfacePaintPowerInfo();
	}
}



template< typename BasePropType >
void PropPaintPowerUser<BasePropType>::ChooseActivePaintPowers( BaseClass_PaintPowerInfoVector& activePowers )
{
	this->MapSurfacesToPowers();

	// Get the contacts
	PaintPowerConstRange powerRange = this->GetSurfacePaintPowerInfo();
	size_t count = powerRange.second - powerRange.first;

	// Set our desired paint power to be our current painted color
	PaintPowerInfo_t desiredPower;
	desiredPower.m_PaintPowerType = NO_POWER;

	// Get the first active power, since props can only have one at a time
	const PaintPowerInfo_t* pHighestPriorityActivePower = this->FindHighestPriorityActivePaintPower();
	PaintPowerInfo_t currentPower = pHighestPriorityActivePower ? *pHighestPriorityActivePower : PaintPowerInfo_t( Vector(0, 0, 1), this->GetAbsOrigin(), 0 );

	PaintPowerType paintedPower = this->GetPaintedPower();

	// If we're touching something
	if( count != 0 )
	{
		this->PrioritySortSurfacePaintPowerInfo( &DescendingPaintPriorityCompare );

		// Default our desired color to be our current painted color so this will default
		// as our power if all else fails
		if( paintedPower != NO_POWER )
		{
			for( PaintPowerConstIter i = powerRange.first; i != powerRange.second; ++i )
			{
				if( i->m_PaintPowerType != INVALID_PAINT_POWER )
				{
					desiredPower = *i;
					desiredPower.m_PaintPowerType = paintedPower;
					break;
				}
			}
		}

		// Go through all the surfaces and try to find a power to use
		for( PaintPowerConstIter i = powerRange.first; i != powerRange.second; ++i )
		{
			const PaintPowerInfo_t& powerInfo = *i;

			if( currentPower.m_PaintPowerType == SPEED_POWER )
			{
				// Always take bounce when currently using speed
				if( powerInfo.m_PaintPowerType == BOUNCE_POWER )
				{
					desiredPower = powerInfo;
				}
				// Take speed if others are not present
				else if( desiredPower.m_PaintPowerType == NO_POWER &&
					powerInfo.m_PaintPowerType == SPEED_POWER )
				{
					desiredPower = powerInfo;
				}
			}
			else if( powerInfo.m_PaintPowerType != NO_POWER && 
				powerInfo.m_PaintPowerType < desiredPower.m_PaintPowerType )
			{	// Accept whatever it was if it's of higher priority
				desiredPower = powerInfo;
			}
		}//for
	}//if count

	// Add the power to the active list
	activePowers.AddToTail( desiredPower );
}

template< typename BasePropType >
PaintPowerState PropPaintPowerUser<BasePropType>::ActivateSpeedPower( PaintPowerInfo_t& powerInfo )
{
	IPhysicsObject* pPhysObject = this->VPhysicsGetObject();
	if( pPhysObject )
	{
		pPhysObject->SetMaterialIndex( ThisClass::GetSpeedMaterialIndex() );
	}

	return ACTIVE_PAINT_POWER;
}


template< typename BasePropType >
PaintPowerState PropPaintPowerUser<BasePropType>::UseSpeedPower( PaintPowerInfo_t& powerInfo )
{
	return ACTIVE_PAINT_POWER;
}


template< typename BasePropType >
PaintPowerState PropPaintPowerUser<BasePropType>::DeactivateSpeedPower( PaintPowerInfo_t& powerInfo )
{
	IPhysicsObject* pPhysObject = this->VPhysicsGetObject();
	if( pPhysObject )
	{
		pPhysObject->SetMaterialIndex( m_nOriginalMaterialIndex );
	}

	return INACTIVE_PAINT_POWER;
}


extern ConVar sv_wall_bounce_trade;
extern ConVar bounce_paint_wall_jump_upward_speed;
extern ConVar bounce_paint_min_speed;

template< typename BasePropType >
PaintPowerState PropPaintPowerUser<BasePropType>::ActivateBouncePower( PaintPowerInfo_t& info )
{
	IPhysicsObject* pPhysObject = this->VPhysicsGetObject();
	if( pPhysObject )
	{
		float flTrade = sv_wall_bounce_trade.GetFloat();	// We trade some outward velocity for upward velocity
		const Vector vUp = Vector(0,0,1);
		Vector vBounceVel(0,0,0);

		// Cancel out velocity going into the surface
		Vector velocity;
		AngularImpulse angularVel;
		pPhysObject->GetVelocity( &velocity, &angularVel);
		velocity -= info.m_SurfaceNormal  * DotProduct( velocity, info.m_SurfaceNormal );

		// Cancel out downward velocity (allows for going up parallel walls)
		velocity -= vUp * DotProduct( velocity, vUp );

		// Store this for later
		Vector velNorm = velocity;
		velNorm.NormalizeInPlace();

		float flNormDot = DotProduct( vUp, info.m_SurfaceNormal );

		float flBounceScale = 0.f;
		// Add upward velocity if surface normal is facing up, relative to the player
		if( flNormDot > -0.1f )
		{
			// Extra upward wall bounce velocity
			flBounceScale = (1.f - DotProduct( vUp, info.m_SurfaceNormal ));
			vBounceVel += vUp * bounce_paint_wall_jump_upward_speed.GetFloat() * flBounceScale;
		}
		else // Downward facing wall.  Add velocity in the XY plane
		{
			// Vector pointing out of the surface in the XY plane
			Vector vOut = ( info.m_SurfaceNormal - (vUp * DotProduct( vUp, info.m_SurfaceNormal )) ).Normalized();

			// Extra lateral velocity off the wall
			flBounceScale = DotProduct( vOut, info.m_SurfaceNormal );
			vBounceVel += vOut * bounce_paint_wall_jump_upward_speed.GetFloat() * flBounceScale;
		}

		// Calculate how much bounce velocity is left to spend after the lateral
		float fWallBounceScale = flTrade + ( (1.f - flBounceScale) *  (1.0 - flTrade) );
		// Velocity off of the surface
		vBounceVel += info.m_SurfaceNormal * bounce_paint_min_speed.GetFloat() * fWallBounceScale;

		// If we're going to bounce straight up, add some random XY velocity.  Bouncing straight up
		// doesn't look natural.
		if( vBounceVel.x == 0.f && vBounceVel.y == 0.f )
		{
			vBounceVel += Vector( RandomFloat(-80.f, 80.f), RandomFloat(-80.f, 80.f), 0.f );
		}

		// Dont let our velocity fight the new bounce velocity
		velocity -= vBounceVel.Normalized() * DotProduct( velocity, vBounceVel.Normalized() );

		velocity += vBounceVel;

		// Add velocity to physics object
		pPhysObject->SetVelocity( &velocity, &angularVel );
	}

	return DEACTIVATING_PAINT_POWER;
}


template< typename BasePropType >
PaintPowerState PropPaintPowerUser<BasePropType>::UseBouncePower( PaintPowerInfo_t& powerInfo )
{
	return DEACTIVATING_PAINT_POWER;
}


template< typename BasePropType >
PaintPowerState PropPaintPowerUser<BasePropType>::DeactivateBouncePower( PaintPowerInfo_t& powerInfo )
{
	return INACTIVE_PAINT_POWER;
}

template< typename BasePropType >
int PropPaintPowerUser<BasePropType>::GetSpeedMaterialIndex()
{
	static int s_SpeedMaterialIndex = physprops->GetSurfaceIndex( "ice" );
	return s_SpeedMaterialIndex;
}

template< typename BasePropType >
void PropPaintPowerUser<BasePropType>::Paint( PaintPowerType type, const Vector& worldContactPt )
{
	BaseClass::Paint( type, worldContactPt );

	IPhysicsObject* pPhysicsObject = this->VPhysicsGetObject();
	if( pPhysicsObject != NULL && pPhysicsObject->IsAsleep() )
	{
		pPhysicsObject->Wake();
	}
}

#endif // ifndef PROP_PAINT_POWER_USER_H
