//============ Copyright (c) Valve Corporation, All rights reserved. ==========
//
//=============================================================================

#ifndef DMEEYELID_H
#define DMEEYELID_H


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
class CDmeEyelid : public CDmElement
{
	DEFINE_ELEMENT( CDmeEyelid, CDmElement );

public:
	CDmaVar< bool > m_bUpper;
	CDmaString m_sLowererFlex;
	CDmaVar< float > m_flLowererHeight;
	CDmaString m_sNeutralFlex;
	CDmaVar< float > m_flNeutralHeight;
	CDmaString m_sRaiserFlex;
	CDmaVar< float > m_flRaiserHeight;
	CDmaString m_sRightEyeballName;
	CDmaString m_sLeftEyeballName;
};


#endif // DMEEYELID_H