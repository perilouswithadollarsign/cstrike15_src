//====== Copyright  Valve Corporation, All rights reserved. =================
//
// Purpose:
//
//=============================================================================
#include "cbase.h"
#include "cs_player_rank_mgr.h"
#include "achievementmgr.h"
#include "achievements_cs.h"
#include "cs_client_gamestats.h"
#include "keyvalues.h"
#include "c_playerresource.h"
#include "cs_hud_chat.h"

#ifdef _PS3
#include "fmtstr.h"
#endif

#if defined ( _GAMECONSOLE )
#include "usermessages.h"
#include "inputsystem/iinputsystem.h"
#endif


ConVar cl_player_rank_debug( "cl_player_rank_debug", "0", FCVAR_DEVELOPMENTONLY );
ConVar cl_force_progress_notice_every_change( "cl_force_progress_notice_every_change", "0", FCVAR_DEVELOPMENTONLY );
ConVar cl_medal_progress_shown_fraction( "cl_medal_progress_shown_fraction", "5", FCVAR_DEVELOPMENTONLY, "Show progress on the win panel every GOAL/X increments for stat based achievements.", true, 1, true, 10 );


CPlayerRankManager g_PlayerRankManager;

MedalCategory_t GetAchievementCategory( int id );

static CFmtStr s_MedalCategoryNameActiveSeason( "SFUI_MedalCategory_Season%d_CAPHTML", MEDAL_SEASON_ACCESS_VALUE + 1 );
const char *s_MedalCategoryNames[] = 
{
	"SFUI_MedalCategory_TeamAndObjective_CAPHTML",
	"SFUI_MedalCategory_Combat_CAPHTML",
	"SFUI_MedalCategory_Weapon_CAPHTML",
	"SFUI_MedalCategory_Map_CAPHTML",
	"SFUI_MedalCategory_GunGame_CAPHTML",
	s_MedalCategoryNameActiveSeason.Access()
};

const char *s_MedalCategoryRankNames[] = 
{
	"SFUI_Medal_RankName_0",
	"SFUI_Medal_RankName_1",
	"SFUI_Medal_RankName_2",
	"SFUI_Medal_RankName_3"
};

CPlayerRankManager::CPlayerRankManager()
	: m_bMedalCategoriesBuilt( false ),
	  m_iEloBracketDelta( 0 ),
	  m_iNewEloBracket( -1 )
{
	COMPILE_TIME_ASSERT( ARRAYSIZE( s_MedalCategoryRankNames ) == MEDAL_RANK_COUNT );
	COMPILE_TIME_ASSERT( ARRAYSIZE( s_MedalCategoryNames ) == MEDAL_CATEGORY_COUNT );

	Q_memset( m_rank, 0, sizeof( m_rank ) );
}

CPlayerRankManager::~CPlayerRankManager()
{
}

bool CPlayerRankManager::Init()
{
	ListenForGameEvent( "achievement_earned_local" );
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "achievement_info_loaded" );
	ListenForGameEvent( "seasoncoin_levelup" );
	
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );
	return true;
}

void CPlayerRankManager::Shutdown()
{
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );

	StopListeningForAllEvents();
}

void CPlayerRankManager::LevelInitPostEntity()
{
	// Intention here is to send this once we finish connecting to a server.
	SendRankDataToServer();
	ResetRecordedEloBracketChange();
}

void CPlayerRankManager::FireGameEvent( IGameEvent *event )
{
	const char *name = event->GetName();
	if ( 0 == Q_strcmp( name, "round_start" ) )
	{
		OnRoundStart();
	}
	else if ( 0 == Q_strcmp( name, "achievement_earned_local" ) )
	{
		OnAchievementEarned( event->GetInt( "achievement", CSInvalidAchievement ) );
	}
	else if ( 0 == Q_strcmp( name, "achievement_info_loaded" ) )
	{
		// After we retrieve achievement info from steam or titledata, 
		// get lists of our achievements in each category and determine the 'rank' for the player.
		BuildMedalCategories();
	}
	else if ( 0 == Q_strcmp( name, "seasoncoin_levelup" ) )
	{
		int iPlayerIndex = event->GetInt( "player" );
		int iCategory = event->GetInt( "category" );
		int iRank = event->GetInt( "rank" );
		C_BasePlayer *pPlayer = UTIL_PlayerByIndex( iPlayerIndex );
		if ( C_BasePlayer::GetLocalPlayer() && pPlayer && C_BasePlayer::GetLocalPlayer() == pPlayer )
		{
			OnSeasonCoinLeveledUp( iCategory, iRank );
		}
	}
}

