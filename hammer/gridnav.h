//========= Copyright © 2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines the regular grid nav as required by DOTA. Builds, renders,
//				and saves out the nav. Traversable edges and cells are determined by
//				picking into the world.
//
//=============================================================================//

#ifndef GRIDNAV_H
#define GRIDNAV_H
#ifdef _WIN32
#pragma once
#endif

class CRender3D;
class CMapDoc;

#include "mathlib/vector.h"
#include "utlvector.h"

class CGridNavCell
{
public:

	inline CGridNavCell();

	bool m_bTraversable;

	int m_nGridPosX;
	int m_nGridPosY;

	float m_flHeight;
};

class CGridNav
{
public:

	CGridNav();

	inline static bool IsEnabled();

	inline bool IsPreviewActive();
	inline void TogglePreview();

	static void Init( bool enabled, float flEdgeSize = 0.0f, float flOffsetX = 0.0f, float flOffsetY = 0.0f, float flTraceHeight = 0.0f );

	void Render( CRender3D *pRender, const Vector &vViewPos, const Vector &vViewDir );

	void Update( CMapDoc *pMapDoc, const Vector &vViewPos, const Vector &vViewDir );

	void GenerateGridNavFile( const char *pFileFullPath );

	inline int CoordToGridPosX( float flCoordX ) const;
	inline int CoordToGridPosY( float flCoordY ) const;

	inline float GridPosXToCoordCenter( int nGridPosX ) const;
	inline float GridPosYToCoordCenter( int nGridPosY ) const;

private:

	static bool sm_bEnabled;
	static float sm_flEdgeSize;
	static float sm_flOffsetX;
	static float sm_flOffsetY;
	static float sm_flTraceHeight;

	Vector m_vLatestCameraPos;
	Vector m_vLatestCameraDir;
	bool m_bNeedsCameraRecompute;
	float m_flTimeCameraLastMoved;
	int m_nTicksCameraStill;
	
	CUtlVector< CGridNavCell > m_CurrentCells;

	bool m_bPreviewActive;
};


////////////////////

bool CGridNav::IsEnabled()
{
	return sm_bEnabled;
}


bool CGridNav::IsPreviewActive()
{
	return m_bPreviewActive;
}


void CGridNav::TogglePreview()
{
	m_bPreviewActive = !m_bPreviewActive;
}


int CGridNav::CoordToGridPosX( float flCoordX ) const
{
	return (int)floor( ( ( flCoordX - sm_flOffsetX ) + ( sm_flEdgeSize * 0.5f ) ) / sm_flEdgeSize );
}


int CGridNav::CoordToGridPosY( float flCoordY ) const
{
	return (int)floor( ( ( flCoordY - sm_flOffsetY ) + ( sm_flEdgeSize * 0.5f ) ) / sm_flEdgeSize );
}


float CGridNav::GridPosXToCoordCenter( int nGridPosX ) const
{
	return ( ( (float)(nGridPosX) ) * sm_flEdgeSize ) + sm_flOffsetX;
}


float CGridNav::GridPosYToCoordCenter( int nGridPosY ) const
{
	return ( ( (float)(nGridPosY) ) * sm_flEdgeSize ) + sm_flOffsetY;
}


CGridNavCell::CGridNavCell()
	: m_bTraversable( false )
	, m_nGridPosX( 0 )
	, m_nGridPosY( 0 )
	, m_flHeight( 0.0f )
{
}

#endif // GRIDNAV_H