//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_element_helper.h"
#include "iclientmode.h"
#include "view.h"
#include "vgui_controls/Controls.h"
#include "vgui/ISurface.h"
#include "ivrenderview.h"
#include "scaleformui/scaleformui.h"
#include "sfhudreticle.h"
#include "vgui/ILocalize.h"
#include "c_cs_hostage.h"
#include "c_cs_player.h"
#include "c_cs_playerresource.h"
#include "engineinterface.h"
#include "interfaces/interfaces.h"
#include "matchmaking/imatchframework.h"
#include "matchmaking/iplayermanager.h"
#include "gameui_util.h"
#include "c_plantedc4.h"
#include "sfhudfreezepanel.h"
#include "inputsystem/iinputsystem.h"
#include "voice_status.h"
#include "basepanel.h"
#include "cs_shareddefs.h"
#include "hltvcamera.h"
#include "cs_hud_weaponselection.h"
#include "HUD/sfhud_teamcounter.h"
#include "hltvreplaysystem.h"
#if !defined (NO_STEAM)
#include "steam/steam_api.h"
#endif

#ifdef SIXENSE
#include "sixense/in_sixense.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//ConVar sfcrosshair( "sfcrosshair", "1", FCVAR_ARCHIVE  | FCVAR_SS );
ConVar crosshair( "crosshair", "1", FCVAR_ARCHIVE  | FCVAR_SS );
ConVar cl_observercrosshair( "cl_observercrosshair", "1", FCVAR_ARCHIVE  | FCVAR_SS );
ConVar cl_fixedcrosshairgap( "cl_fixedcrosshairgap", "3", FCVAR_ARCHIVE  | FCVAR_SS, "How big to make the gap between the pips in the fixed crosshair" );
ConVar lockMoveControllerRet( "lockMoveControllerRet", "0", FCVAR_ARCHIVE );
static ConVar hud_showtargetid( "hud_showtargetid", "1", FCVAR_ARCHIVE  | FCVAR_SS, "Enables display of target names" );

//extern ConVar cl_dynamiccrosshair;
extern ConVar cl_teamid_overhead;
extern ConVar cl_teamid_overhead_maxdist;
extern ConVar cl_teamid_overhead_maxdist_spec;
extern ConVar voice_icons_method;
extern ConVar cl_crosshairstyle;
extern ConVar spec_show_xray;
extern ConVar cl_draw_only_deathnotices;
extern ConVar mp_hostages_takedamage;
static ConVar cl_teamid_overhead_name_alpha( "cl_teamid_overhead_name_alpha", "100", FCVAR_ARCHIVE  | FCVAR_SS, "The max alpha the overhead ID names will draw as." );
static ConVar cl_teamid_overhead_name_fadetime( "cl_teamid_overhead_name_fadetime", "1.0", FCVAR_ARCHIVE  | FCVAR_SS, "How long it takes for the overhad name to fade out once your crosshair has left the target." );
static ConVar mc_use_recoil_on_cursor( "mc_use_recoil_on_cursor", "0", 0 );



void fnTeamIDOverheadAlwaysCallback( IConVar *pConVar, const char *pOldValue, float flOldValue )
{
	ConVarRef var( pConVar );

	( GET_HUDELEMENT( SFHudReticle ) )->ToggleTeamEquipmentVisibility( var.GetInt() == 2 );
}

static ConVar cl_teamid_overhead_always( "cl_teamid_overhead_always", "0", FCVAR_ARCHIVE | FCVAR_RELEASE,"Always show team id over teammates. 1 = pips; 2 = pips, name, and equipment", fnTeamIDOverheadAlwaysCallback );


#define MAX_PLAYER_NAME_ID_PANELS 16

enum ID_TEXT_INDEX
{
	IDTEXT_NONE,
	IDTEXT_FOE,
	IDTEXT_FRIEND,
	IDTEXT_HOSTAGE
};

DECLARE_HUDELEMENT( SFHudReticle );

void IN_ShowTeamEquipmentDown( const CCommand &args )
{
	( GET_HUDELEMENT( SFHudReticle ) )->ToggleTeamEquipmentVisibility( true );
}

void IN_ShowTeamEquipmentUp( const CCommand &args )
{
	( GET_HUDELEMENT( SFHudReticle ) )->ToggleTeamEquipmentVisibility( false );
}

static ConCommand ShowAllTargetIDs( "+cl_show_team_equipment", IN_ShowTeamEquipmentDown );
static ConCommand UnShowAllTargetIDs( "-cl_show_team_equipment", IN_ShowTeamEquipmentUp );




SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD_AS( OnSwapReticle, "SwapReticle" ),
SFUI_END_GAME_API_DEF( SFHudReticle, Reticle );


SFHudReticle::SFHudReticle( const char *value ) : SFHudFlashInterface( value ),
	m_WeaponCrosshairHandle( NULL ),
	m_TopPip( NULL ),
	m_BottomPip( NULL ),
	m_LeftPip( NULL ),
	m_RightPip( NULL ),
	m_topCrosshairArc( NULL ),
	m_rightCrosshairArc( NULL ),
	m_leftCrosshairArc( NULL ),
	m_bottomCrosshairArc( NULL ),
	m_FriendCrosshair( NULL ),
	m_crosshairDot( NULL ),
	m_blackRing( NULL ),	
	m_IDText( NULL ),
	m_IDMovie( NULL ),
	m_FlashedIcon( NULL ),
	m_fIDTimer( 0.0f ),
	m_bTextIDVisible( false ),
	m_iReticleMode( RETICLE_MODE_NONE ),
	m_bFriendlyCrosshairVisible( true ),
	m_bEnemyCrosshairVisible( false ),
	m_bFlashedIconFadingOut(true),
	m_iLastGap( -1 ),
	m_iLastSpread( -1 ),
	m_dotX( 0.0f ),
	m_dotY( 0.0f ),
	m_blackRingX( 0.0f ),
	m_blackRingY( 0.0f ),
	m_friendIndicatorX( 0.0f ),
	m_friendIndicatorY( 0.0f ),
	m_IDMovieX( 0.0f ),
	m_IDMovieY( 0.0f ),
	m_bCrosshairPositionsInitialized( false )
{
	// TODO Auto-generated constructor stub
	SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_CROSSHAIR );
	m_wcIDString[0] = 0;
    m_bWantLateUpdate = true;
}


SFHudReticle::~SFHudReticle()
{
	// TODO Auto-generated destructor stub
}



void SFHudReticle::ShowReticle( RETICLE_MODE mode, bool value )
{
	SFVALUE handle = 0;

	switch( mode )
	{
		case RETICLE_MODE_WEAPON:
			handle = m_WeaponCrosshairHandle;
			break;

		case RETICLE_MODE_OBSERVER:
			handle = m_ObserverCrosshairHandle;
			break;

		default:
			break;
	}

	if ( handle )
	{
		m_pScaleformUI->Value_SetVisible( handle, value );
	}

}

void SFHudReticle::LockSlot( bool wantItLocked, bool& currentlyLocked )
{
	if ( currentlyLocked != wantItLocked )
	{
		if ( wantItLocked )
			LockScaleformSlot();
		else
			UnlockScaleformSlot();
		currentlyLocked = wantItLocked;
	}
}

extern ConVar weapon_debug_spread_show;

