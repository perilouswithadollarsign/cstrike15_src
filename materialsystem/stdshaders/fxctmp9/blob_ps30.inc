// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $CONTACT_SHADOW == 1 ) && ( $SELF_ILLUM_PULSE == 1 )
// ( $BACK_SURFACE == 1 ) && ( $LIGHT_WARP > 0 )
// ( $BACK_SURFACE == 1 ) && ( $FRESNEL_WARP > 0 )
// ( $BACK_SURFACE == 1 ) && ( $INTERIOR_LAYER > 0 )
// ( $BACK_SURFACE == 1 ) && ( $SELF_ILLUM_FRESNEL > 0 )
// ( $BACK_SURFACE == 1 ) && ( $OPACITY_TEXTURE > 0 )
// ( $BACK_SURFACE == 1 ) && ( $CONTACT_SHADOW > 0 )
// ( $BACK_SURFACE == 1 ) && ( $SELF_ILLUM_PULSE > 0 )
// ( $BACK_SURFACE == 1 ) && ( $VOLUME_TEXTURE > 0 )
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
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class blob_ps30_Static_Index
{
private:
	int m_nBACK_SURFACE;
#ifdef _DEBUG
	bool m_bBACK_SURFACE;
#endif
public:
	void SetBACK_SURFACE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBACK_SURFACE = i;
#ifdef _DEBUG
		m_bBACK_SURFACE = true;
#endif
	}
	void SetBACK_SURFACE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nBACK_SURFACE = i ? 1 : 0;
#ifdef _DEBUG
		m_bBACK_SURFACE = true;
#endif
	}
private:
	int m_nLIGHT_WARP;
#ifdef _DEBUG
	bool m_bLIGHT_WARP;
#endif
public:
	void SetLIGHT_WARP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nLIGHT_WARP = i;
#ifdef _DEBUG
		m_bLIGHT_WARP = true;
#endif
	}
	void SetLIGHT_WARP( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nLIGHT_WARP = i ? 1 : 0;
#ifdef _DEBUG
		m_bLIGHT_WARP = true;
#endif
	}
private:
	int m_nFRESNEL_WARP;
#ifdef _DEBUG
	bool m_bFRESNEL_WARP;
#endif
public:
	void SetFRESNEL_WARP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFRESNEL_WARP = i;
#ifdef _DEBUG
		m_bFRESNEL_WARP = true;
#endif
	}
	void SetFRESNEL_WARP( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nFRESNEL_WARP = i ? 1 : 0;
#ifdef _DEBUG
		m_bFRESNEL_WARP = true;
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
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nHIGH_PRECISION_DEPTH = i ? 1 : 0;
#ifdef _DEBUG
		m_bHIGH_PRECISION_DEPTH = true;
#endif
	}
private:
	int m_nINTERIOR_LAYER;
#ifdef _DEBUG
	bool m_bINTERIOR_LAYER;
#endif
public:
	void SetINTERIOR_LAYER( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nINTERIOR_LAYER = i;
#ifdef _DEBUG
		m_bINTERIOR_LAYER = true;
#endif
	}
	void SetINTERIOR_LAYER( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nINTERIOR_LAYER = i ? 1 : 0;
#ifdef _DEBUG
		m_bINTERIOR_LAYER = true;
#endif
	}
private:
	int m_nSELF_ILLUM_FRESNEL;
#ifdef _DEBUG
	bool m_bSELF_ILLUM_FRESNEL;
#endif
public:
	void SetSELF_ILLUM_FRESNEL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSELF_ILLUM_FRESNEL = i;
#ifdef _DEBUG
		m_bSELF_ILLUM_FRESNEL = true;
#endif
	}
	void SetSELF_ILLUM_FRESNEL( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nSELF_ILLUM_FRESNEL = i ? 1 : 0;
#ifdef _DEBUG
		m_bSELF_ILLUM_FRESNEL = true;
#endif
	}
private:
	int m_nOPACITY_TEXTURE;
#ifdef _DEBUG
	bool m_bOPACITY_TEXTURE;
#endif
public:
	void SetOPACITY_TEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nOPACITY_TEXTURE = i;
#ifdef _DEBUG
		m_bOPACITY_TEXTURE = true;
#endif
	}
	void SetOPACITY_TEXTURE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nOPACITY_TEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bOPACITY_TEXTURE = true;
#endif
	}
private:
	int m_nFLASHLIGHTDEPTHFILTERMODE;
#ifdef _DEBUG
	bool m_bFLASHLIGHTDEPTHFILTERMODE;
