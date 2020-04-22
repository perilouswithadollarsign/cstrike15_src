//====== Copyright © 1996-2006, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "dme_controls/BaseAnimationSetEditor.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/Splitter.h"
#include "vgui_controls/Menu.h"
#include "studio.h"
#include "dme_controls/BaseAnimSetAttributeSliderPanel.h"
#include "dme_controls/BaseAnimSetPresetFaderPanel.h"
#include "dme_controls/BaseAnimSetControlGroupPanel.h"
#include "dme_controls/dmecontrols_utils.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmeanimationset.h"
#include "movieobjects/dmegamemodel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

#define ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT 38
#define ANIMATION_SET_BUTTON_INSET 0

CBaseAnimationSetEditor::CBaseAnimationSetEditor( vgui::Panel *parent, const char *className, CBaseAnimationSetControl *pAnimationSetController ) :
	BaseClass( parent, className ),
	m_Layout( LAYOUT_SPLIT ),
	m_pController( pAnimationSetController )
{
	// SETUP_PANEL( this );

//#pragma warning( disable: 4355 )
	m_pController->SetAnimationSetEditorPanel( this );
//#pragma warning( default: 4355 )

	PostMessage( GetVPanel(), new KeyValues( "OnChangeLayout", "value", m_Layout ) );
}

CBaseAnimationSetEditor::~CBaseAnimationSetEditor()
{
}

CBaseAnimationSetControl *CBaseAnimationSetEditor::GetController()
{
	return m_pController;
}

void CBaseAnimationSetEditor::CreateToolsSubPanels()
{
	m_hControlGroup = new CBaseAnimSetControlGroupPanel( (Panel *)NULL, "AnimSetControlGroup", this, false );
	m_hPresetFader = new CBaseAnimSetPresetFaderPanel( (Panel *)NULL, "AnimSetPresetFader", this );
	m_hAttributeSlider = new CBaseAnimSetAttributeSliderPanel( (Panel *)NULL, "AnimSetAttributeSliderPanel", this );
}

void CBaseAnimationSetEditor::ChangeLayout( EAnimSetLayout_t newLayout )
{
	m_Layout = newLayout;

	// Make sure these don't get blown away...
	m_hControlGroup->SetParent( (Panel *)NULL );
	m_hPresetFader->SetParent( (Panel *)NULL );
	m_hAttributeSlider->SetParent( (Panel *)NULL );

	delete m_Splitter.Get();
	m_Splitter = NULL;

	CUtlVector< Panel * > list;
	list.AddToTail( m_hControlGroup.Get() );
	list.AddToTail( m_hPresetFader.Get() );
	list.AddToTail( m_hAttributeSlider.Get() );

	Splitter *sub = NULL;

	switch ( m_Layout )
	{
	default:
	case LAYOUT_SPLIT:
		{
			m_Splitter = new Splitter( this, "AnimSetEditorMainSplitter", SPLITTER_MODE_VERTICAL, 1 );
			m_Splitter->SetAutoResize
				( 
				Panel::PIN_TOPLEFT, 
				Panel::AUTORESIZE_DOWNANDRIGHT,
				0, ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT,
				0, 0
				);
			m_Splitter->SetBounds( 0, ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT, GetWide(), GetTall() - ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT );
			m_Splitter->SetSplitterColor( Color(32, 32, 32, 255) );

			// m_Splitter->EnableBorders( false );

			m_hControlGroup->SetParent( m_Splitter->GetChild( 0 ) );
			m_hControlGroup->SetAutoResize
				( 
				Panel::PIN_TOPLEFT, 
				Panel::AUTORESIZE_DOWNANDRIGHT,
				0, 0,
				0, 0
				);

			sub = new Splitter( m_Splitter->GetChild( 1 ), "AnimSetEditorSubSplitter", SPLITTER_MODE_HORIZONTAL, 1 );
			sub->SetAutoResize
				( 
				Panel::PIN_TOPLEFT, 
				Panel::AUTORESIZE_DOWNANDRIGHT,
				0, 0,
				0, 0
				);

			m_hPresetFader->SetParent( sub->GetChild( 0 ) );
			m_hPresetFader->SetAutoResize
				( 
				Panel::PIN_TOPLEFT, 
				Panel::AUTORESIZE_DOWNANDRIGHT,
				0, 0,
				0, 0
				);
			m_hAttributeSlider->SetParent( sub->GetChild( 1 ) );
			m_hAttributeSlider->SetAutoResize
				( 
				Panel::PIN_TOPLEFT, 
				Panel::AUTORESIZE_DOWNANDRIGHT,
				0, 0,
				0, 0
				);
		}
		break;
	case LAYOUT_VERTICAL:
		{
			m_Splitter = new Splitter( this, "AnimSetEditorMainSplitter", SPLITTER_MODE_VERTICAL, 2 );
			m_Splitter->SetSplitterColor( Color(32, 32, 32, 255) );
			m_Splitter->SetAutoResize
				( 
				Panel::PIN_TOPLEFT, 
				Panel::AUTORESIZE_DOWNANDRIGHT,
				0, ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT,
				0, 0
				);
			m_Splitter->SetBounds( 0, ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT, GetWide(), GetTall() - ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT );

			for ( int i = 0; i < list.Count(); ++i )
			{
				list[ i ]->SetParent( m_Splitter->GetChild( i ) );
				list[ i ]->SetSize( m_Splitter->GetChild( i )->GetWide(), m_Splitter->GetChild( i )->GetTall() );
				list[ i ]->SetAutoResize
					( 
					Panel::PIN_TOPLEFT, 
					Panel::AUTORESIZE_DOWNANDRIGHT,
					0, 0,
					0, 0
					);
			}

			m_Splitter->EvenlyRespaceSplitters();
		}
		break;
	case LAYOUT_HORIZONTAL:
		{
			m_Splitter = new Splitter( this, "AnimSetEditorMainSplitter", SPLITTER_MODE_HORIZONTAL, 2 );
			m_Splitter->SetSplitterColor( Color(32, 32, 32, 255) );
			m_Splitter->SetAutoResize
				( 
				Panel::PIN_TOPLEFT, 
				Panel::AUTORESIZE_DOWNANDRIGHT,
				0, ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT,
				0, 0
				);

			m_Splitter->SetBounds( 0, ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT, GetWide(), GetTall() - ANIMATION_SET_EDITOR_BUTTONTRAY_HEIGHT );

			for ( int i = 0; i < list.Count(); ++i )
			{
				list[ i ]->SetParent( m_Splitter->GetChild( i ) );
				list[ i ]->SetSize( m_Splitter->GetChild( i )->GetWide(), m_Splitter->GetChild( i )->GetTall() );
				list[ i ]->SetAutoResize
					( 
					Panel::PIN_TOPLEFT, 
					Panel::AUTORESIZE_DOWNANDRIGHT,
					0, 0,
					0, 0
					);
			}

			m_Splitter->EvenlyRespaceSplitters();
		}
		break;
	}

	if ( sub )
	{
		sub->OnSizeChanged( sub->GetWide(), sub->GetTall() );
		sub->EvenlyRespaceSplitters();
	}
}

