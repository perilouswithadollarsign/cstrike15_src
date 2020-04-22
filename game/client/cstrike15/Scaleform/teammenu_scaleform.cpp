//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#if defined( INCLUDE_SCALEFORM )
#include <game/client/iviewport.h>
#include "teammenu_scaleform.h"
#include "inputsystem/iinputsystem.h"
#include "c_cs_playerresource.h"
#include "c_cs_player.h"
#include "c_team.h"
#include "iclientmode.h"
#include "ienginevgui.h"
#include "cs_gamerules.h"
#include "../gameui/cstrike15/cstrike15basepanel.h"
#include "gameui_util.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "engine/IEngineSound.h"
#include "vguicenterprint.h"
#include "matchmaking/imatchframework.h"
#include "cdll_client_int.h"
#include "gametypes.h"
#include "HUD/sfhudradar.h"
#include "vgui/ILocalize.h"
#include "vguitextwindow.h"
#include "vstdlib/vstrtools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( OnOk ),
	SFUI_DECL_METHOD( OnCancel ),
	SFUI_DECL_METHOD( OnSpectate ),
	SFUI_DECL_METHOD( OnAutoSelect ),
	SFUI_DECL_METHOD( OnTimer ),
	SFUI_DECL_METHOD( OnShowScoreboard ),
	SFUI_DECL_METHOD( UpdateNavText ),
	SFUI_DECL_METHOD( OnTeamHighlight ),
	SFUI_DECL_METHOD( IsInitialTeamMenu ),
	SFUI_DECL_METHOD( IsQueuedMatchmaking ),
SFUI_END_GAME_API_DEF( CCSTeamMenuScaleform, TeamMenu );

extern ConVar sv_disable_show_team_select_menu;
extern ConVar mp_force_assign_teams;

CCSTeamMenuScaleform::CCSTeamMenuScaleform( CounterStrikeViewport* pViewPort ) :
	m_bVisible( false ),
	m_bLoading( false ),
	m_bAllowSpectate( false ),
	m_pViewPort( pViewPort ),
	m_pCTCountHuman( NULL ),
	m_pCTCountBot( NULL ),
	m_pTCountHuman( NULL ),
	m_pTCountBot( NULL ),
	m_pCTHelpText( NULL ),
	m_pTHelpText( NULL ),
	m_pNavText( NULL ),
	m_pTName( NULL ),
	m_pCTName ( NULL ),
	m_pTimerTextGreen( NULL ),
	m_pTimerTextRed( NULL ),
	m_pTimerTextLabel( NULL ),
	m_OnClosedAction( NOTHING ),
	m_nCTHumanCount( 0 ),
	m_nTHumanCount( 0 ),
	m_bPostSelectOverlay( false ),
	m_nForceSelectTimeLast( -1.0f ),
	m_bGreenTimerVisible( false ),
	m_bRedTimerVisible( false ),
	m_bMatchStart( true ),
	m_pTimerHandle( NULL ),
	m_bSelectingTeam( false )
{
	m_iSplitScreenSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	StartAlwaysListenEvents();
}

CCSTeamMenuScaleform::~CCSTeamMenuScaleform()
{
}

void CCSTeamMenuScaleform::StartAlwaysListenEvents( void )
{
	ListenForGameEvent( "cs_match_end_restart" );
	ListenForGameEvent( "cs_game_disconnected" );
	ListenForGameEvent( "game_newmap" );
	ListenForGameEvent( "jointeam_failed" );
	ListenForGameEvent( "player_spawned" );
}


void CCSTeamMenuScaleform::StartListeningForEvents( void )
{
	StartAlwaysListenEvents();

	ListenForGameEvent( "round_start" );
	ListenForGameEvent( "teamchange_pending" );
}

void CCSTeamMenuScaleform::StopListeningForEvents( void )
{
	StopListeningForAllEvents();

	StartAlwaysListenEvents();
}


