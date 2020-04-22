//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include <string.h>

#include <Carbon/Carbon.h>

#include "vgui_internal.h"
#include "VPanel.h"
#include "UtlVector.h"
#include <KeyValues.h>

#include <vgui/VGUI.h>
#include <vgui/ISystem.h>
#include <vgui/IClientPanel.h>
#include <vgui/IInputInternal.h>
#include <vgui/IPanel.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui/KeyCode.h>
#include <vgui/MouseCode.h>
#include "vgui/Cursor.h"

#include "UtlLinkedList.h"
#include "tier0/icommandline.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgoff.h"

bool IsDispatchingMessageQueue( void );

using namespace vgui;

class CInputOSX : public IInputInternal
{
public:
	CInputOSX();
	~CInputOSX();

	virtual void RunFrame();

	virtual void PanelDeleted(VPANEL panel);

	virtual void UpdateMouseFocus(int x, int y);
	virtual void SetMouseFocus(VPANEL newMouseFocus);

	virtual void SetCursorPos(int x, int y);
	virtual void UpdateCursorPosInternal( int x, int y );
	virtual void GetCursorPos(int &x, int &y);
	virtual void SetCursorOveride(HCursor cursor);
	virtual HCursor GetCursorOveride();


	virtual void SetMouseCapture(VPANEL panel);
	virtual VPANEL GetMouseCapture();

	virtual VPANEL GetFocus();
	virtual VPANEL GetCalculatedFocus();
	virtual VPANEL GetMouseOver();
	virtual VPANEL GetCurrentDefaultButton();
	virtual VPANEL GetLastMousePressedPanel();
    virtual void RegisterPotentialDefaultButton( VPANEL panel );
    virtual void UnregisterPotentialDefaultButton( VPANEL panel );

	virtual bool WasMousePressed(MouseCode code);
	virtual bool WasMouseDoublePressed(MouseCode code);
	virtual bool IsMouseDown(MouseCode code);
	virtual bool WasMouseReleased(MouseCode code);
	virtual bool WasKeyPressed(KeyCode code);
	virtual bool IsKeyDown(KeyCode code);
	virtual bool WasKeyTyped(KeyCode code);
	virtual bool WasKeyReleased(KeyCode code);

	virtual void GetKeyCodeText(KeyCode code, char *buf, int buflen);

	virtual bool InternalCursorMoved(int x,int y); //expects input in surface space
	virtual bool InternalMousePressed(MouseCode code);
	virtual bool InternalMouseDoublePressed(MouseCode code);
	virtual bool InternalMouseReleased(MouseCode code);
	virtual bool InternalMouseWheeled(int delta);
	virtual bool InternalKeyCodePressed(KeyCode code);
	virtual void InternalKeyCodeTyped(KeyCode code);
	virtual void InternalKeyTyped(wchar_t unichar);
	virtual bool InternalKeyCodeReleased(KeyCode code);

	virtual void SetKeyCodeState( KeyCode code, bool bPressed );
	virtual void SetMouseCodeState( MouseCode code, MouseCodeState_t state );
	virtual void UpdateButtonState( const InputEvent_t &event );
	
	virtual VPANEL GetAppModalSurface();
	// set the modal dialog panel.
	// all events will go only to this panel and its children.
	virtual void SetAppModalSurface(VPANEL panel);
	// release the modal dialog panel
	// do this when your modal dialog finishes.
	virtual void ReleaseAppModalSurface();

	// returns true if the specified panel is a child of the current modal panel
	// if no modal panel is set, then this always returns TRUE
	virtual bool IsChildOfModalPanel(VPANEL panel, bool checkModalSubTree = true );

	// Creates/ destroys "input" contexts, which contains information
	// about which controls have mouse + key focus, for example.
	virtual HInputContext CreateInputContext();
	virtual void DestroyInputContext( HInputContext context ); 

	// Associates a particular panel with an input context
	// Associating NULL is valid; it disconnects the panel from the context
	virtual void AssociatePanelWithInputContext( HInputContext context, VPANEL pRoot );

	virtual void PostCursorMessage( );
	virtual void HandleExplicitSetCursor( );

	// Activates a particular input context, use DEFAULT_INPUT_CONTEXT
	// to get the one normally used by VGUI
	virtual void ActivateInputContext( HInputContext context );

	virtual void ResetInputContext( HInputContext context );

	virtual void GetCursorPosition( int &x, int &y );

	virtual void SetIMEWindow( void *hwnd );
	virtual void *GetIMEWindow();

	// Change keyboard layout type
	virtual void OnChangeIME( bool forward );
	virtual int  GetCurrentIMEHandle();
	virtual int  GetEnglishIMEHandle();

	// Returns the Language Bar label (Chinese, Korean, Japanese, Russion, Thai, etc.)
	virtual void GetIMELanguageName( wchar_t *buf, int unicodeBufferSizeInBytes );
	// Returns the short code for the language (EN, CH, KO, JP, RU, TH, etc. ).
	virtual void GetIMELanguageShortCode( wchar_t *buf, int unicodeBufferSizeInBytes );

	// Call with NULL dest to get item count
	virtual int	 GetIMELanguageList( LanguageItem *dest, int destcount );
	virtual int	 GetIMEConversionModes( ConversionModeItem *dest, int destcount );
	virtual int	 GetIMESentenceModes( SentenceModeItem *dest, int destcount );

	virtual void OnChangeIMEByHandle( int handleValue );
	virtual void OnChangeIMEConversionModeByHandle( int handleValue );
	virtual void OnChangeIMESentenceModeByHandle( int handleValue );

	virtual void OnInputLanguageChanged();
	virtual void OnIMEStartComposition();
	virtual void OnIMEComposition( int flags );
	virtual void OnIMEEndComposition();

	virtual void OnIMEShowCandidates();
	virtual void OnIMEChangeCandidates();
	virtual void OnIMECloseCandidates();

	virtual void OnIMERecomputeModes();

	virtual int  GetCandidateListCount();
	virtual void GetCandidate( int num, wchar_t *dest, int destSizeBytes );
	virtual int  GetCandidateListSelectedItem();
	virtual int  GetCandidateListPageSize();
	virtual int  GetCandidateListPageStart();

	virtual void SetCandidateWindowPos( int x, int y );
	virtual bool GetShouldInvertCompositionString();
	virtual bool CandidateListStartsAtOne();

	virtual void SetCandidateListPageStart( int start );

	// Passes in a keycode which allows hitting other mouse buttons w/o cancelling capture mode
	virtual void SetMouseCaptureEx(VPANEL panel, MouseCode captureStartMouseCode );

	virtual void RegisterKeyCodeUnhandledListener( VPANEL panel );
	virtual void UnregisterKeyCodeUnhandledListener( VPANEL panel );

	// Posts unhandled message to all interested panels
	virtual void OnKeyCodeUnhandled( int keyCode );

	// Assumes subTree is a child panel of the root panel for the vgui contect
	//  if restrictMessagesToSubTree is true, then mouse and kb messages are only routed to the subTree and it's children and mouse/kb focus
	//   can only be on one of the subTree children, if a mouse click occurs outside of the subtree, and "UnhandledMouseClick" message is sent to unhandledMouseClickListener panel
	//   if it's set
	//  if restrictMessagesToSubTree is false, then mouse and kb messages are routed as normal except that they are not routed down into the subtree
	//   however, if a mouse click occurs outside of the subtree, and "UnhandleMouseClick" message is sent to unhandledMouseClickListener panel
	//   if it's set
	virtual void	SetModalSubTree( VPANEL subTree, VPANEL unhandledMouseClickListener, bool restrictMessagesToSubTree = true );
	virtual void	ReleaseModalSubTree();
	virtual VPANEL	GetModalSubTree();

	// These toggle whether the modal subtree is exclusively receiving messages or conversely whether it's being excluded from receiving messages
	virtual void	SetModalSubTreeReceiveMessages( bool state );
	virtual bool	ShouldModalSubTreeReceiveMessages() const;

	// Gets the time the last mouse click or kb key press occured
	virtual double GetLastInputEventTime();

	virtual void RegisterMouseClickListener( VPANEL listener );
	virtual void UnregisterMouseClickListener( VPANEL oldListener );
	virtual VPANEL	GetMouseFocus();

	virtual void	SetModalSubTreeShowMouse( bool state );
	virtual bool	ShouldModalSubTreeShowMouse() const;

    virtual void UpdateJoystickXPosInternal ( int pos );
    virtual void UpdateJoystickYPosInternal ( int pos );

    virtual int GetJoystickXPos( ) { return m_JoystickX; }
    virtual int GetJoystickYPos( ) { return m_JoysitckY; }

