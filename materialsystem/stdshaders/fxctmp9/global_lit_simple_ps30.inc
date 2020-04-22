// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $FLASHLIGHT == 0 ) && ( $GLOBALLIGHT == 1 )
// ( $EDITOR == 1 ) && ( $FOW == 1 )
// ( $DEFERRED == 1 ) && ( $FOW == 1 )
// ( $DEFERRED == 1 ) && ( $GLOBALLIGHT == 1 )
// ( $DEFERRED == 1 ) && ( $FLASHLIGHT == 1 )
// ( $DEFERRED == 1 ) && ( $LIGHTING_PREVIEW == 1 )
// ( $DEFERRED == 1 ) && ( $EDITOR == 1 )
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
class global_lit_simple_ps30_Static_Index
{
private:
	int m_nEDITOR;
#ifdef _DEBUG
	bool m_bEDITOR;
#endif
public:
	void SetEDITOR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nEDITOR = i;
#ifdef _DEBUG
		m_bEDITOR = true;
#endif
	}
	void SetEDITOR( bool i )
	{
		m_nEDITOR = i ? 1 : 0;
#ifdef _DEBUG
		m_bEDITOR = true;
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
		m_nFLASHLIGHT = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLASHLIGHT = true;
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
		Assert( i >= 0 && i <= 2 );
		m_nFLASHLIGHTDEPTHFILTERMODE = i;
#ifdef _DEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = true;
#endif
	}
	void SetFLASHLIGHTDEPTHFILTERMODE( bool i )
	{
		m_nFLASHLIGHTDEPTHFILTERMODE = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = true;
#endif
	}
private:
	int m_nSPECULAR;
#ifdef _DEBUG
	bool m_bSPECULAR;
#endif
public:
	void SetSPECULAR( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSPECULAR = i;
#ifdef _DEBUG
		m_bSPECULAR = true;
#endif
	}
	void SetSPECULAR( bool i )
	{
		m_nSPECULAR = i ? 1 : 0;
#ifdef _DEBUG
		m_bSPECULAR = true;
#endif
	}
public:
	// CONSTRUCTOR
	global_lit_simple_ps30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bEDITOR = false;
#endif // _DEBUG
		m_nEDITOR = 0;
#ifdef _DEBUG
		m_bFLASHLIGHT = false;
#endif // _DEBUG
		m_nFLASHLIGHT = 0;
#ifdef _DEBUG
		m_bFLASHLIGHTDEPTHFILTERMODE = false;
#endif // _DEBUG
		m_nFLASHLIGHTDEPTHFILTERMODE = 0;
#ifdef _DEBUG
		m_bSPECULAR = false;
#endif // _DEBUG
		m_nSPECULAR = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bEDITOR && m_bFLASHLIGHT && m_bFLASHLIGHTDEPTHFILTERMODE && m_bSPECULAR;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 64 * m_nEDITOR ) + ( 128 * m_nFLASHLIGHT ) + ( 256 * m_nFLASHLIGHTDEPTHFILTERMODE ) + ( 768 * m_nSPECULAR ) + 0;
	}
};
#define shaderStaticTest_global_lit_simple_ps30 psh_forgot_to_set_static_EDITOR + psh_forgot_to_set_static_FLASHLIGHT + psh_forgot_to_set_static_FLASHLIGHTDEPTHFILTERMODE + psh_forgot_to_set_static_SPECULAR + 0
class global_lit_simple_ps30_Dynamic_Index
{
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
		m_nFLASHLIGHTSHADOWS = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLASHLIGHTSHADOWS = true;
#endif
	}
private:
	int m_nGLOBALLIGHT;
#ifdef _DEBUG
	bool m_bGLOBALLIGHT;
#endif
public:
	void SetGLOBALLIGHT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nGLOBALLIGHT = i;
#ifdef _DEBUG
		m_bGLOBALLIGHT = true;
#endif
	}
	void SetGLOBALLIGHT( bool i )
	{
		m_nGLOBALLIGHT = i ? 1 : 0;
#ifdef _DEBUG
		m_bGLOBALLIGHT = true;
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
		m_nWRITEWATERFOGTODESTALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bWRITEWATERFOGTODESTALPHA = true;
#endif
	}
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
	global_lit_simple_ps30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bFOW = false;
#endif // _DEBUG
		m_nFOW = 0;
#ifdef _DEBUG
		m_bFLASHLIGHTSHADOWS = false;
#endif // _DEBUG
		m_nFLASHLIGHTSHADOWS = 0;
#ifdef _DEBUG
		m_bGLOBALLIGHT = false;
#endif // _DEBUG
		m_nGLOBALLIGHT = 0;
#ifdef _DEBUG
		m_bWRITEWATERFOGTODESTALPHA = false;
#endif // _DEBUG
		m_nWRITEWATERFOGTODESTALPHA = 0;
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
		bool bAllDynamicVarsDefined = m_bFOW && m_bFLASHLIGHTSHADOWS && m_bGLOBALLIGHT && m_bWRITEWATERFOGTODESTALPHA && m_bDEFERRED && m_bPIXELFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nFOW ) + ( 2 * m_nFLASHLIGHTSHADOWS ) + ( 4 * m_nGLOBALLIGHT ) + ( 8 * m_nWRITEWATERFOGTODESTALPHA ) + ( 16 * m_nDEFERRED ) + ( 32 * m_nPIXELFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_global_lit_simple_ps30 psh_forgot_to_set_dynamic_FOW + psh_forgot_to_set_dynamic_FLASHLIGHTSHADOWS + psh_forgot_to_set_dynamic_GLOBALLIGHT + psh_forgot_to_set_dynamic_WRITEWATERFOGTODESTALPHA + psh_forgot_to_set_dynamic_DEFERRED + 0
