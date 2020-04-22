//========= Copyright © 1996-2001, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef DMEDAGRENDERPANEL_H
#define DMEDAGRENDERPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"
#include "matsys_controls/PotteryWheelPanel.h"
#include "datamodel/dmehandle.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeDag;
class CDmeModel;
class CDmeAnimationList;
class CDmeChannelsClip;
class CDmeSourceSkin;
class CDmeSourceAnimation;
class CDmeDCCMakefile;
class CDmeDrawSettings;
class vgui::MenuBar;

namespace vgui
{
	class IScheme;
}


//-----------------------------------------------------------------------------
// Material Viewer Panel
//-----------------------------------------------------------------------------
class CDmeDagRenderPanel : public CPotteryWheelPanel
{
	DECLARE_CLASS_SIMPLE( CDmeDagRenderPanel, CPotteryWheelPanel );

public:
	// constructor, destructor
	CDmeDagRenderPanel( vgui::Panel *pParent, const char *pName );
	virtual ~CDmeDagRenderPanel();

	// Overriden methods of vgui::Panel
	virtual void PerformLayout();
	virtual void Paint();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	// Sets the current scene + animation list
	void SetDmeElement( CDmeDag *pScene );
	void SetAnimationList( CDmeAnimationList *pAnimationList );
	void SetVertexAnimationList( CDmeAnimationList *pAnimationList );
	void DrawJoints( bool bDrawJoint );
	void DrawJointNames( bool bDrawJointNames );
	void DrawGrid( bool bDrawGrid );
	void DrawAxis( bool bDrawAxis );
	void ModelInEngineCoordinates( bool bModelInEngineCoordinates );
	void ModelZUp( bool bModelZUp );

	CDmeDag *GetDmeElement();

	// Other methods which hook into DmePanel
	void SetDmeElement( CDmeSourceSkin *pSkin );
	void SetDmeElement( CDmeSourceAnimation *pAnimation );
	void SetDmeElement( CDmeDCCMakefile *pDCCMakefile );

	// Select animation by name
	void SelectAnimation( const char *pAnimName );
	void SelectVertexAnimation( const char *pAnimName );

private:
	// Select animation by index
	void SelectAnimation( int nIndex );
	void SelectVertexAnimation( int nIndex );

	// paint it!
	void OnPaint3D();
	void OnMouseDoublePressed( vgui::MouseCode code );
	virtual void OnKeyCodePressed( vgui::KeyCode code );

	MESSAGE_FUNC( OnSmoothShade, "SmoothShade" );
	MESSAGE_FUNC( OnFlatShade, "FlatShade" );
	MESSAGE_FUNC( OnWireframe, "Wireframe" );
	MESSAGE_FUNC( OnBoundingBox, "BoundingBox" );
	MESSAGE_FUNC( OnNormals, "Normals" );
	MESSAGE_FUNC( OnWireframeOnShaded, "WireframeOnShaded" );
	MESSAGE_FUNC( OnBackfaceCulling, "BackfaceCulling" );
	MESSAGE_FUNC( OnXRay, "XRay" );
	MESSAGE_FUNC( OnGrayShade, "GrayShade" );
	MESSAGE_FUNC( OnFrame, "Frame" );

	// Draw joint names
	void DrawJointNames( CDmeDag *pRoot, CDmeDag *pDag, const matrix3x4_t& parentToWorld );

	// Draw highlighted vertices
	void DrawHighlightPoints();

	// Draw the coordinate axis
	void DrawAxis();

	// Rebuilds the list of operators
	void RebuildOperatorList();

	// Update Menu Status
	void UpdateMenu();
	CTextureReference m_DefaultEnvCubemap;
	CTextureReference m_DefaultHDREnvCubemap;
	vgui::HFont m_hFont;
	CMaterialReference m_axisMaterial;

	bool m_bDrawJointNames : 1;
	bool m_bDrawJoints : 1;
	bool m_bDrawGrid : 1;
	bool m_bDrawAxis : 1;

	// NOTE: m_bModelInEngineCoordinates overrides m_bZUp
	bool m_bModelInEngineCoordinates : 1;	// Is the model already in engine coordinates

	// Note: m_bZUp implies the data is in Z Up coordinates with -Y as the forward axis
	bool m_bModelZUp : 1;					// Is the model Z Up?

	// NOTE: If neither m_bModelInEngineCoordinates nor m_bModelZUp is true then
	//       the model is in Y Up coordinates with Z as the forward axis

	CDmeHandle< CDmeAnimationList > m_hAnimationList;
	CDmeHandle< CDmeAnimationList > m_hVertexAnimationList;
	CDmeHandle< CDmeChannelsClip > m_hCurrentAnimation;
	CDmeHandle< CDmeChannelsClip > m_hCurrentVertexAnimation;
	CUtlVector< IDmeOperator* > m_operators;
	float m_flStartTime;
	CDmeHandle< CDmeDag > m_hDag;

	CDmeDrawSettings *m_pDrawSettings;
	CDmeHandle< CDmeDrawSettings, HT_STRONG > m_hDrawSettings;

	vgui::MenuBar *m_pMenuBar;

	// Menu item numbers
	vgui::Menu *m_pShadingMenu;
	int m_nMenuSmoothShade;
	int m_nMenuFlatShade;
	int m_nMenuWireframe;
	int m_nMenuBoundingBox;
	int m_nMenuNormals;
	int m_nMenuWireframeOnShaded;
	int m_nMenuBackfaceCulling;
	int m_nMenuXRay;
	int m_nMenuGrayShade;
};



#endif // DMEDAGRENDERPANEL_H