// Use CommandKeyValues to send our rank data to the server so it can
// network to other clients and display in the scoreboard.
void CPlayerRankManager::SendRankDataToServer( void )
{
#if 0
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer || !engine->IsConnected() )
		return;

	KeyValues *kv = new KeyValues("player_medal_ranking");
	for ( int i = MEDAL_CATEGORY_START; i < MEDAL_CATEGORY_COUNT; ++i )
	{
		kv->SetInt( CFmtStr("rank%d",i), m_rank[i] );
	}

	// Base_CmdKeyValues handles the kv deletion.
	engine->ServerCmdKeyValues( kv );
#endif
}

// Once we have achievements loaded, sort them into category lists and build our internal
// category info structures.
void CPlayerRankManager::BuildMedalCategories( void )
{
	// Note: we cut splitscreen, so achievements should all be at idx 0.
	CUtlMap<int, CBaseAchievement *> &achievements = g_AchievementMgrCS.GetAchievements( 0 );

	// clear these out before we rebuild them
	for ( int i=0; i<MEDAL_CATEGORY_COUNT; i++)
	{
		m_medalCategoryList[i].RemoveAll();
	}

	m_StatBasedAchievements.RemoveAll();

	if ( achievements.Count() == 0  )
	{
		//TODO: need disable this whole system when this happens.
		Warning( "PlayerRankManager: No achievements loaded!\n" );
		m_bMedalCategoriesBuilt = false;
		return;
	}

	int count[MEDAL_CATEGORY_COUNT];
	memset( count, 0, sizeof( count ) );
	FOR_EACH_MAP( achievements, idx )
	{
		CBaseAchievement *pAchievement = achievements[idx];
		MedalCategory_t c = GetAchievementCategory( pAchievement->GetAchievementID() );
		if ( c != MEDAL_CATEGORY_NONE )
		{
			count[c]++;
			m_medalCategoryList[c].AddToTail( pAchievement );

			// Keep a separate list of achievements withs stat goals so we can 
			// check for relevant stat changes then report those for display.
			if ( !pAchievement->IsAchieved() && pAchievement->GetGoal() > 1 )	
			{
				CAchievement_StatGoal *pStatGoal = dynamic_cast<CAchievement_StatGoal*>(pAchievement);
				Assert( pStatGoal );
				if ( pStatGoal )
				{
					m_StatBasedAchievements.AddToTail( pStatGoal );	
				}
			}
		}
	}
	
	// This may be temp... First guess at how to distribute the rank requirements
	// This also assumes the medal rank enum never changes...
	for ( int c = MEDAL_CATEGORY_START; c < MEDAL_CATEGORY_COUNT; ++c )
	{
		m_categoryInfo[c].m_totalInCategory = count[c];
		m_categoryInfo[c].m_minCountForRank[MEDAL_RANK_NONE]		= 0;
		m_categoryInfo[c].m_minCountForRank[MEDAL_RANK_BRONZE]		= (int)((float)(count[c])/3.0f); // ~1/3rd for bronze;
		m_categoryInfo[c].m_minCountForRank[MEDAL_RANK_SILVER]		= (int)(2.0f*(float)(count[c])/3.0f); // ~2/3rd for silver;
		m_categoryInfo[c].m_minCountForRank[MEDAL_RANK_GOLD]		= count[c]; // all for gold
	}

#if defined ( DEBUG )
	int dbgCatCount = 0;
	FOR_EACH_MAP( achievements, idx )
	{
		CBaseAchievement *pAchievement = achievements[idx];
		if (  MEDAL_CATEGORY_NONE != GetAchievementCategory( pAchievement->GetAchievementID() ) )
		{
			dbgCatCount++;
		}
	}
	// If you implement a new achievement, give it a category. 
	Assert( dbgCatCount == g_AchievementMgrCS.GetAchievementCount() );
#endif

	m_bMedalCategoriesBuilt = true;

	RecalculateRanks();
}

void CPlayerRankManager::RecalculateRanks( void )
{
	for ( int j = MEDAL_CATEGORY_START; j < MEDAL_CATEGORY_COUNT; ++j )
	{
		MedalRank_t newRank = CalculateRankForCategory( (MedalCategory_t)j );
		if ( newRank > m_rank[j] )
			m_rank[j] = newRank;
	}
}

MedalRank_t CPlayerRankManager::CalculateRankForCategory( MedalCategory_t category ) const
{
	if ( m_bMedalCategoriesBuilt == false )
		return MEDAL_RANK_NONE;

	int count = CountAchievedInCategory( category );
	const MedalCategoryInfo_t& info = m_categoryInfo[category];
	int rank = MEDAL_RANK_NONE;
	for ( int i = 0; i < MEDAL_RANK_COUNT; i++ )
	{
		// increase rank until we no longer meet the reqirements for the rank
		if ( count >= info.m_minCountForRank[i] )
			rank = i;
	}

	if ( cl_player_rank_debug.GetBool() )
		Msg( "PlayerRankManager: Calculating rank for category %d: %d (%d achieved, %d needed)\n", category, rank, count, info.m_minCountForRank[rank] );

	return (MedalRank_t)rank;
}

