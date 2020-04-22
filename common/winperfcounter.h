//========= Copyright © 1996-2006, Valve LLC, All rights reserved. ============
//
// Purpose:     routines to access Windows Performance counter data
//
//=============================================================================

#ifndef WINPERFCOUNTER_H
#define WINPERFCOUNTER_H

#ifdef _WIN32
#pragma once

enum EFormat
{
	k_EFormatInt = 0,		// Signed int
	k_EFormatFloat,			// Floating point
};

struct PerfCounter_t
{
	const char *m_rgchPerfObject;
	const char *m_rgchPerfObjectAlternative; // alternative object to query if the first one is not found - can be NULL
	size_t m_statsOffset;
	EFormat m_eFmt;
	float m_fUnsetValue;
	bool m_bAssertOnFailure;
	bool m_bCounterRequiresRollup;  // Counters requiring rollup should be adjacent
};

class CWinPerfCountersPriv;

class CWinPerfCounters
{
public:
	CWinPerfCounters( );
	~CWinPerfCounters();
	bool Init( const PerfCounter_t *counterMap, int nCounters );
	bool TakeSample();
	bool WriteStats( void *pStatsStruct );
	void Shutdown();
#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );		// Validate our internal structures
#endif // DBGFLAG_VALIDATE

private:
	CWinPerfCountersPriv *m_pPrivData;
	const PerfCounter_t *m_pPerfCounterMap;
	int m_nCounters;
	bool m_bInited;
};

class CWinNetworkPerfCounters
{
public:
	CWinNetworkPerfCounters( );
	~CWinNetworkPerfCounters();
	bool Init();
	bool TakeSample();
	bool WriteStats( uint64 *pu64BytesSentPerSec, uint64 *pu64BytesRecvPerSec );
	void Shutdown();
#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );		// Validate our internal structures
#endif // DBGFLAG_VALIDATE

private:
	CWinPerfCounters m_PerfCounters;

	static const uint32 sm_unMaxNetworkInterfacesToMeasure = 32;
	uint32 m_unNumInterfaces;

	struct Stats_t
	{
		uint32 m_rgunNetworkBytesSentStats[sm_unMaxNetworkInterfacesToMeasure];
		uint32 m_rgunNetworkBytesReceivedStats[sm_unMaxNetworkInterfacesToMeasure];
	} m_Stats;

	// One each for bytes sent and received
	PerfCounter_t m_rgPerfCounterInfo[2 * sm_unMaxNetworkInterfacesToMeasure];
	bool m_bInited;

};

#endif /* _WIN32 */

#endif /* WINPERFCOUNTER_H */
