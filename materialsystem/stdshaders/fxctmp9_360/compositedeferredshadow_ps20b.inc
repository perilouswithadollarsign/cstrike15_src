// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
#include "shaderlib/cshader.h"
class compositedeferredshadow_ps20b_Static_Index
{
public:
	// CONSTRUCTOR
	compositedeferredshadow_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_compositedeferredshadow_ps20b 0
class compositedeferredshadow_ps20b_Dynamic_Index
{
public:
	// CONSTRUCTOR
	compositedeferredshadow_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_compositedeferredshadow_ps20b 0
static const ShaderComboSemantics_t compositedeferredshadow_ps20b_combos =
{
	"compositedeferredshadow_ps20b", NULL, 0, NULL, 0 
};

class ConstructMe_compositedeferredshadow_ps20b
{
public:
	ConstructMe_compositedeferredshadow_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &compositedeferredshadow_ps20b_combos );
	}
};

static ConstructMe_compositedeferredshadow_ps20b s_ConstructMe_compositedeferredshadow_ps20b;