void CCSTeamMenuScaleform::FlashLoaded( void )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	SFVALUE panelValue = m_pScaleformUI->Value_GetMember( m_FlashAPI, "Panel" );

	if ( panelValue )
	{
		SFVALUE navPanelValue = m_pScaleformUI->Value_GetMember( panelValue, "NavPanel" );
		if ( navPanelValue )
		{
			m_pCTCountHuman = m_pScaleformUI->TextObject_MakeTextObjectFromMember( navPanelValue, "CT_CountHuman" );
			m_pTCountHuman = m_pScaleformUI->TextObject_MakeTextObjectFromMember( navPanelValue, "T_CountHuman" );
			m_pCTCountBot = m_pScaleformUI->TextObject_MakeTextObjectFromMember( navPanelValue, "CT_CountBot" );
			m_pTCountBot = m_pScaleformUI->TextObject_MakeTextObjectFromMember( navPanelValue, "T_CountBot" );		

			m_pCTHelpText = m_pScaleformUI->TextObject_MakeTextObjectFromMember( navPanelValue, "CtHelpText" );
			m_pTHelpText = m_pScaleformUI->TextObject_MakeTextObjectFromMember( navPanelValue, "THelpText" );


			m_pTimerHandle = m_pScaleformUI->Value_GetMember( navPanelValue, "Timer" );
			if ( m_pTimerHandle )
			{
				m_pTimerTextGreen = m_pScaleformUI->TextObject_MakeTextObjectFromMember( m_pTimerHandle, "TimerTextGreen" );
				m_pTimerTextRed = m_pScaleformUI->TextObject_MakeTextObjectFromMember( m_pTimerHandle, "TimerTextRed" );
				m_pTimerTextLabel = m_pScaleformUI->TextObject_MakeTextObjectFromMember( m_pTimerHandle, "TimerTextLabel" );

				if ( m_pTimerTextGreen && m_pTimerTextRed && m_pTimerTextLabel )
				{
					m_pTimerTextLabel->SetVisible( false );
					m_pTimerTextGreen->SetVisible( false );
					m_pTimerTextRed->SetVisible( false );

					m_bGreenTimerVisible = false;
					m_bRedTimerVisible = false;
				}
			}

			SFVALUE CT_HeaderPanel  = m_pScaleformUI->Value_GetMember( navPanelValue, "CT_Header" );
			if ( CT_HeaderPanel )
			{
				m_pCTName = m_pScaleformUI->TextObject_MakeTextObjectFromMember( CT_HeaderPanel, "CT_TeamName" );
				if ( m_pCTName )
					m_pCTName->SetText( "#SFUI_CT_Label" );

				m_pScaleformUI->ReleaseValue( CT_HeaderPanel );
			}

			SFVALUE T_HeaderPanel  = m_pScaleformUI->Value_GetMember( navPanelValue, "T_Header" );
			if ( T_HeaderPanel )
			{
				m_pTName = m_pScaleformUI->TextObject_MakeTextObjectFromMember( T_HeaderPanel, "T_TeamName" );
				if ( m_pTName )
					m_pTName->SetText( "#SFUI_T_Label" );

				m_pScaleformUI->ReleaseValue( T_HeaderPanel );
			}

			SFVALUE navBar = m_pScaleformUI->Value_GetMember( navPanelValue, "NavigationMaster" );
			if ( navBar )
			{
				m_pNavText = m_pScaleformUI->TextObject_MakeTextObjectFromMember( navBar, "ControllerNav" );
				m_pScaleformUI->ReleaseValue( navBar );
			}

			// initialize the dynamic text in the dialog
			OnTimer( NULL, NULL );

			m_pScaleformUI->ReleaseValue( navPanelValue );
		}

		m_pScaleformUI->ReleaseValue( panelValue );
	}

	StartListeningForEvents();
}

void CCSTeamMenuScaleform::FlashReady( void )
{
	m_bLoading = false;

	// HACK: Ideally we'd prevent this menu from loading at all in modes where
	// we don't want it, but as a failsafe, remove this menu in QMM to prevent it
	// eating mouse menu inputs.
	if ( ( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() ) || sv_disable_show_team_select_menu.GetBool() )
	{
		WITH_SLOT_LOCKED
		{
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanelAndRemove", 0, NULL );
		}
		return;
	}

	// Reset the lists of player XUIDs we pushed to Scaleform
	V_memset( m_T_Xuids, INVALID_XUID, MAX_TEAM_SIZE*sizeof(XUID) );
	V_memset( m_CT_Xuids, INVALID_XUID, MAX_TEAM_SIZE*sizeof(XUID) );
	V_memset( m_nCTLocalPlayers, -1, MAX_TEAM_SIZE*sizeof(int) );
	V_memset( m_nTLocalPlayers, -1, MAX_TEAM_SIZE*sizeof(int) );
	V_memset( m_chCTNames, 0, sizeof( m_chCTNames ) );
	V_memset( m_chTNames, 0, sizeof( m_chTNames ) );

	if ( m_bVisible )
	{
		Show();
	}
	else
	{
		Hide();
	}
}

