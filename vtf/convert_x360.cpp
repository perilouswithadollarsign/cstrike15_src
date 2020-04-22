//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
//
//	Purpose: Force pc .VTF to preferred .VTF 360 format conversion
//	
//=====================================================================================//

#include "mathlib/mathlib.h" 
#include "tier1/strtools.h"
#include "cvtf.h"
#include "tier1/utlbuffer.h"
#include "tier0/dbg.h"
#include "tier1/utlmemory.h"
#include "bitmap/imageformat.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


// if the entire vtf file is smaller than this threshold, add entirely to preload
#define PRELOAD_VTF_THRESHOLD			2048

struct ResourceCopy_t
{
	void				*m_pData;
	int					m_DataLength;
	ResourceEntryInfo	m_EntryInfo;
};

//-----------------------------------------------------------------------------
// Converts to an alternate format
//-----------------------------------------------------------------------------
ImageFormat PreferredFormat( IVTFTexture *pVTFTexture, ImageFormat fmt, int width, int height, int mipCount, int faceCount, VtfConsoleFormatType_t targetConsole )
{
	switch ( fmt )
	{
		case IMAGE_FORMAT_RGBA8888: 
		case IMAGE_FORMAT_ABGR8888:
		case IMAGE_FORMAT_ARGB8888:
		case IMAGE_FORMAT_BGRA8888:
			return IMAGE_FORMAT_BGRA8888;

		// 24bpp gpu formats don't exist, must convert
		case IMAGE_FORMAT_BGRX8888:
		case IMAGE_FORMAT_RGB888:
		case IMAGE_FORMAT_BGR888:
		case IMAGE_FORMAT_RGB888_BLUESCREEN:
		case IMAGE_FORMAT_BGR888_BLUESCREEN:
			return IMAGE_FORMAT_BGRX8888;

		case IMAGE_FORMAT_BGRX5551:
		case IMAGE_FORMAT_RGB565:
		case IMAGE_FORMAT_BGR565:
			return IMAGE_FORMAT_BGR565;

		// no change
		case IMAGE_FORMAT_I8:
		case IMAGE_FORMAT_IA88:
		case IMAGE_FORMAT_A8:
		case IMAGE_FORMAT_BGRA4444:
		case IMAGE_FORMAT_BGRA5551:
		case IMAGE_FORMAT_UV88:
		case IMAGE_FORMAT_UVWQ8888:
		case IMAGE_FORMAT_UVLX8888:
		case IMAGE_FORMAT_DXT1_ONEBITALPHA:
		case IMAGE_FORMAT_DXT1:
		case IMAGE_FORMAT_DXT3:
		case IMAGE_FORMAT_DXT5:
		case IMAGE_FORMAT_ATI1N:
		case IMAGE_FORMAT_ATI2N:
			break;

		case IMAGE_FORMAT_RGBA16161616:
		case IMAGE_FORMAT_RGBA16161616F:
			if ( targetConsole == VTF_CONSOLE_PS3 )
			{
				return IMAGE_FORMAT_RGBA16161616F;
			}
			else
			{
				return IMAGE_FORMAT_RGBA16161616;
			}
	}

	return fmt;
}

