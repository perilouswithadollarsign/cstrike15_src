//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef INPUTGAMEUI_H
#define INPUTGAMEUI_H
#ifdef _WIN32
#pragma once
#endif


#if !defined( _GAMECONSOLE )
#include <windows.h>
#include <imm.h>
#endif

#include "vgui/iinput.h"
#include "hitarea.h"
#include "keyrepeat.h"

#include "utlvector.h"
#include "utllinkedlist.h"
#include "keyvalues.h"
#include "inputsystem/buttoncode.h"
#include "vgui/cursor.h"
#include "vstdlib/ieventsystem.h"


enum GameUIMouseCodeState_t
{
	BUTTON_RELEASED = 0,
	BUTTON_PRESSED,
	BUTTON_DOUBLECLICKED,
};

namespace vgui
{
	typedef unsigned int HInputContext;

#define DEFAULT_INPUT_CONTEXT ((vgui::HInputContext)~0)
}


using namespace vgui;


class CInputGameUI
{
public:
	CInputGameUI();
	~CInputGameUI();


	struct LanguageItem
	{
		wchar_t		shortname[ 4 ];
		wchar_t		menuname[ 128 ];
		int			handleValue;
		bool		active; // true if this is the active language
	};

	struct ConversionModeItem
	{
		wchar_t		menuname[ 128 ];
		int			handleValue;
		bool		active; // true if this is the active conversion mode
	};

	struct SentenceModeItem
	{
		wchar_t		menuname[ 128 ];
		int			handleValue;
		bool		active; // true if this is the active sentence mode
	};

	void Init();

	void RunFrame();
	void ProcessEvents();

	void Shutdown();

	void SetWindowSize( int width, int height );

	void PanelDeleted( CHitArea *panel );
	void GraphicHidden( CHitArea *focus );
	void ForceInputFocusUpdate();

	void UpdateMouseFocus(int x, int y);
	void SetMouseFocus( CHitArea *newMouseFocus );

	// Temp testing until event system can handle destinations.
	void OnCursorEnter( CHitArea* const & pTarget );
	void OnCursorExit( CHitArea * const & pTarget );
	void OnCursorMove( CHitArea * const & pTarget, const int &x, const int &y );
	
	void OnMouseDown( CHitArea * const & pTarget, const ButtonCode_t &code );
	void OnMouseUp( CHitArea * const & pTarget, CHitArea * const & pTrap, const ButtonCode_t &code );
	void OnMouseDoubleClick( CHitArea * const & pTarget, const ButtonCode_t &code );
	void OnMouseWheel( CHitArea * const & pTarget, const int &delta );

	void OnKeyDown( CHitArea * const & pTarget, const ButtonCode_t &code );
	void OnKeyUp( CHitArea * const & pTarget, const ButtonCode_t &code );
	void OnKeyCodeTyped( CHitArea * const & pTarget, const ButtonCode_t &code );
	void OnKeyTyped( CHitArea * const & pTarget, const wchar_t &unichar );
	
	void OnLoseKeyFocus( CHitArea * const & pTarget );
	void OnGainKeyFocus( CHitArea * const & pTarget );

	void SetCursorPos( int x, int y );
	void UpdateCursorPosInternal( const int &x, const int &y );
	void GetCursorPos( int &x, int &y );
	void SetCursorOveride( vgui::HCursor cursor );
	vgui::HCursor GetCursorOveride();

	void SetKeyFocus( CHitArea *pFocus ); // note this will not post any messages.
	CHitArea *GetKeyFocus();
	CHitArea *GetCalculatedKeyFocus();
	CHitArea *GetMouseOver();

	bool WasMousePressed( ButtonCode_t code );
	bool WasMouseDoublePressed( ButtonCode_t code );
	bool IsMouseDown( ButtonCode_t code );
	bool WasMouseReleased( ButtonCode_t code );
	bool WasKeyPressed( ButtonCode_t code );
	bool IsKeyDown( ButtonCode_t code );
	bool WasKeyTyped( ButtonCode_t code );
	bool WasKeyReleased( ButtonCode_t code );

	void GetKeyCodeText( ButtonCode_t code, char *buf, int buflen );

	bool InternalCursorMoved( int x,int y ); //expects input in surface space
	bool InternalMousePressed( ButtonCode_t code );
	bool InternalMouseDoublePressed( ButtonCode_t code );
	bool InternalMouseReleased( ButtonCode_t code );
	bool InternalMouseWheeled( int delta );
	bool InternalKeyCodePressed( ButtonCode_t code );
	void InternalKeyCodeTyped( ButtonCode_t code );
	void InternalKeyTyped( wchar_t unichar );
	bool InternalKeyCodeReleased( ButtonCode_t code );
	void SetKeyCodeState( ButtonCode_t code, bool bPressed );
	void SetMouseCodeState( ButtonCode_t code, GameUIMouseCodeState_t state );
	void UpdateButtonState( const InputEvent_t &event );


	// Creates/ destroys "input" contexts, which contains information
	// about which controls have mouse + key focus, for example.
	virtual vgui::HInputContext CreateInputContext();
	virtual void DestroyInputContext( vgui::HInputContext context ); 

	// Activates a particular input context, use DEFAULT_INPUT_CONTEXT
	// to get the one normally used by VGUI
	virtual void ActivateInputContext( vgui::HInputContext context );
	virtual void PostCursorMessage();
	virtual void HandleExplicitSetCursor();

	virtual void ResetInputContext( vgui::HInputContext context );

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
	virtual void SetMouseCaptureEx( CHitArea *panel, ButtonCode_t captureStartMouseCode );