    virtual bool InternalJoystickMoved( int axis, int value );

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, char *pchName );
#endif
private:

	void UpdateLastInputEventTime();
	VPanel			*GetMouseFocusIgnoringModalSubtree();

	void InternalSetCompositionString( const wchar_t *compstr );
	void InternalShowCandidateWindow();
	void InternalHideCandidateWindow();
	void InternalUpdateCandidateWindow();

	bool PostKeyMessage(KeyValues *message);
	void SurfaceSetCursorPos( int x, int y );
	void SurfaceGetCursorPos( int &x, int &y );

	void DestroyCandidateList();
	void CreateNewCandidateList();

	VPanel *CalculateNewKeyFocus();

	void UpdateToggleButtonState();

	void PostModalSubTreeMessage( VPanel *subTree, bool state );
	// returns true if the specified panel is a child of the current modal panel
	// if no modal panel is set, then this always returns TRUE
	bool IsChildOfModalSubTree(VPANEL panel);

	struct InputContext_t
	{
		VPANEL _rootPanel;

		bool _mousePressed[MOUSE_LAST];
		bool _mouseDoublePressed[MOUSE_LAST];
		bool _mouseDown[MOUSE_LAST];
		bool _mouseReleased[MOUSE_LAST];
		bool _keyPressed[KEY_LAST];
		bool _keyTyped[KEY_LAST];
		bool _keyDown[KEY_LAST];
		bool _keyReleased[KEY_LAST];

		VPanel *_keyFocus;
		VPanel *_oldMouseFocus;
		VPanel *_mouseFocus;   // the panel that has the current mouse focus - same as _mouseOver unless _mouseCapture is set
		VPanel *_mouseOver;	 // the panel that the mouse is currently over, NULL if not over any vgui item

		VPanel *_mouseCapture; // the panel that has currently captured mouse focus
		MouseCode m_MouseCaptureStartCode; // The Mouse button which was pressed to initiate mouse capture
		VPanel *_appModalPanel; // the modal dialog panel.
		VPanel *m_pMousePressedPanel;	// the panel that was last pressed by a button

		int m_nCursorX;
		int m_nCursorY;
		
		int m_nLastPostedCursorX;
		int m_nLastPostedCursorY;

		int m_nExternallySetCursorX;
		int m_nExternallySetCursorY;
		bool m_bSetCursorExplicitly;

		CUtlVector< VPanel * >	m_KeyCodeUnhandledListeners;
		CUtlVector< VPanel * >  m_MouseClickListeners;

		VPanel	*m_pModalSubTree;
		VPanel	*m_pUnhandledMouseClickListener;
		bool	m_bRestrictMessagesToModalSubTree;
		bool	m_bKeyFocusAcceptsEnterKey;
		bool	m_bModalSubTreeShowMouse;

		CUtlVector<VPanel *> m_VecPotentialDefaultButtons;
		VPanel  *m_pDefaultButton;

	};
#ifdef DBGFLAG_VALIDATE
	void ValidateInputContext( InputContext_t &inputContext, CValidator &validator, char *pchName );
#endif
	void InitInputContext( InputContext_t *pContext );
	InputContext_t *GetInputContext( HInputContext context );
	void PanelDeleted(VPANEL focus, InputContext_t &context);

	HCursor _cursorOverride;
	bool _updateToggleButtonState;

	char *_keyTrans[KEY_LAST];

	InputContext_t m_DefaultInputContext; 
	HInputContext m_hContext; // current input context

	CUtlLinkedList< InputContext_t, HInputContext > m_Contexts;
	int		m_nDebugMessages;

    int m_JoystickX;
    int m_JoysitckY;

    double m_dblLastInputEventTime;
};

CInputOSX g_Input;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CInputOSX, IInput, VGUI_INPUT_INTERFACE_VERSION, g_Input); // export IInput to everyone else, not IInputInternal!
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CInputOSX, IInputInternal, VGUI_INPUTINTERNAL_INTERFACE_VERSION, g_Input); // for use in external surfaces only! (like the engine surface)

namespace vgui
{
vgui::IInputInternal *g_pInput = &g_Input;
}


CInputOSX::CInputOSX()
{
	m_dblLastInputEventTime = 0;
	m_nDebugMessages = -1;
	InitInputContext( &m_DefaultInputContext );
	m_hContext = DEFAULT_INPUT_CONTEXT;
}

CInputOSX::~CInputOSX()
{
	DestroyCandidateList();
}

//-----------------------------------------------------------------------------
// Resets an input context 
//-----------------------------------------------------------------------------
void CInputOSX::InitInputContext( InputContext_t *pContext )
{
	pContext->_rootPanel = NULL;
	pContext->_keyFocus = NULL;
	pContext->_oldMouseFocus = NULL;
	pContext->_mouseFocus = NULL;
	pContext->_mouseOver = NULL;
	pContext->_mouseCapture = NULL;
	pContext->_appModalPanel = NULL;
	pContext->m_pMousePressedPanel = NULL;
	pContext->m_bKeyFocusAcceptsEnterKey = false;

	pContext->m_nCursorX = pContext->m_nCursorY = 0;

	// zero mouse and keys
	memset(pContext->_mousePressed, 0, sizeof(pContext->_mousePressed));
	memset(pContext->_mouseDoublePressed, 0, sizeof(pContext->_mouseDoublePressed));
	memset(pContext->_mouseDown, 0, sizeof(pContext->_mouseDown));
	memset(pContext->_mouseReleased, 0, sizeof(pContext->_mouseReleased));
	memset(pContext->_keyPressed, 0, sizeof(pContext->_keyPressed));
	memset(pContext->_keyTyped, 0, sizeof(pContext->_keyTyped));
	memset(pContext->_keyDown, 0, sizeof(pContext->_keyDown));
	memset(pContext->_keyReleased, 0, sizeof(pContext->_keyReleased));

	pContext->m_MouseCaptureStartCode = (MouseCode)-1;

	pContext->m_KeyCodeUnhandledListeners.RemoveAll();
	pContext->m_MouseClickListeners.RemoveAll();

	pContext->m_pModalSubTree = NULL;
	pContext->m_pUnhandledMouseClickListener = NULL;
	pContext->m_bRestrictMessagesToModalSubTree = false;
	pContext->m_bModalSubTreeShowMouse = false;
}

void CInputOSX::ResetInputContext( HInputContext context )
{
	// FIXME: Needs to release various keys, mouse buttons, etc...?
	// At least needs to cause things to lose focus

	InitInputContext( GetInputContext(context) );
}


//-----------------------------------------------------------------------------
// Creates/ destroys "input" contexts, which contains information
// about which controls have mouse + key focus, for example.
//-----------------------------------------------------------------------------
HInputContext CInputOSX::CreateInputContext()
{
	HInputContext i = m_Contexts.AddToTail();
	InitInputContext( &m_Contexts[i] );
	return i;
}

void CInputOSX::DestroyInputContext( HInputContext context )
{
	Assert( context != DEFAULT_INPUT_CONTEXT );
	if ( m_hContext == context )
	{
		ActivateInputContext( DEFAULT_INPUT_CONTEXT );
	}
	m_Contexts.Remove(context);
}


//-----------------------------------------------------------------------------
// Returns the current input context
//-----------------------------------------------------------------------------
CInputOSX::InputContext_t *CInputOSX::GetInputContext( HInputContext context )
{
	if (context == DEFAULT_INPUT_CONTEXT)
		return &m_DefaultInputContext;
	return &m_Contexts[context];
}


//-----------------------------------------------------------------------------
// Associates a particular panel with an input context
// Associating NULL is valid; it disconnects the panel from the context
//-----------------------------------------------------------------------------
void CInputOSX::AssociatePanelWithInputContext( HInputContext context, VPANEL pRoot )
{
	// Changing the root panel should invalidate keysettings, etc.
	if (GetInputContext(context)->_rootPanel != pRoot)
	{
		ResetInputContext( context );
		GetInputContext(context)->_rootPanel = pRoot;
	}
}


