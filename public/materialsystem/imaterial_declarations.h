//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
#ifndef IMATERIAL_DECLARATIONS_HDR 
#define IMATERIAL_DECLARATIONS_HDR 

#include "materialsystem/imaterialsystemhardwareconfig_declarations.h"

//-----------------------------------------------------------------------------
// Shader state flags can be read from the FLAGS materialvar
// Also can be read or written to with the Set/GetMaterialVarFlags() call
// Also make sure you add/remove a string associated with each flag below to CShaderSystem::ShaderStateString in ShaderSystem.cpp
//-----------------------------------------------------------------------------
enum MaterialVarFlags_t
{
	MATERIAL_VAR_DEBUG					  = (1 << 0),
	MATERIAL_VAR_NO_DEBUG_OVERRIDE		  = (1 << 1),
	MATERIAL_VAR_NO_DRAW				  = (1 << 2),
	MATERIAL_VAR_USE_IN_FILLRATE_MODE	  = (1 << 3),

	MATERIAL_VAR_VERTEXCOLOR			  = (1 << 4),
	MATERIAL_VAR_VERTEXALPHA			  = (1 << 5),
	MATERIAL_VAR_SELFILLUM				  = (1 << 6),
	MATERIAL_VAR_ADDITIVE				  = (1 << 7),
	MATERIAL_VAR_ALPHATEST				  = (1 << 8),
#if defined( _PS3 ) || defined SPU
	// This Material flag is specifically for the final bloom pass only (the bloom add pass).
	// We need to disable alpha writes for 2x MSAA otherwise this pass will cause a heavy
	// blurring artifact to appear. It's only ever used in bloomadd_ps3.vmt, a modified version
	// of bloomadd.vmt for PS3 only - Jawad.
	MATERIAL_VAR_NOALPHAWRITES            = (1 << 30),
#endif //_PS3
	MATERIAL_VAR_ZNEARER				  = (1 << 10),
	MATERIAL_VAR_MODEL					  = (1 << 11),
	MATERIAL_VAR_FLAT					  = (1 << 12),
	MATERIAL_VAR_NOCULL					  = (1 << 13),
	MATERIAL_VAR_NOFOG					  = (1 << 14),
	MATERIAL_VAR_IGNOREZ				  = (1 << 15),
	MATERIAL_VAR_DECAL					  = (1 << 16),
	MATERIAL_VAR_ENVMAPSPHERE			  = (1 << 17), // OBSOLETE
//	MATERIAL_VAR_UNUSED					  = (1 << 18),
	MATERIAL_VAR_ENVMAPCAMERASPACE	      = (1 << 19), // OBSOLETE
	MATERIAL_VAR_BASEALPHAENVMAPMASK	  = (1 << 20),
	MATERIAL_VAR_TRANSLUCENT              = (1 << 21),
	MATERIAL_VAR_NORMALMAPALPHAENVMAPMASK = (1 << 22),
	MATERIAL_VAR_NEEDS_SOFTWARE_SKINNING  = (1 << 23), // OBSOLETE
	MATERIAL_VAR_OPAQUETEXTURE			  = (1 << 24),
	MATERIAL_VAR_MULTIPLY				  = (1 << 25),
	MATERIAL_VAR_SUPPRESS_DECALS		  = (1 << 26),
	MATERIAL_VAR_HALFLAMBERT			  = (1 << 27),
	MATERIAL_VAR_WIREFRAME                = (1 << 28),
	MATERIAL_VAR_ALLOWALPHATOCOVERAGE     = (1 << 29),
	MATERIAL_VAR_ALPHA_MODIFIED_BY_PROXY  = (1 << 30),
	MATERIAL_VAR_VERTEXFOG				  = (1 << 31),

	// NOTE: Only add flags here that either should be read from
	// .vmts or can be set directly from client code. Other, internal
	// flags should to into the flag enum in IMaterialInternal.h
};


