//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Weapon selection hud
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include "sfhud_uniquealerts.h"
#include "hud_macros.h"
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include "iclientmode.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_element_helper.h"
#include "iclientmode.h"
#include "scaleformui/scaleformui.h"
#include "sfhudfreezepanel.h"
#include <engine/IEngineSound.h>
#include "vguicenterprint.h"
#include "hltvreplaysystem.h"
#include "c_team.h"

#include "clientsteamcontext.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( SFUniqueAlerts );
//
SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( OnAlertPanelHideAnimEnd ),
SFUI_END_GAME_API_DEF( SFUniqueAlerts, UniqueAlerts );

extern ConVar mp_dm_bonus_percent;
extern ConVar cl_draw_only_deathnotices;
extern ConVar item_debug_give_fake_random_tourney_awards;
extern ConVar mp_team_timeout_max;

//--------------------------------------------------------------------------------------------------------------
/**
 * Return the number of players on the given team
 */
static int UTIL_PlayersOnTeam( int teamID, bool isAlive = false, bool allowBots = true )
{
	int count = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBaseEntity *entity = UTIL_PlayerByIndex( i );

		if ( entity == NULL )
			continue;

		CBasePlayer *player = static_cast<CBasePlayer *>( entity );

		if ( !allowBots && player->IsBot() )
			continue;

		if ( player->GetTeamNumber() != teamID )
			continue;

		if ( isAlive && !player->IsAlive() )
			continue;

		count++;
	}

	return count;
}


// This function was designed for the quests included in Operation: Wildfire,
// and may need to be updated for future operations.
static bool ShouldQuestShowMissionPanel( const CEconQuestDefinition *pQuestDef )
{
	if ( !pQuestDef )
		return false;

	if ( pQuestDef->IsAnEvent() )
		return false;

	// No mission panel for guardian mode -- we have special case code for these quests
	const char* gameMode = pQuestDef->GetGameMode();
	if ( gameMode && !Q_stricmp( gameMode, "cooperative" ) )
		return false;

	// No mission panel for co-op strike mode.  Goal is just to win.
	if ( gameMode && !Q_stricmp( gameMode, "coopmission" ) )
		return false;

	// Show mission panel for all other quests
	return true;
}

static bool ShouldQuestShowMissionPanel( CCSPlayer* pPlayer, CCSGameRules* pGameRules )
{
	// Don't show quest UI if we don't have an active quest.
	uint32 unQuestId = pPlayer->GetActiveQuestID();
	if ( unQuestId == 0 )
		return false;

	// If the quest isn't valid for some reason (perhaps we are in the wrong map or mode)
	// don't show the UI.  We do allow showing it during transient situations (all bots,
	// or during warmup)
	switch ( pPlayer->GetQuestProgressReason() )
	{
	case QuestProgress::QUEST_OK:
	case QuestProgress::QUEST_NOT_ENOUGH_PLAYERS:
	case QuestProgress::QUEST_WARMUP:
		break;

	default:
		return false;
	}

	// Check if the player's active quest allows showing the mission panel
	if ( !ShouldQuestShowMissionPanel( GetItemSchema()->GetQuestDefinition( unQuestId ) ) )
		return false;

	// Don't show quest progress at game end; at this point the server updates the user's
	// quest progress but the client still has the 'uncommitted' quest progress state, and
	// so it double counts.  Just hide in this case to avoid showing the double-counted amount.
	if ( pGameRules->GetGamePhase() == GAMEPHASE_MATCH_ENDED )
		return false;

	// Everything is OK, show quest UI!
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SFUniqueAlerts::SFUniqueAlerts( const char *value ) 
	: SFHudFlashInterface( value )
	, m_bVisible( false )

	, m_AlertState( ALERTSTATE_HIDDEN )
	, m_flNextAlertTick( -1 )

	, m_bShowDemolitionProgressionPanel( false )

	, m_bDMBonusIsActive( false )
	, m_flNextDMBonusTick( -1 )
	, m_nDMCurrentWeaponPoints( 0 )
	, m_nDMCurrentWeaponBonusPoints( 0 )
	, m_nDMBonusWeaponSlot( -1 )

	, m_bMissionVisible( false )

	, m_guardianPanelInitialized( false )
	, m_guardianPanelHideTick( -1 )
	, m_nGuardianKills( 0 )
	, m_nGuardianWeapon( -1 )
	, m_nPlayerMoney( 0 )

	, m_bQuestMissionActive( false )

#if defined ( ENABLE_GIFT_PANEL )
	, m_bIsGiftPanelActive( false )
	, m_nGlobalGiftsGivenInPeriod( 0 )
	, m_nGlobalNumGiftersInPeriod( 0 )
	, m_nGlobalGiftingPeriodSeconds( 0 )
	, m_flLastGiftPanelUpdate( 0 )
	, m_PlayerSpotlightTempList()
#endif
{		
	SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_MISCSTATUS );

#if defined ( ENABLE_GIFT_PANEL )
	m_PlayerSpotlightTempList.RemoveAll();
#endif
}

static bool UTIL_CheckWeaponMatch(int weaponIdx, const CWeaponCSBase* pWeapon)
{
	if ( !pWeapon )
		return false;

	const CEconItemView *pItemView = pWeapon->GetEconItemView();
	if ( !pItemView )
		return false;

	const CEconItemDefinition *pItemDef = pItemView->GetItemDefinition();
	if ( !pItemDef )
		return false;

	if ( weaponIdx != pItemDef->GetDefinitionIndex() )
		return false;

	return true;
}

