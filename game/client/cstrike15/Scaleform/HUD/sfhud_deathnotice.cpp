//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//


#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include "scaleformui/scaleformui.h"
#include "sfhud_deathnotice.h"
#include "c_cs_playerresource.h"
#include "c_cs_player.h"
#include "utlvector.h"
#include "vgui/ILocalize.h"
#include "vstdlib/vstrtools.h"
#include "sfhudfreezepanel.h"
#include "strtools.h"
#include "c_cs_team.h"
#include <engine/IEngineSound.h>
#include "hltvreplaysystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



DECLARE_HUDELEMENT( SFHudDeathNoticeAndBotStatus );
//
SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( SetConfig ),
SFUI_END_GAME_API_DEF( SFHudDeathNoticeAndBotStatus, DeathNotice );

static const float NOTICE_UPDATE_INTERVAL = 0.05f;
static const float EXTRA_NOTICE_PADDING = 0.0f;

extern ConVar mp_display_kill_assists;
extern ConVar cl_show_clan_in_death_notice;

SFHudDeathNoticeAndBotStatus::SFHudDeathNoticeAndBotStatus( const char *value ) : SFHudFlashInterface( value ),
	m_nNotificationDisplayMax( 0 ),
	m_fNotificationLifetime( 0.0f ),
	m_fNotificationFadeLength( 0.0f ),
	m_fNotificationScrollLength( 0.0f ),
	m_fLocalPlayerLifetimeMod( 0.f ),
	m_bVisible( false )
{
	m_wCTColor[0] = 0x0;
	m_wTColor[0] = 0x0;

	m_vecNoticeText.EnsureCapacity( engine->GetMaxClients() );
	SetHiddenBits( HIDEHUD_MISCSTATUS );

	m_vecNoticeText.EnsureCapacity( 16 );
	m_vecPendingNoticeText.EnsureCapacity( 16 );
	m_vecNoticeHandleCache.EnsureCapacity( 16 );
}


SFHudDeathNoticeAndBotStatus::~SFHudDeathNoticeAndBotStatus()
{
}


void SFHudDeathNoticeAndBotStatus::FlashReady( void )
{
	if ( m_FlashAPI && m_pScaleformUI )
	{
		ListenForGameEvent( "player_death" );
		ShowPanel( false );

		// Populate cache
		WITH_SLOT_LOCKED
		{
			for ( int i = 0; i < 16; ++i )
			{
				SFVALUE result = m_pScaleformUI->Value_Invoke( m_FlashAPI, "AddPanel", NULL, 0 );

				IScaleformUI::_ValueType resultType =  m_pScaleformUI->Value_GetType( result );

				if ( resultType == IScaleformUI::VT_DisplayObject )
				{
					m_vecNoticeHandleCache.AddToTail( result );
				}
				else
				{
					SafeReleaseSFVALUE( result );
					Assert( false && "AddPanel failed. Probably referencing a bad name." );
				}
			}
		}
	}
}


bool SFHudDeathNoticeAndBotStatus::PreUnloadFlash( void )
{
	// cleanup
	StopListeningForAllEvents();

	for (  int nPos = 0; nPos < m_vecNoticeText.Count(); nPos++ )
	{
		SafeReleaseSFVALUE( m_vecNoticeText[nPos].m_pPanel );
	}

	for (  int nPos = 0; nPos < m_vecPendingNoticeText.Count(); nPos++ )
	{
		SafeReleaseSFVALUE( m_vecPendingNoticeText[nPos].m_pPanel );
	}

	for (  int nPos = 0; nPos < m_vecNoticeHandleCache.Count(); nPos++ )
	{
		SafeReleaseSFVALUE( m_vecNoticeHandleCache[nPos] );
	}

	m_vecNoticeText.Purge();
	m_vecPendingNoticeText.Purge();
	m_vecNoticeHandleCache.Purge();

	return true;
}


void SFHudDeathNoticeAndBotStatus::LevelInit( void )
{
	if ( !FlashAPIIsValid() )
	{
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFHudDeathNoticeAndBotStatus, this, DeathNotice );
	}
}


