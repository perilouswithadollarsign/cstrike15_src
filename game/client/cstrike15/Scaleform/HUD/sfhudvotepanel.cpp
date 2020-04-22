//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include "sfhudvotepanel.h"
#include "hud_macros.h"
#include "sfhudcallvotepanel.h"
#include "sfhudfreezepanel.h"
#include "vgui/ILocalize.h"
#include "c_cs_playerresource.h"
#include "cs_hud_chat.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SAFECALL( handle, func )	\
	if ( handle )					\
	{								\
	func							\
	}

ConVar cl_vote_ui_active_after_voting( "cl_vote_ui_active_after_voting", "1" );
ConVar cl_vote_ui_show_notification( "cl_vote_ui_show_notification", "0" );


DECLARE_HUDELEMENT( SFHudVotePanel );
DECLARE_HUD_MESSAGE( SFHudVotePanel, CallVoteFailed );
DECLARE_HUD_MESSAGE( SFHudVotePanel, VoteStart );
DECLARE_HUD_MESSAGE( SFHudVotePanel, VotePass );
DECLARE_HUD_MESSAGE( SFHudVotePanel, VoteFailed );
DECLARE_HUD_MESSAGE( SFHudVotePanel, VoteSetup );


SFUI_BEGIN_GAME_API_DEF	
	//SFUI_DECL_METHOD( TimerCallback ),
	SFUI_DECL_METHOD( VoteYes ),
	SFUI_DECL_METHOD( VoteNo ),
SFUI_END_GAME_API_DEF( SFHudVotePanel, VotePanel );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SFHudVotePanel::SFHudVotePanel( const char *value ) : SFHudFlashInterface( value )
{		
	SetHiddenBits( HIDEHUD_MISCSTATUS );		
	m_hVoteButtonBG = NULL;
	m_hVoteLocalCast = NULL;
	m_option1Text = NULL;
	m_option2Text = NULL;
	m_option1CountText = NULL;
	m_option2CountText = NULL;
	m_bVisible = false;
	m_bHasFocus = false;
	m_nVoteYes = 0;
	m_nVoteNo = 0;
	m_flPostVotedHideTime = -1;
}

void SFHudVotePanel::ProcessInput( void )
{
	OnThink();
}

void SFHudVotePanel::LevelInit( void )
{
	if ( !FlashAPIIsValid() )
	{
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFHudVotePanel, this, VotePanel );
	}
}

void SFHudVotePanel::LevelShutdown( void )
{
	if ( FlashAPIIsValid() )
	{
		RemoveFlashElement();
	}
}

void SFHudVotePanel::FlashReady( void )
{
	if ( !m_FlashAPI )
	{
		return;
	}

	SFVALUE root = g_pScaleformUI->Value_GetMember( m_FlashAPI, "VotePanel" );	

	if ( !root )
	{
		return;
	}

	SFVALUE inner = g_pScaleformUI->Value_GetMember( root, "VotePanelInner" );	

	if ( !inner )
	{
		SafeReleaseSFVALUE( root );
		return;
	}
	
	m_hVoteButtonBG = g_pScaleformUI->Value_GetMember( inner, "VoteButtonBG" );	
	m_hVoteLocalCast = g_pScaleformUI->TextObject_MakeTextObjectFromMember( inner, "VoteConfirm" );
	m_option1Text = g_pScaleformUI->TextObject_MakeTextObjectFromMember( inner, "Vote1" );
	m_option2Text = g_pScaleformUI->TextObject_MakeTextObjectFromMember( inner, "Vote2" );
	m_option1CountText = g_pScaleformUI->TextObject_MakeTextObjectFromMember( inner, "Vote1Count" );
	m_option2CountText = g_pScaleformUI->TextObject_MakeTextObjectFromMember( inner, "Vote2Count" );
	
	
	SafeReleaseSFVALUE( root );
	SafeReleaseSFVALUE( inner );

	// listen for events
	ListenForGameEvent( "vote_changed" );
	ListenForGameEvent( "vote_options" );
	ListenForGameEvent( "vote_cast" );

 	SetVoteActive( false );
 	m_flVoteResultCycleTime = -1;
 	m_flHideTime = -1;
 	m_bIsYesNoVote = true;
 	m_bPlayerVoted = false;
	m_bPlayerLocalVote = VOTE_UNCAST;
 	m_nVoteChoicesCount = 2;  // Yes/No is the default 	
	m_nVoteYes = 0;
	m_nVoteNo = 0;

	HOOK_HUD_MESSAGE( SFHudVotePanel, CallVoteFailed );
	HOOK_HUD_MESSAGE( SFHudVotePanel, VoteStart );
	HOOK_HUD_MESSAGE( SFHudVotePanel, VotePass );
	HOOK_HUD_MESSAGE( SFHudVotePanel, VoteFailed );
	HOOK_HUD_MESSAGE( SFHudVotePanel, VoteSetup );
}

