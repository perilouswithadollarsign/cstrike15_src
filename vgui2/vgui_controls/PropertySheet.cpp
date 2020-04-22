//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <vgui/IBorder.h>
#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui/IScheme.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <vgui/KeyCode.h>
#include <keyvalues.h>
#include <vgui/MouseCode.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/Controls.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/ToolWindow.h>
#include <vgui_controls/TextImage.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/PropertyPage.h>
#include "vgui_controls/AnimationController.h"
#include "tier1/fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#ifdef _PS3
//!!BUG!! "wcsicmp is unsupported on PS3"
#ifdef wcsicmp
#undef wcsicmp
#endif
#define wcsicmp wcscmp
#endif

using namespace vgui;

namespace vgui
{

class ContextLabel : public Label
{
	DECLARE_CLASS_SIMPLE( ContextLabel, Label );
public:

	ContextLabel( Button *parent, char const *panelName, char const *text ):
		BaseClass( (Panel *)parent, panelName, text ),
		m_pTabButton( parent )
	{
		SetBlockDragChaining( true );
	}

	virtual void OnMousePressed( MouseCode code )
	{
		if ( m_pTabButton )
		{
			m_pTabButton->FireActionSignal();
		}
	}

	virtual void OnMouseReleased( MouseCode code )
	{
		BaseClass::OnMouseReleased( code );

		if ( GetParent() )
		{
			GetParent()->OnCommand( "ShowContextMenu" );
		}
	}

	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );

		HFont marlett = pScheme->GetFont( "Marlett" );
		SetFont( marlett );
		SetTextInset( 0, 0 );
		SetContentAlignment( Label::a_northwest );

		if ( GetParent() )
		{
			SetFgColor( pScheme->GetColor( "Button.TextColor", GetParent()->GetFgColor() ) );
			SetBgColor( GetParent()->GetBgColor() );
		}
	}
private:

	Button	*m_pTabButton;
};

