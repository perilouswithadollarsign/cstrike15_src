// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ($DETAIL_BLEND_MODE == 1 )
// ($DETAIL_BLEND_MODE == 2 )
// ($DETAIL_BLEND_MODE == 3 )
// ($DETAIL_BLEND_MODE == 4 )
// ($DETAIL_BLEND_MODE == 5 )
// ($DETAIL_BLEND_MODE == 6 )
// ($DETAIL_BLEND_MODE == 8 )
// ($DETAIL_BLEND_MODE == 9 )
// ( $FLASHLIGHT == 0 ) && ( $FLASHLIGHTSHADOWS == 1 )
// ( $CASCADED_SHADOW_MAPPING == 0 ) && ( $CASCADE_SIZE != 0 ) 
// ( $CASCADED_SHADOW_MAPPING != 0 ) && ( $SFM != 0 ) 
// ( $CASCADED_SHADOW_MAPPING == 0 ) && ( $CSM_MODE != 0 ) 
// $ENVMAPMASK && $BUMPMAP
// $BASEALPHAENVMAPMASK && $ENVMAPMASK
// $BASEALPHAENVMAPMASK && $SELFILLUM
// !$FASTPATH && $FASTPATHENVMAPCONTRAST
// !$FASTPATH && $FASTPATHENVMAPTINT
// !$BUMPMAP && $BUMPMAP2
// $ENVMAPMASK && $BUMPMAP2
// $SEAMLESS && ( $DETAIL_BLEND_MODE != 12 )
// $ENVMAPANISOTROPY && !$ENVMAP && ( $BUMPMAP != 1 )
// $ENVMAPANISOTROPY && $NORMALMAPALPHAENVMAPMASK
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class lightmapped_4wayblend_ps30_Static_Index
{
private:
	int m_nBUMPMAP;
#ifdef _DEBUG
	bool m_bBUMPMAP;
#endif
public:
	void SetBUMPMAP( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nBUMPMAP = i;
#ifdef _DEBUG
		m_bBUMPMAP = true;
#endif
	}
	void SetBUMPMAP( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 2 );
		m_nBUMPMAP = i ? 1 : 0;
#ifdef _DEBUG
		m_bBUMPMAP = true;
#endif
	}
private:
	int m_nBUMPMAP2;
#ifdef _DEBUG
	bool m_bBUMPMAP2;
#endif
public:
	void SetBUMPMAP2( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBUMPMAP2 = i;
#ifdef _DEBUG
		m_bBUMPMAP2 = true;
#endif
	}
	void SetBUMPMAP2( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nBUMPMAP2 = i ? 1 : 0;
#ifdef _DEBUG
		m_bBUMPMAP2 = true;
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
		Assert( i >= 0 && i <= 2 );
		m_nCUBEMAP = i;
#ifdef _DEBUG
		m_bCUBEMAP = true;
#endif
	}
	void SetCUBEMAP( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 2 );
		m_nCUBEMAP = i ? 1 : 0;
#ifdef _DEBUG
		m_bCUBEMAP = true;
#endif
	}
private:
	int m_nENVMAPMASK;
#ifdef _DEBUG
	bool m_bENVMAPMASK;
#endif
public:
	void SetENVMAPMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nENVMAPMASK = i;
#ifdef _DEBUG
		m_bENVMAPMASK = true;
#endif
	}
	void SetENVMAPMASK( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nENVMAPMASK = i ? 1 : 0;
#ifdef _DEBUG
		m_bENVMAPMASK = true;
#endif
	}
private:
	int m_nBASEALPHAENVMAPMASK;
#ifdef _DEBUG
	bool m_bBASEALPHAENVMAPMASK;
#endif
public:
	void SetBASEALPHAENVMAPMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBASEALPHAENVMAPMASK = i;
#ifdef _DEBUG
		m_bBASEALPHAENVMAPMASK = true;