void CCSTeamMenuScaleform::Show( void )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	g_pInputSystem->SetSteamControllerMode( "MenuControls", this );

	CTextWindow *panel = (CTextWindow *)GetViewPortInterface()->FindPanelByName( PANEL_INFO );
	//panel->ShowPanel(false);
	if ( panel && panel->IsVisible() && ( panel->GetAlpha() > 0 ) && !( CSGameRules() && CSGameRules()->IsQueuedMatchmaking() ) )
		return;

	if ( !m_bLoading )
	{
		if ( FlashAPIIsValid() )
		{
			m_bSelectingTeam = true;

			////////////////
			//const char *levelName = engine->GetLevelName();
			//const char* mapFileName = V_GetFileName( levelName );

			char szCurLevel[MAX_PATH];
			V_strcpy_safe( szCurLevel, V_GetFileName( engine->GetLevelName() ) );
			V_FixSlashes( szCurLevel, '/' ); // use consistent slashes.
			V_StripExtension( szCurLevel, szCurLevel, sizeof( szCurLevel ) );

			//////////////////////////////////////////////////////////////////////////
			// Get the map path so we can load the thumbnail
			//////////////////////////////////////////////////////////////////////////
			char szPath[MAX_PATH];
			char szMapID[MAX_PATH];
			char szJPG[MAX_PATH];
			szJPG[0] = '\0';
			bool bHasJpg = false;

			V_strcpy_safe( szPath, szCurLevel );
			V_FixSlashes( szPath, '/' ); // internal path strings use forward slashes, make sure we compare like that.

			if ( V_strstr( szPath, "workshop/" ) )
			{
				V_snprintf( szMapID, MAX_PATH, "%llu", GetMapIDFromMapPath( szPath ) );
				V_StripFilename( szPath );

				V_snprintf( szJPG, MAX_PATH, "maps/%s/thumb%s.jpg", szPath, szMapID );
				if ( !g_pFullFileSystem->FileExists( szJPG ) )
				{
					V_snprintf( szJPG, MAX_PATH, "maps/%s/%s.jpg", szPath, szCurLevel );
					if ( !g_pFullFileSystem->FileExists( szJPG ) )
					{
						// last chance.  try to see if we made one locally
						V_snprintf( szJPG, MAX_PATH, "maps/%s.jpg", szCurLevel );
						if ( g_pFullFileSystem->FileExists( szJPG ) )
						{
							bHasJpg = true;
						}
					}
				}
			}
			else
			{
				V_snprintf( szJPG, MAX_PATH, "maps/%s.jpg", szCurLevel );
				if ( g_pFullFileSystem->FileExists( szJPG ) )
				{
					bHasJpg = true;
				}
			}

			if ( bHasJpg )
			{
				WITH_SFVALUEARRAY_SLOT_LOCKED( args, 1 )
				{
					g_pScaleformUI->ValueArray_SetElement( args, 0, bHasJpg ? szJPG : "" );
					g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetBackgroundJpg", args, 1 );				
				}			
			}

			///////////////////

			SetTeamNames();

			C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

			// Determine if we came into this map with a pre-assigned team (ie. team matchmaking) - if so, join that team immediately and 
			//	display the team assignment now
			int preassignTeam = -1;
			if ( 0 && pLocalPlayer && pLocalPlayer->GetTeamNumber() == TEAM_UNASSIGNED )
			{
				IMatchSession *pMatchSession = g_pMatchFramework->GetMatchSession();
				KeyValues *pSettings = pMatchSession ? pMatchSession->GetSessionSettings() : NULL;

				if ( pSettings )
				{
					// Either one of these will be set, depending on which team we were assigned to
					int team = pSettings->GetInt( "server/team", -1);
					// Clear value so we don't use it again later
					if ( team != -1 )
						pSettings->SetInt( "server/team", -1);

					if ( team == -1 )
					{
						team = pSettings->GetInt( "conteam", -1);
						if ( team != -1 )
							pSettings->SetInt( "conteam", -1);
					}

					if (team != -1)
					{
						preassignTeam = (team == 1) ? TEAM_CT: TEAM_TERRORIST;
					}
				}
			}

			if ( mp_force_assign_teams.GetBool() )
			{
				preassignTeam = 0;
			}

			WITH_SFVALUEARRAY_SLOT_LOCKED( args, 2 )
			{
				// If the player's team is unassigned it means they are loading into the map for the first time. In this case show
				// the team selection screen instantly so that the player is not given a brief glimpse of the map
				m_pScaleformUI->ValueArray_SetElement( args, 0, ( pLocalPlayer && pLocalPlayer->GetTeamNumber() == TEAM_UNASSIGNED )  );

				// Should we display a preassigned team immediately?
				m_pScaleformUI->ValueArray_SetElement( args, 1, preassignTeam );

				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", args, 2 );
			}

			RefreshCounts();
			UpdateSpectatorOption();
			UpdateHelpText();
			UpdateTeamAvatars();

			GetHud().DisableHud();
			m_pViewPort->ShowBackGround( true ); 
			panel->SetMouseInputEnabled( false );

			if ( m_pTimerHandle )
			{
				m_pScaleformUI->Value_SetVisible( m_pTimerHandle, m_bMatchStart );
			}

			// Start the force team select timer
			if ( m_bMatchStart )
			{
#if defined( _X360 )
				( ( CCStrike15BasePanel* )BasePanel() )->Xbox_PromptSwitchToGameVoiceChannel();
#endif

				HandleForceSelect();
			}

			// Scaleform initialization is done - Now perform the join to our preassigned team
			if ( preassignTeam != -1 )
			{
				JoinTeam( preassignTeam );
				DevMsg( "Auto-assign player to side %d\n", preassignTeam );
			}
		}
		else
		{
			m_bLoading = true;

			SFUI_REQUEST_ELEMENT( SF_SS_SLOT( m_iSplitScreenSlot ), g_pScaleformUI, CCSTeamMenuScaleform, this, TeamMenu );
		}
	}

	if( !m_bVisible )
	{
		CLocalPlayerFilter filter;
		PlayMusicSelection( filter, CSMUSIC_SELECTION );
	}

	m_bVisible = true;
}

void CCSTeamMenuScaleform::Hide( bool bRemove )
{
	g_pInputSystem->SetSteamControllerMode( NULL, this );

	if ( !m_bLoading && FlashAPIIsValid() && m_bVisible )
	{
		if ( bRemove )
		{
			WITH_SLOT_LOCKED
			{
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanelAndRemove", 0, NULL );
			}
		}
		else
		{
			WITH_SLOT_LOCKED
			{
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", 0, NULL );
			}
		}

		SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

		C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

		if ( pPlayer )
		{
			CLocalPlayerFilter filter;
			C_BaseEntity::EmitSound(filter, SOUND_FROM_WORLD, "Music.StopSelection" );

			if ( pPlayer->GetTeamNumber() != TEAM_UNASSIGNED )
				m_bSelectingTeam = false;
		}

		m_pViewPort->ShowBackGround( false );
		GetHud().EnableHud();

		SFHudRadar *pPanel = GET_HUDELEMENT( SFHudRadar );
		if ( pPanel )
			pPanel->ResizeHud();

		m_bVisible = false;
	}


}

bool CCSTeamMenuScaleform::PreUnloadFlash( void )
{
	SafeReleaseSFVALUE( m_pTimerHandle );

	SafeReleaseSFTextObject( m_pCTCountHuman );
	SafeReleaseSFTextObject( m_pTCountHuman );
	SafeReleaseSFTextObject( m_pCTCountBot );
	SafeReleaseSFTextObject( m_pTCountBot );

	SafeReleaseSFTextObject( m_pCTHelpText );
	SafeReleaseSFTextObject( m_pTHelpText );

	SafeReleaseSFTextObject( m_pNavText );
	SafeReleaseSFTextObject( m_pTName );
	SafeReleaseSFTextObject( m_pCTName );
	SafeReleaseSFTextObject( m_pTimerTextGreen );
	SafeReleaseSFTextObject( m_pTimerTextRed );
	SafeReleaseSFTextObject( m_pTimerTextLabel );

	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
	StopListeningForEvents();
	m_bLoading = false;
	
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( pPlayer &&
	   ( pPlayer->GetObserverMode() != OBS_MODE_FIXED && pPlayer->GetObserverMode() != OBS_MODE_ROAMING && pPlayer->GetObserverMode() != OBS_MODE_NONE ) )
	{
		GetViewPortInterface()->ShowPanel( PANEL_SPECGUI, true );
	}
	return ScaleformFlashInterface::PreUnloadFlash();
}

