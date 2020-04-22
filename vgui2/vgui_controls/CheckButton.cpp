//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdarg.h>
#include <stdio.h>

#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <keyvalues.h>

#include <vgui_controls/Image.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/TextImage.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Check box image
//-----------------------------------------------------------------------------
class CheckImage : public TextImage
{
public:
	CheckImage(CheckButton *CheckButton) : TextImage( "g" )
	{
		_CheckButton = CheckButton;
		_drawMode = 0;

		SetSize(20, 13);
	}

	virtual void Paint()
	{
		DrawSetTextFont(GetFont());

		if ( !_drawMode )
		{
			// draw background
			if (_CheckButton->IsEnabled() && _CheckButton->IsCheckButtonCheckable() )
			{
				DrawSetTextColor(_bgColor);
			}
			else
			{
				DrawSetTextColor(_CheckButton->GetDisabledBgColor());
			}
			DrawPrintChar(0, 1, 'g');
		
			// draw border box
			DrawSetTextColor(_borderColor1);
			DrawPrintChar(0, 1, 'e');
			DrawSetTextColor(_borderColor2);
			DrawPrintChar(0, 1, 'f');
		}
		else
		{
			// Left4Dead custom background/border:
			// always 1-pixel thick border, for proportional in-game menus
			// slightly rounded corners to match our visuals
			int x, y;
			GetPos( x, y );

			int wide, tall;
			GetContentSize( wide, tall );

			x += 1;
			wide -= 2;
			y += 1;
			tall -= 2;

			// draw background
			if (_CheckButton->IsEnabled() && _CheckButton->IsCheckButtonCheckable() )
			{
				surface()->DrawSetColor(_bgColor);
				surface()->DrawSetTextColor(_bgColor);
			}
			else
			{
				surface()->DrawSetColor(_CheckButton->GetDisabledBgColor());
				surface()->DrawSetTextColor(_CheckButton->GetDisabledBgColor());
			}
			surface()->DrawFilledRect( x+1, y+1, x+wide-1, y+tall-1 );

			// draw border box
			surface()->DrawSetColor(_borderColor1);
			surface()->DrawSetTextColor(_borderColor1);
			surface()->DrawFilledRect( x+1, y, x+wide-1, y+1 );
			surface()->DrawFilledRect( x, y+1, x+1, y+tall-1 );
			surface()->DrawSetColor(_borderColor2);
			surface()->DrawSetTextColor(_borderColor2);
			surface()->DrawFilledRect( x+1, y+tall-1, x+wide-1, y+tall );
			surface()->DrawFilledRect( x+wide-1, y+1, x+wide, y+tall-1 );
		}

		// draw selected check
		if (_CheckButton->IsSelected())
		{
			if ( !_CheckButton->IsEnabled() )
			{
				DrawSetTextColor( _CheckButton->GetDisabledFgColor() );
			}
			else
			{
				DrawSetTextColor(_checkColor);
			}

			DrawPrintChar(0, 2, 'b');
		}
	}

	Color _borderColor1;
	Color _borderColor2;
	Color _checkColor;

	Color _bgColor;

	int _drawMode;

private:
	CheckButton *_CheckButton;
};

DECLARE_BUILD_FACTORY_DEFAULT_TEXT( CheckButton, CheckButton );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CheckButton::CheckButton(Panel *parent, const char *panelName, const char *text) : ToggleButton(parent, panelName, text)
{
 	SetContentAlignment(a_west);
	m_bCheckButtonCheckable = true;

	// create the image
	_checkBoxImage = new CheckImage(this);

	SetTextImageIndex(1);
	SetImageAtIndex(0, _checkBoxImage, CHECK_INSET);

	_selectedFgColor = Color( 196, 181, 80, 255 );
	_disabledFgColor = Color(130, 130, 130, 255);
	_disabledBgColor = Color(62, 70, 55, 255);
}


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CheckButton::~CheckButton()
{
	delete _checkBoxImage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CheckButton::SetCheckDrawMode( int mode )
{
	_checkBoxImage->_drawMode = mode;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CheckButton::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetDefaultColor( GetSchemeColor("CheckButton.TextColor", pScheme), GetBgColor() );
	_checkBoxImage->_bgColor = GetSchemeColor("CheckButton.BgColor", Color(62, 70, 55, 255), pScheme);
	_checkBoxImage->_borderColor1 = GetSchemeColor("CheckButton.Border1", Color(20, 20, 20, 255), pScheme);
	_checkBoxImage->_borderColor2 = GetSchemeColor("CheckButton.Border2", Color(90, 90, 90, 255), pScheme);
	_checkBoxImage->_checkColor = GetSchemeColor("CheckButton.Check", Color(20, 20, 20, 255), pScheme);
	_selectedFgColor = GetSchemeColor("CheckButton.SelectedTextColor", GetSchemeColor("ControlText", pScheme), pScheme);
	_disabledFgColor = GetSchemeColor("CheckButton.DisabledFgColor", Color(130, 130, 130, 255), pScheme);
	_disabledBgColor = GetSchemeColor("CheckButton.DisabledBgColor", Color(62, 70, 55, 255), pScheme);

	Color bgArmedColor = GetSchemeColor( "CheckButton.ArmedBgColor", Color(62, 70, 55, 255), pScheme); 
	SetArmedColor( GetFgColor(), bgArmedColor );

	Color bgDepressedColor = GetSchemeColor( "CheckButton.DepressedBgColor", Color(62, 70, 55, 255), pScheme); 
	SetDepressedColor( GetFgColor(), bgDepressedColor );

	_highlightFgColor = GetSchemeColor( "CheckButton.HighlightFgColor", Color(62, 70, 55, 255), pScheme); 

	SetContentAlignment(Label::a_west);

	_checkBoxImage->SetFont( pScheme->GetFont("Marlett", IsProportional()) );
	_checkBoxImage->ResizeImageToContent();
	SetImageAtIndex(0, _checkBoxImage, CHECK_INSET);

	// don't draw a background
	SetPaintBackgroundEnabled(false);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
IBorder *CheckButton::GetBorder(bool depressed, bool armed, bool selected, bool keyfocus)
{
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Check the button
//-----------------------------------------------------------------------------
void CheckButton::SetSelected(bool state )
{
	if (m_bCheckButtonCheckable)
	{
		// send a message saying we've been checked
		KeyValues *msg = new KeyValues("CheckButtonChecked", "state", (int)state);
		PostActionSignal(msg);
		
		BaseClass::SetSelected(state);
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets whether or not the state of the check can be changed
//-----------------------------------------------------------------------------
void CheckButton::SetCheckButtonCheckable(bool state)
{
	m_bCheckButtonCheckable = state;
	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Gets a different foreground text color if we are selected
//-----------------------------------------------------------------------------
#ifdef _GAMECONSOLE
Color CheckButton::GetButtonFgColor()
{
	if (HasFocus())
	{
		return _selectedFgColor;
	}

	return BaseClass::GetButtonFgColor();
}
#else
Color CheckButton::GetButtonFgColor()
{
	if ( IsArmed() )
	{
		return _highlightFgColor;
	}

	if (IsSelected())
	{
		return _selectedFgColor;
	}

	return BaseClass::GetButtonFgColor();
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CheckButton::OnCheckButtonChecked(Panel *panel)
{
}
