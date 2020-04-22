// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class splinecard_vs20_Static_Index
{
private:
	int m_nADDBASETEXTURE2;
#ifdef _DEBUG
	bool m_bADDBASETEXTURE2;
#endif
public:
	void SetADDBASETEXTURE2( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nADDBASETEXTURE2 = i;
#ifdef _DEBUG
		m_bADDBASETEXTURE2 = true;
#endif
	}
	void SetADDBASETEXTURE2( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nADDBASETEXTURE2 = i ? 1 : 0;
#ifdef _DEBUG
		m_bADDBASETEXTURE2 = true;
#endif
	}
private:
	int m_nEXTRACTGREENALPHA;
#ifdef _DEBUG
	bool m_bEXTRACTGREENALPHA;
#endif
public:
	void SetEXTRACTGREENALPHA( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nEXTRACTGREENALPHA = i;
#ifdef _DEBUG
		m_bEXTRACTGREENALPHA = true;
#endif
	}
	void SetEXTRACTGREENALPHA( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nEXTRACTGREENALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bEXTRACTGREENALPHA = true;
#endif
	}
private:
	int m_nDUALSEQUENCE;
#ifdef _DEBUG
	bool m_bDUALSEQUENCE;
#endif
public:
	void SetDUALSEQUENCE( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nDUALSEQUENCE = i;
#ifdef _DEBUG
		m_bDUALSEQUENCE = true;
#endif
	}
	void SetDUALSEQUENCE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nDUALSEQUENCE = i ? 1 : 0;
#ifdef _DEBUG
		m_bDUALSEQUENCE = true;
#endif
	}
private:
	int m_nDEPTHBLEND;
#ifdef _DEBUG
	bool m_bDEPTHBLEND;
#endif
public:
	void SetDEPTHBLEND( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nDEPTHBLEND = i;
#ifdef _DEBUG
		m_bDEPTHBLEND = true;
#endif
	}
	void SetDEPTHBLEND( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nDEPTHBLEND = i ? 1 : 0;
#ifdef _DEBUG
		m_bDEPTHBLEND = true;
#endif
	}
private:
	int m_nPACKED_INTERPOLATOR;
#ifdef _DEBUG
	bool m_bPACKED_INTERPOLATOR;
#endif
public:
	void SetPACKED_INTERPOLATOR( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nPACKED_INTERPOLATOR = i;
#ifdef _DEBUG
		m_bPACKED_INTERPOLATOR = true;
#endif
	}
	void SetPACKED_INTERPOLATOR( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nPACKED_INTERPOLATOR = i ? 1 : 0;
#ifdef _DEBUG
		m_bPACKED_INTERPOLATOR = true;
#endif
	}
private:
	int m_nANIMBLEND_OR_MAXLUMFRAMEBLEND1;
#ifdef _DEBUG
	bool m_bANIMBLEND_OR_MAXLUMFRAMEBLEND1;
#endif
public:
	void SetANIMBLEND_OR_MAXLUMFRAMEBLEND1( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nANIMBLEND_OR_MAXLUMFRAMEBLEND1 = i;
#ifdef _DEBUG
		m_bANIMBLEND_OR_MAXLUMFRAMEBLEND1 = true;
#endif
	}
	void SetANIMBLEND_OR_MAXLUMFRAMEBLEND1( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nANIMBLEND_OR_MAXLUMFRAMEBLEND1 = i ? 1 : 0;
#ifdef _DEBUG
		m_bANIMBLEND_OR_MAXLUMFRAMEBLEND1 = true;
#endif
	}
public:
	// CONSTRUCTOR
	splinecard_vs20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bADDBASETEXTURE2 = false;
#endif // _DEBUG
		m_nADDBASETEXTURE2 = 0;
#ifdef _DEBUG
		m_bEXTRACTGREENALPHA = false;
#endif // _DEBUG
		m_nEXTRACTGREENALPHA = 0;
#ifdef _DEBUG
		m_bDUALSEQUENCE = false;
