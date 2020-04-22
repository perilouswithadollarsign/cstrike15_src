//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:  Displays HUD elements for medals/achievements, and hint text
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
#include "sfhudinfopanel.h"
#include "vgui/ILocalize.h"
#include "text_message.h"
#include "hud_macros.h"
#include "achievementmgr.h"
#include "fmtstr.h"
#include "sfhudfreezepanel.h"

#include "bannedwords.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

DECLARE_HUDELEMENT( SFHudInfoPanel );

DECLARE_HUD_MESSAGE( SFHudInfoPanel, HintText );
DECLARE_HUD_MESSAGE( SFHudInfoPanel, KeyHintText );

SFUI_BEGIN_GAME_API_DEF
SFUI_END_GAME_API_DEF( SFHudInfoPanel, HelpAchievementModule ); // Asset named HelpAchievementModule to maintain consistency with Flash file naming

// global tunables for this panel
static float g_HintDisplayTime = 6.f;
static float g_MedalDisplayTime = 5.f;

extern ConVar cl_draw_only_deathnotices;

SFHudInfoPanel::SFHudInfoPanel( const char *value ) : SFHudFlashInterface( value ),
	m_HelpPanelHandle(NULL),
	m_HelpBodyTextHandle(NULL),
	m_MedalPanelHandle(NULL),
	m_MedalTitleTextHandle(NULL),
	m_MedalBodyTextHandle(NULL),
	m_DefusePanelHandle(NULL),
	m_DefuseTitleTextHandle(NULL),
	m_DefuseBodyTextHandle(NULL),
	m_DefuseIconKit( NULL ),
	m_DefuseIconNoKit( NULL ),
	m_DefuseTimerTextHandle(NULL),
	m_PriorityMessagePanelHandle(NULL),
	m_PriorityMessageTitleTextHandle(NULL),
	m_PriorityMessageBodyTextHandle(NULL),
	m_activeAchievement(CSInvalidAchievement),
	m_PreviousDefusePercent(-1.0f),
	m_bDeferRaiseHelpPanel(false),
	m_bHintPanelHidden( false ),
	m_bDeferRaisePriorityMessagePanel(false),
	m_bIsVisible(false)
{
	HOOK_HUD_MESSAGE( SFHudInfoPanel, HintText );
	HOOK_HUD_MESSAGE( SFHudInfoPanel, KeyHintText );
	SetHiddenBits( HIDEHUD_MISCSTATUS );

	m_HintDisplayTimer.Invalidate();
	m_AchievementDisplayTimer.Invalidate();
	m_PriorityMsgDisplayTimer.Invalidate();
}

SFHudInfoPanel::~SFHudInfoPanel()
{
}

void SFHudInfoPanel::ShowPanel( HUDINFO_TYPE panelType, bool value )
{
	if ( m_bActive && m_FlashAPI )
	{
		WITH_SLOT_LOCKED
		{
			ShowPanelNoLock( panelType, value );
		}
	}
}

// Caution! If you call this from code that isn't wrapped with Slot Locks, you will run into run-time multi-threading issues!
void SFHudInfoPanel::ShowPanelNoLock( HUDINFO_TYPE panelType, bool value )
{
	if ( m_bActive && FlashAPIIsValid() )
	{
		WITH_SFVALUEARRAY( data, 1 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, panelType );

			if ( value )
			{
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", data, 1 );
				m_bIsVisible = true;
			}
			else
			{
				if ( panelType == SFHUDINFO_PriorityMessage ||
					 panelType == SFHUDINFO_Help )
				{
					STEAMWORKS_TESTSECRET_AMORTIZE( 149 );
				}

				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", data, 1 );
				m_bIsVisible = false;
			}
		}
	}
	else
	{
		// Set the flags so that panels will be raised once this HUD element is visible and loaded
		if ( value )
		{
			if ( panelType == SFHUDINFO_Help )
			{
				m_bDeferRaiseHelpPanel = true;
			}
			else if ( panelType == SFHUDINFO_PriorityMessage )
			{
				m_bDeferRaisePriorityMessagePanel = true;
			}
		}
	}
}

void SFHudInfoPanel::HideAll( void )
{
	if ( m_FlashAPI )
	{
		WITH_SLOT_LOCKED
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hideAll", NULL, 0 );
		}
		m_bIsVisible = false;

		// Reset the defuse percent so that we detect the defuse panel needs to be shown again when we un-hide this HUD element
		m_PreviousDefusePercent = -1.0f;
	}
}

