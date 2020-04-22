//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ====
//
// Elements related to assets
//
//===========================================================================


#ifndef DMEASSET_H
#define DMEASSET_H


#ifdef _WIN32
#pragma once
#endif


// Valve includes
#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDmeRelatedAsset : public CDmElement
{
	DEFINE_ELEMENT( CDmeRelatedAsset, CDmElement );

public:

	CDmaString m_sPath;
	CDmaVar< bool > m_bIncludeModel;
	CDmaString m_sNotes;
	CDmaVar< bool > m_bUseSkeleton;
	CDmaVar< bool > m_bAlwaysIncludeAttachments;
	CDmaElementArray< CDmElement > m_eAssembleCmds;

};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class CDmeAssetRoot : public CDmElement
{
	DEFINE_ELEMENT( CDmeAssetRoot, CDmElement );

public:
	CDmaString m_sMdlPath;
	CDmaString m_sSurfaceProperty;
	CDmaElementArray< CDmElement > m_ePostAssembleCmds;
	CDmaElementArray< CDmElement > m_eRelatedAssets;
	CDmaString m_sNameAtCreationTime;
	CDmaString m_sNotes;
	CDmaVar< bool > m_bAmbientBoost;
	CDmaVar< bool > m_bCastTextureShadows;
	CDmaVar< bool > m_bDoNotCastShadows;
	CDmaString m_sDynamicLightingOrigin;
	CDmaVar< int > m_nOpacity;
	CDmaVar< bool > m_bNoForcedFade;
	CDmaVar< bool > m_bSubdivisionSurface;
	CDmaString m_sContentsDescription;

};


#endif // DMEASSET_H
