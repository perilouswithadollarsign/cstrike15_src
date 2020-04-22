//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Provide interface to custom materials for entities that use them
//
//=============================================================================//

#pragma once

#include "refcount.h"

class IMaterial;
class ICompositeTexture;

class ICustomMaterial : public CRefCounted<>
{
public:
	virtual IMaterial *GetMaterial() = 0;
	virtual void AddTexture( ICompositeTexture *pTexture ) = 0;
	virtual ICompositeTexture *GetTexture( int nIndex ) = 0;
	virtual bool IsValid() const = 0;
	virtual bool CheckRegenerate( int nSize ) = 0;
	virtual const char* GetBaseMaterialName( void ) = 0;
};