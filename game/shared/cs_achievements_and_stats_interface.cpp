//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose:
//
//=============================================================================

#include "cbase.h"

#include "cs_achievements_and_stats_interface.h"
#include "baseachievement.h"
#include "GameEventListener.h"
#include "../common/xlast_csgo/csgo.spa.h"
#include "iachievementmgr.h"
#include "utlmap.h"
#include "steam/steam_api.h"

#include "vgui_controls/Panel.h"
#include "vgui_controls/PHandle.h"
#include "vgui_controls/MenuItem.h"
#include "vgui_controls/messagedialog.h"

#include "cs_gamestats_shared.h"
#include "vgui/IInput.h"
#include "vgui/ILocalize.h"
#include "vgui/IPanel.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"


#if defined(CSTRIKE_DLL) && defined(CLIENT_DLL)

CSAchievementsAndStatsInterface::CSAchievementsAndStatsInterface() : AchievementsAndStatsInterface()
{
	m_pAchievementAndStatsSummary = NULL;

	g_pAchievementsAndStatsInterface = this;
}

void CSAchievementsAndStatsInterface::CreatePanel( vgui::Panel* pParent )
{
	// Create achievement & stats dialog if not already created
	if ( !m_pAchievementAndStatsSummary )
	{
		m_pAchievementAndStatsSummary = new CAchievementAndStatsSummary(NULL);
	}

	if ( m_pAchievementAndStatsSummary )
	{
		m_pAchievementAndStatsSummary->SetParent(pParent);
	}
}

void CSAchievementsAndStatsInterface::DisplayPanel()
{
	// Position & show dialog
	PositionDialog(m_pAchievementAndStatsSummary);
	m_pAchievementAndStatsSummary->Activate();
}

void CSAchievementsAndStatsInterface::ReleasePanel()
{
	// Make sure the BasePanel doesn't try to delete this, because it doesn't really own it.
	if ( m_pAchievementAndStatsSummary )
	{
		m_pAchievementAndStatsSummary->SetParent((vgui::Panel*)NULL);
	}
}

#endif

