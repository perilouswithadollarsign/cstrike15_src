//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"

#if defined( INCLUDE_SCALEFORM )

#include <time.h>

#include "basepanel.h"
#include "medalstatsdialog_scaleform.h"
#include "../engine/filesystem_engine.h"

#include "vgui/IInput.h"
#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "fmtstr.h"
#include "iachievementmgr.h"
#include "achievementmgr.h"
#include "cs_achievementdefs.h"
#include "achievements_cs.h"
#include "cs_player_rank_mgr.h"
#include "cs_player_rank_shared.h"

#include "gameui_util.h"

#ifdef _X360
#include "xbox/xbox_launch.h"
#endif
#include "keyvalues.h"
#include "engineinterface.h"
#include "modinfo.h"
#include "gameui_interface.h"

#include "tier1/utlbuffer.h"
#include "filesystem.h"

#include "cs_gamestats_shared.h"
#include "cs_client_gamestats.h"

using namespace vgui;

// for SRC
#include <vstdlib/random.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

#define MAX_ACHIEVEMENT_STRLEN	1024
#define MAX_STATS_STRLEN		128

// Maximum number of recent achievements to track
//		-- This number should be the same as the maximum number of slots in the UI panel for recently unlocked
#define MAX_RECENT_ACHIEVEMENT_SLOTS	40

extern CAchievementMgr g_AchievementMgrCS;

#if !defined( _GAMECONSOLE )
void RequestEloBracket( volatile int32 *pOutBracket, int32 game_mode, int32 input_device );
#endif

CCreateMedalStatsDialogScaleform* CCreateMedalStatsDialogScaleform::m_pInstance = NULL;

SFUI_BEGIN_GAME_API_DEF
	SFUI_DECL_METHOD( OnOk ),
	SFUI_DECL_METHOD( UpdateCurrentAchievement ),
	SFUI_DECL_METHOD( GetAchievementStatus ),
	SFUI_DECL_METHOD( GetRecentAchievementCount ),
	SFUI_DECL_METHOD( GetRecentAchievementName ),
	SFUI_DECL_METHOD( GetRankForCurrentCatagory ),
	SFUI_DECL_METHOD( GetAchievedInCategory ),
	SFUI_DECL_METHOD( GetMaxAwardsForCatagory ),
	SFUI_DECL_METHOD( GetMinAwardNeededForRank ),
	SFUI_END_GAME_API_DEF( CCreateMedalStatsDialogScaleform, MedalStatsScreen )
	;


ConVar player_last_medalstats_panel( "player_last_medalstats_panel", "0",  FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS, "Last opened panel in the Medals & Stats screen" );
ConVar player_last_medalstats_category( "player_last_medalstats_category", "0",  FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS, "Last selected category on the Medals panel in the Medals & Stats screen" );
ConVar player_medalstats_recent_range( "player_medalstats_recent_range", "432000",  FCVAR_RELEASE, "Window (in seconds of recent achievements to show" );
ConVar player_medalstats_most_recent_time( "player_medalstats_most_recent_time", "0",  FCVAR_ARCHIVE | FCVAR_ARCHIVE_GAMECONSOLE | FCVAR_SS, "Timestamp of most recently earned achievement displayed on Medals & Stats screen" );

void CCreateMedalStatsDialogScaleform::LoadDialog( eDialogType type )
{
	if ( !m_pInstance )
	{
		m_pInstance = new CCreateMedalStatsDialogScaleform( type );
		SFUI_REQUEST_ELEMENT( SF_FULL_SCREEN_SLOT, g_pScaleformUI, CCreateMedalStatsDialogScaleform, m_pInstance, MedalStatsScreen );
	}
}

void CCreateMedalStatsDialogScaleform::UnloadDialog( void )
{
	if ( m_pInstance )
	{
		m_pInstance->RemoveFlashElement();
	}
}

CCreateMedalStatsDialogScaleform::CCreateMedalStatsDialogScaleform( eDialogType type ) :
	m_MedalNameHandle(NULL),
	m_MedalUnlockHandle(NULL),
	m_MedalDescHandle(NULL),
	m_LastMatchTeamStats(NULL),
	m_LastMatchFaveWeaponName(NULL),
	m_LastMatchFaveWeaponStats(NULL),
	m_LastMatchPerfStats(NULL),
	m_LastMatchMiscStats(NULL),
	m_OverallPlayerName(NULL),
	m_OverallMVPsText(NULL),
	m_OverallPlayerStats(NULL),
	m_OverallFaveWeaponName(NULL),
	m_OverallFaveWeaponStats(NULL),
	m_OverallFaveMapStats(NULL),
	m_type( type ),
	m_mostRecentAchievementTime(0)
{
	ACTIVE_SPLITSCREEN_PLAYER_GUARD( GET_ACTIVE_SPLITSCREEN_SLOT() );
	m_iPlayerSlot = GET_ACTIVE_SPLITSCREEN_SLOT();

#if !defined( _GAMECONSOLE )
	m_nEloBracket = -1;
#if 0 // Disabling elo bracket display in the menu.
	// Right now our elo values arn't meaningful so for 1.20 we're disabling the display.
	// This is done in scaleform, but i'm also disabling it here because we still have this unsolved ecoroutine
	// crash on OSX that will be triggered by the yielding gc message... 
	RequestEloBracket( &m_nEloBracket, ELOGameType::CLASSIC_COMPETITIVE, INPUT_DEVICE_KEYBOARD_MOUSE );
#endif
#endif
}

