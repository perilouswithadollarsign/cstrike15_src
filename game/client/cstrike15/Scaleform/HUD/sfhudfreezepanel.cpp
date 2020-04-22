//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include "sfhudfreezepanel.h"
#include "view.h"
//#include "iviewrender.h"
#include "hud_macros.h"
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include "iclientmode.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_element_helper.h"
#include "iclientmode.h"
#include "scaleformui/scaleformui.h"
#include "c_cs_playerresource.h"
#include "gameui_util.h"
#include "vstdlib/vstrtools.h"
#include "sfhudwinpanel.h"
#include "viewrender.h"
#include "steam/steam_api.h"
#include "inputsystem/iinputsystem.h"
#include "sfhudinfopanel.h"
#include "hltvreplaysystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern float g_flFreezeFlash[ MAX_SPLITSCREEN_PLAYERS ];
extern ConVar cl_draw_only_deathnotices;
ConVar spec_freeze_panel_replay_position( "spec_freeze_panel_replay_position", "0.75", FCVAR_CLIENTDLL );

float g_flFreezecamScreenshotFrameTimeStarted = 0;
bool g_bFreezecamScreenshotFrameNumAssigned = false;

#define NUM_FREEZE_CAM_BORDER_IMAGES	4

bool IsTakingAFreezecamScreenshot( void )
{
	// Don't draw in freezecam, or when the game's not running
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	bool bInFreezeCam = ( pPlayer && pPlayer->GetObserverMode() == OBS_MODE_FREEZECAM );

	if ( bInFreezeCam && g_bFreezecamScreenshotFrameNumAssigned &&
		( gpGlobals->curtime - g_flFreezecamScreenshotFrameTimeStarted < 1.0f ) && engine->IsTakingScreenshot() )
		return true;

	SFHudFreezePanel *pPanel = GET_HUDELEMENT( SFHudFreezePanel );
	if ( pPanel )
	{
		if ( pPanel->IsHoldingAfterScreenShot() )
			return true;
	}

	if ( g_bFreezecamScreenshotFrameNumAssigned )
		g_bFreezecamScreenshotFrameNumAssigned = false;

	return false;
}

void CS_FreezePanel_OnTimeJump()
{
	SFHudFreezePanel *pPanel = GET_HUDELEMENT( SFHudFreezePanel );
	if ( pPanel )
	{
		pPanel->OnTimeJump();
	}
}

void CS_FreezePanel_ResetDamageText( int iPlayerIndexKiller, int iPlayerIndexVictim )
{
	SFHudFreezePanel *pPanel = GET_HUDELEMENT( SFHudFreezePanel );
	if ( pPanel )
	{
		pPanel->ResetDamageText( iPlayerIndexKiller, iPlayerIndexVictim );
	}
}

void CS_FreezePanel_OnHltvReplayButtonStateChanged()
{
	SFHudFreezePanel *pPanel = GET_HUDELEMENT( SFHudFreezePanel );
	if ( pPanel )
	{
		pPanel->OnHltvReplayButtonStateChanged();
	}
}



DECLARE_HUDELEMENT( SFHudFreezePanel );

ConVar cl_disablefreezecam(
	"cl_disablefreezecam",
	"0",
	FCVAR_ARCHIVE,
	"Turn on/off freezecam on client"
	);

ConVar cl_freezecampanel_position_dynamic(
	"cl_freezecampanel_position_dynamic",
	"1",
	FCVAR_ARCHIVE,
	"Turn on/off freezecam's kill panel dynamic Y movement"
	);

SFUI_BEGIN_GAME_API_DEF
SFUI_END_GAME_API_DEF( SFHudFreezePanel, FreezePanel );

#define FREEZECAM_SCREENSHOT_STRING "got the upper hand!"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SFHudFreezePanel::SFHudFreezePanel( const char *value ) : SFHudFlashInterface( value )
{		
	SetHiddenBits( HIDEHUD_MISCSTATUS );
	
	for ( int i = 0 ; i < DominationIconMax ; ++i )
	{
		m_dominationIcons[i] = NULL;
	}
	m_dominationText1 = NULL;
	m_dominationText2 = NULL;
	m_killerName = NULL;
	m_freezePanel = NULL;
	m_FollowEntity = NULL;
	m_iKillerIndex = 0;
	m_navigationText = NULL;
	m_weaponInfoText1 = NULL;
	m_weaponInfoText2 = NULL;

	// screenshot
	m_ssDescText = NULL;
	m_ssNameText = NULL;

	m_PosX = 0;
	m_PosY = 0;

	g_flFreezeFlash[ GetSplitScreenPlayerSlot() ] = 0.0f;

	m_bDominationIconVisible = false;

	m_bIsVisible = false;
	m_bIsCancelPanelVisible = false;
	m_bFreezePanelStateRelevant = false;
}

void SFHudFreezePanel::LevelInit( void )
{
	Assert( !m_bFreezePanelStateRelevant );
	if ( !FlashAPIIsValid() )
	{
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFHudFreezePanel, this, FreezePanel );
	}
}

void SFHudFreezePanel::LevelShutdown( void )
{
	if ( FlashAPIIsValid() )
	{
		RemoveFlashElement();
	}
}

bool SFHudFreezePanel::ShouldDraw( void )
{
	return m_bIsVisible && cl_drawhud.GetBool() && cl_draw_only_deathnotices.GetBool() == false && CHudElement::ShouldDraw();
}


void SFHudFreezePanel::SetActive( bool bActive )
{
	CHudElement::SetActive( bActive );
}

