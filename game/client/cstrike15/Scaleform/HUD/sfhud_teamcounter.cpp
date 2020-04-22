//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//


#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include "scaleformui/scaleformui.h"
#include "sfhud_teamcounter.h"
#include "c_team.h"
#include "c_cs_playerresource.h"
#include "c_plantedc4.h"
#include "vgui/ILocalize.h"
#include "matchmaking/imatchframework.h"
#include "voice_status.h"
#include "vstdlib/vstrtools.h"
#include "gameui_util.h"
#include "sfhudfreezepanel.h"
#include "sfhudwinpanel.h"
#include "gametypes.h"
#include "cs_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IGameTypes *g_pGameTypes;
extern ConVar cl_spec_swapplayersides;
extern ConVar cl_draw_only_deathnotices;

DECLARE_HUDELEMENT( SFHudTeamCounter );

SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( OnFlashResize ),
SFUI_END_GAME_API_DEF( SFHudTeamCounter, TeamCount );

// When somebody surpasses the current GG Prog leader, save off their player index
static int g_GGProgLeaderPlayerIdx = -1;

SFHudTeamCounter::SFHudTeamCounter( const char *value ) : SFHudFlashInterface( value ),
	m_pTimeRedText( NULL ),
	m_pTimeGreenText ( NULL ),
	m_pTime ( NULL ),
	m_pCTScore ( NULL ),
	m_pTScore( NULL ),
	m_ProgressiveLeaderHandle( NULL ),
	m_pCTGunGameBombScore( NULL ),
	m_pTGunGameBombScore( NULL ),
	m_bTimerAlertTriggered ( false ),
	m_bRoundStarted ( true ),
	m_bIsBombDefused( false ),
	m_bTimerHidden ( false ),
	m_nTScoreLastUpdate ( -1 ),
	m_nCTScoreLastUpdate ( -1 ),
	m_Mode( VIEW_MODE_NORMAL ),
	m_nLeaderWeaponRank( -1 ),
	m_nTerroristTeamCount( -1 ),
	m_nCTTeamCount( -1 ),
	m_nPreviousGGProgressiveTotalPlayers( -1 ),
	m_bForceAvatarRefresh( false ),
	m_flPlayingTeamFadeoutTime( -1 ),
	m_flLastSpecListUpdate( -1 )
{
	SetHiddenBits( HIDEHUD_MINISCOREBOARD );
}


SFHudTeamCounter::~SFHudTeamCounter()
{
}


void SFHudTeamCounter::FlashReady( void )
{
	if ( m_FlashAPI && m_pScaleformUI )
	{	
		ListenForGameEvent( "round_start" );
		ListenForGameEvent( "round_announce_warmup" );	
		ListenForGameEvent( "round_end" );
		ListenForGameEvent( "cs_match_end_restart" );
		ListenForGameEvent( "bomb_planted" );
		ListenForGameEvent( "bomb_defused" );
		ListenForGameEvent( "player_spawn" );
		ListenForGameEvent( "player_death" );
		ListenForGameEvent( "bot_takeover" );
		ListenForGameEvent("player_team");

		m_GGProgRankingTimer.Invalidate();

		if ( CSGameRules() && (CSGameRules()->IsPlayingGunGameProgressive() || CSGameRules()->IsPlayingGunGameDeathmatch()) )
		{
			SetViewMode( VIEW_MODE_GUN_GAME_PROGRESSIVE );
		}
		else if ( CSGameRules() && CSGameRules()->IsPlayingGunGameTRBomb() )
		{
			SetViewMode( VIEW_MODE_GUN_GAME_BOMB );
			m_nLeaderWeaponRank = -1;
			g_GGProgLeaderPlayerIdx = -1;
		}
		else
		{
			SetViewMode( VIEW_MODE_NORMAL );
		}

		WITH_SLOT_LOCKED
		{
			// initialize scaleform elements
			SFVALUE topPanel = m_pScaleformUI->Value_GetMember( m_FlashAPI, "TopPanel" );

			if ( topPanel )
			{
				SFVALUE panel = m_pScaleformUI->Value_GetMember( topPanel, "Panel" );

				if ( panel )
				{
					// init score variables
					SFVALUE leaderPanel = m_pScaleformUI->Value_GetMember( panel, "ProgressiveLeader" );

					if ( leaderPanel )
					{
						m_ProgressiveLeaderHandle = m_pScaleformUI->Value_GetMember( leaderPanel, "Text" );
						SafeReleaseSFVALUE( leaderPanel );
					}

					// init time variable
					SFVALUE timePanel = m_pScaleformUI->Value_GetMember( panel, "Time" );

					if ( timePanel )
					{
						SFVALUE timeTextGreen = m_pScaleformUI->Value_GetMember( timePanel, "TimeGreen" );

						if ( timeTextGreen )
						{
							m_pTimeGreenText = m_pScaleformUI->TextObject_MakeTextObjectFromMember( timeTextGreen, "Text" );
							m_pTime = m_pTimeGreenText;
							SafeReleaseSFVALUE( timeTextGreen );

							if ( m_pTimeGreenText )
							{
								m_pTimeGreenText->SetText( L"0:00" );
							}
						}

						SFVALUE timeTextRed = m_pScaleformUI->Value_GetMember( timePanel, "TimeRed" );

						if ( timeTextRed )
						{
							m_pTimeRedText = m_pScaleformUI->TextObject_MakeTextObjectFromMember( timeTextRed, "Text" );
							SafeReleaseSFVALUE( timeTextRed );
						}

						SafeReleaseSFVALUE( timePanel );
					}

					// init score variables
					SFVALUE ctLabelPanel = m_pScaleformUI->Value_GetMember( panel, "CTLabel" );

					if ( ctLabelPanel )
					{
						m_pCTScore = m_pScaleformUI->TextObject_MakeTextObjectFromMember( ctLabelPanel, "CTCount" );
						SafeReleaseSFVALUE( ctLabelPanel );

						if ( m_pCTScore )
						{		
							m_pCTScore->SetText( L"0" );
						}
					}

					SFVALUE tLabelPanel = m_pScaleformUI->Value_GetMember( panel, "TLabel" );

					if ( tLabelPanel )
					{
						m_pTScore = m_pScaleformUI->TextObject_MakeTextObjectFromMember( tLabelPanel, "TCount" );
						SafeReleaseSFVALUE( tLabelPanel );

						if ( m_pTScore )
						{
							m_pTScore->SetText( L"0" );
						}
					}


					SFVALUE ctGunGameBombPanel = m_pScaleformUI->Value_GetMember( panel, "GunGameBombCTScore" );

					if ( ctGunGameBombPanel )
					{
						m_pCTGunGameBombScore = m_pScaleformUI->TextObject_MakeTextObjectFromMember( ctGunGameBombPanel, "Text" );
						SafeReleaseSFVALUE( ctGunGameBombPanel );

						if ( m_pCTGunGameBombScore )
						{		
							m_pCTGunGameBombScore->SetText( L"0" );
						}
					}

					SFVALUE tGunGameBombPanel = m_pScaleformUI->Value_GetMember( panel, "GunGameBombTScore" );

					if ( tGunGameBombPanel )
					{
						m_pTGunGameBombScore = m_pScaleformUI->TextObject_MakeTextObjectFromMember( tGunGameBombPanel, "Text" );
						SafeReleaseSFVALUE( tGunGameBombPanel );

						if ( m_pTGunGameBombScore )
						{		
							m_pTGunGameBombScore->SetText( L"0" );
						}
					}


					SafeReleaseSFVALUE( panel );
				}

				SafeReleaseSFVALUE( topPanel );
			}
		}

		ShowPanel( false );

		// Reset all player tracking
		for ( int idx = 0; idx < MAX_TEAM_SIZE; ++idx )
		{
			m_CTTeam[idx].Reset();
			m_TerroristTeam[idx].Reset();
		}
		for ( int idx = 0; idx < MAX_GGPROG_PLAYERS; ++idx )
		{
			m_GGProgressivePlayers[idx].Reset();
		}
		m_nTerroristTeamCount = 0;
		m_nCTTeamCount = 0;
		m_nPreviousGGProgressiveTotalPlayers = 0;

		g_pScaleformUI->AddDeviceDependentObject( this );
	}
}


