//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include <vgui_controls/tgaimagepanel.h>
#include "bitmap/tgaloader.h"
#include "vgui/ISurface.h"
#include <keyvalues.h>
#include "tier1/fmtstr.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

CTGAImagePanel::CTGAImagePanel( vgui::Panel *parent, const char *name ) : BaseClass( parent, name )
{
	m_iTextureID = -1;
	m_bHasValidTexture = false;
	m_bLoadedTexture = false;
	m_bScaleImage = false;
	m_ImageColor = Color( 255, 255, 255, 255 );

	SetPaintBackgroundEnabled( false );
}

CTGAImagePanel::~CTGAImagePanel()
{
	// release the texture memory
}

void CTGAImagePanel::SetTGAFilename( const char *filename )
{
	m_sTGAFilenameWithPath = CFmtStr( "//MOD/%s", filename );
	m_sTGAFilename = filename;
}

void CTGAImagePanel::SetTGAFilenameNonMod( const char *filename )
{
	m_sTGAFilenameWithPath = filename;
	m_sTGAFilename = filename;
}

char const *CTGAImagePanel::GetTGAFilename() const
{
	return m_sTGAFilename.String();
}

void CTGAImagePanel::SetShouldScaleImage( bool state )
{
	m_bScaleImage = state;
}

void CTGAImagePanel::SetImageColor( Color imageColor )
{
	m_ImageColor = imageColor;
}

void CTGAImagePanel::GetSettings(KeyValues *outResourceData)
{
	BaseClass::GetSettings(outResourceData);

	outResourceData->SetBool("scaleImage", m_bScaleImage);
}

void CTGAImagePanel::ApplySettings(KeyValues *inResourceData)
{
	m_bScaleImage = inResourceData->GetBool("scaleImage", false);

	BaseClass::ApplySettings(inResourceData);
}

void CTGAImagePanel::Paint()
{
	if ( !m_bLoadedTexture )
	{
		m_bLoadedTexture = true;
		// get a texture id, if we haven't already
		if ( m_iTextureID < 0 )
		{
			m_iTextureID = vgui::surface()->CreateNewTextureID( true );

			if ( !m_bScaleImage )
			{
				SetSize( 180, 100 );
			}
		}

		// load the file
		CUtlMemory<unsigned char> tga;
		if ( TGALoader::LoadRGBA8888( m_sTGAFilenameWithPath, tga, m_iImageWidth, m_iImageHeight ) )
		{
			// set the textureID
			surface()->DrawSetTextureRGBA( m_iTextureID, tga.Base(), m_iImageWidth, m_iImageHeight );
			m_bHasValidTexture = true;

			if ( !m_bScaleImage )
			{
				// set our size to be the size of the tga
				SetSize( m_iImageWidth, m_iImageHeight );
			}
		}
		else
		{
			m_bHasValidTexture = false;
		}
	}

	// draw the image
	int wide, tall;
	if ( m_bHasValidTexture )
	{
		surface()->DrawGetTextureSize( m_iTextureID, wide, tall );
		surface()->DrawSetTexture( m_iTextureID );
		surface()->DrawSetColor( m_ImageColor );

		int iScaledWide = ( m_bScaleImage ) ? ( GetWide() ) : ( wide );
		int iScaledTall = ( m_bScaleImage ) ? ( GetTall() ) : ( tall );

		surface()->DrawTexturedRect( 0, 0, iScaledWide, iScaledTall );
	}
	else
	{
		// draw a black fill instead
		wide = 180, tall = 100;
		surface()->DrawSetColor( 0, 0, 0, 255 );
		surface()->DrawFilledRect( 0, 0, wide, tall );
	}
}