//-----------------------------------------------------------------------------
// Determines target dimensions
//-----------------------------------------------------------------------------
bool ComputeTargetDimensions( const char *pDebugName, IVTFTexture *pVTFTexture, int picmip, int &width, int &height, int &mipCount, int &mipSkipCount, bool &bNoMip, int nMaxMip )
{
	width  = pVTFTexture->Width();
	height = pVTFTexture->Height();
	ImageFormat format = pVTFTexture->Format();

	// adhere to texture's internal lod setting
	int nClampX = 1<<30;
	int nClampY = 1<<30;

	if ( nMaxMip > 0 )
	{
		// the specified maxmip specified by the caller trumps any embedded LOD
		// cstrike has a merge ton of files with incorrect misspecified LODs from other products, etc. with no trivial way to discover or easily alter wholesale
		// MGD is now adhering to a central script that specifies the texture LODs for faster precise control that can provide wholesale discovery etc.
		nClampX = MIN( nClampX, nMaxMip );
		nClampY = MIN( nClampY, nMaxMip );
	}
	else
	{
		// a default maxmip defaults to smaller of 512x512 or optional embedded LOD
		nClampX = MIN( nClampX, 512 );
		nClampY = MIN( nClampY, 512 );

		// no maxmip has been explicitly specified by caller, use embedded LOD setting if present
		TextureLODControlSettings_t const *pLODInfo = reinterpret_cast<TextureLODControlSettings_t const *> ( pVTFTexture->GetResourceData( VTF_RSRC_TEXTURE_LOD_SETTINGS, NULL ) );
		if ( pLODInfo )
		{
			if ( pLODInfo->m_ResolutionClampX )
			{
				nClampX = MIN( nClampX, 1 << pLODInfo->m_ResolutionClampX );
			}
			if ( pLODInfo->m_ResolutionClampY )
			{
				nClampY = MIN( nClampY, 1 << pLODInfo->m_ResolutionClampY );
			}

			if ( pLODInfo->m_ResolutionClampX_360 )
			{
				nClampX = MIN( nClampX, 1 << pLODInfo->m_ResolutionClampX_360 );
			}
			if ( pLODInfo->m_ResolutionClampY_360 )
			{
				nClampY = MIN( nClampY, 1 << pLODInfo->m_ResolutionClampY_360 );
			}
		}
	}

	// spin down to desired texture size
	// not allowing top mips > 1MB
	mipSkipCount = 0;
	while ( ( mipSkipCount < picmip ) || 
			( width > nClampX ) || 
			( height > nClampY ) || 
			( ImageLoader::GetMemRequired( width, height, 1, format, false ) > 1 * 1024 * 1024 ) )
	{
		if ( width == 1 && height == 1 )
			break;
		width >>= 1;
		height >>= 1;
		if ( width < 1 )
			width = 1;
		if ( height < 1 )
			height = 1;
		mipSkipCount++;
	}

	bNoMip = false;
	if ( pVTFTexture->Flags() & TEXTUREFLAGS_NOMIP )
	{
		bNoMip = true;
	}

	// determine mip quantity based on desired width/height
	if ( bNoMip )
	{
		// avoid serializing unused mips
		mipCount = 1;
	}
	else
	{
		mipCount = ImageLoader::GetNumMipMapLevels( width, height );
	}

	// success
	return true;
}

//-----------------------------------------------------------------------------
// Align the buffer to specified boundary
//-----------------------------------------------------------------------------
int AlignBuffer( CUtlBuffer &buf, int alignment )
{
	int		curPosition;
	int		newPosition;
	byte	padByte = 0;

	// advance to aligned position
	buf.SeekPut( CUtlBuffer::SEEK_TAIL, 0 );
	curPosition = buf.TellPut();
	newPosition = AlignValue( curPosition, alignment );
	buf.EnsureCapacity( newPosition );

	// write empty
	for ( int i=0; i<newPosition-curPosition; i++ )
	{
		buf.Put( &padByte, 1 );
	}

	return newPosition;
}

//-----------------------------------------------------------------------------
// Convert an image from SRGB space to the 360 piecewise linear space.
//-----------------------------------------------------------------------------
bool SRGBCorrectImage( 
	unsigned char	*pImage,
	int				imageSize, 
	ImageFormat		imageFormat)
{
	if ( imageFormat == IMAGE_FORMAT_BGRA8888 || imageFormat == IMAGE_FORMAT_BGRX8888  || imageFormat == IMAGE_FORMAT_RGBA8888)
	{
		//Msg( "   Converting 8888 texture from sRGB gamma to 360 PWL gamma *** %dx%d\n", width, height );
		for ( int i = 0; i < ( imageSize / 4 ); i++ ) // imageSize is the raw data length in bytes
		{
			unsigned char *pRGB[3] = { NULL, NULL, NULL };

			pRGB[0] = &( pImage[ ( i * 4 ) + 0 ] ); 
			pRGB[1] = &( pImage[ ( i * 4 ) + 1 ] ); 
			pRGB[2] = &( pImage[ ( i * 4 ) + 2 ] ); 
			
			// Modify RGB data in place
			for ( int j = 0; j < 3; j++ ) // For red, green, blue
			{
				float flSrgbGamma = float( *( pRGB[j] ) ) / 255.0f;
				float fl360Gamma = SrgbGammaTo360Gamma( flSrgbGamma );

				fl360Gamma = clamp( fl360Gamma, 0.0f, 1.0f );
				*( pRGB[j] ) = ( unsigned char ) ( clamp( ( ( fl360Gamma * 255.0f ) + 0.5f ), 0.0f, 255.0f ) );
			}
		}
	}
	else if( imageFormat == IMAGE_FORMAT_BGR888)
	{
		//Msg( "   Converting 888 texture from sRGB gamma to 360 PWL gamma *** %dx%d\n", width, height );
		for ( int i = 0; i < ( imageSize / 3 ); i++ ) // imageSize is the raw data length in bytes
		{
			unsigned char *pRGB[3] = { NULL, NULL, NULL };

			pRGB[0] = &( pImage[ ( i * 3 ) + 0 ] ); // Blue
			pRGB[1] = &( pImage[ ( i * 3 ) + 1 ] ); // Green
			pRGB[2] = &( pImage[ ( i * 3 ) + 2 ] ); // Red
			
			// Modify RGB data in place
			for ( int j = 0; j < 3; j++ ) // For red, green, blue
			{
				float flSrgbGamma = float( *( pRGB[j] ) ) / 255.0f;
				float fl360Gamma = SrgbGammaTo360Gamma( flSrgbGamma );

				fl360Gamma = clamp( fl360Gamma, 0.0f, 1.0f );
				*( pRGB[j] ) = ( unsigned char ) ( clamp( ( ( fl360Gamma * 255.0f ) + 0.5f ), 0.0f, 255.0f ) );
			}
		}
	}
	else
	{
		Msg("Unknown format:%d In SRGBCorrectImage\n",imageFormat);
	}
	return true;
}

