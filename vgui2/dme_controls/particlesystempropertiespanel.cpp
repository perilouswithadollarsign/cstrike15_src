//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Dialog used to edit properties of a particle system definition
//
//===========================================================================//

#include "dme_controls/ParticleSystemPropertiesPanel.h"
#include "tier1/keyvalues.h"
#include "tier1/utlbuffer.h"
#include "vgui/ivgui.h"
#include "vgui_controls/button.h"
#include "vgui_controls/listpanel.h"
#include "vgui_controls/splitter.h"
#include "vgui_controls/messagebox.h"
#include "vgui_controls/combobox.h"
#include "datamodel/dmelement.h"
#include "movieobjects/dmeparticlesystemdefinition.h"
#include "dme_controls/elementpropertiestree.h"
#include "matsys_controls/picker.h"
#include "dme_controls/dmecontrols_utils.h"
#include "dme_controls/particlesystempanel.h"
#include "dme_controls/dmepanel.h"


// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

class CParticleFunctionTree: public vgui::TreeView
{
	DECLARE_CLASS_SIMPLE( CParticleFunctionTree, vgui::TreeView );

public:
	CParticleFunctionTree(Panel *parent, const char *panelName): TreeView(parent, panelName)
	{

	}

	virtual void GenerateContextMenu( int itemIndex, int x, int y )
	{
		PostActionSignal( new KeyValues("OpenContextMenu", "itemID", itemIndex ));
	}
};


//-----------------------------------------------------------------------------
//
// Purpose: Picker for particle functions
//
//-----------------------------------------------------------------------------
class CParticleFunctionPickerFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CParticleFunctionPickerFrame, vgui::Frame );

public:
	CParticleFunctionPickerFrame( vgui::Panel *pParent, const char *pTitle );
	~CParticleFunctionPickerFrame();

	// Sets the current scene + animation list
	void DoModal( CDmeParticleSystemDefinition *pParticleSystem, ParticleFunctionType_t type, KeyValues *pContextKeyValues );

	// Inherited from Frame
	virtual void OnCommand( const char *pCommand );

private:
	// Refreshes the list of particle functions
	void RefreshParticleFunctions( CDmeParticleSystemDefinition *pDefinition, ParticleFunctionType_t type );
	void CleanUpMessage();

	MESSAGE_FUNC( OnTextChanged, "TextChanged" );

	vgui::ListPanel *m_pFunctionList;
	vgui::Button *m_pOpenButton;
	vgui::Button *m_pCancelButton;
	vgui::ComboBox *m_pFilterCombo;
	KeyValues *m_pContextKeyValues;
};

static int __cdecl ParticleFunctionNameSortFunc( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString( "name" );
	const char *string2 = item2.kv->GetString( "name" );
	return Q_stricmp( string1, string2 );
}

CParticleFunctionPickerFrame::CParticleFunctionPickerFrame( vgui::Panel *pParent, const char *pTitle ) : 
	BaseClass( pParent, "ParticleFunctionPickerFrame" )
{

	SetDeleteSelfOnClose( true );
	m_pContextKeyValues = NULL;

	m_pFunctionList = new vgui::ListPanel( this, "ParticleFunctionList" );
	m_pFunctionList->AddColumnHeader( 0, "name", "Particle Function Name", 52, 0 );
	m_pFunctionList->SetSelectIndividualCells( false );
	m_pFunctionList->SetMultiselectEnabled( false );
	m_pFunctionList->SetEmptyListText( "No particle functions" );
	m_pFunctionList->AddActionSignalTarget( this );
	m_pFunctionList->SetSortFunc( 0, ParticleFunctionNameSortFunc );
	m_pFunctionList->SetSortColumn( 0 );

	m_pOpenButton = new vgui::Button( this, "OkButton", "#MessageBox_OK", this, "Ok" );
	m_pCancelButton = new vgui::Button( this, "CancelButton", "#MessageBox_Cancel", this, "Cancel" );
	m_pFilterCombo = new ComboBox(this, "FilterByOperatorType", 0, false);

	SetBlockDragChaining( true );

	LoadControlSettingsAndUserConfig( "resource/particlefunctionpicker.res" );

	SetTitle( pTitle, false );
}

CParticleFunctionPickerFrame::~CParticleFunctionPickerFrame()
{
	CleanUpMessage();
}

//-----------------------------------------------------------------------------
// Refreshes the list of raw controls
//-----------------------------------------------------------------------------
void CParticleFunctionPickerFrame::RefreshParticleFunctions( CDmeParticleSystemDefinition *pParticleSystem, ParticleFunctionType_t type )
{
	m_pFunctionList->RemoveAll();	
	if ( !pParticleSystem )
		return;
	CUtlVector< IParticleOperatorDefinition *> &list = g_pParticleSystemMgr->GetAvailableParticleOperatorList( type );
	
	int nCount = list.Count();

	// Build a list of used operator IDs
	bool pUsedIDs[OPERATOR_ID_COUNT];
	memset( pUsedIDs, 0, sizeof(pUsedIDs) );

	uint32 allFilters = 0;

	int nFunctionCount = pParticleSystem->GetParticleFunctionCount( type );
	for ( int i = 0; i < nFunctionCount; ++i )
	{
		const char *pFunctionName = pParticleSystem->GetParticleFunction( type, i )->GetName();
		for ( int j = 0; j < nCount; ++j )
		{
			if ( Q_stricmp( pFunctionName, list[j]->GetName() ) )
				continue;

			if ( list[j]->GetId() >= 0 )
			{
				pUsedIDs[ list[j]->GetId() ] = true;
			}
			break;
		}
	}

	for ( int i = 0; i < nCount; ++i )
	{
		const char *pFunctionName = list[i]->GetName();
		uint32 filter = list[i]->GetFilter();
		bool disableItem = false;

		// Look to see if this is in a special operator group
		if ( list[i]->GetId() >= 0 )
		{
			// Disable ones that are already in the particle system
			if ( pUsedIDs[ list[i]->GetId() ] )
				disableItem = true;
		}

		if ( list[i]->GetId() == OPERATOR_SINGLETON )
		{
			// Disable ones that are already in the particle system
			if ( pParticleSystem->FindFunction( type, pFunctionName ) >= 0 ) 
				disableItem = true;
		}

		// Don't display obsolete operators
		if ( list[i]->IsObsolete() )
			continue;

		allFilters = allFilters | filter;
		KeyValues *kv = new KeyValues( "node", "name", pFunctionName );
		kv->SetInt( "typeNumber", type );
		kv->SetInt( "filters", filter );
		m_pFunctionList->AddItem( kv, 0, false, false );
		m_pFunctionList->SetItemDisabled( i, disableItem );
	}

	// Populate the combo box
	m_pFilterCombo->RemoveAll();

	for (int i = 0; i < FILTER_COUNT; ++i )
	{
		if ( (i == 0) || ( allFilters & ( 1 << i ) ) )
		{
			KeyValues *kv = new KeyValues( "Filter" );
			kv->SetInt( "filter", i);
			m_pFilterCombo->AddItem( g_pParticleSystemMgr->GetFilterName( (ParticleFilterType_t) i ), kv );
		}
	}

	m_pFilterCombo->ActivateItemByRow( 0 );

	m_pFunctionList->SortList();
}

