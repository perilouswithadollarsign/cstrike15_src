//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =====//
//
// A list of DmeSequences's
//
//===========================================================================//


#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmedefinebone.h"
#include "mdlobjects/dmedefinebonelist.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeDefineBoneList, CDmeDefineBoneList );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDefineBoneList::OnConstruction()
{
	m_DefineBones.Init( this, "defineBones" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeDefineBoneList::OnDestruction()
{
}
