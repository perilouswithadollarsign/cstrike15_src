//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//

//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "hud_savestatus.h"
#include "iclientmode.h"
#include "view.h"
#include "vgui_controls/Controls.h"
#include "vgui_controls/EditablePanel.h"
#include "engineinterface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Actually must access console primary user id
#if defined( _GAMECONSOLE ) && defined( XBX_GetPrimaryUserId )
#undef XBX_GetPrimaryUserId
#endif

using namespace vgui;

DECLARE_HUDELEMENT( CHudSaveStatus );

#define MIN_HOLD_TIME	1.0f
#define FADE_OUT_TIME	1.0f

CHudSaveStatus::CHudSaveStatus( const char *pElementName ) :
  CHudElement( pElementName ), BaseClass( NULL, "HudSaveStatus" )
{
	vgui::Panel *pParent = GetClientMode()->GetViewport();
	SetParent( pParent );

	SetScheme( "basemodui_scheme" );
	SetProportional( true );

	m_pSavingIcon = NULL;
	m_pSavingLabel = NULL;
	m_pSavedLabel = NULL;

	m_flLastAnimTime = 0;
	m_flFadeOutTime = 0;
	m_flSaveStartedTime = 0;

	m_bNeedsDraw = false;
	m_bIsSteamProfileSave = false;
}

void CHudSaveStatus::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "resource/ui/hud_saving.res" );

	m_pSavingIcon = dynamic_cast< vgui::ImagePanel* >( FindChildByName( "SavingIcon" ) );

	m_pSavingLabel = dynamic_cast< vgui::Label* >( FindChildByName( "SavingLabel" ) );
	if ( m_pSavingLabel )
	{
		m_pSavingLabel->SetVisible( false );
	}

	m_pSavedLabel = dynamic_cast< vgui::Label* >( FindChildByName( "SavedLabel" ) );
	if ( m_pSavedLabel )
	{
		m_pSavedLabel->SetVisible( false );
	}

	// need to keep state consistent, ApplySchemeSettings()
	// will get fired after ShouldDraw() in Split Screen
	SetSavingLabels( !m_bIsSteamProfileSave );

	SetPaintBackgroundEnabled( false );

	SetVisible( false );
}

