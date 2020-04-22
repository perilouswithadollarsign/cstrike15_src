//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VKEYBINDINGS_H__
#define __VKEYBINDINGS_H__

#include "basemodui.h"
#include "tier1/UtlVector.h"
#include "tier1/UtlSymbol.h"

class VControlsListPanel;

namespace BaseModUI {

class BaseModHybridButton;

class CKeyBindings : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( CKeyBindings, CBaseModFrame );

public:
	CKeyBindings( vgui::Panel *pParent, const char *pPanelName );
	~CKeyBindings();

	// Trap row selection message
	MESSAGE_FUNC_INT( ItemSelected, "ItemSelected", itemID );
	MESSAGE_FUNC_CHARPTR( RequestKeyBindingEdit, "RequestKeyBindingEdit", binding );

	void SetDefaultBindingsAndClose();
	void DiscardChangesAndClose();

protected:
	virtual void	Activate();
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	OnKeyCodePressed( vgui::KeyCode code );
	virtual void	OnKeyCodeTyped( vgui::KeyCode code );
	virtual void	OnCommand( const char *pCommand );
	virtual void	OnThink();

private:
	struct KeyBinding
	{
		char *binding;
	};

	void			ResetData();
	void			Finish( ButtonCode_t code );

	void			UpdateFooter();
	void			ShowFooter( bool bShowFooter );

	// Create the key binding list control
	void			CreateKeyBindingList( void );

	// Tell engine to bind/unbind a key
	void			BindKey( ButtonCode_t bc, const char *binding );
	void			UnbindKey( ButtonCode_t bc );

	// Save/restore/cleanup engine's current bindings ( for handling cancel button )
	void			SaveCurrentBindings( void );
	void			DeleteSavedBindings( void );

	// Get column 0 action descriptions for all keys
	void			ParseActionDescriptions( void );

	// Populate list of actions with current engine keybindings
	void			FillInCurrentBindings( void );
	// Remove all current bindings from list of bindings
	void			ClearBindItems( void );
	// Enter edit key mode for the current item of interest
	void			OnEnterEditKeyMode( void );
	// Fill in bindings with mod-specified defaults
	void			FillInDefaultBindings( void );
	// Copy bindings out of list and set them in the engine
	void			ApplyAllBindings( void );

	// Bind a key to the item
	void			AddBinding( KeyValues *item, const char *keyname );

	// Remove all instances of a key from all bindings
	void			RemoveKeyFromBindItems( KeyValues *org_item, const char *key );

	// Find item by binding name
	KeyValues		*GetItemForBinding( const char *binding, int *pnRow = NULL );

	VControlsListPanel	*m_pKeyBindList;

	// List of saved bindings for the keys
	KeyBinding m_Bindings[ BUTTON_CODE_LAST ];

	// List of all the keys that need to have their binding removed
	CUtlVector< ButtonCode_t > m_KeysToUnbind;

	int	m_nSplitScreenUser;
	
	vgui::HFont	m_hKeyFont;
	vgui::HFont	m_hHeaderFont;

	int			m_nActionColumnWidth;
	int			m_nKeyColumnWidth;

	bool		m_bDirtyValues;
	bool		m_bShowFooter;
	bool		m_bApplySchemeSettingsFinished;

	CUtlString  m_sRequestKeyBindingEdit;
};

};

#endif // __VKEYBINDINGS_H__