int CPlayerRankManager::CountAchievedInCategory( MedalCategory_t category ) const
{
	Assert( category >= MEDAL_CATEGORY_START && category < MEDAL_CATEGORY_COUNT );
	if( category < MEDAL_CATEGORY_START || category >= MEDAL_CATEGORY_COUNT )
		return 0;

	int achievedCount = 0;
	FOR_EACH_VEC( m_medalCategoryList[category], iter )
	{
		CBaseAchievement *pAchievement = m_medalCategoryList[category][iter];
		if ( pAchievement->IsAchieved() )
			achievedCount++;
	}
	return achievedCount;
}


void CPlayerRankManager::OnRoundStart()
{
	if ( cl_player_rank_debug.GetBool() )
		Msg( "PlayerRankManager: clearing all rank progress events at round start\n" );

	m_RankIncreases.RemoveAll();
	m_MedalsEarned.RemoveAll();
}

void CPlayerRankManager::OnAchievementEarned( int achievementId )
{
	CBaseAchievement *pAchievement = g_AchievementMgrCS.GetAchievementByID( achievementId, 0 );
	MedalCategory_t category = GetAchievementCategory( achievementId );
	if ( !pAchievement || category == MEDAL_CATEGORY_NONE )
		return;

	if ( cl_player_rank_debug.GetBool() )
		Msg( "PlayerRankManager: Adding achievement event '%s'\n", pAchievement->GetName() );

	MedalEarnedEvent_t event( pAchievement, category );
	m_MedalsEarned.AddToTail( event );

	MedalRank_t oldRank = m_rank[category];
	MedalRank_t newRank = CalculateRankForCategory( category );
	if ( newRank > oldRank )
	{
		if ( engine->IsConnected() )
		{
			RankIncreasedEvent_t event( pAchievement, category );	
			m_RankIncreases.AddToTail( event );
		}

		m_rank[category] = newRank;

		SendRankDataToServer();

		if ( cl_player_rank_debug.GetBool() )
			Msg( "PlayerRankManager: Increasing medal rank category %d to %d\n", category, m_rank[category] );
	}
}

void CPlayerRankManager::OnSeasonCoinLeveledUp( int iCategory, int iRank )
{
	// figure out which coin we leveled up
	if ( iCategory <= MEDAL_CATEGORY_NONE )
	{
		for ( int i = MEDAL_CATEGORY_SEASON_COIN; i < MEDAL_CATEGORY_COUNT; i++ )
		{
			CheckCategoryRankLevelUp( i, -1 );
		}
	}
	else
	{
		CheckCategoryRankLevelUp( iCategory, iRank );
	}
}

bool CPlayerRankManager::CheckCategoryRankLevelUp( int iCategory, int iRank )
{
	if ( iCategory != MEDAL_CATEGORY_SEASON_COIN )
		return false;

	bool bLeveledUp = false;

	MedalCategory_t category = (MedalCategory_t)iCategory;

	MedalRank_t oldRank = m_rank[category];
	MedalRank_t newRank = (iRank > -1) ? (MedalRank_t)iRank : CalculateRankForCategory( category );
	if ( newRank > oldRank )
	{
		if ( engine->IsConnected() && oldRank > MEDAL_RANK_NONE )
		{
			RankIncreasedEvent_t event( NULL, category );	
			m_RankIncreases.AddToTail( event );
		}

		m_rank[category] = newRank;

		SendRankDataToServer();

		if ( cl_player_rank_debug.GetBool() )
			Msg( "PlayerRankManager: Increasing medal rank category %d to %d\n", category, m_rank[category] );

		bLeveledUp = true;
	}

	return bLeveledUp;
}

int CPlayerRankManager::GetTotalMedalsInCategory( MedalCategory_t category ) const
{
	Assert( category >= MEDAL_CATEGORY_START && category < MEDAL_CATEGORY_COUNT );
	if( category < MEDAL_CATEGORY_START || category >= MEDAL_CATEGORY_COUNT )
		return 0;

	return m_categoryInfo[category].m_totalInCategory;
}

int CPlayerRankManager::GetMinMedalsForRank( MedalCategory_t category, MedalRank_t rank ) const
{
	Assert( category >= MEDAL_CATEGORY_START && category < MEDAL_CATEGORY_COUNT );
	if( category < MEDAL_CATEGORY_START || category >= MEDAL_CATEGORY_COUNT )
		return 0;

	Assert( rank >= MEDAL_RANK_NONE && rank < MEDAL_RANK_COUNT );
	if( rank < MEDAL_RANK_NONE || rank >= MEDAL_RANK_COUNT )
		return 0;

	return m_categoryInfo[category].m_minCountForRank[rank];
}

