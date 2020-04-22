//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "uigeometry.h"
#include "materialsystem/imaterialsystem.h"
#include "mathlib/vector.h"
#include "gamegraphic.h"
#include "graphicgroup.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CGeometry::CGeometry()
{
	m_Center.x = 0;
	m_Center.y = 0;

	m_Scale.x = 1;
	m_Scale.y = 1;

	m_Rotation = 0;

	m_Color.r = 255;
	m_Color.g = 255;
	m_Color.b = 255;
	m_Color.a = 255;

	m_TopColor.r = 255;
	m_TopColor.g = 255;
	m_TopColor.b = 255;
	m_TopColor.a = 255;

	m_BottomColor.r = 255;
	m_BottomColor.g = 255;
	m_BottomColor.b = 255;
	m_BottomColor.a = 255;

	m_bHorizontalGradient = false;

	m_SheetSequenceNumber = 0;
	m_AnimationRate = 1;

	m_Sublayer = -1;
	m_bMaintainAspectRatio = true;
	m_bVisible = true;

	m_AnimStartTime = DMETIME_ZERO;
	m_bAnimate = false;

	m_bDirtyExtents = false;
}

//-----------------------------------------------------------------------------
// Rendering helper
// Calculate a matrix that will transform the points of the geometry, which are
// currently relative to the center, into screen coords.
//-----------------------------------------------------------------------------
void CGeometry::UpdateRenderTransforms( const StageRenderInfo_t &stageRenderInfo, const CGraphicGroup *pGroup )
{
	Assert( pGroup );
	if ( !pGroup->IsStageGroup() )
	{
		if ( pGroup->GetVisible() == false )
		{
			Assert( m_bVisible == pGroup->GetVisible() );
		}		
	}
	
	if ( !m_bVisible )
		return;


	// Update positions relative to the center, texture coords, and vertex colors
	// If the group maintains aspect ratio it will already have handled this in its transform update.
	Vector2D center;
	bool bUseMaintainedMatrix = m_bMaintainAspectRatio && !pGroup->MaintainAspectRatio();
	if ( bUseMaintainedMatrix )
	{
		// If this is the case we transform the center to screen coords first.
		// Then take into account any size scaling in the scalemat
		// Note this is not accounting for groups or rotations.
		matrix3x4_t screenScalemat;
		SetScaleMatrix( stageRenderInfo.parentScale.x, stageRenderInfo.parentScale.y, 1, screenScalemat );
		Vector centerVec( m_Center.x, m_Center.y, 0 );
		Vector centerInScreen;
		VectorTransform( centerVec, screenScalemat, centerInScreen );

		// Remove the uniform scale component from the center because it will come back in GetRenderTransform()
		float scaleToUse;
		if ( stageRenderInfo.parentScale.x > stageRenderInfo.parentScale.y )
		{
			scaleToUse = stageRenderInfo.parentScale.y;
		}
		else
		{
			scaleToUse = stageRenderInfo.parentScale.x;
		}
		SetScaleMatrix( 1/scaleToUse, 1/scaleToUse, 1, screenScalemat );
		Vector tempCenter;
		VectorTransform( centerInScreen, screenScalemat, tempCenter );
		center.x = tempCenter.x;
		center.y = tempCenter.y;

		/* old version.
		// If this is the case we transform the center to screen coords first.
		// Then take into account any size scaling in the scalemat
		matrix3x4_t screenScalemat;
		SetScaleMatrix( stageRenderInfo.parentScale.x, stageRenderInfo.parentScale.y, 1, screenScalemat );
		Vector centerVec( m_Center.x, m_Center.y, 0 );
		Vector centerInScreen;
		VectorTransform( centerVec, screenScalemat, centerInScreen );
		center.x = centerInScreen.x;
		center.y = centerInScreen.y;
		*/
	}
	else
	{
		center = m_Center;
	}

	matrix3x4_t transmat;
	Vector position( center.x, center.y, 0 );
	SetIdentityMatrix( transmat );
	PositionMatrix( position, transmat );

	matrix3x4_t scalemat;
	SetScaleMatrix( m_Scale.x, m_Scale.y, 1, scalemat );

	matrix3x4_t rotmat;
	Vector axis( 0, 0, 1 );
	MatrixBuildRotationAboutAxis( axis, m_Rotation, rotmat );

	matrix3x4_t temp;
	MatrixMultiply( rotmat, scalemat, temp );
	matrix3x4_t rawToLocal;
	MatrixMultiply( transmat, temp, rawToLocal );

	matrix3x4_t groupToScreen;
	// Use the matrix that doesn't contain any scale changes if we should
	pGroup->GetRenderTransform( groupToScreen, bUseMaintainedMatrix );
	MatrixMultiply( groupToScreen, rawToLocal, m_RenderToScreen );

	if ( m_bDirtyExtents )
	{
		CalculateExtentsMatrix( stageRenderInfo, pGroup );
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGeometry::UpdateRenderData( CUtlVector< RenderGeometryList_t > &renderGeometryLists, int firstListIndex )
{
	if ( !m_bVisible )
		return;

	int i = renderGeometryLists[firstListIndex].AddToTail();
	CRenderGeometry &renderGeometry = renderGeometryLists[firstListIndex][i];

	// Now transform our array of positions into local graphic coord system.
	int nCount = m_RelativePositions.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		// Position
		Vector relativePosition( m_RelativePositions[i].x, m_RelativePositions[i].y, 0 );
		Vector screenpos;
		VectorTransform( relativePosition, m_RenderToScreen, screenpos );
		renderGeometry.m_Positions.AddToTail( Vector2D( screenpos.x, screenpos.y ) );;
		// TexCoord
		renderGeometry.m_TextureCoords.AddToTail( m_TextureCoords[i] );
		// Vertex Color
		renderGeometry.m_VertexColors.AddToTail( m_VertexColors[i] );
	}

	// Triangles
	nCount = m_Triangles.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		renderGeometry.m_Triangles.AddToTail( m_Triangles[i] );	
	}

	// Anim Info														   
	renderGeometry.m_SheetSequenceNumber = m_SheetSequenceNumber;
	renderGeometry.m_AnimationRate = m_AnimationRate;
	renderGeometry.m_bAnimate = m_bAnimate;
	renderGeometry.m_AnimStartTime = m_AnimStartTime;
	renderGeometry.m_pImageAlias = NULL;

	CalculateExtents();
}