void SFUniqueAlerts::ProcessMissionPanelGuardian()
{
	C_CSPlayer* pPlayer = C_CSPlayer::GetLocalCSPlayer();

	bool guardianForceShow = true; // Always show the mission panel in guardian mode.
	bool guardianPanelChange = false;

	if ( m_nGuardianKills != CSGameRules()->GetGuardianKillsRemaining() )
	{
		// If we got a kill, notice the update
		int newKills = CSGameRules()->GetGuardianKillsRemaining();
		m_nGuardianKills = newKills;
		guardianPanelChange = true;
	}

	if ( m_nGuardianWeapon != CSGameRules()->GetGuardianSpecialWeapon() )
	{
		// Weapon changed, just reinitialize from nothing
		m_nGuardianWeapon = CSGameRules()->GetGuardianSpecialWeapon();
		guardianPanelChange = true;
		m_guardianPanelInitialized = false; // force re-initialize of state.
	}

	bool hasSpecialWeapon = false;
	bool hasSpecialWeaponEquipped = false;

	if ( m_nGuardianWeapon != 0 )
	{
		// Check if the player has the special weapon
		int numWeapons = pPlayer->WeaponCount();
		for ( int iWeapon = 0; iWeapon < numWeapons; ++iWeapon )
		{
			const CWeaponCSBase* pWeapon = dynamic_cast< CWeaponCSBase* >( pPlayer->GetWeapon( iWeapon ) );
			if ( UTIL_CheckWeaponMatch( m_nGuardianWeapon, pWeapon ) )
			{
				hasSpecialWeapon = true;
				break;
			}
		}

		// Check if the player has the special weapon equipped
		CWeaponCSBase* pActiveWeapon = pPlayer->GetActiveCSWeapon();
		hasSpecialWeaponEquipped = UTIL_CheckWeaponMatch( m_nGuardianWeapon, pActiveWeapon );
	}

	// Check for state changes
	if ( hasSpecialWeapon != m_bGuardianHasWeapon )
	{
		m_bGuardianHasWeapon = hasSpecialWeapon;
		guardianPanelChange = true;
	}

	if ( hasSpecialWeaponEquipped != m_bGuardianHasWeaponEquipped )
	{
		m_bGuardianHasWeaponEquipped = hasSpecialWeaponEquipped;
		guardianPanelChange = true;
	}

	if ( guardianPanelChange )
	{
		if ( !m_guardianPanelInitialized )
		{
			m_guardianPanelInitialized = true;

			// Get total number of kills required
			int killsRequired = CSGameRules()->GetGuardianRequiredKills();

			// Get weapon name
			const CEconItemDefinition* pItemDef = NULL;

			if ( m_nGuardianWeapon != 0 )
				pItemDef = GetItemSchema()->GetItemDefinition( m_nGuardianWeapon );

			wchar_t szQuestDescription[128];
			const char* szWeaponIcon;
			const wchar_t* szWeaponName;

			if ( pItemDef && pItemDef->GetDefinitionIndex() != 0 )
			{
				szWeaponIcon = pItemDef->GetDefinitionName();
				szWeaponName = g_pVGuiLocalize->Find( pItemDef->GetItemBaseName() );
			}
			else
			{
				szWeaponIcon = ""; // TODO: new "any weapon" icon
				szWeaponName = g_pVGuiLocalize->Find( "#quest_weapon_any_weapon" );
			}

			g_pVGuiLocalize->ConstructString( szQuestDescription, sizeof( szQuestDescription ), g_pVGuiLocalize->Find( "#quest_hud_guardian_kills_with" ), 1, szWeaponName );

			// function missionInitGuardian( weaponName:String, questDesc:String, killsRequired:Number, weaponCost:Number )
			WITH_SFVALUEARRAY_SLOT_LOCKED( data, 4 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, szWeaponIcon );
				m_pScaleformUI->ValueArray_SetElement( data, 1, szQuestDescription );
				m_pScaleformUI->ValueArray_SetElement( data, 2, killsRequired );
				m_pScaleformUI->ValueArray_SetElement( data, 3, 0 ); // $$$REI TODO ECON: Get weapon cost

				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "missionInitGuardian", data, 4 );
			}
		}

		if ( !m_bMissionVisible )
		{
			m_bMissionVisible = true;

			// Need to show the panel
			WITH_SLOT_LOCKED
			{
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "missionShowPanel", NULL, 0 );
			}
		}

		// Update panel with new data
		int killsRemaining = CSGameRules()->GetGuardianKillsRemaining();
		int money = 0; // pPlayer->GetAccount(); $$$REI TODO ECON

		// function missionUpdateGuardian( killsRemaining:Number, money : Number, hasWeapon : Boolean )
		WITH_SFVALUEARRAY_SLOT_LOCKED( data, 3 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, killsRemaining );
			m_pScaleformUI->ValueArray_SetElement( data, 1, money );
			m_pScaleformUI->ValueArray_SetElement( data, 2, m_bGuardianHasWeapon );

			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "missionUpdateGuardian", data, 3 );
		}

		// Show panel for at least 5 seconds after any change
		m_guardianPanelHideTick = Plat_FloatTime() + 5.0f;
	}
	else if ( guardianForceShow && !m_bMissionVisible )
	{
		m_bMissionVisible = true;

		// Need to show the panel
		WITH_SLOT_LOCKED
		{
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "missionShowPanel", NULL, 0 );
		}
	}

	if ( m_bMissionVisible && !guardianForceShow && m_guardianPanelHideTick <= Plat_FloatTime() )
	{
		m_bMissionVisible = false;

		WITH_SLOT_LOCKED
		{
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "missionHidePanel", NULL, 0 );
		}
	}
}

void SFUniqueAlerts::ProcessDMBonusPanel()
{
	int nSecondsLeft = ( CSGameRules()->GetDMBonusStartTime() + CSGameRules()->GetDMBonusTimeLength() ) - gpGlobals->curtime;

	if ( nSecondsLeft >= 0 && CSGameRules()->IsDMBonusActive() )
	{
		int nSlot = CSGameRules()->GetDMBonusWeaponLoadoutSlot();
		if ( m_nDMBonusWeaponSlot != nSlot || m_bVisible == false )
		{
			m_nDMBonusWeaponSlot = nSlot;
			m_nDMCurrentWeaponPoints = CSGameRules()->GetWeaponScoreForDeathmatch( nSlot );
			m_nDMCurrentWeaponBonusPoints = ( ( float )( mp_dm_bonus_percent.GetInt() ) / 100.0f ) * m_nDMCurrentWeaponPoints;

			CCSPlayerInventory* pPlayerInv = CSInventoryManager()->GetInventoryForPlayer( ClientSteamContext().GetLocalPlayerSteamID() );
			if ( !pPlayerInv )
				return;

			CEconItemView* pItem = pPlayerInv->GetItemInLoadout( C_CSPlayer::GetLocalCSPlayer()->GetTeamNumber(), nSlot );
			if ( !pItem )
				return;

			ShowDMBonusWeapon( pItem, nSlot, nSecondsLeft );
		}
		else
		{
			WITH_SFVALUEARRAY_SLOT_LOCKED( data, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, nSecondsLeft );
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setDMBonusTimer", data, 1 );
			}
		}
	}
	else
	{
		m_bDMBonusIsActive = false;

		WITH_SLOT_LOCKED
		{
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hideDMBonusWeapon", NULL, 0 );
		}

		C_RecipientFilter filter;
		filter.AddRecipient( C_CSPlayer::GetLocalPlayer() );
		C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "UI.DeathMatchBonusAlertEnd" );
	}
}

