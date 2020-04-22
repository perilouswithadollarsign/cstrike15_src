//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_MIXLAYER_H
#define SOS_OP_MIXLAYER_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"
#include "snd_mixgroups.h"


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
struct CSosOperatorMixLayer_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInput, SO_SINGLE )
	MXRMixGroupFields_t m_nFieldType;
	int m_nMixLayerIndex;
	int m_nMixGroupIndex;
};

class CSosOperatorMixLayer : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorMixLayer )
};


#endif // SOS_OP_MIXLAYER_H
