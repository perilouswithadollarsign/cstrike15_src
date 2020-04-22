//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "pch_tier0.h"

#include "tier0/perfstats.h"

CPerfStatsData g_PerfStats;

CPerfStatsData::CPerfStatsData()
{
	Reset();
}

void CPerfStatsData::Tick()
{
	if ( m_nFrames != 0 )
	{
		CPerfStatsSlotData* pMainSlot = &m_Slots[PERF_STATS_SLOT_MAINTHREAD];
		pMainSlot->EndTimer();

		CPerfStatsSlotData* pMainNoWaitSlot = &m_Slots[PERF_STATS_SLOT_MAINTHREAD_NOWAIT];
		pMainNoWaitSlot->m_pszName = "MainThreadNoWait";
		CCycleCount::Sub(pMainSlot->m_CurrFrameTime, m_Slots[PERF_STATS_SLOT_END_FRAME].m_CurrFrameTime, pMainNoWaitSlot->m_CurrFrameTime);
		pMainNoWaitSlot->m_AccTotalTime += pMainNoWaitSlot->m_CurrFrameTime;
	}
	
	++m_nFrames;
	for ( int iSlot = 0; iSlot < PERF_STATS_SLOT_MAX; ++iSlot )
	{
		m_Slots[iSlot].m_PrevFrameTime = m_Slots[iSlot].m_CurrFrameTime;
		m_Slots[iSlot].ResetFrameStats();
	}

	m_Slots[PERF_STATS_SLOT_MAINTHREAD].StartTimer("MainThread");
}

void CPerfStatsData::Reset()
{
	m_nFrames = 0;

	for ( int iSlot = 0; iSlot < PERF_STATS_SLOT_MAX; ++iSlot )
	{
		m_Slots[iSlot].Reset();
	}
}