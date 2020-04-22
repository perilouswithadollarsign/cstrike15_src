// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class bufferclearobeystencil_ps20_Static_Index
{
private:
	int m_nRELOADZCULL;
#ifdef _DEBUG
	bool m_bRELOADZCULL;
#endif
public:
	void SetRELOADZCULL( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nRELOADZCULL = i;
#ifdef _DEBUG
		m_bRELOADZCULL = true;
#endif
	}
	void SetRELOADZCULL( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nRELOADZCULL = i ? 1 : 0;
#ifdef _DEBUG
		m_bRELOADZCULL = true;
#endif
	}
public:
	// CONSTRUCTOR
	bufferclearobeystencil_ps20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bRELOADZCULL = false;
#endif // _DEBUG
		m_nRELOADZCULL = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bRELOADZCULL;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nRELOADZCULL ) + 0;
	}
};
#define shaderStaticTest_bufferclearobeystencil_ps20 psh_forgot_to_set_static_RELOADZCULL + 0
class bufferclearobeystencil_ps20_Dynamic_Index
{
public:
	// CONSTRUCTOR
	bufferclearobeystencil_ps20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_bufferclearobeystencil_ps20 0

static const ShaderComboInformation_t s_StaticComboArray_bufferclearobeystencil_ps20[1] = 
{
	{ "RELOADZCULL", 0, 0 },
};
static const ShaderComboSemantics_t bufferclearobeystencil_ps20_combos =
{
	"bufferclearobeystencil_ps20", NULL, 0, s_StaticComboArray_bufferclearobeystencil_ps20, 1 
};

class ConstructMe_bufferclearobeystencil_ps20
{
public:
	ConstructMe_bufferclearobeystencil_ps20()
	{
		GetShaderDLL()->AddShaderComboInformation( &bufferclearobeystencil_ps20_combos );
	}
};

static ConstructMe_bufferclearobeystencil_ps20 s_ConstructMe_bufferclearobeystencil_ps20;
