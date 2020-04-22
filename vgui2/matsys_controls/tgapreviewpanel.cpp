//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "matsys_controls/tgapreviewpanel.h"
#include "bitmap/tgaloader.h"
#include "tier1/utlbuffer.h"
#include "filesystem.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


using namespace vgui;


//-----------------------------------------------------------------------------
//
// TGA Preview panel
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// constructor
//-----------------------------------------------------------------------------
CTGAPreviewPanel::CTGAPreviewPanel( vgui::Panel *pParent, const char *pName ) :
	BaseClass( pParent, pName )
{
}


//-----------------------------------------------------------------------------
// Sets the current TGA
//-----------------------------------------------------------------------------
void CTGAPreviewPanel::SetTGA( const char *pFullPath )
{
	int nWidth, nHeight;
	ImageFormat format;
	float flGamma;

	CUtlBuffer buf;
	if ( !g_pFullFileSystem->ReadFile( pFullPath, NULL, buf ) )
	{
		Warning( "Can't open TGA file: %s\n", pFullPath );
		return;
	}

	TGALoader::GetInfo( buf, &nWidth, &nHeight, &format, &flGamma );

	Shutdown();
	Init( nWidth, nHeight, true );
	m_TGAName = pFullPath;


	buf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	if ( !TGALoader::Load( (unsigned char*)GetImageBuffer(), buf, 
		  nWidth, nHeight, IMAGE_FORMAT_BGRA8888, flGamma, false ) )
	{
		Shutdown();
	}
	else
	{
		DownloadTexture();
	}

	InvalidateLayout();
}


//-----------------------------------------------------------------------------
// Gets the current TGA
//-----------------------------------------------------------------------------
const char *CTGAPreviewPanel::GetTGA() const
{
	return m_TGAName;
}

//-----------------------------------------------------------------------------
// Lays out the panel
//-----------------------------------------------------------------------------
void CTGAPreviewPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	Rect_t paintRect;
	paintRect.x = 0;
	paintRect.y = 0;
	paintRect.width = GetImageWidth();
	paintRect.height = GetImageHeight();
	SetPaintRect( &paintRect );
}
