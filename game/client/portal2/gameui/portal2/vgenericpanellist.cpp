//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "VGenericPanelList.h"

#include "vgui_controls/Label.h"
#include "vgui_controls/Panel.h"
#include "vgui_controls/ScrollBar.h"
#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "vgui/IImage.h"
#include "vgui/ILocalize.h"
#include "vgui_controls/ImagePanel.h"
#include "KeyValues.h"
#include "VFlyoutMenu.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;


//just passes all navigation up to it's parent
class Panel_PassNavigationToParent : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( Panel_PassNavigationToParent, vgui::Panel );
public:
	Panel_PassNavigationToParent( vgui::Panel *pParent ) : vgui::Panel( pParent ) { }

	virtual Panel* NavigateUp( void )
	{
		return GetParent()->NavigateUp();
	}
	virtual Panel* NavigateDown( void )
	{
		return GetParent()->NavigateDown();
	}
	virtual Panel* NavigateRight( void )
	{
		return GetParent()->NavigateRight();
	}
	virtual Panel* NavigateLeft( void )
	{
		return GetParent()->NavigateLeft();
	}
	virtual void NavigateToChild( Panel *pNavigateTo )
	{
		return GetParent()->NavigateToChild( pNavigateTo );
	}
};

//=============================================================================
GenericPanelList::GenericPanelList(vgui::Panel *parent, const char *panelName, int selectionModeMask ):
BaseClass( parent, panelName ),
m_ItemSelectionModeMask( selectionModeMask ),
m_LastItemAdded( 0 )
{
	m_SchemeBgColorName[0] = '\0';
	m_CurrentSelectedItem = NULL;
	m_PanelItemBorder = vgui::scheme()->GetProportionalScaledValueEx( GetScheme(), 4 );
	m_bShowScrollProgress = false;

	m_PnlItemRegion = new Panel_PassNavigationToParent( this );

	m_ScrVerticalScroll = new ScrollBar( this, "ScrVerticalScroll", true );
	m_ScrVerticalScroll->AddActionSignalTarget(this);
	m_ScrVerticalScroll->SetVisible( false );

	m_LblDownArrow = new Label( this, "LblDownArrow", "#GameUI_Icons_DOWN_ARROW" );
	m_LblUpArrow = new Label( this, "LblUpArrow", "#GameUI_Icons_UP_ARROW" );
	m_LblScrollProgress = new Label( this, "LblScrollProgress", "" );
	m_pItemNavigationChangedCallback = NULL;
}

//=============================================================================
GenericPanelList::~GenericPanelList()
{
	delete m_ScrVerticalScroll;
	delete m_PnlItemRegion;
	delete m_LblDownArrow;
	delete m_LblUpArrow;

	// only delete panels that have not already been deleted by the Panel base class destructor
	for( int i = 0; i < m_PanelItems.Count(); ++i )
	{
		for( int j = 0; j < GetChildCount(); ++j )
		{
			if( GetChild( j ) == m_PanelItems[i] )
			{
				m_PanelItems[i]->DeletePanel();
			}
		}
	}

	m_CurrentSelectedItem = NULL;
}


