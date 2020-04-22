// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $CONVERT_FROM_LINEAR == 0 ) && ( $CONVERT_TO_LINEAR == 1 )
// ( $TOOL_MODE == 0 ) && ( $CONVERT_FROM_LINEAR == 1 )
// ( $TOOL_MODE == 0 ) && ( $CONVERT_TO_LINEAR == 1 )
// ( $CONVERT_FROM_LINEAR == 1 ) && ( $LINEAR_INPUT  == 1 )
// ( $CONVERT_TO_LINEAR   == 1 ) && ( $LINEAR_OUTPUT == 1 )
// ( $LOCAL_CONTRAST_ENABLE == 0 ) && ( $BLURRED_VIGNETTE_ENABLE == 1 )
// ( $TOOL_MODE == 0 ) && $TV_GAMMA
// ( $TOOL_MODE == 0 ) && $DESATURATEENABLE
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class engine_post_ps20b_Static_Index
{
private:
	int m_nTOOL_MODE;
#ifdef _DEBUG
	bool m_bTOOL_MODE;
#endif
public:
	void SetTOOL_MODE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nTOOL_MODE = i;
#ifdef _DEBUG
		m_bTOOL_MODE = true;
#endif
	}
	void SetTOOL_MODE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nTOOL_MODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bTOOL_MODE = true;
#endif
	}
private:
	int m_nDEPTH_BLUR_ENABLE;
#ifdef _DEBUG
	bool m_bDEPTH_BLUR_ENABLE;
#endif
public:
	void SetDEPTH_BLUR_ENABLE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDEPTH_BLUR_ENABLE = i;
#ifdef _DEBUG
		m_bDEPTH_BLUR_ENABLE = true;
#endif
	}
	void SetDEPTH_BLUR_ENABLE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDEPTH_BLUR_ENABLE = i ? 1 : 0;
#ifdef _DEBUG
		m_bDEPTH_BLUR_ENABLE = true;
#endif
	}
private:
	int m_nLINEAR_INPUT;
#ifdef _DEBUG
	bool m_bLINEAR_INPUT;
#endif
public:
	void SetLINEAR_INPUT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nLINEAR_INPUT = i;
#ifdef _DEBUG
		m_bLINEAR_INPUT = true;
#endif
	}
	void SetLINEAR_INPUT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nLINEAR_INPUT = i ? 1 : 0;
#ifdef _DEBUG
		m_bLINEAR_INPUT = true;
#endif
	}
private:
	int m_nLINEAR_OUTPUT;
#ifdef _DEBUG
	bool m_bLINEAR_OUTPUT;
#endif
public:
	void SetLINEAR_OUTPUT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nLINEAR_OUTPUT = i;
#ifdef _DEBUG
		m_bLINEAR_OUTPUT = true;
#endif
	}
	void SetLINEAR_OUTPUT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nLINEAR_OUTPUT = i ? 1 : 0;
#ifdef _DEBUG
		m_bLINEAR_OUTPUT = true;
#endif
	}
public:
	// CONSTRUCTOR
	engine_post_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bTOOL_MODE = false;
#endif // _DEBUG
		m_nTOOL_MODE = 0;
#ifdef _DEBUG
		m_bDEPTH_BLUR_ENABLE = false;
#endif // _DEBUG
		m_nDEPTH_BLUR_ENABLE = 0;
#ifdef _DEBUG
		m_bLINEAR_INPUT = false;
#endif // _DEBUG
		m_nLINEAR_INPUT = 0;
#ifdef _DEBUG
		m_bLINEAR_OUTPUT = false;
#endif // _DEBUG
		m_nLINEAR_OUTPUT = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bTOOL_MODE && m_bDEPTH_BLUR_ENABLE && m_bLINEAR_INPUT && m_bLINEAR_OUTPUT;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 3072 * m_nTOOL_MODE ) + ( 6144 * m_nDEPTH_BLUR_ENABLE ) + ( 12288 * m_nLINEAR_INPUT ) + ( 24576 * m_nLINEAR_OUTPUT ) + 0;
	}
};
#define shaderStaticTest_engine_post_ps20b psh_forgot_to_set_static_TOOL_MODE + psh_forgot_to_set_static_DEPTH_BLUR_ENABLE + psh_forgot_to_set_static_LINEAR_INPUT + psh_forgot_to_set_static_LINEAR_OUTPUT + 0
class engine_post_ps20b_Dynamic_Index
{
private:
	int m_nAA_ENABLE;
#ifdef _DEBUG
	bool m_bAA_ENABLE;
#endif
public:
	void SetAA_ENABLE( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nAA_ENABLE = i;
#ifdef _DEBUG
		m_bAA_ENABLE = true;
#endif
	}
	void SetAA_ENABLE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nAA_ENABLE = i ? 1 : 0;
#ifdef _DEBUG
		m_bAA_ENABLE = true;
#endif
	}
