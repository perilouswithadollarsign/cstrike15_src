// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class debugmrttexture_ps20b_Static_Index
{
private:
	int m_nMRTINDEX;
#ifdef _DEBUG
	bool m_bMRTINDEX;
#endif
public:
	void SetMRTINDEX( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMRTINDEX = i;
#ifdef _DEBUG
		m_bMRTINDEX = true;
#endif
	}
	void SetMRTINDEX( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nMRTINDEX = i ? 1 : 0;
#ifdef _DEBUG
		m_bMRTINDEX = true;
#endif
	}
public:
	// CONSTRUCTOR
	debugmrttexture_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bMRTINDEX = false;
#endif // _DEBUG
		m_nMRTINDEX = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bMRTINDEX;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nMRTINDEX ) + 0;
	}
};
#define shaderStaticTest_debugmrttexture_ps20b psh_forgot_to_set_static_MRTINDEX + 0
class debugmrttexture_ps20b_Dynamic_Index
{
public:
	// CONSTRUCTOR
	debugmrttexture_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_debugmrttexture_ps20b 0

static const ShaderComboInformation_t s_StaticComboArray_debugmrttexture_ps20b[1] = 
{
	{ "MRTINDEX", 0, 1 },
};
static const ShaderComboSemantics_t debugmrttexture_ps20b_combos =
{
	"debugmrttexture_ps20b", NULL, 0, s_StaticComboArray_debugmrttexture_ps20b, 1 
};

class ConstructMe_debugmrttexture_ps20b
{
public:
	ConstructMe_debugmrttexture_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &debugmrttexture_ps20b_combos );
	}
};

static ConstructMe_debugmrttexture_ps20b s_ConstructMe_debugmrttexture_ps20b;
