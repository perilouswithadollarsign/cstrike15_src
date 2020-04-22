//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "hitarea.h"
// To handle scaling
#include "materialsystem/imaterialsystem.h"
#include "animdata.h"
#include "inputsystem/inputenums.h"
#include "inputsystem/analogcode.h"
#include "inputsystem/buttoncode.h"
#include "gameuisystemmgr.h"
#include "graphicgroup.h"
#include "inputgameui.h"
#include "graphicscriptinterface.h"
#include "inputgameui.h"
#include "gameuisystemmgr.h"
#include "gameuiscript.h"
#include "gameuisystem.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define DEBUG_INPUT_EVENTS 0

// Class factory for scripting.
class CHitAreaClassFactory : IGameUIGraphicClassFactory
{
public:

	CHitAreaClassFactory()
	{
		Assert( g_pGameUISystemMgrImpl );
		g_pGameUISystemMgrImpl->RegisterGraphicClassFactory( "hitarea", this );
	}

	// Returns an instance of a graphic interface (keyvalues owned by caller)
	virtual CGameGraphic *CreateNewGraphicClass( KeyValues *kvRequest, CGameUIDefinition *pMenu )
	{
		Assert( pMenu );
		CHitArea *pNewGraphic = NULL;

		const char *pName = kvRequest->GetString( "name", NULL );
		if ( pName )
		{
			pNewGraphic = new CHitArea( pName );
			// Rects are normally 0,0, doing this so we can see script created rects.
			pNewGraphic->SetScale( 100, 100 );
			pMenu->AddGraphicToLayer( pNewGraphic, SUBLAYER_STATIC );
			
			// Now set the attributes.
			for ( KeyValues *arg = kvRequest->GetFirstSubKey(); arg != NULL; arg = arg->GetNextKey() )
			{
				pNewGraphic->HandleScriptCommand( arg );	
			}
		}
		return pNewGraphic;	
	}
};
static CHitAreaClassFactory g_CDynamicRectClassFactory;


BEGIN_DMXELEMENT_UNPACK ( CHitArea ) 
	DMXELEMENT_UNPACK_FIELD_UTLSTRING( "name", "", m_pName ) 
	DMXELEMENT_UNPACK_FIELD( "center", "0 0", Vector2D, m_Geometry.m_Center ) 
	DMXELEMENT_UNPACK_FIELD( "scale", "0 0", Vector2D, m_Geometry.m_Scale ) 
	DMXELEMENT_UNPACK_FIELD( "rotation", "0", float, m_Geometry.m_Rotation )
	DMXELEMENT_UNPACK_FIELD( "maintainaspectratio", "0", bool, m_Geometry.m_bMaintainAspectRatio )
	DMXELEMENT_UNPACK_FIELD( "sublayertype", "0", int, m_Geometry.m_Sublayer )
	DMXELEMENT_UNPACK_FIELD( "visible", "1", bool, m_Geometry.m_bVisible )
	DMXELEMENT_UNPACK_FIELD( "initialstate", "-1", int, m_CurrentState )
	DMXELEMENT_UNPACK_FIELD( "dragenabled", "0", bool, m_bDragEnabled )
	DMXELEMENT_UNPACK_FIELD_UTLSTRING( "on_mouse_left_clicked_cmd", "", m_OnMouseLeftClickedScriptCommand )
	
