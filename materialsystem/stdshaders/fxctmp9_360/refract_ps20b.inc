// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// $MASKED && $BLUR
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
class refract_ps20b_Static_Index
{
private:
	int m_nMAGNIFY;
#ifdef _DEBUG
	bool m_bMAGNIFY;
#endif
public:
	void SetMAGNIFY( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMAGNIFY = i;
#ifdef _DEBUG
		m_bMAGNIFY = true;
#endif
	}
	void SetMAGNIFY( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nMAGNIFY = i ? 1 : 0;
#ifdef _DEBUG
		m_bMAGNIFY = true;
#endif
	}
private:
	int m_nBLUR;
#ifdef _DEBUG
	bool m_bBLUR;
#endif
public:
	void SetBLUR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBLUR = i;
#ifdef _DEBUG
		m_bBLUR = true;
#endif
	}
	void SetBLUR( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nBLUR = i ? 1 : 0;
#ifdef _DEBUG
		m_bBLUR = true;
#endif
	}
private:
	int m_nFADEOUTONSILHOUETTE;
#ifdef _DEBUG
	bool m_bFADEOUTONSILHOUETTE;
#endif
public:
	void SetFADEOUTONSILHOUETTE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFADEOUTONSILHOUETTE = i;
#ifdef _DEBUG
		m_bFADEOUTONSILHOUETTE = true;
#endif
	}
	void SetFADEOUTONSILHOUETTE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nFADEOUTONSILHOUETTE = i ? 1 : 0;
#ifdef _DEBUG
		m_bFADEOUTONSILHOUETTE = true;
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
	int m_nREFRACTTINTTEXTURE;
#ifdef _DEBUG
	bool m_bREFRACTTINTTEXTURE;
#endif
public:
	void SetREFRACTTINTTEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nREFRACTTINTTEXTURE = i;
#ifdef _DEBUG
		m_bREFRACTTINTTEXTURE = true;
#endif
	}
	void SetREFRACTTINTTEXTURE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nREFRACTTINTTEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bREFRACTTINTTEXTURE = true;
#endif
	}
private:
	int m_nMASKED;
#ifdef _DEBUG
	bool m_bMASKED;
#endif
public:
	void SetMASKED( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMASKED = i;
#ifdef _DEBUG
		m_bMASKED = true;
#endif
	}
	void SetMASKED( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nMASKED = i ? 1 : 0;
#ifdef _DEBUG
		m_bMASKED = true;
#endif
	}
private:
	int m_nCOLORMODULATE;
#ifdef _DEBUG
	bool m_bCOLORMODULATE;
#endif
public:
	void SetCOLORMODULATE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCOLORMODULATE = i;
#ifdef _DEBUG
		m_bCOLORMODULATE = true;
#endif
	}
	void SetCOLORMODULATE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nCOLORMODULATE = i ? 1 : 0;
#ifdef _DEBUG
		m_bCOLORMODULATE = true;
#endif
	}
private:
	int m_nSECONDARY_NORMAL;
#ifdef _DEBUG
	bool m_bSECONDARY_NORMAL;
#endif
public:
	void SetSECONDARY_NORMAL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSECONDARY_NORMAL = i;
#ifdef _DEBUG
		m_bSECONDARY_NORMAL = true;
#endif
	}
	void SetSECONDARY_NORMAL( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nSECONDARY_NORMAL = i ? 1 : 0;
#ifdef _DEBUG
		m_bSECONDARY_NORMAL = true;
#endif
	}
private:
	int m_nMIRRORABOUTVIEWPORTEDGES;
#ifdef _DEBUG
	bool m_bMIRRORABOUTVIEWPORTEDGES;
