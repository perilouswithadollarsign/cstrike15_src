//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VGAMESETTINGS_H__
#define __VGAMESETTINGS_H__

#include "basemodui.h"
#include "VFlyoutMenu.h"

namespace BaseModUI {

class DropDownMenu;

FlyoutMenu * UpdateChapterFlyout( KeyValues *pSettings, FlyoutMenuListener *pListener, DropDownMenu *pChapterDropDown );
int GetNumChaptersForMission( KeyValues *pSettings );
KeyValues *GetMapInfoAllowingAnyChapter( KeyValues *pSettings, KeyValues **ppMissionInfo );
KeyValues *GetMapInfoRespectingAnyChapter( KeyValues *pSettings, KeyValues **ppMissionInfo );

class GameSettings : public CBaseModFrame, public FlyoutMenuListener
{
	DECLARE_CLASS_SIMPLE( GameSettings, CBaseModFrame );

public:
	GameSettings(vgui::Panel *parent, const char *panelName);
	~GameSettings();

	void Activate();

	virtual void OnThink();

	void OnKeyCodePressed( vgui::KeyCode code );
	void OnCommand(const char *command);
	virtual void ApplySchemeSettings( vgui::IScheme* pScheme );
	virtual void PaintBackground();
	virtual void SetDataSettings( KeyValues *pSettings );
	
	//FloutMenuListener
	virtual void OnNotifyChildFocus( vgui::Panel* child );
	virtual void OnFlyoutMenuClose( vgui::Panel* flyTo );
	virtual void OnFlyoutMenuCancelled();

public:
	MESSAGE_FUNC_INT_CHARPTR( MsgOnCustomCampaignSelected, "OnCustomCampaignSelected", chapter, campaign );
	void SwitchToGameMode( char const *szNewGameMode, bool bConfirmed );

protected:
	virtual void OnClose();

	void Navigate();

	bool IsCustomMatchSearchCriteria();
	bool IsEditingExistingLobby();
	bool IsAnyChapterAllowed();

protected:
	void UpdateChapterImage( int nChapterIdx = -1, char const *szCampaign = NULL );
	int CountChaptersInCurrentCampaign();

	void SelectNetworkAccess( char const *szNetworkType, char const *szAccessType );
	void DoCustomMatch( char const *szGameState );

	void GenerateDefaultMissionAndChapter( char const *&szMission, int &nChapter );

private:
	void UpdateFooter();

	KeyValues *m_pSettings;
	KeyValues::AutoDelete m_autodelete_pSettings;
	
	bool m_bEditingSession;
	bool m_bPreventSessionModifications;
	void UpdateSessionSettings( KeyValues *pUpdate );

	DropDownMenu* m_drpDifficulty;
	DropDownMenu* m_drpRoundsLimit;
	DropDownMenu* m_drpMission;
	DropDownMenu* m_drpChapter;
	DropDownMenu* m_drpCharacter;
	DropDownMenu* m_drpGameAccess;
	DropDownMenu* m_drpServerType;

	bool m_bBackButtonMeansDone;
	bool m_bCloseSessionOnClose;
	bool m_bAllowChangeToCustomCampaign;
};

}

#endif