bool SFHudTeamCounter::PreUnloadFlash( void )
{
	// cleanup
	g_pScaleformUI->RemoveDeviceDependentObject( this );

	StopListeningForAllEvents();

	SafeReleaseSFTextObject( m_pTimeGreenText );
	SafeReleaseSFTextObject( m_pTimeRedText );
	SafeReleaseSFTextObject( m_pCTScore );
	SafeReleaseSFTextObject( m_pTScore );
	SafeReleaseSFTextObject( m_pCTGunGameBombScore );
	SafeReleaseSFTextObject( m_pTGunGameBombScore );
	
	SafeReleaseSFVALUE( m_ProgressiveLeaderHandle );

	return true;
}


void SFHudTeamCounter::LevelInit( void )
{
	if ( !FlashAPIIsValid() )
	{
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFHudTeamCounter, this, TeamCount );
	}

	m_ggSortedList.RemoveAll();

	m_bTimerAlertTriggered = false;
	m_pTime = m_pTimeGreenText;
	m_bTimerHidden = false;
	m_nTScoreLastUpdate = -1;
	m_nCTScoreLastUpdate = -1;

	m_bRoundStarted = false;

	// check if the round is already in progress when we first join it
	C_CSGameRules *pRules = CSGameRules();
	if ( pRules )
	{
		m_bRoundStarted = ( pRules->GetRoundStartTime() < gpGlobals->curtime );
	}
}


void SFHudTeamCounter::LevelShutdown( void )
{
	if ( FlashAPIIsValid() )
	{
		RemoveFlashElement();
	}
}

bool SFHudTeamCounter::ShouldDraw( void )
{
	/* Removed for partner depot */
	return false;
}

void SFHudTeamCounter::SetActive( bool bActive )
{
	if ( FlashAPIIsValid() )
	{
		if ( bActive != m_bActive )
		{
			ShowPanel( bActive );
		}
	}

	CHudElement::SetActive( bActive );
}

void SFHudTeamCounter::LockSlot( bool wantItLocked, bool& currentlyLocked )
{
	if ( currentlyLocked != wantItLocked )
	{
		if ( wantItLocked )
		{
			LockScaleformSlot();
		}
		else
		{
			UnlockScaleformSlot();
		}

		currentlyLocked = wantItLocked;
	}
}

void SFHudTeamCounter::ProcessInput( void )
{
	// update game clock
	UpdateTimer();

	// update scores
	UpdateScore();

	// update team list (mini-scoreboard)
	UpdateMiniScoreboard();
}

void SFHudTeamCounter::UpdateTimer( void )
{
	C_CSGameRules *pRules = CSGameRules();
	bool bBeginTimerAlert = false;
	bool bCancelTimerAlert = false;
	bool bSetTime = false;

	if ( !pRules || !FlashAPIIsValid() )
	{
		return;
	}

	// timer is hidden when bomb planted, so break out of update
	if ( g_PlantedC4s.Count() > 0 && !CSGameRules()->IsPlayingCoopMission() )
	{
		if ( !m_bTimerHidden )
		{
			m_bTimerHidden = true;

			WITH_SLOT_LOCKED
			{
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onHideTimer", NULL, 0 );
			}
		}

		if ( m_bIsBombDefused )
		{
			WITH_SLOT_LOCKED
			{
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setBombDefused", NULL, 0 );
			}
		}
		else
		{
			WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, g_PlantedC4s[0]->GetDetonationProgress() * 100.0f );
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "updatePlantedBombState", args, 1 );
			}
		}

		return;
	}
	
	if ( CSGameRules() && CSGameRules()->IsWarmupPeriod() && m_pTime )
	{
		WITH_SLOT_LOCKED
		{
			m_pTime->SetText( "" );
		}
		return;
	}
	else if ( CSGameRules() && CSGameRules()->IsFreezePeriod() && 
		( CSGameRules( )->IsMatchWaitingForResume( ) ) )
	{
		WITH_SLOT_LOCKED
		{
			m_pTime = m_pTimeRedText;
			m_pTime->SetText( "❚❚" );
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onBeginTimerAlert", NULL, 0 );
		}
		return;
	}

	int nTimer = static_cast<int>( ceil( pRules->GetRoundRemainingTime() ) );

	bool bFreezePeriod = pRules->IsFreezePeriod();
	if ( bFreezePeriod )
	{
		// countdown to the start of the round while we're in freeze period
		nTimer = static_cast<int>( ceil( pRules->GetRoundStartTime() - gpGlobals->curtime ) );
	}

	const int kTimeRemainingToDisplayRed = 11;

	if ( m_bRoundStarted )
	{
		if ( !m_bTimerAlertTriggered && ( nTimer < kTimeRemainingToDisplayRed ) )
		{
			// when time is low switch to red text
			m_bTimerAlertTriggered = true;
			m_pTime = m_pTimeRedText;
			bBeginTimerAlert = true;
		}
		else if ( m_bTimerAlertTriggered && ( nTimer >= kTimeRemainingToDisplayRed ) )
		{
			// revert to normal timer color
			m_bTimerAlertTriggered = false;
			m_pTime = m_pTimeGreenText;
			bCancelTimerAlert = true;
		}
	}

	if ( nTimer < 0 )
	{
		nTimer = 0;
	}

	int nMinutes = nTimer / 60;
	int nSeconds = nTimer % 60;

	wchar_t szTime[32];
	szTime[0] = 0;

	if ( m_pTime && m_bRoundStarted )
	{
		V_snwprintf( szTime, ARRAYSIZE( szTime ), L"%d:%.2d", nMinutes, nSeconds );
		bSetTime = true;
	}

	if ( ( bSetTime || bBeginTimerAlert || bCancelTimerAlert ) && ( m_FlashAPI ) )
	{
		WITH_SLOT_LOCKED
		{
			if ( bCancelTimerAlert )
			{
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onBeginTimerNormal", NULL, 0 );
			}

			if ( bBeginTimerAlert )
			{
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onBeginTimerAlert", NULL, 0 );
			}

			if ( ( bSetTime ) && ( m_pTime ) )
			{
				m_pTime->SetText( szTime );
			}
		}
	}
}


