//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#include "materialobjects/dmeprecompiledtexture.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "materialobjects/dmetexture.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmePrecompiledTexture, CDmePrecompiledTexture );

//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmePrecompiledTexture::OnConstruction() 
{
	m_ImageFileName.Init( this, "imageFileName" );
	m_Processors.Init( this, "processors" );
	m_pSourceTexture.Init( this, "sourceTexture" );
	m_nStartFrame.InitAndSet( this, "startFrame", -1 );
	m_nEndFrame.InitAndSet( this, "endFrame", -1 );
	m_nVolumeTextureDepth.InitAndSet( this, "volumeTextureDepth", 1 );
	m_nTextureArraySize.InitAndSet( this, "textureArraySize", 1 );
	m_nTextureType.Init( this, "textureType" );
	m_flBumpScale.InitAndSet( this, "bumpScale", 1.0f );
	m_flPFMScale.InitAndSet( this, "pfmScale", 1.0f );
	m_nFilterType.Init( this, "filterType" );
	m_bClampS.Init( this, "clamps" );
	m_bClampT.Init( this, "clampt" );
	m_bClampU.Init( this, "clampu" );
	m_bLoadMipLevels.Init( this, "loadMipLevels" );
	m_bBorder.Init( this, "border" );
	m_bNormalMap.Init( this, "normalMap" );
	m_bSSBump.Init( this, "ssBump" );
	m_bNoMip.Init( this, "noMip" );
	m_bAllMips.Init( this, "allMips" );
	m_bNoLod.Init( this, "noLod" );
	m_bNoDebugOverride.Init( this, "noDebugOverride" );
	m_bNoCompression.Init( this, "noCompression" );
	m_bHintDxt5Compression.Init( this, "hintDxt5Compression" );
}

