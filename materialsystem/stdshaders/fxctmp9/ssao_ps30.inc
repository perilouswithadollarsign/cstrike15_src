// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
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
class ssao_ps30_Static_Index
{
private:
	int m_nFINAL;
#ifdef _DEBUG
	bool m_bFINAL;
#endif
public:
	void SetFINAL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFINAL = i;
#ifdef _DEBUG
		m_bFINAL = true;
#endif
	}
	void SetFINAL( bool i )
	{
		m_nFINAL = i ? 1 : 0;
#ifdef _DEBUG
		m_bFINAL = true;
#endif
	}
private:
	int m_nFOW;
#ifdef _DEBUG
	bool m_bFOW;
#endif
public:
	void SetFOW( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFOW = i;
#ifdef _DEBUG
		m_bFOW = true;
#endif
	}
	void SetFOW( bool i )
	{
		m_nFOW = i ? 1 : 0;
#ifdef _DEBUG
		m_bFOW = true;
#endif
	}
public:
	// CONSTRUCTOR
	ssao_ps30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bFINAL = false;
#endif // _DEBUG
		m_nFINAL = 0;
#ifdef _DEBUG
		m_bFOW = false;
#endif // _DEBUG
		m_nFOW = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bFINAL && m_bFOW;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 4 * m_nFINAL ) + ( 8 * m_nFOW ) + 0;
	}
};
#define shaderStaticTest_ssao_ps30 psh_forgot_to_set_static_FINAL + psh_forgot_to_set_static_FOW + 0
class ssao_ps30_Dynamic_Index
{
private:
	int m_nDEFERRED;
#ifdef _DEBUG
	bool m_bDEFERRED;
#endif
public:
	void SetDEFERRED( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDEFERRED = i;
#ifdef _DEBUG
		m_bDEFERRED = true;
#endif
	}
	void SetDEFERRED( bool i )
	{
		m_nDEFERRED = i ? 1 : 0;
#ifdef _DEBUG
		m_bDEFERRED = true;
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
		m_nPIXELFOGTYPE = i ? 1 : 0;
#ifdef _DEBUG
		m_bPIXELFOGTYPE = true;
#endif
	}
public:
	// CONSTRUCTOR
	ssao_ps30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bDEFERRED = false;
#endif // _DEBUG
		m_nDEFERRED = 0;
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
		bool bAllDynamicVarsDefined = m_bDEFERRED && m_bPIXELFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nDEFERRED ) + ( 2 * m_nPIXELFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_ssao_ps30 psh_forgot_to_set_dynamic_DEFERRED + 0
