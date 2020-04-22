//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ====
//
// List of DmePoseParamaters
//
//===========================================================================

#ifndef DMEPOSEPARAMETERLIST_H
#define DMEPOSEPARAMETERLIST_H


#ifdef _WIN32
#pragma once
#endif


// Valve includes
#include "datamodel/dmattributevar.h"
#include "mdlobjects/dmemdllist.h"


//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------
class CDmePoseParameter;

				 
//-----------------------------------------------------------------------------
// A class representing a list of LODs
//-----------------------------------------------------------------------------
class CDmePoseParameterList : public CDmElement
{
	DEFINE_ELEMENT( CDmePoseParameterList, CDmElement );

public:
	virtual CDmAttribute *GetListAttr() { return m_ePoseParameterList.GetAttribute(); }

	CDmaElementArray< CDmePoseParameter > m_ePoseParameterList;
};


#endif // DMEPOSEPARAMETERLIST_H