void CPlayerRankManager::GetMedalStatsEarnedThisRound( CUtlVector<MedalStatEvent_t>& outVec ) const
{
	const StatsCollection_t &roundStats = g_CSClientGameStats.GetRoundStats(0);

	FOR_EACH_VEC( m_StatBasedAchievements, iter )
	{
		CAchievement_StatGoal *pStatGoal = m_StatBasedAchievements[iter];
		if ( roundStats[pStatGoal->GetStatId()] > 0 && !pStatGoal->IsAchieved() )
		{
			MedalCategory_t cat = GetAchievementCategory( pStatGoal->GetAchievementID() );
			if ( cat == MEDAL_CATEGORY_NONE )
			{
				Assert( 0 );
				continue;
			}

			// Hackish: Logic for culling which stat goal achievements is going here.
			// Current idea is any achievemnt that just reached or crossed a 20% border of the total
			// gets sent to UI code for possible display.
			int showProgresCount = MAX( 1, pStatGoal->GetGoal() / cl_medal_progress_shown_fraction.GetInt() ); // Show a progress notification in the win panel every X stats.
			int cur = pStatGoal->GetCount();
			int prev = cur - roundStats[pStatGoal->GetStatId()];
			
			bool bCrossedBorder = false;
			while ( cur != prev )
			{
				if ( cur%showProgresCount == 0 )
				{
					bCrossedBorder = true;
					break;
				}
				cur--;
			}

			if ( cl_force_progress_notice_every_change.GetBool() )
				bCrossedBorder = true;

			if ( bCrossedBorder )
			{
				MedalStatEvent_t event( pStatGoal, cat, pStatGoal->GetStatId() );
				outVec.AddToTail( event );

				if ( cl_player_rank_debug.GetBool() )
					Msg( "PlayerRankManager: Adding stat progress for achievement '%s' in stat %d\n", pStatGoal->GetName(), pStatGoal->GetStatId() );
			}
		}
	}
}

const CUtlVector<RankIncreasedEvent_t>& CPlayerRankManager::GetRankIncreasesThisRound( void ) const
{
	return m_RankIncreases;
}

const CUtlVector<MedalEarnedEvent_t>& CPlayerRankManager::GetMedalsEarnedThisRound( void ) const
{
	return m_MedalsEarned;
}

const char *CPlayerRankManager::GetMedalCatagoryName( MedalCategory_t category )
{
	Assert( category >= 0 && category < ARRAYSIZE( s_MedalCategoryNames ) );

	return s_MedalCategoryNames[category];
}

const char *CPlayerRankManager::GetMedalCatagoryRankName( int nRank )
{
	Assert( nRank >= 0 && nRank < ARRAYSIZE( s_MedalCategoryRankNames ) );

	return s_MedalCategoryRankNames[nRank];
}

void CPlayerRankManager::PrintRankProgressThisRound() const
{
	Msg( "Rank Increases:\n" );
	FOR_EACH_VEC( m_RankIncreases, iter )
	{
		Msg( "PlayerRankManager: Rank increase because of medal '%s' in category %d\n", m_RankIncreases[iter].m_pAchievement->GetName(), m_RankIncreases[iter].m_category );
	}
	Msg( "Current Ranks: " );
	for ( int i = MEDAL_CATEGORY_START; i < MEDAL_CATEGORY_COUNT; ++i )
	{
		Msg( "%d", m_rank[i] );
	}

	Msg( "\n\n" );

	Msg( "Medals Earned:\n" );
	FOR_EACH_VEC( m_MedalsEarned, iter )
	{
		Msg( "PlayerRankManager: Earned medal '%s' in category %d\n", m_MedalsEarned[iter].m_pAchievement->GetName(), m_MedalsEarned[iter].m_category );
	}

	Msg( "\n\n" );

	Msg( "Medal Progress made:\n" );
	CUtlVector<MedalStatEvent_t> outVec;
	GetMedalStatsEarnedThisRound( outVec );
	FOR_EACH_VEC( outVec, iter )
	{
		Msg( "PlayerRankManager: Earned statid %d towards achievement '%s' in category %d\n", outVec[iter].m_StatType, outVec[iter].m_pAchievement->GetName(), outVec[iter].m_category );
	}
}

void CPlayerRankManager::OnEvent( KeyValues *pEvent )
{
	/* Removed for partner depot */
}

void CPlayerRankManager::NoteEloBracketChanged( int iOldBracket, int iNewBracket )
{
	m_iEloBracketDelta += (iNewBracket - iOldBracket);
	m_iNewEloBracket = iNewBracket;
}

// Returns the delta between the elo at the start of the map and now. 
// Should be reset explictly when we've displayed the 'elo changed' update to the player.
int CPlayerRankManager::GetEloBracketChange( int &iOutNewEloBracket )
{
	if ( m_iEloBracketDelta != 0 ) 
	{
		Assert( m_iNewEloBracket >= 0 );
		iOutNewEloBracket = m_iNewEloBracket;
	}
	return m_iEloBracketDelta;
}

