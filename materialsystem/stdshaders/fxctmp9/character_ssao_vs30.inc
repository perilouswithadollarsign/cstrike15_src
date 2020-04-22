// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class character_ssao_vs30_Static_Index
{
public:
	// CONSTRUCTOR
	character_ssao_vs30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_character_ssao_vs30 0
class character_ssao_vs30_Dynamic_Index
{
public:
	// CONSTRUCTOR
	character_ssao_vs30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_character_ssao_vs30 0
static const ShaderComboSemantics_t character_ssao_vs30_combos =
{
	"character_ssao_vs30", NULL, 0, NULL, 0 
};

class ConstructMe_character_ssao_vs30
{
public:
	ConstructMe_character_ssao_vs30()
	{
		GetShaderDLL()->AddShaderComboInformation( &character_ssao_vs30_combos );
	}
};

static ConstructMe_character_ssao_vs30 s_ConstructMe_character_ssao_vs30;
