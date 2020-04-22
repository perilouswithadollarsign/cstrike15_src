//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "gamerect.h"
// To handle scaling
#include "materialsystem/imaterialsystem.h"
#include "animdata.h"
#include "Color.h"
#include "gameuisystemmgr.h"



// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DMXELEMENT_UNPACK ( CGameRect )
	DMXELEMENT_UNPACK_FIELD_UTLSTRING( "name", "", m_pName ) 
	DMXELEMENT_UNPACK_FIELD( "center", "0 0", Vector2D, m_Geometry.m_Center ) 
	DMXELEMENT_UNPACK_FIELD( "scale", "0 0", Vector2D, m_Geometry.m_Scale ) 
	DMXELEMENT_UNPACK_FIELD( "rotation", "0", float, m_Geometry.m_Rotation )
	DMXELEMENT_UNPACK_FIELD( "maintainaspectratio", "0", bool, m_Geometry.m_bMaintainAspectRatio )
	DMXELEMENT_UNPACK_FIELD( "sublayertype", "0", int, m_Geometry.m_Sublayer )
	DMXELEMENT_UNPACK_FIELD( "visible", "1", bool, m_Geometry.m_bVisible )
	DMXELEMENT_UNPACK_FIELD( "initialstate", "-1", int, m_CurrentState )
	DMXELEMENT_UNPACK_FIELD( "horizgradient", "0", bool, m_Geometry.m_bHorizontalGradient )
	DMXELEMENT_UNPACK_FIELD( "color", "255 255 255 255", Color, m_Geometry.m_Color )
	DMXELEMENT_UNPACK_FIELD( "topcolor", "255 255 255 255", Color, m_Geometry.m_TopColor )
	DMXELEMENT_UNPACK_FIELD( "bottomcolor", "255 255 255 255", Color, m_Geometry.m_BottomColor )


	// color is gotten from log.
	// sheet seq number is gotten from log.
	
END_DMXELEMENT_UNPACK( CGameRect, s_GameRectUnpack )

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CGameRect::CGameRect( const char *pName ) 
{
	m_Geometry.m_SheetSequenceNumber = 0; // FIXME, not updating seq numbers yet.
	m_bCanAcceptInput = false;

	// DME default values.
	m_pName = pName;
	m_Geometry.m_Center.x = 0;
	m_Geometry.m_Center.y = 0;
	m_Geometry.m_Scale.x = 0;
	m_Geometry.m_Scale.y = 0;
	m_Geometry.m_Rotation = 0;
	m_Geometry.m_bMaintainAspectRatio = 0;
	m_Geometry.m_Sublayer = 0;
	m_Geometry.m_bVisible = true;
	m_CurrentState = -1;
	m_Geometry.m_bHorizontalGradient = false;
	m_Geometry.m_Color.r = 255;
	m_Geometry.m_Color.g = 255;
	m_Geometry.m_Color.b = 255;
	m_Geometry.m_Color.a = 255;
	m_Geometry.m_TopColor.r = 255;
	m_Geometry.m_TopColor.g = 255;
	m_Geometry.m_TopColor.b = 255;
	m_Geometry.m_TopColor.a = 255;
	m_Geometry.m_BottomColor.r = 255;
	m_Geometry.m_BottomColor.g = 255;
	m_Geometry.m_BottomColor.b = 255;
	m_Geometry.m_BottomColor.a = 255;

	m_Geometry.m_RelativePositions.AddToTail( Vector2D( -.5, -.5 ) );
	m_Geometry.m_RelativePositions.AddToTail( Vector2D( .5, -.5 ) );
	m_Geometry.m_RelativePositions.AddToTail( Vector2D( .5, .5 ) );
	m_Geometry.m_RelativePositions.AddToTail( Vector2D( -.5, .5 ) );

	m_Geometry.m_TextureCoords.AddToTail( Vector2D( 0.0, 0.0 ) );
	m_Geometry.m_TextureCoords.AddToTail( Vector2D( 1.0, 0.0 ) );
	m_Geometry.m_TextureCoords.AddToTail( Vector2D( 1.0, 1.0 ) );
	m_Geometry.m_TextureCoords.AddToTail( Vector2D( 0.0, 1.0 ) );

	SetupVertexColors();

	CTriangle triangle;
	triangle.m_PointIndex[0] = 0;
	triangle.m_PointIndex[1] = 1;
	triangle.m_PointIndex[2] = 2;
	m_Geometry.m_Triangles.AddToTail( triangle );
	triangle.m_PointIndex[0] = 0;
	triangle.m_PointIndex[1] = 2;
	triangle.m_PointIndex[2] = 3;
	m_Geometry.m_Triangles.AddToTail( triangle );
}


