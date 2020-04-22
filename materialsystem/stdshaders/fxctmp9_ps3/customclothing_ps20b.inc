// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $STYLE == 0 )
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class customclothing_ps20b_Static_Index
{
private:
	int m_nSTYLE;
#ifdef _DEBUG
	bool m_bSTYLE;
#endif
public:
	void SetSTYLE( int i )
	{
		Assert( i >= 0 && i <= 6 );
		m_nSTYLE = i;
#ifdef _DEBUG
		m_bSTYLE = true;
#endif
	}
	void SetSTYLE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 6 );
		m_nSTYLE = i ? 1 : 0;
#ifdef _DEBUG
		m_bSTYLE = true;
#endif
	}
public:
	// CONSTRUCTOR
	customclothing_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bSTYLE = false;
#endif // _DEBUG
		m_nSTYLE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bSTYLE;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nSTYLE ) + 0;
	}
};
#define shaderStaticTest_customclothing_ps20b psh_forgot_to_set_static_STYLE + 0
class customclothing_ps20b_Dynamic_Index
{
public:
	// CONSTRUCTOR
	customclothing_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_customclothing_ps20b 0

static const ShaderComboInformation_t s_StaticComboArray_customclothing_ps20b[1] = 
{
	{ "STYLE", 0, 6 },
};
static const ShaderComboSemantics_t customclothing_ps20b_combos =
{
	"customclothing_ps20b", NULL, 0, s_StaticComboArray_customclothing_ps20b, 1 
};

class ConstructMe_customclothing_ps20b
{
public:
	ConstructMe_customclothing_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &customclothing_ps20b_combos );
	}
};

static ConstructMe_customclothing_ps20b s_ConstructMe_customclothing_ps20b;
