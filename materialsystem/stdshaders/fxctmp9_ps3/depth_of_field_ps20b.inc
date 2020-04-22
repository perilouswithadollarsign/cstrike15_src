// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class depth_of_field_ps20b_Static_Index
{
public:
	// CONSTRUCTOR
	depth_of_field_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_depth_of_field_ps20b 0
class depth_of_field_ps20b_Dynamic_Index
{
private:
	int m_nQUALITY;
#ifdef _DEBUG
	bool m_bQUALITY;
#endif
public:
	void SetQUALITY( int i )
	{
		Assert( i >= 0 && i <= 3 );
		m_nQUALITY = i;
#ifdef _DEBUG
		m_bQUALITY = true;
#endif
	}
	void SetQUALITY( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 3 );
		m_nQUALITY = i ? 1 : 0;
#ifdef _DEBUG
		m_bQUALITY = true;
#endif
	}
public:
	// CONSTRUCTOR
	depth_of_field_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bQUALITY = false;
#endif // _DEBUG
		m_nQUALITY = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bQUALITY;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nQUALITY ) + 0;
	}
};
#define shaderDynamicTest_depth_of_field_ps20b psh_forgot_to_set_dynamic_QUALITY + 0

static const ShaderComboInformation_t s_DynamicComboArray_depth_of_field_ps20b[1] = 
{
	{ "QUALITY", 0, 3 },
};
static const ShaderComboSemantics_t depth_of_field_ps20b_combos =
{
	"depth_of_field_ps20b", s_DynamicComboArray_depth_of_field_ps20b, 1, NULL, 0 
};

class ConstructMe_depth_of_field_ps20b
{
public:
	ConstructMe_depth_of_field_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &depth_of_field_ps20b_combos );
	}
};

static ConstructMe_depth_of_field_ps20b s_ConstructMe_depth_of_field_ps20b;
