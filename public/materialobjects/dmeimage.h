//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// A class representing an image
//
//=============================================================================

#ifndef DMEIMAGE_H
#define DMEIMAGE_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"
#include "bitmap/imageformat.h"
#include "bitmap/floatbitmap.h"


//-----------------------------------------------------------------------------
// A class representing an image (2d or 3d bitmap)
//-----------------------------------------------------------------------------
class CDmeImage : public CDmElement
{
	DEFINE_ELEMENT( CDmeImage, CDmElement );

public:
	virtual void OnAttributeChanged( CDmAttribute *pAttribute );
	virtual void OnElementUnserialized();
	virtual void OnElementSerialized();

public:
	// Initializes the buffer, but doesn't allocate space
	void Init( int nWidth, int nHeight, int nDepth, ImageFormat fmt, float flGamma );

	// Gets dimensions
	int Width() const;
	int Height() const;
	int Depth() const;

	// Methods related to image format
	ImageFormat Format() const;
	const char *FormatName() const;

	// Methods related to gamma
	float Gamma() const;

	// returns the size of one row
	int RowSizeInBytes( ) const;

	// returns the size of one z slice
	int ZSliceSizeInBytes( ) const;

	// returns the total size of the image
	int SizeInBytes( ) const;

	// Sets the storage mode. False = the bits are put in the attribute.
	// True = the bits are put in the float bitmap
	void SetFloatBitmapStorageMode( bool bFloatBitmap, bool bDiscardContents = false );
	bool IsUsingFloatBitmapStorageMode() const;
	bool HasImageData() const;

	// Used for computation
//	void BeginComputation();
//	void EndComputation();

	// Copies the image from the src in whatever storage form they are currently in
	// Potentially color converting
	void CopyFrom( CDmeImage *pSrcImage, ImageFormat fmt = IMAGE_FORMAT_UNKNOWN );

	// Color converts the image into the destination format.
	// Has no immediate effect if the image is in 'float bitmap' mode.
	// Instead, it will cause it to use this format when it eventually
	// reconverts back to 'attribute' mode.
	// NOTE: Doesn't work to convert to a compressed format. 
	void ConvertFormat( ImageFormat fmt );

	// Reinterprets the image as a new color format; no work is done
	// The old + new color formats must be the same size in bytes
	void ReinterpetFormat( ImageFormat fmt );

	//
	// NOTE: The following methods operate on the bits attribute
	//

	// Used for bit modification
	CUtlBinaryBlock &BeginModification( );
	void EndModification( );

	// returns a pointer to the image bits buffer
	const void *ImageBits();

	// Copies bits into the image bits buffer
	void SetImageBits( const void *pBits, int nSize );

	// Compresses an image into this image
	void CompressImage( CDmeImage *pSrcImage, ImageFormat fmt );

	//
	// NOTE: The following methods operate on the float-bitmap version of the bits attribute
	//

	// returns a pointer to the image bits buffer as a float bitmap
	const FloatBitMap_t *FloatBitmap();

	// Allows you to directly manipulate the float bitmap
	FloatBitMap_t &BeginFloatBitmapModification( );
	void EndFloatBitmapModification( );

	// Creates an image 1/4 size of the source using a box filter
	void QuarterSize( CDmeImage *pSrcImage );	

	// Downsample using nice filter (NOTE: Dest bitmap needs to have been initialized w/ final size)
	void DownsampleNiceFiltered( const DownsampleInfo_t& info, CDmeImage *pSrcImage );

	// Sets the color of every pixel
	void Clear( float r, float g, float b, float a );

private:
	enum StorageMode_t
	{
		DMEIMAGE_STORAGE_NONE = 0,
		DMEIMAGE_STORAGE_FLOAT_BITMAP,
		DMEIMAGE_STORAGE_ATTRIBUTE,
	};

	CDmaVar<int> m_nWidth;
	CDmaVar<int> m_nHeight;
	CDmaVar<int> m_nDepth;
	CDmaVar<int> m_nFormat;
	CDmaVar<float> m_flGamma;
	CDmaBinaryBlock m_Bits;

	// Used for computation
	DmAttributeModifyHandle_t m_hModify;
	StorageMode_t m_Mode;
	bool m_bInModification;
	bool m_bInFloatBitmapModification;
	bool m_bIgnoreChangedBitsAttribute;
	FloatBitMap_t m_ComputeBits;
};


//-----------------------------------------------------------------------------
// Gets dimensions
//-----------------------------------------------------------------------------
inline int CDmeImage::Width() const
{
	return ( m_Mode != DMEIMAGE_STORAGE_FLOAT_BITMAP ) ? m_nWidth : m_ComputeBits.NumCols();
}

inline int CDmeImage::Height() const
{
	return ( m_Mode != DMEIMAGE_STORAGE_FLOAT_BITMAP ) ? m_nHeight : m_ComputeBits.NumRows();
}

inline int CDmeImage::Depth() const
{
	return ( m_Mode != DMEIMAGE_STORAGE_FLOAT_BITMAP ) ? m_nDepth : m_ComputeBits.NumSlices();
}


//-----------------------------------------------------------------------------
// Methods related to gamma
//-----------------------------------------------------------------------------
inline float CDmeImage::Gamma() const
{
	return m_flGamma;
}


//-----------------------------------------------------------------------------
// returns a pointer to the image bits buffer
//-----------------------------------------------------------------------------
inline const void *CDmeImage::ImageBits()
{
	SetFloatBitmapStorageMode( false );
	return m_Bits.Get();
}

inline bool CDmeImage::IsUsingFloatBitmapStorageMode() const
{
	return ( m_Mode == DMEIMAGE_STORAGE_FLOAT_BITMAP );
}

inline bool CDmeImage::HasImageData() const
{
	return ( m_Mode != DMEIMAGE_STORAGE_NONE );
}


//-----------------------------------------------------------------------------
// An array of images (used for cubemaps or texture arrays)
//-----------------------------------------------------------------------------
class CDmeImageArray : public CDmElement
{
	DEFINE_ELEMENT( CDmeImageArray, CDmElement );

public:
	int ImageCount() const;
	CDmeImage *GetImage( int nIndex ) const;
	CDmeImage *AddImage( );
	void AddImage( CDmeImage *pImage );

	// Gets dimensions
	int Width() const;
	int Height() const;
	int Depth() const;
	ImageFormat Format() const;

	bool IsConsistent( int nWidth, int nHeight, int nDepth, ImageFormat fmt ) const;

private:
	CDmaElementArray< CDmeImage > m_Images;
};


#endif // DMEIMAGE_H
