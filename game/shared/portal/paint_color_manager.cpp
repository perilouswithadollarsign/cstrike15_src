//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the paint color manager class.
//
//=============================================================================//

#include "cbase.h"
#include "paint_color_manager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Paint color ConVars
ConVar speed_paint_color( "speed_paint_color", "255 106 0 255", FCVAR_REPLICATED, "Color for speed paint" );
ConVar bounce_paint_color( "bounce_paint_color", "0 165 255 255", FCVAR_REPLICATED, "Color for bounce paint" );
// FIXME: Bring this back for DLC2
//ConVar reflect_paint_color( "reflect_paint_color", "0 255 0 255", FCVAR_REPLICATED, "Color for reflect paint" );
ConVar portal_paint_color( "portal_paint_color", "128 128 128 255", FCVAR_REPLICATED, "Color for portal paint");
ConVar erase_color( "erase_color", "0 0 0 0", FCVAR_REPLICATED, "Color for erase" );
ConVar erase_visual_color( "erase_visual_color", "255 255 255 255", FCVAR_REPLICATED, "Color for erase that is rendered" );

ConVar paint_color_max_diff( "paint_color_max_diff", "32", FCVAR_REPLICATED, "The maximum difference between two colors for matching." );

int ComputeColorDiff( const Color& colorA, const Color& colorB )
{
	return abs( colorA.r() - colorB.r() ) +
		   abs( colorA.g() - colorB.g() ) +
		   abs( colorA.b() - colorB.b() );
}


PaintPowerType MapColorToPower( const Color& color )
{
	int result = NO_POWER;

	int minDiff = ComputeColorDiff( color, MapPowerToColor(NO_POWER) );
	for( int i = 0; i < PAINT_POWER_TYPE_COUNT; ++i )
	{
		const int diff = ComputeColorDiff( color, MapPowerToColor( i ) );
		if( diff < minDiff )
		{
			minDiff = diff;
			result = i;
		}
	}

	// Return the closest matching power, if the diff is small enough. Otherwise, return NO_POWER.
	return minDiff <= paint_color_max_diff.GetInt() ? static_cast<PaintPowerType>( result ) : NO_POWER;
}


PaintPowerType MapColorToPower( const color24& color )
{
	return MapColorToPower( Color( color.r, color.g, color.b ) );
}


PaintPowerType MapColorToPower( const CUtlVector<BYTE>& colors )
{
	// Find the hit count of each power for the colors in the array
	unsigned powerCount[PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER] = { 0 };
	PaintPowerType power;
	for ( int i = 0; i < colors.Count(); ++i )
	{
		power = static_cast< PaintPowerType >( colors[i] );
		AssertMsg( power >= 0 && power < PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER, "Invalid power type! This may cause out-of-bounds indexing!" );
		++powerCount[ power ];
	}

	// Find the power with the highest hit count, excluding no power
	power = NO_POWER;
	unsigned highestCount = 0;
	for ( int i = 0; i < PAINT_POWER_TYPE_COUNT; ++i )
	{
		if ( powerCount[i] > highestCount )
		{
			power = static_cast< PaintPowerType >( i );
			highestCount = powerCount[i];
		}
	}

	return power;
}


Color MapPowerToColor( int paintPowerType )
{
	AssertMsg( paintPowerType < PAINT_POWER_TYPE_COUNT_PLUS_NO_POWER, "Index out of bounds." );
	paintPowerType = MIN( paintPowerType, static_cast<int>( NO_POWER ) );

	switch ( paintPowerType )
	{
	case BOUNCE_POWER:
		return bounce_paint_color.GetColor();
	case SPEED_POWER:
		return speed_paint_color.GetColor();
	case REFLECT_POWER:
		return speed_paint_color.GetColor();// FIXME: Bring this back for DLC2 reflect_paint_color.GetColor();
	case PORTAL_POWER:
		return portal_paint_color.GetColor();
	default:
		return erase_color.GetColor();
	}
}

Color MapPowerToVisualColor( int paintPowerType )
{
	if( paintPowerType == NO_POWER )
	{
		return erase_visual_color.GetColor();
	}
	else
	{
		return MapPowerToColor( paintPowerType );
	}
}
