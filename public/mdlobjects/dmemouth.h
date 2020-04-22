//============ Copyright (c) Valve Corporation, All rights reserved. ==========
//
//=============================================================================

#ifndef DMEMOUTH_H
#define DMEMOUTH_H


#ifdef _WIN32
#pragma once
#endif


// Valve includes
#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeDag;


//-----------------------------------------------------------------------------
// DmeEyeball
//-----------------------------------------------------------------------------
class CDmeMouth : public CDmElement
{
	DEFINE_ELEMENT( CDmeMouth, CDmElement );

public:
	CDmaVar< int > m_nMouthNumber;
	CDmaString m_sFlexControllerName;
	CDmaString m_sBoneName;
	CDmaVar< Vector > m_vForward;
};


#endif // DMEMOUTH_H