void CCSTeamMenuScaleform::ShowPanel( bool bShow )
{
	if ( !IsValidSplitScreenSlot( m_iSplitScreenSlot ) )
		return;

	if ( bShow != m_bVisible )
	{
		if ( bShow )
		{
			Show();
		}
		else
		{
			Hide( true );
		}
	}

}

void CCSTeamMenuScaleform::RefreshCounts( void )
{
	// Gather human / bot totals
	m_nCTHumanCount = 0;
	m_nTHumanCount = 0;
	int nCTBotCount = 0;
	int nTBotCount = 0;

	for ( int nPlayerIndex = 1; nPlayerIndex <= MAX_PLAYERS; nPlayerIndex++ )
	{
		if ( g_PR && g_PR->IsConnected( nPlayerIndex ) )
		{
			int nTeamID = g_PR->GetPendingTeam( nPlayerIndex );

			if ( nTeamID == TEAM_CT )
			{
				if ( g_PR->IsFakePlayer( nPlayerIndex ) )
				{
					++nCTBotCount;
				}
				else
				{
					++m_nCTHumanCount;
				}
			}
			else if ( nTeamID == TEAM_TERRORIST )
			{
				if ( g_PR->IsFakePlayer( nPlayerIndex ) )
				{
					++nTBotCount;
				}
				else
				{
					++m_nTHumanCount;
				}
			}
		}
	}

	WITH_SLOT_LOCKED
	{

		if ( m_pCTCountHuman )
		{
			m_pCTCountHuman->SetText( m_nCTHumanCount );
		}

		if ( m_pCTCountBot )
		{
			m_pCTCountBot->SetText( nCTBotCount );
		}

		if ( m_pTCountHuman )
		{
			m_pTCountHuman->SetText( m_nTHumanCount );
		}

		if ( m_pTCountBot )
		{
			m_pTCountBot->SetText( nTBotCount );
		}

		int nMaxPlayers = MIN( g_pGameTypes->GetCurrentServerNumSlots( ), 24 );
		int nMaxTeamSlots = ceil( (double)(nMaxPlayers/2) );

		// Display either player counts or team full notice
		WITH_SFVALUEARRAY( args, 2 )
		{
			m_pScaleformUI->ValueArray_SetElement( args, 0, ( m_nCTHumanCount >= nMaxTeamSlots )  );
			m_pScaleformUI->ValueArray_SetElement( args, 1, ( m_nTHumanCount >= nMaxTeamSlots )  );
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setTeamsFull", args, 2 );
		}
	}
}

void CCSTeamMenuScaleform::UpdateSpectatorOption( void )
{
	if ( m_pNavText )
	{
		const ConVar *allowSpectators = cvar->FindVar( "mp_allowspectators" );
		C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( !pPlayer || !CSGameRules() )
		{
			return;
		}

		// Ensure there is at least one player ( not the local player ) that is alive and can be spectated
		bool bAtLeastOnePlayerAlive = false;
		for ( int playerIndex = 1; playerIndex <= MAX_PLAYERS; playerIndex++ )
		{
			if ( g_PR->IsAlive( playerIndex ) && ( g_PR->IsConnected( playerIndex ) ) && ( !g_PR->IsFakePlayer( playerIndex ) ) )
			{
				bAtLeastOnePlayerAlive = true;
				break;
			}
		}

		m_bAllowSpectate = false;

		if ( allowSpectators &&
			 allowSpectators->GetBool() &&
			 //bAtLeastOnePlayerAlive &&	// dwenger - remove requirement that an active player must be present to allow spectating
			 (  GetGlobalTeam( TEAM_SPECTATOR )->GetNumPlayers() < CSGameRules()->GetMaxSpectatorSlots() ) )
		{
			m_bAllowSpectate = true;
		}

		bool isInitialTeamMenu = m_pViewPort && !m_pViewPort->GetChoseTeamAndClass();

		const char* navStringID = "#SFUI_InitialTeamNavNoSpectate@15"; //Default to the minimum message

		// There are 4 possible nav texts, based on whether you can spectator and whether this is the first time we choose a team for a match.
		if ( isInitialTeamMenu )
		{
			if ( m_bAllowSpectate )
			{
				navStringID = "#SFUI_InitialTeamNavWithSpectate@15";
			}
			else
			{
				navStringID = "#SFUI_InitialTeamNavNoSpectate@15";
			}
		}
		else
		{
			if ( m_bAllowSpectate )
			{
				navStringID = "#SFUI_TeamNavWithSpectate@15";
			}
			else
			{
				navStringID = "#SFUI_TeamNavNoSpectate@15";
			}
		}

		const wchar_t* pTranslated = g_pScaleformUI->Translate( navStringID, NULL );
		if ( FlashAPIIsValid() )
		{

			WITH_SLOT_LOCKED
			{
				m_pNavText->SetTextHTML( pTranslated );

				WITH_SFVALUEARRAY( args, 1 )
				{		
					m_pScaleformUI->ValueArray_SetElement( args, 0, m_bAllowSpectate );
					g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowSpectatorButton", args, 1 );
				}
			}
		}
	}
}