// Call this after we're sure we've shown the user their new rank.
void CPlayerRankManager::ResetRecordedEloBracketChange( void )
{
	m_iEloBracketDelta = 0;
	m_iNewEloBracket = -1;
}

CON_COMMAND_F( cl_player_rank_events_spew, "Spews the contents of all events this round that could be displayed to the player, as well as the player's current ranks.", FCVAR_DEVELOPMENTONLY )
{
	g_PlayerRankManager.PrintRankProgressThisRound();
}

CON_COMMAND_F( print_achievement_categories, "Spews achievements for each category", FCVAR_DEVELOPMENTONLY )
{
	CUtlMap<int, CBaseAchievement *> &achievements = g_AchievementMgrCS.GetAchievements( 0 );
	for( int i = MEDAL_CATEGORY_START; i < MEDAL_CATEGORY_COUNT; ++i )
	{
		Msg( "Category %d:\n", i );
		int count = 0;
		int countAchieved = 0;
		FOR_EACH_MAP( achievements, idx )
		{
			CBaseAchievement *pAchievement = achievements[idx];
			if ( i == GetAchievementCategory( pAchievement->GetAchievementID() ) )
			{
				Msg( "%s %s\n", pAchievement->GetName(), pAchievement->IsAchieved() ? "*achieved*" : " " );
				count++;
				if ( pAchievement->IsAchieved() )
					countAchieved++;
			}

			if ( GetAchievementCategory( pAchievement->GetAchievementID() ) == MEDAL_CATEGORY_NONE )
			{
				Msg( "***%s Uncategorized!\n", pAchievement->GetName() );
			}
		}
		Msg( "Total: %d\n\n", count );
	}
}

