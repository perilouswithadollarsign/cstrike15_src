// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $SFM == 0 ) && ( $UBERLIGHT == 1 )
// (! $DETAILTEXTURE) && ( $DETAIL_BLEND_MODE != 0 )
// ( $FLASHLIGHT == 0 ) && ( $FLASHLIGHTSHADOWS == 1 )
// ( $SELFILLUM == 0 ) && ( $SELFILLUMFRESNEL == 1 )
// ( $WORLD_NORMAL == 1 ) && ( $FLASHLIGHTSHADOWS == 1 ) && ( $NUM_LIGHTS != 0 ) && ( $WRITEWATERFOGTODESTALPHA == 1 )
// ( $CASCADED_SHADOW_MAPPING == 0 ) && ( $CASCADE_SIZE != 0 ) 
// ( $CASCADED_SHADOW_MAPPING != 0 ) && ( $SFM != 0 ) 
// ( $CASCADED_SHADOW_MAPPING != 0 ) && ( $FLASHLIGHT != 0 ) 
// ( $CASCADED_SHADOW_MAPPING == 0 ) && ( $CSM_MODE != 0 ) 
// ( $CASCADED_SHADOW_MAPPING == 0 ) && ( $CSM_VIEWMODELQUALITY != 0 ) 
// ( $CASCADE_SIZE == 0 ) && ( $CSM_VIEWMODELQUALITY != 0 ) 
// ( $NUMLIGHTS > 2 ) 
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
class phong_ps20b_Static_Index
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
	int m_nSELFILLUMFRESNEL;
#ifdef _DEBUG
	bool m_bSELFILLUMFRESNEL;
#endif
public:
	void SetSELFILLUMFRESNEL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSELFILLUMFRESNEL = i;
#ifdef _DEBUG
		m_bSELFILLUMFRESNEL = true;
#endif
	}
	void SetSELFILLUMFRESNEL( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nSELFILLUMFRESNEL = i ? 1 : 0;
#ifdef _DEBUG
		m_bSELFILLUMFRESNEL = true;
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
	int m_nLIGHTWARPTEXTURE;
#ifdef _DEBUG
	bool m_bLIGHTWARPTEXTURE;
#endif
public:
	void SetLIGHTWARPTEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nLIGHTWARPTEXTURE = i;
#ifdef _DEBUG
		m_bLIGHTWARPTEXTURE = true;
#endif
	}
	void SetLIGHTWARPTEXTURE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nLIGHTWARPTEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bLIGHTWARPTEXTURE = true;
#endif
	}
private:
	int m_nPHONGWARPTEXTURE;
#ifdef _DEBUG
	bool m_bPHONGWARPTEXTURE;
#endif
public:
	void SetPHONGWARPTEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPHONGWARPTEXTURE = i;
#ifdef _DEBUG
		m_bPHONGWARPTEXTURE = true;
#endif
	}
	void SetPHONGWARPTEXTURE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nPHONGWARPTEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bPHONGWARPTEXTURE = true;
#endif
	}
private:
	int m_nWRINKLEMAP;
#ifdef _DEBUG
	bool m_bWRINKLEMAP;
#endif
public:
	void SetWRINKLEMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nWRINKLEMAP = i;
#ifdef _DEBUG
		m_bWRINKLEMAP = true;
#endif
	}
	void SetWRINKLEMAP( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nWRINKLEMAP = i ? 1 : 0;
#ifdef _DEBUG
		m_bWRINKLEMAP = true;
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
		Assert( i >= 0 && i <= 7 );
		m_nDETAIL_BLEND_MODE = i;
#ifdef _DEBUG
		m_bDETAIL_BLEND_MODE = true;
#endif
	}
	void SetDETAIL_BLEND_MODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 7 );
		m_nDETAIL_BLEND_MODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bDETAIL_BLEND_MODE = true;
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
	int m_nRIMLIGHT;
#ifdef _DEBUG
	bool m_bRIMLIGHT;
#endif
public:
	void SetRIMLIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nRIMLIGHT = i;
#ifdef _DEBUG
		m_bRIMLIGHT = true;
#endif
	}
	void SetRIMLIGHT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nRIMLIGHT = i ? 1 : 0;
#ifdef _DEBUG
		m_bRIMLIGHT = true;
#endif
	}
private:
	int m_nFLASHLIGHTDEPTHFILTERMODE;