//-----------------------------------------------------------------------------
// Activates a particular input context, use DEFAULT_INPUT_CONTEXT
// to get the one normally used by VGUI
//-----------------------------------------------------------------------------
void CInputOSX::ActivateInputContext( HInputContext context )
{
	Assert( (context == DEFAULT_INPUT_CONTEXT) || m_Contexts.IsValidIndex(context) );
	m_hContext = context;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInputOSX::RunFrame()
{
	if ( m_nDebugMessages == -1 )
	{
		m_nDebugMessages = CommandLine()->FindParm( "-vguifocus" ) ? 1 : 0;
	}

	InputContext_t *pContext = GetInputContext(m_hContext);

	// tick whoever has the focus
	if (pContext->_keyFocus)
	{
		// when modal dialogs are up messages only get sent to the dialogs children.
		if (IsChildOfModalPanel((VPANEL)pContext->_keyFocus))
		{	
			g_pIVgui->PostMessage((VPANEL)pContext->_keyFocus, new KeyValues("KeyFocusTicked"), NULL);
		}
	}

	// tick whoever has the focus
	if (pContext->_mouseFocus)
	{
		// when modal dialogs are up messages only get sent to the dialogs children.
		if (IsChildOfModalPanel((VPANEL)pContext->_mouseFocus))
		{	
			g_pIVgui->PostMessage((VPANEL)pContext->_mouseFocus, new KeyValues("MouseFocusTicked"), NULL);
		}
	}
	// Mouse has wandered "off" the modal panel, just force a regular arrow cursor until it wanders back within the proper bounds
	else if ( pContext->_appModalPanel )
	{
		g_pSurface->SetCursor( vgui::dc_arrow );
	}

	//clear mouse and key states
	int i;
	for (i = 0; i < MOUSE_LAST; i++)
	{
		pContext->_mousePressed[i] = 0;
		pContext->_mouseDoublePressed[i] = 0;
		pContext->_mouseReleased[i] = 0;
	}
	for (i = 0; i < KEY_LAST; i++)
	{
		pContext->_keyPressed[i] = 0;
		pContext->_keyTyped[i] = 0;
		pContext->_keyReleased[i] = 0;
	}

	VPanel *wantedKeyFocus = CalculateNewKeyFocus();

	// make sure old and new focus get painted
	if ( pContext->_keyFocus != wantedKeyFocus )
	{
		if (pContext->_keyFocus != NULL)
		{
			pContext->_keyFocus->Client()->InternalFocusChanged(true);

			// there may be out of order operations here, since we're directly calling SendMessage,
			// but we need to have focus messages happen immediately, since otherwise mouse events
			// happen out of order - more specifically, they happen before the focus changes

			// send a message to the window saying that it's losing focus
			{
				KeyValues *pMessage = new KeyValues( "KillFocus" );
				KeyValues::AutoDelete autodelete_pMessage( pMessage );
				pMessage->SetPtr( "newPanel", wantedKeyFocus );
				pContext->_keyFocus->SendMessage( pMessage, 0 );
			}

			if ( pContext->_keyFocus )
			{
				pContext->_keyFocus->Client()->Repaint();
			}

			// repaint the nearest popup as well, since it will need to redraw after losing focus
			VPanel *dlg = pContext->_keyFocus;
			while (dlg && !dlg->IsPopup())
			{
				dlg = dlg->GetParent();
			}
			if (dlg)
			{
				dlg->Client()->Repaint();
			}
		}
		if (wantedKeyFocus != NULL)
		{
			wantedKeyFocus->Client()->InternalFocusChanged(false);

			// there may be out of order operations here, since we're directly calling SendMessage,
			// but we need to have focus messages happen immediately, since otherwise mouse events
			// happen out of order - more specifically, they happen before the focus changes

			// send a message to the window saying that it's gaining focus
			{
				KeyValues *pMsg = new KeyValues("SetFocus");
				KeyValues::AutoDelete autodelete_pMsg( pMsg );
				wantedKeyFocus->SendMessage( pMsg, 0 );
			}
			wantedKeyFocus->Client()->Repaint();

			// repaint the nearest popup as well, since it will need to redraw after gaining focus
			VPanel *dlg = wantedKeyFocus;
			while (dlg && !dlg->IsPopup())
			{
				dlg = dlg->GetParent();
			}
			if (dlg)
			{
				dlg->Client()->Repaint();
			}
		}

		if ( m_nDebugMessages > 0 )
		{
			Msg( "changing kb focus from %s to %s\n", 
				pContext->_keyFocus ? pContext->_keyFocus->GetName() : "(no name)",
				wantedKeyFocus ? wantedKeyFocus->GetName() : "(no name)" );
		}

		// accept the focus request
		pContext->_keyFocus = wantedKeyFocus;
		if ( pContext->_keyFocus )
		{
			pContext->_keyFocus->MoveToFront();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Calculate the new key focus
//-----------------------------------------------------------------------------
VPanel *CInputOSX::CalculateNewKeyFocus()
{
	InputContext_t *pContext = GetInputContext(m_hContext);

	// get the top-order panel
	VPanel *wantedKeyFocus = NULL;

	VPanel *pRoot = (VPanel *)pContext->_rootPanel;
	VPanel *top = pRoot;
	if ( g_pSurface->GetPopupCount() > 0 )
	{
		// find the highest-level window that is both visible and a popup
		int nIndex = g_pSurface->GetPopupCount();

		while ( nIndex )
		{			
			top = (VPanel *)g_pSurface->GetPopup( --nIndex );

			// traverse the hierarchy and check if the popup really is visible
			if (top &&
				top->IsVisible() && 
				top->IsKeyBoardInputEnabled() && 
				!g_pSurface->IsMinimized((VPANEL)top) &&
				IsChildOfModalSubTree( (VPANEL)top ) &&
				(!pRoot || top->HasParent( pRoot )) )
			{
				bool bIsVisible = true;
				VPanel *p = top->GetParent();
				// drill down the hierarchy checking that everything is visible
				while(p && bIsVisible)
				{
					if( p->IsVisible()==false)
					{
						bIsVisible = false;
						break;
					}
					p=p->GetParent();
				}

				if ( bIsVisible && !g_pSurface->IsMinimized( (VPANEL)top ) )
					break;
			}

			top = pRoot;
		} 
	}

	if (top)
	{
		// ask the top-level panel for what it considers to be the current focus
		wantedKeyFocus = (VPanel *)top->Client()->GetCurrentKeyFocus();
		if (!wantedKeyFocus)
		{
			wantedKeyFocus = top;

		}
	}

	// check to see if any of this surfaces panels have the focus
	if (!g_pSurface->HasFocus())
	{
		wantedKeyFocus=NULL;
	}

	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if (!IsChildOfModalPanel((VPANEL)wantedKeyFocus))
	{	
		wantedKeyFocus=NULL;
	}

	return wantedKeyFocus;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CInputOSX::PanelDeleted(VPANEL vfocus, InputContext_t &context)
{
	VPanel *focus = (VPanel *)vfocus;
	if (context._keyFocus == focus)
	{
		if ( m_nDebugMessages > 0 )
		{
			Msg( "removing kb focus %s\n", 
				context._keyFocus ? context._keyFocus->GetName() : "(no name)" );
		}
		context._keyFocus = NULL;
	}
	if (context._mouseOver == focus)
	{
		/*
		if ( m_nDebugMessages > 0 )
		{
			Msg( "removing kb focus %s\n", 
				context._keyFocus ? pcontext._keyFocus->GetName() : "(no name)" );
		}
		*/
		context._mouseOver = NULL;
	}
	if (context._oldMouseFocus == focus)
	{
		context._oldMouseFocus = NULL;
	}
	if (context._mouseFocus == focus)
	{
		context._mouseFocus = NULL;
	}

	// NOTE: These two will only ever happen for the default context at the moment
	if (context._mouseCapture == focus)
	{
		Assert( &context == &m_DefaultInputContext );
		SetMouseCapture(NULL);
		context._mouseCapture = NULL;
	}
	if (context._appModalPanel == focus)
	{
		Assert( &context == &m_DefaultInputContext );
		ReleaseAppModalSurface();
	}
	if ( context.m_pUnhandledMouseClickListener == focus )
	{
		context.m_pUnhandledMouseClickListener = NULL;
	}
	if ( context.m_pModalSubTree == focus )
	{
		context.m_pModalSubTree = NULL;
		context.m_bRestrictMessagesToModalSubTree = false;
		context.m_bModalSubTreeShowMouse = false;
	}

	context.m_KeyCodeUnhandledListeners.FindAndRemove( focus );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *focus - 
//-----------------------------------------------------------------------------
void CInputOSX::PanelDeleted(VPANEL focus)
{
	HInputContext i;
	for (i = m_Contexts.Head(); i != m_Contexts.InvalidIndex(); i = m_Contexts.Next(i) )
	{
		PanelDeleted( focus, m_Contexts[i] );
	}
	PanelDeleted( focus, m_DefaultInputContext );
}




//-----------------------------------------------------------------------------
// Purpose: Sets the new mouse focus
//			won't override _mouseCapture settings
// Input  : newMouseFocus - 
//-----------------------------------------------------------------------------
void CInputOSX::SetMouseFocus(VPANEL newMouseFocus)
{
	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if (!IsChildOfModalPanel(newMouseFocus))
	{	
		return;	
	}

	bool wantsMouse, isPopup; // =  popup->GetMouseInput();
	VPanel *panel = (VPanel *)newMouseFocus;

	InputContext_t *pContext = GetInputContext( m_hContext );

	
	wantsMouse = false;
	if ( newMouseFocus )
	{
		do 
		{
			wantsMouse = panel->IsMouseInputEnabled();
			isPopup = panel->IsPopup();
			panel = panel->GetParent();
		}
		while ( wantsMouse && !isPopup && panel && panel->GetParent() ); // only consider panels that want mouse input
	}

	// if this panel doesn't want mouse input don't let it get focus
	if (newMouseFocus && !wantsMouse) 
	{
		return;
	}

	if ((VPANEL)pContext->_mouseOver != newMouseFocus || (!pContext->_mouseCapture && (VPANEL)pContext->_mouseFocus != newMouseFocus) )
	{
		pContext->_oldMouseFocus = pContext->_mouseOver;
		pContext->_mouseOver = (VPanel *)newMouseFocus;

		//tell the old panel with the mouseFocus that the cursor exited
		if (pContext->_oldMouseFocus != NULL)
		{
			// only notify of entry if the mouse is not captured or we're a child of the captured panel
			if ( !pContext->_mouseCapture || pContext->_oldMouseFocus->HasParent(pContext->_mouseCapture) )
			{
				g_pIVgui->PostMessage((VPANEL)pContext->_oldMouseFocus, new KeyValues("CursorExited"), NULL);
			}
		}

		//tell the new panel with the mouseFocus that the cursor entered
		if (pContext->_mouseOver != NULL)
		{
			// only notify of entry if the mouse is not captured or we're a child of the captured panel
			if ( !pContext->_mouseCapture || pContext->_mouseOver->HasParent(pContext->_mouseCapture) )
			{
				g_pIVgui->PostMessage((VPANEL)pContext->_mouseOver, new KeyValues("CursorEntered"), NULL);
			}
		}

		// set where the mouse is currently over
		// mouse capture overrides destination
		VPanel *newMouseFocus = pContext->_mouseCapture ? pContext->_mouseCapture : pContext->_mouseOver;

		if ( m_nDebugMessages > 0 )
		{
			Msg( "changing mouse focus from %s to %s\n", 
				pContext->_mouseFocus ? pContext->_mouseFocus->GetName() : "(no name)",
				newMouseFocus ? newMouseFocus->GetName() : "(no name)" );
		}


		pContext->_mouseFocus = newMouseFocus;
	}
}

VPanel *CInputOSX::GetMouseFocusIgnoringModalSubtree()
{
	// find the panel that has the focus
	VPanel *focus = NULL; 

	InputContext_t *pContext = GetInputContext( m_hContext );

	int x, y;
	x = pContext->m_nCursorX;
	y = pContext->m_nCursorY;

	if (!pContext->_rootPanel)
	{
		if (g_pSurface->IsCursorVisible() && g_pSurface->IsWithin(x, y))
		{
			// faster version of code below
			// checks through each popup in order, top to bottom windows
			for (int i = g_pSurface->GetPopupCount() - 1; i >= 0; i--)
			{
				VPanel *popup = (VPanel *)g_pSurface->GetPopup(i);
				VPanel *panel = popup;
				bool wantsMouse = panel->IsMouseInputEnabled();
				bool isVisible = !g_pSurface->IsMinimized((VPANEL)panel);

				while ( isVisible && panel && panel->GetParent() ) // only consider panels that want mouse input
				{
					isVisible = panel->IsVisible();
					panel = panel->GetParent();
				}
				

				if ( wantsMouse && isVisible ) 
				{
					focus = (VPanel *)popup->Client()->IsWithinTraverse(x, y, false);
					if (focus)
						break;
				}
			}
			if (!focus)
			{
				focus = (VPanel *)((VPanel *)g_pSurface->GetEmbeddedPanel())->Client()->IsWithinTraverse(x, y, false);
			}
		}
	}
	else
	{
		focus = (VPanel *)((VPanel *)(pContext->_rootPanel))->Client()->IsWithinTraverse(x, y, false);
	}


	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if ( !IsChildOfModalPanel((VPANEL)focus, false ))
	{	
		// should this be _appModalPanel?
		focus = NULL;
	}

	return focus;
}



//-----------------------------------------------------------------------------
// Purpose: Calculates which panel the cursor is currently over and sets it up
//			as the current mouse focus.
//-----------------------------------------------------------------------------
void CInputOSX::UpdateMouseFocus(int x, int y)
{
	// find the panel that has the focus
	VPanel *focus = NULL; 
	
	InputContext_t *pContext = GetInputContext( m_hContext );
	
	if ( m_hContext != DEFAULT_VGUI_CONTEXT )
	{
		// faster version of code below
		// checks through each popup in order, top to bottom windows
		VPanel *panel = (VPanel *) pContext->_rootPanel;
		
#if defined( _DEBUG )
		char const *pchName = panel->GetName();
		NOTE_UNUSED( pchName );
#endif
		bool wantsMouse = panel->IsMouseInputEnabled();
		bool isVisible = panel->IsVisible();
		
		if ( wantsMouse && isVisible ) 
		{
			focus = (VPanel *)panel->Client()->IsWithinTraverse(x, y, false);
		}
	}
	else if ( g_pSurface->IsCursorVisible() && g_pSurface->IsWithin( x, y ) )
	{
		// faster version of code below
		// checks through each popup in order, top to bottom windows
		int c = g_pSurface->GetPopupCount();
		for (int i = c - 1; i >= 0; i--)
		{
			VPanel *popup = (VPanel *)g_pSurface->GetPopup(i);
			VPanel *panel = popup;
			
			if ( pContext->_rootPanel && !popup->HasParent((VPanel*)pContext->_rootPanel) )
			{
				// if we have a root panel, only consider popups that belong to it
				continue;
			}
#if defined( _DEBUG )
			char const *pchName = popup->GetName();
			NOTE_UNUSED( pchName );
#endif
			bool wantsMouse = panel->IsMouseInputEnabled() && IsChildOfModalSubTree( (VPANEL)panel );
			if ( !wantsMouse )
				continue;
			
			bool isVisible = !g_pSurface->IsMinimized((VPANEL)panel);
			if ( !isVisible )
				continue;
			
			while ( isVisible && panel && panel->GetParent() ) // only consider panels that want mouse input
			{
				isVisible = panel->IsVisible();
				panel = panel->GetParent();
			}
			
			
			if ( !wantsMouse || !isVisible ) 
				continue;
			
			focus = (VPanel *)popup->Client()->IsWithinTraverse(x, y, false);
			if (focus)
				break;
		}
		if (!focus)
		{
			focus = (VPanel *)((VPanel *)g_pSurface->GetEmbeddedPanel())->Client()->IsWithinTraverse(x, y, false);
		}
	}
	
	// mouse focus debugging code
	/*
	 static VPanel *oldFocus = (VPanel *)0x0001;
	 if (oldFocus != focus)
	 {
	 oldFocus = focus;
	 if (focus)
	 {
	 g_pIVgui->DPrintf2("mouse over: (%s, %s)\n", focus->GetName(), focus->GetClassName());
	 }
	 else
	 {
	 g_pIVgui->DPrintf2("mouse over: (NULL)\n");
	 }
	 }
	 */
	
	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if (!IsChildOfModalPanel((VPANEL)focus))
	{	
		// should this be _appModalPanel?
		focus = NULL;
	}
	
	SetMouseFocus((VPANEL)focus);
}

// Passes in a keycode which allows hitting other mouse buttons w/o cancelling capture mode
void CInputOSX::SetMouseCaptureEx(VPANEL panel, MouseCode captureStartMouseCode )
{
	// This sets m_MouseCaptureStartCode to -1, so we set the real value afterward
	SetMouseCapture( panel );

	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if (!IsChildOfModalPanel(panel))
	{	
		return;	
	}

	InputContext_t *pContext = GetInputContext( m_hContext );
	Assert( pContext );
	pContext->m_MouseCaptureStartCode = captureStartMouseCode;
}

//-----------------------------------------------------------------------------
// Purpose: Sets or releases the mouse capture
// Input  : panel - pointer to the panel to get mouse capture
//			a NULL panel means that you want to clear the mouseCapture
//			MouseCaptureLost is sent to the panel that loses the mouse capture
//-----------------------------------------------------------------------------
void CInputOSX::SetMouseCapture(VPANEL panel)
{
	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if (!IsChildOfModalPanel(panel))
	{	
		return;	
	}

	InputContext_t *pContext = GetInputContext( m_hContext );
	Assert( pContext );

	pContext->m_MouseCaptureStartCode = (MouseCode)-1;

	// send a message if the panel is losing mouse capture
	if (pContext->_mouseCapture && panel != (VPANEL)pContext->_mouseCapture)
	{
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseCapture, new KeyValues("MouseCaptureLost"), NULL);
	}

	if (panel == 0)
	{
		if (pContext->_mouseCapture != NULL)
		{
			g_pSurface->EnableMouseCapture((VPANEL)pContext->_mouseCapture, false);
		}
	}
	else
	{
		g_pSurface->EnableMouseCapture(panel, true);
	}

	pContext->_mouseCapture = (VPanel *)panel;
}

VPANEL CInputOSX::GetMouseCapture()
{
	return (VPANEL)( m_DefaultInputContext._mouseCapture );
}

// returns true if the specified panel is a child of the current modal panel
// if no modal panel is set, then this always returns TRUE
bool CInputOSX::IsChildOfModalSubTree(VPANEL panel)
{
	if ( !panel )
		return true;

	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext->m_pModalSubTree )
	{
		// If panel is child of modal subtree, the allow messages to route to it if restrict messages is set
		bool isChildOfModal = ((VPanel *)panel)->HasParent(pContext->m_pModalSubTree );
		if ( isChildOfModal )
		{
			return pContext->m_bRestrictMessagesToModalSubTree;
		}
		// If panel is not a child of modal subtree, then only allow messages if we're not restricting them to the modal subtree
		else
		{
			return !pContext->m_bRestrictMessagesToModalSubTree;
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: check if we are in modal state, 
// and if we are make sure this panel has the modal panel as a parent
//-----------------------------------------------------------------------------
bool CInputOSX::IsChildOfModalPanel(VPANEL panel, bool checkModalSubTree /*= true*/ )
{
	// NULL is ok.
	if (!panel)
		return true;

	InputContext_t *pContext = GetInputContext( m_hContext );

	// if we are in modal state, make sure this panel is a child of us.
	if (pContext->_appModalPanel)
	{	
		if (!((VPanel *)panel)->HasParent(pContext->_appModalPanel))
		{
			return false;
		}
	}

	if ( !checkModalSubTree )
		return true;

	// Defer to modal subtree logic instead...
	return IsChildOfModalSubTree( panel );
}


//-----------------------------------------------------------------------------
// Purpose: returns which panel currently has key focus
//-----------------------------------------------------------------------------
VPANEL CInputOSX::GetFocus()
{
	return (VPANEL)( GetInputContext( m_hContext )->_keyFocus );
}

VPANEL CInputOSX::GetCalculatedFocus()
{
	return (VPANEL) CalculateNewKeyFocus();
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the current focus can be a 
//-----------------------------------------------------------------------------
VPANEL CInputOSX::GetCurrentDefaultButton()
{
	InputContext_t *pInputContext = GetInputContext( m_hContext );
	return (VPANEL)pInputContext->m_pDefaultButton;
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the current focus can be a 
//-----------------------------------------------------------------------------
void CInputOSX::RegisterPotentialDefaultButton( VPANEL panel )
{
	InputContext_t *pInputContext = GetInputContext( m_hContext );
	pInputContext->m_VecPotentialDefaultButtons.AddToTail( (VPanel *)panel );
}


//-----------------------------------------------------------------------------
// Purpose: returns true if the current focus can be a 
//-----------------------------------------------------------------------------
void CInputOSX::UnregisterPotentialDefaultButton( VPANEL panel )
{
	InputContext_t *pInputContext = GetInputContext( m_hContext );
	pInputContext->m_VecPotentialDefaultButtons.FindAndRemove( (VPanel *)panel );
}


//-----------------------------------------------------------------------------
// Purpose: returns the topmost panel the mouse cursor is currently in the bounds of
//-----------------------------------------------------------------------------
VPANEL CInputOSX::GetMouseOver()
{
	return (VPANEL)( GetInputContext( m_hContext )->_mouseOver );
}

VPANEL CInputOSX::GetMouseFocus()
{
	return (VPANEL)( GetInputContext( m_hContext )->_mouseFocus );
}


//-----------------------------------------------------------------------------
// Purpose: returns the panel that a mouse button was last pressed on
//-----------------------------------------------------------------------------
VPANEL CInputOSX::GetLastMousePressedPanel()
{
	return (VPANEL)( GetInputContext( m_hContext )->m_pMousePressedPanel );
}


bool CInputOSX::WasMousePressed(MouseCode code)
{
	return GetInputContext( m_hContext )->_mousePressed[code];
}

bool CInputOSX::WasMouseDoublePressed(MouseCode code)
{
	return GetInputContext( m_hContext )->_mouseDoublePressed[code];
}

bool CInputOSX::IsMouseDown(MouseCode code)
{
	return GetInputContext( m_hContext )->_mouseDown[code];
}

bool CInputOSX::WasMouseReleased(MouseCode code)
{
	return GetInputContext( m_hContext )->_mouseReleased[code];
}

bool CInputOSX::WasKeyPressed(KeyCode code)
{
	return GetInputContext( m_hContext )->_keyPressed[code];
}

bool CInputOSX::IsKeyDown(KeyCode code)
{
	return GetInputContext( m_hContext )->_keyDown[code];
}

bool CInputOSX::WasKeyTyped(KeyCode code)
{
	return GetInputContext( m_hContext )->_keyTyped[code];
}

bool CInputOSX::WasKeyReleased(KeyCode code)
{
	// changed from: only return true if the key was released and the passed in panel matches the keyFocus
	return GetInputContext( m_hContext )->_keyReleased[code];
}

// Returns the last mouse cursor position that we processed
void CInputOSX::GetCursorPosition( int &x, int &y )
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	x = pContext->m_nCursorX;
	y = pContext->m_nCursorY;
}

//-----------------------------------------------------------------------------
// Purpose: Converts a key code into a full key name
//-----------------------------------------------------------------------------
void CInputOSX::GetKeyCodeText(KeyCode code, char *buf, int buflen)
{
	if (!buf)
		return;

	// copy text into buf up to buflen in length
	// skip 2 in _keyTrans because the first two are for GetKeyCodeChar
	for (int i = 0; i < buflen; i++)
	{
		char ch = _keyTrans[code][i+2];
		buf[i] = ch;
		if (ch == 0)
			break;
	}

}

void CInputOSX::SetCursorPos(int x, int y)
{
	if ( IsDispatchingMessageQueue() )
	{
		InputContext_t *pContext = GetInputContext( m_hContext );
		pContext->m_nExternallySetCursorX = x;
		pContext->m_nExternallySetCursorY = y;
		pContext->m_bSetCursorExplicitly = true;
	}
	else
	{
		SurfaceSetCursorPos( x, y );
	}
}

void CInputOSX::GetCursorPos(int &x, int &y)
{
	if ( IsDispatchingMessageQueue() )
	{
		GetCursorPosition( x, y );
	}
	else
	{
		SurfaceGetCursorPos( x, y );
	}	
}

void CInputOSX::UpdateToggleButtonState()
{
	// FIXME: Only do this for the primary context
	// We can't make this work with the VCR stuff...
	if (m_hContext != DEFAULT_INPUT_CONTEXT)
		return;

	// only update toggle button state once per frame
	if (_updateToggleButtonState)
	{
		_updateToggleButtonState = false;
	}
	else
	{
		return;
	}

	// check shift, alt, ctrl keys
	struct key_t
	{
		KeyCode code;
		int winCode;

		KeyCode ignoreIf;
	};
/*
	static key_t keys[] =
	{
		{ KEY_CAPSLOCKTOGGLE, VK_CAPITAL },
		{ KEY_NUMLOCKTOGGLE, VK_NUMLOCK },
		{ KEY_SCROLLLOCKTOGGLE, VK_SCROLL },
		{ KEY_LSHIFT, VK_LSHIFT },
		{ KEY_RSHIFT, VK_RSHIFT },
		{ KEY_LCONTROL, VK_LCONTROL },
		{ KEY_RCONTROL, VK_RCONTROL },
		{ KEY_LALT, VK_LMENU },
		{ KEY_RALT, VK_RMENU },
		{ KEY_RALT, VK_MENU, KEY_LALT },
		{ KEY_RSHIFT, VK_SHIFT, KEY_LSHIFT },
		{ KEY_RCONTROL, VK_CONTROL, KEY_LCONTROL },
	};

	for (int i = 0; i < (sizeof(keys) / sizeof(keys[0])); i++)
	{
		bool vState = IsKeyDown(keys[i].code);
		SHORT winState = System_GetKeyState(keys[i].winCode);

		if (i < 3)
		{
			// toggle keys
			if (LOBYTE(winState) != (BYTE)vState)
			{
				if (LOBYTE(winState))
				{
					InternalKeyCodePressed(keys[i].code);
				}
				else
				{
					InternalKeyCodeReleased(keys[i].code);
				}
			}
		}
		else
		{
			// press keys
			if (keys[i].ignoreIf && IsKeyDown(keys[i].ignoreIf))
				continue;

			if (HIBYTE(winState) != (BYTE)vState)
			{
				if (HIBYTE(winState))
				{
					InternalKeyCodePressed(keys[i].code);
				}
				else
				{
					InternalKeyCodeReleased(keys[i].code);
				}
			}
		}
	}
*/
	
}

void CInputOSX::SetCursorOveride(HCursor cursor)
{
	_cursorOverride = cursor;
}

HCursor CInputOSX::GetCursorOveride()
{
	return _cursorOverride;
}



bool CInputOSX::InternalCursorMoved(int x, int y)
{
	g_pIVgui->PostMessage((VPANEL)-1, new KeyValues("SetCursorPosInternal", "xpos", x, "ypos", y), NULL);

	// @wge Changed from true to false to match implementation in InputWin32 (allows scaleform to intercept mouse input)
	// This allows the new GameUI to receive these messages also
	return false;
 }

bool CInputOSX::InternalMousePressed(MouseCode code)
{
	// True means we've processed the message and other code shouldn't see this message
	bool bFilter = false;
	
	InputContext_t *pContext = GetInputContext( m_hContext );
	VPanel *pTargetPanel = pContext->_mouseOver;
	if ( pContext->_mouseCapture && IsChildOfModalPanel((VPANEL)pContext->_mouseCapture))
	{
		// The faked mouse wheel button messages are specifically ignored by vgui
		if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
			return true;
		
		bFilter = true;
		
		bool captureLost = code == pContext->m_MouseCaptureStartCode || pContext->m_MouseCaptureStartCode == (MouseCode)-1;
		
		// the panel with mouse capture gets all messages
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseCapture, new KeyValues("MousePressed", "code", code), NULL);
		pTargetPanel = pContext->_mouseCapture;
		
		if ( captureLost )
		{
			// this has to happen after MousePressed so the panel doesn't Think it got a mouse press after it lost capture
			SetMouseCapture(NULL);
		}
	}
	else if ( (pContext->_mouseFocus != NULL) && IsChildOfModalPanel((VPANEL)pContext->_mouseFocus) )
	{
		// The faked mouse wheel button messages are specifically ignored by vgui
		if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
			return true;
		
		bFilter = true;
		
		// tell the panel with the mouseFocus that the mouse was presssed
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseFocus, new KeyValues("MousePressed", "code", code), NULL);
		//		g_pIVgui->DPrintf2("MousePressed: (%s, %s)\n", _mouseFocus->GetName(), _mouseFocus->GetClassName());
		pTargetPanel = pContext->_mouseFocus;
	}
	else if ( pContext->m_pModalSubTree && pContext->m_pUnhandledMouseClickListener )
	{
		VPanel *p = GetMouseFocusIgnoringModalSubtree();
		if ( p )
		{
			bool isChildOfModal = IsChildOfModalSubTree( (VPANEL)p );
			bool isUnRestricted = !pContext->m_bRestrictMessagesToModalSubTree;
			
			if ( isUnRestricted != isChildOfModal )
			{
				// The faked mouse wheel button messages are specifically ignored by vgui
				if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
					return true;
				
				g_pIVgui->PostMessage( ( VPANEL )pContext->m_pUnhandledMouseClickListener, new KeyValues( "UnhandledMouseClick", "code", code ), NULL );
				pTargetPanel = pContext->m_pUnhandledMouseClickListener;
				bFilter = true;
			}
		}
	}
	
	
	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if ( IsChildOfModalPanel( (VPANEL)pTargetPanel ) )
	{	
		g_pSurface->SetTopLevelFocus( (VPANEL)pTargetPanel );
	}
	
	return bFilter;	
}

bool CInputOSX::InternalMouseDoublePressed(MouseCode code)
{
	// True means we've processed the message and other code shouldn't see this message
	bool bFilter = false;
	
	InputContext_t *pContext = GetInputContext( m_hContext );
	VPanel *pTargetPanel = pContext->_mouseOver;
	if ( pContext->_mouseCapture && IsChildOfModalPanel((VPANEL)pContext->_mouseCapture))
	{
		// The faked mouse wheel button messages are specifically ignored by vgui
		if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
			return true;
		
		// the panel with mouse capture gets all messages
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseCapture, new KeyValues("MouseDoublePressed", "code", code), NULL);
		pTargetPanel = pContext->_mouseCapture;
		bFilter = true;
	}
	else if ( (pContext->_mouseFocus != NULL) && IsChildOfModalPanel((VPANEL)pContext->_mouseFocus))
	{			
		// The faked mouse wheel button messages are specifically ignored by vgui
		if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
			return true;
		
		// tell the panel with the mouseFocus that the mouse was double presssed
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseFocus, new KeyValues("MouseDoublePressed", "code", code), NULL);
		pTargetPanel = pContext->_mouseFocus;
		
		// Check for input passthrough
		bFilter = true;
		KeyValues *pRequest = new KeyValues( "InputControlState", "event", "mousepressed" );
		KeyValues::AutoDelete autodelete_pRequest( pRequest );
		if ( pContext->_mouseFocus->Client()->RequestInfo( pRequest ) )
		{
			bFilter = !pRequest->GetBool( "passthrough" );
		}
	}
	
	// check if we are in modal state, 
	// and if we are make sure this panel is a child of us.
	if (IsChildOfModalPanel((VPANEL)pTargetPanel))
	{	
		g_pSurface->SetTopLevelFocus((VPANEL)pTargetPanel);
	}
	
	return bFilter;
}

bool CInputOSX::InternalMouseReleased(MouseCode code)
{
	// True means we've processed the message and other code shouldn't see this message
	bool bFilter = false;
	
	InputContext_t *pContext = GetInputContext( m_hContext );
	if (pContext->_mouseCapture && IsChildOfModalPanel((VPANEL)pContext->_mouseCapture))
	{
		// The faked mouse wheel button messages are specifically ignored by vgui
		if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
			return true;
		
		// the panel with mouse capture gets all messages
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseCapture, new KeyValues("MouseReleased", "code", code), NULL );
		bFilter = true;
	}
	else if ((pContext->_mouseFocus != NULL) && IsChildOfModalPanel((VPANEL)pContext->_mouseFocus))
	{
		// The faked mouse wheel button messages are specifically ignored by vgui
		if ( code == MOUSE_WHEEL_DOWN || code == MOUSE_WHEEL_UP )
			return true;
		
		//tell the panel with the mouseFocus that the mouse was release
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseFocus, new KeyValues("MouseReleased", "code", code), NULL );
		
		// Check for input passthrough
		bFilter = true;
		KeyValues *pRequest = new KeyValues( "InputControlState", "event", "mousepressed" );
		KeyValues::AutoDelete autodelete_pRequest( pRequest );
		if ( pContext->_mouseFocus->Client()->RequestInfo( pRequest ) )
		{
			bFilter = !pRequest->GetBool( "passthrough" );
		}
	}
	
	return bFilter;
}

bool CInputOSX::InternalMouseWheeled(int delta)
{
	// True means we've processed the message and other code shouldn't see this message
	bool bFilter = false;
	
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ((pContext->_mouseFocus != NULL) && IsChildOfModalPanel((VPANEL)pContext->_mouseFocus))
	{
		// the mouseWheel works with the mouseFocus, not the keyFocus
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseFocus, new KeyValues("MouseWheeled", "delta", delta), NULL);
		
		// Check for input passthrough
		bFilter = true;
		KeyValues *pRequest = new KeyValues( "InputControlState", "event", "mousepressed" );
		KeyValues::AutoDelete autodelete_pRequest( pRequest );
		if ( pContext->_mouseFocus->Client()->RequestInfo( pRequest ) )
		{
			bFilter = !pRequest->GetBool( "passthrough" );
		}
	}
	return bFilter;
}

bool CInputOSX::InternalKeyCodePressed(KeyCode code)
{
	// mask out bogus keys
	if ( !IsKeyCode( code ) && !IsJoystickCode( code ) )
		return false;
		
	bool bFilter = PostKeyMessage( new KeyValues("KeyCodePressed", "code", code ) );
	return bFilter;
}

void CInputOSX::InternalKeyCodeTyped(KeyCode code)
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	// mask out bogus keys
	if (code < 0 || code >= KEY_LAST)
		return;

	UpdateLastInputEventTime();

	// set key state
	pContext->_keyTyped[code]=1;

	// tell the current focused panel that a key was typed
	PostKeyMessage(new KeyValues("KeyCodeTyped", "code", code));

}