// Pretty ugly... Fragile hand-built categorization using the achievement excel sheet. May need to revisit how
// we tag an achievement with a category more formally.
MedalCategory_t GetAchievementCategory( int id )
{
	switch( id )
	{
	// Team and Objective
	case CSMedalist:
	case CSWinBombPlant:
	case CSWinBombDefuse:
	case CSBombDefuseCloseCall:
	case CSKilledDefuser:
	case CSPlantBombWithin25Seconds:
	case CSKillBombPickup:
	case CSBombMultikill:
	case CSGooseChase:
	case CSWinBombPlantAfterRecovery:
	case CSDefuseDefense:
	case CSPlantBombsLow:
	case CSDefuseBombsLow:
	case CSRescueAllHostagesInARound:
	case CSKilledRescuer:
	case CSFastHostageRescue:
	case CSRescueHostagesLow:
	case CSRescueHostagesMid:
	case CSWinRoundsLow:
	case CSWinRoundsMed:
	case CSWinRoundsHigh:
	case CSFastRoundWin:
	case CSLosslessExtermination:
	case CSFlawlessVictory:
	case CSDonateWeapons:
	case CSBloodlessVictory:
	case CSMoneyEarnedLow:
	case CSMoneyEarnedMed:
	case CSMoneyEarnedHigh:
	case CSKillEnemyTeam:
	case CSLastPlayerAlive:
	case CSWinPistolRoundsLow:
	case CSWinPistolRoundsMed:
	case CSWinPistolRoundsHigh:
	case CSSilentWin:
	case CSWinRoundsWithoutBuying:
	case CSAvengeFriend:
		return MEDAL_CATEGORY_TEAM_AND_OBJECTIVE;

	// Combat
	case CSEnemyKillsLow:
	case CSEnemyKillsMed:
	case CSEnemyKillsHigh:
	case CSKillEnemyReloading:
	case CSKillingSpree:
	case CSKillsWithMultipleGuns:
	case CSHeadshots:
	case CSSurviveGrenade:
	case CSKillEnemyBlinded:
	case CSKillEnemiesWhileBlind:
	case CSKillEnemiesWhileBlindHard:
	case CSKillsEnemyWeapon:
	case CSWinKnifeFightsLow:
	case CSWinKnifeFightsHigh:
	case CSKilledDefuserWithGrenade:
	case CSKillSniperWithSniper:
	case CSKillSniperWithKnife:
	case CSHipShot:
	case CSKillSnipers:
	case CSKillWhenAtLowHealth:
	case CSPistolRoundKnifeKill:
	case CSWinDualDuel:
	case CSGrenadeMultikill:
	case CSKillWhileInAir:
	case CSKillEnemyInAir:
	case CSKillerAndEnemyInAir:
	case CSKillEnemyWithFormerGun:
	case CSKillTwoWithOneShot:
	case CSGiveDamageLow:
	case CSGiveDamageMed:
	case CSGiveDamageHigh:
	case CSKillEnemyLastBullet:
	case CSKillingSpreeEnder:
	case CSSurviveManyAttacks:
	case CSDamageNoKill:
	case CSKillLowDamage:
	case CSUnstoppableForce:
	case CSImmovableObject:
	case CSHeadshotsInRound:
	case CSCauseFriendlyFireWithFlashbang:
		return MEDAL_CATEGORY_COMBAT;

	// Weapon
	case CSEnemyKillsDeagle:
	case CSEnemyKillsHKP2000:
	case CSEnemyKillsGlock:
	case CSEnemyKillsP250:
	case CSEnemyKillsElite:
	case CSEnemyKillsFiveSeven:
	case CSEnemyKillsAWP:
	case CSEnemyKillsAK47:
	case CSEnemyKillsM4A1:
	case CSEnemyKillsAUG:
	case CSEnemyKillsSG556:
	case CSEnemyKillsSCAR20:
	case CSEnemyKillsGALILAR:
	case CSEnemyKillsFAMAS:
	case CSEnemyKillsSSG08:
	case CSEnemyKillsG3SG1:
	case CSEnemyKillsP90:
	case CSEnemyKillsMP7:
	case CSEnemyKillsMP9:
	case CSEnemyKillsMAC10:
	case CSEnemyKillsUMP45:
	case CSEnemyKillsNova:
	case CSEnemyKillsXM1014:
	case CSEnemyKillsMag7:
	case CSEnemyKillsM249:
	case CSEnemyKillsNegev:
	case CSEnemyKillsTec9:
	case CSEnemyKillsSawedoff:
	case CSEnemyKillsBizon:
	case CSEnemyKillsKnife:
	case CSEnemyKillsHEGrenade:
	case CSEnemyKillsMolotov:
	case CSPosthumousGrenadeKill:
	case CSMetaPistol:
	case CSMetaRifle:
	case CSMetaSMG:
	case CSMetaShotgun:
	case CSMetaWeaponMaster:
	case CSEnemyKillsTaser:
	case CSKillWithEveryWeapon:
		return MEDAL_CATEGORY_WEAPON;

	// Map
	case CSWinMapCS_ITALY:
	case CSWinMapCS_OFFICE:
	case CSWinMapDE_AZTEC:
	case CSWinMapDE_DUST:
	case CSWinMapDE_DUST2:
	case CSWinMapDE_INFERNO:
	case CSWinMapDE_NUKE:
	case CSWinMapDE_TRAIN:
	case CSWinMatchAR_SHOOTS:
	case CSWinMatchAR_BAGGAGE:
	case CSWinMatchDE_LAKE:
	case CSWinMatchDE_SAFEHOUSE:   
	case CSWinMatchDE_SUGARCANE:	
	case CSWinMatchDE_STMARC:  
	case CSWinMatchDE_BANK:	
	case CSWinMatchDE_SHORTTRAIN:
	case CSBreakWindows:
	//case CSBreakProps:
		return MEDAL_CATEGORY_MAP;

	// Arsenal
	case CSGunGameKillKnifer:
	case CSWinEveryGGMap:
	case CSGunGameProgressiveRampage:
	case CSGunGameFirstKill:
	case CSFirstBulletKills:
	case CSGunGameConservationist:
	case CSPlantBombsTRLow:
	case CSDefuseBombsTRLow:
	case CSKillEnemyTerrTeamBeforeBombPlant:
	case CSKillEnemyCTTeamBeforeBombPlant:
	case CSBornReady:
	case CSSpawnCamper:
	case CSGunGameKnifeKillKnifer:
	case CSGunGameSMGKillKnifer:
	case CSStillAlive:
	case CSGGRoundsLow:
	case CSGGRoundsMed:
	case CSGGRoundsHigh:
	case CSGGWinRoundsLow:
	case CSGGWinRoundsMed:
	case CSGGWinRoundsHigh:
	case CSGGWinRoundsExtreme:
	case CSGGWinRoundsUltimate:
	case CSPlayEveryGGMap:
	case CSDominationsLow:
	case CSDominationsHigh:
	case CSRevengesLow:
	case CSRevengesHigh:
	case CSDominationOverkillsLow:
	case CSDominationOverkillsHigh:
	case CSDominationOverkillsMatch:
	case CSExtendedDomination:
	case CSConcurrentDominations:
		return MEDAL_CATEGORY_ARSENAL;

	default:
		break;
	}

	// Disabling assert for now, there are some stale achievements with no category that need to be removed first.
	//AssertMsg( 0, "Found achievement with no known category." );
	return MEDAL_CATEGORY_NONE;
}

