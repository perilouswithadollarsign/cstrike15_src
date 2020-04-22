//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//
#include "cbase.h"
#include "hudelement.h"
#include "sfhudmoney.h"
#include "hud_macros.h"
#include "cs_gamerules.h"
#include "sfhudfreezepanel.h"
#include "c_plantedc4.h"
#include "sfhudradar.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


DECLARE_HUDELEMENT( SFHudMoney);
DECLARE_HUD_MESSAGE( SFHudMoney, AdjustMoney );


SFUI_BEGIN_GAME_API_DEF
SFUI_DECL_METHOD( DoneAnimatingAdd ),
SFUI_DECL_METHOD( DoneAnimatingSub ),
SFUI_END_GAME_API_DEF( SFHudMoney, Money );

extern ConVar cl_draw_only_deathnotices;

SFHudMoney::SFHudMoney( const char *value ) : SFHudFlashInterface( value )
{
	m_bAnimatingAdd = false;
	m_bAnimatingSub = false;	
	m_nLastMoney = 0;
	m_lastEntityIndex = 0;

	m_hCash = NULL;
	m_hAddCash = NULL;
	m_hRemoveCash = NULL;
	m_hBuyZoneIcon = NULL;

	m_bShowBuyZoneIcon = false;
	m_nShiftState = -1;

	SetIgnoreGlobalHudDisable( true );
}


SFHudMoney::~SFHudMoney()
{
}

void SFHudMoney::Init( void )
{
	HOOK_HUD_MESSAGE( SFHudMoney, AdjustMoney );
}

void SFHudMoney::LevelInit( void )
{
	if ( !FlashAPIIsValid() )
	{
		SFUI_REQUEST_ELEMENT( SF_SS_SLOT( GET_ACTIVE_SPLITSCREEN_SLOT() ), g_pScaleformUI, SFHudMoney, this, Money );
	}
}

void SFHudMoney::LevelShutdown( void )
{
	if ( FlashAPIIsValid() )
	{
		RemoveFlashElement();
	}
}

bool SFHudMoney::ShouldDraw( void )
{
	if ( IsTakingAFreezecamScreenshot() )
		return false;

	if ( !CSGameRules() )
		return false;

	if ( CSGameRules()->IsPlayingTraining() || !CSGameRules()->CanSpendMoneyInMap() )
		return false;
	
	IViewPortPanel* buyPanel = NULL;
	IViewPortPanel *scoreboard = NULL;
	if ( GetViewPortInterface() )
	{
		buyPanel = GetViewPortInterface()->FindPanelByName( PANEL_BUY );
		scoreboard = GetViewPortInterface()->FindPanelByName( PANEL_SCOREBOARD );
	}

	if ( CSGameRules()->GetGamePhase() == GAMEPHASE_MATCH_ENDED && scoreboard && scoreboard->IsVisible() )
		return false;
	
	bool bGloballyHidden = GetHud().HudDisabled() && ( !buyPanel || !buyPanel->IsVisible() );

	return cl_drawhud.GetBool() && !bGloballyHidden && cl_draw_only_deathnotices.GetBool() == false && CHudElement::ShouldDraw();	
}


void SFHudMoney::SetActive( bool bActive )
{
	Show( bActive );
	CHudElement::SetActive( bActive );
}

void SFHudMoney::Show( bool show )
{
	if ( m_FlashAPI && show != m_bActive )
	{
		WITH_SLOT_LOCKED
		{
			if ( show )
			{
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", NULL, 0 );
			}
			else
			{
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", NULL, 0 );
			}
		}

		UpdateCurrentMoneyText();
	}
}

void SFHudMoney::FlashReady( void )
{
	m_bAnimatingAdd = false;
	m_bAnimatingSub = false;

	m_cashAdjustmentQueue.SetCount( 0 );

	SFVALUE root = m_pScaleformUI->Value_GetMember( m_FlashAPI, "MoneyPanel" );
	if ( root )
	{
		SFVALUE innerPanel = m_pScaleformUI->Value_GetMember( root, "InnerMoneyPanel" );
		if ( innerPanel )
		{
			m_hBuyZoneIcon = m_pScaleformUI->TextObject_MakeTextObjectFromMember( innerPanel, "BuyZoneIcon" );	
			SFVALUE container = m_pScaleformUI->Value_GetMember( innerPanel, "CashContainer" );
			if ( container )
			{
				m_hCash = m_pScaleformUI->TextObject_MakeTextObjectFromMember( container, "Cash" );	
				SFVALUE AddCash = m_pScaleformUI->Value_GetMember( container, "AddCash" );
				if ( AddCash )
				{
					m_hAddCash = m_pScaleformUI->TextObject_MakeTextObjectFromMember( AddCash, "AddText" );
					g_pScaleformUI->ReleaseValue( AddCash );
				}
				SFVALUE RemoveCash = m_pScaleformUI->Value_GetMember( container, "RemoveCash" );
				if ( RemoveCash )
				{
					m_hRemoveCash = m_pScaleformUI->TextObject_MakeTextObjectFromMember( RemoveCash, "RemoveText" );
					g_pScaleformUI->ReleaseValue( RemoveCash );
				}
				g_pScaleformUI->ReleaseValue( container );
			}
			g_pScaleformUI->ReleaseValue( innerPanel );
		}
		g_pScaleformUI->ReleaseValue( root );
	}

	if ( m_bActive )
	{
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", NULL, 0 );
	}
	else
	{
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", NULL, 0 );
	}
	
	if ( m_hBuyZoneIcon )
	{
		m_hBuyZoneIcon->SetVisible( false );
		m_bShowBuyZoneIcon = false;
	}
	m_nShiftState = -1;

	UpdateCurrentMoneyText();
}