void CInputOSX::InternalKeyTyped(wchar_t unichar)
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	// set key state
	if( unichar <= KEY_LAST )
	{
		pContext->_keyTyped[unichar]=1;
	}

	UpdateLastInputEventTime();

	// tell the current focused panel that a key was typed
	PostKeyMessage(new KeyValues("KeyTyped", "unichar", unichar));

}

bool CInputOSX::InternalKeyCodeReleased(KeyCode code)
{	
	// mask out bogus keys
	if ( !IsKeyCode( code ) && !IsJoystickCode( code ) )
		return false;
	
	return PostKeyMessage(new KeyValues("KeyCodeReleased", "code", code));	
}

//-----------------------------------------------------------------------------
// Purpose: posts a message to the key focus if it's valid
//-----------------------------------------------------------------------------
bool CInputOSX::PostKeyMessage(KeyValues *message)
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if( (pContext->_keyFocus!= NULL) && IsChildOfModalPanel((VPANEL)pContext->_keyFocus))
	{
		//tell the current focused panel that a key was released
		g_pIVgui->PostMessage((VPANEL)pContext->_keyFocus, message, NULL );
		return true;
	}

	message->deleteThis();
	return false;
}

VPANEL CInputOSX::GetAppModalSurface()
{
	return (VPANEL)m_DefaultInputContext._appModalPanel;
}

