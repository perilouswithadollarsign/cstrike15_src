//========== Copyright © Valve Corporation, All rights reserved. ========
#ifndef VJOBS_ACCUMPOSE_SHARED_HDR
#define VJOBS_ACCUMPOSE_SHARED_HDR

#include "ps3/spu_job_shared.h"


struct PS3BoneJobData;

namespace job_accumpose
{
	typedef CellSpursJob128 JobDescriptor_t;

	struct JobParams_t
	{
		int				m_testInt_IN;

		int				m_testInt_OUT;
	};

	inline JobParams_t * GetJobParams( void *pJob )
	{
		return VjobGetJobParams< JobParams_t, JobDescriptor_t >( pJob );
	}	

}

#endif