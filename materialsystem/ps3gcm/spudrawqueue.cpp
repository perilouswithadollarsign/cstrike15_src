//========== Copyright © Valve Corporation, All rights reserved. ========
#include "tier0/memalloc.h"
#include "ps3/ps3_gcm_config.h"
#include "spudrawqueue.h"
#include "ps3gcmstate.h"


void SpuDrawQueue::Init( uint nBufferSize, uint32 * pSignal, FnFlushCallback_t fnFlushCallback, FnStallCallback_t fnStallCallback )
{
	if( nBufferSize < 2 * DRAWQUEUE_LSRING_SIZE )
	{
		Warning("SpuDrawQueue requested size (%d bytes) is too small (must be at least %d), auto-adjusting\n", nBufferSize, 2 * DRAWQUEUE_LSRING_SIZE );
		nBufferSize = 2 * DRAWQUEUE_LSRING_SIZE;
	}
	m_pBuffer =  ( uint32* ) g_ps3gcmGlobalState.IoSlackAlloc( 128, nBufferSize );
	m_pBufferEnd = AddBytes( m_pBuffer, nBufferSize & -16 );
	m_pPut = m_pGet = m_pBuffer;
	
	*pSignal = GetSignal();
	m_pSignal = pSignal;

	m_fnFlushCallback = fnFlushCallback;
	m_fnStallCallback = fnStallCallback;
	m_fnFlushCallbackStack = NULL;
	
	#ifdef _DEBUG
	m_nAllocBreakAddress = NULL;
	m_nAllocCount = m_nCollectCount = 0;
	m_nAllocBreak = m_nCollectBreak = 0;
	#endif
	m_nAllocWords = 0;
	
	m_pFlushWatermark = AddBytes( m_pBuffer, DRAWQUEUE_LSRING_SIZE );
	if( m_pFlushWatermark + 8 >= m_pBufferEnd )
	{
		Error( "SpuDrawQueue misconfiguration: allocated buffer of %d bytes, but LS watermark size is %d bytes. Increase the main memory buffer size to avoid PPU deadlocks\n", nBufferSize, DRAWQUEUE_LSRING_SIZE );
	}
}

void SpuDrawQueue::PushFlushCallback( FnFlushCallback_t fnNewCallback )
{
	Assert( !m_fnFlushCallbackStack );
	m_fnFlushCallbackStack = m_fnFlushCallback;
	m_fnFlushCallback = fnNewCallback;
}

void SpuDrawQueue::PopFlushCallback()
{
	Assert( m_fnFlushCallbackStack );
	m_fnFlushCallback = m_fnFlushCallbackStack;
	m_fnFlushCallbackStack = NULL;
}



void SpuDrawQueue::Shutdown()
{
	g_ps3gcmGlobalState.IoSlackFree( m_pBuffer );
}


void SpuDrawQueue::UnallocToAlign()
{
	m_pPut = ( uint32* )( uintp( m_pPut ) & -16 );
}

