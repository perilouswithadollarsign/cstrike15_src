// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class debugdrawenvmapmask_ps20_Static_Index
{
public:
	// CONSTRUCTOR
	debugdrawenvmapmask_ps20_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_debugdrawenvmapmask_ps20 0
class debugdrawenvmapmask_ps20_Dynamic_Index
{
private:
	int m_nSHOWALPHA;
#ifdef _DEBUG
	bool m_bSHOWALPHA;
#endif
public:
	void SetSHOWALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nSHOWALPHA = i;
#ifdef _DEBUG
		m_bSHOWALPHA = true;
#endif
	}
	void SetSHOWALPHA( bool i )
	{
		Assert( ( i ? 1 : 0 ) >= 0 && ( i ? 1 : 0 ) <= 1 );
		m_nSHOWALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bSHOWALPHA = true;
#endif
	}
public:
	// CONSTRUCTOR
	debugdrawenvmapmask_ps20_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bSHOWALPHA = false;
#endif // _DEBUG
		m_nSHOWALPHA = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bSHOWALPHA;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nSHOWALPHA ) + 0;
	}
};
#define shaderDynamicTest_debugdrawenvmapmask_ps20 psh_forgot_to_set_dynamic_SHOWALPHA + 0

static const ShaderComboInformation_t s_DynamicComboArray_DebugDrawEnvmapMask_ps20[1] = 
{
	{ "SHOWALPHA", 0, 1 },
};
static const ShaderComboSemantics_t DebugDrawEnvmapMask_ps20_combos =
{
	"DebugDrawEnvmapMask_ps20", s_DynamicComboArray_DebugDrawEnvmapMask_ps20, 1, NULL, 0 
};

class ConstructMe_DebugDrawEnvmapMask_ps20
{
public:
	ConstructMe_DebugDrawEnvmapMask_ps20()
	{
		GetShaderDLL()->AddShaderComboInformation( &DebugDrawEnvmapMask_ps20_combos );
	}
};

static ConstructMe_DebugDrawEnvmapMask_ps20 s_ConstructMe_DebugDrawEnvmapMask_ps20;
