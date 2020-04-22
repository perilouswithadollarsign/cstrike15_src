//========= Copyright © 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "stdafx.h"
#include "sfuichromehtmlimage.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace SF::Render;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ScaleformUIChromeHTMLImage::ScaleformUIChromeHTMLImage( uint64 imageID, const byte* defaultRgba, int defaultWidth, int defaultHeight, ::ImageFormat defaultFormat, SF::GFx::TextureManager* pTextureManager )
	: ScaleformUIImage( defaultRgba, defaultWidth, defaultHeight, defaultFormat, pTextureManager )
{
	m_imageID = imageID;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool ScaleformUIChromeHTMLImage::LoadChromeHTMLImage( const byte* rgba, int width, int height, ::ImageFormat format )
{
	InitFromBuffer( rgba, width, height, format );

	return true;
}