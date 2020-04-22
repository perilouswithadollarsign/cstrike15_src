// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( ( $DETAIL1 == 0 ) && ( $DETAIL2 != 0 ) )
// ( $TANGENTTOPACITY && $TANGENTSOPACITY )
// ( $FLOWMAP && ( $DETAIL1 || $DETAIL2 ) )
// ( ( $FLOWMAP == 0 ) && ( $VORTEX1 || $VORTEX2 ) )
// ( $DOWATERFOG == 0 ) && ( $DOPIXELFOG && $HARDWAREFOGBLEND )
// ( $DOWATERFOG == 0 ) && ( $HARDWAREFOGBLEND == 0 ) && ( $DOPIXELFOG == 0 )
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class solidenergy_vs30_Static_Index
{
private:
	int m_nDETAIL1;
#ifdef _DEBUG
	bool m_bDETAIL1;
#endif
public:
	void SetDETAIL1( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDETAIL1 = i;
#ifdef _DEBUG
		m_bDETAIL1 = true;
#endif
	}
	void SetDETAIL1( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDETAIL1 = i ? 1 : 0;
#ifdef _DEBUG
		m_bDETAIL1 = true;
#endif
	}
private:
	int m_nDETAIL2;
#ifdef _DEBUG
	bool m_bDETAIL2;
#endif
public:
	void SetDETAIL2( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDETAIL2 = i;
#ifdef _DEBUG
		m_bDETAIL2 = true;
#endif
	}
	void SetDETAIL2( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDETAIL2 = i ? 1 : 0;
#ifdef _DEBUG
		m_bDETAIL2 = true;
#endif
	}
private:
	int m_nTANGENTTOPACITY;
#ifdef _DEBUG
	bool m_bTANGENTTOPACITY;
#endif
public:
	void SetTANGENTTOPACITY( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nTANGENTTOPACITY = i;
#ifdef _DEBUG
		m_bTANGENTTOPACITY = true;
#endif
	}
	void SetTANGENTTOPACITY( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nTANGENTTOPACITY = i ? 1 : 0;
#ifdef _DEBUG
		m_bTANGENTTOPACITY = true;
#endif
	}
private:
	int m_nTANGENTSOPACITY;
#ifdef _DEBUG
	bool m_bTANGENTSOPACITY;
#endif
public:
	void SetTANGENTSOPACITY( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nTANGENTSOPACITY = i;
#ifdef _DEBUG
		m_bTANGENTSOPACITY = true;
#endif
	}
	void SetTANGENTSOPACITY( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nTANGENTSOPACITY = i ? 1 : 0;
#ifdef _DEBUG
		m_bTANGENTSOPACITY = true;
#endif
	}
private:
	int m_nFRESNELOPACITY;
#ifdef _DEBUG
	bool m_bFRESNELOPACITY;
#endif
public:
	void SetFRESNELOPACITY( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFRESNELOPACITY = i;
#ifdef _DEBUG
		m_bFRESNELOPACITY = true;
#endif
	}
	void SetFRESNELOPACITY( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nFRESNELOPACITY = i ? 1 : 0;
#ifdef _DEBUG
		m_bFRESNELOPACITY = true;
#endif
	}
private:
	int m_nVERTEXCOLOR;
#ifdef _DEBUG
	bool m_bVERTEXCOLOR;
#endif
public:
	void SetVERTEXCOLOR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nVERTEXCOLOR = i;
#ifdef _DEBUG
		m_bVERTEXCOLOR = true;
#endif
	}
	void SetVERTEXCOLOR( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nVERTEXCOLOR = i ? 1 : 0;
#ifdef _DEBUG
		m_bVERTEXCOLOR = true;
#endif
	}
private:
	int m_nFLOWMAP;
#ifdef _DEBUG
	bool m_bFLOWMAP;
#endif
public:
	void SetFLOWMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFLOWMAP = i;
#ifdef _DEBUG
		m_bFLOWMAP = true;
#endif
	}
	void SetFLOWMAP( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nFLOWMAP = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLOWMAP = true;
#endif
	}
private:
	int m_nMODELFORMAT;
#ifdef _DEBUG
	bool m_bMODELFORMAT;
#endif
public:
	void SetMODELFORMAT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMODELFORMAT = i;
#ifdef _DEBUG
		m_bMODELFORMAT = true;
#endif
	}
	void SetMODELFORMAT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nMODELFORMAT = i ? 1 : 0;
#ifdef _DEBUG
		m_bMODELFORMAT = true;
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
		Assert( i >= 0 && i <= 0 );
		m_nHARDWAREFOGBLEND = i;
#ifdef _DEBUG
		m_bHARDWAREFOGBLEND = true;
#endif
	}
	void SetHARDWAREFOGBLEND( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nHARDWAREFOGBLEND = i ? 1 : 0;
#ifdef _DEBUG
		m_bHARDWAREFOGBLEND = true;
#endif
	}
public:
	// CONSTRUCTOR
	solidenergy_vs30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bDETAIL1 = false;
#endif // _DEBUG
		m_nDETAIL1 = 0;
#ifdef _DEBUG
		m_bDETAIL2 = false;
#endif // _DEBUG
		m_nDETAIL2 = 0;
#ifdef _DEBUG
		m_bTANGENTTOPACITY = false;
#endif // _DEBUG
		m_nTANGENTTOPACITY = 0;
#ifdef _DEBUG
		m_bTANGENTSOPACITY = false;
#endif // _DEBUG
		m_nTANGENTSOPACITY = 0;
#ifdef _DEBUG
		m_bFRESNELOPACITY = false;
#endif // _DEBUG
		m_nFRESNELOPACITY = 0;