private:
	int m_nCOL_CORRECT_NUM_LOOKUPS;
#ifdef _DEBUG
	bool m_bCOL_CORRECT_NUM_LOOKUPS;
#endif
public:
	void SetCOL_CORRECT_NUM_LOOKUPS( int i )
	{
		Assert( i >= 0 && i <= 3 );
		m_nCOL_CORRECT_NUM_LOOKUPS = i;
#ifdef _DEBUG
		m_bCOL_CORRECT_NUM_LOOKUPS = true;
#endif
	}
	void SetCOL_CORRECT_NUM_LOOKUPS( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 3 );
		m_nCOL_CORRECT_NUM_LOOKUPS = i ? 1 : 0;
#ifdef _DEBUG
		m_bCOL_CORRECT_NUM_LOOKUPS = true;
#endif
	}
private:
	int m_nCONVERT_FROM_LINEAR;
#ifdef _DEBUG
	bool m_bCONVERT_FROM_LINEAR;
#endif
public:
	void SetCONVERT_FROM_LINEAR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCONVERT_FROM_LINEAR = i;
#ifdef _DEBUG
		m_bCONVERT_FROM_LINEAR = true;
#endif
	}
	void SetCONVERT_FROM_LINEAR( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nCONVERT_FROM_LINEAR = i ? 1 : 0;
#ifdef _DEBUG
		m_bCONVERT_FROM_LINEAR = true;
#endif
	}
private:
	int m_nCONVERT_TO_LINEAR;
#ifdef _DEBUG
	bool m_bCONVERT_TO_LINEAR;
#endif
public:
	void SetCONVERT_TO_LINEAR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCONVERT_TO_LINEAR = i;
#ifdef _DEBUG
		m_bCONVERT_TO_LINEAR = true;
#endif
	}
	void SetCONVERT_TO_LINEAR( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nCONVERT_TO_LINEAR = i ? 1 : 0;
#ifdef _DEBUG
		m_bCONVERT_TO_LINEAR = true;
#endif
	}
private:
	int m_nFADE_TO_BLACK;
#ifdef _DEBUG
	bool m_bFADE_TO_BLACK;
#endif
public:
	void SetFADE_TO_BLACK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFADE_TO_BLACK = i;
#ifdef _DEBUG
		m_bFADE_TO_BLACK = true;
#endif
	}
	void SetFADE_TO_BLACK( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nFADE_TO_BLACK = i ? 1 : 0;
#ifdef _DEBUG
		m_bFADE_TO_BLACK = true;
#endif
	}
private:
	int m_nFADE_TYPE;
#ifdef _DEBUG
	bool m_bFADE_TYPE;
#endif
public:
	void SetFADE_TYPE( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nFADE_TYPE = i;
#ifdef _DEBUG
		m_bFADE_TYPE = true;
#endif
	}
	void SetFADE_TYPE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 2 );
		m_nFADE_TYPE = i ? 1 : 0;
#ifdef _DEBUG
		m_bFADE_TYPE = true;
#endif
	}
private:
	int m_nNOISE_ENABLE;
#ifdef _DEBUG
	bool m_bNOISE_ENABLE;
#endif
public:
	void SetNOISE_ENABLE( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nNOISE_ENABLE = i;
#ifdef _DEBUG
		m_bNOISE_ENABLE = true;
#endif
	}
	void SetNOISE_ENABLE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nNOISE_ENABLE = i ? 1 : 0;
#ifdef _DEBUG
		m_bNOISE_ENABLE = true;
#endif
	}
private:
	int m_nVIGNETTE_ENABLE;
#ifdef _DEBUG
	bool m_bVIGNETTE_ENABLE;
#endif
public:
	void SetVIGNETTE_ENABLE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nVIGNETTE_ENABLE = i;
#ifdef _DEBUG
		m_bVIGNETTE_ENABLE = true;
#endif
	}
	void SetVIGNETTE_ENABLE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nVIGNETTE_ENABLE = i ? 1 : 0;
#ifdef _DEBUG
		m_bVIGNETTE_ENABLE = true;
#endif
	}
private:
	int m_nLOCAL_CONTRAST_ENABLE;
