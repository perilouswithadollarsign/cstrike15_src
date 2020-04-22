//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// A class representing a procedural texture
//
//=============================================================================

#ifndef DMETEXTURE_H
#define DMETEXTURE_H
#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "materialsystem/materialsystemutil.h"
#include "materialobjects/dmeimage.h"
#include "tier1/functors.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
enum ImageFormat;


//-----------------------------------------------------------------------------
// Compression types
//-----------------------------------------------------------------------------
enum DmeTextureCompress_t
{
	DMETEXTURE_COMPRESS_DEFAULT = 0,
	DMETEXTURE_COMPRESS_NONE,
	DMETEXTURE_COMPRESS_DXT1,
	DMETEXTURE_COMPRESS_DXT5,
};


//-----------------------------------------------------------------------------
// Filter types
//-----------------------------------------------------------------------------
enum DmeTextureFilter_t
{
	DMETEXTURE_FILTER_DEFAULT = 0,
	DMETEXTURE_FILTER_ANISOTROPIC,
	DMETEXTURE_FILTER_TRILINEAR,
	DMETEXTURE_FILTER_BILINEAR,
	DMETEXTURE_FILTER_POINT,
};


//-----------------------------------------------------------------------------
// Mipmap types
//-----------------------------------------------------------------------------
enum DmeTextureMipmap_t
{
	DMETEXTURE_MIPMAP_DEFAULT = 0,
	DMETEXTURE_MIPMAP_ALL_LEVELS,
	DMETEXTURE_MIPMAP_NONE,
};


//-----------------------------------------------------------------------------
// Texture types
//-----------------------------------------------------------------------------
enum DmeTextureType_t
{
	DMETEXTURE_TYPE_NORMAL = 0,
	DMETEXTURE_TYPE_CUBEMAP,
};


//-----------------------------------------------------------------------------
// A class for textures
//-----------------------------------------------------------------------------
class CDmeTextureFrame : public CDmElement
{
	DEFINE_ELEMENT( CDmeTextureFrame, CDmElement );

public:
	int MipLevelCount() const;
	CDmeImageArray *GetMipLevel( int nIndex ) const;
	void AddMipLevel( CDmeImageArray *pImages );
	CDmeImageArray *AddMipLevel( );

	int ImageCount() const;

private:
	// Mip levels for the texture
	CDmaElementArray< CDmeImageArray > m_MipLevels;
};


//-----------------------------------------------------------------------------
// Helper functors
//-----------------------------------------------------------------------------
template< typename ObjectType_t > class CImageMemberFunctor
{
public:
	CImageMemberFunctor( ObjectType_t *pObject, bool (ObjectType_t::*pMemberFunc)( CDmeImage * ) )
	{
		m_pObject = pObject;
		m_pMemberFunc = pMemberFunc;
	}

	bool operator()( CDmeImage *pImage )
	{
		return m_pObject->*m_pMemberFunc( pImage );
	}

private:
	ObjectType_t *m_pObject;
	bool (ObjectType_t::*m_pMemberFunc)( CDmeImage * );
};

class CImageFunctor
{
public:
	CImageFunctor( bool (*pMemberFunc)( CDmeImage * ) )
	{
		m_pFunc = pMemberFunc;
	}

	bool operator()( CDmeImage *pImage )
	{
		return m_pFunc( pImage );
	}

private:
	bool (*m_pFunc)( CDmeImage * );
};


template< typename ObjectType_t > class CImageProcessorMemberFunctor
{
public:
	CImageProcessorMemberFunctor( ObjectType_t *pObject, void (ObjectType_t::*pMemberFunc)( CDmeImage *, CDmeImage * ) )
	{
		m_pObject = pObject;
		m_pMemberFunc = pMemberFunc;
	}

	void operator()( CDmeImage *pDstImage, CDmeImage *pSrcImage )
	{
		(m_pObject->*m_pMemberFunc)( pDstImage, pSrcImage );
	}

private:
	ObjectType_t *m_pObject;
	void (ObjectType_t::*m_pMemberFunc)( CDmeImage *, CDmeImage * );
};


//-----------------------------------------------------------------------------
// A class for textures
//-----------------------------------------------------------------------------
class CDmeTexture : public CDmElement
{
	DEFINE_ELEMENT( CDmeTexture, CDmElement );

public:
	// Compression type
	void SetCompressionType( DmeTextureCompress_t type );
	DmeTextureCompress_t GetCompressionType() const;

	// Filter type
	void SetFilterType( DmeTextureFilter_t type );
	DmeTextureFilter_t GetFilterType() const;

