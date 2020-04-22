// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $ALPHATESTREF == 1 ) && ( $SHADOWDEPTH == 1 )
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class splinerope_ps20_Static_Index
{
private:
	int m_nSHADER_SRGB_READ;
#ifdef _DEBUG
	bool m_bSHADER_SRGB_READ;
#endif
public:
	void SetSHADER_SRGB_READ( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nSHADER_SRGB_READ = i;
#ifdef _DEBUG
		m_bSHADER_SRGB_READ = true;
#endif
	}
	void SetSHADER_SRGB_READ( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nSHADER_SRGB_READ = i ? 1 : 0;
#ifdef _DEBUG
		m_bSHADER_SRGB_READ = true;
#endif
	}
private:
	int m_nSHADOWDEPTH;
#ifdef _DEBUG
	bool m_bSHADOWDEPTH;
#endif
public:
	void SetSHADOWDEPTH( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSHADOWDEPTH = i;
#ifdef _DEBUG
		m_bSHADOWDEPTH = true;
#endif
	}
	void SetSHADOWDEPTH( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nSHADOWDEPTH = i ? 1 : 0;
#ifdef _DEBUG
		m_bSHADOWDEPTH = true;
#endif
	}
private:
	int m_nALPHATESTREF;
#ifdef _DEBUG
	bool m_bALPHATESTREF;
#endif
public:
	void SetALPHATESTREF( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nALPHATESTREF = i;
#ifdef _DEBUG
		m_bALPHATESTREF = true;
#endif
	}
	void SetALPHATESTREF( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nALPHATESTREF = i ? 1 : 0;
#ifdef _DEBUG
		m_bALPHATESTREF = true;
#endif
	}
public:
	// CONSTRUCTOR
	splinerope_ps20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bSHADER_SRGB_READ = false;
#endif // _DEBUG
		m_nSHADER_SRGB_READ = 0;
#ifdef _DEBUG
		m_bSHADOWDEPTH = false;
#endif // _DEBUG
		m_nSHADOWDEPTH = 0;
#ifdef _DEBUG
		m_bALPHATESTREF = false;
#endif // _DEBUG
		m_nALPHATESTREF = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bSHADER_SRGB_READ && m_bSHADOWDEPTH && m_bALPHATESTREF;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 2 * m_nSHADER_SRGB_READ ) + ( 2 * m_nSHADOWDEPTH ) + ( 4 * m_nALPHATESTREF ) + 0;
	}
};
#define shaderStaticTest_splinerope_ps20 psh_forgot_to_set_static_SHADER_SRGB_READ + psh_forgot_to_set_static_SHADOWDEPTH + psh_forgot_to_set_static_ALPHATESTREF + 0
class splinerope_ps20_Dynamic_Index
{
private:
	int m_nWRITE_DEPTH_TO_DESTALPHA;
#ifdef _DEBUG
	bool m_bWRITE_DEPTH_TO_DESTALPHA;
#endif
public:
	void SetWRITE_DEPTH_TO_DESTALPHA( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nWRITE_DEPTH_TO_DESTALPHA = i;
#ifdef _DEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = true;
#endif
	}
	void SetWRITE_DEPTH_TO_DESTALPHA( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nWRITE_DEPTH_TO_DESTALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = true;
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
	splinerope_ps20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = false;
#endif // _DEBUG
		m_nWRITE_DEPTH_TO_DESTALPHA = 0;
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
		bool bAllDynamicVarsDefined = m_bWRITE_DEPTH_TO_DESTALPHA && m_bPIXELFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nWRITE_DEPTH_TO_DESTALPHA ) + ( 1 * m_nPIXELFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_splinerope_ps20 psh_forgot_to_set_dynamic_WRITE_DEPTH_TO_DESTALPHA + 0

static const ShaderComboInformation_t s_DynamicComboArray_splinerope_ps20[2] = 
{
	{ "WRITE_DEPTH_TO_DESTALPHA", 0, 0 },
	{ "PIXELFOGTYPE", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_splinerope_ps20[3] = 
{
	{ "SHADER_SRGB_READ", 0, 0 },
	{ "SHADOWDEPTH", 0, 1 },
	{ "ALPHATESTREF", 0, 1 },
};
static const ShaderComboSemantics_t splinerope_ps20_combos =
{
	"splinerope_ps20", s_DynamicComboArray_splinerope_ps20, 2, s_StaticComboArray_splinerope_ps20, 3 
};

class ConstructMe_splinerope_ps20
{
public:
	ConstructMe_splinerope_ps20()
	{
		GetShaderDLL()->AddShaderComboInformation( &splinerope_ps20_combos );
	}
};

static ConstructMe_splinerope_ps20 s_ConstructMe_splinerope_ps20;
