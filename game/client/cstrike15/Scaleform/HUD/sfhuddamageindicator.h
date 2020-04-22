//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:  Displays HUD elements to indicate damage taken
//
//=====================================================================================//
#ifndef SFHUDDAMAGEINDICATOR_H_
#define SFHUDDAMAGEINDICATOR_H_

#include "hud.h"
#include "hud_element_helper.h"
#include "scaleformui/scaleformui.h"
#include "sfhudflashinterface.h"

class SFHudDamageIndicator : public SFHudFlashInterface
{
	enum DamageDirection
	{
		SFDD_DamageUp = 0,
		SFDD_DamageDown,
		SFDD_DamageLeft,
		SFDD_DamageRight,

		SFDD_DamageTotal, // also means turn on damage from all directions
		SFDD_DamageFirst = SFDD_DamageUp
	};

public:
	explicit SFHudDamageIndicator( const char *value );
	virtual ~SFHudDamageIndicator();

	// These overload the CHudElement class
	virtual void	ProcessInput( void );
	virtual void	LevelInit( void );
	virtual void	LevelShutdown( void );
	virtual bool 	ShouldDraw( void );
	virtual void	SetActive( bool bActive );

	virtual void	Reset( void );

	// these overload the ScaleformFlashInterfaceMixin class
	virtual void	FlashReady( void );
	virtual bool	PreUnloadFlash( void );

	// Handler for our message
	bool			MsgFunc_Damage( const CCSUsrMsg_Damage &msg );

	CUserMessageBinder m_UMCMsgDamage;

protected:
	void			IndicateDamage( DamageDirection dmgDir, float newPercentage );
	void			HideAll( void );

private:
	void			CalcDamageDirection( const Vector &vecFrom, C_BasePlayer *pVictimPlayer );

protected:

	float			m_lastFrameTime;

	// Parameters copied from cs_hud_damageindicator:
	float			m_flAttackFront;
	float			m_flAttackRear;
	float			m_flAttackLeft;
	float			m_flAttackRight;
	float			m_flFadeCompleteTime;	//don't draw past this time
};


#endif /* SFHUDINFOPANEL_H_ */
