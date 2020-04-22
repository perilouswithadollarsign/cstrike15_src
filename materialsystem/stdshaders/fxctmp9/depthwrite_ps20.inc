// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
#include "shaderlib/cshader.h"
class depthwrite_ps20_Static_Index
{
private:
	int m_nCOLOR_DEPTH;
#ifdef _DEBUG
	bool m_bCOLOR_DEPTH;
#endif
public:
	void SetCOLOR_DEPTH( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCOLOR_DEPTH = i;
#ifdef _DEBUG
		m_bCOLOR_DEPTH = true;
#endif
	}
	void SetCOLOR_DEPTH( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nCOLOR_DEPTH = i ? 1 : 0;
#ifdef _DEBUG
		m_bCOLOR_DEPTH = true;
#endif
	}
public:
	// CONSTRUCTOR
	depthwrite_ps20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bCOLOR_DEPTH = false;
#endif // _DEBUG
		m_nCOLOR_DEPTH = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bCOLOR_DEPTH;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 2 * m_nCOLOR_DEPTH ) + 0;
	}
};
#define shaderStaticTest_depthwrite_ps20 psh_forgot_to_set_static_COLOR_DEPTH + 0
class depthwrite_ps20_Dynamic_Index
{
private:
	int m_nALPHACLIP;
#ifdef _DEBUG
	bool m_bALPHACLIP;
#endif
public:
	void SetALPHACLIP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nALPHACLIP = i;
#ifdef _DEBUG
		m_bALPHACLIP = true;
#endif
	}
	void SetALPHACLIP( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nALPHACLIP = i ? 1 : 0;
#ifdef _DEBUG
		m_bALPHACLIP = true;
#endif
	}
public:
	// CONSTRUCTOR
	depthwrite_ps20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bALPHACLIP = false;
#endif // _DEBUG
		m_nALPHACLIP = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bALPHACLIP;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nALPHACLIP ) + 0;
	}
};
#define shaderDynamicTest_depthwrite_ps20 psh_forgot_to_set_dynamic_ALPHACLIP + 0

static const ShaderComboInformation_t s_DynamicComboArray_depthwrite_ps20[1] = 
{
	{ "ALPHACLIP", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_depthwrite_ps20[1] = 
{
	{ "COLOR_DEPTH", 0, 1 },
};
static const ShaderComboSemantics_t depthwrite_ps20_combos =
{
	"depthwrite_ps20", s_DynamicComboArray_depthwrite_ps20, 1, s_StaticComboArray_depthwrite_ps20, 1 
};

class ConstructMe_depthwrite_ps20
{
public:
	ConstructMe_depthwrite_ps20()
	{
		GetShaderDLL()->AddShaderComboInformation( &depthwrite_ps20_combos );
	}
};

static ConstructMe_depthwrite_ps20 s_ConstructMe_depthwrite_ps20;
