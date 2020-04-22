//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Singleton dialog that generates and presents the entity report.
//
//===========================================================================//

#include "particlesystemdefinitionbrowser.h"
#include "tier1/keyvalues.h"
#include "tier1/utlbuffer.h"
#include "iregistry.h"
#include "vgui/ivgui.h"
#include "vgui_controls/listpanel.h"
#include "vgui_controls/inputdialog.h"
#include "vgui_controls/messagebox.h"
#include "petdoc.h"
#include "pettool.h"
#include "datamodel/dmelement.h"
#include "vgui/keycode.h"
#include "dme_controls/dmecontrols_utils.h"
#include "dme_controls/particlesystempanel.h"
#include "matsys_controls/particlepicker.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

	
//-----------------------------------------------------------------------------
// Sort by particle system definition name
//-----------------------------------------------------------------------------
static int __cdecl ParticleSystemNameSortFunc( vgui::ListPanel *pPanel, const ListPanelItem &item1, const ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString("name");
	const char *string2 = item2.kv->GetString("name");
	return Q_stricmp( string1, string2 );
}


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CParticleSystemDefinitionBrowser::CParticleSystemDefinitionBrowser( CPetDoc *pDoc, vgui::Panel* pParent, const char *pName )
	: BaseClass( pParent, pName ), m_pDoc( pDoc )
{
	SetKeyBoardInputEnabled( true );
	SetPaintBackgroundEnabled( true );
	m_pSystemGrid = new CParticleSnapshotGrid( this, "SnapshotGrid" );
	m_pSystemGrid->AddActionSignalTarget(this);

	CBoxSizer *pBaseSizer = new CBoxSizer( ESLD_HORIZONTAL );

	{
		CBoxSizer *pDefinitionSizer = new CBoxSizer( ESLD_VERTICAL );
		pDefinitionSizer->AddPanel( new Label( this, "ParticleSystemsLabel", "Particle System Definitions:" ), SizerAddArgs_t() );
		pDefinitionSizer->AddPanel( m_pSystemGrid, SizerAddArgs_t().Expand( 1.0f ) );
		{
			CBoxSizer *pBottomRowSizer = new CBoxSizer( ESLD_HORIZONTAL );
			pBottomRowSizer->AddPanel( new Button( this, "SaveButton", "Save", this, "save" ), SizerAddArgs_t() );
			pBottomRowSizer->AddPanel( new Button( this, "SaveAndTestButton", "Save and Test", this, "SaveAndTest" ), SizerAddArgs_t() );
			pDefinitionSizer->AddSizer( pBottomRowSizer, SizerAddArgs_t() );
		}
		pBaseSizer->AddSizer( pDefinitionSizer, SizerAddArgs_t().Expand( 1.0f ) );
	}

	{
		CBoxSizer *pButtonColSizer = new CBoxSizer( ESLD_VERTICAL );
		m_pCreateButton = new Button( this, "CreateButton", "Create", this, "Create" );
		pButtonColSizer->AddPanel( m_pCreateButton, SizerAddArgs_t() );

		m_pDeleteButton = new Button( this, "DeleteButton", "Delete", this, "Delete" );
		m_pDeleteButton->SetEnabled(false);
		pButtonColSizer->AddPanel( m_pDeleteButton, SizerAddArgs_t() );

		m_pCopyButton = new Button( this, "CopyButton", "Duplicate", this, "Copy" );
		m_pCopyButton->SetEnabled(false);
		pButtonColSizer->AddPanel( m_pCopyButton, SizerAddArgs_t() );

		pBaseSizer->AddSizer( pButtonColSizer, SizerAddArgs_t() );
	}

	SetSizer( pBaseSizer );

	UpdateParticleSystemList();
}

CParticleSystemDefinitionBrowser::~CParticleSystemDefinitionBrowser()
{
	SaveUserConfig();
}


//-----------------------------------------------------------------------------
// Gets the ith selected particle system
//-----------------------------------------------------------------------------
CDmeParticleSystemDefinition* CParticleSystemDefinitionBrowser::GetSelectedParticleSystem( int i )
{
	if ( i < 0 || i >= m_pSystemGrid->GetSelectedSystemCount() )
		return NULL;

	int iSel = m_pSystemGrid->GetSelectedSystemId(i);
	return m_pDoc->GetParticleSystem(iSel);
}


