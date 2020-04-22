// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class parallaxtest_vs30_Static_Index
{
public:
	// CONSTRUCTOR
	parallaxtest_vs30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_parallaxtest_vs30 0
class parallaxtest_vs30_Dynamic_Index
{
public:
	// CONSTRUCTOR
	parallaxtest_vs30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_parallaxtest_vs30 0
static const ShaderComboSemantics_t parallaxtest_vs30_combos =
{
	"parallaxtest_vs30", NULL, 0, NULL, 0 
};

class ConstructMe_parallaxtest_vs30
{
public:
	ConstructMe_parallaxtest_vs30()
	{
		GetShaderDLL()->AddShaderComboInformation( &parallaxtest_vs30_combos );
	}
};

static ConstructMe_parallaxtest_vs30 s_ConstructMe_parallaxtest_vs30;
