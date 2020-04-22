//========= Copyright (c) 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "usermessages.h"
#include "shake.h"
#include "voice_gamemgr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void RegisterUserMessages()
{
	//copy/paste from hl2
	usermessages->Register( "Geiger", 1 );
	usermessages->Register( "Train", 1 );
	usermessages->Register( "HudText", -1 );
	usermessages->Register( "SayText", -1 );
	usermessages->Register( "SayText2", -1 );
	usermessages->Register( "TextMsg", -1 );
	usermessages->Register( "HudMsg", -1 );
	usermessages->Register( "ResetHUD", 1);		// called every respawn
	usermessages->Register( "GameTitle", 0 );
	usermessages->Register( "ItemPickup", -1 );
	usermessages->Register( "ShowMenu", -1 );
	usermessages->Register( "Shake", 13 );
	usermessages->Register( "Tilt", 22 );
	usermessages->Register( "Fade", 10 );
	usermessages->Register( "VGUIMenu", -1 );	// Show VGUI menu
	usermessages->Register( "Rumble", 3 );	// Send a rumble to a controller
	usermessages->Register( "Battery", 2 );
	usermessages->Register( "Damage", 18 );		// BUG: floats are sent for coords, no variable bitfields in hud & fixed size Msg
	usermessages->Register( "VoiceMask", VOICE_MAX_PLAYERS_DW*4 * 2 + 1 );
	usermessages->Register( "RequestState", 0 );
	usermessages->Register( "CloseCaption", -1 ); // Show a caption (by string id number)(duration in 10th of a second)
	usermessages->Register( "CloseCaptionDirect", -1 ); // Show a forced caption (by string id number)(duration in 10th of a second)
	usermessages->Register( "HintText", -1 );	// Displays hint text display
	usermessages->Register( "KeyHintText", -1 );	// Displays hint text display
	usermessages->Register( "SquadMemberDied", 0 );
	usermessages->Register( "AmmoDenied", 2 );
	usermessages->Register( "CreditsMsg", 1 );
	usermessages->Register( "LogoTimeMsg", 4 );
	usermessages->Register( "AchievementEvent", -1 );
	usermessages->Register( "UpdateJalopyRadar", -1 );
	usermessages->Register( "CurrentTimescale", 4 );	// Send one float for the new timescale
	usermessages->Register( "DesiredTimescale", 13 );	// Send timescale and some blending vars

	//new stuff for portal
	usermessages->Register( "CreditsPortalMsg", 1 );
	
#ifdef PORTAL2
	usermessages->Register( "InventoryFlash", sizeof( float ) + 1 );
	usermessages->Register( "IndicatorFlash", sizeof( float ) + 1 );
	usermessages->Register( "ControlHelperAnimate", 2 );
	usermessages->Register( "TakePhoto", sizeof( long ) + sizeof( uint8 ) );
	usermessages->Register( "Flash", sizeof( float ) + sizeof( Vector ) );
	usermessages->Register( "HudPingIndicator", sizeof( Vector ) );
	usermessages->Register( "OpenRadialMenu", -1 );
	usermessages->Register( "AddLocator", -1 );
	usermessages->Register( "MPMapCompleted", sizeof( char ) + sizeof( char ) );
	usermessages->Register( "MPMapIncomplete", sizeof( char ) + sizeof( char ) );
	usermessages->Register( "MPMapCompletedData", -1 );
	usermessages->Register( "MPTauntEarned", -1 );
	usermessages->Register( "MPTauntUnlocked", -1 );
	usermessages->Register( "MPTauntLocked", -1 );
	usermessages->Register( "MPAllTauntsLocked", -1 );

	// Portal effects
	usermessages->Register( "PortalFX_Surface", -1 );

	// Paint messages
	usermessages->Register( "PaintWorld", -1 );
	usermessages->Register( "PaintEntity", sizeof( long ) + sizeof( uint8 ) + sizeof( Vector ) );
	usermessages->Register( "ChangePaintColor", sizeof( long ) + sizeof( uint8 ) );
	usermessages->Register( "PaintBombExplode", sizeof( Vector ) + sizeof( uint8 ) + sizeof( uint8 ) + sizeof( BYTE ) );
	usermessages->Register( "RemoveAllPaint", 0 );
	usermessages->Register( "PaintAllSurfaces", sizeof( BYTE ) );
	usermessages->Register( "RemovePaint", sizeof( long ) );

	usermessages->Register( "StartSurvey", sizeof( long ) );
	usermessages->Register( "ApplyHitBoxDamageEffect", sizeof( long ) + sizeof( uint8 ) + sizeof( uint8 ) );
	usermessages->Register( "SetMixLayerTriggerFactor", -1 );
	usermessages->Register( "TransitionFade", sizeof( float ) );

	usermessages->Register( "ScoreboardTempUpdate", sizeof( long ) + sizeof( long ) );
	usermessages->Register( "ChallengeModeCheatSession", -1 );
	usermessages->Register( "ChallengeModeCloseAllUI", -1 );

	// FIXME: Bring this back for DLC2
	//usermessages->Register( "MPVSGameStart", sizeof( char ) );
	//usermessages->Register( "MPVSGameOver", sizeof( BYTE ) );
	//usermessages->Register( "MPVSRoundEnd", sizeof( BYTE ) );
#endif // PORTAL2
}
