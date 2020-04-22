// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class motion_blur_ps20_Static_Index
{
public:
	// CONSTRUCTOR
	motion_blur_ps20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_motion_blur_ps20 0
class motion_blur_ps20_Dynamic_Index
{
private:
	int m_nD_NUM_BLUR_SAMPLES;
#ifdef _DEBUG
	bool m_bD_NUM_BLUR_SAMPLES;
#endif
public:
	void SetD_NUM_BLUR_SAMPLES( int i )
	{
		Assert( i >= 0 && i <= 14 );
		m_nD_NUM_BLUR_SAMPLES = i;
#ifdef _DEBUG
		m_bD_NUM_BLUR_SAMPLES = true;
#endif
	}
	void SetD_NUM_BLUR_SAMPLES( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 14 );
		m_nD_NUM_BLUR_SAMPLES = i ? 1 : 0;
#ifdef _DEBUG
		m_bD_NUM_BLUR_SAMPLES = true;
#endif
	}
public:
	// CONSTRUCTOR
	motion_blur_ps20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bD_NUM_BLUR_SAMPLES = false;
#endif // _DEBUG
		m_nD_NUM_BLUR_SAMPLES = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bD_NUM_BLUR_SAMPLES;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nD_NUM_BLUR_SAMPLES ) + 0;
	}
};
#define shaderDynamicTest_motion_blur_ps20 psh_forgot_to_set_dynamic_D_NUM_BLUR_SAMPLES + 0

static const ShaderComboInformation_t s_DynamicComboArray_motion_blur_ps20[1] = 
{
	{ "D_NUM_BLUR_SAMPLES", 0, 14 },
};
static const ShaderComboSemantics_t motion_blur_ps20_combos =
{
	"motion_blur_ps20", s_DynamicComboArray_motion_blur_ps20, 1, NULL, 0 
};

class ConstructMe_motion_blur_ps20
{
public:
	ConstructMe_motion_blur_ps20()
	{
		GetShaderDLL()->AddShaderComboInformation( &motion_blur_ps20_combos );
	}
};

static ConstructMe_motion_blur_ps20 s_ConstructMe_motion_blur_ps20;
