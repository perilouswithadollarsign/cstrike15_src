//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: Provide interface to custom materials for entities that use them
//
//=============================================================================//
#pragma once 

#include "refcount.h"
#include "materialsystem/icustommaterial.h"

class IMaterial;
class CCustomMaterialOwner;
class CCompositeTexture;

// classes that want to have a custom material should derive from this
class CCustomMaterialOwner
{
public:
	virtual ~CCustomMaterialOwner()	{ ClearCustomMaterials( true ); }
	ICustomMaterial *GetCustomMaterial( int nIndex = 0 ) const; // returns NULL if the index is out of range
	virtual void SetCustomMaterial( ICustomMaterial* pCustomMaterial, int nIndex = 0 );	// either replaces and existing material (releasing the old one), or adds one to the vector
	bool IsCustomMaterialValid( int nIndex = 0 ) const;
	int GetCustomMaterialCount() const { return m_ppCustomMaterials.Count(); }
	void ClearCustomMaterials( bool bPurge = false );
	virtual void OnCustomMaterialsUpdated() {}
	virtual void DuplicateCustomMaterialsToOther( CCustomMaterialOwner *pOther ) const;

private:
	// Pointers to custom materials owned by the mat system for this entity. Index
	// in this vector corresponds to the model material index to override with the custom material.
	CUtlVector< ICustomMaterial* > m_ppCustomMaterials;
};

inline ICustomMaterial *CCustomMaterialOwner::GetCustomMaterial( int nIndex ) const
{ 
	return ( m_ppCustomMaterials.Count() > nIndex ) ? m_ppCustomMaterials[ nIndex ] : NULL; 
}

inline void CCustomMaterialOwner::SetCustomMaterial( ICustomMaterial* pCustomMaterial, int nIndex )
{
	while ( m_ppCustomMaterials.Count() <= nIndex )
	{
		m_ppCustomMaterials.AddToTail( NULL );
	}

	pCustomMaterial->AddRef();
	if ( m_ppCustomMaterials[ nIndex ] )
		m_ppCustomMaterials[ nIndex ]->Release();

	m_ppCustomMaterials[ nIndex ] = pCustomMaterial;
}

inline bool CCustomMaterialOwner::IsCustomMaterialValid( int nIndex ) const
{
	return ( m_ppCustomMaterials.Count() > nIndex && m_ppCustomMaterials[ nIndex ] != NULL ) ? m_ppCustomMaterials[ nIndex ]->IsValid() : false;
}

inline void CCustomMaterialOwner::ClearCustomMaterials( bool bPurge )
{
	for ( int i = 0; i < m_ppCustomMaterials.Count(); i++ )
	{
		if ( m_ppCustomMaterials[ i ] != NULL )
		{ 
			m_ppCustomMaterials[ i ]->Release();
			m_ppCustomMaterials[ i ] = NULL;
		}
	}

	if ( bPurge )
	{
		m_ppCustomMaterials.Purge();
	}
	else
	{
		m_ppCustomMaterials.RemoveAll();
	}
}

inline void CCustomMaterialOwner::DuplicateCustomMaterialsToOther( CCustomMaterialOwner *pOther ) const
{
	pOther->ClearCustomMaterials();
	for ( int i = 0; i < CCustomMaterialOwner::m_ppCustomMaterials.Count(); i++ )
	{
		if ( m_ppCustomMaterials[ i ] == NULL )
			continue;
		
		pOther->SetCustomMaterial( m_ppCustomMaterials[ i ], i );
	}
}