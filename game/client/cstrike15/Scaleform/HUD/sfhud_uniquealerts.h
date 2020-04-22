//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SFUNIQUEALERTS_H
#define SFUNIQUEALERTS_H
#pragma once

#include "hudelement.h"
#include "ehandle.h"
#include "hud.h"
#include "hud_element_helper.h"
#include "scaleformui/scaleformui.h"
#include "sfhudflashinterface.h"
#include "cs_gamerules.h"
#include "c_cs_player.h"
#include <vgui_controls/Panel.h>

#define MAX_PLAYER_GIFT_DROP_DISPLAY 4

// Experimental feature, not ready to enable.  Turn this on to visualize quest progress
// messages in the Flash GUI
#define MISSIONPANEL_ENABLE_QUEST_PROGRESSION 1

//-----------------------------------------------------------------------------
// Purpose: Used to draw the history of weapon / item pickups and purchases by the player
//-----------------------------------------------------------------------------
class SFUniqueAlerts : public SFHudFlashInterface
{
public:
	explicit SFUniqueAlerts( const char *value );	
	
	virtual void ProcessInput( void );	

	virtual void LevelInit( void );
	virtual void LevelShutdown( void );
	virtual void SetActive( bool bActive );
	virtual bool ShouldDraw( void );

	virtual void	Reset( void );

	virtual void FlashReady( void );
	virtual bool PreUnloadFlash( void );

	// CGameEventListener methods
	virtual void FireGameEvent( IGameEvent *event );
	virtual void OnTimeJump() OVERRIDE;

	// This should probably be a message listen, but I'm new and not sure the right layering.  Will update on review.
	void ShowQuestProgress(uint32 numPointsEarned, uint32 numPointsTotal, uint32 numPointsMax, const char* weaponName, const char* questDesc);

	// Scaleform callbacks
	void OnAlertPanelHideAnimEnd( SCALEFORM_CALLBACK_ARGS_DECL );

protected:
	void ShowDMBonusWeapon( CEconItemView* pItem, int nPos, int nSecondsLeft );

	// Helper functions for setting active/non-active
	void Show( void );
	void Hide( void );

	void UpdateGGWeaponIconList( void );
	void ShowWarmupAlertPanel( void );
	void ShowHltvReplayAlertPanel( int nHltvReplayLeft );

	// Display an alert in the 'alert text' area.  If 'oneShot' is true, will automatically hide.
	// One-shot messages always flash on set, otherwise will only flash if the panel is coming from
	// hidden to showing.
	void ShowAlertText( const wchar_t* szMessage, bool oneShot = false );

	// Hide a non-one-shot alert text
	void HideAlertText();

	// Update for each sub-panel
	// These can assume that all the following are non-null:
	//    CSGameRules()
	//    C_CSPlayer::GetLocalCSPlayer()
	//    m_pScaleformUI
	void ProcessMissionPanelGuardian();
	void ProcessDMBonusPanel();
	void ProcessAlertBar();
#if defined ( ENABLE_GIFT_PANEL )
	void ProcessGiftPanel();
#endif
#if MISSIONPANEL_ENABLE_QUEST_PROGRESSION
	void ProcessMissionPanelQuest();
#endif

private:
	enum AlertState {
		ALERTSTATE_HIDDEN,
		ALERTSTATE_SHOWING,
		ALERTSTATE_HIDING,	// Automatically will transition to ALERTSTATE_HIDDEN at some unknown point in the future
	};

	// Whether any of this UI is active
	// (it could all be hidden due to hud settings)
	bool	m_bVisible;

	// Alert line (e.g. "Round Start", "Warmup ends in xx:xx")
	AlertState m_AlertState;
	float	m_flNextAlertTick;

	// Demolition mode panel
	bool	m_bShowDemolitionProgressionPanel;

	// Deathmatch extra points for using a certain weapon panel
	bool	m_bDMBonusIsActive;
	float	m_flNextDMBonusTick;

	int		m_nDMCurrentWeaponPoints;
	int		m_nDMCurrentWeaponBonusPoints;
	int		m_nDMBonusWeaponSlot;

	// Mission/quest panel:
	bool	m_bMissionVisible;
	
	// Mission panel for guardian co-op missions
	bool	m_guardianPanelInitialized;
	float	m_guardianPanelHideTick;
	int		m_nGuardianKills;
	int		m_nGuardianWeapon;
	int		m_nPlayerMoney;
	bool	m_bGuardianHasWeapon;
	bool	m_bGuardianHasWeaponEquipped;

	// Mission panel for quest missions
	// (Only used if MISSIONPANEL_ENABLE_QUEST_PROGRESSION is enabled)
	bool	m_bQuestMissionActive;

	// Gift panel
#if defined ( ENABLE_GIFT_PANEL )
	bool	m_bIsGiftPanelActive;
	int		m_nGlobalGiftsGivenInPeriod;
	int		m_nGlobalNumGiftersInPeriod;
	int		m_nGlobalGiftingPeriodSeconds;
	float	m_flLastGiftPanelUpdate;

	struct GiftTempList_t 
	{
		int nDefindex;
		AccountID_t uiAccountID;
		int nNumGifts;
		bool bIsLocalPlayer;

		GiftTempList_t() : 
		nDefindex( 0 ), uiAccountID( 0 ), nNumGifts( 0 ), bIsLocalPlayer( false )
		{
		}
	};
	CUtlVector<GiftTempList_t> m_PlayerSpotlightTempList;
#endif // ENABLE_GIFT_PANEL

};

#endif // SFUNIQUEALERTS_H
