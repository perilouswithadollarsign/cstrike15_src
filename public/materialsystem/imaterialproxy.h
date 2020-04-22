//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IMATERIALPROXY_H
#define IMATERIALPROXY_H
#pragma once

#include "interface.h"

#define IMATERIAL_PROXY_INTERFACE_VERSION "_IMaterialProxy003"

class IMaterial;
class KeyValues;

abstract_class IMaterialProxy
{
public:
	virtual bool Init( IMaterial* pMaterial, KeyValues *pKeyValues ) = 0;
	virtual void OnBind( void * ) = 0;
	virtual void Release() = 0;
	virtual IMaterial *	GetMaterial() = 0;

	// Is this material proxy allowed to be called in the async thread? Most are no, a few are yes.
	// This could be converted from a true interface to having a single bool that gets set at construction
	// time for this. I'm considering it because this gets called on the hot path, but it's probably not 
	// worth it now.
	virtual bool CanBeCalledAsync() const { return false; }

protected:
	// no one should call this directly
	virtual ~IMaterialProxy() {}
};

#endif // IMATERIALPROXY_H
