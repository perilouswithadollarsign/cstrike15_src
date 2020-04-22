// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class eyeglint_vs20_Static_Index
{
public:
	// CONSTRUCTOR
	eyeglint_vs20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_eyeglint_vs20 0
class eyeglint_vs20_Dynamic_Index
{
public:
	// CONSTRUCTOR
	eyeglint_vs20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_eyeglint_vs20 0
static const ShaderComboSemantics_t eyeglint_vs20_combos =
{
	"eyeglint_vs20", NULL, 0, NULL, 0 
};

class ConstructMe_eyeglint_vs20
{
public:
	ConstructMe_eyeglint_vs20()
	{
		GetShaderDLL()->AddShaderComboInformation( &eyeglint_vs20_combos );
	}
};

static ConstructMe_eyeglint_vs20 s_ConstructMe_eyeglint_vs20;
