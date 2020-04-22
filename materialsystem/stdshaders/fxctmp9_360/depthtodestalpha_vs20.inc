// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class depthtodestalpha_vs20_Static_Index
{
public:
	// CONSTRUCTOR
	depthtodestalpha_vs20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_depthtodestalpha_vs20 0
class depthtodestalpha_vs20_Dynamic_Index
{
public:
	// CONSTRUCTOR
	depthtodestalpha_vs20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_depthtodestalpha_vs20 0
static const ShaderComboSemantics_t depthtodestalpha_vs20_combos =
{
	"depthtodestalpha_vs20", NULL, 0, NULL, 0 
};

class ConstructMe_depthtodestalpha_vs20
{
public:
	ConstructMe_depthtodestalpha_vs20()
	{
		GetShaderDLL()->AddShaderComboInformation( &depthtodestalpha_vs20_combos );
	}
};

static ConstructMe_depthtodestalpha_vs20 s_ConstructMe_depthtodestalpha_vs20;