	// Mipmap type
	void SetMipmapType( DmeTextureMipmap_t type );
	DmeTextureMipmap_t GetMipmapType() const;

	// Texture type
	void SetTextureType( DmeTextureType_t type );
	DmeTextureType_t GetTextureType() const;

	CDmeTextureFrame *AddFrame();
	int FrameCount() const;
	CDmeTextureFrame *GetFrame( int nIndex ) const;
	void RemoveAllFrames();

	// Gets dimensions
	int Width() const;
	int Height() const;
	int Depth() const;
	int MipLevelCount() const;
	int ImageCount() const;

	// Methods related to image format
	ImageFormat Format() const;

	// Returns all images associated with a particular frame + face (mip count amount)
	void GetImages( int nFrame, int nImageIndex, CDmeImage **ppImages, int nSize );

	// Iterates over all images, processes them
	template< typename Functor > void ProcessTexture( CDmeTexture *pSrcTexture, Functor &func );
	template< typename ObjectType_t > void ProcessTexture( CDmeTexture *pSrcTexture, ObjectType_t *pObject, void (ObjectType_t::*pMemberFunc)( CDmeImage *, CDmeImage * ) );

	// Iterates over all images, runs a functor
	template< typename Functor > void ForEachImage( Functor &func );
	template< typename ObjectType_t > void ForEachImage( ObjectType_t *pObject, bool (ObjectType_t::*pMemberFunc)( CDmeImage * ) );
	void ForEachImage( bool (*pFunc)( CDmeImage * ) );

	void CompressTexture( CDmeTexture *pSrcTexture, ImageFormat fmt );

