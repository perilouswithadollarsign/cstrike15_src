// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class rendertargetblit_ps20_Static_Index
{
public:
	// CONSTRUCTOR
	rendertargetblit_ps20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_rendertargetblit_ps20 0
class rendertargetblit_ps20_Dynamic_Index
{
public:
	// CONSTRUCTOR
	rendertargetblit_ps20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_rendertargetblit_ps20 0
static const ShaderComboSemantics_t rendertargetblit_ps20_combos =
{
	"rendertargetblit_ps20", NULL, 0, NULL, 0 
};

class ConstructMe_rendertargetblit_ps20
{
public:
	ConstructMe_rendertargetblit_ps20()
	{
		GetShaderDLL()->AddShaderComboInformation( &rendertargetblit_ps20_combos );
	}
};

static ConstructMe_rendertargetblit_ps20 s_ConstructMe_rendertargetblit_ps20;
