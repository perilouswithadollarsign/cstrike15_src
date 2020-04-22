//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#if defined( INCLUDE_SCALEFORM )
#include "basepanel.h"
#include "upsell_scaleform.h"
#include "IGameUIFuncs.h"
#include "vstdlib/vstrtools.h"
#include "../gameui/cstrike15/cstrike15basepanel.h"
#include "iachievementmgr.h"
#include "achievementmgr.h"
#include "cs_achievementdefs.h"
#include "achievements_cs.h"
#include "gameui_interface.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define UPSELL_ACHIEVEMENT_SLOTS 10

CUpsellScaleform* CUpsellScaleform::m_pInstance = NULL;

SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD_AS( OnQuitPressed, "QuitPressed" ),
	SFUI_DECL_METHOD_AS( OnBackPressed, "BackPressed" ),
	SFUI_DECL_METHOD_AS( OnUnlockPressed, "UnlockPressed" ),
	SFUI_DECL_METHOD_AS( OnBasePanelRunCommand, "BasePanelRunCommand" ),
SFUI_END_GAME_API_DEF( CUpsellScaleform, UpsellMenu );


CUpsellScaleform::CUpsellScaleform() :
	m_bVisible ( false ),
	m_bLoading ( false ),
	m_pTextMedalsCount( NULL )
{
}


CUpsellScaleform::~CUpsellScaleform()
{
}


void CUpsellScaleform::LoadDialog( void )
{
	if ( !m_pInstance )
	{
		m_pInstance = new CUpsellScaleform( );
		SFUI_REQUEST_ELEMENT( SF_FULL_SCREEN_SLOT, g_pScaleformUI, CUpsellScaleform, m_pInstance, UpsellMenu );
	}
}


void CUpsellScaleform::UnloadDialog( void )
{
	// m_pInstance is deleted in PostUnloadFlash. RemoveFlashElement is called at the end of the hide animation.
	if ( m_pInstance )
	{
		// Flash elements are removed after hide animation completes
		m_pInstance->Hide();
	}
}


void CUpsellScaleform::ShowMenu( bool bShow )
{
	if ( bShow && !m_pInstance)
	{
		LoadDialog();
	}
	else
	{
		if ( bShow != m_pInstance->m_bVisible )
		{
			if ( bShow )
			{
				m_pInstance->Show();
			}
			else
			{
				m_pInstance->Hide();
			}
		}
	}
}


void CUpsellScaleform::FlashLoaded( void )
{
}


void CUpsellScaleform::FlashReady( void )
{
	//m_pCTCount = m_pScaleformUI->TextObject_MakeTextObjectFromMember( navPanelValue, "CT_Count" );
	if ( m_FlashAPI && m_pScaleformUI )
	{	
		m_bLoading = false;

		SFVALUE topPanel = m_pScaleformUI->Value_GetMember( m_FlashAPI, "TopPanel" );

		if ( topPanel )
		{
			SFVALUE panel = m_pScaleformUI->Value_GetMember( topPanel, "Panel" );

			if ( panel )
			{

				SFVALUE textPanel = m_pScaleformUI->Value_GetMember( panel, "TextPanel" );

				if ( textPanel )
				{
					SFVALUE totalPanel = m_pScaleformUI->Value_GetMember( textPanel, "TotalCountPanel" );

					if ( totalPanel )
					{
						m_pTextMedalsCount = m_pScaleformUI->TextObject_MakeTextObjectFromMember( totalPanel, "TotalCount" );
						m_pScaleformUI->ReleaseValue( totalPanel );
					}
					
					m_pScaleformUI->ReleaseValue( textPanel );
				}

				m_pScaleformUI->ReleaseValue( panel );
			}

			m_pScaleformUI->ReleaseValue( topPanel );
		}

		PopulateAchievements();
		Show();
	}
}


void CUpsellScaleform::Show( void )
{

	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "ShowPanel", 0, NULL );
		}

		m_bVisible = true;
	}
}


