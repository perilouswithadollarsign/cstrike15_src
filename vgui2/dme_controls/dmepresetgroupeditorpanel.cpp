//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#include "dme_controls/dmepresetgroupeditorpanel.h"
#include "dme_controls/BaseAnimSetPresetFaderPanel.h"
#include "dme_controls/dmecontrols_utils.h"
#include "movieobjects/dmeanimationset.h"
#include "vgui_controls/ListPanel.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/PropertyPage.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/Splitter.h"
#include "vgui_controls/MessageBox.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/InputDialog.h"
#include "vgui_controls/TextEntry.h"
#include "vgui/MouseCode.h"
#include "vgui/IInput.h"
#include "vgui/ISurface.h"
#include "tier1/keyvalues.h"
#include "tier1/utldict.h"
#include "dme_controls/presetpicker.h"
#include "vgui_controls/FileOpenDialog.h"
#include "tier2/fileutils.h"
#include "tier1/utlbuffer.h"
#include "dme_controls/inotifyui.h"
#include "../game/shared/iscenetokenprocessor.h"
#include "studio.h"
#include "phonemeconverter.h"

// Forward declaration
class CDmePresetGroupEditorPanel;


//-----------------------------------------------------------------------------
// Utility scope guards
//-----------------------------------------------------------------------------
DEFINE_SOURCE_UNDO_SCOPE_GUARD( PresetGroup, NOTIFY_SOURCE_PRESET_GROUP_EDITOR );
DEFINE_SOURCE_NOTIFY_SCOPE_GUARD( PresetGroup, NOTIFY_SOURCE_PRESET_GROUP_EDITOR );

#define PRESET_FILE_FORMAT "preset"


//-----------------------------------------------------------------------------
//
// CDmePresetGroupListPanel
//
// Implementation below because of scoping issues
//
//-----------------------------------------------------------------------------
class CDmePresetGroupListPanel : public vgui::ListPanel
{
	DECLARE_CLASS_SIMPLE( CDmePresetGroupListPanel, vgui::ListPanel );

public:
	// constructor, destructor
	CDmePresetGroupListPanel( vgui::Panel *pParent, const char *pName, CDmePresetGroupEditorPanel *pComboPanel );

	virtual void OnCreateDragData( KeyValues *msg );
	virtual bool IsDroppable( CUtlVector< KeyValues * >& msgList );
	virtual void OnPanelDropped( CUtlVector< KeyValues * >& msgList );
	virtual void OnKeyCodeTyped( vgui::KeyCode code );
	virtual void OnMouseDoublePressed( vgui::MouseCode code );
	virtual void OnDroppablePanelPaint( CUtlVector< KeyValues * >& msglist, CUtlVector< Panel * >& dragPanels );

private:
	CDmePresetGroupEditorPanel *m_pPresetGroupPanel;
};


//-----------------------------------------------------------------------------
//
// CDmePresetListPanel
//
// Implementation below because of scoping issues
//
//-----------------------------------------------------------------------------
class CDmePresetListPanel : public vgui::ListPanel
{
	DECLARE_CLASS_SIMPLE( CDmePresetListPanel, vgui::ListPanel );

public:
	// constructor, destructor
	CDmePresetListPanel( vgui::Panel *pParent, const char *pName, CDmePresetGroupEditorPanel *pComboPanel );

	virtual void OnKeyCodeTyped( vgui::KeyCode code );
	virtual void OnCreateDragData( KeyValues *msg );
	virtual bool IsDroppable( CUtlVector< KeyValues * >& msgList );
	virtual void OnPanelDropped( CUtlVector< KeyValues * >& msgList );
	virtual void OnDroppablePanelPaint( CUtlVector< KeyValues * >& msglist, CUtlVector< Panel * >& dragPanels );

private:

	CDmePresetGroupEditorPanel *m_pPresetGroupPanel;
};


//-----------------------------------------------------------------------------
// Sort functions for list panel
//-----------------------------------------------------------------------------
static int __cdecl IndexSortFunc( vgui::ListPanel *pPanel, const vgui::ListPanelItem &item1, const vgui::ListPanelItem &item2 )
{
	int nIndex1 = item1.kv->GetInt("index");
	int nIndex2 = item2.kv->GetInt("index");
	return nIndex1 - nIndex2;
}


//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CDmePresetGroupEditorPanel::CDmePresetGroupEditorPanel( vgui::Panel *pParent, const char *pName ) :
	BaseClass( pParent, pName )
{
	m_pSplitter = new vgui::Splitter( this, "PresetGroupSplitter", vgui::SPLITTER_MODE_VERTICAL, 1 );
	vgui::Panel *pSplitterLeftSide = m_pSplitter->GetChild( 0 );
	vgui::Panel *pSplitterRightSide = m_pSplitter->GetChild( 1 );

	m_pPresetGroupList = new CDmePresetGroupListPanel( pSplitterLeftSide, "PresetGroupList", this );
	m_pPresetGroupList->AddColumnHeader( 0, "name", "Preset Group Name", 150, 0 );
	m_pPresetGroupList->AddColumnHeader( 1, "visible", "Visible", 70, 0 );
	m_pPresetGroupList->AddColumnHeader( 2, "shared", "Shared", 52, 0 );
	m_pPresetGroupList->AddColumnHeader( 3, "readonly", "Read Only", 52, 0 );
	m_pPresetGroupList->SetSelectIndividualCells( false );
	m_pPresetGroupList->SetMultiselectEnabled( false );
	m_pPresetGroupList->SetEmptyListText( "No preset groups" );
	m_pPresetGroupList->AddActionSignalTarget( this );
	m_pPresetGroupList->SetSortFunc( 0, IndexSortFunc );
	m_pPresetGroupList->SetSortFunc( 1, NULL );
	m_pPresetGroupList->SetColumnSortable( 1, false );
	m_pPresetGroupList->SetSortFunc( 2, NULL );
	m_pPresetGroupList->SetColumnSortable( 2, false );
	m_pPresetGroupList->SetSortFunc( 3, NULL );
	m_pPresetGroupList->SetColumnSortable( 3, false );
	m_pPresetGroupList->SetDropEnabled( true );
	m_pPresetGroupList->SetSortColumn( 0 );
	m_pPresetGroupList->SetDragEnabled( true );
	m_pPresetGroupList->SetDropEnabled( true );
	m_pPresetGroupList->SetIgnoreDoubleClick( true );

	m_pPresetList = new CDmePresetListPanel( pSplitterRightSide, "PresetList", this );
	m_pPresetList->AddColumnHeader( 0, "name", "Preset Name", 150, 0 );
	m_pPresetList->SetSelectIndividualCells( false );
	m_pPresetList->SetEmptyListText( "No presets" );
	m_pPresetList->AddActionSignalTarget( this );
	m_pPresetList->SetSortFunc( 0, IndexSortFunc );
	m_pPresetList->SetSortColumn( 0 );
	m_pPresetList->SetDragEnabled( true );
	m_pPresetList->SetDropEnabled( true );
	m_pPresetList->SetIgnoreDoubleClick( true );

	LoadControlSettingsAndUserConfig( "resource/dmepresetgroupeditorpanel.res" );

	m_hFileOpenStateMachine = new vgui::FileOpenStateMachine( this, this );
	m_hFileOpenStateMachine->AddActionSignalTarget( this );
}


CDmePresetGroupEditorPanel::~CDmePresetGroupEditorPanel()
{
	CleanupContextMenu();
	SaveUserConfig();
}

CDmeFilmClip *CDmePresetGroupEditorPanel::GetAnimationSetClip()
{
	return m_hFilmClip;
}

void CDmePresetGroupEditorPanel::SetAnimationSetClip( CDmeFilmClip *pFilmClip )
{
	m_hFilmClip = pFilmClip;
	RefreshAnimationSet();
}

//-----------------------------------------------------------------------------
// Cleans up the context menu
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::CleanupContextMenu()
{
	if ( m_hContextMenu.Get() )
	{
		m_hContextMenu->MarkForDeletion();
		m_hContextMenu = NULL;
	}
}