//-----------------------------------------------------------------------------
// Swizzle PS3 image data
//-----------------------------------------------------------------------------
static inline uint32 Ps3Helper_LinearToSwizzleAddress( uint16 c[3], uint16 size[3] )
{
	uint32 offset = 0;
	uint32 uOffsetBit = 1;
	do
	{
		for ( int jj = 0; jj < 3; ++ jj )
		{
			if ( ( size[jj] >>= 1 ) != 0 )
			{
				offset += ( c[jj] & 1 ) ? uOffsetBit : 0;
				uOffsetBit <<= 1;
				c[jj] >>= 1;
			}
		}
	} while ( c[0]+c[1]+c[2] );
	return offset;
}
void PostConvertSwizzleImageData(
								 unsigned char *pTargetImage,
								 int targetImageSize,
								 ImageFormat targetFormat,
								 int width, int height,
								 VtfConsoleFormatType_t targetConsole
								 )
{
	// Only applicable for PS3
	if ( targetConsole != VTF_CONSOLE_PS3 ) return;

	// Only applicable for power-of-two textures
	if ( !IsPowerOfTwo( width ) || !IsPowerOfTwo( height ) ) return;

	// Not applicable for DXT
	switch ( targetFormat )
	{
	case IMAGE_FORMAT_DXT1:
	case IMAGE_FORMAT_DXT3:
	case IMAGE_FORMAT_DXT5:
		return;
	}

	unsigned int numBlockBytes = ImageLoader::GetMemRequired( 1, 1, 1, targetFormat, false );
	if ( numBlockBytes > 4 )
	{
		width *= ( numBlockBytes / 4 );
		numBlockBytes = 4;
	}
	unsigned char *pSwizzleData = (unsigned char *)malloc( targetImageSize );
	int depth = 1;
	for ( uint16 x = 0; x < width; ++ x )
		for ( uint16 y = 0; y < height; ++ y )
			for ( uint16 z = 0; z < depth; ++ z )
			{
				uint16 c[3] = {x,y,z};
				uint16 sizeArg[3] = { width, height, depth };
				uint32 uiSwizzleOffset = Ps3Helper_LinearToSwizzleAddress( c, sizeArg );
				memcpy( pSwizzleData + (uiSwizzleOffset*numBlockBytes),
					pTargetImage+(y*width+x)*numBlockBytes, numBlockBytes );
			}
	memcpy( pTargetImage, pSwizzleData, targetImageSize );
	free( pSwizzleData );
}


//-----------------------------------------------------------------------------
// Convert the x86 image data to 360
//-----------------------------------------------------------------------------
bool ConvertImageFormatEx( 
	unsigned char	*pSourceImage,
	int				sourceImageSize, 
	ImageFormat		sourceFormat,
	unsigned char	*pTargetImage,
	int				targetImageSize, 
	ImageFormat		targetFormat,
	int				width, 
	int				height,
	VtfConsoleFormatType_t	targetConsole )
{
	// format conversion expects pc oriented data
	// but, formats that are >8 bits per channels need to be element pre-swapped
	ImageLoader::PreConvertSwapImageData( pSourceImage, sourceImageSize, sourceFormat, targetConsole );

	bool bRetVal = ImageLoader::ConvertImageFormat( 
				pSourceImage, 
				sourceFormat, 
				pTargetImage, 
				targetFormat, 
				width, 
				height );
	if ( !bRetVal )
	{
		return false;
	}

	// convert to proper channel order for 360 d3dformats
	ImageLoader::PostConvertSwapImageData( pTargetImage, targetImageSize, targetFormat, targetConsole );
	PostConvertSwizzleImageData( pTargetImage, targetImageSize, targetFormat, width, height, targetConsole );

	return true;
}