#endif
public:
	void SetMIRRORABOUTVIEWPORTEDGES( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMIRRORABOUTVIEWPORTEDGES = i;
#ifdef _DEBUG
		m_bMIRRORABOUTVIEWPORTEDGES = true;
#endif
	}
	void SetMIRRORABOUTVIEWPORTEDGES( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nMIRRORABOUTVIEWPORTEDGES = i ? 1 : 0;
#ifdef _DEBUG
		m_bMIRRORABOUTVIEWPORTEDGES = true;
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
		Assert( i >= 0 && i <= 1 );
		m_nSHADER_SRGB_READ = i;
#ifdef _DEBUG
		m_bSHADER_SRGB_READ = true;
#endif
	}
	void SetSHADER_SRGB_READ( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nSHADER_SRGB_READ = i ? 1 : 0;
#ifdef _DEBUG
		m_bSHADER_SRGB_READ = true;
#endif
	}
private:
	int m_nLOCALREFRACT;
#ifdef _DEBUG
	bool m_bLOCALREFRACT;
#endif
public:
	void SetLOCALREFRACT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nLOCALREFRACT = i;
#ifdef _DEBUG
		m_bLOCALREFRACT = true;
#endif
	}
	void SetLOCALREFRACT( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nLOCALREFRACT = i ? 1 : 0;
#ifdef _DEBUG
		m_bLOCALREFRACT = true;
#endif
	}
public:
	// CONSTRUCTOR
	refract_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bMAGNIFY = false;
#endif // _DEBUG
		m_nMAGNIFY = 0;
#ifdef _DEBUG
		m_bBLUR = false;
#endif // _DEBUG
		m_nBLUR = 0;
#ifdef _DEBUG
		m_bFADEOUTONSILHOUETTE = false;
#endif // _DEBUG
		m_nFADEOUTONSILHOUETTE = 0;
#ifdef _DEBUG
		m_bCUBEMAP = false;
#endif // _DEBUG
		m_nCUBEMAP = 0;
#ifdef _DEBUG
		m_bREFRACTTINTTEXTURE = false;
#endif // _DEBUG
		m_nREFRACTTINTTEXTURE = 0;
#ifdef _DEBUG
		m_bMASKED = false;
#endif // _DEBUG
		m_nMASKED = 0;
#ifdef _DEBUG
		m_bCOLORMODULATE = false;
#endif // _DEBUG
		m_nCOLORMODULATE = 0;
#ifdef _DEBUG
		m_bSECONDARY_NORMAL = false;
#endif // _DEBUG
		m_nSECONDARY_NORMAL = 0;
#ifdef _DEBUG
		m_bMIRRORABOUTVIEWPORTEDGES = false;
#endif // _DEBUG
		m_nMIRRORABOUTVIEWPORTEDGES = 0;
#ifdef _DEBUG
		m_bSHADER_SRGB_READ = false;
#endif // _DEBUG
		m_nSHADER_SRGB_READ = 0;
#ifdef _DEBUG
		m_bLOCALREFRACT = false;