//=============================================================================
void GenericPanelList::OnKeyCodePressed( KeyCode code )
{
	bool itemSelected = false;

	switch( GetBaseButtonCode( code ) )
	{
	case KEY_XSTICK1_DOWN:
	case KEY_XSTICK2_DOWN:
	case KEY_XBUTTON_DOWN:
	case KEY_DOWN:
		if( !s_NavLock && ( m_ItemSelectionModeMask & GenericPanelList::ISM_PERITEM ) )
		{
			s_NavLock = 1;
			// CBaseModPanel::GetSingleton().PlayUISound( UISOUND_FOCUS );
			if( !m_CurrentSelectedItem && m_PanelItems.Count() > 0 )
			{
				itemSelected = SelectPanelItem( 0, GenericPanelList::SD_DOWN );
			}
			else if( m_CurrentSelectedItem != 0 )
			{
				// if we are at the bottom of the list, navigate to the next control
				if( m_CurrentSelectedItem == m_PanelItems[m_PanelItems.Count() - 1] )
				{
#ifdef _GAMECONSOLE
					if( GetNavDown() != 0 )
					{
						BaseClass::OnKeyCodePressed( code );
					}
					else if ( m_bWrap )
#endif // _GAMECONSOLE
					{
						SelectPanelItem( 0, GenericPanelList::SD_DOWN );
					}
				}
				else // otherwise navigate to the next item in the list
				{
					unsigned short index = 0;
					GetPanelItemIndex( m_CurrentSelectedItem, index );
					itemSelected = SelectPanelItem( index + 1, GenericPanelList::SD_DOWN );
				}

				UpdatePanels();
			}
			
			if( !itemSelected )
			{
				BaseClass::OnKeyCodePressed( code );
			}
		}
		else if ( !s_NavLock && ( m_ItemSelectionModeMask & GenericPanelList::ISM_ELEVATOR ) )
		{
			ElevatorScroll( false );
		}
		break;
	case KEY_XSTICK1_UP:
	case KEY_XSTICK2_UP:
	case KEY_XBUTTON_UP:
	case KEY_UP:
		if( !s_NavLock && ( m_ItemSelectionModeMask & GenericPanelList::ISM_PERITEM ) )
		{
			s_NavLock = 1;
			// CBaseModPanel::GetSingleton().PlayUISound( UISOUND_FOCUS );
			if( !m_CurrentSelectedItem && m_PanelItems.Count() > 0 )
			{
				itemSelected = SelectPanelItem( m_PanelItems.Count() - 1, GenericPanelList::SD_UP );
			}
			else if( m_CurrentSelectedItem != 0 )
			{
				// if we are at the top of the list, navigate to the next control
				if( m_CurrentSelectedItem == m_PanelItems[0] )
				{
#ifdef _GAMECONSOLE
					if( GetNavUp() != 0 )
					{
						BaseClass::OnKeyCodePressed( code );
					}
					else if ( m_bWrap )
#endif // _GAMECONSOLE
					{
						SelectPanelItem( m_PanelItems.Count() - 1, GenericPanelList::SD_UP );
					}
				}
				else // otherwise navigate to the next item in the list
				{
					unsigned short index = 0;
					GetPanelItemIndex( m_CurrentSelectedItem, index );
					itemSelected = SelectPanelItem( index - 1, GenericPanelList::SD_UP );
				}

				UpdatePanels();
			}
			
			if( !itemSelected )
			{
				BaseClass::OnKeyCodePressed( code );
			}
		}
		else if ( !s_NavLock && ( m_ItemSelectionModeMask & GenericPanelList::ISM_ELEVATOR ) )
		{
			this->ElevatorScroll( true );
		}
		break;

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}

//=============================================================================
bool GenericPanelList::RemovePanelItem( unsigned short index, bool bDeletePanel /*= true */ )
{
	if( index < m_PanelItems.Count() )
	{
		bool selectNew = false;
		if( m_CurrentSelectedItem == m_PanelItems[index] )
		{
			selectNew = true;
		}

		if( selectNew )
		{
			if ( index == 0 || !SelectPanelItem( index - 1, GenericPanelList::SD_UP, true, false ) )
			{
				if ( index == ( m_PanelItems.Count() - 1 ) || !SelectPanelItem( index + 1, GenericPanelList::SD_DOWN, true, false ) )
				{
					m_CurrentSelectedItem = NULL;
				}
			}
		}

		RelinkNavigation();

		if ( bDeletePanel )
		{
			m_PanelItems[index]->DeletePanel( );
		}

		m_PanelItems.Remove( index );

		InvalidateLayout( );

		CallParentFunction( new KeyValues( "OnItemRemoved", "panelName", GetName() ) );

		return true;
	}

	return false;
}

//=============================================================================
void GenericPanelList::RemoveAllPanelItems( )
{
	for( unsigned short i = 0; i < m_PanelItems.Count(); ++i )
	{
		m_PanelItems[i]->DeletePanel( );
	}

	m_PanelItems.RemoveAll( );
	m_CurrentSelectedItem = NULL;

	InvalidateLayout( );
}

//=============================================================================
Panel* GenericPanelList::GetSelectedPanelItem( )
{
	return m_CurrentSelectedItem;
}

//=============================================================================
unsigned short GenericPanelList::GetPanelItemCount()
{
	return m_PanelItems.Count();
}

//=============================================================================
Panel* GenericPanelList::GetPanelItem( unsigned short index )
{
	if( index < m_PanelItems.Count() )
	{
		return m_PanelItems[index];
	}

	return 0;
}

//=============================================================================
bool GenericPanelList::GetPanelItemIndex( vgui::Panel* panelItem, unsigned short& panelItemIndex )
{
	for( int i = 0; i < m_PanelItems.Count(); ++i )
	{
		if( panelItem == m_PanelItems[i] )
		{
			panelItemIndex = i;
			return true;
		}
	}

	return false;
}

//=============================================================================
bool GenericPanelList::SelectPanelItem( unsigned short index, SEARCH_DIRECTION direction, bool scrollToItem, bool bAllowStealFocus, bool bSuppressSelectionSound )
{
	bool itemSelected = false;

	if( index < m_PanelItems.Count() )
	{
		Panel* previousNav = m_CurrentSelectedItem;

		if( ( m_PanelItems[index] != 0 ) && ( m_PanelItems[index] != previousNav ) && ( m_PanelItems[index]->IsEnabled() ) )
		{
			itemSelected = true;

			if ( m_CurrentSelectedItem ) 
			{
				PostMessage( m_CurrentSelectedItem->GetVPanel(), new KeyValues("PanelUnSelected") );
			}

			m_CurrentSelectedItem = m_PanelItems[index];
			m_CurrentSelectedItem->SetVisible( true );

			//mouse input could have set any panel item as navigated to.
			for( int i = 0; i != m_PanelItems.Count(); ++i )
			{
				if( i != index )
					m_PanelItems[i]->NavigateFrom();
			}

			CBaseModPanel::GetSingletonPtr()->SafeNavigateTo( previousNav ? previousNav : GetParent(), // if no item is selected, then assume parent has focus
				// this is needed when panel list is the only control on the console and all items are removed due to some
				// data source event and later new items get automatically inserted, so SafeNavigateTo doesn't really know
				// what could be having focus to steal from and assumes it's the parent dialog that is not a real control
				m_CurrentSelectedItem ? m_CurrentSelectedItem : GetParent(), bAllowStealFocus );

			if( scrollToItem )
			{
				ScrollToPanelItem( index );
			}

			int regionX, regionY, regionWide, regionTall;
			m_PnlItemRegion->GetBounds( regionX, regionY, regionWide, regionTall );

			int x, y, wide, tall;
			m_CurrentSelectedItem->GetBounds( x, y, wide, tall );

			if ( regionTall > 0 )	// can get called here before our bounds are init
			{
				if ( y < regionY  )
				{
					m_ScrVerticalScroll->SetValue( index * ( tall + m_PanelItemBorder ) );
				}
				else if ( ( y + tall ) > ( regionY + regionTall ) )
				{
					// scroll to regionTall pixels above the bottom of the selected panel
					m_ScrVerticalScroll->SetValue( MAX( ( index + 1 ) * ( tall + m_PanelItemBorder ) - regionTall, 0 ) );
				}
			}

			if ( !bSuppressSelectionSound )
			{
				CBaseModPanel::GetSingleton().PlayUISound( UISOUND_FOCUS );
			}
			

			PostMessage( m_CurrentSelectedItem->GetVPanel(), new KeyValues("PanelSelected") );

			KeyValues *pKv = new KeyValues( "OnItemSelected", "panelName", GetName() );
			pKv->SetInt( "index", index );
			CallParentFunction( pKv );
		}
	}

	return itemSelected;
}

//=============================================================================
bool GenericPanelList::SelectPanelItemByPanel( Panel *pPanelItem )
{
	unsigned short idx = 0;
	if ( GetPanelItemIndex( pPanelItem, idx ) )
		return SelectPanelItem( idx, GenericPanelList::SD_DOWN, true );
	else
		return false;
}

//=============================================================================
void GenericPanelList::ClearPanelSelection()
{
	if ( m_CurrentSelectedItem ) 
	{
		PostMessage( m_CurrentSelectedItem->GetVPanel(), new KeyValues("PanelUnSelected") );
		m_CurrentSelectedItem = NULL;
	}
}

//=============================================================================
void GenericPanelList::ScrollToPanelItem( unsigned short index )
{
	if( index < m_PanelItems.Count() )
	{
		Panel* targetPanelItem = m_PanelItems[index];

		int targetX, targetY, targetWide, targetTall;
		targetPanelItem->GetBounds( targetX, targetY, targetWide, targetTall );

		int x, y, wide, tall;
		m_PnlItemRegion->GetBounds( x, y, wide, tall );

		int firstVisi = GetFirstVisibleItemNumber();
		int lastVisi  = GetLastVisibleItemNumber();

		if( index <= firstVisi || index >= lastVisi  ) // outside or on the border (allow re-alignment if it's at the border)
		{
			int travelDistance = 0;

			if( index <= firstVisi ) // the target item is above the clipping region
			{
				travelDistance = targetY - m_PanelItemBorder; //align to top
			}
			else // below the clipping region
			{
				travelDistance = targetY - ( lastVisi - firstVisi ) * ( targetTall + m_PanelItemBorder ) - m_PanelItemBorder; //align to be last item

				int topX, topY;
				m_PanelItems[0]->GetPos( topX, topY );
				if( topY - travelDistance > 0 ) //don't pull the topmost item away from the top
					travelDistance = topY - m_PanelItemBorder;
			}

			m_ScrVerticalScroll->SetValue( m_ScrVerticalScroll->GetValue() + travelDistance );

			for( int i = 0; i < m_PanelItems.Count(); ++i )
			{
				int itemX, itemY;
				m_PanelItems[i]->GetPos( itemX, itemY );
				m_PanelItems[i]->SetPos( itemX, itemY - travelDistance );
			}

			UpdateArrows( );
			UpdatePanels( );
		}
	}
}

//=============================================================================
unsigned short GenericPanelList::AddPanelItem( vgui::Panel* panelItem, bool bNeedsInvalidateScheme )
{
	m_LastItemAdded = m_PanelItems.AddToTail( panelItem );

	panelItem->SetParent( m_PnlItemRegion );
	panelItem->InvalidateLayout( false, bNeedsInvalidateScheme );

	RelinkNavigation();
	InvalidateLayout( false, false );

	CallParentFunction( new KeyValues( "OnItemAdded", "panelName", GetName() ) );

	panelItem->AddActionSignalTarget(this);

	panelItem->SetMouseInputEnabled( true );

	return m_LastItemAdded;
}

//=============================================================================
void GenericPanelList::MovePanelItemToBottom( vgui::Panel* panelItem )
{
	int idx = m_PanelItems.Find( panelItem );
	if ( idx != m_PanelItems.InvalidIndex() )
	{
		m_PanelItems.Remove( idx );
		m_PanelItems.AddToTail( panelItem );
		m_LastItemAdded = m_PanelItems.Count() - 1;
		RelinkNavigation();
	}
}

//=============================================================================
void GenericPanelList::SortPanelItems( int (__cdecl *pfnCompare)( vgui::Panel* const *, vgui::Panel* const *) )
{
	m_PanelItems.Sort( pfnCompare );
	RelinkNavigation();
	InvalidateLayout( true );

	unsigned short nCurrentlySelectedIndex = 0;
	if ( IsGameConsole() && m_CurrentSelectedItem && GetPanelItemIndex( m_CurrentSelectedItem, nCurrentlySelectedIndex ) )
	{
		ScrollToPanelItem( nCurrentlySelectedIndex );
	}
}

//=============================================================================
void GenericPanelList::PerformLayout()
{
	BaseClass::PerformLayout();

	m_PnlItemRegion->SetPos( 0, m_PanelItemBorder );

	//setup the background
	bool arrowsVisible = GetScrollArrowsVisible();
	if( arrowsVisible )
	{
		m_PnlItemRegion->SetSize(GetWide(), GetTall() - m_LblDownArrow->GetTall() - (m_PanelItemBorder * 2));
	}
	else
	{
		m_PnlItemRegion->SetSize(GetWide(), GetTall() - (m_PanelItemBorder * 2));
	}

	int x, y;
	GetPos( x, y );

	int nextItemY = 0, visibleCount = 0;
	int itemWide = GetWide() - ( m_PanelItemBorder * 2 );
	
#ifdef _GAMECONSOLE
	int firstVisi = GetFirstVisibleItemNumber();
	if( firstVisi > 0 )
	{
		nextItemY -= firstVisi * ( m_PanelItemBorder + m_PanelItems[0]->GetTall() );
	}
	else 
#endif
	if( m_ScrVerticalScroll->IsVisible() )
	{
		itemWide -= m_ScrVerticalScroll->GetWide();
		nextItemY -= m_ScrVerticalScroll->GetValue();

		m_PnlItemRegion->SetWide( m_PnlItemRegion->GetWide() - m_ScrVerticalScroll->GetWide() );
	}

	int nTotalContentHeight = 0;
	for( int i = 0; i < m_PanelItems.Count(); ++i )
	{
		nextItemY += m_PanelItemBorder;

		m_PanelItems[i]->SetPos( m_PanelItemBorder, nextItemY );
		m_PanelItems[i]->SetWide( itemWide );

		int itemX, itemY;
		m_PanelItems[i]->GetPos( itemX, itemY );

		nTotalContentHeight += m_PanelItemBorder + m_PanelItems[i]->GetTall();
		nextItemY = itemY + m_PanelItems[i]->GetTall();
		++visibleCount;
	}

	m_ScrVerticalScroll->SetPos( GetWide( ) - m_ScrVerticalScroll->GetWide(), 0 );
	m_ScrVerticalScroll->SetTall( GetTall() );
	m_ScrVerticalScroll->SetRange( 0, nTotalContentHeight + m_PanelItemBorder );
	m_ScrVerticalScroll->SetRangeWindow( MIN( GetTall(), nTotalContentHeight + m_PanelItemBorder ) );
	m_ScrVerticalScroll->SetButtonPressedScrollValue( 24 );
	m_ScrVerticalScroll->InvalidateLayout();

	int wide, tall;
	m_LblDownArrow->GetContentSize( wide, tall );
	m_LblDownArrow->SetSize( wide, tall );

	m_LblUpArrow->GetContentSize( wide, tall );
	m_LblUpArrow->SetSize( wide, tall );

	int xPos = GetWide() - m_LblUpArrow->GetWide() - m_LblDownArrow->GetWide() - m_PanelItemBorder;
	int yPos = GetTall() - m_LblDownArrow->GetTall() - m_PanelItemBorder;

	m_LblDownArrow->SetPos( xPos, yPos );
	m_LblUpArrow->SetPos( xPos + m_LblDownArrow->GetWide(), yPos );

	m_LblScrollProgress->SetPos( 2 * m_PanelItemBorder, yPos + m_PanelItemBorder );
	m_LblScrollProgress->SetWide( 200 );
	m_LblScrollProgress->SetVisible( m_bShowScrollProgress );

	UpdateArrows();
	UpdatePanels();

#ifdef _GAMECONSOLE
	if( m_CurrentSelectedItem != NULL && ( HasFocus() || m_CurrentSelectedItem->HasFocus() ) )
	{
		m_CurrentSelectedItem->NavigateTo();
	}
#endif // _GAMECONSOLE
}

//=============================================================================
void GenericPanelList::PaintBackground()
{
	BaseClass::PaintBackground();
}

//=============================================================================
void GenericPanelList::Paint()
{
	BaseClass::Paint();
}

//=============================================================================
void GenericPanelList::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	Color bgColor = Color( 255, 255, 255, 255 );
	
	if( m_SchemeBgColorName[0] != '\0' )
	{
		bgColor = GetSchemeColor( m_SchemeBgColorName, pScheme );
	}
	else
	{
		bgColor = GetSchemeColor( "GenericPanelList.BgColor", pScheme );
	}

	SetBorder( pScheme->GetBorder( "GenericPanelListBorder" ) );
	SetBgColor( bgColor );

	m_LblDownArrow->SetFont( pScheme->GetFont( "GameUIButtons" ) );
	m_LblUpArrow->SetFont( pScheme->GetFont( "GameUIButtons" ) );
}

