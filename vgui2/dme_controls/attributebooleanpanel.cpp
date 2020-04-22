//============ Copyright (c) Valve Corporation, All rights reserved. ============

#include "dme_controls/attributebooleanpanel.h"
#include "dme_controls/AttributeTextEntry.h"
#include "dme_controls/AttributeWidgetFactory.h"
#include "tier1/KeyValues.h"
#include "datamodel/dmelement.h"
#include "movieobjects/dmeeditortypedictionary.h"
#include "movieobjects/dmechannel.h"
#include "dme_controls/inotifyui.h"
#include "vgui_controls/CheckButton.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;

//-----------------------------------------------------------------------------

CAttributeBooleanPanel::CAttributeBooleanPanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info )
{
	m_pValueButton = new vgui::CheckButton( this, "", "" );
	m_pValueButton->AddActionSignalTarget( this );
}

//-----------------------------------------------------------------------------

void CAttributeBooleanPanel::ApplySchemeSettings(IScheme *pScheme)
{
	// Need to override the scheme settings for this button
	BaseClass::ApplySchemeSettings( pScheme );

	m_pValueButton->SetBorder(NULL);
	m_pValueButton->SetPaintBorderEnabled( false );
	// Hack to get rid of the checkbox offset of &!^@#% 6
	// m_pValueButton->SetImage( vgui::scheme()->GetImage( "tools/ifm/icon_properties_linkarrow" , false), 0);
	m_pValueButton->SetImageAtIndex( 0, m_pValueButton->GetImageAtIndex( 0 ), 0 );
}

void CAttributeBooleanPanel::OnCheckButtonChecked( int state )
{
	bool attributeValue = GetAttributeValue< bool>();
	bool buttonValue = (state == 1);
	if( buttonValue != attributeValue )
	{
		SetAttributeValue( state );
		Refresh();
	}
}


void CAttributeBooleanPanel::Refresh()
{
	BaseClass::Refresh();
	bool myValue = GetAttributeValue< bool>();
	if( myValue != m_pValueButton->IsSelected() )
	{
		m_pValueButton->SetSelected( myValue );
	}
}

void CAttributeBooleanPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	int viewWidth, viewHeight;
	GetSize( viewWidth, viewHeight );

	m_pValueButton->SetBounds( (FirstColumnWidth - ColumnBorderWidth - 16) * 0.5 , ( viewHeight - 16 )* 0.5 , 16, 16 );
}