void CCreateMedalStatsDialogScaleform::OnOk( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iPlayerSlot );
	CCreateMedalStatsDialogScaleform::UnloadDialog();
}

CEG_NOINLINE void CCreateMedalStatsDialogScaleform::UpdateCurrentAchievement( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iPlayerSlot );
	if ( FlashAPIIsValid() )
	{
		const char *achievementName = pui->Params_GetArgAsString( obj, 0 );

		CCSBaseAchievement* pAchievement = NULL;
		
		// if we passed in an empty string, clear all of the achievement fields
		bool bClearFields = ( achievementName[0] == 0 );
		
		if ( !bClearFields )
		{
			pAchievement = dynamic_cast<CCSBaseAchievement*>(g_AchievementMgrCS.GetAchievementByName( achievementName, m_iPlayerSlot ));			
		}
		
		UpdateMedalProgress( pAchievement );

		bool bShouldHide = ( pAchievement && pAchievement->ShouldHideUntilAchieved() && !pAchievement->IsAchieved() );

		CEG_ENCRYPT_FUNCTION( CCreateMedalStatsDialogScaleform_UpdateCurrentAchievement );

		WITH_SLOT_LOCKED
		{
			if ( m_MedalNameHandle )
			{
				if ( !pAchievement )
				{
					// Report an error if the achievement is not found, not properly setup, etc.
					m_pScaleformUI->Value_SetText( m_MedalNameHandle, bClearFields ? "" : "#SFUI_MedalsInfo_Invalid" );
				}
				else if ( bShouldHide )
				{
					m_pScaleformUI->Value_SetText( m_MedalNameHandle, "#SFUI_MedalsInfo_LockedName" );
				}
				else
				{
					const wchar_t * pName = ACHIEVEMENT_LOCALIZED_NAME( pAchievement );
					if ( pName )
					{
						m_pScaleformUI->Value_SetText( m_MedalNameHandle, pName );
					}
					else
					{
						// Missing our localization string!
						m_pScaleformUI->Value_SetText( m_MedalNameHandle, "#SFUI_MedalsInfo_Invalid" );
					}
				}
			}

			if ( m_MedalUnlockHandle )
			{
				if ( !pAchievement )
				{
					// Report an error if the achievement is not found, not properly setup, etc.
					m_pScaleformUI->Value_SetText( m_MedalUnlockHandle, bClearFields ? "" : "#SFUI_MedalsInfo_Invalid" );
				}
				else if ( bShouldHide )
				{
					m_pScaleformUI->Value_SetText( m_MedalUnlockHandle, "" );
				}
				else
				{
					wchar_t achDate[MAX_ACHIEVEMENT_STRLEN];
					V_wcsncpy( achDate, L"", sizeof(achDate) );

					if ( pAchievement->IsAchieved() )
					{
						int year, month, day, hour, minute, second;
						if ( pAchievement->GetAwardTime(year, month, day, hour, minute, second) )
						{
							V_swprintf_safe( achDate, PRI_WS_FOR_WS L" : %4d-%02d-%02d", g_pVGuiLocalize->Find( "#SFUI_MedalsInfo_Unlocked" ), year, month, day );
						}
						else
						{
							// we don't have an unlock time from our info, so just report it is unlocked
							V_wcsncpy( achDate, L"#SFUI_MedalsInfo_Unlocked", sizeof(achDate) );
						}
					}

					m_pScaleformUI->Value_SetText( m_MedalUnlockHandle, achDate );
				}
			}

			if ( m_MedalDescHandle )
			{
				if ( !pAchievement )
				{
					// Report an error if the achievement is not found, not properly setup, etc.
					m_pScaleformUI->Value_SetText( m_MedalDescHandle, bClearFields ? "" : "#SFUI_MedalsInfo_Invalid" );
				}
				else if ( bShouldHide )
				{
					m_pScaleformUI->Value_SetText( m_MedalDescHandle, "#SFUI_MedalsInfo_LockedDesc" );
				}
				else
				{
					const wchar_t * pDesc = ACHIEVEMENT_LOCALIZED_DESC( pAchievement );
					if ( pDesc )
					{
						m_pScaleformUI->Value_SetText( m_MedalDescHandle, pDesc );
					}
					else
					{
						// Missing our localization string!
						m_pScaleformUI->Value_SetText( m_MedalDescHandle, "#SFUI_MedalsInfo_Invalid" );
					}
				}
			}
		}
	}
}

