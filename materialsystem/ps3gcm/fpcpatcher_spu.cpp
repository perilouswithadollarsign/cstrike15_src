//========= Copyright © Valve Corporation, All rights reserved. ====//
#include "tier0/platform.h"
#ifdef _PS3
#include "dxabstract.h"

#include <sys/memory.h>
#include "ps3/spugcm_shared.h"
#include "fpcpatcher_spu.h"
#include "cg/cg.h"
#include "cg/cgBinary.h"
#include "vjobs_interface.h"
#include "tier0/hardware_clock_fast.h"
#include "vjobs/fpcpatch_shared.h"
#include "vjobs/root.h"
#include "ps3/vjobutils.h"
#include "tier0/microprofiler.h"
#include "ps3/ps3_gcm_config.h"
#include "spugcm.h"

enum
{
	PROFILE_SCE_VP_RSX = 7003,
	PROFILE_SCE_FP_RSX = 7004
};

#define GCM_MUST_SUCCEED( FUNC, ... ) do { int nError = FUNC(__VA_ARGS__); if( nError != CELL_OK ) { Error( "Error 0x%X in " #FUNC ", %s:%d\n", nError, __FILE__, __LINE__ ); } } while( 0 )
DEFINE_LOGGING_CHANNEL_NO_TAGS( LOG_VJOBS, "VJOBS" );

CFragmentProgramConstantPatcher_SPU g_pixelShaderPatcher;	// Patches pixel shader constants


CMicroProfiler g_mpBindProgram, g_mpFpcPatch2;
// debug only
CFragmentProgramConstantPatcher_SPU::CFragmentProgramConstantPatcher_SPU()
{
	m_pBuffer = m_pBufferEnd = NULL;
	m_nIoOffsetDelta = 0; // m_pBuffer + m_nIoOffsetDelta == IO offset usable by RSX

	m_pPutFragmentProgram = NULL;
#ifdef DEBUG_FPC_PATCHER
	m_bSync = ( CommandLine()->FindParm( "-fpcpsync" ) != 0 );
#endif
}


void CFragmentProgramConstantPatcher_SPU::InitLocal( void *pBuffer, uint nSize )
{
	m_nFpcPatchCounter				= 0;
	m_nFpcPatchCounterOfLastSyncJob = 0;

	//cellGcmSetDebugOutputLevel( CELL_GCM_DEBUG_LEVEL2 );
	const uint nOverfetchGuard = 1024; // RSX front end prefetches up to 4k, but 1k is ( should be ) enough to avoid overfetch crashes
	const uint nStateBufferQwords = 1 << 12; // make space for at least 8 full batches of constants...
	uint nPatchStateBufferSize = ( sizeof( job_fpcpatch::FpcPatchState_t ) + sizeof( fltx4 ) * nStateBufferQwords );
	uint32 nBufferIoOffset;
	
	m_bFpcPatchOnPpu = ( 0 != CommandLine()->FindParm( "-fpcpatchonppu" ) );
#ifdef DEBUG_FPC_PATCHER
	m_bTestAlwaysStateSync = ( 0 != CommandLine()->FindParm( "-fpcpstatesync" ) );
#endif
	m_bEnableSPU = true;
	m_nFpcPatchSyncMask = 0;

	// use this passed buffer (probably from local memory) for the patched stuff
	m_pBuffer = ( uint32* ) pBuffer;
	m_pBufferEnd = ( uint32* ) ( uintp( pBuffer ) + nSize );
	m_nBufferLocation = CELL_GCM_LOCATION_LOCAL;
	m_isBufferPassedIn = true;
	m_state.Init( ( job_fpcpatch::FpcPatchState_t* )MemAlloc_AllocAligned( nPatchStateBufferSize, 128 ), nStateBufferQwords );
	GCM_MUST_SUCCEED( cellGcmAddressToOffset, m_pBuffer, &nBufferIoOffset );

#ifdef DBGFLAG_ASSERT
	uint32 nBufferIoOffsetCheck;
	GCM_MUST_SUCCEED( cellGcmAddressToOffset, m_pBuffer, &nBufferIoOffsetCheck );
	Assert( nBufferIoOffsetCheck == nBufferIoOffset );
	
	Assert( !( nBufferIoOffsetCheck & 0x7F ) );
	
	for( uint nOffset = 0; nOffset < nSize; nOffset += 128 )
	{
		GCM_MUST_SUCCEED( cellGcmAddressToOffset, ((uint8*)m_pBuffer) + nOffset, &nBufferIoOffsetCheck );
		Assert( nBufferIoOffsetCheck == nBufferIoOffset + nOffset );
	}
#endif

	m_nIoOffsetDelta = nBufferIoOffset - uintp( m_pBuffer );

#ifdef DEBUG_FPC_PATCHER
	m_pSyncState = ( fltx4* ) MemAlloc_AllocAligned( sizeof( fltx4 ) * job_fpcpatch::MAX_VIRTUAL_CONST_COUNT, 16 );
	V_memset( m_pSyncState, 0xCD, sizeof( fltx4 ) * job_fpcpatch::MAX_VIRTUAL_CONST_COUNT );
	V_memset( m_state.m_pSharedState->m_reg, 0xCD, sizeof( fltx4 ) * job_fpcpatch::MAX_VIRTUAL_CONST_COUNT );
#endif
	ResetPut();
	//cellGcmSetDebugOutputLevel( CELL_GCM_DEBUG_LEVEL0 );
}


