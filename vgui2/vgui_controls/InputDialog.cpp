//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include <vgui_controls/InputDialog.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/TextEntry.h>
#include "tier1/keyvalues.h"
#include "tier1/fmtstr.h"
#include "vgui/IInput.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
BaseInputDialog::BaseInputDialog( Panel *parent, const char *title, bool bShowCancelButton /*= true*/ ) :
	BaseClass( parent, NULL )
{
	m_pContextKeyValues = NULL;

	SetDeleteSelfOnClose( true );
	SetTitle(title, true);
	SetSize(320, 180);
	SetSizeable( false );

	m_pOKButton = new Button( this, "OKButton", "#VGui_OK" );
	m_pOKButton->SetCommand( "OK" );
	m_pOKButton->SetAsDefaultButton( true );

	if ( bShowCancelButton )
	{
		m_pCancelButton = new Button( this, "CancelButton", "#VGui_Cancel" );
		m_pCancelButton->SetCommand( "Cancel" );
	}
	else
	{
		m_pCancelButton = NULL;
	}

	if ( parent )
	{
		AddActionSignalTarget( parent );
	}
}

BaseInputDialog::~BaseInputDialog()
{
	CleanUpContextKeyValues();
}

//-----------------------------------------------------------------------------
// Purpose: Cleans up the keyvalues
//-----------------------------------------------------------------------------
void BaseInputDialog::CleanUpContextKeyValues()
{
	if ( m_pContextKeyValues )
	{
		m_pContextKeyValues->deleteThis();
		m_pContextKeyValues = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void BaseInputDialog::DoModal( KeyValues *pContextKeyValues )
{
	CleanUpContextKeyValues();
	m_pContextKeyValues = pContextKeyValues;
	BaseClass::DoModal();
}

//-----------------------------------------------------------------------------
// Purpose: lays out controls
//-----------------------------------------------------------------------------
void BaseInputDialog::PerformLayout()
{
	BaseClass::PerformLayout();

	int w, h;
	GetSize( w, h );

	// lay out all the controls
	int topy = IsSmallCaption() ? 15 : 30;
	int halfw = w / 2;

	PerformLayout( 12, topy, w - 24, h - 100 );

	if ( m_pCancelButton )
	{
		m_pOKButton->SetBounds( halfw - 84, h - 30, 72, 24 );
		m_pCancelButton->SetBounds( halfw + 12, h - 30, 72, 24 );
	}
	else
	{
		m_pOKButton->SetBounds( halfw - 36, h - 30, 72, 24 );
	}
}


//-----------------------------------------------------------------------------
// Purpose: handles button commands
//-----------------------------------------------------------------------------
void BaseInputDialog::OnCommand(const char *command)
{
	KeyValues *kv = NULL;
	if ( !stricmp( command, "OK" ) )
	{
		kv = new KeyValues( "InputCompleted" );
		WriteDataToKeyValues( kv, true );
	}
	else if ( !stricmp( command, "Cancel" ) )
	{
		kv = new KeyValues( "InputCanceled" );
		WriteDataToKeyValues( kv, false );
	}
	else
	{
		BaseClass::OnCommand( command );
		return;
	}

	if ( m_pContextKeyValues )
	{
		kv->AddSubKey( m_pContextKeyValues );
		m_pContextKeyValues = NULL;
	}
	PostActionSignal( kv );
	CloseModal();
}


//-----------------------------------------------------------------------------
// Purpose: Utility dialog, used to ask yes/no questions of the user
//-----------------------------------------------------------------------------
InputMessageBox::InputMessageBox( Panel *parent, const char *title, char const *prompt )
: BaseClass( parent, title )
{
	SetSize( 320, 120 );

	m_pPrompt = new Label( this, "Prompt", prompt );
}

InputMessageBox::~InputMessageBox()
{
}

void InputMessageBox::PerformLayout( int x, int y, int w, int h )
{
	m_pPrompt->SetBounds( x, y, w, 24 );
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
InputDialog::InputDialog( Panel *parent, const char *title, char const *prompt, char const *defaultValue /*=""*/ ) : 
	BaseClass(parent, title)
{
	SetSize( 320, 120 );

	m_pPrompt = new Label( this, "Prompt", prompt );
	
	m_pInput = new TextEntry( this, "Text" );
	m_pInput->SetText( defaultValue );
	m_pInput->SelectAllText( true );
	m_pInput->RequestFocus();
}


InputDialog::~InputDialog()
{
}


//-----------------------------------------------------------------------------
// Sets the dialog to be multiline
//-----------------------------------------------------------------------------
void InputDialog::SetMultiline( bool state )
{
	m_pInput->SetMultiline( state );
	m_pInput->SetCatchEnterKey( state );
}

	
//-----------------------------------------------------------------------------
// Allow numeric input only
//-----------------------------------------------------------------------------
void InputDialog::AllowNumericInputOnly( bool bOnlyNumeric )
{
	if ( m_pInput )
	{
		m_pInput->SetAllowNumericInputOnly( bOnlyNumeric );
	}
}


//-----------------------------------------------------------------------------
// Purpose: lays out controls
//-----------------------------------------------------------------------------
void InputDialog::PerformLayout( int x, int y, int w, int h )
{
	m_pPrompt->SetBounds( x, y, w, 24 );
	m_pInput ->SetBounds( x, y + 30, w, m_pInput->IsMultiline() ? h - 30 : 24 );
}


//-----------------------------------------------------------------------------
// Purpose: handles button commands
//-----------------------------------------------------------------------------
void InputDialog::WriteDataToKeyValues( KeyValues *pKV, bool bOk )
{
	if ( !bOk )
		return; // don't write any data on cancel

	int nTextLength = m_pInput->GetTextLength() + 1;
	char* txt = (char*)stackalloc( nTextLength * sizeof(char) );
	m_pInput->GetText( txt, nTextLength );
	pKV->SetString( "text", txt );
}


//-----------------------------------------------------------------------------
// Purpose: Utility dialog, used to let user specify multiple bool/float/string values
//-----------------------------------------------------------------------------
MultiInputDialog::MultiInputDialog( Panel *pParent, const char *pTitle, const char *pOKText /*= "#VGui_OK"*/, const char *pCancelText /*= "#VGui_Cancel"*/ )
: BaseClass( pParent, NULL ), m_pOKCommand( NULL ), m_pCancelCommand( NULL ), m_nCurrentTabPosition( 0 )
{
	SetDeleteSelfOnClose( true );
	SetTitle( pTitle, true );

	m_pOKButton = new Button( this, "OKButton", pOKText );
	m_pOKButton->SetCommand( "OK" );
	m_pOKButton->SetAsDefaultButton( true );

	if ( pCancelText && *pCancelText )
	{
		m_pCancelButton = new Button( this, "CancelButton", pCancelText );
		m_pCancelButton->SetCommand( "Cancel" );
	}
	else
	{
		m_pCancelButton = NULL;
	}

	if ( pParent )
	{
		AddActionSignalTarget( pParent );
	}
}

MultiInputDialog::~MultiInputDialog()
{
	SetOKCommand( NULL );
	SetCancelCommand( NULL );
}

void MultiInputDialog::SetOKCommand( KeyValues *pOKCommand )
{
	if ( m_pOKCommand )
	{
		m_pOKCommand->deleteThis();
	}
	m_pOKCommand = pOKCommand;
}

void MultiInputDialog::SetCancelCommand( KeyValues *pCancelCommand )
{
	if ( m_pCancelCommand )
	{
		m_pCancelCommand->deleteThis();
	}
	m_pCancelCommand = pCancelCommand;
}

void MultiInputDialog::AddText( const char *pText )
{
	AddLabel( pText );
	m_inputs.AddToTail( NULL );
	m_entryTypes.AddToTail( T_NONE );
}

void MultiInputDialog::AddEntry( const char *pName, const char *pPrompt, bool bDefaultValue )
{
	m_prompts.AddToTail( NULL );

	CheckButton *pCheckButton = new CheckButton( this, pName, pPrompt );
	pCheckButton->SetSelected( bDefaultValue );
	pCheckButton->SetTabPosition( m_nCurrentTabPosition++ );
	if ( m_nCurrentTabPosition == 1 ) // first entry
	{
		pCheckButton->RequestFocus();
	}

	m_inputs.AddToTail( pCheckButton );
	m_entryTypes.AddToTail( T_BOOL );
}

void MultiInputDialog::AddEntry( const char *pName, const char *pPrompt, float flDefaultValue )
{
	AddLabel( pPrompt );
	TextEntry *pInput = AddTextEntry( pName, CFmtStr( "%f", flDefaultValue ) );
	pInput->SetAllowNumericInputOnly( true );
	m_entryTypes.AddToTail( T_FLOAT );
}

void MultiInputDialog::AddEntry( const char *pName, const char *pPrompt, const char *pDefaultValue )
{
	AddLabel( pPrompt );
	AddTextEntry( pName, pDefaultValue );
	m_entryTypes.AddToTail( T_STRING );
}

Label *MultiInputDialog::AddLabel( const char *pText )
{
	int index = m_prompts.Count();
	Label *pLabel = new Label( this, CFmtStr( "label%d", index ), pText );
	m_prompts.AddToTail( pLabel );
	return pLabel;
}

TextEntry *MultiInputDialog::AddTextEntry( const char *pName, const char *pDefaultValue )
{
	TextEntry *pInput = new TextEntry( this, pName );
	pInput->SetText( pDefaultValue );
	pInput->SetTabPosition( m_nCurrentTabPosition++ );
	if ( m_nCurrentTabPosition == 1 ) // first entry
	{
		pInput->RequestFocus();
	}

	m_inputs.AddToTail( pInput );
	return pInput;
}

void MultiInputDialog::DoModal()
{
	int nCount = m_prompts.Count();
	int nEntryHeight = 24 * nCount;
	int nDesiredHeight = nEntryHeight + 100;

	int nDesiredWidth = GetContentWidth();
	nDesiredWidth = MAX( nDesiredWidth, 320 );

	SetSize( nDesiredWidth, nDesiredHeight );

	BaseClass::DoModal();
}

void MultiInputDialog::PerformLayout()
{
	BaseClass::PerformLayout();

	int w, h;
	GetSize( w, h );

	// lay out all the controls
	int topy = IsSmallCaption() ? 15 : 30;
	int halfw = w / 2;

	PerformLayout( 12, topy, w - 24, h - 100 );

	int nOkayWidth, nOkayHeight;
	m_pOKButton->GetContentSize( nOkayWidth, nOkayHeight );
	nOkayWidth += 10;

	if ( m_pCancelButton )
	{
		int nCancelWidth, nCancelHeight;
		m_pCancelButton->GetContentSize( nCancelWidth, nCancelHeight );
		nCancelWidth += 10;

		int nButtonWidths = nOkayWidth + 24 + nCancelWidth;

		m_pOKButton->SetBounds( halfw - nButtonWidths/2, h - 30, nOkayWidth, 24 );
		m_pCancelButton->SetBounds( halfw + nButtonWidths/2 - nCancelWidth, h - 30, nCancelWidth, 24 );
	}
	else
	{
		m_pOKButton->SetBounds( halfw - nOkayWidth/2, h - 30, nOkayWidth, 24 );
	}
}

void MultiInputDialog::PerformLayout( int x, int y, int w, int h )
{
	y += 10;
	int nLabelWidth = GetLabelWidth() + 10;
	int nCount = m_prompts.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_prompts[ i ] )
		{
			if ( m_inputs[ i ] )
			{
				m_prompts[ i ]->SetBounds( x, y, nLabelWidth, 24 );
				m_inputs [ i ]->SetBounds( x + nLabelWidth, y, w - nLabelWidth, 24 );
			}
			else
			{
				m_prompts[ i ]->SetBounds( x, y, w, 24 );
			}
		}
		else
		{
			m_inputs[ i ]->SetBounds( x, y, w, 24 );
		}
		y += 24;
	}
}

int MultiInputDialog::GetLabelWidth()
{
	int nLabelWidth = 50;
	int nCount = m_prompts.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !m_inputs[ i ] || !m_prompts[ i ] )
			continue; // skip text and bools

		int w, h;
		m_prompts[ i ]->GetContentSize( w, h );
		nLabelWidth = MAX( w, nLabelWidth );
	}
	return nLabelWidth;
}