void SFUniqueAlerts::ProcessAlertBar()
{
	// get the round restart time first
	float flEndTime = CSGameRules()->GetRoundRestartTime() - 0.5f;
	bool bIsRestarting = CSGameRules()->IsGameRestarting();

	if ( g_HltvReplaySystem.GetHltvReplayDelay() > 0 )
	{
		//ShowHltvReplayAlertPanel( int( CL_GetHltvReplayTicksLeft() * gpGlobals->interval_per_tick ) );
		HideAlertText(); // we don't need  the DELAYED REPLAY strip anymore
	}
	else if ( bIsRestarting )
	{
		int nLeft = ( int )flEndTime - ( int )gpGlobals->curtime;
		if ( nLeft >= 0 )
		{
			wchar_t szNotice[128] = L"";
			wchar_t wzSecs[16];
			V_swprintf_safe( wzSecs, L"%d", nLeft );

			if ( CSGameRules()->IsWarmupPeriod() )
			{
				if ( nLeft == 0 )
					g_pVGuiLocalize->ConstructString( szNotice, sizeof( szNotice ), g_pVGuiLocalize->Find( "#SFUI_Notice_Alert_Match_Starting" ), 1, wzSecs );
				else
					g_pVGuiLocalize->ConstructString( szNotice, sizeof( szNotice ), g_pVGuiLocalize->Find( "#SFUI_Notice_Alert_Match_Starting_In" ), 1, wzSecs );
			}
			else
			{
				if ( nLeft == 0 )
					g_pVGuiLocalize->ConstructString( szNotice, sizeof( szNotice ), g_pVGuiLocalize->Find( "#SFUI_Notice_Alert_Match_Restarting" ), 1, wzSecs );
				else
					g_pVGuiLocalize->ConstructString( szNotice, sizeof( szNotice ), g_pVGuiLocalize->Find( "#SFUI_Notice_Alert_Match_Restarting_In" ), 1, wzSecs );
			}

			ShowAlertText( szNotice );
		}
	}
	else if ( CSGameRules()->IsWarmupPeriod() )
	{
		ShowWarmupAlertPanel();
	}
	else if ( CSGameRules()->IsFreezePeriod() )// we're paused
	{
		if ( CSGameRules()->IsTimeOutActive() )
		{
			int nTimeLeftInSec;
			int nTimeOutsRemaining;
			int iTeamIndex;

			if ( CSGameRules()->IsTerroristTimeOutActive() )
			{
				nTimeOutsRemaining = CSGameRules( )->GetTerroristTimeOuts( );
				nTimeLeftInSec = ( int )CSGameRules()->GetTerroristTimeOutRemaining();
				iTeamIndex = TEAM_TERRORIST;
			}
			else if ( CSGameRules()->IsCTTimeOutActive() )
			{
				nTimeOutsRemaining = CSGameRules( )->GetCTTimeOuts( );
				nTimeLeftInSec = ( int )CSGameRules()->GetCTTimeOutRemaining();
				iTeamIndex = TEAM_CT;
			}
			else
			{
				return;
			}

			const char *szTeam = "#SFUI_Notice_Alert_Timeout";

			if ( nTimeLeftInSec > 0 )
			{
 				int nMinLeft = nTimeLeftInSec / 60;
				int nSecLeft = nTimeLeftInSec - ( nMinLeft * 60 );

				wchar_t szNotice[64] = L"";
				wchar_t wzTime[32] = L"";

				if ( mp_team_timeout_max.GetInt() > 1 )
				{
					_snwprintf( wzTime, ARRAYSIZE( wzTime ), L"%d:%02d (%d remaining)", nMinLeft, nSecLeft, nTimeOutsRemaining );
				}
				else
				{
					_snwprintf( wzTime, ARRAYSIZE( wzTime ), L"%d:%02d", nMinLeft, nSecLeft );
				}

				// get the team
				C_Team *pTeam = GetGlobalTeam( iTeamIndex );
				// se if we have a custom clan name
				wchar_t wszSafeName[ MAX_TEAM_NAME_LENGTH ];
				wszSafeName[ 0 ] = L'\0';

				if ( ( pTeam == NULL ) || StringIsEmpty( pTeam->Get_ClanName( ) ) )
				{
					// if not, just use the default T or CT labels
					switch ( iTeamIndex )
					{
					case TEAM_TERRORIST:
						V_snwprintf( wszSafeName, ARRAYSIZE( wszSafeName ), g_pVGuiLocalize->Find( "#terrorists" ) );
						break;

					case TEAM_CT:
						V_snwprintf( wszSafeName, ARRAYSIZE( wszSafeName ), g_pVGuiLocalize->Find( "#counter-terrorists" ) );
						break;
					}
				}
				else
				{
					wchar_t wszName[ MAX_TEAM_NAME_LENGTH ];
					// we have a custom team name, convert to wide
					g_pVGuiLocalize->ConvertANSIToUnicode( pTeam->Get_ClanName( ), wszName, sizeof( wszName ) );
					// now make the team name string safe
					g_pScaleformUI->MakeStringSafe( wszName, wszSafeName, sizeof( wszName ) );
				}

				g_pVGuiLocalize->ConstructString( szNotice, sizeof( szNotice ), g_pVGuiLocalize->Find( szTeam ), 2, wszSafeName, wzTime );

				ShowAlertText( szNotice );
			}
		}
		else if ( CSGameRules()->IsMatchWaitingForResume() )
		{
			ShowAlertText( g_pVGuiLocalize->Find( "#SFUI_Notice_Alert_Freeze_Pause" ) );
		}
		else
		{
			HideAlertText();
		}
	}
	else
	{
		HideAlertText(); // Is this a good thing to do here?  seems like we might be overriding someone else....
	}
}

