//========= Copyright  1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:  Displays HUD elements about health and armor
//
//=====================================================================================//

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
#include "sfhudweaponpanel.h"
#include "vgui/ILocalize.h"
#include "c_cs_hostage.h"
#include "HUD/sfweaponselection.h"
#include "clientsteamcontext.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SAFECALL( handle, func )	\
	if ( handle )					\
	{								\
	func							\
	}

DECLARE_HUDELEMENT( SFHudWeaponPanel );

SFUI_BEGIN_GAME_API_DEF
SFUI_END_GAME_API_DEF( SFHudWeaponPanel, WeaponModule ); // Asset named WeaponModule to maintain consistency with Flash file naming

extern ConVar cl_draw_only_deathnotices;

SFHudWeaponPanel::SFHudWeaponPanel( const char *value ) : SFHudFlashInterface( value ),
	m_PanelHandle( NULL ),
	m_CurrentWeaponImageHandle( NULL ),
	m_CurrentWeaponTextHandle( NULL ),
	m_AmmoTextClipHandle( NULL ),
	m_AmmoTextTotalHandle( NULL ),
	m_AmmoAnimationHandle( NULL ),
	m_BurstIcons_Burst( NULL ),
	m_BurstIcons_Single( NULL ),
	m_WeaponPenetration1( NULL ),
	m_WeaponPenetration2( NULL ),
	m_WeaponPenetration3( NULL ),
	m_UpgradeKill1( NULL ),
	m_UpgradeKill2( NULL ),
	m_UpgradeKillText( NULL ),
	m_BombHandle( NULL ),
	m_DefuseHandle( NULL ),
	m_BombZoneHandle( NULL ),
	m_WeaponItemName( NULL ),
	m_PrevAmmoClipCount( -1 ),
	m_PrevAmmoTotalCount( -1 ),
	m_PrevAmmoType( -1 ),
	m_PrevWeaponID( -1 ),
	m_PrevTRGunGameUpgradePoints( 0 ),
	m_bHiddenNoAmmo( false ),
	m_bCarryingC4( false ),
	m_bCarryingDefuse( false ),
	m_bInBombZone( false ),
	m_lastEntityIndex( 0 ),
	m_LastNumRoundKills( 0 ),
	m_lastKillEaterCount( 0 )

{
	// TODO Auto-generated constructor stub
	SetHiddenBits( HIDEHUD_WEAPONSELECTION );
}


SFHudWeaponPanel::~SFHudWeaponPanel()
{
	// TODO Auto-generated destructor stub
}

void SFHudWeaponPanel::ShowPanel( bool value )
{
	if ( !m_pScaleformUI )
		return;

	WITH_SLOT_LOCKED
	{
		if ( m_FlashAPI )
		{
			if ( value )
			{
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showNow", NULL, 0 );
			}
			else
			{
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hideNow", NULL, 0 );
			}
		}
	}
}

void SFHudWeaponPanel::SetVisible( bool bVisible )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SFVALUEARRAY_SLOT_LOCKED( data, 1 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, bVisible );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setVisible", data, 1 );
		}
	}
}

void SFHudWeaponPanel::LockSlot( bool wantItLocked, bool& currentlyLocked )
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

