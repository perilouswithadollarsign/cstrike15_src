//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =====//
//
// A list of DmeSequences's
//
//===========================================================================//


#ifndef DMEMATERIALGROUPSLIST_H
#define DMEMATERIALGROUPSLIST_H


#ifdef _WIN32
#pragma once
#endif


#include "datamodel/dmattributevar.h"
#include "mdlobjects/dmemdllist.h"


//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------
class CDmeMaterialGroup;


//-----------------------------------------------------------------------------
// A class representing a list of sequences
//-----------------------------------------------------------------------------
class CDmeMaterialGroupList : public CDmeMdlList
{
	DEFINE_ELEMENT( CDmeMaterialGroupList, CDmeMdlList );

public:
	virtual CDmAttribute *GetListAttr() { return m_MaterialGroups.GetAttribute(); }

	CDmaElementArray< CDmeMaterialGroup > m_MaterialGroups;

};


#endif // DMEDEFINEBONELIST_H
