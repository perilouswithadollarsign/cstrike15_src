// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $SFM == 0 ) && ( $UBERLIGHT == 1 )
// $DETAIL_BLEND_MODE == 6				
// ( $FLASHLIGHT == 1 ) && ( $CUBEMAP == 1 ) 
// ($DETAILTEXTURE == 0 ) && ( $DETAIL_BLEND_MODE != 0 )
// ($DETAILTEXTURE == 0 ) && ( $SEAMLESS_DETAIL )
// ($ENVMAPMASK || $SELFILLUM_ENVMAPMASK_ALPHA) && ($SEAMLESS_BASE || $SEAMLESS_DETAIL)
// $BASEALPHAENVMAPMASK && $ENVMAPMASK
// $BASEALPHAENVMAPMASK && $SELFILLUM
// $SELFILLUM && $SELFILLUM_ENVMAPMASK_ALPHA
// $SELFILLUM_ENVMAPMASK_ALPHA && (! $ENVMAPMASK)
// $BASEALPHAENVMAPMASK && ($SEAMLESS_BASE || $SEAMLESS_DETAIL)
// ($DISTANCEALPHA == 0) && ($DISTANCEALPHAFROMDETAIL || $SOFT_MASK || $OUTLINE || $OUTER_GLOW)
// ($DETAILTEXTURE == 0) && ($DISTANCEALPHAFROMDETAIL)
// ( $CUBEMAP == 0 ) && ( ( $ENVMAPFRESNEL == 1 ) || ( $BASEALPHAENVMAPMASK == 1 ) )
// ( $CUBEMAP == 0 ) && ( $ENVMAPMASK == 1 ) && ( $SELFILLUM_ENVMAPMASK_ALPHA == 0 )
// ($DISTANCEALPHA) && ($ENVMAPMASK || $BASEALPHAENVMAPMASK || $SELFILLUM || $SELFILLUM_ENVMAPMASK_ALPHA || $ENVMAPFRESNEL)
// ($DISTANCEALPHA) && ($SEAMLESS_BASE || $SEAMLESS_DETAIL || $CUBEMAP )
// ($DISTANCEALPHA) && ($WRITEWATERFOGTODESTALPHA || $PIXELFOGTYPE || $FLASHLIGHT || $FLASHLIGHTSHADOWS )
// $SEAMLESS_BASE && $SRGB_INPUT_ADAPTER
// $DESATURATEWITHBASEALPHA && ( $SEAMLESS_BASE || $DETAILTEXTURE )
// $DESATURATEWITHBASEALPHA && ( $SELFILLUM || $SELFILLUM_ENVMAPMASK_ALPHA )
// $DESATURATEWITHBASEALPHA && $BASEALPHAENVMAPMASK
// $FLASHLIGHTSHADOWS && ($LIGHTING_PREVIEW == 3)
// ( $CASCADED_SHADOW_MAPPING == 0 ) && ( $CASCADE_SIZE != 0 ) 
// ( $CASCADED_SHADOW_MAPPING != 0 ) && ( $SFM != 0 ) 
// ( $CASCADED_SHADOW_MAPPING != 0 ) && ( $FLASHLIGHT != 0 ) 
// ( $CASCADED_SHADOW_MAPPING == 0 ) && ( $CSM_MODE != 0 ) 
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
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class vertexlit_and_unlit_generic_ps20_Static_Index
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
	int m_nDIFFUSELIGHTING;
#ifdef _DEBUG
	bool m_bDIFFUSELIGHTING;
