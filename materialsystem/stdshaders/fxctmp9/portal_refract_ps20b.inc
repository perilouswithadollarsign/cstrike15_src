// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $STAGE != 2 ) && ( $TINTED == 1 )
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class portal_refract_ps20b_Static_Index
{
private:
	int m_nSTAGE;
#ifdef _DEBUG
	bool m_bSTAGE;
#endif
public:
	void SetSTAGE( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nSTAGE = i;
#ifdef _DEBUG
		m_bSTAGE = true;
#endif
	}
	void SetSTAGE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 2 );
		m_nSTAGE = i ? 1 : 0;
#ifdef _DEBUG
		m_bSTAGE = true;
#endif
	}
private:
	int m_nTINTED;
#ifdef _DEBUG
	bool m_bTINTED;
#endif
public:
	void SetTINTED( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nTINTED = i;
#ifdef _DEBUG
		m_bTINTED = true;
#endif
	}
	void SetTINTED( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nTINTED = i ? 1 : 0;
#ifdef _DEBUG
		m_bTINTED = true;
#endif
	}
private:
	int m_nSHADER_SRGB_READ;
#ifdef _DEBUG
	bool m_bSHADER_SRGB_READ;
#endif
public:
	void SetSHADER_SRGB_READ( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSHADER_SRGB_READ = i;
#ifdef _DEBUG
		m_bSHADER_SRGB_READ = true;
#endif
	}
	void SetSHADER_SRGB_READ( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nSHADER_SRGB_READ = i ? 1 : 0;
#ifdef _DEBUG
		m_bSHADER_SRGB_READ = true;
#endif
	}
private:
	int m_nDOPIXELFOG;
#ifdef _DEBUG
	bool m_bDOPIXELFOG;
#endif
public:
	void SetDOPIXELFOG( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDOPIXELFOG = i;
#ifdef _DEBUG
		m_bDOPIXELFOG = true;
#endif
	}
	void SetDOPIXELFOG( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDOPIXELFOG = i ? 1 : 0;
#ifdef _DEBUG
		m_bDOPIXELFOG = true;
#endif
	}
public:
	// CONSTRUCTOR
	portal_refract_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bSTAGE = false;
#endif // _DEBUG
		m_nSTAGE = 0;
#ifdef _DEBUG
		m_bTINTED = false;
#endif // _DEBUG
		m_nTINTED = 0;
#ifdef _DEBUG
		m_bSHADER_SRGB_READ = false;
#endif // _DEBUG
		m_nSHADER_SRGB_READ = 0;
#ifdef _DEBUG
		m_bDOPIXELFOG = true;
#endif // _DEBUG
		m_nDOPIXELFOG = !IS_FLAG_SET( MATERIAL_VAR_VERTEXFOG ) ? 1 : 0 ;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bSTAGE && m_bTINTED && m_bSHADER_SRGB_READ && m_bDOPIXELFOG;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 4 * m_nSTAGE ) + ( 12 * m_nTINTED ) + ( 24 * m_nSHADER_SRGB_READ ) + ( 48 * m_nDOPIXELFOG ) + 0;
	}
};
#define shaderStaticTest_portal_refract_ps20b psh_forgot_to_set_static_STAGE + psh_forgot_to_set_static_TINTED + psh_forgot_to_set_static_SHADER_SRGB_READ + 0
class portal_refract_ps20b_Dynamic_Index
{
private:
	int m_nD_NVIDIA_STEREO;
#ifdef _DEBUG
	bool m_bD_NVIDIA_STEREO;
#endif
public:
	void SetD_NVIDIA_STEREO( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nD_NVIDIA_STEREO = i;
#ifdef _DEBUG
		m_bD_NVIDIA_STEREO = true;
#endif
	}
	void SetD_NVIDIA_STEREO( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nD_NVIDIA_STEREO = i ? 1 : 0;
#ifdef _DEBUG
		m_bD_NVIDIA_STEREO = true;
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
	portal_refract_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bD_NVIDIA_STEREO = false;
#endif // _DEBUG
		m_nD_NVIDIA_STEREO = 0;
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
		bool bAllDynamicVarsDefined = m_bD_NVIDIA_STEREO && m_bPIXELFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nD_NVIDIA_STEREO ) + ( 2 * m_nPIXELFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_portal_refract_ps20b psh_forgot_to_set_dynamic_D_NVIDIA_STEREO + 0

static const ShaderComboInformation_t s_DynamicComboArray_portal_refract_ps20b[2] = 
{
	{ "D_NVIDIA_STEREO", 0, 1 },
	{ "PIXELFOGTYPE", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_portal_refract_ps20b[4] = 
{
	{ "STAGE", 0, 2 },
	{ "TINTED", 0, 1 },
	{ "SHADER_SRGB_READ", 0, 1 },
	{ "DOPIXELFOG", 0, 1 },
};
static const ShaderComboSemantics_t portal_refract_ps20b_combos =
{
	"portal_refract_ps20b", s_DynamicComboArray_portal_refract_ps20b, 2, s_StaticComboArray_portal_refract_ps20b, 4 
};

class ConstructMe_portal_refract_ps20b
{
public:
	ConstructMe_portal_refract_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &portal_refract_ps20b_combos );
	}
};

static ConstructMe_portal_refract_ps20b s_ConstructMe_portal_refract_ps20b;