void SFHudInfoPanel::LockSlot( bool wantItLocked, bool& currentlyLocked )
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

void SFHudInfoPanel::ProcessInput( void )
{
	// Collect information about defuse progress
	float DefusePercent = -1.0f;
	int	  DefuseTimeRemaining = -1;
	bool  bDefuseCanceled = false;

	C_CSPlayer *pPlayer = C_CSPlayer::GetLocalCSPlayer();
	bool bSpectating = ( pPlayer && ( pPlayer->GetObserverMode() == OBS_MODE_IN_EYE || pPlayer->GetObserverMode() == OBS_MODE_CHASE ) );

	if ( pPlayer && bSpectating )
	{
		C_BaseEntity *pTarget = pPlayer->GetObserverTarget();

		if ( pTarget && pTarget->IsPlayer() )
		{
			pPlayer = ToCSPlayer( pTarget );

			if ( pPlayer && !pPlayer->IsAlive() )
			{
				pPlayer = NULL;
			}
		}
		else
		{
			pPlayer = NULL;
		}
	}

	if ( pPlayer )
	{
		bDefuseCanceled = (pPlayer->m_iProgressBarDuration == 0);

		if ( pPlayer->m_iProgressBarDuration > 0 )
		{
			// ProgressBarStartTime is now with respect to m_flSimulationTime rather than local time
			DefusePercent = (pPlayer->m_flSimulationTime - pPlayer->m_flProgressBarStartTime) / (float)pPlayer->m_iProgressBarDuration;
			DefusePercent = clamp( DefusePercent, 0.f, 1.f );

			DefuseTimeRemaining =  static_cast<int>( ceil( (float)pPlayer->m_iProgressBarDuration - (pPlayer->m_flSimulationTime - pPlayer->m_flProgressBarStartTime) ) );
		}
	}
	else
	{
		// if the player goes away (drops from server, dies, etc) remember to clear any defuse panel for them
		if ( m_PreviousDefusePercent >= 0.f )
			bDefuseCanceled = true;
	}


	// Updating flash, slot locking begins
	bool bSlotIsLocked = false;

	if ( m_HintDisplayTimer.HasStarted() )
	{
		// Check if the current hint text should go away
		if ( m_HintDisplayTimer.IsElapsed() )
		{
			LockSlot( true, bSlotIsLocked );

			// clear the hint, which also clears our timer
			SetHintText(NULL);
			m_bHintPanelHidden = false;
		}
		else if ( m_bHintPanelHidden && m_PriorityMsgDisplayTimer.IsElapsed() )
		{
			// Hint message was defered while a priority message was shown. Bring it back.
			LockSlot( true, bSlotIsLocked );
			m_bHintPanelHidden = false;
			ShowPanelNoLock( SFHUDINFO_Help, true );			
		}
		else if ( m_bActive && m_bDeferRaiseHelpPanel )
		{
			// The help panel was triggered before the HUD was visible: raise it now
			LockSlot( true, bSlotIsLocked );

			ShowPanelNoLock( SFHUDINFO_Help, true );
		}
	}

	if ( m_PriorityMsgDisplayTimer.HasStarted() )
	{
		// Check if the priority message text should go away
		if ( m_PriorityMsgDisplayTimer.IsElapsed() )
		{
			LockSlot( true, bSlotIsLocked );

			// clear the hint, which also clears our timer
			SetPriorityText( static_cast<wchar_t*>(NULL) );
		}
		else if ( m_bActive && m_bDeferRaisePriorityMessagePanel )
		{
			// The priority message panel was triggered before the HUD was visible: raise it now
			LockSlot( true, bSlotIsLocked );

			ShowPanelNoLock( SFHUDINFO_PriorityMessage, true );
		}
	}

	// Update the defuse UI
	if ( DefusePercent >= 0.0f )
	{
		LockSlot( true, bSlotIsLocked );

		// Update the timer text and progress bar
		if ( DefuseTimeRemaining >= 0 && m_DefuseTimerTextHandle )
		{
			char cTimerStr[ 128 ];
			int  clampedTime = MAX( DefuseTimeRemaining, 0 );

			Q_snprintf( cTimerStr, sizeof(cTimerStr), "%02d:%02d", ( clampedTime / 60 ), ( clampedTime % 60 ) );
			m_pScaleformUI->Value_SetText( m_DefuseTimerTextHandle, cTimerStr );
		}

		if ( DefusePercent >= 0.f )
		{
			WITH_SFVALUEARRAY( data, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, DefusePercent);
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setDefuseProgress", data, 1 );
			}
		}
	}

	if ( m_PreviousDefusePercent < 0.f && DefusePercent >= 0.f )
	{
		LockSlot( true, bSlotIsLocked );

		// activated the defuse process: set our static text once, and then show the panel
		if ( m_DefuseTitleTextHandle )
		{
			if ( pPlayer->m_bIsGrabbingHostage )
				m_pScaleformUI->Value_SetText( m_DefuseTitleTextHandle, "#SFUIHUD_InfoPanel_HostageTitle" );
			else
				m_pScaleformUI->Value_SetText( m_DefuseTitleTextHandle, "#SFUIHUD_InfoPanel_DefuseTitle" );
		}

		if ( m_DefuseBodyTextHandle )
		{
			if ( bSpectating )
			{
				C_CSPlayer *pTargetPlayer = ToCSPlayer( pPlayer->GetObserverTarget() );

				if ( pTargetPlayer )
				{
					wchar_t wszLocalized[100];
					wchar_t wszPlayerName[MAX_PLAYER_NAME_LENGTH];
					g_pVGuiLocalize->ConvertANSIToUnicode( pTargetPlayer->GetPlayerName(), wszPlayerName, sizeof(wszPlayerName) );
					
					if ( pTargetPlayer->HasDefuser() )
						g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#SFUIHUD_InfoPanel_Spec_DefuseText" ), 1, wszPlayerName );
					else
						g_pVGuiLocalize->ConstructString( wszLocalized, sizeof( wszLocalized ), g_pVGuiLocalize->Find( "#SFUIHUD_InfoPanel_Spec_DefuseText_NoKit" ), 1, wszPlayerName );

					m_pScaleformUI->Value_SetText( m_DefuseBodyTextHandle, wszLocalized );
				}
				else
				{
					m_pScaleformUI->Value_SetText( m_DefuseBodyTextHandle, "" );
				}
			}
			else
			{
				if ( pPlayer->m_bIsGrabbingHostage )
					m_pScaleformUI->Value_SetText( m_DefuseBodyTextHandle, "#SFUIHUD_InfoPanel_HostageText" );
				else if ( pPlayer->HasDefuser() )
					m_pScaleformUI->Value_SetText( m_DefuseBodyTextHandle, "#SFUIHUD_InfoPanel_DefuseText" );
				else
					m_pScaleformUI->Value_SetText( m_DefuseBodyTextHandle, "#SFUIHUD_InfoPanel_DefuseText_NoKit" );
			}
		}

		if ( m_DefuseIconKit && m_DefuseIconNoKit )
		{
			bool bHasDefuser = pPlayer->HasDefuser();

			m_pScaleformUI->Value_SetVisible( m_DefuseIconKit, bHasDefuser );
			m_pScaleformUI->Value_SetVisible( m_DefuseIconNoKit, !bHasDefuser );
		}

		ShowPanelNoLock( SFHUDINFO_Defuse, true );
	}

	if ( m_PreviousDefusePercent >= 0.f && bDefuseCanceled )
	{
		LockSlot( true, bSlotIsLocked );

		// stopped defusing: hide the panel
		ShowPanelNoLock( SFHUDINFO_Defuse, false );
	}

	m_PreviousDefusePercent = DefusePercent;


	// Update current achievement UI
	if ( m_activeAchievement != CSInvalidAchievement )
	{
		if ( m_AchievementDisplayTimer.HasStarted() && m_AchievementDisplayTimer.IsElapsed() )
		{
			LockSlot( true, bSlotIsLocked );

			m_AchievementDisplayTimer.Invalidate();

			// start the hide process on the panel
			ShowPanelNoLock( SFHUDINFO_Medal, false );
		}
		else
		{
			// Once the panel is fully gone, clear the active achievement so we can display the next one
			if ( m_MedalPanelHandle )
			{
				LockSlot( true, bSlotIsLocked );

				ScaleformDisplayInfo dinfo;
				m_pScaleformUI->Value_GetDisplayInfo( m_MedalPanelHandle, &dinfo );

				if ( !dinfo.GetVisibility() )
				{
					m_activeAchievement = CSInvalidAchievement;
				}
			}
		}
	}
	else if ( m_achievementQueue.Count() > 0 )
	{
		// Grab the next queued achievement and pop it up
		AchivementQueueInfo queueInfo = m_achievementQueue.RemoveAtHead();

		m_AchievementDisplayTimer.Start( g_MedalDisplayTime );

		m_activeAchievement = queueInfo.type;

		// [dwenger] Play the achievement earned sound effect
		vgui::surface()->PlaySound( "UI/achievement_earned.wav" );

		// Here we get the achievement to be displayed and set that in the popup windows
		IAchievementMgr *pAchievementMgr = engine->GetAchievementMgr();
		if ( !pAchievementMgr )
			return;

		IAchievement *pAchievement = pAchievementMgr->GetAchievementByID( m_activeAchievement, queueInfo.playerSlot );
		if ( pAchievement )
		{
			LockSlot( true, bSlotIsLocked );

			if ( m_MedalTitleTextHandle )
			{
				m_pScaleformUI->Value_SetText( m_MedalTitleTextHandle, ACHIEVEMENT_LOCALIZED_NAME( pAchievement ) );
			}

			if ( m_MedalBodyTextHandle )
			{
				// not showing the text for right now because the body field isn't incorperated into the design, will address later
				m_pScaleformUI->Value_SetText( m_MedalBodyTextHandle, "" );
				//m_pScaleformUI->Value_SetText( m_MedalBodyTextHandle, ACHIEVEMENT_LOCALIZED_DESC( pAchievement ) );
			}

			// Notify the panel of the achievement name, so we can display the appropriate icon (icon name MUST match the achievement short name, eg. "ENEMY_KILL_HIGH", "SAME_UNIFORM")
			WITH_SFVALUEARRAY( data, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, pAchievement->GetName());
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setMedalAnnouncement", data, 1 );
			}

			// Achievements and medals always take precedent over hints / priority messages.
			if ( m_PriorityMsgDisplayTimer.HasStarted() )
			{
				if ( m_bHintPanelHidden )
				{
					m_bHintPanelHidden = false;
					m_HintDisplayTimer.Invalidate();
				}

				SetPriorityText( static_cast<wchar_t*>(NULL) );
			}

			if ( m_HintDisplayTimer.HasStarted() )
			{
				SetHintText( NULL );
			}

			ShowPanelNoLock( SFHUDINFO_Medal, true );
		}
	}

	LockSlot( false, bSlotIsLocked );

	// Clear any transient data now
	if ( m_bActive )
	{
		m_bDeferRaiseHelpPanel = false;
		m_bDeferRaisePriorityMessagePanel = false;
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

void SFHudInfoPanel::FlashReady( void )
{
	ListenForGameEvent( "achievement_earned_local" );

	m_HelpPanelHandle = m_pScaleformUI->Value_GetMember( m_FlashAPI, "HudPanelHelp" );

	if ( m_HelpPanelHandle )
	{
		SFVALUE AnimatedPanelHandle = m_pScaleformUI->Value_GetMember( m_HelpPanelHandle, "Panel" );

		if ( AnimatedPanelHandle )
		{
			GetTextBoxForElement( m_pScaleformUI, AnimatedPanelHandle, "HelpText", "TextBox", m_HelpBodyTextHandle );

			m_pScaleformUI->ReleaseValue( AnimatedPanelHandle );
		}
	}

	m_PriorityMessagePanelHandle = m_pScaleformUI->Value_GetMember( m_FlashAPI, "HudPanelCenter" );

	if ( m_PriorityMessagePanelHandle )
	{
		SFVALUE AnimatedPanelHandle = m_pScaleformUI->Value_GetMember( m_PriorityMessagePanelHandle, "Panel" );

		if ( AnimatedPanelHandle )
		{
			GetTextBoxForElement( m_pScaleformUI, AnimatedPanelHandle, "CenterTextTitle", "TextBox", m_PriorityMessageTitleTextHandle );
			GetTextBoxForElement( m_pScaleformUI, AnimatedPanelHandle, "CenterText", "TextBox", m_PriorityMessageBodyTextHandle );

			m_pScaleformUI->ReleaseValue( AnimatedPanelHandle );
		}
	}

	m_MedalPanelHandle = m_pScaleformUI->Value_GetMember( m_FlashAPI, "HudPanelMedal" );

	if ( m_MedalPanelHandle )
	{
		SFVALUE AnimatedPanelHandle = m_pScaleformUI->Value_GetMember( m_MedalPanelHandle, "Panel" );

		if ( AnimatedPanelHandle )
		{
			GetTextBoxForElement( m_pScaleformUI, AnimatedPanelHandle, "MedalTitleText", "TextBox", m_MedalTitleTextHandle );
			GetTextBoxForElement( m_pScaleformUI, AnimatedPanelHandle, "MedalText", "TextBox", m_MedalBodyTextHandle );

			m_pScaleformUI->ReleaseValue( AnimatedPanelHandle );
		}
	}

	m_DefusePanelHandle = m_pScaleformUI->Value_GetMember( m_FlashAPI, "HudPanelDefuse" );

	if ( m_DefusePanelHandle )
	{
		SFVALUE AnimatedPanelHandle = m_pScaleformUI->Value_GetMember( m_DefusePanelHandle, "Panel" );

		m_DefuseIconKit = m_pScaleformUI->Value_GetMember( m_DefusePanelHandle, "icon_defuse" );
		m_DefuseIconNoKit = m_pScaleformUI->Value_GetMember( m_DefusePanelHandle, "icon_no_defusekit" );

		if ( AnimatedPanelHandle )
		{
			GetTextBoxForElement( m_pScaleformUI, AnimatedPanelHandle, "DefuseText", "TextBox", m_DefuseBodyTextHandle );
			
			SFVALUE TitleBarHandle = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "DefuseTextTitle" );

			if ( TitleBarHandle )
			{
				SFVALUE TitleTextHandle = m_pScaleformUI->Value_GetMember( TitleBarHandle, "DefuseTitle" );

				if ( TitleTextHandle )
				{
					GetTextBoxForElement( m_pScaleformUI, TitleTextHandle, "DefuseTitle", "TextBox1", m_DefuseTitleTextHandle );

					GetTextBoxForElement( m_pScaleformUI, TitleTextHandle, "DefuseTitle", "TextBox2", m_DefuseTimerTextHandle );

					m_pScaleformUI->ReleaseValue( TitleTextHandle );
				}

				m_pScaleformUI->ReleaseValue( TitleBarHandle );
			}

			m_pScaleformUI->ReleaseValue( AnimatedPanelHandle );
		}
	}

	// hide everything initially
	HideAll();
}