void SFHudWeaponPanel::ProcessInput( void )
{
	// Update stats
	int		currentClip = 0;
	// we need the map clip to calculate the percentage of ammo left 
	// in the clip so the hud knows when to warn you
	int		maxClip = 0;
	int		totalAmmo = 0;
	int		ammoType = -1;
	int		weaponID = -1;
	const char *weaponName = NULL;
	const char *shortWeaponName = NULL;
	bool	bInTRBombMode = false;
	bool	bHideNoAmmo = false;
	int		CurrTRPoints = -1;

	// Collect all player, weapon and game state data first:

	if ( CSGameRules()->IsPlayingGunGame() )
	{
		if ( CSGameRules()->IsPlayingGunGameTRBomb() )
		{
			bInTRBombMode = true;
		}
	}

	C_CSPlayer *pPlayer = GetHudPlayer();
	CWeaponCSBase *pWeapon = NULL;
	int entityIndex = pPlayer->entindex();

	if ( pPlayer)
	{
		if ( CSGameRules()->IsBombDefuseMap() || CSGameRules()->IsHostageRescueMap() )
		{
			if ( m_bCarryingC4 != pPlayer->HasC4() )
			{
				m_bCarryingC4 = pPlayer->HasC4();
			}

			SFWeaponSelection *pHudWS = GET_HUDELEMENT( SFWeaponSelection );
			if ( pHudWS )
			{
				static ConVarRef cl_hud_bomb_under_radar( "cl_hud_bomb_under_radar" );
				bool bShowBomb = ( cl_hud_bomb_under_radar.GetInt( ) == 0 && m_bCarryingC4 );
				//SAFECALL( m_BombHandle, m_pScaleformUI->Value_SetVisible( m_BombHandle, bShowBomb ); );

				WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
				{
					m_pScaleformUI->ValueArray_SetElement( args, 0, bShowBomb );
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowBomb", args, 1 );
				}
			}

			if ( m_bCarryingDefuse != pPlayer->HasDefuser() )
			{
				m_bCarryingDefuse = pPlayer->HasDefuser();
				SAFECALL( m_DefuseHandle, m_pScaleformUI->Value_SetVisible( m_DefuseHandle, m_bCarryingDefuse ); );
			}

			if ( m_bInBombZone != pPlayer->m_bInBombZone )
			{
				m_bInBombZone = pPlayer->m_bInBombZone;
				SAFECALL( m_BombZoneHandle, m_pScaleformUI->Value_SetVisible( m_BombZoneHandle, m_bInBombZone ); );
			}

		}

		int nRoundKills = pPlayer->GetNumRoundKills();
		int nRoundKillsHeadshots = pPlayer->GetNumRoundKillsHeadshots();
		if( pPlayer->IsControllingBot() )
		{
			C_CSPlayer *controlledPlayerScorer = ToCSPlayer( UTIL_PlayerByIndex( pPlayer->GetControlledBotIndex() ) );
			if( controlledPlayerScorer )
			{
				nRoundKills = controlledPlayerScorer->GetNumRoundKills();
				nRoundKillsHeadshots = controlledPlayerScorer->GetNumRoundKillsHeadshots();
			}
		}

		if ( m_LastNumRoundKills != nRoundKills )
		{
			int nCurIndex = pPlayer->GetPlayerGunGameWeaponIndex();
			int nRequiredKills = 0;
			if ( CSGameRules()->IsPlayingGunGameProgressive() )
				nRequiredKills = CSGameRules()->GetGunGameNumKillsRequiredForWeapon( nCurIndex, pPlayer->GetTeamNumber() );
			
			WITH_SFVALUEARRAY_SLOT_LOCKED( args, 3 )
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, nRoundKills );
				m_pScaleformUI->ValueArray_SetElement( args, 1, nRoundKillsHeadshots );
				m_pScaleformUI->ValueArray_SetElement( args, 2, nRequiredKills );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setNumberKills", args, 3 );
			}

			m_LastNumRoundKills = nRoundKills;
		}

		if ( bInTRBombMode == true )
		{
			CurrTRPoints = pPlayer->GetNumGunGameTRKillPoints();

			const int nUpgradedFlag = 99;

			// We track "upgrade achieved" by saving the upgrade points as 99 - which means it will
			//	always differ from the actual kill points once we've already upgraded.  So, we don't
			//	want to push another change to the panel until our points get reset at the next round
			bool bAlreadyUpgraded = ( m_PrevTRGunGameUpgradePoints == nUpgradedFlag );
			bool bZeroed = ( CurrTRPoints == 0 );

			if ( ( m_PrevTRGunGameUpgradePoints != CurrTRPoints ) && ( !bAlreadyUpgraded || bZeroed ) )
			{
				// disable the little kill flags now because with the NEXT WEAPON panel serves this purpose and the flags cause confusion
				// TODO: find a way to bring the kill flags back some way because they add a lot of value
				/*
				switch( CurrTRPoints )
				{
				case 0:
					SAFECALL( m_UpgradeKill1, m_pScaleformUI->Value_SetVisible( m_UpgradeKill1, false); );
					SAFECALL( m_UpgradeKill2, m_pScaleformUI->Value_SetVisible( m_UpgradeKill2, false); );
					SAFECALL( m_UpgradeKillText, m_pScaleformUI->Value_SetVisible( m_UpgradeKillText, false ); );
					break;

				case 1:
					SAFECALL( m_UpgradeKill1, m_pScaleformUI->Value_SetVisible( m_UpgradeKill1, true); );
					break;

				case 2:
					SAFECALL( m_UpgradeKill2, m_pScaleformUI->Value_SetVisible( m_UpgradeKill2, true); );
					break;

				default:
					break;
				}
				*/

				static ConVarRef mp_ggtr_bomb_pts_for_upgrade( "mp_ggtr_bomb_pts_for_upgrade" );

				if ( CurrTRPoints >= mp_ggtr_bomb_pts_for_upgrade.GetInt() )
				{
					// disable the little kill flags now because with the NEXT WEAPON panel serves this purpose and the flags cause confusion
					// TODO: find a way to bring the kill flags back some way because they add a lot of value
					/*
					SAFECALL( m_UpgradeKillText, m_pScaleformUI->Value_SetVisible( m_UpgradeKillText, true); );

					WITH_SLOT_LOCKED
					{
						m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "playUpgradeAnim", NULL, 0 );
					}
					*/

					m_PrevTRGunGameUpgradePoints = nUpgradedFlag;
				}
				else
				{
					m_PrevTRGunGameUpgradePoints = CurrTRPoints;
				}
			}
		}
		
		pWeapon = pPlayer ? (CWeaponCSBase*)pPlayer->GetActiveWeapon() : NULL;

		if ( pWeapon )
		{
			weaponName = pWeapon->GetPrintName();
			shortWeaponName = pWeapon->GetName();

			// remap weaponID to our ammo numbering (defined in the action script for this hud element) - currently, just bullets vs grenades
			// $TODO: are we going to reflect any other ammo types, like XBLA did?

			weaponID = pWeapon->GetCSWeaponID();
			switch ( weaponID )
			{
			case WEAPON_DECOY: // $FIXME: prototype grenades just display with the flashbang ammo
			case WEAPON_MOLOTOV:
			case WEAPON_INCGRENADE:
			case WEAPON_FLASHBANG:
			case WEAPON_TAGRENADE:
				ammoType = 1;
				break;
			case WEAPON_HEGRENADE:
				ammoType = 2;
				break;
			case WEAPON_SMOKEGRENADE:
				ammoType = 3;
				break;
			case WEAPON_HEALTHSHOT:
				ammoType = 10;
				break;
			default:
				ammoType = 0;
				break;
			}

			// determine what to display for ammo: "clip/total", "total" or nothing (for knife, c4, etc)
			if ( !pWeapon->UsesPrimaryAmmo() )
			{
				currentClip = -1;
				maxClip = -1;
				totalAmmo = -1;
			}
			else
			{
				currentClip = pWeapon->Clip1();
				maxClip = pWeapon->GetMaxClip1();
				if ( currentClip < 0 )
				{
					// we don't use clip ammo, just use the total ammo count
					currentClip = pWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );
					totalAmmo = -1;
				}
				else
				{
					// we use clip ammo, so the second ammo is the total ammo
					totalAmmo = pWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );
;
				}
			}
		}
	}


	// Updating flash, slot locking begins...
	bool bSlotIsLocked = false;
	char cNewStr[ 128 ];

	// Update weapon image and text
	if ( m_PrevWeaponID != weaponID )
	{
		LockSlot( true, bSlotIsLocked );

		if ( weaponName )
		{
			SAFECALL( m_CurrentWeaponTextHandle, m_pScaleformUI->Value_SetText( m_CurrentWeaponTextHandle, weaponName ); );
		}

		// Update the selected weapon image as well
		if ( FlashAPIIsValid() && shortWeaponName )
		{
			WITH_SFVALUEARRAY( data, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, shortWeaponName );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "switchWeaponName", data, 1 );
			}
		}

