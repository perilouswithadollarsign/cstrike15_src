//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ====
//
// Dme version of QC $poseparameter
//
//===========================================================================


#ifndef DMEPOSEPARAMETER_H
#define DMEPOSEPARAMETER_H


#ifdef _WIN32
#pragma once
#endif


// Valve includes
#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"


//-----------------------------------------------------------------------------
// $QC poseparam
//-----------------------------------------------------------------------------
class CDmePoseParameter : public CDmElement
{
	DEFINE_ELEMENT( CDmePoseParameter, CDmElement );

public:
	CDmaVar< float > m_flMin;
	CDmaVar< float > m_flMax;
	CDmaVar< bool > m_bLoop;
	CDmaVar< float > m_flLoopRange;
	CDmaVar< bool > m_bWrap;
	
};


#endif // DMEPOSEPARAMETER_H