void SFHudDeathNoticeAndBotStatus::LevelShutdown( void )
{
	if ( FlashAPIIsValid() )
	{
		RemoveFlashElement();
	}
}


void SFHudDeathNoticeAndBotStatus::ShowPanel( const bool bShow )
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

ConVar cl_drawhud_force_deathnotices( "cl_drawhud_force_deathnotices", "0", FCVAR_RELEASE, "0: default; 1: draw deathnotices even if hud disabled; -1: force no deathnotices" );
bool SFHudDeathNoticeAndBotStatus::ShouldDraw( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	return ( cl_drawhud_force_deathnotices.GetInt() >= 0 ) && (
		( cl_drawhud_force_deathnotices.GetInt() > 0 ) ||
		cl_drawhud.GetBool()
		) && CHudElement::ShouldDraw();
}


void SFHudDeathNoticeAndBotStatus::SetActive( bool bActive )
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


void SFHudDeathNoticeAndBotStatus::FireGameEvent( IGameEvent *event )
{
	if ( FlashAPIIsValid() )
	{
		const char *type = event->GetName();

		if ( !V_strcmp( type, "player_death" ) )
		{
			if ( !g_HltvReplaySystem.GetHltvReplayDelay() || event->GetBool( "realtime_passthrough" ) )
				OnPlayerDeath( event );
		}
	}
}


