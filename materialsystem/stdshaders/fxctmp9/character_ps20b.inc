// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $AMBIENTREFLECTION == 0 ) && ( $USEBOUNCECOLOR == 1 )
// ( $BASEALPHAENVMASK == 1 ) && ( $ENVMAP == 0 )
// ( $BUMPALPHAENVMASK == 1 ) && ( $ENVMAP == 0 )
// ( $BASEALPHAENVMASK == 1 ) && ( $BUMPALPHAENVMASK == 1 )
// ( $BASEALPHASELFILLUMMASK == 1) && ( $BASEALPHAENVMASK == 1 )
// ( $BASEALPHASELFILLUMMASK == 1) && ( $BASEALPHAPHONGMASK == 1 )
// ( $BASEALPHAENVMASK == 1 ) && ( $BASEALPHASELFILLUMMASK )
// ( $CASCADED_SHADOW_MAPPING == 0 ) && ( $DYN_CSM_ENABLED == 1 )
// ( $CASCADED_SHADOW_MAPPING == 0 ) && ( $CSM_MODE != 0 ) 
// ( $AMBIENTREFLECTION == 0 ) && ( $USEBOUNCECOLOR == 1 )
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
class character_ps20b_Static_Index
{
private:
	int m_nMASKS1;
#ifdef _DEBUG
	bool m_bMASKS1;
#endif
public:
	void SetMASKS1( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMASKS1 = i;
#ifdef _DEBUG
		m_bMASKS1 = true;
#endif
	}
	void SetMASKS1( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nMASKS1 = i ? 1 : 0;
#ifdef _DEBUG
		m_bMASKS1 = true;
#endif
	}
private:
	int m_nMASKS2;
#ifdef _DEBUG
	bool m_bMASKS2;
#endif
public:
	void SetMASKS2( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMASKS2 = i;
#ifdef _DEBUG
		m_bMASKS2 = true;
#endif
	}
	void SetMASKS2( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nMASKS2 = i ? 1 : 0;
#ifdef _DEBUG
		m_bMASKS2 = true;
#endif
	}
private:
	int m_nFRESNELRANGESTEXTURE;
#ifdef _DEBUG
	bool m_bFRESNELRANGESTEXTURE;
#endif
public:
	void SetFRESNELRANGESTEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFRESNELRANGESTEXTURE = i;
#ifdef _DEBUG
		m_bFRESNELRANGESTEXTURE = true;
#endif
	}
	void SetFRESNELRANGESTEXTURE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nFRESNELRANGESTEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bFRESNELRANGESTEXTURE = true;
#endif
	}
private:
	int m_nENVMAP;
#ifdef _DEBUG
	bool m_bENVMAP;
#endif
public:
	void SetENVMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nENVMAP = i;
#ifdef _DEBUG
		m_bENVMAP = true;
#endif
	}
	void SetENVMAP( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nENVMAP = i ? 1 : 0;
#ifdef _DEBUG
		m_bENVMAP = true;
#endif
	}
private:
	int m_nAMBIENTREFLECTION;
#ifdef _DEBUG
	bool m_bAMBIENTREFLECTION;
#endif
public:
	void SetAMBIENTREFLECTION( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nAMBIENTREFLECTION = i;
#ifdef _DEBUG
		m_bAMBIENTREFLECTION = true;
#endif
	}
	void SetAMBIENTREFLECTION( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nAMBIENTREFLECTION = i ? 1 : 0;
#ifdef _DEBUG
		m_bAMBIENTREFLECTION = true;
#endif
	}
private:
	int m_nBASEALPHAENVMASK;
#ifdef _DEBUG
	bool m_bBASEALPHAENVMASK;
#endif
public:
	void SetBASEALPHAENVMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBASEALPHAENVMASK = i;
#ifdef _DEBUG
		m_bBASEALPHAENVMASK = true;
#endif
	}
	void SetBASEALPHAENVMASK( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nBASEALPHAENVMASK = i ? 1 : 0;
#ifdef _DEBUG
		m_bBASEALPHAENVMASK = true;
#endif
	}
private:
	int m_nBUMPALPHAENVMASK;
#ifdef _DEBUG
	bool m_bBUMPALPHAENVMASK;
