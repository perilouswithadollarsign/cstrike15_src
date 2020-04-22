#ifndef SFHUDCHAT_H
#define SFHUDCHAT_H

#include "hud.h"
#include "hud_element_helper.h"
#include "scaleformui/scaleformui.h"
#include "sfhudflashinterface.h"
#include "cs_gamerules.h"
#include "c_cs_player.h"
#include "weapon_csbase.h"
#include "takedamageinfo.h"
#include "weapon_csbase.h"
#include "ammodef.h"
#include "iclientmode.h"


class SFHudChat : public SFHudFlashInterface
{
public:
	explicit SFHudChat( const char *value );
	
	virtual ~SFHudChat();
	
	//void ProcessInput( void );
	
	void LevelInit( void );
	
	virtual void LevelShutdown( void );
	
	virtual void SetActive( bool bActive );
	virtual bool ShouldDraw( void );

	// these overload the ScaleformFlashInterfaceMixin class
	virtual void FlashReady( void );

	void StartMessageMode( int mode );

	void OnOK( SCALEFORM_CALLBACK_ARGS_DECL );
	void OnCancel( SCALEFORM_CALLBACK_ARGS_DECL );

	bool ChatRaised( void );
	void AddStringToHistory( const wchar_t *string );

	void ClearHistory();

	// overloads for the CGameEventListener class
	virtual void FireGameEvent( IGameEvent *event );

protected:

	void ShowPanel( bool bShow, bool force );

	void UpdateHistory( void );

protected:
	bool		m_bVisible;
	int			m_iMode;
	float		m_fLastShowTime;

	wchar_t*	m_pHistoryString;
};
#endif
