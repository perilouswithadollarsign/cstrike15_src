//===== Copyright 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#define DISABLE_PROTECTED_THINGS
#include "togl/rendermechanism.h"
#include "TransitionTable.h"
#include "recording.h"
#include "shaderapidx8.h"
#include "shaderapi/ishaderutil.h"
#include "tier1/convar.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"
#include "vertexshaderdx8.h"
#include "tier0/vprof.h"
#include "shaderdevicedx8.h"
#include "shaderapi_global.h"
#include "materialsystem/materialsystem_config.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
 

enum
{
	TEXTURE_STAGE_BIT_COUNT = 4,
	TEXTURE_STAGE_MAX_STAGE = 1 << TEXTURE_STAGE_BIT_COUNT,
	TEXTURE_STAGE_MASK = TEXTURE_STAGE_MAX_STAGE - 1,

	TEXTURE_OP_BIT_COUNT = 7 - TEXTURE_STAGE_BIT_COUNT,
	TEXTURE_OP_SHIFT = TEXTURE_STAGE_BIT_COUNT,
	TEXTURE_OP_MASK = ((1 << TEXTURE_OP_BIT_COUNT) - 1) << TEXTURE_OP_SHIFT,
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
CTransitionTable *g_pTransitionTable = NULL;

#ifdef DEBUG_BOARD_STATE
inline ShadowState_t& BoardState()
{
	return g_pTransitionTable->BoardState();
}
#endif

inline CTransitionTable::CurrentState_t& CurrentState()
{
	return g_pTransitionTable->CurrentState();
}


//-----------------------------------------------------------------------------
// Less functions
//-----------------------------------------------------------------------------
bool CTransitionTable::ShadowStateDictLessFunc::Less( const CTransitionTable::ShadowStateDictEntry_t &src1, const CTransitionTable::ShadowStateDictEntry_t &src2, void *pCtx )
{
	return src1.m_nChecksum < src2.m_nChecksum;
}

bool CTransitionTable::SnapshotDictLessFunc::Less( const CTransitionTable::SnapshotDictEntry_t &src1, const CTransitionTable::SnapshotDictEntry_t &src2, void *pCtx )
{
	return src1.m_nChecksum < src2.m_nChecksum;
}

bool CTransitionTable::UniqueSnapshotLessFunc::Less( const CTransitionTable::TransitionList_t &src1, const CTransitionTable::TransitionList_t &src2, void *pCtx )
{
	return src1.m_NumOperations > src2.m_NumOperations;
}

	
//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CTransitionTable::CTransitionTable() : m_DefaultStateSnapshot(-1),
	m_CurrentShadowId(-1), m_CurrentSnapshotId(-1), m_TransitionOps( 0, 8192 ), m_ShadowStateList( 0, 256 ),
	m_TransitionTable( 0, 256 ), m_SnapshotList( 0, 256 ), 
	m_ShadowStateDict(0, 256 ), 
	m_SnapshotDict( 0, 256 ), 
	m_UniqueTransitions( 0, 4096 ),
	m_bShadowDepthBiasValuesDirty( false )
{
	Assert( !g_pTransitionTable );
	g_pTransitionTable = this;

#ifdef DEBUG_BOARD_STATE
	memset( &m_BoardState, 0, sizeof( m_BoardState ) );
	memset( &m_BoardShaderState, 0, sizeof( m_BoardShaderState ) );
#endif
}

CTransitionTable::~CTransitionTable()
{
	Assert( g_pTransitionTable == this );
	g_pTransitionTable = NULL;
}


//-----------------------------------------------------------------------------
// Initialization, shutdown
//-----------------------------------------------------------------------------
bool CTransitionTable::Init( )
{
	return true;
}

void CTransitionTable::Shutdown( )
{
	Reset();
}


//-----------------------------------------------------------------------------
// Creates a shadow, adding an entry into the shadow list and transition table
//-----------------------------------------------------------------------------
StateSnapshot_t CTransitionTable::CreateStateSnapshot( ShadowStateId_t shadowStateId, const ShadowShaderState_t& currentShaderState )
{
	StateSnapshot_t snapshotId = m_SnapshotList.AddToTail();

	// Copy our snapshot into the list
	SnapshotShaderState_t &shaderState = m_SnapshotList[snapshotId];
	shaderState.m_ShadowStateId = shadowStateId;
	memcpy( &shaderState.m_ShaderState, &currentShaderState, sizeof(ShadowShaderState_t) );
	shaderState.m_nReserved = 0;	// needed to get a good CRC
	shaderState.m_nReserved2 = 0;

	// Insert entry into the lookup table
	SnapshotDictEntry_t insert;

	CRC32_Init(	&insert.m_nChecksum );
	CRC32_ProcessBuffer( &insert.m_nChecksum, &shaderState, sizeof(SnapshotShaderState_t) );
	CRC32_Final( &insert.m_nChecksum );

	insert.m_nSnapshot = snapshotId;
	m_SnapshotDict.Insert( insert );

	return snapshotId;
}


//-----------------------------------------------------------------------------
// Creates a shadow, adding an entry into the shadow list and transition table
//-----------------------------------------------------------------------------
CTransitionTable::ShadowStateId_t CTransitionTable::CreateShadowState( const ShadowState_t &currentState )
{
	int newShaderState = m_ShadowStateList.AddToTail();

	// Copy our snapshot into the list
	memcpy( &m_ShadowStateList[newShaderState], &currentState, sizeof(ShadowState_t) );

	// all existing states must transition to the new state
	int i;
	for ( i = 0; i < newShaderState; ++i )
	{
		// Add a new transition to all existing states
		int newElem = m_TransitionTable[i].AddToTail();
		m_TransitionTable[i][newElem].m_FirstOperation = INVALID_TRANSITION_OP;
		m_TransitionTable[i][newElem].m_NumOperations = 0;
	}

	// Add a new vector for this transition
	int newTransitionElem = m_TransitionTable.AddToTail();
	m_TransitionTable[newTransitionElem].EnsureCapacity( 32 );
	Assert( newShaderState == newTransitionElem );

	for ( i = 0; i <= newShaderState; ++i )
	{
		// Add a new transition from all existing states
		int newElem = m_TransitionTable[newShaderState].AddToTail();
		m_TransitionTable[newShaderState][newElem].m_FirstOperation = INVALID_TRANSITION_OP;
		m_TransitionTable[newShaderState][newElem].m_NumOperations = 0;
	}

	// Insert entry into the lookup table
	ShadowStateDictEntry_t insert;

	CRC32_Init(	&insert.m_nChecksum );
	CRC32_ProcessBuffer( &insert.m_nChecksum, &m_ShadowStateList[newShaderState], sizeof(ShadowState_t) );
	CRC32_Final( &insert.m_nChecksum );

	insert.m_nShadowStateId = newShaderState;
	m_ShadowStateDict.Insert( insert );

	return newShaderState;
}


//-----------------------------------------------------------------------------
// Finds a snapshot, if it exists. Or creates a new one if it doesn't.
//-----------------------------------------------------------------------------
CTransitionTable::ShadowStateId_t CTransitionTable::FindShadowState( const ShadowState_t& currentState ) const
{
	ShadowStateDictEntry_t find;

	CRC32_Init(	&find.m_nChecksum );
	CRC32_ProcessBuffer( &find.m_nChecksum, &currentState, sizeof(ShadowState_t) );
	CRC32_Final( &find.m_nChecksum );
	
	int nDictCount = m_ShadowStateDict.Count();
	int i = m_ShadowStateDict.FindLessOrEqual( find );
	if ( i < 0 )
		return (ShadowStateId_t)-1;

	for ( ; i < nDictCount; ++i )
	{
		const ShadowStateDictEntry_t &entry = m_ShadowStateDict[i];

		// Didn't find a match
		if ( entry.m_nChecksum > find.m_nChecksum )
			break;

		if ( entry.m_nChecksum != find.m_nChecksum )
			continue;

		ShadowStateId_t nShadowState = entry.m_nShadowStateId;
		if (!memcmp(&m_ShadowStateList[nShadowState], &currentState, sizeof(ShadowState_t) ))
			return nShadowState;
	}

	// Need to create a new one
	return (ShadowStateId_t)-1;
}


//-----------------------------------------------------------------------------
// Finds a snapshot, if it exists. Or creates a new one if it doesn't.
//-----------------------------------------------------------------------------
StateSnapshot_t CTransitionTable::FindStateSnapshot( ShadowStateId_t id, const ShadowShaderState_t& currentState ) const
{
	SnapshotShaderState_t temp;
	temp.m_ShaderState = currentState;
	temp.m_ShadowStateId = id;
	temp.m_nReserved = 0;	// needed to get a good CRC
	temp.m_nReserved2 = 0;

	SnapshotDictEntry_t find;

	CRC32_Init(	&find.m_nChecksum );
	CRC32_ProcessBuffer( &find.m_nChecksum, &temp, sizeof(temp) );
	CRC32_Final( &find.m_nChecksum );

	int nDictCount = m_SnapshotDict.Count();
	int i = m_SnapshotDict.FindLessOrEqual( find );
	if ( i < 0 )
		return (StateSnapshot_t)-1;

	for ( ; i < nDictCount; ++i )
	{
		// Didn't find a match
		if ( m_SnapshotDict[i].m_nChecksum > find.m_nChecksum )
			break;

		if ( m_SnapshotDict[i].m_nChecksum != find.m_nChecksum )
			continue;

		StateSnapshot_t nShapshot = m_SnapshotDict[i].m_nSnapshot;
		if ( (id == m_SnapshotList[nShapshot].m_ShadowStateId) && 
			!memcmp(&m_SnapshotList[nShapshot].m_ShaderState, &currentState, sizeof(ShadowShaderState_t)) )
		{
			return nShapshot;
		}
	}

	// Need to create a new one
	return (StateSnapshot_t)-1;
}


//-----------------------------------------------------------------------------
// Used to clear the transition table when we know it's become invalid.
//-----------------------------------------------------------------------------
void CTransitionTable::Reset()
{
	m_ShadowStateList.RemoveAll();
	m_SnapshotList.RemoveAll();
	m_TransitionTable.RemoveAll();
	m_TransitionOps.RemoveAll();
	m_ShadowStateDict.RemoveAll();
	m_SnapshotDict.RemoveAll();
	m_UniqueTransitions.RemoveAll();
	m_CurrentShadowId = -1;
	m_CurrentSnapshotId = -1;
	m_DefaultStateSnapshot = -1;
}


//-----------------------------------------------------------------------------
// Sets the texture stage state
//-----------------------------------------------------------------------------
#ifdef _WIN32
#pragma warning( disable : 4189 )
#endif

static inline void SetTextureStageState( int stage, D3DTEXTURESTAGESTATETYPE state, DWORD val )
{
#if !defined( _X360 )
	Assert( !g_pShaderDeviceDx8->IsDeactivated() );
	Dx9Device()->SetTextureStageState( stage, state, val );
#endif
}

//Moved to a #define so every instance of this skips unsupported render states at compile time
#define SetSamplerState( _stage, _state, _val )								\
	{																		\
		if ( (_state != D3DSAMP_NOTSUPPORTED) )								\
		{																	\
			Assert( !g_pShaderDeviceDx8->IsDeactivated() );					\
			Dx9Device()->SetSamplerState( _stage, _state, _val );			\
		}																	\
	}

//Moved to a #define so every instance of this skips unsupported render states at compile time
#define SetRenderState( _state, _val )						\
	{														\
		if ( _state != D3DRS_NOTSUPPORTED )					\
		{													\
			Assert( !g_pShaderDeviceDx8->IsDeactivated() ); \
			Dx9Device()->SetRenderState( _state, _val );	\
		}													\
	}

#ifdef DX_TO_GL_ABSTRACTION
#define SetRenderStateConstMacro( state, val ) { if ( state != D3DRS_NOTSUPPORTED ) Dx9Device()->SetRenderStateConstInline( state, val ); }
#else
#define SetRenderStateConstMacro( state, val ) SetRenderState( state, val )
#endif

#ifdef _WIN32
#pragma warning( default : 4189 )
#endif

//-----------------------------------------------------------------------------
// Methods that actually apply the state
//-----------------------------------------------------------------------------
#ifdef DEBUG_BOARD_STATE

static bool g_SpewTransitions = false;

#define UPDATE_BOARD_RENDER_STATE( _d3dState, _state )					\
	{																	\
		BoardState().m_ ## _state = shaderState.m_ ## _state;			\
		if (g_SpewTransitions)											\
		{																\
			char buf[128];												\
			sprintf( buf, "Apply %s : %d\n", #_d3dState, shaderState.m_ ## _state ); \
			Plat_DebugString(buf);										\
		}																\
	}

#define UPDATE_BOARD_RENDER_STATE_ALPHATEST_AND_MISC( _d3dState, _state )					\
	{																	\
		BoardState().m_AlphaTestAndMiscState.m_ ## _state = shaderState.m_AlphaTestAndMiscState.m_ ## _state;			\
		if (g_SpewTransitions)											\
		{																\
			char buf[128];												\
			sprintf( buf, "Apply %s : %d\n", #_d3dState, shaderState.m_AlphaTestAndMiscState.m_ ## _state ); \
			Plat_DebugString(buf);										\
		}																\
	}

#define UPDATE_BOARD_TEXTURE_STAGE_STATE( _d3dState, _state, _stage )	\
	{																	\
		BoardState().m_TextureStage[_stage].m_ ## _state = shaderState.m_TextureStage[_stage].m_ ## _state;	\
		if (g_SpewTransitions)											\
		{																\
			char buf[128];												\
			sprintf( buf, "Apply Tex %s (%d): %d\n", #_d3dState, _stage, shaderState.m_TextureStage[_stage].m_ ## _state ); \
			Plat_DebugString(buf);										\
		}																\
	}

#define UPDATE_BOARD_SAMPLER_STATE( _d3dState, _state, _stage )			\
{																		\
	BoardState().m_n ## _state = BoardState().m_n ## _state & ~( 1 << _stage ) | ( shaderState.m_n ## _state & ( 1 << _stage ) ); \
	if (g_SpewTransitions)												\
	{																	\
		char buf[128];													\
		sprintf( buf, "Apply SamplerSate %s (%d): %d\n", #_d3dState, stage, 0 != (shaderState.m_n ## _state & ( 1 << _stage ) ) ); \
		Plat_DebugString(buf);											\
	}																	\
}

#else

#define UPDATE_BOARD_RENDER_STATE( _d3dState, _state ) {}
#define UPDATE_BOARD_RENDER_STATE_ALPHATEST_AND_MISC( _d3dState, _state ) {}
#define UPDATE_BOARD_TEXTURE_STAGE_STATE( _d3dState, _state, _stage ) {}
#define UPDATE_BOARD_SAMPLER_STATE( _d3dState, _state, _stage ) {}

#endif


#define APPLY_RENDER_STATE_FUNC( _d3dState, _state )					\
	void Apply ## _state( const ShadowState_t& shaderState, int arg )	\
	{																	\
		SetRenderState( _d3dState, shaderState.m_ ## _state );			\
		UPDATE_BOARD_RENDER_STATE( _d3dState, _state );					\
	}

#define APPLY_RENDER_STATE_FUNC_ALPHATEST_AND_MISC( _d3dState, _state )					\
	void Apply ## _state( const ShadowState_t& shaderState, int arg )	\
	{																	\
		SetRenderState( _d3dState, shaderState.m_AlphaTestAndMiscState.m_ ## _state );			\
		UPDATE_BOARD_RENDER_STATE_ALPHATEST_AND_MISC( _d3dState, _state );					\
	}

#define APPLY_TEXTURE_STAGE_STATE_FUNC( _d3dState, _state )				\
	void Apply ## _state( const ShadowState_t& shaderState, int stage )	\
	{																	\
		SetTextureStageState( stage, _d3dState, shaderState.m_TextureStage[stage].m_ ## _state );	\
		UPDATE_BOARD_TEXTURE_STAGE_STATE( _d3dState, _state, stage );	\
	}

#define APPLY_SAMPLER_STATE_FUNC( _d3dState, _state )					\
	void Apply ## _state( const ShadowState_t& shaderState, int stage )	\
	{																	\
		SetSamplerState( stage, _d3dState, shaderState.m_SamplerState[stage].m_ ## _state );	\
		UPDATE_BOARD_SAMPLER_STATE( _d3dState, _state, stage );			\
	}	

// Special overridden sampler state to turn on Fetch4 on ATI hardware (and 360?)
void ApplyFetch4Enable( const ShadowState_t& shaderState, int stage )
{
	if ( HardwareConfig()->SupportsFetch4() )
	{
		SetSamplerState( stage, ATISAMP_FETCH4, shaderState.m_nFetch4Enable & ( 1 << stage ) ? ATI_FETCH4_ENABLE : ATI_FETCH4_DISABLE );
	}

	UPDATE_BOARD_SAMPLER_STATE( ATISAMP_FETCH4, Fetch4Enable, stage );
}	

#ifdef DX_TO_GL_ABSTRACTION
void ApplyShadowFilterEnable( const ShadowState_t& shaderState, int stage )
{
	SetSamplerState( stage, D3DSAMP_SHADOWFILTER, shaderState.m_nShadowFilterEnable & ( 1 << stage ) );
	
	UPDATE_BOARD_SAMPLER_STATE( D3DSAMP_SHADOWFILTER, ShadowFilterEnable, stage );
}																	
#endif


//APPLY_RENDER_STATE_FUNC( D3DRS_ZWRITEENABLE,			ZWriteEnable )
//APPLY_RENDER_STATE_FUNC( D3DRS_COLORWRITEENABLE,		ColorWriteEnable )
APPLY_RENDER_STATE_FUNC_ALPHATEST_AND_MISC( D3DRS_FILLMODE,				FillMode )


void ApplyZWriteEnable( const ShadowState_t& shaderState, int arg )
{
	SetRenderState( D3DRS_ZWRITEENABLE, shaderState.m_DepthTestState.m_ZWriteEnable );
#if defined( _X360 )
	//SetRenderState( D3DRS_HIZWRITEENABLE, shaderState.m_ZWriteEnable ? D3DHIZ_AUTOMATIC : D3DHIZ_DISABLE );
#endif
#ifdef DEBUG_BOARD_STATE
	BoardState().m_DepthTestState.m_ZWriteEnable = shaderState.m_DepthTestState.m_ZWriteEnable;
#endif
}

void ApplyColorWriteEnable( const ShadowState_t& shaderState, int arg )
{
	SetRenderState( D3DRS_COLORWRITEENABLE, shaderState.m_DepthTestState.m_ColorWriteEnable );
	g_pTransitionTable->CurrentState().m_ColorWriteEnable = shaderState.m_DepthTestState.m_ColorWriteEnable;

#ifdef DEBUG_BOARD_STATE
	BoardState().m_DepthTestState.m_ColorWriteEnable = shaderState.m_DepthTestState.m_ColorWriteEnable;
#endif
}


void ApplySRGBWriteEnable( const ShadowState_t& shadowState, int stageUnused )
{
	g_pTransitionTable->ApplySRGBWriteEnable( shadowState );
}


void CTransitionTable::ApplySRGBWriteEnable( const ShadowState_t& shaderState  )
{
	// ApplySRGBWriteEnable set to true means that the shader is writing linear values.
	if ( CurrentState().m_bLinearColorSpaceFrameBufferEnable )
	{
		// The shader had better be writing linear values since we can't convert to gamma here.
		// Can't leave this assert here since there are cases where the shader is doing the right thing.
		// This is good to test occasionally to make sure that the shaders are doing the right thing.
		//		Assert( shaderState.m_SRGBWriteEnable );

		// render target is linear
		SetRenderState( D3DRS_SRGBWRITEENABLE, 0 );
		ShaderAPI()->EnabledSRGBWrite( false );

		// fog isn't fixed-function with linear frame buffers, so don't bother with that here.
	}
	else
	{
		// render target is gamma

		// SRGBWrite enable can affect the space in which fog color is defined		
		if ( HardwareConfig()->NeedsShaderSRGBConversion() )
		{
			if ( HardwareConfig()->GetDXSupportLevel() >= 92 ) //in 2b supported devices, we never actually enable SRGB writes, but instead handle the conversion in the pixel shader. But we want all other code to be unaware.
			{
				SetRenderState( D3DRS_SRGBWRITEENABLE, 0 );
			}
			else
			{
				SetRenderState( D3DRS_SRGBWRITEENABLE, shaderState.m_FogAndMiscState.m_SRGBWriteEnable );
			}
		}
		else
		{
			SetRenderState( D3DRS_SRGBWRITEENABLE, shaderState.m_FogAndMiscState.m_SRGBWriteEnable );
		}

		ShaderAPI()->EnabledSRGBWrite( shaderState.m_FogAndMiscState.m_SRGBWriteEnable );

		if ( HardwareConfig()->SpecifiesFogColorInLinearSpace() )
		{
			ShaderAPI()->ApplyFogMode( shaderState.m_FogAndMiscState.FogMode(), shaderState.m_FogAndMiscState.m_bVertexFogEnable, shaderState.m_FogAndMiscState.m_SRGBWriteEnable, shaderState.m_FogAndMiscState.m_bDisableFogGammaCorrection );
		}
	}

#ifdef _DEBUG
	BoardState().m_FogAndMiscState.m_SRGBWriteEnable = shaderState.m_FogAndMiscState.m_SRGBWriteEnable;
	if (g_SpewTransitions)											
	{																
		char buf[128];												
		sprintf( buf, "Apply %s : %d\n", "D3DRS_SRGBWRITEENABLE", shaderState.m_FogAndMiscState.m_SRGBWriteEnable );
		Plat_DebugString(buf);										
	}																
#endif
}

void ApplyDisableFogGammaCorrection( const ShadowState_t& shadowState, int stageUnused )
{
	ShaderAPI()->ApplyFogMode( shadowState.m_FogAndMiscState.FogMode(), shadowState.m_FogAndMiscState.m_bVertexFogEnable, shadowState.m_FogAndMiscState.m_SRGBWriteEnable, shadowState.m_FogAndMiscState.m_bDisableFogGammaCorrection );
		
#ifdef DEBUG_BOARD_STATE
	g_pTransitionTable->BoardState().m_FogAndMiscState.m_bDisableFogGammaCorrection = shadowState.m_FogAndMiscState.m_bDisableFogGammaCorrection;
#endif
}




void ApplyDepthTest( const ShadowState_t& state, int stage )
{
	g_pTransitionTable->ApplyDepthTest( state );
}

void CTransitionTable::SetZEnable( D3DZBUFFERTYPE nEnable )
{
	SetRenderState( D3DRS_ZENABLE, nEnable );
#if defined( _X360 )
		//SetRenderState( D3DRS_HIZENABLE, nEnable ? D3DHIZ_AUTOMATIC : D3DHIZ_DISABLE );
#endif
}

void CTransitionTable::SetZFunc( D3DCMPFUNC nCmpFunc )
{
	SetRenderState( D3DRS_ZFUNC, nCmpFunc );
}

void CTransitionTable::ApplyDepthTest( const ShadowState_t& state )
{
	if ( m_CurrentState.m_nDepthTestStateAsInt != state.m_nDepthTestStateAsInt )
	{
		SetZEnable( ( D3DZBUFFERTYPE ) state.m_DepthTestState.m_ZEnable );
		if (state.m_DepthTestState.m_ZEnable != D3DZB_FALSE)
		{
			SetZFunc( ( D3DCMPFUNC ) state.m_DepthTestState.m_ZFunc );
		}
		if ( ( state.m_DepthTestState.m_ZBias == SHADER_POLYOFFSET_SHADOW_BIAS ) && m_bShadowDepthBiasValuesDirty )
		{
			ShaderAPI()->ApplyZBias( state.m_DepthTestState );
		}

#ifdef DEBUG_BOARD_STATE
	// This isn't quite true, but it's necessary for other error checking to work
		BoardState().m_DepthTestState = state.m_DepthTestState;
#endif
		m_CurrentState.m_nDepthTestStateAsInt = state.m_nDepthTestStateAsInt;
	}

}

void ApplyAlphaTest( const ShadowState_t& state, int stage )
{
	g_pTransitionTable->ApplyAlphaTest( state );
}

void CTransitionTable::ApplyAlphaTest( const ShadowState_t& state )
{
	SetRenderState( D3DRS_ALPHATESTENABLE, state.m_AlphaTestAndMiscState.m_AlphaTestEnable );

	// Set the blend state here...
	SetRenderState( D3DRS_ALPHAFUNC, state.m_AlphaTestAndMiscState.m_AlphaFunc );
	SetRenderState( D3DRS_ALPHAREF, state.m_AlphaTestAndMiscState.m_AlphaRef );

#ifdef DEBUG_BOARD_STATE
	// This isn't quite true, but it's necessary for other error checking to work
	BoardState().m_AlphaTestAndMiscState = state.m_AlphaTestAndMiscState;
#endif
}

void ApplyAlphaBlend( const ShadowState_t& state, int stage )
{
	g_pTransitionTable->ApplyAlphaBlend( state );
}

void CTransitionTable::ApplyAlphaBlend( const ShadowState_t& state )
{
	if (m_CurrentState.m_AlphaBlendState.m_AlphaBlendEnable != state.m_AlphaBlendState.m_AlphaBlendEnable)
	{
		SetRenderState( D3DRS_ALPHABLENDENABLE, state.m_AlphaBlendState.m_AlphaBlendEnable );
		m_CurrentState.m_AlphaBlendState.m_AlphaBlendEnable = state.m_AlphaBlendState.m_AlphaBlendEnable;
	}

	if (state.m_AlphaBlendState.m_AlphaBlendEnable)
	{
		// Set the blend state here...
		if (m_CurrentState.m_AlphaBlendState.m_SrcBlend != state.m_AlphaBlendState.m_SrcBlend)
		{
			SetRenderState( D3DRS_SRCBLEND, state.m_AlphaBlendState.m_SrcBlend );
			m_CurrentState.m_AlphaBlendState.m_SrcBlend = state.m_AlphaBlendState.m_SrcBlend;
		}

		if (m_CurrentState.m_AlphaBlendState.m_DestBlend != state.m_AlphaBlendState.m_DestBlend)
		{
			SetRenderState( D3DRS_DESTBLEND, state.m_AlphaBlendState.m_DestBlend );
			m_CurrentState.m_AlphaBlendState.m_DestBlend = state.m_AlphaBlendState.m_DestBlend;
		}

		if (m_CurrentState.m_AlphaBlendState.m_BlendOp != state.m_AlphaBlendState.m_BlendOp )
		{
			SetRenderState( D3DRS_BLENDOP, state.m_AlphaBlendState.m_BlendOp );
			m_CurrentState.m_AlphaBlendState.m_BlendOp = state.m_AlphaBlendState.m_BlendOp;
		}
	}

#ifdef DEBUG_BOARD_STATE
	// This isn't quite true, but it's necessary for other error checking to work
	BoardState().m_AlphaBlendState.m_AlphaBlendEnable = state.m_AlphaBlendState.m_AlphaBlendEnable;
	BoardState().m_AlphaBlendState.m_AlphaBlendEnabledForceOpaque = state.m_AlphaBlendState.m_AlphaBlendEnabledForceOpaque;
	BoardState().m_AlphaBlendState.m_SrcBlend = state.m_AlphaBlendState.m_SrcBlend;
	BoardState().m_AlphaBlendState.m_DestBlend = state.m_AlphaBlendState.m_DestBlend;
	BoardState().m_AlphaBlendState.m_BlendOp = state.m_AlphaBlendState.m_BlendOp;
#endif
}

void ApplySeparateAlphaBlend( const ShadowState_t& state, int stage )
{
	g_pTransitionTable->ApplySeparateAlphaBlend( state );
}

void CTransitionTable::ApplySeparateAlphaBlend( const ShadowState_t& state )
{
	if (m_CurrentState.m_AlphaBlendState.m_SeparateAlphaBlendEnable != state.m_AlphaBlendState.m_SeparateAlphaBlendEnable)
	{
		SetRenderState( D3DRS_SEPARATEALPHABLENDENABLE, state.m_AlphaBlendState.m_SeparateAlphaBlendEnable );
		m_CurrentState.m_AlphaBlendState.m_SeparateAlphaBlendEnable = state.m_AlphaBlendState.m_SeparateAlphaBlendEnable;
	}

	if (state.m_AlphaBlendState.m_SeparateAlphaBlendEnable)
	{
		// Set the blend state here...
		if (m_CurrentState.m_AlphaBlendState.m_SrcBlendAlpha != state.m_AlphaBlendState.m_SrcBlendAlpha)
		{
			SetRenderState( D3DRS_SRCBLENDALPHA, state.m_AlphaBlendState.m_SrcBlendAlpha );
			m_CurrentState.m_AlphaBlendState.m_SrcBlendAlpha = state.m_AlphaBlendState.m_SrcBlendAlpha;
		}

		if (m_CurrentState.m_AlphaBlendState.m_DestBlendAlpha != state.m_AlphaBlendState.m_DestBlendAlpha)
		{
			SetRenderState( D3DRS_DESTBLENDALPHA, state.m_AlphaBlendState.m_DestBlendAlpha );
			m_CurrentState.m_AlphaBlendState.m_DestBlendAlpha = state.m_AlphaBlendState.m_DestBlendAlpha;
		}

		if (m_CurrentState.m_AlphaBlendState.m_BlendOpAlpha != state.m_AlphaBlendState.m_BlendOpAlpha )
		{
			SetRenderState( D3DRS_BLENDOPALPHA, state.m_AlphaBlendState.m_BlendOpAlpha );
			m_CurrentState.m_AlphaBlendState.m_BlendOpAlpha = state.m_AlphaBlendState.m_BlendOpAlpha;
		}
	}

#ifdef DEBUG_BOARD_STATE
	// This isn't quite true, but it's necessary for other error checking to work
	BoardState().m_AlphaBlendState.m_SeparateAlphaBlendEnable = state.m_AlphaBlendState.m_SeparateAlphaBlendEnable;
	BoardState().m_AlphaBlendState.m_SrcBlendAlpha = state.m_AlphaBlendState.m_SrcBlendAlpha;
	BoardState().m_AlphaBlendState.m_DestBlendAlpha = state.m_AlphaBlendState.m_DestBlendAlpha;
	BoardState().m_AlphaBlendState.m_BlendOpAlpha = state.m_AlphaBlendState.m_BlendOpAlpha;
#endif
}


//-----------------------------------------------------------------------------
// Enables textures
//-----------------------------------------------------------------------------
void ApplyTextureEnable( const ShadowState_t& state, int stage )
{
}


//-----------------------------------------------------------------------------
// All transitions below this point depend on dynamic render state
// FIXME: Eliminate these virtual calls?
//-----------------------------------------------------------------------------
void ApplyCullEnable( const ShadowState_t& state, int arg )
{
	ShaderAPI()->ApplyCullEnable( state.m_AlphaTestAndMiscState.m_CullEnable );

#ifdef DEBUG_BOARD_STATE
	BoardState().m_AlphaTestAndMiscState.m_CullEnable = state.m_AlphaTestAndMiscState.m_CullEnable;
#endif
}

//-----------------------------------------------------------------------------
void ApplyAlphaToCoverage( const ShadowState_t& state, int arg )
{
	ShaderAPI()->ApplyAlphaToCoverage( state.m_AlphaTestAndMiscState.m_EnableAlphaToCoverage );

#ifdef DEBUG_BOARD_STATE
	BoardState().m_AlphaTestAndMiscState.m_EnableAlphaToCoverage = state.m_AlphaTestAndMiscState.m_EnableAlphaToCoverage;
#endif
}


//-----------------------------------------------------------------------------
// Outputs the fog mode string
//-----------------------------------------------------------------------------
#ifdef RECORDING
const char *ShaderFogModeToString( ShaderFogMode_t fogMode )
{
	switch( fogMode )
	{
	case SHADER_FOGMODE_DISABLED:
		return "SHADER_FOGMODE_DISABLED";
	case SHADER_FOGMODE_OO_OVERBRIGHT:
		return "SHADER_FOGMODE_OO_OVERBRIGHT";
	case SHADER_FOGMODE_BLACK:
		return "SHADER_FOGMODE_BLACK";
	case SHADER_FOGMODE_GREY:
		return "SHADER_FOGMODE_GREY";
	case SHADER_FOGMODE_FOGCOLOR:
		return "SHADER_FOGMODE_FOGCOLOR";
	case SHADER_FOGMODE_WHITE:
		return "SHADER_FOGMODE_WHITE";
	case SHADER_FOGMODE_NUMFOGMODES:
		return "SHADER_FOGMODE_NUMFOGMODES";
	default:
		return "ERROR";
	}
}
#endif

// Uses GetConfig().overbright and GetSceneFogMode, so 
// will have to fix up the state manually when those change.
void ApplyFogMode( const ShadowState_t& state, int arg )
{
#ifdef RECORDING
	char buf[1024];
	sprintf( buf, "ApplyFogMode( %s )", ShaderFogModeToString( state.m_FogMode ) );
	RECORD_DEBUG_STRING( buf );
#endif

	ShaderAPI()->ApplyFogMode( state.m_FogAndMiscState.FogMode(), state.m_FogAndMiscState.m_bVertexFogEnable, state.m_FogAndMiscState.m_SRGBWriteEnable, state.m_FogAndMiscState.m_bDisableFogGammaCorrection );

#ifdef DEBUG_BOARD_STATE
	BoardState().m_FogAndMiscState.m_FogMode = state.m_FogAndMiscState.m_FogMode;
	BoardState().m_FogAndMiscState.m_bVertexFogEnable = state.m_FogAndMiscState.m_bVertexFogEnable;
#endif
}




//-----------------------------------------------------------------------------
// Finds identical transition lists and shares them 
//-----------------------------------------------------------------------------
unsigned int CTransitionTable::FindIdenticalTransitionList( unsigned int firstElem, 
					unsigned short numOps, unsigned int nFirstTest ) const
{
	VPROF("CTransitionTable::FindIdenticalTransitionList");
	// As it turns out, this works most of the time
	if ( nFirstTest != INVALID_TRANSITION_OP )
	{
		const TransitionOp_t *pCurrOp = &m_TransitionOps[firstElem];
		const TransitionOp_t *pTestOp = &m_TransitionOps[nFirstTest];
		if ( !memcmp( pCurrOp, pTestOp, numOps * sizeof(TransitionOp_t) ) )
			return nFirstTest;	
	}

	// Look for a common list
	const TransitionOp_t &op = m_TransitionOps[firstElem];

	int nCount = m_UniqueTransitions.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		const TransitionList_t &list = m_UniqueTransitions[i];

		// We can early out here because we've sorted the unique transitions
		// descending by count 
		if ( list.m_NumOperations < numOps )
			return INVALID_TRANSITION_OP;

		// If we don't find a match in the first 
		int nPotentialMatch;
		int nLastTest = list.m_FirstOperation + list.m_NumOperations - numOps;
		for ( nPotentialMatch = list.m_FirstOperation; nPotentialMatch <= nLastTest; ++nPotentialMatch )
		{
			// Find the first match
			const TransitionOp_t &testOp = m_TransitionOps[nPotentialMatch];
			if ( testOp.m_nBits == op.m_nBits )
				break;
		}

		// No matches found, continue
		if ( nPotentialMatch > nLastTest )
			continue;

		// Ok, found a match of the first op, lets see if they all match
		if ( numOps == 1 )
			return nPotentialMatch;

		const TransitionOp_t *pCurrOp = &m_TransitionOps[firstElem + 1];
		const TransitionOp_t *pTestOp = &m_TransitionOps[nPotentialMatch + 1];
		if ( !memcmp( pCurrOp, pTestOp, (numOps - 1) * sizeof(TransitionOp_t) ) )
			return nPotentialMatch;	
	}
	return INVALID_TRANSITION_OP;
}


//-----------------------------------------------------------------------------
// Create startup snapshot
//-----------------------------------------------------------------------------
void CTransitionTable::TakeDefaultStateSnapshot( )
{
	if (m_DefaultStateSnapshot == -1)
	{
		m_DefaultStateSnapshot = TakeSnapshot();

		// This will create a transition which sets *all* shadowed state
		//CreateTransitionTableEntry( m_DefaultStateSnapshot, -1 );
	}
}


//-----------------------------------------------------------------------------
// Takes a snapshot, hooks it into the material
//-----------------------------------------------------------------------------
StateSnapshot_t CTransitionTable::TakeSnapshot( )
{
	// Do any final computation of the shadow state
	ShaderShadow()->ComputeAggregateShadowState();

	// Get the current snapshot
	const ShadowState_t& currentState = ShaderShadow()->GetShadowState();

	// Create a new snapshot
	ShadowStateId_t shadowStateId = FindShadowState( currentState );
	if (shadowStateId == -1)
	{
		// Create entry in state transition table
		shadowStateId = CreateShadowState( currentState );
	}

	const ShadowShaderState_t& currentShaderState = ShaderShadow()->GetShadowShaderState();
 	StateSnapshot_t snapshotId = FindStateSnapshot( shadowStateId, currentShaderState );
	if (snapshotId == -1)
	{
		// Create entry in state transition table
		snapshotId = CreateStateSnapshot( shadowStateId, currentShaderState );
	}

	return snapshotId;
}


//-----------------------------------------------------------------------------
// Apply shader state (stuff that doesn't lie in the transition table)
//-----------------------------------------------------------------------------
void CTransitionTable::ApplyShaderState( const ShadowState_t &shadowState, const ShadowShaderState_t &shaderState )
{
	VPROF("CTransitionTable::ApplyShaderState");

	// FIXME: Improve early-binding of vertex shader index
	ShaderManager()->SetVertexShader( shaderState.m_VertexShader );
	ShaderManager()->SetPixelShader( shaderState.m_PixelShader );

#ifdef DEBUG_BOARD_STATE
	BoardShaderState().m_VertexShader = shaderState.m_VertexShader;
	BoardShaderState().m_PixelShader = shaderState.m_PixelShader;
	BoardShaderState().m_nStaticVshIndex = shaderState.m_nStaticVshIndex;
	BoardShaderState().m_nStaticPshIndex = shaderState.m_nStaticPshIndex;
#endif
}


void CTransitionTable::SetBoardStateFromShadowState( ShadowState_t const &shadowState )
{
	if ( m_CurrentState.m_nDepthTestStateAsInt != shadowState.m_nDepthTestStateAsInt )
	{
		SetRenderState( D3DRS_ZWRITEENABLE, shadowState.m_DepthTestState.m_ZWriteEnable );
		SetRenderState( D3DRS_ZENABLE, shadowState.m_DepthTestState.m_ZEnable );
		if (shadowState.m_DepthTestState.m_ZEnable != D3DZB_FALSE)
		{
			SetRenderState( D3DRS_ZFUNC, shadowState.m_DepthTestState.m_ZFunc );
		}
		if ( ( m_CurrentState.m_DepthTestState.m_ZBias != shadowState.m_DepthTestState.m_ZBias ) || 
			 ( ( shadowState.m_DepthTestState.m_ZBias == SHADER_POLYOFFSET_SHADOW_BIAS ) ) && m_bShadowDepthBiasValuesDirty )
		{
			ShaderAPI()->ApplyZBias( shadowState.m_DepthTestState );
		}
		SetRenderState( D3DRS_COLORWRITEENABLE, shadowState.m_DepthTestState.m_ColorWriteEnable );

		m_CurrentState.m_nDepthTestStateAsInt = shadowState.m_nDepthTestStateAsInt;
#ifdef DEBUG_BOARD_STATE
		BoardState().m_DepthTestState = shadowState.m_DepthTestState;
#endif
	}

	if ( m_CurrentState.m_nAlphaTestAndMiscStateAsInt != shadowState.m_nAlphaTestAndMiscStateAsInt )
	{
		SetRenderState( D3DRS_ALPHATESTENABLE, shadowState.m_AlphaTestAndMiscState.m_AlphaTestEnable );
		SetRenderState( D3DRS_ALPHAFUNC, shadowState.m_AlphaTestAndMiscState.m_AlphaFunc );
		SetRenderState( D3DRS_ALPHAREF, shadowState.m_AlphaTestAndMiscState.m_AlphaRef );
		ShaderAPI()->ApplyAlphaToCoverage( shadowState.m_AlphaTestAndMiscState.m_EnableAlphaToCoverage );
		ShaderAPI()->ApplyCullEnable( shadowState.m_AlphaTestAndMiscState.m_CullEnable );
		SetRenderState( D3DRS_FILLMODE, shadowState.m_AlphaTestAndMiscState.m_FillMode );

		m_CurrentState.m_nAlphaTestAndMiscStateAsInt = shadowState.m_nAlphaTestAndMiscStateAsInt;
#ifdef DEBUG_BOARD_STATE
		BoardState().m_AlphaTestAndMiscState = shadowState.m_AlphaTestAndMiscState;
#endif
	}

	if ( m_CurrentState.m_nFogAndMiscStateAsInt  != shadowState.m_nFogAndMiscStateAsInt )
	{
		bool bSetting = shadowState.m_FogAndMiscState.m_SRGBWriteEnable;
		if ( m_CurrentState.m_FogAndMiscState.m_SRGBWriteEnable != bSetting )
		{
			// ApplySRGBWriteEnable set to true means that the shader is writing linear values.
			if ( ( CurrentState().m_bLinearColorSpaceFrameBufferEnable ) ||
				 ( 
					 HardwareConfig()->NeedsShaderSRGBConversion() && 
					 ( HardwareConfig()->GetDXSupportLevel() >= 92 ) ) ) //in 2b supported devices, we never actually enable SRGB writes, but instead handle the conversion in the pixel shader. But we want all other code to be unaware.
				
			{
				// The shader had better be writing linear values since we can't convert to gamma here.
				// Can't leave this assert here since there are cases where the shader is doing the right thing.
				// This is good to test occasionally to make sure that the shaders are doing the right thing.
				//		Assert( shaderState.m_SRGBWriteEnable );
				
				// render target is linear
				bSetting = false;
				// fog isn't fixed-function with linear frame buffers, so don't bother with that here.
			}
			ShaderAPI()->SetSRGBWrite( bSetting );
		}
		ShaderAPI()->ApplyFogMode( shadowState.m_FogAndMiscState.FogMode(), shadowState.m_FogAndMiscState.m_bVertexFogEnable, bSetting, shadowState.m_FogAndMiscState.m_bDisableFogGammaCorrection );
		m_CurrentState.m_nFogAndMiscStateAsInt = shadowState.m_nFogAndMiscStateAsInt;

#ifdef _DEBUG
		BoardState().m_FogAndMiscState.m_SRGBWriteEnable = shadowState.m_FogAndMiscState.m_SRGBWriteEnable;
		if (g_SpewTransitions)											
		{																
			char buf[128];												
			sprintf( buf, "Apply %s : %d\n", "D3DRS_SRGBWRITEENABLE", shadowState.m_FogAndMiscState.m_SRGBWriteEnable );
			Plat_DebugString(buf);										
		}																
#endif
	}

	if ( m_CurrentState.m_nAlphaBlendStateAsInt != shadowState.m_nAlphaBlendStateAsInt )
	{
		SetRenderState( D3DRS_ALPHABLENDENABLE, shadowState.m_AlphaBlendState.m_AlphaBlendEnable );

		if (shadowState.m_AlphaBlendState.m_AlphaBlendEnable)
		{
			// Set the blend state here...
			SetRenderState( D3DRS_SRCBLEND, shadowState.m_AlphaBlendState.m_SrcBlend );
			SetRenderState( D3DRS_DESTBLEND, shadowState.m_AlphaBlendState.m_DestBlend );
			SetRenderState( D3DRS_BLENDOP, shadowState.m_AlphaBlendState.m_BlendOp );
		}

		SetRenderState( D3DRS_SEPARATEALPHABLENDENABLE, shadowState.m_AlphaBlendState.m_SeparateAlphaBlendEnable );

		if (shadowState.m_AlphaBlendState.m_SeparateAlphaBlendEnable)
		{
			// Set the blend state here...
			SetRenderState( D3DRS_SRCBLENDALPHA, shadowState.m_AlphaBlendState.m_SrcBlendAlpha );
			SetRenderState( D3DRS_DESTBLENDALPHA, shadowState.m_AlphaBlendState.m_DestBlendAlpha );
			SetRenderState( D3DRS_BLENDOPALPHA, shadowState.m_AlphaBlendState.m_BlendOpAlpha );
		}

		m_CurrentState.m_nAlphaBlendStateAsInt = shadowState.m_nAlphaBlendStateAsInt;
#ifdef DEBUG_BOARD_STATE
	// This isn't quite true, but it's necessary for other error checking to work
		BoardState().m_nAlphaBlendStateAsInt = shadowState.m_nAlphaBlendStateAsInt;
#endif
	}

	if ( m_CurrentState.m_ForceDepthFuncEquals || 
		 m_CurrentState.m_bOverrideDepthEnable ||
		 m_CurrentState.m_bOverrideAlphaWriteEnable || 
		 m_CurrentState.m_bOverrideColorWriteEnable )
	{
		PerformShadowStateOverrides();
	}
}

//-----------------------------------------------------------------------------
// Makes the board state match the snapshot
//-----------------------------------------------------------------------------
void CTransitionTable::UseSnapshot( StateSnapshot_t snapshotId, bool bApplyShaderState /*= true*/ )
{
	VPROF("CTransitionTable::UseSnapshot");
	ShadowStateId_t id = m_SnapshotList[snapshotId].m_ShadowStateId;
	if (m_CurrentSnapshotId != snapshotId)
	{
		// apply the shadow state
		if ( m_CurrentShadowId != id )
		{
			SetBoardStateFromShadowState( m_ShadowStateList[id] );
			m_CurrentShadowId = id;
		}
		
		
		m_CurrentSnapshotId = snapshotId;
	}
	// NOTE: This occurs regardless of whether the snapshot changed because it depends
	// on dynamic state (namely, the dynamic vertex + pixel shader index)
	// Followed by things that are not
	if ( bApplyShaderState )
	{
		ApplyShaderState( m_ShadowStateList[id], m_SnapshotList[snapshotId].m_ShaderState );
	}
}


//-----------------------------------------------------------------------------
// Cause the board to match the default state snapshot
//-----------------------------------------------------------------------------
void CTransitionTable::UseDefaultState( )
{
	VPROF("CTransitionTable::UseDefaultState");
	// Need to blat these out because they are tested during transitions
	m_CurrentState.m_AlphaBlendState.m_AlphaBlendEnable = false;
	m_CurrentState.m_AlphaBlendState.m_SrcBlend = D3DBLEND_ONE;
	m_CurrentState.m_AlphaBlendState.m_DestBlend = D3DBLEND_ZERO;
	m_CurrentState.m_AlphaBlendState.m_BlendOp = D3DBLENDOP_ADD;
	SetRenderState( D3DRS_ALPHABLENDENABLE, m_CurrentState.m_AlphaBlendState.m_AlphaBlendEnable );
	SetRenderState( D3DRS_SRCBLEND, m_CurrentState.m_AlphaBlendState.m_SrcBlend );
	SetRenderState( D3DRS_DESTBLEND, m_CurrentState.m_AlphaBlendState.m_DestBlend );
	SetRenderState( D3DRS_BLENDOP, m_CurrentState.m_AlphaBlendState.m_BlendOp );

	m_CurrentState.m_AlphaBlendState.m_SeparateAlphaBlendEnable = false;
	m_CurrentState.m_AlphaBlendState.m_SrcBlendAlpha = D3DBLEND_ONE;
	m_CurrentState.m_AlphaBlendState.m_DestBlendAlpha = D3DBLEND_ZERO;
	m_CurrentState.m_AlphaBlendState.m_BlendOpAlpha = D3DBLENDOP_ADD;
	SetRenderState( D3DRS_SEPARATEALPHABLENDENABLE, m_CurrentState.m_AlphaBlendState.m_SeparateAlphaBlendEnable );
	SetRenderState( D3DRS_SRCBLENDALPHA, m_CurrentState.m_AlphaBlendState.m_SrcBlendAlpha );
	SetRenderState( D3DRS_DESTBLENDALPHA, m_CurrentState.m_AlphaBlendState.m_DestBlendAlpha );
	SetRenderState( D3DRS_BLENDOPALPHA, m_CurrentState.m_AlphaBlendState.m_BlendOpAlpha );

	m_CurrentState.m_nDepthTestStateAsInt = 0;
	m_CurrentState.m_DepthTestState.m_ZEnable = D3DZB_TRUE;
	m_CurrentState.m_DepthTestState.m_ZFunc = (ShaderUtil()->GetConfig().bReverseDepth ^ ReverseDepthOnX360()) ? D3DCMP_GREATEREQUAL : D3DCMP_LESSEQUAL;
	m_CurrentState.m_DepthTestState.m_ZBias = SHADER_POLYOFFSET_DISABLE;
	SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
	SetRenderState( D3DRS_ZFUNC, m_CurrentState.m_DepthTestState.m_ZFunc );

	m_CurrentState.m_nAlphaTestAndMiscStateAsInt = 0;
	m_CurrentState.m_AlphaTestAndMiscState.m_AlphaFunc = D3DCMP_GREATEREQUAL;
	m_CurrentState.m_AlphaTestAndMiscState.m_AlphaRef = 0;
	m_CurrentState.m_AlphaTestAndMiscState.m_FillMode = D3DFILL_SOLID;
	m_CurrentState.m_AlphaTestAndMiscState.m_AlphaTestEnable = false;
	m_CurrentState.m_AlphaTestAndMiscState.m_EnableAlphaToCoverage = false;
	m_CurrentState.m_AlphaTestAndMiscState.m_CullEnable = true;
	
	SetRenderState( D3DRS_ALPHATESTENABLE, m_CurrentState.m_AlphaTestAndMiscState.m_AlphaTestEnable );
	SetRenderState( D3DRS_ALPHAFUNC, m_CurrentState.m_AlphaTestAndMiscState.m_AlphaFunc );
	SetRenderState( D3DRS_ALPHAREF, m_CurrentState.m_AlphaTestAndMiscState.m_AlphaRef );
	ShaderAPI()->ApplyAlphaToCoverage( m_CurrentState.m_AlphaTestAndMiscState.m_EnableAlphaToCoverage );
	ShaderAPI()->ApplyCullEnable( m_CurrentState.m_AlphaTestAndMiscState.m_CullEnable );
	SetRenderState( D3DRS_FILLMODE, m_CurrentState.m_AlphaTestAndMiscState.m_FillMode );

	m_CurrentState.m_FogAndMiscState.m_SRGBWriteEnable = 0;
	if ( D3DRS_SRGBWRITEENABLE != D3DRS_NOTSUPPORTED )
	{
		SetRenderState( D3DRS_SRGBWRITEENABLE, 0 ); 
	}

	int nSamplerCount = ShaderAPI()->GetActualSamplerCount();
	for ( int i = 0; i < nSamplerCount; ++i)
	{
		SetSamplerState( i, D3DSAMP_SRGBTEXTURE, SamplerState(i).m_SRGBReadEnable );

#if 0
		// Set default Fetch4 state on parts which support it
		if ( HardwareConfig()->SupportsFetch4() )
		{
			SetSamplerState( i, ATISAMP_FETCH4, SamplerState(i).m_Fetch4Enable ? ATI_FETCH4_ENABLE : ATI_FETCH4_DISABLE );
		}
#endif
		
#ifdef DX_TO_GL_ABSTRACTION
		SetSamplerState( i, D3DSAMP_SHADOWFILTER, SamplerState(i).m_ShadowFilterEnable );
#endif
	}

	// Disable z overrides...
	m_CurrentState.m_bOverrideDepthEnable = false;
	m_CurrentState.m_bOverrideAlphaWriteEnable = false;
	m_CurrentState.m_bOverrideColorWriteEnable = false;
	m_CurrentState.m_ForceDepthFuncEquals = false;
	m_CurrentState.m_bLinearColorSpaceFrameBufferEnable = false;

	ShaderManager()->SetVertexShader( INVALID_SHADER );
	ShaderManager()->SetPixelShader( INVALID_SHADER );

	m_CurrentSnapshotId = -1;
	m_CurrentShadowId = -1;
}


//-----------------------------------------------------------------------------
// Snapshotted state overrides
//-----------------------------------------------------------------------------
void CTransitionTable::ForceDepthFuncEquals( bool bEnable )
{
	if( bEnable != m_CurrentState.m_ForceDepthFuncEquals )
	{
		m_CurrentState.m_ForceDepthFuncEquals = bEnable;

		if( bEnable )
		{
			SetZFunc( D3DCMP_EQUAL );
		}
		else
		{
			if ( CurrentShadowState() )
			{
				SetZFunc( ( D3DCMPFUNC ) CurrentShadowState()->m_DepthTestState.m_ZFunc );
			}
		}
	}
}

void CTransitionTable::OverrideDepthEnable( bool bEnable, bool bDepthWriteEnable, bool bDepthTestEnable )
{
	if ( bEnable != m_CurrentState.m_bOverrideDepthEnable || 
		( bEnable && ( bDepthWriteEnable != m_CurrentState.m_OverrideZWriteEnable || bDepthTestEnable != m_CurrentState.m_OverrideZTestEnable ) ) )
	{
		m_CurrentState.m_bOverrideDepthEnable = bEnable;
		m_CurrentState.m_OverrideZWriteEnable = bDepthWriteEnable;
		m_CurrentState.m_OverrideZTestEnable = bDepthTestEnable;

		if ( m_CurrentState.m_bOverrideDepthEnable )
		{
			SetZEnable( D3DZB_TRUE );
			SetRenderState( D3DRS_ZWRITEENABLE, m_CurrentState.m_OverrideZWriteEnable ? TRUE : FALSE );
			if ( !m_CurrentState.m_OverrideZTestEnable )
			{
				SetRenderState( D3DRS_ZFUNC, D3DCMP_ALWAYS );
			}
#if defined( _X360 )
			//SetRenderState( D3DRS_HIZWRITEENABLE, m_CurrentState.m_OverrideZWriteEnable ? D3DHIZ_AUTOMATIC : D3DHIZ_DISABLE );
#endif
		}
		else
		{
			if ( CurrentShadowState() )
			{
				SetZEnable( (D3DZBUFFERTYPE ) CurrentShadowState()->m_DepthTestState.m_ZEnable );
				SetRenderState( D3DRS_ZWRITEENABLE, CurrentShadowState()->m_DepthTestState.m_ZWriteEnable );
				SetRenderState( D3DRS_ZFUNC, CurrentShadowState()->m_DepthTestState.m_ZFunc );
#if defined( _X360 )
				//SetRenderState( D3DRS_HIZWRITEENABLE, CurrentShadowState()->m_ZWriteEnable ? D3DHIZ_AUTOMATIC : D3DHIZ_DISABLE );
#endif
			}
		}
	}
}

void CTransitionTable::OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable )
{
	if ( bOverrideEnable != m_CurrentState.m_bOverrideAlphaWriteEnable )
	{
		m_CurrentState.m_bOverrideAlphaWriteEnable = bOverrideEnable;
		m_CurrentState.m_bOverriddenAlphaWriteValue = bAlphaWriteEnable;

		DWORD dwSetValue = m_CurrentState.m_ColorWriteEnable;
		if ( m_CurrentState.m_bOverrideAlphaWriteEnable )
		{			
			if( m_CurrentState.m_bOverriddenAlphaWriteValue )
			{
				dwSetValue |= D3DCOLORWRITEENABLE_ALPHA;
			}
			else
			{
				dwSetValue &= ~D3DCOLORWRITEENABLE_ALPHA;
			}
		}
		else
		{
			if ( CurrentShadowState() )
			{
				//probably being paranoid, but only copy the alpha flag from the shadow state
				dwSetValue &= ~D3DCOLORWRITEENABLE_ALPHA;
				dwSetValue |= CurrentShadowState()->m_DepthTestState.m_ColorWriteEnable & D3DCOLORWRITEENABLE_ALPHA;
			}
		}

		if( dwSetValue != m_CurrentState.m_ColorWriteEnable )
		{
			m_CurrentState.m_ColorWriteEnable = dwSetValue;
			SetRenderState( D3DRS_COLORWRITEENABLE, m_CurrentState.m_ColorWriteEnable );
		}
	}
}

void CTransitionTable::OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable )
{
	if ( bOverrideEnable != m_CurrentState.m_bOverrideColorWriteEnable )
	{
		m_CurrentState.m_bOverrideColorWriteEnable = bOverrideEnable;
		m_CurrentState.m_bOverriddenColorWriteValue = bColorWriteEnable;

		DWORD dwSetValue = m_CurrentState.m_ColorWriteEnable;
		if ( m_CurrentState.m_bOverrideColorWriteEnable )
		{			
			if( m_CurrentState.m_bOverriddenColorWriteValue )
			{
				dwSetValue |= (D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
			}
			else
			{
				dwSetValue &= ~(D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
			}
		}
		else
		{
			if ( CurrentShadowState() )
			{
				//probably being paranoid, but only copy the alpha flag from the shadow state
				dwSetValue &= ~(D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
				dwSetValue |= CurrentShadowState()->m_DepthTestState.m_ColorWriteEnable & (D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
			}
		}

		if( dwSetValue != m_CurrentState.m_ColorWriteEnable )
		{
			m_CurrentState.m_ColorWriteEnable = dwSetValue;
			SetRenderState( D3DRS_COLORWRITEENABLE, m_CurrentState.m_ColorWriteEnable );
		}
	}
}

void CTransitionTable::EnableLinearColorSpaceFrameBuffer( bool bEnable )
{
	if ( m_CurrentState.m_bLinearColorSpaceFrameBufferEnable != bEnable && CurrentShadowState() )
	{
		m_CurrentState.m_bLinearColorSpaceFrameBufferEnable = bEnable;
		ApplySRGBWriteEnable( *CurrentShadowState() );	
	}
}

//-----------------------------------------------------------------------------
// Perform state block overrides
//-----------------------------------------------------------------------------
void CTransitionTable::PerformShadowStateOverrides( )
{
	VPROF("CTransitionTable::PerformShadowStateOverrides");
	// Deal with funky overrides here, because the state blocks can't...

	if ( m_CurrentState.m_ForceDepthFuncEquals )
	{
		SetZFunc( D3DCMP_EQUAL );
	}

	if ( m_CurrentState.m_bOverrideDepthEnable )
	{
		SetZEnable( D3DZB_TRUE );
		SetRenderState( D3DRS_ZWRITEENABLE, m_CurrentState.m_OverrideZWriteEnable ? TRUE : FALSE );
		if ( !m_CurrentState.m_OverrideZTestEnable )
		{
			SetRenderState( D3DRS_ZFUNC, D3DCMP_ALWAYS );
		}
#if defined( _X360 )
		//SetRenderState( D3DRS_HIZWRITEENABLE, m_CurrentState.m_OverrideZWriteEnable ? D3DHIZ_AUTOMATIC : D3DHIZ_DISABLE );
#endif
	}

	if ( m_CurrentState.m_bOverrideAlphaWriteEnable )
	{
		DWORD dwSetValue = m_CurrentState.m_ColorWriteEnable & ~D3DCOLORWRITEENABLE_ALPHA;
		dwSetValue |= m_CurrentState.m_bOverriddenAlphaWriteValue ? D3DCOLORWRITEENABLE_ALPHA : 0;
		if ( dwSetValue != m_CurrentState.m_ColorWriteEnable )
		{
			m_CurrentState.m_ColorWriteEnable = dwSetValue;
			SetRenderState( D3DRS_COLORWRITEENABLE, m_CurrentState.m_ColorWriteEnable );
		}
	}

	if ( m_CurrentState.m_bOverrideColorWriteEnable )
	{
		DWORD dwSetValue = m_CurrentState.m_ColorWriteEnable & ~(D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
		dwSetValue |= m_CurrentState.m_bOverriddenColorWriteValue ? (D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE) : 0;
		if ( dwSetValue != m_CurrentState.m_ColorWriteEnable )
		{
			m_CurrentState.m_ColorWriteEnable = dwSetValue;
			SetRenderState( D3DRS_COLORWRITEENABLE, m_CurrentState.m_ColorWriteEnable );
		}
	}
}
