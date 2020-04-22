#ifndef _INCLUDED_JOYPAD_FOCUS_H
#define _INCLUDED_JOYPAD_FOCUS_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include <vgui_controls/PHandle.h>

// This class provides a way of using VGUI panels with a joypad.  Instead of specific joypad responses being coded
//  into every control and panel, this class allows the joypad to manipulate any set of panels that respond to
//  IsCursorOver and OnMousePressed/Released events.  Panels that want to allow joypad manipulation simply have to
//  register themselves with this on creation and remove themselves from this on destruction.

// holds a list of vgui panels that can have joypad cursor focus
//  along with the currently focused one

// responds to joypad codes to move this focus about and cause mouseclicks when a confirm button is pressed

#define NUM_JF_KEYS 6
#define JF_KEY_REPEAT_DELAY 500
#define JF_KEY_REPEAT_INTERVAL 250

namespace vgui
{
	class ImagePanelColored;
};

class CJoypadOutline;

class CJoypadFocus
{
public:
	enum JoypadFocusKey
	{
		JF_KEY_UP = 0,
		JF_KEY_DOWN,
		JF_KEY_LEFT,
		JF_KEY_RIGHT,
		JF_KEY_CONFIRM,
		JF_KEY_CANCEL,
	};
	struct FocusArea
	{		
		vgui::PHandle hPanel;
		bool bClickOnFocus;
		bool bModal;
	};
	CJoypadFocus();

	// registering for focus
	void AddToFocusList(vgui::Panel* pPanel, bool bClickOnFocus=false, bool bModal=false);
	void RemoveFromFocusList(vgui::Panel* pPanel);

	// changing focus
	void SetFocusPanel(int index);
	void SetFocusPanel(vgui::Panel* pPanel, bool bClickOnFocus=false);
	vgui::Panel* GetFocusPanel();
	int FindNextPanel(vgui::Panel *pSource, float angle);	

	// clicking
	bool OnJoypadButtonPressed(int keynum);	
	bool OnJoypadButtonReleased(int keynum);
	void CheckKeyRepeats();
	void ClickFocusPanel(bool bDown, bool bRightMouse);
	void DoubleClickFocusPanel(bool bRightMouse);
	
	void SetJoypadCodes(int iUpCode, int iDownCode, int iLeftCode, int iRightCode, int iConfirmCode, int iCancelCode);	
	void SetJoypadMode(bool b) { m_bJoypadMode = b; }
	bool IsJoypadMode() { return m_bJoypadMode; }

	// checks a panel and all its parents are visible
	static bool IsPanelReallyVisible(vgui::Panel *pPanel);
	
	// KF_ numbers for the joypad buttons
	int m_KeyNum[NUM_JF_KEYS];

	// status of the joypad buttons
	bool m_bKeyDown[NUM_JF_KEYS];
	float m_fNextKeyRepeatTime[NUM_JF_KEYS];

	// list of panels that have registered themselves for joypad focus
	CUtlVector<FocusArea> m_FocusAreas;
	FocusArea m_CurrentFocus;
	vgui::DHANDLE<CJoypadOutline> m_hOutline;

	int m_iModal;	// how many modal-type focus panels we have.  If there are more than 1, all non-modal panels will be ignored when moving around
	bool m_bJoypadMode;
	bool m_bDebugOutput;
};

CJoypadFocus* GetJoypadFocus();

// graphical representaion of joypad cursor focus - attaches to the focus' parent and puts itself
//  in front of the focus, sized to match
class CJoypadOutline : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CJoypadOutline, vgui::Panel );
public:
	CJoypadOutline(vgui::Panel *parent, const char *name);
	void ApplySchemeSettings(vgui::IScheme* pScheme);
	virtual void OnThink();
	virtual void Paint();
	void SizeTo(int x, int y, int w, int t);
	virtual void GetCornerTextureSize( int& w, int& h );
	vgui::ImagePanelColored* m_pImagePanel;
	vgui::PHandle m_hLastFocusPanel;
};

#endif // _INCLUDED_JOYPAD_FOCUS_H