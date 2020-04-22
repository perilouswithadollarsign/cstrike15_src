// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class sky_vs20_Static_Index
{
public:
	// CONSTRUCTOR
	sky_vs20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_sky_vs20 0
class sky_vs20_Dynamic_Index
{
public:
	// CONSTRUCTOR
	sky_vs20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_sky_vs20 0
static const ShaderComboSemantics_t sky_vs20_combos =
{
	"sky_vs20", NULL, 0, NULL, 0 
};

class ConstructMe_sky_vs20
{
public:
	ConstructMe_sky_vs20()
	{
		GetShaderDLL()->AddShaderComboInformation( &sky_vs20_combos );
	}
};

static ConstructMe_sky_vs20 s_ConstructMe_sky_vs20;
