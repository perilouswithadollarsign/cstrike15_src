// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class ui_ps20b_Static_Index
{
private:
	int m_nGRAYSCALE;
#ifdef _DEBUG
	bool m_bGRAYSCALE;
#endif
public:
	void SetGRAYSCALE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nGRAYSCALE = i;
#ifdef _DEBUG
		m_bGRAYSCALE = true;
#endif
	}
	void SetGRAYSCALE( bool i )
	{
		m_nGRAYSCALE = i ? 1 : 0;
#ifdef _DEBUG
		m_bGRAYSCALE = true;
#endif
	}
private:
	int m_nCOLORWARP;
#ifdef _DEBUG
	bool m_bCOLORWARP;
#endif
public:
	void SetCOLORWARP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nCOLORWARP = i;
#ifdef _DEBUG
		m_bCOLORWARP = true;
#endif
	}
	void SetCOLORWARP( bool i )
	{
		m_nCOLORWARP = i ? 1 : 0;
#ifdef _DEBUG
		m_bCOLORWARP = true;
#endif
	}
public:
	// CONSTRUCTOR
	ui_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bGRAYSCALE = false;
#endif // _DEBUG
		m_nGRAYSCALE = 0;
#ifdef _DEBUG
		m_bCOLORWARP = false;
#endif // _DEBUG
		m_nCOLORWARP = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bGRAYSCALE && m_bCOLORWARP;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 2 * m_nGRAYSCALE ) + ( 4 * m_nCOLORWARP ) + 0;
	}
};
#define shaderStaticTest_ui_ps20b psh_forgot_to_set_static_GRAYSCALE + psh_forgot_to_set_static_COLORWARP + 0
class ui_ps20b_Dynamic_Index
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
		m_nPIXELFOGTYPE = i ? 1 : 0;
#ifdef _DEBUG
		m_bPIXELFOGTYPE = true;
#endif
	}
public:
	// CONSTRUCTOR
	ui_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
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
		bool bAllDynamicVarsDefined = m_bPIXELFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nPIXELFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_ui_ps20b 0
