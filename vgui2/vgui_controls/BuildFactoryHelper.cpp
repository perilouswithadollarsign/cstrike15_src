//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Helper for the CHudElement class to add themselves to the list of hud elements
//
// $NoKeywords: $
//=============================================================================//
#include "vgui/IVGui.h"
#include "vgui_controls/MessageMap.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"
#include "vgui_controls/Panel.h"

using namespace vgui;

// Start with empty list
CBuildFactoryHelper *CBuildFactoryHelper::m_sHelpers = NULL;

//-----------------------------------------------------------------------------
// Purpose: Constructs a panel  factory
// Input  : pfnCreate - fn Ptr to a function which generates a panel
//-----------------------------------------------------------------------------
CBuildFactoryHelper::CBuildFactoryHelper( char const *className, PANELCREATEFUNC func )
{
	// Make this fatal
	if ( HasFactory( className ) )
	{
		Error( "CBuildFactoryHelper:  Factory for '%s' already exists!!!!\n", className );
	}

	//List is empty, or element belongs at front, insert here
	m_pNext			= m_sHelpers;
	m_sHelpers		= this;

	Assert( func );
	m_CreateFunc = func;
	Assert( className );
	m_pClassName = className;
}

//-----------------------------------------------------------------------------
// Purpose: Returns next object in list
// Output : CBuildFactoryHelper
//-----------------------------------------------------------------------------
CBuildFactoryHelper *CBuildFactoryHelper::GetNext( void )
{ 
	return m_pNext;
}

char const *CBuildFactoryHelper::GetClassName() const
{
	return m_pClassName;
}

vgui::Panel *CBuildFactoryHelper::CreatePanel()
{
	if ( !m_CreateFunc )
		return NULL;

	return ( *m_CreateFunc )();
}

// private static meethod
bool CBuildFactoryHelper::HasFactory( char const *className )
{
	CBuildFactoryHelper *p = m_sHelpers;
	while ( p )
	{
		if ( !Q_stricmp( className, p->GetClassName() ) )
			return true;

		p = p->GetNext();
	}
	return false;
}

// static method
vgui::Panel *CBuildFactoryHelper::InstancePanel( char const *className )
{
	CBuildFactoryHelper *p = m_sHelpers;
	while ( p )
	{
		if ( !Q_stricmp( className, p->GetClassName() ) )
			return p->CreatePanel();

		p = p->GetNext();
	}
	return NULL;
}

// static method
void CBuildFactoryHelper::GetFactoryNames( CUtlVector< char const * >& list )
{
	list.RemoveAll();

	CBuildFactoryHelper *p = m_sHelpers;
	while ( p )
	{
		list.AddToTail( p->GetClassName() );
		p = p->GetNext();
	}
}


CDmxElement *CBuildFactoryHelper::CreatePanelDmxElement( vgui::Panel *pPanel )
{
	// Create DMX elements representing panels
	CDmxElement *pPanelElement = CreateDmxElement( "CDmePanelDefinition" );
	CDmxElementModifyScope modify( pPanelElement );

	CDmxAttribute *panelTypeAttr = pPanelElement->AddAttribute( "panelClassType" );
	panelTypeAttr->SetValue( pPanel->GetClassName() );


	const DmxElementUnpackStructure_t *pUnpack = pPanel->GetUnpackStructure();
	if ( pUnpack )
	{
		pPanelElement->AddAttributesFromStructure( pPanel, pUnpack );
	}

	// Hack to deal with IPanel ( pretty sure this works! )
	pUnpack = vgui::ipanel()->GetUnpackStructure( pPanel->GetVPanel() );
	if ( pUnpack )
	{
		pPanelElement->AddAttributesFromStructure( (void *)pPanel->GetVPanel(), pUnpack );
	}

	CDmxAttribute* pAttribute = pPanelElement->AddAttribute( "children" );
	CUtlVector< CDmxElement* >& children = pAttribute->GetArrayForEdit<CDmxElement*>();
	int nChildCount = pPanel->GetChildCount();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CDmxElement *pChildElement = CreatePanelDmxElement( pPanel->GetChild(i) );
		children.AddToTail( pChildElement );
	}

	return pPanelElement;
}



bool CBuildFactoryHelper::Serialize( CUtlBuffer &buf, vgui::Panel *pPanel )
{
	DECLARE_DMX_CONTEXT();
	CDmxElement *pRoot = CreatePanelDmxElement( pPanel );
	bool bOk = SerializeDMX( buf, pRoot );
	CleanupDMX( pRoot );
	return bOk;
}


Panel* CBuildFactoryHelper::UnserializeDmxElementPanel( CDmxElement *pElement )
{
	// FIXME: This will change as the file format changes
	if ( Q_stricmp( pElement->GetTypeString(), "CDmePanelDefinition" ) )
		return NULL;

	Panel *pNewPanel = CBuildFactoryHelper::InstancePanel( pElement->GetValueString( "panelClassType" ) );
	if ( !pNewPanel )
	{
		return NULL;
	}

	const DmxElementUnpackStructure_t *pUnpack = pNewPanel->GetUnpackStructure();         
	if ( pUnpack )
	{
		pElement->UnpackIntoStructure( pNewPanel, pUnpack );
	}

	// Hack to deal with IPanel ( pretty sure this works! )
	pUnpack = vgui::ipanel()->GetUnpackStructure( pNewPanel->GetVPanel() );
	if ( pUnpack )
	{
		pElement->UnpackIntoStructure( (void *)pNewPanel->GetVPanel(), pUnpack );
	}

	// Children are added manually, since the unpack framework doesn't
	// know how to create all the various panel types
	const CDmxAttribute* pAttribute = pElement->GetAttribute( "children" );
	if ( pAttribute && pAttribute->GetType() == AT_ELEMENT_ARRAY )
	{
		const CUtlVector< CDmxElement* >& children = pAttribute->GetArray<CDmxElement*>();
		for ( int i = 0; i < children.Count(); ++i )
		{
			Panel *pChildPanel = UnserializeDmxElementPanel( children[i] );
			if ( pChildPanel )
			{
				pChildPanel->SetParent( pNewPanel );
			}
		}
	}

	// This is a new virtual method in Panel which is called post-serialization
	pNewPanel->OnUnserialized( pElement );
	return pNewPanel;
}



bool CBuildFactoryHelper::Unserialize( Panel **ppPanel, CUtlBuffer &buf, const char *pFileName )
{
	DECLARE_DMX_CONTEXT();
	*ppPanel = NULL;
	CDmxElement *pRootElement;
	if ( !UnserializeDMX( buf, &pRootElement, pFileName ) )
	{
		Warning( "Unable to read panel %s! UtlBuffer is the wrong type!\n", pFileName );
		CleanupDMX( pRootElement );
		return false;
	}

	*ppPanel = UnserializeDmxElementPanel( pRootElement );
	if ( !*ppPanel )
	{
		Warning( "Unable to create panel %s!\n", pFileName );
		CleanupDMX( pRootElement );
		return false;
	}

	CleanupDMX( pRootElement );
	return true;     
}






