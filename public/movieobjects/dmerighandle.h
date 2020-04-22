//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef DMERIGHANDLE_H
#define DMERIGHANDLE_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmeoperator.h"

class CDmeRigHandle : public CDmeDag
{
	DEFINE_ELEMENT( CDmeRigHandle, CDmeDag );

public:

};

#endif // DMERIGHANDLE_H
