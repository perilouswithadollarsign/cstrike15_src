//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_MAPNAME_H
#define SOS_OP_MAPNAME_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"


//-----------------------------------------------------------------------------
// mapname
//-----------------------------------------------------------------------------
struct CSosOperatorMapName_t : CSosOperator_t
{

	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )

};
class CSosOperatorMapName : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorMapName )

};

#endif // SOS_OP_MAPNAME