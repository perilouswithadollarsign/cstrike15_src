//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =====//
//
// A list of DmeHitboxSetList's
//
//===========================================================================//


#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmehitbox.h"
#include "mdlobjects/dmehitboxset.h"
#include "mdlobjects/dmehitboxsetlist.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeHitboxSetList, CDmeHitboxSetList );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeHitboxSetList::OnConstruction()
{
	m_HitboxSetList.Init( this, "hitboxSetList" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeHitboxSetList::OnDestruction()
{
}
