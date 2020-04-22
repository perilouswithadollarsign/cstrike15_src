//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Declares the paint color manager class.
//
//=============================================================================//
#ifndef PAINT_COLOR_MANAGER_H
#define PAINT_COLOR_MANAGER_H

// src/public/
#include "game/shared/portal2/paint_enum.h"

PaintPowerType MapColorToPower( const color24& color );
PaintPowerType MapColorToPower( const Color& color );
PaintPowerType MapColorToPower( const CUtlVector<BYTE>& colors );
Color MapPowerToColor( int paintPowerType );
Color MapPowerToVisualColor( int paintPowerType );

#endif // ifndef PAINT_COLOR_MANAGER_H
