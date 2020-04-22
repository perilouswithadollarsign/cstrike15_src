//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =====//
//
// A list of DmeBoneWeight elements, replacing QC's $WeightList
//
//===========================================================================//


#ifndef DMEBONEMASK_H
#define DMEBONEMASK_H


#ifdef _WIN32
#pragma once
#endif


#include "datamodel/dmattributevar.h"
#include "mdlobjects/dmemdllist.h"


//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------
class CDmeBoneWeight;


//-----------------------------------------------------------------------------
// A class representing a list of bone weights
//-----------------------------------------------------------------------------
class CDmeBoneMask : public CDmeMdlList
{
	DEFINE_ELEMENT( CDmeBoneMask, CDmeMdlList );

public:
	virtual CDmAttribute *GetListAttr() { return m_BoneWeights.GetAttribute(); }

	// Does a case-insensitive search for the specified bone name and returns the weight if it exists or 1.0f if it doesn't
	float GetBoneWeight( const char *pszBoneName );

	CDmaElementArray< CDmeBoneWeight > m_BoneWeights;

};


#endif // DMEBONEMASK_H