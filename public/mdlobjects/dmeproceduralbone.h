//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. =====
//
// DmeProceduralBone
//
//============================================================================

#ifndef DMEPROCEDURALBONE_H
#define DMEPROCEDURALBONE_H

#if defined( _WIN32 )
#pragma once
#endif


// Valve includes
#include "movieobjects/dmejoint.h"


//-----------------------------------------------------------------------------
// DmeProceduralBone
//-----------------------------------------------------------------------------
class CDmeProceduralBone : public CDmeJoint
{
	DEFINE_ELEMENT( CDmeProceduralBone, CDmeJoint );

public:
};


#endif // DMEPROCEDURALBONE_H