//-----------------------------------------------------------------------------
// Calculate the rectangular extents of this graphic.
//-----------------------------------------------------------------------------
void CGeometry::CalculateExtentsMatrix( const StageRenderInfo_t &stageRenderInfo, const CGraphicGroup *pGroup )
{
	Assert( pGroup );

	// Update positions relative to the center, texture coords, and vertex colors
	// If the group maintains aspect ratio it will already have handled this in its transform update.
	Vector2D center;
	bool bUseMaintainedMatrix = m_bMaintainAspectRatio && !pGroup->MaintainAspectRatio();
	if ( bUseMaintainedMatrix )
	{
		// If this is the case we transform the center to screen coords first.
		// Then take into account any size scaling in the scalemat
		matrix3x4_t screenScalemat;
		SetScaleMatrix( stageRenderInfo.parentScale.x, stageRenderInfo.parentScale.y, 1, screenScalemat );
		Vector centerVec( m_Center.x, m_Center.y, 0 );
		Vector centerInScreen;
		VectorTransform( centerVec, screenScalemat, centerInScreen );
		center.x = centerInScreen.x;
		center.y = centerInScreen.y;
	}
	else
	{
		center = m_Center;
	}

	matrix3x4_t transmat;
	Vector position( center.x, center.y, 0 );
	SetIdentityMatrix( transmat );
	PositionMatrix( position, transmat );

	// TODO: account for scaling anims?
	matrix3x4_t scalemat;
	SetScaleMatrix( m_Scale.x, m_Scale.y, 1, scalemat );

	// Rotation is ignored.
	matrix3x4_t rotmat;
	Vector axis( 0, 0, 1 );
	MatrixBuildRotationAboutAxis( axis, 0, rotmat );

	matrix3x4_t temp;
	MatrixMultiply( rotmat, scalemat, temp );
	matrix3x4_t rawToLocal;
	MatrixMultiply( transmat, temp, rawToLocal );

	matrix3x4_t groupToScreen;
	// Use the matrix that doesn't contain any scale changes if we should
	pGroup->GetRenderTransform( groupToScreen, bUseMaintainedMatrix );
	MatrixMultiply( groupToScreen, rawToLocal, m_ExtentsMatrix );	
}