#endif
	}
	void SetBASEALPHAENVMAPMASK( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nBASEALPHAENVMAPMASK = i ? 1 : 0;
#ifdef _DEBUG
		m_bBASEALPHAENVMAPMASK = true;
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
	int m_nENVMAPANISOTROPY;
#ifdef _DEBUG
	bool m_bENVMAPANISOTROPY;
#endif
public:
	void SetENVMAPANISOTROPY( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nENVMAPANISOTROPY = i;
#ifdef _DEBUG
		m_bENVMAPANISOTROPY = true;
#endif
	}
	void SetENVMAPANISOTROPY( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nENVMAPANISOTROPY = i ? 1 : 0;
#ifdef _DEBUG
		m_bENVMAPANISOTROPY = true;
#endif
	}
private:
	int m_nTEXTURE3_BLENDMODE;
#ifdef _DEBUG
	bool m_bTEXTURE3_BLENDMODE;
#endif
public:
	void SetTEXTURE3_BLENDMODE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nTEXTURE3_BLENDMODE = i;
#ifdef _DEBUG
		m_bTEXTURE3_BLENDMODE = true;
#endif
	}
	void SetTEXTURE3_BLENDMODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nTEXTURE3_BLENDMODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bTEXTURE3_BLENDMODE = true;
#endif
	}
private:
	int m_nTEXTURE4_BLENDMODE;
#ifdef _DEBUG
	bool m_bTEXTURE4_BLENDMODE;
#endif
public:
	void SetTEXTURE4_BLENDMODE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nTEXTURE4_BLENDMODE = i;
#ifdef _DEBUG
		m_bTEXTURE4_BLENDMODE = true;
#endif
	}
	void SetTEXTURE4_BLENDMODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nTEXTURE4_BLENDMODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bTEXTURE4_BLENDMODE = true;
#endif
	}
private:
	int m_nDETAIL_BLEND_MODE;
#ifdef _DEBUG
	bool m_bDETAIL_BLEND_MODE;
#endif
public:
	void SetDETAIL_BLEND_MODE( int i )
	{
		Assert( i >= 0 && i <= 12 );
		m_nDETAIL_BLEND_MODE = i;
#ifdef _DEBUG
		m_bDETAIL_BLEND_MODE = true;
#endif
	}
	void SetDETAIL_BLEND_MODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 12 );
		m_nDETAIL_BLEND_MODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bDETAIL_BLEND_MODE = true;
#endif
	}
private:
	int m_nSHADER_SRGB_READ;
#ifdef _DEBUG
	bool m_bSHADER_SRGB_READ;
#endif
public:
	void SetSHADER_SRGB_READ( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nSHADER_SRGB_READ = i;
#ifdef _DEBUG
		m_bSHADER_SRGB_READ = true;
#endif
	}
	void SetSHADER_SRGB_READ( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nSHADER_SRGB_READ = i ? 1 : 0;
#ifdef _DEBUG
		m_bSHADER_SRGB_READ = true;
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
		Assert( i >= 0 && i <= 3 );
		m_nLIGHTING_PREVIEW = i;
#ifdef _DEBUG
		m_bLIGHTING_PREVIEW = true;
#endif
	}
	void SetLIGHTING_PREVIEW( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 3 );
		m_nLIGHTING_PREVIEW = i ? 1 : 0;
#ifdef _DEBUG
		m_bLIGHTING_PREVIEW = true;
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
		Assert( i >= 0 && i <= 1 );
		m_nCASCADED_SHADOW_MAPPING = i;
#ifdef _DEBUG
		m_bCASCADED_SHADOW_MAPPING = true;
#endif
	}
	void SetCASCADED_SHADOW_MAPPING( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nCASCADED_SHADOW_MAPPING = i ? 1 : 0;
#ifdef _DEBUG
		m_bCASCADED_SHADOW_MAPPING = true;
#endif
	}
private:
	int m_nCSM_MODE;
#ifdef _DEBUG
	bool m_bCSM_MODE;
#endif
public:
	void SetCSM_MODE( int i )
	{
		Assert( i >= 0 && i <= 3 );
		m_nCSM_MODE = i;
#ifdef _DEBUG
		m_bCSM_MODE = true;
#endif
	}
	void SetCSM_MODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 3 );
		m_nCSM_MODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bCSM_MODE = true;
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
public:
	// CONSTRUCTOR
	lightmapped_4wayblend_ps30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bBUMPMAP = false;
#endif // _DEBUG
		m_nBUMPMAP = 0;
#ifdef _DEBUG
		m_bBUMPMAP2 = false;
#endif // _DEBUG
		m_nBUMPMAP2 = 0;
#ifdef _DEBUG
		m_bCUBEMAP = false;
#endif // _DEBUG
		m_nCUBEMAP = 0;
#ifdef _DEBUG
		m_bENVMAPMASK = false;
#endif // _DEBUG
		m_nENVMAPMASK = 0;
