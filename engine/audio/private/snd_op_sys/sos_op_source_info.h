//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_SOURCE_INFO_H
#define SOS_OP_SOURCE_INFO_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"

enum SOValueSource_t
{
	SO_SRC_NONE = 0,
	SO_SRC_EMITTER,
	SO_SRC_ENTITY
};

//-----------------------------------------------------------------------------
// simple operator for setting a single position
//-----------------------------------------------------------------------------
struct CSosOperatorSourceInfo_t : CSosOperator_t
{
	SOS_OUTPUT_FLOAT( m_flOutEntityIndex, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutPosition, SO_VEC3 )
	SOS_OUTPUT_FLOAT( m_flOutAngles, SO_VEC3 )
	SOS_OUTPUT_FLOAT( m_flOutRadius, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutVolume, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutLevel, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutPitch, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutSourceCount, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInputSourceIndex, SO_SINGLE )

	SOValueSource_t m_nSource;
	bool m_bGameExtraOrigins;
};

class CSosOperatorSourceInfo : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorSourceInfo )
};

#endif // SOS_OP_SOURCE_INFO_H