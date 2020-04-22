//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//==========================================================================//

#ifndef __VQUICKPLAY_H__
#define __VQUICKPLAY_H__

#include "basemodui.h"
#include "vgui_controls/ImagePanel.h"
#include "VGenericPanelList.h"

#define NUM_QUICKPLAY_ENUMERATION_TYPES	7

namespace BaseModUI {

	class BaseModHybridButton;

	//=============================================================================
	// 
	// Community map item
	//
	//-----------------------------------------------------------------------------

	class EnumerationTypeItem : public vgui::EditablePanel
	{
		DECLARE_CLASS_SIMPLE( EnumerationTypeItem, vgui::EditablePanel );

	public:
		EnumerationTypeItem( vgui::Panel *pParent, const char *pPanelName );

		bool IsSelected() { return m_bSelected; }
		void SetSelected( bool bSelected ) { m_bSelected = bSelected; }

		bool HasMouseover() { return m_bHasMouseover; }

		void SetHasMouseover( bool bHasMouseover )
		{
			if ( bHasMouseover )
			{
				for ( int i = 0; i < m_pListCtrlr->GetPanelItemCount(); i++ )
				{
					EnumerationTypeItem *pItem = dynamic_cast< EnumerationTypeItem* >( m_pListCtrlr->GetPanelItem( i ) );
					if ( pItem && pItem != this )
					{
						pItem->SetHasMouseover( false );
					}
				}
			}
			m_bHasMouseover = bHasMouseover; 
		}

		void	SetTitle( const wchar_t *lpszTitle );
		void	SetEnumerationType( EWorkshopEnumerationType enumType ) { m_EnumerationType = enumType; }

		virtual void	OnKeyCodePressed( vgui::KeyCode code );
		bool			OnKeyCodePressed_Queue( vgui::KeyCode code );
		bool			OnKeyCodePressed_History( vgui::KeyCode code );

		void SetDisabled( bool bState ) { m_bDisabled = bState; }

	protected:
		virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
		virtual void PaintBackground();
		virtual void OnCursorEntered();
		virtual void OnCursorExited();
		virtual void NavigateTo();
		virtual void NavigateFrom();
		virtual void OnMousePressed( vgui::MouseCode code );
		virtual void OnMouseDoublePressed( vgui::MouseCode code );
		virtual void PerformLayout();
		virtual void OnMessage( const KeyValues *params, vgui::VPANEL ifromPanel );

	private:
		int DrawText( int x, int y, int nLabelTall, const wchar_t *pString, vgui::HFont hFont, Color color );

		GenericPanelList	*m_pListCtrlr;

		vgui::HFont			m_hTitleFont;

		int					m_nTextOffsetY;

		Color				m_TextColor;
		Color				m_FocusColor;
		Color				m_CursorColor;
		Color				m_MouseOverCursorColor;
		Color				m_DisabledColor;

		bool				m_bSelected;
		bool				m_bHasMouseover;
		bool				m_bDisabled;

		wchar_t						m_TitleString[128];		
		EWorkshopEnumerationType	m_EnumerationType;
	};

	class CQuickPlay : public CBaseModFrame, public IBaseModFrameListener
	{
		DECLARE_CLASS_SIMPLE( CQuickPlay, CBaseModFrame );

	public:
		CQuickPlay( vgui::Panel *parent, const char *panelName );
		~CQuickPlay();

		MESSAGE_FUNC_CHARPTR( FindQuickPlayMapsAborted, "FindQuickPlayMapsAborted", msg );
		MESSAGE_FUNC_CHARPTR( FindQuickPlayMapsComplete, "FindQuickPlayMapsComplete", msg );
		MESSAGE_FUNC_CHARPTR( FindQuickPlayMapsFailed, "FindQuickPlayMapsFailed", msg );
		MESSAGE_FUNC_CHARPTR( FindQuickPlayMapsError, "FindQuickPlayMapsError", msg );
		MESSAGE_FUNC_INT( EnumerationTypeSelected, "EnumerationTypeSelected", type );
		MESSAGE_FUNC_INT( LaunchRequested, "LaunchRequested", type );

	protected:
		virtual void Activate();
		virtual void ApplySchemeSettings( vgui::IScheme* pScheme );
		virtual void OnKeyCodePressed(vgui::KeyCode code);
		virtual void OnCommand( const char *command );
		virtual void RunFrame( void );

	private:

#if !defined( _GAMECONSOLE )
		void	SetQuickPlaySelection( EWorkshopEnumerationType eType );
#endif

		void	Reset( void );
		void	UpdateFooter( void );
		void	AttemptLaunchQuickPlayMap( void );
		void	LaunchQuickPlayMap( PublishedFileId_t unFileID );
		void	StartEnumeration( void );
		void	BeginQuickPlayEnumeration( void );

		GenericPanelList			*m_pEnumerationTypesList;
		BaseModHybridButton			*m_pQuickPlayTypeButtons[NUM_QUICKPLAY_ENUMERATION_TYPES];
		vgui::Label					*m_pQuickPlayDescription;
		vgui::ImagePanel			*m_pQuickPlayIcon;

		enum QuickPlayState_t
		{
			IDLE,
			QUERIED_FOR_QUICK_PLAY_MAPS,
			WAITING_FOR_ENUMERATION,
			ATTEMPTING_TO_LAUNCH_MAP
		};

		QuickPlayState_t			m_eState;
		float						m_flStartupTime;
	};

};

#endif // __VQUICKPLAY_H__
