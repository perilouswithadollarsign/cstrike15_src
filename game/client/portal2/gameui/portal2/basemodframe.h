
//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __BASEMODFRAME_H__
#define __BASEMODFRAME_H__

#include "vgui_controls/Panel.h"
#include "vgui_controls/Frame.h"
#include "tier1/utllinkedlist.h"

#include "basemodpanel.h"

class CUniformRandomStream;

namespace BaseModUI {

	class BaseModHybridButton;

	struct DialogMetrics_t
	{
		int titleY;
		int titleHeight;
		int dialogY;
		int dialogHeight;
	};

	enum MenuTiles_e
	{
		MT_TOP_LEFT = 0,
		MT_TOP,
		MT_TOP_RIGHT,
		MT_LEFT,
		MT_INTERIOR,
		MT_INTERIOR_ALT,
		MT_RIGHT,
		MT_BOTTOM_LEFT,
		MT_BOTTOM,
		MT_BOTTOM_RIGHT,
		MAX_MENU_TILES
	};

	enum DialogStyle_e
	{
		DS_SIMPLE = 1,
		DS_CONFIRMATION = 2,
		DS_WAITSCREEN = 3,
		DS_CUSTOMTITLE = 4,
	};

	//=============================================================================
	//
	//=============================================================================
	class IBaseModFrameListener
	{
	public:
		virtual void RunFrame() = 0;
	};

	//=============================================================================
	//
	//=============================================================================
	class CBaseModFrame : public vgui::Frame
	{
		DECLARE_CLASS_SIMPLE( CBaseModFrame, vgui::Frame );

	public:
		CBaseModFrame( vgui::Panel *parent, const char *panelName, bool okButtonEnabled = true, 
			bool cancelButtonEnabled = true, bool imgBloodSplatterEnabled = true, bool doButtonEnabled = true );
		virtual ~CBaseModFrame();

		virtual void SetTitle(const char *title, bool surfaceTitle);
		virtual void SetTitle(const wchar_t *title, bool surfaceTitle);
		
		virtual void SetDataSettings( KeyValues *pSettings );

		virtual void LoadLayout();

		void ReloadSettings();

		virtual void OnKeyCodePressed(vgui::KeyCode code);
#ifndef _GAMECONSOLE
		virtual void OnKeyCodeTyped( vgui::KeyCode code );
#endif
		virtual void OnMousePressed( vgui::MouseCode code );

		virtual void OnOpen();
		virtual void OnClose();
		virtual void OnThink();
		virtual void Activate();
		virtual void OnCommand( char const *szCommand );

		virtual vgui::Panel *NavigateBack();
		CBaseModFrame *SetNavBack( CBaseModFrame* navBack );
		CBaseModFrame *GetNavBack() { return m_NavBack.Get(); }	
		bool CanNavBack() { return m_bCanNavBack; }

		virtual void PostChildPaint();
		virtual void PaintBackground();

		virtual void FindAndSetActiveControl();

		virtual void RunFrame();

		// Load the control settings 
		virtual void LoadControlSettings( const char *dialogResourceName, const char *pathID = NULL, KeyValues *pPreloadedKeyValues = NULL, KeyValues *pConditions = NULL );

		MESSAGE_FUNC_CHARPTR( OnNavigateTo, "OnNavigateTo", panelName );

		static void AddFrameListener( IBaseModFrameListener * frameListener );
		static void RemoveFrameListener( IBaseModFrameListener * frameListener );
		static void RunFrameOnListeners();

		virtual bool GetFooterEnabled();

		void CloseWithoutFade();

		void ToggleTitleSafeBorder();

		void PushModalInputFocus();				// Makes this panel take modal input focus and maintains stack of previous panels with focus.  For PC only.
		void PopModalInputFocus();				// Removes modal input focus and gives focus to previous panel on stack. For PC only.

		bool CheckAndDisplayErrorIfNotLoggedIn();	// Displays error if not logged into Steam (no-op on X360)

		void DrawGenericBackground();
		void DrawDialogBackground( const char *pMajor = NULL, const wchar_t *pMajorFormatted = NULL, const char *pMinor = NULL, const wchar_t *pMinorFormatted = NULL, DialogMetrics_t *pMetrics = NULL, bool bAllCapsTitle = false, int iTitleXOffset = INT_MAX );
		void SetupAsDialogStyle();
		void SetDialogTitle( const char *pTitle = NULL, const wchar_t *pTitleFormatted = NULL, bool bShowController = false, int nTilesWide = 0, int nTilesTall = 0, int nTitleTilesWide = 0 );
		void SetDialogSubTitle( const char *pTitle = NULL, const wchar_t *pTitleFormatted = NULL, bool bShowController = false, int nTilesWide = 0, int nTilesTall = 0, int nTitleTilesWide = 0 );
		void GetDialogTileSize( int &nTileWidth, int &nTileHeight );

