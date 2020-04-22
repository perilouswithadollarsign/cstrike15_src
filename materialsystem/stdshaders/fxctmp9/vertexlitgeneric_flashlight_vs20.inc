// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $DOWATERFOG == 0 ) && ( $DOPIXELFOG && $HARDWAREFOGBLEND )
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class vertexlitgeneric_flashlight_vs20_Static_Index
{
private:
	int m_nTEETH;
#ifdef _DEBUG
	bool m_bTEETH;
#endif
public:
	void SetTEETH( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nTEETH = i;
#ifdef _DEBUG
		m_bTEETH = true;
#endif
	}
	void SetTEETH( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nTEETH = i ? 1 : 0;
#ifdef _DEBUG
		m_bTEETH = true;
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
private:
	int m_nHARDWAREFOGBLEND;
#ifdef _DEBUG
	bool m_bHARDWAREFOGBLEND;
#endif
public:
	void SetHARDWAREFOGBLEND( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nHARDWAREFOGBLEND = i;
#ifdef _DEBUG
		m_bHARDWAREFOGBLEND = true;
#endif
	}
	void SetHARDWAREFOGBLEND( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nHARDWAREFOGBLEND = i ? 1 : 0;
#ifdef _DEBUG
		m_bHARDWAREFOGBLEND = true;
#endif
	}
public:
	// CONSTRUCTOR
	vertexlitgeneric_flashlight_vs20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bTEETH = false;
#endif // _DEBUG
		m_nTEETH = 0;
#ifdef _DEBUG
		m_bDOPIXELFOG = true;
#endif // _DEBUG
		m_nDOPIXELFOG = ( g_pHardwareConfig->SupportsPixelShaders_2_b() && !IS_FLAG_SET( MATERIAL_VAR_VERTEXFOG ) ) ? 1 : 0 ;
#ifdef _DEBUG
		m_bHARDWAREFOGBLEND = true;
#endif // _DEBUG
		m_nHARDWAREFOGBLEND = !g_pHardwareConfig->SupportsPixelShaders_2_b() ;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bTEETH && m_bDOPIXELFOG && m_bHARDWAREFOGBLEND;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 4 * m_nTEETH ) + ( 8 * m_nDOPIXELFOG ) + ( 16 * m_nHARDWAREFOGBLEND ) + 0;
	}
};
#define shaderStaticTest_vertexlitgeneric_flashlight_vs20 vsh_forgot_to_set_static_TEETH + 0
class vertexlitgeneric_flashlight_vs20_Dynamic_Index
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
	vertexlitgeneric_flashlight_vs20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bSKINNING = false;
#endif // _DEBUG
		m_nSKINNING = 0;
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
		bool bAllDynamicVarsDefined = m_bSKINNING && m_bDOWATERFOG;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nSKINNING ) + ( 2 * m_nDOWATERFOG ) + 0;
	}
};
#define shaderDynamicTest_vertexlitgeneric_flashlight_vs20 vsh_forgot_to_set_dynamic_SKINNING + 0

static const ShaderComboInformation_t s_DynamicComboArray_vertexlitgeneric_flashlight_vs20[2] = 
{
	{ "SKINNING", 0, 1 },
	{ "DOWATERFOG", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_vertexlitgeneric_flashlight_vs20[3] = 
{
	{ "TEETH", 0, 1 },
	{ "DOPIXELFOG", 0, 1 },
	{ "HARDWAREFOGBLEND", 0, 1 },
};
static const ShaderComboSemantics_t vertexlitgeneric_flashlight_vs20_combos =
{
	"vertexlitgeneric_flashlight_vs20", s_DynamicComboArray_vertexlitgeneric_flashlight_vs20, 2, s_StaticComboArray_vertexlitgeneric_flashlight_vs20, 3 
};

class ConstructMe_vertexlitgeneric_flashlight_vs20
{
public:
	ConstructMe_vertexlitgeneric_flashlight_vs20()
	{
		GetShaderDLL()->AddShaderComboInformation( &vertexlitgeneric_flashlight_vs20_combos );
	}
};

static ConstructMe_vertexlitgeneric_flashlight_vs20 s_ConstructMe_vertexlitgeneric_flashlight_vs20;
