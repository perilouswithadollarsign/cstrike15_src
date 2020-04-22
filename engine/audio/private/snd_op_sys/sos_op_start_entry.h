//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_START_ENTRY_H
#define SOS_OP_START_ENTRY_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"



//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
struct CSosOperatorStartEntry_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInputStart, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInputStartDelay, SO_SINGLE )
	HSOUNDSCRIPTHASH m_nScriptHash;
	int m_nHasStarted;
	bool m_bTriggerOnce;
};

class CSosOperatorStartEntry : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorStartEntry )
};


#endif // SOS_OP_START_ENTRY_H