void SFHudTeamCounter::UpdateScore( void )
{
	float totalTGunGameBombScore = 0.0f;
	float totalCTGunGameBombScore = 0.0f;		
	bool bShowTotalBombScore = false;

	int nCTScore = 0;
	int nTScore = 0;

	if ( !g_PR || !CSGameRules() || !m_bActive )
	{
		return;
	}
									
	if ( m_Mode == VIEW_MODE_NORMAL || m_Mode == VIEW_MODE_GUN_GAME_BOMB )
	{
		// [jason] The numbers now reflect rounds won, instead of remaining players
		C_Team *CT_team = GetGlobalTeam( TEAM_CT );
		if ( CT_team )
		{
			nCTScore = CT_team->Get_Score();
		}

		C_Team *T_team = GetGlobalTeam( TEAM_TERRORIST );
		if ( T_team )
		{
			nTScore = T_team->Get_Score();
		}
	}
		 
	// Total up team kill points
	if ( CSGameRules()->IsPlayingGunGameTRBomb() )
	{         
		bShowTotalBombScore = true;

		C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );	
		if ( cs_PR != NULL )
		{
			for ( int nPlayerIndex = 1; nPlayerIndex <= MAX_PLAYERS; nPlayerIndex++ )
			{
				if ( cs_PR->IsConnected( nPlayerIndex ) )
				{
					if ( cs_PR->GetTeam( nPlayerIndex ) == TEAM_TERRORIST )
					{       			                     
						//$TODO: Use kill points when we have access to them
						totalTGunGameBombScore += cs_PR->GetScore( nPlayerIndex );
					}
					else if ( cs_PR->GetTeam( nPlayerIndex ) == TEAM_CT )
					{      																			
						//$TODO: Use kill points when we have access to them
						totalCTGunGameBombScore += cs_PR->GetScore( nPlayerIndex );
					}
				}
			}			
		}		
	}


	WITH_SLOT_LOCKED
	{
		if ( m_pTScore && ( m_nTScoreLastUpdate != nTScore ) )
		{
			m_nTScoreLastUpdate = nTScore;
			m_pTScore->SetText( m_nTScoreLastUpdate );
		}

		if ( m_pCTScore && ( m_nCTScoreLastUpdate != nCTScore ) )
		{
			m_nCTScoreLastUpdate = nCTScore;
			m_pCTScore->SetText( m_nCTScoreLastUpdate );
		}

		if ( bShowTotalBombScore )
		{
			if ( m_pCTGunGameBombScore )
			{				
				m_pCTGunGameBombScore->SetText( totalCTGunGameBombScore );
			}

			if ( m_pTGunGameBombScore )
			{				
				m_pTGunGameBombScore->SetText( totalTGunGameBombScore );
			}
		}
	}
}

int SFHudTeamCounter::GGProgSortFunction( MiniStatus* const* entry1, MiniStatus* const* entry2 )
{
	if ( entry1 == NULL || (*entry1) == NULL )
		return 1;

	if ( entry2 == NULL || (*entry2) == NULL )
		return -1;

	// Higher GG Progressive weapon ranks higher.  In case of ties for that, we rank according to player index so 
	//   we don't overly shuffle the ordering
	if ( (*entry1)->nGunGameLevel > (*entry2)->nGunGameLevel )
		return -1;
	else if ( (*entry1)->nGunGameLevel < (*entry2)->nGunGameLevel )
		return 1;
	else
	{
		// always put the current leader up top
		if ( ( *entry1 )->bTeamLeader && ( *entry2 )->bTeamLeader == false )
			return -1;
		else if ( ( *entry2 )->bTeamLeader && ( *entry1 )->bTeamLeader == false )
			return 1;

		// Current GG leader always sorts in front in the case of a tie
		if ( (*entry1)->nPlayerIdx == g_GGProgLeaderPlayerIdx )
			return -1;
		else if ( (*entry2)->nPlayerIdx == g_GGProgLeaderPlayerIdx )
			return 1;
		else
			return (*entry1)->nPlayerIdx - (*entry2)->nPlayerIdx;
	}
}

int SFHudTeamCounter::DMSortFunction( MiniStatus* const* entry1, MiniStatus* const* entry2 )
{
	if ( entry1 == NULL || (*entry1) == NULL )
		return 1;

	if ( entry2 == NULL || (*entry2) == NULL )
		return -1;

	// Higher GG Progressive weapon ranks higher.  In case of ties for that, we rank according to player index so 
	//   we don't overly shuffle the ordering
	if ( (*entry1)->nPoints > (*entry2)->nPoints )
		return -1;
	else if ( (*entry1)->nPoints < (*entry2)->nPoints )
		return 1;
	else
		return (*entry1)->nPlayerIdx - (*entry2)->nPlayerIdx;
}

const MiniStatus* SFHudTeamCounter::GetPlayerStatus( int index )
{
	// check to see if we haven't been updated in the last second
	if (m_bActive == false && (m_flLastSpecListUpdate + 1.0f < gpGlobals->curtime))
	{
		// the list hasn;t been updated recently (probably because the hud element is hidden) so update it now
		UpdateMiniScoreboard();
	}

	if ( !CSGameRules() )
		return NULL;

//	bool bIsCompetitive = sv_competitive_official_5v5.GetInt() || CSGameRules()->IsPlayingAnyCompetitiveStrictRuleset();
	return const_cast< const SFHudTeamCounter* > ( this )->GetPlayerStatus( index );
}

const MiniStatus* SFHudTeamCounter::GetPlayerStatus( int index ) const
{
	if ( CSGameRules()->IsPlayingGunGameProgressive() || CSGameRules()->IsPlayingGunGameDeathmatch() )
	{
		if ( index < 0 )
		{
			return NULL;
		}
		else if ( index >= m_ggSortedList.Count() )
		{
			return NULL;
		}
		else
		{
			return m_ggSortedList[index];
		}

	}
	else
	{
		bool bAreTeamsSwitched = false;//Helper_DisplayTeamsOnOppositeSides();

		if ( bAreTeamsSwitched )
		{
			int i = m_nTerroristTeamCount - index - 1;

			if ( i >= 0 )
			{
				return &m_TerroristTeam[i];
			}

			i = -i-1;

			if ( i < m_nCTTeamCount )
			{
				return &m_CTTeam[i];
			}
		}
		else
		{
			int i = m_nCTTeamCount - index - 1;

			if ( i >= 0 )
			{
				return &m_CTTeam[i];
			}

			i = -i-1;

			if ( i < m_nTerroristTeamCount )
			{
				return &m_TerroristTeam[i];
			}
		}

		return NULL;
	}
}

int SFHudTeamCounter::GetPlayerSlotIndex( int playerEntIndex )
{
	// check to see if we haven't been updated in the last second
	if (m_bActive == false && (m_flLastSpecListUpdate + 1.0f < gpGlobals->curtime))
	{
		// the list hasn;t been updated recently (probably because the hud element is hidden) so update it now
		UpdateMiniScoreboard();
	}

	return const_cast< const SFHudTeamCounter* > ( this )->GetPlayerSlotIndex( playerEntIndex );

}

int SFHudTeamCounter::GetPlayerSlotIndex( int playerEntIndex ) const
{
	for ( int i = 0; i < MAX_TEAM_SIZE*2; i++ )
	{
		const MiniStatus* pMS = GetPlayerStatus( i );
		if ( pMS && ( pMS->nPlayerIdx == playerEntIndex ) )
		{
			return i;
		}
	}

	return -1;
}

// Internally this class keeps observer slots 0 based but has a similar check scattered in other files to bump up to one based whenever it displays... Making method for that.
// These are used for the 1 thorugh 0 keyboard keys to select an observer, so return an error value if slot is out of range. 
// This hack will die with scaleform.
int SFHudTeamCounter::GetPlayerSlotIndexForDisplay( int playerEntIndex ) const
{
	int iObserverDisplaySlot = GetPlayerSlotIndex( playerEntIndex );
	if ( iObserverDisplaySlot >= 0 && iObserverDisplaySlot <= 9 )
		return ( iObserverDisplaySlot + 1 ) % 10;
	else
		return -1;
}

