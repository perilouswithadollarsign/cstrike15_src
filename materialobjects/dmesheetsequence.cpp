//===== Copyright © 2005-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: build a sheet data file and a large image out of multiple images
//
//===========================================================================//

#include "materialobjects/dmesheetsequence.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "tier0/dbg.h"



// CDmeSheetImage
//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSheetImage, CDmeSheetImage );

void CDmeSheetImage::OnConstruction()
{
	m_XCoord.Init( this, "xcoord" );
	m_YCoord.Init( this, "ycoord" );	
	m_mapSequences.Init( this, "mapsequences" );
}

void CDmeSheetImage::OnDestruction()
{
}

CDmeSheetSequence *CDmeSheetImage::FindSequence( int index )
{
	if ( index < m_mapSequences.Count() )
	{
		return m_mapSequences[index];
	}
	return NULL;
}



// CDmeSheetSequenceFrame
//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSheetSequenceFrame, CDmeSheetSequenceFrame );
void CDmeSheetSequenceFrame::OnConstruction()
{
	m_pSheetImages.Init( this, "sheetimages" );
	m_fDisplayTime.Init( this, "displaytime" );
}

void CDmeSheetSequenceFrame::OnDestruction()
{
}




// CDmeSheetSequence
//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSheetSequence, CDmeSheetSequence );
void CDmeSheetSequence::OnConstruction()
{
	m_nSequenceNumber.Init( this, "sequencenumber" );	 
	m_Clamp.Init( this, "clamp" );
	m_eMode.Init( this, "mode" );
	m_Frames.Init( this, "frames" );

	m_Clamp = true;
	m_eMode = SQM_RGBA;
}

void CDmeSheetSequence::OnDestruction()
{
}
