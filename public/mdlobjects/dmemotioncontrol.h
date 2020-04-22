//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ====
//
// DmeMotionControl
//
// Used for specifiying motion axis control things like in QC
//
//===========================================================================


#ifndef DMEMOTIONCONTROL_H
#define DMEMOTIONCONTROL_H


#ifdef _WIN32
#pragma once
#endif


#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"


//-----------------------------------------------------------------------------
// CDmeMotionControl
//-----------------------------------------------------------------------------
class CDmeMotionControl : public CDmElement
{
	DEFINE_ELEMENT( CDmeMotionControl, CDmElement );

public:

	// Sets the motion control booleans based on logical OR'd STUDIO_? flags in passed int
	void SetStudioMotionControl( int nStudioMotionControl );

	// Gets the motion control booleans as a logical OR'd STUDIO_? flags int
	int GetStudioMotionControl() const;

	CDmaVar< bool > m_bX;
	CDmaVar< bool > m_bY;
	CDmaVar< bool > m_bZ;
	CDmaVar< bool > m_bXR;
	CDmaVar< bool > m_bYR;
	CDmaVar< bool > m_bZR;
	CDmaVar< bool > m_bLX;
	CDmaVar< bool > m_bLY;
	CDmaVar< bool > m_bLZ;
	CDmaVar< bool > m_bLXR;
	CDmaVar< bool > m_bLYR;
	CDmaVar< bool > m_bLZR;
	CDmaVar< bool > m_bLM;
};


#endif // DMEMOTIONCONTROL_H
