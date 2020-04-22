// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
#include "shaderlib/cshader.h"
class blurgaussian_3x3_ps20b_Static_Index
{
public:
	// CONSTRUCTOR
	blurgaussian_3x3_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
#endif // _DEBUG
		return 0;
	}
};
#define shaderStaticTest_blurgaussian_3x3_ps20b 0
class blurgaussian_3x3_ps20b_Dynamic_Index
{
public:
	// CONSTRUCTOR
	blurgaussian_3x3_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
#endif // _DEBUG
		return 0;
	}
};
#define shaderDynamicTest_blurgaussian_3x3_ps20b 0
static const ShaderComboSemantics_t blurgaussian_3x3_ps20b_combos =
{
	"blurgaussian_3x3_ps20b", NULL, 0, NULL, 0 
};

class ConstructMe_blurgaussian_3x3_ps20b
{
public:
	ConstructMe_blurgaussian_3x3_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &blurgaussian_3x3_ps20b_combos );
	}
};

static ConstructMe_blurgaussian_3x3_ps20b s_ConstructMe_blurgaussian_3x3_ps20b;
