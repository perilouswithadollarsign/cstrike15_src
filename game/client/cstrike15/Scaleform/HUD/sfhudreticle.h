//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//
#ifndef SFHUDRETICLE_H_
#define SFHUDRETICLE_H_

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

#if defined(_PS3) || defined(POSIX)
#define HUDRET_WEPICON_SELECTED_IMG_STRING		L"<img src='icon-%ls.png' height='22'/>"
#define HUDRET_WEPICON_IMG_STRING					L"<img src='icon-%ls_grey.png' height='22'/>"
#else
#define HUDRET_WEPICON_SELECTED_IMG_STRING		L"<img src='icon-%s.png' height='22'/>"
#define HUDRET_WEPICON_IMG_STRING					L"<img src='icon-%s_grey.png' height='22'/>"
#endif

#define VIEWPUNCH_COMPENSATE_MAGIC_SCALAR 0.65 // cl_flinch_scale.GetFloat()
#define VIEWPUNCH_COMPENSATE_MAGIC_ANGLE 1

struct PlayerIDPanel
{	
	EHANDLE hPlayer;
	SFVALUE panel;
	SFVALUE arrowA;
	SFVALUE arrowB;
	SFVALUE arrowF;
	SFVALUE voiceIcon;
	SFVALUE defuseIcon;
	int iconsFlag;
	bool bActive;
	bool bShowName;
	bool bFriend;
	int nTeam;
	int nHealth;
	float flUpdateAt;
	float bFlashedAmt;

	float flLastHighlightTime;
	float flNameAlpha;
};

class SFHudReticle : public SFHudFlashInterface
{
	enum
	{
		TEXTFIELD_LENGTH = 256
	};

public:
	enum RETICLE_MODE
	{
		RETICLE_MODE_NONE,
		RETICLE_MODE_WEAPON,
		RETICLE_MODE_OBSERVER
	};

	explicit SFHudReticle( const char *value );
	virtual ~SFHudReticle();

	void OnSwapReticle( SCALEFORM_CALLBACK_ARGS_DECL );

	// These overload the CHudElement class
	virtual void ProcessInput( void );
	virtual void LevelInit( void );
	virtual void LevelShutdown( void );
	virtual void SetActive( bool bActive );
	virtual bool ShouldDraw( void );

	// these overload the ScaleformFlashInterfaceMixin class
	virtual void FlashReady( void );
	virtual bool PreUnloadFlash( void );
	
	virtual void FireGameEvent( IGameEvent *event );

	void ToggleTeamEquipmentVisibility( bool bShow );

protected:
	void ShowReticle( RETICLE_MODE mode, bool value );
	bool SetReticlePosition( int distance, int crosshairGap , int offsetX, int offsetY, int nDesiredFishtail );
	void LockSlot(bool wantItLocked, bool& currentlyLocked);

	void ResetDisplay( void );
	int  TeamToTextIndex( int iTeamNumber );

	// Swaps the current reticle assets
	void PerformSwapReticle( const char * szReticleName );

	void AddNewPlayerID( CBaseEntity *player, bool bShowName, bool bFriend = false );
	void UpdatePlayerID( CBaseEntity *player, int slot, bool bHealthAndNameOnly = false );
	void RemoveID( int index );
	void RemoveAllIDs( void );

	void GetIconHTML( const wchar_t * szIcon, wchar_t * szBuffer, int nBufferSize, bool bSelected );

	bool ShouldShowAllFriendlyTargetIDs( void );
	bool ShouldShowAllFriendlyEquipment( void );

protected:

	wchar_t m_wcIDString[ TEXTFIELD_LENGTH ];

	bool m_bCrosshairPositionsInitialized;

	double m_TopPipY;
	double m_BottomPipY;
	double m_LeftPipX;
	double m_RightPipX;

	float m_dotX;
	float m_dotY;
	float m_blackRingX;
	float m_blackRingY;
	float m_friendIndicatorX;
	float m_friendIndicatorY;

	float m_IDMovieX;
	float m_IDMovieY;

	SFVALUE m_WeaponCrosshairHandle;
	SFVALUE m_ObserverCrosshairHandle;
	SFVALUE m_TopPip;
	SFVALUE m_BottomPip;
	SFVALUE m_LeftPip;
	SFVALUE m_RightPip;

	SFVALUE m_topCrosshairArc;
	SFVALUE m_rightCrosshairArc;
	SFVALUE m_leftCrosshairArc;
	SFVALUE m_bottomCrosshairArc;

	SFVALUE m_FriendCrosshair;
	SFVALUE m_crosshairDot;
	SFVALUE m_blackRing;

	SFVALUE m_IDMovie;
	SFVALUE m_IDText;

	SFVALUE m_FlashedIcon;

	RETICLE_MODE m_iReticleMode;

	float m_fIDTimer;

	int m_iLastGap;
	int m_iLastSpread;

	bool m_bTextIDVisible;
	bool m_bFriendlyCrosshairVisible;
	bool m_bEnemyCrosshairVisible;

	bool m_bFlashedIconFadingOut;

	bool m_bForceShowAllTeammateTargetIDs;

	CUtlVector<PlayerIDPanel> m_playerIDs;

};

extern ConVar crosshair;
//extern ConVar sfcrosshair;


#endif /* SFHUDRETICLE_H_ */
