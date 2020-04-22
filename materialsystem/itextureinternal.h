//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef ITEXTUREINTERNAL_H
#define ITEXTUREINTERNAL_H

#ifdef _WIN32
#pragma once
#endif

#include "materialsystem/itexture.h"
#include "shaderapi/ishaderapi.h"

class Vector;
enum Sampler_t;
class IFileList;

enum RenderTargetType_t
{
	NO_RENDER_TARGET = 0,
	// GR - using shared depth buffer
	RENDER_TARGET = 1,
	// GR - using own depth buffer
	RENDER_TARGET_WITH_DEPTH = 2,
	// GR - no depth buffer
	RENDER_TARGET_NO_DEPTH = 3,
	// only cares about depth buffer
	RENDER_TARGET_ONLY_DEPTH = 4,
};

abstract_class ITextureInternal : public ITexture
{
public:

	virtual void Bind( Sampler_t sampler, TextureBindFlags_t nBindFlags ) = 0;
	virtual void Bind( Sampler_t sampler1, TextureBindFlags_t nBindFlags, int nFrame, Sampler_t sampler2 = SHADER_SAMPLER_INVALID ) = 0;

	virtual void GetReflectivity( Vector& reflectivity ) = 0;

	// Set this as the render target, return false for failure
	virtual bool SetRenderTarget( int nRenderTargetID ) = 0;

	// Releases the texture's hw memory
	virtual void Release() = 0;

	// Called before Download() on restore. Gives render targets a change to change whether or
	// not they force themselves to have a separate depth buffer due to AA.
	virtual void OnRestore() = 0;

	// Resets the texture's filtering and clamping mode
	virtual void SetFilteringAndClampingMode() = 0;

	// Used by tools.... loads up the non-fallback information about the texture 
	virtual void Precache() = 0;

	// Stretch blit the framebuffer into this texture.
	virtual void CopyFrameBufferToMe( int nRenderTargetID = 0, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL ) = 0;
	virtual void CopyMeToFrameBuffer( int nRenderTargetID = 0, Rect_t *pSrcRect = NULL, Rect_t *pDstRect = NULL ) = 0;

	virtual ITexture *GetEmbeddedTexture( int nIndex ) = 0;

	// Get the shaderapi texture handle associated w/ a particular frame
	virtual ShaderAPITextureHandle_t GetTextureHandle( int nFrame, int nTextureChannel =0 ) = 0;

	virtual ~ITextureInternal()
	{
	}

	virtual ImageFormat GetImageFormat() const = 0;

	// Creates a new texture
	static ITextureInternal *CreateFileTexture( const char *pFileName, const char *pTextureGroupName );
	
	static ITextureInternal *CreateProceduralTexture( 
		const char			*pTextureName,
		const char			*pTextureGroupName, 
		int					w, 
		int					h,
		int					d,
		ImageFormat			fmt, 
		int					nFlags );

	static ITextureInternal *CreateRenderTarget( 
		const char *pRTName, // NULL for an auto-generated name.
		int w, 
		int h, 
		RenderTargetSizeMode_t sizeMode, 
		ImageFormat fmt, 
		RenderTargetType_t type,
		unsigned int textureFlags, 
		unsigned int renderTargetFlags,
		bool bMultipleTargets );

	static void ChangeRenderTarget(
		ITextureInternal *pTexture,
		int w, 
		int h, 
		RenderTargetSizeMode_t sizeMode, 
		ImageFormat fmt, 
		RenderTargetType_t type,
		unsigned int textureFlags, 
		unsigned int renderTargetFlags );

	static ITextureInternal *CreateReferenceTextureFromHandle(
		const char *pTextureName,
		const char *pTextureGroupName,
		ShaderAPITextureHandle_t hTexture );

	static void Destroy( ITextureInternal *pTexture );

	// Set this as the render target, return false for failure
	virtual bool SetRenderTarget( int nRenderTargetID, ITexture* pDepthTexture ) = 0;

	// Bind this to a vertex texture sampler
	virtual void BindVertexTexture( VertexTextureSampler_t sampler, int frameNum = 0 ) = 0;

	virtual void MarkAsPreloaded( bool bSet ) = 0;
	virtual bool IsPreloaded() const = 0;

	virtual void MarkAsExcluded( bool bSet, int nDimensionsLimit, bool bMarkAsTrumpedExclude = false ) = 0;
	virtual bool UpdateExcludedState() = 0;

	virtual bool IsTempRenderTarget( void ) const = 0;

	// Reload any files the texture is responsible for.
	virtual void ReloadFilesInList( IFileList *pFilesToReload ) = 0;

	virtual bool IsMultiRenderTarget( void ) = 0;

#ifdef _PS3
	virtual void Ps3gcmRawBufferAlias( char const *pRTName ) = 0;
#endif

	virtual bool MarkAsTempExcluded( bool bSet, int nExcludedDimensionLimit ) = 0;

	virtual bool IsForceExcluded() const = 0;
	virtual bool ClearForceExclusion() = 0;

	virtual bool IsAsyncDone() const = 0;
};

inline bool IsTextureInternalEnvCubemap( const ITextureInternal *pTexture )
{
	return ( pTexture == ( ITextureInternal * )-1 );
}

//-----------------------------------------------------------------------------
// Ensures that caller provided names are consistent to the dictionary
//-----------------------------------------------------------------------------
inline char *NormalizeTextureName( const char *pName, char *pOutName, int nOutNameSize )
{
	// hdr textures have an ldr version and need to resolve correctly
	int nLen = Q_strlen( pName ) + 1;
	if ( nLen <= 5 || Q_stricmp( pName + nLen - 5, ".hdr" ) )
	{
		// strip any non .hdr extension
		Q_StripExtension( pName, pOutName, nOutNameSize ); 
	}
	else
	{
		// keep .hdr extension
		Q_strncpy( pOutName, pName, nOutNameSize );
	}

	Q_strlower( pOutName );
	Q_FixSlashes( pOutName, '/' );

	return pOutName;
}

#endif // ITEXTUREINTERNAL_H