#endif
public:
	void SetFLASHLIGHTDEPTHFILTERMODE( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nFLASHLIGHTDEPTHFILTERMODE = i;
#ifdef _DEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = true;
#endif
	}
	void SetFLASHLIGHTDEPTHFILTERMODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 2 );
		m_nFLASHLIGHTDEPTHFILTERMODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = true;
#endif
	}
private:
	int m_nCONTACT_SHADOW;
#ifdef _DEBUG
	bool m_bCONTACT_SHADOW;
#endif
public:
	void SetCONTACT_SHADOW( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCONTACT_SHADOW = i;
#ifdef _DEBUG
		m_bCONTACT_SHADOW = true;
#endif
	}
	void SetCONTACT_SHADOW( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nCONTACT_SHADOW = i ? 1 : 0;
#ifdef _DEBUG
		m_bCONTACT_SHADOW = true;
#endif
	}
private:
	int m_nSELF_ILLUM_PULSE;
#ifdef _DEBUG
	bool m_bSELF_ILLUM_PULSE;
#endif
public:
	void SetSELF_ILLUM_PULSE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSELF_ILLUM_PULSE = i;
#ifdef _DEBUG
		m_bSELF_ILLUM_PULSE = true;
#endif
	}
	void SetSELF_ILLUM_PULSE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nSELF_ILLUM_PULSE = i ? 1 : 0;
#ifdef _DEBUG
		m_bSELF_ILLUM_PULSE = true;
#endif
	}
private:
	int m_nVOLUME_TEXTURE;
#ifdef _DEBUG
	bool m_bVOLUME_TEXTURE;
#endif
public:
	void SetVOLUME_TEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nVOLUME_TEXTURE = i;
#ifdef _DEBUG
		m_bVOLUME_TEXTURE = true;
#endif
	}
	void SetVOLUME_TEXTURE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nVOLUME_TEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bVOLUME_TEXTURE = true;
#endif
	}
public:
	// CONSTRUCTOR
	blob_ps30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bBACK_SURFACE = false;
#endif // _DEBUG
		m_nBACK_SURFACE = 0;
#ifdef _DEBUG
		m_bLIGHT_WARP = false;
#endif // _DEBUG
		m_nLIGHT_WARP = 0;
#ifdef _DEBUG
		m_bFRESNEL_WARP = false;
#endif // _DEBUG
		m_nFRESNEL_WARP = 0;
#ifdef _DEBUG
		m_bHIGH_PRECISION_DEPTH = false;
#endif // _DEBUG
		m_nHIGH_PRECISION_DEPTH = 0;
#ifdef _DEBUG
		m_bINTERIOR_LAYER = false;
#endif // _DEBUG
		m_nINTERIOR_LAYER = 0;
#ifdef _DEBUG
		m_bSELF_ILLUM_FRESNEL = false;
#endif // _DEBUG
		m_nSELF_ILLUM_FRESNEL = 0;
#ifdef _DEBUG
		m_bOPACITY_TEXTURE = false;
#endif // _DEBUG
		m_nOPACITY_TEXTURE = 0;
#ifdef _DEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = false;
#endif // _DEBUG
		m_nFLASHLIGHTDEPTHFILTERMODE = 0;
#ifdef _DEBUG
		m_bCONTACT_SHADOW = false;
#endif // _DEBUG
		m_nCONTACT_SHADOW = 0;
#ifdef _DEBUG
		m_bSELF_ILLUM_PULSE = false;
#endif // _DEBUG
		m_nSELF_ILLUM_PULSE = 0;
#ifdef _DEBUG
		m_bVOLUME_TEXTURE = false;
