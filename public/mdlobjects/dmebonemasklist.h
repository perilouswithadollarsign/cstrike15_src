//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =====//
//
// A list of DmeBoneMask elements, representing multiple QC $WeightList's
//
//===========================================================================//


#ifndef DMEBONEMASKLIST_H
#define DMEBONEMASKLIST_H


#ifdef _WIN32
#pragma once
#endif


#include "datamodel/dmattributevar.h"
#include "mdlobjects/dmemdllist.h"


//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------
class CDmeBoneMask;


//-----------------------------------------------------------------------------
// A class representing a list of bone masks
//-----------------------------------------------------------------------------
class CDmeBoneMaskList : public CDmeMdlList
{
	DEFINE_ELEMENT( CDmeBoneMaskList, CDmeMdlList );

public:
	virtual CDmAttribute *GetListAttr() { return m_BoneMaskList.GetAttribute(); }

	CDmaElement< CDmeBoneMask > m_eDefaultBoneMask;
	CDmaElementArray< CDmeBoneMask > m_BoneMaskList;

};


#endif // DMEBONEMASKLIST_H