void SFHudReticle::ProcessInput( void )
{
	if ( !FlashAPIIsValid() )
		return;

	SF_SPLITSCREEN_PLAYER_GUARD();

	bool bShowFriendlyCrosshair = false;
	RETICLE_MODE iDesiredReticleMode = RETICLE_MODE_NONE;

	wchar_t wcNewString[ TEXTFIELD_LENGTH ];
	wcNewString[0] = 0;


	C_CSPlayer *pCSPlayer = GetHudPlayer();
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	// do crosshair calculations first because they can affect the
	// display of the target id

	bool bWantWeaponShown = false;
	bool bHasWeapon = false;
	bool bPlayerInInputMode = false;
	bool bShowCrosshair = false;
	bool bShowFriendEnemyDesignation = false;
	bool bFriendlyCrosshairOkay = false;
	bool bShowDroppedWeaponNames = false;
	bool bCheckWeaponRange = false;
	bool bWantEnemyCrosshair = false;
	int iDesiredGap = -1;
	int iDesiredSpread = -1;
	float fDesiredFishtail = 0.0f;
	int iObsMode = OBS_MODE_NONE;

	/************************************
	 * the logic below is complicated, but here's what it means:
	 * 1 - If we're not in spectator mode, then only show a crosshair if we're not in an input mode
	 *     and we have a weapon ( normally true ) and the weapon wants a crosshair.
	 * 2 - If we're in spectator in eye mode, show a crosshair if the weapon wants it
	 *     but don't show the friendly target icon
	 * 3 - If we're in spectator roaming mode, show the spectator crosshair
	 * 4 - Otherwise show no crosshair
	 */

	if ( pLocalPlayer && crosshair.GetBool() )
	{
		iObsMode = pLocalPlayer->GetObserverMode();

		switch( iObsMode )
		{
			case OBS_MODE_IN_EYE:
			case OBS_MODE_ROAMING:
				bShowCrosshair = true;
				break;

			case OBS_MODE_NONE:
				bFriendlyCrosshairOkay = true;
				bShowDroppedWeaponNames = true;
				bShowCrosshair = true;
				bShowFriendEnemyDesignation = true;
				break;

			default:
				bShowCrosshair = false;
				break;
		}

#ifdef IRONSIGHT
		CWeaponCSBase *pCSWeapon = pLocalPlayer->GetActiveCSWeapon();
		if ( pCSWeapon && pCSWeapon->GetIronSightController() && !weapon_debug_spread_show.GetBool() )
		{
			bShowCrosshair = !pCSWeapon->GetIronSightController()->ShouldHideCrossHair();
		}
#endif //IRONSIGHT

		if ( bShowCrosshair && iObsMode != OBS_MODE_ROAMING )
		{
			if ( pLocalPlayer->IsInVGuiInputMode() || pLocalPlayer->IsInViewModelVGuiInputMode() )
				bPlayerInInputMode = true;
			else
			{
				CWeaponCSBase *pWeapon = ( CWeaponCSBase* )pCSPlayer->GetActiveWeapon();

				if ( pWeapon )
				{
					bHasWeapon = true;

					if ( pWeapon->WantReticleShown() )
					{
						if ( pWeapon->GetCSWeaponID() == WEAPON_TASER )
						{
							// When using the taser we don't want to change the reticle color until the
							// target is in range.
							bCheckWeaponRange = true;
						}

						bWantWeaponShown = true;
						iDesiredSpread = pWeapon->GetReticleWeaponSpread();
						iDesiredGap = pWeapon->GetReticleCrosshairGap();
						fDesiredFishtail = pWeapon->GetAccuracyFishtail();
					}
				}
			}
		}

		// the reticle mode is defaulted to NONE above (outside this clause) so
		// we don't have to explicitly sent none

		if ( bShowCrosshair )
		{
			if ( iObsMode == OBS_MODE_ROAMING )
			{
				iDesiredReticleMode = RETICLE_MODE_OBSERVER;
			}
			else if ( pLocalPlayer && !bPlayerInInputMode && bHasWeapon && bWantWeaponShown )
			{
				iDesiredReticleMode = RETICLE_MODE_WEAPON;
			}
		}
	}

	bool bShowBlankXHairID = false;
	bool bIsHostage = false;
	bool bLocalIsSpectatorViewer = CanSeeSpectatorOnlyTools();
	bool bForceShowCenterID = false;
	bool displayingTargetID = false;

	bool bIsFullyBlinded = pCSPlayer && pCSPlayer->m_flFlashOverlayAlpha >= 180.0f;
	bool bIsFlashed = pCSPlayer && pCSPlayer->m_flFlashBangTime > ( gpGlobals->curtime+0.5 );
	if ( m_FlashedIcon && !cl_draw_only_deathnotices.GetBool() )
	{
		if ( pCSPlayer->m_flFlashOverlayAlpha > 0 && bLocalIsSpectatorViewer )
		{
			float flFlashFrac = 1.0 - clamp( pCSPlayer->m_flFlashOverlayAlpha / 210, 0, 1 );
			// show flashed
			WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, flFlashFrac );

				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashedIcon, "FlashFade", args, 1 );
			}
			m_bFlashedIconFadingOut = true;
		}
		else if ( m_bFlashedIconFadingOut )
		{
			WITH_SLOT_LOCKED
			{
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashedIcon, "Hide", NULL, 0 );
			}
			m_bFlashedIconFadingOut = false;
		}
	}

	if ( hud_showtargetid.GetBool() )
	{
		// if we're still blinded, then leave everything just like it is
		if ( !bLocalIsSpectatorViewer && bIsFlashed )
			return;

		// if we're spectating and the player we're observing is fully blind, say so in the target id
		if ( iObsMode == OBS_MODE_IN_EYE && bIsFullyBlinded && !cl_draw_only_deathnotices.GetBool() )
		{
			displayingTargetID = true;
			g_pVGuiLocalize->ConstructString( wcNewString, sizeof( wcNewString ), g_pVGuiLocalize->Find( "#SFUIHUD_targetid_FLASHED" ), 0 );
		}
		else if ( ( iDesiredReticleMode != RETICLE_MODE_NONE ) || ( pCSPlayer->GetFOV() != pCSPlayer->GetDefaultFOV() ) )
		{
			// Get our target's ent index
			int iEntIndex = hud_showtargetid.GetBool() ? pCSPlayer->GetIDTarget() : 0;

			if ( iEntIndex )
			{
				C_CSPlayer *pPlayer = static_cast<C_CSPlayer*>( cl_entitylist->GetEnt( iEntIndex ) );
				C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

				const char *printFormatString = NULL;
				wchar_t wszClanTag[ MAX_PLAYER_NAME_LENGTH ];
				wchar_t wszPlayerName[ MAX_DECORATED_PLAYER_NAME_LENGTH ];
				wchar_t wszHealthText[ 10 ];
				bool bShowHealth = false;
				bool bShowPlayerName = false;

				// Some entities we always want to check, cause the text may change
				// even while we're looking at it
				// Is it a player?
				if ( IsPlayerIndex( iEntIndex ) )
				{
					// check player because they may have been endeadend
					if ( pPlayer )
					{
						bShowPlayerName = true;

						if ( pPlayer->IsOtherSameTeam( pLocalPlayer->GetTeamNumber() ) && !pPlayer->IsOtherEnemy( pLocalPlayer->entindex() ) )
						{
							printFormatString = ( bShowFriendEnemyDesignation ) ? "#SFUIHUD_playerid_sameteam" : "#SFUIHUD_playerid_specteam";

							bShowFriendlyCrosshair = bFriendlyCrosshairOkay;
							bShowHealth = true;
						}
						else if ( pLocalPlayer->GetTeamNumber() != TEAM_CT && pLocalPlayer->GetTeamNumber() != TEAM_TERRORIST )
						{
							printFormatString = "#SFUIHUD_playerid_noteam";
							bShowFriendlyCrosshair = bFriendlyCrosshairOkay;
							bShowHealth = true;
						}
						else
						{
							printFormatString = ( bShowFriendEnemyDesignation ) ? "#SFUIHUD_playerid_diffteam" : "#SFUIHUD_playerid_specteam";

							bShowFriendlyCrosshair = false;
							bWantEnemyCrosshair = bShowFriendEnemyDesignation;

							if ( bWantEnemyCrosshair && bCheckWeaponRange )
							{
								CWeaponCSBase *pWeapon = ( CWeaponCSBase* )pCSPlayer->GetActiveWeapon();

								if ( pWeapon )
								{
									float flWeaponRange = pWeapon->GetCSWpnData().GetRange();

									vec_t length = VectorLength( pPlayer->WorldSpaceCenter() - pLocalPlayer->Weapon_ShootPosition() );
									bWantEnemyCrosshair = ( length <= flWeaponRange );
								}
							}

						}

						// only get the name and health if necessary
						if ( !( cl_teamid_overhead.GetInt() && bShowFriendlyCrosshair ) )
						{
							C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );

							if ( cs_PR )
							{
								cs_PR->GetDecoratedPlayerName( iEntIndex, wszPlayerName, sizeof( wszPlayerName ), k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot );
							}
							else
							{
								wszPlayerName[0] = L'\0';
							}

							if ( bShowHealth )
							{
								float flHealth = MAX( 0.0f, ( float )pPlayer->GetHealth() );
								V_snwprintf( wszHealthText, ARRAYSIZE( wszHealthText ) - 1, L"%.0f%%",  ( flHealth / ( float )pPlayer->GetMaxHealth() ) * 100 );
								wszHealthText[ ARRAYSIZE( wszHealthText )-1 ] = '\0';
							}
						}
						else
						{
							wszPlayerName[0] = L'\0';
						}
					}
				}
				else
				{
					C_BaseEntity *pEnt = cl_entitylist->GetEnt( iEntIndex );

					//Hostages!
					C_CHostage *pHostage = NULL;

					for( int i=0;i<g_Hostages.Count();i++ )
					{
						// compare entity pointers
						if( g_Hostages[i] == pEnt )
						{
							pHostage = g_Hostages[i];
							break;
						}
					}

					if( pHostage != NULL )
					{		
						float flHealth = MAX( 0.0f, ( float )pPlayer->GetHealth() );
						V_snwprintf( wszHealthText, ARRAYSIZE( wszHealthText ) - 1, L"%.0f%%",  ( flHealth / ( float )pHostage->GetMaxHealth() ) * 100 );
						wszHealthText[ ARRAYSIZE( wszHealthText )-1 ] = '\0';
						bShowFriendlyCrosshair = bFriendlyCrosshairOkay;
						bShowHealth = mp_hostages_takedamage.GetBool();
						bIsHostage = true;

						Vector forward, up;
						pLocalPlayer->EyeVectors( &forward, NULL, &up );

						trace_t tr;
						// Search for objects in a sphere (tests for entities that are not solid, yet still useable)
						Vector searchCenter = pLocalPlayer->EyePosition();
						int useableContents = MASK_NPCSOLID_BRUSHONLY | MASK_OPAQUE_AND_NPCS;
						UTIL_TraceLine( searchCenter, searchCenter + forward * 1024, useableContents, pLocalPlayer, COLLISION_GROUP_NONE, &tr );
						if ( tr.m_pEnt )
						{
							CConfigurationForHighPriorityUseEntity_t cfgUseHostageRule;
							bool bValidHostageRule = pLocalPlayer->GetUseConfigurationForHighPriorityUseEntity( pHostage, cfgUseHostageRule );
							// if we're outside use range, we're a terrorist or the hostage has a leader and it's not the player...
							if ( !bValidHostageRule || !cfgUseHostageRule.UseByPlayerNow( pLocalPlayer, cfgUseHostageRule.k_EPlayerUseType_Start ) ||
								pLocalPlayer->GetTeamNumber() == TEAM_TERRORIST || (pHostage->GetLeader() && pHostage->GetLeader() != pLocalPlayer) ||
								(HOSTAGE_RULE_CAN_PICKUP && pLocalPlayer->m_hCarriedHostage != NULL) )
							{
								if ( pHostage->GetLeader() && pHostage->GetLeader() != pLocalPlayer)
								{
									if ( !bShowHealth )
										printFormatString = "#SFUIHUD_hostageid_nh_following";	
									else
										printFormatString = "#SFUIHUD_hostageid_following";	

									C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );
									char szClan[MAX_PLAYER_NAME_LENGTH];
									g_pVGuiLocalize->ConvertANSIToUnicode( szClan, wszClanTag, sizeof( wszClanTag ) );
									if ( cs_PR )
									{
										cs_PR->GetDecoratedPlayerName( pHostage->GetLeader()->entindex(), wszPlayerName, sizeof( wszPlayerName ), k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot );
									}
									else
									{
										wszPlayerName[0] = L'\0';
									}
									bShowPlayerName = true;
								}
								else
								{
									if ( !bShowHealth )
									{
										//SFUIHUD_hostagename_nh
										if ( CSGameRules()->IsPlayingCooperativeGametype() )
										{
											bShowPlayerName = true;
											g_pVGuiLocalize->ConstructString( wszPlayerName, sizeof( wszPlayerName ), g_pVGuiLocalize->Find( pHostage->GetCustomHostageNameForMap( engine->GetLevelNameShort() ) ), 0 );
											printFormatString = "#SFUIHUD_hostagename_nh";
										}
										else
											printFormatString = "#SFUIHUD_hostageid_nh";
									}
									else
										printFormatString = "#SFUIHUD_hostageid";
								}
							}
							else
							{
								if ( pHostage->GetLeader() == pLocalPlayer )
								{
// 									if ( !bShowHealth )
// 										printFormatString = "#SFUIHUD_hostageid_nh_use_leave";
// 									else
// 										printFormatString = "#SFUIHUD_hostageid_use_leave";
								}
								else if ( !pHostage->GetLeader() )
								{
									if ( !bShowHealth )
										printFormatString = "#SFUIHUD_hostageid_nh_use_lead";
									else
										printFormatString = "#SFUIHUD_hostageid_use_lead";
								}
							}
						}
					}				
				}

				if ( printFormatString )
				{
					if ( cl_teamid_overhead.GetInt() && bShowFriendlyCrosshair && !bIsHostage )
					{
						Vector vDelta = pPlayer->EyePosition() - pLocalPlayer->EyePosition();
						float flDistance = vDelta.Length();

						if ( pPlayer->IsOtherSameTeam( pLocalPlayer->GetTeamNumber() ) && !pPlayer->IsOtherEnemy( pLocalPlayer->entindex() ) 
							 && pPlayer->IsBot() && pPlayer->HasC4() && flDistance < PLAYER_USE_BOT_RADIUS )
						{
							g_pVGuiLocalize->ConstructString( wcNewString, sizeof( wcNewString ), g_pVGuiLocalize->Find( "#SFUIHUD_botid_request_bomb" ), 0 );
							bForceShowCenterID = true;
						}
						else
						{
							V_snwprintf( wcNewString, ARRAYSIZE( wcNewString ), L"" );
							bShowBlankXHairID = true;
						}

						displayingTargetID = true;
					}
					else
					{
						displayingTargetID = true;
						if ( bShowPlayerName && bShowHealth )
						{
							g_pVGuiLocalize->ConstructString( wcNewString, sizeof( wcNewString ), g_pVGuiLocalize->Find( printFormatString ), 2, wszPlayerName, wszHealthText );
						}
						else if ( bShowPlayerName )
						{
							g_pVGuiLocalize->ConstructString( wcNewString, sizeof( wcNewString ), g_pVGuiLocalize->Find( printFormatString ), 1, wszPlayerName );
						}
						else if ( bShowHealth )
						{
							g_pVGuiLocalize->ConstructString( wcNewString, sizeof( wcNewString ), g_pVGuiLocalize->Find( printFormatString ), 1, wszHealthText );
						}
						else
						{
							g_pVGuiLocalize->ConstructString( wcNewString, sizeof( wcNewString ), g_pVGuiLocalize->Find( printFormatString ), 0 );
						}
					}
				}
			}

			if ( !displayingTargetID && bShowDroppedWeaponNames )
			{
				int weaponEntIndex = pCSPlayer->GetTargetedWeapon();
			
				if ( weaponEntIndex > 0 ) //0 is a valid entity index, but will never be used for a weapon
				{			
					C_BaseEntity *pEnt = cl_entitylist->GetEnt( weaponEntIndex );
					C_PlantedC4 *pC4 = dynamic_cast<C_PlantedC4*>( pEnt );
					if ( !pC4 )
					{
						CWeaponCSBase* weapon = static_cast<CWeaponCSBase*>( pEnt );				
						bool isWeaponAllowed = ( weapon && ( !weapon->IsA(WEAPON_TASER) /* || weapon->HasAnyAmmo()*/ ) );  // we allow players to pick up guns without ammo  --mtw

						CEconItemView *pItem = weapon->GetEconItemView();
						wchar_t wcTargetWeaponFormatted[128];
						if ( pItem && pItem->IsValid() )
						{
							const CEconItemRarityDefinition* pRarity = GetItemSchema()->GetRarityDefinition( pItem->GetRarity() );
							const int kColorBufSize = 128;
							wchar_t rwchColor[kColorBufSize];
							Q_UTF8ToUnicode( GetHexColorForAttribColor( pRarity->GetAttribColor() ), rwchColor, kColorBufSize );

							// Update target name
							V_snwprintf( wcTargetWeaponFormatted, ARRAYSIZE( wcTargetWeaponFormatted ), L"<font color=\"" PRI_WS_FOR_WS L"\">" PRI_WS_FOR_WS L"</font>", rwchColor, pItem->GetItemName( true ) );
						}
						else
						{
							V_snwprintf( wcTargetWeaponFormatted, ARRAYSIZE( wcTargetWeaponFormatted ), PRI_WS_FOR_WS, g_pVGuiLocalize->Find( weapon->GetPrintName() ) );
						}

						//const char* szWeaponPrintName = ( pItem ? pItem->GetItemDefinition()->GetItemBaseName() : weapon->GetPrintName() );

						if ( isWeaponAllowed && wcTargetWeaponFormatted )
						{
							displayingTargetID = true;

							CSWeaponType nType = weapon->GetWeaponType();
							if ( pCSPlayer->IsPrimaryOrSecondaryWeapon( nType ) )
							{
								// check to see if we are swapping weapons or if we are just filling in an empty weapon slot
								bool bSwap = false;
								if ( (nType == WEAPONTYPE_PISTOL && pCSPlayer->Weapon_GetSlot( WEAPON_SLOT_PISTOL )) || ( nType != WEAPONTYPE_PISTOL && pCSPlayer->Weapon_GetSlot( WEAPON_SLOT_RIFLE ) ) )
									bSwap = true;

								if ( bSwap )
								{
									//const wchar_t* wszWeaponText = g_pVGuiLocalize->Find( szWeaponPrintName );
									g_pVGuiLocalize->ConstructString( wcNewString, sizeof( wcNewString ), g_pVGuiLocalize->Find( "#SFUIHUD_weaponid_pickup" ), 1, wcTargetWeaponFormatted );
								}
								else
								{
									V_snwprintf( wcNewString, ARRAYSIZE( wcNewString ), PRI_WS_FOR_WS, wcTargetWeaponFormatted );
								}
							}
							else
							{
								V_snwprintf( wcNewString, ARRAYSIZE( wcNewString ), PRI_WS_FOR_WS, wcTargetWeaponFormatted );
								displayingTargetID = true;
							}
						}
					}
				}
			}
		}

	}

	// player ID
	FOR_EACH_VEC( m_playerIDs, i )
	{
		m_playerIDs[i].bActive = false;
	}

	{
	MDLCACHE_CRITICAL_SECTION();

	bool bIsPauseMenuActive = ( BasePanel() && BasePanel()->IsScaleformPauseMenuActive() );
	bool bShowAllNamesForSpec = (bLocalIsSpectatorViewer && spec_show_xray.GetInt());

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		if ( bIsPauseMenuActive )
			break;

		//TODO: check whether they are an enemy as well
		CCSPlayer* pOtherPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );
		
		bool bTurnedOff = false;
		if ( cl_teamid_overhead.GetBool() == false || cl_draw_only_deathnotices.GetBool() == true )
			bTurnedOff = true;

		if ( bTurnedOff || !pLocalPlayer || !pOtherPlayer || !pOtherPlayer->IsAlive( ) || !pOtherPlayer->IsVisible( ) || ( pOtherPlayer == pCSPlayer && !input->CAM_IsThirdPersonOverview( ) ) )
			continue;

		if ( g_HltvReplaySystem.GetHltvReplayDelay() )
			continue; // in replay, there's a clear indication of "You". People seem to not care about the other labels that add to visual noise.
		static ConVarRef sv_show_voip_indicator_for_enemies( "sv_show_voip_indicator_for_enemies" );
		if ( bShowAllNamesForSpec || (pCSPlayer->IsOtherSameTeam( pOtherPlayer->GetTeamNumber() ) && !pCSPlayer->IsOtherEnemy( pOtherPlayer ) ) || ( sv_show_voip_indicator_for_enemies.GetBool() && pOtherPlayer->IsPlayerTalkingOverVOIP() ) )
		{
			bool bSameTeam = pLocalPlayer->InSameTeam( pOtherPlayer );
			bool bIsEnemy = pLocalPlayer->IsOtherEnemy( pOtherPlayer );
			Vector vDelta = pOtherPlayer->EyePosition() - pCSPlayer->EyePosition();
			float flDistance = vDelta.Length();
			// get our spectator target
			int nTargetSpec = g_bEngineIsHLTV ? HLTVCamera()->GetCurrentOrLastTarget() : (pLocalPlayer->GetObserverTarget() ? pLocalPlayer->GetObserverTarget()->entindex() : -1);
			// max distance to draw the names is different depending on whether we are on team spectator or not
			float flMaxDrawDist = bShowAllNamesForSpec ? cl_teamid_overhead_maxdist_spec.GetInt() : cl_teamid_overhead_maxdist.GetInt();
			// always show the name for the player who is our target when roaming
			if ( bShowAllNamesForSpec && nTargetSpec == pOtherPlayer->entindex() && (pLocalPlayer->GetObserverMode() == OBS_MODE_FIXED || pLocalPlayer->GetObserverMode() == OBS_MODE_ROAMING) )
				flMaxDrawDist = 99999.9f;

			if ( flDistance <= flMaxDrawDist )
			{
				Vector vecOtherPlayerEyes = pOtherPlayer->EyePosition() + Vector( 0, 0, 3 );
				// other player is close enough, now make sure the local player is facing the other player.
				Vector vecOtherPlayerPos = pOtherPlayer->EyePosition();
				Vector forward;
				AngleVectors(pCSPlayer->EyeAngles(), &forward, NULL, NULL);
				Vector toAimSpot = vecOtherPlayerEyes - pCSPlayer->EyePosition();
				float rangeToEnemy = toAimSpot.NormalizeInPlace();
				float flTargetIDCone = DotProduct( toAimSpot, forward );
				const float flViewCone = 0.5f;

				bool bPlayerIsNotVisible = false;

				// trace against world for enemies or if client doesn't want team id everywhere+
				{
					// if we can't trace to the player or if they are obscured by smoke, skip them
					CBasePlayer *lastPlayerHit = NULL;
					trace_t tr;
					if (  pCSPlayer->IsAlive( ) && ( flTargetIDCone > flViewCone ) && !bShowAllNamesForSpec )
					{
						if ( LineGoesThroughSmoke( pCSPlayer->EyePosition(), vecOtherPlayerEyes, 1.0f ) )
						{
							if ( !ShouldShowAllFriendlyTargetIDs() || pCSPlayer->IsOtherEnemy( pOtherPlayer ) )
							{
								continue;
							}
							else
							{
								bPlayerIsNotVisible = true;
							}

						}
					
						UTIL_TraceLine( pCSPlayer->EyePosition(), vecOtherPlayerEyes, MASK_VISIBLE, pCSPlayer, COLLISION_GROUP_DEBRIS, &tr );
						{
							CTraceFilterSkipTwoEntities filter( pCSPlayer, lastPlayerHit, COLLISION_GROUP_DEBRIS );

							// Check for player hitboxes extending outside their collision bounds
							const float rayExtension = 40.0f;
							UTIL_ClipTraceToPlayers( pCSPlayer->EyePosition(), vecOtherPlayerEyes + forward * rayExtension, MASK_VISIBLE, &filter, &tr );
							if ( tr.fraction < 1 )
							{
								if ( !ShouldShowAllFriendlyTargetIDs() || pCSPlayer->IsOtherEnemy( pOtherPlayer ) )
								{
									continue;
								}
								else
								{
									bPlayerIsNotVisible = true;
								}
							}
						}
					}
				}

				// aiming tolerance depends on how close the target is - closer targets subtend larger angles
				float aimTolerance = 0.6f; //(float)cos( atan( 256 / rangeToEnemy ) );

				int index = 0;
				bool bExists = false;

				// let's do this!
				if ( bShowAllNamesForSpec || flTargetIDCone > flViewCone )
				{
					FOR_EACH_VEC( m_playerIDs, j )
					{
						if ( m_playerIDs[j].hPlayer.Get() == pOtherPlayer )
						{
							index = j;
							bExists = true;
							m_playerIDs[j].bActive = true;
							break;
						}
					}

					int nIDIndex =  pCSPlayer->GetIDTarget();
					//Msg( "ID Index = %d\n", nIDIndex );
					C_BaseEntity *pIDEnt = cl_entitylist->GetEnt( nIDIndex );
					bool bShowIDName = CSGameRules()->IsFreezePeriod() || bShowAllNamesForSpec || ((pIDEnt == pOtherPlayer || !pCSPlayer->IsAlive()) && bSameTeam && !bIsEnemy);

					if ( !bExists )
					{
						index = m_playerIDs.Count();
						bExists = true;

						bool bFriend = false;

						C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );
						if ( cs_PR )
						{
							XUID nOtherXUID = cs_PR->GetXuid( pOtherPlayer->entindex() );
							if ( g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetFriendByXUID( nOtherXUID ) )
							{
								bFriend = true;
							}
						}
						
						AddNewPlayerID( pOtherPlayer, bShowIDName, bFriend );
					}

					if ( m_playerIDs.Count() > 0 && index < m_playerIDs.Count() )
					{
						// update the team number
						m_playerIDs[index].nTeam = pOtherPlayer->GetTeamNumber();

						bool bFlashedChanged = bLocalIsSpectatorViewer ? (m_playerIDs[index].bFlashedAmt != pOtherPlayer->m_flFlashOverlayAlpha) : false;
						bool bHealthChanged = (m_playerIDs[index].nHealth != pOtherPlayer->GetHealth());
						bool bUpdateNow = (m_playerIDs[index].flUpdateAt != 0 && m_playerIDs[index].flUpdateAt <= gpGlobals->curtime );
						if ( bExists && ( /*CSGameRules()->IsFreezePeriod() ||*/ bUpdateNow || bHealthChanged || bFlashedChanged || m_playerIDs[index].bShowName != bShowIDName) )
						{
							if ( bShowIDName == false && m_playerIDs[index].bShowName == true )
							{
								m_playerIDs[index].flLastHighlightTime = gpGlobals->curtime;
							}

							if ( CSGameRules()->IsFreezePeriod() ) // update again in a bit if we're still in freeze time
								m_playerIDs[index].flUpdateAt = gpGlobals->curtime + 0.5f;
							else
								m_playerIDs[index].flUpdateAt = 0;

							m_playerIDs[index].bShowName = bShowIDName;
							m_playerIDs[index].nHealth = pOtherPlayer->GetHealth();
							UpdatePlayerID( pOtherPlayer, index, true );
						}

						WITH_SLOT_LOCKED
						{
							float flScale = clamp( (float)atan( 512 / (rangeToEnemy*1.0) ), 0.6, 2 );
													
							float flAlp = clamp( 1 - (rangeToEnemy / (flMaxDrawDist*0.75)), 0.25, 1 );

							//Msg( "scale = %f, rangeToEnemy = %f\n", flScale, rangeToEnemy );
							// adjust scale by the screen size
							flScale *= clamp( 1-(600.0f/(float)ScreenHeight()), 0.25, 1 );
							float flFadeRangeClose = 160;
							if ( rangeToEnemy < flFadeRangeClose && !bShowIDName )
							{
								flAlp *= MAX( 0.25, (rangeToEnemy-(flFadeRangeClose*0.35))/(flFadeRangeClose*0.55) );
							}

							float flFadeEdge = (1-aimTolerance)*0.4;
							if ( flTargetIDCone < (aimTolerance+flFadeEdge) && !bShowAllNamesForSpec )
							{
								float flFadeEdgeCur = flTargetIDCone - aimTolerance;
								flAlp *= MAX( 0.3, flFadeEdgeCur/flFadeEdge );
							}

							if ( input->CAM_IsThirdPersonOverview() )
							{
								flScale = 0.8f;
								flAlp = 1.0f;
							}

							// need to make sure that not only do we have a valid handle, but that the handle is of the correct type for GetType to handle it.  Otherwise Scaleform just crashes.....
							if ( m_playerIDs[index].panel && m_pScaleformUI->Value_GetType( m_playerIDs[index].panel ) == IScaleformUI::VT_DisplayObject )
							{
								// if the player is talking, hide the arrow and just show the talk icon
								if ( m_playerIDs[index].arrowA && m_playerIDs[index].arrowB && m_playerIDs[index].arrowF && m_playerIDs[index].voiceIcon && m_playerIDs[index].defuseIcon )
								{
									bool bIsFriend = m_playerIDs[index].bFriend;
									bool bVoiceActive = !bPlayerIsNotVisible && ( pOtherPlayer->IsPlayerTalkingOverVOIP( ) && ( ( bSameTeam && !bIsEnemy ) || sv_show_voip_indicator_for_enemies.GetBool( ) ) && voice_icons_method.GetInt( ) == 2 ); 
									bool bIsDefusing = !bPlayerIsNotVisible && pOtherPlayer->m_bIsDefusing;
									int flags = 0;
									flags |= bIsFriend		? 1<<0 : 0;
									flags |= bVoiceActive	? 1<<1 : 0;
									flags |= bIsDefusing	? 1<<2 : 0;
									if ( m_playerIDs[index].iconsFlag != flags )
									{
										ScaleformDisplayInfo dinfoA;
										ScaleformDisplayInfo dinfoB;
										ScaleformDisplayInfo dinfoF;
										ScaleformDisplayInfo dinfoV;
										ScaleformDisplayInfo dinfoD;
										dinfoA.SetVisibility( !bVoiceActive && !bIsDefusing );
										dinfoB.SetVisibility( !bVoiceActive && /*!bIsFriend &&*/ !bIsDefusing );
										dinfoF.SetVisibility( !bVoiceActive && bIsFriend && !bIsDefusing );
										dinfoV.SetVisibility( bVoiceActive );
										dinfoD.SetVisibility( bIsDefusing && !bVoiceActive );
										m_pScaleformUI->Value_SetDisplayInfo( m_playerIDs[index].arrowA, &dinfoA );
										m_pScaleformUI->Value_SetDisplayInfo( m_playerIDs[index].arrowB, &dinfoB );
										m_pScaleformUI->Value_SetDisplayInfo( m_playerIDs[index].arrowF, &dinfoF );
										m_pScaleformUI->Value_SetDisplayInfo( m_playerIDs[index].voiceIcon, &dinfoV );
										m_pScaleformUI->Value_SetDisplayInfo( m_playerIDs[index].defuseIcon, &dinfoD );

										m_playerIDs[index].iconsFlag = flags;
									}
								}

								// put the indicator right over their head
								bool bHaveHeadBone = true;
								// Make sure he's all the way on screen so his bone is correct
								int iBIndex = -1;
								{
									iBIndex = pOtherPlayer->LookupBone( "ValveBiped.Bip01_Head" );
								}
								
								if( ( iBIndex == -1 ) || !pOtherPlayer->GetBaseAnimating()->isBoneAvailableForRead( iBIndex ) )
								{
									bHaveHeadBone = false;
								}

								Vector vecBone;
								if ( bHaveHeadBone )
								{
									QAngle angBone;
									pOtherPlayer->GetBonePosition( iBIndex, vecBone, angBone );
								}
								else
								{
									vecBone = (pOtherPlayer->EyePosition() - pOtherPlayer->GetAbsOrigin()) + Vector( 0.0f, 0.0f, 20 );
								}

								int iX, iY;
								GetVectorInScreenSpace( vecBone + Vector( 0, 0, 9 ), iX, iY );
								//GetTargetInScreenSpace( pFoundEnt, iX, iY );
								ScaleformDisplayInfo dinfo;
								dinfo.SetVisibility( true );
								dinfo.SetAlpha( 140 * flAlp );
								dinfo.SetX( iX );
								dinfo.SetY( iY );
								dinfo.SetXScale( 240 * flScale );
								dinfo.SetYScale( 240 * flScale );
								m_pScaleformUI->Value_SetDisplayInfo( m_playerIDs[index].panel, &dinfo );
							}
						}
					}
				}
			}
		}
	}

	} // End MDLCACHE_CRITICAL_SECTION()

	// Don't remove players at this point in order to void adding them again (causing spikes)

	FOR_EACH_VEC( m_playerIDs, k )
	{
		if ( m_playerIDs[ k ].bActive == false )
		{
			//RemoveID( k );
			m_pScaleformUI->Value_SetVisible( m_playerIDs[ k ].panel, false );
		}
	}
	
	if ( !ShouldShowAllFriendlyEquipment() )
	{
		FOR_EACH_VEC( m_playerIDs, l )
		{
			float flTimeToFadeOut = cl_teamid_overhead_name_fadetime.GetFloat();
			float flExpireTime = m_playerIDs[l].flLastHighlightTime + flTimeToFadeOut;
			int nMaxAlpha = cl_teamid_overhead_name_alpha.GetInt();

			if ( m_playerIDs[l].bActive && m_playerIDs[l].bShowName == false && m_playerIDs[l].flNameAlpha > 0 )
			{	
				float frac = ( flExpireTime - gpGlobals->curtime ) / flTimeToFadeOut;
				int alpha = frac * nMaxAlpha;

				SFVALUE animatedID = m_pScaleformUI->Value_GetMember( m_playerIDs[l].panel, "IDClip" );
				SFVALUE textBG = m_pScaleformUI->Value_GetMember( m_playerIDs[l].panel, "IDTextBG" );

				if ( animatedID /*&& animatedIDDrop*/ && textBG )
				{
					SFVALUE textmovie = m_pScaleformUI->Value_GetMember( animatedID, "IDTextMovie" );

					if ( textmovie /*&& textDrop*/ )
					{
						ScaleformDisplayInfo dinfo;
						ScaleformDisplayInfo dinfo2;
						dinfo.SetAlpha( alpha );
						dinfo2.SetAlpha( alpha/7 );
						if ( alpha <= 0 || frac <= 0 )
						{
							alpha = 0;
							dinfo.SetVisibility( false );
							dinfo2.SetVisibility( false );
						}
						m_playerIDs[l].flNameAlpha = MAX( 0, alpha );
						m_pScaleformUI->Value_SetDisplayInfo( textmovie, &dinfo );
						m_pScaleformUI->Value_SetDisplayInfo( textBG, &dinfo2 );

						SafeReleaseSFVALUE( textmovie );
					}

					SafeReleaseSFVALUE( animatedID );
					SafeReleaseSFVALUE( textBG );
				}
			}
		}
	}

	// now do all the stuff that might lock the slot
	//0 = default
	//1 = default static
	//2 = classic standard
	//3 = classic dynamic
	//4 = classic static
	if ( cl_crosshairstyle.GetInt() >= 2 )
	//if ( !sfcrosshair.GetBool() )
	{
		iDesiredReticleMode = RETICLE_MODE_NONE;
	}

	bool bSlotIsLocked = false;

	if ( displayingTargetID && (wcNewString[0] || bShowBlankXHairID) )
	{
		// this updates the text if necessary
		if ( V_wcscmp( wcNewString, m_wcIDString ) )
		{
			LockSlot( true, bSlotIsLocked );
			m_pScaleformUI->Value_SetTextHTML( m_IDText, m_pScaleformUI->ReplaceGlyphKeywordsWithHTML( wcNewString ) );
			V_wcsncpy( m_wcIDString, wcNewString, TEXTFIELD_LENGTH*sizeof( wchar_t ) );
		}

		// this shows/hides the friendly crosshair
		if ( m_FriendCrosshair && ( bShowFriendlyCrosshair != m_bFriendlyCrosshairVisible ) )
		{
			LockSlot( true, bSlotIsLocked );
			m_pScaleformUI->Value_SetVisible( m_FriendCrosshair, bShowFriendlyCrosshair );
			m_bFriendlyCrosshairVisible = bShowFriendlyCrosshair;
		}

		// update the timer ( which turns the text field off after quarter a second
		m_fIDTimer = gpGlobals->curtime + 0.25f;

		// call the text field's show animation if it isn't already visible
		if ( !m_bTextIDVisible && (bForceShowCenterID || cl_teamid_overhead.GetInt() == 0 || (cl_teamid_overhead.GetInt() > 0 && !bShowFriendlyCrosshair) || bIsHostage) )
		{
			LockSlot( true, bSlotIsLocked );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_IDMovie, "Show", NULL, 0 );
			m_bTextIDVisible = true;
		}
	}
	else
    {
		// hide the friendly crosshair since we're not looking at anything anymore
		if ( m_bFriendlyCrosshairVisible )
		{
			LockSlot( true, bSlotIsLocked );
			m_pScaleformUI->Value_SetVisible( m_FriendCrosshair, false );
			m_bFriendlyCrosshairVisible = false;
		}

		// once our timer has run out, call the hide animation for the text
		if ( m_bTextIDVisible && gpGlobals->curtime >= m_fIDTimer )
		{
			LockSlot( true, bSlotIsLocked );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_IDMovie, "Hide", NULL, 0 );
			m_bTextIDVisible = false;
		}
    }

	if ( bWantEnemyCrosshair != m_bEnemyCrosshairVisible )
	{
		m_bEnemyCrosshairVisible = bWantEnemyCrosshair;

		float colorIndex = 99;

		if ( !bWantEnemyCrosshair )
		{
			ConVarRef convar( "cl_crosshaircolor" );
			colorIndex = convar.GetFloat();
		}

		WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
		{
			m_pScaleformUI->ValueArray_SetElement( args, 0, colorIndex );
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onUpdateColor", args, 1 );
		}
	}

	if ( iDesiredReticleMode != m_iReticleMode )
	{
		LockSlot( true, bSlotIsLocked );
		ShowReticle( m_iReticleMode, false );
		ShowReticle( iDesiredReticleMode, true );
		m_iReticleMode = iDesiredReticleMode;
	}

	if ( m_iReticleMode == RETICLE_MODE_WEAPON )
	{
		float x = 0.0f;
		float y = 0.0f;

#ifdef SIXENSE
		if( g_pSixenseInput->IsEnabled() && 
			C_BasePlayer::GetLocalPlayer() && 
			( C_BasePlayer::GetLocalPlayer()->GetObserverMode()==OBS_MODE_NONE ) &&
			lockMoveControllerRet.GetBool() == false )
#else
		if ( inputsystem->MotionControllerActive() && 
			C_BasePlayer::GetLocalPlayer() && 
			( C_BasePlayer::GetLocalPlayer()->GetObserverMode()==OBS_MODE_NONE ) &&
			 lockMoveControllerRet.GetBool() == false )
#endif
		{

			Vector aimDirection;
			
			if ( mc_use_recoil_on_cursor.GetBool())
			{
				AngleVectors(pCSPlayer->GetFinalAimAngle(), &aimDirection);
			}
			else
			{
				aimDirection = pCSPlayer->GetAimDirection();
			}

			Vector screen;
			Vector point;
			VectorAdd( CurrentViewOrigin(), aimDirection, point );
			ScreenTransform( point, screen );

			x = 0.5f * screen[0] * ScreenWidth() + 0.5f;
			y = -0.5f * screen[1] * ScreenHeight() + 0.5f;
		}

		//0 = default
		//1 = default static
		//2 = classic standard
		//3 = classic dynamic
		//4 = classic static
		if ( cl_crosshairstyle.GetInt() == 1 )
		{
			fDesiredFishtail = 0;
		}

		SetReticlePosition( iDesiredSpread, iDesiredGap, x, y, ( fDesiredFishtail / 200.0f ) * ( ScreenWidth() * 0.5f ) );
	}

	LockSlot( false, bSlotIsLocked );
}

