//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// DmeEyeball
//
//=============================================================================


// Valve includes
#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmeeyeball.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmematerial.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeEyeball, CDmeEyeball );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeEyeball::OnConstruction()
{
	m_flRadius.InitAndSet( this, "radius", 0.5 );
	m_flYawAngle.InitAndSet( this, "angle", 2.0 );
	m_flIrisScale.InitAndSet( this, "irisScale", 1.0 );
	m_sMaterialName.Init( this, "materialName" );
	m_sParentBoneName.Init( this, "parentBoneName" );
	m_vPosition.Init( this, "position" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeEyeball::OnDestruction()
{
}