//-----------------------------------------------------------------------------
// Filters the list
//-----------------------------------------------------------------------------
void CParticleFunctionPickerFrame::OnTextChanged( )
{
	m_pFunctionList->SetAllVisible( true );
	int nCount = m_pFunctionList->GetItemCount();
	uint32 filter = 1 << m_pFilterCombo->GetActiveItemUserData()->GetInt( "filter", 0 );
	if (filter != 1) 
	{
		for ( int i = 0; i < nCount; ++i )
		{
			// Hide items by filter type
			int currFilter = m_pFunctionList->GetItem(i)->GetInt( "filters", 0 );
			if ( ( (uint32) currFilter ) & filter )
			{
				m_pFunctionList->SetItemVisible( i, true );
			}
			else
			{
				m_pFunctionList->SetItemVisible( i, false );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Deletes the message
//-----------------------------------------------------------------------------
void CParticleFunctionPickerFrame::CleanUpMessage()
{
	if ( m_pContextKeyValues )
	{
		m_pContextKeyValues->deleteThis();
		m_pContextKeyValues = NULL;
	}
}


//-----------------------------------------------------------------------------
// Sets the current scene + animation list
//-----------------------------------------------------------------------------
void CParticleFunctionPickerFrame::DoModal( CDmeParticleSystemDefinition *pParticleSystem, ParticleFunctionType_t type, KeyValues *pContextKeyValues )
{
	CleanUpMessage();
	RefreshParticleFunctions( pParticleSystem, type );
	m_pContextKeyValues = pContextKeyValues;
	BaseClass::DoModal();
}


//-----------------------------------------------------------------------------
// On command
//-----------------------------------------------------------------------------
void CParticleFunctionPickerFrame::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "Ok" ) )
	{
		int nSelectedItemCount = m_pFunctionList->GetSelectedItemsCount();
		if ( nSelectedItemCount == 0 )
			return;

		Assert( nSelectedItemCount == 1 );
		int nItemID = m_pFunctionList->GetSelectedItem( 0 );
		KeyValues *pKeyValues = m_pFunctionList->GetItem( nItemID );

		if ( pKeyValues->GetInt( "disabled" ) == 1 )
			return;

		KeyValues *pActionKeys = new KeyValues( "ParticleFunctionPicked" );
		pActionKeys->SetString( "name", pKeyValues->GetString( "name" ) );
		pActionKeys->SetInt( "typeNumber", pKeyValues->GetInt("typeNumber") );

		if ( m_pContextKeyValues )
		{
			pActionKeys->AddSubKey( m_pContextKeyValues );

			// This prevents them from being deleted later
			m_pContextKeyValues = NULL;
		}

		PostActionSignal( pActionKeys );
		CloseModal();
		return;
	}

	if ( !Q_stricmp( pCommand, "Cancel" ) )
	{
		CloseModal();
		return;
	}

	BaseClass::OnCommand( pCommand );
}

//-----------------------------------------------------------------------------
//
// Purpose: Picker for child particle systems
//
//-----------------------------------------------------------------------------
class CParticleChildrenPickerFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CParticleChildrenPickerFrame, vgui::Frame );

public:
	CParticleChildrenPickerFrame( vgui::Panel *pParent, const char *pTitle, IParticleSystemPropertiesPanelQuery *pQuery );
	~CParticleChildrenPickerFrame();

	// Sets the current scene + animation list
	void DoModal( CDmeParticleSystemDefinition *pParticleSystem, KeyValues *pContextKeyValues );

	// Inherited from Frame
	virtual void OnCommand( const char *pCommand );

private:
	// Refreshes the list of children particle systems
	void RefreshChildrenList( CDmeParticleSystemDefinition *pDefinition );
	void CleanUpMessage();

	IParticleSystemPropertiesPanelQuery *m_pQuery;
	vgui::ListPanel *m_pChildrenList;
	vgui::Button *m_pOpenButton;
	vgui::Button *m_pCancelButton;
	KeyValues *m_pContextKeyValues;
};

static int __cdecl ParticleChildrenNameSortFunc( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	const char *string1 = item1.kv->GetString( "name" );
	const char *string2 = item2.kv->GetString( "name" );
	return Q_stricmp( string1, string2 );
}

CParticleChildrenPickerFrame::CParticleChildrenPickerFrame( vgui::Panel *pParent, const char *pTitle, IParticleSystemPropertiesPanelQuery *pQuery ) : 
	BaseClass( pParent, "ParticleChildrenPickerFrame" ), m_pQuery( pQuery )
{
	SetDeleteSelfOnClose( true );
	m_pContextKeyValues = NULL;

	m_pChildrenList = new vgui::ListPanel( this, "ParticleChildrenList" );
	m_pChildrenList->AddColumnHeader( 0, "name", "Particle System Name", 52, 0 );
	m_pChildrenList->SetSelectIndividualCells( false );
	m_pChildrenList->SetMultiselectEnabled( false );
	m_pChildrenList->SetEmptyListText( "No particle systems" );
	m_pChildrenList->AddActionSignalTarget( this );
	m_pChildrenList->SetSortFunc( 0, ParticleChildrenNameSortFunc );
	m_pChildrenList->SetSortColumn( 0 );

	m_pOpenButton = new vgui::Button( this, "OkButton", "#MessageBox_OK", this, "Ok" );
	m_pCancelButton = new vgui::Button( this, "CancelButton", "#MessageBox_Cancel", this, "Cancel" );
	SetBlockDragChaining( true );

	LoadControlSettingsAndUserConfig( "resource/particlechildrenpicker.res" );

	SetTitle( pTitle, false );
}

