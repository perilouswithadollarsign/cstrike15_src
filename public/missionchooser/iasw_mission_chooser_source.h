#ifndef _INCLUDED_IASW_MISSION_CHOOSER_SOURCE_H
#define _INCLUDED_IASW_MISSION_CHOOSER_SOURCE_H
#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"

struct ASW_Mission_Chooser_Mission
{
	char m_szMissionName[64];
};

struct ASW_Mission_Chooser_Saved_Campaign
{
	char m_szSaveName[64];
	char m_szCampaignName[64];
	char m_szDateTime[64];
	int m_iMissionsComplete;
	char m_szPlayerNames[256];
	char m_szPlayerIDs[512];
	bool m_bMultiplayer;
};

#define ASW_MISSIONS_PER_PAGE 8
#define ASW_CAMPAIGNS_PER_PAGE 3
#define ASW_SAVES_PER_PAGE 8

abstract_class IASW_Mission_Chooser_Source
{
public:
	virtual void Think() = 0;
	virtual void IdleThink() = 0;
	virtual void FindMissions(int nMissionOffset, int iNumSlots, bool bRequireOverview) = 0;
	virtual ASW_Mission_Chooser_Mission* GetMissions() = 0;	// pass an array of mission names back
	virtual ASW_Mission_Chooser_Mission* GetMission( int nIndex, bool bRequireOverview ) = 0;	// may return NULL if asking for a mission outside of the found range
	virtual int	 GetNumMissions(bool bRequireOverview) = 0;
	
	virtual void FindCampaigns(int nCampaignOffset, int iNumSlots) = 0;
	virtual ASW_Mission_Chooser_Mission* GetCampaigns() = 0;	// Passes an array of campaign names back
	virtual ASW_Mission_Chooser_Mission* GetCampaign( int nIndex ) = 0;		// may return NULL when requesting a campaign outside the found range
	virtual int	 GetNumCampaigns() = 0;

	virtual void FindSavedCampaigns(int page, int iNumSlots, bool bMultiplayer, const char *szFilterID) = 0;
	virtual ASW_Mission_Chooser_Saved_Campaign* GetSavedCampaigns() = 0;	// passes an array of summary data for each save
	virtual ASW_Mission_Chooser_Saved_Campaign* GetSavedCampaign( int nIndex, bool bMultiplayer, const char *szFilterID ) = 0;	// may return NULL when requesting a save outside the found range
	virtual int	 GetNumSavedCampaigns(bool bMultiplayer, const char *szFilterID) = 0;
	virtual void RefreshSavedCampaigns() = 0;	// call when the saved campaigns list is dirty and should be refreshed
	virtual int GetNumMissionsCompleted(const char *szSaveName) = 0;
	virtual void OnSaveDeleted(const char *szSaveName) = 0;	// call when a particular save has been deleted
	virtual void OnSaveUpdated(const char *szSaveName) = 0;	// call when a particular save has been updated

	// following only supporter by the local mission source
	virtual bool MissionExists(const char *szMapName, bool bRequireOverview) = 0;
	virtual bool CampaignExists(const char *szCampaignName) = 0;
	virtual bool SavedCampaignExists(const char *szSaveName) = 0;
	virtual bool ASW_Campaign_CreateNewSaveGame(char *szFileName, int iFileNameMaxLen, const char *szCampaignName, bool bMultiplayerGame) = 0;
	virtual void NotifySaveDeleted(const char *szSaveName) = 0;
	virtual const char* GetCampaignSaveIntroMap(const char* szSaveName) = 0;	// returns the intro map for the campaign that this save uses
	
	// returns nice version of the filenames (i.e. title from the overview.txt or from the campaign txt)
	virtual const char* GetPrettyMissionName(const char *szMapName) = 0;
	virtual const char* GetPrettyCampaignName(const char *szCampaignName) = 0;
	virtual const char* GetPrettySavedCampaignName(const char *szSaveName) = 0;	

	// needed by network source
	virtual void ResetCurrentPage() = 0;
};

#endif // _INCLUDED_IASW_MISSION_CHOOSER_SOURCE_H
