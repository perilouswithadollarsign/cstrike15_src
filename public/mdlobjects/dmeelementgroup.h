//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ====
//
// Element group
//
//===========================================================================


#ifndef DMEELEMENTGROUP_H
#define DMEELEMENTGROUP_H


#ifdef _WIN32
#pragma once
#endif


// Valve includes
#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDmeElementGroup : public CDmElement
{
	DEFINE_ELEMENT( CDmeElementGroup, CDmElement );

public:

	CDmaElementArray< CDmeElementGroup > m_eElementList;

};


#endif // DMEELEMENTGROUP_H