#if defined ( ENABLE_GIFT_PANEL )
void SFUniqueAlerts::ProcessGiftPanel()
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	AssertMsg( pLocalPlayer, "Should have checked GetLocalCSPlayer was valid in ProcessInput()" );

	if ( ( CSGameRules()->IsWarmupPeriod() || ( CSGameRules()->IsFreezePeriod() && CSGameRules()->GetTotalRoundsPlayed() < 1 ) ) &&
		!( engine->GetDemoPlaybackParameters() && engine->GetDemoPlaybackParameters()->m_bAnonymousPlayerIdentity )
		)
	{
		// don't update every frame
		if ( ( m_flLastGiftPanelUpdate + 1.0f ) <= gpGlobals->curtime )
		{
			m_flLastGiftPanelUpdate = gpGlobals->curtime;

			m_nGlobalGiftsGivenInPeriod = CSGameRules()->m_numGlobalGiftsGiven;
			m_nGlobalNumGiftersInPeriod = CSGameRules()->m_numGlobalGifters;
			m_nGlobalGiftingPeriodSeconds = CSGameRules()->m_numGlobalGiftsPeriodSeconds;

			if ( m_nGlobalGiftsGivenInPeriod > 0 )
			{
				// get the data from gamerules about: 
				// how many gifts were given
				// how many players gifted them
				// the steamIDs of the five "spotlighted" players
				AccountID_t uiLocalAccountID = steamapicontext->SteamUser()->GetSteamID().GetAccountID();
				bool bIsLocalPlayer = false;

				m_PlayerSpotlightTempList.RemoveAll();

				for ( int i = 0; i < MAX_PLAYER_GIFT_DROP_DISPLAY; i++ )
				{
					AccountID_t uiAccountID = CSGameRules()->m_arrFeaturedGiftersAccounts[i];
					if ( uiAccountID == 0 )
						break;

					int nGiftNum = CSGameRules()->m_arrFeaturedGiftersGifts[i];
					if ( pLocalPlayer && uiLocalAccountID == uiAccountID )
						bIsLocalPlayer = true;

					if ( m_PlayerSpotlightTempList.Count() < MAX_PLAYER_GIFT_DROP_DISPLAY )
					{
						GiftTempList_t drop;
						//drop.nDefindex = nDefindex;
						drop.uiAccountID = uiAccountID;
						drop.nNumGifts = nGiftNum;
						drop.bIsLocalPlayer = bIsLocalPlayer;

						m_PlayerSpotlightTempList.AddToTail( drop );
					}
				}

				WITH_SLOT_LOCKED
				{
					g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ClearAllPlayerReceivedGift", NULL, 0 );
				}

				// send the list of player ids - we'll show these names on screen
				if ( m_nGlobalGiftsGivenInPeriod && m_PlayerSpotlightTempList.Count() > 0 )
				{
					for ( int i = 0; i < m_PlayerSpotlightTempList.Count(); i++ )
					{
						CSteamID steamID( m_PlayerSpotlightTempList[i].uiAccountID, steamapicontext->SteamUser()->GetSteamID().GetEUniverse(), k_EAccountTypeIndividual );

						// only send the first 5
						if ( i < MAX_PLAYER_GIFT_DROP_DISPLAY )
						{
							WITH_SFVALUEARRAY_SLOT_LOCKED( args, 4 )
							{
								char xuidText[255];
								xuidText[0] = '\0';
								V_snprintf( xuidText, ARRAYSIZE( xuidText ), "%llu", steamID.ConvertToUint64() );

								m_pScaleformUI->ValueArray_SetElement( args, 0, xuidText );
								m_pScaleformUI->ValueArray_SetElement( args, 1, m_PlayerSpotlightTempList[i].nDefindex );
								m_pScaleformUI->ValueArray_SetElement( args, 2, m_PlayerSpotlightTempList[i].nNumGifts );
								m_pScaleformUI->ValueArray_SetElement( args, 3, m_PlayerSpotlightTempList[i].bIsLocalPlayer );
								m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "AddPlayerReceivedGift", args, 4 );
							}
						}
					}

					WITH_SFVALUEARRAY_SLOT_LOCKED( data, 3 )
					{
						m_pScaleformUI->ValueArray_SetElement( data, 0, m_nGlobalGiftsGivenInPeriod );
						m_pScaleformUI->ValueArray_SetElement( data, 1, m_nGlobalNumGiftersInPeriod );
						m_pScaleformUI->ValueArray_SetElement( data, 2, m_nGlobalGiftingPeriodSeconds );
						g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "UpdatePanelAndItemNumbers", data, 3 );
					}
				}

				m_bIsGiftPanelActive = true;
			}
		}
	}
	else if ( m_bIsGiftPanelActive == true )
	{
		m_bIsGiftPanelActive = false;
		m_nGlobalGiftsGivenInPeriod = 0;
		// remove our temp list
		m_PlayerSpotlightTempList.RemoveAll();

		WITH_SLOT_LOCKED
		{
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "HidePanelAndItemNumbers", NULL, 0 );
		}
	}
}
#endif // ENABLE_GIFT_PANEL

