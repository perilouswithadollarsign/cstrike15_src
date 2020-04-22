//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef C_PROP_WEIGHTEDCUBE_H
#define C_PROP_WEIGHTEDCUBE_H
#ifdef _WIN32
#pragma once
#endif

#include "c_physicsprop.h"


class C_PropWeightedCube : public C_PhysicsProp
{
public:
	DECLARE_CLASS( C_PropWeightedCube, C_PhysicsProp );
	DECLARE_CLIENTCLASS();

	//IPlayerPickupVPhysics
	virtual QAngle PreferredCarryAngles( void );
	virtual const Vector& GetRenderOrigin( void );

	virtual void UpdateOnRemove( void );
	virtual void Spawn( void );
	
	static CUtlVector<C_PropWeightedCube *> s_AllWeightedCubes;
};

typedef C_PropWeightedCube CPropWeightedCube;

#endif //#ifndef C_TRIGGER_PORTAL_CLEANSER_H