void CCreateMedalStatsDialogScaleform::GetAchievementStatus( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iPlayerSlot );
	const char *achievementName = pui->Params_GetArgAsString( obj, 0 );
	CCSBaseAchievement* pAchievement = dynamic_cast<CCSBaseAchievement*>(g_AchievementMgrCS.GetAchievementByName( achievementName, m_iPlayerSlot ));

	eAchievementStatus Result = eAchievement_Locked;

	if ( pAchievement )
	{
		if ( pAchievement->IsAchieved() )
		{
			// Determine if it's recently achieved, or just unlocked
			if ( pAchievement->GetUnlockTime() > (uint)(player_medalstats_most_recent_time.GetInt()) )
				Result = eAchievement_RecentUnlock;
			else
				Result = eAchievement_Unlocked;
		}
		else if ( pAchievement->ShouldHideUntilAchieved() )
		{
			Result = eAchievement_Secret;
		}
	}

	m_pScaleformUI->Params_SetResult( obj, Result );
}

void CCreateMedalStatsDialogScaleform::FlashLoaded( void )
{
	if ( FlashAPIIsValid() )
	{
		LockInputToSlot( m_iPlayerSlot );
		m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "InitDialogData", NULL, 0 );
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

void CCreateMedalStatsDialogScaleform::FlashReady( void )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iPlayerSlot );
	SFVALUE PanelHandle = m_pScaleformUI->Value_GetMember( m_FlashAPI, "Panel" );

	if ( PanelHandle )
	{
		SFVALUE AnimatedPanelHandle = m_pScaleformUI->Value_GetMember( PanelHandle, "Panel" );

		if ( AnimatedPanelHandle )
		{
			// Medals panel members
			SFVALUE MedalPanel = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "Medals" );

			if ( MedalPanel )
			{
				SFVALUE AnimatedMedalPanel = m_pScaleformUI->Value_GetMember( MedalPanel, "Medals" );

				if ( AnimatedMedalPanel )
				{
					SFVALUE TextPanel = m_pScaleformUI->Value_GetMember( AnimatedMedalPanel, "Text" );

					if ( TextPanel )
					{
						GetTextBoxForElement( m_pScaleformUI, TextPanel, "MedalName", "MedalName", m_MedalNameHandle );
						GetTextBoxForElement( m_pScaleformUI, TextPanel, "MedalUnlockDate", "MedalUnlockDate", m_MedalUnlockHandle );
						GetTextBoxForElement( m_pScaleformUI, TextPanel, "MedalDesc", "MedalDesc", m_MedalDescHandle );
						

						m_pScaleformUI->ReleaseValue( TextPanel );
					}

					m_pScaleformUI->ReleaseValue( AnimatedMedalPanel );
				}

				m_pScaleformUI->ReleaseValue( MedalPanel );
			}


			// Last match panel members
			SFVALUE LastMatchPanel = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "LastMatch" );

			if ( LastMatchPanel )
			{
				SFVALUE AnimatedLastMatchPanel = m_pScaleformUI->Value_GetMember( LastMatchPanel, "LastMatch" );

				if ( AnimatedLastMatchPanel )
				{
					SFVALUE TextPanel = m_pScaleformUI->Value_GetMember( AnimatedLastMatchPanel, "Text" );

					if ( TextPanel )
					{
						GetTextBoxForElement( m_pScaleformUI, TextPanel, "TeamData", "TeamData", m_LastMatchTeamStats );
						GetTextBoxForElement( m_pScaleformUI, TextPanel, "FavoriteWeaponName", "FavoriteWeaponName", m_LastMatchFaveWeaponName );
						GetTextBoxForElement( m_pScaleformUI, TextPanel, "WeaponData", "WeaponData", m_LastMatchFaveWeaponStats );
						GetTextBoxForElement( m_pScaleformUI, TextPanel, "PerformanceData", "PerformanceData", m_LastMatchPerfStats );
						GetTextBoxForElement( m_pScaleformUI, TextPanel, "MiscData", "MiscData", m_LastMatchMiscStats );

						m_pScaleformUI->ReleaseValue( TextPanel );
					}

					m_pScaleformUI->ReleaseValue( AnimatedLastMatchPanel );
				}

				m_pScaleformUI->ReleaseValue( LastMatchPanel );
			}


			// Overall stats panel members
			SFVALUE OverallPanel = m_pScaleformUI->Value_GetMember( AnimatedPanelHandle, "OverAll" );

			if ( OverallPanel )
			{
				SFVALUE AnimatedOverallPanel = m_pScaleformUI->Value_GetMember( OverallPanel, "OverAll" );

				if ( AnimatedOverallPanel )
				{
					SFVALUE TextPanel = m_pScaleformUI->Value_GetMember( AnimatedOverallPanel, "Text" );

					if ( TextPanel )
					{
						GetTextBoxForElement( m_pScaleformUI, TextPanel, "PlayerName", "PlayerName", m_OverallPlayerName );
						GetTextBoxForElement( m_pScaleformUI, TextPanel, "Stars", "Stars", m_OverallMVPsText );
						GetTextBoxForElement( m_pScaleformUI, TextPanel, "PlayerData", "PlayerData", m_OverallPlayerStats );
						GetTextBoxForElement( m_pScaleformUI, TextPanel, "WeaponName", "WeaponName", m_OverallFaveWeaponName );
						GetTextBoxForElement( m_pScaleformUI, TextPanel, "WeaponData", "WeaponData", m_OverallFaveWeaponStats );
						GetTextBoxForElement( m_pScaleformUI, TextPanel, "MapDescription", "MapDescription", m_OverallFaveMapStats );

						m_pScaleformUI->ReleaseValue( TextPanel );
					}

					m_pScaleformUI->ReleaseValue( AnimatedOverallPanel );
				}

				m_pScaleformUI->ReleaseValue( OverallPanel );
			}

			m_pScaleformUI->ReleaseValue( AnimatedPanelHandle );
		}

		m_pScaleformUI->ReleaseValue( PanelHandle );
	}

	PopulateLastMatchStats();

	PopulateOverallStats();

	GenerateRecentAchievements();

	Show();
}

