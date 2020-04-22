//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Weapon selection hud
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "sfweaponselection.h"
#include "hud_macros.h"
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include "iclientmode.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_element_helper.h"
#include "iclientmode.h"
#include "scaleformui/scaleformui.h"
#include "weapon_selection.h"
#include "weapon_basecsgrenade.h"
#include "hltvreplaysystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


extern ConVar hud_drawhistory_time;

DECLARE_HUDELEMENT( SFWeaponSelection );
//
SFUI_BEGIN_GAME_API_DEF
SFUI_END_GAME_API_DEF( SFWeaponSelection, WeaponSelected );

void showloadoutCallBack( IConVar *var, const char *pOldString, float flOldValue );

ConVar cl_showloadout( "cl_showloadout", "1", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS , "Toggles display of current loadout.", showloadoutCallBack );
ConVar cl_loadout_colorweaponnames( "cl_loadout_colorweaponnames", "0", FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS , "If set to true, the weapon names are colored in the weapon loadout to match their rarity." );
ConVar cl_inv_showdividerline( "cl_inv_showdividerline", "0", FCVAR_ARCHIVE | FCVAR_SS , "If 1, will show a divider line above the grenades in the inventory panel." );
extern ConVar cl_draw_only_deathnotices;

void showloadoutCallBack( IConVar *var, const char *pOldString, float flOldValue )
{
	if ( !C_BasePlayer::GetLocalPlayer() || !engine->IsLocalPlayerResolvable() )
		return;

	SFWeaponSelection *pHudWS = GET_HUDELEMENT( SFWeaponSelection );
	if ( pHudWS )
	{
		pHudWS->SetAlwaysShow( cl_showloadout.GetBool( ) );
	}
}