CParticleChildrenPickerFrame::~CParticleChildrenPickerFrame()
{
	CleanUpMessage();
}


//-----------------------------------------------------------------------------
// Refreshes the list of raw controls
//-----------------------------------------------------------------------------
void CParticleChildrenPickerFrame::RefreshChildrenList( CDmeParticleSystemDefinition *pCurrentParticleSystem )
{
	m_pChildrenList->RemoveAll();

	CUtlVector< CDmeParticleSystemDefinition* > definitions;
	if ( m_pQuery )
	{
		m_pQuery->GetKnownParticleDefinitions( definitions );
	}
	int nCount = definitions.Count();
	if ( nCount == 0 )
		return;

	for ( int i = 0; i < nCount; ++i )
	{
		CDmeParticleSystemDefinition *pParticleSystem = definitions[i];
		if ( pParticleSystem == pCurrentParticleSystem )
			continue;

		const char *pName = pParticleSystem->GetName();
		if ( !pName || !pName[0] )
		{
			pName = "<no name>";
		}

		KeyValues *kv = new KeyValues( "node" );
		kv->SetString( "name", pName ); 
		SetElementKeyValue( kv, "particleSystem", pParticleSystem );

		m_pChildrenList->AddItem( kv, 0, false, false );
	}
	m_pChildrenList->SortList();
}


//-----------------------------------------------------------------------------
// Deletes the message
//-----------------------------------------------------------------------------
void CParticleChildrenPickerFrame::CleanUpMessage()
{
	if ( m_pContextKeyValues )
	{
		m_pContextKeyValues->deleteThis();
		m_pContextKeyValues = NULL;
	}
}


//-----------------------------------------------------------------------------
// Sets the current scene + animation list
//-----------------------------------------------------------------------------
void CParticleChildrenPickerFrame::DoModal( CDmeParticleSystemDefinition *pParticleSystem, KeyValues *pContextKeyValues )
{
	CleanUpMessage();
	RefreshChildrenList( pParticleSystem );
	m_pContextKeyValues = pContextKeyValues;
	BaseClass::DoModal();
}


//-----------------------------------------------------------------------------
// On command
//-----------------------------------------------------------------------------
void CParticleChildrenPickerFrame::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "Ok" ) )
	{
		int nSelectedItemCount = m_pChildrenList->GetSelectedItemsCount();
		if ( nSelectedItemCount == 0 )
			return;

		Assert( nSelectedItemCount == 1 );
		int nItemID = m_pChildrenList->GetSelectedItem( 0 );
		KeyValues *pKeyValues = m_pChildrenList->GetItem( nItemID );
		CDmeParticleSystemDefinition *pParticleSystem = GetElementKeyValue<CDmeParticleSystemDefinition>( pKeyValues, "particleSystem" );

		KeyValues *pActionKeys = new KeyValues( "ParticleChildPicked" );
		SetElementKeyValue( pActionKeys, "particleSystem", pParticleSystem );

		if ( m_pContextKeyValues )
		{
			pActionKeys->AddSubKey( m_pContextKeyValues );

			// This prevents them from being deleted later
			m_pContextKeyValues = NULL;
		}

		PostActionSignal( pActionKeys );
		CloseModal();
		return;
	}

	if ( !Q_stricmp( pCommand, "Cancel" ) )
	{
		CloseModal();
		return;
	}

	BaseClass::OnCommand( pCommand );
}


//-----------------------------------------------------------------------------
// Browser of various particle functions
//-----------------------------------------------------------------------------
class CParticleFunctionBrowser : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CParticleFunctionBrowser, vgui::EditablePanel );

public:
	// constructor, destructor
	CParticleFunctionBrowser( vgui::Panel *pParent, const char *pName, IParticleSystemPropertiesPanelQuery *pQuery );
	virtual ~CParticleFunctionBrowser();

	// Inherited from Panel
	virtual void OnKeyCodeTyped( vgui::KeyCode code );

	void SetParticleFunctionProperties( CDmeElementPanel *pPanel );
	void SetParticleSystem( CDmeParticleSystemDefinition *pOp );
	void RefreshParticleFunctionList();
	void SelectDefaultFunction();

	// Called when a list panel's selection changes
	void RefreshParticleFunctionProperties( );

	MESSAGE_FUNC( DeleteSelectedFunctions, "Remove" );
	MESSAGE_FUNC( OnCopy, "OnCopy" );

private:
	MESSAGE_FUNC_PARAMS( OnOpenContextMenu, "OpenContextMenu", kv );
	MESSAGE_FUNC_PARAMS( OnItemSelected, "TreeViewItemSelected", kv );	
	MESSAGE_FUNC_PARAMS( OnItemDeselected, "TreeViewItemDeselected", kv );	
	MESSAGE_FUNC_PARAMS( OnAdd, "Add", kv );
	MESSAGE_FUNC( OnRename, "Rename" );
	MESSAGE_FUNC( OnMoveUp, "MoveUp" );
	MESSAGE_FUNC( OnMoveDown, "MoveDown" );
	MESSAGE_FUNC_PARAMS( OnInputCompleted, "InputCompleted", kv );
	MESSAGE_FUNC_PARAMS( OnParticleFunctionPicked, "ParticleFunctionPicked", kv );
	MESSAGE_FUNC_PARAMS( OnParticleChildPicked, "ParticleChildPicked", kv );
	MESSAGE_FUNC( OnPasteFuncs, "PasteFuncs" );

	void RefreshParticleFunctionSubtree( ParticleFunctionType_t nFunction, const CUtlString &SelectedFuncName );
	void ClearTree( );

	// Cleans up the context menu
	void CleanupContextMenu();

	// Returns the selected particle function
	CDmeParticleFunction* GetSelectedFunction( );
	ParticleFunctionType_t GetSelectedFunctionType( );
	bool IsPropertiesSlotSelected( bool bOnly );

	// Returns the selected particle function
	CDmeParticleFunction* GetSelectedFunction( int nIndex );
	ParticleFunctionType_t GetSelectedFunctionType( int nIndex );

	// Select a particular particle function
	void SelectParticleFunction( CDmeParticleFunction *pFind );

	IParticleSystemPropertiesPanelQuery *m_pQuery;
	CDmeHandle< CDmeParticleSystemDefinition > m_hParticleSystem;
	CParticleFunctionTree *m_pFunctionTree;
	vgui::DHANDLE< vgui::Menu > m_hContextMenu;
	CDmeElementPanel *m_pParticleFunctionProperties;
	int m_FunctionRootTreeIndicies[PARTICLE_FUNCTION_COUNT];
	int m_nPropertiesRootTreeIndex;
	int m_FunctionTreeRoot;
};


