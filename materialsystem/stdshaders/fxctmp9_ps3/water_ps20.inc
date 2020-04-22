// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// $MULTITEXTURE && $BASETEXTURE
// $FLOWMAP && $MULTITEXTURE
// ( $FLASHLIGHT == 0 ) && ( $FLASHLIGHTSHADOWS == 1 )
// ( $FLASHLIGHT || $LIGHTMAPWATERFOG ) && ( ( $BASETEXTURE && !$FLOWMAP ) || $MULTITEXTURE )
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
class water_ps20_Static_Index
{
private:
	int m_nBASETEXTURE;
#ifdef _DEBUG
	bool m_bBASETEXTURE;
#endif
public:
	void SetBASETEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBASETEXTURE = i;
#ifdef _DEBUG
		m_bBASETEXTURE = true;
#endif
	}
	void SetBASETEXTURE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nBASETEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bBASETEXTURE = true;
#endif
	}
private:
	int m_nMULTITEXTURE;
#ifdef _DEBUG
	bool m_bMULTITEXTURE;
#endif
public:
	void SetMULTITEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMULTITEXTURE = i;
#ifdef _DEBUG
		m_bMULTITEXTURE = true;
#endif
	}
	void SetMULTITEXTURE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nMULTITEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bMULTITEXTURE = true;
#endif
	}
private:
	int m_nREFLECT;
#ifdef _DEBUG
	bool m_bREFLECT;
#endif
public:
	void SetREFLECT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nREFLECT = i;
#ifdef _DEBUG
		m_bREFLECT = true;
#endif
	}
	void SetREFLECT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nREFLECT = i ? 1 : 0;
#ifdef _DEBUG
		m_bREFLECT = true;
#endif
	}
private:
	int m_nREFRACT;
#ifdef _DEBUG
	bool m_bREFRACT;
#endif
public:
	void SetREFRACT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nREFRACT = i;
#ifdef _DEBUG
		m_bREFRACT = true;
#endif
	}
	void SetREFRACT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nREFRACT = i ? 1 : 0;
#ifdef _DEBUG
		m_bREFRACT = true;
#endif
	}
private:
	int m_nABOVEWATER;
#ifdef _DEBUG
	bool m_bABOVEWATER;
#endif
public:
	void SetABOVEWATER( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nABOVEWATER = i;
#ifdef _DEBUG
		m_bABOVEWATER = true;
#endif
	}
	void SetABOVEWATER( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nABOVEWATER = i ? 1 : 0;
#ifdef _DEBUG
		m_bABOVEWATER = true;
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
		Assert( i >= 0 && i <= 0 );
		m_nFLOWMAP = i;
#ifdef _DEBUG
		m_bFLOWMAP = true;
#endif
	}
	void SetFLOWMAP( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nFLOWMAP = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLOWMAP = true;
#endif
	}
private:
	int m_nFLOW_DEBUG;
#ifdef _DEBUG
	bool m_bFLOW_DEBUG;
#endif
public:
	void SetFLOW_DEBUG( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nFLOW_DEBUG = i;
#ifdef _DEBUG
		m_bFLOW_DEBUG = true;
#endif
	}
	void SetFLOW_DEBUG( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 2 );
		m_nFLOW_DEBUG = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLOW_DEBUG = true;
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
		Assert( i >= 0 && i <= 0 );
		m_nFLASHLIGHT = i;
#ifdef _DEBUG
		m_bFLASHLIGHT = true;
#endif
	}
	void SetFLASHLIGHT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nFLASHLIGHT = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLASHLIGHT = true;
#endif
	}
private:
	int m_nLIGHTMAPWATERFOG;
#ifdef _DEBUG
	bool m_bLIGHTMAPWATERFOG;
#endif
public:
	void SetLIGHTMAPWATERFOG( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nLIGHTMAPWATERFOG = i;
#ifdef _DEBUG
		m_bLIGHTMAPWATERFOG = true;
#endif
	}
	void SetLIGHTMAPWATERFOG( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nLIGHTMAPWATERFOG = i ? 1 : 0;
#ifdef _DEBUG
		m_bLIGHTMAPWATERFOG = true;
#endif
	}
private:
	int m_nFORCEFRESNEL;
#ifdef _DEBUG
	bool m_bFORCEFRESNEL;
#endif
public:
	void SetFORCEFRESNEL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFORCEFRESNEL = i;
#ifdef _DEBUG
		m_bFORCEFRESNEL = true;
#endif
	}
	void SetFORCEFRESNEL( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nFORCEFRESNEL = i ? 1 : 0;
#ifdef _DEBUG
		m_bFORCEFRESNEL = true;
#endif
	}
public:
	// CONSTRUCTOR
	water_ps20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bBASETEXTURE = false;
#endif // _DEBUG
		m_nBASETEXTURE = 0;
#ifdef _DEBUG
		m_bMULTITEXTURE = false;
#endif // _DEBUG
		m_nMULTITEXTURE = 0;
#ifdef _DEBUG
		m_bREFLECT = false;
#endif // _DEBUG
		m_nREFLECT = 0;
