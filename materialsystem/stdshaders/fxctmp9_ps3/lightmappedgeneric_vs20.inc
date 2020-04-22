// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// $BUMPMASK && $SEAMLESS
// $LIGHTING_PREVIEW && $FLASHLIGHT
// ( $DOWATERFOG == 0 ) && ( $DOPIXELFOG && $HARDWAREFOGBLEND )
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class lightmappedgeneric_vs20_Static_Index
{
private:
	int m_nENVMAP_MASK;
#ifdef _DEBUG
	bool m_bENVMAP_MASK;
#endif
public:
	void SetENVMAP_MASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nENVMAP_MASK = i;
#ifdef _DEBUG
		m_bENVMAP_MASK = true;
#endif
	}
	void SetENVMAP_MASK( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nENVMAP_MASK = i ? 1 : 0;
#ifdef _DEBUG
		m_bENVMAP_MASK = true;
#endif
	}
private:
	int m_nTANGENTSPACE;
#ifdef _DEBUG
	bool m_bTANGENTSPACE;
#endif
public:
	void SetTANGENTSPACE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nTANGENTSPACE = i;
#ifdef _DEBUG
		m_bTANGENTSPACE = true;
#endif
	}
	void SetTANGENTSPACE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nTANGENTSPACE = i ? 1 : 0;
#ifdef _DEBUG
		m_bTANGENTSPACE = true;
#endif
	}
private:
	int m_nBUMPMAP;
#ifdef _DEBUG
	bool m_bBUMPMAP;
#endif
public:
	void SetBUMPMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBUMPMAP = i;
#ifdef _DEBUG
		m_bBUMPMAP = true;
#endif
	}
	void SetBUMPMAP( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nBUMPMAP = i ? 1 : 0;
#ifdef _DEBUG
		m_bBUMPMAP = true;
#endif
	}
private:
	int m_nDETAILTEXTURE;
#ifdef _DEBUG
	bool m_bDETAILTEXTURE;
#endif
public:
	void SetDETAILTEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDETAILTEXTURE = i;
#ifdef _DEBUG
		m_bDETAILTEXTURE = true;
#endif
	}
	void SetDETAILTEXTURE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDETAILTEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bDETAILTEXTURE = true;
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
	int m_nVERTEXALPHATEXBLENDFACTOR;
#ifdef _DEBUG
	bool m_bVERTEXALPHATEXBLENDFACTOR;
#endif
public:
	void SetVERTEXALPHATEXBLENDFACTOR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nVERTEXALPHATEXBLENDFACTOR = i;
#ifdef _DEBUG
		m_bVERTEXALPHATEXBLENDFACTOR = true;
#endif
	}
	void SetVERTEXALPHATEXBLENDFACTOR( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nVERTEXALPHATEXBLENDFACTOR = i ? 1 : 0;
#ifdef _DEBUG
		m_bVERTEXALPHATEXBLENDFACTOR = true;
#endif
	}
private:
	int m_nSEAMLESS;
#ifdef _DEBUG
	bool m_bSEAMLESS;
#endif
public:
	void SetSEAMLESS( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSEAMLESS = i;
#ifdef _DEBUG
		m_bSEAMLESS = true;
#endif
	}
	void SetSEAMLESS( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nSEAMLESS = i ? 1 : 0;
#ifdef _DEBUG
		m_bSEAMLESS = true;
#endif
	}
private:
	int m_nBUMPMASK;
#ifdef _DEBUG
	bool m_bBUMPMASK;
