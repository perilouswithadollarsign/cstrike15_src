//====== Copyright Valve Corporation, All rights reserved. =======
#ifndef GOTV_HTTP_STREAM_HDR
#define GOTV_HTTP_STREAM_HDR

#include "tier1/utlhashtable.h"

struct GotvHttpStreamId_t
{
	GotvHttpStreamId_t()
	{
		m_nMatchId = 0;
		m_nInstanceId = 0;
	}
	uint64 m_nMatchId ;
	uint m_nInstanceId;

	bool operator < ( const GotvHttpStreamId_t &right ) const
	{
		return m_nMatchId == right.m_nMatchId ? m_nInstanceId < right.m_nInstanceId : m_nMatchId < right.m_nMatchId;
	}

	bool operator == ( const GotvHttpStreamId_t &right ) const
	{
		return m_nMatchId == right.m_nMatchId && m_nInstanceId == right.m_nInstanceId;
	}
};


// the match id is already a hash (guid) consisting of a random upper uint32 and a counter in lower uint32. So we could actually conceivably use the lower uint32
template <> struct DefaultHashFunctor< GotvHttpStreamId_t > { unsigned int operator()( const GotvHttpStreamId_t &id ) const { return Mix64HashFunctor().operator()( (id.m_nMatchId << 1) + id.m_nInstanceId ); } };

#endif // DEMO_STREAM_HDR