#endif
public:
	void SetBUMPALPHAENVMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBUMPALPHAENVMASK = i;
#ifdef _DEBUG
		m_bBUMPALPHAENVMASK = true;
#endif
	}
	void SetBUMPALPHAENVMASK( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nBUMPALPHAENVMASK = i ? 1 : 0;
#ifdef _DEBUG
		m_bBUMPALPHAENVMASK = true;
#endif
	}
private:
	int m_nBASEALPHASELFILLUMMASK;
#ifdef _DEBUG
	bool m_bBASEALPHASELFILLUMMASK;
#endif
public:
	void SetBASEALPHASELFILLUMMASK( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBASEALPHASELFILLUMMASK = i;
#ifdef _DEBUG
		m_bBASEALPHASELFILLUMMASK = true;
#endif
	}
	void SetBASEALPHASELFILLUMMASK( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nBASEALPHASELFILLUMMASK = i ? 1 : 0;
#ifdef _DEBUG
		m_bBASEALPHASELFILLUMMASK = true;
#endif
	}
private:
	int m_nFAKERIM;
#ifdef _DEBUG
	bool m_bFAKERIM;
#endif
public:
	void SetFAKERIM( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFAKERIM = i;
#ifdef _DEBUG
		m_bFAKERIM = true;
#endif
	}
	void SetFAKERIM( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nFAKERIM = i ? 1 : 0;
#ifdef _DEBUG
		m_bFAKERIM = true;
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
public:
	// CONSTRUCTOR
	character_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bMASKS1 = false;
#endif // _DEBUG
		m_nMASKS1 = 0;
#ifdef _DEBUG
		m_bMASKS2 = false;
#endif // _DEBUG
		m_nMASKS2 = 0;
#ifdef _DEBUG
		m_bFRESNELRANGESTEXTURE = false;
#endif // _DEBUG
		m_nFRESNELRANGESTEXTURE = 0;
#ifdef _DEBUG
		m_bENVMAP = false;
#endif // _DEBUG
		m_nENVMAP = 0;
#ifdef _DEBUG
		m_bAMBIENTREFLECTION = false;
#endif // _DEBUG
		m_nAMBIENTREFLECTION = 0;
#ifdef _DEBUG
		m_bBASEALPHAENVMASK = false;
#endif // _DEBUG
		m_nBASEALPHAENVMASK = 0;
#ifdef _DEBUG
		m_bBUMPALPHAENVMASK = false;
#endif // _DEBUG
		m_nBUMPALPHAENVMASK = 0;
#ifdef _DEBUG
		m_bBASEALPHASELFILLUMMASK = false;
#endif // _DEBUG
		m_nBASEALPHASELFILLUMMASK = 0;
#ifdef _DEBUG
		m_bFAKERIM = false;
#endif // _DEBUG
		m_nFAKERIM = 0;
#ifdef _DEBUG
		m_bCASCADED_SHADOW_MAPPING = false;
#endif // _DEBUG
		m_nCASCADED_SHADOW_MAPPING = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bMASKS1 && m_bMASKS2 && m_bFRESNELRANGESTEXTURE && m_bENVMAP && m_bAMBIENTREFLECTION && m_bBASEALPHAENVMASK && m_bBUMPALPHAENVMASK && m_bBASEALPHASELFILLUMMASK && m_bFAKERIM && m_bCASCADED_SHADOW_MAPPING;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 80 * m_nMASKS1 ) + ( 160 * m_nMASKS2 ) + ( 320 * m_nFRESNELRANGESTEXTURE ) + ( 640 * m_nENVMAP ) + ( 1280 * m_nAMBIENTREFLECTION ) + ( 2560 * m_nBASEALPHAENVMASK ) + ( 5120 * m_nBUMPALPHAENVMASK ) + ( 10240 * m_nBASEALPHASELFILLUMMASK ) + ( 20480 * m_nFAKERIM ) + ( 40960 * m_nCASCADED_SHADOW_MAPPING ) + 0;
	}
};
#define shaderStaticTest_character_ps20b psh_forgot_to_set_static_MASKS1 + psh_forgot_to_set_static_MASKS2 + psh_forgot_to_set_static_FRESNELRANGESTEXTURE + psh_forgot_to_set_static_ENVMAP + psh_forgot_to_set_static_AMBIENTREFLECTION + psh_forgot_to_set_static_BASEALPHAENVMASK + psh_forgot_to_set_static_BUMPALPHAENVMASK + psh_forgot_to_set_static_BASEALPHASELFILLUMMASK + psh_forgot_to_set_static_FAKERIM + psh_forgot_to_set_static_CASCADED_SHADOW_MAPPING + 0
class character_ps20b_Dynamic_Index
{
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
	int m_nDYN_CSM_ENABLED;
#ifdef _DEBUG
	bool m_bDYN_CSM_ENABLED;
#endif
public:
	void SetDYN_CSM_ENABLED( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDYN_CSM_ENABLED = i;
#ifdef _DEBUG
		m_bDYN_CSM_ENABLED = true;
#endif
	}
	void SetDYN_CSM_ENABLED( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nDYN_CSM_ENABLED = i ? 1 : 0;
#ifdef _DEBUG
		m_bDYN_CSM_ENABLED = true;
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
public:
	// CONSTRUCTOR
	character_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bNUM_LIGHTS = false;
#endif // _DEBUG
		m_nNUM_LIGHTS = 0;
#ifdef _DEBUG
		m_bDYN_CSM_ENABLED = false;
#endif // _DEBUG
		m_nDYN_CSM_ENABLED = 0;
#ifdef _DEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = false;
#endif // _DEBUG
		m_nWRITE_DEPTH_TO_DESTALPHA = 0;
#ifdef _DEBUG
		m_bWRITEWATERFOGTODESTALPHA = false;
#endif // _DEBUG
		m_nWRITEWATERFOGTODESTALPHA = 0;
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
		bool bAllDynamicVarsDefined = m_bNUM_LIGHTS && m_bDYN_CSM_ENABLED && m_bWRITE_DEPTH_TO_DESTALPHA && m_bWRITEWATERFOGTODESTALPHA && m_bPIXELFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nNUM_LIGHTS ) + ( 5 * m_nDYN_CSM_ENABLED ) + ( 10 * m_nWRITE_DEPTH_TO_DESTALPHA ) + ( 20 * m_nWRITEWATERFOGTODESTALPHA ) + ( 40 * m_nPIXELFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_character_ps20b psh_forgot_to_set_dynamic_NUM_LIGHTS + psh_forgot_to_set_dynamic_DYN_CSM_ENABLED + psh_forgot_to_set_dynamic_WRITE_DEPTH_TO_DESTALPHA + psh_forgot_to_set_dynamic_WRITEWATERFOGTODESTALPHA + 0

static const ShaderComboInformation_t s_DynamicComboArray_character_ps20b[5] = 
{
	{ "NUM_LIGHTS", 0, 4 },
	{ "DYN_CSM_ENABLED", 0, 1 },
	{ "WRITE_DEPTH_TO_DESTALPHA", 0, 1 },
	{ "WRITEWATERFOGTODESTALPHA", 0, 1 },
	{ "PIXELFOGTYPE", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_character_ps20b[10] = 
{
	{ "MASKS1", 0, 1 },
	{ "MASKS2", 0, 1 },
	{ "FRESNELRANGESTEXTURE", 0, 1 },
	{ "ENVMAP", 0, 1 },
	{ "AMBIENTREFLECTION", 0, 1 },
	{ "BASEALPHAENVMASK", 0, 1 },
	{ "BUMPALPHAENVMASK", 0, 1 },
	{ "BASEALPHASELFILLUMMASK", 0, 1 },
	{ "FAKERIM", 0, 1 },
	{ "CASCADED_SHADOW_MAPPING", 0, 1 },
};
static const ShaderComboSemantics_t character_ps20b_combos =
{
	"character_ps20b", s_DynamicComboArray_character_ps20b, 5, s_StaticComboArray_character_ps20b, 10 
};

class ConstructMe_character_ps20b
{
public:
	ConstructMe_character_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &character_ps20b_combos );
	}
};

static ConstructMe_character_ps20b s_ConstructMe_character_ps20b;