//-----------------------------------------------------------------------------
// Internal flags not accessible from outside the material system. Stored in Flags2
//-----------------------------------------------------------------------------
enum MaterialVarFlags2_t
{
	// NOTE: These are for $flags2!!!!!
//	UNUSED											= (1 << 0),

	MATERIAL_VAR2_LIGHTING_UNLIT					= 0,
	MATERIAL_VAR2_LIGHTING_VERTEX_LIT				= (1 << 1),
	MATERIAL_VAR2_LIGHTING_LIGHTMAP					= (1 << 2),
	MATERIAL_VAR2_LIGHTING_BUMPED_LIGHTMAP			= (1 << 3),
	MATERIAL_VAR2_LIGHTING_MASK						= 
		( MATERIAL_VAR2_LIGHTING_VERTEX_LIT | 
		  MATERIAL_VAR2_LIGHTING_LIGHTMAP | 
		  MATERIAL_VAR2_LIGHTING_BUMPED_LIGHTMAP ),

	// FIXME: Should this be a part of the above lighting enums?
	MATERIAL_VAR2_DIFFUSE_BUMPMAPPED_MODEL					= (1 << 4),
	MATERIAL_VAR2_USES_ENV_CUBEMAP							= (1 << 5),
	MATERIAL_VAR2_NEEDS_TANGENT_SPACES						= (1 << 6),
	MATERIAL_VAR2_NEEDS_SOFTWARE_LIGHTING					= (1 << 7),
	// GR - HDR path puts lightmap alpha in separate texture...
	MATERIAL_VAR2_BLEND_WITH_LIGHTMAP_ALPHA					= (1 << 8),
	MATERIAL_VAR2_NEEDS_BAKED_LIGHTING_SNAPSHOTS			= (1 << 9),
	MATERIAL_VAR2_USE_FLASHLIGHT							= (1 << 10),
	MATERIAL_VAR2_USE_FIXED_FUNCTION_BAKED_LIGHTING			= (1 << 11),
	MATERIAL_VAR2_NEEDS_FIXED_FUNCTION_FLASHLIGHT			= (1 << 12),
	MATERIAL_VAR2_USE_EDITOR								= (1 << 13),
	MATERIAL_VAR2_NEEDS_POWER_OF_TWO_FRAME_BUFFER_TEXTURE	= (1 << 14),
	MATERIAL_VAR2_NEEDS_FULL_FRAME_BUFFER_TEXTURE			= (1 << 15),
	MATERIAL_VAR2_IS_SPRITECARD								= (1 << 16),
	MATERIAL_VAR2_USES_VERTEXID								= (1 << 17),
	MATERIAL_VAR2_SUPPORTS_HW_SKINNING						= (1 << 18),
	MATERIAL_VAR2_SUPPORTS_FLASHLIGHT						= (1 << 19),
	MATERIAL_VAR2_USE_GBUFFER0                              = (1 << 20),
	MATERIAL_VAR2_USE_GBUFFER1                              = (1 << 21),
	MATERIAL_VAR2_SELFILLUMMASK								= (1 << 22),
	MATERIAL_VAR2_SUPPORTS_TESSELLATION						= (1 << 23),

	// Support for types of vertex compression:
	MATERIAL_VAR2_SUPPORTS_VERTEX_COMPRESSION_BIT			= 26,
	MATERIAL_VAR2_SUPPORTS_VERTEX_COMPRESSION_FULL			= ( VERTEX_COMPRESSION_FULL << MATERIAL_VAR2_SUPPORTS_VERTEX_COMPRESSION_BIT ),
	MATERIAL_VAR2_SUPPORTS_VERTEX_COMPRESSION_NOUV			= ( VERTEX_COMPRESSION_NOUV << MATERIAL_VAR2_SUPPORTS_VERTEX_COMPRESSION_BIT ), 
	MATERIAL_VAR2_SUPPORTS_VERTEX_COMPRESSION_MASK			= 
	( MATERIAL_VAR2_SUPPORTS_VERTEX_COMPRESSION_FULL |
	MATERIAL_VAR2_SUPPORTS_VERTEX_COMPRESSION_NOUV ),
};

#endif