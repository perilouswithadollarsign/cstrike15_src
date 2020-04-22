//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// Purpose: Declaration of the CScriptEditorPanel class and associated helper
// classes. The ScriptEditorPanel class represents a vgui panel which contains
// a text editing panel which may be used to edit a script and text panel which
// displays output from the script.
//
//=============================================================================

#ifndef SCRIPTEDITORPANEL_H
#define SCRIPTEDITORPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/editablepanel.h"



//-----------------------------------------------------------------------------
// CLineNumberPanel -- A simple panel which is used to display line numbers 
// next to the TextEntry control in the script editor. This is done a separate
// panel in order to allow easy manipulation of the positioning.
//-----------------------------------------------------------------------------
class CLineNumberPanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CLineNumberPanel, vgui::Panel );

public:

	CLineNumberPanel( vgui::Panel *pParent, vgui::TextEntry *pTextEntry, const char *pchName );

	// Paint the background of the panel, including the line numbers
	virtual void PaintBackground();
	
	// Apply the settings from the provided scheme, and save the font to display line numbers.
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );


private:

	vgui::TextEntry *m_pTextEntry;	// Pointer to the text entry panel for which line numbers are to be displayed
	vgui::HFont	     m_hFont;		// Handle to the font in which the line numbers are to be displayed
	Color			 m_Color;		// Color in which the line numbers are to be displayed
};



//-----------------------------------------------------------------------------
// CScriptEditorPanel -- A vgui panel class which is used to edit a script. It
// consists of a RichText panel which displays the output of the script and 
// a TextEntry panel which may be used to edit the script. In addition the 
// panel has functionality for running, loading, and saving the script. The 
// virtual RunScript() function is expected to be overridden by a derived class
// to actually execute the script, but the CScriptEditorPanel may be used 
// directly if only editing is required.
//-----------------------------------------------------------------------------
class CScriptEditorPanel : public vgui::EditablePanel, public IConsoleDisplayFunc
{
	DECLARE_CLASS_SIMPLE( CScriptEditorPanel, vgui::EditablePanel );

public:
	
	CScriptEditorPanel( Panel *parent, const char *pchName );
	~CScriptEditorPanel();

	// Inherited from IConsoleDisplayFunc
	virtual void ColorPrint( const Color& clr, const char *pMessage );
	virtual void Print( const char *pMessage );
	virtual void DPrint( const char *pMessage );
	virtual void GetConsoleText( char *pchText, size_t bufSize ) const;

	// Clear the output console
	void Clear();

	// Run the specified script 
	virtual void RunScript( const CUtlBuffer& scriptBuffer );

	bool TextEntryHasFocus() const;
	void TextEntryRequestFocus();

private:		

	MESSAGE_FUNC_PTR( OnTextChanged, "TextChanged", panel );

	// vgui overrides
	virtual void PerformLayout();
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void OnCommand(const char *command);

	vgui::RichText		*m_pOutput;				// Panel which displays the script output
	vgui::TextEntry		*m_pScriptEntry;		// Panel used to input and edit the script
	CLineNumberPanel	*m_pLineNumberPanel;	// Panel used to display line numbers
	vgui::Button		*m_pSubmit;				// Button which issues the submit command
	Color				m_PrintColor;			// Output primary text color
	Color				m_DPrintColor;			// Output developer text color
};


#endif // SCRIPTEDITORPANEL_H
