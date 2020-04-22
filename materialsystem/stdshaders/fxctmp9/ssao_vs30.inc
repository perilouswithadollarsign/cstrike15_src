// ALL SKIP STATEMENTS THAT AFFECT THIS SHADER!!!
// defined $LIGHTING_PREVIEW && defined $FASTPATH && $LIGHTING_PREVIEW && $FASTPATH
#include "shaderlib/cshader.h"
class ssao_vs30_Static_Index
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
public:
	// CONSTRUCTOR
	ssao_vs30_Static_Index( IShaderShadow *pShaderShadow, IMaterialVar **params )
	{
#ifdef _DEBUG
		m_bFINAL = false;
#endif // _DEBUG
		m_nFINAL = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bFINAL;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nFINAL ) + 0;
	}
};
#define shaderStaticTest_ssao_vs30 vsh_forgot_to_set_static_FINAL + 0
class ssao_vs30_Dynamic_Index
{
public:
	// CONSTRUCTOR
	ssao_vs30_Dynamic_Index( IShaderDynamicAPI *pShaderAPI )
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
#define shaderDynamicTest_ssao_vs30 0