//-----------------------------------------------------------------------------
// Write the source data as the desired format into a target buffer
//-----------------------------------------------------------------------------
bool SerializeImageData( IVTFTexture *pSourceVTF, int frame, int face, int mip, ImageFormat targetFormat, CUtlBuffer &targetBuf, VtfConsoleFormatType_t targetConsole )
{
	int					width;
	int					height;
	int					targetImageSize;
	byte				*pSourceImage;
	int					sourceImageSize;
	int					targetSize;
	CUtlMemory<byte>	targetImage;

	width = pSourceVTF->Width() >> mip;
	height = pSourceVTF->Height() >> mip;
	if ( width < 1 )
		width = 1;
	if ( height < 1)
		height = 1;

	sourceImageSize = ImageLoader::GetMemRequired( width, height, 1, pSourceVTF->Format(), false );
	pSourceImage = pSourceVTF->ImageData( frame, face, mip );

	targetImageSize = ImageLoader::GetMemRequired( width, height, 1, targetFormat, false );
	targetImage.EnsureCapacity( targetImageSize );
	byte *pTargetImage = (byte*)targetImage.Base();

	// conversion may skip bytes, ensure all bits initialized
	memset( pTargetImage, 0xFF, targetImageSize );

	// format conversion expects pc oriented data
	bool bRetVal = ConvertImageFormatEx( 
				pSourceImage, 
				sourceImageSize,
				pSourceVTF->Format(), 
				pTargetImage, 
				targetImageSize,
				targetFormat, 
				width, 
				height,
				targetConsole );
	if ( !bRetVal )
	{
		return false;
	}

//X360TBD: incorrect byte order
//	// fixup mip dependent data
//	if ( ( pSourceVTF->Flags() & TEXTUREFLAGS_ONEOVERMIPLEVELINALPHA ) && ( targetFormat == IMAGE_FORMAT_BGRA8888 ) )
//	{
//		unsigned char ooMipLevel = ( unsigned char )( 255.0f * ( 1.0f / ( float )( 1 << mip ) ) );
//		int i;
//
//		for ( i=0; i<width*height; i++ )
//		{
//			pTargetImage[i*4+3] = ooMipLevel;
//		}
//	}

	targetSize = targetBuf.Size() + targetImageSize;
	targetBuf.EnsureCapacity( targetSize );
	targetBuf.Put( pTargetImage, targetImageSize );
	if ( !targetBuf.IsValid() )
	{
		return false;
	}

	// success
	return true;
}

//-----------------------------------------------------------------------------
// Generate the 360 target into a buffer
//-----------------------------------------------------------------------------