void SFHudFreezePanel::FlashReady( void )
{
	if ( !m_FlashAPI )
	{
		return;
	}

	m_freezePanel = g_pScaleformUI->Value_GetMember( m_FlashAPI, "FreezePanel" );	
	if ( !m_freezePanel )
	{
		return;
	}

	SFVALUE root = g_pScaleformUI->Value_GetMember( m_freezePanel, "FreezePanel" );	

	//SafeReleaseSFVALUE( innerPanel );

	if ( !root )
	{
		return;
	}

	m_dominationText1 =				g_pScaleformUI->TextObject_MakeTextObjectFromMember( root, "DescriptionText" );	
	m_dominationText2 =				g_pScaleformUI->TextObject_MakeTextObjectFromMember( root, "DescriptionTextTitle" );
	m_killerName =					g_pScaleformUI->TextObject_MakeTextObjectFromMember( root, "NameText" );	
	m_dominationIcons[Nemesis] =	g_pScaleformUI->TextObject_MakeTextObjectFromMember( root, "NemesisIcon" );
	m_dominationIcons[Revenge] =	g_pScaleformUI->TextObject_MakeTextObjectFromMember( root, "RevengeIcon" );
	m_navigationText =				g_pScaleformUI->TextObject_MakeTextObjectFromMember( root, "Navigation" );	
	m_weaponInfoText1 =				g_pScaleformUI->TextObject_MakeTextObjectFromMember( root, "WeaponInfoText" );	
	m_weaponInfoText2 =				g_pScaleformUI->TextObject_MakeTextObjectFromMember( root, "WeaponCustomText" );

	SafeReleaseSFVALUE( root );

	// screenshot panel
	m_ssFreezePanel = g_pScaleformUI->Value_GetMember( m_FlashAPI, "FreezePanelSS" );	
	if ( !m_ssFreezePanel )
	{
		return;
	}
	else
	{
		m_ssDescText =				g_pScaleformUI->TextObject_MakeTextObjectFromMember( m_ssFreezePanel, "DescriptionText" );	
		m_ssNameText =				g_pScaleformUI->TextObject_MakeTextObjectFromMember( m_ssFreezePanel, "NameText" );	
	}

	SetIcon( None );

	// listen for events
	ListenForGameEvent( "show_freezepanel" );
	ListenForGameEvent( "hide_freezepanel" );	 
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "player_spawn" );	

	WITH_SLOT_LOCKED
	{
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hide", NULL, 0 );

		ScaleformDisplayInfo dinfo;
		m_pScaleformUI->Value_GetDisplayInfo( m_ssFreezePanel, &dinfo );
		dinfo.SetVisibility( false );
		m_pScaleformUI->Value_SetDisplayInfo( m_ssFreezePanel, &dinfo );
	}

	g_pScaleformUI->AddDeviceDependentObject( this );
}

bool SFHudFreezePanel::PreUnloadFlash( void )
{	
	StopListeningForAllEvents();

	for ( int i = 0 ; i < DominationIconMax ; ++i )
	{
		SafeReleaseSFTextObject( m_dominationIcons[i] );
	}
	SafeReleaseSFTextObject( m_dominationText1 );
	SafeReleaseSFTextObject( m_dominationText2 );
	SafeReleaseSFTextObject( m_killerName );
	SafeReleaseSFVALUE( m_freezePanel );
	SafeReleaseSFTextObject( m_navigationText );
	SafeReleaseSFTextObject( m_weaponInfoText1 );
	SafeReleaseSFTextObject( m_weaponInfoText2 );

	SafeReleaseSFTextObject( m_ssDescText );
	SafeReleaseSFTextObject( m_ssNameText );
	SafeReleaseSFVALUE( m_ssFreezePanel );

	g_pScaleformUI->RemoveDeviceDependentObject( this );
	return true;
}

