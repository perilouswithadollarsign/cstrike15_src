//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "dynamicrect.h"
// To handle scaling
#include "materialsystem/imaterialsystem.h"
#include "animdata.h"
#include "Color.h"
#include "gameuisystemmgr.h"
#include "gameuidefinition.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Class factory for scripting.
class CDynamicRectClassFactory : IGameUIGraphicClassFactory
{
public:

	CDynamicRectClassFactory()
	{
		Assert( g_pGameUISystemMgrImpl );
		g_pGameUISystemMgrImpl->RegisterGraphicClassFactory( "rect", this );
	}

	// Returns an instance of a graphic interface (keyvalues owned by caller)
	virtual CGameGraphic *CreateNewGraphicClass( KeyValues *kvRequest, CGameUIDefinition *pMenu )
	{
		Assert( pMenu );
		CDynamicRect *pNewGraphic = NULL;

		const char *pName = kvRequest->GetString( "name", NULL );
		if ( pName )
		{
			pNewGraphic = new CDynamicRect( pName );
			// Rects are normally 0,0, doing this so we can see script created rects.
			pNewGraphic->SetScale( 100, 100 );
			pMenu->AddGraphicToLayer( pNewGraphic, SUBLAYER_DYNAMIC );

			// Now set the attributes.
			for ( KeyValues *arg = kvRequest->GetFirstSubKey(); arg != NULL; arg = arg->GetNextKey() )
			{
				pNewGraphic->HandleScriptCommand( arg );	
			}
		}
		return pNewGraphic;	
	}
};
static CDynamicRectClassFactory g_CDynamicRectClassFactory;


BEGIN_DMXELEMENT_UNPACK ( CDynamicRect )
	DMXELEMENT_UNPACK_FIELD_UTLSTRING( "imagealias", "", m_ImageAlias ) 
	
END_DMXELEMENT_UNPACK( CDynamicRect, s_GameDynamicRectUnpack )

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CDynamicRect::CDynamicRect( const char *pName ) : CGameRect( pName ) 
{
	// The default maps to white pixel.
	m_ImageAlias = "defaultImageAlias";
}


CDynamicRect::~CDynamicRect() 
{
	g_pGameUISystemMgrImpl->ReleaseImageAlias( m_ImageAlias );
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CDynamicRect::Unserialize( CDmxElement *pGraphic )
{
	CGameRect::Unserialize( pGraphic );
	pGraphic->UnpackIntoStructure( this, s_GameDynamicRectUnpack );

	m_CurrentState = -1;
	
	g_pGameUISystemMgrImpl->InitImageAlias( m_ImageAlias );
	g_pGameUISystemMgrImpl->LoadImageAliasTexture( m_ImageAlias, "vguiedit/pixel" );

	return true;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CDynamicRect::UpdateRenderData( color32 parentColor, CUtlVector< RenderGeometryList_t > &renderGeometryLists, int firstListIndex )
{
	if ( !m_Geometry.m_bVisible )
		return;
	
	m_Geometry.SetResultantColor( parentColor );

	int i = renderGeometryLists[firstListIndex].AddToTail();
	CRenderGeometry &renderGeometry = renderGeometryLists[firstListIndex][i];

	// Now transform our array of positions into local graphic coord system.
	int nCount = m_Geometry.m_RelativePositions.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		// Position
		Vector relativePosition( m_Geometry.m_RelativePositions[i].x, m_Geometry.m_RelativePositions[i].y, 0 );
		Vector screenpos;
		VectorTransform( relativePosition, m_Geometry.m_RenderToScreen, screenpos );
		renderGeometry.m_Positions.AddToTail( Vector2D( screenpos.x, screenpos.y ) );;
		// TexCoord
		Vector2D sheetTexCoords;
		g_pGameUISystemMgrImpl->TexCoordsToSheetTexCoords( m_ImageAlias, m_Geometry.m_TextureCoords[i], sheetTexCoords );
		renderGeometry.m_TextureCoords.AddToTail( sheetTexCoords );
		
		// Vertex Color
		renderGeometry.m_VertexColors.AddToTail( m_Geometry.m_VertexColors[i] );
	}

	// Triangles
	nCount = m_Geometry.m_Triangles.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		renderGeometry.m_Triangles.AddToTail( m_Geometry.m_Triangles[i] );	
	}

	// Anim Info														   
	renderGeometry.m_SheetSequenceNumber = m_Geometry.m_SheetSequenceNumber;
	renderGeometry.m_AnimationRate = m_Geometry.m_AnimationRate;
	renderGeometry.m_bAnimate = m_Geometry.m_bAnimate;
	renderGeometry.m_AnimStartTime = m_Geometry.m_AnimStartTime;	
	
	// Set the image alias. This is so we can adjust texture coords if needed if 
	// This rect's texture gets placed in a sheet.
	renderGeometry.m_pImageAlias = m_ImageAlias;



	// Now transform our array of positions into local graphic coord system.
	nCount = m_Geometry.m_RelativePositions.Count();
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

KeyValues *CDynamicRect::HandleScriptCommand( KeyValues *args )
{
	char const *szCommand = args->GetName();

	if ( !Q_stricmp( "SetAlias", szCommand ) )
	{
		g_pGameUISystemMgrImpl->ReleaseImageAlias( m_ImageAlias );
		m_ImageAlias = args->GetString( "alias", "defaultImageAlias" );
		g_pGameUISystemMgrImpl->InitImageAlias( m_ImageAlias );
		return NULL;
	}

	return CGameRect::HandleScriptCommand( args );
}