//=============================================================================
void GenericPanelList::ApplySettings( KeyValues* inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	if( inResourceData->GetInt( "NoDrawPanel", 0 ) )
	{
		SetPaintBackgroundEnabled( false );
	}

	m_PanelItemBorder =  scheme()->GetProportionalScaledValueEx( GetScheme(), inResourceData->GetInt( "panelBorder", 4 ) );

	// Never show arrows on PC.  Show arrows on 360 unless specified not to in .res file.
	bool isArrowVisible = IsGameConsole() ? ( inResourceData->GetInt( "arrowsVisible", 1 ) == 1 ) : false;
	SetScrollArrowsVisible( isArrowVisible );

	m_bWrap = inResourceData->GetInt( "NoWrap", 0 ) == 0;
}

//=============================================================================
void GenericPanelList::UpdateArrows()
{
	if ( GetScrollArrowsVisible() )
	{
		m_LblUpArrow->SetText( "#GameUI_Icons_UP_ARROW" );
		m_LblUpArrow->SetAlpha( 100 );

		m_LblDownArrow->SetText( "#GameUI_Icons_DOWN_ARROW" );
		m_LblDownArrow->SetAlpha( 100 );

		int firstVisiItem = GetFirstVisibleItemNumber();
		int lastVisiItem = GetLastVisibleItemNumber();

		if ( firstVisiItem > 0 )
		{
			m_LblUpArrow->SetAlpha( 255 );
		}
		
		if ( lastVisiItem < m_PanelItems.Count() - 1 )
		{
			m_LblDownArrow->SetAlpha( 255 );
		}

		// Update the scroll progress
		wchar_t localizedScrollProgress[128]; 
		wchar_t wFirstInView[64];
		wchar_t wLastInView[64];
		wchar_t wTotalAchievements[64];
		int nLabelCount = GetPanelItemCount();
		int nLabelFirst = GetFirstVisibleItemNumber();
		int nLabelLast = GetLastVisibleItemNumber();

		// Construct achievement progress string
		Q_snwprintf( wTotalAchievements, sizeof( wTotalAchievements ), L"%d", nLabelCount );
		Q_snwprintf( wFirstInView, sizeof( wFirstInView ), L"%d", nLabelFirst + 1 );
		Q_snwprintf( wLastInView, sizeof( wLastInView ), L"%d", nLabelLast + 1 );

		g_pVGuiLocalize->ConstructString( localizedScrollProgress, sizeof( localizedScrollProgress ), g_pVGuiLocalize->Find( "#L4D360UI_Scroll_Progress" ), 3, wFirstInView, wLastInView, wTotalAchievements );
		m_LblScrollProgress->SetText( localizedScrollProgress );
	}
}