void SFHudFreezePanel::FireGameEvent( IGameEvent* event )
{
	const char *pEventName = event->GetName();

	if ( Q_strcmp( "player_death", pEventName ) == 0 )
	{
		m_bDominationIconVisible = false;

		// see if the local player died
		int iPlayerIndexVictim = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
		int iPlayerIndexKiller = engine->GetPlayerForUserID( event->GetInt( "attacker" ) );		

		SF_SPLITSCREEN_PLAYER_GUARD();
		C_BasePlayer *pVictimPlayer = UTIL_PlayerByIndex( iPlayerIndexVictim ), *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		CCSPlayer* pKiller = ToCSPlayer( ClientEntityList().GetBaseEntity( iPlayerIndexKiller ) );
		m_FollowEntity = pKiller;

		CEconItemView *pItem = NULL;

		extern ConVar spec_replay_others_experimental;
		if ( !g_HltvReplaySystem.GetHltvReplayDelay() && ( spec_replay_others_experimental.GetBool() || ( pLocalPlayer && iPlayerIndexVictim == pLocalPlayer->entindex() ) ) )
		{
			// we need to notify the replay system that the replay is available before some other calls that happen  on this very event, so it would not be reliable to just have replay system listen to player_death event
			m_bHoldingAfterScreenshot = false;

			bool bShowItem = false;

			const char * szWeapon = event->GetString( "weapon" );
//			wchar_t *szWeaponHTML = 0;

			wchar_t wszWeaponNameHTML[64];
			wchar_t wszLocalizedString[256];

 			if ( szWeapon && szWeapon[0] != 0 )
 			{		
// 				if ( Q_strcmp( "knifegg", szWeapon ) == 0 )
// 					szWeapon = "knife";
// 
// 				char szWeaponTemp[64];
// 				if ( Q_strcmp( "knife", szWeapon ) == 0 )
// 				{
// 					if ( pKiller && pKiller->GetTeamNumber() == TEAM_CT )
// 					{
// 						szWeapon = "knife_default_ct";
// 					}
// 					else
// 					{
// 						szWeapon = "knife_default_t";
// 					}
// 				}
// 				else
// 				{
// 					V_snprintf( szWeaponTemp, sizeof(szWeaponTemp), "%s", szWeapon );
// 				}
// 
// 				wchar_t wszWeapon[64];
// 				V_UTF8ToUnicode( szWeaponTemp, wszWeapon, sizeof( wszWeapon ) );
// 				wchar_t wszTempWeaponIcon[64];
// 				V_snwprintf( wszTempWeaponIcon, 64, L"<img src='icon-%s.png'/>", wszWeapon );
// 				szWeaponHTML = wszTempWeaponIcon;

// 				pKiller = C_CSPlayer::GetLocalCSPlayer();
// 				if ( CSInventoryManager() && CSInventoryManager()->GetLocalInventory() )
// 				 	pItem = CSInventoryManager()->GetItemInLoadoutForTeam( pKiller->GetTeamNumber(), LOADOUT_POSITION_RIFLE3 );

				//get weapon name and custom name for the weapon info panel

				const char* szEventWeaponItemID = event->GetString( "weapon_itemid" );
				uint64 itemid = Q_atoui64( szEventWeaponItemID );

				const char* szEventWeaponFauxItemID = event->GetString( "weapon_fauxitemid" );
				uint64 fauxitemid = Q_atoui64( szEventWeaponFauxItemID );

				const char* szEventWeaponOriginalOwnerXuid = event->GetString( "weapon_originalowner_xuid" );
				uint64 ullWeaponOriginalOwnerXuid =  Q_atoui64( szEventWeaponOriginalOwnerXuid );

				if ( pKiller )
				{
					if ( !StringIsEmpty( szEventWeaponItemID ) )
					{
						// Get the real item if it's still in in the SO cache,
						pItem = CEconItemView::FindOrCreateEconItemViewForItemID( itemid );

						// if we don't have the item, see if the killer still has the item equiped and use that instead
						if ( !pItem && pKiller->GetActiveCSWeapon() && pKiller->GetActiveCSWeapon()->GetEconItemView() && pKiller->GetActiveCSWeapon()->GetEconItemView()->GetItemID() == itemid )
						{
							pItem = pKiller->GetActiveCSWeapon()->GetEconItemView();
						}

						//  otherwise fallback to the faux version
						if ( !pItem )
						{
							pItem = CEconItemView::FindOrCreateEconItemViewForItemID( fauxitemid );
						}
					}
					
// 					char szWeapon_Weapon[128];
// 					V_sprintf_safe( szWeapon_Weapon, "weapon_%s", event->GetString( "weapon" ) );
// 
// 					// collapse the various knives into the definition base name ( "knife_default_ct", ... )
// 					if ( !V_strncmp( szWeapon_Weapon, "weapon_knife", 12 ) )
// 					{
// 						V_sprintf_safe( szWeapon_Weapon, "weapon_knife" );
// 					}

					const char *pszLocString = NULL;

					if ( pItem && pItem->IsValid() )
					{
						const CEconItemRarityDefinition *pRarity = GetItemSchema()->GetRarityDefinition( pItem->GetRarity() );

						enum { kColorBufSize = 128 };
						wchar_t rwchColor[kColorBufSize];
						Q_UTF8ToUnicode( GetHexColorForAttribColor( pRarity->GetAttribColor() ), rwchColor, kColorBufSize );

						V_swprintf_safe( wszWeaponNameHTML, L"<font color=\"" PRI_WS_FOR_WS L"\">" PRI_WS_FOR_WS L"</font>", rwchColor, pItem->GetItemName() );

						bShowItem = ( pItem->GetItemID() != 0 );

					}
					else	// we have no econitemview. Print generic message.
					{
						const char * szWeapon = event->GetString( "weapon" );

						//pszLocString = "FreezePanel_Killer1_Weapon";
						if ( StringHasPrefixCaseSensitive( szWeapon, "prop_exploding_barrel" ) )	//"prop_exploding_barrel"
						{
							V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_WPNHUD_Exploding_Barrel" ) );
						}
						else
						{
							if ( StringHasPrefixCaseSensitive( szWeapon, "hegrenade" ) )
							{
								V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_WPNHUD_HE_Grenade" ) );
							}
							else if ( StringHasPrefixCaseSensitive( szWeapon, "flashbang" ) )
							{
								V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_WPNHUD_Flashbang" ) );
							}
							else if ( StringHasPrefixCaseSensitive( szWeapon, "decoy" ) )
							{
								V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_WPNHUD_Decoy" ) );
							}
							else if ( StringHasPrefixCaseSensitive( szWeapon, "smokegrenade" ) )
							{
								V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_WPNHUD_Smoke_Grenade" ) );
							}
							else if ( StringHasPrefixCaseSensitive( szWeapon, "incgrenade" ) )
							{
								V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_WPNHUD_IncGrenade" ) );
							}
							else if ( StringHasPrefixCaseSensitive( szWeapon, "molotov" ) )
							{
								V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_WPNHUD_Molotov" ) );
							}
							else if ( !V_strcmp( szWeapon, "inferno" ) )
							{
								pszLocString = "FreezePanel_Killer1_Weapon_Plural";
								V_swprintf_safe( wszWeaponNameHTML, L"" PRI_WS_FOR_WS, g_pVGuiLocalize->Find( "#SFUI_Inferno" ) );
							}
							else
							{
								g_pVGuiLocalize->ConvertANSIToUnicode( szWeapon, wszWeaponNameHTML, sizeof( wszWeaponNameHTML ) );
							}
						}
						
						bShowItem = false;
					}

					CSteamID pKillerID;
					pKiller->GetSteamID( &pKillerID );
					CSteamID pVictimID = steamapicontext->SteamUser()->GetSteamID();

					if ( !pszLocString )
					{
						if ( pVictimID.ConvertToUint64( ) == ullWeaponOriginalOwnerXuid )
						{
							pszLocString = "FreezePanel_Killer1_YourWeapon"; // Victim's weapon
						}
						else if ( pKillerID.ConvertToUint64( ) == ullWeaponOriginalOwnerXuid ||
							( ( ullWeaponOriginalOwnerXuid == 0 ) && pKiller->IsBot( ) ) )
						{
							pszLocString = "FreezePanel_Killer1_KillerWeapon"; // Killer's weapon
						}
						else if ( ullWeaponOriginalOwnerXuid != 0 )
						{
							pszLocString = "FreezePanel_Killer1_OthersWeapon"; // Someone else's weapon
						}
						else
						{
							pszLocString = "FreezePanel_Killer1_Weapon";
						}
					}

					if ( pszLocString && ullWeaponOriginalOwnerXuid )
					{
						/* Removed for partner depot */
						{
							pszLocString = "FreezePanel_Killer1_Weapon";
							g_pVGuiLocalize->ConstructString( wszLocalizedString, sizeof( wszLocalizedString ), g_pVGuiLocalize->Find(pszLocString), 1, wszWeaponNameHTML );
						}
					}
					else
					{
						g_pVGuiLocalize->ConstructString( wszLocalizedString, sizeof( wszLocalizedString ), g_pVGuiLocalize->Find(pszLocString), 1, wszWeaponNameHTML );
					}
				}
			}

			// the local player is dead, see if this is a new nemesis or a revenge
			if ( event->GetInt( "dominated" ) > 0 )
			{
				PopulateDominationInfo( Nemesis, "#FreezePanel_NewNemesis1", "#FreezePanel_NewNemesis2", L"" );
			}
			// was the killer your pre-existing nemesis?
			else if ( pKiller && pKiller->IsPlayerDominated( iPlayerIndexVictim ) )
			{
				PopulateDominationInfo( Nemesis, "#FreezePanel_OldNemesis1", "#FreezePanel_OldNemesis2", L"" );
			}			
			else if ( event->GetInt( "revenge" ) > 0 )
			{
				PopulateDominationInfo( Revenge, "#FreezePanel_Revenge1", "#FreezePanel_Revenge2", L"" );				
			}
			else if ( pKiller == pVictimPlayer || pKiller == NULL )
			{
				PopulateDominationInfo( None, "#FreezePanel_KilledSelf", "#FreezePanel_Killer2", L"" );
			}
			else
			{
				PopulateDominationInfo( None, "", "", wszLocalizedString );
			}

			// TODO: this is not the correct thing to do!  This assumes that the player's active weapon is the weapon that killed the other player, this is not always true!!!
			// this set the item panel
			{
				static char itemIDStr[256] = { 0 };
				itemIDStr[0] = 0;
				V_snprintf( itemIDStr, sizeof( itemIDStr ), "%llu", (pItem && pItem->IsValid() && pItem->GetItemID() > 0) ? pItem->GetItemID() : 0ull );

				C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );
				if (!cs_PR)
					return;

				static char xuidAsText[256] = { 0 };
				xuidAsText[0] = 0;

				if ( pKiller )
					cs_PR->FillXuidText( pKiller->entindex(), xuidAsText, sizeof( xuidAsText ) );

				static char szFauxItemId[255] = { 0 };

				itemid_t ullFauxItemId;

				if ( bShowItem && pItem )
				{
					ullFauxItemId = CombinedItemIdMakeFromDefIndexAndPaint( pItem->GetItemDefinition()->GetDefinitionIndex(), pItem->GetCustomPaintKitIndex() );
				}
				else
				{
					ullFauxItemId = 0ull;
				}

				V_snprintf( szFauxItemId, ARRAYSIZE(szFauxItemId), "%llu", ullFauxItemId );

				WITH_SFVALUEARRAY_SLOT_LOCKED( args, 4 )
				{
					m_pScaleformUI->ValueArray_SetElement( args, 0, bShowItem );
					m_pScaleformUI->ValueArray_SetElement( args, 1, itemIDStr );
					m_pScaleformUI->ValueArray_SetElement( args, 2, szFauxItemId );
					m_pScaleformUI->ValueArray_SetElement( args, 3, xuidAsText );
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetKillerItem", args, 4 );
				}
			}

			PopulateNavigationText();
		}
	}

	else if ( Q_strcmp( "player_spawn", pEventName ) == 0 )
	{
		int iUserid = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
		SF_SPLITSCREEN_PLAYER_GUARD();
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

		if ( pLocalPlayer && iUserid == pLocalPlayer->entindex() )
		{
			WITH_SLOT_LOCKED
			{
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "HideFreezeFrameBorder", NULL, 0 );
			}
		}
	}
	
	else if ( Q_strcmp( "hide_freezepanel", pEventName ) == 0 )
	{
		g_bFreezecamScreenshotFrameNumAssigned = false;
		m_bFreezePanelStateRelevant = false;

		if ( m_FlashAPI )
		{
			ShowPanel( false );

			m_bHoldingAfterScreenshot = false;
		}
	}
	else if ( Q_strcmp( "show_freezepanel", pEventName ) == 0 )
	{
		g_bFreezecamScreenshotFrameNumAssigned = true;
		g_flFreezecamScreenshotFrameTimeStarted = gpGlobals->curtime;

		SF_SPLITSCREEN_PLAYER_GUARD();
		int iVictimIndex = event->GetInt( "victim" );
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

		if ( pLocalPlayer && ( iVictimIndex == pLocalPlayer->entindex() ) )
		{
			C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );
			if ( !cs_PR )
				return;

			if ( m_FlashAPI )
			{
				bool bPauseBeforeAutoreplay = g_HltvReplaySystem.IsDelayedReplayRequestPending();
				ShowPanel( !bPauseBeforeAutoreplay );
				ShowCancelPanel( bPauseBeforeAutoreplay );
				m_bFreezePanelStateRelevant = true;
			}

			// Get the entity who killed us
			m_iKillerIndex = event->GetInt( "killer" );
			CCSPlayer* pKiller = ToCSPlayer( ClientEntityList().GetBaseEntity( m_iKillerIndex ) );	
			m_FollowEntity = pKiller;

			if ( pKiller )
			{
				int iKillerHealth = pKiller->GetHealth();
				if ( mp_forcecamera.GetInt() != OBS_ALLOW_ALL )
				{
					// we don't want to show the killer's health unless player can spectate killer
					// -1 lets the script know what we should hide it
					iKillerHealth = -1;
				}
				else if ( !pKiller->IsAlive() )
				{
					iKillerHealth = 0;
				}

				wchar_t wszkillerName[MAX_DECORATED_PLAYER_NAME_LENGTH];
				wszkillerName[0] = '\0';
				cs_PR->GetDecoratedPlayerName( m_iKillerIndex, wszkillerName, sizeof( wszkillerName ), k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot );

				if ( FlashAPIIsValid() )
				{
					if ( m_killerName && m_ssNameText )
					{
// 							// Truncate killer name
// 							wchar_t wszTruncatedKillerName[MAX_DECORATED_PLAYER_NAME_LENGTH];
// 							V_wcscpy_safe( wszTruncatedKillerName, wszkillerName );
// 							if ( m_bDominationIconVisible )
// 								TruncatePlayerName( wszTruncatedKillerName, ARRAYSIZE( wszTruncatedKillerName ), FREEZE_PANEL_NAME_TRUNCATE_AT_SHORT );
// 							else
// 								TruncatePlayerName( wszTruncatedKillerName, ARRAYSIZE( wszTruncatedKillerName ), FREEZE_PANEL_NAME_TRUNCATE_AT_LONG );

						WITH_SLOT_LOCKED
						{
							m_killerName->SetTextHTML( wszkillerName );
							m_ssNameText->SetTextHTML( wszkillerName );
						}

						WITH_SLOT_LOCKED
						{
							m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "NameTextUpdated", NULL, 0 );
						}
					}

					char xuidAsText[256] = { 0 };
					g_PR->FillXuidText( m_iKillerIndex, xuidAsText, sizeof( xuidAsText ) );

					int nHitsTaken = 0;
					int nDamTaken = 0;
					int nHitsGiven = 0;
					int nDamGiven = 0;

					static ConVarRef sv_damage_print_enable( "sv_damage_print_enable" );

					if ( sv_damage_print_enable.GetBool() )
					{
						nHitsTaken = event->GetInt( "hits_taken", 0 );
						nDamTaken = event->GetInt( "damage_taken", 0 );
						nHitsGiven = event->GetInt( "hits_given", 0 );
						nDamGiven = event->GetInt( "damage_given", 0 );
					}

					WITH_SFVALUEARRAY( avatarData, 8 )
					{
						m_pScaleformUI->ValueArray_SetElement( avatarData, 0, xuidAsText );
						m_pScaleformUI->ValueArray_SetElement( avatarData, 1, pKiller->GetTeamNumber() == TEAM_CT );
						m_pScaleformUI->ValueArray_SetElement( avatarData, 2, wszkillerName );
						m_pScaleformUI->ValueArray_SetElement( avatarData, 3, nDamTaken );
						m_pScaleformUI->ValueArray_SetElement( avatarData, 4, nHitsTaken );
						m_pScaleformUI->ValueArray_SetElement( avatarData, 5, nDamGiven );
						m_pScaleformUI->ValueArray_SetElement( avatarData, 6, nHitsGiven );
						m_pScaleformUI->ValueArray_SetElement( avatarData, 7, iKillerHealth );				
						m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showAvatar", avatarData, 8 );
					}
				}

				m_PosY = 0;

				bool bShouldShowBorder = false;
				int nNumBorderScreens = NUM_FREEZE_CAM_BORDER_IMAGES;
				if ( bShouldShowBorder )
				{
					int nIndex = (pKiller->entindex()%nNumBorderScreens) + 1;
					WITH_SFVALUEARRAY( data, 1 )
					{
						m_pScaleformUI->ValueArray_SetElement( data, 0, nIndex );
						m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowFreezeFrameBorder", data, 1 );

					}
				}
				else
				{
					WITH_SLOT_LOCKED
					{
						m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "HideFreezeFrameBorder", NULL, 0 );
					}
				}

			}
			else
			{
				// No specific killer (falling suicide can cause this), so set killer info to victim info
				if ( FlashAPIIsValid() )
				{
					wchar_t wszvictimName[MAX_DECORATED_PLAYER_NAME_LENGTH];
					wszvictimName[0] = '\0';
					cs_PR->GetDecoratedPlayerName( iVictimIndex, wszvictimName, sizeof( wszvictimName ), k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot );

					char xuidAsText[256] = { 0 };
					g_PR->FillXuidText( iVictimIndex, xuidAsText, sizeof( xuidAsText ) );

					CCSPlayer* pVictim = ToCSPlayer( ClientEntityList().GetBaseEntity( iVictimIndex ) );	

					WITH_SLOT_LOCKED
					{
						if ( m_killerName && m_ssNameText )
						{
							m_killerName->SetTextHTML( wszvictimName );
							m_ssNameText->SetTextHTML( wszvictimName );
						}

						if ( pVictim )
						{
							WITH_SFVALUEARRAY( avatarData, 3 )
							{
								m_pScaleformUI->ValueArray_SetElement( avatarData, 0, xuidAsText );
								m_pScaleformUI->ValueArray_SetElement( avatarData, 1, pVictim->GetTeamNumber() == TEAM_CT );
								m_pScaleformUI->ValueArray_SetElement( avatarData, 2, wszvictimName );
								m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showAvatar", avatarData, 3 );
							}
						}
					}
				}
			}
		}
	}
}