void CHudSaveStatus::SetSavingLabels( bool bIsGameSave )
{
	const char *pSavingLabel = bIsGameSave ? "#SFUI_Hud_SavingGame" : "#SFUI_Hud_SavingProfile";
	const char *pSavedLabel =  bIsGameSave ? "#SFUI_Hud_GameSaved" : "#SFUI_Hud_ProfileSaved";
	
	if ( m_pSavingLabel )
	{
		m_pSavingLabel->SetText( pSavingLabel );
	}
	if ( m_pSavedLabel )
	{
		m_pSavedLabel->SetText( pSavedLabel );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Save CPU cycles by letting the HUD system early cull
// costly traversal.  Called per frame, return true if thinking and 
// painting need to occur.
//-----------------------------------------------------------------------------
bool CHudSaveStatus::ShouldDraw()
{
#ifdef _GAMECONSOLE
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	int nSlot = GET_ACTIVE_SPLITSCREEN_SLOT();
	if ( XBX_GetUserId( nSlot ) != ( int )XBX_GetPrimaryUserId() )
	{
		return false;
	}
#endif

	bool bNeedsDraw = false;

	if ( m_flSaveStartedTime && Plat_FloatTime() < m_flSaveStartedTime + MIN_HOLD_TIME )
	{
		// force the element to draw
		bNeedsDraw = true;
	}

	// notify on autosaves, except autosavedangerous
	// autosavedangerous notifications are suppressed until marked safe, and might get discarded
	if ( engine->IsSaveInProgress() && 
		!engine->IsAutoSaveDangerousInProgress() && 
		!( IsPC() && engine->IsAutoSaveInProgress() ) ) 
	{
		bool bPrimaryUserIsGuest = false;
#if defined( _GAMECONSOLE )
		bPrimaryUserIsGuest = ( XBX_GetPrimaryUserIsGuest() != 0 );
#endif
		int iController = XBX_GetActiveUserId();
		DWORD nStorageDevice = XBX_GetStorageDeviceId( iController );
		bool bHasStorageDevice = ( XBX_DescribeStorageDevice( nStorageDevice ) != 0 );

		// a guest has no storage, the in-memory saves aren't shown to prevent player confusion as they have no access to the save
		if ( !bPrimaryUserIsGuest && bHasStorageDevice )
		{
			bNeedsDraw = true;
		}
	}

#if defined( _PS3 )
	bool bIsSteamProfileSave = false;
	bool bPS3SaveUtilBusy = ps3saveuiapi->IsSaveUtilBusy();
	if ( bPS3SaveUtilBusy )
	{
		uint32 nOpTag = ps3saveuiapi->GetCurrentOpTag();
		if ( nOpTag == kSAVE_TAG_WRITE_STEAMINFO )
		{
			bNeedsDraw = true;
			bIsSteamProfileSave = true;
		}
	}

	if ( bPS3SaveUtilBusy && ( m_bIsSteamProfileSave != bIsSteamProfileSave ) )
	{
		// change to correct label
		m_bIsSteamProfileSave = bIsSteamProfileSave;
		SetSavingLabels( !m_bIsSteamProfileSave );
	}
#endif

	if ( m_bNeedsDraw != bNeedsDraw )
	{
		// transition change
		m_bNeedsDraw = bNeedsDraw;

		if ( m_bNeedsDraw && !m_flSaveStartedTime )
		{
			// ensure we hold the start state for a minimum
			m_flSaveStartedTime = Plat_FloatTime();
		}
		else if ( !m_bNeedsDraw )
		{
			// clear for next time
			m_flSaveStartedTime = 0;
		}

		// the labels go from Saving...
		// to Saved.
		if ( m_pSavingLabel )
		{
			m_pSavingLabel->SetVisible( m_bNeedsDraw );
		}
		if ( m_pSavedLabel )
		{
			m_pSavedLabel->SetVisible( !m_bNeedsDraw );
		}

		if ( m_bNeedsDraw )
		{
			// stop any prior fading
			m_flFadeOutTime = 0;
		}
		else if ( !m_flFadeOutTime )
		{
			// trigger the fade out
			m_flFadeOutTime = Plat_FloatTime();
		}
	}

	if ( m_flFadeOutTime && Plat_FloatTime() < m_flFadeOutTime + FADE_OUT_TIME )
	{
		// keep drawing during the fade out
		bNeedsDraw = true;
	}

	// respect the hud request to hide (likely a pause)
	if ( !CHudElement::ShouldDraw() )
	{
		bNeedsDraw = false;
		if ( !m_bNeedsDraw && m_flFadeOutTime )
		{
			// stop any mid stream fade out that was occurring
			// to prevent a jarring pop if they pause/unpause
			// the element was going away anyways
			m_flFadeOutTime = 0;
		}
	}

#if defined( _PS3 )
	if ( !bNeedsDraw && m_bIsSteamProfileSave )
	{
		m_bIsSteamProfileSave = false;
		SetSavingLabels( !m_bIsSteamProfileSave );
	}
#endif

	return bNeedsDraw;
}

void CHudSaveStatus::OnThink()
{
	if ( m_pSavingIcon )
	{
		// clock the anim at 10hz
		float time = Plat_FloatTime();
		if ( ( m_flLastAnimTime + 0.1f ) < time )
		{
			m_flLastAnimTime = time;
			m_pSavingIcon->SetFrame( m_pSavingIcon->GetFrame() + 1 );
		}
	}

	float flFade = 1.0f;
	if ( m_flFadeOutTime )
	{
		flFade = RemapValClamped( Plat_FloatTime(), m_flFadeOutTime, m_flFadeOutTime + FADE_OUT_TIME, 1.0f, 0.0f );
	}
	SetAlpha( flFade * 255.0f );
}

void CHudSaveStatus::SaveStarted()
{
	// external event to force the hud element to draw
	// will automatically time out and go away
	if ( !m_flSaveStartedTime )
	{
		m_flSaveStartedTime = Plat_FloatTime();
	}
}