// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ($SEPARATE_DETAIL_UVS) && ($SEAMLESS_DETAIL)
// ($TREESWAY) && ($MORPHING)
// ($TREESWAY) && ($TESSELLATION)
// ( $TREESWAY ) && ( $SEAMLESS_DETAIL || $SEAMLESS_BASE )
// ( $SFM == 0 ) && ( $MORPHING )
// ( $SFM == 0 ) && ( $TESSELLATION )
// ( $FLATTEN_STATIC_CONTROL_FLOW == 1 ) && ( $HARDWAREFOGBLEND )
// ( $DOWATERFOG == 0 ) && ( $DOPIXELFOG && $HARDWAREFOGBLEND )
// ( $STATICLIGHT3 || $VERTEXCOLOR || $SEAMLESS_BASE || $SEAMLESS_DETAIL || $SEPARATE_DETAIL_UVS || $MORPHING || $SKINNING || $COMPRESSED_VERTS ) && $TESSELLATION
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class vertexlit_and_unlit_generic_vs30_Static_Index
{
private:
	int m_nSFM;
#ifdef _DEBUG
	bool m_bSFM;
#endif
public:
	void SetSFM( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSFM = i;
#ifdef _DEBUG
		m_bSFM = true;
#endif
	}
	void SetSFM( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nSFM = i ? 1 : 0;
#ifdef _DEBUG
		m_bSFM = true;
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
	int m_nCUBEMAP;
#ifdef _DEBUG
	bool m_bCUBEMAP;
#endif
public:
	void SetCUBEMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCUBEMAP = i;
#ifdef _DEBUG
		m_bCUBEMAP = true;
#endif
	}
	void SetCUBEMAP( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nCUBEMAP = i ? 1 : 0;
#ifdef _DEBUG
		m_bCUBEMAP = true;
#endif
	}
private:
	int m_nHALFLAMBERT;
#ifdef _DEBUG
	bool m_bHALFLAMBERT;
#endif
public:
	void SetHALFLAMBERT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nHALFLAMBERT = i;
#ifdef _DEBUG
		m_bHALFLAMBERT = true;
#endif
	}
	void SetHALFLAMBERT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nHALFLAMBERT = i ? 1 : 0;
#ifdef _DEBUG
		m_bHALFLAMBERT = true;
#endif
	}
private:
	int m_nFLASHLIGHT;
#ifdef _DEBUG
	bool m_bFLASHLIGHT;
#endif
public:
	void SetFLASHLIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFLASHLIGHT = i;
#ifdef _DEBUG
		m_bFLASHLIGHT = true;
#endif
	}
	void SetFLASHLIGHT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nFLASHLIGHT = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLASHLIGHT = true;
#endif
	}
private:
	int m_nSEPARATE_DETAIL_UVS;
#ifdef _DEBUG
	bool m_bSEPARATE_DETAIL_UVS;
#endif
public:
	void SetSEPARATE_DETAIL_UVS( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nSEPARATE_DETAIL_UVS = i;
#ifdef _DEBUG
		m_bSEPARATE_DETAIL_UVS = true;
#endif
	}
	void SetSEPARATE_DETAIL_UVS( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nSEPARATE_DETAIL_UVS = i ? 1 : 0;
#ifdef _DEBUG
		m_bSEPARATE_DETAIL_UVS = true;
#endif
	}
private:
	int m_nSEAMLESS_BASE;
#ifdef _DEBUG
	bool m_bSEAMLESS_BASE;
#endif
public:
	void SetSEAMLESS_BASE( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nSEAMLESS_BASE = i;
#ifdef _DEBUG
		m_bSEAMLESS_BASE = true;
#endif
	}
	void SetSEAMLESS_BASE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nSEAMLESS_BASE = i ? 1 : 0;
#ifdef _DEBUG
		m_bSEAMLESS_BASE = true;
#endif
	}
private:
	int m_nSEAMLESS_DETAIL;
#ifdef _DEBUG
	bool m_bSEAMLESS_DETAIL;