//		CCSWeaponInfo const * pWeaponInfo = GetWeaponInfo( static_cast<CSWeaponID>( weaponID ) );

		CEconItemView *pItem = pWeapon ? pWeapon->GetEconItemView() : NULL;
// 		if ( pWeaponInfo )
// 		{
// 			SAFECALL( m_WeaponPenetration1, m_pScaleformUI->Value_SetVisible( m_WeaponPenetration1, pWeaponInfo->GetAttributeFloat( "penetration", pItem ) == 1 ? true : false ); );
// 			SAFECALL( m_WeaponPenetration2, m_pScaleformUI->Value_SetVisible( m_WeaponPenetration2, pWeaponInfo->GetAttributeFloat( "penetration", pItem ) == 2 ? true : false ); );
// 			SAFECALL( m_WeaponPenetration3, m_pScaleformUI->Value_SetVisible( m_WeaponPenetration3, pWeaponInfo->GetAttributeFloat( "penetration", pItem ) == 3 ? true : false ); );
// 		}

		// update the weapon name
		if ( pWeapon )
		{
			if ( !pItem || !pItem->IsValid() || pItem->GetItemID() <= 0 || !GetItemSchema() || !GetItemSchema()->GetRarityDefinition( pItem->GetRarity() ) )
			{
				SAFECALL( m_WeaponItemName, m_pScaleformUI->Value_SetVisible( m_WeaponItemName, false ); );
			}
			else
			{
				const CEconItemRarityDefinition* pRarity = GetItemSchema()->GetRarityDefinition( pItem->GetRarity() );
				SAFECALL( m_WeaponItemName, m_pScaleformUI->Value_SetVisible( m_WeaponItemName, true ); );
				const int kColorBufSize = 128;
				wchar_t rwchColor[kColorBufSize];
				Q_UTF8ToUnicode( GetHexColorForAttribColor( pRarity->GetAttribColor() ), rwchColor, kColorBufSize );

				// Update target name
				wchar_t wcTargetWeaponFormatted[128];
				V_snwprintf( wcTargetWeaponFormatted, ARRAYSIZE( wcTargetWeaponFormatted ), L"<font color=\"" PRI_WS_FOR_WS L"\">" PRI_WS_FOR_WS L"</font>", rwchColor, pItem->GetItemName() );
				SAFECALL( m_WeaponItemName, m_pScaleformUI->Value_SetTextHTML( m_WeaponItemName, wcTargetWeaponFormatted ); );
			}
		}
	}

	// Determine if this weapon has no ammo at all, so the panel should be hidden
	bHideNoAmmo = (( totalAmmo < 0 ) && ( currentClip < 0 )) || !pWeapon;

	if ( ( bInTRBombMode && CurrTRPoints > 0 ) ||
		 m_bCarryingC4 || m_bCarryingDefuse || m_bInBombZone || weaponID == WEAPON_KNIFE )
	{
		// Ensure we still show this when in TR Bomb AND we have kill points to display
		//  or if we have bomb/defuse kit or are in the bomb zone
		bHideNoAmmo = false;
	}

	// Update ammo type/count/animating bullets elements

	if ( 
		( totalAmmo != m_PrevAmmoTotalCount ) ||
		( currentClip != m_PrevAmmoClipCount ) ||
		( weaponID != m_PrevWeaponID ) )
	{
		LockSlot( true, bSlotIsLocked );

		// Display current weapon ammunition (or lack thereof)
		bool bAmmoShown = true;
		if ( totalAmmo < 0 )
		{
			// doesn't use ammo at all (knife, c4, etc)
			cNewStr[0] = 0;
			bAmmoShown = false;
		}
		else
		{
			V_snprintf( cNewStr, sizeof( cNewStr ), "%d", currentClip );
		}

		if ( bAmmoShown )
		{
			SAFECALL( m_AmmoTextClipHandle, m_pScaleformUI->Value_SetText( m_AmmoTextClipHandle, cNewStr ); );
			V_snprintf( cNewStr, sizeof( cNewStr ), "/ %d", totalAmmo );
			SAFECALL( m_AmmoTextTotalHandle, m_pScaleformUI->Value_SetText( m_AmmoTextTotalHandle, cNewStr ); );
		}

		SAFECALL( m_AmmoTextClipHandle, m_pScaleformUI->Value_SetVisible( m_AmmoTextClipHandle, bAmmoShown ); );
		SAFECALL( m_AmmoTextTotalHandle, m_pScaleformUI->Value_SetVisible( m_AmmoTextTotalHandle, bAmmoShown ); );
	}

	bool bShowBurstBurst = (pWeapon && pWeapon->WeaponHasBurst() && pWeapon->IsInBurstMode());
	bool bShowBurstSingle = (pWeapon && pWeapon->WeaponHasBurst() && !pWeapon->IsInBurstMode());
	SAFECALL( m_BurstIcons_Burst, m_pScaleformUI->Value_SetVisible( m_BurstIcons_Burst, bShowBurstBurst ); );
	SAFECALL( m_BurstIcons_Single, m_pScaleformUI->Value_SetVisible( m_BurstIcons_Single, bShowBurstSingle ); );
	
	// Show the appropriate ammo for our weapon type
	if ( ( m_PrevAmmoType != ammoType ) ||
		 ( m_PrevAmmoClipCount != currentClip ) || (ammoType != 0 && weaponID != m_PrevWeaponID) )
	{
		if ( m_FlashAPI )
		{
			LockSlot( true, bSlotIsLocked );

			WITH_SFVALUEARRAY( data, 4 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, ammoType );
				m_pScaleformUI->ValueArray_SetElement( data, 1, currentClip );
				m_pScaleformUI->ValueArray_SetElement( data, 2, maxClip );
				m_pScaleformUI->ValueArray_SetElement( data, 3, shortWeaponName );
			
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "updateAmmo", data, 4 );
			}
		}
	}

	// Notify the HUD when we fire - detected by not changing weapon/observed player, and our ammo count decreases
	if ( entityIndex == m_lastEntityIndex && m_PrevWeaponID == weaponID && (ammoType == 0) && m_PrevAmmoClipCount > 0 && currentClip < m_PrevAmmoClipCount )
	{
		if ( m_FlashAPI )
		{
			LockSlot( true, bSlotIsLocked );

			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "weaponFired", NULL, 0 );
		}
	}

	// Update previous state so we know which elements need to be refreshed next time
	m_PrevAmmoClipCount = currentClip;
	m_PrevAmmoTotalCount = totalAmmo;
	m_PrevAmmoType = ammoType;
	m_PrevWeaponID = weaponID;
	m_lastEntityIndex = entityIndex;
	
	LockSlot( false, bSlotIsLocked );

	// Now determine if the panel should be entirely hidden, because our current weapon uses no ammo
	if ( bHideNoAmmo != m_bHiddenNoAmmo )
	{
		if ( m_bActive )
		{
			ShowPanel( !bHideNoAmmo );
		}

		m_bHiddenNoAmmo = bHideNoAmmo;
	}
}

