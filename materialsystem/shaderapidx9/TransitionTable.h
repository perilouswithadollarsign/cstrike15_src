//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef TRANSITION_TABLE_H
#define TRANSITION_TABLE_H

#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"
#include "shadershadowdx8.h"
#include "utlsortvector.h"
#include "checksum_crc.h"
#include "shaderapi/ishaderapi.h"

// Required for DEBUG_BOARD_STATE
#include "shaderapidx8_global.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
struct IDirect3DStateBlock9;

//-----------------------------------------------------------------------------
// Types related to transition table entries
//-----------------------------------------------------------------------------
typedef void (*ApplyStateFunc_t)( const ShadowState_t& shadowState, int arg );


//-----------------------------------------------------------------------------
// The DX8 implementation of the transition table
//-----------------------------------------------------------------------------
class CTransitionTable
{
public:
	struct CurrentSamplerState_t
	{
		bool					m_SRGBReadEnable;
		bool					m_Fetch4Enable;
		bool					m_ShadowFilterEnable;
	};
	struct CurrentState_t
	{
		// Everything in this 'CurrentState' structure is a state whose value we don't care about
		// under certain circumstances, (which therefore can diverge from the shadow state),
		// or states which we override in the dynamic pass.

		// Depth testing states
		union
		{
			DepthTestState_t m_DepthTestState;
			DepthTestState_t::UIntAlias m_nDepthTestStateAsInt;
		};

		// alpha testijng and misc state
		union
		{
			AlphaTestAndMiscState_t m_AlphaTestAndMiscState;
			AlphaTestAndMiscState_t::UIntAlias m_nAlphaTestAndMiscStateAsInt;
		};

		union
		{
			FogAndMiscState_t m_FogAndMiscState;
			FogAndMiscState_t::UIntAlias m_nFogAndMiscStateAsInt;
		};

		union
		{
			AlphaBlendState_t m_AlphaBlendState;
			AlphaBlendState_t::UIntAlias m_nAlphaBlendStateAsInt;
		};


		bool				m_ForceDepthFuncEquals:1;
		bool				m_bOverrideDepthEnable:1;

		bool				m_bOverrideAlphaWriteEnable:1;
		bool				m_bOverrideColorWriteEnable:1;

		bool				m_bOverriddenAlphaWriteValue:1;
		bool				m_bOverriddenColorWriteValue:1;


		DWORD				m_ColorWriteEnable;
		bool				m_OverrideZWriteEnable;
		bool				m_OverrideZTestEnable;

		bool				m_bLinearColorSpaceFrameBufferEnable;

		// Texture stage state
		CurrentSamplerState_t m_SamplerState[MAX_SAMPLERS];
	};

public:
	// constructor, destructor
	CTransitionTable( );
	virtual ~CTransitionTable();

	// Initialization, shutdown
	bool Init( );
	void Shutdown( );

	// Resets the snapshots...
	void Reset();

	// Takes a snapshot
	StateSnapshot_t TakeSnapshot( );

	// Take startup snapshot
	void TakeDefaultStateSnapshot( );

	ShadowShaderState_t  *GetShaderShadowState( StateSnapshot_t snapshotId )
	{
		return  &m_SnapshotList[snapshotId].m_ShaderState;
	}

	// Makes the board state match the snapshot
	void UseSnapshot( StateSnapshot_t snapshotId, bool bApplyShaderState = true );

	// Cause the board to match the default state snapshot
	void UseDefaultState();

	// Snapshotted state overrides
	void ForceDepthFuncEquals( bool bEnable );
	void OverrideDepthEnable( bool bEnable, bool bDepthWriteEnable, bool bDepthTestEnable );
	void OverrideAlphaWriteEnable( bool bOverrideEnable, bool bAlphaWriteEnable );
	void OverrideColorWriteEnable( bool bOverrideEnable, bool bColorWriteEnable );
	void EnableLinearColorSpaceFrameBuffer( bool bEnable );

	// Returns a particular snapshot
	const ShadowState_t &GetSnapshot( StateSnapshot_t snapshotId ) const;
	const ShadowShaderState_t &GetSnapshotShader( StateSnapshot_t snapshotId ) const;

	// Gets the current shadow state
	const ShadowState_t *CurrentShadowState() const;
	const ShadowShaderState_t *CurrentShadowShaderState() const;

	// Return the current shapshot
	int CurrentSnapshot() const { return m_CurrentSnapshotId; }

	CurrentState_t& CurrentState() { return m_CurrentState; }

	void SetShadowDepthBiasValuesDirty( bool bDirty ) { m_bShadowDepthBiasValuesDirty = bDirty; }

#ifdef DEBUG_BOARD_STATE
	ShadowState_t& BoardState() { return m_BoardState; }
	ShadowShaderState_t& BoardShaderState() { return m_BoardShaderState; }
#endif

	// The following are meant to be used by the transition table only
public:
	// Applies alpha blending
	void ApplyAlphaBlend( const ShadowState_t& state );

	// Separate alpha blend
	void ApplySeparateAlphaBlend( const ShadowState_t& state );
	void ApplyAlphaTest( const ShadowState_t& state );
	void ApplyDepthTest( const ShadowState_t& state );

	// Applies alpha texture op
	void ApplyColorTextureStage( const ShadowState_t& state, int stage );
	void ApplyAlphaTextureStage( const ShadowState_t& state, int stage );

	void ApplySRGBWriteEnable( const ShadowState_t& state );
private:
	enum
	{
		INVALID_TRANSITION_OP = 0xFFFFFF
	};

	typedef short ShadowStateId_t;

	// For the transition table
	struct TransitionList_t
	{
		unsigned int m_FirstOperation : 24;
		unsigned int m_NumOperations : 8;
	};