CON_COMMAND_F( show_loadout_toggle, "Toggles loadout display", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	cl_showloadout.SetValue( !cl_showloadout.GetBool() );
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SFWeaponSelection::SFWeaponSelection( const char *value ) : SFHudFlashInterface( value ),
	m_bVisible( false ),
	m_anchorPanel( NULL )
{		
	SetHiddenBits( HIDEHUD_PLAYERDEAD | HIDEHUD_MISCSTATUS | HIDEHUD_WEAPONSELECTION );
	m_flFadeStartTime = 0;
	m_nPrevWepAlignSlot = -1;
	m_nPrevOccupiedWepSlot = -1;

	m_nOrigXPos = 0;
	m_nOrigYPos = 0;
	m_bInitPos = false;

	m_bInitialized = false;

	m_bC4IsVisible = false;

	m_bAlwaysShow = cl_showloadout.GetBool();

	m_bCreatedNextPanel = false;
	m_ggNextPanel.handle = NULL;

	m_nLastTRKills = 0;
	m_nLastGGWepIndex = 0;
	m_bUpdateGGNextPanel = false;
	m_flUpdateInventoryAt = -1;
	m_bUpdateInventoryReset = false;

	m_bSpectatorTargetIndex = -1;

	V_memset( m_weaponPanels, 0, sizeof( m_weaponPanels ) );
}


C_CSPlayer* SFWeaponSelection::GetLocalOrHudPlayer( void )
{
	if ( !g_HltvReplaySystem.GetHltvReplayDelay() )
	{
		int nMaxPlayers = CCSGameRules::GetMaxPlayers();

		if ( C_CSPlayer::GetLocalCSPlayer() && C_CSPlayer::GetLocalCSPlayer()->GetObserverTarget() && ( nMaxPlayers <= 10 || C_CSPlayer::GetLocalCSPlayer()->IsHLTV() ) )
		{
			return C_CSPlayer::GetLocalCSPlayer();
		}
	}

	return GetHudPlayer();
}

void SFWeaponSelection::AddWeapon( C_BaseCombatWeapon *pWeapon, bool bSelected )
{
	if ( !pWeapon || !C_CSPlayer::GetLocalCSPlayer() )
		return;

	int nWepSlot = pWeapon->GetSlot();
	int nWepPos = pWeapon->GetPosition();

	SFVALUE panelHandle = NULL;
	CEconItemView *pItem = pWeapon->GetEconItemView();

	CWeaponCSBase *pCSWeapon = ( CWeaponCSBase* )pWeapon;
	C_BasePlayer *pPlayer = GetLocalOrHudPlayer();
	if ( !pCSWeapon || !pPlayer || !pCSWeapon->GetPlayerOwner() || pCSWeapon->GetPlayerOwner() != pPlayer )
		return;

	bool bShowWeaponColor = ( pItem && pItem->IsValid() && pItem->GetItemID() > 0 && !pCSWeapon->IsKindOf( WEAPONTYPE_GRENADE ) );

	WITH_SLOT_LOCKED
	{
		WITH_SFVALUEARRAY( data, 4 )
		{
			const int kColorBufSize = 128;
			wchar_t rwchColor[kColorBufSize];
			if ( bShowWeaponColor )
			{
				const CEconItemRarityDefinition *pRarity = GetItemSchema()->GetRarityDefinition( pItem->GetRarity() );
				Q_UTF8ToUnicode( GetHexColorForAttribColor( pRarity->GetAttribColor() ), rwchColor, kColorBufSize );
			}

			m_pScaleformUI->ValueArray_SetElement( data, 0, m_weaponPanels[nWepSlot][nWepPos].handle );
			m_pScaleformUI->ValueArray_SetElement( data, 1, (pCSWeapon && pCSWeapon->GetPlayerOwner()) ? pCSWeapon->GetPlayerOwner()->GetTeamNumber() : 0 );
			m_pScaleformUI->ValueArray_SetElement( data, 2, ((pItem && pItem->IsValid()) ? pItem->GetItemDefinition()->GetDefinitionName() : pWeapon->GetName()) );
			m_pScaleformUI->ValueArray_SetElement( data, 3, (bShowWeaponColor ? rwchColor : L"") );
			panelHandle = m_pScaleformUI->Value_Invoke( m_FlashAPI, "AddWeaponData", data, 4 );
		}
	}

	if ( panelHandle && m_pScaleformUI->Value_GetType( panelHandle ) == IScaleformUI::VT_DisplayObject )
	{
		//Put the new panel in the list
		SafeReleaseSFVALUE( m_weaponPanels[nWepSlot][nWepPos].handle );
		m_weaponPanels[nWepSlot][nWepPos].handle = panelHandle;
		m_weaponPanels[nWepSlot][nWepPos].alpha = 255;
		m_weaponPanels[nWepSlot][nWepPos].hWeapon = pWeapon;
		m_weaponPanels[nWepSlot][nWepPos].bSelected = bSelected;
		m_weaponPanels[nWepSlot][nWepPos].bJustPickedUp = !C_CSPlayer::GetLocalCSPlayer()->GetObserverTarget();
		m_weaponPanels[nWepSlot][nWepPos].fLastBlinkTime = gpGlobals->curtime;
		m_weaponPanels[nWepSlot][nWepPos].fEndBlinkTime = gpGlobals->curtime + WEAPON_SELECTION_FADE_DELAY;

		ScaleformDisplayInfo dinfo;
		m_pScaleformUI->Value_GetDisplayInfo( panelHandle, &dinfo );
		dinfo.SetVisibility( true );
		m_pScaleformUI->Value_SetDisplayInfo( panelHandle, &dinfo );
	}

	if ( CSGameRules() && CSGameRules()->IsPlayingGunGameProgressive() )
	{
		if ( !m_bCreatedNextPanel )
		{
			CreateGGNextPanel();
		}

		UpdateGGNextPanel();
	}

	// force a weapon switch to catch where we got a user message but not the network update, yet
	m_flUpdateInventoryAt = gpGlobals->curtime + 0.1;
}

void SFWeaponSelection::UpdateGGNextPanel( bool bForceShowForTRBomb, bool bKnifeReached )
{
	// check if it already exists
	if ( m_ggNextPanel.handle == NULL || !m_bCreatedNextPanel )
	{
		CreateGGNextPanel();
	}

	if ( !FlashAPIIsValid() || m_pScaleformUI->Value_GetType( m_ggNextPanel.handle ) != IScaleformUI::VT_DisplayObject )
		return;

	bool bIsSpectating = !!(C_CSPlayer::GetLocalCSPlayer()->GetObserverTarget());
	// use the spectator target if we have one, otherwise use the local player
	C_CSPlayer *pPlayer = (C_CSPlayer::GetLocalCSPlayer() && bIsSpectating) ? dynamic_cast<C_CSPlayer*>(C_CSPlayer::GetLocalCSPlayer()->GetObserverTarget()) : C_CSPlayer::GetLocalCSPlayer();
	if ( !pPlayer )
		return;

	bool bIsPlayingProgressive = CSGameRules()->IsPlayingGunGameProgressive();
	int nCurrentIndex = pPlayer->GetPlayerGunGameWeaponIndex();
	int nIndexOffset = 0;
	if ( bIsPlayingProgressive )
		nIndexOffset = -1;

	//GetPlayerGunGameWeaponIndex( void ) { return m_iGunGameProgressiveWeaponIndex; }
	int nextWeaponID = CSGameRules()->GetNextGunGameWeapon( MAX( 0, nCurrentIndex+nIndexOffset ), pPlayer->GetTeamNumber() );
	int nextWeaponID1 = CSGameRules()->GetNextGunGameWeapon( nCurrentIndex+1+nIndexOffset, pPlayer->GetTeamNumber() );
	int nextWeaponID2 = CSGameRules()->GetNextGunGameWeapon( nCurrentIndex+2+nIndexOffset, pPlayer->GetTeamNumber() );
	int nextWeaponID3 = CSGameRules()->GetNextGunGameWeapon( nCurrentIndex+3+nIndexOffset, pPlayer->GetTeamNumber() );
	int nextWeaponID4 = CSGameRules()->GetNextGunGameWeapon( nCurrentIndex + 4 + nIndexOffset, pPlayer->GetTeamNumber() );
	int nextWeaponID5 = CSGameRules()->GetNextGunGameWeapon( nCurrentIndex + 5 + nIndexOffset, pPlayer->GetTeamNumber() );


	const CEconItemDefinition *pNextWeaponDef = GetItemSchema()->GetItemDefinition( nextWeaponID );
	const CEconItemDefinition *pNextWeapon1Def = nextWeaponID1 >= 0 ? GetItemSchema()->GetItemDefinition( nextWeaponID1 ) : NULL;
	//if ( !pNextWeapon1Def || pNextWeapon1Def->GetDefinitionIndex() <= 0 )

	const CEconItemDefinition *pNextWeapon2Def = nextWeaponID2 >= 0 ? GetItemSchema()->GetItemDefinition( nextWeaponID2 ) : NULL;
	const CEconItemDefinition *pNextWeapon3Def = nextWeaponID3 >= 0 ? GetItemSchema()->GetItemDefinition( nextWeaponID3 ) : NULL;
	const CEconItemDefinition *pNextWeapon4Def = nextWeaponID4 >= 0 ? GetItemSchema()->GetItemDefinition( nextWeaponID4 ) : NULL;
	const CEconItemDefinition *pNextWeapon5Def = nextWeaponID5 >= 0 ? GetItemSchema()->GetItemDefinition( nextWeaponID5 ) : NULL;

	bool bVisible = true;

	int nMaxIndex = CSGameRules()->GetNumProgressiveGunGameWeapons( pPlayer->GetTeamNumber() )-1;
	int nCurIndex = bKnifeReached ? nMaxIndex : (float)pPlayer->GetPlayerGunGameWeaponIndex();
	int nCurTRKills = pPlayer->GetNumGunGameTRKillPoints();
	if ( nCurIndex == nMaxIndex || ( CSGameRules()->IsPlayingGunGameTRBomb() && !bForceShowForTRBomb ) )
		bVisible = false;

	const char *szGrenName = NULL;

	if ( bForceShowForTRBomb )
	{
		int nBonusGrenade = CSGameRules()->GetGunGameTRBonusGrenade( pPlayer );

		if ( nBonusGrenade == WEAPON_MOLOTOV || nBonusGrenade == WEAPON_INCGRENADE )
		{
			if ( pPlayer->GetTeamNumber() == TEAM_CT )
				szGrenName = "weapon_incgrenade";
			else
				szGrenName = "weapon_molotov";
		}
		else if ( nBonusGrenade == WEAPON_FLASHBANG )
		{
			szGrenName = "weapon_flashbang";
		}
		else if ( nBonusGrenade == WEAPON_HEGRENADE )
		{
			szGrenName = "weapon_hegrenade";
		}
	}

	//float flProgressFrac = (float)nCurIndex / (float)nMaxIndex;

	WITH_SLOT_LOCKED
	{
		if ( bVisible )
		{
			SFVALUE panelHandle = NULL;
			
			WITH_SFVALUEARRAY( data, 13 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, m_ggNextPanel.handle );
				m_pScaleformUI->ValueArray_SetElement( data, 1, pNextWeaponDef ? pNextWeaponDef->GetDefinitionName() : "" );
				m_pScaleformUI->ValueArray_SetElement( data, 2, pPlayer ? pPlayer->GetTeamNumber() : 0 );
				m_pScaleformUI->ValueArray_SetElement( data, 3, nCurIndex );
				m_pScaleformUI->ValueArray_SetElement( data, 4, nMaxIndex );
				m_pScaleformUI->ValueArray_SetElement( data, 5, szGrenName );
				m_pScaleformUI->ValueArray_SetElement( data, 6, !bIsSpectating && bIsPlayingProgressive );  // show the gun drop animation in progressive
				m_pScaleformUI->ValueArray_SetElement( data, 7, !bIsSpectating && (m_nLastGGWepIndex != nCurIndex || m_nLastTRKills != nCurTRKills) );  // show the animation if the new weapon is different from the last
				m_pScaleformUI->ValueArray_SetElement( data, 8, (pNextWeapon1Def && pNextWeapon1Def->GetDefinitionIndex() > 0) ? pNextWeapon1Def->GetDefinitionName() : ((nextWeaponID1 >= 0) ? WeaponIdAsString((CSWeaponID)nextWeaponID1) : "") );
				m_pScaleformUI->ValueArray_SetElement( data, 9, (pNextWeapon2Def && pNextWeapon2Def->GetDefinitionIndex() > 0) ? pNextWeapon2Def->GetDefinitionName() : ((nextWeaponID2 >= 0) ? WeaponIdAsString((CSWeaponID)nextWeaponID2) : "") );
				m_pScaleformUI->ValueArray_SetElement( data, 10, (pNextWeapon3Def && pNextWeapon3Def->GetDefinitionIndex() > 0) ? pNextWeapon3Def->GetDefinitionName() : ((nextWeaponID3 >= 0) ? WeaponIdAsString((CSWeaponID)nextWeaponID3) : "") );
				m_pScaleformUI->ValueArray_SetElement( data, 11, (pNextWeapon4Def && pNextWeapon4Def->GetDefinitionIndex() > 0) ? pNextWeapon4Def->GetDefinitionName() : ((nextWeaponID4 >= 0) ? WeaponIdAsString((CSWeaponID)nextWeaponID4) : "") );
				m_pScaleformUI->ValueArray_SetElement( data, 12, (pNextWeapon5Def && pNextWeapon5Def->GetDefinitionIndex() > 0) ? pNextWeapon5Def->GetDefinitionName() : ((nextWeaponID5 >= 0) ? WeaponIdAsString((CSWeaponID)nextWeaponID5) : "") );

				panelHandle = m_pScaleformUI->Value_Invoke( m_FlashAPI, "AddNextWeaponData", data, 13 );
			}

			if ( panelHandle )
			{
				//Put the new panel in the list
				SafeReleaseSFVALUE( m_ggNextPanel.handle );
				m_ggNextPanel.handle = panelHandle;
				m_ggNextPanel.alpha = 255;
				m_ggNextPanel.hWeapon = NULL;
				m_ggNextPanel.bSelected = false;
				m_ggNextPanel.bJustPickedUp = !bIsSpectating;
				m_ggNextPanel.fLastBlinkTime = gpGlobals->curtime;
				m_ggNextPanel.fEndBlinkTime = gpGlobals->curtime + WEAPON_SELECTION_FADE_DELAY;

				SFVALUE root = g_pScaleformUI->Value_GetMember( m_ggNextPanel.handle, "Panel" );
				if ( root )
				{
					ISFTextObject* textPanel =	g_pScaleformUI->TextObject_MakeTextObjectFromMember( root, "TextPanel" );	
					wchar_t *tempString = g_pVGuiLocalize->Find( (bIsPlayingProgressive && pNextWeapon1Def) ? pNextWeapon1Def->GetItemBaseName() : (pNextWeaponDef ? pNextWeaponDef->GetItemBaseName() : "" ) );
					if ( textPanel && tempString )
					{
						if ( CSGameRules()->IsPlayingGunGameTRBomb() )
						{
							textPanel->SetText( "" );
						}
						else
						{
							textPanel->SetText( tempString );
						}
					
						SafeReleaseSFTextObject( textPanel );
					}

					SFVALUE bgPanel = g_pScaleformUI->Value_GetMember( root, "bgPanel" );
					if ( bgPanel )
					{
						ScaleformDisplayInfo dinfobg;
						m_pScaleformUI->Value_GetDisplayInfo( bgPanel, &dinfobg );
					
						if ( CSGameRules()->IsPlayingGunGameTRBomb() )
							dinfobg.SetVisibility( false );
						else
							dinfobg.SetVisibility( true );

						m_pScaleformUI->Value_SetDisplayInfo( bgPanel, &dinfobg );

						SafeReleaseSFVALUE( bgPanel );
					}

					ISFTextObject* titlePanel =	g_pScaleformUI->TextObject_MakeTextObjectFromMember( root, "TitlePanel" );	
					wchar_t *tempString2 = NULL;
					if ( CSGameRules()->IsPlayingGunGameTRBomb() )
						tempString2 = g_pVGuiLocalize->Find( "#SFUI_WS_GG_AwardNextRound" );
					else
						tempString2 = g_pVGuiLocalize->Find( "#SFUI_WS_GG_NextWep" );
	
					if ( titlePanel && tempString2 )
					{
						titlePanel->SetText( tempString2 );
						SafeReleaseSFTextObject( titlePanel );
					}
				
					SafeReleaseSFVALUE( root );
				}
			}
		}

		ScaleformDisplayInfo dinfo;
		m_pScaleformUI->Value_GetDisplayInfo( m_ggNextPanel.handle, &dinfo );
		dinfo.SetVisibility( bVisible );
		m_pScaleformUI->Value_SetDisplayInfo( m_ggNextPanel.handle, &dinfo );
	}

	m_nLastTRKills = nCurTRKills;
	m_nLastGGWepIndex = nCurIndex;
}

