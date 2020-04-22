//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//==========================================================================//
#ifndef PAINT_H
#define PAINT_H

#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/ipaintmapdatamanager.h"
#include "bitvec.h"
#include "surfacehandle.h"

//forward declaration
struct model_t;

struct PaintRect_t
{
	Rect_t rect;
	Vector2D uvCenter;
	float flCenterAlpha;
	float flCircleRadius;
	float flPaintCoatPercent;
	BYTE colorIndex;
	SurfaceHandle_t surfID;
};

enum PaintDirtyFlags_t
{
	PAINTMAP_CLEAN = 0,
	PAINTMAP_DIRTY_SUBRECT,
	PAINTMAP_DIRTY_FULLRECT
};

class CPaintTextureData
{
public:
	CPaintTextureData();

	// Initializes, shuts down the material
	bool Init( int width, int height, int lightmapPageID );
	void Destroy();

	// Returns the texcoord range
	void GetTexCoordRange( float *pMaxU, float *pMaxV );

	// Returns the size of the paint texture (stored in a subrect of the material itself)
	void GetPaintSize( int *pWidth, int *pHeight );

	void ClearTexture();
	void GetPixels( const Rect_t& splatRect, CUtlVector<BYTE>& surfColors );
	BYTE GetPixel( int x, int y ) const;
	void SetPixel( int x, int y, BYTE color );

	// Returns true if any data changed
	bool Paint( const PaintRect_t& paintRect2 );
	
	void PaintAllSurfaces( BYTE color );

	void GetSurfacePaintData( SurfaceHandle_t surfID, CUtlVector< BYTE > &data ) const;
	void SetSurfacePaintData( SurfaceHandle_t surfID, const CUtlVector< BYTE > &data );

	BYTE* GetPaintmapData() { return m_backbuffer; }

	PaintDirtyFlags_t GetDirtyFlag() const;
	void MarkAsDirty( PaintDirtyFlags_t nDirtyFlag = PAINTMAP_DIRTY_FULLRECT );
	void RemoveDirty();
	void AddDirtyRect( const Rect_t& rect );
	CUtlVectorFixedGrowable<Rect_t, 1024>* GetDirtyRectList();
private:
	enum PaintChangeFlags_t
	{
		TEXEL_CHANGED = 0x1,
		PAINT_POWER_CHANGED = 0x2,
	};

	void ClearBuffer( BYTE *pByte = NULL );

	uint32 BlendLuxel( const PaintRect_t& paintRect, int x, int y, float flNewAlpha, float flMaxAlpha = 1.f );
	uint32 AddSurroundingAlpha( const PaintRect_t& paintRect, int x, int y );

	uint32 DrawLine( const PaintRect_t& paintRect, int x1, int x2, int y );
	uint32 Draw2Lines( const PaintRect_t& paintRect, float x, float y );
	uint32 Draw4Lines( const PaintRect_t& paintRect, float x, float y );
	uint32 DrawCircle( const PaintRect_t& paintRect );

	int m_nPaintWidth;
	int m_nPaintHeight;
	int m_lightmapPageID;

	BYTE *m_backbuffer;

	PaintDirtyFlags_t m_nDirtyFlag;

	CUtlVectorFixedGrowable<Rect_t, 1024> m_dirtyRects; // preallocate 1024, seems to be a conservative count
};

class CPaintmapDataManager : public IPaintmapDataManager
{
public:
	CPaintmapDataManager( void );
	~CPaintmapDataManager( void );

	void RemoveAllPaint( void );
	void RemovePaint( const model_t *pModel );
	void PaintAllSurfaces( BYTE color );

	virtual void BeginPaintmapsDataAllocation( int iPaintmapCount );

	virtual void AllocatePaintmapData( int iPaintmapID, int iCorrespondingLightMapWidth, int iCorrespondingLightMapHeight );

	virtual void DestroyPaintmapsData( void );

	virtual BYTE* GetPaintmapData( int paintmap );
	
	virtual void GetPaintmapSize( int paintmap, int& width, int& height );

	virtual void OnRestorePaintmaps();

	void UpdatePaintmapTextures();

	void GetPaintmapDataRLE( CUtlVector< uint32 > &data ) const;
	void LoadPaintmapDataRLE( const CUtlVector< uint32 > &data );

	CPaintTextureData *m_pPaintTextureDataArray;
	int m_iPaintmaps;
	bool m_bShouldRegister;
};

//global paint atlas
extern CPaintmapDataManager g_PaintManager;

// Returns true if any paint changed
bool ShootPaintSphere( const model_t *pModel, const Vector& vPosition, BYTE colorIndex, float flSphereRadius, float flPaintCoatPercent );
void TracePaintSphere( const model_t *pModel, const Vector& vPosition, const Vector& vContactNormal, float flSphereRadius, CUtlVector<BYTE>& surfColors );

void R_RedownloadAllPaintmaps();

#endif // PAINT_H
