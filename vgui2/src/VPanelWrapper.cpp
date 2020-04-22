//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <assert.h>

#include "VPanel.h"
#include "vgui_internal.h"

#include <vgui/IClientPanel.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct DmxElementUnpackStructure_t;

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Protects internal VPanel through the versionable interface IPanel
//-----------------------------------------------------------------------------
class VPanelWrapper : public vgui::IPanel
{
public:
	virtual void Init(VPANEL vguiPanel, IClientPanel *panel)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->Init(panel);
	}

	// returns a pointer to the Client panel
	virtual IClientPanel *Client(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		return ((VPanel *)vguiPanel)->Client();
	}

	// methods
	virtual void SetPos(VPANEL vguiPanel, int x, int y)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->SetPos(x, y);
	}

	virtual void GetPos(VPANEL vguiPanel, int &x, int &y)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->GetPos(x, y);
	}

	virtual void SetSize(VPANEL vguiPanel, int wide,int tall)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->SetSize(wide, tall);
	}

	virtual void GetSize(VPANEL vguiPanel, int &wide, int &tall)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->GetSize(wide, tall);
	}

	virtual void SetMinimumSize(VPANEL vguiPanel, int wide, int tall)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->SetMinimumSize(wide, tall);
	}

	virtual void GetMinimumSize(VPANEL vguiPanel, int &wide, int &tall)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->GetMinimumSize(wide, tall);
	}

	virtual void SetZPos(VPANEL vguiPanel, int z)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->SetZPos(z);
	}

	virtual int GetZPos(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		return ((VPanel *)vguiPanel)->GetZPos();
	}

	virtual void GetAbsPos(VPANEL vguiPanel, int &x, int &y)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->GetAbsPos(x, y);
	}

	virtual void GetClipRect(VPANEL vguiPanel, int &x0, int &y0, int &x1, int &y1)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->GetClipRect(x0, y0, x1, y1);
	}

	virtual void SetInset(VPANEL vguiPanel, int left, int top, int right, int bottom)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->SetInset(left, top, right, bottom);
	}

	virtual void GetInset(VPANEL vguiPanel, int &left, int &top, int &right, int &bottom)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->GetInset(left, top, right, bottom);
	}

	virtual void SetVisible(VPANEL vguiPanel, bool state)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->SetVisible(state);
	}

	virtual void SetEnabled(VPANEL vguiPanel, bool state)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->SetEnabled(state);
	}

	virtual bool IsVisible(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		return ((VPanel *)vguiPanel)->IsVisible();
	}

	virtual bool IsEnabled(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		return ((VPanel *)vguiPanel)->IsEnabled();
	}

	// Used by the drag/drop manager to always draw on top
	virtual bool IsTopmostPopup( VPANEL vguiPanel )
	{
		AssertAlignedConsole(vguiPanel);
		return ((VPanel *)vguiPanel)->IsTopmostPopup();
	}

	virtual void SetTopmostPopup( VPANEL vguiPanel, bool state )
	{
		AssertAlignedConsole(vguiPanel);
		return ((VPanel *)vguiPanel)->SetTopmostPopup( state );
	}

	virtual void SetParent(VPANEL vguiPanel, VPANEL newParent)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->SetParent((VPanel *)newParent);
	}

	virtual int GetChildCount(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		return ((VPanel *)vguiPanel)->GetChildCount();
	}

	virtual VPANEL GetChild(VPANEL vguiPanel, int index)
	{
		AssertAlignedConsole(vguiPanel);
		return (VPANEL)((VPanel *)vguiPanel)->GetChild(index);
	}

	virtual CUtlVector< VPANEL > &GetChildren( VPANEL vguiPanel )
	{
		AssertAlignedConsole(vguiPanel);
		return (CUtlVector< VPANEL > &)((VPanel *)vguiPanel)->GetChildren();
	}

	virtual VPANEL GetParent(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		return (VPANEL)((VPanel *)vguiPanel)->GetParent();
	}

	virtual void MoveToFront(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->MoveToFront();
	}

	virtual void MoveToBack(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->MoveToBack();
	}

	virtual bool HasParent(VPANEL vguiPanel, VPANEL potentialParent)
	{
		AssertAlignedConsole(vguiPanel);

		if (!vguiPanel)
			return false;

		return ((VPanel *)vguiPanel)->HasParent((VPanel *)potentialParent);
	}

	virtual bool IsPopup(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		return ((VPanel *)vguiPanel)->IsPopup();
	}

	virtual void SetPopup(VPANEL vguiPanel, bool state)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->SetPopup(state);
	}

	virtual bool IsFullyVisible( VPANEL vguiPanel )
	{
		AssertAlignedConsole(vguiPanel);
		return ((VPanel *)vguiPanel)->IsFullyVisible();
	}

	// calculates the panels current position within the hierarchy
	virtual void Solve(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->Solve();
	}

	// used by ISurface to store platform-specific data
	virtual SurfacePlat *Plat(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		return ((VPanel *)vguiPanel)->Plat();
	}

	virtual void SetPlat(VPANEL vguiPanel, SurfacePlat *Plat)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->SetPlat(Plat);
	}

	virtual const char *GetName(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		return ((VPanel *)vguiPanel)->GetName();
	}

	virtual const char *GetClassName(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		return ((VPanel *)vguiPanel)->GetClassName();
	}

	virtual HScheme GetScheme(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		return ((VPanel *)vguiPanel)->GetScheme();
	}

	virtual bool IsProportional(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		return Client(vguiPanel)->IsProportional();
	}
	
	virtual bool IsAutoDeleteSet(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		return Client(vguiPanel)->IsAutoDeleteSet();
	}
	
	virtual void DeletePanel(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		Client(vguiPanel)->DeletePanel();
	}

	virtual void SendMessage(VPANEL vguiPanel, KeyValues *params, VPANEL ifrompanel)
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->SendMessage(params, ifrompanel);
	}

	virtual void Think(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		Client(vguiPanel)->Think();
	}

	virtual void PerformApplySchemeSettings(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		Client(vguiPanel)->PerformApplySchemeSettings();
	}

	virtual void PaintTraverse(VPANEL vguiPanel, bool forceRepaint, bool allowForce)
	{
		AssertAlignedConsole(vguiPanel);
		Client(vguiPanel)->PaintTraverse(forceRepaint, allowForce);
	}

	virtual void Repaint(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		Client(vguiPanel)->Repaint();
	}

	virtual VPANEL IsWithinTraverse(VPANEL vguiPanel, int x, int y, bool traversePopups)
	{
		AssertAlignedConsole(vguiPanel);
		return Client(vguiPanel)->IsWithinTraverse(x, y, traversePopups);
	}

	virtual void OnChildAdded(VPANEL vguiPanel, VPANEL child)
	{
		AssertAlignedConsole(vguiPanel);
		Client(vguiPanel)->OnChildAdded(child);
	}

	virtual void OnSizeChanged(VPANEL vguiPanel, int newWide, int newTall)
	{
		AssertAlignedConsole(vguiPanel);
		Client(vguiPanel)->OnSizeChanged(newWide, newTall);
	}

	virtual void InternalFocusChanged(VPANEL vguiPanel, bool lost)
	{
		AssertAlignedConsole(vguiPanel);
		Client(vguiPanel)->InternalFocusChanged(lost);
	}

	virtual bool RequestInfo(VPANEL vguiPanel, KeyValues *outputData)
	{
		AssertAlignedConsole(vguiPanel);

		if ( !vguiPanel )
			return false;

		return Client(vguiPanel)->RequestInfo(outputData);
	}

	virtual void RequestFocus(VPANEL vguiPanel, int direction = 0)
	{
		AssertAlignedConsole(vguiPanel);
		Client(vguiPanel)->RequestFocus(direction);
	}

	virtual bool RequestFocusPrev(VPANEL vguiPanel, VPANEL existingPanel)
	{
		AssertAlignedConsole(vguiPanel);
		return Client(vguiPanel)->RequestFocusPrev(existingPanel);
	}

	virtual bool RequestFocusNext(VPANEL vguiPanel, VPANEL existingPanel)
	{
		AssertAlignedConsole(vguiPanel);
		return Client(vguiPanel)->RequestFocusNext(existingPanel);
	}

	virtual VPANEL GetCurrentKeyFocus(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		return Client(vguiPanel)->GetCurrentKeyFocus();
	}

	virtual int GetTabPosition(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		return Client(vguiPanel)->GetTabPosition();
	}

	virtual Panel *GetPanel(VPANEL vguiPanel, const char *moduleName)
	{
		AssertAlignedConsole(vguiPanel);

		if (!vguiPanel)
			return NULL;

		if (vguiPanel == g_pSurface->GetEmbeddedPanel())
			return NULL;

		// assert that the specified vpanel is from the same module as requesting the cast
		if ( !vguiPanel || stricmp(GetModuleName(vguiPanel), moduleName) )
		{
			// assert(!("GetPanel() used to retrieve a Panel * from a different dll than which which it was created. This is bad, you can't pass Panel * across dll boundaries else you'll break the versioning.  Please only use a VPANEL."));
			// this is valid for now
			return NULL;
		}
		return Client(vguiPanel)->GetPanel();
	}

	virtual const char *GetModuleName(VPANEL vguiPanel)
	{
		AssertAlignedConsole(vguiPanel);
		return Client(vguiPanel)->GetModuleName();
	}

	virtual void SetKeyBoardInputEnabled( VPANEL vguiPanel, bool state ) 
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->SetKeyBoardInputEnabled(state);
	}

	virtual void SetMouseInputEnabled( VPANEL vguiPanel, bool state ) 
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->SetMouseInputEnabled(state);
	}

	virtual bool IsMouseInputEnabled( VPANEL vguiPanel ) 
	{
		AssertAlignedConsole(vguiPanel);
		return ((VPanel *)vguiPanel)->IsMouseInputEnabled();
	}

	virtual bool IsKeyBoardInputEnabled( VPANEL vguiPanel ) 
	{
		AssertAlignedConsole(vguiPanel);
		return ((VPanel *)vguiPanel)->IsKeyBoardInputEnabled();
	}

	virtual void SetMessageContextId( VPANEL vguiPanel, int nContextId )
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->SetMessageContextId( nContextId );
	}

	virtual int GetMessageContextId( VPANEL vguiPanel )
	{
		AssertAlignedConsole(vguiPanel);
		return ((VPanel *)vguiPanel)->GetMessageContextId();
	}

	virtual const DmxElementUnpackStructure_t *GetUnpackStructure( VPANEL vguiPanel ) const
	{
		AssertAlignedConsole(vguiPanel);
		return ((VPanel *)vguiPanel)->GetUnpackStructure();
	}

	virtual void OnUnserialized( VPANEL vguiPanel, CDmxElement *pElement )
	{
		AssertAlignedConsole(vguiPanel);
		((VPanel *)vguiPanel)->OnUnserialized( pElement );
	}

	virtual void SetSiblingPin(VPANEL vguiPanel, VPANEL newSibling, byte iMyCornerToPin = 0, byte iSiblingCornerToPinTo = 0 )
	{
		AssertAlignedConsole(vguiPanel);
		return ((VPanel *)vguiPanel)->SetSiblingPin( (VPanel *)newSibling, iMyCornerToPin, iSiblingCornerToPinTo );

	}
};

EXPOSE_SINGLE_INTERFACE(VPanelWrapper, IPanel, VGUI_PANEL_INTERFACE_VERSION);

