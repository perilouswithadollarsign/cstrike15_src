//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include "sfhudwinpanel.h"
#include "hud_macros.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "iclientmode.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_element_helper.h"
#include "scaleformui/scaleformui.h"
#include "c_playerresource.h"
#include "c_cs_playerresource.h"
#include "sfhudfreezepanel.h"
#include "cs_player_rank_mgr.h"
#include "achievements_cs.h"
#include "cs_player_rank_shared.h"
#include "gamestringpool.h"
#include "sfhud_teamcounter.h"
#include "cs_client_gamestats.h"
#include "fmtstr.h"
#include <engine/IEngineSound.h>
#include "c_team.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( SFHudWinPanel );

SFUI_BEGIN_GAME_API_DEF
SFUI_END_GAME_API_DEF( SFHudWinPanel, WinPanel );

extern ConVar cl_draw_only_deathnotices;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SFHudWinPanel::SFHudWinPanel( const char *value ) : SFHudFlashInterface( value ),
	m_bVisible( false ),
	m_hWinPanelParent( NULL ),
	m_hWinner( NULL ),
	m_hReason( NULL ),
	m_hMVP( NULL ),
	m_hSurrender( NULL ),
	m_hFunFact( NULL ),
	m_hEloPanel( NULL ),	
	m_hRankPanel( NULL ),
	m_hMedalPanel( NULL ),
	m_hProgressText( NULL ),
	m_hNextWeaponPanel( NULL ),
	m_nFunFactPlayer( 0 ),
	m_nFunfactToken( NULL ),
	m_nFunFactParam1( 0 ),
	m_nFunFactParam2( 0 ),
	m_nFunFactParam3( 0 ),
	m_bShouldSetWinPanelExtraData( false ),
	m_fSetWinPanelExtraDataTime( 0.0 ),
	m_nRoundStartELO( 0 ),
	m_iMVP( 0 )
{
	SetHiddenBits( HIDEHUD_MISCSTATUS );
}

void SFHudWinPanel::LevelInit( void )
{
	int slot = GET_ACTIVE_SPLITSCREEN_SLOT();

	if ( !FlashAPIIsValid() && slot == 0 )
	{
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFHudWinPanel, this, WinPanel );
	}
}

void SFHudWinPanel::LevelShutdown( void )
{
	if ( FlashAPIIsValid() )
	{		
		RemoveFlashElement();
	}
}

bool SFHudWinPanel::ShouldDraw( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	return cl_drawhud.GetBool() && cl_draw_only_deathnotices.GetBool() == false && CHudElement::ShouldDraw();
}


void SFHudWinPanel::SetActive( bool bActive )
{
// 	if ( !bActive && m_bVisible )
// 	{
// 		Hide();
// 	}

	CHudElement::SetActive( bActive );
}

void SFHudWinPanel::FlashReady( void )
{
	if ( !m_FlashAPI )
	{
		return;
	}

	m_hWinPanelParent = g_pScaleformUI->Value_GetMember( m_FlashAPI, "WinPanel" );
	if ( m_hWinPanelParent )
	{
		SFVALUE innerRoot = g_pScaleformUI->Value_GetMember( m_hWinPanelParent, "InnerWinPanel" );

		if ( innerRoot )
		{
			m_hWinner	= g_pScaleformUI->TextObject_MakeTextObjectFromMember( innerRoot, "WinnerText" );	
			m_hReason	= g_pScaleformUI->TextObject_MakeTextObjectFromMember( innerRoot, "WinReason" );
			m_hMVP		= g_pScaleformUI->TextObject_MakeTextObjectFromMember( innerRoot, "MVPText" );
			m_hSurrender = g_pScaleformUI->TextObject_MakeTextObjectFromMember( innerRoot, "Surrender" );
			m_hFunFact	= g_pScaleformUI->TextObject_MakeTextObjectFromMember( innerRoot, "FunFact" );		

			m_hEloPanel = g_pScaleformUI->Value_GetMember( innerRoot, "EloPanel" );		
			m_hRankPanel = g_pScaleformUI->Value_GetMember( innerRoot, "RankPanel" );	
			m_hMedalPanel = g_pScaleformUI->Value_GetMember( innerRoot, "MedalPanel" );	
			m_hProgressText = g_pScaleformUI->Value_GetMember( innerRoot, "ProgressText" );	
			m_hNextWeaponPanel = g_pScaleformUI->Value_GetMember( innerRoot, "NextWeaponPanel" );	

			g_pScaleformUI->ReleaseValue( innerRoot );
		}
	}

	//Tell scaleform about the constant we plan on using later
	WITH_SFVALUEARRAY_SLOT_LOCKED( data, 3 )
	{						
		m_pScaleformUI->ValueArray_SetElement( data, 0, WINNER_DRAW  );
		m_pScaleformUI->ValueArray_SetElement( data, 1, WINNER_CT );
		m_pScaleformUI->ValueArray_SetElement( data, 2, WINNER_TER );

		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setResultConstants", data, 3 );
	}

	// listen for events	
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "cs_win_panel_round" );	
	ListenForGameEvent( "round_mvp" );

	g_pScaleformUI->AddDeviceDependentObject( this );

	Hide();	
}

bool SFHudWinPanel::PreUnloadFlash( void )
{
	g_pScaleformUI->RemoveDeviceDependentObject( this );
	SafeReleaseSFTextObject( m_hWinner );
	SafeReleaseSFTextObject( m_hReason );
	SafeReleaseSFTextObject( m_hMVP );
	SafeReleaseSFTextObject( m_hSurrender );
	SafeReleaseSFTextObject( m_hFunFact );

	SafeReleaseSFVALUE( m_hEloPanel );		
	SafeReleaseSFVALUE( m_hRankPanel );	
	SafeReleaseSFVALUE( m_hMedalPanel );	
	SafeReleaseSFVALUE( m_hProgressText );	
	SafeReleaseSFVALUE( m_hNextWeaponPanel );	

	SafeReleaseSFVALUE( m_hWinPanelParent );	

	return true;
}

