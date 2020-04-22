#if defined( INCLUDE_SCALEFORM )
//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CREATEMEDALSTATSDIALOG_H
#define CREATEMEDALSTATSDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "scaleformui/scaleformui.h"
#include "utlvector.h"

class CCSBaseAchievement;

enum eAchievementStatus
{
	eAchievement_Locked,
	eAchievement_Unlocked,
	eAchievement_Secret,
	eAchievement_RecentUnlock,
};

class CCreateMedalStatsDialogScaleform: public ScaleformFlashInterface
{
public:
	enum eDialogType
	{
		eDialogType_Stats_Overall = 0,
		eDialogType_Stats_Last_Match = 1,
		eDialogType_Medals = 2,
		
	};

protected:
	static CCreateMedalStatsDialogScaleform* m_pInstance;

	CCreateMedalStatsDialogScaleform( eDialogType type );

public:
	static void LoadDialog( eDialogType type );
	static void UnloadDialog( void );

	void OnOk( SCALEFORM_CALLBACK_ARGS_DECL );
	void UpdateCurrentAchievement( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetAchievementStatus( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetRecentAchievementCount( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetRecentAchievementName( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetRankForCurrentCatagory( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetMaxAwardsForCatagory( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetAchievedInCategory( SCALEFORM_CALLBACK_ARGS_DECL );
	void GetMinAwardNeededForRank( SCALEFORM_CALLBACK_ARGS_DECL );


protected:
	virtual void FlashReady( void );
	virtual bool PreUnloadFlash( void );
	virtual void PostUnloadFlash( void );
	virtual void FlashLoaded( void );

	void Show( void );
	void Hide( void );

	void UpdateMedalProgress( CCSBaseAchievement* pAchievement );

	void PopulateLastMatchStats();
	void PopulateOverallStats();

	// Fills out our array of the N most recent achievements
	void GenerateRecentAchievements();
private:
	SFVALUE	m_MedalNameHandle;
	SFVALUE	m_MedalUnlockHandle;
	SFVALUE	m_MedalDescHandle;

	CUtlVector<CCSBaseAchievement*>	m_recentAchievements;

	SFVALUE	m_LastMatchTeamStats;
	SFVALUE	m_LastMatchFaveWeaponName;
	SFVALUE	m_LastMatchFaveWeaponStats;
	SFVALUE	m_LastMatchPerfStats;
	SFVALUE	m_LastMatchMiscStats;

	SFVALUE	m_OverallPlayerName;
	SFVALUE	m_OverallMVPsText;
	SFVALUE	m_OverallPlayerStats;
	SFVALUE	m_OverallFaveWeaponName;
	SFVALUE	m_OverallFaveWeaponStats;
	SFVALUE	m_OverallFaveMapStats;

	int		m_iPlayerSlot;
	uint	m_mostRecentAchievementTime;		// UTC of most recently earned achievement (used for recent achievement highlight)

	int32 volatile m_nEloBracket; // Which elo bracket emblem to show in the ui.

	eDialogType m_type;
};

//=============================================================================
// HPE_END
//=============================================================================

#endif // CREATEMEDALSTATSDIALOG_H
#endif // include scaleform
