// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class worldvertexalpha_ps20b_Static_Index
{
private:
	int m_nPASS;
#ifdef _DEBUG
	bool m_bPASS;
#endif
public:
	void SetPASS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPASS = i;
#ifdef _DEBUG
		m_bPASS = true;
#endif
	}
	void SetPASS( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nPASS = i ? 1 : 0;
#ifdef _DEBUG
		m_bPASS = true;
#endif
	}
public:
	// CONSTRUCTOR
	worldvertexalpha_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bPASS = false;
#endif // _DEBUG
		m_nPASS = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bPASS;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nPASS ) + 0;
	}
};
#define shaderStaticTest_worldvertexalpha_ps20b psh_forgot_to_set_static_PASS + 0
class worldvertexalpha_ps20b_Dynamic_Index
{
public:
	// CONSTRUCTOR
	worldvertexalpha_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_worldvertexalpha_ps20b 0

static const ShaderComboInformation_t s_StaticComboArray_WorldVertexAlpha_ps20b[1] = 
{
	{ "PASS", 0, 1 },
};
static const ShaderComboSemantics_t WorldVertexAlpha_ps20b_combos =
{
	"WorldVertexAlpha_ps20b", NULL, 0, s_StaticComboArray_WorldVertexAlpha_ps20b, 1 
};

class ConstructMe_WorldVertexAlpha_ps20b
{
public:
	ConstructMe_WorldVertexAlpha_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &WorldVertexAlpha_ps20b_combos );
	}
};

static ConstructMe_WorldVertexAlpha_ps20b s_ConstructMe_WorldVertexAlpha_ps20b;