void SFHudReticle::ToggleTeamEquipmentVisibility( bool bShow )
{
	m_bForceShowAllTeammateTargetIDs = bShow;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CCSPlayer* pCSPlayer = ToCSPlayer( UTIL_PlayerByIndex( i ) );

		FOR_EACH_VEC( m_playerIDs, j )
		{
			if ( m_playerIDs[ j ].hPlayer.Get( ) == pCSPlayer )
			{
				UpdatePlayerID( pCSPlayer, j );
			}
		}
	}
}


void SFHudReticle::AddNewPlayerID( CBaseEntity *player, bool bShowName, bool bFriend )
{
	if ( !FlashAPIIsValid() )
		return;

	if ( !player || m_playerIDs.Count() >= MAX_PLAYER_NAME_ID_PANELS )
		return;

	SFVALUE newPanelHandle = NULL;

	WITH_SLOT_LOCKED
	{
		//Create new panel
		newPanelHandle = m_pScaleformUI->Value_Invoke( m_FlashAPI, "AddNewPlayerID", NULL, 0 );
	}

	if ( newPanelHandle )
	{
		//Put the new panel in the list
		PlayerIDPanel newPanel;
		newPanel.hPlayer = player;
		newPanel.panel = newPanelHandle;
		newPanel.bActive = true;
		newPanel.bFriend = bFriend;
		newPanel.nHealth = player->GetHealth();
		newPanel.nTeam = player->GetTeamNumber();
		newPanel.flNameAlpha = 0;
		newPanel.flLastHighlightTime = 0;
		newPanel.bShowName = false;
		newPanel.flUpdateAt = 0;
		newPanel.iconsFlag = -1;
		newPanel.bFlashedAmt = -1;// we will hide the panel immediately

		if ( m_pScaleformUI->Value_GetType( newPanelHandle ) == IScaleformUI::VT_DisplayObject )
		{
			newPanel.arrowA = m_pScaleformUI->Value_GetMember( newPanelHandle, "IDArrow" );
			newPanel.arrowB = m_pScaleformUI->Value_GetMember( newPanelHandle, "IDArrowBorder" );
			newPanel.arrowF = m_pScaleformUI->Value_GetMember( newPanelHandle, "IDArrowFriend" );
			newPanel.voiceIcon = m_pScaleformUI->Value_GetMember( newPanelHandle, "iconChat" );
			newPanel.defuseIcon = m_pScaleformUI->Value_GetMember( newPanelHandle, "DefuseKitIcon" );
		}
		else
		{
			newPanel.arrowA = NULL;
			newPanel.arrowB = NULL;
			newPanel.arrowF = NULL;
			newPanel.voiceIcon = NULL;
			newPanel.defuseIcon = NULL;
		}

		m_playerIDs.AddToTail( newPanel );

		UpdatePlayerID( player, m_playerIDs.Count()-1 );
	}
}

