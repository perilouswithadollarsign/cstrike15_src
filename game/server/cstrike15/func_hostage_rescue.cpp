//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "func_hostage_rescue.h"

//============================================================================
LINK_ENTITY_TO_CLASS( func_hostage_rescue, CHostageRescueZone );

BEGIN_DATADESC( CHostageRescueZone )

	//Functions
	DEFINE_FUNCTION( HostageRescueTouch ),

END_DATADESC()


void CHostageRescueZone::Spawn()
{
	m_hInstructorHint = NULL;

	InitTrigger();
	SetTouch( &CHostageRescueZone::HostageRescueTouch );

	VisibilityMonitor_AddEntity( this, 1600.0f, NULL, NULL );
}

void CHostageRescueZone::ReInitOnRoundStart( void )
{
	if ( m_hInstructorHint.Get() )
	{
		CPointEntity *pEnt = static_cast< CPointEntity* >( m_hInstructorHint.Get() );
		UTIL_Remove( pEnt );
		m_hInstructorHint = NULL;
	}

	//CCSPlayerResource *pCSPR = CSPlayerResource();
	Vector vecPosCur = Vector( 0, 0, 0 );
	Vector vecPosNew = Vector( 0, 0, 0 );
	Vector vecTrigger = CollisionProp()->WorldSpaceCenter();
	if ( CSPlayerResource() )
	{
		vecPosCur = CSPlayerResource()->GetHostageRescuePosition( 0 );

		for ( int i=0; i < MAX_HOSTAGE_RESCUES; i++ )
		{
			vecPosNew = CSPlayerResource()->GetHostageRescuePosition( MIN( i+1, MAX_HOSTAGE_RESCUES-1 ) );
			if ( vecPosNew != vec3_origin )
			{
				if ( vecTrigger.DistTo( vecPosCur ) > vecTrigger.DistTo( vecPosNew ) )
				{
					vecPosCur = vecPosNew;
				}
			}
		}

		CPointEntity *pEnt = NULL;
		if ( vecPosCur != vec3_origin )
		{
			pEnt = static_cast< CPointEntity* >( CreateEntityByName( "info_hostage_rescue_zone_hint" ) );
		}

		if ( pEnt )
		{
			pEnt->Spawn();
			trace_t tr;

			UTIL_TraceLine( vecPosCur, vecPosCur + Vector ( 0, 0, -128 ), MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );
			Vector vecHint = tr.endpos +  Vector ( 0, 0, 16 );

			pEnt->SetAbsOrigin( vecHint );
			pEnt->SetOwnerEntity( this );
			pEnt->SetParent( this );

			m_hInstructorHint = pEnt;
		}
	}
}

void CHostageRescueZone::HostageRescueTouch( CBaseEntity *pOther )
{
	if ( m_bDisabled == false )
	{
		variant_t emptyVariant;
		pOther->AcceptInput( "OnRescueZoneTouch", NULL, NULL, emptyVariant, 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose: A generic target entity that gets replicated to the client for displaying a hint for the hostage rescue zone
//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( info_hostage_rescue_zone_hint, CInfoInstructorHintHostageRescueZone );

BEGIN_DATADESC( CInfoInstructorHintHostageRescueZone )

END_DATADESC()

void CInfoInstructorHintHostageRescueZone::Spawn( void )
{
	VisibilityMonitor_AddEntity( this, 5000.0f, NULL, NULL );
}



