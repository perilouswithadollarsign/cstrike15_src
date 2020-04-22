//========= Copyright © 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "stdafx.h"
#include "sfuiinventoryimage.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace SF::Render;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ScaleformUIInventoryImage::ScaleformUIInventoryImage( uint64 itemid, const byte* defaultRgba, int defaultWidth, int defaultHeight, ::ImageFormat defaultFormat, SF::GFx::TextureManager* pTextureManager )
	: ScaleformUIImage( defaultRgba, defaultWidth, defaultHeight, defaultFormat, pTextureManager )
{
	m_itemid = itemid;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool ScaleformUIInventoryImage::LoadInventoryImage( const CUtlBuffer* rawImageData, int nWidth, int nHeight, ::ImageFormat format )
{
	if ( rawImageData && rawImageData->TellPut() >= ( nWidth * nHeight * 4 ) )
	{
#ifdef USE_OVERLAY_ON_INVENTORY_ICONS
		OverlayFromBuffer( (byte*)(rawImageData->Base()), nWidth, nHeight, format );
#else
		InitFromBuffer( (byte*)(rawImageData->Base()), nWidth, nHeight, format );
#endif
	}

	return true;
}

#ifdef USE_OVERLAY_ON_INVENTORY_ICONS
void ScaleformUIInventoryImage::OverlayFromBuffer( const byte *rgba, int width, int height, ::ImageFormat format )
{
	Assert( rgba && width && height );

	bool bForceOverwrite = false;

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
		bForceOverwrite = true;
	}

	if ( m_pImage )
	{
		// Copy data
		
		ImageData imageData;
		((RawImage*)(m_pImage->GetAsImage()))->GetImageData( &imageData );
		
		ImagePlane &imagePlane = imageData.GetPlaneRef( 0 );

		if ( bForceOverwrite )
		{
			memcpy( imagePlane.pData, rgba, imagePlane.DataSize );
		}
		else
		{
			Assert( ( width * height * 4 ) <= imagePlane.DataSize );

			// pixel by pixel copy overlaying the input rgba onto the pre-existing image data wherever the input is not 0
			for ( int y = 0; y < height; y++ )
			{
				for ( int x = 0; x < width; x++ )
				{
					if ( *( ( uint * )&( rgba[ ( y * ( width * 4 ) ) + ( x * 4 ) ] ) ) != 0 )
					{
						*( ( uint * )&( imagePlane.pData[ ( y * ( width * 4 ) ) + ( x * 4 ) ] ) ) = *( ( uint * )&( rgba[ ( y * ( width * 4 ) ) + ( x * 4 ) ] ) );
					}
				}
			}
		}

		m_pImage->Update();
	}
}
#endif