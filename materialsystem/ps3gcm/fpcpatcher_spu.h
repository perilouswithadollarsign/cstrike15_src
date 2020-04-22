//========= Copyright © Valve Corporation, All rights reserved. ====//
//
// Fragment Program Constant Patcher: an SPU implementation, V0
//
#ifndef PS3_SHADER_CONSTANT_PATCH_SPU_HDR
#define PS3_SHADER_CONSTANT_PATCH_SPU_HDR

#ifdef _PS3 

#include "vjobs/fpcpatch_shared.h"
#include <cg/cg.h>
#include <cg/cgBinary.h>

#ifdef _DEBUG
//#define DEBUG_FPC_PATCHER
#endif


class FpcPatchState
{
public:
	job_fpcpatch::FpcPatchState_t * m_pSharedState;
	uint32 m_nEndOfJournalIdx;	// this is PPU-side variable only, written by PPU only
	fltx4 * GetBufferStart(){ return m_pSharedState->GetBufferStart() ; }           // the buffer start address
	uint32 m_nBufferMask;       // the number of Qwords in the buffer
	
	//#ifdef _DEBUG
	//int m_nRangesAdded;
	//#endif
public:
	FpcPatchState(){m_pSharedState = NULL;}

	void Init( job_fpcpatch::FpcPatchState_t * pSharedState, uint32 nBufferQwords );
	void Reset();
	uint AddRange( uint32 nStart, uint32 nCount, const float * pData );
	
	void GetSyncState( fltx4 * pRegisters );
protected:
	fltx4 * AddInternalPtr()
	{
		fltx4 * pOut = GetBufferStart() + ( m_nEndOfJournalIdx & m_nBufferMask );
		m_nEndOfJournalIdx++;
		return pOut;
	}
	void AddInternal( const fltx4 f4 )
	{
		*AddInternalPtr() = f4;
	}
	inline void AddInternalBlock( const void *pBlock, const uint32 numFltx4s )
	{
		// Fit the first portion until the end of the buffer, second portion at start
		uint32 const nCurrentIdx = ( m_nEndOfJournalIdx & m_nBufferMask ); // the start index to copy to
		uint32 const numFltx4sUntilEnd = ( -nCurrentIdx ) & m_nBufferMask; // number of fltx4's from the nCurrentIdx to the end of the current buffer ring
		uint32 const numFirstCopy = MIN( numFltx4sUntilEnd, numFltx4s );   // number of fltx4's to copy first
		memcpy( GetBufferStart() + nCurrentIdx, pBlock, numFirstCopy * sizeof( fltx4 ) );
		memcpy( GetBufferStart(), ( ( fltx4* ) pBlock ) + numFirstCopy, ( numFltx4s - numFirstCopy ) * sizeof( fltx4 ) );
		m_nEndOfJournalIdx += numFltx4s;
	}
};


struct IDirect3DPixelShader9 ;
class CFragmentProgramConstantPatcher_SPU
{
public:
	CFragmentProgramConstantPatcher_SPU();
	~CFragmentProgramConstantPatcher_SPU();
	void InitLocal( void *pBuffer, uint nSize );
	void Shutdown();
	
	// semantics should match cgGLSetFragmentRegisterBlock()
	void SetFragmentRegisterBlock( uint StartRegister, uint Vector4fCount, const float* pConstantData );
	
	// semantics of cgGLBindProgram( pPixelShader->m_pixProgram->m_CGprogram )
	void BindProgram( const CgBinaryProgram *prog );
	void BindProgram( const struct IDirect3DPixelShader9 * prog );

	void BeginScene();
	void EndScene();
	
	//job_fpcpatch::FpcPatchState_t * GetSharedState(){return m_state.m_pSharedState; }

	uint GetStateEndOfJournalIdx() { return m_state.m_nEndOfJournalIdx; }
	uint GetJournalCapacity() const { return m_state.m_nBufferMask + 1; }
	int GetJournalSpaceUsedSince( uint nMarker )const{ return int( m_state.m_nEndOfJournalIdx - nMarker ); }
	int GetJournalSpaceLeftSince( uint nMarker )const{ return int( ( m_state.m_nBufferMask + 1 ) - ( m_state.m_nEndOfJournalIdx - nMarker ) ); }
protected:	
	void ResetPut();

	void * FpcPatch( const struct CgBinaryProgram * prog, void * pFragmentProgramDestination, uint32 * pJts );
	void FpcPatch2( const job_fpcpatch2::FpHeader_t * psh, uint nFpDmaSize, void *pPatchedProgram, uint32 * pJts );
protected:
	friend class CSpuGcm;

	FpcPatchState m_state;

	uint32* m_pBuffer, *m_pBufferEnd;
	int m_nIoOffsetDelta; // m_pBuffer + m_nIoOffsetDelta == IO offset usable by RSX
	uint32 * m_pPutFragmentProgram;
	uint m_nFpcPatchCounterAtBeginScene; // used for timing
	uint m_nFpcPatchCounterOfLastSyncJob;
	uint m_nBufferLocation;// CELL_GCM_LOCATION_MAIN
	uint m_nFpcPatchCounter, m_nFpcPatchSyncMask;
	//uint m_nStartRangesAfterLastSync; // this is the index used to upload only the useful constants to SPU
	bool m_isBufferPassedIn;
	bool m_bFpcPatchOnPpu, m_bEnableSPU;
	
#ifdef DEBUG_FPC_PATCHER
	void ValidatePatchedProgram( const CgBinaryProgram *prog, void * pPatchedUcode );
	fltx4 *m_pSyncState;
	bool m_bTestAlwaysStateSync;
	bool m_bSync; // don't use JTS, but just patch synchronously (may be more stable with GPAD)
#endif


};

extern CFragmentProgramConstantPatcher_SPU g_pixelShaderPatcher;	// Patches pixel shader constants


#endif

#endif
