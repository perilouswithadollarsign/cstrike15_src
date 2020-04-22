// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
#include "shaderlib/cshader.h"
class shadowmodel_ps20_Static_Index
{
public:
	// CONSTRUCTOR
	shadowmodel_ps20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_shadowmodel_ps20 0
class shadowmodel_ps20_Dynamic_Index
{
public:
	// CONSTRUCTOR
	shadowmodel_ps20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_shadowmodel_ps20 0
static const ShaderComboSemantics_t shadowmodel_ps20_combos =
{
	"shadowmodel_ps20", NULL, 0, NULL, 0 
};

class ConstructMe_shadowmodel_ps20
{
public:
	ConstructMe_shadowmodel_ps20()
	{
		GetShaderDLL()->AddShaderComboInformation( &shadowmodel_ps20_combos );
	}
};

static ConstructMe_shadowmodel_ps20 s_ConstructMe_shadowmodel_ps20;
