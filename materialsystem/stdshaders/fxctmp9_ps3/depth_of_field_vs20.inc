// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class depth_of_field_vs20_Static_Index
{
public:
	// CONSTRUCTOR
	depth_of_field_vs20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_depth_of_field_vs20 0
class depth_of_field_vs20_Dynamic_Index
{
public:
	// CONSTRUCTOR
	depth_of_field_vs20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_depth_of_field_vs20 0
static const ShaderComboSemantics_t depth_of_field_vs20_combos =
{
	"depth_of_field_vs20", NULL, 0, NULL, 0 
};

class ConstructMe_depth_of_field_vs20
{
public:
	ConstructMe_depth_of_field_vs20()
	{
		GetShaderDLL()->AddShaderComboInformation( &depth_of_field_vs20_combos );
	}
};

static ConstructMe_depth_of_field_vs20 s_ConstructMe_depth_of_field_vs20;
