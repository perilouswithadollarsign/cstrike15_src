//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef MDLPICKER_H
#define MDLPICKER_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlstring.h"
#include "vgui_controls/Frame.h"
#include "matsys_controls/baseassetpicker.h"
#include "datacache/imdlcache.h"



//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
	class Splitter;
}

class CMDLPanel;


//-----------------------------------------------------------------------------
// Purpose: Main app window
//-----------------------------------------------------------------------------
class CMDLPicker : public CBaseAssetPicker
{
	DECLARE_CLASS_SIMPLE( CMDLPicker, CBaseAssetPicker );

public:

	enum PageType_t
	{
		PAGE_NONE	= 0,
		PAGE_RENDER  = 0x1,
		PAGE_SEQUENCES = 0x2,
		PAGE_ACTIVITIES = 0x4,
		PAGE_SKINS = 0x8,
		PAGE_INFO = 0x10,
		PAGE_ALL	= 0xFFFFFFFF,
	};

	CMDLPicker( vgui::Panel *pParent, int nFlags = PAGE_ALL	);
	~CMDLPicker();

	// overridden frame functions
	virtual void PerformLayout();
	virtual void OnCommand( const char *pCommand );

	// Get current model
	void		GetSelectedMDLName( char *pBuffer, int nMaxLen );
	
	// get current selected options page
	int			GetSelectedPage();

	// Allows external apps to select a MDL
	void		SelectMDL( const char *pRelativePath );

	// Set/Get Sequence
	void		SelectSequence( const char *pSequenceName );
	const char *GetSelectedSequenceName();

	// Set/Get Activity
	void		SelectActivity( const char *pActivityName );
	const char *GetSelectedActivityName();

	void		SelectSkin( int nSkin );
	int			GetSelectedSkin();

private:
	MESSAGE_FUNC_PARAMS( OnAssetSelected, "AssetSelected", params );

	virtual void OnSelectedAssetPicked( const char *pMDLName );

	void RefreshActivitiesAndSequencesList();
	void RefreshRenderSettings();
	int	 UpdateSkinsList();
	void UpdateInfoTab();
	int  UpdatePropDataList( KeyValues *pkvPropData, bool &bIsStatic );

	// Plays the selected activity
	void PlaySelectedActivity( );

	// Plays the selected sequence
	void PlaySelectedSequence( );

	MESSAGE_FUNC_PARAMS( OnCheckButtonChecked, "CheckButtonChecked", kv );
	MESSAGE_FUNC_PARAMS( OnItemSelected, "ItemSelected", kv );
	MESSAGE_FUNC( OnPageChanged, "PageChanged" );	

	CMDLPanel *m_pMDLPreview;
	vgui::Splitter* m_pFileBrowserSplitter;
	vgui::Splitter* m_pPreviewSplitter;
	
	vgui::PropertySheet *m_pViewsSheet;
	vgui::PropertyPage *m_pRenderPage;
	vgui::PropertyPage *m_pSequencesPage;
	vgui::PropertyPage *m_pActivitiesPage;
	vgui::PropertyPage *m_pSkinsPage;
	vgui::PropertyPage *m_pInfoPage;

	vgui::ListPanel *m_pSequencesList;
	vgui::ListPanel *m_pActivitiesList;
	vgui::ListPanel	*m_pSkinsList;
	vgui::ListPanel *m_pPropDataList;

    MDLHandle_t m_hSelectedMDL;

	int m_nFlags;

	friend class CMDLPickerFrame;
};


//-----------------------------------------------------------------------------
// Purpose: Main app window
//-----------------------------------------------------------------------------
class CMDLPickerFrame : public CBaseAssetPickerFrame
{
	DECLARE_CLASS_SIMPLE( CMDLPickerFrame, CBaseAssetPickerFrame );

public:
	CMDLPickerFrame( vgui::Panel *pParent, const char *pTitle, int nFlags = CMDLPicker::PAGE_ALL );
	virtual ~CMDLPickerFrame();

	// Allows external apps to select a MDL
	void		SelectMDL( const char *pRelativePath );
};


#endif // MDLPICKER_H
