// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class blurfilter_vs20_Static_Index
{
private:
	int m_nKERNEL;
#ifdef _DEBUG
	bool m_bKERNEL;
#endif
public:
	void SetKERNEL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nKERNEL = i;
#ifdef _DEBUG
		m_bKERNEL = true;
#endif
	}
	void SetKERNEL( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nKERNEL = i ? 1 : 0;
#ifdef _DEBUG
		m_bKERNEL = true;
#endif
	}
public:
	// CONSTRUCTOR
	blurfilter_vs20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bKERNEL = false;
#endif // _DEBUG
		m_nKERNEL = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bKERNEL;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nKERNEL ) + 0;
	}
};
#define shaderStaticTest_blurfilter_vs20 vsh_forgot_to_set_static_KERNEL + 0
class blurfilter_vs20_Dynamic_Index
{
public:
	// CONSTRUCTOR
	blurfilter_vs20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_blurfilter_vs20 0

static const ShaderComboInformation_t s_StaticComboArray_blurfilter_vs20[1] = 
{
	{ "KERNEL", 0, 1 },
};
static const ShaderComboSemantics_t blurfilter_vs20_combos =
{
	"blurfilter_vs20", NULL, 0, s_StaticComboArray_blurfilter_vs20, 1 
};

class ConstructMe_blurfilter_vs20
{
public:
	ConstructMe_blurfilter_vs20()
	{
		GetShaderDLL()->AddShaderComboInformation( &blurfilter_vs20_combos );
	}
};

static ConstructMe_blurfilter_vs20 s_ConstructMe_blurfilter_vs20;
