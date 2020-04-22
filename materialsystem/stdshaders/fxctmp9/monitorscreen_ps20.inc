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
class monitorscreen_ps20_Static_Index
{
private:
	int m_nTEXTURE2;
#ifdef _DEBUG
	bool m_bTEXTURE2;
#endif
public:
	void SetTEXTURE2( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nTEXTURE2 = i;
#ifdef _DEBUG
		m_bTEXTURE2 = true;
#endif
	}
	void SetTEXTURE2( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nTEXTURE2 = i ? 1 : 0;
#ifdef _DEBUG
		m_bTEXTURE2 = true;
#endif
	}
public:
	// CONSTRUCTOR
	monitorscreen_ps20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bTEXTURE2 = false;
#endif // _DEBUG
		m_nTEXTURE2 = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bTEXTURE2;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 2 * m_nTEXTURE2 ) + 0;
	}
};
#define shaderStaticTest_monitorscreen_ps20 psh_forgot_to_set_static_TEXTURE2 + 0
class monitorscreen_ps20_Dynamic_Index
{
private:
	int m_nPIXELFOGTYPE;
#ifdef _DEBUG
	bool m_bPIXELFOGTYPE;
#endif
public:
	void SetPIXELFOGTYPE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPIXELFOGTYPE = i;
#ifdef _DEBUG
		m_bPIXELFOGTYPE = true;
#endif
	}
	void SetPIXELFOGTYPE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nPIXELFOGTYPE = i ? 1 : 0;
#ifdef _DEBUG
		m_bPIXELFOGTYPE = true;
#endif
	}
public:
	// CONSTRUCTOR
	monitorscreen_ps20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bPIXELFOGTYPE = true;
#endif // _DEBUG
		m_nPIXELFOGTYPE = ( pShaderAPI->GetSceneFogMode() == MATERIAL_FOG_LINEAR_BELOW_FOG_Z ) ;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bPIXELFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nPIXELFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_monitorscreen_ps20 0

static const ShaderComboInformation_t s_DynamicComboArray_monitorscreen_ps20[1] = 
{
	{ "PIXELFOGTYPE", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_monitorscreen_ps20[1] = 
{
	{ "TEXTURE2", 0, 1 },
};
static const ShaderComboSemantics_t monitorscreen_ps20_combos =
{
	"monitorscreen_ps20", s_DynamicComboArray_monitorscreen_ps20, 1, s_StaticComboArray_monitorscreen_ps20, 1 
};

class ConstructMe_monitorscreen_ps20
{
public:
	ConstructMe_monitorscreen_ps20()
	{
		GetShaderDLL()->AddShaderComboInformation( &monitorscreen_ps20_combos );
	}
};

static ConstructMe_monitorscreen_ps20 s_ConstructMe_monitorscreen_ps20;
