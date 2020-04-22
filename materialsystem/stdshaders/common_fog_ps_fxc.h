//
// - This is the fog header to include for pixel shaders if the shader doesn't support pixel-shader-blended vertex fog.
//

// -- PIXELFOGTYPE is 0 for RANGE FOG, 1 for WATER/HEIGHT FOG --
// DYNAMIC: "PIXELFOGTYPE"	"0..1" [ = ( pShaderAPI->GetSceneFogMode() == MATERIAL_FOG_LINEAR_BELOW_FOG_Z ) ] 

#if ( PIXELFOGTYPE == 1 )
	// No matter what shader model we are, we do pixel fog for water.
	#define DOPIXELFOG 1
	#define HARDWAREFOGBLEND 0
#else
	#if defined( SHADER_MODEL_PS_2_0 )
		// Never do pixel for for ps20 (unless we are in water)
		#define DOPIXELFOG 0
		#define HARDWAREFOGBLEND 1
	#else
		// Never do vertex fog for >ps20
		#define DOPIXELFOG 1
		#define HARDWAREFOGBLEND 0
	#endif
#endif
