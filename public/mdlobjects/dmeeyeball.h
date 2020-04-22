//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// DmeEyeball
//
//=============================================================================


#ifndef DMEEYEBALL_H
#define DMEEYEBALL_H


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
class CDmeMaterial;


//-----------------------------------------------------------------------------
// DmeEyeball
//-----------------------------------------------------------------------------
class CDmeEyeball : public CDmElement
{
	DEFINE_ELEMENT( CDmeEyeball, CDmElement );

public:
	CDmaVar< float > m_flRadius;					// Radius of the ball of the eye
	CDmaVar< float > m_flYawAngle;					// Yaw offset from "forward" for iris.  Humans are typically 2-4 degrees walleyed.
	CDmaVar< float > m_flIrisScale;					// Scale of the iris texture
	CDmaString m_sMaterialName;						// The name of the material assigned to the faces belonging to the eye
	CDmaString m_sParentBoneName;					// The name of the parent bone for the eyes
	CDmaVar< Vector > m_vPosition;					// The name of the attachment at the position of this eye
};


#endif // DMEEYEBALL_H