#endif
public:
	void SetBUMPMASK( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nBUMPMASK = i;
#ifdef _DEBUG
		m_bBUMPMASK = true;
#endif
	}
	void SetBUMPMASK( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nBUMPMASK = i ? 1 : 0;
#ifdef _DEBUG
		m_bBUMPMASK = true;
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
	int m_nFANCY_BLENDING;
#ifdef _DEBUG
	bool m_bFANCY_BLENDING;
#endif
public:
	void SetFANCY_BLENDING( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFANCY_BLENDING = i;
#ifdef _DEBUG
		m_bFANCY_BLENDING = true;
#endif
	}
	void SetFANCY_BLENDING( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nFANCY_BLENDING = i ? 1 : 0;
#ifdef _DEBUG
		m_bFANCY_BLENDING = true;
#endif
	}
private:
	int m_nSELFILLUM;
#ifdef _DEBUG
	bool m_bSELFILLUM;
#endif
public:
	void SetSELFILLUM( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSELFILLUM = i;
#ifdef _DEBUG
		m_bSELFILLUM = true;
#endif
	}
	void SetSELFILLUM( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nSELFILLUM = i ? 1 : 0;
#ifdef _DEBUG
		m_bSELFILLUM = true;
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
		Assert( i >= 0 && i <= 0 );
		m_nLIGHTING_PREVIEW = i;
#ifdef _DEBUG
		m_bLIGHTING_PREVIEW = true;
#endif
	}
	void SetLIGHTING_PREVIEW( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nLIGHTING_PREVIEW = i ? 1 : 0;
#ifdef _DEBUG
		m_bLIGHTING_PREVIEW = true;
#endif
	}
private:
	int m_nPAINT;
#ifdef _DEBUG
	bool m_bPAINT;
#endif
public:
	void SetPAINT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPAINT = i;
#ifdef _DEBUG
		m_bPAINT = true;
#endif
	}
	void SetPAINT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nPAINT = i ? 1 : 0;
#ifdef _DEBUG
		m_bPAINT = true;
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
	lightmappedgeneric_vs20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bENVMAP_MASK = false;
#endif // _DEBUG
		m_nENVMAP_MASK = 0;
#ifdef _DEBUG
		m_bTANGENTSPACE = false;
#endif // _DEBUG
		m_nTANGENTSPACE = 0;
#ifdef _DEBUG
		m_bBUMPMAP = false;
#endif // _DEBUG
		m_nBUMPMAP = 0;
#ifdef _DEBUG
		m_bDETAILTEXTURE = false;
#endif // _DEBUG
		m_nDETAILTEXTURE = 0;
#ifdef _DEBUG
		m_bVERTEXCOLOR = false;
#endif // _DEBUG
		m_nVERTEXCOLOR = 0;
#ifdef _DEBUG
		m_bVERTEXALPHATEXBLENDFACTOR = false;
#endif // _DEBUG
		m_nVERTEXALPHATEXBLENDFACTOR = 0;
#ifdef _DEBUG
		m_bSEAMLESS = false;
#endif // _DEBUG
		m_nSEAMLESS = 0;
#ifdef _DEBUG
		m_bBUMPMASK = false;
#endif // _DEBUG
		m_nBUMPMASK = 0;
#ifdef _DEBUG
		m_bFLASHLIGHT = false;
#endif // _DEBUG
		m_nFLASHLIGHT = 0;
#ifdef _DEBUG
		m_bFANCY_BLENDING = false;
#endif // _DEBUG
		m_nFANCY_BLENDING = 0;
#ifdef _DEBUG
		m_bSELFILLUM = false;
#endif // _DEBUG
		m_nSELFILLUM = 0;
#ifdef _DEBUG
		m_bLIGHTING_PREVIEW = false;
#endif // _DEBUG
		m_nLIGHTING_PREVIEW = 0;
#ifdef _DEBUG
		m_bPAINT = false;