#ifdef _DEBUG
		m_bVERTEXCOLOR = false;
#endif // _DEBUG
		m_nVERTEXCOLOR = 0;
#ifdef _DEBUG
		m_bFLOWMAP = false;
#endif // _DEBUG
		m_nFLOWMAP = 0;
#ifdef _DEBUG
		m_bMODELFORMAT = false;
#endif // _DEBUG
		m_nMODELFORMAT = 0;
#ifdef _DEBUG
		m_bDOPIXELFOG = true;
#endif // _DEBUG
		m_nDOPIXELFOG = g_pHardwareConfig->SupportsPixelShaders_2_b() ;
#ifdef _DEBUG
		m_bHARDWAREFOGBLEND = true;
#endif // _DEBUG
		m_nHARDWAREFOGBLEND = 0 ;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bDETAIL1 && m_bDETAIL2 && m_bTANGENTTOPACITY && m_bTANGENTSOPACITY && m_bFRESNELOPACITY && m_bVERTEXCOLOR && m_bFLOWMAP && m_bMODELFORMAT && m_bDOPIXELFOG && m_bHARDWAREFOGBLEND;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 32 * m_nDETAIL1 ) + ( 64 * m_nDETAIL2 ) + ( 128 * m_nTANGENTTOPACITY ) + ( 256 * m_nTANGENTSOPACITY ) + ( 512 * m_nFRESNELOPACITY ) + ( 1024 * m_nVERTEXCOLOR ) + ( 2048 * m_nFLOWMAP ) + ( 4096 * m_nMODELFORMAT ) + ( 8192 * m_nDOPIXELFOG ) + ( 16384 * m_nHARDWAREFOGBLEND ) + 0;
	}
};
#define shaderStaticTest_solidenergy_vs30 vsh_forgot_to_set_static_DETAIL1 + vsh_forgot_to_set_static_DETAIL2 + vsh_forgot_to_set_static_TANGENTTOPACITY + vsh_forgot_to_set_static_TANGENTSOPACITY + vsh_forgot_to_set_static_FRESNELOPACITY + vsh_forgot_to_set_static_VERTEXCOLOR + vsh_forgot_to_set_static_FLOWMAP + vsh_forgot_to_set_static_MODELFORMAT + 0
class solidenergy_vs30_Dynamic_Index
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
	int m_nVORTEX1;
#ifdef _DEBUG
	bool m_bVORTEX1;
#endif
public:
	void SetVORTEX1( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nVORTEX1 = i;
#ifdef _DEBUG
		m_bVORTEX1 = true;
#endif
	}
	void SetVORTEX1( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nVORTEX1 = i ? 1 : 0;
#ifdef _DEBUG
		m_bVORTEX1 = true;
#endif
	}
private:
	int m_nVORTEX2;
#ifdef _DEBUG
	bool m_bVORTEX2;
#endif
public:
	void SetVORTEX2( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nVORTEX2 = i;
#ifdef _DEBUG
		m_bVORTEX2 = true;
#endif
	}
	void SetVORTEX2( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nVORTEX2 = i ? 1 : 0;
#ifdef _DEBUG
		m_bVORTEX2 = true;
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
	solidenergy_vs30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
		m_bVORTEX1 = false;
#endif // _DEBUG
		m_nVORTEX1 = 0;
#ifdef _DEBUG
		m_bVORTEX2 = false;
#endif // _DEBUG
		m_nVORTEX2 = 0;
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
		bool bAllDynamicVarsDefined = m_bCOMPRESSED_VERTS && m_bSKINNING && m_bVORTEX1 && m_bVORTEX2 && m_bDOWATERFOG;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nCOMPRESSED_VERTS ) + ( 2 * m_nSKINNING ) + ( 4 * m_nVORTEX1 ) + ( 8 * m_nVORTEX2 ) + ( 16 * m_nDOWATERFOG ) + 0;
	}
};
#define shaderDynamicTest_solidenergy_vs30 vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + vsh_forgot_to_set_dynamic_SKINNING + vsh_forgot_to_set_dynamic_VORTEX1 + vsh_forgot_to_set_dynamic_VORTEX2 + 0

static const ShaderComboInformation_t s_DynamicComboArray_solidenergy_vs30[5] = 
{
	{ "COMPRESSED_VERTS", 0, 1 },
	{ "SKINNING", 0, 1 },
	{ "VORTEX1", 0, 1 },
	{ "VORTEX2", 0, 1 },
	{ "DOWATERFOG", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_solidenergy_vs30[10] = 
{
	{ "DETAIL1", 0, 1 },
	{ "DETAIL2", 0, 1 },
	{ "TANGENTTOPACITY", 0, 1 },
	{ "TANGENTSOPACITY", 0, 1 },
	{ "FRESNELOPACITY", 0, 1 },
	{ "VERTEXCOLOR", 0, 1 },
	{ "FLOWMAP", 0, 1 },
	{ "MODELFORMAT", 0, 1 },
	{ "DOPIXELFOG", 0, 1 },
	{ "HARDWAREFOGBLEND", 0, 0 },
};
static const ShaderComboSemantics_t solidenergy_vs30_combos =
{
	"solidenergy_vs30", s_DynamicComboArray_solidenergy_vs30, 5, s_StaticComboArray_solidenergy_vs30, 10 
};

class ConstructMe_solidenergy_vs30
{
public:
	ConstructMe_solidenergy_vs30()
	{
		GetShaderDLL()->AddShaderComboInformation( &solidenergy_vs30_combos );
	}
};

static ConstructMe_solidenergy_vs30 s_ConstructMe_solidenergy_vs30;