void SFHudWinPanel::ProcessInput( void )
{
	if ( m_bShouldSetWinPanelExtraData && m_fSetWinPanelExtraDataTime <= gpGlobals->curtime )
	{
		m_bShouldSetWinPanelExtraData = false;
		SetWinPanelExtraData();
	}
}



CEG_NOINLINE void SFHudWinPanel::FireGameEvent( IGameEvent* event )
{
	const char *pEventName = event->GetName();
	
	if ( V_strcmp( "round_start", pEventName ) == 0 )
	{
		if ( m_pScaleformUI )
		{
			WITH_SLOT_LOCKED
			{
				// At the end of every round, clear the Scaleform mesh cache to recover some memory
				g_pScaleformUI->ClearCache();
			}
		}
		
		// Reset MVP info when round starts
		SetMVP( NULL, CSMVP_UNDEFINED );

		Hide();	
		GetViewPortInterface()->UpdateAllPanels();
	}

	else if ( V_strcmp( "round_mvp", pEventName ) == 0 )
	{		
		C_BasePlayer *basePlayer = UTIL_PlayerByUserId( event->GetInt( "userid" ) );		

		if ( basePlayer )
		{
			CSMvpReason_t mvpReason = (CSMvpReason_t)event->GetInt( "reason" );
			int32 nMusicKitMVPs = event->GetInt( "musickitmvps" );
			
			SetMVP( ToCSPlayer( basePlayer ), mvpReason, nMusicKitMVPs );
		}
	}
	else if ( V_strcmp( "cs_win_panel_round", pEventName ) == 0 )
	{
		if ( !FlashAPIIsValid() )
			return;

		m_bShouldSetWinPanelExtraData = true;
		m_fSetWinPanelExtraDataTime = gpGlobals->curtime + 1.0f;

		int nConnectionProtocol = engine->GetConnectionDataProtocol();
		int iEndEvent = event->GetInt( "final_event" );
		if ( nConnectionProtocol &&
			( iEndEvent >= 0 ) &&					// backwards compatibility: we switched to consistent numbering in the enum
			( nConnectionProtocol < 13500 ) )		// and older demos have one-less numbers in "final_event" before 1.35.0.0 (Sep 15, 2015 3:20 PM release BuildID 776203)
			++ iEndEvent;

		m_nFunFactPlayer = event->GetInt( "funfact_player" );
		m_nFunfactToken = AllocPooledString( event->GetString( "funfact_token", "" ) );
		m_nFunFactParam1 = event->GetInt( "funfact_data1" );
		m_nFunFactParam2 = event->GetInt( "funfact_data2" );
		m_nFunFactParam3 = event->GetInt( "funfact_data3" );

		if ( CSGameRules() && (CSGameRules()->IsPlayingGunGameProgressive() || CSGameRules()->IsPlayingGunGameDeathmatch()) )
		{
			ShowGunGameWinPanel();
		}
		else
		{
			// show the win panel
			switch ( iEndEvent )
			{
			case Target_Bombed:
			case VIP_Assassinated:
			case Terrorists_Escaped:
			case Terrorists_Win:
			case Hostages_Not_Rescued:            
			case VIP_Not_Escaped:
			case CTs_Surrender:
			case Terrorists_Planted:
				ShowTeamWinPanel( WINNER_TER, "SFUI_WinPanel_T_Win" );
				break;

			case VIP_Escaped:
			case CTs_PreventEscape:
			case Escaping_Terrorists_Neutralized:
			case Bomb_Defused:
			case CTs_Win:
			case All_Hostages_Rescued:
			case Target_Saved:
			case Terrorists_Not_Escaped:
			case Terrorists_Surrender:
			case CTs_ReachedHostage:
				ShowTeamWinPanel( WINNER_CT, "SFUI_WinPanel_CT_Win" );
				break;

			case Round_Draw:
				ShowTeamWinPanel( WINNER_DRAW, "SFUI_WinPanel_Round_Draw" );
				break;

			default:
				Assert( 0 );
				break;
			}

			Assert( m_pScaleformUI );
			WITH_SLOT_LOCKED
			{			
				// Set the MVP text.
				if ( m_hSurrender )
				{
					if ( iEndEvent == CTs_Surrender )
					{
						m_hSurrender->SetTextHTML( "#winpanel_end_cts_surrender" );
					}
					else if ( iEndEvent == Terrorists_Surrender )
					{
						m_hSurrender->SetTextHTML( "#winpanel_end_terrorists_surrender" );
					}
					else
					{
						m_hSurrender->SetTextHTML( "" );
					}
				}
			}

			//Map the round end events onto localized strings
			char* endEventToString[RoundEndReason_Count];
			V_memset( endEventToString, 0, sizeof( endEventToString ) );

			//terrorist win events
			endEventToString[Target_Bombed] = "#winpanel_end_target_bombed";
			endEventToString[VIP_Assassinated] = "#winpanel_end_vip_assassinated";
			endEventToString[Terrorists_Escaped] = "#winpanel_end_terrorists_escaped";
			endEventToString[Terrorists_Win] = "#winpanel_end_terrorists__kill";
			endEventToString[Hostages_Not_Rescued] = "#winpanel_end_hostages_not_rescued";
			endEventToString[VIP_Not_Escaped] = "#winpanel_end_vip_not_escaped";
			endEventToString[CTs_Surrender] = "#winpanel_end_cts_surrender";
			endEventToString[Terrorists_Planted] = "TEMP STRING - TERRORISTS PLANTED";

			//CT win events
			endEventToString[VIP_Escaped] = "#winpanel_end_vip_escaped";
			endEventToString[CTs_PreventEscape] = "#winpanel_end_cts_prevent_escape";
			endEventToString[Escaping_Terrorists_Neutralized] = "#winpanel_end_escaping_terrorists_neutralized";
			endEventToString[Bomb_Defused] = "#winpanel_end_bomb_defused";
			endEventToString[CTs_Win] = "#winpanel_end_cts_win";
			endEventToString[All_Hostages_Rescued] = "#winpanel_end_all_hostages_rescued";
			endEventToString[Target_Saved] = "#winpanel_end_target_saved";
			endEventToString[Terrorists_Not_Escaped] = "#winpanel_end_terrorists_not_escaped";
			endEventToString[Terrorists_Surrender] = "#winpanel_end_terrorists_surrender";
			endEventToString[CTs_ReachedHostage] = "#winpanel_end_cts_reach_hostage";		

			//We don't show a round end panel for these
			endEventToString[Game_Commencing] = "";
			endEventToString[Round_Draw] = "";

			const wchar_t* wszEventMessage = NULL;
			if ( iEndEvent >= 0 && iEndEvent < RoundEndReason_Count )
			{
				wszEventMessage = g_pVGuiLocalize->Find( endEventToString[iEndEvent] );
			}

			Assert( m_pScaleformUI );
			WITH_SLOT_LOCKED
			{
				if ( m_hReason )
				{
					if ( wszEventMessage != NULL )
					{
						m_hReason->SetTextHTML( wszEventMessage );
					}
					else
					{
						m_hReason->SetTextHTML( "" );										
					}
				}

			}
		}
	}
}