#ifdef _DEBUG
		m_bBASEALPHAENVMAPMASK = false;
#endif // _DEBUG
		m_nBASEALPHAENVMAPMASK = 0;
#ifdef _DEBUG
		m_bSELFILLUM = false;
#endif // _DEBUG
		m_nSELFILLUM = 0;
#ifdef _DEBUG
		m_bSEAMLESS = false;
#endif // _DEBUG
		m_nSEAMLESS = 0;
#ifdef _DEBUG
		m_bENVMAPANISOTROPY = false;
#endif // _DEBUG
		m_nENVMAPANISOTROPY = 0;
#ifdef _DEBUG
		m_bTEXTURE3_BLENDMODE = false;
#endif // _DEBUG
		m_nTEXTURE3_BLENDMODE = 0;
#ifdef _DEBUG
		m_bTEXTURE4_BLENDMODE = false;
#endif // _DEBUG
		m_nTEXTURE4_BLENDMODE = 0;
#ifdef _DEBUG
		m_bDETAIL_BLEND_MODE = false;
#endif // _DEBUG
		m_nDETAIL_BLEND_MODE = 0;
#ifdef _DEBUG
		m_bSHADER_SRGB_READ = false;
#endif // _DEBUG
		m_nSHADER_SRGB_READ = 0;
#ifdef _DEBUG
		m_bLIGHTING_PREVIEW = false;
#endif // _DEBUG
		m_nLIGHTING_PREVIEW = 0;
#ifdef _DEBUG
		m_bCASCADED_SHADOW_MAPPING = true;
#endif // _DEBUG
		m_nCASCADED_SHADOW_MAPPING = g_pHardwareConfig->SupportsCascadedShadowMapping() && !ToolsEnabled() ;
#ifdef _DEBUG
		m_bCSM_MODE = false;
#endif // _DEBUG
		m_nCSM_MODE = 0;
#ifdef _DEBUG
		m_bCSM_BLENDING = false;
#endif // _DEBUG
		m_nCSM_BLENDING = 0;
#ifdef _DEBUG
		m_bDOPIXELFOG = true;
#endif // _DEBUG
		m_nDOPIXELFOG = !IS_FLAG_SET( MATERIAL_VAR_VERTEXFOG ) ? 1 : 0 ;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bBUMPMAP && m_bBUMPMAP2 && m_bCUBEMAP && m_bENVMAPMASK && m_bBASEALPHAENVMAPMASK && m_bSELFILLUM && m_bSEAMLESS && m_bENVMAPANISOTROPY && m_bTEXTURE3_BLENDMODE && m_bTEXTURE4_BLENDMODE && m_bDETAIL_BLEND_MODE && m_bSHADER_SRGB_READ && m_bLIGHTING_PREVIEW && m_bCASCADED_SHADOW_MAPPING && m_bCSM_MODE && m_bCSM_BLENDING && m_bDOPIXELFOG;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 32 * m_nBUMPMAP ) + ( 96 * m_nBUMPMAP2 ) + ( 192 * m_nCUBEMAP ) + ( 576 * m_nENVMAPMASK ) + ( 1152 * m_nBASEALPHAENVMAPMASK ) + ( 2304 * m_nSELFILLUM ) + ( 4608 * m_nSEAMLESS ) + ( 9216 * m_nENVMAPANISOTROPY ) + ( 18432 * m_nTEXTURE3_BLENDMODE ) + ( 36864 * m_nTEXTURE4_BLENDMODE ) + ( 73728 * m_nDETAIL_BLEND_MODE ) + ( 958464 * m_nSHADER_SRGB_READ ) + ( 958464 * m_nLIGHTING_PREVIEW ) + ( 3833856 * m_nCASCADED_SHADOW_MAPPING ) + ( 7667712 * m_nCSM_MODE ) + ( 30670848 * m_nCSM_BLENDING ) + ( 61341696 * m_nDOPIXELFOG ) + 0;
	}
};
#define shaderStaticTest_lightmapped_4wayblend_ps30 psh_forgot_to_set_static_BUMPMAP + psh_forgot_to_set_static_BUMPMAP2 + psh_forgot_to_set_static_CUBEMAP + psh_forgot_to_set_static_ENVMAPMASK + psh_forgot_to_set_static_BASEALPHAENVMAPMASK + psh_forgot_to_set_static_SELFILLUM + psh_forgot_to_set_static_SEAMLESS + psh_forgot_to_set_static_ENVMAPANISOTROPY + psh_forgot_to_set_static_TEXTURE3_BLENDMODE + psh_forgot_to_set_static_TEXTURE4_BLENDMODE + psh_forgot_to_set_static_DETAIL_BLEND_MODE + psh_forgot_to_set_static_SHADER_SRGB_READ + psh_forgot_to_set_static_LIGHTING_PREVIEW + psh_forgot_to_set_static_CSM_MODE + psh_forgot_to_set_static_CSM_BLENDING + 0
class lightmapped_4wayblend_ps30_Dynamic_Index
{
private:
	int m_nFASTPATHENVMAPCONTRAST;
#ifdef _DEBUG
	bool m_bFASTPATHENVMAPCONTRAST;
#endif
public:
	void SetFASTPATHENVMAPCONTRAST( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFASTPATHENVMAPCONTRAST = i;
#ifdef _DEBUG
		m_bFASTPATHENVMAPCONTRAST = true;
#endif
	}
	void SetFASTPATHENVMAPCONTRAST( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nFASTPATHENVMAPCONTRAST = i ? 1 : 0;
#ifdef _DEBUG
		m_bFASTPATHENVMAPCONTRAST = true;
#endif
	}
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
	int m_nWRITEWATERFOGTODESTALPHA;
#ifdef _DEBUG
	bool m_bWRITEWATERFOGTODESTALPHA;
#endif
public:
	void SetWRITEWATERFOGTODESTALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nWRITEWATERFOGTODESTALPHA = i;
#ifdef _DEBUG
		m_bWRITEWATERFOGTODESTALPHA = true;
#endif
	}
	void SetWRITEWATERFOGTODESTALPHA( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nWRITEWATERFOGTODESTALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bWRITEWATERFOGTODESTALPHA = true;
#endif
	}