void CInputOSX::SetAppModalSurface(VPANEL panel)
{
	m_DefaultInputContext._appModalPanel = (VPanel *)panel;
}


void CInputOSX::ReleaseAppModalSurface()
{
	m_DefaultInputContext._appModalPanel = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *hwnd - 
//-----------------------------------------------------------------------------
void CInputOSX::SetIMEWindow( void *hwnd )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void *CInputOSX::GetIMEWindow()
{
	Assert( false );
	return NULL;
}


// Change keyboard layout type
void CInputOSX::OnChangeIME( bool forward )
{
	Assert( false );
}

int CInputOSX::GetCurrentIMEHandle()
{
	Assert( false );
	return 0;
}

int CInputOSX::GetEnglishIMEHandle()
{
	/*if (!CommandLine()->FindParm("-hushasserts"))
	{
		DevMsg( "CInputOSX::GetEnglishIMEHandle impl" );
	}*/

	return 0;
}

void CInputOSX::OnChangeIMEByHandle( int handleValue )
{
	Assert( false );
}

	// Returns the Language Bar label (Chinese, Korean, Japanese, Russion, Thai, etc.)
void CInputOSX::GetIMELanguageName( wchar_t *buf, int unicodeBufferSizeInBytes )
{
	Assert( false );
	buf[0] = L'\0';
}
	// Returns the short code for the language (EN, CH, KO, JP, RU, TH, etc. ).
void CInputOSX::GetIMELanguageShortCode( wchar_t *buf, int unicodeBufferSizeInBytes )
{
	//Assert( false );
	buf[0] = L'\0';
}

// Call with NULL dest to get item count
int CInputOSX::GetIMELanguageList( LanguageItem *dest, int destcount )
{
	Assert( false );
	return 0;
}

int CInputOSX::GetIMEConversionModes( ConversionModeItem *dest, int destcount )
{
	Assert( false );
	return 0;
}

int CInputOSX::GetIMESentenceModes( SentenceModeItem *dest, int destcount )
{
	Assert( false );
	return 0;
}

void CInputOSX::OnChangeIMEConversionModeByHandle( int handleValue )
{
	Assert( false );

}

void CInputOSX::OnChangeIMESentenceModeByHandle( int handleValue )
{
}

void CInputOSX::OnInputLanguageChanged()
{
}

void CInputOSX::OnIMEStartComposition()
{
}

void CInputOSX::OnIMEComposition( int flags )
{
	/*
	Msg( "OnIMEComposition\n" );

	IMEDesc( VGUI_GCS_COMPREADSTR );
	IMEDesc( VGUI_GCS_COMPREADATTR );
	IMEDesc( VGUI_GCS_COMPREADCLAUSE );
	IMEDesc( VGUI_GCS_COMPSTR );
	IMEDesc( VGUI_GCS_COMPATTR );
	IMEDesc( VGUI_GCS_COMPCLAUSE );
	IMEDesc( VGUI_GCS_CURSORPOS );
	IMEDesc( VGUI_GCS_DELTASTART );
	IMEDesc( VGUI_GCS_RESULTREADSTR );
	IMEDesc( VGUI_GCS_RESULTREADCLAUSE );
	IMEDesc( VGUI_GCS_RESULTSTR );
	IMEDesc( VGUI_GCS_RESULTCLAUSE );
	IMEDesc( VGUI_CS_INSERTCHAR );
	IMEDesc( VGUI_CS_NOMOVECARET );
	*/

	Assert( false );
}

void CInputOSX::OnIMEEndComposition()
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext )
	{
		// tell the current focused panel that a key was typed
		PostKeyMessage( new KeyValues( "DoCompositionString", "string", L"" ) );
	}
}