void CFragmentProgramConstantPatcher_SPU::Shutdown()
{
}



void CFragmentProgramConstantPatcher_SPU::ResetPut()
{
	m_pPutFragmentProgram = m_pBufferEnd; // reserved word for the count of constants to set
}

CFragmentProgramConstantPatcher_SPU::~CFragmentProgramConstantPatcher_SPU()
{
	if( m_isBufferPassedIn )
	{
		MemAlloc_FreeAligned( m_state.m_pSharedState );
	}
	else
	{
		sys_memory_free( ( sys_addr_t )m_pBuffer );
	}
#ifdef DEBUG_FPC_PATCHER
	MemAlloc_FreeAligned( m_pSyncState );
#endif
}


void CFragmentProgramConstantPatcher_SPU::BeginScene()
{
	m_nFpcPatchCounterAtBeginScene = m_nFpcPatchCounter;
	// we shouldn't have in-flight SPU jobs by now.. should we?
	Assert( uint( g_spuGcmShared.m_nFpcpStartRangesAfterLastSync - m_state.m_pSharedState->m_nStartRanges ) <= m_state.m_pSharedState->m_nBufferMask + 1 );
}


void CFragmentProgramConstantPatcher_SPU::EndScene()
{
	#if ENABLE_MICRO_PROFILER > 0
	uint nPatchCounter = m_nFpcPatchCounter - m_nFpcPatchCounterAtBeginScene;
	extern bool g_bDxMicroProfile;
	if( g_bDxMicroProfile && nPatchCounter )
	{
		g_mpBindProgram.PrintAndReset( "[BindProgram] " );
		g_mpFpcPatch2  .PrintAndReset( "[FpcPatch2]   " );
	}
	#endif
}

job_fpcpatch2::FpHeader_t g_nullFpHeader = {0,0,0,0};