void CCSTeamMenuScaleform::SetTeamNames( void )
{
	if ( !CSGameRules() )
		return;

	WITH_SLOT_LOCKED
	{
		C_Team *pTeamT = GetGlobalTeam( TEAM_TERRORIST );
		if ( pTeamT && m_pTName )
		{
			if ( StringIsEmpty( pTeamT->Get_ClanName() ) )
				m_pTName->SetText( "#SFUI_T_Label" );
			else
			{
				wchar_t wszNameT[ MAX_TEAM_NAME_LENGTH ];
				g_pVGuiLocalize->ConvertANSIToUnicode( pTeamT->Get_ClanName(), wszNameT, sizeof( wszNameT ) );
				wchar_t wszSafeNameT[ MAX_TEAM_NAME_LENGTH ];
				wszSafeNameT[0] = L'\0';	
				g_pScaleformUI->MakeStringSafe( wszNameT, wszSafeNameT, sizeof( wszNameT ) );	
				m_pTName->SetTextHTML( wszSafeNameT );
			}
		}

		C_Team *pTeamCT = GetGlobalTeam( TEAM_CT );
		if ( pTeamCT && m_pCTName )
		{
			if ( StringIsEmpty( pTeamCT->Get_ClanName() ) )
				m_pCTName->SetText( "#SFUI_CT_Label" );
			else
			{
				wchar_t wszNameCT[ MAX_TEAM_NAME_LENGTH ];
				g_pVGuiLocalize->ConvertANSIToUnicode( pTeamCT->Get_ClanName(), wszNameCT, sizeof( wszNameCT ) );
				wchar_t wszSafeNameCT[ MAX_TEAM_NAME_LENGTH ];
				wszSafeNameCT[0] = L'\0';	
				g_pScaleformUI->MakeStringSafe( wszNameCT, wszSafeNameCT, sizeof( wszNameCT ) );	
				m_pCTName->SetTextHTML( wszSafeNameCT );
			}
		}
	}
}



void CCSTeamMenuScaleform::UpdateHelpText( void )
{
	if ( !CSGameRules() )
		return;

	const wchar_t *wszRules_CT = L"";	
	const wchar_t *wszRules_T = L"";	

	int nObjective = 0;
	// 0 = generic
	// 1 = plant bomb
	// 2 = rescue hostages
	// 3 = GG

	if ( CSGameRules()->IsPlayingGunGameTRBomb() ) 
	{
		wszRules_CT = g_pVGuiLocalize->FindSafe( "#SFUI_Rules_TS_TRBomb_CT" );
		wszRules_T = g_pVGuiLocalize->FindSafe( "#SFUI_Rules_TS_TRBomb_T" );
		nObjective = 1; // bomb
	}
	else if ( CSGameRules()->IsPlayingGunGameProgressive() )
	{		
		wszRules_CT = g_pVGuiLocalize->FindSafe( "#SFUI_Rules_TS_GunGame_Progressive" );
		wszRules_T = g_pVGuiLocalize->FindSafe( "#SFUI_Rules_TS_GunGame_Progressive" );
		nObjective = 3; // gg
	}
	else
	{
		if ( CSGameRules()->IsHostageRescueMap() )
		{
			wszRules_CT = g_pVGuiLocalize->FindSafe( "#SFUI_Rules_TS_Hostage_CT" );
			wszRules_T = g_pVGuiLocalize->FindSafe( "#SFUI_Rules_TS_Hostage_T" );
			nObjective = 2; // hostage
		}
		else if ( CSGameRules()->IsBombDefuseMap() )
		{
			wszRules_CT = g_pVGuiLocalize->FindSafe( "#SFUI_Rules_TS_Bomb_CT" );
			wszRules_T = g_pVGuiLocalize->FindSafe( "#SFUI_Rules_TS_Bomb_T" );	
			nObjective = 1; // bomb
		}	
		else
		{
			wszRules_CT = g_pVGuiLocalize->FindSafe( "#SFUI_Rules_TS_Generic_CT" );
			wszRules_T = g_pVGuiLocalize->FindSafe( "#SFUI_Rules_TS_Generic_T" );	
		}
	}

	WITH_SFVALUEARRAY_SLOT_LOCKED( args, 3 )
	{
		g_pScaleformUI->ValueArray_SetElement( args, 0, wszRules_CT );
		g_pScaleformUI->ValueArray_SetElement( args, 1, wszRules_T );
		g_pScaleformUI->ValueArray_SetElement( args, 2, nObjective );
		g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setTeamHintText", args, 3 );
	}
}

void CCSTeamMenuScaleform::OnTeamHighlight( SCALEFORM_CALLBACK_ARGS_DECL )
{
	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "VGUI_button.rollover" );

	int nTeamID = ( int ) g_pScaleformUI->Params_GetArgAsNumber( obj );
	NOTE_UNUSED( nTeamID );
}

void CCSTeamMenuScaleform::UpdateNavText( SCALEFORM_CALLBACK_ARGS_DECL )
{
	UpdateSpectatorOption();
}

void CCSTeamMenuScaleform::OnTimer( SCALEFORM_CALLBACK_ARGS_DECL )
{
	if ( FlashAPIIsValid() )
	{
		SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );
		
		RefreshCounts();
		UpdateSpectatorOption();
		UpdateHelpText();
	}
}

void CCSTeamMenuScaleform::OnOk( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	int side = ( int ) g_pScaleformUI->Params_GetArgAsNumber( obj );

	int nCurrentTeamID = -1;

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( pLocalPlayer )
	{
		nCurrentTeamID = pLocalPlayer->GetPendingTeamNumber();

		if ( ( ( side == TEAM_CT ) && ( nCurrentTeamID == TEAM_CT ) ) ||
			 ( ( side == TEAM_TERRORIST ) && ( nCurrentTeamID == TEAM_TERRORIST ) ) )
		{
			// Player is trying to rejoin the same team. Hide the choose team menu.
			Hide( true );
			return;
		}
	}

	
	JoinTeam( side );

	// the team menu gets hidden when the message that the team is joined appears
}