#ifdef _DEBUG
	bool m_bLOCAL_CONTRAST_ENABLE;
#endif
public:
	void SetLOCAL_CONTRAST_ENABLE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nLOCAL_CONTRAST_ENABLE = i;
#ifdef _DEBUG
		m_bLOCAL_CONTRAST_ENABLE = true;
#endif
	}
	void SetLOCAL_CONTRAST_ENABLE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nLOCAL_CONTRAST_ENABLE = i ? 1 : 0;
#ifdef _DEBUG
		m_bLOCAL_CONTRAST_ENABLE = true;
#endif
	}
private:
	int m_nBLURRED_VIGNETTE_ENABLE;
#ifdef _DEBUG
	bool m_bBLURRED_VIGNETTE_ENABLE;
#endif
public:
	void SetBLURRED_VIGNETTE_ENABLE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBLURRED_VIGNETTE_ENABLE = i;
#ifdef _DEBUG
		m_bBLURRED_VIGNETTE_ENABLE = true;
#endif
	}
	void SetBLURRED_VIGNETTE_ENABLE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nBLURRED_VIGNETTE_ENABLE = i ? 1 : 0;
#ifdef _DEBUG
		m_bBLURRED_VIGNETTE_ENABLE = true;
#endif
	}
private:
	int m_nVOMIT_ENABLE;
#ifdef _DEBUG
	bool m_bVOMIT_ENABLE;
#endif
public:
	void SetVOMIT_ENABLE( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nVOMIT_ENABLE = i;
#ifdef _DEBUG
		m_bVOMIT_ENABLE = true;
#endif
	}
	void SetVOMIT_ENABLE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nVOMIT_ENABLE = i ? 1 : 0;
#ifdef _DEBUG
		m_bVOMIT_ENABLE = true;
#endif
	}
private:
	int m_nTV_GAMMA;
#ifdef _DEBUG
	bool m_bTV_GAMMA;
#endif
public:
	void SetTV_GAMMA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nTV_GAMMA = i;
#ifdef _DEBUG
		m_bTV_GAMMA = true;
#endif
	}
	void SetTV_GAMMA( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nTV_GAMMA = i ? 1 : 0;
#ifdef _DEBUG
		m_bTV_GAMMA = true;
#endif
	}
private:
	int m_nDESATURATEENABLE;
#ifdef _DEBUG
	bool m_bDESATURATEENABLE;
#endif
public:
	void SetDESATURATEENABLE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDESATURATEENABLE = i;
#ifdef _DEBUG
		m_bDESATURATEENABLE = true;
#endif
	}
	void SetDESATURATEENABLE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDESATURATEENABLE = i ? 1 : 0;
#ifdef _DEBUG
		m_bDESATURATEENABLE = true;
#endif
	}
public:
	// CONSTRUCTOR
	engine_post_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bAA_ENABLE = false;
#endif // _DEBUG
		m_nAA_ENABLE = 0;
#ifdef _DEBUG
		m_bCOL_CORRECT_NUM_LOOKUPS = false;
#endif // _DEBUG
		m_nCOL_CORRECT_NUM_LOOKUPS = 0;
#ifdef _DEBUG
		m_bCONVERT_FROM_LINEAR = false;
#endif // _DEBUG
		m_nCONVERT_FROM_LINEAR = 0;
#ifdef _DEBUG
		m_bCONVERT_TO_LINEAR = false;
#endif // _DEBUG
		m_nCONVERT_TO_LINEAR = 0;
#ifdef _DEBUG
		m_bFADE_TO_BLACK = false;
#endif // _DEBUG
		m_nFADE_TO_BLACK = 0;
#ifdef _DEBUG
		m_bFADE_TYPE = false;
#endif // _DEBUG
		m_nFADE_TYPE = 0;
#ifdef _DEBUG
		m_bNOISE_ENABLE = false;
#endif // _DEBUG
		m_nNOISE_ENABLE = 0;
#ifdef _DEBUG
		m_bVIGNETTE_ENABLE = false;
#endif // _DEBUG
		m_nVIGNETTE_ENABLE = 0;
#ifdef _DEBUG
		m_bLOCAL_CONTRAST_ENABLE = false;
#endif // _DEBUG
		m_nLOCAL_CONTRAST_ENABLE = 0;
#ifdef _DEBUG
		m_bBLURRED_VIGNETTE_ENABLE = false;
#endif // _DEBUG
		m_nBLURRED_VIGNETTE_ENABLE = 0;
#ifdef _DEBUG
		m_bVOMIT_ENABLE = false;