void SFHudWinPanel::SetWinPanelExtraData()
{
	if ( !FlashAPIIsValid() )
		return;
	
	if ( !CSGameRules() )
		return;

	/*
	WIN_EXTRATYPE_FUN = 0,
	WIN_EXTRATYPE_AWARD,
	WIN_EXTRATYPE_RANK,
	WIN_EXTRATYPE_ELO,
	WIN_EXTRATYPE_SEASONRANK,
	*/
	int nExtraPanelType = WIN_EXTRATYPE_NONE;

	//SetWinPanelItemDrops( index:Number, strId:String, PlayerXuid:String )

	//g_PlayerRankManager.PrintRankProgressThisRound();
	int nIdealProgressCatagory = MEDAL_CATEGORY_NONE;

	// ELO will need to be updated before this event happens so we get the right info
	int nEloBracket = -1;
	int nEloDelta = g_PlayerRankManager.GetEloBracketChange( nEloBracket );
	
	const CUtlVector<RankIncreasedEvent_t> &medalRankIncreases = g_PlayerRankManager.GetRankIncreasesThisRound();
	const CUtlVector<MedalEarnedEvent_t> &medalsAwarded = g_PlayerRankManager.GetMedalsEarnedThisRound();
	CUtlVector<MedalStatEvent_t> medalStatsAwarded;
	g_PlayerRankManager.GetMedalStatsEarnedThisRound( medalStatsAwarded );
	bool bShowProgress = false;
	bool bHideProgressAndFunFact = false;
	bool bIsWarmup = CSGameRules()->IsWarmupPeriod();

	if ( !bIsWarmup && nEloBracket >= 0 && nEloDelta != 0 && m_hEloPanel )
	{
		// tell the script to set the icon and show it
		// get the text needed and set the text
		const wchar_t *eloString;
		if ( nEloDelta > 0 ) 
			eloString = g_pVGuiLocalize->Find( "#SFUI_WinPanel_elo_up_string" );
		else
			eloString = g_pVGuiLocalize->Find( "#SFUI_WinPanel_elo_down_string" );

		if ( !eloString )
		{
			Warning( "Failed to find localization strings for elo change in win panel\n" );
		}

		WITH_SFVALUEARRAY_SLOT_LOCKED( data, 2 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, nEloBracket );
			m_pScaleformUI->ValueArray_SetElement( data, 1, eloString ? eloString : L"" );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetEloBracketInfo", data, 2 );
		}	

		nExtraPanelType = WIN_EXTRATYPE_ELO;

		// Once we display the change in rank, reset the recorded delta.
		g_PlayerRankManager.ResetRecordedEloBracketChange();
	}	
	else if ( !bIsWarmup && medalRankIncreases.Count() > 0 )
	{
		// static "award" text
		ISFTextObject* hRankPromtText	= g_pScaleformUI->TextObject_MakeTextObjectFromMember( m_hRankPanel, "RankEarned" );	
		ISFTextObject* hRankNameText	= g_pScaleformUI->TextObject_MakeTextObjectFromMember( m_hRankPanel, "RankName" );	

		if ( hRankPromtText && hRankNameText )
		{
			int nTotalRankIncreases = medalRankIncreases.Count();

			//MEDAL_CATEGORY_TEAM_AND_OBJECTIVE = MEDAL_CATEGORY_START,
			//MEDAL_CATEGORY_COMBAT,
			//MEDAL_CATEGORY_WEAPON,
			//MEDAL_CATEGORY_MAP,
			//MEDAL_CATEGORY_ARSENAL,

			int nBestIndex = 0;
			int nHighestRank = 0;
			CUtlVector< int > tieList;

			// find the place where we earned the highest rank
			for ( int i=0; i < nTotalRankIncreases; i++ )
			{
				nHighestRank = g_PlayerRankManager.CalculateRankForCategory( medalRankIncreases[nBestIndex].m_category );
				int nCurRank = g_PlayerRankManager.CalculateRankForCategory( medalRankIncreases[i].m_category );
				int nDelta = nCurRank - nHighestRank;

				if ( medalRankIncreases[i].m_category >= MEDAL_CATEGORY_ACHIEVEMENTS_END )
				{
					nBestIndex = i;
					tieList.RemoveAll();
				}
				else if ( nDelta == 0 )
				{
					// keep track of the ranks of the same value
					nBestIndex = i;
					tieList.AddToTail(i);
				}
				else if ( nDelta > 0 )
				{
					nBestIndex = i;
					tieList.RemoveAll();
				}
			}

			if ( tieList.Count() > 0 )
			{
				// break any ties by picking on randomly
				nBestIndex = tieList[ RandomInt( 0, tieList.Count()-1) ];
			}

			bool bIsCoinLevelUp = false;

			MedalCategory_t nCurrentBestCatagory = medalRankIncreases[nBestIndex].m_category;
			int nCurrentRankForCatagory = 0;
			nCurrentRankForCatagory = g_PlayerRankManager.CalculateRankForCategory( medalRankIncreases[nBestIndex].m_category );

			WITH_SFVALUEARRAY_SLOT_LOCKED( data, 2 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, nCurrentBestCatagory );
				m_pScaleformUI->ValueArray_SetElement( data, 1, nCurrentRankForCatagory );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetRankUpIcon", data, 2 );
			}	

			wchar_t finalAwardText[256];

			// say whether they earned a new rank, 2 new ranks, a new rank in two catagories, etc
			if ( hRankPromtText && ( ( nTotalRankIncreases <= 1 ) || bIsCoinLevelUp ) )
			{
				g_pVGuiLocalize->ConstructString( finalAwardText, sizeof(finalAwardText),
					bIsCoinLevelUp ? "#SFUI_WinPanel_coin_awarded" : "#SFUI_WinPanel_rank_awarded",
					NULL );
			}
			else
			{			
				wchar_t wNum[16];
				V_snwprintf( wNum, ARRAYSIZE( wNum ), L"%i", nTotalRankIncreases );

				int param1 = nTotalRankIncreases;
				wchar_t wAnnounceText[256];
				V_snwprintf( wAnnounceText, ARRAYSIZE( wAnnounceText ), L"%i", param1 );
	
				const wchar_t *awardString = g_pVGuiLocalize->Find( "#SFUI_WinPanel_rank_awarded_multi" );
					
				g_pVGuiLocalize->ConstructString( finalAwardText, sizeof(finalAwardText), awardString , 1, wNum );
			}
				
			// set the name of the new rank that you achieved
			wchar_t finalRankText[256];
			if ( hRankNameText )
			{
				const wchar_t *rankString = g_pVGuiLocalize->Find( "#SFUI_WinPanel_rank_name_string" );
				g_pVGuiLocalize->ConstructString( finalRankText, sizeof(finalRankText), rankString , 2, g_pVGuiLocalize->Find(g_PlayerRankManager.GetMedalCatagoryName(nCurrentBestCatagory)), 
					g_pVGuiLocalize->Find(g_PlayerRankManager.GetMedalCatagoryRankName( bIsCoinLevelUp ? (nCurrentRankForCatagory-1) : nCurrentRankForCatagory )) );

				WITH_SLOT_LOCKED
				{
					hRankNameText->SetTextHTML( finalRankText );
				}
			}

			WITH_SFVALUEARRAY_SLOT_LOCKED( data, 2 )
			{						
				m_pScaleformUI->ValueArray_SetElement( data, 0, finalAwardText );
				m_pScaleformUI->ValueArray_SetElement( data, 1, finalRankText );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetRankUpText", data, 2 );
			}

			if ( nHighestRank < (MEDAL_CATEGORY_COUNT-1) )
				nIdealProgressCatagory = nCurrentBestCatagory;

			SafeReleaseSFTextObject( hRankPromtText );
			SafeReleaseSFTextObject( hRankNameText );
		}

		nExtraPanelType = WIN_EXTRATYPE_RANK;
		bShowProgress = true;
	}
	else if ( !bIsWarmup && medalsAwarded.Count() > 0 )
	{
		WITH_SLOT_LOCKED
		{
			int nMaxMedals = 7;
			for ( int i = 0; i < nMaxMedals; i++ )
			{
				//get the award name for slot i			
				WITH_SFVALUEARRAY( data, 2 )
				{						
					m_pScaleformUI->ValueArray_SetElement( data, 0, i );
					if ( i < medalsAwarded.Count() && medalsAwarded[i].m_pAchievement )
					{
						CBaseAchievement *pAchievement = medalsAwarded[i].m_pAchievement;
						m_pScaleformUI->ValueArray_SetElement( data, 1, pAchievement->GetName() );
					}
					else
					{
						m_pScaleformUI->ValueArray_SetElement( data, 1, "" );
					}
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetWinPanelAwardIcon", data, 2 );
				}				
			}
		}

		nExtraPanelType = WIN_EXTRATYPE_AWARD;
		bShowProgress = true;
	}
	else if ( !bIsWarmup && medalStatsAwarded.Count() > 0 )
	{
		MedalStatEvent_t *pBestStat = NULL;
		float flHighestCompletionPct = -1.0f;
		FOR_EACH_VEC( medalStatsAwarded, i )
		{
			MedalStatEvent_t&stat = medalStatsAwarded[i];	
			float pct = (float)(stat.m_pAchievement->GetCount()) / (float)(stat.m_pAchievement->GetGoal());
			if ( pct > flHighestCompletionPct )
			{
				flHighestCompletionPct = pct;
				pBestStat = &medalStatsAwarded[i];
			}
		}
		if ( pBestStat )
		{
			const char* pszLocToken = GetLocTokenForStatId( pBestStat->m_StatType );
			if ( pszLocToken && pBestStat->m_pAchievement )
			{
				WITH_SFVALUEARRAY_SLOT_LOCKED( data, 6 )
				{						
					m_pScaleformUI->ValueArray_SetElement( data, 0, pBestStat->m_pAchievement->GetName() );
					m_pScaleformUI->ValueArray_SetElement( data, 1, pBestStat->m_pAchievement->GetCount() );
					m_pScaleformUI->ValueArray_SetElement( data, 2, pBestStat->m_pAchievement->GetGoal() );
					const StatsCollection_t roundStats = g_CSClientGameStats.GetRoundStats(0);
					m_pScaleformUI->ValueArray_SetElement( data, 3, roundStats[pBestStat->m_StatType] );
					m_pScaleformUI->ValueArray_SetElement( data, 4, pBestStat->m_category );

					// Progress text for this stat based medal 
					const wchar_t *progString = g_pVGuiLocalize->Find( pszLocToken );
					wchar_t finalProgressText[256];
					wchar_t count[8], goal[8];
					V_snwprintf( count, ARRAYSIZE( count ), L"%i", pBestStat->m_pAchievement->GetCount() );
					V_snwprintf( goal, ARRAYSIZE( goal ), L"%i", pBestStat->m_pAchievement->GetGoal() );
					g_pVGuiLocalize->ConstructString( finalProgressText, sizeof(finalProgressText), progString, 3, count, goal, ACHIEVEMENT_LOCALIZED_NAME( pBestStat->m_pAchievement ) );
					
					m_pScaleformUI->ValueArray_SetElement( data, 5, finalProgressText );
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetWinPanelStatProgress", data, 6 );

					bHideProgressAndFunFact = true;
				}				
			}
		}
	}
	else if ( !bIsWarmup && CSGameRules() && CSGameRules()->IsPlayingGunGame() )
	{
		const char *szGrenName = NULL;
		C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pPlayer )
		{
			int nextWeaponID = CSGameRules()->GetNextGunGameWeapon( pPlayer->GetPlayerGunGameWeaponIndex(), pPlayer->GetTeamNumber() );

			const char *pchClassName = "unknown";
			const char *pchPrintName = "unknown";

			const CEconItemDefinition *pDef = GetItemSchema()->GetItemDefinition( nextWeaponID );
			if ( pDef && pDef->GetDefinitionIndex() != 0 )
			{
				pchClassName = pDef->GetDefinitionName();
				pchPrintName = pDef->GetItemBaseName();
			}
			else
			{
				const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( (CSWeaponID)nextWeaponID );
				if ( pWeaponInfo )
				{
					pchClassName = pWeaponInfo->szClassName;
					pchPrintName = pWeaponInfo->szPrintName;
				}
			}

			int nKillValue = pPlayer->GetNumGunGameTRKillPoints();

			int nCurIndex = (float)pPlayer->GetPlayerGunGameWeaponIndex();
			int nMaxIndex = CSGameRules()->GetNumProgressiveGunGameWeapons( pPlayer->GetTeamNumber() );
			if ( nCurIndex != nMaxIndex && nKillValue > 0 )
			{
				int nBonusGrenade = CSGameRules()->GetGunGameTRBonusGrenade( pPlayer );
				if ( nBonusGrenade == WEAPON_MOLOTOV || nBonusGrenade == WEAPON_INCGRENADE )
				{
					if ( pPlayer->GetTeamNumber() == TEAM_CT )
					{
						szGrenName = "weapon_incgrenade";
					}
					else
					{
						szGrenName = "weapon_molotov";
					}
				}
				else if ( nBonusGrenade == WEAPON_FLASHBANG )
				{
					szGrenName = "weapon_flashbang";
				}
				else if ( nBonusGrenade == WEAPON_HEGRENADE )
				{
					szGrenName = "weapon_hegrenade";
				}

				wchar_t *titleString = NULL;
				if ( CSGameRules()->IsPlayingGunGameTRBomb() )
				{
					titleString = g_pVGuiLocalize->Find( "#SFUI_WS_GG_YourNextWeaponIs" );
				}
				else
				{
					titleString = g_pVGuiLocalize->Find( "#SFUI_WS_GG_NextWep" );
				}

				WITH_SFVALUEARRAY_SLOT_LOCKED( data, 4 )
				{	
					// Scaleform cannot handle NULL as a wide string name.  Make sure it always gets a valid string.
					wchar_t *weaponName = L"";
					wchar_t *newWeaponName = g_pVGuiLocalize->Find( pchPrintName );
					if ( newWeaponName )
					{
						weaponName = newWeaponName;
					}

					m_pScaleformUI->ValueArray_SetElement( data, 0, titleString );
					m_pScaleformUI->ValueArray_SetElement( data, 1, weaponName );
					m_pScaleformUI->ValueArray_SetElement( data, 2, pchClassName );
					m_pScaleformUI->ValueArray_SetElement( data, 3, szGrenName ? szGrenName : "");
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetGunGamePanelData", data, 4 );
				}

				nExtraPanelType = WIN_EXTRATYPE_GGNEXT;
			}
		}
	}

	// set the progress bar text
	//int nMedalsNeededToRank = 0;
	CUtlVector< int > nProgressCatagories;
	// if one of the other panel above told us to show, we should show
	if ( !bHideProgressAndFunFact )
	{
		if ( bShowProgress )
		{
			if ( nIdealProgressCatagory == -1 )
			{
				for ( int i = 0; i < MEDAL_CATEGORY_ACHIEVEMENTS_END; i++ )
				{
					int nTempRank = g_PlayerRankManager.CalculateRankForCategory( (MedalCategory_t)i );
					if ( nTempRank < (MEDAL_RANK_COUNT-1) )
						nProgressCatagories.AddToTail(i);
				}

				if ( nProgressCatagories.Count() == 0 )
				{
					// we maxed out all of our ranks, congrats!!!!
					bShowProgress = false;
				}
				else
				{
					// we don't have an ideal category to show a hint, so pick one to display
					nIdealProgressCatagory = nProgressCatagories[ RandomInt( 0, nProgressCatagories.Count()-1 ) ];
					nProgressCatagories.RemoveAll();
				}
			}

			if ( bShowProgress )
			{
				int nCurrentRank = g_PlayerRankManager.CalculateRankForCategory( (MedalCategory_t)nIdealProgressCatagory );
				int nMinMedalsNeeded = g_PlayerRankManager.GetMinMedalsForRank( (MedalCategory_t)nIdealProgressCatagory, (MedalRank_t)(MIN(nCurrentRank + 1, (int)(MEDAL_RANK_COUNT-1))) );
				int nMedalsAchieved = g_PlayerRankManager.CountAchievedInCategory( (MedalCategory_t)nIdealProgressCatagory );

				const wchar_t *progString = g_pVGuiLocalize->Find( "#SFUI_WinPanelProg_need_in_catagory" );
				wchar_t wzAwardNum[4];
				_snwprintf( wzAwardNum, ARRAYSIZE(wzAwardNum), L"%d", (nMinMedalsNeeded-nMedalsAchieved) );
				wchar_t finalProgressText[256];
				g_pVGuiLocalize->ConstructString( finalProgressText, sizeof(finalProgressText), progString , 2, wzAwardNum, g_pVGuiLocalize->Find( g_PlayerRankManager.GetMedalCatagoryName((MedalCategory_t)nIdealProgressCatagory) ) );

				SetProgressBarText( (nMinMedalsNeeded-nMedalsAchieved), finalProgressText );
				//Msg( "MinMedalsNeeded = %d, MedalsAchieved = %d, NeededForNext = %d\n", nMinMedalsNeeded, nMedalsAchieved, (nMinMedalsNeeded-nMedalsAchieved) );
			}
		}

		if ( bIsWarmup )
		{
			SetFunFactLabel( L"" );
		}
		// otherwise we show a FUN FACT!
		else if ( !bShowProgress )
		{
			/*
			"show_timer_defend"	"bool"
			"show_timer_attack"	"bool"
			"timer_time"		"int"

			"final_event"		"byte"		// 0 - no event, 1 - bomb exploded, 2 - flag capped, 3 - timer expired

			"funfact_type"		"byte"		//WINPANEL_FUNFACT in cs_shareddef.h
			"funfact_player"	"byte"
			"funfact_data1"		"long"
			"funfact_data2"		"long"
			"funfact_data3"		"long"
			*/

			if ( m_nFunFactPlayer == GetLocalPlayerIndex() )
			{
				CEG_PROTECT_VIRTUAL_FUNCTION( SFHudWinPanel_FireGameEvent );
			}

			// Final Fun Fact
			SetFunFactLabel( L"" );

			const char *pFunFact = STRING( m_nFunfactToken );
			if ( pFunFact && V_strlen( pFunFact ) > 0 )
			{
				wchar_t funFactText[256];
				wchar_t playerText[MAX_DECORATED_PLAYER_NAME_LENGTH];
				wchar_t dataText1[8], dataText2[8], dataText3[8];
				if ( m_nFunFactPlayer >= 1 && m_nFunFactPlayer <= MAX_PLAYERS )
				{
					playerText[0] = L'\0';

					C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );
					if ( !cs_PR )
						return;

					cs_PR->GetDecoratedPlayerName( m_nFunFactPlayer, playerText, sizeof( playerText ), k_EDecoratedPlayerNameFlag_Simple );

					if ( playerText[0] == L'\0' )
					{
						V_snwprintf( playerText, ARRAYSIZE( playerText ), PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#winpanel_former_player" ) );
					}
				}
				else
				{
					V_snwprintf( playerText, ARRAYSIZE( playerText ), L"" );
				}
				V_snwprintf( dataText1, ARRAYSIZE( dataText1 ), L"%i", m_nFunFactParam1 );
				V_snwprintf( dataText2, ARRAYSIZE( dataText2 ), L"%i", m_nFunFactParam2 );
				V_snwprintf( dataText3, ARRAYSIZE( dataText3 ), L"%i", m_nFunFactParam3 );
				
				// Vararg support on consoles isn't complete, so use the keyvalue version of ConstructString instead so
				//	we can support formatting like "%s2 has fired %s1 shots", etc.
				KeyValues *pkvFunFactVariables = new KeyValues( "variables" );
				KeyValues::AutoDelete autodelete( pkvFunFactVariables );
				pkvFunFactVariables->SetWString( "s1", playerText );
				pkvFunFactVariables->SetWString( "s2", dataText1 );
				pkvFunFactVariables->SetWString( "s3", dataText2 );
				pkvFunFactVariables->SetWString( "s4", dataText3 );

				g_pVGuiLocalize->ConstructString( funFactText, sizeof(funFactText), pFunFact, pkvFunFactVariables );

				SetFunFactLabel( funFactText );

				if( g_pVGuiLocalize->Find( pFunFact ) == NULL )
				{
					Warning( "No valid fun fact string for %s\n", pFunFact );
				}
			}
		}
	}
	
	ShowWinExtraDataPanel( nExtraPanelType );
}