void SFHudReticle::UpdatePlayerID( CBaseEntity *player, int slot, bool bHealthAndNameOnly )
{
	if ( !FlashAPIIsValid() )
		return;

	C_CSPlayer *pCSPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( !pCSPlayer || !player || m_playerIDs.Count() >= MAX_PLAYER_NAME_ID_PANELS || slot >= m_playerIDs.Count() )
		return;

	bool bDidSetName = false;

	if ( !m_playerIDs[slot].panel )
	{
		Warning( "AddNewPlayerID called and updated in sfhudreticle, but m_playerIDs[slot].panel is null!!\n" );
		return;
	}

	C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );

	if ( !bHealthAndNameOnly )
	{
		int playerIndex = 0;
		for ( int i = 0; i <= MAX_PLAYERS; i++ )
		{
			CBasePlayer* pCheckPlayer = UTIL_PlayerByIndex( i );
			if ( pCheckPlayer && pCheckPlayer == player )
				playerIndex = i;
		}

		int nColorID = -1;
		if ( cs_PR && pCSPlayer->ShouldShowTeamPlayerColors( player->GetTeamNumber() ) )
			nColorID = cs_PR->GetCompTeammateColor( playerIndex );

		// set the color of the icon, need to do it by calling the script
		WITH_SFVALUEARRAY_SLOT_LOCKED( args, 4 )
		{
			m_pScaleformUI->ValueArray_SetElement( args, 0, m_playerIDs[slot].panel );
			m_pScaleformUI->ValueArray_SetElement( args, 1, m_playerIDs[slot].nTeam );
			m_pScaleformUI->ValueArray_SetElement( args, 2, m_playerIDs[slot].bFriend );
			m_pScaleformUI->ValueArray_SetElement( args, 3, nColorID );

			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setIDTeamColor", args, 4 );
		}
	}

	if ( !cs_PR )
	{
		Warning( "AddNewPlayerID called and updated in sfhudreticle, but C_CS_PlayerResource is null!!\n" );
		return;
	}

	SFVALUE animatedID = m_pScaleformUI->Value_GetMember( m_playerIDs[slot].panel, "IDClip" );
	SFVALUE textBG = m_pScaleformUI->Value_GetMember( m_playerIDs[slot].panel, "IDTextBG" );

	if ( !animatedID /*&& animatedIDDrop*/ || !textBG )
	{
		Warning( "AddNewPlayerID called and updated in sfhudreticle, but IDClip and IDTextBG objects are null!!\n" );
		return;
	}

	SFVALUE textmovie = m_pScaleformUI->Value_GetMember( animatedID, "IDTextMovie" );

	if ( !textmovie /*&& textDrop*/ )
	{
		Warning( "AddNewPlayerID called and updated in sfhudreticle, but IDTextMovie object is null!!\n" );
		return;
	}

	SFVALUE text = m_pScaleformUI->Value_GetMember( textmovie, "IDText" );

	if ( !text /*&& textDrop*/ )
	{
		Warning( "AddNewPlayerID called and updated in sfhudreticle, but IDText object is null!!\n" );
		return;
	}

	//IDWeaponTextParent
	SFVALUE wepIconParent = m_pScaleformUI->Value_GetMember( textmovie, "IDWeaponTextParent" );

	if ( !wepIconParent )
		return;

	SFVALUE wepIcons = m_pScaleformUI->Value_GetMember( wepIconParent, "IDWeaponText" );
	if ( !wepIcons )
	{
		Warning( "AddNewPlayerID called and updated in sfhudreticle, but IDWeaponText object is null!!\n" );
		return;
	}

	wchar_t wszHealthText[ 10 ];
	float flHealth = 0.0f;
	
	C_CSPlayer *pTargetPlayer = static_cast<C_CSPlayer*>( player );

	bool bLocalIsSpectatorViewer = CanSeeSpectatorOnlyTools();

	bool bShowMoneyInsteadOfHealth = CSGameRules( )->CanSpendMoneyInMap( ) && CSGameRules( )->IsFreezePeriod( ) && !bLocalIsSpectatorViewer;

	if ( bShowMoneyInsteadOfHealth ) // we show money in the place of health during freeze time
		flHealth = pTargetPlayer->GetAccount( );
	else
		flHealth = ( MAX( 0.0f, ( float ) player->GetHealth( ) ) / ( float ) player->GetMaxHealth( ) );


	// show the main equipped weapon, grenades and money during freezetime or if ShouldShowAllFriendlyEquipment

	if ( ( !bLocalIsSpectatorViewer && ( CSGameRules( )->IsFreezePeriod( ) ) || ShouldShowAllFriendlyEquipment() ) )
	{
		CHudWeaponSelection *pHudSelection = (CHudWeaponSelection *)GET_HUDELEMENT( CHudWeaponSelection );
		if ( !pHudSelection )
			return;

		wchar_t wszWeaponIcons[ MAX_DECORATED_PLAYER_NAME_LENGTH ] = { L"" };

		// weapons are only shown in freeze time.
		bool bShowWeapons = CSGameRules( )->IsFreezePeriod( ) || ShouldShowAllFriendlyEquipment();
		bool bShowDefuser = ShouldShowAllFriendlyEquipment();
		bool bShowC4 = ShouldShowAllFriendlyEquipment();

		CSWeaponID nPrimaryWeaponID = WEAPON_KNIFE;

		if ( bShowWeapons )
		{
			if ( pTargetPlayer && pTargetPlayer->GetActiveCSWeapon( ) )
			{		
				if ( !IsPrimaryWeapon( nPrimaryWeaponID )/* && CSGameRules()->IsFreezePeriod()*/ )
				{
					// While in the freeze period we want to show their primary weapon if they have one
					for ( int i = 0; i < MAX_WEAPONS; i++ )
					{
						C_WeaponCSBase *pWeapon = assert_cast< C_WeaponCSBase* >( pTargetPlayer->GetWeapon( i ) );
						if ( !pWeapon )
							continue;

						CSWeaponID nIDTemp = pWeapon->GetCSWeaponID( );
						if ( IsPrimaryWeapon( nIDTemp ) )
						{
							// Found a primary
							nPrimaryWeaponID = nIDTemp;
							break;
						}
						else if ( IsSecondaryWeapon( nIDTemp ) )
						{
							// Fall back to secondary if non-primary and non-secondary is active
							nPrimaryWeaponID = nIDTemp;
						}
					}
				}
			}
		}
			// let's wait on showing armor and defuse kits
// 			int nArmor = cs_PR->GetArmor( pTargetPlayer->entindex() );
// 			bool bHasHelmet = cs_PR->HasHelmet( pTargetPlayer->entindex() );		
// 
// 			if ( nArmor > 0 )
// 			{
// 				const char *szWeapon = "armor";
// 				if ( bHasHelmet )
// 					szWeapon = "armor_helmet";
// 
// 				wchar_t wszWeapon[64];
// 				V_UTF8ToUnicode( szWeapon, wszWeapon, sizeof( wszWeapon ) );
// 				wchar_t szWeaponHTML[64];
// 				GetIconHTML( wszWeapon, szWeaponHTML, ARRAYSIZE( szWeaponHTML ), true );
// 				Q_wcsncat( wszWeaponIcons, szWeaponHTML, sizeof( wszWeaponIcons ), COPY_ALL_CHARACTERS );
// 			}
// 
		if ( bShowDefuser )
		{
			bool bHasDef = cs_PR->HasDefuser( pTargetPlayer->entindex( ) );

			if ( bHasDef )
			{
				wchar_t wszWeapon[ 64 ];
				V_UTF8ToUnicode( "defuser", wszWeapon, sizeof( wszWeapon ) );
				wchar_t szWeaponHTML[ 64 ];
				GetIconHTML( wszWeapon, szWeaponHTML, ARRAYSIZE( szWeaponHTML ), true );
				Q_wcsncat( wszWeaponIcons, szWeaponHTML, sizeof( wszWeaponIcons ), COPY_ALL_CHARACTERS );
			}
		}

		if ( bShowC4 )
		{
			bool bHasC4 = cs_PR->HasC4( pTargetPlayer->entindex( ) );

			if ( bHasC4 )
			{
				wchar_t wszWeapon[ 64 ];
				V_UTF8ToUnicode( "bomb-holder", wszWeapon, sizeof( wszWeapon ) );
				wchar_t szWeaponHTML[ 64 ];
				GetIconHTML( wszWeapon, szWeaponHTML, ARRAYSIZE( szWeaponHTML ), true );
				Q_wcsncat( wszWeaponIcons, szWeaponHTML, sizeof( wszWeaponIcons ), COPY_ALL_CHARACTERS );
			}
		}

		for ( int slot = MAX_WEAPON_SLOTS; slot >= 0; slot-- )
		{
			for ( int pos = MAX_WEAPON_POSITIONS; pos >= 0; pos-- )
			{
				C_WeaponCSBase *pWeapon = assert_cast< C_WeaponCSBase* >( pHudSelection->GetWeaponInSlotForTarget( pTargetPlayer, slot, pos ) );
				// if we dont have a weapon OR if the found weapon is our selected weapon, skip it

				if ( !pWeapon )
					continue;

				// if we're not showing weapons ignore anything that isn't utility
				if ( !bShowWeapons && !pWeapon->IsKindOf( WEAPONTYPE_GRENADE ) )
				{
					continue;
				}
				// if we are showing weapons then we're only showing primary or utility
				else if ( pWeapon->GetCSWeaponID( ) == nPrimaryWeaponID || pWeapon->IsKindOf( WEAPONTYPE_GRENADE ) )
				{
					CEconItemView *pItem = pWeapon->GetEconItemView( );

					const char *szWeapon = ( ( pItem && pItem->IsValid() && pItem->GetItemIndex() && pItem->GetItemDefinition() )
						? pItem->GetItemDefinition( )->GetDefinitionName( )
						: pWeapon->GetClassname( ) );

					if ( IsWeaponClassname( szWeapon ) )
					{
						szWeapon += WEAPON_CLASSNAME_PREFIX_LENGTH;
					}

					if ( Q_strcmp( "knife", szWeapon ) == 0 )
					{
						if ( pTargetPlayer && pTargetPlayer->GetTeamNumber( ) == TEAM_CT )
						{
							szWeapon = "knife_default_ct";
						}
						else
						{
							szWeapon = "knife_default_t";
						}
					}

					wchar_t wszWeapon[ 64 ];
					V_UTF8ToUnicode( szWeapon, wszWeapon, sizeof( wszWeapon ) );

					wchar_t szWeaponHTML[ 64 ];
					GetIconHTML( wszWeapon, szWeaponHTML, ARRAYSIZE( szWeaponHTML ), true );

					int nCount = 1;
					if ( pWeapon->IsKindOf( WEAPONTYPE_GRENADE ) )
						nCount = pWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );

					for ( int nGren = 0; nGren < nCount; nGren++ )
					{
						Q_wcsncat( wszWeaponIcons, szWeaponHTML, sizeof( wszWeaponIcons ), COPY_ALL_CHARACTERS );
					}
				}
			}
		}

		m_pScaleformUI->Value_SetTextHTML( wepIcons, wszWeaponIcons );
	}
	else
	{
		m_pScaleformUI->Value_SetTextHTML( wepIcons, "" );
	}

	// 		bool bIsFullyBlinded = pCSPlayer->m_flFlashOverlayAlpha >= 180.0f;
	// 		bool bIsFlashed = pCSPlayer->m_flFlashBangTime > ( gpGlobals->curtime + 0.5 );
	if ( pTargetPlayer )
	{
		float flFlashFrac = 1.0 - clamp( pTargetPlayer->m_flFlashOverlayAlpha / 150, 0, 1 );
		if ( pTargetPlayer->m_flFlashOverlayAlpha > 0 )
			m_playerIDs[ slot ].bFlashedAmt = pTargetPlayer->m_flFlashOverlayAlpha;

		if ( m_playerIDs[ slot ].bFlashedAmt != 0 )
		{
			if ( pTargetPlayer->m_flFlashOverlayAlpha <= 0 || m_playerIDs[ slot ].bFlashedAmt == -1 )
			{
				WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
				{
					m_pScaleformUI->ValueArray_SetElement( args, 0, m_playerIDs[ slot ].panel );
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hideFlashIcon", args, 1 );
				}
				m_playerIDs[ slot ].bFlashedAmt = 0;
			}
			else if ( CanSeeSpectatorOnlyTools( ) )
			{
				// show flashed
				WITH_SFVALUEARRAY_SLOT_LOCKED( args, 2 )
				{
					m_pScaleformUI->ValueArray_SetElement( args, 0, m_playerIDs[ slot ].panel );
					m_pScaleformUI->ValueArray_SetElement( args, 1, flFlashFrac );

					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setFlashIcon", args, 2 );
				}
			}
		}
	}
	

	WITH_SLOT_LOCKED
	{
		wchar_t wcNewString[ TEXTFIELD_LENGTH ];
		wchar_t wszPlayerName[ MAX_DECORATED_PLAYER_NAME_LENGTH ];

		const char *printFormatString = "#SFUIHUD_playerid_specteam";
// 		if ( pCSPlayer->ShouldShowTeamPlayerColors( player->GetTeamNumber() ) )
// 		{
// 			if ( bShowOverheadWeapons && CSGameRules()->CanSpendMoneyInMap() )
// 				printFormatString = "#SFUIHUD_playerid_overhead_money";
// 			else if ( flHealth < 0.25f )
// 				printFormatString = "#SFUIHUD_playerid_overhead_lowhealth";
// 			else
// 				printFormatString = "#SFUIHUD_playerid_overhead";
// 		}
// 		else if ( m_playerIDs[slot].nTeam == TEAM_CT )
		if ( m_playerIDs[slot].nTeam == TEAM_CT )
		{
			if ( bShowMoneyInsteadOfHealth )
				printFormatString = "#SFUIHUD_playerid_overhead_ct_money";
			else if ( flHealth < 0.25f )
				printFormatString = "#SFUIHUD_playerid_overhead_ct_lowhealth";
			else
				printFormatString = "#SFUIHUD_playerid_overhead_ct";
		}
		else
		{
			if ( bShowMoneyInsteadOfHealth )
				printFormatString = "#SFUIHUD_playerid_overhead_t_money";
			else if ( flHealth < 0.25f )
				printFormatString = "#SFUIHUD_playerid_overhead_t_lowhealth";
			else
				printFormatString = "#SFUIHUD_playerid_overhead_t";
		}

		if ( printFormatString )
		{	
			if ( bShowMoneyInsteadOfHealth )
			{			
				V_snwprintf( wszHealthText, ARRAYSIZE( wszHealthText ) - 1, L"$%d",  (int)flHealth );
			}
			else
			{
			V_snwprintf( wszHealthText, ARRAYSIZE( wszHealthText ) - 1, L"%.0f%%",  flHealth * 100 );
			}

			wszHealthText[ ARRAYSIZE( wszHealthText )-1 ] = '\0';

			cs_PR->GetDecoratedPlayerName( player->entindex(), wszPlayerName, sizeof( wszPlayerName ), k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot );

			g_pVGuiLocalize->ConstructString( wcNewString, sizeof( wcNewString ), g_pVGuiLocalize->Find( printFormatString ), 2, wszPlayerName, wszHealthText );
		}

		m_pScaleformUI->Value_SetTextHTML( text, wcNewString );

		bool bSetVisible = ShouldShowAllFriendlyEquipment() || m_playerIDs[ slot ].bShowName || m_playerIDs[ slot ].flLastHighlightTime == 0;

		ScaleformDisplayInfo dinfo;
		ScaleformDisplayInfo dinfo2;
		m_pScaleformUI->Value_GetDisplayInfo( textmovie, &dinfo );
		m_pScaleformUI->Value_GetDisplayInfo( textBG, &dinfo2 );

		bool bShowAllNamesForSpec = ( bLocalIsSpectatorViewer && spec_show_xray.GetInt() );
		float flMaxDrawDist = bShowAllNamesForSpec ? cl_teamid_overhead_maxdist_spec.GetInt() : cl_teamid_overhead_maxdist.GetInt();

		Vector vecOtherPlayerEyes = pTargetPlayer->EyePosition() + Vector( 0, 0, 3 );
		Vector toAimSpot = vecOtherPlayerEyes - pCSPlayer->EyePosition();
		float rangeToEnemy = toAimSpot.NormalizeInPlace();


		float flAlp = RemapValClamped( rangeToEnemy, 
			500.0f, flMaxDrawDist, 
			100.0f, 50.0f );
			
		m_playerIDs[ slot ].flNameAlpha = bLocalIsSpectatorViewer ? cl_teamid_overhead_name_alpha.GetInt() : flAlp;


		dinfo.SetAlpha( m_playerIDs[slot].flNameAlpha );
		dinfo2.SetAlpha( m_playerIDs[slot].flNameAlpha/7 );
		dinfo.SetVisibility( bSetVisible );
		dinfo2.SetVisibility( bSetVisible );
		m_pScaleformUI->Value_SetDisplayInfo( textmovie, &dinfo );
		m_pScaleformUI->Value_SetDisplayInfo( textBG, &dinfo2 );		

		bDidSetName = true;
	}

	WITH_SLOT_LOCKED
	{
		SafeReleaseSFVALUE( wepIconParent );
		SafeReleaseSFVALUE( wepIcons );
		SafeReleaseSFVALUE( textmovie );
		SafeReleaseSFVALUE( text );

		WITH_SFVALUEARRAY( args, 1 )
		{
			m_pScaleformUI->ValueArray_SetElement( args, 0, m_playerIDs[slot].panel );	
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "UpdateBGSize", args, 1 );
		}

		SafeReleaseSFVALUE( animatedID );
		SafeReleaseSFVALUE( textBG );
	}

	if ( !bDidSetName )
	{
		Warning( "AddNewPlayerID called and updated in sfhudreticle, but no name was displayed!!!!\n" );
	}
}

