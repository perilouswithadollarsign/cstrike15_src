//========= Copyright © 2011-2011, Valve Corporation, All rights reserved. ============//
//
// Purpose: No defuse area
//
//=============================================================================//

#include "cbase.h"
#include "cs_player.h"
#include "func_no_defuse.h"

LINK_ENTITY_TO_CLASS( func_no_defuse, CNoDefuseArea );

BEGIN_DATADESC( CNoDefuseArea )
	DEFINE_FUNCTION( NoDefuseAreaTouch ),
END_DATADESC()

void CNoDefuseArea::Spawn()
{
	InitTrigger();
	SetTouch( &CNoDefuseArea::NoDefuseAreaTouch );
}

void CNoDefuseArea::NoDefuseAreaTouch( CBaseEntity* pOther )
{
	CCSPlayer *player = dynamic_cast< CCSPlayer* >( pOther );
	// CT players are not allowed to defuse a bomb when they are in a no defuse zone.
	// the is to help prevent bugs with CTs being able to defuse bombs through walls and floors etc.
	if ( player )
	{
		player->m_bInNoDefuseArea = true;
	}
}