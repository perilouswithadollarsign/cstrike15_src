//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =====//
//
// Dme version of a list of $includemodel
//
//===========================================================================//

#ifndef DMEINCLUDEMODELLIST_H
#define DMEINCLUDEMODELLIST_H

#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"


//-----------------------------------------------------------------------------
// A class representing a list of $includemodel
//-----------------------------------------------------------------------------
class CDmeIncludeModelList : public CDmElement
{
	DEFINE_ELEMENT( CDmeIncludeModelList, CDmElement );

public:

#ifndef SWIG
	CDmaStringArray m_IncludeModels;
#endif // #ifndef SWIG

};


#endif // DMEINCLUDEMODELLIST_H
