// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class introscreenspaceeffect_ps20_Static_Index
{
public:
	// CONSTRUCTOR
	introscreenspaceeffect_ps20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_introscreenspaceeffect_ps20 0
class introscreenspaceeffect_ps20_Dynamic_Index
{
private:
	int m_nMODE;
#ifdef _DEBUG
	bool m_bMODE;
#endif
public:
	void SetMODE( int i )
	{
		Assert( i >= 0 && i <= 9 );
		m_nMODE = i;
#ifdef _DEBUG
		m_bMODE = true;
#endif
	}
	void SetMODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 9 );
		m_nMODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bMODE = true;
#endif
	}
public:
	// CONSTRUCTOR
	introscreenspaceeffect_ps20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bMODE = false;
#endif // _DEBUG
		m_nMODE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bMODE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nMODE ) + 0;
	}
};
#define shaderDynamicTest_introscreenspaceeffect_ps20 psh_forgot_to_set_dynamic_MODE + 0

static const ShaderComboInformation_t s_DynamicComboArray_IntroScreenSpaceEffect_ps20[1] = 
{
	{ "MODE", 0, 9 },
};
static const ShaderComboSemantics_t IntroScreenSpaceEffect_ps20_combos =
{
	"IntroScreenSpaceEffect_ps20", s_DynamicComboArray_IntroScreenSpaceEffect_ps20, 1, NULL, 0 
};

class ConstructMe_IntroScreenSpaceEffect_ps20
{
public:
	ConstructMe_IntroScreenSpaceEffect_ps20()
	{
		GetShaderDLL()->AddShaderComboInformation( &IntroScreenSpaceEffect_ps20_combos );
	}
};

static ConstructMe_IntroScreenSpaceEffect_ps20 s_ConstructMe_IntroScreenSpaceEffect_ps20;
