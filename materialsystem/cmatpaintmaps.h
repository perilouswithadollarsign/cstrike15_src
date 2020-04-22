//========== Copyright © 2005, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================

#ifndef CMATERIALPaintmapS_H
#define CMATERIALPaintmapS_H

#include "tier1/utlvector.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/ipaintmapdatamanager.h"

#ifndef MATSYS_INTERNAL
#error "This file is private to the implementation of IMaterialSystem/IMaterialSystemInternal"
#endif

#if defined( _WIN32 )
#pragma once
#endif

class CMaterialSystem;

//-----------------------------------------------------------------------------
// Manager of material system Paintmaps
//-----------------------------------------------------------------------------

class CMatPaintmaps : public IPaintmapTextureManager
{
public:
	CMatPaintmaps();

	bool IsEnabled( void );
	void RegisterPaintmapDataManager( IPaintmapDataManager *pDataManager );

	void BeginPaintTextureAllocation( int iPaintmapCount );
	void EndPaintTextureAllocation( void );

	void AllocatePaintmap( int paintmap, int iWidth, int iHeight );

	void ReleasePaintmaps( void );
	void RestorePaintmaps( int nNumLightmaps );

	void CleanupPaintmaps( void );

	ShaderAPITextureHandle_t GetPaintmapPageTextureHandle( int paintmap );

	CMaterialSystem *GetMaterialSystem() const;


	// Derived from IPaintmapTextureManager
	//------------------------------------------------
	virtual void BeginUpdatePaintmaps();
	virtual void UpdatePaintmap( int paintmap, BYTE* pPaintData, int numRects, Rect_t* pRects );
	virtual void EndUpdatePaintmaps();

	//------------------------------------------------
	IPaintmapDataManager *m_pDataManager;

private:
	void InitPaintmapBits( int paintmap );
	bool LockPaintmap( int paintmap );
	void FillRect( int paintmap, Rect_t* RESTRICT pRect, BYTE* RESTRICT pPaintData, Rect_t* RESTRICT pSubRect = NULL ) RESTRICT;
	void AllocatePaintmapTexture( int paintmap, int iWidth, int iHeight );

	CPixelWriter m_PaintmapPixelWriter;
	CUtlVector<ShaderAPITextureHandle_t> m_PaintmapTextureHandles;

	int m_nLockedPaintmap;
	int m_nUpdatingPaintmapsStackDepth;
};

//-----------------------------------------------------------------------------

#endif // CMATERIALPaintmapS_H
