//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef PROCEDURALPRESETS_H
#define PROCEDURALPRESETS_H
#ifdef _WIN32
#pragma once
#endif

// IMPORTANT - only add preset enums to the END, and NEVER remove or replace any!!!

// (alternatively, fix up every dmx file with presets)
// The proper solution is to change CDmePreset::m_nProceduralType to be a bool m_bIsProceduralType
// and have a non-attribute int as a cached value inferred from the preset's name

enum
{
	PROCEDURAL_PRESET_NOT = 0,
	PROCEDURAL_PRESET_DEFAULT_CROSSFADE,
	PROCEDURAL_PRESET_ZERO_CROSSFADE,
	PROCEDURAL_PRESET_HALF_CROSSFADE,
	PROCEDRUAL_PRESET_ONE_CROSSFADE,
	PROCEDURAL_PRESET_HEAD_CROSSFADE,
	PROCEDURAL_PRESET_IN_CROSSFADE,
	PROCEDURAL_PRESET_OUT_CROSSFADE,
	PROCEDURAL_PRESET_INOUT,
	PROCEDURAL_PRESET_REVEAL,
	PROCEDURAL_PRESET_PASTE,
	PROCEDURAL_PRESET_DROP_LAYER, // Provides a blend value for finishing the current modification layer
	PROCEDURAL_PRESET_JITTER,
	PROCEDURAL_PRESET_SMOOTH,
	PROCEDURAL_PRESET_SHARPEN,
	PROCEDURAL_PRESET_SOFTEN,
	PROCEDURAL_PRESET_STAGGER,
	PROCEDURAL_PRESET_HOLD,  // Pushes time samples in falloff toward the green selected region
	PROCEDURAL_PRESET_RELEASE, // Pushes time samples in falloff toward the edges of the falloff
	PROCEDURAL_PRESET_STEADY,  // Smooths the "velocity" of samples in the time selection
	PROCEDURAL_PRESET_SPLINE,

	// Must be last
	NUM_PROCEDURAL_PRESET_TYPES,
};

static const char *g_ProceduralPresetNames[ NUM_PROCEDURAL_PRESET_TYPES ] =
{
	"NotProcedural!!!",
	"Default",
	"Zero",
	"Half",
	"One",
	"Head",
	"In",
	"Out",
	"Ramp",
	"Reveal",
	"Paste",
	"Drop",
	"Jitter",
	"Smooth",
	"Sharpen",
	"Soften",
	"Stagger",
	"Hold",
	"Release",
	"Steady",
	"Spline",
};

#define PROCEDURAL_PRESET_GROUP_NAME "Procedural"

inline char const *GetProceduralPresetName( int nPresetType )
{
	if ( nPresetType < PROCEDURAL_PRESET_NOT || nPresetType >= NUM_PROCEDURAL_PRESET_TYPES )
		return "???";
	return g_ProceduralPresetNames[ nPresetType ];
}

inline int ProceduralTypeForPresetName( const char *pPresetName )
{
	for ( int i = PROCEDURAL_PRESET_NOT; i < NUM_PROCEDURAL_PRESET_TYPES; ++i )
	{
		if ( !V_stricmp( pPresetName, g_ProceduralPresetNames[ i ] ) )
			return i;
	}
	return -1;
}

// Does preset blending only affect timing of samples?
inline bool IsPresetTimeOperation( int nPresetType )
{
	switch ( nPresetType )
	{
	default:
		break;
	case PROCEDURAL_PRESET_STAGGER:
	case PROCEDURAL_PRESET_STEADY:
	case PROCEDURAL_PRESET_HOLD:
	case PROCEDURAL_PRESET_RELEASE:
		return true;
	}

	return false;
}

// Does the preset blend towards a single value
inline bool IsPresetStaticValue( int nPresetType )
{
	switch ( nPresetType )
	{
		case PROCEDURAL_PRESET_NOT:
		case PROCEDURAL_PRESET_DEFAULT_CROSSFADE:
		case PROCEDURAL_PRESET_ZERO_CROSSFADE:
		case PROCEDURAL_PRESET_HALF_CROSSFADE:
		case PROCEDRUAL_PRESET_ONE_CROSSFADE:	
			return true;
		default:
			return false;
	}

	return false;
}

// Should area under time selection be "resampled" into a smooth curve worth of samples before
//  the preset operations?
inline bool ShouldPresetPreserveSamples( int nPresetType )
{
	switch ( nPresetType )
	{
	default:
		break;
	case PROCEDURAL_PRESET_STAGGER:
	case PROCEDURAL_PRESET_STEADY:
		return true;
	}

	return false;
}

#endif // PROCEDURALPRESETS_H