void CBaseAnimationSetEditor::OnChangeLayout( int value )
{
	ChangeLayout( ( EAnimSetLayout_t )value );
}


void CBaseAnimationSetEditor::OnOpenContextMenu( KeyValues *params )
{
	if ( m_hContextMenu.Get() )
	{
		delete m_hContextMenu.Get();
		m_hContextMenu = NULL;
	}

	m_hContextMenu = new Menu( this, "ActionMenu" );

	m_hContextMenu->AddMenuItem( "#BxAnimSetSplitLayout",		new KeyValues( "OnChangeLayout", "value", (int)LAYOUT_SPLIT ), this );
	m_hContextMenu->AddMenuItem( "#BxAnimSetVerticalLayout",	new KeyValues( "OnChangeLayout", "value", (int)LAYOUT_VERTICAL ), this );
	m_hContextMenu->AddMenuItem( "#BxAnimSetHorizontalLayout",	new KeyValues( "OnChangeLayout", "value", (int)LAYOUT_HORIZONTAL ), this );

	Panel *rpanel = reinterpret_cast< Panel * >( params->GetPtr( "contextlabel" ) );
	if ( rpanel )
	{
		// force the menu to compute required width/height
		m_hContextMenu->PerformLayout();
		m_hContextMenu->PositionRelativeToPanel( rpanel, Menu::DOWN, 0, true );
	}
	else
	{
		Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
	}
}

void CBaseAnimationSetEditor::OpenTreeViewContextMenu( KeyValues *pItemData )
{
}

void CBaseAnimationSetEditor::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// Have to manually apply settings here if they aren't attached in hierarchy
	if ( m_hControlGroup->GetParent() != this )
	{
		m_hControlGroup->ApplySchemeSettings( pScheme );
	}
	if ( m_hPresetFader->GetParent() != this )
	{
		m_hPresetFader->ApplySchemeSettings( pScheme );
	}
	if ( m_hAttributeSlider->GetParent() != this )
	{
		m_hAttributeSlider->ApplySchemeSettings( pScheme );
	}
}

CBaseAnimSetControlGroupPanel *CBaseAnimationSetEditor::GetControlGroup()
{
	return m_hControlGroup.Get();
}

CBaseAnimSetPresetFaderPanel *CBaseAnimationSetEditor::GetPresetFader()
{
	return m_hPresetFader.Get();
}

CBaseAnimSetAttributeSliderPanel *CBaseAnimationSetEditor::GetAttributeSlider()
{
	return m_hAttributeSlider.Get();
}

void CBaseAnimationSetEditor::OnControlsAddedOrRemoved()
{
	m_pController->OnControlsAddedOrRemoved();
	if ( m_hControlGroup )
	{
		m_hControlGroup->OnControlsAddedOrRemoved();
	}
	if ( m_hAttributeSlider )
	{
		m_hAttributeSlider->OnControlsAddedOrRemoved();
	}
}

void CBaseAnimationSetEditor::ChangeAnimationSetClip( CDmeFilmClip *pFilmClip )
{
	m_pController->ChangeAnimationSetClip( pFilmClip );
	if ( m_hControlGroup )
	{
		m_hControlGroup->ChangeAnimationSetClip( pFilmClip );
	}
	if ( m_hAttributeSlider )
	{
		m_hAttributeSlider->ChangeAnimationSetClip( pFilmClip );
	}
}

