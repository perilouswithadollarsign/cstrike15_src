//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Declares the interface class for all paint power users.
//
//=============================================================================//
#ifndef PAINT_POWER_USER_INTERFACE_H
#define PAINT_POWER_USER_INTERFACE_H

#include <utility>
#include "paint_color_manager.h"
#include "paint_power_info.h"

typedef PaintPowerInfo_t* PaintPowerIter;
typedef const PaintPowerInfo_t* PaintPowerConstIter;
typedef std::pair< PaintPowerIter, PaintPowerIter > PaintPowerRange;
typedef std::pair< PaintPowerConstIter, PaintPowerConstIter > PaintPowerConstRange;

abstract_class IPaintPowerUser
{
public:
	//-------------------------------------------------------------------------
	// Virtual Destructor
	//-------------------------------------------------------------------------
	virtual ~IPaintPowerUser();

	//-------------------------------------------------------------------------
	// Public Accessors
	//-------------------------------------------------------------------------
	virtual const PaintPowerConstRange GetPaintPowers() const = 0;
	virtual const PaintPowerInfo_t& GetPaintPower( unsigned powerType ) const = 0;
	virtual const PaintPowerInfo_t* FindHighestPriorityActivePaintPower() const = 0;

	//-------------------------------------------------------------------------
	// Paint Power Effects
	//-------------------------------------------------------------------------
	virtual void AddSurfacePaintPowerInfo( const PaintPowerInfo_t& contact, char const* context = 0 ) = 0;
	virtual void UpdatePaintPowers() = 0;
};


void MapSurfaceToPower( PaintPowerInfo_t& info );

#endif // ifndef PAINT_POWER_USER_INTERFACE_H