//-----------------------------------------------------------------------------
// Purpose: Deletes the marked objects.
//-----------------------------------------------------------------------------
void CParticleSystemDefinitionBrowser::DeleteParticleSystems()
{
	{
		// This is undoable
		CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG|NOTIFY_FLAG_PARTICLESYS_ADDED_OR_REMOVED, "Delete Particle Systems", "Delete Particle Systems" );

		//
		// Build a list of objects to delete.
		//
		CUtlVector< CDmeParticleSystemDefinition* > itemsToDelete;
		int nCount = m_pSystemGrid->GetSelectedSystemCount();
		for (int i = 0; i < nCount; i++)
		{
			CDmeParticleSystemDefinition *pParticleSystem = GetSelectedParticleSystem( i );
			if ( pParticleSystem )
			{
				itemsToDelete.AddToTail( pParticleSystem );
			}
		}

		m_pSystemGrid->DeselectAll();

		nCount = itemsToDelete.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			m_pDoc->DeleteParticleSystemDefinition( itemsToDelete[i] );
		}

		g_pPetTool->SetCurrentParticleSystem( NULL, true );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleSystemDefinitionBrowser::OnKeyCodeTyped( vgui::KeyCode code )
{
	if ( code == KEY_DELETE ) 
	{
		DeleteParticleSystems();
	}
	else
	{
		BaseClass::OnKeyCodeTyped( code );
	}
}


//-----------------------------------------------------------------------------
// Called when the selection changes
//-----------------------------------------------------------------------------
void CParticleSystemDefinitionBrowser::UpdateParticleSystemSelection()
{
	if ( m_pSystemGrid->GetSelectedSystemCount() == 1 )
	{
		g_pPetTool->SetCurrentParticleSystem( m_pDoc->GetParticleSystem( m_pSystemGrid->GetSelectedSystemId(0) ), false );
	}
	else
	{
		g_pPetTool->SetCurrentParticleSystem( NULL, false );
	}
}


void CParticleSystemDefinitionBrowser::OnParticleSystemSelectionChanged( )
{
	UpdateParticleSystemSelection();

	bool bAnySelected = ( m_pSystemGrid->GetSelectedSystemCount() > 0 );
	m_pDeleteButton->SetEnabled( bAnySelected );
	m_pCopyButton->SetEnabled( bAnySelected );
}



//-----------------------------------------------------------------------------
// Select a particular node
//-----------------------------------------------------------------------------
void CParticleSystemDefinitionBrowser::SelectParticleSystem( CDmeParticleSystemDefinition *pFind )
{
	for ( int i = 0; i < m_pDoc->GetParticleSystemCount(); ++i )
	{
		if ( m_pDoc->GetParticleSystem(i) == pFind )
		{
			m_pSystemGrid->SelectId( i, false, false );
			break;
		}
	}
}

	
//-----------------------------------------------------------------------------
// Called when buttons are clicked
//-----------------------------------------------------------------------------
void CParticleSystemDefinitionBrowser::OnInputCompleted( KeyValues *pKeyValues )
{
	const char *pText = pKeyValues->GetString( "text", NULL );
	if ( m_pDoc->IsParticleSystemDefined( pText ) )
	{
		char pBuf[1024];
		Q_snprintf( pBuf, sizeof(pBuf), "Particle System \"%s\" already exists!\n", pText ); 
		vgui::MessageBox *pMessageBox = new vgui::MessageBox( "Duplicate Particle System Name!\n", pBuf, g_pPetTool->GetRootPanel() );
		pMessageBox->DoModal( );
		return;
	}

	if ( pKeyValues->FindKey( "create" ) )
	{
		CDmeParticleSystemDefinition *pParticleSystem = m_pDoc->AddNewParticleSystemDefinition( pText );
		g_pPetTool->SetCurrentParticleSystem( pParticleSystem );
	}
	else if ( pKeyValues->FindKey( "copy_one" ) || pKeyValues->FindKey( "copy_many" ) )
	{
		int nCount = m_pSystemGrid->GetSelectedSystemCount();
		
		if ( nCount == 1 )
		{
			CDmeParticleSystemDefinition *pParticleSystem = GetSelectedParticleSystem( 0 );
			CDmeParticleSystemDefinition * pNew = NULL;
			{
				CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG|NOTIFY_FLAG_PARTICLESYS_ADDED_OR_REMOVED, "Duplicate One Particle System", "Duplicate One Particle System" );
				pNew = CastElement<CDmeParticleSystemDefinition>( pParticleSystem->Copy( ) );
				pNew->SetName( pText );
				m_pDoc->AddNewParticleSystemDefinition( pNew, guard );
			}

			g_pPetTool->SetCurrentParticleSystem( pNew );
		}
		else if ( nCount > 1 )
		{
			CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG|NOTIFY_FLAG_PARTICLESYS_ADDED_OR_REMOVED, "Duplicate Multiple Particle Systems", "Duplicate Multiple Particle Systems" );

			CUtlVector<CDmeParticleSystemDefinition*> pNewSystems;

			for ( int i = 0; i < nCount; ++i )
			{
				CDmeParticleSystemDefinition *pParticleSystem = GetSelectedParticleSystem( i );
				CDmeParticleSystemDefinition *pNew = NULL;

				CUtlString newName = pParticleSystem->GetName();
				newName += pText;

				pNew = CastElement<CDmeParticleSystemDefinition>( pParticleSystem->Copy( ) );
				pNew->SetName( newName.Get() );

				pNewSystems.AddToTail(pNew);
			}

			Assert( pNewSystems.Count() == nCount );

			for ( int i = 0; i < nCount; ++i )
			{
				m_pDoc->AddNewParticleSystemDefinition( pNewSystems[i], guard );
			}

			g_pPetTool->SetCurrentParticleSystem( NULL );
		}
	}
}