void CDmePrecompiledTexture::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Ensures all settings are internally consistent
//-----------------------------------------------------------------------------
bool CDmePrecompiledTexture::ValidateValues()
{
	if ( ( m_nTextureType == DMETEXTURE_TYPE_CUBEMAP ) && ( m_nTextureArraySize != 1 ) )
	{
		// Cubemaps are packed into a single texture in a cross shape
		Warning( "Specified a cubemap with a texture array size != 1!\n" );
		return false;
	}

	if ( ( m_nVolumeTextureDepth > 1 ) && ( m_nTextureArraySize != 1 ) )
	{
		Warning( "Specified a volume texture with a texture array size != 1!\n" );
		return false;
	}

	if ( m_bLoadMipLevels )
	{
		char pBaseName[MAX_PATH];
		Q_FileBase( GetName(), pBaseName, sizeof( pBaseName ) );

		char pRight[16];
		V_StrRight( pBaseName, 5, pRight, sizeof( pRight ) );
		if ( !Q_stristr( pRight, "_mip0" ) )
		{
			Warning( "Invalid texture name (\"%s\") for explicitly loading mip levels - the top mip file should end in '_mip0'\n", GetName() );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT( CDmeTextureProcessor );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeTextureProcessor::OnConstruction()
{
}

void CDmeTextureProcessor::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeTP_ComputeMipmaps, CDmeTP_ComputeMipmaps );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeTP_ComputeMipmaps::OnConstruction()
{
	m_bNoNiceFiltering.Init( this, "noNiceFiltering" );
	m_bAlphaTestDownsampling.Init( this, "alphaTestDownsampling" );
	m_flAlphaTestDownsampleThreshhold.Init( this, "alphaTestDownsampleThreshhold" );
	m_flAlphaTestDownsampleHiFreqThreshhold.Init( this, "alphaTestDownsampleHiFreqThreshhold" );
}

void CDmeTP_ComputeMipmaps::OnDestruction()
{
}

void CDmeTP_ComputeMipmaps::ProcessTexture( CDmeTexture *pSrcTexture, CDmeTexture *pDstTexture )
{
	int nWidth = pSrcTexture->Width();
	int nHeight = pSrcTexture->Height();
	int nDepth = pSrcTexture->Depth();
	int nTotalMipCount = ImageLoader::GetNumMipMapLevels( nWidth, nHeight, nDepth );

	pSrcTexture->CopyAttributesTo( pDstTexture, TD_NONE );

	// Copying will copy references to src texture frames. Remove them.
	pDstTexture->RemoveAllFrames();

	DownsampleInfo_t info;
	info.m_flAlphaThreshhold = m_flAlphaTestDownsampleThreshhold;
	info.m_flAlphaHiFreqThreshhold = m_flAlphaTestDownsampleHiFreqThreshhold;
	info.m_nFlags = 0;
	if ( pSrcTexture->m_bClampS )
	{
		info.m_nFlags |= DOWNSAMPLE_CLAMPS;
	}
	if ( pSrcTexture->m_bClampT )
	{
		info.m_nFlags |= DOWNSAMPLE_CLAMPT;
	}
	if ( pSrcTexture->m_bClampU )
	{
		info.m_nFlags |= DOWNSAMPLE_CLAMPU;
	}
	if ( m_bAlphaTestDownsampling )
	{
		info.m_nFlags |= DOWNSAMPLE_ALPHATEST;
	}

	int nFrameCount = pSrcTexture->FrameCount();
	CDmeImage **ppSrcImages = (CDmeImage**)stackalloc( nTotalMipCount * sizeof(CDmeImage*) );
	CDmeImage **ppDstImages = (CDmeImage**)stackalloc( nTotalMipCount * sizeof(CDmeImage*) );
	for ( int f = 0; f < nFrameCount; ++f )
	{
		CDmeTextureFrame *pSrcFrame = pSrcTexture->GetFrame( f );
		CDmeTextureFrame *pDstFrame = pDstTexture->AddFrame();

		// Add mip level 0 for this frame
		CDmeImageArray *pSrcArray = pSrcFrame->GetMipLevel( 0 );
		if ( !pSrcArray )
			continue;

		for ( int m = 0; m < nTotalMipCount; ++m )
		{
			pDstFrame->AddMipLevel();
		}

		int nImageCount = pSrcFrame->ImageCount();
		for ( int i = 0; i < nImageCount; ++i )
		{
			pSrcTexture->GetImages( f, i, ppSrcImages, nTotalMipCount );
			CDmeImage *pSrcImage = ppSrcImages[0];
			if ( !pSrcImage )
				continue;

			int nMipWidth = nWidth;
			int nMipHeight = nHeight;
			int nMipDepth = nDepth;

			for ( int m = 0; m < nTotalMipCount; ++m )
			{
				CDmeImageArray *pDstArray = pDstFrame->GetMipLevel( m );
				CDmeImage *pSrcImage = ppSrcImages[m];
				CDmeImage *pDstImage = pDstArray->AddImage();
				ppDstImages[m] = pDstImage;
				if ( pSrcImage )
				{
					Assert( pSrcImage->Width() == nMipWidth );
					Assert( pSrcImage->Height() == nMipHeight );
					Assert( pSrcImage->Depth() == nMipDepth );

					// FIXME: Just assign the src image into the dest texture
					// and remove it from the src texture

					// For mips where we have data, just copy src -> dest.
					pDstImage->CopyFrom( pSrcImage );
				}
				else
				{
					// For mips where don't have data, generate a mip from the src data of the previous level
					Assert( m != 0 && ppDstImages[m-1] );
					if ( m_bNoNiceFiltering )
					{
						pDstImage->QuarterSize( ppDstImages[m-1] );
					}
					else
					{
						int nSrcMip = clamp( m-3, 0, nTotalMipCount );
						pDstImage->Init( nMipWidth, nMipHeight, nMipDepth, ppDstImages[nSrcMip]->Format(), ppDstImages[nSrcMip]->Gamma() ); 
						pDstImage->DownsampleNiceFiltered( info, ppDstImages[nSrcMip] );
					}
				}

				nMipWidth >>= 1; nMipHeight >>= 1; nMipDepth >>= 1;
				nMipWidth = MAX( 1, nMipWidth ); nMipHeight = MAX( 1, nMipHeight ); nMipDepth = MAX( 1, nMipDepth );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeTP_ChangeColorChannels, CDmeTP_ChangeColorChannels );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeTP_ChangeColorChannels::OnConstruction()
{
	m_nMaxChannels.Init( this, "maxChannels" );
}

void CDmeTP_ChangeColorChannels::OnDestruction()
{
}

static ImageFormat ComputeDestFormat( ImageFormat fmt, int nChannelCount )
{
	switch( fmt )
	{
	case IMAGE_FORMAT_RGBA8888:
	case IMAGE_FORMAT_ABGR8888:
	case IMAGE_FORMAT_ARGB8888:
	case IMAGE_FORMAT_BGRA8888:
		Assert( nChannelCount != 2 );
		return ( nChannelCount == 3 ) ? IMAGE_FORMAT_RGB888 : IMAGE_FORMAT_I8;

	case IMAGE_FORMAT_RGB888: 
	case IMAGE_FORMAT_BGR888:
	case IMAGE_FORMAT_BGRX8888:
	case IMAGE_FORMAT_RGBX8888:
		Assert( nChannelCount == 1 );
		return IMAGE_FORMAT_I8;

	case IMAGE_FORMAT_IA88:
		Assert( nChannelCount == 1 );
		return IMAGE_FORMAT_I8;

	case IMAGE_FORMAT_DXT5:
		Assert( nChannelCount == 3 );
		return IMAGE_FORMAT_DXT1;

	case IMAGE_FORMAT_RGBA32323232F:
		if ( nChannelCount == 3 )
			return IMAGE_FORMAT_RGB323232F;
		// fall through
	case IMAGE_FORMAT_RGB323232F:
		if ( nChannelCount == 2 )
			return IMAGE_FORMAT_RG3232F;
		// fall through
	case IMAGE_FORMAT_RG3232F:
		Assert( nChannelCount == 1 );
		return IMAGE_FORMAT_R32F;

	case IMAGE_FORMAT_RGBA16161616F:
		Assert( nChannelCount != 3 );
		if ( nChannelCount == 2 )
			return IMAGE_FORMAT_RG1616F;
		// fall through
	case IMAGE_FORMAT_RG1616F:
		Assert( nChannelCount == 1 );
		return IMAGE_FORMAT_R16F;

	default:
		Assert( 0 );
		break;
	}
	return fmt;
}

void CDmeTP_ChangeColorChannels::ProcessImage( CDmeImage *pDstImage, CDmeImage *pSrcImage )
{
	ImageFormat fmt = pSrcImage->Format();
	ImageFormat dstFmt = fmt;
	const ImageFormatInfo_t &info = ImageLoader::ImageFormatInfo( fmt );
	int nChannelCount = 0;
	if ( info.m_nNumRedBits > 0 ) ++nChannelCount;
	if ( info.m_nNumGreenBits > 0 ) ++nChannelCount;
	if ( info.m_nNumBlueBits > 0 ) ++nChannelCount;
	if ( info.m_nNumAlphaBits > 0 ) ++nChannelCount;
	if ( nChannelCount > m_nMaxChannels )
	{
		dstFmt = ComputeDestFormat( fmt, m_nMaxChannels );
	}

	pDstImage->CopyFrom( pSrcImage, dstFmt );
}

void CDmeTP_ChangeColorChannels::ProcessTexture( CDmeTexture *pSrcTexture, CDmeTexture *pDstTexture )
{
	pDstTexture->ProcessTexture( pSrcTexture, this, &CDmeTP_ChangeColorChannels::ProcessImage );
}


/*
class CDmeTP_OneOverMipLevelInAlpha : public CDmeTextureProcessor
{
DEFINE_ELEMENT( CDmeTP_OneOverMipLevelInAlpha, CDmeTextureProcessor );

public:
};

IMPLEMENT_ELEMENT_FACTORY( DmeTP_OneOverMipLevelInAlpha, CDmeTP_OneOverMipLevelInAlpha );

class CDmeTP_PreMultiplyColorByOneOverMipLevel : public CDmeTextureProcessor
{
DEFINE_ELEMENT( CDmeTP_PreMultiplyColorByOneOverMipLevel, CDmeTextureProcessor );

public:
};

IMPLEMENT_ELEMENT_FACTORY( DmeTP_PreMultiplyColorByOneOverMipLevel, CDmeTP_PreMultiplyColorByOneOverMipLevel );

class CDmeTP_NormalToDuDv : public CDmeTextureProcessor
{
DEFINE_ELEMENT( CDmeTP_NormalToDuDv, CDmeTextureProcessor );

public:
};

IMPLEMENT_ELEMENT_FACTORY( DmeTP_NormalToDuDv, CDmeTP_NormalToDuDv );

class CDmeTP_CompressTexture : public CDmeTextureProcessor
{
DEFINE_ELEMENT( CDmeTP_CompressTexture, CDmeTextureProcessor );

public:
CDmaVar< bool > m_bNormalGAMap;
CDmaVar< bool > m_bDXT5;		// FIXME: Should nocompress/dxt5 be combined?
CDmaVar< bool > m_bNoCompress;
};

IMPLEMENT_ELEMENT_FACTORY( DmeTP_CompressTexture, CDmeTP_CompressTexture );

class CDmeTP_GenerateMipmaps : public CDmeTextureProcessor
{
DEFINE_ELEMENT( CDmeTP_GenerateMipmaps, CDmeTextureProcessor );

public:
CDmaVar< bool > m_bNoNice;
CDmaVar< bool > m_bAlphaTest;
CDmaVar< float > m_flAlphaThreshhold;
CDmaVar< float > m_flAlphaThreshholdHigh;
};

IMPLEMENT_ELEMENT_FACTORY( DmeTP_GenerateMipmaps, CDmeTP_GenerateMipmaps );
*/