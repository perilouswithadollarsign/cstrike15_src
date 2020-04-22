//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "gamestage.h"
// To handle scaling
#include "materialsystem/imaterialsystem.h"
#include "animdata.h"
#include "Color.h"
#include "gameuisystemmgr.h"



// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DMXELEMENT_UNPACK ( CGameStage )
	DMXELEMENT_UNPACK_FIELD( "stagesize", "1024 768", Vector2D, m_StageSize )
	DMXELEMENT_UNPACK_FIELD( "fullscreen", "1", bool, m_bFullscreen )
	
END_DMXELEMENT_UNPACK( CGameStage, s_GameStageUnpack )

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CGameStage::CGameStage() 
{
	m_pName = "stage";
	m_CurrentState = -1;
	m_StageSize.x = 1024;
	m_StageSize.y = 768;
	m_bFullscreen = true;

	m_Geometry.m_bMaintainAspectRatio = false;

	m_StageRenderInfo.parentPos.x = 0;
	m_StageRenderInfo.parentPos.y = 0;
	m_StageRenderInfo.parentPos.z = 0;
	m_StageRenderInfo.parentScale.x = 0;
	m_StageRenderInfo.parentScale.y = 0;
	m_StageRenderInfo.parentRot = 0;

	m_MaintainAspectRatioStageSize.x = 640;
	m_MaintainAspectRatioStageSize.y = 480;
}


CGameStage::~CGameStage() 
{
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CGameStage::Unserialize( CDmxElement *pElement, CUtlDict< CGameGraphic *, int > &unserializedGraphicMapping )
{
	pElement->UnpackIntoStructure( this, s_GameStageUnpack );

	if ( !CGraphicGroup::Unserialize( pElement, unserializedGraphicMapping ) )
		return false;

	// Ok the initial state is 0, which is ( usually ) default.
	// default could be aliased to another state though so if it is fix the initial state here.
	// default might also not be the state that is 0 so this sets the graphic's initial 
	// state to be the default one.
	SetState( "default" );

	return true;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameStage::UpdateAspectRatio( const Rect_t &viewport )
{
	// First do stage scale. 
	matrix3x4_t scalemat;
	float screenScaleX = (float)(viewport.width) / m_StageSize.x;
	float screenScaleY = (float)(viewport.height) / m_StageSize.y;

	if ( screenScaleX > screenScaleY )
	{
		screenScaleX = screenScaleY;
	}
	else
	{
		screenScaleY = screenScaleX;
	}

	m_MaintainAspectRatioStageSize.x = screenScaleX * m_StageSize.x;
	m_MaintainAspectRatioStageSize.y = screenScaleY * m_StageSize.y;
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameStage::UpdateRenderTransforms( const Rect_t &viewport )
{
	// Build the raw stage matrix.

	// Move the stage to its position.
	matrix3x4_t transmat;
	SetIdentityMatrix( transmat );
	Vector2D screenPos = Vector2D( viewport.width/2, viewport.height/2 );
	screenPos += m_Geometry.m_Center;
	m_StageRenderInfo.parentPos = Vector( screenPos.x, screenPos.y, 0 );
	PositionMatrix( m_StageRenderInfo.parentPos, transmat);

	// First do stage scale. 
	matrix3x4_t scalemat;
	float screenScaleX = (float)(viewport.width) / m_StageSize.x;
	float screenScaleY = (float)(viewport.height) / m_StageSize.y;
	m_StageRenderInfo.parentScale = Vector2D( screenScaleX, screenScaleY );
	m_StageRenderInfo.parentScale *= m_Geometry.m_Scale;
	SetScaleMatrix( m_StageRenderInfo.parentScale.x, m_StageRenderInfo.parentScale.y, 1, scalemat );

	// Build the stage rotation matrix. Normally this is 0.
	matrix3x4_t rotmat;
	m_StageRenderInfo.parentRot = m_Geometry.m_Rotation;
	MatrixBuildRotationAboutAxis( Vector( 0, 0, 1 ), m_StageRenderInfo.parentRot, rotmat );

	// Multiply matrices together in the correct order.
	matrix3x4_t temp;
	MatrixMultiply( rotmat, scalemat, temp );
	matrix3x4_t stageRenderMatrix;
	MatrixMultiply( transmat, temp, stageRenderMatrix );


	m_StageRenderInfo.relToScreen = stageRenderMatrix;


	// Now build another stage matrix that holds the aspect ratio.
	// This will scale the size of the graphic while maintaining its aspect ratio.

	// Move the graphic to its stage position.
	SetIdentityMatrix( transmat );
	PositionMatrix( m_StageRenderInfo.parentPos, transmat);

	if ( screenScaleX > screenScaleY )
	{
		screenScaleX = screenScaleY;
	}
	else
	{
		screenScaleY = screenScaleX;
	}

	Vector2D screenScale = Vector2D( screenScaleX, screenScaleY );
	screenScale *= m_Geometry.m_Scale;
	SetScaleMatrix( screenScale.x, screenScale.y, 1, scalemat );

	// Build the stage rotation matrix. 
	MatrixBuildRotationAboutAxis( Vector( 0, 0, 1 ), m_StageRenderInfo.parentRot, rotmat );

	// Multiply matrices together in the correct order.
	MatrixMultiply( rotmat, scalemat, temp );
	matrix3x4_t stageRenderMatrixHoldAspectRatio;
	MatrixMultiply( transmat, temp, stageRenderMatrixHoldAspectRatio );

	m_StageRenderInfo.relToScreenHoldAspectRatio = stageRenderMatrixHoldAspectRatio;

	// Update all children 
	for ( int i = 0; i < m_MemberList.Count(); i++ )
	{
		m_MemberList[i]->UpdateRenderTransforms( m_StageRenderInfo );		
	}

	m_ResultantColor = m_Geometry.m_Color;
	CGraphicGroup::UpdateRenderData( m_Geometry.m_Color );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameStage::SetStageSize( int nWide, int nTall )
{
	m_StageSize.x = nWide;
	m_StageSize.y = nTall;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGameStage::GetRenderTransform( matrix3x4_t &relToScreen, bool bMaintainAspectRatio ) const 
{ 
	if ( bMaintainAspectRatio )
	{
		relToScreen = m_StageRenderInfo.relToScreenHoldAspectRatio; 
	}
	else
	{
		relToScreen = m_StageRenderInfo.relToScreen;
	}
}