//-----------------------------------------------------------------------------
// Purpose: Helper for drag drop
// Input  : msglist - 
// Output : static PropertySheet
//-----------------------------------------------------------------------------
static PropertySheet *IsDroppingSheet( CUtlVector< KeyValues * >& msglist )
{
	if ( msglist.Count() == 0 )
		return NULL;

	KeyValues *data = msglist[ 0 ];
	PropertySheet *sheet = reinterpret_cast< PropertySheet * >( data->GetPtr( "propertysheet" ) );
	if ( sheet )
		return sheet;

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: A single tab
//-----------------------------------------------------------------------------
class PageTab : public Button
{
	DECLARE_CLASS_SIMPLE( PageTab, Button );

private:
	bool _active;
	Color _textColor;
	Color _dimTextColor;
	int m_bMaxTabWidth;
	IBorder *m_pActiveBorder;
	IBorder *m_pNormalBorder;
	PropertySheet	*m_pParent;
	Panel			*m_pPage;
	ImagePanel		*m_pImage;
	char			*m_pszImageName;
	bool			m_bShowContextLabel;
	bool			m_bAttemptingDrop;
	ContextLabel	*m_pContextLabel;
	long			m_hoverActivatePageTime;
	long			m_dropHoverTime;
	bool			m_bDragDropStyle;

public:
	PageTab(PropertySheet *parent, const char *panelName, const char *text, char const *imageName, int maxTabWidth, Panel *page, bool showContextButton, long hoverActivatePageTime = -1 ) : 
		Button( (Panel *)parent, panelName, text),
		m_pActiveBorder( NULL ),
		m_pNormalBorder( NULL ),
		m_pParent( parent ),
		m_pPage( page ),
		m_pImage( 0 ),
		m_pszImageName( 0 ),
		m_bShowContextLabel( showContextButton ),
		m_bAttemptingDrop( false ),
		m_hoverActivatePageTime( hoverActivatePageTime ),
		m_dropHoverTime( -1 ),
		m_bDragDropStyle( false )
	{
		SetCommand(new KeyValues("TabPressed"));
		_active = false;
		m_bMaxTabWidth = maxTabWidth;
		SetDropEnabled( true );
		SetDragEnabled( m_pParent->IsDraggableTab() );
		if ( imageName )
		{
			m_pImage = new ImagePanel( this, text );
			int buflen = Q_strlen( imageName ) + 1;
			m_pszImageName = new char[ buflen ];
			Q_strncpy( m_pszImageName, imageName, buflen );

		}
		SetMouseClickEnabled( MOUSE_RIGHT, true );
		m_pContextLabel = m_bShowContextLabel ? new ContextLabel( this, "Context", "9" ) : NULL;

		REGISTER_COLOR_AS_OVERRIDABLE( _textColor, "selectedcolor" );
		REGISTER_COLOR_AS_OVERRIDABLE( _dimTextColor, "unselectedcolor" );
	}

	~PageTab()
	{
		delete[] m_pszImageName;
	}

	void SetDragDropStyle( bool bStyle )
	{
		m_bDragDropStyle = bStyle;
	}

	virtual void Paint()
	{
		BaseClass::Paint();
		if ( !m_bDragDropStyle )
			return;
		int w, h;
		GetSize( w, h );
		surface()->DrawSetColor( m_pParent->GetDropFrameColor() );
		// Top
		surface()->DrawOutlinedRect( 0, 0, w, 2 );
		// surface()->DrawOutlinedRect( 0, 0, w, 1 );
		// Left
		surface()->DrawOutlinedRect( 0, 0, 2, h );
		// Right
		surface()->DrawOutlinedRect( w-2, 0, w, h );
	}

	virtual void OnCursorEntered()
	{
		m_dropHoverTime = system()->GetTimeMillis();
	}

	virtual void OnCursorExited()
	{
		m_dropHoverTime = -1;
	}

	virtual void OnThink()
	{
		if ( m_bAttemptingDrop && m_hoverActivatePageTime >= 0 && m_dropHoverTime >= 0 )
		{
			long hoverTime = system()->GetTimeMillis() - m_dropHoverTime;
			if ( hoverTime > m_hoverActivatePageTime )
			{
				FireActionSignal();
				SetSelected(true);
				Repaint();
			}
		}
		m_bAttemptingDrop = false;

		BaseClass::OnThink();
	}

	virtual bool IsDroppable( CUtlVector< KeyValues * >&msglist )
	{
		m_bAttemptingDrop = true;

		if ( !GetParent() )
			return false;

		PropertySheet *sheet = IsDroppingSheet( msglist );
		if ( sheet )
			return GetParent()->IsDroppable( msglist );

		return BaseClass::IsDroppable( msglist );
	}

	virtual void OnDroppablePanelPaint( CUtlVector< KeyValues * >& msglist, CUtlVector< Panel * >& dragPanels )
	{
		PropertySheet *sheet = IsDroppingSheet( msglist );
		if ( sheet )
		{
			Panel *target = GetParent()->GetDropTarget( msglist );
			if ( target )
			{
			// Fixme, mouse pos could be wrong...
				target->OnDroppablePanelPaint( msglist, dragPanels );
				return;
			}
		}

		// Just highlight the tab if dropping onto active page via the tab
		BaseClass::OnDroppablePanelPaint( msglist, dragPanels );
	}

	virtual void OnPanelDropped( CUtlVector< KeyValues * >& msglist )
	{
		PropertySheet *sheet = IsDroppingSheet( msglist );

		// Msg( "Tab::OnPanelDropped(sheet %s)\n", sheet ? "yes" : "no" );
		if ( sheet )
		{
			Panel *target = GetParent()->GetDropTarget( msglist );
			if ( target )
			{
			// Fixme, mouse pos could be wrong...
				target->OnPanelDropped( msglist );
			}
		}

		// Defer to active page...
		Panel *active = m_pParent->GetActivePage();
		if ( !active || !active->IsDroppable( msglist ) )
			return;

		active->OnPanelDropped( msglist );
	}

	virtual void OnDragFailed( CUtlVector< KeyValues * >& msglist )
	{
		PropertySheet *sheet = IsDroppingSheet( msglist );
		if ( !sheet )
			return;

		// Msg( "Tab::OnDragFailed(sheet %s)\n", sheet ? "yes" : "no" );

		// Create a new property sheet
		if ( m_pParent->IsDraggableTab() )
		{
			if ( msglist.Count() == 1 )
			{
				KeyValues *data = msglist[ 0 ];
                int screenx = data->GetInt( "screenx" );
				int screeny = data->GetInt( "screeny" );

				// m_pParent->ScreenToLocal( screenx, screeny );
				if ( !m_pParent->IsWithin( screenx, screeny ) )
				{
					Panel *page = reinterpret_cast< Panel * >( data->GetPtr( "propertypage" ) );
					PropertySheet *sheet = reinterpret_cast< PropertySheet * >( data->GetPtr( "propertysheet" ) );
					char const *title = data->GetString( "tabname", "" );
					if ( !page || !sheet )
						return;
					
					// Can only create if sheet was part of a ToolWindow derived object
					ToolWindow *tw = dynamic_cast< ToolWindow * >( sheet->GetParent() );
					if ( tw )
					{
						IToolWindowFactory *factory = tw->GetToolWindowFactory();
						if ( factory )
						{
							bool hasContextMenu = sheet->PageHasContextMenu( page );
							sheet->RemovePage( page );
							factory->InstanceToolWindow( tw->GetParent(), sheet->ShouldShowContextButtons(), page, title, hasContextMenu );

							if ( sheet->GetNumPages() == 0 )
							{
								tw->MarkForDeletion();
							}
						}
					}
				}
			}
		}
	}

	virtual HCursor GetDragFailCursor( CUtlVector< KeyValues * >& msglist )
	{
		PropertySheet *sheet = IsDroppingSheet( msglist );
		if ( !sheet )
			return BaseClass::GetDragFailCursor( msglist );

		// Dragging a pagetab somewhere invalid will result in a new toolwindow getting created, which is perfectly valid
		return dc_arrow;
	}

	virtual void OnCreateDragData( KeyValues *msg )
	{
		Assert( m_pParent->IsDraggableTab() );

		msg->SetPtr( "propertypage", m_pPage );
		msg->SetPtr( "propertysheet", m_pParent );
		char sz[ 256 ];
		GetText( sz, sizeof( sz ) );
		msg->SetString( "tabname", sz  );
		msg->SetString( "text", sz );
	}

	virtual void ApplySchemeSettings(IScheme *pScheme)
	{
		// set up the scheme settings
		Button::ApplySchemeSettings(pScheme);

		_textColor = GetSchemeColor("PropertySheet.SelectedTextColor", GetFgColor(), pScheme);
		_dimTextColor = GetSchemeColor("PropertySheet.TextColor", GetFgColor(), pScheme);
		m_pActiveBorder = pScheme->GetBorder("TabActiveBorder");
		m_pNormalBorder = pScheme->GetBorder("TabBorder");

		Resize();

		if ( m_pContextLabel )
		{
			SetTextInset( 12, 0 );
		}
	}

	void Resize()
	{
		if ( m_pImage )
		{
			ClearImages();
			m_pImage->SetImage(scheme()->GetImage(m_pszImageName, false));
			AddImage( m_pImage->GetImage(), 2 );
			int w, h;
			m_pImage->GetSize( w, h );
			w += m_pContextLabel ? 10 : 0;
			if ( m_pContextLabel )
			{
				m_pImage->SetPos( 10, 0 );
			}
			SetSize( w + 4, h + 2 );
		}
		else
		{
			int wide, tall;
			int contentWide, contentTall;
			GetSize(wide, tall);
			GetContentSize(contentWide, contentTall);

			wide = MAX(m_bMaxTabWidth, contentWide + 10);  // 10 = 5 pixels margin on each side
			wide += m_pContextLabel ? 10 : 0;
			SetSize(wide, tall);
		}
	}

	virtual void ApplySettings( KeyValues *inResourceData )
	{
		BaseClass::ApplySettings(inResourceData);

		const char *pBorder = inResourceData->GetString("activeborder_override", "");
		if (*pBorder)
		{
			m_pActiveBorder = scheme()->GetIScheme(GetScheme())->GetBorder( pBorder );
		}
		pBorder = inResourceData->GetString("normalborder_override", "");
		if (*pBorder)
		{
			m_pNormalBorder = scheme()->GetIScheme(GetScheme())->GetBorder( pBorder );
		}
	}

	virtual void OnCommand( char const *cmd )
	{
		if ( !Q_stricmp( cmd, "ShowContextMenu" ) )
		{
			KeyValues *kv = new KeyValues("OpenContextMenu");
			kv->SetPtr( "page", m_pPage );
			kv->SetPtr( "contextlabel", m_pContextLabel );
			PostActionSignal( kv );
			return;
		}
		BaseClass::OnCommand( cmd );		
	}

	IBorder *GetBorder(bool depressed, bool armed, bool selected, bool keyfocus)
	{
		if (_active)
		{
			return m_pActiveBorder;
		}
		return m_pNormalBorder;
	}

	virtual Color GetButtonFgColor()
	{
		if (_active)
		{
			return _textColor;
		}
		else
		{
			return _dimTextColor;
		}
	}

	virtual void SetActive(bool state)
	{
		_active = state;
		SetZPos( state ? 100 : 0 );
		InvalidateLayout();
		Repaint();
	}

	virtual void SetTabWidth( int iWidth )
	{
		m_bMaxTabWidth = iWidth;
		InvalidateLayout();
	}

    virtual bool CanBeDefaultButton(void)
    {
        return false;
    }

	//Fire action signal when mouse is pressed down instead  of on release.
	virtual void OnMousePressed(MouseCode code) 
	{
		// check for context menu open
		if (!IsEnabled())
			return;
		
		if (!IsMouseClickEnabled(code))
			return;
		
		if (IsUseCaptureMouseEnabled())
		{
			{
				RequestFocus();
				FireActionSignal();
				SetSelected(true);
				Repaint();
			}
			
			// lock mouse input to going to this button
			input()->SetMouseCapture(GetVPanel());
		}
	}

	virtual void OnMouseReleased(MouseCode code)
	{
		// ensure mouse capture gets released
		if (IsUseCaptureMouseEnabled())
		{
			input()->SetMouseCapture(NULL);
		}

		// make sure the button gets unselected
		SetSelected(false);
		Repaint();

		if (code == MOUSE_RIGHT)
		{
			KeyValues *kv = new KeyValues("OpenContextMenu");
			kv->SetPtr( "page", m_pPage );
			kv->SetPtr( "contextlabel", m_pContextLabel );
			PostActionSignal( kv );
		}
	}

	virtual void PerformLayout()
	{
		BaseClass::PerformLayout();

		if ( m_pContextLabel )
		{
			int w, h;
			GetSize( w, h );
			m_pContextLabel->SetBounds( 0, 0, 10, h );
		}
	}
};

}; // namespace vgui

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
PropertySheet::PropertySheet(
	Panel *parent, 
	const char *panelName, 
	bool draggableTabs /*= false*/ ) : BaseClass(parent, panelName)
{
	_activePage = NULL;
	_activeTab = NULL;
	_tabWidth = 64;
	_activeTabIndex = 0;
	_showTabs = true;
	_combo = NULL;
    _tabFocus = false;
	m_flPageTransitionEffectTime = 0.0f;
	m_bSmallTabs = false;
	m_tabFont = 0;
	m_bDraggableTabs = draggableTabs;
	m_pTabKV = NULL;
	m_iTabHeight = 0;
    m_iTabHeightSmall = 0;

	if ( m_bDraggableTabs )
	{
		SetDropEnabled( true );
	}

	m_bKBNavigationEnabled = true;
	m_pDragDropTab = new PageTab( this, "dragdroptab", "", NULL, _tabWidth, NULL, false, 0 );
	m_pDragDropTab->SetVisible( false );
	m_pDragDropTab->SetDragDropStyle( true );
	m_nPageDropTabVisibleTime = -1;
	m_pTemporarilyRemoved = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor, associates pages with a combo box
//-----------------------------------------------------------------------------
PropertySheet::PropertySheet(Panel *parent, const char *panelName, ComboBox *combo) : BaseClass(parent, panelName)
{
	_activePage = NULL;
	_activeTab = NULL;
	_tabWidth = 64;
	_activeTabIndex = 0;
	_combo=combo;
	_combo->AddActionSignalTarget(this);
	_showTabs = false;
    _tabFocus = false;
	m_flPageTransitionEffectTime = 0.0f;
	m_bSmallTabs = false;
	m_tabFont = 0;
	m_bDraggableTabs = false;
	m_pTabKV = NULL;
	m_pDragDropTab = new PageTab( this, "dragdroptab", "", NULL, _tabWidth, NULL, false, 0 );
	m_pDragDropTab->SetVisible( false );
	m_pDragDropTab->SetDragDropStyle( true );
	m_nPageDropTabVisibleTime = -1;
	m_pTemporarilyRemoved = NULL;
	m_iTabHeight = 0;
    m_iTabHeightSmall = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
PropertySheet::~PropertySheet()
{
}

//-----------------------------------------------------------------------------
// Purpose: ToolWindow uses this to drag tools from container to container by dragging the tab
// Input  :  - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PropertySheet::IsDraggableTab() const
{
	return m_bDraggableTabs;
}

void PropertySheet::SetDraggableTabs( bool state )
{
	m_bDraggableTabs = state;
}

//-----------------------------------------------------------------------------
// Purpose: Lower profile tabs
// Input  : state - 
//-----------------------------------------------------------------------------
void PropertySheet::SetSmallTabs( bool state )
{
	m_bSmallTabs = state;
	m_tabFont = scheme()->GetIScheme( GetScheme() )->GetFont( m_bSmallTabs ? "DefaultVerySmall" : "Default" );
	int c = m_PageTabs.Count();
	for ( int i = 0; i < c ; ++i )
	{
		PageTab *tab = m_PageTabs[ i ];
		Assert( tab );
		tab->SetFont( m_tabFont );
	}
}

void PropertySheet::SetShowTabs( bool state )
{
	_showTabs = state;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PropertySheet::IsSmallTabs() const
{
	return m_bSmallTabs;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void PropertySheet::ShowContextButtons( bool state )
{
	m_bContextButton = state;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PropertySheet::ShouldShowContextButtons() const
{
	return m_bContextButton;
}

int PropertySheet::FindPage( Panel *page ) const
{
	int c = m_Pages.Count();
	for ( int i = 0; i < c; ++i )
	{
		if ( m_Pages[ i ].page == page )
			return i;
	}

	return m_Pages.InvalidIndex();
}

void PropertySheet::SetPageTitle( Panel *page, const char *title )
{
	if ( !page )
		return;

	int nSlot = FindPage( page );
	if ( nSlot == m_Pages.InvalidIndex() )
		return;

	PageTab *pTab = m_PageTabs[ nSlot ];
	pTab->SetText( title );
}

//-----------------------------------------------------------------------------
// Purpose: adds a page to the sheet
//-----------------------------------------------------------------------------
void PropertySheet::AddPage(Panel *page, const char *title, char const *imageName /*= NULL*/, bool bHasContextMenu /*= false*/, int nInsertBefore /*= -1*/ )
{
	if (!page)
		return;

	// don't add the page if we already have it	 
	int nSlot = FindPage( page );
	if ( nSlot != m_Pages.InvalidIndex() )
	{
		// See about re-ordering
		if ( nInsertBefore != -1 && ( nSlot != nInsertBefore ) )
		{
			if ( nSlot < nInsertBefore )
			{
				--nInsertBefore;
			}

			PageTab *pTab = m_PageTabs[ nSlot ];
			m_PageTabs.Remove( nSlot );
			Page_t pt = m_Pages[ nSlot ];
			m_Pages.Remove( nSlot );
			
			m_PageTabs.InsertBefore( nInsertBefore, pTab );
			m_Pages.InsertBefore( nInsertBefore, pt );

		 	InvalidateLayout();
		}
		return;
	}

	long hoverActivatePageTime = 250;
	PageTab *tab = new PageTab(this, CFmtStr( "tab_%s", title ), title, imageName, _tabWidth, page, m_bContextButton && bHasContextMenu, hoverActivatePageTime );
	if ( m_bDraggableTabs )
	{
		tab->SetDragEnabled( true );
	}

	tab->SetFont( m_tabFont );
	if(_showTabs)
	{
		tab->AddActionSignalTarget(this);
	}
	else if (_combo)
	{
		_combo->AddItem(title, NULL);
	}

	if ( m_pTabKV )
	{
		tab->ApplySettings( m_pTabKV );
	}

	if ( nInsertBefore == -1 )
	{
		nInsertBefore = m_PageTabs.Count();
	}

	m_PageTabs.InsertBefore( nInsertBefore, tab );

	Page_t info;
	info.page = page;
	info.contextMenu = m_bContextButton && bHasContextMenu;
	
	m_Pages.InsertBefore( nInsertBefore, info );

	page->SetParent(this);
	page->AddActionSignalTarget(this);
	PostMessage(page, new KeyValues("ResetData"));

	page->SetVisible(false);
	InvalidateLayout();

	if (!_activePage)
	{
		// first page becomes the active page
		ChangeActiveTab( 0 );
		if ( _activePage )
		{
			_activePage->RequestFocus( 0 );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PropertySheet::SetActivePage(Panel *page)
{
	// walk the list looking for this page
	int index = FindPage( page );
	if (!m_Pages.IsValidIndex(index))
		return;

	ChangeActiveTab(index);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PropertySheet::SetTabWidth(int pixels)
{
	_tabWidth = pixels;

	// Update page tabs with the new width	
	int m = m_PageTabs.Count();
	for (int i=0;i<m;i++)
	{
		m_PageTabs[i]->SetTabWidth( pixels );
	}

	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: reloads the data in all the property page
//-----------------------------------------------------------------------------
void PropertySheet::ResetAllData()
{
	// iterate all the dialogs resetting them
	for (int i = 0; i < m_Pages.Count(); i++)
	{
		ipanel()->SendMessage(m_Pages[i].page->GetVPanel(), new KeyValues("ResetData"), GetVPanel());
	}
}

//-----------------------------------------------------------------------------
// Purpose: Applies any changes made by the dialog
//-----------------------------------------------------------------------------
void PropertySheet::ApplyChanges()
{
	// iterate all the dialogs resetting them
	for (int i = 0; i < m_Pages.Count(); i++)
	{
		ipanel()->SendMessage(m_Pages[i].page->GetVPanel(), new KeyValues("ApplyChanges"), GetVPanel());
	}
}

//-----------------------------------------------------------------------------
// Purpose: gets a pointer to the currently active page
//-----------------------------------------------------------------------------
Panel *PropertySheet::GetActivePage()
{
	return _activePage;
}

//-----------------------------------------------------------------------------
// Purpose: gets a pointer to the currently active tab
//-----------------------------------------------------------------------------
Panel *PropertySheet::GetActiveTab()
{
	return _activeTab;
}

//-----------------------------------------------------------------------------
// Purpose: gets a pointer tab "i"
//-----------------------------------------------------------------------------
Panel* PropertySheet::GetTab(int i)
{
	if (i < 0 && i > m_PageTabs.Count()) 
	{
		return NULL;
	}
	return m_PageTabs[i];
}

//-----------------------------------------------------------------------------
// Purpose: returns the number of panels in the sheet
//-----------------------------------------------------------------------------
int PropertySheet::GetNumPages()
{
	return m_Pages.Count();
}

//-----------------------------------------------------------------------------
// Purpose: returns the name contained in the active tab
// Input  : a text buffer to contain the output 
//-----------------------------------------------------------------------------
void PropertySheet::GetActiveTabTitle(char *textOut, int bufferLen)
{
	if(_activeTab) _activeTab->GetText(textOut, bufferLen);
}

//-----------------------------------------------------------------------------
// Purpose: returns the name contained in the active tab
// Input  : a text buffer to contain the output 
//-----------------------------------------------------------------------------
bool PropertySheet::GetTabTitle(int i, char *textOut, int bufferLen)
{
	if (i < 0 && i > m_PageTabs.Count()) 
	{
		return false;
	}

	m_PageTabs[i]->GetText(textOut, bufferLen);
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the index of the currently active page
//-----------------------------------------------------------------------------
int PropertySheet::GetActivePageNum()
{
	for (int i = 0; i < m_Pages.Count(); i++)
	{
		if (m_Pages[i].page == _activePage) 
		{
			return i;
		}
	}
	return -1;
}

//-----------------------------------------------------------------------------
// Purpose: Forwards focus requests to current active page
//-----------------------------------------------------------------------------
void PropertySheet::RequestFocus(int direction)
{
    if (direction == -1 || direction == 0)
    {
    	if (_activePage)
    	{
    		_activePage->RequestFocus(direction);
            _tabFocus = false;
    	}
    }
    else 
    {
        if (_showTabs && _activeTab)
        {
            _activeTab->RequestFocus(direction);
            _tabFocus = true;
        }
		else if (_activePage)
    	{
    		_activePage->RequestFocus(direction);
            _tabFocus = false;
    	}
    }
}

//-----------------------------------------------------------------------------
// Purpose: moves focus back
//-----------------------------------------------------------------------------
bool PropertySheet::RequestFocusPrev(VPANEL panel)
{
    if (_tabFocus || !_showTabs || !_activeTab)
    {
        _tabFocus = false;
        return BaseClass::RequestFocusPrev(panel);
    }
    else
    {
        if (GetVParent())
        {
            PostMessage(GetVParent(), new KeyValues("FindDefaultButton"));
        }
        _activeTab->RequestFocus(-1);
        _tabFocus = true;
        return true;
    }
}

//-----------------------------------------------------------------------------
// Purpose: moves focus forward
//-----------------------------------------------------------------------------
bool PropertySheet::RequestFocusNext(VPANEL panel)
{
    if (!_tabFocus || !_activePage)
    {
        return BaseClass::RequestFocusNext(panel);
    }
    else
    {
        if (!_activeTab)
        {
            return BaseClass::RequestFocusNext(panel);
        }
        else
        {
            _activePage->RequestFocus(1);
            _tabFocus = false;
            return true;
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose: Gets scheme settings
//-----------------------------------------------------------------------------
void PropertySheet::ApplySchemeSettings(IScheme *pScheme)
{
	// Hack Hack:  This resets m_iTabHeight and m_iTabHeightSmall as if we just initialized the panel
	InternalInitDefaultValues( GetAnimMap() );

	BaseClass::ApplySchemeSettings(pScheme);

	// a little backwards-compatibility with old scheme files
	IBorder *pBorder = pScheme->GetBorder("PropertySheetBorder");
	if (pBorder == pScheme->GetBorder("Default"))
	{
		// get the old name
		pBorder = pScheme->GetBorder("RaisedBorder");
	}

	SetBorder(pBorder);
	m_flPageTransitionEffectTime = atof(pScheme->GetResourceString("PropertySheet.TransitionEffectTime"));

	m_tabFont = pScheme->GetFont( m_bSmallTabs ? "DefaultVerySmall" : "Default" );

	if ( m_pTabKV )
	{
		for (int i = 0; i < m_PageTabs.Count(); i++)
		{
			m_PageTabs[i]->ApplySettings( m_pTabKV );
		}
	}


	//=============================================================================
	// HPE_BEGIN:
	// [tj] Here, we used to use a single size variable and overwrite it when we scaled.
	//		This led to problems when we changes resolutions, so now we recalcuate the absolute 
	//      size from the relative size each time (based on proportionality)
	//=============================================================================
	if ( IsProportional() )
	{
		m_iTabHeight = scheme()->GetProportionalScaledValueEx( GetScheme(), m_iSpecifiedTabHeight );
		m_iTabHeightSmall = scheme()->GetProportionalScaledValueEx( GetScheme(), m_iSpecifiedTabHeightSmall );
	}
	else
	{
		m_iTabHeight = m_iSpecifiedTabHeight;
		m_iTabHeightSmall = m_iSpecifiedTabHeightSmall;
	}
	//=============================================================================
	// HPE_END
	//=============================================================================
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PropertySheet::ApplySettings(KeyValues *inResourceData)
{
	BaseClass::ApplySettings(inResourceData);

	KeyValues *pTabKV = inResourceData->FindKey( "tabskv" );
	if ( pTabKV )
	{
		if ( m_pTabKV )
		{
			m_pTabKV->deleteThis();
		}
		m_pTabKV = new KeyValues("tabkv");
		pTabKV->CopySubkeys( m_pTabKV );
	}

	KeyValues *pTabWidthKV = inResourceData->FindKey( "tabwidth" );
	if ( pTabWidthKV )
	{
		_tabWidth = scheme()->GetProportionalScaledValueEx(GetScheme(), pTabWidthKV->GetInt());
		for (int i = 0; i < m_PageTabs.Count(); i++)
		{
			m_PageTabs[i]->SetTabWidth( _tabWidth );
		}
	}

	KeyValues *pTransitionKV = inResourceData->FindKey( "transition_time" );
	if ( pTransitionKV )
	{
		m_flPageTransitionEffectTime = pTransitionKV->GetFloat();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Paint our border specially, with the tabs in mind
//-----------------------------------------------------------------------------
void PropertySheet::PaintBorder()
{
	IBorder *border = GetBorder();
	if (!border)
		return;

	PageTab *pActive = _activeTab;
	if ( m_PageTabs.Find( m_pDragDropTab ) != m_PageTabs.InvalidIndex() ) 
	{
		pActive = m_pDragDropTab;
	}

	// draw the border, but with a break at the active tab
	int px = 0, py = 0, pwide = 0, ptall = 0;
	if (pActive)
	{
		pActive->GetBounds(px, py, pwide, ptall);
		ptall -= 1;
	}

	// draw the border underneath the buttons, with a break
	int wide, tall;
	GetSize(wide, tall);
	border->Paint(0, py + ptall, wide, tall, IBorder::SIDE_TOP, px + 1, px + pwide - 1);
}

// Grabs mouse coords and figures out best tab to insert "before" or -1 if insert should be at end of tab section row
PageTab *PropertySheet::FindInsertBeforeTab()
{
	int mx, my;
	input()->GetCursorPos( mx, my );
	ScreenToLocal( mx, my );

	int tabHeight = IsSmallTabs() ? m_iTabHeightSmall : m_iTabHeight;
	if ( my < 0 || my > tabHeight )
		return NULL;

	// Walk through existing tabs
	int c = m_PageTabs.Count();
	int nPrevX = 0;
	for ( int i = 0; i < c; ++i )
	{
		if ( m_PageTabs[ i ] == m_pTemporarilyRemoved )
			continue;

		int bounds[ 4 ];
		m_PageTabs[ i ]->GetBounds( bounds[ 0 ], bounds[ 1 ], bounds[ 2 ], bounds[ 3 ] );

		int nMidX = bounds[ 0 ] + ( bounds[ 2 ] * 0.5f );

		if ( mx >= nPrevX && mx <= nMidX )
		{
			if ( m_PageTabs[ i ] == m_pDragDropTab )
			{
				if ( i < c - 1 )
				{
					return m_PageTabs[ i + 1 ];
				}
				else
				{
					return NULL;
				}
			}
			return m_PageTabs[ i ];
		}

		nPrevX = nMidX;
	}

	return NULL;
}

void PropertySheet::LayoutTabs()
{
	int x, y, wide, tall;
	GetBounds(x, y, wide, tall);

	int xtab;
	int limit = m_PageTabs.Count();

	xtab = m_iTabXIndent;

	PageTab *pActive = _activeTab;
	if ( m_PageTabs.Find( m_pDragDropTab ) != m_PageTabs.InvalidIndex() ) 
	{
		pActive = m_pDragDropTab;
	}

	// draw the visible tabs
	if ( _showTabs )
	{
		int tabHeight = IsSmallTabs() ? (m_iTabHeightSmall-1) : (m_iTabHeight-1);

		for (int i = 0; i < limit; i++)
		{
			if ( m_pTemporarilyRemoved == m_PageTabs[ i ] )
			{
				continue;
			}
			int width, tall;
			m_PageTabs[i]->GetSize(width, tall);

			if ( m_bTabFitText )
			{
				m_PageTabs[i]->SizeToContents();
				width = m_PageTabs[i]->GetWide();

				int iXInset, iYInset;
				m_PageTabs[i]->GetTextInset( &iXInset, &iYInset );
				width += (iXInset * 2);
			}
			
			if (m_PageTabs[i] == pActive)
			{
				// active tab is taller
				pActive->SetBounds(xtab, 2, width, tabHeight);
			}
			else
			{
				m_PageTabs[i]->SetBounds(xtab, 4, width, tabHeight - 2);
			}
			m_PageTabs[i]->SetVisible(true);
			xtab += (width + 1) + m_iTabXDelta;
		}
	}
	else
	{
		for (int i = 0; i < limit; i++)
		{
			m_PageTabs[i]->SetVisible(false);
		}
	}

	if ( _activeTab )
	{
		_activeTab->MoveToFront();
		_activeTab->Repaint();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Lays out the dialog
//-----------------------------------------------------------------------------
void PropertySheet::PerformLayout()
{
	BaseClass::PerformLayout();

	int x, y, wide, tall;
	GetBounds(x, y, wide, tall);
	for ( int i = 0; i < m_Pages.Count(); ++ i )
	{
		if ( m_Pages[i].page == _activePage )
		{
			int tabHeight = IsSmallTabs() ? m_iTabHeightSmall : m_iTabHeight;

			if(_showTabs)
			{
				_activePage->SetBounds(0, tabHeight, wide, tall - tabHeight);
			}
			else
			{
				_activePage->SetBounds(0, 0, wide, tall );
			}
			_activePage->InvalidateLayout();
			// ensure draw order (page drawing over all the tabs except one)
			_activePage->MoveToFront();
			_activePage->Repaint();
		}
		else
		{
			m_Pages[i].page->SetVisible( false );
		}
	}

	LayoutTabs();
}

//-----------------------------------------------------------------------------
// Purpose: Switches the active panel
//-----------------------------------------------------------------------------
void PropertySheet::OnTabPressed(Panel *panel)
{
	// look for the tab in the list
	for (int i = 0; i < m_PageTabs.Count(); i++)
	{
		if (m_PageTabs[i] == panel)
		{
			// flip to the new tab
			ChangeActiveTab(i);
			return;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the panel associated with index i
// Input  : the index of the panel to return 
//-----------------------------------------------------------------------------
Panel *PropertySheet::GetPage(int i) 
{
	if( i < 0 || i >= m_Pages.Count() ) 
	{
		return NULL;
	}

	return m_Pages[i].page;
}


//-----------------------------------------------------------------------------
// Purpose: disables page by name
//-----------------------------------------------------------------------------
void PropertySheet::DisablePage(const char *title) 
{
	SetPageEnabled(title, false);
}

//-----------------------------------------------------------------------------
// Purpose: enables page by name
//-----------------------------------------------------------------------------
void PropertySheet::EnablePage(const char *title) 
{
	SetPageEnabled(title, true);
}

//-----------------------------------------------------------------------------
// Purpose: enabled or disables page by name
//-----------------------------------------------------------------------------
void PropertySheet::SetPageEnabled(const char *title, bool state) 
{
	for (int i = 0; i < m_PageTabs.Count(); i++)
	{
		if (_showTabs)
		{
			char tmp[50];
			m_PageTabs[i]->GetText(tmp,50);
			if (!strnicmp(title,tmp,strlen(tmp)))
			{	
				m_PageTabs[i]->SetEnabled(state);
			}
		}
		else if ( _combo )
		{
			_combo->SetItemEnabled(title,state);
		}
	}
}

void PropertySheet::RemoveAllPages()
{
	int c = m_Pages.Count();
	for ( int i = c - 1; i >= 0 ; --i )
	{
		RemovePage( m_Pages[ i ].page );
	}
}

void PropertySheet::DeleteAllPages()
{
	int c = m_Pages.Count();
	for ( int i = c - 1; i >= 0 ; --i )
	{
		DeletePage( m_Pages[ i ].page );
	}
}

//-----------------------------------------------------------------------------
// Purpose: deletes the page associated with panel
// Input  : *panel - the panel of the page to remove
//-----------------------------------------------------------------------------
void PropertySheet::RemovePage(Panel *panel) 
{
	int location = FindPage( panel );
	if ( location == m_Pages.InvalidIndex() )
		return;

	// Since it's being deleted, don't animate!!!
	m_hPreviouslyActivePage = NULL;
	_activeTab = NULL;

	// ASSUMPTION = that the number of pages equals number of tabs
	if( _showTabs )
	{
		m_PageTabs[location]->RemoveActionSignalTarget( this );
	}
	// now remove the tab
	PageTab *tab  = m_PageTabs[ location ];
	m_PageTabs.Remove( location );
	tab->MarkForDeletion();
	
	// Remove from page list
	m_Pages.Remove( location );

	// Unparent
	panel->SetParent( (Panel *)NULL );

	if ( _activePage == panel ) 
	{
		_activePage = NULL;
		// if this page is currently active, backup to the page before this.
		ChangeActiveTab( MAX( location - 1, 0 ) ); 
	}

	PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: deletes the page associated with panel
// Input  : *panel - the panel of the page to remove
//-----------------------------------------------------------------------------
void PropertySheet::DeletePage(Panel *panel) 
{
	Assert( panel );
	RemovePage( panel );
	panel->MarkForDeletion();
}

//-----------------------------------------------------------------------------
// Purpose: flips to the new tab, sending out all the right notifications
//			flipping to a tab activates the tab.
//-----------------------------------------------------------------------------
void PropertySheet::ChangeActiveTab( int index )
{
	if ( !m_Pages.IsValidIndex( index ) )
	{
		_activeTab = NULL;
		if ( m_Pages.Count() > 0 )
		{
			_activePage = NULL;
			ChangeActiveTab( 0 );
		}
		return;
	}

	if ( m_Pages[index].page == _activePage )
	{
		if ( _activeTab )
		{
			_activeTab->RequestFocus();
		}
		_tabFocus = true;
		return;
	}

	int c = m_Pages.Count();
	for ( int i = 0; i < c; ++i )
	{
		m_Pages[ i ].page->SetVisible( false );
	}

	m_hPreviouslyActivePage = _activePage;

	// We're going to fade out the active page, so don't hide it yet
	if (_activePage && m_flPageTransitionEffectTime)
	{
		_activePage->SetVisible(true);
	}

	// notify old page
	if (_activePage)
	{
		ivgui()->PostMessage(_activePage->GetVPanel(), new KeyValues("PageHide"), GetVPanel());
		KeyValues *msg = new KeyValues("PageTabActivated");
		msg->SetPtr("panel", (Panel *)NULL);
		ivgui()->PostMessage(_activePage->GetVPanel(), msg, GetVPanel());
	}
	if (_activeTab)
	{
		//_activeTabIndex=index;
		_activeTab->SetActive(false);

		// does the old tab have the focus?
		_tabFocus = _activeTab->HasFocus();
	}
	else
	{
		_tabFocus = false;
	}

	// flip page
	_activePage = m_Pages[index].page;
	_activeTab = m_PageTabs[index];
	_activeTabIndex = index;

	_activePage->SetVisible(true);
	_activePage->MoveToFront();
	
	_activeTab->SetVisible(true);
	_activeTab->MoveToFront();
	_activeTab->SetActive(true);

	if (_tabFocus)
	{
		// if a tab already has focused,give the new tab the focus
		_activeTab->RequestFocus();
	}
	else
	{
		// otherwise, give the focus to the page
		_activePage->RequestFocus();
	}

	if (!_showTabs && _combo)
	{
		_combo->ActivateItemByRow(index);
	}

	_activePage->MakeReadyForUse();

	// transition effect
	if (m_flPageTransitionEffectTime)
	{
		if (m_hPreviouslyActivePage.Get())
		{
			// fade out the previous page
			GetAnimationController()->RunAnimationCommand(m_hPreviouslyActivePage, "Alpha", 0.0f, 0.0f, m_flPageTransitionEffectTime / 2, AnimationController::INTERPOLATOR_LINEAR);
		}

		// fade in the new page
		_activePage->SetAlpha(0);
		GetAnimationController()->RunAnimationCommand(_activePage, "Alpha", 255.0f, m_flPageTransitionEffectTime / 2, m_flPageTransitionEffectTime / 2, AnimationController::INTERPOLATOR_LINEAR);
	}
	else
	{
		if (m_hPreviouslyActivePage.Get())
		{
			// no transition, just hide the previous page
			m_hPreviouslyActivePage->SetVisible(false);
		}
		_activePage->SetAlpha( 255 );
	}

	// notify
	ivgui()->PostMessage(_activePage->GetVPanel(), new KeyValues("PageShow"), GetVPanel());

	KeyValues *msg = new KeyValues("PageTabActivated");
	msg->SetPtr("panel", (Panel *)_activeTab);
	ivgui()->PostMessage(_activePage->GetVPanel(), msg, GetVPanel());

	// tell parent
	PostActionSignal(new KeyValues("PageChanged"));

	// Repaint
	InvalidateLayout();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Gets the panel with the specified hotkey, from the current page
//-----------------------------------------------------------------------------
Panel *PropertySheet::HasHotkey(wchar_t key)
{
	if (!_activePage)
		return NULL;

	for (int i = 0; i < _activePage->GetChildCount(); i++)
	{
		Panel *hot = _activePage->GetChild(i)->HasHotkey(key);
		if (hot)
		{
			return hot;
		}
	}
	
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: catches the opencontextmenu event
//-----------------------------------------------------------------------------
void PropertySheet::OnOpenContextMenu( KeyValues *params )
{
	// tell parent
	KeyValues *kv = params->MakeCopy();
	PostActionSignal( kv );
	Panel *page = reinterpret_cast< Panel * >( params->GetPtr( "page" ) );
	if ( page )
	{
		PostMessage( page->GetVPanel(), params->MakeCopy() );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Handle key presses, through tabs.
//-----------------------------------------------------------------------------
void PropertySheet::OnKeyCodeTyped(KeyCode code)
{
	bool shift = (input()->IsKeyDown(KEY_LSHIFT) || input()->IsKeyDown(KEY_RSHIFT));
	bool ctrl = (input()->IsKeyDown(KEY_LCONTROL) || input()->IsKeyDown(KEY_RCONTROL));
	bool alt = (input()->IsKeyDown(KEY_LALT) || input()->IsKeyDown(KEY_RALT));
	
	if ( ctrl && shift && alt && code == KEY_B)
	{
		// enable build mode
		EditablePanel *ep = dynamic_cast< EditablePanel * >( GetActivePage() );
		if ( ep )
		{
			ep->ActivateBuildMode();
			return;
		}
	}

	if ( IsKBNavigationEnabled() )
	{
		switch (code)
		{
			// for now left and right arrows just open or close submenus if they are there.
		case KEY_RIGHT:
			{
				ChangeActiveTab(_activeTabIndex+1);
				break;
			}
		case KEY_LEFT:
			{
				ChangeActiveTab(_activeTabIndex-1);
				break;
			}
		default:
			BaseClass::OnKeyCodeTyped(code);
			break;
		}
	}
	else
	{
		BaseClass::OnKeyCodeTyped(code);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called by the associated combo box (if in that mode), changes the current panel
//-----------------------------------------------------------------------------
void PropertySheet::OnTextChanged(Panel *panel,const wchar_t *wszText)
{
	if ( panel == _combo )
	{
		wchar_t tabText[30];
		for(int i = 0 ; i < m_PageTabs.Count() ; i++ )
		{
			tabText[0] = 0;
			m_PageTabs[i]->GetText(tabText,30);
			if ( !wcsicmp(wszText,tabText) )
			{
				ChangeActiveTab(i);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PropertySheet::OnCommand(const char *command)
{
    // propogate the close command to our parent
	if (!stricmp(command, "Close") && GetVParent())
    {
		CallParentFunction(new KeyValues("Command", "command", command));
    }
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PropertySheet::OnApplyButtonEnable()
{
	// tell parent
	PostActionSignal(new KeyValues("ApplyButtonEnable"));	
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PropertySheet::OnCurrentDefaultButtonSet( vgui::VPANEL defaultButton )
{
	// forward the message up
	if (GetVParent())
	{
		KeyValues *msg = new KeyValues("CurrentDefaultButtonSet");
		msg->SetInt("button", ivgui()->PanelToHandle( defaultButton ) );
		PostMessage(GetVParent(), msg);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PropertySheet::OnDefaultButtonSet( VPANEL defaultButton )
{
	// forward the message up
	if (GetVParent())
	{
		KeyValues *msg = new KeyValues("DefaultButtonSet");
		msg->SetInt("button", ivgui()->PanelToHandle( defaultButton ) );
		PostMessage(GetVParent(), msg);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PropertySheet::OnFindDefaultButton()
{
    if (GetVParent())
    {
        PostMessage(GetVParent(), new KeyValues("FindDefaultButton"));
    }
}

bool PropertySheet::PageHasContextMenu( Panel *page ) const
{
	int pageNum = FindPage( page );
	if ( pageNum == m_Pages.InvalidIndex() )
		return false;

	return m_Pages[ pageNum ].contextMenu;
}

void PropertySheet::OnPanelDropped( CUtlVector< KeyValues * >& msglist )
{
	PageTab *pBefore = FindInsertBeforeTab();

	ClearPageDropTab();

	if ( msglist.Count() != 1 )
	{
		return;
	}

	PropertySheet *sheet = IsDroppingSheet( msglist );

	// Msg( "PropertySheet::OnPanelDropped(sheet %s)\n", sheet ? "yes" : "no" );

	if ( !sheet )
	{
		// Defer to active page
		if ( _activePage && _activePage->IsDropEnabled() )
		{
			_activePage->OnPanelDropped( msglist );
		}
		return;
	}

	KeyValues *data = msglist[ 0 ];

	Panel *page = reinterpret_cast< Panel * >( data->GetPtr( "propertypage" ) );
	char const *title = data->GetString( "tabname", "" );
	if ( !page || !sheet )
		return;

	// Can only create if sheet was part of a ToolWindow derived object
	ToolWindow *tw = dynamic_cast< ToolWindow * >( sheet->GetParent() );
	if ( tw )
	{
		IToolWindowFactory *factory = tw->GetToolWindowFactory();
		if ( factory )
		{
			bool showContext = sheet->PageHasContextMenu( page );
			sheet->RemovePage( page );
			if ( sheet->GetNumPages() == 0 )
			{
				tw->MarkForDeletion();
			}

			int nSlot = pBefore ? m_PageTabs.Find( pBefore ) : -1;

			AddPage( page, title, NULL, showContext, nSlot );
			SetActivePage( page );
		}
	}
}

bool PropertySheet::IsDroppable( CUtlVector< KeyValues * >& msglist )
{
	if ( !m_bDraggableTabs )
		return false;

	if ( msglist.Count() != 1 )
	{
		return false;
	}

	int mx, my;
	input()->GetCursorPos( mx, my );
	ScreenToLocal( mx, my );

	int tabHeight = IsSmallTabs() ? m_iTabHeightSmall : m_iTabHeight;
	if ( my > tabHeight )
		return false;

	PropertySheet *sheet = IsDroppingSheet( msglist );
	if ( !sheet )
	{
		return false;
	}

	KeyValues *data = msglist[ 0 ];
	char const *text = data->GetString( "tabname", "" );
	Panel *page = reinterpret_cast< Panel * >( data->GetPtr( "propertypage" ) );
	if ( !page )
		return false;

	AddPageDropTab( text, page );

	return true;
}

// Mouse is now over a droppable panel
void PropertySheet::OnDroppablePanelPaint( CUtlVector< KeyValues * >& msglist, CUtlVector< Panel * >& dragPanels )
{
	if ( m_nPageDropTabVisibleTime != -1 )
	{
		// Extend timeout
		m_nPageDropTabVisibleTime = system()->GetTimeMillis();
	}

	// Convert this panel's bounds to screen space
	int x, y, w, h;
	GetSize( w, h );

	int tabHeight = IsSmallTabs() ? m_iTabHeightSmall : m_iTabHeight;
	h = tabHeight + 4;

	x = y = 0;
	LocalToScreen( x, y );

	Color clr;
	clr = GetDropFrameColor();

	surface()->DrawSetColor( clr );
	// Draw 2 pixel frame
	surface()->DrawOutlinedRect( x-1, y-1, x + w+1, y + h+1 );

	clr[ 3 ] *= 0.5f;
	surface()->DrawSetColor( clr );
	surface()->DrawOutlinedRect( x, y, x + w, y + h );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : state - 
//-----------------------------------------------------------------------------
void PropertySheet::SetKBNavigationEnabled( bool state )
{
	m_bKBNavigationEnabled = state;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool PropertySheet::IsKBNavigationEnabled() const
{
	return m_bKBNavigationEnabled;
}

void PropertySheet::AddPageDropTab( char const *pTabName, Panel *pPage )
{
	//if ( m_nPageDropTabVisibleTime != -1 )
	//	return;

	m_PageTabs.FindAndRemove( m_pDragDropTab );

	m_pDragDropTab->SetText( pTabName );
	m_pDragDropTab->Resize();
	m_pDragDropTab->SetVisible( true );

	int nPageSlot = FindPage( pPage );
	if ( nPageSlot != -1 )
	{
		m_pTemporarilyRemoved = m_PageTabs[ nPageSlot ];
		m_pTemporarilyRemoved->SetVisible( false );
		LayoutTabs();
	}

	PageTab *pBefore = FindInsertBeforeTab();
	int nSlot = pBefore ? m_PageTabs.Find( pBefore ) : -1;

	// Msg( "Add tab %s at slot %d\n", pTabName, nSlot );

	if ( nSlot == -1 )
	{
		m_PageTabs.AddToTail( m_pDragDropTab ); 
	}
	else
	{
		m_PageTabs.InsertBefore( nSlot, m_pDragDropTab ); 
	}
	LayoutTabs();
	m_nPageDropTabVisibleTime = system()->GetTimeMillis();
}

void PropertySheet::ClearPageDropTab()
{
	// Msg( "Clearing page drop tab\n" );
	m_nPageDropTabVisibleTime = -1;
	m_PageTabs.FindAndRemove( m_pDragDropTab );
	m_pDragDropTab->SetVisible( false );
	if ( m_pTemporarilyRemoved )
	{
		m_pTemporarilyRemoved->SetVisible( true );
		m_pTemporarilyRemoved = NULL;
	}
	LayoutTabs();
}

void PropertySheet::OnThink()
{
	BaseClass::OnThink();
	if ( m_nPageDropTabVisibleTime == -1 )
		return;
	if ( system()->GetTimeMillis() <= ( m_nPageDropTabVisibleTime + 100 ) )
		return;
	ClearPageDropTab();
}
