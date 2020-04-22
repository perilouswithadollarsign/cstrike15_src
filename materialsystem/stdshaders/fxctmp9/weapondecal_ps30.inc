// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $CASCADED_SHADOW_MAPPING == 0 ) && ( $DYN_CSM_ENABLED == 1 )
// ( $HIGHLIGHT == 1 ) 
// ( $HIGHLIGHT > 0 ) && ( $THIRDPERSON == 1 )
// ( $HIGHLIGHT > 0 ) && ( $PEEL == 1 )
// ( $PEEL == 1 ) && ( $THIRDPERSON == 1 )
// ( $ALPHAMASK == 1 ) && ( $THIRDPERSON == 1 )
// ( $ALPHAMASK == 1 ) && ( $HIGHLIGHT == 1 )
// ( $ALPHAMASK == 1 ) && ( $PEEL == 1 )
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
class weapondecal_ps30_Static_Index
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
		Assert( i >= 0 && i <= 5 );
		m_nDECALSTYLE = i;
#ifdef _DEBUG
		m_bDECALSTYLE = true;
#endif
	}
	void SetDECALSTYLE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 5 );
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
private:
	int m_nCASCADED_SHADOW_MAPPING;
#ifdef _DEBUG
	bool m_bCASCADED_SHADOW_MAPPING;
#endif
public:
	void SetCASCADED_SHADOW_MAPPING( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCASCADED_SHADOW_MAPPING = i;
#ifdef _DEBUG
		m_bCASCADED_SHADOW_MAPPING = true;
#endif
	}
	void SetCASCADED_SHADOW_MAPPING( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nCASCADED_SHADOW_MAPPING = i ? 1 : 0;
#ifdef _DEBUG
		m_bCASCADED_SHADOW_MAPPING = true;
#endif
	}
private:
	int m_nALPHAMASK;
#ifdef _DEBUG
	bool m_bALPHAMASK;
#endif
public:
	void SetALPHAMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nALPHAMASK = i;
#ifdef _DEBUG
		m_bALPHAMASK = true;
#endif
	}
	void SetALPHAMASK( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nALPHAMASK = i ? 1 : 0;
#ifdef _DEBUG
		m_bALPHAMASK = true;
#endif
	}
private:
	int m_nDESATBASETINT;
#ifdef _DEBUG
	bool m_bDESATBASETINT;
#endif
public:
	void SetDESATBASETINT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDESATBASETINT = i;
#ifdef _DEBUG
		m_bDESATBASETINT = true;
#endif
	}
	void SetDESATBASETINT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDESATBASETINT = i ? 1 : 0;
#ifdef _DEBUG
		m_bDESATBASETINT = true;
#endif
	}
public:
	// CONSTRUCTOR
	weapondecal_ps30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#ifdef _DEBUG
		m_bCASCADED_SHADOW_MAPPING = false;
#endif // _DEBUG
		m_nCASCADED_SHADOW_MAPPING = 0;
#ifdef _DEBUG
		m_bALPHAMASK = false;
#endif // _DEBUG
		m_nALPHAMASK = 0;
#ifdef _DEBUG
		m_bDESATBASETINT = false;
#endif // _DEBUG
		m_nDESATBASETINT = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bPHONG && m_bPHONGEXPONENTTEXTURE && m_bCUBEMAP && m_bDECALSTYLE && m_bTHIRDPERSON && m_bCASCADED_SHADOW_MAPPING && m_bALPHAMASK && m_bDESATBASETINT;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 60 * m_nPHONG ) + ( 120 * m_nPHONGEXPONENTTEXTURE ) + ( 240 * m_nCUBEMAP ) + ( 480 * m_nDECALSTYLE ) + ( 2880 * m_nTHIRDPERSON ) + ( 5760 * m_nCASCADED_SHADOW_MAPPING ) + ( 11520 * m_nALPHAMASK ) + ( 23040 * m_nDESATBASETINT ) + 0;
	}
};
#define shaderStaticTest_weapondecal_ps30 psh_forgot_to_set_static_PHONG + psh_forgot_to_set_static_PHONGEXPONENTTEXTURE + psh_forgot_to_set_static_CUBEMAP + psh_forgot_to_set_static_DECALSTYLE + psh_forgot_to_set_static_THIRDPERSON + psh_forgot_to_set_static_CASCADED_SHADOW_MAPPING + psh_forgot_to_set_static_ALPHAMASK + psh_forgot_to_set_static_DESATBASETINT + 0
class weapondecal_ps30_Dynamic_Index
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
private:
	int m_nHIGHLIGHT;