//-----------------------------------------------------------------------------
// Calculate the rectangular extents of this graphic.
//-----------------------------------------------------------------------------
void CGeometry::CalculateExtents()
{
	if ( !m_bDirtyExtents )
		return;

	// Now transform our array of positions into local graphic coord system.
	int nCount = m_RelativePositions.Count();
	CUtlVector< Vector2D > screenPositions;
	for ( int i = 0; i < nCount; ++i )
	{
		// Position
		Vector relativePosition( m_RelativePositions[i].x, m_RelativePositions[i].y, 0 );
		Vector screenpos;
		VectorTransform( relativePosition, m_ExtentsMatrix, screenpos );
		screenPositions.AddToTail( Vector2D( screenpos.x, screenpos.y ) );;
	}


	// left most position is x.
	m_Extents.m_TopLeft.x = INT_MAX;
	for ( int i = 0; i < screenPositions.Count(); ++i )
	{
		if ( m_Extents.m_TopLeft.x > screenPositions[i].x )
			m_Extents.m_TopLeft.x = screenPositions[i].x;
	}

	// top most position is y.
	m_Extents.m_TopLeft.y = INT_MAX;
	for ( int i = 0; i < screenPositions.Count(); ++i )
	{
		if ( m_Extents.m_TopLeft.y > screenPositions[i].y )
			m_Extents.m_TopLeft.y = screenPositions[i].y;
	}


	// right most position is x
	m_Extents.m_BottomRight.x = INT_MIN;
	for ( int i = 0; i < screenPositions.Count(); ++i )
	{
		if ( m_Extents.m_BottomRight.x < screenPositions[i].x )
			m_Extents.m_BottomRight.x = screenPositions[i].x;
	}


	// bottom most position is y
	m_Extents.m_BottomRight.y = INT_MIN;
	for ( int i = 0; i < screenPositions.Count(); ++i )
	{
		if ( m_Extents.m_BottomRight.y < screenPositions[i].y )
			m_Extents.m_BottomRight.y = screenPositions[i].y;
	}

	m_bDirtyExtents = false;
}


//-----------------------------------------------------------------------------
// Return the rectangular bounds of this object
//-----------------------------------------------------------------------------
void CGeometry::GetBounds( Rect_t &bounds )
{
	bounds.x = m_Extents.m_TopLeft.x;
	bounds.y = m_Extents.m_TopLeft.x;
	bounds.width = m_Extents.m_BottomRight.x - m_Extents.m_TopLeft.x;
	bounds.height = m_Extents.m_BottomRight.y - m_Extents.m_TopLeft.y;
}



//-----------------------------------------------------------------------------
// Purpose: Set the vertex colors of the graphic using the base color and the gradient colors.
//-----------------------------------------------------------------------------
void CGeometry::SetResultantColor( color32 parentColor )
{
	SetResultantColor( true, parentColor );
	SetResultantColor( false, parentColor );
}