//-----------------------------------------------------------------------------
// Sort functions for list panel
//-----------------------------------------------------------------------------
static int __cdecl ParticleFunctionSortFunc( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	int i1 = item1.kv->GetInt( "index" );
	int i2 = item2.kv->GetInt( "index" );
	return i1 - i2;
}


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CParticleFunctionBrowser::CParticleFunctionBrowser( vgui::Panel *pParent, const char *pName, IParticleSystemPropertiesPanelQuery *pQuery ) :
	BaseClass( pParent, pName ), m_pQuery( pQuery )
{
	SetKeyBoardInputEnabled( true );

	m_pFunctionTree = new CParticleFunctionTree( this, "FunctionTree" );
	LoadControlSettings( "resource/particlefunctionbrowser.res" );
}

CParticleFunctionBrowser::~CParticleFunctionBrowser()
{
	CleanupContextMenu();
}


//-----------------------------------------------------------------------------
// Cleans up the context menu
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::CleanupContextMenu()
{
	if ( m_hContextMenu.Get() )
	{
		m_hContextMenu->MarkForDeletion();
		m_hContextMenu = NULL;
	}
}


//-----------------------------------------------------------------------------
// Selects a particular function by default
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::SelectDefaultFunction()
{
	if ( m_pFunctionTree->GetSelectedItemCount() == 0 && m_pFunctionTree->GetItemCount() > 0 )
	{
		m_pFunctionTree->AddSelectedItem( m_nPropertiesRootTreeIndex, true );
	}
}


//-----------------------------------------------------------------------------
// Sets the particle system	properties panel
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::SetParticleFunctionProperties( CDmeElementPanel *pPanel )
{
	m_pParticleFunctionProperties = pPanel;
}


//-----------------------------------------------------------------------------
// Sets/gets the particle system
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::SetParticleSystem( CDmeParticleSystemDefinition *pParticleSystem )
{
	if ( pParticleSystem != m_hParticleSystem.Get() )
	{
		m_hParticleSystem = pParticleSystem;
		RefreshParticleFunctionList();
		SelectDefaultFunction();
	}
}


//-----------------------------------------------------------------------------
// Builds the particle function list for the particle system
//-----------------------------------------------------------------------------

void CParticleFunctionBrowser::RefreshParticleFunctionSubtree( ParticleFunctionType_t nFunction, const CUtlString &SelectedFuncName )
{
	int nCount = m_hParticleSystem->GetParticleFunctionCount( nFunction );
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeParticleFunction *pFunction = m_hParticleSystem->GetParticleFunction( nFunction, i );
		KeyValues *kv = new KeyValues( "node", "name", pFunction->GetName() );
		kv->SetString( "text", pFunction->GetName() );
		kv->SetString( "type", pFunction->GetFunctionType() );
		kv->SetInt( "typeNumber", nFunction );
		kv->SetInt( "index", i );
		SetElementKeyValue( kv, "particleFunction", pFunction );
		int nItemID = m_pFunctionTree->AddItem( kv, m_FunctionRootTreeIndicies[nFunction] );
		m_pFunctionTree->SetItemFgColor( nItemID, Color(255, 255, 255, 255) );

		if ( SelectedFuncName == pFunction->GetName() )
		{
			m_pFunctionTree->AddSelectedItem( nItemID, true );
		}
	}
	m_pFunctionTree->ExpandItem(m_FunctionRootTreeIndicies[nFunction],true);
}

void CParticleFunctionBrowser::ClearTree( )
{
	m_pFunctionTree->RemoveAll();

	{
		KeyValues *pKV = new KeyValues("FunctionRoot");
		pKV->SetString( "Text", "System Properties" );
		pKV->SetInt( "typeNumber", -1 );
		pKV->SetBool( "isSystemProperties", true );
		pKV->SetInt( "index", -1 );
		m_nPropertiesRootTreeIndex = m_pFunctionTree->AddItem( pKV, -1 );
		m_pFunctionTree->SetItemFgColor( m_nPropertiesRootTreeIndex, Color(255, 255, 255, 255) );
		m_FunctionTreeRoot = m_nPropertiesRootTreeIndex;
	}

	for( int i = 0; i < PARTICLE_FUNCTION_COUNT; ++i )
	{
		KeyValues *pKV = new KeyValues("FunctionRoot");
		pKV->SetString( "Text", GetParticleFunctionTypeName(ParticleFunctionType_t(i)) );
		pKV->SetInt( "typeNumber", i );
		pKV->SetInt( "index", -1 );
		m_FunctionRootTreeIndicies[i] = m_pFunctionTree->AddItem( pKV, m_FunctionTreeRoot );
		m_pFunctionTree->SetItemFgColor( m_FunctionRootTreeIndicies[i], Color(96, 96, 96, 255) );
	}

	m_pFunctionTree->ExpandItem(m_FunctionTreeRoot,true);
}

void CParticleFunctionBrowser::RefreshParticleFunctionList()
{
	CDmeParticleFunction* pSelectedFunc = GetSelectedFunction();

	CUtlString selectedFuncName = "";
	if ( pSelectedFunc )
	{
		selectedFuncName = pSelectedFunc->GetName();
	}

	ClearTree();

	if ( !m_hParticleSystem.Get() )
	{
		return;
	}

	for ( int i = 0; i < PARTICLE_FUNCTION_COUNT; ++i )
	{
		RefreshParticleFunctionSubtree(ParticleFunctionType_t(i), selectedFuncName);
	}
}


