//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_FALLOFF_H
#define SOS_OP_FALLOFF_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"


//-----------------------------------------------------------------------------
// falloff
//-----------------------------------------------------------------------------
struct CSosOperatorFalloff_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInputDist, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInputLevel, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
};
class CSosOperatorFalloff : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorFalloff )

};


//-----------------------------------------------------------------------------
// falloff
//-----------------------------------------------------------------------------
struct CSosOperatorFalloffTail_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInputDist, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInputAtten, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInputDistantMin, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInputDistantMax, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInputExp, SO_SINGLE )

// 	SOS_INPUT_FLOAT( m_flInputTailMin, SO_SINGLE )
// 	SOS_INPUT_FLOAT( m_flInputTailMax, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
};
class CSosOperatorFalloffTail : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorFalloffTail )

};

#endif // SOS_OP_FALLOFF