//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =====//
//
// A list of DmeBoneWeight elements, replacing QC's $WeightList
//
//===========================================================================//


#include "datamodel/dmelementfactoryhelper.h"
#include "mdlobjects/dmeboneweight.h"
#include "mdlobjects/dmebonemask.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeBoneMask, CDmeBoneMask );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBoneMask::OnConstruction()
{
	m_BoneWeights.Init( this, "boneWeightList" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeBoneMask::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
float CDmeBoneMask::GetBoneWeight( const char *pszBoneName )
{
	for ( int i = 0; i < m_BoneWeights.Count(); ++i )
	{
		CDmeBoneWeight *pDmeBoneWeight = m_BoneWeights[i];
		if ( !pDmeBoneWeight )
			continue;

		if ( !V_stricmp( pszBoneName, pDmeBoneWeight->GetName() ) )
			return pDmeBoneWeight->m_flWeight.Get();
	}

	// Couldn't find the bone, return full weight
	return 1.0f;
}