//-----------------------------------------------------------------------------
// Returns the selected particle function
//-----------------------------------------------------------------------------
CDmeParticleFunction* CParticleFunctionBrowser::GetSelectedFunction( int nIndex )
{
	if ( !m_hParticleSystem.Get() )
		return NULL;

	int nSelectedItemCount = m_pFunctionTree->GetSelectedItemCount();
	if ( nSelectedItemCount <= nIndex )
		return NULL;

	int nItemID = m_pFunctionTree->GetSelectedItem(nIndex);
	KeyValues *pKeyValues = m_pFunctionTree->GetItemData( nItemID );
	return GetElementKeyValue<CDmeParticleFunction>( pKeyValues, "particleFunction" );
}

ParticleFunctionType_t CParticleFunctionBrowser::GetSelectedFunctionType( int nIndex )
{
	if ( !m_hParticleSystem.Get() )
		return PARTICLE_FUNCTION_COUNT; // invalid

	int nSelectedItemCount = m_pFunctionTree->GetSelectedItemCount();
	if ( nSelectedItemCount <= nIndex )
		return PARTICLE_FUNCTION_COUNT; // invalid

	int nItemID = m_pFunctionTree->GetSelectedItem( nIndex );
	KeyValues *pKeyValues = m_pFunctionTree->GetItemData( nItemID );
	return (ParticleFunctionType_t)pKeyValues->GetInt( "typeNumber" );
}

