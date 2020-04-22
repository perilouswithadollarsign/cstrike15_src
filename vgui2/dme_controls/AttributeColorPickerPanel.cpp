//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/AttributeColorPickerPanel.h"
#include "datamodel/dmelement.h"
#include "vgui_controls/Button.h"
#include "dme_controls/AttributeTextEntry.h"
#include "matsys_controls/colorpickerpanel.h"
#include "tier1/keyvalues.h"
#include "dme_controls/inotifyui.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CAttributeColorPickerPanel::CAttributeColorPickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info )
{
	m_pOpen = new vgui::Button( this, "Open", "", this, "open" );
}

void CAttributeColorPickerPanel::OnCommand( char const *cmd )
{
	if ( !Q_stricmp( cmd, "open" ) )
	{
		m_InitialColor = GetAttributeValue<Color>();
		CColorPickerFrame *pColorPickerDialog = new CColorPickerFrame( this, "Select Color" );
		pColorPickerDialog->AddActionSignalTarget( this );
		pColorPickerDialog->DoModal( m_InitialColor );
	}
	else
	{
		BaseClass::OnCommand( cmd );
	}
}


//-----------------------------------------------------------------------------
// Update button color
//-----------------------------------------------------------------------------
void CAttributeColorPickerPanel::UpdateButtonColor()
{
	Color clr = GetAttributeValue<Color>();
	clr.SetColor( clr.r(), clr.g(), clr.b(), 255 );
	m_pOpen->SetDefaultColor( clr, clr );
	m_pOpen->SetArmedColor( clr, clr );
	m_pOpen->SetDepressedColor( clr, clr );
}


//-----------------------------------------------------------------------------
// Used to set the current color on the picker button
//-----------------------------------------------------------------------------
void CAttributeColorPickerPanel::Refresh()
{
	BaseClass::Refresh();
	UpdateButtonColor();
}

void CAttributeColorPickerPanel::ApplySchemeSettings(IScheme *pScheme)
{
	// Need to override the scheme settings for this button
	BaseClass::ApplySchemeSettings( pScheme );
	UpdateButtonColor();
}


//-----------------------------------------------------------------------------
// Called when the picker gets a new color but apply hasn't happened yet
//-----------------------------------------------------------------------------
void CAttributeColorPickerPanel::OnPreview( KeyValues *data )
{
	{
		CDisableUndoScopeGuard guard;
		Color c = data->GetColor( "color" );
		SetAttributeValue( c );
	}
				 
	Refresh( );
	if ( IsAutoApply() )
	{
		// NOTE: Don't call Apply since that generates an undo record
		CElementTreeNotifyScopeGuard guard( "CAttributeColorPickerPanel::OnPreview", NOTIFY_CHANGE_ATTRIBUTE_VALUE, GetNotify() );
	}
	else
	{
		SetDirty( true );
	}
}


//-----------------------------------------------------------------------------
// Called when cancel is hit in the picker
//-----------------------------------------------------------------------------
void CAttributeColorPickerPanel::OnCancelled( )
{
	// Restore the initial color
	CDisableUndoScopeGuard guard;
	SetAttributeValue( m_InitialColor );
	Refresh( );
	CElementTreeNotifyScopeGuard notify( "CAttributeColorPickerPanel::OnCancelled", NOTIFY_CHANGE_ATTRIBUTE_VALUE, GetNotify() );
	SetDirty( false );
}


//-----------------------------------------------------------------------------
// Called when the picker gets a new color
//-----------------------------------------------------------------------------
void CAttributeColorPickerPanel::OnPicked( KeyValues *data )
{
	Color c = data->GetColor( "color" );
	if ( c == m_InitialColor )
 	{
		SetDirty( false );
		return;
	}

	// Kind of an evil hack, but "undo" copies the "old value" off for doing undo, and that value is the new value because
	//  we haven't been tracking undo while manipulating this.  So we'll turn off undo and set the value to the original value.

	// In effect, all of the wheeling will drop out and it'll look just like we started at the original value and ended up at the
	//  final value...
	{
		CDisableUndoScopeGuard guard;
		SetAttributeValue( m_InitialColor );
	}

	{
		CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, GetNotify(), "Set Attribute Value", "Set Attribute Value" );
		SetAttributeValue( c );
	}

	if ( IsAutoApply() )
	{
		Refresh();
		SetDirty( false );
	}
}


//-----------------------------------------------------------------------------
// Lays out the button position
//-----------------------------------------------------------------------------
void CAttributeColorPickerPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	int pickerX, pickerY, pickerW, pickerH;
	GetPickerBounds( &pickerX, &pickerY, &pickerW, &pickerH );

	m_pOpen->SetBounds( pickerX, pickerY, pickerW, pickerH  );
}