//-----------------------------------------------------------------------------
// Copy to clipboard
//-----------------------------------------------------------------------------
void CParticleSystemDefinitionBrowser::OnCopy( )
{
	int nCount = m_pSystemGrid->GetSelectedSystemCount();

	CUtlVector< KeyValues * > list;
	CUtlRBTree< CDmeParticleSystemDefinition* > defs( 0, 0, DefLessFunc( CDmeParticleSystemDefinition* ) );
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeParticleSystemDefinition *pParticleSystem = GetSelectedParticleSystem( i );

		CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
		if ( g_pDataModel->Serialize( buf, "keyvalues2", "pcf", pParticleSystem->GetHandle() ) )
		{
			KeyValues *pData = new KeyValues( "Clipboard" );
			pData->SetString( PARTICLE_CLIPBOARD_DEFINITION_STR, (char*)buf.Base() );
			list.AddToTail( pData );
		}
	}

	if ( list.Count() )
	{
		g_pDataModel->SetClipboardData( list );
	}
}


//-----------------------------------------------------------------------------
// Paste from clipboard
//-----------------------------------------------------------------------------
void CParticleSystemDefinitionBrowser::ReplaceDef_r( CUndoScopeGuard& guard, CDmeParticleSystemDefinition *pDef )
{
	if ( !pDef )
		return;

	m_pDoc->ReplaceParticleSystemDefinition( pDef );
	int nChildCount = pDef->GetParticleFunctionCount( FUNCTION_CHILDREN );
	for ( int i = 0; i < nChildCount; ++i )
	{
		CDmeParticleChild *pChildFunction = static_cast< CDmeParticleChild* >( pDef->GetParticleFunction( FUNCTION_CHILDREN, i ) );
		CDmeParticleSystemDefinition* pChild = pChildFunction->m_Child;
		ReplaceDef_r( guard, pChild );
	}
}

void CParticleSystemDefinitionBrowser::PasteOperator( CUndoScopeGuard& guard, CDmeParticleFunction *pFunc )
{
	int nCount = m_pSystemGrid->GetSelectedSystemCount();

	for ( int i = 0; i < nCount; ++i )
	{
		CDmeParticleSystemDefinition *pParticleSystem = GetSelectedParticleSystem( i );
		pParticleSystem->AddCopyOfOperator( pFunc );
	}
}


void CParticleSystemDefinitionBrowser::PasteDefinitionBody( CUndoScopeGuard& guard, CDmeParticleSystemDefinition *pDef )
{
	int nCount = m_pSystemGrid->GetSelectedSystemCount();

	for ( int i = 0; i < nCount; ++i )
	{
		CDmeParticleSystemDefinition *pParticleSystem = GetSelectedParticleSystem( i );
		pParticleSystem->OverrideAttributesFromOtherDefinition( pDef );
	}
}

void CParticleSystemDefinitionBrowser::PasteFromClipboard( )
{
	// This is undoable
	CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Paste From Clipboard", "Paste From Clipboard" );

	bool bRefreshAll = false;
	CUtlVector< KeyValues * > list;
	g_pDataModel->GetClipboardData( list );
	int nItems = list.Count();
	for ( int i = 0; i < nItems; ++i )
	{
		CDmeParticleSystemDefinition *pDef = ReadParticleClassFromKV<CDmeParticleSystemDefinition>( list[i], PARTICLE_CLIPBOARD_DEFINITION_STR );
		if ( pDef )
		{
			ReplaceDef_r( guard, pDef );
			bRefreshAll = true;
			continue;
		}

		CDmeParticleFunction *pFunc = ReadParticleClassFromKV<CDmeParticleFunction>( list[i], PARTICLE_CLIPBOARD_FUNCTIONS_STR );
		if ( pFunc )
		{
			PasteOperator( guard, pFunc );
			bRefreshAll = true;
			continue;
		}

		pDef = ReadParticleClassFromKV<CDmeParticleSystemDefinition>( list[i], PARTICLE_CLIPBOARD_DEF_BODY_STR );
		if ( pDef )
		{
			PasteDefinitionBody( guard, pDef );
			bRefreshAll = true;
			continue;
		}
	}

	guard.Release();

	if ( bRefreshAll )
	{
		m_pDoc->UpdateAllParticleSystems();
	}
}