#endif
public:
	void SetSEAMLESS_DETAIL( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nSEAMLESS_DETAIL = i;
#ifdef _DEBUG
		m_bSEAMLESS_DETAIL = true;
#endif
	}
	void SetSEAMLESS_DETAIL( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nSEAMLESS_DETAIL = i ? 1 : 0;
#ifdef _DEBUG
		m_bSEAMLESS_DETAIL = true;
#endif
	}
private:
	int m_nDECAL;
#ifdef _DEBUG
	bool m_bDECAL;
#endif
public:
	void SetDECAL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDECAL = i;
#ifdef _DEBUG
		m_bDECAL = true;
#endif
	}
	void SetDECAL( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDECAL = i ? 1 : 0;
#ifdef _DEBUG
		m_bDECAL = true;
#endif
	}
private:
	int m_nLIGHTING_PREVIEW;
#ifdef _DEBUG
	bool m_bLIGHTING_PREVIEW;
#endif
public:
	void SetLIGHTING_PREVIEW( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nLIGHTING_PREVIEW = i;
#ifdef _DEBUG
		m_bLIGHTING_PREVIEW = true;
#endif
	}
	void SetLIGHTING_PREVIEW( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nLIGHTING_PREVIEW = i ? 1 : 0;
#ifdef _DEBUG
		m_bLIGHTING_PREVIEW = true;
#endif
	}
private:
	int m_nTREESWAY;
#ifdef _DEBUG
	bool m_bTREESWAY;
#endif
public:
	void SetTREESWAY( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nTREESWAY = i;
#ifdef _DEBUG
		m_bTREESWAY = true;
#endif
	}
	void SetTREESWAY( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 2 );
		m_nTREESWAY = i ? 1 : 0;
#ifdef _DEBUG
		m_bTREESWAY = true;
#endif
	}
private:
	int m_nCASCADED_SHADOW_MAPPING;
#ifdef _DEBUG
	bool m_bCASCADED_SHADOW_MAPPING;
#endif
public:
	void SetCASCADED_SHADOW_MAPPING( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nCASCADED_SHADOW_MAPPING = i;
#ifdef _DEBUG
		m_bCASCADED_SHADOW_MAPPING = true;
#endif
	}
	void SetCASCADED_SHADOW_MAPPING( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nCASCADED_SHADOW_MAPPING = i ? 1 : 0;
#ifdef _DEBUG
		m_bCASCADED_SHADOW_MAPPING = true;
#endif
	}
private:
	int m_nCSM_BLENDING;
#ifdef _DEBUG
	bool m_bCSM_BLENDING;
