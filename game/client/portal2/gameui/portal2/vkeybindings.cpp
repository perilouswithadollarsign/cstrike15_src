//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "vkeybindings.h"
#include "VFooterPanel.h"
#include "VHybridButton.h"
#include "EngineInterface.h"
#include "gameui_util.h"
#include "vgui/ISurface.h"
#include "VGenericConfirmation.h"
#include "materialsystem/materialsystem_config.h"
#include "VControlsListPanel.h"
#include "VGenericConfirmation.h"
#include "vcontrolleroptionsbuttons.h"

#include "vgui/Cursor.h"
#include "vgui/IVGui.h"
#include "vgui/ISurface.h"
#include "tier1/KeyValues.h"
#include "tier1/convar.h"
#include "vgui/KeyCode.h"
#include "vgui/MouseCode.h"
#include "vgui/ISystem.h"
#include "vgui/IInput.h"

#include "FileSystem.h"
#include "tier1/UtlBuffer.h"
#include "igameuifuncs.h"
#include "vstdlib/IKeyValuesSystem.h"
#include "tier2/tier2.h"
#include "inputsystem/iinputsystem.h"
#include "gameui_util.h"

#ifdef _X360
#include "xbox/xbox_launch.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace BaseModUI;

static const char *UTIL_Parse( const char *data, char *token, int sizeofToken )
{
	data = engine->ParseFile( data, token, sizeofToken );
	return data;
}

static char *UTIL_CopyString( const char *in )
{
	int len = strlen( in ) + 1;
	char *out = new char[ len ];
	Q_strncpy( out, in, len );
	return out;
}

CKeyBindings::CKeyBindings( Panel *pParent, const char *pPanelName ):
	BaseClass( pParent, pPanelName )
{
	SetDeleteSelfOnClose( true );
	SetProportional( true );

	SetDialogTitle( "L4D360UI_Controller_Edit_Keys_Buttons" );

	m_bDirtyValues = false;
	m_bApplySchemeSettingsFinished = false;

	m_hKeyFont = vgui::INVALID_FONT;
	m_hHeaderFont = vgui::INVALID_FONT;

	m_nActionColumnWidth = 0;
	m_nKeyColumnWidth = 0;

	m_pKeyBindList = new VControlsListPanel( this, "listpanel_keybindlist" );

	Q_memset( m_Bindings, 0, sizeof( m_Bindings ));

	m_nSplitScreenUser = 0;
	if ( !IsGameConsole() )
	{
		static CGameUIConVarRef in_forceuser( "in_forceuser" );
		if ( in_forceuser.IsValid() )
		{
			m_nSplitScreenUser = clamp( in_forceuser.GetInt(), 0, 1 );
		}
		else
		{
			m_nSplitScreenUser = MAX( 0, engine->GetActiveSplitScreenPlayerSlot() );
		}
	}

	// Store all current key bindings
	SaveCurrentBindings();

	SetFooterEnabled( true );
	ShowFooter( true );
}

CKeyBindings::~CKeyBindings()
{
	DeleteSavedBindings();
}

void CKeyBindings::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_nActionColumnWidth = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "KeyBindings.ActionColumnWidth" ) ) );
	m_nKeyColumnWidth = vgui::scheme()->GetProportionalScaledValue( atoi( pScheme->GetResourceString( "KeyBindings.KeyColumnWidth" ) ) );

	const char *pHeaderFontString = pScheme->GetResourceString( "KeyBindings.HeaderFont" );
	if ( pHeaderFontString && pHeaderFontString[0] )
	{
		m_hHeaderFont = pScheme->GetFont( pHeaderFontString, true );
	}

	const char *pKeyFontString = pScheme->GetResourceString( "KeyBindings.KeyFont" );
	if ( pKeyFontString && pKeyFontString[0] )
	{
		m_hKeyFont = pScheme->GetFont( pKeyFontString, true );
	}

	// Parse default descriptions
	ParseActionDescriptions();

	for ( int i = 0; i < m_pKeyBindList->GetItemCount(); i++ )
	{
		int nItemId = m_pKeyBindList->GetItemIDFromRow( i );
		m_pKeyBindList->SetItemFont( nItemId, m_hKeyFont );
	}

	ResetData();

	UpdateFooter();

	m_bApplySchemeSettingsFinished = true;
}

