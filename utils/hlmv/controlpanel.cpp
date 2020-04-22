//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
//
//                 Half-Life Model Viewer (c) 1999 by Mete Ciragan
//
// file:           ControlPanel.cpp
// last modified:  May 29 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
// version:        1.2
//
// email:          mete@swissquake.ch
// web:            http://www.swissquake.ch/chumbalum-soft/
//
#include "ControlPanel.h"
#include "ViewerSettings.h"
#include "StudioModel.h"
#include "IStudioRender.h"
#include "MatSysWin.h"
#include "vphysics/constraints.h"
#include "physmesh.h"
#include "sys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mxtk/mx.h>
#include <mxtk/mxBmp.h>
#include "vphysics_interface.h"
#include "UtlVector.h"
#include "UtlSymbol.h"
#include "UtlBuffer.h"
#include "attachments_window.h"
#include "istudiorender.h"
#include "studio_render.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "tier1/keyvalues.h"
#include "tier0/icommandline.h"
#include "mdlobjects/dmehitbox.h"
#include "mdlobjects/dmehitboxset.h"
#include "mdlobjects/dmehitboxsetlist.h"
#include "datamodel/idatamodel.h"
#include "tier2/tier2.h"
#include "tier2/p4helpers.h"
#include "valve_ipc_win32.h"
#include "mdlviewer.h"
#include "materialsystem/imaterialvar.h"
#include "tier1/fmtstr.h"
#include "mathlib/softbodyenvironment.h"

extern char g_appTitle[];
extern IPhysicsSurfaceProps *physprop;
extern void LoadPhysicsProperties( void );
extern ISoundEmitterSystemBase *g_pSoundEmitterBase;
extern CValveIpcClientUtl g_HlmvIpcClient;
extern bool g_bHlmvMaster;
extern CSoftbodyEnvironment g_SoftbodyEnvironment;

bool g_OnlyEditMaterialsThatWantToBeEdited = false;
class CTextBuffer
{
public:
	CTextBuffer( void ) {}
	~CTextBuffer( void ) {}
	inline int GetSize( void ) { return m_buffer.Count(); }
	inline char *GetData( void ) { return m_buffer.Base(); }
	void WriteText( const char *pText )
	{
		int len = strlen( pText );
		CopyData( pText, len );
	}
	void Terminate( void ) { CopyData( "\0", 1 ); }
	void CopyData( const char *pData, int len )
	{
		int offset = m_buffer.AddMultipleToTail( len );
		memcpy( m_buffer.Base() + offset, pData, len );
	}
private:
	CUtlVector<char> m_buffer;
};

//-----------------------------------------------------------------------------
// Reads all of the physics materials from surface property file
//-----------------------------------------------------------------------------

static void ReadPhysicsMaterials( mxChoice *plist )
{
	LoadPhysicsProperties();
	plist->removeAll();

	if ( !physprop || !physprop->SurfacePropCount() )
	{
		plist->add("default");
		plist->select(0);
		return;
	}

	for ( int i = 0; i < physprop->SurfacePropCount(); i++ )
	{
		plist->add(physprop->GetPropName( i ) );
	}
	plist->select(0);
}


//-----------------------------------------------------------------------------
// Populates a control with all the physics bones
//-----------------------------------------------------------------------------

static bool PopulatePhysicsBoneList( mxChoice* pChoice )
{
	pChoice->removeAll();

	if ( g_pStudioModel->Physics_GetBoneCount() )
	{
		for ( int i = 0; i < g_pStudioModel->Physics_GetBoneCount(); i++ )
		{
			pChoice->add (g_pStudioModel->Physics_GetBoneName( i ) );
		}
	}
	else
	{
		pChoice->add( "None" );
		pChoice->select (0);
		return false;
	}

	pChoice->select (0);
	return true;
}

//-----------------------------------------------------------------------------
// Populates a control with all the sound names
//-----------------------------------------------------------------------------

static void PopulateSoundNameList( mxListBox *pListBox )
{
	pListBox->removeAll();

	if ( g_pSoundEmitterBase )
	{
		for ( int i = g_pSoundEmitterBase->First(); i != g_pSoundEmitterBase->InvalidIndex(); i = g_pSoundEmitterBase->Next( i ) )
		{
			pListBox->add( g_pSoundEmitterBase->GetSoundName( i ) );
		}
	}

	pListBox->select( 0 );
	pListBox->deselect( 0 );
}

//-----------------------------------------------------------------------------
// Populates a control with all the physics bones
//-----------------------------------------------------------------------------

bool PopulateBoneList( mxChoice* pChoice, bool bAlwaysAddNone = false )
{
	pChoice->removeAll();

	if ( bAlwaysAddNone )
	{
		pChoice->add( "(None)" );
	}

	if ( g_pStudioModel )
	{
		CStudioHdr *pHdr = g_pStudioModel->GetStudioHdr();
		if ( pHdr && pHdr->numbones() > 0 )
		{
			for ( int i = 0; i < pHdr->numbones(); i++ )
			{
				pChoice->add ( pHdr->pBone(i)->pszName() );
			}

			pChoice->select(0);
			return true;
		}
	}

	if ( !bAlwaysAddNone )
	{
		pChoice->add( "(None)" );
	}

	pChoice->select(0);
	return false;
}

//-----------------------------------------------------------------------------
// Populates a control with all the attachments
//-----------------------------------------------------------------------------

void PopulateAttachmentsList( mxChoice *pChoice )
{
	pChoice->removeAll();

	pChoice->add( "(none)" );

	if ( g_pStudioModel )
	{
		CStudioHdr* pHdr = g_pStudioModel->GetStudioHdr();
		if ( pHdr && pHdr->GetNumAttachments() > 0 )
		{
			for ( int i = 0; i < pHdr->GetNumAttachments(); i++ )
			{
				pChoice->add( pHdr->pAttachment(i).pszName() );
			}
		}
	}

	pChoice->select(0);
}

//-----------------------------------------------------------------------------
// Sets the text of a lineedit to the current frame
//-----------------------------------------------------------------------------
void SetFrameString( mxLineEdit2 *pLineEdit, int iLayer )
{
	float flFrame = g_pStudioModel->GetFrame( iLayer );
	int nFrame = int( flFrame + 0.5f );

	char msg[ 16 ];
	sprintf( msg, "%d", nFrame );
	pLineEdit->setText( msg );
}

//-----------------------------------------------------------------------------
// Finds the index of a surface prop
//-----------------------------------------------------------------------------

static int FindSurfaceProp( const char* pSurfaceProp )
{
	for (int i = 0; i < physprop->SurfacePropCount(); ++i)
	{
		if (!stricmp( physprop->GetPropName( i ), pSurfaceProp ))
			return i;
	}
	return -1;
}

//-----------------------------------------------------------------------------
// The tab associated with bone control
//-----------------------------------------------------------------------------

class CBoneControlWindow : public mxWindow
{
public:
	CBoneControlWindow( ControlPanel* pParent );

	void Init( );

	// This gets called when the model is loaded and unloaded
	void OnLoadModel();
	void OnUnloadModel();

	// Handles various events
	int handleEvent (mxEvent *event);

	// Called when we're selected
	void OnTabSelected();

	int		GetHitboxSet( void );
private:
	// Called when the bone is selected
	void OnBoneSelected( int boneIndex );
	void OnBoneHighlighted( bool isChecked );
	void OnHitboxHighlighted( bool isChecked );
	void OnShowDefaultPose( bool isChecked );
	void OnHitboxSelected( int hitbox );
	void OnHitboxGroupChanged( );
	void OnHitboxChanged( );
	void OnHitboxSetChanged( void );
	void OnAddHitbox( );
	void OnDeleteHitbox( );
	void OnGenerateQC( );
	void OnSaveHitboxes( );
	void OnLoadHitboxes( );
	void OnAutogenerateHitboxes( bool isChecked );

	// Writes out qc-style text to a utlbuffer
	bool SerializeQC( CUtlBuffer& buf );

	// Sets the surface property
	void OnSurfaceProp( int propIndex );

	// Duplictes the surface property to all children
	void OnSurfacePropApplyToChildren( );

	void RefreshHitbox( );
	void ComputeHitboxList( );

	void ComputeHitboxSetList( void );
	void OnHitboxAddSet( void );
	void OnHitboxDeleteSet( void );
	void OnHitboxSetChangeName( void );

	// Generates a list of all hitboxes per bone
	void PopulateHitboxLists();

	// Applies a surface property to all children
	void OnSurfacePropApplyToChildren_R( int bone, CUtlSymbol prop );

	// Selects the bone
	mxChoice* m_cBone;

	mxChoice*	m_cHitboxSet;

	mxLineEdit	*m_eHitboxSetName;
	mxButton	*m_bHitboxSetUpdateName;
	mxButton	*m_bAddHitboxSet;
	mxButton	*m_bDeleteHitboxSet;

	// Selects a hitbox associated with a bone
	mxChoice* m_cHitbox;

	// The materials to assign to the bone
	mxChoice* m_cSurfaceProp;

	// Are we highlighting bones?
	mxCheckBox* m_cBoneHighlight;

	// render bone names
	mxCheckBox* m_cBoneNames;

	// Are we highlighting hitboxes?
	mxCheckBox* m_cHitboxHighlight;

	// Should we show the default pose?
	mxCheckBox* m_cShowDefaultPose;

	// Should hitboxes be autogenerated?
	mxCheckBox* m_cAutoHitbox;

	// The control panel to which we're attached
	ControlPanel* m_pControlPanel;

	// Hitbox group
	mxLineEdit*	m_eHitboxGroup;

	// Hitbox name
	mxLineEdit* m_eHitboxName;

	mxChoice* m_cHitboxEditMode;
	
	// Hitbox buttons
	mxButton*	m_bUpdateHitbox;
	mxButton*	m_bAddHitbox;
	mxButton*	m_bDeleteHitbox;

	// The list of hitboxes per bone...
	typedef CUtlVector< int	>	BoneHitboxList_t;
	typedef CUtlVector<	BoneHitboxList_t > BoneHitboxes_t;
	
	CUtlVector< BoneHitboxes_t >	m_SetBoneHitBoxes;

	// Currently selected hitbox + bone
	int	m_Bone;
	int	m_Hitbox;
	int m_nHitboxSet;
};


//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------

CBoneControlWindow::CBoneControlWindow( ControlPanel* pParent ) : mxWindow( pParent, 0, 0, 0, 0 )
{
	m_pControlPanel = pParent;

	m_nHitboxSet = 0;
}

//-----------------------------------------------------------------------------
// Sets up all the controls in the window
//-----------------------------------------------------------------------------

void CBoneControlWindow::Init( )
{
	int left, top;
	left = 5;
	top = 0;

	// Bone selection list
	new mxLabel (this, left + 3, top + 4, 30, 18, "Bone");
	m_cBone = new mxChoice (this, left, top + 20, 260, 22, IDC_BONE_BONELIST);
	m_cBone->add ("None");
	m_cBone->select (0);
	mxToolTip::add (m_cBone, "Select a bone to modify");

	// Show bone checkbox
	m_cBoneHighlight = new mxCheckBox (this, left, top + 45, 90, 20, "Highlight", IDC_BONE_HIGHLIGHT_BONE);
	mxToolTip::add (m_cBoneHighlight, "Toggle display of the bone being modified");

	//m_cBoneNames
	// Show bone names
	m_cBoneNames = new mxCheckBox (this, left + 90, top + 45, 140, 20, "Names", IDC_BONE_NAMES);
	m_cBoneNames->setChecked( g_viewerSettings.showBoneNames );
	mxToolTip::add (m_cBoneNames, "Toggle bone names");

	// Bone surface property selection
	new mxLabel (this, left + 3, top + 68, 100, 18, "Bone Surface Prop");
	m_cSurfaceProp = new mxChoice( this, left, top + 85, 140, 22, IDC_BONE_SURFACEPROP );
	mxToolTip::add (m_cSurfaceProp, "Select a surface property to apply to the bone");

	// FIXME: We can't read the surface props yet because the vphysics path isn't known!!!
	// This will only add 'default' to the list
	ReadPhysicsMaterials( m_cSurfaceProp );

	// This button will apply the surface prop to all children
	mxButton *btnApplyChild = new mxButton( this, left, top + 115, 140, 20, "Apply to Children", IDC_BONE_APPLY_TO_CHILDREN );
	mxToolTip::add (btnApplyChild, "Apply the surface property to all child bones");

	// Use autogenerated hitboxes
	m_cAutoHitbox = new mxCheckBox (this, left, top + 140, 140, 20, "Autogenerate Hitboxes", IDC_BONE_USE_AUTOGENERATED_HITBOXES);
	mxToolTip::add (m_cAutoHitbox, "When this is checked, studiomdl will automatically generate hitboxes");

	m_cShowDefaultPose = new mxCheckBox (this, left, top + 160, 140, 20, "Show Default Pose", IDC_BONE_SHOW_DEFAULT_POSE);
	mxToolTip::add (m_cShowDefaultPose, "Toggles display of the default physics pose");

	left = 160;

	new mxLabel( this, left + 3, top + 50, 80, 18, "Hitbox Set" );
	m_cHitboxSet = new mxChoice (this, left, top + 66, 100, 22, IDC_BONE_HITBOXSET);
	m_cHitboxSet->add ("unnamed");
	m_cHitboxSet->select (0);
	mxToolTip::add (m_cHitboxSet, "Change hitbox set");

	m_eHitboxSetName = new mxLineEdit(this, left, top + 96, 100, 18, "", IDC_BONE_HITBOXSETNAME_EDIT);
	mxToolTip::add (m_eHitboxSetName, "Type in a name and hit the set button");

	m_bHitboxSetUpdateName = new mxButton( this, left, top + 116, 100, 16, "Set Name", IDC_BONE_HITBOXSETNAME );
	mxToolTip::add (m_bHitboxSetUpdateName, "Press to set hitbox set name from above text field");

	m_bAddHitboxSet = new mxButton( this, left, top + 140, 100, 20, "Add set", IDC_BONE_HITBOXADDSET );
	mxToolTip::add (m_bAddHitboxSet, "Add a hitbox set");
	m_bDeleteHitboxSet= new mxButton( this, left, top + 162, 100, 20, "Delete set", IDC_BONE_HITBOXDELETESET );
	mxToolTip::add (m_bDeleteHitboxSet, "Remove a hitbox set");

	left += 120;

	// Hitbox selection list
	new mxLabel (this, left + 3, top + 4, 30, 18, "Hitbox");
	m_cHitbox = new mxChoice (this, left, top + 20, 100, 22, IDC_BONE_HITBOXLIST);
	m_cHitbox->add ("None");
	m_cHitbox->select (0);
	mxToolTip::add (m_cHitbox, "Select a hitbox to modify");

	// Show hitbox checkbox
	m_cHitboxHighlight = new mxCheckBox (this, left + 3, top + 45, 100, 20, "Highlight Hitbox", IDC_BONE_HIGHLIGHT_HITBOX);
	mxToolTip::add (m_cHitboxHighlight, "Toggle display of the hitbox being modified");

	// Hitbox group
	new mxLabel (this, left + 3, top + 80, 80, 18, "Hitbox Group");
	m_eHitboxGroup = new mxLineEdit(this, left + 80, top + 75, 50, 22, "", IDC_BONE_HITBOX_GROUP);
	mxToolTip::add (m_eHitboxGroup, "The group of the current hitbox");

	// Hitbox name
	new mxLabel (this, left + 133, top + 80, 80, 18, "Hitbox Name");
	m_eHitboxName = new mxLineEdit(this, left + 133+80, top + 75, 50, 22, "", IDC_BONE_HITBOX_NAME);
	m_eHitboxName->setEnabled(false);
	mxToolTip::add (m_eHitboxName, "The name of the current hitbox");

	new mxLabel (this, left + 3,   top + 110, 80, 18, "Edit Hitbox:");
	m_cHitboxEditMode = new mxChoice( this, left + 80, top + 105, 200, 22, IDC_BONE_HITBOX_EDITMODE );
	m_cHitboxEditMode->add ("Rotate");
	m_cHitboxEditMode->add ("Translate BB Min");
	m_cHitboxEditMode->add ("Translate BB Max");
	m_cHitboxEditMode->select (0);
	
	// Update hitboxes here
	m_bUpdateHitbox = new mxButton( this, left, top + 163, 100, 20, "Update Hitbox", IDC_BONE_UPDATE_HITBOX );
	mxToolTip::add (m_bUpdateHitbox, "Apply hitbox group, origin, and size to the hitbox");

	// Load from a .hitbox file
	mxButton* btnLoadHitbox = new mxButton (this, left + 110, top + 163, 70, 20, "Load .HBX", IDC_BONE_LOAD_HITBOXES );
	mxToolTip::add (btnLoadHitbox, "Load hitboxes from a .hbx file");

	// Save as a .hitbox file
	mxButton* btnSaveHitbox = new mxButton (this, left + 190, top + 163, 70, 20, "Save .HBX", IDC_BONE_SAVE_HITBOXES );
	mxToolTip::add (btnSaveHitbox, "Save hitboxes to a .hbx file");

	left += 160;

	// Generate a QC file
	mxButton* btnGenerateQC = new mxButton (this, left, top + 10, 100, 20, "Generate QC", IDC_BONE_GENERATEQC );
	mxToolTip::add (btnGenerateQC, "Copy a .qc file snippet to the clipboard");

	// Add, remove hitboxes here
	m_bAddHitbox = new mxButton( this, left, top + 30, 100, 20, "Add Hitbox", IDC_BONE_ADD_HITBOX );
	m_bDeleteHitbox = new mxButton( this, left, top + 50, 100, 20, "Delete Hitbox", IDC_BONE_DELETE_HITBOX );
	mxToolTip::add (m_bAddHitbox, "Create a new hitbox attached to the current bone");
	mxToolTip::add (m_bDeleteHitbox, "Delete the currently selected hitbox");

	OnAutogenerateHitboxes( false );
}

//-----------------------------------------------------------------------------
// Generates a list of all hitboxes per bone
//-----------------------------------------------------------------------------

void CBoneControlWindow::PopulateHitboxLists()
{
	// Find the surface prop associated with this bone
	if (!g_pStudioModel)
		return;

	m_SetBoneHitBoxes.RemoveAll();
	for (int set = 0; set < g_pStudioModel->m_HitboxSets.Count(); set++ )
	{
		m_SetBoneHitBoxes.AddToTail();

		HitboxList_t &list = g_pStudioModel->m_HitboxSets[ set ].m_Hitboxes;
		for ( unsigned short i = list.Head(); i != list.InvalidIndex(); i = list.Next(i) )
		{
			mstudiobbox_t* pHitbox = &list[i].m_BBox;
			m_SetBoneHitBoxes[ set ].EnsureCount( pHitbox->bone + 1 );
			m_SetBoneHitBoxes[ set ][ pHitbox->bone ].AddToTail( i );
		}
	}
}

//-----------------------------------------------------------------------------
// This gets called when the model is loaded and unloaded
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnLoadModel()
{
	// Now that the vphysics path is known, we can load the surface props
	ReadPhysicsMaterials( m_cSurfaceProp );

	CStudioHdr* pHdr = g_pStudioModel->GetStudioHdr();
	m_cAutoHitbox->setChecked( (pHdr->flags() & STUDIOHDR_FLAGS_AUTOGENERATED_HITBOX) != 0 );
	OnAutogenerateHitboxes( m_cAutoHitbox->isChecked() );

	// Determine all bones for this model
	PopulateBoneList( m_cBone );

	ComputeHitboxSetList();
	PopulateHitboxLists();

	OnBoneSelected( 0 );
	g_bDrawModelInfoValid = false;
}

void CBoneControlWindow::OnUnloadModel()
{
	m_cBone->removeAll();
	m_cBone->add ("None");
	m_cBone->select (0);

	m_cAutoHitbox->setChecked( false );
}

//-----------------------------------------------------------------------------
// Called when we're selected
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnTabSelected()
{
	// Make the selected bone and highlight state match
	OnBoneSelected( m_cBone->getSelectedIndex() );
	OnBoneHighlighted( m_cBoneHighlight->isChecked() ); 
	OnHitboxHighlighted( m_cHitboxHighlight->isChecked() );
	OnShowDefaultPose( m_cShowDefaultPose->isChecked() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CBoneControlWindow::GetHitboxSet( void )
{
	return m_nHitboxSet;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBoneControlWindow::ComputeHitboxSetList( void )
{
	m_cHitboxSet->removeAll();

	for ( int i = 0 ; i < g_pStudioModel->m_HitboxSets.Count(); i++ )
	{
		const char *name = g_pStudioModel->m_HitboxSets[ i ].m_Name;
		m_cHitboxSet->add( name );
	}
	m_cHitboxSet->select( 0 );
}

//-----------------------------------------------------------------------------
// Recomputes the hitbox list
//-----------------------------------------------------------------------------

void CBoneControlWindow::ComputeHitboxList( )
{
	// Reset the hitbox list
	m_cHitbox->removeAll();
	int count = 0;
	if ( m_SetBoneHitBoxes.Count() > 0 )
	{
		if (m_SetBoneHitBoxes[ m_nHitboxSet ].Count() > m_Bone)
			count = m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone].Count();
		if (count > 0)
		{
			for (int i = 0; i < count; ++i )
			{
				char buf[32];
				sprintf( buf, "%d", m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone][i] );
				m_cHitbox->add( buf );
			}
		}
		else
		{
			m_cHitbox->add ("None");
		}
	}
	else
	{
		m_cHitbox->add ("None");
	}

	OnHitboxSelected(0);
}

//-----------------------------------------------------------------------------
// Here's what we do when a new bone is selected
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnBoneSelected( int boneIndex )
{
	// Reset the surface prop
	if (!g_pStudioModel)
		return;

	if ( physprop && g_pStudioModel->m_SurfaceProps.Count() )
	{
		// Find the surface prop associated with this bone
		const char* pSurfaceProp = g_pStudioModel->m_SurfaceProps[boneIndex].String();
		int idx = FindSurfaceProp( pSurfaceProp );

		// Can't find it? Then apply the default one
		if (idx < 0)
			idx = FindSurfaceProp( "default" );
		if (idx < 0)
			idx = 0;
		m_cSurfaceProp->select(idx);
	}
	else
	{
		m_cSurfaceProp->select(0);
	}

	// Store off the bone
	m_Bone = boneIndex;
	if (m_cBoneHighlight->isChecked())
		g_viewerSettings.highlightBone = m_Bone;

	ComputeHitboxList();

	// select the render/highlight bone
	m_pControlPanel->redraw();
}


//-----------------------------------------------------------------------------
// When the bone is highlighted or not
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnShowDefaultPose( bool isChecked )
{
	g_viewerSettings.showPhysicsPreview = isChecked;
}

//-----------------------------------------------------------------------------
// When the bone is highlighted or not
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnBoneHighlighted( bool isChecked )
{
	g_viewerSettings.highlightBone = isChecked ? m_Bone : -1;
}


//-----------------------------------------------------------------------------
// When the hitbox is highlighted or not
//-----------------------------------------------------------------------------
void CBoneControlWindow::OnHitboxHighlighted( bool isChecked )
{
	g_viewerSettings.highlightHitbox = isChecked ? m_Hitbox : -1;
}


//-----------------------------------------------------------------------------
// Refreshes the hitbox size
//-----------------------------------------------------------------------------
void CBoneControlWindow::RefreshHitbox( )
{
	if ( m_nHitboxSet < 0 || m_nHitboxSet >= g_pStudioModel->m_HitboxSets.Count() )
	{
		m_nHitboxSet = 0;
	}

	if ( m_Hitbox >= 0 && g_pStudioModel->m_HitboxSets.Count() > 0 )
	{
		// Set the hitbox size + origin + group
		mstudiobbox_t* pHitbox = &g_pStudioModel->m_HitboxSets[ m_nHitboxSet ].m_Hitboxes[ m_Hitbox ].m_BBox;

		m_eHitboxGroup->setLabel( "%i", pHitbox->group );
		const char *hitboxname = g_pStudioModel->m_HitboxSets[ m_nHitboxSet ].m_Hitboxes[ m_Hitbox ].m_Name;
		m_eHitboxName->setLabel( hitboxname );

	}
	else
	{
		m_eHitboxGroup->setLabel( "" );
		m_eHitboxName->setLabel( "" );
	}
}

//-----------------------------------------------------------------------------
// When a hitbox is selected
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnHitboxSelected( int hitbox )
{
	if ( m_nHitboxSet < 0 || m_nHitboxSet >= g_pStudioModel->m_HitboxSets.Count() )
	{
		m_nHitboxSet = 0;
	}

	m_cHitbox->select(hitbox);

	if ( m_SetBoneHitBoxes.Count() == 0 ||
		(m_SetBoneHitBoxes[ m_nHitboxSet ].Count() <= m_Bone) || 
		(m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone].Count() <= hitbox))
	{
		m_Hitbox = -1;
		CStudioHdr* pHdr = g_pStudioModel->GetStudioHdr();

		// This'll cause no boxes to be drawn
		if (m_cHitboxHighlight->isChecked())
			g_viewerSettings.highlightHitbox = pHdr ? pHdr->numbones() + 1 : 1;
	}
	else
	{
		m_Hitbox = m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone][hitbox];
		if (m_cHitboxHighlight->isChecked())
			g_viewerSettings.highlightHitbox = m_Hitbox;
	}

	RefreshHitbox();
}


//-----------------------------------------------------------------------------
// Hitbox size/origin changed
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnAutogenerateHitboxes( bool isChecked )
{
	m_eHitboxGroup->setEnabled( !isChecked );
	m_eHitboxName->setEnabled( !isChecked );
	m_bUpdateHitbox->setEnabled( !isChecked );
	m_bAddHitbox->setEnabled( !isChecked );
	m_bDeleteHitbox->setEnabled( !isChecked );

	m_cHitboxSet->setEnabled( !isChecked );
	m_eHitboxSetName->setEnabled( !isChecked );
	m_bHitboxSetUpdateName->setEnabled( !isChecked );
	m_bAddHitboxSet->setEnabled( !isChecked );
	m_bDeleteHitboxSet->setEnabled( !isChecked );
}

//-----------------------------------------------------------------------------
// Hitbox size/origin changed
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnHitboxChanged( )
{
	if ( m_nHitboxSet < 0 || m_nHitboxSet >= g_pStudioModel->m_HitboxSets.Count() )
	{
		m_nHitboxSet = 0;
	}

	if ( m_Hitbox < 0 || g_pStudioModel->m_HitboxSets.Count() <= 0 )
	{
		// Blat out the hitbox size + origin + group
		RefreshHitbox();
		return;
	}

	mstudiobbox_t* pHitbox;
	const char *pGroup;

	pHitbox = &g_pStudioModel->m_HitboxSets[ m_nHitboxSet ].m_Hitboxes[m_Hitbox].m_BBox;
	
	pGroup = m_eHitboxGroup->getLabel();
	if (pGroup)
	{
		pHitbox->group = atol( pGroup );
	}

	// Store off the hitbox name
	g_pStudioModel->m_HitboxSets[ m_nHitboxSet ].m_Hitboxes[m_Hitbox].m_Name = m_eHitboxName->getLabel();

	RefreshHitbox();
}

//-----------------------------------------------------------------------------
// Purpose: Hitbox set changed
//-----------------------------------------------------------------------------
void CBoneControlWindow::OnHitboxSetChanged( void )
{
	m_Hitbox = 0;
	m_nHitboxSet = m_cHitboxSet->getSelectedIndex();

	PopulateHitboxLists();

	// Repopulate other controls
	ComputeHitboxList();
	RefreshHitbox();
	//OnHitboxGroupChanged( );  // Refresh hitbox will update the group label from the hitbox's group.  No need to turn that back into a group #
}


//-----------------------------------------------------------------------------
// Hitbox size/origin changed
//-----------------------------------------------------------------------------
void CBoneControlWindow::OnHitboxGroupChanged( )
{
	if ( m_Hitbox >= 0 && g_pStudioModel->m_HitboxSets.Count() > 0 )
	{
		const char *pGroup = m_eHitboxGroup->getLabel();
		HitboxList_t &list = g_pStudioModel->m_HitboxSets[ m_nHitboxSet ].m_Hitboxes;
		if ( pGroup && list.IsInList( m_Hitbox ) )
		{
			mstudiobbox_t* pHitbox = &list[m_Hitbox].m_BBox;
			pHitbox->group = atol( pGroup );
		}
	}
}


//-----------------------------------------------------------------------------
// Add, remove hitboxes
//-----------------------------------------------------------------------------
void CBoneControlWindow::OnAddHitbox( )
{
	if (!g_pStudioModel)
		return;

	// Remove the 'none' entry
	if (m_SetBoneHitBoxes[ m_nHitboxSet ].Count() > 0 &&
		m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone].Count() == 0)
	{
		m_cHitbox->removeAll();
	}

	int i = g_pStudioModel->m_HitboxSets[ m_nHitboxSet ].m_Hitboxes.AddToTail();
	HitboxInfo_t &hitbox = g_pStudioModel->m_HitboxSets[ m_nHitboxSet ].m_Hitboxes[i];
	hitbox.m_BBox.bone = m_Bone;
	hitbox.m_BBox.group = 0;
	hitbox.m_BBox.bbmin.Init( -8, -8, -8 );
	hitbox.m_BBox.bbmax.Init( 8, 8, 8 );
	hitbox.m_BBox.szhitboxnameindex = 0;

	m_SetBoneHitBoxes[ m_nHitboxSet ].EnsureCount( m_Bone + 1 );
	m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone].AddToTail(i);

	char buf[32];
	sprintf(buf, "%d", i );
	m_cHitbox->add ( buf );
	OnHitboxSelected(m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone].Count() - 1);
}