void SFHudFreezePanel::ShowPanel( bool bShow )
{
	if ( !m_FlashAPI )
		return;
	if ( bShow == false && m_bIsVisible )
	{
		// hide it
		WITH_SLOT_LOCKED
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hide", NULL, 0 );

			ScaleformDisplayInfo dinfo_freeze;
			m_pScaleformUI->Value_GetDisplayInfo( m_freezePanel, &dinfo_freeze );
			dinfo_freeze.SetVisibility( false );
			m_pScaleformUI->Value_SetDisplayInfo( m_freezePanel, &dinfo_freeze );

			ScaleformDisplayInfo dinfo;
			m_pScaleformUI->Value_GetDisplayInfo( m_ssFreezePanel, &dinfo );
			dinfo.SetVisibility( false );
			m_pScaleformUI->Value_SetDisplayInfo( m_ssFreezePanel, &dinfo );
		}
		m_bIsVisible = false;
	}
	else if ( bShow && m_bIsVisible == false )
	{
		// show it
		WITH_SLOT_LOCKED
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "show", NULL, 0 );

			ScaleformDisplayInfo dinfo_freeze;
			m_pScaleformUI->Value_GetDisplayInfo( m_freezePanel, &dinfo_freeze );
			dinfo_freeze.SetVisibility( true );
			m_pScaleformUI->Value_SetDisplayInfo( m_freezePanel, &dinfo_freeze );

			ScaleformDisplayInfo dinfo;
			m_pScaleformUI->Value_GetDisplayInfo( m_ssFreezePanel, &dinfo );
			dinfo.SetVisibility( false );
			m_pScaleformUI->Value_SetDisplayInfo( m_ssFreezePanel, &dinfo );
		}
		m_bIsVisible = true;
	}
}