END_DMXELEMENT_UNPACK( CHitArea, s_HitAreaUnpack )

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CHitArea::CHitArea( const char *pName ) 
{
	m_bCanAcceptInput = true;
	m_bCanStartDragging = false;
	m_IsDragging = false;
	m_DragStartCursorPos[0] = 0;
	m_DragStartCursorPos[1] = 0;
	m_DragCurrentCursorPos[0] = 0;
	m_DragCurrentCursorPos[1] = 0;
	

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
	m_bDragEnabled = false;
	m_OnMouseLeftClickedScriptCommand = NULL;

	m_Geometry.m_RelativePositions.AddToTail( Vector2D( -.5, -.5 ) );
	m_Geometry.m_RelativePositions.AddToTail( Vector2D( .5, -.5 ) );
	m_Geometry.m_RelativePositions.AddToTail( Vector2D( .5, .5 ) );
	m_Geometry.m_RelativePositions.AddToTail( Vector2D( -.5, .5 ) );

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


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
CHitArea::~CHitArea() 
{
	// TODO: move to manager?/ as it should control allocations and deallocations.
	g_pInputGameUI->PanelDeleted( this );
}

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
bool CHitArea::Unserialize( CDmxElement *pGraphic )
{
	pGraphic->UnpackIntoStructure( this, s_HitAreaUnpack );


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


	SetState( "default" );

	return true;
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void CHitArea::UpdateGeometry()
{	
	if ( m_CurrentState == -1 )
		return;

	DmeTime_t flAnimTime = GetAnimationTimePassed();

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
void CHitArea::UpdateRenderData( color32 parentColor, CUtlVector< RenderGeometryList_t > &renderGeometryLists, int firstListIndex )
{
	m_Geometry.CalculateExtents();
	bool bDrawHitAreas = false;

	if ( !m_Geometry.m_bVisible )
		return;

	if ( bDrawHitAreas )
	{
		// Time to invent some render data to draw this thing.

		int i = renderGeometryLists[firstListIndex].AddToTail();
		CRenderGeometry &renderGeometry = renderGeometryLists[firstListIndex][i];

		int nCount = m_Geometry.m_RelativePositions.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			// Position
			Vector relativePosition( m_Geometry.m_RelativePositions[i].x, m_Geometry.m_RelativePositions[i].y, 0 );
			Vector screenpos;
			VectorTransform( relativePosition, m_Geometry.m_RenderToScreen, screenpos );
			renderGeometry.m_Positions.AddToTail( Vector2D( screenpos.x, screenpos.y ) );;

			// Vertex Color
			color32 hitAreaColor;
			hitAreaColor.r = 255;
			hitAreaColor.g = 100;
			hitAreaColor.b = 100;
			hitAreaColor.a = 255;

			renderGeometry.m_VertexColors.AddToTail( hitAreaColor );
		}

		// TexCoords
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 0 , 0) );
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 1 , 0) );
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 1 , 1) );
		renderGeometry.m_TextureCoords.AddToTail( Vector2D( 0 , 1) );

		// Triangles
		nCount = m_Geometry.m_Triangles.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			renderGeometry.m_Triangles.AddToTail( m_Geometry.m_Triangles[i] );	
		}

		// Anim Info	
		renderGeometry.m_SheetSequenceNumber = 0;
		renderGeometry.m_AnimationRate = 1;
		renderGeometry.m_bAnimate = 0;
		renderGeometry.m_pImageAlias = NULL;
	}

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
//	Determine if x,y is inside the graphic.
//-----------------------------------------------------------------------------
bool CHitArea::HitTest( int x, int y )
{
	if ( !m_bCanAcceptInput ) // note if graphic is invisible, this is false
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


//-----------------------------------------------------------------------------
// Event that occurs when cursor enters the geometry area
//-----------------------------------------------------------------------------
void CHitArea::OnCursorEnter()
{
#if ( DEBUG_INPUT_EVENTS )
	Msg( "CHitArea::OnCursorEnter\n" );
#endif
	if ( m_pGroup )
	{
		m_pGroup->SetState( "AUTO_GAINMOUSEFOCUS" );
	}

	KeyValues *kvEvent = new KeyValues( "OnMouseFocusGained" );
	KeyValues::AutoDelete autodelete( kvEvent );


	// chain to main system if this graphic doesn't handle it.
	g_pGameUISystemMgrImpl->OnGameGraphicScriptEvent( this, kvEvent );
	g_pGameUISystemMgrImpl->OnMouseFocusGained( this );
}

//-----------------------------------------------------------------------------
// Event that occurs when cursor leaves the geometry area
//-----------------------------------------------------------------------------
void CHitArea::OnCursorExit()
{
#if ( DEBUG_INPUT_EVENTS )
	Msg( "CHitArea::OnCursorExit\n" );
#endif
	if ( m_pGroup )
	{
		m_pGroup->SetState( "AUTO_LOSEMOUSEFOCUS" );
	}

	KeyValues *kvEvent = new KeyValues( "OnMouseFocusLost" );
	KeyValues::AutoDelete autodelete( kvEvent );


	// chain to main system if this graphic doesn't handle it.
	g_pGameUISystemMgrImpl->OnGameGraphicScriptEvent( this, kvEvent );
	g_pGameUISystemMgrImpl->OnMouseFocusLost( this );
}

#define DRAG_THRESHOLD_SQUARED 16
//-----------------------------------------------------------------------------
// Event that occurs when cursor moved inside the geometry area
//-----------------------------------------------------------------------------
void CHitArea::OnCursorMove( const int &cursorX, const int &cursorY )
{
	//Msg( "CHitArea::OnCursorMove\n" );
	if ( m_bCanStartDragging )
	{
		m_DragCurrentCursorPos[0] = cursorX;
		m_DragCurrentCursorPos[1] = cursorY;

		float dx = m_DragCurrentCursorPos[0] - m_DragStartCursorPos[0];
		float dy = m_DragCurrentCursorPos[1] - m_DragStartCursorPos[1];
		float distance = dx * dx + dy * dy;
		if ( distance > DRAG_THRESHOLD_SQUARED )
		{
#if ( DEBUG_INPUT_EVENTS )
			Msg( "CHitArea::Starting dragging\n" );
#endif
			OnDragStartCallScriptEvent( cursorX, cursorY );
			m_bCanStartDragging = false;
			m_IsDragging = true;
		}	
	}
	else if ( m_IsDragging )
	{
		m_DragCurrentCursorPos[0] = cursorX;
		m_DragCurrentCursorPos[1] = cursorY;
		OnDragCallScriptEvent( cursorX, cursorY );
	}
}

//-----------------------------------------------------------------------------
// Event that occurs when left mouse button is pressed
//-----------------------------------------------------------------------------
void CHitArea::OnMouseDown( const ButtonCode_t &code )
{
#if ( DEBUG_INPUT_EVENTS )
	Msg( "CHitArea::OnMouseDown\n" );
#endif
	
	// Drag and drop supported for left mouse button only.
	// This hit area must be drag enabled to support dragging.
	if ( code == MOUSE_LEFT  && m_bDragEnabled )
	{
		m_bCanStartDragging = true;	
		g_pInputGameUI->GetCursorPos( m_DragStartCursorPos[0], m_DragStartCursorPos[1] );
	}

	if ( m_pGroup )
	{
		if ( code == MOUSE_LEFT )
		{
			m_pGroup->SetState( "AUTO_MOUSELEFTDOWN" );
		}
		else if ( code == MOUSE_RIGHT )
		{
			m_pGroup->SetState( "AUTO_MOUSERIGHTDOWN" );
		}
		else if ( code == MOUSE_MIDDLE )
		{
			m_pGroup->SetState( "AUTO_MOUSEMIDDLEDOWN" );
		}
	}

	// Check for scripting for this control first.
	KeyValues *kvEvent = new KeyValues( "OnMouseDown" );
	KeyValues::AutoDelete autodelete( kvEvent );
	kvEvent->SetInt( "code", code );

	
	// Always call generic click handler to allow host-overrides
	kvEvent->SetName( "OnMouseClicked" );
	g_pGameUISystemMgrImpl->OnGameGraphicScriptEvent( this, kvEvent );

	// Call assigned handlers
	if ( code == MOUSE_LEFT && !m_OnMouseLeftClickedScriptCommand.IsEmpty() )
	{
		kvEvent->SetName( m_OnMouseLeftClickedScriptCommand );
		bool bExecuted = g_pGameUISystemMgrImpl->OnGameGraphicScriptEvent( this, kvEvent );
		if ( !bExecuted )
		{
			Warning( "Unable to find script function %s (assigned to OnMouseLeftClicked)\n", kvEvent->GetName() );
		}
	}
}

//-----------------------------------------------------------------------------
// Event that occurs when mouse button is released
// Script events should be tied to mouse release events only, not mouse down.
// Scripts should only fire if this graphic has mousefocus.
//-----------------------------------------------------------------------------
void CHitArea::OnMouseUp( const ButtonCode_t &code, bool bFireScripts )
{
#if ( DEBUG_INPUT_EVENTS )
	Msg( "CHitArea::OnMouseUp\n" );
#endif
	m_bCanStartDragging = false;	
	
	if ( m_pGroup )
	{
		if ( code == MOUSE_LEFT )
		{
			m_pGroup->SetState( "AUTO_MOUSELEFTUP" );
		}
		else if ( code == MOUSE_RIGHT )
		{
			m_pGroup->SetState( "AUTO_MOUSERIGHTUP" );
		}
		else if ( code == MOUSE_MIDDLE )
		{
			m_pGroup->SetState( "AUTO_MOUSEMIDDLEUP" );
		}
	}

	if ( m_IsDragging )
	{
#if ( DEBUG_INPUT_EVENTS )
		Msg( "CHitArea::Stopped dragging\n" );
#endif
		OnDragStopCallScriptEvent( m_DragCurrentCursorPos[0], m_DragCurrentCursorPos[1] );
	}
	else if ( bFireScripts )
	{
		KeyValues *kvEvent = new KeyValues( "OnMouseUp" );
		KeyValues::AutoDelete autodelete( kvEvent );
		kvEvent->SetInt( "code", code );

		// Always call generic click handler to allow host-overrides
		g_pGameUISystemMgrImpl->OnGameGraphicScriptEvent( this, kvEvent );
	}

	m_IsDragging  = false;
}


//-----------------------------------------------------------------------------
// Event that occurs when  mouse button is double clicked
//-----------------------------------------------------------------------------
void CHitArea::OnMouseDoubleClick( const ButtonCode_t &code )
{
#if ( DEBUG_INPUT_EVENTS )
	Msg( "CHitArea::OnMouseDoubleClick\n" );
#endif
	if ( m_pGroup )
	{
		m_pGroup->SetState( "AUTO_MOUSEDOUBLECLICK" );
	}
}


//-----------------------------------------------------------------------------
// Event that occurs when a key is pressed
//-----------------------------------------------------------------------------
void CHitArea::OnKeyDown( const ButtonCode_t &code )
{
#if ( DEBUG_INPUT_EVENTS )
	Msg( "CHitArea::OnKeyDown\n" );
#endif
	// Pad input gives you pressed and released messages only for buttons
	if ( IsJoystickCode( code ) )
	{
		KeyValues *kvEvent = new KeyValues( "OnButtonPressed" );
		KeyValues::AutoDelete autodelete( kvEvent );
		kvEvent->SetInt( "code", code );

		
		// chain to main system if this graphic doesn't handle it.
		g_pGameUISystemMgrImpl->OnKeyCodeTyped( code );
		g_pGameUISystemMgrImpl->OnGameGraphicScriptEvent( this, kvEvent );	
	}
	else
	{
		if ( m_pGroup )
		{
			m_pGroup->SetState( "AUTO_KEYDOWN" );
		}
	}
}

//-----------------------------------------------------------------------------
// Event that occurs when a key is released
//-----------------------------------------------------------------------------
void CHitArea::OnKeyUp( const ButtonCode_t &code )
{
#if ( DEBUG_INPUT_EVENTS )
	Msg( "CHitArea::OnKeyUp\n" );
#endif

	// Pad input gives you pressed and released messages only for buttons
	if ( IsJoystickCode( code ) )
	{
		KeyValues *kvEvent = new KeyValues( "OnButtonReleased" );
		KeyValues::AutoDelete autodelete( kvEvent );
		kvEvent->SetInt( "code", code );

		
		// chain to main system if this graphic doesn't handle it.
		g_pGameUISystemMgrImpl->OnKeyCodeTyped( code );
		g_pGameUISystemMgrImpl->OnGameGraphicScriptEvent( this, kvEvent );
	}
	else
	{
		if ( m_pGroup )
		{
			m_pGroup->SetState( "AUTO_KEYUP" );
		}
	}	
}

//-----------------------------------------------------------------------------
// Event that occurs when a key code is typed
//-----------------------------------------------------------------------------
void CHitArea::OnKeyCodeTyped( const ButtonCode_t &code )
{
#if ( DEBUG_INPUT_EVENTS )
	Msg( "CHitArea::OnKeyCodeTyped\n" );
#endif
	Assert( g_pInputGameUI->GetKeyFocus() == this );

	KeyValues *kvEvent = new KeyValues( "OnKeyTyped" );
	KeyValues::AutoDelete autodelete( kvEvent );
	kvEvent->SetInt( "code", code );

	
	// chain to main system if this graphic doesn't handle it.
	g_pGameUISystemMgrImpl->OnKeyCodeTyped( code );
	g_pGameUISystemMgrImpl->OnGameGraphicScriptEvent( this, kvEvent );	
}

//-----------------------------------------------------------------------------
// Event that occurs when a key is typed 
//-----------------------------------------------------------------------------
void CHitArea::OnKeyTyped( const wchar_t &unichar )
{
#if ( DEBUG_INPUT_EVENTS )
	Msg( "CHitArea::OnKeyTyped\n" );
#endif
	Assert( g_pInputGameUI->GetKeyFocus() == this );


	// chain to main system if this graphic doesn't handle it.
	g_pGameUISystemMgrImpl->OnKeyTyped( unichar );
}

//-----------------------------------------------------------------------------
// Event that occurs when key focus is lost
//-----------------------------------------------------------------------------
void CHitArea::OnLoseKeyFocus()
{
#if ( DEBUG_INPUT_EVENTS )
	Msg( "CHitArea::OnLoseKeyFocus\n" );
#endif
	if ( m_pGroup )
	{
		m_pGroup->SetState( "AUTO_LOSEKEYFOCUS" );
	}

	KeyValues *kvEvent = new KeyValues( "OnKeyFocusLost" );
	KeyValues::AutoDelete autodelete( kvEvent );

	
	// chain to main system if this graphic doesn't handle it.
	g_pGameUISystemMgrImpl->OnGameGraphicScriptEvent( this, kvEvent ); 	
}

//-----------------------------------------------------------------------------
// Event that occurs when key focus is gained
//-----------------------------------------------------------------------------
void CHitArea::OnGainKeyFocus()
{
#if ( DEBUG_INPUT_EVENTS )
	Msg( "CHitArea::OnGainKeyFocus\n" );
#endif
	if ( m_pGroup )
	{
		m_pGroup->SetState( "AUTO_GAINKEYFOCUS" );
	}

	KeyValues *kvEvent = new KeyValues( "OnKeyFocusGained" );
	KeyValues::AutoDelete autodelete( kvEvent );

	
	// chain to main system if this graphic doesn't handle it.
	g_pGameUISystemMgrImpl->OnGameGraphicScriptEvent( this, kvEvent );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CHitArea::OnDragStartCallScriptEvent( const int &cursorX, const int &cursorY )
{

}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CHitArea::OnDragCallScriptEvent( const int &cursorX, const int &cursorY )
{

}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CHitArea::OnDragStopCallScriptEvent( const int &cursorX, const int &cursorY )
{

}


//-----------------------------------------------------------------------------
// Handle focus updating on visibility change.
//-----------------------------------------------------------------------------
void CHitArea::SetVisible( bool bVisible )
{ 
	m_Geometry.m_bVisible = bVisible;
	m_bCanAcceptInput = bVisible;
	m_bCanStartDragging = false;
	if ( bVisible )
	{
		g_pGameUISystemMgrImpl->ForceFocusUpdate();
	}
	else
	{
		// Untested
		g_pInputGameUI->GraphicHidden( this );
	}	
}


//-----------------------------------------------------------------------------
// Handle commands from scripting
//-----------------------------------------------------------------------------
KeyValues * CHitArea::HandleScriptCommand( KeyValues *args )
{
	char const *szCommand = args->GetName();

	if ( !Q_stricmp( "SetDragEnabled", szCommand ) )
	{
		m_bDragEnabled = args->GetBool( "dragenabled", 0 );
		return NULL;
	}
	else if ( !Q_stricmp( "SetMouseLeftClickedCommand", szCommand ) )
	{
		m_OnMouseLeftClickedScriptCommand = args->GetString( "command", "" );
		return NULL;
	}


	if ( !Q_stricmp( "RequestFocus", szCommand ) )
	{
		g_pGameUISystemMgrImpl->RequestKeyFocus( this, args );
		return NULL;
	}
	

	return CGameGraphic::HandleScriptCommand( args );
}




