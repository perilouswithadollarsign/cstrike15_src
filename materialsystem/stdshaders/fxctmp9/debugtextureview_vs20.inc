// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class debugtextureview_vs20_Static_Index
{
public:
	// CONSTRUCTOR
	debugtextureview_vs20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_debugtextureview_vs20 0
class debugtextureview_vs20_Dynamic_Index
{
private:
	int m_nCOMPRESSED_VERTS;
#ifdef _DEBUG
	bool m_bCOMPRESSED_VERTS;
#endif
public:
	void SetCOMPRESSED_VERTS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCOMPRESSED_VERTS = i;
#ifdef _DEBUG
		m_bCOMPRESSED_VERTS = true;
#endif
	}
	void SetCOMPRESSED_VERTS( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nCOMPRESSED_VERTS = i ? 1 : 0;
#ifdef _DEBUG
		m_bCOMPRESSED_VERTS = true;
#endif
	}
public:
	// CONSTRUCTOR
	debugtextureview_vs20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bCOMPRESSED_VERTS = false;
#endif // _DEBUG
		m_nCOMPRESSED_VERTS = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bCOMPRESSED_VERTS;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nCOMPRESSED_VERTS ) + 0;
	}
};
#define shaderDynamicTest_debugtextureview_vs20 vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + 0

static const ShaderComboInformation_t s_DynamicComboArray_DebugTextureView_vs20[1] = 
{
	{ "COMPRESSED_VERTS", 0, 1 },
};
static const ShaderComboSemantics_t DebugTextureView_vs20_combos =
{
	"DebugTextureView_vs20", s_DynamicComboArray_DebugTextureView_vs20, 1, NULL, 0 
};

class ConstructMe_DebugTextureView_vs20
{
public:
	ConstructMe_DebugTextureView_vs20()
	{
		GetShaderDLL()->AddShaderComboInformation( &DebugTextureView_vs20_combos );
	}
};

static ConstructMe_DebugTextureView_vs20 s_ConstructMe_DebugTextureView_vs20;
