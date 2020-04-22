//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#ifndef PARTICLESYSTEMDEFINITIONBROWSER_H
#define PARTICLESYSTEMDEFINITIONBROWSER_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/editablepanel.h"
#include "tier1/utlstring.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CPetDoc;
class CPetTool;
class CDmeParticleSystemDefinition;
class CUndoScopeGuard;
class CParticleSnapshotGrid;

namespace vgui
{
	class ComboBox;
	class Button;
	class TextEntry;
	class ListPanel;
	class CheckButton;
	class RadioButton;
}


//-----------------------------------------------------------------------------
// Panel that shows all entities in the level
//-----------------------------------------------------------------------------
class CParticleSystemDefinitionBrowser : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CParticleSystemDefinitionBrowser, vgui::EditablePanel );

public:
	CParticleSystemDefinitionBrowser( CPetDoc *pDoc, vgui::Panel* pParent, const char *pName );   // standard constructor
	virtual ~CParticleSystemDefinitionBrowser();

	// Inherited from Panel
	virtual void OnCommand( const char *pCommand );
	virtual void OnKeyCodeTyped( vgui::KeyCode code );

	// Methods related to updating the listpanel
	void UpdateParticleSystemList( bool bRetainSelection = true );

	// Select a particular node
	void SelectParticleSystem( CDmeParticleSystemDefinition *pParticleSystem );

	// paste.
	void PasteFromClipboard();

private:
	MESSAGE_FUNC( OnCopy, "OnCopy" );
	KEYBINDING_FUNC_NODECLARE( edit_copy, KEY_C, vgui::MODIFIER_CONTROL, OnCopy, "#edit_copy_help", 0 );

	MESSAGE_FUNC_PARAMS( OnInputCompleted, "InputCompleted", kv );

	MESSAGE_FUNC( OnParticleSystemSelectionChanged, "ParticleSystemSelectionChanged" );

	void ReplaceDef_r( CUndoScopeGuard& guard, CDmeParticleSystemDefinition *pDef );
	void PasteOperator( CUndoScopeGuard& guard, class CDmeParticleFunction *pDef );
	void PasteDefinitionBody( CUndoScopeGuard& guard, CDmeParticleSystemDefinition *pDef );

	// Gets the selected particle system
	CDmeParticleSystemDefinition* GetSelectedParticleSystem( int nIdx );

	// Called when the selection changes
	void UpdateParticleSystemSelection();

	// Deletes selected particle systems
	void DeleteParticleSystems();

	// Shows the most recent selected object in properties window
	void OnProperties();

	CPetDoc *m_pDoc;
	CParticleSnapshotGrid *m_pSystemGrid;
	vgui::Button *m_pCreateButton;
	vgui::Button *m_pDeleteButton;
	vgui::Button *m_pCopyButton;
};


#endif // PARTICLESYSTEMDEFINITIONBROWSER_H
