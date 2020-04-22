//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//
#ifndef SFHUD_DEATHNOTICE_H_
#define SFHUD_DEATHNOTICE_H_

#include "scaleformui/scaleformui.h"
#include "sfhudflashinterface.h"
#include "strtools.h"

#define DEATH_NOTICE_TEXT_MAX					512 // max number of characters in a notice text
#if defined( _GAMECONSOLE )
#define DEATH_NOTICE_NAME_TRUNCATE_AT			16  // number of name character displayed before truncation
#define DEATH_NOTICE_ASSIST_NAME_TRUNCATE_AT	11  // number of name character displayed before truncation
#define DEATH_NOTICE_ASSIST_SHORT_NAME_TRUNCATE_AT	8  // number of name character displayed before truncation
#else
#define DEATH_NOTICE_NAME_TRUNCATE_AT			22  // number of name character displayed before truncation
#define DEATH_NOTICE_ASSIST_NAME_TRUNCATE_AT	18  // number of name character displayed before truncation
#define DEATH_NOTICE_ASSIST_SHORT_NAME_TRUNCATE_AT	12  // number of name character displayed before truncation
#endif

//c9c9c9

#if defined(_PS3) || defined(POSIX)
#define DEATH_NOTICE_IMG_STRING		L" <img src='icon-%ls.png' height='16'/>"
#define DEATH_NOTICE_FONT_STRING	L"<font color=\"%ls\">%ls</font>"
#define DEATH_NOTICE_ATTACKER_PLUS_ASSISTER	L"%ls <font color='#bababa'>+</font> %ls"
#define DEATH_NOTICE_ATTACKER_NO_ASSIST	L"%ls%ls"
#else
#define DEATH_NOTICE_IMG_STRING		L" <img src='icon-%s.png' height='16'/>"
#define DEATH_NOTICE_FONT_STRING	L"<font color=\"%s\">%s</font>"
#define DEATH_NOTICE_ATTACKER_PLUS_ASSISTER	L"%s <font color='#bababa'>+</font> %s"
#define DEATH_NOTICE_ATTACKER_NO_ASSIST	L"%s%s"
#endif

class SFHudDeathNoticeAndBotStatus : public SFHudFlashInterface
{
public:
	explicit SFHudDeathNoticeAndBotStatus( const char *value );
	virtual ~SFHudDeathNoticeAndBotStatus();

	// These overload the CHudElement class
	virtual void ProcessInput( void );
	virtual void LevelInit( void );
	virtual void LevelShutdown( void );
	virtual void SetActive( bool bActive );

	// these overload the ScaleformFlashInterfaceMixin class
	virtual void FlashReady( void );
	virtual bool PreUnloadFlash( void );

	virtual bool ShouldDraw( void );

	// CGameEventListener methods
	virtual void FireGameEvent( IGameEvent *event );

	void OnPlayerDeath( IGameEvent * event );

	void ClearNotices( void );

	// Scaleform callbacks
	void SetConfig( SCALEFORM_CALLBACK_ARGS_DECL );
protected:


	void ShowPanel( const bool bShow );

	// Add the text notice to our internal queue and trigger add
	// animation in scaleform.
	void PushNotice( const char * szNoticeText, bool isVictim, bool isKiller );
	void PushNotice( const wchar_t* wszNoticeText, bool isVictim, bool isKiller );
	
	struct NoticeText_t;
	void PushNotice( NoticeText_t& notice, bool isVictim, bool isKiller );

	void GetIconHTML( const wchar_t * szIcon, wchar_t * szBuffer, int nArraySize );

	// Show the death notices
	void Show( void );
	// Hide the death notices
	void Hide( void );


protected:
	enum
	{
		NS_PENDING,
		NS_SPAWNING,
		NS_IDLE,
		NS_FORCED_OUT,
	} NOTICE_STATE;

	struct NoticeText_t
	{
		NoticeText_t() :
			m_pPanel( NULL ),
			m_bRemove( false ),
			m_fTextHeight( 0.0f ),
			m_fSpawnTime( 0.0f ),
			m_fStateTime( 0.0f ),
			m_fLifetimeMod( 1.0f ),
			m_fY( 0.0f ),
			m_iState( NS_PENDING )
			{
				V_memset( m_szNotice, 0, sizeof( m_szNotice ) );
			};

		wchar_t					m_szNotice[DEATH_NOTICE_TEXT_MAX]; // Notice text
		bool					m_bRemove;					// Removal flag
		SFVALUE					m_pPanel;					// Handle to instantiated scaleform display object
		float					m_fTextHeight;
		int						m_iState;
		float					m_fSpawnTime;
		float					m_fStateTime;
		float					m_fLifetimeMod;
		float					m_fY;
		// its proper state after a show or hide
	};

	int							m_nNotificationDisplayMax;	// Number of notices to display at one time
	float						m_fNotificationLifetime;	// Notification display time before fading out
	float						m_fNotificationFadeLength;
	float						m_fNotificationScrollLength;
	float						m_fNextUpdateTime;
	float						m_fLocalPlayerLifetimeMod;

	CUtlVector<NoticeText_t>	m_vecNoticeText;			// Oldest notices at the end
	CUtlVector<NoticeText_t>	m_vecPendingNoticeText;		// Notices that are queued up to be displayed.
	CUtlVector<SFVALUE>			m_vecNoticeHandleCache;		// Cache of notice panel handles to reuse
															// (to avoid creating a new notice from scratch)

	bool						m_bVisible;					// Element visibility flag
	wchar_t						m_wCTColor[8];				// Highlight color for CT
	wchar_t						m_wTColor[8];				// Highlight color for T
};

#endif /* SFHUD_DEATHNOTICE_H_ */