#if MISSIONPANEL_ENABLE_QUEST_PROGRESSION
void SFUniqueAlerts::ProcessMissionPanelQuest()
{
	// TODO: Only tick every quarter second

	C_CSPlayer* pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	uint32 unQuestId = pPlayer->GetActiveQuestID();
	if ( !unQuestId )
		return;

	const CEconQuestDefinition* pQuestDef = GetItemSchema()->GetQuestDefinition( unQuestId );
	if ( !pQuestDef )
		return;

	// Get current progress
	uint32 numPointsRemaining, numPointsUncommitted;
	if ( !InventoryManager()->BGetPlayerQuestIdPointsRemaining( unQuestId, numPointsRemaining, numPointsUncommitted ) )
		return;

	// Get the required # of points to complete the quest.
	// TODO: This interface is awkward, I think it was originally to support quests with
	// multiple objectives, but this array is always size 1 (or maybe sometimes 0, which means '1 point required').
	int requiredPoints = 1;
	const CCopyableUtlVector< uint32 >& points = pQuestDef->GetQuestPoints();
	if ( !points.IsEmpty() )
		requiredPoints = points[0];

	int currentPoints = ( requiredPoints - numPointsRemaining ) + numPointsUncommitted;
	if ( currentPoints < 0 )
		currentPoints = 0;
	if ( currentPoints > requiredPoints )
		currentPoints = requiredPoints;

	// Initialize panel on first access
	if(!m_bMissionVisible)
	{
		m_bMissionVisible = true;

		// Get the current quest information
		WITH_SFVALUEARRAY_SLOT_LOCKED( args, 2 )
		{
			m_pScaleformUI->ValueArray_SetElement( args, 0, currentPoints );
			m_pScaleformUI->ValueArray_SetElement( args, 1, requiredPoints );
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "missionSetGoal", args, 2 );
		}

		// Get mission description
		const char* szDescToken = pQuestDef->GetHudDesscriptionLocToken();
		const wchar_t *szDescUnformatted = NULL;
		wchar_t szDesc[1024] = L"";
		wchar_t szTrimmedDesc[1024] = L"";

		if ( szDescToken != NULL && szDescToken[0] != '\0' )
			szDescUnformatted = g_pVGuiLocalize->Find( szDescToken );

		if ( szDescUnformatted != NULL )
		{
			// TODO: Factor out this code (it, or something like it, is used in a few places)

			// Get a fake item for this quest
			static CSchemaItemDefHandle hItemDefQuest( "quest" );

			// create a new KV that has localized strings in it
			KeyValues *pKVLocalizedQuestStrings = new KeyValues( "LocalizedQuestStrings" );
			KeyValues::AutoDelete autodelete( pKVLocalizedQuestStrings );

			g_pVGuiLocalize->ConstructString( szDesc, sizeof( szDesc ), szDescUnformatted, pKVLocalizedQuestStrings );

			// Remove double spaces caused by missing optional tags
			UTIL_TrimEmptyWhitespaceFromHTML( szTrimmedDesc, sizeof( szTrimmedDesc ), szDesc );
		}

		WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
		{
			m_pScaleformUI->ValueArray_SetElement( args, 0, szTrimmedDesc );
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "missionSetDescription", args, 1 );
		}

		WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
		{
			// Currently not using Extra Info area.  Could put bonus requirements/points there
			m_pScaleformUI->ValueArray_SetElement( args, 0, "" );
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "missionSetExtraInfo", args, 1 );
		}

		const char* szIcon = pQuestDef->GetIcon();
		if ( !szIcon )
			szIcon = "";

		WITH_SFVALUEARRAY_SLOT_LOCKED( args, 2 )
		{
			m_pScaleformUI->ValueArray_SetElement( args, 0, szIcon );
			m_pScaleformUI->ValueArray_SetElement( args, 1, true ); // always show solid icon for quests for now.
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "missionUpdateIcon", args, 2 );
		}

		WITH_SLOT_LOCKED
		{
			// Show the panel!
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "missionShowPanel", NULL, 0 );
		}
	}

	// Update current quest points
	WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
	{
		m_pScaleformUI->ValueArray_SetElement( args, 0, currentPoints );
		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "missionUpdateGoal", args, 1 );
	}
}
#endif // MISSIONPANEL_ENABLE_QUEST_PROGRESSION

void SFUniqueAlerts::ProcessInput()
{
	if ( !m_pScaleformUI )
		return;

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer || !CSGameRules() )
		return;

	if ( m_bShowDemolitionProgressionPanel )
	{
		m_bShowDemolitionProgressionPanel = false;
		UpdateGGWeaponIconList();
	}

	if ( CSGameRules()->IsPlayingCoopGuardian() )
	{
		// Check if we need to update the guardian panel
		// TODO: Maybe only do this once every .25 seconds or so?  Doesn't seem to be a 
		//       performance issue at the moment, so leaving it as is for now.
		ProcessMissionPanelGuardian();
	}
#if MISSIONPANEL_ENABLE_QUEST_PROGRESSION
	else if( ShouldQuestShowMissionPanel(pPlayer, CSGameRules()) )
	{
		ProcessMissionPanelQuest();
	}
#endif
	else if( m_bMissionVisible )
	{
		// Somehow we ended up with the mission panel visible even though no mission is active.  Hide it.
		m_bMissionVisible = false;

		WITH_SLOT_LOCKED
		{
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "missionHidePanel", NULL, 0 );
		}
	}

	if ( m_bDMBonusIsActive && m_flNextDMBonusTick <= Plat_FloatTime() )
	{
		m_flNextDMBonusTick = Plat_FloatTime() + 0.25f;
		ProcessDMBonusPanel();
	}

	if ( m_bVisible && m_flNextAlertTick <= gpGlobals->curtime )
	{
		m_flNextAlertTick = gpGlobals->curtime + 1;
		ProcessAlertBar();
	}

#if defined ( ENABLE_GIFT_PANEL )
	ProcessGiftPanel();
#endif
}


void SFUniqueAlerts::OnTimeJump()
{
	m_flNextAlertTick = -1; // refresh notices on next Think
}