void CCreateMedalStatsDialogScaleform::UpdateMedalProgress( CCSBaseAchievement *pAchievement )
{
	if ( FlashAPIIsValid() )
	{
		bool bShowProgress = ( pAchievement && pAchievement->GetGoal() > 1 );

		// if this achievement gets saved with game and we're not in a level and have not achieved it, then we do not have any state 
		// for this achievement, don't show progress
		if ( bShowProgress && ( pAchievement->GetFlags() & ACH_SAVE_WITH_GAME ) && !GameUI().IsInLevel() && !pAchievement->IsAchieved() )
		{
			bShowProgress = false;
		}

		int iCount = -1; 
		int iGoal = -1;

		if  ( bShowProgress )
		{
			iCount = pAchievement->GetCount();
			iGoal = pAchievement->GetGoal();
			// Once an achievement is achieved, we can't rely on the count to be accurate any longer - force the count to the max
			if ( iCount > iGoal || pAchievement->IsAchieved() )
			{
				iCount = iGoal;
			}
		}
		
		WITH_SLOT_LOCKED
		{
			WITH_SFVALUEARRAY( data, 2 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, iCount );
				m_pScaleformUI->ValueArray_SetElement( data, 1, iGoal);

				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setMedalsProgress", data, 2 );
			}
		}
	}
}

bool CCreateMedalStatsDialogScaleform::PreUnloadFlash( void )
{
	UnlockInput();

	// save off the time of the most recent achievement so we can highlight new ones on a future invoke
	player_medalstats_most_recent_time.SetValue((int)m_mostRecentAchievementTime);

	SafeReleaseSFVALUE( m_MedalNameHandle );
	SafeReleaseSFVALUE( m_MedalUnlockHandle );
	SafeReleaseSFVALUE( m_MedalDescHandle );
	
	SafeReleaseSFVALUE( m_LastMatchTeamStats );
	SafeReleaseSFVALUE( m_LastMatchFaveWeaponName );
	SafeReleaseSFVALUE( m_LastMatchFaveWeaponStats );
	SafeReleaseSFVALUE( m_LastMatchPerfStats );
	SafeReleaseSFVALUE( m_LastMatchMiscStats );

	SafeReleaseSFVALUE( m_OverallPlayerName );
	SafeReleaseSFVALUE( m_OverallMVPsText );
	SafeReleaseSFVALUE( m_OverallPlayerStats );
	SafeReleaseSFVALUE( m_OverallFaveWeaponName );
	SafeReleaseSFVALUE( m_OverallFaveWeaponStats );
	SafeReleaseSFVALUE( m_OverallFaveMapStats );

	return true;
}

void CCreateMedalStatsDialogScaleform::PostUnloadFlash( void )
{
	if ( GameUI().IsInLevel() )
	{
		BasePanel()->RestorePauseMenu();
	}
	else
	{
		BasePanel()->RestoreMainMenuScreen();
	}

	m_recentAchievements.Purge();

	m_pInstance = NULL;
	delete this;
}

void CCreateMedalStatsDialogScaleform::Show( void )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			WITH_SFVALUEARRAY( args, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( args, 0, m_type );
				ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "showPanel", args, 1 );
			}
		}
	}
}

void CCreateMedalStatsDialogScaleform::Hide( void )
{
	if ( FlashAPIIsValid() )
	{
		WITH_SLOT_LOCKED
		{
			ScaleformUI()->Value_InvokeWithoutReturn( m_FlashAPI, "hidePanel", 0, NULL );
		}
	}
}

