//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_FILTERS_H
#define SOS_OP_FILTERS_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"

//-----------------------------------------------------------------------------
// simple operator for setting a single position
//-----------------------------------------------------------------------------
struct CSosOperatorFloatFilter_t : CSosOperator_t
{

	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInput, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInputMaxVel, SO_SINGLE )

	float m_flPrevTime;
	float m_flPrevValue;

};

class CSosOperatorFloatFilter : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorFloatFilter )
};

#endif // SOS_OP_FILTERS_H