bool CParticleFunctionBrowser::IsPropertiesSlotSelected( bool bOnly )
{
	int nSelectedItemCount = m_pFunctionTree->GetSelectedItemCount();
	if ( bOnly && nSelectedItemCount != 1 )
		return false;

	for ( int i = 0; i < nSelectedItemCount; ++i )
	{
		int nItemID = m_pFunctionTree->GetSelectedItem(i);
		KeyValues *pKeyValues = m_pFunctionTree->GetItemData( nItemID );

		if ( pKeyValues->GetBool( "isSystemProperties" ) )
			return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Returns the selected particle function
//-----------------------------------------------------------------------------
CDmeParticleFunction* CParticleFunctionBrowser::GetSelectedFunction( )
{
	int nSelectedItemCount = m_pFunctionTree->GetSelectedItemCount();
	if ( nSelectedItemCount != 1 )
		return NULL;

	return GetSelectedFunction(0);
}

ParticleFunctionType_t CParticleFunctionBrowser::GetSelectedFunctionType( )
{
	int nSelectedItemCount = m_pFunctionTree->GetSelectedItemCount();
	if ( nSelectedItemCount != 1 )
		return PARTICLE_FUNCTION_COUNT; // invalid

	return GetSelectedFunctionType(0);
}

void CParticleFunctionBrowser::OnMoveUp( )
{
	CDmeParticleFunction* pFunction = GetSelectedFunction( );
	if ( !pFunction )
		return;

	ParticleFunctionType_t nType = GetSelectedFunctionType( );

	{
		CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Move Function Up", "Move Function Up" );
		m_hParticleSystem->MoveFunctionUp( nType, pFunction );
	}
	PostActionSignal( new KeyValues( "ParticleSystemModified" ) );
}

void CParticleFunctionBrowser::OnMoveDown( )
{
	CDmeParticleFunction* pFunction = GetSelectedFunction( );
	if ( !pFunction )
		return;

	ParticleFunctionType_t nType = GetSelectedFunctionType( );

	{
		CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Move Function Down", "Move Function Down" );
		m_hParticleSystem->MoveFunctionDown( nType, pFunction );
	}	
	PostActionSignal( new KeyValues( "ParticleSystemModified" ) );
}


//-----------------------------------------------------------------------------
// Select a particular particle function
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::SelectParticleFunction( CDmeParticleFunction *pFind )
{
	m_pFunctionTree->ClearSelection();
	for ( int nItemID = m_pFunctionTree->FirstItem(); nItemID != m_pFunctionTree->InvalidItemID(); nItemID = m_pFunctionTree->NextItem( nItemID ) )
	{
		KeyValues *kv = m_pFunctionTree->GetItemData( nItemID );
		CDmeParticleFunction *pFunction = GetElementKeyValue<CDmeParticleFunction>( kv, "particleFunction" );

		if ( pFunction == pFind )
		{
			m_pFunctionTree->AddSelectedItem( nItemID, true );
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Add/remove functions
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::OnParticleChildPicked( KeyValues *pKeyValues )
{
	CDmeParticleSystemDefinition *pParticleSystem = GetElementKeyValue<CDmeParticleSystemDefinition>( pKeyValues, "particleSystem" );
	if ( !pParticleSystem )
		return;

	CDmeParticleFunction *pFunction;
	{
		CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Add Particle System Child", "Add Particle System Child" );
		pFunction = m_hParticleSystem->AddChild( pParticleSystem );
	}
	PostActionSignal( new KeyValues( "ParticleSystemModified" ) );
	SelectParticleFunction( pFunction );
}

void CParticleFunctionBrowser::OnParticleFunctionPicked( KeyValues *pKeyValues )
{
	ParticleFunctionType_t nParticleFuncType = (ParticleFunctionType_t)pKeyValues->GetInt( "typeNumber" );
	const char *pParticleFuncName = pKeyValues->GetString( "name" );
	CDmeParticleFunction *pFunction;
	{
		CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Add Particle Function", "Add Particle Function" );
		pFunction = m_hParticleSystem->AddOperator( nParticleFuncType, pParticleFuncName );
	}
	PostActionSignal( new KeyValues( "ParticleSystemModified" ) );
	SelectParticleFunction( pFunction );
}

void CParticleFunctionBrowser::OnAdd( KeyValues *pKeyValues )
{
	ParticleFunctionType_t nParticleFuncType = (ParticleFunctionType_t)pKeyValues->GetInt( "typeNumber" );

	if ( nParticleFuncType != FUNCTION_CHILDREN )
	{
		CParticleFunctionPickerFrame *pPicker = new CParticleFunctionPickerFrame( this, "Select Particle Function" );
		pPicker->DoModal( m_hParticleSystem, nParticleFuncType, NULL );
	}
	else
	{
		CParticleChildrenPickerFrame *pPicker = new CParticleChildrenPickerFrame( this, "Select Child Particle Systems", m_pQuery );
		pPicker->DoModal( m_hParticleSystem, NULL );
	}
}

void CParticleFunctionBrowser::DeleteSelectedFunctions( )
{
	int iSel = m_pFunctionTree->GetSelectedItem( 0 );

	{
		CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Remove Particle Function", "Remove Particle Function" );

		//
		// Build a list of objects to delete.
		//
		CUtlVector< CDmeParticleFunction* > itemsToDelete;
		CUtlVector< ParticleFunctionType_t > typesToDelete;
		int nCount = m_pFunctionTree->GetSelectedItemCount();
		for (int i = 0; i < nCount; i++)
		{
			CDmeParticleFunction *pParticleFunction = GetSelectedFunction( i );
			if ( pParticleFunction )
			{
				itemsToDelete.AddToTail( pParticleFunction );
				typesToDelete.AddToTail( GetSelectedFunctionType(i) );
			}
		}

		nCount = itemsToDelete.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			m_hParticleSystem->RemoveFunction( typesToDelete[i], itemsToDelete[i] );
		}
	}

	PostActionSignal( new KeyValues( "ParticleSystemModified" ) );

	// Update the list box selection.
	if ( m_pFunctionTree->GetItemCount() > 0 )
	{
		m_pFunctionTree->AddSelectedItem( iSel, true );
	}
	else
	{
		m_pFunctionTree->ClearSelection();
	}
}


//-----------------------------------------------------------------------------
// Rename a particle function
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::OnInputCompleted( KeyValues *pKeyValues )
{
	const char *pName = pKeyValues->GetString( "text" );
	CDmeParticleFunction *pFunction = GetSelectedFunction();
	{
		CUndoScopeGuard guard( 0, NOTIFY_SETDIRTYFLAG, "Rename Particle Function", "Rename Particle Function" );
		pFunction->SetName( pName );
	}
	PostActionSignal( new KeyValues( "ParticleSystemModified" ) );
}


//-----------------------------------------------------------------------------
// Rename a particle function
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::OnRename()
{
	int nSelectedItemCount = m_pFunctionTree->GetSelectedItemCount();
	if ( nSelectedItemCount != 1 )
		return;

	vgui::InputDialog *pInput = new vgui::InputDialog( this, "Rename Particle Function", "Enter new name of particle function" );
	pInput->SetMultiline( false );
	pInput->DoModal( );
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::RefreshParticleFunctionProperties( )
{
	if ( IsVisible() )
	{
		CDmeParticleFunction *pFunction = GetSelectedFunction();
		if ( pFunction )
		{
			m_pParticleFunctionProperties->SetTypeDictionary( pFunction->GetEditorTypeDictionary() );
			m_pParticleFunctionProperties->SetDmeElement( pFunction );
		}
		else if( IsPropertiesSlotSelected(true) )
		{
			if ( m_hParticleSystem.Get() )
			{
				m_pParticleFunctionProperties->SetTypeDictionary( m_hParticleSystem->GetEditorTypeDictionary() );
			}

			m_pParticleFunctionProperties->SetDmeElement( m_hParticleSystem );
		}

		// Notify the outside world so we can get helpers to render correctly in the preview
		KeyValues *pMessage = new KeyValues( "ParticleFunctionSelChanged" );
		SetElementKeyValue( pMessage, "function", pFunction );
		PostActionSignal( pMessage );
	}
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::OnItemSelected( KeyValues *kv )
{
	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel == m_pFunctionTree )
	{
		RefreshParticleFunctionProperties();
		return;
	}
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::OnItemDeselected( KeyValues *kv )
{
	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel == m_pFunctionTree )
	{
		RefreshParticleFunctionProperties();
		return;
	}
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------

bool WouldPasteParticleFunctions()
{
	CUtlVector< KeyValues * > list;
	g_pDataModel->GetClipboardData( list );

	for ( int i = 0; i < list.Count(); ++i )
	{
		if ( V_strlen( list[i]->GetString( PARTICLE_CLIPBOARD_FUNCTIONS_STR ) ) )
		{
			return true;
		}
	}

	return false;
}

void CParticleFunctionBrowser::OnOpenContextMenu( KeyValues *kv )
{
	CleanupContextMenu();
	if ( !m_hParticleSystem.Get() )
		return;

	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel != m_pFunctionTree )
		return;

	int nSelectedItemCount = m_pFunctionTree->GetSelectedItemCount();
	m_hContextMenu = new vgui::Menu( this, "ActionMenu" );

	if ( nSelectedItemCount >= 1 )
	{
		m_hContextMenu->AddMenuItem( "#ParticleFunctionBrowser_Remove", new KeyValues( "Remove" ), this );
		m_hContextMenu->AddSeparator();
	}

	if ( nSelectedItemCount > 0 )
	{
		m_hContextMenu->AddMenuItem( "Copy Functions", new KeyValues( "OnCopy" ), this );
	}

	if ( WouldPasteParticleFunctions() )
	{
		m_hContextMenu->AddMenuItem( "Paste Functions", new KeyValues( "PasteFuncs" ), this );
	}

	if ( nSelectedItemCount == 1 )
	{
		m_hContextMenu->AddSeparator();
		int nType = m_pFunctionTree->GetItemData(m_pFunctionTree->GetSelectedItem(0))->GetInt("typeNumber");
		m_hContextMenu->AddMenuItem( "#ParticleFunctionBrowser_Add", new KeyValues( "Add", "typeNumber", nType ), this );

		// only rename/moveup/movedown for actual functions
		int nIndex = m_pFunctionTree->GetItemData(m_pFunctionTree->GetSelectedItem(0))->GetInt("index", -1);
		if ( nIndex != -1 )
		{
			m_hContextMenu->AddMenuItem( "#ParticleFunctionBrowser_Rename", new KeyValues( "Rename" ), this  );
			m_hContextMenu->AddSeparator();
			m_hContextMenu->AddMenuItem( "#ParticleFunctionBrowser_MoveUp", new KeyValues( "MoveUp" ), this );
			m_hContextMenu->AddMenuItem( "#ParticleFunctionBrowser_MoveDown", new KeyValues( "MoveDown" ), this );
		}
	}

	vgui::Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
}

void CParticleFunctionBrowser::OnPasteFuncs( )
{
	PostActionSignal( new KeyValues( "PasteFuncs" ) );
}

void CParticleFunctionBrowser::OnCopy( )
{
	int nSelectedItemCount = m_pFunctionTree->GetSelectedItemCount();

	CUtlVector< KeyValues * > list;
	for ( int i = 0; i < nSelectedItemCount; ++i )
	{
		CDmeParticleFunction* pSelectedFunc = GetSelectedFunction(i);

		if ( !pSelectedFunc )
			continue;

		DmFileId_t tmpFileId = pSelectedFunc->GetFileId();
		pSelectedFunc->SetFileId( DMFILEID_INVALID, TD_NONE );

		CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
		if ( g_pDataModel->Serialize( buf, "keyvalues2", "pcf", pSelectedFunc->GetHandle() ) )
		{
			KeyValues *pData = new KeyValues( "Clipboard" );
			pData->SetString( PARTICLE_CLIPBOARD_FUNCTIONS_STR, (char*)buf.Base() );
			list.AddToTail( pData );
		}

		pSelectedFunc->SetFileId( tmpFileId, TD_NONE );
	}

	if ( IsPropertiesSlotSelected(false) )
	{
		CDmeParticleSystemDefinition* pCopySys = CreateElement<CDmeParticleSystemDefinition>( "tempcopy", DMFILEID_INVALID );
		pCopySys->OverrideAttributesFromOtherDefinition( m_hParticleSystem );

		CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
		if ( g_pDataModel->Serialize( buf, "keyvalues2", "pcf", pCopySys->GetHandle() ) )
		{
			KeyValues *pData = new KeyValues( "Clipboard" );
			pData->SetString( PARTICLE_CLIPBOARD_DEF_BODY_STR, (char*)buf.Base() );
			list.AddToTail( pData );
		}

		g_pDataModel->DestroyElement(pCopySys->GetHandle());
	}

	if ( list.Count() )
	{
		g_pDataModel->SetClipboardData( list );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CParticleFunctionBrowser::OnKeyCodeTyped( vgui::KeyCode code )
{
	if ( code == KEY_DELETE ) 
	{
		DeleteSelectedFunctions();
	}
	else
	{
		BaseClass::OnKeyCodeTyped( code );
	}
}


//-----------------------------------------------------------------------------
// Strings
//-----------------------------------------------------------------------------

ConVar pet_sort_attributes( "pet_sort_attributes", "1", FCVAR_NONE );
//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CParticleSystemPropertiesPanel::CParticleSystemPropertiesPanel( IParticleSystemPropertiesPanelQuery *pQuery, vgui::Panel* pParent )
	: BaseClass( pParent, "ParticleSystemPropertiesPanel" ), m_pQuery( pQuery )
{
	SetKeyBoardInputEnabled( true );

	m_pSplitter = new vgui::Splitter( this, "Splitter", vgui::SPLITTER_MODE_VERTICAL, 1 );
	vgui::Panel *pSplitterLeftSide = m_pSplitter->GetChild( 0 );
	vgui::Panel *pSplitterRightSide = m_pSplitter->GetChild( 1 );

	m_pFunctionBrowserArea = new vgui::EditablePanel( pSplitterLeftSide, "FunctionBrowserArea" );

	m_pParticleFunctionProperties = new CDmeElementPanel( pSplitterRightSide, "FunctionProperties" );
	m_pParticleFunctionProperties->SetSortAttributesByName( pet_sort_attributes.GetBool() );
	m_pParticleFunctionProperties->SetAutoResize( vgui::Panel::PIN_TOPLEFT, vgui::Panel::AUTORESIZE_DOWNANDRIGHT, 6, 6, -6, -6 );
	m_pParticleFunctionProperties->AddActionSignalTarget( this );

	m_pParticleFunctionBrowser = new CParticleFunctionBrowser( m_pFunctionBrowserArea, "FunctionBrowser", m_pQuery );
	m_pParticleFunctionBrowser->SetParticleFunctionProperties( m_pParticleFunctionProperties );
	m_pParticleFunctionBrowser->AddActionSignalTarget( this );

	LoadControlSettings( "resource/particlesystempropertiespanel.res" );
}


//-----------------------------------------------------------------------------
// Sets the particle system to look at
//-----------------------------------------------------------------------------
void CParticleSystemPropertiesPanel::SetParticleSystem( CDmeParticleSystemDefinition *pParticleSystem )
{
	m_hParticleSystem = pParticleSystem;
	m_pParticleFunctionBrowser->SetParticleSystem( pParticleSystem );
}

CDmeParticleSystemDefinition *CParticleSystemPropertiesPanel::GetParticleSystem( )
{
	return m_hParticleSystem;
}

//-----------------------------------------------------------------------------
// Called when something changes in the dmeelement panel
//-----------------------------------------------------------------------------
void CParticleSystemPropertiesPanel::OnDmeElementChanged( KeyValues *pKeyValues )
{
	int nNotifyFlags = pKeyValues->GetInt( "notifyFlags" );
	OnParticleSystemModified();

	if ( nNotifyFlags & ( NOTIFY_CHANGE_TOPOLOGICAL | NOTIFY_CHANGE_ATTRIBUTE_ARRAY_SIZE ) )
	{
		m_pParticleFunctionBrowser->RefreshParticleFunctionList();
	}
	PostActionSignal( new KeyValues( "ParticleSystemModified" ) );
}

void CParticleSystemPropertiesPanel::OnParticleSystemModifiedInternal()
{
	OnParticleSystemModified();
	Refresh( false );
	PostActionSignal( new KeyValues( "ParticleSystemModified" ) );
}


void CParticleSystemPropertiesPanel::OnParticleFunctionSelChanged( KeyValues *pParams )
{
	// Notify the outside world so we can get helpers to render correctly in the preview
	CDmeParticleFunction *pFunction = GetElementKeyValue<CDmeParticleFunction>( pParams, "function" );
	KeyValues *pMessage = new KeyValues( "ParticleFunctionSelChanged" );
	SetElementKeyValue( pMessage, "function", pFunction );
	PostActionSignal( pMessage );
}

void CParticleSystemPropertiesPanel::OnCopy()
{
	m_pParticleFunctionBrowser->OnCopy();
}

void CParticleSystemPropertiesPanel::OnPasteFuncs()
{
	PostActionSignal( new KeyValues( "RequestPaste" ) );
}

//-----------------------------------------------------------------------------
// Refreshes display
//-----------------------------------------------------------------------------
void CParticleSystemPropertiesPanel::Refresh( bool bValuesOnly )
{
	m_pParticleFunctionBrowser->RefreshParticleFunctionList();
	m_pParticleFunctionProperties->Refresh( bValuesOnly ? CElementPropertiesTreeInternal::REFRESH_VALUES_ONLY :
		CElementPropertiesTreeInternal::REFRESH_TREE_VIEW );
}

void CParticleSystemPropertiesPanel::DeleteSelectedFunctions( )
{
	m_pParticleFunctionBrowser->DeleteSelectedFunctions();
}

//-----------------------------------------------------------------------------
// A little panel used as a DmePanel for particle systems
//-----------------------------------------------------------------------------
class CParticleSystemDmePanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CParticleSystemDmePanel, vgui::EditablePanel );

public:
	// constructor, destructor
	CParticleSystemDmePanel( vgui::Panel *pParent, const char *pName );
	virtual ~CParticleSystemDmePanel();

	// Set the material to draw
	void SetDmeElement( CDmeParticleSystemDefinition *pDef );

private:
	MESSAGE_FUNC_INT( OnElementChangedExternally, "ElementChangedExternally", valuesOnly );
	MESSAGE_FUNC( OnParticleSystemModified, "ParticleSystemModified" );
	MESSAGE_FUNC_PARAMS( OnParticleFunctionSelChanged, "ParticleFunctionSelChanged", params );

	MESSAGE_FUNC( OnPaste, "OnPaste" );
	MESSAGE_FUNC( OnRequestPaste, "RequestPaste" );
	KEYBINDING_FUNC_NODECLARE( edit_paste, KEY_V, vgui::MODIFIER_CONTROL, OnPaste, "#edit_paste_help", 0 );

	vgui::Splitter *m_Splitter;
	CParticleSystemPropertiesPanel *m_pProperties;
	CParticleSystemPreviewPanel *m_pPreview;
};


//-----------------------------------------------------------------------------
// Dme panel connection
//-----------------------------------------------------------------------------
IMPLEMENT_DMEPANEL_FACTORY( CParticleSystemDmePanel, DmeParticleSystemDefinition, "DmeParticleSystemDefinitionEditor", "Particle System Editor", true );

//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CParticleSystemDmePanel::CParticleSystemDmePanel( vgui::Panel *pParent, const char *pName ) :
	BaseClass( pParent, pName )
{
	m_Splitter = new vgui::Splitter( this, "Splitter", SPLITTER_MODE_HORIZONTAL, 1 );
	vgui::Panel *pSplitterTopSide = m_Splitter->GetChild( 0 );
	vgui::Panel *pSplitterBottomSide = m_Splitter->GetChild( 1 );
	m_Splitter->SetAutoResize( PIN_TOPLEFT, AUTORESIZE_DOWNANDRIGHT, 0, 0, 0, 0 );

	m_pProperties = new CParticleSystemPropertiesPanel( NULL, pSplitterBottomSide );
	m_pProperties->AddActionSignalTarget( this );
	m_pProperties->SetAutoResize( PIN_TOPLEFT, AUTORESIZE_DOWNANDRIGHT, 0, 0, 0, 0 );

	m_pPreview = new CParticleSystemPreviewPanel( pSplitterTopSide, "Preview" );
	m_pPreview->SetAutoResize( PIN_TOPLEFT, AUTORESIZE_DOWNANDRIGHT, 0, 0, 0, 0 );
}

CParticleSystemDmePanel::~CParticleSystemDmePanel()
{
}


//-----------------------------------------------------------------------------
// Layout
//-----------------------------------------------------------------------------
void CParticleSystemDmePanel::SetDmeElement( CDmeParticleSystemDefinition *pDef )
{
	m_pProperties->SetParticleSystem( pDef );
	m_pPreview->SetParticleSystem( pDef, true );
}


//-----------------------------------------------------------------------------
// Particle system modified	externally to the editor
//-----------------------------------------------------------------------------
void CParticleSystemDmePanel::OnElementChangedExternally( int valuesOnly )
{
	m_pProperties->Refresh( valuesOnly != 0 );
}


//-----------------------------------------------------------------------------
// Called when the selected particle function changes
//-----------------------------------------------------------------------------
void CParticleSystemDmePanel::OnParticleFunctionSelChanged( KeyValues *pParams )
{
	CDmeParticleFunction *pFunction = GetElementKeyValue<CDmeParticleFunction>( pParams, "function" );
	m_pPreview->SetParticleFunction( pFunction );
}


//-----------------------------------------------------------------------------
// Communicate to the owning DmePanel
//-----------------------------------------------------------------------------
void CParticleSystemDmePanel::OnParticleSystemModified()
{
	PostActionSignal( new KeyValues( "DmeElementChanged" ) );
}

//-----------------------------------------------------------------------------

void CParticleSystemDmePanel::OnRequestPaste()
{
	OnPaste();
}

void CParticleSystemDmePanel::OnPaste( )
{
	CDmeParticleSystemDefinition *pEditingDef = m_pProperties->GetParticleSystem();

	if ( !pEditingDef )
		return;

	CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Paste From Clipboard", "Paste From Clipboard" );
	CUtlVector< KeyValues * > list;
	g_pDataModel->GetClipboardData( list );
	int nItems = list.Count();

	for ( int i = 0; i < nItems; ++i )
	{
		CDmeParticleFunction *pFunc = ReadParticleClassFromKV<CDmeParticleFunction>( list[i], PARTICLE_CLIPBOARD_FUNCTIONS_STR );
		if ( pFunc )
		{
			pEditingDef->AddCopyOfOperator( pFunc );
			continue;
		}

		CDmeParticleSystemDefinition *pDef = ReadParticleClassFromKV<CDmeParticleSystemDefinition>( list[i], PARTICLE_CLIPBOARD_DEF_BODY_STR );
		if ( pDef )
		{
			pEditingDef->OverrideAttributesFromOtherDefinition( pDef );
			continue;
		}
	}

	guard.Release();
}