#endif // _DEBUG
		m_nVOMIT_ENABLE = 0;
#ifdef _DEBUG
		m_bTV_GAMMA = false;
#endif // _DEBUG
		m_nTV_GAMMA = 0;
#ifdef _DEBUG
		m_bDESATURATEENABLE = false;
#endif // _DEBUG
		m_nDESATURATEENABLE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bAA_ENABLE && m_bCOL_CORRECT_NUM_LOOKUPS && m_bCONVERT_FROM_LINEAR && m_bCONVERT_TO_LINEAR && m_bFADE_TO_BLACK && m_bFADE_TYPE && m_bNOISE_ENABLE && m_bVIGNETTE_ENABLE && m_bLOCAL_CONTRAST_ENABLE && m_bBLURRED_VIGNETTE_ENABLE && m_bVOMIT_ENABLE && m_bTV_GAMMA && m_bDESATURATEENABLE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nAA_ENABLE ) + ( 1 * m_nCOL_CORRECT_NUM_LOOKUPS ) + ( 4 * m_nCONVERT_FROM_LINEAR ) + ( 8 * m_nCONVERT_TO_LINEAR ) + ( 16 * m_nFADE_TO_BLACK ) + ( 32 * m_nFADE_TYPE ) + ( 96 * m_nNOISE_ENABLE ) + ( 96 * m_nVIGNETTE_ENABLE ) + ( 192 * m_nLOCAL_CONTRAST_ENABLE ) + ( 384 * m_nBLURRED_VIGNETTE_ENABLE ) + ( 768 * m_nVOMIT_ENABLE ) + ( 768 * m_nTV_GAMMA ) + ( 1536 * m_nDESATURATEENABLE ) + 0;
	}
};
#define shaderDynamicTest_engine_post_ps20b psh_forgot_to_set_dynamic_AA_ENABLE + psh_forgot_to_set_dynamic_COL_CORRECT_NUM_LOOKUPS + psh_forgot_to_set_dynamic_CONVERT_FROM_LINEAR + psh_forgot_to_set_dynamic_CONVERT_TO_LINEAR + psh_forgot_to_set_dynamic_FADE_TO_BLACK + psh_forgot_to_set_dynamic_FADE_TYPE + psh_forgot_to_set_dynamic_NOISE_ENABLE + psh_forgot_to_set_dynamic_VIGNETTE_ENABLE + psh_forgot_to_set_dynamic_LOCAL_CONTRAST_ENABLE + psh_forgot_to_set_dynamic_BLURRED_VIGNETTE_ENABLE + psh_forgot_to_set_dynamic_VOMIT_ENABLE + psh_forgot_to_set_dynamic_TV_GAMMA + psh_forgot_to_set_dynamic_DESATURATEENABLE + 0

static const ShaderComboInformation_t s_DynamicComboArray_Engine_Post_ps20b[13] = 
{
	{ "AA_ENABLE", 0, 0 },
	{ "COL_CORRECT_NUM_LOOKUPS", 0, 3 },
	{ "CONVERT_FROM_LINEAR", 0, 1 },
	{ "CONVERT_TO_LINEAR", 0, 1 },
	{ "FADE_TO_BLACK", 0, 1 },
	{ "FADE_TYPE", 0, 2 },
	{ "NOISE_ENABLE", 0, 0 },
	{ "VIGNETTE_ENABLE", 0, 1 },
	{ "LOCAL_CONTRAST_ENABLE", 0, 1 },
	{ "BLURRED_VIGNETTE_ENABLE", 0, 1 },
	{ "VOMIT_ENABLE", 0, 0 },
	{ "TV_GAMMA", 0, 1 },
	{ "DESATURATEENABLE", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_Engine_Post_ps20b[4] = 
{
	{ "TOOL_MODE", 0, 1 },
	{ "DEPTH_BLUR_ENABLE", 0, 1 },
	{ "LINEAR_INPUT", 0, 1 },
	{ "LINEAR_OUTPUT", 0, 1 },
};
static const ShaderComboSemantics_t Engine_Post_ps20b_combos =
{
	"Engine_Post_ps20b", s_DynamicComboArray_Engine_Post_ps20b, 13, s_StaticComboArray_Engine_Post_ps20b, 4 
};

class ConstructMe_Engine_Post_ps20b
{
public:
	ConstructMe_Engine_Post_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &Engine_Post_ps20b_combos );
	}
};

static ConstructMe_Engine_Post_ps20b s_ConstructMe_Engine_Post_ps20b;