void CUpsellScaleform::Hide( const char * szPostHideCommand )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			WITH_SFVALUEARRAY( data, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, szPostHideCommand );
				g_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "HidePanel", data, 1 );
			}
		}

		m_bVisible = false;
	}
}


bool CUpsellScaleform::PreUnloadFlash( void )
{
	SafeReleaseSFTextObject( m_pTextMedalsCount );

	return ScaleformFlashInterface::PreUnloadFlash();
}


void CUpsellScaleform::PostUnloadFlash( void )
{
	if ( m_pInstance )
	{
		delete this;
		m_pInstance = NULL;
	}
	else
	{
		Assert( false );
	}
}


void CUpsellScaleform::OnQuitPressed( SCALEFORM_CALLBACK_ARGS_DECL )
{
	Hide( "QuitNoConfirm" );
}


void CUpsellScaleform::OnBackPressed( SCALEFORM_CALLBACK_ARGS_DECL )
{
	Hide( "RestoreMainMenu" );
}

void CUpsellScaleform::OnUnlockPressed( SCALEFORM_CALLBACK_ARGS_DECL )
{
#if defined ( _X360 )
	xboxsystem->ShowUnlockFullGameUI();
#elif defined( _PS3 )
	//$TODO: Implement PS3 version of xboxsystem->ShowUnlockFullGameUI()
#endif // _X360
}

void CUpsellScaleform::OnBasePanelRunCommand( SCALEFORM_CALLBACK_ARGS_DECL )
{
	char RunCommandStr[1024];
	V_strncpy( RunCommandStr, pui->Params_GetArgAsString( obj, 0 ), sizeof( RunCommandStr ) );

	BasePanel()->PostMessage( BasePanel(), new KeyValues( "RunMenuCommand", "command", RunCommandStr ) );
}

// Not thread safe. Do not invoke outside of flash callbacks
void CUpsellScaleform::PopulateAchievements( void )
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
	int nID = XBX_GetActiveUserId();

	IAchievementMgr * pAchievementMgr = engine->GetAchievementMgr();
	
	if ( !pAchievementMgr )
	{
		return;
	}

	int nAchievementMax = pAchievementMgr->GetAchievementCount();
	int nAchievmentCount = 0;
	int nSlot = 0;

	for ( int i = 0; i < nAchievementMax; i++ )
	{
		CCSBaseAchievement* pAchievement = static_cast<CCSBaseAchievement *>( pAchievementMgr->GetAchievementByIndex( i, nID ) );

		if ( pAchievement && pAchievement->IsAchieved() )
		{
			++nAchievmentCount;

			if ( nSlot < UPSELL_ACHIEVEMENT_SLOTS )
			{
				// populate icon slots
				WITH_SFVALUEARRAY( data, 2 )
				{
					m_pScaleformUI->ValueArray_SetElement( data, 0, nSlot );
					m_pScaleformUI->ValueArray_SetElement( data, 1, pAchievement->GetName() );

					m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetMedal", data, 2 );
				}

				nSlot++;
			}
		}

	}

	// Set progress bar and numerical count
	int nPercent = 0;

	if ( nAchievmentCount > 0 )
	{
		if ( nAchievementMax != 0)
		{
			nPercent = static_cast<int>( ( ( static_cast<float>( nAchievmentCount ) / static_cast<float>( nAchievementMax ) ) * 100.0f ) );
		}
		else
		{
			Assert( false );
		}
	}

	WITH_SFVALUEARRAY( data, 1 )
	{
		m_pScaleformUI->ValueArray_SetElement( data, 0, nPercent );
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "SetProgressBar", data, 1 );
	}

	wchar_t wcCount[32];
	V_snwprintf( wcCount, ARRAYSIZE( wcCount ), L"%d / %d", nAchievmentCount, nAchievementMax );

	if ( m_pTextMedalsCount )
	{
		m_pTextMedalsCount->SetText( wcCount );
	}
}




#endif // INCLUDE_SCALEFORM
