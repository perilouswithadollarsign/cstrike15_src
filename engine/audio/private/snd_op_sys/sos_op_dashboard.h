//============ Copyright (c) Valve Corporation, All rights reserved. ============
//
//
//
//===============================================================================

#ifndef SOS_OP_DASHBOARD_H
#define SOS_OP_DASHBOARD_H
#ifdef _WIN32
#pragma once
#endif

#include "sos_op.h"


//-----------------------------------------------------------------------------
// simple operator for getting the dashboard value
//-----------------------------------------------------------------------------
struct CSosOperatorDashboard_t : CSosOperator_t
{
	bool m_bMusic;
	SOS_OUTPUT_FLOAT( m_flOutput, SO_SINGLE )
};

class CSosOperatorDashboard : public CSosOperator
{
	SOS_HEADER_DESC( CSosOperatorDashboard )

};

#endif // SOS_OP_DASHBOARD