	union TransitionOp_t
	{
		unsigned char m_nBits;
		struct
		{
			unsigned char m_nOpCode : 7;
			unsigned char m_bIsTextureCode : 1;
		} m_nInfo;
	};

	struct SnapshotShaderState_t
	{
		ShadowShaderState_t m_ShaderState;
		ShadowStateId_t m_ShadowStateId;
		unsigned short m_nReserved;	// Pad to 2 ints
		unsigned int m_nReserved2;
	};

	struct ShadowStateDictEntry_t
	{
		CRC32_t	m_nChecksum;
		ShadowStateId_t m_nShadowStateId;
	};

	struct SnapshotDictEntry_t
	{
		CRC32_t	m_nChecksum;
		StateSnapshot_t m_nSnapshot;
	};

	class ShadowStateDictLessFunc
	{
	public:
		bool Less( const ShadowStateDictEntry_t &src1, const ShadowStateDictEntry_t &src2, void *pCtx );
	};

	class SnapshotDictLessFunc
	{
	public:
		bool Less( const SnapshotDictEntry_t &src1, const SnapshotDictEntry_t &src2, void *pCtx );
	};

	class UniqueSnapshotLessFunc
	{
	public:
		bool Less( const TransitionList_t &src1, const TransitionList_t &src2, void *pCtx );
	};

	CurrentSamplerState_t &SamplerState( int stage ) { return m_CurrentState.m_SamplerState[stage]; }
	const CurrentSamplerState_t &SamplerState( int stage ) const { return m_CurrentState.m_SamplerState[stage]; }

	// creates state snapshots
	ShadowStateId_t  CreateShadowState( const ShadowState_t &currentState );
	StateSnapshot_t  CreateStateSnapshot( ShadowStateId_t shadowStateId, const ShadowShaderState_t& currentShaderState );

	// finds state snapshots
	ShadowStateId_t FindShadowState( const ShadowState_t& currentState ) const;
	StateSnapshot_t FindStateSnapshot( ShadowStateId_t id, const ShadowShaderState_t& currentState ) const;

	// Finds identical transition lists
	unsigned int FindIdenticalTransitionList( unsigned int firstElem, 
		unsigned short numOps, unsigned int nFirstTest ) const;

	// Checks if a state is valid
	bool TestShadowState( const ShadowState_t& state, const ShadowShaderState_t &shaderState );

	// Perform state block overrides
	void PerformShadowStateOverrides( );

	// Apply shader state (stuff that doesn't lie in the transition table)
	void ApplyShaderState( const ShadowState_t &shadowState, const ShadowShaderState_t &shaderState );

	// State setting methods
	void SetZEnable( D3DZBUFFERTYPE nEnable );
	void SetZFunc( D3DCMPFUNC nCmpFunc );

	void SetBoardStateFromShadowState( ShadowState_t const &shadowState );

private:
	// Sets up the default state
	StateSnapshot_t m_DefaultStateSnapshot;
	TransitionList_t m_DefaultTransition;
	ShadowState_t m_DefaultShadowState;
	
	// The current snapshot id
	ShadowStateId_t m_CurrentShadowId;
	StateSnapshot_t m_CurrentSnapshotId;

	// Maintains a list of all used snapshot transition states
	CUtlVector< ShadowState_t >	m_ShadowStateList;

	// Lookup table for fast snapshot finding
	CUtlSortVector< ShadowStateDictEntry_t, ShadowStateDictLessFunc >	m_ShadowStateDict;

	// The snapshot transition table
	CUtlVector< CUtlVector< TransitionList_t > >	m_TransitionTable;

	// List of unique transitions
	CUtlSortVector< TransitionList_t, UniqueSnapshotLessFunc >	m_UniqueTransitions;

	// Stores all state transition operations
	CUtlVector< TransitionOp_t > m_TransitionOps;

	// Stores all state for a particular snapshot
	CUtlVector< SnapshotShaderState_t >	m_SnapshotList;

	// Lookup table for fast snapshot finding
	CUtlSortVector< SnapshotDictEntry_t, SnapshotDictLessFunc >	m_SnapshotDict;

	// The current board state.
	CurrentState_t m_CurrentState;
	
	bool m_bShadowDepthBiasValuesDirty;

#ifdef DEBUG_BOARD_STATE
	// Maintains the total shadow state
	ShadowState_t m_BoardState;
	ShadowShaderState_t m_BoardShaderState;
#endif
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline const ShadowState_t &CTransitionTable::GetSnapshot( StateSnapshot_t snapshotId ) const
{
	Assert( (snapshotId >= 0) && (snapshotId < m_SnapshotList.Count()) );
	return m_ShadowStateList[m_SnapshotList[snapshotId].m_ShadowStateId];
}

inline const ShadowShaderState_t &CTransitionTable::GetSnapshotShader( StateSnapshot_t snapshotId ) const
{
	Assert( (snapshotId >= 0) && (snapshotId < m_SnapshotList.Count()) );
	return m_SnapshotList[snapshotId].m_ShaderState;
}

inline const ShadowState_t *CTransitionTable::CurrentShadowState() const
{
	if ( m_CurrentShadowId == -1 )
		return NULL;

	Assert( (m_CurrentShadowId >= 0) && (m_CurrentShadowId < m_ShadowStateList.Count()) );
	return &m_ShadowStateList[m_CurrentShadowId];
}

inline const ShadowShaderState_t *CTransitionTable::CurrentShadowShaderState() const
{
	if ( m_CurrentShadowId == -1 )
		return NULL;

	Assert( (m_CurrentShadowId >= 0) && (m_CurrentShadowId < m_ShadowStateList.Count()) );
	return &m_SnapshotList[m_CurrentShadowId].m_ShaderState;
}


#endif // TRANSITION_TABLE_H
