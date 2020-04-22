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
class blob_arm_ps20b_Static_Index
{
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
	int m_nPRE_PASS;
#ifdef _DEBUG
	bool m_bPRE_PASS;
#endif
public:
	void SetPRE_PASS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPRE_PASS = i;
#ifdef _DEBUG
		m_bPRE_PASS = true;
#endif
	}
	void SetPRE_PASS( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nPRE_PASS = i ? 1 : 0;
#ifdef _DEBUG
		m_bPRE_PASS = true;
#endif
	}
private:
	int m_nNORMALMAP;
#ifdef _DEBUG
	bool m_bNORMALMAP;
#endif
public:
	void SetNORMALMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nNORMALMAP = i;
#ifdef _DEBUG
		m_bNORMALMAP = true;
#endif
	}
	void SetNORMALMAP( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nNORMALMAP = i ? 1 : 0;
#ifdef _DEBUG
		m_bNORMALMAP = true;
#endif
	}
private:
	int m_nMOVING_PULSES;
#ifdef _DEBUG
	bool m_bMOVING_PULSES;
#endif
public:
	void SetMOVING_PULSES( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMOVING_PULSES = i;
#ifdef _DEBUG
		m_bMOVING_PULSES = true;
#endif
	}
	void SetMOVING_PULSES( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nMOVING_PULSES = i ? 1 : 0;
#ifdef _DEBUG
		m_bMOVING_PULSES = true;
#endif
	}
public:
	// CONSTRUCTOR
	blob_arm_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bSELF_ILLUM_FRESNEL = false;
#endif // _DEBUG
		m_nSELF_ILLUM_FRESNEL = 0;
#ifdef _DEBUG
		m_bPRE_PASS = false;
#endif // _DEBUG
		m_nPRE_PASS = 0;
#ifdef _DEBUG
		m_bNORMALMAP = false;
#endif // _DEBUG
		m_nNORMALMAP = 0;
#ifdef _DEBUG
		m_bMOVING_PULSES = false;
#endif // _DEBUG
		m_nMOVING_PULSES = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bSELF_ILLUM_FRESNEL && m_bPRE_PASS && m_bNORMALMAP && m_bMOVING_PULSES;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nSELF_ILLUM_FRESNEL ) + ( 2 * m_nPRE_PASS ) + ( 4 * m_nNORMALMAP ) + ( 8 * m_nMOVING_PULSES ) + 0;
	}
};
#define shaderStaticTest_blob_arm_ps20b psh_forgot_to_set_static_SELF_ILLUM_FRESNEL + psh_forgot_to_set_static_PRE_PASS + psh_forgot_to_set_static_NORMALMAP + psh_forgot_to_set_static_MOVING_PULSES + 0
class blob_arm_ps20b_Dynamic_Index
{
public:
	// CONSTRUCTOR
	blob_arm_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_blob_arm_ps20b 0

static const ShaderComboInformation_t s_StaticComboArray_blob_arm_ps20b[4] = 
{
	{ "SELF_ILLUM_FRESNEL", 0, 1 },
	{ "PRE_PASS", 0, 1 },
	{ "NORMALMAP", 0, 1 },
	{ "MOVING_PULSES", 0, 1 },
};
static const ShaderComboSemantics_t blob_arm_ps20b_combos =
{
	"blob_arm_ps20b", NULL, 0, s_StaticComboArray_blob_arm_ps20b, 4 
};

class ConstructMe_blob_arm_ps20b
{
public:
	ConstructMe_blob_arm_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &blob_arm_ps20b_combos );
	}
};

static ConstructMe_blob_arm_ps20b s_ConstructMe_blob_arm_ps20b;
