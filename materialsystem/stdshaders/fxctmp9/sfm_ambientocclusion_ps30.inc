// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class sfm_ambientocclusion_ps30_Static_Index
{
public:
	// CONSTRUCTOR
	sfm_ambientocclusion_ps30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_sfm_ambientocclusion_ps30 0
class sfm_ambientocclusion_ps30_Dynamic_Index
{
private:
	int m_nAO_MODE;
#ifdef _DEBUG
	bool m_bAO_MODE;
#endif
public:
	void SetAO_MODE( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nAO_MODE = i;
#ifdef _DEBUG
		m_bAO_MODE = true;
#endif
	}
	void SetAO_MODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 2 );
		m_nAO_MODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bAO_MODE = true;
#endif
	}
public:
	// CONSTRUCTOR
	sfm_ambientocclusion_ps30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bAO_MODE = false;
#endif // _DEBUG
		m_nAO_MODE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bAO_MODE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nAO_MODE ) + 0;
	}
};
#define shaderDynamicTest_sfm_ambientocclusion_ps30 psh_forgot_to_set_dynamic_AO_MODE + 0

static const ShaderComboInformation_t s_DynamicComboArray_sfm_ambientocclusion_ps30[1] = 
{
	{ "AO_MODE", 0, 2 },
};
static const ShaderComboSemantics_t sfm_ambientocclusion_ps30_combos =
{
	"sfm_ambientocclusion_ps30", s_DynamicComboArray_sfm_ambientocclusion_ps30, 1, NULL, 0 
};

class ConstructMe_sfm_ambientocclusion_ps30
{
public:
	ConstructMe_sfm_ambientocclusion_ps30()
	{
		GetShaderDLL()->AddShaderComboInformation( &sfm_ambientocclusion_ps30_combos );
	}
};

static ConstructMe_sfm_ambientocclusion_ps30 s_ConstructMe_sfm_ambientocclusion_ps30;