	//-------------------------------------
	// Ignore the macro! To use this nifty feature, use code that looks like this:
	//	pTexture->ForEachImage( &CDmeImage::ConvertFormat, dstFormat );
	//-------------------------------------
	#define DEFINE_FOR_EACH_IMAGE(N) \
		template <typename FUNCTION_RETTYPE FUNC_TEMPLATE_FUNC_PARAMS_##N FUNC_TEMPLATE_ARG_PARAMS_##N> \
		void ForEachImage( FUNCTION_RETTYPE ( CDmeImage::*pfnProxied )( FUNC_BASE_TEMPLATE_FUNC_PARAMS_##N ) FUNC_ARG_FORMAL_PARAMS_##N ) \
		{ \
			int nFrameCount = FrameCount(); \
			for ( int f = 0; f < nFrameCount; ++f ) \
			{ \
				CDmeTextureFrame *pFrame = GetFrame( f ); \
				if ( !pFrame ) \
					continue; \
				int nMipCount = pFrame->MipLevelCount(); \
				for ( int m = 0; m < nMipCount; ++m ) \
				{ \
					CDmeImageArray *pImageArray = pFrame->GetMipLevel( m ); \
					if ( !pImageArray ) \
						continue; \
					int nImageCount = pImageArray->ImageCount(); \
					for ( int i = 0; i < nImageCount; ++i ) \
					{ \
						CDmeImage *pImage = pImageArray->GetImage( i ); \
						if ( !pImage ) \
							continue; \
						FunctorDirectCall( pImage, pfnProxied FUNC_FUNCTOR_CALL_ARGS_##N ); \
					} \
				} \
			} \
		}

	FUNC_GENERATE_ALL( DEFINE_FOR_EACH_IMAGE );
	#undef DEFINE_FOR_EACH_IMAGE

public:
	virtual void Resolve();

public:
	CDmaVar<bool> m_bClampS;
	CDmaVar<bool> m_bClampT;
	CDmaVar<bool> m_bClampU;
	CDmaVar<bool> m_bNoDebugOverride;
	CDmaVar<bool> m_bNoLod;
	CDmaVar<bool> m_bNiceFiltered;
	CDmaVar<bool> m_bNormalMap;
	CDmaVar<float> m_flBumpScale;

protected:
	// Computes texture flags
	int CalcTextureFlags( int nDepth ) const;

	// Computes the desired texture format based on flags
	ImageFormat ComputeDesiredImageFormat( ImageFormat srcFormat, int nWidth, int nHeight, int nDepth, int nFlags );

	CDmaVar<int> m_nCompressType;
	CDmaVar<int> m_nFilterType;
	CDmaVar<int> m_nMipmapType;
	CDmaVar<int> m_nTextureType;

	// Array of images in an animated texture
	CDmaElementArray< CDmeTextureFrame > m_Frames;

	// Computed values
//	CTextureReference m_Texture;
//	IVTFTexture *m_pVTFTexture;
	Vector m_vecReflectivity;
};


//-----------------------------------------------------------------------------
// Inline methods
//-----------------------------------------------------------------------------
inline void CDmeTexture::SetCompressionType( DmeTextureCompress_t type )
{
	m_nCompressType = type;
}

inline DmeTextureCompress_t CDmeTexture::GetCompressionType() const
{
	return (DmeTextureCompress_t)m_nCompressType.Get();
}

inline void CDmeTexture::SetFilterType( DmeTextureFilter_t type )
{
	m_nFilterType = type;
}

inline DmeTextureFilter_t CDmeTexture::GetFilterType() const
{
	return (DmeTextureFilter_t)m_nFilterType.Get();
}

inline void CDmeTexture::SetMipmapType( DmeTextureMipmap_t type )
{
	m_nMipmapType = type;
}

inline DmeTextureMipmap_t CDmeTexture::GetMipmapType() const
{
	return (DmeTextureMipmap_t)m_nMipmapType.Get();
}

inline void CDmeTexture::SetTextureType( DmeTextureType_t type )
{
	m_nTextureType = type;
}

inline DmeTextureType_t CDmeTexture::GetTextureType() const
{
	return (DmeTextureType_t)m_nTextureType.Get();
}

inline int CDmeTexture::FrameCount() const
{
	return m_Frames.Count();
}

inline CDmeTextureFrame *CDmeTexture::GetFrame( int nIndex ) const
{
	return m_Frames[nIndex];
}


//-----------------------------------------------------------------------------
// Invokes a functor on all images in the texture
//-----------------------------------------------------------------------------
template< typename Functor > inline void CDmeTexture::ForEachImage( Functor &func )
{
	int nFrameCount = FrameCount();
	for ( int f = 0; f < nFrameCount; ++f )
	{
		CDmeTextureFrame *pFrame = GetFrame( f );
		if ( !pFrame )
			continue;

		int nMipCount = pFrame->MipLevelCount();
		for ( int m = 0; m < nMipCount; ++m )
		{
			CDmeImageArray *pImageArray = pFrame->GetMipLevel( m );
			if ( !pImageArray )
				continue;

			int nImageCount = pImageArray->ImageCount();
			for ( int i = 0; i < nImageCount; ++i )
			{
				CDmeImage *pImage = pImageArray->GetImage( i );
				if ( !pImage )
					continue;

				if ( !func( pImage ) )
					break;
			}
		}
	}
}

template< typename ObjectType_t > inline void CDmeTexture::ForEachImage( ObjectType_t *pObject, bool (ObjectType_t::*pMemberFunc)( CDmeImage * ) )
{
	CImageMemberFunctor< ObjectType_t > functor( pObject, pMemberFunc );
	ForEachImage( functor );
}

inline void CDmeTexture::ForEachImage( bool (*pFunc)( CDmeImage * ) )
{
	CImageFunctor functor( pFunc );
	ForEachImage( functor );
}


// Iterates over all images, processes them
template< typename Functor > inline void CDmeTexture::ProcessTexture( CDmeTexture *pSrcTexture, Functor &func )
{
	// FIXME: This pattern should go into some templatized type thingy
	pSrcTexture->CopyAttributesTo( this, TD_NONE );

	// Copying will copy references to src texture frames. Remove them.
	RemoveAllFrames();

	int nFrameCount = pSrcTexture->FrameCount();
	for ( int f = 0; f < nFrameCount; ++f )
	{
		CDmeTextureFrame *pSrcFrame = pSrcTexture->GetFrame( f );
		CDmeTextureFrame *pDstFrame = AddFrame();

		int nMipCount = pSrcFrame->MipLevelCount();
		for ( int m = 0; m < nMipCount; ++m )
		{
			CDmeImageArray *pSrcImageArray = pSrcFrame->GetMipLevel( m );
			CDmeImageArray *pDstImageArray = pDstFrame->AddMipLevel();
			if ( !pSrcImageArray )
				continue;
			int nImageCount = pSrcImageArray->ImageCount();
			for ( int i = 0; i < nImageCount; ++i )
			{
				CDmeImage *pSrcImage = pSrcImageArray->GetImage( i );
				CDmeImage *pDstImage = pDstImageArray->AddImage( );
				if ( !pSrcImage )
					continue;

				func( pDstImage, pSrcImage );
			}
		}
	}
}

template< typename ObjectType_t > inline void CDmeTexture::ProcessTexture( CDmeTexture *pSrcTexture, ObjectType_t *pObject, void (ObjectType_t::*pMemberFunc)( CDmeImage *, CDmeImage * ) )
{
	CImageProcessorMemberFunctor< ObjectType_t > functor( pObject, pMemberFunc );
	ProcessTexture( pSrcTexture, functor );
}


#endif // DMETEXTURE_H