void SFHudFreezePanel::ShowCancelPanel( bool bShow )
{
	if ( bShow != m_bIsCancelPanelVisible )
	{
		WITH_SLOT_LOCKED
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, bShow ? "ShowCancelText" : "HideCancelText", NULL, 0 );
		}
		m_bIsCancelPanelVisible = bShow;
	}
}

void SFHudFreezePanel::PopulateDominationInfo( DominationIconType iconType, const char* localizationToken1, const char* localizationToken2, wchar_t *szWeaponHTML )
{
	WITH_SLOT_LOCKED
	{
		SetIcon( iconType );

		if ( m_dominationText1 )
		{
			wchar_t wszCombined[256];

			V_snwprintf( wszCombined,
				ARRAYSIZE( wszCombined ),
#if defined(_PS3) || defined(POSIX)
				L"%ls %ls %ls",
#else
				L"%s %s %s",
#endif
				g_pScaleformUI->Translate( localizationToken1, NULL ),
				g_pScaleformUI->Translate( localizationToken2, NULL ), 
				szWeaponHTML ? szWeaponHTML : L""
				);


			m_dominationText1->SetTextHTML( wszCombined );

			/* Removed for partner depot */
			{
				m_ssDescText->SetTextHTML( wszCombined );
			}
			
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "HideFreezeFrameBorder", NULL, 0 );
		}	
	}
}

