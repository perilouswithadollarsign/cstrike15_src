//========= Copyright (c) 19o96-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <stdio.h>

#include <vgui/IPanel.h>
#include <vgui/IClientPanel.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui/Cursor.h>

#include "vgui_internal.h"
#include "VPanel.h"
#include "dmxloader/dmxelement.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

// DMX serializer fields.
BEGIN_DMXELEMENT_UNPACK_NAMESPACE_SIMPLE_NO_BASE( vgui, VPanel )
DMXELEMENT_UNPACK_SHORT( "xpos", "0", _pos[0] )
DMXELEMENT_UNPACK_SHORT( "ypos", "0", _pos[1] )
DMXELEMENT_UNPACK_SHORT( "width", "64", _size[0] )
DMXELEMENT_UNPACK_SHORT( "height", "24", _size[1] )
DMXELEMENT_UNPACK_SHORT( "min width", "0", _minimumSize[0] )
DMXELEMENT_UNPACK_SHORT( "min height", "0", _minimumSize[1] )
DMXELEMENT_UNPACK_SHORT( "left inset", "0", _inset[0] )
DMXELEMENT_UNPACK_SHORT( "top inset", "0", _inset[1] )
DMXELEMENT_UNPACK_SHORT( "right inset", "0", _inset[2] )
DMXELEMENT_UNPACK_SHORT( "bottom inset", "0", _inset[3] )
DMXELEMENT_UNPACK_BITFIELD( "visible", "1", BITFIELD_TYPE_BOOL, _visible )
DMXELEMENT_UNPACK_BITFIELD( "enabled", "1", BITFIELD_TYPE_BOOL, _enabled )
DMXELEMENT_UNPACK_BITFIELD( "popup", "0", BITFIELD_TYPE_BOOL, _popup )
DMXELEMENT_UNPACK_BITFIELD( "is topmost popup", "0", BITFIELD_TYPE_BOOL, _isTopmostPopup )
DMXELEMENT_UNPACK_BITFIELD( "mouse input", "1", BITFIELD_TYPE_BOOL, _mouseInput )
DMXELEMENT_UNPACK_BITFIELD( "keyboard input", "1", BITFIELD_TYPE_BOOL, _kbInput )
DMXELEMENT_UNPACK_SHORT( "zpos", "0", _zpos )
END_DMXELEMENT_UNPACK_NAMESPACE( vgui, VPanel, s_pUnpackParams )

// Lame copy from Panel
enum PinCorner_e 
{
	PIN_TOPLEFT = 0,
	PIN_TOPRIGHT,
	PIN_BOTTOMLEFT,
	PIN_BOTTOMRIGHT,

	// For sibling pinning
	PIN_CENTER_TOP,
	PIN_CENTER_RIGHT,
	PIN_CENTER_BOTTOM,
	PIN_CENTER_LEFT,

	NUM_PIN_POINTS,
};