void SFHudReticle::GetIconHTML( const wchar_t * szIcon, wchar_t * szBuffer, int nBufferSize, bool bSelected )
{
	if ( bSelected )
		V_snwprintf( szBuffer, nBufferSize, HUDRET_WEPICON_SELECTED_IMG_STRING, szIcon );
	else
		V_snwprintf( szBuffer, nBufferSize, HUDRET_WEPICON_IMG_STRING, szIcon );
}

bool SFHudReticle::ShouldShowAllFriendlyTargetIDs( void )
{
	extern ConVar sv_teamid_overhead_always_prohibit;
	if ( sv_teamid_overhead_always_prohibit.GetBool() )
		return false;

	return ( m_bForceShowAllTeammateTargetIDs || cl_teamid_overhead_always.GetInt() >= 1 );
}

bool SFHudReticle::ShouldShowAllFriendlyEquipment( void )
{
	extern ConVar sv_show_team_equipment_prohibit;
	if ( sv_show_team_equipment_prohibit.GetBool() )
		return false;

	return ( m_bForceShowAllTeammateTargetIDs || cl_teamid_overhead_always.GetInt() == 2 );
}

bool SFHudReticle::SetReticlePosition( int distance, int crosshairGap, int offsetX, int offsetY, int nDesiredFishtail )
{
	bool bMadeChanges = false;
	bool bSlotIsLocked = false;

	if ( m_WeaponCrosshairHandle )
	{
		ScaleformDisplayInfo dinfo;

		LockSlot( true, bSlotIsLocked );

		//0 = default
		//1 = default static
		//2 = classic standard
		//3 = classic dynamic
		//4 = classic static
		if ( cl_crosshairstyle.GetInt() == 0 )
		//if ( cl_dynamiccrosshair.GetBool() )
		{
			// setting this once here will make all of these become visible
			dinfo.SetVisibility( true );

			if ( m_crosshairDot )
			{
				ScaleformDisplayInfo dotDisplayInfo;
				dotDisplayInfo.SetX( offsetX + m_dotX - nDesiredFishtail );
				dotDisplayInfo.SetY( offsetY + m_dotY - nDesiredFishtail );
				m_pScaleformUI->Value_SetDisplayInfo( m_crosshairDot, &dotDisplayInfo );				
			}

			if ( m_blackRing )
			{
				ScaleformDisplayInfo blackRingDisplayInfo;
				blackRingDisplayInfo.SetX( offsetX + m_blackRingX );
				blackRingDisplayInfo.SetY( offsetY + m_blackRingY );
				m_pScaleformUI->Value_SetDisplayInfo( m_blackRing, &blackRingDisplayInfo );				
			}

			if ( m_FriendCrosshair )
			{
				ScaleformDisplayInfo friendlyCrossHairDisplayInfo;
				friendlyCrossHairDisplayInfo.SetX( offsetX + m_friendIndicatorX - nDesiredFishtail );
				friendlyCrossHairDisplayInfo.SetY( offsetY + m_friendIndicatorY - nDesiredFishtail );
				m_pScaleformUI->Value_SetDisplayInfo( m_FriendCrosshair, &friendlyCrossHairDisplayInfo );
			}			
			
			//Hide the arcs if we get the sentinel value as the desired distance
			if ( distance == -1 )
			{
				ScaleformDisplayInfo  hideArcsDisplayInfo;
				hideArcsDisplayInfo.SetVisibility( false );
				m_pScaleformUI->Value_SetDisplayInfo( m_topCrosshairArc, &hideArcsDisplayInfo );
				m_pScaleformUI->Value_SetDisplayInfo( m_bottomCrosshairArc, &hideArcsDisplayInfo );
				m_pScaleformUI->Value_SetDisplayInfo( m_leftCrosshairArc, &hideArcsDisplayInfo );
				m_pScaleformUI->Value_SetDisplayInfo( m_rightCrosshairArc, &hideArcsDisplayInfo );

			}
			else
			{
				dinfo.SetX( offsetX - nDesiredFishtail );
				dinfo.SetY( - distance + offsetY );
				m_pScaleformUI->Value_SetDisplayInfo( m_topCrosshairArc, &dinfo );

				dinfo.SetX( distance + offsetX - nDesiredFishtail );
				dinfo.SetY( offsetY );
				m_pScaleformUI->Value_SetDisplayInfo( m_rightCrosshairArc, &dinfo );

				dinfo.SetX( - distance + offsetX - nDesiredFishtail );
				dinfo.SetY( offsetY );
				m_pScaleformUI->Value_SetDisplayInfo( m_leftCrosshairArc, &dinfo );

				dinfo.SetX( offsetX - nDesiredFishtail );
				dinfo.SetY( distance + offsetY );
				m_pScaleformUI->Value_SetDisplayInfo( m_bottomCrosshairArc, &dinfo );
			}


			if ( crosshairGap == -1 )
			{      
				ScaleformDisplayInfo  hidePipsDisplayInfo;
				hidePipsDisplayInfo.SetVisibility( false );
				m_pScaleformUI->Value_SetDisplayInfo( m_TopPip, &hidePipsDisplayInfo );
				m_pScaleformUI->Value_SetDisplayInfo( m_BottomPip, &hidePipsDisplayInfo );
				m_pScaleformUI->Value_SetDisplayInfo( m_LeftPip, &hidePipsDisplayInfo );
				m_pScaleformUI->Value_SetDisplayInfo( m_RightPip, &hidePipsDisplayInfo );
			}
			else
			{
				dinfo.SetX( offsetX - nDesiredFishtail );
				dinfo.SetY( m_TopPipY - crosshairGap + offsetY);
				m_pScaleformUI->Value_SetDisplayInfo( m_TopPip, &dinfo );

				dinfo.SetX( offsetX + 1 - nDesiredFishtail );
				dinfo.SetY( m_BottomPipY + crosshairGap + offsetY+1);
				m_pScaleformUI->Value_SetDisplayInfo( m_BottomPip, &dinfo );

				dinfo.SetY(offsetY+1);
				dinfo.SetX( m_LeftPipX - crosshairGap + offsetX - nDesiredFishtail );
				m_pScaleformUI->Value_SetDisplayInfo( m_LeftPip, &dinfo );

				dinfo.SetY(offsetY);
				dinfo.SetX( m_RightPipX + crosshairGap + offsetX + 1 - nDesiredFishtail );
				m_pScaleformUI->Value_SetDisplayInfo( m_RightPip, &dinfo );
			}  
			
		}
		else
		{
			// make the arcs invisible
			dinfo.SetVisibility( false );

			m_pScaleformUI->Value_SetDisplayInfo( m_topCrosshairArc, &dinfo );
			m_pScaleformUI->Value_SetDisplayInfo( m_rightCrosshairArc, &dinfo );
			m_pScaleformUI->Value_SetDisplayInfo( m_leftCrosshairArc, &dinfo );
			m_pScaleformUI->Value_SetDisplayInfo( m_bottomCrosshairArc, &dinfo );

			dinfo.SetVisibility( true );

			float cg = cl_fixedcrosshairgap.GetFloat();

			dinfo.SetX( offsetX - nDesiredFishtail );
			dinfo.SetY( m_TopPipY - cg + offsetY);
			m_pScaleformUI->Value_SetDisplayInfo( m_TopPip, &dinfo );

			dinfo.SetX( offsetX + 1 - nDesiredFishtail );
			dinfo.SetY( m_BottomPipY + cg + offsetY+1);
			m_pScaleformUI->Value_SetDisplayInfo( m_BottomPip, &dinfo );

			dinfo.SetY(offsetY+1);
			dinfo.SetX( m_LeftPipX - cg + offsetX - nDesiredFishtail );
			m_pScaleformUI->Value_SetDisplayInfo( m_LeftPip, &dinfo );

			dinfo.SetY(offsetY);
			dinfo.SetX( m_RightPipX + cg + offsetX + 1 - nDesiredFishtail );
			m_pScaleformUI->Value_SetDisplayInfo( m_RightPip, &dinfo );
		}

		m_iLastSpread = distance;
		m_iLastGap = crosshairGap;
		bMadeChanges = true;
	}

	// Update the position of the text.
	if ( m_bTextIDVisible && m_IDMovie && m_IDText )
	{
		LockSlot( true, bSlotIsLocked );

		// text movie
		ScaleformDisplayInfo displayInfo;
		displayInfo.SetX( offsetX + m_IDMovieX );
		displayInfo.SetY( offsetY + m_IDMovieY );
		m_pScaleformUI->Value_SetDisplayInfo( m_IDMovie, &displayInfo );

		bMadeChanges = true;
	}

	LockSlot( false, bSlotIsLocked );

	return bMadeChanges;
}


