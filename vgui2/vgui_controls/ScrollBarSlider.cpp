//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#define PROTECTED_THINGS_DISABLE

#include <vgui/IBorder.h>
#include <vgui/IInput.h>
#include <vgui/ISystem.h>
#include <vgui/IScheme.h>
#include <vgui/ISurface.h>
#include <vgui/MouseCode.h>
#include <keyvalues.h>

#include <vgui_controls/ScrollBarSlider.h>
#include <vgui_controls/Controls.h>

#include <math.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// The ScrollBarSlider is the scroll bar nob that moves up and down in through a range.
//-----------------------------------------------------------------------------
ScrollBarSlider::ScrollBarSlider(Panel *parent, const char *panelName, bool vertical) : Panel(parent, panelName)
{
	_vertical=vertical;	
	_dragging=false;
	_value=0;
	_range[0]=0;
	_range[1]=0;
	_rangeWindow=0;
	_buttonOffset=0;
	Q_memset( _ScrollBarSliderBorder, 0, sizeof( _ScrollBarSliderBorder ) );
	m_bCursorOver = false;
	RecomputeNobPosFromValue();
	SetBlockDragChaining( true );
	m_NobFocusColor = Color( 255, 255, 255, 255 );
	m_NobDragColor = Color( 255, 255, 255, 255 );
	m_nNobInset = 1;
}

//-----------------------------------------------------------------------------
// Purpose: Set the size of the ScrollBarSlider nob
//-----------------------------------------------------------------------------
void ScrollBarSlider::SetSize(int wide,int tall)
{
	BaseClass::SetSize(wide,tall);
	RecomputeNobPosFromValue();
}

//-----------------------------------------------------------------------------
// Purpose: Whether the scroll bar is vertical (true) or not (false)
//-----------------------------------------------------------------------------
bool ScrollBarSlider::IsVertical()
{
	return _vertical;
}