private:
	int m_nWRITE_DEPTH_TO_DESTALPHA;
#ifdef _DEBUG
	bool m_bWRITE_DEPTH_TO_DESTALPHA;
#endif
public:
	void SetWRITE_DEPTH_TO_DESTALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nWRITE_DEPTH_TO_DESTALPHA = i;
#ifdef _DEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = true;
#endif
	}
	void SetWRITE_DEPTH_TO_DESTALPHA( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nWRITE_DEPTH_TO_DESTALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = true;
#endif
	}
private:
	int m_nFLASHLIGHTSHADOWS;
#ifdef _DEBUG
	bool m_bFLASHLIGHTSHADOWS;
#endif
public:
	void SetFLASHLIGHTSHADOWS( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nFLASHLIGHTSHADOWS = i;
#ifdef _DEBUG
		m_bFLASHLIGHTSHADOWS = true;
#endif
	}
	void SetFLASHLIGHTSHADOWS( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nFLASHLIGHTSHADOWS = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLASHLIGHTSHADOWS = true;
#endif
	}
private:
	int m_nCASCADE_SIZE;
#ifdef _DEBUG
	bool m_bCASCADE_SIZE;
#endif
public:
	void SetCASCADE_SIZE( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nCASCADE_SIZE = i;
#ifdef _DEBUG
		m_bCASCADE_SIZE = true;
#endif
	}
	void SetCASCADE_SIZE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nCASCADE_SIZE = i ? 1 : 0;
#ifdef _DEBUG
		m_bCASCADE_SIZE = true;
#endif
	}
private:
	int m_nPIXELFOGTYPE;
#ifdef _DEBUG
	bool m_bPIXELFOGTYPE;
#endif
public:
	void SetPIXELFOGTYPE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPIXELFOGTYPE = i;
#ifdef _DEBUG
		m_bPIXELFOGTYPE = true;
#endif
	}
	void SetPIXELFOGTYPE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nPIXELFOGTYPE = i ? 1 : 0;
#ifdef _DEBUG
		m_bPIXELFOGTYPE = true;
#endif
	}
public:
	// CONSTRUCTOR
	lightmapped_4wayblend_ps30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bFASTPATHENVMAPCONTRAST = false;
#endif // _DEBUG
		m_nFASTPATHENVMAPCONTRAST = 0;
#ifdef _DEBUG
		m_bFASTPATH = false;
#endif // _DEBUG
		m_nFASTPATH = 0;
#ifdef _DEBUG
		m_bWRITEWATERFOGTODESTALPHA = false;
