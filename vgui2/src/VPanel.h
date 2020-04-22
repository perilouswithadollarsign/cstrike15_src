//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef VPANEL_H
#define VPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/Dar.h>
#include <vgui/IPanel.h>
#include "dmxloader/dmxelement.h"

#ifdef GetClassName
#undef GetClassName
#endif

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------

namespace vgui
{

class SurfaceBase;
class IClientPanel;
struct SerialPanel_t;

//-----------------------------------------------------------------------------
// Purpose: VGUI private implementation of panel
//-----------------------------------------------------------------------------
class VPanel
{
	DECLARE_DMXELEMENT_UNPACK_NAMESPACE(vgui);

public:
	VPanel();
	virtual ~VPanel();

	virtual void Init(IClientPanel *attachedClientPanel);

	virtual SurfacePlat *Plat();
	virtual void SetPlat(SurfacePlat *pl);

	virtual HPanel GetHPanel() { return _hPanel; } // safe pointer handling
	virtual void SetHPanel(HPanel hPanel) { _hPanel = hPanel; }

	virtual bool IsPopup();
	virtual void SetPopup(bool state);
	virtual bool IsFullyVisible();

	virtual void SetPos(int x, int y);
	virtual void GetPos(int &x, int &y);
	virtual void SetSize(int wide,int tall);
	virtual void GetSize(int& wide,int& tall);
	virtual void SetMinimumSize(int wide,int tall);
	virtual void GetMinimumSize(int& wide,int& tall);
	virtual void SetZPos(int z);
	virtual int  GetZPos();

	virtual void GetAbsPos(int &x, int &y);
	virtual void GetClipRect(int &x0, int &y0, int &x1, int &y1);
	virtual void SetInset(int left, int top, int right, int bottom);
	virtual void GetInset(int &left, int &top, int &right, int &bottom);

	virtual void Solve();

	virtual void SetVisible(bool state);
	virtual void SetEnabled(bool state);
	virtual bool IsVisible();
	virtual bool IsEnabled();
	virtual void SetParent(VPanel *newParent);
	virtual int GetChildCount();
	virtual VPanel *GetChild(int index);
	virtual CUtlVector< VPanel * > &GetChildren();
	virtual VPanel *GetParent();
	virtual void MoveToFront();
	virtual void MoveToBack();
	virtual bool HasParent(VPanel *potentialParent);

	// gets names of the object (for debugging purposes)
	virtual const char *GetName();
	virtual const char *GetClassName();

	virtual HScheme GetScheme();

	// handles a message
	virtual void SendMessage(KeyValues *params, VPANEL ifromPanel);

	// wrapper to get Client panel interface
	virtual IClientPanel *Client() { return _clientPanel; }

	// input interest
	virtual void SetKeyBoardInputEnabled(bool state);
	virtual void SetMouseInputEnabled(bool state);
	virtual bool IsKeyBoardInputEnabled();
	virtual bool IsMouseInputEnabled();

	virtual bool IsTopmostPopup() const;
	virtual void SetTopmostPopup( bool bEnable );

	virtual void SetMessageContextId( int nContextId );
	virtual int GetMessageContextId();

	virtual void OnUnserialized( CDmxElement *pElement );
	// sibling pins
	virtual void SetSiblingPin(VPanel *newSibling, byte iMyCornerToPin = 0, byte iSiblingCornerToPinTo = 0 );

public:
	virtual void GetInternalAbsPos(int &x, int &y);
private:

	Dar<VPanel*> _childDar;
	VPanel *_parent;
	SurfacePlat	*_plat;	// platform-specific data
	HPanel _hPanel;

	// our companion Client panel
	IClientPanel *_clientPanel;

	short _pos[2];
	short _size[2];
	short _minimumSize[2];

	short _inset[4];
	short _clipRect[4];	 // used for bounds solving
	short _absPos[2];	 // used for bounds solving

	short _zpos;	// z-order position
	
	bool _visible : 1;
	DECLARE_DMXELEMENT_BITFIELD( _visible, bool, VPanel )
	bool _enabled : 1;
	DECLARE_DMXELEMENT_BITFIELD( _enabled, bool, VPanel )
	bool _popup : 1;
	DECLARE_DMXELEMENT_BITFIELD( _popup, bool, VPanel )
	bool _mouseInput : 1; // used for popups
	DECLARE_DMXELEMENT_BITFIELD( _mouseInput, bool, VPanel )
	bool _kbInput : 1;
	DECLARE_DMXELEMENT_BITFIELD( _kbInput, bool, VPanel )
	bool _isTopmostPopup : 1;
	DECLARE_DMXELEMENT_BITFIELD( _isTopmostPopup, bool, VPanel )

	int	 m_nMessageContextId;

	VPanel  *_pinsibling;
	byte	_pinsibling_my_corner;
	byte	_pinsibling_their_corner;
};

}


#endif // VPANEL_H
