//============ Copyright (c) Valve Corporation, All rights reserved. ==========
//
//=============================================================================



// Valve includes
#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmeeyelid.h"
#include "movieobjects/dmedag.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeEyelid, CDmeEyelid );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeEyelid::OnConstruction()
{
	m_bUpper.Init( this, "upper" );
	m_sLowererFlex.Init( this, "lowererFlex" );
	m_flLowererHeight.Init( this, "lowererHeight" );
	m_sNeutralFlex.Init( this, "neutralFlex" );
	m_flNeutralHeight.Init( this, "neutralHeight" );
	m_sRaiserFlex.Init( this, "raiserFlex" );
	m_flRaiserHeight.Init( this, "raiserHeight" );
	m_sRightEyeballName.Init( this, "rightEyeballName" );
	m_sLeftEyeballName.Init( this, "leftEyeballName" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeEyelid::OnDestruction()
{
}
