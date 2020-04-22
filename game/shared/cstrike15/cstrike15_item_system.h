//========= Copyright © 1996-2003, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
//=============================================================================

#ifndef CSTRIKE15_ITEM_SYSTEM_H
#define CSTRIKE15_ITEM_SYSTEM_H
#ifdef _WIN32
#pragma once
#endif

#include "econ_item_system.h"
//#include "cstrike15_item_constants.h"

//-----------------------------------------------------------------------------
// Criteria used by the system to generate a base item for a slot in a class's loadout
//-----------------------------------------------------------------------------
struct baseitemcriteria_t
{
	baseitemcriteria_t()
	{
		iClass = 0;
		iSlot = LOADOUT_POSITION_INVALID;
	}

	int					iClass;
	int					iSlot;
};


class CCStrike15ItemSystem : public CEconItemSystem
{
public:
	// Select and return the base item definition index for a class's load-out slot 
	virtual int GenerateBaseItem( baseitemcriteria_t *pCriteria );
};

CCStrike15ItemSystem *CStrike15ItemSystem( void );

#endif // CSTRIKE15_ITEM_SYSTEM_H