//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include "overwatchresolution_scaleform.h"
#include "game/client/iviewport.h"
#include "basepanel.h"
#include "iclientmode.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

SFHudOverwatchResolutionPanel* SFHudOverwatchResolutionPanel::m_pInstance = NULL;

SFUI_BEGIN_GAME_API_DEF		
	SFUI_DECL_METHOD( HideFromScript ),
SFUI_END_GAME_API_DEF( SFHudOverwatchResolutionPanel, OverwatchResolution );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
SFHudOverwatchResolutionPanel::SFHudOverwatchResolutionPanel()
{		
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );
}

SFHudOverwatchResolutionPanel::~SFHudOverwatchResolutionPanel()
{
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
}

void SFHudOverwatchResolutionPanel::OnEvent( KeyValues *pEvent )
{
	/* Removed for partner depot */
}

void SFHudOverwatchResolutionPanel::LoadDialog( void )
{
	if ( !m_pInstance )
	{
		m_pInstance = new SFHudOverwatchResolutionPanel();
		SFUI_REQUEST_ELEMENT( SF_FULL_SCREEN_SLOT, g_pScaleformUI, SFHudOverwatchResolutionPanel, m_pInstance, OverwatchResolution );	
	}
}

void SFHudOverwatchResolutionPanel::UnloadDialog( void )
{
	if ( m_pInstance )
	{
		m_pInstance->RemoveFlashElement();
	}
}

void SFHudOverwatchResolutionPanel::PostUnloadFlash( void )
{
	m_pInstance = NULL;
	delete this;
}

void SFHudOverwatchResolutionPanel::FlashReady( void )
{
	Show();
}

void SFHudOverwatchResolutionPanel::Show()
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", NULL, 0 );
		}
	}
}

void SFHudOverwatchResolutionPanel::Hide( void )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", 0, NULL );
		}
	}
}

void SFHudOverwatchResolutionPanel::HideFromScript( SCALEFORM_CALLBACK_ARGS_DECL )
{
	UnloadDialog();
}

#endif // INCLUDE_SCALEFORM
