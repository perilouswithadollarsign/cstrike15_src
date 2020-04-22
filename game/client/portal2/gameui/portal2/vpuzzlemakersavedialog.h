//========= Copyright (c) 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=======================================================================================//

#ifndef __VPUZZLEMAKERSAVEDIALOG_H__
#define __VPUZZLEMAKERSAVEDIALOG_H__

#if defined( PORTAL2_PUZZLEMAKER )

#include "basemodui.h"

class CDialogListButton;

namespace BaseModUI
{

class vgui::TextEntry;
class vgui::ImagePanel;
class vgui::CheckButton;
class vgui::Label;

enum PuzzleMakerSaveDialogReason_t
{
	PUZZLEMAKER_SAVE_UNSPECIFIED = 0,
	PUZZLEMAKER_SAVE_ONEXIT,
	PUZZLEMAKER_SAVE_FROMPAUSEMENU,
	PUZZLEMAKER_SAVE_RENAME,
	PUZZLEMAKER_SAVE_PUBLISHFROMPAUSE,
	PUZZLEMAKER_SAVE_SHORTCUT,
	PUZZLEMAKER_SAVE_FROM_NEW_CHAMBER,
	PUZZLEMAKER_SAVE_FROM_OPEN_CHAMBER,
	PUZZLEMAKER_SAVE_ON_QUIT_APP,
	PUZZLEMAKER_SAVE_JOIN_COOP_GAME
};

class CPuzzleMakerSaveDialog : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( CPuzzleMakerSaveDialog, CBaseModFrame );

public:
	CPuzzleMakerSaveDialog( vgui::Panel *pParent, const char *pPanelName );
	~CPuzzleMakerSaveDialog();

	void SetReason( PuzzleMakerSaveDialogReason_t eReason );
	void SetScreenshotName( const char *pszScreenshotName );

	virtual void PaintBackground();
	void DisplayPublishDialog();
protected:
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Activate();
	virtual void OnThink( void );
	virtual void OnCommand( char const *pszCommand );
	virtual void OnKeyCodePressed( vgui::KeyCode code );
	virtual void PerformLayout();

private:
	void UpdateFooter();
	void SetTextEntryBoxColors( vgui::TextEntry *pTextEntryBox, bool bHasFocus );

	void InitializeText();
	void SaveButtonPressed();
	void CloseUI();
	void PublishButtonPressed();
	void ViewButtonPressed();

	PuzzleMakerSaveDialogReason_t m_eReason;

	vgui::TextEntry *m_pChamberNameTextEntry;
	vgui::TextEntry *m_pChamberDescriptionTextEntry;

	vgui::CheckButton *m_pWorkshopAgreementCheckBox;
	vgui::Label *m_pWorkshopAgreementLabel;

	vgui::Label *m_pPublishButtonLabel;
	CDialogListButton *m_pPublishListButton;
	vgui::Label *m_pPublishDescriptionLabel;

	const wchar_t *m_pwscDefaultChamberName;
	const wchar_t *m_pwszDefaultChamberDescription;
	bool m_bFirstTime;

	char m_szScreenshotName[MAX_PATH];
	vgui::ImagePanel *m_pChamberImagePanel;
	int m_nScreenshotID;

	Color m_TextEntryBoxFocusFgColor;
	Color m_TextEntryBoxFocusBgColor;
	Color m_TextEntryBoxNonFocusFgColor;
	Color m_TextEntryBoxNonFocusBgColor;

	ERemoteStoragePublishedFileVisibility m_eVisibility;
};

};

#endif //PORTAL2_PUZZLEMAKER

#endif //__VPUZZLEMAKERSAVEDIALOG_H__
