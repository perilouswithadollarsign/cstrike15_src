//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include <string.h>
#include <stdio.h>
#include "voice_status.h"
#include "radio_status.h"
#include "c_playerresource.h"
#include "cliententitylist.h"
#include "c_baseplayer.h"
#include "materialsystem/imesh.h"
#include "view.h"
#include "materialsystem/imaterial.h"
#include "tier0/dbg.h"
#include "cdll_int.h"
#include "c_cs_player.h"
#include "menu.h" // for CHudMenu defs
#include "Scaleform/HUD/sfhud_radio.h"
#include "cs_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

// ---------------------------------------------------------------------- //
// The radio feedback manager for the client.
// ---------------------------------------------------------------------- //
static CRadioStatus s_RadioStatus;

//
//-----------------------------------------------------
//
ConVar radio_icons_use_particles( "radio_icons_use_particles", "1", FCVAR_NONE, "0 = classic style, 1 = particles" );

// Stuff for the Radio Menus
static void radio1_f( const CCommand &args );
static void radio2_f( const CCommand &args );
static void radio3_f( const CCommand &args );

static ConCommand radio1( "radio1", radio1_f, "Opens a radio menu" );
static ConCommand radio2( "radio2", radio2_f, "Opens a radio menu" );
static ConCommand radio3( "radio3", radio3_f, "Opens a radio menu" );
static int g_whichMenu = 0;

//
//--------------------------------------------------------------
//
// These methods will bring up the radio menus from the client side.
// They mimic the old server commands of the same name, which used
// to require a round-trip causing latency and unreliability in 
// menu responses.  Only 1 message is sent to the server now which
// includes both the menu name and the selected item.  The server
// is never informed that the menu has been displayed.
//
//--------------------------------------------------------------
//
void OpenRadioMenu( int index )
{
	if ( CSGameRules() && CSGameRules()->IsPlayingTraining() )
		return;

	C_CSPlayer *pLocalPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( pLocalPlayer && pLocalPlayer->GetObserverMode() > OBS_MODE_NONE )
		return;

	SFHudRadio* pRadio = GET_HUDELEMENT( SFHudRadio );
	if ( pRadio )
	{
		pRadio->ShowRadioGroup( index );
		g_whichMenu = index;
	}
}

static void radio1_f( const CCommand &args )
{
	OpenRadioMenu( 1 );
}

static void radio2_f( const CCommand &args )
{
	OpenRadioMenu( 2 );
}

static void radio3_f( const CCommand &args )
{
	OpenRadioMenu( 3 );
}

CON_COMMAND_F( menuselect, "menuselect", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	if ( args.ArgC() < 2 )
		return;

	if( g_whichMenu == 0 )
	{
		// if we didn't have a menu open, maybe a plugin did.  send it on to the server.
		const char *cmd = VarArgs( "menuselect %s", args[1] );
		engine->ServerCmd( cmd );
		return;
	}

	int whichEntry = atoi( args[ 1 ] );

	switch( g_whichMenu )
	{
		case 1: //RadioA
		{
			switch( whichEntry )
			{
			case 1: // coverme
				engine->ClientCmd( "coverme" );
				break;
			case 2: // takepoint
				engine->ClientCmd( "takepoint" );
				break;
			case 3: // holdpos
				engine->ClientCmd( "holdpos" );
				break;
			case 4: // regroup
				engine->ClientCmd( "regroup" );
				break;
			case 5: // followme
				engine->ClientCmd( "followme" );
				break;
			case 6: // takingfire
				engine->ClientCmd( "takingfire" );
				break;
			}
		}
		break;

		case 2: //RadioB
		{
			switch( whichEntry )
			{
			case 1: // go
				engine->ClientCmd( "go" );
				break;
			case 2: // fallback
				engine->ClientCmd( "fallback" );
				break;
			case 3: // sticktog
				engine->ClientCmd( "sticktog" );
				break;
			case 4: // getinpos
				engine->ClientCmd( "getinpos" );
				break;
			case 5: // stormfront
				engine->ClientCmd( "stormfront" );
				break;
			case 6: // report
				engine->ClientCmd( "report" );
				break;
			}
		}
		break;

		case 3: //RadioC
		{
			switch( whichEntry )
			{
			case 1: // roger
				engine->ClientCmd( "roger" );
				break;
			case 2: // enemyspot
				engine->ClientCmd( "enemyspot" );
				break;
			case 3: // needbackup
				engine->ClientCmd( "needbackup" );
				break;
			case 4: // sectorclear
				engine->ClientCmd( "sectorclear" );
				break;
			case 5: // inposition
				engine->ClientCmd( "inposition" );
				break;
			case 6: // reportingin
				engine->ClientCmd( "reportingin" );
				break;
			case 7: // getout
				engine->ClientCmd( "getout" );
				break;
			case 8: // negative
				engine->ClientCmd( "negative" );
				break;
			case 9: // enemydown
				engine->ClientCmd( "enemydown" );
				break;
			}
		}
		break;

		default:
			// if we didn't have a menu open, maybe a plugin did.  send it on to the server.
			const char *cmd = VarArgs( "menuselect %d", whichEntry );
			engine->ServerCmd( cmd );
			break;
	}

	// reset menu
	g_whichMenu = 0;
}