bool SFHudInfoPanel::PreUnloadFlash( void )
{
	StopListeningForAllEvents();

	SafeReleaseSFVALUE( m_HelpPanelHandle );
	SafeReleaseSFVALUE( m_HelpBodyTextHandle );

	SafeReleaseSFVALUE( m_MedalPanelHandle );
	SafeReleaseSFVALUE( m_MedalTitleTextHandle );
	SafeReleaseSFVALUE( m_MedalBodyTextHandle );

	SafeReleaseSFVALUE( m_DefusePanelHandle );
	SafeReleaseSFVALUE( m_DefuseTitleTextHandle );
	SafeReleaseSFVALUE( m_DefuseTimerTextHandle );
	SafeReleaseSFVALUE( m_DefuseBodyTextHandle );

	SafeReleaseSFVALUE( m_DefuseIconKit );
	SafeReleaseSFVALUE( m_DefuseIconNoKit );

	SafeReleaseSFVALUE( m_PriorityMessagePanelHandle );
	SafeReleaseSFVALUE( m_PriorityMessageTitleTextHandle );
	SafeReleaseSFVALUE( m_PriorityMessageBodyTextHandle );
	
	return true;
}

void SFHudInfoPanel::LevelInit( void )
{
	if ( !FlashAPIIsValid() )
	{
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFHudInfoPanel, this, HelpAchievementModule );
	}
	else
	{
		// When initially loaded, hide this panel
		HideAll();
	}
}

