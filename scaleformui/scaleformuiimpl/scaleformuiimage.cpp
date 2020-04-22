//========= Copyright � 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "stdafx.h"
#include "scaleformuiimage.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#pragma warning( disable: 4355 ) // disables ' 'this' : used in base member initializer list'

using namespace SF::Render;

//-----------------------------------------------------------------------------
// Purpose: Convert Valve ImageFormat to Scaleform ImageFormat
//-----------------------------------------------------------------------------
SF::Render::ImageFormat ScaleformUIImage::ConvertImageFormat( ::ImageFormat format )
{
	SF::Render::ImageFormat dstFormat = Image_R8G8B8A8;

	switch ( format )
	{
	case IMAGE_FORMAT_RGBA8888:
		dstFormat = Image_R8G8B8A8;
		break;
	case IMAGE_FORMAT_BGRA8888:
		dstFormat = Image_B8G8R8A8;
		break;
	case IMAGE_FORMAT_DXT5:
		dstFormat = Image_DXT5;
		break;
	default:
		DevMsg("ImageFormat %d not supported by ScaleformUIImage, using RGBA8888!", format);
		break;
	}

	return dstFormat;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ScaleformUIImage::ScaleformUIImage( const byte* defaultRgba, int defaultWidth, int defaultHeight, ::ImageFormat defaultFormat, SF::GFx::TextureManager* pTextureManager )
{
	m_nWidth = defaultWidth;
	m_nHeight = defaultHeight;
	m_format = defaultFormat;
	m_nRefCount = 0;
	m_pImage = NULL;
	m_pTextureManager = pTextureManager;
	
	// Set the default bits for the texture so we have something to show while the real icon is loading
	if ( defaultRgba )
	{
		InitFromBuffer( defaultRgba, defaultWidth, defaultHeight, defaultFormat );
	}
}

ScaleformUIImage::~ScaleformUIImage( void )
{
	if ( m_pImage )
	{
		m_pImage->pImage->Release();
		m_pImage->Release();
	}
}

void ScaleformUIImage::OnFinalRelease()
{
	delete this;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Image* ScaleformUIImage::GetImage( void ) 
{ 
	m_pImage->AddRef();
	return m_pImage;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScaleformUIImage::InitFromBuffer( const byte *rgba, int width, int height, ::ImageFormat format )
{
	// Check image updates properly if Steam persona changes
	Assert( rgba && width && height );
	// See if we need to create a new texture
	if ( m_pImage == NULL || width != m_nWidth || height != m_nHeight || format != m_format )
	{
		const SF::Render::ImageFormat sfFormat = ConvertImageFormat( format );
		const unsigned int mipLevelCount = 1;

		if ( m_pImage )
		{
			m_pImage->pImage->Release();
			m_pImage->pImage = RawImage::Create( sfFormat, mipLevelCount, ImageSize( width, height), ImageUse_Update, 0, m_pTextureManager );

			// apply scaling matrix
			SF::GFx::Size<float> scaleParameters( ( (float) m_nWidth ) / width, ( (float) m_nHeight ) / height );
			SF::GFx::Matrix2F textureMatrix = SF::GFx::Matrix2F::Scaling( scaleParameters.Width, scaleParameters.Height );
			m_pImage->SetMatrix( textureMatrix );

			ScaleformUIImpl::m_Instance.ForceUpdateImages();
		}
		else
		{
			m_pImage = new ImageDelegate( RawImage::Create( sfFormat, mipLevelCount, ImageSize( width, height), ImageUse_Update, 0, m_pTextureManager ) );
		}

		m_nWidth = width;
		m_nHeight = height;
		m_format = format;
	}

	if ( m_pImage )
	{
		// Copy data
		
		ImageData imageData;
		((RawImage*)(m_pImage->GetAsImage()))->GetImageData( &imageData );
		
		ImagePlane &imagePlane = imageData.GetPlaneRef( 0 );
		memcpy( imagePlane.pData, rgba, imagePlane.DataSize );

		m_pImage->Update();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ScaleformUIImage::Update( void )
{
#ifdef _X360
	X360_UpdateImageState();
#endif
}
