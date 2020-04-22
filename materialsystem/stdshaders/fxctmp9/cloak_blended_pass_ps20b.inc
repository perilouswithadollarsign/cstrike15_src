// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class cloak_blended_pass_ps20b_Static_Index
{
private:
	int m_nBUMPMAP;
#ifdef _DEBUG
	bool m_bBUMPMAP;
#endif
public:
	void SetBUMPMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBUMPMAP = i;
#ifdef _DEBUG
		m_bBUMPMAP = true;
#endif
	}
	void SetBUMPMAP( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nBUMPMAP = i ? 1 : 0;
#ifdef _DEBUG
		m_bBUMPMAP = true;
#endif
	}
public:
	// CONSTRUCTOR
	cloak_blended_pass_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bBUMPMAP = false;
#endif // _DEBUG
		m_nBUMPMAP = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bBUMPMAP;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nBUMPMAP ) + 0;
	}
};
#define shaderStaticTest_cloak_blended_pass_ps20b psh_forgot_to_set_static_BUMPMAP + 0
class cloak_blended_pass_ps20b_Dynamic_Index
{
public:
	// CONSTRUCTOR
	cloak_blended_pass_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_cloak_blended_pass_ps20b 0

static const ShaderComboInformation_t s_StaticComboArray_cloak_blended_pass_ps20b[1] = 
{
	{ "BUMPMAP", 0, 1 },
};
static const ShaderComboSemantics_t cloak_blended_pass_ps20b_combos =
{
	"cloak_blended_pass_ps20b", NULL, 0, s_StaticComboArray_cloak_blended_pass_ps20b, 1 
};

class ConstructMe_cloak_blended_pass_ps20b
{
public:
	ConstructMe_cloak_blended_pass_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &cloak_blended_pass_ps20b_combos );
	}
};

static ConstructMe_cloak_blended_pass_ps20b s_ConstructMe_cloak_blended_pass_ps20b;
