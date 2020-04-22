//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef IWORLDRENDERERMGR_H
#define IWORLDRENDERERMGR_H

#include "iworldrenderer.h"
#include "iworld.h"

abstract_class IWorldRendererMgr : public IAppSystem
{
public:
	virtual IWorldRenderer *CreateWorldRenderer() = 0;
	virtual void DestroyWorldRenderer( IWorldRenderer *pWorldRenderer ) = 0;

	virtual bool IsHWInstancingEnabled() = 0;
	virtual void SetHWInstancingEnabled( bool bInstancingEnabled ) = 0;

	// New world stuff
	virtual HWorld CreateWorld( const char *pWorldName ) = 0;
	virtual void DestroyWorld( HWorld hWorld ) = 0;
	virtual IWorld *GetWorldPtr( HWorld hWorld ) = 0;
};

#endif