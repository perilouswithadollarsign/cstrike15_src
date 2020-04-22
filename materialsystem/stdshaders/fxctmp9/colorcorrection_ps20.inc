// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class colorcorrection_ps20_Static_Index
{
public:
	// CONSTRUCTOR
	colorcorrection_ps20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_colorcorrection_ps20 0
class colorcorrection_ps20_Dynamic_Index
{
private:
	int m_nNUM_LOOKUPS;
#ifdef _DEBUG
	bool m_bNUM_LOOKUPS;
#endif
public:
	void SetNUM_LOOKUPS( int i )
	{
		Assert( i >= 0 && i <= 4 );
		m_nNUM_LOOKUPS = i;
#ifdef _DEBUG
		m_bNUM_LOOKUPS = true;
#endif
	}
	void SetNUM_LOOKUPS( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 4 );
		m_nNUM_LOOKUPS = i ? 1 : 0;
#ifdef _DEBUG
		m_bNUM_LOOKUPS = true;
#endif
	}
public:
	// CONSTRUCTOR
	colorcorrection_ps20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bNUM_LOOKUPS = false;
#endif // _DEBUG
		m_nNUM_LOOKUPS = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bNUM_LOOKUPS;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nNUM_LOOKUPS ) + 0;
	}
};
#define shaderDynamicTest_colorcorrection_ps20 psh_forgot_to_set_dynamic_NUM_LOOKUPS + 0

static const ShaderComboInformation_t s_DynamicComboArray_colorcorrection_ps20[1] = 
{
	{ "NUM_LOOKUPS", 0, 4 },
};
static const ShaderComboSemantics_t colorcorrection_ps20_combos =
{
	"colorcorrection_ps20", s_DynamicComboArray_colorcorrection_ps20, 1, NULL, 0 
};

class ConstructMe_colorcorrection_ps20
{
public:
	ConstructMe_colorcorrection_ps20()
	{
		GetShaderDLL()->AddShaderComboInformation( &colorcorrection_ps20_combos );
	}
};

static ConstructMe_colorcorrection_ps20 s_ConstructMe_colorcorrection_ps20;
