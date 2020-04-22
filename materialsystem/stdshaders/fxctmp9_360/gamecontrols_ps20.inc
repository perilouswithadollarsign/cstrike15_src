// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
#include "shaderlib/cshader.h"
class gamecontrols_ps20_Static_Index
{
public:
	// CONSTRUCTOR
	gamecontrols_ps20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_gamecontrols_ps20 0
class gamecontrols_ps20_Dynamic_Index
{
public:
	// CONSTRUCTOR
	gamecontrols_ps20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_gamecontrols_ps20 0
static const ShaderComboSemantics_t gamecontrols_ps20_combos =
{
	"gamecontrols_ps20", NULL, 0, NULL, 0 
};

class ConstructMe_gamecontrols_ps20
{
public:
	ConstructMe_gamecontrols_ps20()
	{
		GetShaderDLL()->AddShaderComboInformation( &gamecontrols_ps20_combos );
	}
};

static ConstructMe_gamecontrols_ps20 s_ConstructMe_gamecontrols_ps20;