void SFHudFreezePanel::PopulateNavigationText( void )
{
	WITH_SLOT_LOCKED
	{
		if ( m_navigationText )
		{
			if ( IsTakingAFreezecamScreenshot() )
			{
				m_navigationText->SetTextHTML( "" );
			}
			else
			{
				if ( IsGameConsole() )
				{
					if ( g_pInputSystem->GetCurrentInputDevice() == INPUT_DEVICE_SHARPSHOOTER )
					{
						m_navigationText->SetTextHTML( "#SFUI_Freeze_Skip_Trigger" );
					}
					else
					{
						m_navigationText->SetTextHTML( "#SFUI_Freeze_Skip" );
					}
				}
				else
				{
					if ( g_HltvReplaySystem.GetHltvReplayDelay() )
					{
						if ( wchar_t *wzCancelReplay = g_pVGuiLocalize->Find( "#SFUI_Freeze_Cancel_Replay" ) )
						{
							m_navigationText->SetTextHTML( m_pScaleformUI->ReplaceGlyphKeywordsWithHTML( wzCancelReplay ) );
						}
					}
					else
					{
						wchar_t wzBind[ 16 ] = L"";
						wchar_t navBarText[ 256 ] = L"";
						UTIL_ReplaceKeyBindings( L"%jpeg%", 0, wzBind, sizeof( wzBind ) );
						g_pVGuiLocalize->ConstructString( navBarText, sizeof( navBarText ), g_pVGuiLocalize->Find( "#SFUI_Freeze_Snapshot" ), 1, wzBind );

						if ( g_HltvReplaySystem.IsHltvReplayButtonEnabled() && !g_HltvReplaySystem.IsDelayedReplayRequestPending() ) // no need to show "press F5 for replay" if we're about to show replay anyway
						{
							// we can also start replay right now out of freezecam
							if ( wchar_t *wcReplayLastKill = g_pVGuiLocalize->Find( "#SFUIHUD_Spectate_Navigation_Replay_Death" ) )
							{
								if ( wcReplayLastKill[ 0 ] != L'\0' )
								{
									V_wcscat( navBarText, L" ", sizeof( navBarText ) );
									V_wcscat( navBarText, wcReplayLastKill, sizeof( navBarText ) );
								}
							}
						}

						m_navigationText->SetTextHTML( m_pScaleformUI->ReplaceGlyphKeywordsWithHTML( navBarText ) );
					}
				}
			}	
		}	
	}
}

void SFHudFreezePanel::OnTimeJump( void )
{
	if ( m_FlashAPI )
	{
		WITH_SLOT_LOCKED
		{
			if ( g_HltvReplaySystem.GetHltvReplayDelay() )
			{
				if ( g_HltvReplaySystem.IsReplayingFinalKillOfRound() )
				{
					//m_ssDescText->SetTextHTML( g_pVGuiLocalize->Find( "FreezePanel_FinalKillOfTheRound" ) );
					//m_navigationText->SetTextHTML( g_pVGuiLocalize->Find( "FreezePanel_FinalKillOfTheRound" ) );
					m_dominationText1->SetTextHTML( g_pVGuiLocalize->Find( "FreezePanel_FinalKillOfTheRound" ) );
				}
				ShowPanel( true );
			}
			PopulateNavigationText();
		}
	}
}


void SFHudFreezePanel::ResetDamageText( int iPlayerIndexKiller, int iPlayerIndexVictim )
{
	C_BasePlayer *pKiller = UTIL_PlayerByIndex( iPlayerIndexKiller );
	C_BasePlayer *pVictim = UTIL_PlayerByIndex( iPlayerIndexVictim );
	if ( !pVictim || !pKiller )
		return;
	C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );
	if ( !cs_PR )
		return;

	char xuidAsText[ 256 ] = { 0 };
	g_PR->FillXuidText( m_iKillerIndex, xuidAsText, sizeof( xuidAsText ) );

	int nHitsTaken = pVictim->m_nLastKillerHitsTaken;
	int nDamTaken = pVictim->m_nLastKillerDamageTaken;
	int nHitsGiven = pVictim->m_nLastKillerHitsGiven;
	int nDamGiven = pVictim->m_nLastKillerDamageGiven;

	wchar_t wszkillerName[ MAX_DECORATED_PLAYER_NAME_LENGTH ];
	wszkillerName[ 0 ] = '\0';
	cs_PR->GetDecoratedPlayerName( m_iKillerIndex, wszkillerName, sizeof( wszkillerName ), k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot );

	WITH_SFVALUEARRAY( avatarData, 8 )
	{
		m_pScaleformUI->ValueArray_SetElement( avatarData, 0, xuidAsText );
		m_pScaleformUI->ValueArray_SetElement( avatarData, 1, pKiller->GetTeamNumber() == TEAM_CT );
		m_pScaleformUI->ValueArray_SetElement( avatarData, 2, wszkillerName );
		m_pScaleformUI->ValueArray_SetElement( avatarData, 3, nDamTaken );
		m_pScaleformUI->ValueArray_SetElement( avatarData, 4, nHitsTaken );
		m_pScaleformUI->ValueArray_SetElement( avatarData, 5, nDamGiven );
		m_pScaleformUI->ValueArray_SetElement( avatarData, 6, nHitsGiven );
		m_pScaleformUI->ValueArray_SetElement( avatarData, 7, pKiller->GetHealth() );
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showAvatar", avatarData, 8 );
	}
}

