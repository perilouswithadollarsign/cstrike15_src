//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ==================//
//
// Purpose:  Displays HUD elements about armor, current weapon, ammo, and TR weapon progress
//
//===========================================================================================//
#ifndef SFHUDHEALTHARMORPANEL_H_
#define SFHUDHEALTHARMORPANEL_H_

#include "hud.h"
#include "hud_element_helper.h"
#include "scaleformui/scaleformui.h"
#include "sfhudflashinterface.h"
#include "cs_gamerules.h"
#include "c_cs_player.h"


class SFHudHealthArmorPanel : public SFHudFlashInterface
{
public:
	explicit SFHudHealthArmorPanel( const char *value );
	virtual ~SFHudHealthArmorPanel();

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

protected:
	void	ShowPanel( bool value );
	void	SetVisible( bool bVisible );
	void	LockSlot( bool wantItLocked, bool& currentlyLocked );

protected:
	SFVALUE m_PanelHandle;
	SFVALUE m_HealthPanel;
	SFVALUE m_HealthPanelRed;
	SFVALUE m_HealthPanelRedSmall;
	SFVALUE m_HealthTextHandle;
	SFVALUE m_HealthTextHandleRed;
	SFVALUE m_ArmorTextHandle;
	SFVALUE m_HealthBarHandle;
	SFVALUE m_HealthRedBarHandle;

	int		m_PrevHealth;
	int		m_PrevArmor;
	bool	m_PrevHasHelmet;
	bool	m_PrevHasHeavyArmor;

	CountdownTimer	m_HealthFlashTimer;
};


#endif /* SFHUDHEALTHARMORPANEL_H_ */
