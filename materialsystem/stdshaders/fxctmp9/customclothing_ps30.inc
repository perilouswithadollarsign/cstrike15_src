// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( ( $COMPOSITEMODE == 1 ) && ( $CASCADED_SHADOW_MAPPING != 0 ) )
// ( ( $COMPOSITEMODE == 1 ) && ( $NUM_LIGHTS != 0 ) )
// ( $CASCADED_SHADOW_MAPPING == 0 ) && ( $DYN_CSM_ENABLED == 1 )
// ( $CASCADED_SHADOW_MAPPING == 0 ) && ( $CSM_MODE != 0 )
// ( $USE_PATTERN2 == 1 ) && ( $USE_PATTERN1 == 0 )
// ( $USE_PATTERN_OFFSET == 1 ) && ( $USE_PATTERN1 == 0 && $USE_PATTERN2 == 0 )
// ( $SWAP_PATTERN_MASKS == 1 ) && ( $USE_PATTERN1 == 0 && $USE_PATTERN2 == 0 )
// ( $USE_LOGO2 == 1 ) && ( $USE_LOGO1 == 0 )
// ( $COMPOSITEMODE == 2 ) && ( $USE_PATTERN1 == 1 )
// ( $COMPOSITEMODE == 2 ) && ( $USE_PATTERN2 == 1 )
// ( $COMPOSITEMODE == 2 ) && ( $USE_PATTERN_OFFSET == 1 )
// ( $COMPOSITEMODE == 2 ) && ( $USE_LOGO1 == 1 )
// ( $COMPOSITEMODE == 2 ) && ( $USE_LOGO2 == 1 )
// ( $AO_MODE == 1 ) && ( $COMPOSITEMODE == 1 )
// ( $AO_MODE == 1 ) && ( $NUM_LIGHTS != 0 )
// ( $AO_MODE == 1 ) && ( $DYN_CSM_ENABLED == 1 )
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
class customclothing_ps30_Static_Index
{
private:
	int m_nCOMPOSITEMODE;
#ifdef _DEBUG
	bool m_bCOMPOSITEMODE;
#endif
public:
	void SetCOMPOSITEMODE( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nCOMPOSITEMODE = i;
#ifdef _DEBUG
		m_bCOMPOSITEMODE = true;
#endif
	}
	void SetCOMPOSITEMODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 2 );
		m_nCOMPOSITEMODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bCOMPOSITEMODE = true;
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
	int m_nCSM_MODE;
#ifdef _DEBUG
	bool m_bCSM_MODE;
#endif
public:
	void SetCSM_MODE( int i )
	{
		Assert( i >= 0 && i <= 3 );
		m_nCSM_MODE = i;
#ifdef _DEBUG
		m_bCSM_MODE = true;
#endif
	}
	void SetCSM_MODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 3 );
		m_nCSM_MODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bCSM_MODE = true;
#endif
	}
private:
	int m_nUSE_PATTERN1;
#ifdef _DEBUG
	bool m_bUSE_PATTERN1;
#endif
public:
	void SetUSE_PATTERN1( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nUSE_PATTERN1 = i;
#ifdef _DEBUG
		m_bUSE_PATTERN1 = true;
#endif
	}
	void SetUSE_PATTERN1( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nUSE_PATTERN1 = i ? 1 : 0;
#ifdef _DEBUG
		m_bUSE_PATTERN1 = true;
#endif
	}
private:
	int m_nUSE_PATTERN2;
#ifdef _DEBUG
	bool m_bUSE_PATTERN2;
#endif
public:
	void SetUSE_PATTERN2( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nUSE_PATTERN2 = i;
#ifdef _DEBUG
		m_bUSE_PATTERN2 = true;
#endif
	}
	void SetUSE_PATTERN2( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nUSE_PATTERN2 = i ? 1 : 0;
#ifdef _DEBUG
		m_bUSE_PATTERN2 = true;
#endif
	}
private:
	int m_nUSE_PATTERN_OFFSET;
#ifdef _DEBUG
	bool m_bUSE_PATTERN_OFFSET;
#endif
public:
	void SetUSE_PATTERN_OFFSET( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nUSE_PATTERN_OFFSET = i;
#ifdef _DEBUG
		m_bUSE_PATTERN_OFFSET = true;
#endif
	}
	void SetUSE_PATTERN_OFFSET( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nUSE_PATTERN_OFFSET = i ? 1 : 0;
#ifdef _DEBUG
		m_bUSE_PATTERN_OFFSET = true;
#endif
	}
private:
	int m_nUSE_LOGO1;
#ifdef _DEBUG
	bool m_bUSE_LOGO1;
#endif
public:
	void SetUSE_LOGO1( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nUSE_LOGO1 = i;
#ifdef _DEBUG
		m_bUSE_LOGO1 = true;
#endif
	}
	void SetUSE_LOGO1( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nUSE_LOGO1 = i ? 1 : 0;
#ifdef _DEBUG
		m_bUSE_LOGO1 = true;
#endif
	}
private:
	int m_nUSE_LOGO2;
#ifdef _DEBUG
	bool m_bUSE_LOGO2;
#endif
public:
	void SetUSE_LOGO2( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nUSE_LOGO2 = i;
#ifdef _DEBUG
		m_bUSE_LOGO2 = true;
#endif
	}
	void SetUSE_LOGO2( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nUSE_LOGO2 = i ? 1 : 0;
#ifdef _DEBUG
		m_bUSE_LOGO2 = true;
#endif
	}
private:
	int m_nSWAP_PATTERN_MASKS;
#ifdef _DEBUG
	bool m_bSWAP_PATTERN_MASKS;
#endif
public:
	void SetSWAP_PATTERN_MASKS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSWAP_PATTERN_MASKS = i;
