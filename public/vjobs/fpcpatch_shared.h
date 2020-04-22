//========== Copyright © Valve Corporation, All rights reserved. ========
#if !defined( JOB_FPCPATCH_SHARED_HDR ) && defined( _PS3 )
#define JOB_FPCPATCH_SHARED_HDR


#include "ps3/spu_job_shared.h"
#include "ps3/ps3_gcm_config.h"

namespace job_fpcpatch
{

// On PS/3, fragment programs have a maximum of 256 constant patch-ups that can be applied to each shader
// PS 2.0 defines 96 as minimum, but I'm not sure how many we are actually using
enum GlobalConstEnum_t
{
	MAX_VIRTUAL_CONST_COUNT = MAX_FPCP_VIRTUAL_CONST_COUNT,
	FLAG_PUT_STATE          = 1, // DMA the new state out when done; must be synchronized by vjobs code  running at PPU
	FLAG_BREAK_JOB          = 2,
	FLAG_DEFER_STATE        = 4,
	FLAG_UNDEFER_STATE      = 8 // this flag must fit in the lower 4 bits, because deferred state may be 16-byte aligned, not 128-byte aligned
};



union ConstRangeHeader_t
{
	fltx4 m_f4;
	struct
	{
		uint32 m_nStart;
		uint32 m_nCount;
	} m_u32;
};


struct ALIGN16 FpcPatchStateHeader_t
{
#ifndef SPU
	volatile // there's no need to treat this as volatile on SPU
#endif
	uint32 m_nStartRanges;        // the start index of ConstRangeHeader_t
	uint32 m_nBufferMask;       // the number of Qwords in the buffer - 1
	FpcPatchStateHeader_t * m_eaThis;
	uint32 m_nThisStatePatchCounter; // the patch counter corresponding to this state (the job at which it was up to date)
	uint32 m_eaThisStateJobDescriptor;
	uint32 m_nDebuggerBreak;
}
ALIGN16_POST;

struct ALIGN128	FpcPatchState_t: FpcPatchStateHeader_t
{
	// virtual const register states
	fltx4 m_reg[MAX_VIRTUAL_CONST_COUNT];

	fltx4 * GetBufferStart()
	{
		return ( fltx4* )( this + 1 ) ;    // the buffer start address
	}
}
ALIGN128_POST;

}

namespace job_fpcpatch2
{
using job_fpcpatch::MAX_VIRTUAL_CONST_COUNT;
using job_fpcpatch::FLAG_PUT_STATE;
using job_fpcpatch::FLAG_BREAK_JOB;
using job_fpcpatch::FLAG_DEFER_STATE;
using job_fpcpatch::FLAG_UNDEFER_STATE;
using job_fpcpatch::ConstRangeHeader_t;
using job_fpcpatch::FpcPatchStateHeader_t;
using job_fpcpatch::FpcPatchState_t;

struct ALIGN16 FpHeader_t
{
	uint32 m_nUcodeSize;
	// patches follow Ucode; patch is struct{ uint16 nConstIndex; uint16 nConstOffset; }
	// the offset is a qword index ( offset from the start of Ucode div 16 )
	uint32 m_nPatchCount;

	uint32 m_nShaderControl0;
	uint32 m_nTexControls; //   Always <= 16; 1 tex control corresponds to 2 words in the tex control table

	// the dma size without the texcontrols
	uint GetDmaSize() const
	{
		return sizeof( *this ) + m_nUcodeSize + m_nPatchCount * sizeof( uint32 );
	}
	static uintp GetUcodeEa( uint eaFpHeader )
	{
		return eaFpHeader + sizeof( FpHeader_t );
	}
	uintp GetPatchTableEa( uint eaFpHeader )const
	{
		return GetUcodeEa( eaFpHeader ) + m_nUcodeSize;
	}
	uintp GetTexControlsEa( uint eaFpHeader )const
	{
		return GetPatchTableEa( eaFpHeader ) + AlignValue( m_nPatchCount * sizeof( uint32 ), 16 );
	}
	uintp GetTexControlsBytes( )const
	{
		return sizeof( uint32 ) * 2 * m_nTexControls;
	}
#if !defined( SPU )
	const void * GetUcode()const
	{
		return ( void* )GetUcodeEa( ( uintp ) this );
	}
	const uint32 * GetPatchTable( )const
	{
		return ( uint32* )( uintp( GetUcode() ) + m_nUcodeSize );
	}
	const uint32 * GetTexControls()const
	{
		return ( uint32* )GetTexControlsEa( ( uintp )this );
	}
#endif
}
ALIGN16_POST;

}



#endif

