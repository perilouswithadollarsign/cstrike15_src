//===== Copyright � 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef IPaintmapMANIPULATOR_H
#define IPaintmapMANIPULATOR_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"

struct Rect_t;

//owned by the client
abstract_class IPaintmapDataManager
{
public:
	//called just before the material system initializes the paint textures
	virtual void BeginPaintmapsDataAllocation( int iPaintmapCount ) = 0;

	virtual void AllocatePaintmapData( int iPaintmapID, int iCorrespondingLightMapWidth, int iCorrespondingLightMapHeight ) = 0;

	virtual void DestroyPaintmapsData( void ) = 0; // clean up old Paintmaps data

	virtual BYTE* GetPaintmapData( int paintmap ) = 0;

	virtual void GetPaintmapSize( int paintmap, int& width, int& height ) = 0;

	virtual void OnRestorePaintmaps() = 0;
};


//owned by materialsystem
abstract_class IPaintmapTextureManager
{
public:
	virtual void BeginUpdatePaintmaps() = 0;
	virtual void UpdatePaintmap( int iPaintmapID, BYTE* pPaintData, int numRects, Rect_t* pRects ) = 0;
	virtual void EndUpdatePaintmaps() = 0;
};


#endif // IPaintmapMANIPULATOR_H