#endif
public:
	void SetDIFFUSELIGHTING( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDIFFUSELIGHTING = i;
#ifdef _DEBUG
		m_bDIFFUSELIGHTING = true;
#endif
	}
	void SetDIFFUSELIGHTING( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDIFFUSELIGHTING = i ? 1 : 0;
#ifdef _DEBUG
		m_bDIFFUSELIGHTING = true;
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
	int m_nSELFILLUM_ENVMAPMASK_ALPHA;
#ifdef _DEBUG
	bool m_bSELFILLUM_ENVMAPMASK_ALPHA;
#endif
public:
	void SetSELFILLUM_ENVMAPMASK_ALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSELFILLUM_ENVMAPMASK_ALPHA = i;
#ifdef _DEBUG
		m_bSELFILLUM_ENVMAPMASK_ALPHA = true;
#endif
	}
	void SetSELFILLUM_ENVMAPMASK_ALPHA( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nSELFILLUM_ENVMAPMASK_ALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bSELFILLUM_ENVMAPMASK_ALPHA = true;
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
		Assert( i >= 0 && i <= 9 );
		m_nDETAIL_BLEND_MODE = i;
#ifdef _DEBUG
		m_bDETAIL_BLEND_MODE = true;
#endif
	}
	void SetDETAIL_BLEND_MODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 9 );
		m_nDETAIL_BLEND_MODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bDETAIL_BLEND_MODE = true;
#endif
	}
private:
	int m_nOUTLINE;
#ifdef _DEBUG
	bool m_bOUTLINE;
#endif
public:
	void SetOUTLINE( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nOUTLINE = i;
#ifdef _DEBUG
		m_bOUTLINE = true;
#endif
	}
	void SetOUTLINE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nOUTLINE = i ? 1 : 0;
#ifdef _DEBUG
		m_bOUTLINE = true;
#endif
	}
private:
	int m_nOUTER_GLOW;
#ifdef _DEBUG
	bool m_bOUTER_GLOW;
#endif
public:
	void SetOUTER_GLOW( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nOUTER_GLOW = i;
#ifdef _DEBUG
		m_bOUTER_GLOW = true;
#endif
	}
	void SetOUTER_GLOW( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nOUTER_GLOW = i ? 1 : 0;
#ifdef _DEBUG
		m_bOUTER_GLOW = true;
#endif
	}
private:
	int m_nSOFT_MASK;
#ifdef _DEBUG
	bool m_bSOFT_MASK;
#endif
public:
	void SetSOFT_MASK( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nSOFT_MASK = i;
#ifdef _DEBUG
		m_bSOFT_MASK = true;
#endif
	}
	void SetSOFT_MASK( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nSOFT_MASK = i ? 1 : 0;
#ifdef _DEBUG
		m_bSOFT_MASK = true;
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
	int m_nDISTANCEALPHA;
#ifdef _DEBUG
	bool m_bDISTANCEALPHA;
#endif
public:
	void SetDISTANCEALPHA( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nDISTANCEALPHA = i;
#ifdef _DEBUG
		m_bDISTANCEALPHA = true;
#endif
	}
	void SetDISTANCEALPHA( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nDISTANCEALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bDISTANCEALPHA = true;
#endif
	}
private:
	int m_nDISTANCEALPHAFROMDETAIL;
#ifdef _DEBUG
	bool m_bDISTANCEALPHAFROMDETAIL;
#endif
public:
	void SetDISTANCEALPHAFROMDETAIL( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nDISTANCEALPHAFROMDETAIL = i;
#ifdef _DEBUG
		m_bDISTANCEALPHAFROMDETAIL = true;
#endif
	}
	void SetDISTANCEALPHAFROMDETAIL( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nDISTANCEALPHAFROMDETAIL = i ? 1 : 0;
#ifdef _DEBUG
		m_bDISTANCEALPHAFROMDETAIL = true;
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
	int m_nDESATURATEWITHBASEALPHA;
#ifdef _DEBUG
	bool m_bDESATURATEWITHBASEALPHA;
#endif
public:
	void SetDESATURATEWITHBASEALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDESATURATEWITHBASEALPHA = i;
#ifdef _DEBUG
		m_bDESATURATEWITHBASEALPHA = true;
#endif
	}
	void SetDESATURATEWITHBASEALPHA( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDESATURATEWITHBASEALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bDESATURATEWITHBASEALPHA = true;
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
	int m_nENVMAPFRESNEL;
#ifdef _DEBUG
	bool m_bENVMAPFRESNEL;
#endif
public:
	void SetENVMAPFRESNEL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nENVMAPFRESNEL = i;
#ifdef _DEBUG
		m_bENVMAPFRESNEL = true;
#endif
	}
	void SetENVMAPFRESNEL( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nENVMAPFRESNEL = i ? 1 : 0;
#ifdef _DEBUG
		m_bENVMAPFRESNEL = true;
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
	int m_nCSM_MODE;
#ifdef _DEBUG
	bool m_bCSM_MODE;
#endif
public:
	void SetCSM_MODE( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nCSM_MODE = i;
#ifdef _DEBUG
		m_bCSM_MODE = true;
#endif
	}
	void SetCSM_MODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
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
	int m_nDECAL_BLEND_MODE;
#ifdef _DEBUG
	bool m_bDECAL_BLEND_MODE;
#endif
public:
	void SetDECAL_BLEND_MODE( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nDECAL_BLEND_MODE = i;
#ifdef _DEBUG
		m_bDECAL_BLEND_MODE = true;
#endif
	}
	void SetDECAL_BLEND_MODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 2 );
		m_nDECAL_BLEND_MODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bDECAL_BLEND_MODE = true;
#endif
	}