#ifdef _DEBUG
	bool m_bFLASHLIGHTDEPTHFILTERMODE;
#endif
public:
	void SetFLASHLIGHTDEPTHFILTERMODE( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nFLASHLIGHTDEPTHFILTERMODE = i;
#ifdef _DEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = true;
#endif
	}
	void SetFLASHLIGHTDEPTHFILTERMODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nFLASHLIGHTDEPTHFILTERMODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = true;
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
	int m_nPHONG_HALFLAMBERT;
#ifdef _DEBUG
	bool m_bPHONG_HALFLAMBERT;
#endif
public:
	void SetPHONG_HALFLAMBERT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPHONG_HALFLAMBERT = i;
#ifdef _DEBUG
		m_bPHONG_HALFLAMBERT = true;
#endif
	}
	void SetPHONG_HALFLAMBERT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nPHONG_HALFLAMBERT = i ? 1 : 0;
#ifdef _DEBUG
		m_bPHONG_HALFLAMBERT = true;
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
public:
	// CONSTRUCTOR
	phong_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bSFM = false;
#endif // _DEBUG
		m_nSFM = 0;
#ifdef _DEBUG
		m_bCUBEMAP = false;
#endif // _DEBUG
		m_nCUBEMAP = 0;
#ifdef _DEBUG
		m_bSELFILLUM = false;
#endif // _DEBUG
		m_nSELFILLUM = 0;
#ifdef _DEBUG
		m_bSELFILLUMFRESNEL = false;
#endif // _DEBUG
		m_nSELFILLUMFRESNEL = 0;
#ifdef _DEBUG
		m_bFLASHLIGHT = false;
#endif // _DEBUG
		m_nFLASHLIGHT = 0;
#ifdef _DEBUG
		m_bLIGHTWARPTEXTURE = false;
#endif // _DEBUG
		m_nLIGHTWARPTEXTURE = 0;
#ifdef _DEBUG
		m_bPHONGWARPTEXTURE = false;
#endif // _DEBUG
		m_nPHONGWARPTEXTURE = 0;
#ifdef _DEBUG
		m_bWRINKLEMAP = false;
#endif // _DEBUG
		m_nWRINKLEMAP = 0;
#ifdef _DEBUG
		m_bDETAIL_BLEND_MODE = false;
#endif // _DEBUG
		m_nDETAIL_BLEND_MODE = 0;
#ifdef _DEBUG
		m_bDETAILTEXTURE = false;
#endif // _DEBUG
		m_nDETAILTEXTURE = 0;
#ifdef _DEBUG
		m_bRIMLIGHT = false;
#endif // _DEBUG
		m_nRIMLIGHT = 0;
#ifdef _DEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = false;
#endif // _DEBUG
		m_nFLASHLIGHTDEPTHFILTERMODE = 0;
#ifdef _DEBUG
		m_bSHADER_SRGB_READ = false;
#endif // _DEBUG
		m_nSHADER_SRGB_READ = 0;
#ifdef _DEBUG
		m_bWORLD_NORMAL = false;
#endif // _DEBUG
		m_nWORLD_NORMAL = 0;
#ifdef _DEBUG
		m_bPHONG_HALFLAMBERT = false;
#endif // _DEBUG
		m_nPHONG_HALFLAMBERT = 0;
#ifdef _DEBUG
		m_bCASCADED_SHADOW_MAPPING = false;
#endif // _DEBUG
		m_nCASCADED_SHADOW_MAPPING = 0;
#ifdef _DEBUG
		m_bCSM_MODE = false;
