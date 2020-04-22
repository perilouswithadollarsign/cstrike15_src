//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Handles all the functions for implementing remote access to the engine
//
//=============================================================================//

#include "netadr.h"
#include "sv_ipratelimit.h"
#include "convar.h"
#include "utlrbtree.h"
#include "utlvector.h"
#include "utlmap.h"
#include "../gcsdk/steamextra/tier1/utlhashmaplarge.h"
#include "filesystem.h"
#include "sv_log.h"
#include "tier1/ns_address.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


static ConVar	sv_max_queries_sec( "sv_max_queries_sec", "10.0", FCVAR_RELEASE, "Maximum queries per second to respond to from a single IP address." );
static ConVar	sv_max_queries_window( "sv_max_queries_window", "30", FCVAR_RELEASE, "Window over which to average queries per second averages." );
static ConVar	sv_max_queries_tracked_ips_max( "sv_max_queries_tracked_ips_max", "50000", FCVAR_RELEASE, "Window over which to average queries per second averages." );
static ConVar	sv_max_queries_tracked_ips_prune( "sv_max_queries_tracked_ips_prune", "10", FCVAR_RELEASE, "Window over which to average queries per second averages." );
static ConVar	sv_max_queries_sec_global( "sv_max_queries_sec_global", "500", FCVAR_RELEASE, "Maximum queries per second to respond to from anywhere." );
static ConVar	sv_logblocks("sv_logblocks", "0", FCVAR_RELEASE, "If true when log when a query is blocked (can cause very large log files)");

class CIPRateLimit
{
public:
	CIPRateLimit();
	~CIPRateLimit();

	// updates an ip entry, return true if the ip is allowed, false otherwise
	bool CheckIP( netadr_t ip );

	void Reset()
	{
		m_IPTimes.RemoveAll();
		m_IPStorage.RemoveAll();
		m_iGlobalCount = 0;
		m_lLastTime = -1;
		m_lLastDistributedDetection = -1;
		m_lLastPersonalDetection = -1;
	}

private:
	typedef int ip_t;
	struct iprate_val
	{
		long lastTime;
		int count;
		int32 idxiptime;
	};
	struct IpHashNoopFunctor
	{
		typedef uint32 TargetType;
		TargetType operator()( const ip_t &key ) const
		{
			return key;
		}
	};

	typedef CUtlHashMapLarge< ip_t, iprate_val, CDefEquals< ip_t >, IpHashNoopFunctor > IPStorage_t;
	IPStorage_t m_IPStorage;

	typedef CUtlMap< long, ip_t, int32, CDefLess< long > > IPTimes_t;
	IPTimes_t m_IPTimes;

	int m_iGlobalCount;
	long m_lLastTime;
	long m_lLastDistributedDetection;
	long m_lLastPersonalDetection;
};

static CIPRateLimit rateChecker;

