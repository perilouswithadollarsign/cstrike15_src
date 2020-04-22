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
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class manifoldsurface_ps30_Static_Index
{
private:
	int m_nLIGHT_LUT_TYPE;
#ifdef _DEBUG
	bool m_bLIGHT_LUT_TYPE;
#endif
public:
	void SetLIGHT_LUT_TYPE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nLIGHT_LUT_TYPE = i;
#ifdef _DEBUG
		m_bLIGHT_LUT_TYPE = true;
#endif
	}
	void SetLIGHT_LUT_TYPE( bool i )
	{
		m_nLIGHT_LUT_TYPE = i ? 1 : 0;
#ifdef _DEBUG
		m_bLIGHT_LUT_TYPE = true;
#endif
	}
private:
	int m_nHIGH_PRECISION_DEPTH;
#ifdef _DEBUG
	bool m_bHIGH_PRECISION_DEPTH;
#endif
public:
	void SetHIGH_PRECISION_DEPTH( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nHIGH_PRECISION_DEPTH = i;
#ifdef _DEBUG
		m_bHIGH_PRECISION_DEPTH = true;
#endif
	}
	void SetHIGH_PRECISION_DEPTH( bool i )
	{
		m_nHIGH_PRECISION_DEPTH = i ? 1 : 0;
#ifdef _DEBUG
		m_bHIGH_PRECISION_DEPTH = true;
#endif
	}
public:
	// CONSTRUCTOR
	manifoldsurface_ps30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bLIGHT_LUT_TYPE = false;
#endif // _DEBUG
		m_nLIGHT_LUT_TYPE = 0;
#ifdef _DEBUG
		m_bHIGH_PRECISION_DEPTH = false;
#endif // _DEBUG
		m_nHIGH_PRECISION_DEPTH = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bLIGHT_LUT_TYPE && m_bHIGH_PRECISION_DEPTH;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 5 * m_nLIGHT_LUT_TYPE ) + ( 10 * m_nHIGH_PRECISION_DEPTH ) + 0;
	}
};
#define shaderStaticTest_manifoldsurface_ps30 psh_forgot_to_set_static_LIGHT_LUT_TYPE + psh_forgot_to_set_static_HIGH_PRECISION_DEPTH + 0
class manifoldsurface_ps30_Dynamic_Index
{
private:
	int m_nNUM_LIGHTS;
#ifdef _DEBUG
	bool m_bNUM_LIGHTS;
#endif
public:
	void SetNUM_LIGHTS( int i )
	{
		Assert( i >= 0 && i <= 4 );
		m_nNUM_LIGHTS = i;
#ifdef _DEBUG
		m_bNUM_LIGHTS = true;
#endif
	}
	void SetNUM_LIGHTS( bool i )
	{
		m_nNUM_LIGHTS = i ? 1 : 0;
#ifdef _DEBUG
		m_bNUM_LIGHTS = true;
#endif
	}
public:
	// CONSTRUCTOR
	manifoldsurface_ps30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bNUM_LIGHTS = false;
#endif // _DEBUG
		m_nNUM_LIGHTS = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bNUM_LIGHTS;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nNUM_LIGHTS ) + 0;
	}
};
#define shaderDynamicTest_manifoldsurface_ps30 psh_forgot_to_set_dynamic_NUM_LIGHTS + 0
