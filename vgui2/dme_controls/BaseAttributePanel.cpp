//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======//
//
// Purpose: base class for all element attribute panels
//    An attribute panel is a one line widget that can be used by a list
//    or tree control.
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/BaseAttributePanel.h"
#include "dme_controls/attributewidgetfactory.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/Label.h"
#include "movieobjects/dmeeditortypedictionary.h"
#include "dme_controls/inotifyui.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Lessfunc for columns.
//-----------------------------------------------------------------------------
bool CBaseAttributePanel::ColInfoLessFunc( const CBaseAttributePanel::colinfo_t& lhs, const CBaseAttributePanel::colinfo_t& rhs )
{
	return lhs.panel < rhs.panel;
}

	
//-----------------------------------------------------------------------------
// CBaseAttributePanel constructor
//-----------------------------------------------------------------------------
CBaseAttributePanel::CBaseAttributePanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info.m_pAttributeName ),
	m_pType( 0 ),
	m_hObject( info.m_pElement ),
	m_hEditorInfo( info.m_pEditorInfo ),
	m_hEditorTypeDict( info.m_pEditorTypeDictionary ),
	m_pNotify( info.m_pNotify ),
	m_nArrayIndex( info.m_nArrayIndex ),
	m_ColumnSize( 0, 0, ColInfoLessFunc )
{
	Assert( info.m_pElement );

	InitializeFlags( info );

	Assert( info.m_pAttributeName );
	Q_strncpy( m_szAttributeName, info.m_pAttributeName, sizeof( m_szAttributeName ) );

	m_pType = new Label( this, "AttributeType", "" );
	SetColumnSize( m_pType, 100 );

	CDmAttribute *pAttribute = info.m_pElement->GetAttribute( info.m_pAttributeName );
	m_AttributeType = pAttribute ? pAttribute->GetType() : AT_UNKNOWN;
	if ( m_nArrayIndex >= 0 )
	{
		m_AttributeType = ArrayTypeToValueType( m_AttributeType );
	}

	m_pType->SetText( g_pDataModel->GetAttributeNameForType( m_AttributeType ) );

	m_hFont = NULL;

	// These are draggable
	SetDragEnabled( true );
}


//-----------------------------------------------------------------------------
// This only exists so every class always can chain PostConstructors
//-----------------------------------------------------------------------------
void CBaseAttributePanel::PostConstructor()
{
}

	
//-----------------------------------------------------------------------------
// Initializes flags from the attribute editor info
//-----------------------------------------------------------------------------
void CBaseAttributePanel::InitializeFlags( const AttributeWidgetInfo_t &info )
{
	m_nFlags = 0;
	if ( info.m_pEditorInfo )
	{
		if ( info.m_pEditorInfo->m_bHideType )
		{
			m_nFlags |= HIDETYPE;
		}
		if ( info.m_pEditorInfo->m_bHideValue )
		{
			m_nFlags |= HIDEVALUE;
		}
		if ( info.m_pEditorInfo->m_bIsReadOnly )
		{
			m_nFlags |= READONLY;
		}
	}

	CDmAttribute *pAttribute = info.m_pElement->GetAttribute( info.m_pAttributeName );
	if ( pAttribute && pAttribute->IsFlagSet( FATTRIB_READONLY ) )
	{
		m_nFlags |= READONLY;
	}

	if ( info.m_bAutoApply )
	{
		m_nFlags |= AUTOAPPLY;
	}
}


//-----------------------------------------------------------------------------
// Returns the editor info
//-----------------------------------------------------------------------------
CDmeEditorTypeDictionary *CBaseAttributePanel::GetEditorTypeDictionary()
{
	return m_hEditorTypeDict;
}

CDmeEditorAttributeInfo *CBaseAttributePanel::GetEditorInfo()
{
	return m_hEditorInfo;
}


//-----------------------------------------------------------------------------
// Does the element have the attribute we're attempting to reference?
//-----------------------------------------------------------------------------
bool CBaseAttributePanel::HasAttribute() const
{
	return GetPanelElement()->HasAttribute( m_szAttributeName );
}


//-----------------------------------------------------------------------------
// Returns the attribute array count
//-----------------------------------------------------------------------------
int CBaseAttributePanel::GetAttributeArrayCount() const
{
	CDmrGenericArrayConst array( GetPanelElement(), m_szAttributeName );
	return array.IsValid() ? array.Count() : -1;
}


//-----------------------------------------------------------------------------
// Sets the font
//-----------------------------------------------------------------------------
void CBaseAttributePanel::SetFont( HFont font )
{
	m_hFont = font;
	m_pType->SetFont(font);
}

//-----------------------------------------------------------------------------
// Applies scheme settings
//-----------------------------------------------------------------------------
void CBaseAttributePanel::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// set the color of the "type" column
	m_pType->SetFgColor( Color ( 160, 160, 160, 255 ) );

	if ( GetDirty() )
	{
		SetBgColor( pScheme->GetColor( "AttributeWidget.DirtyBgColor", Color( 100, 100, 200, 63 ) ) );
	}
	else
	{
		SetBgColor( pScheme->GetColor( "Panel.BgColor", Color( 0, 0, 0, 0 ) ) );
	}
	HFont font = pScheme->GetFont( "DmePropertyVerySmall", IsProportional() );
	// m_pType->SetFont(font);

	if ( !m_hFont )
	{
		m_hFont = font;
	}
	SetFont( m_hFont );

}