void SFHudFreezePanel::OnHltvReplayButtonStateChanged( void )
{
	if ( m_FlashAPI )
	{
		WITH_SLOT_LOCKED
		{
			PopulateNavigationText();
			bool bPauseBeforeAutoreplay = !g_HltvReplaySystem.GetHltvReplayDelay() && g_HltvReplaySystem.IsDelayedReplayRequestPending();
			ShowPanel( m_bFreezePanelStateRelevant && !bPauseBeforeAutoreplay );
			ShowCancelPanel( m_bFreezePanelStateRelevant && bPauseBeforeAutoreplay );
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// NOTE: 	This function is not threadsafe. If it is not called from ActionScript,
//			it needs to be called within a lock.
////////////////////////////////////////////////////////////////////////////////
void SFHudFreezePanel::SetIcon( DominationIconType iconType )
{
	for ( int i = 0 ; i < DominationIconMax ; ++i )
	{
		ISFTextObject* pIcon = m_dominationIcons[i];
		if ( pIcon )
		{
			pIcon->SetVisible( i == iconType );

			m_bDominationIconVisible = (iconType != 0);
		}
	}
}

void SFHudFreezePanel::DeviceReset( void *pDevice, void *pPresentParameters, void *pHWnd )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "RefreshAvatarImage", NULL, 0 );
		}
	}
}

void SFHudFreezePanel::ProcessInput( void )
{
	PositionPanel();
}

void SFHudFreezePanel::PositionPanel( void )
{
	if ( !m_freezePanel )
		return;

	ScaleformDisplayInfo dinfo;
	m_pScaleformUI->Value_GetDisplayInfo( m_freezePanel, &dinfo );
	if ( !dinfo.GetVisibility() || dinfo.GetAlpha() <= 0 )
		return;

	if ( cl_freezecampanel_position_dynamic.GetInt() > 0 )
	{
		SF_SPLITSCREEN_PLAYER_GUARD();
		int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

		C_CSPlayer *pPlayer = ToCSPlayer( C_BasePlayer::GetLocalPlayer() );
		if ( pPlayer == NULL )
			return;

		// TODO: get panels that this might overlap with, get their height and make sure they don't overlap
		// need to be able to get the height/width of a panel for this work to be done

		bool bWinPanelVisible = false;
		SFHudWinPanel * pWinPanel = GET_HUDELEMENT( SFHudWinPanel );
		if ( pWinPanel && pWinPanel->IsVisible() )
		{
			bWinPanelVisible = true;
		}

		float flInterp = pPlayer->GetFreezeFrameInterpolant();

		if ( g_HltvReplaySystem.GetHltvReplayDelay() )
		{
			// we're in replay, position the panel so that it plays nicely with the Replay framing
			m_PosX = ScreenWidth() / 2;
			m_PosY = ScreenHeight() * spec_freeze_panel_replay_position.GetFloat();
		}
		else if ( flInterp < 1.0f )
		{
			// Reposition the callout based on our target's position
			Vector vecTarget = pPlayer->GetRenderOrigin();
			CCSPlayer *pKiller = dynamic_cast<CCSPlayer*>( m_FollowEntity.Get() );
			if ( pKiller )
			{
				vecTarget = pKiller->EyePosition();
				vecTarget.z -= 50;
			}
			Vector vDelta = vecTarget - MainViewOrigin( nSlot );
			//float flDistance = vDelta.Length();
			VectorNormalize( vDelta );	// Only necessary so we can use it as part of our alpha calculation

			// Is the target visible on screen?
			int iX, iY;
			bool bOnscreen = GetVectorInScreenSpace( vecTarget, iX, iY );
			m_PosX = iX;

			bool bMsgBoxVisible = false;
			SFHudInfoPanel *pElement = dynamic_cast<SFHudInfoPanel*>( GetHud().FindElement( "SFHudInfoPanel" ) );
			if ( pElement )
				bMsgBoxVisible = pElement->IsVisible();

			// some nasty hardcoded numbers until we can get the height/width of the panels surrounding it
			int nMaxY = ScreenHeight() * 0.8;
			if ( bMsgBoxVisible )
			{
				m_PosY = ( ScreenHeight()*0.75 );
			}
			else if ( bWinPanelVisible || !bOnscreen  )
			{
				m_PosY = (ScreenHeight()*0.5);
			}
			else
			{
				m_PosY = clamp( iY, ScreenHeight()*0.55, nMaxY );
			}
		}

		int nPrevY = dinfo.GetY();

		int nCurY = Lerp( flInterp, nPrevY, m_PosY );
		//float flRot = Lerp( flInterp, 0.0f, -pPlayer->GetFreezeFrameTilt() );
		dinfo.SetY( nCurY );
		//dinfo.SetRotation( flRot );

		m_pScaleformUI->Value_SetDisplayInfo( m_freezePanel, &dinfo );
	}
	else
	{
		// let the script handle it
		WITH_SFVALUEARRAY_SLOT_LOCKED( panelData, 1 )
		{
			m_pScaleformUI->ValueArray_SetElement( panelData, 0, m_freezePanel );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "positionDeathPanel", panelData, 1 );
		}
	}

}

