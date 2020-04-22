//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Healthshot, it makes you go
//
// $NoKeywords: $
//=====================================================================================//

#ifndef _ITEM_HEALTHSHOT_H_
#define _ITEM_HEALTHSHOT_H_

#ifdef _WIN32
#pragma once
#endif

#include "weapon_baseitem.h"
#include "util_shared.h"

#if defined( CLIENT_DLL )
#define CItem_Healthshot C_Item_Healthshot
#endif

//-----------------------------------------------------------------------------------------------------------
/**
* Healthshot. When used, give the player speed boost for a short amount of time
*/
class CItem_Healthshot : public CWeaponBaseItem
{
public:
	DECLARE_CLASS( CItem_Healthshot, CWeaponBaseItem );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

// #ifndef CLIENT_DLL
// 	DECLARE_DATADESC();
// #endif

	CItem_Healthshot() 	{}

	virtual void	Precache( void );

	virtual bool	CanPrimaryAttack( void );

	virtual CSWeaponID GetCSWeaponID( void ) const		{ return WEAPON_HEALTHSHOT; }

	void DropHealthshot( void );
	// CBaseBeltItem
	virtual bool CanUseOnSelf( CCSPlayer *pPlayer );
	virtual void OnStartUse( CCSPlayer *pPlayer );
	virtual float GetUseTimerDuration( void );
	virtual void WeaponIdle();

#ifndef CLIENT_DLL
	virtual void CompleteUse( CCSPlayer *pPlayer );
#endif

private:
	CItem_Healthshot( const CItem_Healthshot & ) {}
};

#endif // _ITEM_HEALTHSHOT_H_
