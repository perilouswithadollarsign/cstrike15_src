// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class showz_ps20b_Static_Index
{
private:
	int m_nFLASHLIGHTDEPTHFILTERMODE;
#ifdef _DEBUG
	bool m_bFLASHLIGHTDEPTHFILTERMODE;
#endif
public:
	void SetFLASHLIGHTDEPTHFILTERMODE( int i )
	{
		Assert( i >= 0 && i <= 3 );
		m_nFLASHLIGHTDEPTHFILTERMODE = i;
#ifdef _DEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = true;
#endif
	}
	void SetFLASHLIGHTDEPTHFILTERMODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 3 );
		m_nFLASHLIGHTDEPTHFILTERMODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = true;
#endif
	}
private:
	int m_nDEPTH_IN_ALPHA;
#ifdef _DEBUG
	bool m_bDEPTH_IN_ALPHA;
#endif
public:
	void SetDEPTH_IN_ALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDEPTH_IN_ALPHA = i;
#ifdef _DEBUG
		m_bDEPTH_IN_ALPHA = true;
#endif
	}
	void SetDEPTH_IN_ALPHA( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDEPTH_IN_ALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bDEPTH_IN_ALPHA = true;
#endif
	}
public:
	// CONSTRUCTOR
	showz_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = false;
#endif // _DEBUG
		m_nFLASHLIGHTDEPTHFILTERMODE = 0;
#ifdef _DEBUG
		m_bDEPTH_IN_ALPHA = false;
#endif // _DEBUG
		m_nDEPTH_IN_ALPHA = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bFLASHLIGHTDEPTHFILTERMODE && m_bDEPTH_IN_ALPHA;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nFLASHLIGHTDEPTHFILTERMODE ) + ( 4 * m_nDEPTH_IN_ALPHA ) + 0;
	}
};
#define shaderStaticTest_showz_ps20b psh_forgot_to_set_static_FLASHLIGHTDEPTHFILTERMODE + psh_forgot_to_set_static_DEPTH_IN_ALPHA + 0
class showz_ps20b_Dynamic_Index
{
public:
	// CONSTRUCTOR
	showz_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_showz_ps20b 0

static const ShaderComboInformation_t s_StaticComboArray_showz_ps20b[2] = 
{
	{ "FLASHLIGHTDEPTHFILTERMODE", 0, 3 },
	{ "DEPTH_IN_ALPHA", 0, 1 },
};
static const ShaderComboSemantics_t showz_ps20b_combos =
{
	"showz_ps20b", NULL, 0, s_StaticComboArray_showz_ps20b, 2 
};

class ConstructMe_showz_ps20b
{
public:
	ConstructMe_showz_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &showz_ps20b_combos );
	}
};

static ConstructMe_showz_ps20b s_ConstructMe_showz_ps20b;
