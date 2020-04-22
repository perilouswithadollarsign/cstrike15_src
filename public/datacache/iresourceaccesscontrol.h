//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//

#ifndef IRESOURCEACCESSCONTROL_H
#define IRESOURCEACCESSCONTROL_H

#ifdef _WIN32
#pragma once
#endif


#include "tier0/dbg.h"
#include "tier2/resourceprecacher.h"
#include "appframework/iappsystem.h"

//-----------------------------------------------------------------------------
// Resource access control API
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// IResourceAccessControl
// Purpose: Maintains lists of resources to use them as filters to prevent access
// to ensure proper precache behavior in game code
//-----------------------------------------------------------------------------
abstract_class IResourceAccessControl : public IAppSystem
{
public:
	// Creates, destroys a resource list
	virtual ResourceList_t CreateResourceList( const char *pDebugName ) = 0;
	virtual void DestroyAllResourceLists( ) = 0;

	// Adds a resource to a resource list
	virtual void AddResource( ResourceList_t hResourceList, ResourceTypeOld_t nType, const char *pResourceName ) = 0;

	// Prevents access to anything except the specified resource list
	// Pass RESOURCE_LIST_INVALID to allow access to all resources
	virtual void LimitAccess( ResourceList_t hResourceList ) = 0;

	// Is access to this resource allowed?
	virtual bool IsAccessAllowed( ResourceTypeOld_t nType, const char *pResource ) = 0;
};


#endif // IRESOURCEACCESSCONTROL_H
