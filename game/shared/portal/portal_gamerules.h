//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Game rules for Portal.
//
//=============================================================================//

#ifndef PORTAL_GAMERULES_H
#define PORTAL_GAMERULES_H
#ifdef _WIN32
#pragma once
#endif

#include "gamerules.h"
#include "hl2_gamerules.h"

#define DISABLE_DEBUG_HISTORY 1

#ifdef CLIENT_DLL
	#define CPortalGameRules C_PortalGameRules
	#define CPortalGameRulesProxy C_PortalGameRulesProxy
#endif


class CPortalGameRulesProxy : public CGameRulesProxy
{
public:
	DECLARE_CLASS( CPortalGameRulesProxy, CGameRulesProxy );
	DECLARE_NETWORKCLASS();
};


class CPortalGameRules : public CHalfLife2
{
public:
	DECLARE_CLASS( CPortalGameRules, CHalfLife2 );

	CPortalGameRules();
	virtual ~CPortalGameRules() {}

#ifdef CLIENT_DLL
	virtual bool IsBonusChallengeTimeBased( void );
	virtual bool IsChallengeMode();
#endif

	virtual bool ShouldCollide( int collisionGroup0, int collisionGroup1 );

private:

#ifdef CLIENT_DLL
	DECLARE_CLIENTCLASS_NOBASE(); // This makes datatables able to access our private vars.
#else
	DECLARE_SERVERCLASS_NOBASE(); // This makes datatables able to access our private vars.
	
public:

	virtual const char *	GetGameDescription( void );
	virtual bool			AllowDamage( CBaseEntity *pVictim, const CTakeDamageInfo &info );
	virtual void			RegisterScriptFunctions( void );

	virtual bool			ShouldBurningPropsEmitLight() { return false; }
	virtual float			FlPlayerFallDamage( CBasePlayer *pPlayer ) { return 0.0f; } //no fall damage in portal
	virtual bool			ClientCommand( CBaseEntity *pEdict, const CCommand &args );

	virtual bool			IsSavingAllowed( void );
#endif
};


//-----------------------------------------------------------------------------
// Gets us at the Portal game rules
//-----------------------------------------------------------------------------
inline CPortalGameRules* PortalGameRules()
{
	return dynamic_cast<CPortalGameRules*>(g_pGameRules);
}



#endif // PORTAL_GAMERULES_H