int SFHudTeamCounter::GetPlayerEntIndexInSlot( int nIndex )
{
	if ( !FlashAPIIsValid() )
	{
		return -1;
	}

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
	{
		return -1;
	}

	const MiniStatus* pMS = GetPlayerStatus( nIndex );
	if ( pMS )
	{
		return pMS->nPlayerIdx;
	}

	return -1;
}

int SFHudTeamCounter::GetSpectatorTargetFromSlot( int idx )
{
	if ( !FlashAPIIsValid() )
	{
		return -1;
	}

	// check to see if we haven't been updated in the last second
	if (m_bActive == false && m_flLastSpecListUpdate + 1.0f < gpGlobals->curtime)
	{
		// the list hasn;t been updated recently (probably because the hud element is hidden) so update it now
		UpdateMiniScoreboard();
	}

	SF_SPLITSCREEN_PLAYER_GUARD();

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
	{
		return -1;
	}

//	int localPlayerIndex = GetLocalPlayerIndex();
// 
 	C_CS_PlayerResource* pCSPR = ( C_CS_PlayerResource* )g_PR;
// 
 	int spectatedTargetIndex = -1;
// 	//if ( GetSpectatorMode() == OBS_MODE_IN_EYE || GetSpectatorMode() == OBS_MODE_CHASE )
// 	{
 		spectatedTargetIndex = GetSpectatorTarget();
// 
// 		if ( !spectatedTargetIndex )
// 		{
// 			spectatedTargetIndex = localPlayerIndex;
// 		}
// 	}
// 
// 	if ( !spectatedTargetIndex )
// 	{
// 		return -1;
// 	}
// 
// 	int controlledPlayer = pCSPR->GetControlledPlayer( spectatedTargetIndex );
// 
// 	if ( controlledPlayer != 0 )
// 	{
// 		spectatedTargetIndex = controlledPlayer;
// 	}
// 
// 	int originalSlotIndex = GetPlayerSlotIndex( spectatedTargetIndex );
// 
// 	if ( originalSlotIndex == -1 || originalSlotIndex == idx )
// 	{
// 		return -1;
// 	}

	ConVarRef mp_forcecamera( "mp_forcecamera" );
	bool showEnemy = mp_forcecamera.GetInt() == OBS_ALLOW_ALL;

	if ( pLocalPlayer->GetTeamNumber() < TEAM_TERRORIST )
	{
		showEnemy = true;
	}

	bool bWantCT = spectatedTargetIndex > 0 ? ( pCSPR->GetTeam( spectatedTargetIndex ) == TEAM_CT ) : false;

	const MiniStatus* pStatus = GetPlayerStatus( idx );

	if ( pStatus && !pStatus->bDead && ( showEnemy || ( pStatus->bIsCT == bWantCT ) ) )
	{
		int result = pCSPR->GetControlledByPlayer( pStatus->nPlayerIdx );
		if ( !result )
		{
			result = pStatus->nPlayerIdx;
		}

		return result;
	}

	return -1;
}


int SFHudTeamCounter::FindNextObserverTargetIndex( bool reverse )
{
	if ( !FlashAPIIsValid() )
	{
		return -1;
	}

	// check to see if we haven't been updated in the last second
	if ( m_flLastSpecListUpdate + 1.0f < gpGlobals->curtime )
	{
		// the list hasn;t been updated recently (probably because the hud element is hidden) so update it now
		UpdateMiniScoreboard();
	}

	SF_SPLITSCREEN_PLAYER_GUARD();

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
	{
		return -1;
	}

	int localPlayerIndex = GetLocalPlayerIndex();

	C_CS_PlayerResource* pCSPR = ( C_CS_PlayerResource* )g_PR;

	int spectatedTargetIndex = -1;
	if ( GetSpectatorMode() == OBS_MODE_IN_EYE || GetSpectatorMode() == OBS_MODE_CHASE )
	{
		spectatedTargetIndex = GetSpectatorTarget();

		if ( !spectatedTargetIndex )
		{
			spectatedTargetIndex = localPlayerIndex;
		}
	}

	if ( !spectatedTargetIndex )
	{
		return -1;
	}

	int controlledPlayer = pCSPR->GetControlledPlayer( spectatedTargetIndex );

	if ( controlledPlayer != 0 )
	{
		spectatedTargetIndex = controlledPlayer;
	}

	int originalSlotIndex = GetPlayerSlotIndex( spectatedTargetIndex );

	if ( originalSlotIndex == -1 )
	{
		return -1;
	}

	ConVarRef mp_forcecamera( "mp_forcecamera" );
	bool showEnemy = mp_forcecamera.GetInt() == OBS_ALLOW_ALL;

	if ( ( pLocalPlayer->GetTeamNumber() < TEAM_TERRORIST ) && ( !pLocalPlayer->IsCoach() ) )
	{
		showEnemy = true;
	}

	bool bWantCT = ( pCSPR->GetTeam( spectatedTargetIndex ) == TEAM_CT );

	int result = -1;
	int currentSlotIndex = originalSlotIndex;

	int nMaxPlayers = MAX_TEAM_SIZE*2;//MIN( g_pGameTypes->GetCurrentServerNumSlots( ), 24 );

	while ( result == -1 )
	{
		if ( reverse )
		{
			currentSlotIndex--;
		}
		else
		{
			currentSlotIndex++;
		}

		if ( currentSlotIndex >= nMaxPlayers )
		{
			currentSlotIndex -= nMaxPlayers;
		}
		else if ( currentSlotIndex < 0 )
		{
			currentSlotIndex += nMaxPlayers;
		}

		if ( currentSlotIndex == originalSlotIndex )
		{
			break;
		}

		const MiniStatus *pStatus = GetPlayerStatus( currentSlotIndex );

		if ( pStatus && !pStatus->bDead && ( showEnemy || ( pStatus->bIsCT == bWantCT ) ) )
		{
			result = pCSPR->GetControlledByPlayer( pStatus->nPlayerIdx );
			if ( !result )
			{
				result = pStatus->nPlayerIdx;
			}
		}
	}

	return result;
}


// How many parameters we allocate to pass to function "UpdateAvatarSlot"
static const int kNumAvatarParams = 11;	

