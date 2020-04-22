//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PORTAL_BASE2D_SHARED_H
#define PORTAL_BASE2D_SHARED_H

#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"

#ifdef CLIENT_DLL
class C_Portal_Base2D;
#else
class CPortal_Base2D;
#endif

class CPhysCollide;

// CProp_Portal enum for the portal corners (if a user wants a specific corner)
enum PortalCorners_t { PORTAL_DOWN_RIGHT = 0, PORTAL_DOWN_LEFT, PORTAL_UP_RIGHT, PORTAL_UP_LEFT };

class CPortal_Base2D_Shared  //defined as a class to make intellisense more intelligent
{
public:
	static void UpdatePortalTransformationMatrix( const matrix3x4_t &localToWorld, const matrix3x4_t &remoteToWorld, VMatrix *pMatrix );

	static bool IsEntityTeleportable( CBaseEntity *pEntity );
	static bool ShouldPhysicsCloneNonTeleportableEntityAcrossPortals( CBaseEntity *pEntity );
	
#ifdef CLIENT_DLL
	static CUtlVector<C_Portal_Base2D *> AllPortals; //an array of existing portal entities	
#else
	static CUtlVector<CPortal_Base2D *> AllPortals; //an array of existing portal entities
#endif //#ifdef CLIENT_DLL

protected:

	bool					ShouldTeleportTouchingEntity( CBaseEntity *pOther ); //assuming the entity is or was just touching the portal, check for teleportation conditions
	void					TeleportTouchingEntity( CBaseEntity *pOther );


	void					UpdateCollisionShape( void );
	CPhysCollide			*m_pCollisionShape;
};

//make it so shared code can just load the shared header and get the right version
#ifdef CLIENT_DLL
#include "c_portal_base2d.h"
#else
#include "portal_base2d.h"
#endif

#endif //#ifndef PORTAL_BASE2D_SHARED_H

