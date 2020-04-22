//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef DECALS_H
#define DECALS_H
#ifdef _WIN32
#pragma once
#endif

// NOTE: If you add a tex type, be sure to modify the s_pImpactEffect
// array in fx_impact.cpp to get an effect when that surface is shot.
#define CHAR_TEX_ANTLION		'A'
#define CHAR_TEX_BLOODYFLESH	'B'
#define	CHAR_TEX_CONCRETE		'C'
#define CHAR_TEX_DIRT			'D'
#define CHAR_TEX_EGGSHELL		'E' ///< the egg sacs in the tunnels in ep2.
#define CHAR_TEX_FLESH			'F'
#define CHAR_TEX_GRATE			'G'
#define CHAR_TEX_ALIENFLESH		'H'
#define CHAR_TEX_CLIP			'I'
#define CHAR_TEX_GRASS			'J'	// L4D addition
#define CHAR_TEX_SNOW			'K'
#define CHAR_TEX_PLASTIC		'L'
#define CHAR_TEX_METAL			'M'
#define CHAR_TEX_SAND			'N'
#define CHAR_TEX_FOLIAGE		'O'
#define CHAR_TEX_COMPUTER		'P'
#define CHAR_TEX_ASPHALT		'Q'	// L4D addition
#define CHAR_TEX_BRICK			'R'	// L4D addition
#define CHAR_TEX_SLOSH			'S'
#define CHAR_TEX_TILE			'T'
#define CHAR_TEX_CARDBOARD		'U'	// L4D addition
#define CHAR_TEX_VENT			'V'
#define CHAR_TEX_WOOD			'W'
//#define CHAR_TEX_UNUSED		'X' ///< do not use - "fake" materials use this (ladders, wading, clips, etc)
#define CHAR_TEX_GLASS			'Y'
#define CHAR_TEX_WARPSHIELD		'Z' ///< wierd-looking jello effect for advisor shield.

#define FIRST_CHAR_TEX			CHAR_TEX_ANTLION
#define LAST_CHAR_TEX			CHAR_TEX_WARPSHIELD

#define CHAR_TEX_CLAY			1 	// L4D addition
#define CHAR_TEX_PLASTER		2	// L4D addition
#define CHAR_TEX_ROCK			3	// L4D addition
#define CHAR_TEX_RUBBER			4	// L4D addition
#define CHAR_TEX_SHEETROCK		5	// L4D addition
#define CHAR_TEX_CLOTH			6	// L4D addition
#define CHAR_TEX_CARPET			7	// L4D addition
#define CHAR_TEX_PAPER			8	// L4D addition
#define CHAR_TEX_UPHOLSTERY		9	// L4D addition
#define CHAR_TEX_PUDDLE			10	// L4D addition
#define CHAR_TEX_MUD			11	// L4D addition

#define CHAR_TEX_SANDBARREL		12	// CSGO addition (no penetration) - sand filled barrel

#define FIRST_L4D_CHAR_TEX		CHAR_TEX_CLAY
#define LAST_L4D_CHAR_TEX		CHAR_TEX_SANDBARREL

abstract_class IDecalEmitterSystem
{
public:
	virtual int	GetDecalIndexForName( char const *decalname ) = 0;
	virtual char const *TranslateDecalForGameMaterial( char const *decalName, unsigned char gamematerial ) = 0;
};

extern IDecalEmitterSystem *decalsystem;

#endif // DECALS_H