void SFHudDeathNoticeAndBotStatus::OnPlayerDeath( IGameEvent * event )
{
	C_CS_PlayerResource *cs_PR = dynamic_cast<C_CS_PlayerResource *>( g_PR );

	if ( !cs_PR )
	{
		Assert( false );
		return;
	}

	int nAttacker = engine->GetPlayerForUserID( event->GetInt( "attacker" ) );
	int nVictim = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
	int nAssister = mp_display_kill_assists.GetBool() ? engine->GetPlayerForUserID( event->GetInt( "assister" ) ) : 0;

	const char * szWeapon = event->GetString( "weapon" );
	if ( szWeapon == NULL || szWeapon[ 0 ] == 0 )
		return;

	bool bHeadshot = event->GetInt( "headshot" ) > 0;
	bool bSuicide = ( nAttacker == 0 || nAttacker == nVictim );
	bool bDominated = false;
	bool bRevenge = false;
	bool isVictim = GetLocalPlayerIndex() == nVictim;
	bool bPenetrated = ( event->GetInt( "penetrated" ) > 0 );
	bool isKiller = !isVictim && ( GetLocalPlayerIndex() == nAttacker || GetLocalPlayerIndex() == nAssister ); //Need to check isVictim because the player is both killer and victim in a suicide

	wchar_t szAttackerName[ MAX_DECORATED_PLAYER_NAME_LENGTH ];
	wchar_t szVictimName[ MAX_DECORATED_PLAYER_NAME_LENGTH ];
	wchar_t szAssisterName[ MAX_DECORATED_PLAYER_NAME_LENGTH ];

	szAttackerName[ 0 ] = L'\0';
	szVictimName[ 0 ] = L'\0';
	szAssisterName[ 0 ] = L'\0';

	EDecoratedPlayerNameFlag_t kDontShowClanName = k_EDecoratedPlayerNameFlag_DontShowClanName;
	if ( cl_show_clan_in_death_notice.GetInt() > 0 )
		kDontShowClanName = k_EDecoratedPlayerNameFlag_Simple;

	if ( nAttacker > 0 )
	{
		cs_PR->GetDecoratedPlayerName( nAttacker, szAttackerName, sizeof( szAttackerName ), EDecoratedPlayerNameFlag_t( k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot | kDontShowClanName ) );
	}

	if ( nVictim > 0 )
	{
		cs_PR->GetDecoratedPlayerName( nVictim, szVictimName, sizeof( szVictimName ), EDecoratedPlayerNameFlag_t( k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot | kDontShowClanName ) );
	}

	if ( nAssister > 0 )
	{
		// if our attacker is the same as our assiter, it means a bot attacked the victim and a player took over that bot
		if ( nAssister == nAttacker )
			nAssister = 0;
		else
		{
			cs_PR->GetDecoratedPlayerName( nAssister, szAssisterName, sizeof( szAssisterName ), EDecoratedPlayerNameFlag_t( k_EDecoratedPlayerNameFlag_AddBotToNameIfControllingBot | kDontShowClanName ) );
		}
	}

	// Highlight "The Suspect" in death feed
	if ( CDemoPlaybackParameters_t const *pParameters = engine->GetDemoPlaybackParameters() )
	{
		if ( pParameters->m_uiLockFirstPersonAccountID )
		{
			player_info_t pinfo;
			engine->GetPlayerInfo( nAttacker, &pinfo );
			if ( pParameters->m_uiLockFirstPersonAccountID == CSteamID( pinfo.xuid ).GetAccountID() )
			{
				isKiller = true;
			}
		}
	}

	if ( bSuicide )
	{
		STEAMWORKS_TESTSECRETALWAYS();
	}
	int nVictimTeam = nVictim  > 0 ? cs_PR->GetTeam( nVictim ) : 0;
	int nAtackerTeam = nAttacker > 0 ? cs_PR->GetTeam( nAttacker ) : 0;
	int nAssisterTeam = nAssister > 0 ? cs_PR->GetTeam( nAssister ) : 0;

	CCSPlayer* pCSPlayerKiller = ToCSPlayer( ClientEntityList().GetBaseEntity( nAttacker ) );
	CCSPlayer* pCSPlayerAssister = ToCSPlayer( ClientEntityList().GetBaseEntity( nAssister ) );

	if ( event->GetInt( "dominated" ) > 0 || ( pCSPlayerKiller != NULL && pCSPlayerKiller->IsPlayerDominated( nVictim ) ) )
	{
		bDominated = true;
	}
	else if ( event->GetInt( "revenge" ) > 0 )
	{
		bRevenge = true;
	}

	// Setup HTML
	wchar_t szDomination[ 64 ];
	GetIconHTML( L"domination", szDomination, ARRAYSIZE( szDomination ) );

	wchar_t szHeadshot[ 64 ];
	GetIconHTML( L"headshot", szHeadshot, ARRAYSIZE( szHeadshot ) );

	wchar_t szPenetration[ 64 ];
	GetIconHTML( L"penetrate", szPenetration, ARRAYSIZE( szPenetration ) );

	wchar_t szRevenge[ 64 ];
	GetIconHTML( L"revenge", szRevenge, ARRAYSIZE( szRevenge ) );

	wchar_t szSuicide[ 64 ];
	GetIconHTML( L"suicide", szSuicide, ARRAYSIZE( szSuicide ) );

	C_CSPlayer *pLocalCSPlayer = C_CSPlayer::GetLocalCSPlayer();
	if ( !pLocalCSPlayer )
		return;

	bool bAssisterIsTarget = pCSPlayerAssister && pCSPlayerAssister->IsAssassinationTarget();
	bool bAttackerIsTarget = pCSPlayerKiller && pCSPlayerKiller->IsAssassinationTarget();

	if ( nAssister > 0 )
	{
		if ( !bAttackerIsTarget )
			TruncatePlayerName( szAttackerName, ARRAYSIZE( szAttackerName ), DEATH_NOTICE_ASSIST_NAME_TRUNCATE_AT );

		if ( !bAssisterIsTarget )
			TruncatePlayerName( szAssisterName, ARRAYSIZE( szAssisterName ), DEATH_NOTICE_ASSIST_SHORT_NAME_TRUNCATE_AT );
	}
	else
	{
		if ( !bAttackerIsTarget )
			TruncatePlayerName( szAttackerName, ARRAYSIZE( szAttackerName ), DEATH_NOTICE_NAME_TRUNCATE_AT );
	}

	wchar_t szAttackerHTML[ MAX_DECORATED_PLAYER_NAME_LENGTH ];
	V_snwprintf( szAttackerHTML, ARRAYSIZE( szAttackerHTML ), DEATH_NOTICE_FONT_STRING, ( nAtackerTeam == TEAM_CT ) ? m_wCTColor : m_wTColor, ( !bSuicide ) ? szAttackerName : L"" );
	wchar_t szAssisterHTML[ MAX_DECORATED_PLAYER_NAME_LENGTH ];
	V_snwprintf( szAssisterHTML, ARRAYSIZE( szAssisterHTML ), DEATH_NOTICE_FONT_STRING, ( nAssisterTeam == TEAM_CT ) ? m_wCTColor : m_wTColor, ( nAssister > 0 ) ? szAssisterName : L"" );

	CCSPlayer* pCSPlayerVictim = ToCSPlayer( ClientEntityList().GetBaseEntity( nVictim ) );
	bool bVictimIsTarget = pCSPlayerVictim && pCSPlayerVictim->IsAssassinationTarget();
	if ( !bVictimIsTarget )
	{
		if ( nAssister > 0 )
			TruncatePlayerName( szVictimName, ARRAYSIZE( szVictimName ), DEATH_NOTICE_ASSIST_NAME_TRUNCATE_AT );
		else
			TruncatePlayerName( szVictimName, ARRAYSIZE( szVictimName ), DEATH_NOTICE_NAME_TRUNCATE_AT );
	}

	wchar_t szVictimHTML[ MAX_DECORATED_PLAYER_NAME_LENGTH ];
	V_snwprintf( szVictimHTML, ARRAYSIZE( szVictimHTML ), DEATH_NOTICE_FONT_STRING, ( nVictimTeam == TEAM_CT ) ? m_wCTColor : m_wTColor, szVictimName );

	wchar_t szAttAssComboHTML[ MAX_DECORATED_PLAYER_NAME_LENGTH ];
	V_snwprintf( szAttAssComboHTML, ARRAYSIZE( szAttAssComboHTML ), ( nAssister > 0 ) ? DEATH_NOTICE_ATTACKER_PLUS_ASSISTER : DEATH_NOTICE_ATTACKER_NO_ASSIST, bSuicide ? szVictimHTML : szAttackerHTML, szAssisterHTML );

	if ( Q_strcmp( "knifegg", szWeapon ) == 0 )
		szWeapon = "knife";
	else if ( Q_strcmp( "molotov_projectile_impact", szWeapon ) == 0 )
		szWeapon = "inferno";

	if ( Q_strcmp( "knife", szWeapon ) == 0 )
	{
		if ( pCSPlayerKiller && pCSPlayerKiller->GetTeamNumber() == TEAM_CT )
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
	GetIconHTML( wszWeapon, szWeaponHTML, ARRAYSIZE( szWeaponHTML ) );

	wchar_t szNotice[ COMPILETIME_MAX( DEATH_NOTICE_TEXT_MAX, 3 * MAX_DECORATED_PLAYER_NAME_LENGTH ) ];

	V_snwprintf( szNotice,
		ARRAYSIZE( szNotice ),
#if defined(_PS3) || defined(POSIX)
		L"%ls%ls%ls%ls%ls%ls%ls%ls",
#else
		L"%s%s%s%s%s%s%s%s",
#endif
		bRevenge ? szRevenge : L"",
		bDominated ? szDomination : L"",
		szAttAssComboHTML,
		bSuicide ? L"" : szWeaponHTML,
		bPenetrated ? szPenetration : L"",
		bHeadshot ? szHeadshot : L"",
		bSuicide ? szSuicide : L"",
		szVictimHTML );

	PushNotice( szNotice, isVictim, isKiller );
}


void SFHudDeathNoticeAndBotStatus::ClearNotices( void )
{
	ScaleformDisplayInfo dinfo;

	FOR_EACH_VEC_BACK( m_vecNoticeText, i )
	{
		dinfo.Clear();
		dinfo.SetVisibility( false );
		m_pScaleformUI->Value_SetDisplayInfo( m_vecNoticeText[i].m_pPanel, &dinfo );

		m_vecNoticeHandleCache.AddToTail( m_vecNoticeText[i].m_pPanel );
		m_vecNoticeText.Remove( i );
	}

	FOR_EACH_VEC_BACK( m_vecPendingNoticeText, i )
	{
		m_vecNoticeHandleCache.AddToTail( m_vecPendingNoticeText[i].m_pPanel );
		m_vecPendingNoticeText.Remove( i );
	}
}

void SFHudDeathNoticeAndBotStatus::PushNotice( const char * szNoticeText, bool isVictim, bool isKiller )
{
	NoticeText_t notice;

	g_pVGuiLocalize->ConvertANSIToUnicode( szNoticeText,  notice.m_szNotice, sizeof( notice.m_szNotice ) );

	PushNotice( notice, isVictim, isKiller );
}

void SFHudDeathNoticeAndBotStatus::PushNotice( const wchar_t* wszNoticeText, bool isVictim, bool isKiller )
{
	NoticeText_t notice;

	V_wcsncpy( notice.m_szNotice, wszNoticeText, sizeof( notice.m_szNotice ) );

	PushNotice( notice, isVictim, isKiller );
}

void SFHudDeathNoticeAndBotStatus::PushNotice( NoticeText_t& notice, bool isVictim, bool isKiller )
{
	WITH_SLOT_LOCKED
	{
		SFVALUE result = NULL;
		if ( m_vecNoticeHandleCache.Count() )
		{
			result = m_vecNoticeHandleCache.Tail();
			m_vecNoticeHandleCache.FastRemove( m_vecNoticeHandleCache.Count()-1 );
		}
		else
		{
			result = m_pScaleformUI->Value_Invoke( m_FlashAPI, "AddPanel", NULL, 0 );
		}

		IScaleformUI::_ValueType resultType =  m_pScaleformUI->Value_GetType( result );

		if ( resultType == IScaleformUI::VT_DisplayObject )
		{
			notice.m_pPanel = result;

			WITH_SFVALUEARRAY( args, 4 )
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, notice.m_pPanel);
				m_pScaleformUI->ValueArray_SetElement( args, 1, &notice.m_szNotice[0]);
				m_pScaleformUI->ValueArray_SetElement( args, 2, isVictim );
				m_pScaleformUI->ValueArray_SetElement( args, 3, isKiller );

				SFVALUE boxheight = m_pScaleformUI->Value_Invoke( m_FlashAPI, "SetPanelText", args, 4);

				notice.m_fTextHeight = (float)m_pScaleformUI->Value_GetNumber( boxheight ) + EXTRA_NOTICE_PADDING;
				
				SafeReleaseSFVALUE( boxheight );

			}

			notice.m_fLifetimeMod = isVictim || isKiller ? m_fLocalPlayerLifetimeMod : 1.0f;
			notice.m_fSpawnTime = gpGlobals->curtime;

			m_vecPendingNoticeText.AddToHead( notice );

			// if we have too many pending notices, then remove the oldest one
			// this happens when we are skipping in demos
			int numPending = m_vecPendingNoticeText.Count();
			if ( numPending > 15 )
			{
				// pull the oldest one 
				numPending -= 1;
				NoticeText_t noticeToRemove = m_vecPendingNoticeText[numPending];
				m_vecPendingNoticeText.Remove( numPending );

				m_vecNoticeHandleCache.AddToTail( noticeToRemove.m_pPanel );
			}
		}
		else
		{
			SafeReleaseSFVALUE( result );
			Assert( false && "AddPanel failed. Probably referencing a bad name." );
		}
	}
}