#endif // _DEBUG
		m_nCSM_MODE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bSFM && m_bCUBEMAP && m_bSELFILLUM && m_bSELFILLUMFRESNEL && m_bFLASHLIGHT && m_bLIGHTWARPTEXTURE && m_bPHONGWARPTEXTURE && m_bWRINKLEMAP && m_bDETAIL_BLEND_MODE && m_bDETAILTEXTURE && m_bRIMLIGHT && m_bFLASHLIGHTDEPTHFILTERMODE && m_bSHADER_SRGB_READ && m_bWORLD_NORMAL && m_bPHONG_HALFLAMBERT && m_bCASCADED_SHADOW_MAPPING && m_bCSM_MODE;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 160 * m_nSFM ) + ( 160 * m_nCUBEMAP ) + ( 320 * m_nSELFILLUM ) + ( 640 * m_nSELFILLUMFRESNEL ) + ( 1280 * m_nFLASHLIGHT ) + ( 2560 * m_nLIGHTWARPTEXTURE ) + ( 5120 * m_nPHONGWARPTEXTURE ) + ( 10240 * m_nWRINKLEMAP ) + ( 20480 * m_nDETAIL_BLEND_MODE ) + ( 163840 * m_nDETAILTEXTURE ) + ( 327680 * m_nRIMLIGHT ) + ( 655360 * m_nFLASHLIGHTDEPTHFILTERMODE ) + ( 655360 * m_nSHADER_SRGB_READ ) + ( 655360 * m_nWORLD_NORMAL ) + ( 655360 * m_nPHONG_HALFLAMBERT ) + ( 1310720 * m_nCASCADED_SHADOW_MAPPING ) + ( 2621440 * m_nCSM_MODE ) + 0;
	}
};
#define shaderStaticTest_phong_ps20b psh_forgot_to_set_static_SFM + psh_forgot_to_set_static_CUBEMAP + psh_forgot_to_set_static_SELFILLUM + psh_forgot_to_set_static_SELFILLUMFRESNEL + psh_forgot_to_set_static_FLASHLIGHT + psh_forgot_to_set_static_LIGHTWARPTEXTURE + psh_forgot_to_set_static_PHONGWARPTEXTURE + psh_forgot_to_set_static_WRINKLEMAP + psh_forgot_to_set_static_DETAIL_BLEND_MODE + psh_forgot_to_set_static_DETAILTEXTURE + psh_forgot_to_set_static_RIMLIGHT + psh_forgot_to_set_static_FLASHLIGHTDEPTHFILTERMODE + psh_forgot_to_set_static_SHADER_SRGB_READ + psh_forgot_to_set_static_WORLD_NORMAL + psh_forgot_to_set_static_PHONG_HALFLAMBERT + psh_forgot_to_set_static_CASCADED_SHADOW_MAPPING + psh_forgot_to_set_static_CSM_MODE + 0
class phong_ps20b_Dynamic_Index
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
private:
	int m_nNUM_LIGHTS;
#ifdef _DEBUG
	bool m_bNUM_LIGHTS;
#endif
public:
	void SetNUM_LIGHTS( int i )
	{
		Assert( i >= 0 && i <= 4 );
		m_nNUM_LIGHTS = i;
#ifdef _DEBUG
		m_bNUM_LIGHTS = true;
#endif
	}
	void SetNUM_LIGHTS( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 4 );
		m_nNUM_LIGHTS = i ? 1 : 0;
#ifdef _DEBUG
		m_bNUM_LIGHTS = true;
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
		Assert( i >= 0 && i <= 0 );
		m_nWRITE_DEPTH_TO_DESTALPHA = i;
#ifdef _DEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = true;
#endif
	}
	void SetWRITE_DEPTH_TO_DESTALPHA( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
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
		Assert( i >= 0 && i <= 1 );
		m_nFLASHLIGHTSHADOWS = i;
#ifdef _DEBUG
		m_bFLASHLIGHTSHADOWS = true;
#endif
	}
	void SetFLASHLIGHTSHADOWS( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
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
		Assert( i >= 0 && i <= 1 );
		m_nCASCADE_SIZE = i;
#ifdef _DEBUG
		m_bCASCADE_SIZE = true;
#endif
	}
	void SetCASCADE_SIZE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nCASCADE_SIZE = i ? 1 : 0;
#ifdef _DEBUG
		m_bCASCADE_SIZE = true;
#endif
	}
private:
	int m_nCSM_VIEWMODELQUALITY;
#ifdef _DEBUG
	bool m_bCSM_VIEWMODELQUALITY;
#endif
public:
	void SetCSM_VIEWMODELQUALITY( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCSM_VIEWMODELQUALITY = i;
#ifdef _DEBUG
		m_bCSM_VIEWMODELQUALITY = true;
#endif
	}
	void SetCSM_VIEWMODELQUALITY( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nCSM_VIEWMODELQUALITY = i ? 1 : 0;
#ifdef _DEBUG
		m_bCSM_VIEWMODELQUALITY = true;
#endif
	}
public:
	// CONSTRUCTOR
	phong_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bWRITEWATERFOGTODESTALPHA = false;
#endif // _DEBUG
		m_nWRITEWATERFOGTODESTALPHA = 0;
#ifdef _DEBUG
		m_bPIXELFOGTYPE = true;
