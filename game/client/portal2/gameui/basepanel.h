//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BASEPANEL_H
#define BASEPANEL_H
#ifdef _WIN32
#pragma once
#endif

#if defined( PORTAL2 )
#include "portal2/basemodpanel.h"
#elif defined( SWARM_DLL )
#include "swarm/basemodpanel.h"
#endif

inline BaseModUI::CBaseModPanel * BasePanel() { return &BaseModUI::CBaseModPanel::GetSingleton(); }

#endif // BASEPANEL_H
