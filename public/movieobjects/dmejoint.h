//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =====//
//
// Dme version of a joint of a skeletal model (gets compiled into a MDL)
//
//===========================================================================//

#ifndef DMEJOINT_H
#define DMEJOINT_H

#ifdef _WIN32
#pragma once
#endif


// Valve includes
#include "movieobjects/dmedag.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/materialsystemutil.h"


// Forward declarations
class CDmeDrawSettings;


//-----------------------------------------------------------------------------
// A class representing a skeletal model
//-----------------------------------------------------------------------------
class CDmeJoint : public CDmeDag
{
	DEFINE_ELEMENT( CDmeJoint, CDmeDag );

public:
	virtual void Draw( CDmeDrawSettings *pDrawSettings = NULL );

	static void DrawJointHierarchy( bool bDrawJoints );

private:
	void DrawJoints();

	static bool sm_bDrawJoints;
	static IMaterial *sm_pMatJoint;
	CMaterialReference m_MatRefJoint;
};


#endif // DMEJOINT_H