void SFHudWinPanel::SetProgressBarText( int nAmount, const wchar *wszDescText )
{
	if ( !FlashAPIIsValid() )
		return;

	wchar_t wNum[16];
	V_snwprintf( wNum, ARRAYSIZE( wNum ), L"%i", nAmount );

	WITH_SFVALUEARRAY_SLOT_LOCKED( data, 2 )
	{						
		m_pScaleformUI->ValueArray_SetElement( data, 0, wNum );
		m_pScaleformUI->ValueArray_SetElement( data, 1, wszDescText );
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetProgressText", data, 2 );
	}				
}


void SFHudWinPanel::SetFunFactLabel( const wchar *szFunFact )
{
	if ( !FlashAPIIsValid() )
		return;

	WITH_SFVALUEARRAY_SLOT_LOCKED( data, 1 )
	{						
		m_pScaleformUI->ValueArray_SetElement( data, 0, szFunFact );
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetFunFactText", data, 1 );
	}
}

void SFHudWinPanel::SetMVP( C_CSPlayer* pPlayer, CSMvpReason_t reason, int32 nMusicKitMVPs /* = 0 */ )
{
	if ( pPlayer )
		m_iMVP = pPlayer->entindex();

	if ( FlashAPIIsValid() )
	{
		if ( g_PR && pPlayer )
		{
			// First set the text to the name of the player.
			wchar_t wszPlayerName[MAX_DECORATED_PLAYER_NAME_LENGTH];

			( ( C_CS_PlayerResource* ) g_PR )->GetDecoratedPlayerName( pPlayer->entindex(), wszPlayerName, sizeof( wszPlayerName ), EDecoratedPlayerNameFlag_t( k_EDecoratedPlayerNameFlag_Simple | k_EDecoratedPlayerNameFlag_DontUseNameOfControllingPlayer ) );

			//
			// Construct the reason text.
			//

			const char* mvpReasonToken = NULL;
			switch ( reason )
			{
			case CSMVP_ELIMINATION:
				mvpReasonToken = "winpanel_mvp_award_kills";
				break;
			case CSMVP_BOMBPLANT:
				mvpReasonToken = "winpanel_mvp_award_bombplant";
				break;
			case CSMVP_BOMBDEFUSE:
				mvpReasonToken = "winpanel_mvp_award_bombdefuse";
				break;
			case CSMVP_HOSTAGERESCUE:
				mvpReasonToken = "winpanel_mvp_award_rescue";
				break;
			case CSMVP_GUNGAMEWINNER:
				mvpReasonToken = "winpanel_mvp_award_gungame";
				break;
			default:
				mvpReasonToken = "winpanel_mvp_award";
				break;
			}

			wchar_t *pReason = g_pVGuiLocalize->Find( mvpReasonToken );
			if ( !pReason )
			{
				pReason = L"%s1";
			}

			wchar_t wszBuf[256];
			g_pVGuiLocalize->ConstructString( wszBuf, sizeof( wszBuf ), pReason, 1, wszPlayerName );

			// Get the player xuid.
			char xuidText[255];
			g_PR->FillXuidText( pPlayer->entindex(), xuidText, sizeof( xuidText ) );

			const int nParamCount = 5;
			WITH_SFVALUEARRAY_SLOT_LOCKED( data, nParamCount )
			{			
				if ( m_hMVP )
				{
					m_hMVP->SetTextHTML( wszBuf );
				}
	
				int nParamNum = 0;
				m_pScaleformUI->ValueArray_SetElement( data, nParamNum++, xuidText );
				m_pScaleformUI->ValueArray_SetElement( data, nParamNum++, pPlayer->GetPlayerName() );
				m_pScaleformUI->ValueArray_SetElement( data, nParamNum++, pPlayer->GetTeamNumber() );
				m_pScaleformUI->ValueArray_SetElement( data, nParamNum++, pPlayer->IsLocalPlayer() );
				m_pScaleformUI->ValueArray_SetElement( data, nParamNum++, nMusicKitMVPs );
				Assert( nParamNum == nParamCount );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowAvatar", data, nParamCount );
			}

			if ( reason == CSMVP_GUNGAMEWINNER )
			{
				WITH_SFVALUEARRAY_SLOT_LOCKED( data, 5 )
				{			
					int nSecond = -1;
					int nThird = -1;

					SFHudTeamCounter* pTeamCounter = GET_HUDELEMENT( SFHudTeamCounter );
					if ( pTeamCounter )
					{
						for ( int i = 0 ; i < MAX_PLAYERS ; ++i)
						{
							int indx = pTeamCounter->GetPlayerEntIndexInSlot( i );
							if ( pPlayer->entindex() != indx )
							{
								if ( nSecond == -1 )
								{
									nSecond = indx;
								}
								else
								{
									nThird = indx;
									break;
								}				
							}
						}
					}

					char xuidText1[255];
					g_PR->FillXuidText( nSecond, xuidText1, sizeof( xuidText1 ) );
					char xuidText2[255];
					g_PR->FillXuidText( nThird, xuidText2, sizeof( xuidText2 ) );

					CBasePlayer* pBasePlayer1 = UTIL_PlayerByIndex( nSecond );
					C_CSPlayer* pPlayer1 = ToCSPlayer( pBasePlayer1 );
					CBasePlayer* pBasePlayer2 = UTIL_PlayerByIndex( nThird );
					C_CSPlayer* pPlayer2 = ToCSPlayer( pBasePlayer2 );

					m_pScaleformUI->ValueArray_SetElement( data, 0, wszPlayerName );
					m_pScaleformUI->ValueArray_SetElement( data, 1, xuidText1 );
					m_pScaleformUI->ValueArray_SetElement( data, 2, xuidText2  );
					m_pScaleformUI->ValueArray_SetElement( data, 3, pPlayer1 ? pPlayer1->GetTeamNumber() : -1 );
					m_pScaleformUI->ValueArray_SetElement( data, 4, pPlayer2 ? pPlayer2->GetTeamNumber() : -1 );
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowGGRunnerUpAvatars", data, 5 );
				}
			}
		}
		else
		{
			WITH_SLOT_LOCKED
			{				
				// Hide the MVP text.
				if ( m_hMVP )
				{
					m_hMVP->SetTextHTML( "" );
				}
				
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "HideAvatar", NULL, 0 );				
			}
		}
	}
}

