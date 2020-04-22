// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $MODE == 1 )
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
class embroider_ps30_Static_Index
{
private:
	int m_nMODE;
#ifdef _DEBUG
	bool m_bMODE;
#endif
public:
	void SetMODE( int i )
	{
		Assert( i >= 0 && i <= 3 );
		m_nMODE = i;
#ifdef _DEBUG
		m_bMODE = true;
#endif
	}
	void SetMODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 3 );
		m_nMODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bMODE = true;
#endif
	}
private:
	int m_nPALETTIZE;
#ifdef _DEBUG
	bool m_bPALETTIZE;
#endif
public:
	void SetPALETTIZE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPALETTIZE = i;
#ifdef _DEBUG
		m_bPALETTIZE = true;
#endif
	}
	void SetPALETTIZE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nPALETTIZE = i ? 1 : 0;
#ifdef _DEBUG
		m_bPALETTIZE = true;
#endif
	}
public:
	// CONSTRUCTOR
	embroider_ps30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bMODE = false;
#endif // _DEBUG
		m_nMODE = 0;
#ifdef _DEBUG
		m_bPALETTIZE = false;
#endif // _DEBUG
		m_nPALETTIZE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bMODE && m_bPALETTIZE;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nMODE ) + ( 4 * m_nPALETTIZE ) + 0;
	}
};
#define shaderStaticTest_embroider_ps30 psh_forgot_to_set_static_MODE + psh_forgot_to_set_static_PALETTIZE + 0
class embroider_ps30_Dynamic_Index
{
public:
	// CONSTRUCTOR
	embroider_ps30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_embroider_ps30 0

static const ShaderComboInformation_t s_StaticComboArray_embroider_ps30[2] = 
{
	{ "MODE", 0, 3 },
	{ "PALETTIZE", 0, 1 },
};
static const ShaderComboSemantics_t embroider_ps30_combos =
{
	"embroider_ps30", NULL, 0, s_StaticComboArray_embroider_ps30, 2 
};

class ConstructMe_embroider_ps30
{
public:
	ConstructMe_embroider_ps30()
	{
		GetShaderDLL()->AddShaderComboInformation( &embroider_ps30_combos );
	}
};

static ConstructMe_embroider_ps30 s_ConstructMe_embroider_ps30;
