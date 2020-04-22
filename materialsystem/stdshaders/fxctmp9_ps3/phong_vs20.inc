// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $SFM == 0 ) && ( $MORPHING )
// ( $SFM == 0 ) && ( $TESSELLATION )
// ( $MORPHING || $SKINNING || $COMPRESSED_VERTS ) && $TESSELLATION
// ( $DOWATERFOG == 0 ) && ( $DOPIXELFOG && $HARDWAREFOGBLEND )
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class phong_vs20_Static_Index
{
private:
	int m_nSFM;
#ifdef _DEBUG
	bool m_bSFM;
#endif
public:
	void SetSFM( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nSFM = i;
#ifdef _DEBUG
		m_bSFM = true;
#endif
	}
	void SetSFM( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nSFM = i ? 1 : 0;
#ifdef _DEBUG
		m_bSFM = true;
#endif
	}
private:
	int m_nWORLD_NORMAL;
#ifdef _DEBUG
	bool m_bWORLD_NORMAL;
#endif
public:
	void SetWORLD_NORMAL( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nWORLD_NORMAL = i;
#ifdef _DEBUG
		m_bWORLD_NORMAL = true;
#endif
	}
	void SetWORLD_NORMAL( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nWORLD_NORMAL = i ? 1 : 0;
#ifdef _DEBUG
		m_bWORLD_NORMAL = true;
#endif
	}
private:
	int m_nFLATTEN_STATIC_CONTROL_FLOW;
#ifdef _DEBUG
	bool m_bFLATTEN_STATIC_CONTROL_FLOW;
#endif
public:
	void SetFLATTEN_STATIC_CONTROL_FLOW( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nFLATTEN_STATIC_CONTROL_FLOW = i;
#ifdef _DEBUG
		m_bFLATTEN_STATIC_CONTROL_FLOW = true;
#endif
	}
	void SetFLATTEN_STATIC_CONTROL_FLOW( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nFLATTEN_STATIC_CONTROL_FLOW = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLATTEN_STATIC_CONTROL_FLOW = true;
#endif
	}
private:
	int m_nDOPIXELFOG;
#ifdef _DEBUG
	bool m_bDOPIXELFOG;
#endif
public:
	void SetDOPIXELFOG( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDOPIXELFOG = i;
#ifdef _DEBUG
		m_bDOPIXELFOG = true;
#endif
	}
	void SetDOPIXELFOG( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDOPIXELFOG = i ? 1 : 0;
#ifdef _DEBUG
		m_bDOPIXELFOG = true;
#endif
	}
public:
	// CONSTRUCTOR
	phong_vs20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bSFM = false;
#endif // _DEBUG
		m_nSFM = 0;
#ifdef _DEBUG
		m_bWORLD_NORMAL = false;
#endif // _DEBUG
		m_nWORLD_NORMAL = 0;
#ifdef _DEBUG
		m_bFLATTEN_STATIC_CONTROL_FLOW = false;
#endif // _DEBUG
		m_nFLATTEN_STATIC_CONTROL_FLOW = 0;
#ifdef _DEBUG
		m_bDOPIXELFOG = true;
#endif // _DEBUG
		m_nDOPIXELFOG = ( g_pHardwareConfig->SupportsPixelShaders_2_b() && !IS_FLAG_SET( MATERIAL_VAR_VERTEXFOG ) ) ? 1 : 0 ;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bSFM && m_bWORLD_NORMAL && m_bFLATTEN_STATIC_CONTROL_FLOW && m_bDOPIXELFOG;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 8 * m_nSFM ) + ( 8 * m_nWORLD_NORMAL ) + ( 8 * m_nFLATTEN_STATIC_CONTROL_FLOW ) + ( 8 * m_nDOPIXELFOG ) + 0;
	}
};
#define shaderStaticTest_phong_vs20 vsh_forgot_to_set_static_SFM + vsh_forgot_to_set_static_WORLD_NORMAL + vsh_forgot_to_set_static_FLATTEN_STATIC_CONTROL_FLOW + 0
class phong_vs20_Dynamic_Index
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
	int m_nMORPHING;
#ifdef _DEBUG
	bool m_bMORPHING;
