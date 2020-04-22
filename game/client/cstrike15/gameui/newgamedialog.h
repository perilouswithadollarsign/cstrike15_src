//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef NEWGAMEDIALOG_H
#define NEWGAMEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"
#include "vgui_controls/footerpanel.h"
#include "utlvector.h"

class CGameChapterPanel;
class CSkillSelectionDialog;

// Slot indices in new game menu
#define INVALID_INDEX	-1
#define SLOT_OFFLEFT	0
#define SLOT_LEFT		1
#define SLOT_CENTER		2
#define SLOT_RIGHT		3
#define SLOT_OFFRIGHT	4
#define	NUM_SLOTS		5

//-----------------------------------------------------------------------------
// Purpose: Handles starting a new game, skill and chapter selection
//-----------------------------------------------------------------------------
class CNewGameDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CNewGameDialog, vgui::Frame );

public:
	MESSAGE_FUNC( FinishScroll,	"FinishScroll" );
	MESSAGE_FUNC( StartGame, "StartGame" );

	CNewGameDialog(vgui::Panel *parent, bool bCommentaryMode );
	~CNewGameDialog();

	virtual void	Activate( void );

	virtual void	ApplySettings( KeyValues *inResourceData );
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void	OnCommand( const char *command );
	virtual void	OnClose( void );
	virtual void	PaintBackground();
	void			SetSelectedChapterIndex( int index );
	void			SetSelectedChapter( const char *chapter );
	void			UpdatePanelLockedStatus( int iUnlockedChapter, int i, CGameChapterPanel *pChapterPanel );

	void			SetCommentaryMode( bool bCommentary ) { m_bCommentaryMode = bCommentary; }

	// Xbox: Defined values are also used to shift the slot indices
	enum EScrollDirection
	{
		SCROLL_RIGHT	= -1,
		SCROLL_NONE		=  0,
		SCROLL_LEFT		=  1
	};
	EScrollDirection	m_ScrollDirection;

private:
	int m_iSelectedChapter;

	CUtlVector<CGameChapterPanel *> m_ChapterPanels;

	vgui::DHANDLE<CSkillSelectionDialog> m_hSkillSelectionDialog;

	vgui::Button		*m_pPlayButton;
	vgui::Button		*m_pNextButton;
	vgui::Button		*m_pPrevButton;
	vgui::Panel			*m_pCenterBg;
	vgui::Label			*m_pChapterTitleLabels[2];
	vgui::Label			*m_pBonusSelection;
	vgui::ImagePanel	*m_pBonusSelectionBorder;
	vgui::CFooterPanel	*m_pFooter;
	bool				m_bCommentaryMode;
	vgui::Label			*m_pCommentaryLabel;

	// Xbox
	void	ScrollSelectionPanels( EScrollDirection dir );
	void	ScrollBonusSelection( EScrollDirection dir );
	void	PreScroll( EScrollDirection dir );
	void	PostScroll( EScrollDirection dir );
	void	SetFastScroll( bool fast );
	void	ContinueScrolling( void );
	void	AnimateSelectionPanels( void );
	void	ShiftPanelIndices( int offset );
	bool	IsValidPanel( const int idx );
	void	InitPanelIndexForDisplay( const int idx );
	void	UpdateMenuComponents( EScrollDirection dir );
	void	UpdateBonusSelection( void );

	int		m_PanelXPos[ NUM_SLOTS ];
	int		m_PanelYPos[ NUM_SLOTS ];
	float	m_PanelAlpha[ NUM_SLOTS ];
	int		m_PanelIndex[ NUM_SLOTS ];
	float	m_ScrollSpeed;
	int		m_ButtonPressed;
	int		m_ScrollCt;
	bool	m_bScrolling;
	char	m_ActiveTitleIdx;
	bool	m_bMapStarting;
	int		m_iBonusSelection;
	bool	m_bScrollToFirstBonusMap;
	
	struct BonusMapDescription_t	*m_pBonusMapDescription;
};

#endif // NEWGAMEDIALOG_H
