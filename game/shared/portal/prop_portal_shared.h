//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PROP_PORTAL_SHARED_H
#define PROP_PORTAL_SHARED_H

#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"
#include "portal_base2d_shared.h"

#ifdef CLIENT_DLL
#include "C_Prop_Portal.h"
#else
#include "prop_portal.h"
#endif

#define DEFAULT_PORTAL_HALF_WIDTH	32.0f
#define DEFAULT_PORTAL_HALF_HEIGHT	56.0f

class CProp_Portal_Shared  //defined as a class to make intellisense more intelligent
{
public:

#ifdef CLIENT_DLL
	static CUtlVector<C_Prop_Portal *> AllPortals; //an array of existing portal entities	
#else
	static CUtlVector<CProp_Portal *> AllPortals; //an array of existing portal entities
#endif //#ifdef CLIENT_DLL
};





#endif //#ifndef PROP_PORTAL_SHARED_H