//-----------------------------------------------------------------------------
// Gets/Sets the attribute value from a string
//-----------------------------------------------------------------------------
void CBaseAttributePanel::SetAttributeValueFromString( const char *pString )
{
	if ( m_nArrayIndex < 0 )
	{
		GetPanelElement()->SetValueFromString( m_szAttributeName, pString );
	}
	else
	{
		CDmrGenericArray array( GetPanelElement(), m_szAttributeName );
		array.SetFromString( m_nArrayIndex, pString );
	}
}

bool CBaseAttributePanel::GetAttributeValueAsString( char *pBuf, int nLength )
{
	CDmElement *pElement = m_hObject.Get();
	if ( !pElement )
	{
		*pBuf = '\0';
		return false;
	}

	if ( m_nArrayIndex < 0 )
	{
		pElement->GetValueAsString( m_szAttributeName, pBuf, nLength );
	}
	else
	{
		CDmrGenericArray array( pElement, m_szAttributeName );
		array.GetAsString( m_nArrayIndex, pBuf, nLength );
	}
	return true;
}

CDmAttribute *CBaseAttributePanel::GetAttribute()
{
	CDmElement *pElement = m_hObject.Get();
	if ( !pElement )
		return NULL;

	return pElement->GetAttribute( m_szAttributeName );
}


//-----------------------------------------------------------------------------
// Helper to get/set the attribute value for elements
//-----------------------------------------------------------------------------
CDmElement *CBaseAttributePanel::GetAttributeValueElement()
{
	return GetElement< CDmElement >( GetAttributeValue<DmElementHandle_t>( ) );
}

void CBaseAttributePanel::SetAttributeValueElement( CDmElement *pElement )
{
	return SetAttributeValue( pElement->GetHandle() );
}


void CBaseAttributePanel::SetDirty( bool dirty )
{
	SetFlag( DIRTY, dirty );
	InvalidateLayout( false, true );
}

void CBaseAttributePanel::SetColumnSize( Panel *panel, int width )
{
	colinfo_t search;
	search.panel = panel;
	int idx = m_ColumnSize.Find( search );
	if ( idx == m_ColumnSize.InvalidIndex() )
	{
		idx = m_ColumnSize.Insert( search );
	}
	m_ColumnSize[ idx ].width = width;
}

int CBaseAttributePanel::GetSizeForColumn( Panel *panel )
{
	colinfo_t search;
	search.panel = panel;
	int idx = m_ColumnSize.Find( search );
	if ( idx == m_ColumnSize.InvalidIndex() )
	{
		return 100;
	}
	return m_ColumnSize[ idx ].width;
}

void CBaseAttributePanel::GetPickerBounds( int *x, int *y, int *w, int *h )
{
	int viewX, viewY, viewWidth, viewHeight;
	GetBounds( viewX, viewY, viewWidth, viewHeight );

	*x = ColumnBorderWidth;
	*w = PickerWidth;
	*y = MAX(0, ceil((viewHeight - PickerHeight) * 0.5)) + 1;
	*h = PickerHeight;
}

//-----------------------------------------------------------------------------
// Creates a widget using editor attribute info
//-----------------------------------------------------------------------------
void CBaseAttributePanel::PerformLayout()
{
	BaseClass::PerformLayout();
	
	vgui::Panel *valuePanel = GetDataPanel();
	if ( valuePanel && HasFlag( HIDEVALUE ) )
	{
		valuePanel->SetVisible( false );
	}
	
	vgui::Panel *typePanel = m_pType;
	if ( typePanel && HasFlag( HIDETYPE ) )
	{
		typePanel->SetVisible( false );
		
	}

	int viewWidth, viewHeight;
	GetSize( viewWidth, viewHeight );

	if( typePanel->IsVisible() && valuePanel->IsVisible() )
	{
		valuePanel->SetBounds( 
			FirstColumnWidth, 
			1, 
			viewWidth - FirstColumnWidth - ColumnBorderWidth - TypeColumnWidth - ColumnBorderWidth,
			viewHeight );
		typePanel->SetBounds( 
			viewWidth - TypeColumnWidth, 
			1, 
			TypeColumnWidth,
			viewHeight );
	}
	else if( typePanel->IsVisible() )
	{
		typePanel->SetBounds( FirstColumnWidth, 1, viewWidth - FirstColumnWidth - ColumnBorderWidth - ColumnBorderWidth, viewHeight);	
	}
	else if( valuePanel->IsVisible() )
	{
		valuePanel->SetBounds( FirstColumnWidth, 1, viewWidth - FirstColumnWidth - ColumnBorderWidth - ColumnBorderWidth, viewHeight);
	}
}

void CBaseAttributePanel::OnApplyChanges()
{
	Assert( !IsAutoApply() );

	Apply();
	SetDirty(false);
}

void CBaseAttributePanel::OnRefresh()
{
	Refresh();
}

void CBaseAttributePanel::OnCreateDragData( KeyValues *msg )
{
	if ( GetPanelElement() )
	{
		msg->SetInt( "root", GetPanelElement() ? GetPanelElement()->GetHandle() : DMELEMENT_HANDLE_INVALID );
		msg->SetString( "type", g_pDataModel->GetAttributeNameForType( m_AttributeType ) );
		msg->SetString( "attributename", m_szAttributeName );
		if ( m_nArrayIndex >= 0 )
		{
			msg->SetInt( "arrayIndex", m_nArrayIndex );
		}

		if ( m_AttributeType != AT_ELEMENT && m_AttributeType != AT_ELEMENT_ARRAY )
		{
			char pTemp[512];
			GetAttributeValueAsString( pTemp, sizeof( pTemp ) );
			msg->SetString( "text", pTemp );
		}
	}
}
