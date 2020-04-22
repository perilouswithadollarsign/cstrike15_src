//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef INPUTDIALOG_H
#define INPUTDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Controls.h>
#include <vgui_controls/Frame.h>

namespace vgui
{

class Label;
class Button;
class TextEntry;


//-----------------------------------------------------------------------------
// Purpose: Utility dialog base class - just has context kv and ok/cancel buttons
//-----------------------------------------------------------------------------
class BaseInputDialog : public Frame
{
	DECLARE_CLASS_SIMPLE( BaseInputDialog, Frame );

public:
	BaseInputDialog( vgui::Panel *parent, const char *title, bool bShowCancelButton = true );
	~BaseInputDialog();

	void DoModal( KeyValues *pContextKeyValues = NULL );

protected:
	virtual void PerformLayout();
	virtual void PerformLayout( int x, int y, int w, int h ) {}

	// command buttons
	virtual void OnCommand( const char *command );
	virtual void WriteDataToKeyValues( KeyValues *pKV, bool bOk ) {}

	void CleanUpContextKeyValues();
	KeyValues		*m_pContextKeyValues;
	vgui::Button	*m_pCancelButton;
	vgui::Button	*m_pOKButton;
};

//-----------------------------------------------------------------------------
// Purpose: Utility dialog, used to ask yes/no questions of the user
//-----------------------------------------------------------------------------
class InputMessageBox : public BaseInputDialog
{
	DECLARE_CLASS_SIMPLE( InputMessageBox, BaseInputDialog );

public:
	InputMessageBox( vgui::Panel *parent, const char *title, char const *prompt );
	~InputMessageBox();

protected:
	virtual void PerformLayout( int x, int y, int w, int h );

private:
	vgui::Label			*m_pPrompt;
};

//-----------------------------------------------------------------------------
// Purpose: Utility dialog, used to let user type in some text
//-----------------------------------------------------------------------------
class InputDialog : public BaseInputDialog
{
	DECLARE_CLASS_SIMPLE( InputDialog, BaseInputDialog );

public:
	InputDialog( vgui::Panel *parent, const char *title, char const *prompt, char const *defaultValue = "" );
	~InputDialog();

	void SetMultiline( bool state );

	/* action signals

		"InputCompleted"
			"text"	- the text entered

		"InputCanceled"
	*/
	void AllowNumericInputOnly( bool bOnlyNumeric );

protected:
	virtual void PerformLayout( int x, int y, int w, int h );

	// command buttons
	virtual void WriteDataToKeyValues( KeyValues *pKV, bool bOk );

private:
	vgui::Label			*m_pPrompt;
	vgui::TextEntry		*m_pInput;
};

//-----------------------------------------------------------------------------
// Purpose: Utility dialog, used to let user specify multiple bool/float/string values
//-----------------------------------------------------------------------------
class MultiInputDialog : public Frame
{
	DECLARE_CLASS_SIMPLE( MultiInputDialog, Frame );

public:
	MultiInputDialog( Panel *pParent, const char *pTitle, const char *pOKText = "#VGui_OK", const char *pCancelText = "#VGui_Cancel" );
	~MultiInputDialog();

	void SetOKCommand    ( KeyValues *pOKCommand );     // defaults to "InputCompleted" to match InputDialog
	void SetCancelCommand( KeyValues *pCancelCommand ); // defaults to "InputCanceled" to match InputDialog (yes, this is spelled incorrectly)

	void AddText( const char *pText );
	void AddEntry( const char *pName, const char *pPrompt, bool bDefaultValue );
	void AddEntry( const char *pName, const char *pPrompt, float flDefaultValue );
	void AddEntry( const char *pName, const char *pPrompt, const char *pDefaultValue );

	virtual void DoModal();
	virtual void PerformLayout();
	virtual void OnCommand( const char *pCommand );

private:
	void WriteDataToKeyValues( KeyValues *pKV );
	void PerformLayout( int x, int y, int w, int h );
	int GetLabelWidth();
	int GetContentWidth();
	Label *AddLabel( const char *pText );
	TextEntry *AddTextEntry( const char *pName, const char *pDefaultValue );

	enum EntryType_t { T_NONE, T_BOOL, T_FLOAT, T_STRING };

	CUtlVector< Label* >	m_prompts;
	CUtlVector< Panel* >	m_inputs; // TextEntry or CheckButton
	CUtlVector< EntryType_t >	m_entryTypes;

	Button	*m_pOKButton;
	Button	*m_pCancelButton;

	KeyValues		*m_pOKCommand;
	KeyValues		*m_pCancelCommand;

	int				m_nCurrentTabPosition;
};


} // namespace vgui

#endif // INPUTDIALOG_H
