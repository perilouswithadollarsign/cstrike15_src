// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $FLATTEN_STATIC_CONTROL_FLOW == 0 ) && ( $NUM_LIGHTS > 0 )  [PC]
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class paintblob_vs20_Static_Index
{
private:
	int m_nFLATTEN_STATIC_CONTROL_FLOW;
#ifdef _DEBUG
	bool m_bFLATTEN_STATIC_CONTROL_FLOW;
#endif
public:
	void SetFLATTEN_STATIC_CONTROL_FLOW( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFLATTEN_STATIC_CONTROL_FLOW = i;
#ifdef _DEBUG
		m_bFLATTEN_STATIC_CONTROL_FLOW = true;
#endif
	}
	void SetFLATTEN_STATIC_CONTROL_FLOW( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nFLATTEN_STATIC_CONTROL_FLOW = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLATTEN_STATIC_CONTROL_FLOW = true;
#endif
	}
public:
	// CONSTRUCTOR
	paintblob_vs20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bFLATTEN_STATIC_CONTROL_FLOW = false;
#endif // _DEBUG
		m_nFLATTEN_STATIC_CONTROL_FLOW = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bFLATTEN_STATIC_CONTROL_FLOW;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 12 * m_nFLATTEN_STATIC_CONTROL_FLOW ) + 0;
	}
};
#define shaderStaticTest_paintblob_vs20 vsh_forgot_to_set_static_FLATTEN_STATIC_CONTROL_FLOW + 0
class paintblob_vs20_Dynamic_Index
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
private:
	int m_nSKINNING;
#ifdef _DEBUG
	bool m_bSKINNING;
#endif
public:
	void SetSKINNING( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSKINNING = i;
#ifdef _DEBUG
		m_bSKINNING = true;
#endif
	}
	void SetSKINNING( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nSKINNING = i ? 1 : 0;
#ifdef _DEBUG
		m_bSKINNING = true;
#endif
	}
private:
	int m_nNUM_LIGHTS;
#ifdef _DEBUG
	bool m_bNUM_LIGHTS;
#endif
public:
	void SetNUM_LIGHTS( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nNUM_LIGHTS = i;
#ifdef _DEBUG
		m_bNUM_LIGHTS = true;
#endif
	}
	void SetNUM_LIGHTS( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 2 );
		m_nNUM_LIGHTS = i ? 1 : 0;
#ifdef _DEBUG
		m_bNUM_LIGHTS = true;
#endif
	}
public:
	// CONSTRUCTOR
	paintblob_vs20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bCOMPRESSED_VERTS = false;
#endif // _DEBUG
		m_nCOMPRESSED_VERTS = 0;
#ifdef _DEBUG
		m_bSKINNING = false;
#endif // _DEBUG
		m_nSKINNING = 0;
#ifdef _DEBUG
		m_bNUM_LIGHTS = false;
#endif // _DEBUG
		m_nNUM_LIGHTS = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bCOMPRESSED_VERTS && m_bSKINNING && m_bNUM_LIGHTS;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nCOMPRESSED_VERTS ) + ( 2 * m_nSKINNING ) + ( 4 * m_nNUM_LIGHTS ) + 0;
	}
};
#define shaderDynamicTest_paintblob_vs20 vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + vsh_forgot_to_set_dynamic_SKINNING + vsh_forgot_to_set_dynamic_NUM_LIGHTS + 0

static const ShaderComboInformation_t s_DynamicComboArray_paintblob_vs20[3] = 
{
	{ "COMPRESSED_VERTS", 0, 1 },
	{ "SKINNING", 0, 1 },
	{ "NUM_LIGHTS", 0, 2 },
};

static const ShaderComboInformation_t s_StaticComboArray_paintblob_vs20[1] = 
{
	{ "FLATTEN_STATIC_CONTROL_FLOW", 0, 1 },
};
static const ShaderComboSemantics_t paintblob_vs20_combos =
{
	"paintblob_vs20", s_DynamicComboArray_paintblob_vs20, 3, s_StaticComboArray_paintblob_vs20, 1 
};

class ConstructMe_paintblob_vs20
{
public:
	ConstructMe_paintblob_vs20()
	{
		GetShaderDLL()->AddShaderComboInformation( &paintblob_vs20_combos );
	}
};

static ConstructMe_paintblob_vs20 s_ConstructMe_paintblob_vs20;
