//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =====//
//
// A list of DmeMaterialGroups's - QC $texturegroup
//
//===========================================================================//


#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmematerialgroup.h"
#include "mdlobjects/dmematerialgrouplist.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeMaterialGroupList, CDmeMaterialGroupList );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMaterialGroupList::OnConstruction()
{
	m_MaterialGroups.Init( this, "materialGroups" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeMaterialGroupList::OnDestruction()
{
}