void SFUniqueAlerts::ShowDMBonusWeapon( CEconItemView* pItem, int nPos, int nSecondsLeft )
{
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return;

	WITH_SFVALUEARRAY_SLOT_LOCKED( data, 5 )
	{
		m_pScaleformUI->ValueArray_SetElement( data, 0, pItem->GetStaticData()->GetDefinitionName() );
		m_pScaleformUI->ValueArray_SetElement( data, 1, g_pVGuiLocalize->Find( pItem->GetStaticData()->GetItemBaseName() ) );
		m_pScaleformUI->ValueArray_SetElement( data, 2, nSecondsLeft );
		m_pScaleformUI->ValueArray_SetElement( data, 3, m_nDMCurrentWeaponPoints );
		m_pScaleformUI->ValueArray_SetElement( data, 4, m_nDMCurrentWeaponBonusPoints );
		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showDMBonusWeapon", data, 5 );
	}

	CBaseCombatWeapon *pOwnedWeapon = pLocalPlayer->Weapon_GetPosition( ( loadout_positions_t )nPos );

	// If we already have the bonus weapon, switch to it.
	if ( pOwnedWeapon || pLocalPlayer->CanPlayerBuy( false ) )
		GetCenterPrint()->Print( "#SFUI_Notice_DM_BonusSwitchTo" );
	else
		GetCenterPrint()->Print( "#SFUI_Notice_DM_BonusRespawn" );

	C_RecipientFilter filter;
	filter.AddRecipient( pLocalPlayer );
	C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "UI.DeathMatchBonusAlertStart" );
}

void SFUniqueAlerts::ShowQuestProgress(uint32 numPointsEarned, uint32 numPointsTotal, uint32 numPointsMax, const char* weaponName, const char* questDesc)
{
#if MISSIONPANEL_ENABLE_QUEST_PROGRESSION
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if (!pLocalPlayer)
		return;

	//function showQuestProgress( itemName:String, questDesc:String, nProgress:Number, nMaxProgress:Number, nPoints:Number)
	WITH_SFVALUEARRAY_SLOT_LOCKED(data, 5)
	{
		m_pScaleformUI->ValueArray_SetElement(data, 0, weaponName);
		m_pScaleformUI->ValueArray_SetElement(data, 1, questDesc);
		m_pScaleformUI->ValueArray_SetElement(data, 2, (int)numPointsTotal);
		m_pScaleformUI->ValueArray_SetElement(data, 3, (int)numPointsMax);
		m_pScaleformUI->ValueArray_SetElement(data, 4, (int)numPointsEarned);
		g_pScaleformUI->Value_InvokeWithoutReturn(m_FlashAPI, "showQuestProgress", data, 5);
	}

	// TODO: Better sound effects?
	C_RecipientFilter filter;
	filter.AddRecipient(pLocalPlayer);
	C_BaseEntity::EmitSound(filter, SOUND_FROM_WORLD, "UI.DeathMatchBonusAlertStart");
#endif
}


void SFUniqueAlerts::LevelInit( void )
{
	if ( !FlashAPIIsValid() )
	{
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFUniqueAlerts, this, UniqueAlerts );
	}
	else
	{
		WITH_SLOT_LOCKED
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onLevelReset", NULL, 0 );
		}
	}

	// New level, reset everything
	m_AlertState = ALERTSTATE_HIDDEN;
	m_flNextAlertTick = -1;

	m_bShowDemolitionProgressionPanel = false;

	m_bDMBonusIsActive = false;
	m_flNextDMBonusTick = -1;
	m_nDMCurrentWeaponPoints = 0;
	m_nDMCurrentWeaponBonusPoints = 0;
	m_nDMBonusWeaponSlot = -1;

	m_bMissionVisible = false;
	m_guardianPanelInitialized = false;
	m_guardianPanelHideTick = -1;
	m_nGuardianKills = 0;
	m_nGuardianWeapon = -1;
	m_nPlayerMoney = 0;
	m_bGuardianHasWeapon = false;
	m_bGuardianHasWeaponEquipped = false;

	m_bQuestMissionActive = false;

#if defined ( ENABLE_GIFT_PANEL )
	m_bIsGiftPanelActive = false;
	m_nGlobalGiftsGivenInPeriod = 0;
	m_nGlobalNumGiftersInPeriod = 0;
	m_nGlobalGiftingPeriodSeconds = 0;
	m_flLastGiftPanelUpdate = 0;
	m_PlayerSpotlightTempList.Purge();
#endif
}

void SFUniqueAlerts::LevelShutdown( void )
{
	if ( FlashAPIIsValid() )
	{
		RemoveFlashElement();
	}
}

bool SFUniqueAlerts::ShouldDraw( void )
{
	bool bTrainingMode = CSGameRules() && CSGameRules()->IsPlayingTraining();

	return
		cl_drawhud.GetBool()
		&& cl_draw_only_deathnotices.GetBool() == false
		&& !IsTakingAFreezecamScreenshot()
		&& !bTrainingMode
		&& CHudElement::ShouldDraw();
}

void SFUniqueAlerts::SetActive( bool bActive )
{
	if ( FlashAPIIsValid() )
	{
		if ( bActive != m_bVisible )
		{
			if(bActive)
			{
				Show();
			}
			else
			{
				Hide();
			}
		}
	}

	CHudElement::SetActive( bActive );
}

void SFUniqueAlerts::FlashReady( void )
{
	if ( m_FlashAPI && m_pScaleformUI )
	{	
		ListenForGameEvent( "player_spawn" );
		ListenForGameEvent( "round_start" );
		ListenForGameEvent( "round_announce_final" );
		ListenForGameEvent( "round_announce_match_point" );
		ListenForGameEvent( "round_announce_last_round_half" );		
		ListenForGameEvent( "round_announce_match_start" );
		ListenForGameEvent( "round_announce_warmup" );	
		ListenForGameEvent( "dm_bonus_weapon_start" );
		ListenForGameEvent( "gg_killed_enemy" );

	}
}

bool SFUniqueAlerts::PreUnloadFlash( void )
{
	return true;
}