void SFHudDeathNoticeAndBotStatus::SetConfig( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_nNotificationDisplayMax	= m_pScaleformUI->Params_GetArgAsNumber( obj, 0 );
	m_fNotificationScrollLength	= m_pScaleformUI->Params_GetArgAsNumber( obj, 1 );
	m_fNotificationFadeLength	= m_pScaleformUI->Params_GetArgAsNumber( obj, 2 );
	m_fNotificationLifetime		= m_pScaleformUI->Params_GetArgAsNumber( obj, 3 );

	V_UTF8ToUnicode( pui->Params_GetArgAsString( obj, 4 ), m_wCTColor, sizeof( m_wCTColor ) );
	V_UTF8ToUnicode( pui->Params_GetArgAsString( obj, 5 ), m_wTColor, sizeof( m_wTColor ) );

	m_fLocalPlayerLifetimeMod	= m_pScaleformUI->Params_GetArgAsNumber( obj, 6 );
}


void SFHudDeathNoticeAndBotStatus::ProcessInput( void )
{
	// limit how often we update this stuff because it doesn't have to be too
	// smooth
	if ( gpGlobals->curtime > m_fNextUpdateTime )
	{
		m_fNextUpdateTime = gpGlobals->curtime + NOTICE_UPDATE_INTERVAL;

		// go through the visible list, moving and setting positions as appropriate
		// we're going to to move the first one. And we're going to make sure that each item after that
		// is above the item below it. so if we go in order from head to tail, we should end up
		// moving the whole list just by calculating where the first entry should be.

		ScaleformDisplayInfo dinfo;
		float previousY = 0.f;
		float currentY = 0.f;
		float alpha = 0.f;
		bool bANoticeIsSpawning = false;

		for ( int i = m_vecNoticeText.Count() - 1; i >= 0; i-- )
		{
			// keep track of these through the function and set them at the bottom.
			// if the alpha is zero at the end of the function ( and it's not spawning )
			// we'll mark this notice for removal
			// doing it this way lets us add in the alpha from the lifetime being over
			alpha =0.0f;
			currentY = m_vecNoticeText[i].m_fY;

			if ( m_vecNoticeText[i].m_iState == NS_SPAWNING )
			{
				// this notice is fading in at the bottom of the panel
				// we want to scroll its position up the screen from 0, and bring the alpha up too
				// we do both of these over m_fNotificationScrollLength seconds
				float t = ( gpGlobals->curtime - m_vecNoticeText[i].m_fStateTime ) / m_fNotificationScrollLength;

				if ( t >= 1 )
				{
					// done spawning, now move to normal mode ( text just sits, and scrolls up )
					t = 1;
					m_vecNoticeText[i].m_iState = NS_IDLE;
				}
				else 
				{
					// keep track of whether a notice is currently spawning
					// we can only add one notice at a time, so we need to know
					// if this one is still fading in. We'll wait until this stays false
					// to move a notice over from the pending list.
					bANoticeIsSpawning = true;

					if ( t < 0 )
						t = 0;
				}

				// these will be set on the panel at the bottom of the loop
				currentY = ( -m_vecNoticeText[i].m_fTextHeight * t ) +  ( previousY + ( m_vecNoticeText[i].m_fTextHeight * 2 ) );
				alpha = t;
			}
			else
			{
				// make this notice's position be just below the previous notice's
				// position
				currentY = previousY + m_vecNoticeText[i].m_fTextHeight;

				alpha = 1.0f;
			}

			// now figure out if we need to fade this notice out because it's so old
			float dt = gpGlobals->curtime - m_vecNoticeText[i].m_fSpawnTime;

			if ( dt > ( m_fNotificationLifetime * m_vecNoticeText[i].m_fLifetimeMod ) )
			{
				float t = ( dt - ( m_fNotificationLifetime * m_vecNoticeText[i].m_fLifetimeMod ) ) / m_fNotificationFadeLength;

				// this is just Max( Min( t,1 ) , 0 );
				t = ( t < 0 ) ? 0 : ( ( t > 1 ) ? 1 : t );

				alpha *= 1.0f - t;
			}

			// if our alpha is zero ( and we're not spawning ) it means this notice
			// is now dead and can be removed
			if ( m_vecNoticeText[i].m_iState != NS_SPAWNING && alpha <= 0.0f )
			{
				m_vecNoticeText[i].m_bRemove = true;
			}
			else
			{
				// set the actual position / alpha on the notice
				dinfo.SetAlpha( alpha*100 );
				dinfo.SetY( currentY );
				m_pScaleformUI->Value_SetDisplayInfo( m_vecNoticeText[i].m_pPanel, &dinfo );
			}

			// remember this notice's position so that the next notice can be
			// placed below it
			previousY = currentY;
		}

		// now remove all the guys that are marked for removal ( they had 0 alpha )

		for ( int i = m_vecNoticeText.Count() - 1; i >= 0; i-- )
		{
			if ( m_vecNoticeText[i].m_bRemove )
			{
				dinfo.Clear();
				dinfo.SetVisibility( false );
				m_pScaleformUI->Value_SetDisplayInfo( m_vecNoticeText[i].m_pPanel, &dinfo );

				m_vecNoticeHandleCache.AddToTail( m_vecNoticeText[i].m_pPanel );
				m_vecNoticeText.Remove( i );
			}
		}


		// if we aren't currently bringing in a message, then
		// see if there are any pending ones to add
		if ( !bANoticeIsSpawning )
		{
			int numPending = m_vecPendingNoticeText.Count();
			if ( numPending )
			{
				// pull the next pending notice and put it on the bottom of the notice stack
				numPending -= 1;
				NoticeText_t notice = m_vecPendingNoticeText[numPending];
				m_vecPendingNoticeText.Remove( numPending );

				// only add notices that are not too old (stop spew when coming back from pause menu or when skipping in demos)
				if ( ( gpGlobals->curtime - notice.m_fSpawnTime ) < ( ( m_fNotificationLifetime * notice.m_fLifetimeMod ) + m_fNotificationFadeLength ) )
				{
					notice.m_iState = NS_SPAWNING;
					notice.m_fSpawnTime = gpGlobals->curtime;
					notice.m_fStateTime = gpGlobals->curtime;

					dinfo.Clear();
					dinfo.SetAlpha( 0.f );
					dinfo.SetY( 0.f );
					dinfo.SetVisibility( true );
					m_pScaleformUI->Value_SetDisplayInfo( notice.m_pPanel, &dinfo );

					m_vecNoticeText.AddToHead( notice );

					WITH_SFVALUEARRAY_SLOT_LOCKED( data, 1 )
					{
						m_pScaleformUI->ValueArray_SetElement( data, 0, notice.m_pPanel );
						m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "UpdateWidth", data, 1 );
					}
				}
				else
				{
					m_vecNoticeHandleCache.AddToTail( notice.m_pPanel );
				}
			}
		}
	}
	else if ( m_fNextUpdateTime > gpGlobals->curtime + NOTICE_UPDATE_INTERVAL )
	{
		m_fNextUpdateTime  = gpGlobals->curtime + NOTICE_UPDATE_INTERVAL;
	}
}


