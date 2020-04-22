//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// NavMeshFactory.cpp
// Factory to create the NavMesh singleton
// Author: Michael S. Booth, June 2005

#include "cbase.h"
#include "nav_mesh.h"

#ifdef TERROR
#include "terror/TerrorNav.h"
#endif

#ifdef CSTRIKE_DLL
#include "cs_nav_mesh.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNavMesh *NavMeshFactory( void )
{
#ifdef TERROR
	return new TerrorNavMesh;
#endif

#ifdef CSTRIKE_DLL
	return new CSNavMesh;
#endif

	return new CNavMesh;
}
