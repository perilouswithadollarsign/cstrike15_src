// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class watercheap_vs20_Static_Index
{
private:
	int m_nBLEND;
#ifdef _DEBUG
	bool m_bBLEND;
#endif
public:
	void SetBLEND( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBLEND = i;
#ifdef _DEBUG
		m_bBLEND = true;
#endif
	}
	void SetBLEND( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nBLEND = i ? 1 : 0;
#ifdef _DEBUG
		m_bBLEND = true;
#endif
	}
public:
	// CONSTRUCTOR
	watercheap_vs20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bBLEND = false;
#endif // _DEBUG
		m_nBLEND = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bBLEND;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nBLEND ) + 0;
	}
};
#define shaderStaticTest_watercheap_vs20 vsh_forgot_to_set_static_BLEND + 0
class watercheap_vs20_Dynamic_Index
{
public:
	// CONSTRUCTOR
	watercheap_vs20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_watercheap_vs20 0

static const ShaderComboInformation_t s_StaticComboArray_watercheap_vs20[1] = 
{
	{ "BLEND", 0, 1 },
};
static const ShaderComboSemantics_t watercheap_vs20_combos =
{
	"watercheap_vs20", NULL, 0, s_StaticComboArray_watercheap_vs20, 1 
};

class ConstructMe_watercheap_vs20
{
public:
	ConstructMe_watercheap_vs20()
	{
		GetShaderDLL()->AddShaderComboInformation( &watercheap_vs20_combos );
	}
};

static ConstructMe_watercheap_vs20 s_ConstructMe_watercheap_vs20;
