//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: Implements all the functions exported by the GameUI dll
//
// $NoKeywords: $
//===========================================================================//

#ifdef COMPILER_MSVC
#pragma once
#endif

#include "vgui/vgui.h"
#include "tier3/tier3.h"

namespace vgui
{
	class Panel;
}

class IMatSystemSurface;
class CVGuiWnd;
FORWARD_DECLARE_HANDLE( InputContextHandle_t );


class CHammerVGui
{
public:
	CHammerVGui(void);
	~CHammerVGui(void);

	bool Init( HWND hWindow );
	void Simulate();
	void Shutdown();
	
	bool HasFocus( CVGuiWnd *pWnd );
	void SetFocus( CVGuiWnd *pWnd );

	bool IsInitialized() { return m_hMainWindow != NULL; };

	vgui::HScheme GetHammerScheme()	{ return m_hHammerScheme; }

	
protected:

	CVGuiWnd		*m_pActiveWindow;			// the VGUI window that has the focus
	bool			m_bCurrentDialogIsModal;	// m_pActiveWindow->IsModal()

	HWND			m_hMainWindow;
	vgui::Panel		*m_pDummyPopup;

	vgui::HScheme	m_hHammerScheme;

	InputContextHandle_t m_hVguiInputContext;
};

CHammerVGui *HammerVGui();