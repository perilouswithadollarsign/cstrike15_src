// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $DOWATERFOG == 0 ) && ( $DOPIXELFOG && $HARDWAREFOGBLEND )
// ( $DOWATERFOG == 0 ) && ( $HARDWAREFOGBLEND == 0 ) && ( $DOPIXELFOG == 0 )
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class shadow_vs20_Static_Index
{
private:
	int m_nDEFERRED_SHADOWS;
#ifdef _DEBUG
	bool m_bDEFERRED_SHADOWS;
#endif
public:
	void SetDEFERRED_SHADOWS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDEFERRED_SHADOWS = i;
#ifdef _DEBUG
		m_bDEFERRED_SHADOWS = true;
#endif
	}
	void SetDEFERRED_SHADOWS( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDEFERRED_SHADOWS = i ? 1 : 0;
#ifdef _DEBUG
		m_bDEFERRED_SHADOWS = true;
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
	shadow_vs20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bDEFERRED_SHADOWS = false;
#endif // _DEBUG
		m_nDEFERRED_SHADOWS = 0;
#ifdef _DEBUG
		m_bDOPIXELFOG = true;
#endif // _DEBUG
		m_nDOPIXELFOG = g_pHardwareConfig->SupportsPixelShaders_2_b() ;
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
		bool bAllStaticVarsDefined = m_bDEFERRED_SHADOWS && m_bDOPIXELFOG && m_bHARDWAREFOGBLEND;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 2 * m_nDEFERRED_SHADOWS ) + ( 4 * m_nDOPIXELFOG ) + ( 8 * m_nHARDWAREFOGBLEND ) + 0;
	}
};
#define shaderStaticTest_shadow_vs20 vsh_forgot_to_set_static_DEFERRED_SHADOWS + 0
class shadow_vs20_Dynamic_Index
{
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
	shadow_vs20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
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
		bool bAllDynamicVarsDefined = m_bDOWATERFOG;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nDOWATERFOG ) + 0;
	}
};
#define shaderDynamicTest_shadow_vs20 0

static const ShaderComboInformation_t s_DynamicComboArray_shadow_vs20[1] = 
{
	{ "DOWATERFOG", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_shadow_vs20[3] = 
{
	{ "DEFERRED_SHADOWS", 0, 1 },
	{ "DOPIXELFOG", 0, 1 },
	{ "HARDWAREFOGBLEND", 0, 1 },
};
static const ShaderComboSemantics_t shadow_vs20_combos =
{
	"shadow_vs20", s_DynamicComboArray_shadow_vs20, 1, s_StaticComboArray_shadow_vs20, 3 
};

class ConstructMe_shadow_vs20
{
public:
	ConstructMe_shadow_vs20()
	{
		GetShaderDLL()->AddShaderComboInformation( &shadow_vs20_combos );
	}
};

static ConstructMe_shadow_vs20 s_ConstructMe_shadow_vs20;
