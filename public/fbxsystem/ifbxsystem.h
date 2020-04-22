//============ Copyright (c) Valve Corporation, All rights reserved. ==========
//
//=============================================================================


#ifndef IFBXSYSTEM_H
#define IFBXSYSTEM_H
#pragma once


// FBX includes
#include <fbxsdk.h>

// Valve includes
#include "appframework/iappsystem.h"
#include "tier0/logging.h"


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
abstract_class IFbxSystem : public IAppSystem
{
public:
	virtual InitReturnVal_t Init() = 0;
	virtual void Shutdown() = 0;
	virtual FbxManager *GetFbxManager() = 0;

};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
extern IFbxSystem *g_pFbx;


//-----------------------------------------------------------------------------
// External declaration of the logging channel for FBX system
//-----------------------------------------------------------------------------
DECLARE_LOGGING_CHANNEL( LOG_FBX_SYSTEM );

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
#define FBX_INTERFACE_VERSION   "FBXSystem001"


#endif // IFBXSYSTEM_H
