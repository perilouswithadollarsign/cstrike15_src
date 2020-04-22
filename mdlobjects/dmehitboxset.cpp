//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =====//
//
// Dme version of a hitbox set
//
//===========================================================================//


#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmehitbox.h"
#include "mdlobjects/dmehitboxset.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeHitboxSet, CDmeHitboxSet );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeHitboxSet::OnConstruction()
{
	m_HitboxList.Init( this, "hitboxList" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeHitboxSet::OnDestruction()
{
}