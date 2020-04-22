//========= Copyright © 1996-2013, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __SFUIINVENTORYIMAGE_H__
#define __SFUIINVENTORYIMAGE_H__

#include "scaleformuiimage.h"

#if !defined( _X360 )
typedef uint64 XUID;
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class ScaleformUIInventoryImage : public ScaleformUIImage
{
public:
	ScaleformUIInventoryImage( uint64 itemid, const byte* defaultRgba, int defaultWidth, int defaultHeight, ::ImageFormat defaultFormat, SF::GFx::TextureManager* pTextureManager );

	bool LoadInventoryImage( const CUtlBuffer* rawImageData, int nWidth, int nHeight, ::ImageFormat format );

private:
#ifdef USE_OVERLAY_ON_INVENTORY_ICONS
	void OverlayFromBuffer( const byte *rgba, int width, int height, ::ImageFormat format );
#endif
	uint64 m_itemid;
};



#endif // __SFUIINVENTORYIMAGE_H__