//
//-----------------------------------------------------
//

CRadioStatus* RadioManager()
{
	return &s_RadioStatus;
}


// ---------------------------------------------------------------------- //
// CRadioStatus.
// ---------------------------------------------------------------------- //

CRadioStatus::CRadioStatus()
{
	m_pHeadLabelMaterial = NULL;
	Q_memset(m_radioUntil, 0, sizeof(m_radioUntil));
	Q_memset(m_voiceUntil, 0, sizeof(m_voiceUntil));
}

bool CRadioStatus::Init()
{
	if ( !m_pHeadLabelMaterial )
	{
		m_pHeadLabelMaterial = materials->FindMaterial( "sprites/radio", TEXTURE_GROUP_VGUI );
	}

	if ( IsErrorMaterial( m_pHeadLabelMaterial ) && !g_bTextMode )
		return false;

	m_pHeadLabelMaterial->IncrementReferenceCount();

	return true;
}

void CRadioStatus::Shutdown()
{
	if ( m_pHeadLabelMaterial )
		m_pHeadLabelMaterial->DecrementReferenceCount();

	m_pHeadLabelMaterial = NULL;		
}

void CRadioStatus::LevelInitPostEntity()
{
	ExpireBotVoice( true );
	Q_memset(m_radioUntil, 0, sizeof(m_radioUntil));
	Q_memset(m_voiceUntil, 0, sizeof(m_voiceUntil));
}

void CRadioStatus::LevelShutdownPreEntity()
{
	ExpireBotVoice( true );
	Q_memset(m_radioUntil, 0, sizeof(m_radioUntil));
	Q_memset(m_voiceUntil, 0, sizeof(m_voiceUntil));
}

static float s_flHeadIconSize = 7;

