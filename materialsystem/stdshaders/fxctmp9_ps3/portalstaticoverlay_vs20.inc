// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class portalstaticoverlay_vs20_Static_Index
{
private:
	int m_nMODEL;
#ifdef _DEBUG
	bool m_bMODEL;
#endif
public:
	void SetMODEL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMODEL = i;
#ifdef _DEBUG
		m_bMODEL = true;
#endif
	}
	void SetMODEL( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nMODEL = i ? 1 : 0;
#ifdef _DEBUG
		m_bMODEL = true;
#endif
	}
private:
	int m_nPORTALGHOSTOVERLAY;
#ifdef _DEBUG
	bool m_bPORTALGHOSTOVERLAY;
#endif
public:
	void SetPORTALGHOSTOVERLAY( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nPORTALGHOSTOVERLAY = i;
#ifdef _DEBUG
		m_bPORTALGHOSTOVERLAY = true;
#endif
	}
	void SetPORTALGHOSTOVERLAY( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 2 );
		m_nPORTALGHOSTOVERLAY = i ? 1 : 0;
#ifdef _DEBUG
		m_bPORTALGHOSTOVERLAY = true;
#endif
	}
public:
	// CONSTRUCTOR
	portalstaticoverlay_vs20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bMODEL = false;
#endif // _DEBUG
		m_nMODEL = 0;
#ifdef _DEBUG
		m_bPORTALGHOSTOVERLAY = false;
#endif // _DEBUG
		m_nPORTALGHOSTOVERLAY = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bMODEL && m_bPORTALGHOSTOVERLAY;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 2 * m_nMODEL ) + ( 4 * m_nPORTALGHOSTOVERLAY ) + 0;
	}
};
#define shaderStaticTest_portalstaticoverlay_vs20 vsh_forgot_to_set_static_MODEL + vsh_forgot_to_set_static_PORTALGHOSTOVERLAY + 0
class portalstaticoverlay_vs20_Dynamic_Index
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
public:
	// CONSTRUCTOR
	portalstaticoverlay_vs20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bSKINNING = false;
#endif // _DEBUG
		m_nSKINNING = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bSKINNING;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nSKINNING ) + 0;
	}
};
#define shaderDynamicTest_portalstaticoverlay_vs20 vsh_forgot_to_set_dynamic_SKINNING + 0

static const ShaderComboInformation_t s_DynamicComboArray_portalstaticoverlay_vs20[1] = 
{
	{ "SKINNING", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_portalstaticoverlay_vs20[2] = 
{
	{ "MODEL", 0, 1 },
	{ "PORTALGHOSTOVERLAY", 0, 2 },
};
static const ShaderComboSemantics_t portalstaticoverlay_vs20_combos =
{
	"portalstaticoverlay_vs20", s_DynamicComboArray_portalstaticoverlay_vs20, 1, s_StaticComboArray_portalstaticoverlay_vs20, 2 
};

class ConstructMe_portalstaticoverlay_vs20
{
public:
	ConstructMe_portalstaticoverlay_vs20()
	{
		GetShaderDLL()->AddShaderComboInformation( &portalstaticoverlay_vs20_combos );
	}
};

static ConstructMe_portalstaticoverlay_vs20 s_ConstructMe_portalstaticoverlay_vs20;
