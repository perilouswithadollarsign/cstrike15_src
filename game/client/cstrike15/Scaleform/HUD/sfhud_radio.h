//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:  Displays HUD element to show we are having connectivity trouble
//
//=====================================================================================//
#ifndef SFHUDRADIO_H_
#define SFHUDRADIO_H_

#include "hud.h"
#include "hud_element_helper.h"
#include "scaleformui/scaleformui.h"
#include "sfhudflashinterface.h"

class SFHudRadio : public SFHudFlashInterface
{
public:
	explicit SFHudRadio( const char *value );
	virtual ~SFHudRadio();

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

	bool			PanelRaised( void );
	void			ShowRadioGroup( int nSetID );
	void			InvokeCommand( SCALEFORM_CALLBACK_ARGS_DECL );

private:
	void			ShowPanel( bool bShow );

private:
	bool			m_bVisible;
};


#endif /* SFHUDRADIO_H_ */