// semantics should match cgGLSetFragmentRegisterBlock()
void CFragmentProgramConstantPatcher_SPU::SetFragmentRegisterBlock( uint nStartRegister, uint nVector4fCount, const float * pConstantData )
{
#ifndef _CERT
	if ( nStartRegister >= job_fpcpatch::MAX_VIRTUAL_CONST_COUNT || nStartRegister + nVector4fCount > job_fpcpatch::MAX_VIRTUAL_CONST_COUNT )
		Error( "Invalid Fragment Register Block Range %u..%u\n", nStartRegister, nStartRegister + nVector4fCount );
#endif

#ifdef DEBUG_FPC_PATCHER
	if( m_bSync )
	{	
		fltx4 reg[job_fpcpatch::MAX_VIRTUAL_CONST_COUNT];
		m_state.GetSyncState( reg );
		Assert( !V_memcmp( m_pSyncState, reg, sizeof( fltx4 ) * job_fpcpatch::MAX_VIRTUAL_CONST_COUNT ) );
	}
	uint nEnd = m_state.m_nEndOfJournalIdx;
#endif

	// we have 4 DMA elements ( 2..6 ) to fit the constant buffer; the 1st element may have to be as small as 16 bytes.
	// this leaves the max constant buffer size 4 * 16kb + 16 bytes
	const uint nMaxUploadRangeBeforeStateSync = ( 32 * 1024 ) / sizeof( fltx4 );
	uint numUploadRangeQwords = m_state.m_nEndOfJournalIdx - g_spuGcmShared.m_nFpcpStartRangesAfterLastSync;
	///////////////////////////////////////////////////////////////////////////
	//
	//   PREPATCH MUST BE DONE IN (CTXFLUSH OR) DRAW JOB FROM NOW ON!!! g_spuGcmShared.m_nFpcpStartRangesAfterLastSync IS SYNCHRONOUS AND CORRECT THERE
	//
	//////////////////////////////////////////////////////////////////////////
	
	
/*
	bool bPrePatch = nVector4fCount + 1 + numUploadRangeQwords > nMaxUploadRangeBeforeStateSync;
	if( bPrePatch )
	{
		// force state sync now
		if( g_spuGcmShared.m_enableStallWarnings )
		{
			Warning( "PPU-SPU Wait for RSX. SetFragmentRegisterBlock: Forced to set state on PPU, %u vectors, %u qwords in history. This is slow fallback path.\n", nVector4fCount, numUploadRangeQwords );
		}
		FpcPatch2( &g_nullFpHeader, sizeof( g_nullFpHeader ), NULL, NULL );
	}

*/
	if( uint nAttempts = m_state.AddRange( nStartRegister, nVector4fCount, pConstantData ) )
	{
		if( g_spuGcmShared.m_enableStallWarnings )
		{
			Warning( "PPU-SPU Wait for RSX. SetFragmentRegisterBlock: Stall, %d spins. Waiting for more memory; %d qwords, %d jobs buffered up\n", nAttempts, m_state.m_nEndOfJournalIdx - m_state.m_pSharedState->m_nStartRanges, g_spuGcmShared.m_nFpcPatchCounter - m_state.m_pSharedState->m_nThisStatePatchCounter );
		}
	}
	
#ifdef DEBUG_FPC_PATCHER
	if( m_bTestAlwaysStateSync && !bPrePatch )
	{
		FpcPatch2( &g_nullFpHeader, sizeof( g_nullFpHeader ), NULL, NULL );
	}

	V_memcpy( m_pSyncState + nStartRegister, pConstantData, nVector4fCount * sizeof( fltx4 ) );
	if( m_bSync )
	{	
		fltx4 reg[job_fpcpatch::MAX_VIRTUAL_CONST_COUNT];
		m_state.GetSyncState( reg );
		Assert( !V_memcmp( m_pSyncState, reg, sizeof( fltx4 ) * job_fpcpatch::MAX_VIRTUAL_CONST_COUNT ) );
	}
#endif
}

//volatile int g_nDebugStage = 0;

//
// Match the semantics of cgGLBindProgram()
// There are 2 formats of fragment shaders, see SDK docs "2. 2 Cg Compiler Options" and
//   in Cg Compiler User's Guide:
//      "7. 2 NV Binary Shader Format (VPO and FPO)"
//      "7. 4 Cgb File Format Specification"
//

