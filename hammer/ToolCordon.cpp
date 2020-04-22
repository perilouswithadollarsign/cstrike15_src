//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Implements the cordon tool. The cordon tool defines a rectangular
//			volume that acts as a visibility filter. Only objects that intersect
//			the cordon are rendered in the views. When saving the MAP file while
//			the cordon tool is active, only brushes that intersect the cordon
//			bounds are saved. The cordon box is replaced by brushes in order to
//			seal the map.
//
//=============================================================================//

#include "stdafx.h"
#include "ChunkFile.h"
#include "ToolCordon.h"
#include "History.h"
#include "GlobalFunctions.h"
#include "MainFrm.h"
#include "MapDoc.h"
#include "MapDefs.h"
#include "MapSolid.h"
#include "MapView2D.h"
#include "MapView3D.h"
#include "MapWorld.h"
#include "render2d.h"
#include "StatusBarIDs.h"
#include "ToolManager.h"
#include "Options.h"
#include "WorldSize.h"
#include "vgui/Cursor.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


Vector Cordon3D::m_vecLastMins;	// Last mins & maxs the user dragged out with this tool;
Vector Cordon3D::m_vecLastMaxs;	// used to fill in the third axis when starting a new box.



//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
Cordon3D::Cordon3D(void)
{
	SetDrawColors(RGB(0, 255, 255), RGB(255, 0, 0));
	m_vecLastMins.Init( COORD_NOTINIT, COORD_NOTINIT, COORD_NOTINIT );
	m_vecLastMaxs.Init( COORD_NOTINIT, COORD_NOTINIT, COORD_NOTINIT );
}


//-----------------------------------------------------------------------------
// Purpose: Called when the tool is activated.
// Input  : eOldTool - The ID of the previously active tool.
//-----------------------------------------------------------------------------
void Cordon3D::OnActivate()
{
	RefreshToolState();
}


//-----------------------------------------------------------------------------
// Fetch data from the document and update our internal state.
//-----------------------------------------------------------------------------
void Cordon3D::RefreshToolState()
{
	Vector mins( COORD_NOTINIT, COORD_NOTINIT, COORD_NOTINIT );
	Vector maxs( COORD_NOTINIT, COORD_NOTINIT, COORD_NOTINIT );
	
	if ( m_pDocument->Cordon_GetCount() > 0 )
	{
		m_pDocument->Cordon_GetEditCordon( mins, maxs );
		m_vecLastMins = mins;
		m_vecLastMaxs = maxs;
	}

	SetBounds( mins, maxs );
	m_bEmpty = !IsValidBox();
	EnableHandles( true );
}


//-----------------------------------------------------------------------------
// Return true of the boxes intersect (but not if they just touch).
//-----------------------------------------------------------------------------
inline bool BoxesIntersect2D( const Vector2D &vBox1Min, const Vector2D &vBox1Max, const Vector2D &vBox2Min, const Vector2D &vBox2Max )
{
	return ( vBox1Min.x < vBox2Max.x ) && ( vBox1Max.x > vBox2Min.x ) &&
		   ( vBox1Min.y < vBox2Max.y ) && ( vBox1Max.y > vBox2Min.y );
}


