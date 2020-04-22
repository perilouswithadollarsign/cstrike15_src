//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef PRESETGROUPEDITORPANEL_H
#define PRESETGROUPEDITORPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"
#include "vgui_controls/Frame.h"
#include "datamodel/dmehandle.h"
#include "vgui_controls/fileopenstatemachine.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeFilmClip;
class CDmePresetListPanel;
class CDmePresetGroupListPanel;
class CDmePresetGroup;
class CDmePreset;
namespace vgui
{
	class PropertySheet;
	class PropertyPage;
	class Button;
}


//-----------------------------------------------------------------------------
// Dag editor panel
//-----------------------------------------------------------------------------
class CDmePresetGroupEditorPanel : public vgui::EditablePanel, public vgui::IFileOpenStateMachineClient
{
	DECLARE_CLASS_SIMPLE( CDmePresetGroupEditorPanel, vgui::EditablePanel );

public:
	// constructor, destructor
	CDmePresetGroupEditorPanel( vgui::Panel *pParent, const char *pName );
	virtual ~CDmePresetGroupEditorPanel();

	void SetAnimationSetClip( CDmeFilmClip *pFilmClip );
	CDmeFilmClip *GetAnimationSetClip();

	void RefreshAnimationSet();
	void NotifyDataChanged();

	// Returns selected presets/groups
	const char* GetSelectedPresetGroupName();
	const char* GetSelectedPresetName();

	// Drag/drop reordering of preset groups
	void MovePresetGroupInFrontOf( const char *pDragGroupName, const char *pDropGroupName );

	// Drag/drop reordering of presets
	void MovePresetInFrontOf( const char *pDragPresetName, const char *pDropPresetName );

	// Drag/drop preset moving
	void MovePresetIntoGroup( const char *pPresetName, const char *pSrcGroupName, const char *pDstGroupName );

	// Toggle group visibility
	void ToggleGroupVisibility( const char *pPresetGroupName );

	MESSAGE_FUNC( OnMovePresetUp, "MovePresetUp" );
	MESSAGE_FUNC( OnMovePresetDown, "MovePresetDown" );
	MESSAGE_FUNC( OnMoveGroupUp, "MoveGroupUp" );
	MESSAGE_FUNC( OnMoveGroupDown, "MoveGroupDown" );
	MESSAGE_FUNC( OnRemoveGroup, "RemoveGroup" );
	MESSAGE_FUNC( OnRemovePreset, "RemovePreset" );

	// Inherited from IFileOpenStateMachineClient
	virtual void SetupFileOpenDialog( vgui::FileOpenDialog *pDialog, bool bOpenFile, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnReadFileFromDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );
	virtual bool OnWriteFileToDisk( const char *pFileName, const char *pFileFormat, KeyValues *pContextKeyValues );

private:
	MESSAGE_FUNC_PARAMS( OnOpenContextMenu, "OpenContextMenu", kv );
	MESSAGE_FUNC_PARAMS( OnInputCompleted, "InputCompleted", kv );
	MESSAGE_FUNC( OnAddGroup, "AddGroup" );
	MESSAGE_FUNC( OnAddPhonemeGroup, "AddPhonemeGroup" );
	MESSAGE_FUNC( OnRenameGroup, "RenameGroup" );
	MESSAGE_FUNC( OnRemoveDefaultControls, "RemoveDefaultControls" );
	MESSAGE_FUNC( OnRenamePreset, "RenamePreset" );
	MESSAGE_FUNC( OnToggleGroupVisibility, "ToggleGroupVisibility" );
	MESSAGE_FUNC( OnToggleGroupSharing, "ToggleGroupSharing" );
	MESSAGE_FUNC_PARAMS( OnItemSelected, "ItemSelected", kv );	
	MESSAGE_FUNC_PARAMS( OnItemDeselected, "ItemDeselected", kv );	
	MESSAGE_FUNC( OnImportPresets, "ImportPresets" );
	MESSAGE_FUNC( OnExportPresets, "ExportPresets" );
	MESSAGE_FUNC( OnImportPresetGroups, "ImportPresetGroups" );
	MESSAGE_FUNC( OnExportPresetGroups, "ExportPresetGroups" );
	MESSAGE_FUNC( OnExportPresetGroupToVFE, "ExportPresetGroupsToVFE" );
	MESSAGE_FUNC( OnExportPresetGroupToTXT, "ExportPresetGroupsToTXT" );
	MESSAGE_FUNC_PARAMS( OnPresetPicked, "PresetPicked", params );
	MESSAGE_FUNC_PARAMS( OnPresetPickCancelled, "PresetPickCancelled", params );
	MESSAGE_FUNC_PARAMS( OnFileStateMachineFinished, "FileStateMachineFinished", params );

