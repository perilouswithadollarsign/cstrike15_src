// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class blob_arm_vs20_Static_Index
{
private:
	int m_nWIDEN_TIPS;
#ifdef _DEBUG
	bool m_bWIDEN_TIPS;
#endif
public:
	void SetWIDEN_TIPS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nWIDEN_TIPS = i;
#ifdef _DEBUG
		m_bWIDEN_TIPS = true;
#endif
	}
	void SetWIDEN_TIPS( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nWIDEN_TIPS = i ? 1 : 0;
#ifdef _DEBUG
		m_bWIDEN_TIPS = true;
#endif
	}
public:
	// CONSTRUCTOR
	blob_arm_vs20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bWIDEN_TIPS = false;
#endif // _DEBUG
		m_nWIDEN_TIPS = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bWIDEN_TIPS;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 2 * m_nWIDEN_TIPS ) + 0;
	}
};
#define shaderStaticTest_blob_arm_vs20 vsh_forgot_to_set_static_WIDEN_TIPS + 0
class blob_arm_vs20_Dynamic_Index
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
	blob_arm_vs20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_blob_arm_vs20 vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + 0

static const ShaderComboInformation_t s_DynamicComboArray_blob_arm_vs20[1] = 
{
	{ "COMPRESSED_VERTS", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_blob_arm_vs20[1] = 
{
	{ "WIDEN_TIPS", 0, 1 },
};
static const ShaderComboSemantics_t blob_arm_vs20_combos =
{
	"blob_arm_vs20", s_DynamicComboArray_blob_arm_vs20, 1, s_StaticComboArray_blob_arm_vs20, 1 
};

class ConstructMe_blob_arm_vs20
{
public:
	ConstructMe_blob_arm_vs20()
	{
		GetShaderDLL()->AddShaderComboInformation( &blob_arm_vs20_combos );
	}
};

static ConstructMe_blob_arm_vs20 s_ConstructMe_blob_arm_vs20;