void CKeyBindings::Activate()
{
	BaseClass::Activate();
	UpdateFooter();
}

void CKeyBindings::ShowFooter( bool bShowFooter )
{
	m_bShowFooter = bShowFooter;
	UpdateFooter();
}

void CKeyBindings::UpdateFooter()
{
	CBaseModFooterPanel *pFooter = BaseModUI::CBaseModPanel::GetSingleton().GetFooterPanel();
	if ( pFooter )
	{
		if ( m_bShowFooter )
		{
			int visibleButtons = FB_BBUTTON|FB_XBUTTON|FB_YBUTTON|FB_LSHOULDER;
			if ( m_bDirtyValues )
			{
				 visibleButtons |= FB_ABUTTON;
			}

			pFooter->SetButtons( visibleButtons );
		}
		else
		{
			pFooter->SetButtons( FB_NONE );
		}

		pFooter->SetButtonText( FB_ABUTTON, "#GameUI_Apply" );
		pFooter->SetButtonText( FB_BBUTTON, "#L4D360UI_Back" );
		pFooter->SetButtonText( FB_XBUTTON, "#GameUI_SetNewKey"  );
		pFooter->SetButtonText( FB_YBUTTON, "#GameUI_ClearKey" );
		pFooter->SetButtonText( FB_LSHOULDER, "#GameUI_UseDefaults" );
	}
}

static void AcceptDefaultsOkCallback()
{
	CKeyBindings *pSelf = 
		static_cast< CKeyBindings* >( CBaseModPanel::GetSingleton().GetWindow( WT_KEYBINDINGS ) );
	if ( pSelf )
	{
		pSelf->SetDefaultBindingsAndClose();
	}
}

static void DiscardChangesOkCallback()
{
	CKeyBindings *pSelf = 
		static_cast< CKeyBindings* >( CBaseModPanel::GetSingleton().GetWindow( WT_KEYBINDINGS ) );
	if ( pSelf )
	{
		pSelf->DiscardChangesAndClose();
	}
}

//-----------------------------------------------------------------------------
// Purpose: reloads current keybinding
//-----------------------------------------------------------------------------
void CKeyBindings::ResetData()
{
	// Populate list based on current settings
	FillInCurrentBindings();
	m_pKeyBindList->SetSelectedItem( 0 );
}

//-----------------------------------------------------------------------------
// Purpose: binds double-clicking or hitting enter in the keybind list to changing the key
//-----------------------------------------------------------------------------
void CKeyBindings::OnKeyCodeTyped( vgui::KeyCode code )
{
	if ( code == KEY_ENTER )
	{
		OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_X, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
	}
	else
	{
		BaseClass::OnKeyCodeTyped( code );
	}
}

void CKeyBindings::SetDefaultBindingsAndClose()
{
	// Restore defaults from default keybindings file
	// Destructive action, cannot be undone
	FillInDefaultBindings();
	
	// no choice due to above interior logic, save it
	ApplyAllBindings();

	BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
}

void CKeyBindings::DiscardChangesAndClose()
{
	BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
}