#endif
public:
	void SetMORPHING( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nMORPHING = i;
#ifdef _DEBUG
		m_bMORPHING = true;
#endif
	}
	void SetMORPHING( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nMORPHING = i ? 1 : 0;
#ifdef _DEBUG
		m_bMORPHING = true;
#endif
	}
private:
	int m_nTESSELLATION;
#ifdef _DEBUG
	bool m_bTESSELLATION;
#endif
public:
	void SetTESSELLATION( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nTESSELLATION = i;
#ifdef _DEBUG
		m_bTESSELLATION = true;
#endif
	}
	void SetTESSELLATION( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nTESSELLATION = i ? 1 : 0;
#ifdef _DEBUG
		m_bTESSELLATION = true;
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
		Assert( i >= 0 && i <= 0 );
		m_nNUM_LIGHTS = i;
#ifdef _DEBUG
		m_bNUM_LIGHTS = true;
#endif
	}
	void SetNUM_LIGHTS( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nNUM_LIGHTS = i ? 1 : 0;
#ifdef _DEBUG
		m_bNUM_LIGHTS = true;
#endif
	}
private:
	int m_nDOWATERFOG;
#ifdef _DEBUG
	bool m_bDOWATERFOG;
#endif
public:
	void SetDOWATERFOG( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDOWATERFOG = i;
#ifdef _DEBUG
		m_bDOWATERFOG = true;
#endif
	}
	void SetDOWATERFOG( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDOWATERFOG = i ? 1 : 0;
#ifdef _DEBUG
		m_bDOWATERFOG = true;
#endif
	}
public:
	// CONSTRUCTOR
	phong_vs20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
		m_bMORPHING = true;
#endif // _DEBUG
		m_nMORPHING = false ;
#ifdef _DEBUG
		m_bTESSELLATION = false;
#endif // _DEBUG
		m_nTESSELLATION = 0;
#ifdef _DEBUG
		m_bNUM_LIGHTS = false;
#endif // _DEBUG
		m_nNUM_LIGHTS = 0;
#ifdef _DEBUG
		m_bDOWATERFOG = true;
#endif // _DEBUG
		m_nDOWATERFOG = ( pShaderAPI->GetSceneFogMode() == MATERIAL_FOG_LINEAR_BELOW_FOG_Z ) ;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bCOMPRESSED_VERTS && m_bSKINNING && m_bMORPHING && m_bTESSELLATION && m_bNUM_LIGHTS && m_bDOWATERFOG;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nCOMPRESSED_VERTS ) + ( 2 * m_nSKINNING ) + ( 4 * m_nMORPHING ) + ( 4 * m_nTESSELLATION ) + ( 4 * m_nNUM_LIGHTS ) + ( 4 * m_nDOWATERFOG ) + 0;
	}
};
#define shaderDynamicTest_phong_vs20 vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + vsh_forgot_to_set_dynamic_SKINNING + vsh_forgot_to_set_dynamic_TESSELLATION + vsh_forgot_to_set_dynamic_NUM_LIGHTS + 0

static const ShaderComboInformation_t s_DynamicComboArray_phong_vs20[6] = 
{
	{ "COMPRESSED_VERTS", 0, 1 },
	{ "SKINNING", 0, 1 },
	{ "MORPHING", 0, 0 },
	{ "TESSELLATION", 0, 0 },
	{ "NUM_LIGHTS", 0, 0 },
	{ "DOWATERFOG", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_phong_vs20[4] = 
{
	{ "SFM", 0, 0 },
	{ "WORLD_NORMAL", 0, 0 },
	{ "FLATTEN_STATIC_CONTROL_FLOW", 0, 0 },
	{ "DOPIXELFOG", 0, 1 },
};
static const ShaderComboSemantics_t phong_vs20_combos =
{
	"phong_vs20", s_DynamicComboArray_phong_vs20, 6, s_StaticComboArray_phong_vs20, 4 
};

class ConstructMe_phong_vs20
{
public:
	ConstructMe_phong_vs20()
	{
		GetShaderDLL()->AddShaderComboInformation( &phong_vs20_combos );
	}
};

static ConstructMe_phong_vs20 s_ConstructMe_phong_vs20;