void CRadioStatus::DrawHeadLabels()
{
	ExpireBotVoice();
	ConVarRef voice_head_icon_height( "voice_head_icon_height" );

	if( !m_pHeadLabelMaterial )
		return;

	for(int i=0; i < VOICE_MAX_PLAYERS; i++)
	{
		if ( m_radioUntil[i] < gpGlobals->curtime )
			continue;

		IClientNetworkable *pClient = cl_entitylist->GetClientEntity( i+1 );
		
		C_CSPlayer *pPlayer = dynamic_cast< C_CSPlayer* >( pClient );
		if( !pPlayer )
			continue;

		// Don't show an icon if the player is not in our PVS.
		if ( !pClient || pClient->IsDormant() )
		{
			if ( radio_icons_use_particles.GetBool() )
			{
				pPlayer->UpdateRadioHeadIcon( false );
			}

			continue;
		}

		if( !UTIL_PlayerByIndex( i+1 ) )
		{
			if ( radio_icons_use_particles.GetBool() )
			{
				pPlayer->UpdateRadioHeadIcon( false );
			}

			continue;
		}

		// Don't show an icon for dead or spectating players (ie: invisible entities).
		if( pPlayer->IsPlayerDead() )
		{
			if ( radio_icons_use_particles.GetBool() )
			{
				pPlayer->UpdateRadioHeadIcon( false );
			}

			continue;
		}

		// Don't show an icon for players we can't hear
		if ( !GetClientVoiceMgr()->IsPlayerAudible( i+1 ) )
			continue;

		if ( radio_icons_use_particles.GetBool() )
		{
			pPlayer->UpdateRadioHeadIcon( true );
			return;
		}

		// Place it above his head.
		Vector vOrigin = pPlayer->EyePosition();
		vOrigin.z += GetClientVoiceMgr()->GetHeadLabelOffset();
		
		// Place this above the speaking bubble if he's currently speaking.
		if ( GetClientVoiceMgr()->IsPlayerSpeaking( i+1 ) )
		{
			vOrigin.z += GetClientVoiceMgr()->GetHeadLabelOffset();
		}
		
		// Align it so it never points up or down.
		Vector vUp( 0, 0, 1 );
		Vector vRight = CurrentViewRight();
		if ( fabs( vRight.z ) > 0.95 )	// don't draw it edge-on
			continue;

		vRight.z = 0;
		VectorNormalize( vRight );


		float flSize = s_flHeadIconSize;

		CMatRenderContextPtr pRenderContext( materials );

		pRenderContext->Bind( m_pHeadLabelMaterial );
		IMesh *pMesh = pRenderContext->GetDynamicMesh();
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,0,0 );
		meshBuilder.Position3fv( (vOrigin + (vRight * -flSize) + (vUp * flSize)).Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,1,0 );
		meshBuilder.Position3fv( (vOrigin + (vRight * flSize) + (vUp * flSize)).Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,1,1 );
		meshBuilder.Position3fv( (vOrigin + (vRight * flSize) + (vUp * -flSize)).Base() );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,0,1 );
		meshBuilder.Position3fv( (vOrigin + (vRight * -flSize) + (vUp * -flSize)).Base() );
		meshBuilder.AdvanceVertex();
		meshBuilder.End();
		pMesh->Draw();
	}
}


void CRadioStatus::UpdateRadioStatus(int entindex, float duration)
{
	if(entindex > 0 && entindex <= VOICE_MAX_PLAYERS)
	{
		int iClient = entindex - 1;
		if(iClient < 0)
			return;

		m_radioUntil[iClient] = gpGlobals->curtime + duration;
	}
}


void CRadioStatus::UpdateVoiceStatus(int entindex, float duration)
{
	if(entindex > 0 && entindex <= VOICE_MAX_PLAYERS)
	{
		int iClient = entindex - 1;
		if(iClient < 0)
			return;

		m_voiceUntil[iClient] = gpGlobals->curtime + duration;
		GetClientVoiceMgr()->UpdateSpeakerStatus( entindex, -1, true );
	}
}

void CRadioStatus::ExpireBotVoice( bool force )
{
	for(int i=0; i < VOICE_MAX_PLAYERS; i++)
	{
		if ( m_voiceUntil[i] > 0.0f )
		{
			bool expire = force;

			C_CSPlayer *player = static_cast<C_CSPlayer*>( cl_entitylist->GetEnt(i+1) );
			if ( !player )
			{
				// player left the game
				expire = true;
			}
			else if ( m_voiceUntil[i] < gpGlobals->curtime )
			{
				// player is done speaking
				expire = true;
			}

			if ( expire )
			{
				m_voiceUntil[i] = 0.0f;
				GetClientVoiceMgr()->UpdateSpeakerStatus( i+1, -1, false );
			}
		}
	}
}

