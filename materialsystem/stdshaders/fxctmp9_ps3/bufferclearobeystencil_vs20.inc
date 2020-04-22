// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class bufferclearobeystencil_vs20_Static_Index
{
private:
	int m_nUSESCOLOR;
#ifdef _DEBUG
	bool m_bUSESCOLOR;
#endif
public:
	void SetUSESCOLOR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nUSESCOLOR = i;
#ifdef _DEBUG
		m_bUSESCOLOR = true;
#endif
	}
	void SetUSESCOLOR( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nUSESCOLOR = i ? 1 : 0;
#ifdef _DEBUG
		m_bUSESCOLOR = true;
#endif
	}
public:
	// CONSTRUCTOR
	bufferclearobeystencil_vs20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bUSESCOLOR = false;
#endif // _DEBUG
		m_nUSESCOLOR = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bUSESCOLOR;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nUSESCOLOR ) + 0;
	}
};
#define shaderStaticTest_bufferclearobeystencil_vs20 vsh_forgot_to_set_static_USESCOLOR + 0
class bufferclearobeystencil_vs20_Dynamic_Index
{
public:
	// CONSTRUCTOR
	bufferclearobeystencil_vs20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_bufferclearobeystencil_vs20 0

static const ShaderComboInformation_t s_StaticComboArray_bufferclearobeystencil_vs20[1] = 
{
	{ "USESCOLOR", 0, 1 },
};
static const ShaderComboSemantics_t bufferclearobeystencil_vs20_combos =
{
	"bufferclearobeystencil_vs20", NULL, 0, s_StaticComboArray_bufferclearobeystencil_vs20, 1 
};

class ConstructMe_bufferclearobeystencil_vs20
{
public:
	ConstructMe_bufferclearobeystencil_vs20()
	{
		GetShaderDLL()->AddShaderComboInformation( &bufferclearobeystencil_vs20_combos );
	}
};

static ConstructMe_bufferclearobeystencil_vs20 s_ConstructMe_bufferclearobeystencil_vs20;