int MultiInputDialog::GetContentWidth()
{
	int nContentWidth = 100;
	int nCount = m_prompts.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		int h, nEntryWidth = 0;
		Panel *pInput = m_inputs[ i ];
		if ( !pInput )
			continue;

		if ( Label *pPrompt = m_prompts[ i ] )
		{
			pPrompt->GetContentSize( nEntryWidth, h );
			nEntryWidth += 100; // allow room for input
		}
		else if ( CheckButton *pCheckButton = dynamic_cast< CheckButton* >( pInput ) )
		{
			pCheckButton->GetContentSize( nEntryWidth, h );
			nEntryWidth += 20; // allow room for box
		}

		nContentWidth = MAX( nContentWidth, nEntryWidth );
	}
	return nContentWidth;
}

void MultiInputDialog::WriteDataToKeyValues( KeyValues *pKV )
{
	int nCount = m_prompts.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		Panel *pInput = m_inputs[ i ];
		if ( !pInput )
			continue; // T_NONE

		if ( m_entryTypes[ i ] == T_BOOL )
		{
			CheckButton *pCheckButton = dynamic_cast< CheckButton* >( pInput );
			pKV->SetBool( pCheckButton->GetName(), pCheckButton->IsSelected() );
		}
		else
		{
			TextEntry *pTextEntry = dynamic_cast< TextEntry* >( pInput );
			if ( m_entryTypes[ i ] == T_FLOAT )
			{
				pKV->SetFloat( pTextEntry->GetName(), pTextEntry->GetValueAsFloat() );
			}
			else
			{
				char text[ 256 ];
				pTextEntry->GetText( text, sizeof( text ) );
				pKV->SetString( pTextEntry->GetName(), text );
			}
		}
	}
}

void MultiInputDialog::OnCommand( const char *pCommand )
{
	if ( !V_stricmp( pCommand, "OK" ) )
	{
		KeyValues *pOKCommand = m_pOKCommand ? m_pOKCommand->MakeCopy() : new KeyValues( "InputCompleted" );
		WriteDataToKeyValues( pOKCommand );
		PostActionSignal( pOKCommand );
	}
	else if ( !V_stricmp( pCommand, "Cancel" ) )
	{
		KeyValues *pCancelCommand = m_pCancelCommand ? m_pCancelCommand->MakeCopy() : new KeyValues( "InputCanceled" );
		PostActionSignal( pCancelCommand );
	}
	else
	{
		BaseClass::OnCommand( pCommand );
		return;
	}

	CloseModal();
}
