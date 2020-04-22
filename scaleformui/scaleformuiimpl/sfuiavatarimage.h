//========= Copyright © 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __SFUIAVATARIMAGE_H__
#define __SFUIAVATARIMAGE_H__

#include "scaleformuiimage.h"

#ifndef NO_STEAM
#include "steam/steam_api.h"
#endif

#if !defined( _X360 )
typedef uint64 XUID;
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class ScaleformUIAvatarImage : public ScaleformUIImage
{
public:
	static IScaleformAvatarImageProvider *sm_pProvider;

public:
	ScaleformUIAvatarImage( XUID xuid, const byte* defaultRgba, int defaultWidth, int defaultHeight, ::ImageFormat defaultFormat, SF::GFx::TextureManager* pTextureManager );

	bool LoadAvatarImage( IScaleformAvatarImageProvider *pProvider = NULL );

private:
	XUID m_xXUID;
	CUtlBuffer m_bufRgbaBuffer;
};

#endif // __SFUIAVATARIMAGE_H__