template< VtfConsoleFormatType_t e > struct ConvertVTFToConsoleFormatHelper_t
{
	;
};
template<> struct ConvertVTFToConsoleFormatHelper_t< VTF_CONSOLE_360 >
{
	typedef VTFFileHeaderX360_t Type;
	enum Const_t { VER_MAJOR = VTF_X360_MAJOR_VERSION, VER_MINOR = VTF_X360_MINOR_VERSION };
	static char const * GetHeaderType() { return "VTFX"; }
};
template<> struct ConvertVTFToConsoleFormatHelper_t< VTF_CONSOLE_PS3 >
{
	typedef VTFFileHeaderPS3_t Type;
	enum Const_t { VER_MAJOR = VTF_PS3_MAJOR_VERSION, VER_MINOR = VTF_PS3_MINOR_VERSION };
	static char const * GetHeaderType() { return "VTF3"; }
};
template < VtfConsoleFormatType_t eConFmtType >
bool ConvertVTFToConsoleFormatHelper( const char *pDebugName, CUtlBuffer &sourceBuf, CUtlBuffer &targetBuf, CompressFunc_t pCompressFunc, int nMaxMip )
{
	bool				bRetVal;
	IVTFTexture			*pSourceVTF;
	int					targetWidth;
	int					targetHeight;
	int					targetMipCount;
	typedef ConvertVTFToConsoleFormatHelper_t< eConFmtType > TargetHelper;
	typedef typename TargetHelper::Type TargetHeaderType;
	TargetHeaderType	targetHeader;
	int					frame;
	int					face;
	int					mip;
	ImageFormat			targetFormat;
	int					targetLowResWidth;
	int					targetLowResHeight;
	int					targetFlags;
	int					mipSkipCount;
	int					targetFaceCount;
	int					preloadDataSize;
	int					targetImageDataOffset;
	int					targetFrameCount;
	VTFFileHeaderV7_1_t	*pVTFHeader71;
	bool				bNoMip;
	CByteswap			byteSwapWriter;
	CUtlVector< ResourceCopy_t > targetResources;
	bool bHasLowResData = false;
	unsigned int resourceTypes[MAX_RSRC_DICTIONARY_ENTRIES];
	unsigned char targetLowResSample[4];
	int numTypes;
	
	// Only need to byte swap writes if we are running the coversion on the PC, and data will be read from console
	byteSwapWriter.ActivateByteSwapping( !IsGameConsole() );

	// need mathlib
	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );

	// default failure
	bRetVal = false;

	pSourceVTF = NULL;

	// unserialize the vtf with just the header
	pSourceVTF = CreateVTFTexture();
	if ( !pSourceVTF->Unserialize( sourceBuf, true, 0 ) )
		goto cleanUp;

	// volume textures not supported
	if ( pSourceVTF->Depth() != 1 )
		goto cleanUp;

	if ( !ImageLoader::IsFormatValidForConversion( pSourceVTF->Format() ) )
		goto cleanUp;

	if ( !ComputeTargetDimensions( pDebugName, pSourceVTF, 0, targetWidth, targetHeight, targetMipCount, mipSkipCount, bNoMip, nMaxMip ) )
		goto cleanUp;

	// must crack vtf file to determine if mip levels exist from header
	// vtf interface does not expose the true presence of this data
	pVTFHeader71 = (VTFFileHeaderV7_1_t*)sourceBuf.Base();
	if ( mipSkipCount >= pVTFHeader71->numMipLevels )
	{
		// can't skip mips that aren't there
		// ideally should just reconstruct them
		goto cleanUp;
	}

	// unserialize the vtf with all the data configured with the desired starting mip
	sourceBuf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	if ( !pSourceVTF->Unserialize( sourceBuf, false, mipSkipCount ) )
	{
		Msg( "ConvertVTFToConsoleFormatHelper: Error reading in %s\n", pDebugName );
		goto cleanUp;
	}

	// add the default resource image
	ResourceCopy_t resourceCopy;
	resourceCopy.m_EntryInfo.eType = VTF_LEGACY_RSRC_IMAGE;
	resourceCopy.m_EntryInfo.resData = 0;
	resourceCopy.m_pData = NULL;
	resourceCopy.m_DataLength = 0;
	targetResources.AddToTail( resourceCopy );

	// get the resources
	numTypes = pSourceVTF->GetResourceTypes( resourceTypes, MAX_RSRC_DICTIONARY_ENTRIES );
	for ( int i=0; i<numTypes; i++ )
	{
		size_t resourceLength;
		void *pResourceData;

		switch ( resourceTypes[i] & ~RSRCF_MASK )
		{
		case VTF_LEGACY_RSRC_LOW_RES_IMAGE:
		case VTF_LEGACY_RSRC_IMAGE:
		case VTF_RSRC_TEXTURE_LOD_SETTINGS:
		case VTF_RSRC_TEXTURE_SETTINGS_EX:
		case VTF_RSRC_TEXTURE_CRC:
			// not needed, presence already folded into conversion
			continue;

		default:
			pResourceData = pSourceVTF->GetResourceData( resourceTypes[i], &resourceLength );
			if ( pResourceData )
			{
				resourceCopy.m_EntryInfo.eType = resourceTypes[i] & ~RSRCF_MASK;
				resourceCopy.m_EntryInfo.resData = 0;
				resourceCopy.m_pData = new char[resourceLength];
				Assert( resourceLength == ( int )resourceLength );
				resourceCopy.m_DataLength = ( int )resourceLength;
				V_memcpy( resourceCopy.m_pData, pResourceData, resourceLength );
				targetResources.AddToTail( resourceCopy );
			}
			break;
		}
	}

	if ( targetResources.Count() > MAX_X360_RSRC_DICTIONARY_ENTRIES )
	{
		Msg( "ConvertVTFToConsoleFormatHelper: More resources than expected in %s\n", pDebugName );
		goto cleanUp;
	}

	targetFlags = pSourceVTF->Flags();
	targetFrameCount = pSourceVTF->FrameCount();
	targetFaceCount = pSourceVTF->FaceCount();

	// determine target format
	targetFormat = PreferredFormat( pSourceVTF, pSourceVTF->Format(), targetWidth, targetHeight, targetMipCount, targetFaceCount, eConFmtType );

	// reset nomip flags
	if ( bNoMip )
	{
		targetFlags |= TEXTUREFLAGS_NOMIP;
	}
	else
	{
		targetFlags &= ~TEXTUREFLAGS_NOMIP;
	}

	// the lowres texture is used for coarse light sampling lookups
	bHasLowResData = ( pSourceVTF->LowResFormat() != -1 ) && pSourceVTF->LowResWidth() && pSourceVTF->LowResHeight();
	if ( bHasLowResData )
	{
		// ensure lowres data is serialized in preferred runtime expected format
		targetLowResWidth  = pSourceVTF->LowResWidth();
		targetLowResHeight = pSourceVTF->LowResHeight();
	}
	else
	{
		// discarding low res data, ensure lowres data is culled
		targetLowResWidth  = 0;
		targetLowResHeight = 0;
	}

	// start serializing output data
	// skip past header
	// serialize in order, 0) Header 1) ResourceDictionary, 3) Resources, 4) image
	// preload may extend into image
	targetBuf.EnsureCapacity( sizeof( targetHeader ) + targetResources.Count() * sizeof( ResourceEntryInfo ) );
	targetBuf.SeekPut( CUtlBuffer::SEEK_CURRENT, sizeof( targetHeader ) + targetResources.Count() * sizeof( ResourceEntryInfo ) );
	
	// serialize low res
	if ( targetLowResWidth && targetLowResHeight )
	{
		CUtlMemory<byte> targetLowResImage;

		int sourceLowResImageSize = ImageLoader::GetMemRequired( pSourceVTF->LowResWidth(), pSourceVTF->LowResHeight(), 1, pSourceVTF->LowResFormat(), false );
		int targetLowResImageSize = ImageLoader::GetMemRequired( targetLowResWidth, targetLowResHeight, 1, IMAGE_FORMAT_RGB888, false );
	
		// conversion may skip bytes, ensure all bits initialized
		targetLowResImage.EnsureCapacity( targetLowResImageSize );
		byte* pTargetLowResImage = (byte*)targetLowResImage.Base();
		memset( pTargetLowResImage, 0xFF, targetLowResImageSize );

		// convert and save lowres image in final format
		bRetVal = ConvertImageFormatEx( 
					pSourceVTF->LowResImageData(), 
					sourceLowResImageSize,
					pSourceVTF->LowResFormat(), 
					pTargetLowResImage, 
					targetLowResImageSize,
					IMAGE_FORMAT_RGB888, 
					targetLowResWidth, 
					targetLowResHeight,
					eConFmtType );
		if ( !bRetVal )
		{
			goto cleanUp;
		}

		// boil to a single linear color
		Vector linearColor;
		linearColor.x = linearColor.y = linearColor.z = 0;
		for ( int j = 0; j < targetLowResWidth * targetLowResHeight; j++ )
		{
			linearColor.x += SrgbGammaToLinear( pTargetLowResImage[j*3+0] * 1.0f/255.0f );
			linearColor.y += SrgbGammaToLinear( pTargetLowResImage[j*3+1] * 1.0f/255.0f );
			linearColor.z += SrgbGammaToLinear( pTargetLowResImage[j*3+2] * 1.0f/255.0f );
		}
		VectorScale( linearColor, 1.0f/(targetLowResWidth * targetLowResHeight), linearColor );

		// serialize as a single texel
		targetLowResSample[0] = 255.0f * SrgbLinearToGamma( linearColor[0] );
		targetLowResSample[1] = 255.0f * SrgbLinearToGamma( linearColor[1] );
		targetLowResSample[2] = 255.0f * SrgbLinearToGamma( linearColor[2] );

		// identifies color presence
		targetLowResSample[3] = 0xFF;
	}
	else
	{
		targetLowResSample[0] = 0;
		targetLowResSample[1] = 0;
		targetLowResSample[2] = 0;
		targetLowResSample[3] = 0;
	}

	// serialize resource data
	for ( int i=0; i<targetResources.Count(); i++ )
	{
		int resourceDataLength = targetResources[i].m_DataLength;
		if ( resourceDataLength == 4 )
		{
			// data goes directly into structure, as is
			targetResources[i].m_EntryInfo.eType |= RSRCF_HAS_NO_DATA_CHUNK;
			V_memcpy( &targetResources[i].m_EntryInfo.resData, targetResources[i].m_pData, 4 );
		}
		else if ( resourceDataLength != 0 )
		{
			targetResources[i].m_EntryInfo.resData = targetBuf.TellPut();
			int swappedLength;
			byteSwapWriter.SwapBufferToTargetEndian( &swappedLength, &resourceDataLength );
			targetBuf.PutInt( swappedLength );
			if ( !targetBuf.IsValid() )
			{
				goto cleanUp;
			}
	
			// put the data
			targetBuf.Put( targetResources[i].m_pData, resourceDataLength );	
			if ( !targetBuf.IsValid() )
			{
				goto cleanUp;
			}
		}
	}
	
	// mark end of preload data
	// preload data might be updated and pushed to extend into the image data mip chain
	preloadDataSize = targetBuf.TellPut();

	// image starts on an aligned boundary
	AlignBuffer( targetBuf, 4 );
	
	// start of image data
	targetImageDataOffset = targetBuf.TellPut();
	if ( targetImageDataOffset >= 65536 )
	{
		// possible bug, or may have to offset to 32 bits
		Msg( "ConvertVTFToConsoleFormatHelper: non-image portion exceeds 16 bit boundary %s\n", pDebugName );
		goto cleanUp;
	}

	// format conversion, data is stored by ascending mips, 1x1 up to NxN
	// data is stored ascending to allow picmipped loads
	for ( mip = targetMipCount - 1; mip >= 0; mip-- )
	{
		for ( frame = 0; frame < targetFrameCount; frame++ )
		{
			for ( face = 0; face < targetFaceCount; face++ )
			{
				if ( !SerializeImageData( pSourceVTF, frame, face, mip, targetFormat, targetBuf, eConFmtType ) )
				{
					goto cleanUp;
				}
			}
		}
	}

	if ( preloadDataSize < VTFFileHeaderSize( TargetHelper::VER_MAJOR, TargetHelper::VER_MINOR ) )
	{
		// preload size must be at least what game attempts to initially read
		preloadDataSize = VTFFileHeaderSize( TargetHelper::VER_MAJOR, TargetHelper::VER_MINOR ); 
	}

	if ( targetBuf.TellPut() <= PRELOAD_VTF_THRESHOLD )
	{
		// the entire file is too small, preload entirely
		preloadDataSize = targetBuf.TellPut();
	}

	if ( preloadDataSize >= 65536 )
	{
		// possible overflow due to large frames, faces, and format, may have to offset to 32 bits
		Msg( "ConvertVTFToConsoleFormatHelper: preload portion exceeds 16 bit boundary %s\n", pDebugName );
		goto cleanUp;
	}

	// finalize header
	V_memset( &targetHeader, 0, sizeof( targetHeader ) );

	V_memcpy( targetHeader.fileTypeString, TargetHelper::GetHeaderType(), 4 );
	targetHeader.version[0] = TargetHelper::VER_MAJOR;
	targetHeader.version[1] = TargetHelper::VER_MINOR;
	targetHeader.headerSize = sizeof( targetHeader ) + targetResources.Count() * sizeof( ResourceEntryInfo );

	targetHeader.flags = targetFlags;
	targetHeader.width = targetWidth;
	targetHeader.height = targetHeight;
	targetHeader.depth = 1;
	targetHeader.numFrames = targetFrameCount;
	targetHeader.preloadDataSize = preloadDataSize;
	targetHeader.mipSkipCount = mipSkipCount;
	targetHeader.numResources = targetResources.Count();
	VectorCopy( pSourceVTF->Reflectivity(), targetHeader.reflectivity );
	targetHeader.bumpScale = pSourceVTF->BumpScale();
	targetHeader.imageFormat = targetFormat;
	targetHeader.lowResImageSample[0] = targetLowResSample[0];
	targetHeader.lowResImageSample[1] = targetLowResSample[1];
	targetHeader.lowResImageSample[2] = targetLowResSample[2];
	targetHeader.lowResImageSample[3] = targetLowResSample[3];

	if ( !IsGameConsole() )
	{
		byteSwapWriter.SwapFieldsToTargetEndian( &targetHeader );
	}

	// write out finalized header
	targetBuf.SeekPut( CUtlBuffer::SEEK_HEAD, 0 );
	targetBuf.Put( &targetHeader, sizeof( targetHeader ) );
	if ( !targetBuf.IsValid() )
	{
		goto cleanUp;
	}

	// fixup and write out finalized resource dictionary
	for ( int i=0; i<targetResources.Count(); i++ )
	{
		switch ( targetResources[i].m_EntryInfo.eType & ~RSRCF_MASK )
		{
			case VTF_LEGACY_RSRC_IMAGE:
				targetResources[i].m_EntryInfo.resData = targetImageDataOffset;
				break;
		}

		if ( !( targetResources[i].m_EntryInfo.eType & RSRCF_HAS_NO_DATA_CHUNK ) )
		{
			// swap the offset holders only
			byteSwapWriter.SwapBufferToTargetEndian( &targetResources[i].m_EntryInfo.resData );
		}

		targetBuf.Put( &targetResources[i].m_EntryInfo, sizeof( ResourceEntryInfo ) );
		if ( !targetBuf.IsValid() )
		{
			goto cleanUp;
		}
	}

	targetBuf.SeekPut( CUtlBuffer::SEEK_TAIL, 0 );

	if ( preloadDataSize < targetBuf.TellPut() && pCompressFunc )
	{
		// only compress files that are not entirely in preload
		CUtlBuffer compressedBuffer;
		targetBuf.SeekGet( CUtlBuffer::SEEK_HEAD, targetImageDataOffset );
		bool bCompressed = pCompressFunc( targetBuf, compressedBuffer );
		if ( bCompressed )
		{
			// copy all the header data off
			CUtlBuffer headerBuffer;
			headerBuffer.EnsureCapacity( targetImageDataOffset );
			headerBuffer.Put( targetBuf.Base(), targetImageDataOffset );

			// reform the target with the header and then the compressed data
			targetBuf.Clear();
			targetBuf.Put( headerBuffer.Base(), targetImageDataOffset );
			targetBuf.Put( compressedBuffer.Base(), compressedBuffer.TellPut() );

			TargetHeaderType *pHeader = (TargetHeaderType *)targetBuf.Base();
			if ( !IsGameConsole() )
			{
				// swap it back into pc space
				byteSwapWriter.SwapFieldsToTargetEndian( pHeader );
			}

			pHeader->compressedSize = compressedBuffer.TellPut();

			if ( !IsGameConsole() )
			{
				// swap it back into console space
				byteSwapWriter.SwapFieldsToTargetEndian( pHeader );
			}
		}

		targetBuf.SeekGet( CUtlBuffer::SEEK_HEAD, 0 );
	}

	// success
	bRetVal = true;