#ifdef _DEBUG
	bool m_bHIGHLIGHT;
#endif
public:
	void SetHIGHLIGHT( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nHIGHLIGHT = i;
#ifdef _DEBUG
		m_bHIGHLIGHT = true;
#endif
	}
	void SetHIGHLIGHT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 2 );
		m_nHIGHLIGHT = i ? 1 : 0;
#ifdef _DEBUG
		m_bHIGHLIGHT = true;
#endif
	}
private:
	int m_nPEEL;
#ifdef _DEBUG
	bool m_bPEEL;
#endif
public:
	void SetPEEL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPEEL = i;
#ifdef _DEBUG
		m_bPEEL = true;
#endif
	}
	void SetPEEL( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nPEEL = i ? 1 : 0;
#ifdef _DEBUG
		m_bPEEL = true;
#endif
	}
public:
	// CONSTRUCTOR
	weapondecal_ps30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bNUM_LIGHTS = false;
#endif // _DEBUG
		m_nNUM_LIGHTS = 0;
#ifdef _DEBUG
		m_bDYN_CSM_ENABLED = false;
#endif // _DEBUG
		m_nDYN_CSM_ENABLED = 0;
#ifdef _DEBUG
		m_bHIGHLIGHT = false;
#endif // _DEBUG
		m_nHIGHLIGHT = 0;
#ifdef _DEBUG
		m_bPEEL = false;
#endif // _DEBUG
		m_nPEEL = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bNUM_LIGHTS && m_bDYN_CSM_ENABLED && m_bHIGHLIGHT && m_bPEEL;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nNUM_LIGHTS ) + ( 5 * m_nDYN_CSM_ENABLED ) + ( 10 * m_nHIGHLIGHT ) + ( 30 * m_nPEEL ) + 0;
	}
};
#define shaderDynamicTest_weapondecal_ps30 psh_forgot_to_set_dynamic_NUM_LIGHTS + psh_forgot_to_set_dynamic_DYN_CSM_ENABLED + psh_forgot_to_set_dynamic_HIGHLIGHT + psh_forgot_to_set_dynamic_PEEL + 0

static const ShaderComboInformation_t s_DynamicComboArray_weapondecal_ps30[4] = 
{
	{ "NUM_LIGHTS", 0, 4 },
	{ "DYN_CSM_ENABLED", 0, 1 },
	{ "HIGHLIGHT", 0, 2 },
	{ "PEEL", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_weapondecal_ps30[8] = 
{
	{ "PHONG", 0, 1 },
	{ "PHONGEXPONENTTEXTURE", 0, 1 },
	{ "CUBEMAP", 0, 1 },
	{ "DECALSTYLE", 0, 5 },
	{ "THIRDPERSON", 0, 1 },
	{ "CASCADED_SHADOW_MAPPING", 0, 1 },
	{ "ALPHAMASK", 0, 1 },
	{ "DESATBASETINT", 0, 1 },
};
static const ShaderComboSemantics_t weapondecal_ps30_combos =
{
	"weapondecal_ps30", s_DynamicComboArray_weapondecal_ps30, 4, s_StaticComboArray_weapondecal_ps30, 8 
};

class ConstructMe_weapondecal_ps30
{
public:
	ConstructMe_weapondecal_ps30()
	{
		GetShaderDLL()->AddShaderComboInformation( &weapondecal_ps30_combos );
	}
};

static ConstructMe_weapondecal_ps30 s_ConstructMe_weapondecal_ps30;