//-----------------------------------------------------------------------------
// Purpose: return false if this IP exceeds rate limits
//-----------------------------------------------------------------------------
bool CheckConnectionLessRateLimits( const ns_address &adr )
{
	if ( !adr.IsType< netadr_t >() )
		return true;

	// This function can be called from socket thread, mutex around it
	static CThreadMutex s_mtx;
	AUTO_LOCK( s_mtx );

	bool ret = rateChecker.CheckIP( adr.AsType<netadr_t>() );
	if ( !ret && sv_logblocks.GetBool() == true )
	{
		g_Log.Printf("Traffic from %s was blocked for exceeding rate limits\n", ns_address_render( adr ).String() );	
	}
	return ret;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CIPRateLimit::CIPRateLimit()
{
	m_iGlobalCount = 0;
	m_lLastTime = -1;
	m_lLastDistributedDetection = -1;
	m_lLastPersonalDetection = -1;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CIPRateLimit::~CIPRateLimit()
{
}

//-----------------------------------------------------------------------------
// Purpose: return false if this IP has exceeded limits
//-----------------------------------------------------------------------------
bool CIPRateLimit::CheckIP( netadr_t adr )
{
	long curTime = (long)Plat_FloatTime();
	
	// check the per ip rate (do this first, so one person dosing doesn't add to the global max rate
	ip_t clientIP;
	memcpy( &clientIP, adr.ip, sizeof(ip_t) );
	
	int const MAX_TREE_SIZE = sv_max_queries_tracked_ips_max.GetInt();
	int const MAX_TREE_PRUNE = sv_max_queries_tracked_ips_prune.GetInt();
	
	// Prune some elements from the tree
	int numPruned = 0;
	for ( int32 itIPTime = m_IPTimes.FirstInorder(); ( itIPTime != m_IPTimes.InvalidIndex() ); )
	{
		int32 itIPTimeNext = m_IPTimes.NextInorder( itIPTime );
		ip_t ipTracked = m_IPTimes.Element( itIPTime );
		if ( ipTracked != clientIP )
		{
			if ( ( curTime - m_IPTimes.Key( itIPTime ) ) < sv_max_queries_window.GetFloat() )
				break; // need to still keep monitoring this IP address, time is in order so next ones are even more recent

			m_IPStorage.Remove( ipTracked );
			m_IPTimes.RemoveAt( itIPTime );
			++ numPruned;
			if ( ( numPruned >= MAX_TREE_PRUNE ) && ( m_IPStorage.Count() < MAX_TREE_SIZE ) )
				break;
		}
		itIPTime = itIPTimeNext;
	}

	if ( m_IPStorage.Count() > MAX_TREE_SIZE )
	{
		// This looks like we are under distributed attack where we are seeing a
		// very large number of IP addresses in a short time period
		// Stop tracking individual IP addresses and turn on global rate limit
		Msg( "IP rate limit detected distributed packet load (%u buckets, %u global count).\n", m_IPStorage.Count(), m_iGlobalCount );
		Reset();
		m_iGlobalCount = MAX( 1, ( sv_max_queries_sec_global.GetFloat() + 1 ) * ( sv_max_queries_window.GetFloat() + 1 ) );
		m_lLastTime = curTime;
		m_lLastDistributedDetection = curTime;
	}

	// now find the entry and check if it's within our rate limits
	bool bPerIpLimitingPerformed = false;
	IPStorage_t::IndexType_t ipEntry = m_IPStorage.Find( clientIP );
	if ( m_IPStorage.IsValidIndex( ipEntry ) )
	{
		bPerIpLimitingPerformed = true;
		iprate_val &iprateval = m_IPStorage.Element( ipEntry );
		if ( ( curTime - iprateval.lastTime ) > sv_max_queries_window.GetFloat() )
		{
			float query_rate = static_cast< float >( iprateval.count ) / sv_max_queries_window.GetFloat(); // add one so the bottom is never zero
			if ( query_rate > sv_max_queries_sec.GetFloat() )
			{
				if ( ( curTime - m_lLastPersonalDetection ) > sv_max_queries_window.GetFloat()/10 )
				{
					Msg( "IP rate limiting client %s sustained %u hits at %.1f pps (%u buckets, %u global count).\n", adr.ToString(), iprateval.count, query_rate, m_IPStorage.Count(), m_iGlobalCount );
				}
			}
			m_IPTimes.RemoveAt( iprateval.idxiptime );
			iprateval.idxiptime = m_IPTimes.Insert( curTime, clientIP );
			iprateval.lastTime = curTime;
			iprateval.count = 1;
		}
		else
		{
			++ iprateval.count;
			float query_rate = static_cast< float >( iprateval.count ) / sv_max_queries_window.GetFloat(); // add one so the bottom is never zero
			if ( query_rate > sv_max_queries_sec.GetFloat() )
			{
				if ( ( curTime - m_lLastPersonalDetection ) > sv_max_queries_window.GetFloat() )
				{
					m_lLastPersonalDetection = curTime;
					Msg( "IP rate limiting client %s at %u hits (%u buckets, %u global count).\n", adr.ToString(), iprateval.count, m_IPStorage.Count(), m_iGlobalCount );
				}
				return false;
			}
		}
	}
	
	// now check the global rate
	m_iGlobalCount++;
	
	if( (curTime - m_lLastTime) > sv_max_queries_window.GetFloat() )
	{
		float query_rate = static_cast< float >( m_iGlobalCount ) / sv_max_queries_window.GetFloat(); // add one so the bottom is never zero
		if ( query_rate > sv_max_queries_sec_global.GetFloat() )
		{
			if ( ( curTime - m_lLastDistributedDetection ) > sv_max_queries_window.GetFloat()/10 )
			{
				Msg( "IP rate limit sustained %u distributed packets at %.1f pps (%u buckets).\n", m_iGlobalCount, query_rate, m_IPStorage.Count() );
			}
		}
		m_lLastTime = curTime;
		m_iGlobalCount = 1;
	}
	else
	{
		float query_rate = static_cast<float>( m_iGlobalCount ) / sv_max_queries_window.GetFloat(); // add one so the bottom is never zero
		if( query_rate > sv_max_queries_sec_global.GetFloat() ) 
		{
			if ( ( curTime - m_lLastDistributedDetection ) > sv_max_queries_window.GetFloat() )
			{
				m_lLastDistributedDetection = curTime;
				Msg( "IP rate limit under distributed packet load (%u buckets, %u global count), rejecting %s.\n", m_IPStorage.Count(), m_iGlobalCount, adr.ToString() );
			}
			return false;
		}
	}

	if ( !bPerIpLimitingPerformed )
	{
		iprate_val iprateval;
		iprateval.count = 1;
		iprateval.lastTime = curTime;

		// not found, insert this new guy
		iprateval.idxiptime = m_IPTimes.Insert( curTime, clientIP );
		m_IPStorage.Insert( clientIP, iprateval );
	}

	return true;
}
