//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef PARTICLEPICKER_H
#define PARTICLEPICKER_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlstring.h"
#include "vgui_controls/Frame.h"
#include "matsys_controls/baseassetpicker.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace vgui
{
	class Splitter;
	class EditablePanel;
	class ScrollBar;
	class IScheme;
	class IImage;
}

class CParticleSystemPanel;
class CParticleSnapshotPanel;
class CParticleCollection;

//-----------------------------------------------------------------------------
// Purpose: Grid of particle snapshots
//-----------------------------------------------------------------------------

class CParticleSnapshotGrid: public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CParticleSnapshotGrid, vgui::EditablePanel );

public:
	CParticleSnapshotGrid( vgui::Panel *pParent, const char *pName );
	
	virtual void PerformLayout();
	virtual void OnTick();
	virtual void OnMousePressed(vgui::MouseCode code);
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnMouseWheeled(int delta);
	virtual void OnKeyCodeTyped( vgui::KeyCode code );

	/*
	Since the grid can be re-ordered:
	- "index" is the location of a preview panel in m_Panels, which determines physical placement
	- "id" is the location of the system in the original SetParticleList() array that was handed in (useful for clients)
	*/

	void SetParticleList( const CUtlVector<const char *>& ParticleNames );
	void LayoutScrolled();

	const char *GetSystemName( int nId );

	int GetSelectedSystemId( int nSelectionIndex );
	int GetSelectedSystemCount( );

	void SelectId( int nId, bool bAddToSelection, bool bToggle );
	void SelectSystem( const char *pSystemName, bool bAddToSelection, bool bToggle );
	void DeselectAll();

	MESSAGE_FUNC_CHARPTR( OnParticleSystemSelected, "ParticleSystemSelected", SystemName );
	MESSAGE_FUNC_CHARPTR( OnParticleSystemCtrlSelected, "ParticleSystemCtrlSelected", SystemName );
	MESSAGE_FUNC_CHARPTR( OnParticleSystemShiftSelected, "ParticleSystemShiftSelected", SystemName );
	MESSAGE_FUNC_CHARPTR( OnParticleSystemPicked, "ParticleSystemPicked", SystemName );

	MESSAGE_FUNC( OnScrollBarSliderMoved, "ScrollBarSliderMoved" );
	MESSAGE_FUNC_PARAMS( OnCheckButtonChecked, "CheckButtonChecked", kv );

	struct PSysRelativeInfo_t
	{
		CUtlString relName;
		bool bVisibleInCurrentView;
	};

private:
	void SelectIndex( int nIndex, bool bAddToSelection, bool bToggle );

	int IdToIndex( int nId );
	int InternalFindSystemIndexByName( const char *pSystemName );

	bool IsSystemVisible( int nInternalIndex );

	void UpdatePanelRelatives( int nInternalIndex );
	void MapSystemRelatives( );
	void UpdateAllRelatives( );

	void SetAllPreviewEnabled( bool bEnabled );

	int GetPanelWide();
	int GetPanelTall();

	vgui::Panel *m_pScrollPanel;
	vgui::Panel *m_pToolPanel;
	vgui::ScrollBar *m_pScrollBar;
	vgui::Label *m_pNoSystemsLabel;

	vgui::CheckButton *m_pPreviewCheckbox;

	CUtlVector<CParticleSnapshotPanel*> m_Panels;

	int m_nCurrentColCount;

	bool m_bAllowMultipleSelection;
	int m_nMostRecentSelectedIndex;

	vgui::IImage *m_pRelativesImgNeither;
	vgui::IImage *m_pRelativesImgPOnly;
	vgui::IImage *m_pRelativesImgCOnly;
	vgui::IImage *m_pRelativesImgBoth;

	CUtlVector< CUtlVector<PSysRelativeInfo_t> > m_ParentsMap;
	CUtlVector< CUtlVector<PSysRelativeInfo_t> > m_ChildrenMap;
};

//-----------------------------------------------------------------------------
// Particle picker panel
//-----------------------------------------------------------------------------
class CParticlePicker : public CBaseAssetPicker
{
	DECLARE_CLASS_SIMPLE( CParticlePicker, CBaseAssetPicker );

public:

	CParticlePicker( vgui::Panel *pParent );
	~CParticlePicker();

	virtual void PerformLayout();
	virtual void OnMousePressed(vgui::MouseCode code);

	void GetSelectedParticleSysName( char *pBuffer, int nMaxLen );
	void SelectParticleSys( const char *pRelativePath );

	// asset cache interface
	virtual int GetAssetCount();
	virtual const char *GetAssetName( int nAssetIndex );
	virtual const CachedAssetInfo_t& GetCachedAsset( int nAssetIndex );
	virtual int GetCachedAssetCount();
	virtual bool IncrementalCacheAssets( float flTimeAllowed ); // return true if finished
	virtual bool BeginCacheAssets( bool bForceRecache ); // return true if finished
	virtual CUtlString GetSelectedAssetFullPath( int nIndex );

protected:
	virtual void OnAssetListChanged( );

private:
	MESSAGE_FUNC_PARAMS( OnAssetSelected, "AssetSelected", params );
	MESSAGE_FUNC( OnParticleSystemSelectionChanged, "ParticleSystemSelectionChanged" );
	MESSAGE_FUNC_CHARPTR( OnParticleSystemPicked, "ParticleSystemPicked", SystemName );

	virtual void OnSelectedAssetPicked( const char *pParticleSysName );
	void CachePCFInfo( int nModIndex, const char *pFileName );
	void HandleModParticles( int nModIndex );

	vgui::Splitter* m_pFileBrowserSplitter;

	CParticleSnapshotGrid *m_pSnapshotGrid;

	friend class CParticlePickerFrame;
};


//-----------------------------------------------------------------------------
// Purpose: Main app window
//-----------------------------------------------------------------------------
class CParticlePickerFrame : public CBaseAssetPickerFrame
{
	DECLARE_CLASS_SIMPLE( CParticlePickerFrame, CBaseAssetPickerFrame );

public:
	CParticlePickerFrame( vgui::Panel *pParent, const char *pTitle );
	virtual ~CParticlePickerFrame();

	// Allows external apps to select a MDL
	void SelectParticleSys( const char *pRelativePath );
};


#endif // MDLPICKER_H
