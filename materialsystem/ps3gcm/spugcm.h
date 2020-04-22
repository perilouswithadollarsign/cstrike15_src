//========== Copyright © Valve Corporation, All rights reserved. ========
// This is the central hub for controlling SPU activities relating to 
// RSX/graphics processing/rendering
//
#ifndef SPU_GCM_HDR
#define SPU_GCM_HDR

#include "ps3/spugcm_shared.h"
//#include "ps3/rsx_spu_double_ring.h"
#include "vjobs_interface.h"
#include "ps3/vjobchain.h"
#include "ps3/vjobpool.h"
#include "ps3/ps3gcmmemory.h"
#include "spudrawqueue.h"
#include "gcmfunc.h"
#include <edge/post/edgePost_ppu.h>
#include <edge/post/edgepost_mlaa_handler_ppu.h>

extern CSpuGcmSharedState g_spuGcmShared;
extern void StallAndWarning( const char * pWarning );


class ZPass
{
public:
	void Init();
	bool CanBegin();
	void Begin( uint32 * pCursor );
	void End() { m_pCursor = NULL; }
	void Shutdown();
	void Validate()const{Assert( !m_nDummy && ( m_nPut - m_nGet ) <= m_nJobs );}
	uint GetSubchainCapacity()const { Validate();  return m_nJobs - ( m_nPut - m_nGet ) ; }
	uint64 * GetCurrentCommandPtr() { return &m_pJobs[ m_nPut & ( m_nJobs - 1 ) ]; }
	void PushCommand( uint64 nCommand );
	operator bool () const { return m_pCursor != NULL; }
public:
	uint m_nDrawPassSubchain;
	uint m_nJobPoolMarker;
	uint m_nJobs;
	uint m_nDummy;
	uint m_nPut;
	uint m_isInEndZPass;

	ZPassSavedState_t  * m_pSavedState;
	uint32 * m_pCursor;	
	uint64 * m_pSubchain;
	uint64 * m_pJobs; // this ring buffer contains recorded rendering jobs to be replayed
	uint m_nFpcpStateEndOfJournalIdxAtZPassBegin; // ... at the beginning of Zpass

	// Notice: this m_pGet member is patched by SPU after a corresponding job subchain is finished
	volatile uint32 m_nGet;
protected:
};




class ALIGN128 CEdgePostWorkload
{
public:
	CEdgePostWorkload(){m_isInitialized = false;}
	void OnVjobsInit( VJobsRoot* pRoot );
	void OnVjobsShutdown( VJobsRoot* pRoot );
	void Kick( void * dst, uint nSetLabel );
	bool ShouldUseLabelForSynchronization()const{return true;}
	bool IsResultInMainMemory()const { return true; }

	enum EnumConst_t{STAGE_COUNT=1};
	EdgePostProcessStage m_stages[STAGE_COUNT];
	EdgePostMlaaContext m_mlaaContext;
	EdgePostWorkload m_workload;
	void * m_pMlaaScratch;
	
	bool m_isInitialized;

} ALIGN128_POST;
extern CEdgePostWorkload g_edgePostWorkload;


class CSpuGcm: public VJobInstance
{
public:
	void CreateRsxBuffers();
	void CreateIoBuffers();
	void UseIoBufferSlack( uint nIoBufferSlack );
	void OnGcmInit();
	void Shutdown();
	
	void BeginScene();
	void EndScene();
	
	void CmdBufferFlush( )
	{
		GcmStateFlush();
		//PutPcbringCtx();
	}
	
	void CmdBufferFinish();

	int OnGcmCommandBufferReserveCallback( struct CellGcmContextData *context, uint32_t nCount );
	int OnGcmCommandBufferReserveCallbackOld( struct CellGcmContextData *context, uint32_t nCount );
	
	void GcmStateFlush( );
	SpuDrawHeader_t * BeginDrawBatch();
	void SubmitDrawBatch( IDirect3DVertexDeclaration9 *pVertDecl, OptimizedModel::OptimizedIndexBufferMarkupPs3_t *pIbMarkup );
	
