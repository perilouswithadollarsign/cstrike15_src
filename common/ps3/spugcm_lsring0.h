//========== Copyright © Valve Corporation, All rights reserved. ========
// Easy ("tier0") implementations of simple GCM contexts
//
#ifndef PS3_SPUGCM_LSRING0_HDR
#define PS3_SPUGCM_LSRING0_HDR

class CSpuGcmMeasureBuffer: public CellGcmContextData
{
public:
	CSpuGcmMeasureBuffer( )
	{
		this->begin = 0;
		this->end   = 0;
		this->current = 0;
		this->callback = CallbackDelegator;
	}

	uint GetSizeBytes()const
	{
		return uintp( this->end );
	}
	uint GetSizeWords()const
	{
		return this->end - this->begin;
	}
protected:
	void Callback( uint nCount )
	{
		this->end = ( uint32* )AlignValue( uintp( this->current + nCount ), 16 );
	}
	static int32_t CallbackDelegator( struct CellGcmContextData *pContext, uint32_t nCount )
	{
		static_cast<CSpuGcmMeasureBuffer*>( pContext )->Callback( nCount );
		return CELL_OK;
	}
};


class CSpuGcmAlignedBuffer: public CellGcmContextData
{
public:

	void Init( void * lsBuffer, uint lsBufferSize, uint eaBegin, uint nIoOffsetDelta )
	{
		uint nShift = ( eaBegin - uint( lsBuffer ) ) & 0x7F;
		this->begin = ( uint32* )( uintp( lsBuffer ) + nShift );
		m_eaBuffer = eaBegin;
		m_nIoOffsetDelta = nIoOffsetDelta;
		this->end = ( uint32* )( uintp( this->begin ) + lsBufferSize );
		this->current = this->begin;
		this->callback = CallbackDelegator;

		Assert( uint( this->begin ) >= uint( lsBuffer ) && !( 0x7F & ( uint( this->begin ) ^ eaBegin ) ) );
	}

	uint LsToLocalOffset( uint32 * lsCommand )
	{
		return EaToLocalOffset( LsToEa( lsCommand ) );
	}
	uint LsToEa( uint32 * lsCommand )
	{
		return uintp( lsCommand ) - uintp( this->begin ) + m_eaBuffer;
	}
	uint32 * EaToLs( uint32 eaCommand )
	{
		return ( uint32* )( ( eaCommand - m_eaBuffer ) + uintp( this->begin ) );
	}
	uint EaToLocalOffset( uint eaCommand )
	{
		return eaCommand + m_nIoOffsetDelta;
	}

	void AlignWithNops()
	{
		while ( 0xF & uintp( this->current ) )
		{
			*( this->current++ ) = CELL_GCM_METHOD_NOP;
		}
	}

	void AppendJumpToNext()
	{
		*( this->current++ ) = CELL_GCM_JUMP( LsToLocalOffset( this->current + 1 ) );
	}
	void AppendJumpToNextIfNeededForDmaPutJtn()
	{
		// the JTN is not needed if the whole buffer fits into 128-byte cache line
		Assert( this->current < this->end );
		if( ( uintp( this->current ) ^ ( uintp( this->end ) - 1 ) ) & -128 )
		{
			// the first and the last bytes are in separate cache lines; we need to insert JTN at the beginning
			AppendJumpToNext();			
		}
	}

	void Append( const SetLabelAlignedCommand_t & cmd )
	{
		AlignWithNops();
		*( vector unsigned int * )( this->current ) = cmd.m_cmd.m_vuCmd;
		this->current += 4;
	}


	// DMA put a segment of command buffer using JTN method. The start of the buffer shall be JTN, unless the buffer is small enough to be DMA'd
	void DmaPutJtn()
	{
		Assert( this->current <= this->end && !( 0x7F & ( uintp( this->begin ) ^ m_eaBuffer ) ) );
		while ( this->current < this->end )
		{
			*( this->current ++ ) = CELL_GCM_METHOD_NOP;
		}
		// skip the first 16 bytes where JTN resides; skip the whole cache line, while we're at it
		Assert( this->current <= this->end );
		uint32 * pRest = ( uint32* )( ( uintp( this->begin ) + 128 ) & -128 );
		Assert( pRest > this->begin );
		if ( pRest < this->end )
		{
			//VjobSpuLog( "lsring0 put %p..%p->%X tag:%d\n", pRest, this->end, LsToEa( pRest ), VJOB_IOBUFFER_DMATAG );
			Assert( !( 0x7F & LsToEa( pRest ) ) );
			VjobDmaPut( pRest, LsToEa( pRest ), uintp( this->end ) - uintp( pRest ), VJOB_IOBUFFER_DMATAG, 0, 0 );
			//VjobSpuLog( "lsring0 putf (JTN %X) %p..%p->%X tag:%d\n", *this->begin, this->begin, pRest, m_eaBuffer, VJOB_IOBUFFER_DMATAG );
			VjobDmaPutf( this->begin, m_eaBuffer, uintp( pRest ) - uintp( this->begin ), VJOB_IOBUFFER_DMATAG, 0, 0 );
		}
		else
		{
			// this case is pretty simple and doesn't require JTN at the beginning of the memory block , because it will be DMA'd atomically
			// check that we really start the block with JTN
			//Assert( CELL_GCM_JUMP( EaToLocalOffset( m_eaBuffer ) + 4 ) == *this->begin );
			// overwrite JTN with NOP as we won't need JTN
			//*this->begin = CELL_GCM_METHOD_NOP;
			Assert( pRest >= this->end && this->end - this->begin <= 128 / 4 );
			VjobDmaPutf( this->begin, m_eaBuffer, uintp( this->end ) - uintp( this->begin ), VJOB_IOBUFFER_DMATAG, 0, 0 );
		}
	}
protected:
	uint m_eaBuffer;
	uint m_nIoOffsetDelta;
protected:
	void Callback( uint nCount )
	{
		DebuggerBreak();
	}
	static int32_t CallbackDelegator( struct CellGcmContextData *pContext, uint32_t nCount )
	{
		static_cast<CSpuGcmAlignedBuffer*>( pContext )->Callback( nCount );
		return CELL_ERROR_ERROR_FLAG;
	}
};


#endif
