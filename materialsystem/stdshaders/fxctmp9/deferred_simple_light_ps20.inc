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
class deferred_simple_light_ps20_Static_Index
{
public:
	// CONSTRUCTOR
	deferred_simple_light_ps20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
#endif // _DEBUG
		return 0;
	}
};
#define shaderStaticTest_deferred_simple_light_ps20 0
class deferred_simple_light_ps20_Dynamic_Index
{
private:
	int m_nSPOT;
#ifdef _DEBUG
	bool m_bSPOT;
#endif
public:
	void SetSPOT( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSPOT = i;
#ifdef _DEBUG
		m_bSPOT = true;
#endif
	}
	void SetSPOT( bool i )
	{
		m_nSPOT = i ? 1 : 0;
#ifdef _DEBUG
		m_bSPOT = true;
#endif
	}
private:
	int m_nPROJECTION;
#ifdef _DEBUG
	bool m_bPROJECTION;
#endif
public:
	void SetPROJECTION( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPROJECTION = i;
#ifdef _DEBUG
		m_bPROJECTION = true;
#endif
	}
	void SetPROJECTION( bool i )
	{
		m_nPROJECTION = i ? 1 : 0;
#ifdef _DEBUG
		m_bPROJECTION = true;
#endif
	}
private:
	int m_nROTATION;
#ifdef _DEBUG
	bool m_bROTATION;
#endif
public:
	void SetROTATION( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nROTATION = i;
#ifdef _DEBUG
		m_bROTATION = true;
#endif
	}
	void SetROTATION( bool i )
	{
		m_nROTATION = i ? 1 : 0;
#ifdef _DEBUG
		m_bROTATION = true;
#endif
	}
private:
	int m_nALPHA_FALLOFF;
#ifdef _DEBUG
	bool m_bALPHA_FALLOFF;
#endif
public:
	void SetALPHA_FALLOFF( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nALPHA_FALLOFF = i;
#ifdef _DEBUG
		m_bALPHA_FALLOFF = true;
#endif
	}
	void SetALPHA_FALLOFF( bool i )
	{
		m_nALPHA_FALLOFF = i ? 1 : 0;
#ifdef _DEBUG
		m_bALPHA_FALLOFF = true;
#endif
	}
private:
	int m_nDEBUG;
#ifdef _DEBUG
	bool m_bDEBUG;
#endif
public:
	void SetDEBUG( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nDEBUG = i;
#ifdef _DEBUG
		m_bDEBUG = true;
#endif
	}
	void SetDEBUG( bool i )
	{
		m_nDEBUG = i ? 1 : 0;
#ifdef _DEBUG
		m_bDEBUG = true;
#endif
	}
public:
	// CONSTRUCTOR
	deferred_simple_light_ps20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bSPOT = false;
#endif // _DEBUG
		m_nSPOT = 0;
#ifdef _DEBUG
		m_bPROJECTION = false;
#endif // _DEBUG
		m_nPROJECTION = 0;
#ifdef _DEBUG
		m_bROTATION = false;
#endif // _DEBUG
		m_nROTATION = 0;
#ifdef _DEBUG
		m_bALPHA_FALLOFF = false;
#endif // _DEBUG
		m_nALPHA_FALLOFF = 0;
#ifdef _DEBUG
		m_bDEBUG = false;
#endif // _DEBUG
		m_nDEBUG = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bSPOT && m_bPROJECTION && m_bROTATION && m_bALPHA_FALLOFF && m_bDEBUG;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nSPOT ) + ( 2 * m_nPROJECTION ) + ( 4 * m_nROTATION ) + ( 8 * m_nALPHA_FALLOFF ) + ( 16 * m_nDEBUG ) + 0;
	}
};
#define shaderDynamicTest_deferred_simple_light_ps20 psh_forgot_to_set_dynamic_SPOT + psh_forgot_to_set_dynamic_PROJECTION + psh_forgot_to_set_dynamic_ROTATION + psh_forgot_to_set_dynamic_ALPHA_FALLOFF + psh_forgot_to_set_dynamic_DEBUG + 0
