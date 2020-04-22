//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "graphicgroup.h"
#include "gamegraphic.h"
#include "gameuidefinition.h"
#include "animdata.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DMXELEMENT_UNPACK ( CGraphicGroup ) 
	DMXELEMENT_UNPACK_FIELD_UTLSTRING( "name", "", m_pName ) 
	DMXELEMENT_UNPACK_FIELD( "center", "0 0", Vector2D, m_Geometry.m_Center ) 
	DMXELEMENT_UNPACK_FIELD( "scale", "1 1", Vector2D, m_Geometry.m_Scale ) 
	DMXELEMENT_UNPACK_FIELD( "rotation", "0", float, m_Geometry.m_Rotation )
	DMXELEMENT_UNPACK_FIELD( "maintainaspectratio", "0", bool, m_Geometry.m_bMaintainAspectRatio )
	DMXELEMENT_UNPACK_FIELD( "sublayertype", "0", int, m_Geometry.m_Sublayer )
	DMXELEMENT_UNPACK_FIELD( "visible", "1", bool, m_Geometry.m_bVisible )
	DMXELEMENT_UNPACK_FIELD( "initialstate", "-1", int, m_CurrentState )
	DMXELEMENT_UNPACK_FIELD( "horizgradient", "0", bool, m_Geometry.m_bHorizontalGradient )
	DMXELEMENT_UNPACK_FIELD( "color", "255 255 255 255", Color, m_Geometry.m_Color )
	DMXELEMENT_UNPACK_FIELD( "topcolor", "255 255 255 255", Color, m_Geometry.m_TopColor )
	DMXELEMENT_UNPACK_FIELD( "bottomcolor", "255 255 255 255", Color, m_Geometry.m_BottomColor )
END_DMXELEMENT_UNPACK( CGraphicGroup, s_GraphicGroupUnpack )

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CGraphicGroup::CGraphicGroup()
{
	m_ResultantColor.r = 0;
	m_ResultantColor.g = 0;
	m_ResultantColor.b = 0;
	m_ResultantColor.a = 0;
}

CGraphicGroup::~CGraphicGroup()
{
}

//-----------------------------------------------------------------------------
// Load data from file.
//-----------------------------------------------------------------------------
bool CGraphicGroup::Unserialize( CDmxElement *pElement, CUtlDict< CGameGraphic *, int > &unserializedGraphicMapping )
{
	pElement->UnpackIntoStructure( this, s_GraphicGroupUnpack );

	CDmxAttribute *pGroupElements = pElement->GetAttribute( "groupElements" );
	if ( !pGroupElements || pGroupElements->GetType() != AT_ELEMENT_ARRAY )
	{
		return false;
	}
	const CUtlVector< CDmxElement * > &elements = pGroupElements->GetArray< CDmxElement * >( );
	int nCount = elements.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		// Find the element in the map.
		char pBuf[255];
		UniqueIdToString( elements[i]->GetId(), pBuf, 255 );
		int index = unserializedGraphicMapping.Find( pBuf );
		Assert( unserializedGraphicMapping.IsValidIndex( index ) );
		CGameGraphic *pGraphic = unserializedGraphicMapping.Element(index);	
		pGraphic->SetGroup( this );
		m_MemberList.AddToTail( pGraphic );
	}


	// ANIMSTATES
	CDmxAttribute *pImageAnims = pElement->GetAttribute( "imageanims" );
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

	char pBuf[255];
	UniqueIdToString( pElement->GetId(), pBuf, 255 );
	unserializedGraphicMapping.Insert( pBuf, this );

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
void CGraphicGroup::UpdateGeometry()
{	
	if ( m_CurrentState == -1 )
		return;

	DmeTime_t flAnimTime = GetAnimationTimePassed();

	// Update color
	m_Anims[ m_CurrentState ]->m_ColorAnim.GetValue( flAnimTime, &m_Geometry.m_Color );

	// Update center location
	m_Anims[ m_CurrentState ]->m_CenterPosAnim.GetValue( flAnimTime, &m_Geometry.m_Center );

	// Update scale
	m_Anims[ m_CurrentState ]->m_ScaleAnim.GetValue( flAnimTime, &m_Geometry.m_Scale );

	// Update rotation
	m_Anims[ m_CurrentState ]->m_RotationAnim.GetValue( flAnimTime, &m_Geometry.m_Rotation );

	for ( int i = 0; i < m_MemberList.Count(); ++i )
	{
		m_MemberList[i]->UpdateGeometry();
	}
}

