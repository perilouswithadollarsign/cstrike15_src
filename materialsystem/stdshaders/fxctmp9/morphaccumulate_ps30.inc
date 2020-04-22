// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class morphaccumulate_ps30_Static_Index
{
private:
	int m_nCONSTANTBASEDMORPH;
#ifdef _DEBUG
	bool m_bCONSTANTBASEDMORPH;
#endif
public:
	void SetCONSTANTBASEDMORPH( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCONSTANTBASEDMORPH = i;
#ifdef _DEBUG
		m_bCONSTANTBASEDMORPH = true;
#endif
	}
	void SetCONSTANTBASEDMORPH( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nCONSTANTBASEDMORPH = i ? 1 : 0;
#ifdef _DEBUG
		m_bCONSTANTBASEDMORPH = true;
#endif
	}
public:
	// CONSTRUCTOR
	morphaccumulate_ps30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bCONSTANTBASEDMORPH = false;
#endif // _DEBUG
		m_nCONSTANTBASEDMORPH = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bCONSTANTBASEDMORPH;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nCONSTANTBASEDMORPH ) + 0;
	}
};
#define shaderStaticTest_morphaccumulate_ps30 psh_forgot_to_set_static_CONSTANTBASEDMORPH + 0
class morphaccumulate_ps30_Dynamic_Index
{
public:
	// CONSTRUCTOR
	morphaccumulate_ps30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_morphaccumulate_ps30 0

static const ShaderComboInformation_t s_StaticComboArray_morphaccumulate_ps30[1] = 
{
	{ "CONSTANTBASEDMORPH", 0, 1 },
};
static const ShaderComboSemantics_t morphaccumulate_ps30_combos =
{
	"morphaccumulate_ps30", NULL, 0, s_StaticComboArray_morphaccumulate_ps30, 1 
};

class ConstructMe_morphaccumulate_ps30
{
public:
	ConstructMe_morphaccumulate_ps30()
	{
		GetShaderDLL()->AddShaderComboInformation( &morphaccumulate_ps30_combos );
	}
};

static ConstructMe_morphaccumulate_ps30 s_ConstructMe_morphaccumulate_ps30;
