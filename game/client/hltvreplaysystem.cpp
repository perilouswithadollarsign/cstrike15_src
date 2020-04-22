//======= Copyright (c) Valve Corporation, All rights reserved. ======
#include "cbase.h"
#include "hltvreplaysystem.h"
#include "hltvcamera.h"
#include "cs_gamerules.h"
#include "iviewrender.h"
#include "engine/IEngineSound.h"
#include "netmessages.h"
#include "cstrike15/c_cs_player.h"
#include "ihltv.h"

ConVar snd_deathcam_replay_mix( "snd_deathcam_replay_mix", "0", 0, "When set to non-0, client switches to DeathCam_Replay_Mix mixgroup during deathcam replay" );
ConVar spec_replay_review_sound( "spec_replay_review_sound", "1", FCVAR_CLIENTDLL, "When set to non-0, a sound effect is played during Killer Replay" );
ConVar spec_replay_rate_slowdown( "spec_replay_rate_slowdown", "1", FCVAR_CLIENTDLL, "The part of Killer Replay right before death is played at this rate" );
ConVar spec_replay_rate_slowdown_length( "spec_replay_rate_slowdown_length", "0.5", FCVAR_CLIENTDLL, "The part of Killer Replay right before death is played at this rate" );
ConVar spec_replay_fadein( "spec_replay_fadein", "0.75", FCVAR_CLIENTDLL, "Amount of time in seconds it takes to visually fade into replay, or into real-time after replay" ); // Original tuning: 1.0; MattWood 12/3/15: 0.75
ConVar spec_replay_fadeout( "spec_replay_fadeout", "0.5", FCVAR_CLIENTDLL, "Amount of time in seconds it takes to visually fade out of replay, or out of real-time before replay" );
ConVar spec_replay_sound_fadein( "spec_replay_sound_fadein", "1", FCVAR_CLIENTDLL, "Amount of time in seconds it takes to fade in the audio before or after replay" );
ConVar spec_replay_sound_fadeout( "spec_replay_sound_fadeout", "1.5", FCVAR_CLIENTDLL, "Amount of time in seconds it takes to fade out the audio before or after replay" );
ConVar spec_replay_cache_ragdolls( "spec_replay_cache_ragdolls", "1", FCVAR_CLIENTDLL, "when set to 0, ragdolls will settle dynamically before and after Killer Replay" );
ConVar spec_replay_others_experimental( "spec_replay_others_experimental", "0", FCVAR_CLIENTDLL, "Replay the last death of the round, if possible. Disabled on official servers by default. Experimental." );
ConVar spec_replay_autostart( "spec_replay_autostart", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Auto-start Killer Replay when available" ); // in game/csgo/scripts/game_options.txt
ConVar spec_replay_autostart_delay( "spec_replay_autostart_delay", "1.5", FCVAR_CLIENTDLL, "Time in freeze panel before switching to Killer Replay automatically" ); // original tuning: 2.4; MattWood 12/3/15: 1.5
ConVar spec_replay_victim_pov( "spec_replay_victim_pov", "0", FCVAR_CLIENTDLL, "Killer Replay - replay from victim's point of view (1); the default is killer's (0). Experimental." );
CHltvReplaySystem g_HltvReplaySystem;
extern void CS_FreezePanel_OnHltvReplayButtonStateChanged();

CHltvReplaySystem::CHltvReplaySystem()
{
	m_nHltvReplayDelay = 0;
	m_nHltvReplayStopAt = 0;
	m_nHltvReplayStartAt = 0;
	m_flHltvLastRequestRealTime = 0;
	m_bWaitingForHltvReplayTick = false;
	m_flStartedWaitingForHltvReplayTickRealTime = 0;
	m_bDemoPlayback = false;
	m_bListeningForGameEvents = false;
	m_nDemoPlaybackStartAt = 0;
	m_nDemoPlaybackStopAt = 0;
	m_nCurrentPlaybackTick = 0;
	m_flCurrentPlaybackTime = 0.0f;
	m_nDemoPlaybackXuid = 0;
	m_nSteamSelfAccountId = 0;
	m_flReplayVideoFadeAmount = 0.0f;
	m_flReplaySoundFadeAmount = 0.0f;
	m_nExperimentalEvents = 0;
	m_flFadeinStartRealTime = -1000;
	m_flFadeoutEndTime = -1000;
	m_DelayedReplay.Reset();
	m_nHltvReplayBeginTick = -1;
	m_nHltvReplayPrimaryTarget = 0;
	m_bHltvReplayButtonTimedOut = false;
}

CHltvReplaySystem::~CHltvReplaySystem()
{

}

int CL_GetHltvReplayDelay() { return g_HltvReplaySystem.GetHltvReplayDelay(); }

void CHltvReplaySystem::EmitTimeJump()
{
	GetHud().OnTimeJump();
#if defined( CSTRIKE15 )
	C_BasePlayer::OnTimeJumpAllPlayers();
#endif
	//GetHud().UpdateHud( true ); // may call SFUniqueAlerts::ShowHltvReplayAlertPanel
	//GetHud().ProcessInput( true );
	//GetHud().OnTimeJump();
}

void CHltvReplaySystem::StopHltvReplay()
{
	if ( m_nHltvReplayDelay )
	{
		CSVCMsg_HltvReplay msg;
		OnHltvReplay( msg );
	}
}


void CHltvReplaySystem::OnHltvReplay( const CSVCMsg_HltvReplay  &msg )
{
	int nNewReplayDelay = msg.delay();
	DevMsg( "%.2f OnHltvReplay %s\n", gpGlobals->curtime, nNewReplayDelay ? "START" : "END" );

	extern bool g_bForceCLPredictOff;
	g_bForceCLPredictOff = ( nNewReplayDelay != 0 );

	static int s_nReplayLayerIndex = engine->GetMixLayerIndex( "ReplayLayer" );

	if ( m_nHltvReplayDelay )
	{
		if ( !nNewReplayDelay )
		{
			// we're about to go out of HLTV replay. Current time is delayed replay time.
			if ( C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer() )
			{
				C_BaseEntity::StopSound( -1, "Deathcam.Review_Start" );
				if ( spec_replay_review_sound.GetBool() )
					pPlayer->EmitSound( "Deathcam.Review_End" );
			}
			engine->SetMixLayerLevel( s_nReplayLayerIndex, 0.0f );
			IGameEvent *pEvent = gameeventmanager->CreateEvent( "hide_freezepanel" );
			if ( pEvent )
			{
				gameeventmanager->FireEventClientSide( pEvent );
			}
			if ( snd_deathcam_replay_mix.GetBool() )
			{
				ConVar *pSoundmixer = ( ConVar * )cvar->FindVar( "snd_soundmixer" );
				pSoundmixer->Revert();
			}
		}
		StopFadeout();// Msg( "%.2f Fadeout not stopped (%d,%d,%.2f)\n", gpGlobals->curtime, m_nHltvReplayDelay, nNewReplayDelay, m_flFadeoutEndTime );
	}
	else
	{
		if ( nNewReplayDelay )
		{
			m_DelayedReplay.Invalidate(); // we don't do delayed replay if we are about to do the replay
			CacheRagdollBones();
			// we're about to go into HLTV replay. Current time is real time.
			HLTVCamera()->SetPrimaryTarget( m_nHltvReplayPrimaryTarget = msg.primary_target() );
			HLTVCamera()->SetMode( OBS_MODE_IN_EYE );
			GetHud().ResetHUD();
			/*IGameEvent *pEvent = gameeventmanager->CreateEvent( "hide_freezepanel" );
			if ( pEvent )
			{
				gameeventmanager->FireEventClientSide( pEvent );
			}*/
			if ( spec_replay_review_sound.GetBool() )
			{
				if ( C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer() )
				{
					EmitSound_t params;
					params.m_pSoundName = "Deathcam.Review_Start";
					params.m_bWarnOnDirectWaveReference = true;
					params.m_hSoundScriptHash = soundemitterbase->HashSoundName( params.m_pSoundName );
					CLocalPlayerFilter filter;
					C_BaseEntity::EmitSound( filter, -1, params );
				}
			}
			if ( snd_deathcam_replay_mix.GetBool() )
			{
				ConVar *pSoundmixer = ( ConVar * )cvar->FindVar( "snd_soundmixer" );
				pSoundmixer->SetValue( "DeathCam_Replay_Mix" );
			}
			engine->SetMixLayerLevel( s_nReplayLayerIndex, 1.0f );
			if ( int nSlowdownRatio = msg.replay_slowdown_rate() )
			{
				m_flHltvReplaySlowdownRate = 1.0f / nSlowdownRatio;
				m_nHltvReplaySlowdownBegin = msg.replay_slowdown_begin();
				m_nHltvReplaySlowdownEnd = msg.replay_slowdown_end();
			}
			else
			{
				m_nHltvReplaySlowdownBegin = 0;
				m_nHltvReplaySlowdownEnd = 0;
				m_flHltvReplaySlowdownRate = 1.0f;
			}

			m_nExperimentalEvents |= EE_REPLAY_STARTED;
			// currently, Hltv Replay Active (m_nHltvReplayDelay!=0)
			StartFadeout( msg.replay_stop_at() * gpGlobals->interval_per_tick, spec_replay_fadeout.GetFloat() );
		}
		else
		{
			// Hltv replay is NOT active. Requested NO Hltv replay . There's nothing to do.
			StopFadeout(); //Msg( "%.2f Fadeout not stopped-2 (%d,%d,%.2f)\n", gpGlobals->curtime, m_nHltvReplayDelay, nNewReplayDelay, m_flFadeoutEndTime );
		}
	}
	StartFadein( gpGlobals->realtime );
	m_flHltvLastRequestRealTime = gpGlobals->realtime;
	m_bWaitingForHltvReplayTick = true;
	m_flStartedWaitingForHltvReplayTickRealTime = gpGlobals->realtime;
	m_nHltvReplayBeginTick = gpGlobals->tickcount; 
	debugoverlay->ClearAllOverlays();

	m_nHltvReplayDelay = nNewReplayDelay;
	//enginesound->SetReplayMusicGain( nNewReplayDelay ? 0.0f : 1.0f ); 

	m_nHltvReplayStopAt = msg.replay_stop_at(); // we'll stop at this current tick, more or less; we'll jump back in time and stop here
	m_nHltvReplayStartAt = msg.replay_start_at() - nNewReplayDelay;
	// I'd like to call ParticleMgr()->RemoveAllEffects(); but there are still some entities referencing the particles in question
	view->FreezeFrame( 0.0f );
	Update();

	EmitTimeJump();
}


void CHltvReplaySystem::OnHltvReplayTick()
{
	Assert( m_bWaitingForHltvReplayTick );
	m_bWaitingForHltvReplayTick = false;
	m_nHltvReplayBeginTick = gpGlobals->tickcount;
	Update();
	CS_FreezePanel_OnHltvReplayButtonStateChanged();
}


float ReplayFade( float flCurTime, int nStartAt, int nStopAt, float flFadeIn, float flFadeOut )
{
	if ( flFadeIn > 0.001f )
	{
		float flTimeIn = flCurTime - nStartAt * gpGlobals->interval_per_tick;
		if ( flTimeIn < flFadeIn )
		{
			return 1.0f - Max( 0.0f, flTimeIn / flFadeIn );
		}
	}

	if ( flFadeOut > 0.001f )
	{
		float flTimeOut = nStopAt * gpGlobals->interval_per_tick - flCurTime;
		if ( flTimeOut < flFadeOut )
		{
			return 1.0f - Max( 0.0f, flTimeOut / flFadeOut );
		}
	}
	return 0.0f;
}

void CHltvReplaySystem::Update()
{
	float flCurTime = gpGlobals->curtime;
	float flPrevReplaySoundFadeAmount = m_flReplaySoundFadeAmount;
	//Assert( enginesound->GetReplaySoundFade() == m_flReplaySoundFadeAmount );

	m_flReplayVideoFadeAmount = m_flReplaySoundFadeAmount = 0.0f; // there are no replay post effects going on, by default

	if ( !m_nHltvReplayDelay && gpGlobals->tickcount > m_nHltvReplayBeginTick + 5 )
	{
		PurgeRagdollBoneCache();
	}

	if ( m_bWaitingForHltvReplayTick )
	{
		// we're in this state when server let us know that Hltv replay is either about to stop or start, but we haven't received the frame update yet and the new timeline hasn't started yet
		// whether we're going in or out of replay, let's darken the screen. It's possible that we'll miss that message, though. If we disconnect or reconnect, we'll want to reset this flag
		m_flReplayVideoFadeAmount = m_flReplaySoundFadeAmount = 1.0f; // fade everything out completely		

		if ( gpGlobals->realtime - m_flStartedWaitingForHltvReplayTickRealTime > 0.75f )
		{
			Msg( "Killer replay stuck waiting for tick message for %.2fs, requesting update\n", gpGlobals->realtime - m_flHltvLastRequestRealTime );
			m_nExperimentalEvents |= EE_REPLAY_STUCK;

			m_flStartedWaitingForHltvReplayTickRealTime = gpGlobals->realtime;
			CCLCMsg_HltvReplay_t msgReplay;
			msgReplay.set_request( REPLAY_EVENT_STUCK_NEED_FULL_UPDATE ); // request full update
			engine->SendMessageToServer( &msgReplay );
		}
	}
	else if ( m_bDemoPlayback )
	{
		if ( m_nDemoPlaybackStartAt < m_nDemoPlaybackStopAt )
		{
			m_flReplayVideoFadeAmount = ReplayFade( m_nCurrentPlaybackTick * gpGlobals->interval_per_tick + flCurTime - m_flCurrentPlaybackTime, m_nDemoPlaybackStartAt, m_nDemoPlaybackStopAt - 1, spec_replay_fadein.GetFloat(), spec_replay_fadeout.GetFloat() );
			m_flReplaySoundFadeAmount = ReplayFade( m_nCurrentPlaybackTick * gpGlobals->interval_per_tick + flCurTime - m_flCurrentPlaybackTime, m_nDemoPlaybackStartAt, m_nDemoPlaybackStopAt - 1, spec_replay_sound_fadein.GetFloat(), spec_replay_sound_fadeout.GetFloat() );
		}
	}
	else
	{
		if ( m_flFadeinStartRealTime > 0.0f )
		{
			float flTimeIn = gpGlobals->realtime - m_flFadeinStartRealTime;
			bool bFadeInStop = true;

			if ( flTimeIn < m_flFadeinDuration )
			{
				m_flReplayVideoFadeAmount = Max( m_flReplayVideoFadeAmount, 1.0f - Max( 0.0f, flTimeIn / Max( m_flFadeinDuration, 0.01f ) ) );
				bFadeInStop = false;
			}
			

			if ( flTimeIn < spec_replay_sound_fadein.GetFloat() )
			{
				m_flReplaySoundFadeAmount = Max( m_flReplaySoundFadeAmount, 1.0f - Max( 0.0f, flTimeIn / Max( spec_replay_sound_fadein.GetFloat(), 0.01f ) ) );
				bFadeInStop = false;
			}

			if ( bFadeInStop )
				m_flFadeinStartRealTime = -1000;
		}

		if ( m_flFadeoutEndTime > 0.0f )
		{
			float flTimeLeft = m_flFadeoutEndTime - gpGlobals->curtime;
			if ( flTimeLeft < m_flFadeoutDuration )
			{
				m_flReplayVideoFadeAmount = Max( m_flReplayVideoFadeAmount, 1 - Max( 0.0f, flTimeLeft / Max( m_flFadeoutDuration, 0.01f ) ) );
				// Msg( "%.2f +%.2f fade %.2f\n", gpGlobals->curtime, flTimeLeft, m_flReplayVideoFadeAmount ); // replayfade
			}
			if ( flTimeLeft < spec_replay_sound_fadeout.GetFloat() )
			{
				m_flReplaySoundFadeAmount = Max( m_flReplaySoundFadeAmount, 1 - Max( 0.0f, flTimeLeft / Max( spec_replay_sound_fadeout.GetFloat(), 0.01f ) ) );
			}

			if ( flTimeLeft < -1.0f )
			{
				// we're in full fade-out for over 1 second. All fadeouts must be very short to avoid disorienting the player. This may only happen when something went wrong: disconnect, dropped connection, server failed to reply to a request... Just stop it
				DevMsg( "%.2f Aborting fadeout\n", gpGlobals->curtime );
				m_flFadeoutEndTime = -1000;
				m_flFadeinStartRealTime = gpGlobals->realtime;
				m_flFadeinDuration = .3f; // fade in quickly
			}
		}
	}

	if ( m_DelayedReplay.IsValid() )
	{
		if ( gpGlobals->curtime >= m_DelayedReplay.flTime )
		{
			if ( IsHltvReplayFeatureEnabled() ) // we checked the button enabled when we set up this delayed replay
				RequestHltvReplay( m_DelayedReplay );
			m_DelayedReplay.Invalidate();
		}
	}

	bool bLocalPlayerChanged = m_LocalPlayer.Update();
	bool bHltvReplayButtonTimeOutChanged = UpdateHltvReplayButtonTimeOutState();
	if ( bLocalPlayerChanged || bHltvReplayButtonTimeOutChanged )
	{
		CS_FreezePanel_OnHltvReplayButtonStateChanged();
		if ( m_LocalPlayer.m_bLastSeenAlive && !m_nHltvReplayDelay ) // || m_bHltvReplayButtonTimedOut would cancel pending replay start, but we already don't allow replays right before time-out events
			CancelDelayedHltvReplay(); // we can't have a replay if we see the player alive in real-time timeline
	}
	if ( m_nHltvReplayDelay )
	{
		// set the primary target
		int nPrevTarget = HLTVCamera()->GetCurrentTargetEntindex(), nNewTarget = m_nHltvReplayPrimaryTarget;
		if ( C_BasePlayer *pPlayer = UTIL_PlayerByIndex( nNewTarget ) )
		{
			if ( !pPlayer->IsAlive() )
			{
				if ( C_BaseEntity *pKillerSpectating = pPlayer->GetObserverTarget() )
				{
					nNewTarget = pKillerSpectating->entindex();
				}
			}
			if ( nNewTarget != nPrevTarget )
			{
				HLTVCamera()->SetPrimaryTarget( nNewTarget );
			}
		}

		if ( !m_bWaitingForHltvReplayTick && gpGlobals->tickcount > m_nHltvReplayStopAt + 32 )
		{
			// time to ask server to stop replay, again
			RequestCancelHltvReplay( true );
			// .. and ask the server again in 32 ticks, if this packet is lost, too
			m_nHltvReplayStopAt = gpGlobals->tickcount;
		}
	}

	if ( flPrevReplaySoundFadeAmount != m_flReplaySoundFadeAmount )
	{
		SetReplaySoundMixLayer( m_flReplaySoundFadeAmount );
	}
}




void CHltvReplaySystem::PurgeRagdollBoneCache()
{
	if ( int nCount = m_mapCachedRagdollBones.Count() )
	{
		// destroy all unclaimed bones
		FOR_EACH_HASHTABLE( m_mapCachedRagdollBones, it )
		{
			FreeCachedRagdollBones( m_mapCachedRagdollBones[ it ] );
		}
		m_mapCachedRagdollBones.Purge();
		DevMsg( "%d ragdoll states purged\n", nCount );
	}
}


bool CHltvReplaySystem::IsFadeoutActive()
{
	return m_flFadeoutEndTime > 0.0f && gpGlobals->curtime > m_flFadeoutEndTime - m_flFadeoutDuration; // fadeout is requested and has already started
}

bool CHltvReplaySystem::IsFadeoutFinished()
{
	return gpGlobals->curtime >= m_flFadeoutEndTime;
}


void CHltvReplaySystem::OnLevelShutdown()
{
	m_bWaitingForHltvReplayTick = false;
	StopFades();
	StopHltvReplay();
	Update();
}

void CHltvReplaySystem::StopFadeout()
{
	if ( m_flFadeoutEndTime > 0 )
		DevMsg( "%.2f Stopping fadeout\n", gpGlobals->curtime );

	SetReplaySoundMixLayer( 0.0f );
	m_flFadeoutEndTime = -1000;
}

//ConVar spec_replay_sound_fade( "spec_replay_sound_fade", "2", /*FCVAR_CLIENTDLL | FCVAR_RELEASE*/ 0, "0 : fade everything out at replay transition; 1 : use ReplayBlinkLayer for the replay sound transition; 2: use both ReplayLayer and ReplayBlinkLayer for replay sound transition" );

void CHltvReplaySystem::SetReplaySoundMixLayer( float flFade )
{
	static int s_nReplayBlinkLayerIndex = engine->GetMixLayerIndex( "ReplayBlinkLayer" );
	engine->SetMixLayerLevel( s_nReplayBlinkLayerIndex, flFade );
	//enginesound->SetReplaySoundFade( m_flReplaySoundFadeAmount );
}


void CHltvReplaySystem::StartFadeout( float flTimeEnd, float flDuration )
{
	DevMsg( "%.2f Starting fadeout %.2f->%.2f\n", gpGlobals->curtime, flTimeEnd, flDuration );
	m_flFadeoutEndTime = flTimeEnd;
	m_flFadeoutDuration = flDuration;
}

void CHltvReplaySystem::StartFadein( float flStartRealTime )
{
	m_flFadeinStartRealTime = flStartRealTime;
	m_flFadeinDuration = spec_replay_fadein.GetFloat();
}

void CHltvReplaySystem::StopFades()
{
	StopFadeout();
	m_flFadeinStartRealTime = -1000;
}

void CHltvReplaySystem::OnLevelInit()
{
	m_DelayedReplay.Reset();
	m_bWaitingForHltvReplayTick = false;
	m_flHltvLastRequestRealTime = 0;
	m_flCurrentPlaybackTime = 0;
	StopFades();
	Assert( !m_nHltvReplayDelay ); // we MAY be in demo playback
	if ( !m_bListeningForGameEvents )
	{
		ListenForGameEvent( "round_start" );
		ListenForGameEvent( "bot_takeover" );
		ListenForGameEvent( "player_death" );
		m_bListeningForGameEvents = true; // now we're listening
	}
}

void CHltvReplaySystem::FireGameEvent( IGameEvent *event )
{
	const char *pEventName = event->GetName();
	if ( !V_strcmp( pEventName, "round_start" ) )
	{
		m_nExperimentalEvents = 0;
		CancelDelayedHltvReplay();
	}
	else if ( m_DelayedReplay.IsValid() && !V_strcmp( pEventName, "bot_takeover" ) )
	{
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		int nEventUserID = event->GetInt( "userid", -1 );

		if ( pLocalPlayer->GetObserverMode() != OBS_MODE_NONE && pLocalPlayer->GetUserID() == nEventUserID )
		{
			int nBotId = event->GetInt( "botid" );
			C_BasePlayer *pBot = UTIL_PlayerByUserId( nBotId );
			DevMsg( "%.2f Aborting hltv replay scheduled @%.2f because %s (%d) took over bot %s (%d)\n", gpGlobals->curtime, m_DelayedReplay.flTime, pLocalPlayer->GetPlayerName(), nEventUserID, pBot ? pBot->GetPlayerName() : "(null)", nBotId );
			CancelDelayedHltvReplay();
		}
	}
	else if ( !V_strcmp( pEventName, "player_death" ) )
	{
		OnPlayerDeath( event );
	}
}



float CHltvReplaySystem::GetHltvReplayDistortAmount()const
{
	return m_nHltvReplayDelay ? 1.0f : 0.0f; // no distortion in highlights or lowlights, just the fade in/out
}

void CHltvReplaySystem::OnDemoPlayback( bool bPlaying )
{ 
	m_bDemoPlayback = bPlaying;
	if ( !bPlaying )
	{
		if ( !m_nHltvReplayDelay )
		{
			StopFades();
		}
		m_nDemoPlaybackXuid = 0;
		m_bDemoPlaybackLowLights = false;
	}
	// by default, we don't know if we need fade in demo playback
	m_nDemoPlaybackStartAt = 0;
	m_nDemoPlaybackStopAt = 0;
	m_nCurrentPlaybackTick = 0;
	m_flCurrentPlaybackTime = gpGlobals->curtime;
	EmitTimeJump();
}


void CHltvReplaySystem::SetDemoPlaybackHighlightXuid( uint64 xuid, bool bLowlights )
{
	m_nDemoPlaybackXuid = xuid;
	m_bDemoPlaybackLowLights = bLowlights;
	if ( !m_nSteamSelfAccountId && steamapicontext )
	{
		if ( ISteamUser* pSteamUser = steamapicontext->SteamUser() )
		{
			m_nSteamSelfAccountId = pSteamUser->GetSteamID().GetAccountID();
		}
	}
}


bool CHltvReplaySystem::IsDemoPlaybackXuidOther()const
{
	return m_nSteamSelfAccountId != uint32( m_nDemoPlaybackXuid );
}


C_BasePlayer *CHltvReplaySystem::GetDemoPlaybackPlayer()
{
	if ( C_BasePlayer *pPlayer = m_DemoPlaybackPlayer.Get() )
	{
		CSteamID steamId;
		if ( pPlayer->GetSteamID(&steamId) )
		{
			if ( steamId.GetAccountID() == uint32( m_nDemoPlaybackXuid ) )
				return pPlayer;
		}
	}
	m_DemoPlaybackPlayer.Term();

	for ( int i = 1; i <= MAX_PLAYERS; ++i )
	{
		if ( C_BasePlayer *pPlayer = UTIL_PlayerByIndex( i ) )
		{
			CSteamID steamId;
			if ( pPlayer->GetSteamID( &steamId ) )
			{
				if ( steamId.GetAccountID() == uint32( m_nDemoPlaybackXuid ) )
				{
					m_DemoPlaybackPlayer.Set( pPlayer );
					return pPlayer;
				}
			}
		}
	}
	return NULL;
}


void CHltvReplaySystem::SetDemoPlaybackFadeBrackets( int nCurrentPlaybackTick, int nStartAt, int nStopAt )
{
	Assert( m_bDemoPlayback );
	if ( nCurrentPlaybackTick + 320 < nStartAt && nStartAt != m_nDemoPlaybackStartAt )
	{
		// we're starting a jump forward in time. The jump is long (over 320 ticks), so it may take some time, and we don't want the sounds currently in the queue to randomly play
		enginesound->StopAllSounds( true );
	}

	m_nDemoPlaybackStartAt = nStartAt;
	m_nDemoPlaybackStopAt = nStopAt;
	m_nCurrentPlaybackTick = nCurrentPlaybackTick;
	m_flCurrentPlaybackTime = gpGlobals->curtime;
}




bool CHltvReplaySystem::IsHltvReplayFeatureEnabled()
{
#if HLTV_REPLAY_ENABLED
	if ( g_bEngineIsHLTV )
		return false; // no replay during replay or GOTV

	static ConVarRef spec_replay_enable( "spec_replay_enable" );
	return spec_replay_enable.GetBool();
#else
	return false;
#endif
}

bool CHltvReplaySystem::IsHltvReplayButtonEnabled()
{
	if ( !IsHltvReplayFeatureEnabled() )
		return false;

	static ConVarRef spec_replay_rate_limit( "spec_replay_rate_limit" );

	if ( m_LocalPlayer.m_bLastSeenAlive ) // must be dead to allow replay
		return false;

	// Note: player death time may not be networked yet, so current pPlayer->GetDeathTime() may not be up to date

	if ( m_bHltvReplayButtonTimedOut )
		return false;

	if ( gpGlobals->curtime > m_DelayedReplay.flEventTime + GetReplayMessageTime() )
		return false;// did we die too long ago ? Then we can't replay

	if ( gpGlobals->realtime < m_flHltvLastRequestRealTime + spec_replay_rate_limit.GetFloat() ) // rate-limit mashing the Replay button
		return false;

	return true;
}




void CHltvReplaySystem::CancelDelayedHltvReplay()
{
	StopFadeout(); // fadeins are fine, leave them be
	// abort the pending death replay, stop all fades because the user took over a bot
	if ( m_DelayedReplay.IsValid() )
	{
		m_DelayedReplay.Stop();
		CS_FreezePanel_OnHltvReplayButtonStateChanged();
	}
}


void CHltvReplaySystem::RequestHltvReplayDeath()
{
	ReplayParams_t rp;
	rp.nRequest = REPLAY_EVENT_DEATH;
	RequestHltvReplay( rp );
}

void CHltvReplaySystem::RequestHltvReplay( const ReplayParams_t &replay )
{
	CCLCMsg_HltvReplay_t msgReplay;
	msgReplay.set_request( replay.nRequest );
	float flSlowdown = spec_replay_rate_slowdown.GetFloat();
	if ( flSlowdown != 1.0f )
	{
		float flSlowdownLength = spec_replay_rate_slowdown_length.GetFloat();
		if ( flSlowdownLength > 0.01f && flSlowdownLength < 5.0f )
		{
			msgReplay.set_slowdown_length( flSlowdownLength );
			msgReplay.set_slowdown_rate( flSlowdown );
		}
	}
	if ( replay.nRequest != REPLAY_EVENT_DEATH )
	{
		msgReplay.set_primary_target_ent_index( replay.nPrimaryTargetEntIndex );
		msgReplay.set_event_time( replay.flEventTime );
	}

	// if we want to always see from victim's perspective, always set the primary target to victim
	if ( spec_replay_victim_pov.GetBool() && replay.nPrimaryVictimEntIndex > 0 )
	{
		msgReplay.set_primary_target_ent_index( replay.nPrimaryVictimEntIndex );
	}

	engine->SendMessageToServer( &msgReplay );

	StartFadein( gpGlobals->realtime + 1.0f ); // if we don't get hltv replay message in 1 second, it means we couldn't start replay normally, and it's time to fade out
	StopFadeout();

	m_flHltvLastRequestRealTime = gpGlobals->realtime;
	m_nExperimentalEvents |= EE_REPLAY_REQUESTED;
}

void CHltvReplaySystem::RequestCancelHltvReplay( bool bSuppressFadeout )
{
	if ( GetHltvReplayDelay() )
	{
		CCLCMsg_HltvReplay_t msgReplay;
		msgReplay.set_request( 0 ); // cancel replay
		engine->SendMessageToServer( &msgReplay );
		if ( bSuppressFadeout )
		{
			StopFadeout();
		}
		else
		{
			if ( !IsFadeoutActive() )
			{// if there's no other fadeout happening now, start fading out for cancel; pretend we're already .1 seconde into fadeout
				StartFadeout( gpGlobals->curtime + m_flFadeoutDuration, 0.3f );// approximately how much it'll take for server to come back with the cancellation
			}
		}
		m_nExperimentalEvents |= EE_REPLAY_CANCELLED;
		DevMsg( "%.2f Replay Cancel requested (%d,%d..%d)\n", gpGlobals->curtime, gpGlobals->tickcount, m_nHltvReplayStartAt, m_nHltvReplayStopAt );
	}
	else
	{
		DevMsg( "%.2f Replay Cancel request failed, not in replay now\n", gpGlobals->curtime );
	}
}



bool IsPlayerTeamDead( int nPlayerEntIndex )
{
	if ( C_BasePlayer* pPrincipalPlayer = UTIL_PlayerByIndex( nPlayerEntIndex ) )
	{
		int nTeam = pPrincipalPlayer->GetTeamNumber();
		for ( int i = 1; i <= MAX_PLAYERS; ++i )
		{
			if ( i != nPlayerEntIndex )
			{
				if ( CBasePlayer *pPlayer = UTIL_PlayerByIndex( i ) )
				{
					Assert( pPlayer != pPrincipalPlayer );
					if ( pPlayer->GetTeamNumber() == nTeam )
					{
						if ( pPlayer->IsAlive() )
							return false;
					}
				}
			}
		}
		return true;
	}
	else
		return false; // something's wrong - can't find a team of a non-existing player
}


void CHltvReplaySystem::OnPlayerDeath( IGameEvent *event )
{
	if ( m_DelayedReplay.IsValid() || GetHltvReplayDelay() )
	{
		// we're already replaying or preparing to replay. Let's not react to new player deaths
		return;
	}

	int iPlayerIndexVictim = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
	int iPlayerIndexKiller = engine->GetPlayerForUserID( event->GetInt( "attacker" ) );
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
	{
		DevMsg( "OnPlayerDeath: Cannot replay without local player\n" );
		return;
	}
	bool isLocalVictim = pLocalPlayer && iPlayerIndexVictim == pLocalPlayer->entindex(), isReplayAvailable = iPlayerIndexKiller && iPlayerIndexKiller != iPlayerIndexVictim && !event->GetBool( "noreplay", false );

	UpdateHltvReplayButtonTimeOutState();

	if ( !isReplayAvailable || m_bHltvReplayButtonTimedOut )
		return; // no need to react to anything if replay isn't available for this death

	if ( isLocalVictim )
	{
		m_LocalPlayer.m_bLastSeenAlive = false; // we can assume the local player is dead and unlock replays. The next Update may turn it back on for a few frames, but then it'll go back to dead
		if ( iPlayerIndexVictim != iPlayerIndexKiller ) // we don't replay suicides
		{
			m_DelayedReplay.bFinalKillOfRound = false;
			if ( PrepareHltvReplayCountdown() )
			{
				m_DelayedReplay.nRequest = REPLAY_EVENT_DEATH;
			}
		}
	}
	else if (
		iPlayerIndexKiller <= MAX_PLAYERS // the killer must be an actual player (not e.g. a bomb) for replay to make sense
		&& !m_LocalPlayer.m_bLastSeenAlive && spec_replay_autostart.GetBool()
		&& spec_replay_others_experimental.GetBool() && IsPlayerTeamDead( iPlayerIndexVictim )
		)
	{
		int nLocalPlayerTeam = pLocalPlayer->GetTeamNumber();
		if ( nLocalPlayerTeam == TEAM_CT || nLocalPlayerTeam == TEAM_TERRORIST )
		{
			if ( CBaseEntity *pTarget = pLocalPlayer->GetObserverTarget() )
			{
				int nObserverTarget = pTarget->entindex();
				if ( nObserverTarget != iPlayerIndexKiller && nObserverTarget != iPlayerIndexVictim ) // 
				{
					m_DelayedReplay.bFinalKillOfRound = true;
					// EXPERIMENT: showing death of other players to people
					if ( PrepareHltvReplayCountdown() )
					{
						// we're dead anyway... request a replay, it may be interesting
						m_DelayedReplay.nRequest = REPLAY_EVENT_GENERIC;
						extern void CS_FreezePanel_ResetDamageText( int iPlayerIndexKiller, int iPlayerIndexVictim );
						CS_FreezePanel_ResetDamageText( iPlayerIndexKiller, iPlayerIndexVictim );
					}
				}
			}
		}
	}

	if ( isLocalVictim || m_DelayedReplay.nRequest >= 0 )
	{// Some interesting event happened, and we are not replaying anything else. Store some stats that may or may not be useful for later replay
		m_DelayedReplay.nPrimaryTargetEntIndex = iPlayerIndexKiller;
		m_DelayedReplay.flEventTime = gpGlobals->curtime;
		m_DelayedReplay.flTime = gpGlobals->curtime + spec_replay_autostart_delay.GetFloat();
		m_DelayedReplay.nPrimaryVictimEntIndex = iPlayerIndexVictim;
		m_DelayedReplay.bPrimaryVictimIsLocalPlayer = isLocalVictim;
	}
}

void CHltvReplaySystem::OnLocalPlayerRespawning()
{
}

bool CHltvReplaySystem::PrepareHltvReplayCountdown()
{
	bool bCountdownStarted = false;
	if ( !GetHltvReplayDelay() )
	{
		m_nExperimentalEvents |= EE_REPLAY_OFFERED;

		// if we have to show freezeframe before deathcam, this is the place to request it
		if ( IsHltvReplayFeatureEnabled() && spec_replay_autostart.GetBool() )
		{
			bCountdownStarted = true;
			m_nExperimentalEvents |= EE_REPLAY_AUTOMATIC;

			float flDelay = spec_replay_autostart_delay.GetFloat();
			m_DelayedReplay.flTime = gpGlobals->curtime + flDelay;
			if ( !IsFadeoutActive() )
			{
				// use fadeout 
				StartFadeout( gpGlobals->curtime + flDelay, Min( flDelay, spec_replay_fadeout.GetFloat() ) );
			}
			// DevMsg( "%.2f Replay: starting to fade out\n", gpGlobals->curtime ); // replayfade
		}
		CS_FreezePanel_OnHltvReplayButtonStateChanged();
	}
	return bCountdownStarted;
}




ConVar r_replay_post_effect( "r_replay_post_effect", "-1", FCVAR_CHEAT );

bool CHltvReplaySystem::WantsReplayEffect() const
{
	if ( r_replay_post_effect.GetInt() == -1 )
	{
		return ( g_HltvReplaySystem.GetHltvReplayDelay() != 0 );
	}
	else
		return r_replay_post_effect.GetBool();
}


void CHltvReplaySystem::CacheRagdollBones()
{
	PurgeRagdollBoneCache();
	if ( !spec_replay_cache_ragdolls.GetBool() )
		return;
	C_BaseEntityIterator iterator;
	C_BaseEntity *pEnt;
	int nRagdollsCached = 0;
	while ( ( pEnt = iterator.Next() ) != NULL )
	{
		if ( C_CSRagdoll * pRagdoll = dynamic_cast< C_CSRagdoll * >( pEnt ) )
		{
			if ( int nEntIndex = pRagdoll->entindex() )
			{
				if ( pRagdoll->m_pRagdoll )
				{
					nRagdollsCached++;
					int nBones = pRagdoll->GetModelPtr()->numbones();
					int nBodyParts = pRagdoll->m_pRagdoll->RagdollBoneCount();
					CachedRagdollBones_t *pCachedRagdoll = ( CachedRagdollBones_t * )MemAlloc_AllocAligned( sizeof( CachedRagdollBones_t ) + sizeof( matrix3x4a_t ) * ( nBones + nBodyParts + 1 ), 16 );
					pCachedRagdoll->bAllAsleep = true;
					pCachedRagdoll->nBones = nBones;
					pCachedRagdoll->nBodyParts = nBodyParts;
					matrix3x4a_t *pBones = pCachedRagdoll->GetBones();
					pBones[ nBones ].SetColumn( Vector( 11, 22, 33 ), X_AXIS );
					pRagdoll->SetupBones( pBones, nBones, BONE_USED_BY_ANYTHING, gpGlobals->curtime );
					matrix3x4a_t *pBodyParts = pCachedRagdoll->GetBodyParts();
					for ( int i = 0; i < nBodyParts; i++ )
					{
						if ( IPhysicsObject* pObj = pRagdoll->m_pRagdoll->RagdollPhysicsObject( i ) )
						{
							pObj->GetPositionMatrix( &pBodyParts[ i ] );
							if ( !pObj->IsAsleep() )
								pCachedRagdoll->bAllAsleep = false;
						}
						else
						{
							pBodyParts[ i ].SetToIdentity();
						}
					}

					Assert( pBones[ nBones ].GetColumn( X_AXIS ) == Vector( 11, 22, 33 ) );
					Assert( m_mapCachedRagdollBones.Find( nEntIndex ) == m_mapCachedRagdollBones.InvalidHandle() );
					m_mapCachedRagdollBones.Insert( nEntIndex, pCachedRagdoll );
				}
			}
			else
			{
				Warning( "A ragdoll without entindex found @%p\n", pRagdoll );
			}
		}
	}
	if ( nRagdollsCached )
		DevMsg( "%d ragdolls cached\n", nRagdollsCached );
}


CachedRagdollBones_t *CHltvReplaySystem::GetCachedRagdollBones( int nEntIndex, bool bTake )
{
	if ( gpGlobals->tickcount > m_nHltvReplayBeginTick + 5 )
		return NULL; // this cache isn't useful anymore, we need to create ragdoll dynamically - otherwise it'll just teleport to the final position instantaneously

	UtlHashHandle_t hFind = m_mapCachedRagdollBones.Find( nEntIndex );
	CachedRagdollBones_t *pTakenBones = NULL;

	if ( hFind != m_mapCachedRagdollBones.InvalidHandle() )
	{
		pTakenBones = m_mapCachedRagdollBones[ hFind ];
		if ( bTake )
			m_mapCachedRagdollBones.RemoveByHandle( hFind );
	}
	
	return pTakenBones;
}


void CHltvReplaySystem::FreeCachedRagdollBones( CachedRagdollBones_t*pBones )
{
	MemAlloc_FreeAligned( pBones );
}


CHltvReplaySystem::CLocalPlayerProps::CLocalPlayerProps()
{
	m_bLastSeenAlive = false;
	m_nLastTickUpdated = gpGlobals->tickcount;
}

bool CHltvReplaySystem::CLocalPlayerProps::Update()
{
	if ( m_nLastTickUpdated != gpGlobals->tickcount )
	{
		m_nLastTickUpdated = gpGlobals->tickcount;
		bool bLastLastSeenAlive = m_bLastSeenAlive;
		if ( C_BasePlayer *pBasePlayer = C_BasePlayer::GetLocalPlayer() )
		{
			m_bLastSeenAlive = pBasePlayer->IsAlive();
		}
		else
		{
			m_bLastSeenAlive = false;
		}

		if ( bLastLastSeenAlive != m_bLastSeenAlive )
			return true; // something changed
	}
	return false; // nothing changed
}


float CHltvReplaySystem::GetReplayMessageTime()
{
	static ConVarRef spec_replay_message_time( "spec_replay_message_time" );
	float flReplayMessageTime = spec_replay_message_time.GetFloat();
	if ( CCSGameRules* pRules = CSGameRules() )
	{
		if ( pRules->m_iRoundWinStatus != WINNER_NONE )
		{// the message time is cut short by the end of round
			flReplayMessageTime = spec_replay_autostart_delay.GetFloat();
		}
	}
	return flReplayMessageTime;
}


bool CHltvReplaySystem::UpdateHltvReplayButtonTimeOutState()
{
	bool bTimedOut = false;

	if ( CCSGameRules* pRules = CSGameRules() )
	{
		if ( pRules->IsWarmupPeriod() && !pRules->IsWarmupPeriodPaused() )
		{
			static ConVarRef spec_replay_leadup_time( "spec_replay_leadup_time" );
			static ConVarRef spec_replay_winddown_time( "spec_replay_winddown_time" );

			float flWarmupTimeLeft = pRules->GetWarmupPeriodEndTime() - gpGlobals->curtime;
			float flReplayRoundtripTime = spec_replay_autostart_delay.GetFloat() + spec_replay_leadup_time.GetFloat() + spec_replay_winddown_time.GetFloat();
			if ( flWarmupTimeLeft <= flReplayRoundtripTime )
			{
				bTimedOut = true;
			}
		}
	}

	bool bStateUpdated = ( m_bHltvReplayButtonTimedOut != bTimedOut );
	m_bHltvReplayButtonTimedOut = bTimedOut;
	return bStateUpdated;
}