#endif
public:
	void SetCSM_BLENDING( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCSM_BLENDING = i;
#ifdef _DEBUG
		m_bCSM_BLENDING = true;
#endif
	}
	void SetCSM_BLENDING( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nCSM_BLENDING = i ? 1 : 0;
#ifdef _DEBUG
		m_bCSM_BLENDING = true;
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
	vertexlit_and_unlit_generic_vs30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bSFM = false;
#endif // _DEBUG
		m_nSFM = 0;
#ifdef _DEBUG
		m_bVERTEXCOLOR = false;
#endif // _DEBUG
		m_nVERTEXCOLOR = 0;
#ifdef _DEBUG
		m_bCUBEMAP = false;
#endif // _DEBUG
		m_nCUBEMAP = 0;
#ifdef _DEBUG
		m_bHALFLAMBERT = false;
#endif // _DEBUG
		m_nHALFLAMBERT = 0;
#ifdef _DEBUG
		m_bFLASHLIGHT = false;
#endif // _DEBUG
		m_nFLASHLIGHT = 0;
#ifdef _DEBUG
		m_bSEPARATE_DETAIL_UVS = false;
#endif // _DEBUG
		m_nSEPARATE_DETAIL_UVS = 0;
#ifdef _DEBUG
		m_bSEAMLESS_BASE = false;
#endif // _DEBUG
		m_nSEAMLESS_BASE = 0;
#ifdef _DEBUG
		m_bSEAMLESS_DETAIL = false;
#endif // _DEBUG
		m_nSEAMLESS_DETAIL = 0;
#ifdef _DEBUG
		m_bDECAL = false;
#endif // _DEBUG
		m_nDECAL = 0;
#ifdef _DEBUG
		m_bLIGHTING_PREVIEW = false;
#endif // _DEBUG
		m_nLIGHTING_PREVIEW = 0;
#ifdef _DEBUG
		m_bTREESWAY = false;
#endif // _DEBUG
		m_nTREESWAY = 0;
#ifdef _DEBUG
		m_bCASCADED_SHADOW_MAPPING = false;
#endif // _DEBUG
		m_nCASCADED_SHADOW_MAPPING = 0;
#ifdef _DEBUG
		m_bCSM_BLENDING = false;
#endif // _DEBUG
		m_nCSM_BLENDING = 0;
#ifdef _DEBUG
		m_bDOPIXELFOG = true;
#endif // _DEBUG
		m_nDOPIXELFOG = ( g_pHardwareConfig->SupportsPixelShaders_2_b() && !IS_FLAG_SET( MATERIAL_VAR_VERTEXFOG ) ) ? 1 : 0 ;
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
		bool bAllStaticVarsDefined = m_bSFM && m_bVERTEXCOLOR && m_bCUBEMAP && m_bHALFLAMBERT && m_bFLASHLIGHT && m_bSEPARATE_DETAIL_UVS && m_bSEAMLESS_BASE && m_bSEAMLESS_DETAIL && m_bDECAL && m_bLIGHTING_PREVIEW && m_bTREESWAY && m_bCASCADED_SHADOW_MAPPING && m_bCSM_BLENDING && m_bDOPIXELFOG && m_bHARDWAREFOGBLEND;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 48 * m_nSFM ) + ( 96 * m_nVERTEXCOLOR ) + ( 192 * m_nCUBEMAP ) + ( 384 * m_nHALFLAMBERT ) + ( 768 * m_nFLASHLIGHT ) + ( 1536 * m_nSEPARATE_DETAIL_UVS ) + ( 1536 * m_nSEAMLESS_BASE ) + ( 1536 * m_nSEAMLESS_DETAIL ) + ( 1536 * m_nDECAL ) + ( 3072 * m_nLIGHTING_PREVIEW ) + ( 6144 * m_nTREESWAY ) + ( 18432 * m_nCASCADED_SHADOW_MAPPING ) + ( 18432 * m_nCSM_BLENDING ) + ( 36864 * m_nDOPIXELFOG ) + ( 73728 * m_nHARDWAREFOGBLEND ) + 0;
	}
};
#define shaderStaticTest_vertexlit_and_unlit_generic_vs30 vsh_forgot_to_set_static_SFM + vsh_forgot_to_set_static_VERTEXCOLOR + vsh_forgot_to_set_static_CUBEMAP + vsh_forgot_to_set_static_HALFLAMBERT + vsh_forgot_to_set_static_FLASHLIGHT + vsh_forgot_to_set_static_SEPARATE_DETAIL_UVS + vsh_forgot_to_set_static_SEAMLESS_BASE + vsh_forgot_to_set_static_SEAMLESS_DETAIL + vsh_forgot_to_set_static_DECAL + vsh_forgot_to_set_static_LIGHTING_PREVIEW + vsh_forgot_to_set_static_TREESWAY + vsh_forgot_to_set_static_CASCADED_SHADOW_MAPPING + vsh_forgot_to_set_static_CSM_BLENDING + 0
class vertexlit_and_unlit_generic_vs30_Dynamic_Index
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
	int m_nDYNAMIC_LIGHT;
#ifdef _DEBUG
	bool m_bDYNAMIC_LIGHT;
#endif
public:
	void SetDYNAMIC_LIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDYNAMIC_LIGHT = i;
#ifdef _DEBUG
		m_bDYNAMIC_LIGHT = true;
#endif
	}
	void SetDYNAMIC_LIGHT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDYNAMIC_LIGHT = i ? 1 : 0;
#ifdef _DEBUG
		m_bDYNAMIC_LIGHT = true;
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
	int m_nSTATICLIGHT3;
#ifdef _DEBUG
	bool m_bSTATICLIGHT3;