static int AchievementDateSortPredicate( CCSBaseAchievement* const* pLeft, CCSBaseAchievement* const* pRight )
{
	if (!pLeft || !pRight || !(*pLeft) || ! (*pRight))
	{
		return 0;
	}

	if	((*pLeft)->GetSortKey() < (*pRight)->GetSortKey())
	{
		return	1;
	}
	else if ((*pLeft)->GetSortKey() > (*pRight)->GetSortKey())
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

void CCreateMedalStatsDialogScaleform::GenerateRecentAchievements( void )
{
	// "Recent" achievements are defined as all earned achievments with an "age" less than that value
	// specified by player_medalstats_recent_range convar. 

	int iCount = g_AchievementMgrCS.GetAchievementCount();
	m_recentAchievements.RemoveAll();
	m_recentAchievements.EnsureCapacity(iCount);

	time_t ltime;
	time( &ltime );
	uint now = (uint)ltime;

	for ( int i = 0; i < iCount; ++i )
	{
		CCSBaseAchievement* pAchievement = (CCSBaseAchievement*)g_AchievementMgrCS.GetAchievementByIndex( i, m_iPlayerSlot );
		if ( pAchievement && pAchievement->IsAchieved() )
		{
			uint unlockTime = pAchievement->GetUnlockTime();

			// figure out which was earned most recently for highlighting new ones
			if ( unlockTime > m_mostRecentAchievementTime )
				m_mostRecentAchievementTime = unlockTime;

			int age = now - unlockTime;
			if ( age < player_medalstats_recent_range.GetInt() )
			{
				m_recentAchievements.AddToTail(pAchievement);
			}
		}
	}

	m_recentAchievements.Sort( AchievementDateSortPredicate );
	int unlockCount = m_recentAchievements.Count();

	// Cap our list to the X most recent achievements only (should coincide with the number of slots to show on the screen)
	if ( unlockCount > MAX_RECENT_ACHIEVEMENT_SLOTS )
	{
		m_recentAchievements.RemoveMultipleFromTail( unlockCount - MAX_RECENT_ACHIEVEMENT_SLOTS );
	}
}

void CCreateMedalStatsDialogScaleform::GetRecentAchievementCount( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iPlayerSlot );
	m_pScaleformUI->Params_SetResult( obj, m_recentAchievements.Count() );
}

void CCreateMedalStatsDialogScaleform::GetRecentAchievementName( SCALEFORM_CALLBACK_ARGS_DECL )
{
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iPlayerSlot );
	int achievementIdx = (int) pui->Params_GetArgAsNumber( obj, 0 );

	if ( achievementIdx < m_recentAchievements.Count() )
	{
		CCSBaseAchievement* pAch = m_recentAchievements[ achievementIdx ];

		if ( pAch )
		{
			m_pScaleformUI->Params_SetResult( obj, pAch->GetName() );
			return;
		}
	}

	// Fall through to here, reset the achievement name
	m_pScaleformUI->Params_SetResult( obj, L"" );
}

void CCreateMedalStatsDialogScaleform::GetRankForCurrentCatagory( SCALEFORM_CALLBACK_ARGS_DECL )
{
	// this list is different, unfortunately.  In this list, 0 is "recently earned"
	int nCurCatagory = (int) pui->Params_GetArgAsNumber( obj, 0 );
	// catagory 5 is the recently unlocked catagory, hide the progress
	if ( nCurCatagory >= MEDAL_CATEGORY_COUNT || nCurCatagory < MEDAL_CATEGORY_START )
		return;

	int nCurRank = g_PlayerRankManager.CalculateRankForCategory( (MedalCategory_t)nCurCatagory );

	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iPlayerSlot );
	m_pScaleformUI->Params_SetResult( obj, nCurRank );
}

void CCreateMedalStatsDialogScaleform::GetMaxAwardsForCatagory( SCALEFORM_CALLBACK_ARGS_DECL )
{
	// this list is different, unfortunately.  In this list, 0 is "recently earned"
	int nCurCatagory = (int) pui->Params_GetArgAsNumber( obj, 0 );
	// catagory 5 is the recently unlocked catagory, hide the progress
	if ( nCurCatagory >= MEDAL_CATEGORY_COUNT || nCurCatagory < MEDAL_CATEGORY_START )
		return;

	int nAwards = g_PlayerRankManager.GetTotalMedalsInCategory( (MedalCategory_t)nCurCatagory );

	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iPlayerSlot );
	m_pScaleformUI->Params_SetResult( obj, nAwards );
}

void CCreateMedalStatsDialogScaleform::GetAchievedInCategory( SCALEFORM_CALLBACK_ARGS_DECL )
{
	// this list is different, unfortunately.  In this list, 0 is "recently earned"
	int nCurCatagory = (int) pui->Params_GetArgAsNumber( obj, 0 );
	// catagory 5 is the recently unlocked catagory, hide the progress
	if ( nCurCatagory >= MEDAL_CATEGORY_COUNT || nCurCatagory < MEDAL_CATEGORY_START )
		return;

	int nCurAwards = g_PlayerRankManager.CountAchievedInCategory( (MedalCategory_t)nCurCatagory );

	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iPlayerSlot );
	m_pScaleformUI->Params_SetResult( obj, nCurAwards );
}

