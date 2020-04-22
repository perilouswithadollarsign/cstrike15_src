//========== Copyright © Valve Corporation, All rights reserved. ========
// This is shared between SPU and PPU 

#ifndef COMMON_PS3_VJOBUTILS_SHARED_HDR
#define COMMON_PS3_VJOBUTILS_SHARED_HDR



#ifndef _PS3
#error "This is PS3 specific header"
#endif

#include "spu_job_shared.h"
#include <cell/spurs/job_descriptor.h>


template <typename T>
inline void AddInputDma( T * pJob, uint nSize, const void * pEa )
{
	Assert( !( nSize & 0xF ) );
	Assert( !( pJob->header.sizeDmaList & ( sizeof( uint64 ) - 1 ) ) );
	//int nError = cellSpursJobGetInputList( &pJob->workArea.dmaList[pJob->header.sizeDmaList / sizeof( uint64_t )], nSize, (uint32) pEa );
	//Assert( nError == CELL_OK );
	pJob->workArea.dmaList[pJob->header.sizeDmaList / sizeof( uint64_t )] = ( uint64( nSize ) << 32 ) | ( uint32 )pEa;
	
	// ( nSIze << 32 ) | pEa
	pJob->header.sizeDmaList += sizeof( uint64_t );
	pJob->header.sizeInOrInOut += nSize;
	Assert( pJob->header.sizeDmaList <= sizeof( pJob->workArea ) );
}

template <typename T>
inline void AlignInputDma( T * pJob )
{
	pJob->workArea.dmaList[pJob->header.sizeDmaList / sizeof( uint64_t )] = 0;
	pJob->header.sizeDmaList = AlignValue( pJob->header.sizeDmaList, 16 );
	Assert( pJob->header.sizeDmaList <= sizeof( pJob->workArea ) );
}

inline void AddCacheDma( struct CellSpursJob128 * pJob, uint nSize, const void * pEa )
{
	int nError = cellSpursJobGetInputList( &pJob->workArea.dmaList[( pJob->header.sizeDmaList + pJob->header.sizeCacheDmaList ) / sizeof( uint64 ) ], nSize, (uint32)pEa );
	( void )nError;
	Assert( nError == CELL_OK );
	pJob->header.sizeCacheDmaList += sizeof( uint64_t );
}

inline uint64 MakeDmaElement( void * pData, uint nSize )
{
	return ((uint32)pData) | (uint64(nSize)<<32);
}

template <uint nSize>
inline void V_memcpy16( void *pDest, const void * pSrc )
{
	Assert( !( nSize & 0xF ) && !( uintp( pDest ) & 0xF ) && !( uintp( pSrc ) & 0xF ) );
	for( uint i = 0; i < nSize / 16; ++i )
	{
		( ( vector unsigned int * ) pDest )[i] = ( ( vector unsigned int * ) pSrc )[i];
	}
}

class CDmaListConstructor
{
	uint32 *m_pListBegin, *m_pList;
	uint m_sizeInOrInOut;
	uint m_sizeCacheDmaList;
public:
	CDmaListConstructor( void * pList )
	{
		m_pList = m_pListBegin = ( uint32* )pList;
		m_sizeInOrInOut = m_sizeCacheDmaList = 0;
	}

	void AddInputDma( uint nSize, const void *pEa )
	{
		Assert( !m_sizeCacheDmaList );
		Assert( !( nSize & 0xF ) && nSize <= 16 * 1024 && ( !nSize || pEa ) );
		Assert( !IsAddressInStack( pEa ) );
		m_pList[0] = nSize;
		m_pList[1] = ( uint32 )pEa;
		m_pList += 2;

		m_sizeInOrInOut += nSize;
	}
	
	void AddCacheDma( uint nSize, const void *pEa )
	{
		// WARNING : NEVER use size=0, as there's a bug in SPURS that can corrupt data if you do
		Assert( !IsAddressInStack( pEa ) && pEa && nSize > 0 );
		uint32 * pCache = AddBytes( m_pList, m_sizeCacheDmaList );
		pCache[0] = nSize;
		pCache[1] = ( uint32 )pEa;
		
		m_sizeCacheDmaList += 8;
		Assert( m_sizeCacheDmaList <= 32 );
	}
	
	void AddInputDmaLargeUnalignedRegion( void * pBegin, void * pEnd )
	{
		uint32 eaBeginAligned = uint32( pBegin ) & -16;
		uint32 eaEndAligned = AlignValue( uint32( pEnd ), 16 );
		AddInputDmaLarge( eaEndAligned - eaBeginAligned, ( void* )eaBeginAligned );
	}

	void* AddInputDmaUnalignedRegion( void * pBegin, void * pEnd, int nAlignment = 16 )
	{
		uint32 eaBeginAligned = uint32( pBegin ) & -nAlignment;
		uint32 eaEndAligned = AlignValue( uint32( pEnd ), nAlignment );
		AddInputDma( eaEndAligned - eaBeginAligned, ( void* )eaBeginAligned );
		return ( void* )eaBeginAligned;
	}

