//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Contains all texture state for the material system surface to use
//
// $Revision: $
// $NoKeywords: $
//=============================================================================//

#ifndef TEXTUREDICTIONARY_H
#define TEXTUREDICTIONARY_H
#ifdef _WIN32
#pragma once
#endif

#include "rendersystem/irenderdevice.h"

class IMaterial;
class IMaterial2;

enum
{
	INVALID_TEXTURE_ID = -1
};


enum ETextureScaling
{
	k_ETextureScalingLinear = 1,
	k_ETextureScalingPointSample = 2,
};


//-----------------------------------------------------------------------------
// A class that manages textures used by the material system surface
//-----------------------------------------------------------------------------
class ITextureDictionary
{
public:
	// Create, destroy textures
	virtual int	CreateTexture( bool procedural = false ) = 0;
	virtual void DestroyTexture( int id ) = 0;
	virtual void DestroyAllTextures() = 0;

	// Is this a valid id?
	virtual bool IsValidId( int id ) const = 0;

	// Binds a material to a texture
	virtual void BindTextureToFile( int id, const char *pFileName ) = 0;

	// Binds a material to a texture
	virtual void BindTextureToMaterial( int id, IMaterial *pMaterial ) = 0;

	// Binds a material to a texture
	virtual void BindTextureToMaterialReference( int id, int referenceId, IMaterial *pMaterial ) = 0;

	// Texture info
	virtual IMaterial *GetTextureMaterial( int id ) = 0; // Source1 version
	virtual HRenderTexture GetTextureHandle( int textureId ) = 0; // Source2 version
	virtual void GetTextureSize(int id, int& iWide, int& iTall ) = 0;
	virtual void GetTextureTexCoords( int id, float &s0, float &t0, float &s1, float &t1 ) = 0;

	virtual void SetTextureRGBA( int id, const char* rgba, int wide, int tall ) = 0;
	virtual void SetTextureRGBAEx( int id, const char* rgba, int wide, int tall, ImageFormat format, ETextureScaling eScaling ) = 0;

	virtual int	FindTextureIdForTextureFile( char const *pFileName ) = 0;
	virtual void SetSubTextureRGBA( int id, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall ) = 0;
	virtual void SetSubTextureRGBAEx( int id, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat format ) = 0;

	virtual void UpdateSubTextureRGBA( int id, int drawX, int drawY, unsigned const char *rgba, int subTextureWide, int subTextureTall, ImageFormat imageFormat ) = 0;

	virtual void BindTextureToMaterial2Reference( int id, int referenceId, IMaterial2 *pMaterial ) = 0;
	virtual void BindTextureToMaterial2( int id, IMaterial2 *pMaterial ) = 0;
	virtual IMaterial2 *GetTextureMaterial2( int id ) = 0; // Source2 version

#if defined( _X360 )

	//
	// Local gamerpic
	//

	// Get the texture id for the local gamerpic.
	virtual int GetLocalGamerpicTextureID( void ) const = 0;

	// Update the local gamerpic texture. Use the given texture if a gamerpic cannot be loaded.
	virtual bool SetLocalGamerpicTexture( DWORD userIndex, const char *pDefaultGamerpicFileName ) = 0;

	//
	// Remote gamerpic
	//

	// Get the texture id for a remote gamerpic with the given xuid.
	virtual int GetRemoteGamerpicTextureID( XUID xuid ) = 0;

	// Update the remote gamerpic texture for the given xuid. Use the given texture if a gamerpic cannot be loaded.
	virtual bool SetRemoteGamerpicTextureID( XUID xuid, const char *pDefaultGamerpicFileName ) = 0;

#endif // _X360
};

ITextureDictionary *TextureDictionary();

#endif // TEXTUREDICTIONARY_H