void SFHudReticle::FlashReady( void )
{
	m_ObserverCrosshairHandle = m_pScaleformUI->Value_GetMember( m_FlashAPI, "Observer" );

	PerformSwapReticle( "Crosshair1" );

	m_IDMovie = m_pScaleformUI->Value_GetMember( m_FlashAPI, "TargetID" );

	if ( m_IDMovie )
	{
		SFVALUE animatedID = m_pScaleformUI->Value_GetMember( m_IDMovie, "IDAnimated" );

		if ( animatedID )
		{
			m_IDText = m_pScaleformUI->Value_GetMember( animatedID, "TextBox" );
			m_pScaleformUI->Value_SetTextHTML( m_IDText, " " );
			SafeReleaseSFVALUE( animatedID );
		}
	}

	m_FlashedIcon = m_pScaleformUI->Value_GetMember( m_FlashAPI, "FlashedBlind" );
// 	// Update the position of the text.
// 	if ( m_FlashedIcon )
// 	{
// 		LockSlot( true, bSlotIsLocked );
// 
// 		// text movie
// 		ScaleformDisplayInfo displayInfo;
// 		m_pScaleformUI->Value_GetDisplayInfo( m_FlashedIcon, &displayInfo );
// 		displayInfo.SetX( offsetX + m_IDMovieX );
// 		displayInfo.SetY( offsetY + m_IDMovieY );
// 		m_pScaleformUI->Value_SetDisplayInfo( m_IDMovie, &displayInfo );
// 
// 		bMadeChanges = true;
// 	}

	ResetDisplay();

	ListenForGameEvent( "item_pickup" );
	ListenForGameEvent( "ammo_pickup" );
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "round_end" );
	ListenForGameEvent( "round_freeze_end" );

	ListenForGameEvent( "bomb_planted" );
	ListenForGameEvent( "defuser_pickup" );
	ListenForGameEvent( "bomb_dropped" );
	ListenForGameEvent( "bomb_pickup" );
	ListenForGameEvent( "grenade_thrown" );

}

