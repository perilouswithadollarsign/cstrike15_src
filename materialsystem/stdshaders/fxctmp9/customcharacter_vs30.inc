// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class customcharacter_vs30_Static_Index
{
public:
	// CONSTRUCTOR
	customcharacter_vs30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_customcharacter_vs30 0
class customcharacter_vs30_Dynamic_Index
{
public:
	// CONSTRUCTOR
	customcharacter_vs30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_customcharacter_vs30 0
static const ShaderComboSemantics_t customcharacter_vs30_combos =
{
	"customcharacter_vs30", NULL, 0, NULL, 0 
};

class ConstructMe_customcharacter_vs30
{
public:
	ConstructMe_customcharacter_vs30()
	{
		GetShaderDLL()->AddShaderComboInformation( &customcharacter_vs30_combos );
	}
};

static ConstructMe_customcharacter_vs30 s_ConstructMe_customcharacter_vs30;
