//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_MIXER_H
#define SOS_OP_MIXER_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"


//-----------------------------------------------------------------------------
// basic mixgroup operator
//-----------------------------------------------------------------------------
struct CSosOperatorMixGroup_t : CSosOperator_t
{
	int	m_nMixGroupIndex;
	bool m_bSetMixGroupOnChannel;
	SOS_OUTPUT_FLOAT( m_flOutputVolume, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputLevel, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputDSP, SO_SINGLE )

};

class CSosOperatorMixGroup : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorMixGroup )
};

#endif // SOS_OP_MIXER