//========= Copyright © 1996-2007, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef C_SURFACERENDER_H
#define C_SURFACERENDER_H

#if defined( _WIN32 )
#pragma once
#endif

#ifdef USE_BLOBULATOR

#include "../../common/blobulator/Implicit/ImpDefines.h"
#include "../../common/blobulator/Implicit/ImpRenderer.h"
#include "../../common/blobulator/Implicit/ImpTiler.h"
#include "../../common/blobulator/Implicit/UserFunctions.h"

void Surface_Draw( IClientRenderable *pClientRenderable, const Vector &vecRenderOrigin, IMaterial *pMaterial, float flCubeWidth, bool bSurfaceNoParticleCull = false );

void Surface_SafeLightCubeUpdate( const Vector &vecRenderOrigin, Vector4D *cachedCubeColours );


extern CUtlVector<ImpParticleWithFourInterpolants, CUtlMemoryAligned<ImpParticleWithFourInterpolants, 16>> g_SurfaceRenderParticles;
extern const QAngle g_SurfaceRenderAnglesAngles;

#endif


#endif // C_SURFACERENDER_H
