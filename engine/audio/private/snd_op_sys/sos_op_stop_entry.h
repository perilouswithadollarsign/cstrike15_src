//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_STOP_ENTRY_H
#define SOS_OP_STOP_ENTRY_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"

//-----------------------------------------------------------------------------
// stop other sound entries
//-----------------------------------------------------------------------------
struct CSosOperatorStopEntry_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInputMaxVoices, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInputStopDelay, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputVoicesMatching, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutputIndexOfThis, SO_SINGLE )

	char m_nMatchEntryName[64];
	bool m_bMatchEntry;
	char m_nMatchSoundName[64];
	bool m_bMatchSound;
	bool m_bMatchSubString;
	bool m_bMatchEntity;
	bool m_bMatchChannel;
	bool m_bStopOldest;
	bool m_bStopThis;
	bool m_bMatchThisEntry;
	bool m_bInvertMatch;

};

class CSosOperatorStopEntry : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorStopEntry )
};

#endif // SOS_OP_STOP_ENTRY_H