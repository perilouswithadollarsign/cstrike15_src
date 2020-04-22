// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
#include "shaderlib/cshader.h"
class clearalpha_ps20b_Static_Index
{
public:
	// CONSTRUCTOR
	clearalpha_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_clearalpha_ps20b 0
class clearalpha_ps20b_Dynamic_Index
{
public:
	// CONSTRUCTOR
	clearalpha_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_clearalpha_ps20b 0
static const ShaderComboSemantics_t clearalpha_ps20b_combos =
{
	"clearalpha_ps20b", NULL, 0, NULL, 0 
};

class ConstructMe_clearalpha_ps20b
{
public:
	ConstructMe_clearalpha_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &clearalpha_ps20b_combos );
	}
};

static ConstructMe_clearalpha_ps20b s_ConstructMe_clearalpha_ps20b;
