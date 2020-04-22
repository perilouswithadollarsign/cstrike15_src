//========== Copyright © Valve Corporation, All rights reserved. ========
#ifndef JOB_NOTIFY_HDR 
#define JOB_NOTIFY_HDR

#include "ps3/spu_job_shared.h"

namespace job_notify
{
	struct ALIGN16 NotifyArea_t
	{
		uint32 m_nCopyTo;
		uint32 m_nCopyFrom;
		uint32 m_nSpuId;
	}ALIGN16_POST;

}
#endif

