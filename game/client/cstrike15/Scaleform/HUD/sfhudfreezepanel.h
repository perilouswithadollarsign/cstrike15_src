//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SFHUDFREEZEPANEL_H
#define SFHUDFREEZEPANEL_H
#pragma once

#include "hudelement.h"
#include "ehandle.h"
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
#include <vgui_controls/Panel.h>

bool IsTakingAFreezecamScreenshot( void );

#define FREEZE_PANEL_NAME_TRUNCATE_AT_SHORT			10  // number of name character displayed before truncation
#define FREEZE_PANEL_NAME_TRUNCATE_AT_LONG			16  // number of name character displayed before truncation

class SFHudFreezePanel: public SFHudFlashInterface, public IShaderDeviceDependentObject 
{
public:
	explicit SFHudFreezePanel( const char *value );	
	virtual void LevelInit( void );
	virtual void LevelShutdown( void );
	virtual void SetActive( bool bActive );
	virtual bool ShouldDraw( void );
	virtual void FlashReady( void );
	virtual bool PreUnloadFlash( void );
	virtual void FireGameEvent( IGameEvent * event );
	virtual void ShowPanel( bool bShow );
	virtual void ShowCancelPanel( bool bShow );
	virtual bool IsVisible( void ) { return m_bIsVisible; }
	virtual void OnTimeJump( void ) OVERRIDE;
	void ResetDamageText( int iPlayerIndexKiller, int iPlayerIndexVictim );
	void OnHltvReplayButtonStateChanged( void );

	virtual void DeviceLost( void ) { }
	virtual void DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd );
	virtual void ScreenSizeChanged( int width, int height ) { }

	bool	IsHoldingAfterScreenShot( void ) { return m_bHoldingAfterScreenshot; }
	void	TakeFreezeShot( void );

	enum DominationIconType
	{
		None,
		Nemesis,
		Revenge,
		DominationIconMax
	};

private:
	void PopulateDominationInfo( DominationIconType iconType, const char* localizationToken1, const char* localizationToken2, wchar_t *szWeaponHTML );
	void PopulateWeaponInfo( wchar_t *szWeaponName );
	void PopulateNavigationText( void );
	void SetIcon( DominationIconType iconType );
	void ProcessInput( void );
	void PositionPanel( void );

	ISFTextObject*	m_dominationIcons[DominationIconMax];
	ISFTextObject*  m_dominationText1;
	ISFTextObject*	m_dominationText2;
	ISFTextObject*	m_killerName;
	ISFTextObject*  m_navigationText;

	ISFTextObject*  m_weaponInfoText1;
	ISFTextObject*	m_weaponInfoText2;

	ISFTextObject* m_ssDescText;
	ISFTextObject* m_ssNameText;

	SFVALUE m_freezePanel;
	SFVALUE m_ssFreezePanel;

	int m_PosX;
	int m_PosY;
	CHandle< CBaseEntity > m_FollowEntity;
	int m_iKillerIndex;

	const char *GetFilesafePlayerName( const char *pszOldName );
	bool m_bHoldingAfterScreenshot;

	bool m_bDominationIconVisible;
	bool m_bIsVisible;
	bool m_bIsCancelPanelVisible;
	bool m_bFreezePanelStateRelevant; // flag showing if the information in the freeze panel is relevant and it makes sense to show, tracks show_freezepanel and hide_freezepanel events from the game; the panel may still be hidden sometimes (like before autoreplay) to avoid confusing the player (e.g. right before autoplay kicks in and there's not enough time to virually process it for a human being), even if the information in there is relevant

};

#endif // SFHUDFREEZEPANEL_H
