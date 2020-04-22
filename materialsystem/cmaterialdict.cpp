//========== Copyright © 2005, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#include "pch_materialsystem.h"

#ifndef _PS3
#define MATSYS_INTERNAL
#endif

#include "cmaterialdict.h"

#include "materialsystem_global.h"
#include "filesystem.h"
#include "imaterialinternal.h"

// NOTE: This must be the last file included!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// sort function
//-----------------------------------------------------------------------------
bool CMaterialDict::MaterialLessFunc( const MaterialLookup_t& src1, 
										   const MaterialLookup_t& src2 )
{
	// Always sort manually-created materials to the front
	if ( src1.m_bManuallyCreated != src2.m_bManuallyCreated )
		return src1.m_bManuallyCreated;

	return src1.m_Name < src2.m_Name;
}

//-----------------------------------------------------------------------------
// sort function for missing materials
//-----------------------------------------------------------------------------
bool CMaterialDict::MissingMaterialLessFunc( const MissingMaterial_t& src1, 
											const MissingMaterial_t& src2 )
{
	return src1.m_Name < src2.m_Name;
}

void CMaterialDict::Shutdown( )
{
	// Clean up all materials..
	RemoveAllMaterials();

	// FIXME: Could dump list here...
	m_MissingList.RemoveAll();

}

//-----------------------------------------------------------------------------
// Adds/removes the material to the list of all materials
//-----------------------------------------------------------------------------
void CMaterialDict::AddMaterialToMaterialList( IMaterialInternal *pMaterial )
{
	MaterialLookup_t lookup;
	lookup.m_pMaterial = pMaterial;
	lookup.m_Name = pMaterial->GetName();
	lookup.m_bManuallyCreated = pMaterial->IsManuallyCreated();
	m_MaterialDict.Insert( lookup );
}

void CMaterialDict::RemoveMaterialFromMaterialList( IMaterialInternal *pMaterial )
{
	// Gotta iterate over this manually; name-based lookup is bogus if there are two
	// materials with the same name, which can happen for procedural materials
	// First remove all the subrect materials, because they'll point at their material pages.
	MaterialHandle_t i;
	MaterialHandle_t iNext = InvalidMaterial();
	for (i = FirstMaterial(); i != InvalidMaterial(); i = iNext )
	{
		iNext = NextMaterial(i);
		if ( m_MaterialDict[i].m_pMaterial == pMaterial )
		{
			m_MaterialDict.RemoveAt( i );
			break;
		}
	}

	Assert( i != InvalidMaterial() );

#ifdef _DEBUG
	for ( i = iNext; i != InvalidMaterial(); i = NextMaterial(i) )
	{
		Assert( m_MaterialDict[i].m_pMaterial != pMaterial );
	}
#endif
}


//-----------------------------------------------------------------------------
// Adds, removes materials
//-----------------------------------------------------------------------------
IMaterialInternal* CMaterialDict::AddMaterial( char const* pName, const char *pTextureGroupName )
{
	IMaterialInternal *pMaterial = IMaterialInternal::CreateMaterial( pName, pTextureGroupName );
	Assert( pMaterial && pMaterial->IsRealTimeVersion() );
	AddMaterialToMaterialList( pMaterial );
	return pMaterial;
}

void CMaterialDict::RemoveMaterial( IMaterialInternal* pMaterial )
{
	Assert( (pMaterial == NULL) || pMaterial->IsRealTimeVersion() );
	RemoveMaterialFromMaterialList( pMaterial );
	IMaterialInternal::DestroyMaterial( pMaterial );
}

IMaterialInternal *CMaterialDict::AddMaterialSubRect( const char *pName, const char *pTextureGroupName, KeyValues *pKeyValues, KeyValues *pPatchKeyValues )
{
	IMaterialInternal *pMaterial = IMaterialInternal::CreateMaterialSubRect( pName, pTextureGroupName, pKeyValues, pPatchKeyValues, true );
	Assert( pMaterial );
	AddMaterialToMaterialList( pMaterial );
	return pMaterial;
}

void CMaterialDict::RemoveMaterialSubRect( IMaterialInternal *pMaterial )
{
	RemoveMaterialFromMaterialList( pMaterial );
	IMaterialInternal::DestroyMaterialSubRect( pMaterial );
}

void CMaterialDict::RemoveMaterialFromMaterialList( MaterialHandle_t h )
{
	m_MaterialDict.RemoveAt( h );
}

void CMaterialDict::RemoveAllMaterialsFromMaterialList()
{
	m_MaterialDict.RemoveAll();
}

void CMaterialDict::RemoveAllMaterials()
{
	// First remove all the subrect materials, because they'll point at their material pages.
	MaterialHandle_t i, iNext;
	for (i = FirstMaterial(); i != InvalidMaterial(); i = iNext )
	{
		iNext = NextMaterial(i);
		IMaterialInternal *pMaterial = GetMaterialInternal( i );
		if ( pMaterial->InMaterialPage() )
		{
			IMaterialInternal::DestroyMaterialSubRect( pMaterial );
			RemoveMaterialFromMaterialList( i );
		}
	}

	// Now get rid of the rest of the materials, including the pages.
	for (i = FirstMaterial(); i != InvalidMaterial(); i = NextMaterial(i) )
	{
		IMaterialInternal::DestroyMaterial( GetMaterialInternal( i ) );
	}

	RemoveAllMaterialsFromMaterialList();
}

