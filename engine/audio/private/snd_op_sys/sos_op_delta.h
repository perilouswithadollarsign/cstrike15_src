//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_DELTA_H
#define SOS_OP_DELTA_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"


//-----------------------------------------------------------------------------
// get elapsed time
//-----------------------------------------------------------------------------
struct CSosOperatorDelta_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInput, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
	
	float m_flLastInput;
	float m_flLastTime;

};

class CSosOperatorDelta : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorDelta )

};


#endif // SOS_OP_DELTA_VALUE_H
