// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class introscreenspaceeffect_ps20b_Static_Index
{
private:
	int m_nLINEAR_TO_SRGB;
#ifdef _DEBUG
	bool m_bLINEAR_TO_SRGB;
#endif
public:
	void SetLINEAR_TO_SRGB( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nLINEAR_TO_SRGB = i;
#ifdef _DEBUG
		m_bLINEAR_TO_SRGB = true;
#endif
	}
	void SetLINEAR_TO_SRGB( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nLINEAR_TO_SRGB = i ? 1 : 0;
#ifdef _DEBUG
		m_bLINEAR_TO_SRGB = true;
#endif
	}
public:
	// CONSTRUCTOR
	introscreenspaceeffect_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bLINEAR_TO_SRGB = false;
#endif // _DEBUG
		m_nLINEAR_TO_SRGB = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bLINEAR_TO_SRGB;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 10 * m_nLINEAR_TO_SRGB ) + 0;
	}
};
#define shaderStaticTest_introscreenspaceeffect_ps20b psh_forgot_to_set_static_LINEAR_TO_SRGB + 0
class introscreenspaceeffect_ps20b_Dynamic_Index
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
	introscreenspaceeffect_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_introscreenspaceeffect_ps20b psh_forgot_to_set_dynamic_MODE + 0

static const ShaderComboInformation_t s_DynamicComboArray_IntroScreenSpaceEffect_ps20b[1] = 
{
	{ "MODE", 0, 9 },
};

static const ShaderComboInformation_t s_StaticComboArray_IntroScreenSpaceEffect_ps20b[1] = 
{
	{ "LINEAR_TO_SRGB", 0, 1 },
};
static const ShaderComboSemantics_t IntroScreenSpaceEffect_ps20b_combos =
{
	"IntroScreenSpaceEffect_ps20b", s_DynamicComboArray_IntroScreenSpaceEffect_ps20b, 1, s_StaticComboArray_IntroScreenSpaceEffect_ps20b, 1 
};

class ConstructMe_IntroScreenSpaceEffect_ps20b
{
public:
	ConstructMe_IntroScreenSpaceEffect_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &IntroScreenSpaceEffect_ps20b_combos );
	}
};

static ConstructMe_IntroScreenSpaceEffect_ps20b s_ConstructMe_IntroScreenSpaceEffect_ps20b;
