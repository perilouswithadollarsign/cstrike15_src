//========== Copyright © Valve Corporation, All rights reserved. ========
//
// Producer-consumer FIFO ring buffers
// THese are shared between SPU and PPU, and define some common misc functions
// 
//
#ifndef VJOBS_PCRING_HDR
#define VJOBS_PCRING_HDR

#include "ps3/ps3_gcm_config.h"
#include "ps3/spu_job_shared.h"

struct ALIGN16 SetLabelAlignedCommand_t
{
	union CmdUnion_t
	{
		uint32 m_nCmd[4];
		vector unsigned int m_vuCmd;
	};
	CmdUnion_t m_cmd;
	// 		uint32 m_nMethodSetSemaphoreOffset;
	// 		uint32 m_nSemaphoreOffset;
	// 		uint32 m_nMethodSemaphoreRelease;
	// 		uint32 m_nSemaphoreValue;
	void SetWriteTextureLabel( uint nIndex, uint nValue )
	{
		uint offset = 0x10 * nIndex;

#ifdef SPU
		m_cmd.m_vuCmd = ( vector unsigned int )
		{
			CELL_GCM_METHOD(CELL_GCM_NV4097_SET_SEMAPHORE_OFFSET, 1),
				(offset),
				CELL_GCM_METHOD(CELL_GCM_NV4097_TEXTURE_READ_SEMAPHORE_RELEASE, 1),
				(nValue)
		};
#else
		uint32 * p = m_cmd.m_nCmd;

		CELL_GCM_METHOD_SET_SEMAPHORE_OFFSET(p, offset);
		CELL_GCM_METHOD_TEXTURE_READ_SEMAPHORE_RELEASE(p, nValue);
#endif
	}
	
	void UpdateWriteTextureLabel( uint nValue )
	{
		m_cmd.m_nCmd[3] = nValue;
	}
	
	uint32 GetWriteTextureLabel( )
	{
		return m_cmd.m_nCmd[3];
	}
}
ALIGN16_POST;

// read-only part of pcring
struct ALIGN16 PcRingRo_t
{
	SetLabelAlignedCommand_t m_head[2]; // the variants of the head sync 
	
	void Init( uint nLabel );
};

//
// producer-consumer FIFO ring buffer for command buffer.
// Kept in main memory, controlled/produced by SPU, consumed by RSX
//
class ALIGN16 SysFifo
{
public:
	uint32 m_eaBuffer;     // the buffer begin, EA
	uint32 m_nSize;        // the whole buffer size
	// the important thing here is that put and end pointers are independent
	// and can be updated from different threads lock-free, wait-free
	// we're putting into Put segment; we can increment it until we hit "end", at which point we need to wait for RSX to eat up and move the "End" forward
	uint32 m_nPut, m_nEnd; // high bit means odd-even ring
	
	enum {ODD_BIT = 0x80000000};
	
	void Init( uintp eaBuffer, uint nSize, uint nPut = 0 )
	{
		// put may be anywhere (it must be the GCM control register PUT, realtively to eaBuffer), but it must be aligned and within the buffer
		Assert( nPut < nSize && !( 0xF & nPut ) );
		m_eaBuffer = ( uint32 )eaBuffer;
		m_nSize = nSize;
		m_nPut = nPut;
		m_nEnd = ODD_BIT;
	}
	
	void HardReset()
	{
		m_nPut = 0;
		m_nEnd = ODD_BIT;
	}
	
	// must wrap before put?
	bool MustWrap( uint nPutBytes ) const
	{
		return ( ( m_nPut + nPutBytes ) & ~ODD_BIT ) > m_nSize;
	}
	
	bool IsOrdered( uint nSignal0, uint nSignal1 )
	{
		if( ( nSignal0 ^ nSignal1 ) & ODD_BIT )
		{
			return ( nSignal1 & ~ODD_BIT ) <= ( nSignal0 & ~ODD_BIT );
		}
		else
		{
			return nSignal0 <= nSignal1;
		}
	}
	