cleanUp:
	if ( pSourceVTF )
	{
		DestroyVTFTexture( pSourceVTF );
	}

	for ( int i=0; i<targetResources.Count(); i++ )
	{
		delete [] (char *)targetResources[i].m_pData;
		targetResources[i].m_pData = NULL;
	}

	return bRetVal;
}

bool ConvertVTFTo360Format( const char *pDebugName, CUtlBuffer &sourceBuf, CUtlBuffer &targetBuf, CompressFunc_t pCompressFunc, int nMaxMip )
{
	return ConvertVTFToConsoleFormatHelper< VTF_CONSOLE_360 >( pDebugName, sourceBuf, targetBuf, pCompressFunc, nMaxMip );
}

bool ConvertVTFToPS3Format( const char *pDebugName, CUtlBuffer &sourceBuf, CUtlBuffer &targetBuf, CompressFunc_t pCompressFunc, int nMaxMip )
{
	return ConvertVTFToConsoleFormatHelper< VTF_CONSOLE_PS3 >( pDebugName, sourceBuf, targetBuf, pCompressFunc, nMaxMip );
}

//-----------------------------------------------------------------------------
// Copy the 360 preload data into a buffer. Used by tools to request the preload,
// as part of the preload build process. Caller doesn't have to know cracking details.
// Not to be used at gametime.
//-----------------------------------------------------------------------------
bool GetVTFPreload360Data( const char *pDebugName, CUtlBuffer &fileBufferIn, CUtlBuffer &preloadBufferOut )
{
	preloadBufferOut.Purge();

	fileBufferIn.ActivateByteSwapping( IsPC() );

	VTFFileHeaderX360_t	header;
	fileBufferIn.GetObjects( &header );

	if ( V_strnicmp( header.fileTypeString, "VTFX", 4 ) ||	
		header.version[0] != VTF_X360_MAJOR_VERSION || 
		header.version[1] != VTF_X360_MINOR_VERSION )
	{
		// bad format
		return false;
	}

	preloadBufferOut.EnsureCapacity( header.preloadDataSize );
	preloadBufferOut.Put( fileBufferIn.Base(), header.preloadDataSize );

	return true;
}

//-----------------------------------------------------------------------------
// Copy the PS3 preload data into a buffer. Used by tools to request the preload,
// as part of the preload build process. Caller doesn't have to know cracking details.
// Not to be used at gametime.
//-----------------------------------------------------------------------------
bool GetVTFPreloadPS3Data( const char *pDebugName, CUtlBuffer &fileBufferIn, CUtlBuffer &preloadBufferOut )
{
	preloadBufferOut.Purge();

	fileBufferIn.ActivateByteSwapping( IsPC() );

	VTFFileHeaderPS3_t	header;
	fileBufferIn.GetObjects( &header );

	if ( V_strnicmp( header.fileTypeString, "VTF3", 4 ) ||	
		header.version[0] != VTF_PS3_MAJOR_VERSION || 
		header.version[1] != VTF_PS3_MINOR_VERSION )
	{
		// bad format
		return false;
	}

	preloadBufferOut.EnsureCapacity( header.preloadDataSize );
	preloadBufferOut.Put( fileBufferIn.Base(), header.preloadDataSize );

	return true;
}
