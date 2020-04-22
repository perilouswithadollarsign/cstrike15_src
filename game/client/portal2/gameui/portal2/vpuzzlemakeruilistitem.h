//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#ifndef __VPUZZLEMAKERUILISTITEM_H__
#define __VPUZZLEMAKERUILISTITEM_H__

#include "basemodui.h"

#if defined( PORTAL2_PUZZLEMAKER )

using namespace vgui;
using namespace BaseModUI;


namespace BaseModUI {

	class GenericPanelList;
	class CPuzzleMakerUIPanel;

	//=============================================================================
	class CPuzzleMakerUIListItem : public vgui::EditablePanel
	{
		DECLARE_CLASS_SIMPLE( CPuzzleMakerUIListItem, vgui::EditablePanel );
	public:
		CPuzzleMakerUIListItem( vgui::Panel *pParent, const char *pPanelName );

		bool IsSelected( void ) const { return m_bSelected; }
		void SetSelected( bool bSelected ) { m_bSelected = bSelected; }

		bool HasMouseover( void ) const { return m_bHasMouseover; }
		void SetHasMouseover( bool bHasMouseover );
		void OnKeyCodePressed( vgui::KeyCode code );

		virtual void NavigateTo();
		virtual void NavigateFrom();

		virtual void SetPrimaryText( const char *pText );
		const char *GetPrimaryText( void ) const { return m_szPrimaryLabelText; }

	protected:
		virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
		virtual void PaintBackground();
		virtual void OnCursorEntered();
		virtual void OnCursorExited() { SetHasMouseover( false ); }

		virtual void OnMousePressed( vgui::MouseCode code );
		virtual void OnMouseDoublePressed( vgui::MouseCode code );
		void PerformLayout();

		virtual void DrawListItemLabel( Label *pLabel );

		Color							m_BaseColor;
		Color							m_TextColor;
		Color							m_FocusColor;
		Color							m_DisabledColor;
		Color							m_CursorColor;
		Color							m_LockedColor;
		Color							m_MouseOverCursorColor;
		Color							m_LostFocusColor;

		Label							*m_pLblName;
		char							m_szPrimaryLabelText[ MAX_MAP_NAME ];

	private:

		CPuzzleMakerUIPanel				*m_pParentPanel;
		GenericPanelList				*m_pListCtrlr;

		bool							m_bSelected;
		bool							m_bHasMouseover;

		/*vgui::HFont						m_hTextFont;
		vgui::HFont						m_hFriendsListFont;
		vgui::HFont						m_hFriendsListSmallFont;
		vgui::HFont						m_hFriendsListVerySmallFont;*/

		
	};

};

#endif // PORTAL2_PUZZLEMAKER

#endif // __VPUZZLEMAKERUILISTITEM_H__