void CCSTeamMenuScaleform::JoinTeam( int side )
{
	switch ( side )
	{
	case 0:
		engine->ClientCmd( "jointeam 0 1" );
		break;
	case TEAM_CT:
		if ( m_bMatchStart )
		{
			engine->ClientCmd( "jointeam 3 1" );
		}
		else
		{
			engine->ClientCmd( "jointeam 3 0" );
		}
		break;

	case TEAM_TERRORIST:
		if ( m_bMatchStart )
		{
			engine->ClientCmd( "jointeam 2 1" );
		}
		else
		{
			engine->ClientCmd( "jointeam 2 0" );
		}

	default:
		break;
	}

	if ( side > 0 )
	{
		// hide this after we join a team
		Hide( true );
	}
}

void CCSTeamMenuScaleform::OnCancel( SCALEFORM_CALLBACK_ARGS_DECL )
{
	Hide( true );
}

void CCSTeamMenuScaleform::OnSpectate( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	if ( m_bAllowSpectate )
	{
		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

		if ( pLocalPlayer )
		{
			if ( pLocalPlayer->GetPendingTeamNumber() == TEAM_SPECTATOR )
			{
				// Player is trying to rejoin the same team. Hide the choose team menu.
				Hide( true );
			}
			else
			{
				if ( m_bMatchStart )
				{
					engine->ClientCmd( "jointeam 1 1" );
				}
				else
				{
					engine->ClientCmd( "jointeam 1 0" );
				}

				m_bSelectingTeam = false;
			}
		}
	}
}

void CCSTeamMenuScaleform::OnAutoSelect( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	if ( m_bMatchStart )
	{
		engine->ClientCmd( "jointeam 0 1" );	
	}
	else
	{
		engine->ClientCmd( "jointeam 0 0" );
	}

	m_bSelectingTeam = false;
}

//-----------------------------------------------------------------------------
// Purpose: respond to game events
//-----------------------------------------------------------------------------
void CCSTeamMenuScaleform::FireGameEvent( IGameEvent *event )
{
	if ( !IsValidSplitScreenSlot( m_iSplitScreenSlot ) )
		return;

	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	const char *type = event->GetName();
	bool bHideScreen = false;

	if ( !V_strcmp( type, "jointeam_failed" ) )
	{
		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pLocalPlayer && pLocalPlayer->GetUserID() == event->GetInt( "userid" ) )
		{
			int reason = event->GetInt( "reason" );

			const char* msg = NULL;

			switch( reason )
			{
				case TeamJoinFailedReason::CHANGED_TOO_OFTEN:			msg = "#Cstrike_TitlesTXT_Only_1_Team_Change"; break;
				case TeamJoinFailedReason::BOTH_TEAMS_FULL:				msg = "#Cstrike_TitlesTXT_All_Teams_Full"; break;
				case TeamJoinFailedReason::TERRORISTS_FULL:				msg = "#Cstrike_TitlesTXT_Terrorists_Full"; break;
				case TeamJoinFailedReason::CTS_FULL:					msg = "#Cstrike_TitlesTXT_CTs_Full"; break;
				case TeamJoinFailedReason::CANT_JOIN_SPECTATOR:			msg = "#Cstrike_TitlesTXT_Cannot_Be_Spectator"; break;
				case TeamJoinFailedReason::HUMANS_CAN_ONLY_JOIN_TS:		msg = "#Cstrike_TitlesTXT_Humans_Join_Team_T"; break;
				case TeamJoinFailedReason::HUMANS_CAN_ONLY_JOIN_CTS:	msg = "#Cstrike_TitlesTXT_Humans_Join_Team_CT"; break;
				case TeamJoinFailedReason::TOO_MANY_TS:					msg = "#Cstrike_TitlesTXT_Too_Many_Terrorists"; break;
				case TeamJoinFailedReason::TOO_MANY_CTS:				msg = "#Cstrike_TitlesTXT_Too_Many_CTs"; break;

				default: msg = NULL; break;
			}

			if ( msg != NULL )
			{
				CLocalPlayerFilter filter;
				pLocalPlayer->EmitSound(filter, SOUND_FROM_WORLD, "buymenu_cant_buy.click" );

				if ( !m_bVisible || !FlashAPIIsValid())
				{
					GetCenterPrint()->Print( const_cast<char *>( msg ) );
				}
				else
				{
					WITH_SFVALUEARRAY(args, 1)
					{
						m_pScaleformUI->ValueArray_SetElement( args, 0, msg  );
						WITH_SLOT_LOCKED
						{
							g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showErrorText", args, 1 );
						}
					}
				}
			}

		}
	}
	else if( !V_strcmp( type, "cs_match_end_restart" ) )
	{
		if ( !engine->IsPlayingDemo() )
		{
			m_bMatchStart = true;
			if ( CSGameRules() && !CSGameRules()->IsPlayingTraining() && !CSGameRules()->IsQueuedMatchmaking() )
				GetViewPortInterface()->ShowPanel( PANEL_TEAM , true );
		}
	}
	else if ( !V_strcmp( type, "player_spawned" ) )
	{
		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
		if ( pLocalPlayer && pLocalPlayer->GetUserID() == event->GetInt( "userid" ) )
		{
			m_bMatchStart = false;
			ResetForceSelect();

			if ( event->GetBool( "inrestart" ) )
			{
				// Waiting for a map restart. Show the post team select overlay
				// until the round starts.
				HandlePostTeamSelect( pLocalPlayer->GetPendingTeamNumber() );
			}
			else
			{
				bHideScreen = true;

			}
		}
	}
	else if ( !V_strcmp( type, "round_start" ) )
	{
		SetTeamNames();

		if ( m_bPostSelectOverlay )
		{
			m_bPostSelectOverlay = false;
			bHideScreen = true;
		}
	}
	else if ( !V_strcmp( type, "cs_game_disconnected" ) || !V_strcmp( type, "game_newmap" ) )
	{
		m_bMatchStart = true;
		bHideScreen = true;

		if ( !V_strcmp( type, "game_newmap" ) )
		{
			if ( !FlashAPIIsValid() && !engine->IsHLTV() )
			{
				m_bLoading = true;
				SFUI_REQUEST_ELEMENT( SF_SS_SLOT( m_iSplitScreenSlot ), g_pScaleformUI, CCSTeamMenuScaleform, this, TeamMenu );
			}
		}

	}
	else if ( !V_strcmp( type, "teamchange_pending" ) )
	{
		bHideScreen = true;
		m_bSelectingTeam = false;
	}

	if ( bHideScreen )
	{
		if ( m_bVisible ) 
		{
			Hide( true );
		}
		else
		{
			RemoveFlashElement();
		}
	}

}

