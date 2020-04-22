//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VCHALLENGEMODEDIALOG_H__
#define __VCHALLENGEMODEDIALOG_H__

#include "basemodui.h"

namespace BaseModUI {

class GenericPanelList;
class CChallengeModeDialog;

class CMapListItem : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CMapListItem, vgui::EditablePanel );
public:
	CMapListItem( vgui::Panel *pParent, const char *pPanelName );

	void SetChapterIndex( int nIndex, bool bMapEntry = false );
	void SetMapIndex( int nIndex );

	bool IsSelected( void ) { return m_bSelected; }
	void SetSelected( bool bSelected ) { m_bSelected = bSelected; }

	bool HasMouseover( void ) { return m_bHasMouseover; }
	void SetHasMouseover( bool bHasMouseover );
	void OnKeyCodePressed( vgui::KeyCode code );

	bool IsLocked( void ) { return m_bLocked; }

	int GetChapterIndex() { return m_nChapterIndex; }
	int GetMapIndex() { return m_nMapIndex; }

protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PaintBackground();
	virtual void OnCursorEntered();
	virtual void OnCursorExited() { SetHasMouseover( false ); }
	virtual void NavigateTo();
	virtual void NavigateFrom();
	//virtual vgui::Panel* NavigateRight();

	void OnMousePressed( vgui::MouseCode code );
	void OnMouseDoublePressed( vgui::MouseCode code );
	void PerformLayout();

private:
	void DrawListItemLabel( vgui::Label *pLabel );
	bool ActivateSelectedItem();

	CChallengeModeDialog	*m_pDialog;

	GenericPanelList	*m_pListCtrlr;
	vgui::HFont			m_hTextFont;
	int					m_nChapterIndex;
	int					m_nMapIndex;

	Color				m_TextColor;
	Color				m_FocusColor;
	Color				m_DisabledColor;
	Color				m_CursorColor;
	Color				m_LockedColor;
	Color				m_MouseOverCursorColor;

	Color				m_LostFocusColor;

	bool				m_bSelected;
	bool				m_bHasMouseover;
	bool				m_bLocked;

	int					m_nTextOffsetY;
};

class CChallengeModeDialog : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( CChallengeModeDialog, CBaseModFrame );

public:
	CChallengeModeDialog( vgui::Panel *pParent, const char *pPanelName );
	~CChallengeModeDialog();

	int GetNumAllowedChapters() { return m_nAllowedChapters; }
	int GetCurrentChapter() { return m_nChapterIndex + 1; }
	int GetCurrentMap() { return m_nMapIndex + 1; }
	bool InLeaderboards( ) { return m_bLeaderboards; }

	MESSAGE_FUNC_CHARPTR( OnItemSelected, "OnItemSelected", pPanelName );

protected:
	virtual void OnCommand( char const *pCommand );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Activate();
	virtual void PaintBackground();
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual vgui::Panel* NavigateRight();
	virtual vgui::Panel* NavigateLeft();
	virtual void SetDataSettings( KeyValues *pSettings );

private:
	void UpdateFooter();

	//bool				m_bIsCommentaryDialog;

	int					m_nAllowedChapters;

	bool				m_bDrawAsLocked;

	bool				m_bLeaderboards;

	int					m_nChapterIndex;
	int					m_nMapIndex;

	bool				m_bMapListActive;

	GenericPanelList	*m_pChapterList;
	GenericPanelList	*m_pMapList;
};

};

#endif
