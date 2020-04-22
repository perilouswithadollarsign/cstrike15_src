//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Normal HUD mode
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//


#include "cbase.h"
#include "clientmode_shared.h"
#include "iinput.h"
#include "view_shared.h"
#include "iviewrender.h"
#include "hud_basechat.h"
#include "weapon_selection.h"
#include <vgui/IVGui.h>
#include <vgui/Cursor.h>
#include <vgui/IPanel.h>
#include <vgui/IInput.h>
#include "engine/IEngineSound.h"
#include <keyvalues.h>
#include <vgui_controls/AnimationController.h>
#include "vgui_int.h"
#include "hud_macros.h"
#include "hltvcamera.h"
#include "hud.h"
#include "hud_element_helper.h"
#include "Scaleform/HUD/sfhud_chat.h"
#include "Scaleform/HUD/sfhudfreezepanel.h"
#include "Scaleform/HUD/sfhud_teamcounter.h"
#include "Scaleform/mapoverview.h"
#include "hltvreplaysystem.h"
#include "netmessages.h"
#if defined( REPLAY_ENABLED )
#include "replaycamera.h"
#endif
#include "particlemgr.h"
#include "c_vguiscreen.h"
#include "c_team.h"
#include "c_rumble.h"
#include "fmtstr.h"
#include "c_playerresource.h"
#include <localize/ilocalize.h>
#include "gameui_interface.h"
#include "menu.h" // CHudMenu
#if defined( _X360 )
#include "xbox/xbox_console.h"
#endif
#include "matchmaking/imatchframework.h"
#include "clientmode_csnormal.h"


#ifdef PORTAL2
#include "c_basehlplayer.h"
#endif // PORTAL2

#ifdef CSTRIKE15
#include "c_cs_playerresource.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CHudWeaponSelection;
class CHudChat;

static vgui::HContext s_hVGuiContext = DEFAULT_VGUI_CONTEXT;

ConVar cl_drawhud( "cl_drawhud", "1", FCVAR_CHEAT, "Enable the rendering of the hud" );
ConVar hud_takesshots( "hud_takesshots", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Auto-save a scoreboard screenshot at the end of a map." );
ConVar spec_usenumberkeys_nobinds( "spec_usenumberkeys_nobinds", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "If set to 1, map voting and spectator view use the raw number keys instead of the weapon binds (slot1, slot2, etc)." );
ConVar spec_cameraman_ui( "spec_cameraman_ui", "0", FCVAR_CLIENTDLL | FCVAR_CLIENTCMD_CAN_EXECUTE, "If a cameraman is active then use their UI commands (scoreboard, overview, etc.)" );
ConVar spec_cameraman_xray( "spec_cameraman_xray", "0", FCVAR_CLIENTDLL | FCVAR_CLIENTCMD_CAN_EXECUTE, "If a cameraman is active then use their Xray state." );
ConVar spec_cameraman_disable_with_user_control( "spec_cameraman_disable_with_user_control", "0", FCVAR_CLIENTDLL | FCVAR_CLIENTCMD_CAN_EXECUTE, "Disable cameraman UI control when user controls camera." );


extern ConVar v_viewmodel_fov;
extern ConVar spec_show_xray;
extern ConVar spec_hide_players;

extern bool IsInCommentaryMode( void );

CON_COMMAND( hud_reloadscheme, "Reloads hud layout and animation scripts." )
{
	g_pFullFileSystem->SyncDvdDevCache();

	for ( int hh = 0; hh < MAX_SPLITSCREEN_PLAYERS; ++hh )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD_VGUI( hh );
		ClientModeShared *mode = ( ClientModeShared * )GetClientModeNormal();
		if ( mode )
		{
			mode->ReloadScheme();
		}
	}
	ClientModeShared *mode = ( ClientModeShared * )GetFullscreenClientMode();
	if ( mode )
	{
		mode->ReloadSchemeWithRoot( VGui_GetFullscreenRootVPANEL() );
	}
}

#if 0
CON_COMMAND_F( crash, "Crash the client. Optional parameter -- type of crash:\n 0: read from NULL\n 1: write to NULL\n 2: DmCrashDump() (xbox360 only)", FCVAR_CHEAT )
{
	int crashtype = 0;
	int dummy;
	if ( args.ArgC() > 1 )
	{
		crashtype = Q_atoi( args[1] );
	}
	switch (crashtype)
	{
	case 0:
		dummy = *((int *) NULL);
		Msg("Crashed! %d\n", dummy); // keeps dummy from optimizing out
		break;
	case 1:
		*((int *)NULL) = 42;
		break;
#if defined( _GAMECONSOLE )
	case 2:
		XBX_CrashDump( false );
		break;
	case 3:
		XBX_CrashDumpFullHeap( true );
		break;
#endif
	default:
		Msg("Unknown variety of crash. You have now failed to crash. I hope you're happy.\n");
		break;
	}
}
#endif // _DEBUG

static bool __MsgFunc_Rumble( const CCSUsrMsg_Rumble &msg )
{
	unsigned char waveformIndex;
	unsigned char rumbleData;
	unsigned char rumbleFlags;

	waveformIndex = msg.index();
	rumbleData = msg.data();
	rumbleFlags = msg.flags();

	int userID = XBX_GetActiveUserId();

	RumbleEffect( userID, waveformIndex, rumbleData, rumbleFlags );

	return true;
}

