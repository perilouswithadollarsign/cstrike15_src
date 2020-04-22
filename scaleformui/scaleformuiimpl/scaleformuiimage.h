//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __SCALEFORMUIIMAGE_H__
#define __SCALEFORMUIIMAGE_H__

#include "tier1/utlbuffer.h"
#include "bitmap/imageformat_declarations.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class ScaleformUIImage
{
public:
	ScaleformUIImage( const byte* defaultRgba, int defaultWidth, int defaultHeight, ::ImageFormat defaultFormat, SF::GFx::TextureManager* pTextureManager );

	virtual ~ScaleformUIImage( void );

	// Tick any loading task that needs to be updated
	virtual void Update( void );

	virtual int GetWidth( void ) const { return m_nWidth; }
	virtual int GetHeight( void ) const { return m_nHeight; }

	virtual SF::Render::Image* GetImage( void );

	virtual int AddRef()
	{
		return ThreadInterlockedIncrement( &m_nRefCount );
	}

	virtual int Release()
	{
		int refCount = ThreadInterlockedDecrement( &m_nRefCount );
		if ( refCount == 0 )
		{
			OnFinalRelease();
		}

		return refCount;
	}

	virtual void OnFinalRelease();

protected:
	SF::Render::ImageFormat ConvertImageFormat( ::ImageFormat format );
	virtual void InitFromBuffer( const byte *rgba, int width, int height, ::ImageFormat format );

	volatile int32 m_nRefCount;
	int m_nWidth;
	int m_nHeight;
	ImageFormat m_format;
	SF::GFx::ImageDelegate* m_pImage;
	SF::GFx::TextureManager* m_pTextureManager;
};

#endif // __SCALEFORMUIIMAGE_H__
