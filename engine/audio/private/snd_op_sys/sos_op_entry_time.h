//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_ENTRY_TIME_H
#define SOS_OP_ENTRY_TIME_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"


//-----------------------------------------------------------------------------
// get elapsed time
//-----------------------------------------------------------------------------
struct CSosOperatorEntryTime_t : CSosOperator_t
{
	SOS_OUTPUT_FLOAT( m_flOutputSoundDuration, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputSoundElapsed, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputEntryElapsed, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputStopElapsed, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputHostElapsed, SO_SINGLE )

	HSOUNDSCRIPTHASH m_nScriptHash;
};

class CSosOperatorEntryTime : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorEntryTime )

};


#endif // SOS_OP_ENTRY_TIME_H
