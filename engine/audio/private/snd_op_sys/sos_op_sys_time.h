//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_SYS_TIME_H
#define SOS_OP_SYS_TIME_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"


//-----------------------------------------------------------------------------
// get elapsed time
//-----------------------------------------------------------------------------
struct CSosOperatorSysTime_t : CSosOperator_t
{
	SOS_OUTPUT_FLOAT( m_flOutputClientElapsed, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputHostElapsed, SO_SINGLE )

};

class CSosOperatorSysTime : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorSysTime )

};


#endif // SOS_OP_SYS_TIME_H
