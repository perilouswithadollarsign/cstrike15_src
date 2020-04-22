//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:  Displays HUD element to show we are having connectivity trouble
//
//=====================================================================================//
#ifndef SFHUDAUTODISCONNECT_H_
#define SFHUDAUTODISCONNECT_H_

#include "hud.h"
#include "hud_element_helper.h"
#include "scaleformui/scaleformui.h"
#include "sfhudflashinterface.h"

class SFHudAutodisconnect : public SFHudFlashInterface
{
public:
	explicit SFHudAutodisconnect( const char *value );
	virtual ~SFHudAutodisconnect();

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

protected:
	void			ShowPanel( bool bShow );

protected:
	SFVALUE			m_sfuiControlBg, m_sfuiControlTopLabel, m_sfuiControlBottomLabel, m_sfuiControlTimerLabel, m_sfuiControlTimerIcon;
};


#endif /* SFHUDAUTODISCONNECT_H_ */