	// Cleans up the context menu
	void CleanupContextMenu();

	// If it finds a duplicate group/preset name, reports an error message and returns it found one
	bool HasDuplicatePresetName( const char *pPresetName, const char *pIgnorePresetName = NULL );
	bool HasDuplicateGroupName ( const char *pGroupName,  const char *pIgnorePresetGroupName = NULL );

	// Refreshes the list of presets
	void RefreshPresetNames( );

	// Called by OnInputCompleted after we get a new group or preset name
	void PerformAddGroup( const char *pNewGroupName );
	void PerformAddPhonemeGroup( const char *pNewGroupName );
	void PerformRenameGroup( const char *pNewGroupName );
	void PerformRenamePreset( const char *pNewPresetName );

	// Called to open a context-sensitive menu for a particular preset
	void OnOpenPresetContextMenu( );

	// Gets/sets a selected preset
	void SetSelectedPreset( const char* pPresetName );

	// Selects a particular preset group
	void SetSelectedPresetGroup( const char* pPresetGroupName );

	// Imports presets
	void ImportPresets( CUtlVector< const char * >& presetNames, CDmElement *pRoot );

	CDmeHandle< CDmeFilmClip > m_hFilmClip;
	vgui::Splitter *m_pSplitter;
	CDmePresetGroupListPanel *m_pPresetGroupList;
	CDmePresetListPanel *m_pPresetList;
	vgui::DHANDLE< vgui::Menu > m_hContextMenu;
	vgui::DHANDLE< vgui::FileOpenStateMachine > m_hFileOpenStateMachine;
};


//-----------------------------------------------------------------------------
// Frame for combination system
//-----------------------------------------------------------------------------
class CDmePresetGroupEditorFrame : public vgui::Frame, public IDmNotify
{
	DECLARE_CLASS_SIMPLE( CDmePresetGroupEditorFrame, vgui::Frame );

public:
	CDmePresetGroupEditorFrame( vgui::Panel *pParent, const char *pTitle );
	~CDmePresetGroupEditorFrame();

	// Inherited from IDmNotify
	virtual void NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags );

	void SetAnimationSetClip( CDmeFilmClip *pClip ) { m_pEditor->SetAnimationSetClip( pClip ); }
	void RefreshAnimationSet() { m_pEditor->RefreshAnimationSet(); }

private:
	MESSAGE_FUNC( OnPresetsChanged, "PresetsChanged" );
	KEYBINDING_FUNC( undo, KEY_Z, vgui::MODIFIER_CONTROL, OnUndo, "#undo_help", 0 );
	KEYBINDING_FUNC( redo, KEY_Z, vgui::MODIFIER_CONTROL | vgui::MODIFIER_SHIFT, OnRedo, "#redo_help", 0 );

	// Inherited from Frame
	virtual void OnCommand( const char *pCommand );

	CDmePresetGroupEditorPanel *m_pEditor;
	vgui::Button *m_pOkButton;
};


#endif // PRESETGROUPEDITORPANEL_H