	bool CanPutNoWrap( uint nPutBytes ) const
	{
		Assert( !MustWrap( nPutBytes ) );
		if ( ( m_nPut ^ m_nEnd ) & ODD_BIT )  // bits are different => we have enough space till the end of the buffer
		{
			Assert( ( m_nPut | ODD_BIT ) >= ( m_nEnd | ODD_BIT ) ); // the End must be trailing behind Put , only in the NEXT ring
			return true;
		}
		else
		{
			// bits are the same  => we have continuous unsigned range between put, put+add, end
			// we don't want to put commands up to m_nEnd because theoretically we can get to the situation when put==get and RSX will skip the whole SYSring
			return ( m_nPut + nPutBytes < m_nEnd );  
		}
	}
	
	bool CanWrapAndPut( uint nPutBytes )const
	{
		if ( ( m_nPut ^ m_nEnd ) & ODD_BIT ) // to wrap, "end" must be in the next ring
		{
			// Important: Assume that we'll reset Put to 0 when we put ... and "add" must be before "end"
			// we don't want to put commands up to m_nEnd because theoretically we can get to the situation when put==get and RSX will skip the whole SYSring
			return ( nPutBytes < ( m_nEnd & ~ODD_BIT ) ); 
		}
		else
		{
			Assert( m_nPut <= m_nEnd );	// the End must be in front of Put, since it's in the same ring
			return false;
		}
	}
	
	void Wrap( )
	{
		Assert( ( m_nPut ^ m_nEnd ) & ODD_BIT );
		m_nPut = ( ~m_nPut ) & ODD_BIT; // begin from the start, only in the next ring (invert odd/even)
	}

	// prepare to Put(nBytes); wrap if necessary; don't do anything unless subsequent Put(nBytes) is valid
	enum PreparePutEnum_t
	{
		PUT_PREPARED_WRAPPED,
		PUT_PREPARED_NOWRAP,
		PUT_PREPARE_FAILED
	};
	
	
	PreparePutEnum_t PreparePut( uint nBytes )	
	{
		if( MustWrap( nBytes ) )
		{
			if( CanWrapAndPut( nBytes ) )
			{
				Wrap();
				Assert( CanPutNoWrap( nBytes ) );
				return PUT_PREPARED_WRAPPED;
			}
		}
		else
		{
			if( CanPutNoWrap( nBytes ) )
			{
				return PUT_PREPARED_NOWRAP;
			}
		}
		return PUT_PREPARE_FAILED;
	}
	
	// NOTE: the guarantee of this function is that multiple Puts are additive: Put(100) is equivalent to Put(25),Put(75) and such
	void Put( uint nPutBytes )
	{
		Assert( CanPutNoWrap( nPutBytes ) );
		m_nPut += nPutBytes;
	}
	
	uint EaPut( )const
	{
		return m_eaBuffer + ( m_nPut & ~ODD_BIT );
	}
	
	uint PutToEa( uint nPut )const
	{
		return m_eaBuffer + ( nPut & ~ODD_BIT );
	}
	
	uint EaWrapAndPut()const // EA of PUT after Wrap() is executed
	{
		return m_eaBuffer + sizeof( SetLabelAlignedCommand_t ); 
	}
	
	// how much memory is left in this ring, without Wrapping?
	int GetNoWrapCapacity()const
	{
		return m_nSize - ( m_nPut & ~ODD_BIT );
	}
	
	// returns a value that will signal that the buffer has been processed to m_nPut pointer
	uint GetSignal()const
	{
		return m_nPut ^ ODD_BIT;
	}
	
	// FFFFFFFF would imply a 2-Gb buffer, which we clearly can't have on PS3
	static uint GetInvalidSignal() { return 0xFFFFFFFF; }
	
