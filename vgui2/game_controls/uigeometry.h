//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef UIGEOMETRY_H
#define UIGEOMETRY_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"
#include "tier1/timeutils.h"
#include "materialsystem/materialsystemutil.h"
#include "bitmap/psheet.h"
#include "resourcesystem/stronghandle.h"
#include "rendersystem/irenderdevice.h"

struct StageRenderInfo_t;
class CGraphicGroup;

//-----------------------------------------------------------------------------
// Geometry makes an array of triangles by indexing into the point data
//-----------------------------------------------------------------------------
class CTriangle
{
public:
	int m_PointIndex[3];
};


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class CRenderGeometry
{
public:
	int GetTriangleCount();
	int GetVertexCount();
	DmeTime_t GetAnimStartTime();

	CUtlVector< Vector2D > m_Positions;
	CUtlVector< Vector2D > m_TextureCoords;
	CUtlVector< color32 > m_VertexColors;
	CUtlVector< CTriangle > m_Triangles;

	union 
	{
		int m_SheetSequenceNumber;
		int m_FontTextureID;
	};
	float m_AnimationRate;
	DmeTime_t m_AnimStartTime;
	bool m_bAnimate;

	const char *m_pImageAlias;
};

typedef CUtlVector< CRenderGeometry > RenderGeometryList_t;

struct LayerRenderLists_t
{
	CUtlVector< RenderGeometryList_t > m_RenderGeometryLists;
	int m_LayerType;
	IMaterial *m_pMaterial;
	CSheet *m_pSheet;
	HRenderTextureStrong m_hTexture;	
};

struct RectExtents_t
{
	Vector2D m_TopLeft;
	Vector2D m_BottomRight;
};

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class CGeometry
{
public:
	CGeometry();

	int GetTriangleCount()
	{
		return m_Triangles.Count();
	}

	void SetResultantColor( color32 parentColor );
	void UpdateRenderTransforms( const StageRenderInfo_t &stageRenderInfo, const CGraphicGroup *pGroup );
	void UpdateRenderData( CUtlVector< RenderGeometryList_t > &renderGeometryLists,  int firstListIndex );

	void GetBounds( Rect_t &bounds );

	CUtlVector< Vector2D > m_RelativePositions;
	CUtlVector< Vector2D > m_TextureCoords;
	CUtlVector< color32 > m_VertexColors;
	CUtlVector< CTriangle > m_Triangles;
	Vector2D m_Center;
	Vector2D m_Scale;
	float m_Rotation;
	color32 m_Color;
	color32 m_TopColor;
	color32 m_BottomColor;
	bool m_bHorizontalGradient;

	int m_SheetSequenceNumber;
	float m_AnimationRate;
	
	int m_Sublayer;
	bool m_bMaintainAspectRatio;
	bool m_bVisible;


	DmeTime_t m_AnimStartTime;

	bool m_bAnimate;


	matrix3x4_t m_RenderToScreen;
	matrix3x4_t m_RenderToScreenHoldAspectRatio;

	bool m_bDirtyExtents;  // if true, recalculate extents on the next render.
	RectExtents_t m_Extents;

	void CalculateExtents();
	void DrawExtents( CUtlVector< RenderGeometryList_t > &renderGeometryLists,  int firstListIndex, color32 extentLineColor );


private:
	void CalculateExtentsMatrix( const StageRenderInfo_t &stageRenderInfo, const CGraphicGroup *pGroup );


	// For applying color gradients
	void SetResultantColor( bool bTop, color32 parentColor );
	void SetTopVerticesColor( color32 c );
	void SetBottomVerticesColor( color32 c );


	// Use this matrix to calcuate extents positions on screen.
	matrix3x4_t m_ExtentsMatrix;
	
};


extern bool PointTriangleHitTest( Vector2D tringleVert0, Vector2D tringleVert1, Vector2D tringleVert2, Vector2D point );





#endif // UIGEOMETRY_H