void CBoneControlWindow::OnDeleteHitbox( )
{
	if ( !g_pStudioModel || ( m_Hitbox < 0 ) )
		return;

	g_pStudioModel->m_HitboxSets[ m_nHitboxSet ].m_Hitboxes.Remove(m_Hitbox);
	for (int i = m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone].Count(); --i >= 0; )
	{
		if (m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone][i] == m_Hitbox)
		{
			m_SetBoneHitBoxes[ m_nHitboxSet ][m_Bone].Remove(i);
			break;
		}
	}

	// Recompute the list of hitboxes
	ComputeHitboxList();
}


//-----------------------------------------------------------------------------
// Sets the surface property
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnSurfaceProp( int propIndex )
{
	if (g_pStudioModel->IsModelLoaded())
	{
		// Store off the new surface prop symbol
		CUtlSymbol prop( physprop->GetPropName( propIndex ) );
		g_pStudioModel->m_SurfaceProps[m_Bone] = prop;
	}
}

//-----------------------------------------------------------------------------
// Duplictes the surface property to all children
//-----------------------------------------------------------------------------

void CBoneControlWindow::OnSurfacePropApplyToChildren_R( int bone, CUtlSymbol prop )
{
	g_pStudioModel->m_SurfaceProps[bone] = prop;

	CStudioHdr* pHdr = g_pStudioModel->GetStudioHdr();
	for ( int i = 0; i < pHdr->numbones(); i++ )
	{
		const mstudiobone_t* pBone = pHdr->pBone(i);
		if (pBone->parent == bone)
		{
			OnSurfacePropApplyToChildren_R( i, prop );
		}
	}
}

void CBoneControlWindow::OnSurfacePropApplyToChildren( )
{
	if (!g_pStudioModel)
		return;

	CUtlSymbol prop = g_pStudioModel->m_SurfaceProps[m_Bone];
	OnSurfacePropApplyToChildren_R( m_Bone, prop );
}

//-----------------------------------------------------------------------------
// Writes out qc-style text to a utlbuffer
//-----------------------------------------------------------------------------

bool CBoneControlWindow::SerializeQC( CUtlBuffer& buf )
{
	CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
	if (!hdr)
		return false;

	buf.Printf("// .qc block generated by HLMV begins.\n\n");

	// Print out the surface props
	buf.Printf( "$surfaceprop \"%s\"\n", g_pStudioModel->m_SurfaceProps[0].String() );
	Assert( g_pStudioModel->m_SurfaceProps.Count() == hdr->numbones() );

	int i;
	for ( i = 1; i < g_pStudioModel->m_SurfaceProps.Count(); ++i)
	{
		const mstudiobone_t* pBone = hdr->pBone(i);

		// Don't bother printing out the name if it's got the same
		// surface prop as the parent does
		if (pBone->parent >= 0)
		{
			if (!stricmp( g_pStudioModel->m_SurfaceProps[i].String(), 
							g_pStudioModel->m_SurfaceProps[pBone->parent].String() ))
				continue;
		}

		buf.Printf( "$jointsurfaceprop \"%s\"\t \"%s\"\n", pBone->pszName(), 
			g_pStudioModel->m_SurfaceProps[i].String() );
	}

	if (!m_cAutoHitbox->isChecked())
	{
		buf.Printf("\n");

		float flInvScale = 1.0f;// / 1.07f;

		for ( i = 0 ; i < g_pStudioModel->m_HitboxSets.Count(); i++ )
		{
			buf.Printf( "\n$hboxset \"%s\"\n\n", g_pStudioModel->m_HitboxSets[ i ].m_Name.String() );

			HitboxList_t &list = g_pStudioModel->m_HitboxSets[ i ].m_Hitboxes;
			for ( unsigned short j = list.Head(); j != list.InvalidIndex(); j = list.Next(j) )
			{
				mstudiobbox_t &hitbox = list[j].m_BBox;
				const mstudiobone_t* pBone = hdr->pBone( hitbox.bone );
				buf.Printf( "$hbox %d \"%s\"\t  %7.2f %7.2f %7.2f  %7.2f %7.2f %7.2f  %7.2f %7.2f %7.2f %7.2f", 
					hitbox.group, pBone->pszName(), 
					hitbox.bbmin.x * flInvScale, hitbox.bbmin.y * flInvScale, hitbox.bbmin.z * flInvScale,
					hitbox.bbmax.x * flInvScale, hitbox.bbmax.y * flInvScale, hitbox.bbmax.z * flInvScale,
					hitbox.angOffsetOrientation.x, hitbox.angOffsetOrientation.y, hitbox.angOffsetOrientation.z,
					hitbox.flCapsuleRadius );
				if ( !list[j].m_Name.IsEmpty() )
				{
					buf.Printf( " \"%s\"", list[j].m_Name.String() );
				}
				buf.Printf( "\n" );
			}
		}
	}

	buf.Printf("\n// .qc block generated by HLMV ends.\n\n");

	return true;
}

//-----------------------------------------------------------------------------
// Generates the QC file and copies it to the clipboard
//-----------------------------------------------------------------------------
void CBoneControlWindow::OnGenerateQC( )
{
	CUtlBuffer outbuf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	SerializeQC( outbuf );
	if ( outbuf.TellPut() )
	{
		// Null-terminate the string so CopyString works...
		outbuf.PutChar('\0');
		Sys_CopyStringToClipboard( (const char*)outbuf.Base() );
	}
}


//-----------------------------------------------------------------------------
// Generates a hitbox file's dmelements
//-----------------------------------------------------------------------------
static CDmElement *GenerateHitboxFileElements( )
{
	CStudioHdr* pHdr = g_pStudioModel->GetStudioHdr();
	Assert( g_pStudioModel->m_SurfaceProps.Count() == pHdr->numbones() );

	CDmeHitboxSetList *pRoot = CreateElement< CDmeHitboxSetList >( "hitboxSetList", DMFILEID_INVALID );

	int nSetCount = g_pStudioModel->m_HitboxSets.Count();
	for ( int i = 0; i < nSetCount; ++i )
	{
		CDmeHitboxSet *pHitboxSet = CreateElement< CDmeHitboxSet >( g_pStudioModel->m_HitboxSets[ i ].m_Name, DMFILEID_INVALID );
		pRoot->m_HitboxSetList.AddToTail( pHitboxSet );

		HitboxList_t &list = g_pStudioModel->m_HitboxSets[ i ].m_Hitboxes;
		for ( unsigned short j = list.Head(); j != list.InvalidIndex(); j = list.Next(j) )
		{
			const mstudiobbox_t &srcHitbox = list[ j ].m_BBox;
			const char *pHitboxName = list[ j ].m_Name;
			const mstudiobone_t* pBone = pHdr->pBone( srcHitbox.bone );

			CDmeHitbox *pHitbox = CreateElement< CDmeHitbox >( pHitboxName, DMFILEID_INVALID );
			pHitboxSet->m_HitboxList.AddToTail( pHitbox );

			pHitbox->m_vMinBounds = srcHitbox.bbmin;
			pHitbox->m_vMaxBounds = srcHitbox.bbmax;
			pHitbox->m_sBoneName = pBone->pszName();
			pHitbox->m_nGroupId = srcHitbox.group;
			pHitbox->m_sSurfaceProperty = g_pStudioModel->m_SurfaceProps[ srcHitbox.bone ].String();
		}
	}

	return pRoot;
}


//-----------------------------------------------------------------------------
// Saves hitboxes to a .hb file
//-----------------------------------------------------------------------------
void CBoneControlWindow::OnSaveHitboxes( )
{
	const char *pFileName = mxGetSaveFileName( this, 0, "*.hbx" );
	if ( !pFileName )
		return;

	char pActualFileName[MAX_PATH];
	Q_strncpy( pActualFileName, pFileName, sizeof(pActualFileName) );
	Q_DefaultExtension( pActualFileName, ".hbx", sizeof(pActualFileName) );

	CP4AutoEditAddFile autop4( pActualFileName );
	CDmElement *pRoot = GenerateHitboxFileElements();
	bool bOk = g_pDataModel->SaveToFile( pActualFileName, NULL, NULL, "hitbox", pRoot );
	DestroyElement( pRoot, TD_ALL );
	if ( !bOk )
	{
		Warning( "Error serializing hitbox file \"%s\"!\n", pActualFileName );
		return;
	}

}


//-----------------------------------------------------------------------------
// Loads hitboxes from dmelements
//-----------------------------------------------------------------------------
static void LoadHitboxesFromFile( CDmElement *pRoot )
{
	CStudioHdr* pHdr = g_pStudioModel->GetStudioHdr();
	Assert( g_pStudioModel->m_SurfaceProps.Count() == pHdr->numbones() );

	CDmrElementArray< CDmeHitboxSet > hitboxSetList( pRoot, "hitboxsets", false );
	if ( !hitboxSetList.IsValid() )
	{
		Warning( "Hitbox file contains no hitbox sets!\n" );
		return;
	}

	g_pStudioModel->m_HitboxSets.RemoveAll();

	int nHitboxSetCount = hitboxSetList.Count();
	for ( int i = 0; i < nHitboxSetCount; ++i )
	{
		CDmeHitboxSet *pSrcHitboxSet = hitboxSetList[i];

		// Add a new hitboxset
		HitboxSet_t *pHitboxSet = &g_pStudioModel->m_HitboxSets[ g_pStudioModel->m_HitboxSets.AddToTail() ];
		pHitboxSet->m_Name = pSrcHitboxSet->GetName();

		int nCount = pSrcHitboxSet->m_HitboxList.Count();
		for ( int j = 0; j < nCount; ++j )
		{
			CDmeHitbox *pSrcHitbox = pSrcHitboxSet->m_HitboxList[j];
			HitboxInfo_t *pHitbox = &pHitboxSet->m_Hitboxes[ pHitboxSet->m_Hitboxes.AddToTail() ];
			pHitbox->m_Name = pSrcHitbox->GetName();
			pHitbox->m_BBox.szhitboxnameindex = 0;
			pHitbox->m_BBox.group = pSrcHitbox->m_nGroupId;
			pHitbox->m_BBox.bbmin = pSrcHitbox->m_vMinBounds;
			pHitbox->m_BBox.bbmax = pSrcHitbox->m_vMaxBounds;

			bool bFoundBone = false;
			for ( int k = 0; k < pHdr->numbones(); ++k )
			{
				bFoundBone = !Q_stricmp( pHdr->pBone(k)->pszName(), pSrcHitbox->m_sBoneName );
				if ( bFoundBone )
				{
					pHitbox->m_BBox.bone = k;
					break;
				}
			}

			if ( !bFoundBone )
			{
				Warning( "HB file contained a reference to an unknown bone \"%s\"!\n", pSrcHitbox->m_sBoneName.Get() ); 
			}

			if ( bFoundBone && !pSrcHitbox->m_sSurfaceProperty.IsEmpty() )
			{
				// Store off the new surface prop symbol
				CUtlSymbol prop( pSrcHitbox->m_sSurfaceProperty );
				g_pStudioModel->m_SurfaceProps[ pHitbox->m_BBox.bone ] = prop;
			}
		}
	}
}

