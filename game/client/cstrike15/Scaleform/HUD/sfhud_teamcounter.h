//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//
#ifndef SFHUD_TEAMCOUNTER_H_
#define SFHUD_TEAMCOUNTER_H_

#include "scaleformui/scaleformui.h"
#include "sfhudflashinterface.h"
#include "cs_gamerules.h"
#include "c_cs_player.h"

#if !defined( _X360 )
#include "xbox/xboxstubs.h"
#endif

#if defined(_PS3) || defined(POSIX)
#define TEAM_COUNT_IMG_STRING	L"<img src='icon-%ls.png' height='16'/>"
#define TEAM_COUNT_FONT_STRING	L"<font color=\"%ls\">%ls</font>"
#define TEAM_COUNT_FINAL_STRING L"%ls %ls"
#else
#define TEAM_COUNT_IMG_STRING	L"<img src='icon-%s.png' height='16'/>"
#define TEAM_COUNT_FONT_STRING	L"<font color=\"%s\">%s</font>"
#define TEAM_COUNT_FINAL_STRING L"%s %s"
#endif

struct MiniStatus 
{
	XUID		nXUID;
	int			nEntIdx;
	int			nPlayerIdx;
	int			nGunGameLevel;
	int			nHealth;
	int			nArmor;
	int			nTeammateColor;
	bool		bIsCT;
	bool		bLocalPlayer;
	bool		bDead;
	bool		bDominated;
	bool		bDominating;
	bool		bSpeaking;
	bool		bPlayerBot;		// indicates that the player took over this bot
	bool		bSpectated;	
	bool		bTeamLeader;	
	float		flLastRefresh;
	
	int			nGGProgressiveRank;	// save this character's rank
	int			nPoints;
	int			nTeam;

	MiniStatus() : 
		nXUID( INVALID_XUID ), nPlayerIdx(-1), nGunGameLevel(-1), bIsCT(false), bLocalPlayer(false), bDead(false), bDominated(false), 
		bDominating( false ), bTeamLeader( false ), bSpeaking( false ), bPlayerBot( false ), bSpectated( false ), nGGProgressiveRank( -1 ), nPoints( 0 ), nTeam( 0 ), nTeammateColor( -1 ), flLastRefresh( -1 )
	{
	}
	
	MiniStatus( const MiniStatus &copy )
	{
		memcpy( this, &copy, sizeof(MiniStatus) );
	}

	MiniStatus& operator=( const MiniStatus &rhs )
	{
		memcpy( this, &rhs, sizeof(MiniStatus) );
		return *this;
	}

	void Reset()
	{
		memset( this, 0, sizeof(MiniStatus) );
		nXUID = INVALID_XUID;
		nPlayerIdx = -1;
		nEntIdx = 0;
		nGGProgressiveRank = -1;
		flLastRefresh = -1;
	}

	// Returns true if any of these fields has changed since last update
	bool Update( XUID _Xuid, int _EntIdx, int _PlayerIdx, int _GunGameLevel, int _nHealth, int _nArmor, bool _IsCT, bool _LocalPlayer, bool _Dead, bool _Dominated, bool _Dominating, bool _bTeamLeader, bool _Speaking, bool _PlayerBot, bool _Spectated, int _Points, int _Team, int _TeammateColor, float _CurtimeRefresh )
	{
		bool bDiff = ( _Xuid != nXUID ) ||
			( _EntIdx != nEntIdx ) ||
			( _PlayerIdx != nPlayerIdx ) ||
			( _GunGameLevel != nGunGameLevel ) ||
			( _nHealth != nHealth ) ||
			( _nArmor != nArmor ) ||
			( _GunGameLevel != nGunGameLevel ) ||
			( _IsCT			^ bIsCT ) ||
			( _LocalPlayer	^ bLocalPlayer ) ||
			( _Dead			^ bDead ) ||
			( _Dominated	^ bDominated ) ||
			( _Dominating	^ bDominating ) ||
			( _bTeamLeader	^ bTeamLeader ) ||
			( _Speaking		^ bSpeaking ) ||
			( _PlayerBot	^ bPlayerBot ) ||
			( _Spectated	^ bSpectated ) ||
			( _Points != nPoints ) ||
			( _Team != nTeam ) ||
			( _TeammateColor != nTeammateColor ) ||
			( _CurtimeRefresh > flLastRefresh + 3.0f );

		nXUID			= _Xuid;
		nEntIdx			= _EntIdx;
		nPlayerIdx		= _PlayerIdx;
		nGunGameLevel	= _GunGameLevel;
		nHealth			= _nHealth;
		nArmor			= _nArmor;
		bIsCT			= _IsCT;
		bLocalPlayer	= _LocalPlayer;
		bDead			= _Dead;
		bDominated		= _Dominated;
		bDominating		= _Dominating;
		bTeamLeader		= _bTeamLeader;
		bSpeaking		= _Speaking;
		bPlayerBot		= _PlayerBot;
		bSpectated		= _Spectated;
		nPoints			= _Points;
		nTeam			= _Team;
		nTeammateColor  = _TeammateColor;
		if ( bDiff )
			flLastRefresh	= _CurtimeRefresh;

		return bDiff;
	}
};


class SFHudTeamCounter : public SFHudFlashInterface, public IShaderDeviceDependentObject
{
public:
	explicit SFHudTeamCounter( const char *value );
	virtual ~SFHudTeamCounter();

