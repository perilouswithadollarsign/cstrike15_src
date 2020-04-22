//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =====//
//
// Globals applyign to Eyeballs
//
//===========================================================================//


#ifndef DMEEYEBALLGLOBALS_H
#define DMEEYEBALLGLOBALS_H


#ifdef _WIN32
#pragma once
#endif


// Valve includes
#include "datamodel/dmattributevar.h"
#include "datamodel/dmelement.h"


//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------
class CDmeDag;


//-----------------------------------------------------------------------------
// Globals applying to Eyeballs
//-----------------------------------------------------------------------------
class CDmeEyeballGlobals : public CDmElement
{
	DEFINE_ELEMENT( CDmeEyeballGlobals, CDmElement );

public:
	CDmaVar< Vector > m_vEyePosition;			// The position of the eye
	CDmaVar< float > m_flMaxEyeDeflection;		// The maximum amount of eye deflection
};


#endif // DMEEYEBALLGLOBALS_H
