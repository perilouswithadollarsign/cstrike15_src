//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ========
//
// Purpose: Implementation of the CScriptEditorPanel class and associated helper
// classes. The ScriptEditorPanel class represents a vgui panel which contains
// a text editing panel which may be used to edit a script and text panel which
// displays output from the script.
//
//=============================================================================

#include "toolutils/scripteditorpanel.h"
#include "vgui/iinput.h"
#include "vgui/ischeme.h"
#include "vgui/ivgui.h"
#include "vgui/isurface.h"
#include "vgui_controls/button.h"
#include "vgui_controls/textentry.h"
#include "vgui_controls/richtext.h"
#include "tier1/utlbuffer.h"
#include "vgui/ilocalize.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CLineNumberPanel::CLineNumberPanel( vgui::Panel *pParent, vgui::TextEntry *pTextEntry, const char *pchName )
: BaseClass( pParent, pchName )
, m_pTextEntry( pTextEntry )
, m_hFont( NULL )
{

}


//-----------------------------------------------------------------------------
// Purpose: Paint the background of the panel, including the line numbers
//-----------------------------------------------------------------------------
void CLineNumberPanel::PaintBackground()
{
	BaseClass::PaintBackground();

	Panel *pParent = GetParent();
	if ( pParent )
	{
		if ( pParent->IsVisible() == false )
			return;
	}

	int width, height;
	GetSize( width, height );

	int fontHeight = surface()->GetFontTall( m_hFont );
	int yPos = 1;
	int line = m_pTextEntry->GetCurrentStartLine() + 1;

	surface()->DrawSetTextFont( m_hFont );
	surface()->DrawSetTextColor( m_Color );

	while ( (yPos + fontHeight) < height )
	{
		wchar_t sz[ 32 ];
		_snwprintf( sz, sizeof( sz ), L"%4i", line );

		surface()->DrawSetTextPos( 0, yPos );
		surface()->DrawPrintText( sz, wcslen( sz ) );

		yPos += fontHeight + 1;
		++line;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Apply the settings from the provided scheme, and save the font to 
// display line numbers.
//-----------------------------------------------------------------------------
void CLineNumberPanel::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	m_hFont = pScheme->GetFont( "ConsoleText", IsProportional() );
	m_Color = GetSchemeColor( "Console.TextColor", pScheme );
}




//-----------------------------------------------------------------------------
// Purpose: Constructor, creates all of the controls owned by the panel.
//-----------------------------------------------------------------------------
CScriptEditorPanel::CScriptEditorPanel( vgui::Panel *pParent, const char *pName ) 
: BaseClass( pParent, pName )
{
	SetKeyBoardInputEnabled( true );

	SetMinimumSize(300,200);

	// Create the output panel
	m_pOutput = new RichText(this, "ConsoleHistory");
	m_pOutput->SetAllowKeyBindingChainToParent( false );
	SETUP_PANEL( m_pOutput );
	m_pOutput->SetVerticalScrollbar( true );
	m_pOutput->GotoTextEnd();

	// Create the submit button
	m_pSubmit = new Button(this, "ConsoleSubmit", "#Console_Submit");
	m_pSubmit->SetCommand("submit");
	m_pSubmit->SetVisible( true );

	// Create the script entry panel
	m_pScriptEntry = new TextEntry(this, "ScriptEntry" );
	m_pScriptEntry->AddActionSignalTarget(this);
	m_pScriptEntry->SetMultiline( true );
	m_pScriptEntry->SetVerticalScrollbar( true );
	m_pScriptEntry->SetCatchEnterKey( true );
	m_pScriptEntry->SetCatchTabKey( true );
	m_pScriptEntry->SendNewLine( true );
	m_pScriptEntry->SetTabPosition(1);

	// Create the line number display panel
	m_pLineNumberPanel = new CLineNumberPanel( this, m_pScriptEntry, "ScriptLineNumbers" );
	
	// Set the default text colors, these will be overridden by ApplySchemeSettings
	m_PrintColor = Color(216, 222, 211, 255);
	m_DPrintColor = Color(196, 181, 80, 255);

	// Add to global console list so the the console 
	// output will be displayed in the output panel.
	g_pCVar->InstallConsoleDisplayFunc( this );
}


//-----------------------------------------------------------------------------
// Purpose: Destructor, removes the output from the global console list and
// destroys the controls.
//-----------------------------------------------------------------------------
CScriptEditorPanel::~CScriptEditorPanel()
{
	// Remove from the global console list
	g_pCVar->RemoveConsoleDisplayFunc( this );

	// Destroy the controls
	delete m_pOutput;
	m_pOutput = NULL;

	delete m_pSubmit;
	m_pSubmit = NULL;

	delete m_pScriptEntry;
	m_pScriptEntry = NULL;

	delete m_pLineNumberPanel;
	m_pLineNumberPanel = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: Clears the output console
//-----------------------------------------------------------------------------
void CScriptEditorPanel::Clear()
{
	m_pOutput->SetText("");
	m_pOutput->GotoTextEnd();
}


//-----------------------------------------------------------------------------
// Purpose: color text print
//-----------------------------------------------------------------------------
void CScriptEditorPanel::ColorPrint( const Color& clr, const char *msg )
{
	m_pOutput->InsertColorChange( clr );
	m_pOutput->InsertString( msg );
}


//-----------------------------------------------------------------------------
// Purpose: normal text print
//-----------------------------------------------------------------------------
void CScriptEditorPanel::Print(const char *msg)
{
	ColorPrint( m_PrintColor, msg );
}


//-----------------------------------------------------------------------------
// Purpose: debug text print
//-----------------------------------------------------------------------------
void CScriptEditorPanel::DPrint( const char *msg )
{
	ColorPrint( m_DPrintColor, msg );
}


//-----------------------------------------------------------------------------
// Returns the console text
//-----------------------------------------------------------------------------
void CScriptEditorPanel::GetConsoleText( char *pchText, size_t bufSize ) const
{
	wchar_t *temp = new wchar_t[ bufSize ];
	m_pScriptEntry->GetText( temp, bufSize * sizeof( wchar_t ) );
	g_pVGuiLocalize->ConvertUnicodeToANSI( temp, pchText, bufSize );
	delete[] temp;
}

//-----------------------------------------------------------------------------
// Purpose: Called whenever the user types text
//-----------------------------------------------------------------------------
void CScriptEditorPanel::OnTextChanged(Panel *panel)
{
	if (panel != m_pScriptEntry)
		return;

	RequestFocus();
	m_pScriptEntry->RequestFocus();
}


//-----------------------------------------------------------------------------
// Purpose: generic vgui command handler
//-----------------------------------------------------------------------------
void CScriptEditorPanel::OnCommand(const char *command)
{
	if ( !Q_stricmp( command, "Submit" ) )
	{
		// always go the end of the buffer when the user has typed something
		m_pOutput->GotoTextEnd();

		// Get the text from the script entry and store in a single string.
		int textLength = m_pScriptEntry->GetTextLength() + 2;
		CUtlBuffer scriptBuffer( 0, textLength, CUtlBuffer::TEXT_BUFFER );
		m_pScriptEntry->GetText( (char*)scriptBuffer.Base(), scriptBuffer.Size() );
		
		// Run the script 
		RunScript( scriptBuffer );
	}
	else
	{
		BaseClass::OnCommand(command);
	}
}



//-----------------------------------------------------------------------------
// Purpose: Run the specified script 
//-----------------------------------------------------------------------------
void CScriptEditorPanel::RunScript( const CUtlBuffer& scriptBuffer )
{
	// No script execution exists at this level, derived classes should
	// override this function to execute scripts.
}


//-----------------------------------------------------------------------------
// Purpose: Determine of the script entry panel has the current focus
//-----------------------------------------------------------------------------
bool CScriptEditorPanel::TextEntryHasFocus() const
{
	return ( input()->GetFocus() == m_pScriptEntry->GetVPanel() );
}


//-----------------------------------------------------------------------------
// Purpose: Request that the script entry panel receive the input focus
//-----------------------------------------------------------------------------
void CScriptEditorPanel::TextEntryRequestFocus()
{
	m_pScriptEntry->RequestFocus();
}


//-----------------------------------------------------------------------------
// Purpose: Lays out the controls within the panel
//-----------------------------------------------------------------------------
void CScriptEditorPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	// setup tab ordering
	GetFocusNavGroup().SetDefaultButton(m_pSubmit);

	IScheme *pScheme = scheme()->GetIScheme( GetScheme() );
	m_pScriptEntry->SetBorder(pScheme->GetBorder("DepressedButtonBorder"));
	m_pOutput->SetBorder(pScheme->GetBorder("DepressedButtonBorder"));

	// layout controls
	int wide, tall;
	GetSize(wide, tall);

	const int lineNumberWidth = 24;
	const int inset = 8;
	const int topHeight = 4;
	const int entryInset = 4;
	const int submitWide = 64;
	const int submitHeight = 20;
	const int submitInset = 7; // x inset to pull the submit button away from the frame grab
	
	const int editHeight = ( tall - ( submitHeight + topHeight + (inset * 4 ) ) ) / 2;

	m_pOutput->SetPos(inset, inset + topHeight); 
	m_pOutput->SetSize(wide - (inset * 2), editHeight );
	m_pOutput->InvalidateLayout();

	int nSubmitXPos = wide - ( inset + submitWide + submitInset );
	m_pSubmit->SetPos( nSubmitXPos, tall - (entryInset * 2 + submitHeight ));
	m_pSubmit->SetSize( submitWide, submitHeight );

	int nScriptEntryYPos = topHeight + ( inset * 2 ) + editHeight;
	m_pScriptEntry->SetPos( ( inset * 2 ) + lineNumberWidth,  nScriptEntryYPos );
	m_pScriptEntry->SetSize( wide - ( (inset * 3) + lineNumberWidth ), editHeight );

	m_pLineNumberPanel->SetPos( inset, nScriptEntryYPos );
	m_pLineNumberPanel->SetSize( lineNumberWidth, editHeight );
}


//-----------------------------------------------------------------------------
// Purpose: Apply the color, font, and size settings from the specified scheme
//-----------------------------------------------------------------------------
void CScriptEditorPanel::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_PrintColor = GetSchemeColor( "Console.TextColor", pScheme );
	m_DPrintColor = GetSchemeColor( "Console.DevTextColor", pScheme );
	m_pOutput->SetFont( pScheme->GetFont( "ConsoleText", IsProportional() ) );
	m_pScriptEntry->SetFont( pScheme->GetFont( "ConsoleText", IsProportional() ) );

	InvalidateLayout();
}

