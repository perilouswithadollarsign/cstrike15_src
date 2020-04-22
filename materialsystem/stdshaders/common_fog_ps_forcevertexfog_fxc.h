//
// - This is the fog header to include for pixel shaders if the shader does support pixel-shader-blended vertex fog.
//

// -- PIXELFOGTYPE is 0 for RANGE FOG, 1 for WATER/HEIGHT FOG --
// DYNAMIC: "PIXELFOGTYPE"	"0..0" [ = 0; ] //( pShaderAPI->GetSceneFogMode() == MATERIAL_FOG_LINEAR_BELOW_FOG_Z ) ? 1 : 0 ] 

// -- DOPIXELFOG is 1 if the material has $vertexfog 0, or if the material forces pixel fog by default (lightmappedgeneric for instance.) --
// STATIC: "DOPIXELFOG" "0..0" [ = 0 ]

#if ( PIXELFOGTYPE == 1 )
	// No matter what shader model we are, we do pixel fog for water and don't use fixed function hardware fog blending.
	#undef DOPIXELFOG
	#define DOPIXELFOG 1
#endif

#if defined( SHADER_MODEL_PS_2_0 ) && ( PIXELFOGTYPE == 0 )
	#define HARDWAREFOGBLEND 1
#else
	#define HARDWAREFOGBLEND 0
#endif

// FIXME!  Will need to revisit this once we are doing water/height fog again.  Needs to be pixel for in the water case.