	bool TruePause();
	void RenderEmptyFrame();
	
	void SyncMlaa( void * pLocalSurface );
	void SyncMlaa( ) { SyncMlaa( m_pMlaaBuffer ); }

	bool BeginZPass( );
	void SetPredication( uint nPredicationMask ); // D3DPRED_* mask
	void EndZPass( bool bPopMarker );
	void AbortZPass(){ EndZPass( false ); }
	void OnSetPixelShaderConstant();
	
	SpuDrawQueue * GetDrawQueue(){ return &m_spuDrawQueues[m_nSpuDrawQueueSelector];}
	SpuDrawQueue * GetDrawQueueNormal(){ return &m_spuDrawQueues[0]; }
	void DrawQueueNormal( bool bExecuteDeferredQueueSegment = true );
	struct DrawQueueDeferred_Result{ bool isFirstInFrame; };
	DrawQueueDeferred_Result DrawQueueDeferred(); // may flush previous frame deferred queue
	uint IsDeferredDrawQueue() { return m_nSpuDrawQueueSelector; }
	bool ExecuteDeferredDrawQueue( uint nPrevious );
	void FlipDeferredDrawQueue();
	bool ExecuteDeferredDrawQueueSegment( uint32 * pCmdBegin, uint32 * pCmdEnd, bool bExecuteDraws );
	void ValidateDeferredQueue();
	//void DisableMlaaForTwoFrames();
	void DisableMlaaPermanently();
	void DisableMlaa();
protected:
	static void OnSpuDrawQueueStallDeferredDelegator( SpuDrawQueue *pDrawQueue, uint32 * pGet, uint nWords );
	void OnSpuDrawQueueStallDeferred( SpuDrawQueue *pDrawQueue, uint32 * pGet, uint nWords );
	static void OnSpuDrawQueueFlushDeferred( SpuDrawQueue *pDrawQueue );
	static void OnSpuDrawQueueStall( SpuDrawQueue *pDrawQueue, uint32 * pGet, uint nWords );
	static void OnSpuDrawQueueFlush( SpuDrawQueue *pDrawQueue );
	static void OnSpuDrawQueueFlushDoNothing( SpuDrawQueue *pDrawQueue ){}
	static void OnSpuDrawQueueFlushInZPass( SpuDrawQueue *pDrawQueue );
	void OnSpuDrawQueueFlushInZPass( );
	void OnVjobsInit(); // gets called after m_pRoot was created and assigned
	void TestPriorities();
	void OnVjobsShutdown(); // gets called before m_pRoot is about to be destructed and NULL'ed
	
	uint32 * GetPcbringPtr( uint nOffsetBytes ) { return AddBytes( m_pPcbringBuffer, nOffsetBytes & ( g_spuGcmShared.m_nPcbringSize - 1 ) ); }
	uint32 * GetPcbringBufferEnd() {return AddBytes( m_pPcbringBuffer, g_spuGcmShared.m_nPcbringSize ); }
	signed int GetPcbringAvailableBytes()const;
	//void SetCtxBuffer( uint nSegment );
	#if 0
	volatile uint64* PutPcbringCtx( uint32 * pSkipTo, uint32 * pNewEnd );
	volatile uint64* PutPcbringCtx();
	#endif
	inline uint GetMaxPcbringSegmentBytes()const { return m_nMaxPcbringSegmentBytes; }
	void BeginGcmStateTransaction();
	void PushSpuGcmJob( CellSpursJob128 * pJob );
	void PushStateFlushJob( SpuDrawQueue * pDrawQueue, uint nResultantSpuDrawQueueSignal, uint32 *pCursorBegin, uint32 * pCursorEnd );
	void PushSpuGcmJobCommand( uint64 nCommand );
	void PushSpuGcmCallSubchain( uint64 * eaJobChain ){ m_jobSink.Push( CELL_SPURS_JOB_COMMAND_CALL( eaJobChain ) );}
	void ZPassCheckpoint( uint nReserveSlots );
	CellSpursJob128 * PushDrawBatchJob( uint nResultantSpuDrawQueueSignal, SpuDrawHeader_t * pDrawHeader, IDirect3DVertexDeclaration9 *pVertDecl, OptimizedModel::OptimizedIndexBufferMarkupPs3_t *pIbMarkup );
public:

