//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h" 
#include "weapon_csbasegun.h"


#if defined( CLIENT_DLL )
	#define CDEagle C_DEagle
	#include "c_cs_player.h"
#else
	#include "cs_player.h"
#endif

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

class CDEagle : public CWeaponCSBaseGun
{
public:
	DECLARE_CLASS( CDEagle, CWeaponCSBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CDEagle();

	// overload for dryfire animation
	virtual bool SendWeaponAnim( int iActivity );

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_DEAGLE; }

private:
	CDEagle( const CDEagle & );
};


IMPLEMENT_NETWORKCLASS_ALIASED( DEagle, DT_WeaponDEagle )

BEGIN_NETWORK_TABLE( CDEagle, DT_WeaponDEagle )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CDEagle )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS_ALIASED( weapon_deagle, DEagle );
// PRECACHE_REGISTER( weapon_deagle );

CDEagle::CDEagle()
{
}

bool CDEagle::SendWeaponAnim( int iActivity )
{
	if ( iActivity == ACT_VM_PRIMARYATTACK && m_iClip1 == 1 )
		iActivity = ACT_VM_DRYFIRE;
	return BaseClass::SendWeaponAnim( iActivity );
}