#endif // _DEBUG
		m_nVOLUME_TEXTURE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bBACK_SURFACE && m_bLIGHT_WARP && m_bFRESNEL_WARP && m_bHIGH_PRECISION_DEPTH && m_bINTERIOR_LAYER && m_bSELF_ILLUM_FRESNEL && m_bOPACITY_TEXTURE && m_bFLASHLIGHTDEPTHFILTERMODE && m_bCONTACT_SHADOW && m_bSELF_ILLUM_PULSE && m_bVOLUME_TEXTURE;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 20 * m_nBACK_SURFACE ) + ( 40 * m_nLIGHT_WARP ) + ( 80 * m_nFRESNEL_WARP ) + ( 160 * m_nHIGH_PRECISION_DEPTH ) + ( 320 * m_nINTERIOR_LAYER ) + ( 640 * m_nSELF_ILLUM_FRESNEL ) + ( 1280 * m_nOPACITY_TEXTURE ) + ( 2560 * m_nFLASHLIGHTDEPTHFILTERMODE ) + ( 7680 * m_nCONTACT_SHADOW ) + ( 15360 * m_nSELF_ILLUM_PULSE ) + ( 30720 * m_nVOLUME_TEXTURE ) + 0;
	}
};
#define shaderStaticTest_blob_ps30 psh_forgot_to_set_static_BACK_SURFACE + psh_forgot_to_set_static_LIGHT_WARP + psh_forgot_to_set_static_FRESNEL_WARP + psh_forgot_to_set_static_HIGH_PRECISION_DEPTH + psh_forgot_to_set_static_INTERIOR_LAYER + psh_forgot_to_set_static_SELF_ILLUM_FRESNEL + psh_forgot_to_set_static_OPACITY_TEXTURE + psh_forgot_to_set_static_FLASHLIGHTDEPTHFILTERMODE + psh_forgot_to_set_static_CONTACT_SHADOW + psh_forgot_to_set_static_SELF_ILLUM_PULSE + psh_forgot_to_set_static_VOLUME_TEXTURE + 0
class blob_ps30_Dynamic_Index
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
	int m_nFLASHLIGHT;
#ifdef _DEBUG
	bool m_bFLASHLIGHT;
#endif
public:
	void SetFLASHLIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFLASHLIGHT = i;
#ifdef _DEBUG
		m_bFLASHLIGHT = true;
#endif
	}
	void SetFLASHLIGHT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nFLASHLIGHT = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLASHLIGHT = true;
#endif
	}
private:
	int m_nFLASHLIGHTSHADOWS;
#ifdef _DEBUG
	bool m_bFLASHLIGHTSHADOWS;
#endif
public:
	void SetFLASHLIGHTSHADOWS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFLASHLIGHTSHADOWS = i;
#ifdef _DEBUG
		m_bFLASHLIGHTSHADOWS = true;
#endif
	}
	void SetFLASHLIGHTSHADOWS( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nFLASHLIGHTSHADOWS = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLASHLIGHTSHADOWS = true;
#endif
	}
public:
	// CONSTRUCTOR
	blob_ps30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bNUM_LIGHTS = false;
#endif // _DEBUG
		m_nNUM_LIGHTS = 0;
#ifdef _DEBUG
		m_bFLASHLIGHT = false;
#endif // _DEBUG
		m_nFLASHLIGHT = 0;
#ifdef _DEBUG
		m_bFLASHLIGHTSHADOWS = false;
#endif // _DEBUG
		m_nFLASHLIGHTSHADOWS = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bNUM_LIGHTS && m_bFLASHLIGHT && m_bFLASHLIGHTSHADOWS;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nNUM_LIGHTS ) + ( 5 * m_nFLASHLIGHT ) + ( 10 * m_nFLASHLIGHTSHADOWS ) + 0;
	}
};
#define shaderDynamicTest_blob_ps30 psh_forgot_to_set_dynamic_NUM_LIGHTS + psh_forgot_to_set_dynamic_FLASHLIGHT + psh_forgot_to_set_dynamic_FLASHLIGHTSHADOWS + 0

static const ShaderComboInformation_t s_DynamicComboArray_blob_ps30[3] = 
{
	{ "NUM_LIGHTS", 0, 4 },
	{ "FLASHLIGHT", 0, 1 },
	{ "FLASHLIGHTSHADOWS", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_blob_ps30[11] = 
{
	{ "BACK_SURFACE", 0, 1 },
	{ "LIGHT_WARP", 0, 1 },
	{ "FRESNEL_WARP", 0, 1 },
	{ "HIGH_PRECISION_DEPTH", 0, 1 },
	{ "INTERIOR_LAYER", 0, 1 },
	{ "SELF_ILLUM_FRESNEL", 0, 1 },
	{ "OPACITY_TEXTURE", 0, 1 },
	{ "FLASHLIGHTDEPTHFILTERMODE", 0, 2 },
	{ "CONTACT_SHADOW", 0, 1 },
	{ "SELF_ILLUM_PULSE", 0, 1 },
	{ "VOLUME_TEXTURE", 0, 1 },
};
static const ShaderComboSemantics_t blob_ps30_combos =
{
	"blob_ps30", s_DynamicComboArray_blob_ps30, 3, s_StaticComboArray_blob_ps30, 11 
};

class ConstructMe_blob_ps30
{
public:
	ConstructMe_blob_ps30()
	{
		GetShaderDLL()->AddShaderComboInformation( &blob_ps30_combos );
	}
};

static ConstructMe_blob_ps30 s_ConstructMe_blob_ps30;
