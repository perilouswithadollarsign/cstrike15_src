// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class lightmappedreflective_vs20_Static_Index
{
private:
	int m_nBASETEXTURE;
#ifdef _DEBUG
	bool m_bBASETEXTURE;
#endif
public:
	void SetBASETEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBASETEXTURE = i;
#ifdef _DEBUG
		m_bBASETEXTURE = true;
#endif
	}
	void SetBASETEXTURE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nBASETEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bBASETEXTURE = true;
#endif
	}
public:
	// CONSTRUCTOR
	lightmappedreflective_vs20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bBASETEXTURE = false;
#endif // _DEBUG
		m_nBASETEXTURE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bBASETEXTURE;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nBASETEXTURE ) + 0;
	}
};
#define shaderStaticTest_lightmappedreflective_vs20 vsh_forgot_to_set_static_BASETEXTURE + 0
class lightmappedreflective_vs20_Dynamic_Index
{
public:
	// CONSTRUCTOR
	lightmappedreflective_vs20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_lightmappedreflective_vs20 0

static const ShaderComboInformation_t s_StaticComboArray_lightmappedreflective_vs20[1] = 
{
	{ "BASETEXTURE", 0, 1 },
};
static const ShaderComboSemantics_t lightmappedreflective_vs20_combos =
{
	"lightmappedreflective_vs20", NULL, 0, s_StaticComboArray_lightmappedreflective_vs20, 1 
};

class ConstructMe_lightmappedreflective_vs20
{
public:
	ConstructMe_lightmappedreflective_vs20()
	{
		GetShaderDLL()->AddShaderComboInformation( &lightmappedreflective_vs20_combos );
	}
};

static ConstructMe_lightmappedreflective_vs20 s_ConstructMe_lightmappedreflective_vs20;