bool SFHudMoney::PreUnloadFlash( void )
{
	SafeReleaseSFTextObject( m_hCash );
	SafeReleaseSFTextObject( m_hAddCash );
	SafeReleaseSFTextObject( m_hRemoveCash );
	SafeReleaseSFTextObject( m_hBuyZoneIcon );
	return true;
}

void SFHudMoney::ProcessInput( void )
{
	C_CSPlayer* pPlayer = pPlayer = GetHudPlayer();
	CCSGameRules* pGameRules = CSGameRules();
	if ( pGameRules && pPlayer )
	{
		int entityIndex = pPlayer->entindex();
		if ( pPlayer->IsControllingBot() )
			entityIndex = pPlayer->GetControlledBotIndex();

		// let's always draw attention to when the player's money has changed
		if ( entityIndex == m_lastEntityIndex )
		{
			if ( m_nLastMoney != pPlayer->GetAccount() )
			{
				// if this is the start of the very first round, don't show the change that can happen from the warmup round to the start round
				if ( pGameRules->GetTotalRoundsPlayed() == 0 && pGameRules->GetRoundElapsedTime() < 1 )
				{
					UpdateCurrentMoneyText();
				}
				else
				{
					UpdateMoneyChange( pPlayer->GetAccount() - m_nLastMoney );
				}
			}
		}
		else
		{
			// we changed who we're observing, so just update it directly
			m_lastEntityIndex = entityIndex;
			UpdateCurrentMoneyText();
		}

		m_nLastMoney = pPlayer->GetAccount();

		bool bShowBuyZoneIcon =	CSGameRules()->CanSpendMoneyInMap() &&
			!pGameRules->IsBuyTimeElapsed() &&
			pPlayer->IsInBuyZone();

		if ( m_hBuyZoneIcon && ( m_bShowBuyZoneIcon != bShowBuyZoneIcon ) )
		{
			m_bShowBuyZoneIcon = bShowBuyZoneIcon;

			WITH_SLOT_LOCKED
			{
				m_hBuyZoneIcon->SetVisible( m_bShowBuyZoneIcon );
			}
		}

		int nShiftState = 0;

		bool bRoundRadar = ( GET_HUDELEMENT( SFHudRadar ) )->m_bRound;

		nShiftState =	(	pGameRules->IsHostageRescueMap() ||	!bRoundRadar ) ? 1: nShiftState;

		nShiftState =	pPlayer->IsBuyMenuOpen() ? 2: nShiftState;

		if ( FlashAPIIsValid() && ( m_nShiftState != nShiftState ) )
		{
			m_nShiftState = nShiftState;

			WITH_SFVALUEARRAY_SLOT_LOCKED( data, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, nShiftState );
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetShift", data, 1 );
			}
		}
	}					
}

bool SFHudMoney::MsgFunc_AdjustMoney( const CCSUsrMsg_AdjustMoney &msg )
{		
	return true;
}

void SFHudMoney::UpdateMoneyChange( int nDelta )
{		
	if ( FlashAPIIsValid() )
	{
		WITH_SFVALUEARRAY_SLOT_LOCKED( data, 1 )
		{
			m_pScaleformUI->ValueArray_SetElement( data, 0, nDelta );
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "DisplayMoneyAdjustment", data, 1 );
		}
	}

	if ( nDelta < 0 )
	{
		UpdateCurrentMoneyText();
		m_bAnimatingSub = true;
	}
	else
	{
		m_bAnimatingAdd = true;
	}
}

void SFHudMoney::DoneAnimatingAdd( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_bAnimatingAdd = false;
	UpdateCurrentMoneyText();
}

void SFHudMoney::DoneAnimatingSub( SCALEFORM_CALLBACK_ARGS_DECL )
{
	m_bAnimatingSub = false;
}

void SFHudMoney::UpdateCurrentMoneyText( void )
{	
	C_CSPlayer *pPlayer = GetHudPlayer();
	if ( pPlayer && m_hCash )
	{
		WITH_SLOT_LOCKED
		{
			m_hCash->SetText( CFmtStr( "$%d", pPlayer->GetAccount() ) );
		}
	}
}

