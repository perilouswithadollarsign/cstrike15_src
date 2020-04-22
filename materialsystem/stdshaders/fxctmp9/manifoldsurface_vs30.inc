// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class manifoldsurface_vs30_Static_Index
{
private:
	int m_nVERTEX_ALPHA_BIAS;
#ifdef _DEBUG
	bool m_bVERTEX_ALPHA_BIAS;
#endif
public:
	void SetVERTEX_ALPHA_BIAS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nVERTEX_ALPHA_BIAS = i;
#ifdef _DEBUG
		m_bVERTEX_ALPHA_BIAS = true;
#endif
	}
	void SetVERTEX_ALPHA_BIAS( bool i )
	{
		m_nVERTEX_ALPHA_BIAS = i ? 1 : 0;
#ifdef _DEBUG
		m_bVERTEX_ALPHA_BIAS = true;
#endif
	}
public:
	// CONSTRUCTOR
	manifoldsurface_vs30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bVERTEX_ALPHA_BIAS = false;
#endif // _DEBUG
		m_nVERTEX_ALPHA_BIAS = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bVERTEX_ALPHA_BIAS;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 4 * m_nVERTEX_ALPHA_BIAS ) + 0;
	}
};
#define shaderStaticTest_manifoldsurface_vs30 vsh_forgot_to_set_static_VERTEX_ALPHA_BIAS + 0
class manifoldsurface_vs30_Dynamic_Index
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
		m_nSKINNING = i ? 1 : 0;
#ifdef _DEBUG
		m_bSKINNING = true;
#endif
	}
public:
	// CONSTRUCTOR
	manifoldsurface_vs30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bCOMPRESSED_VERTS = false;
#endif // _DEBUG
		m_nCOMPRESSED_VERTS = 0;
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
		bool bAllDynamicVarsDefined = m_bCOMPRESSED_VERTS && m_bSKINNING;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nCOMPRESSED_VERTS ) + ( 2 * m_nSKINNING ) + 0;
	}
};
#define shaderDynamicTest_manifoldsurface_vs30 vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + vsh_forgot_to_set_dynamic_SKINNING + 0