private:
	int m_nTINTMASKTEXTURE;
#ifdef _DEBUG
	bool m_bTINTMASKTEXTURE;
#endif
public:
	void SetTINTMASKTEXTURE( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nTINTMASKTEXTURE = i;
#ifdef _DEBUG
		m_bTINTMASKTEXTURE = true;
#endif
	}
	void SetTINTMASKTEXTURE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nTINTMASKTEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bTINTMASKTEXTURE = true;
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
		Assert( i >= 0 && i <= 0 );
		m_nDOPIXELFOG = i;
#ifdef _DEBUG
		m_bDOPIXELFOG = true;
#endif
	}
	void SetDOPIXELFOG( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nDOPIXELFOG = i ? 1 : 0;
#ifdef _DEBUG
		m_bDOPIXELFOG = true;
#endif
	}
public:
	// CONSTRUCTOR
	vertexlit_and_unlit_generic_ps20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bSFM = false;
#endif // _DEBUG
		m_nSFM = 0;
#ifdef _DEBUG
		m_bDETAILTEXTURE = false;
#endif // _DEBUG
		m_nDETAILTEXTURE = 0;
#ifdef _DEBUG
		m_bCUBEMAP = false;
#endif // _DEBUG
		m_nCUBEMAP = 0;
#ifdef _DEBUG
		m_bDIFFUSELIGHTING = false;
#endif // _DEBUG
		m_nDIFFUSELIGHTING = 0;
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
		m_bVERTEXCOLOR = false;
#endif // _DEBUG
		m_nVERTEXCOLOR = 0;
#ifdef _DEBUG
		m_bFLASHLIGHT = false;
#endif // _DEBUG
		m_nFLASHLIGHT = 0;
#ifdef _DEBUG
		m_bSELFILLUM_ENVMAPMASK_ALPHA = false;
#endif // _DEBUG
		m_nSELFILLUM_ENVMAPMASK_ALPHA = 0;
#ifdef _DEBUG
		m_bDETAIL_BLEND_MODE = false;
#endif // _DEBUG
		m_nDETAIL_BLEND_MODE = 0;
#ifdef _DEBUG
		m_bOUTLINE = false;
#endif // _DEBUG
		m_nOUTLINE = 0;
#ifdef _DEBUG
		m_bOUTER_GLOW = false;
#endif // _DEBUG
		m_nOUTER_GLOW = 0;
#ifdef _DEBUG
		m_bSOFT_MASK = false;