void CFragmentProgramConstantPatcher_SPU::BindProgram( const struct IDirect3DPixelShader9 * psh )
{
	MICRO_PROFILE( g_mpBindProgram );

	const job_fpcpatch2::FpHeader_t * prog = psh->m_data.m_eaFp;
	uint32 nFragmentProgramOffset = uintp( m_pPutFragmentProgram ) + m_nIoOffsetDelta;

	g_spuGcmShared.m_fpcpRing.UnlockRsxMemoryForSpu();
	m_pPutFragmentProgram = ( uint32* )g_spuGcmShared.m_fpcpRing.LockRsxMemoryForSpu( &g_spuGcmShared.m_fpcpJobChain, prog->m_nUcodeSize );
	nFragmentProgramOffset = uintp( m_pPutFragmentProgram ) - uintp( g_ps3gcmGlobalState.m_pLocalBaseAddress );
	if( !IsCert() && nFragmentProgramOffset >= g_ps3gcmGlobalState.m_nLocalSize )
	{
		Error( "Fragment program Ucode buffer offset 0x%X is at unexpected address not in local memory\n", nFragmentProgramOffset );
	}

	if ( !IsCert() && ( m_pPutFragmentProgram < m_pBuffer || m_pPutFragmentProgram >= m_pBufferEnd ) )
	{
		Error( "Fragment Program UCode buffer overflow.\n" );
	}
	
#ifdef DEBUG_FPC_PATCHER
	if( m_bSync )
	{	
		fltx4 reg[job_fpcpatch::MAX_VIRTUAL_CONST_COUNT];
		m_state.GetSyncState( reg );
		Assert( !V_memcmp( m_pSyncState, reg, sizeof( fltx4 ) * job_fpcpatch::MAX_VIRTUAL_CONST_COUNT ) );
	}
#endif

	uint nTexControls = prog->m_nTexControls;

	// set jump to self
	GCM_CTX_RESERVE( 7 + 2 * nTexControls );
	uint32 * pJts = NULL;

	FpcPatch2( prog, psh->m_data.m_nFpDmaSize, m_pPutFragmentProgram, pJts );

	CELL_GCM_METHOD_SET_SHADER_CONTROL( GCM_CTX->current, prog->m_nShaderControl0 ); // +2
	CELL_GCM_METHOD_SET_SHADER_PROGRAM( GCM_CTX->current, m_nBufferLocation + 1, ( nFragmentProgramOffset & 0x1fffffff ) ); // +2
	CELL_GCM_METHOD_SET_VERTEX_ATTRIB_OUTPUT_MASK( GCM_CTX->current, psh->m_data.m_attributeInputMask | 0x20 );  // +2
	V_memcpy( GCM_CTX->current, prog->GetTexControls(), nTexControls * sizeof( uint32 ) * 2 );
	GCM_CTX->current += 2 * nTexControls;

	#ifdef DEBUG_FPC_PATCHER
	if( m_bSync )
	{
		g_ps3gcmGlobalState.CmdBufferFlush( CPs3gcmGlobalState::kFlushForcefully );
		while ( *( volatile uint32* )pJts )
		{
			sys_timer_usleep( 50 );// wait for nop
		}
		#ifdef DEBUG_FPC_PATCHER
		{	
			fltx4 reg[job_fpcpatch::MAX_VIRTUAL_CONST_COUNT];
			m_state.GetSyncState( reg );
			Assert( !V_memcmp( m_pSyncState, reg, sizeof( fltx4 ) * job_fpcpatch::MAX_VIRTUAL_CONST_COUNT ) );
		}
		ValidatePatchedProgram( psh->m_pCgProg, m_pPutFragmentProgram );
		uint32 nFragmentProgramOffsetCheck;
		GCM_MUST_SUCCEED( cellGcmAddressToOffset, m_pPutFragmentProgram, &nFragmentProgramOffsetCheck );
		Assert( nFragmentProgramOffsetCheck == nFragmentProgramOffset );
		#endif
		
		g_ps3gcmGlobalState.CmdBufferFinish();
	}
	#endif
	m_nFpcPatchCounter++;
}





uint g_nFpcPatch2JobExtraFlags = 0; // set this to 2 and SPU will break


static int s_nFpcPatch2Calls = 0;

