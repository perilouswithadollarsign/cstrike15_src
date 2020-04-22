//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_DISTANT_DSP_H
#define SOS_OP_DISTANT_DSP_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"

//-----------------------------------------------------------------------------
// basic dsp by distance operator
//-----------------------------------------------------------------------------
struct CSosOperatorDistantDSP_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInputDist, SO_SINGLE )
		SOS_INPUT_FLOAT( m_flInputLevel, SO_SINGLE )
		SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
};

class CSosOperatorDistantDSP : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorDistantDSP )
};

#endif // SOS_OP_DISTANT_DSP_H