void CCreateMedalStatsDialogScaleform::GetMinAwardNeededForRank( SCALEFORM_CALLBACK_ARGS_DECL )
{
	// this list is different, unfortunately.  In this list, 0 is "recently earned"
	int nCurCatagory = (int) pui->Params_GetArgAsNumber( obj, 0 );
	// catagory 5 is the recently unlocked catagory, hide the progress
	if ( nCurCatagory >= MEDAL_CATEGORY_COUNT || nCurCatagory < MEDAL_CATEGORY_START )
		return;

	int nRank = (int) pui->Params_GetArgAsNumber( obj, 1 );
	if ( nRank >= MEDAL_RANK_COUNT || nRank < MEDAL_RANK_NONE )
		return;

	int nCurAwards = g_PlayerRankManager.GetMinMedalsForRank( (MedalCategory_t)nCurCatagory, (MedalRank_t)nRank );

	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iPlayerSlot );
	m_pScaleformUI->Params_SetResult( obj, nCurAwards );
}

static int GetStatValue( int desiredStat, const StatsCollection_t& statsCollection )
{

	Assert(desiredStat >= 0 && desiredStat < CSSTAT_MAX);
	if ( desiredStat >= 0 && desiredStat < CSSTAT_MAX)
	{
		return statsCollection[desiredStat];
	}	
	else
	{
		return CSSTAT_UNDEFINED;
	}
}

void CCreateMedalStatsDialogScaleform::PopulateLastMatchStats()
{
	int userSlot = STEAM_PLAYER_SLOT;

#if defined ( _X360 )
	SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iPlayerSlot );
	userSlot = m_iPlayerSlot;
#endif

	// We actually want to query the LASTMATCH stats from the lifetime collection, where they're written at the end of each match
	const StatsCollection_t& personalMatchStats = g_CSClientGameStats.GetLifetimeStats( userSlot );

	wchar_t		teamStats[MAX_STATS_STRLEN];
	{
		int roundWonTValue = GetStatValue( CSSTAT_LASTMATCH_T_ROUNDS_WON, personalMatchStats );
		int roundWonCTValue = GetStatValue( CSSTAT_LASTMATCH_CT_ROUNDS_WON, personalMatchStats );
		int roundWonValue = GetStatValue( CSSTAT_LASTMATCH_ROUNDS_WON, personalMatchStats );
		int maxPlayersValue = GetStatValue( CSSTAT_LASTMATCH_MAX_PLAYERS, personalMatchStats );
		
		V_snwprintf( teamStats, ARRAYSIZE(teamStats), L"%d\n%d\n%d\n%d", roundWonTValue, roundWonCTValue, roundWonValue, maxPlayersValue );
	}

	const char *weaponName = "#SFUI_LastMatch_NoFaveWeapon";
	const char *shortWeaponName = "weapon_none";

	wchar_t		weaponStats[MAX_STATS_STRLEN];
	{	
		CSWeaponID faveWeaponID = static_cast<CSWeaponID>( GetStatValue( CSSTAT_LASTMATCH_FAVWEAPON_ID, personalMatchStats ) );
		
		shortWeaponName = WeaponIdAsString( faveWeaponID );

		const CCSWeaponInfo *pInfo = GetWeaponInfo( faveWeaponID );
		if ( pInfo )
		{
			weaponName = pInfo->szPrintName;
		}

		int faveWeaponHits = GetStatValue( CSSTAT_LASTMATCH_FAVWEAPON_HITS, personalMatchStats );
		int faveWeaponKills = GetStatValue( CSSTAT_LASTMATCH_FAVWEAPON_KILLS, personalMatchStats );

		float accuracy = 0.0f;
		int faveWeaponShots = GetStatValue( CSSTAT_LASTMATCH_FAVWEAPON_SHOTS, personalMatchStats );

		if (faveWeaponShots != 0)
		{
			accuracy = ((float)faveWeaponHits / (float)faveWeaponShots) * 100.0f;
		}

		V_snwprintf( weaponStats, ARRAYSIZE( weaponStats ), L"%d\n%d\n%3.2f", faveWeaponHits, faveWeaponKills, accuracy );
	}

	wchar_t		perfStats[MAX_STATS_STRLEN];
	wchar_t		miscStats[MAX_STATS_STRLEN];
	{
		int stars = GetStatValue( CSSTAT_LASTMATCH_MVPS, personalMatchStats );
		int kills = GetStatValue( CSSTAT_LASTMATCH_KILLS, personalMatchStats );
		int deaths = GetStatValue( CSSTAT_LASTMATCH_DEATHS, personalMatchStats );
		int roundsPlayed = GetStatValue( CSTAT_LASTMATCH_ROUNDS_PLAYED, personalMatchStats );
		int contribScore = GetStatValue( CSSTAT_LASTMATCH_CONTRIBUTION_SCORE, personalMatchStats );

		float killDeathRatio = 0.f;
		if (deaths > 0)
		{
			killDeathRatio = (float)kills / (float)deaths;
		}

		float contribPerRound = contribScore;
		if ( roundsPlayed > 0 )
		{
			contribPerRound = (float)contribScore / (float)roundsPlayed;
		}

		V_snwprintf( perfStats, ARRAYSIZE( perfStats ), L"%d\n%d\n%d\n%.3f\n%3.1f", stars, kills, deaths, killDeathRatio, contribPerRound );

		int damage = GetStatValue( CSSTAT_LASTMATCH_DAMAGE, personalMatchStats );
		int dominations = GetStatValue( CSSTAT_LASTMATCH_DOMINATIONS, personalMatchStats );
		int revenges = GetStatValue( CSSTAT_LASTMATCH_REVENGES, personalMatchStats );
		int moneySpent = GetStatValue( CSSTAT_LASTMATCH_MONEYSPENT, personalMatchStats );

		float costPerKill = 0.f;
		if (kills > 0)
		{
			costPerKill = (float)moneySpent / (float)kills;
		}

		V_snwprintf( miscStats, ARRAYSIZE( miscStats ), L"%d\n%.3f\n%d\n%d", damage, costPerKill, dominations, revenges );
	}

	WITH_SLOT_LOCKED
	{
		// Update the favorite weapon icon
		{
			WITH_SFVALUEARRAY( data, 1 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, shortWeaponName );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setLastMatchFavoriteWeapon", data, 1 );
			}
		}

		if ( m_LastMatchTeamStats )
		{
			m_pScaleformUI->Value_SetText( m_LastMatchTeamStats, teamStats );
		}

		if ( m_LastMatchFaveWeaponName )
		{
			m_pScaleformUI->Value_SetText( m_LastMatchFaveWeaponName, weaponName );
		}

		if ( m_LastMatchFaveWeaponStats )
		{
			m_pScaleformUI->Value_SetText( m_LastMatchFaveWeaponStats, weaponStats );
		}

		if ( m_LastMatchPerfStats )
		{
			m_pScaleformUI->Value_SetText( m_LastMatchPerfStats, perfStats );
		}

		if ( m_LastMatchMiscStats )
		{
			m_pScaleformUI->Value_SetText( m_LastMatchMiscStats, miscStats );
		}	
	}
}

