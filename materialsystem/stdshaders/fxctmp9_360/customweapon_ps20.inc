// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class customweapon_ps20_Static_Index
{
private:
	int m_nPAINTSTYLE;
#ifdef _DEBUG
	bool m_bPAINTSTYLE;
#endif
public:
	void SetPAINTSTYLE( int i )
	{
		Assert( i >= 0 && i <= 4 );
		m_nPAINTSTYLE = i;
#ifdef _DEBUG
		m_bPAINTSTYLE = true;
#endif
	}
	void SetPAINTSTYLE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 4 );
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
public:
	// CONSTRUCTOR
	customweapon_ps20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bPAINTSTYLE = false;
#endif // _DEBUG
		m_nPAINTSTYLE = 0;
#ifdef _DEBUG
		m_bEXPONENTMODE = false;
#endif // _DEBUG
		m_nEXPONENTMODE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bPAINTSTYLE && m_bEXPONENTMODE;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nPAINTSTYLE ) + ( 5 * m_nEXPONENTMODE ) + 0;
	}
};
#define shaderStaticTest_customweapon_ps20 psh_forgot_to_set_static_PAINTSTYLE + psh_forgot_to_set_static_EXPONENTMODE + 0
class customweapon_ps20_Dynamic_Index
{
public:
	// CONSTRUCTOR
	customweapon_ps20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_customweapon_ps20 0

static const ShaderComboInformation_t s_StaticComboArray_customweapon_ps20[2] = 
{
	{ "PAINTSTYLE", 0, 4 },
	{ "EXPONENTMODE", 0, 1 },
};
static const ShaderComboSemantics_t customweapon_ps20_combos =
{
	"customweapon_ps20", NULL, 0, s_StaticComboArray_customweapon_ps20, 2 
};

class ConstructMe_customweapon_ps20
{
public:
	ConstructMe_customweapon_ps20()
	{
		GetShaderDLL()->AddShaderComboInformation( &customweapon_ps20_combos );
	}
};

static ConstructMe_customweapon_ps20 s_ConstructMe_customweapon_ps20;
