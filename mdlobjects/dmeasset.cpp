//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ====
//
// Dme Asset
//
//===========================================================================


// Valve includes
#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmeasset.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// DmeRelatedAsset
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeRelatedAsset, CDmeRelatedAsset );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeRelatedAsset::OnConstruction()
{
	m_sPath.Init( this, "path" );
	m_bIncludeModel.Init( this, "includeModel" );
	m_sNotes.Init( this, "notes" );
	m_bUseSkeleton.Init( this, "useSkeleton" );
	m_bAlwaysIncludeAttachments.Init( this, "alwaysIncludeAttachments" );
	m_eAssembleCmds.Init( this, "assembleCmds" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeRelatedAsset::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// DmeAssetRoot
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeAssetRoot, CDmeAssetRoot );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeAssetRoot::OnConstruction()
{
	m_sMdlPath.Init( this, "mdlPath" );
	m_sSurfaceProperty.Init( this, "surfaceProperty" );
	m_ePostAssembleCmds.Init( this, "postAssembleCmds" );
	m_eRelatedAssets.Init( this, "relatedAssets" );
	m_sNameAtCreationTime.Init( this, "nameAtCreationTime" );
	m_sNotes.Init( this, "notes" );
	m_bAmbientBoost.Init( this, "ambientBoost" );
	m_bCastTextureShadows.Init( this, "castTextureShadows" );
	m_bDoNotCastShadows.Init( this, "doNotCastShadows" );
	m_sDynamicLightingOrigin.Init( this, "dynamicLightingOrigin" );
	m_nOpacity.Init( this, "opacity" );
	m_bNoForcedFade.Init( this, "noForcedFace" );
	m_bSubdivisionSurface.Init( this, "subdivisionSurface" );
	m_sContentsDescription.Init( this, "contentsDescription" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeAssetRoot::OnDestruction()
{
}