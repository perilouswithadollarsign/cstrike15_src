//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VNEWGAMEDIALOG_H__
#define __VNEWGAMEDIALOG_H__

#include "basemodui.h"

namespace BaseModUI {

class GenericPanelList;
class ChapterLabel;

class CNewGameDialog : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( CNewGameDialog, CBaseModFrame );

public:
	CNewGameDialog( vgui::Panel *pParent, const char *pPanelName, bool bIsCommentaryDialog );
	~CNewGameDialog();

	int GetNumAllowedChapters() { return m_nAllowedChapters; }
	bool IsCommentaryDialog() { return m_bIsCommentaryDialog; }

	MESSAGE_FUNC_CHARPTR( OnItemSelected, "OnItemSelected", pPanelName );

protected:
	virtual void OnCommand( char const *pCommand );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Activate();
	virtual void PaintBackground();
	virtual void OnKeyCodePressed( vgui::KeyCode code );

private:
	void UpdateFooter();
	void SetChapterImage( int nChapter, bool bIsLocked = false );
	int	GetImageId( const char *pImageName );
	void DrawChapterImage();

	bool				m_bIsCommentaryDialog;

	int					m_nAllowedChapters;

	CUtlVector< int >	m_ChapterImages;
	int					m_nVignetteImageId;
	int					m_nChapterImageId;
	bool				m_bDrawAsLocked;

	GenericPanelList	*m_pChapterList;
	vgui::ImagePanel	*m_pChapterImage;
	ChapterLabel		*m_pChapterLabel;
};

};

#endif
