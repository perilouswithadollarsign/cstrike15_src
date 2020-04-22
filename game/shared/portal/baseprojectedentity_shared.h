//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BASEPROJECTEDENTITY_SHARED_H
#define BASEPROJECTEDENTITY_SHARED_H

#ifdef _WIN32
#pragma once
#endif

#include "cbase.h"

#if defined( GAME_DLL )
#include "baseprojectedentity.h"
#else
#include "c_baseprojectedentity.h"
#endif

// Create these a small amount in front of portals they redirect through
// and in front of solid objects they project into to prevent z fighting and stuck-in-solid problems.
#define PROJECTION_END_POINT_EPSILON 0.1f

#define PROJECTOR_MAX_LENGTH 4096

#endif // BASEPROJECTEDENTITY_SHARED_H
