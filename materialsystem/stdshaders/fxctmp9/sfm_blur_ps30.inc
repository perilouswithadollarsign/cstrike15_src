// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class sfm_blur_ps30_Static_Index
{
public:
	// CONSTRUCTOR
	sfm_blur_ps30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_sfm_blur_ps30 0
class sfm_blur_ps30_Dynamic_Index
{
public:
	// CONSTRUCTOR
	sfm_blur_ps30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_sfm_blur_ps30 0
static const ShaderComboSemantics_t sfm_blur_ps30_combos =
{
	"sfm_blur_ps30", NULL, 0, NULL, 0 
};

class ConstructMe_sfm_blur_ps30
{
public:
	ConstructMe_sfm_blur_ps30()
	{
		GetShaderDLL()->AddShaderComboInformation( &sfm_blur_ps30_combos );
	}
};

static ConstructMe_sfm_blur_ps30 s_ConstructMe_sfm_blur_ps30;
