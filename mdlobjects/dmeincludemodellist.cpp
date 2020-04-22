//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =====//
//
// Dme version of a body group list
//
//===========================================================================//


#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmeincludemodellist.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeIncludeModelList, CDmeIncludeModelList );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeIncludeModelList::OnConstruction()
{
	m_IncludeModels.Init( this, "includeModels" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeIncludeModelList::OnDestruction()
{
}
