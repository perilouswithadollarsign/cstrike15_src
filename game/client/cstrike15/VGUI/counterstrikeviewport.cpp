//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Client DLL VGUI2 Viewport
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

// vgui panel includes
#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include <keyvalues.h>
#include <vgui/Cursor.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>
#include <vgui/ILocalize.h>
#include <vgui/vgui.h>

// client dll/engine defines
#include "hud.h"
#include <voice_status.h>

// cstrike specific dialogs
#include "clientmode_csnormal.h"
#include "IGameUIFuncs.h"

// viewport definitions
#include <baseviewport.h>
#include "counterstrikeviewport.h"
#include "cs_gamerules.h"
// #include "c_user_message_register.h"
#include "vguicenterprint.h"
#include "text_message.h"

#include "teammenu_scaleform.h"
#include "chooseclass_scaleform.h"
#include "Scaleform/HUD/sfhudinfopanel.h"
#include "Scaleform/HUD/sfhudwinpanel.h"
#include "Scaleform/loadingscreen_scaleform.h"

#if defined( CSTRIKE15 )
#include "basepanel.h"
#endif

static void OpenPanelWithCheck( const char *panelToOpen, const char *panelToCheck )
{
	IViewPortPanel *checkPanel = GetViewPortInterface()->FindPanelByName( panelToCheck );
	if ( !checkPanel || !checkPanel->IsVisible() )
	{
		GetViewPortInterface()->ShowPanel( panelToOpen, true );
	}
}

void PrintBuyTimeOverMessage( void )
{
	CHudElement *pElement = GetHud().FindElement( "SFHudInfoPanel" );
	if ( pElement )														
	{																	

		char strBuyTime[16];
		int nBuyTime = ( int )CSGameRules()->GetBuyTimeLength();
		Q_snprintf( strBuyTime, sizeof( strBuyTime ), "%d", nBuyTime );

		wchar_t buffer[128];
		wchar_t buytime[16];
		g_pVGuiLocalize->ConvertANSIToUnicode( strBuyTime, buytime, sizeof( buytime ) );

		if ( nBuyTime == 0 )
			g_pVGuiLocalize->ConstructString( buffer, sizeof( buffer ), g_pVGuiLocalize->Find( "#SFUI_BuyMenu_YoureOutOfTime" ), 0 );
		else
			g_pVGuiLocalize->ConstructString( buffer, sizeof( buffer ), g_pVGuiLocalize->Find( "#SFUI_BuyMenu_OutOfTime" ), 1, buytime );

		((SFHudInfoPanel *)pElement)->SetPriorityText( buffer );				
	}
}


CON_COMMAND_F( teammenu, "Show team selection window", FCVAR_SERVER_CAN_EXECUTE )
{
	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	
	if( pPlayer && pPlayer->CanShowTeamMenu() )
	{
		( ( CounterStrikeViewport * )GetViewPortInterface() )->SetChoseTeamAndClass( true );
		GetViewPortInterface()->ShowPanel( PANEL_TEAM, true );
	}

}

CON_COMMAND_F( buymenu, "Show or hide main buy menu", FCVAR_SERVER_CAN_EXECUTE )
{
	bool bShowIt = true;

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( args.ArgC() == 2 )
	{
		bShowIt = atoi( args[ 1 ] ) == 1;
	}

	if( pPlayer && bShowIt )
	{
		if ( pPlayer->m_lifeState != LIFE_ALIVE && pPlayer->State_Get() != STATE_ACTIVE )
			return;

		extern ConVar mp_buy_anywhere;
		extern ConVar mp_buy_during_immunity;
		static ConVarRef sv_buy_status_override_ref( "sv_buy_status_override" );
		// UNUSED: int nGuardianTeam = CSGameRules()->IsHostageRescueMap() ? TEAM_TERRORIST : TEAM_CT;

		if ( CSGameRules()->IsPlayingCooperativeGametype() )
		{
			if ( CSGameRules()->IsWarmupPeriod() == false && 
				 CSGameRules()->m_flGuardianBuyUntilTime < gpGlobals->curtime )
			{
				int nTeam = CSGameRules()->IsHostageRescueMap() ? TEAM_TERRORIST : TEAM_CT;
				int iBuyStatus = sv_buy_status_override_ref.GetInt();
				if ( iBuyStatus > 0 && (( nTeam == TEAM_CT && iBuyStatus != 1 ) || ( nTeam == TEAM_TERRORIST && iBuyStatus != 2 )) )
					GetCenterPrint()->Print( "#SFUI_BuyMenu_CantBuy" );
				else
					GetCenterPrint()->Print( "#SFUI_BuyMenu_CantBuyTilNextWave" );
			}
			else
			{
				CSGameRules()->OpenBuyMenu( pPlayer->GetUserID() );
			}
		}
		else if ( CSGameRules()->IsPlayingCoopMission() && sv_buy_status_override_ref.GetInt() == 3 )
		{
			GetCenterPrint()->Print( "#SFUI_BuyMenu_CantBuy" );
		}
		else if ( !pPlayer->IsInBuyPeriod() )
		{
			PrintBuyTimeOverMessage();
		}
		else if ( !pPlayer->IsInBuyZone()  )
		{
			GetCenterPrint()->Print( "#SFUI_BuyMenu_NotInBuyZone" );
		}
		else
		{
			CSGameRules()->OpenBuyMenu( pPlayer->GetUserID() );
		}
	}
	else if( pPlayer && !bShowIt )
	{
		// Hide the menu
		CSGameRules()->CloseBuyMenu( pPlayer->GetUserID() );
	}

}