void CFragmentProgramConstantPatcher_SPU::FpcPatch2( const job_fpcpatch2::FpHeader_t * prog, uint nFpDmaSize, void *pPatchedProgram, uint32 * pJts )
{
	MICRO_PROFILE( g_mpFpcPatch2 );

#ifdef VJOBS_ON_SPURS
	VjobChain3 &jobChain = g_spuGcm.m_jobSink;
	uint32 nUCodeSize = prog->m_nUcodeSize;
	CellSpursJob128 * pJob = g_spuGcm.m_jobPool128.Alloc( *g_spuGcm.m_pRoot->m_pFpcPatch2 );
	Assert( pJob->header.sizeDmaList == 0 && pJob->header.sizeInOrInOut == 0 ); // the default MUST always be 1
	pJob->header.useInOutBuffer = 1;
	
	CDmaListConstructor dmaConstructor( pJob->workArea.dmaList );

	dmaConstructor.AddInputDma( nFpDmaSize, prog );
	dmaConstructor.AddInputDma( sizeof( *m_state.m_pSharedState ), ( void* )m_state.m_pSharedState );

	// the g_spuGcmShared.m_nFpcpStartRangesAfterLastSync runs ahead of m_state.m_pSharedState->m_nStartRanges , because it's a PREDICTED
	// start of range. It'll be absolutely in-sync with m_state.m_pSharedState->m_nStartRanges if we run SPUs synchronously
	#ifdef DBGFLAG_ASSERT
	uint nSharedStateStartRanges = m_state.m_pSharedState->m_nStartRanges; 
	#endif
	// NOTE: if the asserts below fire, it may be due to invalid value in nSharedStateStartRanges because SPU DMAs stuff right down to m_state.m_pSharedState and it's changing while this code executes
	Assert( uint( m_state.m_nEndOfJournalIdx - nSharedStateStartRanges ) <= m_state.m_pSharedState->m_nBufferMask + 1 );
	Assert( uint( g_spuGcmShared.m_nFpcpStartRangesAfterLastSync - nSharedStateStartRanges ) <= uint( m_state.m_nEndOfJournalIdx - nSharedStateStartRanges ) );

	uint nStartOfJournal = /*nSharedStateStartRanges*/g_spuGcmShared.m_nFpcpStartRangesAfterLastSync, nBufferMask = m_state.m_pSharedState->m_nBufferMask;
	
	// we have 4 DMA elements ( 2..6 ) to fit the constant buffer; the 1st element may have to be as small as 16 bytes.
	// this leaves the max constant buffer size 4 * 16kb + 16 bytes
	
	const uint numRangeQwords = ( m_state.m_nEndOfJournalIdx - nStartOfJournal );
	Assert( numRangeQwords <= nBufferMask + 1 );
	if ( numRangeQwords != 0 )
	{
		uint nEndOfSpan0 = ( nStartOfJournal + nBufferMask + 1 ) & ~nBufferMask;
		if ( ( signed int )( nEndOfSpan0 - m_state.m_nEndOfJournalIdx ) >= 0 )
		{
			//numRangeQwords = ( m_state.m_nEndOfJournalIdx - nStartOfJournal );
			dmaConstructor.AddInputDmaLarge( ( numRangeQwords ) * sizeof( fltx4 ), m_state.m_pSharedState->GetBufferStart() + ( nStartOfJournal & nBufferMask ) );
		}
		else
		{
			//numRangeQwords = nFirstRange + nSecondRange ;
			dmaConstructor.AddInputDmaLarge( ( nEndOfSpan0 - nStartOfJournal ) * sizeof( fltx4 ), m_state.m_pSharedState->GetBufferStart() + ( nStartOfJournal & nBufferMask ) );
			dmaConstructor.AddInputDmaLarge( ( m_state.m_nEndOfJournalIdx - nEndOfSpan0 ) * sizeof( fltx4 ), m_state.m_pSharedState->GetBufferStart() );
		}
	}
	else
	{
		dmaConstructor.AddSizeInOrInOut( 16 ); // we need at least 16 bytes in the ranges area for temporary storage
	}

	dmaConstructor.FinishIoBuffer( &pJob->header );
	if( pJob->header.sizeDmaList > 7 * sizeof( uint64 ) )
	{
		Error( "FpcPatch2: DMA list size out of range (%d). job_fpcpatch2 parameters won't fit. numRangeQwords = %d\n", pJob->header.sizeDmaList, numRangeQwords );
	}
	
	
	// IMPORTANT: make it always synchronous , in case we don't have the target to patch. The only reason for this job to exist is to make it synchronous
	// Also, if the range is large, still make it synchronous, to avoid subsequent jobs doing a lot of computations in vein
	uint nAsync = !pPatchedProgram || numRangeQwords >= 1024 ? 0 : ( ( m_nFpcPatchCounter ) & m_nFpcPatchSyncMask ) ;

	dmaConstructor[7][0] = m_nFpcPatchCounterOfLastSyncJob;
	dmaConstructor[7][1] = m_nFpcPatchCounter;
	dmaConstructor[8][0] = ( uint32 ) pPatchedProgram;
	dmaConstructor[8][1] = uintp( pJts ); // the SPU->RSX dma element; may be NULL
	dmaConstructor[9][0] = m_state.m_nEndOfJournalIdx;
	dmaConstructor[9][1] = ( uint32 ) nStartOfJournal;
	if( !IsCert() )
	{
		pJob->header.jobType |= CELL_SPURS_JOB_TYPE_MEMORY_CHECK;
	}

	dmaConstructor[8][0] |= g_nFpcPatch2JobExtraFlags;
	if ( !nAsync )
	{
		dmaConstructor[8][0] |= job_fpcpatch::FLAG_PUT_STATE;
		m_nFpcPatchCounterOfLastSyncJob = m_nFpcPatchCounter;
		pJob->header.jobType |= CELL_SPURS_JOB_TYPE_STALL_SUCCESSOR;
		g_spuGcmShared.m_nFpcpStartRangesAfterLastSync = m_state.m_nEndOfJournalIdx;
	}

#ifdef DBGFLAG_ASSERT
	int nError = cellSpursCheckJob( ( const CellSpursJob256* )pJob, sizeof( *pJob ), 256 );
	static int s_nJobErrors = 0;
	if( CELL_OK != nError )
	{
		++s_nJobErrors;
	}
#endif

	if ( !nAsync )
	{
		jobChain.PushSyncJobSync( CELL_SPURS_JOB_COMMAND_JOB( pJob ) );
	}
	else
	{
		jobChain.Push( CELL_SPURS_JOB_COMMAND_JOB( pJob ) );
	}
	
#ifdef DEBUG_FPC_PATCHER
	if( m_bSync )
	{
		if( pJts )
		{
			volatile uint32 * pJts2 = pJts;
			while( *pJts2 )
				continue;
		}
		
		volatile uint64_t * pEaJob = &pJob->header.eaBinary;
		while( * pEaJob )
			continue;
	}
#endif	
	s_nFpcPatch2Calls++;
	
#endif
}