void CCSTeamMenuScaleform::OnShowScoreboard( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	if ( GetViewPortInterface() )
	{
		GetViewPortInterface()->ShowPanel( PANEL_SCOREBOARD, true );
	}
}

void CCSTeamMenuScaleform::HandlePostTeamSelect( int team )
{
	CLocalPlayerFilter filter;
	C_BaseEntity::EmitSound( filter, SOUND_FROM_WORLD, "VGUI_button.click" );

	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			WITH_SFVALUEARRAY( args, 1 )
			{
				g_pScaleformUI->ValueArray_SetElement( args, 0, team );
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showPreMatchOverlay", args, 1 );
			}
		}

		m_bPostSelectOverlay = true;
	}
}

void CCSTeamMenuScaleform::HandleForceSelect( void )
{
	if ( !m_bMatchStart )
		return;

	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iSplitScreenSlot );

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( pLocalPlayer && pLocalPlayer->GetTeamNumber() == TEAM_UNASSIGNED && pLocalPlayer->GetForceTeamTime() != -1.0f )
	{

		int nDelta = static_cast<int>( ceil( pLocalPlayer->GetForceTeamTime() - gpGlobals->curtime ) );

		if ( m_nForceSelectTimeLast == -1.0f )
		{
			m_nForceSelectTimeLast = nDelta + 1;
		}

		if ( nDelta < m_nForceSelectTimeLast )
		{
			// Update on screen timer
			int nMinutes = nDelta / 60;
			int nSeconds = nDelta % 60;

			wchar_t szTime[32];
			szTime[0] = 0;

			V_snwprintf( szTime, ARRAYSIZE( szTime ), L"%d:%.2d", nMinutes, nSeconds );
			
			if( nDelta <= 3 && m_nForceSelectTimeLast != nDelta )
			{
				if( nDelta > 0 )
				{
					CLocalPlayerFilter filter;
					CBaseEntity::EmitSound( filter, 0, "UI.CounterBeep" );
				}
				else
				{
					CLocalPlayerFilter filter;
					CBaseEntity::EmitSound( filter, 0, "UI.CounterDoneBeep" );

					Hide( true );
				}
			}


			m_nForceSelectTimeLast = nDelta;

			if ( FlashAPIIsValid() && m_pTimerTextGreen && m_pTimerTextRed && m_pTimerTextLabel )
			{				
				// Change timer to red when only three seconds remain
				if ( nDelta <= 3 && !m_bRedTimerVisible )
				{
					m_pTimerTextLabel->SetVisible( true );
					m_pTimerTextGreen->SetVisible( false );
					m_pTimerTextRed->SetVisible( true );
					m_bRedTimerVisible = true;
					m_bGreenTimerVisible = false;
				}
				else if ( nDelta > 3 && !m_bGreenTimerVisible)
				{
					m_pTimerTextLabel->SetVisible( true );
					m_pTimerTextGreen->SetVisible( true );
					m_pTimerTextRed->SetVisible( false );
					m_bGreenTimerVisible = true;
				}
				else if ( nDelta < 0 )
				{
					// Ran out of time. Force team selection.
					m_pTimerTextLabel->SetVisible( false );
					m_pTimerTextGreen->SetVisible( false );
					m_pTimerTextRed->SetVisible( false );
				}

				WITH_SLOT_LOCKED
				{
					if ( m_bGreenTimerVisible )
					{
						m_pTimerTextGreen->SetText( szTime );
					}
					else
					{
						m_pTimerTextRed->SetText( szTime );
					}
				}
			}		
		}
	}
}