#endif
public:
	void SetSTATICLIGHT3( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nSTATICLIGHT3 = i;
#ifdef _DEBUG
		m_bSTATICLIGHT3 = true;
#endif
	}
	void SetSTATICLIGHT3( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 2 );
		m_nSTATICLIGHT3 = i ? 1 : 0;
#ifdef _DEBUG
		m_bSTATICLIGHT3 = true;
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
	vertexlit_and_unlit_generic_vs30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bCOMPRESSED_VERTS = false;
#endif // _DEBUG
		m_nCOMPRESSED_VERTS = 0;
#ifdef _DEBUG
		m_bDYNAMIC_LIGHT = false;
#endif // _DEBUG
		m_nDYNAMIC_LIGHT = 0;
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
		m_bSTATICLIGHT3 = false;
#endif // _DEBUG
		m_nSTATICLIGHT3 = 0;
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
		bool bAllDynamicVarsDefined = m_bCOMPRESSED_VERTS && m_bDYNAMIC_LIGHT && m_bSKINNING && m_bMORPHING && m_bTESSELLATION && m_bSTATICLIGHT3 && m_bDOWATERFOG;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nCOMPRESSED_VERTS ) + ( 2 * m_nDYNAMIC_LIGHT ) + ( 4 * m_nSKINNING ) + ( 8 * m_nMORPHING ) + ( 8 * m_nTESSELLATION ) + ( 8 * m_nSTATICLIGHT3 ) + ( 24 * m_nDOWATERFOG ) + 0;
	}
};
#define shaderDynamicTest_vertexlit_and_unlit_generic_vs30 vsh_forgot_to_set_dynamic_COMPRESSED_VERTS + vsh_forgot_to_set_dynamic_DYNAMIC_LIGHT + vsh_forgot_to_set_dynamic_SKINNING + vsh_forgot_to_set_dynamic_TESSELLATION + vsh_forgot_to_set_dynamic_STATICLIGHT3 + 0

static const ShaderComboInformation_t s_DynamicComboArray_vertexlit_and_unlit_generic_vs30[7] = 
{
	{ "COMPRESSED_VERTS", 0, 1 },
	{ "DYNAMIC_LIGHT", 0, 1 },
	{ "SKINNING", 0, 1 },
	{ "MORPHING", 0, 0 },
	{ "TESSELLATION", 0, 0 },
	{ "STATICLIGHT3", 0, 2 },
	{ "DOWATERFOG", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_vertexlit_and_unlit_generic_vs30[15] = 
{
	{ "SFM", 0, 1 },
	{ "VERTEXCOLOR", 0, 1 },
	{ "CUBEMAP", 0, 1 },
	{ "HALFLAMBERT", 0, 1 },
	{ "FLASHLIGHT", 0, 1 },
	{ "SEPARATE_DETAIL_UVS", 0, 0 },
	{ "SEAMLESS_BASE", 0, 0 },
	{ "SEAMLESS_DETAIL", 0, 0 },
	{ "DECAL", 0, 1 },
	{ "LIGHTING_PREVIEW", 0, 1 },
	{ "TREESWAY", 0, 2 },
	{ "CASCADED_SHADOW_MAPPING", 0, 0 },
	{ "CSM_BLENDING", 0, 1 },
	{ "DOPIXELFOG", 0, 1 },
	{ "HARDWAREFOGBLEND", 0, 0 },
};
static const ShaderComboSemantics_t vertexlit_and_unlit_generic_vs30_combos =
{
	"vertexlit_and_unlit_generic_vs30", s_DynamicComboArray_vertexlit_and_unlit_generic_vs30, 7, s_StaticComboArray_vertexlit_and_unlit_generic_vs30, 15 
};

class ConstructMe_vertexlit_and_unlit_generic_vs30
{
public:
	ConstructMe_vertexlit_and_unlit_generic_vs30()
	{
		GetShaderDLL()->AddShaderComboInformation( &vertexlit_and_unlit_generic_vs30_combos );
	}
};

static ConstructMe_vertexlit_and_unlit_generic_vs30 s_ConstructMe_vertexlit_and_unlit_generic_vs30;