//-----------------------------------------------------------------------------
// Builds the preset group list for the animation set
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::RefreshAnimationSet()
{
	const char *pSelectedPresetGroupName = GetSelectedPresetGroupName();

	m_pPresetGroupList->RemoveAll();	
	if ( !m_hFilmClip.Get() )
		return;

	CUtlVector< PresetGroupInfo_t > presetGroupInfo;
	CollectPresetGroupInfo( m_hFilmClip, presetGroupInfo );

	int nCount = presetGroupInfo.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		PresetGroupInfo_t &info = presetGroupInfo[ i ];
		const char *pPresetGroupName = info.presetGroupSym.String();

		KeyValues *kv = new KeyValues( "node", "name", pPresetGroupName );
		kv->SetString( "presetGroupName", pPresetGroupName ); // TODO - determine if this extra copy of the groupname is necessary
		kv->SetString( "visible", info.bGroupVisible ? "Yes" : "No" ); 
		kv->SetString( "shared", info.bGroupShared ? "Yes" : "No" ); 
		kv->SetString( "readonly", info.bGroupReadOnly ? "Yes" : "No" ); 
		kv->SetColor( "cellcolor", info.bGroupReadOnly ? Color( 255, 0, 0, 255 ) : Color( 255, 255, 255, 255 ) ); 
		kv->SetInt( "index", i );
		int nItemID = m_pPresetGroupList->AddItem( kv, 0, false, false );

		if ( pSelectedPresetGroupName && !V_strcmp( pSelectedPresetGroupName, pPresetGroupName ) )
		{
			m_pPresetGroupList->AddSelectedItem( nItemID );
		}
	}

	m_pPresetGroupList->SortList();

	RefreshPresetNames();
}


//-----------------------------------------------------------------------------
// Tells any class that cares that the data in this thing has changed
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::NotifyDataChanged()
{
	PostActionSignal( new KeyValues( "PresetsChanged" ) );
}


