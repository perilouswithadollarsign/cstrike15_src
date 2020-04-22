// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class sfm_integercombine_ps20_Static_Index
{
public:
	// CONSTRUCTOR
	sfm_integercombine_ps20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_sfm_integercombine_ps20 0
class sfm_integercombine_ps20_Dynamic_Index
{
private:
	int m_nISFLOAT;
#ifdef _DEBUG
	bool m_bISFLOAT;
#endif
public:
	void SetISFLOAT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nISFLOAT = i;
#ifdef _DEBUG
		m_bISFLOAT = true;
#endif
	}
	void SetISFLOAT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nISFLOAT = i ? 1 : 0;
#ifdef _DEBUG
		m_bISFLOAT = true;
#endif
	}
public:
	// CONSTRUCTOR
	sfm_integercombine_ps20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bISFLOAT = false;
#endif // _DEBUG
		m_nISFLOAT = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bISFLOAT;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nISFLOAT ) + 0;
	}
};
#define shaderDynamicTest_sfm_integercombine_ps20 psh_forgot_to_set_dynamic_ISFLOAT + 0

static const ShaderComboInformation_t s_DynamicComboArray_sfm_integercombine_ps20[1] = 
{
	{ "ISFLOAT", 0, 1 },
};
static const ShaderComboSemantics_t sfm_integercombine_ps20_combos =
{
	"sfm_integercombine_ps20", s_DynamicComboArray_sfm_integercombine_ps20, 1, NULL, 0 
};

class ConstructMe_sfm_integercombine_ps20
{
public:
	ConstructMe_sfm_integercombine_ps20()
	{
		GetShaderDLL()->AddShaderComboInformation( &sfm_integercombine_ps20_combos );
	}
};

static ConstructMe_sfm_integercombine_ps20 s_ConstructMe_sfm_integercombine_ps20;