void GenericPanelList::UpdatePanels()
{
	int firstVisi = GetFirstVisibleItemNumber();
	int lastVisi = GetLastVisibleItemNumber();

	int offsetY = 0;

	vgui::Panel* firstPanel = GetPanelItem( firstVisi );
	if( firstPanel )
	{
		int x;
		firstPanel->GetPos( x, offsetY );
	}

	for( int i = 0; i < GetPanelItemCount(); ++i )
	{
		vgui::Panel *panel = GetPanelItem( i );
		if( panel )
		{
			if( i < firstVisi || i > lastVisi )
			{
				panel->SetVisible( IsGameConsole() && ( m_ItemSelectionModeMask & GenericPanelList::ISM_PERITEM ) );
				if ( m_ItemSelectionModeMask & ISM_ALPHA_INVISIBLE )
					panel->SetAlpha( 0 );
			}
			else
			{
				panel->SetVisible( true );
				if ( m_ItemSelectionModeMask & ISM_ALPHA_INVISIBLE )
					panel->SetAlpha( 255 );
			}
		}
	} 
}

bool GenericPanelList::IsPanelItemVisible( Panel *pPanelItem, bool bRequireFullyVisible )
{
	if ( !pPanelItem )
		return false;

	int nRegionX, nRegionY, nRegionTall;
	m_PnlItemRegion->GetPos( nRegionX, nRegionY );
	nRegionTall = m_PnlItemRegion->GetTall();

	int nPanelX, nPanelY, nPanelTall;
	pPanelItem->GetPos( nPanelX, nPanelY );
	nPanelTall = pPanelItem->GetTall();

	if( bRequireFullyVisible )
	{
		return ( nPanelY >= nRegionY && nPanelY + nPanelTall <= nRegionY + nRegionTall );
	}
	else
	{
		return ( nPanelY >= nRegionY && nPanelY < nRegionY + nRegionTall ) ||
			( nPanelY + nPanelTall > nRegionY && nPanelY + nPanelTall <= nRegionY + nRegionTall );
	}

	return 0;
}