bool SFHudReticle::PreUnloadFlash( void )
{
 	SafeReleaseSFVALUE( m_WeaponCrosshairHandle );
 	SafeReleaseSFVALUE( m_ObserverCrosshairHandle );

	SafeReleaseSFVALUE( m_TopPip );
	SafeReleaseSFVALUE( m_BottomPip );
	SafeReleaseSFVALUE( m_LeftPip );
	SafeReleaseSFVALUE( m_RightPip );

	SafeReleaseSFVALUE( m_topCrosshairArc );
	SafeReleaseSFVALUE( m_rightCrosshairArc );
	SafeReleaseSFVALUE( m_leftCrosshairArc );
	SafeReleaseSFVALUE( m_bottomCrosshairArc );

	SafeReleaseSFVALUE( m_FriendCrosshair );
	SafeReleaseSFVALUE( m_crosshairDot );
	SafeReleaseSFVALUE( m_blackRing );
	
	
	SafeReleaseSFVALUE( m_IDMovie );
	SafeReleaseSFVALUE( m_IDText );

	SafeReleaseSFVALUE( m_FlashedIcon );
	
	RemoveAllIDs();

	return true;
}

void SFHudReticle::ResetDisplay( void )
{

	// hide everything we don't want / need;
	WITH_SLOT_LOCKED
	{
		if ( m_IDMovie )
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_IDMovie, "HideNow", NULL, 0 );
		}

		if ( m_FlashedIcon )
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashedIcon, "Hide", NULL, 0 );
			m_bFlashedIconFadingOut = false;
		}

		if ( m_FriendCrosshair ) 
		{
			m_pScaleformUI->Value_SetVisible( m_FriendCrosshair, false );
		}

		ShowReticle( RETICLE_MODE_WEAPON, false );
		ShowReticle( RETICLE_MODE_OBSERVER, false );
	}

	m_fIDTimer = 0;
	m_bTextIDVisible = false;
	m_bFriendlyCrosshairVisible = false;

	m_iReticleMode = RETICLE_MODE_NONE;

	RemoveAllIDs();
}