//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button down events in the 2D view.
// Input  : Per CWnd::OnLButtonDown.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Cordon3D::OnLMouseDown2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint)
{
	Tool3D::OnLMouseDown2D(pView, nFlags, vPoint);

	Vector vecWorld;
	pView->ClientToWorld(vecWorld, vPoint);

	unsigned int uConstraints = GetConstraints( nFlags );

	if ( HitTest(pView, vPoint, true) )
	{
		StartTranslation( pView, vPoint, m_LastHitTestHandle );
	}
	else
	{
		//	
		// Test against all active cordons
		//
		CMapDoc *pDoc = pView->GetMapDoc();
		if ( !pDoc )
			return true;

		// Make a little box around the cursor to test against.
		const int iSelUnits = 2;
		Vector2D selMins( vPoint.x - iSelUnits, vPoint.y - iSelUnits );
		Vector2D selMaxs( vPoint.x + iSelUnits, vPoint.y + iSelUnits );

		for ( int i = 0; i < pDoc->Cordon_GetCount(); i++ )
		{
			Cordon_t *cordon = pDoc->Cordon_GetCordon( i );
			if ( !cordon->m_bActive )
				continue;
			
			for ( int j = 0; j < cordon->m_Boxes.Count(); j++ )
			{
				BoundBox *box = &cordon->m_Boxes[j];
				
				Vector2D vecClientMins;
				Vector2D vecClientMaxs;
				pView->WorldToClient( vecClientMins, box->bmins );
				pView->WorldToClient( vecClientMaxs, box->bmaxs );
				
				// 2D projection can flip Y
				NormalizeBox( vecClientMins, vecClientMaxs );
				
				if ( BoxesIntersect2D( vecClientMins, vecClientMaxs, selMins, selMaxs ) )
				{
					pDoc->Cordon_SelectCordonForEditing( cordon, box, SELECT_CORDON_FROM_TOOL );
					RefreshToolState();
					return true;
				}
			}
		}
		
		// getvisiblepoint fills in any coord that's still set to COORD_NOTINIT:
		vecWorld[pView->axThird] = m_vecLastMins[pView->axThird];
		m_pDocument->GetBestVisiblePoint(vecWorld);

		// snap starting position to grid
		if ( uConstraints & constrainSnap )
			m_pDocument->Snap(vecWorld,uConstraints);
			
		// Create a new box 
		// Add it to the current edit cordon
		// Set the edit cordon to current edit cordon and the new box
		Cordon_t *cordon = m_pDocument->Cordon_GetSelectedCordonForEditing();
		BoundBox *box = NULL;
		if ( !cordon )
		{
			// No cordon, we need a new one.
			cordon = m_pDocument->Cordon_CreateNewCordon( DEFAULT_CORDON_NAME, &box );
		}
		else
		{
			// Just add a box to the current cordon.
			box = m_pDocument->Cordon_AddBox( cordon );
		}
		
		box->bmins = box->bmaxs = vecWorld;
		
		Vector vecSize( 0, 0, 0 );
		if ( ( m_vecLastMins[pView->axThird] == COORD_NOTINIT ) || ( m_vecLastMaxs[pView->axThird] == COORD_NOTINIT ) )
		{
			vecSize[pView->axThird] = pDoc->GetGridSpacing();
		}
		else
		{
			vecSize[pView->axThird] = m_vecLastMaxs[pView->axThird] - m_vecLastMins[pView->axThird];
		}
		StartNew( pView, vPoint, vecWorld, vecSize );

		m_pDocument->Cordon_SelectCordonForEditing( cordon, box, SELECT_CORDON_FROM_TOOL );
		
		if ( pDoc->Cordon_IsCordoning() )
		{
			pDoc->UpdateVisibilityAll();
		}

		pDoc->SetModifiedFlag( true );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles left mouse button up events in the 2D view.
// Input  : Per CWnd::OnLButtonUp.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Cordon3D::OnLMouseUp2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	bool bShift = ( nFlags & MK_SHIFT ) != 0;

	Tool3D::OnLMouseUp2D(pView, nFlags, vPoint) ;

	if ( IsTranslating() )
	{
		if ( bShift )
		{
		}
		
		FinishTranslation( true );
		
		if ( bShift )
		{
			// Clone the selected cordon
			Cordon_t *cordon = m_pDocument->Cordon_GetSelectedCordonForEditing();

			BoundBox *box = m_pDocument->Cordon_AddBox( cordon );
			box->bmins = bmins;
			box->bmaxs = bmaxs;

			m_pDocument->Cordon_SelectCordonForEditing( cordon, box, SELECT_CORDON_FROM_TOOL );
			RefreshToolState();
		}
		else
		{
			m_pDocument->Cordon_SetEditCordon( bmins, bmaxs );
			
			// Remember these bounds for the next time we start dragging out a new cordon.
			m_vecLastMins = bmins;
			m_vecLastMaxs = bmaxs;
		}
	}

	m_pDocument->UpdateStatusbar();
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles mouse move events in the 2D view.
// Input  : Per CWnd::OnMouseMove.
// Output : Returns true if the message was handled, false if not.
//-----------------------------------------------------------------------------
bool Cordon3D::OnMouseMove2D(CMapView2D *pView, UINT nFlags, const Vector2D &vPoint) 
{
	vgui::HCursor hCursor = vgui::dc_arrow;

	Tool3D::OnMouseMove2D(pView, nFlags, vPoint) ;

	unsigned int uConstraints = GetConstraints( nFlags );
					    
	// Convert to world coords.
	Vector vecWorld;
	pView->ClientToWorld(vecWorld, vPoint);
	
	// Update status bar position display.
	//
	char szBuf[128];

	m_pDocument->Snap(vecWorld,uConstraints);

	sprintf(szBuf, " @%.0f, %.0f ", vecWorld[pView->axHorz], vecWorld[pView->axVert]);
	SetStatusText(SBI_COORDS, szBuf);
	
	if ( IsTranslating() )
	{
		// cursor is cross here
		Tool3D::UpdateTranslation( pView, vPoint, uConstraints );
		
		hCursor = vgui::dc_none;
	}
	else if ( HitTest(pView, vPoint, true) )
	{
		hCursor = UpdateCursor( pView, m_LastHitTestHandle, m_TranslateMode );
	}
	
	if ( hCursor != vgui::dc_none  )
		pView->SetCursor( hCursor );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Handles the escape key in the 2D or 3D views.
//-----------------------------------------------------------------------------
void Cordon3D::OnEscape(void)
{
	if ( IsTranslating() )
	{
		FinishTranslation( false );
	}
	else
	{
		m_pDocument->GetTools()->SetTool(TOOL_POINTER);
	}
}


//-----------------------------------------------------------------------------
// Deletes the currently selected cordon in response to the user hitting DELETE
//-----------------------------------------------------------------------------
void Cordon3D::OnDelete()
{
	BoundBox *pBox = NULL;
	Cordon_t *pCordon = m_pDocument->Cordon_GetSelectedCordonForEditing( &pBox );
	if ( !pCordon || !pBox )
		return;
	
	if ( !pBox || ( pCordon->m_Boxes.Count() <= 1 ) )
	{
		m_pDocument->Cordon_RemoveCordon( pCordon );
	}
	else
	{
		m_pDocument->Cordon_RemoveBox( pCordon, pBox );
	}
	
	m_pDocument->UpdateVisibilityAll();
	m_pDocument->SetModifiedFlag( true );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool Cordon3D::OnKeyDown2D(CMapView2D *pView, UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if (nChar == VK_ESCAPE)
	{
		OnEscape();
		return true;
	}
	
	if ( nChar == VK_DELETE )
	{
		OnDelete();
	}

	return false;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool Cordon3D::OnKeyDown3D(CMapView3D *pView, UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	if (nChar == VK_ESCAPE)
	{
		OnEscape();
		return true;
	}

	if ( nChar == VK_DELETE )
	{
		OnDelete();
	}

	return false;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void Cordon3D::RenderTool2D( CRender2D *pRender )
{
	pRender->PushRenderMode( RENDER_MODE_DOTTED );
	pRender->SetDrawColor( 255, 255, 0 );

	// If cordoning is active, the document's rendering code handles drawing the cordons.
	if ( !m_pDocument->Cordon_IsCordoning() )
	{
		int nCount = m_pDocument->Cordon_GetCount();	
		for ( int i = 0; i < nCount; i++ )
		{
			Cordon_t *pCordon = m_pDocument->Cordon_GetCordon( i );
			if ( pCordon->m_bActive )
			{
				for ( int j = 0; j < pCordon->m_Boxes.Count(); j++ )
				{
					// draw simple rectangle
					pRender->DrawRectangle( pCordon->m_Boxes[j].bmins, pCordon->m_Boxes[j].bmaxs, false, 0 );
				}
			}
		}
	}
	
	pRender->PopRenderMode();

	BaseClass::RenderTool2D( pRender );
}
