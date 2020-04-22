// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $PAINTSTYLE == 0 )
// ( ( $CHEAPMODE == 1 ) && !( ( $PAINTSTYLE == 3 ) || ( $PAINTSTYLE == 6 ) ) )
// ( ( $EXPONENTMODE == 1 ) && ( $CHEAPMODE == 1 ) )
// ( ( $NUM_LIGHTS != 0 ) && ( $PREVIEW == 0 ) )
// ( ( $PREVIEW == 0 ) && ( $PREVIEWPHONGALBEDOTINT == 1 ) )
// ( ( $PREVIEW == 1 ) && ( $EXPONENTMODE == 1 ) )
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
class customweapon_ps30_Static_Index
{
private:
	int m_nPAINTSTYLE;
#ifdef _DEBUG
	bool m_bPAINTSTYLE;
#endif
public:
	void SetPAINTSTYLE( int i )
	{
		Assert( i >= 0 && i <= 9 );
		m_nPAINTSTYLE = i;
#ifdef _DEBUG
		m_bPAINTSTYLE = true;
#endif
	}
	void SetPAINTSTYLE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 9 );
		m_nPAINTSTYLE = i ? 1 : 0;
#ifdef _DEBUG
		m_bPAINTSTYLE = true;
#endif
	}
private:
	int m_nEXPONENTMODE;
#ifdef _DEBUG
	bool m_bEXPONENTMODE;
#endif
public:
	void SetEXPONENTMODE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nEXPONENTMODE = i;
#ifdef _DEBUG
		m_bEXPONENTMODE = true;
#endif
	}
	void SetEXPONENTMODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nEXPONENTMODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bEXPONENTMODE = true;
#endif
	}
private:
	int m_nCHEAPMODE;
#ifdef _DEBUG
	bool m_bCHEAPMODE;
#endif
public:
	void SetCHEAPMODE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCHEAPMODE = i;
#ifdef _DEBUG
		m_bCHEAPMODE = true;
#endif
	}
	void SetCHEAPMODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nCHEAPMODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bCHEAPMODE = true;
#endif
	}
private:
	int m_nPREVIEW;
#ifdef _DEBUG
	bool m_bPREVIEW;
#endif
public:
	void SetPREVIEW( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPREVIEW = i;
#ifdef _DEBUG
		m_bPREVIEW = true;
#endif
	}
	void SetPREVIEW( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nPREVIEW = i ? 1 : 0;
#ifdef _DEBUG
		m_bPREVIEW = true;
#endif
	}
private:
	int m_nPREVIEWPHONGALBEDOTINT;
#ifdef _DEBUG
	bool m_bPREVIEWPHONGALBEDOTINT;
#endif
public:
	void SetPREVIEWPHONGALBEDOTINT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPREVIEWPHONGALBEDOTINT = i;
#ifdef _DEBUG
		m_bPREVIEWPHONGALBEDOTINT = true;
#endif
	}
	void SetPREVIEWPHONGALBEDOTINT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nPREVIEWPHONGALBEDOTINT = i ? 1 : 0;
#ifdef _DEBUG
		m_bPREVIEWPHONGALBEDOTINT = true;
#endif
	}
private:
	int m_nPHONGALBEDOFACTORMODE;
#ifdef _DEBUG
	bool m_bPHONGALBEDOFACTORMODE;
#endif
public:
	void SetPHONGALBEDOFACTORMODE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPHONGALBEDOFACTORMODE = i;
#ifdef _DEBUG
		m_bPHONGALBEDOFACTORMODE = true;
#endif
	}
	void SetPHONGALBEDOFACTORMODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nPHONGALBEDOFACTORMODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bPHONGALBEDOFACTORMODE = true;
#endif
	}
public:
	// CONSTRUCTOR
	customweapon_ps30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bPAINTSTYLE = false;
#endif // _DEBUG
		m_nPAINTSTYLE = 0;
#ifdef _DEBUG
		m_bEXPONENTMODE = false;
#endif // _DEBUG
		m_nEXPONENTMODE = 0;
#ifdef _DEBUG
		m_bCHEAPMODE = false;
#endif // _DEBUG
		m_nCHEAPMODE = 0;
#ifdef _DEBUG
		m_bPREVIEW = false;
#endif // _DEBUG
		m_nPREVIEW = 0;
#ifdef _DEBUG
		m_bPREVIEWPHONGALBEDOTINT = false;
#endif // _DEBUG
		m_nPREVIEWPHONGALBEDOTINT = 0;
#ifdef _DEBUG
		m_bPHONGALBEDOFACTORMODE = false;
#endif // _DEBUG
		m_nPHONGALBEDOFACTORMODE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bPAINTSTYLE && m_bEXPONENTMODE && m_bCHEAPMODE && m_bPREVIEW && m_bPREVIEWPHONGALBEDOTINT && m_bPHONGALBEDOFACTORMODE;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 5 * m_nPAINTSTYLE ) + ( 50 * m_nEXPONENTMODE ) + ( 100 * m_nCHEAPMODE ) + ( 200 * m_nPREVIEW ) + ( 400 * m_nPREVIEWPHONGALBEDOTINT ) + ( 800 * m_nPHONGALBEDOFACTORMODE ) + 0;
	}
};
#define shaderStaticTest_customweapon_ps30 psh_forgot_to_set_static_PAINTSTYLE + psh_forgot_to_set_static_EXPONENTMODE + psh_forgot_to_set_static_CHEAPMODE + psh_forgot_to_set_static_PREVIEW + psh_forgot_to_set_static_PREVIEWPHONGALBEDOTINT + psh_forgot_to_set_static_PHONGALBEDOFACTORMODE + 0
class customweapon_ps30_Dynamic_Index
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
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 4 );
		m_nNUM_LIGHTS = i ? 1 : 0;
#ifdef _DEBUG
		m_bNUM_LIGHTS = true;
#endif
	}
public:
	// CONSTRUCTOR
	customweapon_ps30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_customweapon_ps30 psh_forgot_to_set_dynamic_NUM_LIGHTS + 0

static const ShaderComboInformation_t s_DynamicComboArray_customweapon_ps30[1] = 
{
	{ "NUM_LIGHTS", 0, 4 },
};

static const ShaderComboInformation_t s_StaticComboArray_customweapon_ps30[6] = 
{
	{ "PAINTSTYLE", 0, 9 },
	{ "EXPONENTMODE", 0, 1 },
	{ "CHEAPMODE", 0, 1 },
	{ "PREVIEW", 0, 1 },
	{ "PREVIEWPHONGALBEDOTINT", 0, 1 },
	{ "PHONGALBEDOFACTORMODE", 0, 1 },
};
static const ShaderComboSemantics_t customweapon_ps30_combos =
{
	"customweapon_ps30", s_DynamicComboArray_customweapon_ps30, 1, s_StaticComboArray_customweapon_ps30, 6 
};

class ConstructMe_customweapon_ps30
{
public:
	ConstructMe_customweapon_ps30()
	{
		GetShaderDLL()->AddShaderComboInformation( &customweapon_ps30_combos );
	}
};

static ConstructMe_customweapon_ps30 s_ConstructMe_customweapon_ps30;