#endif // _DEBUG
		m_nSOFT_MASK = 0;
#ifdef _DEBUG
		m_bSEAMLESS_BASE = false;
#endif // _DEBUG
		m_nSEAMLESS_BASE = 0;
#ifdef _DEBUG
		m_bSEAMLESS_DETAIL = false;
#endif // _DEBUG
		m_nSEAMLESS_DETAIL = 0;
#ifdef _DEBUG
		m_bDISTANCEALPHA = false;
#endif // _DEBUG
		m_nDISTANCEALPHA = 0;
#ifdef _DEBUG
		m_bDISTANCEALPHAFROMDETAIL = false;
#endif // _DEBUG
		m_nDISTANCEALPHAFROMDETAIL = 0;
#ifdef _DEBUG
		m_bSHADER_SRGB_READ = false;
#endif // _DEBUG
		m_nSHADER_SRGB_READ = 0;
#ifdef _DEBUG
		m_bDESATURATEWITHBASEALPHA = false;
#endif // _DEBUG
		m_nDESATURATEWITHBASEALPHA = 0;
#ifdef _DEBUG
		m_bLIGHTING_PREVIEW = false;
#endif // _DEBUG
		m_nLIGHTING_PREVIEW = 0;
#ifdef _DEBUG
		m_bENVMAPFRESNEL = false;
#endif // _DEBUG
		m_nENVMAPFRESNEL = 0;
#ifdef _DEBUG
		m_bCASCADED_SHADOW_MAPPING = false;
#endif // _DEBUG
		m_nCASCADED_SHADOW_MAPPING = 0;
#ifdef _DEBUG
		m_bCSM_MODE = false;
#endif // _DEBUG
		m_nCSM_MODE = 0;
#ifdef _DEBUG
		m_bCSM_BLENDING = false;
#endif // _DEBUG
		m_nCSM_BLENDING = 0;
#ifdef _DEBUG
		m_bDECAL_BLEND_MODE = false;
#endif // _DEBUG
		m_nDECAL_BLEND_MODE = 0;
#ifdef _DEBUG
		m_bTINTMASKTEXTURE = false;
#endif // _DEBUG
		m_nTINTMASKTEXTURE = 0;
#ifdef _DEBUG
		m_bDOPIXELFOG = true;