#endif // _DEBUG
		m_nDUALSEQUENCE = 0;
#ifdef _DEBUG
		m_bDEPTHBLEND = false;
#endif // _DEBUG
		m_nDEPTHBLEND = 0;
#ifdef _DEBUG
		m_bPACKED_INTERPOLATOR = false;
#endif // _DEBUG
		m_nPACKED_INTERPOLATOR = 0;
#ifdef _DEBUG
		m_bANIMBLEND_OR_MAXLUMFRAMEBLEND1 = false;
#endif // _DEBUG
		m_nANIMBLEND_OR_MAXLUMFRAMEBLEND1 = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bADDBASETEXTURE2 && m_bEXTRACTGREENALPHA && m_bDUALSEQUENCE && m_bDEPTHBLEND && m_bPACKED_INTERPOLATOR && m_bANIMBLEND_OR_MAXLUMFRAMEBLEND1;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 4 * m_nADDBASETEXTURE2 ) + ( 4 * m_nEXTRACTGREENALPHA ) + ( 4 * m_nDUALSEQUENCE ) + ( 4 * m_nDEPTHBLEND ) + ( 4 * m_nPACKED_INTERPOLATOR ) + ( 4 * m_nANIMBLEND_OR_MAXLUMFRAMEBLEND1 ) + 0;
	}
};
#define shaderStaticTest_splinecard_vs20 vsh_forgot_to_set_static_ADDBASETEXTURE2 + vsh_forgot_to_set_static_EXTRACTGREENALPHA + vsh_forgot_to_set_static_DUALSEQUENCE + vsh_forgot_to_set_static_DEPTHBLEND + vsh_forgot_to_set_static_PACKED_INTERPOLATOR + vsh_forgot_to_set_static_ANIMBLEND_OR_MAXLUMFRAMEBLEND1 + 0
class splinecard_vs20_Dynamic_Index
{
private:
	int m_nORIENTATION;
#ifdef _DEBUG
	bool m_bORIENTATION;
#endif
public:
	void SetORIENTATION( int i )
	{
		Assert( i >= 0 && i <= 3 );
		m_nORIENTATION = i;
#ifdef _DEBUG
		m_bORIENTATION = true;
#endif
	}
	void SetORIENTATION( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 3 );
		m_nORIENTATION = i ? 1 : 0;
#ifdef _DEBUG
		m_bORIENTATION = true;
#endif
	}
public:
	// CONSTRUCTOR
	splinecard_vs20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bORIENTATION = false;
#endif // _DEBUG
		m_nORIENTATION = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bORIENTATION;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nORIENTATION ) + 0;
	}
};
#define shaderDynamicTest_splinecard_vs20 vsh_forgot_to_set_dynamic_ORIENTATION + 0

static const ShaderComboInformation_t s_DynamicComboArray_splinecard_vs20[1] = 
{
	{ "ORIENTATION", 0, 3 },
};

static const ShaderComboInformation_t s_StaticComboArray_splinecard_vs20[6] = 
{
	{ "ADDBASETEXTURE2", 0, 0 },
	{ "EXTRACTGREENALPHA", 0, 0 },
	{ "DUALSEQUENCE", 0, 0 },
	{ "DEPTHBLEND", 0, 0 },
	{ "PACKED_INTERPOLATOR", 0, 0 },
	{ "ANIMBLEND_OR_MAXLUMFRAMEBLEND1", 0, 1 },
};
static const ShaderComboSemantics_t splinecard_vs20_combos =
{
	"splinecard_vs20", s_DynamicComboArray_splinecard_vs20, 1, s_StaticComboArray_splinecard_vs20, 6 
};

class ConstructMe_splinecard_vs20
{
public:
	ConstructMe_splinecard_vs20()
	{
		GetShaderDLL()->AddShaderComboInformation( &splinecard_vs20_combos );
	}
};

static ConstructMe_splinecard_vs20 s_ConstructMe_splinecard_vs20;
