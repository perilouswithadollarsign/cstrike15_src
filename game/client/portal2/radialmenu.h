// RadialMenu.h
// Copyright (c) 2006 Turtle Rock Studios, Inc.

#ifndef RADIALMENU_H
#define RADIALMENU_H
#ifdef _WIN32
#pragma once
#endif

#include "viewport_panel_names.h"
#include <vgui_controls/frame.h>
#include <game/client/iviewport.h>
#include <vgui_controls/EditablePanel.h>
#include "hudelement.h"

class CRadialButton;
class C_Portal_Player;

enum RadialMenuTypes_t
{
	MENU_PING = 0,
	MENU_TAUNT,
	MENU_PLAYTEST,
	MENU_COUNT
};

//--------------------------------------------------------------------------------------------------------
/**
* Helper class for managing our list of possible menus.  This means we can load RadialMenu.txt,
* and add in additional menus from other text files (UserRadialMenu.txt?)
*/
class ClientMenuManager
{
public:
	ClientMenuManager()
	{
		m_menuKeys = NULL;
	}

	virtual ~ClientMenuManager()
	{
		Reset();
	}

	void AddMenuFile( const char *filename );
	virtual KeyValues *FindMenu( const char *menuName );
	virtual void Flush( void );

	void PrintStats( void )
	{
		DevMsg( "Client menus:\n" );
		for ( KeyValues *pKey = m_menuKeys->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey() )
		{
			DevMsg( "\t%s\n", pKey->GetName() );
		}
	}

protected:
	void Reset( void )
	{
		if ( m_menuKeys )
		{
			m_menuKeys->deleteThis();
		}
		m_menuKeys = NULL;
	}

	KeyValues *m_menuKeys;
};


class ClientMenuManagerPlaytest	: public ClientMenuManager
{
public:
	ClientMenuManagerPlaytest() {}

	virtual ~ClientMenuManagerPlaytest() {}

	virtual KeyValues *FindMenu( const char *menuName );
	virtual void Flush( void );
};


class CRadialMenuPanel : public vgui::Frame, public IViewPortPanel
{
private:
	DECLARE_CLASS_SIMPLE( CRadialMenuPanel, vgui::Frame );

public:
	CRadialMenuPanel(IViewPort *pViewPort);
	virtual ~CRadialMenuPanel() {}

	virtual const char *GetName( void ) { return PANEL_RADIAL_MENU; }
	virtual void SetData(KeyValues *data) {};
	virtual void Reset() {}
	virtual void Update() {}
	virtual bool NeedsUpdate( void ) { return false; }
	virtual bool HasInputElements( void ) { return true; }
	virtual void ShowPanel( bool bShow );
	virtual bool WantsBackgroundBlurred( void ) { return false; }

	// both vgui::Frame and IViewPortPanel define these, so explicitly define them here as passthroughs to vgui
	vgui::VPANEL GetVPanel( void ) { return BaseClass::GetVPanel(); }
	virtual bool IsVisible() { return BaseClass::IsVisible(); }
	virtual void SetParent( vgui::VPANEL parent ) 
	{ 
		BaseClass::SetParent( parent );
	}

protected:
	IViewPort	*m_pViewPort;
};


//--------------------------------------------------------------------------------------------------------
/**
 * Viewport panel that gives us a simple rosetta menu
 */
class CRadialMenu : public CHudElement, public vgui::EditablePanel
{
public:
	DECLARE_CLASS_SIMPLE( CRadialMenu, vgui::EditablePanel );

	CRadialMenu( const char *pElementName );
	virtual ~CRadialMenu();

	enum ButtonDir
	{
		CENTER = 0,
		NORTH,
		NORTH_EAST,
		EAST,
		SOUTH_EAST,
		SOUTH,
		SOUTH_WEST,
		WEST,
		NORTH_WEST,
		NUM_BUTTON_DIRS
	};

	virtual void SetData( KeyValues *data );
	//virtual KeyValues *GetData( void ) { return NULL; }
	bool		IsFading( void ) { return IsVisible() && m_fading; }
	void		StartFade( void );
	void		ChooseArmedButton();
	void		SetArmedButtonDir( ButtonDir dir );

	void OnRadialMenuOpen( void );

	virtual bool ShouldDraw() { return m_bEnabled; }
	virtual void ShowPanel( bool bShow );
	virtual void OnMousePressed( vgui::MouseCode code );
	virtual void OnMouseReleased( vgui::MouseCode code );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PerformLayout( void );
	virtual void PaintBackground( void );
	virtual void Paint( void );
	virtual void OnCommand( const char *command );
	virtual vgui::Panel *CreateControlByName( const char *controlName );
	virtual void OnThink( void );
	virtual void OnTick( void );
	virtual int	KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );

	void OnCursorEnteredButton( int x, int y, CRadialButton *button );

	// For loading our internal data while using this menu
	void SetTargetEntity( EHANDLE hTargetEntity ) { m_hTargetEntity = hTargetEntity; }
	void SetTraceData( const Vector &vPosition, const Vector &vNormal ) { m_vPosition = vPosition; m_vNormal = vNormal; }

	void SetRadialType( RadialMenuTypes_t menuType ) { m_menuType = menuType; }
	void SetFadeInTime( float fFadeInTime ) { m_fFadeInTime = fFadeInTime; }
	void ClearLockInTime( void ) { m_fSelectionLockInTime = 0.0f; }
	void SetDemoCursorPos( int cursorX, int cursorY ) { m_demoCursorX = cursorX; m_demoCursorY = cursorY; }

	ButtonDir GetArmedDir( void ) const { return m_armedButtonDir; }

	bool m_bFirstCentering;

	static float m_fLastPingTime[ MAX_SPLITSCREEN_PLAYERS ][ 2 ];
	static int m_nNumPings[ MAX_SPLITSCREEN_PLAYERS ][ 2 ];

	void SetRadialMenuEnabled( bool bEnable ) { m_bEnabled = bEnable; }
	void SetQuickPingForceClose( bool bEnable ) { m_bQuickPingForceClose = bEnable; }

protected:
	void ClearGlowEntity( void );

	void SendCommand( const char *commandStr );
	const char *ButtonNameFromDir( ButtonDir dir );
	ButtonDir DirFromButtonName( const char * name );
	CRadialButton *m_buttons[NUM_BUTTON_DIRS];	// same order as vgui::Label::Alignment
	ButtonDir m_armedButtonDir;
	void UpdateButtonBounds( void );

	void HandleControlSettings();

	void StartDrag( void );
	void EndDrag( void );

	CPanelAnimationVar( vgui::HFont, m_hCustomizeFont, "CustomizeFont", "Default" );

	bool m_fading;
	float m_fadeStart;

	Color m_lineColor;

	KeyValues *m_resource;
	KeyValues *m_menuData;

	int m_nArrowTexture;

	int m_minButtonX;
	int m_minButtonY;
	int m_maxButtonX;
	int m_maxButtonY;

	// Internal data for holding our polling information
	Vector	m_vPosition;
	Vector	m_vNormal;
	EHANDLE	m_hTargetEntity;
	int		m_nEntityGlowIndex;

	RadialMenuTypes_t m_menuType;
	bool m_bEditing;
	bool m_bDragging;
	int m_nDraggingTaunt;

	float m_fFadeInTime;
	float m_fSelectionLockInTime;

	bool m_bEnabled;
	int m_cursorX;
	int m_cursorY;

	int m_demoCursorX;
	int m_demoCursorY;

	bool m_bQuickPingForceClose;
};

bool IsRadialMenuOpen( void );


#endif // RADIALMENU_H
