//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_CONVAR_H
#define SOS_OP_CONVAR_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"
#include "tier1/convar.h"

//-----------------------------------------------------------------------------
// simple operator for getting a convar value
//-----------------------------------------------------------------------------
struct CSosOperatorConvar_t : CSosOperator_t
{
	CSosOperatorConvar_t()
		:
	m_ConVarRef( ( IConVar * )NULL )
	{
		// Do nothing...
	}

	char m_nConvar[64];
	ConVarRef m_ConVarRef;
	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
};

class CSosOperatorConvar : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorConvar )

};


//-----------------------------------------------------------------------------
// simple operator for setting a convar value
//-----------------------------------------------------------------------------
struct CSosOperatorSetConvar_t : CSosOperator_t
{
	CSosOperatorSetConvar_t()
		:
	m_ConVarRef( ( IConVar * )NULL )
	{
		// Do nothing...
	}

	char m_nConvar[64];
	ConVarRef m_ConVarRef;
	SOS_INPUT_FLOAT( m_flInput, SO_SINGLE )
};

class CSosOperatorSetConvar : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorSetConvar )

};


#endif // SOS_OP_CONVAR_H