void CInputOSX::DestroyCandidateList()
{
	//DevMsg( "Impl CInputOSX::DestroyCandidateList" );
}

void CInputOSX::OnIMEShowCandidates() 
{
	Assert( false );
}

void CInputOSX::OnIMECloseCandidates() 
{
	Assert( false );

}

void CInputOSX::OnIMEChangeCandidates() 
{
	Assert( false );
}

void CInputOSX::CreateNewCandidateList()
{
	Assert( false );
}

int  CInputOSX::GetCandidateListCount()
{
	Assert( false );
	return 0;
}

void CInputOSX::GetCandidate( int num, wchar_t *dest, int destSizeBytes )
{
	Assert( false );
	dest[ 0 ] = L'\0';
}

int  CInputOSX::GetCandidateListSelectedItem()
{
	Assert( false );
	return 0;
}

int  CInputOSX::GetCandidateListPageSize()
{
	Assert( false );
	return 0;
}

int  CInputOSX::GetCandidateListPageStart()
{
	Assert( false );
	return 0;
}

void CInputOSX::SetCandidateListPageStart( int start )
{
	Assert( false );
}

void CInputOSX::OnIMERecomputeModes()
{
	/*
	Msg( "OnIMERecomputeModes\n" );

	HIMC hImc = ImmGetContext( ( HWND )GetIMEWindow() );
	if ( hImc )
	{
		DWORD	dwConvMode, dwSentMode;

		ImmGetConversionStatus( hImc, &dwConvMode, &dwSentMode );

		// Describe them
		int flags = dwConvMode;

		IMEDesc( IME_CMODE_ALPHANUMERIC );
		IMEDesc( IME_CMODE_NATIVE );
		IMEDesc( IME_CMODE_KATAKANA );
		IMEDesc( IME_CMODE_LANGUAGE );
		IMEDesc( IME_CMODE_FULLSHAPE );
		IMEDesc( IME_CMODE_ROMAN );
		IMEDesc( IME_CMODE_CHARCODE );
		IMEDesc( IME_CMODE_HANJACONVERT );
		IMEDesc( IME_CMODE_SOFTKBD );
		IMEDesc( IME_CMODE_NOCONVERSION );
		IMEDesc( IME_CMODE_EUDC );
		IMEDesc( IME_CMODE_SYMBOL );
		IMEDesc( IME_CMODE_FIXED );

		flags = dwSentMode;

		IMEDesc( IME_SMODE_NONE );
		IMEDesc( IME_SMODE_PLAURALCLAUSE );
		IMEDesc( IME_SMODE_SINGLECONVERT );
		IMEDesc( IME_SMODE_AUTOMATIC );
		IMEDesc( IME_SMODE_PHRASEPREDICT );
		IMEDesc( IME_SMODE_CONVERSATION );

		ImmReleaseContext( ( HWND )GetIMEWindow(), hImc );
	}
	*/
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CInputOSX::CandidateListStartsAtOne()
{
	Assert( false );
	return false;
}

void CInputOSX::SetCandidateWindowPos( int x, int y ) 
{
	Assert( false );
}

void CInputOSX::InternalSetCompositionString( const wchar_t *compstr )
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext )
	{
		// tell the current focused panel that a key was typed
		PostKeyMessage( new KeyValues( "DoCompositionString", "string", compstr ) );
	}
}