#endif // _DEBUG
		m_nLOCALREFRACT = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bMAGNIFY && m_bBLUR && m_bFADEOUTONSILHOUETTE && m_bCUBEMAP && m_bREFRACTTINTTEXTURE && m_bMASKED && m_bCOLORMODULATE && m_bSECONDARY_NORMAL && m_bMIRRORABOUTVIEWPORTEDGES && m_bSHADER_SRGB_READ && m_bLOCALREFRACT;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 2 * m_nMAGNIFY ) + ( 4 * m_nBLUR ) + ( 8 * m_nFADEOUTONSILHOUETTE ) + ( 16 * m_nCUBEMAP ) + ( 32 * m_nREFRACTTINTTEXTURE ) + ( 64 * m_nMASKED ) + ( 128 * m_nCOLORMODULATE ) + ( 256 * m_nSECONDARY_NORMAL ) + ( 512 * m_nMIRRORABOUTVIEWPORTEDGES ) + ( 1024 * m_nSHADER_SRGB_READ ) + ( 2048 * m_nLOCALREFRACT ) + 0;
	}
};
#define shaderStaticTest_refract_ps20b psh_forgot_to_set_static_MAGNIFY + psh_forgot_to_set_static_BLUR + psh_forgot_to_set_static_FADEOUTONSILHOUETTE + psh_forgot_to_set_static_CUBEMAP + psh_forgot_to_set_static_REFRACTTINTTEXTURE + psh_forgot_to_set_static_MASKED + psh_forgot_to_set_static_COLORMODULATE + psh_forgot_to_set_static_SECONDARY_NORMAL + psh_forgot_to_set_static_MIRRORABOUTVIEWPORTEDGES + psh_forgot_to_set_static_SHADER_SRGB_READ + psh_forgot_to_set_static_LOCALREFRACT + 0
class refract_ps20b_Dynamic_Index
{
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
	int m_nD_NVIDIA_STEREO;
#ifdef _DEBUG
	bool m_bD_NVIDIA_STEREO;
#endif
public:
	void SetD_NVIDIA_STEREO( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nD_NVIDIA_STEREO = i;
#ifdef _DEBUG
		m_bD_NVIDIA_STEREO = true;
#endif
	}
	void SetD_NVIDIA_STEREO( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nD_NVIDIA_STEREO = i ? 1 : 0;
#ifdef _DEBUG
		m_bD_NVIDIA_STEREO = true;
#endif
	}
public:
	// CONSTRUCTOR
	refract_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bPIXELFOGTYPE = true;
#endif // _DEBUG
		m_nPIXELFOGTYPE = ( pShaderAPI->GetSceneFogMode() == MATERIAL_FOG_LINEAR_BELOW_FOG_Z ) ;
#ifdef _DEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = false;
#endif // _DEBUG
		m_nWRITE_DEPTH_TO_DESTALPHA = 0;
#ifdef _DEBUG
		m_bD_NVIDIA_STEREO = false;
#endif // _DEBUG
		m_nD_NVIDIA_STEREO = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bPIXELFOGTYPE && m_bWRITE_DEPTH_TO_DESTALPHA && m_bD_NVIDIA_STEREO;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nPIXELFOGTYPE ) + ( 2 * m_nWRITE_DEPTH_TO_DESTALPHA ) + ( 2 * m_nD_NVIDIA_STEREO ) + 0;
	}
};
#define shaderDynamicTest_refract_ps20b psh_forgot_to_set_dynamic_WRITE_DEPTH_TO_DESTALPHA + psh_forgot_to_set_dynamic_D_NVIDIA_STEREO + 0

static const ShaderComboInformation_t s_DynamicComboArray_refract_ps20b[3] = 
{
	{ "PIXELFOGTYPE", 0, 1 },
	{ "WRITE_DEPTH_TO_DESTALPHA", 0, 0 },
	{ "D_NVIDIA_STEREO", 0, 0 },
};

static const ShaderComboInformation_t s_StaticComboArray_refract_ps20b[11] = 
{
	{ "MAGNIFY", 0, 1 },
	{ "BLUR", 0, 1 },
	{ "FADEOUTONSILHOUETTE", 0, 1 },
	{ "CUBEMAP", 0, 1 },
	{ "REFRACTTINTTEXTURE", 0, 1 },
	{ "MASKED", 0, 1 },
	{ "COLORMODULATE", 0, 1 },
	{ "SECONDARY_NORMAL", 0, 1 },
	{ "MIRRORABOUTVIEWPORTEDGES", 0, 1 },
	{ "SHADER_SRGB_READ", 0, 1 },
	{ "LOCALREFRACT", 0, 1 },
};
static const ShaderComboSemantics_t refract_ps20b_combos =
{
	"refract_ps20b", s_DynamicComboArray_refract_ps20b, 3, s_StaticComboArray_refract_ps20b, 11 
};

class ConstructMe_refract_ps20b
{
public:
	ConstructMe_refract_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &refract_ps20b_combos );
	}
};

static ConstructMe_refract_ps20b s_ConstructMe_refract_ps20b;
