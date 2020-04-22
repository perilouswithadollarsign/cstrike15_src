//============ Copyright (c) Valve Corporation, All rights reserved. ==========
//
//=============================================================================


#ifndef FBXSYSTEM_H
#define FBXSYSTEM_H


#if COMPILER_MSVC
#pragma once
#endif


// Valve includes
#include "fbxsystem/ifbxsystem.h"


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CFbxSystem : public CBaseAppSystem< IFbxSystem >
{
	typedef CBaseAppSystem< IFbxSystem > BaseClass;

public:
	CFbxSystem();
	virtual ~CFbxSystem();

	// From CBaseAppSystem
	virtual InitReturnVal_t Init() OVERRIDE;
	virtual void Shutdown() OVERRIDE;
	virtual AppSystemTier_t GetTier() OVERRIDE { return APP_SYSTEM_TIER2; }
	virtual bool IsSingleton() OVERRIDE{ return false; }

	// From IFbxSystem
	virtual FbxManager *GetFbxManager();

private:
	FbxManager *m_pFbxManager;

};

#endif // FBXSYSTEM_H