//-----------------------------------------------------------------------------
// Purpose: Set the ScrollBarSlider value of the nob.
//-----------------------------------------------------------------------------
void ScrollBarSlider::SetValue(int value)
{
	int oldValue = _value;

	if (value > _range[1] - _rangeWindow)
	{
		// note our scrolling range must take into acount _rangeWindow
		value = _range[1] - _rangeWindow;	
	}

	if (value < _range[0])
	{
		value = _range[0];
	}

	_value = value;
	RecomputeNobPosFromValue();

	if (_value != oldValue)
	{
		SendScrollBarSliderMovedMessage();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Get the ScrollBarSlider value of the nob.
//-----------------------------------------------------------------------------
int ScrollBarSlider::GetValue()
{
	return _value;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScrollBarSlider::PerformLayout()
{
	RecomputeNobPosFromValue();
	BaseClass::PerformLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Given the value of the ScrollBarSlider, adjust the ends of the nob.
//-----------------------------------------------------------------------------
void ScrollBarSlider::RecomputeNobPosFromValue()
{
	int wide, tall;
	GetPaintSize(wide, tall);

	float fwide = (float)( wide - 1 );
	float ftall = (float)( tall - 1 );
	float frange = (float)(_range[1] -_range[0]);
	float fvalue = (float)(_value - _range[0]);
	float frangewindow = (float)(_rangeWindow);
	float fper = ( frange != frangewindow ) ? fvalue / ( frange-frangewindow ) : 0;

	if ( frangewindow > 0 )
	{
		if ( frange <= 0.0 )
		{
			frange = 1.0;
		}

		float width, length;
		if (_vertical)
		{
			width = fwide;
			length = ftall;
		}
		else
		{
			width = ftall;
			length = fwide;
		}
		
		// our size is proportional to frangewindow/frange
		// the scroll bar nob's length reflects the amount of stuff on the screen 
		// vs the total amount of stuff we could scroll through in window
		// so if a window showed half its contents and the other half is hidden the
		// scroll bar's length is half the window.
		// if everything is on the screen no nob is displayed
		// frange is how many 'lines' of stuff we can display
		// frangewindow is how many 'lines' are in the display window
		
		// proportion of whole window that is on screen
		float proportion = frangewindow / frange;
		float fnobsize = length * proportion;
		if ( fnobsize < width ) fnobsize = (float)width;
		
		float freepixels = length - fnobsize;
		
		float firstpixel = freepixels * fper;
		
		_nobPos[0] = (int)( firstpixel );
		_nobPos[1] = (int)( firstpixel + fnobsize );
		
		if ( _nobPos[1] > length )
		{
			_nobPos[0] = (int)( length - fnobsize );
			_nobPos[1] = (int)length;
		}
		
	}

	Repaint();
}

//-----------------------------------------------------------------------------
// Purpose: Get the ScrollBarSlider value using the location of the nob ends.
//-----------------------------------------------------------------------------
void ScrollBarSlider::RecomputeValueFromNobPos()
{
	int wide, tall;
	GetPaintSize(wide, tall);

	float fwide = (float)( wide - 1 );
	float ftall = (float)( tall - 1 );
	float frange = (float)( _range[1] - _range[0] );
	float fvalue = (float)( _value - _range[0] );
	float fnob = (float)_nobPos[0];
	float frangewindow = (float)(_rangeWindow);

	if ( frangewindow > 0 )
	{
		if ( frange <= 0.0 )
		{
			frange = 1.0;
		}

		// set local width and length
		float width, length;
		if ( _vertical )
		{
			width = fwide;
			length = ftall;
		}
		else
		{
			width = ftall;
			length = fwide;
		}
		
		// calculate the size of the nob
		float proportion = frangewindow / frange;
		float fnobsize = length * proportion;
		
		if ( fnobsize < width )
		{
			fnobsize = width;
		}
		
		// Our scroll bar actually doesnt scroll through all frange lines in the truerange, we
		// actually only scroll through frange-frangewindow number of lines so we must take that 
		// into account when we calculate the value
		// convert to our local size system

		// Make sure we don't divide by zero
		if ( length - fnobsize == 0 )
		{
			fvalue = 0.0f;
		}
		else
		{
			fvalue = (frange - frangewindow) * ( fnob / ( length - fnobsize ) );
		}
	}

	// check to see if we should just snap to the bottom
	if (fabs(fvalue + _rangeWindow - _range[1]) < (0.01f * frange))
	{
		// snap to the end
		_value = _range[1] - _rangeWindow;
	}
	else
	{
		// Take care of rounding issues.
		_value = (int)( fvalue + _range[0] + 0.5);
	}

	// Clamp final result
	_value = ( _value < (_range[1] - _rangeWindow) ) ? _value : (_range[1] - _rangeWindow);

	if (_value < _range[0])
	{
		_value = _range[0];
	}
}

//-----------------------------------------------------------------------------
// Purpose: Check if the ScrollBarSlider can move through one or more pixels per
//			unit of its range.
//-----------------------------------------------------------------------------
bool ScrollBarSlider::HasFullRange()
{
	int wide, tall;
	GetPaintSize(wide, tall);

	float frangewindow = (float)(_rangeWindow);

	float checkAgainst = 0;
	if(_vertical)
	{
		checkAgainst = (float)tall;
	}
	else
	{
		checkAgainst = (float)wide;
	}

	if ( frangewindow > 0 )
	{
		if( frangewindow <= ( checkAgainst + _buttonOffset ) )
		{
			return true;
		}
	}

	return false;
}
	
//-----------------------------------------------------------------------------
// Purpose: Inform other watchers that the ScrollBarSlider was moved
//-----------------------------------------------------------------------------
void ScrollBarSlider::SendScrollBarSliderMovedMessage()
{	
	// send a changed message
	PostActionSignal(new KeyValues("ScrollBarSliderMoved", "position", _value));
}

void ScrollBarSlider::SendScrollBarSliderReleasedMessage()
{
	// send a released message
	PostActionSignal(new KeyValues("ScrollBarSliderReleased", "position", _value));
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this slider is actually drawing itself
//-----------------------------------------------------------------------------
bool ScrollBarSlider::IsSliderVisible( void )
{
	int itemRange = _range[1] - _range[0];

	// Don't draw nob, no items in list
	if ( itemRange <= 0 )
		return false ;

	// Not enough range
	if ( itemRange <= _rangeWindow )
		return false;

	return true;
}

static char const *g_pBorderStyles[ ScrollBarSlider::Slider_Count ] = 
{
	"ScrollBarSliderBorder",
	"ScrollBarSliderBorderHover",
	"ScrollBarSliderBorderDragging",
};
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScrollBarSlider::ApplySchemeSettings(IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetFgColor(GetSchemeColor("ScrollBarSlider.FgColor", pScheme));
	SetBgColor(GetSchemeColor("ScrollBarSlider.BgColor", pScheme));
	SetNobFocusColor(GetSchemeColor("ScrollBarSlider.NobFocusColor", GetBgColor(), pScheme ));
	SetNobDragColor( GetSchemeColor("ScrollBarSlider.NobDragColor", GetBgColor(), pScheme ));

	IBorder *pButtonBorder = pScheme->GetBorder("ButtonBorder");
	IBorder *pSliderBorder = pScheme->GetBorder("ScrollBarSliderBorder");

	// Prefer the sliderborder, but use ButtonBorder as a fallback
	IBorder *pFallback = pSliderBorder ? pSliderBorder : pButtonBorder;

	for ( int i = 0; i < Slider_Count; ++i )
	{
		IBorder *pBorder = pScheme->GetBorder( g_pBorderStyles[ i ] );
		_ScrollBarSliderBorder[ i ] = pBorder ? pBorder : pFallback;
	}

	const char *resourceString = pScheme->GetResourceString( "ScrollBarSlider.Inset" );
	if ( resourceString )
	{
		int nValue = Q_atoi(resourceString);
		if (IsProportional())
		{
			nValue = scheme()->GetProportionalScaledValueEx(GetScheme(), nValue);
		}
		m_nNobInset = MAX( nValue, 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ScrollBarSlider::Paint()
{
	int wide,tall;
	GetPaintSize(wide,tall);

	if ( !IsSliderVisible() )	
		return;

	SliderBorderType_t bt = Slider_Idle;
	Color col = GetFgColor();
	if ( _dragging )
	{
		col = m_NobDragColor;
		bt = Slider_Dragging;
	}
	else if ( m_bCursorOver && IsMouseOverNob() )
	{
		col = m_NobFocusColor;
		bt = Slider_Hover;
	}

	surface()->DrawSetColor( col );

	IBorder *pBorder = _ScrollBarSliderBorder[ bt ];

	int nInset = m_nNobInset;

	if (_vertical)
	{
		// Nob
		surface()->DrawFilledRect(nInset, _nobPos[0], wide - nInset, _nobPos[1]);

		// border
		if (pBorder)
		{
			pBorder->Paint(nInset, _nobPos[0], wide - nInset, _nobPos[1]);
		}
	}
	else
	{
		// horizontal nob
		surface()->DrawFilledRect(_nobPos[0], nInset, _nobPos[1], tall - nInset );

		// border
		if (pBorder)
		{
			pBorder->Paint(_nobPos[0], nInset, _nobPos[1], tall - nInset );
		}
	}

}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ScrollBarSlider::PaintBackground()
{
//	BaseClass::PaintBackground();
	
	int wide,tall;
	GetPaintSize(wide,tall);
	surface()->DrawSetColor(GetBgColor());
	surface()->DrawFilledRect(0, 0, wide-1, tall-1);
}

//-----------------------------------------------------------------------------
// Purpose: Set the range of the ScrollBarSlider
//-----------------------------------------------------------------------------
void ScrollBarSlider::SetRange(int min,int max)
{
	if(max<min)
	{
		max=min;
	}

	if(min>max)
	{
		min=max;
	}

	_range[0]=min;
	_range[1]=max;

	// update the value (forces it within the range)
	SetValue( _value );
	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: Get the range values of the ScrollBarSlider
//-----------------------------------------------------------------------------
void ScrollBarSlider::GetRange(int& min,int& max)
{
	min=_range[0];
	max=_range[1];
}

//-----------------------------------------------------------------------------
// Purpose: Respond to cursor movements, we only care about clicking and dragging
//-----------------------------------------------------------------------------
void ScrollBarSlider::OnCursorMoved(int x,int y)
{
	if (!_dragging)
	{
		return;
	}

//	input()->GetCursorPos(x, y);
//	ScreenToLocal(x, y);

	int wide, tall;
	GetPaintSize(wide, tall);
	tall;

	if (_vertical)
	{
		_nobPos[0] = _nobDragStartPos[0] + (y - _dragStartPos[1]);
		_nobPos[1] = _nobDragStartPos[1] + (y - _dragStartPos[1]);
		
		if (_nobPos[1] > tall)
		{
			_nobPos[0] = tall - (_nobPos[1] - _nobPos[0]);
			_nobPos[1] = tall;
			SetValue( _range[1] - _rangeWindow );
		}
	}
	else
	{
		_nobPos[0] = _nobDragStartPos[0] + (x - _dragStartPos[0]);
		_nobPos[1] = _nobDragStartPos[1] + (x - _dragStartPos[0]);
		
		if (_nobPos[1] > wide)
		{
			_nobPos[0] = wide - (_nobPos[1] - _nobPos[0]);
			_nobPos[1] = wide;
		}
		
	}
	if (_nobPos[0] < 0)
	{
		_nobPos[1] = _nobPos[1] - _nobPos[0];
		_nobPos[0] = 0;
		SetValue(0);
	}
	
	InvalidateLayout();		// not invalidatelayout - because it won't draw while we're scrolling the slider
	RecomputeValueFromNobPos();
//	Repaint();
	SendScrollBarSliderMovedMessage();
}

//-----------------------------------------------------------------------------
// Purpose: Respond to mouse clicks on the ScrollBarSlider
//-----------------------------------------------------------------------------
void ScrollBarSlider::OnMousePressed(MouseCode code)
{
	int x,y;
	input()->GetCursorPos(x,y);
	ScreenToLocal(x,y);

	if (_vertical)
	{
		if ((y >= _nobPos[0]) && (y < _nobPos[1]))
		{
			_dragging = true;
			input()->SetMouseCapture(GetVPanel());
			_nobDragStartPos[0] = _nobPos[0];
			_nobDragStartPos[1] = _nobPos[1];
			_dragStartPos[0] = x;
			_dragStartPos[1] = y;
		}
		else if (y < _nobPos[0])
		{
			// jump the bar up by the range window
			int val = GetValue();
			val -= _rangeWindow;
			SetValue(val);
		}
		else if (y >= _nobPos[1])
		{
			// jump the bar down by the range window
			int val = GetValue();
			val += _rangeWindow;
			SetValue(val);
		}
	}
	else
	{
		if((x >= _nobPos[0]) && (x < _nobPos[1]))
		{
			_dragging = true;
			input()->SetMouseCapture(GetVPanel());
			_nobDragStartPos[0] = _nobPos[0];
			_nobDragStartPos[1] = _nobPos[1];
			_dragStartPos[0] = x;
			_dragStartPos[1] = y;
		}
		else if (x < _nobPos[0])
		{
			// jump the bar up by the range window
			int val = GetValue();
			val -= _rangeWindow;
			SetValue(val);
		}
		else if (x >= _nobPos[1])
		{
			// jump the bar down by the range window
			int val = GetValue();
			val += _rangeWindow;
			SetValue(val);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Treat double clicks as single clicks
//-----------------------------------------------------------------------------
void ScrollBarSlider::OnMouseDoublePressed(MouseCode code)
{
	OnMousePressed(code);
}

//-----------------------------------------------------------------------------
// Purpose: Stop looking for mouse events when mouse is up.
//-----------------------------------------------------------------------------
void ScrollBarSlider::OnMouseReleased(MouseCode code)
{
	if ( !_dragging )
		return;
	_dragging = false;
	input()->SetMouseCapture(0);
	SendScrollBarSliderReleasedMessage();
}

//-----------------------------------------------------------------------------
// Purpose: Get the position of the ends of the ScrollBarSlider.
//-----------------------------------------------------------------------------
void ScrollBarSlider::GetNobPos(int& min, int& max)
{
	min=_nobPos[0];
	max=_nobPos[1];
}

//-----------------------------------------------------------------------------
// Purpose: Set the number of lines visible in the window the ScrollBarSlider is attached to
//-----------------------------------------------------------------------------
void ScrollBarSlider::SetRangeWindow(int rangeWindow)
{
	_rangeWindow = rangeWindow;
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of lines visible in the window the ScrollBarSlider is attached to
//-----------------------------------------------------------------------------
int ScrollBarSlider::GetRangeWindow()
{
	return _rangeWindow;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void ScrollBarSlider::SetButtonOffset(int buttonOffset)
{
	_buttonOffset = buttonOffset;
}

void ScrollBarSlider::OnCursorEntered()
{
	m_bCursorOver = true;
}

void ScrollBarSlider::OnCursorExited()
{
	m_bCursorOver = false;
}

void ScrollBarSlider::SetNobFocusColor( const Color &color )
{
	m_NobFocusColor = color;
}

void ScrollBarSlider::SetNobDragColor( const Color &color )
{
	m_NobDragColor = color;
}

bool ScrollBarSlider::IsMouseOverNob()
{
	int x,y;
	input()->GetCursorPos(x,y);
	ScreenToLocal(x,y);

	int nCheckValue = _vertical ? y : x;

	if ( ( nCheckValue >= _nobPos[0] ) && 
		( nCheckValue < _nobPos[1] ) )
	{
		return true;
	}
	return false;
}