//-----------------------------------------------------------------------------
// Purpose: command handler
//-----------------------------------------------------------------------------
void CKeyBindings::OnCommand( const char *pCommand )
{
	if ( !V_stricmp( "Back", pCommand ) )
	{
		OnKeyCodePressed( KEY_XBUTTON_B );
	}
	else
	{
		BaseClass::OnCommand( pCommand );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CKeyBindings::ParseActionDescriptions( void )
{
	// Load the default keys list
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFullFileSystem->ReadFile( "scripts/kb_act.lst", NULL, buf ) )
		return;

	const char *data = (const char*)buf.Base();

	int sectionIndex = 0;
	char token[512];
	char szBinding[256];
	char szDescription[256];

	m_pKeyBindList->SetHeaderFont( m_hHeaderFont );

	while ( 1 )
	{
		data = UTIL_Parse( data, token, sizeof(token) );
		// Done.
		if ( strlen( token ) <= 0 )  
			break;

		Q_strncpy( szBinding, token, sizeof( szBinding ) );

		data = UTIL_Parse( data, token, sizeof(token) );
		if ( strlen(token) <= 0 )
		{
			break;
		}

		Q_strncpy(szDescription, token, sizeof( szDescription ) );

		// Skip '======' rows
		if ( szDescription[ 0 ] != '=' )
		{
			// Flag as special header row if binding is "blank"
			if ( !V_stricmp(szBinding, "blank") )
			{
				m_pKeyBindList->AddSection(++sectionIndex, szDescription);
				m_pKeyBindList->AddColumnToSection(sectionIndex, "Action", szDescription, SectionedListPanel::COLUMN_BRIGHT, m_nActionColumnWidth );
				m_pKeyBindList->AddColumnToSection(sectionIndex, "Key", "#GameUI_KeyButton", SectionedListPanel::COLUMN_BRIGHT, m_nKeyColumnWidth );
			}
			else
			{
				// Create a new: blank item
				KeyValues *item = new KeyValues( "Item" );

				// fill in data
				item->SetString("Action", szDescription);
				item->SetString("Binding", szBinding);
				item->SetString("Key", "");

				// Add to list
				m_pKeyBindList->AddItem(sectionIndex, item);
				item->deleteThis();
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Search current data set for item which has the specified binding string
// Input  : *binding - string to find
// Output : KeyValues or NULL on failure
//-----------------------------------------------------------------------------
KeyValues *CKeyBindings::GetItemForBinding( const char *binding, int *pnRow )
{
	static int bindingSymbol = KeyValuesSystem()->GetSymbolForString("Binding");

	// Loop through all items
	for (int i = 0; i < m_pKeyBindList->GetItemCount(); i++)
	{
		KeyValues *item = m_pKeyBindList->GetItemData(m_pKeyBindList->GetItemIDFromRow(i));
		if ( !item )
			continue;

		KeyValues *bindingItem = item->FindKey(bindingSymbol);
		const char *bindString = bindingItem->GetString();

		// Check the "Binding" key
		if (!stricmp(bindString, binding))
		{
			if ( pnRow )
				*pnRow = i;
			return item;
		}
	}

	// Didn't find it
	if ( pnRow )
		*pnRow = -1;
	return NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Bind the specified keyname to the specified item
// Input  : *item - Item to which to add the key
//			*keyname - The key to be added
//-----------------------------------------------------------------------------
void CKeyBindings::AddBinding( KeyValues *item, const char *keyname )
{
	// See if it's already there as a binding
	if ( !stricmp( item->GetString( "Key", "" ), keyname ) )
		return;

	// Make sure it doesn't live anywhere
	RemoveKeyFromBindItems( item, keyname );

	const char *binding = item->GetString( "Binding", "" );

	// Uppercase single-letter bindings
	char chCaps[2] = {0};
	if ( keyname && keyname[0] && !keyname[1] )
	{
		chCaps[0] = keyname[0];
		keyname = V_strupr( chCaps );
	}

	// Loop through all the key bindings and set all entries that have
	// the same binding. This allows us to have multiple entries pointing 
	// to the same binding.
	for (int i = 0; i < m_pKeyBindList->GetItemCount(); i++)
	{
		KeyValues *curitem = m_pKeyBindList->GetItemData(m_pKeyBindList->GetItemIDFromRow(i));
		if ( !curitem )
			continue;

		const char *curbinding = curitem->GetString( "Binding", "" );

		if (!stricmp(curbinding, binding))
		{
			curitem->SetString( "Key", keyname );
			m_pKeyBindList->InvalidateItem(i);
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Remove all keys from list
//-----------------------------------------------------------------------------
void CKeyBindings::ClearBindItems( void )
{
	for (int i = 0; i < m_pKeyBindList->GetItemCount(); i++)
	{
		KeyValues *item = m_pKeyBindList->GetItemData(m_pKeyBindList->GetItemIDFromRow(i));
		if ( !item )
			continue;

		// Reset keys
		item->SetString( "Key", "" );

		m_pKeyBindList->InvalidateItem(i);
	}

	m_pKeyBindList->InvalidateLayout();
}

void CKeyBindings::OnEnterEditKeyMode()
{
	if ( !m_pKeyBindList->IsCapturing() )
	{
		int nItemId = m_pKeyBindList->GetItemOfInterest();
		if ( nItemId != -1 )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

			SetOkButtonEnabled( false );
			SetCancelButtonEnabled( false );
			ShowFooter( false );

			m_pKeyBindList->ScrollToItem( nItemId );
			m_pKeyBindList->StartCaptureMode( dc_blank );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Remove all instances of the specified key from bindings
//-----------------------------------------------------------------------------
void CKeyBindings::RemoveKeyFromBindItems( KeyValues *org_item, const char *key )
{
	Assert( key && key[ 0 ] );
	if ( !key || !key[ 0 ] )
		return;

	int len = Q_strlen( key );
	char *pszKey = new char[len + 1];

	if ( !pszKey )
		return;

	Q_memcpy( pszKey, key, len+1 );

	for (int i = 0; i < m_pKeyBindList->GetItemCount(); i++)
	{
		KeyValues *item = m_pKeyBindList->GetItemData(m_pKeyBindList->GetItemIDFromRow(i));
		if ( !item )
			continue;

		// If it's bound to the primary: then remove it
		if ( !stricmp( pszKey, item->GetString( "Key", "" ) ) )
		{
			bool bClearEntry = true;

			if ( org_item )
			{
				// Only clear it out if the actual binding isn't the same. This allows
				// us to have the same key bound to multiple entries in the keybinding list
				// if they point to the same command.
				const char *org_binding = org_item->GetString( "Binding", "" );
				const char *binding = item->GetString( "Binding", "" );
				if ( !stricmp( org_binding, binding ) )
				{
					bClearEntry = false;
				}
			}

			if ( bClearEntry )
			{
				item->SetString( "Key", "" );
				m_pKeyBindList->InvalidateItem(i);
			}
		}
	}

	delete [] pszKey;

	// Make sure the display is up to date
	m_pKeyBindList->InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Ask the engine for all bindings and set up the list
//-----------------------------------------------------------------------------
void CKeyBindings::FillInCurrentBindings( void )
{
	// reset the unbind list
	// we only unbind keys used by the normal config items (not custom binds)
	m_KeysToUnbind.RemoveAll();

	// Clear any current settings
	ClearBindItems();

	for ( int i = 0; i < BUTTON_CODE_LAST; i++ )
	{
		ButtonCode_t bc = ( ButtonCode_t )i;

		bool bIsJoystickCode = IsJoystickCode( bc );
		// Skip Joystick buttons for the "other" user
		if ( bIsJoystickCode && GetJoystickForCode( bc ) != m_nSplitScreenUser )
			continue;

		// Look up binding
		const char *binding = gameuifuncs->GetBindingForButtonCode( bc );
		if ( !binding )
			continue;

		// See if there is an item for this one?
		KeyValues *item = GetItemForBinding( binding );
		if ( item )
		{
			// Bind it by name
			const char *keyName = g_pInputSystem->ButtonCodeToString( bc );

			// Already in list, means user had two keys bound to this item.
			char const *currentKey = item->GetString( "Key", "" );
			if ( currentKey && currentKey[ 0 ] )
			{
				ButtonCode_t currentBC = (ButtonCode_t)gameuifuncs->GetButtonCodeForBind( currentKey );

				// only interested in keyboard bindings
				bool bShouldOverride = !( bIsJoystickCode || IsJoystickCode( currentBC ) );
				if ( !bShouldOverride )
					continue;

				// Remove the key we're about to override from the unbinding list
				m_KeysToUnbind.FindAndRemove( currentBC );
			}

			AddBinding( item, keyName );

			// remember to apply unbinding of this key when we apply
			m_KeysToUnbind.AddToTail( bc );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Clean up memory used by saved bindings
//-----------------------------------------------------------------------------
void CKeyBindings::DeleteSavedBindings( void )
{
	for ( int i = 0; i < BUTTON_CODE_LAST; i++ )
	{
		if ( m_Bindings[ i ].binding )
		{
			delete[] m_Bindings[ i ].binding;
			m_Bindings[ i ].binding = NULL;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Copy all bindings into save array
//-----------------------------------------------------------------------------
void CKeyBindings::SaveCurrentBindings( void )
{
	DeleteSavedBindings();
	for (int i = 0; i < BUTTON_CODE_LAST; i++)
	{
		const char *binding = gameuifuncs->GetBindingForButtonCode( (ButtonCode_t)i );
		if ( !binding || !binding[0])
			continue;

		// Copy the binding string
		m_Bindings[ i ].binding = UTIL_CopyString( binding );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Tells the engine to bind a key
//-----------------------------------------------------------------------------
void CKeyBindings::BindKey( ButtonCode_t bc, const char *binding )
{
	char const *pszKeyName = g_pInputSystem->ButtonCodeToString( bc );
	Assert( pszKeyName );
	if ( !pszKeyName || !*pszKeyName )
		return;

	int nSlot = GetJoystickForCode( bc );
	engine->ClientCmd_Unrestricted( CFmtStr( "cmd%d bind \"%s\" \"%s\"\n", nSlot + 1, pszKeyName, binding ) );
}

//-----------------------------------------------------------------------------
// Purpose: Tells the engine to unbind a key
//-----------------------------------------------------------------------------
void CKeyBindings::UnbindKey( ButtonCode_t bc )
{
	char const *pszKeyName = g_pInputSystem->ButtonCodeToString( bc );
	Assert( pszKeyName );
	if ( !pszKeyName || !*pszKeyName )
		return;

	int nSlot = GetJoystickForCode( bc );
	engine->ClientCmd_Unrestricted( CFmtStr( "cmd%d unbind \"%s\"\n", nSlot + 1, pszKeyName ) );
}

//-----------------------------------------------------------------------------
// Purpose: Go through list and bind specified keys to actions
//-----------------------------------------------------------------------------
void CKeyBindings::ApplyAllBindings( void )
{
	// unbind everything that the user unbound
	for (int i = 0; i < m_KeysToUnbind.Count(); i++)
	{
		ButtonCode_t bc = m_KeysToUnbind[ i ];
		UnbindKey( bc );
	}
	m_KeysToUnbind.RemoveAll();

	// free binding memory
	DeleteSavedBindings();

	for (int i = 0; i < m_pKeyBindList->GetItemCount(); i++)
	{
		KeyValues *item = m_pKeyBindList->GetItemData(m_pKeyBindList->GetItemIDFromRow(i));
		if ( !item )
			continue;

		// See if it has a binding
		const char *binding = item->GetString( "Binding", "" );
		if ( !binding || !binding[ 0 ] )
			continue;

		const char *keyname;

		// Check main binding
		keyname = item->GetString( "Key", "" );
		if ( keyname && keyname[ 0 ] )
		{
			ButtonCode_t code = g_pInputSystem->StringToButtonCode( keyname );
			if ( IsJoystickCode( code ) )
			{
				code = ButtonCodeToJoystickButtonCode( code, m_nSplitScreenUser );
			}

			// Tell the engine
			BindKey( code, binding );
			if ( code != BUTTON_CODE_INVALID )
			{
				m_Bindings[ code ].binding = UTIL_CopyString( binding );
			}
		}
	}

	// Now exec their custom bindings
	engine->ClientCmd_Unrestricted( "exec userconfig.cfg\nhost_writeconfig\n" );
	engine->ExecuteClientCmd( "\n" ); // force the command buffer to get executed
}

//-----------------------------------------------------------------------------
// Purpose: Read in defaults from game's default config file and populate list 
//			using those defaults
//-----------------------------------------------------------------------------
void CKeyBindings::FillInDefaultBindings( void )
{
	FileHandle_t fh = g_pFullFileSystem->Open( "cfg/config_default.cfg", "rb" );
	if ( fh == FILESYSTEM_INVALID_HANDLE )
		return;

	m_bDirtyValues = true;
	UpdateFooter();

	// NOTE: This code was already here, not sure why it was put in,
	// but it makes applying the defaults a destructive action.
	// The outer logic will pop up a confirmation, but a post-cancel is not possible
	engine->ClientCmd_Unrestricted( "unbindall\n" );

	// due to the unbindall - need to reestablish the controller bindings
	ResetControllerConfig();

	int size = g_pFullFileSystem->Size(fh);
	CUtlBuffer buf( 0, size, CUtlBuffer::TEXT_BUFFER );
	g_pFullFileSystem->Read( buf.Base(), size, fh );
	g_pFullFileSystem->Close(fh);

	// Clear out all current bindings
	ClearBindItems();

	const char *data = (const char*)buf.Base();

	// loop through all the binding
	while ( data != NULL )
	{
		char cmd[64];
		data = UTIL_Parse( data, cmd, sizeof(cmd) );
		if ( cmd[ 0 ] == '\0' )
			break;

		if ( !Q_stricmp(cmd, "bind") ||
			!Q_stricmp(cmd, "cmd2 bind") )
		{
			// FIXME:  If we ever support > 2 player splitscreen this will need to be reworked.
			int nJoyStick = 0;
			if ( !stricmp(cmd, "cmd2 bind") )
			{
				nJoyStick = 1;
			}

			// Key name
			char szKeyName[256];
			data = UTIL_Parse( data, szKeyName, sizeof(szKeyName) );
			if ( szKeyName[ 0 ] == '\0' )
				break; // Error

			char szBinding[256];
			data = UTIL_Parse( data, szBinding, sizeof(szBinding) );
			if ( szKeyName[ 0 ] == '\0' )  
				break; // Error

			// Skip it if it's a bind for the other slit
			if ( nJoyStick != m_nSplitScreenUser )
				continue;

			// Find item
			KeyValues *item = GetItemForBinding( szBinding );
			if ( item )
			{
				// Bind it
				AddBinding( item, szKeyName );
			}
		}
		else
		{
			// L4D: Use Defaults also resets cvars listed in config_default.cfg
			CGameUIConVarRef var( cmd );
			if ( var.IsValid() )
			{
				char szValue[256] = "";
				data = UTIL_Parse( data, szValue, sizeof(szValue) );
				var.SetValue( szValue );
			}
		}
	}

	// Make sure console and escape key are always valid
	KeyValues *item = GetItemForBinding( "toggleconsole" );
	if ( item )
	{
		// Bind it
		AddBinding( item, "`" );
	}
	item = GetItemForBinding( "cancelselect" );
	if ( item )
	{
		// Bind it
		AddBinding( item, "ESCAPE" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: User clicked on item: remember where last active row/column was
//-----------------------------------------------------------------------------
void CKeyBindings::ItemSelected( int itemID )
{
	m_pKeyBindList->SetItemOfInterest(itemID);

	CBaseModPanel::GetSingleton().PlayUISound( UISOUND_FOCUS );
}

void CKeyBindings::RequestKeyBindingEdit( char const *binding )
{
	m_sRequestKeyBindingEdit = binding;
}

//-----------------------------------------------------------------------------
// Purpose: called when the capture has finished
//-----------------------------------------------------------------------------
void CKeyBindings::Finish( ButtonCode_t code )
{
	CBaseModPanel::GetSingleton().PlayUISound( UISOUND_CLICK );

	int r = m_pKeyBindList->GetItemOfInterest();

	// Retrieve clicked row and column
	m_pKeyBindList->EndCaptureMode( dc_arrow );

	// Find item for this row
	KeyValues *item = m_pKeyBindList->GetItemData( r );
	if ( item )
	{
		// Handle keys: but never rebind the escape key
		// Esc just exits bind mode silently
		if ( code != BUTTON_CODE_NONE && code != KEY_ESCAPE && code != BUTTON_CODE_INVALID )
		{
			m_bDirtyValues = true;
			UpdateFooter();

			// Bind the named key
			AddBinding( item, g_pInputSystem->ButtonCodeToString( code ) );
		}

		m_pKeyBindList->InvalidateItem(r);
	}

	SetOkButtonEnabled( true );
	SetCancelButtonEnabled( true );
	ShowFooter( true );
}

//-----------------------------------------------------------------------------
// Purpose: Scans for captured key presses
//-----------------------------------------------------------------------------
void CKeyBindings::OnThink()
{
	BaseClass::OnThink();

	if ( m_pKeyBindList->IsCapturing() )
	{
		ButtonCode_t code = BUTTON_CODE_INVALID;
		if ( engine->CheckDoneKeyTrapping( code ) )
		{
			if ( IsJoystickCode( code ) )
			{
				// reject any joystick code, continue capturing
				m_pKeyBindList->StartCaptureMode( dc_blank );
			}
			else
			{
				Finish( code );
			}
		}
	}

	if ( m_bApplySchemeSettingsFinished && !m_pKeyBindList->IsCapturing() )
	{
		if ( !m_sRequestKeyBindingEdit.IsEmpty() )
		{
			int nRow = -1;
			GetItemForBinding( m_sRequestKeyBindingEdit, &nRow );
			m_sRequestKeyBindingEdit.Clear();
			if ( nRow >= 0 )
			{
				m_pKeyBindList->SetSelectedItem( nRow );
				m_pKeyBindList->SetItemOfInterest( nRow );
				((vgui::Panel*)m_pKeyBindList)->PerformLayout();

				m_pKeyBindList->ScrollToItem( nRow + 2 );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check for enter key and go into keybinding mode if it was pressed
//-----------------------------------------------------------------------------
void CKeyBindings::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( m_pKeyBindList->IsCapturing() )
	{
		return;
	}
	
	int joystick = GetJoystickForCode( code );
	int userId = CBaseModPanel::GetSingleton().GetLastActiveUserId();
	if ( joystick != userId || joystick < 0 )
	{	
		return;
	}

	// Grab which item was set as interesting
	int r = m_pKeyBindList->GetItemOfInterest();

	// Check that it's visible
	int x, y, w, h;
	bool visible = m_pKeyBindList->GetCellBounds(r, 1, x, y, w, h);
	if ( visible && r != -1 )
	{
		// visible here doesn't mean wether you can see it
		// force it to be seen
		m_pKeyBindList->ScrollToItem( r );

		if ( KEY_DELETE == code )
		{
			m_bDirtyValues = true;
			UpdateFooter();

			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

			// find the current binding and remove it
			KeyValues *kv = m_pKeyBindList->GetItemData( r );
			const char *key = kv->GetString( "Key", NULL );
			if ( key && *key )
			{
				RemoveKeyFromBindItems( NULL, key );
			}

//			m_pClearBindingButton->SetEnabled( false );
			m_pKeyBindList->InvalidateItem( r );

			// message handled, don't pass on
			return;
		}
	}

	switch ( GetBaseButtonCode( code ) )
	{
	case KEY_XBUTTON_A:
		// accept
		if ( m_bDirtyValues )
		{
			ApplyAllBindings();
		}
		BaseClass::OnKeyCodePressed( ButtonCodeToJoystickButtonCode( KEY_XBUTTON_B, CBaseModPanel::GetSingleton().GetLastActiveUserId() ) );
		break;

	case KEY_XBUTTON_B:
		if ( m_bDirtyValues )
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

			GenericConfirmation *pConfirmation = 
				static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

			GenericConfirmation::Data_t data;
			data.pWindowTitle = "#PORTAL2_KeyboardSettingsConf";
			data.pMessageText = "#PORTAL2_KeyboardSettingsDiscardQ";
			data.bOkButtonEnabled = true;
			data.pfnOkCallback = &DiscardChangesOkCallback;
			data.pOkButtonText = "#PORTAL2_ButtonAction_Discard";
			data.bCancelButtonEnabled = true;
			pConfirmation->SetUsageData( data );	
		}
		else
		{
			// cancel
			BaseClass::OnKeyCodePressed( code );
		}
		break;

	case KEY_XBUTTON_X:
		// edit key
		OnEnterEditKeyMode();
		break;

	case KEY_XBUTTON_Y:
		// clear key
		if ( !m_pKeyBindList->IsCapturing() )
		{
			m_pKeyBindList->RequestFocus();
			OnKeyCodePressed( KEY_DELETE );
		}
		break;

	case KEY_XBUTTON_LEFT_SHOULDER:
		// use defaults
		{
			CBaseModPanel::GetSingleton().PlayUISound( UISOUND_ACCEPT );

			GenericConfirmation *pConfirmation = 
				static_cast< GenericConfirmation* >( CBaseModPanel::GetSingleton().OpenWindow( WT_GENERICCONFIRMATION, this, true ) );

			GenericConfirmation::Data_t data;
			data.pWindowTitle = "#PORTAL2_KeyboardSettingsConf";
			data.pMessageText = "#GameUI_KeyboardSettingsText";
			data.bOkButtonEnabled = true;
			data.pfnOkCallback = &AcceptDefaultsOkCallback;
			data.pOkButtonText = "#PORTAL2_ButtonAction_Reset";
			data.bCancelButtonEnabled = true;
			pConfirmation->SetUsageData( data );
		}
		break;

	default:
		BaseClass::OnKeyCodePressed( code );
		break;
	}
}