int GenericPanelList::GetFirstVisibleItemNumber( bool bRequireFullyVisible )
{
	for( int i = 0; i < m_PanelItems.Count(); ++i )
	{
		if ( IsPanelItemVisible( m_PanelItems[ i ], bRequireFullyVisible ) )
			return i;
	}

	return 0;
} 

int GenericPanelList::GetLastVisibleItemNumber( bool bRequireFullyVisible )
{
	for( int i = m_PanelItems.Count(); i --> 0 ; )
	{
		if ( IsPanelItemVisible( m_PanelItems[ i ], bRequireFullyVisible ) )
			return i;
	}

	return 0; 
}

//=============================================================================
Panel* GenericPanelList::GetFirstVisibleItem()
{
	int idx = GetFirstVisibleItemNumber();
	if ( idx < 0 || idx >= m_PanelItems.Count() )
		return NULL;
	else
		return m_PanelItems[idx];
}

void GenericPanelList::NavigateToChild( Panel *pNavigateTo )
{
	if( GetParent() )
		GetParent()->NavigateToChild( this );

	return BaseClass::NavigateToChild( pNavigateTo );
}

#ifndef _GAMECONSOLE
void GenericPanelList::NavigateTo()
{
	for( int i = 0; i != m_PanelItems.Count(); ++i )
	{
		if( m_PanelItems[i] == m_CurrentSelectedItem )
			m_PanelItems[i]->NavigateTo();
		else
			m_PanelItems[i]->NavigateFrom();
	}
}