void SFHudWeaponPanel::FireGameEvent( IGameEvent *event )
{
	const char *type = event->GetName();
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return;

	int nPlayerUserID = pLocalPlayer->GetUserID();
	int nEventUserID = event->GetInt( "userid" );

	if ( StringHasPrefix( type, "round_start" ) || 
		 ( StringHasPrefix( type, "player_death" ) && nPlayerUserID == nEventUserID ) ||
		 ( StringHasPrefix( type, "bot_takeover" ) && nEventUserID == nPlayerUserID ) )
	{
		// reset these when the reound restarts of if the player dies
		m_bCarryingC4 = false;
		m_bCarryingDefuse = false;
		SAFECALL( m_BombHandle, m_pScaleformUI->Value_SetVisible( m_BombHandle, false ); );
		SAFECALL( m_DefuseHandle, m_pScaleformUI->Value_SetVisible( m_DefuseHandle, false ); );

		if ( ( StringHasPrefix( type, "player_death" ) && !CSGameRules()->IsWarmupPeriod() ) )
		{
			int nCurIndex = pLocalPlayer->GetPlayerGunGameWeaponIndex();

			int nRequiredKills = 0;
			if ( CSGameRules()->IsPlayingGunGameProgressive() )
				nRequiredKills = CSGameRules()->GetGunGameNumKillsRequiredForWeapon( nCurIndex, pLocalPlayer->GetTeamNumber() );

			WITH_SFVALUEARRAY_SLOT_LOCKED( args, 3 )
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, 0 );
				m_pScaleformUI->ValueArray_SetElement( args, 1, 0 );
				m_pScaleformUI->ValueArray_SetElement( args, 2, nRequiredKills );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setNumberKills", args, 3 );
			}
		}

		if ( CSGameRules()->IsPlayingGunGameProgressive() )
		{
			m_LastNumRoundKills = -1;
		}
	}
}

