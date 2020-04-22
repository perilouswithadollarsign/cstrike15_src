// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// ( $FLOAT_BACK_BUFFER == 1 ) && ( $SRGB_INPUT_ADAPTER == 1 )
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class downsample_nohdr_ps20_Static_Index
{
private:
	int m_nBLOOMTYPE;
#ifdef _DEBUG
	bool m_bBLOOMTYPE;
#endif
public:
	void SetBLOOMTYPE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBLOOMTYPE = i;
#ifdef _DEBUG
		m_bBLOOMTYPE = true;
#endif
	}
	void SetBLOOMTYPE( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nBLOOMTYPE = i ? 1 : 0;
#ifdef _DEBUG
		m_bBLOOMTYPE = true;
#endif
	}
private:
	int m_nPS3REGCOUNT48;
#ifdef _DEBUG
	bool m_bPS3REGCOUNT48;
#endif
public:
	void SetPS3REGCOUNT48( int i )
	{
		Assert( i >= 0 && i <= 0 );
		m_nPS3REGCOUNT48 = i;
#ifdef _DEBUG
		m_bPS3REGCOUNT48 = true;
#endif
	}
	void SetPS3REGCOUNT48( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 0 );
		m_nPS3REGCOUNT48 = i ? 1 : 0;
#ifdef _DEBUG
		m_bPS3REGCOUNT48 = true;
#endif
	}
public:
	// CONSTRUCTOR
	downsample_nohdr_ps20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bBLOOMTYPE = false;
#endif // _DEBUG
		m_nBLOOMTYPE = 0;
#ifdef _DEBUG
		m_bPS3REGCOUNT48 = false;
#endif // _DEBUG
		m_nPS3REGCOUNT48 = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bBLOOMTYPE && m_bPS3REGCOUNT48;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nBLOOMTYPE ) + ( 2 * m_nPS3REGCOUNT48 ) + 0;
	}
};
#define shaderStaticTest_downsample_nohdr_ps20 psh_forgot_to_set_static_BLOOMTYPE + psh_forgot_to_set_static_PS3REGCOUNT48 + 0
class downsample_nohdr_ps20_Dynamic_Index
{
public:
	// CONSTRUCTOR
	downsample_nohdr_ps20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_downsample_nohdr_ps20 0

static const ShaderComboInformation_t s_StaticComboArray_Downsample_nohdr_ps20[2] = 
{
	{ "BLOOMTYPE", 0, 1 },
	{ "PS3REGCOUNT48", 0, 0 },
};
static const ShaderComboSemantics_t Downsample_nohdr_ps20_combos =
{
	"Downsample_nohdr_ps20", NULL, 0, s_StaticComboArray_Downsample_nohdr_ps20, 2 
};

class ConstructMe_Downsample_nohdr_ps20
{
public:
	ConstructMe_Downsample_nohdr_ps20()
	{
		GetShaderDLL()->AddShaderComboInformation( &Downsample_nohdr_ps20_combos );
	}
};

static ConstructMe_Downsample_nohdr_ps20 s_ConstructMe_Downsample_nohdr_ps20;
