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
#include "shaderlib/cshader.h"
class decalmodulate_ps20b_Static_Index
{
private:
	int m_nVERTEXALPHA;
#ifdef _DEBUG
	bool m_bVERTEXALPHA;
#endif
public:
	void SetVERTEXALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nVERTEXALPHA = i;
#ifdef _DEBUG
		m_bVERTEXALPHA = true;
#endif
	}
	void SetVERTEXALPHA( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nVERTEXALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bVERTEXALPHA = true;
#endif
	}
private:
	int m_nFOGFADE;
#ifdef _DEBUG
	bool m_bFOGFADE;
#endif
public:
	void SetFOGFADE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFOGFADE = i;
#ifdef _DEBUG
		m_bFOGFADE = true;
#endif
	}
	void SetFOGFADE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nFOGFADE = i ? 1 : 0;
#ifdef _DEBUG
		m_bFOGFADE = true;
#endif
	}
public:
	// CONSTRUCTOR
	decalmodulate_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bVERTEXALPHA = false;
#endif // _DEBUG
		m_nVERTEXALPHA = 0;
#ifdef _DEBUG
		m_bFOGFADE = false;
#endif // _DEBUG
		m_nFOGFADE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bVERTEXALPHA && m_bFOGFADE;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 2 * m_nVERTEXALPHA ) + ( 4 * m_nFOGFADE ) + 0;
	}
};
#define shaderStaticTest_decalmodulate_ps20b psh_forgot_to_set_static_VERTEXALPHA + psh_forgot_to_set_static_FOGFADE + 0
class decalmodulate_ps20b_Dynamic_Index
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
public:
	// CONSTRUCTOR
	decalmodulate_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
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
		bool bAllDynamicVarsDefined = m_bPIXELFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nPIXELFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_decalmodulate_ps20b 0

static const ShaderComboInformation_t s_DynamicComboArray_decalmodulate_ps20b[1] = 
{
	{ "PIXELFOGTYPE", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_decalmodulate_ps20b[2] = 
{
	{ "VERTEXALPHA", 0, 1 },
	{ "FOGFADE", 0, 1 },
};
static const ShaderComboSemantics_t decalmodulate_ps20b_combos =
{
	"decalmodulate_ps20b", s_DynamicComboArray_decalmodulate_ps20b, 1, s_StaticComboArray_decalmodulate_ps20b, 2 
};

class ConstructMe_decalmodulate_ps20b
{
public:
	ConstructMe_decalmodulate_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &decalmodulate_ps20b_combos );
	}
};

static ConstructMe_decalmodulate_ps20b s_ConstructMe_decalmodulate_ps20b;
