//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "filesystem.h"
#include "matsys_controls/tgapicker.h"
#include "matsys_controls/tgapreviewpanel.h"
#include "vgui_controls/Splitter.h"


using namespace vgui;


//-----------------------------------------------------------------------------
//
// Asset Picker with no preview
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CTGAPicker::CTGAPicker( vgui::Panel *pParent ) : 
	BaseClass( pParent, "TGA Files", "tga", "materialsrc", "tgaName", "CONTENT" )
{
	// Horizontal splitter for preview
	m_pPreviewSplitter = new Splitter( this, "PreviewSplitter", SPLITTER_MODE_VERTICAL, 1 );
	vgui::Panel *pSplitterLeftSide = m_pPreviewSplitter->GetChild( 0 );
	vgui::Panel *pSplitterRightSide = m_pPreviewSplitter->GetChild( 1 );

	// TGA preview
	m_pTGAPreview = new CTGAPreviewPanel( pSplitterRightSide, "TGAPreview" );
	m_pTGAPreview->MaintainProportions( true );

	// Standard browser controls
 	CreateStandardControls( pSplitterLeftSide );

	LoadControlSettingsAndUserConfig( "resource/tgapicker.res" );
}

CTGAPicker::~CTGAPicker()
{
}


//-----------------------------------------------------------------------------
// Derived classes have this called when the previewed asset changes
//-----------------------------------------------------------------------------
void CTGAPicker::OnSelectedAssetPicked( const char *pAssetName )
{
	char pFullPath[ MAX_PATH ];
	char pRelativePath[MAX_PATH];
	Q_snprintf( pRelativePath, sizeof(pRelativePath), "materialsrc\\%s", pAssetName );
	g_pFullFileSystem->RelativePathToFullPath( pRelativePath, "CONTENT", pFullPath, sizeof(pFullPath) );
	m_pTGAPreview->SetTGA( pFullPath );
}


//-----------------------------------------------------------------------------
//
// Purpose: Modal picker frame
//
//-----------------------------------------------------------------------------
CTGAPickerFrame::CTGAPickerFrame( vgui::Panel *pParent, const char *pTitle ) : 
	BaseClass( pParent )
{
	SetAssetPicker( new CTGAPicker( this ) );
	LoadControlSettingsAndUserConfig( "resource/tgapickerframe.res" );
	SetTitle( pTitle, false );
}


	