void SFUniqueAlerts::FireGameEvent( IGameEvent *event )
{
	if ( !FlashAPIIsValid() )
		return;

	const char *type = event->GetName();
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return;

	int EventUserID = event->GetInt( "userid", -1 );
	int LocalPlayerID = ( pLocalPlayer != NULL ) ? pLocalPlayer->GetUserID() : -2;

	if ( Q_strcmp( "round_announce_match_start", type ) == 0 )
	{
		ShowAlertText( g_pVGuiLocalize->Find( "#SFUI_Notice_Alert_Match_Start" ), true );
		
		C_RecipientFilter filter;
		filter.AddRecipient( pLocalPlayer );
		C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "Music.Match_Start_Stinger" );
	}
	else if ( Q_strcmp( "round_start", type ) == 0 )
	{
		if ( CSGameRules() && CSGameRules()->IsPlayingGunGameTRBomb()/* && !pLocalPlayer->IsControllingBot()*/ )
		{
			m_bShowDemolitionProgressionPanel = true;
		}
		
		if ( pLocalPlayer->IsHLTV() )
			HideAlertText();

		m_flNextAlertTick = -1;

		if ( !CSGameRules()->IsFreezePeriod() && !CSGameRules()->IsWarmupPeriod() )
		{
			WITH_SLOT_LOCKED
			{
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "HidePanelAndItemNumbers", NULL, 0 );	
			}
		}
	}
	else if (	Q_strcmp( "round_announce_final", type ) == 0 || 
				Q_strcmp( "round_announce_last_round_half", type ) == 0 || 
				Q_strcmp( "round_announce_match_point", type ) == 0 ||
				Q_strcmp( "player_spawn", type ) == 0 )
	{
		if ( Q_strcmp( "round_announce_final", type ) == 0 )
		{
			ShowAlertText( g_pVGuiLocalize->Find( "#SFUI_Notice_Alert_Final_Round" ), true );

			C_RecipientFilter filter;
			filter.AddRecipient( pLocalPlayer );
			C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "Music.Final_Round_Stinger" );
		}
		else if ( Q_strcmp( "round_announce_match_point", type ) == 0 )
		{
			ShowAlertText( g_pVGuiLocalize->Find( "#SFUI_Notice_Alert_Match_Point" ), true );

			C_RecipientFilter filter;
			filter.AddRecipient( pLocalPlayer );
			C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "Music.Match_Point_Stinger" );
		}
		else if ( Q_strcmp( "round_announce_last_round_half", type ) == 0 )
		{
			ShowAlertText( g_pVGuiLocalize->Find( "#SFUI_Notice_Alert_Last_Round_Half" ), true );
		}
		else if ( Q_strcmp( "player_spawn", type ) == 0 && EventUserID == LocalPlayerID )
		{
			HideAlertText();
		}
	}
	else if ( Q_strcmp( "round_announce_warmup", type ) == 0 )
	{
		HideAlertText();
	}
	else if ( Q_strcmp( "dm_bonus_weapon_start", type ) == 0 )
	{
		// don't do the initial show if we are set not to draw right now
		// we'll fill the data out during the ProcessInput
		if ( ShouldDraw() )
		{
			int nTime = event->GetInt( "time", 0 );
			//	int nWepID = event->GetInt( "wepID", -1 );
			int nPos = event->GetInt( "Pos", 0 );

			CCSPlayerInventory* pPlayerInv = CSInventoryManager()->GetInventoryForPlayer( ClientSteamContext().GetLocalPlayerSteamID() );
			if ( !pPlayerInv )
				return;

			m_nDMBonusWeaponSlot = nPos;
			//CSGameRules()->GetDMBonusWeaponLoadoutSlot()

			CEconItemView* pItem = pPlayerInv->GetItemInLoadout( C_CSPlayer::GetLocalCSPlayer()->GetTeamNumber(), nPos );
			if ( !pItem )
				return;

			m_bDMBonusIsActive = true;
			m_flNextDMBonusTick = Plat_FloatTime() + 0.25f;

			if ( CSGameRules() )
			{
				m_nDMCurrentWeaponPoints = CSGameRules()->GetWeaponScoreForDeathmatch( nPos );
				m_nDMCurrentWeaponBonusPoints = ( ( float )( mp_dm_bonus_percent.GetInt() ) / 100.0f ) * m_nDMCurrentWeaponPoints;
			}

			ShowDMBonusWeapon( pItem, nPos, nTime );
		}
	}
	else if ( Q_strcmp( "gg_killed_enemy", type ) == 0 )
	{
		if ( CSGameRules()->IsPlayingGunGame() && pLocalPlayer && pLocalPlayer->GetUserID() == event->GetInt( "attackerid" ) )
		{
			if ( event->GetInt( "bonus" ) )
			{
				WITH_SLOT_LOCKED
				{
					g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "registerBonusKill", NULL, 0 );
				}
			}
		}
	}
}

void SFUniqueAlerts::Show( void )
{
	if ( !m_pScaleformUI || !m_FlashAPI )
		return;

	m_bVisible = true;

#if defined ( ENABLE_GIFT_PANEL )
	m_flLastGiftPanelUpdate = gpGlobals->curtime;
#endif

	ScaleformDisplayInfo dinfo;
	m_pScaleformUI->Value_GetDisplayInfo( m_FlashAPI, &dinfo );
	dinfo.SetVisibility( true );
	m_pScaleformUI->Value_SetDisplayInfo( m_FlashAPI, &dinfo );
}

void SFUniqueAlerts::Hide( void )
{
	m_bVisible = false;

	if ( !m_pScaleformUI || !m_FlashAPI)
		return;

	HideAlertText();

	//Msg( "------------------------ SFUniqueAlerts::Hide\n" ); 

	ScaleformDisplayInfo dinfo;
	m_pScaleformUI->Value_GetDisplayInfo( m_FlashAPI, &dinfo );
	dinfo.SetVisibility( false );
	m_pScaleformUI->Value_SetDisplayInfo( m_FlashAPI, &dinfo );
}

void SFUniqueAlerts::Reset( void )
{
}

void SFUniqueAlerts::UpdateGGWeaponIconList( void )
{
	C_CSPlayer *pPlayer = (C_CSPlayer::GetLocalCSPlayer() && C_CSPlayer::GetLocalCSPlayer()->GetObserverTarget()) ? dynamic_cast<C_CSPlayer*>(C_CSPlayer::GetLocalCSPlayer()->GetObserverTarget()) : C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	int nMaxIndex = CSGameRules()->GetNumProgressiveGunGameWeapons( pPlayer->GetTeamNumber() );
	int nextWeaponID = -1;
	for ( int i = 0; i < nMaxIndex; i++ )
	{
		nextWeaponID = CSGameRules()->GetProgressiveGunGameWeapon( i, pPlayer->GetTeamNumber() );

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
			if ( !pWeaponInfo )
				continue;

			pchClassName = pWeaponInfo->szClassName;
			pchPrintName = pWeaponInfo->szPrintName;
		}

		bool bSelected = pPlayer->GetPlayerGunGameWeaponIndex() == i;

		WITH_SFVALUEARRAY_SLOT_LOCKED( data, 4 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, i );
			m_pScaleformUI->ValueArray_SetElement( data, 1, pchClassName );
			m_pScaleformUI->ValueArray_SetElement( data, 2, pchPrintName );
			m_pScaleformUI->ValueArray_SetElement( data, 3, bSelected );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setWeaponsIconsAndShow", data, 4 );
		}
	}
}