void SFHudInfoPanel::LevelShutdown( void )
{
	if ( FlashAPIIsValid() )
	{
		RemoveFlashElement();
	}
}

void SFHudInfoPanel::Reset( void )
{
	HideAll();
}

bool SFHudInfoPanel::ShouldDraw( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	return cl_drawhud.GetBool() && cl_draw_only_deathnotices.GetBool() == false && CHudElement::ShouldDraw();
}


void SFHudInfoPanel::SetActive( bool bActive )
{
	if ( m_bActive && !bActive )
	{
		HideAll();
	}

	CHudElement::SetActive( bActive );
}

bool SFHudInfoPanel::MsgFunc_HintText( const CCSUsrMsg_HintText &msg )
{
	const char *tmpStr = hudtextmessage->LookupString( msg.text().c_str(), NULL );
	LocalizeAndDisplay( tmpStr, tmpStr );
	return true;
}

bool SFHudInfoPanel::MsgFunc_KeyHintText( const CCSUsrMsg_KeyHintText &msg )
{
	const char *tmpStr = hudtextmessage->LookupString( msg.hints(0).c_str(), NULL );
	LocalizeAndDisplay( tmpStr, tmpStr );
	return true;
}

void SFHudInfoPanel::LocalizeAndDisplay( const char *pszHudTxtMsg, const char *szRawString )
{
	wchar_t szBuf[255];
	wchar_t *pszBuf;

	// init buffers & pointers
	szBuf[0] = 0;
	pszBuf = szBuf;

	// try to localize
	if ( pszHudTxtMsg )
	{
		pszBuf = g_pVGuiLocalize->Find( pszHudTxtMsg );
	}
	else
	{
		pszBuf = g_pVGuiLocalize->Find( szRawString );
	}

	if ( !pszBuf )
	{
		// use plain ASCII string 
		g_pVGuiLocalize->ConvertANSIToUnicode( szRawString, szBuf, sizeof(szBuf) );
		pszBuf = szBuf;
	}

	// replace key binding text
	wchar_t keyBindingBuf[512];
	UTIL_ReplaceKeyBindings( pszBuf, sizeof( szBuf ), keyBindingBuf, sizeof( keyBindingBuf ) );

	// make it visible
	SetHintText( pszBuf );
}

