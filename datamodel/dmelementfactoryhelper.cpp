//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "datamodel/dmelementfactoryhelper.h"
#include "tier0/dbg.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CDmElementFactoryHelper *CDmElementFactoryHelper::s_pHelpers[2] = { NULL, NULL };

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CDmElementFactoryHelper::CDmElementFactoryHelper( const char *classname, CDmElementFactoryInternal *pFactory, bool bIsStandardFactory )
	: m_pParent( NULL ), m_pChild( NULL ), m_pSibling( NULL )
{
	m_pNext = s_pHelpers[bIsStandardFactory];
	s_pHelpers[bIsStandardFactory] = this;

	// Set attributes
	Assert( pFactory );
	m_pFactory = pFactory;
	Assert( classname );
	m_pszClassname	= classname;
}


//-----------------------------------------------------------------------------
// Installs all factories into the datamodel system
//-----------------------------------------------------------------------------
// NOTE: The name of this extern is defined by the macro IMPLEMENT_ELEMENT_FACTORY 
extern CDmElementFactoryHelper g_CDmElement_Helper;

void CDmElementFactoryHelper::InstallFactories()
{
	static bool s_bInstalled = false;
	if ( s_bInstalled )
		return;
	s_bInstalled = true;

	// Just set up the type symbols of the other factories
	for ( CDmElementFactoryHelper *pHelper = s_pHelpers[ 0 ]; pHelper; pHelper = pHelper->GetNext() )
	{
		CDmElementFactoryInternal *pFactory = pHelper->GetFactory();

		g_pDataModel->AddElementFactory( pHelper );

		// Set up the type symbol. Note this can't be done at
		// constructor time since we don't have a DataModel pointer then
		pFactory->SetElementTypeSymbol( g_pDataModel->GetSymbol( pHelper->GetClassname() ) );
	}

	for ( CDmElementFactoryHelper *pHelper = s_pHelpers[ 1 ]; pHelper; pHelper = pHelper->GetNext() )
	{
		// Add factories to database, but not if they've been overridden
		if ( !g_pDataModel->HasElementFactory( pHelper->GetClassname() ) )
		{
			CDmElementFactoryInternal *pFactory = pHelper->GetFactory();

			g_pDataModel->AddElementFactory( pHelper );

			// Set up the type symbol. Note this can't be done at
			// constructor time since we don't have a DataModel pointer then

			// Backward compat--don't let the type symbol be 'DmeElement'
			if ( Q_stricmp( pHelper->GetClassname(), "DmeElement" ) )
			{
				pFactory->SetElementTypeSymbol( g_pDataModel->GetSymbol( pHelper->GetClassname() ) );
			}
		}
	}

	// Also install the DmElement factory as the default factory
	g_pDataModel->SetDefaultElementFactory( g_CDmElement_Helper.GetFactory() );

	for ( int i = 0; i < 2; ++i )
	{
		for ( CDmElementFactoryHelper *pHelper = s_pHelpers[ i ]; pHelper; pHelper = pHelper->GetNext() )
		{
			CDmElementFactoryInternal *pFactory = pHelper->GetFactory();
			CUtlSymbolLarge parentElementTypeSym = pFactory->GetParentElementTypeSymbol();
			if ( !parentElementTypeSym.IsValid() )
				continue; // helper has no parent, and therefore no sibling

			const char *pParentFactoryName = parentElementTypeSym.String();
			CDmElementFactoryHelper *pParent = g_pDataModel->GetElementFactoryHelper( pParentFactoryName );
			if ( !pParent )
				continue;

			const char *pClassName = pHelper->GetClassname();

			CDmElementFactoryHelper *pSibling = pParent->GetChild();
			if ( !pSibling || V_stricmp( pClassName, pSibling->GetClassname() ) < 0 )
			{
				pParent->m_pChild = pHelper;
				pHelper->m_pSibling = pSibling;
				Assert( pHelper->m_pSibling != pHelper );
			}
			else
			{
				while ( true )
				{
					CDmElementFactoryHelper *pNext = pSibling->GetSibling();
					if ( !pNext || V_stricmp( pClassName, pNext->GetClassname() ) < 0 )
						break;

					pSibling = pNext;
				}

				pHelper->m_pSibling = pSibling->m_pSibling;
				Assert( pHelper->m_pSibling != pHelper );
				pSibling->m_pSibling = pHelper;
				Assert( pSibling->m_pSibling != pSibling );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Installs all DmElement factories
//-----------------------------------------------------------------------------
void InstallDmElementFactories()
{
	CDmElementFactoryHelper::InstallFactories();
}




//-----------------------------------------------------------------------------
void CDmElementFactoryInternal::AddOnElementCreatedCallback( IDmeElementCreated *pCallback )
{
	if ( m_CallBackList.Find( pCallback ) == m_CallBackList.InvalidIndex() )
	{
		m_CallBackList.AddToTail( pCallback );
	}
}

void CDmElementFactoryInternal::RemoveOnElementCreatedCallback( IDmeElementCreated *pCallback )
{
	m_CallBackList.FindAndRemove( pCallback );
}

void CDmElementFactoryInternal::OnElementCreated( CDmElement* pElement )
{
	for ( int i = 0; i < m_CallBackList.Count(); i++ )
	{
		m_CallBackList[i]->OnElementCreated( pElement );
	}
}
	