bool SFHudVotePanel::PreUnloadFlash( void )
{			
	SafeReleaseSFVALUE( m_hVoteButtonBG );
	SafeReleaseSFTextObject( m_hVoteLocalCast );
	SafeReleaseSFTextObject( m_option1Text );
	SafeReleaseSFTextObject( m_option2Text );
	SafeReleaseSFTextObject( m_option1CountText );
	SafeReleaseSFTextObject( m_option2CountText );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SFHudVotePanel::FireGameEvent( IGameEvent *event )
{
	const char *eventName = event->GetName();
	if ( !eventName )
		return;

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
		return;

	if( FStrEq( eventName, "vote_changed" ) )
	{
		for ( int index = 0; index < MAX_VOTE_OPTIONS; index++ )
		{
			char szOption[2];
			Q_snprintf( szOption, sizeof( szOption ), "%i", index + 1 );

			char szVoteOptionCount[13] = "vote_option";
			Q_strncat( szVoteOptionCount, szOption, sizeof( szVoteOptionCount ), COPY_ALL_CHARACTERS );

			m_nVoteOptionCount[index] = event->GetInt( szVoteOptionCount );
		}
		m_nPotentialVotes = event->GetInt( "potentialVotes" );
	}
	else if ( FStrEq( eventName, "vote_options" ) )
	{
		m_VoteSetupChoices.RemoveAll();

		m_nVoteChoicesCount = event->GetInt( "count" );
		for ( int iIndex = 0; iIndex < m_nVoteChoicesCount; iIndex++ )
		{
			char szNumber[2];
			Q_snprintf( szNumber, sizeof( szNumber ), "%i", iIndex + 1 );

			char szOptionName[8] = "option";
			Q_strncat( szOptionName, szNumber, sizeof( szOptionName ), COPY_ALL_CHARACTERS );

			const char *pszOptionName = event->GetString( szOptionName );
			m_VoteSetupChoices.CopyAndAddToTail( pszOptionName );
		}
	}
	else if ( FStrEq( eventName, "vote_cast" ) )
	{
		int iPlayer = event->GetInt( "entityid" );
		C_BasePlayer *pPlayer = UTIL_PlayerByIndex( iPlayer );
		if ( pPlayer != pLocalPlayer )
			return;

		int vote_option = event->GetInt( "vote_option", TEAM_UNASSIGNED );
		if( vote_option == VOTE_OPTION1 )
		{
			WITH_SLOT_LOCKED
			{
				if ( m_pScaleformUI )
				{
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "flashYesVote", NULL, 0 );
				}
			}
			//ThumbBg1
			//[tj]Convert toScaleform animation g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( m_pVoteActive, "PulseOption1" );
		}
		else if( vote_option == VOTE_OPTION2 )
		{
			WITH_SLOT_LOCKED
			{
				if ( m_pScaleformUI )
				{
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "flashNoVote", NULL, 0 );
				}
			}
			//ThumbBg2
			//[tj]Convert toScaleform animation g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( m_pVoteActive, "PulseOption2" );
		}
		else if( vote_option == VOTE_OPTION3 )
		{
			//[tj]Convert toScaleform animation g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( m_pVoteActive, "PulseOption3" );
		}
		else if( vote_option == VOTE_OPTION4 )
		{
			//[tj]Convert toScaleform animation g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( m_pVoteActive, "PulseOption4" );
		}
		else if( vote_option == VOTE_OPTION5 )
		{
			//[tj]Convert toScaleform animation g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( m_pVoteActive, "PulseOption5" );
		}

		m_bPlayerVoted = true;
		m_bPlayerLocalVote = vote_option;

		if ( !cl_vote_ui_active_after_voting.GetBool() )
		{
			m_flPostVotedHideTime = gpGlobals->curtime + 1.5f;
		}
	}
}

void SFHudVotePanel::Hide( void )
{
	WITH_SLOT_LOCKED
	{
		if ( m_pScaleformUI )
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hide", NULL, 0 );
		}
	}			

	m_bHasFocus = false;
}

bool SFHudVotePanel::ShouldDraw( void )
{
	if ( IsTakingAFreezecamScreenshot() || (CSGameRules() && CSGameRules()->IsPlayingTraining()) )
		return false;

	return cl_drawhud.GetBool() && CHudElement::ShouldDraw();
}

// void SFHudVotePanel::TimerCallback( SCALEFORM_CALLBACK_ARGS_DECL )
// {	
// 	OnThink();
// }

