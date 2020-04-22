// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class floatcombine_ps20b_Static_Index
{
public:
	// CONSTRUCTOR
	floatcombine_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_floatcombine_ps20b 0
class floatcombine_ps20b_Dynamic_Index
{
public:
	// CONSTRUCTOR
	floatcombine_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_floatcombine_ps20b 0
static const ShaderComboSemantics_t floatcombine_ps20b_combos =
{
	"floatcombine_ps20b", NULL, 0, NULL, 0 
};

class ConstructMe_floatcombine_ps20b
{
public:
	ConstructMe_floatcombine_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &floatcombine_ps20b_combos );
	}
};

static ConstructMe_floatcombine_ps20b s_ConstructMe_floatcombine_ps20b;