void GenericPanelList::NavigateFrom()
{
	if( m_CurrentSelectedItem )
		m_CurrentSelectedItem->NavigateFrom();
}
#endif


#ifdef _GAMECONSOLE
//=============================================================================
void GenericPanelList::NavigateTo()
{
	BaseClass::NavigateTo();

	if ( m_CurrentSelectedItem )
	{
		unsigned short uIdx = 0;
		if ( GetPanelItemIndex( m_CurrentSelectedItem, uIdx ) )
		{
			SelectPanelItem( uIdx );
			m_CurrentSelectedItem->NavigateTo();
			return;
		}
	}

	m_CurrentSelectedItem = NULL;

	if( m_PanelItems.Count() > 0 )
	{
		// determine which item to select based on how we were navigated to
		switch( GetLastNavDirection() )
		{
		case ND_UP:
			if( !SelectPanelItem( m_PanelItems.Count() - 1, GenericPanelList::SD_UP ) )
			{
				// Prevent recursion
				if( GetNavUp() != this )
				{
					BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XSTICK1_UP, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
				}
			}
			break;

		case ND_DOWN:
		default:
			if( !SelectPanelItem( 0, GenericPanelList::SD_DOWN ) )
			{
				// Prevent recursion
				if( GetNavDown() != this )
				{
					BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XSTICK1_DOWN, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
				}
			}
			break;
		}
	}
	else
	{
		// determine which item to select based on how we were navigated to
		switch( GetLastNavDirection() )
		{
		case ND_LEFT:
			// If there is something in the direction we were going, continue on to that control
			if( GetNavLeft() != 0 && GetNavLeft() != this )
			{
				BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XSTICK1_LEFT, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
			}
			// Otherwise check the other direction
			else if( GetNavRight() != 0 && GetNavRight() != this )
			{
				BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XSTICK1_RIGHT, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
			}
			else // Otherwise just let our base class handle it
			{
				BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XSTICK1_LEFT, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
			}
			break;

		case ND_RIGHT:
			// If there is something in the direction we were going, continue on to that control
			if( GetNavRight() != 0 && GetNavRight() != this )
			{
				BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XSTICK1_RIGHT, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
			}
			// Otherwise check the other direction
			else if( GetNavLeft() != 0 && GetNavLeft() != this )
			{
				BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XSTICK1_LEFT, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
			}
			else // Otherwise just let our base class handle it
			{
				BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XSTICK1_RIGHT, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
			}
			break;

		case ND_UP:
			// If there is something in the direction we were going, continue on to that control
			if( GetNavUp() != 0 && GetNavUp() != this )
			{
				BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XSTICK1_UP, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
			}
			// Otherwise check the other direction
			else if( GetNavDown() != 0 && GetNavDown() != this )
			{
				BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XSTICK1_DOWN, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
			}
			else // Otherwise just let our base class handle it
			{
				BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XSTICK1_UP, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
			}
			break;

		case ND_DOWN:
		default:
			// If there is something in the direction we were going, continue on to that control
			if( GetNavDown() != 0 && GetNavDown() != this )
			{
				BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XSTICK1_DOWN, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
			}
			// Otherwise check the other direction
			else if( GetNavUp() != 0 && GetNavUp() != this )
			{
				BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XSTICK1_UP, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
			}
			else // Otherwise just let our base class handle it
			{
				BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XSTICK1_DOWN, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
			}
			break;
		}
	}
}