void SFHudWinPanel::ApplyYOffset( int nOffset )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SFVALUEARRAY_SLOT_LOCKED( data, 1 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, nOffset );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setYOffset", data, 1 );
		}
	}
}

void SFHudWinPanel::ShowTeamWinPanel( int result, const char* winnerText )
{
	bool bOkToDraw = true;
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return;

	if ( pLocalPlayer->GetTeamNumber() == TEAM_UNASSIGNED && !pLocalPlayer->IsHLTV() )
	{
		bOkToDraw = false;
	}

	bool bShowTeamLogo = false;

	char szLogoString[64];
	szLogoString[0] = '\0';
	wchar_t wszWinnerText[1024];
	wszWinnerText[0] = L'\0';

	C_Team *pTeam = GetGlobalTeam( result );
	if ( pTeam && !StringIsEmpty( pTeam->Get_LogoImageString() ) )
	{
		bShowTeamLogo = true;
		V_snprintf( szLogoString, ARRAYSIZE( szLogoString ), TEAM_LOGO_IMG_STRING, pTeam->Get_LogoImageString() );
	}

	wchar_t wszName[512];

	if ( m_hWinPanelParent && m_hWinner && FlashAPIIsValid() && m_bActive && bOkToDraw )
	{
		if ( CSGameRules() && CSGameRules()->IsPlayingCoopMission() )
		{
			if ( result == TEAM_CT )
				g_pVGuiLocalize->ConstructString( wszName, sizeof( wszName ), g_pVGuiLocalize->Find( "#SFUI_WinPanel_Coop_Mission_Win" ), nullptr );
			else
				g_pVGuiLocalize->ConstructString( wszName, sizeof( wszName ), g_pVGuiLocalize->Find( "#SFUI_WinPanel_Coop_Mission_Lose" ), nullptr );
			
			g_pScaleformUI->MakeStringSafe( wszName, wszWinnerText, sizeof( wszWinnerText ) );
		}
		else if ( ( pTeam != NULL ) && !StringIsEmpty( pTeam->Get_ClanName() ) )
		{
 			wchar_t wszTemp[MAX_TEAM_NAME_LENGTH];
 			g_pVGuiLocalize->ConvertANSIToUnicode( pTeam->Get_ClanName(), wszTemp, sizeof( wszTemp ) );
// 			const wchar_t *winString = g_pVGuiLocalize->Find( "#SFUI_WinPanel_Team_Win_Team" );
			g_pVGuiLocalize->ConstructString( wszName, sizeof( wszName ), g_pVGuiLocalize->Find( "#SFUI_WinPanel_Team_Win_Team" ), 1, wszTemp );

			// we have a custom team name, convert to wide
			//
			// now make the team name string safe
			g_pScaleformUI->MakeStringSafe( wszName, wszWinnerText, sizeof( wszWinnerText ) );		
		}
		else
		{
			g_pVGuiLocalize->ConstructString( wszName, sizeof( wszName ), g_pVGuiLocalize->Find( winnerText ), nullptr );
			g_pScaleformUI->MakeStringSafe( wszName, wszWinnerText, sizeof( wszWinnerText ) );
		}

		WITH_SFVALUEARRAY_SLOT_LOCKED( data, 2 )
		{
			m_hWinner->SetTextHTML( wszWinnerText );					
			
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, result );
				m_pScaleformUI->ValueArray_SetElement( data, 1, bShowTeamLogo ? szLogoString : "" );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showTeamWin", data, 2 );
			}
		}

		if ( GetViewPortInterface() && !pLocalPlayer->IsSpectator() && !engine->IsHLTV() )
		{
			GetViewPortInterface()->ShowPanel( PANEL_ALL, false );
		}

		m_bVisible = true;
	}
}