//CON_COMMAND_F( spec_help, "Show spectator help screen", FCVAR_CLIENTCMD_CAN_EXECUTE )
//{
//	if ( GetViewPortInterface() )
//		GetViewPortInterface()->ShowPanel( PANEL_INFO, true );
//}

CON_COMMAND_F( spec_menu, "Activates spectator menu", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	bool bShowIt = true;

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( pPlayer && !pPlayer->IsObserver() )
		return;

	if ( args.ArgC() == 2 )
	{
		 bShowIt = atoi( args[ 1 ] ) == 1;
	}
	
	if ( GetViewPortInterface() )
		GetViewPortInterface()->ShowPanel( PANEL_SPECMENU, bShowIt );
}

CON_COMMAND_F( spec_gui, "Shows or hides the spectator bar", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	bool bShowIt = true;

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();

	if ( pPlayer && !pPlayer->IsObserver() )
		return;

	if ( args.ArgC() == 2 )
	{
		bShowIt = atoi( args[ 1 ] ) == 1;
	}

	if ( bShowIt && GetViewPortInterface() && GetViewPortInterface()->GetActivePanel() )
	{
		// if the team screen is up, it takes precedence - don't show the spectator GUI
		if ( !V_strcmp( GetViewPortInterface()->GetActivePanel()->GetName(), PANEL_TEAM ) )
			return;
	}

	if ( GetViewPortInterface() )
		GetViewPortInterface()->ShowPanel( PANEL_SPECGUI, bShowIt );
}

CON_COMMAND_F( togglescores, "Toggles score panel", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	if ( !GetViewPortInterface() )
		return;
	
	IViewPortPanel *scoreboard = GetViewPortInterface()->FindPanelByName( PANEL_SCOREBOARD );

	if ( !scoreboard )
		return;

	if ( scoreboard->IsVisible() )
	{
		GetViewPortInterface()->ShowPanel( scoreboard, false );
		GetClientVoiceMgr()->StopSquelchMode();
	}
	else
	{
		// Disallow bringing the Scoreboard up while we are paused
		if ( BasePanel() && BasePanel()->IsScaleformPauseMenuActive() )
			return;

		GetViewPortInterface()->ShowPanel( scoreboard, true );
	}
}

