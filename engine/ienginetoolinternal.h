//====== Copyright  1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Methods of IEngineTool visible only inside the engine
//
//=============================================================================

#ifndef IENGINETOOLINTERNAL_H
#define IENGINETOOLINTERNAL_H

#include "toolframework/ienginetool.h"


//-----------------------------------------------------------------------------
// Purpose: Singleton implementation of external tools callback interface
//-----------------------------------------------------------------------------
class IEngineToolInternal : public IEngineTool
{
public:
	virtual void	SetIsInGame( bool bIsInGame ) = 0;
};

#if !defined( DEICATED )
extern IEngineToolInternal *g_pEngineToolInternal;
#endif // !LINUX

#endif // IENGINETOOLINTERNAL_H