bool SFHudInfoPanel::SetHintText( wchar_t *text )
{
	if ( FlashAPIIsValid() && !m_AchievementDisplayTimer.HasStarted() )
	{
		WITH_SLOT_LOCKED
		{
			if ( text )
			{
				if ( m_HelpBodyTextHandle )
				{
					WITH_SLOT_LOCKED
					{
						m_pScaleformUI->Value_SetTextHTML( m_HelpBodyTextHandle, m_pScaleformUI->ReplaceGlyphKeywordsWithHTML( text ) );
					}
				}

				if ( m_PriorityMsgDisplayTimer.HasStarted() )
				{
					// Defer display of the hint message until the priority message has finished displaying
					m_bHintPanelHidden = true;
					m_HintDisplayTimer.Start( g_HintDisplayTime + m_PriorityMsgDisplayTimer.GetRemainingTime() );
				}
				else
				{
					// start (or reset) the timer for auto-hiding this latest hint
					m_HintDisplayTimer.Start( g_HintDisplayTime );
					ShowPanelNoLock( SFHUDINFO_Help, true );
				}
			}
			else
			{
				m_HintDisplayTimer.Invalidate();

				ShowPanelNoLock( SFHUDINFO_Help, false );
			}
		}
	}

	return true;
}

#ifndef _GAMECONSOLE
ConVar cl_display_scaleform_achievement_popups( "cl_display_scaleform_achievement_popups", "0", FCVAR_CLIENTDLL );
#else
ConVar cl_display_scaleform_achievement_popups( "cl_display_scaleform_achievement_popups", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE );
#endif

