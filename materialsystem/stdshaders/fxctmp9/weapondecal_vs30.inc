// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class weapondecal_vs30_Static_Index
{
private:
	int m_nMIRROR;
#ifdef _DEBUG
	bool m_bMIRROR;
#endif
public:
	void SetMIRROR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMIRROR = i;
#ifdef _DEBUG
		m_bMIRROR = true;
#endif
	}
	void SetMIRROR( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nMIRROR = i ? 1 : 0;
#ifdef _DEBUG
		m_bMIRROR = true;
#endif
	}
private:
	int m_nNORMALMAP;
#ifdef _DEBUG
	bool m_bNORMALMAP;
#endif
public:
	void SetNORMALMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nNORMALMAP = i;
#ifdef _DEBUG
		m_bNORMALMAP = true;
#endif
	}
	void SetNORMALMAP( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nNORMALMAP = i ? 1 : 0;
#ifdef _DEBUG
		m_bNORMALMAP = true;
#endif
	}
public:
	// CONSTRUCTOR
	weapondecal_vs30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bMIRROR = false;
#endif // _DEBUG
		m_nMIRROR = 0;
#ifdef _DEBUG
		m_bNORMALMAP = false;
#endif // _DEBUG
		m_nNORMALMAP = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bMIRROR && m_bNORMALMAP;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 20 * m_nMIRROR ) + ( 40 * m_nNORMALMAP ) + 0;
	}
};
#define shaderStaticTest_weapondecal_vs30 vsh_forgot_to_set_static_MIRROR + vsh_forgot_to_set_static_NORMALMAP + 0
class weapondecal_vs30_Dynamic_Index
{
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
	int m_nNUM_LIGHTS;
#ifdef _DEBUG
	bool m_bNUM_LIGHTS;
#endif
public:
	void SetNUM_LIGHTS( int i )
	{
		Assert( i >= 0 && i <= 4 );
		m_nNUM_LIGHTS = i;
#ifdef _DEBUG
		m_bNUM_LIGHTS = true;
#endif
	}
	void SetNUM_LIGHTS( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 4 );
		m_nNUM_LIGHTS = i ? 1 : 0;
#ifdef _DEBUG
		m_bNUM_LIGHTS = true;
#endif
	}
public:
	// CONSTRUCTOR
	weapondecal_vs30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bSKINNING = false;
#endif // _DEBUG
		m_nSKINNING = 0;
#ifdef _DEBUG
		m_bCOMPRESSED_VERTS = false;
#endif // _DEBUG
		m_nCOMPRESSED_VERTS = 0;
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
		bool bAllDynamicVarsDefined = m_bSKINNING && m_bCOMPRESSED_VERTS && m_bNUM_LIGHTS;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nSKINNING ) + ( 2 * m_nCOMPRESSED_VERTS ) + ( 4 * m_nNUM_LIGHTS ) + 0;
	}
};
#define shaderDynamicTest_weapondecal_vs30 vsh_forgot_to_set_dynamic_SKINNING + vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + vsh_forgot_to_set_dynamic_NUM_LIGHTS + 0

static const ShaderComboInformation_t s_DynamicComboArray_weapondecal_vs30[3] = 
{
	{ "SKINNING", 0, 1 },
	{ "COMPRESSED_VERTS", 0, 1 },
	{ "NUM_LIGHTS", 0, 4 },
};

static const ShaderComboInformation_t s_StaticComboArray_weapondecal_vs30[2] = 
{
	{ "MIRROR", 0, 1 },
	{ "NORMALMAP", 0, 1 },
};
static const ShaderComboSemantics_t weapondecal_vs30_combos =
{
	"weapondecal_vs30", s_DynamicComboArray_weapondecal_vs30, 3, s_StaticComboArray_weapondecal_vs30, 2 
};

class ConstructMe_weapondecal_vs30
{
public:
	ConstructMe_weapondecal_vs30()
	{
		GetShaderDLL()->AddShaderComboInformation( &weapondecal_vs30_combos );
	}
};

static ConstructMe_weapondecal_vs30 s_ConstructMe_weapondecal_vs30;
