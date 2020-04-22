// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class morphweight_vs30_Static_Index
{
public:
	// CONSTRUCTOR
	morphweight_vs30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_morphweight_vs30 0
class morphweight_vs30_Dynamic_Index
{
public:
	// CONSTRUCTOR
	morphweight_vs30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_morphweight_vs30 0
static const ShaderComboSemantics_t morphweight_vs30_combos =
{
	"morphweight_vs30", NULL, 0, NULL, 0 
};

class ConstructMe_morphweight_vs30
{
public:
	ConstructMe_morphweight_vs30()
	{
		GetShaderDLL()->AddShaderComboInformation( &morphweight_vs30_combos );
	}
};

static ConstructMe_morphweight_vs30 s_ConstructMe_morphweight_vs30;
