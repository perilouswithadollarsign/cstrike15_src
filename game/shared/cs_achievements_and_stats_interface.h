//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#ifndef CSACHIEVEMENTSANDSTATSINTERFACE_H
#define CSACHIEVEMENTSANDSTATSINTERFACE_H
#ifdef _WIN32
#pragma once
#endif

#include "achievements_and_stats_interface.h"
#include "vgui/achievement_stats_summary.h"


#if defined(CSTRIKE_DLL) && defined(CLIENT_DLL)

class CSAchievementsAndStatsInterface : public AchievementsAndStatsInterface
{
public:
	CSAchievementsAndStatsInterface();

	virtual void CreatePanel( vgui::Panel* pParent );
	virtual void DisplayPanel();
	virtual void ReleasePanel();
	virtual int GetAchievementsPanelMinWidth( void ) const { return cAchievementsDialogMinWidth; }

protected:
	vgui::DHANDLE<vgui::Frame>  m_pAchievementAndStatsSummary;
};

#endif

#endif // CSACHIEVEMENTSANDSTATSINTERFACE_H
