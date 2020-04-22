//========= Copyright © 2011-2011, Valve Corporation, All rights reserved. ============//
//
// Purpose: No defuse area ent
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "triggers.h"

class CNoDefuseArea : public CBaseTrigger
{
public:
	DECLARE_CLASS( CNoDefuseArea, CBaseTrigger );
	DECLARE_DATADESC();

	void Spawn();
	void EXPORT NoDefuseAreaTouch( CBaseEntity* pOther );
};