void SFWeaponSelection::HideWeapon( int nSlot, int nPos )
{
	if ( !m_weaponPanels[nSlot][nPos].handle || m_pScaleformUI->Value_GetType( m_weaponPanels[nSlot][nPos].handle ) != IScaleformUI::VT_DisplayObject )
		return;

	ScaleformDisplayInfo dinfo;
	if ( m_weaponPanels[nSlot][nPos].hWeapon.Get() )
	{
		m_pScaleformUI->Value_GetDisplayInfo( m_weaponPanels[nSlot][nPos].handle, &dinfo );
		dinfo.SetVisibility( false );
		m_pScaleformUI->Value_SetDisplayInfo( m_weaponPanels[nSlot][nPos].handle, &dinfo );
	}
}

void SFWeaponSelection::RemoveWeapon( int nSlot, int nPos )
{
	if ( nSlot< 0 || nSlot >= MAX_WEP_SELECT_PANELS )
		return;

	if ( m_weaponPanels[nSlot][nPos].handle && m_pScaleformUI->Value_GetType( m_weaponPanels[nSlot][nPos].handle ) == IScaleformUI::VT_DisplayObject )
	{
		m_weaponPanels[nSlot][nPos].hWeapon = NULL;

		ScaleformDisplayInfo dinfo;
		m_pScaleformUI->Value_GetDisplayInfo( m_weaponPanels[nSlot][nPos].handle, &dinfo );
		dinfo.SetVisibility( false );
		m_pScaleformUI->Value_SetDisplayInfo( m_weaponPanels[nSlot][nPos].handle, &dinfo );
	}

	// force a weapon switch to catch where we got a user message but not the network update, yet
	m_flUpdateInventoryAt = gpGlobals->curtime + 0.1;
}

WeaponSelectPanel SFWeaponSelection::CreateNewPanel( C_BaseCombatWeapon *pWeapon, bool bSelected )
{
	WeaponSelectPanel newPanel;

	if ( pWeapon == NULL || C_CSPlayer::GetLocalCSPlayer() == NULL )
	{
		bSelected = false;
	}

	SFVALUE newPanelHandle = NULL;
	WITH_SLOT_LOCKED
	{
		newPanelHandle = m_pScaleformUI->Value_Invoke( m_FlashAPI, "AddPanel", NULL, 0 );
	}

	if ( newPanelHandle && m_pScaleformUI->Value_GetType( newPanelHandle ) == IScaleformUI::VT_DisplayObject )
	{
		//Put the new panel in the list
		newPanel.handle = newPanelHandle;
		newPanel.alpha = 255;
		newPanel.hWeapon = pWeapon;
		newPanel.bSelected = bSelected;
		newPanel.bJustPickedUp = !C_CSPlayer::GetLocalCSPlayer()->GetObserverTarget();
		newPanel.fLastBlinkTime = gpGlobals->curtime;
		newPanel.fEndBlinkTime = gpGlobals->curtime + WEAPON_SELECTION_FADE_DELAY;
	}

	return newPanel;
}

void SFWeaponSelection::CreateGGNextPanel( void )
{
	// check if it already exists
	if ( !FlashAPIIsValid() || m_ggNextPanel.handle != NULL )
		return;
	
	WeaponSelectPanel newPanel;

	SFVALUE newPanelHandle = NULL;
	WITH_SLOT_LOCKED
	{
		newPanelHandle = m_pScaleformUI->Value_Invoke( m_FlashAPI, "AddNextPanel", NULL, 0 );
	}

	if ( newPanelHandle && m_pScaleformUI->Value_GetType( newPanelHandle ) == IScaleformUI::VT_DisplayObject )
	{
		//Put the new panel in the list
		newPanel.handle = newPanelHandle;
		newPanel.alpha = 255;
		newPanel.hWeapon = NULL;
		newPanel.bSelected = false;
		newPanel.bJustPickedUp = false;
		newPanel.fLastBlinkTime = gpGlobals->curtime;
		newPanel.fEndBlinkTime = gpGlobals->curtime + WEAPON_SELECTION_FADE_DELAY;
	}

	m_ggNextPanel = newPanel;
	m_bCreatedNextPanel = true;
}