#ifdef DEBUG_FPC_PATCHER
extern void PatchUcodeConstSwap( uint32 * pDestination, const uint32 * pSource, int nLength );
extern uint fspatchGetLength( CGtype nType );

uint32 g_nConstLengthCounter[5] = { 0, 0, 0, 0, 0 };

void CFragmentProgramConstantPatcher_SPU::ValidatePatchedProgram( const CgBinaryProgram * prog, void * pPatchedUcode )
{
	Assert( prog->profile == PROFILE_SCE_FP_RSX && prog->binaryFormatRevision == CG_BINARY_FORMAT_REVISION );
	uint32 nUCodeSize = prog->ucodeSize;
	void * pUcode = stackalloc( nUCodeSize );
	void * pSourceUcode =  ( ( uint8* ) prog ) + prog->ucode;
	V_memcpy( pUcode, ( ( uint8* ) prog ) + prog->ucode, nUCodeSize );

	CgBinaryParameter * pParameters = ( CgBinaryParameter * )( uintp( prog ) + prog->parameterArray ) ;

	uint32 * pPatchDestination = NULL;
	Assert( cellGcmCgGetCountParameter( ( CGprogram ) prog ) == prog->parameterCount );
	for ( int nPar = 0; nPar < prog->parameterCount; ++nPar )
	{
		CgBinaryParameter * pPar = pParameters + nPar;
		Assert( pPar == ( CgBinaryParameter * ) cellGcmCgGetIndexParameter( ( CGprogram ) prog, nPar ) );

#ifdef DBGFLAG_ASSERT
		const char * pLeafName = ( const char * )( uintp( prog ) + pPar->name );
		( void )pLeafName;
		uint32 * pDefault = pPar->defaultValue ? ( uint32* )( uintp( prog ) + pPar->defaultValue ) : NULL ;
#endif

		if ( pPar->embeddedConst )
		{
			Assert( pPar->res == CG_C && pPar->var == CG_UNIFORM ); // this MUST be a uniform constant.. at least I think that's the only kind we need to patch
			const CgBinaryEmbeddedConstant * pEmbedded = ( const CgBinaryEmbeddedConstant* )( uintp( prog ) + pPar->embeddedConst );
			int nLength = fspatchGetLength( pPar->type );
			g_nConstLengthCounter[nLength] ++;
			for ( uint nEm = 0; nEm < pEmbedded->ucodeCount; ++ nEm )
			{
				uint ucodeOffset = pEmbedded->ucodeOffset[nEm]; // is this the offset from prog structure start?
				Assert( ucodeOffset < nUCodeSize - 4 );
#ifdef DBGFLAG_ASSERT
				Assert( cellGcmCgGetEmbeddedConstantOffset( ( CGprogram ) prog, ( CGparameter ) pPar, nEm ) == ucodeOffset );
				const float * pDefaultCheck = cellGcmCgGetParameterValues( ( CGprogram ) prog, ( CGparameter ) pPar );
				Assert( pDefault == ( uint32* ) pDefaultCheck );
				uint32 * pUcodeEmConst = ( uint32* )( uintp( pSourceUcode ) + ucodeOffset );
				Assert( !pDefault || !V_memcmp( pDefault, pUcodeEmConst, nLength * 4 ) );
#endif

				pPatchDestination = ( uint32* )( uintp( pUcode ) + ucodeOffset );
				uint32 * pPatchedCheck = ( uint32* )( uintp( pPatchedUcode ) + ucodeOffset );
				PatchUcodeConstSwap( pPatchDestination, ( uint32* ) & ( m_pSyncState[pPar->resIndex] ), nLength );
				Assert( !V_memcmp( pPatchDestination, pPatchedCheck, nLength * 4 ) );
			}
		}
	}

	Assert( !V_memcmp( pPatchedUcode, pUcode, nUCodeSize ) );
}
#endif