//-----------------------------------------------------------------------------
// Purpose: Set the vertex colors of the graphic using the base color and the gradient colors.
//-----------------------------------------------------------------------------
void CGeometry::SetResultantColor( bool bTop, color32 parentColor )
{
	if ( bTop )
	{
		color32 localColor;
		localColor.r = (int)( (float)m_TopColor.r * (float)(m_Color.r/255.0) );
		localColor.g = (int)( (float)m_TopColor.g * (float)(m_Color.g/255.0) );
		localColor.b = (int)( (float)m_TopColor.b * (float)(m_Color.b/255.0) );	
		localColor.a = (int)( (float)m_TopColor.a * (float)(m_Color.a/255.0) );

		color32 resultantColor;
		resultantColor.r = (int)( (float)localColor.r * (float)(parentColor.r/255.0) );
		resultantColor.g = (int)( (float)localColor.g * (float)(parentColor.g/255.0) );
		resultantColor.b = (int)( (float)localColor.b * (float)(parentColor.b/255.0) );	
		resultantColor.a = (int)( (float)localColor.a * (float)(parentColor.a/255.0) );

		SetTopVerticesColor( resultantColor );
	}
	else 
	{
		color32 localColor;
		localColor.r = (int)( (float)m_BottomColor.r * (float)(m_Color.r/255.0) );
		localColor.g = (int)( (float)m_BottomColor.g * (float)(m_Color.g/255.0) );
		localColor.b = (int)( (float)m_BottomColor.b * (float)(m_Color.b/255.0) );	
		localColor.a = (int)( (float)m_BottomColor.a * (float)(m_Color.a/255.0) );

		color32 resultantColor;
		resultantColor.r = (int)( (float)localColor.r * (float)(parentColor.r/255.0) );
		resultantColor.g = (int)( (float)localColor.g * (float)(parentColor.g/255.0) );
		resultantColor.b = (int)( (float)localColor.b * (float)(parentColor.b/255.0) );	
		resultantColor.a = (int)( (float)localColor.a * (float)(parentColor.a/255.0) );

		SetBottomVerticesColor( resultantColor );
	}
}

//-----------------------------------------------------------------------------
// Note corner colors will be stomped if the base graphic's color changes.
//-----------------------------------------------------------------------------
void CGeometry::SetTopVerticesColor( color32 c )
{
	if ( m_bHorizontalGradient )
	{
		for ( int i = 0; i < m_VertexColors.Count(); i += 4 )
		{
			m_VertexColors[i] = c;
			m_VertexColors[i+3] = c;	
		}
	}
	else
	{
		for ( int i = 0; i < m_VertexColors.Count(); i += 4 )
		{
			m_VertexColors[i] = c;
			m_VertexColors[i+1] = c;	
		}
	}	
}