	// These overload the CHudElement class
	virtual void ProcessInput( void );
	virtual void LevelInit( void );
	virtual void LevelShutdown( void );
	virtual void SetActive( bool bActive );
	virtual bool ShouldDraw( void );

	// these overload the ScaleformFlashInterfaceMixin class
	virtual void FlashReady( void );
	virtual bool PreUnloadFlash( void );

	// CGameEventListener methods
	virtual void FireGameEvent( IGameEvent *event );

	int FindNextObserverTargetIndex( bool reverse );
	int GetSpectatorTargetFromSlot( int idx );

	virtual void DeviceLost( void ) { }
	virtual void DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd ) { m_bForceAvatarRefresh = true; }
	virtual void ScreenSizeChanged( int width, int height ) { }

	void OnFlashResize( SCALEFORM_CALLBACK_ARGS_DECL );	

	int GetPlayerEntIndexInSlot( int nIndex );
	int GetPlayerSlotIndex( int playerEntIndex );
	int GetPlayerSlotIndex( int playerEntIndex ) const; // version with no side effects
	int GetPlayerSlotIndexForDisplay( int playerEntIndex ) const; // When displayed to users, slot is one higher and slot 10 is displayed as 0 for legacy reasons. 

protected:
	void	LockSlot( bool wantItLocked, bool& currentlyLocked );

	// update game clock
	void	UpdateTimer( void );

	// update team scores and balance of power indicator
	void	UpdateScore( void );

	// update current player's team selection graphic
	void	UpdateTeamSelection( void );

	// update mini-scoreboard (team list + status bar)
	void	UpdateMiniScoreboard( void );

	void	ShowPanel( const bool bShow );
	void	GetIconHTML( const char * szIcon, wchar_t * szBuffer, int nBufferSize );

	// resets the progressive leader HUD with the local player and weapon
	// and resets m_nLeaderWeaponRank
	void	ResetLeader();

	void	InvokeAvatarSlotUpdate( SFVALUEARRAY &avatarData, const MiniStatus* msInfo, int slotNumber );

	static int GGProgSortFunction( MiniStatus* const* entry1, MiniStatus* const* entry2 );
	static int DMSortFunction( MiniStatus* const* entry1, MiniStatus* const* entry2 );

protected:
	enum BALANCE_OF_POWER
	{
		BOP_EVEN = 0,			// teams even
		BOP_CT = TEAM_CT,		// CT winning
		BOP_T = TEAM_TERRORIST	// T winning 
	};

	enum VIEW_MODE
	{
		VIEW_MODE_NORMAL = 0,			
		VIEW_MODE_GUN_GAME_PROGRESSIVE,	// Gun game progressive view mode
		VIEW_MODE_GUN_GAME_BOMB,	// Gun game bomb view mode
		VIEW_MODE_NUM,
	};

	ISFTextObject *		m_pTimeRedText;
	ISFTextObject *		m_pTimeGreenText;
	ISFTextObject *		m_pTime;
	ISFTextObject *		m_pCTScore;
	ISFTextObject *		m_pTScore;
	ISFTextObject *		m_pCTGunGameBombScore;
	ISFTextObject *		m_pTGunGameBombScore;

	SFVALUE				m_ProgressiveLeaderHandle;

	bool				m_bTimerAlertTriggered;		// True if the timer's alert state has been triggered
	bool				m_bRoundStarted;			// True if a round is currently in progress
	bool				m_bTimerHidden;				// True if the timer is hidden from view
	int					m_nTScoreLastUpdate;		// Terrorist score posted to Scaleform
	int					m_nCTScoreLastUpdate;		// Counter-Terrorist score posted to Scaleform
	int					m_nTeamSelectionLastUpdate;	// Team selection posted to Scaleform
	int					m_nLeaderWeaponRank;		// The weapon rank of the current leader
	VIEW_MODE			m_Mode;						// Tracks the current view mode

	bool				m_bRoundIsEnding;			// True if the round ended event was received
	bool				m_bIsBombDefused;

	bool				m_bColorTabsInitialized;

	enum PLAYER_TEAM_COUNT
	{
		MAX_TEAM_SIZE = 16
	};
	enum GG_PROG_PLAYER_COUNT
	{
		MAX_GGPROG_PLAYERS = 32
	};

	// track the team info we've already pushed to the mini-scoreboard
	int					m_nTerroristTeamCount;
	int					m_nCTTeamCount;
	MiniStatus			m_TerroristTeam[MAX_TEAM_SIZE];
	MiniStatus			m_CTTeam[MAX_TEAM_SIZE];

	// for GG Prog only: Track the status of the players, regardless of team
	int					m_nPreviousGGProgressiveTotalPlayers;
	MiniStatus			m_GGProgressivePlayers[MAX_GGPROG_PLAYERS];
	CUtlVector<MiniStatus*> m_ggSortedList;
	
	CountdownTimer		m_GGProgRankingTimer;

	// Signals that avatar images should be reloaded (needed after a render device reset)
	bool				m_bForceAvatarRefresh;

protected:
	// Sets the view mode, hiding or showing elements and changing
	// update behavior
	void	SetViewMode( VIEW_MODE mode );

	const MiniStatus* GetPlayerStatus( int index );
	const MiniStatus* GetPlayerStatus( int index ) const; // version with no side effects

private:
	float m_flPlayingTeamFadeoutTime;
	float m_flLastSpecListUpdate;

};

#endif /* SFHUD_TEAMCOUNTER_H_ */
