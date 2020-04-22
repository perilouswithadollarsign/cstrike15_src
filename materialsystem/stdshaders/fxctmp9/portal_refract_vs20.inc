// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class portal_refract_vs20_Static_Index
{
private:
	int m_nSTAGE;
#ifdef _DEBUG
	bool m_bSTAGE;
#endif
public:
	void SetSTAGE( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nSTAGE = i;
#ifdef _DEBUG
		m_bSTAGE = true;
#endif
	}
	void SetSTAGE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 2 );
		m_nSTAGE = i ? 1 : 0;
#ifdef _DEBUG
		m_bSTAGE = true;
#endif
	}
private:
	int m_nUSEONSTATICPROP;
#ifdef _DEBUG
	bool m_bUSEONSTATICPROP;
#endif
public:
	void SetUSEONSTATICPROP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nUSEONSTATICPROP = i;
#ifdef _DEBUG
		m_bUSEONSTATICPROP = true;
#endif
	}
	void SetUSEONSTATICPROP( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nUSEONSTATICPROP = i ? 1 : 0;
#ifdef _DEBUG
		m_bUSEONSTATICPROP = true;
#endif
	}
public:
	// CONSTRUCTOR
	portal_refract_vs20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bSTAGE = false;
#endif // _DEBUG
		m_nSTAGE = 0;
#ifdef _DEBUG
		m_bUSEONSTATICPROP = false;
#endif // _DEBUG
		m_nUSEONSTATICPROP = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bSTAGE && m_bUSEONSTATICPROP;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 2 * m_nSTAGE ) + ( 6 * m_nUSEONSTATICPROP ) + 0;
	}
};
#define shaderStaticTest_portal_refract_vs20 vsh_forgot_to_set_static_STAGE + vsh_forgot_to_set_static_USEONSTATICPROP + 0
class portal_refract_vs20_Dynamic_Index
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
	portal_refract_vs20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_portal_refract_vs20 vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + 0

static const ShaderComboInformation_t s_DynamicComboArray_portal_refract_vs20[1] = 
{
	{ "COMPRESSED_VERTS", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_portal_refract_vs20[2] = 
{
	{ "STAGE", 0, 2 },
	{ "USEONSTATICPROP", 0, 1 },
};
static const ShaderComboSemantics_t portal_refract_vs20_combos =
{
	"portal_refract_vs20", s_DynamicComboArray_portal_refract_vs20, 1, s_StaticComboArray_portal_refract_vs20, 2 
};

class ConstructMe_portal_refract_vs20
{
public:
	ConstructMe_portal_refract_vs20()
	{
		GetShaderDLL()->AddShaderComboInformation( &portal_refract_vs20_combos );
	}
};

static ConstructMe_portal_refract_vs20 s_ConstructMe_portal_refract_vs20;
