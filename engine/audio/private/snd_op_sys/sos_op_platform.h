//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_PLATFORM_H
#define SOS_OP_PLATFORM_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"


//-----------------------------------------------------------------------------
// mapname
//-----------------------------------------------------------------------------
struct CSosOperatorPlatform_t : CSosOperator_t
{

	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )

};
class CSosOperatorPlatform : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorPlatform )

};

#endif // SOS_OP_PLATFORM_H