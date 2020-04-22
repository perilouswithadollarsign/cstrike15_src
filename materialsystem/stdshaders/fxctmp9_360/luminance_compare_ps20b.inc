// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class luminance_compare_ps20b_Static_Index
{
private:
	int m_nPS3REGCOUNT12;
#ifdef _DEBUG
	bool m_bPS3REGCOUNT12;
#endif
public:
	void SetPS3REGCOUNT12( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nPS3REGCOUNT12 = i;
#ifdef _DEBUG
		m_bPS3REGCOUNT12 = true;
#endif
	}
	void SetPS3REGCOUNT12( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nPS3REGCOUNT12 = i ? 1 : 0;
#ifdef _DEBUG
		m_bPS3REGCOUNT12 = true;
#endif
	}
public:
	// CONSTRUCTOR
	luminance_compare_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bPS3REGCOUNT12 = false;
#endif // _DEBUG
		m_nPS3REGCOUNT12 = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bPS3REGCOUNT12;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nPS3REGCOUNT12 ) + 0;
	}
};
#define shaderStaticTest_luminance_compare_ps20b psh_forgot_to_set_static_PS3REGCOUNT12 + 0
class luminance_compare_ps20b_Dynamic_Index
{
public:
	// CONSTRUCTOR
	luminance_compare_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_luminance_compare_ps20b 0

static const ShaderComboInformation_t s_StaticComboArray_luminance_compare_ps20b[1] = 
{
	{ "PS3REGCOUNT12", 0, 0 },
};
static const ShaderComboSemantics_t luminance_compare_ps20b_combos =
{
	"luminance_compare_ps20b", NULL, 0, s_StaticComboArray_luminance_compare_ps20b, 1 
};

class ConstructMe_luminance_compare_ps20b
{
public:
	ConstructMe_luminance_compare_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &luminance_compare_ps20b_combos );
	}
};

static ConstructMe_luminance_compare_ps20b s_ConstructMe_luminance_compare_ps20b;