//-----------------------------------------------------------------------------
// Purpose:  Sent only to the caller
//-----------------------------------------------------------------------------
bool SFHudVotePanel::MsgFunc_CallVoteFailed( const CCSUsrMsg_CallVoteFailed &msg )
{
	if ( !FlashAPIIsValid() )
	{
		return true;
	}

	vote_create_failed_t nReason = (vote_create_failed_t)msg.reason();
	int nTime = msg.time();

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
		return true;

	if ( CDemoPlaybackParameters_t const *pParams = engine->GetDemoPlaybackParameters() )
	{
		if ( pParams->m_bAnonymousPlayerIdentity )
			return true;
	}

	pLocalPlayer->EmitSound("Vote.Failed");

	m_flHideTime = gpGlobals->curtime + 3.0;

	char szTime[256];
	wchar_t wszTime[256];
	Q_snprintf( szTime, sizeof ( szTime), "%i", nTime );
	g_pVGuiLocalize->ConvertANSIToUnicode( szTime, wszTime, sizeof( wszTime ) );

	wchar_t wszFailureString[512];	

	const char * voteFailedStrings[VOTE_FAILED_MAX] = { NULL };

	voteFailedStrings[VOTE_FAILED_GENERIC] = "#SFUI_vote_failed";
	voteFailedStrings[VOTE_FAILED_TRANSITIONING_PLAYERS] = "#SFUI_vote_failed_transition_vote";
	voteFailedStrings[VOTE_FAILED_RATE_EXCEEDED] = "#SFUI_vote_failed_vote_spam";
	voteFailedStrings[VOTE_FAILED_ISSUE_DISABLED] = "#SFUI_vote_failed_disabled_issue";
	voteFailedStrings[VOTE_FAILED_MAP_NOT_FOUND] = "#SFUI_vote_failed_map_not_found";
	voteFailedStrings[VOTE_FAILED_MAP_NAME_REQUIRED] = "#SFUI_vote_failed_map_name_required";

	voteFailedStrings[VOTE_FAILED_FAILED_RECENTLY] = "#SFUI_vote_failed_recently";
	voteFailedStrings[VOTE_FAILED_FAILED_RECENT_KICK] = "#SFUI_vote_failed_recent_kick";
	voteFailedStrings[VOTE_FAILED_FAILED_RECENT_CHANGEMAP] = "#SFUI_vote_failed_recent_changemap";
	voteFailedStrings[VOTE_FAILED_FAILED_RECENT_SWAPTEAMS] = "#SFUI_vote_failed_recent_swapteams";
	voteFailedStrings[VOTE_FAILED_FAILED_RECENT_SCRAMBLETEAMS] = "#SFUI_vote_failed_recent_scrambleteams";
	voteFailedStrings[VOTE_FAILED_FAILED_RECENT_RESTART] = "#SFUI_vote_failed_recent_restart";
	
	voteFailedStrings[VOTE_FAILED_TEAM_CANT_CALL] = "#SFUI_vote_failed_team_cant_call";
	voteFailedStrings[VOTE_FAILED_WAITINGFORPLAYERS] = "#SFUI_vote_failed_waitingforplayers";
	voteFailedStrings[VOTE_FAILED_CANNOT_KICK_ADMIN] = "#SFUI_vote_failed_cannot_kick_admin";
	voteFailedStrings[VOTE_FAILED_SWAP_IN_PROGRESS] = "#SFUI_vote_failed_swap_in_prog";
	voteFailedStrings[VOTE_FAILED_SCRAMBLE_IN_PROGRESS] = "#SFUI_vote_failed_scramble_in_prog";
	voteFailedStrings[VOTE_FAILED_SPECTATOR] = "#SFUI_vote_failed_spectator";
	voteFailedStrings[VOTE_FAILED_DISABLED] = "#SFUI_vote_failed_disabled";	
	voteFailedStrings[VOTE_FAILED_NEXTLEVEL_SET] = "#SFUI_vote_failed_nextlevel_set";

	voteFailedStrings[VOTE_FAILED_TOO_EARLY_SURRENDER] = "#SFUI_vote_failed_surrender_too_early";

	voteFailedStrings[ VOTE_FAILED_MATCH_PAUSED ] = "#SFUI_vote_failed_paused";
	voteFailedStrings[ VOTE_FAILED_MATCH_NOT_PAUSED ] = "#SFUI_vote_failed_not_paused";
	voteFailedStrings[ VOTE_FAILED_NOT_10_PLAYERS ] = "#SFUI_vote_failed_not_10_players";
	voteFailedStrings[ VOTE_FAILED_NOT_IN_WARMUP ] = "#SFUI_vote_failed_not_in_warmup";
	voteFailedStrings[ VOTE_FAILED_CANT_ROUND_END ] = "#SFUI_vote_failed_cant_round_end";

	voteFailedStrings[ VOTE_FAILED_TIMEOUT_EXHAUSTED ] = "#SFUI_vote_failed_timeouts_exhausted";
	voteFailedStrings[ VOTE_FAILED_TIMEOUT_ACTIVE ] = "#SFUI_vote_failed_timeout_active";


	Assert( nReason < VOTE_FAILED_MAX );
	if ( nReason < VOTE_FAILED_MAX )
	{
		if ( nReason == VOTE_FAILED_RATE_EXCEEDED || nReason == VOTE_FAILED_FAILED_RECENTLY || nReason == VOTE_FAILED_FAILED_RECENT_KICK
			|| nReason == VOTE_FAILED_FAILED_RECENT_CHANGEMAP || nReason == VOTE_FAILED_FAILED_RECENT_SWAPTEAMS 
			|| nReason == VOTE_FAILED_FAILED_RECENT_SCRAMBLETEAMS || nReason == VOTE_FAILED_FAILED_RECENT_RESTART )
		{
			g_pVGuiLocalize->ConstructString( wszFailureString, sizeof( wszFailureString ), g_pVGuiLocalize->FindSafe( voteFailedStrings[nReason] ? voteFailedStrings[nReason] : voteFailedStrings[VOTE_FAILED_GENERIC] ), 1, wszTime );
		}
		else
		{
			g_pVGuiLocalize->ConstructString( wszFailureString, sizeof( wszFailureString ), g_pVGuiLocalize->FindSafe( voteFailedStrings[nReason] ? voteFailedStrings[nReason] : voteFailedStrings[VOTE_FAILED_GENERIC] ), 0 );
		}	

		WITH_SLOT_LOCKED
		{			
			WITH_SFVALUEARRAY( data, 4 )
			{			
				m_pScaleformUI->ValueArray_SetElement( data, 0, "#SFUI_vote_failed" );
				m_pScaleformUI->ValueArray_SetElement( data, 1, wszFailureString );
				m_pScaleformUI->ValueArray_SetElement( data, 2, false );  // did not pass
				m_pScaleformUI->ValueArray_SetElement( data, 3, false ); // shows the thumb results?

				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showResult", data, 4 );
			}
		}
		
	}	

	return true;
}