void SFHudInfoPanel::FireGameEvent( IGameEvent * event )
{
	const char *pEventName = event->GetName();

	if ( cl_display_scaleform_achievement_popups.GetBool() )
	{
		if ( Q_strcmp( "achievement_earned_local", pEventName ) == 0 )
		{
			AchivementQueueInfo queueInfo;
			queueInfo.type = (eCSAchievementType)event->GetInt( "achievement" );
			queueInfo.playerSlot = event->GetInt( "splitscreenplayer" );
		
			// If this achievement is for this player, enqueue it to display on the next tick
			if ( queueInfo.playerSlot == engine->GetActiveSplitScreenPlayerSlot() )
			{
				m_achievementQueue.Insert(queueInfo); 
			}
		}
	}
}

// Common code to setup the priority text window for a new message, or tear it down if the message is cleared
void SFHudInfoPanel::ModifyPriorityTextWindow( bool bMsgSet )
{
	if ( bMsgSet )
	{
		bool bAlreadyActive = m_PriorityMsgDisplayTimer.HasStarted() && !m_PriorityMsgDisplayTimer.IsElapsed();

		// start (or reset) the timer for auto-hiding this latest text
		static ConVarRef scr_centertime( "scr_centertime" );
		m_PriorityMsgDisplayTimer.Start( scr_centertime.GetFloat() );

		if ( m_PriorityMessageTitleTextHandle )
		{
			WITH_SLOT_LOCKED
			{
				m_pScaleformUI->Value_SetText( m_PriorityMessageTitleTextHandle, "#SFUIHUD_InfoPanel_PriorityMsgTitle" );
			}
		}

		if ( bAlreadyActive )
		{
			WITH_SLOT_LOCKED
			{
				// don't re-animate the window into position, just flash it and swap the text
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "flashCenterText", NULL, 0 );
			}
		}
		else
		{
			if ( m_HintDisplayTimer.HasStarted() )
			{
				// Priority messages take precedent. Temporarily hide the hint panel.
				m_bHintPanelHidden = true;
				m_HintDisplayTimer.Start( m_HintDisplayTimer.GetElapsedTime() + scr_centertime.GetFloat() );
				ShowPanelNoLock( SFHUDINFO_Help, false );
			}

			ShowPanelNoLock( SFHUDINFO_PriorityMessage, true );
		}
	}
	else
	{
		m_PriorityMsgDisplayTimer.Invalidate();

		ShowPanelNoLock( SFHUDINFO_PriorityMessage, false );
	}
}

