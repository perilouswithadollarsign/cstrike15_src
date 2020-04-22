//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_BLOCK_ENTRY_H
#define SOS_OP_BLOCK_ENTRY_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"

//-----------------------------------------------------------------------------
// stop other sound entries
//-----------------------------------------------------------------------------
struct CSosOperatorBlockEntry_t : CSosOperator_t
{
	SOS_INPUT_FLOAT( m_flInputActive, SO_SINGLE )
	SOS_INPUT_FLOAT( m_flInputDuration, SO_SINGLE )

	char m_nMatchEntryName[64];
	bool m_bMatchEntry;
	char m_nMatchSoundName[64];
	bool m_bMatchSound;
	bool m_bMatchSubString;
	bool m_bMatchEntity;
	bool m_bMatchChannel;

	int m_nEntryIndex;


};

class CSosOperatorBlockEntry : public CSosOperator
{
	virtual void	StackShutdown( void *pVoidMem, CSosOperatorStack *pStack, int nOpIndex );

	SOS_HEADER_DESC( CSosOperatorBlockEntry )
};

#endif // SOS_OP_BLOCK_ENTRY_H