	void AddInputDmaLarge( uint nMinReserve, uint nSize, const void * pEa )
	{
		AddInputDmaLarge( nSize, pEa );
		Assert( !( nMinReserve & 15 ) );
		if( nMinReserve > nSize )
		{
			m_sizeInOrInOut += nMinReserve - nSize;
		}
	}

	uint AddInputDmaLargeRegion( const void * pBegin, const void * pEnd )
	{
		uint nSize = uintp( pEnd ) - uintp( pBegin );
		AddInputDmaLarge( nSize, pBegin );
		return nSize;
	}
	
	void AddInputDmaLarge( uint nSize, const void * pEa )
	{
		Assert( !( nSize & 0xF ) && nSize < 248 * 1024 );
		Assert( !IsAddressInStack( pEa ) );
		uint nSizeRemaining = nSize;
		uintp eaRemaining = ( uintp )pEa;
		const uint nMaxDmaElementSize = 16 * 1024;
		while( nSizeRemaining > nMaxDmaElementSize )
		{
			m_pList[0] = nMaxDmaElementSize;
			m_pList[1] = eaRemaining;
			m_pList += 2;
			nSizeRemaining -= nMaxDmaElementSize;
			eaRemaining += nMaxDmaElementSize;
		}
		m_pList[0] = nSizeRemaining;
		m_pList[1] = eaRemaining;
		m_pList += 2;
		
		m_sizeInOrInOut += nSize;
	}
	
	void AddSizeInOrInOut( uint nAddIoBufferSize )
	{
		Assert( !( nAddIoBufferSize & 0xF ) );
		m_sizeInOrInOut += nAddIoBufferSize;
	}
	
	void EnsureCapacityInOrInOut( uint nCapacity )
	{
		Assert( !( nCapacity & 0xF ) );
		m_sizeInOrInOut = Max( m_sizeInOrInOut, nCapacity );
	}

	void FinishIoBuffer( CellSpursJobHeader * pHeader )
	{
		FinishInOrIoBuffer( pHeader );
		Assert( pHeader->useInOutBuffer == 1 );
	}

	void FinishInBuffer( CellSpursJobHeader * pHeader )
	{
		FinishInOrIoBuffer( pHeader );
		Assert( pHeader->useInOutBuffer == 0 );
	}
		
	void FinishIoBuffer( CellSpursJobHeader * pHeader, void * pParams )
	{
		FinishIoBuffer( pHeader );
		// we only use up to 256 byte jobs, which have up to 26 DMA slots. Check that the params belongs to this job structure
		// and check that it doesn't overlap with IO DMA list or cache DMA list
		Assert( uintp( pParams ) <= uintp( m_pListBegin + 26 * 2 ) && uintp( pParams ) >= uintp( m_pList ) + m_sizeCacheDmaList );
	}

	void FinishInBuffer( CellSpursJobHeader * pHeader, void * pParams )
	{
		FinishInBuffer( pHeader );
		// we only use up to 256 byte jobs, which have up to 26 DMA slots. Check that the params belongs to this job structure
		// and check that it doesn't overlap with IO DMA list or cache DMA list
		Assert( uintp( pParams ) <= uintp( m_pListBegin + 26 * 2 ) && uintp( pParams ) >= uintp( m_pList ) + m_sizeCacheDmaList );
	}
	
	inline uint32 * operator [] ( int i )
	{
		uint32 * pResult = m_pListBegin + 2 * i;
		Assert( pResult >= m_pList ); // are we not overwriting the dma list tail that we wrote previously?
		return pResult;
	}

private:
	void FinishInOrIoBuffer( CellSpursJobHeader * pHeader )
	{
		pHeader->sizeDmaList = uintp( m_pList ) - uintp( m_pListBegin );
		pHeader->sizeInOrInOut = m_sizeInOrInOut;
		pHeader->sizeCacheDmaList = m_sizeCacheDmaList;
	}

	// We can't DMA from / to the PPU stack, let's verify that
	bool IsAddressInStack( const void * pEa )
	{
#if IsPlatformPS3_PPU()
		uint64 fp = __reg(1);
		void * minStack = ( void* )( ( uint32 ) fp - 16 * 1024 );	// The 16 * 1024 should is not really necessary (as it means somebody addresses some portion that could be erased by the stack.
																	// Never the less, we want to be more conservative.
		void * maxStack = (void *)((uint32)fp + 64 * 1024);			// Assume that the stack is 64 Kb deep, make sure there is no allocations around if the stack is smaller
		return ( ( pEa >= minStack ) && ( pEa <= maxStack ) );
#else
		return false;
#endif
	}
};


#endif