//-----------------------------------------------------------------------------
// Note corner colors will be stomped if the base graphic's color changes.
//-----------------------------------------------------------------------------
void CGeometry::SetBottomVerticesColor( color32 c )
{
	if ( m_bHorizontalGradient )
	{
		for ( int i = 0; i < m_VertexColors.Count(); i += 4 )
		{
			m_VertexColors[i+1] = c;
			m_VertexColors[i+2] = c;	
		}
	}
	else
	{
		for ( int i = 0; i < m_VertexColors.Count(); i += 4 )
		{
			m_VertexColors[i + 2] = c;
			m_VertexColors[i + 3] = c;
		}
	}
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGeometry::DrawExtents( CUtlVector< RenderGeometryList_t > &renderGeometryLists,  int firstListIndex, color32 extentLineColor )
{
	if ( !m_bVisible )
		return;

	float lineWidth = 2.0;
	// Time to invent some render data to draw this thing.
	{
		int i = renderGeometryLists[firstListIndex].AddToTail();
		CRenderGeometry &renderGeometry = renderGeometryLists[firstListIndex][i];

		renderGeometry.m_Positions.AddToTail( Vector2D( m_Extents.m_TopLeft.x, m_Extents.m_TopLeft.y ) );
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 0 , 0) );
		renderGeometry.m_VertexColors.AddToTail( extentLineColor );


		renderGeometry.m_Positions.AddToTail( Vector2D( m_Extents.m_BottomRight.x, m_Extents.m_TopLeft.y ) );
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 1 , 0) );
		renderGeometry.m_VertexColors.AddToTail( extentLineColor );

		renderGeometry.m_Positions.AddToTail( Vector2D( m_Extents.m_BottomRight.x, m_Extents.m_TopLeft.y + lineWidth ) );
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 1 , 1) );
		renderGeometry.m_VertexColors.AddToTail( extentLineColor );

		renderGeometry.m_Positions.AddToTail( Vector2D( m_Extents.m_TopLeft.x, m_Extents.m_TopLeft.y + lineWidth ) );
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 0 , 1) );
		renderGeometry.m_VertexColors.AddToTail( extentLineColor );

		// Triangles
		CTriangle tri;
		tri.m_PointIndex[0] = 0;
		tri.m_PointIndex[1] = 1;
		tri.m_PointIndex[2] = 2;
		renderGeometry.m_Triangles.AddToTail( tri );
		tri.m_PointIndex[0] = 0;
		tri.m_PointIndex[1] = 2;
		tri.m_PointIndex[2] = 3;
		renderGeometry.m_Triangles.AddToTail( tri );
		
		// Anim Info														   
		renderGeometry.m_SheetSequenceNumber = 0;
		renderGeometry.m_AnimationRate = m_AnimationRate;
		renderGeometry.m_bAnimate = false;
		renderGeometry.m_AnimStartTime = m_AnimStartTime;
		renderGeometry.m_pImageAlias = NULL;
	}

	
	{
		// Time to invent some render data to draw this thing.
		int i = renderGeometryLists[firstListIndex].AddToTail();
		CRenderGeometry &renderGeometry = renderGeometryLists[firstListIndex][i];

		renderGeometry.m_Positions.AddToTail( Vector2D( m_Extents.m_BottomRight.x - lineWidth, m_Extents.m_TopLeft.y ) );
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 0 , 0) );
		renderGeometry.m_VertexColors.AddToTail( extentLineColor );

		renderGeometry.m_Positions.AddToTail( Vector2D( m_Extents.m_BottomRight.x, m_Extents.m_TopLeft.y ) );
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 1 , 0) );
		renderGeometry.m_VertexColors.AddToTail( extentLineColor );

		renderGeometry.m_Positions.AddToTail( Vector2D( m_Extents.m_BottomRight.x, m_Extents.m_BottomRight.y ) );
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 1 , 1) );
		renderGeometry.m_VertexColors.AddToTail( extentLineColor );

		renderGeometry.m_Positions.AddToTail( Vector2D( m_Extents.m_BottomRight.x - lineWidth, m_Extents.m_BottomRight.y  ) );
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 0 , 1) );
		renderGeometry.m_VertexColors.AddToTail( extentLineColor );

		// Triangles
		CTriangle tri;
		tri.m_PointIndex[0] = 0;
		tri.m_PointIndex[1] = 1;
		tri.m_PointIndex[2] = 2;
		renderGeometry.m_Triangles.AddToTail( tri );
		tri.m_PointIndex[0] = 0;
		tri.m_PointIndex[1] = 2;
		tri.m_PointIndex[2] = 3;
		renderGeometry.m_Triangles.AddToTail( tri );

		// Anim Info														   
		renderGeometry.m_SheetSequenceNumber = 0;
		renderGeometry.m_AnimationRate = m_AnimationRate;
		renderGeometry.m_bAnimate = false;
		renderGeometry.m_AnimStartTime = m_AnimStartTime;
		renderGeometry.m_pImageAlias = NULL;
	}

	{
		int i = renderGeometryLists[firstListIndex].AddToTail();
		CRenderGeometry &renderGeometry = renderGeometryLists[firstListIndex][i];

		renderGeometry.m_Positions.AddToTail( Vector2D( m_Extents.m_TopLeft.x, m_Extents.m_BottomRight.y - lineWidth  ) );
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 0 , 0) );
		renderGeometry.m_VertexColors.AddToTail( extentLineColor );

		renderGeometry.m_Positions.AddToTail( Vector2D( m_Extents.m_BottomRight.x, m_Extents.m_BottomRight.y - lineWidth ) );
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 1 , 0) );
		renderGeometry.m_VertexColors.AddToTail( extentLineColor );


		renderGeometry.m_Positions.AddToTail( Vector2D( m_Extents.m_BottomRight.x, m_Extents.m_BottomRight.y ) );
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 1 , 1) );
		renderGeometry.m_VertexColors.AddToTail( extentLineColor );


		renderGeometry.m_Positions.AddToTail( Vector2D( m_Extents.m_TopLeft.x, m_Extents.m_BottomRight.y ) );
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 0 , 1) );
		renderGeometry.m_VertexColors.AddToTail( extentLineColor );

	 
		// Triangles
		CTriangle tri;
		tri.m_PointIndex[0] = 0;
		tri.m_PointIndex[1] = 1;
		tri.m_PointIndex[2] = 2;
		renderGeometry.m_Triangles.AddToTail( tri );
		tri.m_PointIndex[0] = 0;
		tri.m_PointIndex[1] = 2;
		tri.m_PointIndex[2] = 3;
		renderGeometry.m_Triangles.AddToTail( tri );

		// Anim Info														   
		renderGeometry.m_SheetSequenceNumber = 0;
		renderGeometry.m_AnimationRate = m_AnimationRate;
		renderGeometry.m_bAnimate = false;
		renderGeometry.m_AnimStartTime = m_AnimStartTime;
		renderGeometry.m_pImageAlias = NULL;
	}
	
	{
		// Time to invent some render data to draw this thing.
		int i = renderGeometryLists[firstListIndex].AddToTail();
		CRenderGeometry &renderGeometry = renderGeometryLists[firstListIndex][i];
	
		renderGeometry.m_Positions.AddToTail( Vector2D( m_Extents.m_TopLeft.x, m_Extents.m_TopLeft.y ) );
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 0 , 0) );
		renderGeometry.m_VertexColors.AddToTail( extentLineColor );

		renderGeometry.m_Positions.AddToTail( Vector2D( m_Extents.m_TopLeft.x + lineWidth, m_Extents.m_TopLeft.y ) );
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 1 , 0) );
		renderGeometry.m_VertexColors.AddToTail( extentLineColor );

		renderGeometry.m_Positions.AddToTail( Vector2D( m_Extents.m_TopLeft.x + lineWidth, m_Extents.m_BottomRight.y ) );
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 1 , 1) );
		renderGeometry.m_VertexColors.AddToTail( extentLineColor );

		renderGeometry.m_Positions.AddToTail( Vector2D( m_Extents.m_TopLeft.x, m_Extents.m_BottomRight.y ) );
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 0 , 1) );
		renderGeometry.m_VertexColors.AddToTail( extentLineColor );


		// Triangles
		CTriangle tri;
		tri.m_PointIndex[0] = 0;
		tri.m_PointIndex[1] = 1;
		tri.m_PointIndex[2] = 2;
		renderGeometry.m_Triangles.AddToTail( tri );
		tri.m_PointIndex[0] = 0;
		tri.m_PointIndex[1] = 2;
		tri.m_PointIndex[2] = 3;
		renderGeometry.m_Triangles.AddToTail( tri );

		// Anim Info														   
		renderGeometry.m_SheetSequenceNumber = 0;
		renderGeometry.m_AnimationRate = m_AnimationRate;
		renderGeometry.m_bAnimate = false;
		renderGeometry.m_AnimStartTime = m_AnimStartTime;
		renderGeometry.m_pImageAlias = NULL;
	}
	
}












