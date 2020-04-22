// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class blob_vs30_Static_Index
{
private:
	int m_nCONTACT_SHADOW;
#ifdef _DEBUG
	bool m_bCONTACT_SHADOW;
#endif
public:
	void SetCONTACT_SHADOW( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCONTACT_SHADOW = i;
#ifdef _DEBUG
		m_bCONTACT_SHADOW = true;
#endif
	}
	void SetCONTACT_SHADOW( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nCONTACT_SHADOW = i ? 1 : 0;
#ifdef _DEBUG
		m_bCONTACT_SHADOW = true;
#endif
	}
private:
	int m_nVOLUME_TEXTURE;
#ifdef _DEBUG
	bool m_bVOLUME_TEXTURE;
#endif
public:
	void SetVOLUME_TEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nVOLUME_TEXTURE = i;
#ifdef _DEBUG
		m_bVOLUME_TEXTURE = true;
#endif
	}
	void SetVOLUME_TEXTURE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nVOLUME_TEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bVOLUME_TEXTURE = true;
#endif
	}
public:
	// CONSTRUCTOR
	blob_vs30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bCONTACT_SHADOW = false;
#endif // _DEBUG
		m_nCONTACT_SHADOW = 0;
#ifdef _DEBUG
		m_bVOLUME_TEXTURE = false;
#endif // _DEBUG
		m_nVOLUME_TEXTURE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bCONTACT_SHADOW && m_bVOLUME_TEXTURE;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 4 * m_nCONTACT_SHADOW ) + ( 8 * m_nVOLUME_TEXTURE ) + 0;
	}
};
#define shaderStaticTest_blob_vs30 vsh_forgot_to_set_static_CONTACT_SHADOW + vsh_forgot_to_set_static_VOLUME_TEXTURE + 0
class blob_vs30_Dynamic_Index
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
public:
	// CONSTRUCTOR
	blob_vs30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_blob_vs30 vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + vsh_forgot_to_set_dynamic_SKINNING + 0

static const ShaderComboInformation_t s_DynamicComboArray_blob_vs30[2] = 
{
	{ "COMPRESSED_VERTS", 0, 1 },
	{ "SKINNING", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_blob_vs30[2] = 
{
	{ "CONTACT_SHADOW", 0, 1 },
	{ "VOLUME_TEXTURE", 0, 1 },
};
static const ShaderComboSemantics_t blob_vs30_combos =
{
	"blob_vs30", s_DynamicComboArray_blob_vs30, 2, s_StaticComboArray_blob_vs30, 2 
};

class ConstructMe_blob_vs30
{
public:
	ConstructMe_blob_vs30()
	{
		GetShaderDLL()->AddShaderComboInformation( &blob_vs30_combos );
	}
};

static ConstructMe_blob_vs30 s_ConstructMe_blob_vs30;
