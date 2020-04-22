//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef SFHUDWINPANEL_H
#define SFHUDWINPANEL_H
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

enum
{
	WIN_EXTRATYPE_NONE = 0,
	WIN_EXTRATYPE_AWARD,
	WIN_EXTRATYPE_RANK,
	WIN_EXTRATYPE_ELO,
	WIN_EXTRATYPE_GGNEXT,
	WIN_EXTRATYPE_SEASONRANK,
};

//-----------------------------------------------------------------------------
// Purpose: Used to draw the history of weapon / item pickups and purchases by the player
//-----------------------------------------------------------------------------
class SFHudWinPanel: public SFHudFlashInterface, public IShaderDeviceDependentObject 
{
public:
	explicit SFHudWinPanel( const char *value );	
	virtual void LevelInit( void );
	virtual void LevelShutdown( void );
	virtual void SetActive( bool bActive );	
	virtual void FlashReady( void );
	virtual bool PreUnloadFlash( void );	
	virtual void ProcessInput( void );
	virtual void FireGameEvent( IGameEvent * event );
	virtual bool ShouldDraw( void );

	// Offsets the Y location of the win panel
	void	ApplyYOffset( int nOffset );

	bool	IsVisible( void );

	// IShaderDeviceDependentObject methods
	virtual void DeviceLost( void ) { }
	virtual void DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd );
	virtual void ScreenSizeChanged( int width, int height ) { }

	int m_iMVP;


protected:
	void SetMVP( C_CSPlayer* pPlayer, CSMvpReason_t reason, int32 nMusicKitMVPs = 0  );
	void SetFunFactLabel( const wchar *szFunFact );
	void SetProgressBarText( int nAmount, const wchar *wszDescText );

	//void MovieClipSetVisibility( SFVALUE panel, bool bShow );
	//void TextPanelSetVisibility( ISFTextObject* panel, bool bShow );

private:	

	void ShowTeamWinPanel( int result, const char* winnerText );
	void ShowGunGameWinPanel( void /*int nWinner, int nSecond, int nThird*/ );
	void ShowWinExtraDataPanel( int nExtraPanelType );
	void SetWinPanelExtraData( void );
	void Hide( void );

	SFVALUE				m_hWinPanelParent;
	ISFTextObject*		m_hWinner;
	ISFTextObject*		m_hReason;	
	ISFTextObject*		m_hMVP;	
	ISFTextObject*		m_hSurrender;	

	ISFTextObject*		m_hFunFact;	

	SFVALUE			m_hEloPanel;	
	SFVALUE			m_hRankPanel;
	SFVALUE			m_hItemPanel;
	SFVALUE			m_hMedalPanel;
	SFVALUE			m_hProgressText;
	SFVALUE			m_hNextWeaponPanel;

	bool			m_bVisible;

	int				m_nFunFactPlayer;
	string_t		m_nFunfactToken;
	int				m_nFunFactParam1;
	int				m_nFunFactParam2;
	int				m_nFunFactParam3;


	int				m_nRoundStartELO;

	bool			m_bShouldSetWinPanelExtraData;
	float			m_fSetWinPanelExtraDataTime;
};

#endif // SFHUDWINPANEL_H