	const SetLabelAlignedCommand_t * GetHead( PcRingRo_t &ro )const
	{
		return &ro.m_head[m_nPut >> 31];
	}
	const SetLabelAlignedCommand_t * GetNextHead( PcRingRo_t &ro )const
	{
		return &ro.m_head[( ~m_nPut ) >> 31];
	}
	
	// notify about a signal coming in asynchronously , must be a result of GetSignal() after Put()
	void NotifySignal( uint nSignal )
	{
		AssertSpuMsg( ( nSignal & ~ODD_BIT ) <= m_nSize, "{ea=0x%X,size=0x%X,put=0x%X,end=0x%X}.NotifySignal(0x%X)\n", m_eaBuffer, m_nSize, m_nPut, m_nEnd, nSignal );
		if( SPUGCM_ENABLE_NOTIFY_RSX_GET )
		{
			// we can artificially set the signal ahead sometimes, because we have 2 streams of signals from RSX :
			// THe control register GET and the cmd buffer label (GCM_LABEL_SYSRING_SIGNAL)
			// so we'll filter extra signals here: we may NOT step back
			
			if( !( ( nSignal ^ m_nEnd ) & ODD_BIT ) && nSignal < m_nEnd )
			{
				return;// skip this: signal and end are in the same ring and signal is earlier than end
			}
		}
		
		AssertSpuMsg( ( ( nSignal ^ m_nPut ) & ODD_BIT ) ? 
			( nSignal & ~ODD_BIT ) <= ( m_nPut & ~ODD_BIT )// signal and put are in different rings
			:
			nSignal >= m_nPut, // signal and put are in the same ring
			"{ea=0x%X,size=0x%X,put=0x%X,end=0x%X}.NotifySignal(0x%X)\n",
			m_eaBuffer, m_nSize, m_nPut, m_nEnd, nSignal
		);
		m_nEnd = nSignal;
		
	}
	
	// NotifySignal() version that can tolerate outdated signals due to different latencies between SPU and RSX
	void NotifySignalSafe( uint nSignal )
	{
		if( ( ( nSignal ^ m_nPut ) & ODD_BIT ) ? 
			  ( nSignal & ~ODD_BIT ) <= ( m_nPut & ~ODD_BIT )// signal and put are in different rings
			  :
			  nSignal >= m_nPut ) // signal and put are in the same ring
		{
			m_nEnd = nSignal;
		}
	}
	
	bool IsSignalDifferent( uint nSignal )
	{
		return m_nEnd != nSignal;
	}
	
	// WARNING this is a debug-only function. Do not use for anything but debugging, because it's slow
	// and because it will signal "finished" incorrectly when the whole ring is full 
	// Expects RSX get relative to the base of the buffer (i.e. 0 when Get == the byte 0 of this PCring)
	bool NotifyRsxGet( uint nRsxControlRegisterGet )
	{
		if( nRsxControlRegisterGet == ( m_nPut & ~ODD_BIT ) )
		{
			m_nEnd = m_nPut ^ ODD_BIT; // assume this means we've processed all SYSRING buffer
			return true;
		}
		else
		{
			return false;
		}
	}

	bool IsRsxFinished( uint nRsxControlRegisterGet )
	{
		return nRsxControlRegisterGet == ( m_nPut & ~ODD_BIT );
	}
	
	bool IsDone()const
	{
		return ( m_nEnd == m_nPut ^ ODD_BIT );
	}
}
ALIGN16_POST;


inline void PcRingRo_t::Init( uint nLabel )
{
	m_head[0].SetWriteTextureLabel( nLabel, SysFifo::ODD_BIT | sizeof( SetLabelAlignedCommand_t ) ); // m_nPut ==       0 -> signal == ODD_BIT
	m_head[1].SetWriteTextureLabel( nLabel, sizeof( SetLabelAlignedCommand_t ) ); // m_nPut == ODD_BIT -> signal == 0
}


#endif
