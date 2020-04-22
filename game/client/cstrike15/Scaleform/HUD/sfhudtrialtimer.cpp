//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: [jpaquin] The "Player Two press start" widget
//
//=============================================================================//

#include "cbase.h"

#include "basepanel.h"
#include "scaleformui/scaleformui.h"
#include "iclientmode.h"
#include "clientmode_csnormal.h"
#include "sfhudflashinterface.h"
#include "vgui/ILocalize.h"
#include "VGuiMatSurface/IMatSystemSurface.h"

#if defined( _X360 )
#include "xbox/xbox_launch.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static  const float UPDATE_INTERVAL = 60.0f;

class SFHudTrialTimer : public SFHudFlashInterface
{
public:
	explicit SFHudTrialTimer( const char *value ) : SFHudFlashInterface( value ),
		m_fNextUpdate( 0.0f ),
		m_bVisible( false )
	{
		SetHiddenBits( /* HIDEHUD_MISCSTATUS */ 0 );
	}


	virtual ~SFHudTrialTimer()
	{
	}

	void ProcessInput( void )
	{
		if ( FlashAPIIsValid() )
		{
			if ( m_fNextUpdate < gpGlobals->curtime )
			{
				m_fNextUpdate = gpGlobals->curtime + UPDATE_INTERVAL;

				bool bUnlocked = false;
				float timeLeft = 0;


#if defined( _X360 )
				if ( xboxsystem )
				{
					bUnlocked = xboxsystem->IsArcadeTitleUnlocked();

					if ( !bUnlocked )
					{
						timeLeft = xboxsystem->GetArcadeRemainingTrialTime( m_iFlashSlot - SF_FIRST_SS_SLOT );
					}
				}
#else
				ConVarRef xbox_arcade_title_unlocked( "xbox_arcade_title_unlocked" );
				ConVarRef xbox_arcade_remaining_trial_time( "xbox_arcade_remaining_trial_time" );

				bUnlocked = xbox_arcade_title_unlocked.GetBool();
				timeLeft = xbox_arcade_remaining_trial_time.GetFloat();
#endif
				if ( !bUnlocked )
				{
					int minutesLeft = floorf( timeLeft / 60.0f );

					minutesLeft = MAX( minutesLeft, 0 );

					if ( minutesLeft == 1 )
					{
						WITH_SLOT_LOCKED
						{
							m_pTimerMessage->SetTextHTML( "#SFUI_TrialHudTextMinute" );
						}
					}
					else
					{
						char strTrialTime[16];
						Q_snprintf( strTrialTime, sizeof( strTrialTime ), "%d", minutesLeft );

						wchar_t buffer[128];
						wchar_t wTrialTime[16];
						g_pVGuiLocalize->ConvertANSIToUnicode( strTrialTime, wTrialTime, sizeof( wTrialTime ) );
						g_pVGuiLocalize->ConstructString( buffer, sizeof( buffer ), g_pVGuiLocalize->Find( "#SFUI_TrialHudTextMinutes" ), 1, wTrialTime );

						WITH_SLOT_LOCKED
						{
							m_pTimerMessage->SetTextHTML( buffer );
						}
					}
				}
			}
		}
	}

	void LevelInit( void )
	{
		if ( !FlashAPIIsValid() )
		{
			SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFHudTrialTimer, this, TrialTimer );
		}
	}

	virtual void LevelShutdown( void )
	{
		if ( FlashAPIIsValid() )
		{
			RemoveFlashElement();
		}
	}

	virtual void SetActive( bool bActive )
	{
		ShowPanel( bActive, false );
		CHudElement::SetActive( bActive );
	}

	virtual bool ShouldDraw( void )
	{
		bool result = cl_drawhud.GetBool();

#if defined( _X360 )
		if ( result && xboxsystem )
		{
			result = !( xboxsystem->IsArcadeTitleUnlocked() );
		}
#else
		ConVarRef xbox_arcade_title_unlocked( "xbox_arcade_title_unlocked" );
		result = result && !xbox_arcade_title_unlocked.GetBool();
#endif

		return result && CHudElement::ShouldDraw();
	}


	// these overload the ScaleformFlashInterfaceMixin class
	virtual void FlashReady( void )
	{
		SFVALUE panel = m_pScaleformUI->Value_GetMember( m_FlashAPI, "Panel" );

		if ( panel != NULL )
		{
			m_pTimerMessage = m_pScaleformUI->TextObject_MakeTextObjectFromMember( panel, "AnimatedText" );

			m_pScaleformUI->ReleaseValue( panel );
		}

		ShowPanel( m_bVisible, true );
	}


	virtual bool PreUnloadFlash( void )
	{
		SafeReleaseSFTextObject( m_pTimerMessage );
		return SFHudFlashInterface::PreUnloadFlash();
	}

protected:

	void ShowPanel( bool bShow, bool force )
	{
		if ( ( bShow != m_bVisible ) || force )
		{
			m_bVisible = bShow;

			if ( m_FlashAPI )
			{
				if  ( m_bVisible )
				{
					m_fNextUpdate = 0.0f;
					ProcessInput();
					WITH_SLOT_LOCKED
						m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowPanel", NULL, 0 );
				}
				else
				{
					WITH_SLOT_LOCKED
						m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "HidePanel", NULL, 0 );
				}
			}
		}
	}

protected:
	ISFTextObject*	m_pTimerMessage;
	float m_fNextUpdate;
	bool m_bVisible;

};

DECLARE_HUDELEMENT( SFHudTrialTimer );

SFUI_BEGIN_GAME_API_DEF
SFUI_END_GAME_API_DEF( SFHudTrialTimer, TrialTimer );