const char *SFHudFreezePanel::GetFilesafePlayerName( const char *pszOldName )
{
	if ( !pszOldName )
		return "";

	static char szSafeName[ MAX_PLAYER_NAME_LENGTH ];
	int nSafeNameBufSize = sizeof( szSafeName );
	int nNewPos = 0;

	for( const char *p = pszOldName; *p != 0 && nNewPos < nSafeNameBufSize-1; p++ )
	{
		if( *p == '.' )
		{
			szSafeName[ nNewPos ] = '-';
		}
		else if( *p == '/' )
		{
			szSafeName[ nNewPos ] = '-';
		}
		else if( *p == '\\' )
		{
			szSafeName[ nNewPos ] = '-';
		}
		else if( *p == ':' )
		{
			szSafeName[ nNewPos ] = '-';
		}
		else
		{
			szSafeName[ nNewPos ] = *p;
		}

		nNewPos++;
	}

	szSafeName[ nNewPos ] = 0;

	return szSafeName;
}

void SFHudFreezePanel::TakeFreezeShot( void )
{
	if ( ShouldDraw() )
	{
		// Establish the frame when we are starting freezecam shot
		g_bFreezecamScreenshotFrameNumAssigned = true;
		g_flFreezecamScreenshotFrameTimeStarted = gpGlobals->curtime;

		WITH_SLOT_LOCKED
		{
			//C_CSPlayer *pLocalPlayer = ToCSPlayer( C_BasePlayer::GetLocalPlayer() );
			//if ( EconHolidays_IsHolidayActive( kHoliday_Christmas ) && pLocalPlayer && pLocalPlayer->GetObserverTarget() && ( pLocalPlayer->GetObserverTarget() != pLocalPlayer ) )
			//{
			//	m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowFreezeFrameBorder", NULL, 0 );
			//}

			// hide the main freeze panel
			ScaleformDisplayInfo dinfo_freeze;
			m_pScaleformUI->Value_GetDisplayInfo( m_freezePanel, &dinfo_freeze );
			dinfo_freeze.SetVisibility( false );
			m_pScaleformUI->Value_SetDisplayInfo( m_freezePanel, &dinfo_freeze );

			ScaleformDisplayInfo dinfo;
			m_pScaleformUI->Value_GetDisplayInfo( m_ssFreezePanel, &dinfo );
			dinfo.SetVisibility( true );
			// move the target id to the corner
			if ( m_ssFreezePanel && dinfo.GetVisibility() && dinfo.GetAlpha() > 0 )
			{
				if ( cl_freezecampanel_position_dynamic.GetInt() > 0 )
				{
					// we don't know how big the panels are.......
					//m_pBasePanel->SetPos( ScreenWidth() - w, ScreenHeight() - h );

					C_CSPlayer *pPlayer = ToCSPlayer( C_BasePlayer::GetLocalPlayer() );
					if ( pPlayer == NULL )
						return;

					float flRot = -pPlayer->GetFreezeFrameTilt();
					dinfo.SetRotation( flRot );

					m_PosY = ScreenHeight() - (ScreenHeight() * 0.3);
					if ( flRot > 0 )
						m_PosY -= (ScreenHeight() * 0.2);

					dinfo.SetY( m_PosY );

					int nXPos = -(ScreenWidth() * 0.05);
					// widescreen
					if ( ScreenHeight()/ScreenWidth() < 0.75 )
						nXPos = -(ScreenWidth() * 0.01);

					dinfo.SetX( -(ScreenWidth() * 0.02) );

					dinfo.SetXScale( 200.0f * ((float)ScreenWidth()/1920.0f) );
					dinfo.SetYScale( 200.0f * ((float)ScreenWidth()/1920.0f) );

					m_pScaleformUI->Value_SetDisplayInfo( m_ssFreezePanel, &dinfo );
				}
				else
				{
					// let the script handle it
					WITH_SFVALUEARRAY( panelData, 1 )
					{
						m_pScaleformUI->ValueArray_SetElement( panelData, 0, m_ssFreezePanel );
						m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "positionScreenshotPanel", panelData, 1 );
					}
				}
			}
		}

		// Get the local player.
		C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pPlayer )
		{
			//Do effects
			g_flFreezeFlash[ GetSplitScreenPlayerSlot() ] = gpGlobals->curtime + 0.75f;
			pPlayer->EmitSound( "FreezeShot.TakeScreenshot" );

			//Extend Freezecam by a couple more seconds.
			engine->ClientCmd( "extendfreeze" );
			view->FreezeFrame( 3.0f );

			m_bHoldingAfterScreenshot = true;

			/*
			// Hide everything?
			if ( hud_freezecamhide.GetBool() )
			{
				SetVisible( false );
				DeleteCalloutPanels();
			}
			*/

			//Set the screenshot name
			if ( m_iKillerIndex <= MAX_PLAYERS )
			{
				const char *pszKillerName = g_PR->GetPlayerName( m_iKillerIndex );

				if ( pszKillerName )
				{
					ConVarRef cl_screenshotname( "cl_screenshotname" );

					if ( cl_screenshotname.IsValid() )
					{
						char szScreenShotName[512];

						Q_snprintf( szScreenShotName, sizeof( szScreenShotName ), "%s %s", GetFilesafePlayerName( pszKillerName ), FREEZECAM_SCREENSHOT_STRING );

						cl_screenshotname.SetValue( szScreenShotName );
					}
				}

				// clear the navigation text (space to skip)
				PopulateNavigationText();

				C_CSPlayer *pKiller = ToCSPlayer( UTIL_PlayerByIndex( m_iKillerIndex ) );
				if ( pKiller && !pKiller->IsBot() )
				{
					CSteamID steamID;
					if ( pKiller->GetSteamID( &steamID ) && steamID.IsValid() )
					{
						ConVarRef cl_screenshotusertag( "cl_screenshotusertag" );
						if ( cl_screenshotusertag.IsValid() )
						{
							cl_screenshotusertag.SetValue( (int)steamID.GetAccountID() );
						}
					}
				}
			}
		}
	}
}

#endif // INCLUDE_SCALEFORM