void SFWeaponSelection::ShowAndUpdateSelection( int nType, C_BaseCombatWeapon *pWeapon, bool bGiveInitial )
{
	if ( !m_pScaleformUI )
		return;

	if ( !pWeapon || pWeapon->IsMarkedForDeletion() )
	{
		pWeapon = NULL;
		nType = WEPSELECT_SWITCH;
	}

	C_BasePlayer *pPlayer = GetLocalOrHudPlayer();
	if ( !pPlayer || !engine->IsLocalPlayerResolvable() )
		return;

	if ( !m_bInitialized )
	{
		for ( int i=0; i < MAX_WEP_SELECT_PANELS; i++ )
		{
			for ( int j=0; j < MAX_WEP_SELECT_POSITIONS; j++ )
			{
				WeaponSelectPanel panel = CreateNewPanel();

				if ( panel.handle && m_pScaleformUI->Value_GetType( panel.handle ) == IScaleformUI::VT_DisplayObject )
				{
					WITH_SLOT_LOCKED
					{
						panel.hWeapon = NULL;
						ScaleformDisplayInfo dinfo;
						m_pScaleformUI->Value_GetDisplayInfo( panel.handle, &dinfo );
						dinfo.SetVisibility( false );
						m_pScaleformUI->Value_SetDisplayInfo( panel.handle, &dinfo );

						m_weaponPanels[i][j] = panel;
					}
				}	
			}
		}

		m_bInitialized = true;
	}

	CHudWeaponSelection *pHudSelection = (CHudWeaponSelection *)GET_HUDELEMENT( CHudWeaponSelection );
	if ( !pHudSelection )
		return;

	m_hSelectedWeapon = (pWeapon && nType == WEPSELECT_SWITCH) ? pWeapon : pPlayer->GetActiveWeapon();

	C_WeaponCSBase *pNextWeapon= NULL;

	switch ( nType )
	{
	default:
	case WEPSELECT_SWITCH :
		{
			bool bFoundSelectedItem = false;
			// loop through all of our slots and update differences
			for ( int i=0; i < MAX_WEP_SELECT_PANELS; i++ )
			{
				for ( int j=0; j < MAX_WEP_SELECT_POSITIONS; j++ )
				{
					C_WeaponCSBase *pPanelWeapon = dynamic_cast<C_WeaponCSBase*>(m_weaponPanels[i][j].hWeapon.Get());
					pNextWeapon = dynamic_cast<C_WeaponCSBase*>(pHudSelection->GetWeaponInSlotForTarget( pPlayer, i, j ));
					CBaseCSGrenade *pGrenade = NULL;

					// we have a weapon in our list, but we don't have it in our inventory, remove it
					if ( pPanelWeapon && !pNextWeapon )
					{
						RemoveWeapon( i, j );
					}
					else if ( pNextWeapon && !pPanelWeapon )
					{
						if ( IsGrenadeWeapon( pNextWeapon->GetCSWeaponID() ) )
						{
							pGrenade = static_cast<CBaseCSGrenade*>( pNextWeapon );
						}
						// this is an edge case where this happens when the very first round starts
						// but we can't add grenades back after they've been thrown because they are set as thrown before they've left our inventory.....
						// if it's not a grenade, OR if its a grenade and hasn't been thrown, add it back
						// we are awarded bonus grenades late during gun gun arsenal mode, so we have to catch them here
						if ( !pGrenade || (pGrenade && !pGrenade->IsPinPulled() && !pGrenade->IsBeingThrown()) )
						{
							AddWeapon( pNextWeapon, (GetSelectedWeapon() == pNextWeapon) );
						}
					}
					
					if ( pPanelWeapon && pNextWeapon && pPanelWeapon == pNextWeapon )
					{
						bool bJustRemovedGrenade = false;
						if ( IsGrenadeWeapon( pPanelWeapon->GetCSWeaponID() ) )
						{
							pGrenade = static_cast<CBaseCSGrenade*>( pPanelWeapon );
 							bool bShouldRemove = true;
 							if ( pPanelWeapon && IsGrenadeWeapon( pPanelWeapon->GetCSWeaponID() ) )
 							{
 								int ammo = pPanelWeapon->UsesPrimaryAmmo() ? pPanelWeapon->Clip1() : 0;
 								if ( ammo < 0 )
 									ammo = pPanelWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );
 
 								if ( ammo > 0 )
 									bShouldRemove = false;
 							}
 
 							if ( bShouldRemove )
 							{
 								RemoveWeapon( i, j );
 								bJustRemovedGrenade = true;
 							}

//							pGrenade = static_cast<CBaseCSGrenade*>( pPanelWeapon );
//							if ( pGrenade && pGrenade->GetIsThrown() && pGrenade->IsPinPulled() )
//							{
//								RemoveWeapon( i, j );
//								bJustRemovedGrenade = true;
//							}

						}

						// if we just removed the panel because we just threw the grenade, don't count it as selected because it's about to be removed
						if ( GetSelectedWeapon() == pPanelWeapon && !bJustRemovedGrenade )
							bFoundSelectedItem = true;

						m_weaponPanels[i][j].bSelected = (GetSelectedWeapon() == pPanelWeapon);
					}
				}
			}

			// if we didn't find the item selected, try again in a short while
			if ( bFoundSelectedItem == false )
			{
				m_flUpdateInventoryAt = gpGlobals->curtime + 0.1;
			}

			break;
		}

	case WEPSELECT_PICKUP :
		{
			if ( pWeapon )
			{
				int nWepSlot = pWeapon->GetSlot();
				int nWepPos = pWeapon->GetPosition();

				bool bSelected = (pWeapon == GetSelectedWeapon());

				if ( m_weaponPanels[nWepSlot][nWepPos].handle != NULL && m_pScaleformUI->Value_GetType( m_weaponPanels[nWepSlot][nWepPos].handle ) == IScaleformUI::VT_DisplayObject )
				{
					m_weaponPanels[nWepSlot][nWepPos].bSelected = bSelected;

					ScaleformDisplayInfo dinfo;
					m_pScaleformUI->Value_GetDisplayInfo( m_weaponPanels[nWepSlot][nWepPos].handle, &dinfo );

					AddWeapon( pWeapon, bSelected );
				}
			}


			break;
		}

	case WEPSELECT_DROP :
		{
			for ( int i=0; i < MAX_WEP_SELECT_PANELS; i++ )
			{
				for ( int j=0; j < MAX_WEP_SELECT_POSITIONS; j++ )
				{
					C_WeaponCSBase *pPanelWeapon = dynamic_cast<C_WeaponCSBase*>(m_weaponPanels[i][j].hWeapon.Get());
					// we're dropping a weapon so just find it, remove it and be done
					if ( pPanelWeapon && pWeapon && pPanelWeapon == pWeapon )
					{
						bool bShouldRemove = true;
						if ( pPanelWeapon && IsGrenadeWeapon( pPanelWeapon->GetCSWeaponID() ) )
						{
							int ammo = pPanelWeapon->UsesPrimaryAmmo() ? pPanelWeapon->Clip1() : 0;
							if ( ammo < 0 )
								ammo = pPanelWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );

							if ( ammo > 0 )
								bShouldRemove = false;
						}

						if ( bShouldRemove )
							RemoveWeapon( i, j );

						// force a weapon switch to catch where we got a user message but not the network update, yet
						m_flUpdateInventoryAt = gpGlobals->curtime + 0.1;
					}
				}
			}

			break;
		}
	}

	UpdatePanelPositions();

	float flStartFadeTime = gpGlobals->curtime + WEAPON_SELECTION_FADE_DELAY;
	if ( nType == WEPSELECT_DROP )
		flStartFadeTime -= WEAPON_SELECTION_FADE_DELAY/2;

	m_flFadeStartTime = flStartFadeTime;
}

