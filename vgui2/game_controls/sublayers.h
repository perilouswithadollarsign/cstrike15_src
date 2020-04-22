//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef GAMEGRAPHIC_H
#define GAMEGRAPHIC_H

#ifdef _WIN32
#pragma once
#endif

#include "game_controls/uiquadinfo.h"
#include "tier1/utlvector.h"


//-----------------------------------------------------------------------------
// A class that contains a texture.
//-----------------------------------------------------------------------------
class IGameGraphic
{
public:
	virtual void GetQuads( CUtlVector<CQuadInfo> &quadInfo ) = 0;

};


#endif // GAMEGRAPHIC_H
