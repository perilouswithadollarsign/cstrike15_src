//================ Copyright (c) Valve Corporation. All Rights Reserved. ===========================
//
// SPU Profiling
//
//==================================================================================================

#ifndef INCLUDED_CELLMGR_SPU_PROFILE_H
#define INCLUDED_CELLMGR_SPU_PROFILE_H

//--------------------------------------------------------------------------------------------------
// Headers
//--------------------------------------------------------------------------------------------------

#include <stdint.h>

//--------------------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------------------

// Uncomment to enabled profiling
//#define ENABLE_SPU_PROFILE

//--------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------

const int NUM_BOOKMARKS_IN_EVENT = 6;

//--------------------------------------------------------------------------------------------------
// Functions
//--------------------------------------------------------------------------------------------------

/*
* Insert a marker that is displayed in Tuner
*/ 
void insert_bookmark( uint32_t bookmark );

/*
* 400 cycles delay per bookmark when emitting bookmarks on multiple SPUs
*/
void bookmark_delay( int NumBookmarks );

/*
* Inserting 6 SPU bookmarks, which will
* be identified by Tuner as a start event
*/
void raw_spu_prof_start( int iLevel, uint16_t lsa );

/*
* Inserting 6 SPU bookmarks, which will
* be identified by Tuner as a stop event
*/
void raw_spu_prof_stop( uint16_t lsa );

/*
*Profiling macros
*/
#ifdef ENABLE_SPU_PROFILE

#define BEGIN_PROFILE(level) raw_spu_prof_start(level, 0)
#define END_PROFILE(level) raw_spu_prof_stop(level)

#define BEGIN_BOOKMARK(colour) insert_bookmark( colour )
#define END_BOOKMARK(colour)

#else

#define BEGIN_PROFILE(level)
#define END_PROFILE(level)

#define BEGIN_BOOKMARK(colour)
#define END_BOOKMARK(colour)

#endif

#endif // INCLUDED_CELLMGR_SPU_PROFILE_H