		void SetUseAlternateTiles( bool bUseAlternate ) { m_bUseAlternateTiles = bUseAlternate; };
		bool UsesAlternateTiles( void ) const { return m_bUseAlternateTiles; }

		static char const * GetEntityOverMouseCursorInEngine();

		virtual KeyValues * GetResourceLoadConditions() { return m_pResourceLoadConditions; }
		
		WINDOW_TYPE GetWindowType();
		WINDOW_PRIORITY GetWindowPriority();

		void RestoreFocusToActiveControl();

	protected:
		virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
		virtual void ApplySettings(KeyValues *inResourceData);
		virtual void PerformLayout();

		virtual void PostApplySettings();
		virtual void CreateVirtualUiControls();
		virtual void ExecuteCommandForEntity( char const *szEntityName );
		virtual void GetEntityNameForControl( char const *szControlId, char chEntityName[256] );
		virtual void ResolveEntityName( char const *szEntityName, char chEntityName[256] );

		void SetOkButtonEnabled( bool bEnabled );
		void SetCancelButtonEnabled( bool bEnabled );
		void SetUpperGarnishEnabled( bool bEnabled );
		void SetFooterEnabled( bool bEnabled );

		void DrawControllerIndicator();

	protected:
		static CUtlVector< IBaseModFrameListener * > m_FrameListeners;
		static bool m_DrawTitleSafeBorder;

		vgui::Panel* m_ActiveControl;

		vgui::Label* m_LblTitle;

		bool m_FooterEnabled;

		bool m_OkButtonEnabled;
		bool m_CancelButtonEnabled;
		bool m_bLayoutLoaded;
		bool m_bIsFullScreen;
		bool m_bDelayPushModalInputFocus;		// set to true if we need to consider taking modal input focus, but can't do it yet

		char m_ResourceName[64];
		KeyValues *m_pResourceLoadConditions;

	private:
		friend class CBaseModPanel;

		void SetWindowType(WINDOW_TYPE windowType);
		void SetWindowPriority( WINDOW_PRIORITY pri );

		void SetCanBeActiveWindowType(bool allowed);
		bool GetCanBeActiveWindowType();

		WINDOW_TYPE m_WindowType;
		WINDOW_PRIORITY m_WindowPriority;
		bool m_CanBeActiveWindowType;
		vgui::DHANDLE<CBaseModFrame> m_NavBack;			// panel to nav back to
		bool m_bCanNavBack;							// can we nav back: use this to distinguish between "no panel set" vs "panel has gone away" for nav back
		CUtlVector<vgui::HPanel> m_vecModalInputFocusStack;

		int				m_nDialogStyle;

		vgui::HFont		m_hTitleFont;
		wchar_t			m_TitleString[256];

		vgui::HFont		m_hSubTitleFont;
		wchar_t			m_SubTitleString[256];
		Color			m_SubTitleColor;
		Color			m_SubTitleColorAlt;
		int				m_nSubTitleWide;
		int				m_nSubTitleTall;

		int				m_nTitleWide;
		int				m_nTitleTall;
		int				m_nTitleOffsetX;
		int				m_nTitleOffsetY;
		Color			m_TitleColor;
		Color			m_TitleColorAlt;
		Color			m_MessageBoxTitleColor;
		Color			m_MessageBoxTitleColorAlt;

		int				m_nTileWidth;
		int				m_nTileHeight;

		int				m_nOriginalTall;
		int				m_nOriginalWide;

		int				m_nTilesWide;
		int				m_nTilesTall;
		int				m_nTitleTilesWide;

		int				m_nPinFromBottom;
		int				m_nPinFromLeft;
		int				m_nFooterOffsetY;

		bool			m_bUseAlternateTiles;
		int				m_nTileImageId[MAX_MENU_TILES];
		int				m_nAltTileImageId[MAX_MENU_TILES];
		int				m_nAltRandomTileImageId[2];
		int				m_nAltSeed;
		CUniformRandomStream	m_RandomStream;
	
		bool			m_bLayoutFixed;

		vgui::HFont		m_hButtonFont;
		bool			m_bShowController;

	protected:
		CPanelAnimationVarAliasType( int, m_iHeaderY, "header_y", "19", "proportional_int" );
		CPanelAnimationVarAliasType( int, m_iHeaderTall, "header_tall", "41", "proportional_int" );
		CPanelAnimationVarAliasType( int, m_iTitleXOffset, "header_title_x", "180", "proportional_int" );		// pixels left of center

	protected:
		// Virtual UI Settings
		KeyValues *m_pVuiSettings;
		CUtlVector< BaseModHybridButton * > m_arrVirtualUiControls;
	};
};

class CBackgroundMapActiveControlManager
{
public:
	CBackgroundMapActiveControlManager();

public:
	void NavigateToEntity( char const *szEntity );
	void Reset();
	void Apply();

protected:
	char m_chLastActiveEntity[256];
};

extern CBackgroundMapActiveControlManager g_BackgroundMapActiveControlManager;

#endif
