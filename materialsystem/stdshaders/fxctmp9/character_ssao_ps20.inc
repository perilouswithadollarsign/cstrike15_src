// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class character_ssao_ps20_Static_Index
{
private:
	int m_nQUALITY_MODE;
#ifdef _DEBUG
	bool m_bQUALITY_MODE;
#endif
public:
	void SetQUALITY_MODE( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nQUALITY_MODE = i;
#ifdef _DEBUG
		m_bQUALITY_MODE = true;
#endif
	}
	void SetQUALITY_MODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nQUALITY_MODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bQUALITY_MODE = true;
#endif
	}
public:
	// CONSTRUCTOR
	character_ssao_ps20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bQUALITY_MODE = false;
#endif // _DEBUG
		m_nQUALITY_MODE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bQUALITY_MODE;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nQUALITY_MODE ) + 0;
	}
};
#define shaderStaticTest_character_ssao_ps20 psh_forgot_to_set_static_QUALITY_MODE + 0
class character_ssao_ps20_Dynamic_Index
{
public:
	// CONSTRUCTOR
	character_ssao_ps20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_character_ssao_ps20 0

static const ShaderComboInformation_t s_StaticComboArray_character_ssao_ps20[1] = 
{
	{ "QUALITY_MODE", 0, 0 },
};
static const ShaderComboSemantics_t character_ssao_ps20_combos =
{
	"character_ssao_ps20", NULL, 0, s_StaticComboArray_character_ssao_ps20, 1 
};

class ConstructMe_character_ssao_ps20
{
public:
	ConstructMe_character_ssao_ps20()
	{
		GetShaderDLL()->AddShaderComboInformation( &character_ssao_ps20_combos );
	}
};

static ConstructMe_character_ssao_ps20 s_ConstructMe_character_ssao_ps20;