void CInputOSX::InternalShowCandidateWindow()
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext )
	{
		PostKeyMessage( new KeyValues( "DoShowIMECandidates" ) );
	}
}

void CInputOSX::InternalHideCandidateWindow()
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext )
	{
		PostKeyMessage( new KeyValues( "DoHideIMECandidates" ) );
	}
}

void CInputOSX::InternalUpdateCandidateWindow()
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext )
	{
		PostKeyMessage( new KeyValues( "DoUpdateIMECandidates" ) );
	}
}

bool CInputOSX::GetShouldInvertCompositionString()
{
	AssertOnce( !"CInputOSX::GetShouldInvertCompositionString impl" );
	return false;
}

void CInputOSX::RegisterKeyCodeUnhandledListener( VPANEL panel )
{
	if ( !panel )
		return;

	InputContext_t *pContext = GetInputContext(m_hContext);
	if ( !pContext )
		return;

	VPanel *listener = (VPanel *)panel;

	if ( pContext->m_KeyCodeUnhandledListeners.Find( listener ) == pContext->m_KeyCodeUnhandledListeners.InvalidIndex() )
	{
		pContext->m_KeyCodeUnhandledListeners.AddToTail( listener );
	}
}

void CInputOSX::UnregisterKeyCodeUnhandledListener( VPANEL panel )
{
	if ( !panel )
		return;

	InputContext_t *pContext = GetInputContext(m_hContext);
	if ( !pContext )
		return;

	VPanel *listener = (VPanel *)panel;

	pContext->m_KeyCodeUnhandledListeners.FindAndRemove( listener );
}


// Posts unhandled message to all interested panels
void CInputOSX::OnKeyCodeUnhandled( int keyCode )
{
	InputContext_t *pContext = GetInputContext(m_hContext);
	if ( !pContext )
		return;

	int c = pContext->m_KeyCodeUnhandledListeners.Count();
	for ( int i = 0; i < c; ++i )
	{
		VPanel *listener = pContext->m_KeyCodeUnhandledListeners[ i ];
		g_pIVgui->PostMessage((VPANEL)listener, new KeyValues( "KeyCodeUnhandled", "code", keyCode ), NULL );
	}
}

void CInputOSX::PostModalSubTreeMessage( VPanel *subTree, bool state )
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if( pContext->m_pModalSubTree == NULL )
		return;

	//tell the current focused panel that a key was released
	KeyValues *kv = new KeyValues( "ModalSubTree", "state", state ? 1 : 0 );
	g_pIVgui->PostMessage( (VPANEL)pContext->m_pModalSubTree, kv, NULL );
}

// Assumes subTree is a child panel of the root panel for the vgui contect
//  if restrictMessagesToSubTree is true, then mouse and kb messages are only routed to the subTree and it's children and mouse/kb focus
//   can only be on one of the subTree children, if a mouse click occurs outside of the subtree, and "UnhandledMouseClick" message is sent to unhandledMouseClickListener panel
//   if it's set
//  if restrictMessagesToSubTree is false, then mouse and kb messages are routed as normal except that they are not routed down into the subtree
//   however, if a mouse click occurs outside of the subtree, and "UnhandleMouseClick" message is sent to unhandledMouseClickListener panel
//   if it's set
void CInputOSX::SetModalSubTree( VPANEL subTree, VPANEL unhandledMouseClickListener, bool restrictMessagesToSubTree /*= true*/ )
{
	InputContext_t *pContext = GetInputContext(m_hContext);
	if ( !pContext )
		return;

	if ( pContext->m_pModalSubTree && 
		pContext->m_pModalSubTree != (VPanel *)subTree )
	{
		ReleaseModalSubTree();
	}

	if ( !subTree )
		return;

	pContext->m_pModalSubTree = (VPanel *)subTree;
	pContext->m_pUnhandledMouseClickListener = (VPanel *)unhandledMouseClickListener;
	pContext->m_bRestrictMessagesToModalSubTree = restrictMessagesToSubTree;
	pContext->m_bModalSubTreeShowMouse = false;

	PostModalSubTreeMessage( pContext->m_pModalSubTree, true );
}

void CInputOSX::ReleaseModalSubTree()
{
	InputContext_t *pContext = GetInputContext(m_hContext);
	if ( !pContext )
		return;

	if ( pContext->m_pModalSubTree )
	{
		PostModalSubTreeMessage( pContext->m_pModalSubTree, false );
	}

	pContext->m_pModalSubTree = NULL;
	pContext->m_pUnhandledMouseClickListener = NULL;
	pContext->m_bRestrictMessagesToModalSubTree = false;
	pContext->m_bModalSubTreeShowMouse = false;
}

VPANEL CInputOSX::GetModalSubTree()
{
	InputContext_t *pContext = GetInputContext(m_hContext);
	if ( !pContext )
		return 0;

	return (VPANEL)pContext->m_pModalSubTree;
}

// These toggle whether the modal subtree is exclusively receiving messages or conversely whether it's being excluded from receiving messages
void CInputOSX::SetModalSubTreeReceiveMessages( bool state )
{
	InputContext_t *pContext = GetInputContext(m_hContext);
	if ( !pContext )
		return;

	Assert( pContext->m_pModalSubTree );
	if ( !pContext->m_pModalSubTree )
		return;

	pContext->m_bRestrictMessagesToModalSubTree = state;
	
}

bool CInputOSX::ShouldModalSubTreeReceiveMessages() const
{
	InputContext_t *pContext = const_cast< CInputOSX * >( this )->GetInputContext(m_hContext);
	if ( !pContext )
		return true;

	return pContext->m_bRestrictMessagesToModalSubTree;
}

void CInputOSX::SetModalSubTreeShowMouse( bool bState )
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( !pContext )
		return;

	Assert( pContext->m_pModalSubTree );
	if ( !pContext->m_pModalSubTree )
		return;

	pContext->m_bModalSubTreeShowMouse = bState;

}

bool CInputOSX::ShouldModalSubTreeShowMouse() const
{
	InputContext_t *pContext = const_cast< CInputOSX * >( this )->GetInputContext( m_hContext );
	if ( !pContext )
		return true;

	return pContext->m_bModalSubTreeShowMouse;
}

void CInputOSX::UpdateLastInputEventTime()
{
	m_dblLastInputEventTime = g_pSystem->GetFrameTime();
}

double CInputOSX::GetLastInputEventTime()
{
	return m_dblLastInputEventTime;
}

void CInputOSX::RegisterMouseClickListener( VPANEL listener )
{
	InputContext_t *pContext = GetInputContext(m_hContext);
	if ( !pContext )
		return;

	pContext->m_MouseClickListeners.AddToTail( (VPanel *)listener );
}

void CInputOSX::UnregisterMouseClickListener( VPANEL oldListener )
{
	InputContext_t *pContext = GetInputContext(m_hContext);
	if ( !pContext )
		return;

	pContext->m_MouseClickListeners.FindAndRemove( (VPanel *)oldListener );
}

