// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class debugtextureview_ps20b_Static_Index
{
private:
	int m_nSHOWALPHA;
#ifdef _DEBUG
	bool m_bSHOWALPHA;
#endif
public:
	void SetSHOWALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSHOWALPHA = i;
#ifdef _DEBUG
		m_bSHOWALPHA = true;
#endif
	}
	void SetSHOWALPHA( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nSHOWALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bSHOWALPHA = true;
#endif
	}
public:
	// CONSTRUCTOR
	debugtextureview_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bSHOWALPHA = false;
#endif // _DEBUG
		m_nSHOWALPHA = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bSHOWALPHA;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 2 * m_nSHOWALPHA ) + 0;
	}
};
#define shaderStaticTest_debugtextureview_ps20b psh_forgot_to_set_static_SHOWALPHA + 0
class debugtextureview_ps20b_Dynamic_Index
{
private:
	int m_nISCUBEMAP;
#ifdef _DEBUG
	bool m_bISCUBEMAP;
#endif
public:
	void SetISCUBEMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nISCUBEMAP = i;
#ifdef _DEBUG
		m_bISCUBEMAP = true;
#endif
	}
	void SetISCUBEMAP( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nISCUBEMAP = i ? 1 : 0;
#ifdef _DEBUG
		m_bISCUBEMAP = true;
#endif
	}
public:
	// CONSTRUCTOR
	debugtextureview_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bISCUBEMAP = false;
#endif // _DEBUG
		m_nISCUBEMAP = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bISCUBEMAP;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nISCUBEMAP ) + 0;
	}
};
#define shaderDynamicTest_debugtextureview_ps20b psh_forgot_to_set_dynamic_ISCUBEMAP + 0

static const ShaderComboInformation_t s_DynamicComboArray_debugtextureview_ps20b[1] = 
{
	{ "ISCUBEMAP", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_debugtextureview_ps20b[1] = 
{
	{ "SHOWALPHA", 0, 1 },
};
static const ShaderComboSemantics_t debugtextureview_ps20b_combos =
{
	"debugtextureview_ps20b", s_DynamicComboArray_debugtextureview_ps20b, 1, s_StaticComboArray_debugtextureview_ps20b, 1 
};

class ConstructMe_debugtextureview_ps20b
{
public:
	ConstructMe_debugtextureview_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &debugtextureview_ps20b_combos );
	}
};

static ConstructMe_debugtextureview_ps20b s_ConstructMe_debugtextureview_ps20b;