//-----------------------------------------------------------------------------
// Purpose:  Sent to everyone
//-----------------------------------------------------------------------------
bool SFHudVotePanel::MsgFunc_VoteFailed( const CCSUsrMsg_VoteFailed &msg )
{
	if ( !FlashAPIIsValid() )
	{
		return true;
	}

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
		return true;

	if ( CDemoPlaybackParameters_t const *pParams = engine->GetDemoPlaybackParameters() )
	{
		if ( pParams->m_bAnonymousPlayerIdentity )
			return true;
	}

	bool bShouldShowResults = false;

	// Is this a team-only vote?
	int iTeam = msg.team();
	int invalidTeam = TEAM_INVALID;
	if ( (iTeam == invalidTeam || iTeam == pLocalPlayer->GetTeamNumber()) && (m_nVoteYes != 0 || m_nVoteNo != 0) )
	{
		bShouldShowResults = true;
	}

	bool bOtherTeam = false;
	if ( iTeam != invalidTeam && iTeam != pLocalPlayer->GetTeamNumber() )
		bOtherTeam = true;

	vote_create_failed_t nReason = (vote_create_failed_t)msg.reason();

	const char* failureString;

	bool bThumbsCondition = false;

	switch ( nReason )
	{
	case VOTE_FAILED_GENERIC:		
		failureString = "#SFUI_vote_failed";
		break;

	case VOTE_FAILED_YES_MUST_EXCEED_NO:		
		failureString = "#SFUI_vote_failed_yesno";
		bThumbsCondition = true;
		break;

	case VOTE_FAILED_QUORUM_FAILURE:		
		failureString = "#SFUI_vote_failed_quorum";
		bThumbsCondition = true;
		break;

	case VOTE_FAILED_REMATCH:
		failureString = "#SFUI_vote_failed_rematch";
		bThumbsCondition = true;
		break;

	case VOTE_FAILED_CONTINUE:
		failureString = "#SFUI_vote_failed_continue";
		bThumbsCondition = true;
		break;

	default:
		AssertMsg( false, "Invalid vote failure reason" );
		failureString = "";
		break;
	}

	if ( bOtherTeam )
	{
		// don't do anything because you'll see the result of the vote in chat based on the action
		// 		CBaseHudChat *hudChat = (CBaseHudChat *)GET_HUDELEMENT( CHudChat );
		// 		if ( pwcIssue[0] )
		// 		{
		// 			hudChat->ChatPrintfW( pLocalPlayer->entindex(), CHAT_FILTER_SERVERMSG, pwcIssue );
		// 		}
	}
	else
	{
		//This will be hidden in OnThink
		WITH_SFVALUEARRAY( data, 4 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, "#SFUI_vote_failed" );		
			m_pScaleformUI->ValueArray_SetElement( data, 1, failureString );
			m_pScaleformUI->ValueArray_SetElement( data, 2, false ); // did not pass
			m_pScaleformUI->ValueArray_SetElement( data, 3, bThumbsCondition && bShouldShowResults ); // shows the thumb results?
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showResult", data, 4 );
		}

		SetVoteActive( false );
		m_bVotePassed = false;
		m_flVoteResultCycleTime = gpGlobals->curtime + 2;
		m_flHideTime = gpGlobals->curtime + 3.5;

		pLocalPlayer->EmitSound("Vote.Failed");
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool SFHudVotePanel::MsgFunc_VoteStart( const CCSUsrMsg_VoteStart &msg )
{
	if ( !FlashAPIIsValid() )
	{
		return true;
	}

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
		return true;

	if ( CDemoPlaybackParameters_t const *pParams = engine->GetDemoPlaybackParameters() )
	{
		if ( pParams->m_bAnonymousPlayerIdentity )
			return true;
	}

	// Is this a team-only vote?
	// first
	int iTeam = msg.team();
	int invalidTeam = TEAM_INVALID;
	bool bShowingOtherTeam = false;
	if ( iTeam != invalidTeam && iTeam != pLocalPlayer->GetTeamNumber() )
	{
		m_nVoteYes = 0;
		m_nVoteNo = 0;
		bShowingOtherTeam = true;
	}

	// Entity calling the vote
	bool bShowNotif = cl_vote_ui_show_notification.GetBool();
	const char *pszCallerName = "Server";
	// second
	int iEntityCallingVote = msg.ent_idx();
	if ( iEntityCallingVote != 99 )
	{
		C_BasePlayer *pVoteCaller = UTIL_PlayerByIndex( iEntityCallingVote );
		if ( pVoteCaller )
		{
			pszCallerName = pVoteCaller->GetPlayerName();

			// Don't show a notification to the caller
			if ( pVoteCaller == pLocalPlayer )
			{
				bShowNotif = false;
				m_bPlayerVoted = true;
				// we're assuming they did vote option 1 here because they voted for it
				m_bPlayerLocalVote = VOTE_OPTION1;
			}
		}
		else
		{
			// Caller invalid for some reason
			pszCallerName = "Player";
		}
	}

	// the vote type
	// third
	int nVoteType = msg.vote_type();

	// DisplayString
	// fourth
	const char *szIssue = msg.disp_str().c_str();

	// DetailString
	// fifth
	const char *szParam1 = msg.details_str().c_str();

	// OtherTeam string
	// sixth
	const char *szOtherTeam = msg.other_team_str().c_str();

	// seventh
	m_bIsYesNoVote = msg.is_yes_no_vote();

	// Display vote caller's name
	wchar_t wszCallerName[MAX_PLAYER_NAME_LENGTH];

  	wchar_t wszCleanName[MAX_DECORATED_PLAYER_NAME_LENGTH];


	wchar_t wszHeaderString[512];
	wchar_t *pwszHeaderString;

	// Player
	g_pVGuiLocalize->ConvertANSIToUnicode( pszCallerName, wszCallerName, sizeof( wszCallerName ) );

    g_pScaleformUI->MakeStringSafe( wszCallerName, wszCleanName, sizeof( wszCleanName ) );

	TruncatePlayerName( wszCleanName, ARRAYSIZE( wszCleanName ), VOTE_PANEL_NAME_TRUNCATE_AT );

	// String
	g_pVGuiLocalize->ConstructString( wszHeaderString, sizeof(wszHeaderString), g_pVGuiLocalize->Find( "#SFUI_vote_header" ), 1, wszCleanName );
	pwszHeaderString = wszHeaderString;	

	// Display the Issue
	wchar_t *pwcParam;
	wchar_t wcParam[128] = {0};

	wchar_t *pwcIssue;
	wchar_t wcIssue[512] = {0};

	if ( Q_strlen( szParam1 ) > 0 )
	{
		const int nMaxLength = MAX_MAP_NAME+10+1;
		char szToken[nMaxLength];

		if ( nVoteType == VOTEISSUE_NEXTLEVEL || nVoteType == VOTEISSUE_CHANGELEVEL )
		{
			szParam1 = V_GetFileName( szParam1 );
		}

		if ( (nVoteType == VOTEISSUE_CHANGELEVEL || nVoteType == VOTEISSUE_NEXTLEVEL ) && CSGameRules()->GetFriendlyMapNameToken(szParam1, szToken, nMaxLength) )
		{
			pwcParam = g_pVGuiLocalize->Find( szToken );
		}
		else
		{
			if ( szParam1[0] == '#' )
			{
				pwcParam = g_pVGuiLocalize->Find( szParam1 );
			}
			else
			{
				// Convert to wchar
				g_pVGuiLocalize->ConvertANSIToUnicode( szParam1, wcParam, sizeof( wcParam ) );
				pwcParam = wcParam;
			}
		}

		if ( bShowingOtherTeam )
			g_pVGuiLocalize->ConstructString( wcIssue, sizeof(wcIssue), g_pVGuiLocalize->Find( szOtherTeam ), 1, pwcParam );
		else
			g_pVGuiLocalize->ConstructString( wcIssue, sizeof(wcIssue), g_pVGuiLocalize->Find( szIssue ), 1, pwcParam );

		pwcIssue = wcIssue;
	}
	else
	{
		// no param, just localize the issue
		if ( bShowingOtherTeam )
			pwcIssue = g_pVGuiLocalize->Find( szOtherTeam );
		else
			pwcIssue = g_pVGuiLocalize->Find( szIssue );

		if ( !pwcIssue )
			pwcIssue = L"VOTE";
	}

	g_pScaleformUI->MakeStringSafe( pwcIssue, wszCleanName, sizeof( wszCleanName ) );
	pwcIssue = wszCleanName;
	
	// if the vote is called by the other team, just show a chat message here informing this team
	if ( bShowingOtherTeam )
	{
		CBaseHudChat *hudChat = (CBaseHudChat *)GET_HUDELEMENT( CHudChat );
		if ( pwcIssue[0] )
		{
			hudChat->ChatPrintfW( pLocalPlayer->entindex(), CHAT_FILTER_SERVERMSG, pwcIssue );
		}

		return true;
	}

	SetVoteActive( true );	

	SAFECALL( m_hVoteButtonBG, m_pScaleformUI->Value_SetVisible( m_hVoteButtonBG, false ); );

	// Figure out which UI
	if ( m_bIsYesNoVote )
	{
		UpdateYesNoButtonText( bShowingOtherTeam );
	}

#if !defined( CSTRIKE15 )
	else
	{
		// GENERAL UI
		if ( m_VoteSetupChoices.Count() )
		{
			// Clear the labels to prevent previous options from being displayed,
			// such as when there are fewer options this vote than the previous
			for ( int iIndex = 0; iIndex < MAX_VOTE_OPTIONS; iIndex++ )
			{
				// Construct Label name
				char szOptionNum[2];
				Q_snprintf( szOptionNum, sizeof( szOptionNum ), "%i", iIndex + 1 );

				char szVoteOptionCount[13] = "LabelOption";
				Q_strncat( szVoteOptionCount, szOptionNum, sizeof( szVoteOptionCount ), COPY_ALL_CHARACTERS );

				//Convert to Scaleform if we go to non-yes/no votes: m_pVoteActive->SetControlString( szVoteOptionCount, "" );
			}

			// Set us up the vote
			for ( int iIndex = 0; iIndex < m_nVoteChoicesCount; iIndex++ )
			{
				// Construct Option name
				const char *pszChoiceName = m_VoteSetupChoices[iIndex];

				char szOptionName[256];
				Q_snprintf( szOptionName, sizeof( szOptionName ), "F%i. ", iIndex + 1 );

				Q_strncat( szOptionName, pszChoiceName, sizeof( szOptionName ), COPY_ALL_CHARACTERS );

				// Construct Label name
				char szOptionNum[2];
				Q_snprintf( szOptionNum, sizeof( szOptionNum ), "%i", iIndex + 1 );

				char szVoteOptionCount[13] = "LabelOption";
				Q_strncat( szVoteOptionCount, szOptionNum, sizeof( szVoteOptionCount ), COPY_ALL_CHARACTERS );

				// Set Label string
				//Convert to Scaleform if we go to non-yes/no votes: 
				/*
				if ( m_pVoteActive )
				{
					m_pVoteActive->SetControlString( szVoteOptionCount, szOptionName );
				}
				*/
			}
		}
	}
#endif //!defined( CSTRIKE15 )

	ShowVoteUI( pwszHeaderString, pwcIssue );

	return true;
}

void SFHudVotePanel::UpdateYesNoButtonText( bool bShowOtherTeam )
{
	bool bUsingKeyboard = ( m_pScaleformUI && false == m_pScaleformUI->IsSetToControllerUI( SF_FULL_SCREEN_SLOT ) );

	// YES / NO UI
	wchar_t *pszTextVoted;
	wchar_t *pszTextY;
	wchar_t *pszTextN;

	if ( bShowOtherTeam )
	{
		pszTextN = L"";
		pszTextY = L"";
		pszTextVoted = L"";
	}
	else if ( !m_bPlayerVoted )
	{
		if ( bUsingKeyboard )
		{		
			pszTextN = g_pVGuiLocalize->Find( "#SFUI_vote_no_pc_instruction" );
			pszTextY = g_pVGuiLocalize->Find( "#SFUI_vote_yes_pc_instruction" );
		}
		else
		{
			pszTextN = g_pVGuiLocalize->Find( "#SFUI_vote_no_console_instruction" );
			pszTextY = g_pVGuiLocalize->Find( "#SFUI_vote_yes_console_instruction" );
		}

		pszTextVoted = L"";

		SAFECALL( m_hVoteButtonBG, m_pScaleformUI->Value_SetVisible( m_hVoteButtonBG, true ); );
	}
	else
	{
		if( m_bPlayerLocalVote == VOTE_OPTION1 )
		{
			pszTextVoted = g_pVGuiLocalize->Find( "#SFUI_vote_yes_confirmation_pc_instruction" );
		}
		else
		{
			pszTextVoted = g_pVGuiLocalize->Find( "#SFUI_vote_no_confirmation_pc_instruction" );
		}

		pszTextN = L"";
		pszTextY = L"";

		SAFECALL( m_hVoteButtonBG, m_pScaleformUI->Value_SetVisible( m_hVoteButtonBG, true ); );
	}

	if ( pszTextY )
	{			
		WITH_SLOT_LOCKED
		{
			if ( m_option1Text )
			{
				m_option1Text->SetTextHTML( m_pScaleformUI->ReplaceGlyphKeywordsWithHTML( pszTextY ) );
			}
		}
	}

	if ( pszTextN )
	{
		WITH_SLOT_LOCKED
		{
			if ( m_option2Text )
			{
				m_option2Text->SetTextHTML(  m_pScaleformUI->ReplaceGlyphKeywordsWithHTML( pszTextN )  );
			}
		}			
	}

	if ( pszTextVoted )
	{
		WITH_SLOT_LOCKED
		{
			if ( m_hVoteLocalCast )
			{
				m_hVoteLocalCast->SetTextHTML(  m_pScaleformUI->ReplaceGlyphKeywordsWithHTML( pszTextVoted )  );
			}
		}			
	}

	if ( !bShowOtherTeam )
	{
		char szYesCount[512] = "";
		Q_snprintf( szYesCount, 512, "%d", m_nVoteYes );
		char szNoCount[512] = "";
		Q_snprintf( szNoCount, 512, "%d", m_nVoteNo );
		WITH_SLOT_LOCKED
		{
			if ( m_option1CountText )
			{
				m_option1CountText->SetText( szYesCount );
			}

			if ( m_option2CountText )
			{
				m_option2CountText->SetText( szNoCount );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool SFHudVotePanel::MsgFunc_VotePass( const CCSUsrMsg_VotePass &msg )
{
	if ( !FlashAPIIsValid() )
	{
		return true;
	}

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
		return true;

	if ( CDemoPlaybackParameters_t const *pParams = engine->GetDemoPlaybackParameters() )
	{
		if ( pParams->m_bAnonymousPlayerIdentity )
			return true;
	}

	bool bShouldShowResults = false;

	// Is this a team-only vote?
	int iTeam = msg.team();
	int invalidTeam = TEAM_INVALID;
	if ( pLocalPlayer->GetTeamNumber() > 0 && (iTeam == invalidTeam || iTeam == pLocalPlayer->GetTeamNumber()) && (m_nVoteYes != 0 || m_nVoteNo != 0) )
	{
		bShouldShowResults = true;
	}

	bool bOtherTeam = false;
	if ( pLocalPlayer->GetTeamNumber() > 0 && iTeam != invalidTeam && iTeam != pLocalPlayer->GetTeamNumber() )
		bOtherTeam = true;

	int nVoteType = msg.vote_type();

	// Passed string
	const char *szResult = msg.disp_str().c_str();
	
	// Detail string
	const char *szParam1 = msg.details_str().c_str();

	// Localize
	wchar_t *pwcParam;
	wchar_t wcParam[128];

	const wchar_t *pwcIssue;
	wchar_t wcIssue[512];

	if ( Q_strlen( szParam1 ) > 0 )
	{
		const int nMaxLength = MAX_MAP_NAME+10+1;
		char szToken[nMaxLength];

		if ( nVoteType == VOTEISSUE_NEXTLEVEL || nVoteType == VOTEISSUE_CHANGELEVEL )
		{
			szParam1 = V_GetFileName( szParam1 );
		}

		if ( (nVoteType == VOTEISSUE_CHANGELEVEL || nVoteType == VOTEISSUE_NEXTLEVEL ) && CSGameRules()->GetFriendlyMapNameToken(szParam1, szToken, nMaxLength) )
		{
			pwcParam = g_pVGuiLocalize->Find( szToken );
		}
		else
		{
			if ( szParam1[0] == '#' )
			{
				pwcParam = g_pVGuiLocalize->Find( szParam1 );
			}
			else
			{
				// Convert to wchar
				g_pVGuiLocalize->ConvertANSIToUnicode( szParam1, wcParam, sizeof( wcParam ) );
				pwcParam = wcParam;
			}
		}

		g_pVGuiLocalize->ConstructString( wcIssue, sizeof(wcIssue), g_pVGuiLocalize->Find( szResult ), 1, pwcParam );
		pwcIssue = wcIssue;
	}
	else
	{
		// No param, just localize the result
		pwcIssue = g_pVGuiLocalize->FindSafe( szResult );
	}

	if ( bOtherTeam )
	{
		// don't do anything because you'll see the result of the vote in chat based on the action
// 		CBaseHudChat *hudChat = (CBaseHudChat *)GET_HUDELEMENT( CHudChat );
// 		if ( pwcIssue[0] )
// 		{
// 			hudChat->ChatPrintfW( pLocalPlayer->entindex(), CHAT_FILTER_SERVERMSG, pwcIssue );
// 		}
	}
	else
	{
		UpdateYesNoButtonText();

		WITH_SFVALUEARRAY( data, 4 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, "#SFUI_vote_passed" );
			m_pScaleformUI->ValueArray_SetElement( data, 1, pwcIssue );
			m_pScaleformUI->ValueArray_SetElement( data, 2, true ); // passed!
			m_pScaleformUI->ValueArray_SetElement( data, 3, bShouldShowResults ); // shows the thumb results?
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showResult", data, 4 );
		}

		pLocalPlayer->EmitSound( "Vote.Passed" );

		SetVoteActive( false );
		m_bVotePassed = true;
		m_flVoteResultCycleTime = gpGlobals->curtime + 2;
		m_flHideTime = gpGlobals->curtime + 3.5;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Creates a UI for Vote Issue selection
//-----------------------------------------------------------------------------
bool SFHudVotePanel::MsgFunc_VoteSetup( const CCSUsrMsg_VoteSetup &msg )
{	
	if ( CDemoPlaybackParameters_t const *pParams = engine->GetDemoPlaybackParameters() )
	{
		if ( pParams->m_bAnonymousPlayerIdentity )
			return true;
	}

	SFHudCallVotePanel::LoadDialog();

	//[tj] This stuff should be ported to the CallVote module.
	/*
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
		return;

	// Load up the list of Vote Issues
	m_VoteSetupIssues.RemoveAll();
	int nIssueCount = msg.ReadByte();
	if ( nIssueCount )
	{
		for ( int index = 0; index < nIssueCount; index++ )
		{
			char szIssue[256];
			msg.ReadString( szIssue, sizeof(szIssue) );
			if ( !m_VoteSetupIssues.HasElement( szIssue ) )
			{
				// Send it over to the listpanel
				m_VoteSetupIssues.CopyAndAddToTail( szIssue );
			}
		}
	}
	else
	{
		m_VoteSetupIssues.CopyAndAddToTail( "Voting disabled on this Server" );
	}
	m_pVoteSetupDialog->AddVoteIssues( m_VoteSetupIssues );


	
	// Load up the list of Vote Issue Parameters
	m_VoteSetupMapCycle.RemoveAll();
	if ( g_pStringTableServerMapCycle )
	{
		int index = g_pStringTableServerMapCycle->FindStringIndex( "ServerMapCycle" );
		if ( index != ::INVALID_STRING_INDEX )
		{
			int nLength = 0;
			const char *pszMapCycle = (const char *)g_pStringTableServerMapCycle->GetStringUserData( index, &nLength );
			if ( pszMapCycle && pszMapCycle[0] )
			{
				if ( pszMapCycle && nLength )
				{
					V_SplitString( pszMapCycle, "\n", m_VoteSetupMapCycle );
				}

				// Alphabetize
				if ( m_VoteSetupMapCycle.Count() )
				{
					m_VoteSetupMapCycle.Sort( m_VoteSetupMapCycle.SortFunc );
				}
			}
		}
	}	

	// Now send any data we gathered over to the listpanel
	PropagateOptionParameters();

	m_pVoteSetupDialog->Activate();	

	*/

	return true;
}

//[tj] This stuff should be ported to the CallVote module.
/*
//-----------------------------------------------------------------------------
// Purpose: Propagate vote option parameters to the Issue Parameters list
//-----------------------------------------------------------------------------
void SFHudVotePanel::PropagateOptionParameters( void )
{
	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pLocalPlayer )
		return;

	m_pVoteSetupDialog->AddVoteIssueParams_MapCycle( m_VoteSetupMapCycle );

	// Insert future issue param data containers here
}
*/


void SFHudVotePanel::SetVoteActive( bool bActive )
{
	m_bVoteActive = bActive;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SFHudVotePanel::ShowVoteUI( wchar_t* headerText, wchar_t* voteText, bool bShowingOtherTeam )
{
	if ( !FlashAPIIsValid() )
	{
		return;
	}

	m_bHasFocus = false;

	//Force a refresh before showing
	OnThink();

	WITH_SLOT_LOCKED
	{
		WITH_SFVALUEARRAY( data, 2 )
		{		
			m_pScaleformUI->ValueArray_SetElement( data, 0, headerText );
			m_pScaleformUI->ValueArray_SetElement( data, 1, voteText );
			if ( bShowingOtherTeam )
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showOtherTeamCastVote", data, 2 );
			else
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showCastVote", data, 2 );
		}
	}

	if ( !bShowingOtherTeam )
	{
		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
		if ( pLocalPlayer )
		{
			pLocalPlayer->EmitSound("Vote.Created");
		}
	}
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void SFHudVotePanel::OnThink( void )
{
	if ( !FlashAPIIsValid() )
		return;

	if ( !m_bVoteActive && m_flHideTime < 0 && m_flPostVotedHideTime < 0 )
		return;

	if ( m_bVoteActive && m_bHasFocus == false && GetHud().HudDisabled() == false )
	{
		// do not give focus to the vote panel until the HUD becomes visible
		m_bHasFocus = true;
		
		WITH_SLOT_LOCKED
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "focusCastVote", NULL, 0 );
		}
	}

	if ( m_bVoteActive && m_bVisible == false )
	{
		SetActive( true );
	}
	
	// We delay hiding the menu after we cast a vote
	if ( m_bPlayerVoted && m_flPostVotedHideTime > 0 && m_flPostVotedHideTime < gpGlobals->curtime )
	{		
		Hide();					
		m_flPostVotedHideTime = -1;
	}

	if ( !m_bVoteActive && m_flHideTime != -1 && m_flHideTime < gpGlobals->curtime )
	{
		Hide();
		m_flHideTime = -1;
	}	

	if ( m_flVoteResultCycleTime > 0 && m_flVoteResultCycleTime < gpGlobals->curtime )
	{		
		//[tj]Convert toScaleform animation g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( m_pVoteActive, "HideVoteBackgrounds" );

		m_flVoteResultCycleTime = -1;
		m_bPlayerVoted = false;
		m_bPlayerLocalVote = VOTE_UNCAST;
		m_bVoteActive = false;
	}

	if ( m_bIsYesNoVote )
	{
		if ( m_nVoteOptionCount[0] != m_nVoteYes )
		{
			m_nVoteYes = m_nVoteOptionCount[0];
		}

		if ( m_nVoteOptionCount[1] != m_nVoteNo )
		{
			m_nVoteNo = m_nVoteOptionCount[1];
		}

		UpdateYesNoButtonText();
	}	
}

void SFHudVotePanel::VoteYes( SCALEFORM_CALLBACK_ARGS_DECL )
{
	bool bEatKey = false;
	if ( m_bVoteActive )
	{
		if ( !m_bPlayerVoted )
		{
			engine->ClientCmd( "vote option1" );
			bEatKey = true;
		}
	}

	m_pScaleformUI->Params_SetResult( obj, bEatKey );
}

void SFHudVotePanel::VoteNo( SCALEFORM_CALLBACK_ARGS_DECL )
{
	bool bEatKey = false;
	if ( m_bVoteActive )
	{
		if ( !m_bPlayerVoted )
		{
			engine->ClientCmd( "vote option2" );
			bEatKey = true;
		}
	}

	m_pScaleformUI->Params_SetResult( obj, bEatKey );
}

void SFHudVotePanel::SetActive( bool bActive )
{
	if ( bActive != m_bVisible )
	{
		if ( FlashAPIIsValid() )
		{
			if ( bActive )
			{
				WITH_SLOT_LOCKED
				{
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showImmediate", NULL, 0 );
				}	
			}
			else
			{
				WITH_SLOT_LOCKED
				{
					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hideImmediate", NULL, 0 );
				}	
			}
		}

		m_bVisible = bActive;
	}

	CHudElement::SetActive( bActive );
}

#endif // INCLUDE_SCALEFORM