#endif // _DEBUG
		m_nPIXELFOGTYPE = ( pShaderAPI->GetSceneFogMode() == MATERIAL_FOG_LINEAR_BELOW_FOG_Z ) ;
#ifdef _DEBUG
		m_bNUM_LIGHTS = false;
#endif // _DEBUG
		m_nNUM_LIGHTS = 0;
#ifdef _DEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = false;
#endif // _DEBUG
		m_nWRITE_DEPTH_TO_DESTALPHA = 0;
#ifdef _DEBUG
		m_bFLASHLIGHTSHADOWS = false;
#endif // _DEBUG
		m_nFLASHLIGHTSHADOWS = 0;
#ifdef _DEBUG
		m_bCASCADE_SIZE = false;
#endif // _DEBUG
		m_nCASCADE_SIZE = 0;
#ifdef _DEBUG
		m_bCSM_VIEWMODELQUALITY = false;
#endif // _DEBUG
		m_nCSM_VIEWMODELQUALITY = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bWRITEWATERFOGTODESTALPHA && m_bPIXELFOGTYPE && m_bNUM_LIGHTS && m_bWRITE_DEPTH_TO_DESTALPHA && m_bFLASHLIGHTSHADOWS && m_bCASCADE_SIZE && m_bCSM_VIEWMODELQUALITY;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nWRITEWATERFOGTODESTALPHA ) + ( 2 * m_nPIXELFOGTYPE ) + ( 4 * m_nNUM_LIGHTS ) + ( 20 * m_nWRITE_DEPTH_TO_DESTALPHA ) + ( 20 * m_nFLASHLIGHTSHADOWS ) + ( 40 * m_nCASCADE_SIZE ) + ( 80 * m_nCSM_VIEWMODELQUALITY ) + 0;
	}
};
#define shaderDynamicTest_phong_ps20b psh_forgot_to_set_dynamic_WRITEWATERFOGTODESTALPHA + psh_forgot_to_set_dynamic_NUM_LIGHTS + psh_forgot_to_set_dynamic_WRITE_DEPTH_TO_DESTALPHA + psh_forgot_to_set_dynamic_FLASHLIGHTSHADOWS + psh_forgot_to_set_dynamic_CASCADE_SIZE + psh_forgot_to_set_dynamic_CSM_VIEWMODELQUALITY + 0

static const ShaderComboInformation_t s_DynamicComboArray_phong_ps20b[7] = 
{
	{ "WRITEWATERFOGTODESTALPHA", 0, 1 },
	{ "PIXELFOGTYPE", 0, 1 },
	{ "NUM_LIGHTS", 0, 4 },
	{ "WRITE_DEPTH_TO_DESTALPHA", 0, 0 },
	{ "FLASHLIGHTSHADOWS", 0, 1 },
	{ "CASCADE_SIZE", 0, 1 },
	{ "CSM_VIEWMODELQUALITY", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_phong_ps20b[17] = 
{
	{ "SFM", 0, 0 },
	{ "CUBEMAP", 0, 1 },
	{ "SELFILLUM", 0, 1 },
	{ "SELFILLUMFRESNEL", 0, 1 },
	{ "FLASHLIGHT", 0, 1 },
	{ "LIGHTWARPTEXTURE", 0, 1 },
	{ "PHONGWARPTEXTURE", 0, 1 },
	{ "WRINKLEMAP", 0, 1 },
	{ "DETAIL_BLEND_MODE", 0, 7 },
	{ "DETAILTEXTURE", 0, 1 },
	{ "RIMLIGHT", 0, 1 },
	{ "FLASHLIGHTDEPTHFILTERMODE", 0, 0 },
	{ "SHADER_SRGB_READ", 0, 0 },
	{ "WORLD_NORMAL", 0, 0 },
	{ "PHONG_HALFLAMBERT", 0, 1 },
	{ "CASCADED_SHADOW_MAPPING", 0, 1 },
	{ "CSM_MODE", 0, 0 },
};
static const ShaderComboSemantics_t phong_ps20b_combos =
{
	"phong_ps20b", s_DynamicComboArray_phong_ps20b, 7, s_StaticComboArray_phong_ps20b, 17 
};

class ConstructMe_phong_ps20b
{
public:
	ConstructMe_phong_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &phong_ps20b_combos );
	}
};

static ConstructMe_phong_ps20b s_ConstructMe_phong_ps20b;