void SFHudDeathNoticeAndBotStatus::Show( void )
{
	if ( m_bVisible == false )
	{
		for ( int nPos = 0; nPos < m_vecNoticeText.Count(); nPos++ )
		{
			if ( m_vecNoticeText[nPos].m_pPanel )
			{
				ScaleformDisplayInfo dinfo;
				m_pScaleformUI->Value_GetDisplayInfo( m_vecNoticeText[nPos].m_pPanel, &dinfo );
				dinfo.SetVisibility( true );
				m_pScaleformUI->Value_SetDisplayInfo(  m_vecNoticeText[nPos].m_pPanel, &dinfo );
			}
		}

		m_bVisible = true;
	}
}

void SFHudDeathNoticeAndBotStatus::Hide( void )
{
	if ( m_bVisible == true )
	{
		for ( int nPos = 0; nPos < m_vecNoticeText.Count(); nPos++ )
		{
			if ( m_vecNoticeText[nPos].m_pPanel )
			{
				ScaleformDisplayInfo dinfo;
				m_pScaleformUI->Value_GetDisplayInfo( m_vecNoticeText[nPos].m_pPanel, &dinfo );
				dinfo.SetVisibility( false );
				m_pScaleformUI->Value_SetDisplayInfo(  m_vecNoticeText[nPos].m_pPanel, &dinfo );
			}
		}

		m_bVisible = false;
	}
}



void SFHudDeathNoticeAndBotStatus::GetIconHTML( const wchar_t * szIcon, wchar_t * szBuffer, int nArraySize )
{
	V_snwprintf( szBuffer, nArraySize, DEATH_NOTICE_IMG_STRING, szIcon );
}




#endif // INCLUDE_SCALEFORM