//=============================================================================
void GenericPanelList::NavigateFrom()
{
	if( m_CurrentSelectedItem != 0 )
	{
		m_CurrentSelectedItem->NavigateFrom();
		m_CurrentSelectedItem = NULL;
	}

	BaseClass::NavigateFrom();
}

#endif // _GAMECONSOLE

//=============================================================================
void GenericPanelList::Sort( GPL_LHS_less_RHS* sortFunction )
{
	for( int i = 0; i < m_PanelItems.Count() - 1; ++i )
	{
		for( int j = 0; j < m_PanelItems.Count() - 1 - i; ++j )
		{
			if( sortFunction( *m_PanelItems[j + 1], *m_PanelItems[j] ) )
			{
				Panel* temp = m_PanelItems[j];
				m_PanelItems[j] = m_PanelItems[j + 1];
				m_PanelItems[j + 1] = temp;
			}
		}
	}

	RelinkNavigation();
	InvalidateLayout();
}

//=============================================================================
void GenericPanelList::Filter( GPL_SHOW_ITEM* filterFunction )
{
	for( int i = 0; i < m_PanelItems.Count(); ++i )
	{
		if( filterFunction( *m_PanelItems[i] ) )
		{
			m_PanelItems[i]->SetVisible( true );
		}
		else
		{
			m_PanelItems[i]->SetVisible( false );
		}
	}

	for( int i = 0; i < m_PanelItems.Count() - 1; ++i )
	{
		for( int j = 0; j < m_PanelItems.Count() - 1 - i; ++j )
		{
			if( ( m_PanelItems[j + 1]->IsVisible() ) && ( !m_PanelItems[j]->IsVisible() ) )
			{
				Panel* temp = m_PanelItems[j];
				m_PanelItems[j] = m_PanelItems[j + 1];
				m_PanelItems[j + 1] = temp;
			}
		}
	}

	RelinkNavigation();
	InvalidateLayout();
}

