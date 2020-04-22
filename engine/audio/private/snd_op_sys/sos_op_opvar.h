//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_OPVAR_H
#define SOS_OP_OPVAR_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"

//-----------------------------------------------------------------------------
// set opvar
//-----------------------------------------------------------------------------
struct CSosOperatorSetOpvarFloat_t : CSosOperator_t
{

	SOS_INPUT_FLOAT( m_flInput, SO_SINGLE )
	char m_nOpVarName[128];

};
class CSosOperatorSetOpvarFloat : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorSetOpvarFloat )
};

//-----------------------------------------------------------------------------
// get opvar
//-----------------------------------------------------------------------------
struct CSosOperatorGetOpvarFloat_t : CSosOperator_t
{

	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOpVarExists, SO_SINGLE )
	char m_nOpVarName[128];

};
class CSosOperatorGetOpvarFloat : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorGetOpvarFloat )

};

//-----------------------------------------------------------------------------
// increment opvar
//-----------------------------------------------------------------------------
struct CSosOperatorIncrementOpvarFloat_t : CSosOperator_t
{
	
	SOS_INPUT_FLOAT( m_flInput, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
	SOS_OUTPUT_FLOAT( m_flOpVarExists, SO_SINGLE )
	char m_nOpVarName[128];

};
class CSosOperatorIncrementOpvarFloat : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorIncrementOpvarFloat )

};

#endif // SOS_OP_OPVAR_H