//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_POS_VEC8_H
#define SOS_OP_POS_VEC8_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"


//-----------------------------------------------------------------------------
// simple operator for setting a single position
//-----------------------------------------------------------------------------
struct CSosOperatorPosVec8_t : CSosOperator_t
{
	SOS_OUTPUT_FLOAT( m_flOutPosition, SO_VEC3 )
	SOS_OUTPUT_FLOAT( m_flOutMaxIndex, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInputIndex, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInputEntryCount, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInputPos0, SO_VEC3 )
	SOS_INPUT_FLOAT( m_flInputPos1, SO_VEC3 )
	SOS_INPUT_FLOAT( m_flInputPos2, SO_VEC3 )
	SOS_INPUT_FLOAT( m_flInputPos3, SO_VEC3 )
	SOS_INPUT_FLOAT( m_flInputPos4, SO_VEC3 )
	SOS_INPUT_FLOAT( m_flInputPos5, SO_VEC3 )
	SOS_INPUT_FLOAT( m_flInputPos6, SO_VEC3 )
	SOS_INPUT_FLOAT( m_flInputPos7, SO_VEC3 )

};

class CSosOperatorPosVec8 : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorPosVec8 )
};

#endif // SOS_OP_POS_VEC8_H