void SFWeaponSelection::UpdatePanelPositions( void )
{
	int nYPos = 0;
	int nXPos = 0;
	bool bFirstTime = true;

	int nStartWepSlot = -1;
	int nStartWepSlotPos = -1;
	bool bReverse = true;
	if ( bReverse )
	{
		nStartWepSlot = MAX_WEAPON_SLOTS;
		nStartWepSlotPos = MAX_WEAPON_POSITIONS;
	}

	if ( !C_BasePlayer::GetLocalPlayer() || !engine->IsLocalPlayerResolvable() )
		 return; 

	CHudWeaponSelection *pHudSelection = (CHudWeaponSelection *)GET_HUDELEMENT( CHudWeaponSelection );
	if ( !pHudSelection )
		return;

	C_CSPlayer *pPlayer = GetLocalOrHudPlayer();
	if ( !pPlayer )
		return;

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return;

	bool bUsingKeyboard = ( false == m_pScaleformUI->IsSetToControllerUI( SF_FULL_SCREEN_SLOT ) );

	// update whether we should always show
	m_bAlwaysShow = cl_showloadout.GetBool();

	bool bIsSpectating = (pLocalPlayer != pPlayer);

	C_WeaponCSBase *pNextWeapon = NULL;
	for ( int i=MAX_WEP_SELECT_PANELS-1; i >= 0; i-- )
	{
		for ( int j=0; j < MAX_WEP_SELECT_POSITIONS; j++ )
		{
			C_WeaponCSBase *pPanelWeapon = static_cast<C_WeaponCSBase*>(m_weaponPanels[i][j].hWeapon.Get());

			pNextWeapon = static_cast<C_WeaponCSBase*>( pHudSelection->GetWeaponInSlotForTarget( pPlayer, i, j ) );
			// little hack checking knives because we remove the first knife given on initial spawn and the panel doesn't pick up the change until next update, so the knife isn't shown on initial spawn int eh first map
			if ( pPanelWeapon && pNextWeapon && (pPanelWeapon == pNextWeapon || (pPanelWeapon->IsA(WEAPON_KNIFE) && pNextWeapon->IsA(WEAPON_KNIFE) ) ) )
			{
				bool bSelected = m_weaponPanels[i][j].bSelected;
				//bool bBuyOpen = pPlayer->IsBuyMenuOpen();
				m_weaponPanels[i][j].alpha = 255;

				// testing
// 				if ( pPanelWeapon->GetCSWeaponID() == WEAPON_C4 )
// 					m_weaponPanels[i][j].alpha = 0;

				if ( bSelected )
				{
					m_selectedPanel = m_weaponPanels[i][j];
				}

				ScaleformDisplayInfo dinfo;
				m_pScaleformUI->Value_GetDisplayInfo( m_weaponPanels[i][j].handle, &dinfo );

				bool bShowCountNumber = false;
				int nSlot = pPanelWeapon->GetSlot();
				if ( nSlot == 3 || (nSlot == 5 && pPanelWeapon->IsKindOf( WEAPONTYPE_STACKABLEITEM ) ) )
					bShowCountNumber = true;

				if ( bFirstTime )
				{
					if ( m_bInitPos == false )
					{
						m_nOrigXPos = dinfo.GetX();
						m_nOrigYPos = dinfo.GetY();;
						m_bInitPos = true;
					}

					nXPos = m_nOrigXPos;

					if ( pPlayer && pPlayer->HasDefuser() )				
					{
						nYPos = m_nOrigYPos - WEAPON_SELECT_PANEL_HEIGHT;
					}
					else
					{
						nYPos = m_nOrigYPos;
					}	

					bFirstTime = false;
				}
				else
				{
					bool bShiftLeft = false;
					if ( m_nPrevWepAlignSlot == nSlot )
					{
						bShiftLeft = true;
					}
					else
					{
						nXPos = m_nOrigXPos;
					}

					if ( bShiftLeft )
					{
						// grenades are 3
						if ( nSlot == 3 )
							nXPos -= WEAPON_SELECT_PANEL_HEIGHT*0.75;
						else
							nXPos -= WEAPON_SELECT_PANEL_HEIGHT*1.25;
					}
					else
					{
						nYPos -= WEAPON_SELECT_PANEL_HEIGHT;

						// if the previous was the grenade (or item) slot, put a gap here to make sense of the controls
						if ( (((m_nPrevOccupiedWepSlot == 3 || m_nPrevOccupiedWepSlot == 2) && cl_inv_showdividerline.GetInt() == 1) || m_nPrevOccupiedWepSlot == cl_inv_showdividerline.GetInt()) && !bUsingKeyboard  )
							nYPos -= WEAPON_SELECT_PANEL_HEIGHT/4;
					}
				}

				WITH_SLOT_LOCKED
				{
					const CCSWeaponInfo* pWeaponInfo = GetWeaponInfo( pPanelWeapon->GetCSWeaponID() );
					CEconItemView *pItem = pPanelWeapon->GetEconItemView();
					SFVALUE root = g_pScaleformUI->Value_GetMember( m_weaponPanels[i][j].handle, "Panel" );
					if ( root && pWeaponInfo )
					{
						SFVALUE iconS = g_pScaleformUI->Value_GetMember( root, "iconSelected" );
						if ( iconS )
						{
							ScaleformDisplayInfo dinfo2;
							m_pScaleformUI->Value_GetDisplayInfo( iconS, &dinfo2 );
							dinfo2.SetVisibility(bSelected);
							m_pScaleformUI->Value_SetDisplayInfo( iconS, &dinfo2 );
							SafeReleaseSFVALUE( iconS );
						}

						SFVALUE iconNS = g_pScaleformUI->Value_GetMember( root, "iconNotSelected" );
						if ( iconNS )
						{
							ScaleformDisplayInfo dinfo2;
							m_pScaleformUI->Value_GetDisplayInfo( iconNS, &dinfo2 );
							dinfo2.SetVisibility(!bSelected);
							m_pScaleformUI->Value_SetDisplayInfo( iconNS, &dinfo2 );
							SafeReleaseSFVALUE( iconNS );
						}

						// find the first panel in our row
						WeaponSelectPanel firstBGPanel;
						for ( int k=0; k < MAX_WEP_SELECT_POSITIONS; k++ )
						{
							// find the first panel in use in our "position" list and that'll be our background
							// and show the key binding
							if ( m_weaponPanels[nSlot][k].hWeapon.Get() )
							{
								firstBGPanel = m_weaponPanels[nSlot][k];
								break;
							}
						}

						SFVALUE bgPanel = g_pScaleformUI->Value_GetMember( root, "bgPanel" );
						SFVALUE divPanel = g_pScaleformUI->Value_GetMember( root, "PanelDivider" );
						if ( bgPanel && divPanel )
						{
							ScaleformDisplayInfo dinfo2;
							m_pScaleformUI->Value_GetDisplayInfo( bgPanel, &dinfo2 );

							dinfo2.SetVisibility( false );
							m_pScaleformUI->Value_SetDisplayInfo( bgPanel, &dinfo2 );
							SafeReleaseSFVALUE( bgPanel );

							// divider
							ScaleformDisplayInfo dinfo2a;
							m_pScaleformUI->Value_GetDisplayInfo( divPanel, &dinfo2a );
							dinfo2a.SetVisibility( false );
							m_pScaleformUI->Value_SetDisplayInfo( divPanel, &dinfo2a );
							SafeReleaseSFVALUE( divPanel );

							if ( firstBGPanel.handle && m_pScaleformUI->Value_GetType( firstBGPanel.handle ) == IScaleformUI::VT_DisplayObject )
							{
								SFVALUE root2 = g_pScaleformUI->Value_GetMember( firstBGPanel.handle, "Panel" );
								if ( root2 )
								{
									if ( bSelected )
									{
										SFVALUE bgPanel2 = g_pScaleformUI->Value_GetMember( root2, "bgPanel" );
										if ( bgPanel2 )
										{
											ScaleformDisplayInfo dinfo3;
											m_pScaleformUI->Value_GetDisplayInfo( bgPanel2, &dinfo3 );
											dinfo3.SetVisibility( true );
											m_pScaleformUI->Value_SetDisplayInfo( bgPanel2, &dinfo3 );
											SafeReleaseSFVALUE( bgPanel2 );
										}
									}

									SFVALUE divPanel = g_pScaleformUI->Value_GetMember( root2, "PanelDivider" );
									if ( divPanel )
									{
										// if we're the first panel in the horizontal list of grenade panels and we aren't using the keyboard, show the divider
										ScaleformDisplayInfo dinfo3;
										m_pScaleformUI->Value_GetDisplayInfo( divPanel, &dinfo3 );

										dinfo3.SetVisibility( !bUsingKeyboard && (((nSlot == 3 || nSlot == 2) && cl_inv_showdividerline.GetInt() == 1) || nSlot == cl_inv_showdividerline.GetInt()) );
										m_pScaleformUI->Value_SetDisplayInfo( divPanel, &dinfo3 );
										SafeReleaseSFVALUE( divPanel );
									}

									SafeReleaseSFVALUE( root2 );
								}
							}
						}

						bool bWeaponIsItem = (pItem && pItem->IsValid());

						wchar_t wcTargetWeaponFormatted[128];
						if ( bWeaponIsItem )
						{
							if ( cl_loadout_colorweaponnames.GetBool() && pItem->GetItemID() > 0 )
							{
								const int kColorBufSize = 128;
								wchar_t rwchColor[kColorBufSize];
								const CEconItemRarityDefinition *pRarity = GetItemSchema()->GetRarityDefinition( pItem->GetRarity() );
								Q_UTF8ToUnicode( GetHexColorForAttribColor( pRarity->GetAttribColor() ), rwchColor, kColorBufSize );
								V_snwprintf( wcTargetWeaponFormatted, ARRAYSIZE( wcTargetWeaponFormatted ), L"<font color=\"" PRI_WS_FOR_WS L"\">" PRI_WS_FOR_WS L"</font>", rwchColor, pItem->GetItemName() );
							}
							else
							{
								V_snwprintf( wcTargetWeaponFormatted, ARRAYSIZE( wcTargetWeaponFormatted ), PRI_WS_FOR_WS, pItem->GetItemName() );
							}	
						}
						else
						{
							V_snwprintf( wcTargetWeaponFormatted, ARRAYSIZE( wcTargetWeaponFormatted ), PRI_WS_FOR_WS, g_pVGuiLocalize->Find( pWeaponInfo->szPrintName ) );
						}

						wchar_t wcTargetWeaponPossessed[128] = { 0 };

						WITH_SFVALUEARRAY( data, 5 )
						{
							m_pScaleformUI->ValueArray_SetElement( data, 0, root );
							m_pScaleformUI->ValueArray_SetElement( data, 1, bWeaponIsItem );
							m_pScaleformUI->ValueArray_SetElement( data, 2, bSelected );
							m_pScaleformUI->ValueArray_SetElement( data, 3, bShowCountNumber );
							m_pScaleformUI->ValueArray_SetElement( data, 4, StringIsEmpty( wcTargetWeaponPossessed ) ? wcTargetWeaponFormatted : wcTargetWeaponPossessed );
							m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetWeaponName", data, 5 );
						}

						ISFTextObject* countPanel =	g_pScaleformUI->TextObject_MakeTextObjectFromMember( root, "CountPanel" );	
						if ( countPanel && pPanelWeapon )
						{
							int ammo = pPanelWeapon->UsesPrimaryAmmo() ? pPanelWeapon->Clip1() : 0;
							if ( ammo < 0 )
								ammo = pPanelWeapon->GetReserveAmmoCount( AMMO_POSITION_PRIMARY );

							countPanel->SetVisible( bShowCountNumber && ammo > 1 );
							wchar_t buf[32];
							V_snwprintf( buf, ARRAYSIZE( buf ), L"(%d)", ammo );
							countPanel->SetText( buf );
							SafeReleaseSFTextObject( countPanel );
						}

						ISFTextObject* bindPanel =	g_pScaleformUI->TextObject_MakeTextObjectFromMember( root, "BindingPanel" );	
						if ( bindPanel )
						{
							if ( !bIsSpectating && bUsingKeyboard && firstBGPanel.handle && firstBGPanel.handle == m_weaponPanels[i][j].handle && m_pScaleformUI->Value_GetType( firstBGPanel.handle ) == IScaleformUI::VT_DisplayObject )
							{
								wchar_t wzFinal[32] = L"";
								wchar_t buf[32];
								V_snwprintf( buf, ARRAYSIZE( buf ), L"%%slot%d%%", i+1 );
								//g_pVGuiLocalize->ConstructString( buf, sizeof( buf ), "\%slot%d\%", 1, i );
								if ( bindPanel && buf )
								{
									UTIL_ReplaceKeyBindings( buf, 0, wzFinal, sizeof( wzFinal ) );

									bindPanel->SetVisible( true );
									bindPanel->SetText( wzFinal );
								}
								else
								{
									bindPanel->SetVisible( false );
								}
							}
							else
							{
								bindPanel->SetVisible( false );
							}

							SafeReleaseSFTextObject( bindPanel );
						}

						SafeReleaseSFVALUE( root );

						m_nPrevOccupiedWepSlot = nSlot;
					}
				}

				dinfo.SetY( nYPos );
				dinfo.SetX( nXPos );
				dinfo.SetVisibility( true );
				dinfo.SetAlpha( 255 );
				m_pScaleformUI->Value_SetDisplayInfo( m_weaponPanels[i][j].handle, &dinfo );

				m_nPrevWepAlignSlot = nSlot;
			}
		}
	}

	if ( CSGameRules() && (CSGameRules()->IsPlayingGunGameTRBomb() || CSGameRules()->IsPlayingGunGameProgressive()) )
	{
		if ( m_ggNextPanel.handle && m_pScaleformUI->Value_GetType( m_ggNextPanel.handle ) == IScaleformUI::VT_DisplayObject )
		{
			ScaleformDisplayInfo dinfo;
			m_pScaleformUI->Value_GetDisplayInfo( m_ggNextPanel.handle, &dinfo );

			nYPos -= WEAPON_SELECT_PANEL_HEIGHT * 1.2f;
			dinfo.SetY( nYPos );
			dinfo.SetX( nXPos );
			//dinfo.SetVisibility( true );
			dinfo.SetAlpha( 255 );
			m_pScaleformUI->Value_SetDisplayInfo( m_ggNextPanel.handle, &dinfo );

			// hide the progress bar in the TRbomb mode
			SFVALUE root = g_pScaleformUI->Value_GetMember( m_ggNextPanel.handle, "Panel" );
			if ( root )
			{
				SFVALUE progress = g_pScaleformUI->Value_GetMember( root, "Progress" );
				if ( progress )
				{
					ScaleformDisplayInfo dinfo2;
					m_pScaleformUI->Value_GetDisplayInfo( progress, &dinfo2 );
					dinfo2.SetVisibility( !CSGameRules()->IsPlayingGunGameTRBomb() );
					m_pScaleformUI->Value_SetDisplayInfo( progress, &dinfo2 );

					SafeReleaseSFVALUE( progress );
				}
				SafeReleaseSFVALUE( root );
			}
		}
	}
}