#endif // _DEBUG
		m_nDOPIXELFOG = 0 ;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bSFM && m_bDETAILTEXTURE && m_bCUBEMAP && m_bDIFFUSELIGHTING && m_bENVMAPMASK && m_bBASEALPHAENVMAPMASK && m_bSELFILLUM && m_bVERTEXCOLOR && m_bFLASHLIGHT && m_bSELFILLUM_ENVMAPMASK_ALPHA && m_bDETAIL_BLEND_MODE && m_bOUTLINE && m_bOUTER_GLOW && m_bSOFT_MASK && m_bSEAMLESS_BASE && m_bSEAMLESS_DETAIL && m_bDISTANCEALPHA && m_bDISTANCEALPHAFROMDETAIL && m_bSHADER_SRGB_READ && m_bDESATURATEWITHBASEALPHA && m_bLIGHTING_PREVIEW && m_bENVMAPFRESNEL && m_bCASCADED_SHADOW_MAPPING && m_bCSM_MODE && m_bCSM_BLENDING && m_bDECAL_BLEND_MODE && m_bTINTMASKTEXTURE && m_bDOPIXELFOG;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 8 * m_nSFM ) + ( 8 * m_nDETAILTEXTURE ) + ( 16 * m_nCUBEMAP ) + ( 32 * m_nDIFFUSELIGHTING ) + ( 64 * m_nENVMAPMASK ) + ( 128 * m_nBASEALPHAENVMAPMASK ) + ( 256 * m_nSELFILLUM ) + ( 512 * m_nVERTEXCOLOR ) + ( 1024 * m_nFLASHLIGHT ) + ( 2048 * m_nSELFILLUM_ENVMAPMASK_ALPHA ) + ( 4096 * m_nDETAIL_BLEND_MODE ) + ( 40960 * m_nOUTLINE ) + ( 40960 * m_nOUTER_GLOW ) + ( 40960 * m_nSOFT_MASK ) + ( 40960 * m_nSEAMLESS_BASE ) + ( 40960 * m_nSEAMLESS_DETAIL ) + ( 40960 * m_nDISTANCEALPHA ) + ( 40960 * m_nDISTANCEALPHAFROMDETAIL ) + ( 40960 * m_nSHADER_SRGB_READ ) + ( 40960 * m_nDESATURATEWITHBASEALPHA ) + ( 81920 * m_nLIGHTING_PREVIEW ) + ( 81920 * m_nENVMAPFRESNEL ) + ( 163840 * m_nCASCADED_SHADOW_MAPPING ) + ( 163840 * m_nCSM_MODE ) + ( 163840 * m_nCSM_BLENDING ) + ( 327680 * m_nDECAL_BLEND_MODE ) + ( 983040 * m_nTINTMASKTEXTURE ) + ( 983040 * m_nDOPIXELFOG ) + 0;
	}
};
#define shaderStaticTest_vertexlit_and_unlit_generic_ps20 psh_forgot_to_set_static_SFM + psh_forgot_to_set_static_DETAILTEXTURE + psh_forgot_to_set_static_CUBEMAP + psh_forgot_to_set_static_DIFFUSELIGHTING + psh_forgot_to_set_static_ENVMAPMASK + psh_forgot_to_set_static_BASEALPHAENVMAPMASK + psh_forgot_to_set_static_SELFILLUM + psh_forgot_to_set_static_VERTEXCOLOR + psh_forgot_to_set_static_FLASHLIGHT + psh_forgot_to_set_static_SELFILLUM_ENVMAPMASK_ALPHA + psh_forgot_to_set_static_DETAIL_BLEND_MODE + psh_forgot_to_set_static_OUTLINE + psh_forgot_to_set_static_OUTER_GLOW + psh_forgot_to_set_static_SOFT_MASK + psh_forgot_to_set_static_SEAMLESS_BASE + psh_forgot_to_set_static_SEAMLESS_DETAIL + psh_forgot_to_set_static_DISTANCEALPHA + psh_forgot_to_set_static_DISTANCEALPHAFROMDETAIL + psh_forgot_to_set_static_SHADER_SRGB_READ + psh_forgot_to_set_static_DESATURATEWITHBASEALPHA + psh_forgot_to_set_static_LIGHTING_PREVIEW + psh_forgot_to_set_static_ENVMAPFRESNEL + psh_forgot_to_set_static_CASCADED_SHADOW_MAPPING + psh_forgot_to_set_static_CSM_MODE + psh_forgot_to_set_static_CSM_BLENDING + psh_forgot_to_set_static_DECAL_BLEND_MODE + psh_forgot_to_set_static_TINTMASKTEXTURE + 0
class vertexlit_and_unlit_generic_ps20_Dynamic_Index
{
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
	int m_nSMOKEGRENADEBLEND;
#ifdef _DEBUG
	bool m_bSMOKEGRENADEBLEND;
#endif
public:
	void SetSMOKEGRENADEBLEND( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSMOKEGRENADEBLEND = i;
#ifdef _DEBUG
		m_bSMOKEGRENADEBLEND = true;
#endif
	}
	void SetSMOKEGRENADEBLEND( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nSMOKEGRENADEBLEND = i ? 1 : 0;
#ifdef _DEBUG
		m_bSMOKEGRENADEBLEND = true;
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
	vertexlit_and_unlit_generic_ps20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bWRITEWATERFOGTODESTALPHA = false;
#endif // _DEBUG
		m_nWRITEWATERFOGTODESTALPHA = 0;
#ifdef _DEBUG
		m_bCASCADE_SIZE = false;
#endif // _DEBUG
		m_nCASCADE_SIZE = 0;
#ifdef _DEBUG
		m_bSMOKEGRENADEBLEND = false;
#endif // _DEBUG
		m_nSMOKEGRENADEBLEND = 0;
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
		bool bAllDynamicVarsDefined = m_bWRITEWATERFOGTODESTALPHA && m_bCASCADE_SIZE && m_bSMOKEGRENADEBLEND && m_bPIXELFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nWRITEWATERFOGTODESTALPHA ) + ( 2 * m_nCASCADE_SIZE ) + ( 2 * m_nSMOKEGRENADEBLEND ) + ( 4 * m_nPIXELFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_vertexlit_and_unlit_generic_ps20 psh_forgot_to_set_dynamic_WRITEWATERFOGTODESTALPHA + psh_forgot_to_set_dynamic_CASCADE_SIZE + psh_forgot_to_set_dynamic_SMOKEGRENADEBLEND + 0

static const ShaderComboInformation_t s_DynamicComboArray_vertexlit_and_unlit_generic_ps20[4] = 
{
	{ "WRITEWATERFOGTODESTALPHA", 0, 1 },
	{ "CASCADE_SIZE", 0, 0 },
	{ "SMOKEGRENADEBLEND", 0, 1 },
	{ "PIXELFOGTYPE", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_vertexlit_and_unlit_generic_ps20[28] = 
{
	{ "SFM", 0, 0 },
	{ "DETAILTEXTURE", 0, 1 },
	{ "CUBEMAP", 0, 1 },
	{ "DIFFUSELIGHTING", 0, 1 },
	{ "ENVMAPMASK", 0, 1 },
	{ "BASEALPHAENVMAPMASK", 0, 1 },
	{ "SELFILLUM", 0, 1 },
	{ "VERTEXCOLOR", 0, 1 },
	{ "FLASHLIGHT", 0, 1 },
	{ "SELFILLUM_ENVMAPMASK_ALPHA", 0, 1 },
	{ "DETAIL_BLEND_MODE", 0, 9 },
	{ "OUTLINE", 0, 0 },
	{ "OUTER_GLOW", 0, 0 },
	{ "SOFT_MASK", 0, 0 },
	{ "SEAMLESS_BASE", 0, 0 },
	{ "SEAMLESS_DETAIL", 0, 0 },
	{ "DISTANCEALPHA", 0, 0 },
	{ "DISTANCEALPHAFROMDETAIL", 0, 0 },
	{ "SHADER_SRGB_READ", 0, 0 },
	{ "DESATURATEWITHBASEALPHA", 0, 1 },
	{ "LIGHTING_PREVIEW", 0, 0 },
	{ "ENVMAPFRESNEL", 0, 1 },
	{ "CASCADED_SHADOW_MAPPING", 0, 0 },
	{ "CSM_MODE", 0, 0 },
	{ "CSM_BLENDING", 0, 1 },
	{ "DECAL_BLEND_MODE", 0, 2 },
	{ "TINTMASKTEXTURE", 0, 0 },
	{ "DOPIXELFOG", 0, 0 },
};
static const ShaderComboSemantics_t vertexlit_and_unlit_generic_ps20_combos =
{
	"vertexlit_and_unlit_generic_ps20", s_DynamicComboArray_vertexlit_and_unlit_generic_ps20, 4, s_StaticComboArray_vertexlit_and_unlit_generic_ps20, 28 
};

class ConstructMe_vertexlit_and_unlit_generic_ps20
{
public:
	ConstructMe_vertexlit_and_unlit_generic_ps20()
	{
		GetShaderDLL()->AddShaderComboInformation( &vertexlit_and_unlit_generic_ps20_combos );
	}
};

static ConstructMe_vertexlit_and_unlit_generic_ps20 s_ConstructMe_vertexlit_and_unlit_generic_ps20;
