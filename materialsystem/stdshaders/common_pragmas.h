//========== Copyright (c) Valve Corporation, All rights reserved. ==========//
//
// Purpose: Common shader compiler pragmas
//
// $NoKeywords: $
//
//===========================================================================//
#ifndef COMMON_PRAGMAS_H_
#define COMMON_PRAGMAS_H_

//
// Validated shader models:
//
// SHADER_MODEL_VS_1_1
// SHADER_MODEL_VS_2_0
// SHADER_MODEL_VS_3_0
//
// SHADER_MODEL_PS_1_1
// SHADER_MODEL_PS_1_4
// SHADER_MODEL_PS_2_0
// SHADER_MODEL_PS_2_B
// SHADER_MODEL_PS_3_0
//
//
//
// Platforms:
//
//  PC
// _X360
//

// Special pragmas silencing common warnings
#pragma warning ( disable : 3557 ) // warning X3557: Loop only executes for N iteration(s), forcing loop to unroll
#pragma warning ( disable : 3595 ) // warning X3595: Microcode Compiler possible performance issue: pixel shader input semantic ___ is unused
#pragma warning ( disable : 3596 ) // warning X3596: Microcode Compiler possible performance issue: pixel shader input semantic ___ is unused
#pragma warning ( disable : 4702 ) // warning X4702: complement opportunity missed because input result WAS clamped from 0 to 1
#pragma warning ( disable : 3571 ) // warning X3571: pow(f, e) will not work for negative f, use abs(f) or conditionally handle negative values if you expect them
#pragma warning ( disable : 3206 ) // warning X3206: implicit truncation of vector type
#pragma warning ( disable : 3205 ) // warning X3205: conversion from larger type to smaller, possible loss of data
#pragma warning ( disable : 3576 ) // warning X3576: semantics in type overridden by variable/function or enclosing type

#endif //#ifndef COMMON_PRAGMAS_H_
