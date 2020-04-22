// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( CROSSHAIR_MODE == 1 ) && ( LIGHTING_PREVIEW > 0 )
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
class unlittwotexture_ps20b_Static_Index
{
private:
	int m_nTRANSLUCENT;
#ifdef _DEBUG
	bool m_bTRANSLUCENT;
#endif
public:
	void SetTRANSLUCENT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nTRANSLUCENT = i;
#ifdef _DEBUG
		m_bTRANSLUCENT = true;
#endif
	}
	void SetTRANSLUCENT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nTRANSLUCENT = i ? 1 : 0;
#ifdef _DEBUG
		m_bTRANSLUCENT = true;
#endif
	}
private:
	int m_nLIGHTING_PREVIEW;
#ifdef _DEBUG
	bool m_bLIGHTING_PREVIEW;
#endif
public:
	void SetLIGHTING_PREVIEW( int i )
	{
		Assert( i >= 0 && i <= 3 );
		m_nLIGHTING_PREVIEW = i;
#ifdef _DEBUG
		m_bLIGHTING_PREVIEW = true;
#endif
	}
	void SetLIGHTING_PREVIEW( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 3 );
		m_nLIGHTING_PREVIEW = i ? 1 : 0;
#ifdef _DEBUG
		m_bLIGHTING_PREVIEW = true;
#endif
	}
private:
	int m_nCROSSHAIR_MODE;
#ifdef _DEBUG
	bool m_bCROSSHAIR_MODE;
#endif
public:
	void SetCROSSHAIR_MODE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCROSSHAIR_MODE = i;
#ifdef _DEBUG
		m_bCROSSHAIR_MODE = true;
#endif
	}
	void SetCROSSHAIR_MODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nCROSSHAIR_MODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bCROSSHAIR_MODE = true;
#endif
	}
public:
	// CONSTRUCTOR
	unlittwotexture_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bTRANSLUCENT = false;
#endif // _DEBUG
		m_nTRANSLUCENT = 0;
#ifdef _DEBUG
		m_bLIGHTING_PREVIEW = false;
#endif // _DEBUG
		m_nLIGHTING_PREVIEW = 0;
#ifdef _DEBUG
		m_bCROSSHAIR_MODE = false;
#endif // _DEBUG
		m_nCROSSHAIR_MODE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bTRANSLUCENT && m_bLIGHTING_PREVIEW && m_bCROSSHAIR_MODE;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 4 * m_nTRANSLUCENT ) + ( 8 * m_nLIGHTING_PREVIEW ) + ( 32 * m_nCROSSHAIR_MODE ) + 0;
	}
};
#define shaderStaticTest_unlittwotexture_ps20b psh_forgot_to_set_static_TRANSLUCENT + psh_forgot_to_set_static_LIGHTING_PREVIEW + psh_forgot_to_set_static_CROSSHAIR_MODE + 0
class unlittwotexture_ps20b_Dynamic_Index
{
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
private:
	int m_nWRITE_DEPTH_TO_DESTALPHA;
#ifdef _DEBUG
	bool m_bWRITE_DEPTH_TO_DESTALPHA;
#endif
public:
	void SetWRITE_DEPTH_TO_DESTALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nWRITE_DEPTH_TO_DESTALPHA = i;
#ifdef _DEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = true;
#endif
	}
	void SetWRITE_DEPTH_TO_DESTALPHA( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nWRITE_DEPTH_TO_DESTALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = true;
#endif
	}
public:
	// CONSTRUCTOR
	unlittwotexture_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bPIXELFOGTYPE = true;
#endif // _DEBUG
		m_nPIXELFOGTYPE = ( pShaderAPI->GetSceneFogMode() == MATERIAL_FOG_LINEAR_BELOW_FOG_Z ) ;
#ifdef _DEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = false;
#endif // _DEBUG
		m_nWRITE_DEPTH_TO_DESTALPHA = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bPIXELFOGTYPE && m_bWRITE_DEPTH_TO_DESTALPHA;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nPIXELFOGTYPE ) + ( 2 * m_nWRITE_DEPTH_TO_DESTALPHA ) + 0;
	}
};
#define shaderDynamicTest_unlittwotexture_ps20b psh_forgot_to_set_dynamic_WRITE_DEPTH_TO_DESTALPHA + 0

static const ShaderComboInformation_t s_DynamicComboArray_unlittwotexture_ps20b[2] = 
{
	{ "PIXELFOGTYPE", 0, 1 },
	{ "WRITE_DEPTH_TO_DESTALPHA", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_unlittwotexture_ps20b[3] = 
{
	{ "TRANSLUCENT", 0, 1 },
	{ "LIGHTING_PREVIEW", 0, 3 },
	{ "CROSSHAIR_MODE", 0, 1 },
};
static const ShaderComboSemantics_t unlittwotexture_ps20b_combos =
{
	"unlittwotexture_ps20b", s_DynamicComboArray_unlittwotexture_ps20b, 2, s_StaticComboArray_unlittwotexture_ps20b, 3 
};

class ConstructMe_unlittwotexture_ps20b
{
public:
	ConstructMe_unlittwotexture_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &unlittwotexture_ps20b_combos );
	}
};

static ConstructMe_unlittwotexture_ps20b s_ConstructMe_unlittwotexture_ps20b;