void CCSTeamMenuScaleform::UpdateTeamAvatars( void )
{
	//int nMaxPlayers = MIN( g_pGameTypes->GetCurrentServerNumSlots( ), 24 );
	//int nMaxTeamSlots = ceil( (double)(nMaxPlayers/2) );

	XUID ctXuids[TEAM_MENU_MAX_PLAYERS];
	int numCtHumans = 0;
	XUID tXuids[TEAM_MENU_MAX_PLAYERS];
	int numTHumans = 0;

	const char *szCTNames[TEAM_MENU_MAX_PLAYERS];
	const char *szTNames[TEAM_MENU_MAX_PLAYERS];	

	bool ctLocalPlayer[TEAM_MENU_MAX_PLAYERS];
	bool tLocalPlayer[TEAM_MENU_MAX_PLAYERS];

	V_memset( ctXuids, 0, sizeof(ctXuids) );
	V_memset( tXuids, 0, sizeof(tXuids) );
	V_memset( ctLocalPlayer, 0, sizeof(ctLocalPlayer) );
	V_memset( tLocalPlayer, 0, sizeof(tLocalPlayer) );
	for ( int j = 0; j < TEAM_MENU_MAX_PLAYERS; ++ j )
	{
		szCTNames[j] = "";
		szTNames[j] = "";
	}

	if ( g_PR != NULL )
	{
		for ( int nPlayerIndex = 1; nPlayerIndex <= MAX_PLAYERS; nPlayerIndex++ )
		{
			if ( g_PR->IsConnected( nPlayerIndex ) && !g_PR->IsFakePlayer( nPlayerIndex ) )
			{
				XUID PlayerXuid = g_PR->GetXuid( nPlayerIndex );
				int PlayerPendingTeam = g_PR->GetPendingTeam( nPlayerIndex );
				const char *szName = g_PR->GetPlayerName( nPlayerIndex );

				bool bLocalPlayer = ( nPlayerIndex == GetLocalPlayerIndex() );

				if ( PlayerPendingTeam == TEAM_TERRORIST && numTHumans < ARRAYSIZE(szTNames) )
				{
					szTNames[ numTHumans ] = szName;
					tLocalPlayer[ numTHumans ] = bLocalPlayer;
					tXuids[ numTHumans++ ] = PlayerXuid;
				}
				else if ( PlayerPendingTeam == TEAM_CT && numCtHumans < ARRAYSIZE(szCTNames) )
				{
					szCTNames[ numCtHumans ] = szName;
					ctLocalPlayer[ numCtHumans ] = bLocalPlayer;
					ctXuids[ numCtHumans++ ] = PlayerXuid;
				}
			}
		}
	}

	WITH_SLOT_LOCKED
	{
		// compare these versus the values already pushed
		for ( int idx = 0; idx < TEAM_MENU_MAX_PLAYERS; ++idx )
		{
			if ( ctXuids[idx] != m_CT_Xuids[idx] || m_nCTLocalPlayers[idx] != static_cast<int>( ctLocalPlayer[idx] ) || V_strncmp( m_chCTNames[idx], szCTNames[idx], MAX_PLAYER_NAME_LENGTH - 1 ) )
				SetPlayerXuid( true, idx, ctXuids[idx], szCTNames[idx], ctLocalPlayer[idx] );
			if ( tXuids[idx] != m_T_Xuids[idx] || m_nTLocalPlayers[idx] != static_cast<int>( tLocalPlayer[idx] ) || V_strncmp( m_chTNames[idx], szTNames[idx], MAX_PLAYER_NAME_LENGTH - 1 )  )
				SetPlayerXuid( false, idx, tXuids[idx], szTNames[idx], tLocalPlayer[idx] );
		}
	}
}

// This function is not slot-locked, so it MUST be called within a slot-locking block
void CCSTeamMenuScaleform::SetPlayerXuid( bool bIsCT, int index, XUID xuid, const char* pPlayerName, bool bIsLocalPlayer )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SFVALUEARRAY( args, 5 )
		{
			char csXuid[256] = { 0 };
			V_snprintf( csXuid, ARRAYSIZE( csXuid ), "%llu", xuid );

			// Make safe user name
			wchar_t bufUserName[64];
			V_UTF8ToUnicode( pPlayerName, bufUserName, sizeof( bufUserName ) );
			wchar_t bufUserNameSafe[256];
			g_pScaleformUI->MakeStringSafe( bufUserName, bufUserNameSafe, sizeof( bufUserNameSafe ) );

			g_pScaleformUI->ValueArray_SetElement( args, 0, bIsCT );
			g_pScaleformUI->ValueArray_SetElement( args, 1, index );
			g_pScaleformUI->ValueArray_SetElement( args, 2, csXuid );
			g_pScaleformUI->ValueArray_SetElement( args, 3, bufUserNameSafe );
			g_pScaleformUI->ValueArray_SetElement( args, 4, bIsLocalPlayer );
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetAvatar", args, 5 );
		}

		// Save off the Xuid we passed into Scaleform
		if ( bIsCT )
		{
			V_strcpy_safe( m_chCTNames[index], pPlayerName );
			m_CT_Xuids[index] = xuid;
			m_nCTLocalPlayers[index] = bIsLocalPlayer;
		}
		else
		{
			V_strcpy_safe( m_chTNames[index], pPlayerName );
			m_T_Xuids[index] = xuid;
			m_nTLocalPlayers[index] = bIsLocalPlayer;
		}
	}
}

void CCSTeamMenuScaleform::ViewportThink( void )
{
	// Early out if the screen isn't active
	if ( FlashAPIIsValid() && m_bVisible )
	{
		HandleForceSelect();

		UpdateTeamAvatars();

//		C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
//		if ( pLocalPlayer && pLocalPlayer->GetTeamNumber() != TEAM_UNASSIGNED && m_bSelectingTeam == false )
//		{
//			//Hide( true );
//			GetViewPortInterface()->ShowPanel( PANEL_TEAM, false );	
//		}
	}
}

void CCSTeamMenuScaleform::ResetForceSelect( void )
{
	m_nForceSelectTimeLast = -1.0f;
	m_bRedTimerVisible = false;
	m_bGreenTimerVisible = false;
}

void CCSTeamMenuScaleform::IsInitialTeamMenu( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_pScaleformUI->Params_SetResult( obj, ( m_pViewPort && !m_pViewPort->GetChoseTeamAndClass() ) );
}

void CCSTeamMenuScaleform::IsQueuedMatchmaking( SCALEFORM_CALLBACK_ARGS_DECL )
{
	bool bQ = CSGameRules() && CSGameRules()->IsQueuedMatchmaking();

	m_pScaleformUI->Params_SetResult( obj, bQ );
}





#endif