#endif // _DEBUG
		m_nWRITEWATERFOGTODESTALPHA = 0;
#ifdef _DEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = false;
#endif // _DEBUG
		m_nWRITE_DEPTH_TO_DESTALPHA = 0;
#ifdef _DEBUG
		m_bFLASHLIGHTSHADOWS = false;
#endif // _DEBUG
		m_nFLASHLIGHTSHADOWS = 0;
#ifdef _DEBUG
		m_bCASCADE_SIZE = true;
#endif // _DEBUG
		m_nCASCADE_SIZE = 0 ;
#ifdef _DEBUG
		m_bPIXELFOGTYPE = true;
#endif // _DEBUG
		m_nPIXELFOGTYPE = ( pShaderAPI->GetSceneFogMode() == MATERIAL_FOG_LINEAR_BELOW_FOG_Z ) ;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bFASTPATHENVMAPCONTRAST && m_bFASTPATH && m_bWRITEWATERFOGTODESTALPHA && m_bWRITE_DEPTH_TO_DESTALPHA && m_bFLASHLIGHTSHADOWS && m_bCASCADE_SIZE && m_bPIXELFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nFASTPATHENVMAPCONTRAST ) + ( 2 * m_nFASTPATH ) + ( 4 * m_nWRITEWATERFOGTODESTALPHA ) + ( 8 * m_nWRITE_DEPTH_TO_DESTALPHA ) + ( 16 * m_nFLASHLIGHTSHADOWS ) + ( 16 * m_nCASCADE_SIZE ) + ( 16 * m_nPIXELFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_lightmapped_4wayblend_ps30 psh_forgot_to_set_dynamic_FASTPATHENVMAPCONTRAST + psh_forgot_to_set_dynamic_FASTPATH + psh_forgot_to_set_dynamic_WRITEWATERFOGTODESTALPHA + psh_forgot_to_set_dynamic_WRITE_DEPTH_TO_DESTALPHA + psh_forgot_to_set_dynamic_FLASHLIGHTSHADOWS + 0

static const ShaderComboInformation_t s_DynamicComboArray_lightmapped_4wayblend_ps30[7] = 
{
	{ "FASTPATHENVMAPCONTRAST", 0, 1 },
	{ "FASTPATH", 0, 1 },
	{ "WRITEWATERFOGTODESTALPHA", 0, 1 },
	{ "WRITE_DEPTH_TO_DESTALPHA", 0, 1 },
	{ "FLASHLIGHTSHADOWS", 0, 0 },
	{ "CASCADE_SIZE", 0, 0 },
	{ "PIXELFOGTYPE", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_lightmapped_4wayblend_ps30[17] = 
{
	{ "BUMPMAP", 0, 2 },
	{ "BUMPMAP2", 0, 1 },
	{ "CUBEMAP", 0, 2 },
	{ "ENVMAPMASK", 0, 1 },
	{ "BASEALPHAENVMAPMASK", 0, 1 },
	{ "SELFILLUM", 0, 1 },
	{ "SEAMLESS", 0, 1 },
	{ "ENVMAPANISOTROPY", 0, 1 },
	{ "TEXTURE3_BLENDMODE", 0, 1 },
	{ "TEXTURE4_BLENDMODE", 0, 1 },
	{ "DETAIL_BLEND_MODE", 0, 12 },
	{ "SHADER_SRGB_READ", 0, 0 },
	{ "LIGHTING_PREVIEW", 0, 3 },
	{ "CASCADED_SHADOW_MAPPING", 0, 1 },
	{ "CSM_MODE", 0, 3 },
	{ "CSM_BLENDING", 0, 1 },
	{ "DOPIXELFOG", 0, 1 },
};
static const ShaderComboSemantics_t lightmapped_4wayblend_ps30_combos =
{
	"lightmapped_4wayblend_ps30", s_DynamicComboArray_lightmapped_4wayblend_ps30, 7, s_StaticComboArray_lightmapped_4wayblend_ps30, 17 
};

class ConstructMe_lightmapped_4wayblend_ps30
{
public:
	ConstructMe_lightmapped_4wayblend_ps30()
	{
		GetShaderDLL()->AddShaderComboInformation( &lightmapped_4wayblend_ps30_combos );
	}
};

static ConstructMe_lightmapped_4wayblend_ps30 s_ConstructMe_lightmapped_4wayblend_ps30;
