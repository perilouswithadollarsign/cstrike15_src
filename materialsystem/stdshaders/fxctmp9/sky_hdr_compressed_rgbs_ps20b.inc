// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $PIXELFOGTYPE && defined $WRITEWATERFOGTODESTALPHA && ( $PIXELFOGTYPE != 1 ) && $WRITEWATERFOGTODESTALPHA
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPTINT && $LIGHTING_PREVIEW && $FASTPATHENVMAPTINT
// defined $LIGHTING_PREVIEW && defined $FASTPATHENVMAPCONTRAST && $LIGHTING_PREVIEW && $FASTPATHENVMAPCONTRAST
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
// ($FLASHLIGHT || $FLASHLIGHTSHADOWS) && $LIGHTING_PREVIEW
#include "shaderlib/cshader.h"
class sky_hdr_compressed_rgbs_ps20b_Static_Index
{
public:
	// CONSTRUCTOR
	sky_hdr_compressed_rgbs_ps20b_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
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
#define shaderStaticTest_sky_hdr_compressed_rgbs_ps20b 0
class sky_hdr_compressed_rgbs_ps20b_Dynamic_Index
{
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
public:
	// CONSTRUCTOR
	sky_hdr_compressed_rgbs_ps20b_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
	{
#ifdef _DEBUG
		m_bWRITE_DEPTH_TO_DESTALPHA = false;
#endif // _DEBUG
		m_nWRITE_DEPTH_TO_DESTALPHA = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bWRITE_DEPTH_TO_DESTALPHA;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nWRITE_DEPTH_TO_DESTALPHA ) + 0;
	}
};
#define shaderDynamicTest_sky_hdr_compressed_rgbs_ps20b psh_forgot_to_set_dynamic_WRITE_DEPTH_TO_DESTALPHA + 0

static const ShaderComboInformation_t s_DynamicComboArray_sky_hdr_compressed_rgbs_ps20b[1] = 
{
	{ "WRITE_DEPTH_TO_DESTALPHA", 0, 1 },
};
static const ShaderComboSemantics_t sky_hdr_compressed_rgbs_ps20b_combos =
{
	"sky_hdr_compressed_rgbs_ps20b", s_DynamicComboArray_sky_hdr_compressed_rgbs_ps20b, 1, NULL, 0 
};

class ConstructMe_sky_hdr_compressed_rgbs_ps20b
{
public:
	ConstructMe_sky_hdr_compressed_rgbs_ps20b()
	{
		GetShaderDLL()->AddShaderComboInformation( &sky_hdr_compressed_rgbs_ps20b_combos );
	}
};

static ConstructMe_sky_hdr_compressed_rgbs_ps20b s_ConstructMe_sky_hdr_compressed_rgbs_ps20b;
