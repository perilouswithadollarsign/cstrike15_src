//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ====
//
// List of DmePoseParamaters
//
//===========================================================================//


// Valve includes
#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmeposeparameter.h"
#include "mdlobjects/dmeposeparameterlist.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmePoseParameterList, CDmePoseParameterList );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmePoseParameterList::OnConstruction()
{
	m_ePoseParameterList.Init( this, "poseParameterList" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmePoseParameterList::OnDestruction()
{
}