void GenericPanelList::ElevatorScroll( bool bScrollUp )
{
	if( bScrollUp )
	{
		int iFirstVisi = GetFirstVisibleItemNumber( true );
		if( ( 0 != iFirstVisi ) ) 
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_FOCUS );
			
			ScrollToPanelItem( iFirstVisi - 1 );		

			UpdateArrows();
			UpdatePanels();
		}
		else
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_DENY );
		}
	}
	else 
	{
		int lastVisi = GetLastVisibleItemNumber( true );
		if( GetPanelItemCount() - 1 > lastVisi ) 
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_FOCUS );

			ScrollToPanelItem( lastVisi + 1 );

			UpdateArrows();
			UpdatePanels();
		}
		else
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_DENY );
		}
	}
}

//=============================================================================
bool GenericPanelList::GetScrollArrowsVisible( )
{
	return m_LblDownArrow->IsVisible() || m_LblUpArrow->IsVisible();
}

//=============================================================================
void GenericPanelList::SetScrollBarVisible( bool visible )
{
	m_ScrVerticalScroll->SetVisible( visible );

	InvalidateLayout();
}

//=============================================================================
void GenericPanelList::SetScrollArrowsVisible( bool visible )
{
	m_LblDownArrow->SetVisible( visible );
	m_LblUpArrow->SetVisible( visible );

	InvalidateLayout();
}

//=============================================================================
void GenericPanelList::SetSchemeBgColorName( const char* schemeBgColorName )
{
	Q_strcpy( m_SchemeBgColorName, schemeBgColorName );

	InvalidateLayout( false, true );
}

//=============================================================================
unsigned short GenericPanelList::GetLastItemAdded()
{
	return m_LastItemAdded;
}

//=============================================================================
void GenericPanelList::OnItemSelected( const char* panelName )
{
}

//=============================================================================
void GenericPanelList::OnItemAdded( const char* panelName )
{
}

//=============================================================================
void GenericPanelList::OnItemRemoved( const char* panelName )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void GenericPanelList::OnSliderMoved()
{
	InvalidateLayout();
	Repaint();

	PostActionSignal( new KeyValues( "Command", "Command", "PanelListSliderMoved" ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void GenericPanelList::OnChildResized()
{
	InvalidateLayout();
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Scrolls the list according to the mouse wheel movement
//-----------------------------------------------------------------------------
void GenericPanelList::OnMouseWheeled(int delta)
{
	int val = m_ScrVerticalScroll->GetValue();
	val -= (delta * 24);
	m_ScrVerticalScroll->SetValue(val);
}

void GenericPanelList::SetNavigationChangedCallback( void (*pFunction)( GenericPanelList *, vgui::Panel * ) )
{
	m_pItemNavigationChangedCallback = pFunction;
}

void GenericPanelList::RelinkNavigation( void )
{
	int i;
	for( i = 0; i != m_PanelItems.Count(); ++i )
	{
		m_PanelItems[i]->SetNavUp( (vgui::Panel *)NULL );
		m_PanelItems[i]->SetNavDown( (vgui::Panel *)NULL );
	}

	if ( m_ItemSelectionModeMask & GenericPanelList::ISM_PERITEM )
	{
		Panel *pLastValid = NULL;
		Panel *pFirstValid = NULL;

		for ( i = 0; i != m_PanelItems.Count(); ++i )
		{
			if ( m_PanelItems[i]->IsVisible() )
			{
				pFirstValid = m_PanelItems[i];
				pLastValid = pFirstValid;
				++ i;
				break;
			}
		}

		for ( ; i != m_PanelItems.Count(); ++i )
		{
			Panel *pCurrentPanel = m_PanelItems[i];
			if( pCurrentPanel->IsVisible() )
			{
				pLastValid->SetNavDown( pCurrentPanel );
				pCurrentPanel->SetNavUp( pLastValid );
				pLastValid = pCurrentPanel;
			}
		}

		if ( pFirstValid && m_bWrap )
		{
			pFirstValid->SetNavUp( pLastValid );
			pLastValid->SetNavDown( pFirstValid );
		}
	}

	if ( m_pItemNavigationChangedCallback )
	{
		for( i = 0; i != m_PanelItems.Count(); ++i )
		{
			m_pItemNavigationChangedCallback( this, m_PanelItems[i] );
		}
	}
}
