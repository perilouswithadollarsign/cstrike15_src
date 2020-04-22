//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:  Displays HUD elements about health and armor
//
//=====================================================================================//
#ifndef SFHUDWEAPONPANEL_H_
#define SFHUDWEAPONPANEL_H_

#include "hud.h"
#include "hud_element_helper.h"
#include "scaleformui/scaleformui.h"
#include "sfhudflashinterface.h"
#include "cs_gamerules.h"
#include "c_cs_player.h"


class SFHudWeaponPanel : public SFHudFlashInterface
{
public:
	explicit SFHudWeaponPanel( const char *value );
	virtual ~SFHudWeaponPanel();

	// These overload the CHudElement class
	virtual void ProcessInput( void );
	virtual void LevelInit( void );
	virtual void LevelShutdown( void );
	virtual void SetActive( bool bActive );
	virtual bool ShouldDraw( void );

	virtual void Init( void ) { SetVisible( true ); }
	virtual void Reset( void ) { SetVisible( true ); }

	// these overload the ScaleformFlashInterfaceMixin class
	virtual void FlashReady( void );
	virtual bool PreUnloadFlash( void );

	virtual void FireGameEvent( IGameEvent *event );

protected:
	void	ShowPanel( bool value );
	void	SetVisible( bool bVisible );
	void	LockSlot( bool wantItLocked, bool& currentlyLocked );

protected:
	SFVALUE m_PanelHandle;
	SFVALUE m_CurrentWeaponImageHandle;
	SFVALUE m_CurrentWeaponTextHandle;
	SFVALUE m_AmmoTextClipHandle;
	SFVALUE m_AmmoTextTotalHandle;
	SFVALUE m_AmmoAnimationHandle;
	SFVALUE m_BurstIcons_Burst;
	SFVALUE m_BurstIcons_Single;
	SFVALUE m_WeaponPenetration1;
	SFVALUE m_WeaponPenetration2;
	SFVALUE m_WeaponPenetration3;
	SFVALUE m_UpgradeKill1;
	SFVALUE m_UpgradeKill2;
	SFVALUE m_UpgradeKillText;
	SFVALUE m_BombHandle;
	SFVALUE m_DefuseHandle;
	SFVALUE m_BombZoneHandle;
	SFVALUE	m_WeaponItemName;

	int		m_PrevAmmoClipCount;
	int		m_PrevAmmoTotalCount;
	int		m_PrevAmmoType;
	int		m_PrevWeaponID;
	int		m_PrevTRGunGameUpgradePoints;
	bool	m_bHiddenNoAmmo;				// we hid the panel because our current weapon has no ammo at all
	bool	m_bCarryingC4;					// player is carrying the bomb
	bool	m_bCarryingDefuse;				// player is carrying a defuse kit
	bool	m_bInBombZone ;					// player is in the bomb zone
	int		m_lastEntityIndex;
	int		m_LastNumRoundKills;
	int		m_lastKillEaterCount;
};


#endif /* SFHUDWEAPONPANEL_H_ */