// WARNING: This function MUST be called from within a locked block
void SFHudTeamCounter::InvokeAvatarSlotUpdate( SFVALUEARRAY &avatarData, const MiniStatus* ms, int slotNumber )
{
	if ( !ms )
		return;

	WITH_SLOT_LOCKED
	{
		if ( avatarData.GetValues() == NULL )
		{
			m_pScaleformUI->CreateValueArray( avatarData, kNumAvatarParams );
		}

		m_pScaleformUI->ValueArray_SetElement( avatarData, 0,  slotNumber );

		char xuidAsText[256] = { 0 };
		g_PR->FillXuidText( ms->nPlayerIdx, xuidAsText, sizeof( xuidAsText ) );
		m_pScaleformUI->ValueArray_SetElement( avatarData, 1, xuidAsText );

		bool bGGProgressive = ( ms->nGGProgressiveRank >= 0 );
		bool bDeathmatch = CSGameRules() && CSGameRules()->IsPlayingGunGameDeathmatch();

		int nTeam = ms->bIsCT ? TEAM_CT: TEAM_TERRORIST;
		bool bIsLeader = false;
		if ( CSGameRules()->IsPlayingGunGameProgressive() )
			bIsLeader = (ms->nEntIdx == GetGlobalTeam( nTeam )->GetGGLeader( nTeam ));

		// Pack all the flags as a bitfield to send to Scaleform
		int Flags = 0;
		Flags |= ( (0x1 & ms->bIsCT)		<< 0 );
		Flags |= ( (0x1 & ms->bLocalPlayer)	<< 1 );
		Flags |= ( (0x1 & ms->bDead)		<< 2 );
		Flags |= ( (0x1 & ms->bDominated)	<< 3 );
		Flags |= ( (0x1 & ms->bDominating)	<< 4 );
		Flags |= ( (0x1 & ms->bSpeaking)	<< 5 );
		Flags |= ( (0x1 & ms->bPlayerBot)	<< 6 );
		Flags |= ( (0x1 & ms->bSpectated)	<< 7 );
		Flags |= ( (0x1 & bGGProgressive)	<< 8 );
		Flags |= ( (0x1 & bIsLeader)		<< 9 );

		m_pScaleformUI->ValueArray_SetElement( avatarData, 2, Flags );
		m_pScaleformUI->ValueArray_SetElement( avatarData, 3, g_PR->GetPlayerName( ms->nPlayerIdx ) );

		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
		// set health for all spectators and gotv clients
		bool bShowHealth = CanSeeSpectatorOnlyTools();
		if ( bShowHealth == false && pLocalPlayer )
		{
			ConVarRef mp_forcecamera( "mp_forcecamera" );
			if ( !pLocalPlayer->IsOtherEnemy( ms->nPlayerIdx ) || (!pLocalPlayer->IsAlive() && mp_forcecamera.GetInt() == OBS_ALLOW_ALL) )
				bShowHealth = true;
		}

		m_pScaleformUI->ValueArray_SetElement( avatarData, 4, bShowHealth ? ms->nHealth : 0 );
		m_pScaleformUI->ValueArray_SetElement( avatarData, 5, bShowHealth ? ms->nArmor : 0 );
		m_pScaleformUI->ValueArray_SetElement( avatarData, 6, m_bForceAvatarRefresh );

		//int nReverseLevel = CSGameRules()->GetNumProgressiveGunGameWeapons( ms->bIsCT ? TEAM_CT : TEAM_TERRORIST ) - ms->nGunGameLevel;
		int nLevel = 0;
		if ( bDeathmatch )
			nLevel = ms->nPoints;
		else if ( bGGProgressive )
			nLevel = ms->nGunGameLevel;

		m_pScaleformUI->ValueArray_SetElement( avatarData, 7, nLevel );

		int nWeaponIndex = CSGameRules()->GetCurrentGunGameWeapon(ms->nGunGameLevel, ms->bIsCT ? TEAM_CT : TEAM_TERRORIST);
		const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo((CSWeaponID)nWeaponIndex);
		const char *szWeaponName = (!bDeathmatch && pWeaponInfo) ? pWeaponInfo->szClassName : "";

		if (CSGameRules() && CSGameRules()->IsPlayingGunGameProgressive())
		{
			const CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinition(nWeaponIndex);
			if (pDef && pDef->GetDefinitionIndex() != 0)
			{
				szWeaponName = pDef->GetDefinitionName();
			}
			else
			{
				const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo((CSWeaponID)nWeaponIndex);
				if (pWeaponInfo)
				{
					szWeaponName = pWeaponInfo->szClassName;
				}
			}
		}

		// strip the weapon_* from the weapon name
		if (IsWeaponClassname(szWeaponName))
		{
			szWeaponName += WEAPON_CLASSNAME_PREFIX_LENGTH;
		}
		if (StringHasPrefix(szWeaponName, "knifegg"))
		{
			szWeaponName = "knife";
		}

		wchar_t wszWeaponHTML[64];
		GetIconHTML(szWeaponName, wszWeaponHTML, ARRAYSIZE(wszWeaponHTML));
		m_pScaleformUI->ValueArray_SetElement(avatarData, 8, wszWeaponHTML);
		m_pScaleformUI->ValueArray_SetElement( avatarData, 9, ms->nTeammateColor );

		bool bShowLetter = false;
		if ( pLocalPlayer && pLocalPlayer->ShouldShowTeamPlayerColors( ms->bIsCT ? TEAM_CT : TEAM_TERRORIST ) )
			bShowLetter = pLocalPlayer->ShouldShowTeamPlayerColorLetters();

		m_pScaleformUI->ValueArray_SetElement( avatarData, 10, bShowLetter );

		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "UpdateAvatarSlot", avatarData, kNumAvatarParams );
	}
}

