//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
//
//
//==================================================================================================

//--------------------------------------------------------------------------------------------------
// Headers
//--------------------------------------------------------------------------------------------------

#include "Profile.h"
#include <spu_intrinsics.h>

//--------------------------------------------------------------------------------------------------
// Functions
//--------------------------------------------------------------------------------------------------

/*
* Insert a marker that is displayed in Tuner
*/ 
void insert_bookmark( uint32_t bookmark )
{
	__asm__ volatile ("wrch $69, %0" :: "r" (bookmark));
	// Must wait for 16 cycles
	__asm__ volatile ("nop;nop;nop;nop;nop;nop;nop;nop");
	__asm__ volatile ("nop;nop;nop;nop;nop;nop;nop;nop");
}

void bookmark_delay( int NumBookmarks )
{
	// 400 cycles per bookmark when emitting bookmarks on both SPUs
	for ( int i=0; i<NumBookmarks*400/8; i++)
	{
		__asm__ volatile ("nop;nop;nop;nop;nop;nop;nop;nop");
	}
}

/*
* Inserting 6 SPU bookmarks, which will
* be identified by Tuner as a start event
*/
void raw_spu_prof_start( int iLevel, uint16_t lsa )
{
	typedef union { char c4[4]; uint16_t u16[2]; uint32_t u32; } Module_u;
	static Module_u s_mu = { { 't', 'e', 's', 't' } };

	insert_bookmark( 0xffaa );		// start marker 1
	insert_bookmark( s_mu.u16[0] );	// name
	insert_bookmark( s_mu.u16[1] );	// name
	insert_bookmark( iLevel );		// level
	insert_bookmark( lsa >> 2 );	// LSA is shifted by 2 as per the SPURS spec.
	insert_bookmark( 0xffab );		// start marker 2
	bookmark_delay( NUM_BOOKMARKS_IN_EVENT );
}

/*
* Inserting 6 SPU bookmarks, which will
* be identified by Tuner as a stop event
*/
void raw_spu_prof_stop( uint16_t lsa )
{
	typedef union { uint16_t u16[4]; uint64_t u64; } GUID_u;
	GUID_u guid;

	qword insn = si_roti(*(qword*)(0x80 + lsa), 7);
	qword pattern = (qword)(vec_uchar16){0,1,4,5,8,9,12,13,0,1,4,5,8,9,12,13};
	guid.u64 = si_to_ullong(si_shufb(insn, insn, pattern));

	insert_bookmark( 0xffac );		// start marker 1
	insert_bookmark( guid.u16[0] );	// guid
	insert_bookmark( guid.u16[1] );	// guid
	insert_bookmark( guid.u16[2] );	// guid
	insert_bookmark( guid.u16[3] );	// guid
	insert_bookmark( 0xffad );		// start marker 2
	bookmark_delay( NUM_BOOKMARKS_IN_EVENT );
}