static void GetTextBoxForElement( IScaleformUI *pScaleformUI, SFVALUE root, const char *elementName, const char *textElementName, SFVALUE &sfv )
{
	SFVALUE TempHandle = pScaleformUI->Value_GetMember( root, elementName );

	if ( TempHandle )
	{
		sfv = pScaleformUI->Value_GetMember( TempHandle, textElementName );
		pScaleformUI->ReleaseValue( TempHandle );
	}
}

void SFHudWeaponPanel::FlashReady( void )
{
	m_PanelHandle = m_pScaleformUI->Value_GetMember( m_FlashAPI, "HudPanel" );
	
	if ( m_PanelHandle )
	{
		SFVALUE AnimatedPanelHandle = m_pScaleformUI->Value_GetMember( m_PanelHandle, "WeaponPanel" );
		
		if ( AnimatedPanelHandle )
		{
			m_CurrentWeaponImageHandle = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "CurrentWeapon" );
			GetTextBoxForElement( m_pScaleformUI, AnimatedPanelHandle, "WeaponText", "TextBox", m_CurrentWeaponTextHandle );

			GetTextBoxForElement( m_pScaleformUI, AnimatedPanelHandle, "AmmoCountClip", "TextBox", m_AmmoTextClipHandle );
			GetTextBoxForElement( m_pScaleformUI, AnimatedPanelHandle, "AmmoCountTotal", "TextBox", m_AmmoTextTotalHandle );
			GetTextBoxForElement( m_pScaleformUI, AnimatedPanelHandle, "WeaponName", "TextBox", m_WeaponItemName );
			m_AmmoAnimationHandle = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "AmmoAnim" );

			m_BurstIcons_Burst = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "BurstTypeBurst" );
			m_BurstIcons_Single = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "BurstTypeSingle" );

			m_WeaponPenetration1 = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "Penetration1" );
			m_WeaponPenetration2 = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "Penetration2" );
			m_WeaponPenetration3 = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "Penetration3" );
			m_UpgradeKill1 = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "Kill1" );
			m_UpgradeKill2 = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "Kill2" );
			m_UpgradeKillText = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "UpgradeText" );

			m_BombHandle = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "BombCarrierIcon" );
			m_DefuseHandle = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "DefuseKitIcon" );
			m_BombZoneHandle = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "InBombZoneIcon" );

			SAFECALL( m_UpgradeKill1, m_pScaleformUI->Value_SetVisible( m_UpgradeKill1, false); );
			SAFECALL( m_UpgradeKill2, m_pScaleformUI->Value_SetVisible( m_UpgradeKill2, false); );
			SAFECALL( m_UpgradeKillText, m_pScaleformUI->Value_SetVisible( m_UpgradeKillText, false); );

			SAFECALL( m_BombHandle, m_pScaleformUI->Value_SetVisible( m_BombHandle, false ); );
			SAFECALL( m_DefuseHandle, m_pScaleformUI->Value_SetVisible( m_DefuseHandle, false ); );
			SAFECALL( m_BombZoneHandle, m_pScaleformUI->Value_SetVisible( m_BombZoneHandle, false ); );

			SAFECALL( m_WeaponItemName, m_pScaleformUI->Value_SetVisible( m_WeaponItemName, false ); );

			m_pScaleformUI->ReleaseValue( AnimatedPanelHandle );
		}
	}

	if ( m_FlashAPI && m_pScaleformUI )
	{	
		ListenForGameEvent( "round_start" );
		ListenForGameEvent( "player_death" );
		ListenForGameEvent( "bot_takeover" );
	}

	// hide everything initially
	SetVisible( false );
}

