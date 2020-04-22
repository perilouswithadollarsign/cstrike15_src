//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef DEMO_POLISH_H
#define DEMO_POLISH_H
#ifdef _WIN32
#pragma once
#endif

//------------------------------------------------------------------------------------------------------------------------

#include "demo_polish/demo_polish_recorder.h"
#include "demo_polish/demo_polish_controller.h"

//------------------------------------------------------------------------------------------------------------------------

bool IsDemoPolishEnabled();
bool IsDemoPolishRecording();
bool IsDemoPolishPlaying();

bool DemoPolish_ShouldReplaceRoot( int iEntIndex );

void DemoPolish_Think();

CDemoPolishRecorder&	DemoPolish_GetRecorder();
CDemoPolishController&	DemoPolish_GetController();

//------------------------------------------------------------------------------------------------------------------------

extern ConVar demo_polish_auto_polish;
extern ConVar demo_polish_bone_test_index;
extern ConVar demo_polish_root_adjustments_enabled;
extern ConVar demo_polish_global_adjustments_enabled;
extern ConVar demo_polish_local_adjustments_enabled;
extern ConVar demo_polish_bone_overrides_enabled;
extern ConVar demo_polish_leaning_enabled;
extern ConVar demo_polish_leaning_strafe_kneebend_enabled;
extern ConVar demo_polish_ik_enabled;
extern ConVar demo_polish_draw_path_enabled;
extern ConVar demo_polish_step_in_place_enabled;
extern ConVar demo_polish_pelvis_noise_enabled;
extern ConVar demo_polish_terrain_adjust_enabled;
extern ConVar demo_polish_foot_plants_enabled;
extern ConVar demo_polish_draw_skeleton;
extern ConVar demo_polish_draw_prev_skeleton_frames;
extern ConVar demo_polish_draw_next_skeleton_frames;
extern ConVar demo_polish_path_frames;					// # of path frames to draw before/after current frame

//------------------------------------------------------------------------------------------------------------------------

//
// One day this could go in a math library - this is very similar to FLerp() except it does a bounds check and the params
// are in a different order.
//
inline float LerpScale( float flInLo, float flInHi, float flOutLo, float flOutHi, float flX )
{
	float const tt = MIN( 1, MAX( 0, (flX - flInLo) / (flInHi - flInLo) ) );
	return Lerp(tt, flOutLo, flOutHi);
}

//------------------------------------------------------------------------------------------------------------------------

#endif // DEMO_POLISH_H
