//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef PORTAL_CLIENTMODE_H
#define PORTAL_CLIENTMODE_H
#ifdef _WIN32
#pragma once
#endif

#include "clientmode_shared.h"
#include <vgui_controls/EditablePanel.h>
#include <vgui/Cursor.h>

class CHudViewport;
class CRadialMenu;

namespace vgui
{
	typedef unsigned long HScheme;
}

class ClientModePortalNormal : public ClientModeShared 
{
DECLARE_CLASS( ClientModePortalNormal, ClientModeShared );

private:

// IClientMode overrides.
public:

					ClientModePortalNormal();
	virtual			~ClientModePortalNormal();

	virtual void	Init();
	virtual void	InitViewport();
	virtual void	LevelInit( const char *newmap );
	virtual void	LevelShutdown( void );
	virtual void	SetBlurFade( float scale );
	virtual float	GetBlurFade( void ) { return m_BlurFadeScale; }
	virtual void	OnColorCorrectionWeightsReset( void );
	virtual float	GetColorCorrectionScale( void ) const { return 1.0f; }
	virtual void	InitWeaponSelectionHudElement( void ) { return; } // don't init this hud
	virtual bool	ShouldDrawCrosshair( void );
	virtual void	DoPostScreenSpaceEffects( const CViewSetup *pSetup );

	virtual int		HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );
	void InitRadialMenuHudElement( void );

	virtual float GetViewModelFOV( void );

	void StartTransitionFade( float flFadeTime );
private:
	
	//	void	UpdateSpectatorMode( void );
	// ClientCCHandle_t	m_CCDeathHandle;	// handle to death cc effect
	// float				m_flDeathCCWeight;	// for fading in cc effect

	CHandle<C_ColorCorrection>	m_hCurrentColorCorrection;

	float m_BlurFadeScale;

	CRadialMenu	*m_pRadialMenu;
};


extern IClientMode *GetClientModeNormal();
extern ClientModePortalNormal* GetClientModePortalNormal();


#endif // PORTAL_CLIENTMODE_H
