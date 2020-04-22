//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HITAREA_H
#define HITAREA_H

#ifdef _WIN32
#pragma once
#endif

#include "gamegraphic.h"
#include "dmxloader/dmxelement.h"
#include "tier1/utlvector.h"
#include "vstdlib/ieventsystem.h"



//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
class CHitArea : public CGameGraphic
{
	DECLARE_DMXELEMENT_UNPACK()

public:

	CHitArea( const char *pName );
	virtual ~CHitArea(); 

	bool Unserialize( CDmxElement *pGraphic );

	// Update geometry and execute scripting.
	virtual void UpdateGeometry();
	virtual void UpdateRenderData( color32 parentColor, CUtlVector< RenderGeometryList_t > &renderGeometryLists, int firstListIndex );

	virtual bool HitTest( int x, int y );

	// Cursor events
	// AUTO_GAINMOUSEFOCUS
	void OnCursorEnter();
	// AUTO_LOSEMOUSEFOCUS
	void OnCursorExit();
	void OnCursorMove( const int &cursorX, const int &cursorY );

	// Mouse events
	// AUTO_MOUSELEFTDOWN 
	// AUTO_MOUSERIGHTDOWN
	// AUTO_MOUSEMIDDLEDOWN
	void OnMouseDown( const ButtonCode_t &code );
	// AUTO_MOUSELEFTUP
	// AUTO_MOUSERIGHTUP
	// AUTO_MOUSEMIDDLEUP
	void OnMouseUp( const ButtonCode_t &code, bool bFireScripts = true );
	// AUTO_MOUSEDOUBLECLICK
	void OnMouseDoubleClick( const ButtonCode_t &code );
	void OnMouseWheel( const int &delta ){}

	// Keyboard events
	// AUTO_KEYDOWN
	void OnKeyDown( const ButtonCode_t &code );
	// AUTO_KEYUP
	void OnKeyUp( const ButtonCode_t &code );
	void OnKeyCodeTyped( const ButtonCode_t &code );
	void OnKeyTyped( const wchar_t &unichar );

	// AUTO_GAINKEYFOCUS
	void OnGainKeyFocus();
	// AUTO_LOSEKEYFOCUS
	void OnLoseKeyFocus();

	// Calls to scripting.
	void OnDragStartCallScriptEvent( const int &cursorX, const int &cursorY );
	void OnDragCallScriptEvent( const int &cursorX, const int &cursorY );
	void OnDragStopCallScriptEvent( const int &cursorX, const int &cursorY );

	virtual void SetVisible( bool bVisible );

	virtual bool IsHitArea() const { return true; }

	virtual KeyValues *HandleScriptCommand( KeyValues *args );

private:
	CHitArea();

	CUtlVector< Vector2D > m_ScreenPositions;

	bool	  m_bDragEnabled;
	bool      m_bCanStartDragging;
	bool      m_IsDragging;

	int       m_DragStartCursorPos[2];
	int       m_DragCurrentCursorPos[2];

	CUtlString m_OnMouseLeftClickedScriptCommand;

};


#endif // HITAREA_H
