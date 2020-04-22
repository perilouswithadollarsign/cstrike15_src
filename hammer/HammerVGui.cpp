#include "stdafx.h"
#include "hammer.h"
#include "hammervgui.h"
#include <vgui/IVGui.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include "vgui/IInput.h"
#include "vgui_controls/EditablePanel.h"
#include <VGuiMatSurface/IMatSystemSurface.h>
#include <matsys_controls/matsyscontrols.h>
#include "material.h"
#include "vgui_controls/AnimationController.h"
#include "inputsystem/iinputsystem.h"
#include "VGuiWnd.h"
#include "toolutils/enginetools_int.h"
#include "toolframework/ienginetool.h"
#include "inputsystem/iinputstacksystem.h"


//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------

// This window doesn't do anything other than tell CMatSystemSurface::CalculateMouseVisible to deem the mouse visible.
class CDummyPopupPanel : public vgui::Panel
{
public:
	virtual void PaintBackground() {}
	virtual void Paint() {}
};

static CHammerVGui s_HammerVGui;

CHammerVGui *HammerVGui()
{
	return &s_HammerVGui;
}

CHammerVGui::CHammerVGui(void)
{
	m_pActiveWindow = NULL;
	m_hMainWindow = NULL;
	m_pDummyPopup = NULL;
	m_bCurrentDialogIsModal = false;
	m_hHammerScheme = NULL;
	m_hVguiInputContext = INPUT_CONTEXT_HANDLE_INVALID;
}

//-----------------------------------------------------------------------------
// Setup the base vgui panels
//-----------------------------------------------------------------------------
bool CHammerVGui::Init( HWND hWindow )
{
	m_hMainWindow = hWindow;
	if ( !APP()->IsFoundryMode() ) // We don't need to init most stuff in Foundry mode because the engine has already done it.
	{
		// initialize vgui_control interfaces
		if (!vgui::VGui_InitInterfacesList( "HAMMER", &g_Factory, 1 ))
			return false;
		
		if ( !vgui::VGui_InitMatSysInterfacesList( "HAMMER", &g_Factory, 1 ) )
			return false;

		if ( !g_pMatSystemSurface )
			return false;
		
		// configuration settings
		vgui::system()->SetUserConfigFile("hammer.vdf", "EXECUTABLE_PATH");

		// Are we trapping input?
		g_pMatSystemSurface->EnableWindowsMessages( true );
	}

	m_hVguiInputContext = g_pInputStackSystem->PushInputContext();
	g_pMatSystemSurface->SetInputContext( m_hVguiInputContext );

	// Need to be able to play sounds through vgui
	// g_pMatSystemSurface->InstallPlaySoundFunc( VGui_PlaySound );

	// load scheme
	m_hHammerScheme = vgui::scheme()->LoadSchemeFromFile("//PLATFORM/Resource/SourceScheme.res", "Hammer");
	if ( !m_hHammerScheme )
	{
		return false;
	}

	if ( !APP()->IsFoundryMode() ) // We don't need to init most stuff in Foundry mode because the engine has already done it.
	{
		// Start the App running
		vgui::ivgui()->Start();
		vgui::ivgui()->SetSleep(false);

		// Create a popup window. This window doesn't do anything other than tell CMatSystemSurface::CalculateMouseVisible to deem the mouse visible.
		m_pDummyPopup = new CDummyPopupPanel;
		m_pDummyPopup->MakePopup( false, true );
		m_pDummyPopup->SetVisible( true );
	}

	return true;
}

void CHammerVGui::SetFocus( CVGuiWnd *pVGuiWnd )
{
	if ( pVGuiWnd == m_pActiveWindow )
		return;

	g_pInputSystem->PollInputState();
	vgui::ivgui()->RunFrame(); 

	g_pMatSystemSurface->SetAppDrivesInput( true );
	g_pInputSystem->DetachFromWindow( );

	// Disable mouse input on the previous panel so it doesn't get input the engine should get.
	if ( m_pActiveWindow && m_pActiveWindow->GetMainPanel() )
	{
		m_pActiveWindow->GetMainPanel()->SetMouseInputEnabled( false );
	}
	
	if ( pVGuiWnd )
	{
		m_pActiveWindow = pVGuiWnd;
		m_bCurrentDialogIsModal = m_pActiveWindow->IsModal();
		
		Assert( pVGuiWnd->GetMainPanel() != NULL );
		if ( pVGuiWnd->GetMainPanel() )
			pVGuiWnd->GetMainPanel()->SetMouseInputEnabled( true );
			
		g_pInputSystem->AttachToWindow( pVGuiWnd->GetParentWnd()->GetSafeHwnd() );
		g_pMatSystemSurface->SetAppDrivesInput( !m_bCurrentDialogIsModal );
		vgui::ivgui()->ActivateContext( pVGuiWnd->GetVGuiContext() );

		// If this is a modal VGuiWnd (like the model browser), don't let the engine's message loop get called at all
		// or else it'll screw up stuff - it'll give focus to other CVGuiWnds and the engine might drive
		// some vgui stuff instead of the VGuiWnd message loop (in CVGuiWnd::WindowProcVGui).
		if ( pVGuiWnd->IsModal() && enginetools )
			::EnableWindow( (HWND)enginetools->GetEngineHwnd(), false );
	}
	else
	{
		if ( enginetools )
		{
			// We can't call m_pActiveWindow->IsModal here because it might be in its destructor (as with the model browser)
			// and it's a virtual function.
			if ( m_bCurrentDialogIsModal )
				::EnableWindow( (HWND)enginetools->GetEngineHwnd(), true );

			g_pInputSystem->AttachToWindow( enginetools->GetEngineHwnd() );
			g_pMatSystemSurface->SetAppDrivesInput( true );
		}

		m_pActiveWindow = NULL;
		vgui::ivgui()->ActivateContext( vgui::DEFAULT_VGUI_CONTEXT );
	}
}

bool CHammerVGui::HasFocus( CVGuiWnd *pWnd )
{
	return m_pActiveWindow == pWnd;
}

void CHammerVGui::Simulate()
{
// VPROF( "CHammerVGui::Simulate" );

	if ( !IsInitialized() )
		return;

	g_pInputSystem->PollInputState();
	vgui::ivgui()->RunFrame(); 

	// run vgui animations
	vgui::GetAnimationController()->UpdateAnimations( vgui::system()->GetCurrentTime() );
}

void CHammerVGui::Shutdown()
{
	// Give panels a chance to settle so things
	//  Marked for deletion will actually get deleted

	if ( !IsInitialized() )
		return;

	if ( m_pDummyPopup )
	{
		delete m_pDummyPopup;
		m_pDummyPopup = NULL;
	}

	if ( m_hVguiInputContext != INPUT_CONTEXT_HANDLE_INVALID )
	{
		g_pMatSystemSurface->SetInputContext( NULL );
		g_pInputStackSystem->PopInputContext();
		m_hVguiInputContext = INPUT_CONTEXT_HANDLE_INVALID;
	}

	g_pInputSystem->PollInputState();
	vgui::ivgui()->RunFrame();

	// stop the App running
	vgui::ivgui()->Stop();
}

CHammerVGui::~CHammerVGui(void)
{
}
