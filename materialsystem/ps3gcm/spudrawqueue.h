//========== Copyright © Valve Corporation, All rights reserved. ========
//
//   This is PPU->SPU fifo queue to feed draw jobs
//

#ifndef SPUDRAWQUEUE_HDR
#define SPUDRAWQUEUE_HDR

#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include "vjobs/pcring.h"
#include "ps3/vjobutils_shared.h"
#include "vjobs/spudrawqueue_shared.h"

extern void StallAndWarning( const char * pWarning );

class SpuDrawQueue
{
public:
	typedef void ( *FnFlushCallback_t)( SpuDrawQueue * );
	typedef void ( *FnStallCallback_t)( SpuDrawQueue *, uint32 * pGet, uint nWords );

	void Init( uint nBufferSize, uint32 * pSignal, FnFlushCallback_t fnFlushCallback, FnStallCallback_t fnStallCallback );
	void Shutdown();
	
	void PushFlushCallback( FnFlushCallback_t fnFlushCallback );
	void PopFlushCallback();
					     
	uint32 *AllocWords( uint nWords /*, uint nAlignMask = 0, uint nAlignValue = 0*/ );
	void UnallocToAlign();

	template<typename T>
	T *AllocAligned( )
	{
		COMPILE_TIME_ASSERT( sizeof( T ) % 4 == 0 );
		Align();
		return ( T* )AllocWords( sizeof( T ) / 4 );
	}
	
	template <typename T>
	T *AllocWithHeader( uint nHeader ) { uint32 * pHeader = AllocWords( 1 + sizeof( T ) / 4 ); *pHeader = nHeader; return ( T* )( pHeader + 1 ); }
	
	uint Collect( uint32 * pStartBatch, uint32 * pEndBatch, CDmaListConstructor & dmac );
	uint32 * GetCursor(){ return m_pPut; }
	uint32 * GetFlushWatermark() {return m_pFlushWatermark;}
	void Align();
	
	void Push4( uint32 a, uint32 b, uint32 c, uint32 d ){ uint32 * p = AllocWords( 4 ); p[0] = a; p[1] = b; p[2] = c; p[3] = d; }
	void Push3( uint32 a, uint32 b, uint32 c ){ uint32 * p = AllocWords( 3 ); p[0] = a; p[1] = b; p[2] = c; }
	void Push2( uint32 a, uint32 b ){ uint32 * p = AllocWords( 2 ); p[0] = a; p[1] = b; }
	void Push1( uint32 a ){ uint32 * p = AllocWords( 1 ); p[0] = a; }

	enum ConstEnum_t {DMA_ALIGNMENT = 16 };
	
	void SetFlushWatermarkFrom( uint32 *pPut );
	
	uint32 GetSignal()const{ return ( uint32 )m_pPut; }
	uint32 * GetBuffer()const{ return m_pBuffer; }
	uint32 * GetBufferEnd()const { return m_pBufferEnd; }
	uint32 GetBufferWords()const { return m_pBufferEnd - m_pBuffer; }
	bool IsValidCursor( uint32 * p )const { return m_pBuffer <= p && p <= m_pBufferEnd && 0 == ( uintp( p ) & 3 ); }
	uint32 * NormalizeCursor( uint32 * p ) { Assert( IsValidCursor( p ) ); return ( p >= m_pBufferEnd ? m_pBuffer : p ); }
	uint Length( uint32 * pBegin, uint32 * pEnd )const;
protected:
	void SetSignal( uint32 nSignal );
public:
	uint64 m_nAllocWords;
	#ifdef _DEBUG
	uint64 m_nAllocCount, m_nCollectCount;
	uint64 m_nAllocBreak, m_nCollectBreak;
	uint32 * m_nAllocBreakAddress;
	#endif
protected:
	// the begin and end of the whole buffer
	// it must be 16-byte aligned
	uint32 *m_pBuffer, *m_pBufferEnd;
	
	// up to this point, we may write stuff. Starting at this point, SPU is reading data
	// m_pPut==m_pGet means "buffer empty"
	// m_pPut > m_pGet means we can write to the end of the buffer and then start at the start
	// m_pPut < m_pGet means we can write from put to get, exclusively
	uint32 *m_pGet;
	
	// this is the point where we can write stuff, up to m_pGet
	uint32 *m_pPut;
	
	// external signal in the structure where SPU writes
	volatile uint32 * m_pSignal;
	
	uint32 *m_pFlushWatermark;
	
	// FlushCallback member is implemented elsewhere. DrawQueue calls this callback
	// as an advice to flush the queue. The callback doesn't have to flush the queue
	// if the current transaction is deemed atomic. Also, even if the queue is flushed,
	// this object does not get immediate feedback until it reads the signal that SPU sets
	// much later, asynchronously. This callback is important to slice the long transactions 
	// into smaller chunks that fit into LS
	FnFlushCallback_t m_fnFlushCallback;
	FnStallCallback_t m_fnStallCallback;

	//enum EnumConst_t{STACK_SIZE = 1 };
	FnFlushCallback_t m_fnFlushCallbackStack;
};				   


inline void SpuDrawQueue::SetSignal( uint32 nSignal )
{
	uint32 *pNewGet = (uint32*)nSignal;
	
	// the new get must be between old get and put
	
	Assert( pNewGet == m_pGet ||
			( pNewGet > m_pGet  ? m_pPut < m_pGet || pNewGet <= m_pPut  // the new get doesn't wrap around the buffer, 
								: m_pPut < m_pGet && pNewGet <= m_pPut // the new get wraps around the buffer, so the put must wrap around, too
			)
	);
	
	m_pGet = pNewGet;
}



inline void SpuDrawQueue::Align()
{
	while( uintp( m_pPut ) & 0xF )
	{
		Push1( 0 );
	}
}


#endif