void CCreateMedalStatsDialogScaleform::PopulateOverallStats()
{
	int userSlot = STEAM_PLAYER_SLOT;

	const char	*playerName = "Player";
	{
		SF_FORCE_SPLITSCREEN_PLAYER_GUARD( m_iPlayerSlot );
		IPlayerLocal *pProfile = g_pMatchFramework->GetMatchSystem()->GetPlayerManager()->GetLocalPlayer( XBX_GetUserId( m_iPlayerSlot ) );
		if ( pProfile )
		{
			playerName = pProfile->GetName();
		}
		userSlot = m_iPlayerSlot;
	}

	const StatsCollection_t& lifetimeStats = g_CSClientGameStats.GetLifetimeStats( userSlot );

	wchar_t	starsText[MAX_STATS_STRLEN];
	{
		int numMVPs = GetStatValue( CSSTAT_MVPS, lifetimeStats );
		V_snwprintf( starsText, ARRAYSIZE( starsText ), g_pVGuiLocalize->Find("#SFUI_Overall_Stars"), numMVPs );
	}

	wchar_t	playerStats[MAX_STATS_STRLEN];
	{
		int roundsPlayed = GetStatValue( CSSTAT_ROUNDS_PLAYED, lifetimeStats );
		int roundsWon = GetStatValue( CSSTAT_ROUNDS_WON, lifetimeStats );
		int shotsFired = GetStatValue( CSSTAT_SHOTS_FIRED, lifetimeStats );
		int shotsHit = GetStatValue( CSSTAT_SHOTS_HIT, lifetimeStats );
		int numKills = GetStatValue( CSSTAT_KILLS, lifetimeStats );
		int numDeaths = GetStatValue( CSSTAT_DEATHS, lifetimeStats );

		float winPct = 0.f;
		if ( roundsPlayed > 0 )
		{
			winPct = (float)roundsWon / (float)roundsPlayed * 100.0f;
		}

		float hitRatio = 0.f;
		if ( shotsFired > 0 )
		{
			hitRatio = (float)shotsHit / (float)shotsFired;
		}

		float killDeathRatio = 0.f;
		if ( numDeaths > 0 )
		{
			killDeathRatio = (float)numKills / (float)numDeaths;
		}

		V_snwprintf( playerStats, ARRAYSIZE( playerStats ), L"%d\n%d\n%3.2f\n%d\n%d\n%3.2f\n\n%d\n%d\n%.3f", roundsPlayed, roundsWon, winPct,
				shotsFired, shotsHit, hitRatio, numKills, numDeaths, killDeathRatio );
	}

	const char *shortMapName = "map_none";
	const wchar_t* wszFaveMapName;
	wchar_t	mapStats[MAX_STATS_STRLEN];
	
	{
		wszFaveMapName = g_pVGuiLocalize->Find("#SFUI_Overall_NoFavorite");

		// determine which map we've played the most
		const MapName_MapStatId* pFavoriteMapStat = NULL;
		int numRounds = 0;
		for (int i = 0; MapName_StatId_Table[i].statWinsId != CSSTAT_UNDEFINED; ++i)
		{
			const MapName_MapStatId& mapStatId = MapName_StatId_Table[i];
			PlayerStatData_t stat = g_CSClientGameStats.GetStatById( mapStatId.statRoundsId, userSlot );
			if (stat.iStatValue > numRounds)
			{
				pFavoriteMapStat = &mapStatId;
				numRounds = stat.iStatValue;
			}
		}

		numRounds = 0;
		int numWins = 0;
		float winPct = 0.f;

		// use the values from our favorite map
		if ( pFavoriteMapStat )
		{
			shortMapName = pFavoriteMapStat->szMapName;
			wszFaveMapName = CSGameRules()->GetFriendlyMapName(shortMapName);

			numWins = g_CSClientGameStats.GetStatById( pFavoriteMapStat->statWinsId, userSlot ).iStatValue;
			numRounds  = g_CSClientGameStats.GetStatById( pFavoriteMapStat->statRoundsId, userSlot ).iStatValue;
			
			if (numRounds > 0)
			{
				winPct = ((float)numWins / (float)numRounds) * 100.0f;
			}
		}

		V_snwprintf( mapStats, ARRAYSIZE( mapStats ), g_pVGuiLocalize->Find("#SFUI_Overall_MapStats"), wszFaveMapName, numRounds, numWins, winPct );
	}

	const char *shortWeaponName = "weapon_none";
	const char *weaponName = "#SFUI_Overall_NoFavorite";

	wchar_t faveWeaponStats[MAX_STATS_STRLEN];
	{
		const WeaponName_StatId* pFavoriteWeaponStatEntry = NULL;

		// Find the weapon stat with the most kills
		int numKills = 0;
		for (int i = 0; WeaponName_StatId_Table[i].killStatId != CSSTAT_UNDEFINED; ++i)
		{
			// ignore weapons with no hit counts (knife and grenade)
			if (WeaponName_StatId_Table[i].hitStatId == CSSTAT_UNDEFINED )
				continue;

			const WeaponName_StatId& weaponStatEntry = WeaponName_StatId_Table[i];
			PlayerStatData_t stat = g_CSClientGameStats.GetStatById( weaponStatEntry.killStatId, userSlot );
			if (stat.iStatValue > numKills)
			{
				pFavoriteWeaponStatEntry = &weaponStatEntry;
				numKills = stat.iStatValue;
			}
		}

		int hits = 0;
		int shots = 0;
		int kills = 0;
		float killsPerShot = 0.f;	

		if (pFavoriteWeaponStatEntry)
		{
			CSWeaponID faveWeaponID = static_cast<CSWeaponID>( pFavoriteWeaponStatEntry->weaponId );

			shortWeaponName = WeaponIdAsString( faveWeaponID );
			const CCSWeaponInfo *pInfo = GetWeaponInfo( faveWeaponID );
			if ( pInfo )
			{
				weaponName = pInfo->szPrintName;
			}
		
			hits = g_CSClientGameStats.GetStatById( pFavoriteWeaponStatEntry->hitStatId, userSlot ).iStatValue;
			shots = g_CSClientGameStats.GetStatById( pFavoriteWeaponStatEntry->shotStatId, userSlot ).iStatValue;
			kills = g_CSClientGameStats.GetStatById( pFavoriteWeaponStatEntry->killStatId, userSlot ).iStatValue;

			// Do we want accuracy, or kills/shot?
			if (shots > 0)
			{
				killsPerShot = (float)kills / (float)shots;
			}
		}

		V_snwprintf( faveWeaponStats, ARRAYSIZE( faveWeaponStats ), L"%d\n%d\n%d\n%.3f", shots, hits, kills, killsPerShot );
	}

	WITH_SLOT_LOCKED
	{
		// Update the favorite weapon & map icon
		{
			WITH_SFVALUEARRAY( data, 2 )
			{
				m_pScaleformUI->ValueArray_SetElement( data, 0, shortWeaponName );
				m_pScaleformUI->ValueArray_SetElement( data, 1, shortMapName );
				m_pScaleformUI->Value_InvokeWithoutReturn( m_FlashAPI, "setOverallFavoriteWeaponAndMap", data, 2 );
			}
		}

		if ( m_OverallPlayerName )
		{
			m_pScaleformUI->Value_SetText( m_OverallPlayerName, playerName );
		}

		if ( m_OverallMVPsText )
		{
			m_pScaleformUI->Value_SetText( m_OverallMVPsText, starsText );
		}

		if ( m_OverallPlayerStats )
		{
			m_pScaleformUI->Value_SetText( m_OverallPlayerStats, playerStats );
		}

		if ( m_OverallFaveWeaponName )
		{
			m_pScaleformUI->Value_SetText( m_OverallFaveWeaponName, weaponName );
		}

		if ( m_OverallFaveWeaponStats )
		{
			m_pScaleformUI->Value_SetText( m_OverallFaveWeaponStats, faveWeaponStats );
		}	

		if ( m_OverallFaveMapStats )
		{
			m_pScaleformUI->Value_SetText( m_OverallFaveMapStats, mapStats );
		}	
		
	}
}


#endif