#endif // _DEBUG
		m_nPAINT = 0;
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
		bool bAllStaticVarsDefined = m_bENVMAP_MASK && m_bTANGENTSPACE && m_bBUMPMAP && m_bDETAILTEXTURE && m_bVERTEXCOLOR && m_bVERTEXALPHATEXBLENDFACTOR && m_bSEAMLESS && m_bBUMPMASK && m_bFLASHLIGHT && m_bFANCY_BLENDING && m_bSELFILLUM && m_bLIGHTING_PREVIEW && m_bPAINT && m_bDOPIXELFOG;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 4 * m_nENVMAP_MASK ) + ( 8 * m_nTANGENTSPACE ) + ( 16 * m_nBUMPMAP ) + ( 32 * m_nDETAILTEXTURE ) + ( 64 * m_nVERTEXCOLOR ) + ( 128 * m_nVERTEXALPHATEXBLENDFACTOR ) + ( 256 * m_nSEAMLESS ) + ( 512 * m_nBUMPMASK ) + ( 512 * m_nFLASHLIGHT ) + ( 1024 * m_nFANCY_BLENDING ) + ( 2048 * m_nSELFILLUM ) + ( 4096 * m_nLIGHTING_PREVIEW ) + ( 4096 * m_nPAINT ) + ( 8192 * m_nDOPIXELFOG ) + 0;
	}
};
#define shaderStaticTest_lightmappedgeneric_vs20 vsh_forgot_to_set_static_ENVMAP_MASK + vsh_forgot_to_set_static_TANGENTSPACE + vsh_forgot_to_set_static_BUMPMAP + vsh_forgot_to_set_static_DETAILTEXTURE + vsh_forgot_to_set_static_VERTEXCOLOR + vsh_forgot_to_set_static_VERTEXALPHATEXBLENDFACTOR + vsh_forgot_to_set_static_SEAMLESS + vsh_forgot_to_set_static_BUMPMASK + vsh_forgot_to_set_static_FLASHLIGHT + vsh_forgot_to_set_static_FANCY_BLENDING + vsh_forgot_to_set_static_SELFILLUM + vsh_forgot_to_set_static_LIGHTING_PREVIEW + vsh_forgot_to_set_static_PAINT + 0
class lightmappedgeneric_vs20_Dynamic_Index
{
private:
	int m_nFASTPATH;
#ifdef _DEBUG
	bool m_bFASTPATH;
#endif
public:
	void SetFASTPATH( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFASTPATH = i;
#ifdef _DEBUG
		m_bFASTPATH = true;
#endif
	}
	void SetFASTPATH( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nFASTPATH = i ? 1 : 0;
#ifdef _DEBUG
		m_bFASTPATH = true;
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
	lightmappedgeneric_vs20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bFASTPATH = false;
#endif // _DEBUG
		m_nFASTPATH = 0;
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
		bool bAllDynamicVarsDefined = m_bFASTPATH && m_bDOWATERFOG;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nFASTPATH ) + ( 2 * m_nDOWATERFOG ) + 0;
	}
};
#define shaderDynamicTest_lightmappedgeneric_vs20 vsh_forgot_to_set_dynamic_FASTPATH + 0

static const ShaderComboInformation_t s_DynamicComboArray_lightmappedgeneric_vs20[2] = 
{
	{ "FASTPATH", 0, 1 },
	{ "DOWATERFOG", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_lightmappedgeneric_vs20[14] = 
{
	{ "ENVMAP_MASK", 0, 1 },
	{ "TANGENTSPACE", 0, 1 },
	{ "BUMPMAP", 0, 1 },
	{ "DETAILTEXTURE", 0, 1 },
	{ "VERTEXCOLOR", 0, 1 },
	{ "VERTEXALPHATEXBLENDFACTOR", 0, 1 },
	{ "SEAMLESS", 0, 1 },
	{ "BUMPMASK", 0, 0 },
	{ "FLASHLIGHT", 0, 1 },
	{ "FANCY_BLENDING", 0, 1 },
	{ "SELFILLUM", 0, 1 },
	{ "LIGHTING_PREVIEW", 0, 0 },
	{ "PAINT", 0, 1 },
	{ "DOPIXELFOG", 0, 1 },
};
static const ShaderComboSemantics_t lightmappedgeneric_vs20_combos =
{
	"lightmappedgeneric_vs20", s_DynamicComboArray_lightmappedgeneric_vs20, 2, s_StaticComboArray_lightmappedgeneric_vs20, 14 
};

class ConstructMe_lightmappedgeneric_vs20
{
public:
	ConstructMe_lightmappedgeneric_vs20()
	{
		GetShaderDLL()->AddShaderComboInformation( &lightmappedgeneric_vs20_combos );
	}
};

static ConstructMe_lightmappedgeneric_vs20 s_ConstructMe_lightmappedgeneric_vs20;