void SFHudTeamCounter::UpdateMiniScoreboard( void )
{
	if ( !FlashAPIIsValid() || !CSGameRules() )
		return;

	SF_SPLITSCREEN_PLAYER_GUARD();

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return;

	// set the time when we updated
	m_flLastSpecListUpdate = gpGlobals->curtime;

	C_CS_PlayerResource* pCSPR = ( C_CS_PlayerResource* )g_PR;

	int localPlayerIndex = GetLocalPlayerIndex();

	m_nTeamSelectionLastUpdate = pLocalPlayer->GetTeamNumber();	

	int spectatedTargetIndex = -1;
	if ( GetSpectatorMode() == OBS_MODE_IN_EYE || GetSpectatorMode() == OBS_MODE_CHASE )
	{
		spectatedTargetIndex = GetSpectatorTarget();
	}

	bool bGunGameProgressive = CSGameRules()->IsPlayingGunGameProgressive();
	bool bDeathmatch = CSGameRules()->IsPlayingGunGameDeathmatch();

	int	nTerroristTeamCount = 0;
	int	nCTTeamCount = 0;

	SFVALUEARRAY avatarData;

	bool bSlotIsLocked = false;

//	bool bIsCompetitive = sv_competitive_official_5v5.GetInt( ) || CSGameRules( )->IsPlayingAnyCompetitiveStrictRuleset( );

	if ( pLocalPlayer )
	{
		int LocalBotControlledIdx = -1;
		if ( pLocalPlayer->IsControllingBot() )
		{
			LocalBotControlledIdx = pLocalPlayer->GetControlledBotIndex();
		}

		int nPlayersAlive_T = 0;
		int nPlayersAlive_CT = 0;
		bool bUpdatePlayerNumbers = false;

		for ( int playerIndex = 1; playerIndex <= MAX_PLAYERS; playerIndex++ )
		{
			bool bIsConnected = g_PR->IsConnected( playerIndex );

			if ( !bIsConnected )
				continue;

			int TeamId = g_PR->GetTeam( playerIndex );

			// Mini-Scoreboard only reflects the active players, not spectators or those who haven't selected a team
			if ( TeamId != TEAM_CT && TeamId != TEAM_TERRORIST )
				continue;
				
			int gunGameLevel = -1;

			int nEntIdx = 0;
			C_CSPlayer *pPlayer = ToCSPlayer( UTIL_PlayerByIndex( playerIndex ) );
			if ( pPlayer )
			{
				nEntIdx = pPlayer->entindex();

				if ( bGunGameProgressive )
				{
					gunGameLevel = pPlayer->GetPlayerGunGameWeaponIndex();

					if ( pPlayer->MadeFinalGunGameProgressiveKill() )
						gunGameLevel++;
				}
			}

			bool bIsLocalPlayer = ( localPlayerIndex == playerIndex );

			bool bSpeaking = GetClientVoiceMgr()->IsPlayerSpeaking( playerIndex );
			if ( bSpeaking )
			{
				if ( !g_PR->IsFakePlayer( playerIndex ) && !GetClientVoiceMgr()->IsPlayerAudible( playerIndex ) )
					bSpeaking = false;
			}

			bool bIsCT = ( TeamId == TEAM_CT );

			bool bShowHealth = CanSeeSpectatorOnlyTools();
			if ( bShowHealth == false && pLocalPlayer )
			{
				ConVarRef mp_forcecamera( "mp_forcecamera" );
				if ( !pLocalPlayer->IsOtherEnemy( playerIndex ) || ( !pLocalPlayer->IsAlive() && mp_forcecamera.GetInt() == OBS_ALLOW_ALL ) )
					bShowHealth = true;
			}

			XUID playerXuid = g_PR->GetXuid( playerIndex );

			bool bDead = false;

			int ControlledByIdx = pCSPR->GetControlledByPlayer( playerIndex );

			if ( ControlledByIdx != 0 )
			{
				// If we're a bot currently controlled by a player, we defer to their alive state
				bDead = !g_PR->IsAlive( ControlledByIdx );
			}
			else
			{
				// We are dead if we are not alive, or whenever we're controlling a bot (in this case, we want to show the bot as alive instead)
				bDead = !g_PR->IsAlive( playerIndex ) || pCSPR->IsControllingBot( playerIndex );

				//If a bot is kicked while we control it, just show us as alive.
				int controlledBot = pCSPR->GetControlledPlayer( playerIndex );
				if ( pCSPR->IsControllingBot( playerIndex ) && ( !pCSPR->IsConnected( controlledBot ) || !pCSPR->IsFakePlayer( controlledBot ) ) )
				{
					bDead = false;
				}
			}

			if ( TeamId == TEAM_CT && !bDead )
				nPlayersAlive_CT++;
			else if ( TeamId == TEAM_TERRORIST && !bDead )
				nPlayersAlive_T++;

			bool bIsSpectating = false; 

			int nHealth = 0; 
			int nArmor = 0;
			int nPoints = 0;

			if ( pCSPR->GetControlledPlayer( playerIndex ) == 0 )
			{
				int playerHealthIndex = playerIndex;
				int controlledByIndex = pCSPR->GetControlledByPlayer( playerIndex );

				if ( controlledByIndex != 0 )
				{
					playerHealthIndex = controlledByIndex;
				}

				bIsSpectating = ( playerHealthIndex == spectatedTargetIndex );
				
				if ( g_PR->IsAlive( playerHealthIndex ) && bShowHealth )
				{
					nHealth = pCSPR->GetHealth( playerHealthIndex );
					nArmor = pCSPR->GetArmor( playerHealthIndex );
				}

				nPoints = pCSPR->GetScore( playerHealthIndex );
			}

			MiniStatus *ms = NULL;

			int slotIdx = -1;
			if ( bIsCT )
			{
				// this team is full - we have no more space for players in our roster
				if ( !bGunGameProgressive && !bDeathmatch )
				{
					if ( nCTTeamCount == (MAX_GGPROG_PLAYERS/2) )
						continue;
				}
				else
				{
					if ( nCTTeamCount == MAX_TEAM_SIZE )
						continue;
				}

				slotIdx = nCTTeamCount++;
			}
			else
			{
				// this team is full - we have no more space for players in our roster
				if ( !bGunGameProgressive && !bDeathmatch )
				{
					if ( nTerroristTeamCount == (MAX_GGPROG_PLAYERS/2) )
						continue;
				}
				else
				{
					if ( nTerroristTeamCount == MAX_TEAM_SIZE )
						continue;
				}

				slotIdx = nTerroristTeamCount++;
			}

			if ( !bGunGameProgressive && !bDeathmatch )
			{
				// Grab from the team container
				MiniStatus *pTeamStatuses = bIsCT ? m_CTTeam : m_TerroristTeam;
				ms = &pTeamStatuses[ slotIdx ];
			}
			else
			{
				slotIdx = nCTTeamCount + nTerroristTeamCount - 1;
					
				// we only support up to this number of players in GG Progressive
				if ( slotIdx >= MAX_GGPROG_PLAYERS )
				{
					continue;
				}

				ms = &m_GGProgressivePlayers[ slotIdx ];
			}

			int nPlayerIdxForColor = -1;		
			if ( pLocalPlayer && pLocalPlayer->ShouldShowTeamPlayerColors( ms->bIsCT ? TEAM_CT : TEAM_TERRORIST ) && TeamId == pLocalPlayer->GetTeamNumber( ) )
				nPlayerIdxForColor = pCSPR->GetCompTeammateColor( playerIndex );
// 
// 			C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
// 			if ( pLocalPlayer && pLocalPlayer->ShouldShowTeamPlayerColors( ms->bIsCT ? TEAM_CT : TEAM_TERRORIST ) )
// 			{
// 				//m_bColorTabsInitialized = false;
// 				bool bShowLetter = pLocalPlayer->ShouldShowTeamPlayerColorLetters();
// 
// 				C_CS_PlayerResource* pCSPR = ( C_CS_PlayerResource* )g_PR;
// 				if ( pCSPR )
// 				{
// 					nPlayerIdxForColor = pCSPR->GetCompTeammateColor( ms->nPlayerIdx );
// 					if ( nPlayerIdxForColor != -1 )
// 						ms->bNeedsColorUpdate = false;
// 				}
// 			}
// 			//else
// 			//	m_bColorTabsInitialized = true;

			int nTeam = bIsCT ? TEAM_CT : TEAM_TERRORIST;
			bool bTeamLeader = ( nEntIdx == GetGlobalTeam( nTeam )->GetGGLeader( nTeam ) );

			bool bRefresh = ms->Update( 
				playerXuid,
				nEntIdx,
				playerIndex,
				gunGameLevel,
				nHealth,
				nArmor,
				bIsCT,
				bIsLocalPlayer,
				bDead,
				pLocalPlayer->IsPlayerDominated( playerIndex ),
				pLocalPlayer->IsPlayerDominatingMe( playerIndex ),
				bTeamLeader,
				bSpeaking,
				( LocalBotControlledIdx == playerIndex ),
				bIsSpectating,
				nPoints,
				TeamId,
				nPlayerIdxForColor,
				gpGlobals->curtime
			);

// 			if ( CSGameRules( )->IsPlayingAnyCompetitiveStrictRuleset( ) && nPlayerIdxForColor == -1 && TeamId == pLocalPlayer->GetTeamNumber( ) )
// 				ms->bForceRefreshColor = true;

			if ( !bIsConnected )
				bRefresh = true;

			if ( m_bForceAvatarRefresh || bRefresh )
			{
				bUpdatePlayerNumbers = true;

				// If we're in Gun Game progressive and this player isn't ranked yet, do not update their slot until the next re-ranking
				if ( (!bDeathmatch && !bGunGameProgressive) || ms->nGGProgressiveRank != -1 )
				{
					LockSlot( true, bSlotIsLocked );
					InvokeAvatarSlotUpdate( avatarData, ms, (bGunGameProgressive || bDeathmatch) ? ms->nGGProgressiveRank : slotIdx );
				}
			}
		}

		if ( bUpdatePlayerNumbers)
		{
			WITH_SFVALUEARRAY_SLOT_LOCKED(data, 2)
			{
				m_pScaleformUI->ValueArray_SetElement(data, 0, nPlayersAlive_CT);
				m_pScaleformUI->ValueArray_SetElement(data, 1, nPlayersAlive_T);
				g_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "SetNumPlayersAlive", data, 2);
			}
		}
	}

	if ( bGunGameProgressive || bDeathmatch )
	{
		bool bPlayerCountChange = false;

		if ( (nCTTeamCount + nTerroristTeamCount)  != m_nPreviousGGProgressiveTotalPlayers )
		{
			m_nPreviousGGProgressiveTotalPlayers = (nCTTeamCount + nTerroristTeamCount);

			WITH_SFVALUEARRAY_SLOT_LOCKED( data, 2 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, m_nPreviousGGProgressiveTotalPlayers );
				m_pScaleformUI->ValueArray_SetElement( data, 1, bDeathmatch );
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "UpdateTotalProgressivePlayers", data, 2 );
			}

			bPlayerCountChange = true;
		}

		if ( bPlayerCountChange || !m_GGProgRankingTimer.HasStarted() || m_GGProgRankingTimer.IsElapsed() )
		{
			static const float kRankingUpdateInterval = 0.5f;
			m_GGProgRankingTimer.Start( kRankingUpdateInterval );

			m_ggSortedList.RemoveAll();

			for ( int NewIdx = 0; NewIdx < MIN( MAX_GGPROG_PLAYERS, m_nPreviousGGProgressiveTotalPlayers ); NewIdx++ )
			{
				MiniStatus *ms = &m_GGProgressivePlayers[NewIdx];

				m_ggSortedList.AddToTail( ms );
			}

			if ( bDeathmatch )
				m_ggSortedList.Sort( DMSortFunction );
			else
				m_ggSortedList.Sort( GGProgSortFunction );

			for ( int NewIdx = 0; NewIdx < MIN( MAX_GGPROG_PLAYERS, m_nPreviousGGProgressiveTotalPlayers ); NewIdx++ )
			{
				MiniStatus *ms = m_ggSortedList[NewIdx];
				if ( ms && ms->nPlayerIdx != -1 )
				{
					ms->nGGProgressiveRank = NewIdx;

					LockSlot( true, bSlotIsLocked );
					InvokeAvatarSlotUpdate( avatarData, ms, NewIdx );
				}
			}

			// Update the current leader text every time we sort the list
			if ( m_ggSortedList.Count() > 0 )
			{
				MiniStatus *ms = m_ggSortedList[0];
				if ( ms )
				{			
					int nPrevGGLevel = 0;
					bool bNoMoreLeads = false;
					for ( int i = 0; i < m_ggSortedList.Count(); i++ )
					{
						int nGGLevel = m_ggSortedList[i]->nGunGameLevel;
						if ( nGGLevel < nPrevGGLevel || nGGLevel == 0 )
							bNoMoreLeads = true;

						WITH_SFVALUEARRAY_SLOT_LOCKED( data, 2 )
						{
							m_pScaleformUI->ValueArray_SetElement( data, 0, i );
							m_pScaleformUI->ValueArray_SetElement( data, 1, !bNoMoreLeads );
							m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "UpdateLeaderWeaponVisibility", data, 2 );
						}

						nPrevGGLevel = nGGLevel;
					}
				}
			}
		}
	}
	else
	{
		m_ggSortedList.RemoveAll();

		// Hide remaining unused T avatars
		if ( nTerroristTeamCount != m_nTerroristTeamCount && nTerroristTeamCount < MAX_TEAM_SIZE )
		{
			LockSlot( true, bSlotIsLocked );
			
			int nCount = nTerroristTeamCount;

			// Clear the player index
			for ( int Idx = nCount; Idx < MAX_TEAM_SIZE; ++Idx )
			{
					m_TerroristTeam[Idx].nPlayerIdx = -1;			
			}			

			WITH_SFVALUEARRAY( data, 2 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, false );
				m_pScaleformUI->ValueArray_SetElement( data, 1, nCount );
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "DisableRemainingPlayerIcons", data, 2 );
			}
		}

		// Hide remaining unused CT avatars
		if ( nCTTeamCount != m_nCTTeamCount && nCTTeamCount < MAX_TEAM_SIZE )
		{
			LockSlot( true, bSlotIsLocked );

			int nCount = nCTTeamCount;

			// Clear the player index
			for ( int Idx = nCount; Idx < MAX_TEAM_SIZE; ++Idx )
			{
					m_CTTeam[Idx].nPlayerIdx = -1;			
			}			

			WITH_SFVALUEARRAY( data, 2 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, true );
				m_pScaleformUI->ValueArray_SetElement( data, 1, nCount );
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "DisableRemainingPlayerIcons", data, 2 );
			}
		}
	}

	if ( avatarData.GetValues() != NULL )
	{
		WITH_SLOT_LOCKED
		{
			m_pScaleformUI->ReleaseValueArray( avatarData );
		}
	}

	m_bForceAvatarRefresh = false;

	m_nTerroristTeamCount = nTerroristTeamCount;
	m_nCTTeamCount = nCTTeamCount;

	if ( m_flPlayingTeamFadeoutTime > -1 )
	{
		// if we've surpassed the fadeout time and it hasn't been surpassed by a full second, fade it out
		if ( m_flPlayingTeamFadeoutTime <= gpGlobals->curtime )
		{
			WITH_SLOT_LOCKED
			{
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "fadeOutSelectedTeam", NULL, 0 );	
			}
			m_flPlayingTeamFadeoutTime = -1;
		}
	}

	// Unlock the slot, we're done with updating
	LockSlot( false, bSlotIsLocked );
}