static bool __MsgFunc_VGUIMenu( const CCSUsrMsg_VGUIMenu &msg )
{
	bool bShow = msg.show();

	ASSERT_LOCAL_PLAYER_RESOLVABLE();

	KeyValues *keys = NULL;

	if ( msg.subkeys_size() > 0 )
	{
		keys = new KeyValues("data");

		for (int i = 0; i < msg.subkeys_size(); i ++ )
		{
			const CCSUsrMsg_VGUIMenu::Subkey& subkey = msg.subkeys( i );
						
			keys->SetString( subkey.name().c_str(), subkey.str().c_str() );
		}
	}

	GetViewPortInterface()->ShowPanel( msg.name().c_str(), bShow, keys, true );

	// Don't do this since ShowPanel auto-deletes the keys
	// keys->deleteThis();

	// is the server telling us to show the scoreboard (at the end of a map)?
	if ( Q_stricmp( msg.name().c_str(), "scores" ) == 0 )
	{
		if ( hud_takesshots.GetBool() == true )
		{
			GetHud().SetScreenShotTime( gpGlobals->curtime + 1.0 ); // take a screenshot in 1 second
		}
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ClientModeShared::ClientModeShared()
{
	m_pViewport = NULL;
	m_pChatElement = NULL;
	m_pWeaponSelection = NULL;
	m_nRootSize[ 0 ] = m_nRootSize[ 1 ] = -1;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ClientModeShared::~ClientModeShared()
{
	// VGui_Shutdown() should have deleted/NULL'd
	Assert( !m_pViewport );
}

void ClientModeShared::ReloadScheme( void )
{
	ReloadSchemeWithRoot( VGui_GetClientDLLRootPanel() );
}

void ClientModeShared::ReloadSchemeWithRoot( vgui::VPANEL pRoot )
{
	if ( pRoot )
	{
		int wide, tall;
		vgui::ipanel()->GetSize(pRoot, wide, tall);
		m_nRootSize[ 0 ] = wide;
		m_nRootSize[ 1 ] = tall;
	}

	m_pViewport->ReloadScheme( "resource/ClientScheme.res" );
	if ( GET_ACTIVE_SPLITSCREEN_SLOT() == 0 )
	{
		ClearKeyValuesCache();
	}
	// Msg( "Reload scheme [%d]\n", GET_ACTIVE_SPLITSCREEN_SLOT() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::Init()
{
	InitChatHudElement();

	InitWeaponSelectionHudElement();

	// Derived ClientMode class must make sure m_Viewport is instantiated
	Assert( m_pViewport );
	m_pViewport->LoadHudLayout();

	ListenForGameEvent( "player_connect_full" );
	ListenForGameEvent( "player_connect" );
	ListenForGameEvent( "player_disconnect" );
	ListenForGameEvent( "player_team" );
	ListenForGameEvent( "server_cvar" );
	ListenForGameEvent( "player_changename" );
	ListenForGameEvent( "teamplay_broadcast_audio" );
	ListenForGameEvent( "achievement_earned" );

#if defined( TF_CLIENT_DLL ) || defined( CSTRIKE_CLIENT_DLL )
	ListenForGameEvent( "item_found" );
	ListenForGameEvent( "items_gifted" );
#endif

#if defined( INFESTED_DLL )
	ListenForGameEvent( "player_fullyjoined" );	
#endif




	HLTVCamera()->Init();
#if defined( REPLAY_ENABLED )
	ReplayCamera()->Init();
#endif

	m_CursorNone = vgui::dc_none;

	HOOK_MESSAGE( VGUIMenu );
	HOOK_MESSAGE( Rumble );
}

void ClientModeShared::InitChatHudElement()
{
	m_pChatElement = CBaseHudChat::GetHudChat();
	Assert( m_pChatElement );
}

void ClientModeShared::InitWeaponSelectionHudElement()
{
	m_pWeaponSelection = ( CBaseHudWeaponSelection * )GET_HUDELEMENT( CHudWeaponSelection );
	Assert( m_pWeaponSelection );
}

void ClientModeShared::InitViewport()
{
}


void ClientModeShared::VGui_Shutdown()
{
	delete m_pViewport;
	m_pViewport = NULL;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::Shutdown()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : frametime - 
//			*cmd - 
//-----------------------------------------------------------------------------
bool ClientModeShared::CreateMove( float flInputSampleTime, CUserCmd *cmd )
{
	// Let the player override the view.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if(!pPlayer)
		return true;

	// Let the player at it
	return pPlayer->CreateMove( flInputSampleTime, cmd );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSetup - 
//-----------------------------------------------------------------------------
void ClientModeShared::OverrideView( CViewSetup *pSetup )
{
	QAngle camAngles;

	// Let the player override the view.
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if(!pPlayer)
		return;

	pPlayer->OverrideView( pSetup );

	if( ::input->CAM_IsThirdPerson() )
	{
		Vector cam_ofs;

		::input->CAM_GetCameraOffset( cam_ofs );

		camAngles[ PITCH ] = cam_ofs[ PITCH ];
		camAngles[ YAW ] = cam_ofs[ YAW ];
		camAngles[ ROLL ] = 0;

		Vector camForward, camRight, camUp;
		AngleVectors( camAngles, &camForward, &camRight, &camUp );

		float flSavedZ = pSetup->origin.z;
		pSetup->origin = pPlayer->GetThirdPersonViewPosition();
		pSetup->origin.z -= (pSetup->origin.z - flSavedZ);

		VectorMA( pSetup->origin, -cam_ofs[ ROLL ], camForward, pSetup->origin );

		static ConVarRef c_thirdpersonshoulder( "c_thirdpersonshoulder" );
		if ( c_thirdpersonshoulder.GetBool() )
		{
			static ConVarRef c_thirdpersonshoulderoffset( "c_thirdpersonshoulderoffset" );
			static ConVarRef c_thirdpersonshoulderheight( "c_thirdpersonshoulderheight" );
			static ConVarRef c_thirdpersonshoulderaimdist( "c_thirdpersonshoulderaimdist" );

			// add the shoulder offset to the origin in the cameras right vector
			VectorMA( pSetup->origin, c_thirdpersonshoulderoffset.GetFloat(), camRight, pSetup->origin );

			// add the shoulder height to the origin in the cameras up vector
			VectorMA( pSetup->origin, c_thirdpersonshoulderheight.GetFloat(), camUp, pSetup->origin );

			// adjust the yaw to the aim-point
			camAngles[ YAW ] += RAD2DEG( atan(c_thirdpersonshoulderoffset.GetFloat() / (c_thirdpersonshoulderaimdist.GetFloat() + cam_ofs[ ROLL ])) );

			// adjust the pitch to the aim-point
			camAngles[ PITCH ] += RAD2DEG( atan(c_thirdpersonshoulderheight.GetFloat() / (c_thirdpersonshoulderaimdist.GetFloat() + cam_ofs[ ROLL ])) );
		}

		// Override angles from third person camera
		VectorCopy( camAngles, pSetup->angles );
	}
	else if (::input->CAM_IsOrthographic())
	{
		pSetup->m_bOrtho = true;
		float w, h;
		::input->CAM_OrthographicSize( w, h );
		w *= 0.5f;
		h *= 0.5f;
		pSetup->m_OrthoLeft   = -w;
		pSetup->m_OrthoTop    = -h;
		pSetup->m_OrthoRight  = w;
		pSetup->m_OrthoBottom = h;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool ClientModeShared::ShouldDrawEntity(C_BaseEntity *pEnt)
{
	return true;
}

bool ClientModeShared::ShouldDrawParticles( )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Allow weapons to override mouse input (for binoculars)
//-----------------------------------------------------------------------------
void ClientModeShared::OverrideMouseInput( float *x, float *y )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	C_BaseCombatWeapon *pWeapon = pPlayer ? pPlayer->GetActiveWeapon() : NULL;;
	if ( pWeapon )
	{
		pWeapon->OverrideMouseInput( x, y );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool ClientModeShared::ShouldDrawViewModel()
{
	return true;
}

bool ClientModeShared::ShouldDrawDetailObjects( )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool ClientModeShared::ShouldDrawCrosshair( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Don't draw the current view entity if we are not in 3rd person
//-----------------------------------------------------------------------------
bool ClientModeShared::ShouldDrawLocalPlayer( C_BasePlayer *pPlayer )
{
	if ( pPlayer->IsViewEntity() && !pPlayer->ShouldDrawLocalPlayer() )
		return false;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: The mode can choose to not draw fog
//-----------------------------------------------------------------------------
bool ClientModeShared::ShouldDrawFog( void )
{
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::AdjustEngineViewport( int& x, int& y, int& width, int& height )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::PreRender( CViewSetup *pSetup )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::PostRender()
{
	// Let the particle manager simulate things that haven't been simulated.
	ParticleMgr()->PostRender();
}

void ClientModeShared::PostRenderVGui()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::Update()
{
	if ( m_pViewport->IsVisible() != cl_drawhud.GetBool() )
	{
		m_pViewport->SetVisible( cl_drawhud.GetBool() );
	}

	UpdateRumbleEffects( XBX_GetActiveUserId() );
}

//-----------------------------------------------------------------------------
// This processes all input before SV Move messages are sent
//-----------------------------------------------------------------------------

void ClientModeShared::ProcessInput(bool bActive)
{
	GetHud().ProcessInput( bActive );
}

//-----------------------------------------------------------------------------
// Purpose: We've received a keypress from the engine. Return 1 if the engine is allowed to handle it.
//-----------------------------------------------------------------------------
int	ClientModeShared::KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	if ( engine->Con_IsVisible() )
		return 1;

	// Should we start typing a message?
	if ( pszCurrentBinding &&
		( Q_strcmp( pszCurrentBinding, "messagemode" ) == 0 ||
		Q_strcmp( pszCurrentBinding, "say" ) == 0 ) )
	{
		if ( down )
		{
			StartMessageMode( MM_SAY );
		}
		return 0;
	}
	else if ( pszCurrentBinding &&
		( Q_strcmp( pszCurrentBinding, "messagemode2" ) == 0 ||
		Q_strcmp( pszCurrentBinding, "say_team" ) == 0 ) )
	{
		if ( down )
		{
			StartMessageMode( MM_SAY_TEAM );
		}
		return 0;
	}

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();

	if ( IsJoystickCode( keynum ) )
	{
		keynum = GetBaseButtonCode( keynum );
	}

	// If SourceMod menu is open (they use CHudMenu), give it input priority.
	bool bIsHudMenuOpen = false;
	CHudMenu *pHudMenu = GET_HUDELEMENT( CHudMenu );
	bIsHudMenuOpen = ( pHudMenu && pHudMenu->IsMenuOpen() );
	if ( bIsHudMenuOpen && !HudElementKeyInput( down, keynum, pszCurrentBinding ) )
	{
		return 0;
	}

	// if ingame spectator mode, let spectator input intercept key event here
	if( pPlayer &&
		( pPlayer->GetObserverMode() > OBS_MODE_DEATHCAM ) &&
		!HandleSpectatorKeyInput( down, keynum, pszCurrentBinding ) )
	{
		return 0;
	}

	// Let game-specific hud elements get a crack at the key input
	if ( !HudElementKeyInput( down, keynum, pszCurrentBinding ) )
	{
		return 0;
	}

	C_BaseCombatWeapon *pWeapon = pPlayer ? pPlayer->GetActiveWeapon() : NULL;
	if ( pWeapon )
	{
		return pWeapon->KeyInput( down, keynum, pszCurrentBinding );
	}

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: Helper to find if a binding exists in a possible chain of bindings
//-----------------------------------------------------------------------------
bool ContainsBinding( const char *pszBindingString, const char *pszBinding, bool bSearchAliases /*= false*/ )
{
	if ( !strchr( pszBindingString, ';' ) && !bSearchAliases )
	{
		return !Q_stricmp( pszBindingString, pszBinding );
	}
	else
	{
		// Tokenize the binding name
		CUtlVectorAutoPurge< char *> cmdStrings;
		V_SplitString( pszBindingString, ";", cmdStrings );
		FOR_EACH_VEC( cmdStrings, i )
		{
			char* szCmd = cmdStrings[ i ];
			if ( bSearchAliases )
			{
				// Search for command in any contained aliases. 
				const char* szAliasCmd = engine->AliasToCommandString( szCmd );
				// NOTE: we could use some kind of recursion guard, but recursive aliases already infinite loop 
				// when being processed by the cmd system. 
				if ( szAliasCmd )
				{
					CUtlString strCmd( szAliasCmd );
					V_StripTrailingWhitespace( strCmd.Access() ); // Alias adds trailing spaces to commands, strip it here so the compare works
					if ( ContainsBinding( strCmd.Get(), pszBinding, true ) )
						return true;
				}
			}

			if ( !Q_stricmp( pszBinding, szCmd ) )
			{
				return true;
			}
		}
		return false;
	}
}

void ClientModeShared::UpdateCameraManUIState( int iType, int nOptionalParam, uint64 xuid )
{
	/* Removed for partner depot */
}

void SendCameraManUIStateChange( HltvUiType_t eventType, int nOptionalParam )
{
	// this sends a client command to the server which will then change the server side states and propagate that out to everyone
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer && pPlayer->IsActiveCameraMan() )
	{
		char szTemp[ 256 ];
		V_sprintf_safe( szTemp, "cameraman_ui_state %d %d", eventType, nOptionalParam );
		engine->ClientCmd( szTemp );
	}
}

void ClientModeShared::ScoreboardOff()
{
	//SendCameraManUIStateChange( HLTV_UI_SCOREBOARD_OFF );
}

void ClientModeShared::GraphPageChanged()
{
	/* Removed for partner depot */
}

//-----------------------------------------------------------------------------
// Purpose: See if spectator input occurred. Return 0 if the key is swallowed.
//-----------------------------------------------------------------------------
int ClientModeShared::HandleSpectatorKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	/* Removed for partner depot */
	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: See if hud elements want key input. Return 0 if the key is swallowed
//-----------------------------------------------------------------------------
int ClientModeShared::HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding )
{
	if ( GetFullscreenClientMode() && GetFullscreenClientMode() != this &&
		!GetFullscreenClientMode()->HudElementKeyInput( down, keynum, pszCurrentBinding ) )
		return 0;

	if ( CSGameRules() && CSGameRules()->IsEndMatchVotingForNextMap() )
	{
		// this looks messy, but essentially, if the convar is set to true, use the bindings, if not use the raw keys
		if ( down && (( spec_usenumberkeys_nobinds.GetBool() == false && pszCurrentBinding &&
			( ContainsBinding( pszCurrentBinding, "slot1" ) ||
			ContainsBinding( pszCurrentBinding, "slot2" ) ||
			ContainsBinding( pszCurrentBinding, "slot3" ) ||
			ContainsBinding( pszCurrentBinding, "slot4" ) ||
			ContainsBinding( pszCurrentBinding, "slot5" ) ||
			ContainsBinding( pszCurrentBinding, "slot6" ) ||
			ContainsBinding( pszCurrentBinding, "slot7" ) ||
			ContainsBinding( pszCurrentBinding, "slot8" ) ||
			ContainsBinding( pszCurrentBinding, "slot9" ) ||
			ContainsBinding( pszCurrentBinding, "slot10" ) ) )
			||
			( spec_usenumberkeys_nobinds.GetBool() == true &&
			( keynum == KEY_1 ||
			keynum == KEY_2 ||
			keynum == KEY_3 ||
			keynum == KEY_4 ||
			keynum == KEY_5 ||
			keynum == KEY_6 ||
			keynum == KEY_7 ||
			keynum == KEY_8 ||
			keynum == KEY_9 ||
			keynum == KEY_0 ) ) ) )
		{
			int slotnum = 0;
			if ( spec_usenumberkeys_nobinds.GetBool() )
			{
				slotnum = ( keynum - KEY_0 ) - 1;
			}
			else
			{
				char* slotnumberchar = ( char * )pszCurrentBinding + strlen( pszCurrentBinding ) - 1;
				slotnum = atoi( slotnumberchar ) - 1;
			}

			if ( slotnum < 0 )
				slotnum = 10 + slotnum;

			char commandBuffer[32];
			V_snprintf( commandBuffer, sizeof( commandBuffer ), "endmatch_votenextmap %d", slotnum );
			engine->ClientCmd( commandBuffer );

			return 0;
		}
	}

	if ( down && pszCurrentBinding && ContainsBinding( pszCurrentBinding, "radio1" ) )
	{
		/* Removed for partner depot */
		return 0;
	}

	if ( m_pWeaponSelection )
	{
		if ( !m_pWeaponSelection->KeyInput( down, keynum, pszCurrentBinding ) )
		{
			return 0;
		}
	}		

	return 1;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : vgui::Panel
//-----------------------------------------------------------------------------
vgui::Panel *ClientModeShared::GetMessagePanel()
{
	if ( m_pChatElement && m_pChatElement->GetInputPanel() && m_pChatElement->GetInputPanel()->IsVisible() )
		return m_pChatElement->GetInputPanel();

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: The player has started to type a message
//-----------------------------------------------------------------------------
void ClientModeShared::StartMessageMode( int iMessageModeType )
{
	// Can only show chat UI in multiplayer!!!
	if ( gpGlobals->maxClients == 1 )
	{
		return;
	}

	SFHudChat* pChat = GET_HUDELEMENT( SFHudChat );
	if ( pChat )
	{
		pChat->StartMessageMode( iMessageModeType );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *newmap - 
//-----------------------------------------------------------------------------
void ClientModeShared::LevelInit( const char *newmap )
{
	m_pViewport->GetAnimationController()->StartAnimationSequence("LevelInit");

	// Tell the Chat Interface
	if ( m_pChatElement )
	{
		m_pChatElement->LevelInit( newmap );
	}

	// we have to fake this event clientside, because clients connect after that
	IGameEvent *event = gameeventmanager->CreateEvent( "game_newmap" );
	if ( event )
	{
		event->SetString("mapname", newmap );
		gameeventmanager->FireEventClientSide( event );
	}

	// Create a vgui context for all of the in-game vgui panels...
	if ( s_hVGuiContext == DEFAULT_VGUI_CONTEXT )
	{
		s_hVGuiContext = vgui::ivgui()->CreateContext();
	}

	// Reset any player explosion/shock effects
	CLocalPlayerFilter filter;
	enginesound->SetPlayerDSP( filter, 0, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeShared::LevelShutdown( void )
{
	if ( m_pChatElement )
	{
	m_pChatElement->LevelShutdown();
	}
	if ( s_hVGuiContext != DEFAULT_VGUI_CONTEXT )
	{
		vgui::ivgui()->DestroyContext( s_hVGuiContext );
		s_hVGuiContext = DEFAULT_VGUI_CONTEXT;
	}

	// Reset any player explosion/shock effects
	CLocalPlayerFilter filter;
	enginesound->SetPlayerDSP( filter, 0, true );
}

void ClientModeShared::Enable()
{
	vgui::VPANEL pRoot = VGui_GetClientDLLRootPanel();
	EnableWithRootPanel( pRoot );
}

void ClientModeShared::EnableWithRootPanel( vgui::VPANEL pRoot )
{
	// Add our viewport to the root panel.
	if( pRoot != NULL )
	{
		m_pViewport->SetParent( pRoot );
	}

	// All hud elements should be proportional
	// This sets that flag on the viewport and all child panels
	m_pViewport->SetProportional( true );

	m_pViewport->SetCursor( m_CursorNone );
	vgui::surface()->SetCursor( m_CursorNone );

	m_pViewport->SetVisible( true );
	if ( m_pViewport->IsKeyBoardInputEnabled() )
	{
		m_pViewport->RequestFocus();
	}

	Layout();
}


void ClientModeShared::Disable()
{
	vgui::VPANEL pRoot;

	// Remove our viewport from the root panel.
	if( ( pRoot = VGui_GetClientDLLRootPanel() ) != NULL )
	{
		m_pViewport->SetParent( (vgui::VPANEL)NULL );
	}

	m_pViewport->SetVisible( false );
}


void ClientModeShared::Layout( bool bForce /*= false*/)
{
	vgui::VPANEL pRoot;
	int wide, tall;

	// Make the viewport fill the root panel.
	if( ( pRoot = m_pViewport->GetVParent() ) != NULL )
	{
		vgui::ipanel()->GetSize(pRoot, wide, tall);
		bool changed = wide != m_nRootSize[ 0 ] || tall != m_nRootSize[ 1 ];
		m_pViewport->SetBounds(0, 0, wide, tall);
		if ( changed || bForce )
		{
			ReloadSchemeWithRoot( pRoot );
		}
	}
}

#ifdef IRONSIGHT
#ifdef DEBUG
	ConVar ironsight_scoped_viewmodel_fov( "ironsight_scoped_viewmodel_fov", "54", FCVAR_CHEAT, "The fov of the viewmodel when ironsighted" );
#else
	#define IRONSIGHT_SCOPED_FOV 54.0f
#endif
#endif

float ClientModeShared::GetViewModelFOV( void )
{

#ifdef IRONSIGHT
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pPlayer )
	{
		CWeaponCSBase *pIronSightWeapon = (CWeaponCSBase*)pPlayer->GetActiveWeapon();
		if ( pIronSightWeapon )
		{
			CIronSightController* pIronSightController = pIronSightWeapon->GetIronSightController();
			if ( pIronSightController && pIronSightController->IsInIronSight() )
			{
				return FLerp( v_viewmodel_fov.GetFloat(),	
					#ifdef DEBUG
						ironsight_scoped_viewmodel_fov.GetFloat(),
					#else
						IRONSIGHT_SCOPED_FOV,
					#endif
				pIronSightController->GetIronSightAmount() );
			}
		}
	}
#endif

	return v_viewmodel_fov.GetFloat();
}

vgui::Panel *ClientModeShared::GetPanelFromViewport( const char *pchNamePath )
{
	char szTagetName[ 256 ];
	Q_strncpy( szTagetName, pchNamePath, sizeof(szTagetName) );

	char *pchName = szTagetName;

	char *pchEndToken = strchr( pchName, ';' );
	if ( pchEndToken )
	{
		*pchEndToken = '\0';
	}

	char *pchNextName = strchr( pchName, '/' );
	if ( pchNextName )
	{
		*pchNextName = '\0';
		pchNextName++;
	}

	// Comma means we want to count to a specific instance by name
	int nInstance = 0;

	char *pchInstancePos = strchr( pchName, ',' );
	if ( pchInstancePos )
	{
		*pchInstancePos = '\0';
		pchInstancePos++;

		nInstance = atoi( pchInstancePos );
	}

	// Find the child
	int nCurrentInstance = 0;
	vgui::Panel *pPanel = NULL;

	for ( int i = 0; i < GetViewport()->GetChildCount(); i++ )
	{
		Panel *pChild = GetViewport()->GetChild( i );
		if ( !pChild )
			continue;

		if ( stricmp( pChild->GetName(), pchName ) == 0 )
		{
			nCurrentInstance++;

			if ( nCurrentInstance > nInstance )
			{
				pPanel = pChild;
				break;
			}
		}
	}

	pchName = pchNextName;

	while ( pPanel )
	{
		if ( !pchName || pchName[ 0 ] == '\0' )
		{
			break;
		}

		pchNextName = strchr( pchName, '/' );
		if ( pchNextName )
		{
			*pchNextName = '\0';
			pchNextName++;
		}

		// Comma means we want to count to a specific instance by name
		nInstance = 0;

		pchInstancePos = strchr( pchName, ',' );
		if ( pchInstancePos )
		{
			*pchInstancePos = '\0';
			pchInstancePos++;

			nInstance = atoi( pchInstancePos );
		}

		// Find the child
		nCurrentInstance = 0;
		vgui::Panel *pNextPanel = NULL;

		for ( int i = 0; i < pPanel->GetChildCount(); i++ )
		{
			Panel *pChild = pPanel->GetChild( i );
			if ( !pChild )
				continue;

			if ( stricmp( pChild->GetName(), pchName ) == 0 )
			{
				nCurrentInstance++;

				if ( nCurrentInstance > nInstance )
				{
					pNextPanel = pChild;
					break;
				}
			}
		}

		pPanel = pNextPanel;
		pchName = pchNextName;
	}

	return pPanel;
}

class CHudChat;

bool PlayerNameNotSetYet( const char *pszName )
{
	if ( pszName && pszName[0] )
	{
		// Don't show "unconnected" if we haven't got the players name yet
		if ( StringHasPrefix( pszName, "unconnected" ) )
			return true;
		if ( StringHasPrefix( pszName, "NULLNAME" ) )
			return true;
	}

	return false;
}

void ClientModeShared::FireGameEvent( IGameEvent *event )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GetSplitScreenPlayerSlot() );

	CBaseHudChat *hudChat = CBaseHudChat::GetHudChat();

	const char *eventname = event->GetName();

	if ( Q_strcmp( "player_connect", eventname ) == 0 )
	{
#ifdef PORTAL2
		// dont show these message on the console at all
		if ( IsGameConsole() )
			return;
#endif

		if ( this == GetFullscreenClientMode() )
			return;
		if ( !hudChat )
			return;
		if ( PlayerNameNotSetYet(event->GetString("name")) )
			return;

		if ( !IsInCommentaryMode() )
		{
			wchar_t wszLocalized[100];
			wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
			g_pVGuiLocalize->ConvertANSIToUnicode( event->GetString("name"), wszPlayerName, sizeof(wszPlayerName) );
			g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#game_player_connecting" ), 1, wszPlayerName );

			char szLocalized[100];
			g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalized, szLocalized, sizeof(szLocalized) );

			hudChat->Printf( CHAT_FILTER_JOINLEAVE, "%s", szLocalized );
		}
	}
	else if ( Q_strcmp( "player_connect_full", eventname ) == 0 )
	{
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "Music.StopMenuMusic" );
		GameUI().SetBackgroundMusicDesired( false );
	}
	else if ( Q_strcmp( "player_disconnect", eventname ) == 0 )
	{
#ifdef PORTAL2
		// dont show these message on the console at all
		if ( IsGameConsole() )
			return;
#endif

		if ( this == GetFullscreenClientMode() )
			return;
		int userID = event->GetInt("userid");
		C_BasePlayer *pPlayer = USERID2PLAYER( userID );

		// don't show disconnects for bots in coop
		if ( CSGameRules() && CSGameRules()->IsPlayingCooperativeGametype() && (pPlayer && pPlayer->IsBot()) )
			return;

		if ( !hudChat || !pPlayer )
			return;
		if ( PlayerNameNotSetYet(event->GetString("name")) )
			return;

		if ( !IsInCommentaryMode() )
		{

#ifdef CSTRIKE15
			wchar_t wszPlayerName[MAX_DECORATED_PLAYER_NAME_LENGTH];
			C_CS_PlayerResource *pCSPR = ( C_CS_PlayerResource* )GameResources();
			pCSPR->GetDecoratedPlayerName( pPlayer->entindex(), wszPlayerName, sizeof( wszPlayerName ), ( EDecoratedPlayerNameFlag_t) ( k_EDecoratedPlayerNameFlag_DontUseNameOfControllingPlayer | k_EDecoratedPlayerNameFlag_DontUseAssassinationTargetName ) );
#else
			wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
			g_pVGuiLocalize->ConvertANSIToUnicode( pPlayer->GetPlayerName(), wszPlayerName, sizeof(wszPlayerName) );
#endif

			wchar_t wszReasonBuf[64];
			wchar_t const *wszReason = wszReasonBuf;
			char const *szReasonToken = event->GetString("reason");

			static bool s_bPerfectWorld = !!CommandLine()->FindParm( "-perfectworld" ); // we don't localize reasons in Perfect World because they are supplied by server
			if ( s_bPerfectWorld && !( szReasonToken && szReasonToken[0] == '#' ) )
				wszReason = g_pVGuiLocalize->Find( "#SFUI_Disconnect_Title" );
			else
				g_pVGuiLocalize->ConvertANSIToUnicode( event->GetString( "reason" ), wszReasonBuf, sizeof( wszReasonBuf ) );

			wchar_t wszLocalized[100];
			if ( IsPC() )
			{
				g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#game_player_left_game" ), 2, wszPlayerName, wszReason );
			}
			else
			{
				g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#game_player_left_game" ), 1, wszPlayerName );
			}

			char szLocalized[100];
			g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalized, szLocalized, sizeof(szLocalized) );

			hudChat->Printf( CHAT_FILTER_JOINLEAVE, "%s", szLocalized );
		}
	}
	else if ( Q_strcmp( "player_fullyjoined", eventname ) == 0 )
	{
#ifdef PORTAL2
		// dont show these message on the console at all
		if ( IsGameConsole() )
			return;
#endif
		if ( !hudChat )
			return;
		if ( PlayerNameNotSetYet(event->GetString("name")) )
			return;

		wchar_t wszLocalized[100];
		wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
		g_pVGuiLocalize->ConvertANSIToUnicode( event->GetString("name"), wszPlayerName, sizeof(wszPlayerName) );
		g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#game_player_joined_game" ), 1, wszPlayerName );

		char szLocalized[100];
		g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalized, szLocalized, sizeof(szLocalized) );

		hudChat->Printf( CHAT_FILTER_JOINLEAVE, "%s", szLocalized );
	}
	else if ( Q_strcmp( "player_team", eventname ) == 0 )
	{
		if ( this == GetFullscreenClientMode() )
			return;

		C_BasePlayer *pPlayer = USERID2PLAYER( event->GetInt("userid") );
		if ( !hudChat )
			return;
		if ( !pPlayer )
			return;

		bool bDisconnected = event->GetBool("disconnect");

		if ( bDisconnected )
			return;

		int team = event->GetInt( "team" );
		bool bAutoTeamed = event->GetBool( "autoteam", false );
		bool bSilent = event->GetBool( "silent", false );

		const char *pszName = pPlayer->GetPlayerName();
		if ( PlayerNameNotSetYet(pszName) )
			return;

		if ( !bSilent )
		{
			wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
			g_pVGuiLocalize->ConvertANSIToUnicode( pszName, wszPlayerName, sizeof(wszPlayerName) );

			wchar_t wszTeam[64];
			C_Team *pTeam = GetGlobalTeam( team );
			if ( pTeam )
			{
				g_pVGuiLocalize->ConvertANSIToUnicode( pTeam->Get_Name(), wszTeam, sizeof(wszTeam) );
			}
			else
			{
				Q_snwprintf ( wszTeam, sizeof( wszTeam ) / sizeof( wchar_t ), L"%d", team );
			}

			if ( !IsInCommentaryMode() )
			{
				wchar_t wszLocalized[100];
				if ( bAutoTeamed )
				{
					g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#game_player_joined_autoteam" ), 2, wszPlayerName, wszTeam );
				}
				else
				{
					g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#game_player_joined_team" ), 2, wszPlayerName, wszTeam );
				}

				char szLocalized[100];
				g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalized, szLocalized, sizeof(szLocalized) );

				hudChat->Printf( CHAT_FILTER_TEAMCHANGE, "%s", szLocalized );
			}
		}

		if ( C_BasePlayer::IsLocalPlayer( pPlayer ) )
		{
			ACTIVE_SPLITSCREEN_PLAYER_GUARD_ENT( pPlayer );

			// that's me
			pPlayer->TeamChange( team );
		}
	}
	else if ( Q_strcmp( "player_changename", eventname ) == 0 )
	{
		if ( this == GetFullscreenClientMode() )
			return;

		if ( !hudChat )
			return;

		const char *pszOldName = event->GetString("oldname");
		if ( PlayerNameNotSetYet(pszOldName) )
			return;

		wchar_t wszOldName[MAX_PLAYER_NAME_LENGTH];
		g_pVGuiLocalize->ConvertANSIToUnicode( pszOldName, wszOldName, sizeof(wszOldName) );

		wchar_t wszNewName[MAX_PLAYER_NAME_LENGTH];
		g_pVGuiLocalize->ConvertANSIToUnicode( event->GetString( "newname" ), wszNewName, sizeof(wszNewName) );

		wchar_t wszLocalized[100];
		g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#game_player_changed_name" ), 2, wszOldName, wszNewName );

		char szLocalized[100];
		g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalized, szLocalized, sizeof(szLocalized) );

		hudChat->Printf( CHAT_FILTER_NAMECHANGE, "%s", szLocalized );
	}
	else if ( Q_strcmp( "teamplay_broadcast_audio", eventname ) == 0 )
	{
		if ( this == GetFullscreenClientMode() )
			return;

		int team = event->GetInt( "team" );

		bool bValidTeam = false;

		if ( (GetLocalTeam() && GetLocalTeam()->GetTeamNumber() == team) )
		{
			bValidTeam = true;
		}

		//If we're in the spectator team then we should be getting whatever messages the person I'm spectating gets.
		if ( bValidTeam == false )
		{
			CBasePlayer *pSpectatorTarget = UTIL_PlayerByIndex( GetSpectatorTarget() );

			if ( pSpectatorTarget && (GetSpectatorMode() == OBS_MODE_IN_EYE || GetSpectatorMode() == OBS_MODE_CHASE) )
			{
				if ( pSpectatorTarget->GetTeamNumber() == team )
				{
					bValidTeam = true;
				}
			}
		}

		if ( team == 0 && GetLocalTeam() > 0 )
		{
			bValidTeam = false;
		}

		if ( team == 255 )
		{
			bValidTeam = true;
		}

		if ( bValidTeam == true )
		{
			CLocalPlayerFilter filter;
			const char *pszSoundName = event->GetString("sound");
			C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, pszSoundName );
		}
	}
	else if ( Q_strcmp( "teamplay_broadcast_audio", eventname ) == 0 )
	{
		if ( this == GetFullscreenClientMode() )
			return;

		int team = event->GetInt( "team" );
		if ( !team || (GetLocalTeam() && GetLocalTeam()->GetTeamNumber() == team) )
		{
			CLocalPlayerFilter filter;
			const char *pszSoundName = event->GetString("sound");
			C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, pszSoundName );
		}
	}
	else if ( Q_strcmp( "server_cvar", eventname ) == 0 )
	{
		if ( (IsGameConsole() && IsCert()) || (IsGameConsole() && developer.GetInt() < 2) )
			return;

		if ( this == GetFullscreenClientMode() )
			return;

		static bool s_bPerfectWorld = !!CommandLine()->FindParm( "-perfectworld" );
		if ( s_bPerfectWorld )
			return; // Perfect World cvars are not printed in client

		char const *szCvarName = event->GetString("cvarname");
		char const * arrIgnoredCvars[] = { "nextlevel" };
		bool bIgnoredCvar = false;
		for ( int ii = 0; ii < Q_ARRAYSIZE( arrIgnoredCvars ); ++ ii )
		{
			if ( !V_stricmp( szCvarName, arrIgnoredCvars[ii] ) )
			{
				bIgnoredCvar = true;
				break;
			}
		}

		if ( !IsInCommentaryMode() && !bIgnoredCvar )
		{
			wchar_t wszCvarName[64];
			g_pVGuiLocalize->ConvertANSIToUnicode( event->GetString("cvarname"), wszCvarName, sizeof(wszCvarName) );

			wchar_t wszCvarValue[16];
			g_pVGuiLocalize->ConvertANSIToUnicode( event->GetString("cvarvalue"), wszCvarValue, sizeof(wszCvarValue) );

			wchar_t wszLocalized[100];
			g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#game_server_cvar_changed" ), 2, wszCvarName, wszCvarValue );

			char szLocalized[100];
			g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalized, szLocalized, sizeof(szLocalized) );

			hudChat->Printf( CHAT_FILTER_SERVERMSG, "%s", szLocalized );
		}
	}
	else if ( Q_strcmp( "achievement_earned", eventname ) == 0 )
	{
		if ( this == GetFullscreenClientMode() )
			return;

		int iPlayerIndex = event->GetInt( "player" );
		C_BasePlayer *pPlayer = UTIL_PlayerByIndex( iPlayerIndex );
		int iAchievement = event->GetInt( "achievement" );

		if ( !hudChat || !pPlayer )
			return;

		if ( !IsInCommentaryMode() )
		{
			char const *szAchievementName = NULL; // should arrive as part of event instead of achievement ID
			if ( szAchievementName )
			{
				if ( !pPlayer->IsDormant() )
				{
					// no particle effect if the local player is the one with the achievement or the player is dead
					if ( !C_BasePlayer::IsLocalPlayer( pPlayer ) && pPlayer->IsAlive() ) 
					{
						//tagES using the "head" attachment won't work for CS and DoD
						pPlayer->ParticleProp()->Create( "achieved", PATTACH_POINT_FOLLOW, "head" );
					}

					pPlayer->OnAchievementAchieved( iAchievement );
				}

				if ( g_PR )
				{
					wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
					g_pVGuiLocalize->ConvertANSIToUnicode( g_PR->GetPlayerName( iPlayerIndex ), wszPlayerName, sizeof( wszPlayerName ) );

					const wchar_t *pchLocalizedAchievement = g_pVGuiLocalize->Find( CFmtStr( "#%s_T", szAchievementName ) );
					if ( pchLocalizedAchievement )
					{
						wchar_t wszLocalizedString[128];
						g_pVGuiLocalize->ConstructString( wszLocalizedString, sizeof( wszLocalizedString ), g_pVGuiLocalize->Find( "#Achievement_Earned" ), 2, wszPlayerName, pchLocalizedAchievement );

						char szLocalized[128];
						g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalizedString, szLocalized, sizeof( szLocalized ) );

						hudChat->ChatPrintf( iPlayerIndex, CHAT_FILTER_SERVERMSG, "%s", szLocalized );
					}
				}
			}
		}
	}
#if defined( TF_CLIENT_DLL ) || defined( CSTRIKE_CLIENT_DLL )
	else if ( Q_strcmp( "item_found", eventname ) == 0 )
	{
		int iPlayerIndex = event->GetInt( "player" );
		//entityquality_t iItemQuality = event->GetInt( "quality" );
		int iMethod = event->GetInt( "method" );
		int iItemDef = event->GetInt( "itemdef" );
		C_BasePlayer *pPlayer = UTIL_PlayerByIndex( iPlayerIndex );
		const GameItemDefinition_t *pItemDefinition = dynamic_cast<const GameItemDefinition_t *>( GetItemSchema()->GetItemDefinition( iItemDef ) );

		if ( !pPlayer || !pItemDefinition )
			return;

		if ( g_PR )
		{
			wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
			g_pVGuiLocalize->ConvertANSIToUnicode( g_PR->GetPlayerName( iPlayerIndex ), wszPlayerName, sizeof( wszPlayerName ) );

			if ( iMethod < 0 || iMethod >= ARRAYSIZE( g_pszItemFoundMethodStrings ) )
			{
				iMethod = 0;
			}

			const char *pszLocString = g_pszItemFoundMethodStrings[iMethod];
			if ( pszLocString )
			{
				wchar_t wszItemFound[256];
				V_swprintf_safe( wszItemFound, L"%ls", g_pVGuiLocalize->Find( pszLocString ) );

				wchar_t *colorMarker = wcsstr( wszItemFound, L"::" );
				if ( colorMarker )
				{
					//const char *pszQualityColorString = EconQuality_GetColorString( (EEconItemQuality)iItemQuality );
					//if ( pszQualityColorString )
					//{
					//	hudChat->SetCustomColor( pszQualityColorString );
					//	*(colorMarker+1) = COLOR_CUSTOM;
					//}

					*(colorMarker+1) = wszItemFound[ 0 ];
				}

				// TODO: Update the localization strings to only have two format parameters since that's all we need.
				wchar_t wszLocalizedString[256];
				g_pVGuiLocalize->ConstructString( wszLocalizedString, sizeof( wszLocalizedString ), wszItemFound, 3, wszPlayerName, g_pVGuiLocalize->Find( pItemDefinition->GetItemBaseName() ), L"" );

				char szLocalized[256];
				g_pVGuiLocalize->ConvertUnicodeToANSI( wszLocalizedString, szLocalized, sizeof( szLocalized ) );

				hudChat->ChatPrintf( iPlayerIndex, CHAT_FILTER_SERVERMSG, "%s", szLocalized );
			}
		}		
	}
#endif
	else
	{
		DevMsg( 2, "Unhandled GameEvent in ClientModeShared::FireGameEvent - %s\n", event->GetName()  );
	}
}





//-----------------------------------------------------------------------------
// In-game VGUI context 
//-----------------------------------------------------------------------------
void ClientModeShared::ActivateInGameVGuiContext( vgui::Panel *pPanel )
{
	vgui::ivgui()->AssociatePanelWithContext( s_hVGuiContext, pPanel->GetVPanel() );
	vgui::ivgui()->ActivateContext( s_hVGuiContext );
}

void ClientModeShared::DeactivateInGameVGuiContext()
{
	vgui::ivgui()->ActivateContext( DEFAULT_VGUI_CONTEXT );
}

int ClientModeShared::GetSplitScreenPlayerSlot() const
{
	int nSplitScreenUserSlot = vgui::ipanel()->GetMessageContextId( m_pViewport->GetVPanel() );
	Assert( nSplitScreenUserSlot != -1 );
	return nSplitScreenUserSlot;
}
