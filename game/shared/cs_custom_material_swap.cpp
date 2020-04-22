//========= Copyright © 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose: Provide custom material swapping for weapons (when switch from world to view or vice versa)
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "cs_custom_material_swap.h"
#include "materialsystem/icustommaterial.h"

CCSCustomMaterialSwapManager g_CSCustomMaterialSwapManager;

//
// global custom material swap manager
//  the game uses this to swap custom materials (after the new one is done)
//

CCSCustomMaterialSwapManager::CCSCustomMaterialSwapManager()
{
	m_pPendingSwaps.EnsureCapacity( 4 );
}

CCSCustomMaterialSwapManager::~CCSCustomMaterialSwapManager()
{	
	ClearAllPendingSwaps();
}

// this is called at the end of each frame
bool ProcessCustomMaterialSwapManager()
{
	return g_CSCustomMaterialSwapManager.Process();
}

bool CCSCustomMaterialSwapManager::Init()
{
	g_pMaterialSystem->AddEndFramePriorToNextContextFunc( ::ProcessCustomMaterialSwapManager );
	return true;
}

void CCSCustomMaterialSwapManager::Shutdown()
{
	g_pMaterialSystem->RemoveEndFramePriorToNextContextFunc( ::ProcessCustomMaterialSwapManager );
	ClearAllPendingSwaps();
}

// handles swapping materials that are pending swap and ready
bool CCSCustomMaterialSwapManager::Process()
{
	for ( int i = m_pPendingSwaps.Count() - 1; i >= 0 ; i-- )
	{
		if ( m_pPendingSwaps[ i ].m_pNewCustomMaterial->IsValid() )
		{
			C_BaseAnimating* pOwner = ( m_pPendingSwaps[ i ].m_hOwner ) ?  m_pPendingSwaps[ i ].m_hOwner->GetBaseAnimating() : NULL;
			if ( pOwner )
			{
				pOwner->SetCustomMaterial( m_pPendingSwaps[ i ].m_pNewCustomMaterial,  m_pPendingSwaps[ i ].m_nCustomMaterialIndex );
			}

			m_pPendingSwaps[ i ].m_hOwner = NULL;
			m_pPendingSwaps[ i ].m_nCustomMaterialIndex = -1;
			m_pPendingSwaps[ i ].m_pNewCustomMaterial->Release();
			m_pPendingSwaps[ i ].m_pNewCustomMaterial = NULL;
			if ( m_pPendingSwaps[ i ].m_pOldCustomMaterial )
			{
				m_pPendingSwaps[ i ].m_pOldCustomMaterial->Release();
				m_pPendingSwaps[ i ].m_pOldCustomMaterial = NULL;
			}
			m_pPendingSwaps.Remove( i );
		}
	}

	return false;
}

void CCSCustomMaterialSwapManager::RequestMaterialSwap( EHANDLE hOwner, int nCustomMaterialIndex, ICustomMaterial *pNewCustomMaterialInterface )
{
	int nSwapIndex = m_pPendingSwaps.AddToTail();
	CCSPendingCustomMaterialSwap_t &materialSwap = m_pPendingSwaps[ nSwapIndex ];

	materialSwap.m_hOwner = hOwner;
	materialSwap.m_nCustomMaterialIndex = nCustomMaterialIndex;

	materialSwap.m_pNewCustomMaterial = pNewCustomMaterialInterface;
	materialSwap.m_pNewCustomMaterial->AddRef();

	ICustomMaterial *pOldCustomMaterialInterface = hOwner->GetBaseAnimating()->GetCustomMaterial( nCustomMaterialIndex );
	materialSwap.m_pOldCustomMaterial = pOldCustomMaterialInterface;
	if ( materialSwap.m_pOldCustomMaterial )
	{
		materialSwap.m_pOldCustomMaterial->AddRef();
	}
}

void CCSCustomMaterialSwapManager::ClearPendingSwaps( EHANDLE hOwner, int nCustomMaterialIndex )
{
	for ( int i = m_pPendingSwaps.Count() - 1; i >= 0 ; i-- )
	{
		if ( m_pPendingSwaps[ i ].m_hOwner == hOwner && m_pPendingSwaps[ i ].m_nCustomMaterialIndex == nCustomMaterialIndex )
		{
			m_pPendingSwaps[ i ].m_hOwner = NULL;
			m_pPendingSwaps[ i ].m_nCustomMaterialIndex = -1;
			m_pPendingSwaps[ i ].m_pNewCustomMaterial->Release();
			m_pPendingSwaps[ i ].m_pNewCustomMaterial = NULL;
			if ( m_pPendingSwaps[ i ].m_pOldCustomMaterial )
			{
				m_pPendingSwaps[ i ].m_pOldCustomMaterial->Release();
				m_pPendingSwaps[ i ].m_pOldCustomMaterial = NULL;
			}
			m_pPendingSwaps.Remove( i );
		}
	}
}

void CCSCustomMaterialSwapManager::ClearAllPendingSwaps( void )
{
	for ( int i = 0; i < m_pPendingSwaps.Count(); ++i )
	{
		CCSPendingCustomMaterialSwap_t &materialSwap = m_pPendingSwaps[ i ];
		materialSwap.m_hOwner = NULL;
		materialSwap.m_nCustomMaterialIndex = -1;
		materialSwap.m_pNewCustomMaterial->Release();
		materialSwap.m_pNewCustomMaterial = NULL;
		if ( m_pPendingSwaps[ i ].m_pOldCustomMaterial )
		{
			materialSwap.m_pOldCustomMaterial->Release();
			materialSwap.m_pOldCustomMaterial = NULL;
		}
	}
	m_pPendingSwaps.RemoveAll();
}