CGameRect::~CGameRect() 
{
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CGameRect::Unserialize( CDmxElement *pGraphic )
{
	pGraphic->UnpackIntoStructure( this, s_GameRectUnpack );


	// GEOMETRY
	CDmxAttribute *pRelativePositions = pGraphic->GetAttribute( "relativepositions" );
	if ( !pRelativePositions || pRelativePositions->GetType() != AT_VECTOR2_ARRAY )
    {
		return false;
    }
	const CUtlVector< Vector2D > &relpositions = pRelativePositions->GetArray< Vector2D >( );
	int nCount = relpositions.Count();
	m_Geometry.m_RelativePositions.RemoveAll();
    for ( int i = 0; i < nCount; ++i )
    {
		m_Geometry.m_RelativePositions.AddToTail( Vector2D( relpositions[i].x, relpositions[i].y ) );	
    }



	CDmxAttribute *pTexCoords = pGraphic->GetAttribute( "texcoords" );
	if ( !pTexCoords || pTexCoords->GetType() != AT_VECTOR2_ARRAY )
    {
		return false;
    }
	const CUtlVector< Vector2D > &texcoords = pTexCoords->GetArray< Vector2D >( );
	nCount = texcoords.Count();
	m_Geometry.m_TextureCoords.RemoveAll();
    for ( int i = 0; i < nCount; ++i )
    {
		m_Geometry.m_TextureCoords.AddToTail( Vector2D( texcoords[i].x, texcoords[i].y ) );	
    }

	SetupVertexColors();


	CDmxAttribute *pTriangles = pGraphic->GetAttribute( "triangles" );
	if ( !pTriangles || pTriangles->GetType() != AT_ELEMENT_ARRAY )
    {
		return false;
    }
	const CUtlVector< CDmxElement * > &triangles = pTriangles->GetArray< CDmxElement * >( );
	nCount = triangles.Count();
	m_Geometry.m_Triangles.RemoveAll();
    for ( int i = 0; i < nCount; ++i )
    {
		CDmxAttribute *pPoints = triangles[i]->GetAttribute( "positionindexes" );
		const CUtlVector< int > &points = pPoints->GetArray< int >( );

		CTriangle triangle;
		triangle.m_PointIndex[0] = points[0];
		triangle.m_PointIndex[1] = points[1];
		triangle.m_PointIndex[2] = points[2];

		m_Geometry.m_Triangles.AddToTail( triangle );	
    }



	// ANIMSTATES
	CDmxAttribute *pImageAnims = pGraphic->GetAttribute( "imageanims" );
	if ( !pImageAnims || pImageAnims->GetType() != AT_ELEMENT_ARRAY )
    {
		return false;
    }
	const CUtlVector< CDmxElement * > &imageanims = pImageAnims->GetArray< CDmxElement * >( );
	nCount = imageanims.Count();
    for ( int i = 0; i < nCount; ++i )
    {
		CAnimData *pAnimData = new CAnimData;
		if ( !pAnimData->Unserialize( imageanims[i] ) )
		{
			delete pAnimData;
			return false;
		}
		m_Anims.AddToTail( pAnimData );
    }

	// Ok the initial state is 0, which is (usually ) default.
	// default could be aliased to another state though so if it is fix the initial state here.
	// default might also not be the state that is 0 so this sets the graphic's initial 
	// state to be the default one.
	SetState( "default" );

	return true;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameRect::UpdateGeometry()
{	
	if ( m_CurrentState == -1 )
		return;

	Assert( m_CurrentState < m_Anims.Count() );

	DmeTime_t flAnimTime = GetAnimationTimePassed();

	// Update texture
	m_Geometry.m_SheetSequenceNumber = m_Anims[ m_CurrentState ]->m_TextureAnimSheetSeqNumber;
	m_Geometry.m_AnimationRate = m_Anims[ m_CurrentState ]->m_AnimationRate;

	// Update color
	m_Anims[ m_CurrentState ]->m_ColorAnim.GetValue( flAnimTime, &m_Geometry.m_Color );
	
	// Update center location
	m_Anims[ m_CurrentState ]->m_CenterPosAnim.GetValue( flAnimTime, &m_Geometry.m_Center );
	
	// Update scale
	m_Anims[ m_CurrentState ]->m_ScaleAnim.GetValue( flAnimTime, &m_Geometry.m_Scale );

	// Update rotation
	m_Anims[ m_CurrentState ]->m_RotationAnim.GetValue( flAnimTime, &m_Geometry.m_Rotation );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameRect::UpdateRenderData( color32 parentColor, CUtlVector< RenderGeometryList_t > &renderGeometryLists, int firstListIndex )
{
	if ( !m_Geometry.m_bVisible )
		return;

	m_Geometry.SetResultantColor( parentColor );
	m_Geometry.UpdateRenderData( renderGeometryLists, firstListIndex );

	// Now transform our array of positions into local graphic coord system.
	int nCount = m_Geometry.m_RelativePositions.Count();
	m_ScreenPositions.RemoveAll();
	for ( int i = 0; i < nCount; ++i )
	{
		// Position
		Vector relativePosition( m_Geometry.m_RelativePositions[i].x, m_Geometry.m_RelativePositions[i].y, 0 );
		Vector screenpos;
		VectorTransform( relativePosition, m_Geometry.m_RenderToScreen, screenpos );
		m_ScreenPositions.AddToTail( Vector2D( screenpos.x, screenpos.y ) );
	}	
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameRect::SetupVertexColors()
{
	m_Geometry.m_VertexColors.RemoveAll();

	// Create 4 vertex colors for this rect.
	color32 c;
	c.r = 255;
	c.g = 255;
	c.b = 255;
	c.a = 255;
	m_Geometry.m_VertexColors.AddToTail( c );
	m_Geometry.m_VertexColors.AddToTail( c );
	m_Geometry.m_VertexColors.AddToTail( c );
	m_Geometry.m_VertexColors.AddToTail( c );
}

//-----------------------------------------------------------------------------
//	Determine if x,y is inside the graphic.
//-----------------------------------------------------------------------------
bool CGameRect::HitTest( int x, int y )
{
	if ( !m_Geometry.m_bVisible ) 
		return false;

	if ( m_ScreenPositions.Count() == 0 )
		return false;

	for ( int i = 0; i < m_Geometry.GetTriangleCount(); ++i )
	{
		if ( PointTriangleHitTest( 
			m_ScreenPositions[ m_Geometry.m_Triangles[i].m_PointIndex[0] ],
			m_ScreenPositions[ m_Geometry.m_Triangles[i].m_PointIndex[1] ],
			m_ScreenPositions[ m_Geometry.m_Triangles[i].m_PointIndex[2] ],
			Vector2D( x, y ) ) )
		{
			//Msg( "%d, %d hit\n", x, y );
			return true;
		}
	}

	return false;
}