CON_COMMAND_F( hidescores, "Forcibly hide score panel", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	if ( !GetViewPortInterface() )
		return;

	IViewPortPanel *scoreboard = GetViewPortInterface()->FindPanelByName( PANEL_SCOREBOARD );

	if ( !scoreboard )
		return;

	if ( scoreboard->IsVisible() )
	{
		GetViewPortInterface()->ShowPanel( scoreboard, false );
		GetClientVoiceMgr()->StopSquelchMode();
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when the VGUI subsystem starts up
//			Creates the sub panels and initialises them
//-----------------------------------------------------------------------------
void CounterStrikeViewport::Start( IGameUIFuncs *pGameUIFuncs, IGameEventManager2 * pGameEventManager )
{
	BaseClass::Start( pGameUIFuncs, pGameEventManager );
	SetChoseTeamAndClass( false );
}

void CounterStrikeViewport::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	ListenForGameEvent( "cs_win_panel_match" );

	GetHud().InitColors( pScheme );

	SetPaintBackgroundEnabled( false );
}


IViewPortPanel* CounterStrikeViewport::CreatePanelByName( const char *szPanelName )
{
	IViewPortPanel* newpanel = NULL;

	// overwrite MOD specific panel creation

 	if ( Q_strcmp( PANEL_TEAM, szPanelName ) == 0 )
 	{
 		newpanel = new CCSTeamMenuScaleform( this );
 	}

	else
	{
		// create a generic base panel, don't add twice
		newpanel = BaseClass::CreatePanelByName( szPanelName );
	}

	return newpanel; 
}

void CounterStrikeViewport::CreateDefaultPanels( void )
{
	AddNewPanel( CreatePanelByName( PANEL_TEAM ), "PANEL_TEAM" );

	AddNewPanel( CreatePanelByName( PANEL_BUY ), "PANEL_BUY" );

	BaseClass::CreateDefaultPanels();
}

int CounterStrikeViewport::GetDeathMessageStartHeight( void )
{
	int x = YRES( 2 );

	return x;
}

void CounterStrikeViewport::FireGameEvent( IGameEvent * event )
{
	const char * type = event->GetName();

	if ( Q_strcmp( type, "game_newmap" ) == 0 || Q_strcmp( type, "cs_win_panel_match" ) == 0 )
	{
		SetChoseTeamAndClass( false );
	}

	BaseClass::FireGameEvent( event );
}

void CounterStrikeViewport::UpdateAllPanels( void )
{
	bool bSomethingIsVisible = false;

	ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( vgui::ipanel()->GetMessageContextId( GetVPanel() ) );

	for ( int i = 0; i < m_UnorderedPanels.Count(); ++i )
	{
		IViewPortPanel *p = m_UnorderedPanels[i];

		if ( p->IsVisible() )
		{
			bSomethingIsVisible = true;
			p->Update();
			if ( m_pActivePanel == NULL )
			{
				// if a visible panel exists, then there should be an activePanel.
				m_pActivePanel = p;
			}
		}
	}

	// see if we need to show a special ui instead of the hud
	//	[jason] Do not rearrange viewport panels while the Pause menu is opened - it takes precedence over all viewports
	if ( !bSomethingIsVisible && !BasePanel()->IsScaleformPauseMenuActive() )
	{
		C_CSPlayer *pCSPlayer = C_CSPlayer::GetLocalCSPlayer();

		const char* UIToShow = NULL;

		if ( !pCSPlayer )
		{
			UIToShow = PANEL_SPECGUI;
		}
		else if ( pCSPlayer->GetObserverMode() != OBS_MODE_NONE )
		{
			if ( pCSPlayer->State_Get() != STATE_PICKINGTEAM && ( pCSPlayer->GetTeamNumber() == TEAM_UNASSIGNED ) && !pCSPlayer->IsHLTV() )
			{
				// not a member of a team and not a spectator. show the team select screen.
				if ( !CLoadingScreenScaleform::IsOpen() && 
					 ( (GetActivePanel() && !V_strcmp( GetActivePanel()->GetName(), PANEL_TEAM )) || !GetActivePanel() ) )
				{
					// don't show the team panel if the team panel is already up
					UIToShow = PANEL_TEAM;
				}
			}
			else
			{
				SFHudWinPanel * pWinPanel = GET_HUDELEMENT( SFHudWinPanel );
				if ( pWinPanel && !pWinPanel->IsVisible() )
				{
					UIToShow = PANEL_SPECGUI;
				}
			}
		}

		if ( UIToShow )
		{
			ShowPanel( UIToShow, true );
		}
	}
}

/*
==========================
HUD_ChatInputPosition

Sets the location of the input for chat text
==========================
*/
//MIKETODO: positioning of chat text ( and other engine output )
/*
	#include "Exports.h"

	void CL_DLLEXPORT HUD_ChatInputPosition( int *x, int *y )
	{
		RecClChatInputPosition( x, y );
		if ( GetViewPortInterface() )
		{
			GetViewPortInterface()->ChatInputPosition( x, y );
		}
	}

	EXPOSE_SINGLE_INTERFACE( CounterStrikeViewport, IClientVGUI, CLIENTVGUI_INTERFACE_VERSION );
*/