float PinDeltas[NUM_PIN_POINTS][2] =
{
	{ 0, 0 },	// PIN_TOPLEFT,
	{ 1, 0 },	// PIN_TOPRIGHT,
	{ 0, 1 },	// PIN_BOTTOMLEFT,
	{ 1, 1 },	// PIN_BOTTOMRIGHT,
	{ 0.5, 0 },	// PIN_CENTER_TOP,
	{ 1, 0.5 },	// PIN_CENTER_RIGHT,
	{ 0.5, 1 },	// PIN_CENTER_BOTTOM,
	{ 0, 0.5 },	// PIN_CENTER_LEFT,
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
VPanel::VPanel()
{
	_pos[0] = _pos[1] = 0;
	_absPos[0] = _absPos[1] = 0;
	_size[0] = _size[1] = 0;

	_minimumSize[0] = 0;
	_minimumSize[1] = 0;

	_zpos = 0;

	_inset[0] = _inset[1] = _inset[2] = _inset[3] = 0;
	_clipRect[0] = _clipRect[1] = _clipRect[2] = _clipRect[3] = 0;

	_visible = true;
	_enabled = true;
	_clientPanel = NULL;
	_parent = NULL;
	_plat = NULL;
	_popup = false;
	_isTopmostPopup = false;
	_hPanel = INVALID_PANEL;

	_mouseInput = true; // by default you want mouse and kb input to this panel
	_kbInput = true;

	m_nMessageContextId = -1;
	_pinsibling = NULL;
	_pinsibling_my_corner = PIN_TOPLEFT;
	_pinsibling_their_corner = PIN_TOPLEFT;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
VPanel::~VPanel()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::Init(IClientPanel *attachedClientPanel)
{
	AssertAlignedConsole(attachedClientPanel);
	_clientPanel = attachedClientPanel;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::Solve()
{
	short basePos[2];
	basePos[0] = _pos[0];
	basePos[1] = _pos[1];

	int baseSize[2];
	GetSize( baseSize[0], baseSize[1] );

	VPanel *parent = GetParent();
	if (IsPopup())
	{
		// if we're a popup, draw at the highest level
		parent = (VPanel *)g_pSurface->GetEmbeddedPanel();
	}

	int pabs[2];
	if ( parent )
	{
		parent->GetAbsPos(pabs[0], pabs[1]);
	}

	if ( _pinsibling )
	{
		int sibPos[2];
		int sibSize[2];
		_pinsibling->GetInternalAbsPos( sibPos[0], sibPos[1] );
		_pinsibling->GetSize( sibSize[0], sibSize[1] );

		for ( int i = 0; i < 2; i++ )
		{
			if ( parent )
			{
				sibPos[i] -= pabs[i];
			}

			// Determine which direction positive values move in. For center pins, we use screen relative signs.
			int iSign = 1;
			if ( i == 0 && (_pinsibling_their_corner == PIN_CENTER_LEFT || _pinsibling_their_corner == PIN_TOPLEFT || _pinsibling_their_corner == PIN_BOTTOMLEFT) )
			{
				iSign = -1;
			}
			else if ( i == 1 && (_pinsibling_their_corner == PIN_CENTER_TOP || _pinsibling_their_corner == PIN_TOPLEFT || _pinsibling_their_corner == PIN_TOPRIGHT) )
			{
				iSign = -1;
			}

			int iPos = sibPos[i] + (sibSize[i] * PinDeltas[_pinsibling_their_corner][i]);
			iPos -= (baseSize[i] * PinDeltas[_pinsibling_my_corner][i]);
			iPos += basePos[i] * iSign;

			basePos[i] = iPos;
		}
	}

	int absX = basePos[0];
	int absY = basePos[1];
	_absPos[0] = basePos[0];
	_absPos[1] = basePos[1];

	// put into parent space
	int pinset[4] = {0, 0, 0, 0}; 
	if ( parent )
	{
		parent->GetInset( pinset[0], pinset[1], pinset[2], pinset[3] );

		absX += pabs[0] + pinset[0];
		absY += pabs[1] + pinset[1];

		_absPos[0] = clamp( absX, -32767, 32767 );
		_absPos[1] = clamp( absY, -32767, 32767 );
	}

	// set initial bounds
	_clipRect[0] = _absPos[0];
	_clipRect[1] = _absPos[1];

	int absX2 = absX + baseSize[0];
	int absY2 = absY + baseSize[1];
	_clipRect[2] = clamp( absX2, -32767, 32767 );
	_clipRect[3] = clamp( absY2, -32767, 32767 );

	// clip to parent, if we're not a popup
	if ( parent && !IsPopup() )
	{ 
		int pclip[4];
		parent->GetClipRect(pclip[0], pclip[1], pclip[2], pclip[3]);

		if (_clipRect[0] < pclip[0])
		{
			_clipRect[0] = pclip[0];
		}

		if (_clipRect[1] < pclip[1])
		{
			_clipRect[1] = pclip[1];
		}

		if(_clipRect[2] > pclip[2])
		{
			_clipRect[2] = pclip[2] - pinset[2];
		}

		if(_clipRect[3] > pclip[3])
		{
			_clipRect[3] = pclip[3] - pinset[3];
		}

		if ( _clipRect[0] > _clipRect[2] )
		{
			_clipRect[2] = _clipRect[0];
		}

		if ( _clipRect[1] > _clipRect[3] )
		{
			_clipRect[3] = _clipRect[1];
		}
	}

	Assert( _clipRect[0] <= _clipRect[2] );
	Assert( _clipRect[1] <= _clipRect[3] );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::SetPos(int x, int y)
{
	// This is a hack to get popups to position relative to their "split screen" context parent panel
	if ( m_nMessageContextId != -1 && IsPopup() )
	{
		int px, py;
		g_pSurface->GetAbsPosForContext( m_nMessageContextId, px, py );
		x += px;
		y += py;
	}

	_pos[0] = x;
	_pos[1] = y;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::GetPos(int &x, int &y)
{
	x = _pos[0];
	y = _pos[1];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::SetSize(int wide,int tall)
{
	if (wide<_minimumSize[0])
	{
		wide=_minimumSize[0];
	}
	if (tall<_minimumSize[1])
	{
		tall=_minimumSize[1];
	}

	if (_size[0] == wide && _size[1] == tall)
		return;

	_size[0]=wide;
	_size[1]=tall;

	Client()->OnSizeChanged(wide, tall);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::GetSize(int& wide,int& tall)
{
	wide=_size[0];
	tall=_size[1];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::SetMinimumSize(int wide,int tall)
{
	_minimumSize[0]=wide;
	_minimumSize[1]=tall;

	// check if we're currently smaller than the new minimum size
	int currentWidth = _size[0];
	if (currentWidth < wide)
	{
		currentWidth = wide;
	}
	int currentHeight = _size[1];
	if (currentHeight < tall)
	{
		currentHeight = tall;
	}

	// resize to new minimum size if necessary
	if (currentWidth != _size[0] || currentHeight != _size[1])
	{
		SetSize(currentWidth, currentHeight);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::GetMinimumSize(int &wide, int &tall)
{
	wide = _minimumSize[0];
	tall = _minimumSize[1];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::SetVisible(bool state)
{
	if (_visible == state)
		return;

	// need to tell the surface (in case special window processing needs to occur)
	g_pSurface->SetPanelVisible((VPANEL)this, state);

	_visible = state;

	if( IsPopup() )
	{
		vgui::g_pSurface->CalculateMouseVisible();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::SetEnabled(bool state)
{
	_enabled = state;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool VPanel::IsVisible()
{
	return _visible;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool VPanel::IsEnabled()
{
	return _enabled;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::GetAbsPos(int &x, int &y)
{
	x = _absPos[0];
	y = _absPos[1];

	g_pSurface->OffsetAbsPos( x, y );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::GetInternalAbsPos(int &x, int &y)
{
	x = _absPos[0];
	y = _absPos[1];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::GetClipRect(int &x0, int &y0, int &x1, int &y1)
{
	x0 = _clipRect[0];
	y0 = _clipRect[1];
	x1 = _clipRect[2];
	y1 = _clipRect[3];

	g_pSurface->OffsetAbsPos( x0, y0 );
	g_pSurface->OffsetAbsPos( x1, y1 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::SetInset(int left, int top, int right, int bottom)
{
	_inset[0] = left;
	_inset[1] = top;
	_inset[2] = right;
	_inset[3] = bottom;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::GetInset(int &left, int &top, int &right, int &bottom)
{
	left = _inset[0];
	top = _inset[1];
	right = _inset[2];
	bottom = _inset[3];
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::SetParent(VPanel *newParent)
{
	AssertAlignedConsole(newParent);

	if (this == newParent)
		return;

	if (_parent == newParent)
		return;
	
	if (_parent != NULL)
	{
		_parent->_childDar.RemoveElement(this);
		_parent = 0;
	}

	if (newParent != NULL)
	{
		_parent = newParent;
		_parent->_childDar.PutElement(this);
		SetZPos(_zpos);						// re-sort parent's panel order if necessary
		if (_parent->Client())
		{
			_parent->Client()->OnChildAdded((VPANEL)this);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int VPanel::GetChildCount()
{
	return _childDar.GetCount();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
VPanel *VPanel::GetChild(int index)
{
	AssertAlignedConsole(&_childDar);
	return _childDar[index];
}

CUtlVector< VPanel *> &VPanel::GetChildren()
{
	AssertAlignedConsole(&_childDar);
	return _childDar;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
VPanel *VPanel::GetParent()
{
	AssertAlignedConsole(_parent);
	return _parent;
}

//-----------------------------------------------------------------------------
// Purpose: Sets the Z position of a panel and reorders it appropriately
//-----------------------------------------------------------------------------
void VPanel::SetZPos(int z)
{
	_zpos = z;

	if (_parent)
	{
		// find the child in the list
		int childCount = _parent->GetChildCount();
		int i;
		for (i = 0; i < childCount; i++)
		{
			if (_parent->GetChild(i) == this)
				break;
		}

		if (i == childCount)
			return;

		while (1)
		{
			VPanel *prevChild = NULL, *nextChild = NULL;

			if ( i > 0 )
			{
				prevChild = _parent->GetChild( i - 1 );
			}
			if ( i <(childCount - 1) )
			{
				nextChild = _parent->GetChild( i + 1 );
			}

			// check either side of the child to see if it should move
			if ( i > 0 && prevChild && ( prevChild->_zpos > _zpos ) )
			{
				// swap with the lower
				_parent->_childDar.SetElementAt(prevChild, i);
				_parent->_childDar.SetElementAt(this, i - 1);
				i--;
			}
			else if (i < (childCount - 1) && nextChild && ( nextChild->_zpos < _zpos ) )
			{
				// swap with the higher
				_parent->_childDar.SetElementAt(nextChild, i);
				_parent->_childDar.SetElementAt(this, i + 1);
				i++;
			}
			else
			{
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the z position of this panel
//-----------------------------------------------------------------------------
int VPanel::GetZPos()
{
	return _zpos;
}

//-----------------------------------------------------------------------------
// Purpose: Moves the panel to the front of the z-order
//-----------------------------------------------------------------------------
void VPanel::MoveToFront(void)
{
	g_pSurface->MovePopupToFront((VPANEL)this);

	if (_parent)
	{
		// move this panel to the end of it's parents list
		_parent->_childDar.MoveElementToEnd(this);

		// Validate the Z order
		int i = _parent->_childDar.GetCount() - 2;
		while (i >= 0)
		{
			if (_parent->_childDar[i]->_zpos > _zpos)
			{
				// we can't be in front of this; swap positions
				_parent->_childDar.SetElementAt(_parent->_childDar[i], i + 1);
				_parent->_childDar.SetElementAt(this, i);

				// check the next value
				i--;
			}
			else
			{
				// order valid
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Moves the panel to the back of the z-order
//-----------------------------------------------------------------------------
void VPanel::MoveToBack()
{
	if (_parent)
	{
		// move this panel to the end of it's parents list
		_parent->_childDar.RemoveElement(this);
		_parent->_childDar.InsertElementAt(this, 0);

		// Validate the Z order
		int i = 1;
		while (i < _parent->_childDar.GetCount())
		{
			if (_parent->_childDar[i]->_zpos < _zpos)
			{
				// we can't be behind this; swap positions
				_parent->_childDar.SetElementAt(_parent->_childDar[i], i - 1);
				_parent->_childDar.SetElementAt(this, i);

				// check the next value
				i++;
			}
			else
			{
				// order valid
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Iterates up the hierarchy looking to see if a panel has the specified ancestor
//-----------------------------------------------------------------------------
bool VPanel::HasParent(VPanel *potentialParent)
{
	if (this == potentialParent)
		return true;

	if (_parent)
	{
		return _parent->HasParent(potentialParent);
	}

	return false;
}

SurfacePlat *VPanel::Plat()
{
	AssertAlignedConsole(_plat);
	return _plat;
}

void VPanel::SetPlat(SurfacePlat *Plat)
{
	AssertAlignedConsole(Plat);
	_plat = Plat;
}

bool VPanel::IsPopup()
{
	return _popup;
}

void VPanel::SetPopup(bool state)
{
	_popup = state;
}

bool VPanel::IsTopmostPopup() const
{
	return _isTopmostPopup;
}

void VPanel::SetTopmostPopup( bool bEnable )
{
	_isTopmostPopup = bEnable;
}

bool VPanel::IsFullyVisible()
{
	// recursively check to see if the panel and all it's parents are visible
	VPanel *panel = this;
	while (panel)
	{
		if (!panel->_visible)
		{
			return false;
		}

		panel = panel->_parent;
	}

	// we're visible all the way up the hierarchy
	return true;
}

const char *VPanel::GetName()
{
	return Client()->GetName();
}

const char *VPanel::GetClassName()
{
	return Client()->GetClassName();
}

HScheme VPanel::GetScheme()
{
	return Client()->GetScheme();
}


void VPanel::SendMessage(KeyValues *params, VPANEL ifrompanel)
{
	Client()->OnMessage(params, ifrompanel);
}


void VPanel::SetKeyBoardInputEnabled(bool state)
{
	_kbInput = state;
}

void VPanel::SetMouseInputEnabled(bool state)
{
	_mouseInput = state;
}

bool VPanel::IsKeyBoardInputEnabled()
{
	return _kbInput;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool VPanel::IsMouseInputEnabled()
{
	return _mouseInput;
}

void VPanel::SetMessageContextId( int nContextId )
{
	m_nMessageContextId = nContextId;
}

int VPanel::GetMessageContextId()
{
	return m_nMessageContextId;
}

// Purpose: sibling pins
//-----------------------------------------------------------------------------
void VPanel::SetSiblingPin(VPanel *newSibling, byte iMyCornerToPin, byte iSiblingCornerToPinTo )
{
	_pinsibling = newSibling;
	_pinsibling_my_corner = iMyCornerToPin;
	_pinsibling_their_corner = iSiblingCornerToPinTo;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void VPanel::OnUnserialized( CDmxElement *pElement )
{
}