// Find the loc string with the correct phrasing for this stat based achievement's progress 
// ie, "You've killed X out of Y enemies" vs "You've planted X out of Y bombs".
// Returns NULL if no string token mapped to stat id.
const char* GetLocTokenForStatId( const CSStatType_t id )
{
	switch ( id )
	{
	case CSSTAT_KILLS:
	case CSSTAT_KILLS_DEAGLE:
	case CSSTAT_KILLS_GLOCK:
	case CSSTAT_KILLS_ELITE:
	case CSSTAT_KILLS_FIVESEVEN:
	case CSSTAT_KILLS_BIZON:
	case CSSTAT_KILLS_TEC9:
	case CSSTAT_KILLS_TASER:
	case CSSTAT_KILLS_HKP2000:
	case CSSTAT_KILLS_P250:
	case CSSTAT_KILLS_AWP:
	case CSSTAT_KILLS_AK47:
	case CSSTAT_KILLS_M4A1:
	case CSSTAT_KILLS_AUG:
	case CSSTAT_KILLS_GALILAR:
	case CSSTAT_KILLS_FAMAS:
	case CSSTAT_KILLS_G3SG1:
	case CSSTAT_KILLS_SCAR20:
	case CSSTAT_KILLS_SG556:
	case CSSTAT_KILLS_SSG08:
	case CSSTAT_KILLS_P90:
	case CSSTAT_KILLS_MAC10:
	case CSSTAT_KILLS_UMP45:
	case CSSTAT_KILLS_MP7:
	case CSSTAT_KILLS_MP9:
	case CSSTAT_KILLS_XM1014:
	case CSSTAT_KILLS_MAG7:
	case CSSTAT_KILLS_SAWEDOFF:
	case CSSTAT_KILLS_NOVA:
	case CSSTAT_KILLS_M249:
	case CSSTAT_KILLS_NEGEV:
	case CSSTAT_KILLS_KNIFE:
	case CSSTAT_KILLS_HEGRENADE:
	case CSSTAT_KILLS_MOLOTOV:
	case CSSTAT_KILLS_ENEMY_WEAPON:
	case CSSTAT_KILLS_ENEMY_BLINDED:
	case CSSTAT_KILLS_KNIFE_FIGHT:
	case CSSTAT_KILLS_AGAINST_ZOOMED_SNIPER:
		return "SFUI_WinPanelProg_stat_progress_kills";

	case  CSSTAT_MAP_WINS_CS_ITALY:
	case  CSSTAT_MAP_WINS_CS_OFFICE:
	case  CSSTAT_MAP_WINS_DE_AZTEC:
	case  CSSTAT_MAP_WINS_DE_DUST2:
	case  CSSTAT_MAP_WINS_DE_DUST:
	case  CSSTAT_MAP_WINS_DE_INFERNO:
	case  CSSTAT_MAP_WINS_DE_NUKE:
	case  CSSTAT_MAP_WINS_DE_TRAIN:
	case  CSSTAT_MAP_MATCHES_WON_BANK:
	case  CSSTAT_MAP_MATCHES_WON_SHOOTS:
	case  CSSTAT_MAP_MATCHES_WON_STMARC:
	case  CSSTAT_MAP_MATCHES_WON_SUGARCANE:
	case  CSSTAT_MAP_MATCHES_WON_SAFEHOUSE:
	case  CSSTAT_MAP_MATCHES_WON_LAKE:
	case  CSSTAT_MAP_MATCHES_WON_BAGGAGE:
	case  CSSTAT_MAP_MATCHES_WON_SHORTTRAIN:
	case CSSTAT_ROUNDS_WON:
	case CSSTAT_PISTOLROUNDS_WON:
	case CSSTAT_GUN_GAME_MATCHES_WON:
		return "SFUI_WinPanelProg_stat_progress_wins";

	case CSSTAT_GUN_GAME_MATCHES_PLAYED:
		return "SFUI_WinPanelProg_stat_progress_played";

	case CSSTAT_MONEY_EARNED:
		return "SFUI_WinPanelProg_stat_progress_money";

	case CSSTAT_DAMAGE:
		return "SFUI_WinPanelProg_stat_progress_damage";

	case CSSTAT_KILLS_HEADSHOT:
		return "SFUI_WinPanelProg_stat_progress_headshots";

	case CSSTAT_NUM_BOMBS_PLANTED:
		return "SFUI_WinPanelProg_stat_progress_bomb_plant";

	case CSSTAT_NUM_BOMBS_DEFUSED:
		return "SFUI_WinPanelProg_stat_progress_bomb_diffuse";

	case CSSTAT_NUM_HOSTAGES_RESCUED:
		return "SFUI_WinPanelProg_stat_progress_rescue";

	case CSSTAT_WEAPONS_DONATED:
		return "SFUI_WinPanelProg_stat_progress_donate";

	case CSSTAT_DOMINATIONS:
		return "SFUI_WinPanelProg_stat_progress_dominate";

	case CSSTAT_DOMINATION_OVERKILLS:
		return "SFUI_WinPanelProg_stat_progress_overkill";

	case CSSTAT_REVENGES:
		return "SFUI_WinPanelProg_stat_progress_revenge";

	default:
		break;
	}

	return NULL;
}

