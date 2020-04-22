//===== Copyright © 1996-2007, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
// This file defines a number of constants and structured which are used to build up a command
// buffer to pass to ShaderAPI for state setting and other operations.  Since the prupose of these
// command buffers is to minimize and optimize calls into shaderapi, their structure is not
// abstract - they are built out by the calling process.
//
//===========================================================================//

#ifndef COMMANDBUFFER_H
#define COMMANDBUFFER_H

#ifdef _WIN32
#pragma once
#endif

#ifdef _PS3
#define CBCMD_MAX_PS3TEX 8			// Max PS3 textures set in an ECB
#endif

//-----------------------------------------------------------------------------
// Commands used by the per-pass command buffers
//-----------------------------------------------------------------------------
enum CommandBufferCommand_t
{
	// flow control commands.
	CBCMD_END = 0,									// end of stream
	CBCMD_JUMP,										// int cmd, void *adr. jump to another
													// stream. Can be used to implement
													// non-sequentially allocated storage
	CBCMD_JSR,										// int cmd, void *adr. subroutine call to another stream.

#ifdef _PS3
	CBCMD_PS3TEX,									// Textures. This command stores pointers to the BIND_TEXTURE commands
													// Will fill in the Bind Texture commands just before issueing the ECB to the SPU
													// This better emulates what happens on DX platforms, and so allows the weaponcache to function
													// whilst still allowing the VRAM defrag feature om PS3
	CBCMD_LENGTH,									// Length of command buffer
#endif
	// constant setting commands
	CBCMD_SET_PIXEL_SHADER_FLOAT_CONST,				// int cmd,int first_reg, int nregs, float values[nregs*4]


	CBCMD_SET_VERTEX_SHADER_FLOAT_CONST,			// int cmd,int first_reg, int nregs, float values[nregs*4]
	CBCMD_SET_VERTEX_SHADER_FLOAT_CONST_REF,		// int cmd,int first_reg, int nregs, &float values[nregs*4]
	CBCMD_SETPIXELSHADERFOGPARAMS,					// int cmd, int regdest
	CBCMD_STORE_EYE_POS_IN_PSCONST,					// int cmd, int regdest
	CBCMD_SET_DEPTH_FEATHERING_CONST,				// int cmd, int constant register, float blend scale

	// texture binding. sampler indices have TEXTURECMD_BINDFLAGS_xxx flags OR'd into them.

	CBCMD_BIND_STANDARD_TEXTURE,					// cmd, sampler, texture id
	CBCMD_BIND_SHADERAPI_TEXTURE_HANDLE,			// cmd, sampler, texture handle
#ifdef _PS3
	CBCMD_BIND_PS3_TEXTURE,							// cmd, CPs3BindTexture_t
	CBCMD_BIND_PS3_STANDARD_TEXTURE,				// cmd, idx
#endif

	// shaders
	CBCMD_SET_PSHINDEX,								// cmd, idx
	CBCMD_SET_VSHINDEX,								// cmd, idx

	CBCMD_SET_VERTEX_SHADER_FLASHLIGHT_STATE,		// cmd, int first_reg (for worldToTexture matrix)
	CBCMD_SET_PIXEL_SHADER_FLASHLIGHT_STATE,		// cmd, int color reg, int atten reg, int origin reg, sampler (for flashlight texture)

	CBCMD_SET_PIXEL_SHADER_UBERLIGHT_STATE,			// cmd

	CBCMD_SET_VERTEX_SHADER_NEARZFARZ_STATE,		// cmd
};

//-----------------------------------------------------------------------------
// Commands used by the per-instance command buffer
// NOTE: If you add commands, you probably want to change the size of 
// CInstanceStorageBuffer and/or the choice of making it a fixed-size allocation
// see shaderlib/baseshader.*
//
// FIXME!! NOTE that this whole scheme here generates a dependency of the
// shaders on internal guts of shaderapidx8, since it's responsible for
// setting various pixel shader + vertex shader constants based on the
// commands below. We need to remove this dependency as it's way too restrictive
// and puts the smarts in the wrong place (see CBICMD_SETPIXELSHADERGLINTDAMPING
// as an example). Not going to solve this for l4d though, as I don't anticipate
// a large amount of new shader writing for that product.
//-----------------------------------------------------------------------------
enum CommandBufferInstanceCommand_t
{
	CBICMD_END = 0,										// end of stream
	CBICMD_JUMP,										// int cmd, void *adr. jump to another
	// stream. Can be used to implement
	// non-sequentially allocated storage
	CBICMD_JSR,											// int cmd, void *adr. subroutine call to another stream.

	CBICMD_SETSKINNINGMATRICES,							// int cmd

	CBICMD_SETVERTEXSHADERLOCALLIGHTING,				// int cmd
	CBICMD_SETPIXELSHADERLOCALLIGHTING,					// int cmd, int regdest
	CBICMD_SETVERTEXSHADERAMBIENTLIGHTCUBE,				// int cmd
	CBICMD_SETPIXELSHADERAMBIENTLIGHTCUBE,				// int cmd, int regdest
	CBICMD_SETPIXELSHADERAMBIENTLIGHTCUBELUMINANCE,		// int cmd, int regdest
	CBICMD_SETPIXELSHADERGLINTDAMPING,					// int cmd, int regdest

	CBICMD_BIND_ENV_CUBEMAP_TEXTURE,					// cmd, sampler

	CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE,
	CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARCOLORSPACE_LINEARSCALE, // int cmd, int constant register, Vector color2, scale
	CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARCOLORSPACE,			// int cmd, int constant register, Vector4d( color2, 1.0 )
	CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARSCALE,				// int cmd, int constant register, Vector4d( color2, 1.0 ) float scale
	CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_LINEARSCALE_SCALEINW,		// int cmd, int constant register, Vector color2, float scale
	CBICMD_SETMODULATIONVERTEXSHADERDYNAMICSTATE,							// int cmd, int constant register, Vector color2
	CBICMD_SETMODULATIONPIXELSHADERDYNAMICSTATE_IDENTITY,					// int cmd, int constant register
	CBICMD_SETMODULATIONVERTEXSHADERDYNAMICSTATE_LINEARSCALE,				// int cmd, int constant register, Vector color2, float scale
	// This must be last
	CBICMD_COUNT,
};

#endif // COMMANDBUFFER_H