//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
int CRenderGeometry::GetTriangleCount()
{
	return m_Triangles.Count();
}

int CRenderGeometry::GetVertexCount()
{
	return m_Positions.Count();
}


DmeTime_t CRenderGeometry::GetAnimStartTime()
{
	return m_AnimStartTime;
}



//-----------------------------------------------------------------------------
// Return true if the point x, y lies inside the triangle.
// Triangle points should be supplied in clockwise dir.
//-----------------------------------------------------------------------------
bool PointTriangleHitTest( Vector2D tringleVert0, Vector2D tringleVert1, Vector2D tringleVert2, Vector2D point )
{
	// Compute vectors 
	Vector2D v0 = tringleVert2 - tringleVert0;
	Vector2D v1 = tringleVert1 - tringleVert0;
	Vector2D v2 = point - tringleVert0;

	// Compute dot products
	float dot00 = v0.Dot( v0 );
	float dot01 = v0.Dot( v1 );
	float dot02 = v0.Dot( v2 );
	float dot11 = v1.Dot( v1 );
	float dot12 = v1.Dot( v2 );

	// Compute barycentric coordinates
	float invDenom = 1 / (dot00 * dot11 - dot01 * dot01);
	float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
	float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

	// Check if point is in triangle
	return ( u > 0 ) && ( v > 0 ) && ( u + v < 1 );
}