bool CPlayerRankManager::HasBuiltMedalCategories( void ) const
{
	return m_bMedalCategoriesBuilt;
}


#if defined ( _GAMECONSOLE )

// Official servers request elo bracket info when a player connects. 
bool MsgFunc_RequestEloBracketInfo( const CCSUsrMsg_RequestEloBracketInfo &msg )
{
	ELOGameType_t game_mode = (ELOGameType_t) msg.bracket();
	g_PlayerRankManager.ServerRequestBracketInfo( game_mode );
	return true;
}

// We want all the bracket calculation logic to happen on the server (because we expect to change it post ship)
// but all the storage for console must happen on the client. Thus this horribleness. 
void CPlayerRankManager::ServerRequestBracketInfo( ELOGameType_t game_mode )
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer || !engine->IsConnected() )
		return;

	// Restrict bracket changes to gamempads for console 
	InputDevice_t device = g_pInputSystem->GetCurrentInputDevice();	
	if ( device != INPUT_DEVICE_GAMEPAD )
		return;

	unsigned short idx =	m_PlayerBracketInfos.Find( game_mode );
	if ( idx == m_PlayerBracketInfos.InvalidIndex() )
		return;

	PlayerELOBracketInfo_t& eloBracketInfo = m_PlayerBracketInfos[idx];

	KeyValues *kv = new KeyValues("player_elo_bracket_info");
	kv->SetInt( "game_mode", game_mode );
	kv->SetInt( "display_bracket",  eloBracketInfo.m_DisplayBracket );
	kv->SetInt( "previous_bracket", eloBracketInfo.m_PreviousBracket );
	kv->SetInt( "games_in_bracket", eloBracketInfo.m_NumGamesInBracket );

	// Base_CmdKeyValues handles the kv deletion.
	engine->ServerCmdKeyValues( kv );
}

// official servers send this after rounds end so clients can handle storage.
bool MsgFunc_SetEloBracketInfo( const CCSUsrMsg_SetEloBracketInfo &msg )
{
	ELOGameType_t game_mode = (ELOGameType_t) msg.game_mode();
	int8 display_bracket = msg.display_bracket();
	int8 prev_bracket = msg.prev_bracket();
	int8 games_in_bracket = msg.num_games_in_bracket();

	//Msg( "SetEloBracket %d %d %d %d\n", game_mode, display_bracket, prev_bracket, games_in_bracket );

	g_PlayerRankManager.Console_SetEloBracket( game_mode, display_bracket, prev_bracket, games_in_bracket );

	return true;
}

bool CPlayerRankManager::Console_SetEloBracket( ELOGameType_t game_mode, uint8 display_bracket, uint8 prev_bracket, uint8 num_games_in_bracket )
{
	// make sure they'll fit in four bits.
	Assert( (display_bracket & 0xF0) == 0 );
	Assert( (prev_bracket & 0xF0) == 0 );

	//Msg( " %d %d %d %d \n", game_mode, display_bracket, prev_bracket, num_games_in_bracket );

	unsigned short idx = m_PlayerBracketInfos.Find( game_mode );
	if ( idx == m_PlayerBracketInfos.InvalidIndex() )
	{
		if ( game_mode > ELOGameType::INVALID && game_mode < ELOGameType::COUNT )
		{
			idx = m_PlayerBracketInfos.Insert( game_mode );
		}
		else
		{
			return false;
		}
	}

	PlayerELOBracketInfo_t& eloBracketInfo = m_PlayerBracketInfos[idx];

	eloBracketInfo.m_DisplayBracket = display_bracket;
	eloBracketInfo.m_PreviousBracket = prev_bracket;
	eloBracketInfo.m_NumGamesInBracket = num_games_in_bracket;

	return true;
}

bool CPlayerRankManager::Console_SetEloBracket( ELOGameType_t game_mode, const PlayerELOBracketInfo_t& bracket )
{
	return Console_SetEloBracket( game_mode, bracket.m_DisplayBracket, bracket.m_PreviousBracket, bracket.m_NumGamesInBracket );
}

// Returns the display bracket, -1 on failure (no recorded bracket info for specified game mode).
// Optional pointer to elo bracket struct can be filled out.
int CPlayerRankManager::Console_GetEloBracket( ELOGameType_t game_mode, PlayerELOBracketInfo_t *pOutBracketInfo /*=NULL*/ )
{
	unsigned short idx =	m_PlayerBracketInfos.Find( game_mode );
	if ( idx != m_PlayerBracketInfos.InvalidIndex() )
	{
		PlayerELOBracketInfo_t &info = m_PlayerBracketInfos[idx];
		if ( pOutBracketInfo )
			*pOutBracketInfo = info;

		return info.m_DisplayBracket;
	}
	return -1;
}

#endif // _GAMECONSOLE


