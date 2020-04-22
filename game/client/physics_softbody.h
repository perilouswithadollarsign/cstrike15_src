//========= Copyright © Valve Corporation, All rights reserved. ============//
#ifndef PHYSICS_SOFTBODY_HDR
#define PHYSICS_SOFTBODY_HDR

#include "mathlib/softbodyenvironment.h"

#ifndef CLIENT_DLL
#error Softbodies should only be used on client
#endif

extern CSoftbodyEnvironment g_SoftbodyEnvironment; // in physics_softbody.cpp
extern ConVar softbody_debug; // in studio.cpp

class CSoftbodyProcess : public CAutoGameSystemPerFrame
{
public:
	CSoftbodyProcess() : CAutoGameSystemPerFrame( "SoftbodyProcess" ) {}
	virtual void OnRestore() OVERRIDE;
	virtual void Update( float frametime ) OVERRIDE;

	virtual void PostRender() OVERRIDE;
};

#endif