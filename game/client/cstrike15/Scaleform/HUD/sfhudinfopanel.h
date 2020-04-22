//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:  Displays HUD elements for medals/achievements, and hint text
//
//=====================================================================================//
#ifndef SFHUDINFOPANEL_H_
#define SFHUDINFOPANEL_H_

#include "hud.h"
#include "hud_element_helper.h"
#include "scaleformui/scaleformui.h"
#include "sfhudflashinterface.h"
#include "cs_gamerules.h"
#include "c_cs_player.h"
#include "tier1/utlqueue.h"

class SFHudInfoPanel : public SFHudFlashInterface
{
	enum HUDINFO_TYPE
	{
		SFHUDINFO_All,
		SFHUDINFO_Help,
		SFHUDINFO_Defuse,
		SFHUDINFO_Medal,
		SFHUDINFO_PriorityMessage
	};

public:
	explicit SFHudInfoPanel( const char *value );
	virtual ~SFHudInfoPanel();

	// These overload the CHudElement class
	virtual void	ProcessInput( void );
	virtual void	LevelInit( void );
	virtual void	LevelShutdown( void );
	virtual bool 	ShouldDraw( void );
	virtual void	SetActive( bool bActive );

	virtual void	Reset( void );

	// these overload the ScaleformFlashInterfaceMixin class
	virtual void FlashReady( void );
	virtual bool PreUnloadFlash( void );

	// receivers for hint messages
	bool MsgFunc_HintText( const CCSUsrMsg_HintText &msg );
	bool MsgFunc_KeyHintText( const CCSUsrMsg_KeyHintText &msg );

	virtual void FireGameEvent( IGameEvent * event );

	// Priority text replaces what used to be called CenterPrint text in VGui
	void	SetPriorityText( char *pMsg );
	void	SetPriorityText( wchar_t *pMsg );
	void	SetPriorityHintText( wchar_t *pMsg );

	// Offsets the Y location of the notification panels
	void	ApplyYOffset( int nOffset );
	bool	IsVisible( void ) { return m_bIsVisible; }

	CUserMessageBinder m_UMCMsgHintText;
	CUserMessageBinder m_UMCMsgKeyHintText;
	CUserMessageBinder m_UMCMsgQuestProgress;

protected:
	void	ModifyPriorityTextWindow( bool bMsgSet );

	void	ShowPanel( HUDINFO_TYPE panelType, bool value );

	// Only call this function if you're already inside of a slot-lock block!
	void	ShowPanelNoLock( HUDINFO_TYPE panelType, bool value );

	void	HideAll( void );
	void	LockSlot( bool wantItLocked, bool& currentlyLocked );
	
	bool	SetHintText( wchar_t *text );

	void	LocalizeAndDisplay( const char *pszHudTxtMsg, const char *szRawString );

protected:
	SFVALUE m_HelpPanelHandle;
	SFVALUE m_HelpBodyTextHandle;

	CountdownTimer	m_HintDisplayTimer;

	SFVALUE m_DefusePanelHandle;
	SFVALUE m_DefuseTitleTextHandle;
	SFVALUE m_DefuseBodyTextHandle;
	SFVALUE m_DefuseTimerTextHandle;

	SFVALUE m_DefuseIconKit;
	SFVALUE m_DefuseIconNoKit;

	float	m_PreviousDefusePercent;

	SFVALUE m_MedalPanelHandle;
	SFVALUE m_MedalTitleTextHandle;
	SFVALUE m_MedalBodyTextHandle;

	struct AchivementQueueInfo
	{
		eCSAchievementType			type;
		int							playerSlot;
	};

	CUtlQueue<AchivementQueueInfo>  m_achievementQueue;
	eCSAchievementType				m_activeAchievement;

	CountdownTimer					m_AchievementDisplayTimer;

	CountdownTimer					m_PriorityMsgDisplayTimer;

	SFVALUE m_PriorityMessagePanelHandle;
	SFVALUE m_PriorityMessageTitleTextHandle;
	SFVALUE m_PriorityMessageBodyTextHandle;

	bool	m_bDeferRaiseHelpPanel;
	bool	m_bDeferRaisePriorityMessagePanel;
	bool	m_bHintPanelHidden;					// True when we have hidden a help panel in order to show a priority message
	bool	m_bIsVisible;
};


#endif /* SFHUDINFOPANEL_H_ */