void SFWeaponSelection::ProcessInput( void )
{
	if ( !m_pScaleformUI )
		return;

	C_CSPlayer *pPlayer = GetLocalOrHudPlayer();
	if ( !pPlayer )
		return;

	int nKillValue = pPlayer->GetNumGunGameTRKillPoints();

	// we need to delay this a frame to allow the next network update to happen
	// if we update it now, the data hasn't been networked down yet
	if ( m_bUpdateGGNextPanel && m_nLastTRKills < nKillValue )
	{
		m_bUpdateGGNextPanel = false;

		UpdateGGNextPanel( true );
		UpdatePanelPositions();
	}

	if ( m_flUpdateInventoryAt > 0 && m_flUpdateInventoryAt < gpGlobals->curtime )
	{
		if ( m_bUpdateInventoryReset )
		{
			RemoveAllItems();
			m_bUpdateInventoryReset = false;
		}
		
		ShowAndUpdateSelection( WEPSELECT_SWITCH );
		m_flUpdateInventoryAt = -1;
		
	}

	if ( ( CSGameRules() && CSGameRules()->IsPlayingGunGameProgressive() ) && 
		pPlayer->GetObserverTarget() && pPlayer->GetObserverTarget()->entindex() != m_bSpectatorTargetIndex )
	{
		m_bSpectatorTargetIndex = pPlayer->GetObserverTarget()->entindex();
		C_CSPlayer *pTargetPlayer = dynamic_cast<C_CSPlayer*>(C_CSPlayer::GetLocalCSPlayer()->GetObserverTarget());
		if ( pTargetPlayer )
			UpdateGGNextPanel();
	}

	// fade each panel
	float deltaTime = gpGlobals->curtime - m_lastUpdate;
	m_lastUpdate = gpGlobals->curtime;

	m_bC4IsVisible = false;

	for ( int i=0; i < MAX_WEP_SELECT_PANELS; i++ )
	{
		for ( int j=0; j < MAX_WEP_SELECT_POSITIONS; j++ )
		{
			if ( m_weaponPanels[i][j].handle == NULL || m_pScaleformUI->Value_GetType( m_weaponPanels[i][j].handle ) != IScaleformUI::VT_DisplayObject )
				continue;

			ScaleformDisplayInfo dinfo;
			m_pScaleformUI->Value_GetDisplayInfo( m_weaponPanels[i][j].handle, &dinfo );

			C_CSPlayer *pLocalPlayer = GetLocalOrHudPlayer();
			if ( pLocalPlayer && !pLocalPlayer->IsAlive() )
			{
				m_weaponPanels[i][j].alpha = 0;
			}
			else
			{
				C_WeaponCSBase *pPanelWeapon = static_cast<C_WeaponCSBase*>(m_weaponPanels[i][j].hWeapon.Get());
				if ( !pPanelWeapon && m_weaponPanels[i][j].alpha > 0 && dinfo.GetVisibility() )
				{
					dinfo.SetVisibility( false );
				}

				if ( m_selectedPanel.alpha <= 0 && !m_bAlwaysShow && !( CSGameRules() && CSGameRules()->IsPlayingGunGameProgressive() ) )
				{
					ScaleformDisplayInfo dinfo2;
					m_pScaleformUI->Value_GetDisplayInfo( m_weaponPanels[i][j].handle, &dinfo2 );
					if ( dinfo2.GetVisibility() )
					{
						dinfo2.SetVisibility( false );
						m_pScaleformUI->Value_SetDisplayInfo( m_weaponPanels[i][j].handle, &dinfo2 );
					}
				}
				else
				{
					if ( pPanelWeapon && pPanelWeapon->IsKindOf( WEAPONTYPE_C4 ) && m_weaponPanels[i][j].alpha > 0 )
						m_bC4IsVisible = true;

					if ( gpGlobals->curtime >= m_flFadeStartTime )
					{
						if ( m_bAlwaysShow || ( CSGameRules() && CSGameRules()->IsPlayingGunGameProgressive() ) )
						{
							m_weaponPanels[i][j].alpha = 255;
						}
						else
						{
							m_weaponPanels[i][j].alpha -= deltaTime * WEAPON_SELECTION_FADE_SPEED;
							m_weaponPanels[i][j].alpha = MAX( 0, m_weaponPanels[i][j].alpha );
						}
					}

					if ( m_weaponPanels[i][j].fEndBlinkTime <= gpGlobals->curtime && m_weaponPanels[i][j].bJustPickedUp )
					{
						m_weaponPanels[i][j].alpha = 255;
						m_weaponPanels[i][j].bJustPickedUp = false;
					}
					else if ( m_weaponPanels[i][j].bJustPickedUp )
					{
						if ( m_weaponPanels[i][j].fLastBlinkTime + (WEAPON_SELECTION_FADE_DELAY/7) <= gpGlobals->curtime )
						{
							if ( m_weaponPanels[i][j].alpha == 255 )
								m_weaponPanels[i][j].alpha = 50;
							else
								m_weaponPanels[i][j].alpha = 255;

							m_weaponPanels[i][j].fLastBlinkTime = gpGlobals->curtime;
						}
					}
				}
			}

			dinfo.SetAlpha( m_weaponPanels[i][j].alpha );

			m_pScaleformUI->Value_SetDisplayInfo(m_weaponPanels[i][j].handle, &dinfo );
		}
	}
}

