//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VGENERICWAITSCREEN_H__
#define __VGENERICWAITSCREEN_H__

#include "basemodui.h"
#include <utlvector.h>
#include <utlstring.h>
#include "vfooterpanel.h"

namespace BaseModUI {

class IWaitscreenCallbackInterface
{
public:
	// Allows a callback interface get a first crack at keycode
	// return true to allow processing by waitscreen
	// return false to swallow the keycode
	virtual bool OnKeyCodePressed( vgui::KeyCode code ) { return true; }

	virtual void OnThink() {}
};

class GenericWaitScreen : public CBaseModFrame, public IBaseModFrameListener
{
	DECLARE_CLASS_SIMPLE( GenericWaitScreen, CBaseModFrame );

public:
	GenericWaitScreen( vgui::Panel *parent, const char *panelName );
	~GenericWaitScreen();

	void			SetMaxDisplayTime( float flMaxTime, void (*pfn)() = NULL );
	void			AddMessageText( const char* message, float minDisplayTime );
	void			AddMessageText( const wchar_t* message, float minDisplayTime );
	void			SetCloseCallback( vgui::Panel * panel, const char * message );
	void			ClearData();
#if defined( PORTAL2_PUZZLEMAKER )
	void			SetTargetFileHandle( UGCHandle_t hFileHandle, float flCustomProgress = -1.f );
#endif //

protected:
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	PaintBackground();
	virtual void	OnThink();
	virtual void	RunFrame();
	virtual void	OnKeyCodePressed( vgui::KeyCode code );
	virtual void	SetDataSettings( KeyValues *pSettings );

	virtual void	ClockAnim( void );

	void			SetUpText();
	void			CheckIfNeedsToClose();
	void			UpdateFooter();
	void			SetMessageText( const char* message );
	void			SetMessageText( const wchar_t *message );
	void			FixLayout();

	float			m_LastEngineTime;
	int				m_CurrentSpinnerValue;

	vgui::Panel		*m_callbackPanel;
	CUtlString		m_callbackMessage;

	CUtlString		m_currentDisplayText;
	bool			m_bTextSet;

	float			m_MsgStartDisplayTime;
	float			m_MsgMinDisplayTime;
	float			m_MsgMaxDisplayTime;
	void			(*m_pfnMaxTimeOut)();
	bool			m_bClose;

	bool			m_bValid;
	bool			m_bNeedsLayoutFixed;

	vgui::ImagePanel	*m_pWorkingAnim;
	vgui::Label			*m_pLblMessage;

	vgui::HFont		m_hMessageFont;

	int				m_nTextOffsetX;

	struct WaitMessage
	{
		WaitMessage() : minDisplayTime( 0 ), mWchMsgText( NULL ) {}
		CUtlString	mDisplayString;
		float		minDisplayTime;
		wchar_t const *mWchMsgText;
	};
	CUtlVector< WaitMessage > m_MsgVector;

	IMatchAsyncOperation *m_pAsyncOperationAbortable;
	IWaitscreenCallbackInterface *m_pWaitscreenCallbackInterface;

#if defined( PORTAL2_PUZZLEMAKER )
	UGCHandle_t			m_hTargetFileHandle;
	float				m_flCustomProgress;
#endif // PORTAL2_PUZZLEMAKER
};

}

#endif // __VSEARCHINGFORLIVEGAMES_H__