	// Passes in a keycode which allows hitting other mouse buttons w/o cancelling capture mode
	virtual void SetMouseCapture( CHitArea *panel );
	virtual CHitArea *GetMouseCapture();

	virtual CHitArea *GetMouseFocus();
private:

	void InternalSetCompositionString( const wchar_t *compstr );
	void InternalShowCandidateWindow();
	void InternalHideCandidateWindow();
	void InternalUpdateCandidateWindow();

	bool PostKeyMessage( KeyValues *message );

	void DestroyCandidateList();
	void CreateNewCandidateList();

	CHitArea *CalculateNewKeyFocus();

	void SurfaceSetCursorPos( int x, int y );
	void SurfaceGetCursorPos( int &x, int &y );

	struct InputContext_t
	{
		bool _mousePressed[MOUSE_COUNT];
		bool _mouseDoublePressed[MOUSE_COUNT];
		bool _mouseDown[MOUSE_COUNT];
		bool _mouseReleased[MOUSE_COUNT];
		bool _keyPressed[BUTTON_CODE_COUNT];
		bool _keyTyped[BUTTON_CODE_COUNT];
		bool _keyDown[BUTTON_CODE_COUNT];
		bool _keyReleased[BUTTON_CODE_COUNT];

		CHitArea *_keyFocus;
		bool _bKeyTrap; // true if the graphic with keyfocus recieved a down event. Send the up if it got a down.
		CHitArea *_oldMouseFocus;
		CHitArea *_mouseFocus;   // the panel that has the current mouse focus - same as _mouseOver unless _mouseCapture is set
		CHitArea *_mouseOver;	 // the panel that the mouse is currently over, NULL if not over any vgui item

		CHitArea *_mouseCapture; // the panel that has currently captured mouse focus
		ButtonCode_t m_MouseCaptureStartCode; // The Mouse button which was pressed to initiate mouse capture

		CHitArea *_mouseLeftTrap; // the panel that should receive the next mouse left up
		CHitArea *_mouseMiddleTrap; // the panel that should receive the next mouse middle up
		CHitArea *_mouseRightTrap; // the panel that should receive the next mouse right up

		int m_nCursorX;
		int m_nCursorY;

		int m_nLastPostedCursorX;
		int m_nLastPostedCursorY;

		int m_nExternallySetCursorX;
		int m_nExternallySetCursorY;
		bool m_bSetCursorExplicitly;

		CUtlVector< CHitArea * > m_KeyCodeUnhandledListeners;

		CHitArea *m_pUnhandledMouseClickListener;
		bool	m_bRestrictMessagesToModalSubTree;

		CKeyRepeatHandler m_keyRepeater;
	};

	void InitInputContext( InputContext_t *pContext );
	InputContext_t *GetInputContext( vgui::HInputContext context );
	void PanelDeleted( CHitArea *focus, InputContext_t &context);
	void GraphicHidden( CHitArea *focus, InputContext_t &context );
	vgui::HCursor _cursorOverride;

	char *_keyTrans[KEY_LAST];

	InputContext_t m_DefaultInputContext; 
	vgui::HInputContext m_hContext; // current input context

	CUtlLinkedList< InputContext_t, vgui::HInputContext > m_Contexts;

#ifndef _GAMECONSOLE	
	void			*_imeWnd;
	CANDIDATELIST	*_imeCandidates;
#endif

	int		m_nDebugMessages;

	EventQueue_t m_hEventChannel;

	int m_nWindowWidth, m_nWindowHeight;
};


extern CInputGameUI *g_pInputGameUI;

bool InputGameUIHandleInputEvent( const InputEvent_t &event );


DEFINE_EVENT1_WITHNAMES( CursorEnterEvent, CHitArea *, pTarget );
DEFINE_EVENT1_WITHNAMES( CursorExitEvent, CHitArea *, pTarget );
DEFINE_EVENT3_WITHNAMES( CursorMoveEvent, CHitArea *, pTarget, int, x, int, y );
DEFINE_EVENT2_WITHNAMES( InternalCursorMoveEvent, int, x, int, y );

DEFINE_EVENT2_WITHNAMES( MouseDownEvent, CHitArea *, pTarget, ButtonCode_t, code );
DEFINE_EVENT3_WITHNAMES( MouseUpEvent, CHitArea *, pTarget, CHitArea *, pTrap, ButtonCode_t, code );
DEFINE_EVENT2_WITHNAMES( MouseDoubleClickEvent, CHitArea *, pTarget, ButtonCode_t, code );
DEFINE_EVENT2_WITHNAMES( MouseWheelEvent, CHitArea *, pTarget, int, delta  );

DEFINE_EVENT2_WITHNAMES( KeyDownEvent, CHitArea *, pTarget, ButtonCode_t, code  );
DEFINE_EVENT2_WITHNAMES( KeyUpEvent, CHitArea *, pTarget, ButtonCode_t, code  );
DEFINE_EVENT2_WITHNAMES( KeyCodeTypedEvent, CHitArea *, pTarget, ButtonCode_t, code  );
DEFINE_EVENT2_WITHNAMES( KeyTypedEvent, CHitArea *, pTarget, wchar_t, unichar  );

DEFINE_EVENT1_WITHNAMES( GainKeyFocusEvent, CHitArea *, pTarget );
DEFINE_EVENT1_WITHNAMES( LoseKeyFocusEvent, CHitArea *, pTarget );


#endif // INPUTGAMEUI_H