void FpcPatchState::Init( job_fpcpatch::FpcPatchState_t * pSharedState, uint32 nBufferQwords )
{
#ifdef _DEBUG
	//m_nRangesAdded = 0;
#endif
	pSharedState->m_nBufferMask   = m_nBufferMask      = nBufferQwords - 1;
	pSharedState->m_nStartRanges  = m_nEndOfJournalIdx = IsCert() ? 0 : nBufferQwords - 128;
	pSharedState->m_eaThis        = m_pSharedState     = pSharedState;
	pSharedState->m_nThisStatePatchCounter = 0;
	pSharedState->m_nDebuggerBreak = 0;
}




void FpcPatchState::GetSyncState( fltx4 * pRegisters )
{
	V_memcpy( pRegisters, m_pSharedState->m_reg, job_fpcpatch:: MAX_VIRTUAL_CONST_COUNT * sizeof( fltx4 ) );
	for( uint nJournalIdx = m_pSharedState->m_nStartRanges; nJournalIdx < m_nEndOfJournalIdx ; )
	{
		job_fpcpatch:: ConstRangeHeader_t & range = ((job_fpcpatch::ConstRangeHeader_t*)m_pSharedState->GetBufferStart())[ nJournalIdx & m_pSharedState->m_nBufferMask ];
		nJournalIdx++;
		for( uint nConstIdx = 0 ; nConstIdx < range.m_u32.m_nCount; ++nConstIdx, ++nJournalIdx )
		{
			pRegisters[ range.m_u32.m_nStart + nConstIdx ] = m_pSharedState->GetBufferStart()[nJournalIdx & m_pSharedState->m_nBufferMask ];
		}
	}
}

/*
void FpcPatchState::Reset()
{
	m_nEndOfJournalIdx = 0;
	m_pSharedState->m_nStartRanges = 0;
}
*/
#ifdef _DEBUG
static int s_nDebugRangeAdd = -1, s_nDebugSetConst = -1;
#endif

