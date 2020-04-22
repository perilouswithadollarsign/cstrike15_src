//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_SPATIALIZE_H
#define SOS_OP_SPATIALIZE_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"


//-----------------------------------------------------------------------------
// spatialize a sound in the speakers
//-----------------------------------------------------------------------------
struct CSosOperatorSpatializeSpeakers_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInputPosition, SO_VEC3 )
		SOS_INPUT_FLOAT( m_flInputDistance, SO_SINGLE )
		SOS_INPUT_FLOAT( m_flInputRadiusMax, SO_SINGLE )
		SOS_INPUT_FLOAT( m_flInputRadiusMin, SO_SINGLE )
		SOS_INPUT_FLOAT( m_flInputTimeStartStereoSpread, SO_SINGLE)
		SOS_INPUT_FLOAT( m_flInputTimeFinishStereoSpread, SO_SINGLE)
		SOS_INPUT_FLOAT( m_flInputFinalStereoSpread, SO_SINGLE )
		SOS_INPUT_FLOAT( m_flInputRearStereoScale, SO_SINGLE )
		SOS_OUTPUT_FLOAT( m_flOutput, SO_SPEAKERS )

};

class CSosOperatorSpatializeSpeakers : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorSpatializeSpeakers )
};

#endif // SOS_OP_SPATIALIZE_H