void SFUniqueAlerts::ShowWarmupAlertPanel( void )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer || !CSGameRules() || !m_pScaleformUI )
		return;

	if ( !m_bVisible || !m_bActive )
	{
		//DevMsg( "SFUniqueAlerts:ShowWarmupAlertPanel : RETURNING : m_bVisible == %s, m_bActive == %s\n", m_bVisible ? "true" : "false", m_bActive ? "true" : "false" );
		return;
	}

	float flEndTime = CSGameRules()->GetWarmupPeriodEndTime() - 0.5f;
	bool bIsRestarting = CSGameRules()->IsGameRestarting();

	int nTimeLeftInSec = (int)flEndTime - (int)gpGlobals->curtime;
	if ( nTimeLeftInSec > 0 )
	{
		int nMinLeft = nTimeLeftInSec / 60;
		int nSecLeft = nTimeLeftInSec - ( nMinLeft * 60 ); 

		wchar_t szNotice[64] = L"";
		wchar_t wzTime[8] = L"";
			
		if ( !CSGameRules()->IsWarmupPeriodPaused() )
		{
			V_swprintf_safe( wzTime, L"%d:%02d", nMinLeft, nSecLeft );
		}

		if ( nTimeLeftInSec <= 5 )
		{
			g_pVGuiLocalize->ConstructString( szNotice, sizeof( szNotice ), g_pVGuiLocalize->Find( "#SFUI_Notice_Alert_Warmup_Period_Ending" ), 1, wzTime );
			ShowAlertText( szNotice );

			if ( !bIsRestarting )
				pPlayer->EmitSound("Alert.WarmupTimeoutBeep");
		}
		else
		{
				
			if ( CSGameRules()->IsQueuedMatchmaking() )
			{
				// client-side UTIL_HumansInGame
				int nTotalPlayers = 0;
				for ( int i = 1; i <= gpGlobals->maxClients; i++ )
				{
					CCSPlayer *pPlayer = ( CCSPlayer* )UTIL_PlayerByIndex( i );
					if ( pPlayer && !pPlayer->IsSpectator() && !pPlayer->IsBot() )
						nTotalPlayers++;
				}

				int numHumansNeeded = 10;
				if ( CSGameRules()->IsPlayingCoopGuardian() || CSGameRules()->IsPlayingCoopMission() )
					numHumansNeeded = 2;

				if ( nTotalPlayers < numHumansNeeded )
				{
					g_pVGuiLocalize->ConstructString( szNotice, sizeof( szNotice ), g_pVGuiLocalize->Find( "#SFUI_Notice_Alert_Waiting_For_Players" ), 1, wzTime );
				}
				else
				{
					g_pVGuiLocalize->ConstructString( szNotice, sizeof( szNotice ), g_pVGuiLocalize->Find( "#SFUI_Notice_Alert_Warmup_Period" ), 1, wzTime );
				}
			}
			else
			{
				g_pVGuiLocalize->ConstructString( szNotice, sizeof( szNotice ), g_pVGuiLocalize->Find( "#SFUI_Notice_Alert_Warmup_Period" ), 1, wzTime );
			}

			ShowAlertText( szNotice );
		}
	}
}


void SFUniqueAlerts::ShowHltvReplayAlertPanel( int nHltvReplayLeft )
{
	wchar_t szNotice[ 128 ] = L"";
	wchar_t wzSecs[ 16 ] = L"";
	if ( IsDebug() )
	{
		Msg( "ShowHltvReplayAlertPanel\n" );
		V_swprintf_safe( wzSecs, L"%d @ frame %d", nHltvReplayLeft, gpGlobals->tickcount );
	}
	else if ( nHltvReplayLeft > 0 ) // 0 may mean we either haven't started streaming yet (time hasn't warped back yet), or have almost finished streaming (to the warped point or beyond), or the tick rate is invalid so our timeout computations are invalid, or that we're playing past the point we recorded as the end of playback... either way, there's no need to show that to the user
	{
		V_swprintf_safe( wzSecs, L"%d", nHltvReplayLeft );
	}

	g_pVGuiLocalize->ConstructString( szNotice, sizeof( szNotice ), g_pVGuiLocalize->Find( "#SFUI_Notice_Alert_Replaying" ), 1, wzSecs );
	ShowAlertText( szNotice );
}

void SFUniqueAlerts::ShowAlertText( const wchar_t *szMsg, bool oneShot )
{
	if ( !m_FlashAPI || !m_pScaleformUI )
		return;

	WITH_SFVALUEARRAY_SLOT_LOCKED( data, 1 )
	{
		m_pScaleformUI->ValueArray_SetElement( data, 0, szMsg );

		if ( oneShot )
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setAlertTextAndShow", data, 1 );
			m_AlertState = ALERTSTATE_HIDING; // animation will automatically hide at some point
		}
		else if ( m_AlertState == ALERTSTATE_SHOWING )
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "updateAlertText", data, 1 );
		}
		else // either HIDING or HIDDEN, need to switch to 'new message' animation
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setAlertTextAndShowPause", data, 1 );
			m_AlertState = ALERTSTATE_SHOWING;
		}
	}
}

void SFUniqueAlerts::HideAlertText()
{
	if(m_AlertState != ALERTSTATE_HIDDEN)
	{
		WITH_SFVALUEARRAY_SLOT_LOCKED( data, 1 )
		{
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hideFlash", NULL, 0 );
		}

		m_AlertState = ALERTSTATE_HIDING;
	}
}

// Scaleform callbacks
void SFUniqueAlerts::OnAlertPanelHideAnimEnd( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_AlertState = ALERTSTATE_HIDDEN;
}

#endif // INCLUDE_SCALEFORM