uint FpcPatchState::AddRange( uint32 nStart, uint32 nCount, const float * pData )
{
	#ifndef _CERT
	if( nStart + nCount > job_fpcpatch::MAX_VIRTUAL_CONST_COUNT )
	{
		Error( "AddRange(%d..%d) out of range <%d\n", nStart, nCount, int( job_fpcpatch::MAX_VIRTUAL_CONST_COUNT ) );
	}
	#endif
	#ifdef _DEBUG
	//Assert( s_nDebugRangeAdd != m_nRangesAdded );
	if( int( s_nDebugSetConst - nStart ) >= 0 && int( s_nDebugSetConst - nStart ) < int( nCount ) )
	{
		fltx4 flDebugRegister = LoadUnalignedSIMD( pData + 4 * int( s_nDebugSetConst - nStart ) );
		DebuggerBreak();
	}
	//++m_nRangesAdded;
	#endif
	
	// spin-wait, then V_memcpy range
	COMPILE_TIME_ASSERT( sizeof( job_fpcpatch::ConstRangeHeader_t ) == 16 );
	const uint nSpins = 0x1FF;
	Assert( !( nSpins & ( nSpins + 1 ) ) );

	//
	//   We need space for nCount + 1 QWords (1 Qword for the ConstRangeHeader_t)
	//   And we need m_nEndOfJournalIdx !=	m_nStartRanges to distinguish between
	//   the all-empty and all-full buffers
	//

	uint nAttempts = 0;
	for ( ; ; ++nAttempts )
	{
		uint32 nStartRanges = m_pSharedState->m_nStartRanges;
		Assert( uint32( m_nEndOfJournalIdx - nStartRanges ) <= m_nBufferMask + 1 );
		// compute the new end - start; is it running further than buffer size away?
		if ( ( m_nEndOfJournalIdx + nCount - ( nStartRanges + m_nBufferMask + 1 ) ) & 0x80000000 )
		{	// no, the comparison is negative, therefore it's safe to fill it in
			break;
		}

		// if ( ( nAttempts & nSpins ) == nSpins )
		{
			// the caller prints warning about this stall.
			sys_timer_usleep( 60 ); // TODO: proper spinwait; proper OS syncronization
			if( nAttempts == ( 1000000 / 60 ) )
			{
				// waiting for a second already ...
				Warning(
					"***************************************************************************************************************\n"
					"* SPU hang in FpcPatchState::AddRange(). Please send this log (including a couple of screens above) to Sergiy *\n"
				);
				Msg( "AddRange(%d,%d,%p), ", nStart, nCount, pData );
				Msg( "SharedState @%p {start=0x%X&0x%X,patch=%X,job=%X},", m_pSharedState, m_pSharedState->m_nStartRanges, m_pSharedState->m_nBufferMask, m_pSharedState->m_nThisStatePatchCounter, m_pSharedState->m_eaThisStateJobDescriptor );
				Msg( "FpcpState @%p {end=0x%X},", this, this->m_nEndOfJournalIdx );
				Msg( "SpuGcmShared trace {0x%X,0x%X,0x%X}\n", g_spuGcmShared.m_nFpcPatchCounterOfLastSyncJob, g_spuGcmShared.m_nFpcPatchCounter, g_spuGcmShared.m_nFpcpStartRangesAfterLastSync );
				
				Msg( "RSX put=%X, get=%X sysring{put=%X,end=%X}\n", g_spuGcmShared.m_eaGcmControlRegister->put, g_spuGcmShared.m_eaGcmControlRegister->get,
					g_spuGcmShared.m_sysring.m_nPut, g_spuGcmShared.m_sysring.m_nEnd );
				
				Msg( "last JTS ret guard patched @%X, ", *cellGcmGetLabelAddress( GCM_LABEL_DEBUG_FPCP_RING ) );
				Msg( "ringRsx[%d]:", g_spuGcmShared.m_fpcpRing.m_ringRsx.Count() );
				for( int i = 0; i < g_spuGcmShared.m_fpcpRing.m_ringRsx.Count(); ++i )
				{
					RsxSpuDoubleRing::Segment_t & segment = g_spuGcmShared.m_fpcpRing.m_ringRsx[i];
					Msg(" {%X,%p,%s}", segment.m_eaBase, segment.m_pSpuJts, *(segment.m_pSpuJts) == CELL_SPURS_JOB_COMMAND_LWSYNC ? "LWSYNC" : *(segment.m_pSpuJts) == CELL_SPURS_JOB_COMMAND_JTS ? "JTS" : "ERROR" );
				}
				Msg( "\nringSpu[%d]:", g_spuGcmShared.m_fpcpRing.m_ringSpu.Count() );
				for( int i = 0; i < g_spuGcmShared.m_fpcpRing.m_ringSpu.Count(); ++i )
				{
					RsxSpuDoubleRing::Segment_t & segment = g_spuGcmShared.m_fpcpRing.m_ringSpu[i];
					Msg(" {%X,%p,%s}", segment.m_eaBase, segment.m_pSpuJts, *(segment.m_pSpuJts) == CELL_SPURS_JOB_COMMAND_LWSYNC ? "LWSYNC" : *(segment.m_pSpuJts) == CELL_SPURS_JOB_COMMAND_JTS ? "JTS" : "ERROR" );
				}
				Msg( "***************************************************************************************************************\n" );
			}
		}
	}
	// we have enough free buffer to insert stuff
	job_fpcpatch::ConstRangeHeader_t *hdr = (job_fpcpatch::ConstRangeHeader_t *)AddInternalPtr();
	hdr->m_u32.m_nStart = nStart;
	hdr->m_u32.m_nCount = nCount;

	// add constants block
	AddInternalBlock( pData, nCount );
	
	return nAttempts;
}

#endif