//-----------------------------------------------------------------------------
// Called when buttons are clicked
//-----------------------------------------------------------------------------
void CParticleSystemDefinitionBrowser::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "create" ) )
	{
		vgui::InputDialog *pInputDialog = new vgui::InputDialog( g_pPetTool->GetRootPanel(), "Enter Particle System Name", "Name:", "" );
		pInputDialog->SetSmallCaption( true );
		pInputDialog->SetMultiline( false );
		pInputDialog->AddActionSignalTarget( this );
		pInputDialog->DoModal( new KeyValues("create") );
		return;
	}
	if ( !Q_stricmp( pCommand, "copy" ) )
	{
		if ( m_pSystemGrid->GetSelectedSystemCount() == 1 )
		{
			CUtlString newName = m_pSystemGrid->GetSystemName(m_pSystemGrid->GetSelectedSystemId(0));
			newName += "_copy";

			vgui::InputDialog *pInputDialog = new vgui::InputDialog( g_pPetTool->GetRootPanel(), "Enter Duplicate System Name", "Name:", newName.Get() );
			pInputDialog->SetSmallCaption( true );
			pInputDialog->SetMultiline( false );
			pInputDialog->AddActionSignalTarget( this );
			pInputDialog->DoModal( new KeyValues("copy_one") );
		}
		else if ( m_pSystemGrid->GetSelectedSystemCount() > 1 )
		{
			vgui::InputDialog *pInputDialog = new vgui::InputDialog( g_pPetTool->GetRootPanel(), "Enter Suffix for New Systems", "Suffix:", "_copy" );
			pInputDialog->SetSmallCaption( true );
			pInputDialog->SetMultiline( false );
			pInputDialog->AddActionSignalTarget( this );
			pInputDialog->DoModal( new KeyValues("copy_many") );
		}

		return;
	}

	if ( !Q_stricmp( pCommand, "delete" ) )
	{
		DeleteParticleSystems();
		return;
	}

	if ( !Q_stricmp( pCommand, "Save" ) )
	{
		g_pPetTool->Save();
		return;
	}

	if ( !Q_stricmp( pCommand, "SaveAndTest" ) )
	{
		g_pPetTool->SaveAndTest();
		return;
	}

	BaseClass::OnCommand( pCommand );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleSystemDefinitionBrowser::UpdateParticleSystemList( bool bRetainSelection )
{
	/////////////////////////
	// build a list of previously selected systems
	CUtlVector< CUtlString > selectedItems;
	if ( bRetainSelection )
	{
		int nCount = m_pSystemGrid->GetSelectedSystemCount();
		for ( int i = 0; i < nCount; ++i )
		{
			CDmeParticleSystemDefinition *pParticleSystem = GetSelectedParticleSystem( i );
			if ( pParticleSystem )
			{
				selectedItems.AddToTail( pParticleSystem->GetName() );
			}
		}
	}

	/////////////////////////
	// now go nuts

	const CDmrParticleSystemList particleSystemList = m_pDoc->GetParticleSystemDefinitionList();
	if ( !particleSystemList.IsValid() )
	{
		m_pSystemGrid->SetParticleList( CUtlVector<const char *>() );
		return;
	}

	CUtlVector<const char *> systemNames;
	CUtlVector<int> selectionIndicies;

	/////////////////////////
	// populate the new list
	for ( int i = 0; i < particleSystemList.Count(); ++i )
	{
		CDmeParticleSystemDefinition *pParticleSystem = particleSystemList[i];
		if ( !pParticleSystem )
			continue;

		systemNames.AddToTail( pParticleSystem->GetName() );

		// see if the system was previously selected
		for ( int s = 0; s < selectedItems.Count(); ++s )
		{
			if( !V_strcmp(pParticleSystem->GetName(), selectedItems[s]) )
			{
				selectionIndicies.AddToTail(i);
			}
		}
	}

	m_pSystemGrid->SetParticleList( systemNames );

	/////////////////////////
	// reselect any identified systems
	if ( bRetainSelection )
	{
		for ( int i = 0; i < selectionIndicies.Count(); ++i )
		{
			m_pSystemGrid->SelectId( selectionIndicies[i], true, false );
		}
	}
}