void SFWeaponSelection::LevelInit( void )
{
	if ( !FlashAPIIsValid() )
	{
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFWeaponSelection, this, WeaponSelected );
	}
}

void SFWeaponSelection::LevelShutdown( void )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			for ( int i=0; i < MAX_WEP_SELECT_PANELS; i++ )
			{
				for ( int j=0; j < MAX_WEP_SELECT_POSITIONS; j++ )
				{
					//Remove all items before we exit
					RemoveItem(i,j);
				}
			}
		}

		V_memset( m_weaponPanels, 0, sizeof( m_weaponPanels ) );
		m_bInitialized = false;
		RemoveFlashElement();
	}
}

bool SFWeaponSelection::ShouldDraw( void )
{
	return cl_drawhud.GetBool() && cl_draw_only_deathnotices.GetBool() == false && CHudElement::ShouldDraw();
}


void SFWeaponSelection::SetActive( bool bActive )
{
	if ( FlashAPIIsValid() )
	{
		if ( bActive != m_bVisible )
		{
			ShowPanel( bActive );
		}
	}

	if ( bActive == false && m_bActive == true )
	{
		// We want to continue to run ProcessInput while the HUD element is hidden
		// so that the notifications continue advancing down the screen
		return;
	}

	CHudElement::SetActive( bActive );
}

void SFWeaponSelection::SetAlwaysShow( bool bAlwaysShow )
{
	if ( bAlwaysShow != m_bAlwaysShow )
	{
		if ( bAlwaysShow )
		{
			UpdatePanelPositions();
		}
		else
		{
			// reset the update timer so it begins to fade
			m_flFadeStartTime = gpGlobals->curtime;
		}

		m_bAlwaysShow = bAlwaysShow;
	}
}

void SFWeaponSelection::FlashReady( void )
{
	m_lastUpdate = gpGlobals->curtime;	


	if ( m_FlashAPI && m_pScaleformUI )
	{	
		m_anchorPanel = m_pScaleformUI->Value_GetMember( m_FlashAPI, "Anchor" );

		ListenForGameEvent( "round_prestart" );
		ListenForGameEvent( "round_start" );
		ListenForGameEvent( "player_death" );
		ListenForGameEvent( "gg_player_impending_upgrade" );
		ListenForGameEvent( "ggprogressive_player_levelup" );
		ListenForGameEvent( "bot_takeover" );
		ListenForGameEvent( "buymenu_open" );
		ListenForGameEvent( "buymenu_close" );
		ListenForGameEvent( "spec_mode_updated" );
		ListenForGameEvent( "spec_target_updated" );
		ListenForGameEvent( "hltv_changed_mode" );
	}
}