void SFHudTeamCounter::FireGameEvent( IGameEvent *event )
{
	const char *type = event->GetName();
	CBasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	int EventUserID = event->GetInt( "userid", -1 );
	int LocalPlayerID = ( pLocalPlayer != NULL ) ? pLocalPlayer->GetUserID() : -2;

	if ( !V_strcmp( type, "round_start" ) )
	{
		m_bRoundStarted = true;
		m_bIsBombDefused = false;

		if ( m_FlashAPI && m_pScaleformUI )
		{
			WITH_SLOT_LOCKED
			{
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onBeginTimerNormal", NULL, 0 );

				if ( m_Mode == VIEW_MODE_GUN_GAME_PROGRESSIVE )
				{
					ResetLeader();
					m_GGProgRankingTimer.Invalidate();
					m_GGProgRankingTimer.Start( 0.5f );
				}
			}
		}
	}
	else if ( !V_strcmp( type, "round_announce_warmup" ) )
	{
		if ( m_FlashAPI && m_pScaleformUI )
		{
			WITH_SLOT_LOCKED
			{
				if ( m_pTime )
					m_pTime->SetText( "" );
			}
		}
	}
	else if ( !V_strcmp( type, "round_end" ) )
	{
		// Update the team timer one last time at round end, so we properly show "0:00" if the round timed out
		C_CSGameRules *pRules = CSGameRules();
		if ( pRules && m_pTime )
		{
			int nTimer = static_cast<int>( floor( pRules->GetRoundRemainingTime() ) );
			if ( nTimer < 0 )
				nTimer = 0;

			wchar_t szTime[32];
			V_snwprintf( szTime, ARRAYSIZE( szTime ), L"%d:%.2d", (nTimer / 60), (nTimer % 60) );

			WITH_SLOT_LOCKED
			{
				m_pTime->SetText( szTime );
			}

			int iReason = event->GetInt( "reason", -1 );

			switch ( iReason )
			{
// 			case Target_Bombed:
// 				break;

			case Bomb_Defused:

				m_bIsBombDefused = true;

			default:
				WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
				{
					m_pScaleformUI->ValueArray_SetElement( args, 0, 0.0f );
					g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "updatePlantedBombState", args, 1 );
				}
			}
		}

		m_bTimerAlertTriggered = false;
		m_pTime = m_pTimeGreenText;
		m_bRoundStarted = false;

		WITH_SLOT_LOCKED
		{
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hideDisplayTeamPanels", NULL, 0 );
		}
	}
	else if ( !V_strcmp( type, "cs_match_end_restart" ) )
	{
		if ( m_Mode == VIEW_MODE_GUN_GAME_PROGRESSIVE || m_Mode == VIEW_MODE_GUN_GAME_BOMB )
		{
			ResetLeader();
		}
	}
	else if ( !V_strcmp( type, "bomb_planted" ) )
	{
		if ( m_FlashAPI && m_pScaleformUI )
		{
			if ( !CSGameRules()->IsPlayingCoopMission() )
			{
				WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
				{
					g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onHideTimer", NULL, 0 );

					m_pScaleformUI->ValueArray_SetElement( args, 0, 100.0f );
					g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "updatePlantedBombState", args, 1 );
				}
			}
		}
	}

	else if ( !V_strcmp( type, "player_spawn" ) )
	{
		UpdateMiniScoreboard();

		if ( EventUserID == LocalPlayerID )
		{
			WITH_SLOT_LOCKED
			{
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hideDisplayTeamPanels", NULL, 0 );

				int nTeam = event->GetInt( "teamnum", -1 );

				if ( nTeam > 0 && !CSGameRules()->IsPlayingGunGameProgressive() && !CSGameRules()->IsPlayingGunGameDeathmatch() )
				{
					WITH_SFVALUEARRAY( data, 1 )
					{
						m_pScaleformUI->ValueArray_SetElement( data, 0, nTeam );
						g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onSpawnDisplaySelectedTeam", data, 1 );
					}

					m_flPlayingTeamFadeoutTime = gpGlobals->curtime + 10.0f;
				}
			}

			if ( m_Mode == VIEW_MODE_GUN_GAME_PROGRESSIVE || m_Mode == VIEW_MODE_GUN_GAME_BOMB )
			{
				// only need to update this once when the local player first spawns in
				if ( m_nLeaderWeaponRank == -1 )
				{
					ResetLeader();
				}
			}

			// Sanity check: round should have already started once we are spawned
			m_bRoundStarted = true;
		}
	}
	else if ( !V_strcmp( type, "player_death" ) && EventUserID == LocalPlayerID )
	{
		WITH_SLOT_LOCKED
		{
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hideDisplayTeamPanels", NULL, 0 );
		}
	}
	else if (!V_strcmp(type, "player_team") && (EventUserID == LocalPlayerID))
	{
		m_bForceAvatarRefresh = true;

		UpdateMiniScoreboard();

// 		int playerIndex = GetPlayerIndexFromUserID(event->GetInt("userid"));
// 
// 		if (playerIndex == INVALID_INDEX)
// 		{
// 			return;
// 		}
// 
// 		SFHudRadarIconPackage* pPackage = GetRadarPlayer(playerIndex);
// 		pPackage->SetPlayerTeam(event->GetInt("team"));
// 		UpdatePlayerNumber(pPackage);
	}
	else if ( !V_strcmp( type, "bot_takeover" ) && ( EventUserID == LocalPlayerID ) )
	{
		C_BasePlayer *pBot = UTIL_PlayerByUserId( event->GetInt( "botid" ) );
		if ( pBot )
		{
			wchar_t wszLocalized[100];
			wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
			g_pVGuiLocalize->ConvertANSIToUnicode( pBot->GetPlayerName(), wszPlayerName, sizeof(wszPlayerName) );
			g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#SFUI_Notice_Hint_Bot_Takeover" ), 1, wszPlayerName );

			WITH_SFVALUEARRAY_SLOT_LOCKED( data, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, wszLocalized );
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onTakeOverBot", data, 1 );
			}

			m_flPlayingTeamFadeoutTime = -1;
		}	
	}
}

