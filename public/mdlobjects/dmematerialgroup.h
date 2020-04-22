//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =====//
//
// DmeMaterialGroup
//
//===========================================================================//


#ifndef DMEMATERIALGROUP_H
#define DMEMATERIALGROUP_H


#ifdef _WIN32
#pragma once
#endif


#include "datamodel/dmattributevar.h"
#include "mdlobjects/dmemdllist.h"


//-----------------------------------------------------------------------------
// Dme MaterialGroup element
//-----------------------------------------------------------------------------
class CDmeMaterialGroup : public CDmeMdlList
{
	DEFINE_ELEMENT( CDmeMaterialGroup, CDmeMdlList );

public:
	virtual CDmAttribute *GetListAttr() { return m_MaterialList.GetAttribute(); }

#ifndef SWIG
	CDmaStringArray m_MaterialList;
#endif // #ifndef SWIG

};

#endif // DMEMATERIALGROUP_H