//-----------------------------------------------------------------------------
// Updates the internal key/mouse state associated with the current input context without sending messages
//-----------------------------------------------------------------------------
void CInputOSX::SetMouseCodeState( MouseCode code, MouseCodeState_t state )
{
	if ( !IsMouseCode( code ) )
		return;
	
	InputContext_t *pContext = GetInputContext( m_hContext );
	switch( state )
	{
		case BUTTON_RELEASED:
			pContext->_mouseReleased[ code ] = 1;
			break;
			
		case BUTTON_PRESSED:
			pContext->_mousePressed[ code ] = 1;
			break;
			
		case BUTTON_DOUBLECLICKED:
			pContext->_mouseDoublePressed[ code ] = 1;
			break;
	}
	
	pContext->_mouseDown[ code ] = ( state != BUTTON_RELEASED );
}

void CInputOSX::SetKeyCodeState( KeyCode code, bool bPressed )
{
	if ( !IsKeyCode( code ) /* && !IsJoystickCode( code ) */ )
		return;
	
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( bPressed )
	{
		//set key state
		pContext->_keyPressed[ code ] = 1;
	}
	else
	{
		// set key state
		pContext->_keyReleased[ code ] = 1;
	}
	pContext->_keyDown[ code ] = bPressed;
}

void CInputOSX::UpdateButtonState( const InputEvent_t &event )
{
	switch( event.m_nType )
	{
		case IE_ButtonPressed:
		case IE_ButtonReleased:
		case IE_ButtonDoubleClicked:
		{
			// NOTE: data2 is the virtual key code (data1 contains the scan-code one)
			ButtonCode_t code = (ButtonCode_t)event.m_nData2;
			
			// FIXME: Workaround hack
			if ( IsKeyCode( code ) || IsJoystickCode( code ) )
			{
				SetKeyCodeState( code, ( event.m_nType != IE_ButtonReleased ) );
				break;
			}
			
			if ( IsMouseCode( code ) )
			{
				MouseCodeState_t state;
				state = ( event.m_nType == IE_ButtonReleased ) ? vgui::BUTTON_RELEASED : vgui::BUTTON_PRESSED;
				if ( event.m_nType == IE_ButtonDoubleClicked )
				{
					state = vgui::BUTTON_DOUBLECLICKED;
				}
				
				SetMouseCodeState( code, state );
				break;
			}
		}
			break;
	}
}

//-----------------------------------------------------------------------------
// Makes sure the windows cursor is in the right place after processing input 
//-----------------------------------------------------------------------------
void CInputOSX::HandleExplicitSetCursor( )
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	
	if ( pContext->m_bSetCursorExplicitly )
	{
		pContext->m_nCursorX = pContext->m_nExternallySetCursorX;
		pContext->m_nCursorY = pContext->m_nExternallySetCursorY;
		pContext->m_bSetCursorExplicitly = false;
		
		// NOTE: This forces a cursor moved message to be posted next time
		pContext->m_nLastPostedCursorX = pContext->m_nLastPostedCursorY = -9999;
		
		SurfaceSetCursorPos( pContext->m_nCursorX, pContext->m_nCursorY );
		UpdateMouseFocus( pContext->m_nCursorX, pContext->m_nCursorY ); 
	}
}


//-----------------------------------------------------------------------------
// Called when we've detected cursor has moved via a windows message
//-----------------------------------------------------------------------------
void CInputOSX::PostCursorMessage( )
{
	InputContext_t *pContext = GetInputContext( m_hContext );
	
	if ( pContext->m_bSetCursorExplicitly )
	{
		// NOTE m_bSetCursorExplicitly will be reset to false in HandleExplicitSetCursor
		pContext->m_nCursorX = pContext->m_nExternallySetCursorX;
		pContext->m_nCursorY = pContext->m_nExternallySetCursorY;
	}
	
	if ( pContext->m_nLastPostedCursorX == pContext->m_nCursorX && pContext->m_nLastPostedCursorY == pContext->m_nCursorY )
		return;
	
	pContext->m_nLastPostedCursorX = pContext->m_nCursorX;
	pContext->m_nLastPostedCursorY = pContext->m_nCursorY;
	
	if ( pContext->_mouseCapture )
	{
		if (!IsChildOfModalPanel((VPANEL)pContext->_mouseCapture))
			return;	
		
		// the panel with mouse capture gets all messages
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseCapture, new KeyValues("CursorMoved", "xpos", pContext->m_nCursorX, "ypos", pContext->m_nCursorY), NULL);
	}
	else if (pContext->_mouseFocus != NULL)
	{
		// mouse focus is current from UpdateMouse focus
		// so the appmodal check has already been made.
		g_pIVgui->PostMessage((VPANEL)pContext->_mouseFocus, new KeyValues("CursorMoved", "xpos", pContext->m_nCursorX, "ypos", pContext->m_nCursorY), NULL);
	}
}

//-----------------------------------------------------------------------------
// Cursor position; this is the current position read from the input queue.
// We need to set it because client code may read this during Mouse Pressed
// events, etc.
//-----------------------------------------------------------------------------
void CInputOSX::UpdateCursorPosInternal( int x, int y )
{
	// Windows sends a CursorMoved message even when you haven't actually
	// moved the cursor, this means we are going into this fxn just by clicking
	// in the window. We only want to execute this code if we have actually moved
	// the cursor while dragging. So this code has been added to check
	// if we have actually moved from our previous position.
	InputContext_t *pContext = GetInputContext( m_hContext );
	if ( pContext->m_nCursorX == x && pContext->m_nCursorY == y )
		return;
	
	pContext->m_nCursorX = x;
	pContext->m_nCursorY = y;
	
	// Cursor has moved, so make sure the mouseFocus is current
	UpdateMouseFocus( x, y );
}

//=============================================================================
// Handle gamepad joystick movement.
//=============================================================================

void CInputOSX::UpdateJoystickXPosInternal( int pos )
{
    m_JoystickX = pos;
}

void CInputOSX::UpdateJoystickYPosInternal( int pos )
{
    m_JoysitckY = pos;
}

bool CInputOSX::InternalJoystickMoved( int axis, int value )
{
    if ( axis == 0 )
        g_pIVgui->PostMessage( (VPANEL)-1, new KeyValues( "SetJoystickXPosInternal", "pos", value ), NULL );
    else if ( axis == 1 )
        g_pIVgui->PostMessage( (VPANEL)-1, new KeyValues( "SetJoystickYPosInternal", "pos", value ), NULL );

    InputContext_t *pContext = GetInputContext( m_hContext );
    if ( ( pContext->_keyFocus != NULL ) && IsChildOfModalPanel( (VPANEL)pContext->_keyFocus) )
    {
        const char* axis_message_map[4] = { "Stick1XChanged", "Stick1YChanged", "Stick2XChanged", "Stick2YChanged" };
        g_pIVgui->PostMessage( (VPANEL)pContext->_keyFocus, new KeyValues( axis_message_map[axis], "pos", value ), NULL );
    }

    return true;
}

//-----------------------------------------------------------------------------
// Low-level cursor getting/setting functions 
//-----------------------------------------------------------------------------
void CInputOSX::SurfaceSetCursorPos(int x, int y)
{
	if ( g_pSurface->HasCursorPosFunctions() ) // does the surface export cursor functions for us to use?
	{
		g_pSurface->SurfaceSetCursorPos(x,y);
	}
	else
	{
		int px, py, pw, pt;
		g_pSurface->GetAbsoluteWindowBounds(px, py, pw, pt);
		x += px;
		y += py;
		Assert( false );
	}
}

void CInputOSX::SurfaceGetCursorPos( int &x, int &y )
{
#ifndef _GAMECONSOLE // X360TBD
	if ( g_pSurface->HasCursorPosFunctions() ) // does the surface export cursor functions for us to use?
	{
		g_pSurface->SurfaceGetCursorPos( x,y );
	}
	else
	{
		CGEventRef event = CGEventCreate( NULL );
		CGPoint pnt = CGEventGetLocation( event );
		
		x = pnt.x;
		y = pnt.y;
		
		// translate into coordinates relative to surface
		int px, py, pw, pt;
		g_pSurface->GetAbsoluteWindowBounds(px, py, pw, pt);
		x -= px;
		y -= py;
	}
#else
	x = 0;
	y = 0;
#endif
}



#ifdef DBGFLAG_VALIDATE
//-----------------------------------------------------------------------------
// Purpose: Ensure that all of our internal structures are consistent, and
//			account for all memory that we've allocated.
// Input:	validator -		Our global validator object
//			pchName -		Our name (typically a member var in our container)
//-----------------------------------------------------------------------------
void CInputOSX::Validate( CValidator &validator, char *pchName )
{
	validator.Push( "CInputOSX", this, pchName );

	ValidateObj( m_Contexts );
	ValidateInputContext( m_DefaultInputContext, validator, "m_DefaultInputContext" );
	for ( int i = 0; i < m_Contexts.Count(); i++ )
	{
		ValidateInputContext( m_Contexts[i], validator, "m_Contexts[i]" );
	}

	validator.Pop();
}

//-----------------------------------------------------------------------------
// Purpose: validates a single input context
//-----------------------------------------------------------------------------
void CInputOSX::ValidateInputContext( InputContext_t &inputContext, CValidator &validator, char *pchName )
{
	validator.Push( "CInputOSX::InputContext_t", this, pchName );

	ValidateObj( inputContext.m_KeyCodeUnhandledListeners );
	ValidateObj( inputContext.m_MouseClickListeners );
	ValidateObj( inputContext.m_VecPotentialDefaultButtons );

	validator.Pop();
}

//-----------------------------------------------------------------------------
// Purpose: passthrough to validating our input object
//-----------------------------------------------------------------------------
void Validate_Input( CValidator &validator )
{
	ValidateObj( g_Input );
}
#endif // DBGFLAG_VALIDATE
