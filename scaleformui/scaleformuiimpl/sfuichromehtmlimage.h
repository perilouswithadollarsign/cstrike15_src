//========= Copyright © 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __SFUICHROMEHTMLIMAGE_H__
#define __SFUICHROMEHTMLIMAGE_H__

#include "scaleformuiimage.h"

//-----------------------------------------------------------------------------
// Purpose: sfuia
//-----------------------------------------------------------------------------
class ScaleformUIChromeHTMLImage : public ScaleformUIImage
{
public:
	ScaleformUIChromeHTMLImage( uint64 imageID, const byte* defaultRgba, int defaultWidth, int defaultHeight, ::ImageFormat defaultFormat, SF::GFx::TextureManager* pTextureManager );

	bool LoadChromeHTMLImage( const byte* rgba, int width, int height, ::ImageFormat format );

private:
	uint64 m_imageID;
};

#endif // __SFUICHROMEHTMLIMAGE_H__