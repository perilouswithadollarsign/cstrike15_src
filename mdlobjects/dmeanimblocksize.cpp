//===== Copyright (c) 1996-2009, Valve Corporation, All rights reserved. ====
//
// Dme $animblocksize
//
//===========================================================================


#include "mdlobjects/dmeanimblocksize.h"
#include "datamodel/dmelementfactoryhelper.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeAnimBlockSize, CDmeAnimBlockSize );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeAnimBlockSize::OnConstruction()
{
	m_nSize.Init( this, "size" );
	m_bStall.InitAndSet( this, "stall", true );
	m_nStorageType.InitAndSet( this, "storageType", ANIMBLOCKSTORAGETYPE_LOWRES );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeAnimBlockSize::OnDestruction()
{
}