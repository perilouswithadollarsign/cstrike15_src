//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_OUTPUT_H
#define SOS_OP_OUTPUT_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"



//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
enum SOSOutputType_t
{
	SOS_OUT_NONE = 0,
	SOS_OUT_VOLUME,
	SOS_OUT_DSP,
	SOS_OUT_POSITION,
	SOS_OUT_SPEAKERS,
	SOS_OUT_FACING,
	SOS_OUT_DISTVAR,
	SOS_OUT_PITCH,
	SOS_OUT_DELAY,
	SOS_OUT_STOPHOLD,
	SOS_OUT_MIXLAYER_TRIGGER,
	SOS_OUT_SAVE_RESTORE,
	SOS_OUT_BLOCK_START,
	SOS_OUT_PHONON_XFADE,
};
struct CSosOperatorOutput_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInputFloat, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInputVec3, SO_VEC3 )
	SOS_INPUT_FLOAT( m_flInputSpeakers, SO_SPEAKERS )

	SOSOutputType_t m_nOutType;
};

class CSosOperatorOutput : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorOutput )
};


#endif // SOS_OP_OUTPUT_H