//-----------------------------------------------------------------------------
// Store the resultant color for groups so kids can just get that.
//-----------------------------------------------------------------------------
void CGraphicGroup::UpdateRenderData( color32 parentColor )
{
	if ( !m_Geometry.m_bVisible )
		return;

	m_ResultantColor.r = (int)( (float)m_Geometry.m_Color.r * (float)(parentColor.r/255.0) );
	m_ResultantColor.g = (int)( (float)m_Geometry.m_Color.g * (float)(parentColor.g/255.0) );
	m_ResultantColor.b = (int)( (float)m_Geometry.m_Color.b * (float)(parentColor.b/255.0) );	
	m_ResultantColor.a = (int)( (float)m_Geometry.m_Color.a * (float)(parentColor.a/255.0) );

	// Update any children that are groups 
	// Graphic members are not done because the colors are calculated when we get the render data out.
	for ( int i = 0; i < m_MemberList.Count(); i++ )
	{
		if ( m_MemberList[i]->IsGroup() )
		{
			CGraphicGroup *pGroup = (CGraphicGroup *)m_MemberList[i];
			pGroup->UpdateRenderData( parentColor );
		}			
	}
}

//-----------------------------------------------------------------------------
// Populate lists for rendering
//-----------------------------------------------------------------------------
void CGraphicGroup::UpdateRenderTransforms( const StageRenderInfo_t &stageRenderInfo )
{
	m_Geometry.UpdateRenderTransforms( stageRenderInfo, GetGroup() );


	// Create a matrix that ensures no aspect ratio changes.
	// Update positions relative to the center, texture coords, and vertex colors
	// If the group maintains aspect ratio it will already have handled this in its transform update.
	Vector2D center;
	// If this is the case we transform the center to screen coords first.
	// Then take into account any size scaling in the scalemat
	matrix3x4_t screenScalemat;
	SetScaleMatrix( stageRenderInfo.parentScale.x, stageRenderInfo.parentScale.y, 1, screenScalemat );
	Vector centerVec( m_Geometry.m_Center.x, m_Geometry.m_Center.y, 0 );
	Vector centerInScreen;
	VectorTransform( centerVec, screenScalemat, centerInScreen );
	center.x = centerInScreen.x;
	center.y = centerInScreen.y;
	

	matrix3x4_t transmat;
	Vector position( center.x, center.y, 0 );
	SetIdentityMatrix( transmat );
	PositionMatrix( position, transmat );

	matrix3x4_t scalemat;
	SetScaleMatrix( m_Geometry.m_Scale.x, m_Geometry.m_Scale.y, 1, scalemat );

	matrix3x4_t rotmat;
	Vector axis( 0, 0, 1 );
	MatrixBuildRotationAboutAxis( axis, m_Geometry.m_Rotation, rotmat );

	matrix3x4_t temp;
	MatrixMultiply( rotmat, scalemat, temp );
	matrix3x4_t rawToLocal;
	MatrixMultiply( transmat, temp, rawToLocal );

	matrix3x4_t groupToScreen;
	// Use the matrix that doesn't contain any scale changes if we should
	m_pGroup->GetRenderTransform( groupToScreen, true );
	MatrixMultiply( groupToScreen, rawToLocal, m_RelToScreenHoldAspectRatio );




	// Update all children 
	for ( int i = 0; i < m_MemberList.Count(); i++ )
	{
		m_MemberList[i]->UpdateRenderTransforms( stageRenderInfo );		
	}

}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CGraphicGroup::AddToGroup( CGameGraphic *pGraphic )
{
	m_MemberList.AddToTail( pGraphic );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CGraphicGroup::RemoveFromGroup( CGameGraphic *pGraphic )
{
	for ( int i = 0; i < m_MemberList.Count(); i++ )
	{
		// TODO, what if the graphic is a group?
		if ( m_MemberList[i] == pGraphic )
		{
			m_MemberList.Remove( i );
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Returns true if any graphic in this group has the state.
//-----------------------------------------------------------------------------
bool CGraphicGroup::HasState( const char *pStateName )
{
	if ( CGameGraphic::HasState( pStateName ) )
		return true;

	for ( int i = 0; i < m_MemberList.Count(); i++ )
	{
		if ( m_MemberList[i]->HasState( pStateName ) )
			return true;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Set the state of all members to this state
//-----------------------------------------------------------------------------
void CGraphicGroup::SetState( const char *pStateName )
{
	CGameGraphic::SetState( pStateName );

	for ( int i = 0; i < m_MemberList.Count(); i++ )
	{
		m_MemberList[i]->SetState( pStateName );		
	}
}

//-----------------------------------------------------------------------------
// Start playing animations
//-----------------------------------------------------------------------------
void CGraphicGroup::StartPlaying()
{
	CGameGraphic::StartPlaying();

	for ( int i = 0; i < m_MemberList.Count(); i++ )
	{
		m_MemberList[i]->StartPlaying();		
	}
}

//-----------------------------------------------------------------------------
// Stop playing animations
//-----------------------------------------------------------------------------
void CGraphicGroup::StopPlaying()
{
	CGameGraphic::StopPlaying();

	for ( int i = 0; i < m_MemberList.Count(); i++ )
	{
		m_MemberList[i]->StopPlaying();		
	}
}

//-----------------------------------------------------------------------------
// Move all members to the next available state
// Note this could put all of them into different states
//-----------------------------------------------------------------------------
void CGraphicGroup::AdvanceState()
{
	CGameGraphic::AdvanceState();

	for ( int i = 0; i < m_MemberList.Count(); i++ )
	{
		m_MemberList[i]->AdvanceState();		
	}
}


//-----------------------------------------------------------------------------
// Return the first member of this group that can have keyfocus.
//-----------------------------------------------------------------------------
CHitArea *CGraphicGroup::GetKeyFocusRequestGraphic()
{
	for ( int i = 0; i < m_MemberList.Count(); i++ )
	{
		if ( m_MemberList[i]->CanAcceptInput() )
		{
			return ( CHitArea * )m_MemberList[i];
		}
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Does this graphic own a graphic with this name?
//-----------------------------------------------------------------------------
CGameGraphic *CGraphicGroup::FindGraphicByName( const char *pName )	const
{ 
	int nGraphicCount = m_MemberList.Count();
	for ( int i = 0; i < nGraphicCount; ++i )
	{
		CGameGraphic *pMember = m_MemberList[i];
		if ( pMember->IsGraphicNamed( pName ) )
		{
			// Match.
			return pMember;
		}
	}

	return NULL;
}


//-----------------------------------------------------------------------------
// Group visibility affects all children.
//-----------------------------------------------------------------------------
void CGraphicGroup::SetVisible( bool bVisible )
{
	CGameGraphic::SetVisible( bVisible );

	int nGraphicCount = m_MemberList.Count();
	for ( int i = 0; i < nGraphicCount; ++i )
	{
		CGameGraphic *pMember = m_MemberList[i];
		pMember->SetVisible( bVisible );
	}
}

//-----------------------------------------------------------------------------
// Return the appropriate render transform.
// m_RelToScreenHoldAspectRatio is calculated for a stage aspect ratio that has not changed.
//-----------------------------------------------------------------------------
void CGraphicGroup::GetRenderTransform( matrix3x4_t &relToScreen, bool bMaintainAspectRatio ) const 
{  
	if ( bMaintainAspectRatio )
	{
		relToScreen = m_RelToScreenHoldAspectRatio; 
	}
	else
	{
		relToScreen = m_Geometry.m_RenderToScreen;
	}
} 



//-----------------------------------------------------------------------------
// If any parent of this group should maintain aspect ratio, this group should.
//-----------------------------------------------------------------------------
bool CGraphicGroup::MaintainAspectRatio() const 
{ 
	if ( m_pGroup && !m_Geometry.m_bMaintainAspectRatio )
	{
		return m_pGroup->MaintainAspectRatio();
	}

	return m_Geometry.m_bMaintainAspectRatio;	 
}