bool SFWeaponSelection::PreUnloadFlash( void )
{
	SafeReleaseSFVALUE( m_anchorPanel );
	if ( m_ggNextPanel.handle && m_pScaleformUI->Value_GetType( m_ggNextPanel.handle ) == IScaleformUI::VT_DisplayObject )
	{
		SafeReleaseSFVALUE( m_ggNextPanel.handle );
		m_bCreatedNextPanel = false;
	}

	for ( int i=0; i < MAX_WEP_SELECT_PANELS; i++ )
	{
		for ( int j=0; j < MAX_WEP_SELECT_POSITIONS; j++ )
		{
			if ( m_weaponPanels[i][j].handle )
				SafeReleaseSFVALUE( m_weaponPanels[i][j].handle );
		}
	}

	return true;
}

void SFWeaponSelection::FireGameEvent( IGameEvent *event )
{
	const char *type = event->GetName();
	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalPlayer )
		return;

	int nPlayerUserID = pLocalPlayer->GetUserID();
	int nEventUserID = event->GetInt( "userid" );

	bool bLocalPlayerDeath = false;
	if ( StringHasPrefix( type, "player_death" ) && nPlayerUserID == nEventUserID )
		bLocalPlayerDeath = true;


	if ( StringHasPrefix( type, "round_start" ) )
	{
		ShowAndUpdateSelection( WEPSELECT_SWITCH );
		m_nLastTRKills = 0;

		if ( CSGameRules()->IsPlayingGunGameProgressive() )
		{
			UpdateGGNextPanel( CSGameRules()->IsPlayingGunGameTRBomb(), false );
		}
	}

	if ( CSGameRules() && (CSGameRules()->IsPlayingGunGameTRBomb() || CSGameRules()->IsPlayingGunGameProgressive()) )
	{
		if ( bLocalPlayerDeath || (Q_strcmp( "bot_takeover", type ) == 0 || Q_strcmp( "spec_target_updated", type ) == 0) && nPlayerUserID == nEventUserID )
		{
			UpdateGGNextPanel( false );
			ShowAndUpdateSelection( WEPSELECT_SWITCH );
		}

		if ( Q_strcmp( "round_prestart", type ) == 0 ||  ((Q_strcmp( "bot_takeover", type ) == 0) && ( nEventUserID == nPlayerUserID )) )
		{
			// update the inventory
			m_flUpdateInventoryAt = gpGlobals->curtime + 0.1;

			UpdateGGNextPanel( false );

			if (  FlashAPIIsValid() && m_ggNextPanel.handle && m_pScaleformUI->Value_GetType( m_ggNextPanel.handle ) == IScaleformUI::VT_DisplayObject )
			{
				ScaleformDisplayInfo dinfo;
				m_pScaleformUI->Value_GetDisplayInfo( m_ggNextPanel.handle, &dinfo );
				dinfo.SetVisibility( false );
				m_pScaleformUI->Value_SetDisplayInfo( m_ggNextPanel.handle, &dinfo );
			}
		}

		if ( Q_strcmp( "gg_player_impending_upgrade", type ) == 0 && nPlayerUserID == nEventUserID )
		{
			// Let the local player know a level-up is impending
			DisplayLevelUpNextWeapon();
		}
		else if ( Q_strcmp( "ggprogressive_player_levelup", type ) == 0 && nPlayerUserID == nEventUserID )
		{
			if (  Q_strcmp( "weapon_knifegg", event->GetString( "weaponname" ) ) == 0 )
			{
				UpdateGGNextPanel( false, true );
			}
		}

		/*else if ( Q_strcmp( "player_death", type ) == 0 )
		{
			int iPlayerIndexKiller = (engine->GetPlayerForUserID( event->GetInt( "attacker" ) )) + 1;		

			// Let the local player know a level-up is impending
			DisplayLevelUpNextWeapon();
		}*/
	}
	else
	{
		if ( (!Q_strcmp( "buymenu_open", type ) || !Q_strcmp( "buymenu_close", type )) && nPlayerUserID == nEventUserID )
		{
			ShowAndUpdateSelection( WEPSELECT_SWITCH );
			m_flUpdateInventoryAt = gpGlobals->curtime + 0.1;
		}
		else if ( ((Q_strcmp( "bot_takeover", type ) == 0 || Q_strcmp( "spec_target_updated", type ) == 0) && nPlayerUserID == nEventUserID) )
		{
			ShowAndUpdateSelection( WEPSELECT_SWITCH );
			m_flUpdateInventoryAt = gpGlobals->curtime + 0.1;
			m_bUpdateInventoryReset = true;
		}
		else if ( (Q_strcmp( "spec_mode_updated", type ) == 0 && nPlayerUserID == nEventUserID) || Q_strcmp( "hltv_changed_mode", type ) == 0 )
		{
			ShowAndUpdateSelection( WEPSELECT_SWITCH );
			m_flUpdateInventoryAt = gpGlobals->curtime + 0.1;
		}
	}
}

void SFWeaponSelection::DisplayLevelUpNextWeapon()
{
	if ( !m_bCreatedNextPanel )
	{
		CreateGGNextPanel();
	}

	// we need to delay this a frame to allow the next network update to happen
	// if we update it now, the data hasn't been networked down yet
	m_bUpdateGGNextPanel = true;
}

////////////////////////////////////////////////////////////////////////////////
// NOTE: 	This function is not threadsafe. If it is not called from ActionScript,
//			it needs to be called within a lock.
////////////////////////////////////////////////////////////////////////////////
void SFWeaponSelection::RemoveItem( int nSlot, int nPos )
{	
	if ( !m_weaponPanels[nSlot][nPos].handle || m_pScaleformUI->Value_GetType( m_weaponPanels[nSlot][nPos].handle ) != IScaleformUI::VT_DisplayObject )
		return;

	WITH_SLOT_LOCKED
	{
		WITH_SFVALUEARRAY( data, 1 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, m_weaponPanels[nSlot][nPos].handle );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "RemovePanel", data, 1 );
		}
		SafeReleaseSFVALUE( m_weaponPanels[nSlot][nPos].handle );	
	}
}

void SFWeaponSelection::RemoveAllItems( void )
{	
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			//Remove all items before we exit
			for ( int i=0; i < MAX_WEP_SELECT_PANELS; i++ )
			{
				for ( int j=0; j < MAX_WEP_SELECT_POSITIONS; j++ )
				{
					//Remove all items before we exit
					RemoveItem(i,j);
				}
			}
		}

		V_memset( m_weaponPanels, 0, sizeof( m_weaponPanels ) );
		m_bInitialized = false;
	}
}

void SFWeaponSelection::Show( void )
{
	m_bVisible = true;

	if ( m_anchorPanel && m_pScaleformUI && m_pScaleformUI->Value_GetType( m_anchorPanel ) == IScaleformUI::VT_DisplayObject )
	{
		m_pScaleformUI->Value_SetVisible( m_anchorPanel, true );
	}
}

void SFWeaponSelection::Hide( void )
{
	m_bVisible = false;

	if ( m_anchorPanel && m_pScaleformUI && m_pScaleformUI->Value_GetType( m_anchorPanel ) == IScaleformUI::VT_DisplayObject )
	{
		m_pScaleformUI->Value_SetVisible( m_anchorPanel, false );
	}
}

void SFWeaponSelection::ShowPanel( const bool bShow )
{
	if ( m_FlashAPI )
	{
		if ( bShow )
		{
			Show();
		}
		else
		{
			Hide();
		}
	}
}