//////////////////////////////////////////////////////////////////////////
// REENTRANT: m_fnFlushCallback can in turn call AllocWords with a small number of words
//
uint32 *SpuDrawQueue::AllocWords( uint nWords /*, uint nAlignMask, uint nAlignValue*/ )
{
#ifdef _DEBUG
	uint32 * pSavePut = m_pPut, *pSaveGet = m_pGet;(void)(pSavePut, pSaveGet);
	m_nAllocCount++;
	if( m_nAllocCount == m_nAllocBreak )
		DebuggerBreak();
#endif
	Assert( nWords * sizeof( uint32 ) <= SPUDRAWQUEUE_NOPCOUNT_MASK );
	uint32 * pOldPut = m_pPut, * pAllocation = pOldPut;//( uint32* )( uintp( pOldPut ) + ( ( nAlignValue - uintp( pOldPut ) ) & nAlignMask ) );
	uint32 * pNewPut = pAllocation + nWords;
	bool bWrap = false;

	if( pNewPut > m_pBufferEnd ) // do we need to wrap?
	{
		//we have to wrap...
		if( m_pPut < m_pBufferEnd )
			*m_pPut = SPUDRAWQUEUE_NOPCOUNT_METHOD | ( m_pBufferEnd - m_pPut - 1 );
		pNewPut = m_pBuffer + nWords;
		bWrap = true;
		pAllocation = m_pBuffer;
	}

	// since this put may be the last, we need to make sure that even after alignment, put != get
	// so we wait for the space to free up for aligned put
	uint32 * pNewAlignedPut = ( uint32* )AlignValue( uintp( pNewPut ), DMA_ALIGNMENT );

	if( bWrap ? pOldPut <= m_pFlushWatermark || m_pFlushWatermark < pNewAlignedPut:
				pOldPut <= m_pFlushWatermark && m_pFlushWatermark < pNewAlignedPut )
	{
		// collects , aligns and submits commands to SPU
		m_fnFlushCallback( this );
		// m_pPut may have changed slightly for alignment or EndZPass(), so we need to reconsider wrapping and recompute all pointers
		
		pOldPut = m_pPut; pAllocation = pOldPut;
		pNewPut = pOldPut + nWords;
		bWrap = false;

		if( pNewPut > m_pBufferEnd ) // do we need to wrap?
		{
			//we have to wrap...
			if( m_pPut < m_pBufferEnd )
				*m_pPut = SPUDRAWQUEUE_NOPCOUNT_METHOD | ( m_pBufferEnd - m_pPut - 1 );
			pNewPut = m_pBuffer + nWords;
			bWrap = true;
			pAllocation = m_pBuffer;
		}

		// since this put may be the last, we need to make sure that even after alignment, put != get
		// so we wait for the space to free up for aligned put
		pNewAlignedPut = ( uint32* )AlignValue( uintp( pNewPut ), DMA_ALIGNMENT );
	}

	// we must not allow new put == get, because it will cause the whole ring to suddenly be marked as empty
	uint nSpins = 0;
	while( bWrap ? pOldPut < m_pGet || m_pGet <= pNewAlignedPut : pOldPut < m_pGet && m_pGet <= pNewAlignedPut )
	{
		if( nSpins++ > 2 )
		{
			m_fnStallCallback( this, m_pGet, nWords );
		}

		SetSignal( *m_pSignal );
	}
	
	Assert( pNewPut >= m_pBuffer && pNewPut <= m_pBufferEnd );
	Assert( pAllocation >= m_pBuffer && pAllocation <= m_pBufferEnd );
	Assert( pAllocation + nWords >= m_pBuffer && pAllocation + nWords <= m_pBufferEnd );

	m_pPut = pNewPut; // we don't need to use up the whole aligned buffer

#ifdef _DEBUG	
	if( pAllocation == m_nAllocBreakAddress )
		DebuggerBreak();
#endif
	m_nAllocWords += nWords;
	return pAllocation;
}


// This is called within the Flush callback. May change m_pPut
// returns the number of bytes written from UNaligned start to UNaligned end
uint SpuDrawQueue::Collect( uint32 * pStartBatch, uint32 * pEndBatch, CDmaListConstructor & dmac )
{
#ifdef _DEBUG
	CDmaListConstructor saveDmac = dmac;(void)saveDmac;
	m_nCollectCount++;
	Assert( m_nCollectCount != m_nCollectBreak );
#endif
	Assert( pStartBatch >= m_pBuffer && pStartBatch <= m_pBufferEnd && pEndBatch >= m_pBuffer && pEndBatch <= m_pBufferEnd );
	uint nSize = 0;
	if( pEndBatch != pStartBatch ) //		or else it's an empty transaction, nothing to upload
	{
		// align the put pointer for DMA - always safe because SPUs can't be processing the remainder of 16-byte block
		// while we're writing into its beginning.
		// while( uintp( pEndBatch ) & ( DMA_ALIGNMENT - 1 ) )
		// {
		// 	*( pEndBatch++ ) = 0;
		// }		

		if( pEndBatch > pStartBatch )
		{
			// it wraps
			dmac.AddInputDmaLargeUnalignedRegion( pStartBatch, pEndBatch );
			nSize += uintp( pEndBatch ) - uintp( pStartBatch );
		}
		else
		{
			if( pStartBatch != m_pBufferEnd )
			{
				dmac.AddInputDmaLargeUnalignedRegion( pStartBatch, m_pBufferEnd );
				nSize += uintp( m_pBufferEnd ) - uintp( pStartBatch );
			}
			dmac.AddInputDmaLargeUnalignedRegion( m_pBuffer, pEndBatch );
			nSize += uintp( pEndBatch ) - uintp( m_pBuffer );
		}
	}
	
	SetFlushWatermarkFrom( pEndBatch );
	
	return nSize;
}

void SpuDrawQueue::SetFlushWatermarkFrom( uint32 *pPut )
{
	m_pFlushWatermark = ( uint32* )( ( uintp( pPut ) + DRAWQUEUE_LSRING_SIZE ) & -16 );
	while( m_pFlushWatermark >= m_pBufferEnd )
	{
		m_pFlushWatermark -= m_pBufferEnd - m_pBuffer;
	}
}

uint SpuDrawQueue::Length( uint32 * pBegin, uint32 * pEnd )const
{
	Assert( IsValidCursor( pBegin ) && IsValidCursor( pEnd ) );
	if( pBegin < pEnd )
		return uintp( pEnd ) - uintp( pBegin );
	else
		return ( uintp( m_pBufferEnd ) - uintp( pBegin ) ) +
			   ( uintp( pEnd ) - uintp( m_pBuffer ) );
}