void SFHudWinPanel::ShowGunGameWinPanel( void /*int nWinner, int nSecond, int nThird*/ )
{
	if ( m_hWinPanelParent && m_hWinner && FlashAPIIsValid() && m_bActive )
	{
		WITH_SLOT_LOCKED
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showArsenalWin", NULL, 0 );
		}

		if ( GetViewPortInterface() )
		{
			GetViewPortInterface()->ShowPanel( PANEL_ALL, false );
		}

		m_bVisible = true;
	}
}

void SFHudWinPanel::ShowWinExtraDataPanel( int nExtraPanelType )
{
	if ( m_hWinPanelParent && m_hWinner && m_FlashAPI && m_bActive )
	{
		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
		if( pLocalPlayer )
		{
			C_RecipientFilter filter;
			filter.AddRecipient( pLocalPlayer );
			C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "Player.InfoPanel" );
		}
		WITH_SLOT_LOCKED
		{		
			WITH_SFVALUEARRAY( data, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, nExtraPanelType );	
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showTeamWinDataPanel", data, 1 );
			}
		}
	}
}

bool SFHudWinPanel::IsVisible( void )
{
	return m_bVisible;
}

void SFHudWinPanel::Hide( void )
{
	if ( m_FlashAPI && m_pScaleformUI)
	{
		WITH_SLOT_LOCKED
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hide", NULL, 0 );
		}

		m_bVisible = false;
	}	
}

void SFHudWinPanel::DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "RefreshAvatarImage", NULL, 0 );
		}
	}
}


#endif // INCLUDE_SCALEFORM