bool SFHudWeaponPanel::PreUnloadFlash( void )
{
 	SafeReleaseSFVALUE( m_PanelHandle );
	SafeReleaseSFVALUE( m_CurrentWeaponImageHandle );
	SafeReleaseSFVALUE( m_CurrentWeaponTextHandle );
	SafeReleaseSFVALUE( m_AmmoTextClipHandle );
	SafeReleaseSFVALUE( m_AmmoTextTotalHandle );
	SafeReleaseSFVALUE( m_AmmoAnimationHandle );
	SafeReleaseSFVALUE( m_BurstIcons_Burst );
	SafeReleaseSFVALUE( m_BurstIcons_Single );
	SafeReleaseSFVALUE( m_WeaponPenetration1 );
	SafeReleaseSFVALUE( m_WeaponPenetration2 );
	SafeReleaseSFVALUE( m_WeaponPenetration3 );
	SafeReleaseSFVALUE( m_UpgradeKill1 );
	SafeReleaseSFVALUE( m_UpgradeKill2 );
	SafeReleaseSFVALUE( m_UpgradeKillText );
	SafeReleaseSFVALUE( m_BombHandle );
	SafeReleaseSFVALUE( m_DefuseHandle );
	SafeReleaseSFVALUE( m_BombZoneHandle );
	SafeReleaseSFVALUE( m_WeaponItemName );

	return true;
}

void SFHudWeaponPanel::LevelInit( void )
{
	if ( !FlashAPIIsValid() )
	{
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFHudWeaponPanel, this, WeaponModule );
	}
	else
	{
		// When initially loaded, hide this panel
		SetVisible( false );
	}

	// Reset all transient data
	m_PrevAmmoClipCount = -1;
	m_PrevAmmoTotalCount = -1;
	m_PrevAmmoType = -1;
	m_PrevWeaponID = -1;
	m_PrevTRGunGameUpgradePoints = 0;
	m_bHiddenNoAmmo = false;
}

void SFHudWeaponPanel::LevelShutdown( void )
{
	if ( FlashAPIIsValid() )
	{
		RemoveFlashElement();
	}
}

bool SFHudWeaponPanel::ShouldDraw( void )
{
	return cl_drawhud.GetBool() && cl_draw_only_deathnotices.GetBool() == false && CHudElement::ShouldDraw();
}


void SFHudWeaponPanel::SetActive( bool bActive )
{
	// Do not show the panel if we have hidden it because of an ammo-less weapon
	if ( bActive != m_bActive && !m_bHiddenNoAmmo )
	{
		ShowPanel( bActive );
	}

	CHudElement::SetActive( bActive );
}