	void CloseDeferredChunk();
	uint32* OpenDeferredChunk( uint nHeader = SPUDRAWQUEUE_DEFERRED_GCMFLUSH_METHOD, uint nAllocExtra = 0 );

	void SetCurrentBatchCursor( uint32 * pCursor )
	{
		m_pCurrentBatchCursor[m_nSpuDrawQueueSelector] = pCursor;
	}
	
	uint32 * GetCurrentBatchCursor() 
	{
		return m_pCurrentBatchCursor[m_nSpuDrawQueueSelector];
	}
	
protected:
	SpuDrawQueue m_spuDrawQueues[2];
	
	// this frame [0]  and previous frames [1] "end" markers for replay
	// gets updated on every chunk close
	uint32* m_pDeferredQueueCursors[3]; 
	
	// this is the last point where DrawQueueDeferred() was called
	uint32 * m_pDeferredQueueSegment;
	
	// pointer to deferred chunk last open; NULL if the last deferred chunk was closed, but none new was open yet
	// this may stay non-NULL( thus indicating non-closed chunk) during executing deferred commands, too,
	// in case of out-of-memory condition. Then, StallDeferred callback will execute deferred commands without closing current chunk.
	// Relation: MANY chunks per ONE batch
	uint32* m_pDeferredChunkHead;
	
	uint32 m_nDeferredChunkHead;
	uint32 *m_pDeferredChunkSubmittedTill[4]; // only [1] is used; [0] and [2] are write- and debug-only
	uint16 m_nSpuDrawQueueSelector;
	uint16 m_nFramesToDisableDeferredQueue; // disable for this number of frames if we don't have enough memory

public:
	// fragment program constant patcher double ring, JTS->RET , RSX->SPU
	CPs3gcmLocalMemoryBlock m_fpcpRingBuffer, m_edgeGeomRingBuffer;
	VjobChain3 m_jobSink;
	VjobPool<CellSpursJob128> m_jobPool128;

	volatile uint32 * m_pFinishLabel;
	
	uint32 *m_pPcbringBuffer;
	
	ZPass m_zPass; // NULL when we aren't in Zpass
	
	DeferredState_t * m_pDeferredStates[2];
	
	uint m_nPcbringBegin; // this byte offset corresponds to GCM_CTX->begin
 	uint32 m_nPcbringWaitSpins;
 	uint32 m_nMaxPcbringSegmentBytes;
 	uint32 m_nGcmFlushJobScratchSize;
 	
 	uintp m_eaLastJobThatUpdatesSharedState;
 	uint m_nFpcpStateEndOfJournalIdxAtSpuGcmJob;
 	
 	enum TransactionBatchEnum_t
 	{
 		BATCH_GCMSTATE, // the default transaction type
 		BATCH_DRAW
 	};
 	TransactionBatchEnum_t m_nCurrentBatch;
 	// the batch is a batch of commands to send to an SPU job: job_gcmflush (BATCH_GCMSTATE) or job_drawindexedprimitive (BATCH_DRAW)
 	uint32 * m_pCurrentBatchCursor[2];
 	
 	void * m_pMlaaBuffer, *m_pMlaaBufferOut;
 	volatile vec_uint4 * m_pMlaaBufferCookie;
 	uint32 *m_pEdgePostRsxLock;

	uint m_nFrame;
	
	#ifdef _DEBUG
	uint m_nJobsPushed, m_nChunksClosedInSegment;
	#endif
	uint64 m_nDeferredQueueWords;

	bool m_bUseDeferredDrawQueue;
};
extern CSpuGcm g_spuGcm;
extern const vec_uint4 g_vuSpuGcmCookie;


struct ALIGN128 PriorityTest_t
{
	CellSpursJob128 m_job;
	job_notify::NotifyArea_t m_notify;
	bool Test( class VjobChain4 *pJobChain );
} ALIGN128_POST;






#endif
