// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class particlesphere_vs20_Static_Index
{
public:
	// CONSTRUCTOR
	particlesphere_vs20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_particlesphere_vs20 0
class particlesphere_vs20_Dynamic_Index
{
private:
	int m_nFOGTYPE;
#ifdef _DEBUG
	bool m_bFOGTYPE;
#endif
public:
	void SetFOGTYPE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFOGTYPE = i;
#ifdef _DEBUG
		m_bFOGTYPE = true;
#endif
	}
	void SetFOGTYPE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nFOGTYPE = i ? 1 : 0;
#ifdef _DEBUG
		m_bFOGTYPE = true;
#endif
	}
public:
	// CONSTRUCTOR
	particlesphere_vs20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bFOGTYPE = false;
#endif // _DEBUG
		m_nFOGTYPE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_particlesphere_vs20 vsh_forgot_to_set_dynamic_FOGTYPE + 0

static const ShaderComboInformation_t s_DynamicComboArray_particlesphere_vs20[1] = 
{
	{ "FOGTYPE", 0, 1 },
};
static const ShaderComboSemantics_t particlesphere_vs20_combos =
{
	"particlesphere_vs20", s_DynamicComboArray_particlesphere_vs20, 1, NULL, 0 
};

class ConstructMe_particlesphere_vs20
{
public:
	ConstructMe_particlesphere_vs20()
	{
		GetShaderDLL()->AddShaderComboInformation( &particlesphere_vs20_combos );
	}
};

static ConstructMe_particlesphere_vs20 s_ConstructMe_particlesphere_vs20;