void SFHudReticle::LevelInit( void )
{
	if ( !FlashAPIIsValid() )
	{
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFHudReticle, this, Reticle );
	}
	else
	{
		ResetDisplay();
	}
}

void SFHudReticle::LevelShutdown( void )
{
	if ( FlashAPIIsValid() )
	{
		RemoveFlashElement();
	}
}

bool SFHudReticle::ShouldDraw( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return false;

	if ( pPlayer->GetObserverInterpState() == C_CSPlayer::OBSERVER_INTERP_TRAVELING )
		return false;

	bool bNeedsDraw = false;
	if ( (pPlayer->GetObserverMode() == OBS_MODE_ROAMING) || (pPlayer->GetObserverMode() == OBS_MODE_FIXED) )
		bNeedsDraw = true;

	return cl_drawhud.GetBool() && (bNeedsDraw || CHudElement::ShouldDraw());
}


void SFHudReticle::SetActive( bool bActive )
{
	if ( !FlashAPIIsValid() )
		return;

	SF_SPLITSCREEN_PLAYER_GUARD();

	if ( !bActive && m_bActive )
	{
		ResetDisplay();
	} 
	else if ( bActive && !m_bActive )
	{
		SplitScreenSlottedConVarRef convar( GET_ACTIVE_SPLITSCREEN_SLOT(), "cl_crosshaircolor" );

		WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
		{
			g_pScaleformUI->ValueArray_SetElement( args, 0, convar.GetFloat() );
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "onUpdateColor", args, 1 );
		}
	}

	CHudElement::SetActive( bActive );
}

void SFHudReticle::OnSwapReticle( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_SPLITSCREEN_PLAYER_GUARD();
	PerformSwapReticle( m_pScaleformUI->Params_GetArgAsString( obj, 0 ) );
}

void SFHudReticle::PerformSwapReticle( const char * szReticleName )
{
	SafeReleaseSFVALUE( m_WeaponCrosshairHandle );
	SafeReleaseSFVALUE( m_FriendCrosshair );
	SafeReleaseSFVALUE( m_crosshairDot );
	SafeReleaseSFVALUE( m_blackRing );
	SafeReleaseSFVALUE( m_TopPip );
	SafeReleaseSFVALUE( m_BottomPip );
	SafeReleaseSFVALUE( m_LeftPip );
	SafeReleaseSFVALUE( m_RightPip );
	SafeReleaseSFVALUE( m_topCrosshairArc );
	SafeReleaseSFVALUE( m_rightCrosshairArc );
	SafeReleaseSFVALUE( m_leftCrosshairArc );
	SafeReleaseSFVALUE( m_bottomCrosshairArc );

	m_WeaponCrosshairHandle = m_pScaleformUI->Value_GetMember( m_FlashAPI, szReticleName );

	if ( m_WeaponCrosshairHandle )
	{
		ScaleformDisplayInfo dinfo;

		m_FriendCrosshair = m_pScaleformUI->Value_GetMember( m_WeaponCrosshairHandle, "FriendCrosshair" );
		m_crosshairDot = m_pScaleformUI->Value_GetMember( m_WeaponCrosshairHandle, "Dot" );		
		m_blackRing = m_pScaleformUI->Value_GetMember( m_WeaponCrosshairHandle, "BlackRing" );

		if ( !m_bCrosshairPositionsInitialized )
		{
			m_bCrosshairPositionsInitialized = true;

			if ( m_FriendCrosshair )
			{
				m_pScaleformUI->Value_GetDisplayInfo( m_FriendCrosshair, &dinfo );
				m_friendIndicatorX = dinfo.GetX();
				m_friendIndicatorY = dinfo.GetY();
			}

			if ( m_crosshairDot )
			{
				m_pScaleformUI->Value_GetDisplayInfo( m_crosshairDot, &dinfo );
				m_dotX = dinfo.GetX();
				m_dotY = dinfo.GetY();
			}			

			if ( m_blackRing )
			{
				m_pScaleformUI->Value_GetDisplayInfo( m_blackRing, &dinfo );
				m_blackRingX = dinfo.GetX();
				m_blackRingY = dinfo.GetY();
			}			

		}	

		m_TopPip = m_pScaleformUI->Value_GetMember( m_WeaponCrosshairHandle, "TopPip" );
		if ( m_TopPip )
		{
			m_TopPipY = 0;
		}

		m_BottomPip = m_pScaleformUI->Value_GetMember( m_WeaponCrosshairHandle, "BottomPip" );
		if ( m_BottomPip )
		{
			m_BottomPipY = 0;
		}

		m_LeftPip = m_pScaleformUI->Value_GetMember( m_WeaponCrosshairHandle, "LeftPip" );
		if ( m_LeftPip )
		{
			m_LeftPipX = 0;
		}

		m_RightPip = m_pScaleformUI->Value_GetMember( m_WeaponCrosshairHandle, "RightPip" );
		if ( m_RightPip )
		{
			m_RightPipX = 0;
		}

		m_topCrosshairArc = m_pScaleformUI->Value_GetMember( m_WeaponCrosshairHandle, "TopArc" );	
		m_rightCrosshairArc = m_pScaleformUI->Value_GetMember( m_WeaponCrosshairHandle, "RightArc" );
		m_leftCrosshairArc = m_pScaleformUI->Value_GetMember( m_WeaponCrosshairHandle, "LeftArc" );
		m_bottomCrosshairArc = m_pScaleformUI->Value_GetMember( m_WeaponCrosshairHandle, "BottomArc" );
	}

	if ( m_FriendCrosshair ) 
	{
		m_pScaleformUI->Value_SetVisible( m_FriendCrosshair, false );
	}

	if ( m_IDMovie )
	{
		ScaleformDisplayInfo dinfo;
		m_pScaleformUI->Value_GetDisplayInfo( m_IDMovie, &dinfo );
		m_IDMovieX = dinfo.GetX();
		m_IDMovieY = dinfo.GetY();
	}

	// set these here because the flash code makes them invisible
	m_bFriendlyCrosshairVisible = false;
	m_iReticleMode = RETICLE_MODE_NONE;

	m_iLastSpread = -1;
	m_iLastGap = -1;	
}

void SFHudReticle::RemoveID( int index )
{
	SFVALUE panel = m_playerIDs[ index ].panel;
	SafeReleaseSFVALUE( m_playerIDs[ index ].arrowA );
	SafeReleaseSFVALUE( m_playerIDs[ index ].arrowB );
	SafeReleaseSFVALUE( m_playerIDs[ index ].arrowF );
	SafeReleaseSFVALUE( m_playerIDs[ index ].voiceIcon );
	SafeReleaseSFVALUE( m_playerIDs[ index ].defuseIcon );
	m_playerIDs.Remove( index );
	WITH_SFVALUEARRAY_SLOT_LOCKED( data, 1 )
	{
		m_pScaleformUI->ValueArray_SetElement( data, 0, panel );
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "RemovePlayerID", data, 1 );
	}
	SafeReleaseSFVALUE( panel );
}

void SFHudReticle::RemoveAllIDs( void )
{
	if ( m_pScaleformUI )
	{
		WITH_SLOT_LOCKED
		{
			//Remove all items before we exit
			while( m_playerIDs.Count() > 0 )
			{
				RemoveID( 0 );
			}
		}
	}
}

void SFHudReticle::FireGameEvent( IGameEvent *event )
{
	const char *type = event->GetName();

	int nUserId = event->GetInt( "userid" );
	C_BasePlayer *pPlayer = UTIL_PlayerByUserId( nUserId );
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !V_strcmp( type, "player_death" ) )
	{		
		// if we were the one that died or the player we are spectating, remove all the stuff!
		if ( pPlayer && (pPlayer->entindex() == pLocalPlayer->entindex() || (pLocalPlayer->GetObserverTarget() && pPlayer == pLocalPlayer->GetObserverTarget())) )
		{
			RemoveAllIDs();
			
			if ( m_IDMovie )
			{
				WITH_SLOT_LOCKED
				{
					m_pScaleformUI->Value_InvokeWithoutReturn( m_IDMovie, "HideNow", NULL, 0 );
				}
			}

			if ( m_FlashedIcon )
			{
				WITH_SLOT_LOCKED
				{
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashedIcon, "Hide", NULL, 0 );
					m_bFlashedIconFadingOut = false;
				}
			}
		}
	}
	else if ( !V_strcmp( type, "round_end" ) )
	{
		RemoveAllIDs();
	}
	else if ( !V_strcmp( type, "round_start" ) )
	{
		RemoveAllIDs();

		if ( m_IDMovie && FlashAPIIsValid() )
		{
			WITH_SLOT_LOCKED
			{
				m_pScaleformUI->Value_InvokeWithoutReturn( m_IDMovie, "HideNow", NULL, 0 );
			}
		}

		if ( m_FlashedIcon )
		{
			WITH_SLOT_LOCKED
			{
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashedIcon, "Hide", NULL, 0 );
				m_bFlashedIconFadingOut = false;
			}
		}
	}
	else if ( !V_strcmp( type, "item_pickup" ) || 
		!V_strcmp( type, "ammo_pickup" ) ||
		!V_strcmp( type, "bomb_planted" ) ||
		!V_strcmp( type, "defuser_pickup" ) ||
		!V_strcmp( type, "bomb_dropped" ) ||
		!V_strcmp( type, "bomb_pickup" ) ||
		!V_strcmp( type, "grenade_thrown" ) )
	{
		if ( pLocalPlayer )
		{
			FOR_EACH_VEC( m_playerIDs, j )
			{
				if ( m_playerIDs[j].hPlayer.Get() == pPlayer )
				{
					m_playerIDs[j].flUpdateAt = (gpGlobals->curtime + 0.1f);
					break;
				}
			}
		}
	}
	else if ( !V_strcmp( type, "round_freeze_end" ) )
	{
		if ( pLocalPlayer )
		{
			FOR_EACH_VEC( m_playerIDs, j )
			{
				m_playerIDs[j].flUpdateAt = (gpGlobals->curtime + 0.1f);
			}
		}
	}
}

