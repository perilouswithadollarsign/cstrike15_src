//===== Copyright © 2005-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: Format of the ps3optimalschedules.bin file, written
// by the ps3shaderoptimizer command line tool.
//
//===========================================================================//
#ifndef PS3OPTIMALSCHEDULESFMT_H
#define PS3OPTIMALSCHEDULESFMT_H

#ifdef _WIN32
#pragma once
#endif

#define OPTIMAL_COMBO_SCHEDULE_FILE_HEADER_ID 0xF15EB9A4

struct OptimalComboScheduleFileHeader_t
{
	uint m_nID;
	uint m_nNumCombos;
};

// 16 bytes per record, or ~8MB per 500k combos
#define COMBO_CYCLE_BITS 9
#define COMBO_SCHEDULE_BITS 10
#define COMBO_SEED_BITS 10
struct OptimalComboScheduleFileRecord_t
{
	uint64 m_nComboHash;

	uint64 m_nOptCycles		: COMBO_CYCLE_BITS;
	enum { cDefaultScheduleIndex = ( 1 << COMBO_SCHEDULE_BITS ) - 1 };
	uint64 m_nOptSchedule	: COMBO_SCHEDULE_BITS;
	uint64 m_nOptSeed		: COMBO_SEED_BITS;
	uint64 m_nAltSchedule0	: COMBO_SCHEDULE_BITS;
	uint64 m_nAltSchedule1	: COMBO_SCHEDULE_BITS;
	uint64 m_nAltSchedule2	: COMBO_SCHEDULE_BITS;
	uint64 m_nUnused		: 5;

	bool operator< ( const OptimalComboScheduleFileRecord_t &rhs ) const
	{
		return m_nComboHash < rhs.m_nComboHash;
	}
};

enum ShaderSchedulerParamSource_t
{
	SHADER_SCHEDULER_PARAM_SOURCE_UNOPTIMIZED,
	SHADER_SCHEDULER_PARAM_SOURCE_UNOPTIMIZED_FALLBACK,
	SHADER_SCHEDULER_PARAM_SOURCE_FOUND_OPTIMAL,
	SHADER_SCHEDULER_PARAM_SOURCE_FROM_SCHEDULER_FILE,
	
	TOTAL_SHADER_SCHEDULER_PARAM_SOURCES
};

#endif // PS3OPTIMALSCHEDULESFMT_H
