//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_UTIL_H
#define SOS_OP_UTIL_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"


//-----------------------------------------------------------------------------
// basic mixgroup operator
//-----------------------------------------------------------------------------
struct CSosOperatorPrintFloat_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInput, SO_SINGLE )

};

class CSosOperatorPrintFloat : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorPrintFloat )
};

#endif // SOS_OP_UTIL