#ifdef _DEBUG
		m_bSWAP_PATTERN_MASKS = true;
#endif
	}
	void SetSWAP_PATTERN_MASKS( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nSWAP_PATTERN_MASKS = i ? 1 : 0;
#ifdef _DEBUG
		m_bSWAP_PATTERN_MASKS = true;
#endif
	}
public:
	// CONSTRUCTOR
	customclothing_ps30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bCOMPOSITEMODE = false;
#endif // _DEBUG
		m_nCOMPOSITEMODE = 0;
#ifdef _DEBUG
		m_bCASCADED_SHADOW_MAPPING = false;
#endif // _DEBUG
		m_nCASCADED_SHADOW_MAPPING = 0;
#ifdef _DEBUG
		m_bCSM_MODE = false;
#endif // _DEBUG
		m_nCSM_MODE = 0;
#ifdef _DEBUG
		m_bUSE_PATTERN1 = false;
#endif // _DEBUG
		m_nUSE_PATTERN1 = 0;
#ifdef _DEBUG
		m_bUSE_PATTERN2 = false;
#endif // _DEBUG
		m_nUSE_PATTERN2 = 0;
#ifdef _DEBUG
		m_bUSE_PATTERN_OFFSET = false;
#endif // _DEBUG
		m_nUSE_PATTERN_OFFSET = 0;
#ifdef _DEBUG
		m_bUSE_LOGO1 = false;
#endif // _DEBUG
		m_nUSE_LOGO1 = 0;
#ifdef _DEBUG
		m_bUSE_LOGO2 = false;
#endif // _DEBUG
		m_nUSE_LOGO2 = 0;
#ifdef _DEBUG
		m_bSWAP_PATTERN_MASKS = false;
#endif // _DEBUG
		m_nSWAP_PATTERN_MASKS = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bCOMPOSITEMODE && m_bCASCADED_SHADOW_MAPPING && m_bCSM_MODE && m_bUSE_PATTERN1 && m_bUSE_PATTERN2 && m_bUSE_PATTERN_OFFSET && m_bUSE_LOGO1 && m_bUSE_LOGO2 && m_bSWAP_PATTERN_MASKS;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 20 * m_nCOMPOSITEMODE ) + ( 60 * m_nCASCADED_SHADOW_MAPPING ) + ( 120 * m_nCSM_MODE ) + ( 480 * m_nUSE_PATTERN1 ) + ( 960 * m_nUSE_PATTERN2 ) + ( 1920 * m_nUSE_PATTERN_OFFSET ) + ( 3840 * m_nUSE_LOGO1 ) + ( 7680 * m_nUSE_LOGO2 ) + ( 15360 * m_nSWAP_PATTERN_MASKS ) + 0;
	}
};
#define shaderStaticTest_customclothing_ps30 psh_forgot_to_set_static_COMPOSITEMODE + psh_forgot_to_set_static_CASCADED_SHADOW_MAPPING + psh_forgot_to_set_static_CSM_MODE + psh_forgot_to_set_static_USE_PATTERN1 + psh_forgot_to_set_static_USE_PATTERN2 + psh_forgot_to_set_static_USE_PATTERN_OFFSET + psh_forgot_to_set_static_USE_LOGO1 + psh_forgot_to_set_static_USE_LOGO2 + psh_forgot_to_set_static_SWAP_PATTERN_MASKS + 0
class customclothing_ps30_Dynamic_Index
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
	int m_nAO_MODE;
#ifdef _DEBUG
	bool m_bAO_MODE;
#endif
public:
	void SetAO_MODE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nAO_MODE = i;
#ifdef _DEBUG
		m_bAO_MODE = true;
#endif
	}
	void SetAO_MODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nAO_MODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bAO_MODE = true;
#endif
	}
public:
	// CONSTRUCTOR
	customclothing_ps30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
		m_bAO_MODE = false;
#endif // _DEBUG
		m_nAO_MODE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bNUM_LIGHTS && m_bDYN_CSM_ENABLED && m_bAO_MODE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nNUM_LIGHTS ) + ( 5 * m_nDYN_CSM_ENABLED ) + ( 10 * m_nAO_MODE ) + 0;
	}
};
#define shaderDynamicTest_customclothing_ps30 psh_forgot_to_set_dynamic_NUM_LIGHTS + psh_forgot_to_set_dynamic_DYN_CSM_ENABLED + psh_forgot_to_set_dynamic_AO_MODE + 0

static const ShaderComboInformation_t s_DynamicComboArray_customclothing_ps30[3] = 
{
	{ "NUM_LIGHTS", 0, 4 },
	{ "DYN_CSM_ENABLED", 0, 1 },
	{ "AO_MODE", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_customclothing_ps30[9] = 
{
	{ "COMPOSITEMODE", 0, 2 },
	{ "CASCADED_SHADOW_MAPPING", 0, 1 },
	{ "CSM_MODE", 0, 3 },
	{ "USE_PATTERN1", 0, 1 },
	{ "USE_PATTERN2", 0, 1 },
	{ "USE_PATTERN_OFFSET", 0, 1 },
	{ "USE_LOGO1", 0, 1 },
	{ "USE_LOGO2", 0, 1 },
	{ "SWAP_PATTERN_MASKS", 0, 1 },
};
static const ShaderComboSemantics_t customclothing_ps30_combos =
{
	"customclothing_ps30", s_DynamicComboArray_customclothing_ps30, 3, s_StaticComboArray_customclothing_ps30, 9 
};

class ConstructMe_customclothing_ps30
{
public:
	ConstructMe_customclothing_ps30()
	{
		GetShaderDLL()->AddShaderComboInformation( &customclothing_ps30_combos );
	}
};

static ConstructMe_customclothing_ps30 s_ConstructMe_customclothing_ps30;
