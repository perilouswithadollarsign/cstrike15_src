// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $CASCADED_SHADOW_MAPPING == 0 ) && ( $DYN_CSM_ENABLED == 1 )
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
class cs_grass_ps30_Static_Index
{
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
public:
	// CONSTRUCTOR
	cs_grass_ps30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bCASCADED_SHADOW_MAPPING = false;
#endif // _DEBUG
		m_nCASCADED_SHADOW_MAPPING = 0;
#ifdef _DEBUG
		m_bCSM_MODE = false;
#endif // _DEBUG
		m_nCSM_MODE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bCASCADED_SHADOW_MAPPING && m_bCSM_MODE;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 4 * m_nCASCADED_SHADOW_MAPPING ) + ( 8 * m_nCSM_MODE ) + 0;
	}
};
#define shaderStaticTest_cs_grass_ps30 psh_forgot_to_set_static_CASCADED_SHADOW_MAPPING + psh_forgot_to_set_static_CSM_MODE + 0
class cs_grass_ps30_Dynamic_Index
{
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
	cs_grass_ps30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bDYN_CSM_ENABLED = false;
#endif // _DEBUG
		m_nDYN_CSM_ENABLED = 0;
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
		bool bAllDynamicVarsDefined = m_bDYN_CSM_ENABLED && m_bPIXELFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nDYN_CSM_ENABLED ) + ( 2 * m_nPIXELFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_cs_grass_ps30 psh_forgot_to_set_dynamic_DYN_CSM_ENABLED + 0

static const ShaderComboInformation_t s_DynamicComboArray_cs_grass_ps30[2] = 
{
	{ "DYN_CSM_ENABLED", 0, 1 },
	{ "PIXELFOGTYPE", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_cs_grass_ps30[2] = 
{
	{ "CASCADED_SHADOW_MAPPING", 0, 1 },
	{ "CSM_MODE", 0, 3 },
};
static const ShaderComboSemantics_t cs_grass_ps30_combos =
{
	"cs_grass_ps30", s_DynamicComboArray_cs_grass_ps30, 2, s_StaticComboArray_cs_grass_ps30, 2 
};

class ConstructMe_cs_grass_ps30
{
public:
	ConstructMe_cs_grass_ps30()
	{
		GetShaderDLL()->AddShaderComboInformation( &cs_grass_ps30_combos );
	}
};

static ConstructMe_cs_grass_ps30 s_ConstructMe_cs_grass_ps30;
