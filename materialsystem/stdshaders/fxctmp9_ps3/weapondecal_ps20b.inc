// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( CASCADED_SHADOW_MAPPING ) != ( DYN_CSM_ENABLED )
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
class weapondecal_ps20b_Static_Index
{
private:
	int m_nPHONG;
#ifdef _DEBUG
	bool m_bPHONG;
#endif
public:
	void SetPHONG( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPHONG = i;
#ifdef _DEBUG
		m_bPHONG = true;
#endif
	}
	void SetPHONG( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nPHONG = i ? 1 : 0;
#ifdef _DEBUG
		m_bPHONG = true;
#endif
	}
private:
	int m_nPHONGEXPONENTTEXTURE;
#ifdef _DEBUG
	bool m_bPHONGEXPONENTTEXTURE;
#endif
public:
	void SetPHONGEXPONENTTEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPHONGEXPONENTTEXTURE = i;
#ifdef _DEBUG
		m_bPHONGEXPONENTTEXTURE = true;
#endif
	}
	void SetPHONGEXPONENTTEXTURE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nPHONGEXPONENTTEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bPHONGEXPONENTTEXTURE = true;
#endif
	}
private:
	int m_nCUBEMAP;
#ifdef _DEBUG
	bool m_bCUBEMAP;
#endif
public:
	void SetCUBEMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCUBEMAP = i;
#ifdef _DEBUG
		m_bCUBEMAP = true;
#endif
	}
	void SetCUBEMAP( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nCUBEMAP = i ? 1 : 0;
#ifdef _DEBUG
		m_bCUBEMAP = true;
#endif
	}
private:
	int m_nDECALSTYLE;
#ifdef _DEBUG
	bool m_bDECALSTYLE;
#endif
public:
	void SetDECALSTYLE( int i )
	{
		Assert( i >= 0 && i <= 4 );
		m_nDECALSTYLE = i;
#ifdef _DEBUG
		m_bDECALSTYLE = true;
#endif
	}
	void SetDECALSTYLE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 4 );
		m_nDECALSTYLE = i ? 1 : 0;
#ifdef _DEBUG
		m_bDECALSTYLE = true;
#endif
	}
private:
	int m_nTHIRDPERSON;
#ifdef _DEBUG
	bool m_bTHIRDPERSON;
#endif
public:
	void SetTHIRDPERSON( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nTHIRDPERSON = i;
#ifdef _DEBUG
		m_bTHIRDPERSON = true;
#endif
	}
	void SetTHIRDPERSON( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nTHIRDPERSON = i ? 1 : 0;
#ifdef _DEBUG
		m_bTHIRDPERSON = true;
#endif
	}
public:
	// CONSTRUCTOR
	weapondecal_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bPHONG = false;
#endif // _DEBUG
		m_nPHONG = 0;
#ifdef _DEBUG
		m_bPHONGEXPONENTTEXTURE = false;
#endif // _DEBUG
		m_nPHONGEXPONENTTEXTURE = 0;
#ifdef _DEBUG
		m_bCUBEMAP = false;
#endif // _DEBUG
		m_nCUBEMAP = 0;
#ifdef _DEBUG
		m_bDECALSTYLE = false;
#endif // _DEBUG
		m_nDECALSTYLE = 0;
#ifdef _DEBUG
		m_bTHIRDPERSON = false;
#endif // _DEBUG
		m_nTHIRDPERSON = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bPHONG && m_bPHONGEXPONENTTEXTURE && m_bCUBEMAP && m_bDECALSTYLE && m_bTHIRDPERSON;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 10 * m_nPHONG ) + ( 20 * m_nPHONGEXPONENTTEXTURE ) + ( 40 * m_nCUBEMAP ) + ( 80 * m_nDECALSTYLE ) + ( 400 * m_nTHIRDPERSON ) + 0;
	}
};
#define shaderStaticTest_weapondecal_ps20b psh_forgot_to_set_static_PHONG + psh_forgot_to_set_static_PHONGEXPONENTTEXTURE + psh_forgot_to_set_static_CUBEMAP + psh_forgot_to_set_static_DECALSTYLE + psh_forgot_to_set_static_THIRDPERSON + 0
class weapondecal_ps20b_Dynamic_Index
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
private:
	int m_nDYN_CSM_ENABLED;
#ifdef _DEBUG
	bool m_bDYN_CSM_ENABLED;
#endif
public:
	void SetDYN_CSM_ENABLED( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDYN_CSM_ENABLED = i;
#ifdef _DEBUG
		m_bDYN_CSM_ENABLED = true;
#endif
	}
	void SetDYN_CSM_ENABLED( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDYN_CSM_ENABLED = i ? 1 : 0;
#ifdef _DEBUG
		m_bDYN_CSM_ENABLED = true;
#endif
	}
public:
	// CONSTRUCTOR
	weapondecal_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bNUM_LIGHTS = false;
#endif // _DEBUG
		m_nNUM_LIGHTS = 0;
#ifdef _DEBUG
		m_bDYN_CSM_ENABLED = false;
#endif // _DEBUG
		m_nDYN_CSM_ENABLED = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bNUM_LIGHTS && m_bDYN_CSM_ENABLED;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nNUM_LIGHTS ) + ( 5 * m_nDYN_CSM_ENABLED ) + 0;
	}
};
#define shaderDynamicTest_weapondecal_ps20b psh_forgot_to_set_dynamic_NUM_LIGHTS + psh_forgot_to_set_dynamic_DYN_CSM_ENABLED + 0

static const ShaderComboInformation_t s_DynamicComboArray_weapondecal_ps20b[2] = 
{
	{ "NUM_LIGHTS", 0, 4 },
	{ "DYN_CSM_ENABLED", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_weapondecal_ps20b[5] = 
{
	{ "PHONG", 0, 1 },
	{ "PHONGEXPONENTTEXTURE", 0, 1 },
	{ "CUBEMAP", 0, 1 },
	{ "DECALSTYLE", 0, 4 },
	{ "THIRDPERSON", 0, 1 },
};
static const ShaderComboSemantics_t weapondecal_ps20b_combos =
{
	"weapondecal_ps20b", s_DynamicComboArray_weapondecal_ps20b, 2, s_StaticComboArray_weapondecal_ps20b, 5 
};

class ConstructMe_weapondecal_ps20b
{
public:
	ConstructMe_weapondecal_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &weapondecal_ps20b_combos );
	}
};

static ConstructMe_weapondecal_ps20b s_ConstructMe_weapondecal_ps20b;