void CBoneControlWindow::OnLoadHitboxes( )
{
	const char *pFileName = mxGetOpenFileName( this, 0, "*.hbx" );
	if ( !pFileName )
		return;

	char pActualFileName[MAX_PATH];
	Q_strncpy( pActualFileName, pFileName, sizeof(pActualFileName) );
	Q_DefaultExtension( pActualFileName, ".hbx", sizeof(pActualFileName) );

	CDmElement *pRoot;
	DmFileId_t fileid = g_pDataModel->RestoreFromFile( pActualFileName, NULL, NULL, &pRoot, CR_FORCE_COPY );
	if ( fileid == DMFILEID_INVALID )
	{
		Warning( "Unable to read hitbox file \"%s\"\n", pActualFileName );
		return;
	}

	LoadHitboxesFromFile( pRoot );

	g_pDataModel->RemoveFileId( fileid );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBoneControlWindow::OnHitboxAddSet( void )
{
	char sz[ 32 ];
	sprintf( sz, "set%02i", g_pStudioModel->m_HitboxSets.Count() + 1 );

	int newsetnumber = g_pStudioModel->m_HitboxSets.AddToTail();
	g_pStudioModel->m_HitboxSets[ newsetnumber ].m_Name = sz;
	
	ComputeHitboxSetList();

	m_cHitboxSet->select( newsetnumber );

	OnHitboxSetChanged();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBoneControlWindow::OnHitboxDeleteSet( void )
{
	// Can't remove last element
	if ( m_nHitboxSet == 0 )
	{
		return;
	}

	g_pStudioModel->m_HitboxSets.Remove( m_nHitboxSet );

	ComputeHitboxSetList();

	m_cHitboxSet->select( 0 );

	OnHitboxSetChanged();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBoneControlWindow::OnHitboxSetChangeName( void )
{
	if ( g_pStudioModel->m_HitboxSets.Count() <= 0 )
		return;

	char newname[ 512 ];

	strcpy( newname, m_eHitboxSetName->getLabel() );
	if ( !newname[ 0 ] )
		return;

	g_pStudioModel->m_HitboxSets[ m_nHitboxSet ].m_Name = newname;

	int oldsel = m_nHitboxSet;

	ComputeHitboxSetList();

	m_cHitboxSet->select( oldsel );

	OnHitboxSetChanged();
}

//-----------------------------------------------------------------------------
// Responds to events on controls in the window
//-----------------------------------------------------------------------------

int CBoneControlWindow::handleEvent (mxEvent *event)
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	if ( event->event == mxEvent::KeyDown )
	{
		// FIXME: calling this forces the edit box to lose position
 		// OnHitboxChanged( );
		return 1;
	}

	switch( event->action )
	{
	case IDC_BONE_BONELIST:
		if (g_pStudioModel->IsModelLoaded())
		{
 			OnHitboxGroupChanged( );
			OnBoneSelected( m_cBone->getSelectedIndex() );
		}
		break;
	case IDC_BONE_NAMES:
		g_viewerSettings.showBoneNames = ((mxCheckBox *) event->widget)->isChecked();
		break;

	case IDC_BONE_HIGHLIGHT_BONE:
		OnBoneHighlighted( ((mxCheckBox *) event->widget)->isChecked() );
		break;

	case IDC_BONE_HIGHLIGHT_HITBOX:
 		OnHitboxHighlighted( ((mxCheckBox *) event->widget)->isChecked() );
		break;

	case IDC_BONE_SHOW_DEFAULT_POSE:
 		OnShowDefaultPose( ((mxCheckBox *) event->widget)->isChecked() );
		break;

	case IDC_BONE_HITBOXLIST:
 		OnHitboxGroupChanged( );
 		OnHitboxSelected( m_cHitbox->getSelectedIndex() );
		break;

	case IDC_BONE_HITBOXSET:
		OnHitboxSetChanged();
 		break;

	case IDC_BONE_HITBOXADDSET:
		OnHitboxAddSet();
		break;
	case IDC_BONE_HITBOXDELETESET:
		OnHitboxDeleteSet();
		break;
	case IDC_BONE_HITBOXSETNAME:
		OnHitboxSetChangeName();
		break;

	case IDC_BONE_UPDATE_HITBOX:
 		OnHitboxChanged( );
		break;

	case IDC_BONE_HITBOX_EDITMODE:
		g_viewerSettings.hitboxEditMode = (HitboxEditMode)m_cHitboxEditMode->getSelectedIndex();
		break;

	case IDC_BONE_ADD_HITBOX:
 		OnHitboxGroupChanged( );
 		OnAddHitbox( );
		break;

	case IDC_BONE_DELETE_HITBOX:
 		OnDeleteHitbox( );
		break;

	case IDC_BONE_USE_AUTOGENERATED_HITBOXES:
		if (g_pStudioModel->IsModelLoaded())
 			OnAutogenerateHitboxes( m_cAutoHitbox->isChecked() );
		break;

	case IDC_BONE_SURFACEPROP:
 		OnSurfaceProp( m_cSurfaceProp->getSelectedIndex() );
		break;

	case IDC_BONE_APPLY_TO_CHILDREN:
		if (g_pStudioModel->IsModelLoaded())
	 		OnSurfacePropApplyToChildren( );
		break;

	case IDC_BONE_GENERATEQC:
		if (g_pStudioModel->IsModelLoaded())
		{
 			OnHitboxGroupChanged( );
	 		OnGenerateQC( );
		}
		break;

	case IDC_BONE_SAVE_HITBOXES:
		if ( g_pStudioModel->IsModelLoaded() && !m_cAutoHitbox->isChecked() )
		{
			OnHitboxGroupChanged( );
			OnSaveHitboxes( );
		}
		break;

	case IDC_BONE_LOAD_HITBOXES:
		if ( g_pStudioModel->IsModelLoaded() )
		{
			OnLoadHitboxes( );
			OnHitboxGroupChanged( );
		}
		break;

	default:
		return 0;
	}

	return 1;
}


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------

ControlPanel *g_ControlPanel = 0;

#define TAB_RENDER		0
#define TAB_SEQUENCE	1
#define TAB_BODY		2
#define TAB_FLEX		3
#define TAB_PHYSICS		4
#define TAB_BONE		5
#define TAB_ATTACHMENT	6
#define TAB_IK			7
#define TAB_EVENT		8

ControlPanel::ControlPanel( mxWindow *parent )
: mxWindow( parent, 0, 0, 0, 0, "Control Panel", mxWindow::Normal )
{
	InitViewerSettings ( "hlmv" );

	// frame slider
	new mxLabel( this, 5, 220, 50, 18, "Frame" );
	slForceFrame = new mxSlider( this, 55, 220, 450, 18, IDC_FORCEFRAME );
	slForceFrame->setRange( 0, 1.0 );
	slForceFrame->setValue( 0.0 );
	slForceFrame->setSteps( 1, 1 );
	mxToolTip::add( slForceFrame, "Force To Frame" );
	lForcedFrame = new mxLabel( this, 505, 220, 30, 18, "0" );

	// create tabcontrol with subdialog windows
	tab = new mxTab( this, 0, 20, 0, 0, IDC_TAB );
#ifdef WIN32
	SetWindowLong( ( HWND )tab->getHandle(), GWL_EXSTYLE, WS_EX_CLIENTEDGE );
#endif

	SetupRenderWindow( tab );
	SetupSequenceWindow( tab );
	SetupBodyWindow( tab );
	SetupFlexWindow( tab );
	SetupPhysicsWindow( tab );
	SetupSoftbodyWindow( tab );
	SetupBoneControlWindow( tab );
	SetupAttachmentsWindow( tab );
	SetupIKRuleWindow( tab );
	SetupEventWindow( tab );
	SetupMatVarWindow( tab );
	SetupSubmodelWindow( tab );
	SetupCompileWindow( tab );

	g_ControlPanel = this;

	memset( iSelectionToSequence, 0, sizeof(iSelectionToSequence ) );
	memset( iSequenceToSelection, 0, sizeof(iSequenceToSelection ) );

	memset( m_iSavedSequences, 0, sizeof(m_iSavedSequences) );
	memset( m_flSavedWeights, 0, sizeof(m_flSavedWeights) );
}

void ControlPanel::UpdateSubmodelSelection( void )
{
	int iSelectedSubmodel = cSubmodelList->getSelectedIndex();

	bool bSubmodelButtonsEnabled = (iSelectedSubmodel != -1);
	bSubmodelRemoveSelected->setEnabled( bSubmodelButtonsEnabled );
	cSubmodelAttachTo->setEnabled( bSubmodelButtonsEnabled );
	cSubmodelLocalAttachOrigin->setEnabled( bSubmodelButtonsEnabled );

	if ( bSubmodelButtonsEnabled )
	{
		cSubmodelAttachTo->removeAll();
		cSubmodelAttachTo->add( "_none_" );
		cSubmodelAttachTo->select(0);
		CStudioHdr *pHdr = g_pStudioModel->GetStudioHdr();
		if ( pHdr )
		{
			for ( int n = 0; n < pHdr->numbones(); n++ )
			{
				const mstudiobone_t *pBone = pHdr->pBone(n);
				if ( pBone )
				{
					cSubmodelAttachTo->add( pBone->pszName() );
					if ( Q_stricmp(g_MergeModelBonePairs[iSelectedSubmodel].szTargetBone, pBone->pszName()) == 0 )
					{
						cSubmodelAttachTo->select(n+1);
					}
				}
			}
			for ( int n = 0; n < pHdr->GetNumAttachments(); n++ )
			{
				mstudioattachment_t &pModelAttachment = (mstudioattachment_t &)pHdr->pAttachment( n );
				cSubmodelAttachTo->add( pModelAttachment.pszName() );
				if ( Q_stricmp(g_MergeModelBonePairs[iSelectedSubmodel].szTargetBone, pModelAttachment.pszName()) == 0 )
				{
					cSubmodelAttachTo->select(pHdr->numbones()+n+1);
				}
			}
		}

		cSubmodelLocalAttachOrigin->removeAll();
		cSubmodelLocalAttachOrigin->add( "_none_" );
		cSubmodelLocalAttachOrigin->select(0);
		CStudioHdr *pHdrSub = g_pStudioExtraModel[iSelectedSubmodel]->GetStudioHdr();
		if ( pHdrSub )
		{
			for ( int n = 0; n < pHdrSub->numbones(); n++ )
			{
				const mstudiobone_t *pBone = pHdrSub->pBone(n);
				if ( pBone )
				{
					cSubmodelLocalAttachOrigin->add( pBone->pszName() );
					if ( Q_stricmp(g_MergeModelBonePairs[iSelectedSubmodel].szLocalBone, pBone->pszName()) == 0 )
					{
						cSubmodelLocalAttachOrigin->select(n+1);
					}
				}
			}
			for ( int n = 0; n < pHdrSub->GetNumAttachments(); n++ )
			{
				mstudioattachment_t &pModelAttachment = (mstudioattachment_t &)pHdrSub->pAttachment( n );
				cSubmodelLocalAttachOrigin->add( pModelAttachment.pszName() );
				if ( Q_stricmp(g_MergeModelBonePairs[iSelectedSubmodel].szLocalBone, pModelAttachment.pszName()) == 0 )
				{
					cSubmodelLocalAttachOrigin->select(pHdrSub->numbones()+n+1);
				}
			}
		}

	}
	else
	{
		cSubmodelAttachTo->removeAll();
		cSubmodelLocalAttachOrigin->removeAll();
	}
}

void ControlPanel::UpdateSubmodelWindow( void )
{
	cSubmodelList->removeAll();
	for ( int i = 0; i < HLMV_MAX_MERGED_MODELS; i++ )
	{
		if ( g_viewerSettings.mergeModelFile[i][0] != 0 )
		{
			cSubmodelList->add( g_viewerSettings.mergeModelFile[i] );
		}
	}
	UpdateSubmodelSelection();
}

void ControlPanel::SetupSubmodelWindow( mxTab* pTab )
{

	mxWindow *wSubmodels = new mxWindow (this, 0, 0, 0, 0);
	tab->add (wSubmodels, "Submodels");

	bSubmodelAdd = new mxButton( wSubmodels, 2, 10, 90, 70, "Add Submodel", IDC_SUBMODEL_LOADMERGEDMODEL );
	bSubmodelAddSteam = new mxButton( wSubmodels, 2, 90, 90, 20, "Add [Steam]", IDC_SUBMODEL_LOADMERGEDMODEL_STEAM );
	bSubmodelRemoveAll = new mxButton( wSubmodels, 2, 120, 90, 50, "Remove All", IDC_SUBMODEL_UNLOADALLMERGEDMODELS );

	//new mxLabel( wSubmodels, 100, 2, 100, 18, "Loaded Submodels:" );
	cSubmodelList = new mxListBox( wSubmodels, 100, 10, 450, 175, IDC_SUBMODEL_UPDATE_SELECTION );
	mxToolTip::add (cSubmodelList, "Select submodels to add/remove/change");

	bSubmodelRemoveSelected = new mxButton( wSubmodels, 555, 10, 120, 20, "Remove Selected", IDC_SUBMODEL_UNLOADMERGEDMODEL );
	bSubmodelRemoveSelected->setEnabled(false);

	new mxLabel( wSubmodels, 555, 45, 160, 18, "Force attach to:" );
	cSubmodelAttachTo = new mxChoice( wSubmodels, 555, 60, 200, 20, IDC_SUBMODEL_UPDATE_BONESELECTION );
	cSubmodelAttachTo->setEnabled(false);

	new mxLabel( wSubmodels, 555, 80, 160, 18, "From local attach origin:" );
	cSubmodelLocalAttachOrigin = new mxChoice( wSubmodels, 555, 95, 200, 20, IDC_SUBMODEL_UPDATE_BONESELECTION );
	cSubmodelLocalAttachOrigin->setEnabled(false);
	
}

void ControlPanel::SetupMatVarWindow( mxTab* pTab )
{
	mxWindow *wMatVars = new mxWindow (this, 0, 0, 0, 0);
	tab->add (wMatVars, "Materials");

	new mxLabel( wMatVars, 2, 2, 100, 18, "Materials:" );
	cMaterialList = new mxListBox( wMatVars, 0, 20, 200, 170, IDC_MATERIALVARMATS );
	cMaterialList->add ("None");
	cMaterialList->select (1);
	mxToolTip::add (cMaterialList, "Materials (VMT files) this model has loaded");

	new mxLabel( wMatVars, 202, 2, 100, 18, "Material Parameters:" );
	cMaterialParamList = new mxListBox( wMatVars, 200, 20, 200, 170, IDC_MATERIALVARPARAMS );
	cMaterialParamList->add ("None");
	cMaterialParamList->select (1);
	mxToolTip::add (cMaterialParamList, "Material parameters of this material");

	new mxLabel (wMatVars, 405, 2, 100, 18, "Modify Parameter:");
	
	leMaterialParamText = new mxLineEdit2(wMatVars, 405, 25, 510, 24, "", IDC_MATVAREDIT);
	leMaterialParamText->setVisible(false);

	lblMatrixRotation = new mxLabel (wMatVars, 405, 55, 70, 18, "Rotation:");
	slMaterialParamMatrixSliderRotation = new mxSlider(wMatVars, 465, 55, 450, 20, IDC_MATVARSLIDERMATRIX);
	slMaterialParamMatrixSliderRotation->setRange( -180.0, 180.0 );
	slMaterialParamMatrixSliderRotation->setSteps( 1, 1 );
	slMaterialParamMatrixSliderRotation->setValue( 0.0 );

	lblMatrixScaleX = new mxLabel (wMatVars, 405, 75, 70, 18, "Scale X:");
	slMaterialParamMatrixSliderScaleX = new mxSlider(wMatVars, 465, 75, 450, 20, IDC_MATVARSLIDERMATRIX);
	slMaterialParamMatrixSliderScaleX->setRange( -5.0, 5.0 );
	slMaterialParamMatrixSliderScaleX->setSteps( 1, 1 );
	slMaterialParamMatrixSliderScaleX->setValue( 1.0 );

	lblMatrixScaleY = new mxLabel (wMatVars, 405, 95, 70, 18, "Scale Y:");
	slMaterialParamMatrixSliderScaleY = new mxSlider(wMatVars, 465, 95, 450, 20, IDC_MATVARSLIDERMATRIX);
	slMaterialParamMatrixSliderScaleY->setRange( -5.0, 5.0 );
	slMaterialParamMatrixSliderScaleY->setSteps( 1, 1 );
	slMaterialParamMatrixSliderScaleY->setValue( 1.0 );

	lblMatrixTranslateX = new mxLabel (wMatVars, 405, 115, 70, 18, "Translate X:");
	slMaterialParamMatrixSliderTranslateX = new mxSlider(wMatVars, 465, 115, 450, 20, IDC_MATVARSLIDERMATRIX);
	slMaterialParamMatrixSliderTranslateX->setRange( -2.0, 2.0 );
	slMaterialParamMatrixSliderTranslateX->setSteps( 1, 1 );
	slMaterialParamMatrixSliderTranslateX->setValue( 0.0 );

	lblMatrixTranslateY = new mxLabel (wMatVars, 405, 135, 70, 18, "Translate Y:");
	slMaterialParamMatrixSliderTranslateY = new mxSlider(wMatVars, 465, 135, 450, 20, IDC_MATVARSLIDERMATRIX);
	slMaterialParamMatrixSliderTranslateY->setRange( -2.0, 2.0 );
	slMaterialParamMatrixSliderTranslateY->setSteps( 1, 1 );
	slMaterialParamMatrixSliderTranslateY->setValue( 0.0 );
	
	slMaterialParamMatrixSliderRotation->setVisible(false);
	slMaterialParamMatrixSliderScaleX->setVisible(false);
	slMaterialParamMatrixSliderScaleY->setVisible(false);
	slMaterialParamMatrixSliderTranslateX->setVisible(false);
	slMaterialParamMatrixSliderTranslateY->setVisible(false);
	lblMatrixRotation->setVisible(false);
	lblMatrixScaleX->setVisible(false);
	lblMatrixScaleY->setVisible(false);
	lblMatrixTranslateX->setVisible(false);
	lblMatrixTranslateY->setVisible(false);

	bMaterialParamColor = new mxButton( wMatVars, 405, 55, 100, 30, "Color picker", IDC_MATVARCOLORPICKER );
	bMaterialParamColor->setVisible(false);

	slMaterialParamFloat = new mxSlider(wMatVars, 405, 55, 510, 20, IDC_MATVARSLIDERFLOAT);
	slMaterialParamFloat->setRange( -1.0, 1.0 );
	slMaterialParamFloat->setValue( 0.0 );
	slMaterialParamFloat->setVisible(false);

	cbMaterialParamMultiEdit = new mxCheckBox (wMatVars, 505, 0, 150, 20, "Affect all loaded materials", NULL);

#ifdef MATERIAL_SCRIPT_SAVE_FEATURE
	bMaterialParamSave = new mxButton( wMatVars, 405, 159, 100, 20, "Run Script", IDC_MATVARSAVE );
	leMaterialParamSavePath = new mxLineEdit2( wMatVars, 515, 159, 150, 20, "saved_material" );
	cbMaterialParamSaveRun = new mxCheckBox( wMatVars, 680, 159, 50, 20, "Run:", NULL );
	cbMaterialParamSaveRun->setChecked( true );
	leMaterialParamSaveRun = new mxLineEdit2( wMatVars, 725, 159, 190, 20, "material_ops.py" );
#endif

	bMaterialParamLoad = new mxButton( wMatVars, 405, 159, 100, 20, "Replace VMT", IDC_MATVARLOAD );
	mxToolTip::add (bMaterialParamLoad, "Temporarily replace this material with a custom set of VMT parameters.");
	bMaterialParamLoad->setVisible(false);

	bMaterialParamCopyToClipboard = new mxButton( wMatVars, 510, 159, 100, 20, "Copy to clipboard", IDC_MATVARCOPYTOCLIPBOARD );
	mxToolTip::add (bMaterialParamCopyToClipboard, "");
	bMaterialParamCopyToClipboard->setVisible(false);
}

void ControlPanel::SetupCompileWindow( mxTab* pTab )
{
	mxWindow *wCompile = new mxWindow (this, 0, 0, 0, 0);
	tab->add (wCompile, "Compile");

	new mxLabel( wCompile, 2, 2, 200, 15, "Recent QC scripts:" );

	cCompileRecentQCpaths = new mxListBox( wCompile, 2, 18, 300, 155, IDC_COMPILE_UPDATE_QCPATHSELECTION );
	cCompileRecentQCpaths->setEnabled(true);

	bCompileQCRemoveFromList = new mxButton( wCompile, 2, 168, 160, 18, "Remove selected from list", IDC_COMPILE_REMOVEFROMLIST );
	bCompileQCRemoveFromList->setEnabled(false);

	bCompileQCWhenSelected = new mxButton( wCompile, 170, 168, 130, 18, "One-click compile [OFF]", IDC_COMPILE_SELECTEDTOGGLE );
	bCompileQCWhenSelected->setEnabled(true);
	mxToolTip::add (bCompileQCWhenSelected, "When enabled, selecting a QC in the list will start a recompile automatically.");
	bCompileSelectedToggle = false;

	int nButtonsLeft = 310;
	int nButtonsTop = 22;

	lblFullQCPath = new mxLabel( wCompile, nButtonsLeft, 2, 500, 18, "No QC script selected. To add a QC file, drag and drop it onto the viewport." );

	bCompileQCCompile = new mxButton( wCompile, nButtonsLeft, nButtonsTop, 120, 24, "Recompile", IDC_COMPILE_CALLSTUDIOMDL );
	bCompileQCCompile->setEnabled(false);
	
	int nButtonsRight = 690;

	bCompileQCLoadModel = new mxButton( wCompile, nButtonsRight, nButtonsTop, 100, 24, "Load in HLMV", IDC_COMPILE_LOADMODELFILE );
	bCompileQCLoadModel->setEnabled(false);
	
	bCompileQCShowCompileOutput = new mxButton( wCompile, nButtonsRight + 110, nButtonsTop, 100, 24, "Open full log file", IDC_COMPILE_OPENLOGFILE );
	bCompileQCShowCompileOutput->setEnabled(false);

	bCompileQCBrowseToQC = new mxButton( wCompile, nButtonsRight + 220, nButtonsTop, 100, 24, "Explore to QC", IDC_COMPILE_EXPLORETOQC );
	bCompileQCBrowseToQC->setEnabled(false);

	cCompileQCOutput = new mxListBox(wCompile, nButtonsLeft, 56, 700, 130);

	lblCompileWarningOrError = new mxLabel( wCompile, nButtonsLeft, 168, 1000, 18, "" );

	nCompileLastUpdateTick = GetTickCount();
}

void ControlPanel::UpdateQCPathPanel( bool bUpdateList /* = true */, int nForceSelection /* = -1 */ )
{
	int nSelection = ( nForceSelection >= 0 ) ? nForceSelection : cCompileRecentQCpaths->getSelectedIndex();

	if ( bUpdateList )
	{
		cCompileRecentQCpaths->removeAll();
		FOR_EACH_VEC( g_QCPathRecords, i )
		{
			if ( i >= MAX_NUM_QCPATH_RECORDS )
				break;

			const char* szStatus = "";
			if ( g_QCPathRecords[i].status == QCSTATUS_COMPILING )
			{
				szStatus = " --> compiling...";
			}
			else if (g_QCPathRecords[i].status == QCSTATUS_ERROR)
			{
				szStatus = "[ ERROR! ]";
			}
			else if (g_QCPathRecords[i].status == QCSTATUS_COMPLETE_WITH_WARNING)
			{
				szStatus = "[ warning ]";
			}
			else if (g_QCPathRecords[i].status == QCSTATUS_COMPLETE)
			{
				szStatus = "[ COMPLETE ]";
			}

			char szStatusAndName[256];
			V_sprintf_safe( szStatusAndName, "%s %s", szStatus, g_QCPathRecords[i].szPrettyPath );

			cCompileRecentQCpaths->add( szStatusAndName );
		}
		cCompileRecentQCpaths->select(nSelection);
	}
	
	bCompileQCRemoveFromList->setEnabled(false);
	bCompileQCCompile->setEnabled(false);
	bCompileQCShowCompileOutput->setEnabled(false);
	bCompileQCBrowseToQC->setEnabled(false);
	bCompileQCLoadModel->setEnabled(false);

	if ( nSelection >= 0 && g_QCPathRecords.Count() > nSelection )
	{
		bCompileQCBrowseToQC->setEnabled(true);

		lblFullQCPath->setLabel( g_QCPathRecords[nSelection].szAbsPath );
		bCompileQCRemoveFromList->setEnabled(true);

		lblCompileWarningOrError->setLabel( g_QCPathRecords[nSelection].szMostRecentWarningOrError );

		if ( g_QCPathRecords[nSelection].status != QCSTATUS_COMPILING )
		{
			bCompileQCCompile->setEnabled(true);

			if ( strlen(g_QCPathRecords[nSelection].szModelPath) > 0 )
			{
				bCompileQCLoadModel->setEnabled(true);
			}
		}

		if ( g_QCPathRecords[nSelection].status != QCSTATUS_NOLOGFILE )
		{
			bCompileQCShowCompileOutput->setEnabled(true);
			
			cCompileQCOutput->removeAll();

			FILE *file = fopen( g_QCPathRecords[nSelection].szLogFilePath, "rt" );
			if ( file )
			{
				char line[1024];
				while ( fgets( line, 1024, file ) )
				{
					cCompileQCOutput->add( line );
				}
			}
			if ( file )
				fclose( file );
		}

	}
	
	SaveCompileQCPathSettings();
}

void ControlPanel::UpdateBoneWeightInspect( void )
{
	if ( g_viewerSettings.renderMode == RM_BONEWEIGHTS )
	{
		if (!g_pStudioModel)
			return;

		CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
		if (hdr)
		{
			int nNumBones = hdr->numbones();

			const mstudiobone_t *pbones = hdr->pBone( 0 );

			char szWeightResult[255];

			V_sprintf_safe( szWeightResult, "weights: " );

			for ( int i=0; i<3; i++ )
			{
				int nIndex = g_BoneWeightInspectResults[i].index;
				float flWeight = g_BoneWeightInspectResults[i].flweight;

				if ( nIndex < nNumBones && flWeight > 0 )
				{
					V_sprintf_safe( szWeightResult, "%s  %f [%s]", szWeightResult, flWeight, pbones[nIndex].pszName() );
				}
			}
			lblBoneWeightInspectValues->setLabel( szWeightResult );
		}
	}
}

void ControlPanel::CompileTimerUpdate( void )
{
	//update status of visible qcs

	if ( (GetTickCount() - nCompileLastUpdateTick) < 1000 )
		return;
	nCompileLastUpdateTick = GetTickCount();

	bool bNeedToRefreshList = false;

	int nTopIndex = cCompileRecentQCpaths->getTopIndex();
	for ( int i=0; i<11; i++ )
	{
		int nRecordIndex = nTopIndex + i;
		if ( nRecordIndex < g_QCPathRecords.Count() )
		{
			if ( g_QCPathRecords[nRecordIndex].status == QCSTATUS_UNKNOWN || g_QCPathRecords[nRecordIndex].status == QCSTATUS_COMPILING )
			{
				// need to parse the log file for changes
				FILE *file = fopen( g_QCPathRecords[nRecordIndex].szLogFilePath, "rt" );
				if ( file )
				{
					bool bFoundWarning = false;
					bool bFoundError = false;
					char line[1024];
					while ( fgets( line, 1024, file ) )
					{
						if ( !bFoundError && !bFoundWarning && V_stristr( line, "WARNING" ) )
						{
							V_strcpy_safe( g_QCPathRecords[nRecordIndex].szMostRecentWarningOrError, line );
							bFoundWarning = true;
						}

						if ( !bFoundError && V_stristr( line, "ERROR" ) )
						{
							V_strcpy_safe( g_QCPathRecords[nRecordIndex].szMostRecentWarningOrError, line );
							bFoundError = true;
						}

						if ( V_stristr( line, "OUTPUT MODEL: " ) )
						{
							char szTrimmedPath[1024];
							V_StrRight( line, strlen(line) - strlen("OUTPUT MODEL: "), szTrimmedPath, sizeof(szTrimmedPath) );
							V_FixSlashes( szTrimmedPath );
							V_strcpy_safe( g_QCPathRecords[nRecordIndex].szModelPath, szTrimmedPath );
						}

						if ( V_stristr( line, "RESULT: SUCCESS" ) )
						{
							if ( !bFoundWarning )
								V_strcpy_safe( g_QCPathRecords[nRecordIndex].szMostRecentWarningOrError, "Most recent compile finished successfully." );

							g_QCPathRecords[nRecordIndex].status = bFoundWarning ? QCSTATUS_COMPLETE_WITH_WARNING : QCSTATUS_COMPLETE;
							bNeedToRefreshList = true;

							char szCurrent[256];
							char szFinished[256];

							V_FileBase( g_pStudioModel->GetFileName(), szCurrent, sizeof(szCurrent) );
							V_FileBase( g_QCPathRecords[nRecordIndex].szModelPath, szFinished, sizeof(szFinished) );

							if ( !V_strcmp( szCurrent, szFinished ) )
							{
								g_MDLViewer->Refresh();
							}

							break;
						}
						else if ( V_stristr( line, "RESULT: ERROR" ) )
						{
							g_QCPathRecords[nRecordIndex].status = QCSTATUS_ERROR;
							bNeedToRefreshList = true;
							break;
						}
					}
				}
				else
				{
					// there's no log file. don't retry unless recompiled
					g_QCPathRecords[nRecordIndex].status = QCSTATUS_NOLOGFILE;
				}
				if ( file )
					fclose( file );
			}
		}
	}

	if ( bNeedToRefreshList )
	{
		UpdateQCPathPanel();
	}

}

void ControlPanel::CompileSelectedIndex( void )
{
	int nSelection = cCompileRecentQCpaths->getSelectedIndex();
	if ( nSelection >= 0 && g_QCPathRecords.Count() > nSelection && g_QCPathRecords[nSelection].status != QCSTATUS_COMPILING )
	{
		DeleteFile( TEXT(g_QCPathRecords[nSelection].szLogFilePath) );
		
		bCompileQCCompile->setEnabled(false);

		g_QCPathRecords[nSelection].status = QCSTATUS_COMPILING;

		// unghhhh here we go
		SECURITY_ATTRIBUTES sa;
		sa.nLength = sizeof(sa);
		sa.lpSecurityDescriptor = NULL;
		sa.bInheritHandle = TRUE;  

		HANDLE h = CreateFile( TEXT(g_QCPathRecords[nSelection].szLogFilePath),
			FILE_GENERIC_WRITE,
			FILE_SHARE_WRITE | FILE_SHARE_READ,
			&sa,
			CREATE_NEW,
			FILE_ATTRIBUTE_NORMAL,
			NULL );

		PROCESS_INFORMATION pi; 
		STARTUPINFO si;
		BOOL ret = FALSE; 
		DWORD flags = CREATE_NO_WINDOW;

		ZeroMemory( &pi, sizeof(PROCESS_INFORMATION) );
		ZeroMemory( &si, sizeof(STARTUPINFO) );
		si.cb = sizeof(STARTUPINFO); 
		si.dwFlags |= STARTF_USESTDHANDLES;
		si.hStdInput = NULL;
		si.hStdError = h;
		si.hStdOutput = h;

		char cmd[2048];
		V_snprintf( cmd, sizeof(cmd), "studiomdl.exe -parsecompletion %s", g_QCPathRecords[nSelection].szAbsPath );

		ret = CreateProcess( NULL, TEXT(cmd), NULL, NULL, TRUE, flags, NULL, TEXT(g_QCPathRecords[nSelection].szCWDPath), &si, &pi);

		if ( ret ) 
		{
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}

		if ( h )
		{
			CloseHandle( h );
		}

		qcpathrecord_t temp = g_QCPathRecords[nSelection];
		g_QCPathRecords.Remove(nSelection);
		g_QCPathRecords.AddToHead(temp);

		UpdateQCPathPanel( true, 0 );
	}
}

void ControlPanel::AddQCRecordPath( const char* szPath, bool bCompileWhenLoaded /* = false */ )
{
	if ( !V_stristr( szPath, ".qc" ) )
		return;

	FOR_EACH_VEC_BACK( g_QCPathRecords, i )
	{
		if ( g_QCPathRecords[i].DoesAbsPathMatch(szPath) )
		{
			 g_QCPathRecords.Remove(i);
		}
	}

	g_QCPathRecords[ g_QCPathRecords.AddToHead() ].InitFromAbsPath( szPath );

	UpdateQCPathPanel( true, bCompileWhenLoaded ? 0 : -1 );

	if ( bCompileWhenLoaded )
	{
		CompileSelectedIndex();
	}
}

//-----------------------------------------------------------------------------
// Sets up the window dealing with render control
//-----------------------------------------------------------------------------

void ControlPanel::SetupRenderWindow( mxTab* pTab )
{
	wRender = new mxWindow (this, 0, 0, 0, 0);
	tab->add (wRender, "Render");
	cRenderMode = new mxChoice (wRender, 5, 2, 100, 22, IDC_RENDERMODE);
	cRenderMode->add ("Wireframe");
//	cRenderMode->add ("Flatshaded");
	cRenderMode->add ("Smoothshaded");
	cRenderMode->add ("Textured");
	cRenderMode->add ("BoneWeights");
	cRenderMode->add ("BadVertexData");
	cRenderMode->add ("UV Chart");
	cRenderMode->add ("Co-LocatedVerts");
	cRenderMode->select (2);
	mxToolTip::add (cRenderMode, "Select Render Mode");

	new mxLabel (wRender, 450, 103, 200, 18, "Inspect bone weights:");
	cbBoneWeightInspectIndex = new mxChoice( wRender, 450, 120, 80, 20, IDC_BONEWEIGHTINDEX );
	lblBoneWeightInspectValues = new mxLabel( wRender, 530, 120, 600, 20, "" );

	for ( int i=0; i<32766; i++ )
	{
		char szNumber[16];
		itoa( i, szNumber, 10 );
		cbBoneWeightInspectIndex->add( szNumber );
	}

	cbGround = new mxCheckBox (wRender, 125, 5, 150, 20, "Ground (Ctrl-G)", IDC_GROUND);
	cbGround->setEnabled( true );
	cbMovement = new mxCheckBox (wRender, 125, 25, 150, 20, "Movement (Ctrl-M)", IDC_MOVEMENT);
	cbMovement->setEnabled( true );
	cbBackground = new mxCheckBox (wRender, 125, 45, 150, 20, "Background (Ctrl-B)", IDC_BACKGROUND);
	cbBackground->setEnabled( true );
	cbHitBoxes = new mxCheckBox (wRender, 125, 65, 150, 20, "Hit Boxes (Ctrl-H)", IDC_HITBOXES);
	cbSequenceBoxes = new mxCheckBox (wRender, 125, 85, 150, 20, "Seq. Boxes", IDC_SEQUENCEBOXES);
	cbShadow = new mxCheckBox (wRender, 125, 105, 150, 20, "Shadow (Ctrl-S)", IDC_SHADOW);
	cbSoftwareSkin = new mxCheckBox (wRender, 125, 125, 150, 20, "Software Skin", IDC_SOFTWARESKIN);
	cbSoftwareSkin->setEnabled( true );
	cbOverbright2 = new mxCheckBox (wRender, 125, 145, 150, 20, "Enable Overbrightening", IDC_OVERBRIGHT2);
	cbOverbright2->setEnabled( true );
	cbOverbright2->setChecked( true );
	setOverbright( true );

	cbAttachments = new mxCheckBox (wRender, 5, 45, 120, 20, "Attachments (Ctrl-A)", IDC_ATTACHMENTS);
	cbAttachments->setEnabled( true );

	cbBones = new mxCheckBox (wRender, 5, 65, 120, 20, "Bones (Ctrl-O)", IDC_BONES);

	cbNormals = new mxCheckBox (wRender, 5, 85, 120, 20, "Normals (Ctrl-N)", IDC_NORMALS);
	cbNormals->setEnabled( true );
	cbNormals->setChecked( false );

	cbTangentFrame = new mxCheckBox( wRender, 5, 105, 120, 20, "Tangents (Ctrl-T)", IDC_TANGENTFRAME );
	cbTangentFrame->setEnabled( true );
	cbTangentFrame->setChecked( false );

	cbOverlayWireframe = new mxCheckBox (wRender, 5, 125, 120, 20, "Wireframe (Ctrl-W)", IDC_OVERLAY_WIREFRAME);
	cbOverlayWireframe->setEnabled( true );
	cbOverlayWireframe->setChecked( false );

//	cbParallaxMap = new mxCheckBox (wRender, 5, 125, 100, 20, "Parallax Mapping", IDC_PARALLAXMAP);
//	cbParallaxMap->setEnabled( true );
//	cbParallaxMap->setChecked( true );

	cbSpecular = new mxCheckBox( wRender, 5, 145, 120, 20, "Specular", IDC_SPECULAR );
	cbSpecular->setEnabled( true );
	cbSpecular->setChecked( true );

	cbNormalMap = new mxCheckBox( wRender, 5, 25, 100, 20, "Normal Mapping", IDC_NORMALMAP );
	cbNormalMap->setEnabled( true );
	cbNormalMap->setChecked( false );

	cbDisplacementMap = new mxCheckBox( wRender, 275, 45, 150, 20, "Displacement (Ctrl-D)", IDC_DISPLACEMENTMAP );
	cbDisplacementMap->setEnabled( true );
	cbDisplacementMap->setChecked( true );

	cbRunIK = new mxCheckBox (wRender, 275, 65, 150, 20, "Enable IK", IDC_RUNIK);
	cbEnableHead = new mxCheckBox (wRender, 275, 85, 150, 20, "Head Turn", IDC_HEADTURN);

	cbIllumPosition = new mxCheckBox (wRender, 275, 105, 150, 20, "Illum. Position", IDC_ILLUMPOSITION);

	cbPlaySounds = new mxCheckBox (wRender, 275, 125, 150, 20, "Play Sounds", IDC_PLAYSOUNDS);

	cbShowOriginAxis = new mxCheckBox (wRender, 275, 145, 150, 20, "Show Origin Axis", IDC_SHOWORIGINAXIS);

	new mxLabel (wRender, 275, 170, 45, 18, "Axis Len:");
	leOriginAxisLength = new mxSlider(wRender, 320, 165, 105, 22, IDC_ORIGINAXISLENGTH);
	leOriginAxisLength->setRange( 1, 100 );
	leOriginAxisLength->setValue( 10 );

	new mxCheckBox (wRender, 275, 5, 150, 20, "Physics Model", IDC_PHYSICSMODEL);
	cHighlightBone = new mxChoice (wRender, 275, 25, 150, 22, IDC_PHYSICSHIGHLIGHT);
	cHighlightBone->add ("None");
	cHighlightBone->select (0);
	mxToolTip::add (cHighlightBone, "Select Physics Bone to highlight");

	new mxLabel (wRender, 450, 29, 60, 18, "HitBox Set:");
	cDrawHitBoxSet = new mxChoice (wRender, 510, 25, 90, 22, IDC_DRAWHITBOXSET);
	cDrawHitBoxSet->add ("All");
	cDrawHitBoxSet->select (0);
	cDrawHitBoxSet->setEnabled( false );

	new mxLabel (wRender, 450, 48, 60, 18, "HitBox:");
	cDrawHitBoxNumber = new mxChoice (wRender, 510, 48, 90, 22, IDC_DRAWHITBOXNUMBER);
	cDrawHitBoxNumber->setEnabled( false );

	new mxLabel (wRender, 5, 170, 30, 18, "FOV:");
	leFOV = new mxLineEdit(wRender, 35, 165, 30, 22, "65", IDC_RENDER_FOV);

	cIncludedModels = new mxChoice( wRender, 450, 68, 250, 20, IDC_INCLUDEDMODELS );
	cIncludedModels->add( "Included Models" );
	cIncludedModels->add( "---------------" );
	cIncludedModels->setEnabled( false );
}

//-----------------------------------------------------------------------------
// Updates control sizes for the window dealing with sequence control
//-----------------------------------------------------------------------------

void ControlPanel::updateSequenceSizes( int tabWidth )
{
	int extraWidth = (tabWidth - 640);
	int extraPoseParamWidth = 0;
	if ( extraWidth > 200 )
	{
		extraPoseParamWidth = extraWidth - 200;
		extraWidth = 200;
	}
	if ( extraWidth < 0 )
	{
		extraWidth = 0;
	}

	extraPoseParamWidth = clamp( extraPoseParamWidth, 0, 200 );

	for ( int i = 0; i < MAX_SEQUENCES; i++ )
	{
		cSequence[i]->setBounds( 5, 5 + i * 22, 200 + extraWidth, 22 + 500 );	// mxChoice adds 500 internally for the dropdown in constructor, not in setBounds
		slSequence[i]->setBounds( 208 + extraWidth, 5 + i * 22, 80, 18 );
		rbFrameSelection[i]->setBounds( 300 + extraWidth, 5 + i * 22, 35, 22 );

		leSequenceFilter[i]->setBounds( 520 + extraWidth + 96 + extraPoseParamWidth + 90, 5 + i * 22, 100, 18 );
	}

	laFilters->setBounds( 520 + extraWidth + 96 + extraPoseParamWidth + 5, 5, 80, 18 );

	laGroundSpeed->setBounds( 208 + extraWidth, 5, 80, 18 );

	for ( int i = 0; i < NUM_POSEPARAMETERS; i++ )
	{
		int x, y;
		x = 334;
		y = 2 + (i % 8) * 17;
		cPoseParameter[i]->setBounds( 520 + extraWidth, y, 96 + extraPoseParamWidth, 22 + 500 );	// mxChoice adds 500 internally for the dropdown in constructor, not in setBounds
		slPoseParameter[i]->setBounds( x + extraWidth, y, 140, 16 );
		lePoseParameter[i]->setBounds(  x + 146 + extraWidth, y, 40, 16 );
	}
}

//-----------------------------------------------------------------------------
// Sets up the window dealing with sequence control
//-----------------------------------------------------------------------------

void ControlPanel::SetupSequenceWindow( mxTab* pTab )
{
	mxWindow *wSequence = new mxWindow (this, 0, 0, 0, 0);
	tab->add (wSequence, "Sequence");

	for ( int i = 0; i < MAX_SEQUENCES; i++ )
	{
		cSequence[i] = new mxChoice (wSequence, 5, 5 + i * 22, 200, 22, IDC_SEQUENCE0+i);	
		mxToolTip::add (cSequence[i], "Select Sequence");
		slSequence[i] = new mxSlider (wSequence, 208, 5 + i * 22, 80, 18, IDC_SEQUENCESCALE0+i);
		slSequence[i]->setRange (0, 1.0, 100);
		slSequence[i]->setValue (0.0);

		rbFrameSelection[i] = new mxRadioButton (wSequence, 300, 5 + i * 22, 35, 22, "", IDC_FRAMESELECTION0+i, i == 0);

		leSequenceFilter[i] = new mxLineEdit( wSequence, 0, 0, 0, 0, "", IDC_SEQUENCEFILTER0+i);
	}

	laFilters = new mxLabel( wSequence, 0, 0, 0, 0, "Sequence Filters" );

	slSequence[0]->setVisible( false );

	laGroundSpeed = new mxLabel( wSequence, 208, 5, 80, 18, "" );

	for ( int i = 0; i < NUM_POSEPARAMETERS; i++ )
	{
		int x, y;
		x = 334;
		y = 2 + (i % 8) * 17;

		cPoseParameter[i] = new mxChoice (wSequence, 520, y, 96, 22, IDC_POSEPARAMETER+i);	
		cPoseParameter[i]->setVisible( false );

		slPoseParameter[i] = new mxSlider (wSequence, x, y, 140, 16, IDC_POSEPARAMETER_SCALE+i);
		slPoseParameter[i]->setRange (0.0, 1.0, 1000);
		mxToolTip::add (slPoseParameter[i], "Parameter");
		slPoseParameter[i]->setVisible( false );

		lePoseParameter[i] = new mxLineEdit ( wSequence, x + 146, y, 40, 16, "X", IDC_POSEPARAMETER_VALUE+i );
		lePoseParameter[i]->setVisible( false );
	}

	slSpeedScale = new mxSlider (wSequence, 5, 115, 200, 18, IDC_SPEEDSCALE);
	slSpeedScale->setRange (0, 1.0, 100);
	slSpeedScale->setValue (1.0);
	mxToolTip::add (slSpeedScale, "Speed Scale");
	laFPS = new mxLabel (wSequence, 208, 115, 128, 22, "" );

	new mxCheckBox (wSequence, 5, 142, 150, 20, "Blend Sequence Changes", IDC_BLENDSEQUENCECHANGES);
	new mxButton( wSequence, 155, 142, 80, 20, "Blend Now", IDC_BLENDNOW );
	laBlendAmount = new mxLabel( wSequence, 240, 142, 60, 20, "" );

	slBlendTime = new mxSlider( wSequence, 308, 142, 200, 18, IDC_BLENDTIME );
	slBlendTime->setRange( 0, 1.0, 100 );
	slBlendTime->setValue( DEFAULT_BLEND_TIME );
	laBlendTime = new mxLabel( wSequence, 540, 142, 80, 22, "" );

	new mxLabel (wSequence, 5, 170, 90, 18, "Activity modifiers:");
	cActivityModifiers = new mxChoice (wSequence, 105, 166, 350, 22, IDC_ACTIVITY_MODIFIERS);

	new mxCheckBox (wSequence, 460, 166, 350, 22, "Animate weapons", IDC_ANIMATEWEAPONS);
}

//-----------------------------------------------------------------------------
// Sets up the window dealing with body control
//-----------------------------------------------------------------------------

void ControlPanel::SetupBodyWindow( mxTab* pTab )
{
	mxWindow *wBody = new mxWindow (this, 0, 0, 0, 0);
	pTab->add (wBody, "Model");
	cBodypart = new mxChoice (wBody, 5, 5, 100, 22, IDC_BODYPART);
	mxToolTip::add (cBodypart, "Choose a bodypart");
	cSubmodel = new mxChoice (wBody, 110, 5, 100, 22, IDC_SUBMODEL);
	mxToolTip::add (cSubmodel, "Choose a submodel of current bodypart");

	cBodyGroupPreset = new mxChoice (wBody, 110, 55, 100, 22, IDC_BODYGROUPPRESET);
	mxToolTip::add (cBodyGroupPreset, "Choose a bodygroup preset");

	cController = new mxChoice (wBody, 5, 30, 100, 22, IDC_CONTROLLER);	
	mxToolTip::add (cController, "Choose a bone controller");
	slController = new mxSlider (wBody, 105, 32, 100, 18, IDC_CONTROLLERVALUE);
	slController->setRange (0, 255);
	mxToolTip::add (slController, "Change current bone controller value");
	lModelInfo1 = new mxLabel (wBody, 220, 5, 120, 80, "No Model.");
	lModelInfo2 = new mxLabel (wBody, 340, 5, 120, 130, "");
	cSkin = new mxChoice (wBody, 5, 55, 100, 22, IDC_SKINS);
	mxToolTip::add (cSkin, "Choose a skin family");
	new mxLabel (wBody, 5, 170, 90, 18, "Materials used:");
	cMaterials = new mxChoice (wBody, 105, 166, 350, 22, IDC_MATERIALS);	
	mxToolTip::add (cMaterials, "Select material for UV Chart view");

	lModelInfo3 = new mxLabel (wBody, 220, 100, 120, 22, "");
	lModelInfo4 = new mxLabel (wBody, 220, 118, 120, 22, "");
	setTransparent( false );

	cbAutoLOD = new mxCheckBox (wBody, 5, 80, 100, 20, "Auto LOD", IDC_AUTOLOD);
	cbAutoLOD->setEnabled( true );
	cLODChoice = new mxChoice( wBody, 5, 101, 100, 22, IDC_LODCHOICE);
	mxToolTip::add (cLODChoice, "Select model LOD to render");
	new mxLabel (wBody, 5, 126, 60, 18, "LOD Switch:");
	leLODSwitch = new mxLineEdit(wBody, 70, 126, 35, 22, "", IDC_LODSWITCH);
	new mxLabel (wBody, 5, 151, 60, 18, "LOD Metric:" ); 
	lLODMetric = new mxLabel( wBody, 70, 151, 35, 22, "" );

	new mxLabel( wBody, 505, 5, 100, 18, "VMTs Loaded:" );
	cMessageList = new mxListBox( wBody, 500, 25, 540, 160, IDC_MESSAGES );
	cMessageList->add ("None");
	cMessageList->select (1);
	mxToolTip::add (cMessageList, "Materials (VMT files) this model has loaded");

	m_bExploreToMaterial = new mxButton( wBody, 500, 172, 100, 20, "Explore to material", IDC_EXPLORE_TO_VMT );

	new mxLabel( wBody, 785, 5, 100, 18, "Shader:" );
	cShaderUsed = new mxListBox( wBody, 830, 3, 210, 28, IDC_SHADERS );
	cShaderUsed->add ("Select material to show shader");
	cShaderUsed->select (0);
	mxToolTip::add (cShaderUsed, "Shader Used");

	m_bRandomizeWeaponModuleSlots = new mxButton( wBody, 390, 140, 100, 20, "Roll bodygroups", IDC_ROLL_BODYGROUPS );
	mxToolTip::add (m_bRandomizeWeaponModuleSlots, "Randomize model bodygroup selections");

}

//-----------------------------------------------------------------------------
// Sets up the window dealing with flexes
//-----------------------------------------------------------------------------

#define FLEX_DROPDOWN_WIDTH		90
#define FLEX_SLIDER_WIDTH		100
#define FLEX_ROWS_OF_SLIDERS	8

void ControlPanel::SetupFlexWindow( mxTab* pTab )
{
	mxWindow *wFlex = new mxWindow (this, 0, 0, 0, 0);
	pTab->add (wFlex, "Flex");
	for (int i = 0; i < NUM_FLEX_SLIDERS; i++)
	{
		int w = (i / FLEX_ROWS_OF_SLIDERS) * (FLEX_SLIDER_WIDTH + FLEX_DROPDOWN_WIDTH + 4) + 5;
		int h = (i % FLEX_ROWS_OF_SLIDERS) * 20 + 5;

		cFlex[i] = new mxChoice (wFlex, w, h, FLEX_DROPDOWN_WIDTH, 22, IDC_FLEX + i);
		mxToolTip::add (cFlex[i], "Select Flex");
		slFlexScale[i] = new mxSlider (wFlex, w + FLEX_DROPDOWN_WIDTH, h, FLEX_SLIDER_WIDTH, 18, IDC_FLEXSCALE + i);
		slFlexScale[i]->setRange (0, 1.0);
		slFlexScale[i]->setValue (0);
		mxToolTip::add (slFlexScale[i], "Flex Scale");
	}

	{
		int h = FLEX_ROWS_OF_SLIDERS * 20 + 5;
		new mxButton( wFlex, 5, h + 1, FLEX_DROPDOWN_WIDTH - 2, 18, "Reset", IDC_FLEXDEFAULTS );

		int w = ( FLEX_DROPDOWN_WIDTH + 4 ) + 5;
		new mxButton( wFlex, w, h + 1, FLEX_DROPDOWN_WIDTH - 2, 18, "Random", IDC_FLEXRANDOM );

		w += ( FLEX_DROPDOWN_WIDTH + 4 ) + 5;
		new mxButton( wFlex, w, h + 1, FLEX_DROPDOWN_WIDTH - 2, 18, "Zero", IDC_FLEXZERO );

		w += ( FLEX_DROPDOWN_WIDTH + 4 ) + 5;
		new mxButton( wFlex, w, h + 1, FLEX_DROPDOWN_WIDTH - 2, 18, "One", IDC_FLEXONE );
	}
}

//-----------------------------------------------------------------------------
// Sets up the window dealing with physics
//-----------------------------------------------------------------------------

void ControlPanel::SetupPhysicsWindow( mxTab* pTab )
{
	// Physics Window
	mxWindow *wPhysics = new mxWindow (this, 0, 0, 0, 0);
	pTab->add (wPhysics, "Physics");

	new mxLabel (wPhysics, 5, 33, 30, 18, "Mass");
	leMass = new mxLineEdit(wPhysics, 35, 30, 70, 22, "", IDC_PHYS_MASS);

	cPhysicsBone = new mxChoice (wPhysics, 5, 5, 345, 22, IDC_PHYS_BONE);
	cPhysicsBone->add ("None");
	cPhysicsBone->select (0);

	new mxCheckBox (wPhysics, 5, 55, 100, 20, "Highlight", IDC_PHYSICSMODEL);
	int x = 5;
	int y = 80;

	rbConstraintAxis[0] = new mxRadioButton (wPhysics, x, y, 35, 22, "X", IDC_PHYS_CON_AXIS_X, true);
	rbConstraintAxis[1] = new mxRadioButton (wPhysics, x+35, y, 35, 22, "Y", IDC_PHYS_CON_AXIS_Y);
	rbConstraintAxis[2] = new mxRadioButton (wPhysics, x+70, y, 35, 22, "Z", IDC_PHYS_CON_AXIS_Z);
	setPhysicsAxis( 0 );
	y += 25;

	new mxLabel (wPhysics, x, y, 45, 18, "Friction");
	slPhysicsFriction = new mxSlider (wPhysics, x+50, y, 180, 18, IDC_PHYS_CON_FRICTION);
	slPhysicsFriction->setRange (0, 1000.0, 1000);
	slPhysicsFriction->setValue (0);
	slPhysicsFriction->setSteps( 1, 1 );
	lPhysicsFriction = new mxLabel (wPhysics, x+230, y, 30, 18, "0");

	x = 135;
	y = 30;

	new mxLabel (wPhysics, 115, y, 30, 18, "Min");
	slPhysicsConMin = new mxSlider (wPhysics, x, y, 180, 18, IDC_PHYS_CON_MIN);
	slPhysicsConMin->setRange (-180, 180.0, 360);
	slPhysicsConMin->setValue (-90);
	slPhysicsConMin->setSteps( 1, 1 );
	lPhysicsConMin = new mxLabel (wPhysics, x+180, y, 30, 18, "-90");
	y += 25;

	new mxLabel (wPhysics, 115, y, 30, 18, "Max");
	slPhysicsConMax = new mxSlider (wPhysics, x, y, 180, 18, IDC_PHYS_CON_MAX);
	slPhysicsConMax->setRange (-180.0, 180.0, 360);
	slPhysicsConMax->setValue (90.0);
	slPhysicsConMax->setSteps( 1, 1 );
	lPhysicsConMax = new mxLabel (wPhysics, x+180, y, 30, 18, "90");
	y += 25;

	new mxLabel (wPhysics, 115, y, 30, 18, "Test");
	slPhysicsConTest = new mxSlider (wPhysics, x, y, 55, 18, IDC_PHYS_CON_TEST);
	slPhysicsConTest->setRange (0, 1.0, 100);
	slPhysicsConTest->setValue (0);

	cbLinked = new mxCheckBox (wPhysics, 200, y, 50, 20, "Link", IDC_PHYS_CON_LINK_LIMITS);
	mxToolTip::add (cbLinked, "Link mins/maxs to be symmetric");
	new mxButton (wPhysics, 250, y, 80, 22, "Generate QC", IDC_PHYS_QCFILE);

	x += 220;
	y = 5;
	new mxLabel (wPhysics, x, y, 55, 18, "Mass Bias");
	new mxLabel (wPhysics, x, y+25, 55, 18, "Inertia");
	new mxLabel (wPhysics, x, y+50, 55, 18, "Damping");
	new mxLabel (wPhysics, x, y+75, 55, 18, "Rot Damp");
	new mxLabel (wPhysics, x, y+100+3, 55, 18, "Material");
	x += 55;

	slPhysicsParamMassBias = new mxSlider (wPhysics, x, y, 76, 18, IDC_PHYS_P_MASSBIAS );
	slPhysicsParamMassBias->setRange (0, 10.0, 100);
	slPhysicsParamMassBias->setValue (1.0);
	slPhysicsParamMassBias->setSteps( 1, 1 );
	lPhysicsParamMassBias = new mxLabel (wPhysics, x+80, y, 30, 18, "1.0");
	y += 25;

	slPhysicsParamInertia = new mxSlider (wPhysics, x, y, 76, 18, IDC_PHYS_P_INERTIA );
	slPhysicsParamInertia->setRange (0, 10.0, 100);
	slPhysicsParamInertia->setValue (1.0);
	slPhysicsParamInertia->setSteps( 1, 1 );
	lPhysicsParamInertia = new mxLabel (wPhysics, x+80, y, 30, 18, "1.0");
	y += 25;

	slPhysicsParamDamping = new mxSlider (wPhysics, x, y, 76, 18, IDC_PHYS_P_DAMPING );
	slPhysicsParamDamping->setRange (0, 1.0, 100);
	slPhysicsParamDamping->setValue (0.01);
	slPhysicsParamDamping->setSteps( 1, 1 );
	lPhysicsParamDamping = new mxLabel (wPhysics, x+80, y, 30, 18, "0.5");
	y += 25;

	slPhysicsParamRotDamping = new mxSlider (wPhysics, x, y, 76, 18, IDC_PHYS_P_ROT_DAMPING );
	slPhysicsParamRotDamping->setRange (0, 10.0, 200);
	slPhysicsParamRotDamping->setValue (0.2);
	slPhysicsParamRotDamping->setSteps( 1, 1 );
	lPhysicsParamRotDamping = new mxLabel (wPhysics, x+80, y, 30, 18, "0.2");
	y += 25;

	lPhysicsMaterial = new mxLabel( wPhysics, x, y+3, 110, 18, "default" );
}
void ControlPanel::SetupSoftbodyWindow( mxTab* pTab )
{
	mxWindow *wSoftbody = new mxWindow( this, 0, 0, 0, 0 );
	pTab->add( wSoftbody, "Cloth" );
	leSoftbodyIterations = new mxLabel( wSoftbody, 5, 33, 90, 18, "Iterations" );
	slSoftbodyIterations = new mxSlider( wSoftbody, 100, 33, 100, 18, IDC_SOFT_ITERATIONS );
	slSoftbodyIterations->setRange( 1, 101, 100 );
	slSoftbodyIterations->setValue( 1.0f );
	slSoftbodyIterations->setSteps( 1, 5 );
	new mxLabel( wSoftbody, 210, 33, 60, 18, "Wind Yaw" );
	slSoftbodyWindYaw = new mxSlider( wSoftbody, 285, 33, 105, 18, IDC_SOFT_WIND_YAW );
	slSoftbodyWindYaw->setRange( 0, 360, 360 );
	slSoftbodyWindYaw->setValue( 0 );
	slSoftbodyWindYaw->setSteps( 1, 5 );
	new mxLabel( wSoftbody, 405, 33, 80, 18, "Wind Strength" );
	slSoftbodyWindStrength = new mxSlider( wSoftbody, 480, 33, 100, 18, IDC_SOFT_WIND_STRENGTH );
	slSoftbodyWindStrength->setRange( 0, 1, 100 );
	slSoftbodyWindStrength->setValue( 0 );
	slSoftbodyWindStrength->setSteps( 1, 5 );
	cSoftbodyCtrl = new mxChoice( wSoftbody, 5, 5, 345, 22, IDC_SOFT_BONE );
	cSoftbodyCtrl->add( "None" );
	cSoftbodyCtrl->select( 0 );
	cbSoftbodySimulate = new mxCheckBox( wSoftbody, 5, 55, 80, 20, "Simulate", IDC_SOFT_SIMULATE );
	new mxLabel( wSoftbody, 100, 55, 70, 18, "2D Stretch" );
	slSoftbodySurfaceStretch = new mxSlider( wSoftbody, 170, 55, 100, 20, IDC_SOFT_SURFACE_STRETCH);
	slSoftbodySurfaceStretch->setRange( 0, 3, 300 );
	slSoftbodySurfaceStretch->setValue( 0 );
	slSoftbodySurfaceStretch->setSteps( 1, 10 );
	new mxLabel( wSoftbody, 270, 55, 70, 18, "1D Stretch" );
	slSoftbodyThreadStretch = new mxSlider( wSoftbody, 340, 55, 100, 20, IDC_SOFT_THREAD_STRETCH );
	slSoftbodyThreadStretch->setRange( 0, 3, 300 );
	slSoftbodyThreadStretch->setValue( 0 );
	slSoftbodyThreadStretch->setSteps( 1, 10 );
	cbSoftbodyPolygons = new mxCheckBox( wSoftbody, 5, 80, 80, 20, "Polygons", IDC_SOFT_SHOW_POLYGONS );
	cbSoftbodyEdges = new mxCheckBox( wSoftbody, 100, 80, 80, 20, "Edges", IDC_SOFT_SHOW_EDGES );
	cbSoftbodyBases = new mxCheckBox( wSoftbody, 200, 80, 80, 20, "Bases", IDC_SOFT_SHOW_BASES );
	cbSoftbodyWind = new mxCheckBox( wSoftbody, 300, 80, 80, 20, "Wind", IDC_SOFT_SHOW_WIND );
	cbSoftbodyIndices = new mxCheckBox( wSoftbody, 400, 80, 80, 20, "Indices", IDC_SOFT_SHOW_INDICES );
	int x = 5;
	int y = 105;
	rbSoftbodyAxis[ 0 ] = new mxRadioButton( wSoftbody, x, y, 35, 22, "X", IDC_SOFT_CON_AXIS_X, true );
	rbSoftbodyAxis[ 1 ] = new mxRadioButton( wSoftbody, x + 35, y, 35, 22, "Y", IDC_SOFT_CON_AXIS_Y );
	rbSoftbodyAxis[ 2 ] = new mxRadioButton( wSoftbody, x + 70, y, 35, 22, "Z", IDC_SOFT_CON_AXIS_Z );
	setSoftbodyAxis( 0 );
	y += 25;
}

//-----------------------------------------------------------------------------
// Sets up the window dealing with ik rules
//-----------------------------------------------------------------------------

void ControlPanel::SetupIKRuleWindow( mxTab *pTab )
{
	mxWindow *wIKRule = new mxWindow( this, 0, 0, 0, 0 );
	pTab->add( wIKRule, "IKRule" );

	new mxLabel( wIKRule, 5, 5, 80, 20, "Chain:" );
	cIKChain = new mxChoice( wIKRule, 90, 5, 50, 20, IDC_IKRULE_CHAIN );
	cIKChain->add( "lfoot" );
	cIKChain->add( "rfoot" );
	cIKChain->add( "lhand" );
	cIKChain->add( "rhand" );
	cIKChain->select( 0 );
	mxToolTip::add( cIKChain, "Select IK Chain" );

	new mxLabel( wIKRule, 145, 5, 80, 20, "Type:" );
	cIKType = new mxChoice( wIKRule, 230, 5, 80, 20, IDC_IKRULE_CHOICE );
	cIKType->add( "footstep" );
	cIKType->add( "touch" );
	cIKType->add( "release" );
	cIKType->add( "attachment" );
	cIKType->add( "unlatch" );
	cIKType->select( 0 );
	mxToolTip::add( cIKType, "Select IK Type" );

	lIKTouch = new mxLabel( wIKRule, 315, 5, 80, 20, "Bone:" );
	cIKTouch = new mxChoice( wIKRule, 400, 5, 200, 20, IDC_IKRULE_TOUCH );
	PopulateBoneList( cIKTouch );
	mxToolTip::add( cIKTouch, "Select Touched Bone" );

	lIKAttachment = new mxLabel( wIKRule, 315, 5, 80, 20, "Attachment:" );
	leIKAttachment = new mxLineEdit( wIKRule, 400, 5, 80, 20, "", IDC_IKRULE_ATTACHMENT );

	cbIKRangeToggle = new mxCheckBox( wIKRule, 5, 30, 80, 20, "Range", IDC_IKRULE_RANGE_TOGGLE );
	mxToolTip::add( cbIKRangeToggle, "Toggle range option" );
	new mxButton( wIKRule,  90, 30, 30, 20, "start", IDC_IKRULE_RANGE_START_NOW );
	leIKRangeStart = new mxLineEdit2( wIKRule, 120, 30, 35, 20, "..", IDC_IKRULE_RANGE_START );
	new mxButton( wIKRule, 160, 30, 30, 20, "peak", IDC_IKRULE_RANGE_PEAK_NOW );
	leIKRangePeak = new mxLineEdit2( wIKRule, 190, 30, 35, 20, "..", IDC_IKRULE_RANGE_PEAK );
	new mxButton( wIKRule, 230, 30, 30, 20, "tail", IDC_IKRULE_RANGE_TAIL_NOW );
	leIKRangeTail = new mxLineEdit2( wIKRule, 260, 30, 35, 20, "..", IDC_IKRULE_RANGE_TAIL );
	new mxButton( wIKRule, 300, 30, 30, 20, "end", IDC_IKRULE_RANGE_END_NOW );
	leIKRangeEnd = new mxLineEdit2( wIKRule, 330, 30, 35, 20, "..", IDC_IKRULE_RANGE_END );

	cbIKContactToggle = new mxCheckBox( wIKRule, 5, 55, 80, 20, "Contact", IDC_IKRULE_CONTACT_TOGGLE );
	mxToolTip::add( cbIKContactToggle, "Toggle contact option" );
	new mxButton( wIKRule,  90, 55, 30, 20, "frame", IDC_IKRULE_CONTACT_FRAME_NOW );
	leIKContactFrame = new mxLineEdit2( wIKRule, 120, 55, 35, 20, "", IDC_IKRULE_CONTACT_FRAME );

	new mxLabel( wIKRule, 5, 80, 80, 20, "Transform using:" );
	cIKUsing = new mxChoice( wIKRule, 90, 80, 80, 20, IDC_IKRULE_USING );
	cIKUsing->add( "neither" );
	cIKUsing->add( "source" );
	cIKUsing->add( "sequence" );
	cIKUsing->select( 0 );
	mxToolTip::add( cIKUsing, "Choose Transform To Use" );

	new mxLabel( wIKRule, 5, 105, 80, 20, "QC String:" );
	leIKQCString = new mxLineEdit2( wIKRule, 90, 105, 500, 20, "", IDC_IKRULE_QC_STRING );
	UpdateIKRuleWindow();
}

//-----------------------------------------------------------------------------
// Sets up the window dealing with events
//-----------------------------------------------------------------------------

void ControlPanel::SetupEventWindow( mxTab *pTab )
{
	mxWindow *wEvents = new mxWindow( this, 0, 0, 0, 0 );
	pTab->add( wEvents, "Events" );

	new mxLabel( wEvents, 5, 5, 80, 20, "Sound:" );
	mxButton *bSoundFrameNow = new mxButton( wEvents,  90, 5, 30, 20, "frame", IDC_EVENT_SOUND_FRAME_NOW );
	mxToolTip::add( bSoundFrameNow, "Set sound start at the current frame" );
	leEventSoundFrame = new mxLineEdit2( wEvents, 120, 5, 35, 20, "0", IDC_EVENT_SOUND_FRAME );
	lbEventSoundName = new mxListBox( wEvents, 160, 5, 300, 170, IDC_EVENT_SOUND_NAME );
	PopulateSoundNameList( lbEventSoundName );
	mxToolTip::add( lbEventSoundName, "Select Sound Name" );

	new mxLabel( wEvents, 5, 170, 80, 20, "QC String:" );
	leEventQCString = new mxLineEdit2( wEvents, 90, 170, 450, 20, "", IDC_EVENT_QC_STRING );
	BuildEventQCString();

	lEventSequence = new mxLabel( wEvents, 460, 5, 300, 20, "" );
	lbEventHistory = new mxListBox( wEvents, 460, 25, 300, 150 );
	m_lastEventCycle = 0.0f;
}

//-----------------------------------------------------------------------------
// Sets up the window dealing with bone control
//-----------------------------------------------------------------------------

void ControlPanel::SetupBoneControlWindow( mxTab* pTab )
{
	m_pBoneWindow = new CBoneControlWindow(this);
	pTab->add (m_pBoneWindow, "Bones");
	m_pBoneWindow->Init();
}


void ControlPanel::SetupAttachmentsWindow( mxTab *pTab )
{
	m_pAttachmentsWindow = new CAttachmentsWindow(this);
	pTab->add( m_pAttachmentsWindow, "Attachments" );
	m_pAttachmentsWindow->Init();
}


int ControlPanel::GetCurrentHitboxSet( void )
{
	return m_pBoneWindow ? m_pBoneWindow->GetHitboxSet() : 0;
}

ControlPanel::~ControlPanel()
{
	g_ControlPanel = NULL;
}

void ControlPanel::OnDelete()
{
	// for some reason, the destructor only gets called from mx when breakpoints are set,
	// so to be safe, clear the pointer (possibly twice)
	g_ControlPanel = NULL;
}

void ControlPanel::BuildEventQCString()
{
	if ( g_ControlPanel == NULL )
		return;

	char qcstr[ 256 ];
	Q_strcpy( qcstr, "{ event AE_CL_PLAYSOUND " );
	Q_strcat( qcstr, leEventSoundFrame->getLabel(), sizeof(qcstr) );
	Q_strcat( qcstr, " \"", sizeof(qcstr) );
	int i = lbEventSoundName->getSelectedIndex();
	Q_strcat( qcstr, lbEventSoundName->getItemText( i ), sizeof(qcstr) );
	Q_strcat( qcstr, "\" }", sizeof(qcstr) );

	leEventQCString->setText( qcstr );
}

void ControlPanel::BuildIKRuleQCString()
{
	if ( g_ControlPanel == NULL )
		return;

	char qcstr[ 256 ];
	Q_strcpy( qcstr, "ikrule " );
	Q_strcat( qcstr, cIKChain->getLabel(), sizeof(qcstr) );
	Q_strcat( qcstr, " ", sizeof(qcstr) );
	const char *pType = cIKType->getLabel();
	Q_strcat( qcstr, pType, sizeof(qcstr) );

	if ( Q_strcmp( pType, "touch" ) == 0 )
	{
		Q_strcat( qcstr, " \"", sizeof(qcstr) );
		if ( cIKTouch->getSelectedIndex() > 0 )
		{
			Q_strcat( qcstr, cIKTouch->getLabel(), sizeof(qcstr) );
		}
		Q_strcat( qcstr, "\"", sizeof(qcstr) );
	}
	else if ( Q_strcmp( pType, "attachment" ) == 0 )
	{
		Q_strcat( qcstr, " \"", sizeof(qcstr) );
		Q_strcat( qcstr, leIKAttachment->getLabel(), sizeof(qcstr) );
		Q_strcat( qcstr, "\"", sizeof(qcstr) );
	}

	if ( cbIKRangeToggle->isChecked() )
	{
		Q_strcat( qcstr, " range ", sizeof(qcstr) );

		char str[ 20 ];
		leIKRangeStart->getText( str, sizeof( str ) );
		Q_strcat( qcstr, str, sizeof(qcstr) );
		Q_strcat( qcstr, " ", sizeof(qcstr) );
		leIKRangePeak->getText( str, sizeof( str ) );
		Q_strcat( qcstr, str, sizeof(qcstr) );
		Q_strcat( qcstr, " ", sizeof(qcstr) );
		leIKRangeTail->getText( str, sizeof( str ) );
		Q_strcat( qcstr, str, sizeof(qcstr) );
		Q_strcat( qcstr, " ", sizeof(qcstr) );
		leIKRangeEnd->getText( str, sizeof( str ) );
		Q_strcat( qcstr, str, sizeof(qcstr) );
	}

	if ( cbIKContactToggle->isChecked() )
	{
		Q_strcat( qcstr, " contact ", sizeof(qcstr) );

		char str[ 20 ];
		leIKContactFrame->getText( str, sizeof( str ) );
		Q_strcat( qcstr, str, sizeof(qcstr) );
	}

	int nUsing = cIKUsing->getSelectedIndex();
	if ( nUsing > 0 )
	{
		if ( nUsing == 1 ) // source
		{
			Q_strcat( qcstr, " usesource", sizeof(qcstr) );
		}
		else if ( nUsing == 2 ) // sequence
		{
			Q_strcat( qcstr, " usesequence", sizeof(qcstr) );
		}
	}

	leIKQCString->setText( qcstr );
}

void ControlPanel::UpdateIKRuleWindow()
{
	const char *pIKType = cIKType->getLabel();
	bool bIsTouch = Q_strcmp( pIKType, "touch" ) == 0;
	bool bIsAttachment = Q_strcmp( pIKType, "attachment" ) == 0;

	lIKTouch->setVisible( bIsTouch );
	cIKTouch->setVisible( bIsTouch );
	lIKAttachment->setVisible( bIsAttachment );
	leIKAttachment->setVisible( bIsAttachment );

	BuildIKRuleQCString();
}

int
ControlPanel::handleEvent (mxEvent *event)
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	if ( !g_ControlPanel )
		return 1;

	if ( event->event == mxEvent::Size )
	{
		tab->setBounds( 0, 0, event->width, max( 0, event->height - 20 ) );
		updateSequenceSizes( event->width );
		return 1;
	}
	
	if ( event->event == mxEvent::KeyDown )
	{
		if ( tab->getSelectedIndex() == 4 )
		{
			handlePhysicsKey( event );
		}

		if (event->action >= IDC_POSEPARAMETER_VALUE && event->action < IDC_POSEPARAMETER_VALUE + NUM_POSEPARAMETERS)
		{
			int index = event->action - IDC_POSEPARAMETER_VALUE;
			int poseparam = cPoseParameter[index]->getSelectedIndex();

			float value =  atof( lePoseParameter[index]->getLabel() );
			setBlend( poseparam, value );
			slPoseParameter[index]->setValue( g_pStudioModel->GetPoseParameter( poseparam ) );
			return 1;
		}

		switch (event->key)
		{
			case 27:
				if (!getParent ()) // fullscreen mode ?
					mx::quit ();
				break;

			case '1':
			case '2':
			case '3':
			case '4':
				// don't do quick keys when in edit mode
				if ( tab->getSelectedIndex() < TAB_PHYSICS )
				{
					g_viewerSettings.renderMode = event->key - '1';
				}
				break;

			case '-':
				g_viewerSettings.speedScale -= 0.1f;
				if (g_viewerSettings.speedScale < 0.0f)
					g_viewerSettings.speedScale = 0.0f;
				break;

			case '+':
				g_viewerSettings.speedScale += 0.1f;
				if (g_viewerSettings.speedScale > 5.0f)
					g_viewerSettings.speedScale = 5.0f;
				break;
			default:
				return 0;
		}
		
		return 1;
	}

	switch (event->action)
	{
		case IDC_TAB:
		{
			int tabIndex = tab->getSelectedIndex();
			
			// g_viewerSettings.highlightBone = -1;
			g_viewerSettings.highlightHitbox = -1;
			g_viewerSettings.showTexture = (tabIndex == TAB_FLEX) ? true : false;
			g_viewerSettings.showPhysicsPreview = (tabIndex == TAB_PHYSICS) ? true : false;
			setHighlightBone(cHighlightBone->getSelectedIndex());

			if (tabIndex == TAB_PHYSICS)
			{
				setupPhysicsBone(cPhysicsBone->getSelectedIndex());
			}

			if (tabIndex == TAB_BONE)
			{
				m_pBoneWindow->OnTabSelected();
			}

			if ( tabIndex == TAB_ATTACHMENT )
			{
				m_pAttachmentsWindow->OnTabSelected();
			}
			else
			{
				m_pAttachmentsWindow->OnTabUnselected();
			}
		}
		break;

		case IDC_RENDERMODE:
		{
			int index = cRenderMode->getSelectedIndex();
			if (index >= 0)
			{
				setRenderMode (index);
			}
		}
		break;

		case IDC_PHYSICSHIGHLIGHT:
		{
			int index = cHighlightBone->getSelectedIndex();
			setHighlightBone(index);
		}
		break;

		case IDC_RENDER_FOV:
		{
			mxLineEdit *pLineEdit = ((mxLineEdit *) event->widget);
			const char *pText = pLineEdit->getLabel();
			float val = atof( pText );
			g_viewerSettings.fov = val;
			break;
		}

		case IDC_ORIGINAXISLENGTH:
		{
			g_viewerSettings.originAxisLength = reinterpret_cast< mxSlider * >( event->widget )->getValue();
			break;
		}

		case IDC_MATERIALVARMATS:
		{
			//when a material is selected, populate the material param list with all the parameters of this material
			cMaterialParamList->removeAll();
			
			//hide slider controls
			slMaterialParamMatrixSliderRotation->setVisible(false);
			slMaterialParamMatrixSliderScaleX->setVisible(false);
			slMaterialParamMatrixSliderScaleY->setVisible(false);
			slMaterialParamMatrixSliderTranslateX->setVisible(false);
			slMaterialParamMatrixSliderTranslateY->setVisible(false);
			lblMatrixRotation->setVisible(false);
			lblMatrixScaleX->setVisible(false);
			lblMatrixScaleY->setVisible(false);
			lblMatrixTranslateX->setVisible(false);
			lblMatrixTranslateY->setVisible(false);
			slMaterialParamFloat->setVisible(false);
			leMaterialParamText->setVisible(false);
			bMaterialParamColor->setVisible(false);
			bMaterialParamLoad->setVisible(false);
			bMaterialParamCopyToClipboard->setVisible(false);

			studiohdr_t* pStudioR = g_pStudioModel->GetStudioRenderHdr();
			if ( pStudioR )
			{
				IMaterial *pMaterials[128];
				g_pStudioRender->GetMaterialList( pStudioR, ARRAYSIZE( pMaterials ), &pMaterials[0] );

				IMaterial *pSelectedMaterial = pMaterials[ cMaterialList->getSelectedIndex() ];

				if ( g_OnlyEditMaterialsThatWantToBeEdited )
				{
					bool bLocalHideOthersInHLMV = false;
					pSelectedMaterial->FindVar("$hlmvallowedit", &bLocalHideOthersInHLMV, false);

					if ( !bLocalHideOthersInHLMV )
						break;
				}

				if ( !pSelectedMaterial->IsErrorMaterial() )
				{
					bMaterialParamLoad->setVisible( true );
					bMaterialParamCopyToClipboard->setVisible(true);

					int nShaderParams = pSelectedMaterial->ShaderParamCount();
					IMaterialVar **pMatVars = pSelectedMaterial->GetShaderParams();

					for (int n=0; n<nShaderParams; n++ )
					{
						IMaterialVar *pThisVar = pMatVars[n];
						if (pThisVar->IsDefined() )
						{
							cMaterialParamList->add( pThisVar->GetName() );
						}
					}
				}
			}
			break;
		}

		case IDC_MATERIALVARPARAMS:
		{
			// when a material parameter is selected, populate the lineedit control with the string value of that parameter
			leMaterialParamText->clear();

			studiohdr_t* pStudioR = g_pStudioModel->GetStudioRenderHdr();
			if ( pStudioR )
			{
				IMaterial *pMaterials[128];
				//int nMaterials = 
				g_pStudioRender->GetMaterialList( pStudioR, ARRAYSIZE( pMaterials ), &pMaterials[0] );

				bool bHideInHLMV = false;
				if ( g_OnlyEditMaterialsThatWantToBeEdited )
				{
					bool bLocalHideOthersInHLMV = false;
					pMaterials[cMaterialList->getSelectedIndex()]->FindVar("$hlmvallowedit", &bLocalHideOthersInHLMV, false);
					if ( !bLocalHideOthersInHLMV )
						bHideInHLMV = true;
				}

				leMaterialParamText->setVisible(!bHideInHLMV);
				bMaterialParamColor->setVisible(!bHideInHLMV);

#ifdef MATERIAL_SCRIPT_SAVE_FEATURE
				bMaterialParamSave->setVisible(!bHideInHLMV);
				leMaterialParamSavePath->setVisible(!bHideInHLMV);
				cbMaterialParamSaveRun->setVisible(!bHideInHLMV);
				leMaterialParamSaveRun->setVisible(!bHideInHLMV);
#endif
				bMaterialParamLoad->setVisible(!bHideInHLMV);

				if ( !pMaterials[ cMaterialList->getSelectedIndex() ]->IsErrorMaterial() )
				{
					bool bFoundParam = false;
					IMaterialVar *pThisVar = pMaterials[cMaterialList->getSelectedIndex()]->FindVar( cMaterialParamList->getItemText(cMaterialParamList->getSelectedIndex()), &bFoundParam, false );
					if (bFoundParam)
					{

						//hide type-specific controls
						bMaterialParamColor->setVisible(false);
						slMaterialParamMatrixSliderRotation->setVisible(false);
						slMaterialParamMatrixSliderScaleX->setVisible(false);
						slMaterialParamMatrixSliderScaleY->setVisible(false);
						slMaterialParamMatrixSliderTranslateX->setVisible(false);
						slMaterialParamMatrixSliderTranslateY->setVisible(false);
						lblMatrixRotation->setVisible(false);
						lblMatrixScaleX->setVisible(false);
						lblMatrixScaleY->setVisible(false);
						lblMatrixTranslateX->setVisible(false);
						lblMatrixTranslateY->setVisible(false);
						slMaterialParamFloat->setVisible(false);

						switch ( pThisVar->GetType() )
						{

							case MATERIAL_VAR_TYPE_FLOAT:
								{
									slMaterialParamFloat->setVisible(true);
									if ( pThisVar->GetFloatValue() > slMaterialParamFloat->getMaxValue() || pThisVar->GetFloatValue() < slMaterialParamFloat->getMinValue() )
									{
										slMaterialParamFloat->setRange( -pThisVar->GetFloatValue() * 2.0, pThisVar->GetFloatValue() * 2.0 );
									}
									else
									{
										slMaterialParamFloat->setRange( -1.0, 1.0 );
									}
									slMaterialParamFloat->setValue( pThisVar->GetFloatValue() );
									leMaterialParamText->setText( pThisVar->GetStringValue() );
								}
								break;

							case MATERIAL_VAR_TYPE_VECTOR:
								{
									bMaterialParamColor->setVisible(true);
									leMaterialParamText->setText( pThisVar->GetStringValue() );
								}
								break;

							case MATERIAL_VAR_TYPE_MATRIX:
								{

									slMaterialParamMatrixSliderRotation->setVisible(true);
									slMaterialParamMatrixSliderScaleX->setVisible(true);
									slMaterialParamMatrixSliderScaleY->setVisible(true);
									slMaterialParamMatrixSliderTranslateX->setVisible(true);
									slMaterialParamMatrixSliderTranslateY->setVisible(true);
									lblMatrixRotation->setVisible(true);
									lblMatrixScaleX->setVisible(true);
									lblMatrixScaleY->setVisible(true);
									lblMatrixTranslateX->setVisible(true);
									lblMatrixTranslateY->setVisible(true);

									VMatrix mat = pThisVar->GetMatrixValue();
									Vector tempScale = mat.GetScale();
									Vector tempTrans = mat.GetTranslation();
									QAngle tempAngle;
									MatrixToAngles( mat, tempAngle );

									slMaterialParamMatrixSliderScaleX->setValue( tempScale.x );
									slMaterialParamMatrixSliderScaleY->setValue( tempScale.y );
									slMaterialParamMatrixSliderTranslateX->setValue( tempTrans.x );
									slMaterialParamMatrixSliderTranslateY->setValue( tempTrans.y );
									slMaterialParamMatrixSliderRotation->setValue( tempAngle.y );

									char temp[255];
									V_snprintf( temp, sizeof(temp), " scale %f %f translate %f %f rotate %f", tempScale.x, tempScale.y, tempTrans.x, tempTrans.y, tempAngle.y );

									leMaterialParamText->setText( temp );
								}
								break;

							default:
								{
									leMaterialParamText->setText( pThisVar->GetStringValue() );
								}
								break;
						}

					}
				}
			}
			break;
		}

		case IDC_MATVAREDIT:
		{
			char str[ 255 ];
			leMaterialParamText->getText( str, sizeof( str ) );

			if ( V_strcmp( str, "" ) )
			{
				studiohdr_t* pStudioR = g_pStudioModel->GetStudioRenderHdr();
				if ( pStudioR )
				{
					IMaterial *pMaterials[128];
					g_pStudioRender->GetMaterialList( pStudioR, ARRAYSIZE( pMaterials ), &pMaterials[0] );

					if ( !pMaterials[ cMaterialList->getSelectedIndex() ]->IsErrorMaterial() )
					{
						bool bFoundParam = false;
						IMaterialVar *pThisVar = pMaterials[cMaterialList->getSelectedIndex()]->FindVar( cMaterialParamList->getItemText(cMaterialParamList->getSelectedIndex()), &bFoundParam, false );
						if (bFoundParam)
						{
							pThisVar->SetValueAutodetectType( str );
							pMaterials[cMaterialList->getSelectedIndex()]->RefreshPreservingMaterialVars();
						}

						//if affect all loaded materials is checked, loop through other materials and attempt the same change
						if ( cbMaterialParamMultiEdit->isChecked() )
						{
							for ( int i=0; i<cMaterialList->getItemCount(); i++ )
							{
								if ( i == cMaterialList->getSelectedIndex() )
									continue;

								if ( g_OnlyEditMaterialsThatWantToBeEdited )
								{
									bool bLocalHideOthersInHLMV = false;
									pMaterials[i]->FindVar("$hlmvallowedit", &bLocalHideOthersInHLMV, false);
									if ( !bLocalHideOthersInHLMV )
										continue;
								}

								IMaterialVar *pThisVar = pMaterials[i]->FindVar( cMaterialParamList->getItemText(cMaterialParamList->getSelectedIndex()), &bFoundParam, false );
								if (bFoundParam)
								{
									pThisVar->SetValueAutodetectType( str );
									pMaterials[i]->RefreshPreservingMaterialVars();
								}

							}
						}

					}

				}
			}

			break;
		}

		case IDC_MATVARCOLORPICKER:
		{
			
			char str[255];
			char str2[255];
			leMaterialParamText->getText(str,sizeof(str));

			V_StrSubst( str, "[ ", "", str2, sizeof(str2) );
			V_StrSubst( str2, " ]", "", str, sizeof(str) );

			CUtlVector< char * > vectorComponents;
			V_SplitString(str, " ", vectorComponents );

			int r = atoi(vectorComponents[0]);
			int g = atoi(vectorComponents[1]);
			int b = atoi(vectorComponents[2]);

			if (mxChooseColor (this, &r, &g, &b))
			{
				char result[255];
				V_snprintf(result, sizeof(result), "[ %i %i %i ]", r, g, b );
				leMaterialParamText->setText(result);
			}

			break;
		}


		case IDC_MATVARCOPYTOCLIPBOARD:
		{
			int nMatSelection = cMaterialList->getSelectedIndex();
			if ( nMatSelection < 0 || !strcmp( cMaterialList->getItemText(nMatSelection), "None" ) )
			{
				mxMessageBox (this, "No material selected.", g_appTitle, MX_MB_OK | MX_MB_ERROR);
				break;
			}
			studiohdr_t* pStudioR = g_pStudioModel->GetStudioRenderHdr();
			if ( !pStudioR )
			{
				mxMessageBox (this, "No loaded model.", g_appTitle, MX_MB_OK | MX_MB_ERROR);
				break;
			}

			IMaterial *pMaterials[128];
			g_pStudioRender->GetMaterialList(pStudioR, ARRAYSIZE(pMaterials), &pMaterials[0]);
			IMaterial *pSelectedMaterial = pMaterials[nMatSelection];

			if (pSelectedMaterial->IsErrorMaterial())
			{
				mxMessageBox(this, "Selected material is ErrorMaterial.", g_appTitle, MX_MB_OK | MX_MB_ERROR);
				break;
			}

			// write material property KVs to clipboard

			CTextBuffer out;
			out.WriteText( pSelectedMaterial->GetShaderName() );
			out.WriteText( "\r\n{\r\n" );
			IMaterialVar **pMatVars = pSelectedMaterial->GetShaderParams();
			for ( int n = 0; n < pSelectedMaterial->ShaderParamCount(); n++ )
			{
				IMaterialVar *pThisVar = pMatVars[n];
				if ( pThisVar->IsDefined() )
				{
					char tmp[512];
					sprintf( tmp, "\t\"%s\" \"%s\"\r\n", pThisVar->GetName(), pThisVar->GetStringValue() );
					out.WriteText( tmp );
				}
			}
			out.WriteText( "\r\n}\r\n" );
			
			if ( out.GetSize() )
			{
				char *pOutput = new char[out.GetSize()];
				memcpy( pOutput, out.GetData(), out.GetSize() );
				Sys_CopyStringToClipboard( pOutput );

				mxMessageBox (this, "Material properties copied to clipboard.", g_appTitle, MX_MB_OK | MX_MB_INFORMATION);
			}

			break;
		}


		case IDC_MATVARLOAD:
		{
			int nMatSelection = cMaterialList->getSelectedIndex();
			if ( nMatSelection < 0 || !strcmp( cMaterialList->getItemText(nMatSelection), "None" ) )
			{
				mxMessageBox (this, "Can't replace VMT parameters: No material selected.", g_appTitle, MX_MB_OK | MX_MB_ERROR);
				break;
			}
			studiohdr_t* pStudioR = g_pStudioModel->GetStudioRenderHdr();
			if ( !pStudioR )
			{
				mxMessageBox (this, "Can't replace VMT parameters: No loaded model.", g_appTitle, MX_MB_OK | MX_MB_ERROR);
				break;
			}

			IMaterial *pMaterials[128];
			g_pStudioRender->GetMaterialList(pStudioR, ARRAYSIZE(pMaterials), &pMaterials[0]);
			IMaterial *pSelectedMaterial = pMaterials[nMatSelection];

			if (pSelectedMaterial->IsErrorMaterial())
			{
				mxMessageBox(this, "Can't replace VMT parameters: Selected material is ErrorMaterial.", g_appTitle, MX_MB_OK | MX_MB_ERROR);
				break;
			}

			const char *pFilePath = mxGetOpenFileName (this, 0, "*.vmt");
			if (pFilePath)
			{
				KeyValues *kvLoadedFromFile = new KeyValues( pSelectedMaterial->GetShaderName() );

				if ( kvLoadedFromFile->LoadFromFile( g_pFullFileSystem, pFilePath ) )
				{
					cMaterialList->deselect(nMatSelection);
					cMaterialParamList->removeAll();
					
					//hide slider controls
					slMaterialParamMatrixSliderRotation->setVisible(false);
					slMaterialParamMatrixSliderScaleX->setVisible(false);
					slMaterialParamMatrixSliderScaleY->setVisible(false);
					slMaterialParamMatrixSliderTranslateX->setVisible(false);
					slMaterialParamMatrixSliderTranslateY->setVisible(false);
					lblMatrixRotation->setVisible(false);
					lblMatrixScaleX->setVisible(false);
					lblMatrixScaleY->setVisible(false);
					lblMatrixTranslateX->setVisible(false);
					lblMatrixTranslateY->setVisible(false);
					slMaterialParamFloat->setVisible(false);
					leMaterialParamText->setVisible(false);
					bMaterialParamColor->setVisible(false);
					bMaterialParamLoad->setVisible(false);
					
					KeyValues *kv = new KeyValues(pSelectedMaterial->GetShaderName());
					IMaterialVar **pMatVars = pSelectedMaterial->GetShaderParams();
					for (int n = 0; n < pSelectedMaterial->ShaderParamCount(); n++)
					{
						IMaterialVar *pThisVar = pMatVars[n];
						if (pThisVar->IsDefined())
							kv->SetString(pThisVar->GetName(), pThisVar->GetStringValue());
					}
					
					kv->MergeFrom( kvLoadedFromFile, KeyValues::MERGE_KV_UPDATE );
					
					pSelectedMaterial->SetShaderAndParams( kv );
					pSelectedMaterial->Refresh();

					if (cbMaterialParamMultiEdit->isChecked())
					{
						for (int i = 0; i < cMaterialList->getItemCount(); i++)
						{
							if (i == cMaterialList->getSelectedIndex())
								continue;

							if ( g_OnlyEditMaterialsThatWantToBeEdited )
							{
								bool bLocalHideOthersInHLMV = false;
								pMaterials[i]->FindVar("$hlmvallowedit", &bLocalHideOthersInHLMV, false);
								if ( !bLocalHideOthersInHLMV )
									continue;
							}

							pSelectedMaterial = pMaterials[i];

							KeyValues *kv = new KeyValues(pSelectedMaterial->GetShaderName());
							IMaterialVar **pMatVars = pSelectedMaterial->GetShaderParams();
							for (int n = 0; n < pSelectedMaterial->ShaderParamCount(); n++)
							{
								IMaterialVar *pThisVar = pMatVars[n];
								if (pThisVar->IsDefined())
									kv->SetString(pThisVar->GetName(), pThisVar->GetStringValue());
							}

							kv->MergeFrom(kvLoadedFromFile, KeyValues::MERGE_KV_UPDATE);

							pSelectedMaterial->SetShaderAndParams(kv);
							pSelectedMaterial->Refresh();

							if ( kv )
								delete kv;

						}
					}

					if ( kv )
						delete kv;

					if ( kvLoadedFromFile )
						delete kvLoadedFromFile;
					
				}
				else
				{
					mxMessageBox(this, "Failed to load vmt file.", g_appTitle, MX_MB_OK | MX_MB_ERROR);
					break;
				}

			}

		}

#ifdef MATERIAL_SCRIPT_SAVE_FEATURE
		case IDC_MATVARSAVE:
		{
			int nMatSelection = cMaterialList->getSelectedIndex();
			if ( nMatSelection < 0 || !strcmp( cMaterialList->getItemText(nMatSelection), "None" ) )
			{
				mxMessageBox (this, "Error saving VMT keyvalues: No material selected.", g_appTitle, MX_MB_OK | MX_MB_ERROR);
				break;
			}

			studiohdr_t* pStudioR = g_pStudioModel->GetStudioRenderHdr();
			if ( !pStudioR )
			{
				mxMessageBox (this, "Error saving VMT keyvalues: No loaded model.", g_appTitle, MX_MB_OK | MX_MB_ERROR);
				break;
			}

			IMaterial *pMaterials[128];
			g_pStudioRender->GetMaterialList( pStudioR, ARRAYSIZE( pMaterials ), &pMaterials[0] );
			IMaterial *pSelectedMaterial = pMaterials[nMatSelection];

			if ( pSelectedMaterial->IsErrorMaterial() )
			{
				mxMessageBox (this, "Error saving VMT keyvalues: Material is ErrorMaterial.", g_appTitle, MX_MB_OK | MX_MB_ERROR);
				break;
			}

			KeyValues *kv = new KeyValues( pSelectedMaterial->GetShaderName() );
			IMaterialVar **pMatVars = pSelectedMaterial->GetShaderParams();
			for (int n=0; n<pSelectedMaterial->ShaderParamCount(); n++ )
			{
				IMaterialVar *pThisVar = pMatVars[n];
				if (pThisVar->IsDefined() )
					kv->SetString( pThisVar->GetName(), pThisVar->GetStringValue() );
			}

			char szVmtPath[255];
			leMaterialParamSavePath->getText( szVmtPath, sizeof(szVmtPath) );
			strcat( szVmtPath, ".vmt" );

			if ( !strcmp( szVmtPath, ".vmt" ) )
			{
				mxMessageBox (this, "Error saving VMT keyvalues: no output filename.", g_appTitle, MX_MB_OK | MX_MB_ERROR);
				break;
			}

			if ( !kv->SaveToFile( g_pFullFileSystem, szVmtPath, "MOD" ) )
			{
				mxMessageBox (this, "Error saving VMT keyvalues.", g_appTitle, MX_MB_OK | MX_MB_ERROR);
				break;
			}

			if ( cbMaterialParamSaveRun->isChecked() )
			{
				char szExecuteStr[512];
				leMaterialParamSaveRun->getText( szExecuteStr, sizeof(szExecuteStr) );
				if ( szExecuteStr[0] != '\0' )
				{
					char fullpath[ 512 ];
					g_pFullFileSystem->RelativePathToFullPath( szExecuteStr, "GAME", fullpath, sizeof( fullpath ) );
					system( fullpath );
				}
			}

			break;
		}
#endif

		case IDC_MATVARSLIDERMATRIX:
		{
			char temp[255];
			V_snprintf( temp, sizeof(temp), " scale %f %f translate %f %f rotate %f", 
				slMaterialParamMatrixSliderScaleX->getValue(), slMaterialParamMatrixSliderScaleY->getValue(),
				slMaterialParamMatrixSliderTranslateX->getValue(), slMaterialParamMatrixSliderTranslateY->getValue(),
				slMaterialParamMatrixSliderRotation->getValue() );
			leMaterialParamText->setText(temp);

			break;
		}

		case IDC_MATVARSLIDERFLOAT:
			{
				char temp[255];
				V_snprintf( temp, sizeof(temp), "%f", slMaterialParamFloat->getValue() );
				leMaterialParamText->setText(temp);

				break;
			}

		case IDC_LODCHOICE:
		{
			int index = cLODChoice->getSelectedIndex();
			if( index >= 0 )
			{
				setLOD( index, false, false );
			}
			break;
		}

		case IDC_LODSWITCH:
		{
			mxLineEdit *pLineEdit = ((mxLineEdit *) event->widget);
			const char *pText = pLineEdit->getLabel();
			float val = atof( pText );
			g_pStudioModel->SetLODSwitchValue( g_viewerSettings.lod, val );
			break;
		}
		
		case IDC_BONEWEIGHTINDEX:
		{
			g_BoneWeightInspectVert = cbBoneWeightInspectIndex->getSelectedIndex();
			break;
		}

		case IDC_AUTOLOD:
			setAutoLOD (((mxCheckBox *) event->widget)->isChecked());
			break;
		
		case IDC_SOFTWARESKIN:
			setSoftwareSkin(((mxCheckBox *) event->widget)->isChecked());
			break;
		
		case IDC_OVERBRIGHT2:
			setOverbright(((mxCheckBox *) event->widget)->isChecked());
			break;

		case IDC_GROUND:
			setShowGround (((mxCheckBox *) event->widget)->isChecked());
			break;

		case IDC_MOVEMENT:
			setShowMovement (((mxCheckBox *) event->widget)->isChecked());
			break;

		case IDC_BACKGROUND:
			setShowBackground (((mxCheckBox *) event->widget)->isChecked());
			break;

		case IDC_HITBOXES:
			if ( g_pStudioModel->GetStudioHdr() == NULL )
				((mxCheckBox *) event->widget)->setChecked( false );

			g_viewerSettings.showHitBoxes = ((mxCheckBox *) event->widget)->isChecked();

			cDrawHitBoxSet->select( 0 );
			cDrawHitBoxNumber->select( 0 );

			g_viewerSettings.showHitBoxSet = -1;
			g_viewerSettings.showHitBoxNumber = -1;

			cDrawHitBoxSet->setEnabled( g_viewerSettings.showHitBoxes );
			cDrawHitBoxNumber->setEnabled( g_viewerSettings.showHitBoxes && g_viewerSettings.showHitBoxSet != -1 );
			break;

		case IDC_SEQUENCEBOXES:
			g_viewerSettings.showSequenceBoxes = ((mxCheckBox *) event->widget)->isChecked();
			break;

		case IDC_SHADOW:
			g_viewerSettings.showShadow = ((mxCheckBox *) event->widget)->isChecked();
			break;

		case IDC_ILLUMPOSITION:
			g_viewerSettings.showIllumPosition = ((mxCheckBox *) event->widget)->isChecked();
			break;

		case IDC_RUNIK:
			g_viewerSettings.enableIK = ((mxCheckBox *) event->widget)->isChecked();
			g_viewerSettings.enableTargetIK = ((mxCheckBox *) event->widget)->isChecked();
			break;

		case IDC_HEADTURN:
			g_pStudioModel->SetSolveHeadTurn( ((mxCheckBox *) event->widget)->isChecked() ? 1 : 0 );
			break;

		case IDC_PHYSICSMODEL:
			g_viewerSettings.showPhysicsModel = ((mxCheckBox *) event->widget)->isChecked();
			break;

		case IDC_BONES:
			g_viewerSettings.showBones = ((mxCheckBox *) event->widget)->isChecked();
			break;

		case IDC_PLAYSOUNDS:
			g_viewerSettings.playSounds = ((mxCheckBox *) event->widget)->isChecked();
			break;

		case IDC_NORMALMAP:
			g_viewerSettings.enableNormalMapping = ((mxCheckBox *) event->widget)->isChecked();
			break;

		case IDC_DISPLACEMENTMAP:
			g_viewerSettings.enableDisplacementMapping = ((mxCheckBox *) event->widget)->isChecked();
			break;

//		case IDC_PARALLAXMAP:
//			g_viewerSettings.enableParallaxMapping = ((mxCheckBox *) event->widget)->isChecked();
//			break;

		case IDC_SPECULAR:
			g_viewerSettings.enableSpecular = ((mxCheckBox *) event->widget)->isChecked();
			break;

		case IDC_NORMALS:
			g_viewerSettings.showNormals = ((mxCheckBox *) event->widget)->isChecked();
			break;

		case IDC_TANGENTFRAME:
			g_viewerSettings.showTangentFrame = ((mxCheckBox *) event->widget)->isChecked();
			break;

		case IDC_OVERLAY_WIREFRAME:
			g_viewerSettings.overlayWireframe = ((mxCheckBox *) event->widget)->isChecked();
			break;

		case IDC_ATTACHMENTS:
			g_viewerSettings.showAttachments = ((mxCheckBox *) event->widget)->isChecked();
			break;

		case IDC_SHOWORIGINAXIS:
			setShowOriginAxis (((mxCheckBox *) event->widget)->isChecked());
			break;
			
		case IDC_MESSAGES:
		{
			int index = cMessageList->getSelectedIndex();
			if (index >= 0)
			{
				studiohdr_t* pStudioR = g_pStudioModel->GetStudioRenderHdr();
				if ( pStudioR )
				{
					IMaterial *pMaterials[128];
					g_pStudioRender->GetMaterialList( pStudioR, ARRAYSIZE( pMaterials ), &pMaterials[0] );

					cShaderUsed->removeAll();
					if ( pMaterials[index]->IsErrorMaterial() )
					{
						cShaderUsed->add( "*** ERROR *** Can't load VMT");
					}
					else
					{
						cShaderUsed->add( pMaterials[index]->GetShaderName() );
					}
				}
			}
		}
		break;

		case IDC_INCLUDEDMODELS:
			// Do nothing
			break;

		case IDC_SEQUENCE0:
		case IDC_SEQUENCE1:
		case IDC_SEQUENCE2:
		case IDC_SEQUENCE3:
		case IDC_SEQUENCE4:
		{
			int i = event->action - IDC_SEQUENCE0;
			int index = ((mxChoice *) event->widget)->getSelectedIndex();
			if (index >= 0)
			{
				index = GetSequenceForSelection( i, index );
				if (i == 0)
				{
					setSequence (index);
					showActivityModifiers( index );
				}
				else
				{
					setOverlaySequence( i, index, slSequence[i]->getValue() );
				}
			}
		}
		break;

		case IDC_SEQUENCESCALE0:
		case IDC_SEQUENCESCALE1:
		case IDC_SEQUENCESCALE2:
		case IDC_SEQUENCESCALE3:
		case IDC_SEQUENCESCALE4:
		{
			int i = event->action - IDC_SEQUENCESCALE0;
			int index = cSequence[i]->getSelectedIndex();
			if (index >= 0)
			{
				index = GetSequenceForSelection( i, index );
				if (i == 0)
				{
					setSequence (index);
					showActivityModifiers( index );
				}
				else
				{
					setOverlaySequence( i, index, (float)((mxSlider *) event->widget)->getValue() );
				}
			}
			else
			{
				setOverlaySequence( i, 0, (float)((mxSlider *) event->widget)->getValue() );
			}
		}
		break;

		case IDC_SEQUENCEFILTER0:
		case IDC_SEQUENCEFILTER1:
		case IDC_SEQUENCEFILTER2:
		case IDC_SEQUENCEFILTER3:
		case IDC_SEQUENCEFILTER4:
			{
				int sequenceSlot = event->action - IDC_SEQUENCEFILTER0;
				SaveSelectedSequences();
				initSequenceChoices( sequenceSlot );
				RestoreSelectedSequences();
			}
			break;

		case IDC_FRAMESELECTION0:
		case IDC_FRAMESELECTION1:
		case IDC_FRAMESELECTION2:
		case IDC_FRAMESELECTION3:
		case IDC_FRAMESELECTION4:
		{
			updateFrameSelection();
		}
		break;

		case IDC_BLENDTIME:
			{
				char sz[ 256 ];
				sprintf( sz, "%.2f s", (float)((mxSlider *) event->widget)->getValue() );

				g_pStudioModel->SetBlendTime( ((mxSlider *) event->widget)->getValue() );
				laBlendTime->setLabel( sz );
			}
			break;
		case IDC_BLENDSEQUENCECHANGES:
			g_viewerSettings.blendSequenceChanges = ((mxCheckBox *) event->widget)->isChecked();
			break;
		case IDC_ANIMATEWEAPONS:
			g_viewerSettings.animateWeapons = ((mxCheckBox *) event->widget)->isChecked();
			break;
		case IDC_BLENDNOW:
			startBlending();
			break;
		case IDC_SPEEDSCALE:
		{
			setSpeedScale( ((mxSlider *) event->widget)->getValue() );
		}
		break;

		case IDC_FORCEFRAME:
		{
			setFrame( ((mxSlider *) event->widget)->getValue() );

			// stop the animation
			setSpeedScale( 0 );

			if ( g_bHlmvMaster && g_HlmvIpcClient.Connect() )
			{
				CUtlBuffer cmd;
				CUtlBuffer res;
				cmd.Printf( "%s %f", "hlmvForceFrame", reinterpret_cast< mxSlider * >( event->widget )->getValue() );
				g_HlmvIpcClient.ExecuteCommand( cmd, res );
				g_HlmvIpcClient.Disconnect();
			}

		}
		break;

		case IDC_BODYPART:
		{
			int index = cBodypart->getSelectedIndex();
			if (index >= 0)
			{
				//don't change bodygroup states just by selecting the group dropdown. The submodel dropdown does this
				//g_pStudioModel->SetBodygroup (cBodypart->getSelectedIndex(), index);
				setBodypart (index);

			}
		}
		break;

		case IDC_BODYGROUPPRESET:
		{
			int index = cBodyGroupPreset->getSelectedIndex();
			if (index >= 0)
			{
				CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
				if (hdr)
				{
					g_pStudioModel->SetBodygroupPreset( cBodyGroupPreset->getItemText(index) );
				}
			}
		}
		break;

		case IDC_ROLL_BODYGROUPS:
		{
			CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
			if (hdr)
			{
				mstudiobodyparts_t *pbodyparts = hdr->pBodypart(0);
				for ( int i=0; i<hdr->numbodyparts(); i++ )
				{
					int randSelect = RandomInt(0, pbodyparts[i].nummodels - 1);
					g_pStudioModel->SetBodygroup( i, randSelect );

					if (cBodypart->getSelectedIndex() == i)
						cSubmodel->select( randSelect );
				}
			}
		}
		break;

		case IDC_EXPLORE_TO_VMT:
		{
			int index = cMessageList->getSelectedIndex();
			if (index >= 0)
			{
				char szAbsPath[260];
				V_sprintf_safe( szAbsPath, "%s\\%s\\materials\\%s.vmt", getenv("VGAME"), getenv("VMOD"), cMessageList->getItemText( index ) );
				
				for (char *cp = szAbsPath; *cp; cp++)
				{
					if (*cp == '/')
						*cp = '\\';
				
					if (*cp == '\r' ||*cp == '\n')
						*cp = '\0';
				}

				ShellExecute( 0, 0, _T(szAbsPath), 0, 0, SW_SHOW );
			}
		}
		break;

		case IDC_SUBMODEL:
		{
			int index = cSubmodel->getSelectedIndex();
			if (index >= 0)
			{
				setSubmodel (index);

			}
		}
		break;

		case IDC_CONTROLLER:
		{
			int index = cController->getSelectedIndex();
			if (index >= 0)
				setBoneController (index);
		}
		break;

		case IDC_CONTROLLERVALUE:
		{
			int index = cController->getSelectedIndex();
			if (index >= 0)
			{
				float flValue = slController->getValue();
				CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
				mstudiobonecontroller_t *pbonecontroller = hdr->pBonecontroller(index);
				if (pbonecontroller->end < pbonecontroller->start)
				{
					flValue *= -1.0f;
				}
				setBoneControllerValue(index, flValue);
			}
		}
		break;

		case IDC_SKINS:
		{
			int index = cSkin->getSelectedIndex();
			if (index >= 0)
			{
				g_pStudioModel->SetSkin (index);
				g_viewerSettings.skin = index;
				d_MatSysWindow->redraw();
			}
		}
		break;

		case IDC_MATERIALS:
			{
				int index = cMaterials->getSelectedIndex();
				if (index >= 0)
				{
					g_viewerSettings.materialIndex = index;
				}
			}
			break;

		case IDC_IKRULE_CHAIN:
		case IDC_IKRULE_CHOICE:
		case IDC_IKRULE_RANGE_TOGGLE:
		case IDC_IKRULE_RANGE_START:
		case IDC_IKRULE_RANGE_PEAK:
		case IDC_IKRULE_RANGE_TAIL:
		case IDC_IKRULE_RANGE_END:
		case IDC_IKRULE_CONTACT_TOGGLE:
		case IDC_IKRULE_CONTACT_FRAME:
		case IDC_IKRULE_USING:
			UpdateIKRuleWindow();
			break;

		case IDC_IKRULE_TOUCH:
		case IDC_IKRULE_ATTACHMENT:
			BuildIKRuleQCString();
			break;

		case IDC_IKRULE_RANGE_START_NOW:
			SetFrameString( leIKRangeStart, getFrameSelection() );
			break;

		case IDC_IKRULE_RANGE_PEAK_NOW:
			SetFrameString( leIKRangePeak, getFrameSelection() );
			break;

		case IDC_IKRULE_RANGE_TAIL_NOW:
			SetFrameString( leIKRangeTail, getFrameSelection() );
			break;

		case IDC_IKRULE_RANGE_END_NOW:
			SetFrameString( leIKRangeEnd, getFrameSelection() );
			break;

		case IDC_IKRULE_CONTACT_FRAME_NOW:
			SetFrameString( leIKContactFrame, getFrameSelection() );
			break;

		case IDC_IKRULE_QC_STRING:
			// ignore edits to the qc text box
			break;

		case IDC_EVENT_SOUND_FRAME:
		case IDC_EVENT_SOUND_NAME:
			BuildEventQCString();
			break;

		case IDC_EVENT_SOUND_FRAME_NOW:
			SetFrameString( leEventSoundFrame, getFrameSelection() );
			break;

		case IDC_EVENT_QC_STRING:
			// ignore edits to the qc text box
			break;

		case IDC_SUBMODEL_UPDATE_BONESELECTION:
			{
				int iSelectedSubmodel = cSubmodelList->getSelectedIndex();
				if ( iSelectedSubmodel != -1 )
				{
					strcpy( g_MergeModelBonePairs[iSelectedSubmodel].szTargetBone, cSubmodelAttachTo->getLabel() );
					strcpy( g_MergeModelBonePairs[iSelectedSubmodel].szLocalBone, cSubmodelLocalAttachOrigin->getLabel() );
				}
			}
			break;

		case IDC_SUBMODEL_UPDATE_SELECTION:
			{
				UpdateSubmodelSelection();
			}
			break;

		case IDC_SUBMODEL_LOADMERGEDMODEL:
			{
				const char *ptr = mxGetOpenFileName (this, 0, "*.mdl");
				if (ptr)
				{
					// find the first free slot
					int iChosenSlot = 0;
					for ( int i = 0; i < HLMV_MAX_MERGED_MODELS; i++ )
					{
						if ( g_viewerSettings.mergeModelFile[i][0] == 0 )
						{
							iChosenSlot = i;
							break;
						}
					}
					strcpy( g_viewerSettings.mergeModelFile[iChosenSlot], ptr );
					g_MDLViewer->LoadModelFile( ptr, iChosenSlot );
				}
			}
			break;

		case IDC_SUBMODEL_LOADMERGEDMODEL_STEAM:
			{
				const char *pFilename = g_MDLViewer->SteamGetOpenFilename();
				if ( pFilename )
				{
					// find the first free slot
					int iChosenSlot = 0;
					for ( int i = 0; i < HLMV_MAX_MERGED_MODELS; i++ )
					{
						if ( g_viewerSettings.mergeModelFile[i][0] == 0 )
						{
							iChosenSlot = i;
							break;
						}
					}
					strcpy( g_viewerSettings.mergeModelFile[iChosenSlot], pFilename );
					g_MDLViewer->LoadModelFile( pFilename, iChosenSlot );
				}
			}
			break;


		case IDC_SUBMODEL_UNLOADMERGEDMODEL:
			{
				int i = cSubmodelList->getSelectedIndex();
				// FIXME: move to d_cpl
				if ( i != -1 && g_pStudioExtraModel[i])
				{
					strcpy( g_viewerSettings.mergeModelFile[i], "" );
					g_pStudioExtraModel[i]->FreeModel( false );
					delete g_pStudioExtraModel[i];
					g_pStudioExtraModel[i] = NULL;
				}

				//need to push the missing index out of the merged model list
				for ( int i = 0; i < HLMV_MAX_MERGED_MODELS - 1; i++ )
				{
					if ( g_pStudioExtraModel[i] == NULL && g_pStudioExtraModel[i+1] != NULL )
					{
						strcpy( g_viewerSettings.mergeModelFile[i], g_viewerSettings.mergeModelFile[i+1] );
						strcpy( g_viewerSettings.mergeModelFile[i+1], "" );
						g_pStudioExtraModel[i] = g_pStudioExtraModel[i+1];
						g_pStudioExtraModel[i+1] = NULL;

						
						strcpy( g_MergeModelBonePairs[i].szLocalBone, g_MergeModelBonePairs[i+1].szLocalBone );
						strcpy( g_MergeModelBonePairs[i+1].szLocalBone, "" );
						strcpy( g_MergeModelBonePairs[i].szTargetBone, g_MergeModelBonePairs[i+1].szTargetBone );
						strcpy( g_MergeModelBonePairs[i+1].szTargetBone, "" );
					}
				}

				UpdateSubmodelWindow();
			}
			break;


		case IDC_SUBMODEL_UNLOADALLMERGEDMODELS:
			{
				for (int i=0; i<HLMV_MAX_MERGED_MODELS; i++)
				{
					// FIXME: move to d_cpl
					if (g_pStudioExtraModel[i])
					{
						strcpy( g_viewerSettings.mergeModelFile[i], "" );
						g_pStudioExtraModel[i]->FreeModel( false );
						delete g_pStudioExtraModel[i];
						g_pStudioExtraModel[i] = NULL;
					}
				}
				UpdateSubmodelWindow();	
			}
			break;

		case IDC_COMPILE_UPDATE_QCPATHSELECTION:
			{
				if ( bCompileSelectedToggle )
				{
					CompileSelectedIndex();
				}
				else
				{
					UpdateQCPathPanel( false );
				}
			}
			break;

		case IDC_COMPILE_REMOVEFROMLIST:
			{
				int nSelection = cCompileRecentQCpaths->getSelectedIndex();
				if ( nSelection >= 0 && g_QCPathRecords.Count() > nSelection )
				{
					DeleteFile( TEXT(g_QCPathRecords[nSelection].szLogFilePath) );
					g_QCPathRecords.Remove( nSelection );
				}
				cCompileRecentQCpaths->select(-1);
				UpdateQCPathPanel();
			}
			break;

		case IDC_COMPILE_SELECTEDTOGGLE:
			{
				bCompileSelectedToggle = !bCompileSelectedToggle;
				bCompileQCWhenSelected->setLabel( bCompileSelectedToggle ? "One-click compile [ON]" : "One-click compile [OFF]" );
			}
			break;

		case IDC_COMPILE_OPENLOGFILE:
			{
				int nSelection = cCompileRecentQCpaths->getSelectedIndex();
				if ( nSelection >= 0 && g_QCPathRecords.Count() > nSelection )
				{
					if ( strlen(g_QCPathRecords[nSelection].szLogFilePath) > 0 )
					{
						ShellExecute(0, 0, g_QCPathRecords[nSelection].szLogFilePath, 0, 0 , SW_SHOW );
					}
				}
			}
			break;

		case IDC_COMPILE_EXPLORETOQC:
			{
				int nSelection = cCompileRecentQCpaths->getSelectedIndex();
				if ( nSelection >= 0 && g_QCPathRecords.Count() > nSelection )
				{
					char cmd[1024];
					V_sprintf_safe( cmd, "/select,\"%s\"", g_QCPathRecords[nSelection].szAbsPath );
					ShellExecute(0, _T("open"), _T("explorer.exe"), cmd, 0, SW_NORMAL);
				}
			}
			break;

		case IDC_COMPILE_LOADMODELFILE:
			{
				int nSelection = cCompileRecentQCpaths->getSelectedIndex();
				if ( nSelection >= 0 && g_QCPathRecords.Count() > nSelection )
				{
					if ( strlen(g_QCPathRecords[nSelection].szModelPath) > 0 )
					{
						char szAbsPath[260];
						V_sprintf_safe( szAbsPath, "%s\\%s", getenv("VGAME"), g_QCPathRecords[nSelection].szModelPath );

						for (char *cp = szAbsPath; *cp; cp++)
						{
							if (*cp == '/')
								*cp = '\\';

							if (*cp == '\r' ||*cp == '\n')
								*cp = '\0';
						}

						g_MDLViewer->LoadModelFile( szAbsPath );
					}
				}
			}
			break;

		case IDC_COMPILE_CALLSTUDIOMDL:
			{
				CompileSelectedIndex();
			}
			break;

		default:
		{
			if ( event->action == IDC_FLEXDEFAULTS )
			{
				CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
				for (LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++)
				{
					g_pStudioModel->SetFlexController( i, 0 );
				}
				for (int i = 0; i < NUM_FLEX_SLIDERS; i++)
				{
					LocalFlexController_t index = (LocalFlexController_t)cFlex[i]->getSelectedIndex();
					slFlexScale[ i ]->setValue( g_pStudioModel->GetFlexControllerRaw( index ) );
				}
			}
			else if ( event->action == IDC_FLEXRANDOM )
			{
				CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
				for (LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++)
				{
					float r = rand() / float( VALVE_RAND_MAX );
					g_pStudioModel->SetFlexController( i, r );
				}

				for (int i = 0; i < NUM_FLEX_SLIDERS; i++)
				{
					LocalFlexController_t index = (LocalFlexController_t)cFlex[i]->getSelectedIndex();
					slFlexScale[ i ]->setValue( g_pStudioModel->GetFlexControllerRaw( index ) );
				}
			}
			else if ( event->action == IDC_FLEXZERO )
			{
				CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
				for (LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++)
				{
					g_pStudioModel->SetFlexController( i, 0 );
				}

				for (int i = 0; i < NUM_FLEX_SLIDERS; i++)
				{
					LocalFlexController_t index = (LocalFlexController_t)cFlex[i]->getSelectedIndex();
					slFlexScale[ i ]->setValue( g_pStudioModel->GetFlexControllerRaw( index ) );
				}
			}
			else if ( event->action == IDC_FLEXONE )
			{
				CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
				for (LocalFlexController_t i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++)
				{
					g_pStudioModel->SetFlexController( i, 1 );
				}

				for (int i = 0; i < NUM_FLEX_SLIDERS; i++)
				{
					LocalFlexController_t index = (LocalFlexController_t)cFlex[i]->getSelectedIndex();
					slFlexScale[ i ]->setValue( g_pStudioModel->GetFlexControllerRaw( index ) );
				}
			}
			else if (event->action >= IDC_FLEX && event->action < IDC_FLEX + NUM_FLEX_SLIDERS)
			{
				int flex = (event->action - IDC_FLEX);
				LocalFlexController_t index = (LocalFlexController_t)cFlex[flex]->getSelectedIndex();

				if (index >= 0)
				{
					slFlexScale[flex]->setValue(g_pStudioModel->GetFlexControllerRaw(index));
				}
			}
			else if (event->action >= IDC_FLEXSCALE && event->action < IDC_FLEXSCALE + NUM_FLEX_SLIDERS)
			{
				int flex = (event->action - IDC_FLEXSCALE);
				LocalFlexController_t index = (LocalFlexController_t)cFlex[flex]->getSelectedIndex();

				g_pStudioModel->SetFlexControllerRaw( index, ((mxSlider *) event->widget)->getValue() );
			}
			else if ( event->action >= IDC_PHYS_FIRST && event->action <= IDC_PHYS_LAST )
			{
				return handlePhysicsEvent( event );
			}
			else if ( event->action >= IDC_SOFT_FIRST && event->action <= IDC_SOFT_LAST )
			{
				return handleSoftbodyEvent( event );
			}
			else if (event->action >= IDC_POSEPARAMETER && event->action < IDC_POSEPARAMETER + NUM_POSEPARAMETERS)
			{
				int index = event->action - IDC_POSEPARAMETER;
				int poseparam = cPoseParameter[index]->getSelectedIndex();

				float flMin, flMax;
				if (g_pStudioModel->GetPoseParameterRange( poseparam, &flMin, &flMax ))
				{
					slPoseParameter[index]->setRange( flMin, flMax, 1000 );
					slPoseParameter[index]->setValue( g_pStudioModel->GetPoseParameter( poseparam ) );
					lePoseParameter[index]->setLabel( "%.1f", g_pStudioModel->GetPoseParameter( poseparam ) );
				}
			}
			else if (event->action >= IDC_POSEPARAMETER_SCALE && event->action < IDC_POSEPARAMETER_SCALE + NUM_POSEPARAMETERS)
			{
				int index = event->action - IDC_POSEPARAMETER_SCALE;
				int poseparam = cPoseParameter[index]->getSelectedIndex();

				setBlend( poseparam, ((mxSlider *) event->widget)->getValue() );
				lePoseParameter[index]->setLabel( "%.1f", ((mxSlider *) event->widget)->getValue() );
			}
		}
		break;
	}

	return 1;
}



void
ControlPanel::dumpModelInfo()
{
#if 0
	CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
	if (hdr)
	{
		DeleteFile ("midump.txt");
		FILE *file = fopen ("midump.txt", "wt");
		if (file)
		{
			byte *phdr = (byte *) hdr;
			int i;

			fprintf (file, "id: %c%c%c%c\n", phdr[0], phdr[1], phdr[2], phdr[3]);
			fprintf (file, "version: %d\n", hdr->version);
			fprintf (file, "name: \"%s\"\n", hdr->name);
			fprintf (file, "length: %d\n\n", hdr->length);

			fprintf (file, "eyeposition: %f %f %f\n", hdr->eyeposition[0], hdr->eyeposition[1], hdr->eyeposition[2]);
			fprintf (file, "min: %f %f %f\n", hdr->min[0], hdr->min[1], hdr->min[2]);
			fprintf (file, "max: %f %f %f\n", hdr->max[0], hdr->max[1], hdr->max[2]);
			fprintf (file, "bbmin: %f %f %f\n", hdr->bbmin[0], hdr->bbmin[1], hdr->bbmin[2]);
			fprintf (file, "bbmax: %f %f %f\n", hdr->bbmax[0], hdr->bbmax[1], hdr->bbmax[2]);
			
			fprintf (file, "flags: %d\n\n", hdr->flags);

			fprintf (file, "numbones: %d\n", hdr->numbones);
			for (i = 0; i < hdr->numbones; i++)
			{
				const mstudiobone_t *pbones = (mstudiobone_t *) (phdr + hdr->boneindex);
				fprintf (file, "\nbone %d.name: \"%s\"\n", i + 1, pbones[i].name);
				fprintf (file, "bone %d.parent: %d\n", i + 1, pbones[i].parent);
				fprintf (file, "bone %d.flags: %d\n", i + 1, pbones[i].flags);
				fprintf (file, "bone %d.bonecontroller: %d %d %d %d %d %d\n", i + 1, pbones[i].bonecontroller[0], pbones[i].bonecontroller[1], pbones[i].bonecontroller[2], pbones[i].bonecontroller[3], pbones[i].bonecontroller[4], pbones[i].bonecontroller[5]);
				fprintf (file, "bone %d.value: %f %f %f %f %f %f\n", i + 1, pbones[i].value[0], pbones[i].value[1], pbones[i].value[2], pbones[i].value[3], pbones[i].value[4], pbones[i].value[5]);
				fprintf (file, "bone %d.scale: %f %f %f %f %f %f\n", i + 1, pbones[i].scale[0], pbones[i].scale[1], pbones[i].scale[2], pbones[i].scale[3], pbones[i].scale[4], pbones[i].scale[5]);
			}

			fprintf (file, "\nnumbonecontrollers: %d\n", hdr->numbonecontrollers);
			for (i = 0; i < hdr->numbonecontrollers; i++)
			{
				mstudiobonecontroller_t *pbonecontrollers = (mstudiobonecontroller_t *) (phdr + hdr->bonecontrollerindex);
				fprintf (file, "\nbonecontroller %d.bone: %d\n", i + 1, pbonecontrollers[i].bone);
				fprintf (file, "bonecontroller %d.type: %d\n", i + 1, pbonecontrollers[i].type);
				fprintf (file, "bonecontroller %d.start: %f\n", i + 1, pbonecontrollers[i].start);
				fprintf (file, "bonecontroller %d.end: %f\n", i + 1, pbonecontrollers[i].end);
				fprintf (file, "bonecontroller %d.rest: %d\n", i + 1, pbonecontrollers[i].rest);
				fprintf (file, "bonecontroller %d.index: %d\n", i + 1, pbonecontrollers[i].index);
			}

			fprintf (file, "\nnumhitboxes: %d\n", hdr->numhitboxes);
			for (i = 0; i < hdr->numhitboxes; i++)
			{
				mstudiobbox_t *pbboxes = (mstudiobbox_t *) (phdr + hdr->hitboxindex);
				fprintf (file, "\nhitbox %d.bone: %d\n", i + 1, pbboxes[i].bone);
				fprintf (file, "hitbox %d.group: %d\n", i + 1, pbboxes[i].group);
				fprintf (file, "hitbox %d.bbmin: %f %f %f\n", i + 1, pbboxes[i].bbmin[0], pbboxes[i].bbmin[1], pbboxes[i].bbmin[2]);
				fprintf (file, "hitbox %d.bbmax: %f %f %f\n", i + 1, pbboxes[i].bbmax[0], pbboxes[i].bbmax[1], pbboxes[i].bbmax[2]);
			}

			fprintf (file, "\nnumseq: %d\n", hdr->GetNumSeq());
			for (i = 0; i < hdr->GetNumSeq(); i++)
			{
				mstudioseqdesc_t *pseqdescs = (mstudioseqdesc_t *) (phdr + hdr->seqindex);
				fprintf (file, "\nseqdesc %d.label: \"%s\"\n", i + 1, pseqdescs[i].label);
				fprintf (file, "seqdesc %d.fps: %f\n", i + 1, pseqdescs[i].fps);
				fprintf (file, "seqdesc %d.flags: %d\n", i + 1, pseqdescs[i].flags);
				fprintf (file, "<...>\n");
			}
/*
			fprintf (file, "\nnumseqgroups: %d\n", hdr->GetNumSeq()groups);
			for (i = 0; i < hdr->GetNumSeq()groups; i++)
			{
				mstudioseqgroup_t *pseqgroups = (mstudioseqgroup_t *) (phdr + hdr->seqgroupindex);
				fprintf (file, "\nseqgroup %d.label: \"%s\"\n", i + 1, pseqgroups[i].label);
				fprintf (file, "\nseqgroup %d.namel: \"%s\"\n", i + 1, pseqgroups[i].name);
				fprintf (file, "\nseqgroup %d.data: %d\n", i + 1, pseqgroups[i].data);
			}
*/
			hdr = g_pStudioModel->getTextureHeader();
			fprintf (file, "\nnumtextures: %d\n", hdr->numtextures);
			fprintf (file, "textureindex: %d\n", hdr->textureindex);
			fprintf (file, "texturedataindex: %d\n", hdr->texturedataindex);
			for (i = 0; i < hdr->numtextures; i++)
			{
				mstudiotexture_t *ptextures = (mstudiotexture_t *) ((byte *) hdr + hdr->textureindex);
				fprintf (file, "\ntexture %d.name: \"%s\"\n", i + 1, ptextures[i].name);
				fprintf (file, "texture %d.flags: %d\n", i + 1, ptextures[i].flags);
				fprintf (file, "texture %d.width: %d\n", i + 1, ptextures[i].width);
				fprintf (file, "texture %d.height: %d\n", i + 1, ptextures[i].height);
				fprintf (file, "texture %d.index: %d\n", i + 1, ptextures[i].index);
			}

			hdr = g_pStudioModel->GetStudioHdr();
			fprintf (file, "\nnumskinref: %d\n", hdr->numskinref);
			fprintf (file, "numskinfamilies: %d\n", hdr->numskinfamilies);

			fprintf (file, "\nnumbodyparts: %d\n", hdr->numbodyparts);
			for (i = 0; i < hdr->numbodyparts; i++)
			{
				mstudiobodyparts_t *pbodyparts = (mstudiobodyparts_t *) ((byte *) hdr + hdr->bodypartindex);
				fprintf (file, "\nbodypart %d.name: \"%s\"\n", i + 1, pbodyparts[i].name);
				fprintf (file, "bodypart %d.nummodels: %d\n", i + 1, pbodyparts[i].nummodels);
				fprintf (file, "bodypart %d.base: %d\n", i + 1, pbodyparts[i].base);
				fprintf (file, "bodypart %d.modelindex: %d\n", i + 1, pbodyparts[i].modelindex);
			}

			fprintf (file, "\nnumattachments: %d\n", hdr->numattachments);
			for (i = 0; i < hdr->numattachments; i++)
			{
				mstudioattachment_t *pattachments = (mstudioattachment_t *) ((byte *) hdr + hdr->attachmentindex);
				fprintf (file, "attachment %d.name: \"%s\"\n", i + 1, pattachments[i].name);
			}

			fclose (file);

			ShellExecute ((HWND) getHandle(), "open", "midump.txt", 0, 0, SW_SHOW);
		}
	}
#endif
}


LoadModelResult_t ControlPanel::loadModel(const char *filename)
{
	SaveViewerSettings( g_pStudioModel->GetFileName(), g_pStudioModel );

	g_pStudioModel->FreeModel( false );

	if (!g_pStudioModel->LoadModel( filename ))
	{
		return LoadModel_LoadFail;
	}
	
	if (!g_pStudioModel->PostLoadModel( filename ))
	{
		return LoadModel_PostLoadFail;
	}

	if (!g_pStudioModel->HasModel())
	{
		return LoadModel_NoModel;
	}

	OnLoadModel( );

	return LoadModel_Success;
}

void ControlPanel::OnLoadModel( void )
{
	int i;
	m_bVMTInfoLoaded = false;

	if (!g_pStudioModel->HasModel())
		return;

	initSequenceChoices();
	initBodypartChoices();
	initBoneControllers();
	initSkinChoices();
	initMaterialChoices();
	initPhysicsBones();
	initIncludedModels();
	initLODs();
	initFlexes();

	setModelInfo();

	const bool bNoModelSettings = LoadViewerSettings( g_pStudioModel->GetFileName(), g_pStudioModel );
	if ( !bNoModelSettings )
	{
		InitViewerSettings( "hlmv" );
		setSequence( 0 );
		showActivityModifiers( 0 );
		setSpeedScale( 1.0 );
	}

	resetControlPanel();

	/*!!
	for (i = 0; i < 32; i++)
		g_viewerSettings.submodels[i] = 0;
	for (i = 0; i < 8; i++)
		g_viewerSettings.controllers[i] = 0;
	*/

	setupPhysics();
	m_pBoneWindow->OnLoadModel();
	m_pAttachmentsWindow->OnLoadModel();

	PopulateBoneList( cIKTouch, true );

	mx_setcwd (mx_getpath (g_pStudioModel->GetFileName()));

	for (i = 0; i < HLMV_MAX_MERGED_MODELS; i++)
	{
		if (g_pStudioExtraModel[i])
		{
			g_pStudioExtraModel[i]->FreeModel( false );
			delete g_pStudioExtraModel[i];
			g_pStudioExtraModel[i] = NULL;
		}
		if (strlen( g_viewerSettings.mergeModelFile[i] ) != 0)
		{
			loadModel( g_viewerSettings.mergeModelFile[i], i );
		}
	}

	g_pWidgetControl = new WidgetControl();

	// Center the model if we don't have last view position data in the registry
	if ( !bNoModelSettings )
	{
		// Need to call this twice for some reason. Really - I don't have OCD!
		centerView();
		centerView();
	}

	// guess the category and set a reasonable default fov
	if ( V_stristr( g_pStudioModel->GetFileName(), "\\player\\" ) )
	{
		setFOV( 90 );
	}
	else if ( V_stristr( g_pStudioModel->GetFileName(), "weapons\\v_" ) )
	{
		setFOV( 54 );
	}
	else if ( V_stristr( g_pStudioModel->GetFileName(), "weapons\\w_" ) )
	{
		setFOV( 90 );
	}

}


LoadModelResult_t ControlPanel::loadModel(const char *filename, int slot )
{
	if (slot == -1)
	{
		return loadModel( filename );
	}

	if (g_pStudioExtraModel[slot] == NULL)
	{
		g_pStudioExtraModel[slot] = new StudioModel;
	}
	else
	{
		g_pStudioExtraModel[slot]->FreeModel( false );
	}

	if (g_pStudioExtraModel[slot]->LoadModel( filename ))
	{
		if (g_pStudioExtraModel[slot]->PostLoadModel( filename ))
		{
			MapExtraFlexes( slot );

			UpdateSubmodelWindow();
			return LoadModel_Success;
		}
		else
		{
			return LoadModel_PostLoadFail;
		}
	}

	return LoadModel_LoadFail;
}


void
ControlPanel::resetControlPanel( void )
{
	setSequence( g_pStudioModel->GetSequence() );
	showActivityModifiers( g_pStudioModel->GetSequence() );
	setOverlaySequence( 1, g_pStudioModel->GetOverlaySequence( 0 ), g_pStudioModel->GetOverlaySequenceWeight( 0 ) );
	setOverlaySequence( 2, g_pStudioModel->GetOverlaySequence( 1 ), g_pStudioModel->GetOverlaySequenceWeight( 1 ) );
	setOverlaySequence( 3, g_pStudioModel->GetOverlaySequence( 2 ), g_pStudioModel->GetOverlaySequenceWeight( 2 ) );
	setOverlaySequence( 4, g_pStudioModel->GetOverlaySequence( 3 ), g_pStudioModel->GetOverlaySequenceWeight( 3 ) );

	setSpeedScale( g_viewerSettings.speedScale );

	cbGround->setChecked(  g_viewerSettings.showGround );
	cbMovement->setChecked( g_viewerSettings.showMovement );
	cbShadow->setChecked( g_viewerSettings.showShadow );
	cbNormalMap->setChecked( g_viewerSettings.enableNormalMapping );
	cbDisplacementMap->setChecked( g_viewerSettings.enableDisplacementMapping );
	cbIllumPosition->setChecked( g_viewerSettings.showIllumPosition );

	cbHitBoxes->setChecked( g_viewerSettings.showHitBoxes );
	cbBones->setChecked( g_viewerSettings.showBones );
	cbPlaySounds->setChecked( g_viewerSettings.playSounds );
	cbShowOriginAxis->setChecked( g_viewerSettings.showOriginAxis );
	cbSequenceBoxes->setChecked( g_viewerSettings.showSequenceBoxes );
	cbRunIK->setChecked( g_viewerSettings.enableIK );

	cbBackground->setChecked( g_viewerSettings.showBackground );
	cbSoftwareSkin->setChecked( g_viewerSettings.softwareSkin );
	cbOverbright2->setChecked( g_viewerSettings.overbright );
	cbAttachments->setChecked( g_viewerSettings.showAttachments );
	cbNormals->setChecked( g_viewerSettings.showNormals );
	cbEnableHead->setChecked( g_pStudioModel->GetSolveHeadTurn() ? 1 : 0 );

	cbShowOriginAxis->setChecked( g_viewerSettings.showOriginAxis );
	setOriginAxisLength( g_viewerSettings.originAxisLength );
}


void
ControlPanel::setRenderMode (int mode)
{
	g_viewerSettings.renderMode = mode;
	d_MatSysWindow->redraw();
}


void 
ControlPanel::setHighlightBone( int index )
{
	if ( index >= 0 )
	{
		g_viewerSettings.highlightPhysicsBone = index;
	}
}

void
ControlPanel::setLOD( int index, bool setLODchoice, bool force )
{
#if 1
	if( !force && ( g_viewerSettings.lod == index ) )
	{
		return;
	}
#endif
	g_viewerSettings.lod = index;
	if ( !g_pStudioModel->HasMesh() )
		return;

	float lodSwitch = g_pStudioModel->GetLODSwitchValue( index );
	char tmp[128];
	sprintf( tmp, "%0.0f", lodSwitch );
	leLODSwitch->setLabel( tmp );
	HWND wnd = ( HWND )leLODSwitch->getHandle();
	if( setLODchoice )
	{
		cLODChoice->select( index );
	}
	setModelInfo();
	InvalidateRect( wnd, NULL, TRUE );
	UpdateWindow( wnd );
}

void
ControlPanel::setLODMetric( float metric )
{
	static int saveMetric = -10;
	int intMetric = ( int )metric;
	if( intMetric == saveMetric )
	{
		return;
	}
	saveMetric = intMetric;
	char tmp[128];
	sprintf( tmp, "%d", intMetric );
	lLODMetric->setLabel( tmp );
}

void
ControlPanel::setPolycount( int polycount )
{
	static int savePolycount = -10;
	if( polycount == savePolycount )
	{
		return;
	}
	savePolycount = polycount;
	char tmp[128];
	sprintf( tmp, "Polycount: %d", polycount );
	lModelInfo3->setLabel( tmp );
}

void
ControlPanel::setTransparent( bool isTransparent )
{
	static int saveTransparent = -1;
	if( (int)isTransparent == saveTransparent )
		return;

	saveTransparent = isTransparent;
	char tmp[128];
	sprintf( tmp, "Model is: %s", isTransparent ? "transparent" : "opaque" );
	lModelInfo4->setLabel( tmp );
}

void
ControlPanel::updatePoseParameters( )
{
	for (int i = 0; i < NUM_POSEPARAMETERS; i++)
	{
		if (slPoseParameter[i]->isEnabled())
		{
			int j = cPoseParameter[i]->getSelectedIndex();
			float value = g_pStudioModel->GetPoseParameter( j );

			float temp = atof( lePoseParameter[i]->getLabel( ) );

			if (fabs( temp - value ) > 0.1)
			{
				slPoseParameter[i]->setValue( value );
				lePoseParameter[i]->setLabel( "%.1f", value );
			}
		}
	}
}

void
ControlPanel::setShowGround (bool b)
{
	g_viewerSettings.showGround = b;
	cbGround->setChecked (b);
}

void
ControlPanel::setAutoLOD( bool b )
{
	g_viewerSettings.autoLOD = b;
	cbAutoLOD->setChecked( b );
}

void
ControlPanel::setSoftwareSkin( bool b )
{
	g_viewerSettings.softwareSkin = b;
	cbSoftwareSkin->setChecked( b );
}

void
ControlPanel::setOverbright( bool b )
{
	g_viewerSettings.overbright = b;
	cbOverbright2->setChecked( b );
}

void
ControlPanel::setShowMovement (bool b)
{
	g_viewerSettings.showMovement = b;
	cbMovement->setChecked (b);
}

void
ControlPanel::setShowBackground (bool b)
{
	g_viewerSettings.showBackground = b;
	cbBackground->setChecked (b);
}

void
ControlPanel::setShowNormals (bool b)
{
	g_viewerSettings.showNormals = b;
	cbNormals->setChecked (b);
}

void
ControlPanel::setShowTangentFrame (bool b)
{
	g_viewerSettings.showTangentFrame = b;
	cbTangentFrame->setChecked (b);
}

void
ControlPanel::setOverlayWireframe (bool b)
{
	g_viewerSettings.overlayWireframe = b;
	cbOverlayWireframe->setChecked (b);
}

void
ControlPanel::setDisplacementMapping( bool b )
{
	g_viewerSettings.enableDisplacementMapping = b;
	cbDisplacementMap->setChecked( b );
}

void
ControlPanel::setShowShadow (bool b)
{
	g_viewerSettings.showShadow = b;
	cbShadow->setChecked (b);
}

void
ControlPanel::setShowHitBoxes (bool b)
{
	g_viewerSettings.showHitBoxes = b;
	cbHitBoxes->setChecked (b);
}

void
ControlPanel::setShowBones (bool b)
{
	g_viewerSettings.showBones = b;
	cbBones->setChecked (b);
}

void
ControlPanel::setShowAttachments (bool b)
{
	g_viewerSettings.showAttachments = b;
	cbAttachments->setChecked (b);
}


void
ControlPanel::setPlaySounds (bool b)
{
	g_viewerSettings.playSounds = b;
	cbPlaySounds->setChecked (b);
}

void
ControlPanel::setShowOriginAxis (bool b)
{
	g_viewerSettings.showOriginAxis = b;
	cbShowOriginAxis->setChecked (b);
}

void ComposeSequenceDisplayName( CStudioHdr *hdr, int nSequence, char *buffer, int bufferLength )
{
	if ( g_viewerSettings.showSequenceIndices )
	{
		if ( g_viewerSettings.showActivities )
		{
			V_snprintf( buffer, bufferLength, "[%d] %s", nSequence, hdr->pSeqdesc(nSequence).pszActivityName() );
		}
		else
		{
			V_snprintf( buffer, bufferLength, "[%d] %s", nSequence, hdr->pSeqdesc(nSequence).pszLabel() );
		}
	}
	else if ( g_viewerSettings.showActivities )
	{
		V_strncpy( buffer, hdr->pSeqdesc(nSequence).pszActivityName(), bufferLength );
	}
	else
	{
		V_strncpy( buffer, hdr->pSeqdesc(nSequence).pszLabel(), bufferLength );
	}
}

struct SortInfo_t
{
	int m_nSequence;
	char m_szName[256];
	int m_nType;
};

int SortSequenceFunc( const void *p1, const void *p2 ) 
{
	const SortInfo_t* pSort1 = (const SortInfo_t*)p1;
	const SortInfo_t* pSort2 = (const SortInfo_t*)p2;
	if ( pSort1->m_nType < pSort2->m_nType )
		return -10000;
	if ( pSort1->m_nType > pSort2->m_nType )
		return 10000;
	return Q_stricmp( pSort1->m_szName, pSort2->m_szName );
}

void ControlPanel::CreateSortedSequenceList( CStudioHdr* hdr, int *pSequence )
{
	int nSequenceCount = hdr->GetNumSeq();
	SortInfo_t *pSort = (SortInfo_t*)malloc( nSequenceCount * sizeof(SortInfo_t) );

	// Set up sort info
	for ( int j = 0; j < nSequenceCount; j++ )
	{
		pSort[j].m_nSequence = j;
		ComposeSequenceDisplayName( hdr, j, pSort[j].m_szName, sizeof( pSort[j].m_szName ) );

		pSort[j].m_nType = 0;

		const char *pKeyValuesText = Studio_GetKeyValueText( hdr, j );
		if ( !pKeyValuesText )
			continue;

		KeyValues *pKeyValues = new KeyValues( "sort" );

		if ( pKeyValues->LoadFromBuffer( "mdl", pKeyValuesText ) )
		{
			KeyValues *pFacePoserKeys = pKeyValues->FindKey( "faceposer" );
			if ( pFacePoserKeys )
			{
				const char *pType = pFacePoserKeys->GetString( "type", "" );
				if ( !Q_stricmp( pType, "posture" ) )
				{
					pSort[j].m_nType = 2;
				}
				else if ( !Q_stricmp( pType, "gesture" ) )
				{
					pSort[j].m_nType = 1;
				}
			}
		}
		pKeyValues->deleteThis();
	}

	if ( g_viewerSettings.sortSequences )
	{
		qsort( pSort, nSequenceCount, sizeof(SortInfo_t), SortSequenceFunc );
	}

	for ( int i = 0; i < nSequenceCount; ++i )
	{
		pSequence[i] = pSort[i].m_nSequence;
	}

	free( pSort );
}

void ControlPanel::initSequenceChoices( int iOnlyInitSlot /* = -1 */ )
{
	CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
	if (hdr)
	{
		int nSequenceCount = hdr->GetNumSeq();
		m_iLastSequenceCount = nSequenceCount;
		int *pSequence = (int*)malloc( nSequenceCount * sizeof(int) );
		CreateSortedSequenceList( hdr, pSequence );

		char composedName[256];
		char filter[64];
		for (int i = 0; i < MAX_SEQUENCES; i++)
		{
			if ( iOnlyInitSlot >= 0 && i != iOnlyInitSlot )
				continue;

			if ( iSelectionToSequence[i] )
			{
				free( iSelectionToSequence[i] );
			}
			int iAllocSize = nSequenceCount * sizeof(int);
			iSelectionToSequence[i] = (int*)malloc( iAllocSize );
			memset( iSelectionToSequence[i], 0, iAllocSize );

			if ( iSequenceToSelection[i] )
			{
				free( iSequenceToSelection[i] );
			}
			iSequenceToSelection[i] = (int*)malloc( iAllocSize );
			memset( iSequenceToSelection[i], 0, iAllocSize );

			GetSequenceFilter( i, filter, sizeof(filter) );

			cSequence[i]->removeAll();

			// filter sequence list
			int k = 0;
			for (int j = 0; j < nSequenceCount; j++)
			{
				int nSequence = pSequence[j];

				ComposeSequenceDisplayName( hdr, nSequence, composedName, sizeof( composedName ) );

				bool bFilteredOut = ( *filter && !V_stristr( composedName, filter ) );

				if ( !bFilteredOut && ( g_viewerSettings.showHidden || !(hdr->pSeqdesc(nSequence).flags & STUDIO_HIDDEN) ) )
				{
					cSequence[i]->add( composedName );
					SetSequenceForSelection( i, k, nSequence );
					SetSelectionForSequence( i, nSequence, k );
					k++;
				}
				else
				{
					// previous valid selection
					SetSelectionForSequence( i, nSequence, (k > 0) ? (k - 1) : 0 );
				}
			}
			if ( k == 0 )
			{
				ComposeSequenceDisplayName( hdr, 0, composedName, sizeof( composedName ) );
				cSequence[i]->add( composedName );
				SetSequenceForSelection( i, 0, 0 );
				SetSelectionForSequence( i, 0, 0 );
			}

			cSequence[i]->select( 0 );

			if ( iOnlyInitSlot == -1 )
				slSequence[i]->setValue( 0 );

			int iSequence = GetSequenceForSelection( i, 0 );
			if (i == 0)
			{
				setSequence( iSequence );
				showActivityModifiers( iSequence );
			}
			else
			{
				setOverlaySequence( i, iSequence, slSequence[i]->getValue() );
			}
		}
		free( pSequence );
	}

	if ( iOnlyInitSlot == -1 )
	{
		float flMin, flMax;
		for (int i = 0; i < NUM_POSEPARAMETERS; i++)
		{
			if (g_pStudioModel->GetPoseParameterRange( i, &flMin, &flMax ))
			{
				cPoseParameter[i]->removeAll();
				for (int j = 0; j < hdr->GetNumPoseParameters(); j++)
				{
					cPoseParameter[i]->add( hdr->pPoseParameter(j).pszName() );
				}
				cPoseParameter[i]->select( i );
				cPoseParameter[i]->setEnabled( true );
				cPoseParameter[i]->setVisible( true );

				slPoseParameter[i]->setEnabled( true );
				slPoseParameter[i]->setRange( flMin, flMax, 1000 );
				mxToolTip::add (slPoseParameter[i], hdr->pPoseParameter(i).pszName() );
				slPoseParameter[i]->setVisible( true );
				lePoseParameter[i]->setVisible( true );
				lePoseParameter[i]->setLabel( "%.1f", 0.0 );
			}
			else
			{
				cPoseParameter[i]->setEnabled( false );
				cPoseParameter[i]->setVisible( false );
				slPoseParameter[i]->setEnabled( false );
				slPoseParameter[i]->setVisible( false );
				lePoseParameter[i]->setVisible( false );
			}
			slPoseParameter[i]->setValue( 0.0 );
			setBlend( i, 0.0 );
		}

		if ( hdr )
		{
			for (int i = 0; i < hdr->GetNumPoseParameters(); i++)
			{
				setBlend( i, 0.0 );
			}
		}
	}
}


void ControlPanel::setSequence( int nSequence )
{
	int nSelection = GetSelectionForSequence( 0, nSequence );
	if ( nSelection >= 0 )
	{
		cSequence[0]->select( nSelection );
		g_pStudioModel->SetSequence( nSequence );
	}
	else
	{
		cSequence[0]->select( 0 );
		g_pStudioModel->SetSequence( GetSequenceForSelection( 0, nSelection ) );
	}

	updateFrameSelection();
	updateGroundSpeed();
}


void ControlPanel::updateGroundSpeed( void )
{
	char sz[100];
	float flGroundSpeed = g_pStudioModel->GetGroundSpeed();
	sprintf( sz, "Speed: %.2f", flGroundSpeed );
	laGroundSpeed->setLabel( sz );
}


void ControlPanel::setOverlaySequence( int num, int nSequence, float weight )
{
	int nSelection = GetSelectionForSequence( num, nSequence );
	if ( nSelection >= 0 )
	{
		cSequence[num]->select( nSelection );	
		g_pStudioModel->SetOverlaySequence( num-1, nSequence, weight );
	}
	else
	{
		cSequence[num]->select( 0 );
		nSequence = GetSequenceForSelection( num, 0 );
		g_pStudioModel->SetOverlaySequence( num-1, nSequence, weight );
	}
	slSequence[num]->setValue( weight );

	updateFrameSelection();
}


void ControlPanel::startBlending( void )
{
	g_pStudioModel->StartBlending();
}

void ControlPanel::updateTransitionAmount( void )
{
	char sz[ 256 ];
	sprintf( sz, "%.3f %%", 100.0f * g_pStudioModel->GetTransitionAmount() );
	laBlendAmount->setLabel( sz );
}


int ControlPanel::getFrameSelection( void )
{
	for ( int i = 0; i < MAX_SEQUENCES; i++ )
		if ( rbFrameSelection[i]->isChecked() )
			return i;
	return 0;
}

void ControlPanel::setFrame( float frame )
{
	int iLayer = getFrameSelection();
	int iFrame = g_pStudioModel->SetFrame( iLayer, frame );
	char buf[128];
	sprintf(buf, "%3d", iFrame );
	lForcedFrame->setLabel( buf );
}


void ControlPanel::updateFrameSelection( void )
{
	int iLayer = getFrameSelection();
	int maxFrame = g_pStudioModel->GetMaxFrame( iLayer );
	slForceFrame->setRange( 0, maxFrame, maxFrame );
	slForceFrame->setSteps(1,1);
}

void ControlPanel::updateFrameSlider( void )
{
	int iLayer = getFrameSelection();
	float flFrame = g_pStudioModel->GetFrame( iLayer );
	char buf[128];
	sprintf(buf, "%3.1f", flFrame );
	lForcedFrame->setLabel( buf );
	slForceFrame->setValue( flFrame );

	UpdateEventHistory();
}

void ControlPanel::UpdateEventHistory( void )
{
	int iLayer = getFrameSelection();
	float cycle = g_pStudioModel->GetCycle( iLayer );

	CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
	if ( !hdr )
	{
		return;
	}

	int sequence = 0;
	if ( !iLayer )
	{
		sequence = g_pStudioModel->GetSequence();
	}
	else
	{
		sequence = g_pStudioModel->GetOverlaySequence( iLayer-1 );
	}

	if ( !hdr->SequencesAvailable() )
		return;

	if ( sequence < 0 || sequence >= hdr->GetNumSeq() )
		return;

	mstudioseqdesc_t &desc = hdr->pSeqdesc( sequence );
	for ( int i=0; i<desc.numevents; ++i )
	{
		mstudioevent_t *e = desc.pEvent( i );

		bool isInCycleRange = false;
		if ( cycle >= m_lastEventCycle )
		{
			if ( e->cycle >= m_lastEventCycle && e->cycle < cycle )
			{
				isInCycleRange = true;
			}
		}
		else
		{
			// wrap-around
			if ( e->cycle >= m_lastEventCycle && e->cycle < 1.0f )
			{
				isInCycleRange = true;
			}
			if ( e->cycle >= 0.0f && e->cycle < cycle )
			{
				isInCycleRange = true;
			}
		}

		if ( isInCycleRange )
		{
			const char *eventName = e->pszEventName();
			const char *eventOptions = e->pszOptions();

			while ( lbEventHistory->getItemCount() > 10 )
			{
				lbEventHistory->remove( 0 );
			}

			char buf[128];
			if ( e->event == 0 )
			{
				sprintf( buf, "%.2f: %s %s", e->cycle, eventName, eventOptions );
			}
			else
			{
				sprintf( buf, "%.2f: %d %s", e->cycle, e->event, eventOptions );
			}
			lbEventHistory->add( buf );
			int count = lbEventHistory->getItemCount();
			lbEventHistory->select( count - 1 );

			ComposeSequenceDisplayName( hdr, sequence, buf, sizeof( buf ) );
			lEventSequence->setLabel( "%s", buf );
		}
	}

	m_lastEventCycle = cycle;
}

void ControlPanel::setSpeedScale( float scale )
{
	slSpeedScale->setValue( scale );
	g_viewerSettings.speedScale = scale;

	updateSpeedScale( );
}


void ControlPanel::updateSpeedScale( void )
{
	char szFPS[32];
	sprintf( szFPS, "x %.2f = %.1f fps", g_viewerSettings.speedScale, g_viewerSettings.speedScale * g_pStudioModel->GetFPS( ) );
	laFPS->setLabel( szFPS );
}


void ControlPanel::setBlend(int index, float value )
{
	g_pStudioModel->SetPoseParameter( index, value );
	// reset number of frames....
	updateFrameSelection( );

	updateGroundSpeed( );
}

void
ControlPanel::initBodypartChoices()
{
	CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
	if (hdr)
	{
		int i;
		mstudiobodyparts_t *pbodyparts = hdr->pBodypart(0);

		cBodypart->removeAll();
		if (hdr->numbodyparts() > 0)
		{
			for (i = 0; i < hdr->numbodyparts(); i++)
				cBodypart->add (pbodyparts[i].pszName());

			cBodypart->select (0);

			cSubmodel->removeAll();
			for (i = 0; i < pbodyparts[0].nummodels; i++)
			{
				char str[64];
				sprintf (str, "Submodel %d", i + 1);
				cSubmodel->add (str);
			}
			cSubmodel->select (0);
		}

		cBodyGroupPreset->removeAll();		
		if ( hdr->GetNumBodyGroupPresets() > 0 )
		{
			const mstudiobodygrouppreset_t *pbodygrouppresets = hdr->GetBodyGroupPreset(0);
			for (i = 0; i < hdr->GetNumBodyGroupPresets(); i++)
			{
				cBodyGroupPreset->add( pbodygrouppresets[i].pszName() );
			}
			cBodyGroupPreset->setEnabled( true );
		}
		else
		{
			cBodyGroupPreset->setEnabled( false );
		}

	}
}



void
ControlPanel::setBodypart (int index)
{
	CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
	if (hdr)
	{
		//cBodypart->setEn
		cBodypart->select (index);
		if (index < hdr->numbodyparts())
		{
			mstudiobodyparts_t *pbodyparts = hdr->pBodypart(0);
			cSubmodel->removeAll();
		
			for (int i = 0; i < pbodyparts[index].nummodels; i++)
			{
				char str[64];
				sprintf (str, "Submodel %d", i + 1);
				cSubmodel->add (str);
			}
			//instead of bashing the dropdown selection to 0, select whatever the state of this bodygroup is on the model
			cSubmodel->select ( g_pStudioModel->GetBodygroup( index ) );
		}
	}
	setModelInfo();
}



void
ControlPanel::setSubmodel (int index)
{
	g_pStudioModel->SetBodygroup (cBodypart->getSelectedIndex(), index);
	//!!g_viewerSettings.submodels[cBodypart->getSelectedIndex()] = index;
	setModelInfo();
}



void 
ControlPanel::initPhysicsBones()
{
	cHighlightBone->removeAll();
	cHighlightBone->add( "None" );
	for ( int i = 0; i < g_pStudioModel->Physics_GetBoneCount(); i++ )
	{
		cHighlightBone->add (g_pStudioModel->Physics_GetBoneName( i ) );
	}
	cHighlightBone->select (0);
}

void 
ControlPanel::initIncludedModels()
{
	cIncludedModels->removeAll();
	cIncludedModels->add( "Included Models" );
	cIncludedModels->add( "---------------" );

	int iNumIncludeModels = g_pStudioModel->GetNumIncludeModels();
	for ( int i=0; i<iNumIncludeModels; i++ )
	{
		cIncludedModels->add( g_pStudioModel->GetIncludeModelName(i) );
	}

	cIncludedModels->select (0);
	cIncludedModels->setEnabled( iNumIncludeModels > 0 );
}

void 
ControlPanel::initLODs()
{
	cLODChoice->removeAll();
	for ( int i = 0; i < g_pStudioModel->GetNumLODs(); i++ )
	{
		char tmp[10];
		sprintf( tmp, "%d", i );
		cLODChoice->add( tmp );
	}
	setLOD( 0, true, true );
}

void
ControlPanel::initBoneControllers()
{
	CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
	if (hdr)
	{
		cController->setEnabled (hdr->numbonecontrollers() > 0);
		slController->setEnabled (hdr->numbonecontrollers() > 0);
		cController->removeAll();

		int i = 0;
		for (i; i < hdr->numbonecontrollers(); i++)
		{
			char str[32];
			mstudiobonecontroller_t *pbonecontroller = hdr->pBonecontroller(i);
			sprintf (str, "Controller %d", pbonecontroller->inputfield);
			cController->add (str);
		}

		if (hdr->numbonecontrollers() > 0)
		{
			cController->select (0);
			mstudiobonecontroller_t *pbonecontroller = hdr->pBonecontroller(0);
			slController->setRange (pbonecontroller->start, pbonecontroller->end);
			slController->setValue (0);
		}
	}
}



void
ControlPanel::setBoneController (int index)
{
	CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
	if (hdr)
	{
		mstudiobonecontroller_t *pbonecontroller = hdr->pBonecontroller(index);
		slController->setRange ( pbonecontroller->start, pbonecontroller->end);
		slController->setValue (0);
	}
}



void
ControlPanel::setBoneControllerValue (int index, float value)
{
	CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
	if (hdr)
	{
		mstudiobonecontroller_t *pbonecontroller = hdr->pBonecontroller(index);
		g_pStudioModel->SetController (pbonecontroller->inputfield, value);
	}
}



void
ControlPanel::initSkinChoices()
{
	CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
	if (hdr)
	{
		cSkin->setEnabled (hdr->numskinfamilies() > 0);
		cSkin->removeAll();

		for (int i = 0; i < hdr->numskinfamilies(); i++)
		{
			char str[32];
			sprintf (str, "Skin %d", i + 1);
			cSkin->add (str);
		}

		cSkin->select (0);
		g_pStudioModel->SetSkin (0);
		g_viewerSettings.skin = 0;
	}
}

void ControlPanel::initMaterialChoices()
{
	CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
	if (hdr)
	{
		const studiohdr_t *pStudioHdr = hdr->GetRenderHdr();
		if (pStudioHdr) 
		{
			cMaterials->setEnabled(pStudioHdr->numtextures > 0);
			cMaterials->removeAll();

			for (int i = 0; i < pStudioHdr->numtextures; i++)
			{
				char str[512];
				sprintf (str, "%s", pStudioHdr->pTexture(i)->pszName() );
				cMaterials->add (str);
			}

			cMaterials->select (0);
			g_viewerSettings.materialIndex = 0;
		}
	}
}

void ControlPanel::showActivityModifiers( int sequence )
{
	CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
	if ( !hdr->SequencesAvailable() )
		return;

	if ( sequence < 0 || sequence >= hdr->GetNumSeq() )
		return;

	mstudioseqdesc_t &desc = hdr->pSeqdesc( sequence );

	cActivityModifiers->setEnabled( desc.numactivitymodifiers > 0);
	cActivityModifiers->removeAll();

	for (int i = 0; i < desc.numactivitymodifiers; i++)
	{
		char str[512];
		sprintf (str, "%s", desc.pActivityModifier( i )->pszName() );
		cActivityModifiers->add (str);
	}

	cActivityModifiers->select (0);
}

void 
ControlPanel::GetSequenceFilter( int sequenceSlot, char *pszFilterBuf, int iBufSize )
{
	if ( sequenceSlot < 0 || sequenceSlot >= MAX_SEQUENCES )
	{
		if ( iBufSize > 0 )
			pszFilterBuf[0] = '\0';
		return;
	}

	leSequenceFilter[sequenceSlot]->getText( pszFilterBuf, iBufSize );
}

void 
ControlPanel::setModelInfo()
{
	static char str[2048];
	CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();

	if (!hdr)
		return;

	static int checkSum = 0;
	static int boneLODCount = 0;
	static int numBatches = 0;
	if ( g_pStudioModel && !m_bVMTInfoLoaded )
	{
		UpdateMaterialList();
		UpdateMaterialVars();
	}

	if( checkSum == hdr->GetRenderHdr()->checksum && boneLODCount == g_DrawModelResults.m_NumHardwareBones && numBatches == g_DrawModelResults.m_NumBatches)
	{
		return;
	}

	checkSum = hdr->GetRenderHdr()->checksum;
	boneLODCount = g_DrawModelResults.m_NumHardwareBones;
	numBatches = g_DrawModelResults.m_NumBatches;

	int hbcount = 0;
	for ( int s = 0; s < hdr->numhitboxsets(); s++ )
	{
		hbcount += hdr->iHitboxCount( s );
	}
	
	sprintf (str,
		"Total bones: %d\n"
		"HW Bones: %d\n"
		"Batches: %d\n"
		"Bone Controllers: %d\n"
		"Hit Boxes: %d in %d sets\n"
		"Sequences: %d\n",
		hdr->numbones(),
		boneLODCount,
		numBatches,
		hdr->numbonecontrollers(),
		hbcount,
		hdr->numhitboxsets(),
		hdr->GetNumSeq()
		);

	lModelInfo1->setLabel (str);

	Vector vecHullExtent;
	vecHullExtent.Init();
	
	if ( hdr )
	{
		vecHullExtent.x = abs( hdr->hull_min().x - hdr->hull_max().x );
		vecHullExtent.y = abs( hdr->hull_min().y - hdr->hull_max().y );
		vecHullExtent.z = abs( hdr->hull_min().z - hdr->hull_max().z );
	}

	sprintf (str,
		"Materials: %d\n"
		"Skin Families: %d\n"
		"Bodyparts: %d\n"
		"Attachments: %d\n"
		"Body index: %d\n"
		"Hull extent (X): %.2f\n"
		"Hull extent (Y): %.2f\n"
		"Hull extent (Z): %.2f\n",
		g_DrawModelResults.m_NumMaterials,
		hdr->numskinfamilies(),
		hdr->numbodyparts(),
		hdr->GetNumAttachments(),
		g_pStudioModel->GetBodyIndex(),
		vecHullExtent.x,
		vecHullExtent.y,
		vecHullExtent.z		
		);

	lModelInfo2->setLabel (str);
}

void ControlPanel::UpdateMaterialList( )
{
	cMessageList->removeAll();
	studiohdr_t* pStudioR = g_pStudioModel->GetStudioRenderHdr();
	if ( pStudioR )
	{
		IMaterial *pMaterials[128];
		int nMaterials = g_pStudioRender->GetMaterialList( pStudioR, ARRAYSIZE( pMaterials ), &pMaterials[0] );

		for ( int i = 0; i < nMaterials; i++ )
		{
			char c_MaterialLine[256];
			Q_strcpy( c_MaterialLine, "" );

			if ( pMaterials[i]->IsErrorMaterial() )
			{
				Q_strcat( c_MaterialLine, "*** ERROR *** Model attempted to load one or more VMTs it can't find." , sizeof( c_MaterialLine ) );
			}
			else
			{
				Q_strcat( c_MaterialLine, pMaterials[i]->GetName() , sizeof( c_MaterialLine ) );
			}
			cMessageList->add( c_MaterialLine );
		}
		m_bVMTInfoLoaded = true;
	}
}

void ControlPanel::UpdateMaterialVars( )
{
	cMaterialList->removeAll();
	cMaterialParamList->removeAll();
	leMaterialParamText->clear();

	//hide slider controls
	slMaterialParamMatrixSliderRotation->setVisible(false);
	slMaterialParamMatrixSliderScaleX->setVisible(false);
	slMaterialParamMatrixSliderScaleY->setVisible(false);
	slMaterialParamMatrixSliderTranslateX->setVisible(false);
	slMaterialParamMatrixSliderTranslateY->setVisible(false);
	lblMatrixRotation->setVisible(false);
	lblMatrixScaleX->setVisible(false);
	lblMatrixScaleY->setVisible(false);
	lblMatrixTranslateX->setVisible(false);
	lblMatrixTranslateY->setVisible(false);
	slMaterialParamFloat->setVisible(false);

	studiohdr_t* pStudioR = g_pStudioModel->GetStudioRenderHdr();
	if ( pStudioR )
	{
		IMaterial *pMaterials[128];
		int nMaterials = g_pStudioRender->GetMaterialList( pStudioR, ARRAYSIZE( pMaterials ), &pMaterials[0] );

		//first parse all materials to see if any materials want to hide any others
		g_OnlyEditMaterialsThatWantToBeEdited = false;
		for (int i = 0; i < nMaterials; i++)
		{
			bool bLocalHideOthersInHLMV = false;
			pMaterials[i]->FindVar("$hlmvallowedit", &bLocalHideOthersInHLMV, false);

			if ( bLocalHideOthersInHLMV )
			{
				g_OnlyEditMaterialsThatWantToBeEdited = true;
				break;
			}
		}

		for ( int i = 0; i < nMaterials; i++ )
		{
			char c_MaterialLine[256];
			Q_strcpy( c_MaterialLine, "" );

			bool bLocalHideOthersInHLMV = false;
			pMaterials[i]->FindVar( "$hlmvallowedit", &bLocalHideOthersInHLMV, false );

			if ( pMaterials[i]->IsErrorMaterial() )
			{
				Q_strcpy( c_MaterialLine, "[error] could not load material" );
			}
			else if ( g_OnlyEditMaterialsThatWantToBeEdited && !bLocalHideOthersInHLMV )
			{
				Q_strcpy( c_MaterialLine, "[ " );
				Q_strcat( c_MaterialLine, V_GetFileName( pMaterials[i]->GetName() ), sizeof( c_MaterialLine ) );
				Q_strcat( c_MaterialLine, " ]", sizeof( c_MaterialLine ) );
			}
			else
			{
				Q_strcat( c_MaterialLine, V_GetFileName( pMaterials[i]->GetName() ), sizeof( c_MaterialLine ) );
			}

			cMaterialList->add(c_MaterialLine);
		}
	}
}


extern 	matrix3x4a_t g_viewtransform;

void ControlPanel::centerView( )
{
	Vector min, max;

	int hitboxset = 0; //g_MDLViewer->GetCurrentHitboxSet();

	min.Init( 999999,999999,999999);
	max.Init( -999999,-999999,-999999);

	matrix3x4_t rootxform;
	MatrixInvert( g_viewtransform, rootxform );

	HitboxList_t &list = g_pStudioModel->m_HitboxSets[ hitboxset ].m_Hitboxes;
	for (unsigned short j = list.Head(); j != list.InvalidIndex(); j = list.Next(j) )
	{
		mstudiobbox_t *pBBox = &list[j].m_BBox;

		Vector tmpmin, tmpmax;
		matrix3x4_t bonesetup;

		ConcatTransforms( rootxform, *g_pStudioModel->BoneToWorld( pBBox->bone ), bonesetup );

		TransformAABB( bonesetup, pBBox->bbmin, pBBox->bbmax, tmpmin, tmpmax );

		if (tmpmin.x < min.x) min.x = tmpmin.x;
		if (tmpmin.y < min.y) min.y = tmpmin.y;
		if (tmpmin.z < min.z) min.z = tmpmin.z;
		if (tmpmax.x > max.x) max.x = tmpmax.x;
		if (tmpmax.y > max.y) max.y = tmpmax.y;
		if (tmpmax.z > max.z) max.z = tmpmax.z;
	}

	if (min.x > max.x)
	{
		g_pStudioModel->ExtractBbox(min, max);
	}

	float dx = max[0] - min[0];
	float dy = max[1] - min[1];
	float dz = max[2] - min[2];

	// Determine the distance from the model such that it will fit in the screen.
	float d = dx;
	if (dy > d)
		d = dy;
	if (dz > d)
		d = dz;

	g_pStudioModel->m_origin[0] = d * 1.34f;
	g_pStudioModel->m_origin[1] = 0.0f;
	g_pStudioModel->m_origin[2] = min[2] + dz / 2;
	g_pStudioModel->m_angles[0] = 0.0f;
	g_pStudioModel->m_angles[1] = 0.0f;
	g_pStudioModel->m_angles[2] = 0.0f;
	g_viewerSettings.lightrot[0] = 0.0f;
	g_viewerSettings.lightrot[1] = 180.0f; // light should aim at models front
	g_viewerSettings.lightrot[2] = 0.0f;
	setFOV( 65.0f );
	d_MatSysWindow->redraw();
}


void ControlPanel::centerVerts( )
{
	g_pStudioModel->m_origin[0] = 0.0f;
	g_pStudioModel->m_origin[1] = 0.0f;
	g_pStudioModel->m_origin[2] = 0.0f;

	AngleMatrix( g_pStudioModel->m_angles, g_viewtransform );
	PositionMatrix( -g_pStudioModel->m_origin, g_viewtransform );

	Vector vecMin, vecMax;
	g_pStudioModel->ExtractVertExtents( vecMin, vecMax );

	g_pStudioModel->m_origin.x = vecMax.x;
	g_pStudioModel->m_origin.y = (vecMax.y + vecMin.y) * 0.5;
	g_pStudioModel->m_origin.z = (vecMax.z + vecMin.z) * 0.5;

	AngleMatrix( g_pStudioModel->m_angles, g_viewtransform );
	PositionMatrix( -g_pStudioModel->m_origin, g_viewtransform );

	d_MatSysWindow->redraw();
}

void ControlPanel::cs_gunsidemodelView()
{

	for (int i=0; i<HLMV_MAX_MERGED_MODELS; i++)
	{
		// FIXME: move to d_cpl
		if (g_pStudioExtraModel[i])
		{
			g_pStudioExtraModel[i]->FreeModel( false );
			delete g_pStudioExtraModel[i];
			g_pStudioExtraModel[i] = NULL;				
		}
	}

	setFOV( 80.0f );

	g_pStudioModel->m_angles[0] = 0.0f;
	g_pStudioModel->m_angles[1] = -90.0f;
	g_pStudioModel->m_angles[2] = 0.0f;

	centerVerts();

	Vector vecMin, vecMax;
	g_pStudioModel->ExtractVertExtents( vecMin, vecMax );
	g_pStudioModel->m_origin.x = MAX( MAX( MAX( abs(vecMax.z) * 2.0f, abs(vecMin.x) * 2.0f ), vecMax.y), abs(vecMin.y) );
	g_pStudioModel->m_origin.x *= 1.25f;
	
	g_viewerSettings.lightrot[0] = 0.0f;
	g_viewerSettings.lightrot[1] = 180.0f;
	g_viewerSettings.lightrot[2] = 0.0f;
	d_MatSysWindow->redraw();
}

void ControlPanel::viewmodelView()
{

	// Sit the camera at the origin with a 54 degree FOV for viewmodels
	g_pStudioModel->m_origin[0] = 0.0f;
	g_pStudioModel->m_origin[1] = 0.0f;
	g_pStudioModel->m_origin[2] = 0.0f;
	g_pStudioModel->m_angles[0] = 0.0f;
	g_pStudioModel->m_angles[1] = 180.0f;
	g_pStudioModel->m_angles[2] = 0.0f;
	g_viewerSettings.lightrot[0] = 0.0f;
	g_viewerSettings.lightrot[1] = 180.0f; // light should aim at models front
	g_viewerSettings.lightrot[2] = 0.0f;
	setFOV( 54.0f );
	d_MatSysWindow->redraw();
}

void ControlPanel::dotaView()
{
	// Set the camera to the standard DotA view
	g_pStudioModel->m_origin[0] = 1334.0f;
	g_pStudioModel->m_origin[1] = 0.0f;
	g_pStudioModel->m_origin[2] = 0.0f;

	g_pStudioModel->m_angles[0] = -60.0f;
	g_pStudioModel->m_angles[1] = 180.0f;
	g_pStudioModel->m_angles[2] = 0.0f;

	g_viewerSettings.lightrot[YAW] = 180.0f; // light should aim at models front
	g_viewerSettings.lightrot[PITCH] = 0.0f;
	g_viewerSettings.lightrot[ROLL] = 0.0f;

	setFOV( 65.0f );
	d_MatSysWindow->redraw();
}

void ControlPanel::setFOV( float fov )
{
	char buf[64];
	g_viewerSettings.fov = fov;
	sprintf( buf, "%.0f", fov );
	leFOV->setLabel( buf );
}


void ControlPanel::setOriginAxisLength( float originAxisLength )
{
	leOriginAxisLength->setValue( originAxisLength );
}


void ControlPanel::setCameraOrigin( float flX, float flY, float flZ )
{
	g_pStudioModel->m_origin[0] = flX;
	g_pStudioModel->m_origin[1] = flY;
	g_pStudioModel->m_origin[2] = flZ;
	d_MatSysWindow->redraw();
}

void ControlPanel::setCameraAngles(float flX, float flY, float flZ)
{
	g_pStudioModel->m_angles[0] = flX;
	g_pStudioModel->m_angles[1] = flY;
	g_pStudioModel->m_angles[2] = flZ;
	d_MatSysWindow->redraw();
}

void ControlPanel::setLightAngles(float flX, float flY, float flZ)
{
	g_viewerSettings.lightrot[YAW] = flX;
	g_viewerSettings.lightrot[PITCH] = flY;
	g_viewerSettings.lightrot[ROLL] = flZ;
	d_MatSysWindow->redraw();
}

void ControlPanel::setMaterialVar( const char *p_szMatParameterName, const char *p_szMatParameterValue )
{
	studiohdr_t* pStudioR = g_pStudioModel->GetStudioRenderHdr();
	if (!pStudioR)
		return;

	IMaterial *pMaterials[128];
	int nMaterials = g_pStudioRender->GetMaterialList(pStudioR, ARRAYSIZE(pMaterials), &pMaterials[0]);

	for (int i = 0; i < nMaterials; i++)
	{
		bool bFoundParam;
		IMaterialVar *pThisVar = pMaterials[i]->FindVar( p_szMatParameterName, &bFoundParam, false);
		if (bFoundParam)
			pThisVar->SetValueAutodetectType( p_szMatParameterValue );

		pMaterials[i]->RefreshPreservingMaterialVars();
	}
}

void ControlPanel::redrawMatSysWin( void )
{
	d_MatSysWindow->redraw();
}

//-----------------------------------------------------------------------------
// For any models in g_pStudioExtraModel, simply map the flex controllers
// locally but don't expose in the UI.  Previously they weren't accessible
// at all but this will allow them to be controlled via the bone flex drivers
//-----------------------------------------------------------------------------
void ControlPanel::MapExtraFlexes( int nSlot )
{
	if ( nSlot < 0 || nSlot >= ARRAYSIZE( g_pStudioExtraModel ) )
		return;

	StudioModel *pExtraStudioModel = g_pStudioExtraModel[ nSlot ];
	if ( !pExtraStudioModel )
		return;

	CStudioHdr *pExtraStudioHdr = pExtraStudioModel->GetStudioHdr();
	if ( !pExtraStudioHdr )
		return;

	for ( LocalFlexController_t i = static_cast< LocalFlexController_t >( 0 ); i < pExtraStudioHdr->numflexcontrollers(); ++i )
	{
		pExtraStudioHdr->pFlexcontroller( i )->localToGlobal = i;
	}
}


void ControlPanel::initFlexes()
{
	CStudioHdr *hdr = g_pStudioModel->GetStudioHdr();
	LocalFlexController_t i;
	int j;

	if (hdr)
	{
		for (j = 0; j < NUM_FLEX_SLIDERS; j++)
		{
			cFlex[j]->removeAll();
			for (i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++)
			{
				cFlex[j]->add( hdr->pFlexcontroller(i)->pszName() );
			}
			if ( false ) // TODO: Add a configuration option to do it the sensible way
			{
				cFlex[j]->select( j );
			}
			else
			{
				cFlex[j]->select( hdr->numflexcontrollers() - NUM_FLEX_SLIDERS + j );
			}
		}
	}

	for (i = LocalFlexController_t(0); i < hdr->numflexcontrollers(); i++)
	{
		g_pStudioModel->SetFlexController( i, 0.0f );
		hdr->pFlexcontroller( i )->localToGlobal = i;
	}

	for (j = 0; j < NUM_FLEX_SLIDERS; j++)
	{
		i = (LocalFlexController_t)cFlex[j]->getSelectedIndex();

		if (i >= 0)
		{
			slFlexScale[j]->setValue( g_pStudioModel->GetFlexControllerRaw( i ) );
		}
	}
}


static void UpdateSliderLabel( mxSlider *slider, mxLabel *label )
{
	float value = slider->getValue();
	char buf[80];
	sprintf( buf, "%0.2f", value );
	label->setLabel( buf );
}

void ControlPanel::UpdateConstraintSliders( int clamp )
{
	float vmin = slPhysicsConMin->getValue();
	float vmax = slPhysicsConMax->getValue();

	// reflect mins/maxs
	if ( cbLinked->isChecked() )
	{
		if ( clamp == IDC_PHYS_CON_MIN && vmin <= 0 )
		{
			vmax = -vmin;
		}
		else if ( vmax >= 0 )
		{
			vmin = -vmax;
		}
	}

	if ( vmax < vmin )
	{
		if ( clamp == IDC_PHYS_CON_MIN )
			vmin = vmax;
		else
			vmax = vmin;
	}

	slPhysicsConMin->setValue( vmin );
	slPhysicsConMax->setValue( vmax );
	UpdateSliderLabel( slPhysicsConMin, lPhysicsConMin );
	UpdateSliderLabel( slPhysicsConMax, lPhysicsConMax );
	UpdateSliderLabel( slPhysicsFriction, lPhysicsFriction );
}


int ControlPanel::handlePhysicsEvent( mxEvent *event )
{
	switch( event->action )
	{
	case IDC_PHYS_BONE:
		setupPhysicsBone( cPhysicsBone->getSelectedIndex() );
		break;
	case IDC_PHYS_CON_AXIS_X:
	case IDC_PHYS_CON_AXIS_Y:
	case IDC_PHYS_CON_AXIS_Z:
		setupPhysicsAxis( cPhysicsBone->getSelectedIndex(), event->action - IDC_PHYS_CON_AXIS_X );
		break;
	case IDC_PHYS_CON_TYPE_FREE:
	case IDC_PHYS_CON_TYPE_FIXED:
	case IDC_PHYS_CON_TYPE_LIMIT:
		break;
	case IDC_PHYS_CON_MIN:
	case IDC_PHYS_CON_MAX:
	case IDC_PHYS_CON_FRICTION:
		UpdateConstraintSliders( event->action );
		break;
	case IDC_PHYS_CON_TEST:
		g_pStudioModel->Physics_SetPreview( cPhysicsBone->getSelectedIndex(), getPhysicsAxis(), slPhysicsConTest->getValue() );
		break;
	case IDC_PHYS_P_MASSBIAS:
		UpdateSliderLabel( slPhysicsParamMassBias, lPhysicsParamMassBias );
		break;
	case IDC_PHYS_P_INERTIA:
		UpdateSliderLabel( slPhysicsParamInertia, lPhysicsParamInertia );
		break;
	case IDC_PHYS_P_DAMPING:
		UpdateSliderLabel( slPhysicsParamDamping, lPhysicsParamDamping );
		break;
	case IDC_PHYS_P_ROT_DAMPING:
		UpdateSliderLabel( slPhysicsParamRotDamping, lPhysicsParamRotDamping );
		break;
	case IDC_PHYS_QCFILE:
		{
			writePhysicsData();
			char *pOut = g_pStudioModel->Physics_DumpQC();
			if ( pOut )
			{
				Sys_CopyStringToClipboard( pOut );
			}
			delete[] pOut;
			return 1;
		}
		break;

	}
	writePhysicsData();
	return 1;
}


int ControlPanel::handleSoftbodyEvent( mxEvent *event )
{
	switch ( event->action )
	{
	case IDC_SOFT_ITERATIONS:
		{
			int nIterations = int( slSoftbodyIterations->getValue() );
			leSoftbodyIterations->setLabel( CFmtStr( "Iterations: %d", nIterations ).Get() );
			g_SoftbodyEnvironment.SetSoftbodyIterations( nIterations );
		}
		break;
	case IDC_SOFT_SURFACE_STRETCH:
		if ( CSoftbody *pSoftbody = g_pStudioModel->GetSoftbody() )
		{
			pSoftbody->SetSurfaceStretch( slSoftbodySurfaceStretch->getValue() );
		}
		break;
	case IDC_SOFT_THREAD_STRETCH:
		if ( CSoftbody *pSoftbody = g_pStudioModel->GetSoftbody() )
		{
			pSoftbody->SetThreadStretch( slSoftbodyThreadStretch->getValue() );
		}
		break;
	case IDC_SOFT_WIND_STRENGTH:
	case IDC_SOFT_WIND_YAW:
	{
		QAngle vecWindAngle( 0, slSoftbodyWindYaw->getValue(), 0 );
		Vector vWindDir;
		AngleVectors( vecWindAngle, &vWindDir );
		g_SoftbodyEnvironment.SetWindDesc( vWindDir, slSoftbodyWindStrength->getValue() );
	}
	break;
	case IDC_SOFT_SIMULATE:
		g_viewerSettings.simulateSoftbodies = cbSoftbodySimulate->isChecked();
		if ( CSoftbody *pSoftbody = g_pStudioModel->GetSoftbody() )
		{
			pSoftbody->SetPose( MatrixTransform( g_viewtransform ) );
		}
		break;
	case IDC_SOFT_SHOW_POLYGONS:
		g_viewerSettings.softbodyDrawOptions.EnableLayers( RN_SOFTBODY_DRAW_POLYGONS, cbSoftbodyPolygons->isChecked() );
		break;
	case IDC_SOFT_SHOW_WIND:
		g_viewerSettings.softbodyDrawOptions.EnableLayers( RN_SOFTBODY_DRAW_WIND, cbSoftbodyWind->isChecked() );
		break;
	case IDC_SOFT_SHOW_INDICES:
		g_viewerSettings.softbodyDrawOptions.EnableLayers( RN_SOFTBODY_DRAW_INDICES, cbSoftbodyIndices->isChecked() );
		break;
	case IDC_SOFT_SHOW_EDGES:
		g_viewerSettings.softbodyDrawOptions.EnableLayers( RN_SOFTBODY_DRAW_EDGES, cbSoftbodyEdges->isChecked() );
		break;
	case IDC_SOFT_SHOW_BASES:
		g_viewerSettings.softbodyDrawOptions.EnableLayers( RN_SOFTBODY_DRAW_BASES, cbSoftbodyBases->isChecked() );
		break;
	}
	return 1;
}


void ControlPanel::handlePhysicsKey( mxEvent *event )
{
	if ( event->key == '[' || event->key == ']' )
	{
		int boneIndex = cPhysicsBone->getSelectedIndex();
		int boneCount = cPhysicsBone->getItemCount();
		int axisIndex = getPhysicsAxis();
		int axisCount = 3;

		if ( event->key == '[' )
		{
			axisIndex = (axisIndex+axisCount-1) % axisCount;
			if ( axisIndex == (axisCount-1) )
			{
				boneIndex = (boneIndex+boneCount-1) % boneCount;
			}
		}
		else
		{
			axisIndex = (axisIndex+1) % axisCount;
			if ( axisIndex == 0 )
			{
				boneIndex = (boneIndex+1) % boneCount;
			}
		}
		setPhysicsAxis( axisIndex );
		cPhysicsBone->select( boneIndex );
		setupPhysicsBone( boneIndex );
	}
}


void ControlPanel::setupPhysics( void )
{
	if (!PopulatePhysicsBoneList( cPhysicsBone) )
		return;

	cPhysicsBone->select (0);
	setPhysicsAxis(0);
	setupPhysicsBone( 0 );

	char buf[64];
	sprintf( buf, "%.2f", g_pStudioModel->Physics_GetMass() );
	leMass->setLabel( buf );

	// Default to "None"
	setHighlightBone(0);
}

static void SetSlider( mxSlider *slider, mxLabel *label, float value )
{
	slider->setValue( value );
	UpdateSliderLabel( slider, label );
}

int ControlPanel::getPhysicsAxis( void )
{
	for ( int i = 0; i < 3; i++ )
		if ( rbConstraintAxis[i]->isChecked() )
			return i;

	return 0;
}


void ControlPanel::setPhysicsAxis( int axisIndex )
{
	for ( int i = 0; i < 3; i++ )
	{
		rbConstraintAxis[i]->setChecked( (i==axisIndex)?true : false );
	}
}
void ControlPanel::setSoftbodyAxis( int axisIndex )
{
	for ( int i = 0; i < 3; i++ )
	{
		rbSoftbodyAxis[ i ]->setChecked( ( i == axisIndex ) ? true : false );
	}
}


void ControlPanel::setupPhysicsBone( int boneIndex )
{
	if (!g_pStudioModel->m_pPhysics)
		return;

	int axisIndex = getPhysicsAxis();
	setupPhysicsAxis( boneIndex, axisIndex );
	// read per-bone data
	hlmvsolid_t solid;
	g_pStudioModel->Physics_GetData( boneIndex, &solid, NULL );
	
	SetSlider( slPhysicsParamMassBias, lPhysicsParamMassBias, solid.massBias );
	SetSlider( slPhysicsParamInertia, lPhysicsParamInertia, solid.params.inertia );
	SetSlider( slPhysicsParamDamping, lPhysicsParamDamping, solid.params.damping );
	SetSlider( slPhysicsParamRotDamping, lPhysicsParamRotDamping, solid.params.rotdamping );

	// Find the bone
	CStudioHdr* pHdr = g_pStudioModel->GetStudioHdr();
	for ( int i = 0; i < pHdr->numbones(); i++ )
	{
		const mstudiobone_t* pBone = pHdr->pBone(i);
		if (!stricmp(pBone->pszName(), solid.name ))
		{
			// Once found, set the surface property accordingly
			lPhysicsMaterial->setLabel( g_pStudioModel->m_SurfaceProps[i].String() );
			break;
		}
	}

	// select the render/highlight bone
	setHighlightBone( boneIndex + 1 );

	redraw();
}


void ControlPanel::setupPhysicsAxis( int boneIndex, int axis )
{
	// read the per-axis data
	constraint_ragdollparams_t constraint;

	g_pStudioModel->Physics_GetData( boneIndex, NULL, &constraint );
	setPhysicsAxis( axis );
	SetSlider( slPhysicsConMin, lPhysicsConMin, constraint.axes[axis].minRotation );
	SetSlider( slPhysicsConMax, lPhysicsConMax, constraint.axes[axis].maxRotation );
	SetSlider( slPhysicsFriction, lPhysicsFriction, constraint.axes[axis].torque );
	g_pStudioModel->Physics_SetPreview( -1, 0, 0 );
	redraw();
}

void ControlPanel::writePhysicsData( void )
{
	int boneIndex = cPhysicsBone->getSelectedIndex();
	int axis = getPhysicsAxis();

	hlmvsolid_t	solid;
	constraint_ragdollparams_t	constraint;
	// read the existing data
	g_pStudioModel->Physics_GetData( boneIndex, &solid, &constraint );

	const char *pMass = leMass->getLabel();
	float mass = 0;
	if ( pMass )
	{
		mass = atof( pMass );
	}

	g_pStudioModel->Physics_SetMass( mass );

	// write back the data we're editing on this dialog
	constraint.axes[axis].minRotation = slPhysicsConMin->getValue();
	constraint.axes[axis].maxRotation = slPhysicsConMax->getValue();
	constraint.axes[axis].torque = slPhysicsFriction->getValue();

	solid.massBias = slPhysicsParamMassBias->getValue();
	solid.index = boneIndex;
	//solid.params.mass;
	solid.params.inertia = slPhysicsParamInertia->getValue();
	solid.params.damping = slPhysicsParamDamping->getValue();
	solid.params.rotdamping = slPhysicsParamRotDamping->getValue();

	// store it in the model
	g_pStudioModel->Physics_SetData( boneIndex, &solid, &constraint );
}

int ControlPanel::GetSequenceForSelection( int sequenceSlot, int selection )
{
	DbgAssert( selection >= 0 );
	return iSelectionToSequence[sequenceSlot][selection];
}

int ControlPanel::GetSelectionForSequence( int sequenceSlot, int sequence )
{
	DbgAssert( sequence >= 0 );
	return iSequenceToSelection[sequenceSlot][sequence];
}

int ControlPanel::SetSequenceForSelection( int sequenceSlot, int selection, int sequence )
{
	DbgAssert( selection >= 0 && sequence >= 0 );
	return iSelectionToSequence[sequenceSlot][selection] = sequence;
}

int ControlPanel::SetSelectionForSequence( int sequenceSlot, int sequence, int selection )
{
	DbgAssert( selection >= 0 && sequence >= 0 );
	return iSequenceToSelection[sequenceSlot][sequence] = selection;
}

void ControlPanel::SaveSelectedSequences( void )
{
	// Save the currently selected sequences
	for ( int i=0; i<MAX_SEQUENCES; i++ )
	{
		int index = cSequence[i]->getSelectedIndex();
		
		int iSequence = (index >= 0 ) ? GetSequenceForSelection( i, index ) : 0;
		m_iSavedSequences[i] = iSequence;
		if ( i > 0 )
		{
			m_flSavedWeights[i] = slSequence[i]->getValue();
		}
	}
}

void ControlPanel::SetFrameSlider( float flFrame )
{
	slForceFrame->setValue( flFrame );
}

void ControlPanel::RestoreSelectedSequences( void )
{
	for ( int i=0; i<MAX_SEQUENCES; i++ )
	{
		int iSequence = m_iSavedSequences[i];

		if ( i == 0 )
		{
			setSequence( iSequence );
		}
		else
 		{
 			setOverlaySequence( i, iSequence, m_flSavedWeights[i] );
 		}
	}
}