// ANSI C-string version
void SFHudInfoPanel::SetPriorityText( char *pMsg )
{
	if ( FlashAPIIsValid() && !m_AchievementDisplayTimer.HasStarted() )
	{
		WITH_SLOT_LOCKED
		{
			ModifyPriorityTextWindow( (pMsg != NULL) );
			
			if ( pMsg && m_PriorityMessageBodyTextHandle )
			{
				if ( g_BannedWords.BInitialized() )
				{
					int nLen = V_strlen( pMsg );
					int cubDestSizeInBytes = ( 1 + nLen ) * sizeof( wchar_t );
					wchar_t * pwchBuffer = ( wchar_t * ) stackalloc( cubDestSizeInBytes );
					V_UTF8ToUnicode( pMsg, pwchBuffer, cubDestSizeInBytes );
					if ( g_BannedWords.CensorBannedWordsInplace( pwchBuffer ) )
					{
						m_pScaleformUI->Value_SetText( m_PriorityMessageBodyTextHandle, pwchBuffer );
						pMsg = NULL;
					}
				}
				if ( pMsg )
				{
					m_pScaleformUI->Value_SetText( m_PriorityMessageBodyTextHandle, pMsg );
				}
			}
		}
	}
}

// TCHAR (multi-byte) string version
void SFHudInfoPanel::SetPriorityText( wchar_t *pMsg )
{
	if ( FlashAPIIsValid() && !m_AchievementDisplayTimer.HasStarted() )
	{
		WITH_SLOT_LOCKED
		{
			ModifyPriorityTextWindow( (pMsg != NULL) );

			if ( pMsg && m_PriorityMessageBodyTextHandle )
			{
				g_BannedWords.CensorBannedWordsInplace( pMsg );
				m_pScaleformUI->Value_SetTextHTML( m_PriorityMessageBodyTextHandle, m_pScaleformUI->ReplaceGlyphKeywordsWithHTML( pMsg ) );
			}
		}
	}
}

void SFHudInfoPanel::SetPriorityHintText( wchar_t *pMsg )
{
	if ( FlashAPIIsValid() && !m_AchievementDisplayTimer.HasStarted() )
	{
		WITH_SLOT_LOCKED
		{
			//m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "flashCenterText", NULL, 0 );

			if ( pMsg )
			{
				// make it visible
				g_BannedWords.CensorBannedWordsInplace( pMsg );
				SetHintText( pMsg );
			}
		}
	}
}

void SFHudInfoPanel::ApplyYOffset( int nOffset )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SFVALUEARRAY_SLOT_LOCKED( data, 1 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, nOffset );
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setYOffset", data, 1 );
		}
	}
}