//-----------------------------------------------------------------------------
// Refreshes the list of presets in the selected preset group
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::RefreshPresetNames()
{
	const char *pSelectedPresetName = GetSelectedPresetName();

	m_pPresetList->RemoveAll();	
	if ( !m_hFilmClip.Get() )
		return;

	const char *pPresetGroupName = GetSelectedPresetGroupName();
	if ( !pPresetGroupName )
		return;

	CUtlVector< CUtlSymbolLarge > presetNames;
	if ( !V_strcmp( pPresetGroupName, PROCEDURAL_PRESET_GROUP_NAME ) )
	{
		CollectProceduralPresetNames( presetNames );
	}
	else
	{
		CollectPresetNamesForGroup( m_hFilmClip, pPresetGroupName, presetNames );
	}

	int nPresets = presetNames.Count();
	for ( int i = 0; i < nPresets; ++i )
	{
		const char *pPresetName = presetNames[ i ].String();
		KeyValues *kv = new KeyValues( "node", "name", pPresetName );
		kv->SetString( "presetName", pPresetName ); // TODO - determine if this extra copy of the presetname is necessary
		kv->SetInt( "index", i );
		int nItemID = m_pPresetList->AddItem( kv, 0, false, false );
		if ( pSelectedPresetName && !V_strcmp( pSelectedPresetName, pPresetName ) )
		{
			m_pPresetList->AddSelectedItem( nItemID );
		}
	}

	m_pPresetList->SortList();
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
const char* CDmePresetGroupEditorPanel::GetSelectedPresetName()
{
	if ( !m_hFilmClip.Get() )
		return NULL;

	int nSelectedPresetCount = m_pPresetList->GetSelectedItemsCount();
	if ( nSelectedPresetCount != 1 )
		return NULL;

	int nItemID = m_pPresetList->GetSelectedItem( 0 );
	KeyValues *pKeyValues = m_pPresetList->GetItem( nItemID );
	return pKeyValues->GetString( "presetName" );
}


//-----------------------------------------------------------------------------
// Selects a particular preset 
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::SetSelectedPreset( const char* pPresetName )
{
	m_pPresetList->ClearSelectedItems();
	for ( int nItemID = m_pPresetList->FirstItem(); 
		nItemID != m_pPresetList->InvalidItemID(); 
		nItemID = m_pPresetList->NextItem( nItemID ) )
	{
		KeyValues* pKeyValues = m_pPresetList->GetItem( nItemID );
		if ( !V_strcmp( pKeyValues->GetString( "presetName" ), pPresetName ) )
		{
			m_pPresetList->AddSelectedItem( nItemID );
		}
	}
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
const char *CDmePresetGroupEditorPanel::GetSelectedPresetGroupName()
{
	if ( !m_hFilmClip.Get() )
		return NULL;

	int nSelectedItemCount = m_pPresetGroupList->GetSelectedItemsCount();
	if ( nSelectedItemCount != 1 )
		return NULL;

	int nItemID = m_pPresetGroupList->GetSelectedItem( 0 );
	KeyValues *pKeyValues = m_pPresetGroupList->GetItem( nItemID );
	return pKeyValues->GetString( "presetGroupName" );
}


//-----------------------------------------------------------------------------
// Selects a particular preset group
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::SetSelectedPresetGroup( const char* pPresetGroupName )
{
	m_pPresetGroupList->ClearSelectedItems();
	for ( int nItemID = m_pPresetGroupList->FirstItem(); 
		nItemID != m_pPresetGroupList->InvalidItemID(); 
		nItemID = m_pPresetGroupList->NextItem( nItemID ) )
	{
		KeyValues* pKeyValues = m_pPresetGroupList->GetItem( nItemID );
		if ( !V_strcmp( pKeyValues->GetString( "presetGroupName" ), pPresetGroupName ) )
		{
			m_pPresetGroupList->AddSelectedItem( nItemID );
		}
	}
}


//-----------------------------------------------------------------------------
// If it finds a duplicate preset name, reports an error message and returns it found one
//-----------------------------------------------------------------------------
bool CDmePresetGroupEditorPanel::HasDuplicatePresetName( const char *pPresetName, const char *pIgnorePresetName )
{
	const char *pPresetGroupName = GetSelectedPresetGroupName();
	if ( !pPresetGroupName )
		return false;

	if ( FindAnyPreset( m_hFilmClip, pPresetGroupName, pPresetName ) && V_strcmp( pPresetName, pIgnorePresetName ) )
	{
		vgui::MessageBox *pError = new vgui::MessageBox( "#DmePresetGroupEditor_DuplicatePresetNameTitle", "#DmePresetGroupEditor_DuplicatePresetNameText", this );
		pError->DoModal();
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Called by OnInputCompleted after we get a new group name
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::PerformRenamePreset( const char *pNewPresetName )
{
	const char *pPresetGroupName = GetSelectedPresetGroupName();
	if ( !pPresetGroupName )
		return;

	const char *pPresetName = GetSelectedPresetName();
	if ( !pPresetName )
		return;

	if ( HasDuplicatePresetName( pNewPresetName, pPresetName ) ) 
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Rename Preset" );
	CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		CDmePresetGroup *pPresetGroup = pAnimSet->FindPresetGroup( pPresetGroupName );
		if ( !pPresetGroup )
			continue;
		CDmePreset *pPreset = pPresetGroup->FindOrAddPreset( pPresetName );
		if ( !pPreset )
			continue;
		pPreset->SetName( pNewPresetName );
	}
	sg.Release();

	RefreshPresetNames();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Rename a preset 
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnRenamePreset()
{
	const char *pPresetName = GetSelectedPresetName();
	if ( !pPresetName )
		return;

	vgui::InputDialog *pInput = new vgui::InputDialog( this, "Rename Preset", "Enter new name of preset" );
	pInput->SetMultiline( false );
	pInput->DoModal( new KeyValues( "OnRenamePreset" ) );
}


//-----------------------------------------------------------------------------
// Remove a preset
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnRemovePreset()
{
	const char *pPresetGroupName = GetSelectedPresetGroupName();
	if ( !pPresetGroupName )
		return;

	const char *pPresetName = GetSelectedPresetName();
	if ( !pPresetName )
		return;

	int nItemID = m_pPresetList->GetSelectedItem( 0 );
	int nCurrentRow = m_pPresetList->GetItemCurrentRow( nItemID );

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Remove Preset" );
	CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		CDmePresetGroup *pPresetGroup = pAnimSet->FindPresetGroup( pPresetGroupName );
		if ( !pPresetGroup )
			continue;
		pPresetGroup->RemovePreset( pPresetName );
	}
	sg.Release();

	RefreshPresetNames();
	if ( nCurrentRow >= m_pPresetList->GetItemCount() )
	{
		--nCurrentRow;
	}
	if ( nCurrentRow >= 0 )
	{
		nItemID = m_pPresetList->GetItemIDFromRow( nCurrentRow );
		m_pPresetList->ClearSelectedItems();
		m_pPresetList->AddSelectedItem( nItemID );
	}
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnMovePresetUp()
{
	if ( m_pPresetList->GetSelectedItemsCount() != 1 )
		return;

	int nItemID = m_pPresetList->GetSelectedItem( 0 );
	int nCurrentRow = m_pPresetList->GetItemCurrentRow( nItemID );
	int nPrevItemID = m_pPresetList->GetItemIDFromRow( nCurrentRow - 1 );
	if ( nPrevItemID < 0 )
		return;

	KeyValues *pKeyValues = m_pPresetList->GetItem( nItemID );
	KeyValues *pPrevKeyValues = m_pPresetList->GetItem( nPrevItemID );

	MovePresetInFrontOf( pKeyValues->GetString( "presetName" ), pPrevKeyValues->GetString( "presetName" ) );
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnMovePresetDown()
{
	if ( m_pPresetList->GetSelectedItemsCount() != 1 )
		return;

	int nItemID = m_pPresetList->GetSelectedItem( 0 );
	int nCurrentRow = m_pPresetList->GetItemCurrentRow( nItemID );
	int nNextItemID = m_pPresetList->GetItemIDFromRow( nCurrentRow + 1 );
	if ( nNextItemID < 0 )
		return;

	KeyValues *pKeyValues = m_pPresetList->GetItem( nItemID );
	KeyValues *pNextKeyValues = m_pPresetList->GetItem( nNextItemID );

	MovePresetInFrontOf( pNextKeyValues->GetString( "presetName" ), pKeyValues->GetString( "presetName" ) );
}


//-----------------------------------------------------------------------------
// Drag/drop reordering of presets
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::MovePresetInFrontOf( const char *pDragPresetName, const char *pDropPresetName )
{
	const char *pPresetGroupName = GetSelectedPresetGroupName();
	if ( !pPresetGroupName )
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Reorder Presets" );
	CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		CDmePresetGroup *pPresetGroup = pAnimSet->FindPresetGroup( pPresetGroupName );
		if ( !pPresetGroup )
			continue;

		CDmePreset *pDragPreset = pPresetGroup->FindPreset( pDragPresetName );
		if ( !pDragPreset )
			continue;

		CDmePreset *pDropPreset = pPresetGroup->FindPreset( pDropPresetName );
		pPresetGroup->MovePresetInFrontOf( pDragPreset, pDropPreset );
	}
	sg.Release();

	RefreshPresetNames();
	SetSelectedPreset( pDragPresetName );
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Fileopen state machine
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnFileStateMachineFinished( KeyValues *pParams )
{
	KeyValues *pContextKeyValues = pParams->GetFirstTrueSubKey();
	if ( Q_stricmp( pContextKeyValues->GetName(), "ImportPresets" ) )
		return;

	CDmElement *pRoot = GetElementKeyValue<CDmElement>( pContextKeyValues, "presets" );
	if ( !pRoot )
		return;

	if ( pParams->GetInt( "completionState", 0 ) != 0 )
	{
		CPresetPickerFrame *pPresetPicker = new CPresetPickerFrame( this, "Select Preset(s) to Import" );
		pPresetPicker->AddActionSignalTarget( this );
		KeyValues *pContextKeyValues = new KeyValues( "ImportPicked" );
		SetElementKeyValue( pContextKeyValues, "presets", pRoot );
		pPresetPicker->DoModal( pRoot, true, pContextKeyValues );
	}
	else
	{
		// Clean up the read-in file
		CDisableUndoScopeGuard sg;
		g_pDataModel->RemoveFileId( pRoot->GetFileId() );
	}
}

//-----------------------------------------------------------------------------
// Fileopen state machine
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	if ( bOpenFile )
	{
		pDialog->SetTitle( "Import Preset File", true );
	}
	else
	{
		pDialog->SetTitle( "Export Preset File", true );
	}

	char pPresetPath[MAX_PATH];
	if ( !Q_stricmp( pFileFormat, PRESET_FILE_FORMAT ) )
	{
		GetModSubdirectory( "models", pPresetPath, sizeof(pPresetPath) );
		pDialog->SetStartDirectoryContext( "preset_importexport", pPresetPath );
		pDialog->AddFilter( "*.*", "All Files (*.*)", false );
		pDialog->AddFilter( "*.pre", "Preset File (*.pre)", true, PRESET_FILE_FORMAT );
	}
	else if ( !Q_stricmp( pFileFormat, "vfe" ) )
	{
		GetModSubdirectory( "expressions", pPresetPath, sizeof(pPresetPath) );
		pDialog->SetStartDirectoryContext( "preset_exportvfe", pPresetPath );
		pDialog->AddFilter( "*.*", "All Files (*.*)", false );
		pDialog->AddFilter( "*.vfe", "Expression File (*.vfe)", true, "vfe" );
	}
	else if ( !Q_stricmp( pFileFormat, "txt" ) )
	{
		GetModSubdirectory( "expressions", pPresetPath, sizeof(pPresetPath) );
		pDialog->SetStartDirectoryContext( "preset_exportvfe", pPresetPath );
		pDialog->AddFilter( "*.*", "All Files (*.*)", false );
		pDialog->AddFilter( "*.txt", "Faceposer Expression File (*.txt)", true, "txt" );
	}
}

bool CDmePresetGroupEditorPanel::OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	CDmElement *pRoot;
	CDisableUndoScopeGuard sg;
	DmFileId_t fileId = g_pDataModel->RestoreFromFile( pFileName, NULL, pFileFormat, &pRoot, CR_FORCE_COPY );
	sg.Release();

	if ( fileId == DMFILEID_INVALID )
		return false;

	// When importing an entire group, we can do it all right here
	if ( !Q_stricmp( pContextKeyValues->GetName(), "ImportPresetGroup" ) )
	{
		CDmePresetGroup *pPresetGroup = CastElement< CDmePresetGroup >( pRoot );
		if ( !pPresetGroup )
			return false;

		// TODO - we should be storing which animationset an item is associated with
		CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
		CDmeAnimationSet *pAnimSet = traversal.Next();
		Assert( !traversal.IsValid() );

		// TODO - make copy if shared bit is set?

		CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Import Preset Group" );
		pPresetGroup->SetFileId( pAnimSet->GetFileId(), TD_DEEP );
		pAnimSet->RemovePresetGroup( pPresetGroup->GetName() );
		pAnimSet->GetPresetGroups().AddToTail( pPresetGroup );
		sg.Release();

		RefreshAnimationSet();
		NotifyDataChanged();
		return true;
	}

	CDmAttribute* pPresets = pRoot->GetAttribute( "presets", AT_ELEMENT_ARRAY );
	if ( !pPresets )
		return false;

	SetElementKeyValue( pContextKeyValues, "presets", pRoot );
	return true;
}

bool CDmePresetGroupEditorPanel::OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues )
{
	const char *pPresetGroupName = pContextKeyValues->GetString( "presetGroupName" );
	CDmeAnimationSet *pAnimSet = GetElementKeyValue< CDmeAnimationSet >( pContextKeyValues, "animSet" );
	CDmePresetGroup *pPresetGroup = ( pAnimSet && pPresetGroupName ) ? pAnimSet->FindPresetGroup( pPresetGroupName ) : NULL;

	// Used when exporting an entire preset group
	if ( !Q_stricmp( pContextKeyValues->GetName(), "ExportPresetGroup" ) )
	{
		if ( !pPresetGroup )
			return false;

		bool bOk = g_pDataModel->SaveToFile( pFileName, NULL, g_pDataModel->GetDefaultEncoding( pFileFormat ), pFileFormat, pPresetGroup );
		return bOk;
	}

	// Used when exporting an entire preset group
	if ( !Q_stricmp( pContextKeyValues->GetName(), "ExportPresetGroupToVFE" ) )
	{
		if ( !pPresetGroup )
			return false;

		bool bOk = pPresetGroup->ExportToVFE( pFileName, pAnimSet );
		return bOk;
	}

	// Used when exporting an entire preset group
	if ( !Q_stricmp( pContextKeyValues->GetName(), "ExportPresetGroupToTXT" ) )
	{
		if ( !pPresetGroup )
			return false;

		bool bOk = pPresetGroup->ExportToTXT( pFileName, pAnimSet );
		return bOk;
	}

	// Used when exporting a subset of a preset group
	int nCount = pContextKeyValues->GetInt( "count" );
	if ( nCount == 0 )
		return true;

	Assert( pPresetGroupName == NULL );
	pPresetGroupName = GetSelectedPresetGroupName();
	if ( !pPresetGroupName )
	{
		pPresetGroupName = "root";
	}

	CDisableUndoScopeGuard sg;
	CDmePresetGroup *pRoot = CreateElement< CDmePresetGroup >( pPresetGroupName, DMFILEID_INVALID );
	CDmaElementArray< CDmePreset >& presets = pRoot->GetPresets( );

	// TODO - we should be storing which animationset an item is associated with
	CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
	pAnimSet = traversal.Next();
	Assert( !traversal.IsValid() );

	CDmePresetGroup *pSrcPresetGroup = pAnimSet->FindPresetGroup( pPresetGroupName );

	// Build list of selected presets 
	for ( int i = 0; i < nCount; ++i )
	{
		char pBuf[32];
		Q_snprintf( pBuf, sizeof(pBuf), "%d", i );
		const char *pPresetName = pContextKeyValues->GetString( pBuf );
		CDmePreset *pPreset = pSrcPresetGroup->FindPreset( pPresetName );
		if ( !pPreset )
			continue;

		presets.AddToTail( pPreset );
	}

	bool bOk = g_pDataModel->SaveToFile( pFileName, NULL, g_pDataModel->GetDefaultEncoding( pFileFormat ), pFileFormat, pRoot );
	g_pDataModel->DestroyElement( pRoot->GetHandle() );
	return bOk;
}


//-----------------------------------------------------------------------------
// Called when preset picking is cancelled
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnPresetPickCancelled( KeyValues *pParams )
{
	KeyValues *pContextKeyValues = pParams->FindKey( "ImportPicked" );
	if ( pContextKeyValues )
	{
		// Clean up the read-in file
		CDisableUndoScopeGuard sg;
		CDmElement *pRoot = GetElementKeyValue<CDmElement>( pContextKeyValues, "presets" );
		g_pDataModel->RemoveFileId( pRoot->GetFileId() );
		return;
	}
}


//-----------------------------------------------------------------------------
// Actually imports the presets from a file
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::ImportPresets( CUtlVector< const char * >& presetNames, CDmElement *pRoot )
{
	// TODO - we should be storing which animationset an item is associated with
	CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
	CDmeAnimationSet *pAnimSet = traversal.Next();
	Assert( !traversal.IsValid() );

	CDmePresetGroup *pPresetGroup = pAnimSet->FindPresetGroup( GetSelectedPresetGroupName() );
	if ( !pPresetGroup )
		return;

	CDmrElementArray< CDmePreset > srcPresets( pRoot->GetAttribute( "presets" ) );
	if ( !srcPresets.IsValid() || srcPresets.Count() == 0 )
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Import Presets" );

	int nPresetCount = presetNames.Count();
	for ( int i = 0; i < nPresetCount; ++i )
	{
		const char *pPresetName = presetNames[i];
		CDmePreset *pPreset = pPresetGroup->FindOrAddPreset( pPresetName );

		CDmePreset *pSrcPreset = NULL;
		int nSrcPresets = srcPresets.Count();
		for ( int j = 0; j < nSrcPresets; ++j )
		{
			CDmePreset *p = srcPresets[ j ];
			if ( p && !V_strcmp( p->GetName(), pPresetName ) )
			{
				pSrcPreset = p;
				break;
			}
		}

		const CDmaElementArray< CDmElement > &srcValues = pSrcPreset->GetControlValues();
		CDmaElementArray< CDmElement > &values = pPreset->GetControlValues( );
		values.RemoveAll();

		int nValueCount = srcValues.Count();
		for ( int j = 0; j < nValueCount; ++j )
		{
			CDmElement *pSrcControlValue = srcValues[j];
			CDmElement *pControlValue = pSrcControlValue->Copy( );
			pControlValue->SetFileId( pPresetGroup->GetFileId(), TD_DEEP );
			values.AddToTail( pControlValue );
		}
	}

	RefreshAnimationSet();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// The 'export presets' context menu option
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnPresetPicked( KeyValues *pParams )
{
	CUtlVector< const char * > presetNames;
	int nCount = pParams->GetInt( "count" );
	if ( nCount == 0 )
		return;

	// Build list of selected presets 
	for ( int i = 0; i < nCount; ++i )
	{
		char pBuf[32];
		Q_snprintf( pBuf, sizeof(pBuf), "%d", i );
		const char *pPresetName = pParams->GetString( pBuf );
		presetNames.AddToTail( pPresetName );
	}

	if ( pParams->FindKey( "ExportPicked" ) )
	{
		KeyValues *pContextKeyValues = new KeyValues( "ExportPresets" );
		pContextKeyValues->SetInt( "count", nCount );
		for ( int i = 0; i < nCount; ++i )
		{
			char pBuf[32];
			Q_snprintf( pBuf, sizeof(pBuf), "%d", i );
			pContextKeyValues->SetString( pBuf, presetNames[ i ] );
		}

		m_hFileOpenStateMachine->SaveFile( pContextKeyValues, NULL, PRESET_FILE_FORMAT, vgui::FOSM_SHOW_PERFORCE_DIALOGS );
		return;
	}

	KeyValues *pContextKeyValues = pParams->FindKey( "ImportPicked" );
	if ( pContextKeyValues )
	{
		CDmElement *pRoot = GetElementKeyValue< CDmElement >( pContextKeyValues, "presets" );
		ImportPresets( presetNames, pRoot );

		// Clean up the read-in file
		{
			CDisableUndoScopeGuard sg;
			CDmElement *pRoot = GetElementKeyValue<CDmElement>( pContextKeyValues, "presets" );
			g_pDataModel->RemoveFileId( pRoot->GetFileId() );
			return;
		}
	}
}


//-----------------------------------------------------------------------------
// The 'export presets' context menu option
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnExportPresets()
{
	// TODO - we should be storing which animationset an item is associated with
	CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
	CDmeAnimationSet *pAnimSet = traversal.Next();
	Assert( !traversal.IsValid() );

	CDmePresetGroup *pPresetGroup = pAnimSet->FindPresetGroup( GetSelectedPresetGroupName() );
	if ( !pPresetGroup )
		return;

	CPresetPickerFrame *pPresetPicker = new CPresetPickerFrame( this, "Select Preset(s) to Export" );
	pPresetPicker->AddActionSignalTarget( this );
	pPresetPicker->DoModal( pPresetGroup, true, new KeyValues( "ExportPicked" ) );
}


//-----------------------------------------------------------------------------
// The 'import presets' context menu option
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnImportPresets()
{
	KeyValues *pContextKeyValues = new KeyValues( "ImportPresets" );
	m_hFileOpenStateMachine->OpenFile( PRESET_FILE_FORMAT, pContextKeyValues );
}


//-----------------------------------------------------------------------------
// The 'export preset groups to VFE' context menu option
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnExportPresetGroupToVFE()
{
	// TODO - we should be storing which animationset an item is associated with
	CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
	CDmeAnimationSet *pAnimSet = traversal.Next();
	Assert( !traversal.IsValid() );

	const char *pPresetGroupName = GetSelectedPresetGroupName();
	if ( !pAnimSet->FindPresetGroup( pPresetGroupName ) )
		return;

	KeyValues *pContextKeyValues = new KeyValues( "ExportPresetGroupToVFE" );
	SetElementKeyValue( pContextKeyValues, "animSet", pAnimSet );
	pContextKeyValues->SetString( "presetGroupName", pPresetGroupName );
	m_hFileOpenStateMachine->SaveFile( pContextKeyValues, NULL, "vfe", vgui::FOSM_SHOW_PERFORCE_DIALOGS );
}


//-----------------------------------------------------------------------------
// The 'export preset groups to TXT' context menu option
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnExportPresetGroupToTXT()
{
	// TODO - we should be storing which animationset an item is associated with
	CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
	CDmeAnimationSet *pAnimSet = traversal.Next();
	Assert( !traversal.IsValid() );

	const char *pPresetGroupName = GetSelectedPresetGroupName();
	if ( !pAnimSet->FindPresetGroup( pPresetGroupName ) )
		return;

	KeyValues *pContextKeyValues = new KeyValues( "ExportPresetGroupToTXT" );
	SetElementKeyValue( pContextKeyValues, "animSet", pAnimSet );
	pContextKeyValues->SetString( "presetGroupName", pPresetGroupName );
	m_hFileOpenStateMachine->SaveFile( pContextKeyValues, NULL, "txt", vgui::FOSM_SHOW_PERFORCE_DIALOGS );
}


//-----------------------------------------------------------------------------
// The 'export preset groups' context menu option
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnExportPresetGroups()
{
	// TODO - we should be storing which animationset an item is associated with
	CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
	CDmeAnimationSet *pAnimSet = traversal.Next();
	Assert( !traversal.IsValid() );

	const char *pPresetGroupName = GetSelectedPresetGroupName();
	if ( !pAnimSet->FindPresetGroup( pPresetGroupName ) )
		return;

	KeyValues *pContextKeyValues = new KeyValues( "ExportPresetGroup" );
	SetElementKeyValue( pContextKeyValues, "animSet", pAnimSet );
	pContextKeyValues->SetString( "presetGroupName", pPresetGroupName );
	m_hFileOpenStateMachine->SaveFile( pContextKeyValues, NULL, PRESET_FILE_FORMAT, vgui::FOSM_SHOW_PERFORCE_DIALOGS );
}


//-----------------------------------------------------------------------------
// The 'import preset groups' context menu option
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnImportPresetGroups()
{
	KeyValues *pContextKeyValues = new KeyValues( "ImportPresetGroup" );
	m_hFileOpenStateMachine->OpenFile( PRESET_FILE_FORMAT, pContextKeyValues );
}


//-----------------------------------------------------------------------------
// Preset remap editor
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnRemoveDefaultControls()
{
	const char *pPresetGroupName = GetSelectedPresetGroupName();
	if ( !pPresetGroupName )
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Remove Default Controls" );

	CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		CDmePresetGroup *pPresetGroup = pAnimSet->FindPresetGroup( pPresetGroupName );
		if ( !pPresetGroup )
			continue;

		CDmrElementArray< CDmePreset > presets = pPresetGroup->GetPresets();
		int nPresetCount = presets.Count();
		for ( int i = 0; i < nPresetCount; ++i )
		{
			CDmePreset *pPreset = presets[i];
			Assert( !pPreset->IsAnimated() ); // deal with this after GDC
			if ( pPreset->IsAnimated() )
				continue;

			CDmrElementArray< CDmElement > controls = pPreset->GetControlValues();	
			int nControlCount = controls.Count();
			for ( int j = nControlCount; --j >= 0; )
			{
				CDmElement *pControlValue = controls[j];
				CDmElement *pControl = pAnimSet->FindControl( pControlValue->GetName() );
				if ( !pControl )
				{
					controls.Remove( j );
					continue;
				}

				bool bIsDefault = true;
				float flDefaultValue = pControl->GetValue< float >( DEFAULT_FLOAT_ATTR );

				if ( IsStereoControl( pControl ) )
				{
					if ( flDefaultValue != pControlValue->GetValue<float>( "leftValue" ) )
					{
						bIsDefault = false;
					}
					if ( flDefaultValue != pControlValue->GetValue<float>( "rightValue" ) )
					{
						bIsDefault = false;
					}
				}
				else
				{
					if ( flDefaultValue != pControlValue->GetValue<float>( "value" ) )
					{
						bIsDefault = false;
					}
				}

				if ( bIsDefault )
				{
					controls.Remove( j );
				}
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular preset
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnOpenPresetContextMenu()
{
	if ( !m_hFilmClip.Get() )
		return;

	const char *pPresetGroupName = GetSelectedPresetGroupName();
	if ( !pPresetGroupName )
		return;

	m_hContextMenu = new vgui::Menu( this, "ActionMenu" );

	// NOTE - we're assuming that presetgroups that are readonly in one animationset are readonly in others
	CDmePresetGroup *pPresetGroup = FindAnyPresetGroup( m_hFilmClip, pPresetGroupName );
	if ( !pPresetGroup )
		return;

	if ( !pPresetGroup->m_bIsReadOnly )
	{
		if ( GetSelectedPresetName() )
		{
			m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_RenamePreset", new KeyValues( "RenamePreset" ), this );
			m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_RemovePreset", new KeyValues( "RemovePreset" ), this );
			m_hContextMenu->AddSeparator();
			m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_MoveUp", new KeyValues( "MovePresetUp" ), this );
			m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_MoveDown", new KeyValues( "MovePresetDown" ), this );
		}

		m_hContextMenu->AddSeparator();
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_ImportPresets", new KeyValues( "ImportPresets" ), this );
	}
	m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_ExportPresets", new KeyValues( "ExportPresets" ), this );

	vgui::Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnOpenContextMenu( KeyValues *kv )
{
	CleanupContextMenu();
	if ( !m_hFilmClip.Get() )
		return;

	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel == m_pPresetList )
	{
		OnOpenPresetContextMenu();
		return;
	}

	if ( pPanel != m_pPresetGroupList )
		return;

	m_hContextMenu = new vgui::Menu( this, "ActionMenu" );
	m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_AddGroup", new KeyValues( "AddGroup" ), this );
	m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_AddPhonemeGroup", new KeyValues( "AddPhonemeGroup" ), this );

	const char *pPresetGroupName = GetSelectedPresetGroupName();
	if ( pPresetGroupName )
	{
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_RenameGroup", new KeyValues( "RenameGroup" ), this );
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_RemoveGroup", new KeyValues( "RemoveGroup" ), this );
		m_hContextMenu->AddSeparator();
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_ToggleVisibility", new KeyValues( "ToggleGroupVisibility" ), this );
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_ToggleSharing", new KeyValues( "ToggleGroupSharing" ), this );
		m_hContextMenu->AddSeparator();
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_MoveUp", new KeyValues( "MoveGroupUp" ), this );
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_MoveDown", new KeyValues( "MoveGroupDown" ), this );

		// NOTE - we're assuming that presetgroups that are readonly in one animationset are readonly in others
		CDmePresetGroup *pPresetGroup = FindAnyPresetGroup( m_hFilmClip, pPresetGroupName );
		if ( !pPresetGroup )
			return;

		if ( !pPresetGroup->m_bIsReadOnly )
		{
			m_hContextMenu->AddSeparator();
			m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_RemoveDefaultControls", new KeyValues( "RemoveDefaultControls" ), this );
		}
	}
	m_hContextMenu->AddSeparator();
	m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_ImportPresets", new KeyValues( "ImportPresetGroups" ), this );
	if ( pPresetGroupName )
	{
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_ExportPresets", new KeyValues( "ExportPresetGroups" ), this );
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_ExportPresetsToFaceposer", new KeyValues( "ExportPresetGroupsToTXT" ), this );
		m_hContextMenu->AddMenuItem( "#DmePresetGroupEditor_ExportPresetsToExpression", new KeyValues( "ExportPresetGroupsToVFE" ), this );
	}

	vgui::Menu::PlaceContextMenu( this, m_hContextMenu.Get() );
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnItemSelected( KeyValues *kv )
{
	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel == m_pPresetGroupList )
	{
		RefreshPresetNames();
		return;
	}
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnItemDeselected( KeyValues *kv )
{
	Panel *pPanel = (Panel *)kv->GetPtr( "panel", NULL );
	if ( pPanel == m_pPresetGroupList )
	{
		RefreshPresetNames();
		return;
	}
}


//-----------------------------------------------------------------------------
// If it finds a duplicate control name, reports an error message and returns it found one
//-----------------------------------------------------------------------------
bool CDmePresetGroupEditorPanel::HasDuplicateGroupName( const char *pGroupName, const char *pIgnorePresetGroupName )
{
	if ( !m_hFilmClip )
		return false;

	if ( FindAnyPresetGroup( m_hFilmClip, pGroupName ) && V_strcmp( pGroupName, pIgnorePresetGroupName ) )
	{
		vgui::MessageBox *pError = new vgui::MessageBox( "#DmePresetGroupEditor_DuplicateNameTitle", "#DmePresetGroupEditor_DuplicateNameText", this );
		pError->DoModal();
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Called by OnInputCompleted after we get a new group name
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::PerformAddGroup( const char *pNewGroupName )
{
	if ( !m_hFilmClip )
		return;

	if ( HasDuplicateGroupName( pNewGroupName ) ) 
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Add Preset Group" );
	CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		pAnimSet->FindOrAddPresetGroup( pNewGroupName );
	}
	sg.Release();

	RefreshAnimationSet();
	SetSelectedPresetGroup( pNewGroupName );
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called by OnInputCompleted after we get a new group name
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::PerformAddPhonemeGroup( const char *pNewGroupName )
{
	if ( !m_hFilmClip )
		return;

	if ( HasDuplicateGroupName( pNewGroupName ) ) 
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Add Phoneme Preset Group" );

	CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		CDmePresetGroup *pPresetGroup = pAnimSet->FindOrAddPresetGroup( pNewGroupName );

		int nPhonemeCount = NumPhonemes();
		for ( int i = 0; i < nPhonemeCount; ++i )
		{
			if ( !IsStandardPhoneme( i ) )
				continue;

			char pTempBuf[256];
			const char *pPhonemeName = NameForPhonemeByIndex( i );
			if ( !Q_stricmp( pPhonemeName, "<sil>" ) )
			{
				pPhonemeName = "silence";
			}
			Q_snprintf( pTempBuf, sizeof(pTempBuf), "p_%s", pPhonemeName );

			pPresetGroup->FindOrAddPreset( pTempBuf );
		}
	}

	sg.Release();

	RefreshAnimationSet();
	SetSelectedPresetGroup( pNewGroupName );
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called by OnInputCompleted after we get a new group name
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::PerformRenameGroup( const char *pNewGroupName )
{
	const char *pPresetGroupName = GetSelectedPresetGroupName();
	if ( !pPresetGroupName )
		return;

	if ( HasDuplicateGroupName( pNewGroupName, pPresetGroupName ) ) 
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Rename Preset Group" );
	CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		CDmePresetGroup *pPresetGroup = pAnimSet->FindPresetGroup( pPresetGroupName );
		if ( !pPresetGroup )
			continue;

		pPresetGroup->SetName( pNewGroupName );
	}
	sg.Release();

	RefreshAnimationSet();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called by OnGroupControls after we get a new group name
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnInputCompleted( KeyValues *pKeyValues )
{
	const char *pName = pKeyValues->GetString( "text", NULL );
	if ( !pName || !pName[0] )						  
		return;

	if ( pKeyValues->FindKey( "OnAddGroup" ) )
	{
		PerformAddGroup( pName );
		return;
	}

	if ( pKeyValues->FindKey( "OnAddPhonemeGroup" ) )
	{
		PerformAddPhonemeGroup( pName );
		return;
	}

	if ( pKeyValues->FindKey( "OnRenameGroup" ) )
	{
		PerformRenameGroup( pName );
		return;
	}

	if ( pKeyValues->FindKey( "OnRenamePreset" ) )
	{
		PerformRenamePreset( pName );
		return;
	}
}


//-----------------------------------------------------------------------------
// Toggle group visibility
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::ToggleGroupVisibility( const char *pPresetGroupName )
{
	if ( !pPresetGroupName )
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Toggle Preset Group Visibility" );
	CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		CDmePresetGroup *pPresetGroup = pAnimSet->FindPresetGroup( pPresetGroupName );
		if ( !pPresetGroup )
			continue;

		pPresetGroup->m_bIsVisible = !pPresetGroup->m_bIsVisible;
	}

	RefreshAnimationSet();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Ungroup controls from each other
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnToggleGroupVisibility()
{
	const char *pPresetGroupName = GetSelectedPresetGroupName();
	if ( !pPresetGroupName )
		return;

	ToggleGroupVisibility( pPresetGroupName );
}


//-----------------------------------------------------------------------------
// Ungroup controls from each other
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnToggleGroupSharing()
{
	const char *pPresetGroupName = GetSelectedPresetGroupName();
	if ( !pPresetGroupName )
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Toggle Preset Group Sharing" );

	CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		CDmePresetGroup *pPresetGroup = pAnimSet->FindPresetGroup( pPresetGroupName );
		if ( !pPresetGroup )
			continue;

		pPresetGroup->SetShared( !pPresetGroup->IsShared() );
	}

	RefreshAnimationSet();
}


//-----------------------------------------------------------------------------
// Add a preset group
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnAddGroup()
{
	vgui::InputDialog *pInput = new vgui::InputDialog( this, "Add Preset Group", "Enter name of new preset group" );
	pInput->SetMultiline( false );
	pInput->DoModal( new KeyValues( "OnAddGroup" ) );
}


//-----------------------------------------------------------------------------
// Add a preset group
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnAddPhonemeGroup()
{
	vgui::InputDialog *pInput = new vgui::InputDialog( this, "Add Phoneme Preset Group", "Enter name of new preset group", "phoneme" );
	pInput->SetMultiline( false );
	pInput->DoModal( new KeyValues( "OnAddPhonemeGroup" ) );
}


//-----------------------------------------------------------------------------
// Rename a preset group
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnRenameGroup()
{
	const char *pPresetGroupName = GetSelectedPresetGroupName();
	if ( !pPresetGroupName )
		return;

	vgui::InputDialog *pInput = new vgui::InputDialog( this, "Rename Preset Group", "Enter new name of preset group" );
	pInput->SetMultiline( false );
	pInput->DoModal( new KeyValues( "OnRenameGroup" ) );
}


//-----------------------------------------------------------------------------
// Remove a preset group
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnRemoveGroup()
{
	if ( !m_hFilmClip.Get() )
		return;

	const char *pPresetGroupName = GetSelectedPresetGroupName();
	if ( !pPresetGroupName )
		return;

	if ( !Q_stricmp( pPresetGroupName, "procedural" ) )
	{
		vgui::MessageBox *pError = new vgui::MessageBox( "#DmePresetGroupEditor_CannotRemovePresetGroupTitle", "#DmePresetGroupEditor_CannotRemovePresetGroupText", this );
		pError->DoModal();
		return;
	}

	int nItemID = m_pPresetGroupList->GetSelectedItem( 0 );
	int nCurrentRow = m_pPresetGroupList->GetItemCurrentRow( nItemID );

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Remove Preset Group" );

	CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		pAnimSet->RemovePresetGroup( pPresetGroupName );
	}

	sg.Release();

	RefreshAnimationSet();
	if ( nCurrentRow >= m_pPresetGroupList->GetItemCount() )
	{
		--nCurrentRow;
	}
	if ( nCurrentRow >= 0 )
	{
		nItemID = m_pPresetGroupList->GetItemIDFromRow( nCurrentRow );
		m_pPresetGroupList->ClearSelectedItems();
		m_pPresetGroupList->AddSelectedItem( nItemID );
	}
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnMoveGroupUp()
{
	if ( m_pPresetGroupList->GetSelectedItemsCount() != 1 )
		return;

	int nItemID = m_pPresetGroupList->GetSelectedItem( 0 );
	int nCurrentRow = m_pPresetGroupList->GetItemCurrentRow( nItemID );
	int nPrevItemID = m_pPresetGroupList->GetItemIDFromRow( nCurrentRow - 1 );
	if ( nPrevItemID < 0 )
		return;

	KeyValues *pKeyValues = m_pPresetGroupList->GetItem( nItemID );
	KeyValues *pPrevKeyValues = m_pPresetGroupList->GetItem( nPrevItemID );

	MovePresetGroupInFrontOf( pKeyValues->GetString( "presetGroupName" ), pPrevKeyValues->GetString( "presetGroupName" ) );
}


//-----------------------------------------------------------------------------
// Called to open a context-sensitive menu for a particular menu item
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::OnMoveGroupDown()
{
	if ( m_pPresetGroupList->GetSelectedItemsCount() != 1 )
		return;

	int nItemID = m_pPresetGroupList->GetSelectedItem( 0 );
	int nCurrentRow = m_pPresetGroupList->GetItemCurrentRow( nItemID );
	int nNextItemID = m_pPresetGroupList->GetItemIDFromRow( nCurrentRow + 1 );
	if ( nNextItemID < 0 )
		return;

	KeyValues *pKeyValues = m_pPresetGroupList->GetItem( nItemID );
	KeyValues *pNextKeyValues = m_pPresetGroupList->GetItem( nNextItemID );

	MovePresetGroupInFrontOf( pNextKeyValues->GetString( "presetGroupName" ), pKeyValues->GetString( "presetGroupName" ) );
}


//-----------------------------------------------------------------------------
// Drag/drop reordering of preset groups
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::MovePresetGroupInFrontOf( const char *pDragGroupName, const char *pDropGroupName )
{
	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Reorder Preset Groups" );
	CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		CDmePresetGroup *pDragPresetGroup = pAnimSet->FindPresetGroup( pDragGroupName );
		if ( !pDragPresetGroup )
			continue;

		CDmePresetGroup *pDropPresetGroup = pAnimSet->FindPresetGroup( pDropGroupName );
		pAnimSet->MovePresetGroupInFrontOf( pDragPresetGroup, pDropPresetGroup );
	}
	sg.Release();

	RefreshAnimationSet();
	SetSelectedPresetGroup( pDragGroupName );
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
// Drag/drop preset moving
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorPanel::MovePresetIntoGroup( const char *pPresetName, const char *pSrcGroupName, const char *pDstGroupName )
{
	if ( !m_hFilmClip.Get() || !pPresetName || !pSrcGroupName || !pDstGroupName )
		return;

	CPresetGroupUndoScopeGuard sg( NOTIFY_SETDIRTYFLAG, "Change Preset Group" );

	CAnimSetGroupAnimSetTraversal traversal( m_hFilmClip );
	while ( CDmeAnimationSet *pAnimSet = traversal.Next() )
	{
		CDmePresetGroup *pPresetGroup = pAnimSet->FindPresetGroup( pSrcGroupName );
		if ( !pPresetGroup )
			continue;
		CDmePreset *pPreset = pPresetGroup->FindPreset( pPresetName );
		if ( !pPreset )
			continue;
		pPresetGroup->RemovePreset( pPresetName );

		CDmePresetGroup *pDstPresetGroup = pAnimSet->FindOrAddPresetGroup( pDstGroupName );
		pDstPresetGroup->FindOrAddPreset( pPresetName );
	}

	sg.Release();

	RefreshPresetNames();
	NotifyDataChanged();
}


//-----------------------------------------------------------------------------
//
//
// CDmePresetGroupListPanel
//
// Declaration above because of scoping issues
//
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CDmePresetGroupListPanel::CDmePresetGroupListPanel( vgui::Panel *pParent, const char *pName, CDmePresetGroupEditorPanel *pComboPanel ) : 
	BaseClass( pParent, pName ), m_pPresetGroupPanel( pComboPanel )
{
}


//-----------------------------------------------------------------------------
// Handle keypresses
//-----------------------------------------------------------------------------
void CDmePresetGroupListPanel::OnMouseDoublePressed( vgui::MouseCode code )
{
	if ( code == MOUSE_LEFT )
	{
		int x, y, row, column;
		vgui::input()->GetCursorPos( x, y );
		GetCellAtPos( x, y, row, column );
		int itemId = GetItemIDFromRow( row );
		KeyValues *pKeyValues = GetItem( itemId );
		m_pPresetGroupPanel->ToggleGroupVisibility( pKeyValues->GetString( "presetGroupName" ) );
		return;
	}

	BaseClass::OnMouseDoublePressed( code );
}


//-----------------------------------------------------------------------------
// Handle keypresses
//-----------------------------------------------------------------------------
void CDmePresetGroupListPanel::OnKeyCodeTyped( vgui::KeyCode code )
{
	if ( code == KEY_DELETE || code == KEY_BACKSPACE )
	{
		m_pPresetGroupPanel->OnRemoveGroup();
		return;
	}

	if ( vgui::input()->IsKeyDown( KEY_LSHIFT ) || vgui::input()->IsKeyDown( KEY_RSHIFT ) )
	{
		if ( code == KEY_UP )
		{
			m_pPresetGroupPanel->OnMoveGroupUp();
			return;
		}
		
		if ( code == KEY_DOWN )
		{
			m_pPresetGroupPanel->OnMoveGroupDown();
			return;
		}
	}

	vgui::ListPanel::OnKeyCodeTyped( code );
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CDmePresetGroupListPanel::OnCreateDragData( KeyValues *msg )
{
	const char *pPresetGroupName = m_pPresetGroupPanel->GetSelectedPresetGroupName();
	if ( !pPresetGroupName )
		return;

	msg->SetString( "presetGroupName", pPresetGroupName );
	msg->SetInt( "selfDroppable", 1 );
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
bool CDmePresetGroupListPanel::IsDroppable( CUtlVector< KeyValues * >& msgList )
{
	if ( msgList.Count() > 0 )
	{
		KeyValues *pData( msgList[ 0 ] );
		if ( m_pPresetGroupPanel )
		{
			const char *pPresetGroupName = pData->GetString( "presetGroupName" );
			if ( pPresetGroupName )
				return true;

			const char *pPresetName = pData->GetString( "presetName" );
			if ( pPresetName )
			{
				// Can't drop presets onto read-only preset groups
				int x, y, row, column;
				vgui::input()->GetCursorPos( x, y );
				GetCellAtPos( x, y, row, column );
				KeyValues *pKeyValues = GetItem( row );
				const char *pDropGroupName = pKeyValues ? pKeyValues->GetString( "presetGroupName" ) : NULL;
				CDmePresetGroup *pDropGroup = FindAnyPresetGroup( m_pPresetGroupPanel->GetAnimationSetClip(), pDropGroupName );
				if ( pDropGroup && !pDropGroup->m_bIsReadOnly )
					return true;
			}
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CDmePresetGroupListPanel::OnPanelDropped( CUtlVector< KeyValues * >& msgList )
{
	if ( msgList.Count() == 0 )
		return;

	KeyValues *pData = msgList[ 0 ];
	if ( !m_pPresetGroupPanel )
		return;

	// Discover the cell the panel is over
	int x, y, row, column;
	vgui::input()->GetCursorPos( x, y );
	GetCellAtPos( x, y, row, column );

	int nItemID = GetItemIDFromRow( row );
	KeyValues *pKeyValues = GetItem( nItemID );
	if ( !pKeyValues )
		return;

	const char *pDropGroupName = pKeyValues->GetString( "presetGroupName" );
	const char *pDragGroupName = pData->GetString( "presetGroupName" );
	if ( pDragGroupName )
	{
		m_pPresetGroupPanel->MovePresetGroupInFrontOf( pDragGroupName, pDropGroupName );
		return;
	}

	const char *pDragPresetName = pData->GetString( "presetName" );
	if ( pDragPresetName )
	{
		m_pPresetGroupPanel->MovePresetIntoGroup( pDragPresetName, pDragGroupName, pDropGroupName );
	}
}


//-----------------------------------------------------------------------------
// Mouse is now over a droppable panel
//-----------------------------------------------------------------------------
void CDmePresetGroupListPanel::OnDroppablePanelPaint( CUtlVector< KeyValues * >& msglist, CUtlVector< Panel * >& dragPanels )
{ 
	// Discover the cell the panel is over
	int x, y, w, h, row, column;
	vgui::input()->GetCursorPos( x, y );
	GetCellAtPos( x, y, row, column );
	GetCellBounds( row, 0, x, y, w, h );

	int x2, y2, w2, h2;
	GetCellBounds( row, 3, x2, y2, w2, h2 );
	w = x2 + w2 - x;

	LocalToScreen( x, y );

	surface()->DrawSetColor( GetDropFrameColor() );

	// Draw insertion point
	surface()->DrawFilledRect( x,			y, x + w, y + 2 );
	surface()->DrawFilledRect( x,	y + h - 2, x + w, y + h );
	surface()->DrawFilledRect( x,			y, x + 2, y + h );
	surface()->DrawFilledRect( x + w - 2,	y, x + w, y + h );
}


//-----------------------------------------------------------------------------
//
//
// CDmePresetListPanel
//
// Declaration above because of scoping issues
//
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CDmePresetListPanel::CDmePresetListPanel( vgui::Panel *pParent, const char *pName, CDmePresetGroupEditorPanel *pComboPanel ) :
	BaseClass( pParent, pName ), m_pPresetGroupPanel( pComboPanel )
{
}

void CDmePresetListPanel::OnKeyCodeTyped( vgui::KeyCode code )
{
	const char *pPresetGroupName = m_pPresetGroupPanel->GetSelectedPresetGroupName();
	if ( !pPresetGroupName )
		return;

	// NOTE - we're assuming that presetgroups that are readonly in one animationset are readonly in others
	CDmePresetGroup *pPresetGroup = FindAnyPresetGroup( m_pPresetGroupPanel->GetAnimationSetClip(), pPresetGroupName );
	if ( pPresetGroup && !pPresetGroup->m_bIsReadOnly )
	{
		if ( code == KEY_DELETE || code == KEY_BACKSPACE )
		{
			m_pPresetGroupPanel->OnRemovePreset();
			return;
		}

		// Not sure how to handle 'edit' mode... the relevant stuff is private
		if ( vgui::input()->IsKeyDown( KEY_LSHIFT ) || vgui::input()->IsKeyDown( KEY_RSHIFT ) )
		{
			if ( code == KEY_UP )
			{
				m_pPresetGroupPanel->OnMovePresetUp();
				return;
			}

			if ( code == KEY_DOWN )
			{
				m_pPresetGroupPanel->OnMovePresetDown();
				return;
			}
		}
	}	

	vgui::ListPanel::OnKeyCodeTyped( code );
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CDmePresetListPanel::OnCreateDragData( KeyValues *msg )
{
	const char *pPresetGroupName = m_pPresetGroupPanel->GetSelectedPresetGroupName();
	if ( !pPresetGroupName )
		return;

	// NOTE - we're assuming that presetgroups that are readonly in one animationset are readonly in others
	CDmePresetGroup *pPresetGroup = FindAnyPresetGroup( m_pPresetGroupPanel->GetAnimationSetClip(), pPresetGroupName );
	if ( pPresetGroup->m_bIsReadOnly )
		return;

	const char *pPresetName = m_pPresetGroupPanel->GetSelectedPresetName();
	if ( !pPresetName )
		return;

	msg->SetString( "presetName", pPresetName );
	msg->SetString( "presetGroupName", pPresetGroupName );
	msg->SetInt( "selfDroppable", 1 );
}


//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
bool CDmePresetListPanel::IsDroppable( CUtlVector< KeyValues * >& msgList )
{
	if ( msgList.Count() > 0 )
	{
		KeyValues *pData( msgList[ 0 ] );
		if ( pData->GetPtr( "panel", NULL ) == this && m_pPresetGroupPanel )
		{
			if ( pData->GetString( "presetName", NULL ) )
				return true;
		}
	}

	return false;
}

//-----------------------------------------------------------------------------
// Called when a list panel's selection changes
//-----------------------------------------------------------------------------
void CDmePresetListPanel::OnPanelDropped( CUtlVector< KeyValues * >& msgList )
{
	if ( msgList.Count() == 0 )
		return;

	KeyValues *pData = msgList[ 0 ];
	if ( pData->GetPtr( "panel", NULL ) != this || !m_pPresetGroupPanel )
		return;

	// Discover the cell the panel is over
	int x, y, row, column;
	vgui::input()->GetCursorPos( x, y );
	GetCellAtPos( x, y, row, column );

	int nItemID = GetItemIDFromRow( row );
	KeyValues *pKeyValues = GetItem( nItemID );

	const char *pDragPresetName = pData     ->GetString( "presetName", NULL );
	const char *pDropPresetName = pKeyValues->GetString( "presetName", NULL );
	if ( pDragPresetName && pDropPresetName )
	{
		m_pPresetGroupPanel->MovePresetInFrontOf( pDragPresetName, pDropPresetName );
	}
}


//-----------------------------------------------------------------------------
// Mouse is now over a droppable panel
//-----------------------------------------------------------------------------
void CDmePresetListPanel::OnDroppablePanelPaint( CUtlVector< KeyValues * >& msglist, CUtlVector< Panel * >& dragPanels )
{
	// Discover the cell the panel is over
	int x, y, w, h, row, column;
	vgui::input()->GetCursorPos( x, y );
	GetCellAtPos( x, y, row, column );
	GetCellBounds( row, column, x, y, w, h );
	LocalToScreen( x, y );

	surface()->DrawSetColor( GetDropFrameColor() );

	// Draw insertion point
	surface()->DrawFilledRect( x,			y, x + w, y + 2 );
	surface()->DrawFilledRect( x,	y + h - 2, x + w, y + h );
	surface()->DrawFilledRect( x,			y, x + 2, y + h );
	surface()->DrawFilledRect( x + w - 2,	y, x + w, y + h );
}



//-----------------------------------------------------------------------------
//
// Purpose: Combination system editor frame
//
//-----------------------------------------------------------------------------
CDmePresetGroupEditorFrame::CDmePresetGroupEditorFrame( vgui::Panel *pParent, const char *pTitle ) : 
	BaseClass( pParent, "DmePresetGroupEditorFrame" )
{
	SetDeleteSelfOnClose( true );
	m_pEditor = new CDmePresetGroupEditorPanel( this, "DmePresetGroupEditorPanel" );
	m_pEditor->AddActionSignalTarget( this );
	m_pOkButton = new vgui::Button( this, "OkButton", "#VGui_OK", this, "Ok" );
	SetBlockDragChaining( true );

	LoadControlSettingsAndUserConfig( "resource/dmepresetgroupeditorframe.res" );

	SetTitle( pTitle, false );
	g_pDataModel->InstallNotificationCallback( this );
}

CDmePresetGroupEditorFrame::~CDmePresetGroupEditorFrame()
{
	g_pDataModel->RemoveNotificationCallback( this );
}


//-----------------------------------------------------------------------------
// On command
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorFrame::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "Ok" ) )
	{
		CloseModal();
		return;
	}

	BaseClass::OnCommand( pCommand );
}

//-----------------------------------------------------------------------------
// Inherited from IDmNotify
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorFrame::NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	if ( !IsVisible() )
		return;

	if ( nNotifySource == NOTIFY_SOURCE_PRESET_GROUP_EDITOR )
		return;

	m_pEditor->RefreshAnimationSet();
}


//-----------------------------------------------------------------------------
// Chains notification messages from the contained panel to external clients
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorFrame::OnPresetsChanged()
{
	PostActionSignal( new KeyValues( "PresetsChanged" ) );
}


//-----------------------------------------------------------------------------
// Various command handlers related to the Edit menu
//-----------------------------------------------------------------------------
void CDmePresetGroupEditorFrame::OnUndo()
{
	if ( g_pDataModel->CanUndo() )
	{
		CDisableUndoScopeGuard guard;
		g_pDataModel->Undo();
	}
}

void CDmePresetGroupEditorFrame::OnRedo()
{
	if ( g_pDataModel->CanRedo() )
	{
		CDisableUndoScopeGuard guard;
		g_pDataModel->Redo();
	}
}
