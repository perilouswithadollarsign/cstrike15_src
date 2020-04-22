//========= Copyright © Valve Corporation, All rights reserved. ============//
//
// Purpose:  Use mouse control to select among displayed options
//
//=====================================================================================//
#pragma once

#include "hud.h"
#include "hud_element_helper.h"
#include "scaleformui/scaleformui.h"
#include "sfhudflashinterface.h"

class SFHudRosettaSelector : public SFHudFlashInterface
{
public:
	explicit SFHudRosettaSelector( const char *value );
	virtual ~SFHudRosettaSelector();

	// These overload the CHudElement class
	virtual void	ProcessInput( void ) OVERRIDE;
	virtual void	LevelInit( void ) OVERRIDE;
	virtual void	LevelShutdown( void ) OVERRIDE;
	virtual bool 	ShouldDraw( void ) OVERRIDE;
	virtual void	SetActive( bool bActive ) OVERRIDE;

	virtual void	Reset( void ) OVERRIDE;

	// these overload the ScaleformFlashInterfaceMixin class
	virtual void	FlashReady( void ) OVERRIDE;
	virtual bool	PreUnloadFlash( void ) OVERRIDE;

	void FlashHide( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetMouseEnableBindingName( SCALEFORM_CALLBACK_ARGS_DECL );
	void SetShowRosetta( bool bShow, const char* szType );

	bool Visible() const { return m_bVisible; }

	void HACK_OnShowCursorBindingDown( const char* szKeyName );
	int	KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );
private:
	void			ShowPanel( bool bShow );

	void Visible( bool val ) { m_bVisible = val; }
private:
	bool			m_bVisible;
};

bool Helper_CanUseSprays();