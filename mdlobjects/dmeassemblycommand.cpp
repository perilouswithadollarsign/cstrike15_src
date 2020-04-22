//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// DmeAssemblyCommand
//
//=============================================================================


// Valve includes
#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmeassemblycommand.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// DmeJiggleBone
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeAssemblyCommand, CDmeAssemblyCommand );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeAssemblyCommand::OnConstruction()
{
	// No attributes
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeAssemblyCommand::OnDestruction()
{
}
