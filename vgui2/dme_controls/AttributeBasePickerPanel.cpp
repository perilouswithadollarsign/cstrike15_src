//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/AttributeBasePickerPanel.h"
#include "FileSystem.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/FileOpenDialog.h"
#include "dme_controls/AttributeTextEntry.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CAttributeBasePickerPanel::CAttributeBasePickerPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info )
{
	m_pOpen = new vgui::Button( this, "Open", "...", this, "open" );
	
	// m_pOpen->SetImage( vgui::scheme()->GetImage( "tools/ifm/icon_properties_linkarrow" , false), 0 );
	// m_pOpen->SetPaintBorderEnabled( false );
	// m_pOpen->SetContentAlignment( vgui::Label::a_center );

}

void CAttributeBasePickerPanel::OnCommand( char const *cmd )
{
	if ( !Q_stricmp( cmd, "open" ) )
	{
		ShowPickerDialog();
	}
	else
	{
		BaseClass::OnCommand( cmd );
	}
}

void CAttributeBasePickerPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	int viewWidth, viewHeight;
	GetSize( viewWidth, viewHeight );

	IImage *arrowImage = vgui::scheme()->GetImage( "tools/ifm/icon_properties_linkarrow" , false);
	if( arrowImage )
	{
		m_pOpen->SetImage( arrowImage , 0 );
		m_pOpen->SetPaintBorderEnabled( false );
		m_pOpen->SetContentAlignment( vgui::Label::a_center );
		m_pOpen->SetBounds( (FirstColumnWidth - ColumnBorderWidth - 16) * 0.5 , ( viewHeight - 16 )* 0.5 , 16, 16 );
	}
	else
	{
		m_pOpen->SetBounds( 0, 0, 50, 20 );
	}
}