#ifdef _DEBUG
		m_bREFRACT = false;
#endif // _DEBUG
		m_nREFRACT = 0;
#ifdef _DEBUG
		m_bABOVEWATER = false;
#endif // _DEBUG
		m_nABOVEWATER = 0;
#ifdef _DEBUG
		m_bFLOWMAP = true;
#endif // _DEBUG
		m_nFLOWMAP = 0; ;
#ifdef _DEBUG
		m_bFLOW_DEBUG = false;
#endif // _DEBUG
		m_nFLOW_DEBUG = 0;
#ifdef _DEBUG
		m_bFLASHLIGHT = true;
#endif // _DEBUG
		m_nFLASHLIGHT = 0; ;
#ifdef _DEBUG
		m_bLIGHTMAPWATERFOG = true;
#endif // _DEBUG
		m_nLIGHTMAPWATERFOG = 0; ;
#ifdef _DEBUG
		m_bFORCEFRESNEL = false;
#endif // _DEBUG
		m_nFORCEFRESNEL = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bBASETEXTURE && m_bMULTITEXTURE && m_bREFLECT && m_bREFRACT && m_bABOVEWATER && m_bFLOWMAP && m_bFLOW_DEBUG && m_bFLASHLIGHT && m_bLIGHTMAPWATERFOG && m_bFORCEFRESNEL;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 4 * m_nBASETEXTURE ) + ( 8 * m_nMULTITEXTURE ) + ( 16 * m_nREFLECT ) + ( 32 * m_nREFRACT ) + ( 64 * m_nABOVEWATER ) + ( 128 * m_nFLOWMAP ) + ( 128 * m_nFLOW_DEBUG ) + ( 384 * m_nFLASHLIGHT ) + ( 384 * m_nLIGHTMAPWATERFOG ) + ( 384 * m_nFORCEFRESNEL ) + 0;
	}
};
#define shaderStaticTest_water_ps20 psh_forgot_to_set_static_BASETEXTURE + psh_forgot_to_set_static_MULTITEXTURE + psh_forgot_to_set_static_REFLECT + psh_forgot_to_set_static_REFRACT + psh_forgot_to_set_static_ABOVEWATER + psh_forgot_to_set_static_FLOW_DEBUG + psh_forgot_to_set_static_FORCEFRESNEL + 0
class water_ps20_Dynamic_Index
{
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
	int m_nBUILDWORLDIMPOSTER;
#ifdef _DEBUG
	bool m_bBUILDWORLDIMPOSTER;
#endif
public:
	void SetBUILDWORLDIMPOSTER( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBUILDWORLDIMPOSTER = i;
#ifdef _DEBUG
		m_bBUILDWORLDIMPOSTER = true;
#endif
	}
	void SetBUILDWORLDIMPOSTER( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nBUILDWORLDIMPOSTER = i ? 1 : 0;
#ifdef _DEBUG
		m_bBUILDWORLDIMPOSTER = true;
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
	water_ps20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bFLASHLIGHTSHADOWS = true;
#endif // _DEBUG
		m_nFLASHLIGHTSHADOWS = 0; ;
#ifdef _DEBUG
		m_bBUILDWORLDIMPOSTER = true;
#endif // _DEBUG
		m_nBUILDWORLDIMPOSTER = r_buildingmapforworld.GetBool() ? 1 : 0 ;
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
		bool bAllDynamicVarsDefined = m_bFLASHLIGHTSHADOWS && m_bBUILDWORLDIMPOSTER && m_bPIXELFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nFLASHLIGHTSHADOWS ) + ( 1 * m_nBUILDWORLDIMPOSTER ) + ( 2 * m_nPIXELFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_water_ps20 0

static const ShaderComboInformation_t s_DynamicComboArray_water_ps20[3] = 
{
	{ "FLASHLIGHTSHADOWS", 0, 0 },
	{ "BUILDWORLDIMPOSTER", 0, 1 },
	{ "PIXELFOGTYPE", 0, 1 },
};

static const ShaderComboInformation_t s_StaticComboArray_water_ps20[10] = 
{
	{ "BASETEXTURE", 0, 1 },
	{ "MULTITEXTURE", 0, 1 },
	{ "REFLECT", 0, 1 },
	{ "REFRACT", 0, 1 },
	{ "ABOVEWATER", 0, 1 },
	{ "FLOWMAP", 0, 0 },
	{ "FLOW_DEBUG", 0, 2 },
	{ "FLASHLIGHT", 0, 0 },
	{ "LIGHTMAPWATERFOG", 0, 0 },
	{ "FORCEFRESNEL", 0, 1 },
};
static const ShaderComboSemantics_t water_ps20_combos =
{
	"water_ps20", s_DynamicComboArray_water_ps20, 3, s_StaticComboArray_water_ps20, 10 
};

class ConstructMe_water_ps20
{
public:
	ConstructMe_water_ps20()
	{
		GetShaderDLL()->AddShaderComboInformation( &water_ps20_combos );
	}
};

static ConstructMe_water_ps20 s_ConstructMe_water_ps20;