void SFHudTeamCounter::ShowPanel( const bool bShow )
{
	if ( m_FlashAPI )
	{
		WITH_SLOT_LOCKED
		{
			if ( bShow )
			{
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowPanel", NULL, 0 );
			}
			else
			{
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "HidePanel", NULL, 0 );
			}
		}
	}
}

void SFHudTeamCounter::SetViewMode( VIEW_MODE mode )
{
	Assert( mode >= VIEW_MODE_NORMAL && mode < VIEW_MODE_NUM );
	
	m_Mode = mode;

	// show ten if we are forcing this on the server
	int nMaxPlayers = CCSGameRules::GetMaxPlayers();

	if ( m_Mode == VIEW_MODE_GUN_GAME_PROGRESSIVE )
	{
		WITH_SLOT_LOCKED
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onSetModeGunGameProgressive", NULL, 0 );
		}
	}
	else if ( m_Mode == VIEW_MODE_GUN_GAME_BOMB )
	{
		WITH_SLOT_LOCKED
		{
			if ( nMaxPlayers > 10 )
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onSetModeGunGameBomb", NULL, 0 );
			else
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onSetModeGunGameBombTen", NULL, 0 );
		}
	}
	else
	{
		WITH_SLOT_LOCKED
		{
			if ( nMaxPlayers > 10 )
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onSetModeNormal", NULL, 0 );
			else
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onSetModeNormalTen", NULL, 0 );

			UpdateScore();
		}
	}
}


void SFHudTeamCounter::GetIconHTML( const char * szIcon, wchar_t * szBuffer, int nBufferSize )
{
	if ( szIcon == NULL || szIcon[0] == '\0' )
	{
		V_snwprintf( szBuffer, nBufferSize, L"" );
		return;
	}

	wchar_t wszIcon[64];

	wszIcon[0] = L'\0';
	V_UTF8ToUnicode( szIcon, wszIcon, sizeof( wszIcon ) );

	// convert to an "img src" formatting string for HTML
	V_snwprintf( szBuffer, nBufferSize, TEAM_COUNT_IMG_STRING, wszIcon );
}

void SFHudTeamCounter::ResetLeader()
{
	m_nLeaderWeaponRank = -1;
	g_GGProgLeaderPlayerIdx = -1;
}

void SFHudTeamCounter::OnFlashResize( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( FlashAPIIsValid() )
	{
		if ( GET_ACTIVE_SPLITSCREEN_SLOT() == 0 && C_BasePlayer::GetLocalPlayer() && engine->IsLocalPlayerResolvable() )
		{
			WITH_SLOT_LOCKED
			{
				//Resize the win panel when we resize the mini scoreboard
				SFHudWinPanel * pWinPanel = GET_HUDELEMENT( SFHudWinPanel );
				if ( pWinPanel )
				{
					pWinPanel->ApplyYOffset( m_pScaleformUI->Params_GetArgAsNumber( obj, 0 )  );
				}	
			}
		}
	}
}

#endif // INCLUDE_SCALEFORM
