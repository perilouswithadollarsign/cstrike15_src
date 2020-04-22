//========= Copyright © 1996-2012, Valve Corporation, All rights reserved. ============//
//
// Purpose: Provide custom material swapping for weapons (when switch from world to view or vice versa)
//
// $NoKeywords: $
//=============================================================================//

#ifndef CS_CUSTOM_MATERIAL_SWAP_H
#define CS_CUSTOM_MATERIAL_SWAP_H

class ICustomMaterial;

struct CCSPendingCustomMaterialSwap_t
{
	ICustomMaterial *m_pNewCustomMaterial;
	ICustomMaterial *m_pOldCustomMaterial;
	int m_nCustomMaterialIndex;
	EHANDLE m_hOwner;
};

class CCSCustomMaterialSwapManager : public CAutoGameSystem
{
public:
	CCSCustomMaterialSwapManager();
	virtual ~CCSCustomMaterialSwapManager();

	virtual bool Init();
	virtual void Shutdown();

	bool Process();

	void RequestMaterialSwap( EHANDLE hOwner, int nCustomMaterialIndex, ICustomMaterial *pNewCustomMaterialInterface );
	void ClearPendingSwaps( EHANDLE hOwner, int nCustomMaterialIndex );

private:
	void ClearAllPendingSwaps();

	CUtlVector< CCSPendingCustomMaterialSwap_t > m_pPendingSwaps;
};

extern CCSCustomMaterialSwapManager g_CSCustomMaterialSwapManager;

#endif // CS_CUSTOM_MATERIAL_SWAP_H
